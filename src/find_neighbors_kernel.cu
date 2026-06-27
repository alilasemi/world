#include <assert.h>
#include "find_neighbors_kernel.h"
#include "cuda_check.h"


// Map a particle coordinate into a grid cell index, clamped to [0, grid_size).
// The domain need not be [-1, 1]: cells span [domain.*_min, domain.*_max].
__device__ inline int cell_index_x(float x, DomainParams domain, int grid_size) {
    return min(grid_size - 1, max(0, static_cast<int>(
            (x - domain.x_min) / (domain.x_max - domain.x_min) * grid_size)));
}
__device__ inline int cell_index_y(float y, DomainParams domain, int grid_size) {
    return min(grid_size - 1, max(0, static_cast<int>(
            (y - domain.y_min) / (domain.y_max - domain.y_min) * grid_size)));
}


// Pass 1: mark each cell that contains at least one particle.
// atomicCAS changes -1 → 1; if another thread already set it, the CAS is a
// no-op. No index assignment yet, so no spin-waiting is needed.
__global__ void mark_cells_kernel(
        int* spatial_grid, const float* state, int n, int grid_size, DomainParams domain) {
    int index = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;
    for (int i = index; i < n; i += stride) {
        int cell_x = cell_index_x(state[4*i+0], domain, grid_size);
        int cell_y = cell_index_y(state[4*i+1], domain, grid_size);
        atomicCAS(&spatial_grid[cell_x * grid_size + cell_y], -1, 1);
    }
}


// Pass 2: assign a compact occupied-cell index to every marked cell.
// Each thread owns exactly one cell, so the write to spatial_grid[c] is
// single-writer; atomicAdd only protects the shared counter.
__global__ void compact_cells_kernel(
        int* spatial_grid, int* num_occupied_cells, int num_cells) {
    int index = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;
    for (int c = index; c < num_cells; c += stride) {
        if (spatial_grid[c] == 1) {
            spatial_grid[c] = atomicAdd(num_occupied_cells, 1);
        }
    }
}


// Pass 3: each particle reserves a slot in its cell's compact row and writes
// its index there. occ_idx is guaranteed ≥ 0 after the compact pass.
__global__ void fill_cells_kernel(
        const int* spatial_grid, int* particles_in_cell, int* num_per_cell,
        const float* state, int n, int grid_size, int particles_per_cell, DomainParams domain) {
    int index = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;
    for (int i = index; i < n; i += stride) {
        int cell_x = cell_index_x(state[4*i+0], domain, grid_size);
        int cell_y = cell_index_y(state[4*i+1], domain, grid_size);
        int occ_idx = spatial_grid[cell_x * grid_size + cell_y];
        int slot = atomicAdd(&num_per_cell[occ_idx], 1);
        assert(slot < particles_per_cell);
        particles_in_cell[occ_idx * particles_per_cell + slot] = i;
    }
}


// Pass 4: walk each particle's 3x3 neighbor-cell stencil and write found
// neighbors (excluding self) into the flat device_neighbors array, terminated
// by a -1 sentinel. Output format is identical to the old cuco version so
// ComputeRHSKernel needs no changes.
__global__ void find_neighbors_kernel(
        const int* spatial_grid, const int* particles_in_cell, const int* num_per_cell,
        const float* state, int* neighbors, int n, int grid_size, int particles_per_cell,
        DomainParams domain) {
    int index = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;
    int row_stride = 9 * particles_per_cell;
    for (int i = index; i < n; i += stride) {
        int cell_x = cell_index_x(state[4*i+0], domain, grid_size);
        int cell_y = cell_index_y(state[4*i+1], domain, grid_size);
        int base = i * row_stride;
        int out = 0;
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                int nx = cell_x + dx;
                int ny = cell_y + dy;
                if (nx >= 0 && nx < grid_size && ny >= 0 && ny < grid_size) {
                    int occ_idx = spatial_grid[nx * grid_size + ny];
                    if (occ_idx >= 0) {
                        int count = num_per_cell[occ_idx];
                        for (int slot = 0; slot < count; ++slot) {
                            int j = particles_in_cell[occ_idx * particles_per_cell + slot];
                            if (j != i) {
                                neighbors[base + out] = j;
                                ++out;
                            }
                        }
                    }
                }
            }
        }
        neighbors[base + out] = -1;
    }
}


FindNeighborsKernel::FindNeighborsKernel(
        const float* state_, const int n_, const int grid_size_,
        const int particles_per_cell_, const DomainParams domain_,
        const int threads_per_block_, int* neighbors_, bool timing_enabled_)
        : Kernel(n_, threads_per_block_, timing_enabled_), state(state_), grid_size(grid_size_),
          particles_per_cell(particles_per_cell_), domain(domain_), neighbors(neighbors_),
          spatial_grid(static_cast<size_t>(grid_size_) * grid_size_),
          particles_in_cell(static_cast<size_t>(n_) * particles_per_cell_),
          num_per_cell(static_cast<size_t>(n_)),
          num_occupied_cells(1) {
    CUDA_CHECK(cudaMemset(spatial_grid.data(), -1,
            static_cast<size_t>(grid_size_) * grid_size_ * sizeof(int)));
    CUDA_CHECK(cudaMemset(num_per_cell.data(), 0,
            static_cast<size_t>(n_) * sizeof(int)));
    CUDA_CHECK(cudaMemset(num_occupied_cells.data(), 0, sizeof(int)));
}


void FindNeighborsKernel::call_kernel(int blocks, int threads_per_block) {
    // Reset per-frame state. cudaMemset 0xFF gives all-1-bits = -1 for int.
    CUDA_CHECK(cudaMemset(spatial_grid.data(), -1,
            static_cast<size_t>(grid_size) * grid_size * sizeof(int)));
    CUDA_CHECK(cudaMemset(num_per_cell.data(), 0,
            static_cast<size_t>(n) * sizeof(int)));
    CUDA_CHECK(cudaMemset(num_occupied_cells.data(), 0, sizeof(int)));

    mark_cells_kernel<<<blocks, threads_per_block>>>(
            spatial_grid.data(), state, n, grid_size, domain);

    int num_cells = grid_size * grid_size;
    int compact_blocks = (num_cells + threads_per_block - 1) / threads_per_block;
    compact_cells_kernel<<<compact_blocks, threads_per_block>>>(
            spatial_grid.data(), num_occupied_cells.data(), num_cells);

    fill_cells_kernel<<<blocks, threads_per_block>>>(
            spatial_grid.data(), particles_in_cell.data(), num_per_cell.data(),
            state, n, grid_size, particles_per_cell, domain);

    find_neighbors_kernel<<<blocks, threads_per_block>>>(
            spatial_grid.data(), particles_in_cell.data(), num_per_cell.data(),
            state, neighbors, n, grid_size, particles_per_cell, domain);
}
