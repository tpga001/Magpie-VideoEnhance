# 原生超分后端隔离运行验证

日期：2026-09-28  
源码提交：`a957f8181338215e2498acbd58dbd98ddb95d193`  
显卡：NVIDIA GeForce RTX 3080  
候选包：`release/native-upscaler-lab-a957f81/Magpie-NativeUpscaler-Lab`

## 结论

DLSS SR、XeSS SR 与 FSR 3.1.5 SR 均已在独立候选包中编译，并通过真实 WGC 捕获、2× 原生超分、首帧呈现与至少 120 次后端发布验证。正式安装目录没有被替换，测试结束后已恢复原 Magpie 进程。

| 后端 | 输入 → 输出 | 初始化 | 首帧 | 持续处理 | 结果 |
|---|---:|---:|---:|---:|---:|
| DLSS SR | 938×524 → 1876×1048 | 成功 | 成功 | ≥120 次发布 | 通过 |
| XeSS SR | 938×524 → 1876×1048 | 成功 | 成功 | ≥120 次发布 | 通过 |
| FSR 3.1.5 SR | 938×524 → 1876×1048 | 成功 | 成功 | ≥120 次发布 | 通过 |

## 验证方式

测试使用同一个自动动态窗口、相同源尺寸、相同 2× 目标尺寸、WGC 捕获以及零运动向量配置。每个后端独立运行 8 秒。通过条件包括：

- 原生后端输出准确的初始化成功日志；
- 目标矩形为 `1876×1048`；
- 缩放窗口提交第一个前端帧；
- 日志出现 `Backend publication CPU timing: samples=120`；
- 不存在原生后端初始化失败或 `Native effect Draw failed`。

完整机器可读结果位于：

```text
E:\LocalConfig\magpie-universal-ai-screen-lab\.tools\native-super-resolution\capture-20260928-221620\capture-verification.json
```

## 关键日志

### DLSS SR

```text
DLSS SR initialized (Balanced, preset J): 938x524 -> 1876x1048
DLSS SR guidance frameId=1: requested motion=false, bound motion=zero depth=zero, fallback=none
Backend publication CPU timing: samples=120 ... fenceAvgMs=4.643
```

### XeSS SR

```text
XeSS experimental D3D11/D3D12 backend initialized (quality 101, Zero-MV, BGRA conversion=true, color=LDR UNORM): 938x524 -> 1876x1048
Backend publication CPU timing: samples=120 ... fenceAvgMs=1.915
```

### FSR 3.1.5 SR

```text
FSR 3.1.5 D3D11/D3D12 backend initialized (opticalFlow=false, virtual auxiliary inputs): 938x524 -> 1876x1048
Backend publication CPU timing: samples=120 ... fenceAvgMs=2.326
```

`fenceAvgMs` 是当前捕获链的 CPU 等待观测值，不是纯 GPU 算法耗时，也不能单独用来判断最终帧率或画质。

## 当前边界

- 本轮证明三个后端可以初始化并持续处理、呈现帧，没有比较主观画质。
- 输入是合成动态画面，还没有完成同一段 1080P 视频到 4K 的像素对照。
- 捕获式时序超分缺少游戏引擎提供的真实深度、抖动与运动向量，效果不能等同于游戏内原生集成。
- FSR4 与 FSR3 共用构建开关，但会在 RTX 3080 上使用实验性的 INT8 兼容路径，本轮没有启用运行测试。
- 候选包清单标记 `sourceDirty=true`，只用于本机实验，不能作为正式发布包。

## 下一步

使用同一段本地 1080P 视频，固定播放窗口、目标 3840×2160、播放器设置和截帧时间，依次对照：

1. 原始画面；
2. RTX VSR Ultra；
3. DLSS SR；
4. XeSS SR；
5. FSR 3.1.5 SR。

记录静态文字、人物面部、快速运动与遮挡区域的截图，同时采集实际呈现帧率、GPU 占用、显存和丢帧。完成这一步后再决定浏览器和本地视频默认采用哪个超分后端。

## 恢复核对

- 正式 Magpie：`E:\Program Files\Magpie-Experimental-x64\Magpie.exe`
- 正式 EXE SHA-256：`885575B133FAA3B031048335230BC37DC6AD5D8416CC0DB874A9482B05DD845A`
- `BuildOptions.props.user` 已按字节恢复到测试前内容。
