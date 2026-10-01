#Requires -Version 7.0
$ErrorActionPreference = 'Stop'
$policyRepo = Split-Path $PSScriptRoot -Parent
$policyWorkspace = Split-Path $policyRepo -Parent
$policyOutput = Join-Path $policyWorkspace 'release/v0.6.8-local/obj/VideoOutputPolicyTests'
New-Item -ItemType Directory -Path $policyOutput -Force | Out-Null
$policyVswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$policyVs = & $policyVswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1
if (!$policyVs) { throw 'A Visual C++ toolchain is required.' }
$policySource = Join-Path $PSScriptRoot 'VideoOutputPolicyTests.cpp'
$policyExe = Join-Path $policyOutput 'policy.exe'
$policyObj = Join-Path $policyOutput 'policy.obj'
$policyInclude = Join-Path $policyRepo 'src/Magpie.Core/include'
$policyDevShell = Join-Path $policyVs 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll'
if (Test-Path -LiteralPath $policyDevShell) {
    Import-Module $policyDevShell
    Enter-VsDevShell -VsInstallPath $policyVs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
    & cl.exe /nologo /std:c++17 /EHsc /utf-8 /MT /O2 /W4 /WX "/I$policyInclude" `
        $policySource "/Fe:$policyExe" "/Fo:$policyObj"
} else {
    $policyDevCmd = Join-Path $policyVs 'Common7/Tools/VsDevCmd.bat'
    if (!(Test-Path -LiteralPath $policyDevCmd -PathType Leaf)) {
        throw "Visual Studio developer environment not found: $policyDevCmd"
    }
    $policyCompile = 'call "{0}" -arch=x64 -host_arch=x64 >nul && cl.exe /nologo /std:c++17 /EHsc /utf-8 /MT /O2 /W4 /WX /I"{1}" "{2}" /Fe:"{3}" /Fo:"{4}"' -f `
        $policyDevCmd, $policyInclude, $policySource, $policyExe, $policyObj
    & cmd.exe /d /s /c $policyCompile
}
if ($LASTEXITCODE) { throw 'Video output policy compilation failed' }
& $policyExe
if ($LASTEXITCODE) { throw 'Video output policy tests failed' }
