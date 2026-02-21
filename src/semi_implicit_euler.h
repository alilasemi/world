#pragma once

#include "defines.h"

template <class System>
class SemiImplicitEuler {
public:
    System& system;

    SemiImplicitEuler(System& _system, const float _dt);

    void take_step();

private:
    float dt;
    size_t n;
    std::vector<float> rhs;
};
