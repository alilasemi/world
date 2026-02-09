#include "simulation.h"
#include "defines.h"
#include "initial_condition.h"

#include <stdio.h>
#include <stdlib.h>
#include <string>


Simulation::Simulation(const unsigned N_, const float visc_, const float diff_, const float dt_, const unsigned n_iter_)
    : N(N_), visc(visc_), diff(diff_), dt(dt_), n_iter(n_iter_), size((N + 2) * (N + 2)) {
    u = Vector(size);
    v = Vector(size);
    rho = Vector(size);

    generate_mesh();

    // By default, initialize to two stagnant fluids
    initialize_to_two_stagnant_fluids(1.0, 10.0);

    rho_prev = rho;
    u_prev = u;
    v_prev = v;

    // By default, apply no-slip wall boundary conditions
    boundary_conditions.push_back(std::make_unique<NoSlipWallBoundaryCondition>(N));
}

void Simulation::generate_mesh() {
    // Generate a grid of (N+2) x (N+2) nodes in the xy-plane, ranging from -1 to 1 in both x and y
    const unsigned nx = N + 2;
    const unsigned ny = N + 2;
    node_coords = std::vector<float>(nx*ny*3);
    for (unsigned i = 0; i < nx; i++) {
        for (unsigned j = 0; j < ny; j++) {
            node_coords[3*IX(i,j)]     = 2*static_cast<float>(i)/static_cast<float>(nx - 1) - 1.f;
            node_coords[3*IX(i,j) + 1] = 2*static_cast<float>(j)/static_cast<float>(ny - 1) - 1.f;
            node_coords[3*IX(i,j) + 2] = 0.0f;
        }
    }

    // Generate the connectivity for the triangles connecting them. The actual
    // fluid simulation algorithm does not operate on the triangles, but
    // triangles are helpful for rendering.
    triangles = std::vector<unsigned>(2*(nx - 1)*(ny - 1) * 3);
    unsigned triangle_index = 0;
    for (unsigned j = 0; j < ny - 1; j++) {
        for (unsigned i = 0; i < nx - 1; i++) {
            // First triangle
            triangles[3*triangle_index]     = IX(i, j);
            triangles[3*triangle_index + 1] = IX(i + 1, j + 1);
            triangles[3*triangle_index + 2] = IX(i, j + 1);
            ++triangle_index;
            // Second triangle
            triangles[3*triangle_index]     = IX(i, j);
            triangles[3*triangle_index + 1] = IX(i + 1, j);
            triangles[3*triangle_index + 2] = IX(i + 1, j + 1);
            ++triangle_index;
        }
    }
}

void Simulation::write_to_file(const unsigned iter, Vector data, std::string filename_prefix) {
     std::string filename = "results/" + filename_prefix + "_" + std::to_string(iter) + ".txt";
     FILE *file = fopen(filename.c_str(), "w");

     if (file == NULL) {
         perror("Error opening file");
         return;
     }
     for (unsigned i = 0; i < (N + 2); ++i) {
         for (unsigned j = 0; j < (N + 2); ++j) {
             fprintf(file, "%f ", static_cast<double>(data[IX(i, j)]) );
         }
         fprintf(file, "\n");
     }
     fclose(file);
}

void Simulation::run() {
    for (unsigned iter = 0; iter < n_iter; ++iter) {
        write_to_file(iter, rho, "density");
        write_to_file(iter, u, "u");
        write_to_file(iter, v, "v");
        take_step();
    }
    write_to_file(n_iter, rho, "density");
    write_to_file(n_iter, u, "u");
    write_to_file(n_iter, v, "v");
}

void Simulation::take_step() {
    //    get_from_UI ( rho_prev, u_prev, v_prev );
    //    the code assumes that rho_prev, u_prev, and v_prev (later called x0,
    //    u0, v0) are actually source terms, not rho/u/v. This is some c
    //    programmer weirdness, trying to minimize memory usage by reusing
    //    arrays. This will need to be refactored to be more clear, at least.
    //    For now, set density source to zero and velocity source to
    //    gravitational acceleration in the negative y direction.
    const float g = 9.81f;
    for (unsigned i = 0; i < size; i++) {
        rho_prev[i] = 0.0f;
        u_prev[i] = 0.0f;
        v_prev[i] = -rho[i] * g;
    }
    velocity_step();
    density_step();
}

void Simulation::add_source(Vector& x, const Vector& s) {
    for (unsigned i = 0; i < size; i++) x[i] += dt*s[i];
}


void Simulation::diffuse(StateVariable var, Vector& x, const Vector& x0) {
    float a = static_cast<float>(dt) * diff * static_cast<float>(N * N);
    for (unsigned k = 0; k < 20; k++) {
        for (unsigned i = 1 ; i <= N; i++) {
            for (unsigned j = 1 ; j <= N; j++) {
                x[IX(i,j)] = (x0[IX(i,j)] + a*(x[IX(i-1,j)] + x[IX(i+1,j)] + x[IX(i,j-1)] + x[IX(i,j+1)]))/(1+4*a);
            }
        }
        set_bnd(var, x);
    }
}

void Simulation::advect(StateVariable var, Vector& d, const Vector& d0, const Vector& u, const Vector& v) {
    int i0, j0, i1, j1;
    float x, y, s0, t0, s1, t1, dt0;
    dt0 = dt * static_cast<float>(N);
    for (unsigned i = 1 ; i <= N; ++i ) {
        for (unsigned j = 1 ; j <= N; ++j ) {
            x = static_cast<float>(i) - dt0*u[IX(i,j)];
            y = static_cast<float>(j) - dt0*v[IX(i,j)];

            if (x < 0.5f) x = 0.5f;
            if (x > static_cast<float>(N) + 0.5f) x = static_cast<float>(N) + 0.5f;
            i0 = static_cast<int>(x);
            i1 = i0 + 1;

            if (y < 0.5f) y = 0.5f;
            if (y > static_cast<float>(N) + 0.5f) y = static_cast<float>(N) + 0.5f;
            j0 = static_cast<int>(y);
            j1 = j0 + 1;

            s1 = x - static_cast<float>(i0);
            s0 = 1.f - s1;
            t1 = y - static_cast<float>(j0);
            t0 = 1.f - t1;
            d[IX(i,j)] = s0*(t0*d0[IX(i0,j0)] + t1*d0[IX(i0,j1)]) + s1*(t0*d0[IX(i1,j0)] + t1*d0[IX(i1,j1)]);
        }
    }
    set_bnd(var, d);
}

void Simulation::density_step() {
    add_source(rho, rho_prev);
    std::swap(rho_prev, rho);
    diffuse(RHO, rho, rho_prev);
    std::swap(rho_prev, rho);
    advect(RHO, rho, rho_prev, u, v);
}

void Simulation::velocity_step() {
    add_source(u, u_prev);
    add_source(v, v_prev);
    std::swap(u_prev, u);
    std::swap(v_prev, v);
    diffuse(U, u, u_prev);
    diffuse(V, v, v_prev);
    project(u, v, u_prev, v_prev);
    std::swap(u_prev, u);
    std::swap(v_prev, v);
    advect(U, u, u_prev, u_prev, v_prev);
    advect(V, v, v_prev, u_prev, v_prev);
    project(u, v, u_prev, v_prev);
}

void Simulation::project(Vector& u, Vector& v, Vector& p, Vector& div) {
    float h = 1.f / static_cast<float>(N);
    for (unsigned i = 1; i <= N; i++) {
        for (unsigned j = 1; j <= N; j++) {
            div[IX(i,j)] = -0.5f*h*(u[IX(i+1,j)] - u[IX(i-1,j)] + v[IX(i,j+1)] - v[IX(i,j-1)]);
            p[IX(i,j)] = 0;
        }
    }
    set_bnd(DIV, div);
    set_bnd(P, p);

    for (unsigned k = 0; k < 20; k++) {
        for (unsigned i = 1; i <= N; i++) {
            for (unsigned j = 1; j <= N; j++) {
                p[IX(i,j)] = (div[IX(i,j)] + p[IX(i-1,j)] + p[IX(i+1,j)] + p[IX(i,j-1)] + p[IX(i,j+1)])/4;
            }
        }
        set_bnd(P, p);
    }
    for (unsigned i = 1; i <= N; i++) {
        for (unsigned j = 1; j <= N; j++) {
            u[IX(i,j)] -= 0.5f*(p[IX(i+1,j)] - p[IX(i-1,j)])/h;
            v[IX(i,j)] -= 0.5f*(p[IX(i,j+1)] - p[IX(i,j-1)])/h;
        }
    }
    set_bnd(U, u);
    set_bnd(V, v);
}

void Simulation::initialize_to_stagnant_fluid(const float rho_0) {
    InitialCondition initial_condition(N, rho, u, v);
    initial_condition.initialize_to_stagnant_fluid(rho_0);
}

void Simulation::initialize_to_two_stagnant_fluids(const float rho_0, const float rho_1) {
    InitialCondition initial_condition(N, rho, u, v);
    initial_condition.initialize_to_two_stagnant_fluids(rho_0, rho_1);
}

void Simulation::set_inflow_boundary_condition(const float x_left, const float x_right,
        InflowBoundaryCondition::Position position, const float inflow_density, const float inflow_velocity) {
    boundary_conditions.push_back(std::make_unique<InflowBoundaryCondition>(
            N, node_coords, x_left, x_right, position, inflow_density, inflow_velocity));
}

void Simulation::set_bnd(StateVariable var, Vector& x) {
    // Apply all boundary conditions. Here, the order does matter - later
    // boundary conditions can overwrite earlier ones.
    for (auto& bc : boundary_conditions) {
        bc->apply_boundary_condition(var, x);
    }

    // For the corners, set to the average of the two adjacent cells
    x[IX(0, 0)]     = 0.5f*(x[IX(1, 0)]   + x[IX(0, 1)]);
    x[IX(0, N+1)]   = 0.5f*(x[IX(1, N+1)] + x[IX(0, N)]);
    x[IX(N+1, 0)]   = 0.5f*(x[IX(N, 0)]   + x[IX(N+1, 1)]);
    x[IX(N+1, N+1)] = 0.5f*(x[IX(N, N+1)] + x[IX(N+1, N)]);
}
