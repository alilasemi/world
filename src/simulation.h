#pragma once

#include "defines.h"


class Simulation {
public:
    Simulation();

    void take_step();

    void unpack_state();
    void compute_rhs(std::vector<float>& rhs, const std::vector<float>& u, const float t);

    void initialize_to_one_particle(const float x0, const float y0);

    Vector xx;
    Vector xy;
//    Vector vx;
//    Vector vy;
//    Vector ax;
//    Vector ay;
    size_t n;

    std::vector<float> state;
    float time;

private:
    void resize(const size_t new_n);
    void compute_accelerations();
    float compute_timestep(const float dt_max);
};
