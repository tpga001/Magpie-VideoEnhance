// Run with a modern Node and PLAYWRIGHT_MODULE pointing to installed Playwright.
// This drives a separate Chrome profile. Browser API transport is simulated;
// content.js, native messaging framing, Magpie capture/VSR/FG and resize are real.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const http = require('node:http');
const { spawn, execFileSync } = require('node:child_process');
const { chromium } = require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const runtime = process.env.MAGPIE_RUNTIME || 'E:\\Program Files\\Magpie-Experimental-x64';
const output = path.resolve(__dirname, '../output/playwright/browser-lifecycle');
fs.mkdirSync(output, { recursive: true });
const logPath = path.join(runtime, 'logs/magpie.log');
const evidence = [];
const sleep = ms => new Promise(resolve => setTimeout(resolve, ms));
const record = (name, details = {}) => {
  const item = { name, at: new Date().toISOString(), ...details };
  evidence.push(item); console.log(JSON.stringify(item));
  fs.writeFileSync(path.join(output, 'result.json'), JSON.stringify(evidence, null, 2));
};
function control(action) {
  return JSON.parse(execFileSync(process.env.POWERSHELL_EXE || 'pwsh.exe', [
    '-NoProfile', '-File', path.join(__dirname, 'BrowserVideoSmokeControl.ps1'),
    '-Action', action, '-RuntimeDirectory', runtime
  ], { encoding: 'utf8', windowsHide: true, timeout: 20000 }));
}
function native(message) {
  return new Promise((resolve, reject) => {
    const proc = spawn(path.join(runtime, 'Magpie.BrowserVideoHost.exe'), [], { windowsHide: true });
    const chunks = [], errors = [];
    const timer = setTimeout(() => { proc.kill(); reject(new Error('Native host timeout')); }, 15000);
    proc.stdout.on('data', bytes => chunks.push(bytes));
    proc.stderr.on('data', bytes => errors.push(bytes));
    proc.on('error', error => { clearTimeout(timer); reject(error); });
    proc.on('close', code => {
      clearTimeout(timer);
      try {
        const bytes = Buffer.concat(chunks);
        assert.ok(bytes.length >= 4, `native host empty reply (${code}): ${Buffer.concat(errors)}`);
        assert.equal(bytes.readUInt32LE(0), bytes.length - 4);
        const response = JSON.parse(bytes.subarray(4).toString());
        assert.equal(response.ok, true, JSON.stringify(response)); resolve(response);
      } catch (error) { reject(error); }
    });
    const payload = Buffer.from(JSON.stringify(message));
    const size = Buffer.alloc(4); size.writeUInt32LE(payload.length);
    proc.stdin.end(Buffer.concat([size, payload]));
  });
}
const readLog = () => fs.readFileSync(logPath, 'utf8');
async function until(check, description, timeout = 30000) {
  const deadline = Date.now() + timeout;
  while (Date.now() < deadline) { if (await check()) return; await sleep(250); }
  throw new Error(`Timeout: ${description}`);
}

(async () => {
  assert.equal(control('state').running, false, 'Close the target Magpie runtime before this isolated smoke test');
  for (let i = 0; i < 3; i++) {
    assert.equal((await native({ action: 'update', sessionId: 'smoke' })).state, 'stopped');
  }
  assert.equal(control('state').running, false);
  record('PASS passive updates do not launch Magpie');
  let browser, server, queue = Promise.resolve(), accepting = true, lastCandidate, bridgeError;
  const replies = [];
  try {
    assert.equal((await native({ action: 'enable' })).state, 'running');
    record('PASS explicit enable launches Magpie', control('state'));
    server = http.createServer((req, res) => {
      res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });
      res.end(fs.readFileSync(path.join(__dirname, 'browser-video-1080p30.html')));
    });
    await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
    browser = await chromium.launch({
      executablePath: process.env.CHROME_EXE || 'C:/Program Files/Google/Chrome/Application/chrome.exe',
      headless: false, args: ['--window-size=1100,800', '--window-position=30,30', '--autoplay-policy=no-user-gesture-required']
    });
    const context = await browser.newContext({ viewport: null });
    const page = await context.newPage();
    await page.exposeFunction('magpieSmokeReport', message => {
      if (!accepting || !message.candidate) return;
      lastCandidate = message.candidate;
      const candidate = message.candidate;
      queue = queue.then(async () => {
        if (!accepting) return;
        const response = await native({ action: 'update', sessionId: 'chromium:smoke:resize', ...candidate });
        replies.push(response);
      }).catch(error => { bridgeError = error; });
    });
    await page.goto(`http://127.0.0.1:${server.address().port}/`);
    await page.waitForFunction(() => document.title.includes('[READY]'), null, { timeout: 30000 });
    await page.evaluate(() => {
      window.chrome = {
        storage: { local: { get: async () => ({ enabled: true }) } },
        runtime: { onMessage: { addListener() {} }, sendMessage: value => window.magpieSmokeReport(value) }
      };
      // Reproduce danmaku/player styles changing faster than the old debounce.
      const status = document.querySelector('#status');
      let tick = 0;
      window.smokeMutations = setInterval(() => { status.className = `tick-${++tick % 2}`; }, 20);
    });
    let offset = readLog().length;
    await page.addScriptTag({ content: fs.readFileSync(path.join(__dirname, '../browser-extension/content.js'), 'utf8') });
    async function expectGeometry(name) {
      await until(() => {
        if (bridgeError) throw bridgeError;
        if (!lastCandidate) return false;
        const rect = lastCandidate.rect, scale = lastCandidate.viewport.devicePixelRatio;
        const width = Math.round((rect.left + rect.width) * scale) - Math.round(rect.left * scale);
        const height = Math.round((rect.top + rect.height) * scale) - Math.round(rect.top * scale);
        const lines = readLog().slice(offset);
        const target = [...lines.matchAll(/目标矩形:.*\((\d+)x(\d+)\)/g)].at(-1);
        // Client-edge clipping rounds fractional-DPI bounds by up to two pixels.
        return target && Math.abs(Number(target[1]) - width) <= 2 && Math.abs(Number(target[2]) - height) <= 2 &&
          lines.slice(target.index).includes('RTX Video VSR Draw succeeded') &&
          lines.slice(target.index).includes('XeSSFG SDK output:');
      }, name);
      const lines = readLog().slice(offset);
      const sizes = [...lines.matchAll(/overlay=(\d+x\d+) processing=(\d+x\d+)/g)];
      record(name, { viewport: lastCandidate.viewport, rect: lastCandidate.rect,
        overlay: sizes.at(-1)?.[1], processing: sizes.at(-1)?.[2] });
      fs.writeFileSync(path.join(output, `${evidence.length}-runtime.log`), lines);
    }
    await expectGeometry('PASS initial real VSR/FG output');
    const cdp = await context.newCDPSession(page);
    const { windowId } = await cdp.send('Browser.getWindowForTarget');
    for (const [width, height] of [[850, 650], [1280, 920]]) {
      offset = readLog().length;
      const before = lastCandidate.viewport.innerWidth;
      await cdp.send('Browser.setWindowBounds', { windowId, bounds: { width, height, windowState: 'normal' } });
      await until(() => lastCandidate.viewport.innerWidth !== before, 'fresh browser viewport report');
      await expectGeometry(`PASS window resize ${width}x${height}`);
    }
    offset = readLog().length;
    const previousWidth = lastCandidate.rect.width;
    await page.evaluate(() => {
      const video = document.querySelector('video'); video.style.width = '75%'; video.style.height = '75%';
    });
    await until(() => lastCandidate.rect.width !== previousWidth, 'video-only resize report');
    await expectGeometry('PASS player-only resize during continuous DOM mutations');
    await page.screenshot({ path: path.join(output, 'browser-video-layout.png') });
    control('quit');
    const beforeReplies = replies.length;
    await until(() => replies.slice(beforeReplies).filter(reply => reply.state === 'stopped').length >= 3,
      'three passive reports after manual exit', 15000);
    assert.equal(control('state').running, false);
    record('PASS graceful exit stays stopped under repeated live video reports', { reports: replies.length - beforeReplies });
    offset = readLog().length;
    assert.equal((await native({ action: 'enable' })).state, 'running');
    await expectGeometry('PASS explicit re-enable restores current player size');
  } finally {
    accepting = false;
    if (browser) await browser.close();
    await queue;
    if (server) await new Promise(resolve => server.close(resolve));
    if (control('state').running) control('quit');
  }
  record('PASS cleanup leaves Magpie stopped');
})().catch(error => { record('FAIL', { error: error.stack }); process.exitCode = 1; });
