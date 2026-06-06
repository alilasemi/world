#include "particle_dynamics_cuda.h"


int main() {

    // Set up simulation
    ParticleDynamicsCUDA sim;

    while (true) {
        // Run sim and send state
        for (int steps = 0; steps < 10; ++steps) {
            sim.take_step();
        }
        sim.unpack_state();
    }
}
