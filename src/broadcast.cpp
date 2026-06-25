#include "App.h"
#include <iostream>
using std::unique_ptr;

#include "particle_dynamics_cuda.h"

struct PerSocketData {};

int main() {

    // Set up simulation
    using SimType = ParticleDynamicsCUDA;
    unique_ptr<SimType> sim;

    auto app = uWS::App();
    app.ws<PerSocketData>("/*", {
        .open = [](auto *ws) {
        },
        .message = [&sim](auto *ws, std::string_view message, uWS::OpCode opCode) {
            std::cout << "Received message: " << message << std::endl;
            if (opCode == uWS::OpCode::TEXT) {
                if (message == "initialize") {
                    sim.reset();
                    sim = std::make_unique<SimType>();
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
                    for (int steps = 0; steps < 100; ++steps) {
                        sim->take_step();
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
                    const char* state_data = reinterpret_cast<const char*>(payload.data());
                    size_t length = payload.size() * sizeof(float);
                    ws->send(std::string_view(state_data, length), uWS::OpCode::BINARY);
                } else {
                    std::cout << "Unknown message: " << message << std::endl;
                }
            }
        }
    }).listen(8081, [](auto *listen_socket) {
        if (listen_socket) std::cout << "Listening on port 8081" << std::endl;
    });

    app.run();
}
