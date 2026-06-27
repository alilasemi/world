#pragma once
#include <cuda_runtime.h>
#include "kernel.h"
#include "sim_config.h"

class InterpolateForceKernel : public Kernel {
public:
    InterpolateForceKernel(const float* state_, const float* grid_force_x_, const float* grid_force_y_,
            const int n_, const int m_, const DomainParams domain_, const int threads_per_block_,
            float* force_x_, float* force_y_);

private:
    const float* state;
    const float* grid_force_x;
    const float* grid_force_y;
    const int m;
    const DomainParams domain;
    float* force_x;
    float* force_y;

    void call_kernel(int blocks, int threads_per_block) override;
};
