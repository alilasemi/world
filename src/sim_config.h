#pragma once
#include <string>
#include <vector>

// Small plain-old-data structs passed by value into the CUDA kernels. They hold
// only the values each kernel needs device-side, so they're trivially copyable
// and safe as __global__ arguments.

// Simulation domain bounds. Kernels map a particle (x, y) into a grid cell via
// (x - x_min) / (x_max - x_min), so making these configurable lets the domain be
// something other than the historical [-1, 1] x [-1, 1].
struct DomainParams {
    float x_min;
    float x_max;
    float y_min;
    float y_max;
};

// Physics constants consumed by ComputeRHSKernel (and a subset by EnergyKernel).
struct PhysicsParams {
    float gravity;
    float particle_radius;
    float max_force;
    float floor_y;
    float ceiling_y;
    float left_wall_x;
    float right_wall_x;
};

// Full host-side configuration. Every member defaults to the value that was
// hardcoded in the source before this was introduced, so a default-constructed
// SimConfig reproduces the original behavior exactly (important: the tests and
// the profiling/stability drivers rely on the default-constructed sim).
struct SimConfig {
    // Simulation / grid
    float dt = 0.0001f;
    int grid_size = 32;
    int particles_per_cell = 64;
    int force_grid_size = 16;
    int occupancy_grid_size = 32;
    int threads_per_block = 256;

    // Domain bounds
    DomainParams domain{-1.0f, 1.0f, -1.0f, 1.0f};

    // Physics
    PhysicsParams physics{9.81f, 0.01f, 100.0f, -1.0f, 1.0f, -1.0f, 1.0f};

    // Per-material masses, indexed by material id (0 = wall, 1 = snow, 2 = sled).
    std::vector<float> masses{0.0f, 0.04f, 0.04f};

    // Initialization
    std::string init_type = "cube";  // "cube" | "two_particles"
    float init_x0 = -0.5f;
    float init_y0 = 0.0f;
    float cube_length = 1.0f;  // particle spacing is derived from physics.particle_radius
    float sled_x = -0.9f;
    float sled_y = 0.9f;
    float sled_vx = 0.4f;
    float sled_vy = 0.0f;
    int sled_material = 2;
    float two_particle_separation = 0.1f;
    int particle_material = 1;

    // Rendering. Sent to the JS client over the WebSocket at initialize time so
    // the client doesn't keep its own copies. The render radius deliberately
    // reuses physics.particle_radius rather than a separate field.
    int num_triangles = 20;

    // Drivers
    int port = 8081;
    int steps_per_frame = 10;
    int profiling_outer_iters = 10;
    int profiling_steps_per_iter = 10;
    float stability_sim_time = 0.1f;
    std::vector<float> stability_dt_sweep{0.1f, 0.05f, 0.01f, 0.005f, 0.001f, 0.0005f, 0.0001f};
    bool kernel_timing = true;

    // Time integration
    std::string time_integrator = "semi_implicit_euler";  // "semi_implicit_euler" | "backward_euler_picard"
    int picard_iterations = 3;

    // RL agent
    float rl_max_force = 0.1f;
};

// Loads a config from a YAML file, starting from defaults and overriding only
// the keys present in the file. If the file cannot be opened, prints a notice to
// stderr and returns the defaults (so drivers and tests still work without one).
SimConfig load_config(const std::string& path);
