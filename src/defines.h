#pragma once

#include <iostream>
#include <vector>

using std::cout, std::endl;

#define IX(i,j) (static_cast<unsigned>(i) + (N+2u)*static_cast<unsigned>(j))

using Vector = std::vector<float>;

enum StateVariable {
    RHO,
    U,
    V,
    DIV,
    P
};
