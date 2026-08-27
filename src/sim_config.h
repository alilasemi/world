#pragma once
#include <string>
#include <vector>

// Spatial dimension and per-particle state layout. The simulation is 3D:
// state is a flat array of kStateStride floats per particle, laid out as
// [x, y, z, vx, vy, vz]. z is "up" -- gravity acts along -z -- which is the
// usual convention for a DEM/CFD code, at the cost of one axis flip in the
// renderer's view matrix (GL convention is y-up).
//
// These exist so the layout is stated once rather than as a bare `4 *` (now
// `6 *`) scattered across every kernel. Position components live at offsets
// 0..kDim-1 and velocity components at kDim..kStateStride-1.
constexpr int kDim = 3;
constexpr int kStateStride = 2 * kDim;

// Number of cells in the neighbor-search stencil: 3^kDim (3x3x3 in 3D). Used
// to size the flat neighbor array as n * kStencilCells * particles_per_cell.
constexpr int kStencilCells = 27;

// Small plain-old-data structs passed by value into the CUDA kernels. They hold
// only the values each kernel needs device-side, so they're trivially copyable
// and safe as __global__ arguments.

// Simulation domain bounds. Kernels map a particle (x, y, z) into a grid cell
// via (x - x_min) / (x_max - x_min), so making these configurable lets the
// domain be something other than the historical [-1, 1] cube.
struct DomainParams {
    float x_min;
    float x_max;
    float y_min;
    float y_max;
    float z_min;
    float z_max;
};

// Physics constants consumed by ComputeRHSKernel (and a subset by EnergyKernel).
struct PhysicsParams {
    float gravity;
    float particle_radius;
    float max_force;
    // The six domain boundaries. z is up, so floor/ceiling are the z walls and
    // the remaining four are the vertical sides.
    float floor_z;
    float ceiling_z;
    float wall_x_min;
    float wall_x_max;
    float wall_y_min;
    float wall_y_max;
    // Target coefficient of restitution (0 = fully inelastic/critically
    // damped, 1 = perfectly elastic/no damping) for the linear
    // spring-dashpot contact model in ComputeRHSKernel. See
    // restitution_to_damping() in compute_rhs_kernel.cu for how this maps
    // to an actual dashpot coefficient.
    float restitution;
    // Coulomb friction coefficient (mu) for the tangential contact force.
    // 0 reproduces the old frictionless behavior, in which the material has no
    // yield stress and therefore no angle of repose -- a released pile spreads
    // until flat. ~0.5 is representative of dry sand.
    float friction;
};

// Full host-side configuration.
struct SimConfig {
    // Simulation / grid. The collision grid is used for neighbor lookup (see
    // FindNeighborsKernel / "collision_grid" there); its x/y/z cell counts are
    // independently configurable.
    float dt = 0.0001f;
    int collision_grid_size_x = 32;
    int collision_grid_size_y = 32;
    int collision_grid_size_z = 32;
    int particles_per_cell = 64;
    int threads_per_block = 256;

    // Coarse grid the density/momentum latent is deposited onto (see
    // DensityGridKernel). Sized INDEPENDENTLY of the collision grid: the
    // collision grid must be about one particle diameter per cell for the
    // neighbor search to stay cheap, whereas the latent wants to be coarse
    // enough that a learned model has few outputs and each node averages many
    // grains.
    int density_grid_size_x = 32;
    int density_grid_size_y = 32;
    int density_grid_size_z = 32;

    // Domain bounds
    DomainParams domain{-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f};

    // Physics: gravity, radius, max_force, floor_z, ceiling_z,
    // wall_x_min/max, wall_y_min/max, restitution.
    PhysicsParams physics{9.81f, 0.01f, 100.0f,
                          -1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 0.3f, 0.5f};

    // Per-material masses, indexed by material id: 0 = wall (massless/fixed),
    // 1 = sand. The "snow" and "sled" materials were leftovers from an earlier
    // sled-on-snow experiment and are gone.
    std::vector<float> masses{0.0f, 0.04f};

    // Initialization. The default cube is deliberately small (0.2 on a side at
    // radius 0.01 -> 10^3 = 1000 particles): in 3D the particle count scales as
    // the cube of the side length, and the GoogleTest suite constructs a
    // default-configured sim repeatedly.
    std::string init_type = "cube";  // "cube" | "two_particles" | "single_particle" | "file"
    // For init_type "file": path to a flat binary state, int32 n followed by
    // n*kStateStride float32 in host_state layout. Written by
    // surrogate/decode_particles.py, which lets a surrogate-predicted state be
    // handed back to the solver.
    std::string init_state_path = "";
    float init_x0 = -0.5f;
    float init_y0 = 0.0f;
    float init_z0 = 0.0f;
    // Initial velocity, applied to EVERY grain of a "cube" (and to the lone
    // "single_particle"). This is what makes a cube a thrown blob.
    float init_vx0 = 0.0f;
    float init_vy0 = 0.0f;
    float init_vz0 = 0.0f;
    // Random perturbation applied to each cube-lattice position, as a fraction
    // of particle_radius, drawn uniformly in [-jitter, +jitter] per axis.
    //
    // This is not cosmetic. A perfect simple-cubic lattice is *exactly*
    // symmetric: every sphere sits directly atop another with a vertical
    // contact normal, so there is no lateral force anywhere and the block is
    // self-supporting no matter how far it is dropped. Frictionless, it still
    // collapsed because floating-point asymmetries grew unopposed; with
    // friction those are suppressed and the lattice survives intact forever,
    // which is a degenerate initial condition rather than granular material.
    // A little jitter breaks the symmetry and gives a genuinely disordered
    // packing. The same knob supplies the epsilon-perturbed initial conditions
    // the predictability-horizon measurement needs.
    //
    // Deterministic: seeded from init_seed, so repeated runs of the same config
    // are still bit-identical (RepeatedRunsAreDeterministic relies on this).
    float init_jitter = 0.0f;
    unsigned int init_seed = 12345u;

    // A SECOND, independent random displacement applied on top of the jittered
    // lattice, with its own magnitude (again as a fraction of particle_radius)
    // and seed. This exists to generate ensembles that share an initial
    // condition to within a controlled perturbation:
    //   * tiny magnitude, varying perturbation_seed -> members that start
    //     almost identically. Their separation over time measures the Lyapunov
    //     growth rate and hence the predictability horizon.
    //   * magnitude comparable to init_jitter -> members that are different
    //     realizations of the same macroscopic state. Their spread is the
    //     irreducible scatter a surrogate conditioned on theta cannot beat.
    // Kept separate from init_jitter so the base packing can be held fixed
    // (common random numbers) while only the perturbation varies.
    float init_perturbation = 0.0f;
    unsigned int init_perturbation_seed = 1u;
    float cube_length_x = 0.2f;  // particle spacing is derived from physics.particle_radius
    float cube_length_y = 0.2f;
    float cube_length_z = 0.2f;
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

    // Dataset driver (build/bin/dataset): how long each rollout runs and how
    // many density-grid snapshots to record along the way. Recording several
    // snapshots rather than only the final one is what lets ONE dataset serve
    // both the one-shot outcome surrogate (theta -> final field) and the
    // autoregressive latent-dynamics model (field_t -> field_t+1).
    float dataset_sim_time = 2.0f;
    int dataset_checkpoints = 10;

    // Time integration
    std::string time_integrator = "semi_implicit_euler";  // "semi_implicit_euler" | "backward_euler_picard"
    int picard_iterations = 3;
};

// Loads a config from a YAML file, starting from defaults and overriding only
// the keys present in the file. If the file cannot be opened, prints a notice to
// stderr and returns the defaults (so drivers and tests still work without one).
SimConfig load_config(const std::string& path);
