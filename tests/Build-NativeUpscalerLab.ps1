#Requires -Version 7.0
param(
    [string]$InstalledRuntime = 'E:\Program Files\Magpie-Experimental-x64',
    [string]$ReleaseDirectory = 'native-upscaler-lab-a957f81',
    [string]$DLSSRuntimeDll = '',
    [string]$DLSSLicenseFile = '',
    [ValidateRange(1, 8)][int]$MaxCpuCount = 2,
    [ValidateRange(1, 8)][int]$MaxCompilerProcesses = 2
)

$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$workspace = Split-Path $repo -Parent
$dependencies = Join-Path $workspace 'dependencies'
$xessSdk = Join-Path $dependencies 'XeSS-SDK-3.0.1'
$fsrSdk = Join-Path $dependencies 'FSR-SDK-v2.3.0'
$rtxVideoSdk = Join-Path $dependencies 'RTX-Video-SDK-1.1.0'
$toolsRoot = Join-Path $workspace '.tools/native-upscaler-lab'
$dlssSdk = Join-Path $toolsRoot 'dlss-sdk-shim'
$userOptions = Join-Path $repo 'src/BuildOptions.props.user'
$hadUserOptions = Test-Path -LiteralPath $userOptions
$userOptionsBytes = if ($hadUserOptions) { [IO.File]::ReadAllBytes($userOptions) } else { $null }
$resolvedDlssRuntime = if ($DLSSRuntimeDll) {
    [IO.Path]::GetFullPath($DLSSRuntimeDll)
} else {
    Join-Path $InstalledRuntime 'nvngx_dlss.dll'
}
$resolvedDlssLicense = if ($DLSSLicenseFile) {
    [IO.Path]::GetFullPath($DLSSLicenseFile)
} else {
    Join-Path $InstalledRuntime 'NVIDIA-DLSS-LICENSE.txt'
}

foreach ($required in @(
    (Join-Path $xessSdk 'inc/xess/xess.h'),
    (Join-Path $xessSdk 'lib/libxess.lib'),
    (Join-Path $fsrSdk 'Kits/FidelityFX/api/include/ffx_api.h'),
    (Join-Path $fsrSdk 'Kits/FidelityFX/signedbin/amd_fidelityfx_upscaler_dx12.dll'),
    (Join-Path $rtxVideoSdk 'include/nvsdk_ngx.h'),
    (Join-Path $rtxVideoSdk 'lib/Windows/x64/nvsdk_ngx_s.lib'),
    $resolvedDlssRuntime,
    $resolvedDlssLicense
)) {
    if (!(Test-Path -LiteralPath $required)) { throw "Required native-upscaler dependency is missing: $required" }
}

# The locally available RTX Video SDK contains the common NGX headers and import
# library used by this integration, while the previously validated runtime owns
# the DLSS redistributable. Assemble them in the directory layout expected by
# the existing project without changing either source SDK directory.
New-Item -ItemType Directory -Path `
    (Join-Path $dlssSdk 'include'), `
    (Join-Path $dlssSdk 'lib/Windows_x86_64/x64'), `
    (Join-Path $dlssSdk 'lib/Windows_x86_64/rel') -Force | Out-Null
Get-ChildItem -LiteralPath (Join-Path $rtxVideoSdk 'include') -File |
    Copy-Item -Destination (Join-Path $dlssSdk 'include') -Force
Copy-Item -LiteralPath (Join-Path $rtxVideoSdk 'lib/Windows/x64/nvsdk_ngx_s.lib') `
    -Destination (Join-Path $dlssSdk 'lib/Windows_x86_64/x64/nvsdk_ngx_s.lib') -Force
Copy-Item -LiteralPath $resolvedDlssRuntime `
    -Destination (Join-Path $dlssSdk 'lib/Windows_x86_64/rel/nvngx_dlss.dll') -Force
Copy-Item -LiteralPath $resolvedDlssLicense `
    -Destination (Join-Path $dlssSdk 'LICENSE.txt') -Force

$conan = Get-Command conan.exe -ErrorAction SilentlyContinue
if (!$conan) {
    $conan = Get-ChildItem -LiteralPath (Join-Path $env:APPDATA 'Python') -Filter conan.exe `
        -File -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
}
if (!$conan) { throw 'Conan was not found.' }
$conanPath = if ($conan.PSObject.Properties['Source'] -and $conan.Source) {
    $conan.Source
} else {
    $conan.FullName
}

$temporaryOptions = @"
<?xml version="1.0" encoding="utf-8"?>
<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup>
    <ConanExecutable>$([Security.SecurityElement]::Escape($conanPath))</ConanExecutable>
    <EnableDLSSSR>true</EnableDLSSSR>
    <DLSSSdkDir>$([Security.SecurityElement]::Escape($dlssSdk))</DLSSSdkDir>
    <EnableFSR3ZeroMV>true</EnableFSR3ZeroMV>
    <EnableAmdOpticalFlow>true</EnableAmdOpticalFlow>
    <FSR3SdkDir>$([Security.SecurityElement]::Escape($fsrSdk))</FSR3SdkDir>
    <EnableXeSSZeroMV>true</EnableXeSSZeroMV>
    <EnableXeSSFrameGeneration>true</EnableXeSSFrameGeneration>
    <XeSSSdkDir>$([Security.SecurityElement]::Escape($xessSdk))</XeSSSdkDir>
    <RtxVideoSdkDir>$([Security.SecurityElement]::Escape($rtxVideoSdk))</RtxVideoSdkDir>
  </PropertyGroup>
</Project>
"@

try {
    [IO.File]::WriteAllText($userOptions, $temporaryOptions, [Text.UTF8Encoding]::new($false))
    # Codex supplies both PATH and Path. MSBuild's C++ Exec task treats the
    # environment table as case-sensitive, so launch the release script in a
    # child with one normalized Path entry, matching the proven local deploy flow.
    $buildLog = Join-Path $toolsRoot 'build-console.log'
    New-Item -ItemType Directory -Path $toolsRoot -Force | Out-Null
    $startInfo = [Diagnostics.ProcessStartInfo]::new('pwsh.exe')
    foreach ($argument in @(
        '-NoProfile', '-File', (Join-Path $repo 'scripts/Build-Release.ps1'),
        '-PackageName', 'Magpie-NativeUpscaler-Lab',
        '-ReleaseDirectory', $ReleaseDirectory,
        '-Version', '0.6.8-native-upscaler-lab',
        '-MaxCpuCount', "$MaxCpuCount",
        '-MaxCompilerProcesses', "$MaxCompilerProcesses",
        '-AllowDirtySource', '-RequireMagpieClosed'
    )) { [void]$startInfo.ArgumentList.Add($argument) }
    $startInfo.WorkingDirectory = $repo
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $pathValue = $startInfo.Environment['Path']
    if (!$pathValue) { $pathValue = $startInfo.Environment['PATH'] }
    [void]$startInfo.Environment.Remove('PATH')
    $startInfo.Environment['Path'] = (Split-Path $conanPath -Parent) + ';' + $pathValue
    # MSBuild imports environment variables as global properties before the
    # generated _ConanDeps project is evaluated.
    $startInfo.Environment['ConanExecutable'] = $conanPath
    $startInfo.Environment['MSBUILDDISABLENODEREUSE'] = '1'
    $process = [Diagnostics.Process]::Start($startInfo)
    $stdout = $process.StandardOutput.ReadToEndAsync()
    $stderr = $process.StandardError.ReadToEndAsync()
    $process.WaitForExit()
    $buildOutput = $stdout.Result + $stderr.Result
    [IO.File]::WriteAllText($buildLog, $buildOutput)
    if ($process.ExitCode) {
        $tail = ($buildOutput -split "`r?`n" | Select-Object -Last 120) -join "`n"
        throw "Native-upscaler release build failed; inspect $buildLog`n$tail"
    }
    Write-Output "Build log: $buildLog"
} finally {
    if ($hadUserOptions) {
        [IO.File]::WriteAllBytes($userOptions, $userOptionsBytes)
    } elseif (Test-Path -LiteralPath $userOptions) {
        Remove-Item -LiteralPath $userOptions
    }
}

$runtime = Join-Path $workspace "release/$ReleaseDirectory/Magpie-NativeUpscaler-Lab"
$manifestPath = Join-Path $runtime 'build-manifest.json'
if (!(Test-Path -LiteralPath $manifestPath)) { throw 'Native-upscaler lab manifest was not generated.' }
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
foreach ($option in @('EnableDLSSSR', 'EnableFSR3ZeroMV', 'EnableXeSSZeroMV', 'EnableXeSSFrameGeneration')) {
    if ($manifest.featureOptions.$option -ne 'true') { throw "Manifest did not record $option=true" }
}
foreach ($required in @(
    'Magpie.exe', 'nvngx_dlss.dll', 'libxess.dll',
    'amd_fidelityfx_loader_dx12.dll', 'amd_fidelityfx_upscaler_dx12.dll'
)) {
    if (!(Test-Path -LiteralPath (Join-Path $runtime $required))) {
        throw "Built native-upscaler runtime is incomplete: $required"
    }
}

$result = [ordered]@{
    passed = $true
    runtime = $runtime
    commit = $manifest.commit
    sourceDirty = $manifest.sourceDirty
    featureOptions = $manifest.featureOptions
    magpieSha256 = (Get-FileHash -LiteralPath (Join-Path $runtime 'Magpie.exe')).Hash
    dlssRuntime = @{
        source = $resolvedDlssRuntime
        version = (Get-Item -LiteralPath $resolvedDlssRuntime).VersionInfo.FileVersion
        sha256 = (Get-FileHash -LiteralPath $resolvedDlssRuntime -Algorithm SHA256).Hash
        signature = (Get-AuthenticodeSignature -LiteralPath $resolvedDlssRuntime).Status.ToString()
    }
    stableRuntimeUntouched = (Get-FileHash -LiteralPath (Join-Path $InstalledRuntime 'Magpie.exe')).Hash -eq
        (Get-Content -LiteralPath (Join-Path $InstalledRuntime 'build-manifest.json') -Raw | ConvertFrom-Json).files.Where({ $_.path -eq 'Magpie.exe' }).sha256
}
$resultPath = Join-Path $toolsRoot 'build-verification.json'
New-Item -ItemType Directory -Path $toolsRoot -Force | Out-Null
$result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $resultPath -Encoding utf8
Write-Output "Native-upscaler lab build passed: $runtime"
Write-Output "Verification: $resultPath"
