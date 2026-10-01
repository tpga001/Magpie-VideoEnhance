const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const { test } = require('node:test');

const source = fs.readFileSync(path.join(__dirname, '../browser-extension/frame-relay-client.js'), 'utf8');
const context = { console, performance: { now: () => 0 } };
context.globalThis = context;
vm.createContext(context);
vm.runInContext(source, context);

function run(sourceFps, seconds = 600) {
  const pacer = new context.MagpieFramePacer(30);
  const sourceFrames = Math.round(sourceFps * seconds);
  let submitted = 0;
  for (let frame = 0; frame < sourceFrames; ++frame) {
    if (pacer.shouldSubmit(frame / sourceFps)) ++submitted;
  }
  return { sourceFrames, submitted };
}

test('30 and 29.97 fps sources keep every decoded frame for ten minutes', () => {
  assert.deepEqual(run(30), { sourceFrames: 18000, submitted: 18000 });
  assert.deepEqual(run(29.97), { sourceFrames: 17982, submitted: 17982 });
});

test('60 and 59.94 fps sources are phase-locked to 30 fps without cumulative drift', () => {
  assert.deepEqual(run(60), { sourceFrames: 36000, submitted: 18000 });
  assert.deepEqual(run(59.94), { sourceFrames: 35964, submitted: 18000 });
});

test('slower sources are never thinned and seek discontinuities restart immediately', () => {
  assert.deepEqual(run(24), { sourceFrames: 14400, submitted: 14400 });
  const pacer = new context.MagpieFramePacer(30);
  assert.equal(pacer.shouldSubmit(100), true);
  assert.equal(pacer.shouldSubmit(100.01), false);
  assert.equal(pacer.shouldSubmit(4), true, 'backward seek starts a fresh pacing epoch');
  assert.equal(pacer.shouldSubmit(6), true, 'long forward seek starts a fresh pacing epoch');
});

test('sub-millisecond timestamp rounding cannot lose a nominal 30 fps frame', () => {
  const pacer = new context.MagpieFramePacer(30);
  let submitted = 0;
  for (let frame = 0; frame < 3000; ++frame) {
    const rounding = frame % 2 ? -0.0004 : 0.0004;
    if (pacer.shouldSubmit(frame / 30 + rounding)) ++submitted;
  }
  assert.equal(submitted, 3000);
});

test('five-second telemetry separates pacing, copy, queue and host costs', () => {
  const reports = [];
  const originalDebug = context.console.debug;
  context.console.debug = (_, report) => reports.push(report);
  try {
    const relay = new context.MagpieFrameRelay(() => null, () => {});
    relay.stats = {
      callbacks: 150, submitted: 140, frames: 137, sourceCallbackMissed: 2,
      skippedPacing: 4, skippedCopying: 3, skippedPending: 2, skippedSelection: 1,
      dropped: 3, droppedQueue: 1, droppedBackpressure: 1, droppedHost: 1,
      copyMs: 280, acks: 138, ackMs: 414, hostWriteMs: 138, hostLockMs: 69,
      maxPending: 3, maxBufferedBytes: 9000000, startedAt: 0
    };
    relay.statsSnapshot = { startedAt: 0 };
    relay.statsReportedAt = 0;
    relay.reportStats(4999);
    assert.equal(reports.length, 0);
    relay.reportStats(5000);
    assert.equal(reports.length, 1);
    assert.deepEqual(JSON.parse(JSON.stringify(relay.stats.last5s)), {
      durationMs: 5000, callbacks: 150, submitted: 140, accepted: 137, sourceCallbackMissed: 2,
      skipped: { pacing: 4, copying: 3, pending: 2, selection: 1 },
      dropped: { total: 3, queue: 1, backpressure: 1, host: 1 },
      copy: { averageMs: 2 },
      ack: { count: 138, averageMs: 3, hostWriteAverageMs: 1, hostLockAverageMs: 0.5 },
      queue: { maxPending: 3, maxBufferedBytes: 9000000 }
    });
  } finally { context.console.debug = originalDebug; }
});
