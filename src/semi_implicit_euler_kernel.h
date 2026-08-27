#pragma once
#include <cuda_runtime.h>
#include "kernel.h"
#include "sim_config.h"  // kDim / kStateStride: the state layout these integrators walk

class SemiImplicitEulerKernel : public Kernel {
public:
    SemiImplicitEulerKernel(float* state_, const float* rhs_, const int n_, const float dt_,
            const int threads_per_block_, bool timing_enabled_ = true);

    void set_dt(const float dt_) { dt = dt_; }

private:
    float* state;
    const float* rhs;
    float dt;

    void call_kernel(int blocks, int threads_per_block) override;
};
