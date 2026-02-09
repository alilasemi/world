#pragma once

#include "boundary_condition.h"
#include "defines.h"

#include <memory>
#include <string>


class Simulation {

public:
    Simulation(const unsigned N_, const float visc_, const float diff_, const float dt_, const unsigned n_iter_);

    void generate_mesh();

    void write_to_file(const unsigned iter, Vector data, std::string filename_prefix);

    void run();

    void take_step();

    void add_source(Vector& x, const Vector& s);

    void diffuse(StateVariable var, Vector& x, const Vector& x0);

    void advect(StateVariable var, Vector& d, const Vector& d0, const Vector& u, const Vector& v);

    void density_step();

    void velocity_step();

    void project(Vector& u, Vector& v, Vector& p, Vector& div);

    void set_inflow_boundary_condition(const float x_left, const float x_right,
            InflowBoundaryCondition::Position position, const float inflow_density, const float inflow_velocity);

    void initialize_to_stagnant_fluid(const float rho_0);

    void initialize_to_two_stagnant_fluids(const float rho_0, const float rho_1);

    void set_bnd(StateVariable var, Vector& x);

    Vector rho;
    Vector u;
    Vector v;

    std::vector<float> node_coords;
    std::vector<unsigned> triangles;


private:
    const unsigned N;
    const float visc;
    const float diff;
    const float dt;
    const unsigned n_iter;
    const unsigned size;

    Vector u_prev;
    Vector v_prev;
    Vector rho_prev;

    bool apply_inlet_boundary_condition = false;
    float inlet_velocity;
    std::vector<std::unique_ptr<BoundaryCondition>> boundary_conditions;
};
