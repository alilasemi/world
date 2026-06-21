#include "occupancy_grid_kernel.h"
#include "cuda_check.h"


__global__ void occupancy_grid_kernel(int* occupancy, const float* state, size_t n, int m) {
    int index = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;
    for (int i = index; i < n; i += stride) {
        const int cell_x = min(m - 1, max(0, static_cast<int>((state[4 * i + 0] + 1.0f) / 2.0f * m)));
        const int cell_y = min(m - 1, max(0, static_cast<int>((state[4 * i + 1] + 1.0f) / 2.0f * m)));
        // Multiple particles can land in the same cell; atomicExch (rather
        // than a plain store) avoids a same-value write race on a technicality.
        atomicExch(&occupancy[cell_x * m + cell_y], 1);
    }
}


OccupancyGridKernel::OccupancyGridKernel(int* occupancy_, const float* state_, const int n_, const int m_)
        : Kernel(n_), occupancy(occupancy_), state(state_), m(m_) {
}


void OccupancyGridKernel::call_kernel(int blocks, int threads_per_block) {
    CUDA_CHECK(cudaMemset(occupancy, 0, static_cast<size_t>(m) * static_cast<size_t>(m) * sizeof(int)));
    occupancy_grid_kernel<<<blocks, threads_per_block>>>(occupancy, state, n, m);
}
