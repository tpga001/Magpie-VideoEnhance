# 本地视频超分与补帧

## 使用

1. 启动桌面「本地视频增强」，托盘出现开关。
2. 用 64 位 PotPlayer 打开本地视频，并保持播放器在前台。
3. 自动处理视频区域；双击托盘图标开关，右键菜单可退出。

窗口和播放器控制栏保持原大小。主屏按当前屏幕上限处理；1080P 副屏不会强制做 4K。改变窗口大小或跨屏后会重新对齐。暂停、切到其他应用、最小化时撤掉增强层。退出 Magpie 后控制器不会轮询重启它；要再次启用，手动关闭再开启增强。

浏览器增强仍用浏览器扩展开关。本地增强独立开关，复用已安装的 Magpie。一次只处理前台播放器或浏览器的视频。

## 本版范围

- 64 位 PotPlayer、默认皮肤、正常画面比例的 SDR 视频；30 FPS 测试素材已验证。
- RTX Video VSR 超分与现有光流/XeSS-FG 两倍补帧。30 FPS 源的目标是 60 FPS；24/25 FPS 源的两倍结果是 48/50 FPS，尚未分别验收。当前控制器只接受约 20–33 FPS，其余源帧率暂不增强。
- 解码输入支持 NV12、I420/IYUV、YV12、RGB32/ARGB32，最大画面面积为 3840×2160。滤镜输出独立的 RGB32 缓冲给播放器，同时按播放时钟向 Magpie 提供原始尺寸的 RGBA 帧。
- 主屏 1080P 源可内部处理到 3840×2160，再映射回原视频区域；这不会使窗口占用全屏，也不等于创造了超出物理屏幕的像素密度。
- 1080P 源在 1080P 副屏不做无意义的 4K 超分；补帧仍可启用。
- HDR/P010、旋转、非方形像素、特殊皮肤、自定义裁切和播放速度尚不支持或未验收；外置字幕、播放器 OSD 不属于解码视频帧，不能保证在覆盖区域保留。遇到这些场景请关闭本地增强。
- 这次验证使用无声 VP8/WebM 测试素材，不能覆盖所有容器、解码器、音轨或音画同步。实际显示稳定 60 FPS 和长时间音画同步另行验收。
- 关闭增强会停止向 Magpie 发布帧。已加载的 DirectShow 滤镜仍参与 RGB32 转换；彻底恢复原始解码渲染链路可在 PotPlayer 外部滤镜中禁用「Magpie Local Video Tap」，再重新打开视频。
- 保留 GPUPixel 原有独立滤镜及配置，不自动启用美颜；本版没有验收与美颜同时运行。

## 构建与安装

依赖本机 Visual Studio 2022 C++、Windows SDK 10.0.26100.0、.NET 9 Desktop Runtime，以及已有的 DirectShow BaseClasses 与静态 libyuv x64 库。默认复用 `C:\tmp\potplayer-gpupixel-bridge` 下的构建依赖；`Build-LocalVideo.ps1` 提供路径参数，可在其他机器指定依赖。

```powershell
pwsh -NoProfile -File scripts/Test-LocalVideo.ps1
pwsh -NoProfile -File scripts/Deploy-Local068.ps1 -BuildOnly -IncludeLocalVideo
# 关闭测试及已安装的 Magpie，再用既有校验安装器安装干净提交的构建清单
pwsh -NoProfile -File scripts/Install-ValidatedRuntime.ps1 -RuntimeDirectory <构建目录> -Destination <安装目录>
# 关闭 PotPlayer 和本地增强控制器，再注册当前用户滤镜并创建桌面入口
pwsh -NoProfile -File scripts/Install-LocalVideo.ps1 -RuntimeDirectory <安装目录>
```

注册只添加本组件自己的 CLSID 和 PotPlayer 外部滤镜项，保留其他滤镜设置。安装前备份 `HKCU\Software\DAUM\PotPlayerMini64\Override`，输出备份位置。没有增加登录启动任务。

回退时先退出本地增强和 PotPlayer，再禁用本组件的外部滤镜；主程序可从 `Install-ValidatedRuntime.ps1` 返回的备份目录还原。不要直接导入很早的完整注册表备份覆盖后续用户设置。

## 验证与性能边界

`Test-LocalVideo.ps1` 验证通道顺序、上下方向、带行尾填充的数据边界，以及晚到帧的发布时间。`tests/LocalVideoBehavior.ps1` 仅操作标题以 `magpie-local-` 开头的测试播放器，记录前台归属、增强层位置、原始帧推进和暂停行为。

2026-09-27 跨屏复测记录在 `output/local-video/validation/20260927-230721-primary` 到 `20260927-230817-background`：主屏 33/33、副屏 43/43、返回主屏 33/33、恢复播放 33/33 个样本可见；暂停和切到测试应用时均 0/10 可见。前四个播放阶段没有前台丢失。该轮通过功能验收，不是稳定 60 FPS 的测量。

性能优化另见工作区 `performance-optimization-plan-20260927.md`。本功能不更改全局 Present/VSync 策略。
