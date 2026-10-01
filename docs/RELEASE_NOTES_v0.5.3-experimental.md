# Magpie Experimental v0.5.3

## 中文

这是一个针对 DLSSFG 和 XeSSFG 输入帧率异常的热修复版本。

### 帧生成基础帧过滤

- 修复鼠标移动时，重复捕获画面可能被 DLSSFG 或 XeSSFG 视为新基础帧，导致输入或提交帧率虚高的问题。
- 启用任意帧生成 Effect 时，强制使用逐像素重复帧检测；即使处于 3D 游戏模式或用户关闭常规重复帧检测也仍然生效。
- 帧生成启用期间不再使用 Magpie 的最低帧率计时器合成重复基础帧。
- XeSSFG 不再向代理交换链提交仅由 Magpie 软件光标或叠加层变化触发的前端 Present；光标会随下一张真实捕获帧更新。

这项修复会增加一次现有的 GPU 重复帧比较，但可以避免在空更新上运行完整效果链和帧生成，通常能减少无效开销并改善帧生成节奏。

### 注意

- 这里过滤的是完全相同的捕获颜色帧。如果游戏把软件光标直接绘制进游戏画面，光标变化仍属于真实像素变化，Magpie 无法在捕获后可靠地将它与场景分离。
- DLSSFG 和 XeSSFG 仍缺少游戏引擎提供的真实运动向量、深度及 UI 分离信息，画质、延迟和稳定性不能等同于原生接入。

## English

This is a hotfix for incorrect DLSSFG and XeSSFG input-frame rates.

### Frame Generation base-frame filtering

- Fixed repeated captured images being treated as new DLSSFG or XeSSFG base frames while the mouse is moving, which could inflate the reported input or submission rate.
- Enabling any Frame Generation Effect now forces exact per-pixel duplicate-frame detection, including in 3D-game mode and when normal duplicate detection is disabled by the user.
- Magpie's minimum-FPS timer no longer synthesizes repeated base frames while Frame Generation is active.
- XeSSFG no longer submits frontend Presents caused only by Magpie's software cursor or overlay to the proxy swap chain. The cursor is updated with the next genuine captured frame.

The fix adds one existing GPU duplicate-frame comparison, but avoids running the full Effect chain and Frame Generation on empty updates. It should generally reduce wasted work and improve Frame Generation pacing.

### Notes

- Filtering applies to completely identical captured colour frames. If a game draws a software cursor directly into its rendered image, the cursor is a real pixel change and Magpie cannot reliably separate it from the scene after capture.
- DLSSFG and XeSSFG still lack engine-provided motion vectors, depth, and UI separation. Their image quality, latency, and stability cannot match native game integrations.
