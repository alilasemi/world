class Shader {
    program;
    gl;

    constructor(gl, vertexSource, fragmentSource) {
        this.gl = gl;
        const vertexShader = this.compileShader(vertexSource, this.gl.VERTEX_SHADER);
        const fragmentShader = this.compileShader(fragmentSource, this.gl.FRAGMENT_SHADER);
        this.createProgram(vertexShader, fragmentShader);
    }

    compileShader(source, type) {
        const shader = this.gl.createShader(type);
        this.gl.shaderSource(shader, source);
        this.gl.compileShader(shader);
        // Check for errors
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
        // Check for errors
        if (!this.gl.getProgramParameter(this.program, this.gl.LINK_STATUS)) {
            console.error("ERROR::PROGRAM_LINKING_ERROR: ", this.gl.getProgramInfoLog(this.program));
            this.gl.deleteProgram(this.program);
            return;
        }
        this.gl.deleteShader(vertexShader);
        this.gl.deleteShader(fragmentShader);
    }

    use() {
        this.gl.useProgram(this.program);
    }

    // Uniform setters. Each one binds the program first, so callers can set
    // uniforms without tracking which program is currently active.
    setUniformMatrix4(name, matrix) {
        this.use();
        this.gl.uniformMatrix4fv(this.gl.getUniformLocation(this.program, name), false, matrix);
    }

    setUniform3f(name, x, y, z) {
        this.use();
        this.gl.uniform3f(this.gl.getUniformLocation(this.program, name), x, y, z);
    }

    setUniform1f(name, x) {
        this.use();
        this.gl.uniform1f(this.gl.getUniformLocation(this.program, name), x);
    }
}


// ---------------------------------------------------------------------------
// Minimal column-major 4x4 matrix math (GL convention: m[col*4 + row]).
// Hand-rolled rather than pulling in gl-matrix, since a fixed camera only
// needs these three functions and the client otherwise has no build step or
// browser-side dependencies.
// ---------------------------------------------------------------------------

function mat4Ortho(left, right, bottom, top, near, far) {
    const m = new Float32Array(16);
    m[0] = 2 / (right - left);
    m[5] = 2 / (top - bottom);
    m[10] = -2 / (far - near);
    m[12] = -(right + left) / (right - left);
    m[13] = -(top + bottom) / (top - bottom);
    m[14] = -(far + near) / (far - near);
    m[15] = 1;
    return m;
}

function mat4LookAt(eye, center, up) {
    const sub = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
    const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
                             a[0] * b[1] - a[1] * b[0]];
    const dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
    const norm = (v) => { const l = Math.hypot(v[0], v[1], v[2]); return [v[0] / l, v[1] / l, v[2] / l]; };

    const f = norm(sub(center, eye));   // forward (camera looks along -z in view space)
    const s = norm(cross(f, up));       // right
    const u = cross(s, f);              // true up

    const m = new Float32Array(16);
    m[0] = s[0];  m[1] = u[0];  m[2] = -f[0];  m[3] = 0;
    m[4] = s[1];  m[5] = u[1];  m[6] = -f[1];  m[7] = 0;
    m[8] = s[2];  m[9] = u[2];  m[10] = -f[2]; m[11] = 0;
    m[12] = -dot(s, eye);  m[13] = -dot(u, eye);  m[14] = dot(f, eye);  m[15] = 1;
    return m;
}

// Transform a point (w = 1) by a column-major mat4.
function mat4TransformPoint(m, p) {
    return [
        m[0] * p[0] + m[4] * p[1] + m[8]  * p[2] + m[12],
        m[1] * p[0] + m[5] * p[1] + m[9]  * p[2] + m[13],
        m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14],
    ];
}


// ---------------------------------------------------------------------------
// Camera: a FIXED isometric view. No orbit, no pan, no perspective -- the
// simulation animates but the camera does not move.
//
// The view direction is normalize(1,1,1), which is what makes this a true
// isometric projection (azimuth 45 degrees, elevation atan(1/sqrt(2)) ~=
// 35.26 degrees): the three world axes project to three directions 120
// degrees apart and equal world-space lengths along each axis project to
// equal screen lengths. World up is +z (gravity acts along -z server-side),
// so the "up" vector below is +z rather than GL's usual +y -- that flip is
// the entire cost of using the physics convention rather than the graphics
// one, and it lives here.
//
// The orthographic extents are fitted to the domain box rather than
// hardcoded, then widened on one axis so the projection's aspect ratio
// matches the canvas's. That is what keeps spheres circular: uniform
// world-to-pixel scale on both screen axes.
// ---------------------------------------------------------------------------
function buildCamera(domain, canvasWidth, canvasHeight) {
    const center = [
        0.5 * (domain.xMin + domain.xMax),
        0.5 * (domain.yMin + domain.yMax),
        0.5 * (domain.zMin + domain.zMax),
    ];
    const diagonal = Math.hypot(domain.xMax - domain.xMin,
                                domain.yMax - domain.yMin,
                                domain.zMax - domain.zMin);
    // Distance is irrelevant to an orthographic image (it only has to be far
    // enough that the whole box sits inside the near/far range), so use the
    // box diagonal and be done.
    const d = diagonal;
    const dir = 1 / Math.sqrt(3);
    const eye = [center[0] + d * dir, center[1] + d * dir, center[2] + d * dir];
    const view = mat4LookAt(eye, center, [0, 0, 1]);

    // Fit the ortho box to the domain's eight corners in view space.
    let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity, maxDepth = -Infinity;
    for (const cx of [domain.xMin, domain.xMax]) {
        for (const cy of [domain.yMin, domain.yMax]) {
            for (const cz of [domain.zMin, domain.zMax]) {
                const v = mat4TransformPoint(view, [cx, cy, cz]);
                minX = Math.min(minX, v[0]); maxX = Math.max(maxX, v[0]);
                minY = Math.min(minY, v[1]); maxY = Math.max(maxY, v[1]);
                maxDepth = Math.max(maxDepth, -v[2]);  // camera looks down -z
            }
        }
    }
    // A little margin so grains resting exactly on a wall aren't clipped.
    const margin = 0.04 * diagonal;
    let halfW = 0.5 * (maxX - minX) + margin;
    let halfH = 0.5 * (maxY - minY) + margin;
    const midX = 0.5 * (maxX + minX);
    const midY = 0.5 * (maxY + minY);

    // Match the canvas aspect by EXPANDING the narrower axis -- never by
    // scaling one axis independently, which would turn spheres into ellipses.
    const canvasAspect = canvasWidth / canvasHeight;
    if (halfW / halfH < canvasAspect) {
        halfW = halfH * canvasAspect;
    } else {
        halfH = halfW / canvasAspect;
    }

    const projection = mat4Ortho(midX - halfW, midX + halfW,
                                 midY - halfH, midY + halfH,
                                 0.0, maxDepth + margin + diagonal);
    return { view, projection };
}


// ---------------------------------------------------------------------------
// Impostor-sphere shaders.
//
// Each particle is ONE camera-facing quad (4 vertices, instanced), not a
// sphere mesh: the fragment shader reconstructs the sphere from the quad's
// local coordinates, discards fragments outside the unit circle, and writes
// an analytic gl_FragDepth so spheres intersect and occlude each other
// correctly. This is the standard molecular-viewer approach -- fewer
// vertices than instanced sphere meshes, less code, and a perfectly smooth
// silhouette at any zoom.
// ---------------------------------------------------------------------------
const particleVertexSource = `#version 300 es

    precision highp float;

    layout (location = 0) in vec2 aCorner;   // per-vertex: quad corner in [-1,1]^2
    layout (location = 1) in vec3 aCenter;   // per-instance: particle center, world space

    uniform mat4 uView;
    uniform mat4 uProjection;
    uniform float uRadius;

    out vec2 vCorner;
    out vec3 vViewCenter;
    out float vHeight;

    void main()
    {
        vViewCenter = (uView * vec4(aCenter, 1.0f)).xyz;
        // Offset in VIEW space, so the quad always faces the camera.
        vec3 viewPos = vViewCenter + vec3(aCorner * uRadius, 0.0f);
        gl_Position = uProjection * vec4(viewPos, 1.0f);
        vCorner = aCorner;
        vHeight = aCenter.z;
    }
`;

const particleFragmentSource = `#version 300 es

    precision highp float;

    in vec2 vCorner;
    in vec3 vViewCenter;
    in float vHeight;

    uniform mat4 uProjection;
    uniform float uRadius;
    uniform vec3 uLightDir;      // view space, normalized
    uniform vec3 uColorLow;
    uniform vec3 uColorHigh;
    uniform float uHeightMin;
    uniform float uHeightMax;

    out vec4 FragColor;

    void main()
    {
        // Reconstruct the sphere: the quad's local coords are the x/y of the
        // view-space normal, so anything outside the unit circle is off-sphere.
        float r2 = dot(vCorner, vCorner);
        if (r2 > 1.0f) discard;
        float nz = sqrt(1.0f - r2);
        vec3 normal = vec3(vCorner, nz);   // already unit length, view space

        // Analytic depth: project the actual sphere-surface point rather than
        // the flat quad, so overlapping spheres resolve correctly against each
        // other and against the domain box.
        vec3 surface = vViewCenter + normal * uRadius;
        vec4 clip = uProjection * vec4(surface, 1.0f);
        gl_FragDepth = 0.5f * (clip.z / clip.w) + 0.5f;

        // Lambert + ambient, plus a tight specular so the spheres read as
        // spheres rather than flat discs.
        float diffuse = max(dot(normal, uLightDir), 0.0f);
        vec3 halfway = normalize(uLightDir + vec3(0.0f, 0.0f, 1.0f));
        float specular = pow(max(dot(normal, halfway), 0.0f), 32.0f);

        // Tint by height so the pile's structure is readable in a still image.
        float t = clamp((vHeight - uHeightMin) / max(uHeightMax - uHeightMin, 1e-6f), 0.0f, 1.0f);
        vec3 base = mix(uColorLow, uColorHigh, t);

        vec3 color = base * (0.28f + 0.72f * diffuse) + vec3(0.35f) * specular;
        FragColor = vec4(color, 1.0f);
    }
`;

// The domain box / collision-grid overlay is a fixed color, so no per-vertex
// attribute beyond position is needed.
const lineVertexSource = `#version 300 es

    precision highp float;

    layout (location = 0) in vec3 aPos;

    uniform mat4 uView;
    uniform mat4 uProjection;

    void main()
    {
        gl_Position = uProjection * uView * vec4(aPos, 1.0f);
    }
`;

const lineFragmentSource = `#version 300 es

    precision highp float;

    uniform vec3 uColor;

    out vec4 FragColor;

    void main()
    {
        FragColor = vec4(uColor, 1.0f);
    }
`;


// The 12 edges of the domain box, in world coordinates. Always drawn: in an
// orthographic isometric view with no camera motion, the box is the only
// thing that establishes scale and orientation.
function buildDomainBoxVertices(domain) {
    const { xMin, xMax, yMin, yMax, zMin, zMax } = domain;
    const c = [
        [xMin, yMin, zMin], [xMax, yMin, zMin], [xMax, yMax, zMin], [xMin, yMax, zMin],
        [xMin, yMin, zMax], [xMax, yMin, zMax], [xMax, yMax, zMax], [xMin, yMax, zMax],
    ];
    const edges = [[0, 1], [1, 2], [2, 3], [3, 0],   // bottom face
                   [4, 5], [5, 6], [6, 7], [7, 4],   // top face
                   [0, 4], [1, 5], [2, 6], [3, 7]];  // verticals
    const vertices = new Float32Array(edges.length * 2 * 3);
    let o = 0;
    for (const [a, b] of edges) {
        vertices[o++] = c[a][0]; vertices[o++] = c[a][1]; vertices[o++] = c[a][2];
        vertices[o++] = c[b][0]; vertices[o++] = c[b][1]; vertices[o++] = c[b][2];
    }
    return vertices;
}

// Collision-grid cell boundaries, drawn on the FLOOR PLANE only (z = zMin).
//
// Deliberately not the full 3D grid: cells are sized to roughly one particle
// diameter, so a checked-in config has ~100 cells per axis and drawing all
// three families of planes is opaque visual noise that hides the particles
// the overlay is supposed to be read against. On the floor it still shows
// the actual cell size of the running simulation, which is the point of the
// overlay.
function buildFloorGridVertices(gridSizeX, gridSizeY, domain) {
    const { xMin, xMax, yMin, yMax, zMin } = domain;
    const vertices = new Float32Array((2 * (gridSizeX + 1) + 2 * (gridSizeY + 1)) * 3);
    let o = 0;
    for (let i = 0; i <= gridSizeX; ++i) {
        const tx = xMin + (xMax - xMin) * i / gridSizeX;
        vertices[o++] = tx; vertices[o++] = yMin; vertices[o++] = zMin;
        vertices[o++] = tx; vertices[o++] = yMax; vertices[o++] = zMin;
    }
    for (let j = 0; j <= gridSizeY; ++j) {
        const ty = yMin + (yMax - yMin) * j / gridSizeY;
        vertices[o++] = xMin; vertices[o++] = ty; vertices[o++] = zMin;
        vertices[o++] = xMax; vertices[o++] = ty; vertices[o++] = zMin;
    }
    return vertices;
}

// Fills the browser window. Unlike the 2D version, the canvas no longer needs
// to match the domain's aspect ratio -- buildCamera() handles aspect inside
// the projection, so the canvas can simply take all the space available.
function fitCanvasToWindow(canvas) {
    canvas.width = Math.max(1, Math.floor(window.innerWidth));
    canvas.height = Math.max(1, Math.floor(window.innerHeight));
    canvas.style.width = `${canvas.width}px`;
    canvas.style.height = `${canvas.height}px`;
}


class Graphics {
    gl;
    canvas;
    domain;
    n;
    radius;
    collisionGridSizeX;
    collisionGridSizeY;

    particleShader;
    lineShader;

    VAO_particles;
    VBO_corners;
    VBO_centers;

    VAO_box;
    VBO_box;
    boxVertexCount;

    VAO_grid;
    VBO_grid;
    gridVertexCount;

    constructor(n, collisionGridSizeX, collisionGridSizeY, domain, radius) {
        this.n = n;
        this.domain = domain;
        this.radius = radius;
        this.collisionGridSizeX = collisionGridSizeX;
        this.collisionGridSizeY = collisionGridSizeY;

        const canvas = document.querySelector("#gl-canvas");
        this.canvas = canvas;
        fitCanvasToWindow(canvas);

        const gl = canvas.getContext("webgl2");
        this.gl = gl;
        if (gl === null) {
            alert("Unable to initialize WebGL. Your browser or machine may not support it.");
            return;
        }
        gl.viewport(0, 0, canvas.width, canvas.height);

        // Depth testing is what makes the 3D view readable; the impostor
        // fragment shader writes analytic depth so spheres interpenetrate
        // correctly. Opaque geometry only, so no blending and no sorting.
        gl.enable(gl.DEPTH_TEST);
        gl.depthFunc(gl.LESS);
        gl.clearColor(0.09, 0.10, 0.12, 1.0);
        gl.clearDepth(1.0);

        this.particleShader = new Shader(gl, particleVertexSource, particleFragmentSource);
        this.lineShader = new Shader(gl, lineVertexSource, lineFragmentSource);

        // --- particle impostors: one instanced quad per particle ---
        // aCorner is static (4 vertices, divisor 0); aCenter is per-instance
        // (divisor 1) and re-uploaded every frame from the server's positions.
        const corners = new Float32Array([-1, -1, 1, -1, -1, 1, 1, 1]);
        this.VAO_particles = gl.createVertexArray();
        gl.bindVertexArray(this.VAO_particles);

        this.VBO_corners = gl.createBuffer();
        gl.bindBuffer(gl.ARRAY_BUFFER, this.VBO_corners);
        gl.bufferData(gl.ARRAY_BUFFER, corners, gl.STATIC_DRAW);
        gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 0, 0);
        gl.enableVertexAttribArray(0);
        gl.vertexAttribDivisor(0, 0);

        this.VBO_centers = gl.createBuffer();
        gl.bindBuffer(gl.ARRAY_BUFFER, this.VBO_centers);
        gl.bufferData(gl.ARRAY_BUFFER, n * 3 * Float32Array.BYTES_PER_ELEMENT, gl.DYNAMIC_DRAW);
        gl.vertexAttribPointer(1, 3, gl.FLOAT, false, 0, 0);
        gl.enableVertexAttribArray(1);
        gl.vertexAttribDivisor(1, 1);

        // --- domain box ---
        const boxVertices = buildDomainBoxVertices(domain);
        this.boxVertexCount = boxVertices.length / 3;
        this.VAO_box = gl.createVertexArray();
        this.VBO_box = gl.createBuffer();
        gl.bindVertexArray(this.VAO_box);
        gl.bindBuffer(gl.ARRAY_BUFFER, this.VBO_box);
        gl.bufferData(gl.ARRAY_BUFFER, boxVertices, gl.STATIC_DRAW);
        gl.vertexAttribPointer(0, 3, gl.FLOAT, false, 0, 0);
        gl.enableVertexAttribArray(0);

        // --- collision-grid overlay on the floor (static geometry) ---
        const gridVertices = buildFloorGridVertices(collisionGridSizeX, collisionGridSizeY, domain);
        this.gridVertexCount = gridVertices.length / 3;
        this.VAO_grid = gl.createVertexArray();
        this.VBO_grid = gl.createBuffer();
        gl.bindVertexArray(this.VAO_grid);
        gl.bindBuffer(gl.ARRAY_BUFFER, this.VBO_grid);
        gl.bufferData(gl.ARRAY_BUFFER, gridVertices, gl.STATIC_DRAW);
        gl.vertexAttribPointer(0, 3, gl.FLOAT, false, 0, 0);
        gl.enableVertexAttribArray(0);

        this.updateCamera();

        // Fixed light in VIEW space, so the shading is stable regardless of
        // where the (fixed) camera happens to be -- slightly up and to the
        // right of the viewer.
        const lx = 0.35, ly = 0.45, lz = 0.82;
        const ll = Math.hypot(lx, ly, lz);
        this.particleShader.setUniform3f('uLightDir', lx / ll, ly / ll, lz / ll);
        this.particleShader.setUniform1f('uRadius', radius);
        this.particleShader.setUniform3f('uColorLow', 0.42, 0.47, 0.58);
        this.particleShader.setUniform3f('uColorHigh', 0.96, 0.94, 0.88);
        this.particleShader.setUniform1f('uHeightMin', domain.zMin);
        this.particleShader.setUniform1f('uHeightMax', domain.zMax);
    }

    // Rebuilds view/projection for the current canvas size and pushes them to
    // both shaders. Called once at construction and on every resize.
    updateCamera() {
        const { view, projection } = buildCamera(this.domain, this.canvas.width, this.canvas.height);
        for (const s of [this.particleShader, this.lineShader]) {
            s.setUniformMatrix4('uView', view);
            s.setUniformMatrix4('uProjection', projection);
        }
    }

    // Called from a single window "resize" listener owned by Client, not one
    // per Graphics instance, so repeated restarts don't accumulate listeners
    // retaining stale Graphics/canvas state.
    handleResize() {
        fitCanvasToWindow(this.canvas);
        this.gl.viewport(0, 0, this.canvas.width, this.canvas.height);
        this.updateCamera();
    }

    render(positions) {
        const gl = this.gl;

        // Upload this frame's particle centers straight from the server's
        // buffer -- no per-frame geometry generation at all, which is the
        // point of impostors.
        gl.bindBuffer(gl.ARRAY_BUFFER, this.VBO_centers);
        gl.bufferSubData(gl.ARRAY_BUFFER, 0, positions);

        gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);

        // Lines first, particles second: with depth testing on, order doesn't
        // affect correctness, but drawing the frame of reference first reads
        // better when the pile is dense.
        this.lineShader.use();
        this.lineShader.setUniform3f('uColor', 0.45, 0.45, 0.52);
        gl.bindVertexArray(this.VAO_box);
        gl.drawArrays(gl.LINES, 0, this.boxVertexCount);

        if (document.getElementById('gridButton').checked) {
            this.lineShader.use();
            this.lineShader.setUniform3f('uColor', 0.65, 0.16, 0.16);
            gl.bindVertexArray(this.VAO_grid);
            gl.drawArrays(gl.LINES, 0, this.gridVertexCount);
        }

        this.particleShader.use();
        gl.bindVertexArray(this.VAO_particles);
        gl.drawArraysInstanced(gl.TRIANGLE_STRIP, 0, 4, this.n);
    }

    destroy() {
        const gl = this.gl;
        gl.deleteVertexArray(this.VAO_particles);
        gl.deleteVertexArray(this.VAO_box);
        gl.deleteVertexArray(this.VAO_grid);
        gl.deleteBuffer(this.VBO_corners);
        gl.deleteBuffer(this.VBO_centers);
        gl.deleteBuffer(this.VBO_box);
        gl.deleteBuffer(this.VBO_grid);
    }
}


// Format a time (ms) and its percentage of total as a fixed-width string
// for the monospace HUD. nameWidth controls the label column width.
function fmtRow(label, ms, pct, indent = '') {
    const msStr = ms.toFixed(1).padStart(7);
    const pctStr = pct.toFixed(1).padStart(5);
    return `${(indent + label).padEnd(20)}${msStr} ms  ${pctStr}%`;
}


class Client {
    socket;
    state;
    n;
    collisionGridSizeX;
    collisionGridSizeY;
    collisionGridSizeZ;
    domain;
    positions;
    graphics;
    mode;
    ip;
    hud;
    lastFrameStart;   // performance.now() at start of previous steady-state message

    constructor(mode, ip) {
        this.state = 0;
        this.mode = mode;
        this.ip = ip;
        this.hud = document.getElementById('hud');
        this.lastFrameStart = null;

        // One resize listener for the lifetime of the page, delegating to
        // whichever Graphics instance is current -- avoids accumulating a
        // new listener (and retaining the old Graphics/canvas) on every
        // "Restart" click, since setup() reassigns this.graphics each time.
        window.addEventListener('resize', () => this.graphics?.handleResize());

        // Connect to the websocket server
        console.log("Running in " + mode + " mode.");
        const websocketUri = 'ws://' + ip + ':8081';
        const socket = new WebSocket(websocketUri);
        socket.binaryType = "arraybuffer";

        socket.onmessage = (event) => this.onmessage(event);

        socket.onerror = function(error) {
            console.error('WebSocket Error:', error);
        };

        socket.onopen = function(event) {
            console.log('Connected to WebSocket server at ' + websocketUri);
            socket.send('initialize');
        };
        this.socket = socket;
    }

    onmessage(event) {
        if (this.state == 0) {
            // Read simulation size
            const dataView = new DataView(event.data);
            this.n = dataView.getInt32(0, true);
            console.log('Received n from server: ', this.n);
            ++this.state;
        } else if (this.state == 1) {
            // Read collision grid resolution, all three axes.
            const dataView = new DataView(event.data);
            this.collisionGridSizeX = dataView.getInt32(0, true);
            this.collisionGridSizeY = dataView.getInt32(4, true);
            this.collisionGridSizeZ = dataView.getInt32(8, true);
            console.log('Received collisionGridSize from server: ',
                    this.collisionGridSizeX, this.collisionGridSizeY, this.collisionGridSizeZ);
            ++this.state;
        } else if (this.state == 2) {
            // Read rendering constant: triangles per particle. Retained for
            // wire-protocol compatibility but unused by the impostor
            // renderer, which needs no tessellation.
            const dataView = new DataView(event.data);
            this.numTriangles = dataView.getInt32(0, true);
            ++this.state;
        } else if (this.state == 3) {
            // Read rendering constant: particle radius (sent once at init;
            // reuses the simulation's particle_radius)
            const dataView = new DataView(event.data);
            this.particleRadius = dataView.getFloat32(0, true);
            console.log('Received particleRadius from server: ', this.particleRadius);
            ++this.state;
        } else if (this.state == 4) {
            // Read domain bounds, six floats. Used for the view volume and
            // for the in-domain particle counter -- the same bounds the
            // server's is_stable() checks.
            const dataView = new DataView(event.data);
            this.domain = {
                xMin: dataView.getFloat32(0, true),
                xMax: dataView.getFloat32(4, true),
                yMin: dataView.getFloat32(8, true),
                yMax: dataView.getFloat32(12, true),
                zMin: dataView.getFloat32(16, true),
                zMax: dataView.getFloat32(20, true),
            };
            console.log('Received domain bounds from server: ', this.domain);
            ++this.state;
        } else if (this.state == 5) {
            // Read initial condition (n*3 position floats)
            this.positions = new Float32Array(event.data);
            console.log('Received initial condition from server: ', this.positions.length, 'floats');
            this.setup();
            this.socket.send('run');
            ++this.state;
        } else {
            const restartButton = document.getElementById("restartButton");
            if (restartButton.checked) {
                this.state = 0;
                this.lastFrameStart = null;
                this.socket.send('initialize');
                restartButton.checked = false;
            } else {
                const frameStart = performance.now();
                const frameTime = this.lastFrameStart !== null
                    ? frameStart - this.lastFrameStart : null;

                // Parse payload:
                // [n*3 positions | sim_time | real_time_ratio |
                //  t_find_neighbors | t_compute_rhs | t_take_step | t_unpack_state]
                const floats = new Float32Array(event.data);
                const base = this.n * 3;
                this.positions.set(floats.subarray(0, base));
                const simTime    = floats[base + 0];
                const rtRatio    = floats[base + 1];
                const tFind      = floats[base + 2];
                const tRhs       = floats[base + 3];
                const tStep      = floats[base + 4];
                const tUnpack    = floats[base + 5];

                const t0Render = performance.now();
                this.graphics.render(this.positions);
                const tRender = performance.now() - t0Render;

                this.socket.send('run');

                // Update HUD (after WS send so send latency is in "overhead")
                const tServer = tFind + tRhs + tStep + tUnpack;
                const tClient = tRender;
                const tSum    = tServer + tClient;
                const ft      = frameTime ?? 0;
                const pct = (ms) => ft > 0 ? ms / ft * 100 : 0;

                // Count particles currently within domain bounds vs. the total --
                // same bounds check as the server's ParticleDynamics::is_stable().
                let numInDomain = 0;
                for (let i = 0; i < this.n; ++i) {
                    const x = this.positions[3 * i + 0];
                    const y = this.positions[3 * i + 1];
                    const z = this.positions[3 * i + 2];
                    if (x >= this.domain.xMin && x <= this.domain.xMax &&
                            y >= this.domain.yMin && y <= this.domain.yMax &&
                            z >= this.domain.zMin && z <= this.domain.zMax) {
                        ++numInDomain;
                    }
                }

                const sep = '─'.repeat(38);
                const lines = [
                    `Sim time: ${simTime.toFixed(4)} s    Realtime ratio: ${rtRatio.toFixed(2)}×`,
                    `Particles in domain: ${numInDomain} / ${this.n}`,
                    '',
                    frameTime !== null
                        ? `Frame time:          ${ft.toFixed(1).padStart(7)} ms`
                        : 'Frame time:          (first frame)',
                    sep,
                    fmtRow('Server', tServer, pct(tServer)),
                    fmtRow('Find neighbors',  tFind,   pct(tFind),   '  '),
                    fmtRow('Compute RHS',     tRhs,    pct(tRhs),    '  '),
                    fmtRow('Take step',       tStep,   pct(tStep),   '  '),
                    fmtRow('Unpack state',    tUnpack, pct(tUnpack), '  '),
                    fmtRow('Client', tClient, pct(tClient)),
                    fmtRow('WebGL render',    tRender, pct(tRender), '  '),
                    sep,
                    fmtRow('Sum of components', tSum, pct(tSum)),
                ];
                this.hud.textContent = lines.join('\n');

                this.lastFrameStart = frameStart;
            }
        }
    }

    setup() {
        this.graphics = new Graphics(this.n, this.collisionGridSizeX, this.collisionGridSizeY,
                this.domain, this.particleRadius);
        console.log("Initialized graphics");
        this.graphics.render(this.positions);
    }
}


function main(mode, ip) {
    const client = new Client(mode, ip);
}

const config = await (await fetch('/config')).json();

main(config.mode, config.ip);
