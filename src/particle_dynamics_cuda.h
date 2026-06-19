#include <vector>
#include <memory>
#include <cuda_runtime.h>

#include "compute_rhs_kernel.h"
#include "device_vector.h"
#include "host_vector.h"

class ParticleDynamicsCUDA {
public:
    float time;
    int n;
    float dt;
    int grid_size;
    int particles_per_cell;

    std::vector<float> xy;

    HostVector<float> host_state;
    DeviceVector<float> device_state;
    DeviceVector<float> device_rhs;
    HostVector<int> host_material;
    DeviceVector<int> device_material;
    HostVector<float> host_mass;
    DeviceVector<float> device_mass;

    DeviceVector<int> device_grid;

    // Timing
    float time_update_grid = 0.f;
    float time_compute_rhs = 0.f;
    float time_take_step = 0.f;
    float time_unpack_state = 0.f;
    cudaEvent_t start_event, stop_event;
    void start_timer();
    void stop_timer(float& elapsed_time);


    ParticleDynamicsCUDA();

    void resize(const int new_n);

    void unpack_state();


    void initialize_to_two_particles(const float x0, const float y0);

    void initialize_to_cube(const float x0, const float y0);

    void take_step();

    ~ParticleDynamicsCUDA();

    // Prevent copying and assignment. It's error prone since CPU code can copy
    // these device pointers and cause problems.
    ParticleDynamicsCUDA(const ParticleDynamicsCUDA&) = delete;
    ParticleDynamicsCUDA& operator=(const ParticleDynamicsCUDA&) = delete;

private:
    std::unique_ptr<ComputeRHSKernel> compute_rhs_kernel;
};
