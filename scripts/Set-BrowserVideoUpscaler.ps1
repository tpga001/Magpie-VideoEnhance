#Requires -Version 7.0
<#
.SYNOPSIS
Select the super-resolution effect used by Magpie's automatic browser-video mode.
.DESCRIPTION
Changes only the SR effect in the existing automatic mode. The frame-rate filter,
XeSS frame generation, mode order, profiles and all other settings are retained.
Magpie must be closed before writing its live configuration. Every write creates
a timestamped backup and uses an atomic same-directory replacement.
#>
[CmdletBinding()]
param(
    [ValidateSet('RTX-VSR', 'DLSS-SR', 'XeSS-SR', 'FSR3-SR')]
    [string]$Mode,
    [string]$ConfigPath = (Join-Path $env:LOCALAPPDATA 'Magpie\config\v4e\config.json'),
    [string]$RollbackBackup,
    [switch]$Preview
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if ($RollbackBackup -and ($Mode -or $Preview)) {
    throw 'Use -RollbackBackup by itself with -ConfigPath.'
}
if (!$RollbackBackup -and !$Mode) {
    throw 'Specify -Mode or -RollbackBackup.'
}

$configFile = (Resolve-Path -LiteralPath $ConfigPath).Path
$configDirectory = Split-Path -Parent $configFile
$liveConfigFile = [IO.Path]::GetFullPath((Join-Path $env:LOCALAPPDATA 'Magpie\config\v4e\config.json'))
$modeName = '浏览器视频 30fps → 当前屏幕原生60fps'
$effectByMode = [ordered]@{
    'RTX-VSR' = 'RTXVideo\RTXVideo_VSR'
    'DLSS-SR' = 'DLSS\DLSS_SR'
    'XeSS-SR' = 'XeSS\XeSS_SR'
    'FSR3-SR' = 'FSR3\FSR3_SR'
}
$knownEffects = @($effectByMode.Values)

function Read-Config([string]$path) {
    $raw = [IO.File]::ReadAllText($path, [Text.Encoding]::UTF8)
    $value = ConvertFrom-Json -InputObject $raw
    if (!$value -or !$value.PSObject.Properties['scalingModes'] -or
        $value.scalingModes -isnot [array]) {
        throw "Missing scalingModes array: $path"
    }
    return $value
}

function Get-Mode([object]$config) {
    $found = @($config.scalingModes | Where-Object { $_.name -eq $modeName })
    if ($found.Count -ne 1) {
        throw "Expected one automatic browser-video mode; found $($found.Count)."
    }
    $item = $found[0]
    if ($item.effects -isnot [array]) {
        throw 'Automatic browser-video mode has no effects array.'
    }
    return $item
}

function Write-AtomicConfig([string]$json, [string]$action) {
    if ($configFile.Equals($liveConfigFile, [StringComparison]::OrdinalIgnoreCase) -and
        (Get-Process -Name 'Magpie' -ErrorAction SilentlyContinue)) {
        throw 'Close Magpie before changing its configuration; otherwise it can overwrite the selection on exit.'
    }
    $currentBytes = [IO.File]::ReadAllBytes($configFile)
    $currentHash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($currentBytes))
    $stamp = (Get-Date).ToUniversalTime().ToString('yyyyMMdd-HHmmss')
    $nonce = [Guid]::NewGuid().ToString('N').Substring(0, 8)
    $backup = Join-Path $configDirectory "config.json.$stamp.$nonce.$action.bak"
    $temp = Join-Path $configDirectory "config.json.$nonce.tmp"
    try {
        [IO.File]::WriteAllText($temp, $json,
            [Text.UTF8Encoding]::new($false))
        $null = Read-Config $temp
        $latestBytes = [IO.File]::ReadAllBytes($configFile)
        $latestHash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($latestBytes))
        if ($latestHash -ne $currentHash) {
            throw 'Configuration changed while preparing the update. No file was replaced.'
        }
        [IO.File]::Replace($temp, $configFile, $backup, $true)
        $null = Read-Config $configFile
        $backupHash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData(
            [IO.File]::ReadAllBytes($backup)))
        if ($backupHash -ne $currentHash) {
            throw "Backup checksum mismatch; inspect $backup before further changes."
        }
        return $backup
    } finally {
        if (Test-Path -LiteralPath $temp) { Remove-Item -LiteralPath $temp -Force }
    }
}

if ($RollbackBackup) {
    $backupFile = (Resolve-Path -LiteralPath $RollbackBackup).Path
    if ((Split-Path -Parent $backupFile) -ne $configDirectory -or
        $backupFile -notmatch '^.+config\.json\.\d{8}-\d{6}\.[a-f0-9]{8}\.(set|rollback)\.bak$') {
        throw 'Rollback file must be a backup made by this script in the configuration directory.'
    }
    $restored = Read-Config $backupFile
    $null = Get-Mode $restored
    $newBackup = Write-AtomicConfig ([IO.File]::ReadAllText($backupFile, [Text.Encoding]::UTF8)) 'rollback'
    Write-Output "Restored $backupFile; replaced configuration saved at $newBackup"
    return
}

$config = Read-Config $configFile
$automaticMode = Get-Mode $config
$sr = @($automaticMode.effects | Where-Object { $_.name -in $knownEffects })
if ($sr.Count -ne 1) {
    throw "Expected one known SR effect in the automatic mode; found $($sr.Count)."
}
$filter = @($automaticMode.effects | Where-Object { $_.name -eq 'FrameRate_Filter' })
$fg = @($automaticMode.effects | Where-Object { $_.name -eq 'XeSSFG\XeSS_FrameGeneration' })
if ($filter.Count -ne 1 -or $fg.Count -ne 1) {
    throw 'Automatic mode chain differs from the validated frame-rate filter + XeSS FG chain.'
}
$current = [string]$sr[0].name
$target = [string]$effectByMode[$Mode]
if ($current -eq $target) {
    Write-Output "Already selected: $Mode ($target). No file changed."
    return
}
$newEffect = [ordered]@{
    name = $target
    scalingType = 1
    scale = [ordered]@{ x = 1.0; y = 1.0 }
    parameters = switch ($Mode) {
        'RTX-VSR' { [ordered]@{ strength = 3.0 } }
        'DLSS-SR' { [ordered]@{ dlssModelPreset = 11.0; opticalFlowMethod = 0.0 } }
        default { [ordered]@{ opticalFlowMethod = 0.0 } }
    }
}
$effectIndex = [array]::IndexOf($automaticMode.effects, $sr[0])
if ($Preview) {
    Write-Output "Would change automatic browser-video SR at effect #$($effectIndex + 1): $current -> $target. Other effects and profiles remain in place."
    return
}
$automaticMode.effects[$effectIndex] = $newEffect
$json = ConvertTo-Json -InputObject $config -Depth 100
$roundTrip = ConvertFrom-Json -InputObject $json
$updated = Get-Mode $roundTrip
if ($updated.effects[$effectIndex].name -ne $target -or
    @($roundTrip.scalingModes).Count -ne @($config.scalingModes).Count) {
    throw 'Generated configuration failed validation; no file changed.'
}
$backup = Write-AtomicConfig $json 'set'
Write-Output "Selected $Mode ($target). Backup: $backup. Reopen Magpie to use the new effect."
