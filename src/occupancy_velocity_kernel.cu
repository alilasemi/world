#include "occupancy_velocity_kernel.h"
#include "cuda_check.h"


__global__ void accumulate_velocity_kernel(float* velocity_x, float* velocity_y, float* mass_sum,
        const float* state, const int* material, const float* mass, size_t n, int m_x, int m_y,
        DomainParams domain) {
    int index = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;
    for (int i = index; i < n; i += stride) {
        const int cell_x = min(m_x - 1, max(0, static_cast<int>(
                (state[4 * i + 0] - domain.x_min) / (domain.x_max - domain.x_min) * m_x)));
        const int cell_y = min(m_y - 1, max(0, static_cast<int>(
                (state[4 * i + 1] - domain.y_min) / (domain.y_max - domain.y_min) * m_y)));
        const int cell = cell_x * m_y + cell_y;
        const float m = mass[material[i]];
        // velocity_x/velocity_y temporarily hold the numerator (sum of m*v)
        // here; normalize_velocity_kernel divides by mass_sum afterward.
        atomicAdd(&velocity_x[cell], m * state[4 * i + 2]);
        atomicAdd(&velocity_y[cell], m * state[4 * i + 3]);
        atomicAdd(&mass_sum[cell], m);
    }
}


__global__ void normalize_velocity_kernel(float* velocity_x, float* velocity_y, const float* mass_sum,
        size_t num_cells) {
    int index = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;
    for (int c = index; c < num_cells; c += stride) {
        const float m = mass_sum[c];
        // Guards both "no particles landed here" and "only zero-mass (wall)
        // particles landed here" -- both should read 0, not NaN/Inf.
        if (m > 1e-8f) {
            velocity_x[c] /= m;
            velocity_y[c] /= m;
        } else {
            velocity_x[c] = 0.f;
            velocity_y[c] = 0.f;
        }
    }
}


OccupancyVelocityKernel::OccupancyVelocityKernel(float* velocity_x_, float* velocity_y_,
        const float* state_, const int* material_, const float* mass_, const int n_,
        const int m_x_, const int m_y_, const DomainParams domain_, const int threads_per_block_,
        bool timing_enabled_)
        : Kernel(n_, threads_per_block_, timing_enabled_), velocity_x(velocity_x_),
          velocity_y(velocity_y_), state(state_), material(material_), mass(mass_),
          m_x(m_x_), m_y(m_y_), domain(domain_),
          mass_sum(static_cast<size_t>(m_x_) * static_cast<size_t>(m_y_)) {
}


void OccupancyVelocityKernel::call_kernel(int blocks, int threads_per_block) {
    const size_t num_cells = static_cast<size_t>(m_x) * static_cast<size_t>(m_y);
    CUDA_CHECK(cudaMemset(velocity_x, 0, num_cells * sizeof(float)));
    CUDA_CHECK(cudaMemset(velocity_y, 0, num_cells * sizeof(float)));
    CUDA_CHECK(cudaMemset(mass_sum.data(), 0, num_cells * sizeof(float)));

    accumulate_velocity_kernel<<<blocks, threads_per_block>>>(
            velocity_x, velocity_y, mass_sum.data(), state, material, mass, n, m_x, m_y, domain);

    const int cell_blocks = (static_cast<int>(num_cells) + threads_per_block - 1) / threads_per_block;
    normalize_velocity_kernel<<<cell_blocks, threads_per_block>>>(
            velocity_x, velocity_y, mass_sum.data(), num_cells);
}
