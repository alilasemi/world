#include "semi_implicit_euler.h"

#include "defines.h"
#include "particle_dynamics.h"
#include "example_odes.h"

#include <cmath>


template <class System>
SemiImplicitEuler<System>::SemiImplicitEuler(System& _system, const float _dt) : system(_system) {
    dt = _dt;
    rhs = std::vector<float>(system.state.size());
    n = system.state.size() / 4;
}


template <class System>
void SemiImplicitEuler<System>::take_step() {
    system.compute_rhs(rhs, system.state, system.time);

    // Update velocities
    for (size_t i = 0; i < n; ++i) {
        system.state[4*i + 2] += dt * rhs[4*i + 2];
        system.state[4*i + 3] += dt * rhs[4*i + 3];
    }
    // Then, update positions using the new velocities
    for (size_t i = 0; i < n; ++i) {
        system.state[4*i + 0] += dt * system.state[4*i + 2];
        system.state[4*i + 1] += dt * system.state[4*i + 3];
    }
    system.time += dt;
    std::cout << "t = " << system.time << ": ";
//    for (size_t dof = 0; dof < system.state.size(); ++dof) {
//        std::cout << system.state[dof] << " ";
//    }
    std::cout << std::endl;

//    // I want the dt to be .001 when y (aka state[1]) is around -1,
//    // and .01 when y is around 0
//    dt = 0.01f * (1 + system.state[1]) - 0.001f * system.state[1];
    dt = 0.001f;
}


template class SemiImplicitEuler<ParticleDynamics>;
template class SemiImplicitEuler<ExampleODE>;
