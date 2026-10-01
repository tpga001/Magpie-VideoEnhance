# Magpie Experimental v0.5.7

这是面向 NVIDIA RTX 用户的 x64 实验版本，重点改善 DLSSNR 参数配置、DLSS 组合兼容性和 DLSS Frame Generation 的启动与呈现稳定性。

## 下载与安装

1. 下载 `Magpie-Experimental-x64.zip`。
2. 完全退出正在运行的 Magpie。
3. 将 ZIP 完整解压到新目录，不要直接在压缩包内运行，也不建议覆盖旧实验版目录。
4. 运行解压目录中的 `Magpie.exe`。

已有配置会自动迁移，但不会自动替换用户现有的缩放模式。v0.5.6 用户可以在“缩放模式”页面导入 `ScalingModes-v0.5.7-experimental.json`，追加 `DLSSFG`、`XeSSFG` 和 `DLSSNR` 三个预设。

## 自 v0.5.6 以来的变化

### DLSSNR

- 新增 `NR Preset`、`Skin Structure Strength` 和 `NR UI Correction`。
- `NR Preset` 提供 `0 Default`、`1 Preset #1`、`2 Preset #2`、`3 Preset #3`。
- `NR Style` 提供 `0 Default`、`1 Natural`、`2 Cinematic`。
- `NR Intensity`、`Local Tone Strength` 和 `Local Structure Strength` 范围为 `0–2`，默认值为 `1`。
- `Skin Structure Strength` 范围为 `-1–2`，默认值为 `-1`。
- `Automatic Mask` 和 `NR UI Correction` 默认关闭。
- 深度估算改为异步更新，减少启用 Frame Guidance 时的周期性卡顿。

### DLSS SR 与组合使用

- 主 DLSS 超分效果更名为 `DLSS SR_Experimental`，内部标识改为 `DLSS\DLSS_SR`。
- 旧 `DLSS\DLSS_ZeroMV` 配置会自动迁移，并保留参数和缩放方式。
- 改善 DLSS SR 改变处理分辨率后再连接 DLSSNR 时的兼容性。
- 改善 DLSSNR 与 DLSS Frame Generation 同时使用时的初始化、重建和退出稳定性。

### DLSS Frame Generation

- 修正估算运动矢量的尺度，提高 x3/x4 中间帧的运动一致性。
- 改善 x3/x4 的生成帧排队和呈现节拍，降低生成帧被覆盖或顺序异常的概率。
- 修复部分启动中鼠标已隐藏但缩放窗口没有显示的问题。
- 改善高刷新率环境中的输出呈现；实际可见帧率仍受显示器刷新率、GPU 性能和应用输入帧率限制。
- `Use Motion Vectors` 默认开启，`Use Estimated Depth (Experimental)` 默认关闭。

## 使用建议

- 不要同时启用 DLSS FG、XeSS FG、NVIDIA Smooth Motion 或其他帧生成方案。
- DLSS FG x3/x4 需要足够高的显示器刷新率；目标帧率超过刷新率时，不会显示全部生成帧。
- Magpie 的 Motion 和 Depth 来自捕获颜色画面的估算，不能等同于游戏引擎原生数据。快速运动、反遮挡、镜头切换和 UI 仍可能出现伪影。
- DLSSNR 是同分辨率 SDR 后处理，不负责放大，可能同时改变文字和 UI；HDR 暂不支持。
- NVIDIA Indicator 不保证在所有环境显示。可以在 `logs\magpie.log` 中搜索 `DLSSNR STATUS` 确认创建和执行状态。

## DLSSNR DLL 选择

主 Release Pack 携带社区修改的 `nvngx_dlssnr.dll` 310.8.0.0，目标是同时兼容 RTX 40 系和 RTX 50 系。该文件不是保持 NVIDIA 原始签名完整性的官方 DLL，Windows Authenticode 会报告哈希不匹配。

同一 Release 的 `DLSSNR-DLL-Options-310.8.0.0.zip` 包含：

- `NVIDIA-Original`：NVIDIA 原版签名文件。
- `Community-RTX40-RTX50`：主 Release Pack 使用的社区兼容文件。

切换 DLL 前请完全退出 Magpie，再替换程序目录中的 `nvngx_dlssnr.dll`。这些第三方 DLL 不包含在源码仓库或 GitHub 自动生成的源码归档中。

## NGX OTA 临时方案

同一 Release 提供 `NGX_OTA_Switch.bat`，用于处理部分环境中 `nvngx_update.exe` 进程异常累积的问题。该工具需要管理员权限，并修改系统级 NGX 设置，可能影响其他使用 NGX 的程序。测试完成后可以删除覆盖设置，恢复 NVIDIA 默认行为。

## English summary

Magpie Experimental v0.5.7 expands the user-facing DLSSNR controls, improves DLSS SR to DLSSNR compatibility, and fixes several DLSS Frame Generation startup, motion-scaling, multi-frame ordering, and high-refresh presentation issues. Existing `DLSS\DLSS_ZeroMV` configurations migrate automatically to `DLSS\DLSS_SR`.

The main x64 package includes a community-modified `nvngx_dlssnr.dll` 310.8.0.0 intended for RTX 40-series and RTX 50-series compatibility. A separate DLL-options ZIP contains both the original NVIDIA-signed runtime and the community version. `NGX_OTA_Switch.bat` and an importable scaling-mode preset file are provided as separate Release assets.
