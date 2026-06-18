#pragma once
#include <cuda_runtime.h>
#include "kernel.h"

class ComputeRHSKernel : public Kernel {
public:
    float wall_clock_time = 0.f;

    ComputeRHSKernel(const float* state_, const int* material_, const float* mass_, const int* grid_,
            const int n_, const int grid_size_, const int particles_per_cell_, float* rhs_);

private:
    const float* state;
    const int* material;
    const float* mass;
    const int* grid;
    const int grid_size;
    const int particles_per_cell;
    float* rhs;

    void call_kernel();
};
