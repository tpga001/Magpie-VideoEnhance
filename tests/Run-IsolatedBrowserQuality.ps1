param(
    [Parameter(Mandatory)][string]$RuntimeDirectory,
    [ValidateRange(10, 360)][int]$QualitySeconds = 60,
    [string]$QualityDisplays = 'DISPLAY1',
    [switch]$SidePanelMeasurement,
    [switch]$GeometryOnly
)
$ErrorActionPreference = 'Stop'
$installed = 'E:\Program Files\Magpie-Experimental-x64'
$installedExe = [IO.Path]::GetFullPath((Join-Path $installed 'Magpie.exe'))
$candidate = [IO.Path]::GetFullPath($RuntimeDirectory)
$candidateExe = Join-Path $candidate 'Magpie.exe'
if (!(Test-Path -LiteralPath $candidateExe -PathType Leaf)) { throw "Candidate executable not found: $candidateExe" }
if ($candidateExe -eq $installedExe) { throw 'Use the ordinary quality test for the installed runtime' }
$control = Join-Path $PSScriptRoot 'BrowserVideoSmokeControl.ps1'
$test = Join-Path $PSScriptRoot 'Run-BrowserDecodedExtensionTest.ps1'
$portableConfig = Join-Path $candidate 'config\v4e\config.json'
$userConfig = Join-Path $env:LOCALAPPDATA 'Magpie\config\v4e\config.json'
$createdPortableConfig = $false
function MagpieProcesses {
    @(Get-Process -Name Magpie -ErrorAction SilentlyContinue)
}
function WaitForRuntime([string]$exe) {
    $deadline = (Get-Date).AddSeconds(15)
    do {
        $processes = MagpieProcesses
        if (@($processes | Where-Object Path -eq $exe).Count -gt 0) { return }
        Start-Sleep -Milliseconds 200
    } while ((Get-Date) -lt $deadline)
    throw "Runtime did not start: $exe"
}
$initial = MagpieProcesses
$unexpected = @($initial | Where-Object { $_.Path -ne $installedExe })
if ($unexpected.Count) { throw 'Another non-installed Magpie runtime is already running' }
$restoreInstalled = @($initial | Where-Object Path -eq $installedExe).Count -gt 0
try {
    if (!(Test-Path -LiteralPath $portableConfig)) {
        if (!(Test-Path -LiteralPath $userConfig -PathType Leaf)) { throw "User config not found: $userConfig" }
        New-Item -ItemType Directory -Path (Split-Path $portableConfig -Parent) -Force | Out-Null
        Copy-Item -LiteralPath $userConfig -Destination $portableConfig
        $createdPortableConfig = $true
    }
    if ($restoreInstalled) {
        & $control -Action quit -RuntimeDirectory $installed | Out-Null
    }
    Start-Process -FilePath $candidateExe -ArgumentList '-t' -WorkingDirectory $candidate -WindowStyle Hidden | Out-Null
    WaitForRuntime $candidateExe
    if (@(MagpieProcesses | Where-Object Path -eq $installedExe).Count -gt 0) {
        throw 'Installed runtime restarted while candidate was starting; test would be invalid'
    }
    $testArgs = @{
        RuntimeDirectory = $candidate
        Quality = $true
        QualitySeconds = $QualitySeconds
        QualityDisplays = $QualityDisplays
        SidePanelMeasurement = $SidePanelMeasurement
        GeometryOnly = $GeometryOnly
    }
    & $test @testArgs
} finally {
    try {
        if (@(MagpieProcesses | Where-Object Path -eq $candidateExe).Count -gt 0) {
            & $control -Action quit -RuntimeDirectory $candidate | Out-Null
        }
    } finally {
        try {
            if ($restoreInstalled -and @(MagpieProcesses | Where-Object Path -eq $installedExe).Count -eq 0) {
                Start-Process -FilePath $installedExe -ArgumentList '-t' -WorkingDirectory $installed -WindowStyle Hidden | Out-Null
                WaitForRuntime $installedExe
            }
        } finally {
            if ($createdPortableConfig) { Remove-Item -LiteralPath $portableConfig -Force }
        }
    }
}
