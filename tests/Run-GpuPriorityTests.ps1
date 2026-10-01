#Requires -Version 7.0
$ErrorActionPreference = 'Stop'
$priorityRepo = Split-Path $PSScriptRoot -Parent
$priorityOutput = Join-Path (Split-Path $priorityRepo -Parent) '.tools/067-beta4/panel-priority/tests'
python (Join-Path $PSScriptRoot 'prepare_gpu_priority_test.py') $priorityOutput
if ($LASTEXITCODE) { throw 'GPU priority extraction failed' }
$priorityVswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$priorityVs = & $priorityVswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath | Select-Object -First 1
Import-Module (Join-Path $priorityVs 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $priorityVs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
cl.exe /nologo /std:c++20 /EHsc /utf-8 /W4 /WX /MT "$priorityOutput/gpu_priority.cpp" "/Fe:$priorityOutput/gpu_priority.exe" "/Fo:$priorityOutput/gpu_priority.obj"
if ($LASTEXITCODE) { throw 'GPU priority test compilation failed' }
& "$priorityOutput/gpu_priority.exe"
if ($LASTEXITCODE) { throw 'GPU priority regression failed' }
