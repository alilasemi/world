#pragma once
#include <cuda_runtime.h>
#include "kernel.h"
#include "device_vector.h"
#include "sim_config.h"

// Fills two occupancy_grid_size_x*occupancy_grid_size_y grids with the
// mass-weighted average velocity of particles in each cell (0 for cells with
// no particles, or whose particles' total mass is 0 -- e.g. wall-only
// cells), same row-major layout and cell mapping as OccupancyGridKernel.
// This gives the RL observation velocity information the occupancy grid
// alone can't -- two states with the same positions but different
// velocities otherwise look identical to the policy, breaking the Markov
// assumption RL relies on.
class OccupancyVelocityKernel : public Kernel {
public:
    OccupancyVelocityKernel(float* velocity_x_, float* velocity_y_, const float* state_,
            const int* material_, const float* mass_, const int n_, const int m_x_, const int m_y_,
            const DomainParams domain_, const int threads_per_block_, bool timing_enabled_ = true);

private:
    float* velocity_x;
    float* velocity_y;
    const float* state;
    const int* material;
    const float* mass;
    const int m_x;
    const int m_y;
    const DomainParams domain;

    // Scratch accumulator for the total mass landed in each cell this call --
    // owned privately here (mirrors FindNeighborsKernel owning its collision
    // grid buffers), re-zeroed every call_kernel().
    DeviceVector<float> mass_sum;

    void call_kernel(int blocks, int threads_per_block) override;
};
