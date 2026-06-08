#include <vector>
#include <cuda_runtime.h>

class ParticleDynamicsCUDA {
public:
    float time;
    int n;
    float dt;
    int grid_size;
    int particles_per_cell;

    std::vector<float> xy;

    float* host_state;
    float* device_state;
    float* host_rhs;
    float* device_rhs;
    int* host_material;
    int* device_material;
    float* host_mass;
    float* device_mass;

    int* device_grid;

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
};
