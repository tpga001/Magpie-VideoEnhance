// Uses an isolated test browser. No user tabs, profiles, or playback state change.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const http = require('node:http');
const { spawn } = require('node:child_process');
const { chromium } = require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const hostPath = process.env.FRAME_HOST;
const output = path.resolve(__dirname, '../output/playwright/browser-decoded');
fs.mkdirSync(output, { recursive: true });
const wait = ms => new Promise(resolve => setTimeout(resolve, ms));
function frameHost(pageUrl) {
  const child = spawn(hostPath, [], { windowsHide: true });
  const ready = new Promise((resolve, reject) => {
    let buffer = Buffer.alloc(0);
    child.stdout.on('data', chunk => {
      buffer = Buffer.concat([buffer, chunk]);
      if (buffer.length >= 4 && buffer.length >= buffer.readUInt32LE(0) + 4) {
        const reply = JSON.parse(buffer.subarray(4, 4 + buffer.readUInt32LE(0)).toString());
        if (!reply.ok || reply.state !== 'frames') reject(new Error(JSON.stringify(reply))); else resolve(reply);
      }
    });
    child.on('error', reject);
  });
  const payload = Buffer.from(JSON.stringify({ action: 'frames', pageUrl }));
  const length = Buffer.alloc(4); length.writeUInt32LE(payload.length);
  child.stdin.write(Buffer.concat([length, payload]));
  return { child, ready };
}
(async () => {
  let server, browser, host, relayConnection;
  const evidence = [];
  try {
    server = http.createServer((req, res) => {
      const resource = new URL(req.url, 'http://localhost');
      if (resource.pathname === '/test-relay-connection') { res.writeHead(200, { 'Content-Type': 'application/json' }); res.end(JSON.stringify(relayConnection)); return; }
      if (resource.pathname === '/test-relay-shim.js') { res.writeHead(200, { 'Content-Type': 'application/javascript' });
        res.end(`window.chrome={runtime:{getURL:name=>new URL(name,location.href).href,sendMessage:()=>fetch('/test-relay-connection').then(r=>r.json())}};`); return; }
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
    const url = `http://127.0.0.1:${server.address().port}/`;
    host = frameHost(url);
    const connection = await host.ready;
    relayConnection = connection;
    browser = await chromium.launch({ executablePath: 'C:/Program Files/Google/Chrome/Application/chrome.exe',
      headless: false, args: ['--window-size=1100,800', '--window-position=80,80', '--autoplay-policy=no-user-gesture-required'] });
    const page = await browser.newPage({ viewport: null });
    await page.goto(url);
    await page.waitForFunction(() => document.title.includes('[READY]'), null, { timeout: 30000 });
    if (process.env.IPC_ONLY) {
      console.log(JSON.stringify(await page.evaluate(connection => new Promise((resolve, reject) => {
        const socket = new WebSocket(connection.url), buffer = new ArrayBuffer(64 + 1920*1080*3/2), header = new DataView(buffer);
        [0x31465642,1,1920,1080,1920,1920*1080*3/2].forEach((x,i)=>header.setUint32(i*4,x,true));
        header.setUint32(48,1,true);header.setUint32(52,1,true);
        let count=0, start=0;
        const send = () => socket.send(connection.blob ? new Blob([buffer]) : buffer);
        const timer=setTimeout(()=>reject(new Error('Browser IPC timeout')),20000);
        socket.onopen=()=>{start=performance.now();send();};
        socket.onerror=reject;
        socket.onmessage=()=>{if(++count===60){clearTimeout(timer);resolve({frames:count,elapsedMs:performance.now()-start});socket.close();}else send();};
      }), { ...connection, blob: Boolean(process.env.IPC_BLOB) })));
      return;
    }
    for (const file of ['video-geometry.js', 'frame-packet.js', 'frame-relay-client.js', 'decoded-frames.js'])
      await page.addScriptTag({ path: path.join(__dirname, '../browser-extension', file) });
    await page.evaluate(() => { window.chrome = { runtime: { getURL: name => new URL(name, location.href).href } }; });
    await page.evaluate(connection => {
      window.frameTest = new MagpieDecodedFrames(() => {
        const video = document.querySelector('video');
        return { video, geometry: MagpieVideoGeometry(video, window, getComputedStyle(video)) };
      }, () => {});
      frameTest.start(connection);
    }, connection);
    await page.waitForFunction(() => frameTest.state?.mapName || frameTest.state?.error, null, { timeout: 10000 });
    let state = await page.evaluate(() => ({ ...frameTest.state, time: frameTest.lastTime }));
    assert.equal(state.width, 1920, JSON.stringify(state)); assert.equal(state.height, 1080);
    evidence.push({ check: 'decoded input is 1080p independent of CSS size', width: state.width, height: state.height });
    await wait(10000);
    state = await page.evaluate(() => ({ ...frameTest.state, time: frameTest.lastTime, pending: frameTest.pending,
      stats: frameTest.stats, elapsedMs: performance.now() - frameTest.stats.startedAt }));
    assert.ok(!state.error, JSON.stringify(state));
    evidence.push({ check: 'transport performance without GPU effects', stats: state.stats, elapsedMs: state.elapsedMs });
    console.log(JSON.stringify(evidence.at(-1)));
    fs.writeFileSync(path.join(output, 'transport-result.json'), JSON.stringify(evidence, null, 2));
    await page.evaluate(() => { document.querySelector('video').style.width = '70%'; });
    await wait(2000);
    state = await page.evaluate(() => ({ ...frameTest.state, time: frameTest.lastTime }));
    assert.equal(state.width, 1920); assert.equal(state.height, 1080);
    evidence.push({ check: 'CSS resize keeps original input resolution', width: state.width, height: state.height });
    await page.screenshot({ path: path.join(output, 'transport.png') });
    await page.evaluate(() => frameTest.stop());
    host.child.stdin.end();
    if (host.child.exitCode === null) await Promise.race([new Promise(resolve => host.child.on('exit', resolve)), wait(3000).then(() => { throw new Error('Host did not exit on port disconnect'); })]);
    evidence.push({ check: 'native bridge exits when the browser port disconnects' });
    fs.writeFileSync(path.join(output, 'transport-result.json'), JSON.stringify(evidence, null, 2));
    console.log(JSON.stringify(evidence));
  } finally { host?.child.kill(); await browser?.close(); server?.close(); }
})().catch(error => { console.error(error); process.exitCode = 1; });
