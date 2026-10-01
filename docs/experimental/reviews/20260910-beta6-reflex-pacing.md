# Beta 6：DLSSFG Reflex 驱动基础限帧

日期：2026-09-10。基线：原 Beta 6 `5796b01d`。包版本：`0.6.7-beta6`。

## 完成状态

| 维度 | 状态 | 证据边界 |
| --- | --- | --- |
| 代码实现 | 完成 | Reflex 请求进入真正驱动基础限帧路径；不再映射为 Async |
| 自动验证 | 无界面回归与完整 Release x64 构建通过 | 生产解析／控制器／限帧配置、反向测试、Beta 6 FIFO、WARP；不等于真实驱动行为 |
| 真实 NVIDIA 驱动行为 | 待验证 | 未自动启动或操作 Magpie、游戏及其他应用 |
| 性能收益 | 未验证 | 不预先宣称帧率、延迟或显示稳定性改善 |

## 唯一基础限帧器

生产解析器将 DLSSFG 的 Reflex 请求解析为 Reflex。Renderer 合并手动／自动基础目标与更低的配置、捕获上限，然后传入 `SetSleepMode.minimumIntervalUs`。驱动接受期间 StepTimer 的固定上限为空；Front Edge 的处理后 FG 输入等待仍只在 Front Edge 分支运行。

捕获前先保留 FIFO 容量和资源背压，然后每个基础候选调用一次 Sleep。捕获返回 Waiting 时保留同一候选，接受新输入后使用同一基础 ID 关联捕获、颜色、引导、NGX 与渲染。生成输出和真实输出共享基础 ID，使用递增且独立的 Present ID；异步 FG／前端标记、生成帧优先顺序、输出期限、槽位回收与 fence 均保留。

## 单位契约

核对材料：本地 DLSS Frame Generation SDK 310.7.0 编程指南第 7 节（应用负责输出顺序和间隔）、[NVIDIA 直接 NGX SDK](https://github.com/NVIDIA/DLSS)、[NVAPI SetSleepMode／Sleep／异步标记接口](https://docs.nvidia.com/nvapi/group__dx.html)。当前集成直接调用 NGX 和 NVAPI，没有经过 Streamline 的选项转换层。

驱动间隔按一个基础捕获／SIMULATION 周期计算：`ceil(1,000,000 / 有效基础 FPS)`。生成 Present 使用 OUT_OF_BAND 标记且不新增 Sleep。此单位选择来自当前基础周期与异步输出的代码对应关系；厂商文档没有为本项目的跨 D3D11／D3D12 自定义呈现提供实测保证，硬件最终行为仍列为验收项，不将推导写成实机结论。

| 请求与环境 | 2× | 3× | 4× |
| --- | --- | --- | --- |
| 手动基础 80 FPS | 12500 µs | 12500 µs | 12500 µs |
| 0 自动，240 Hz | 基础 120；8334 µs | 基础 80；12500 µs | 基础 60；16667 µs |
| 0 自动，144 Hz | 基础 72；13889 µs | 基础 48；20834 µs | 基础 36；27778 µs |
| 更低捕获上限 30 FPS | 33334 µs | 33334 µs | 33334 µs |

倍率仅在自动目标中使用一次；刷新率变化重新计算，不将先前解析的自动值当成固定上限反馈回去。实际源帧率不足时允许低于目标，不能重复旧输入填满目标。

## 驱动状态与回退

低延迟状态（Unavailable／Active／DriverOff／Paused／Faulted／Stopped）与限帧状态（Clear／Configuring／Active／CleanupFailed）独立。SetSleepMode 成功代表接受配置；GetSleepStatus 只报告低延迟状态。查询成功 Off 保留有效驱动限帧，也保留 Sleep／标记；不会伪造 API 错误或逐帧强制请求 On。

发生真实调用失败，先进入 Configuring，禁止 Async 接管；随后请求低延迟 Off、interval=0。清除 Set 成功后才发布 Clear，Renderer 使用同一基础目标回退 Async。清除 Set 失败则发布 CleanupFailed，阻止继续捕获，仅记简短日志并停止本轮缩放；不重试刷日志，不声称干净回退。清除后的查询错误单独记录，不能据此声称已经成功清除的间隔仍然存在。

控制器保守记录是否曾尝试设置非零应用间隔。仅请求过 0 的会话没有应用限帧残留：驱动初始化不支持时，即使 Off 清理也不支持，仍可正常回退 Async；不会把低延迟接口不可用误报为限帧残留。已尝试过非零间隔时，必须等清零 Set 成功后才允许回退；即使非零 Set 返回错误也按可能部分生效处理。该区别已加入故障测试。

DXGI／DComp 暂停恢复通过相同事务清除与恢复最新配置。配置代次使暂停、目标变化期间遗留的捕获候选失效，恢复后重新 Sleep 并使用单调 ID。源重建／参数重启创建新的 Renderer 和控制器；故障会话只在重建后重试。没有将同目标的 Async 调度藏在 Reflex 模式中。

Sleep 不持有配置互斥锁，驱动对象在后端线程退出前保持存活；生产队列等待保持可取消和有界消息等待。并发测试证明控制器的 Present／Stop 不被 Sleep 所占的应用锁阻塞。NVAPI 没有供应用取消正在执行的同步 Sleep 的接口；驱动自身不返回的异常无法由此测试证明可中断，仍是实际驱动验收边界。

## UI 与兼容性

按维护者要求，实时参数面板不显示当前同步方式、Reflex 请求／查询状态、回退状态或 FG 输出状态。保留模式选择、目标 FPS 与效果参数。Reflex 自动回退仅记录简短日志，不调用 Toast 或错误报告；清除失败保留内部停止处理，也不弹窗。已删除对应状态展示接口和英／简中／繁中资源。

本项属于 `0.6.7-beta6` 的迭代，不另设 Fix 版本或发布说明。

显式 Async 继续在捕获前限基础 FPS，Front Edge 保留原输入期限行为，帧同步关闭时保留已有上限；这些 DLSSFG 模式的应用驱动间隔为 0，低延迟仍独立请求。普通非 FG Reflex 采用相同独立状态修正；XeSSFG／MFG 仍由 XeLL 接管。默认 Front Edge 60 FPS 和已有配置值不变。

## 自动验证与后续实机矩阵

- `Run-ReflexControllerTests.ps1`：生产控制器的 2×／3×／4× ID、同候选一次 Sleep、14 类 API 失败、Off 查询、清除前阻止回退／清除失败阻止捕获、暂停恢复、并发 Sleep／Present／Stop。
- `Run-FrameSyncModesTests.ps1`：生产模式解析、StepTimer、Renderer 实际限帧配置和实际 ActiveFrameSyncBackend 方法，与生产 ReflexController 共同运行；手动／自动目标、144／240 Hz、配置／捕获上限、暂停恢复、清除失败。受控修改生产解析器使 DLSS Reflex 回到 Async，以及恢复 Reflex 同目标 StepTimer 上限，两项都必须编译成功且以测试失败码退出。
- `Run-Beta6RegressionTests.ps1`：1000 次按住输入重试、持久源状态请求、停止优先级、FIFO 期限／容量／资源重试统计、旧代次隔离、槽位释放和 2×／3×／4× 背压。
- `Run-DlssR2ResourceTests.ps1`：WARP 软件设备，25 个 NR 输出／别名场景、96 次 FG 持久读回，两种计时编译配置。

用户明确同意后，在相同场景、源上限、基础目标和倍率下比较 Reflex／Async，覆盖源 FPS 高低变化、GPU 满载、静止恢复、全屏／窗口、HDR、捕获方式和 DXGI／DComp 切换。记录真实基础帧率、生成有效率、排队帧龄、捕获后延迟与显示间隔；API 成功、Present 次数和自动测试不能代替显示测量。提交前完整构建为 0 error，包版本与清单哈希已核对；提交后的完整重建、资源与逐文件核对结果保存在随包 `LOCAL-PREPARATION-AUDIT.json`，并提供 `SHA256SUMS.txt`。源码和旧包保留，未创建 GitHub Draft 或发布。仓库一致性检查为已有 FAQ 编码空格链接失败 1 项、历史警告 47 项，没有新增失败。
