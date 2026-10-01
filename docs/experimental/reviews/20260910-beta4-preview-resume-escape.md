# Beta 4：预览点击恢复编辑与 Esc 逐级退出

## 行为

- 参数按钮／参数快捷键继续控制开关：关闭时进入编辑，编辑或预览时关闭。
- 编辑中的 Esc 先退出下拉菜单或数值输入，随后退出到预览；游戏／缩放窗口前台时，预览中的普通 Esc 关闭面板。
- 单击预览面板的标题、控件或子区域进入编辑。首击只激活编辑，完整接收按下／松开，不修改参数，不交给游戏。
- 预览面板以外的游戏输入不被宿主覆盖。切到其他应用时隐藏预览命中窗口，不抢焦点或接管其他应用的 Esc。

## 实现

ImGui 预览继续使用 `NoInputs`。单独记录参数根窗口最后成功呈现的矩形，用同一个透明原生输入宿主覆盖这个范围，裁剪到渲染区域，以 `SWP_NOACTIVATE` 显示。仅实际点击时，通过 `WM_MOUSEACTIVATE` 激活，再扩展为编辑宿主；首击成对接收，松开后允许普通参数操作。关闭和停止会隐藏宿主；失败呈现不更新点击范围。

现有缩放线程键盘钩子只在预览及本会话前台时接管普通 Esc 的首次按下，随后配对接收重复和松开。松开只记录关闭请求，由外层循环执行。已按住的键、带修饰键的系统操作和外部应用的键不接管；切出应用或变更面板状态使待关闭请求失效。

相关 Win32 约定：[WM_MOUSEACTIVATE](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-mouseactivate)、[SetCapture](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setcapture)、[LowLevelKeyboardProc](https://learn.microsoft.com/en-us/windows/win32/winmsg/lowlevelkeyboardproc)。

## 验证

`tests/Run-ParameterInputTests.ps1 -NativePrototype` 已通过：从当前生产代码提取的输入宿主、ImGui 后端和状态转换回归，以及真实 USER32/DWM 原型。覆盖标题／控件／子区域激活、越界松开、首击不改参数、面板外输入、失败呈现、Esc 长按／完整配对／外部焦点／旧状态失效、停止等待、数值输入和 16 种视口／DPI 布局。

原生原型验证透明宿主只覆盖预览面板，显示时不激活，点击时激活并完整接收首击。此验证不替代 `PARAMETER-INPUT.md` 中的实际游戏、捕获与补帧验收矩阵。
