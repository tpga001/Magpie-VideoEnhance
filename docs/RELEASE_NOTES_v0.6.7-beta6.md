# Magpie Experimental v0.6.7 Beta 6

保留 Beta 5 的全部功能，本次更新：

- 修复全屏参数编辑遇到源窗口状态变化时，反复请求停止、刷日志或丢失待重建请求的问题；按住输入期间保留请求，释放后由外层循环完成。
- 为参数焦点交接增加有界稳定检查，短暂窗口变化恢复后继续呈现；持续变化、外部前台策略和源窗口关闭仍按规则处理。
- 源状态检查暂停时保留 DLSSFG 队列，避免队列被清空而待呈现计数和资源槽仍被占用。
- 区分 Reflex 请求、驱动实际低延迟状态、暂时暂停和调用失败；驱动报告关闭不再被误报为“不支持”。回退 Async 前清除独立驱动限帧。
- DLSSFG 选择 Reflex 时由驱动负责统一基础 FPS，每个基础候选在捕获前调用一次 Sleep，不叠加同目标 Async／StepTimer 限帧。Front Edge 兼容选项继续保留。
- 输出队列满时提前反馈背压，修正 FIFO 多次重试的等待统计，并为期限等待接入消息循环计时器。实时参数面板不显示同步、Reflex 或 FG 输出状态。Reflex 自动回退仅记简短日志，不弹提示或报错。

完整包为 `Magpie-Experimental-x64 v0.6.7-beta6.zip`。从托盘完全退出 Magpie，完整解压到新目录后运行；EXE 与 `resources.pri` 必须来自同一次构建。所有修正均属于 Beta 6，不另设 Fix 版本。

已通过相关无界面回归。首次全屏点击触发的具体源窗口变化、应用兼容性、实际显示间隔和性能收益仍需实机验证；本次未自动操作应用或游戏。Streamline 迁移和资源池并行化已完成初步评估，未直接替换现有 NGX 路径或删除资源同步等待。

---

# Magpie Experimental v0.6.7 Beta 6

Includes all Beta 5 features, with the following updates:

- Retains a single pending source-window transition while parameter input is held, then processes it in the outer message loop. This fixes repeated stop requests, log flooding and lost rebuild requests.
- Allows a bounded settling period for internal parameter-focus handoff. Temporary window changes can recover; persistent changes, source destruction and external foreground policy remain handled.
- Preserves DLSSFG FIFO ownership while source-state checks pause rendering, avoiding orphaned pending counts and resource slots.
- Distinguishes Reflex requests, actual driver activation, presentation pauses and API failures. A successful Off query no longer becomes an unsupported-driver error; Async fallback clears any independent driver limit.
- Selecting Reflex with DLSSFG now uses driver pacing for the unified base FPS, with one Sleep before capture per base candidate and no additional Async/StepTimer cap for the same target. The explicit Front Edge compatibility option remains available.
- Applies full-output-queue backpressure before the next base input, accumulates FIFO retry timings, and waits for presentation deadlines through the message loop. The live parameter panel does not display synchronization, Reflex or FG output status. Automatic Reflex fallback only writes a brief log entry, with no notification or error dialog.

Use the full `Magpie-Experimental-x64 v0.6.7-beta6.zip`. Fully exit Magpie from the tray and extract it into a new folder; keep the EXE and `resources.pri` from the same build together. All fixes belong to Beta 6, without a separate Fix version.

Relevant headless regressions passed. Full-application compatibility, the original fullscreen trigger, actual display intervals and performance improvements still require hands-on validation. This update did not automatically operate applications or games. Streamline migration and resource pooling were assessed; the existing NGX path and required synchronization remain in place.
