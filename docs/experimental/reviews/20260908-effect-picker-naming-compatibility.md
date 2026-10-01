# 效果器 review 修订与命名兼容方案

日期：2026-09-08。对应当前 0.6.7 分支基线 `3976e0aa0230f99f73b93cef905ae629bbd535e9`。

本文件记录内容 review 的修订与配置兼容决策。DLSS SR 的内容表已按最新用户要求更新；下方 RTX Video 的分阶段命名方案保留为历史记录，r1 已完成参数化与内部 ID 统一。当前使用说明见 [效果器选择器](../testing/EFFECT-PICKER.md)。

## 已确认的选择器交互

功能名称：**带用途说明和组合建议的效果器选择器**。用户已选择「单击立即添加、默认追加末尾」，替代此前的“选中后确认＋推荐位置可调整”提案。

- 单击可用效果器条目，立即向当前效果组末尾添加一项；键盘聚焦后按 Enter 执行同一操作。
- 通过悬停或键盘聚焦阅读用途、适用场景、组合建议和使用条件。点击左侧分类只筛选列表。
- 面板取消独立的“添加”确认按钮和“添加位置”选择控件。前后位置建议保留在详情中，用户添加后可在效果组中手动调整；已有项目的相对顺序保持不变。
- 已知组合冲突在添加前检查。冲突项仍可查看说明，显示具体下一步，例如“当前组已使用 XeSS 补帧；如需改用 DLSS 补帧，请先移除 XeSS 补帧项”。该次点击不修改效果组。
- RTX Video 的强度控件独立处理点击，修改档位不会同时触发添加；点击效果器条目时使用当前显示档位。新建条目从低档开始，旧配置档位保持精确映射。
- 搜索、分类浏览、悬停和添加只处理说明与效果组配置；GPU 初始化仍在启动缩放时执行。

交互验收：一次有效单击仅追加一项，Enter 行为一致；浏览、切换分类和操作强度控件均不误添加；冲突点击不修改配置；位置建议不触发自动排序。

## 内容修订

| 对象 | 修订后的说明 |
|---|---|
| DLSS SR | 归为「放大与超分 → 时序重建」，与 FSR、XeSS SR 同组，作为主要缩放步骤。保留缺少原始相机抖动和真实深度带来的重建限制；L／M 模型可能附带的轻微抗锯齿效果仍标明为用户反馈。配置 ID、输出尺寸设置和后端模型保持不变。 |
| DLSS SR 的输入 | 本项目不提供真实深度和相机 jitter：深度为全零资源，提交的 jitter offset 为 `(0,0)`；运动向量可以选择估算。实现中 depth 并非传递 `nullptr`，应区分“没有真实深度”与“没有绑定资源”。 |
| DLSS SR 的模型 | 当前源码请求 Balanced / Preset J，尚未提供 L／M 选择参数。L／M 的效果描述来自用户反馈；后续若将其作为推荐入口，需要核对实际生效模型，并决定如何提供模型选择。本轮未切换模型。 |
| DLSSNR | 保持「风格显示 → AI 画面重塑」，不描述为降噪。按用户提供的本项目兼容口径，说明驱动 615+、RTX 50 系原生支持；RTX 40／30／20 系可能需要手动替换兼容 DLL。较旧显卡的 DLL 替换不保证成功。 |

DLSSNR 的驱动阈值和代际支持来自本轮用户提供的项目经验，本轮没有进行跨显卡验证，也不将其表述成 NVIDIA 所有 DLSS 产品的通用要求。较旧显卡可显示“可能需要兼容 DLL”的使用条件，最终可用性仍依据实际功能初始化结果。

源码依据：

- [DLSS SR 当前请求 Preset J](../../../src/Magpie.Core/DLSSSRUpscaler.cpp#L138)。
- [DLSS SR 的深度资源、可选估算运动与零 jitter](../../../src/Magpie.Core/DLSSSRUpscaler.cpp#L202)。
- [深度提交与 jitter offset](../../../src/Magpie.Core/DLSSSRUpscaler.cpp#L267)。

## RTX Video：八个旧条目收为两个选择入口

面板提供 `RTXVideo_Denoise` 和 `RTXVideo_VSR`，各自选择强度档位：低、中、高、极高。当前底层使用离散的质量档位；先沿用四档精确映射，不假设存在连续的 0–100 强度或已验证的线性强弱关系。

| 新选择入口 | 强度 | 当前配置 ID | 当前 SDK QualityLevel |
|---|---|---|---:|
| RTXVideo_Denoise | 低 | `RTXVideo\RTXVideo_Denoise_Low` | 8 |
| RTXVideo_Denoise | 中 | `RTXVideo\RTXVideo_Denoise_Medium` | 9 |
| RTXVideo_Denoise | 高 | `RTXVideo\RTXVideo_Denoise_High` | 10 |
| RTXVideo_Denoise | 极高 | `RTXVideo\RTXVideo_Denoise_Ultra` | 11 |
| RTXVideo_VSR | 低 | `RTXVideo\RTXVideo_VSR_Low` | 1 |
| RTXVideo_VSR | 中 | `RTXVideo\RTXVideo_VSR_Medium` | 2 |
| RTXVideo_VSR | 高 | `RTXVideo\RTXVideo_VSR_High` | 3 |
| RTXVideo_VSR | 极高 | `RTXVideo\RTXVideo_VSR_Ultra` | 4 |

例如旧配置 `RTXVideo\RTXVideo_VSR_High` 在新面板显示为 `RTXVideo_VSR`、强度“高”。其输入、输出、效果组内位置和其他参数均保留。已有组内若放了多个 RTX Video 阶段，每个阶段仍独立存在；合并的是选择器入口，不是用户的效果组内容。

八个源文件仍分别列入 161 项审计目录。按此方案合并选择入口后，选择器可展示 155 个逻辑条目；该数量不代表已经删除或合并了源文件。

源码依据：[RTX Video 档位分派](../../../src/Magpie.Core/NativeEffectBackendFactory.cpp#L238)、[后端档位与尺寸校验](../../../src/Magpie.Core/RTXVideoDenoiser.cpp#L156)。

## XeSSFG：去掉显示名称中的 ZeroMV

| 当前配置 ID | 统一显示名称 | 当前可选运动输入 |
|---|---|---|
| `XeSSFG\XeSS_FrameGeneration_x2_ZeroMV` | `XeSS_FrameGeneration_x2` | 无、AMD 估算、NVIDIA 估算 |
| `XeSSFG\XeSS_MultiFrameGeneration_ZeroMV` | `XeSS_MultiFrameGeneration` | 无、AMD 估算 |

两个入口仍区分固定 2× 与多帧倍率；名称不再暗示始终使用零运动向量。多帧版本的可选运动输入范围按当前实现保留。

当前 [EffectHelper::GetDisplayName](../../../src/Magpie/EffectHelper.h#L6) 已有以上显示别名；review 目录原先从文件名生成显示名称，未复用该别名，本次修正目录。当前持久化 ID 和后端匹配仍使用旧名称。

## 建议先采用显示层兼容

第一阶段将“显示名称／选择入口”和“保存的效果器 ID”分开：

- XeSSFG 统一使用新的显示名称，继续识别和保存已有内部 ID。
- RTX Video 在选择器中合并为两项，强度选择映射回对应旧 ID。打开已有配置时，从旧 ID 还原强度；改变强度只替换当前阶段的对应 ID，保留其位置、尺寸规则和参数。
- 配置文件读取、便携配置、导入与导出都继续接受旧 ID。只打开、浏览面板时不写回配置。
- 保留每个已有阶段，以及用户自定义效果组的名称、顺序和程序配置引用。新建条目默认低档，旧配置档位由映射表精确保留。
- 运行时变更档位需接入现有停止／重新初始化流程，并向用户明确生效时机；档位变化涉及原生后端重新创建，不能仅当作普通着色器常量更新。

该阶段即可完成用户可见的名称和选择方式调整，同时让升级前后的配置继续使用同一套效果器标识。

## 如果后续也统一内部 ID

可进一步采用：

- `RTXVideo\RTXVideo_Denoise`、`RTXVideo\RTXVideo_VSR`，参数 `qualityLevel` 为 1／2／3／4，对应低／中／高／极高。适配器将降噪档位映射到 SDK 的 8／9／10／11，VSR 映射到 1／2／3／4。
- `XeSSFG\XeSS_FrameGeneration_x2`、`XeSSFG\XeSS_MultiFrameGeneration`，原有倍率和运动估算参数保持含义。

这一步需要完整迁移，而非只更新文件名：

1. 建立精确的内置效果器旧 ID → 新 ID 映射，在配置加载和导入的统一入口执行，早于效果器查找、参数校验和配置恢复判断。未知的自定义效果器保持原样。
2. 将旧 RTX Video 后缀转成相应 `qualityLevel`；已有合法档位参数保留，缺失时从后缀恢复。XeSSFG 保留已保存的运动估算方式，缺失参数按旧版本含义补齐，不能因名字去掉 ZeroMV 就自动启用光流。
3. 连同资源打包、后端工厂、HDR 路由、补帧互斥、参数显隐／重启、默认组、导入导出和日志定位一起更新识别规则。首次升级先备份原配置，再原子写回，重复加载的结果不变。
4. 明确回退旧版本的路径：保留升级前备份，或导出为旧 ID／后缀格式。新内部 ID 写回之后，不能承诺旧程序仍可直接读取。

现有 [ScalingModesService::Import](../../../src/Magpie/ScalingModesService.cpp#L442) 同时服务加载和手动导入，并调用 [NormalizeV065ScalingModes](../../../src/Magpie/ScalingModesService.cpp#L284)；可复用此统一入口。XeSSFG 参数已有专用归一化逻辑，迁移时需与其顺序协调。

## 实现时的验收范围

- 八种 RTX Video 旧 ID 分别还原到相同档位，两个 XeSSFG 旧 ID 分别还原到相同倍率、运动估算和其他参数。
- 全局配置、便携配置、手动导入与重复加载得到一致结果；旧配置不触发误重置，组索引和程序引用不变化。
- 同组多个 RTX Video 阶段仍保持各自的次序、档位和尺寸；未操作强度时保存后数值不漂移。
- 新旧名称都能被搜索；产品显示名称不出现 ZeroMV，诊断可保留内部 ID 以便定位。
- 若统一内部 ID，额外验证别名幂等、备份与写入失败恢复、旧格式导出／回退，以及启动前互斥检查仍有效。

已实现第一阶段显示层兼容；配置继续保存旧 ID。RTX Video 档位编辑在下次启动缩放时应用，不执行热切换。原生后端与配置迁移逻辑未改动。
