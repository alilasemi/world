#pragma once
#include <cuda_runtime.h>
#include "kernel.h"

class UpdateGridKernel : public Kernel {
public:
    UpdateGridKernel(int* grid_, const float* state_, const int n_,
            const int grid_size_, const int particles_per_cell_);

private:
    int* grid;
    const float* state;
    const int grid_size;
    const int particles_per_cell;

    void call_kernel(int blocks, int threads_per_block) override;
};
