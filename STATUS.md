# Magpie Video Enhance 当前状态

本仓库是基于 [SAOG0721/Magpie](https://github.com/SAOG0721/Magpie) 与 [Blinue/Magpie](https://github.com/Blinue/Magpie) 的源码开发快照，尚无本项目安装包。源码采用 [GPLv3](LICENSE)；第三方 SDK、模型和运行组件遵循各自许可，见[依赖与再分发说明](docs/THIRD_PARTY_AND_REDISTRIBUTION.md)。

## 已实现的视频路径

- Chrome／Edge 扩展识别播放中的主要视频，并把视频帧和画面位置送到 Magpie；增强层跟随视频区域，避免放大浏览器工具栏和书签栏。实现与限制见[浏览器解码帧说明](docs/BROWSER_DECODED_FRAMES_20260927.md)。
- 64 位 PotPlayer 可通过独立的 DirectShow 视频接入组件和控制器，把本地视频交给 Magpie 做超分与补帧；安装、适用范围与回退方法见[本地视频说明](docs/experimental/testing/LOCAL-VIDEO.md)。
- 源码保留原有整窗口效果路径，并包含 RTX Video、XeSS FG 等实验后端。实际可选效果取决于构建选项、设备和运行组件。

## DLSS 构建与运行边界

默认源码构建在 [`src/BuildOptions.props`](src/BuildOptions.props) 中关闭 DLSS FG、DLSS SR、DLSS NR；它们是可选构建功能，所需 SDK 与运行组件不随本仓库提交。开发者可以在本地构建中开启相应选项，但编译成功不等于当前设备能初始化或稳定运行。

DLSS NR 有三个独立状态：`EnableDLSSNR=true` 只表示 NR 代码已编译；`PackageDLSSNRRuntime=true` 才会尝试把由构建者单独取得并核验的 `nvngx_dlssnr.dll` 随包复制，默认关闭，构建脚本只检查文件是否存在；当前 RTX 3080 能否实际处理画面仍要看运行库是否支持该 GPU，以及真实视频链路的初始化和持续输出。缺少运行库时，效果会透传并在“最近一次问题”的详情中说明原因。构建清单分别记录编译开关、随包开关与实际文件是否存在。

- **DLSS SR**：已在隔离候选包中通过初始化和持续处理测试；尚未作为本项目正式发布包验收。见[候选包验证记录](docs/experimental/reviews/20260928-native-super-resolution-runtime-smoke.md)。
- **DLSS NR**：源码存在可选后端；真实浏览器和本地视频链路仍需单独验证画质、性能及稳定性。
- **DLSS FG**：源码存在可选后端，但 RTX 3080 不在 [NVIDIA 官方 DLSS Frame Generation 支持范围](https://www.nvidia.com/en-us/geforce/technologies/dlss/)内。实验兼容路径若另行使用，也须逐机验证，不能以构建开关证明可用。

XeSS FG 的 3×／4×属于实验兼容模式；SDK 输出帧数不能代替屏幕实际显示帧率。进行性能结论前应记录原始输入、显示帧间隔和卡顿分布。

## 2026-10-01 本机 DLSS 候选

从提交 `556b1a3` 构建的私有候选已安装到测试机，构建清单中的 DLSS SR、FG、NR 均为 `true`，而 `PackageDLSSNRRuntime=false`。SR/FG 运行库与 NVIDIA 官方 SDK 310.9.1 文件哈希相同且签名有效；NR 运行库未随包提供。安装后 205 个清单文件的哈希全部匹配，原用户配置未改变，主窗口可正常唤起。本项目仍未发布公开安装包。

这些结果证明三项代码已编译、可在界面中选择，不代表三项都能在当前 RTX 3080 上处理视频。旧版隔离测试证明过 SR 可在该显卡上工作；本次候选的合成窗口自动缩放测试没有启动，因此尚未完成新的真实视频链路验收。FG 仍受 NVIDIA 的 GPU 支持范围约束；NR 缺少可信运行库，只会记录错误并透传。详见[本次构建与验收记录](docs/experimental/reviews/20261001-dlss-compile-deploy.md)。
