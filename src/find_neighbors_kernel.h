#pragma once
#include "device_vector.h"
#include "kernel.h"
#include "sim_config.h"

class FindNeighborsKernel : public Kernel {
public:
    FindNeighborsKernel(const float* state_, int n_, int grid_size_x_, int grid_size_y_,
            int grid_size_z_, int particles_per_cell_, const DomainParams domain_,
            int threads_per_block_, int* neighbors_, bool timing_enabled_ = true);
    ~FindNeighborsKernel() override = default;

    // Lifetime-cumulative count of (cell, step)-overflow incidents -- i.e.
    // how many times, across every step so far, a cell's particle count
    // exceeded particles_per_cell (see fill_cells_kernel). Never reset, and
    // deliberately not checked automatically every step (that would force a
    // host sync per step); callers that want to detect a diverged
    // simulation without a device-assert flood should poll this
    // occasionally instead.
    int overflow_count() const;

private:
    const float* state;
    const int grid_size_x;
    const int grid_size_y;
    const int grid_size_z;
    const int particles_per_cell;
    const DomainParams domain;
    int* neighbors;

    // Dense collision grid, used for neighbor lookup:
    // collision_grid[(cell_x*grid_size_y + cell_y)*grid_size_z + cell_z]
    // holds the compact occupied-cell index into particles_in_cell
    // (or -1 if empty). particles_in_cell[occ_idx * k + slot] holds particle
    // indices; num_per_cell[occ_idx] is the count for that occupied cell.
    // num_occupied_cells is a single device int used as an atomic counter
    // to assign new occ_idx values in the compact pass.
    DeviceVector<int> collision_grid;        // grid_size_x*grid_size_y*grid_size_z
    DeviceVector<int> particles_in_cell;     // n*k
    DeviceVector<int> num_per_cell;          // n (max n occupied cells)
    DeviceVector<int> num_occupied_cells; // 1

    // Overflow tracking for the fill pass: cell_overflowed[occ_idx] is set
    // once a cell's particle count exceeds particles_per_cell (further
    // particles for that cell are dropped rather than written out of
    // bounds); reset every frame. num_overflowed_cells counts overflow
    // incidents and, unlike everything else here, is NOT reset per frame --
    // see overflow_count().
    DeviceVector<int> cell_overflowed;    // n (max n occupied cells)
    DeviceVector<int> num_overflowed_cells; // 1

    void call_kernel(int blocks, int threads_per_block) override;
};
