#pragma once
#include <cuda_runtime.h>
#include "kernel.h"
#include "sim_config.h"

// Deposits particle mass and momentum onto a coarse regular grid -- the
// "latent" the outcome surrogate is trained on.
//
// Four channels are produced in one pass (the trilinear weights are the
// expensive part, so computing them once for all four is strictly better than
// separate kernels): mass, then the three components of momentum (mass *
// velocity). The momentum channels are not decoration: a density field alone
// is not a Markov state -- two piles with identical shapes but different
// velocity fields evolve differently -- so a latent dynamics model needs them.
// (This is the same defect that was diagnosed in the removed RL branch, whose
// position-only observation space was non-Markov.)
//
// Layout is CHANNEL-MAJOR: channel c occupies
// [c*num_nodes, (c+1)*num_nodes), and within a channel the node index is
// (ix*size_y + iy)*size_z + iz. Channel-major keeps each field contiguous,
// which is what downstream POD/PCA wants when it slices a single channel.
class DensityGridKernel : public Kernel {
public:
    static constexpr int kChannels = 1 + kDim;  // mass, then momentum per axis

    DensityGridKernel(float* grid_, const float* state_, const int* material_,
            const float* mass_, int n_, int size_x_, int size_y_, int size_z_,
            const DomainParams domain_, int threads_per_block_, bool timing_enabled_ = true);

    // Total float count of the grid: kChannels * size_x * size_y * size_z.
    size_t grid_size() const;

private:
    float* grid;
    const float* state;
    const int* material;
    const float* mass;
    const int size_x;
    const int size_y;
    const int size_z;
    const DomainParams domain;

    void call_kernel(int blocks, int threads_per_block) override;
};
