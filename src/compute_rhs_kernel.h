#pragma once
#include <cuda_runtime.h>
#include "grid_map_fwd.h"
#include "kernel.h"

class ComputeRHSKernel : public Kernel {
public:
    ComputeRHSKernel(const float* state_, const int* material_, const float* mass_,
            GridMap* particles_in_cell_, GridMap* num_particles_in_cell_,
            const float* body_force_x_, const float* body_force_y_,
            const int n_, const int grid_size_, const int particles_per_cell_, float* rhs_);

private:
    const float* state;
    const int* material;
    const float* mass;
    GridMap* particles_in_cell;
    GridMap* num_particles_in_cell;
    const float* body_force_x;
    const float* body_force_y;
    const int grid_size;
    const int particles_per_cell;
    float* rhs;

    void call_kernel(int blocks, int threads_per_block) override;
};
