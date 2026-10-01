#Requires -Version 7.0
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
& (Join-Path $repoRoot 'scripts/Generate-EffectCatalog.ps1') -Check
$catalog = Get-Content -LiteralPath (Join-Path $repoRoot 'src/Magpie/EffectCatalog/zh-Hans.json') -Raw | ConvertFrom-Json
$englishCatalogText = Get-Content -LiteralPath (Join-Path $repoRoot 'src/Magpie/EffectCatalog/en-US.json') -Raw
if ($englishCatalogText -match '[\u3400-\u9fff]') { throw 'Chinese text remains in the English catalog.' }
$englishCatalog = $englishCatalogText | ConvertFrom-Json
$effectRoot = Join-Path $repoRoot 'src/Effects'
$ids = @(Get-ChildItem -LiteralPath $effectRoot -Filter '*.hlsl' -Recurse -File | ForEach-Object { [IO.Path]::GetRelativePath($effectRoot, $_.FullName).Replace('.hlsl', '') })
if (@(Compare-Object $ids @($catalog.effects.id)).Count -or @($catalog.effects.id | Group-Object | Where-Object Count -gt 1).Count) { throw 'Catalog and installed IDs differ or contain duplicates.' }
if (@(Compare-Object @($catalog.effects.id) @($englishCatalog.effects.id) -SyncWindow 0).Count) { throw 'English catalog effect IDs or ordering differ.' }
if (@(Compare-Object @($catalog.categories.id) @($englishCatalog.categories.id) -SyncWindow 0).Count) { throw 'English catalog category IDs or ordering differ.' }
for ($i = 0; $i -lt $catalog.categories.Count; ++$i) {
    if (@($catalog.categories[$i].subcategories).Count -ne @($englishCatalog.categories[$i].subcategories).Count) { throw 'English catalog subcategory structure differs.' }
}
for ($i = 0; $i -lt $catalog.effects.Count; ++$i) {
    $source = $catalog.effects[$i]
    $translated = $englishCatalog.effects[$i]
    foreach ($field in @('id','category','level')) {
        if ($source.$field -ne $translated.$field) { throw "English catalog changed structural field $field for $($source.id)" }
    }
    if (($source.purposes -join '|') -ne ($translated.purposes -join '|') -or
        $source.family.id -ne $translated.family.id -or $source.subfamily.id -ne $translated.subfamily.id) {
        throw "English catalog changed classification metadata for $($source.id)"
    }
    foreach ($field in @('name','summary','details','category','search')) {
        if (!$translated.$field) { throw "Missing English $field for $($source.id)" }
        if ($translated.$field -match '[\u3400-\u9fff]') { throw "Chinese text remains in English $field for $($source.id)" }
    }
}
foreach ($entry in $catalog.effects) {
    foreach ($field in @('name','summary','details','category','search')) { if (!$entry.$field) { throw "Missing $field for $($entry.id)" } }
    if ($entry.category -notin $catalog.categories.id) { throw 'Unknown category' }
    foreach ($family in @($entry.family, $entry.subfamily)) {
        if ($family -and (!$family.id -or !$family.name -or !$family.summary)) { throw "Incomplete family: $($entry.id)" }
    }
    if ($entry.subfamily -and !$entry.family) { throw 'Subfamily without a parent' }
}
$dlssnr = $catalog.effects | Where-Object id -eq 'DLSSNR\DLSSNR_AI_Filter'
if ($dlssnr.category -ne 'style' -or $dlssnr.purposes -contains 'cleanup' -or $dlssnr.summary -match '降噪') { throw 'DLSSNR purpose regression' }
$dlss = $catalog.effects | Where-Object id -eq 'DLSS\DLSS_SR'
if ($dlss.category -ne 'upscale' -or $dlss.subcategory -ne '时序重建' -or $dlss.name -ne 'DLSS SR' -or
    $dlss.purposes -contains 'antialiasing' -or $dlss.details -notmatch 'J' -or $dlss.details -notmatch 'L／M') { throw 'DLSS SR classification regression' }
foreach ($id in @('FSR2\FSR2_SR', 'FSR3\FSR3_SR', 'FSR4\FSR4_SR', 'XeSS\XeSS_SR')) {
    $peer = $catalog.effects | Where-Object id -eq $id
    if ($peer.category -ne $dlss.category -or $peer.subcategory -ne $dlss.subcategory) { throw 'Temporal SR entries are in different categories' }
}
$rtx = @($catalog.effects | Where-Object id -in @('RTXVideo\RTXVideo_Denoise','RTXVideo\RTXVideo_VSR'))
if ($rtx.Count -ne 2 -or @($rtx.name | Sort-Object -Unique).Count -ne 2) { throw 'RTX Video grouping regression' }
foreach ($entry in $rtx) {
    $file = Join-Path $effectRoot ($entry.id + '.hlsl')
    if ((Get-Content -LiteralPath $file -Raw) -notmatch '(?s)//!DEFAULT 1.*//!OPTION 0 Low.*//!OPTION 3 Ultra.*int strength;') { throw 'RTX strength metadata regression.' }
    foreach ($tier in @('Low','Medium','High','Ultra')) { if (!$entry.search.Contains($entry.id + '_' + $tier)) { throw 'Missing legacy search alias' } }
}
if (@($catalog.effects | Where-Object { $_.id -like 'XeSSFG\*' -and $_.name -like '*ZeroMV*' }).Count) { throw 'XeSS display alias regression' }
$cunny = @($catalog.effects | Where-Object { $_.family.id -eq 'cunny' })
if ($cunny.Count -ne 29 -or @($cunny.subfamily.id | Sort-Object -Unique).Count -ne 2) { throw 'CuNNy generation grouping regression' }
if (@($catalog.effects | Where-Object { $_.family.id -eq 'nnedi3' }).Count -ne 10) { throw 'NNEDI3 family regression' }
if (@($catalog.effects | Where-Object { $_.id -match '^(CRT|Sharpen|Diagnostics|RTXVideo)\\' -and $_.family }).Count) { throw 'Mixed algorithms were merged into a family' }
"Catalog validated: $($ids.Count) source effects, $($catalog.effects.Count) built-in picker entries; eight RTX names retained as aliases."
