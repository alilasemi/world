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
        const float* mass, float* rhs, const int* neighbors, const float* body_force_x, const float* body_force_y,
        size_t n, int particles_per_cell, PhysicsParams physics) {
    int index = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;

    const float g = physics.gravity;
    const float floor_y = physics.floor_y;
    const float ceiling_y = physics.ceiling_y;
    const float left_wall_x = physics.left_wall_x;
    const float right_wall_x = physics.right_wall_x;
    const float max_force = physics.max_force;
    const float radius = physics.particle_radius;
    const float restitution = physics.restitution;
    const float spring_k = max_force / radius;  // linear spring constant, pre-clamp
    int row_stride = 9 * particles_per_cell;
    for (int i = index; i < n; i += stride) {
        const float x = state[4 * i + 0];
        const float y = state[4 * i + 1];
        const float vx = state[4 * i + 2];
        const float vy = state[4 * i + 3];
        float force_x = 0;
        float force_y = 0;
        const size_t mat = material[i];
        const float m_i = mass[mat];
        // Fixed boundaries (floor/ceiling/walls) are the infinite-mass limit
        // of a two-body contact, so the dashpot just uses this particle's
        // own mass -- see restitution_to_damping() above.
        const float c_wall = restitution_to_damping(restitution, spring_k, m_i);

        // Gravity
        force_y -= m_i * g;

        // Floor force
        float floor_dist = y - floor_y - radius;
        float floor_force = max(0.f, min(max_force, -max_force / radius * floor_dist));
        force_y += floor_force; // Repulsive force
        if (floor_dist < 0.f) force_y -= c_wall * vy; // Damping, contact-only
        // Ceiling force
        float ceiling_dist = ceiling_y - y - radius;
        float ceiling_force = max(0.f, min(max_force, -max_force / radius * ceiling_dist));
        force_y -= ceiling_force; // Repulsive force
        if (ceiling_dist < 0.f) force_y -= c_wall * vy; // Damping, contact-only
        // Wall on the left
        float wall_dist_left = x - left_wall_x - radius;
        float left_wall_force = max(0.f, min(max_force, -max_force / radius * wall_dist_left));
        force_x += left_wall_force; // Repulsive force
        if (wall_dist_left < 0.f) force_x -= c_wall * vx; // Damping, contact-only
        // Wall on the right
        float wall_dist_right = right_wall_x - x - radius;
        float right_wall_force = max(0.f, min(max_force, -max_force / radius * wall_dist_right));
        force_x -= right_wall_force; // Repulsive force
        if (wall_dist_right < 0.f) force_x -= c_wall * vx; // Damping, contact-only

        // Particle-particle interactions, from the flat neighbor list that
        // FindNeighborsKernel already computed via the spatial grid.
        int base = i * row_stride;
        for (int k = 0; k < row_stride; ++k) {
            int j = neighbors[base + k];
            if (j < 0) break; // sentinel: no more neighbors for this particle
            const float x_j = state[4 * j + 0];
            const float y_j = state[4 * j + 1];
            const float vx_j = state[4 * j + 2];
            const float vy_j = state[4 * j + 3];
            const float dx = x - x_j;
            const float dy = y - y_j;
            const float norm = sqrtf(dx * dx + dy * dy);
            const float dist = norm - radius - radius;
            const size_t mat_j = material[j];
            // Repulsive force
            float force = max(0.f, min(max_force, -max_force / radius * dist));
            force_y += force * (dy / norm);
            force_x += force * (dx / norm);
            // Damping based on relative velocity, contact-only, using the
            // two-body reduced mass (m_i*m_j / (m_i+m_j)) so the same
            // restitution target is met regardless of the mass ratio.
            if (dist < 0.f) {
                const float m_j = mass[mat_j];
                const float m_sum = m_i + m_j;
                const float m_reduced = (m_sum > 0.f) ? (m_i * m_j) / m_sum : 0.f;
                const float c_pair = restitution_to_damping(restitution, spring_k, m_reduced);
                force_x -= c_pair * (vx - vx_j);
                force_y -= c_pair * (vy - vy_j);
            }
        }

        // Externally-supplied (e.g. AI-predicted) body force, already
        // interpolated onto this particle's position.
        force_x += body_force_x[i];
        force_y += body_force_y[i];

        const float ax = force_x / mass[mat];
        const float ay = force_y / mass[mat];

        rhs[4 * i + 0] = vx; // dx/dt = vx
        rhs[4 * i + 1] = vy; // dy/dt = vy
        rhs[4 * i + 2] = ax; // dvx/dt = ax
        rhs[4 * i + 3] = ay; // dvy/dt = ay
    }
}


ComputeRHSKernel::ComputeRHSKernel(const float* state_, const int* material_, const float* mass_,
            const int* neighbors_, const float* body_force_x_, const float* body_force_y_,
            const int n_, const int particles_per_cell_, const PhysicsParams physics_,
            const int threads_per_block_, float* rhs_, bool timing_enabled_)
        : Kernel(n_, threads_per_block_, timing_enabled_), state(state_), material(material_), mass(mass_), neighbors(neighbors_),
          body_force_x(body_force_x_), body_force_y(body_force_y_),
          particles_per_cell(particles_per_cell_), physics(physics_), rhs(rhs_) {
}


void ComputeRHSKernel::call_kernel(int blocks, int threads_per_block) {
    compute_rhs_kernel<<<blocks, threads_per_block>>>(state, material, mass, rhs,
            neighbors, body_force_x, body_force_y, n, particles_per_cell, physics);
}
