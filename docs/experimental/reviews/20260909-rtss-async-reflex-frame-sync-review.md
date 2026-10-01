# RTSS Async／Reflex 与 Magpie 帧同步选项调查

日期：2026-09-09。源码基线：`0.6.7`，`3928bdbe112c93b07d9d75ec224176582e9e2fc7`。本轮为资料与源码调查，未修改运行逻辑、RTSS 配置或部署文件，未运行游戏／Magpie／GPU 延迟测试。

## 结论

可以扩展现有帧同步入口。建议保留原开关与目标基础 FPS，新增模式选择：**Front Edge Sync（现有默认）、Async（严格间隔）、NVIDIA Reflex（驱动限帧，实验）**。XeSS FG 实际使用 XeLL 时显示 SDK 接管状态。

优先实现普通 NR／SR 的 Async，再验证无 FG 的 Reflex，最后适配 DLSS FG 的驱动限帧。Reflex 低延迟和 Reflex 帧率限制是两个独立功能；当前 DLSS FG 已请求前者，后者的 `minimumIntervalUs` 仍为 0。切换帧同步模式不应顺带关闭 DLSS FG 已有的低延迟功能。

收益主要可能来自减少已经处理完的画面等待、限制无用的提前工作和降低排队。现有 Front Edge 模式已经有共享结果消费反馈，不能把这些收益全部算成新增功能，也不能从限帧器名称推导出必然更低的延迟。

## RTSS 的可确认行为

本机 RTSS 为 **7.3.5.28314**。直接读取安装包随附的 `Help/Properties/General/SYNC_LIMITER`、`REFLEX_SLEEP`、`REFLEX_SET_LATENCY_MARKER`、`ENABLE_PASSIVE_WAITING`。这些属于官方用户帮助；本轮没有取得 RTSS 核心限帧源码，不将内部补偿公式、精确 hook 布局或驱动内部算法当作已确认实现。

| 模式 | 官方帮助描述的目标 | 迟到后的取舍 |
| --- | --- | --- |
| Async | 限制连续帧开始之间的最小间隔，偏向低输入延迟 | 跟不上目标时允许起始时间漂移，优先满足间隔下限 |
| Front Edge Sync | 对齐 Present 开始时间，偏向提交间隔稳定 | 可以短暂超出帧率上限以恢复时钟相位；处理完成后可能继续等待 |
| Back Edge Sync | 对齐 Present 结束／下一帧开始时间 | 也允许相位恢复，和 Async 接近但并不相同 |
| NVIDIA Reflex | 停用 RTSS 自有精确节奏，使用 NVIDIA 的帧率限制机制 | 驱动决定等待；RTSS 不再保证其自有精确限帧曲线 |

这里的 Async 不代表额外创建一个异步线程，也不是异步计算或插帧。Front Edge Sync 不等于 VSync／扫描线同步；帧开始稳定、Present 稳定、实际显示稳定是不同指标。

本机帮助说明 Reflex 模式支持 NVIDIA 上的 D3D11／D3D12，条件不满足时回退 Async。其主要用途包括 DLSS FG；在没有原生 Reflex 的游戏里还会请求低延迟功能。不能据此断言当前 Magpie 的直接 NGX、自管输出实现能原样套用 RTSS 的 FG 限帧方式。

`REFLEX_SLEEP` 帮助单独解释了注入位置：可由驱动自动选择，也可放在 Present 前或后。前者偏提交稳定，后者在模拟和输入也运行于渲染线程的游戏中可能更有利于延迟。它是第三方工具无法直接改写游戏主循环时的折中；Magpie 能控制自己的取帧入口，应优先在那里等待，不能照搬“总放到 Present 后”。

被动／主动等待是另一个维度。它改变 CPU 占用与唤醒精度，不改变帧同步模式的目标。默认继续使用可中断定时器；整周期忙等不作为首批功能。

## 官方 Reflex 依据与新取得的资料

[NVAPI 文档](https://docs.nvidia.com/nvapi/group__dx.html) 明确区分 `bLowLatencyMode` 与 `minimumIntervalUs`，并建议在每帧开始调用一次 `NvAPI_D3D_Sleep`。设置只需在变化时更新；应用可以让该调用承担限帧等待。

本轮从 [NVIDIA 官方 REFLEX 仓库](https://github.com/NVIDIA-RTX/REFLEX/tree/c2522cee75be7a952ab29281c4cf6d978c5e2e0e) 取得原生集成指南 v1.6，并核对第 14–16 页的完整图文。PDF SHA256：`7225E00D42B9B540BDE2CA35F3F4EA0749AA0908ABE8004BD7DAFD3CB145ED19`。仓库也包含验证资料入口，更新了此前“原生资料下载需要登录、尚未取得”的调查状态；本轮没有安装或运行验证工具。

指南补充说明：

- 低延迟关闭与结束整个 Reflex 生命周期不同；不能把正常 Off 模式实现成停止所有 Sleep／marker 调用。
- `bUseMarkersToOptimize` 在指南描述的实现中依赖 Boost，不能把当前 `true + Boost Off` 当作已经获得这项额外优化的证据。开启 Boost 也不等于必然改善 Magpie，需要按功耗与延迟对比。
- 基础同步等待与实际工作阶段应区分，跨线程工作不能只相加各段时长来推导总延迟。

原生指南没有给出针对 Magpie 当前双 D3D11 设备、NGX D3D12 FG 与自有 Present FIFO 的直接配置答案。帧率目标到底按基础周期还是所有输出 Present 被驱动解释，需在此组合中验证，不能预先套用 XeLL 的倍率折算。

## 当前源码的差距

| 位置 | 已确认状态 | 扩展影响 |
| --- | --- | --- |
| `FramePacingOptions.h:12` | 帧同步状态只有开关和 FPS | 增加明确模式，并扩展已有保存、两处界面合并和受控重启逻辑 |
| `Renderer.cpp:1000` | 普通路径支持提前准备，在 Present 前等待 | 保留作为 Front Edge；Async／Reflex 不再走同一 FPS 的提交截止时间 |
| `StepTimer.cpp:88` | 实际开始时间会向旧周期网格回拨 | 关闭 Front Edge 后的普通限帧不能直接称为严格 Async |
| `FramePresentationTiming.h:11` | 现有 Front Edge 小迟到时最低间隔允许到周期的 75% | 属于相位补偿，Async 应采用独立规则 |
| `Renderer.cpp:2916` | 消费端控制节奏时停用重复的 StepTimer 限帧 | 已有单一限帧职责的基础，适合推广为模式解析结果 |
| `Renderer.cpp:438,2792` | Reflex 初始化和工作周期入口仅对 DLSS FG 开放 | 单独 DLSSNR／SR 当前没有这条低延迟接入 |
| `ReflexController.cpp:55` | On、Boost Off、额外目标 0，接口只接受 enabled | 要让驱动限帧，需增加目标和独立低延迟状态 |
| `DeviceResources.cpp:177` | 前端设备跨线程支持由 DLSS FG 条件触发 | 无 FG Reflex 必须同步调整设备条件，并核对效果设备与呈现设备的标记覆盖 |
| `Renderer.cpp:3290` | DLSS FG 可在图像处理完成后暂存输入，等待 FG 输入时钟 | Reflex 接管基础限帧后，应撤掉同目标的这层等待；保留颜色／光流／历史一致性 |
| `XeSSFGPresenter.cpp:691,732` | 基础目标换算为输出目标传给 XeLL；调用 xellSleep | 继续由 XeLL 管理，避免再套 Reflex 或 Async 限帧 |

源码入口：[计时器](../../../src/Magpie.Core/StepTimer.cpp)、[时钟](../../../src/Magpie.Core/FramePresentationTiming.h)、[Renderer](../../../src/Magpie.Core/Renderer.cpp)、[Reflex 驱动层](../../../src/Magpie.Core/ReflexController.cpp)、[Reflex 生命周期](../../../src/Magpie.Core/ReflexController.h)、[XeSS 呈现器](../../../src/Magpie.Core/XeSSFGPresenter.cpp)。

以 80 FPS 的 12.5 ms 周期为例，旧网格锚点为 100 ms，新帧实际在 119 ms 开始时，StepTimer 会将锚点修正为 112.5 ms，下一次可在 125 ms 开始，两个实际开始之间仅 6 ms。严格 Async 则应到 131.5 ms 才准入下一帧。这是源码公式的算术例子，不是帧率实测。

## 建议的模式职责

### Front Edge Sync

保留目前体验与旧配置默认值。无 FG 时偏向稳定提交；DLSS FG 保留现行输入与输出调度；XeSS FG 显示实际由 XeLL 控制。仍然存在输入中断和处理超时的边界，不能用重复旧图掩盖内容停顿。

### Async（先做普通 NR／SR）

在取出本次最新捕获帧之前等待，按上一次实际准入时间维护严格最小间隔。迟到后以实际时间重新确定节奏，不为了赶回网格缩短下一次间隔。等待发生在工作之前，避免“效果耗时 + 再等完整周期”意外降低帧率。

继续保留一个结果尚未消费时的反馈、共享纹理锁／fence 和交换链容量控制；处理完后在容量可用时提交，不为同一目标再等 Front Edge 截止时间。反馈机制要从 `_frontEdgeSyncEnabled` 中拆开，否则简单关闭该布尔值会连队列约束一并关闭。

基础内容目标和 UI／光标重绘计数分别定义；UI 可以合并更新，但不能令牌饥饿，也不能把旧图重绘算成新的 NR／FG 输入。Async 控制基础内容开始频率，不承诺所有 Present 统计必然严格等于该 FPS。

### NVIDIA Reflex（驱动限帧）

沿用现有原生 NVAPI 接入，增加普通无 FG 工作周期。将解析后的唯一目标交给 `minimumIntervalUs`，在取帧前调用 Sleep；同目标的 StepTimer／普通 Front Edge 截止时间退出。保留资源安全等待、容量反馈和故障回退。

普通无 FG 的目标换算清楚：80 FPS 对应 12,500 µs。DLSS FG 需另测 2x／3x／4x；先保留逐张生成帧／真实帧的 FIFO 间隔和顺序，再确认驱动限帧与这条输出时钟的分工。**移除重复基础限帧不等于取消生成帧的输出节奏。**

对于普通 NR／SR，需复查多设备工作归属、一个工作周期一次 Sleep、静止输入、仅 UI 重绘、停止和重启。现有故障 Stop 是终止状态，不能直接复用为正常 Off 切换。DLSS FG 中默认 On、Boost Off 保持原策略；模式选择控制限帧职责，不额外暴露为效果器参数。

不支持或调用失败时恢复已解析的同一基础目标，回退 Async 并显示实际模式及下一步，例如：“当前呈现路径无法使用 Reflex，已使用 Async 保持 80 FPS；切换到支持的 NVIDIA DXGI 路径后重新启用可重试。”选项可见但条件说明明确，避免只显示失败。

### XeSS FG／XeSS MFG

[Intel XeLL 指南](https://github.com/intel/xess/blob/main/doc/xell_developer_guide_english.md) 建议将目标交给 XeLL，并避免同时启用应用限帧器和其他低延迟机制。保留当前 XeLL 限帧路线，界面明确显示 SDK 接管，不能在 NVIDIA 显卡上因为选择了 Reflex 就给 XeSS 再叠加一层。

## 界面和兼容性

- 保留现有帧同步开关、目标基础 FPS 与 0 自动语义，在其下新增模式选择；旧配置迁移到 Front Edge，默认值不变。
- 开关关闭表示退出额外帧同步策略，不代表清除 FrameRate Filter、应用配置或 SDK 自身的限制。
- FrameRate Filter 跟随项改为“跟随帧同步目标”，从统一配置取得目标，三种模式都只解析一次；保留旧 ID、自定义数值和关闭同步后的原选择。
- 模式按现有“应用并重新启用”生效；控制器实例、待提交帧和旧时钟不能跨模式复用。程序输出实际策略供定位，稳定运行不周期刷日志。
- 首批不新增 Back Edge、整周期主动忙等、Reflex Sync 高级反馈或独立 Boost 开关。前两者对当前捕获处理架构的新增价值有限，后两者需要独立验证。

## 性能和验收范围

Async 的软件时钟成本很低，但不会让同一套 NR／SR 算法本身更快；其潜在收益是工作时机与积压控制。Reflex 增加驱动调用，等待本来就是它的工作方式，不能把等待时间直接当成性能浪费，也不能提前承诺延迟降低百分比。

现有 StepTimer 在最后 1 ms 使用 `Sleep(0)` 循环，新模式可复用 `FramePacingWait` 的可中断定时器来降低这段 CPU 活动；唤醒抖动仍需比较，不把“更省 CPU”和“更精确”混为同一结论。

验证建议使用相同源程序限帧、目标 FPS、效果与场景，对比无 FG NR／SR 的 Front Edge、Async、Reflex，再分别测试 DLSS 2x／3x／4x 与 XeSS。观察新捕获／新内容间隔、Present 间隔、实际显示间隔、捕获后帧年龄、CPU／GPU 占用及队列等待。静止、源帧率不足、长卡顿、跨屏、停止重启和参数变更单独覆盖。

测量放在独立诊断构建或可关闭的短时采集中；普通 Release 不新增每帧文本日志、持续 GetLatency 查询或 GPU 像素回读。Magpie 能优化捕获到显示这一段，无法从现有捕获接口控制游戏的输入采样／模拟线程；源程序端限帧仍由源程序或 RTSS 负责。

建议分三批验收：普通 Async → 普通 Reflex → DLSS FG 驱动限帧。UI 模式、基础限帧和低延迟状态内部拆开，SDK 接管状态显式呈现，再根据实际结果决定是否调整默认模式。
