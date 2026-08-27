#include "density_grid_kernel.h"
#include "cuda_check.h"


// Trilinear ("cloud-in-cell") deposit weights for one axis.
//
// This is exactly the particle-to-grid transfer MPM uses, and it is chosen over
// nearest-node binning deliberately: binning makes the field jump
// discontinuously the instant a grain crosses a cell boundary, and that
// discontinuity would land in the surrogate's regression target as noise the
// model cannot explain. Trilinear deposit is C0 in particle position, so the
// field varies smoothly as grains move, which conditions both the POD basis and
// the GP fit far better.
//
// The grid is NODE-centered: `size` nodes span [lo, hi] inclusive, so spacing is
// (hi - lo)/(size - 1) and a particle exactly on a boundary deposits entirely
// onto the boundary node.
//
// Out-of-range indices are CLAMPED rather than dropped, which keeps the weights
// summing to exactly 1 and therefore makes mass deposition exactly conservative
// (see the sum-of-cells test). The cost is a slight pile-up on the boundary
// nodes for anything outside the domain -- acceptable here, since the walls
// confine the grains and a grain touching a wall genuinely belongs at the
// boundary.
__device__ inline void axis_weights(float p, float lo, float hi, int size,
        int& i0, int& i1, float& w0, float& w1) {
    if (size <= 1) {
        i0 = 0; i1 = 0; w0 = 1.f; w1 = 0.f;
        return;
    }
    const float h = (hi - lo) / static_cast<float>(size - 1);
    const float u = (p - lo) / h;
    float base = floorf(u);
    const float frac = u - base;
    int b = static_cast<int>(base);
    i0 = min(size - 1, max(0, b));
    i1 = min(size - 1, max(0, b + 1));
    w0 = 1.f - frac;
    w1 = frac;
}


__global__ void density_grid_kernel(float* grid, const float* state, const int* material,
        const float* mass, size_t n, int size_x, int size_y, int size_z, DomainParams domain) {
    int index = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;
    const int num_nodes = size_x * size_y * size_z;

    for (int i = index; i < n; i += stride) {
        const float m = mass[material[i]];
        // Zero-mass materials (the wall id) contribute nothing to either
        // channel, so skip the eight atomics entirely.
        if (m == 0.f) continue;

        int ix[2], iy[2], iz[2];
        float wx[2], wy[2], wz[2];
        axis_weights(state[kStateStride * i + 0], domain.x_min, domain.x_max, size_x,
                ix[0], ix[1], wx[0], wx[1]);
        axis_weights(state[kStateStride * i + 1], domain.y_min, domain.y_max, size_y,
                iy[0], iy[1], wy[0], wy[1]);
        axis_weights(state[kStateStride * i + 2], domain.z_min, domain.z_max, size_z,
                iz[0], iz[1], wz[0], wz[1]);

        float momentum[kDim];
        for (int a = 0; a < kDim; ++a) {
            momentum[a] = m * state[kStateStride * i + kDim + a];
        }

        for (int cx = 0; cx < 2; ++cx) {
            for (int cy = 0; cy < 2; ++cy) {
                for (int cz = 0; cz < 2; ++cz) {
                    const float w = wx[cx] * wy[cy] * wz[cz];
                    if (w == 0.f) continue;
                    const int node = (ix[cx] * size_y + iy[cy]) * size_z + iz[cz];
                    atomicAdd(&grid[node], m * w);
                    for (int a = 0; a < kDim; ++a) {
                        atomicAdd(&grid[(a + 1) * num_nodes + node], momentum[a] * w);
                    }
                }
            }
        }
    }
}


DensityGridKernel::DensityGridKernel(float* grid_, const float* state_, const int* material_,
        const float* mass_, const int n_, const int size_x_, const int size_y_, const int size_z_,
        const DomainParams domain_, const int threads_per_block_, bool timing_enabled_)
        : Kernel(n_, threads_per_block_, timing_enabled_), grid(grid_), state(state_),
          material(material_), mass(mass_), size_x(size_x_), size_y(size_y_), size_z(size_z_),
          domain(domain_) {
}


size_t DensityGridKernel::grid_size() const {
    return static_cast<size_t>(kChannels) * static_cast<size_t>(size_x)
            * static_cast<size_t>(size_y) * static_cast<size_t>(size_z);
}


void DensityGridKernel::call_kernel(int blocks, int threads_per_block) {
    // Fresh field every call, not a running sum across calls.
    CUDA_CHECK(cudaMemset(grid, 0, grid_size() * sizeof(float)));
    density_grid_kernel<<<blocks, threads_per_block>>>(grid, state, material, mass, n,
            size_x, size_y, size_z, domain);
}
