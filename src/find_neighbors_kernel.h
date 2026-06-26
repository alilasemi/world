#pragma once
#include "device_vector.h"
#include "kernel.h"

class FindNeighborsKernel : public Kernel {
public:
    FindNeighborsKernel(const float* state_, int n_, int grid_size_,
            int particles_per_cell_, int* neighbors_);
    ~FindNeighborsKernel() override = default;

private:
    const float* state;
    const int grid_size;
    const int particles_per_cell;
    int* neighbors;

    // Dense spatial grid: spatial_grid[cell_x*grid_size + cell_y] holds the
    // compact occupied-cell index into particles_in_cell (or -1 if empty).
    // particles_in_cell[occ_idx * k + slot] holds particle indices;
    // num_per_cell[occ_idx] is the count for that occupied cell.
    // num_occupied_cells is a single device int used as an atomic counter
    // to assign new occ_idx values in the compact pass.
    DeviceVector<int> spatial_grid;          // m*m
    DeviceVector<int> particles_in_cell;     // n*k
    DeviceVector<int> num_per_cell;          // n (max n occupied cells)
    DeviceVector<int> num_occupied_cells; // 1

    void call_kernel(int blocks, int threads_per_block) override;
};
