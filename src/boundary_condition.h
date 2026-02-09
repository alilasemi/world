#pragma once

#include "defines.h"


class BoundaryCondition {
public:
    BoundaryCondition(const unsigned int N_) : N(N_) {}

    virtual ~BoundaryCondition() = default;

    virtual void apply_boundary_condition(const StateVariable var, Vector& state) = 0;

protected:
    const unsigned int N;
};

class NoSlipWallBoundaryCondition : public BoundaryCondition {
public:
    NoSlipWallBoundaryCondition(const unsigned int N_) : BoundaryCondition(N_) {}
    void apply_boundary_condition(const StateVariable var, Vector& state) override;
};

class InflowBoundaryCondition : public BoundaryCondition {
public:
    enum Position { LEFT, RIGHT, TOP, BOTTOM };
    InflowBoundaryCondition(const unsigned int N_, const std::vector<float>& node_coords_, const float left_,
            const float right_, Position position_, const float inflow_density_, const float inflow_velocity_)
        : BoundaryCondition(N_), node_coords(node_coords_), left(left_), right(right_),
          position(position_), inflow_density(inflow_density_), inflow_velocity(inflow_velocity_) {}
    void apply_boundary_condition(const StateVariable var, Vector& state) override;

private:
    const std::vector<float>& node_coords;
    const float left;
    const float right;
    Position position;
    const float inflow_density;
    const float inflow_velocity;
};
