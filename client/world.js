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
        this.radius = 0.01;
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

    draw(xy) {
        for (let i = 0; i < this.n; ++i) {
            const start_index = i * (this.num_triangles + 1);
            for (let i_tri = 0; i_tri < this.num_triangles; ++i_tri) {
                const angle = i_tri / this.num_triangles * 2.0 * Math.PI;
                this.node_coords[(start_index + i_tri) * 3 + 0] = xy[2*i + 0] + Math.cos(angle) * this.radius;
                this.node_coords[(start_index + i_tri) * 3 + 1] = xy[2*i + 1] + Math.sin(angle) * this.radius;
                this.node_coords[(start_index + i_tri) * 3 + 2] = 0.0;
            }
            this.node_coords[(start_index + this.num_triangles) * 3 + 0] = xy[2*i + 0];
            this.node_coords[(start_index + this.num_triangles) * 3 + 1] = xy[2*i + 1];
            this.node_coords[(start_index + this.num_triangles) * 3 + 2] = 0.0;
        }
    }
}


class Graphics {
    gl;
    VBO_nodes;
    VBO_color;
    VAO;
    EBO;
    drawer;
    shader;
    colors;

    constructor(drawer) {
        this.drawer = drawer;
        const canvas = document.querySelector("#gl-canvas");
        // Initialize the GL context
        const gl = canvas.getContext("webgl2");
        this.gl = gl;
        // Only continue if WebGL is available and working
        if (gl === null) {
            alert(
                "Unable to initialize WebGL. Your browser or machine may not support it.",
            );
            return;
        }

        // Create shader
        this.shader = new Shader(gl, "vertex.glsl", "fragment.glsl");

        // Set clear color to black, fully opaque
        gl.clearColor(0.0, 0.0, 0.0, 1.0);
        // Clear the color buffer with specified clear color
        gl.clear(gl.COLOR_BUFFER_BIT);

        // Set up vertex buffer objects and configure vertex attributes
        this.VBO_nodes = gl.createBuffer();
        this.VBO_color = gl.createBuffer();
        this.VAO = gl.createVertexArray();
        this.EBO = gl.createBuffer();
        // Must bind VAO before setting up VBOs and attribute pointers
        gl.bindVertexArray(this.VAO);

        // Vertex positions (the buffer data will be updated each frame)
        gl.bindBuffer(gl.ARRAY_BUFFER, this.VBO_nodes);
        gl.vertexAttribPointer(0, 3, gl.FLOAT, false, 3 * Float32Array.BYTES_PER_ELEMENT, 0);
        gl.enableVertexAttribArray(0);

        // Vertex colors (the buffer data will be updated each frame)
        gl.bindBuffer(gl.ARRAY_BUFFER, this.VBO_color);
        gl.vertexAttribPointer(1, 3, gl.FLOAT, false, 3 * Float32Array.BYTES_PER_ELEMENT, 0);
        gl.enableVertexAttribArray(1);

        // Element buffer
        gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, this.EBO);
        gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, new Int32Array(this.drawer.triangles), gl.STATIC_DRAW);

        this.colors = new Float32Array(this.drawer.node_coords.length).fill(1.0);
        // Render loop

//        render();
//
//        xx = xx.map(x => x + 0.5);
//        xy = xy.map(y => y + 0.5);
//        this.drawer.draw(xx, xy);
//        render();
    }

    render() {
        const gl = this.gl

        // Update vertex positions
        gl.bindBuffer(gl.ARRAY_BUFFER, this.VBO_nodes);
        gl.bufferData(gl.ARRAY_BUFFER, this.drawer.node_coords, gl.STATIC_DRAW);

        // Update vertex colors
        this.colors.fill(1.0); // Set all vertices to white for now
        gl.bindBuffer(gl.ARRAY_BUFFER, this.VBO_color);
        gl.bufferData(gl.ARRAY_BUFFER, this.colors, gl.DYNAMIC_DRAW);

        // Render background
        gl.clearColor(0.2, 0.2, 0.2, 1.0);
        gl.clear(gl.COLOR_BUFFER_BIT);

        // Render the particles
        this.shader.use();
        gl.bindVertexArray(this.VAO);
        gl.drawElements(gl.TRIANGLES, this.drawer.triangles.length, gl.UNSIGNED_INT, 0);

    }

    destroy() {
        // De-allocate and clean up
        gl.deleteVertexArray(VAO);
        gl.deleteBuffer(VBO_nodes);
        gl.deleteBuffer(VBO_color);
        gl.deleteBuffer(EBO);
    }
}


class Client {
    socket;
    state;
    n;
    xy;
    drawer;
    graphics;
    mode;
    ip;

    constructor(mode, ip) {
        this.state = 0;
        this.mode = mode;
        this.ip = ip;

        // Connect to the websocket server
        console.log("Running in " + mode + " mode.");
        const websocketUri = 'ws://' + ip + ':8081';
        const socket = new WebSocket(websocketUri);
        socket.binaryType = "arraybuffer";

        // Log messages from server
        socket.onmessage = (event) => this.onmessage(event);

        // Log errors
        socket.onerror = function(error) {
            console.error('WebSocket Error:', error);
        };

        // Log open connection
        socket.onopen = function(event) {
            console.log('Connected to WebSocket server at ' + websocketUri);
            // Optional: Send a test message
            socket.send('initialize');
        };
        this.socket = socket;
    }

    onmessage(event) {
        console.log(this.state);
        if (this.state == 0) {
            console.log('Initializing from server:', event.data);
            // Read simulation size
            const dataView = new DataView(event.data);
            this.n = dataView.getInt32(0, true);
            console.log('Received n from server: ', this.n);
            ++this.state;
        } else if (this.state == 1) {
            // Read initial condition
            const dataView = new DataView(event.data);
            this.xy = new Float32Array(event.data);
            console.log('Received initial condition from server: ', this.xy);
            this.setup();
            this.socket.send('run');
            ++this.state;
        } else {
            const restartButton = document.getElementById("restartButton");
            if (restartButton.checked) {
                this.state = 0;
                this.socket.send('initialize');
                restartButton.checked = false;
            } else {
                // Read new state
                const dataView = new DataView(event.data);
                this.xy.set(new Float32Array(event.data));
                console.log('Received new state from server: ', this.xy);
                this.drawer.draw(this.xy);
                this.graphics.render();
                this.socket.send('run');
            }
        }

    }

    setup() {
        // Set up particle drawer
        this.drawer = new ParticleDrawer(this.n, 20);
        this.drawer.draw(this.xy)
        // Set up graphics
        this.graphics = new Graphics(this.drawer);
        console.log("Initialized graphics");
        this.graphics.render();
    }
}


function main(mode, ip) {
    const client = new Client(mode, ip);
}

const config = await (await fetch('/config')).json();

main(config.mode, config.ip);
