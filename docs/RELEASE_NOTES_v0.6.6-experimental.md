# Magpie Experimental v0.6.6

## 更新内容

1. 目标帧率改为滑条：**15–360 FPS，步进 1**。
2. 参数浮窗记住位置和大小；捕获区域缩小时自动适配，变大后恢复保存尺寸。
3. 修复停止缩放崩溃，支持参数实时更新，并恢复重新启用前的工具栏／分析器状态。
4. 修复 NGX 异常后再次启用效果组可能导致死锁的问题。

## 使用说明

### 安装或从旧版升级

## **建议删除此前所有版本的 Magpie 程序目录（包括旧内测版），再安装本版；不要直接覆盖旧目录。**

1. 如需保留设置或截图，先备份到程序目录之外。
2. 从托盘完全退出 Magpie，删除所有旧版程序目录。
3. 将 `Magpie-Experimental-x64.zip` 完整解压到新目录，运行其中的 `Magpie.exe`。

普通配置位于 `%LOCALAPPDATA%\Magpie\config\v4\config.json`；便携配置位于原程序目录的 `config\config.json`。需要沿用设置时可保留或恢复配置，**不要复制旧版效果目录、DLL 或深度组件到新目录**。

### 参数与帧率设置

- **参数生效方式**：Live／实时立即生效；Restart／重启需点击“应用并重新启用”。原自动重启类图像参数现已改为实时生效。
- **DLSSNR 残差控制**：先开启“调整输入分辨率”，即使比例为 100% 也能使用残差调整。降低比例可减轻性能压力，但会损失部分画面信息。
- **帧同步设置**：可在主页或工具栏的参数面板顶部修改开关和目标 FPS；修改自动保存，点击“应用并重新启用”后统一生效，不需要重启 Magpie。
- **Front Edge Sync**：手动目标表示补帧前帧率，主页填写 `0` 表示按显示器刷新率自动设置，有 FG 时按倍率折算。例如 80 FPS 配合 2 倍补帧，名义输出为 160 FPS；请同步限制目标程序帧率。
- **补帧搭配**：一个效果组只使用一种 FG；出现延迟或性能压力时，可降低光流质量，或关闭 Front Edge Sync 比较表现。

## 附件的作用与使用

| 附件                                    | 用途与使用方法                                                                                                                                                               |
| --------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `Magpie-Experimental-x64.zip`           | **必选主包**，包含程序和所需运行组件；按上面的安装步骤完整解压使用。其余附件均为可选。                                                                                       |
| `DLSSNR-DLL-Options-310.8.0.0.zip`      | 提供 NVIDIA 官方版和 RTX 40/50 社区兼容版 DLL 选项；仅需切换版本时下载。完全退出 Magpie 并备份现有 `nvngx_dlssnr.dll`，再按包内说明选择一个版本放到 `Magpie.exe` 旁。        |
| `NGX_OTA_Switch.bat`                    | 用于查看、开关 NVIDIA NGX OTA 更新及清理更新进程，常规安装无需运行。相关操作需管理员权限且影响系统级 NGX 设置；恢复时使用菜单的 **Restore default**，删除 BAT 不会撤销设置。 |

---

# Magpie Experimental v0.6.6

## Updates

1. Target frame rate now uses a slider: **15–360 FPS, step 1**.
2. The parameter window remembers its position and size, adapts to a smaller capture area, and restores the saved size when the area grows.
3. Fixed a crash when stopping scaling, added live parameter updates, and restored toolbar/profiler state when re-enabling effects.
4. Fixed a possible deadlock when re-enabling effects after an NGX error.

## Usage

### Install or Upgrade

## **We recommend deleting all previous Magpie program folders, including older beta builds, before installing this version; do not install over an old folder.**

1. If you want to keep settings or screenshots, back them up outside the program folders first.
2. Fully exit Magpie from the system tray and delete all old program folders.
3. Extract `Magpie-Experimental-x64.zip` completely into a new folder and run its `Magpie.exe`.

Normal settings are stored at `%LOCALAPPDATA%\Magpie\config\v4\config.json`; portable settings are in the old program folder's `config\config.json`. Keep or restore the configuration if needed, but **do not copy old effects, DLLs or depth components into the new folder**.

### Parameters and Frame Rates

- **Applying parameters**: Live takes effect immediately; Restart requires Apply and restart. Image parameters previously marked Auto restart now apply live.
- **DLSSNR residual controls**: enable Adjust Input Resolution first, even when using 100%. Lowering the percentage reduces processing pressure but loses some image information.
- **Frame-sync settings**: edit the switch and target FPS on Home or at the top of the toolbar's parameter panel. Changes are saved automatically and take effect together with Apply and restart; restarting Magpie is unnecessary.
- **Front Edge Sync**: a manual target is the rate before FG; `0` on Home selects a rate from the display refresh rate, divided by the FG multiplier when applicable. For example, 80 FPS with 2× FG nominally outputs 160 FPS; apply a matching source-application limiter.
- **Combining FG**: use only one FG per group; if latency or GPU pressure is high, lower optical-flow quality or compare with Front Edge Sync disabled.

## Assets: Purpose and Instructions

| Asset | Purpose and Instructions |
| --- | --- |
| `Magpie-Experimental-x64.zip` | **Required main package**, containing the application and its runtime components; extract it completely as described above. All other assets are optional. |
| `DLSSNR-DLL-Options-310.8.0.0.zip` | Contains official NVIDIA and community RTX 40/50-compatible DLL choices; download only when switching versions. Fully exit Magpie, back up `nvngx_dlssnr.dll`, then follow the archive instructions to place one choice beside `Magpie.exe`. |
| `NGX_OTA_Switch.bat` | Inspects or toggles NVIDIA NGX OTA updates and cleans up update processes; normal installation does not require it. Relevant actions require administrator privileges and affect system-wide NGX settings; use **Restore default** to undo changes, since deleting the BAT does not restore settings. |
