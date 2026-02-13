#pragma once

#include "defines.h"

template <class System>
class DormandPrince {
public:
    System& system;

    DormandPrince(System& _system);

    void take_step();

private:
    bool first_step;
    float t;
    float dt;
    std::vector<float> state_new;
    std::vector<float> state_hat;
    std::vector<std::vector<float>> k;
    std::vector<std::vector<float>> aij = {
        {0, 0, 0, 0, 0, 0},
        {1/5.0f, 0, 0, 0, 0, 0},
        {3/40.0f, 9/40.0f, 0, 0, 0, 0},
        {44/45.0f, -56/15.0f, 32/9.0f, 0, 0, 0},
        {19372/6561.0f, -25360/2187.0f, 64448/6561.0f, -212/729.0f, 0, 0},
        {9017/3168.0f, -355/33.0f, 46732/5247.0f, 49/176.0f, -5103/18656.0f, 0},
        {35/384.0f, 0, 500/1113.0f, 125/192.0f, -2187/6784.0f, 11/84.0f}
    };
    std::vector<float> ci;
    std::vector<float> bhat = {35/384.0f, 0, 500/1113.0f, 125/192.0f, -2187/6784.0f, 11/84.0f, 0};
    std::vector<float> b = {5179/57600.0f, 0, 7571/16695.0f, 393/640.0f, -92097/339200.0f, 187/2100.0f, 1/40.0f};
    //aij = np.array([
    //    [0, 0, 0, 0, 0, 0],
    //    [1/5, 0, 0, 0, 0, 0],
    //    [3/40, 9/40, 0, 0, 0, 0],
    //    [44/45, -56/15, 32/9, 0, 0, 0],
    //    [19372/6561, -25360/2187, 64448/6561, -212/729, 0, 0],
    //    [9017/3168, -355/33, 46732/5247, 49/176, -5103/18656, 0],
    //    [35/384, 0, 500/1113, 125/192, -2187/6784, 11/84]
    //])
    //ci = np.sum(aij, axis=1)
    //bhat = np.array([35/384, 0, 500/1113, 125/192, -2187/6784, 11/84, 0])
    //b = np.array([5179/57600, 0, 7571/16695, 393/640, -92097/339200, 187/2100, 1/40])

    void compute_k(const size_t i);

};
