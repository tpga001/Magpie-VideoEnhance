#Requires -Version 7.0
param(
    [Parameter(Mandatory)][string]$ConfigPath
)

$ErrorActionPreference = 'Stop'
if (Get-Process Magpie -ErrorAction SilentlyContinue) {
    throw 'Close Magpie before installing the browser display-native preset.'
}
$configFile = (Resolve-Path -LiteralPath $ConfigPath).Path
$configDir = Split-Path $configFile -Parent
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$backup = Join-Path $configDir "config.before-browser-4k60-$stamp.json"
Copy-Item -LiteralPath $configFile -Destination $backup

$config = Get-Content -LiteralPath $configFile -Raw | ConvertFrom-Json
$modeName = '浏览器视频 30fps → 当前屏幕原生60fps'
$effects = @(
    [ordered]@{
        name = 'FrameRate_Filter'
        parameters = [ordered]@{ frameRateMode = 1; targetFrameRate = 30 }
    },
    [ordered]@{
        name = 'RTXVideo\RTXVideo_VSR'
        scalingType = 1
        scale = [ordered]@{ x = 1.0; y = 1.0 }
        parameters = [ordered]@{ strength = 3.0 }
    },
    [ordered]@{
        name = 'XeSSFG\XeSS_FrameGeneration'
        parameters = [ordered]@{
            multiplier = 2.0
            duplicateFrameFiltering = 1.0
            opticalFlowMethod = 1.0
            amdOpticalFlowMode = 1.0
            nvidiaOpticalFlowQuality = 2.0
        }
    }
)

$modes = @($config.scalingModes)
$modeIndex = -1
for ($i = 0; $i -lt $modes.Count; ++$i) {
    if ($modes[$i].name -eq $modeName) {
        $modeIndex = $i
        $modes[$i].effects = $effects
        break
    }
}
if ($modeIndex -lt 0) {
    $modeIndex = $modes.Count
    $modes += [pscustomobject][ordered]@{ name = $modeName; effects = $effects }
}
$vsrOnlyName = '浏览器视频 → 当前屏幕原生（仅RTX VSR）'
$vsrOnlyEffects = @(
    [ordered]@{
        name = 'RTXVideo\RTXVideo_VSR'
        scalingType = 1
        scale = [ordered]@{ x = 1.0; y = 1.0 }
        parameters = [ordered]@{ strength = 3.0 }
    }
)
$vsrOnlyIndex = -1
for ($i = 0; $i -lt $modes.Count; ++$i) {
    if ($modes[$i].name -eq $vsrOnlyName) {
        $vsrOnlyIndex = $i
        $modes[$i].effects = $vsrOnlyEffects
        break
    }
}
if ($vsrOnlyIndex -lt 0) {
    $vsrOnlyIndex = $modes.Count
    $modes += [pscustomobject][ordered]@{ name = $vsrOnlyName; effects = $vsrOnlyEffects }
}
$config.scalingModes = $modes

$managedNames = @(
    'Microsoft Edge 视频 4K60',
    'Google Chrome 视频 4K60',
    'Microsoft Edge 视频·当前屏幕原生60fps',
    'Google Chrome 视频·当前屏幕原生60fps'
)
$profiles = @($config.profiles | Where-Object { $_.name -notin $managedNames })
$config.profiles = $profiles

# Do not install an executable/class browser profile. Chromium uses the same
# class for ordinary tabs and video-only windows, so such a profile can process
# the complete browser chrome again. The mode is installed for isolated tests
# and for a future video-region picker; existing user profiles remain intact.

$temp = Join-Path $configDir "config.browser-4k60-$stamp.tmp"
$json = $config | ConvertTo-Json -Depth 20
[IO.File]::WriteAllText($temp, $json, [Text.UTF8Encoding]::new($false))
$roundTrip = Get-Content -LiteralPath $temp -Raw | ConvertFrom-Json
if (@($roundTrip.scalingModes)[$modeIndex].name -ne $modeName) {
    throw 'The generated configuration failed round-trip validation.'
}
if (@($roundTrip.scalingModes)[$vsrOnlyIndex].name -ne $vsrOnlyName) {
    throw 'The generated VSR-only configuration failed round-trip validation.'
}
[IO.File]::Move($temp, $configFile, $true)

[ordered]@{
    installed = $true
    modeName = $modeName
    modeIndex = $modeIndex
    vsrOnlyModeName = $vsrOnlyName
    vsrOnlyModeIndex = $vsrOnlyIndex
    profilesAdded = @()
    unsafeManagedProfilesRemoved = $managedNames
    activation = 'video-region or video-only window only; no whole-browser profile is installed'
    baseFps = 30
    targetFps = 60
    outputPolicy = 'fit to the monitor containing most of the cropped video region'
    superResolution = 'RTX Video VSR when output is larger; bypass at native size or downscale'
    backup = $backup
    config = $configFile
    sha256 = (Get-FileHash -LiteralPath $configFile -Algorithm SHA256).Hash
} | ConvertTo-Json -Depth 5
