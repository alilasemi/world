#include "energy_kernel.h"
#include "cuda_check.h"


// Elastic energy stored in one contact of overlap `delta`.
//
// The contact force is a linear spring k*delta CLAMPED at max_force, so the
// potential is piecewise: quadratic until the clamp engages, linear after.
// Since k = max_force/radius, the clamp engages at exactly delta = radius:
//
//   delta <= radius:  U = 0.5*k*delta^2
//   delta >  radius:  U = 0.5*k*radius^2 + max_force*(delta - radius)
//                       = 0.5*max_force*radius + max_force*(delta - radius)
//
// Getting this piecewise form right matters: the clamped regime is exactly
// where overlaps are large and the stored energy is biggest, so treating the
// spring as purely quadratic would badly overestimate it there.
//
// Only the spring is included. The dashpot and Coulomb friction are
// dissipative, not conservative -- they store nothing, so they must not
// appear here.
__device__ inline float contact_energy(float delta, float radius, float max_force) {
    if (delta <= 0.f) return 0.f;
    const float k = max_force / radius;
    if (delta <= radius) return 0.5f * k * delta * delta;
    return 0.5f * max_force * radius + max_force * (delta - radius);
}


__global__ void energy_kernel(const float* state, const int* material, const float* mass,
        const int* neighbors, size_t n, int particles_per_cell, PhysicsParams physics,
        float* energy) {
    int index = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;
    const float radius = physics.particle_radius;
    const float max_force = physics.max_force;
    const float lower[kDim] = {physics.wall_x_min, physics.wall_y_min, physics.floor_z};
    const float upper[kDim] = {physics.wall_x_max, physics.wall_y_max, physics.ceiling_z};
    const int row_stride = kStencilCells * particles_per_cell;

    for (int i = index; i < n; i += stride) {
        float total = 0.f;

        // Kinetic energy.
        float speed_sq = 0.f;
        for (int a = 0; a < kDim; ++a) {
            const float v = state[kStateStride * i + kDim + a];
            speed_sq += v * v;
        }
        const float m = mass[material[i]];
        total += 0.5f * m * speed_sq;

        // Gravitational potential: z is up, so height above the floor is the arm.
        total += m * physics.gravity * (state[kStateStride * i + 2] - physics.floor_z);

        // Elastic energy in the six wall contacts.
        for (int a = 0; a < kDim; ++a) {
            const float p = state[kStateStride * i + a];
            total += contact_energy(radius - (p - lower[a]), radius, max_force);
            total += contact_energy(radius - (upper[a] - p), radius, max_force);
        }

        // Elastic energy in particle-particle contacts. Each pair appears in
        // both particles' neighbor rows, so count it only from the lower index
        // -- otherwise every contact would be double-counted.
        const int base = i * row_stride;
        for (int slot = 0; slot < row_stride; ++slot) {
            const int j = neighbors[base + slot];
            if (j < 0) break;  // sentinel: no more neighbors
            if (j <= i) continue;
            float dist_sq = 0.f;
            for (int a = 0; a < kDim; ++a) {
                const float d = state[kStateStride * i + a] - state[kStateStride * j + a];
                dist_sq += d * d;
            }
            total += contact_energy(2.f * radius - sqrtf(dist_sq), radius, max_force);
        }

        atomicAdd(energy, total);
    }
}


EnergyKernel::EnergyKernel(const float* state_, const int* material_, const float* mass_,
        const int* neighbors_, const int n_, const int particles_per_cell_,
        const PhysicsParams physics_, const int threads_per_block_, float* energy_,
        bool timing_enabled_)
        : Kernel(n_, threads_per_block_, timing_enabled_), state(state_), material(material_),
          mass(mass_), neighbors(neighbors_), particles_per_cell(particles_per_cell_),
          physics(physics_), energy(energy_) {
}


void EnergyKernel::call_kernel(int blocks, int threads_per_block) {
    // Reset the accumulator before each launch -- we want a fresh total every
    // call, not a running sum across calls.
    CUDA_CHECK(cudaMemset(energy, 0, sizeof(float)));
    energy_kernel<<<blocks, threads_per_block>>>(state, material, mass, neighbors, n,
            particles_per_cell, physics, energy);
}
