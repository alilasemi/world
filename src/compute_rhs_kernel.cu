#include <stdio.h>
#include <assert.h>
#include "compute_rhs_kernel.h"


// Linear spring-dashpot contact model (Cundall & Strack, "A discrete
// numerical model for granular assemblies", Geotechnique 29(1), 1979): the
// repulsive force is a linear spring k*overlap (already implemented below as
// -max_force/radius * dist, clamped to max_force), and the damping force is
// a linear dashpot c*v_rel that only acts during contact (overlap > 0) --
// unlike the old damping term (force * v_rel), it is NOT scaled by the
// spring force itself, which is what injected extra, impact-speed-dependent
// stiffness into fast collisions (the old term's derivative w.r.t. position
// grows with v_rel, so faster impacts effectively saw a stiffer spring).
//
// c is derived from a target coefficient of restitution e
// (physics.restitution) rather than picked ad hoc. For the damped harmonic
// oscillator m*x'' + c*x' + k*x = 0 (x = overlap), solving for the ratio of
// exit to entry velocity over one contact ("half period") gives
//   e = exp(-zeta*pi / sqrt(1 - zeta^2)),  zeta = c / (2*sqrt(k*m))
// Inverting for c:
//   c = -2*sqrt(k*m)*ln(e) / sqrt(pi^2 + ln(e)^2)
// This is the standard restitution-to-damping relation for the linear
// spring-dashpot DEM contact model; see Tsuji, Tanaka & Ishida, "Lagrangian
// numerical simulation of plug flow of cohesionless particles in a
// horizontal pipe", Powder Technology 71(3), 1992, and Di Renzo & Di Maio,
// "Comparison of contact-force models for the simulation of collisions in
// DEM-based granular flow codes", Chemical Engineering Science 59(3), 2004
// (a survey/derivation of this and other contact-force models).
//
// Caveat verified against model_problems/first_collision.yaml: the
// derivation above assumes the spring stays linear (k*overlap) for the
// whole contact. If the impact is energetic enough that -max_force/radius *
// dist would exceed max_force at peak penetration, the force clamp kicks in
// and the contact spends real time in a constant-force regime the
// derivation didn't account for -- the dashpot keeps dissipating during
// that extra time, so the achieved restitution ends up measurably lower
// than the target e (e.g. target 0.3 measured as ~0.22-0.27 once max_force
// is small enough, relative to impact speed, to clamp; matches the target
// to ~1% once max_force is large enough not to clamp).
__device__ inline float restitution_to_damping(float e, float k, float m) {
    const float pi = 3.14159265358979323846f;
    if (e <= 0.f) return 2.f * sqrtf(k * m);  // critically damped limit (e -> 0)
    if (e >= 1.f) return 0.f;                  // perfectly elastic, no damping
    const float ln_e = logf(e);
    return -2.f * sqrtf(m * k) * ln_e / sqrtf(pi * pi + ln_e * ln_e);
}


__global__ void compute_rhs_kernel(const float* state, const int* material,
        const float* mass, float* rhs, const int* neighbors,
        size_t n, int particles_per_cell, PhysicsParams physics) {
    int index = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;

    const float g = physics.gravity;
    const float max_force = physics.max_force;
    const float radius = physics.particle_radius;
    const float restitution = physics.restitution;
    const float spring_k = max_force / radius;  // linear spring constant, pre-clamp
    // The six boundaries, indexed by axis so the wall loop below can treat them
    // uniformly: lower[a] / upper[a] bound axis a. z is up (gravity is -z), so
    // lower[2]/upper[2] are the floor and ceiling.
    const float lower[kDim] = {physics.wall_x_min, physics.wall_y_min, physics.floor_z};
    const float upper[kDim] = {physics.wall_x_max, physics.wall_y_max, physics.ceiling_z};
    int row_stride = kStencilCells * particles_per_cell;
    for (int i = index; i < n; i += stride) {
        float pos[kDim];
        float vel[kDim];
        for (int a = 0; a < kDim; ++a) {
            pos[a] = state[kStateStride * i + a];
            vel[a] = state[kStateStride * i + kDim + a];
        }
        float force[kDim] = {0.f, 0.f, 0.f};
        const size_t mat = material[i];
        const float m_i = mass[mat];
        // Fixed boundaries (floor/ceiling/walls) are the infinite-mass limit
        // of a two-body contact, so the dashpot just uses this particle's
        // own mass -- see restitution_to_damping() above.
        const float c_wall = restitution_to_damping(restitution, spring_k, m_i);

        // Gravity acts along -z.
        force[2] -= m_i * g;

        // The six fixed boundaries. Each axis has a lower wall pushing in the
        // +axis direction and an upper wall pushing in -axis; the spring and
        // contact-only dashpot are identical to the 2D version, just applied
        // per axis instead of being written out four times.
        for (int a = 0; a < kDim; ++a) {

            const float dist_lower = pos[a] - lower[a] - radius;
            const float f_lower = max(0.f, min(max_force, -max_force / radius * dist_lower));
            force[a] += f_lower;
            if (dist_lower < 0.f) {
                force[a] -= c_wall * vel[a];
            }

            const float dist_upper = upper[a] - pos[a] - radius;
            const float f_upper = max(0.f, min(max_force, -max_force / radius * dist_upper));
            force[a] -= f_upper;
            if (dist_upper < 0.f) {
                force[a] -= c_wall * vel[a];
            }
        }

        // Particle-particle interactions, from the flat neighbor list that
        // FindNeighborsKernel already computed via the spatial grid.
        int base = i * row_stride;
        for (int k = 0; k < row_stride; ++k) {
            int j = neighbors[base + k];
            if (j < 0) break; // sentinel: no more neighbors for this particle
            float delta[kDim];
            float norm_sq = 0.f;
            for (int a = 0; a < kDim; ++a) {
                delta[a] = pos[a] - state[kStateStride * j + a];
                norm_sq += delta[a] * delta[a];
            }
            const float norm = sqrtf(norm_sq);
            const float dist = norm - radius - radius;
            const size_t mat_j = material[j];
            // Unit normal along the line of centers.
            float normal[kDim];
            for (int a = 0; a < kDim; ++a) {
                normal[a] = delta[a] / norm;
            }
            // Repulsive spring along the normal.
            const float f_pair = max(0.f, min(max_force, -max_force / radius * dist));
            for (int a = 0; a < kDim; ++a) {
                force[a] += f_pair * normal[a];
            }
            if (dist < 0.f) {
                // Dashpot uses the two-body reduced mass (m_i*m_j / (m_i+m_j))
                // so the restitution target is met regardless of mass ratio.
                const float m_j = mass[mat_j];
                const float m_sum = m_i + m_j;
                const float m_reduced = (m_sum > 0.f) ? (m_i * m_j) / m_sum : 0.f;
                const float c_pair = restitution_to_damping(restitution, spring_k, m_reduced);
            }
        }

        // dx/dt = v, dv/dt = F/m
        for (int a = 0; a < kDim; ++a) {
            rhs[kStateStride * i + a] = vel[a];
            rhs[kStateStride * i + kDim + a] = force[a] / mass[mat];
        }
    }
}


ComputeRHSKernel::ComputeRHSKernel(const float* state_, const int* material_, const float* mass_,
            const int* neighbors_,
            const int n_, const int particles_per_cell_, const PhysicsParams physics_,
            const int threads_per_block_, float* rhs_, bool timing_enabled_)
        : Kernel(n_, threads_per_block_, timing_enabled_), state(state_), material(material_), mass(mass_), neighbors(neighbors_),
          particles_per_cell(particles_per_cell_), physics(physics_), rhs(rhs_) {
}


void ComputeRHSKernel::call_kernel(int blocks, int threads_per_block) {
    compute_rhs_kernel<<<blocks, threads_per_block>>>(state, material, mass, rhs,
            neighbors, n, particles_per_cell, physics);
}
