#include <stdio.h>
#include <cmath>
#include <vector>




//void ParticleDynamics::initialize_to_cube(const float x0, const float y0) {
//    size_t num_per_side = 10;
//    resize(num_per_side * num_per_side + 1);
//
//    for (size_t i = 0; i < num_per_side; ++i) {
//        for (size_t j = 0; j < num_per_side; ++j) {
//            state[4 * (i * num_per_side + j) + 0] = x0 + i * 0.1f;
//            state[4 * (i * num_per_side + j) + 1] = y0 + j * 0.1f;
//            state[4 * (i * num_per_side + j) + 2] = 0.0f;
//            state[4 * (i * num_per_side + j) + 3] = 0.0f;
//        }
//    }
//
//    // Make one particle of sled on top
//    state[4 * (n - 1) + 0] = -.9;
//    state[4 * (n - 1) + 1] = .9;
//    state[4 * (n - 1) + 2] = .4;
//    state[4 * (n - 1) + 3] = 0;
//
//    // Set all particles to be material 1 (snow) except for the last one
//    material = std::vector<size_t>(n, 1);
//    material[n - 1] = 2;
//
//    unpack_state();
//    update_grid();
//}
//
//
//void ParticleDynamics::update_grid() {
//    int grid_size = static_cast<int>(grid.size());
//    // The domain is an [-1, 1] x [-1, 1] square, and the grid is grid_size x grid_size, so each cell is 2 / grid_size wide and tall
//    // I will assign particles to grid cells based on their positions, and store the indices of the particles in each cell
//    for (size_t i = 0; i < grid_size; ++i) {
//        for (size_t j = 0; j < grid_size; ++j) {
//            grid[i][j].clear();
//        }
//    }
//    for (size_t i = 0; i < n; ++i) {
//        int cell_x = std::min(grid_size - 1, std::max(0, static_cast<int>((state[4 * i + 0] + 1.0f) / 2.0f * grid_size)));
//        int cell_y = std::min(grid_size - 1, std::max(0, static_cast<int>((state[4 * i + 1] + 1.0f) / 2.0f * grid_size)));
//        grid[cell_x][cell_y].push_back(i);
//    }
//}



class ParticleDynamicsCUDA {
public:
    float time;
    int n;
    float dt;

    std::vector<float> xx;
    std::vector<float> xy;
    float* state;
    float* rhs;
    int* material;

    float* mass;


    ParticleDynamicsCUDA() {
        initialize_to_two_particles(0.0f, 0.0f);
        time = 0.0f;
        // I will assign material 0 to be the walls, material 1 to be the snow, and
        // material 2 to be the sled
        mass = (float*)malloc(3 * sizeof(float));

        mass[0] = 0.0f;
        mass[1] = 1.0f;
        mass[2] = 10.0f;

        dt = 0.001f;

    //    size_t grid_size = 16;
    //    grid = std::vector<std::vector<std::vector<size_t>>>(grid_size, std::vector<std::vector<size_t>>(grid_size));
    }


    void resize(const int new_n) {
        n = new_n;
        xx = std::vector<float>(n);
        xy = std::vector<float>(n);
        state = (float*)malloc(4 * n * sizeof(float));
        rhs = (float*)malloc(4 * n * sizeof(float));
        material = (int*)malloc(n * sizeof(int));
    }


    void unpack_state() {
        printf("Unpacking state for %d particles...\n", n);
        for (size_t i = 0; i < n; ++i) {
            printf("Particle %zu: state = (%.2f, %.2f, %.2f, %.2f)\n", i, state[4 * i + 0], state[4 * i + 1], state[4 * i + 2], state[4 * i + 3]);
            xx[0] = 1;
            printf("hello\n");
            xx[i] = state[4 * i + 0];
            xy[i] = state[4 * i + 1];
        }
        printf("Unpacked state for %d particles.\n", n);
    }


    void initialize_to_two_particles(const float x0, const float y0) {
        printf("Initializing to two particles at (%.2f, %.2f) and (%.2f, %.2f)...\n", x0, y0, x0, y0 + 0.1f);
        resize(2);

        printf("Resized to %d particles.\n", n);
        state[0] = x0;
        state[1] = y0;
        state[2] = 0.0f;
        state[3] = 0.0f;

        printf("Initialized first particle at (%.2f, %.2f).\n", state[0], state[1]);
        state[4] = x0;
        state[5] = y0 + 0.1f;
        state[6] = 0.0f;
        state[7] = 0.0f;

        material[0] = 1;
        material[1] = 1;

        printf("Initialized second particle at (%.2f, %.2f).\n", state[4], state[5]);
        unpack_state();
    }


    void compute_rhs() {
    //    update_grid();

        const float g = 9.81f;
        float floor_y = -1.0f;
        float wall_x = -1.0f;
        float max_force = 1000.f;
        float r0 = 0.05f;
        float r1 = 0.1f;
        float force;
        for (size_t i = 0; i < n; ++i) {
            const float x = state[4 * i + 0];
            const float y = state[4 * i + 1];
            const float vx = state[4 * i + 2];
            const float vy = state[4 * i + 3];
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
//            // Wall on the left (at -1)
//            force = fmax(0, fmin(max_force, max_force / (r0 - r1) * (x - wall_x - r1)));
//            force_x += force; // Repulsive force
//            force_x -= force * vx; // Damping based on velocity


    //        int grid_size = static_cast<int>(grid.size());
    //        int cell_x = std::min(grid_size - 1, std::max(0, static_cast<int>((u[4 * i + 0] + 1.0f) / 2.0f * grid_size)));
    //        int cell_y = std::min(grid_size - 1, std::max(0, static_cast<int>((u[4 * i + 1] + 1.0f) / 2.0f * grid_size)));
    //        for (int dx = -1; dx <= 1; ++dx) {
    //            for (int dy = -1; dy <= 1; ++dy) {
    //                int neighbor_cell_x = cell_x + dx;
    //                int neighbor_cell_y = cell_y + dy;
    //                if (neighbor_cell_x >= 0 && neighbor_cell_x < grid_size && neighbor_cell_y >= 0 && neighbor_cell_y < grid_size) {
    //                    for (size_t j : grid[neighbor_cell_x][neighbor_cell_y]) {
    //                        if (i != j) {
    //                            const float x_j = u[4 * j + 0];
    //                            const float y_j = u[4 * j + 1];
    //                            const float vx_j = u[4 * j + 2];
    //                            const float vy_j = u[4 * j + 3];
    //                            const float dx = x - x_j;
    //                            const float dy = y - y_j;
    //                            const float dist = sqrtf(dx * dx + dy * dy);
    //                            const size_t mat_j = material[j];
    //                            // Repulsive force
    //                            force = fmaxf(0.f, fminf(max_force, max_force / (r0 - r1) * (dist - r1)));
    //                            force_y += force * (dy / dist);
    //                            force_x += force * (dx / dist);
    //                            // Damping based on relative velocity
    //                            force_x -= force * (vx - vx_j);
    //                            force_y -= force * (vy - vy_j);
    //                        }
    //                    }
    //                }
    //            }
    //        }

    //        // Particle collisions, without using the grid (O(n^2))
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

    void take_step() {
        compute_rhs();

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
};


//int main() {
//    float time;
//    float* mass;
//    ParticleDynamicsCUDA system;
//
//
//    printf("Initializing to two particles...\n");
////    initialize_to_two_particles(0.0f, 0.0f, n, &xx, &xy,
////            &state, &rhs, &material);
//
//    take_step(rhs, state, time, mass, material, n, dt);
//
//    free(xx);
//    free(xy);
//    free(state);
//    free(material);
//    free(mass);
//    return 0;
//}
