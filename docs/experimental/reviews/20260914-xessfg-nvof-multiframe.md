# XeSSFG NVIDIA 光流多帧支持

## 结论与实现

此前 3×／4×与 NVIDIA 光流的组合被 `Renderer` 主动拒绝，原因是首期仅验证无光流和 AMD 光流，并没有已确认的 NVIDIA 多帧不兼容证据。本次移除这项倍率与光流方法的固定组合限制；保留运行库身份、实际最大倍率、NVIDIA 设备／驱动／网格质量能力及初始化错误检查。

NVIDIA 提供器比较当前捕获帧与上一捕获帧，将 S10.5 网格向量转换为逐像素 RG16F，方向为当前到上一帧。共享 Frame Guidance 再按消费者输出尺寸适配。XeSSFG 接收同一种运动纹理，不读取其 AMD/NVIDIA 来源；2×／3×／4×都按每个真实输入提交一次颜色、深度和运动向量。生成帧数不要求重复运行 NVIDIA 光流，也不改变运动向量方向或缩放单位。

共享 D3D11／D3D12 纹理沿用现有围栏与 Present 完成同步，不因开放 NVIDIA 3×／4×改变资源生命周期。截获图像的光流仍不是游戏引擎原生运动向量，平面深度、遮挡／UI／反射及高倍率节奏等现有限制继续适用。高质量光流会增加处理成本，不能保证提高最终显示 FPS。

参考：[Intel XeSS-FG 输入契约](https://github.com/intel/xess/blob/main/doc/xess_fg_developer_guide_english.md)、[NVIDIA 光流能力查询](https://docs.nvidia.com/video-technologies/optical-flow-sdk/nvofa-programming-guide/index.html)。上述资料没有将外部运动纹理的来源作为 3×／4×限制；非 Intel 多帧仍由本项目既有实验兼容路径启用，不能等同于 Intel 官方对该组合的认证。

## 验证

`Run-XeSSFGCaptureSmoke.ps1` 增加 `-OpticalFlow NVIDIA -NvidiaQuality 1..5`，支持真实 WGC 捕获、指定 NVOF 质量、2／3／4 倍率和 DLSSNR 组合。测试检查提供器确实初始化、未因失败退回零运动、SDK 输出比例以及有效外部运动提交数。日志新增 `motionFrames`，仅在实际 SDK 状态采样中累计有效且无需光流历史重置的外部运动输入；不表示每个像素都具有非零运动，也不是显示器显示事件。

测试输出在工作区 `.tools/xess-mfg-impl/capture-*/capture-verification.json`，本轮汇总在 `.tools/v068-nvof/`。实际通过／失败及未完成项写入分发目录审计记录，不以代码解除限制代替测试结果。既有 RTSS 重建期注入崩溃独立保留，不为本次测试修改 RTSS 全局设置。

## Draft 正文与标签

维护者要求保留其在线修改的中文，只删除已明确批准的过期光流限制句，再按中文同步英文；不新增 NVIDIA 多帧支持条目，也不恢复维护者已经删除的段落。

已推送的 `v0.6.8-experimental` 标签保持原提交。更新包使用新候选标签 `v0.6.8-experimental.1`，程序版本仍为 `0.6.8`；同一个 GitHub Draft 的目标与附件随之更新，保持 Draft／Pre-release，绝不公开发布。
