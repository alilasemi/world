#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <cuda_runtime.h>

#include "compute_rhs_kernel.h"
#include "device_vector.h"
#include "density_grid_kernel.h"
#include "energy_kernel.h"
#include "find_neighbors_kernel.h"
#include "host_vector.h"
#include "sim_config.h"
#include "backward_euler_picard_kernel.h"
#include "semi_implicit_euler_kernel.h"

class ParticleDynamics {
public:
    // Full configuration this sim was constructed from. Defaults reproduce the
    // historical hardcoded behavior.
    SimConfig config;

    float time;
    float last_time;
    std::chrono::steady_clock::time_point last_wall_clock_time;
    float real_time_ratio;
    int n;
    float dt;
    int collision_grid_size_x;
    int collision_grid_size_y;
    int collision_grid_size_z;
    int particles_per_cell;

    // Positions only (kDim floats per particle), refreshed by unpack_state()
    // and streamed to the client. Named for what it holds: the full state
    // including velocities lives in host_state.
    std::vector<float> positions;

    HostVector<float> host_state;
    DeviceVector<float> device_state;
    DeviceVector<float> device_state_n;  // x^n saved at start of each Picard step
    DeviceVector<float> device_rhs;
    HostVector<int> host_material;
    DeviceVector<int> device_material;
    HostVector<float> host_mass;
    DeviceVector<float> device_mass;

    // Flat per-particle neighbor list, populated each step by
    // FindNeighborsKernel and consumed by ComputeRHSKernel.
    // Sized n*kStencilCells*particles_per_cell (27 = 3x3x3 stencil).
    // Each particle's row is terminated by a -1 sentinel.
    DeviceVector<int> device_neighbors;

    DeviceVector<float> device_energy;
    HostVector<float> host_energy;

    // Coarse density/momentum latent (see DensityGridKernel). Channel-major:
    // channel 0 is mass, channels 1..kDim are momentum components. Refreshed on
    // demand by compute_density_grid(), not every step.
    DeviceVector<float> device_density_grid;
    HostVector<float> host_density_grid;
    int density_grid_size_x;
    int density_grid_size_y;
    int density_grid_size_z;

    // Timing
    float time_unpack_state = 0.f;
    cudaEvent_t start_event, stop_event;
    void start_timer();
    void stop_timer(float& elapsed_time);


    // Defaults to a built-in config that matches the original hardcoded values,
    // so existing call sites (tests, profiling/stability drivers) keep working.
    explicit ParticleDynamics(const SimConfig& config_ = SimConfig{});

    void resize(const int new_n);

    void unpack_state();
    float get_real_time_ratio();

    // Lifetime-accumulated wall-clock times for each kernel (ms).
    // Take before/after deltas to get per-frame costs.
    float find_neighbors_wct()    const;
    float compute_rhs_wct()       const;
    float take_step_wct()         const;
    float density_grid_wct()      const;


    void initialize_to_two_particles(const float x0, const float y0, const float z0);

    void initialize_to_cube(const float x0, const float y0, const float z0);

    void initialize_to_single_particle(const float x0, const float y0, const float z0);

    // Loads a state written by surrogate/decode_particles.py. Enables the
    // round trip: simulate -> encode -> predict -> decode -> simulate again.
    void initialize_from_file(const std::string& path);

    void take_step();

    float compute_total_energy();

    // Deposits mass and momentum onto the coarse latent grid and copies it to
    // host_density_grid. Returns the number of floats written (kChannels *
    // nodes). Trilinear/CIC deposit, so total deposited mass equals the total
    // particle mass exactly.
    size_t compute_density_grid();

    // Number of nodes per channel in the density grid.
    size_t density_grid_nodes() const;

    // Copies device state to host and checks whether every particle's
    // position is finite and inside the configured domain box.
    bool is_stable();

    // Max |acceleration| over all particles, from the most recent take_step()'s
    // ComputeRHSKernel pass (the velocity half of device_rhs).
    float compute_max_acceleration();

    // Lifetime-cumulative count of collision-grid overflow incidents (see
    // FindNeighborsKernel::overflow_count()) -- a nonzero value means the
    // simulation has diverged badly enough to overflow particles_per_cell at
    // some point. Not checked automatically every step; poll occasionally.
    int grid_overflow_count() const;

    ~ParticleDynamics();

    // Prevent copying and assignment. It's error prone since CPU code can copy
    // these device pointers and cause problems.
    ParticleDynamics(const ParticleDynamics&) = delete;
    ParticleDynamics& operator=(const ParticleDynamics&) = delete;

private:
    std::unique_ptr<FindNeighborsKernel> find_neighbors_kernel;
    std::unique_ptr<ComputeRHSKernel> compute_rhs_kernel;
    std::unique_ptr<SemiImplicitEulerKernel> semi_implicit_euler_kernel;
    std::unique_ptr<BackwardEulerPicardKernel> backward_euler_picard_kernel;
    std::unique_ptr<DensityGridKernel> density_grid_kernel;
    std::unique_ptr<EnergyKernel> energy_kernel;
};
