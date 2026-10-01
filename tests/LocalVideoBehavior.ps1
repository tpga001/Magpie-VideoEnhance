param([Parameter(Mandatory)][int]$PlayerId,[ValidateSet('play','resize','pause','resume','secondary','primary','background')][string]$Phase='play',[int]$Seconds=8)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$out=Join-Path $root ('output/local-video/validation/'+(Get-Date -Format yyyyMMdd-HHmmss)+'-'+$Phase)
New-Item -ItemType Directory -Force $out | Out-Null
. (Join-Path $PSScriptRoot 'LocalVideoProbe.ps1') -PlayerId $PlayerId | Out-Null
$player=Get-Process -Id $PlayerId
if($player.MainWindowTitle -notlike 'magpie-local-*'){throw 'Only the dedicated Magpie local fixture may be controlled'}
$window=$player.MainWindowHandle
[void][LocalVideoProbe]::FocusWindow($window)
switch($Phase){
 'play' {}
 'pause' {[void][LocalVideoProbe]::PostMessageW($window,273,[IntPtr]10014,[IntPtr]::Zero)}
 'resume' {[void][LocalVideoProbe]::PostMessageW($window,273,[IntPtr]10014,[IntPtr]::Zero)}
 'resize' {[void][LocalVideoProbe]::SetWindowPos($window,[IntPtr]::Zero,500,300,1400,950,0x14)}
 'secondary' {[void][LocalVideoProbe]::SetWindowPos($window,[IntPtr]::Zero,-1040,400,980,800,0x14)}
 'primary' {[void][LocalVideoProbe]::SetWindowPos($window,[IntPtr]::Zero,500,300,1800,1200,0x14)}
 'background' {Add-Type -AssemblyName System.Windows.Forms;$form=[Windows.Forms.Form]::new();$form.Text='Magpie 本地视频焦点验收';$form.Width=460;$form.Height=180;$form.Show();[Windows.Forms.Application]::DoEvents();[void][LocalVideoProbe]::FocusWindow($form.Handle)}
}
Start-Sleep -Seconds 3
$samples=[Collections.Generic.List[object]]::new();$clock=[Diagnostics.Stopwatch]::StartNew()
while($clock.Elapsed.TotalSeconds -lt $Seconds){
 if($form){[Windows.Forms.Application]::DoEvents()}
 $overlay=[LocalVideoProbe]::FindWindow('Window_Magpie_967EB565-6F73-4E94-AE53-00CC42592A22',$null)
 $s=@{time=(Get-Date -Format o);foreground=[LocalVideoProbe]::GetForegroundWindow().ToInt64();player=$window.ToInt64();overlay=$overlay.ToInt64();overlayVisible=($overlay -ne 0 -and [LocalVideoProbe]::IsWindowVisible($overlay));source=$null;bounds=$null}
 if($overlay -ne 0){$s.bounds=([LocalVideoProbe]::Info($overlay)).rect}
 try{$m=[IO.MemoryMappedFiles.MemoryMappedFile]::OpenExisting(('Local\MagpieLocalVideo_'+$PlayerId),[IO.MemoryMappedFiles.MemoryMappedFileRights]::Read);$v=$m.CreateViewAccessor(0,296,[IO.MemoryMappedFiles.MemoryMappedFileAccess]::Read);$s.source=@{running=$v.ReadUInt32(12);width=$v.ReadUInt32(16);height=$v.ReadUInt32(20);sequence=$v.ReadUInt64(32);received=$v.ReadUInt64(288);pts=$v.ReadInt64(264);streamTime=$v.ReadInt64(272)};$v.Dispose();$m.Dispose()}catch{}
 $samples.Add($s);Start-Sleep -Milliseconds 200
}
$samples | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $out 'samples.json') -Encoding utf8
if($form){$form.Close();$form.Dispose()}else{& (Join-Path $PSScriptRoot 'LocalVideoProbe.ps1') -PlayerId $PlayerId -Screenshot (Join-Path $out 'screen.png') | Out-Null}
$result=@{phase=$Phase;path=$out;samples=$samples.Count;visible=@($samples | Where-Object overlayVisible).Count;foregroundLoss=@($samples | Where-Object {$_.foreground -ne $_.player}).Count;first=$samples[0].source;last=$samples[-1].source;bounds=$samples[-1].bounds}
$result | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $out 'result.json') -Encoding utf8
$result | ConvertTo-Json -Depth 5
