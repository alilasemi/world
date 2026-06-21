#include <stdio.h>
#include <assert.h>
#include "compute_rhs_kernel.h"


__global__ void compute_rhs_kernel(const float* state, const int* material,
        const float* mass, float* rhs, const int* grid, const float* body_force_x, const float* body_force_y,
        size_t n, int grid_size, int particles_per_cell) {
    int index = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;

    const float g = 9.81f;
    float floor_y = -1.0f;
    float left_wall_x = -1.0f;
    float right_wall_x = 1.0f;
    float max_force = 100.f;
    float radius = .01f;
    for (int i = index; i < n; i += stride) {
        const float x = state[4 * i + 0];
        const float y = state[4 * i + 1];
        const float vx = state[4 * i + 2];
        const float vy = state[4 * i + 3];
        float force_x = 0;
        float force_y = 0;
        const size_t mat = material[i];

        // Gravity
        force_y -= mass[mat] * g;

        // Floor force
        float floor_dist = abs(y - floor_y) - radius;
        float floor_force = max(0.f, min(max_force, -max_force / radius * floor_dist));
        force_y += floor_force; // Repulsive force
        force_y -= floor_force * vy; // Damping based on velocity
        // Wall on the left
        float wall_dist_left = abs(x - left_wall_x) - radius;
        float left_wall_force = max(0.f, min(max_force, -max_force / radius * wall_dist_left));
        force_x += left_wall_force; // Repulsive force
        force_x -= left_wall_force * vx; // Damping based on velocity
        // Wall on the right
        float wall_dist_right = abs(x - right_wall_x) - radius;
        float right_wall_force = max(0.f, min(max_force, -max_force / radius * wall_dist_right));
        force_x -= right_wall_force; // Repulsive force
        force_x -= right_wall_force * vx; // Damping based on velocity

        // Particle-particle interactions using the spatial grid
        int cell_x = min(grid_size - 1, max(0, static_cast<int>((x + 1.0f) / 2.0f * grid_size)));
        int cell_y = min(grid_size - 1, max(0, static_cast<int>((y + 1.0f) / 2.0f * grid_size)));
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                int neighbor_cell_x = cell_x + dx;
                int neighbor_cell_y = cell_y + dy;
                if (neighbor_cell_x >= 0 && neighbor_cell_x < grid_size && neighbor_cell_y >= 0 && neighbor_cell_y < grid_size) {
                    int index = neighbor_cell_x * grid_size * particles_per_cell + neighbor_cell_y * particles_per_cell;
                    int count = grid[index];
                    for (int particle_idx = 0; particle_idx < count; ++particle_idx) {
                        int j = grid[index + 1 + particle_idx];
                        if (i != j) {
                            const float x_j = state[4 * j + 0];
                            const float y_j = state[4 * j + 1];
                            const float vx_j = state[4 * j + 2];
                            const float vy_j = state[4 * j + 3];
                            const float dx = x - x_j;
                            const float dy = y - y_j;
                            const float norm = sqrtf(dx * dx + dy * dy);
                            const float dist = norm - radius - radius;
                            const size_t mat_j = material[j];
                            // Repulsive force
                            float force = max(0.f, min(max_force, -max_force / radius * dist));
                            force_y += force * (dy / norm);
                            force_x += force * (dx / norm);
                            // Damping based on relative velocity
                            force_x -= force * (vx - vx_j);
                            force_y -= force * (vy - vy_j);
                        }
                    }
                }
            }
        }

        // Externally-supplied (e.g. AI-predicted) body force, already
        // interpolated onto this particle's position.
        force_x += body_force_x[i];
        force_y += body_force_y[i];

        const float ax = force_x / mass[mat];
        const float ay = force_y / mass[mat];

        rhs[4 * i + 0] = vx; // dx/dt = vx
        rhs[4 * i + 1] = vy; // dy/dt = vy
        rhs[4 * i + 2] = ax; // dvx/dt = ax
        rhs[4 * i + 3] = ay; // dvy/dt = ay
    }
}


ComputeRHSKernel::ComputeRHSKernel(const float* state_, const int* material_, const float* mass_, const int* grid_,
            const float* body_force_x_, const float* body_force_y_,
            const int n_, const int grid_size_, const int particles_per_cell_, float* rhs_)
        : state(state_), material(material_), mass(mass_), grid(grid_),
          body_force_x(body_force_x_), body_force_y(body_force_y_),
          Kernel(n_), grid_size(grid_size_), particles_per_cell(particles_per_cell_), rhs(rhs_) {
}


void ComputeRHSKernel::call_kernel(int blocks, int threads_per_block) {
    compute_rhs_kernel<<<blocks, threads_per_block>>>(state, material, mass, rhs, grid, body_force_x, body_force_y,
            n, grid_size, particles_per_cell);
}

