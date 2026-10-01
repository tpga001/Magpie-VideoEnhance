param(
 [string]$RuntimeDirectory,
 [switch]$FocusOnly,
 [switch]$Quality,
 [switch]$GeometryOnly,
 [switch]$SidePanelMeasurement,
 [ValidateRange(0,360)][int]$QualitySeconds=0,
 [string]$QualityDisplays='',
 [ValidateSet('Edge','Chrome')][string]$Browser='Edge'
)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$RuntimeDirectory=[IO.Path]::GetFullPath($RuntimeDirectory)
$output=Join-Path $root 'output/playwright/browser-decoded'
New-Item -ItemType Directory -Path $output -Force | Out-Null
$manifest=Join-Path $output 'edge-native-host.json'
$browserConfig=if($Browser -eq 'Chrome'){
 @{ Registry='HKCU:\Software\Google\Chrome\NativeMessagingHosts'; Executable='C:\Program Files\Google\Chrome\Application\chrome.exe' }
}else{
 @{ Registry='HKCU:\Software\Microsoft\Edge\NativeMessagingHosts'; Executable='C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe' }
}
if(!(Test-Path -LiteralPath $browserConfig.Executable -PathType Leaf)){throw "$Browser executable not found: $($browserConfig.Executable)"}
$key=Join-Path $browserConfig.Registry 'com.magpie.browser_video_decoded_test'
$existed=Test-Path -LiteralPath $key
$previous=if($existed){(Get-Item -LiteralPath $key).GetValue('')}else{$null}
@{
 name='com.magpie.browser_video_decoded_test';description='Isolated decoded frame integration test'
 path=(Join-Path $RuntimeDirectory 'Magpie.BrowserVideoHost.exe');type='stdio'
 allowed_origins=@('chrome-extension://peflachlahjaaldgfbjkiniphnplmhgp/')
}|ConvertTo-Json|Set-Content -LiteralPath $manifest -Encoding utf8NoBOM
try {
 New-Item -Path $key -Force | Out-Null
 Set-Item -LiteralPath $key -Value $manifest
 $env:MAGPIE_RUNTIME=$RuntimeDirectory
 $env:MAGPIE_TEST_BROWSER_EXECUTABLE=$browserConfig.Executable
 $env:MAGPIE_TEST_BROWSER_NAME=$Browser
 $env:MAGPIE_FOCUS_ONLY=if($FocusOnly){'1'}else{''}
 $env:MAGPIE_GEOMETRY_ONLY=if($GeometryOnly){'1'}else{''}
 $env:MAGPIE_USE_SIDEPANEL_MEASUREMENT=if($SidePanelMeasurement){'1'}else{''}
 if($QualitySeconds){$env:MAGPIE_QUALITY_SECONDS="$QualitySeconds"}
 if($QualityDisplays){$env:MAGPIE_QUALITY_DISPLAYS=$QualityDisplays}
 if($Quality){$env:MAGPIE_QUALITY_OUTPUT=Join-Path $root ('output/playwright/browser-quality/'+(Get-Date -Format 'yyyyMMdd-HHmmss'))}
 $env:PLAYWRIGHT_MODULE='C:\Users\31273\.cache\codex-runtimes\codex-primary-runtime\dependencies\node\node_modules\playwright'
 $testScript=if($Quality){'browser-video-quality-e2e.cjs'}else{'browser-decoded-extension-e2e.cjs'}
 & 'C:\Users\31273\.cache\codex-runtimes\codex-primary-runtime\dependencies\node\bin\node.exe' (Join-Path $PSScriptRoot $testScript)
 if($LASTEXITCODE -ne 0){throw 'Real extension test failed; inspect the reported result file'}
}finally{
 if($existed){Set-Item -LiteralPath $key -Value $previous}
 elseif((Test-Path -LiteralPath $key) -and (Get-Item -LiteralPath $key).GetValue('') -eq $manifest){Remove-Item -LiteralPath $key}
}
