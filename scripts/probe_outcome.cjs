// Probe the outcome of one rollout: is the deposit interior, how deep is it, and does the
// throw velocity actually move it? Speaks the binary protocol directly, no browser.
//
//   ./build/bin/world probe/p4.yaml &
//   node scripts/probe_outcome.cjs 2.0
//
// Reports everything in grain diameters as well as metres, so configurations at different
// grain sizes can be compared. Wall clearance is the figure that decides whether a design is
// usable: a deposit that reaches a wall is clipped, and a model fitted to clipped outcomes
// learns which wall was hit instead of the physics.

const target = parseFloat(process.argv[2] || '2.0');
const socket = new WebSocket('ws://localhost:8081');
socket.binaryType = 'arraybuffer';

let state = 0, n = 0, radius = 0, domain = null;
const q = (s, p) => s[Math.min(s.length - 1, Math.max(0, Math.round(p * (s.length - 1))))];

socket.onopen = () => socket.send('initialize');
socket.onmessage = (event) => {
    const view = new DataView(event.data);
    if (state === 0) { n = view.getInt32(0, true); ++state; return; }
    if (state === 1 || state === 2) { ++state; return; }
    if (state === 3) { radius = view.getFloat32(0, true); ++state; return; }
    if (state === 4) {
        domain = { xMin: view.getFloat32(0, true), xMax: view.getFloat32(4, true),
                   yMin: view.getFloat32(8, true), yMax: view.getFloat32(12, true),
                   zMin: view.getFloat32(16, true), zMax: view.getFloat32(20, true) };
        ++state; return;
    }
    if (state === 5) { ++state; socket.send('run'); return; }

    const f = new Float32Array(event.data);
    if (f[3 * n] < target) { socket.send('run'); return; }

    const d = 2 * radius;
    const half = Math.min(domain.xMax, domain.yMax);
    let cx = 0, cy = 0, nearWall = 0;
    const rad = new Float64Array(n), hgt = new Float64Array(n);
    for (let i = 0; i < n; ++i) {
        const x = f[3 * i], y = f[3 * i + 1], z = f[3 * i + 2];
        cx += x; cy += y;
        rad[i] = Math.hypot(x, y);
        hgt[i] = z - domain.zMin;
        // "near a wall" means within two diameters of any side wall.
        if (Math.abs(x) > domain.xMax - 2 * d || Math.abs(y) > domain.yMax - 2 * d) ++nearWall;
    }
    cx /= n; cy /= n;
    rad.sort(); hgt.sort();
    const r50 = q(rad, 0.50), r99 = q(rad, 0.99), rmax = rad[n - 1];
    const h50 = q(hgt, 0.50), hmax = hgt[n - 1];
    console.log(`t = ${f[3 * n].toFixed(3)} s   n = ${n}   radius = ${radius}   half-domain = ${half}`);
    console.log(`  centroid            (${cx.toFixed(3)}, ${cy.toFixed(3)}) m` +
                `   = (${(cx / d).toFixed(1)}, ${(cy / d).toFixed(1)}) diameters`);
    console.log(`  radial extent       p50 ${r50.toFixed(3)}  p99 ${r99.toFixed(3)}  ` +
                `max ${rmax.toFixed(3)} m   = ${(r50 / d).toFixed(1)} / ${(r99 / d).toFixed(1)} / ${(rmax / d).toFixed(1)} diameters`);
    console.log(`  height above floor  p50 ${h50.toFixed(3)}  max ${hmax.toFixed(3)} m   ` +
                `= ${(h50 / d).toFixed(2)} / ${(hmax / d).toFixed(2)} diameters`);
    console.log(`  wall clearance      ${(half / rmax).toFixed(2)}x     ` +
                `grains within 2 diameters of a wall: ${(100 * nearWall / n).toFixed(2)}%`);
    console.log(`  aspect r50/h50      ${(r50 / h50).toFixed(1)}`);
    socket.close(); process.exit(0);
};
socket.onerror = (e) => { console.error('websocket error', e.message ?? e); process.exit(1); };
