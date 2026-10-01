# Beta 6：全屏参数交接与 DLSSFG 调度修复

本文保留 Beta 6 初始实现记录；后续驱动限帧及静默回退以 [当前实现](20260910-beta6-reflex-pacing.md) 为准，均属于 Beta 6。

基线：Beta 5 `ef538ffc`。实施日期：2026-09-10。

## 全屏参数交接

源窗口状态变化现在写入一个持久的待处理请求，由缩放线程外层消息循环执行。按键／鼠标未释放时保留请求，停止优先于重定位；首次检测记录具体原因、前台和输入宿主 HWND、窗口矩形、可见性及最小化状态，后续等待不重复排队或刷日志。

参数输入宿主进入和退出编辑时设置 100 ms 稳定期，仅在会话内部窗口仍持有前台时有效。短暂位置／尺寸／可见性变化不会覆盖源窗口基线；恢复后继续呈现，持续变化按原有重建规则处理。源窗口销毁不等待稳定期。3D 模式仅额外允许稳定期内的缩放窗口前台，外部前台仍受原策略约束。

源状态检查暂停期间保留 DLSSFG FIFO。真正销毁时先发布生产取消，再清理队列和 Renderer，避免仅清空 FIFO 却遗留待呈现计数和槽位。

这些改动修复源码中已确认的重试、请求丢失和队列失配路径。Beta 5 首次触发的具体窗口变化已被日志轮转覆盖，仍须新版本实机复现确认；100 ms 是有界交接窗口，不代表已覆盖所有应用的焦点行为。

## Reflex 与基础限帧

- 原生驱动边界分别返回 SetSleepMode、GetSleepStatus 状态，以及是否执行查询、实际低延迟状态。成功查询 Off 不再转换为 `NVAPI_NOT_SUPPORTED`。
- 控制器分别表示接口不可用、Active、DriverOff、Paused、Faulted 和 Stopped。DriverOff 保留接口，在呈现恢复或配置改变时重新查询，避免逐帧强制启用；UI 显示最近一次配置查询结果，不持续轮询控制面板。
- 驱动低延迟未启用时，若已请求普通 Reflex 限帧，则先清除该驱动限帧再回退 Async。请求和状态可能不同、低延迟与限帧彼此独立，这是 [NVAPI 文档](https://docs.nvidia.com/nvapi/group__dx.html) 明确区分的行为。
- DLSSFG 选择 Reflex 时，基础 FPS 由捕获前 Async 调度负责；低延迟请求 On、Boost Off，额外驱动间隔保持 0。显式 Front Edge 兼容选项和已保存默认值保留。普通非 FG Reflex 与 XeLL 的限帧职责不变。
- Sleep 仍每个基础捕获候选最多一次，捕获等待保留候选，生成帧不新增 Sleep。颜色、引导及 NGX 的基础帧 ID 保持一致，生成帧／真实帧使用独立递增 Present ID。

## 队列与时间含义

输出队列满时，后端在接受下一基础帧前等待消费通知；资源槽事件仍负责最终发布许可。停止会唤醒这一等待。等待结束后重新进入基础调度，避免在等待前固定处理起点。

每个 FIFO 任务携带独立计时，跨多次重试累计期限等待、容量重试、其他资源重试和实际尝试的 CPU 时间。完成和丢弃均结算，旧资源代次的任务不得释放当前槽位或减少当前待呈现计数。期限等待已接入外层计时器；交换链容量就绪不会使其忙轮询。

| 观测 | 含义与边界 |
| --- | --- |
| `CaptureAccepted` 的时间戳字段 | 捕获接口提供的源时间戳；无有效时间戳的捕获方式可为 0，不能冒充游戏引擎时间 |
| `BackendRender` | Magpie 实际开始处理的 CPU 区间，包含其内部子阶段；不能与嵌套事件直接相加 |
| `FgQueued`／`FgDequeued` | 按帧 ID、槽位和资源代次关联的发布入队／FIFO 终止事件；丢弃事件需结合呈现记录区分 |
| `Present`／`PresentGap` | Present 调用及调用间隔，不等于屏幕扫描输出间隔 |
| `paceWait`、`capacityRetry`、`resourceRetry` | 相应重试之间经过的墙钟时间，期间可能处理输入消息，不是纯内核睡眠时间 |
| `cpuWall`、`beginFrame`、`draw`、`endFrame` | 前端尝试和子阶段的 CPU 墙钟；子阶段包含在总量中 |
| `fifoAge` | 前端处理通知后到任务终止的时间，包含排队、重试和工作；不包含通知到达消息队列但尚未处理的时间 |
| `GenerationFence`／生成 fence 统计 | 每张插值图等待其 SDK 输出及共享 allocator 安全复用的 CPU 等待 |
| `PublicationFence` | D3D11 发布复制完成的 CPU 等待，保护下一次生成输出复用 |

`CaptureFrameCadence` 仍是接受帧时刻扣除已知下游队列等待的调度估计，不是源应用原生帧率。新的捕获前背压也计入下游等待，避免把它再次反馈到输出节奏。

普通 Release 只有低频汇总，不新增逐帧文字日志和诊断像素回读。生成 fence 细分汇总需 `EnableNativeBackendTiming`，有界事件记录需 `EnableFrameTrace`；SDK 要求的插值禁用标志读取继续保留。后续使用真实显示间隔工具另行验证画面节奏，不以平均 FPS 宣称性能改进。

## 验证

完整 Release x64 构建已通过。首次严格构建检出的 `std::exchange` 整数收窄已改为同类型零值，后续构建无编译错误。
`FrameTrace.cpp`、`DLSSFrameGenerator.cpp` 和 `Renderer.cpp` 另以真实 Release SDK 参数开启 FrameTrace／NativeBackendTiming 完成语法检查，验证诊断分支可编译。

- `tests/Run-Beta6RegressionTests.ps1`：提取生产源状态、3D 前台检查和 FIFO 呈现函数。覆盖 1000 次按住重试、释放重建、短暂恢复、持久变化、源关闭、停止优先级；确定性时钟验证多次期限／容量／资源重试只结算一次、旧代次隔离及 2×／3×／4× 队列背压。
- `tests/Run-ReflexControllerTests.ps1`：生产控制器的 14 个驱动失败点、Off 查询、清除驱动限帧、暂停恢复、同候选一次 Sleep、生成／真实帧身份及并发 Sleep／Present／Stop。
- `tests/Run-FrameSyncModesTests.ps1`：模式解析、生产 StepTimer、生产限帧配置、单一限帧所有者、DLSSFG 倍率及配置恢复。
- `tests/Run-ParameterInputTests.ps1`：实际 ImGui 控件、工具栏及热键路线、输入宿主切换、拖动／下拉／数值输入和 16 组布局恢复。未开启 NativePrototype。
- `tests/Run-DlssR2ResourceTests.ps1`：WARP 软件设备下 25 组 NR 输出、96 次 FG 状态缓冲复用和 fence 完成判定，在普通／详细计时配置下均通过；不代表实际 NVIDIA 硬件性能。

仓库一致性检查没有新增资源键或文档链接错误；现有唯一 FAIL 是未改动的 `docs/FAQ (EN).md` 中 `About%20touch%20support.md` 的链接校验，其余历史文档／翻译警告仍在。

实机全屏点击、游戏兼容性及性能对比尚未执行，保留在 Beta 6 TODO 中。Streamline 评估见[独立报告](20260910-beta6-streamline-assessment.md)。
