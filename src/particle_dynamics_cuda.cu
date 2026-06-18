#include <stdio.h>
#include <assert.h>
#include "particle_dynamics_cuda.h"



__global__ void take_step_kernel(float* state, const float* rhs, size_t n, float time, float dt) {
    // Update velocities
    for (size_t i = 0; i < n; ++i) {
        state[4*i + 2] += dt * rhs[4*i + 2];
        state[4*i + 3] += dt * rhs[4*i + 3];
    }
    // Then, update positions using the new velocities
    for (size_t i = 0; i < n; ++i) {
        state[4*i + 0] += dt * state[4*i + 2];
        state[4*i + 1] += dt * state[4*i + 3];
    }
    time += dt;
//    printf("t = %.3f: \n", time);
//    for (size_t dof = 0; dof < 4*n; ++dof) {
//        printf("%.2f ", state[dof]);
//    }
//    printf("\n");

//    // I want the dt to be .001 when y (aka state[1]) is around -1,
//    // and .01 when y is around 0
//    dt = 0.01f * (1 + system.state[1]) - 0.001f * system.state[1];
    dt = 0.001f;
}

__global__ void update_grid_kernel(int* grid, const float* state, size_t n, int
        grid_size, int particles_per_cell) {
    // Let the first "particle" position actually be the count of particles in
    // this cell. Initialize to zero
    for (size_t i = 0; i < n; ++i) {
        int cell_x = min(grid_size - 1, max(0, static_cast<int>((state[4 * i + 0] + 1.0f) / 2.0f * grid_size)));
        int cell_y = min(grid_size - 1, max(0, static_cast<int>((state[4 * i + 1] + 1.0f) / 2.0f * grid_size)));
        grid[cell_x * grid_size * particles_per_cell + cell_y * particles_per_cell] = 0;
    }
    // Then, loop over particles and add them to the grid
    for (size_t i = 0; i < n; ++i) {
        int cell_x = min(grid_size - 1, max(0, static_cast<int>((state[4 * i + 0] + 1.0f) / 2.0f * grid_size)));
        int cell_y = min(grid_size - 1, max(0, static_cast<int>((state[4 * i + 1] + 1.0f) / 2.0f * grid_size)));
        int index = cell_x * grid_size * particles_per_cell + cell_y * particles_per_cell;
        grid[index] += 1; // Increment count of particles in this cell
        if (grid[index] < particles_per_cell) {
            grid[index + grid[index]] = i; // Store particle index
        } else {
            // Handle overflow (too many particles in this cell)
            printf("Error: too many particles in cell (%d, %d)\n", cell_x, cell_y);
            assert(false);
        }
    }
}


ParticleDynamicsCUDA::ParticleDynamicsCUDA() {
    // Initialize on host
//    initialize_to_two_particles(0.0f, 0.0f);
    initialize_to_cube(0.0f, 0.0f);
    // Copy to device
    cudaError_t err = cudaMemcpy(device_state, host_state, 4 * n * sizeof(float), cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        printf("Memcpy failed: %s\n",
               cudaGetErrorString(err));
    }
    err = cudaMemcpy(device_material, host_material, n * sizeof(int), cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        printf("Memcpy failed: %s\n",
               cudaGetErrorString(err));
    }
    unpack_state();

    // Create grid
    grid_size = 16;
    particles_per_cell = 10;
    cudaMalloc((void**)&device_grid, grid_size * grid_size * particles_per_cell * sizeof(int*));

    time = 0.0f;
    // I will assign material 0 to be the walls, material 1 to be the snow, and
    // material 2 to be the sled
    host_mass = (float*)malloc(3 * sizeof(float));
    cudaMalloc((void**)&device_mass, 3 * sizeof(float));

    host_mass[0] = 0.0f;
    host_mass[1] = 1.0f;
    host_mass[2] = 10.0f;
    cudaMemcpy(device_mass, host_mass, 3 * sizeof(float), cudaMemcpyHostToDevice);

    dt = 0.001f;

//    size_t grid_size = 16;
//    grid = std::vector<std::vector<std::vector<size_t>>>(grid_size, std::vector<std::vector<size_t>>(grid_size));
    cudaEventCreate(&start_event);
    cudaEventCreate(&stop_event);

    // Create CUDA kernels
    compute_rhs_kernel = std::make_unique<ComputeRHSKernel>(device_state, device_material, device_mass, device_grid,
            n, grid_size, particles_per_cell, device_rhs);
}


void ParticleDynamicsCUDA::resize(const int new_n) {
    cudaError_t err;
    n = new_n;
    xy = std::vector<float>(2*n);

    host_state = (float*)malloc(4 * n * sizeof(float));
    err = cudaMalloc((void**)&device_state, 4 * n * sizeof(float));
    printf("cudaMalloc device_state: %s\n", cudaGetErrorString(err));

cudaPointerAttributes attr;
err =
    cudaPointerGetAttributes(&attr, device_state);
printf("attr err = %s\n",
       cudaGetErrorString(err));
printf("type = %d\n", (int)attr.type);

    host_rhs = (float*)malloc(4 * n * sizeof(float));
    err = cudaMalloc((void**)&device_rhs, 4 * n * sizeof(float));
    printf("cudaMalloc device_rhs: %s\n", cudaGetErrorString(err));

    host_material = (int*)malloc(n * sizeof(int));
    err = cudaMalloc((void**)&device_material, n * sizeof(int));
    printf("cudaMalloc device_material: %s\n", cudaGetErrorString(err));
}


void ParticleDynamicsCUDA::unpack_state() {
    start_timer();
    cudaError_t err;

    err = cudaDeviceSynchronize();
    printf("kernel: %s\n", cudaGetErrorString(err));

    err = cudaMemcpy(host_state, device_state, 4 * n * sizeof(float), cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) {
        printf("Memcpy failed: %s\n",
               cudaGetErrorString(err));
        exit(1);
    }

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
    cudaPointerGetAttributes(&attr, device_state);
printf("attr err = %s\n",
       cudaGetErrorString(err));
printf("type = %d\n", (int)attr.type);
}


__global__ void k() {
    printf("hello\n");
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
    cudaError_t err;

    start_timer();
    update_grid_kernel<<<1,1>>>(device_grid, device_state, n, grid_size,
            particles_per_cell);
    err = cudaDeviceSynchronize();
    if (err != cudaSuccess) {
        printf("CUDA error: %s\n", cudaGetErrorString(err));
    }
    printf("Updated grid for %d particles.\n", n);
    stop_timer(time_update_grid);

    start_timer();
    (*compute_rhs_kernel)();
    err = cudaDeviceSynchronize();
    if (err != cudaSuccess) {
        printf("CUDA error: %s\n", cudaGetErrorString(err));
    }
    printf("Computed rhs for %d particles.\n", n);
    stop_timer(time_compute_rhs);

    start_timer();
    take_step_kernel<<<1,1>>>(device_state, device_rhs, n, time, dt);
    err = cudaDeviceSynchronize();
    if (err != cudaSuccess) {
        printf("CUDA error: %s\n", cudaGetErrorString(err));
    }
    printf("took step\n");
    stop_timer(time_take_step);
}

ParticleDynamicsCUDA::~ParticleDynamicsCUDA() {
    free(host_state);
    cudaFree(device_state);
    free(host_rhs);
    cudaFree(device_rhs);
    free(host_material);
    cudaFree(device_material);
    free(host_mass);
    cudaFree(device_mass);
}
