param([ValidateSet('Control','Native')][string]$Mode='Native')
$tag=$Mode.ToLowerInvariant();$statusPath=Join-Path $PSScriptRoot "$tag.status.json"
if(!(Test-Path -LiteralPath $statusPath)){throw 'No worker status exists'}
$s=Get-Content -LiteralPath $statusPath -Raw | ConvertFrom-Json
$observed=[ordered]@{mode=$Mode;recordedState=$s.state;parentPid=$s.parentPid;runnerPid=$s.runnerPid;observedUtc=[DateTime]::UtcNow.ToString('o');processState='Unknown'}
try{
 $proc=Get-CimInstance Win32_Process -Filter "ProcessId=$($s.runnerPid)" -ErrorAction Stop
 if(!$proc){$observed.processState='Missing'}
 elseif($proc.Name -eq 'CCES_runner.exe' -and $proc.CommandLine -like "*stream_$tag.dxe*"){
  $observed.processState='LiveMatchingRunner';$observed['commandLine']=$proc.CommandLine
  $p=Get-Process -Id $s.runnerPid -ErrorAction Stop
  $observed['cpuSeconds']=$p.CPU;$observed['processStarted']=$p.StartTime
 }else{$observed.processState='PIDBelongsToDifferentProcess'}
}catch{$observed['observationError']=$_.Exception.Message}
$log=Join-Path $PSScriptRoot "$tag.sim.log"
if(Test-Path -LiteralPath $log){
 $lines=Get-Content -LiteralPath $log -ErrorAction SilentlyContinue
 $observed['lastProgress']=($lines | Where-Object {$_ -match '^(STREAM|PROGRESS)'} | Select-Object -Last 1)
 $observed['eventLines']=@($lines | Where-Object {$_ -match '^EVENT '})
 $observed['stopRegisterLines']=@($lines | Where-Object {$_ -match '\$PC =|\$RETX =|Error:'})
}
$observed['finalDumpExists']=Test-Path -LiteralPath (Join-Path $PSScriptRoot "stream_$tag.bin")
$observed | ConvertTo-Json -Depth 6
