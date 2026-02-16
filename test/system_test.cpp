#include <dormand_prince.h>
#include <gtest/gtest.h>

#include <cmath>

#include "example_odes.h"
#include "simulation.h"


std::vector<float> linspace(const float start, const float end, const size_t num_points) {
    std::vector<float> points(num_points);
    float step = (end - start) / (static_cast<float>(num_points - 1));
    for (size_t i = 0; i < num_points; ++i) {
        points[i] = start + static_cast<float>(i) * step;
    }
    return points;
}

std::vector<float> finite_difference_jacobian(const ExampleODE& system, const std::vector<float>& u, const float t, const float epsilon) {
    std::vector<float> jacobian(1);
    std::vector<float> u_right = u;
    std::vector<float> u_left = u;
    u_right[0] += epsilon;
    u_left[0] -= epsilon;
    std::vector<float> rhs_right(1);
    std::vector<float> rhs_left(1);
    system.compute_rhs(rhs_right, u_right, t);
    system.compute_rhs(rhs_left, u_left, t);
    for (size_t i = 0; i < jacobian.size(); ++i) {
        jacobian[i] = (rhs_right[i] - rhs_left[i]) / (2.0f * epsilon);
    }
    return jacobian;
}


TEST(SystemTestSuite, JacobianShouldBeCloseToFiniteDifferenceApproximation) {
    ExampleODE system;

    std::vector<std::vector<float>> u = {
        {0.0f},
        {1.0f},
        {2.0f},
        {3.0f}
    };
    std::vector<float> t = {
        0.0f,
        1.0f,
        2.0f,
        3.0f
    };

    for (size_t i = 0; i < u.size(); ++i) {
        std::vector<float> jacobian_analytical(1);
        system.compute_jacobian(jacobian_analytical, u[i], t[i]);

        std::vector<float> jacobian_fd = finite_difference_jacobian(system, u[i], t[i], 1e-3f);

        float norm_diff = 0.0f;
        for (size_t j = 0; j < jacobian_analytical.size(); ++j) {
            norm_diff += powf(jacobian_analytical[j] - jacobian_fd[j], 2);
        }
        norm_diff = std::sqrt(norm_diff);
        ASSERT_LT(norm_diff, 1e-4f) << "Jacobian differs from finite difference approximation at u = " << u[i][0] << ", t = " << t[i];
    }
}
