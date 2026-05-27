#include <stdio.h>
#include "particle_dynamics_cuda.h"


__global__ void compute_rhs_kernel(const float* state, const int* material,
        const float* mass, float* rhs, const int* grid, size_t n, int grid_size) {
    printf("ff\n");
    const float g = 9.81f;
    float floor_y = -1.0f;
    float wall_x = -1.0f;
    float max_force = 1000.f;
    float r0 = 0.05f;
    float r1 = 0.1f;
    for (size_t i = 0; i < n; ++i) {
        const float x = state[4 * i + 0];
        const float y = state[4 * i + 1];
        const float vx = state[4 * i + 2];
        const float vy = state[4 * i + 3];
        float force_x = 0;
        float force_y = 0;
        const size_t mat = material[i];
//        // Gravity
        force_y -= mass[mat] * g;
        printf("Particle %lu: mat=%lu, pos=(%.2f, %.2f), vel=(%.2f, %.2f), force=(%.2f, %.2f)\n",
               i, mat, x, y, vx, vy, force_x, force_y);
//        // Floor force
//        force_y += c_r[mat][0] * powf(1 / (y - floor_y), 4); // Repulsive force
        float floor_force = max(0.f, min(max_force, max_force / (r0 - r1) * (y - floor_y - r1)));
        force_y += floor_force; // Repulsive force
        force_y -= floor_force * vy; // Damping based on velocity
//            // Wall on the left (at -1)
//            float wall_force = max(0, min(max_force, max_force / (r0 - r1) * (x - wall_x - r1)));
//            force_x += wall_force; // Repulsive force
//            force_x -= wall_force * vx; // Damping based on velocity


        int cell_x = min(grid_size - 1, max(0, static_cast<int>((x + 1.0f) / 2.0f * grid_size)));
        int cell_y = min(grid_size - 1, max(0, static_cast<int>((y + 1.0f) / 2.0f * grid_size)));
        for (int dx = -3; dx <= 3; ++dx) {
            for (int dy = -3; dy <= 3; ++dy) {
                int neighbor_cell_x = cell_x + dx;
                int neighbor_cell_y = cell_y + dy;
                if (neighbor_cell_x >= 0 && neighbor_cell_x < grid_size && neighbor_cell_y >= 0 && neighbor_cell_y < grid_size) {
                    int j = grid[cell_x * grid_size + cell_y];
                    if (i != j) {
                        const float x_j = state[4 * j + 0];
                        const float y_j = state[4 * j + 1];
                        const float vx_j = state[4 * j + 2];
                        const float vy_j = state[4 * j + 3];
                        const float dx = x - x_j;
                        const float dy = y - y_j;
                        const float dist = sqrtf(dx * dx + dy * dy);
                        const size_t mat_j = material[j];
                        // Repulsive force
                        float force = max(0.f, min(max_force, max_force / (r0 - r1) * (dist - r1)));
                        force_y += force * (dy / dist);
                        force_x += force * (dx / dist);
                        // Damping based on relative velocity
                        force_x -= force * (vx - vx_j);
                        force_y -= force * (vy - vy_j);
                    }
                }
            }
        }

        const float ax = force_x / mass[mat];
        const float ay = force_y / mass[mat];

        rhs[4 * i + 0] = vx; // dx/dt = vx
        rhs[4 * i + 1] = vy; // dy/dt = vy
        rhs[4 * i + 2] = ax; // dvx/dt = ax
        rhs[4 * i + 3] = ay; // dvy/dt = ay
    }
}


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
    printf("t = %.3f: \n", time);
    for (size_t dof = 0; dof < 4*n; ++dof) {
        printf("%.2f ", state[dof]);
    }
    printf("\n");

//    // I want the dt to be .001 when y (aka state[1]) is around -1,
//    // and .01 when y is around 0
//    dt = 0.01f * (1 + system.state[1]) - 0.001f * system.state[1];
    dt = 0.001f;
}

__global__ void update_grid_kernel(int* grid, const float* state, size_t n, int grid_size) {
    printf("gg\n");
    for (size_t i = 0; i < n; ++i) {
        int cell_x = min(grid_size - 1, max(0, static_cast<int>((state[4 * i + 0] + 1.0f) / 2.0f * grid_size)));
        int cell_y = min(grid_size - 1, max(0, static_cast<int>((state[4 * i + 1] + 1.0f) / 2.0f * grid_size)));
        // Add particle i to the appropriate cell in the grid
        grid[cell_x * grid_size + cell_y] = i; // This assumes one particle per
                                               // cell TODO
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
    grid_size = 10;
    cudaMalloc((void**)&device_grid, grid_size * grid_size * sizeof(int*));

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
    cudaError_t err;

    err = cudaDeviceSynchronize();
    printf("kernel: %s\n", cudaGetErrorString(err));

printf("host_state = %p\n", host_state);
printf("device_state = %p\n", device_state);


cudaGetLastError(); // clear old errors

cudaPointerAttributes attr;
err =
    cudaPointerGetAttributes(&attr, device_state);
printf("attr err = %s\n",
       cudaGetErrorString(err));
printf("type = %d\n", (int)attr.type);

printf("bytes = %llu\n",
       (unsigned long long)(4 * n * sizeof(float)));
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


    printf("after unpack\n");
err =
    cudaPointerGetAttributes(&attr, device_state);
printf("attr err = %s\n",
       cudaGetErrorString(err));
printf("type = %d\n", (int)attr.type);
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


void ParticleDynamicsCUDA::take_step() {
    cudaError_t err;

    printf("in take step\n");
cudaPointerAttributes attr;
err =
    cudaPointerGetAttributes(&attr, device_state);
printf("attr err = %s\n",
       cudaGetErrorString(err));
printf("type = %d\n", (int)attr.type);


    update_grid_kernel<<<1,1>>>(device_grid, device_state, n, grid_size);
    err = cudaDeviceSynchronize();
    if (err != cudaSuccess) {
        printf("CUDA error: %s\n", cudaGetErrorString(err));
    }
    printf("Updated grid for %d particles.\n", n);

    compute_rhs_kernel<<<1,1>>>(device_state, device_material,
            device_mass, device_rhs, device_grid, n, grid_size);
    err = cudaDeviceSynchronize();
    if (err != cudaSuccess) {
        printf("CUDA error: %s\n", cudaGetErrorString(err));
    }
    printf("Computed rhs for %d particles.\n", n);

    take_step_kernel<<<1,1>>>(device_state, device_rhs, n, time, dt);
    err = cudaDeviceSynchronize();
    if (err != cudaSuccess) {
        printf("CUDA error: %s\n", cudaGetErrorString(err));
    }
    printf("took step\n");

    printf("Now type is:\n");
err =
    cudaPointerGetAttributes(&attr, device_state);
printf("attr err = %s\n",
       cudaGetErrorString(err));
printf("type = %d\n", (int)attr.type);
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
