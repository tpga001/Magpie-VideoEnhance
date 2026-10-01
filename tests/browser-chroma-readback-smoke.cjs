// Real Chromium VideoFrame.copyTo validation; no window activation or runtime changes.
const fs = require('node:fs'), path = require('node:path'), http = require('node:http');
const { execFileSync } = require('node:child_process');
const { chromium } = require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const output = path.resolve(__dirname, '../output/playwright/browser-chroma-readback');
fs.mkdirSync(output, { recursive: true });
(async () => {
  let browser, server;
  try {
    server = http.createServer((req, res) => {
      res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });
      res.end(fs.readFileSync(path.join(__dirname, 'browser-decoded-1080p30.html')));
    });
    await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
    browser = await chromium.launch({ executablePath: 'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
      headless: true, chromiumSandbox: true, args: ['--autoplay-policy=no-user-gesture-required'] });
    const page = await browser.newPage();
    await page.goto(`http://127.0.0.1:${server.address().port}/`, { waitUntil: 'domcontentloaded' });
    await page.waitForFunction(() => document.title.includes('[READY]'), null, { timeout: 60000 });
    const previous = execFileSync('git', ['show', 'HEAD:browser-extension/video-geometry.js'],
      { cwd: path.join(__dirname, '..'), encoding: 'utf8', windowsHide: true });
    await page.addScriptTag({ content: previous.replace('globalThis.MagpieVideoGeometry', 'globalThis.PreviousGeometry') });
    await page.addScriptTag({ path: path.join(__dirname, '../browser-extension/video-geometry.js') });
    await page.addScriptTag({ path: path.join(__dirname, '../browser-extension/frame-packet.js') });
    const result = await page.evaluate(async () => {
      const video = document.querySelector('video');
      await new Promise(resolve => video.requestVideoFrameCallback(resolve));
      const decoded = new VideoFrame(video);
      const fakeBox = { videoWidth: video.videoWidth, videoHeight: video.videoHeight,
        getBoundingClientRect: () => ({ left: 0, top: 0, width: 3840, height: 2160, right: 3840, bottom: 2160 }) };
      const viewport = { innerWidth: 3840, innerHeight: 2159 };
      const before = PreviousGeometry(fakeBox, viewport, {}), after = MagpieVideoGeometry(fakeBox, viewport, {});
      const read = async (frame, geometry) => {
        const start = performance.now();
        const data = await MagpieReadFrame(frame, geometry.crop, 0);
        return { crop: geometry.crop, format: data.format, bytes: data.packet.byteLength, copyMs: performance.now() - start };
      };
      try {
        const decodedInput = { format: decoded.format, matrix: decoded.colorSpace.matrix,
          before: await read(decoded, before), after: await read(decoded, after) };
        // Independently validate NV12 even if this machine's VP8 decoder chooses I420/RGB.
        const bytes = new Uint8Array(1920 * 1080 * 3 / 2); bytes.fill(128);
        const nv12 = new VideoFrame(bytes, { format: 'NV12', codedWidth: 1920, codedHeight: 1080,
          timestamp: 0, colorSpace: { matrix: 'bt709', primaries: 'bt709', transfer: 'bt709', fullRange: false } });
        try {
          const rawNv12 = { before: await read(nv12, before), after: await read(nv12, after) };
          if (rawNv12.before.format !== 'RGBA' || rawNv12.after.format !== 'NV12') throw new Error('NV12 regression');
          if (rawNv12.after.bytes !== 64 + 1920 * 1078 * 3 / 2) throw new Error('Incorrect packet layout');
          if (['NV12', 'I420'].includes(decoded.format) && decodedInput.after.format !== decoded.format) throw new Error('Decoded format was not preserved');
          return { status: 'passed', decodedInput, rawNv12,
            limitation: 'Real copyTo API validation; individual timings are not a displayed-FPS benchmark.' };
        } finally { nv12.close(); }
      } finally { decoded.close(); }
    });
    fs.writeFileSync(path.join(output, 'result.json'), JSON.stringify(result, null, 2));
    console.log(JSON.stringify(result));
  } catch (error) { console.error(error); process.exitCode = 1; }
  finally { await browser?.close(); server?.close(); }
})();
