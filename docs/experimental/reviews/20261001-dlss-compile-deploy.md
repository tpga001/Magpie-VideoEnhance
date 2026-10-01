# DLSS 三项编译与本机安装记录

日期：2026-10-01。此记录针对私有本机候选，不是公开发行版。

## 结果

| 功能 | 编译进程序 | 随包运行库 | 本机运行结论 |
|---|---|---|---|
| DLSS SR | 是 | NVIDIA 官方 `nvngx_dlss.dll`，签名有效 | RTX 3080 上旧候选曾完成 WGC 2× 持续处理；本次新候选尚未完成真实视频验收 |
| DLSS FG | 是 | NVIDIA 官方 `nvngx_dlssg.dll`，签名有效 | RTX 3080 不在官方 FG 支持范围；选择效果不能绕过硬件限制 |
| DLSS NR | 是 | 无；打包选项关闭 | 代码可选，但缺少可信 `nvngx_dlssnr.dll` 时记录错误并透传，不应误认为已处理画面 |

构建源提交为 `556b1a310c9774ec575dac8382d09d807c07f1b0`，版本为 `0.6.9-dlss-all-candidate`，`sourceDirty=false`。构建清单明确记录 `EnableDLSSSR=true`、`EnableDLSSFrameGeneration=true`、`EnableDLSSNR=true`、`PackageDLSSNRRuntime=false`、`bundledRuntimeFiles.nvngx_dlssnr=false`。候选 ZIP 的 SHA-256 为 `BE41BA765ECE26F629B88CBB67674033635DDBCEA7B1536A65E3E69138CA4A70`。安装后对清单中的 205 个文件逐一复核，均与 SHA-256 记录一致。

本次 SDK 来自 [NVIDIA DLSS SDK 310.9.1](https://github.com/NVIDIA/DLSS/releases/tag/v310.9.1)。其中 `nvngx_dlss.dll` 与 `nvngx_dlssg.dll` 和 SDK 原件哈希一致，Windows 签名检查均为 `Valid`。SDK 没有提供项目现有 NR 后端所需的 `nvngx_dlssnr.dll`；SDK 中的 `nvngx_dlssd.dll` 是光线重建组件，不能替代它。正式安装目录中此前残留的 NR DLL 签名检查为 `HashMismatch`，安装时已单独移入本机备份，未执行或上传。

## 运行验证边界

- 新程序安装后保持运行；再次启动同一程序能唤起已有实例的主窗口。用户配置文件哈希与安装前备份相同；Chrome／Edge Native Messaging 注册路径仍指向原安装目录，PotPlayer 接入文件与注册未改动。
- 两次独立 WGC 合成窗口测试均未触发自动缩放，测试记录中的 `fixtureExitCode=1`、`initialized=false`。第一次出现托盘图标创建错误，提权重试不再出现该错误，但仍未进入效果初始化。因此这两次测试不能证明或否定新候选的 DLSS SR 算法处理能力；不能写成 SR 运行通过。
- [2026-09-28 的旧候选测试](20260928-native-super-resolution-runtime-smoke.md)曾在同一 RTX 3080 上记录 `938×524 → 1876×1048`、首帧及至少 120 次后端发布。那不是本次二进制的运行证据。
- 本次没有重新播放浏览器或 PotPlayer 的真实视频，也没有测量帧率、掉帧、画质或延迟。切换效果前应保持当前默认模式，后续逐项做真实播放验收。

## 选择与回退

用户可以在 Magpie 的缩放模式中选择 DLSS 效果；编译开关不限制用户选择。若 SR 初始化失败，可切回原 RTX VSR／XeSS FG 模式。FG 在当前 RTX 3080 上预计会由 NVIDIA NGX 拒绝；NR 在未提供可信运行库前不会执行神经处理。程序的“最近一次问题”与日志用于区分这些状态。

安装前文件与配置已单独备份，浏览器扩展、BrowserVideoHost、LocalVideoHost、PotPlayer Tap 与注册信息未整体替换。若主界面、浏览器或本地视频出现回归，可逐文件恢复原版；不要把编译成功当成三项功能的完成验收。
