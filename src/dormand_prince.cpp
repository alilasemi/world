#include "dormand_prince.h"

#include "defines.h"
#include "simulation.h"
#include "example_odes.h"

#include <cmath>


template <class System>
DormandPrince<System>::DormandPrince(System& _system) : system(_system) {
    first_step = true;
    dt = 1e-5f;
    k = std::vector<std::vector<float>>(7, std::vector<float>(system.state.size()));
    ci = std::vector<float>(7);
    for (size_t i = 0; i < 7; ++i) {
        ci[i] = 0;
        for (size_t j = 0; j < 6; ++j) {
            ci[i] += aij[i][j];
        }
    }
}


template <class System>
DormandPrince<System>::DormandPrince(System& _system, const float _dt) : DormandPrince(_system) {
    dt = _dt;
    adaptive_timestepping = false;
}

template <class System>
void DormandPrince<System>::compute_k(const size_t i) {
    state_new = system.state;
    for (size_t dof = 0; dof < system.state.size(); ++dof) {
        for (size_t j = 0; j < i; ++j) {
            state_new[dof] += aij[i][j] * dt * k[j][dof];
        }
    }
    system.compute_rhs(k[i], state_new, system.time + ci[i] * dt);
}

template <class System>
void DormandPrince<System>::take_step() {
    // Integrator stages
    size_t start_i;
    if (first_step) {
        start_i = 0;
    } else {
        k[0] = k[k.size() - 1];
        start_i = 1;
    }

    for (size_t i = start_i; i < 7; ++i) {
        compute_k(i);
    }

    state_hat = system.state;
    for (size_t dof = 0; dof < system.state.size(); ++dof) {
        for (size_t i = 0; i < 7; ++i) {
            state_hat[dof] += bhat[i] * dt * k[i][dof];
        }
    }

    state_new = system.state;
    for (size_t dof = 0; dof < system.state.size(); ++dof) {
        for (size_t i = 0; i < 7; ++i) {
            state_new[dof] += b[i] * dt * k[i][dof];
        }
    }
    first_step = false;

    // Store solution as the higher order one
    system.time += dt;
    system.state = state_new;
    for (size_t dof = 0; dof < system.state.size(); ++dof) {
        std::cout << system.state[dof] << " ";
    }
    std::cout << std::endl;

    // Error estimation and timestep update
    float error = 0;
    for (size_t dof = 0; dof < system.state.size(); ++dof) {
        error = fmax(error, fabs(state_hat[dof] - state_new[dof]));
    }
    const int p = 4;
    const float delta = 1e-5f;

    if (adaptive_timestepping) {
        dt = .9 * dt * pow(delta / error, 1.f/(p+1));
    }
    // I think the stiffness is causing bad things to happen - the timestep is
    // small so the error is small, making the error go to zero, giving division
    // by zero. But a larger timestep (i.e. larger delta) causes the simulation
    // to blow up. Cool.
}


template class DormandPrince<Simulation>;
template class DormandPrince<ExampleODE>;
