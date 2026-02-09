#include "initial_condition.h"

void InitialCondition::initialize_to_stagnant_fluid(const float rho_0) {
    rho = Vector(size, rho_0);
    u = Vector(size, 0.0);
    v = Vector(size, 0.0);
}


void InitialCondition::initialize_to_two_stagnant_fluids(const float rho_0, const float rho_1) {
    u = Vector(size, 0.0);
    v = Vector(size, 0.0);

    // Set the density to rho_0 in the left half and rho_1 in the right half
    for (unsigned i = 0; i < N + 2; ++i) {
        for (unsigned j = 0; j < N + 2; ++j) {
            if (j < (N + 2) / 2) {
                rho[IX(i, j)] = rho_0;
            } else {
                rho[IX(i, j)] = rho_1;
            }
        }
    }
}
