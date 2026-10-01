const NATIVE_HOST = "com.magpie.browser_video";
const BADGE_COLOR = "#168A45";
const HEARTBEAT_MS = 2000;
const FOCUS_LOSS_GRACE_MS = 400;
let enabled = false;
let activeSession = "";
let lastPayload = "";
let stateRevision = 0;
let nativeQueue = Promise.resolve();
let reportRevision = 0;
let lastDeliveredAt = 0;
let frameBridge = null;
let failedFrameSession = "";
let currentDocument = "";
let focusRevision = 0;
let pendingSession = "";
let focusEventRevision = 0;
let focusLossTimer = 0;

function stopFrameBridge() {
  const previous = frameBridge;
  frameBridge = null;
  if (!previous) return;
  previous.port.disconnect();
  chrome.tabs.sendMessage(previous.tabId, { type: "magpie-frames-stop" }).catch(() => {});
}

async function ensureFrameBridge(sessionId, tabId, pageUrl) {
  if (frameBridge?.sessionId === sessionId) {
    if (frameBridge.connection) await chrome.tabs.sendMessage(tabId, {
      type: "magpie-frames-start", connection: frameBridge.connection
    }).catch(() => {});
    return;
  }
  stopFrameBridge();
  const port = chrome.runtime.connectNative(NATIVE_HOST);
  const bridge = { sessionId, tabId, port, connection: null };
  frameBridge = bridge;
  port.onMessage.addListener(async response => {
    if (frameBridge !== bridge || !enabled) return;
    if (response.state === "stopped") {
      await chrome.storage.local.set({ enabled: false, statusNote: "Magpie 已退出；重新开启开关即可恢复增强。" });
    } else if (response.ok && response.state === "frames") {
      bridge.connection = { url: response.url, mapName: response.mapName };
      await chrome.tabs.sendMessage(tabId, { type: "magpie-frames-start", connection: bridge.connection }).catch(() => {});
    } else {
      failedFrameSession = sessionId;
      stopFrameBridge();
      await disableActive("frame-bridge-error");
      await chrome.storage.local.set({ lastError: response.error || "原始帧通道不可用" });
    }
  });
  port.onDisconnect.addListener(async () => {
    const message = chrome.runtime.lastError?.message;
    if (frameBridge !== bridge) return;
    frameBridge = null;
    failedFrameSession = sessionId;
    await disableActive("frame-bridge-disconnected");
    await chrome.tabs.sendMessage(tabId, { type: "magpie-frames-stop" }).catch(() => {});
    await chrome.storage.local.set({ lastError: message || "原始帧通道断开，请重新开启增强" });
  });
  port.postMessage({ action: "frames", pageUrl });
}

async function setBadge() {
  await chrome.action.setBadgeBackgroundColor({ color: BADGE_COLOR });
  await chrome.action.setBadgeText({ text: enabled ? "ON" : "" });
}

async function sendNativeNow(message) {
  try {
    const response = await chrome.runtime.sendNativeMessage(NATIVE_HOST, message);
    const error = response && response.ok ? "" : (response && response.error) || "Magpie 未确认请求";
    await chrome.storage.local.set({ lastError: error });
    return error ? null : response;
  } catch (error) {
    await chrome.storage.local.set({ lastError: error.message || String(error) });
    return null;
  }
}

function sendNative(message, isCurrent = () => true) {
  // Discard geometry superseded while a native host was starting or replying.
  const run = async () => await isCurrent() ? sendNativeNow(message) : null;
  const pending = nativeQueue.then(run, run);
  nativeQueue = pending.then(() => undefined, () => undefined);
  return pending;
}

async function disableActive(reason, sessionHint = "") {
  const sessionId = activeSession || sessionHint || pendingSession || frameBridge?.sessionId ||
    (reason === "toggle-off" ? "global" : "");
  if (!sessionId) return;
  activeSession = "";
  pendingSession = "";
  lastPayload = "";
  lastDeliveredAt = 0;
  await chrome.storage.session.remove("activeSession");
  await sendNative({ action: "disable", sessionId, reason });
}

async function suspendForFocus(reason) {
  // Invalidate work before any await, including the first update still in flight.
  ++focusRevision;
  ++reportRevision;
  const session = frameBridge?.sessionId || "";
  stopFrameBridge();
  await disableActive(reason, session);
  if (enabled) await chrome.storage.local.set({ statusNote: "已开启，等待前台页面的视频" });
}

async function focusedActiveTab() {
  const focused = await chrome.windows.getLastFocused({ populate: true });
  if (!focused || !focused.focused) return null;
  return (focused.tabs || []).find((tab) => tab.active) || null;
}

function sessionForTab(tab) {
  return tab?.id == null || tab?.windowId == null ? "" : `chromium:${tab.windowId}:${tab.id}`;
}

function currentVideoSession() {
  return frameBridge?.sessionId || activeSession || pendingSession || "";
}

async function handleFocusedBrowserWindow(windowId, eventRevision) {
  await ready;
  if (eventRevision !== focusEventRevision || !enabled) return;
  const tabs = await chrome.tabs.query({ active: true, windowId });
  if (eventRevision !== focusEventRevision || !enabled) return;
  const tab = tabs[0];
  if (!tab?.id) return;
  const expectedSession = sessionForTab(tab);
  const session = currentVideoSession();
  // Chrome commonly reports WINDOW_ID_NONE for a fraction of a second while
  // focus settles. Returning to the same tab must keep its native frame mapping
  // alive; a genuinely different browser tab/window still switches immediately.
  if (session && session !== expectedSession) await suspendForFocus("browser-focus-changed");
  if (eventRevision === focusEventRevision && enabled) {
    chrome.tabs.sendMessage(tab.id, { type: "magpie-rescan" }).catch(() => {});
  }
}

function scheduleFocusLossCheck(eventRevision) {
  focusLossTimer = setTimeout(async () => {
    focusLossTimer = 0;
    await ready;
    if (eventRevision !== focusEventRevision || !enabled) return;
    const tab = await focusedActiveTab();
    if (eventRevision !== focusEventRevision || !enabled) return;
    if (!tab) await suspendForFocus("browser-focus-changed");
    else await handleFocusedBrowserWindow(tab.windowId, eventRevision);
  }, FOCUS_LOSS_GRACE_MS);
}

chrome.runtime.onMessage.addListener((message, sender) => {
  if (message?.type === "magpie-relay-connect") {
    return Promise.resolve(enabled && sender.tab?.id === frameBridge?.tabId &&
      sender.url === chrome.runtime.getURL("frame-relay.html") &&
      message.mapName === frameBridge?.connection?.mapName ? frameBridge.connection : null);
  }
  if (!message || message.type !== "magpie-video-state" || !sender.tab) return;
  return (async () => {
    await ready;
    if (!enabled) return;
    const revision = stateRevision;
    const focus = focusRevision;
    const activeTab = await focusedActiveTab();
    if (!enabled || revision !== stateRevision || focus !== focusRevision) return;
    if (!activeTab) {
      // chrome.windows.onFocusChanged owns the bounded focus-loss decision.
      // A video heartbeat can arrive during Chrome's short WINDOW_ID_NONE pulse;
      // tearing the frame bridge down here would bypass that grace period.
      // Schedule the same bounded check as a fallback if Chrome omitted the
      // focus event entirely.
      if (!focusLossTimer) scheduleFocusLossCheck(++focusEventRevision);
      return;
    }
    if (activeTab.id !== sender.tab.id) return;
    const report = ++reportRevision;
    const isCurrent = () => enabled && revision === stateRevision && focus === focusRevision && report === reportRevision;

    const sessionId = `chromium:${sender.tab.windowId}:${sender.tab.id}`;
    const documentId = sender.documentId || sessionId;
    if (documentId !== currentDocument) {
      currentDocument = documentId;
      failedFrameSession = "";
      stopFrameBridge();
      await disableActive("document-changed");
    }
    if (!message.candidate) {
      stopFrameBridge();
      if (activeSession === sessionId) await disableActive("no-visible-video");
      return;
    }

    const source = message.candidate.source;
    if (source) {
      if (source.frame?.error) {
        stopFrameBridge();
        await disableActive("decoded-frame-unavailable");
        failedFrameSession = sessionId;
        await chrome.storage.local.set({ statusNote: source.frame.error });
        return;
      }
      if (!source.supported) {
        // Geometry can be temporarily unavailable while a player resizes or
        // moves between displays. Do not poison the whole document session;
        // a later heartbeat can recover without toggling the extension.
        stopFrameBridge();
        await disableActive("decoded-frame-waiting");
        await chrome.storage.local.set({ statusNote: "等待视频画面稳定后自动恢复增强。" });
        return;
      }
      if (source.paused) {
        await disableActive("video-paused");
        return;
      }
      if (failedFrameSession === sessionId) return;
      await ensureFrameBridge(sessionId, sender.tab.id, message.candidate.pageUrl);
      if (!isCurrent() || !source.frame?.mapName || source.frame.mapName !== frameBridge?.connection?.mapName) return;
    }

    const request = {
      action: "update",
      sessionId,
      rect: message.candidate.rect,
      viewport: message.candidate.viewport,
      playing: message.candidate.playing,
      frameSource: source?.frame,
      uiRects: source?.frame ? message.candidate.uiRects : undefined,
      pageUrl: message.candidate.pageUrl,
      pageTitle: message.candidate.pageTitle
    };
    const serialized = JSON.stringify(request);
    if (serialized === lastPayload && activeSession === sessionId &&
        Date.now() - lastDeliveredAt < HEARTBEAT_MS) return;
    if (activeSession && activeSession !== sessionId) await disableActive("active-tab-changed");
    if (!isCurrent()) return;
    pendingSession = sessionId;
    const response = await sendNative(request, async () => {
      if (!isCurrent()) return false;
      const tab = await focusedActiveTab();
      return isCurrent() && tab?.id === sender.tab.id;
    });
    if (!isCurrent() || !response) return;
    pendingSession = "";
    if (response.state === "stopped") {
      enabled = false;
      ++stateRevision;
      await chrome.storage.local.set({
        enabled: false, statusNote: "Magpie 已退出；重新开启此开关即可恢复增强。"
      });
      await setBadge();
      return;
    }
    if (response.delivered) {
      activeSession = sessionId;
      lastPayload = serialized;
      lastDeliveredAt = Date.now();
      await chrome.storage.session.set({ activeSession });
      if (source?.frame) await chrome.storage.local.set({ statusNote: `原始帧增强 · ${source.frame.width}×${source.frame.height} 输入` });
    }
  })();
});

const ready = Promise.all([
  chrome.storage.local.get({ enabled: false }),
  chrome.storage.session.get({ activeSession: "" })
]).then(async ([state, session]) => {
  enabled = Boolean(state.enabled);
  activeSession = session.activeSession || "";
  await setBadge();
  if (enabled) {
    const tab = await focusedActiveTab();
    if (tab?.id) chrome.tabs.sendMessage(tab.id, { type: "magpie-rescan" }).catch(() => {});
  }
});

chrome.storage.onChanged.addListener(async (changes, area) => {
  if (area !== "local" || !changes.enabled) return;
  await ready;
  const revision = ++stateRevision;
  failedFrameSession = "";
  stopFrameBridge();
  ++reportRevision;
  enabled = false;
  if (changes.enabled.newValue) {
    const response = await sendNative({ action: "enable" }, () => revision === stateRevision);
    if (revision !== stateRevision) return;
    enabled = Boolean(response && response.state === "running");
    await chrome.storage.local.set({
      enabled, statusNote: enabled ? "" : "Magpie 启动失败，请重新开启或查看连接错误。"
    });
  }
  if (revision !== stateRevision) return;
  await setBadge();
  if (!enabled) await disableActive("toggle-off");
  const tabs = await chrome.tabs.query({});
  await Promise.allSettled(tabs.map((tab) => tab.id && chrome.tabs.sendMessage(tab.id, {
    type: "magpie-enabled-changed",
    enabled
  })));
});

chrome.tabs.onActivated.addListener(async ({ tabId }) => {
  const suspended = suspendForFocus("active-tab-changed");
  await ready;
  await suspended;
  if (!enabled) return;
  failedFrameSession = "";
  chrome.tabs.sendMessage(tabId, { type: "magpie-rescan" }).catch(() => {});
});

chrome.windows.onFocusChanged.addListener(async (windowId) => {
  const eventRevision = ++focusEventRevision;
  if (focusLossTimer) clearTimeout(focusLossTimer);
  focusLossTimer = 0;
  if (windowId !== chrome.windows.WINDOW_ID_NONE) {
    await handleFocusedBrowserWindow(windowId, eventRevision);
    return;
  }
  // A short WINDOW_ID_NONE pulse occurs during ordinary Chromium/overlay focus
  // settling. Confirm that focus really left the browser before destroying the
  // decoded-frame host and its shared mapping.
  scheduleFocusLossCheck(eventRevision);
});
