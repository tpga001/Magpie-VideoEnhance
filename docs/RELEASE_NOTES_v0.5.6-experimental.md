# Magpie Experimental v0.5.6

这是面向 NVIDIA RTX 用户的 x64 实验版本，主要更新 DLSS Super Resolution、DLSS Frame Generation 和 DLSSNR 的实验接入。

## 主要变化

- 新增 `DLSS SR_Experimental` 非 Jitter 路径，可选择输入 Magpie 从捕获画面估算的运动矢量和深度。
- 新增 `DLSS FG_Experimental` 2x/3x/4x，并可复用同一组 Frame Guidance。
- DLSS SR 和 DLSS FG 的 `Use Motion Vectors` 默认开启；`Use Estimated Depth (Experimental)` 默认关闭。
- Jitter DLSS SR 路径保持原样；XeSS Frame Generation 本次没有接入新的 Guidance。
- DLSSNR 使用同分辨率 SDR AI 滤镜路径，并通过日志中的 `DLSSNR STATUS` 报告创建和 Evaluate 状态。

Magpie 无法取得游戏引擎原生运动矢量、深度或 UI 分离信息。本版本使用从捕获色彩帧估算的数据，因此画质、延迟和稳定性不能等同于游戏原生 DLSS 接入。

## DLSSNR 运行库

二进制 Release 中的 `nvngx_dlssnr.dll` 310.8.0.0 是面向 RTX 40 系和 RTX 50 系测试的社区修改版本，并非保持 NVIDIA 原始签名完整性的官方原版文件。修改会导致 Windows Authenticode 报告文件哈希不匹配。

该 DLL 仅作为 GitHub Release 的二进制资产分发，不属于项目源码，也不包含在本仓库或 GitHub 自动生成的源码归档中。请在了解实验性质和风险后使用；若安全软件拦截，请自行判断，不建议关闭系统安全防护。

## NGX OTA 临时方案

同一 GitHub Release 提供 `NGX_OTA_Switch.bat`，用于处理部分环境中 `nvngx_update.exe` 大量累积的问题。它可临时关闭系统级 NGX OTA、结束现有更新进程，或删除设置以恢复 NVIDIA 默认行为。该工具需要管理员权限，并可能影响其他使用 NGX 的程序。

下载和文件哈希请以 [v0.5.6-experimental GitHub Release](https://github.com/SAOG0721/Magpie/releases/tag/v0.5.6-experimental) 为准。

## English summary

This experimental x64 release adds shared estimated motion and optional depth inputs to the non-jitter `DLSS SR_Experimental` and `DLSS FG_Experimental` paths. Motion vectors default to on, estimated depth defaults to off, and XeSS Frame Generation is unchanged.

The binary Release uses a community-modified `nvngx_dlssnr.dll` 310.8.0.0 intended for RTX 40-series and RTX 50-series testing. It is a binary Release asset only and is not included in this repository or GitHub's generated source archives. The same Release also provides `NGX_OTA_Switch.bat` as a temporary system-wide NGX OTA workaround.
