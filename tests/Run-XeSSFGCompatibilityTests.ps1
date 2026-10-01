#Requires -Version 7.0
param([string]$RuntimeDll)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$output = Join-Path (Split-Path $repo -Parent) '.tools/xess-mfg-impl/tests'
New-Item -ItemType Directory -Path $output -Force | Out-Null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath | Select-Object -First 1
Import-Module (Join-Path $vs 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
& cl.exe /nologo /std:c++20 /EHsc /utf-8 /W4 /WX /MT /O2 /DNOMINMAX /DUNICODE /D_UNICODE `
    "$repo/tests/XeSSFGCompatibilityTests.cpp" "/Fe:$output/XeSSFGCompatibilityTests.exe" "/Fo:$output/XeSSFGCompatibilityTests.obj"
if ($LASTEXITCODE) { throw 'XeSSFG tests compilation failed' }
if ($RuntimeDll) { & "$output/XeSSFGCompatibilityTests.exe" (Resolve-Path -LiteralPath $RuntimeDll).Path }
else { & "$output/XeSSFGCompatibilityTests.exe" }
if ($LASTEXITCODE) { throw 'XeSSFG tests failed' }
