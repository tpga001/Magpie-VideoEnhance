# Magpie Experimental v0.5.2

## 中文

本版本继续完善实验性帧生成、重启兼容性与 Renderer 内部结构。

### 新增与改进

- 新增 XeSS Frame Generation：
  - 通用显卡 x2 Zero-MV 路径。
  - Intel Arc 显卡 x2-x4 Multi-Frame Generation 路径；实际可用倍率取决于 GPU 与驱动报告的能力。
- 完善 DLSS Frame Generation 失败保护：
  - 失败时继续显示真实捕获帧。
  - 首次失败重置历史，随后最多重建一次。
  - 连续失败后仅在当前缩放会话禁用 DLSSFG，避免无限重试导致卡死。
  - 继续保留 CPU/GPU Fence 同步。
- 提升 Smooth Motion 兼容模式的重启可靠性：
  - 保留普通窗口、最小化或托盘状态。
  - 替代进程等待旧进程完全退出后再初始化。
  - 旧进程会提前释放快捷键，减少重启后的快捷键注册冲突。
- 重构 Renderer 原生后端分派：
  - DLSS、FSR、XeSS 与 RTX Video 等原生后端统一通过 `NativeEffectBackend` 和 `NativeEffectBackendFactory` 管理。
  - 帧生成 Presenter 继续作为最终呈现阶段的独立路径。
- 暂时关闭内置更新检查；本版本不会后台联网检查，也不显示手动检查更新入口。

### 作者的主观使用组合建议

以下建议来自有限游戏和设备上的主观观察，不代表普遍画质结论。SMAA 应放在 DLSS 等上采样 Effect 之前；L/M/J/K 指通过 NVIDIA Profile Inspector 等工具设置的驱动层 DLSS Preset，并非 Magpie 内部选项。关于历史帧和当前帧权重的解释属于现象推测。

#### NVIDIA 显卡

| 使用目标 | 建议组合 | 主观观察与取舍 |
| --- | --- | --- |
| 3D 游戏 AA：更少锯齿 | `SMAA Ultra → DLSS Zero-MV`，驱动 Preset L | L 可能给予历史帧更高权重，边缘更平滑、锯齿更少，但运动时可能更容易保留历史拖影。 |
| 3D 游戏 AA：更少拖影 | `SMAA Ultra → DLSS Jitter`，驱动 Preset M | 虚假 jitter 似乎会提高当前帧影响，M 的当前帧权重也似乎略高于 L；代价是可能出现偶发抖动。 |
| 3D 游戏 AA：更少闪烁 | `SMAA Ultra → DLSS`，驱动 Preset K 或 J | 主观上对细节闪烁更保守；可再根据拖影与抖动表现选择 Zero-MV 或 Jitter。 |
| 2D 游戏、Galgame 和视频 | `RTX Video VSR Ultra` | 优先选择 Ultra；性能消耗过大时改用 `RTX Video VSR High`。 |

#### AMD 显卡

作者尚未在 AMD 显卡上深入对比。建议优先尝试 `FSR2 Zero-MV` 和 `FSR4 Zero-MV`，再根据具体游戏选择；当前 FSR3 路径的主观效果不佳，不建议优先使用。

#### 帧生成

在目前有限的主观对比中，XeSSFG 的插帧效果似乎优于 DLSSFG。通用显卡可先尝试 XeSSFG x2；Intel Arc 可进一步测试 x2-x4 Multi-Frame Generation。不建议（但如果你非要试试的话也不是不行）与 NVIDIA Smooth Motion 或另一个帧生成 Effect 同时使用。

### 注意

- XeSSFG 和 DLSSFG 都是缺少真实运动向量、深度及 UI 分离信息的实验接入，画质、延迟和稳定性不能等同于游戏原生实现。
- 不建议将任何帧生成 Effect 与 NVIDIA Smooth Motion 同时使用，也不建议叠加多个帧生成 Effect；如果仍要尝试，请自行留意兼容性与稳定性。
- XeSS x3/x4 多帧生成主要面向 Intel Arc；其他显卡会被限制或回退到 x2。

## English

This release further improves experimental Frame Generation, restart compatibility, and Renderer internals.

### Added and improved

- Added XeSS Frame Generation:
  - An x2 Zero-MV path for supported GPUs across vendors.
  - An Intel Arc x2-x4 Multi-Frame Generation path; the available multiplier depends on capabilities reported by the GPU and driver.
- Improved DLSS Frame Generation failure protection:
  - Real captured frames remain visible when generation fails.
  - The first failure resets history, followed by at most one feature recreation.
  - Repeated failures disable DLSSFG only for the current scaling session instead of retrying indefinitely.
  - CPU/GPU fence synchronization remains enabled.
- Improved the reliability of Smooth Motion compatibility-mode restarts:
  - Preserve normal-window, minimized, or tray state.
  - The replacement waits for the old process to exit before initialization.
  - The old process releases shortcuts early to reduce registration conflicts after restart.
- Refactored native backend dispatch inside Renderer:
  - DLSS, FSR, XeSS, RTX Video, and other native backends now share `NativeEffectBackend` and `NativeEffectBackendFactory`.
  - Frame-generation presenters remain independent terminal presentation paths.
- Temporarily disabled built-in update checking. This release does not perform background update requests or expose the manual update-check interface.

### Author's subjective combination suggestions

These suggestions are subjective observations from a limited set of games and systems, not universal image-quality conclusions. Place SMAA before an upscaling Effect such as DLSS. L/M/J/K refer to driver-level DLSS Presets selected through tools such as NVIDIA Profile Inspector, not options built into Magpie. The explanations about current-frame and history weighting are hypotheses based on observed behaviour.

#### NVIDIA GPUs

| Goal | Suggested combination | Subjective observations and trade-offs |
| --- | --- | --- |
| 3D-game AA: fewer jagged edges | `SMAA Ultra → DLSS Zero-MV`, driver Preset L | L may assign more weight to history, producing smoother edges and fewer jaggies, but it may retain more history ghosting in motion. |
| 3D-game AA: less ghosting | `SMAA Ultra → DLSS Jitter`, driver Preset M | The synthetic jitter appears to increase the influence of the current frame, and M also appears to give the current frame slightly more weight than L; occasional visible jitter is the trade-off. |
| 3D-game AA: less flicker | `SMAA Ultra → DLSS`, driver Preset K or J | These Presets appeared more conservative with fine-detail flicker. Choose Zero-MV or Jitter afterwards according to the preferred ghosting-versus-jitter balance. |
| 2D games, visual novels, and video | `RTX Video VSR Ultra` | Prefer Ultra; use `RTX Video VSR High` when Ultra is too expensive. |

#### AMD GPUs

The author has not performed extensive comparisons on AMD GPUs. Try `FSR2 Zero-MV` and `FSR4 Zero-MV` first, then choose per game. The current FSR3 path produced poor subjective results and is not a priority recommendation.

#### Frame Generation

In the limited subjective comparison so far, XeSSFG appeared to produce better interpolation results than DLSSFG. Start with XeSSFG x2 on generally supported GPUs; Intel Arc users can additionally test x2-x4 Multi-Frame Generation. Combining it with NVIDIA Smooth Motion or another Frame Generation Effect is not recommended, though you can still try it if you really want to.

### Notes

- XeSSFG and DLSSFG are experimental colour-frame-only integrations without real motion vectors, depth, or UI separation. Image quality, latency, and stability cannot match native game integrations.
- Combining any Frame Generation Effect with NVIDIA Smooth Motion, or stacking multiple Frame Generation Effects, is not recommended. If you still want to try it, keep an eye on compatibility and stability.
- XeSS x3/x4 multi-frame generation primarily targets Intel Arc; other GPUs are limited or fall back to x2.
