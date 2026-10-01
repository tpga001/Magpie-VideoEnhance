// Read unscaled pixels in the page renderer, then transfer the buffer to an
// extension worker. Bound readback/transport queues to avoid accumulating lag.
//
// Keep the pacing phase independent from the last frame that happened to make
// it through readback. A fixed "time since last accepted frame" threshold can
// drift when callbacks arrive a little early, and recovering from a busy copy
// can move that threshold again. This clock always advances on the original
// media timeline in exact target-frame intervals.
globalThis.MagpieFramePacer = class {
  constructor(targetFps = 30) {
    this.interval = 1 / targetFps;
    // mediaTime is normally exact, but Chromium/WebCodecs timestamp conversion
    // can introduce sub-millisecond rounding around a due boundary.
    this.tolerance = Math.min(0.001, this.interval * 0.05);
    this.reset();
  }
  reset() { this.nextTime = NaN; this.lastTime = NaN; }
  shouldSubmit(mediaTime) {
    if (!Number.isFinite(mediaTime)) return false;
    const discontinuity = Number.isFinite(this.lastTime) &&
      (mediaTime < this.lastTime - this.tolerance ||
       mediaTime - this.lastTime > Math.max(1, this.interval * 4));
    if (!Number.isFinite(this.nextTime) || discontinuity) {
      this.lastTime = mediaTime;
      this.nextTime = mediaTime + this.interval;
      return true;
    }
    this.lastTime = mediaTime;
    if (mediaTime + this.tolerance < this.nextTime) return false;
    const overdue = Math.max(0, mediaTime - this.nextTime + this.tolerance);
    this.nextTime += Math.max(1, Math.floor(overdue / this.interval) + 1) * this.interval;
    return true;
  }
};

globalThis.MagpieFrameRelay = class {
  constructor(select, changed) {
    this.select = select; this.changed = changed; this.generation = 0; this.state = null;
    this.pacer = new MagpieFramePacer(30);
  }
  stop() {
    ++this.generation;
    if (this.video && this.callback) this.video.cancelVideoFrameCallback(this.callback);
    clearTimeout(this.startTimer); clearTimeout(this.retryTimer); clearTimeout(this.watchdog); clearTimeout(this.copyTimer);
    this.port?.postMessage({ type: "stop" });
    this.port?.close(); this.iframe?.remove();
    this.port = null; this.iframe = null; this.state = null; this.pending = 0; this.callback = 0;
    this.copying = false; this.buffers = [];
    this.pacer.reset(); this.lastPresentedFrames = null;
  }
  start(connection) {
    if (this.connection?.mapName === connection.mapName && (this.port || this.iframe)) return;
    this.stop(); this.connection = connection;
    const generation = this.generation;
    this.lastTime = -1;
    this.stats = {
      frames: 0, callbacks: 0, submitted: 0, copyMs: 0,
      skippedPacing: 0, skippedCopying: 0, skippedPending: 0,
      skippedSelection: 0, sourceCallbackMissed: 0,
      dropped: 0, droppedQueue: 0, droppedBackpressure: 0, droppedHost: 0,
      acks: 0, ackMs: 0, hostWriteMs: 0, hostLockMs: 0,
      startedAt: performance.now()
    };
    this.statsSnapshot = { ...this.stats };
    this.statsReportedAt = this.stats.startedAt;
    const fail = reason => {
      if (generation !== this.generation) return;
      this.stop(); this.state = { error: reason }; this.changed();
    };
    const iframe = document.createElement("iframe");
    iframe.setAttribute("aria-hidden", "true"); iframe.tabIndex = -1;
    iframe.style.cssText = "position:fixed!important;left:0!important;bottom:0!important;width:1px!important;height:1px!important;border:0!important;opacity:0!important;pointer-events:none!important";
    iframe.src = chrome.runtime.getURL("frame-relay.html");
    this.iframe = iframe;
    this.startTimer = setTimeout(() => fail("原始帧后台通道启动超时"), 8000);
    iframe.onload = () => {
      if (generation !== this.generation) return;
      const channel = new MessageChannel(); this.port = channel.port1;
      channel.port1.onmessage = event => {
        if (generation !== this.generation) return;
        const message = event.data;
        if (message.type === "error") { fail(message.error); return; }
        if (message.type === "ready") {
          clearTimeout(this.startTimer); this.schedule(generation, fail); return;
        }
        if (message.type === "queued") {
          this.pending = Math.max(0, this.pending - 1);
          if (message.packet && this.buffers.length < 2) this.buffers.push(message.packet);
          if (message.stats) this.mergeWorkerStats(message.stats);
          clearTimeout(this.watchdog);
          if (this.pending) this.watchdog = setTimeout(() => fail("原始视频帧读取超时"), 2500);
          this.reportStats();
          return;
        }
        if (message.type === "ack") {
          this.mergeWorkerStats(message.stats);
          const next = { mapName: connection.mapName, width: message.width, height: message.height };
          if (JSON.stringify(this.state) !== JSON.stringify(next)) { this.state = next; this.changed(); }
          this.reportStats();
          return;
        }
        if (message.type === "stats") { this.mergeWorkerStats(message.stats); this.reportStats(); }
      };
      iframe.contentWindow.postMessage({ type: "magpie-frame-relay", mapName: connection.mapName },
        new URL(iframe.src).origin, [channel.port2]);
    };
    document.documentElement.appendChild(iframe);
  }
  mergeWorkerStats(stats) {
    for (const name of ["frames", "dropped", "droppedQueue", "droppedBackpressure", "droppedHost",
      "acks", "ackMs", "hostWriteMs", "hostLockMs", "maxPending", "maxBufferedBytes"]) {
      if (Number.isFinite(stats?.[name])) this.stats[name] = stats[name];
    }
  }
  reportStats(now = performance.now()) {
    if (now - this.statsReportedAt < 5000) return;
    const previous = this.statsSnapshot;
    const delta = name => (this.stats[name] || 0) - (previous[name] || 0);
    const submitted = delta("submitted"), acks = delta("acks");
    this.stats.last5s = {
      durationMs: Math.round(now - this.statsReportedAt),
      callbacks: delta("callbacks"),
      submitted,
      accepted: delta("frames"),
      sourceCallbackMissed: delta("sourceCallbackMissed"),
      skipped: {
        pacing: delta("skippedPacing"), copying: delta("skippedCopying"),
        pending: delta("skippedPending"), selection: delta("skippedSelection")
      },
      dropped: {
        total: delta("dropped"), queue: delta("droppedQueue"),
        backpressure: delta("droppedBackpressure"), host: delta("droppedHost")
      },
      copy: { averageMs: submitted ? Math.round(delta("copyMs") / submitted * 100) / 100 : 0 },
      ack: {
        count: acks,
        averageMs: acks ? Math.round(delta("ackMs") / acks * 100) / 100 : 0,
        hostWriteAverageMs: acks ? Math.round(delta("hostWriteMs") / acks * 100) / 100 : 0,
        hostLockAverageMs: acks ? Math.round(delta("hostLockMs") / acks * 100) / 100 : 0
      },
      queue: { maxPending: this.stats.maxPending || 0, maxBufferedBytes: this.stats.maxBufferedBytes || 0 }
    };
    // This is visible in the page's extension context and in automated E2E
    // capture without sending another service-worker/native update.
    console.debug("[Magpie] decoded frame relay (5 s)", this.stats.last5s);
    this.statsSnapshot = { ...this.stats };
    this.statsReportedAt = now;
  }
  schedule(generation, fail) {
    if (generation !== this.generation) return;
    const selected = this.select();
    if (!selected || selected.video.paused || selected.video.ended) {
      this.retryTimer = setTimeout(() => this.schedule(generation, fail), 100); return;
    }
    if (this.video !== selected.video) {
      this.pacer.reset();
      this.lastPresentedFrames = null;
    }
    this.video = selected.video;
    this.callback = this.video.requestVideoFrameCallback(async (_, metadata) => {
      if (generation !== this.generation) return;
      this.callback = 0; ++this.stats.callbacks;
      if (Number.isFinite(metadata.presentedFrames)) {
        if (Number.isFinite(this.lastPresentedFrames) && metadata.presentedFrames > this.lastPresentedFrames + 1) {
          this.stats.sourceCallbackMissed += metadata.presentedFrames - this.lastPresentedFrames - 1;
        }
        this.lastPresentedFrames = metadata.presentedFrames;
      }
      this.schedule(generation, fail);
      if (this.copying) { ++this.stats.skippedCopying; this.reportStats(); return; }
      if (this.pending >= 2) { ++this.stats.skippedPending; this.reportStats(); return; }
      const current = this.select();
      if (!current?.geometry || current.video !== selected.video) {
        ++this.stats.skippedSelection; this.reportStats(); return;
      }
      // Do not advance the media clock when a copy/queue is busy. The next
      // callback can then recover immediately instead of losing another slot.
      if (!this.pacer.shouldSubmit(metadata.mediaTime)) { ++this.stats.skippedPacing; this.reportStats(); return; }
      let frame;
      try {
        this.copying = true;
        this.copyTimer = setTimeout(() => fail("原始视频帧读取超时"), 2500);
        frame = new VideoFrame(current.video, { timestamp: Math.round(metadata.mediaTime * 1e6) });
        const start = performance.now();
        const data = await MagpieReadFrame(frame, current.geometry.crop, Math.round(metadata.mediaTime * 1e6), this.buffers.pop());
        if (generation !== this.generation) return;
        this.stats.copyMs += performance.now() - start; this.stats.format = data.format;
        this.port.postMessage({ type: "frame", ...data }, [data.packet]);
        ++this.pending; ++this.stats.submitted; this.lastTime = metadata.mediaTime;
        if (this.pending === 1) this.watchdog = setTimeout(() => fail("原始视频帧读取超时"), 2500);
      } catch (error) {
        fail(error.name === "SecurityError" ? "此视频不允许读取原始帧（跨域或受保护内容）" : error.message);
      } finally {
        frame?.close();
        if (generation === this.generation) { clearTimeout(this.copyTimer); this.copying = false; this.reportStats(); }
      }
    });
  }
};
