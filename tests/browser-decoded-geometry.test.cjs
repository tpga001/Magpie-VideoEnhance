const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const ctx = {};
vm.runInNewContext(fs.readFileSync(require('node:path').join(__dirname, '../browser-extension/video-geometry.js'), 'utf8'), ctx);
const video = box => ({ videoWidth: 1920, videoHeight: 1080,
  getBoundingClientRect: () => ({ ...box, right: box.left + box.width, bottom: box.top + box.height }) });
const geometry = (box, style = {}, viewport = { innerWidth: 3840, innerHeight: 2160 }) =>
  ctx.MagpieVideoGeometry(video(box), viewport, style);
test('CSS enlargement preserves the original 1920x1080 input', () => {
  const g = geometry({ left: 0, top: 0, width: 3840, height: 2160 });
  assert.equal(g.crop.width, 1920); assert.equal(g.crop.height, 1080);
  assert.equal(g.rect.width, 3840); assert.equal(g.rect.height, 2160);
});
test('letterboxing is excluded without stretching the decoded picture', () => {
  const g = geometry({ left: 0, top: 0, width: 1000, height: 1000 });
  assert.equal(g.rect.top, 218.75); assert.equal(g.rect.height, 562.5);
  assert.equal(g.crop.width, 1920); assert.equal(g.crop.height, 1080);
});
test('partly offscreen video crops original pixels rather than squeezing all pixels', () => {
  const g = geometry({ left: -960, top: 0, width: 1920, height: 1080 });
  assert.equal(g.crop.x, 960); assert.equal(g.crop.width, 960);
  assert.equal(g.rect.left, 0); assert.equal(g.rect.width, 960);
});
test('object-fit cover crops the input to match the visible image', () => {
  const g = geometry({ left: 0, top: 0, width: 1080, height: 1080 }, { objectFit: 'cover' });
  assert.equal(g.crop.x, 420); assert.equal(g.crop.width, 1080);
});
test('unsupported transforms are not silently interpreted as unscaled frames', () => {
  assert.equal(geometry({ left: 0, top: 0, width: 1920, height: 1080 }, { transform: 'rotate(90deg)' }), null);
});
test('the CSS translation used by centered players preserves original pixels', () => {
  const g = geometry({ left: 20, top: 30, width: 1920, height: 1080 }, { transform: 'matrix(1, 0, 0, 1, -960, -540)' });
  assert.equal(g.crop.width, 1920); assert.equal(g.rect.left, 20);
});
test('one physical-pixel viewport clipping keeps a chroma-aligned source crop', () => {
  const g = geometry({ left: 0, top: 0, width: 3840, height: 2160 }, {},
    { innerWidth: 3840, innerHeight: 2159 });
  assert.deepEqual(JSON.parse(JSON.stringify(g.crop)), { x: 0, y: 0, width: 1920, height: 1078 });
  assert.equal(g.rect.height, 2156);
  assert.ok(g.rect.top + g.rect.height <= 2159, 'never extend the overlay past the visible video');
});
test('partly hidden top and left edges align inward without shifting or stretching pixels', () => {
  const g = geometry({ left: -1, top: -3, width: 1920, height: 1080 });
  assert.deepEqual(JSON.parse(JSON.stringify(g.crop)), { x: 2, y: 4, width: 1918, height: 1076 });
  assert.equal(g.rect.left, 1); assert.equal(g.rect.top, 1);
  assert.equal(g.rect.width, g.crop.width); assert.equal(g.rect.height, g.crop.height);
});
test('fractional display sizes keep even source boundaries inside the viewport', () => {
  for (const scale of [0.5, 1, 1.3333, 1.8328125, 2]) {
    for (const offset of [0, 0.25, 1, 1.75, 3]) {
      const viewport = { innerWidth: 1920 * scale - 0.25, innerHeight: 1080 * scale - 0.5 };
      const g = geometry({ left: -offset, top: -offset, width: 1920 * scale, height: 1080 * scale }, {}, viewport);
      assert.ok(g);
      for (const coordinate of Object.values(g.crop)) assert.equal(coordinate % 2, 0);
      assert.ok(g.rect.left >= -1e-6 && g.rect.top >= -1e-6);
      assert.ok(g.rect.left + g.rect.width <= viewport.innerWidth + 1e-6);
      assert.ok(g.rect.top + g.rect.height <= viewport.innerHeight + 1e-6);
      assert.ok(Math.abs(g.rect.width / g.crop.width - scale) < 1e-6);
      assert.ok(Math.abs(g.rect.height / g.crop.height - scale) < 1e-6);
    }
  }
});
