#include "energy_kernel.h"
#include "cuda_check.h"


__global__ void energy_kernel(const float* state, const int* material, const float* mass, size_t n, float* energy) {
    int index = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;
    for (int i = index; i < n; i += stride) {
        const float vx = state[4 * i + 2];
        const float vy = state[4 * i + 3];
        const float y = state[4 * i + 1];
        const float m = mass[material[i]];
        const float g = 9.81f;        // matches compute_rhs_kernel.cu
        const float floor_y = -1.0f;  // matches compute_rhs_kernel.cu
        const float h = y - floor_y;
        atomicAdd(energy, 0.5f * m * (vx * vx + vy * vy) + m * g * h);
    }
}


EnergyKernel::EnergyKernel(const float* state_, const int* material_, const float* mass_,
        const int n_, float* energy_)
        : Kernel(n_), state(state_), material(material_), mass(mass_), energy(energy_) {
}


void EnergyKernel::call_kernel(int blocks, int threads_per_block) {
    // Reset the accumulator before each launch -- we want a fresh total every
    // call, not a running sum across calls.
    CUDA_CHECK(cudaMemset(energy, 0, sizeof(float)));
    energy_kernel<<<blocks, threads_per_block>>>(state, material, mass, n, energy);
}
