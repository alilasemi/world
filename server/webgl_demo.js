// Convert the above Shader class from C++ to JavaScript
class Shader {
    gl;
    program;

    constructor(gl, vertexPath, fragmentPath) {
        this.gl = gl;
        this.loadShader(vertexPath, fragmentPath);
    }

    loadShader(vertexPath, fragmentPath) {
        try {
//            const vertexCode = fs.readFileSync(vertexPath, 'utf8');
//            const fragmentCode = fs.readFileSync(fragmentPath, 'utf8');
            // TODO: Ran into some issues reading from files, so hardcoding shader code for now
            const vertexCode = `#version 300 es

                precision highp float;

                layout (location = 0) in vec3 aPos;
                layout (location = 1) in vec3 aColor;

                out vec3 ourColor;

                void main()
                {
                    gl_Position = vec4(aPos, 1.0f);
                    ourColor = aColor;
                }
            `;
            const fragmentCode = `#version 300 es

                precision highp float;

                out vec4 FragColor;

                in vec3 ourColor;

                void main()
                {
                    FragColor = vec4(ourColor, 1.0f);
                }
            `;

            const vertexShader = this.compileShader(vertexCode, this.gl.VERTEX_SHADER);
            const fragmentShader = this.compileShader(fragmentCode, this.gl.FRAGMENT_SHADER);

            this.createProgram(vertexShader, fragmentShader);
        } catch (error) {
            console.error("ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: ", error);
        }
    }

    compileShader(source, type) {
        const shader = this.gl.createShader(type);
        this.gl.shaderSource(shader, source);
        this.gl.compileShader(shader);

        if (!this.gl.getShaderParameter(shader, this.gl.COMPILE_STATUS)) {
            console.error("ERROR::SHADER_COMPILATION_ERROR: ", this.gl.getShaderInfoLog(shader));
            this.gl.deleteShader(shader);
            return null;
        }
        return shader;
    }

    createProgram(vertexShader, fragmentShader) {
        this.program = this.gl.createProgram();
        this.gl.attachShader(this.program, vertexShader);
        this.gl.attachShader(this.program, fragmentShader);
        this.gl.linkProgram(this.program);

        if (!this.gl.getProgramParameter(this.program, this.gl.LINK_STATUS)) {
            console.error("ERROR::PROGRAM_LINKING_ERROR: ", this.gl.getProgramInfoLog(this.program));
            this.gl.deleteProgram(this.program);
            return null;
        }
        this.gl.validateProgram(this.program);
        if (!this.gl.getProgramParameter(this.program, this.gl.VALIDATE_STATUS)) {
            console.warn('Program not guaranteed to execute with current state: ' + this.gl.getProgramInfoLog(this.program));
        } else {
            console.log("Shader program created successfully.");
        }
    }

    use() {
        this.gl.validateProgram(this.program);
        if (!this.gl.getProgramParameter(this.program, this.gl.VALIDATE_STATUS)) {
            console.warn('Program not guaranteed to execute with current state: ' + this.gl.getProgramInfoLog(this.program));
        }
        this.gl.useProgram(this.program);
    }
}


// Convert the above ParticleDrawer class from C++ to JavaScript
class ParticleDrawer {
    constructor(n, num_triangles) {
        this.n = n;
        this.num_triangles = num_triangles;
        this.radius = 0.05;
        this.node_coords = new Float32Array(n * (num_triangles + 1) * 3);
        this.triangles = new Int32Array(num_triangles * n * 3);
        this.create_triangle_connectivity();
    }

    create_triangle_connectivity() {
        for (let i = 0; i < this.n; ++i) {
            const start_index = i * (this.num_triangles + 1);
            const center_index = start_index + this.num_triangles;
            for (let i_tri = 0; i_tri < this.num_triangles - 1; ++i_tri) {
                this.triangles[i * this.num_triangles * 3 + i_tri * 3 + 0] = start_index + i_tri;
                this.triangles[i * this.num_triangles * 3 + i_tri * 3 + 1] = start_index + i_tri + 1;
                this.triangles[i * this.num_triangles * 3 + i_tri * 3 + 2] = center_index;
            }
            this.triangles[i * this.num_triangles * 3 + (this.num_triangles - 1) * 3 + 0] = start_index + this.num_triangles - 1;
            this.triangles[i * this.num_triangles * 3 + (this.num_triangles - 1) * 3 + 1] = start_index;
            this.triangles[i * this.num_triangles * 3 + (this.num_triangles - 1) * 3 + 2] = center_index;
        }
    }

    draw(x, y) {
        for (let i = 0; i < this.n; ++i) {
            const start_index = i * (this.num_triangles + 1);
            for (let i_tri = 0; i_tri < this.num_triangles; ++i_tri) {
                const angle = i_tri / this.num_triangles * 2.0 * Math.PI;
                this.node_coords[(start_index + i_tri) * 3 + 0] = x[i] + Math.cos(angle) * this.radius;
                this.node_coords[(start_index + i_tri) * 3 + 1] = y[i] + Math.sin(angle) * this.radius;
                this.node_coords[(start_index + i_tri) * 3 + 2] = 0.0;
            }
            this.node_coords[(start_index + this.num_triangles) * 3 + 0] = x[i];
            this.node_coords[(start_index + this.num_triangles) * 3 + 1] = y[i];
            this.node_coords[(start_index + this.num_triangles) * 3 + 2] = 0.0;
        }
    }
}



function main() {
    const canvas = document.querySelector("#gl-canvas");
    // Initialize the GL context
    const gl = canvas.getContext("webgl2");

    var websocketUri = "wss://echo.websocket.org/";
    const socket = new WebSocket(websocketUri);

    // Only continue if WebGL is available and working
    if (gl === null) {
        alert(
            "Unable to initialize WebGL. Your browser or machine may not support it.",
        );
        return;
    }

    // Create shader
    var shader = new Shader(gl, "vertex.glsl", "fragment.glsl");

    // Set up particled drawer
    var xx = [0.0, 0.5, -0.5];
    var xy = [0.0, 0.5, -0.5];
    const n = xx.length;
    var drawer = new ParticleDrawer(n, 20);
    drawer.draw(xx, xy);

    // Set clear color to black, fully opaque
    gl.clearColor(0.0, 0.0, 0.0, 1.0);
    // Clear the color buffer with specified clear color
    gl.clear(gl.COLOR_BUFFER_BIT);

    // Set up vertex buffer objects and configure vertex attributes
    const VBO_nodes = gl.createBuffer();
    const VBO_color = gl.createBuffer();
    const VAO = gl.createVertexArray();
    const EBO = gl.createBuffer();
    // Must bind VAO before setting up VBOs and attribute pointers
    gl.bindVertexArray(VAO);

    // Vertex positions (the buffer data will be updated each frame)
    gl.bindBuffer(gl.ARRAY_BUFFER, VBO_nodes);
    gl.vertexAttribPointer(0, 3, gl.FLOAT, false, 3 * Float32Array.BYTES_PER_ELEMENT, 0);
    gl.enableVertexAttribArray(0);

    // Vertex colors (the buffer data will be updated each frame)
    gl.bindBuffer(gl.ARRAY_BUFFER, VBO_color);
    gl.vertexAttribPointer(1, 3, gl.FLOAT, false, 3 * Float32Array.BYTES_PER_ELEMENT, 0);
    gl.enableVertexAttribArray(1);

    // Element buffer
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, EBO);
    gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, new Int32Array(drawer.triangles), gl.STATIC_DRAW);

    var colors = new Float32Array(drawer.node_coords.length).fill(1.0);

//    // Render loop
//    std::vector<float> colors(drawer.node_coords.size());
//    int frame_count = 0;
//    while (!glfwWindowShouldClose(window)) {
//        // Receive user input
//        processInput(window);
//
////        // Update vertex colors based on simulation density
////        for (int i = 0; i < N + 2; i++) {
////            for (int j = 0; j < N + 2; j++) {
////                float normalized_rho = sim.rho[IX(i,j)] / max_rho;
////                if (normalized_rho < 0.f) normalized_rho = 0.f;
////                if (normalized_rho > 1.f) normalized_rho = 1.f;
////                std::vector<float> rgb = color_map.get_color(normalized_rho);
////                colors[IX(i,j)*3]     = rgb[0]; // Red channel
////                colors[IX(i,j)*3 + 1] = rgb[1]; // Green channel
////                colors[IX(i,j)*3 + 2] = rgb[2]; // Blue channel
////            }
////        }
//
//        // Update vertex positions
//        glBindBuffer(GL_ARRAY_BUFFER, VBO_nodes);
//        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(drawer.node_coords.size() * sizeof(float)), drawer.node_coords.data(), GL_STATIC_DRAW);
//
//        // Update vertex colors
//        std::fill(colors.begin(), colors.end(), 1.f); // Set all vertices to white for now
//        glBindBuffer(GL_ARRAY_BUFFER, VBO_color);
//        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(colors.size() * sizeof(float)), colors.data(), GL_DYNAMIC_DRAW);
//
//        // Render background
//        glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
//        glClear(GL_COLOR_BUFFER_BIT);
//
//        // Render the fluid
//        shader.use();
//        glBindVertexArray(VAO);
//        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(drawer.triangles.size() * sizeof(int)), GL_UNSIGNED_INT, 0);
//
//        // GLFW: swap buffers and poll user inputs
//        glfwSwapBuffers(window);
//        glfwPollEvents();
//
//        // Advance the simulation
//        for (int steps = 0; steps < 2; ++steps) {
//            sim.take_step();
//        }
//        sim.unpack_state();
//        drawer.draw(sim.xx, sim.xy);
//        ++frame_count;
//    }

// Convert the above render loop from C++ to JavaScript
    // Render loop
    function render() {
        // Update vertex positions
        gl.bindBuffer(gl.ARRAY_BUFFER, VBO_nodes);
        gl.bufferData(gl.ARRAY_BUFFER, drawer.node_coords, gl.STATIC_DRAW);

        // Update vertex colors
        colors.fill(1.0); // Set all vertices to white for now
        gl.bindBuffer(gl.ARRAY_BUFFER, VBO_color);
        gl.bufferData(gl.ARRAY_BUFFER, colors, gl.DYNAMIC_DRAW);

        // Render background
        gl.clearColor(0.2, 0.2, 0.2, 1.0);
        gl.clear(gl.COLOR_BUFFER_BIT);

        // Render the particles
        shader.use();
        gl.bindVertexArray(VAO);
        gl.drawElements(gl.TRIANGLES, drawer.triangles.length, gl.UNSIGNED_INT, 0);

    }

    render();

    xx = xx.map(x => x + 0.5);
    xy = xy.map(y => y + 0.5);
    drawer.draw(xx, xy);
    render();



    // De-allocate and clean up
    gl.deleteVertexArray(VAO);
    gl.deleteBuffer(VBO_nodes);
    gl.deleteBuffer(VBO_color);
    gl.deleteBuffer(EBO);
}


main();
