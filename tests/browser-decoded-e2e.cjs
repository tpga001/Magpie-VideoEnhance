// Production extension scripts + real native messaging + real GPU processing.
// Only Chrome's extension API transport is shimmed in an isolated test profile.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const http = require('node:http');
const { spawn, execFileSync } = require('node:child_process');
const { chromium } = require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const runtime = process.env.MAGPIE_RUNTIME;
const output = path.resolve(__dirname, '../output/playwright/browser-decoded');
const logPath = path.join(runtime, 'logs/magpie.log');
fs.mkdirSync(output, { recursive: true });
const event = () => { const listeners = []; return { addListener: fn => listeners.push(fn), emit: (...args) => Promise.all(listeners.map(fn => fn(...args))) }; };
const wait = ms => new Promise(resolve => setTimeout(resolve, ms));
const evidence = [], ports = new Set(), requests = [];
const relayConnections = new Map();
const protocolEvents = [];
let page, browser, server;
const record = (name, details) => {
  evidence.push({ name, ...details }); console.log(JSON.stringify(evidence.at(-1)));
  fs.writeFileSync(path.join(output, 'e2e-result.json'), JSON.stringify(evidence, null, 2));
};
function control(action) {
  return JSON.parse(execFileSync('pwsh.exe', ['-NoProfile', '-File', path.join(__dirname, 'BrowserVideoSmokeControl.ps1'),
    '-Action', action, '-RuntimeDirectory', runtime], { encoding: 'utf8', windowsHide: true, timeout: 20000 }));
}
function connectNative() {
  const child = spawn(path.join(runtime, 'Magpie.BrowserVideoHost.exe'), [], { windowsHide: true });
  let bytes = Buffer.alloc(0);
  const port = { onMessage: event(), onDisconnect: event(),
    postMessage(message) {
      const payload = Buffer.from(JSON.stringify(message)), size = Buffer.alloc(4); size.writeUInt32LE(payload.length);
      child.stdin.write(Buffer.concat([size, payload]));
    }, disconnect() { child.stdin.end(); }, child
  };
  child.stdout.on('data', chunk => {
    bytes = Buffer.concat([bytes, chunk]);
    while (bytes.length >= 4 && bytes.length >= bytes.readUInt32LE(0) + 4) {
      const length = bytes.readUInt32LE(0);
      const message = JSON.parse(bytes.subarray(4, 4 + length)); bytes = bytes.subarray(4 + length);
      if (message.state === 'frames') relayConnections.set(message.mapName, message);
      if (message.error || message.state === 'frames') protocolEvents.push({ at: Date.now(), state: message.state, error: message.error });
      port.onMessage.emit(message).catch(console.error);
    }
  });
  child.on('exit', () => { ports.delete(port); port.onDisconnect.emit().catch(console.error); });
  child.on('error', console.error);
  ports.add(port); return port;
}
async function native(message) {
  requests.push(message);
  return new Promise((resolve, reject) => {
    const port = connectNative();
    const timeout = setTimeout(() => { port.child.kill(); reject(new Error('Native command timeout')); }, 15000);
    port.onMessage.addListener(response => { clearTimeout(timeout); resolve(response); });
    port.postMessage(message); port.child.stdin.end();
  });
}
const changes = event(), state = { enabled: false }, session = {};
const area = (values, name) => ({
  get: async defaults => ({ ...defaults, ...values }),
  remove: async key => { delete values[key]; },
  set: async patch => {
    const delta = {};
    for (const [key, value] of Object.entries(patch)) {
      if (values[key] !== value) delta[key] = { oldValue: values[key], newValue: value };
      values[key] = value;
    }
    if (Object.keys(delta).length) await changes.emit(delta, name);
  }
});
const chrome = {
  runtime: { onMessage: event(), connectNative, sendNativeMessage: (_, message) => native(message) },
  storage: { local: area(state, 'local'), session: area(session, 'session'), onChanged: changes },
  action: { setBadgeBackgroundColor: async () => {}, setBadgeText: async () => {} },
  tabs: { onActivated: event(), query: async () => [{ id: 7, windowId: 3, active: true }],
    sendMessage: async (_, message) => { if (page) await page.evaluate(message => window.contentMessages?.forEach(fn => fn(message)), message); } },
  windows: { onFocusChanged: event(), WINDOW_ID_NONE: -1, getLastFocused: async () => ({ focused: true, tabs: [{ id: 7, active: true }] }) }
};
const log = () => fs.existsSync(logPath) ? fs.readFileSync(logPath, 'utf8') : '';
async function until(check, description, timeout = 45000) {
  const deadline = Date.now() + timeout;
  while (Date.now() < deadline) { if (await check()) return; await wait(250); }
  throw new Error(`${description}; state=${JSON.stringify(state)}; log=${log().slice(-2500)}`);
}
(async () => {
  assert.equal(control('state').running, false);
  let offset = log().length;
  try {
    vm.runInNewContext(fs.readFileSync(path.join(__dirname, '../browser-extension/service-worker.js'), 'utf8'), { chrome, Date });
    server = http.createServer((req, res) => {
      const resource = new URL(req.url, 'http://localhost');
      if (resource.pathname === '/test-relay-connection') { res.writeHead(200, { 'Content-Type': 'application/json' }); res.end(JSON.stringify(relayConnections.get(resource.searchParams.get('map')) || null)); return; }
      if (resource.pathname === '/test-relay-shim.js') { res.writeHead(200, { 'Content-Type': 'application/javascript' });
        res.end(`window.chrome={runtime:{getURL:name=>new URL(name,location.href).href,sendMessage:message=>fetch('/test-relay-connection?map='+encodeURIComponent(message.mapName)).then(r=>r.json())}};`); return; }
      if (['/frame-relay.html','/frame-relay.js','/frame-worker.js'].includes(resource.pathname)) {
        const filename = resource.pathname.slice(1);
        let content = fs.readFileSync(path.join(__dirname, '../browser-extension', filename), 'utf8');
        if (filename.endsWith('.html')) content = content.replace('<script src=', '<script src="test-relay-shim.js"></script><script src=');
        res.writeHead(200, { 'Content-Type': filename.endsWith('.html') ? 'text/html' : 'application/javascript' }); res.end(content); return;
      }
      res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });
      res.end(fs.readFileSync(path.join(__dirname, 'browser-decoded-1080p30.html')));
    });
    await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
    browser = await chromium.launch({ executablePath: 'C:/Program Files/Google/Chrome/Application/chrome.exe', headless: false,
      args: ['--window-size=1100,800', '--window-position=80,80', '--autoplay-policy=no-user-gesture-required'] });
    const context = await browser.newContext({ viewport: null });
    page = await context.newPage();
    await page.exposeFunction('magpieReport', message => chrome.runtime.onMessage.emit(message, { tab: { id: 7, windowId: 3 } }));
    await page.goto(`http://127.0.0.1:${server.address().port}/`);
    await page.waitForFunction(() => document.title.includes('[READY]'), null, { timeout: 30000 });
    await page.evaluate(() => {
      window.contentMessages = [];
      window.chrome = { storage: { local: { get: async () => ({ enabled: false }) } }, runtime: {
        getURL: name => new URL(name, location.href).href,
        onMessage: { addListener: fn => contentMessages.push(fn) }, sendMessage: message => window.magpieReport(message)
      } };
    });
    for (const file of ['video-geometry.js', 'frame-packet.js', 'frame-relay-client.js', 'decoded-frames.js'])
      await page.addScriptTag({ path: path.join(__dirname, '../browser-extension', file) });
    await page.evaluate(() => { const Decoder = MagpieDecodedFrames; globalThis.MagpieDecodedFrames = class extends Decoder {
      constructor(...args) { super(...args); window.testDecoder = this; }
    }; });
    await page.addScriptTag({ path: path.join(__dirname, '../browser-extension/content.js') });
    await chrome.storage.local.set({ enabled: true });
    await until(() => log().slice(offset).includes('Browser decoded source initialized: 1920x1080') &&
      log().slice(offset).includes('RTX Video VSR Draw succeeded') && log().slice(offset).includes('XeSSFG SDK output:'), 'Original frame did not reach VSR/FG');
    record('PASS decoded 1080p reaches real VSR and frame generation', {
      transport: await page.evaluate(() => ({ ...testDecoder.stats, elapsedMs: performance.now() - testDecoder.stats.startedAt, visibility: document.visibilityState })),
      source: requests.filter(x => x.frameSource).at(-1)?.frameSource,
      vsr: log().slice(offset).split('\n').filter(x => /VSR initialized|Browser video auto mode:/.test(x))
    });
    await until(() => log().slice(offset).includes('Browser decoded frames uploaded: frames=300'), '300 original frames were not processed', 60000);
    record('PASS 300 decoded frames uploaded', {});
    const cdp = await context.newCDPSession(page);
    const { windowId } = await cdp.send('Browser.getWindowForTarget');
    offset = log().length;
    await cdp.send('Browser.setWindowBounds', { windowId, bounds: { width: 850, height: 650, windowState: 'normal' } });
    await until(() => log().slice(offset).includes('Browser decoded source initialized: 1920x1080') &&
      log().slice(offset).includes('RTX Video VSR Draw succeeded'), 'Resize did not rebuild decoded-frame rendering');
    record('PASS window resize preserves original 1080p input', { geometry: requests.filter(x => x.frameSource).at(-1)?.rect });
    offset = log().length;
    await page.evaluate(() => document.querySelector('video').pause());
    await until(() => log().slice(offset).includes('Browser video auto mode stopped'), 'Pause did not remove overlay');
    record('PASS pause removes the overlay', {});
    offset = log().length;
    await page.evaluate(() => document.querySelector('video').play());
    await until(() => log().slice(offset).includes('RTX Video VSR Draw succeeded'), 'Resume failed');
    record('PASS resume restores decoded-frame processing', {});
    control('quit');
    await until(() => state.enabled === false, 'Magpie exit did not turn off the extension', 15000);
    assert.equal(control('state').running, false);
    record('PASS exit does not relaunch Magpie and closes frame bridge', {});
    fs.writeFileSync(path.join(output, 'e2e-runtime.log'), log());
  } finally {
    fs.writeFileSync(path.join(output, 'protocol-events.json'), JSON.stringify({ protocolEvents, requests }, null, 2));
    await chrome.storage.local.set({ enabled: false }).catch(() => {});
    for (const port of ports) { port.disconnect(); port.child.kill(); }
    await browser?.close(); server?.close();
    if (control('state').running) control('quit');
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
