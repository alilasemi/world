#pragma once
#include <cuda_runtime.h>
#include "kernel.h"

class EnergyKernel : public Kernel {
public:
    EnergyKernel(const float* state_, const int* material_, const float* mass_,
            const int n_, float* energy_);

private:
    const float* state;
    const int* material;
    const float* mass;
    float* energy;

    void call_kernel(int blocks, int threads_per_block) override;
};
