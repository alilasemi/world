#include "take_step_kernel.h"


__global__ void take_step_kernel(float* state, const float* rhs, size_t n, float dt) {
    int index = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;
    for (int i = index; i < n; i += stride) {
        // Update velocity, then update position using the new velocity
        state[4*i + 2] += dt * rhs[4*i + 2];
        state[4*i + 3] += dt * rhs[4*i + 3];
        state[4*i + 0] += dt * state[4*i + 2];
        state[4*i + 1] += dt * state[4*i + 3];
    }
}


TakeStepKernel::TakeStepKernel(float* state_, const float* rhs_, const int n_, const float dt_,
        const int threads_per_block_)
        : Kernel(n_, threads_per_block_), state(state_), rhs(rhs_), dt(dt_) {
}


void TakeStepKernel::call_kernel(int blocks, int threads_per_block) {
    take_step_kernel<<<blocks, threads_per_block>>>(state, rhs, n, dt);
}
