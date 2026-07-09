#include "App.h"
#include <cstdlib>
#include <iostream>
#include <string>
using std::unique_ptr;

#include "particle_dynamics.h"
#include "sim_config.h"

struct PerSocketData {};

int main(int argc, char** argv) {

    // Load configuration (optional path as argv[1], default "config.yaml").
    const std::string config_path = (argc > 1) ? argv[1] : "config.yaml";
    const SimConfig config = load_config(config_path);

    // Set up simulation
    using SimType = ParticleDynamics;
    unique_ptr<SimType> sim;

    auto app = uWS::App();
    app.ws<PerSocketData>("/*", {
        .open = [](auto *ws) {
        },
        .message = [&sim, &config](auto *ws, std::string_view message, uWS::OpCode opCode) {
            std::cout << "Received message: " << message << std::endl;
            if (opCode == uWS::OpCode::TEXT) {
                if (message == "initialize") {
                    sim.reset();
                    sim = std::make_unique<SimType>(config);
                    // Send size, needs to be stringview
                    int32_t size = sim->n;
                    const char* size_data = reinterpret_cast<const char*>(&size);
                    size_t length = sizeof(size);
                    ws->send(std::string_view(size_data, length), uWS::OpCode::BINARY);
                    // Send collision grid size (x then y, one message) so the
                    // client can draw the collision-grid overlay.
                    int32_t collision_grid_size[2] = {sim->collision_grid_size_x, sim->collision_grid_size_y};
                    ws->send(std::string_view(reinterpret_cast<const char*>(collision_grid_size),
                            sizeof(collision_grid_size)), uWS::OpCode::BINARY);
                    // Send rendering constants (only at initialize, never per frame):
                    // number of triangles per particle, then the particle render
                    // radius (which reuses the simulation's particle_radius).
                    int32_t num_triangles = sim->config.num_triangles;
                    const char* num_triangles_data = reinterpret_cast<const char*>(&num_triangles);
                    length = sizeof(num_triangles);
                    ws->send(std::string_view(num_triangles_data, length), uWS::OpCode::BINARY);
                    float particle_radius = sim->config.physics.particle_radius;
                    const char* radius_data = reinterpret_cast<const char*>(&particle_radius);
                    length = sizeof(particle_radius);
                    ws->send(std::string_view(radius_data, length), uWS::OpCode::BINARY);
                    // Send domain bounds so the client can count how many particles
                    // are currently inside vs. the total (same bounds is_stable()
                    // checks server-side). One message, 4 floats: x_min/x_max/y_min/y_max.
                    const float domain_bounds[4] = {
                        sim->config.domain.x_min, sim->config.domain.x_max,
                        sim->config.domain.y_min, sim->config.domain.y_max,
                    };
                    ws->send(std::string_view(reinterpret_cast<const char*>(domain_bounds),
                            sizeof(domain_bounds)), uWS::OpCode::BINARY);
                    // Send state vector, sim.xy, which is vector<float>
                    const char* state_data = reinterpret_cast<const char*>(sim->xy.data());
                    length = sim->xy.size() * sizeof(float);
                    ws->send(std::string_view(state_data, length), uWS::OpCode::BINARY);
                } else if (message == "run") {
                    // Kernel::wall_clock_time() is a lifetime accumulator, so
                    // snapshot before and after to get this frame's total.
                    float find0  = sim->find_neighbors_wct();
                    float rhs0   = sim->compute_rhs_wct();
                    float step0  = sim->take_step_wct();
                    for (int steps = 0; steps < config.steps_per_frame; ++steps) {
                        sim->take_step();
                    }
                    // Checked once per steps_per_frame batch (i.e. once per
                    // "run" message) rather than every step -- a host sync
                    // that often would serialize what would otherwise be
                    // async-queued kernel launches. See
                    // stability_and_accuracy.cpp's check_grid_overflow() for
                    // the same idea applied to the sweep driver.
                    const int overflowed = sim->grid_overflow_count();
                    if (overflowed > 0) {
                        std::cerr << "world: collision grid overflowed particles_per_cell capacity in "
                                  << overflowed << " cell-step(s) (dt=" << sim->dt
                                  << "); simulation has diverged. Exiting." << std::endl;
                        std::exit(1);
                    }
                    float t_find  = sim->find_neighbors_wct()  - find0;
                    float t_rhs   = sim->compute_rhs_wct()   - rhs0;
                    float t_step  = sim->take_step_wct()  - step0;
                    sim->unpack_state();
                    // Pack xy + metadata into one buffer.
                    // Layout: [n*2 xy floats | sim_time | real_time_ratio |
                    //          t_find_neighbors | t_compute_rhs | t_take_step |
                    //          t_unpack_state]
                    // All times are in milliseconds.
                    const size_t n_xy = sim->xy.size();
                    std::vector<float> payload(n_xy + 6);
                    std::copy(sim->xy.begin(), sim->xy.end(), payload.begin());
                    payload[n_xy + 0] = sim->time;
                    payload[n_xy + 1] = sim->real_time_ratio;
                    payload[n_xy + 2] = t_find;
                    payload[n_xy + 3] = t_rhs;
                    payload[n_xy + 4] = t_step;
                    payload[n_xy + 5] = sim->time_unpack_state;

                    const char* state_data = reinterpret_cast<const char*>(payload.data());
                    size_t length = payload.size() * sizeof(float);
                    std::string_view state_sv(state_data, length);
                    ws->send(state_sv, uWS::OpCode::BINARY);
                } else {
                    std::cout << "Unknown message: " << message << std::endl;
                }
            }
        }
    }).listen(config.port, [&config](auto *listen_socket) {
        if (listen_socket) std::cout << "Listening on port " << config.port << std::endl;
    });

    app.run();
}
