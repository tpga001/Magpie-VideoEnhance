#Requires -Version 7.0
param(
    [string]$RuntimeDirectory = $PSScriptRoot,
    [switch]$OpenExtensionsPage
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$runtime = [IO.Path]::GetFullPath($RuntimeDirectory)
$hostPath = Join-Path $runtime 'Magpie.BrowserVideoHost.exe'
$extensionDirectory = Join-Path $runtime 'BrowserVideoExtension'
$extensionManifestPath = Join-Path $extensionDirectory 'manifest.json'
foreach ($required in @($hostPath, $extensionManifestPath)) {
    if (!(Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Missing browser-video component: $required"
    }
}

$extensionId = 'peflachlahjaaldgfbjkiniphnplmhgp'
$hostName = 'com.magpie.browser_video'
$hostManifestPath = Join-Path $runtime "$hostName.json"
[ordered]@{
    name = $hostName
    description = 'Magpie automatic browser video region bridge'
    path = $hostPath
    type = 'stdio'
    allowed_origins = @("chrome-extension://$extensionId/")
} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $hostManifestPath -Encoding utf8NoBOM

$registryPaths = @(
    "HKCU:\Software\Microsoft\Edge\NativeMessagingHosts\$hostName",
    "HKCU:\Software\Google\Chrome\NativeMessagingHosts\$hostName"
)
$state = [ordered]@{
    installedAtUtc = [DateTime]::UtcNow.ToString('o')
    extensionId = $extensionId
    extensionDirectory = $extensionDirectory
    nativeHostManifest = $hostManifestPath
    registry = @()
}
foreach ($registryPath in $registryPaths) {
    $previous = $null
    if (Test-Path -LiteralPath $registryPath) {
        $previous = (Get-Item -LiteralPath $registryPath).GetValue('')
    } else {
        New-Item -Path $registryPath -Force | Out-Null
    }
    Set-Item -LiteralPath $registryPath -Value $hostManifestPath
    $state.registry += [ordered]@{ path = $registryPath; previous = $previous }
}
$state | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath `
    (Join-Path $runtime 'browser-video-auto-install-state.json') -Encoding utf8NoBOM

if ($OpenExtensionsPage) {
    $edge = Join-Path ${env:ProgramFiles(x86)} 'Microsoft\Edge\Application\msedge.exe'
    if (Test-Path -LiteralPath $edge) {
        Start-Process -FilePath $edge -ArgumentList 'edge://extensions'
        Start-Process -FilePath 'explorer.exe' -ArgumentList $extensionDirectory
    }
}

[ordered]@{
    installed = $true
    extensionId = $extensionId
    extensionDirectory = $extensionDirectory
    next = '在 edge://extensions 开启开发人员模式，选择“加载解压缩的扩展”，然后选择 extensionDirectory。此操作只需一次。'
} | ConvertTo-Json -Depth 4
