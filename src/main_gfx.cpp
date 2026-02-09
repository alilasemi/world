#include "color_map.h"
#include "defines.h"
#include "shader.h"
#include "simulation.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <cmath>


void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);


int main() {
    // GLFW initialization
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // GLFW window creation
    const int screen_width = 1000;
    const int screen_height = 1000;
    GLFWwindow* window = glfwCreateWindow(screen_width, screen_height, "Real Time Particles", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // GLAD: load all OpenGL function pointers
    if (!gladLoadGLLoader((GLADloadproc)(glfwGetProcAddress))) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // Compile and link shaders
    Shader shader("src/vertex.glsl", "src/fragment.glsl");

    // Set up simulation
    const unsigned N = 200;
    const float visc = 1e-5f;
    const float diff = 1e-5f;
    const float dt = 0.001f;
    const unsigned n_iter = 100;
    Simulation sim(N, visc, diff, dt, n_iter);
    float max_rho = 10.f;
    sim.set_inflow_boundary_condition(-0.2f, 0.2f, InflowBoundaryCondition::Position::TOP,
            max_rho, 0.f);
    sim.initialize_to_stagnant_fluid(1.f);
    //sim.initialize_to_two_stagnant_fluids(1.f, 10.f);

    // Set up vertex buffer objects and configure vertex attributes
    unsigned VBO_nodes, VBO_color, VAO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO_nodes);
    glGenBuffers(1, &VBO_color);
    glGenBuffers(1, &EBO);
    // Must bind VAO before setting up VBOs and attribute pointers
    glBindVertexArray(VAO);

    // An octagon centered around 0, 0, with radius of .5
    float r = .5;
    std::vector<float> node_coords = {
        r, 0, 0,
        r / std::sqrt(2.f), r / std::sqrt(2.f), 0,
        0, r, 0,
        -r / std::sqrt(2.f), r / std::sqrt(2.f), 0,
        -r, 0, 0,
        -r / std::sqrt(2.f), -r / std::sqrt(2.f), 0,
        0, -r, 0,
        r / std::sqrt(2.f), -r / std::sqrt(2.f), 0,
        0, 0, 0
    };
    std::vector<int> triangles = {
        0, 1, 8,
        1, 2, 8,
        2, 3, 8,
        3, 4, 8,
        4, 5, 8,
        5, 6, 8,
        6, 7, 8,
        7, 0, 8
    };


    // Vertex positions
    glBindBuffer(GL_ARRAY_BUFFER, VBO_nodes);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(node_coords.size() * sizeof(float)), node_coords.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, static_cast<GLsizeiptr>(3 * sizeof(float)), static_cast<void*>(0));
    glEnableVertexAttribArray(0);

    // Vertex colors (the buffer data will be updated each frame)
    glBindBuffer(GL_ARRAY_BUFFER, VBO_color);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, static_cast<GLsizeiptr>(3 * sizeof(float)), static_cast<void*>(0));
    glEnableVertexAttribArray(1);

    // Element buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(triangles.size() * sizeof(int)),
            triangles.data(), GL_STATIC_DRAW);

    // Color map
    ColorMap color_map("assets/color_maps/magma.csv");

    // Render loop
    std::vector<float> colors(node_coords.size());
    int frame_count = 0;
    while (!glfwWindowShouldClose(window)) {
        // Receive user input
        processInput(window);

//        // Update vertex colors based on simulation density
//        for (int i = 0; i < N + 2; i++) {
//            for (int j = 0; j < N + 2; j++) {
//                float normalized_rho = sim.rho[IX(i,j)] / max_rho;
//                if (normalized_rho < 0.f) normalized_rho = 0.f;
//                if (normalized_rho > 1.f) normalized_rho = 1.f;
//                std::vector<float> rgb = color_map.get_color(normalized_rho);
//                colors[IX(i,j)*3]     = rgb[0]; // Red channel
//                colors[IX(i,j)*3 + 1] = rgb[1]; // Green channel
//                colors[IX(i,j)*3 + 2] = rgb[2]; // Blue channel
//            }
//        }
        colors = {
            1.f, 0.f, 0.f,
            1.f, 0.5f, 0.f,
            1.f, 1.f, 0.f,
            0.5f, 1.f, 0.f,
            0.f, 1.f, 0.f,
            0.f, 1.f, 0.5f,
            0.f, 1.f, 1.f,
            0.5f, 0.5f, 1.f,
            1.f, 0.5f, 1.f
        };
        glBindBuffer(GL_ARRAY_BUFFER, VBO_color);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(colors.size() * sizeof(float)), colors.data(), GL_DYNAMIC_DRAW);

        // Render background
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Render the fluid
        shader.use();
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(sim.triangles.size() * sizeof(int)), GL_UNSIGNED_INT, 0);

        // GLFW: swap buffers and poll user inputs
        glfwSwapBuffers(window);
        glfwPollEvents();

        // Advance the simulation by one step
        sim.take_step();
        ++frame_count;
    }

    // De-allocate and clean up
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO_nodes);
    glDeleteBuffers(1, &VBO_color);
    glDeleteBuffers(1, &EBO);
    glfwTerminate();

    return 0;
}

// Process user inputs
void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
}

// GLWF callback for window resizing
void framebuffer_size_callback([[maybe_unused]] GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}
