#include <dormand_prince.h>
#include <gtest/gtest.h>

#include <cmath>

#include "example_odes.h"


TEST(DormandPrinceTestSuite, ShouldBeFifthOrderAccurate) {
    const std::vector<float> timesteps = {1.5f, 1.25f, 1.f, 0.75f, 0.5f};
    float final_time = 20.0f;
    std::vector<float> errors;

    for (const float dt : timesteps) {
        ExampleODE system;
        DormandPrince<ExampleODE> integrator(system, dt);

        std::vector<float> solution = {system.state[0]};
        std::vector<float> time = {system.time};
        while (system.time < final_time) {
            integrator.take_step();
            solution.push_back(system.state[0]);
            time.push_back(system.time);
        }

        std::vector<float> exact_solution;
        system.compute_exact_solution(time, exact_solution);
        errors.push_back(std::abs(solution.back() - exact_solution.back()));
    }
    size_t n = errors.size();
    float order_of_accuracy = std::log(errors[n - 1] / errors[n - 2]) / std::log(timesteps[n - 1] / timesteps[n - 2]);
    ASSERT_GT(order_of_accuracy, 4.9f) << "Expected order of accuracy to be at least 5, but got " << order_of_accuracy;
}

//class SimulationTestSuiteFixture :public ::testing::TestWithParam<unsigned> {
//protected:
//    Simulation sim;
//
//    // Create simulation with no viscosity, no diffusion, and variable grid size
//    SimulationTestSuiteFixture()
//            : sim(GetParam(), 0.0f, 0.0f, 0.01f, 20) {
//    }
//};
//
//TEST_P(SimulationTestSuiteFixture, StagnantInviscidFluidWithNoInflowShouldNotChange) {
//    // Save initial state, then run
//    auto initial_rho = sim.rho;
//    auto initial_u = sim.u;
//    auto initial_v = sim.v;
//    sim.run();
//
//    // Make sure nothing has changed
//    for (size_t i = 0; i < sim.rho.size(); ++i) {
//        ASSERT_FLOAT_EQ(initial_rho[i], sim.rho[i]) << "at index " << i;
//        ASSERT_FLOAT_EQ(initial_u[i], sim.u[i]) << "at index " << i;
//        ASSERT_FLOAT_EQ(initial_v[i], sim.v[i]) << "at index " << i;
//    }
//}
//
//INSTANTIATE_TEST_SUITE_P(
//        SimulationTestSuite,
//        SimulationTestSuiteFixture,
//        ::testing::Values(
//                2, 3, 4, 20, 100
//        ));
//
//
//TEST(SimulationTestSuite, GenerateMeshShouldCreateCorrectNodesAndTrianglesFor3x3Grid) {
//    Simulation sim(1, 0.0f, 0.0f, 0.01f, 20);
//    sim.generate_mesh();
//
//    // Diagram of the grid and triangles:
//    //
//    // (0,2) O-------O------O (2,2)
//    //       | 4   / | 6  / |
//    //       |   /   |  /   |
//    //       | /   5 |/   7 |
//    //       O-------O------O
//    //       | 0   / | 2  / |
//    //       |   /   |  /   |
//    //       | /   1 |/   3 |
//    // (0,0) O-------O------O (2,0)
//
//    // Make sure there are 9 nodes and 8 triangles
//    ASSERT_EQ(sim.node_coords.size(), 9 * 3); // 9 nodes, each with 3 coordinates (x, y, z)
//    ASSERT_EQ(sim.triangles.size(), 8 * 3); // 8 triangles, each with 3 vertex indices
//
//    // Check node coordinates
//    std::vector<float> expected_node_coords = {
//        -1.0f, -1.0f, 0.0f, // (0,0)
//         0.0f, -1.0f, 0.0f, // (1,0)
//         1.0f, -1.0f, 0.0f, // (2,0)
//        -1.0f,  0.0f, 0.0f, // (0,1)
//         0.0f,  0.0f, 0.0f, // (1,1)
//         1.0f,  0.0f, 0.0f, // (2,1)
//        -1.0f,  1.0f, 0.0f, // (0,2)
//         0.0f,  1.0f, 0.0f, // (1,2)
//         1.0f,  1.0f, 0.0f  // (2,2)
//    };
//    for (size_t i = 0; i < expected_node_coords.size(); ++i) {
//        ASSERT_FLOAT_EQ(sim.node_coords[i], expected_node_coords[i]) << "at index " << i;
//    }
//
//    // Check triangle connectivity
//    std::vector<unsigned> expected_triangles = {
//        0, 4, 3, // Triangle 0
//        0, 1, 4, // Triangle 1
//        1, 5, 4, // Triangle 2
//        1, 2, 5, // Triangle 3
//        3, 7, 6, // Triangle 4
//        3, 4, 7, // Triangle 5
//        4, 8, 7, // Triangle 6
//        4, 5, 8  // Triangle 7
//    };
//    for (size_t i = 0; i < expected_triangles.size(); ++i) {
//        ASSERT_EQ(sim.triangles[i], expected_triangles[i]) << "at index " << i;
//    }
//}
