#include "App.h"
#include <iostream>

struct PerSocketData {};

int main() {
    auto app = uWS::App().ws<PerSocketData>("/*", {
        .open = [](auto *ws) {
        },
        .message = [](auto *ws, std::string_view message, uWS::OpCode opCode) {
            std::cout << "Received message: " << message << std::endl;
            if (opCode == uWS::OpCode::TEXT) {
                if (message == "initialize") {
                    // Send size, needs to be stringview
                    int32_t size = 20;
                    const char* data = reinterpret_cast<const char*>(&size);
                    size_t length = sizeof(size);
                    ws->send(std::string_view(data, length), uWS::OpCode::BINARY);
                }
            } else {
//                // Send back an array of 6 floats in binary
//                float data[6] = {0.0f, 0.5f, -0.5f, 0.0f, 0.5f, -0.5f};
//                ws->send(data, sizeof(data), uWS::OpCode::BINARY);
            }
        }
    }).listen(8081, [](auto *listen_socket) {
        if (listen_socket) std::cout << "Listening on port 8081" << std::endl;
    });

    app.run();
}
