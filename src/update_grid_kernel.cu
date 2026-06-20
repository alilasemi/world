#include <stdio.h>
#include <assert.h>
#include "update_grid_kernel.h"
#include "cuda_check.h"


__global__ void zero_grid_counts_kernel(int* grid, const float* state, size_t n, int grid_size, int particles_per_cell) {
    int index = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;
    for (int i = index; i < n; i += stride) {
        int cell_x = min(grid_size - 1, max(0, static_cast<int>((state[4 * i + 0] + 1.0f) / 2.0f * grid_size)));
        int cell_y = min(grid_size - 1, max(0, static_cast<int>((state[4 * i + 1] + 1.0f) / 2.0f * grid_size)));
        // Zero the same 3x3 neighborhood that compute_rhs_kernel's stencil will
        // read, not just this particle's own cell -- otherwise a cell with no
        // particles of its own, but adjacent to one that has some, is read
        // uninitialized (cudaMalloc does not zero device memory). Still O(n)
        // overall since each particle only ever touches 9 cells.
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                int neighbor_cell_x = cell_x + dx;
                int neighbor_cell_y = cell_y + dy;
                if (neighbor_cell_x >= 0 && neighbor_cell_x < grid_size && neighbor_cell_y >= 0 && neighbor_cell_y < grid_size) {
                    grid[neighbor_cell_x * grid_size * particles_per_cell + neighbor_cell_y * particles_per_cell] = 0;
                }
            }
        }
    }
}


__global__ void update_grid_kernel(int* grid, const float* state, size_t n, int grid_size, int particles_per_cell) {
    int index = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;
    for (int i = index; i < n; i += stride) {
        int cell_x = min(grid_size - 1, max(0, static_cast<int>((state[4 * i + 0] + 1.0f) / 2.0f * grid_size)));
        int cell_y = min(grid_size - 1, max(0, static_cast<int>((state[4 * i + 1] + 1.0f) / 2.0f * grid_size)));
        int cell_index = cell_x * grid_size * particles_per_cell + cell_y * particles_per_cell;
        int slot = atomicAdd(&grid[cell_index], 1);
        if (slot < particles_per_cell - 1) {
            grid[cell_index + 1 + slot] = i; // Store particle index
        } else {
            // Handle overflow (too many particles in this cell)
            printf("Error: too many particles in cell (%d, %d)\n", cell_x, cell_y);
            assert(false);
        }
    }
}


UpdateGridKernel::UpdateGridKernel(int* grid_, const float* state_, const int n_,
        const int grid_size_, const int particles_per_cell_)
        : Kernel(n_), grid(grid_), state(state_), grid_size(grid_size_), particles_per_cell(particles_per_cell_) {
}


void UpdateGridKernel::call_kernel(int blocks, int threads_per_block) {
    // All cells touched by the bucketing pass must be zeroed first. A
    // grid-wide barrier is needed (not just __syncthreads()) since different
    // particles' cells may be touched by threads in different blocks, so this
    // requires two separate kernel launches with a sync in between.
    zero_grid_counts_kernel<<<blocks, threads_per_block>>>(grid, state, n, grid_size, particles_per_cell);
    CUDA_CHECK(cudaGetLastError());
    CUDA_CHECK(cudaDeviceSynchronize());

    update_grid_kernel<<<blocks, threads_per_block>>>(grid, state, n, grid_size, particles_per_cell);
}
