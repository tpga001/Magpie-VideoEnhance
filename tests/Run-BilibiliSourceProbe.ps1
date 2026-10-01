param([ValidateRange(30,90)][int]$Seconds=45)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$candidate=[IO.Path]::GetFullPath((Join-Path $root '..\release\frame-stability-vsync-a957f81\Magpie-Experimental-x64'))
$installed='E:\Program Files\Magpie-Experimental-x64'
$candidateExe=Join-Path $candidate 'Magpie.exe'
$installedExe=Join-Path $installed 'Magpie.exe'
$portableConfig=Join-Path $candidate 'config\v4e\config.json'
$userConfig=Join-Path $env:LOCALAPPDATA 'Magpie\config\v4e\config.json'
$output=Join-Path $root ('output\playwright\bilibili-source\'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
$manifest=Join-Path $output 'native-host.json'
$key='HKCU:\Software\Microsoft\Edge\NativeMessagingHosts\com.magpie.browser_video_decoded_test'
$node='C:\Users\31273\.cache\codex-runtimes\codex-primary-runtime\dependencies\node\bin\node.exe'
$edge='C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe'
$control=Join-Path $PSScriptRoot 'BrowserVideoSmokeControl.ps1'
$createdConfig=$false
$hadKey=Test-Path -LiteralPath $key
$oldValue=if($hadKey){(Get-Item -LiteralPath $key).GetValue('')}else{$null}
$wasInstalled=@(Get-Process -Name Magpie -ErrorAction SilentlyContinue | Where-Object Path -eq $installedExe).Count -gt 0
if(@(Get-Process -Name Magpie -ErrorAction SilentlyContinue | Where-Object Path -ne $installedExe).Count){throw 'A non-installed Magpie is already running'}
if(!(Test-Path -LiteralPath $candidateExe -PathType Leaf)){throw "Candidate missing: $candidateExe"}
if(!(Test-Path -LiteralPath $edge -PathType Leaf)){throw "Edge missing: $edge"}
New-Item -ItemType Directory -Path $output -Force | Out-Null
try {
  if(!(Test-Path -LiteralPath $portableConfig)){
    New-Item -ItemType Directory -Path (Split-Path $portableConfig -Parent) -Force | Out-Null
    Copy-Item -LiteralPath $userConfig -Destination $portableConfig
    $createdConfig=$true
  }
  if($wasInstalled){& $control -Action quit -RuntimeDirectory $installed | Out-Null}
  Start-Process -FilePath $candidateExe -ArgumentList '-t' -WorkingDirectory $candidate -WindowStyle Hidden | Out-Null
  $deadline=(Get-Date).AddSeconds(15)
  while((Get-Date) -lt $deadline){
    if(@(Get-Process -Name Magpie -ErrorAction SilentlyContinue | Where-Object Path -eq $candidateExe).Count){break}
    Start-Sleep -Milliseconds 200
  }
  if(!@(Get-Process -Name Magpie -ErrorAction SilentlyContinue | Where-Object Path -eq $candidateExe).Count){throw 'Candidate did not start'}
  @{name='com.magpie.browser_video_decoded_test';description='Isolated Bilibili source probe';path=(Join-Path $candidate 'Magpie.BrowserVideoHost.exe');type='stdio';allowed_origins=@('chrome-extension://peflachlahjaaldgfbjkiniphnplmhgp/')} |
    ConvertTo-Json | Set-Content -LiteralPath $manifest -Encoding utf8NoBOM
  New-Item -Path $key -Force | Out-Null
  Set-Item -LiteralPath $key -Value $manifest
  $env:MAGPIE_RUNTIME=$candidate
  $env:MAGPIE_PROBE_OUTPUT=$output
  $env:MAGPIE_PROBE_SECONDS="$Seconds"
  $env:MAGPIE_TEST_BROWSER_EXECUTABLE=$edge
  $env:PLAYWRIGHT_MODULE='C:\Users\31273\.cache\codex-runtimes\codex-primary-runtime\dependencies\node\node_modules\playwright'
  & $node (Join-Path $PSScriptRoot 'bilibili-source-probe.cjs')
  if($LASTEXITCODE -ne 0){throw "Source probe failed: $output\result.json"}
} finally {
  try {
    if($hadKey){Set-Item -LiteralPath $key -Value $oldValue}
    elseif((Test-Path -LiteralPath $key) -and (Get-Item -LiteralPath $key).GetValue('') -eq $manifest){Remove-Item -LiteralPath $key}
  } finally {
    try {
      if(@(Get-Process -Name Magpie -ErrorAction SilentlyContinue | Where-Object Path -eq $candidateExe).Count){& $control -Action quit -RuntimeDirectory $candidate | Out-Null}
    } finally {
      try {
        if($wasInstalled -and !@(Get-Process -Name Magpie -ErrorAction SilentlyContinue | Where-Object Path -eq $installedExe).Count){
          Start-Process -FilePath $installedExe -ArgumentList '-t' -WorkingDirectory $installed -WindowStyle Hidden | Out-Null
        }
      } finally {
        if($createdConfig){Remove-Item -LiteralPath $portableConfig -Force}
      }
    }
  }
}
