// Convert the above Shader class from C++ to JavaScript
class Shader {
    gl;
    program;

    constructor(gl, vertexSource, fragmentSource) {
        this.gl = gl;
        this.loadShader(vertexSource, fragmentSource);
    }

    loadShader(vertexSource, fragmentSource) {
        const vertexShader = this.compileShader(vertexSource, this.gl.VERTEX_SHADER);
        const fragmentShader = this.compileShader(fragmentSource, this.gl.FRAGMENT_SHADER);

        this.createProgram(vertexShader, fragmentShader);
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

const particleVertexSource = `#version 300 es

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
const particleFragmentSource = `#version 300 es

    precision highp float;

    out vec4 FragColor;

    in vec3 ourColor;

    void main()
    {
        FragColor = vec4(ourColor, 1.0f);
    }
`;

// Grid lines are a fixed red, so unlike the particle shader, no per-vertex
// color attribute is needed.
const gridVertexSource = `#version 300 es

    precision highp float;

    layout (location = 0) in vec3 aPos;

    void main()
    {
        gl_Position = vec4(aPos, 1.0f);
    }
`;
const gridFragmentSource = `#version 300 es

    precision highp float;

    out vec4 FragColor;

    void main()
    {
        FragColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);
    }
`;

// Builds vertex positions for the gridSize x gridSize spatial grid's
// cell-boundary lines, covering the [-1,1]x[-1,1] world domain that particle
// positions already live in (no projection/camera transform anywhere in
// this client, so these need no transform either). Static geometry -- built
// once, not regenerated per frame like ParticleDrawer.draw().
function buildGridLineVertices(gridSize) {
    const vertices = new Float32Array(4 * (gridSize + 1) * 3);
    let offset = 0;
    for (let i = 0; i <= gridSize; ++i) {
        const t = -1.0 + 2.0 * i / gridSize;
        // Horizontal line at y = t, spanning x in [-1, 1]
        vertices[offset++] = -1.0; vertices[offset++] = t; vertices[offset++] = 0.0;
        vertices[offset++] = 1.0; vertices[offset++] = t; vertices[offset++] = 0.0;
        // Vertical line at x = t, spanning y in [-1, 1]
        vertices[offset++] = t; vertices[offset++] = -1.0; vertices[offset++] = 0.0;
        vertices[offset++] = t; vertices[offset++] = 1.0; vertices[offset++] = 0.0;
    }
    return vertices;
}


// Convert the above ParticleDrawer class from C++ to JavaScript
class ParticleDrawer {
    constructor(n, num_triangles, radius) {
        this.n = n;
        this.num_triangles = num_triangles;
        this.radius = radius;
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
    VBO_grid;
    VAO_grid;
    gridShader;
    gridVertexCount;

    constructor(drawer, gridSize) {
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
        this.shader = new Shader(gl, particleVertexSource, particleFragmentSource);
        this.gridShader = new Shader(gl, gridVertexSource, gridFragmentSource);

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

        // Grid lines: static geometry (built once here, not per frame), no
        // color attribute needed since the grid shader hardcodes red.
        const gridVertices = buildGridLineVertices(gridSize);
        this.gridVertexCount = gridVertices.length / 3;
        this.VAO_grid = gl.createVertexArray();
        this.VBO_grid = gl.createBuffer();
        gl.bindVertexArray(this.VAO_grid);
        gl.bindBuffer(gl.ARRAY_BUFFER, this.VBO_grid);
        gl.bufferData(gl.ARRAY_BUFFER, gridVertices, gl.STATIC_DRAW);
        gl.vertexAttribPointer(0, 3, gl.FLOAT, false, 3 * Float32Array.BYTES_PER_ELEMENT, 0);
        gl.enableVertexAttribArray(0);
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

        // Render the grid first so particles draw on top of it
        if (document.getElementById('gridButton').checked) {
            this.gridShader.use();
            gl.bindVertexArray(this.VAO_grid);
            gl.drawArrays(gl.LINES, 0, this.gridVertexCount);
        }

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


// Format a time (ms) and its percentage of total as a fixed-width string
// for the monospace HUD. nameWidth controls the label column width.
function fmtRow(label, ms, pct, indent = '') {
    const msStr = ms.toFixed(1).padStart(7);
    const pctStr = `(${pct.toFixed(1)}%)`.padStart(8);
    return `${indent}${label.padEnd(20 - indent.length)}${msStr} ms ${pctStr}`;
}

class Client {
    socket;
    state;
    n;
    gridSize;
    xy;
    drawer;
    graphics;
    mode;
    ip;
    hud;
    lastFrameStart;   // performance.now() at start of previous steady-state message

    constructor(mode, ip, observeMode) {
        this.state = 0;
        this.mode = mode;
        this.ip = ip;
        this.observeMode = observeMode;
        this.hud = document.getElementById('hud');
        this.lastFrameStart = null;

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
            // Read spatial grid resolution (for drawing the grid overlay)
            const dataView = new DataView(event.data);
            this.gridSize = dataView.getInt32(0, true);
            console.log('Received gridSize from server: ', this.gridSize);
            ++this.state;
        } else if (this.state == 2) {
            // Read rendering constant: triangles per particle (sent once at init)
            const dataView = new DataView(event.data);
            this.numTriangles = dataView.getInt32(0, true);
            console.log('Received numTriangles from server: ', this.numTriangles);
            ++this.state;
        } else if (this.state == 3) {
            // Read rendering constant: particle radius (sent once at init;
            // reuses the simulation's particle_radius)
            const dataView = new DataView(event.data);
            this.particleRadius = dataView.getFloat32(0, true);
            console.log('Received particleRadius from server: ', this.particleRadius);
            ++this.state;
        } else if (this.state == 4) {
            // Read initial condition
            const dataView = new DataView(event.data);
            this.xy = new Float32Array(event.data);
            console.log('Received initial condition from server: ', this.xy);
            this.setup();
            if (this.observeMode) {
                this.socket.send('observe');
            } else {
                this.socket.send('run');
            }
            ++this.state;
        } else {
            const restartButton = document.getElementById("restartButton");
            if (!this.observeMode && restartButton.checked) {
                this.state = 0;
                this.lastFrameStart = null;
                this.socket.send('initialize');
                restartButton.checked = false;
            } else {
                const frameStart = performance.now();
                const frameTime = this.lastFrameStart !== null
                    ? frameStart - this.lastFrameStart : null;

                // Parse payload:
                // [n*2 xy | sim_time | real_time_ratio |
                //  t_find_neighbors | t_interpolate_force |
                //  t_compute_rhs | t_take_step | t_unpack_state]
                const floats = new Float32Array(event.data);
                const base = this.n * 2;
                this.xy.set(floats.subarray(0, base));
                const simTime    = floats[base + 0];
                const rtRatio    = floats[base + 1];
                const tFind      = floats[base + 2];
                const tInterp    = floats[base + 3];
                const tRhs       = floats[base + 4];
                const tStep      = floats[base + 5];
                const tUnpack    = floats[base + 6];

                // Client-side timers
                const t0Draw = performance.now();
                this.drawer.draw(this.xy);
                const tDraw = performance.now() - t0Draw;

                const t0Render = performance.now();
                this.graphics.render();
                const tRender = performance.now() - t0Render;

                if (!this.observeMode) {
                    this.socket.send('run');
                }

                // Update HUD (after WS send so send latency is in "overhead")
                const tServer = tFind + tInterp + tRhs + tStep + tUnpack;
                const tClient = tDraw + tRender;
                const tSum    = tServer + tClient;
                const ft      = frameTime ?? 0;
                const pct = (ms) => ft > 0 ? ms / ft * 100 : 0;

                const sep = '─'.repeat(38);
                const lines = [
                    `Sim time: ${simTime.toFixed(4)} s    Realtime ratio: ${rtRatio.toFixed(2)}×`,
                    '',
                    frameTime !== null
                        ? `Frame time:          ${ft.toFixed(1).padStart(7)} ms`
                        : 'Frame time:          (first frame)',
                    sep,
                    fmtRow('Server', tServer, pct(tServer)),
                    fmtRow('Find neighbors',  tFind,   pct(tFind),   '  '),
                    fmtRow('Interp. force',   tInterp, pct(tInterp), '  '),
                    fmtRow('Compute RHS',     tRhs,    pct(tRhs),    '  '),
                    fmtRow('Take step',       tStep,   pct(tStep),   '  '),
                    fmtRow('Unpack state',    tUnpack, pct(tUnpack), '  '),
                    fmtRow('Client', tClient, pct(tClient)),
                    fmtRow('Draw geometry',   tDraw,   pct(tDraw),   '  '),
                    fmtRow('WebGL render',    tRender, pct(tRender), '  '),
                    sep,
                    fmtRow('Sum of components', tSum, pct(tSum)),
                ];
                this.hud.textContent = lines.join('\n');

                console.log('Received new state from server: ', this.xy);
                this.lastFrameStart = frameStart;
            }
        }

    }

    setup() {
        // Set up particle drawer (num triangles + radius come from the server)
        this.drawer = new ParticleDrawer(this.n, this.numTriangles, this.particleRadius);
        this.drawer.draw(this.xy)
        // Set up graphics
        this.graphics = new Graphics(this.drawer, this.gridSize);
        console.log("Initialized graphics");
        this.graphics.render();
    }
}


function main(mode, ip) {
    const observeMode = new URLSearchParams(window.location.search).has('observe');
    const client = new Client(mode, ip, observeMode);
}

const config = await (await fetch('/config')).json();

main(config.mode, config.ip);
