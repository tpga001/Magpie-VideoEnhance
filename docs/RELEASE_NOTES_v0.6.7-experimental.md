# Magpie Experimental v0.6.7

## 更新内容

### Magpie 本体

- **全新的效果器选择器**：支持用途分类、搜索、同族折叠、入门／进阶筛选和字母定位，并提供效果说明与搭配建议；切换为英文界面时，分类、说明、引导和搜索内容会同步显示为英文。
- **可选的参数焦点切换**：默认配置和各应用配置可独立设置“参数调整焦点切换”，新配置默认关闭，使用基于 0.6.6 的鼠标操作方式。开启后支持编辑／预览切换及 `Ctrl+单击` 输入数值，但在部分窗口上运行时可能不稳定。
- **可选的切屏自动停用**：主页“高级”新增“切屏时自动停用生效效果组”，默认关闭。开启后，使用 `Alt+Tab`、`Win+Tab` 等切屏组合键时自动停用全屏效果组，切回来后需手动重新启用；可用于缓解部分 DLSSNR 切屏异常。
- **新增帧同步模式**：在 Front Edge Sync 之外增加 Async 和 NVIDIA Reflex；普通效果与 DLSS FG 均可选择 Reflex 基础限帧，主页和参数面板共用同一组设置。
- **独立保存增强版配置**：与原版 Magpie 的设置分开保存，支持导入旧配置。工具栏快捷键提示及 FAQ、帮助入口同步完善。

### 效果器

- **RTX Video 降噪与 VSR**：增加低／中／高／极高四档实时强度，默认中档；旧效果组保留对应强度。
- **DLSSNR 强度调整**：NR 强度、局部色调强度和局部结构强度扩大至 `0–2`，默认 `1`；皮肤结构强度为 `0–2`，默认 `0`，步进均为 `0.05`。
- **更多光流选择**：DLSS NR、DLSS FG 和光流诊断效果支持 AMD／NVIDIA 光流及质量选择。光流默认关闭，从旧配置首次迁移时也会关闭一次，之后可手动开启并保存。
- **新增 HDR 组件**：提供 HDR → SDR、SDR → HDR 和 RTX Video HDR，可按源内容与显示需要搭配使用。HDR 默认关闭，通过添加组件启用。

### 错误与兼容性

- 改善全屏下调整参数、切换窗口和重新启用效果组时的稳定性，修复部分场景中画面停止更新的问题。
- 修复添加效果、删除效果组或停止缩放时可能出现的闪退。
- 修复配置目录缺失导致无法启动、便携配置保存失败等问题；配置损坏时尝试从备份或有效条目恢复，并提示需要处理的失效效果。
- 修复部分 HDR 内容亮度显示不正确的问题，改善 HDR 与不同效果组合的兼容性。
- 完善错误详情和解决步骤。窗口模式遇到全屏或最大化的源应用时，会提示先切换为普通窗口。

## 使用说明

### 安装或从旧版升级

**请使用全新程序目录安装，不直接覆盖 0.6.6 或旧 Beta 目录。**

1. 如需保留设置或截图，先备份到程序目录之外。
2. 从托盘完全退出 Magpie。
3. 将 `Magpie-Experimental-x64.zip` 完整解压到新目录，运行其中的 `Magpie.exe`。程序与 `resources.pri` 必须来自同一次构建，不要仅替换 EXE。

升级后发现同名效果组时，原内容会保留，请逐个改名；出现失效效果时，可恢复对应效果文件，或移除失效项后添加替代效果。

旧版全局“参数调整焦点切换”会迁移到已有配置；已有单独设置优先保留，之后新建的配置默认关闭。

## 附件的作用与使用

| 附件                               | 用途与使用方法                                                                                                                                                                                        |
| ---------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `Magpie-Experimental-x64.zip`      | **必选主包**，包含程序、匹配的界面资源、效果与所需运行组件；按上述步骤完整解压。其余附件均为可选。                                                                                                    |
| `DLSSNR-DLL-Options-310.8.0.0.zip` | 提供 NVIDIA 官方版、RTX 40/50 社区兼容版和 SF-v2 三种 DLL 选择，附中英双语 README；仅需切换 NR DLL 时下载。完全退出 Magpie，备份现有 `nvngx_dlssnr.dll`，按包内说明选择一个版本放到 `Magpie.exe` 旁。 |
| `NGX_OTA_Switch.bat`               | 沿用 0.6.6 的可选工具，用于查看、开关 NVIDIA NGX OTA 更新及清理更新进程，常规安装无需运行。相关操作需管理员权限且影响系统级 NGX 设置；恢复时使用 **Restore default**，删除 BAT 不会撤销设置。         |

Contributor: [TurnX-alt](https://github.com/TurnX-alt) 提供界面、预设与构建一致性修复；[konodiodaaaaa1](https://github.com/konodiodaaaaa1) 提供 HDR 支持。

---

# Magpie Experimental v0.6.7

## Updates

### Magpie Application

- **New effect picker** with purpose categories, search, collapsible families, Beginner/Advanced filtering and alphabetical navigation, plus effect descriptions and combination advice. Categories, guidance, descriptions, and search content now switch to English with the English interface.
- **Optional focus switching for parameters**: The default profile and each application profile can independently set Switch focus for parameter adjustment. New profiles default to Off for mouse interaction based on 0.6.6. Enabling it provides Edit/Preview switching and `Ctrl+click` numeric entry, but may be unstable with some windows.
- **Optional stop on task switching**: Advanced on Home adds Disable active effects when switching tasks, disabled by default. When enabled, combinations such as `Alt+Tab` and `Win+Tab` stop fullscreen effects, which must be re-enabled manually after returning. This can help with some DLSSNR task-switching issues.
- **Additional frame-sync modes**: Async and NVIDIA Reflex join Front Edge Sync. Ordinary effects and DLSS FG can use Reflex base pacing, with shared settings on Home and in the parameter panel.
- **Separate enhanced settings** from the original Magpie, with support for importing older configurations. Shortcut tooltips, FAQ links and help access are also improved.

### Effects

- **RTX Video Denoise and VSR** offer Low/Medium/High/Ultra strength levels that apply live, defaulting to Medium. Existing groups retain their corresponding strength.
- **Expanded DLSSNR controls**: NR intensity, local tone strength and local structure strength now range from `0–2`, defaulting to `1`. Skin structure strength ranges from `0–2`, defaulting to `0`. All four use steps of `0.05`.
- **More optical-flow choices**: DLSS NR, DLSS FG and optical-flow diagnostics support AMD/NVIDIA methods and quality levels. Optical flow defaults to Off and is disabled once when first migrating older settings; users can then enable and save their choice.
- **New HDR Components**: HDR to SDR, SDR to HDR and RTX Video HDR can be combined for the source content and display. HDR defaults to Off and is enabled by adding components.

### Errors and Compatibility

- Improved stability when adjusting parameters in fullscreen, switching windows and re-enabling effect groups; fixed cases where the image could stop updating.
- Fixed possible crashes when adding effects, deleting effect groups or stopping scaling.
- Fixed startup failures caused by missing configuration directories and portable-setting save failures. Damaged settings can recover from backups or valid entries, with invalid effects identified for attention.
- Fixed incorrect brightness in some HDR content and improved compatibility between HDR and different effect combinations.
- Improved error details and recovery steps. Windowed mode asks users to switch fullscreen or maximized source applications to a normal window first.

## Usage

### Install or Upgrade

**Install into a new program folder; do not overwrite a 0.6.6 or older Beta folder.**

1. Back up any settings or screenshots you want to keep outside the program folders.
2. Fully exit Magpie from the system tray.
3. Extract `Magpie-Experimental-x64.zip` completely into a new folder and run its `Magpie.exe`. Keep the EXE and `resources.pri` from the same build together; do not replace only the EXE.


Existing groups with duplicate names retain their contents and should be renamed individually. For an invalid effect, restore its file or remove the entry and add a replacement.

The former global focus-switching choice migrates to existing profiles, preserving any individual choices. Profiles created afterward default to Off.

## Assets: Purpose and Instructions

| Asset                              | Purpose and Instructions                                                                                                                                                                                                                                                                                                       |
| ---------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `Magpie-Experimental-x64.zip`      | **Required main package**, containing the application, matching UI resources, effects and runtime components. Extract it completely as described above. All other assets are optional.                                                                                                                                         |
| `DLSSNR-DLL-Options-310.8.0.0.zip` | Offers official NVIDIA, community RTX 40/50-compatible and SF-v2 DLL choices, with a Chinese/English README. Download only when switching NR DLLs. Fully exit Magpie, back up `nvngx_dlssnr.dll`, and follow the archive instructions to place one choice beside `Magpie.exe`.                                                 |
| `NGX_OTA_Switch.bat`               | Reuses the optional 0.6.6 tool to inspect or toggle NVIDIA NGX OTA updates and clean up update processes; normal installation does not require it. Relevant actions require administrator privileges and affect system-wide NGX settings. Use **Restore default** to undo changes; deleting the BAT does not restore settings. |

Contributor: [TurnX-alt](https://github.com/TurnX-alt) contributed UI, preset and build-consistency fixes; [konodiodaaaaa1](https://github.com/konodiodaaaaa1) contributed HDR support.
