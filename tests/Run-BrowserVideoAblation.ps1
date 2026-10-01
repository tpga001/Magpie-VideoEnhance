param(
 [string]$RuntimeDirectory='E:\LocalConfig\magpie-universal-ai-screen-lab\release\v0.6.8-local\Magpie-Experimental-x64',
 [string]$InstalledDirectory='E:\Program Files\Magpie-Experimental-x64'
)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$output=Join-Path $root ('output/playwright/browser-quality/ablation-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
if([IO.Path]::GetFullPath($RuntimeDirectory).TrimEnd('\') -eq [IO.Path]::GetFullPath($InstalledDirectory).TrimEnd('\')){throw 'A separate portable runtime is required'}
$config=Join-Path $RuntimeDirectory 'config/v4e/config.json'
if(!(Test-Path -LiteralPath $config)){throw 'Portable configuration is required'}
New-Item -ItemType Directory -Path $output -Force | Out-Null
$backup=[IO.File]::ReadAllBytes($config)
[IO.File]::WriteAllBytes((Join-Path $output 'portable-config.original.json'),$backup)
$configBak=$config+'.bak';$hadBak=Test-Path -LiteralPath $configBak
$backupBak=if($hadBak){[IO.File]::ReadAllBytes($configBak)}else{$null}
$key='HKCU:\Software\Microsoft\Edge\NativeMessagingHosts\com.magpie.browser_video_decoded_test'
$existed=Test-Path -LiteralPath $key
$previous=if($existed){(Get-Item -LiteralPath $key).GetValue('')}else{$null}
$manifest=Join-Path $output 'edge-native-host.json'
@{name='com.magpie.browser_video_decoded_test';description='Isolated video quality comparison';path=(Join-Path $RuntimeDirectory 'Magpie.BrowserVideoHost.exe');type='stdio';allowed_origins=@('chrome-extension://peflachlahjaaldgfbjkiniphnplmhgp/')} | ConvertTo-Json | Set-Content -LiteralPath $manifest -Encoding utf8NoBOM
$installedState=& (Join-Path $PSScriptRoot 'BrowserVideoSmokeControl.ps1') -Action state -RuntimeDirectory $InstalledDirectory | ConvertFrom-Json
try {
 & (Join-Path $PSScriptRoot 'BrowserVideoSmokeControl.ps1') -Action quit -RuntimeDirectory $InstalledDirectory
 & (Join-Path $PSScriptRoot 'BrowserVideoSmokeControl.ps1') -Action quit -RuntimeDirectory $RuntimeDirectory
 New-Item -Path $key -Force | Out-Null
 Set-Item -LiteralPath $key -Value $manifest
 $env:MAGPIE_RUNTIME=$RuntimeDirectory
 $env:MAGPIE_ABLATION_OUTPUT=$output
 $env:PLAYWRIGHT_MODULE='C:\Users\31273\.cache\codex-runtimes\codex-primary-runtime\dependencies\node\node_modules\playwright'
 & 'C:\Users\31273\.cache\codex-runtimes\codex-primary-runtime\dependencies\node\bin\node.exe' (Join-Path $PSScriptRoot 'browser-video-ablation-e2e.cjs')
 if($LASTEXITCODE -ne 0){throw "Ablation failed; inspect $output"}
} finally {
 try {
  & (Join-Path $PSScriptRoot 'BrowserVideoSmokeControl.ps1') -Action quit -RuntimeDirectory $RuntimeDirectory
  [IO.File]::WriteAllBytes($config,$backup)
  if($hadBak){[IO.File]::WriteAllBytes($configBak,$backupBak)}
 } finally {
  if($existed){Set-Item -LiteralPath $key -Value $previous}
  elseif((Test-Path -LiteralPath $key) -and (Get-Item -LiteralPath $key).GetValue('') -eq $manifest){Remove-Item -LiteralPath $key}
  if($installedState.running){Start-Process -FilePath (Join-Path $InstalledDirectory 'Magpie.exe') -ArgumentList '-t' -WorkingDirectory $InstalledDirectory -WindowStyle Hidden}
  @{output=$output;portableConfigRestored=((Get-FileHash -LiteralPath $config).Hash -eq (Get-FileHash -LiteralPath (Join-Path $output 'portable-config.original.json')).Hash);testHostRestored=$true;installedRestartRequested=[bool]$installedState.running} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $output 'cleanup.json') -Encoding utf8
 }
}
