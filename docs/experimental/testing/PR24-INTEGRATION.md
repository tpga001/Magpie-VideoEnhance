# 0.6.7：PR #23 同步确认与 PR #24 集成

集成对象为 PR #24 的 `c91e8b0586037daf45a2cba6ecebb34f6ec0d9b2`，共同基点为 `ac1cc8b0`。本地错误引导改动先保存为 `3fe47d0f`，随后在独立工作目录的 `integration/pr24-hdr` 分支合入 PR，保留其原始提交和作者信息。

## PR #23 已同步

GitHub 确认 PR #23 已合并，合并提交为 `2d9f914f428e4f2f2e33dfc8dce9ddbf250f80cb`。已通过 Git 祖先检查确认该提交同时包含于本地 `0.6.7` 基点、远端 `origin/experimental` 及 PR #24 的提交历史。因此本轮无需重复合并 PR #23。

## 追加修复

| 问题 | 集成后的处理 |
| --- | --- |
| 工具链指向作者 D/E 盘，BuildRoot 无默认值 | 恢复标准 SDK/NuGet 工具发现、项目内 WinRT 投影及基于 SolutionDir 的输出和 Conan 路径；SDK 私有位置继续使用未跟踪的本机配置 |
| HDR→SDR 高光亮度反转，逆变换不能恢复 | CPU/HLSL 改用配对的单调对数变换，黑点为 0，峰值为 1，白点以上保留编码空间；R8 仍存在量化损失 |
| DLSS SR 在线性 HDR 输入下按 LDR 模式创建 | 根据有效 HDR 协议设置 NGX IsHDR 创建标志，关闭 HDR 时保持已有标志 |
| FG marker 误用 R8 SDR fallback | 效果链中的 marker 使用线性 FP16 交接；真正的 FG/presenter 终端保持独立合同，编译和运行时采用同一默认路由选择 |
| 自动 Bicubic 超出用户效果数组并混用 HDR/SDR 描述缓存 | 为附加效果使用独立配置，分开 HDR/SDR 编译缓存；HDR 编译、初始化、活动描述和 resize 统一使用 FP16 路由及实际上游尺寸 |
| HDR 手工创建后端未接入新的诊断捕获 | DLSS、FSR HDR 初始化失败保留 GPU、尺寸、失败步骤和系统码，与本地错误引导一致 |
| RTX Video 代理遗漏函数，完整构建无法链接 | 使用完整的官方 nvCVImageProxy 与官方 D3D11 声明，移除只实现部分函数的重复代理 |
| 版本文件指向贡献者 fork 的发行包 | 保留本仓库已有发行元数据；构建使用 0.6.7-dev，不把未发布的开发构建冒充正式发行 |

## 验证

- `scripts/Run-HdrRegression.ps1` 提供可重复执行的 Windows 回归入口，使用 Visual Studio x64 开发命令环境及已有 Conan fmt 依赖，不依赖厂商 GPU SDK。
- CPU：81 组白点、峰值、曝光及 shoulder 参数，每组 4,097 个采样点，合计 331,857 点；覆盖单调性、端点、连续性、往返、R8 保序和量化误差范围。
- WARP：直接提取并编译生产 HLSL，在 D3D11 软件设备上执行 54 组梯度与往返测试；包括真实 R8 纹理量化、CPU/HLSL 一致性、alpha 保留和 canonical 原样交接。
- 真实 R16 纹理的 640×360、1280×720 尺寸边界：Bicubic、DLSSFG 和 XeSSFG marker 均可准备为 DirectFP16，无额外 SDR 映射。
- Bicubic：提取生产 `_AppendBicubic` 函数，验证 SDR→HDR→SDR→HDR 切换的缓存隔离、编译格式、初始化边界及上游尺寸传递。
- 错误引导与持久化：文件占用、只读、解除限制后重试、旧保存结果乱序、配置恢复与备份保留，以及处理入口策略。
- 捕获生命周期：12 个场景、1,000 次重启循环、8 项集成顺序检查。

上述自动化验证通过。完整 x64 Release 构建（本机已配置的 DLSS/FG/NR、FSR、XeSS、光流和 RTX Video SDK）通过，最终退出码为 0。仓库一致性检查为 `FAIL=0 WARN=20 INFO=358`，未新增失败；警告来自保留的历史审查记录等既有内容。构建产物在独立集成工作目录的 `bin/x64/Release/`，本次没有启动它替换正在运行的程序。

## 验证边界

WARP 读回测试验证真实 shader 执行，不代表实体 HDR 显示器或 DLSS/FSR/XeSS/RTX Video SDK 的端到端画面验收。后续应分别检查 HDR 关闭的 SDR 回归、HDR 开启下的高光和颜色、窗口模式/resize、补帧，以及各厂商 SDK 组合。当前合入目标为本地开发分支。

HDR 经 R8 SDR 代理仍有量化损失；CPU 测试覆盖范围内误差小于峰值的 3.5%，该界限不是任意参数、任意中间效果的画质保证。
