#include "backward_euler_picard_kernel.h"


__global__ void backward_euler_picard_kernel(float* state, const float* state_n,
        const float* rhs, size_t n, float dt) {
    int index = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;
    for (int i = index; i < n; i += stride) {
        for (int a = 0; a < kDim; ++a) {
            // v^{n+1}_{k+1} = v^n + dt * a(x^{n+1}_k)
            const float v_new = state_n[kStateStride*i + kDim + a] + dt * rhs[kStateStride*i + kDim + a];
            state[kStateStride*i + kDim + a] = v_new;
            // x^{n+1}_{k+1} = x^n + dt * v^{n+1}_{k+1}
            state[kStateStride*i + a] = state_n[kStateStride*i + a] + dt * v_new;
        }
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
