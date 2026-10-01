param(
    [string]$OutputDirectory,
    [string]$FmtIncludeDirectory
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (!$OutputDirectory) { $OutputDirectory = Join-Path $root 'obj\tests\hdr' }
if (!(Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    throw 'Open an x64 Native Tools Command Prompt for Visual Studio, then run this script again.'
}
if (!$FmtIncludeDirectory) {
    $conanProps = Join-Path $root 'obj\x64\Release\_ConanDeps\Magpie\conan_fmt__fmt_vars_release_x64.props'
    if (Test-Path -LiteralPath $conanProps) {
        [xml]$conan = Get-Content -LiteralPath $conanProps -Raw
        $FmtIncludeDirectory = Join-Path $conan.Project.PropertyGroup.Conanfmt__fmtRootFolder 'include'
    }
}
if (!$FmtIncludeDirectory -or !(Test-Path -LiteralPath (Join-Path $FmtIncludeDirectory 'fmt\format.h'))) {
    throw 'Build the x64 Release dependencies first, or pass -FmtIncludeDirectory with the directory containing fmt/format.h.'
}

New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$OutputDirectory = (Resolve-Path -LiteralPath $OutputDirectory).Path
$core = Join-Path $root 'src\Magpie.Core'
$utf8 = [Text.UTF8Encoding]::new($false)
foreach ($name in @('HdrFrame.cpp', 'HdrColorTransform.cpp', 'HdrProtocol.cpp', 'HdrAdapterDispatcher.cpp', 'HdrEffectBoundary.cpp')) {
    $text = [IO.File]::ReadAllText((Join-Path $core $name))
    if (!$text.Contains('#include "pch.h"')) { throw "Review the test extraction for $name; the application PCH include changed." }
    [IO.File]::WriteAllText((Join-Path $OutputDirectory $name), $text.Replace('#include "pch.h"', ''), $utf8)
}
& python (Join-Path $root 'scripts\tests\test_hdr_surface_shader.py') $OutputDirectory
if ($LASTEXITCODE -ne 0) { throw 'HDR shader extraction failed.' }
& python (Join-Path $root 'tests\prepare_hdr_bicubic_test.py') $OutputDirectory
if ($LASTEXITCODE -ne 0) { throw 'Bicubic regression extraction failed.' }
& python (Join-Path $root 'tests\prepare_hdr_components_boundary_test.py') $OutputDirectory
if ($LASTEXITCODE -ne 0) { throw 'HDR component boundary extraction failed.' }

Push-Location $OutputDirectory
try {
    $common = @('/nologo', '/std:c++20', '/EHsc', '/O2', '/MT', '/utf-8', '/DNOMINMAX', "/I$core", "/I$core/include")
    & cl.exe @common /DFMT_HEADER_ONLY /FIfmt/format.h "/I$FmtIncludeDirectory" `
        HdrFrame.cpp HdrColorTransform.cpp HdrProtocol.cpp HdrAdapterDispatcher.cpp HdrEffectBoundary.cpp `
        (Join-Path $root 'tests\HdrMechanicalTests.cpp') /Fe:hdr_mechanical.exe
    if ($LASTEXITCODE -ne 0) { throw 'HDR CPU test compilation failed; inspect the compiler output above.' }
    & '.\hdr_mechanical.exe'
    if ($LASTEXITCODE -ne 0) { throw 'HDR CPU regressions failed; inspect the failing cases above.' }

    & cl.exe @common /DFMT_HEADER_ONLY /FIfmt/format.h "/I$FmtIncludeDirectory" `
        HdrFrame.cpp HdrColorTransform.cpp HdrProtocol.cpp HdrAdapterDispatcher.cpp HdrEffectBoundary.cpp `
        hdr_bicubic.cpp /Fe:hdr_bicubic.exe
    if ($LASTEXITCODE -ne 0) { throw 'Bicubic regression compilation failed; inspect the compiler output above.' }
    & '.\hdr_bicubic.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Bicubic session/cache regressions failed; inspect the failing cases above.' }

    & cl.exe @common /DFMT_HEADER_ONLY /FIfmt/format.h "/I$FmtIncludeDirectory" `
        HdrFrame.cpp HdrColorTransform.cpp HdrProtocol.cpp HdrAdapterDispatcher.cpp HdrEffectBoundary.cpp `
        hdr_components_boundary.cpp /Fe:hdr_components_boundary.exe
    if ($LASTEXITCODE -ne 0) { throw 'HDR component boundary test compilation failed.' }
    & '.\hdr_components_boundary.exe'
    if ($LASTEXITCODE -ne 0) { throw 'HDR component boundary traversal failed.' }

    & cl.exe @common /I. /DFMT_HEADER_ONLY /FIfmt/format.h "/I$FmtIncludeDirectory" `
        HdrFrame.cpp HdrColorTransform.cpp HdrProtocol.cpp HdrAdapterDispatcher.cpp HdrEffectBoundary.cpp `
        (Join-Path $root 'scripts\tests\hdr_surface_warp.cpp') d3d11.lib d3dcompiler.lib /Fe:hdr_surface_warp.exe
    if ($LASTEXITCODE -ne 0) { throw 'HDR WARP test compilation failed; inspect the compiler output above.' }
    & '.\hdr_surface_warp.exe'
    if ($LASTEXITCODE -ne 0) { throw 'HDR WARP regressions failed; inspect the failing cases above.' }
} finally { Pop-Location }
