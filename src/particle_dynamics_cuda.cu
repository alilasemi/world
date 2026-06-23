#include <stdio.h>
#include <assert.h>
#include <algorithm>
#include <cmath>
#include "particle_dynamics_cuda.h"
#include "cuda_check.h"
#include "grid_map.h"


ParticleDynamicsCUDA::ParticleDynamicsCUDA() {
    cudaEventCreate(&start_event);
    cudaEventCreate(&stop_event);

    // Initialize on host
//    initialize_to_two_particles(0.0f, 0.0f);
    initialize_to_cube(-.5f, 0.0f);
    // Copy to device
    device_state.copy_from_host(host_state);
    device_material.copy_from_host(host_material);
    unpack_state();

    // Create grid
    grid_size = 256;
    particles_per_cell = 10;
    // Capacity bounds the *number of entries*, not the key's numeric range --
    // GridMap hashes keys into buckets rather than indexing by key value
    // directly. particles_in_cell's composite keys range up to
    // grid_size*grid_size*particles_per_cell, but only ever holds n live
    // entries (one per particle) at a time, so its capacity is sized off n,
    // not that key range.
    size_t num_particles_in_cell_capacity = static_cast<size_t>(2 * std::min(n, grid_size * grid_size)); // bounded by distinct occupied cells
    size_t particles_in_cell_capacity = static_cast<size_t>(2 * n); // exactly one live entry per particle
    num_particles_in_cell = std::make_unique<GridMap>(num_particles_in_cell_capacity,
            cuco::empty_key<int>{kEmptyKeySentinel}, cuco::empty_value<int>{kEmptyValueSentinel},
            cuda::std::equal_to<int>{}, cuco::linear_probing<1, cuco::default_hash_function<int>>{});
    particles_in_cell = std::make_unique<GridMap>(particles_in_cell_capacity,
            cuco::empty_key<int>{kEmptyKeySentinel}, cuco::empty_value<int>{kEmptyValueSentinel},
            cuda::std::equal_to<int>{}, cuco::linear_probing<1, cuco::default_hash_function<int>>{});

    time = 0.0f;
    // I will assign material 0 to be the walls, material 1 to be the snow, and
    // material 2 to be the sled
    host_mass = HostVector<float>(3);
    device_mass = DeviceVector<float>(3);

    host_mass[0] = 0.0f;
    host_mass[1] = .04f;
    host_mass[2] = .04f;
    device_mass.copy_from_host(host_mass);

    dt = 0.0001f;

//    size_t grid_size = 16;
//    grid = std::vector<std::vector<std::vector<size_t>>>(grid_size, std::vector<std::vector<size_t>>(grid_size));

    // m*m grid of externally-supplied (e.g. AI) body forces, plus the
    // occupancy snapshot fed back to that external model. Independent of the
    // 256x256 collision grid above.
    force_grid_size = 16;
    device_body_force_x = DeviceVector<float>(n);
    device_body_force_y = DeviceVector<float>(n);
    device_grid_force_x = DeviceVector<float>(force_grid_size * force_grid_size);
    device_grid_force_y = DeviceVector<float>(force_grid_size * force_grid_size);
    // Nothing else initializes these, and InterpolateForceKernel reads them
    // every step starting from the very first take_step() call -- zero is
    // also the correct "no AI model configured yet" default.
    CUDA_CHECK(cudaMemset(device_grid_force_x.data(), 0,
            static_cast<size_t>(force_grid_size) * static_cast<size_t>(force_grid_size) * sizeof(float)));
    CUDA_CHECK(cudaMemset(device_grid_force_y.data(), 0,
            static_cast<size_t>(force_grid_size) * static_cast<size_t>(force_grid_size) * sizeof(float)));
    device_occupancy_grid = DeviceVector<int>(force_grid_size * force_grid_size);

    // Create CUDA kernels
    update_grid_kernel = std::make_unique<UpdateGridKernel>(particles_in_cell.get(), num_particles_in_cell.get(),
            device_state.data(), n, grid_size, particles_per_cell);
    compute_rhs_kernel = std::make_unique<ComputeRHSKernel>(device_state.data(), device_material.data(),
            device_mass.data(), particles_in_cell.get(), num_particles_in_cell.get(),
            device_body_force_x.data(), device_body_force_y.data(),
            n, grid_size, particles_per_cell, device_rhs.data());
    take_step_kernel = std::make_unique<TakeStepKernel>(device_state.data(), device_rhs.data(), n, dt);

    device_energy = DeviceVector<float>(1);
    host_energy = HostVector<float>(1);
    energy_kernel = std::make_unique<EnergyKernel>(device_state.data(), device_material.data(),
            device_mass.data(), n, device_energy.data());

    interpolate_force_kernel = std::make_unique<InterpolateForceKernel>(device_state.data(),
            device_grid_force_x.data(), device_grid_force_y.data(), n, force_grid_size,
            device_body_force_x.data(), device_body_force_y.data());
    occupancy_grid_kernel = std::make_unique<OccupancyGridKernel>(device_occupancy_grid.data(),
            device_state.data(), n, force_grid_size);
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

    for (size_t i = 0; i < n; ++i) {
//        printf("Particle %lu: host_state = (%.2f, %.2f, %.2f, %.2f)\n", i, host_state[4 * i + 0], host_state[4 * i + 1], host_state[4 * i + 2], host_state[4 * i + 3]);
        xy[2*i + 0] = host_state[4 * i + 0];
        xy[2*i + 1] = host_state[4 * i + 1];
    }
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
    float length = 1.f;
    float radius = 0.01f;
    int num_per_side = static_cast<int>(length / (2 * radius));
    resize(num_per_side * num_per_side + 1);

    for (size_t i = 0; i < num_per_side; ++i) {
        for (size_t j = 0; j < num_per_side; ++j) {
            host_state[4 * (i * num_per_side + j) + 0] = x0 + i * 2 * radius;
            host_state[4 * (i * num_per_side + j) + 1] = y0 + j * 2 * radius;
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

    (*interpolate_force_kernel)();

    (*compute_rhs_kernel)();
    time_compute_rhs = compute_rhs_kernel->wall_clock_time();

    take_step_kernel->set_dt(dt);
    (*take_step_kernel)();
    time_take_step = take_step_kernel->wall_clock_time();
}

float ParticleDynamicsCUDA::compute_total_energy() {
    (*energy_kernel)();
    host_energy.copy_from_device(device_energy);
    return host_energy[0];
}


bool ParticleDynamicsCUDA::is_stable() {
    host_state.copy_from_device(device_state);
    for (int i = 0; i < n; ++i) {
        const float x = host_state[4 * i + 0];
        const float y = host_state[4 * i + 1];
        if (!std::isfinite(x) || !std::isfinite(y) || x < -1.0f || x > 1.0f || y < -1.0f || y > 1.0f) {
            return false;
        }
    }
    return true;
}


void ParticleDynamicsCUDA::update_occupancy_grid() {
    (*occupancy_grid_kernel)();
}


ParticleDynamicsCUDA::~ParticleDynamicsCUDA() = default;
