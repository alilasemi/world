// Capture a single still of the live WebGL2 client, headlessly.
//
// Same gating trick as scripts/capture_gif.cjs: the client's "run" requests are held so the
// simulation advances a known number of frames before the screenshot is taken, which makes
// the still reproducible rather than dependent on how fast the software rasterizer happens
// to be.
//
// Usage:
//   ./build/bin/world demos/grain_detail.yaml &
//   (cd client && node index.js local &)
//   node scripts/capture_screenshot.cjs assets/screenshots/out.png 40 [--grid]

const { execFileSync } = require('child_process');
const fs = require('fs');
const path = require('path');

const globalModules = execFileSync('npm', ['root', '-g'], { encoding: 'utf8' }).trim();
const { chromium } = require(path.join(globalModules, 'playwright'));

const out = process.argv[2];
const frames = parseInt(process.argv[3] || '40', 10);
const grid = process.argv.includes('--grid');
const width = parseInt(process.env.CAPTURE_WIDTH || '900', 10);
const height = parseInt(process.env.CAPTURE_HEIGHT || '900', 10);

if (!out) {
    console.error('usage: capture_screenshot.cjs <out.png> [frames] [--grid]');
    process.exit(1);
}

const readSimTime = () => {
    const hud = document.getElementById('hud');
    if (!hud) return null;
    const m = hud.textContent.match(/Sim time:\s*([0-9.]+)/);
    return m ? parseFloat(m[1]) : null;
};

(async () => {
    const browser = await chromium.launch({
        channel: 'chrome',
        args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader'],
    });
    const page = await browser.newPage({ viewport: { width, height } });
    page.on('pageerror', (e) => console.error('pageerror:', e.message));

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
    await page.addStyleTag({ content: '#hud, #controls { visibility: hidden; }' });
    await page.locator('canvas').waitFor({ timeout: 60000 });
    await page.waitForFunction(() => window.__held && window.__held.length > 0,
        null, { timeout: 120000, polling: 100 });
    // Set the property directly: the controls are hidden for the capture, and a hidden
    // checkbox cannot be clicked.
    if (grid) await page.evaluate(() => { document.getElementById('gridButton').checked = true; });

    let previous = null;
    for (let i = 0; i < frames; ++i) {
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
    }
    fs.mkdirSync(path.dirname(out), { recursive: true });
    await page.screenshot({ path: out, timeout: 120000 });
    console.log(`wrote ${out} at t = ${previous.toFixed(3)} s`);
    await browser.close();
})();
