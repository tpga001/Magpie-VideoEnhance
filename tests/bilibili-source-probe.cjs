// Bounded real-site probe. Uses a disposable Edge profile and records timing
// metadata only; it never saves video pixels, URLs of media segments or cookies.
const fs = require('node:fs');
const path = require('node:path');
const { chromium } = require(process.env.PLAYWRIGHT_MODULE || 'playwright');

const runtime = process.env.MAGPIE_RUNTIME;
const output = process.env.MAGPIE_PROBE_OUTPUT;
const browserExecutable = process.env.MAGPIE_TEST_BROWSER_EXECUTABLE;
const seconds = Number(process.env.MAGPIE_PROBE_SECONDS || 55);
if (!runtime || !output || !browserExecutable || !Number.isInteger(seconds) || seconds < 30 || seconds > 90) {
  throw new Error('Runtime, output, Edge executable and 30-90 seconds are required');
}
fs.mkdirSync(output, { recursive: true });
const extension = path.join(output, 'extension');
fs.mkdirSync(extension, { recursive: true });
for (const name of fs.readdirSync(path.join(__dirname, '../browser-extension'))) {
  let data = fs.readFileSync(path.join(__dirname, '../browser-extension', name));
  if (name === 'service-worker.js') data = Buffer.from(data.toString().replace(
    'com.magpie.browser_video', 'com.magpie.browser_video_decoded_test'));
  if (name === 'content.js') {
    const marker = '  function clippedRect(element) {';
    if (!data.toString().includes(marker)) throw new Error('Test stats injection marker is missing');
    data = Buffer.from(data.toString().replace(marker,
      '  chrome.runtime.onMessage.addListener(message => { if (message.type === "magpie-test-stats") return Promise.resolve({ stats: frames?.stats || null, state: frames?.state || null }); });\n' + marker));
  }
  fs.writeFileSync(path.join(extension, name), data);
}

const wait = ms => new Promise(resolve => setTimeout(resolve, ms));
const result = { status: 'running', startedAt: new Date().toISOString(), seconds,
  site: 'https://www.bilibili.com/video/BV1YDem6VEzC/', runtime, samples: [] };
const save = () => fs.writeFileSync(path.join(output, 'result.json'), JSON.stringify(result, null, 2));
const delta = (before, after, key) => (after?.[key] ?? 0) - (before?.[key] ?? 0);
let context, worker;
const snapshotPage = page => page.evaluate(() => {
  const video = document.querySelector('video');
  if (!video) return null;
  const quality = video.getVideoPlaybackQuality?.();
  return { at: performance.now(), currentTime: video.currentTime, paused: video.paused,
    ended: video.ended, playbackRate: video.playbackRate, readyState: video.readyState,
    width: video.videoWidth, height: video.videoHeight,
    quality: quality && { total: quality.totalVideoFrames, dropped: quality.droppedVideoFrames },
    callbackSamples: window.magpieProbeFrames?.length || 0 };
});
const snapshotRelay = () => worker.evaluate(async () => {
  const [tab] = await chrome.tabs.query({ active: true, lastFocusedWindow: true });
  const windowState = await chrome.windows.getLastFocused();
  const relay = tab && await chrome.tabs.sendMessage(tab.id, { type: 'magpie-test-stats' });
  const storage = await chrome.storage.local.get(['enabled', 'lastError', 'statusNote']);
  return { focused: windowState.focused, relay, storage };
});

(async () => {
  try {
    context = await chromium.launchPersistentContext(path.join(output, 'edge-profile'), {
      executablePath: browserExecutable, headless: false, viewport: null,
      args: [`--disable-extensions-except=${extension}`, `--load-extension=${extension}`,
        '--window-size=1600,1020', '--window-position=120,70', '--autoplay-policy=no-user-gesture-required']
    });
    const page = context.pages()[0] || await context.newPage();
    await page.goto(result.site, { waitUntil: 'domcontentloaded', timeout: 60000 });
    await page.locator('video').first().waitFor({ state: 'attached', timeout: 45000 });
    worker = context.serviceWorkers().find(item => item.url().includes('peflachlahjaaldgfbjkiniphnplmhgp')) ||
      await context.waitForEvent('serviceworker', { timeout: 20000 });
    await worker.evaluate(() => chrome.storage.local.set({ enabled: true }));
    await page.bringToFront();
    await page.locator('video').first().evaluate(async video => { video.muted = true; await video.play(); });
    await page.waitForFunction(() => {
      const video = document.querySelector('video');
      return video && !video.paused && video.readyState >= 2 && video.currentTime > 1;
    }, null, { timeout: 30000 });
    await page.evaluate(() => {
      window.magpieProbeFrames = [];
      const video = document.querySelector('video');
      const sample = (now, metadata) => {
        if (window.magpieProbeFrames.length < 7000) window.magpieProbeFrames.push({
          now, mediaTime: metadata.mediaTime, presentedFrames: metadata.presentedFrames });
        video.requestVideoFrameCallback(sample);
      };
      video.requestVideoFrameCallback(sample);
    });
    const readyDeadline = Date.now() + 30000;
    while (Date.now() < readyDeadline) {
      const state = await snapshotRelay();
      if (state.relay?.stats?.frames > 0) break;
      if (state.storage.lastError || state.relay?.state?.error) throw new Error(
        'Decoded relay unavailable: ' + (state.storage.lastError || state.relay.state.error));
      await wait(500);
    }
    // Magpie's first overlay can briefly take OS focus during startup. Let it
    // settle, then re-establish the Edge foreground before the timed window.
    await wait(6000);
    await page.bringToFront();
    const stableDeadline = Date.now() + 12000;
    let stableFrames = 0;
    while (Date.now() < stableDeadline) {
      await wait(500);
      const state = await snapshotRelay();
      if (state.focused && state.relay?.state?.mapName && state.relay?.stats?.frames > stableFrames) {
        stableFrames = state.relay.stats.frames;
        break;
      }
      if (state.storage.lastError) throw new Error('Decoded relay unavailable after focus recovery: ' + state.storage.lastError);
    }
    result.before = { page: await snapshotPage(page), relay: await snapshotRelay() };
    if (!result.before.relay.relay?.stats?.frames) throw new Error('Real-site decoded relay did not start');
    if (!result.before.relay.focused || !result.before.relay.relay.state?.mapName) throw new Error('Edge/relay did not recover foreground before observation');
    await page.evaluate(() => { window.magpieProbeFrames = []; });
    save();
    const started = Date.now();
    while (Date.now() - started < seconds * 1000) {
      await wait(5000);
      const sample = { page: await snapshotPage(page), relay: await snapshotRelay() };
      result.samples.push(sample);
      save();
      if (!sample.page || sample.page.paused || sample.page.ended) throw new Error('Video stopped during observation');
      if (sample.relay.storage.lastError || sample.relay.relay?.state?.error) throw new Error(
        'Decoded relay failed: ' + (sample.relay.storage.lastError || sample.relay.relay.state.error));
      if (!sample.relay.focused) throw new Error('Edge lost foreground during observation');
    }
    result.after = { page: await snapshotPage(page), relay: await snapshotRelay() };
    result.sourceFrames = await page.evaluate(() => window.magpieProbeFrames || []);
    const frames = result.sourceFrames;
    const mediaMs = frames.length > 1 ? (frames.at(-1).mediaTime - frames[0].mediaTime) * 1000 : 0;
    const frameIntervals = frames.slice(1).map((frame, index) => Math.round(
      (frame.mediaTime - frames[index].mediaTime) * 1000));
    const intervalHistogram = {};
    for (const interval of frameIntervals) intervalHistogram[interval] = (intervalHistogram[interval] || 0) + 1;
    const first = result.before.relay.relay.stats, last = result.after.relay.relay?.stats;
    if (!last || first.startedAt !== last.startedAt) throw new Error('Relay session changed during observation');
    const elapsed = (result.after.page.at - result.before.page.at) / 1000;
    result.analysis = { elapsedSeconds: elapsed, mediaSeconds: mediaMs / 1000,
      callbackCount: frames.length, callbackFps: frames.length / elapsed,
      presentedFrameGap: frames.length > 1 ? frames.at(-1).presentedFrames - frames[0].presentedFrames : 0,
      intervalHistogram,
      browserQuality: { total: result.after.page.quality.total - result.before.page.quality.total,
        dropped: result.after.page.quality.dropped - result.before.page.quality.dropped },
      relay: Object.fromEntries(['callbacks', 'submitted', 'frames', 'skippedPacing', 'skippedCopying',
        'skippedPending', 'skippedSelection', 'sourceCallbackMissed', 'dropped', 'droppedQueue',
        'droppedBackpressure', 'droppedHost'].map(key => [key, delta(first, last, key)])) };
    result.status = 'captured';
  } catch (error) {
    result.status = 'failed';
    result.error = String(error.stack || error);
    try { if (context?.pages()[0]) result.sourceFrames = await context.pages()[0].evaluate(() => window.magpieProbeFrames || []); }
    catch {}
    process.exitCode = 1;
    console.error(error);
  } finally {
    try { if (worker) await worker.evaluate(() => chrome.storage.local.set({ enabled: false })); }
    catch (error) { result.cleanupError = String(error); }
    try { await context?.close(); } catch (error) { result.cleanupError = String(error); }
    result.endedAt = new Date().toISOString();
    save();
    console.log(JSON.stringify({ status: result.status, output, analysis: result.analysis, error: result.error }));
  }
})();
