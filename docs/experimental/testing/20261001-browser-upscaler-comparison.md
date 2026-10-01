# 浏览器视频超分对照说明

日期：2026-10-01。本页用于逐项验证画质；目前保持 RTX Video VSR Ultra 为浏览器自动模式的默认超分，补帧链仍为现有 XeSS FG 3×。切换超分时不要同时改补帧倍率、浏览器窗口、视频源或屏幕。

## 已准备的选项

| 切换值 | 超分后端 | 当前验收边界 |
| --- | --- | --- |
| `RTX-VSR` | NVIDIA RTX Video VSR Ultra | 现用对照项；本轮不重新证明画质与帧率 |
| `DLSS-SR` | DLSS SR，预设 K，零运动向量 | 旧候选曾以预设 J 完成合成画面 2× 连续处理；当前 K 的正式浏览器视频待验 |
| `XeSS-SR` | Intel XeSS SR，零运动向量 | 已有独立候选的合成画面 2× 初始化与连续处理记录；正式浏览器视频待验 |
| `FSR3-SR` | AMD FSR 3.1.5 SR，零运动向量 | 已有独立候选的合成画面 2× 初始化与连续处理记录；正式浏览器视频待验 |

上述旧记录仅覆盖同一动态合成画面 `938×524 → 1876×1048`、WGC 捕获、首帧与至少 120 次后端发布，见[原生超分隔离运行验证](../reviews/20260928-native-super-resolution-runtime-smoke.md)。捕获画面没有游戏引擎的真实深度和运动向量，因此不能把各方案的效果等同于游戏内原生集成，也不能据此排名。FSR 4 在 RTX 3080 上仅有未完成运行验收的实验性 INT8 兼容路径，本切换工具不开放它。

## 切换与回退

1. 在 Magpie 托盘菜单中退出程序。PowerShell 7 打开项目目录 `E:\LocalConfig\magpie-universal-ai-screen-lab\Magpie-VideoEnhance`。
2. 先预览，例如 `./scripts/Set-BrowserVideoUpscaler.ps1 -Mode DLSS-SR -Preview`。将 `DLSS-SR` 换成表中的任一切换值；预览不改配置。
3. 去掉 `-Preview` 执行切换，然后重新打开 Magpie。脚本只替换浏览器自动模式中的一个超分效果，保留 30 FPS 输入过滤、XeSS FG、模式顺序和其他配置。每次写入前会生成一个带时间戳的配置备份，并在输出中给出完整路径。
4. 需要恢复某次切换前配置时，先退出 Magpie，再执行 `./scripts/Set-BrowserVideoUpscaler.ps1 -RollbackBackup '<上一步输出的完整备份路径>'`，随后重启 Magpie。也可以切回 `-Mode RTX-VSR`；它同样会备份当前配置。

如果自动模式结构与预期不同、Magpie 仍在运行或写入期间配置被别处改变，脚本会拒绝覆盖。当前版本的浏览器自动识别固定使用名为「浏览器视频 30fps → 当前屏幕原生60fps」的模式；在普通缩放模式列表中选别的模式，不会切换浏览器自动增强。

## 建议的主观对照

选同一段 1080P 视频，在同一屏幕和窗口尺寸上依次查看四个选项，观察小字、人物边缘、低码率块状纹理、快速运动与遮挡。每次至少连续播放三分钟，同时记录播放器统计中的源帧率、Magpie 日志的后端初始化状态和实际显示帧率。不要用画面最终尺寸或 SDK 的补帧输出计数代替画质与流畅度判断。浏览器补帧的独立排查见[补帧卡顿改善计划](../plans/20261001-browser-frame-pacing-plan.md)。

尤其要确认日志出现 `Browser decoded source initialized`，并显示约 1920×1080 的解码输入，以及所选 `DLSS SR initialized`、`XeSS ... initialized` 或 `FSR 3.1.5 ... initialized`。如果浏览器没有提供解码帧映射，Magpie 可能只捕获已按 CSS 放大的约 4K 视频矩形；这时输入和目标等尺寸，原生超分会旁路，不能用该画面判断 1080P→4K 算法效果。副屏 1080P 视频输入与显示目标同尺寸时同样会旁路，属于预期行为。
