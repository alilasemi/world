#include "semi_implicit_euler_kernel.h"


__global__ void semi_implicit_euler_kernel(float* state, const float* rhs, size_t n, float dt) {
    int index = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;
    for (int i = index; i < n; i += stride) {
        // Update velocity, then update position using the new velocity
        for (int a = 0; a < kDim; ++a) {
            state[kStateStride*i + kDim + a] += dt * rhs[kStateStride*i + kDim + a];
        }
        for (int a = 0; a < kDim; ++a) {
            state[kStateStride*i + a] += dt * state[kStateStride*i + kDim + a];
        }
    }
}


SemiImplicitEulerKernel::SemiImplicitEulerKernel(float* state_, const float* rhs_, const int n_,
        const float dt_, const int threads_per_block_, bool timing_enabled_)
        : Kernel(n_, threads_per_block_, timing_enabled_), state(state_), rhs(rhs_), dt(dt_) {
}


void SemiImplicitEulerKernel::call_kernel(int blocks, int threads_per_block) {
    semi_implicit_euler_kernel<<<blocks, threads_per_block>>>(state, rhs, n, dt);
}
