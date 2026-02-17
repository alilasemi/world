#include "color_map.h"
#include "defines.h"
#include "dormand_prince.h"
#include "shader.h"
#include "simulation.h"
#include "particle_drawer.h"

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
    Simulation sim;
    DormandPrince<Simulation> integrator(sim, 0.01f);

    // Set up particle drawer
    ParticleDrawer drawer(sim.n, 20);
    drawer.draw(sim.xx, sim.xy);

    // Set up vertex buffer objects and configure vertex attributes
    unsigned VBO_nodes, VBO_color, VAO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO_nodes);
    glGenBuffers(1, &VBO_color);
    glGenBuffers(1, &EBO);
    // Must bind VAO before setting up VBOs and attribute pointers
    glBindVertexArray(VAO);

    // Vertex positions (the buffer data will be updated each frame)
    glBindBuffer(GL_ARRAY_BUFFER, VBO_nodes);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, static_cast<GLsizeiptr>(3 * sizeof(float)), static_cast<void*>(0));
    glEnableVertexAttribArray(0);

    // Vertex colors (the buffer data will be updated each frame)
    glBindBuffer(GL_ARRAY_BUFFER, VBO_color);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, static_cast<GLsizeiptr>(3 * sizeof(float)), static_cast<void*>(0));
    glEnableVertexAttribArray(1);

    // Element buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(drawer.triangles.size() * sizeof(int)),
            drawer.triangles.data(), GL_STATIC_DRAW);

    // Color map
    ColorMap color_map("assets/color_maps/magma.csv");

    // Render loop
    std::vector<float> colors(drawer.node_coords.size());
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

        // Update vertex positions
        glBindBuffer(GL_ARRAY_BUFFER, VBO_nodes);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(drawer.node_coords.size() * sizeof(float)), drawer.node_coords.data(), GL_STATIC_DRAW);

        // Update vertex colors
        std::fill(colors.begin(), colors.end(), 1.f); // Set all vertices to white for now
        glBindBuffer(GL_ARRAY_BUFFER, VBO_color);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(colors.size() * sizeof(float)), colors.data(), GL_DYNAMIC_DRAW);

        // Render background
        glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Render the fluid
        shader.use();
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(drawer.triangles.size() * sizeof(int)), GL_UNSIGNED_INT, 0);

        // GLFW: swap buffers and poll user inputs
        glfwSwapBuffers(window);
        glfwPollEvents();

        // Advance the simulation by one step
        integrator.take_step();
        sim.unpack_state();
        drawer.draw(sim.xx, sim.xy);
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
