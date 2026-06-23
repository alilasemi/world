#pragma once
#include <cuda_runtime.h>
#include "grid_map_fwd.h"
#include "kernel.h"

class UpdateGridKernel : public Kernel {
public:
    UpdateGridKernel(GridMap* particles_in_cell_, GridMap* num_particles_in_cell_, const float* state_,
            const int n_, const int grid_size_, const int particles_per_cell_);

private:
    GridMap* particles_in_cell;
    GridMap* num_particles_in_cell;
    const float* state;
    const int grid_size;
    const int particles_per_cell;

    void call_kernel(int blocks, int threads_per_block) override;
};
