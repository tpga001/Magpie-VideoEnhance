#Requires -Version 7.0
$ErrorActionPreference = 'Stop'
$r2Repo = Split-Path $PSScriptRoot -Parent
$r2Workspace = Split-Path $r2Repo -Parent
$r2Output = Join-Path $r2Workspace '.tools/067-r2-resources'
$r2Vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$r2Vs = & $r2Vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath | Select-Object -First 1
Import-Module (Join-Path $r2Vs 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $r2Vs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
$r2Wil = Get-ChildItem (Join-Path $r2Repo 'packages') -Directory -Filter 'Microsoft.Windows.ImplementationLibrary.*' | Sort-Object Name -Descending | Select-Object -First 1
& python (Join-Path $PSScriptRoot 'prepare_dlss_r2_test.py') $r2Output
if ($LASTEXITCODE) { throw 'Extract production functions failed' }
$r2IncludePaths = @($r2Output; (Join-Path $r2Repo 'src/Magpie.Core'); (Join-Path $r2Wil.FullName 'include'); (Join-Path $r2Workspace 'release/v0.6.7-local/obj/Magpie.Core/Generated Files'))
$r2Includes = @($r2IncludePaths | ForEach-Object { '/I' + $_ })
foreach ($r2Timing in @($false, $true)) {
    $r2Name = if ($r2Timing) { 'resources-timing' } else { 'resources-default' }
    $r2Define = @(if ($r2Timing) { '/DMP_ENABLE_NATIVE_BACKEND_TIMING' })
    & cl.exe /nologo /std:c++20 /EHsc /utf-8 /MT /O2 @r2Includes @r2Define (Join-Path $PSScriptRoot 'DlssR2ResourceTests.cpp') "/Fe:$r2Output/$r2Name.exe" "/Fo:$r2Output/$r2Name.obj" /link d3d11.lib d3d12.lib dxgi.lib d3dcompiler.lib WindowsApp.lib
    if ($LASTEXITCODE) { throw "Compile failed: $r2Name" }
    & "$r2Output/$r2Name.exe"
    if ($LASTEXITCODE) { throw "Test failed: $r2Name" }
}
