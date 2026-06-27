#pragma once
#include <cuda_runtime.h>
#include "kernel.h"

// One Picard update for backward Euler: x^{n+1}_{k+1} = x^n + dt * f(x^{n+1}_k).
// Call this kernel once per Picard iteration; the caller is responsible for
// saving x^n into state_n before the iteration loop and for re-running
// FindNeighbors / InterpolateForce / ComputeRHS on each iteration so that rhs
// reflects f evaluated at the current iterate in device_state.
class BackwardEulerPicardKernel : public Kernel {
public:
    BackwardEulerPicardKernel(float* state_, const float* state_n_, const float* rhs_,
            const int n_, const float dt_, const int threads_per_block_,
            bool timing_enabled_ = true);

    void set_dt(const float dt_) { dt = dt_; }

private:
    float* state;
    const float* state_n;
    const float* rhs;
    float dt;

    void call_kernel(int blocks, int threads_per_block) override;
};
