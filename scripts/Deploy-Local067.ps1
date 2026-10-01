#Requires -Version 7.0
param([string]$BuildEnvironmentProps, [switch]$BuildOnly)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$sourceRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$workspaceRoot = Split-Path $sourceRoot -Parent
$releaseRoot = Join-Path $workspaceRoot 'release'
$container = Join-Path $releaseRoot 'v0.6.7-local'
$destination = Join-Path $container 'Magpie-Experimental-x64'
$buildOutput = [IO.Path]::GetFullPath($destination)
$intermediateRoot = Join-Path $container 'obj'
$version = '0.6.7-local'

function Assert-MagpieClosed {
    if (Get-Process -Name Magpie -ErrorAction SilentlyContinue) {
        throw 'Exit Magpie from its tray menu, then rerun this deployment.'
    }
}

Assert-MagpieClosed
$commit = (& git -C $sourceRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE) { throw 'Read the source commit failed.' }
$dirty = @(& git -C $sourceRoot status --porcelain)
if ($LASTEXITCODE) { throw 'Read source status failed.' }
if (!$BuildOnly -and $dirty.Count) { throw 'Commit the reviewed source before deployment, or use -BuildOnly for local validation.' }
$shortCommit = $commit.Substring(0, 12)
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath |
    Select-Object -First 1
if (!$vs) { throw 'Install Visual Studio with the C++ desktop workload.' }
$msbuild = Join-Path $vs 'MSBuild/Current/Bin/amd64/MSBuild.exe'
$env:Path = (Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin') + ';' + $env:Path
if ($BuildEnvironmentProps) { $env:ForceImportBeforeCppTargets = (Resolve-Path -LiteralPath $BuildEnvironmentProps).Path }
$env:MSBUILDDISABLENODEREUSE = '1'

# Build in the existing runtime directory. Never clean/rebuild or mirror it:
# config, cache, logs, custom effects and local runtime additions belong to the user.
New-Item -ItemType Directory -Path $buildOutput, $intermediateRoot -Force | Out-Null
$statePath = Join-Path $container 'deployment-state.json'
[ordered]@{ status = 'building'; commit = $commit; sourceDirty = [bool]$dirty.Count } |
    ConvertTo-Json | Set-Content -LiteralPath $statePath -Encoding utf8
$outputArg = $buildOutput.Replace('\', '/') + '/'
$logPath = Join-Path $container 'build.log'
$buildArgs = @(
    'Magpie.slnx', '/m:2', '/nr:false', '/v:minimal', '/t:Build',
    '/p:Configuration=Release', '/p:Platform=x64',
    '/p:MajorVersion=0', '/p:MinorVersion=6', '/p:PatchVersion=7',
    "/p:VersionString=$version", "/p:CommitId=$shortCommit",
    '/p:PreferredToolArchitecture=x64', '/p:UseMultiToolTask=true',
    '/p:CL_MPCount=4', '/p:MultiProcMaxCount=4', '/p:EnforceProcessCountAcrossBuilds=true',
    '/p:DisablePDB=false', '/p:ReproducibleBuild=true', '/p:EnableFrameTrace=false',
    "/p:OutDir=$outputArg",
    ('/p:MagpieIntermediateRoot=' + $intermediateRoot.Replace('\', '/') + '/'),
    '/p:PreserveLocalRuntime=true'
)
Push-Location $sourceRoot
try {
    & $msbuild @buildArgs *> $logPath
    if ($LASTEXITCODE) { throw "Build failed; inspect $logPath" }
} catch {
    [ordered]@{ status = 'build_failed'; commit = $commit; sourceDirty = [bool]$dirty.Count; log = 'build.log' } |
        ConvertTo-Json | Set-Content -LiteralPath $statePath -Encoding utf8
    throw
} finally { Pop-Location }

foreach ($required in @('Magpie.exe', 'Magpie.pdb', 'resources.pri', 'Microsoft.UI.Xaml.dll', 'TouchHelper.exe', 'Updater.exe', 'effects')) {
    if (!(Test-Path -LiteralPath (Join-Path $buildOutput $required))) { throw "Incomplete build: $required" }
}
if ((Get-Item -LiteralPath (Join-Path $buildOutput 'Magpie.exe')).VersionInfo.FileVersion -ne $version) {
    throw 'The executable version does not match this deployment.'
}
if (!$BuildOnly -and ((& git -C $sourceRoot rev-parse HEAD).Trim() -ne $commit -or
    @(& git -C $sourceRoot status --porcelain).Count)) {
    throw 'Source changed during the build; rerun against the reviewed commit.'
}

Copy-Item -LiteralPath (Join-Path $sourceRoot 'LICENSE') -Destination (Join-Path $buildOutput 'LICENSE-Magpie.txt')
Copy-Item -LiteralPath (Join-Path $sourceRoot 'docs/experimental/testing/SCALING-PREFLIGHT.md') -Destination (Join-Path $buildOutput 'LOCAL-NOTES.md')
Copy-Item -LiteralPath (Join-Path $sourceRoot 'docs/experimental/testing/EFFECT-PICKER.md') -Destination (Join-Path $buildOutput 'EFFECT-PICKER.md')
Copy-Item -LiteralPath (Join-Path $sourceRoot 'docs/experimental/testing/PARAMETER-INPUT.md') -Destination (Join-Path $buildOutput 'PARAMETER-INPUT.md')
if ($BuildOnly) {
    [ordered]@{ status = 'built_for_validation'; commit = $commit; sourceDirty = [bool]$dirty.Count } |
        ConvertTo-Json | Set-Content -LiteralPath $statePath -Encoding utf8
    Write-Output "Built for validation: $destination"
    return
}
# Inventory only build/runtime artifacts, bundled licenses and exact source
# effect assets. User config/logs/custom effect files are never package inputs.
$files = @(Get-ChildItem -LiteralPath $buildOutput -File | Where-Object {
    $_.Extension -in @('.exe', '.dll', '.pri', '.pdb', '.map') -or
    $_.Name -in @('LICENSE-Magpie.txt', 'LOCAL-NOTES.md', 'EFFECT-PICKER.md',
        'AMD-FSR-SDK-THIRD-PARTY.md', 'AMD-FSR2-DX11-LICENSE.txt', 'INTEL-XESS-LICENSE.txt',
        'INTEL-XESS-THIRD-PARTY.txt', 'NVIDIA-DLSS-LICENSE.txt', 'NVIDIA-NVAPI-LICENSE.txt', 'NVIDIA-RTX-VIDEO-LICENSE.pdf')
})
$vfxLicenses = Join-Path $buildOutput 'NVIDIA-VFX-Licenses'
if (Test-Path -LiteralPath $vfxLicenses) { $files += @(Get-ChildItem -LiteralPath $vfxLicenses -File -Recurse) }
[xml]$effectProject = Get-Content -LiteralPath (Join-Path $sourceRoot 'src/Effects/Effects.vcxproj') -Raw
foreach ($item in $effectProject.Project.ItemGroup.CopyFileToFolders) {
    if (!$item -or !$item.Include) { continue }
    $source = Join-Path $sourceRoot ('src/Effects/' + $item.Include)
    $target = Join-Path $buildOutput ('effects/' + $item.Include)
    if (!(Test-Path -LiteralPath $target) -or (Get-FileHash -LiteralPath $source).Hash -ne (Get-FileHash -LiteralPath $target).Hash) {
        throw "Effect asset differs from source: $($item.Include)"
    }
    $files += Get-Item -LiteralPath $target
}
$records = @($files | Sort-Object FullName -Unique | ForEach-Object {
    [ordered]@{
        path = [IO.Path]::GetRelativePath($buildOutput, $_.FullName).Replace('\', '/')
        bytes = $_.Length
        sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
    }
})
Assert-MagpieClosed
# Retire only the eight exact built-in tier aliases after the direct build has
# been verified. Keep the actual installed bytes, including local edits.
$retiredRoot = [IO.Path]::GetFullPath((Join-Path $container ('retired-effects/' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fffffff'))))
$allowedRuntime = [IO.Path]::GetFullPath($destination) + [IO.Path]::DirectorySeparatorChar
$allowedRetired = [IO.Path]::GetFullPath((Join-Path $container 'retired-effects')) + [IO.Path]::DirectorySeparatorChar
foreach ($family in @('Denoise', 'VSR')) {
    foreach ($tier in @('Low', 'Medium', 'High', 'Ultra')) {
        $old = [IO.Path]::GetFullPath((Join-Path $destination "effects/RTXVideo/RTXVideo_${family}_${tier}.hlsl"))
        $backup = [IO.Path]::GetFullPath((Join-Path $retiredRoot "RTXVideo_${family}_${tier}.hlsl"))
        if (!$old.StartsWith($allowedRuntime, [StringComparison]::OrdinalIgnoreCase) -or
            !$backup.StartsWith($allowedRetired, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unexpected retired effect path.' }
        if (Test-Path -LiteralPath $old) {
            New-Item -ItemType Directory -Path $retiredRoot -Force | Out-Null
            Copy-Item -LiteralPath $old -Destination $backup
            if ((Get-FileHash -LiteralPath $old).Hash -ne (Get-FileHash -LiteralPath $backup).Hash) { throw 'Retired effect backup verification failed.' }
            Remove-Item -LiteralPath $old
        }
    }
}
# No directory-wide deletion or mirroring: retain local config, logs and diagnostics.
[ordered]@{
    schemaVersion = 1; version = $version; commit = $commit; sourceDirty = $false
    configuration = 'Release'; platform = 'x64'; deployedAtUtc = [DateTime]::UtcNow.ToString('o')
    files = $records
} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $destination 'build-manifest.json') -Encoding utf8
[ordered]@{ status = 'deployed'; commit = $commit; sourceDirty = $false } |
    ConvertTo-Json | Set-Content -LiteralPath $statePath -Encoding utf8
Write-Output "Deployed $version ($shortCommit): $destination"
