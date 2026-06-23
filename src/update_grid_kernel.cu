#include <stdio.h>
#include <assert.h>
#include <cuda/atomic>
#include "update_grid_kernel.h"
#include "cuda_check.h"
#include "grid_map.h"


// Replaces the old zero_grid_counts_kernel + update_grid_kernel two-launch
// pattern: with a hash map, an absent cell_index already means "0
// particles" (handled at lookup time in compute_rhs_kernel), so there's no
// uninitialized-memory hazard to zero ahead of time -- clear() in
// UpdateGridKernel::call_kernel is enough, and bucketing collapses to one
// launch.
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


UpdateGridKernel::UpdateGridKernel(GridMap* particles_in_cell_, GridMap* num_particles_in_cell_,
        const float* state_, const int n_, const int grid_size_, const int particles_per_cell_)
        : Kernel(n_), particles_in_cell(particles_in_cell_), num_particles_in_cell(num_particles_in_cell_),
          state(state_), grid_size(grid_size_), particles_per_cell(particles_per_cell_) {
}


void UpdateGridKernel::call_kernel(int blocks, int threads_per_block) {
    particles_in_cell->clear();
    num_particles_in_cell->clear();

    auto particles_in_cell_ref = particles_in_cell->ref(cuco::insert);
    auto num_particles_in_cell_ref = num_particles_in_cell->ref(cuco::insert_and_find);

    populate_grid_kernel<<<blocks, threads_per_block>>>(particles_in_cell_ref, num_particles_in_cell_ref,
            state, n, grid_size, particles_per_cell);
}
