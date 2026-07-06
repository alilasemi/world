#include "occupancy_grid_kernel.h"
#include "cuda_check.h"


__global__ void occupancy_grid_kernel(int* occupancy, const float* state, size_t n, int m_x, int m_y,
        DomainParams domain) {
    int index = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;
    for (int i = index; i < n; i += stride) {
        const int cell_x = min(m_x - 1, max(0, static_cast<int>(
                (state[4 * i + 0] - domain.x_min) / (domain.x_max - domain.x_min) * m_x)));
        const int cell_y = min(m_y - 1, max(0, static_cast<int>(
                (state[4 * i + 1] - domain.y_min) / (domain.y_max - domain.y_min) * m_y)));
        // Multiple particles can land in the same cell; atomicExch (rather
        // than a plain store) avoids a same-value write race on a technicality.
        atomicExch(&occupancy[cell_x * m_y + cell_y], 1);
    }
}


OccupancyGridKernel::OccupancyGridKernel(int* occupancy_, const float* state_, const int n_,
        const int m_x_, const int m_y_, const DomainParams domain_, const int threads_per_block_,
        bool timing_enabled_)
        : Kernel(n_, threads_per_block_, timing_enabled_), occupancy(occupancy_), state(state_),
          m_x(m_x_), m_y(m_y_), domain(domain_) {
}


void OccupancyGridKernel::call_kernel(int blocks, int threads_per_block) {
    CUDA_CHECK(cudaMemset(occupancy, 0, static_cast<size_t>(m_x) * static_cast<size_t>(m_y) * sizeof(int)));
    occupancy_grid_kernel<<<blocks, threads_per_block>>>(occupancy, state, n, m_x, m_y, domain);
}
