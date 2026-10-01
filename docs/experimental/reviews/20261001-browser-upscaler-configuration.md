# 浏览器超分对照配置与安装记录

日期：2026-10-01。正式安装已更新，浏览器自动模式的默认超分仍为 RTX Video VSR Ultra，XeSS FG 仍为原有 3×；本轮只增加可切换的超分后端和尺寸保护，没有调补帧时序。

## 安装结果

| 项目 | 结果 |
| --- | --- |
| 安装目录 | `E:\Program Files\Magpie-Experimental-x64` |
| 版本与源码 | `0.6.9-upscaler-compare-10ce75de`，`10ce75de410ae35ba1ae97463f84e306da49f828`，构建清单 `sourceDirty=false` |
| 干净候选包 | `E:\LocalConfig\magpie-universal-ai-screen-lab\release\upscaler-compare-clean-10ce75de-20261001\Magpie-VideoEnhance-x64.zip` |
| ZIP SHA-256 | `4E029BE4CF9DD2FBCC38BFE452FBA928F2B044CAD8C49F0282EDBF8D19C0F2F8` |
| 备份 | `E:\LocalConfig\magpie-universal-ai-screen-lab\installed-backups\upscaler-compare-20261001-122120`，含原运行文件和原配置副本 |

候选清单中的 208 个文件及 ZIP 内对应内容均逐项通过大小和 SHA-256 核对。与安装前目录相比，清单内只有 `Magpie.exe`、`Magpie.RtxVideo.dll`、`README-Experimental.txt`、`resources.pri`、`TouchHelper.exe`、`Updater.exe` 六个 payload 文件内容不同；安装脚本对整个清单做备份、复制、复核和失败回滚。`libxess.dll`、AMD FidelityFX loader/upscaler、`nvngx_dlss.dll`、`nvngx_dlssg.dll` 的 Windows 签名均为 `Valid`，并与本地官方 SDK 原件哈希相同。未把缺乏可信运行库的 DLSS NR DLL 放入包中。

安装后 Magpie 从原目录运行（PID 70108，12:21:32 启动），新 `Magpie.exe` 哈希与候选包一致。用户配置文件在安装前后均为 SHA-256 `5705E412185309B79BBE6FF385B39A708F1584E3F9A8306C4F59F9EDCEEFA31A`。四个选项在正式配置上执行只读预览均能准确识别：`RTX-VSR` 已选，`DLSS-SR`、`XeSS-SR`、`FSR3-SR` 均会只替换浏览器自动模式的第二个超分效果；预览没有写配置。

## 已验证和待验证

- Release x64 编译成功，源码和构建清单一致；浏览器几何抖动/缩放策略测试、显示器原生尺寸策略测试通过。切换脚本在隔离配置上通过四模式切换、备份及回退。
- 本轮没有在正式浏览器中逐个播放和比较 DLSS SR、XeSS SR、FSR 3.1.5 SR 的画质、实际显示 FPS 或长帧，也没有声称它们优于 RTX VSR。2026-09-28 的[独立候选冒烟记录](20260928-native-super-resolution-runtime-smoke.md)只证明旧构建在合成画面上能持续处理；其中 DLSS 使用预设 J，本次切换预设为 K。
- 真正的 1080P→4K 验收需要浏览器提供约 1920×1080 解码帧映射，且日志出现所选原生后端的 `initialized`。如果只能捕获已按网页布局放大的 4K 区域，等尺寸超分会旁路。
- 浏览器窗口明显缩放会触发自动模式重建。若只有 1–2 像素抖动却跨过 1×/3× 边界，新增保护会退回线性呈现以保住播放链；回到有效倍率须等下一次几何重建。此运行边界尚未用正式浏览器现场测量。

日后逐项切换和回退见[浏览器视频超分对照说明](../testing/20261001-browser-upscaler-comparison.md)。漏帧与卡顿的分阶段排查见[浏览器视频补帧卡顿改善计划](../plans/20261001-browser-frame-pacing-plan.md)。
