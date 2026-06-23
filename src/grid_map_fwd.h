#pragma once

// GridMap (defined in grid_map.h) wraps cuco::static_map, which requires
// nvcc to even parse (cuco/detail/__config errors out without
// __CUDACC_VER_MAJOR__). ParticleDynamicsCUDA and the kernel classes are
// declared in headers that plain g++-compiled .cpp files (profiling_sim.cpp,
// broadcast.cpp, tests) also include, so those headers must only see this
// forward declaration -- never grid_map.h itself. Only the nvcc-compiled
// .cu files that actually construct/call into a GridMap include grid_map.h.
class GridMap;
