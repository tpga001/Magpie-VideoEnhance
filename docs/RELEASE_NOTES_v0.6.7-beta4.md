# Magpie Experimental v0.6.7 Beta 4

保留 Beta 3 的全部功能，本次更新：

- 效果器选择左侧分类改为整行点击：图标、名称、数量和空白区域统一选中分类并切换展开／收起，悬停、按下和键盘焦点反馈覆盖整行。
- 修复旧帧被拒收后消费确认未推进、可能阻塞后续帧的问题；DLSS 插帧队列也会正确完成丢弃任务，避免重放旧画面。
- DLSSNR 皮肤结构强度范围调整为 `0–2`，默认值为 `0`；旧配置中的负值按 `0` 使用。

- 实时参数面板支持常驻预览与编辑切换：编辑时释放鼠标；单击游戏区域后返回游戏，本次点击不会执行游戏操作。预览时单击参数面板回到编辑，首击仅激活编辑，不改动参数或传给游戏；面板以外仍可正常操作游戏。
- 工具栏参数按钮与默认快捷键 `Alt+Shift+E` 统一控制面板开关：打开即进入编辑，显示时再次触发即关闭。移除额外关闭按钮，预览时参数按钮仍保持选中。
- 工具栏提示统一为“按钮名”或“按钮名（当前快捷键）”，快捷键改绑或清除后即时更新，不再附加操作说明。
- GPU 进程优先级固定请求实时；初始化及资源重建后读回确认，会话中每秒检查并恢复意外降级，失败会记录并重试。
- `Esc` 优先关闭下拉菜单／数值输入，然后按“编辑 → 预览 → 关闭”逐级退出；预览时的关闭按键仅在游戏／缩放窗口处于前台时接收。重新启用效果组后恢复面板位置和交互状态，切到其他应用时尊重新焦点。
- 主动停止与重建会等待参数层的本次输入结束，避免松开事件落入游戏。不同游戏的失焦、锁鼠与后台原始输入表现仍需分别实测。

- 修复删除效果组时可能闪退的问题。
- 效果组重命名增加重复检查，名称不区分大小写和首尾空格。
- 升级时发现旧配置中的同名效果组，会保留全部内容并提示逐个改名。
- 新建、复制和导入效果组时自动避免重名，导入的重复名称会添加序号。

---

# Magpie Experimental v0.6.7 Beta 4

Includes all Beta 3 features, with the following updates:

- Effect-picker categories now use one full-row button: the icon, name, count and padding select the category and toggle expansion together, with shared hover, press and keyboard focus feedback.
- Fixed missing consumption acknowledgement after rejecting a stale frame, which could block subsequent frames. DLSS frame-generation queues also complete dropped jobs without replaying an old image.
- DLSSNR Skin Structure Strength now ranges from `0–2`, with a default of `0`. Negative values in existing configurations are clamped to `0`.

- Live parameters can remain visible as a read-only preview. Editing releases the cursor; clicking the game returns control after consuming that complete click. Clicking the preview panel resumes editing; that first gesture only activates editing, without changing a parameter or reaching the game. Input outside the preview panel continues to reach the game.
- The parameter toolbar button and configurable `Alt+Shift+E` shortcut share one open/close action: opening enters editing, and invoking it while visible closes the panel. The extra close button is removed; the parameter button remains selected during preview.
- Toolbar tooltips show only the button name and its current shortcut, when assigned. Rebinding or clearing a shortcut updates the tooltip immediately; extra instructions are removed.
- GPU process priority always requests Realtime. Initialization and resource recreation verify the actual priority; active sessions check once per second and restore unexpected reductions, logging failures and retrying.
- Escape closes temporary controls first, then steps from Edit to Preview to Closed. Preview Escape is handled only while the game or scaling window is in the foreground. Effect-group restarts restore panel geometry and interaction state while respecting a switch to another app.
- Requested stops and restarts wait for outstanding parameter input to finish. Game-specific focus, confinement and background raw-input compatibility requires separate testing.

- Fixed a possible crash when deleting an effect group.
- Renaming now checks for duplicate names, ignoring capitalization and surrounding spaces.
- Existing groups with duplicate names are preserved during upgrades and marked for renaming.
- New, copied and imported groups receive unique names, with numbered suffixes added to duplicate import names.
