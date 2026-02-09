#pragma once

#include "defines.h"


class InitialCondition {
public:
    InitialCondition(const unsigned N_, Vector& rho_, Vector& u_, Vector& v_)
        : N(N_), size(rho_.size()), rho(rho_), u(u_), v(v_) {
    }

    void initialize_to_stagnant_fluid(const float rho_0);

    void initialize_to_two_stagnant_fluids(const float rho_0, const float rho_1);

private:
    const unsigned N;
    const size_t size;
    Vector& rho;
    Vector& u;
    Vector& v;
};
