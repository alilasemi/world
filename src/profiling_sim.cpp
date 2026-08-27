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

    // Total energy per outer iteration. This is the cheapest diagnostic for
    // whether a configuration is *numerically* sound: no work is done on the
    // system after release, so total energy must decrease monotonically (the
    // dashpot and friction only dissipate). Energy that GROWS means the
    // timestep is too large for the contact stiffness and collisions are
    // injecting energy -- which shows up macroscopically as a pile that
    // "boils" and spreads like a fluid rather than settling into a heap.
    // compute_total_energy() rebuilds the neighbor list (it needs pair contacts
    // consistent with current positions), which advances the same lifetime
    // timing accumulator the report below reads. Subtract that off, or the
    // energy diagnostic would silently inflate the reported neighbor-search
    // time by one extra pass per outer iteration.
    float find_energy_overhead = 0.f;
    const auto energy_at = [&sim, &find_energy_overhead]() {
        const float before = sim.find_neighbors_wct();
        const float e = sim.compute_total_energy();
        find_energy_overhead += sim.find_neighbors_wct() - before;
        return e;
    };

    std::cout << "iter,sim_time,total_energy" << std::endl;
    std::cout << "0," << sim.time << "," << energy_at() << std::endl;
    for (int i = 0; i < config.profiling_outer_iters; ++i) {
        for (int steps = 0; steps < config.profiling_steps_per_iter; ++steps) {
            sim.take_step();
        }
        sim.unpack_state();
        std::cout << (i + 1) << "," << sim.time << "," << energy_at() << std::endl;
    }

    std::cout << "Time to update grid: "    << sim.find_neighbors_wct() - find0 - find_energy_overhead
              << " ms" << std::endl;
    std::cout << "Time to compute RHS: "    << sim.compute_rhs_wct()       - rhs0   << " ms" << std::endl;
    std::cout << "Time to take step: "      << sim.take_step_wct()         - step0  << " ms" << std::endl;
    std::cout << "Time to unpack_state: "   << sim.time_unpack_state                << " ms" << std::endl;
}
