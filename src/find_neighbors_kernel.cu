#include <stdio.h>
#include <assert.h>
#include <algorithm>
#include <cuda/atomic>
#include "find_neighbors_kernel.h"
#include "cuda_check.h"
#include "grid_map.h"


// Buckets particles into the two grid maps. Replaces the old
// zero_grid_counts_kernel + update_grid_kernel two-launch pattern: with a
// hash map, an absent cell_index already means "0 particles" (handled at
// lookup time below), so there's no uninitialized-memory hazard to zero
// ahead of time -- clear() in FindNeighborsKernel::call_kernel is enough.
template <typename ParticlesRef, typename CountRef>
__global__ void populate_grid_kernel(ParticlesRef particles_in_cell_ref, CountRef num_particles_in_cell_ref,
        const float* state, size_t n, int grid_size, int particles_per_cell) {
    int index = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;
    for (int i = index; i < n; i += stride) {
        int cell_x = min(grid_size - 1, max(0, static_cast<int>((state[4 * i + 0] + 1.0f) / 2.0f * grid_size)));
        int cell_y = min(grid_size - 1, max(0, static_cast<int>((state[4 * i + 1] + 1.0f) / 2.0f * grid_size)));
        int cell_index = cell_x * grid_size + cell_y;

        // Get-or-create the count entry for this cell (starting at 0 if this
        // is the first particle to land in it this step), then atomically
        // increment it to reserve this particle's slot.
        auto insert_result = num_particles_in_cell_ref.insert_and_find(cuco::pair<int, int>{cell_index, 0});
        auto count_iter = insert_result.first;
        cuda::atomic_ref<int, cuda::thread_scope_device> count_ref{count_iter->second};
        int slot = count_ref.fetch_add(1, cuda::memory_order_relaxed);

        if (slot < particles_per_cell - 1) {
            int key = cell_index * particles_per_cell + slot;
            particles_in_cell_ref.insert(cuco::pair<int, int>{key, i});
        } else {
            // Handle overflow (too many particles in this cell)
            printf("Error: too many particles in cell (%d, %d)\n", cell_x, cell_y);
            assert(false);
        }
    }
}


// Re-walks the same 3x3 neighbor-cell stencil against the now-populated grid
// maps and writes each particle's found neighbors (excluding itself) into
// its row of the flat `neighbors` array, terminated by a -1 sentinel. This
// is the main output of this kernel now -- ComputeRHSKernel only ever reads
// this flat array, never the maps themselves, which is what lets it (and
// ParticleDynamicsCUDA) stay free of cuco entirely.
template <typename ParticlesRef, typename CountRef>
__global__ void find_neighbors_kernel(ParticlesRef particles_in_cell_ref, CountRef num_particles_in_cell_ref,
        const float* state, int* neighbors, size_t n, int grid_size, int particles_per_cell) {
    int index = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;
    int row_stride = 9 * particles_per_cell;
    for (int i = index; i < n; i += stride) {
        int cell_x = min(grid_size - 1, max(0, static_cast<int>((state[4 * i + 0] + 1.0f) / 2.0f * grid_size)));
        int cell_y = min(grid_size - 1, max(0, static_cast<int>((state[4 * i + 1] + 1.0f) / 2.0f * grid_size)));
        int base = i * row_stride;
        int out = 0;
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                int neighbor_cell_x = cell_x + dx;
                int neighbor_cell_y = cell_y + dy;
                if (neighbor_cell_x >= 0 && neighbor_cell_x < grid_size && neighbor_cell_y >= 0 && neighbor_cell_y < grid_size) {
                    int cell_index = neighbor_cell_x * grid_size + neighbor_cell_y;
                    auto count_iter = num_particles_in_cell_ref.find(cell_index);
                    int count = (count_iter == num_particles_in_cell_ref.end()) ? 0 : count_iter->second;
                    for (int slot = 0; slot < count; ++slot) {
                        auto particle_iter = particles_in_cell_ref.find(cell_index * particles_per_cell + slot);
                        // Should always be found: count came from the same
                        // populate pass that wrote exactly `count` entries
                        // for this cell_index.
                        assert(particle_iter != particles_in_cell_ref.end());
                        int j = particle_iter->second;
                        if (i != j) {
                            neighbors[base + out] = j;
                            ++out;
                        }
                    }
                }
            }
        }
        // Sentinel: out is always < row_stride (9*particles_per_cell), since
        // each of the 9 cells contributes at most particles_per_cell entries
        // and this cell's own entry for particle i is excluded above.
        neighbors[base + out] = -1;
    }
}


FindNeighborsKernel::FindNeighborsKernel(const float* state_, const int n_, const int grid_size_,
        const int particles_per_cell_, int* neighbors_)
        : Kernel(n_), state(state_), grid_size(grid_size_), particles_per_cell(particles_per_cell_),
          neighbors(neighbors_) {
    // Capacity bounds the *number of entries*, not the key's numeric range --
    // GridMap hashes keys into buckets rather than indexing by key value
    // directly. particles_in_cell's composite keys range up to
    // grid_size*grid_size*particles_per_cell, but only ever holds n_ live
    // entries (one per particle) at a time, so its capacity is sized off
    // n_, not that key range.
    size_t num_particles_in_cell_capacity = static_cast<size_t>(2 * std::min(n_, grid_size_ * grid_size_)); // bounded by distinct occupied cells
    size_t particles_in_cell_capacity = static_cast<size_t>(2 * n_); // exactly one live entry per particle
    num_particles_in_cell = std::make_unique<GridMap>(num_particles_in_cell_capacity,
            cuco::empty_key<int>{kEmptyKeySentinel}, cuco::empty_value<int>{kEmptyValueSentinel},
            cuda::std::equal_to<int>{}, cuco::linear_probing<1, cuco::default_hash_function<int>>{});
    particles_in_cell = std::make_unique<GridMap>(particles_in_cell_capacity,
            cuco::empty_key<int>{kEmptyKeySentinel}, cuco::empty_value<int>{kEmptyValueSentinel},
            cuda::std::equal_to<int>{}, cuco::linear_probing<1, cuco::default_hash_function<int>>{});
}


FindNeighborsKernel::~FindNeighborsKernel() = default;


void FindNeighborsKernel::call_kernel(int blocks, int threads_per_block) {
    particles_in_cell->clear();
    num_particles_in_cell->clear();

    auto particles_in_cell_insert_ref = particles_in_cell->ref(cuco::insert);
    auto num_particles_in_cell_insert_ref = num_particles_in_cell->ref(cuco::insert_and_find);
    populate_grid_kernel<<<blocks, threads_per_block>>>(particles_in_cell_insert_ref, num_particles_in_cell_insert_ref,
            state, n, grid_size, particles_per_cell);

    auto particles_in_cell_find_ref = particles_in_cell->ref(cuco::find);
    auto num_particles_in_cell_find_ref = num_particles_in_cell->ref(cuco::find);
    find_neighbors_kernel<<<blocks, threads_per_block>>>(particles_in_cell_find_ref, num_particles_in_cell_find_ref,
            state, neighbors, n, grid_size, particles_per_cell);
}
