# Magpie Experimental v0.6.7 Beta 1

相较 0.6.6 的更新：

- 新增效果器选择器，支持用途分类、搜索、同族折叠和字母快速定位。
- 为效果器增加用途说明、组合建议和入门／进阶分类，方便选择适合的效果。
- RTX Video 降噪与 VSR 支持实时调节四档强度，默认中档，并兼容旧效果组。
- 完善 DLSS SR、DLSS NR 和 XeSS 补帧的分类、名称与用途说明。
- DLSS NR、DLSS FG 和光流调试效果新增 AMD／NVIDIA 光流及质量选择。
- 所有效果器默认关闭光流，旧效果组升级时也会关闭一次，可在参数中重新开启。
- DLSS NR 自动适配 SDR／HDR 输入，减少手动设置。
- 优化 DLSS NR 和 DLSS FG 的运行开销。
- DLSS FG 新增 Reflex 低延迟支持，默认开启，Boost 默认关闭。
- 帧同步新增 Async 模式，并支持为未使用补帧的效果组启用 NVIDIA Reflex。
- 新增 HDR → SDR、SDR → HDR 和 RTX Video HDR 效果，HDR 默认关闭，可通过“HDR 组件”搭建转换效果组。
- 增强版配置与原版分开保存，升级时自动沿用旧设置。
- 新增损坏配置自动恢复，尽量保留原有效果组，并提示替换失效效果。
- 错误提示增加解决步骤，主页可查看问题详情和日志。
- 窗口模式遇到全屏或最大化应用时，会提示先将源应用切换为普通窗口。
- 修复添加效果器和停止缩放时的闪退，以及参数设置失效的问题。
- 修复列表空行、窄窗口文字显示和部分帮助链接，并增加缩放启动成功提示。
- 修复便携配置保存和旧预设补帧倍率不生效的问题。

Contributor: [TurnX-alt](https://github.com/TurnX-alt) 提供界面与预设修复；[konodiodaaaaa1](https://github.com/konodiodaaaaa1) 提供 HDR 支持。

---

# Magpie Experimental v0.6.7 Beta 1

Changes since 0.6.6:

- Added an effect picker with purpose categories, search, collapsible families and an alphabetical index.
- Added effect descriptions, combination advice and Beginner/Advanced categories to help choose suitable effects.
- RTX Video Denoise and VSR now offer four live strength levels, default to Medium and support existing groups.
- Improved the categories, names and descriptions of DLSS SR, DLSS NR and XeSS frame generation.
- DLSS NR, DLSS FG and optical-flow diagnostics now offer AMD/NVIDIA optical flow and quality selection.
- Optical flow is off by default for all effects and is disabled once when upgrading existing groups, with the option to enable it again in parameters.
- DLSS NR automatically adapts to SDR/HDR input, reducing manual setup.
- Optimized the processing overhead of DLSS NR and DLSS FG.
- Added Reflex low-latency support to DLSS FG, enabled by default with Boost off.
- Frame synchronization adds Async mode and NVIDIA Reflex for groups without frame generation.
- Added HDR to SDR, SDR to HDR and RTX Video HDR effects under HDR Components for custom conversion groups, with HDR off by default.
- Enhanced settings are saved separately from the original Magpie, with automatic import of existing settings during upgrade.
- Added automatic recovery of damaged settings, retaining groups where possible and identifying effects that need replacement.
- Error messages now provide recovery steps, with issue details and logs accessible from Home.
- Windowed mode asks users to switch fullscreen or maximized source applications to a normal window before scaling.
- Fixed crashes when adding effects or stopping scaling, along with parameter settings that failed to apply.
- Fixed blank list rows, text display in narrow windows and some help links, and added a confirmation when scaling starts.
- Fixed portable settings saving and frame-generation multipliers in older presets.

Contributor: [TurnX-alt](https://github.com/TurnX-alt) contributed UI and preset fixes; [konodiodaaaaa1](https://github.com/konodiodaaaaa1) contributed HDR support.
