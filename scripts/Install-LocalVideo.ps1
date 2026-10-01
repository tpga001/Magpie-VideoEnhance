#Requires -Version 7
param([string]$RuntimeDirectory='E:\Program Files\Magpie-Experimental-x64',[switch]$NoShortcut)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$bin=Join-Path $root 'output/local-video/build/bin'
if(Get-Process PotPlayerMini64 -ErrorAction SilentlyContinue){throw 'Close PotPlayer before installing its local-video filter.'}
if(Get-Process Magpie.LocalVideoHost -ErrorAction SilentlyContinue){throw 'Exit local video enhancement before updating.'}
$files=@('MagpieLocalVideoTap.ax','Magpie.LocalVideoHost.exe','Magpie.LocalVideoHost.dll','Magpie.LocalVideoHost.deps.json','Magpie.LocalVideoHost.runtimeconfig.json','libyuv-LICENSE.txt')
foreach($f in $files){if(!(Test-Path (Join-Path $bin $f))){throw "Build artifact missing: $f"}}
$backup=Join-Path $root ('output/local-video/install-backup/'+(Get-Date -Format yyyyMMdd-HHmmss))
New-Item -ItemType Directory -Force $backup,$RuntimeDirectory | Out-Null
reg.exe export 'HKCU\Software\DAUM\PotPlayerMini64\Override' (Join-Path $backup 'PotPlayer-Override.reg') /y | Out-Null
if($LASTEXITCODE){throw 'PotPlayer override backup failed'}
foreach($f in $files){$target=Join-Path $RuntimeDirectory $f;if(Test-Path $target){Copy-Item -LiteralPath $target -Destination $backup};Copy-Item -LiteralPath (Join-Path $bin $f) -Destination $target -Force
 if((Get-FileHash -LiteralPath $target).Hash -ne (Get-FileHash -LiteralPath (Join-Path $bin $f)).Hash){throw "Installed file hash mismatch: $f"}
}
$clsid='{671BF4B2-6DA8-4F3B-A174-7C8937C561C2}'
$clsidKey='HKCU:\Software\Classes\CLSID\'+$clsid
$inproc=Join-Path $clsidKey 'InprocServer32'
New-Item -Path $inproc -Force | Out-Null
Set-Item -LiteralPath $clsidKey -Value 'Magpie Local Video Tap'
Set-Item -LiteralPath $inproc -Value (Join-Path $RuntimeDirectory 'MagpieLocalVideoTap.ax')
New-ItemProperty -LiteralPath $inproc -Name ThreadingModel -Value Both -PropertyType String -Force | Out-Null
$overrides='HKCU:\Software\DAUM\PotPlayerMini64\Override'
$key=Get-ChildItem $overrides | Where-Object {(Get-ItemProperty $_.PSPath).CLSID -eq $clsid} | Select-Object -First 1
# PotPlayer stops reading at the first empty numbered entry. Reuse that entry.
$index=0
while($true){
 $candidate=$overrides+'\'+$index.ToString('0000')
 if(!(Test-Path $candidate)){break}
 $entry=Get-ItemProperty $candidate
 if(!$entry.CLSID -or $entry.CLSID -eq $clsid){break}
 $index++
}
$overrideKey=$candidate
if($key -and $key.PSChildName -ne $index.ToString('0000')){Remove-Item -LiteralPath $key.PSPath}
New-Item -Path $overrideKey -Force | Out-Null
foreach($name in @('Type','Disabled','FilterType','Merit','MeritHi')){
 $value=switch($name){Type{1} Disabled{0} FilterType{0} Merit{2097152} MeritHi{0}}
 New-ItemProperty -LiteralPath $overrideKey -Name $name -Value $value -PropertyType DWord -Force | Out-Null
}
$values=@{Path=(Join-Path $RuntimeDirectory 'MagpieLocalVideoTap.ax');Name='Magpie Local Video Tap';CLSID=$clsid;mod0000='{73646976-0000-0010-8000-00AA00389B71}';mod0001='{00000000-0000-0000-0000-000000000000}'}
foreach($name in $values.Keys){New-ItemProperty -LiteralPath $overrideKey -Name $name -Value $values[$name] -PropertyType String -Force | Out-Null}
if(!$NoShortcut){
 $shell=New-Object -ComObject WScript.Shell
 $shortcut=$shell.CreateShortcut((Join-Path ([Environment]::GetFolderPath('Desktop')) '本地视频增强.lnk'))
 $shortcut.TargetPath=Join-Path $RuntimeDirectory 'Magpie.LocalVideoHost.exe';$shortcut.WorkingDirectory=$RuntimeDirectory;$shortcut.Description='PotPlayer 视频区域超分和补帧；双击托盘图标开关';$shortcut.Save()
}
@{installedAt=(Get-Date -Format o);runtime=$RuntimeDirectory;backup=$backup;overrideKey=$overrideKey;clsid=$clsid;files=$files} | ConvertTo-Json | Set-Content (Join-Path $backup 'install.json') -Encoding utf8
@{installed=$true;backup=$backup;overrideKey=$overrideKey} | ConvertTo-Json
