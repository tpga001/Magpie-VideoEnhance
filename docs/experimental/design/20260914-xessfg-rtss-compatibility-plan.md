# XeSS_FrameGeneration 与 RTSS 兼容性定位及修改方案

> 2026-09-14 执行前修订：用户报告的“3×／4×画面已出现、帧率延迟升高”已通过 RTSS 保持开启的重复单变量实验定位到多帧期限延期与捕获周期估计的反馈。先修正这条慢启动路径；不要把关闭 RTSS 标记注入当作其必要修复。原文的标记崩溃证据作为独立问题保留。详见 [慢启动定位与实测](20260914-xessfg-startup-pacing-investigation.md)。本文件后文为修订前的调查与方案。

日期：2026-09-14。基于 068 提交 `3ac545454c33d2342117e7e6924e3467687a03fa`、本机 RTSS 帮助及 SDK、此前真实捕获日志和崩溃转储。本轮为离线定位和方案，不修改程序、RTSS 设置或部署包；未重新启动 RTSS 做开关对照。

## 1. 结论

当前有两个独立问题，应分别修复、验收：

1. **崩溃触发路径已定位到 RTSS 注入的 NVIDIA Reflex 延迟标记。** SDK 初始化交换链时进入 RTSS，RTSS 调用 `NvAPI_D3D_SetLatencyMarker`，NVAPI 查询 D3D12 设备接口时发生访问异常，随后控制流保护终止进程。转储中的标记帧号带 `RTSS` 签名，证据比仅有模块调用栈更明确。具体设备／接口为何无效，尚不能判定是过期引用、钩子链损坏还是驱动兼容性。
2. **额外限帧造成的多帧节奏问题。** 换名测试进程受到 RTSS 全局 60 FPS 配置影响；3×出现较长 Present 和调度等待，4×测试时段内未积累足够提交样本。它不能解释原名 Magpie 进程的初始化崩溃。

推荐目标：**RTSS 继续运行并显示 OSD；XeSS 会话由 XeLL 管理低延迟和输出限帧，RTSS 不再向 Magpie 注入 Reflex 标记、Sleep 或额外限帧。** 如用户要用 RTSS 限制源游戏，可以继续针对源游戏进程配置；该限制不等于限制 Magpie 的插帧输出。

## 2. 为什么看起来 2×可以、3×和 4×不行

| 差异 | 2× | 3×／4×（本机 NVIDIA GPU） |
|---|---|---|
| SDK 路径 | 原生 XeFG | 验证过的跨厂商多帧解锁及调度补丁 |
| 一次源帧对应输出 | 1 张生成帧＋1 张真实帧 | 2／3 张生成帧＋1 张真实帧 |
| 额外调度代码 | 不启用 | Present／scheduler／timestamp 三处适配 |
| 历史计时 | 原生路径 | 使用正式捕获时间估计；每帧错误重置已在上一提交修复 |
| RTSS／XeLL 是否可能冲突 | 可能 | 可能，且更多输出调用使外部等待影响更明显 |

不能把倍率作为崩溃的充分条件：

- 原名测试 `capture-20260914-152049` 中，3×已有 **360 次提交、1078 张 SDK 输出**，仅首轮为部分输出；两个后续 120 次提交区间各增加 360 张输出。这证明在开启 RTSS 的那次进程中，3×能够正常生成多帧。
- 崩溃转储发生在 SDK 交换链初始化链，而非 `Pacing::Detour`／`TsDetour` 的逐帧执行链。日志在 3×会话收尾处中断，不能仅凭缓冲日志精确确认下一次初始化的倍率。
- RTSS 专用配置有 `InjectionDelay=15000`。初始化顺序、已注入状态和设备重建与倍率切换同时变化；必须用长时间 2×和重复 2→2 重建作对照，排除“2×永远安全”的误判。该延时配置不能证明转储进程的实际注入时刻。
- Intel Arc 的原生多帧路径不使用本机 NVIDIA 的兼容补丁，不能直接套用本机结论。

最初 3×／4×只有单帧输出的历史重置缺陷已经修复；本轮 RTSS 问题不应与它混为一谈。

## 3. 崩溃证据：从模块定位到具体调用

转储：`C:/Users/81443/AppData/Local/CrashDumps/Magpie.exe.43460.dmp`，2026-09-14 15:21:36；NVAPI 驱动版本 `32.0.16.1692`。

已确认的调用链：

```text
XeSSFGPresenter::_Initialize
  → xefgSwapChainD3D12InitFromSwapChainDesc
  → libxess_fg 内部初始化
  → RTSSHooks64
  → NvAPI_D3D_SetLatencyMarker
  → D3D12Core!CLayeredObject<CDevice>::CContainedObject::QueryInterface
  → 间接调用检查／访问异常
  → FAST_FAIL_GUARD_ICALL_CHECK_FAILURE
```

关键证据：

- `RTSSHooks64+0x26a36` 查询 NVAPI ID `0xD9984C05`，随后 `+0x26a7a` 调用解析出的函数。NVIDIA 官方接口表将该 ID 映射为 `NvAPI_D3D_SetLatencyMarker`。[NVAPI 接口表](https://raw.githubusercontent.com/NVIDIA/nvapi/main/nvapi_interface.h)
- 恢复调用栈帧可得参数地址 `0x00000040bb0fdb70`。内容为结构版本 `0x00010058`、frameID `0x5254535300000935`、markerType `1`。帧号高 32 位 `0x52545353` 对应 `RTSS`；字段布局与 NVIDIA 的 `NV_LATENCY_MARKER_PARAMS_V1` 一致。[NVAPI 结构定义](https://github.com/NVIDIA/nvapi/blob/main/nvapi.h)
- 内层异常记录 `0x00000040bb0fd1b0` 为 `0xc0000005`，在 `0x00007fff6cf060c0` 尝试读取 `0xffffffffffffffff`；最终异常为 `0xc0000409`、子码 `0xa`。因此不能仅凭最终异常名断言“只缺 CFG 注册”。
- mini dump 未包含关键目标内存和完整设备对象，无法证明具体指针的分配、销毁责任；也不能将缺失内存直接解释为已释放内存。
- Magpie 自身 `Renderer.cpp` 仅为 DLSS FG／普通 Reflex 模式初始化 `_reflex`；XeSS 使用 XeLL。当前证据不支持“Magpie 同时主动开启自己的 Reflex 和 XeLL”这一猜测。

外部交叉证据：本机 RTSS 官方帮助明确表示标记注入可能导致应用不兼容，并支持按应用关闭；OptiScaler 的 XeFG 发布说明要求关闭 RTSS Reflex 标记注入，其源码也检测 RTSS 标记并提示冲突。它提供已知兼容性背景，不代替本机对照实验。[OptiScaler 发布说明](https://github.com/optiscaler/OptiScaler/releases) · [固定提交的检测代码](https://github.com/optiscaler/OptiScaler/blob/731f3b79c762bc92971e5fe33dade87c6f83067b/OptiScaler/hooks/Reflex_Hooks.cpp)

## 4. 限帧和调度证据

本轮读取的磁盘配置：

| 字段 | Magpie.exe 专用配置 | 全局配置／换名测试来源 |
|---|---|---|
| Limit | 0 | 60 |
| SyncLimiter | 1 | 0 |
| ReflexSetLatencyMarker | 1 | 应在测试时查询实际生效值 |
| ReflexSleep | 0 | 应在测试时查询实际生效值 |
| UseDetours | 1 | 1 |
| InjectionDelay | 15000 | 15000 |

磁盘值不等于进程实际生效值。不能把全局 60 FPS 套到原名 Magpie；也不能认为“Limit=0”会关闭 Reflex 标记注入。`UseDetours=1` 已开启，因此仅建议开启 MS Detours 不能作为本机的新修复。

`capture-20260914-152433` 的换名测试中，3×在 120 次提交时记录：SDK 输出 346 张、源帧间隔约 53.332 ms、估计 50.003 ms、Present 48.199 ms、额外等待 16.349 ms；provider 提交间隔 P50 31.113 ms、P95 62.569 ms。说明生成仍在进行，但呈现链明显变慢。这里尚未把每段等待精确归因到 RTSS、XeLL 和补丁，也没有最终显示事件，不能据此宣称测得完整输出 FPS 或某一方的全部耗时。

机制上，RTSS 在 Magpie 呈现调用处等待，XeLL 和兼容调度同时维护时间计划，可能形成串联等待。假设 RTSS 钩住每一次实际输出且上限为 L，则输出上限仍为 L，稳定完整倍率 M 对应的输入上限约为 L/M；这只是条件模型，必须先验证钩住的是代理、真实交换链还是多条交换链。

现有调度还有需要加固的代码：`PaceFrame` 在目标落后时重新等待一个间隔；`TsDetour` 在后续输出迟到时将期限延至当前时间加一个间隔。外部已经阻塞后继续延后，可能放大突发输出耗时。捕获时间估计优先于 provider ring 是正确方向，但“被接收帧的时间间隔”仍可能包含背压导致的上游跳帧，不能当成游戏原始渲染周期，也不能简单减去整个 Present 耗时。

Intel 官方建议 XeLL 工作时关闭其他低延迟／额外限帧机制，限制目标传给 XeLL；这支持由 XeLL 统一管理 Magpie 输出的方案。[XeLL 开发指南](https://github.com/intel/xess/blob/main/doc/xell_developer_guide_english.md#requirements)

## 5. 推荐修改路线

### 第一步：完成最小变量对照并增加诊断

扩展 `Run-XeSSFGCaptureSmoke.ps1` 和 `XeSSFGCaptureFixture.cpp`：每种倍率单独冷启动、持续至少 60 秒；增加 2→2、3→3、4→4 和 2→3→4→2 重建循环。记录 RTSS 模块首次出现时间、文件身份、实际查询结果、SDK 初始化／销毁阶段、设备身份及钩子启用状态。

优先做 **OSD 开启、Limit=0，仅改变 Reflex 标记注入开关** 的对照。若关闭标记注入消除相同崩溃，再决定最小兼容措施。仍崩溃时才依次比较注入时机、设备销毁／重建和单独关闭 OSD／D3D12 hook；不要同时改变多项后宣称找到了单一根因。

逐帧诊断应保留：源时间、输入 ID、burst ID／index、调度目标、等待前后 QPC、native provider 调用耗时，以及交换链地址和线程 ID。统计要能区分代理 Present 与最终 DXGI 输出；不能只靠每 120 次 SDK 累计计数验收。

### 第二步：RTSS 与 XeLL 的按进程协调（主方案）

新增隔离的 `RtssCompatibility` 适配模块，供 XeSS 2×／3×／4×共同使用：

1. 识别当前进程是否已加载 RTSSHooks64；只检测，不为了探测而主动加载注入 DLL。读取可查询的生效属性，并区分“不支持／未知”和“已关闭”。
2. 目标设置为保留 OSD、关闭 Magpie 的 Reflex 标记注入和外层限帧；关闭可能启用的 Reflex Sleep／Reflex 限帧。XeLL 的上下文和必要标记继续保留。
3. 优先验证 RTSS 官方属性接口能否安全控制当前进程。SDK 示例确有 `GetProfileProperty/SetProfileProperty` 和 `FramerateLimit`，但示例没有承诺 `ReflexSetLatencyMarker`、`ReflexSleep` 磁盘键就是可用的 API 属性名，也没承诺 setter 会立刻撤销已安装的 NVAPI hook。必须先验证查询、修改、回读和实际调用链。
4. 若运行时接口不支持目标字段或生效太晚，提供一次性的 Magpie 专用配置协调及重启生效路径。只调整经过验证的专用配置字段；不修改 RTSS 全局或游戏配置。不要通过修改配置的成功返回推断旧进程的注入已经撤销。
5. 若需要临时设置，保存原值；释放时仅恢复仍等于本模块写入值的字段，避免覆盖用户中途修改。需明确属性是进程状态还是持久配置，再确定恢复与重启策略。
6. 处理延迟注入：初始化时无模块不代表整个会话没有 RTSS。增加低频检测／阶段检测；已知不安全的注入组合在设备初始化前处理，不能等到第一帧后才补救。无法消除加载竞态时，应要求专用配置在下次启动前生效，不用任意 Sleep 充当修复。

这条路线的产品目标是用户仍能保留 RTSS 的 OSD 和源游戏限帧；不承诺 RTSS 的 Reflex 注入与 XeLL 在 Magpie 中同时工作。

### 第三步：多帧调度抗外部阻塞

对 `XeSSFGPacing.h` 做可独立测试的调度改进：

- 一个 burst 使用固定时间锚点和有界期限；外部等待已消耗的时隙不再机械追加完整间隔。
- 为严重迟到设定有界恢复：避免连续立即提交所有过期帧造成新的突发，也避免每张迟到帧继续向后延期。下一 burst 重新定相；保留 SDK 所需的输出顺序。
- 分离自有等待、native provider 调用时间与来源不明的等待。当前 `extraWaitNs` 包含 scheduler 函数总耗时，不能直接贴上“纯 Magpie 等待”的标签。
- 测试 0／4／16.7／33.3 ms 外部阻塞、不规则源间隔、捕获重启、资源换代和无时间戳回退；验证期限有界、倍率恢复、输入估计不自激增长。
- 保留 DLL 哈希、字节、页面校验和销毁后回滚；不为兼容 RTSS 放宽补丁验证。

调度加固不能让输出突破有效的 RTSS 硬上限，也不能修复初始化时无效设备接口。应作为独立补充，不替代第二步。

### 第四步：仅在主方案失败时深入处理钩子／生命周期

若关闭 RTSS 标记后仍出现同一设备查询异常，用完整转储／首机会异常断点定位实际 device、vtable 和钩子跳板的分配、引用及销毁过程，再判断是否需要调整资源生命周期。当前尚无证据要求改写 XeFG 初始化 API或永久缓存设备。

不推荐直接拦截所有 NVAPI 标记、仅按 frameID 高位过滤、手工修补 RTSS 跳板或关闭 CFG。它们可能误伤正常标记、增加钩子冲突，且没有解决无效接口来源。若未来必须做专门的标记过滤，应验证调用来源、作用设备、会话范围及卸载次序，作为单独评审的后备路线。

## 6. 验收矩阵和顺序

| 场景 | 主要验证内容 |
|---|---|
| RTSS 退出 | 保持现有 2×／3×／4×和 DLSSNR＋4×基线 |
| RTSS 开启、OSD 开启、无标记注入、无 Magpie 限帧 | 主兼容目标，冷启动和循环重建无同类崩溃 |
| RTSS 开启、OSD 开启、标记注入开启、无 Magpie 限帧 | 明确标记开关的因果差异及协调措施是否在初始化前生效 |
| RTSS 仅限制源游戏 | 输入稳定，Magpie 仍按目标倍率输出 |
| RTSS 限制 Magpie 为 60／120 FPS | 验证代理／真实 Present 钩子层级；不能错误要求输出超过上限 |
| RTSS 延迟注入、运行中开关、倍率反复切换 | 加载时序、清理与配置恢复 |
| 窗口／无边框、SDR／HDR、VRR 开关 | 主方案通过后再扩大验证范围 |

先做每个倍率 60 秒及至少 10 次同进程重建，再做 15 分钟稳定性。通过 ETW／PresentMon 按进程、交换链和显示状态核对 SDK 生成、提交与真正显示；记录重复／丢弃、间隔 P95／P99 和输入延迟。OSD 能显示、SDK 计数达到倍率、无崩溃分别是不同验收项。

后续执行建议：**先验证并处理 RTSS Reflex 标记注入，再统一限帧控制，随后加固多帧调度，最后扩展显示与长期回归。** 本轮仅完成定位与方案，未宣称 RTSS 兼容已经修复。

## 7. 本地证据索引

- `.tools/xess-mfg-impl/sequence-fix-crash-analysis.log`：原始已符号化调用栈。
- `.tools/xess-mfg-impl/rtss-dump-exception.log`：设备 QueryInterface 栈及驱动版本。
- `.tools/xess-mfg-impl/rtss-dump-callsite.log`：RTSS API ID、D3D12 间接调用及内层访问异常。
- `.tools/xess-mfg-impl/rtss-dump-marker.log`、`rtss-dump-marker-params.log`：调用帧及参数地址恢复。调试器的 `@rdi` 在该批命令中仍取原上下文，实际参数字节使用下面的显式地址读取结果。
- `.tools/xess-mfg-impl/rtss-dump-marker-data.log`：显式读取参数地址，确认 `0x5254535300000935`。
- `.tools/xess-mfg-impl/capture-20260914-152049/runtime/logs/magpie.log`：开启 RTSS 的原名进程，3×成功输出后发生切换期崩溃。
- `.tools/xess-mfg-impl/capture-20260914-152433/runtime/logs/magpie.log`：全局限帧影响的换名测试。
- `.tools/xess-mfg-impl/capture-20260914-152800/`：RTSS 退出后的完整倍率及组合测试。
- `C:/Program Files (x86)/RivaTuner Statistics Server/Help/Properties/General/REFLEX_SET_LATENCY_MARKER`：标记注入的官方说明；同目录的 `REFLEX_SLEEP`、`USE_DETOURS`、`SYNC_LIMITER` 为相关帮助。
- `C:/Program Files (x86)/RivaTuner Statistics Server/SDK/Samples/SharedMemory/RTSSSharedMemorySample/RTSSProfileInterface.h`：属性接口说明；`SDK/Tools/DesktopOverlayHost/RTSSHooksInterface.cpp` 为已加载模块的调用示例。

上述 `.tools` 路径相对于工作区 `C:/Users/81443/Documents/Magpie enhance`，为本机诊断产物，不随 Release ZIP 分发。
