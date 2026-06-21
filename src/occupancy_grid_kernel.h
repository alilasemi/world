#pragma once
#include <cuda_runtime.h>
#include "kernel.h"

class OccupancyGridKernel : public Kernel {
public:
    OccupancyGridKernel(int* occupancy_, const float* state_, const int n_, const int m_);

private:
    int* occupancy;
    const float* state;
    const int m;

    void call_kernel(int blocks, int threads_per_block) override;
};
