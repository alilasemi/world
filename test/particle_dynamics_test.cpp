#include <gtest/gtest.h>

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
        sim.host_state[4 * i + 2] = 0.0f;
        sim.host_state[4 * i + 3] = 0.0f;
    }
    sim.device_state.copy_from_host(sim.host_state);
    const float y0 = sim.host_state[1]; // particle 0: bottom-left corner of the cube
    const int steps = 100;
    for (int i = 0; i < steps; ++i) {
        sim.take_step();
    }
    sim.unpack_state();
    // Symplectic Euler (velocity updated, then position with the new
    // velocity): x_k = x0 - g*dt^2 * k*(k+1)/2
    const float expected_dy = -9.81f * sim.dt * sim.dt * static_cast<float>(steps) * static_cast<float>(steps + 1) / 2.0f;
    EXPECT_NEAR(sim.xy[1] - y0, expected_dy, 1e-6f);
}

TEST(ParticleDynamicsTest, ParticleDoesNotPenetrateFloor) {
    ParticleDynamics sim;
    sim.host_state[0] = 0.5f;
    sim.host_state[1] = -0.5f; // isolated, mid-domain
    sim.host_state[2] = 0.0f;
    sim.host_state[3] = 0.0f;
    sim.device_state.copy_from_host(sim.host_state);
    for (int i = 0; i < 20000; ++i) { // dt=1e-4 -> 2s simulated
        sim.take_step();
    }
    sim.unpack_state();
    EXPECT_GE(sim.xy[1], -1.0f + 0.01f - 1e-3f); // stays at/above floor+radius
    EXPECT_TRUE(sim.is_stable());
}

TEST(ParticleDynamicsTest, RepeatedRunsAreDeterministic) {
    auto run = [](std::vector<float>& out) {
        ParticleDynamics sim;
        for (int i = 0; i < 50; ++i) {
            sim.take_step();
        }
        sim.unpack_state();
        out = sim.xy;
    };
    std::vector<float> a, b;
    run(a);
    run(b);
    ASSERT_EQ(a.size(), b.size());
    for (size_t i = 0; i < a.size(); ++i) {
        EXPECT_EQ(a[i], b[i]);
    }
}

TEST(ParticleDynamicsTest, SledParticleMovesUnderInitialVelocity) {
    ParticleDynamics sim;
    const int sled = sim.n - 1;
    const float x0 = sim.host_state[4 * sled + 0];
    for (int i = 0; i < 10; ++i) {
        sim.take_step();
    }
    sim.unpack_state();
    EXPECT_GT(sim.xy[2 * static_cast<size_t>(sled) + 0], x0);
}
