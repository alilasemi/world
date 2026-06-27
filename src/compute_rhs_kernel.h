#pragma once
#include <cuda_runtime.h>
#include "kernel.h"
#include "sim_config.h"

class ComputeRHSKernel : public Kernel {
public:
    ComputeRHSKernel(const float* state_, const int* material_, const float* mass_,
            const int* neighbors_, const float* body_force_x_, const float* body_force_y_,
            const int n_, const int particles_per_cell_, const PhysicsParams physics_,
            const int threads_per_block_, float* rhs_, bool timing_enabled_ = true);

private:
    const float* state;
    const int* material;
    const float* mass;
    const int* neighbors;
    const float* body_force_x;
    const float* body_force_y;
    const int particles_per_cell;
    const PhysicsParams physics;
    float* rhs;

    void call_kernel(int blocks, int threads_per_block) override;
};
