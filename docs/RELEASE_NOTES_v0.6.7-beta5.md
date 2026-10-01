# Magpie Experimental v0.6.7 Beta 5

保留 Beta 4 的全部功能，本次更新：

- 预览面板的激活点击直接交给参数控件：单击开关、打开下拉菜单或按住滑条拖动，无需再点一次。
- 进入编辑前恢复上一帧面板的输入标记和有效命中范围，保留点击时的屏幕坐标，避免状态切换清空命中信息或窗口位置变化丢失首击。
- 统一缩放窗口和参数输入宿主的事件路由，修复面板外点击漏掉“编辑 → 预览”、面板内点击漏掉“预览 → 编辑”的路径；以点击消息的坐标判断区域。
- 实时参数快捷键改由已注册的系统热键触发窗口激活；区分打开时继承的修饰键和实际收到的按键，避免修饰键已松开却一直等待，无法退出编辑。
- 预览面板的命中处理优先于 3D 锁鼠和游戏坐标映射，使光标进入面板范围时能够接收激活点击。
- 保留 `Esc` 的“编辑 → 预览 → 关闭”逐级退出，以及参数按钮／快捷键的面板开关行为。

无界面回归覆盖真实工具栏按钮、两条窗口消息路由、延迟鼠标坐标、修饰键松开、预览捕获交接及首击控件操作，并通过恢复旧行为的反向测试确认能检出缺陷。实际应用和游戏兼容性仍需实测确认。

---

# Magpie Experimental v0.6.7 Beta 5

Includes all Beta 4 features, with the following updates:

- The click that activates the preview panel also operates the parameter control: toggle a checkbox, open a dropdown or start dragging a slider without clicking again.
- Restores the panel's previous-frame input flags and valid hit bounds before editing, and preserves the original screen coordinates so activation does not lose the first press when input state is cleared or the host moves.
- Routes scaling-window and parameter-host events through the same state transitions, including outside clicks returning to Preview and preview clicks entering Edit. Hit testing uses the click message's coordinates.
- Uses the registered system hotkey to trigger parameter-window activation and tracks inherited modifiers separately from delivered key presses, so released modifiers cannot leave editing stuck waiting for a missing release.
- Handles the preview target before 3D confinement and game-coordinate mapping, allowing the native cursor to receive activation clicks inside the panel.
- Keeps Escape's Edit → Preview → Closed sequence and the parameter toolbar button/shortcut's open-close behavior.

Headless regressions cover the actual toolbar button, both window-message routes, delayed mouse coordinates, inherited modifier release, preview capture handoff and first-click controls. Negative controls restore old behavior and confirm the tests detect the defects. Full-application and game compatibility still require hands-on validation.
