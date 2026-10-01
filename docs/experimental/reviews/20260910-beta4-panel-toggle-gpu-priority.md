# Beta 4：统一参数面板开关与 GPU 优先级检查

## 参数面板

工具栏参数按钮和可配置快捷键（默认 Alt+Shift+E）都调用 `OverlayAction::EffectParameters`，进入同一个 `_ToggleParameterPanel`：面板关闭时打开并编辑；编辑或预览时关闭。工具栏选中状态来自可见性，预览时仍选中。移除额外的工具栏关闭按钮和标题栏 X，更新英／简中／繁中说明及快捷键名称。

关闭沿用输入宿主的配对交接：仍按住修饰键或鼠标时先记住关闭请求，接收到松开并消费输入后才归还游戏焦点。Esc／游戏区域单击继续返回预览，不与明确的面板开关动作混用。

## GPU 优先级

全源码检索只有 Renderer 的一处进程 GPU 优先级写入，原值已是 `D3DKMT_SCHEDULINGPRIORITYCLASS_REALTIME`，未发现主动降级调用。原实现位于前端 D3D 设备初始化之后、SDK 和后端初始化之前，只检查设置返回值，没有读回，也没有会话内复查。不能据此指认某个 SDK 实际修改了优先级。

现在 `_EnsureGpuPriority` 在原初始化位置执行，并在后端完成全部初始化、资源 resize、DLSSFG 失败恢复后强制检查。活跃后端循环最多每秒做一次普通检查，普通滑条拖动不强制增加查询。实际值不为 REALTIME 或首次查询失败时，只请求 REALTIME，随后读回确认；验证失败不会标记成功，没有降为 High/Normal 的回退。重复失败日志去重，后续继续重试，恢复后重新允许记录错误。

检查状态在创建后端前由前端使用，启动后由后端独占，避免跨线程竞争；没有新增轮询线程、等待或 CPU 进程优先级修改。停止时不写低优先级恢复值。

这是读回与恢复机制，不是阻止其他进程、驱动或 SDK 调用 Windows API 的权限锁。普通检查间隔为一秒；后端阻塞时要等循环恢复才能检查，系统拒绝请求时也无法保证实际为实时，因此日志必须保留失败结果。

API 依据：[Microsoft 设置接口](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmthk/nf-d3dkmthk-d3dkmtsetprocessschedulingpriorityclass)、[读取接口](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmthk/nf-d3dkmthk-d3dkmtgetprocessschedulingpriorityclass)及[实时枚举](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmthk/ne-d3dkmthk-_d3dkmt_schedulingpriorityclass)。

## 验证与范围

- 生产代码提取的 ImGui 输入回归覆盖统一开关、编辑／预览关闭、关闭时等待修饰键松开；原输入、首击配对、拖动、焦点和 16 组布局回归通过。
- `Run-GpuPriorityTests.ps1` 编译生产检查函数，通过 REALTIME-only 写入、读回验证、1000 次周期内调用无额外查询、降级恢复、强制检查、拒绝写入、虚假成功、查询失败、日志去重和恢复测试。仅替换 Windows 调用及时间，不修改实际系统 GPU 优先级。
- 检查时 Magpie 已无运行进程，未取得旧二进制的即时优先级读数。未加载游戏／SDK 做实际切屏验证。
- 使用现有 Release SDK 编译参数对 Renderer.cpp、OverlayDrawer.cpp、ParameterInputHost.cpp 执行 `/Zs /Y-` 语法检查通过，不输出二进制、不写运行包。
- 按用户要求仅修改、验证和提交源码，不启动程序、不部署、不更新现有发布包。
