#Requires -Version 7.0
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$workspace = Split-Path $repo -Parent
$output = Join-Path $workspace '.tools/browser-video-geometry-policy-tests'
New-Item -ItemType Directory -Path $output -Force | Out-Null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1
if (!$vs) { throw 'A Visual C++ toolchain is required.' }
$source = Join-Path $PSScriptRoot 'BrowserVideoGeometryPolicyTests.cpp'
$exe = Join-Path $output 'browser-video-geometry-policy.exe'
$obj = Join-Path $output 'browser-video-geometry-policy.obj'
$include = Join-Path $repo 'src/Magpie.Core/include'
$devShell = Join-Path $vs 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll'
if (Test-Path -LiteralPath $devShell) {
    Import-Module $devShell
    Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
    & cl.exe /nologo /std:c++20 /EHsc /utf-8 /MT /O2 /W4 /WX "/I$include" `
        $source "/Fe:$exe" "/Fo:$obj"
} else {
    $devCmd = Join-Path $vs 'Common7/Tools/VsDevCmd.bat'
    if (!(Test-Path -LiteralPath $devCmd -PathType Leaf)) {
        throw "Visual Studio developer environment not found: $devCmd"
    }
    $compile = 'call "{0}" -arch=x64 -host_arch=x64 >nul && cl.exe /nologo /std:c++20 /EHsc /utf-8 /MT /O2 /W4 /WX /I"{1}" "{2}" /Fe:"{3}" /Fo:"{4}"' -f `
        $devCmd, $include, $source, $exe, $obj
    & cmd.exe /d /s /c $compile
}
if ($LASTEXITCODE) { throw 'Browser video geometry policy test compilation failed' }
& $exe
if ($LASTEXITCODE) { throw 'Browser video geometry policy tests failed' }
