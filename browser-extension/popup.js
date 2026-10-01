const checkbox = document.getElementById("enabled");
const status = document.getElementById("status");
const error = document.getElementById("error");

function render(enabled, lastError, statusNote = "") {
  checkbox.checked = enabled;
  status.textContent = statusNote || (enabled ? "已开启，等待或处理当前视频" : "已关闭");
  error.hidden = !lastError;
  error.textContent = lastError ? `连接失败：${lastError}` : "";
}

function refresh() {
  chrome.storage.local.get({ enabled: false, lastError: "", statusNote: "" }).then((state) => {
    render(Boolean(state.enabled), state.lastError || "", state.statusNote || "");
  });
}
refresh();
chrome.storage.onChanged.addListener((changes, area) => {
  if (area === "local" && (changes.enabled || changes.lastError || changes.statusNote)) refresh();
});

checkbox.addEventListener("change", async () => {
  await chrome.storage.local.set({ enabled: checkbox.checked, lastError: "", statusNote: "" });
  render(checkbox.checked, "");
});
