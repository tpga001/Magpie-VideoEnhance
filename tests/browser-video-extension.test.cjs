const assert = require('node:assert/strict');
const { readFileSync } = require('node:fs');
const { join } = require('node:path');
const vm = require('node:vm');
const { test } = require('node:test');

const source = name => readFileSync(join(__dirname, '../browser-extension', name), 'utf8');
const flush = async () => { for (let i = 0; i < 50; i++) await Promise.resolve(); };
const event = () => {
  const listeners = [];
  return { addListener: fn => listeners.push(fn), emit: (...args) => listeners.map(fn => fn(...args)) };
};

function contentHarness(initialEnabled = true) {
  let now = 10000, nextId = 1;
  const timers = new Map(), messages = [], handlers = {}, observers = {};
  const video = {
    paused: false, ended: false, readyState: 4, currentSrc: 'test.webm', currentTime: 1,
    rect: { left: 10, top: 20, right: 810, bottom: 470 },
    getBoundingClientRect() { return this.rect; }
  };
  const setTimer = (fn, delay, interval = false) => {
    const id = nextId++;
    timers.set(id, { fn, at: now + delay, delay, interval });
    return id;
  };
  const document = {
    title: 'Regression video', visibilityState: 'visible', documentElement: {},
    querySelectorAll: () => [video], addEventListener: (name, fn) => { handlers[name] = fn; }
  };
  const window = {
    innerWidth: 1280, innerHeight: 720, outerWidth: 1280, outerHeight: 800,
    devicePixelRatio: 1, screenX: 0, screenY: 0,
    addEventListener: (name, fn) => { handlers[name] = fn; }
  };
  vm.runInNewContext(source('content.js'), {
    window, document, location: { href: 'http://localhost/test' },
    getComputedStyle: () => ({ display: 'block', visibility: 'visible', opacity: '1' }),
    Date: { now: () => now },
    setTimeout: (fn, delay) => setTimer(fn, delay), clearTimeout: id => timers.delete(id),
    setInterval: (fn, delay) => setTimer(fn, delay, true),
    MutationObserver: class { constructor(fn) { observers.mutation = fn; } observe() {} },
    ResizeObserver: class {
      constructor(fn) { observers.resize = fn; }
      observe(target) { observers.target = target; }
      disconnect() { observers.target = null; }
    },
    chrome: {
      storage: { local: { get: async () => ({ enabled: initialEnabled }) } },
      runtime: { onMessage: event(), sendMessage: async value => messages.push({ at: now, value }) }
    }
  });
  return {
    video, messages, handlers, observers, window,
    advance(ms) {
      const end = now + ms;
      for (;;) {
        const due = [...timers].filter(([, timer]) => timer.at <= end).sort((a, b) => a[1].at - b[1].at)[0];
        if (!due) break;
        const [id, timer] = due;
        now = timer.at;
        if (timer.interval) timer.at += timer.delay; else timers.delete(id);
        timer.fn();
      }
      now = end;
    }
  };
}

test('disabled enhancement does not send periodic messages to the worker', async () => {
  const h = contentHarness(false);
  await flush(); h.advance(0);
  const count = h.messages.length;
  for (let i = 0; i < 500; i++) { h.observers.mutation(); h.advance(20); }
  assert.equal(h.messages.length, count);
});

test('continuous page mutations cannot starve video resize reports', async () => {
  const h = contentHarness();
  await flush(); h.advance(0);
  assert.equal(h.messages.at(-1).value.candidate.rect.width, 800);
  assert.equal(h.observers.target, h.video);
  h.video.rect.right = 650;
  h.video.rect.bottom = 380;
  h.handlers.resize();
  for (let i = 0; i < 100; i++) { h.observers.mutation(); h.advance(20); }
  const resized = h.messages.find(x => x.value.candidate.rect.width === 640);
  assert.ok(resized, 'new rectangle delivered despite 50 mutations/second');
  assert.ok(resized.at - h.messages[0].at <= 200, 'bounded delivery delay');
  assert.equal(resized.value.candidate.rect.height, 360);
});

test('video-only layout changes and unchanged-geometry heartbeats are delivered', async () => {
  const h = contentHarness();
  await flush(); h.advance(0);
  h.video.rect.right = 970;
  h.observers.resize(); h.advance(200);
  assert.equal(h.messages.at(-1).value.candidate.rect.width, 960);
  const count = h.messages.length;
  h.advance(4000);
  assert.ok(h.messages.length > count, 'heartbeat survives unchanged DOM geometry');
});

function workerHarness(initialEnabled = true) {
  let now = 10000, nextTimer = 1;
  const timers = new Map();
  let focused = true, activeTabId = 7;
  const changes = event(), calls = [], sent = [];
  const state = { enabled: initialEnabled };
  let native = async msg => ({ ok: true, state: 'running', delivered: msg.action !== 'enable' });
  const area = (values, name) => ({
    get: async defaults => ({ ...defaults, ...values }),
    set: async patch => {
      const delta = {};
      for (const [key, value] of Object.entries(patch)) {
        if (values[key] !== value) delta[key] = { oldValue: values[key], newValue: value };
        values[key] = value;
      }
      if (Object.keys(delta).length) changes.emit(delta, name);
    },
    remove: async key => { delete values[key]; }
  });
  const onMessage = event();
  const onStartup = event();
  const onInstalled = event();
  const chrome = {
    action: { setBadgeBackgroundColor: async () => {}, setBadgeText: async () => {} },
    storage: { local: area(state, 'local'), session: area({}, 'session'), onChanged: changes },
    runtime: {
      onMessage, onStartup, onInstalled,
      sendNativeMessage: async (_, msg) => { calls.push(msg); return native(msg); }
    },
    tabs: {
      onActivated: event(), query: async () => [{ id: activeTabId, windowId: 3, active: true }],
      sendMessage: async (id, msg) => { sent.push(msg); }
    },
    windows: {
      onFocusChanged: event(), WINDOW_ID_NONE: -1,
      getLastFocused: async () => ({ focused, tabs: [{ id: activeTabId, active: true }] })
    }
  };
  const setTimeout = (fn, delay) => { const id = nextTimer++; timers.set(id, { fn, at: now + delay }); return id; };
  const clearTimeout = id => timers.delete(id);
  vm.runInNewContext(source('service-worker.js'), { chrome, Date: { now: () => now }, setTimeout, clearTimeout });
  return {
    state, calls, sent, chrome,
    advance: ms => {
      const end = now + ms;
      for (;;) {
        const due = [...timers].filter(([, timer]) => timer.at <= end).sort((a, b) => a[1].at - b[1].at)[0];
        if (!due) break;
        const [id, timer] = due; timers.delete(id); now = timer.at; timer.fn();
      }
      now = end;
    },
    respond: fn => { native = fn; },
    startup: () => Promise.all(onStartup.emit()),
    install: reason => Promise.all(onInstalled.emit({ reason })),
    setFocused: value => { focused = value; },
    focus: value => { focused = value; return Promise.all(chrome.windows.onFocusChanged.emit(value ? 3 : -1)); },
    activate: id => { activeTabId = id; return Promise.all(chrome.tabs.onActivated.emit({ tabId:id, windowId:3 })); },
    report: (width = 800) => Promise.all(onMessage.emit({
      type: 'magpie-video-state', candidate: {
        rect: { left: 0, top: 0, width, height: 450 },
        viewport: { innerWidth: 1280, innerHeight: 720 }, playing: true
      }
    }, { tab: { id: 7, windowId: 3 } }))
  };
}

test('worker wake never launches; stopped Magpie turns the switch off', async () => {
  const h = workerHarness(); await flush();
  assert.equal(h.calls.length, 0);
  h.respond(async () => ({ ok: true, delivered: false, state: 'stopped' }));
  await h.report(); await flush();
  assert.equal(h.state.enabled, false);
  assert.match(h.state.statusNote, /已退出/);
  await h.report(900);
  assert.equal(h.calls.filter(x => x.action === 'update').length, 1);
  assert.ok(h.calls.every(x => x.action !== 'enable'));
});

test('browser startup restores the saved ON state once and rescans the foreground tab', async () => {
  const h = workerHarness();
  await h.startup(); await flush();
  assert.deepEqual(h.calls.map(x => x.action), ['enable']);
  assert.equal(h.state.enabled, true);
  assert.equal(h.sent.at(-1).type, 'magpie-rescan');
  await h.report();
  assert.deepEqual(h.calls.map(x => x.action), ['enable', 'update']);
});

test('browser startup respects a saved OFF switch', async () => {
  const h = workerHarness(false);
  await h.startup(); await flush();
  assert.equal(h.calls.length, 0);
  assert.equal(h.state.enabled, false);
});

test('failed browser startup leaves the switch OFF without retrying on video heartbeat', async () => {
  const h = workerHarness(); await flush();
  h.respond(async () => ({ ok: false, error: 'start failed' }));
  await h.startup(); await flush();
  assert.equal(h.state.enabled, false);
  assert.match(h.state.statusNote, /启动失败/);
  await h.report();
  assert.equal(h.calls.filter(x => x.action === 'enable').length, 1);
});

test('turning the switch OFF during browser startup wins over the pending launch', async () => {
  const h = workerHarness(); await flush();
  let release;
  h.respond(() => new Promise(resolve => { release = resolve; }));
  const starting = h.startup(); await flush();
  await h.chrome.storage.local.set({ enabled: false }); await flush();
  h.respond(async () => ({ ok: true, state: 'running' }));
  release({ ok: true, state: 'running' });
  await starting; await flush();
  assert.equal(h.state.enabled, false);
  assert.equal(h.calls.filter(x => x.action === 'enable').length, 1);
  assert.equal(h.calls.at(-1).action, 'disable');
  assert.ok(!h.sent.some(x => x.type === 'magpie-rescan'));
});

test('fresh installation enables enhancement through the existing switch path', async () => {
  const h = workerHarness(false); await flush();
  await h.install('install'); await flush();
  assert.equal(h.state.enabled, true);
  assert.equal(h.calls.filter(x => x.action === 'enable').length, 1);
});

test('switching to another app removes the overlay without disabling the user toggle', async () => {
  const h=workerHarness(); await flush(); await h.report();
  await h.focus(false); h.advance(399); await flush();
  assert.equal(h.calls.filter(x=>x.action==='disable').length,0,'brief focus loss keeps the frame bridge alive');
  h.advance(1); await flush();
  assert.equal(h.calls.at(-1).action,'disable');
  assert.equal(h.calls.at(-1).sessionId,'chromium:3:7');
  assert.equal(h.state.enabled,true);
  const count=h.calls.length;
  for(let i=0;i<3;i++){h.advance(2100);await h.report();}
  assert.equal(h.calls.length,count,'background heartbeats cannot restart or update enhancement');
  await h.focus(true);await h.report();
  assert.equal(h.calls.at(-1).action,'update');
  assert.ok(h.calls.every(x=>x.action!=='enable'),'focus return never launches Magpie');
});

test('transient WINDOW_ID_NONE followed by the same browser tab does not rebuild enhancement',async()=>{
  const h=workerHarness();await flush();await h.report();
  const before=h.calls.length;
  await h.focus(false);h.advance(150);await flush();
  await h.report();
  await h.focus(true);h.advance(500);await flush();
  assert.equal(h.calls.length,before,'heartbeat during same-tab focus recovery keeps the native mapping and overlay');
  assert.equal(h.sent.at(-1).type,'magpie-rescan');
});

test('focus loss during the first native update cancels queued work and removes the in-flight overlay',async()=>{
  const h=workerHarness();await flush();let release;
  h.respond(()=>new Promise(resolve=>{release=resolve;}));
  const first=h.report();await flush();
  const queued=h.report(950);await flush();
  const lost=h.focus(false);await flush();h.advance(400);await flush();
  h.respond(async()=>({ok:true,delivered:true,state:'running'}));
  release({ok:true,delivered:true,state:'running'});
  await Promise.all([first,queued,lost]);await flush();
  assert.deepEqual(h.calls.map(x=>x.action),['update','disable']);
  assert.equal(h.calls.at(-1).sessionId,'chromium:3:7');
});

test('a tab switch cancels a first update before the next page reports any video',async()=>{
  const h=workerHarness();await flush();let release;
  h.respond(()=>new Promise(resolve=>{release=resolve;}));
  const first=h.report();await flush();
  const switched=h.activate(8);await flush();
  h.respond(async()=>({ok:true,delivered:true,state:'running'}));
  release({ok:true,delivered:true,state:'running'});
  await Promise.all([first,switched]);
  assert.equal(h.calls.at(-1).action,'disable');
  const count=h.calls.length;await h.report(900);
  assert.equal(h.calls.length,count,'old tab cannot reappear over the new page');
});

test('missing focus events are caught by the report-time foreground check',async()=>{
  const h=workerHarness();await flush();await h.report();
  h.setFocused(false);h.advance(2100);await h.report();
  assert.notEqual(h.calls.at(-1).action,'disable','report-time fallback observes the same focus grace');
  h.advance(400);await flush();
  assert.equal(h.calls.at(-1).action,'disable');
});

test('explicit switch-on launches once and geometry keeps a recovery heartbeat', async () => {
  const h = workerHarness(false); await flush();
  await h.chrome.storage.local.set({ enabled: true }); await flush();
  assert.equal(h.calls[0].action, 'enable');
  await h.report(); await h.report();
  assert.equal(h.calls.filter(x => x.action === 'update').length, 1);
  h.advance(2100); await h.report();
  assert.equal(h.calls.filter(x => x.action === 'update').length, 2);
  assert.equal(h.calls.filter(x => x.action === 'enable').length, 1);
});

test('queued obsolete geometry is skipped and switching off cancels pending updates', async () => {
  const h = workerHarness(); await flush();
  let release;
  h.respond(() => new Promise(resolve => { release = resolve; }));
  const first = h.report(800); await flush();
  const middle = h.report(900); await flush();
  const latest = h.report(1000); await flush();
  h.respond(async () => ({ ok: true, delivered: true, state: 'running' }));
  release({ ok: true, delivered: true, state: 'running' });
  await Promise.all([first, middle, latest]);
  assert.deepEqual(h.calls.filter(x => x.action === 'update').map(x => x.rect.width), [800, 1000]);
  h.respond(() => new Promise(resolve => { release = resolve; }));
  const inflight = h.report(1100); await flush();
  const cancelled = h.report(1200); await flush();
  await h.chrome.storage.local.set({ enabled: false }); await flush();
  h.respond(async () => ({ ok: true, delivered: true, state: 'running' }));
  release({ ok: true, delivered: true, state: 'running' });
  await Promise.all([inflight, cancelled]); await flush();
  assert.ok(!h.calls.some(x => x.rect?.width === 1200));
  assert.equal(h.calls.at(-1).action, 'disable');
});
