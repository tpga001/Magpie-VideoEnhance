# Magpie Experimental v0.5.8 使用说明

> **更新提示：** 这个版本主要解决窗口模式的初始缩放倍数不能直接设为 `1x` 的问题。如果你不需要使用 1x 窗口模式缩放，可以不用下载这个版本。与 v0.5.7 相比，本版本没有其他较大的兼容性优化或功能改动。

## 这个版本适合谁

当你只想使用 DLSSNR AI Filter、RTX Video 降噪等同分辨率滤镜，不希望画面先被放大或重采样时，可以使用新的 `1x` 初始缩放倍率。

原有的“自动”、1.25x、1.5x、1.75x、2x、3x 和“自定义”选项保持原来的含义。已有配置可以继续使用。

## 应该下载哪个文件

### 完整包：`Magpie-Experimental-x64.zip`

推荐新安装、不是从 v0.5.7 更新，或者不确定当前文件是否完整的用户下载。

1. 完全退出正在运行的 Magpie，包括托盘中的 Magpie。
2. 将 ZIP 完整解压到一个新目录。
3. 不要直接在压缩包内运行，也不要只复制其中的 `Magpie.exe`。
4. 运行新目录中的 `Magpie.exe`。

### 最小更新包：`Magpie-v0.5.8-Minimal-Update-from-v0.5.7.zip`

仅适用于当前正在使用本项目发布的 `v0.5.7-experimental` 完整包、且不想重新下载全部运行时文件的用户。

1. 完全退出 Magpie，并确认任务管理器中没有 `Magpie.exe`。
2. 备份旧目录中的 `Magpie.exe` 和 `resources.pri`。
3. 将最小更新包中的 `Magpie.exe` 和 `resources.pri` **一起**复制到 v0.5.7 程序目录并覆盖原文件。
4. 启动 Magpie，在“关于”页面确认版本为 v0.5.8，然后检查“窗口模式缩放 → 初始缩放倍数”中是否出现 `1x`。

不能只替换 `Magpie.exe`。新版下拉选项位于 `resources.pri`，而选项对应的程序逻辑位于 `Magpie.exe`；两者版本不一致会造成显示倍率与实际倍率错位。如果当前版本不是 v0.5.7、曾单独替换过程序文件，或更新后出现异常，请改用完整包。

## 如何使用 1x

1. 打开需要使用的配置文件。
2. 找到“窗口模式缩放 → 初始缩放倍数”。
3. 在下拉列表中选择 `1x`。
4. 选择 DLSSNR AI Filter、RTX Video 降噪或其他同分辨率效果链，然后启动窗口模式缩放。

`1x` 会使用精确的 1.0 初始倍率。其他倍率以及“自动”模式的行为没有改变。

## 已知限制

1x 仍使用现有的窗口模式缩放框架。启用裁剪、源程序使用自绘非客户区，或者在 Windows 10 上使用 Graphics Capture/WGC 时，可能看到源窗口边缘或黄色捕获边框。

## 与 v0.5.7 相同的内容

- DLSS SR_Experimental、DLSS FG_Experimental 和 DLSSNR AI Filter。
- XeSS FG、FSR 2/3 Zero-MV、XeSS Zero-MV、RTX Video VSR/降噪及实验性 Frame Guidance。
- `DLSSNR-DLL-Options-310.8.0.0.zip`、`NGX_OTA_Switch.bat`、`Toggle_DLSS_Debug_Info.bat` 和 `ScalingModes-v0.5.7-experimental.json`。

主完整包继续携带社区修改的 `nvngx_dlssnr.dll` 310.8.0.0，目标是兼容 RTX 40 系和 RTX 50 系。该文件不是保持 NVIDIA 原始签名完整性的官方 DLL，Windows Authenticode 会报告哈希不匹配。

## English summary

v0.5.8 mainly adds a directly selectable `1x` initial scale factor for windowed mode. If you do not need 1x scaling, there is no need to update from v0.5.7; this release does not contain other major compatibility improvements or feature changes.

The minimal update ZIP is only for an intact v0.5.7-experimental installation. Replace both `Magpie.exe` and `resources.pri` together. Never replace the executable alone, because the UI definition and its selection mapping must come from the same build.
