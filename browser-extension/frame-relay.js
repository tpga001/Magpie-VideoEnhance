let connected = false;
window.addEventListener("message", async event => {
  if (connected || event.source !== parent || event.data?.type !== "magpie-frame-relay" || event.ports.length !== 1) return;
  connected = true;
  const port = event.ports[0];
  let worker;
  try {
    // The webpage cannot select a network endpoint. Only the service worker
    // grants the connection for this tab's currently active native session.
    const connection = await chrome.runtime.sendMessage({ type: "magpie-relay-connect", mapName: event.data.mapName });
    if (!connection?.url) throw new Error("原始帧后台通道未获授权");
    worker = new Worker(chrome.runtime.getURL("frame-worker.js"));
    port.onmessageerror = () => port.postMessage({ type: "error", error: "浏览器拒绝传递原始视频像素" });
    worker.onmessage = event => port.postMessage(event.data, event.data.packet ? [event.data.packet] : []);
    worker.onerror = event => port.postMessage({ type: "error", error: event.message || "原始帧后台处理失败" });
    port.onmessage = event => {
      if (event.data.type === "stop") { worker.terminate(); port.close(); return; }
      if (event.data.type === "frame") worker.postMessage(event.data, [event.data.packet]);
    };
    worker.postMessage({ type: "init", connection });
  } catch (error) { worker?.terminate(); port.postMessage({ type: "error", error: error.message }); }
});
