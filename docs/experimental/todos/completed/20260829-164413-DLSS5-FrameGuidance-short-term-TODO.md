# DLSS5 Frame Guidance 短期 TODO

> 创建时间：2026-08-29 16:44:13（Asia/Shanghai）
> 原执行分支：`experimental/dlssnr`（已合并至 `experimental`）
> 状态：阶段 1–4 工程实现完成；启用/禁用实验 Provider 的 Release x64 构建均通过，待 GPU 运行时验收
> 后续：性能、pass-through 和 DLSSNR 可观测性转入 [`20260829-DLSSNR-performance-observability-TODO.md`](../20260829-DLSSNR-performance-observability-TODO.md)

## 目标

为捕获帧建立独立于 DLSS Effect 的深度与运动引导数据管线，并按以下顺序推进：

1. 引入 `FrameGuidanceService` 和 Zero Provider，不改变当前画质与默认行为。
2. 接入 NVIDIA Optical Flow，提供运动矢量与置信度调试可视化。
3. 接入 Depth Anything V2，确认深度方向并解决短时跨帧不稳定。
4. 首先只让 DLSS5/NR 消费真实引导数据。

本阶段不接入 DLSS SR、DLSS FG，不处理 HDR、UI 分离、异步多帧深度推理和正式发行授权。

## 固定的数据约定

- `FrameGuidanceService` 由 `Renderer` 每个渲染会话持有，Provider 不由 DLSS Effect 创建。
- 每个真实捕获帧只生成一次引导数据；FG 中间帧、重复帧不得推进 Provider 历史。
- Motion：`R16G16_FLOAT`，当前帧到上一帧，单位为源图像像素。
- Depth：`R32_FLOAT`，归一化相对逆深度，近处为 1、远处为 0，标记 `depthInverted=true`。
- 每份输出携带 `frameId`、源尺寸、有效区域、有效性、是否需要历史重置。
- resize、切场、捕获中断、设备重建和长时间停顿统一触发 `ResetHistory()`。
- Provider 失败或本帧结果无效时，消费者必须自动取得 Zero Provider，不中断渲染。

## 阶段 1：FrameGuidanceService + Zero Provider

- [x] 新增 `FrameGuidanceTypes`，定义 Depth、Motion、Confidence、坐标约定和同步信息。
- [x] 新增 `IDepthProvider`、`IMotionVectorProvider`，Provider 生命周期为 `Initialize / BeginFrame / Reset / Resize`。
- [x] 新增 `ZeroDepthProvider` 和 `ZeroMotionVectorProvider`，复用一组零填充纹理。
- [x] 新增 Renderer 级 `FrameGuidanceService`，按 `frameId + source extent` 缓存结果。
- [x] 给 native backend 的绘制上下文增加只读 `FrameGuidanceView`，禁止 backend 直接持有具体 Provider。
- [x] 将 DLSSNR、DLSS SR、DLSS FG 内部重复创建的零 Depth/MV 逐步改为服务输出；本阶段允许先只迁移 DLSSNR，但接口必须能覆盖三者。
- [x] 保留全部原有效果名、默认参数和失败回退。

实现记录（2026-08-29）：当前仅 DLSSNR 消费 Renderer 级共享 Zero Guidance；DLSS SR/FG 的消费接入仍按“暂不处理”保留。Frame Guidance 只在 DLSSNR 出现在效果链时分配纹理，其他默认路径不增加显存占用。真实捕获帧推进 `frameId`，强制重复帧复用缓存；resize 会重建零纹理并传播 history reset。Release x64 全解决方案编译通过，数值、resize/reset 与 Feature 18 Evaluate 仍需在目标 NVIDIA GPU 上完成运行时验收。

验收：Zero Provider 的格式、尺寸、数值、Reset 和 DLSSNR 参数与当前实现一致；不开启后续 Provider 时输出观感及运行路径不变。

## 阶段 2：NVIDIA Optical Flow Provider

- [x] 增加独立构建开关 `EnableNvidiaOpticalFlow`，运行时动态加载驱动提供的 `nvofapi64.dll`，不随项目分发该 DLL。
- [x] 使用 Renderer 的 D3D11 设备创建唯一 NVOF Session，并查询输入格式、输出 grid、双向流和 cost 能力。
- [x] 以当前帧为 `inputFrame`、上一帧为 `referenceFrame`，生成“当前到上一帧”的 forward flow。
- [x] 将 NVOF 的 S10.5 输出解码、稠密化并转换为规范化的 `R16G16_FLOAT` 像素位移。
- [x] 启用 `R8_UNORM` cost/confidence；高 cost 转换为低 confidence。
- [x] 可用时生成 backward flow，使用前后向一致性标出遮挡和错误矢量。
- [x] 首帧、切场、resize、丢帧时关闭 temporal hint 并输出零矢量。
- [x] 保持 NVOF API 调用串行，避免多个 DLSS 消费者各自创建 Session。

调试可视化作为独立诊断 Effect/Backend，不属于 DLSSNR：

- [x] `Diagnostics\\FrameGuidance_Motion`：HSV 显示方向和幅度，可调显示倍率。
- [x] `Diagnostics\\FrameGuidance_Confidence`：灰度显示置信度，低置信区域突出显示。
- [x] 增加水平/垂直平移约定自检，明确 X/Y 符号、像素单位和 grid 上采样；实际素材幅度由 GPU 验收确认。

实现记录（2026-08-29）：NVOF 使用单一 Renderer Session，从系统驱动目录动态加载 API；优先 4/2/1 grid，支持时启用双向流和 cost。GPU compute shader 将 S10.5 稠密化为源分辨率像素位移，并以 cost 与前后向一致性共同生成置信度。编译期覆盖水平正向、垂直负向的 S10.5 解码约定，运行时日志记录 API、grid、preset 和 frameId。

验收：静止画面接近零矢量；已知平移方向和幅度正确；遮挡边界置信度明显下降；重复帧不会污染历史。

## 阶段 3：Depth Anything V2 Provider

- [x] 默认采用 `Depth-Anything-V2-Small` FP16 ONNX；模型、运行时和许可证放在独立 Provider 目录，不放入 DLSSNR Effect 目录。
- [x] 定义独立的 `IDepthInferenceBackend`，同时实现 `TensorRTDepthBackend` 和 `DirectMLDepthBackend`，两者共用相同的模型输入、预处理、后处理和输出约定。
- [x] 固定使用 FP16，不在本阶段引入 FP8、FP4、INT8、Q/DQ 量化或量化校准流程。
- [x] TensorRT FP16 作为默认主后端；ONNX Runtime DirectML FP16 作为备用后端；两者均失败时回退 `ZeroDepthProvider`。
- [x] 内部测试阶段同时初始化 TensorRT 和 DirectML，DirectML 保持热备用但不同时执行；记录两套 Session 的初始化时间和显存占用。
- [x] TensorRT 从同一份 FP16 ONNX 在本机生成并缓存 engine，不分发通用 `.engine` 文件。
- [x] TensorRT engine cache key 至少包含 ONNX SHA-256、TensorRT/CUDA 版本、GPU 能力、输入尺寸和构建参数；任一不匹配时重建。
- [x] TensorRT DLL/CUDA 缺失、Adapter 不匹配、ONNX 解析、engine 构建/加载、显存分配或运行失败时切换到 DirectML。
- [x] TensorRT 运行中失败时，本帧输出 Zero Depth；完成 GPU 同步后从下一真实帧启用 DirectML，并同时重置 Depth 历史和 `DLSSNR.Reset`，本次会话不反复重试 TensorRT。
- [x] 固定模型版本、ONNX opset、输入归一化、尺寸策略和 SHA-256。
- [x] 保持输入宽高比，推理尺寸对齐 ViT patch 的 14 倍数；输出上采样为源帧尺寸的 `R32_FLOAT`。
- [x] 先显示 raw depth 与 inverted depth，使用近景/远景明确判断输出极性。
- [x] 禁止每帧独立 min/max；使用稳定百分位范围与 EMA，记录 P02/P98 和尺度漂移。
- [x] 使用当前到上一帧的 NVOF 重投影上一帧深度，生成 temporal residual 调试纹理。
- [x] 低光流置信度、遮挡显露和切场区域降低历史权重或直接使用当前推理结果。
- [x] 第一版每个真实帧同步推理；确认正确后再评估隔帧推理与异步执行。

验证视图：

- [x] `Diagnostics\\FrameGuidance_Depth`：深度灰度、反向切换、百分位裁剪开关。
- [x] `Diagnostics\\FrameGuidance_DepthResidual`：当前深度与光流重投影历史的差异热图。

实现记录（2026-08-29）：锁定 ONNX opset 14、`pixel_values`/`predicted_depth`、ImageNet normalization、长边 518 且宽高对齐 14，模型 SHA-256 为 `2DF6223F206B5164E21F664ACE61DABEB9BB6A49B8B5A3E00510B4807D0F5B04`。TensorRT cache key 使用实际 TensorRT/CUDA 版本、CUDA Driver、DXGI LUID 映射后的 CUDA device、SM 能力、输入尺寸和 FP16 构建参数。本机 DirectML runtime/export 预检通过；当前 PATH 缺少 cuDNN 9，TensorRT provider 预检返回 Win32 126，因此本机首次测试预计走 DirectML 热备用。

验收：TensorRT 正常时使用 TensorRT FP16；人为破坏 TensorRT 初始化或 engine cache 时能稳定切换 DirectML；两者失败时使用 Zero Depth；近远方向稳定；静止场景没有全局明暗泵动；缓慢平移时轮廓不明显拖尾；切场或后端切换后一帧内完成历史重置。

## 阶段 4：仅接入 DLSS5/NR

- [x] DLSSNR 从 `FrameGuidanceView` 获取 D3D12 可见的 Depth/Motion 资源，删除其私有零纹理职责。
- [x] 由共享 GPU 互操作层管理 D3D11/D3D12 资源视图、状态转换、Fence 和生命周期。
- [x] Depth、Motion、Color 必须具有相同 `frameId`；不一致时整组回退为 Zero Provider。
- [x] 设置 `DLSSNR.DepthInverted=1`，完整填写 Depth/MVec subrect。
- [ ] 用固定平移素材分别测试 `DLSSNR.MVecScale=1` 与归一化尺度，确认未公开接口的实际约定后再固定。
- [x] Provider reset 同步映射到 `DLSSNR.Reset=1`。
- [x] 在 Effect 参数中只暴露“使用可用 Frame Guidance/强制 Zero”与必要调试项，不暴露或实例化具体 Provider。
- [ ] 比较 Zero、仅 NVOF、仅 DAV2、NVOF+DAV2 四组结果。

实现记录（2026-08-29）：新增共享 D3D11/D3D12 NT-handle/Fence 互操作层，DLSSNR 只接收同一 frameId 的完整引导组；资源失配时整组 Zero。`DepthInverted=1`、Depth/MVec subrect、reset 和状态转换已接线。`Guidance Mode` 提供 Available、Force Zero、Motion Only、Depth Only 四种测试模式；`MVecScale` 暂按源像素约定设为 1，最终值等待固定平移素材验收。

验收：Feature 18 持续 Evaluate 成功；平移和运动物体场景相较 Zero 输入没有新增方向性拖尾；Provider 故障可无闪退地回退；关闭 Provider 后恢复当前行为。

## 必测场景与记录项

- [ ] 静止画面、匀速水平/垂直平移、快速转场、前景横穿、镜面/透明物体、固定 UI、resize、暂停恢复、重复捕获帧。
- [ ] 保存 Color、Depth、Motion、Confidence、DepthResidual 的截图或 DDS。
- [ ] 记录 NVOF、Depth 推理、格式转换、DLSSNR Evaluate 的 GPU/CPU 耗时和显存占用。
- [x] 日志包含 Provider 名称、模型哈希、NVOF grid/preset、frameId、reset 原因和回退原因。

## 暂不处理

- DLSS SR、DLSS FG 的实际消费接入。
- 相机矩阵、真实 near/far/FOV 推导。
- HUD-less、UI Alpha 和畸变场。
- HDR 与色彩空间适配。
- Base/Large Depth Anything 模型和非商业许可证模型的打包。
- FP8、FP4、INT8 及其他量化推理路径。
- 自动下载模型、正式发布和第三方再分发。

## 参考

- NVIDIA NVOFA Programming Guide: <https://docs.nvidia.com/video-technologies/optical-flow-sdk/nvofa-programming-guide/index.html>
- Depth Anything V2: <https://github.com/DepthAnything/Depth-Anything-V2>
- ONNX Runtime DirectML: <https://onnxruntime.ai/docs/execution-providers/DirectML-ExecutionProvider.html>
- NVIDIA DLSS-G Programming Guide: <https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuideDLSS_G.md>
