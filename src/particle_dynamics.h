#pragma once

#include "defines.h"


class ParticleDynamics {
public:
    ParticleDynamics();

    void unpack_state();
    void compute_rhs(std::vector<float>& rhs, const std::vector<float>& u, const float t);
    void compute_jacobian(std::vector<float>& rhs, const std::vector<float>& u, const float t);

    void initialize_to_one_particle(const float x0, const float y0);
    void initialize_to_two_particles(const float x0, const float y0);
    void initialize_to_cube(const float x0, const float y0);

    Vector xx;
    Vector xy;
    size_t n;

    std::vector<float> state;
    float time;

private:
    void resize(const size_t new_n);

    std::vector<size_t> material;
    std::vector<float> mass;
    std::vector<std::vector<float>> c_a;
    std::vector<std::vector<float>> c_r;
    std::vector<std::vector<float>> c_d;
};
