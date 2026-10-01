#Requires -Version 7.0
param([switch]$Check)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
$review = Get-Content -LiteralPath (Join-Path $repoRoot 'docs/experimental/reviews/20260908-effect-content-catalog.json') -Raw | ConvertFrom-Json
$familyReview = Get-Content -LiteralPath (Join-Path $repoRoot 'docs/experimental/reviews/20260908-effect-family-review.json') -Raw | ConvertFrom-Json
$familyMap = @{}
foreach ($family in $familyReview.families) {
    foreach ($sourceFamily in $family.sourceFamilies) {
        if ($familyMap.ContainsKey($sourceFamily)) { throw "Duplicate family assignment: $sourceFamily" }
        $familyMap[$sourceFamily] = $family
    }
}
$subDescriptions = @{
    '通用' = '用于照片、视频或游戏等常见画面，按性能余量选择。'
    '动画线条' = '重建动画、视觉小说中的轮廓和色块，留意纹理与线条变化。'
    '像素画' = '保留像素网格，或按偏好平滑像素游戏的轮廓。'
    '时序重建' = '利用连续捕获帧尝试恢复细节，运动与遮挡画面需要对比。'
    '缩小与尺寸整理' = '整理输出尺寸，适合缩小或多阶段尺寸转换。'
    '降噪' = '减轻颗粒、压缩噪声；逐步增加强度并观察细小纹理。'
    '去色带' = '平滑天空、阴影等渐变中的色阶分层。'
    '线条修复' = '整理动画轮廓、线条粗细或振铃。'
    '空间' = '根据当前画面平滑边缘锯齿，留意细节变软。'
    '时序' = '尝试利用连续帧平滑边缘，需比较运动中的重影与稳定性。'
    '通用锐化' = '增强局部对比与边缘清晰度，适合缩放后的收尾处理。'
    '补帧' = '生成中间帧，提高运动流畅度；一个效果组选择一种补帧方案。'
    '帧率调度' = '调节处理帧率；实际执行由运行时调度，与列表位置分开。'
    'CRT' = '模拟扫描线、遮罩、辉光等复古显示特征。'
    '图像调整' = '调整亮度、颜色或 Gamma，使画面更符合观看偏好。'
    'AI 画面重塑' = '通过 AI 改变纹理与画面风格，按实际观感选择。'
    '诊断' = '观察运动或中间处理结果，用于定位画面问题。'
}
$levels = @{ first_try = '优先尝试'; situational = '特定场景推荐'; advanced = '进阶／实验'; diagnostic = '调试用途' }
$categories = @($review.categories | ForEach-Object {
    [ordered]@{ id = $_.id; name = $_.name; description = $_.description
        subcategories = @($_.subcategories | ForEach-Object {
            if (!$subDescriptions.ContainsKey($_)) { throw "Missing subcategory description: $_" }
            [ordered]@{ name = $_; description = $subDescriptions[$_] }
        })
    }
})
$effects = @($review.effects | ForEach-Object {
    $effect = $_
    $category = $review.categories | Where-Object id -eq $effect.category
    $description = $effect.description
    $variant = $effect.variantNotes
    # Keep the product explanation about visible results; source evidence remains in the review.
    if ($effect.family -eq 'RTXVideo') {
        $description = if ($effect.category -eq 'cleanup') { '适合压缩噪声或颗粒明显的视频。可选择四档强度；逐步比较纹理保留与处理成本。' } else { '适合视频与串流内容的放大。可选择四档强度；结合源清晰度和性能余量比较。' }
        $variant = ''
    }
    if ($effect.id -eq 'DLSSNR\DLSSNR_AI_Filter') { $description += ' HDR 路径按输入颜色信息自动选择，亮度转换使用输入白点；画质与高光表现仍需在目标内容上比较。' }
    $lines = [Collections.Generic.List[string]]::new()
    $lines.Add($category.name + ' › ' + $effect.subcategory)
    $lines.Add($description)
    if ($variant) { $lines.Add('变体差异：' + $variant) }
    $lines.Add($levels[$effect.recommendation.level] + '：' + $effect.recommendation.reason)
    if ($effect.recommendation.conditions.Count) { $lines.Add('尝试建议：' + ($effect.recommendation.conditions -join '；')) }
    $pipeline = $effect.pipeline.suggestedStage
    if ($effect.pipeline.after.Count) { $pipeline += '；在「' + ($effect.pipeline.after -join '、') + '」之后' }
    if ($effect.pipeline.before.Count) { $pipeline += '；在「' + ($effect.pipeline.before -join '、') + '」之前' }
    $lines.Add('组合建议：' + $pipeline + '。' + $effect.pipeline.reason)
    if ($effect.category -eq 'frame') { $lines.Add('执行位置：补帧在呈现端处理，限帧由运行时扫描效果组；拖动条目不改变实际调度阶段。') }
    $lines.Add('输出尺寸：' + $effect.outputSize.summary)
    if ($effect.pipeline.constraints.Count) { $lines.Add('组合注意：' + ($effect.pipeline.constraints -join '；')) }
    if ($effect.requirements.Count) { $lines.Add('使用条件：' + ($effect.requirements -join '；')) }
    $lines.Add('性能成本：' + $effect.cost.qualitative + '；具体帧率需在目标画面实测。')
    if ($effect.id -eq 'DLSSNR\DLSSNR_AI_Filter') { $lines.Add('HDR 注意：自动使用输入颜色信息与白点适配；FP16 路径仍需对比高光、亮度与颜色变化。输入分辨率调整在此路径下不生效。') }
    elseif ($effect.hdr.showWarning) { $lines.Add('HDR 注意：' + $effect.hdr.warning) }
    $purposes = [Collections.Generic.List[string]]::new()
    $purposes.Add($effect.category)
    # Explicit secondary uses only. Never infer DLSSNR denoising from its internal name.
    if ($effect.id -eq 'ACNet' -or $effect.id -match 'Upscale_Denoise|_DN$') { if (!$purposes.Contains('cleanup')) { $purposes.Add('cleanup') } }
    if ($effect.id -match 'Anime4K_3D_AA_' -and !$purposes.Contains('antialiasing')) { $purposes.Add('antialiasing') }
    $search = @($effect.id, $effect.displayName, $effect.family, $effect.tags, $effect.purpose, $description, $category.name, $effect.subcategory) | ForEach-Object { $_ }
    $family = $familyMap[$effect.family]
    $subfamily = if ($family) { $family.subfamilies | Where-Object sourceFamily -eq $effect.family } else { $null }
    if ($family) { $search += $family.name }
    [ordered]@{
        id = $effect.id; name = $effect.displayName; category = $effect.category; subcategory = $effect.subcategory
        purposes = @($purposes); summary = $effect.purpose; details = $lines -join "`n`n"
        level = $effect.recommendation.level; recommendation = $levels[$effect.recommendation.level]
        search = $search -join ' '
        family = if ($family) { [ordered]@{ id = $family.id; name = $family.name; summary = $family.summary } } else { $null }
        subfamily = if ($subfamily) { [ordered]@{ id = $subfamily.id; name = $subfamily.name; summary = $subfamily.summary } } else { $null }
    }
})
# r1 publishes two parameterized effects. The original 161-file audit remains
# historical evidence; old names are retained only as search/migration aliases.
$rtxEntries = @($effects | Where-Object { $_.id -like 'RTXVideo\*' })
$effects = @($effects | Where-Object { $_.id -notlike 'RTXVideo\*' })
foreach ($family in @('Denoise', 'VSR')) {
    $aliases = @($rtxEntries | Where-Object { $_.id -like "RTXVideo\RTXVideo_${family}_*" })
    $entry = $aliases | Where-Object { $_.id -eq "RTXVideo\RTXVideo_${family}_Medium" } | Select-Object -First 1
    $entry.id = "RTXVideo\RTXVideo_$family"
    $entry.search = $entry.id + ' ' + (($aliases | ForEach-Object { $_.search }) -join ' ')
    $effects += $entry
}
# 068 merges the two historical XeSSFG entries; retain the original review as evidence.
$effects = @($effects | Where-Object { $_.id -ne 'XeSSFG\XeSS_MultiFrameGeneration_ZeroMV' })
$xess = $effects | Where-Object { $_.id -eq 'XeSSFG\XeSS_FrameGeneration_x2_ZeroMV' }
$xess.id = 'XeSSFG\XeSS_FrameGeneration'
$xess.name = 'XeSS_FrameGeneration'
$xess.summary = '在捕获的真实帧之间生成帧，可选 2×～4×。'
$xess.details = '默认 2×、AMD 光流、质量。旧 x2/MFG 配置自动合并到 XeSSFG，保留光流选择。' + "`n`n" +
    '2×使用原生路径；非 Intel 显卡上的 3×/4×自动启用多帧兼容，需要随附的匹配运行库。兼容路径仍处于实验验证阶段。' + "`n`n" +
    '3×/4×支持无光流或 AMD 光流；NVIDIA 光流目前用于 2×。同一效果组只使用一个补帧器。' + "`n`n" +
    '补帧由呈现端处理，列表位置不改变执行阶段。实际流畅度、延迟、遮挡边缘和游戏 HUD 需要结合目标画面比较。'
$xess.search = $xess.id + ' XeSSFG XeSS x2 MFG 补帧 多帧 AMD NVIDIA 光流 ' + $xess.summary
$xess.family.summary = '统一 XeSSFG，支持 2×～4×及可选光流；多帧兼容自动启用。'
$hdrComponents = Get-Content -LiteralPath (Join-Path $repoRoot 'src/Magpie/EffectCatalog/hdr-components.json') -Raw | ConvertFrom-Json
$categories += $hdrComponents.category
$effects += @($hdrComponents.effects)
$result = [ordered]@{ schemaVersion = 1; language = 'zh-Hans'; categories = $categories; effects = $effects } | ConvertTo-Json -Depth 8
$output = Join-Path $repoRoot 'src/Magpie/EffectCatalog/zh-Hans.json'
if ($Check) {
    if (!(Test-Path -LiteralPath $output) -or (Get-Content -LiteralPath $output -Raw).TrimEnd() -cne $result.TrimEnd()) { throw 'Effect catalog is stale. Run scripts/Generate-EffectCatalog.ps1.' }
    'Effect catalog matches the reviewed content.'
} else {
    New-Item -ItemType Directory -Path (Split-Path $output -Parent) -Force | Out-Null
    [IO.File]::WriteAllText($output, $result + "`n", [Text.UTF8Encoding]::new($false))
}
