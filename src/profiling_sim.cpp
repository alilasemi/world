#include <iostream>
#include "particle_dynamics.h"


int main() {

    // Set up simulation
    ParticleDynamics sim;

    float find0  = sim.find_neighbors_wct();
    float interp0 = sim.interpolate_force_wct();
    float rhs0   = sim.compute_rhs_wct();
    float step0  = sim.take_step_wct();

    for (int i = 0; i < 10; ++i) {
        for (int steps = 0; steps < 10; ++steps) {
            sim.take_step();
        }
        sim.unpack_state();
    }

    std::cout << "Time to update grid: "    << sim.find_neighbors_wct()    - find0  << " ms" << std::endl;
    std::cout << "Time to interpolate:  "   << sim.interpolate_force_wct() - interp0 << " ms" << std::endl;
    std::cout << "Time to compute RHS: "    << sim.compute_rhs_wct()       - rhs0   << " ms" << std::endl;
    std::cout << "Time to take step: "      << sim.take_step_wct()         - step0  << " ms" << std::endl;
    std::cout << "Time to unpack_state: "   << sim.time_unpack_state                << " ms" << std::endl;
}
