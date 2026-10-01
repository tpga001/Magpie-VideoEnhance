param(
 [Parameter(Mandatory)][string]$OutputPath,
 [ValidateRange(20,360)][int]$Seconds=150,
 [ValidateRange(0,[int]::MaxValue)][int]$TargetProcessId=0,
 [switch]$ElevatedChild
)
$ErrorActionPreference='Stop'
$taskOutput=[IO.Path]::GetFullPath($OutputPath)
$taskAllowed=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\output\playwright\browser-quality'))+'\'
if(!$taskOutput.StartsWith($taskAllowed,[StringComparison]::OrdinalIgnoreCase)){throw 'Capture output must stay in the browser-quality evidence directory'}
$taskIsAdmin=([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
 [Security.Principal.WindowsBuiltInRole]::Administrator)
if(!$taskIsAdmin){
 if($ElevatedChild){throw 'PresentMon capture still lacks an elevated Windows token'}
 $taskArgs=@('-NoProfile','-ExecutionPolicy','Bypass','-File',$PSCommandPath,
  '-OutputPath',$taskOutput,'-Seconds',"$Seconds",'-TargetProcessId',"$TargetProcessId",'-ElevatedChild')
 $taskElevated=Start-Process -FilePath (Get-Command pwsh.exe).Source -ArgumentList $taskArgs `
  -Verb RunAs -WindowStyle Hidden -Wait -PassThru
 exit $taskElevated.ExitCode
}
New-Item -ItemType Directory -Path (Split-Path $taskOutput -Parent) -Force | Out-Null
$taskStatus=$taskOutput+'.status.json'
$taskSessionName='MagpieQualityAdmin'
@{state='starting';startedAt=(Get-Date -Format o);seconds=$Seconds;processId=$TargetProcessId;sessionName=$taskSessionName} | ConvertTo-Json | Set-Content -LiteralPath $taskStatus -Encoding utf8
$taskStart=[Diagnostics.ProcessStartInfo]::new('E:\LocalConfig\performance-panel\deps\PresentMon.exe')
$taskStart.UseShellExecute=$false;$taskStart.CreateNoWindow=$true;$taskStart.RedirectStandardOutput=$true;$taskStart.RedirectStandardError=$true
$taskSelector=if($TargetProcessId){@('--process_id',"$TargetProcessId")}else{@('--process_name','Magpie.exe')}
foreach($arg in @($taskSelector)+@('--session_name',$taskSessionName,'--stop_existing_session','--timed',"$Seconds",'--terminate_after_timed','--output_file',$taskOutput,'--no_console_stats','--no_track_input','--no_track_gpu','--date_time','--track_frame_type','--v2_metrics')){[void]$taskStart.ArgumentList.Add($arg)}
$taskCapture=[Diagnostics.Process]::Start($taskStart)
$taskOut=$taskCapture.StandardOutput.ReadToEndAsync();$taskErr=$taskCapture.StandardError.ReadToEndAsync()
@{state='running';pid=$taskCapture.Id;startedAt=(Get-Date -Format o);seconds=$Seconds;processId=$TargetProcessId;sessionName=$taskSessionName} | ConvertTo-Json | Set-Content -LiteralPath $taskStatus -Encoding utf8
$taskCapture.WaitForExit()
[IO.File]::WriteAllText($taskOutput+'.stdout.txt',$taskOut.Result)
[IO.File]::WriteAllText($taskOutput+'.stderr.txt',$taskErr.Result)
@{state='complete';exitCode=$taskCapture.ExitCode;endedAt=(Get-Date -Format o);seconds=$Seconds;processId=$TargetProcessId;sessionName=$taskSessionName} | ConvertTo-Json | Set-Content -LiteralPath $taskStatus -Encoding utf8
exit $taskCapture.ExitCode
