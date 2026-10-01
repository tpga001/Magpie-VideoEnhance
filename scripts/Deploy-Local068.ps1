#Requires -Version 7.0
param([string]$BuildEnvironmentProps, [switch]$BuildOnly, [switch]$IncludeLocalVideo)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$sourceRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$workspaceRoot = Split-Path $sourceRoot -Parent
$releaseRoot = Join-Path $workspaceRoot 'release'
$container = Join-Path $releaseRoot 'v0.6.8-local'
$destination = Join-Path $container 'Magpie-Experimental-x64'
$buildOutput = [IO.Path]::GetFullPath($destination)
$intermediateRoot = Join-Path $container 'obj'
$version = '0.6.8-local'

function Assert-MagpieClosed {
    if (Get-Process Magpie -ErrorAction SilentlyContinue |
        Where-Object { $_.Path -eq (Join-Path $destination 'Magpie.exe') }) {
        throw 'Exit Magpie from its tray menu, then rerun this deployment.'
    }
}

function Invoke-NativeProcessLogged(
    [string]$FilePath,
    [string[]]$Arguments,
    [string]$WorkingDirectory,
    [string]$LogPath
) {
    $startInfo = [Diagnostics.ProcessStartInfo]::new($FilePath)
    foreach ($argument in $Arguments) { [void]$startInfo.ArgumentList.Add($argument) }
    $startInfo.WorkingDirectory = $WorkingDirectory
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    # Codex currently supplies both PATH and Path. MSBuild's C++ ToolTask uses
    # a case-sensitive table and otherwise fails before launching CL.exe.
    $pathValue = $startInfo.Environment['PATH']
    [void]$startInfo.Environment.Remove('PATH')
    $startInfo.Environment.Add('Path', $pathValue)
    $process = [Diagnostics.Process]::Start($startInfo)
    $stdout = $process.StandardOutput.ReadToEndAsync()
    $stderr = $process.StandardError.ReadToEndAsync()
    $process.WaitForExit()
    [IO.File]::WriteAllText($LogPath, $stdout.Result + $stderr.Result)
    return $process.ExitCode
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
$conan = Get-Command conan.exe -ErrorAction SilentlyContinue
if (!$conan) {
    $conan = Get-ChildItem -LiteralPath (Join-Path $env:APPDATA 'Python') -Filter conan.exe `
        -File -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
}
if (!$conan) { throw 'Conan was not found. Install Conan 2 before building Magpie.' }
$conanPath = if ($conan.PSObject.Properties['Source']) { $conan.Source } else { $conan.FullName }
$env:Path = (Split-Path $conanPath -Parent) + ';' + $env:Path
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
    '/p:MajorVersion=0', '/p:MinorVersion=6', '/p:PatchVersion=8',
    "/p:VersionString=$version", "/p:CommitId=$shortCommit",
    '/p:PreferredToolArchitecture=x64', '/p:UseMultiToolTask=true',
    '/p:CL_MPCount=4', '/p:MultiProcMaxCount=4', '/p:EnforceProcessCountAcrossBuilds=true',
    "/p:ConanExecutable=$($conanPath.Replace('\', '/'))",
    '/p:DisablePDB=false', '/p:ReproducibleBuild=true', '/p:EnableFrameTrace=false',
    "/p:OutDir=$outputArg",
    ('/p:MagpieIntermediateRoot=' + $intermediateRoot.Replace('\', '/') + '/'),
    '/p:PreserveLocalRuntime=true'
)
Push-Location $sourceRoot
try {
    $buildExitCode = Invoke-NativeProcessLogged $msbuild $buildArgs $sourceRoot $logPath
    if ($buildExitCode) { throw "Build failed; inspect $logPath" }

    $dotnetHome = Join-Path $intermediateRoot 'dotnet-home'
    $nugetPackages = Join-Path $intermediateRoot 'nuget-packages'
    $hostPublish = Join-Path $intermediateRoot 'BrowserVideoHost/publish'
    $env:DOTNET_CLI_HOME = $dotnetHome
    $env:NUGET_PACKAGES = $nugetPackages
    $env:DOTNET_SKIP_FIRST_TIME_EXPERIENCE = '1'
    & dotnet publish (Join-Path $sourceRoot 'src/BrowserVideoHost/BrowserVideoHost.csproj') `
        -c Release --self-contained false -p:RestoreIgnoreFailedSources=true -o $hostPublish *> `
        (Join-Path $container 'browser-video-host-build.log')
    if ($LASTEXITCODE) { throw 'Browser video native host build failed.' }
} catch {
    [ordered]@{ status = 'build_failed'; commit = $commit; sourceDirty = [bool]$dirty.Count; log = 'build.log' } |
        ConvertTo-Json | Set-Content -LiteralPath $statePath -Encoding utf8
    throw
} finally { Pop-Location }

foreach ($required in @('Magpie.exe', 'Magpie.pdb', 'resources.pri', 'Microsoft.UI.Xaml.dll', 'TouchHelper.exe', 'Updater.exe', 'effects')) {
    if (!(Test-Path -LiteralPath (Join-Path $buildOutput $required))) { throw "Incomplete build: $required" }
}
$hostPublish = Join-Path $intermediateRoot 'BrowserVideoHost/publish'
foreach ($hostFile in @(
    'Magpie.BrowserVideoHost.exe', 'Magpie.BrowserVideoHost.dll',
    'Magpie.BrowserVideoHost.deps.json', 'Magpie.BrowserVideoHost.runtimeconfig.json'
)) {
    Copy-Item -LiteralPath (Join-Path $hostPublish $hostFile) -Destination $buildOutput
}
if (!(Test-Path -LiteralPath (Join-Path $buildOutput 'Magpie.BrowserVideoHost.exe'))) {
    throw 'Incomplete build: Magpie.BrowserVideoHost.exe'
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
Copy-Item -LiteralPath (Join-Path $sourceRoot 'docs/experimental/effects/DLSSNR-MULTIPASS.md') -Destination (Join-Path $buildOutput 'DLSSNR-MULTIPASS.md')
Copy-Item -LiteralPath (Join-Path $sourceRoot 'docs/experimental/effects/XESSFG.md') -Destination (Join-Path $buildOutput 'XESSFG.md')
Copy-Item -LiteralPath (Join-Path $sourceRoot 'docs/experimental/design/XESSFG-COMPATIBILITY-NOTICE.md') -Destination (Join-Path $buildOutput 'XESSFG-COMPATIBILITY-NOTICE.md')
Copy-Item -LiteralPath (Join-Path $sourceRoot 'scripts/Select-BrowserVideoRegion.ps1') -Destination (Join-Path $buildOutput 'BrowserVideoRegionPicker.ps1')
Copy-Item -LiteralPath (Join-Path $sourceRoot 'scripts/Select-BrowserVideoRegion.cmd') -Destination (Join-Path $buildOutput 'Select-BrowserVideoRegion.cmd')
Copy-Item -LiteralPath (Join-Path $sourceRoot 'scripts/Install-BrowserVideoAuto.ps1') -Destination (Join-Path $buildOutput 'Install-BrowserVideoAuto.ps1')
Copy-Item -LiteralPath (Join-Path $sourceRoot 'scripts/Install-BrowserVideoAuto.cmd') -Destination (Join-Path $buildOutput 'Install-BrowserVideoAuto.cmd')
$extensionOutput = Join-Path $buildOutput 'BrowserVideoExtension'
New-Item -ItemType Directory -Path $extensionOutput -Force | Out-Null
Get-ChildItem -LiteralPath (Join-Path $sourceRoot 'browser-extension') -File |
    Copy-Item -Destination $extensionOutput -Force
$localVideoFiles = @('MagpieLocalVideoTap.ax', 'Magpie.LocalVideoHost.exe', 'Magpie.LocalVideoHost.dll',
    'Magpie.LocalVideoHost.deps.json', 'Magpie.LocalVideoHost.runtimeconfig.json', 'libyuv-LICENSE.txt')
if ($IncludeLocalVideo) {
    & (Join-Path $PSScriptRoot 'Build-LocalVideo.ps1')
    foreach ($name in $localVideoFiles) {
        Copy-Item -LiteralPath (Join-Path $sourceRoot "output/local-video/build/bin/$name") -Destination $buildOutput -Force
    }
}
# Inventory only build/runtime artifacts, bundled licenses and exact source
# effect assets. User config/logs/custom effect files are never package inputs.
$files = @(Get-ChildItem -LiteralPath $buildOutput -File | Where-Object {
    $_.Name -notin $localVideoFiles -and (
    $_.Extension -in @('.exe', '.dll', '.pri', '.pdb', '.map') -or
    $_.Name -in @('LICENSE-Magpie.txt', 'LOCAL-NOTES.md', 'EFFECT-PICKER.md', 'DLSSNR-MULTIPASS.md', 'PARAMETER-INPUT.md', 'XESSFG.md', 'XESSFG-COMPATIBILITY-NOTICE.md',
        'BrowserVideoRegionPicker.ps1', 'Select-BrowserVideoRegion.cmd',
        'Install-BrowserVideoAuto.ps1', 'Install-BrowserVideoAuto.cmd',
        'Magpie.BrowserVideoHost.deps.json', 'Magpie.BrowserVideoHost.runtimeconfig.json',
        'AMD-FSR-SDK-THIRD-PARTY.md', 'AMD-FSR2-DX11-LICENSE.txt', 'INTEL-XESS-LICENSE.txt',
        'INTEL-XESS-THIRD-PARTY.txt', 'NVIDIA-DLSS-LICENSE.txt', 'NVIDIA-NVAPI-LICENSE.txt', 'NVIDIA-RTX-VIDEO-LICENSE.pdf'))
})
if ($IncludeLocalVideo) { $files += @($localVideoFiles | ForEach-Object { Get-Item -LiteralPath (Join-Path $buildOutput $_) }) }
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
$files += @(Get-ChildItem -LiteralPath $extensionOutput -File -Recurse)
$records = @($files | Sort-Object FullName -Unique | ForEach-Object {
    [ordered]@{
        path = [IO.Path]::GetRelativePath($buildOutput, $_.FullName).Replace('\', '/')
        bytes = $_.Length
        sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
    }
})
if ($BuildOnly) {
    [ordered]@{
        schemaVersion = 1; version = $version; commit = $commit; sourceDirty = [bool]$dirty.Count
        configuration = 'Release'; platform = 'x64'; builtAtUtc = [DateTime]::UtcNow.ToString('o')
        files = $records
    } | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $destination 'build-manifest.json') -Encoding utf8
    [ordered]@{ status = 'built_for_validation'; commit = $commit; sourceDirty = [bool]$dirty.Count } |
        ConvertTo-Json | Set-Content -LiteralPath $statePath -Encoding utf8
    Write-Output "Built for validation: $destination"
    return
}
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
foreach ($name in @('XeSS_FrameGeneration_x2_ZeroMV', 'XeSS_MultiFrameGeneration_ZeroMV')) {
    $old = [IO.Path]::GetFullPath((Join-Path $destination "effects/XeSSFG/$name.hlsl"))
    $backup = [IO.Path]::GetFullPath((Join-Path $retiredRoot "$name.hlsl"))
    if (!$old.StartsWith($allowedRuntime, [StringComparison]::OrdinalIgnoreCase) -or
        !$backup.StartsWith($allowedRetired, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unexpected retired XeSSFG path.' }
    if (Test-Path -LiteralPath $old) {
        New-Item -ItemType Directory -Path $retiredRoot -Force | Out-Null
        Copy-Item -LiteralPath $old -Destination $backup
        if ((Get-FileHash -LiteralPath $old).Hash -ne (Get-FileHash -LiteralPath $backup).Hash) { throw 'Retired XeSSFG backup verification failed.' }
        Remove-Item -LiteralPath $old
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
