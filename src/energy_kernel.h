#pragma once
#include <cuda_runtime.h>
#include "kernel.h"
#include "sim_config.h"

class EnergyKernel : public Kernel {
public:
    EnergyKernel(const float* state_, const int* material_, const float* mass_,
            const int* neighbors_, const int n_, const int particles_per_cell_,
            const PhysicsParams physics_, const int threads_per_block_, float* energy_,
            bool timing_enabled_ = true);

private:
    const float* state;
    const int* material;
    const float* mass;
    // Needed for the particle-particle contact energy. Read-only here; owned
    // and populated by ParticleDynamics/FindNeighborsKernel.
    const int* neighbors;
    const int particles_per_cell;
    const PhysicsParams physics;
    float* energy;

    void call_kernel(int blocks, int threads_per_block) override;
};
