// Controlled visual comparison using the same real browser, clip and rectangle.
// Only a separate portable runtime's configuration changes; the wrapper restores it.
const fs = require('node:fs'), path = require('node:path'), http = require('node:http');
const { execFileSync, spawn } = require('node:child_process');
const { chromium } = require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const runtime = process.env.MAGPIE_RUNTIME, output = process.env.MAGPIE_ABLATION_OUTPUT;
if (!runtime || !output) throw new Error('Portable runtime and output are required');
const configPath = path.join(runtime, 'config/v4e/config.json');
const original = fs.readFileSync(configPath);
const config = JSON.parse(original.toString('utf8').replace(/^\uFEFF/, ''));
const modeName = '浏览器视频 30fps → 当前屏幕原生60fps';
const mode = config.scalingModes.find(item => item.name === modeName);
if (!mode) throw new Error('Browser effect mode is missing');
const filter = mode.effects.find(item => item.name === 'FrameRate_Filter');
const vsr = mode.effects.find(item => item.name === 'RTXVideo\\RTXVideo_VSR');
const fg = mode.effects.find(item => item.name === 'XeSSFG\\XeSS_FrameGeneration');
if (!filter || !vsr || !fg) throw new Error('Expected existing VSR/FG effects');
// Match the 4K intermediate image in both spatial upscaling branches.
const lanczos = { name: 'Lanczos', scalingType: 2, scale: { x: 3840, y: 2160 } };
const variants = [
  { name: 'browser-original', effects: null },
  { name: 'lanczos-only', effects: [filter, lanczos] },
  { name: 'vsr-only', effects: [filter, vsr] },
  { name: 'lanczos-fg', effects: [filter, lanczos, fg] },
  { name: 'vsr-fg', effects: [filter, vsr, fg] },
  { name: 'browser-original-repeat', effects: null }
];
const result = { status: 'running', startedAt: new Date().toISOString(), runtime,
  buildCommit: JSON.parse(fs.readFileSync(path.join(runtime, 'build-manifest.json'))).commit,
  stages: [], limitations: ['Screen image sampling, no displayed-FPS measurement in this run.'] };
const save = () => fs.writeFileSync(path.join(output, 'result.json'), JSON.stringify(result, null, 2));
const wait = ms => new Promise(resolve => setTimeout(resolve, ms));
function ps(script, args = []) {
  return JSON.parse(execFileSync('pwsh.exe', ['-NoProfile', '-File', path.join(__dirname, script), ...args],
    { encoding: 'utf8', windowsHide: true, timeout: 30000 }));
}
const probe = (action = 'state', args = []) => ps('BrowserVideoDisplayProbe.ps1', ['-Action', action, ...args]);
const control = action => ps('BrowserVideoSmokeControl.ps1', ['-Action', action, '-RuntimeDirectory', runtime]);
const readLog = start => ['magpie.1.log', 'magpie.log'].map(name => {
  const file = path.join(runtime, 'logs', name); return fs.existsSync(file) ? fs.readFileSync(file, 'utf8') : '';
}).join('\n').split('\n').filter(line => line.slice(0, 19) >= start).join('\n');
const localStamp = () => new Date(Date.now() - new Date().getTimezoneOffset() * 60000).toISOString().replace('T', ' ').slice(0, 19);
function observe(file) {
  return new Promise((resolve, reject) => {
    const child = spawn('pwsh.exe', ['-NoProfile', '-File', path.join(__dirname, 'BrowserVideoDisplayProbe.ps1'),
      '-Action', 'observe', '-Seconds', '5', '-OutputPath', file], { windowsHide: true });
    let stdout = '', stderr = '';
    child.stdout.on('data', data => stdout += data); child.stderr.on('data', data => stderr += data);
    const timer = setTimeout(() => { child.kill(); reject(new Error('Observation timed out')); }, 15000);
    child.on('error', error => { clearTimeout(timer); reject(error); });
    child.on('close', code => { clearTimeout(timer); code === 0 ? resolve(JSON.parse(stdout)) : reject(new Error(stderr)); });
  });
}
(async () => {
  let context, server, worker;
  const errors = [];
  try {
    fs.mkdirSync(output, { recursive: true });
    const extension = path.join(output, 'extension'); fs.mkdirSync(extension, { recursive: true });
    for (const name of fs.readdirSync(path.join(__dirname, '../browser-extension'))) {
      let data = fs.readFileSync(path.join(__dirname, '../browser-extension', name));
      if (name === 'service-worker.js') data = Buffer.from(data.toString().replace('com.magpie.browser_video', 'com.magpie.browser_video_decoded_test'));
      fs.writeFileSync(path.join(extension, name), data);
    }
    const monitor = probe('monitors').find(item => item.Device.endsWith('DISPLAY1'));
    if (!monitor || monitor.Monitor.Right - monitor.Monitor.Left !== 3840 || monitor.Monitor.Bottom - monitor.Monitor.Top !== 2160) throw new Error('Expected existing 4K monitor');
    result.monitor = monitor;
    server = http.createServer((req, res) => { res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' }); res.end(fs.readFileSync(path.join(__dirname, 'browser-decoded-1080p30.html'))); });
    await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
    context = await chromium.launchPersistentContext(path.join(output, 'edge-profile'), {
      executablePath: 'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe', headless: false, viewport: null, chromiumSandbox: true,
      args: [`--disable-extensions-except=${extension}`, `--load-extension=${extension}`, '--window-size=1100,800', '--window-position=100,80', '--autoplay-policy=no-user-gesture-required']
    });
    const page = context.pages()[0];
    page.on('pageerror', error => errors.push(error.message));
    await page.goto(`http://127.0.0.1:${server.address().port}/`, { waitUntil: 'domcontentloaded' });
    await page.waitForFunction(() => document.title.includes('[READY]'), null, { timeout: 60000 });
    worker = context.serviceWorkers().find(item => item.url().includes('peflachlahjaaldgfbjkiniphnplmhgp')) || await context.waitForEvent('serviceworker', { timeout: 15000 });
    await worker.evaluate(() => chrome.storage.local.set({ enabled: false }));
    probe('move', ['-Device', monitor.Device]); await page.bringToFront(); ps('BrowserFocusWindow.ps1', ['-Action', 'focusBrowser']);
    // An integer physical-pixel 16:9 rectangle prevents fractional CSS fitting
    // from trimming a source row and changing one branch's internal resolution.
    await page.evaluate(() => {
      const video = document.querySelector('video'), dpr = window.devicePixelRatio;
      video.style.cssText = `position:fixed;left:${48/dpr}px;top:${48/dpr}px;width:${2880/dpr}px;height:${1620/dpr}px;object-fit:contain`;
    });
    await wait(2000);
    const until = async (check, label) => {
      const deadline = Date.now() + 40000;
      while (Date.now() < deadline) { if (await check()) return; await wait(500); }
      throw new Error(label + ': ' + JSON.stringify({ native: probe(), extension: await worker.evaluate(() => chrome.storage.local.get(null)) }));
    };
    const disable = async () => {
      await worker.evaluate(() => chrome.storage.local.set({ enabled: false }));
      await until(() => !probe().overlayVisible, 'Overlay must close');
      await wait(500); control('quit');
    };
    const enable = async effects => {
      mode.effects = effects; fs.writeFileSync(configPath, JSON.stringify(config, null, 2));
      await worker.evaluate(() => chrome.storage.local.set({ enabled: true, lastError: '' }));
      await until(() => { const state = probe(); return state.overlayVisible && state.foreground === state.browser; }, 'Enhancement must become visible');
      await wait(6000);
    };
    // Use the real overlay to define one fixed physical-pixel sample rectangle.
    console.log('Calibrating a fixed video rectangle');
    await enable([filter, lanczos]);
    const region = probe().overlayRect; result.videoRegion = region; save();
    await disable();
    for (const variant of variants) {
      console.log('Comparing ' + variant.name);
      const stage = { name: variant.name, effects: variant.effects, startedAt: new Date().toISOString(), logStart: localStamp() };
      result.stages.push(stage); save();
      if (variant.effects) await enable(variant.effects); else await wait(1500);
      stage.initial = probe();
      if (stage.initial.foreground !== stage.initial.browser || Boolean(stage.initial.overlayVisible) !== Boolean(variant.effects)) throw new Error('Unexpected foreground or overlay');
      if (variant.effects && JSON.stringify(stage.initial.overlayRect) !== JSON.stringify(region)) throw new Error('Comparison geometry changed');
      stage.observation = await observe(path.join(output, variant.name + '-windows.json'));
      if (stage.observation.foregroundLoss || (variant.effects && stage.observation.overlayMissing)) throw new Error('Observation interrupted');
      probe('capture', ['-OutputPath', path.join(output, variant.name + '.png')]);
      probe('burst', ['-OutputPath', path.join(output, variant.name + '-text'), '-RegionJson', JSON.stringify(region), '-BurstCount', '90']);
      const samples = JSON.parse(fs.readFileSync(path.join(output, variant.name + '-text/samples.json'), 'utf8').replace(/^\uFEFF/, ''));
      if (samples.samples.some(item => Boolean(item.overlayVisible) !== Boolean(variant.effects))) throw new Error('Overlay changed during text samples');
      stage.sampleSeconds = samples.samples.at(-1).ms / 1000;
      stage.final = probe(); stage.endedAt = new Date().toISOString();
      await disable();
      const logs = readLog(stage.logStart); fs.writeFileSync(path.join(output, variant.name + '.log'), logs);
      stage.route = {
        decodedInput: logs.includes('Browser decoded source initialized: 1920x1080'),
        vsrInitialized: logs.includes('RTX Video VSR initialized:'),
        vsrDraw: logs.includes('RTX Video VSR Draw succeeded'),
        fgInitialized: logs.includes('XeSSFG initialized:'),
        fgOutput: logs.includes('XeSSFG SDK output:')
      };
      if (variant.effects && (!stage.route.decodedInput || stage.route.vsrInitialized !== variant.effects.includes(vsr) || stage.route.fgInitialized !== variant.effects.includes(fg))) throw new Error('Runtime route does not match requested effects: ' + JSON.stringify(stage.route));
      save(); console.log('Captured ' + variant.name);
    }
    result.status = 'captured';
  } catch (error) { result.status = 'failed'; result.error = error.stack; process.exitCode = 1; console.error(error); }
  finally {
    if (worker) await worker.evaluate(() => chrome.storage.local.set({ enabled: false })).catch(error => errors.push(error.message));
    try { control('quit'); } catch (error) { errors.push(error.message); }
    await context?.close().catch(error => errors.push(error.message)); server?.close();
    fs.writeFileSync(configPath, original); result.finishedAt = new Date().toISOString(); result.errors = errors; save();
    console.log(JSON.stringify({ status: result.status, output, error: result.error }));
  }
})();
