# Magpie 托盘模式主窗口恢复与超分切换说明

日期：2026-09-30。正式运行目录：`E:\Program Files\Magpie-Experimental-x64`。

## 结果

Magpie 以 `-t` 启动后，托盘图标存在，但主窗口可能已经创建为隐藏窗口。旧版 `MainWindow::Show()` 只处理最小化窗口；再次启动程序或点击托盘图标时，对隐藏窗口只请求前台，不会将它显示出来。

已修复两条唤出路径：托盘模式中新建主窗口后立即显示；主窗口已存在但隐藏时，先显示再请求前台。未更改视频效果配置。当前本地安装版本为 `0.6.9-tray-window-fix-local`。

## 验证

- 先以 `-t` 启动，确认无主窗口；再正常启动同一个 `Magpie.exe`，主窗口在约 0.3 秒内创建并可见，3 秒后仍可见。
- 向主窗口发送正常关闭消息后，Magpie 继续驻留托盘；再次正常启动，主窗口重新创建且可见，进程未重启。
- Release x64 完整构建成功；运行包清单包含 201 个文件。安装后程序版本、清单版本及 `Magpie.exe` SHA-256 一致，哈希为 `A61B7161FB579DE715BA0C985DA7E0F450F3C461EC0DAE65673F84E83C82415B`。
- 新安装版日志确认浏览器自动增强仍进入 RTX Video VSR 质量 4 的绘制路径；本次没有重新测量实际显示帧率。

安装前的可回退备份位于 `E:\LocalConfig\magpie-universal-ai-screen-lab\installed-backups\tray-window-fix-20260930-2320`。独立候选运行包位于 `E:\LocalConfig\magpie-universal-ai-screen-lab\release\v0.6.9-tray-window-fix-local\Magpie-Experimental-x64`。部署使用带逐文件哈希校验和失败回滚的 `scripts/Install-ValidatedRuntime.ps1`。

## 用户操作

点击任务栏通知区域的 Magpie 图标即可打开主窗口。在左侧“效果组”中编辑 **`浏览器视频 30fps → 当前屏幕原生60fps`**：其中 `RTXVideo_VSR` 是当前超分效果，`XeSS_FrameGeneration` 是 2 倍补帧效果。不要改动效果组名称；浏览器自动模式按该名称查找效果组，不读取左侧“默认”配置所选的普通效果组。更改效果后，先结束当前浏览器增强会话，再重新播放或重新开启扩展开关；同一会话会复用当前效果链。

当前浏览器区域到屏幕内部处理尺寸的适配只对 `RTXVideo_VSR` 实现。界面虽然允许移除它并添加 `DLSS SR` 等效果器，但直接替换尚不能证明仍保持主屏内部 4K、副屏原生分辨率或相同稳定性。换算法前需要先适配该输出尺寸路径，再按画质、实际显示帧率和稳定性验收。
