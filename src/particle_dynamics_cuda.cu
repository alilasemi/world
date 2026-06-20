#include <stdio.h>
#include <assert.h>
#include "particle_dynamics_cuda.h"
#include "cuda_check.h"


ParticleDynamicsCUDA::ParticleDynamicsCUDA() {
    cudaEventCreate(&start_event);
    cudaEventCreate(&stop_event);

    // Initialize on host
//    initialize_to_two_particles(0.0f, 0.0f);
    initialize_to_cube(0.0f, 0.0f);
    // Copy to device
    device_state.copy_from_host(host_state);
    device_material.copy_from_host(host_material);
    unpack_state();

    // Create grid
    grid_size = 16;
    particles_per_cell = 10;
    device_grid = DeviceVector<int>(grid_size * grid_size * particles_per_cell);

    time = 0.0f;
    // I will assign material 0 to be the walls, material 1 to be the snow, and
    // material 2 to be the sled
    host_mass = HostVector<float>(3);
    device_mass = DeviceVector<float>(3);

    host_mass[0] = 0.0f;
    host_mass[1] = 1.0f;
    host_mass[2] = 10.0f;
    device_mass.copy_from_host(host_mass);

    dt = 0.001f;

//    size_t grid_size = 16;
//    grid = std::vector<std::vector<std::vector<size_t>>>(grid_size, std::vector<std::vector<size_t>>(grid_size));

    // Create CUDA kernels
    update_grid_kernel = std::make_unique<UpdateGridKernel>(device_grid.data(), device_state.data(),
            n, grid_size, particles_per_cell);
    compute_rhs_kernel = std::make_unique<ComputeRHSKernel>(device_state.data(), device_material.data(),
            device_mass.data(), device_grid.data(), n, grid_size, particles_per_cell, device_rhs.data());
    take_step_kernel = std::make_unique<TakeStepKernel>(device_state.data(), device_rhs.data(), n, dt);
}


void ParticleDynamicsCUDA::resize(const int new_n) {
    n = new_n;
    xy = std::vector<float>(2*n);

    host_state = HostVector<float>(4 * n);
    device_state = DeviceVector<float>(4 * n);
    device_rhs = DeviceVector<float>(4 * n);
    host_material = HostVector<int>(n);
    device_material = DeviceVector<int>(n);
}


void ParticleDynamicsCUDA::unpack_state() {
    start_timer();

    CUDA_CHECK(cudaDeviceSynchronize());

    host_state.copy_from_device(device_state);

    printf("Unpacking state for %d particles...\n", n);
    for (size_t i = 0; i < n; ++i) {
        printf("Particle %lu: host_state = (%.2f, %.2f, %.2f, %.2f)\n", i, host_state[4 * i + 0], host_state[4 * i + 1], host_state[4 * i + 2], host_state[4 * i + 3]);
        xy[2*i + 0] = host_state[4 * i + 0];
        xy[2*i + 1] = host_state[4 * i + 1];
    }
    printf("Unpacked state for %d particles.\n", n);
    stop_timer(time_unpack_state);
}



void ParticleDynamicsCUDA::initialize_to_two_particles(const float x0, const float y0) {
    printf("Initializing to two particles at (%.2f, %.2f) and (%.2f, %.2f)...\n", x0, y0, x0, y0 + 0.1f);
    resize(2);

    printf("Resized to %d particles.\n", n);
    host_state[0] = x0;
    host_state[1] = y0;
    host_state[2] = 0.0f;
    host_state[3] = 0.0f;

    printf("Initialized first particle at (%.2f, %.2f).\n", host_state[0], host_state[1]);
    host_state[4] = x0;
    host_state[5] = y0 + 0.1f;
    host_state[6] = 0.0f;
    host_state[7] = 0.0f;

    host_material[0] = 1;
    host_material[1] = 1;

    printf("Initialized second particle at (%.2f, %.2f).\n", host_state[4],
            host_state[5]);
}


void ParticleDynamicsCUDA::initialize_to_cube(const float x0, const float y0) {
    size_t num_per_side = 10;
    resize(num_per_side * num_per_side + 1);

    for (size_t i = 0; i < num_per_side; ++i) {
        for (size_t j = 0; j < num_per_side; ++j) {
            host_state[4 * (i * num_per_side + j) + 0] = x0 + i * 0.1f;
            host_state[4 * (i * num_per_side + j) + 1] = y0 + j * 0.1f;
            host_state[4 * (i * num_per_side + j) + 2] = 0.0f;
            host_state[4 * (i * num_per_side + j) + 3] = 0.0f;
        }
    }

    // Make one particle of sled on top
    host_state[4 * (n - 1) + 0] = -.9;
    host_state[4 * (n - 1) + 1] = .9;
    host_state[4 * (n - 1) + 2] = .4;
    host_state[4 * (n - 1) + 3] = 0;

    // Set all particles to be material 1 (snow) except for the last one
    for (size_t i = 0; i < n - 1; ++i) {
        host_material[i] = 1;
    }
    host_material[n - 1] = 2;

    cudaError_t err;

    printf("in init cube\n");
cudaPointerAttributes attr;
err =
    cudaPointerGetAttributes(&attr, device_state.data());
printf("attr err = %s\n",
       cudaGetErrorString(err));
printf("type = %d\n", (int)attr.type);
}


void ParticleDynamicsCUDA::start_timer() {
    cudaEventRecord(start_event);
}

void ParticleDynamicsCUDA::stop_timer(float& elapsed_time) {
    cudaEventRecord(stop_event);
    cudaEventSynchronize(stop_event);
    float time = 0.f;
    cudaEventElapsedTime(&time, start_event, stop_event);
    elapsed_time += time;
}


void ParticleDynamicsCUDA::take_step() {
    (*update_grid_kernel)();
    time_update_grid = update_grid_kernel->wall_clock_time();

    (*compute_rhs_kernel)();
    time_compute_rhs = compute_rhs_kernel->wall_clock_time();

    take_step_kernel->set_dt(dt);
    (*take_step_kernel)();
    time_take_step = take_step_kernel->wall_clock_time();
}

ParticleDynamicsCUDA::~ParticleDynamicsCUDA() = default;
