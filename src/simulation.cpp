#include "simulation.h"
#include "defines.h"

#include </usr/include/boost/numeric/odeint/stepper/runge_kutta_dopri5.hpp>

#include <cmath>
#include <stdio.h>
#include <stdlib.h>


Simulation::Simulation() {
    // By default, initialize to one particle
    initialize_to_one_particle(0.0f, 0.0f);
}


void Simulation::resize(const size_t new_n) {
    n = new_n;
    xx = Vector(n);
    xy = Vector(n);
//    vx = Vector(n);
//    vy = Vector(n);
//    ax = Vector(n);
//    ay = Vector(n);
    state = std::vector<float>(4 * n);
//    rhs = std::vector<float>(4 * n);
}


void Simulation::initialize_to_one_particle(const float x0, const float y0) {
    resize(1);
    state[0] = x0;
    state[1] = y0;
    state[2] = 0.0f;
    state[3] = 0.0f;
}


//void Simulation::compute_accelerations() {
//    const float g = 9.81f;
//    const float m = 1.0f;
//    for (size_t i = 0; i < n; ++i) {
//        float force_x = 0;
//        float force_y = -m * g;
//        // Floor force
//        const float c_a = .1;
//        const float c_b = 0.5f;
//        force_y += c_a * pow(c_b / (y[i] + 1), 4);
//        ax[i] = force_x / m;
//        ay[i] = force_y / m;
//    }
//}


void Simulation::compute_rhs(std::vector<float>& rhs, const std::vector<float>& u, const float t) {
    const float g = 9.81f;
    const float m = 1.0f;
    float force_x = 0;
    float force_y = -m * g;
    for (size_t i = 0; i < n; ++i) {
        const float x = u[4 * i + 0];
        const float y = u[4 * i + 1];
        const float vx = u[4 * i + 2];
        const float vy = u[4 * i + 3];
        // Floor force
        const float c_a = .1;
        const float c_b = 0.5f;
        force_y += c_a * powf(c_b / (y + 1), 4);
        const float ax = force_x / m;
        const float ay = force_y / m;

        rhs[4 * i + 0] = vx; // dx/dt = vx
        rhs[4 * i + 1] = vy; // dy/dt = vy
        rhs[4 * i + 2] = ax; // dvx/dt = ax
        rhs[4 * i + 3] = ay; // dvy/dt = ay
    }
}

void Simulation::unpack_state() {
    for (size_t i = 0; i < n; ++i) {
        xx[i] = state[4 * i + 0];
        xy[i] = state[4 * i + 1];
    }
}
