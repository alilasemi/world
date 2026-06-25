#include <vector>
#include <memory>
#include <chrono>
#include <cuda_runtime.h>

#include "compute_rhs_kernel.h"
#include "device_vector.h"
#include "energy_kernel.h"
#include "find_neighbors_kernel.h"
#include "host_vector.h"
#include "interpolate_force_kernel.h"
#include "occupancy_grid_kernel.h"
#include "take_step_kernel.h"

class ParticleDynamicsCUDA {
public:
    float time;
    float last_time;
    std::chrono::steady_clock::time_point last_wall_clock_time;
    float real_time_ratio;
    int n;
    float dt;
    int grid_size;
    int particles_per_cell;
    int force_grid_size;

    std::vector<float> xy;

    HostVector<float> host_state;
    DeviceVector<float> device_state;
    DeviceVector<float> device_rhs;
    HostVector<int> host_material;
    DeviceVector<int> device_material;
    HostVector<float> host_mass;
    DeviceVector<float> device_mass;

    // Flat per-particle neighbor list, populated each step by
    // FindNeighborsKernel (which does the actual spatial-grid lookup
    // internally, via cuco hash maps it owns privately) and consumed by
    // ComputeRHSKernel. Sized n*9*particles_per_cell: 9 = the 3x3 stencil's
    // cell count, particles_per_cell = worst-case occupancy per cell
    // (enforced by an overflow assert in FindNeighborsKernel). Each
    // particle's row is terminated by a -1 sentinel once its neighbors run
    // out. Neither ParticleDynamicsCUDA nor ComputeRHSKernel ever touch
    // cuco directly -- that's the point of this flat array as the
    // interface, instead of passing hash-map handles around.
    DeviceVector<int> device_neighbors;

    DeviceVector<float> device_energy;
    HostVector<float> host_energy;

    // Per-particle body force, interpolated every step from the m*m grid
    // fields below and consumed by ComputeRHSKernel.
    DeviceVector<float> device_body_force_x;
    DeviceVector<float> device_body_force_y;

    // m*m externally-supplied (e.g. AI-predicted) force field, one scalar per
    // cell, row-major grid[cell_x * force_grid_size + cell_y]. Zeroed at
    // construction; nothing else writes these until an external caller
    // uploads a field via DeviceVector::copy_from_host.
    DeviceVector<float> device_grid_force_x;
    DeviceVector<float> device_grid_force_y;

    // m*m occupancy snapshot (0/1), same layout as above. Only refreshed when
    // update_occupancy_grid() is called -- not part of take_step().
    DeviceVector<int> device_occupancy_grid;

    // Timing (per-frame; reset/accumulated in broadcast.cpp each run)
    float time_update_grid = 0.f;
    float time_interpolate_force = 0.f;
    float time_compute_rhs = 0.f;
    float time_take_step = 0.f;
    float time_unpack_state = 0.f;
    cudaEvent_t start_event, stop_event;
    void start_timer();
    void stop_timer(float& elapsed_time);


    ParticleDynamicsCUDA();

    void resize(const int new_n);

    void unpack_state();
    float get_real_time_ratio();


    void initialize_to_two_particles(const float x0, const float y0);

    void initialize_to_cube(const float x0, const float y0);

    void take_step();

    float compute_total_energy();

    // Copies device state to host and checks whether every particle's
    // (x, y) is finite and within the [-1, 1]x[-1, 1] domain.
    bool is_stable();

    // Refreshes device_occupancy_grid from current particle positions. Not
    // called from take_step() -- a future AI-driving loop must call this
    // itself before reading device_occupancy_grid back to host; the snapshot
    // is only as fresh as the last call.
    void update_occupancy_grid();

    ~ParticleDynamicsCUDA();

    // Prevent copying and assignment. It's error prone since CPU code can copy
    // these device pointers and cause problems.
    ParticleDynamicsCUDA(const ParticleDynamicsCUDA&) = delete;
    ParticleDynamicsCUDA& operator=(const ParticleDynamicsCUDA&) = delete;

private:
    std::unique_ptr<FindNeighborsKernel> find_neighbors_kernel;
    std::unique_ptr<ComputeRHSKernel> compute_rhs_kernel;
    std::unique_ptr<TakeStepKernel> take_step_kernel;
    std::unique_ptr<EnergyKernel> energy_kernel;
    std::unique_ptr<InterpolateForceKernel> interpolate_force_kernel;
    std::unique_ptr<OccupancyGridKernel> occupancy_grid_kernel;
};
