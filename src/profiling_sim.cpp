#include <iostream>
#include <string>
#include "particle_dynamics.h"
#include "sim_config.h"


int main(int argc, char** argv) {

    // Load configuration (optional path as argv[1], default "config.yaml").
    const std::string config_path = (argc > 1) ? argv[1] : "config.yaml";
    const SimConfig config = load_config(config_path);

    // Set up simulation
    ParticleDynamics sim(config);

    float find0  = sim.find_neighbors_wct();
    float rhs0   = sim.compute_rhs_wct();
    float step0  = sim.take_step_wct();

    for (int i = 0; i < config.profiling_outer_iters; ++i) {
        for (int steps = 0; steps < config.profiling_steps_per_iter; ++steps) {
            sim.take_step();
        }
        sim.unpack_state();
    }

    std::cout << "Time to update grid: "    << sim.find_neighbors_wct()    - find0  << " ms" << std::endl;
    std::cout << "Time to compute RHS: "    << sim.compute_rhs_wct()       - rhs0   << " ms" << std::endl;
    std::cout << "Time to take step: "      << sim.take_step_wct()         - step0  << " ms" << std::endl;
    std::cout << "Time to unpack_state: "   << sim.time_unpack_state                << " ms" << std::endl;
}
