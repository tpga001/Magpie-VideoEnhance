# 161 个效果器内容 review

日期：2026-09-08。由 Sol（中等推理强度）完成逐项初稿与修订，主代理核对清单、抽查内容并整理交付。源码基线：`3976e0aa0230f99f73b93cef905ae629bbd535e9`。

初始审阅基线为 **161 项**，当时源码、发布工程与本地部署的 ID 和 HLSL 哈希一致。r1 将 RTX Video 合并为两个参数化效果器后，当前产品目录为 155 项；本报告保留原始逐项审计，DLSS SR 分类与说明按最新用户要求更新。

这是一份供用户复核的内容草案。用途分类、推荐程度与搭配建议可以继续调整；尺寸表达式、当前后端限制和协议路径以本次源码为依据。完整字段与每项源码行号见 [结构化目录](20260908-effect-content-catalog.json)。可搜索效果器名称定位条目。

**DLSSNR 暂归「风格显示 → AI 画面重塑」**。按用户纠正，分类、标签、用途说明及搭配建议均去掉降噪功能描述；最终分类名称待用户复核。

## 本轮用户修订

- **选择器交互已确认**：单击效果器立即添加，默认追加当前效果组末尾；悬停或键盘聚焦查看说明，管线位置作为手动调整建议。取消独立添加确认按钮和添加位置选择控件，已知组合冲突在添加前检查并提示下一步。
- **DLSS SR → 放大与超分／时序重建**：与 FSR、XeSS SR 同组，作为主要缩放步骤。保留原始相机抖动和真实深度缺失带来的重建限制，以及当前 Preset J、用户反馈的 L／M 轻微抗锯齿收益说明。
- **DLSSNR 使用条件**：按用户提供的本项目兼容口径，驱动 615+，RTX 50 系原生支持；RTX 40／30／20 系可能需要手动替换兼容 DLL。本轮未做跨显卡验证。
- **RTX Video 拟收为两个入口**：`RTXVideo_Denoise`、`RTXVideo_VSR`，强度为低／中／高／极高四档。下面仍保留八个实际源条目的逐项审计，未来选择器按两组显示，逻辑条目数为 155。
- **XeSSFG 显示名称**：`XeSS_FrameGeneration_x2`、`XeSS_MultiFrameGeneration`。界面名称去掉 ZeroMV，旧配置 ID 继续兼容，可选运动估算保留原有含义。

详细的旧 ID／档位映射、当前显示别名与后续迁移方案见 [命名与配置兼容设计](20260908-effect-picker-naming-compatibility.md)。选择器已按本轮决策实现，操作说明见 [效果器选择器](../testing/EFFECT-PICKER.md)。具体画质仍待实测。

## 分类总览

| 用途分类   | 数量 | 子分组                                           |
| ---------- | ----:| ------------------------------------------------ |
| 放大与超分 | 111  | 通用、动画线条、像素画、时序重建、缩小与尺寸整理 |
| 清理修复   | 19   | 降噪、去色带、线条修复                           |
| 抗锯齿     | 12   | 空间、时序                                       |
| 锐化细节   | 6    | 通用锐化                                         |
| 补帧与帧率 | 4    | 补帧、帧率调度                                   |
| 风格显示   | 7    | CRT、图像调整、AI 画面重塑                       |
| 调试自定义 | 2    | 诊断                                             |

## 阅读规则

- **优先尝试**：可作为指定场景的起点；仍需结合条目的 HDR 或硬件条件。**特定场景推荐**：针对相应素材与问题选择。**进阶／实验**：适合有明确比较需求时尝试。**仅诊断使用**：用于查看处理信息。等级不代表实测画质排名。
- **管线位置为搭配建议**，并非已经实现的自动排序或禁用规则。要求栏同时列出算法使用条件与现有后端限制；性能、光晕等提醒属于画质取舍。补帧在呈现端执行，限帧由 Renderer 扫描全链处理，列表位置不改变这两类功能的调度范围。
- **输出尺寸**分为配置尺寸 36 项、保持输入尺寸 45 项、固定表达式 80 项。通用条件是效果着色器可以编译、输入输出尺寸合法；这些共性要求不在每个条目中重复。
- **本轮未做 GPU、HDR 画面或性能实测。** JSON 中的 HDR 状态描述当前代码路径，与运行验证状态分开记录。报告仅在 `showWarning=true` 时逐项显示 HDR 注意事项；其余条目未显示提示也不意味着已验证所有素材。
- 算法家族共用基础说明，各条目列出倍率、模型、档位或处理方式差异。定性成本取自处理结构和原生依赖，不据此推断具体帧率。

## ACNet（1）

### ACNet

当前配置 ID：`ACNet`。

卷积神经网络 2× 放大，主要面向动漫素材并带较强降噪取向。 适合带压缩噪声的动画与线稿；相对干净或真人素材应同时比较保纹理更强的模型。

- **用途**：放大与超分 → 动画线条。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：固定 2×；9 个计算通道，强降噪可能损失细纹理，未做本机画面对比。
- **推荐**：进阶／实验。多通道或较大网络模型，适合在有性能余量时比较。 适用条件／尝试建议：按目标分辨率检查性能；不要仅按名称推断画质。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **成本**：中到高。源码声明 9 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[ACNet.hlsl:4](<../../../src/Effects/ACNet.hlsl#L4>)、[ACNet.hlsl:18](<../../../src/Effects/ACNet.hlsl#L18>)、[HdrEffectBoundary.cpp:8](<../../../src/Magpie.Core/HdrEffectBoundary.cpp#L8>)。

## Anime4K（27）

### Anime4K 3D AA Upscale US

当前配置 ID：`Anime4K\Anime4K_3D_AA_Upscale_US`。

面向 3D 游戏捕获画面的固定 2× 空间放大。 AA 组合版在放大时加入边缘平滑，适合锯齿较明显的 3D 游戏。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：US 预设，固定 2×、3 通道，包含 AA 组合。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中。源码声明 3 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[Anime4K_3D_AA_Upscale_US.hlsl:4](<../../../src/Effects/Anime4K/Anime4K_3D_AA_Upscale_US.hlsl#L4>)、[Anime4K_3D_AA_Upscale_US.hlsl:19](<../../../src/Effects/Anime4K/Anime4K_3D_AA_Upscale_US.hlsl#L19>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### Anime4K 3D Upscale US

当前配置 ID：`Anime4K\Anime4K_3D_Upscale_US`。

面向 3D 游戏捕获画面的固定 2× 空间放大。 基础版用于 3D 游戏画面放大，避免将 Anime4K 品牌名误解为只适合动画。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：US 预设，固定 2×、3 通道。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中。源码声明 3 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[Anime4K_3D_Upscale_US.hlsl:4](<../../../src/Effects/Anime4K/Anime4K_3D_Upscale_US.hlsl#L4>)、[Anime4K_3D_Upscale_US.hlsl:19](<../../../src/Effects/Anime4K/Anime4K_3D_Upscale_US.hlsl#L19>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### Anime4K Denoise Bilateral Mean

当前配置 ID：`Anime4K\Anime4K_Denoise_Bilateral_Mean`。

用 Anime4K 双边滤波减轻动画或压缩素材噪声，保留主要边缘。 适合在线条放大前清理轻度噪点；均值统计会产生不同的纹理取舍。

- **用途**：清理修复 → 降噪。**输出尺寸**：保持输入尺寸。
- **变体差异**：均值变体；同尺寸、1 通道，需与另外两种统计方式按素材比较。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：放大前。 建议放在「放大/超分、锐化」之前。 先清理输入可减少后续放大噪声。
- **条件与取舍**：强度过高会损失纹理。
- **成本**：低到中。源码声明 1 个 pass；1 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[Anime4K_Denoise_Bilateral_Mean.hlsl:5](<../../../src/Effects/Anime4K/Anime4K_Denoise_Bilateral_Mean.hlsl#L5>)、[Anime4K_Denoise_Bilateral_Mean.hlsl:23](<../../../src/Effects/Anime4K/Anime4K_Denoise_Bilateral_Mean.hlsl#L23>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### Anime4K Denoise Bilateral Median

当前配置 ID：`Anime4K\Anime4K_Denoise_Bilateral_Median`。

用 Anime4K 双边滤波减轻动画或压缩素材噪声，保留主要边缘。 适合在线条放大前清理轻度噪点；中值统计会产生不同的纹理取舍。

- **用途**：清理修复 → 降噪。**输出尺寸**：保持输入尺寸。
- **变体差异**：中值变体；同尺寸、1 通道，需与另外两种统计方式按素材比较。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：放大前。 建议放在「放大/超分、锐化」之前。 先清理输入可减少后续放大噪声。
- **条件与取舍**：强度过高会损失纹理。
- **成本**：低到中。源码声明 1 个 pass；1 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[Anime4K_Denoise_Bilateral_Median.hlsl:4](<../../../src/Effects/Anime4K/Anime4K_Denoise_Bilateral_Median.hlsl#L4>)、[Anime4K_Denoise_Bilateral_Median.hlsl:22](<../../../src/Effects/Anime4K/Anime4K_Denoise_Bilateral_Median.hlsl#L22>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### Anime4K Denoise Bilateral Mode

当前配置 ID：`Anime4K\Anime4K_Denoise_Bilateral_Mode`。

用 Anime4K 双边滤波减轻动画或压缩素材噪声，保留主要边缘。 适合在线条放大前清理轻度噪点；众数统计会产生不同的纹理取舍。

- **用途**：清理修复 → 降噪。**输出尺寸**：保持输入尺寸。
- **变体差异**：众数变体；同尺寸、1 通道，需与另外两种统计方式按素材比较。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：放大前。 建议放在「放大/超分、锐化」之前。 先清理输入可减少后续放大噪声。
- **条件与取舍**：强度过高会损失纹理。
- **成本**：低到中。源码声明 1 个 pass；1 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[Anime4K_Denoise_Bilateral_Mode.hlsl:5](<../../../src/Effects/Anime4K/Anime4K_Denoise_Bilateral_Mode.hlsl#L5>)、[Anime4K_Denoise_Bilateral_Mode.hlsl:23](<../../../src/Effects/Anime4K/Anime4K_Denoise_Bilateral_Mode.hlsl#L23>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### Anime4K Restore L

当前配置 ID：`Anime4K\Anime4K_Restore_L`。

修复动画线条与局部细节，适合放大前的线稿整理。 标准 Restore 版强调线条恢复，适合轮廓受压缩影响的动画。

- **用途**：清理修复 → 线条修复。**输出尺寸**：保持输入尺寸。
- **变体差异**：L 预设；同尺寸、5 通道。字母表示预设规模，不代表已实测的画质排名。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：放大前。 建议放在「放大/超分、锐化」之前。 先清理输入可减少后续放大噪声。
- **条件与取舍**：强度过高会损失纹理。
- **成本**：中到高。源码声明 5 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[Anime4K_Restore_L.hlsl:3](<../../../src/Effects/Anime4K/Anime4K_Restore_L.hlsl#L3>)、[Anime4K_Restore_L.hlsl:18](<../../../src/Effects/Anime4K/Anime4K_Restore_L.hlsl#L18>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### Anime4K Restore M

当前配置 ID：`Anime4K\Anime4K_Restore_M`。

修复动画线条与局部细节，适合放大前的线稿整理。 标准 Restore 版强调线条恢复，适合轮廓受压缩影响的动画。

- **用途**：清理修复 → 线条修复。**输出尺寸**：保持输入尺寸。
- **变体差异**：M 预设；同尺寸、7 通道。字母表示预设规模，不代表已实测的画质排名。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：放大前。 建议放在「放大/超分、锐化」之前。 先清理输入可减少后续放大噪声。
- **条件与取舍**：强度过高会损失纹理。
- **成本**：中到高。源码声明 7 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[Anime4K_Restore_M.hlsl:4](<../../../src/Effects/Anime4K/Anime4K_Restore_M.hlsl#L4>)、[Anime4K_Restore_M.hlsl:19](<../../../src/Effects/Anime4K/Anime4K_Restore_M.hlsl#L19>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### Anime4K Restore S

当前配置 ID：`Anime4K\Anime4K_Restore_S`。

修复动画线条与局部细节，适合放大前的线稿整理。 标准 Restore 版强调线条恢复，适合轮廓受压缩影响的动画。

- **用途**：清理修复 → 线条修复。**输出尺寸**：保持输入尺寸。
- **变体差异**：S 预设；同尺寸、4 通道。字母表示预设规模，不代表已实测的画质排名。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：放大前。 建议放在「放大/超分、锐化」之前。 先清理输入可减少后续放大噪声。
- **条件与取舍**：强度过高会损失纹理。
- **成本**：中。源码声明 4 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[Anime4K_Restore_S.hlsl:4](<../../../src/Effects/Anime4K/Anime4K_Restore_S.hlsl#L4>)、[Anime4K_Restore_S.hlsl:19](<../../../src/Effects/Anime4K/Anime4K_Restore_S.hlsl#L19>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### Anime4K Restore Soft L

当前配置 ID：`Anime4K\Anime4K_Restore_Soft_L`。

修复动画线条与局部细节，适合放大前的线稿整理。 Soft 版抑制更激进的边缘改变，适合担心过锐或伪影时尝试。

- **用途**：清理修复 → 线条修复。**输出尺寸**：保持输入尺寸。
- **变体差异**：Soft L 预设；同尺寸、5 通道。字母表示预设规模，不代表已实测的画质排名。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：放大前。 建议放在「放大/超分、锐化」之前。 先清理输入可减少后续放大噪声。
- **条件与取舍**：强度过高会损失纹理。
- **成本**：中到高。源码声明 5 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[Anime4K_Restore_Soft_L.hlsl:3](<../../../src/Effects/Anime4K/Anime4K_Restore_Soft_L.hlsl#L3>)、[Anime4K_Restore_Soft_L.hlsl:18](<../../../src/Effects/Anime4K/Anime4K_Restore_Soft_L.hlsl#L18>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### Anime4K Restore Soft M

当前配置 ID：`Anime4K\Anime4K_Restore_Soft_M`。

修复动画线条与局部细节，适合放大前的线稿整理。 Soft 版抑制更激进的边缘改变，适合担心过锐或伪影时尝试。

- **用途**：清理修复 → 线条修复。**输出尺寸**：保持输入尺寸。
- **变体差异**：Soft M 预设；同尺寸、7 通道。字母表示预设规模，不代表已实测的画质排名。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：放大前。 建议放在「放大/超分、锐化」之前。 先清理输入可减少后续放大噪声。
- **条件与取舍**：强度过高会损失纹理。
- **成本**：中到高。源码声明 7 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[Anime4K_Restore_Soft_M.hlsl:4](<../../../src/Effects/Anime4K/Anime4K_Restore_Soft_M.hlsl#L4>)、[Anime4K_Restore_Soft_M.hlsl:19](<../../../src/Effects/Anime4K/Anime4K_Restore_Soft_M.hlsl#L19>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### Anime4K Restore Soft S

当前配置 ID：`Anime4K\Anime4K_Restore_Soft_S`。

修复动画线条与局部细节，适合放大前的线稿整理。 Soft 版抑制更激进的边缘改变，适合担心过锐或伪影时尝试。

- **用途**：清理修复 → 线条修复。**输出尺寸**：保持输入尺寸。
- **变体差异**：Soft S 预设；同尺寸、4 通道。字母表示预设规模，不代表已实测的画质排名。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：放大前。 建议放在「放大/超分、锐化」之前。 先清理输入可减少后续放大噪声。
- **条件与取舍**：强度过高会损失纹理。
- **成本**：中。源码声明 4 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[Anime4K_Restore_Soft_S.hlsl:4](<../../../src/Effects/Anime4K/Anime4K_Restore_Soft_S.hlsl#L4>)、[Anime4K_Restore_Soft_S.hlsl:19](<../../../src/Effects/Anime4K/Anime4K_Restore_Soft_S.hlsl#L19>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### Anime4K Restore Soft UL

当前配置 ID：`Anime4K\Anime4K_Restore_Soft_UL`。

修复动画线条与局部细节，适合放大前的线稿整理。 Soft 版抑制更激进的边缘改变，适合担心过锐或伪影时尝试。

- **用途**：清理修复 → 线条修复。**输出尺寸**：保持输入尺寸。
- **变体差异**：Soft UL 预设；同尺寸、8 通道。字母表示预设规模，不代表已实测的画质排名。
- **推荐**：进阶／实验。多通道或较大网络模型，适合在有性能余量时比较。 适用条件／尝试建议：按目标分辨率检查性能；不要仅按名称推断画质。
- **管线位置**：放大前。 建议放在「放大/超分、锐化」之前。 先清理输入可减少后续放大噪声。
- **条件与取舍**：强度过高会损失纹理。
- **成本**：中到高。源码声明 8 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[Anime4K_Restore_Soft_UL.hlsl:4](<../../../src/Effects/Anime4K/Anime4K_Restore_Soft_UL.hlsl#L4>)、[Anime4K_Restore_Soft_UL.hlsl:19](<../../../src/Effects/Anime4K/Anime4K_Restore_Soft_UL.hlsl#L19>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### Anime4K Restore Soft VL

当前配置 ID：`Anime4K\Anime4K_Restore_Soft_VL`。

修复动画线条与局部细节，适合放大前的线稿整理。 Soft 版抑制更激进的边缘改变，适合担心过锐或伪影时尝试。

- **用途**：清理修复 → 线条修复。**输出尺寸**：保持输入尺寸。
- **变体差异**：Soft VL 预设；同尺寸、8 通道。字母表示预设规模，不代表已实测的画质排名。
- **推荐**：进阶／实验。多通道或较大网络模型，适合在有性能余量时比较。 适用条件／尝试建议：按目标分辨率检查性能；不要仅按名称推断画质。
- **管线位置**：放大前。 建议放在「放大/超分、锐化」之前。 先清理输入可减少后续放大噪声。
- **条件与取舍**：强度过高会损失纹理。
- **成本**：中到高。源码声明 8 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[Anime4K_Restore_Soft_VL.hlsl:4](<../../../src/Effects/Anime4K/Anime4K_Restore_Soft_VL.hlsl#L4>)、[Anime4K_Restore_Soft_VL.hlsl:19](<../../../src/Effects/Anime4K/Anime4K_Restore_Soft_VL.hlsl#L19>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### Anime4K Restore UL

当前配置 ID：`Anime4K\Anime4K_Restore_UL`。

修复动画线条与局部细节，适合放大前的线稿整理。 标准 Restore 版强调线条恢复，适合轮廓受压缩影响的动画。

- **用途**：清理修复 → 线条修复。**输出尺寸**：保持输入尺寸。
- **变体差异**：UL 预设；同尺寸、8 通道。字母表示预设规模，不代表已实测的画质排名。
- **推荐**：进阶／实验。多通道或较大网络模型，适合在有性能余量时比较。 适用条件／尝试建议：按目标分辨率检查性能；不要仅按名称推断画质。
- **管线位置**：放大前。 建议放在「放大/超分、锐化」之前。 先清理输入可减少后续放大噪声。
- **条件与取舍**：强度过高会损失纹理。
- **成本**：中到高。源码声明 8 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[Anime4K_Restore_UL.hlsl:4](<../../../src/Effects/Anime4K/Anime4K_Restore_UL.hlsl#L4>)、[Anime4K_Restore_UL.hlsl:19](<../../../src/Effects/Anime4K/Anime4K_Restore_UL.hlsl#L19>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### Anime4K Restore VL

当前配置 ID：`Anime4K\Anime4K_Restore_VL`。

修复动画线条与局部细节，适合放大前的线稿整理。 标准 Restore 版强调线条恢复，适合轮廓受压缩影响的动画。

- **用途**：清理修复 → 线条修复。**输出尺寸**：保持输入尺寸。
- **变体差异**：VL 预设；同尺寸、8 通道。字母表示预设规模，不代表已实测的画质排名。
- **推荐**：进阶／实验。多通道或较大网络模型，适合在有性能余量时比较。 适用条件／尝试建议：按目标分辨率检查性能；不要仅按名称推断画质。
- **管线位置**：放大前。 建议放在「放大/超分、锐化」之前。 先清理输入可减少后续放大噪声。
- **条件与取舍**：强度过高会损失纹理。
- **成本**：中到高。源码声明 8 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[Anime4K_Restore_VL.hlsl:4](<../../../src/Effects/Anime4K/Anime4K_Restore_VL.hlsl#L4>)、[Anime4K_Restore_VL.hlsl:19](<../../../src/Effects/Anime4K/Anime4K_Restore_VL.hlsl#L19>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### Anime4K Thin HQ

当前配置 ID：`Anime4K\Anime4K_Thin_HQ`。

细化过粗的动画线条，减少后续放大时的线条发胖。 通常作为动画放大前的线条预处理，少量使用并观察细线是否断裂。

- **用途**：清理修复 → 线条修复。**输出尺寸**：保持输入尺寸。
- **变体差异**：HQ 线条细化；同尺寸、5 通道。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：动画放大或线条恢复之后。 建议放在「动画放大／线条恢复」之后。 建议放在「最终锐化」之前。 用于修整放大或恢复后显得过粗的轮廓；这是按画面问题给出的搭配建议。
- **条件与取舍**：细化过强可能使细线断裂，应从较低强度比较。
- **成本**：中到高。源码声明 5 个 pass；2 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[Anime4K_Thin_HQ.hlsl:4](<../../../src/Effects/Anime4K/Anime4K_Thin_HQ.hlsl#L4>)、[Anime4K_Thin_HQ.hlsl:34](<../../../src/Effects/Anime4K/Anime4K_Thin_HQ.hlsl#L34>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### Anime4K Upscale Denoise L

当前配置 ID：`Anime4K\Anime4K_Upscale_Denoise_L`。

面向动画和线稿的 2× 放大并同时降噪。 把清理与放大合在一项中，适合噪声与低分辨率同时存在的素材。

- **用途**：放大与超分 → 动画线条。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：L 预设，固定 2×、4 通道。未做本机变体画质排名。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中。源码声明 4 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[Anime4K_Upscale_Denoise_L.hlsl:4](<../../../src/Effects/Anime4K/Anime4K_Upscale_Denoise_L.hlsl#L4>)、[Anime4K_Upscale_Denoise_L.hlsl:19](<../../../src/Effects/Anime4K/Anime4K_Upscale_Denoise_L.hlsl#L19>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### Anime4K Upscale Denoise S

当前配置 ID：`Anime4K\Anime4K_Upscale_Denoise_S`。

面向动画和线稿的 2× 放大并同时降噪。 把清理与放大合在一项中，适合噪声与低分辨率同时存在的素材。

- **用途**：放大与超分 → 动画线条。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：S 预设，固定 2×、4 通道。未做本机变体画质排名。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中。源码声明 4 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[Anime4K_Upscale_Denoise_S.hlsl:4](<../../../src/Effects/Anime4K/Anime4K_Upscale_Denoise_S.hlsl#L4>)、[Anime4K_Upscale_Denoise_S.hlsl:19](<../../../src/Effects/Anime4K/Anime4K_Upscale_Denoise_S.hlsl#L19>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### Anime4K Upscale Denoise UL

当前配置 ID：`Anime4K\Anime4K_Upscale_Denoise_UL`。

面向动画和线稿的 2× 放大并同时降噪。 把清理与放大合在一项中，适合噪声与低分辨率同时存在的素材。

- **用途**：放大与超分 → 动画线条。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：UL 预设，固定 2×、8 通道。未做本机变体画质排名。
- **推荐**：进阶／实验。多通道或较大网络模型，适合在有性能余量时比较。 适用条件／尝试建议：按目标分辨率检查性能；不要仅按名称推断画质。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中到高。源码声明 8 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[Anime4K_Upscale_Denoise_UL.hlsl:4](<../../../src/Effects/Anime4K/Anime4K_Upscale_Denoise_UL.hlsl#L4>)、[Anime4K_Upscale_Denoise_UL.hlsl:19](<../../../src/Effects/Anime4K/Anime4K_Upscale_Denoise_UL.hlsl#L19>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### Anime4K Upscale Denoise VL

当前配置 ID：`Anime4K\Anime4K_Upscale_Denoise_VL`。

面向动画和线稿的 2× 放大并同时降噪。 把清理与放大合在一项中，适合噪声与低分辨率同时存在的素材。

- **用途**：放大与超分 → 动画线条。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：VL 预设，固定 2×、8 通道。未做本机变体画质排名。
- **推荐**：进阶／实验。多通道或较大网络模型，适合在有性能余量时比较。 适用条件／尝试建议：按目标分辨率检查性能；不要仅按名称推断画质。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中到高。源码声明 8 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[Anime4K_Upscale_Denoise_VL.hlsl:4](<../../../src/Effects/Anime4K/Anime4K_Upscale_Denoise_VL.hlsl#L4>)、[Anime4K_Upscale_Denoise_VL.hlsl:19](<../../../src/Effects/Anime4K/Anime4K_Upscale_Denoise_VL.hlsl#L19>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### Anime4K Upscale GAN x2 M

当前配置 ID：`Anime4K\Anime4K_Upscale_GAN_x2_M`。

面向动画和线稿的 2× 放大。 GAN 变体更偏向学习式细节重建，适合可接受模型化纹理的动画素材。

- **用途**：放大与超分 → 动画线条。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：M 预设，固定 2×、7 通道。未做本机变体画质排名。
- **推荐**：特定场景推荐。适合特定素材、风格或实验场景。 适用条件／尝试建议：先在代表性画面比较副作用。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中到高。源码声明 7 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[Anime4K_Upscale_GAN_x2_M.hlsl:4](<../../../src/Effects/Anime4K/Anime4K_Upscale_GAN_x2_M.hlsl#L4>)、[Anime4K_Upscale_GAN_x2_M.hlsl:27](<../../../src/Effects/Anime4K/Anime4K_Upscale_GAN_x2_M.hlsl#L27>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### Anime4K Upscale GAN x2 S

当前配置 ID：`Anime4K\Anime4K_Upscale_GAN_x2_S`。

面向动画和线稿的 2× 放大。 GAN 变体更偏向学习式细节重建，适合可接受模型化纹理的动画素材。

- **用途**：放大与超分 → 动画线条。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：S 预设，固定 2×、7 通道。未做本机变体画质排名。
- **推荐**：特定场景推荐。适合特定素材、风格或实验场景。 适用条件／尝试建议：先在代表性画面比较副作用。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中到高。源码声明 7 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[Anime4K_Upscale_GAN_x2_S.hlsl:4](<../../../src/Effects/Anime4K/Anime4K_Upscale_GAN_x2_S.hlsl#L4>)、[Anime4K_Upscale_GAN_x2_S.hlsl:19](<../../../src/Effects/Anime4K/Anime4K_Upscale_GAN_x2_S.hlsl#L19>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### Anime4K Upscale GAN x3 L

当前配置 ID：`Anime4K\Anime4K_Upscale_GAN_x3_L`。

面向动画和线稿的 3× 放大。 GAN 变体更偏向学习式细节重建，适合可接受模型化纹理的动画素材。

- **用途**：放大与超分 → 动画线条。**输出尺寸**：固定倍率：宽 ×3，高 ×3。
- **变体差异**：L 预设，固定 3×、8 通道。未做本机变体画质排名。
- **推荐**：特定场景推荐。适合特定素材、风格或实验场景。 适用条件／尝试建议：先在代表性画面比较副作用。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中到高。源码声明 8 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[Anime4K_Upscale_GAN_x3_L.hlsl:4](<../../../src/Effects/Anime4K/Anime4K_Upscale_GAN_x3_L.hlsl#L4>)、[Anime4K_Upscale_GAN_x3_L.hlsl:27](<../../../src/Effects/Anime4K/Anime4K_Upscale_GAN_x3_L.hlsl#L27>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### Anime4K Upscale L

当前配置 ID：`Anime4K\Anime4K_Upscale_L`。

面向动画和线稿的 2× 放大。 强调动画轮廓与平坦色块，真人内容建议与通用放大器比较。

- **用途**：放大与超分 → 动画线条。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：L 预设，固定 2×、4 通道。未做本机变体画质排名。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中。源码声明 4 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[Anime4K_Upscale_L.hlsl:4](<../../../src/Effects/Anime4K/Anime4K_Upscale_L.hlsl#L4>)、[Anime4K_Upscale_L.hlsl:18](<../../../src/Effects/Anime4K/Anime4K_Upscale_L.hlsl#L18>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### Anime4K Upscale S

当前配置 ID：`Anime4K\Anime4K_Upscale_S`。

面向动画和线稿的 2× 放大。 强调动画轮廓与平坦色块，真人内容建议与通用放大器比较。

- **用途**：放大与超分 → 动画线条。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：S 预设，固定 2×、4 通道。未做本机变体画质排名。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中。源码声明 4 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[Anime4K_Upscale_S.hlsl:4](<../../../src/Effects/Anime4K/Anime4K_Upscale_S.hlsl#L4>)、[Anime4K_Upscale_S.hlsl:19](<../../../src/Effects/Anime4K/Anime4K_Upscale_S.hlsl#L19>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### Anime4K Upscale UL

当前配置 ID：`Anime4K\Anime4K_Upscale_UL`。

面向动画和线稿的 2× 放大。 强调动画轮廓与平坦色块，真人内容建议与通用放大器比较。

- **用途**：放大与超分 → 动画线条。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：UL 预设，固定 2×、8 通道。未做本机变体画质排名。
- **推荐**：进阶／实验。多通道或较大网络模型，适合在有性能余量时比较。 适用条件／尝试建议：按目标分辨率检查性能；不要仅按名称推断画质。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中到高。源码声明 8 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[Anime4K_Upscale_UL.hlsl:4](<../../../src/Effects/Anime4K/Anime4K_Upscale_UL.hlsl#L4>)、[Anime4K_Upscale_UL.hlsl:19](<../../../src/Effects/Anime4K/Anime4K_Upscale_UL.hlsl#L19>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### Anime4K Upscale VL

当前配置 ID：`Anime4K\Anime4K_Upscale_VL`。

面向动画和线稿的 2× 放大。 强调动画轮廓与平坦色块，真人内容建议与通用放大器比较。

- **用途**：放大与超分 → 动画线条。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：VL 预设，固定 2×、8 通道。未做本机变体画质排名。
- **推荐**：进阶／实验。多通道或较大网络模型，适合在有性能余量时比较。 适用条件／尝试建议：按目标分辨率检查性能；不要仅按名称推断画质。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中到高。源码声明 8 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[Anime4K_Upscale_VL.hlsl:4](<../../../src/Effects/Anime4K/Anime4K_Upscale_VL.hlsl#L4>)、[Anime4K_Upscale_VL.hlsl:19](<../../../src/Effects/Anime4K/Anime4K_Upscale_VL.hlsl#L19>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

## Bicubic（1）

### Bicubic

当前配置 ID：`Bicubic`。

经典三次插值，可作为速度与平滑度之间的基础选择。 计算和观感容易预测，适合先建立缩放基线；锐度可调。

- **用途**：放大与超分 → 通用。**输出尺寸**：使用效果组配置的输出尺寸。
- **变体差异**：输出尺寸由效果组决定；可调锐度，当前 HDR 路径有专门 DirectFP16 分支。
- **推荐**：优先尝试。通用且链路职责清楚，便于建立画质/性能基线。 适用条件／尝试建议：按内容类型比较锐度与振铃。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **成本**：低到中。源码声明 1 个 pass；2 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[Bicubic.hlsl:4](<../../../src/Effects/Bicubic.hlsl#L4>)、[Bicubic.hlsl:31](<../../../src/Effects/Bicubic.hlsl#L31>)、[HdrEffectBoundary.cpp:8](<../../../src/Magpie.Core/HdrEffectBoundary.cpp#L8>)。

## Bilinear（1）

### Bilinear

当前配置 ID：`Bilinear`。

快速线性插值，画面偏柔和，适合性能优先或作为链路基线。 开销较低但不会重建新细节，可用于性能排查与柔和放大。

- **用途**：放大与超分 → 通用。**输出尺寸**：使用效果组配置的输出尺寸。
- **变体差异**：输出尺寸由效果组决定；单通道处理。
- **推荐**：优先尝试。通用且链路职责清楚，便于建立画质/性能基线。 适用条件／尝试建议：按内容类型比较锐度与振铃。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **成本**：低到中。源码声明 1 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[Bilinear.hlsl:1](<../../../src/Effects/Bilinear.hlsl#L1>)、[Bilinear.hlsl:8](<../../../src/Effects/Bilinear.hlsl#L8>)、[HdrEffectBoundary.cpp:8](<../../../src/Magpie.Core/HdrEffectBoundary.cpp#L8>)。

## CAS（2）

### CAS Scaling

当前配置 ID：`CAS\CAS_Scaling`。

在一个通道内完成 CAS 缩放与锐化。 适合需要任意输出尺寸并希望同时锐化时。

- **用途**：放大与超分 → 通用。**输出尺寸**：使用效果组配置的输出尺寸。
- **变体差异**：OUTPUT 未声明固定宽高，因此由效果组目标尺寸决定；不能按旧文档视为同尺寸。
- **推荐**：优先尝试。通用且链路职责清楚，便于建立画质/性能基线。 适用条件／尝试建议：按内容类型比较锐度与振铃。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **成本**：低到中。源码声明 1 个 pass；1 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[CAS_Scaling.hlsl:3](<../../../src/Effects/CAS/CAS_Scaling.hlsl#L3>)、[CAS_Scaling.hlsl:21](<../../../src/Effects/CAS/CAS_Scaling.hlsl#L21>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### CAS

当前配置 ID：`CAS\CAS`。

使用对比度自适应锐化增强同尺寸细节。 适合放在放大后，小幅补偿模糊。

- **用途**：锐化细节 → 通用锐化。**输出尺寸**：保持输入尺寸。
- **变体差异**：明确同尺寸、单通道。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：链尾收尾。 建议放在「放大/超分、降噪」之后。 建议放在「CRT/显示调整」之前。 按最终尺寸补偿柔和感。
- **条件与取舍**：过强会放大噪声并产生光晕。
- **成本**：低到中。源码声明 1 个 pass；1 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[CAS.hlsl:3](<../../../src/Effects/CAS/CAS.hlsl#L3>)、[CAS.hlsl:24](<../../../src/Effects/CAS/CAS.hlsl#L24>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

## CRT（5）

### CRT Easymode

当前配置 ID：`CRT\CRT_Easymode`。

轻量入门 CRT 风格，加入扫描线、遮罩与适度辉光。 参数较少，适合先确认复古显示方向。 建议接近链尾使用。

- **用途**：风格显示 → CRT。**输出尺寸**：使用效果组配置的输出尺寸。
- **变体差异**：Easymode 模型，1 通道。
- **推荐**：特定场景推荐。适合特定素材、风格或实验场景。 适用条件／尝试建议：先在代表性画面比较副作用。
- **管线位置**：链尾显示。 建议放在「主要画质处理」之后。 风格与显示调整通常针对最终输出观感。
- **成本**：低到中。源码声明 1 个 pass；17 个可调参数。未进行 GPU 基准测试。
- **HDR 注意**：CRT 数学按 SDR 显示观感设计，HDR 高光和颜色可能明显改变。 GroupAHdrRoutes 明确以 R8/sRGB SDR 适配运行，FP16 中间纹理不构成原生 HDR 证明。
- **源码依据**：[CRT_Easymode.hlsl:33](<../../../src/Effects/CRT/CRT_Easymode.hlsl#L33>)、[CRT_Easymode.hlsl:177](<../../../src/Effects/CRT/CRT_Easymode.hlsl#L177>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### CRT Geom

当前配置 ID：`CRT\CRT_Geom`。

带 CRT 屏幕曲面、边角与扫描线的几何模拟。 几何弯曲会改变画面边缘，适合愿意保留黑边/曲面的复古显示。 建议接近链尾使用。

- **用途**：风格显示 → CRT。**输出尺寸**：使用效果组配置的输出尺寸。
- **变体差异**：Geom 模型，1 通道。
- **推荐**：特定场景推荐。适合特定素材、风格或实验场景。 适用条件／尝试建议：先在代表性画面比较副作用。
- **管线位置**：链尾显示。 建议放在「主要画质处理」之后。 风格与显示调整通常针对最终输出观感。
- **成本**：低到中。源码声明 1 个 pass；15 个可调参数。未进行 GPU 基准测试。
- **HDR 注意**：CRT 数学按 SDR 显示观感设计，HDR 高光和颜色可能明显改变。 GroupAHdrRoutes 明确以 R8/sRGB SDR 适配运行，FP16 中间纹理不构成原生 HDR 证明。
- **源码依据**：[CRT_Geom.hlsl:26](<../../../src/Effects/CRT/CRT_Geom.hlsl#L26>)、[CRT_Geom.hlsl:154](<../../../src/Effects/CRT/CRT_Geom.hlsl#L154>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### CRT Hyllian

当前配置 ID：`CRT\CRT_Hyllian`。

偏清晰的扫描线与磷光遮罩 CRT 模拟。 源码明确要求整数倍缩放，适合像素游戏的整数倍输出。 建议接近链尾使用。

- **用途**：风格显示 → CRT。**输出尺寸**：使用效果组配置的输出尺寸。
- **变体差异**：Hyllian 模型，1 通道。
- **推荐**：特定场景推荐。适合特定素材、风格或实验场景。 适用条件／尝试建议：先在代表性画面比较副作用。
- **管线位置**：链尾显示。 建议放在「主要画质处理」之后。 风格与显示调整通常针对最终输出观感。
- **条件与取舍**：输出必须采用整数倍缩放。
- **成本**：低到中。源码声明 1 个 pass；13 个可调参数。未进行 GPU 基准测试。
- **HDR 注意**：CRT 数学按 SDR 显示观感设计，HDR 高光和颜色可能明显改变。 GroupAHdrRoutes 明确以 R8/sRGB SDR 适配运行，FP16 中间纹理不构成原生 HDR 证明。
- **源码依据**：[CRT_Hyllian.hlsl:30](<../../../src/Effects/CRT/CRT_Hyllian.hlsl#L30>)、[CRT_Hyllian.hlsl:142](<../../../src/Effects/CRT/CRT_Hyllian.hlsl#L142>)、[CRT_Hyllian.hlsl:3](<../../../src/Effects/CRT/CRT_Hyllian.hlsl#L3>)。

### CRT Lottes

当前配置 ID：`CRT\CRT_Lottes`。

较强扫描线、阴影遮罩与亮度塑形的 CRT 模拟。 可调遮罩与硬度，适合希望外观更强烈时。 建议接近链尾使用。

- **用途**：风格显示 → CRT。**输出尺寸**：使用效果组配置的输出尺寸。
- **变体差异**：Lottes 模型，1 通道。
- **推荐**：特定场景推荐。适合特定素材、风格或实验场景。 适用条件／尝试建议：先在代表性画面比较副作用。
- **管线位置**：链尾显示。 建议放在「主要画质处理」之后。 风格与显示调整通常针对最终输出观感。
- **成本**：低到中。源码声明 1 个 pass；12 个可调参数。未进行 GPU 基准测试。
- **HDR 注意**：CRT 数学按 SDR 显示观感设计，HDR 高光和颜色可能明显改变。 GroupAHdrRoutes 明确以 R8/sRGB SDR 适配运行，FP16 中间纹理不构成原生 HDR 证明。
- **源码依据**：[CRT_Lottes.hlsl:19](<../../../src/Effects/CRT/CRT_Lottes.hlsl#L19>)、[CRT_Lottes.hlsl:123](<../../../src/Effects/CRT/CRT_Lottes.hlsl#L123>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### GTU v050

当前配置 ID：`CRT\GTU_v050`。

模拟带宽受限的模拟视频/CRT 模糊、混色与扫描线。 重点是水平信号模糊和色彩串扰，不以曲面或明显遮罩为核心。 建议接近链尾使用。

- **用途**：风格显示 → CRT。**输出尺寸**：使用效果组配置的输出尺寸。
- **变体差异**：GTU_v050 模型，2 通道（横纵分离）。
- **推荐**：特定场景推荐。适合特定素材、风格或实验场景。 适用条件／尝试建议：先在代表性画面比较副作用。
- **管线位置**：链尾显示。 建议放在「主要画质处理」之后。 风格与显示调整通常针对最终输出观感。
- **成本**：中。源码声明 2 个 pass；8 个可调参数。未进行 GPU 基准测试。
- **HDR 注意**：CRT 数学按 SDR 显示观感设计，HDR 高光和颜色可能明显改变。 GroupAHdrRoutes 明确以 R8/sRGB SDR 适配运行，FP16 中间纹理不构成原生 HDR 证明。
- **源码依据**：[GTU_v050.hlsl:11](<../../../src/Effects/CRT/GTU_v050.hlsl#L11>)、[GTU_v050.hlsl:84](<../../../src/Effects/CRT/GTU_v050.hlsl#L84>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

## CuNNy（20）

### CuNNy 16x16C NVL DN

当前配置 ID：`CuNNy\CuNNy-16x16C-NVL-DN`。

神经网络 2× 放大并抑制噪声，优先面向视觉小说、动画与线条内容。 DN 模型适合输入带压缩噪点时一并清理和放大。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：16 层 / 16 通道 / DN 降噪训练；固定 2×、18 个效果通道。网络规模更大通常代价更高，但未做本机速度或质量排序。
- **推荐**：进阶／实验。多通道或较大网络模型，适合在有性能余量时比较。 适用条件／尝试建议：按目标分辨率检查性能；不要仅按名称推断画质。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：高。源码声明 18 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[CuNNy-16x16C-NVL-DN.hlsl:17](<../../../src/Effects/CuNNy/CuNNy-16x16C-NVL-DN.hlsl#L17>)、[CuNNy-16x16C-NVL-DN.hlsl:31](<../../../src/Effects/CuNNy/CuNNy-16x16C-NVL-DN.hlsl#L31>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### CuNNy 16x16C NVL

当前配置 ID：`CuNNy\CuNNy-16x16C-NVL`。

神经网络 2× 放大，优先面向视觉小说、动画与线条内容。 标准模型保留更多输入纹理，适合相对干净的视觉小说或动画来源。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：16 层 / 16 通道；固定 2×、18 个效果通道。网络规模更大通常代价更高，但未做本机速度或质量排序。
- **推荐**：进阶／实验。多通道或较大网络模型，适合在有性能余量时比较。 适用条件／尝试建议：按目标分辨率检查性能；不要仅按名称推断画质。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：高。源码声明 18 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[CuNNy-16x16C-NVL.hlsl:17](<../../../src/Effects/CuNNy/CuNNy-16x16C-NVL.hlsl#L17>)、[CuNNy-16x16C-NVL.hlsl:31](<../../../src/Effects/CuNNy/CuNNy-16x16C-NVL.hlsl#L31>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### CuNNy 2x4C NVL DN

当前配置 ID：`CuNNy\CuNNy-2x4C-NVL-DN`。

神经网络 2× 放大并抑制噪声，优先面向视觉小说、动画与线条内容。 DN 模型适合输入带压缩噪点时一并清理和放大。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：2 层 / 4 通道 / DN 降噪训练；固定 2×、4 个效果通道。网络规模更大通常代价更高，但未做本机速度或质量排序。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中。源码声明 4 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[CuNNy-2x4C-NVL-DN.hlsl:17](<../../../src/Effects/CuNNy/CuNNy-2x4C-NVL-DN.hlsl#L17>)、[CuNNy-2x4C-NVL-DN.hlsl:31](<../../../src/Effects/CuNNy/CuNNy-2x4C-NVL-DN.hlsl#L31>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### CuNNy 2x4C NVL

当前配置 ID：`CuNNy\CuNNy-2x4C-NVL`。

神经网络 2× 放大，优先面向视觉小说、动画与线条内容。 标准模型保留更多输入纹理，适合相对干净的视觉小说或动画来源。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：2 层 / 4 通道；固定 2×、4 个效果通道。网络规模更大通常代价更高，但未做本机速度或质量排序。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中。源码声明 4 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[CuNNy-2x4C-NVL.hlsl:17](<../../../src/Effects/CuNNy/CuNNy-2x4C-NVL.hlsl#L17>)、[CuNNy-2x4C-NVL.hlsl:31](<../../../src/Effects/CuNNy/CuNNy-2x4C-NVL.hlsl#L31>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### CuNNy 3x4C NVL DN

当前配置 ID：`CuNNy\CuNNy-3x4C-NVL-DN`。

神经网络 2× 放大并抑制噪声，优先面向视觉小说、动画与线条内容。 DN 模型适合输入带压缩噪点时一并清理和放大。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：3 层 / 4 通道 / DN 降噪训练；固定 2×、5 个效果通道。网络规模更大通常代价更高，但未做本机速度或质量排序。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中到高。源码声明 5 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[CuNNy-3x4C-NVL-DN.hlsl:17](<../../../src/Effects/CuNNy/CuNNy-3x4C-NVL-DN.hlsl#L17>)、[CuNNy-3x4C-NVL-DN.hlsl:31](<../../../src/Effects/CuNNy/CuNNy-3x4C-NVL-DN.hlsl#L31>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### CuNNy 3x4C NVL

当前配置 ID：`CuNNy\CuNNy-3x4C-NVL`。

神经网络 2× 放大，优先面向视觉小说、动画与线条内容。 标准模型保留更多输入纹理，适合相对干净的视觉小说或动画来源。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：3 层 / 4 通道；固定 2×、5 个效果通道。网络规模更大通常代价更高，但未做本机速度或质量排序。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中到高。源码声明 5 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[CuNNy-3x4C-NVL.hlsl:17](<../../../src/Effects/CuNNy/CuNNy-3x4C-NVL.hlsl#L17>)、[CuNNy-3x4C-NVL.hlsl:31](<../../../src/Effects/CuNNy/CuNNy-3x4C-NVL.hlsl#L31>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### CuNNy 4x16C NVL DN

当前配置 ID：`CuNNy\CuNNy-4x16C-NVL-DN`。

神经网络 2× 放大并抑制噪声，优先面向视觉小说、动画与线条内容。 DN 模型适合输入带压缩噪点时一并清理和放大。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：4 层 / 16 通道 / DN 降噪训练；固定 2×、6 个效果通道。网络规模更大通常代价更高，但未做本机速度或质量排序。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中到高。源码声明 6 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[CuNNy-4x16C-NVL-DN.hlsl:17](<../../../src/Effects/CuNNy/CuNNy-4x16C-NVL-DN.hlsl#L17>)、[CuNNy-4x16C-NVL-DN.hlsl:31](<../../../src/Effects/CuNNy/CuNNy-4x16C-NVL-DN.hlsl#L31>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### CuNNy 4x16C NVL

当前配置 ID：`CuNNy\CuNNy-4x16C-NVL`。

神经网络 2× 放大，优先面向视觉小说、动画与线条内容。 标准模型保留更多输入纹理，适合相对干净的视觉小说或动画来源。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：4 层 / 16 通道；固定 2×、6 个效果通道。网络规模更大通常代价更高，但未做本机速度或质量排序。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中到高。源码声明 6 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[CuNNy-4x16C-NVL.hlsl:17](<../../../src/Effects/CuNNy/CuNNy-4x16C-NVL.hlsl#L17>)、[CuNNy-4x16C-NVL.hlsl:31](<../../../src/Effects/CuNNy/CuNNy-4x16C-NVL.hlsl#L31>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### CuNNy 4x4C NVL DN

当前配置 ID：`CuNNy\CuNNy-4x4C-NVL-DN`。

神经网络 2× 放大并抑制噪声，优先面向视觉小说、动画与线条内容。 DN 模型适合输入带压缩噪点时一并清理和放大。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：4 层 / 4 通道 / DN 降噪训练；固定 2×、6 个效果通道。网络规模更大通常代价更高，但未做本机速度或质量排序。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中到高。源码声明 6 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[CuNNy-4x4C-NVL-DN.hlsl:17](<../../../src/Effects/CuNNy/CuNNy-4x4C-NVL-DN.hlsl#L17>)、[CuNNy-4x4C-NVL-DN.hlsl:31](<../../../src/Effects/CuNNy/CuNNy-4x4C-NVL-DN.hlsl#L31>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### CuNNy 4x4C NVL

当前配置 ID：`CuNNy\CuNNy-4x4C-NVL`。

神经网络 2× 放大，优先面向视觉小说、动画与线条内容。 标准模型保留更多输入纹理，适合相对干净的视觉小说或动画来源。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：4 层 / 4 通道；固定 2×、6 个效果通道。网络规模更大通常代价更高，但未做本机速度或质量排序。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中到高。源码声明 6 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[CuNNy-4x4C-NVL.hlsl:17](<../../../src/Effects/CuNNy/CuNNy-4x4C-NVL.hlsl#L17>)、[CuNNy-4x4C-NVL.hlsl:31](<../../../src/Effects/CuNNy/CuNNy-4x4C-NVL.hlsl#L31>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### CuNNy 4x8C NVL DN

当前配置 ID：`CuNNy\CuNNy-4x8C-NVL-DN`。

神经网络 2× 放大并抑制噪声，优先面向视觉小说、动画与线条内容。 DN 模型适合输入带压缩噪点时一并清理和放大。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：4 层 / 8 通道 / DN 降噪训练；固定 2×、6 个效果通道。网络规模更大通常代价更高，但未做本机速度或质量排序。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中到高。源码声明 6 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[CuNNy-4x8C-NVL-DN.hlsl:17](<../../../src/Effects/CuNNy/CuNNy-4x8C-NVL-DN.hlsl#L17>)、[CuNNy-4x8C-NVL-DN.hlsl:31](<../../../src/Effects/CuNNy/CuNNy-4x8C-NVL-DN.hlsl#L31>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### CuNNy 4x8C NVL

当前配置 ID：`CuNNy\CuNNy-4x8C-NVL`。

神经网络 2× 放大，优先面向视觉小说、动画与线条内容。 标准模型保留更多输入纹理，适合相对干净的视觉小说或动画来源。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：4 层 / 8 通道；固定 2×、6 个效果通道。网络规模更大通常代价更高，但未做本机速度或质量排序。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中到高。源码声明 6 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[CuNNy-4x8C-NVL.hlsl:17](<../../../src/Effects/CuNNy/CuNNy-4x8C-NVL.hlsl#L17>)、[CuNNy-4x8C-NVL.hlsl:31](<../../../src/Effects/CuNNy/CuNNy-4x8C-NVL.hlsl#L31>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### CuNNy 6x8C NVL DN

当前配置 ID：`CuNNy\CuNNy-6x8C-NVL-DN`。

神经网络 2× 放大并抑制噪声，优先面向视觉小说、动画与线条内容。 DN 模型适合输入带压缩噪点时一并清理和放大。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：6 层 / 8 通道 / DN 降噪训练；固定 2×、8 个效果通道。网络规模更大通常代价更高，但未做本机速度或质量排序。
- **推荐**：进阶／实验。多通道或较大网络模型，适合在有性能余量时比较。 适用条件／尝试建议：按目标分辨率检查性能；不要仅按名称推断画质。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中到高。源码声明 8 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[CuNNy-6x8C-NVL-DN.hlsl:17](<../../../src/Effects/CuNNy/CuNNy-6x8C-NVL-DN.hlsl#L17>)、[CuNNy-6x8C-NVL-DN.hlsl:31](<../../../src/Effects/CuNNy/CuNNy-6x8C-NVL-DN.hlsl#L31>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### CuNNy 6x8C NVL

当前配置 ID：`CuNNy\CuNNy-6x8C-NVL`。

神经网络 2× 放大，优先面向视觉小说、动画与线条内容。 标准模型保留更多输入纹理，适合相对干净的视觉小说或动画来源。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：6 层 / 8 通道；固定 2×、8 个效果通道。网络规模更大通常代价更高，但未做本机速度或质量排序。
- **推荐**：进阶／实验。多通道或较大网络模型，适合在有性能余量时比较。 适用条件／尝试建议：按目标分辨率检查性能；不要仅按名称推断画质。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中到高。源码声明 8 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[CuNNy-6x8C-NVL.hlsl:17](<../../../src/Effects/CuNNy/CuNNy-6x8C-NVL.hlsl#L17>)、[CuNNy-6x8C-NVL.hlsl:31](<../../../src/Effects/CuNNy/CuNNy-6x8C-NVL.hlsl#L31>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### CuNNy 8x16C NVL DN

当前配置 ID：`CuNNy\CuNNy-8x16C-NVL-DN`。

神经网络 2× 放大并抑制噪声，优先面向视觉小说、动画与线条内容。 DN 模型适合输入带压缩噪点时一并清理和放大。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：8 层 / 16 通道 / DN 降噪训练；固定 2×、10 个效果通道。网络规模更大通常代价更高，但未做本机速度或质量排序。
- **推荐**：进阶／实验。多通道或较大网络模型，适合在有性能余量时比较。 适用条件／尝试建议：按目标分辨率检查性能；不要仅按名称推断画质。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：高。源码声明 10 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[CuNNy-8x16C-NVL-DN.hlsl:17](<../../../src/Effects/CuNNy/CuNNy-8x16C-NVL-DN.hlsl#L17>)、[CuNNy-8x16C-NVL-DN.hlsl:31](<../../../src/Effects/CuNNy/CuNNy-8x16C-NVL-DN.hlsl#L31>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### CuNNy 8x16C NVL

当前配置 ID：`CuNNy\CuNNy-8x16C-NVL`。

神经网络 2× 放大，优先面向视觉小说、动画与线条内容。 标准模型保留更多输入纹理，适合相对干净的视觉小说或动画来源。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：8 层 / 16 通道；固定 2×、10 个效果通道。网络规模更大通常代价更高，但未做本机速度或质量排序。
- **推荐**：进阶／实验。多通道或较大网络模型，适合在有性能余量时比较。 适用条件／尝试建议：按目标分辨率检查性能；不要仅按名称推断画质。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：高。源码声明 10 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[CuNNy-8x16C-NVL.hlsl:17](<../../../src/Effects/CuNNy/CuNNy-8x16C-NVL.hlsl#L17>)、[CuNNy-8x16C-NVL.hlsl:31](<../../../src/Effects/CuNNy/CuNNy-8x16C-NVL.hlsl#L31>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### CuNNy 8x4C NVL DN

当前配置 ID：`CuNNy\CuNNy-8x4C-NVL-DN`。

神经网络 2× 放大并抑制噪声，优先面向视觉小说、动画与线条内容。 DN 模型适合输入带压缩噪点时一并清理和放大。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：8 层 / 4 通道 / DN 降噪训练；固定 2×、10 个效果通道。网络规模更大通常代价更高，但未做本机速度或质量排序。
- **推荐**：进阶／实验。多通道或较大网络模型，适合在有性能余量时比较。 适用条件／尝试建议：按目标分辨率检查性能；不要仅按名称推断画质。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：高。源码声明 10 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[CuNNy-8x4C-NVL-DN.hlsl:17](<../../../src/Effects/CuNNy/CuNNy-8x4C-NVL-DN.hlsl#L17>)、[CuNNy-8x4C-NVL-DN.hlsl:31](<../../../src/Effects/CuNNy/CuNNy-8x4C-NVL-DN.hlsl#L31>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### CuNNy 8x4C NVL

当前配置 ID：`CuNNy\CuNNy-8x4C-NVL`。

神经网络 2× 放大，优先面向视觉小说、动画与线条内容。 标准模型保留更多输入纹理，适合相对干净的视觉小说或动画来源。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：8 层 / 4 通道；固定 2×、10 个效果通道。网络规模更大通常代价更高，但未做本机速度或质量排序。
- **推荐**：进阶／实验。多通道或较大网络模型，适合在有性能余量时比较。 适用条件／尝试建议：按目标分辨率检查性能；不要仅按名称推断画质。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：高。源码声明 10 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[CuNNy-8x4C-NVL.hlsl:17](<../../../src/Effects/CuNNy/CuNNy-8x4C-NVL.hlsl#L17>)、[CuNNy-8x4C-NVL.hlsl:31](<../../../src/Effects/CuNNy/CuNNy-8x4C-NVL.hlsl#L31>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### CuNNy 8x8C NVL DN

当前配置 ID：`CuNNy\CuNNy-8x8C-NVL-DN`。

神经网络 2× 放大并抑制噪声，优先面向视觉小说、动画与线条内容。 DN 模型适合输入带压缩噪点时一并清理和放大。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：8 层 / 8 通道 / DN 降噪训练；固定 2×、10 个效果通道。网络规模更大通常代价更高，但未做本机速度或质量排序。
- **推荐**：进阶／实验。多通道或较大网络模型，适合在有性能余量时比较。 适用条件／尝试建议：按目标分辨率检查性能；不要仅按名称推断画质。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：高。源码声明 10 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[CuNNy-8x8C-NVL-DN.hlsl:17](<../../../src/Effects/CuNNy/CuNNy-8x8C-NVL-DN.hlsl#L17>)、[CuNNy-8x8C-NVL-DN.hlsl:31](<../../../src/Effects/CuNNy/CuNNy-8x8C-NVL-DN.hlsl#L31>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### CuNNy 8x8C NVL

当前配置 ID：`CuNNy\CuNNy-8x8C-NVL`。

神经网络 2× 放大，优先面向视觉小说、动画与线条内容。 标准模型保留更多输入纹理，适合相对干净的视觉小说或动画来源。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：8 层 / 8 通道；固定 2×、10 个效果通道。网络规模更大通常代价更高，但未做本机速度或质量排序。
- **推荐**：进阶／实验。多通道或较大网络模型，适合在有性能余量时比较。 适用条件／尝试建议：按目标分辨率检查性能；不要仅按名称推断画质。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：高。源码声明 10 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[CuNNy-8x8C-NVL.hlsl:17](<../../../src/Effects/CuNNy/CuNNy-8x8C-NVL.hlsl#L17>)、[CuNNy-8x8C-NVL.hlsl:31](<../../../src/Effects/CuNNy/CuNNy-8x8C-NVL.hlsl#L31>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

## CuNNy2（9）

### CuNNy 3x12 NVL

当前配置 ID：`CuNNy2\CuNNy-3x12-NVL`。

CuNNy2 神经网络 2× 放大，优先用于视觉小说与动画，并在速度与细节重建间选择模型。 显式深度/通道模型适合需要控制网络规模时比较。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：3 层 / 12 通道；固定 2×、5 个效果通道。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中到高。源码声明 5 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[CuNNy-3x12-NVL.hlsl:18](<../../../src/Effects/CuNNy2/CuNNy-3x12-NVL.hlsl#L18>)、[CuNNy-3x12-NVL.hlsl:32](<../../../src/Effects/CuNNy2/CuNNy-3x12-NVL.hlsl#L32>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### CuNNy 4x12 NVL

当前配置 ID：`CuNNy2\CuNNy-4x12-NVL`。

CuNNy2 神经网络 2× 放大，优先用于视觉小说与动画，并在速度与细节重建间选择模型。 显式深度/通道模型适合需要控制网络规模时比较。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：4 层 / 12 通道；固定 2×、6 个效果通道。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中到高。源码声明 6 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[CuNNy-4x12-NVL.hlsl:18](<../../../src/Effects/CuNNy2/CuNNy-4x12-NVL.hlsl#L18>)、[CuNNy-4x12-NVL.hlsl:32](<../../../src/Effects/CuNNy2/CuNNy-4x12-NVL.hlsl#L32>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### CuNNy 4x16 NVL

当前配置 ID：`CuNNy2\CuNNy-4x16-NVL`。

CuNNy2 神经网络 2× 放大，优先用于视觉小说与动画，并在速度与细节重建间选择模型。 显式深度/通道模型适合需要控制网络规模时比较。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：4 层 / 16 通道；固定 2×、6 个效果通道。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中到高。源码声明 6 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[CuNNy-4x16-NVL.hlsl:18](<../../../src/Effects/CuNNy2/CuNNy-4x16-NVL.hlsl#L18>)、[CuNNy-4x16-NVL.hlsl:32](<../../../src/Effects/CuNNy2/CuNNy-4x16-NVL.hlsl#L32>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### CuNNy 4x24 NVL

当前配置 ID：`CuNNy2\CuNNy-4x24-NVL`。

CuNNy2 神经网络 2× 放大，优先用于视觉小说与动画，并在速度与细节重建间选择模型。 显式深度/通道模型适合需要控制网络规模时比较。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：4 层 / 24 通道；固定 2×、6 个效果通道。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中到高。源码声明 6 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[CuNNy-4x24-NVL.hlsl:18](<../../../src/Effects/CuNNy2/CuNNy-4x24-NVL.hlsl#L18>)、[CuNNy-4x24-NVL.hlsl:32](<../../../src/Effects/CuNNy2/CuNNy-4x24-NVL.hlsl#L32>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### CuNNy 4x32 NVL

当前配置 ID：`CuNNy2\CuNNy-4x32-NVL`。

CuNNy2 神经网络 2× 放大，优先用于视觉小说与动画，并在速度与细节重建间选择模型。 显式深度/通道模型适合需要控制网络规模时比较。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：4 层 / 32 通道；固定 2×、6 个效果通道。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中到高。源码声明 6 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[CuNNy-4x32-NVL.hlsl:18](<../../../src/Effects/CuNNy2/CuNNy-4x32-NVL.hlsl#L18>)、[CuNNy-4x32-NVL.hlsl:32](<../../../src/Effects/CuNNy2/CuNNy-4x32-NVL.hlsl#L32>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### CuNNy 8x32 NVL

当前配置 ID：`CuNNy2\CuNNy-8x32-NVL`。

CuNNy2 神经网络 2× 放大，优先用于视觉小说与动画，并在速度与细节重建间选择模型。 显式深度/通道模型适合需要控制网络规模时比较。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：8 层 / 32 通道；固定 2×、10 个效果通道。
- **推荐**：进阶／实验。多通道或较大网络模型，适合在有性能余量时比较。 适用条件／尝试建议：按目标分辨率检查性能；不要仅按名称推断画质。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：高。源码声明 10 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[CuNNy-8x32-NVL.hlsl:18](<../../../src/Effects/CuNNy2/CuNNy-8x32-NVL.hlsl#L18>)、[CuNNy-8x32-NVL.hlsl:32](<../../../src/Effects/CuNNy2/CuNNy-8x32-NVL.hlsl#L32>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### CuNNy fast NVL

当前配置 ID：`CuNNy2\CuNNy-fast-NVL`。

CuNNy2 神经网络 2× 放大，优先用于视觉小说与动画，并在速度与细节重建间选择模型。 fast 速度预设适合先确认实时余量，再与显式网络规模变体比较。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：fast 命名预设；固定 2×、4 通道，名称来自模型但本机代价未测。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中。源码声明 4 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[CuNNy-fast-NVL.hlsl:18](<../../../src/Effects/CuNNy2/CuNNy-fast-NVL.hlsl#L18>)、[CuNNy-fast-NVL.hlsl:32](<../../../src/Effects/CuNNy2/CuNNy-fast-NVL.hlsl#L32>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### CuNNy faster NVL

当前配置 ID：`CuNNy2\CuNNy-faster-NVL`。

CuNNy2 神经网络 2× 放大，优先用于视觉小说与动画，并在速度与细节重建间选择模型。 faster 速度预设适合先确认实时余量，再与显式网络规模变体比较。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：faster 命名预设；固定 2×、4 通道，名称来自模型但本机代价未测。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中。源码声明 4 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[CuNNy-faster-NVL.hlsl:18](<../../../src/Effects/CuNNy2/CuNNy-faster-NVL.hlsl#L18>)、[CuNNy-faster-NVL.hlsl:32](<../../../src/Effects/CuNNy2/CuNNy-faster-NVL.hlsl#L32>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### CuNNy veryfast NVL

当前配置 ID：`CuNNy2\CuNNy-veryfast-NVL`。

CuNNy2 神经网络 2× 放大，优先用于视觉小说与动画，并在速度与细节重建间选择模型。 veryfast 速度预设适合先确认实时余量，再与显式网络规模变体比较。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：veryfast 命名预设；固定 2×、4 通道，名称来自模型但本机代价未测。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中。源码声明 4 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[CuNNy-veryfast-NVL.hlsl:18](<../../../src/Effects/CuNNy2/CuNNy-veryfast-NVL.hlsl#L18>)、[CuNNy-veryfast-NVL.hlsl:32](<../../../src/Effects/CuNNy2/CuNNy-veryfast-NVL.hlsl#L32>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

## Deband（1）

### Deband

当前配置 ID：`Deband`。

减轻渐变天空、暗部或低码率内容中的色带。 建议放在放大与锐化前，从较低强度开始观察渐变和纹理。

- **用途**：清理修复 → 去色带。**输出尺寸**：保持输入尺寸。
- **变体差异**：同尺寸处理；强度过高可能抹去细微纹理。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：放大前。 建议放在「放大/超分、锐化」之前。 先清理输入可减少后续放大噪声。
- **条件与取舍**：强度过高会损失纹理。
- **成本**：低到中。源码声明 1 个 pass；4 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[Deband.hlsl:4](<../../../src/Effects/Deband.hlsl#L4>)、[Deband.hlsl:58](<../../../src/Effects/Deband.hlsl#L58>)、[HdrEffectBoundary.cpp:8](<../../../src/Magpie.Core/HdrEffectBoundary.cpp#L8>)。

## DLSS（1）

### DLSS SR

当前配置 ID：`DLSS\DLSS_SR`。

使用 DLSS 时序超分后端，将捕获画面重建到目标尺寸。 适合在 NVIDIA RTX 显卡上尝试超分重建，可与 FSR、XeSS 对比细节与运动表现。捕获路径缺少原始相机抖动和真实深度，细节重建与运动稳定性受到限制；运动向量可以选择估算。

- **用途**：放大与超分 → 时序重建。**输出尺寸**：使用效果组配置的输出尺寸。
- **变体差异**：当前请求 Balanced / Preset J。用户反馈 L／M 模型可能附带轻微抗锯齿效果；当前版本尚未提供 L／M 模型选择参数，这项收益不代表默认模型的已验证表现。
- **推荐**：进阶／实验。缺少原始相机抖动和真实深度等原生重建条件，细节收益与运动稳定性需要按素材比较。 尝试建议：先从默认参数比较静止细节和文字边缘；再检查快速运动、遮挡和画面切换时的拖影；按画面需要选择估算运动，并与 FSR、XeSS 比较质量和开销。
- **管线位置**：主要缩放步骤。在必要的降噪／修复之后，在按需添加的抗锯齿、最终锐化／显示风格之前。先完成目标尺寸重建，再按实际画面需要添加边缘平滑、锐化和显示风格。
- **条件与取舍**：输出每一轴至少等于输入尺寸，由效果组的尺寸设置决定；深度使用全零资源，jitter=(0,0)；运动向量可选择估算；捕获式重建的细节和运动表现受输入条件限制；受支持的 NVIDIA GPU/驱动；包含对应 NGX 功能的构建与运行库。
- **成本**：硬件/运行时相关。实际成本来自原生 SDK 调用、资源传输与同步；HLSL 占位 pass 数无法代表这些开销。本轮未进行 GPU 基准测试。
- **源码依据**：[DLSS_SR.hlsl:3](<../../../src/Effects/DLSS/DLSS_SR.hlsl#L3>)、[DLSS_SR.hlsl:35](<../../../src/Effects/DLSS/DLSS_SR.hlsl#L35>)、[NativeEffectBackendFactory.cpp:113](<../../../src/Magpie.Core/NativeEffectBackendFactory.cpp#L113>)、[DLSSSRUpscaler.cpp:138](<../../../src/Magpie.Core/DLSSSRUpscaler.cpp#L138>)、[DLSSSRUpscaler.cpp:202](<../../../src/Magpie.Core/DLSSSRUpscaler.cpp#L202>)、[DLSSSRUpscaler.cpp:267](<../../../src/Magpie.Core/DLSSSRUpscaler.cpp#L267>)、[20260908-effect-picker-naming-compatibility.md:7](<../../../docs/experimental/reviews/20260908-effect-picker-naming-compatibility.md#L7>)。

## DLSSFG（1）

### DLSS FrameGeneration

当前配置 ID：`DLSSFG\DLSS_FrameGeneration`。

通过 DLSS 原生呈现后端在捕获的真实帧之间生成帧。 列表项是 identity marker；生成帧由呈现端独立发布，不是着色器在该列表位置输出。外部捕获的运动信息有限，UI、光标与遮挡场景需实测。

- **用途**：补帧与帧率 → 补帧。**输出尺寸**：保持输入尺寸。
- **变体差异**：2–4× 请求（实际接受值由后端能力决定）补帧标记；同组只能有一个补帧效果，宜配合全局 Frame Rate Filter 理解输入/输出帧率。
- **推荐**：进阶／实验。原生呈现端补帧会改变延迟与发布节奏，并受捕获运动估计限制。 适用条件／尝试建议：同组只选一个补帧器；检查 UI、光标、切屏和快速运动。
- **管线位置**：呈现终端。 建议放在「全部普通画面效果」之后。 生成帧由呈现端独立发布。
- **条件与取舍**：同一效果组只能有一个补帧效果；identity marker 的列表位置不等于原生后端执行位置；受支持的 NVIDIA GPU/驱动；包含对应 NGX 功能的构建与运行库。
- **成本**：硬件/运行时相关。实际成本来自原生 SDK 调用、资源传输与同步；HLSL 占位 pass 数无法代表这些开销。本轮未进行 GPU 基准测试。
- **HDR 注意**：DLSS 补帧的 HDR backbuffer/UI 色彩协议依赖运行时，尚无本机画面验证。 EffectProtocolCatalogC 记录 FP16 终端候选，但 defaultForHdr=false 且证据为本地/社区实验级。
- **源码依据**：[DLSS_FrameGeneration.hlsl:4](<../../../src/Effects/DLSSFG/DLSS_FrameGeneration.hlsl#L4>)、[DLSS_FrameGeneration.hlsl:33](<../../../src/Effects/DLSSFG/DLSS_FrameGeneration.hlsl#L33>)、[Renderer.cpp:118](<../../../src/Magpie.Core/Renderer.cpp#L118>)、[Renderer.cpp:3171](<../../../src/Magpie.Core/Renderer.cpp#L3171>)。

## DLSSNR（1）

### DLSSNR AI Filter

当前配置 ID：`DLSSNR\DLSSNR_AI_Filter`。

调用实验性 DLSSNR 原生适配器做同尺寸 AI 画面与纹理风格重塑。 用于改变细节组织与整体观感；原生创建失败时工厂允许占位直通，因此会话启动不代表算法已实际启用。

- **用途**：风格显示 → AI 画面重塑。**输出尺寸**：保持输入尺寸。
- **变体差异**：同尺寸原生替换；依赖 NGX Feature 18 与受支持环境；底层接口标识只用于定位，不据此扩展用户功能说明。
- **推荐**：进阶／实验。这是实验性同尺寸 AI 风格重塑，且占位直通可能掩盖原生初始化失败。 适用条件／尝试建议：从日志确认原生功能创建成功；用代表性画面检查纹理重塑是否符合偏好。
- **管线位置**：同尺寸 AI 画面重塑。 建议放在「主要尺寸整理（若希望在最终尺寸重塑纹理）」之后。 建议放在「最终锐化/显示风格」之前。 前后位置会改变其处理到的纹理范围；先把它当主处理步骤，再做少量收尾。
- **条件与取舍**：运行时由原生后端替换占位 pass；初始化失败可能继续使用直通占位；NVIDIA 驱动 615 及以上；RTX 50 系列原生支持（按用户提供的本项目兼容口径）；RTX 40／30／20 系列可能需要手动替换兼容 DLL，替换后仍需确认实际功能初始化结果；包含对应 NGX 功能的构建与运行库。
- **成本**：硬件/运行时相关。实际成本来自原生 SDK 调用、资源传输与同步；HLSL 占位 pass 数无法代表这些开销。本轮未进行 GPU 基准测试。
- **HDR 注意**：HDR 路由取决于实验开关与归一化倍率；请确认原生后端实际启用并检查颜色。 GroupBHdrRoutes 为默认 R8 SDR 与实验性有界 FP16 分别建模。
- **源码依据**：[DLSSNR_AI_Filter.hlsl:4](<../../../src/Effects/DLSSNR/DLSSNR_AI_Filter.hlsl#L4>)、[DLSSNR_AI_Filter.hlsl:169](<../../../src/Effects/DLSSNR/DLSSNR_AI_Filter.hlsl#L169>)、[NativeEffectBackendFactory.cpp:93](<../../../src/Magpie.Core/NativeEffectBackendFactory.cpp#L93>)、[GroupBHdrRoutes.cpp:56](<../../../src/Magpie.Core/GroupBHdrRoutes.cpp#L56>)、[20260908-effect-picker-naming-compatibility.md:7](<../../../docs/experimental/reviews/20260908-effect-picker-naming-compatibility.md#L7>)。

## Frame Guidance Diagnostics（2）

### FrameGuidance Confidence

当前配置 ID：`Diagnostics\FrameGuidance_Confidence`。

显示当前帧引导置信度，帮助诊断时序后端的可靠区域。 这是诊断输出，不是画质增强；应临时放在链尾观察。

- **用途**：调试自定义 → 诊断。**输出尺寸**：保持输入尺寸。
- **变体差异**：置信度视图，同尺寸、单通道；输出不应继续当作正常图像处理。
- **推荐**：仅诊断使用。仅在排查帧引导时使用。 适用条件／尝试建议：观察完毕后从正常效果组移除。
- **管线位置**：临时链尾诊断。 建议放在「被诊断的处理」之后。 便于观察最终帧引导状态。
- **条件与取舍**：输出为可视化，不应继续作为正常画面。
- **成本**：低到中。源码声明 1 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **HDR 注意**：这是诊断色图，不代表 HDR 画面输出。 诊断后端生成运动或置信度可视化。
- **源码依据**：[FrameGuidance_Confidence.hlsl:1](<../../../src/Effects/Diagnostics/FrameGuidance_Confidence.hlsl#L1>)、[FrameGuidance_Confidence.hlsl:10](<../../../src/Effects/Diagnostics/FrameGuidance_Confidence.hlsl#L10>)、[HdrEffectBoundary.cpp:8](<../../../src/Magpie.Core/HdrEffectBoundary.cpp#L8>)。

### FrameGuidance Motion

当前配置 ID：`Diagnostics\FrameGuidance_Motion`。

把帧引导运动可视化，帮助检查捕获帧间运动估计。 这是诊断输出，不是画质增强；应临时放在链尾观察。

- **用途**：调试自定义 → 诊断。**输出尺寸**：保持输入尺寸。
- **变体差异**：运动视图，同尺寸、单通道；输出不应继续当作正常图像处理。
- **推荐**：仅诊断使用。仅在排查帧引导时使用。 适用条件／尝试建议：观察完毕后从正常效果组移除。
- **管线位置**：临时链尾诊断。 建议放在「被诊断的处理」之后。 便于观察最终帧引导状态。
- **条件与取舍**：输出为可视化，不应继续作为正常画面。
- **成本**：低到中。源码声明 1 个 pass；1 个可调参数。未进行 GPU 基准测试。
- **HDR 注意**：这是诊断色图，不代表 HDR 画面输出。 诊断后端生成运动或置信度可视化。
- **源码依据**：[FrameGuidance_Motion.hlsl:1](<../../../src/Effects/Diagnostics/FrameGuidance_Motion.hlsl#L1>)、[FrameGuidance_Motion.hlsl:18](<../../../src/Effects/Diagnostics/FrameGuidance_Motion.hlsl#L18>)、[HdrEffectBoundary.cpp:8](<../../../src/Magpie.Core/HdrEffectBoundary.cpp#L8>)。

## FrameRate_Filter（1）

### Frame Rate Filter

当前配置 ID：`FrameRate_Filter`。

限制捕获与处理目标帧率，便于稳帧或给补帧设定基础帧率。 它控制运行调度而非画面内容，常用于给补帧设定基础帧率或减少无效处理。

- **用途**：补帧与帧率 → 帧率调度。**输出尺寸**：保持输入尺寸。
- **变体差异**：HLSL 只是同尺寸直通；Renderer 扫描整条效果链并取最低目标值，列表位置不改变调度范围。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：全局帧率调度。 这是调度标记，不是按列表位置执行的画面处理。
- **条件与取舍**：Renderer 扫描全链，列表顺序不改变限速作用域；多项同时存在时采用最低有效目标帧率。
- **成本**：低到中。源码声明 1 个 pass；2 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[FrameRate_Filter.hlsl:4](<../../../src/Effects/FrameRate_Filter.hlsl#L4>)、[FrameRate_Filter.hlsl:29](<../../../src/Effects/FrameRate_Filter.hlsl#L29>)、[Renderer.cpp:2820](<../../../src/Magpie.Core/Renderer.cpp#L2820>)。

## FSR（2）

### FSR EASU

当前配置 ID：`FSR\FSR_EASU`。

使用 FSR EASU 做空间放大。 适合跨 GPU 的通用低成本空间放大。

- **用途**：放大与超分 → 通用。**输出尺寸**：使用效果组配置的输出尺寸。
- **变体差异**：输出尺寸由效果组决定、单通道。
- **推荐**：优先尝试。通用且链路职责清楚，便于建立画质/性能基线。 适用条件／尝试建议：按内容类型比较锐度与振铃。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **成本**：低到中。源码声明 1 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[FSR_EASU.hlsl:4](<../../../src/Effects/FSR/FSR_EASU.hlsl#L4>)、[FSR_EASU.hlsl:14](<../../../src/Effects/FSR/FSR_EASU.hlsl#L14>)、[HdrEffectBoundary.cpp:8](<../../../src/Magpie.Core/HdrEffectBoundary.cpp#L8>)。

### FSR RCAS

当前配置 ID：`FSR\FSR_RCAS`。

使用 FSR RCAS 做同尺寸锐化。 适合放大后补偿柔和感。

- **用途**：锐化细节 → 通用锐化。**输出尺寸**：保持输入尺寸。
- **变体差异**：同尺寸单通道。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：链尾收尾。 建议放在「放大/超分、降噪」之后。 建议放在「CRT/显示调整」之前。 按最终尺寸补偿柔和感。
- **条件与取舍**：过强会放大噪声并产生光晕。
- **成本**：低到中。源码声明 1 个 pass；1 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[FSR_RCAS.hlsl:4](<../../../src/Effects/FSR/FSR_RCAS.hlsl#L4>)、[FSR_RCAS.hlsl:24](<../../../src/Effects/FSR/FSR_RCAS.hlsl#L24>)、[HdrEffectBoundary.cpp:8](<../../../src/Magpie.Core/HdrEffectBoundary.cpp#L8>)。

## FSR2（1）

### FSR2 SR

当前配置 ID：`FSR2\FSR2_SR`。

调用 FSR2 原生时序超分后端，把较低分辨率捕获画面重建到目标尺寸。 外部捕获只能从相邻图像估计运动，并使用合成深度/jitter等辅助量；实际稳定性不同于游戏内 SDK 原生集成。

- **用途**：放大与超分 → 时序重建。**输出尺寸**：使用效果组配置的输出尺寸。
- **变体差异**：HLSL 为占位通道，运行时由 FSR2 后端替换；只支持放大，具体倍率/硬件/运行库限制由初始化检查决定。
- **推荐**：进阶／实验。依赖原生运行库和外部捕获的光流/合成辅助量。 适用条件／尝试建议：确认后端初始化成功；检查快速运动、遮挡和历史重置。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：只用于放大；外部捕获辅助量不等同游戏内集成；仅支持放大，不声明 3× 上限；包含 FSR2 后端的构建与运行库；只用于放大；当前代码未声明 3× 上限。
- **成本**：硬件/运行时相关。实际成本来自原生 SDK 调用、资源传输与同步；HLSL 占位 pass 数无法代表这些开销。本轮未进行 GPU 基准测试。
- **源码依据**：[FSR2_SR.hlsl:3](<../../../src/Effects/FSR2/FSR2_SR.hlsl#L3>)、[FSR2_SR.hlsl:35](<../../../src/Effects/FSR2/FSR2_SR.hlsl#L35>)、[NativeEffectBackendFactory.cpp:113](<../../../src/Magpie.Core/NativeEffectBackendFactory.cpp#L113>)。

## FSR3（1）

### FSR3 SR

当前配置 ID：`FSR3\FSR3_SR`。

调用 FSR3 原生时序超分后端，把较低分辨率捕获画面重建到目标尺寸。 外部捕获只能从相邻图像估计运动，并使用合成深度/jitter等辅助量；实际稳定性不同于游戏内 SDK 原生集成。

- **用途**：放大与超分 → 时序重建。**输出尺寸**：使用效果组配置的输出尺寸。
- **变体差异**：HLSL 为占位通道，运行时由 FSR3 后端替换；只支持放大，具体倍率/硬件/运行库限制由初始化检查决定。
- **推荐**：进阶／实验。依赖原生运行库和外部捕获的光流/合成辅助量。 适用条件／尝试建议：确认后端初始化成功；检查快速运动、遮挡和历史重置。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：只用于放大；外部捕获辅助量不等同游戏内集成；每轴放大倍率限于 1× 至 3×；包含对应 FidelityFX 后端的构建与运行库；满足运行时 GPU 能力检查；每轴放大倍率 1× 至 3×。
- **成本**：硬件/运行时相关。实际成本来自原生 SDK 调用、资源传输与同步；HLSL 占位 pass 数无法代表这些开销。本轮未进行 GPU 基准测试。
- **源码依据**：[FSR3_SR.hlsl:3](<../../../src/Effects/FSR3/FSR3_SR.hlsl#L3>)、[FSR3_SR.hlsl:35](<../../../src/Effects/FSR3/FSR3_SR.hlsl#L35>)、[NativeEffectBackendFactory.cpp:113](<../../../src/Magpie.Core/NativeEffectBackendFactory.cpp#L113>)、[FSR3Upscaler.cpp:287](<../../../src/Magpie.Core/FSR3Upscaler.cpp#L287>)。

## FSR4（1）

### FSR4 SR

当前配置 ID：`FSR4\FSR4_SR`。

调用 FSR4 原生时序超分后端，把较低分辨率捕获画面重建到目标尺寸。 外部捕获只能从相邻图像估计运动，并使用合成深度/jitter等辅助量；实际稳定性不同于游戏内 SDK 原生集成。

- **用途**：放大与超分 → 时序重建。**输出尺寸**：使用效果组配置的输出尺寸。
- **变体差异**：HLSL 为占位通道，运行时由 FSR4 后端替换；只支持放大，具体倍率/硬件/运行库限制由初始化检查决定。
- **推荐**：进阶／实验。依赖原生运行库和外部捕获的光流/合成辅助量。 适用条件／尝试建议：确认后端初始化成功；检查快速运动、遮挡和历史重置。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：只用于放大；外部捕获辅助量不等同游戏内集成；每轴放大倍率限于 1× 至 3×；包含对应 FidelityFX 后端的构建与运行库；满足运行时 GPU 能力检查；每轴放大倍率 1× 至 3×。
- **成本**：硬件/运行时相关。实际成本来自原生 SDK 调用、资源传输与同步；HLSL 占位 pass 数无法代表这些开销。本轮未进行 GPU 基准测试。
- **源码依据**：[FSR4_SR.hlsl:3](<../../../src/Effects/FSR4/FSR4_SR.hlsl#L3>)、[FSR4_SR.hlsl:35](<../../../src/Effects/FSR4/FSR4_SR.hlsl#L35>)、[NativeEffectBackendFactory.cpp:113](<../../../src/Magpie.Core/NativeEffectBackendFactory.cpp#L113>)、[FSR3Upscaler.cpp:287](<../../../src/Magpie.Core/FSR3Upscaler.cpp#L287>)。

## FSRCNNX（2）

### FSRCNNX LineArt

当前配置 ID：`FSRCNNX\FSRCNNX_LineArt`。

FSRCNNX 神经网络固定 2× 放大，针对线稿/动画。 优先在线条清晰、平坦色块较多的素材上尝试。

- **用途**：放大与超分 → 动画线条。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：LineArt模型，固定 2×、6 通道。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中到高。源码声明 6 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[FSRCNNX_LineArt.hlsl:4](<../../../src/Effects/FSRCNNX/FSRCNNX_LineArt.hlsl#L4>)、[FSRCNNX_LineArt.hlsl:18](<../../../src/Effects/FSRCNNX/FSRCNNX_LineArt.hlsl#L18>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### FSRCNNX

当前配置 ID：`FSRCNNX\FSRCNNX`。

FSRCNNX 神经网络固定 2× 放大。 通用模型可用于一般视频与图像，动画素材也可与 LineArt 版比较。

- **用途**：放大与超分 → 动画线条。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：通用模型，固定 2×、6 通道。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中到高。源码声明 6 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[FSRCNNX.hlsl:5](<../../../src/Effects/FSRCNNX/FSRCNNX.hlsl#L5>)、[FSRCNNX.hlsl:19](<../../../src/Effects/FSRCNNX/FSRCNNX.hlsl#L19>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

## FXAA（3）

### FXAA High

当前配置 ID：`FXAA\FXAA_High`。

快速空间抗锯齿，适合低成本缓和边缘锯齿。 单帧处理，不依赖运动历史；可能轻微软化文字和细纹理。

- **用途**：抗锯齿 → 空间。**输出尺寸**：保持输入尺寸。
- **变体差异**：High 预设、单通道；档位差异来自源码参数，未做本机清晰度排名。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：放大后、锐化前。 建议放在「主要缩放」之后。 建议放在「锐化」之前。 在目标分辨率上平滑最终几何边缘。
- **成本**：低到中。源码声明 1 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[FXAA_High.hlsl:3](<../../../src/Effects/FXAA/FXAA_High.hlsl#L3>)、[FXAA_High.hlsl:14](<../../../src/Effects/FXAA/FXAA_High.hlsl#L14>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### FXAA Medium

当前配置 ID：`FXAA\FXAA_Medium`。

快速空间抗锯齿，适合低成本缓和边缘锯齿。 单帧处理，不依赖运动历史；可能轻微软化文字和细纹理。

- **用途**：抗锯齿 → 空间。**输出尺寸**：保持输入尺寸。
- **变体差异**：Medium 预设、单通道；档位差异来自源码参数，未做本机清晰度排名。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：放大后、锐化前。 建议放在「主要缩放」之后。 建议放在「锐化」之前。 在目标分辨率上平滑最终几何边缘。
- **成本**：低到中。源码声明 1 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[FXAA_Medium.hlsl:3](<../../../src/Effects/FXAA/FXAA_Medium.hlsl#L3>)、[FXAA_Medium.hlsl:14](<../../../src/Effects/FXAA/FXAA_Medium.hlsl#L14>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

### FXAA Ultra

当前配置 ID：`FXAA\FXAA_Ultra`。

快速空间抗锯齿，适合低成本缓和边缘锯齿。 单帧处理，不依赖运动历史；可能轻微软化文字和细纹理。

- **用途**：抗锯齿 → 空间。**输出尺寸**：保持输入尺寸。
- **变体差异**：Ultra 预设、单通道；档位差异来自源码参数，未做本机清晰度排名。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：放大后、锐化前。 建议放在「主要缩放」之后。 建议放在「锐化」之前。 在目标分辨率上平滑最终几何边缘。
- **成本**：低到中。源码声明 1 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[FXAA_Ultra.hlsl:3](<../../../src/Effects/FXAA/FXAA_Ultra.hlsl#L3>)、[FXAA_Ultra.hlsl:14](<../../../src/Effects/FXAA/FXAA_Ultra.hlsl#L14>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

## ImageAdjustment（1）

### Image Adjustment

当前配置 ID：`ImageAdjustment`。

统一调整 Gamma、亮度、对比度、饱和度与 RGB 通道。 调整 Gamma、亮度、对比度、饱和度与 RGB 通道，适合作为最终观感修正。

- **用途**：风格显示 → 图像调整。**输出尺寸**：保持输入尺寸。
- **变体差异**：同尺寸显示变换；建议接近链尾，HDR 下需留意参数的显示域含义。
- **推荐**：特定场景推荐。适合特定素材、风格或实验场景。 适用条件／尝试建议：先在代表性画面比较副作用。
- **管线位置**：链尾显示。 建议放在「主要画质处理」之后。 风格与显示调整通常针对最终输出观感。
- **成本**：低到中。源码声明 1 个 pass；10 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[ImageAdjustment.hlsl:3](<../../../src/Effects/ImageAdjustment.hlsl#L3>)、[ImageAdjustment.hlsl:103](<../../../src/Effects/ImageAdjustment.hlsl#L103>)、[HdrEffectBoundary.cpp:8](<../../../src/Magpie.Core/HdrEffectBoundary.cpp#L8>)。

## Jinc（1）

### Jinc

当前配置 ID：`Jinc`。

高质量重采样，适合摄影、界面与一般视频放大。 边缘较锐利，适合与 Lanczos/Bicubic 比较振铃和纹理保持。

- **用途**：放大与超分 → 通用。**输出尺寸**：使用效果组配置的输出尺寸。
- **变体差异**：输出尺寸由效果组决定；负瓣可能带来轻微振铃。
- **推荐**：优先尝试。通用且链路职责清楚，便于建立画质/性能基线。 适用条件／尝试建议：按内容类型比较锐度与振铃。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **成本**：低到中。源码声明 1 个 pass；3 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[Jinc.hlsl:12](<../../../src/Effects/Jinc.hlsl#L12>)、[Jinc.hlsl:47](<../../../src/Effects/Jinc.hlsl#L47>)、[HdrEffectBoundary.cpp:8](<../../../src/Magpie.Core/HdrEffectBoundary.cpp#L8>)。

## k7_modernAnime_FHD_x2（1）

### k7 modernAnime FHD x2

当前配置 ID：`k7_modernAnime_FHD_x2`。

面向现代动画全高清素材的神经网络 2× 放大。 目标素材较明确；低清、真人或高噪声来源应与其他模型对比。

- **用途**：放大与超分 → 动画线条。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：固定 2×、8 通道；模型目标域较窄，真人或低清素材效果需另行比较。
- **推荐**：进阶／实验。多通道或较大网络模型，适合在有性能余量时比较。 适用条件／尝试建议：按目标分辨率检查性能；不要仅按名称推断画质。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **成本**：中到高。源码声明 8 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[k7_modernAnime_FHD_x2.hlsl:4](<../../../src/Effects/k7_modernAnime_FHD_x2.hlsl#L4>)、[k7_modernAnime_FHD_x2.hlsl:16](<../../../src/Effects/k7_modernAnime_FHD_x2.hlsl#L16>)、[HdrEffectBoundary.cpp:8](<../../../src/Magpie.Core/HdrEffectBoundary.cpp#L8>)。

## Lanczos（1）

### Lanczos

当前配置 ID：`Lanczos`。

经典锐利重采样，适合文字、界面和一般图像。 适合希望比双线性更锐利的场景，需观察高对比边缘的光晕。

- **用途**：放大与超分 → 通用。**输出尺寸**：使用效果组配置的输出尺寸。
- **变体差异**：输出尺寸由效果组决定；锐利边缘附近可能出现振铃。
- **推荐**：优先尝试。通用且链路职责清楚，便于建立画质/性能基线。 适用条件／尝试建议：按内容类型比较锐度与振铃。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **成本**：低到中。源码声明 1 个 pass；1 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[Lanczos.hlsl:4](<../../../src/Effects/Lanczos.hlsl#L4>)、[Lanczos.hlsl:20](<../../../src/Effects/Lanczos.hlsl#L20>)、[HdrEffectBoundary.cpp:8](<../../../src/Magpie.Core/HdrEffectBoundary.cpp#L8>)。

## MLAA（1）

### MLAA

当前配置 ID：`MLAA\MLAA`。

形态学空间抗锯齿，分析边缘后做同帧混合。 适合不具备时序信息时缓和几何锯齿。

- **用途**：抗锯齿 → 空间。**输出尺寸**：保持输入尺寸。
- **变体差异**：同尺寸、3 通道；可能影响细小文字。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：放大后、锐化前。 建议放在「主要缩放」之后。 建议放在「锐化」之前。 在目标分辨率上平滑最终几何边缘。
- **成本**：中。源码声明 3 个 pass；2 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[MLAA.hlsl:25](<../../../src/Effects/MLAA/MLAA.hlsl#L25>)、[MLAA.hlsl:51](<../../../src/Effects/MLAA/MLAA.hlsl#L51>)、[GroupAHdrRoutes.cpp:48](<../../../src/Magpie.Core/GroupAHdrRoutes.cpp#L48>)。

## MMPX（1）

### MMPX

当前配置 ID：`Pixel Art\MMPX`。

MMPX 固定 2× 像素画放大，保留像素块并修整局部斜线。 适合低分辨率精灵和复古游戏；避免用于照片。

- **用途**：放大与超分 → 像素画。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：固定 2×、单通道。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：主要像素画缩放步骤。 建议放在「按需添加的 CRT 风格／显示调整」之前。 先观察像素画缩放本身的结果，保留像素边界，再决定是否添加风格处理。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：低到中。源码声明 1 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[MMPX.hlsl:4](<../../../src/Effects/Pixel Art/MMPX.hlsl#L4>)、[MMPX.hlsl:14](<../../../src/Effects/Pixel Art/MMPX.hlsl#L14>)、[EffectProtocolCatalogC.h:70](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L70>)。

## Nearest（1）

### Nearest

当前配置 ID：`Nearest`。

最近邻缩放，完整保留像素块，不做平滑。 不会混合相邻像素，适合整数倍像素画；普通视频会显得块状。

- **用途**：放大与超分 → 像素画。**输出尺寸**：使用效果组配置的输出尺寸。
- **变体差异**：输出尺寸由效果组决定；适合像素画，也可作为采样基线。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：主要像素画缩放步骤。 建议放在「按需添加的 CRT 风格／显示调整」之前。 先观察像素画缩放本身的结果，保留像素边界，再决定是否添加风格处理。
- **成本**：低到中。源码声明 1 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[Nearest.hlsl:1](<../../../src/Effects/Nearest.hlsl#L1>)、[Nearest.hlsl:8](<../../../src/Effects/Nearest.hlsl#L8>)、[HdrEffectBoundary.cpp:8](<../../../src/Magpie.Core/HdrEffectBoundary.cpp#L8>)。

## NIS（2）

### NIS

当前配置 ID：`NIS\NIS`。

NIS 空间缩放并可锐化。 适合通用实时放大，输出尺寸由效果组决定。

- **用途**：放大与超分 → 通用。**输出尺寸**：使用效果组配置的输出尺寸。
- **变体差异**：任意输出尺寸、单通道；HDR 编译路径仍需当前构建与像素验证。
- **推荐**：优先尝试。通用且链路职责清楚，便于建立画质/性能基线。 适用条件／尝试建议：按内容类型比较锐度与振铃；优先在 SDR 素材中比较；HDR 画面需先核对本报告列出的着色器模式疑点。。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **成本**：低到中。源码声明 1 个 pass；1 个可调参数。未进行 GPU 基准测试。
- **HDR 注意**：协议选择 FP16 HDR，但当前 NIS 着色器未设置 NIS_HDR_MODE，静态配置可能仍走 NONE 数学路径。 GroupBHdrRoutes 声明 DirectFP16；NIS_Scaler.hlsli 默认 NIS_HDR_MODE_NONE，当前编译仅注入 MP_HDR_COMPATIBILITY。
- **源码依据**：[NIS.hlsl:1](<../../../src/Effects/NIS/NIS.hlsl#L1>)、[NIS.hlsl:19](<../../../src/Effects/NIS/NIS.hlsl#L19>)、[GroupBHdrRoutes.cpp:73](<../../../src/Magpie.Core/GroupBHdrRoutes.cpp#L73>)、[NIS_Scaler.hlsli:121](<../../../src/Effects/NIS/NIS_Scaler.hlsli#L121>)、[EffectCompiler.cpp:1631](<../../../src/Magpie.Core/EffectCompiler.cpp#L1631>)。

### NVSharpen

当前配置 ID：`NIS\NVSharpen`。

NVIDIA 风格同尺寸锐化。 适合在主要放大后补清晰度。

- **用途**：锐化细节 → 通用锐化。**输出尺寸**：保持输入尺寸。
- **变体差异**：同尺寸、单通道。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用；优先在 SDR 素材中比较；HDR 画面需先核对本报告列出的着色器模式疑点。。
- **管线位置**：链尾收尾。 建议放在「放大/超分、降噪」之后。 建议放在「CRT/显示调整」之前。 按最终尺寸补偿柔和感。
- **条件与取舍**：过强会放大噪声并产生光晕。
- **成本**：低到中。源码声明 1 个 pass；1 个可调参数。未进行 GPU 基准测试。
- **HDR 注意**：协议选择 FP16 HDR，但当前 NIS 着色器未设置 NIS_HDR_MODE，静态配置可能仍走 NONE 数学路径。 GroupBHdrRoutes 声明 DirectFP16；NIS_Scaler.hlsli 默认 NIS_HDR_MODE_NONE，当前编译仅注入 MP_HDR_COMPATIBILITY。
- **源码依据**：[NVSharpen.hlsl:3](<../../../src/Effects/NIS/NVSharpen.hlsl#L3>)、[NVSharpen.hlsl:23](<../../../src/Effects/NIS/NVSharpen.hlsl#L23>)、[GroupBHdrRoutes.cpp:73](<../../../src/Magpie.Core/GroupBHdrRoutes.cpp#L73>)、[NIS_Scaler.hlsli:121](<../../../src/Effects/NIS/NIS_Scaler.hlsli#L121>)、[EffectCompiler.cpp:1631](<../../../src/Magpie.Core/EffectCompiler.cpp#L1631>)。

## NNEDI3（10）

### NNEDI3 nns128 win8x4

当前配置 ID：`NNEDI3\NNEDI3_nns128_win8x4`。

NNEDI3 神经插值 2× 放大，适合线条、动画及需要方向性边缘插值的素材。 通过分方向处理完成固定 2× 放大；较大的神经元数和窗口需要更多运算。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：nns=128，窗口 8×4；固定 2×、2 个主要通道，未做本机质量/速度排名。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中。源码声明 2 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[NNEDI3_nns128_win8x4.hlsl:18](<../../../src/Effects/NNEDI3/NNEDI3_nns128_win8x4.hlsl#L18>)、[NNEDI3_nns128_win8x4.hlsl:32](<../../../src/Effects/NNEDI3/NNEDI3_nns128_win8x4.hlsl#L32>)、[EffectProtocolCatalogC.h:66](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L66>)。

### NNEDI3 nns128 win8x6

当前配置 ID：`NNEDI3\NNEDI3_nns128_win8x6`。

NNEDI3 神经插值 2× 放大，适合线条、动画及需要方向性边缘插值的素材。 通过分方向处理完成固定 2× 放大；较大的神经元数和窗口需要更多运算。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：nns=128，窗口 8×6；固定 2×、2 个主要通道，未做本机质量/速度排名。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中。源码声明 2 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[NNEDI3_nns128_win8x6.hlsl:18](<../../../src/Effects/NNEDI3/NNEDI3_nns128_win8x6.hlsl#L18>)、[NNEDI3_nns128_win8x6.hlsl:32](<../../../src/Effects/NNEDI3/NNEDI3_nns128_win8x6.hlsl#L32>)、[EffectProtocolCatalogC.h:66](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L66>)。

### NNEDI3 nns16 win8x4

当前配置 ID：`NNEDI3\NNEDI3_nns16_win8x4`。

NNEDI3 神经插值 2× 放大，适合线条、动画及需要方向性边缘插值的素材。 通过分方向处理完成固定 2× 放大；较大的神经元数和窗口需要更多运算。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：nns=16，窗口 8×4；固定 2×、2 个主要通道，未做本机质量/速度排名。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中。源码声明 2 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[NNEDI3_nns16_win8x4.hlsl:18](<../../../src/Effects/NNEDI3/NNEDI3_nns16_win8x4.hlsl#L18>)、[NNEDI3_nns16_win8x4.hlsl:32](<../../../src/Effects/NNEDI3/NNEDI3_nns16_win8x4.hlsl#L32>)、[EffectProtocolCatalogC.h:66](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L66>)。

### NNEDI3 nns16 win8x6

当前配置 ID：`NNEDI3\NNEDI3_nns16_win8x6`。

NNEDI3 神经插值 2× 放大，适合线条、动画及需要方向性边缘插值的素材。 通过分方向处理完成固定 2× 放大；较大的神经元数和窗口需要更多运算。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：nns=16，窗口 8×6；固定 2×、2 个主要通道，未做本机质量/速度排名。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中。源码声明 2 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[NNEDI3_nns16_win8x6.hlsl:18](<../../../src/Effects/NNEDI3/NNEDI3_nns16_win8x6.hlsl#L18>)、[NNEDI3_nns16_win8x6.hlsl:32](<../../../src/Effects/NNEDI3/NNEDI3_nns16_win8x6.hlsl#L32>)、[EffectProtocolCatalogC.h:66](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L66>)。

### NNEDI3 nns256 win8x4

当前配置 ID：`NNEDI3\NNEDI3_nns256_win8x4`。

NNEDI3 神经插值 2× 放大，适合线条、动画及需要方向性边缘插值的素材。 通过分方向处理完成固定 2× 放大；较大的神经元数和窗口需要更多运算。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：nns=256，窗口 8×4；固定 2×、2 个主要通道，未做本机质量/速度排名。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中。源码声明 2 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[NNEDI3_nns256_win8x4.hlsl:18](<../../../src/Effects/NNEDI3/NNEDI3_nns256_win8x4.hlsl#L18>)、[NNEDI3_nns256_win8x4.hlsl:32](<../../../src/Effects/NNEDI3/NNEDI3_nns256_win8x4.hlsl#L32>)、[EffectProtocolCatalogC.h:66](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L66>)。

### NNEDI3 nns256 win8x6

当前配置 ID：`NNEDI3\NNEDI3_nns256_win8x6`。

NNEDI3 神经插值 2× 放大，适合线条、动画及需要方向性边缘插值的素材。 通过分方向处理完成固定 2× 放大；较大的神经元数和窗口需要更多运算。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：nns=256，窗口 8×6；固定 2×、2 个主要通道，未做本机质量/速度排名。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中。源码声明 2 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[NNEDI3_nns256_win8x6.hlsl:18](<../../../src/Effects/NNEDI3/NNEDI3_nns256_win8x6.hlsl#L18>)、[NNEDI3_nns256_win8x6.hlsl:32](<../../../src/Effects/NNEDI3/NNEDI3_nns256_win8x6.hlsl#L32>)、[EffectProtocolCatalogC.h:66](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L66>)。

### NNEDI3 nns32 win8x4

当前配置 ID：`NNEDI3\NNEDI3_nns32_win8x4`。

NNEDI3 神经插值 2× 放大，适合线条、动画及需要方向性边缘插值的素材。 通过分方向处理完成固定 2× 放大；较大的神经元数和窗口需要更多运算。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：nns=32，窗口 8×4；固定 2×、2 个主要通道，未做本机质量/速度排名。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中。源码声明 2 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[NNEDI3_nns32_win8x4.hlsl:18](<../../../src/Effects/NNEDI3/NNEDI3_nns32_win8x4.hlsl#L18>)、[NNEDI3_nns32_win8x4.hlsl:32](<../../../src/Effects/NNEDI3/NNEDI3_nns32_win8x4.hlsl#L32>)、[EffectProtocolCatalogC.h:66](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L66>)。

### NNEDI3 nns32 win8x6

当前配置 ID：`NNEDI3\NNEDI3_nns32_win8x6`。

NNEDI3 神经插值 2× 放大，适合线条、动画及需要方向性边缘插值的素材。 通过分方向处理完成固定 2× 放大；较大的神经元数和窗口需要更多运算。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：nns=32，窗口 8×6；固定 2×、2 个主要通道，未做本机质量/速度排名。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中。源码声明 2 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[NNEDI3_nns32_win8x6.hlsl:18](<../../../src/Effects/NNEDI3/NNEDI3_nns32_win8x6.hlsl#L18>)、[NNEDI3_nns32_win8x6.hlsl:32](<../../../src/Effects/NNEDI3/NNEDI3_nns32_win8x6.hlsl#L32>)、[EffectProtocolCatalogC.h:66](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L66>)。

### NNEDI3 nns64 win8x4

当前配置 ID：`NNEDI3\NNEDI3_nns64_win8x4`。

NNEDI3 神经插值 2× 放大，适合线条、动画及需要方向性边缘插值的素材。 通过分方向处理完成固定 2× 放大；较大的神经元数和窗口需要更多运算。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：nns=64，窗口 8×4；固定 2×、2 个主要通道，未做本机质量/速度排名。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中。源码声明 2 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[NNEDI3_nns64_win8x4.hlsl:18](<../../../src/Effects/NNEDI3/NNEDI3_nns64_win8x4.hlsl#L18>)、[NNEDI3_nns64_win8x4.hlsl:32](<../../../src/Effects/NNEDI3/NNEDI3_nns64_win8x4.hlsl#L32>)、[EffectProtocolCatalogC.h:66](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L66>)。

### NNEDI3 nns64 win8x6

当前配置 ID：`NNEDI3\NNEDI3_nns64_win8x6`。

NNEDI3 神经插值 2× 放大，适合线条、动画及需要方向性边缘插值的素材。 通过分方向处理完成固定 2× 放大；较大的神经元数和窗口需要更多运算。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：nns=64，窗口 8×6；固定 2×、2 个主要通道，未做本机质量/速度排名。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中。源码声明 2 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[NNEDI3_nns64_win8x6.hlsl:18](<../../../src/Effects/NNEDI3/NNEDI3_nns64_win8x6.hlsl#L18>)、[NNEDI3_nns64_win8x6.hlsl:32](<../../../src/Effects/NNEDI3/NNEDI3_nns64_win8x6.hlsl#L32>)、[EffectProtocolCatalogC.h:66](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L66>)。

## Pixellate（1）

### Pixellate

当前配置 ID：`Pixel Art\Pixellate`。

像素画面积重采样，根据输出像素覆盖范围混合源像素。 适合像素画在非整数倍率下减少像素宽窄不均，并保持块面观感。

- **用途**：放大与超分 → 像素画。**输出尺寸**：使用效果组配置的输出尺寸。
- **变体差异**：输出尺寸由效果组决定；是像素画缩放器，不是马赛克滤镜。
- **推荐**：特定场景推荐。适合特定素材、风格或实验场景。 适用条件／尝试建议：先在代表性画面比较副作用。
- **管线位置**：主要像素画缩放步骤。 建议放在「按需添加的 CRT 风格／显示调整」之前。 先观察像素画缩放本身的结果，保留像素边界，再决定是否添加风格处理。
- **成本**：低到中。源码声明 1 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[Pixellate.hlsl:3](<../../../src/Effects/Pixel Art/Pixellate.hlsl#L3>)、[Pixellate.hlsl:11](<../../../src/Effects/Pixel Art/Pixellate.hlsl#L11>)、[EffectProtocolCatalogC.h:70](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L70>)。

## RAVU（26）

### RAVU 3x R2 RGB

当前配置 ID：`RAVU\RAVU_3x_R2_RGB`。

RAVU 3× 放大，用于一般视频、插画与边缘结构重建。 RGB 版按彩色信息处理；

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×3，高 ×3。
- **变体差异**：固定 3×，R2 / RGB，1 通道。半径与颜色变体需按素材比较。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：低到中。源码声明 1 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[RAVU_3x_R2_RGB.hlsl:18](<../../../src/Effects/RAVU/RAVU_3x_R2_RGB.hlsl#L18>)、[RAVU_3x_R2_RGB.hlsl:31](<../../../src/Effects/RAVU/RAVU_3x_R2_RGB.hlsl#L31>)、[EffectProtocolCatalogC.h:74](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L74>)。

### RAVU 3x R2

当前配置 ID：`RAVU\RAVU_3x_R2`。

RAVU 3× 放大，用于一般视频、插画与边缘结构重建。 非 RGB 版采用该文件的默认颜色路径。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×3，高 ×3。
- **变体差异**：固定 3×，R2，1 通道。半径与颜色变体需按素材比较。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：低到中。源码声明 1 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[RAVU_3x_R2.hlsl:18](<../../../src/Effects/RAVU/RAVU_3x_R2.hlsl#L18>)、[RAVU_3x_R2.hlsl:31](<../../../src/Effects/RAVU/RAVU_3x_R2.hlsl#L31>)、[EffectProtocolCatalogC.h:74](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L74>)。

### RAVU 3x R3 RGB

当前配置 ID：`RAVU\RAVU_3x_R3_RGB`。

RAVU 3× 放大，用于一般视频、插画与边缘结构重建。 RGB 版按彩色信息处理；

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×3，高 ×3。
- **变体差异**：固定 3×，R3 / RGB，1 通道。半径与颜色变体需按素材比较。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：低到中。源码声明 1 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[RAVU_3x_R3_RGB.hlsl:18](<../../../src/Effects/RAVU/RAVU_3x_R3_RGB.hlsl#L18>)、[RAVU_3x_R3_RGB.hlsl:31](<../../../src/Effects/RAVU/RAVU_3x_R3_RGB.hlsl#L31>)、[EffectProtocolCatalogC.h:74](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L74>)。

### RAVU 3x R3

当前配置 ID：`RAVU\RAVU_3x_R3`。

RAVU 3× 放大，用于一般视频、插画与边缘结构重建。 非 RGB 版采用该文件的默认颜色路径。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×3，高 ×3。
- **变体差异**：固定 3×，R3，1 通道。半径与颜色变体需按素材比较。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：低到中。源码声明 1 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[RAVU_3x_R3.hlsl:18](<../../../src/Effects/RAVU/RAVU_3x_R3.hlsl#L18>)、[RAVU_3x_R3.hlsl:31](<../../../src/Effects/RAVU/RAVU_3x_R3.hlsl#L31>)、[EffectProtocolCatalogC.h:74](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L74>)。

### RAVU 3x R4 RGB

当前配置 ID：`RAVU\RAVU_3x_R4_RGB`。

RAVU 3× 放大，用于一般视频、插画与边缘结构重建。 RGB 版按彩色信息处理；

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×3，高 ×3。
- **变体差异**：固定 3×，R4 / RGB，1 通道。半径与颜色变体需按素材比较。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：低到中。源码声明 1 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[RAVU_3x_R4_RGB.hlsl:18](<../../../src/Effects/RAVU/RAVU_3x_R4_RGB.hlsl#L18>)、[RAVU_3x_R4_RGB.hlsl:31](<../../../src/Effects/RAVU/RAVU_3x_R4_RGB.hlsl#L31>)、[EffectProtocolCatalogC.h:74](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L74>)。

### RAVU 3x R4

当前配置 ID：`RAVU\RAVU_3x_R4`。

RAVU 3× 放大，用于一般视频、插画与边缘结构重建。 非 RGB 版采用该文件的默认颜色路径。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×3，高 ×3。
- **变体差异**：固定 3×，R4，1 通道。半径与颜色变体需按素材比较。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：低到中。源码声明 1 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[RAVU_3x_R4.hlsl:18](<../../../src/Effects/RAVU/RAVU_3x_R4.hlsl#L18>)、[RAVU_3x_R4.hlsl:31](<../../../src/Effects/RAVU/RAVU_3x_R4.hlsl#L31>)、[EffectProtocolCatalogC.h:74](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L74>)。

### RAVU Lite AR R2

当前配置 ID：`RAVU\RAVU_Lite_AR_R2`。

RAVU 2× 放大，用于一般视频、插画与边缘结构重建。 Lite 版减少处理规模；AR 版加入抗振铃取向；非 RGB 版采用该文件的默认颜色路径。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：Lite 固定 2×，R2 / AR，1 通道。半径与颜色变体需按素材比较。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：低到中。源码声明 1 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[RAVU_Lite_AR_R2.hlsl:18](<../../../src/Effects/RAVU/RAVU_Lite_AR_R2.hlsl#L18>)、[RAVU_Lite_AR_R2.hlsl:31](<../../../src/Effects/RAVU/RAVU_Lite_AR_R2.hlsl#L31>)、[EffectProtocolCatalogC.h:74](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L74>)。

### RAVU Lite AR R3

当前配置 ID：`RAVU\RAVU_Lite_AR_R3`。

RAVU 2× 放大，用于一般视频、插画与边缘结构重建。 Lite 版减少处理规模；AR 版加入抗振铃取向；非 RGB 版采用该文件的默认颜色路径。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：Lite 固定 2×，R3 / AR，1 通道。半径与颜色变体需按素材比较。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：低到中。源码声明 1 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[RAVU_Lite_AR_R3.hlsl:18](<../../../src/Effects/RAVU/RAVU_Lite_AR_R3.hlsl#L18>)、[RAVU_Lite_AR_R3.hlsl:31](<../../../src/Effects/RAVU/RAVU_Lite_AR_R3.hlsl#L31>)、[EffectProtocolCatalogC.h:74](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L74>)。

### RAVU Lite AR R4

当前配置 ID：`RAVU\RAVU_Lite_AR_R4`。

RAVU 2× 放大，用于一般视频、插画与边缘结构重建。 Lite 版减少处理规模；AR 版加入抗振铃取向；非 RGB 版采用该文件的默认颜色路径。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：Lite 固定 2×，R4 / AR，1 通道。半径与颜色变体需按素材比较。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：低到中。源码声明 1 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[RAVU_Lite_AR_R4.hlsl:18](<../../../src/Effects/RAVU/RAVU_Lite_AR_R4.hlsl#L18>)、[RAVU_Lite_AR_R4.hlsl:31](<../../../src/Effects/RAVU/RAVU_Lite_AR_R4.hlsl#L31>)、[EffectProtocolCatalogC.h:74](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L74>)。

### RAVU Lite R2

当前配置 ID：`RAVU\RAVU_Lite_R2`。

RAVU 2× 放大，用于一般视频、插画与边缘结构重建。 Lite 版减少处理规模；非 RGB 版采用该文件的默认颜色路径。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：Lite 固定 2×，R2，1 通道。半径与颜色变体需按素材比较。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：低到中。源码声明 1 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[RAVU_Lite_R2.hlsl:18](<../../../src/Effects/RAVU/RAVU_Lite_R2.hlsl#L18>)、[RAVU_Lite_R2.hlsl:31](<../../../src/Effects/RAVU/RAVU_Lite_R2.hlsl#L31>)、[EffectProtocolCatalogC.h:74](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L74>)。

### RAVU Lite R3

当前配置 ID：`RAVU\RAVU_Lite_R3`。

RAVU 2× 放大，用于一般视频、插画与边缘结构重建。 Lite 版减少处理规模；非 RGB 版采用该文件的默认颜色路径。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：Lite 固定 2×，R3，1 通道。半径与颜色变体需按素材比较。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：低到中。源码声明 1 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[RAVU_Lite_R3.hlsl:18](<../../../src/Effects/RAVU/RAVU_Lite_R3.hlsl#L18>)、[RAVU_Lite_R3.hlsl:31](<../../../src/Effects/RAVU/RAVU_Lite_R3.hlsl#L31>)、[EffectProtocolCatalogC.h:74](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L74>)。

### RAVU Lite R4

当前配置 ID：`RAVU\RAVU_Lite_R4`。

RAVU 2× 放大，用于一般视频、插画与边缘结构重建。 Lite 版减少处理规模；非 RGB 版采用该文件的默认颜色路径。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：Lite 固定 2×，R4，1 通道。半径与颜色变体需按素材比较。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：低到中。源码声明 1 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[RAVU_Lite_R4.hlsl:18](<../../../src/Effects/RAVU/RAVU_Lite_R4.hlsl#L18>)、[RAVU_Lite_R4.hlsl:31](<../../../src/Effects/RAVU/RAVU_Lite_R4.hlsl#L31>)、[EffectProtocolCatalogC.h:74](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L74>)。

### RAVU R2 RGB

当前配置 ID：`RAVU\RAVU_R2_RGB`。

RAVU 2× 放大，用于一般视频、插画与边缘结构重建。 RGB 版按彩色信息处理；

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：固定 2×，R2 / RGB，2 通道。半径与颜色变体需按素材比较。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **成本**：中。源码声明 2 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[RAVU_R2_RGB.hlsl:18](<../../../src/Effects/RAVU/RAVU_R2_RGB.hlsl#L18>)、[RAVU_R2_RGB.hlsl:31](<../../../src/Effects/RAVU/RAVU_R2_RGB.hlsl#L31>)、[EffectProtocolCatalogC.h:74](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L74>)。

### RAVU R2

当前配置 ID：`RAVU\RAVU_R2`。

RAVU 2× 放大，用于一般视频、插画与边缘结构重建。 非 RGB 版采用该文件的默认颜色路径。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：固定 2×，R2，2 通道。半径与颜色变体需按素材比较。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中。源码声明 2 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[RAVU_R2.hlsl:18](<../../../src/Effects/RAVU/RAVU_R2.hlsl#L18>)、[RAVU_R2.hlsl:31](<../../../src/Effects/RAVU/RAVU_R2.hlsl#L31>)、[EffectProtocolCatalogC.h:74](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L74>)。

### RAVU R3 RGB

当前配置 ID：`RAVU\RAVU_R3_RGB`。

RAVU 2× 放大，用于一般视频、插画与边缘结构重建。 RGB 版按彩色信息处理；

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：固定 2×，R3 / RGB，2 通道。半径与颜色变体需按素材比较。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **成本**：中。源码声明 2 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[RAVU_R3_RGB.hlsl:18](<../../../src/Effects/RAVU/RAVU_R3_RGB.hlsl#L18>)、[RAVU_R3_RGB.hlsl:31](<../../../src/Effects/RAVU/RAVU_R3_RGB.hlsl#L31>)、[EffectProtocolCatalogC.h:74](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L74>)。

### RAVU R3

当前配置 ID：`RAVU\RAVU_R3`。

RAVU 2× 放大，用于一般视频、插画与边缘结构重建。 非 RGB 版采用该文件的默认颜色路径。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：固定 2×，R3，2 通道。半径与颜色变体需按素材比较。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中。源码声明 2 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[RAVU_R3.hlsl:18](<../../../src/Effects/RAVU/RAVU_R3.hlsl#L18>)、[RAVU_R3.hlsl:31](<../../../src/Effects/RAVU/RAVU_R3.hlsl#L31>)、[EffectProtocolCatalogC.h:74](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L74>)。

### RAVU R4 RGB

当前配置 ID：`RAVU\RAVU_R4_RGB`。

RAVU 2× 放大，用于一般视频、插画与边缘结构重建。 RGB 版按彩色信息处理；

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：固定 2×，R4 / RGB，2 通道。半径与颜色变体需按素材比较。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **成本**：中。源码声明 2 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[RAVU_R4_RGB.hlsl:18](<../../../src/Effects/RAVU/RAVU_R4_RGB.hlsl#L18>)、[RAVU_R4_RGB.hlsl:31](<../../../src/Effects/RAVU/RAVU_R4_RGB.hlsl#L31>)、[EffectProtocolCatalogC.h:74](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L74>)。

### RAVU R4

当前配置 ID：`RAVU\RAVU_R4`。

RAVU 2× 放大，用于一般视频、插画与边缘结构重建。 非 RGB 版采用该文件的默认颜色路径。

- **用途**：放大与超分 → 通用。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：固定 2×，R4，2 通道。半径与颜色变体需按素材比较。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：中。源码声明 2 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[RAVU_R4.hlsl:18](<../../../src/Effects/RAVU/RAVU_R4.hlsl#L18>)、[RAVU_R4.hlsl:31](<../../../src/Effects/RAVU/RAVU_R4.hlsl#L31>)、[EffectProtocolCatalogC.h:74](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L74>)。

### RAVU Zoom AR R2 RGB

当前配置 ID：`RAVU\RAVU_Zoom_AR_R2_RGB`。

RAVU 任意输出尺寸 放大，用于一般视频、插画与边缘结构重建。 AR 版加入抗振铃取向；RGB 版按彩色信息处理；

- **用途**：放大与超分 → 通用。**输出尺寸**：使用效果组配置的输出尺寸。
- **变体差异**：Zoom 任意倍率，R2 / AR / RGB，1 通道。半径与颜色变体需按素材比较。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：Zoom 变体只支持放大；Zoom 变体只用于放大。
- **成本**：低到中。源码声明 1 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[RAVU_Zoom_AR_R2_RGB.hlsl:18](<../../../src/Effects/RAVU/RAVU_Zoom_AR_R2_RGB.hlsl#L18>)、[RAVU_Zoom_AR_R2_RGB.hlsl:31](<../../../src/Effects/RAVU/RAVU_Zoom_AR_R2_RGB.hlsl#L31>)、[EffectProtocolCatalogC.h:74](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L74>)。

### RAVU Zoom AR R2

当前配置 ID：`RAVU\RAVU_Zoom_AR_R2`。

RAVU 任意输出尺寸 放大，用于一般视频、插画与边缘结构重建。 AR 版加入抗振铃取向；非 RGB 版采用该文件的默认颜色路径。

- **用途**：放大与超分 → 通用。**输出尺寸**：使用效果组配置的输出尺寸。
- **变体差异**：Zoom 任意倍率，R2 / AR，1 通道。半径与颜色变体需按素材比较。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：Zoom 变体只支持放大；Zoom 变体只用于放大。
- **成本**：低到中。源码声明 1 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[RAVU_Zoom_AR_R2.hlsl:18](<../../../src/Effects/RAVU/RAVU_Zoom_AR_R2.hlsl#L18>)、[RAVU_Zoom_AR_R2.hlsl:31](<../../../src/Effects/RAVU/RAVU_Zoom_AR_R2.hlsl#L31>)、[EffectProtocolCatalogC.h:74](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L74>)。

### RAVU Zoom AR R3 RGB

当前配置 ID：`RAVU\RAVU_Zoom_AR_R3_RGB`。

RAVU 任意输出尺寸 放大，用于一般视频、插画与边缘结构重建。 AR 版加入抗振铃取向；RGB 版按彩色信息处理；

- **用途**：放大与超分 → 通用。**输出尺寸**：使用效果组配置的输出尺寸。
- **变体差异**：Zoom 任意倍率，R3 / AR / RGB，1 通道。半径与颜色变体需按素材比较。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：Zoom 变体只支持放大；Zoom 变体只用于放大。
- **成本**：低到中。源码声明 1 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[RAVU_Zoom_AR_R3_RGB.hlsl:18](<../../../src/Effects/RAVU/RAVU_Zoom_AR_R3_RGB.hlsl#L18>)、[RAVU_Zoom_AR_R3_RGB.hlsl:31](<../../../src/Effects/RAVU/RAVU_Zoom_AR_R3_RGB.hlsl#L31>)、[EffectProtocolCatalogC.h:74](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L74>)。

### RAVU Zoom AR R3

当前配置 ID：`RAVU\RAVU_Zoom_AR_R3`。

RAVU 任意输出尺寸 放大，用于一般视频、插画与边缘结构重建。 AR 版加入抗振铃取向；非 RGB 版采用该文件的默认颜色路径。

- **用途**：放大与超分 → 通用。**输出尺寸**：使用效果组配置的输出尺寸。
- **变体差异**：Zoom 任意倍率，R3 / AR，1 通道。半径与颜色变体需按素材比较。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：Zoom 变体只支持放大；Zoom 变体只用于放大。
- **成本**：低到中。源码声明 1 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[RAVU_Zoom_AR_R3.hlsl:18](<../../../src/Effects/RAVU/RAVU_Zoom_AR_R3.hlsl#L18>)、[RAVU_Zoom_AR_R3.hlsl:31](<../../../src/Effects/RAVU/RAVU_Zoom_AR_R3.hlsl#L31>)、[EffectProtocolCatalogC.h:74](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L74>)。

### RAVU Zoom R2 RGB

当前配置 ID：`RAVU\RAVU_Zoom_R2_RGB`。

RAVU 任意输出尺寸 放大，用于一般视频、插画与边缘结构重建。 RGB 版按彩色信息处理；

- **用途**：放大与超分 → 通用。**输出尺寸**：使用效果组配置的输出尺寸。
- **变体差异**：Zoom 任意倍率，R2 / RGB，1 通道。半径与颜色变体需按素材比较。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：Zoom 变体只支持放大；Zoom 变体只用于放大。
- **成本**：低到中。源码声明 1 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[RAVU_Zoom_R2_RGB.hlsl:18](<../../../src/Effects/RAVU/RAVU_Zoom_R2_RGB.hlsl#L18>)、[RAVU_Zoom_R2_RGB.hlsl:31](<../../../src/Effects/RAVU/RAVU_Zoom_R2_RGB.hlsl#L31>)、[EffectProtocolCatalogC.h:74](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L74>)。

### RAVU Zoom R2

当前配置 ID：`RAVU\RAVU_Zoom_R2`。

RAVU 任意输出尺寸 放大，用于一般视频、插画与边缘结构重建。 非 RGB 版采用该文件的默认颜色路径。

- **用途**：放大与超分 → 通用。**输出尺寸**：使用效果组配置的输出尺寸。
- **变体差异**：Zoom 任意倍率，R2，1 通道。半径与颜色变体需按素材比较。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：Zoom 变体只支持放大；Zoom 变体只用于放大。
- **成本**：低到中。源码声明 1 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[RAVU_Zoom_R2.hlsl:18](<../../../src/Effects/RAVU/RAVU_Zoom_R2.hlsl#L18>)、[RAVU_Zoom_R2.hlsl:31](<../../../src/Effects/RAVU/RAVU_Zoom_R2.hlsl#L31>)、[EffectProtocolCatalogC.h:74](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L74>)。

### RAVU Zoom R3 RGB

当前配置 ID：`RAVU\RAVU_Zoom_R3_RGB`。

RAVU 任意输出尺寸 放大，用于一般视频、插画与边缘结构重建。 RGB 版按彩色信息处理；

- **用途**：放大与超分 → 通用。**输出尺寸**：使用效果组配置的输出尺寸。
- **变体差异**：Zoom 任意倍率，R3 / RGB，1 通道。半径与颜色变体需按素材比较。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：Zoom 变体只支持放大；Zoom 变体只用于放大。
- **成本**：低到中。源码声明 1 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[RAVU_Zoom_R3_RGB.hlsl:18](<../../../src/Effects/RAVU/RAVU_Zoom_R3_RGB.hlsl#L18>)、[RAVU_Zoom_R3_RGB.hlsl:31](<../../../src/Effects/RAVU/RAVU_Zoom_R3_RGB.hlsl#L31>)、[EffectProtocolCatalogC.h:74](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L74>)。

### RAVU Zoom R3

当前配置 ID：`RAVU\RAVU_Zoom_R3`。

RAVU 任意输出尺寸 放大，用于一般视频、插画与边缘结构重建。 非 RGB 版采用该文件的默认颜色路径。

- **用途**：放大与超分 → 通用。**输出尺寸**：使用效果组配置的输出尺寸。
- **变体差异**：Zoom 任意倍率，R3，1 通道。半径与颜色变体需按素材比较。
- **推荐**：特定场景推荐。该变体用途明确且当前 pass 数不高，可按目标素材比较模型取向。 适用条件／尝试建议：按视觉小说/动画/一般图像的实际类型选择；检查固定倍率约束。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：Zoom 变体只支持放大；Zoom 变体只用于放大。
- **成本**：低到中。源码声明 1 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[RAVU_Zoom_R3.hlsl:18](<../../../src/Effects/RAVU/RAVU_Zoom_R3.hlsl#L18>)、[RAVU_Zoom_R3.hlsl:31](<../../../src/Effects/RAVU/RAVU_Zoom_R3.hlsl#L31>)、[EffectProtocolCatalogC.h:74](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L74>)。

## RTXVideo（8）

### RTXVideo_Denoise · 强度：高

当前配置 ID：`RTXVideo\RTXVideo_Denoise_High`。

调用 RTX Video 原生后端做同尺寸视频降噪。 适合压缩噪声或颗粒明显的视频；HLSL 仅是占位，运行时由原生 VFX 后端替换。

- **用途**：清理修复 → 降噪。**输出尺寸**：保持输入尺寸。
- **变体差异**：High 档；映射到后端质量级别 10。档位成本与效果需在目标 GPU/驱动上验证。
- **推荐**：进阶／实验。需要 NVIDIA Video Effects 运行库与受支持硬件，质量档位应在目标机器比较。 适用条件／尝试建议：确认原生后端初始化成功；按视频噪声或缩放目标选择 Denoise/VSR。
- **管线位置**：放大前。 建议放在「放大/超分、锐化」之前。 先清理输入可减少后续放大噪声。
- **条件与取舍**：强度过高会损失纹理；受支持的 NVIDIA RTX GPU/驱动；可加载 NVIDIA Video Effects 运行库。
- **成本**：硬件/运行时相关。实际成本来自原生 SDK 调用、资源传输与同步；HLSL 占位 pass 数无法代表这些开销。本轮未进行 GPU 基准测试。
- **HDR 注意**：当前 RTX Video 边界是 SDR/U8 兼容路径，HDR 高光会经过映射与量化。 EffectProtocolCatalogC 明确选择 BGRA8/U8 SDR 路径。
- **源码依据**：[RTXVideo_Denoise_High.hlsl:2](<../../../src/Effects/RTXVideo/RTXVideo_Denoise_High.hlsl#L2>)、[RTXVideo_Denoise_High.hlsl:11](<../../../src/Effects/RTXVideo/RTXVideo_Denoise_High.hlsl#L11>)、[NativeEffectBackendFactory.cpp:239](<../../../src/Magpie.Core/NativeEffectBackendFactory.cpp#L239>)、[EffectProtocolCatalogC.h:88](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L88>)。

### RTXVideo_Denoise · 强度：低

当前配置 ID：`RTXVideo\RTXVideo_Denoise_Low`。

调用 RTX Video 原生后端做同尺寸视频降噪。 适合压缩噪声或颗粒明显的视频；HLSL 仅是占位，运行时由原生 VFX 后端替换。

- **用途**：清理修复 → 降噪。**输出尺寸**：保持输入尺寸。
- **变体差异**：Low 档；映射到后端质量级别 8。档位成本与效果需在目标 GPU/驱动上验证。
- **推荐**：进阶／实验。需要 NVIDIA Video Effects 运行库与受支持硬件，质量档位应在目标机器比较。 适用条件／尝试建议：确认原生后端初始化成功；按视频噪声或缩放目标选择 Denoise/VSR。
- **管线位置**：放大前。 建议放在「放大/超分、锐化」之前。 先清理输入可减少后续放大噪声。
- **条件与取舍**：强度过高会损失纹理；受支持的 NVIDIA RTX GPU/驱动；可加载 NVIDIA Video Effects 运行库。
- **成本**：硬件/运行时相关。实际成本来自原生 SDK 调用、资源传输与同步；HLSL 占位 pass 数无法代表这些开销。本轮未进行 GPU 基准测试。
- **HDR 注意**：当前 RTX Video 边界是 SDR/U8 兼容路径，HDR 高光会经过映射与量化。 EffectProtocolCatalogC 明确选择 BGRA8/U8 SDR 路径。
- **源码依据**：[RTXVideo_Denoise_Low.hlsl:2](<../../../src/Effects/RTXVideo/RTXVideo_Denoise_Low.hlsl#L2>)、[RTXVideo_Denoise_Low.hlsl:11](<../../../src/Effects/RTXVideo/RTXVideo_Denoise_Low.hlsl#L11>)、[NativeEffectBackendFactory.cpp:239](<../../../src/Magpie.Core/NativeEffectBackendFactory.cpp#L239>)、[EffectProtocolCatalogC.h:88](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L88>)。

### RTXVideo_Denoise · 强度：中

当前配置 ID：`RTXVideo\RTXVideo_Denoise_Medium`。

调用 RTX Video 原生后端做同尺寸视频降噪。 适合压缩噪声或颗粒明显的视频；HLSL 仅是占位，运行时由原生 VFX 后端替换。

- **用途**：清理修复 → 降噪。**输出尺寸**：保持输入尺寸。
- **变体差异**：Medium 档；映射到后端质量级别 9。档位成本与效果需在目标 GPU/驱动上验证。
- **推荐**：进阶／实验。需要 NVIDIA Video Effects 运行库与受支持硬件，质量档位应在目标机器比较。 适用条件／尝试建议：确认原生后端初始化成功；按视频噪声或缩放目标选择 Denoise/VSR。
- **管线位置**：放大前。 建议放在「放大/超分、锐化」之前。 先清理输入可减少后续放大噪声。
- **条件与取舍**：强度过高会损失纹理；受支持的 NVIDIA RTX GPU/驱动；可加载 NVIDIA Video Effects 运行库。
- **成本**：硬件/运行时相关。实际成本来自原生 SDK 调用、资源传输与同步；HLSL 占位 pass 数无法代表这些开销。本轮未进行 GPU 基准测试。
- **HDR 注意**：当前 RTX Video 边界是 SDR/U8 兼容路径，HDR 高光会经过映射与量化。 EffectProtocolCatalogC 明确选择 BGRA8/U8 SDR 路径。
- **源码依据**：[RTXVideo_Denoise_Medium.hlsl:2](<../../../src/Effects/RTXVideo/RTXVideo_Denoise_Medium.hlsl#L2>)、[RTXVideo_Denoise_Medium.hlsl:11](<../../../src/Effects/RTXVideo/RTXVideo_Denoise_Medium.hlsl#L11>)、[NativeEffectBackendFactory.cpp:239](<../../../src/Magpie.Core/NativeEffectBackendFactory.cpp#L239>)、[EffectProtocolCatalogC.h:88](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L88>)。

### RTXVideo_Denoise · 强度：极高

当前配置 ID：`RTXVideo\RTXVideo_Denoise_Ultra`。

调用 RTX Video 原生后端做同尺寸视频降噪。 适合压缩噪声或颗粒明显的视频；HLSL 仅是占位，运行时由原生 VFX 后端替换。

- **用途**：清理修复 → 降噪。**输出尺寸**：保持输入尺寸。
- **变体差异**：Ultra 档；映射到后端质量级别 11。档位成本与效果需在目标 GPU/驱动上验证。
- **推荐**：进阶／实验。需要 NVIDIA Video Effects 运行库与受支持硬件，质量档位应在目标机器比较。 适用条件／尝试建议：确认原生后端初始化成功；按视频噪声或缩放目标选择 Denoise/VSR。
- **管线位置**：放大前。 建议放在「放大/超分、锐化」之前。 先清理输入可减少后续放大噪声。
- **条件与取舍**：强度过高会损失纹理；受支持的 NVIDIA RTX GPU/驱动；可加载 NVIDIA Video Effects 运行库。
- **成本**：硬件/运行时相关。实际成本来自原生 SDK 调用、资源传输与同步；HLSL 占位 pass 数无法代表这些开销。本轮未进行 GPU 基准测试。
- **HDR 注意**：当前 RTX Video 边界是 SDR/U8 兼容路径，HDR 高光会经过映射与量化。 EffectProtocolCatalogC 明确选择 BGRA8/U8 SDR 路径。
- **源码依据**：[RTXVideo_Denoise_Ultra.hlsl:2](<../../../src/Effects/RTXVideo/RTXVideo_Denoise_Ultra.hlsl#L2>)、[RTXVideo_Denoise_Ultra.hlsl:11](<../../../src/Effects/RTXVideo/RTXVideo_Denoise_Ultra.hlsl#L11>)、[NativeEffectBackendFactory.cpp:239](<../../../src/Magpie.Core/NativeEffectBackendFactory.cpp#L239>)、[EffectProtocolCatalogC.h:88](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L88>)。

### RTXVideo_VSR · 强度：高

当前配置 ID：`RTXVideo\RTXVideo_VSR_High`。

调用 RTX Video 原生后端做视频超分。 适合受支持 NVIDIA 环境的视频与串流内容；HLSL 仅是占位，运行时由原生 VFX 后端替换。

- **用途**：放大与超分 → 通用。**输出尺寸**：使用效果组配置的输出尺寸。
- **变体差异**：High 档；映射到后端质量级别 3。档位成本与效果需在目标 GPU/驱动上验证。
- **推荐**：进阶／实验。需要 NVIDIA Video Effects 运行库与受支持硬件，质量档位应在目标机器比较。 适用条件／尝试建议：确认原生后端初始化成功；按视频噪声或缩放目标选择 Denoise/VSR。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：受支持的 NVIDIA RTX GPU/驱动；可加载 NVIDIA Video Effects 运行库。
- **成本**：硬件/运行时相关。实际成本来自原生 SDK 调用、资源传输与同步；HLSL 占位 pass 数无法代表这些开销。本轮未进行 GPU 基准测试。
- **HDR 注意**：当前 RTX Video 边界是 SDR/U8 兼容路径，HDR 高光会经过映射与量化。 EffectProtocolCatalogC 明确选择 BGRA8/U8 SDR 路径。
- **源码依据**：[RTXVideo_VSR_High.hlsl:2](<../../../src/Effects/RTXVideo/RTXVideo_VSR_High.hlsl#L2>)、[RTXVideo_VSR_High.hlsl:9](<../../../src/Effects/RTXVideo/RTXVideo_VSR_High.hlsl#L9>)、[NativeEffectBackendFactory.cpp:239](<../../../src/Magpie.Core/NativeEffectBackendFactory.cpp#L239>)、[EffectProtocolCatalogC.h:78](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L78>)。

### RTXVideo_VSR · 强度：低

当前配置 ID：`RTXVideo\RTXVideo_VSR_Low`。

调用 RTX Video 原生后端做视频超分。 适合受支持 NVIDIA 环境的视频与串流内容；HLSL 仅是占位，运行时由原生 VFX 后端替换。

- **用途**：放大与超分 → 通用。**输出尺寸**：使用效果组配置的输出尺寸。
- **变体差异**：Low 档；映射到后端质量级别 1。档位成本与效果需在目标 GPU/驱动上验证。
- **推荐**：进阶／实验。需要 NVIDIA Video Effects 运行库与受支持硬件，质量档位应在目标机器比较。 适用条件／尝试建议：确认原生后端初始化成功；按视频噪声或缩放目标选择 Denoise/VSR。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：受支持的 NVIDIA RTX GPU/驱动；可加载 NVIDIA Video Effects 运行库。
- **成本**：硬件/运行时相关。实际成本来自原生 SDK 调用、资源传输与同步；HLSL 占位 pass 数无法代表这些开销。本轮未进行 GPU 基准测试。
- **HDR 注意**：当前 RTX Video 边界是 SDR/U8 兼容路径，HDR 高光会经过映射与量化。 EffectProtocolCatalogC 明确选择 BGRA8/U8 SDR 路径。
- **源码依据**：[RTXVideo_VSR_Low.hlsl:2](<../../../src/Effects/RTXVideo/RTXVideo_VSR_Low.hlsl#L2>)、[RTXVideo_VSR_Low.hlsl:9](<../../../src/Effects/RTXVideo/RTXVideo_VSR_Low.hlsl#L9>)、[NativeEffectBackendFactory.cpp:239](<../../../src/Magpie.Core/NativeEffectBackendFactory.cpp#L239>)、[EffectProtocolCatalogC.h:78](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L78>)。

### RTXVideo_VSR · 强度：中

当前配置 ID：`RTXVideo\RTXVideo_VSR_Medium`。

调用 RTX Video 原生后端做视频超分。 适合受支持 NVIDIA 环境的视频与串流内容；HLSL 仅是占位，运行时由原生 VFX 后端替换。

- **用途**：放大与超分 → 通用。**输出尺寸**：使用效果组配置的输出尺寸。
- **变体差异**：Medium 档；映射到后端质量级别 2。档位成本与效果需在目标 GPU/驱动上验证。
- **推荐**：进阶／实验。需要 NVIDIA Video Effects 运行库与受支持硬件，质量档位应在目标机器比较。 适用条件／尝试建议：确认原生后端初始化成功；按视频噪声或缩放目标选择 Denoise/VSR。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：受支持的 NVIDIA RTX GPU/驱动；可加载 NVIDIA Video Effects 运行库。
- **成本**：硬件/运行时相关。实际成本来自原生 SDK 调用、资源传输与同步；HLSL 占位 pass 数无法代表这些开销。本轮未进行 GPU 基准测试。
- **HDR 注意**：当前 RTX Video 边界是 SDR/U8 兼容路径，HDR 高光会经过映射与量化。 EffectProtocolCatalogC 明确选择 BGRA8/U8 SDR 路径。
- **源码依据**：[RTXVideo_VSR_Medium.hlsl:2](<../../../src/Effects/RTXVideo/RTXVideo_VSR_Medium.hlsl#L2>)、[RTXVideo_VSR_Medium.hlsl:9](<../../../src/Effects/RTXVideo/RTXVideo_VSR_Medium.hlsl#L9>)、[NativeEffectBackendFactory.cpp:239](<../../../src/Magpie.Core/NativeEffectBackendFactory.cpp#L239>)、[EffectProtocolCatalogC.h:78](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L78>)。

### RTXVideo_VSR · 强度：极高

当前配置 ID：`RTXVideo\RTXVideo_VSR_Ultra`。

调用 RTX Video 原生后端做视频超分。 适合受支持 NVIDIA 环境的视频与串流内容；HLSL 仅是占位，运行时由原生 VFX 后端替换。

- **用途**：放大与超分 → 通用。**输出尺寸**：使用效果组配置的输出尺寸。
- **变体差异**：Ultra 档；映射到后端质量级别 4。档位成本与效果需在目标 GPU/驱动上验证。
- **推荐**：进阶／实验。需要 NVIDIA Video Effects 运行库与受支持硬件，质量档位应在目标机器比较。 适用条件／尝试建议：确认原生后端初始化成功；按视频噪声或缩放目标选择 Denoise/VSR。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：受支持的 NVIDIA RTX GPU/驱动；可加载 NVIDIA Video Effects 运行库。
- **成本**：硬件/运行时相关。实际成本来自原生 SDK 调用、资源传输与同步；HLSL 占位 pass 数无法代表这些开销。本轮未进行 GPU 基准测试。
- **HDR 注意**：当前 RTX Video 边界是 SDR/U8 兼容路径，HDR 高光会经过映射与量化。 EffectProtocolCatalogC 明确选择 BGRA8/U8 SDR 路径。
- **源码依据**：[RTXVideo_VSR_Ultra.hlsl:2](<../../../src/Effects/RTXVideo/RTXVideo_VSR_Ultra.hlsl#L2>)、[RTXVideo_VSR_Ultra.hlsl:9](<../../../src/Effects/RTXVideo/RTXVideo_VSR_Ultra.hlsl#L9>)、[NativeEffectBackendFactory.cpp:239](<../../../src/Magpie.Core/NativeEffectBackendFactory.cpp#L239>)、[EffectProtocolCatalogC.h:78](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L78>)。

## SGSR（1）

### SGSR

当前配置 ID：`SGSR`。

单通道空间放大与细节重建，适合先做低成本通用尝试。 用作轻量空间放大的候选，实际锐度与伪影需按素材比较。

- **用途**：放大与超分 → 通用。**输出尺寸**：使用效果组配置的输出尺寸。
- **变体差异**：输出尺寸由效果组决定；源码为像素着色器，质量与代价未实测。
- **推荐**：优先尝试。通用且链路职责清楚，便于建立画质/性能基线。 适用条件／尝试建议：按内容类型比较锐度与振铃。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **成本**：低到中。源码声明 1 个 pass；2 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[SGSR.hlsl:4](<../../../src/Effects/SGSR.hlsl#L4>)、[SGSR.hlsl:27](<../../../src/Effects/SGSR.hlsl#L27>)、[HdrEffectBoundary.cpp:8](<../../../src/Magpie.Core/HdrEffectBoundary.cpp#L8>)。

## SharpBilinear（1）

### SharpBilinear

当前配置 ID：`Pixel Art\SharpBilinear`。

像素画友好的锐利双线性缩放，减少非整数倍率下的不均匀像素。 适合像素画无法使用严格整数倍显示时。

- **用途**：放大与超分 → 像素画。**输出尺寸**：使用效果组配置的输出尺寸。
- **变体差异**：输出尺寸由效果组决定、单通道。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：主要像素画缩放步骤。 建议放在「按需添加的 CRT 风格／显示调整」之前。 先观察像素画缩放本身的结果，保留像素边界，再决定是否添加风格处理。
- **成本**：低到中。源码声明 1 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[SharpBilinear.hlsl:3](<../../../src/Effects/Pixel Art/SharpBilinear.hlsl#L3>)、[SharpBilinear.hlsl:11](<../../../src/Effects/Pixel Art/SharpBilinear.hlsl#L11>)、[EffectProtocolCatalogC.h:70](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L70>)。

## Sharpen（4）

### AdaptiveSharpen

当前配置 ID：`Sharpen\AdaptiveSharpen`。

自适应锐化，根据局部边缘强弱增强细节。 适合一般视频与游戏收尾，强度过高会强调噪声。

- **用途**：锐化细节 → 通用锐化。**输出尺寸**：保持输入尺寸。
- **变体差异**：AdaptiveSharpen 算法，1 通道；参数尺度不能跨算法直接比较。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：链尾收尾。 建议放在「放大/超分、降噪」之后。 建议放在「CRT/显示调整」之前。 按最终尺寸补偿柔和感。
- **条件与取舍**：过强会放大噪声并产生光晕。
- **成本**：低到中。源码声明 1 个 pass；1 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[AdaptiveSharpen.hlsl:8](<../../../src/Effects/Sharpen/AdaptiveSharpen.hlsl#L8>)、[AdaptiveSharpen.hlsl:29](<../../../src/Effects/Sharpen/AdaptiveSharpen.hlsl#L29>)、[EffectProtocolCatalogC.h:97](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L97>)。

### FineSharp

当前配置 ID：`Sharpen\FineSharp`。

多阶段细节增强与反光晕控制。 参数更细、5 通道，适合愿意调节细节/光晕平衡的用户。

- **用途**：锐化细节 → 通用锐化。**输出尺寸**：保持输入尺寸。
- **变体差异**：FineSharp 算法，5 通道；参数尺度不能跨算法直接比较。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：链尾收尾。 建议放在「放大/超分、降噪」之后。 建议放在「CRT/显示调整」之前。 按最终尺寸补偿柔和感。
- **条件与取舍**：过强会放大噪声并产生光晕。
- **成本**：中到高。源码声明 5 个 pass；4 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[FineSharp.hlsl:8](<../../../src/Effects/Sharpen/FineSharp.hlsl#L8>)、[FineSharp.hlsl:55](<../../../src/Effects/Sharpen/FineSharp.hlsl#L55>)、[EffectProtocolCatalogC.h:97](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L97>)。

### LCAS

当前配置 ID：`Sharpen\LCAS`。

LCAS 缩放与局部对比增强。 输出尺寸由效果组决定，承担主要缩放步骤而非纯同尺寸收尾。

- **用途**：放大与超分 → 通用。**输出尺寸**：使用效果组配置的输出尺寸。
- **变体差异**：LCAS 算法，1 通道；参数尺度不能跨算法直接比较。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **成本**：低到中。源码声明 1 个 pass；1 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[LCAS.hlsl:3](<../../../src/Effects/Sharpen/LCAS.hlsl#L3>)、[LCAS.hlsl:18](<../../../src/Effects/Sharpen/LCAS.hlsl#L18>)、[EffectProtocolCatalogC.h:97](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L97>)。

### LumaSharpen

当前配置 ID：`Sharpen\LumaSharpen`。

在亮度域增强边缘，减少直接改动色彩。 适合放大后做低成本清晰度补偿。

- **用途**：锐化细节 → 通用锐化。**输出尺寸**：保持输入尺寸。
- **变体差异**：LumaSharpen 算法，1 通道；参数尺度不能跨算法直接比较。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：链尾收尾。 建议放在「放大/超分、降噪」之后。 建议放在「CRT/显示调整」之前。 按最终尺寸补偿柔和感。
- **条件与取舍**：过强会放大噪声并产生光晕。
- **成本**：低到中。源码声明 1 个 pass；4 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[LumaSharpen.hlsl:15](<../../../src/Effects/Sharpen/LumaSharpen.hlsl#L15>)、[LumaSharpen.hlsl:68](<../../../src/Effects/Sharpen/LumaSharpen.hlsl#L68>)、[EffectProtocolCatalogC.h:97](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L97>)。

## SMAA（8）

### SMAA 4x Experimental

当前配置 ID：`SMAA\SMAA_4x_Experimental`。

SMAA 时序近似抗锯齿，缓和几何边缘台阶。 利用捕获帧历史近似时序采样；外部捕获没有游戏内真实 jitter、运动矢量与深度，快速运动可能重影或不稳定。

- **用途**：抗锯齿 → 时序。**输出尺寸**：保持输入尺寸。
- **变体差异**：4x 实验变体，7 通道；不是游戏内 SMAA 时序集成。
- **推荐**：特定场景推荐。适合特定素材、风格或实验场景。 适用条件／尝试建议：先在代表性画面比较副作用。
- **管线位置**：放大后、锐化前。 建议放在「主要缩放」之后。 建议放在「锐化」之前。 在目标分辨率上平滑最终几何边缘。
- **条件与取舍**：依赖捕获帧历史，缺少游戏内真实 jitter/MV/depth。
- **成本**：中到高。源码声明 7 个 pass；2 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[SMAA_4x_Experimental.hlsl:6](<../../../src/Effects/SMAA/SMAA_4x_Experimental.hlsl#L6>)、[SMAA_4x_Experimental.hlsl:33](<../../../src/Effects/SMAA/SMAA_4x_Experimental.hlsl#L33>)、[EffectProtocolCatalogC.h:101](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L101>)。

### SMAA 4x NoJitter Experimental

当前配置 ID：`SMAA\SMAA_4x_NoJitter_Experimental`。

SMAA 时序近似抗锯齿，缓和几何边缘台阶。 利用捕获帧历史近似时序采样；外部捕获没有游戏内真实 jitter、运动矢量与深度，快速运动可能重影或不稳定。

- **用途**：抗锯齿 → 时序。**输出尺寸**：保持输入尺寸。
- **变体差异**：4x 实验变体 / NoJitter，6 通道；不是游戏内 SMAA 时序集成。
- **推荐**：特定场景推荐。适合特定素材、风格或实验场景。 适用条件／尝试建议：先在代表性画面比较副作用。
- **管线位置**：放大后、锐化前。 建议放在「主要缩放」之后。 建议放在「锐化」之前。 在目标分辨率上平滑最终几何边缘。
- **条件与取舍**：依赖捕获帧历史，缺少游戏内真实 jitter/MV/depth。
- **成本**：中到高。源码声明 6 个 pass；2 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[SMAA_4x_NoJitter_Experimental.hlsl:5](<../../../src/Effects/SMAA/SMAA_4x_NoJitter_Experimental.hlsl#L5>)、[SMAA_4x_NoJitter_Experimental.hlsl:32](<../../../src/Effects/SMAA/SMAA_4x_NoJitter_Experimental.hlsl#L32>)、[EffectProtocolCatalogC.h:101](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L101>)。

### SMAA High

当前配置 ID：`SMAA\SMAA_High`。

SMAA 空间抗锯齿，缓和几何边缘台阶。 只看当前帧，适合不希望引入历史依赖的通用边缘平滑。

- **用途**：抗锯齿 → 空间。**输出尺寸**：保持输入尺寸。
- **变体差异**：High 质量预设，3 通道。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：放大后、锐化前。 建议放在「主要缩放」之后。 建议放在「锐化」之前。 在目标分辨率上平滑最终几何边缘。
- **成本**：中。源码声明 3 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[SMAA_High.hlsl:1](<../../../src/Effects/SMAA/SMAA_High.hlsl#L1>)、[SMAA_High.hlsl:12](<../../../src/Effects/SMAA/SMAA_High.hlsl#L12>)、[EffectProtocolCatalogC.h:101](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L101>)。

### SMAA Low

当前配置 ID：`SMAA\SMAA_Low`。

SMAA 空间抗锯齿，缓和几何边缘台阶。 只看当前帧，适合不希望引入历史依赖的通用边缘平滑。

- **用途**：抗锯齿 → 空间。**输出尺寸**：保持输入尺寸。
- **变体差异**：Low 质量预设，3 通道。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：放大后、锐化前。 建议放在「主要缩放」之后。 建议放在「锐化」之前。 在目标分辨率上平滑最终几何边缘。
- **成本**：中。源码声明 3 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[SMAA_Low.hlsl:1](<../../../src/Effects/SMAA/SMAA_Low.hlsl#L1>)、[SMAA_Low.hlsl:12](<../../../src/Effects/SMAA/SMAA_Low.hlsl#L12>)、[EffectProtocolCatalogC.h:101](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L101>)。

### SMAA Medium

当前配置 ID：`SMAA\SMAA_Medium`。

SMAA 空间抗锯齿，缓和几何边缘台阶。 只看当前帧，适合不希望引入历史依赖的通用边缘平滑。

- **用途**：抗锯齿 → 空间。**输出尺寸**：保持输入尺寸。
- **变体差异**：Medium 质量预设，3 通道。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：放大后、锐化前。 建议放在「主要缩放」之后。 建议放在「锐化」之前。 在目标分辨率上平滑最终几何边缘。
- **成本**：中。源码声明 3 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[SMAA_Medium.hlsl:1](<../../../src/Effects/SMAA/SMAA_Medium.hlsl#L1>)、[SMAA_Medium.hlsl:12](<../../../src/Effects/SMAA/SMAA_Medium.hlsl#L12>)、[EffectProtocolCatalogC.h:101](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L101>)。

### SMAA T2x Experimental

当前配置 ID：`SMAA\SMAA_T2x_Experimental`。

SMAA 时序近似抗锯齿，缓和几何边缘台阶。 利用捕获帧历史近似时序采样；外部捕获没有游戏内真实 jitter、运动矢量与深度，快速运动可能重影或不稳定。

- **用途**：抗锯齿 → 时序。**输出尺寸**：保持输入尺寸。
- **变体差异**：T2x 实验变体，7 通道；不是游戏内 SMAA 时序集成。
- **推荐**：特定场景推荐。适合特定素材、风格或实验场景。 适用条件／尝试建议：先在代表性画面比较副作用。
- **管线位置**：放大后、锐化前。 建议放在「主要缩放」之后。 建议放在「锐化」之前。 在目标分辨率上平滑最终几何边缘。
- **条件与取舍**：依赖捕获帧历史，缺少游戏内真实 jitter/MV/depth。
- **成本**：中到高。源码声明 7 个 pass；2 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[SMAA_T2x_Experimental.hlsl:5](<../../../src/Effects/SMAA/SMAA_T2x_Experimental.hlsl#L5>)、[SMAA_T2x_Experimental.hlsl:32](<../../../src/Effects/SMAA/SMAA_T2x_Experimental.hlsl#L32>)、[EffectProtocolCatalogC.h:101](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L101>)。

### SMAA T2x NoJitter Experimental

当前配置 ID：`SMAA\SMAA_T2x_NoJitter_Experimental`。

SMAA 时序近似抗锯齿，缓和几何边缘台阶。 利用捕获帧历史近似时序采样；外部捕获没有游戏内真实 jitter、运动矢量与深度，快速运动可能重影或不稳定。

- **用途**：抗锯齿 → 时序。**输出尺寸**：保持输入尺寸。
- **变体差异**：T2x 实验变体 / NoJitter，6 通道；不是游戏内 SMAA 时序集成。
- **推荐**：特定场景推荐。适合特定素材、风格或实验场景。 适用条件／尝试建议：先在代表性画面比较副作用。
- **管线位置**：放大后、锐化前。 建议放在「主要缩放」之后。 建议放在「锐化」之前。 在目标分辨率上平滑最终几何边缘。
- **条件与取舍**：依赖捕获帧历史，缺少游戏内真实 jitter/MV/depth。
- **成本**：中到高。源码声明 6 个 pass；2 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[SMAA_T2x_NoJitter_Experimental.hlsl:5](<../../../src/Effects/SMAA/SMAA_T2x_NoJitter_Experimental.hlsl#L5>)、[SMAA_T2x_NoJitter_Experimental.hlsl:32](<../../../src/Effects/SMAA/SMAA_T2x_NoJitter_Experimental.hlsl#L32>)、[EffectProtocolCatalogC.h:101](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L101>)。

### SMAA Ultra

当前配置 ID：`SMAA\SMAA_Ultra`。

SMAA 空间抗锯齿，缓和几何边缘台阶。 只看当前帧，适合不希望引入历史依赖的通用边缘平滑。

- **用途**：抗锯齿 → 空间。**输出尺寸**：保持输入尺寸。
- **变体差异**：Ultra 质量预设，3 通道。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：放大后、锐化前。 建议放在「主要缩放」之后。 建议放在「锐化」之前。 在目标分辨率上平滑最终几何边缘。
- **成本**：中。源码声明 3 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[SMAA_Ultra.hlsl:1](<../../../src/Effects/SMAA/SMAA_Ultra.hlsl#L1>)、[SMAA_Ultra.hlsl:12](<../../../src/Effects/SMAA/SMAA_Ultra.hlsl#L12>)、[EffectProtocolCatalogC.h:101](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L101>)。

## SSimDownscaler（1）

### SSimDownscaler

当前配置 ID：`SSimDownscaler`。

面向缩小的结构相似度重采样，帮助在降分辨率时保留观感。 用于缩小而非放大，适合将高分辨率内容压到较小窗口。

- **用途**：放大与超分 → 缩小与尺寸整理。**输出尺寸**：使用效果组配置的输出尺寸。
- **变体差异**：只建议用于输出小于输入的场景；5 通道分离处理。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：尺寸链主步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **成本**：中到高。源码声明 5 个 pass；1 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[SSimDownscaler.hlsl:6](<../../../src/Effects/SSimDownscaler.hlsl#L6>)、[SSimDownscaler.hlsl:21](<../../../src/Effects/SSimDownscaler.hlsl#L21>)、[HdrEffectBoundary.cpp:8](<../../../src/Magpie.Core/HdrEffectBoundary.cpp#L8>)。

## xBRZ（6）

### xBRZ 2x

当前配置 ID：`xBRZ\xBRZ_2x`。

像素画边缘感知放大，减少斜线台阶同时维持块面风格。 固定 2× 版本适合整数倍像素画显示。

- **用途**：放大与超分 → 像素画。**输出尺寸**：固定倍率：宽 ×2，高 ×2。
- **变体差异**：固定 2×、单通道；倍率越大输出像素数越多。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：主要像素画缩放步骤。 建议放在「按需添加的 CRT 风格／显示调整」之前。 先观察像素画缩放本身的结果，保留像素边界，再决定是否添加风格处理。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：低到中。源码声明 1 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[xBRZ_2x.hlsl:3](<../../../src/Effects/xBRZ/xBRZ_2x.hlsl#L3>)、[xBRZ_2x.hlsl:13](<../../../src/Effects/xBRZ/xBRZ_2x.hlsl#L13>)、[EffectProtocolCatalogC.h:105](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L105>)。

### xBRZ 3x

当前配置 ID：`xBRZ\xBRZ_3x`。

像素画边缘感知放大，减少斜线台阶同时维持块面风格。 固定 3× 版本适合整数倍像素画显示。

- **用途**：放大与超分 → 像素画。**输出尺寸**：固定倍率：宽 ×3，高 ×3。
- **变体差异**：固定 3×、单通道；倍率越大输出像素数越多。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：主要像素画缩放步骤。 建议放在「按需添加的 CRT 风格／显示调整」之前。 先观察像素画缩放本身的结果，保留像素边界，再决定是否添加风格处理。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：低到中。源码声明 1 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[xBRZ_3x.hlsl:3](<../../../src/Effects/xBRZ/xBRZ_3x.hlsl#L3>)、[xBRZ_3x.hlsl:13](<../../../src/Effects/xBRZ/xBRZ_3x.hlsl#L13>)、[EffectProtocolCatalogC.h:105](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L105>)。

### xBRZ 4x

当前配置 ID：`xBRZ\xBRZ_4x`。

像素画边缘感知放大，减少斜线台阶同时维持块面风格。 固定 4× 版本适合整数倍像素画显示。

- **用途**：放大与超分 → 像素画。**输出尺寸**：固定倍率：宽 ×4，高 ×4。
- **变体差异**：固定 4×、单通道；倍率越大输出像素数越多。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：主要像素画缩放步骤。 建议放在「按需添加的 CRT 风格／显示调整」之前。 先观察像素画缩放本身的结果，保留像素边界，再决定是否添加风格处理。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：低到中。源码声明 1 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[xBRZ_4x.hlsl:3](<../../../src/Effects/xBRZ/xBRZ_4x.hlsl#L3>)、[xBRZ_4x.hlsl:13](<../../../src/Effects/xBRZ/xBRZ_4x.hlsl#L13>)、[EffectProtocolCatalogC.h:105](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L105>)。

### xBRZ 5x

当前配置 ID：`xBRZ\xBRZ_5x`。

像素画边缘感知放大，减少斜线台阶同时维持块面风格。 固定 5× 版本适合整数倍像素画显示。

- **用途**：放大与超分 → 像素画。**输出尺寸**：固定倍率：宽 ×5，高 ×5。
- **变体差异**：固定 5×、单通道；倍率越大输出像素数越多。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：主要像素画缩放步骤。 建议放在「按需添加的 CRT 风格／显示调整」之前。 先观察像素画缩放本身的结果，保留像素边界，再决定是否添加风格处理。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：低到中。源码声明 1 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[xBRZ_5x.hlsl:3](<../../../src/Effects/xBRZ/xBRZ_5x.hlsl#L3>)、[xBRZ_5x.hlsl:13](<../../../src/Effects/xBRZ/xBRZ_5x.hlsl#L13>)、[EffectProtocolCatalogC.h:105](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L105>)。

### xBRZ 6x

当前配置 ID：`xBRZ\xBRZ_6x`。

像素画边缘感知放大，减少斜线台阶同时维持块面风格。 固定 6× 版本适合整数倍像素画显示。

- **用途**：放大与超分 → 像素画。**输出尺寸**：固定倍率：宽 ×6，高 ×6。
- **变体差异**：固定 6×、单通道；倍率越大输出像素数越多。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：主要像素画缩放步骤。 建议放在「按需添加的 CRT 风格／显示调整」之前。 先观察像素画缩放本身的结果，保留像素边界，再决定是否添加风格处理。
- **条件与取舍**：固定倍率，按源码尺寸表达式使用。
- **成本**：低到中。源码声明 1 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[xBRZ_6x.hlsl:3](<../../../src/Effects/xBRZ/xBRZ_6x.hlsl#L3>)、[xBRZ_6x.hlsl:13](<../../../src/Effects/xBRZ/xBRZ_6x.hlsl#L13>)、[EffectProtocolCatalogC.h:105](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L105>)。

### xBRZ Freescale

当前配置 ID：`xBRZ\xBRZ_Freescale`。

像素画边缘感知放大，减少斜线台阶同时维持块面风格。 Freescale 版本用于非整数或由效果组决定的输出尺寸。

- **用途**：放大与超分 → 像素画。**输出尺寸**：使用效果组配置的输出尺寸。
- **变体差异**：自由倍率、2 通道；同一素材可与固定整数倍版本比较边缘一致性。
- **推荐**：特定场景推荐。有明确用途，但结果依赖素材和参数。 适用条件／尝试建议：从默认值开始并查看副作用。
- **管线位置**：主要像素画缩放步骤。 建议放在「按需添加的 CRT 风格／显示调整」之前。 先观察像素画缩放本身的结果，保留像素边界，再决定是否添加风格处理。
- **成本**：中。源码声明 2 个 pass；0 个可调参数。未进行 GPU 基准测试。
- **源码依据**：[xBRZ_Freescale.hlsl:3](<../../../src/Effects/xBRZ/xBRZ_Freescale.hlsl#L3>)、[xBRZ_Freescale.hlsl:11](<../../../src/Effects/xBRZ/xBRZ_Freescale.hlsl#L11>)、[EffectProtocolCatalogC.h:105](<../../../src/Magpie.Core/EffectProtocolCatalogC.h#L105>)。

## XeSS（1）

### XeSS SR

当前配置 ID：`XeSS\XeSS_SR`。

调用 XeSS 原生时序超分后端，把较低分辨率捕获画面重建到目标尺寸。 外部捕获只能从相邻图像估计运动，并使用合成深度/jitter等辅助量；实际稳定性不同于游戏内 SDK 原生集成。

- **用途**：放大与超分 → 时序重建。**输出尺寸**：使用效果组配置的输出尺寸。
- **变体差异**：HLSL 为占位通道，运行时由 XeSS 后端替换；只支持放大，具体倍率/硬件/运行库限制由初始化检查决定。
- **推荐**：进阶／实验。依赖原生运行库和外部捕获的光流/合成辅助量。 适用条件／尝试建议：确认后端初始化成功；检查快速运动、遮挡和历史重置。
- **管线位置**：主要缩放步骤。 建议放在「必要的降噪/修复」之后。 建议放在「抗锯齿、锐化、显示风格」之前。 让尺寸重建先完成，再做输出分辨率相关的收尾。
- **条件与取舍**：只用于放大；外部捕获辅助量不等同游戏内集成；每轴放大倍率限于 1× 至 3×；支持对应 XeSS 功能的 GPU/驱动；可加载对应 XeSS 运行库；每轴放大倍率 1× 至 3×。
- **成本**：硬件/运行时相关。实际成本来自原生 SDK 调用、资源传输与同步；HLSL 占位 pass 数无法代表这些开销。本轮未进行 GPU 基准测试。
- **源码依据**：[XeSS_SR.hlsl:3](<../../../src/Effects/XeSS/XeSS_SR.hlsl#L3>)、[XeSS_SR.hlsl:35](<../../../src/Effects/XeSS/XeSS_SR.hlsl#L35>)、[NativeEffectBackendFactory.cpp:113](<../../../src/Magpie.Core/NativeEffectBackendFactory.cpp#L113>)、[XeSSUpscaler.cpp:207](<../../../src/Magpie.Core/XeSSUpscaler.cpp#L207>)。

## XeSSFG（2）

### XeSS_FrameGeneration_x2

当前配置 ID：`XeSSFG\XeSS_FrameGeneration_x2_ZeroMV`。

通过 XeSS 原生呈现后端在捕获的真实帧之间生成帧。 列表项是 identity marker；生成帧由呈现端独立发布，不是着色器在该列表位置输出。外部捕获的运动信息有限，UI、光标与遮挡场景需实测。

- **用途**：补帧与帧率 → 补帧。**输出尺寸**：保持输入尺寸。
- **变体差异**：固定 2× 请求补帧标记；同组只能有一个补帧效果，宜配合全局 Frame Rate Filter 理解输入/输出帧率。
- **推荐**：进阶／实验。原生呈现端补帧会改变延迟与发布节奏，并受捕获运动估计限制。 适用条件／尝试建议：同组只选一个补帧器；检查 UI、光标、切屏和快速运动。
- **管线位置**：呈现终端。 建议放在「全部普通画面效果」之后。 生成帧由呈现端独立发布。
- **条件与取舍**：同一效果组只能有一个补帧效果；identity marker 的列表位置不等于原生后端执行位置；支持对应 XeSS 功能的 GPU/驱动；可加载对应 XeSS 运行库。
- **成本**：硬件/运行时相关。实际成本来自原生 SDK 调用、资源传输与同步；HLSL 占位 pass 数无法代表这些开销。本轮未进行 GPU 基准测试。
- **HDR 注意**：HDR 补帧使用终端 HDR10/R10 呈现协议，外部捕获链仍需显示器实测。 EffectProtocolCatalogC 声明 PQ/BT.2100 R10 PresentationTerminal。
- **源码依据**：[XeSS_FrameGeneration_x2_ZeroMV.hlsl:5](<../../../src/Effects/XeSSFG/XeSS_FrameGeneration_x2_ZeroMV.hlsl#L5>)、[XeSS_FrameGeneration_x2_ZeroMV.hlsl:40](<../../../src/Effects/XeSSFG/XeSS_FrameGeneration_x2_ZeroMV.hlsl#L40>)、[Renderer.cpp:118](<../../../src/Magpie.Core/Renderer.cpp#L118>)、[Renderer.cpp:3171](<../../../src/Magpie.Core/Renderer.cpp#L3171>)、[EffectHelper.h:6](<../../../src/Magpie/EffectHelper.h#L6>)。

### XeSS_MultiFrameGeneration

当前配置 ID：`XeSSFG\XeSS_MultiFrameGeneration_ZeroMV`。

通过 XeSS 原生呈现后端在捕获的真实帧之间生成帧。 列表项是 identity marker；生成帧由呈现端独立发布，不是着色器在该列表位置输出。外部捕获的运动信息有限，UI、光标与遮挡场景需实测。

- **用途**：补帧与帧率 → 补帧。**输出尺寸**：保持输入尺寸。
- **变体差异**：2–4× 请求（实际接受值由后端能力决定）补帧标记；同组只能有一个补帧效果，宜配合全局 Frame Rate Filter 理解输入/输出帧率。
- **推荐**：进阶／实验。原生呈现端补帧会改变延迟与发布节奏，并受捕获运动估计限制。 适用条件／尝试建议：同组只选一个补帧器；检查 UI、光标、切屏和快速运动。
- **管线位置**：呈现终端。 建议放在「全部普通画面效果」之后。 生成帧由呈现端独立发布。
- **条件与取舍**：同一效果组只能有一个补帧效果；identity marker 的列表位置不等于原生后端执行位置；支持对应 XeSS 功能的 GPU/驱动；可加载对应 XeSS 运行库。
- **成本**：硬件/运行时相关。实际成本来自原生 SDK 调用、资源传输与同步；HLSL 占位 pass 数无法代表这些开销。本轮未进行 GPU 基准测试。
- **HDR 注意**：HDR 补帧使用终端 HDR10/R10 呈现协议，外部捕获链仍需显示器实测。 EffectProtocolCatalogC 声明 PQ/BT.2100 R10 PresentationTerminal。
- **源码依据**：[XeSS_MultiFrameGeneration_ZeroMV.hlsl:4](<../../../src/Effects/XeSSFG/XeSS_MultiFrameGeneration_ZeroMV.hlsl#L4>)、[XeSS_MultiFrameGeneration_ZeroMV.hlsl:36](<../../../src/Effects/XeSSFG/XeSS_MultiFrameGeneration_ZeroMV.hlsl#L36>)、[Renderer.cpp:118](<../../../src/Magpie.Core/Renderer.cpp#L118>)、[Renderer.cpp:3171](<../../../src/Magpie.Core/Renderer.cpp#L3171>)、[EffectHelper.h:6](<../../../src/Magpie/EffectHelper.h#L6>)。

## 关键问题与待验证项

1. **原生后端成功状态**：DLSSNR 初始化失败时允许回到占位直通，必须在未来 UI/日志中区分“效果组启动”和“算法后端已启用”。DLSS/FSR/XeSS/RTX Video 同样应暴露明确的初始化结果。
2. **时序输入真实性**：当前捕获集成没有游戏引擎提供的真实运动矢量、深度、曝光与 jitter；光流、零运动和合成辅助资源必须作为实际项目约束写入说明。
3. **倍率与尺寸**：固定 2×/3×/整数倍模型不可包装成任意缩放；CAS Scaling、LCAS 与通用采样器等没有固定 OUTPUT 尺寸。CRT Hyllian 明确要求整数倍；FSR3/4 与 XeSS 每轴只接受 1×–3×，FSR2 当前只拒绝缩小而未声明 3× 上限，RAVU Zoom 只用于放大。SSimDownscaler 应单列“缩小与尺寸整理”。
4. **HDR**：着色器能在 FP16 表面编译或存在上游 FP16 版本，均不足以证明当前原生 HDR。优先验证原生时序后端、RTX Video SDR/U8 边界、CRT/锐化/AA 的显示域阈值。
   **NIS 静态疑点**：GroupB 路由把 NIS 选为 DirectFP16，但当前 NIS.hlsl/NVSharpen.hlsl 未设置 `NIS_HDR_MODE`，而 NIS_Scaler.hlsli 默认 NONE；EffectCompiler 只注入 `MP_HDR_COMPATIBILITY`。这表示协议与着色器数学路径可能不一致，尚未实测，不能宣称已确认色彩错误。
5. **推荐与成本**：本次没有 GPU 基准或主观画质盲测，目录只按 pass 数、网络结构和原生依赖给定定性成本；不得把 Ultra、High、新版本或更大网络直接展示成“更好”。

## 源码证据入口

- [效果器源码目录](../../../src/Effects) 与 [发布工程清单](../../../src/Effects/Effects.vcxproj)：161 项实际全集、元数据、pass、参数和尺寸表达式。
- [NativeEffectBackendFactory.cpp](../../../src/Magpie.Core/NativeEffectBackendFactory.cpp)：DLSSNR、DLSS/FSR/XeSS SR、RTX Video 与诊断效果的原生分派及失败语义。
- [Renderer.cpp](../../../src/Magpie.Core/Renderer.cpp)：补帧 marker 的呈现端分流与 Frame Rate Filter 的全链扫描。
- [EffectParameterRules.h](../../../src/Magpie.Core/include/EffectParameterRules.h)：补帧互斥、帧率参数和运行时规则。
- [HDR_EFFECT_PROTOCOL_AUDIT_20260907.md](../HDR_EFFECT_PROTOCOL_AUDIT_20260907.md) 与 [HDR_EFFECT_IMPLEMENTATION_CATALOG.md](../HDR_EFFECT_IMPLEMENTATION_CATALOG.md)：当前协议/adapter 证据与尚未完成的像素验证。

## 审阅边界

本次是内容与信息架构 review，没有修改产品代码、HLSL 或配置，没有编译、部署、启动 Magpie 或进行 GPU/HDR 实测。目录中的每个效果器都有独立记录；同家族共用用途描述时，`variantNotes` 保留倍率、模型规模、档位、通道数或实验语义差异。

## 交付校验

已校验 161 个唯一 ID、各必填说明字段、分类与子分组、每项 OUTPUT 尺寸表达式、源码文件哈希、证据文件与行号范围，以及 Markdown 与 JSON 的逐项对应。该校验不替代算法画质、硬件兼容或 HDR 像素验证。
