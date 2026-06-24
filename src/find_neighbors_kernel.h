#pragma once
#include <cuda_runtime.h>
#include <memory>
#include "grid_map_fwd.h"
#include "kernel.h"

class FindNeighborsKernel : public Kernel {
public:
    FindNeighborsKernel(const float* state_, const int n_, const int grid_size_,
            const int particles_per_cell_, int* neighbors_);
    // Declared here, defined out-of-line in find_neighbors_kernel.cu (NOT
    // = default inline) -- unique_ptr<GridMap>'s destructor needs GridMap
    // complete, which this header (grid_map_fwd.h's forward declaration
    // only) doesn't have. The .cu does, since it includes grid_map.h.
    ~FindNeighborsKernel();

private:
    const float* state;
    const int grid_size;
    const int particles_per_cell;
    int* neighbors;

    std::unique_ptr<GridMap> particles_in_cell;
    std::unique_ptr<GridMap> num_particles_in_cell;

    void call_kernel(int blocks, int threads_per_block) override;
};
