# Universal AI Screen：Phase 0 本机验证与实现边界

## 本阶段结论

本机 RTX 3080、NVIDIA 驱动 `32.0.16.1692`（616.92）已经证明可以创建并执行 DLSSNR Feature 18。成立条件是运行库本身包含 `sm_86` 可执行映像；本次验证不需要 NVAPI 架构伪装。

因此第一版实现采用以下路线：

1. Magpie 继续作为捕获、效果链、窗口和呈现宿主。
2. DLSSNR 先增加独立进程能力探针和显式运行时选择，不把架构钩子并入主进程。
3. GPUPixel 后续以 GPU 纹理效果接口接入，不复用 PotPlayer DirectShow 桥中的 CPU/OpenGL 回读路径。
4. DLSS Frame Generation 继续作为独立终端阶段；不能由本次 NR 成功推导为 RTX 30 可用。

## 已核验环境

| 项目 | 值 |
|---|---|
| GPU | NVIDIA GeForce RTX 3080 10 GiB |
| 驱动 | 616.92 |
| Windows build | 26200.9457 |
| Magpie | 0.6.8，提交 `3841698348bfb246623d4acf791984c8b68a577b` |
| GPUPixel PotPlayer Bridge | 0.4.60 |
| 当前分支 | `feature/universal-ai-screen-phase0` |

## 运行库鉴定

已安装 Magpie 的两个运行库均不含 Ampere 内核：

| 文件 | SHA-256 | CUDA 架构 | RTX 3080 判断 |
|---|---|---|---|
| 已安装 `nvngx_dlssnr.dll` 310.8 | `984BEE0F775C277D5829B8FD6775D53A7B0F75396C852B3AAF06A18375F81014` | `sm_89`, `sm_120` | 缺少 `sm_86` |
| 已安装 `nvngx_dlssg.dll` 310.7 | `135EAF0733C1E37381A8C28ABCF7A862404A54132B81787C04E35D09EFC5E36F` | `sm_89`, `sm_120` | 缺少 `sm_86` |

隔离下载的 NeuralScreen v1.15.0 发行归档：

- 归档 SHA-256：`C49B49E567F7B631C4F05BA71DF93EC0DA31B5E1EF3C3855A99E25BD2FA986B9`。
- 版本记录提交：`d9e78dcb71d54c4b1863cd12f66bdca8381122e1`。
- 归档内 `SHA256SUMS`、`runtime-manifest.json` 与抽取文件逐项匹配。
- NR 候选 `nvngx_dlssnr.dll` SHA-256 为 `DCC0DC2414AEDEC4A8E084647070383BE068554042587180C20C784D4772D36F`，包含 `sm_75/sm_86/sm_89/sm_120`。
- NR 候选与已安装 310.8 DLL 的 55 个导出名称完全一致。
- FG 候选 310.9.1 仍只有 `sm_89/sm_120`，不能用于 RTX 3080。
- NR 候选的 Authenticode 状态为 `HashMismatch`。归档内部哈希匹配只能证明下载包内一致，不能替代厂商签名验证；不得静默替换正式安装文件。

## 独立 NR 探针结果

探针使用独立短生命周期进程、独立 D3D12 设备、三张确定性的 640×360 RGBA8 帧和零 RG16F motion。探针不打开 Desktop Duplication，也不创建呈现窗口。

### 对照 A：架构伪装开启

```text
GPU: RTX 3080 (30xx, arch 0x170)
NVSDK_NGX_D3D12_Init -> Success
patch installed: architecture 0x170
direct feature 18 ready
compat: pass 3/3
```

### 对照 B：架构伪装关闭

```text
GPU: RTX 3080 (30xx, arch 0x170)
NVSDK_NGX_D3D12_Init -> Success
direct feature 18 ready
compat: pass 3/3
```

两个对照均通过。当前驱动与该 `sm_86` 运行库组合不需要架构伪装。第一版若加入钩子只会增加主进程修改、缓存键和故障恢复复杂度，没有本机证据支持。

探针还暴露一个上游缓存问题：`NS_ARCH_SPOOF=0/1` 产生了同一个 compatibility key。Magpie 的能力键必须包含所有会改变 Create/Evaluate 行为的开关；不能让不同执行路径复用 PASS。

## Magpie 真链路验收状态

新增 `tests/Run-DLSSNRCaptureSmoke.ps1`，行为如下：

1. 按 `build-manifest.json` 将正式包复制到独立便携目录。
2. 只在副本中替换候选 `nvngx_dlssnr.dll`。
3. 写入独立 profile 和 scaling mode，不读取用户配置。
4. 编译并启动动画窗口，等待实际 WGC 缩放。
5. 检查 `Feature=18 created=true`，并拒绝 Create/Evaluate 失败或 pass-through。
6. 正常结束测试进程，输出 `capture-verification.json`。

当前自动运行停在第 4 步：测试会话中 `SetForegroundWindow` 返回失败，`GetForegroundWindow` 为空，Magpie 的自动缩放条件没有触发。隔离日志显示应用和便携配置已正常加载，没有进入 DLSSNR 初始化。因此这次失败不能解释为候选运行库失败；需要在可取得真实交互前台的会话中重跑。

## 代码架构落点

### `RuntimeCandidate`

运行时候选记录至少包含：

- 绝对路径、文件大小、SHA-256、版本资源；
- Authenticode 状态与签名者指纹；
- CUDA fatbin 架构集合；
- PE 导出集合及预期接口版本；
- 来源和许可说明；
- 探针输入合同与探针结果。

运行时不得只因文件出现在某个目录就自动启用。激活记录必须绑定候选哈希和完整能力键。

### `RuntimeCapabilityKey`

能力键至少包含：

- Magpie 版本和探针版本；
- worker、运行库与转发层哈希；
- GPU LUID、架构、驱动版本；
- Windows build；
- 输入/输出格式、尺寸和 Feature；
- 所有影响加载、钩子、转发和参数设置的开关。

只有 Create 成功、预期数量的 Evaluate 全部成功、输出格式/尺寸/字节数匹配时才能保存 PASS。超时、崩溃、TDR、SEH、pass-through 和未知结果均不能成为 PASS。

### `IFrameEffect`

DLSSNR 与 GPUPixel 属于单输入、单输出效果：

```cpp
struct FrameEffectContext {
    ID3D11Texture2D* input;
    ID3D11Texture2D* output;
    FrameId frameId;
    ColorSpace colorSpace;
    FrameGuidanceView guidance;
};

class IFrameEffect {
public:
    virtual EffectResult Process(const FrameEffectContext&) noexcept = 0;
};
```

GPUPixel 的 Magpie 后端应直接消费/产生 GPU 纹理。PotPlayer Bridge 0.4.60 继续独立维护，它的 DirectShow、RGBA/ABGR 与异步 4K 行为只作为参数和画质参考。

### `IFrameGenerator`

Frame Generation 保持终端多帧发布接口：

```cpp
class IFrameGenerator {
public:
    virtual GeneratorResult Submit(const GeneratorInput&) noexcept = 0;
    virtual std::span<const GeneratedFrame> ReadyFrames() const noexcept = 0;
};
```

它不进入普通 effect 链，也不以 NR 探针结果解锁。RTX 3080 当前默认使用 XeSSFG；DLSSFG 只有在独立运行时包含 `sm_86`、Feature 可创建且连续输出验证通过后才显示为可选。

## 下一实现顺序

1. 将 fatbin、签名、导出、哈希检查整理为仓库内只读资格检查工具。
2. 实现独立 `Magpie.RuntimeProbe`，先支持 DLSSNR 640×360 三帧合同。
3. 增加显式候选选择与 hash-gated 激活记录；正式包内 DLL仍作为回退。
4. 在可取得前台焦点的会话中运行 `Run-DLSSNRCaptureSmoke.ps1`，补齐 WGC 真链路证据。
5. 再开始 GPUPixel GPU 纹理适配层；先做 BGRA/RGBA、方向、alpha 和色彩空间机械测试。
6. FG 独立推进，不阻塞 NR 与 GPUPixel 效果链。

## Phase 0 验收

- 已安装 Magpie、GPUPixel 和用户配置未被覆盖。
- 正式 Magpie 在测试后已从原路径恢复启动。
- NR 的 RTX 3080 可执行性已由 Create + 3/3 Evaluate 证明。
- 架构伪装在当前组合中没有必要。
- FG 仍未通过 RTX 3080 能力门禁。
- 真 WGC 捕获验收脚本已落到 fork 分支，但本次前台会话无法触发自动缩放，状态明确保留为未完成。
