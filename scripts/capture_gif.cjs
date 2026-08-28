// Record an animated GIF of the live WebGL2 client, headlessly.
//
// The capture drives the simulation rather than sampling it. The client's request/response
// loop is gated: every outgoing "run" command is intercepted and held until this script
// releases one, so exactly one server step batch, one render, and one screenshot happen per
// iteration. Frames are therefore spaced by exactly drivers.steps_per_frame * simulation.dt
// seconds of simulated time, independent of how slow the software rasterizer is.
//
// Usage:
//   ./build/bin/world demos/sand.yaml &
//   (cd client && node index.js local &)
//   node scripts/capture_gif.cjs assets/gifs/sand.gif 70 [fps]
//
// Requires a global Playwright install and system Chrome on PATH; WebGL2 comes from
// SwiftShader. Frames are written to a temporary directory and assembled with ffmpeg.

const { execFileSync } = require('child_process');
const fs = require('fs');
const os = require('os');
const path = require('path');

// Playwright is installed globally, not as a client/ dependency, so it has to be resolved
// out of the global module root rather than from node_modules next to this script.
const globalModules = execFileSync('npm', ['root', '-g'], { encoding: 'utf8' }).trim();
const { chromium } = require(path.join(globalModules, 'playwright'));

const out = process.argv[2];
const numFrames = parseInt(process.argv[3] || '70', 10);
const fps = parseInt(process.argv[4] || '25', 10);
const scale = process.env.CAPTURE_SCALE || '400';
const crop = process.env.CAPTURE_CROP || '724:848:88:26';
const width = parseInt(process.env.CAPTURE_WIDTH || '900', 10);
const height = parseInt(process.env.CAPTURE_HEIGHT || '900', 10);

if (!out) {
    console.error('usage: capture_gif.cjs <out.gif> [frames] [fps]');
    process.exit(1);
}

const readSimTime = () => {
    const hud = document.getElementById('hud');
    if (!hud) return null;
    const m = hud.textContent.match(/Sim time:\s*([0-9.]+)/);
    return m ? parseFloat(m[1]) : null;
};

(async () => {
    // CAPTURE_FRAME_DIR keeps the full-resolution PNG frames, so the clip can be
    // re-encoded or trimmed without re-running the simulation.
    const keep = !!process.env.CAPTURE_FRAME_DIR;
    const frameDir = keep ? process.env.CAPTURE_FRAME_DIR
        : fs.mkdtempSync(path.join(os.tmpdir(), 'world-gif-'));
    fs.mkdirSync(frameDir, { recursive: true });
    const browser = await chromium.launch({
        channel: 'chrome',
        args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader'],
    });
    const page = await browser.newPage({ viewport: { width, height } });
    page.on('pageerror', (e) => console.error('pageerror:', e.message));

    // Installed before any page script runs, so the very first "run" the client sends is
    // already gated. Everything else on the socket (notably "initialize") passes through.
    await page.addInitScript(() => {
        window.__held = [];
        window.__credits = 0;
        const send = WebSocket.prototype.send;
        WebSocket.prototype.send = function (data) {
            if (data !== 'run') return send.call(this, data);
            const fire = () => send.call(this, 'run');
            if (window.__credits > 0) { window.__credits -= 1; fire(); }
            else window.__held.push(fire);
        };
        window.__step = () => {
            if (window.__held.length > 0) window.__held.shift()();
            else window.__credits += 1;
        };
    });

    await page.goto('http://localhost:8080', { waitUntil: 'load' });

    // The HUD reports a real-time ratio dominated by the software rasterizer, which says
    // nothing about the solver, so it is hidden rather than recorded. visibility (not
    // display) keeps its text readable for the frame clock below.
    await page.addStyleTag({ content: '#hud, #controls { visibility: hidden; }' });

    await page.locator('canvas').waitFor({ timeout: 60000 });
    // Wait for the initial-condition handshake to finish, i.e. for the client to reach its
    // steady loop and queue its first gated "run".
    await page.waitForFunction(() => window.__held && window.__held.length > 0,
        null, { timeout: 120000, polling: 100 });

    let previous = null;
    for (let i = 0; i < numFrames; ++i) {
        await page.evaluate(() => window.__step());
        await page.waitForFunction(
            (last) => {
                const hud = document.getElementById('hud');
                if (!hud) return false;
                const m = hud.textContent.match(/Sim time:\s*([0-9.]+)/);
                if (!m) return false;
                return last === null || parseFloat(m[1]) > last;
            },
            previous,
            { timeout: 180000, polling: 50 },
        );
        previous = await page.evaluate(readSimTime);
        const file = path.join(frameDir, `frame_${String(i).padStart(4, '0')}.png`);
        // page.screenshot, not locator.screenshot: the latter first waits for the element to
        // be stable across two animation frames, and a software-rasterized frame of 32k
        // impostor spheres takes long enough for that check to time out. The canvas fills
        // the viewport anyway.
        await page.screenshot({ path: file, timeout: 120000 });
        if (i % 10 === 0) console.log(`frame ${i}/${numFrames} at t = ${previous.toFixed(3)} s`);
    }
    console.log(`captured ${numFrames} frames, final t = ${previous.toFixed(3)} s`);
    await browser.close();

    // Two-pass palette generation: one 256-colour palette for the whole clip, so the
    // shading of the grains does not flicker between frames.
    fs.mkdirSync(path.dirname(out), { recursive: true });
    const pattern = path.join(frameDir, 'frame_%04d.png');
    const palette = path.join(frameDir, 'palette.png');
    // The camera fits the domain box to the viewport with a wide margin, so most of the
    // frame is background; cropping to the box before scaling spends the GIF's bytes on the
    // grains. The crop is tied to the 900x900 viewport above.
    const filters = 'crop=' + crop + ',scale=' + scale + ':-1:flags=lanczos';
    execFileSync('ffmpeg', ['-y', '-loglevel', 'error', '-framerate', String(fps),
        '-i', pattern, '-vf', filters + ',palettegen=max_colors=64:stats_mode=diff',
        '-frames:v', '1', palette]);
    execFileSync('ffmpeg', ['-y', '-loglevel', 'error', '-framerate', String(fps),
        '-i', pattern, '-i', palette,
        '-lavfi', filters + '[x];[x][1:v]paletteuse=dither=bayer:bayer_scale=4',
        '-loop', '0', out]);
    console.log('wrote ' + out + ' (' + (fs.statSync(out).size / 1e6).toFixed(2) + ' MB)');
    if (!keep) fs.rmSync(frameDir, { recursive: true, force: true });
})();
