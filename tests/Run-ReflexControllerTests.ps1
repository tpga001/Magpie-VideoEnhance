#Requires -Version 7.0
$ErrorActionPreference = 'Stop'
$reflexRepo = Split-Path $PSScriptRoot -Parent
$reflexWorkspace = Split-Path $reflexRepo -Parent
$reflexOutput = Join-Path $reflexWorkspace '.tools/067-r2-reflex'
New-Item -ItemType Directory -Path $reflexOutput -Force | Out-Null
$reflexVswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$reflexVs = & $reflexVswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath | Select-Object -First 1
Import-Module (Join-Path $reflexVs 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $reflexVs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
& cl.exe /nologo /std:c++20 /EHsc /utf-8 /MT /O2 /W4 /WX "/I$reflexRepo/src/Magpie.Core" `
    (Join-Path $PSScriptRoot 'ReflexControllerTests.cpp') "/Fe:$reflexOutput/reflex-controller-tests.exe" "/Fo:$reflexOutput/reflex-controller-tests.obj"
if ($LASTEXITCODE) { throw 'Reflex controller test compilation failed' }
& "$reflexOutput/reflex-controller-tests.exe"
if ($LASTEXITCODE) { throw 'Reflex controller test failed' }
