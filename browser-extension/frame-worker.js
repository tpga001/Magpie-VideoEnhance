let socket, pending = [], ackTimer, stopped = false;
const stats = {
  frames: 0, dropped: 0, droppedQueue: 0, droppedBackpressure: 0, droppedHost: 0,
  acks: 0, ackMs: 0, hostWriteMs: 0, hostLockMs: 0, maxPending: 0, maxBufferedBytes: 0
};
const fail = error => {
  if (stopped) return;
  stopped = true; clearTimeout(ackTimer);
  postMessage({ type: "error", error: error.message || String(error) }); socket?.close();
};
function armTimeout() {
  clearTimeout(ackTimer);
  if (pending.length) ackTimer = setTimeout(() => fail(new Error("原始视频帧传输超时")), Math.max(1, 2500 - (performance.now() - pending[0].sentAt)));
}
onmessage = event => {
  if (event.data.type === "init") {
    const url = event.data.connection.url;
    if (socket || !/^ws:\/\/127\.0\.0\.1:\d+\/[A-F0-9]{64}$/.test(url)) { fail(new Error("Invalid local frame endpoint")); return; }
    socket = new WebSocket(url);
    socket.onopen = () => postMessage({ type: "ready" });
    socket.onerror = () => fail(new Error("原始帧后台连接失败"));
    socket.onclose = () => fail(new Error("原始帧后台连接已关闭"));
    socket.onmessage = event => {
      const frame = pending.shift(); if (!frame || stopped) return;
      armTimeout();
      try {
        const ack = JSON.parse(event.data);
        if (!ack.ok) throw new Error("原始帧未被接收");
        ++stats.acks;
        stats.ackMs += performance.now() - frame.sentAt;
        stats.hostWriteMs += ack.writeMs || 0; stats.hostLockMs += ack.lockMs || 0;
        if (ack.dropped) {
          ++stats.dropped; ++stats.droppedHost;
          postMessage({ type: "stats", stats });
          return;
        }
        ++stats.frames;
        postMessage({ type: "ack", width: frame.width, height: frame.height, stats });
      } catch (error) { fail(error); }
    };
  } else if (event.data.type === "frame") {
    const { packet, width, height } = event.data;
    try {
      if (stopped || socket?.readyState !== WebSocket.OPEN) throw new Error("原始帧通道已关闭");
      stats.maxBufferedBytes = Math.max(stats.maxBufferedBytes, socket.bufferedAmount);
      if (pending.length >= 3) {
        ++stats.dropped; ++stats.droppedQueue;
        return;
      }
      if (socket.bufferedAmount >= 8 * 1024 * 1024) {
        ++stats.dropped; ++stats.droppedBackpressure;
        return;
      }
      pending.push({ width, height, sentAt: performance.now() });
      stats.maxPending = Math.max(stats.maxPending, pending.length);
      if (pending.length === 1) armTimeout();
      socket.send(new Blob([packet]));
    } catch (error) { fail(error); }
    finally { postMessage({ type: "queued", packet, stats }, [packet]); }
  }
};
