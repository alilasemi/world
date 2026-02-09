#include "boundary_condition.h"


void NoSlipWallBoundaryCondition::apply_boundary_condition(StateVariable var, Vector& state) {
    for (unsigned i = 1; i <= N; ++i) {
        if (var == RHO || var == DIV || var == P) {
            state[IX(0, i)] = state[IX(1, i)]; // Left wall
            state[IX(N+1, i)] = state[IX(N, i)]; // Right wall
            state[IX(i, 0)] = state[IX(i, 1)]; // Bottom wall
            state[IX(i, N+1)] = state[IX(i, N)]; // Top wall
        } else if (var == U) {
            state[IX(0, i)] = 0;
            state[IX(N+1, i)] = 0;
            state[IX(i, 0)] = 0;
            state[IX(i, N+1)] = 0;
        } else if (var == V) {
            state[IX(0, i)] = 0;
            state[IX(N+1, i)] = 0;
            state[IX(i, 0)] = 0;
            state[IX(i, N+1)] = 0;
        }
    }
}

void InflowBoundaryCondition::apply_boundary_condition(StateVariable var, Vector& state) {
    if (position == LEFT || position == RIGHT) {
        const unsigned i = (position == LEFT) ? 0 : N + 1;
        const float sign = (position == LEFT) ? 1.f : -1.f;
        for (unsigned j = 1; j <= N; ++j) {
            float y = node_coords[3*IX(i, j) + 1];
            if (y >= left && y <= right) {
                if (var == RHO) {
                    state[IX(i, j)] = inflow_density;
                } else if (var == U) {
                    state[IX(i, j)] = sign * inflow_velocity;
                } else if (var == V) {
                    state[IX(i, j)] = 0.f;
                }
            }
        }
    } else if (position == TOP || position == BOTTOM) {
        const unsigned j = (position == BOTTOM) ? 0 : N + 1;
        const float sign = (position == BOTTOM) ? 1.f : -1.f;
        for (unsigned i = 1; i <= N; ++i) {
            float x = node_coords[3*IX(i, j)];
            if (x >= left && x <= right) {
                if (var == RHO) {
                    state[IX(i, j)] = inflow_density;
                } else if (var == U) {
                    state[IX(i, j)] = 0.f;
                } else if (var == V) {
                    state[IX(i, j)] = sign * inflow_velocity;
                }
            }
        }
    }
}
