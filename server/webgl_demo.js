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


class Client {
    socket;
    initialized;

    constructor() {
        // Connect to the websocket server
        var websocketUri = 'ws://34.125.228.87:8081/';
        const socket = new WebSocket(websocketUri);
        socket.binaryType = "arraybuffer";

        // Log messages from server
        socket.onmessage = function(event) {
            if (!this.initialized) {
                console.log('Initializing from server:', event.data);
                // The data is just an int passed as binary, read it into an int
                const dataView = new DataView(event.data);
                const n = dataView.getInt32(0, true);
                console.log('Received n from server: ', n);
            }
        };

        // Log errors
        socket.onerror = function(error) {
            console.error('WebSocket Error:', error);
        };

        // Log open connection
        socket.onopen = function(event) {
            console.log('Connected to ws://localhost:9001');
            // Optional: Send a test message
            socket.send('initialize');
        };
        this.socket = socket;
    }
}


function main() {
    const client = new Client();

    const canvas = document.querySelector("#gl-canvas");
    // Initialize the GL context
    const gl = canvas.getContext("webgl2");
    // Only continue if WebGL is available and working
    if (gl === null) {
        alert(
            "Unable to initialize WebGL. Your browser or machine may not support it.",
        );
        return;
    }

    // Create shader
    var shader = new Shader(gl, "vertex.glsl", "fragment.glsl");

    // Set up particle drawer
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
