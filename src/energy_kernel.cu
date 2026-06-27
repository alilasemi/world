#include "energy_kernel.h"
#include "cuda_check.h"


__global__ void energy_kernel(const float* state, const int* material, const float* mass,
        size_t n, float gravity, float floor_y, float* energy) {
    int index = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;
    for (int i = index; i < n; i += stride) {
        const float vx = state[4 * i + 2];
        const float vy = state[4 * i + 3];
        const float y = state[4 * i + 1];
        const float m = mass[material[i]];
        const float h = y - floor_y;
        atomicAdd(energy, 0.5f * m * (vx * vx + vy * vy) + m * gravity * h);
    }
}


EnergyKernel::EnergyKernel(const float* state_, const int* material_, const float* mass_,
        const int n_, const PhysicsParams physics_, const int threads_per_block_, float* energy_,
        bool timing_enabled_)
        : Kernel(n_, threads_per_block_, timing_enabled_), state(state_), material(material_), mass(mass_),
          physics(physics_), energy(energy_) {
}


void EnergyKernel::call_kernel(int blocks, int threads_per_block) {
    // Reset the accumulator before each launch -- we want a fresh total every
    // call, not a running sum across calls.
    CUDA_CHECK(cudaMemset(energy, 0, sizeof(float)));
    energy_kernel<<<blocks, threads_per_block>>>(state, material, mass, n,
            physics.gravity, physics.floor_y, energy);
}
