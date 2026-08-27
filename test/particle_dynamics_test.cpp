#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "host_vector.h"
#include "particle_dynamics.h"

// None of these tests call resize()/initialize_to_*() after construction:
// those reallocate host_state/device_state, but the kernel objects already
// baked in the original n at construction time, so doing so afterward would
// desync kernel loop bounds from buffer size (out-of-bounds device access).
// Instead, tests mutate sim.host_state[...] directly and push the change
// with sim.device_state.copy_from_host(sim.host_state).

TEST(ParticleDynamicsTest, RunsStablyForFixedIterations) {
    ParticleDynamics sim;
    for (int i = 0; i < 100; ++i) {
        sim.take_step();
    }
    EXPECT_TRUE(sim.is_stable());
}

TEST(ParticleDynamicsTest, FreeFallMatchesKinematics) {
    ParticleDynamics sim;
    for (int i = 0; i < sim.n; ++i) {
        for (int a = 0; a < kDim; ++a) {
            sim.host_state[kStateStride * i + kDim + a] = 0.0f;
        }
    }
    sim.device_state.copy_from_host(sim.host_state);
    // Particle 0 is the cube's min corner; z is the gravity axis.
    const float z0 = sim.host_state[2];
    const int steps = 100;
    for (int i = 0; i < steps; ++i) {
        sim.take_step();
    }
    sim.unpack_state();
    // Symplectic Euler (velocity updated, then position with the new
    // velocity): z_k = z0 - g*dt^2 * k*(k+1)/2
    const float expected_dz = -9.81f * sim.dt * sim.dt * static_cast<float>(steps) * static_cast<float>(steps + 1) / 2.0f;
    EXPECT_NEAR(sim.positions[2] - z0, expected_dz, 1e-6f);
}

TEST(ParticleDynamicsTest, ParticleDoesNotPenetrateFloor) {
    ParticleDynamics sim;
    // Move particle 0 well clear of the cube, then let it fall to the floor.
    const float start[kStateStride] = {0.5f, 0.5f, -0.5f, 0.0f, 0.0f, 0.0f};
    for (int a = 0; a < kStateStride; ++a) {
        sim.host_state[a] = start[a];
    }
    sim.device_state.copy_from_host(sim.host_state);
    for (int i = 0; i < 20000; ++i) { // dt=1e-4 -> 2s simulated
        sim.take_step();
    }
    sim.unpack_state();
    // Rests at/above floor + radius, along z.
    EXPECT_GE(sim.positions[2], -1.0f + 0.01f - 1e-3f);
    EXPECT_TRUE(sim.is_stable());
}

TEST(ParticleDynamicsTest, RepeatedRunsAreDeterministic) {
    auto run = [](std::vector<float>& out) {
        ParticleDynamics sim;
        for (int i = 0; i < 50; ++i) {
            sim.take_step();
        }
        sim.unpack_state();
        out = sim.positions;
    };
    std::vector<float> a, b;
    run(a);
    run(b);
    ASSERT_EQ(a.size(), b.size());
    for (size_t i = 0; i < a.size(); ++i) {
        EXPECT_EQ(a[i], b[i]);
    }
}

// Forces the collision-grid overflow path, which no checked-in config reaches:
// a 1x1 grid puts every particle in a single cell with room for only two, so
// fill_cells_kernel must drop the rest. Before the num_per_cell clamp, the
// dropped particles still incremented the count, and find_neighbors_kernel
// trusted that count as its loop bound and read past the end of the cell's row.
// Run the test binary under `compute-sanitizer --tool memcheck` to confirm the
// read stays in bounds; this test asserts the overflow is reported and the step
// still produces finite state.
TEST(ParticleDynamicsTest, GridOverflowIsCountedAndStaysInBounds) {
    SimConfig config;
    config.collision_grid_size_x = 1;
    config.collision_grid_size_y = 1;
    config.collision_grid_size_z = 1;
    config.particles_per_cell = 2;
    ParticleDynamics sim(config);
    ASSERT_GT(sim.n, config.particles_per_cell);

    EXPECT_EQ(sim.grid_overflow_count(), 0); // lifetime counter starts clean
    sim.take_step();
    EXPECT_GT(sim.grid_overflow_count(), 0); // the single cell overflowed

    sim.unpack_state();
    for (float coordinate : sim.positions) {
        EXPECT_TRUE(std::isfinite(coordinate));
    }
}
TEST(ParticleDynamicsTest, SledParticleMovesUnderInitialVelocity) {
    ParticleDynamics sim;

// Every grain of a cube must receive the configured initial velocity -- this is
// what makes a cube a thrown blob, and it is the whole basis of the dataset's
// "action". (Replaces an older test that checked a lone "sled" particle moved;
// the sled and snow materials were leftovers from an earlier experiment and are
// gone, so the cube itself now carries the initial velocity.)
TEST(ParticleDynamicsTest, CubeGrainsReceiveInitialVelocity) {
    SimConfig config;
    config.init_vx0 = 1.5f;
    config.init_vy0 = -0.5f;
    config.init_vz0 = 0.25f;
    config.physics.gravity = 0.0f;   // isolate the initial velocity from free fall
    ParticleDynamics sim(config);
    ASSERT_GT(sim.n, 1);

    // Applied to every grain, not just one.
    const float expected[kDim] = {1.5f, -0.5f, 0.25f};
    for (int i = 0; i < sim.n; ++i) {
        for (int a = 0; a < kDim; ++a) {
            EXPECT_FLOAT_EQ(sim.host_state[kStateStride * i + kDim + a], expected[a]);
        }
    }

    // And it actually transports them: with gravity off and the blob starting
    // clear of every wall, each axis advances by v*t.
    std::vector<float> before(sim.positions.begin(), sim.positions.end());
    const int steps = 50;
    for (int step = 0; step < steps; ++step) {
        sim.take_step();
    }
    sim.unpack_state();
    const float elapsed = sim.dt * static_cast<float>(steps);
    for (int a = 0; a < kDim; ++a) {
        const float moved = sim.positions[a] - before[a];
        EXPECT_NEAR(moved, expected[a] * elapsed, 1e-4f * std::abs(expected[a] * elapsed) + 1e-6f);
    }
}
