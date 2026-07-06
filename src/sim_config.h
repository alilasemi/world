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
    // Target coefficient of restitution (0 = fully inelastic/critically
    // damped, 1 = perfectly elastic/no damping) for the linear
    // spring-dashpot contact model in ComputeRHSKernel. See
    // restitution_to_damping() in compute_rhs_kernel.cu for how this maps
    // to an actual dashpot coefficient.
    float restitution;
};

// Full host-side configuration. Every member defaults to the value that was
// hardcoded in the source before this was introduced, so a default-constructed
// SimConfig reproduces the original behavior exactly (important: the tests and
// the profiling/stability drivers rely on the default-constructed sim).
struct SimConfig {
    // Simulation / grid. All three grids (collision, force, occupancy) can
    // have independent x/y cell counts -- the collision grid is used for
    // neighbor lookup (see FindNeighborsKernel / "collision_grid" there),
    // distinct from the force grid (RL action output) and occupancy grid
    // (RL observation input).
    float dt = 0.0001f;
    int collision_grid_size_x = 32;
    int collision_grid_size_y = 32;
    int particles_per_cell = 64;
    int force_grid_size_x = 16;
    int force_grid_size_y = 16;
    int occupancy_grid_size_x = 32;
    int occupancy_grid_size_y = 32;
    int threads_per_block = 256;

    // Domain bounds
    DomainParams domain{-1.0f, 1.0f, -1.0f, 1.0f};

    // Physics
    PhysicsParams physics{9.81f, 0.01f, 100.0f, -1.0f, 1.0f, -1.0f, 1.0f, 0.3f};

    // Per-material masses, indexed by material id (0 = wall, 1 = snow, 2 = sled).
    std::vector<float> masses{0.0f, 0.04f, 0.04f};

    // Initialization
    std::string init_type = "cube";  // "cube" | "two_particles" | "single_particle"
    float init_x0 = -0.5f;
    float init_y0 = 0.0f;
    float init_vx0 = 0.0f;  // single_particle only; cube/two_particles always start at rest
    float init_vy0 = 0.0f;
    float cube_length_x = 1.0f;  // particle spacing is derived from physics.particle_radius
    float cube_length_y = 1.0f;
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
    bool kernel_timing = true;

    // Shared dt x max_force sweep axes (YAML "sweep:" section), consumed by
    // both stability_and_accuracy and model_problem. Empty means "just use
    // the single simulation.dt / physics.max_force value" -- both drivers
    // treat that as a degenerate 1x1 sweep, so an unswept config behaves
    // exactly like a single run.
    std::vector<float> sweep_dt{};
    std::vector<float> sweep_max_force{};
    // Third sweep axis (same "sweep:" section, key "restitution"), currently
    // only consumed by model_problem (not stability_and_accuracy). Empty
    // means "just use the single physics.restitution value", same
    // degenerate-1-value convention as the two axes above.
    std::vector<float> sweep_restitution{};

    // Stability & accuracy driver (build/bin/stability_and_accuracy)
    std::string stability_mode = "fixed_time";  // "fixed_time" | "steady_state"
    float stability_sim_time = 0.1f;            // max simulated time per run (cap in both modes)
    float stability_acceleration_cutoff = 0.01f;  // steady when max|accel| < cutoff * gravity
    int stability_steps_per_steadiness_check = 100;  // how often (in steps) to test steadiness

    // Model problem driver (build/bin/model_problem)
    float model_problem_sim_time = 1.0f;  // seconds of simulated time to record and plot

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
