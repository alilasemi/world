#include <iostream>
#include "particle_dynamics_cuda.h"


int main() {

    // Set up simulation
    ParticleDynamicsCUDA sim;

    for (int i = 0; i < 10; ++i) {
        // Run sim and send state
        for (int steps = 0; steps < 10; ++steps) {
            sim.take_step();
        }
        sim.unpack_state();
    }

    // Print timings
    std::cout << "Time to update grid: " << sim.time_update_grid << " ms" << std::endl;
    std::cout << "Time to compute RHS: " << sim.time_compute_rhs << " ms" << std::endl;
    std::cout << "Time to take step: " << sim.time_take_step << " ms" << std::endl;
    std::cout << "Time to unpack_state: " << sim.time_unpack_state << " ms" << std::endl;
}
