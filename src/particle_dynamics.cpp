#include "particle_dynamics.h"
#include "defines.h"

#include <cmath>
#include <stdio.h>
#include <stdlib.h>


ParticleDynamics::ParticleDynamics() {
    // By default, initialize to one particle
    initialize_to_cube(0.0f, 0.0f);
    time = 0.0f;
    // I will assign material 0 to be the walls, material 1 to be the snow, and
    // material 2 to be the sled
    mass = std::vector<float>(3);
    c_a = std::vector<std::vector<float>>(3, std::vector<float>(3, 0.0f));
    c_r = std::vector<std::vector<float>>(3, std::vector<float>(3, 0.0f));
    c_d = std::vector<std::vector<float>>(3, std::vector<float>(3, 0.0f));

    mass[0] = 0.0f;
    mass[1] = 1.0f;
    mass[2] = 10.0f;

    c_a[1][0] = 2.5f;
    c_a[1][1] = 0.0025f;
    c_a[1][2] = 0.025f;
    c_a[2][0] = 0.0f;
    c_a[2][1] = 0.0f;
    c_a[2][2] = 0.0f;

    c_r[1][0] = 0.00625f;
    c_r[1][1] = 0.000000000625f;
    c_r[1][2] = 0.00625f;
    c_r[2][0] = 0.00625f;
    c_r[2][1] = 0.00625f;
    c_r[2][2] = 0.00625f;

    c_d[1][0] = 0.25f;
    c_d[1][1] = 0.025f;
    c_d[1][2] = 0.025f;
    c_d[2][0] = 0.025f;
    c_d[2][1] = 0.025f;
    c_d[2][2] = 0.025f;

    dt = 0.001f;

    size_t grid_size = 16;
    grid = std::vector<std::vector<std::vector<size_t>>>(grid_size, std::vector<std::vector<size_t>>(grid_size));
}


void ParticleDynamics::resize(const size_t new_n) {
    n = new_n;
    xy = Vector(2*n);
    state = std::vector<float>(4 * n);
    rhs = std::vector<float>(4 * n);
    material = std::vector<size_t>(n);
}


void ParticleDynamics::initialize_to_one_particle(const float x0, const float y0) {
    resize(1);

    state[0] = x0;
    state[1] = y0;
    state[2] = 0.0f;
    state[3] = 0.0f;

    material = std::vector<size_t>(n, 1);

    unpack_state();
}


void ParticleDynamics::initialize_to_two_particles(const float x0, const float y0) {
    resize(2);

    state[0] = x0;
    state[1] = y0;
    state[2] = 0.0f;
    state[3] = 0.0f;

    state[4] = x0;
    state[5] = y0 + 0.1f;
    state[6] = 0.0f;
    state[7] = 0.0f;

    material = std::vector<size_t>(n, 1);

    unpack_state();
}


void ParticleDynamics::initialize_to_cube(const float x0, const float y0) {
    size_t num_per_side = 10;
    resize(num_per_side * num_per_side + 1);

    for (size_t i = 0; i < num_per_side; ++i) {
        for (size_t j = 0; j < num_per_side; ++j) {
            state[4 * (i * num_per_side + j) + 0] = x0 + i * 0.1f;
            state[4 * (i * num_per_side + j) + 1] = y0 + j * 0.1f;
            state[4 * (i * num_per_side + j) + 2] = 0.0f;
            state[4 * (i * num_per_side + j) + 3] = 0.0f;
        }
    }

    // Make one particle of sled on top
    state[4 * (n - 1) + 0] = -.9;
    state[4 * (n - 1) + 1] = .9;
    state[4 * (n - 1) + 2] = .4;
    state[4 * (n - 1) + 3] = 0;

    // Set all particles to be material 1 (snow) except for the last one
    material = std::vector<size_t>(n, 1);
    material[n - 1] = 2;

    unpack_state();
}


void ParticleDynamics::update_grid() {
    int grid_size = static_cast<int>(grid.size());
    // The domain is an [-1, 1] x [-1, 1] square, and the grid is grid_size x grid_size, so each cell is 2 / grid_size wide and tall
    // I will assign particles to grid cells based on their positions, and store the indices of the particles in each cell
    for (size_t i = 0; i < grid_size; ++i) {
        for (size_t j = 0; j < grid_size; ++j) {
            grid[i][j].clear();
        }
    }
    for (size_t i = 0; i < n; ++i) {
        int cell_x = std::min(grid_size - 1, std::max(0, static_cast<int>((state[4 * i + 0] + 1.0f) / 2.0f * grid_size)));
        int cell_y = std::min(grid_size - 1, std::max(0, static_cast<int>((state[4 * i + 1] + 1.0f) / 2.0f * grid_size)));
        grid[cell_x][cell_y].push_back(i);
    }
}


void ParticleDynamics::compute_rhs(std::vector<float>& rhs, const std::vector<float>& u, const float t) {
    const float g = 9.81f;
    float floor_y = -1.0f;
    float wall_x = -1.0f;
    float max_force = 1000.f;
    float r0 = 0.05f;
    float r1 = 0.1f;
    float force;
    for (size_t i = 0; i < n; ++i) {
        const float x = u[4 * i + 0];
        const float y = u[4 * i + 1];
        const float vx = u[4 * i + 2];
        const float vy = u[4 * i + 3];
        float force_x = 0;
        float force_y = 0;
        const size_t mat = material[i];
//        // Gravity
        force_y -= mass[mat] * g;
//        // Floor force
//        force_y += c_r[mat][0] * powf(1 / (y - floor_y), 4); // Repulsive force
        force = fmax(0, fmin(max_force, max_force / (r0 - r1) * (y - floor_y - r1)));
        force_y += force; // Repulsive force
        force_y -= force * vy; // Damping based on velocity
//        // Wall on the left (at -1)
//        force = fmax(0, fmin(max_force, max_force / (r0 - r1) * (x - wall_x - r1)));
//        force_x += force; // Repulsive force
//        force_x -= force * vx; // Damping based on velocity


        int grid_size = static_cast<int>(grid.size());
        int cell_x = std::min(grid_size - 1, std::max(0, static_cast<int>((state[4 * i + 0] + 1.0f) / 2.0f * grid_size)));
        int cell_y = std::min(grid_size - 1, std::max(0, static_cast<int>((state[4 * i + 1] + 1.0f) / 2.0f * grid_size)));
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                int neighbor_cell_x = cell_x + dx;
                int neighbor_cell_y = cell_y + dy;
                if (neighbor_cell_x >= 0 && neighbor_cell_x < grid_size && neighbor_cell_y >= 0 && neighbor_cell_y < grid_size) {
                    for (size_t j : grid[neighbor_cell_x][neighbor_cell_y]) {
                        if (i != j) {
                            const float x_j = u[4 * j + 0];
                            const float y_j = u[4 * j + 1];
                            const float vx_j = u[4 * j + 2];
                            const float vy_j = u[4 * j + 3];
                            const float dx = x - x_j;
                            const float dy = y - y_j;
                            const float dist = sqrtf(dx * dx + dy * dy);
                            const size_t mat_j = material[j];
                            // Repulsive force
                            force = fmaxf(0.f, fminf(max_force, max_force / (r0 - r1) * (dist - r1)));
                            force_y += force * (dy / dist);
                            force_x += force * (dx / dist);
                            // Damping based on relative velocity
                            force_x -= force * (vx - vx_j);
                            force_y -= force * (vy - vy_j);
                        }
                    }
                }
            }
        }
//        // Particle collisions
//        for (size_t j = 0; j < n; ++j) {
//            if (i != j) {
//                const float x_j = u[4 * j + 0];
//                const float y_j = u[4 * j + 1];
//                const float vx_j = u[4 * j + 2];
//                const float vy_j = u[4 * j + 3];
//                const float dx = x - x_j;
//                const float dy = y - y_j;
//                const float dist = sqrtf(dx * dx + dy * dy);
//                const size_t mat_j = material[j];
//                // Repulsive force
//                force = fmaxf(0.f, fminf(max_force, max_force / (r0 - r1) * (dist - r1)));
//                force_y += force * (dy / dist);
//                force_x += force * (dx / dist);
//                // Damping based on relative velocity
//                force_x -= force * (vx - vx_j);
//                force_y -= force * (vy - vy_j);
////                // Attractive force
////                force_x -= c_a[mat][mat_j] * powf(1 / dist, 2) * (dx / dist);
////                force_y -= c_a[mat][mat_j] * powf(1 / dist, 2) * (dy / dist);
//            }
//        }

        const float ax = force_x / mass[mat];
        const float ay = force_y / mass[mat];

        rhs[4 * i + 0] = vx; // dx/dt = vx
        rhs[4 * i + 1] = vy; // dy/dt = vy
        rhs[4 * i + 2] = ax; // dvx/dt = ax
        rhs[4 * i + 3] = ay; // dvy/dt = ay
    }
}


void ParticleDynamics::compute_jacobian(std::vector<float>& rhs, const std::vector<float>& u, const float t) {
    const float g = 9.81f;
    const float m = 1.0f;
    const float c_a = .1;
    const float c_b = 0.5f;
    float force_x = 0;
    float force_y = -m * g;
    for (size_t i = 0; i < n; ++i) {
        const float x = u[4 * i + 0];
        const float y = u[4 * i + 1];
        const float vx = u[4 * i + 2];
        const float vy = u[4 * i + 3];
        // d(vx) / dx
        rhs[4 * i + 0] = vx; // dx/dt = vx
    }
}

void ParticleDynamics::unpack_state() {
    for (size_t i = 0; i < n; ++i) {
        xy[2*i + 0] = state[4 * i + 0];
        xy[2*i + 1] = state[4 * i + 1];
    }
}


void ParticleDynamics::take_step() {
    std::cout << "Taking step with dt = " << dt << std::endl;
    std::cout << "Current state: ";
    std::cout << state[0] << " " << state[1] << " " << state[2] << " " << state[3] << std::endl;
    update_grid();

    compute_rhs(rhs, state, time);

    std::cout << state[0] << " " << state[1] << " " << state[2] << " " << state[3] << std::endl;
    std::cout << rhs[0] << " " << rhs[1] << " " << rhs[2] << " " << rhs[3] << std::endl;

    // Update velocities
    for (size_t i = 0; i < n; ++i) {
        state[4*i + 2] += dt * rhs[4*i + 2];
        state[4*i + 3] += dt * rhs[4*i + 3];
    }
    // Then, update positions using the new velocities
    for (size_t i = 0; i < n; ++i) {
        state[4*i + 0] += dt * state[4*i + 2];
        state[4*i + 1] += dt * state[4*i + 3];
    }
    time += dt;
    printf("t = %.3f: \n", time);
    for (size_t dof = 0; dof < 4*n; ++dof) {
        printf("%.2f ", state[dof]);
    }
    printf("\n");

//    // I want the dt to be .001 when y (aka state[1]) is around -1,
//    // and .01 when y is around 0
//    dt = 0.01f * (1 + system.state[1]) - 0.001f * system.state[1];
    dt = 0.001f;
}


float distance_between_two_points(
        const float x0, const float y0,
        const float x1, const float y1) {
    float dx = x1 - x0;
    float dy = y1 - y0;
    return sqrtf(dx * dx + dy * dy);
}

// The point is at (x0, y0).
// The line goes from (x1, y1) to (x2, y2).
float distance_between_point_and_line(
        const float x0, const float y0,
        const float x1, const float y1,
        const float x2, const float y2) {
    // Vector from 1 to 0
    float u0 = x0 - x1;
    float u1 = y0 - y1;
    // Vector along the line
    float v0 = x2 - x1;
    float v1 = y2 - y1;
    float v_norm = sqrtf(v0*v0 + v1*v1);
    // Component of u along the line
    float component = u0*v0 + u1*v1;
    // If it's negative, the particle is behind the line, so the closest point
    // is 0 to 1
    if (component < 0) {
        return distance_between_two_points(x0, y0, x1, y1);
    // If it's positive but less than the length of v, then the closest point is
    // on the line, so take the projection distance
    } else if (component < v_norm) {
        // w is the vector from to the projected point on the line. This is u
        // minus the component along the line.
        float w0 = u0 - component * v0 / v_norm;
        float w1 = u1 - component * v1 / v_norm;
        return sqrtf(w0*w0 + w1*w1);
    // Otherwise, it is beyond the line, so the closest point is 0 to 2
    } else {
        return distance_between_two_points(x0, y0, x2, y2);
    }

}

// The particle is at (x0, y0) and has radius r0.
// The rod goes from (x1, y1) to (x2, y2) and has radius r1.
float distance_between_particle_and_rod(
        const float x0, const float y0,
        const float x1, const float y1,
        const float x2, const float y2,
        const float r0, const float r1) {
    return distance_between_point_and_line(x0, y0, x1, y1, x2, y2) - r0 - r1;
}
