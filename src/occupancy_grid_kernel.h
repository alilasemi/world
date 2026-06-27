#pragma once
#include <cuda_runtime.h>
#include "kernel.h"
#include "sim_config.h"

class OccupancyGridKernel : public Kernel {
public:
    OccupancyGridKernel(int* occupancy_, const float* state_, const int n_, const int m_,
            const DomainParams domain_, const int threads_per_block_);

private:
    int* occupancy;
    const float* state;
    const int m;
    const DomainParams domain;

    void call_kernel(int blocks, int threads_per_block) override;
};
