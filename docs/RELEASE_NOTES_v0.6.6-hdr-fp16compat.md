# Magpie Experimental v0.6.6 HDR FP16 Compatibility

## 中文说明

本版本将官方 Magpie 0.6.6 的核心生命周期、NGX 异常保护、DLSSNR history 重置、Renderer session 管理、延迟回调代际校验、窗口状态恢复和帧率控制流程合入本地 HDR 兼容分支。

HDR 兼容链路保留 canonical FP16 工作面、捕获边界路由、正反向归一化、DLSSNR FP16 实验路径、DLSSFG bridge，以及 XeSS、XeSS-FG、FSR、RTX Video、光流和 PassThrough 的 HDR 分支。

关闭 HDR 兼容总开关后，HDR 参数自动回到基线状态并进入线上 U8 处理链；HDR 相关设置只在总开关开启时生效。参数窗口的位置和尺寸会持久化到配置文件，旧配置继续使用默认尺寸。

完整安装包为 `Magpie-Experimental-x64.zip`，包含 `Magpie.exe`、`resources.pri`、`TouchHelper.exe`、`Updater.exe`、运行时 DLL、效果文件和许可证文件。

## English

This release brings the official Magpie 0.6.6 core lifecycle changes into the local HDR-compatible branch, including NGX exception guards, DLSSNR history reset on input revision changes, renderer session lifetime handling, delayed-callback generation checks, window-state restoration, and the updated frame-rate control.

The HDR pipeline keeps its canonical FP16 working surface, capture boundary routing, forward and inverse normalization, DLSSNR FP16 experimental path, DLSSFG normalization bridge, and HDR branches for XeSS, XeSS-FG, FSR, RTX Video, optical flow, and PassThrough.

When the global HDR compatibility switch is off, HDR settings are reset to their baseline state and the online U8 processing chain is selected. HDR-specific settings take effect only while the global switch is enabled. Effect-parameter window position and size are persisted; older configurations use the default size.

The complete package is `Magpie-Experimental-x64.zip` and includes `Magpie.exe`, `resources.pri`, `TouchHelper.exe`, `Updater.exe`, runtime DLLs, effect files, and license files.
