(() => {
  const MIN_WIDTH = 160;
  const MIN_HEIGHT = 90;
  const REPORT_DELAY_MS = 200;
  const POLL_INTERVAL_MS = 1000;
  const HEARTBEAT_MS = 2000;
  let enabled = false;
  let reportTimer = 0;
  let lastSignature = "";
  let lastReportAt = 0;
  let observedVideo = null;
  let pointerActiveUntil = 0;
  const frames = globalThis.MagpieDecodedFrames ? new MagpieDecodedFrames(chooseMainVideo, () => scheduleReport(true)) : null;
  const videoResizeObserver = new ResizeObserver(() => scheduleReport());

  function clippedRect(element) {
    const rect = element.getBoundingClientRect();
    const left = Math.max(0, rect.left);
    const top = Math.max(0, rect.top);
    const right = Math.min(window.innerWidth, rect.right);
    const bottom = Math.min(window.innerHeight, rect.bottom);
    return {
      left,
      top,
      width: Math.max(0, right - left),
      height: Math.max(0, bottom - top)
    };
  }

  function isRendered(video, rect) {
    if (rect.width < MIN_WIDTH || rect.height < MIN_HEIGHT) return false;
    const style = getComputedStyle(video);
    return style.display !== "none" && style.visibility !== "hidden" &&
      Number(style.opacity || "1") > 0.05 && video.readyState >= 1 &&
      Boolean(video.currentSrc || video.src);
  }

  function chooseMainVideo() {
    let best = null;
    for (const video of document.querySelectorAll("video")) {
      const rect = clippedRect(video);
      if (!isRendered(video, rect)) continue;
      const playing = !video.paused && !video.ended && video.readyState >= 2;
      const area = rect.width * rect.height;
      // A playing video always wins over paused previews or decorative loops;
      // area then selects the main player when a page contains several videos.
      const score = (playing ? Number.MAX_SAFE_INTEGER / 2 : 0) + area;
      if (!best || score > best.score) {
        const geometry = globalThis.MagpieVideoGeometry?.(video, window, getComputedStyle(video));
        best = { video, rect: geometry?.rect || rect, geometry, playing, score };
      }
    }
    return best;
  }

  function rounded(value) {
    return Math.round(value * 10) / 10;
  }

  function playerUiRects(rect) {
    const result = [];
    const add = box => {
      const left = Math.max(rect.left, box.left - 2), top = Math.max(rect.top, box.top - 2);
      const right = Math.min(rect.left + rect.width, box.right + 2), bottom = Math.min(rect.top + rect.height, box.bottom + 2);
      if (right > left && bottom > top) result.push([
        (left - rect.left) / rect.width, (top - rect.top) / rect.height,
        (right - rect.left) / rect.width, (bottom - rect.top) / rect.height
      ].map(x => Math.round(x * 10000) / 10000));
    };
    // Leave webpage controls and DOM subtitles visible and clickable. Embedded
    // subtitles remain part of the decoded video and are enhanced normally.
    const selectors = ".bpx-player-control-wrap,.bpx-player-top-wrap,.bpx-player-subtitle-panel," +
      ".bpx-player-subtitle-wrap,.bpx-player-context-menu,.bpx-player-toast-wrap," +
      ".bpx-player-ending-panel,.bpx-player-setting-panel,.bili-danmaku-x-dm,[role=menu],[role=slider],[role=dialog]";
    for (const element of document.querySelectorAll(selectors)) {
      if (result.length >= 90) break;
      let visible = true;
      for (let node = element, depth = 0; node && depth < 6; node = node.parentElement, ++depth) {
        const style = getComputedStyle(node);
        if (style.display === "none" || style.visibility === "hidden" || Number(style.opacity || 1) < 0.05) { visible = false; break; }
      }
      if (visible) add(element.getBoundingClientRect());
    }
    if (Date.now() < pointerActiveUntil) {
      add({ left: rect.left, right: rect.left + rect.width, top: rect.top + Math.max(0, rect.height - 112), bottom: rect.top + rect.height });
    }
    return result;
  }

  function currentReport() {
    // Magpie's fullscreen output can fully occlude the Chromium window. Chrome
    // then marks the active page as hidden even though it is still the source
    // video. The service worker already restricts reports to the active tab,
    // so page visibility must not tear down an established video session.
    if (!enabled) {
      return { candidate: null };
    }
    const candidate = chooseMainVideo();
    if (candidate?.video !== observedVideo) {
      videoResizeObserver.disconnect();
      observedVideo = candidate?.video || null;
      if (observedVideo) videoResizeObserver.observe(observedVideo);
    }
    if (!candidate) return { candidate: null };
    const { rect, video, playing } = candidate;
    return {
      candidate: {
        rect: {
          left: rounded(rect.left),
          top: rounded(rect.top),
          width: rounded(rect.width),
          height: rounded(rect.height)
        },
        viewport: {
          innerWidth: window.innerWidth,
          innerHeight: window.innerHeight,
          devicePixelRatio: window.devicePixelRatio || 1,
          screenX: window.screenX,
          screenY: window.screenY,
          outerWidth: window.outerWidth,
          outerHeight: window.outerHeight
        },
        playing,
        uiRects: frames ? playerUiRects(rect) : undefined,
        source: frames ? {
          width: video.videoWidth, height: video.videoHeight,
          paused: video.paused || (video.ended && !video.loop),
          supported: Boolean(candidate.geometry && typeof VideoFrame === "function" && video.requestVideoFrameCallback),
          frame: frames.state
        } : undefined,
        currentTime: Math.round(video.currentTime || 0),
        pageUrl: location.href,
        pageTitle: document.title
      }
    };
  }

  function reportNow(force = false) {
    reportTimer = 0;
    const report = currentReport();
    const signature = JSON.stringify(report.candidate && {
      rect: report.candidate.rect,
      viewport: report.candidate.viewport,
      playing: report.candidate.playing,
      source: report.candidate.source,
      uiRects: report.candidate.uiRects,
      visible: document.visibilityState
    });
    const now = Date.now();
    if (!force && signature === lastSignature && (!enabled || now - lastReportAt < HEARTBEAT_MS)) return;
    lastSignature = signature;
    lastReportAt = now;
    chrome.runtime.sendMessage({ type: "magpie-video-state", ...report }).catch(() => {});
  }

  function scheduleReport(force = false) {
    // Throttle from the first event. A trailing debounce never fires on pages
    // with continuously animated captions, controls or danmaku DOM mutations.
    if (reportTimer && !force) return;
    if (reportTimer) clearTimeout(reportTimer);
    reportTimer = setTimeout(() => reportNow(force), force ? 0 : REPORT_DELAY_MS);
  }

  function setEnabled(value) {
    enabled = Boolean(value);
    if (!enabled) frames?.stop();
    lastSignature = "";
    scheduleReport(true);
  }

  chrome.storage.local.get({ enabled: false }).then(({ enabled: value }) => setEnabled(value));
  chrome.runtime.onMessage.addListener((message) => {
    if (message && message.type === "magpie-enabled-changed") setEnabled(message.enabled);
    if (message && message.type === "magpie-rescan") scheduleReport(true);
    if (message && message.type === "magpie-frames-start" && enabled) frames?.start(message.connection);
    if (message && message.type === "magpie-frames-stop") frames?.stop();
  });

  for (const eventName of ["play", "pause", "loadedmetadata", "emptied", "ended", "enterpictureinpicture", "leavepictureinpicture"]) {
    document.addEventListener(eventName, () => scheduleReport(), true);
  }
  document.addEventListener("visibilitychange", () => scheduleReport(true), true);
  document.addEventListener("fullscreenchange", () => scheduleReport(), true);
  window.addEventListener("resize", () => scheduleReport(), true);
  window.addEventListener("scroll", () => scheduleReport(), true);
  document.addEventListener("pointermove", event => {
    if (!enabled || !frames) return;
    const rect = observedVideo?.getBoundingClientRect();
    if (rect && event.clientX >= rect.left && event.clientX <= rect.right && event.clientY >= rect.top && event.clientY <= rect.bottom) {
      const wasInactive = Date.now() >= pointerActiveUntil;
      pointerActiveUntil = Date.now() + 1500;
      if (wasInactive) scheduleReport(true);
    }
  }, true);
  new MutationObserver(() => scheduleReport()).observe(document.documentElement, {
    childList: true,
    subtree: true,
    attributes: true,
    attributeFilter: ["style", "class", "src"]
  });
  setInterval(() => scheduleReport(), POLL_INTERVAL_MS);
})();
