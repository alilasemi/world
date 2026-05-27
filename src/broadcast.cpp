#include "App.h"
#include <iostream>

#include "particle_dynamics_cuda.h"
#include "particle_dynamics.h"

struct PerSocketData {};

int main() {

    // Set up simulation
    using SimType = ParticleDynamicsCUDA;
    SimType* sim;

    auto app = uWS::App();
    app.ws<PerSocketData>("/*", {
        .open = [](auto *ws) {
        },
        .message = [&sim](auto *ws, std::string_view message, uWS::OpCode opCode) {
            std::cout << "Received message: " << message << std::endl;
            if (opCode == uWS::OpCode::TEXT) {
                if (message == "initialize") {
                    if (sim) delete sim;
                    sim = new SimType();
                    // Send size, needs to be stringview
                    int32_t size = sim->n;
                    const char* size_data = reinterpret_cast<const char*>(&size);
                    size_t length = sizeof(size);
                    ws->send(std::string_view(size_data, length), uWS::OpCode::BINARY);
                    // Send state vector, sim.xy, which is vector<float>
                    const char* state_data = reinterpret_cast<const char*>(sim->xy.data());
                    length = sim->xy.size() * sizeof(float);
                    ws->send(std::string_view(state_data, length), uWS::OpCode::BINARY);
                } else if (message == "run") {
                    // Run sim and send state
                    for (int steps = 0; steps < 10; ++steps) {
                        sim->take_step();
                    }
                    sim->unpack_state();
                    const char* state_data = reinterpret_cast<const char*>(sim->xy.data());
                    size_t length = sim->xy.size() * sizeof(float);
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
