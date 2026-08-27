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

// Total energy must never increase: after release, gravity is the only
// external force and the dashpot and Coulomb friction can only dissipate. This
// is the sharpest cheap check that the integrator is not injecting energy --
// and it only became meaningful once compute_total_energy() accounted for
// elastic energy stored in compressed contacts (before that, spring energy
// reappearing as kinetic energy looked exactly like numerical heating).
TEST(ParticleDynamicsTest, TotalEnergyNeverIncreases) {
    ParticleDynamics sim;
    float previous = sim.compute_total_energy();
    const float initial = previous;
    ASSERT_GT(initial, 0.0f);
    // Float32 atomicAdd over thousands of terms is not exactly reproducible, so
    // allow a small slack rather than demanding strict monotonicity.
    const float tolerance = 1e-3f * initial;
    for (int interval = 0; interval < 12; ++interval) {
        for (int step = 0; step < 100; ++step) {
            sim.take_step();
        }
        const float current = sim.compute_total_energy();
        EXPECT_LE(current, previous + tolerance)
                << "energy rose at interval " << interval
                << " (" << previous << " -> " << current << ")";
        previous = current;
    }
    // And it should actually have dissipated a meaningful amount by now, not
    // merely failed to grow.
    EXPECT_LT(previous, initial);
}

// ---------------------------------------------------------------------------
// Density/momentum latent (DensityGridKernel). These are the tests that make
// the deposit trustworthy as a regression target: if mass is silently lost or
// smeared with the wrong weights, every downstream surrogate result inherits
// the error with no obvious symptom.
// ---------------------------------------------------------------------------

// Trilinear deposit must be exactly mass-conserving: sum over all nodes of the
// mass channel equals the total particle mass, for an arbitrary (jittered,
// mid-flight) configuration. This is the single most important property --
// weights that fail to sum to 1, or dropped out-of-range nodes, show up here.
TEST(ParticleDynamicsTest, DensityGridConservesTotalMass) {
    SimConfig config;
    config.init_jitter = 0.3f;   // break the lattice so nodes are hit off-center
    ParticleDynamics sim(config);
    for (int step = 0; step < 250; ++step) {
        sim.take_step();   // let it fall and disorder, so this is not a trivial case
    }
    sim.compute_density_grid();

    double deposited = 0.0;
    for (size_t node = 0; node < sim.density_grid_nodes(); ++node) {
        deposited += sim.host_density_grid[node];
    }

    sim.unpack_state();
    double expected = 0.0;
    for (int i = 0; i < sim.n; ++i) {
        expected += sim.host_mass[sim.host_material[i]];
    }
    ASSERT_GT(expected, 0.0);
    EXPECT_NEAR(deposited, expected, 1e-4 * expected);
}

// A single grain sitting exactly on a grid node must put all of its mass on
// that one node -- the degenerate case of the trilinear weights.
TEST(ParticleDynamicsTest, DensityGridPutsGrainOnNodeAtSingleNode) {
    SimConfig config;
    config.init_type = "single_particle";
    config.density_grid_size_x = 5;
    config.density_grid_size_y = 5;
    config.density_grid_size_z = 5;
    // Node-centered grid over [-1,1] with 5 nodes -> spacing 0.5, nodes at
    // -1, -0.5, 0, 0.5, 1. Place the grain exactly on node (1,2,3).
    config.init_x0 = -0.5f;
    config.init_y0 = 0.0f;
    config.init_z0 = 0.5f;
    ParticleDynamics sim(config);
    sim.compute_density_grid();

    const int expected_node = (1 * 5 + 2) * 5 + 3;
    const float m = sim.host_mass[config.particle_material];
    for (size_t node = 0; node < sim.density_grid_nodes(); ++node) {
        if (static_cast<int>(node) == expected_node) {
            EXPECT_NEAR(sim.host_density_grid[node], m, 1e-6f * m);
        } else {
            EXPECT_NEAR(sim.host_density_grid[node], 0.0f, 1e-6f * m);
        }
    }
}

// A grain at the exact center of a cell must split eight ways, 1/8 each.
TEST(ParticleDynamicsTest, DensityGridSplitsCellCenterEightWays) {
    SimConfig config;
    config.init_type = "single_particle";
    config.density_grid_size_x = 5;
    config.density_grid_size_y = 5;
    config.density_grid_size_z = 5;
    // Spacing 0.5; halfway between nodes on every axis.
    config.init_x0 = -0.75f;
    config.init_y0 = -0.25f;
    config.init_z0 = 0.25f;
    ParticleDynamics sim(config);
    sim.compute_density_grid();

    const float m = sim.host_mass[config.particle_material];
    int nonzero = 0;
    float total = 0.0f;
    for (size_t node = 0; node < sim.density_grid_nodes(); ++node) {
        const float value = sim.host_density_grid[node];
        total += value;
        if (value > 1e-9f) {
            ++nonzero;
            EXPECT_NEAR(value, m / 8.0f, 1e-5f * m);
        }
    }
    EXPECT_EQ(nonzero, 8);
    EXPECT_NEAR(total, m, 1e-5f * m);
}

// The momentum channels must reproduce mass * velocity, so that dividing
// channel 1..3 by channel 0 recovers a mass-weighted velocity field.
TEST(ParticleDynamicsTest, DensityGridMomentumMatchesMassTimesVelocity) {
    SimConfig config;
    config.init_type = "single_particle";
    config.init_vx0 = 2.0f;
    config.init_vy0 = -3.0f;
    config.init_vz0 = 0.5f;
    ParticleDynamics sim(config);
    sim.compute_density_grid();

    const size_t nodes = sim.density_grid_nodes();
    const float m = sim.host_mass[config.particle_material];
    const float expected_velocity[kDim] = {2.0f, -3.0f, 0.5f};
    for (int a = 0; a < kDim; ++a) {
        double momentum = 0.0;
        for (size_t node = 0; node < nodes; ++node) {
            momentum += sim.host_density_grid[(a + 1) * nodes + node];
        }
        EXPECT_NEAR(momentum, static_cast<double>(m) * expected_velocity[a],
                1e-4 * std::abs(static_cast<double>(m) * expected_velocity[a]) + 1e-9);
    }
}

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
