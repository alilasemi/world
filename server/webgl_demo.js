// Convert the above Shader class from C++ to JavaScript
class Shader {
    constructor(vertexPath, fragmentPath) {
        this.ID = null;
        this.loadShader(vertexPath, fragmentPath);
        const canvas = document.querySelector("#gl-canvas");
        this.gl = canvas.getContext("webgl2");
    }

    async loadShader(vertexPath, fragmentPath) {
        try {
            const vertexResponse = await fetch(vertexPath);
            const fragmentResponse = await fetch(fragmentPath);
            const vertexCode = await vertexResponse.text();
            const fragmentCode = await fragmentResponse.text();

            const vertexShader = this.compileShader(vertexCode, this.gl.VERTEX_SHADER);
            const fragmentShader = this.compileShader(fragmentCode, this.gl.FRAGMENT_SHADER);

            this.ID = this.createProgram(vertexShader, fragmentShader);
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
        const program = this.gl.createProgram();
        this.gl.attachShader(program, vertexShader);
        this.gl.attachShader(program, fragmentShader);
        this.gl.linkProgram(program);

        if (!this.gl.getProgramParameter(program, this.gl.LINK_STATUS)) {
            console.error("ERROR::PROGRAM_LINKING_ERROR: ", this.gl.getProgramInfoLog(program));
            this.gl.deleteProgram(program);
            return null;
        }
        return program;
    }

    use() {
        if (this.ID) {
            this.gl.useProgram(this.ID);
        }
    }

    setBool(name, value) {
        const location = this.gl.getUniformLocation(this.ID, name);
        this.gl.uniform1i(location, value ? 1 : 0);
    }

    setInt(name, value) {
        const location = this.gl.getUniformLocation(this.ID, name);
        this.gl.uniform1i(location, value);
    }

    setFloat(name, value) {
        const location = this.gl.getUniformLocation(this.ID, name);
        this.gl.uniform1f(location, value);
    }
}


function main() {
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
    var shader = new Shader("vertex.glsl", "fragment.glsl");

    // Set clear color to black, fully opaque
    gl.clearColor(0.0, 0.0, 0.0, 1.0);
    // Clear the color buffer with specified clear color
    gl.clear(gl.COLOR_BUFFER_BIT);
}


main();
