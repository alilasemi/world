#include "interpolate_force_kernel.h"


// Treats grid[] as one scalar sampled at the center of each of its m_x*m_y
// cells (row-major: grid[i*m_y + j]) and bilinearly blends the 4 nearest
// cell centers. m_y is also the row-stride, consistent with the row-major
// layout used for the collision/force/occupancy grids throughout.
__device__ float bilerp_cell(const float* grid, int i0, int i1, int j0, int j1, int m_y, float frac_u, float frac_v) {
    const float f00 = grid[i0 * m_y + j0];
    const float f10 = grid[i1 * m_y + j0];
    const float f01 = grid[i0 * m_y + j1];
    const float f11 = grid[i1 * m_y + j1];
    const float f0 = f00 * (1.0f - frac_u) + f10 * frac_u;
    const float f1 = f01 * (1.0f - frac_u) + f11 * frac_u;
    return f0 * (1.0f - frac_v) + f1 * frac_v;
}


__global__ void interpolate_force_kernel(const float* state, const float* grid_force_x, const float* grid_force_y,
        size_t n, int m_x, int m_y, DomainParams domain, float* force_x, float* force_y) {
    int index = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;
    const float cell_width_x = (domain.x_max - domain.x_min) / m_x;
    const float cell_width_y = (domain.y_max - domain.y_min) / m_y;
    for (int i = index; i < n; i += stride) {
        const float x = state[4 * i + 0];
        const float y = state[4 * i + 1];

        // Cell-center-spacing coordinates: u=0 sits exactly at cell 0's center.
        const float u = (x - domain.x_min) / cell_width_x - 0.5f;
        const float v = (y - domain.y_min) / cell_width_y - 0.5f;

        // floorf, not a truncating cast -- u/v can be negative near the
        // domain's left/bottom edge and must round toward -infinity.
        const int i0 = static_cast<int>(floorf(u));
        const int j0 = static_cast<int>(floorf(v));
        const float frac_u = u - static_cast<float>(i0);
        const float frac_v = v - static_cast<float>(j0);

        const int i0c = min(m_x - 1, max(0, i0));
        const int i1c = min(m_x - 1, max(0, i0 + 1));
        const int j0c = min(m_y - 1, max(0, j0));
        const int j1c = min(m_y - 1, max(0, j0 + 1));

        force_x[i] = bilerp_cell(grid_force_x, i0c, i1c, j0c, j1c, m_y, frac_u, frac_v);
        force_y[i] = bilerp_cell(grid_force_y, i0c, i1c, j0c, j1c, m_y, frac_u, frac_v);
    }
}


InterpolateForceKernel::InterpolateForceKernel(const float* state_, const float* grid_force_x_,
        const float* grid_force_y_, const int n_, const int m_x_, const int m_y_, const DomainParams domain_,
        const int threads_per_block_, float* force_x_, float* force_y_, bool timing_enabled_)
        : Kernel(n_, threads_per_block_, timing_enabled_), state(state_), grid_force_x(grid_force_x_),
          grid_force_y(grid_force_y_), m_x(m_x_), m_y(m_y_), domain(domain_),
          force_x(force_x_), force_y(force_y_) {
}


void InterpolateForceKernel::call_kernel(int blocks, int threads_per_block) {
    interpolate_force_kernel<<<blocks, threads_per_block>>>(state, grid_force_x, grid_force_y, n, m_x, m_y,
            domain, force_x, force_y);
}
