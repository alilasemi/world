#include "App.h"
#include <cstdlib>
#include <cstring>
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
        .message = [&sim, &config, &app](auto *ws, std::string_view message, uWS::OpCode opCode) {
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
                    // Send grid_size so the client can draw the spatial grid
                    int32_t grid_size = sim->grid_size;
                    const char* grid_size_data = reinterpret_cast<const char*>(&grid_size);
                    length = sizeof(grid_size);
                    ws->send(std::string_view(grid_size_data, length), uWS::OpCode::BINARY);
                    // Send force grid size so the client can render the RL force field
                    int32_t force_grid_size = sim->force_grid_size;
                    const char* fgs_data = reinterpret_cast<const char*>(&force_grid_size);
                    ws->send(std::string_view(fgs_data, sizeof(force_grid_size)), uWS::OpCode::BINARY);
                    // Send RL max force so the client can normalize arrow lengths
                    float rl_max_force = config.rl_max_force;
                    const char* rlmf_data = reinterpret_cast<const char*>(&rl_max_force);
                    ws->send(std::string_view(rlmf_data, sizeof(rl_max_force)), uWS::OpCode::BINARY);
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
                    float interp0 = sim->interpolate_force_wct();
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
                        std::cerr << "world: spatial grid overflowed particles_per_cell capacity in "
                                  << overflowed << " cell-step(s) (dt=" << sim->dt
                                  << "); simulation has diverged. Exiting." << std::endl;
                        std::exit(1);
                    }
                    float t_find  = sim->find_neighbors_wct()  - find0;
                    float t_interp = sim->interpolate_force_wct() - interp0;
                    float t_rhs   = sim->compute_rhs_wct()   - rhs0;
                    float t_step  = sim->take_step_wct()  - step0;
                    sim->unpack_state();
                    // Pack xy + metadata into one buffer.
                    // Layout: [n*2 xy floats | sim_time | real_time_ratio |
                    //          t_find_neighbors | t_interpolate_force |
                    //          t_compute_rhs | t_take_step | t_unpack_state]
                    // All times are in milliseconds.
                    const size_t n_xy = sim->xy.size();
                    std::vector<float> payload(n_xy + 7);
                    std::copy(sim->xy.begin(), sim->xy.end(), payload.begin());
                    payload[n_xy + 0] = sim->time;
                    payload[n_xy + 1] = sim->real_time_ratio;
                    payload[n_xy + 2] = t_find;
                    payload[n_xy + 3] = t_interp;
                    payload[n_xy + 4] = t_rhs;
                    payload[n_xy + 5] = t_step;
                    payload[n_xy + 6] = sim->time_unpack_state;
                    // Append current force grids so the client can visualize the
                    // RL agent's input. Zeros when no agent is connected.
                    const size_t m2 = (size_t)sim->force_grid_size * sim->force_grid_size;
                    HostVector<float> host_fx(m2), host_fy(m2);
                    host_fx.copy_from_device(sim->device_grid_force_x);
                    host_fy.copy_from_device(sim->device_grid_force_y);
                    payload.insert(payload.end(), host_fx.data(), host_fx.data() + m2);
                    payload.insert(payload.end(), host_fy.data(), host_fy.data() + m2);

                    const char* state_data = reinterpret_cast<const char*>(payload.data());
                    size_t length = payload.size() * sizeof(float);
                    std::string_view state_sv(state_data, length);
                    ws->send(state_sv, uWS::OpCode::BINARY);
                    // Push to any passive observers (e.g. browser watching RL training).
                    app.publish("state_updates", state_sv, uWS::OpCode::BINARY);
                } else if (message == "observe") {
                    // Register this socket as a passive observer: it will receive
                    // a state push after every "run" step without driving the sim.
                    ws->subscribe("state_updates");
                } else if (message == "get_occupancy") {
                    if (!sim) {
                        std::cout << "get_occupancy: sim not initialized" << std::endl;
                        return;
                    }
                    sim->update_occupancy_grid();
                    const size_t m2 = (size_t)sim->occupancy_grid_size * sim->occupancy_grid_size;
                    HostVector<int> host_occ(m2);
                    host_occ.copy_from_device(sim->device_occupancy_grid);
                    ws->send(std::string_view(
                        reinterpret_cast<const char*>(host_occ.data()),
                        m2 * sizeof(int)),
                        uWS::OpCode::BINARY);
                } else {
                    std::cout << "Unknown message: " << message << std::endl;
                }
            } else if (opCode == uWS::OpCode::BINARY) {
                // Force-grid upload from RL agent.
                // Payload: 2 * force_grid_size^2 floats, little-endian.
                // First half: force_x (row-major cell_x*m + cell_y).
                // Second half: force_y.
                if (!sim) return;
                const int m = sim->force_grid_size;
                const size_t m2 = (size_t)m * m;
                const size_t expected = 2 * m2 * sizeof(float);
                if (message.size() != expected) {
                    std::cout << "Force upload: unexpected size " << message.size()
                              << " (expected " << expected << ")" << std::endl;
                    return;
                }
                HostVector<float> host_fx(m2), host_fy(m2);
                std::memcpy(host_fx.data(), message.data(), m2 * sizeof(float));
                std::memcpy(host_fy.data(), message.data() + m2 * sizeof(float), m2 * sizeof(float));
                sim->device_grid_force_x.copy_from_host(host_fx);
                sim->device_grid_force_y.copy_from_host(host_fy);
            }
        }
    }).listen(config.port, [&config](auto *listen_socket) {
        if (listen_socket) std::cout << "Listening on port " << config.port << std::endl;
    });

    app.run();
}
