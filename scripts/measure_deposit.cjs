// Measure the deposit left by a granular column collapse, straight off the wire.
//
// Speaks the same binary protocol as client/world.js (see the "Backend" section of
// CLAUDE.md) so it needs no changes to the solver: initialize, step for the requested
// simulated time, then reduce the final position buffer to the runout radius, deposit
// height and packed footprint. Used to caption assets/gifs/*.gif with numbers.
//
// Usage:
//   ./build/bin/world demos/sand.yaml &
//   node scripts/measure_deposit.cjs 2.4
//
// The argument is the simulated time in seconds; the number of "run" requests follows
// from drivers.steps_per_frame * simulation.dt, which is reported in the payload.

const target = parseFloat(process.argv[2] || '2.4');
const socket = new WebSocket('ws://localhost:8081');
socket.binaryType = 'arraybuffer';

let state = 0;
let n = 0;
let domain = null;

const quantile = (sorted, q) => sorted[Math.min(sorted.length - 1,
    Math.max(0, Math.round(q * (sorted.length - 1))))];

socket.onopen = () => socket.send('initialize');

socket.onmessage = (event) => {
    const view = new DataView(event.data);
    if (state === 0) { n = view.getInt32(0, true); ++state; return; }
    if (state >= 1 && state <= 3) { ++state; return; }   // grid sizes, triangles, radius
    if (state === 4) {
        domain = {
            xMin: view.getFloat32(0, true), xMax: view.getFloat32(4, true),
            yMin: view.getFloat32(8, true), yMax: view.getFloat32(12, true),
            zMin: view.getFloat32(16, true), zMax: view.getFloat32(20, true),
        };
        ++state; return;
    }
    if (state === 5) { ++state; socket.send('run'); return; }   // initial condition

    const floats = new Float32Array(event.data);
    const simTime = floats[3 * n + 0];
    if (simTime < target) { socket.send('run'); return; }

    const radius = new Float64Array(n);
    const height = new Float64Array(n);
    for (let i = 0; i < n; ++i) {
        const x = floats[3 * i + 0], y = floats[3 * i + 1], z = floats[3 * i + 2];
        radius[i] = Math.hypot(x, y);
        height[i] = z - domain.zMin;
    }
    radius.sort(); height.sort();
    // The 99th percentile rather than the maximum: a handful of grains skitter well past
    // the coherent front and would set a runout the deposit does not have.
    console.log(`t = ${simTime.toFixed(3)} s, n = ${n}`);
    console.log(`  radial extent   p50 ${quantile(radius, 0.50).toFixed(3)} m` +
                `   p99 ${quantile(radius, 0.99).toFixed(3)} m` +
                `   max ${radius[n - 1].toFixed(3)} m`);
    console.log(`  height above floor  p50 ${quantile(height, 0.50).toFixed(3)} m` +
                `   p99 ${quantile(height, 0.99).toFixed(3)} m` +
                `   max ${height[n - 1].toFixed(3)} m`);
    socket.close();
    process.exit(0);
};

socket.onerror = (e) => { console.error('websocket error', e.message ?? e); process.exit(1); };
