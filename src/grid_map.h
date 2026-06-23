#pragma once
#include <cuco/static_map.cuh>
#include <cuda/std/functional>

#include "grid_map_fwd.h"

// Single-thread (non-cooperative-group) linear probing -- required since
// each particle thread issues its own insert/find, not a cooperative-group
// operation, and cuCollections' default probing scheme assumes a CG size of
// 4.
using GridMapImpl = cuco::static_map<int, int, cuco::extent<std::size_t>, cuda::thread_scope_device,
        cuda::std::equal_to<int>, cuco::linear_probing<1, cuco::default_hash_function<int>>>;

// Gives the cuco::static_map instantiation above a stable name that can be
// forward-declared (see grid_map_fwd.h) from headers nvcc-only code can't
// reach. Public inheritance + inherited constructors expose everything
// (insert/find/clear/ref) unchanged; move semantics fall out of the base's
// defaulted move constructor/assignment, copy stays deleted via the base.
class GridMap : public GridMapImpl {
public:
    using GridMapImpl::GridMapImpl;
};

// Empty-slot sentinels for both grid maps. Safe since cell indices, the
// cell_index*particles_per_cell+slot composite key, particle indices, and
// per-cell counts are never negative.
inline constexpr int kEmptyKeySentinel = -1;
inline constexpr int kEmptyValueSentinel = -1;
