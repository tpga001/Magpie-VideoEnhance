#Requires -Version 7.0
param([string]$Destination)
$ErrorActionPreference = 'Stop'
if (!$Destination) { $Destination = Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) 'dependencies/RTX-Video-SDK-1.1.0' }
$Destination = [IO.Path]::GetFullPath($Destination)
New-Item -ItemType Directory -Path $Destination -Force | Out-Null
$archive = Join-Path $Destination 'RTX_Video_SDK_v1.1.0.zip'
$sha256 = 'ABF4F34E2B5A618E355B0D5A0365D8ECC3DB4396E756E4C850A867E1AE2ED69E'
# Public NVIDIA NGC artifact: nvidia/multimedia/dlpp:1.5. Its download API
# returns a signed CDN location in a response header, not the archive body.
if (!(Test-Path -LiteralPath $archive) -or (Get-FileHash -LiteralPath $archive).Hash -ne $sha256) {
    $response = Invoke-WebRequest 'https://api.ngc.nvidia.com/v2/models/nvidia/multimedia/dlpp/versions/1.5/files/RTX_Video_SDK_v1.1.0.zip'
    $location = [string]($response.Headers.Location | Select-Object -First 1)
    if (([uri]$location).Scheme -ne 'https' -or ([uri]$location).Host -ne 'xfiles.ngc.nvidia.com') { throw 'Unexpected NVIDIA NGC download host.' }
    Invoke-WebRequest -Uri $location -OutFile $archive
}
if ((Get-FileHash -LiteralPath $archive).Hash -ne $sha256) { throw 'RTX Video SDK checksum mismatch. Retry the official NGC download.' }
Expand-Archive -LiteralPath $archive -DestinationPath $Destination -Force
foreach ($file in @('include/nvsdk_ngx_helpers_truehdr.h','lib/Windows/x64/nvsdk_ngx_s.lib','bin/Windows/x64/rel/nvngx_truehdr.dll','NVIDIA_RTX_Video_SDK_License.pdf')) {
    if (!(Test-Path -LiteralPath (Join-Path $Destination $file))) { throw "Incomplete SDK: $file" }
}
"RTX Video SDK 1.1 ready: $Destination (official NGC SHA256 verified)"
