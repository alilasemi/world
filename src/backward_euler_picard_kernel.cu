#include "backward_euler_picard_kernel.h"


__global__ void backward_euler_picard_kernel(float* state, const float* state_n,
        const float* rhs, size_t n, float dt) {
    int index = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;
    for (int i = index; i < n; i += stride) {
        // v^{n+1}_{k+1} = v^n + dt * a(x^{n+1}_k)
        const float vx_new = state_n[4*i + 2] + dt * rhs[4*i + 2];
        const float vy_new = state_n[4*i + 3] + dt * rhs[4*i + 3];
        state[4*i + 2] = vx_new;
        state[4*i + 3] = vy_new;
        // x^{n+1}_{k+1} = x^n + dt * v^{n+1}_{k+1}
        state[4*i + 0] = state_n[4*i + 0] + dt * vx_new;
        state[4*i + 1] = state_n[4*i + 1] + dt * vy_new;
    }
}


BackwardEulerPicardKernel::BackwardEulerPicardKernel(float* state_, const float* state_n_,
        const float* rhs_, const int n_, const float dt_, const int threads_per_block_,
        bool timing_enabled_)
        : Kernel(n_, threads_per_block_, timing_enabled_), state(state_), state_n(state_n_),
          rhs(rhs_), dt(dt_) {
}


void BackwardEulerPicardKernel::call_kernel(int blocks, int threads_per_block) {
    backward_euler_picard_kernel<<<blocks, threads_per_block>>>(state, state_n, rhs, n, dt);
}
