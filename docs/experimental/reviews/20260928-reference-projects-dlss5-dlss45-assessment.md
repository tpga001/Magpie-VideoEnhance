# NeuralScreen、SAOG Magpie 与 dlssg_for_sm86 更新评估

日期：2026-09-28  
范围：RTX 3080 上的浏览器／本地视频区域超分与补帧  
状态：只构建和运行隔离实验包，未覆盖正式安装目录

## 结论

1. **当前不能把浏览器或本地视频的超分切换为“DLSS 5”。** NVIDIA 官方把 DLSS 5 定义为 3D-Guided Neural Rendering。它依赖游戏引擎提供的颜色帧、运动矢量、时序状态、控制参数和逐像素遮罩，目前面向 RTX 50 系列。屏幕捕获链路只有颜色画面和估计光流，RTX 3080 也不在官方支持范围内。
2. **RTX 3080 当前正确的升级目标是 DLSS 4.5 Super Resolution。** 它仍是 DLSS Super Resolution，第二代 Transformer 模型通过 Preset L／M 提供；Preset K 是 RTX 30 上更现实的画质与速度平衡点。
3. 本机已完成一个隔离候选：官方签名的 `nvngx_dlss.dll 310.9.1.0`、K／L／M 模型选择、按实际缩放倍率自动选择 NGX Quality Mode。K、M、L 均通过真实 WGC 2×捕获短测，但尚未部署。
4. 三个参考项目都没有可直接拿来实现“RTX 3080 视频 DLSS 5 超分”的更新。最值得借鉴的是窗口变尺寸后的首帧显示、重复帧过滤、Feature 重建代际隔离、分层能力诊断，以及对运行库参数是否真实生效的测试方法。

官方资料：

- [NVIDIA DLSS 5 研究页](https://research.nvidia.com/labs/adlr/DLSS5/)
- [NVIDIA DLSS 5 开发者说明](https://developer.nvidia.com/blog/whats-new-for-game-developers-dlss-5-with-3d-guided-neural-rendering-nvidia-ace-updates-and-new-rtx-kit-capabilities)
- [NVIDIA DLSS 产品页](https://www.nvidia.com/en-us/geforce/technologies/dlss/)
- [NVIDIA DLSS 4.5 Super Resolution 说明](https://www.nvidia.com/en-us/geforce/news/dlss-4-5-super-resolution-available-now/)
- [NVIDIA DLSS SDK Releases](https://github.com/NVIDIA/DLSS/releases)

## DLSS 5 与 DLSS 4.5 的功能区别

| 名称 | 实际功能 | 输入条件 | RTX 3080 视频捕获链路 |
|---|---|---|---|
| DLSS 4.5 Super Resolution | 第二代 Transformer 时域超分 | 低分辨率颜色、运动信息、抖动等 | 可实验；捕获链路缺少原生游戏运动矢量与深度，需使用零运动或估计光流 |
| DLSS 5 | 3D-Guided Neural Rendering，生成最终材质、照明和外观 | 游戏引擎内部 G-Buffer／运动／控制遮罩／时序状态 | 不可正式接入；硬件与输入契约都不满足 |
| DLSSNR Feature 18 | 私有运行库中的同分辨率神经画面增强 | 同尺寸输入输出及私有参数契约 | 本机独立探针可运行，但来源、签名和再分发条件不满足正式部署 |
| DLSS-G／MFG | 插帧 | 引擎级运动、深度、交换链和 Streamline 契约 | `dlssg_for_sm86` 可实验绕过硬件门禁，但风险高；当前产品链继续使用已验证的 XeSSFG |

把 Feature 18 称为“DLSS 5 超分”会混淆功能。当前产品链应使用以下名称：

```text
DLSS 4.5 SR（真实升分辨率）
    → 可选的实验性神经画面增强
    → XeSSFG（30→60 或按屏幕刷新率）
```

## 三个参考项目的更新

### 1. NeuralScreen

检查结果：主分支最新提交为 [`74210d9`](https://github.com/perseval-BLR/NeuralScreen/commit/74210d925a802e381fc44cda242db56a8c022260)，最新 Release 为 [v2.1.8](https://github.com/perseval-BLR/NeuralScreen/releases/tag/v2.1.8)。

可参考更新：

| 更新 | 对当前方案的价值 |
|---|---|
| 窗口尺寸变化后，等第一张正确尺寸的帧成功 Present 再显示 overlay | 可减少调整浏览器／PotPlayer 窗口时短暂出现旧尺寸画面 |
| 独立高优先级呈现队列，去掉不必要的等待 | 值得先用 ETW／PresentMon 验证 Magpie 是否存在相同等待，再决定是否改队列 |
| 只把真正的新 DDA／WGC 帧送入 FG | 与现有重复帧过滤方向一致，可继续统一所有捕获入口的语义 |
| 启动 FG 时清空陈旧的 waitable 信号 | 可防止效果重建后使用上一代的呈现节奏状态 |
| 区分静态画面与捕获中断 | 静止画面不应重置时域历史；断流或时间戳倒退才重置 |
| 扫描 DLL 参数字符串、对参数输出做哈希和冻结帧稳定性测试 | 适合验证私有 Feature 18 的参数是否真实存在、是否真的改变输出 |

不能直接采用：

- NeuralScreen 当前使用私有／泄露来源的 `nvngx_dlssnr.dll` Feature 18，并采用 PolyForm Strict 许可。不能复制代码或重新分发该 DLL。
- 它为旧 GPU 注入并修改 NVAPI 架构信息。本机 RTX 3080 已在含 `sm_86` 的候选运行库上无需伪装完成 Feature 18 Create 和 Evaluate，不应引入全局架构伪装。
- 它曾尝试 DLSS SR，后来因桌面捕获发糊而删除。这说明真实视频画质必须 A/B 验证，不能仅凭 Feature 初始化成功就部署。

### 2. SAOG0721/Magpie

检查结果：默认分支 `experimental` 最新提交为 [`3841698`](https://github.com/SAOG0721/Magpie/commit/3841698348bfb246623d4acf791984c8b68a577b)，最新 Release 为 [v0.6.8-experimental.1](https://github.com/SAOG0721/Magpie/releases/tag/v0.6.8-experimental.1)。本地以该提交为基线，当前 `0 behind / 22 ahead`。

结论：

- SAOG 公开分支没有新的 DLSS 5 或 DLSS 4.5 SR 实现需要合并。
- 本地已经包含它现有的 DLSSNR、DLSS SR、共享 Guidance、DLSSFG 和 XeSSFG 代码，并额外具备浏览器视频区域、解码帧传输、前台约束、窗口几何跟随和 PotPlayer 接入。
- 公开的 [PR #44](https://github.com/SAOG0721/Magpie/pull/44) 是效果启用／禁用交互，不改变 DLSS 算法。

值得继续保留的上游思路包括 DLSSNR 多 Pass、时域残差稳定、共享光流和 XeSSFG 对重复捕获帧的过滤；这些本地已有，不需要重复移植。

### 3. dlssg_for_sm86

检查结果：最新版本为 [v0.3.5](https://github.com/sdli1995/dlssg_for_sm86/releases/tag/v0.3.5)，对应提交 [`9621db5`](https://github.com/sdli1995/dlssg_for_sm86/commit/9621db573e07ed54f50c15bbb585ed9a7bdfac28)。

可参考更新：

| 版本 | 修复 | 应独立实现的设计 |
|---|---|---|
| 0.3.5 | Feature 反复创建后可能绑定旧优化内核，导致花屏、崩溃或驱动重置 | 每次 Feature 创建分配 generation ID；资源、内核和 fence 只能属于当前 generation |
| 0.3.4 | 架构伪装泄漏到 NVIDIA SR 组件，RTX 30 可能 GPU hang | 所有 NVIDIA SR／NGX 组件必须看到真实 GPU；任何实验能力门禁必须限定在最小调用边界 |
| 0.3.3 | 宿主、Streamline、NGX Core、runtime 各自存在门禁 | 把能力检测拆成分层日志，明确失败发生在哪一层 |
| 路线图 | 提到 Transformer SR 的 INT8 优化 | 当前 v0.3.5 未实现，不能计入现有能力 |

不应把它的 `version.dll` 代理直接放进 Magpie：该项目通过 DLL 侧载、LoadLibrary hook、架构伪装和 GPU kernel 替换工作，公共仓库也不足以提供完整、可复现、可审计的构建。它解决的是 DLSS-G／MFG，不是 DLSS 5 或 DLSS SR。

## 本机 DLSS 4.5 SR 隔离候选

### 实现内容

- `nvngx_dlss.dll` 从 310.7.0 升级为 NVIDIA 官方 310.9.1.0。
- 增加四个模型选项：J、K、L、M；默认 K。
- 根据真实输入／输出比例选择 NGX Quality Mode：
  - 同尺寸：DLAA
  - 输入比例 `< 0.42`：Ultra Performance
  - `< 0.54`：Performance
  - `< 0.62`：Balanced
  - 其余：Quality
- 保留旧效果别名固定使用 J，避免旧配置静默改变模型。
- 运行时日志明确记录 Quality Mode、Preset、输入尺寸和输出尺寸。

### 构建身份

| 项目 | 值 |
|---|---|
| 隔离运行目录 | `E:\LocalConfig\magpie-universal-ai-screen-lab\release\native-upscaler-lab-a957f81\Magpie-NativeUpscaler-Lab` |
| Magpie SHA-256 | `0B5C23EED3DFEC11FB4C5E06A9959EDCBD0ACEAF6037994A3E1AE649CC7E7619` |
| DLSS runtime | `310.9.1.0` |
| DLSS SHA-256 | `3975567B8943C53ACCE397F2B72380092F84F162D00B0D2C7D08A1025C563983` |
| Authenticode | `Valid`，签名者 NVIDIA Corporation |
| 正式安装目录 | 未覆盖，构建核验为 `stableRuntimeUntouched=true` |

### 真实 WGC 2×短测

固定输入 `938×524`，输出 `1876×1048`，零运动向量契约，8 秒动画窗口；每档要求 Feature 成功创建、第一帧成功呈现并记录至少 120 帧。

| 模型 | NGX 模式 | 结果 | 120 帧 CPU fence 等待均值 | 最大值 | 说明 |
|---|---|---:|---:|---:|---|
| K | Performance | 通过 | 4.424 ms | 9.336 ms | 当前 RTX 3080 默认候选 |
| M | Performance | 通过 | 8.514 ms | 22.726 ms | 开销明显高于 K |
| L | Performance | 通过 | 4.990 ms | 24.602 ms | 均值接近 K，峰值波动更大 |

这些数值是当前发布路径里的 CPU fence 等待，不是纯 GPU kernel 时间；它们只能用于同一夹具的初步相对比较。三档都证明了“可创建并连续 Evaluate”，尚未证明真实视频中 M／L 的画质收益足以抵消性能代价。

测试证据：

- K：`.tools/native-super-resolution/capture-20260928-230024/capture-verification.json`
- M：`.tools/native-super-resolution/capture-20260928-230158/capture-verification.json`
- L：`.tools/native-super-resolution/capture-20260928-230227/capture-verification.json`
- 构建核验：`.tools/native-upscaler-lab/build-verification.json`

测试期间 K 和 M 各有一次自动配置未触发，重试后通过。失败日志没有进入 NGX 初始化，因此属于自动匹配夹具波动，不是模型运行失败。后续应把测试入口改为显式请求缩放，减少这项噪声。

## 建议的功能推进顺序

### 下一步 1：真实视频 K／M／L 画质对照

在同一段 1080P 视频、同一窗口尺寸、同一屏幕上分别测试：

- 字幕、招牌和细线是否稳定；
- 人脸皮肤和头发是否过度生成；
- 横移、镜头切换、进度条和 UI 边缘是否出现水波、拖影；
- 实际呈现 FPS、GPU 占用、显存、掉帧与延迟。

RTX 3080 默认从 K 开始。只有 M 或 L 在动态画质上有肉眼可复现的提升，并且仍能稳定达到目标帧率，才允许进入自动策略。

### 下一步 2：确定增强顺序

对照以下两条链：

```text
A. DLSS 4.5 SR → Feature 18 神经增强 → XeSSFG
B. Feature 18 神经增强 → DLSS 4.5 SR → XeSSFG
```

Feature 18 仍只属于实验分支。若运行库签名、来源与分发条件不能解决，即使画质更好也不能进入正式包。

### 下一步 3：性能与稳定性

1. 增加纯 GPU timestamp，分开记录捕获、光流、SR、增强、FG 和 Present。
2. 参考 NeuralScreen 实现“新尺寸第一帧 Present 后再显示”的状态机。
3. 给所有可重建的 NGX／FG Feature 增加 generation ID，丢弃上一代资源和 fence 回调。
4. 在同一视频上比较零运动、AMD 光流和 NVIDIA 光流；没有画质收益时跳过光流。
5. 根据屏幕与视频区域实际尺寸选择目标：1080P 副屏不强制生成 4K；4K 主屏才启用相应 SR 比例。

### 自动策略候选

```text
视频区域输出像素接近源像素
    → 跳过 SR，只做必要的增强／补帧

输出像素明显高于源像素，RTX 30
    → 默认 DLSS 4.5 Preset K

用户选择画质优先且实测余量充足
    → 允许 Preset M／L 实验档

目标刷新率为 60 Hz 且源视频约 24/25/30 fps
    → XeSSFG 补到显示器可稳定呈现的帧率
```

正式部署前仍需完成真实浏览器和 PotPlayer 视频 A/B；当前隔离结果不应自动替换现有稳定配置。
