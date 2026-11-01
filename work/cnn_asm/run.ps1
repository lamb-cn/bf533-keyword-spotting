param([int]$Timeout=600000,[switch]$Smoke,[ValidateSet(1,3,15)][int]$Cases=15)
$ErrorActionPreference='Stop'
$cc='C:\Analog Devices\CrossCore Embedded Studio 2.10.0'
$root=Split-Path -Parent $PSScriptRoot
$root=Split-Path -Parent $root
Push-Location $root
try {
 $extra=@();if($Smoke){$extra+='-DRED_SMOKE'}
 $extra+="-DBENCH_CASES=$Cases"
 if(!$Smoke){
  & 'C:\Users\lenovo\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' 'work\cnn_asm\make_oracle.py'
  if($LASTEXITCODE -ne 0){throw 'Host oracle generation failed'}
 }
 & "$cc\ccblkfn.exe" -proc ADSP-BF533 -si-revision 0.6 -O -g -T 'work\cnn_asm\bench.ldf' @extra 'work\cnn_asm\bench.c' 'work\cnn_asm\cnn_ops.c' 'work\cnn_asm\cnn_c_baseline.c' 'work\cnn_asm\kernels.asm' 'work\cnn_asm\tail_ops.c' 'work\cnn_asm\tail_ops.asm' -o 'work\cnn_asm\bench.dxe' *> 'work\cnn_asm\build.log'
 if($LASTEXITCODE -ne 0){Get-Content 'work\cnn_asm\build.log';throw 'Build failed'}
 $syms=& "$cc\elfsyms.exe" 'work\cnn_asm\bench.dxe'
 $syms | Set-Content 'work\cnn_asm\symbols.txt'
 $address=[regex]::Match(($syms -join "`n"),'(?m)^_results = (0x[0-9A-Fa-f]+);').Groups[1].Value
 $doneAddress=[regex]::Match(($syms -join "`n"),'(?m)^_done = (0x[0-9A-Fa-f]+);').Groups[1].Value
 $mainAddress=[regex]::Match(($syms -join "`n"),'(?m)^_main = (0x[0-9A-Fa-f]+);').Groups[1].Value
 if(!$address -or !$doneAddress){throw 'Missing result symbols'}
 if([Convert]::ToUInt32($doneAddress,16)-[Convert]::ToUInt32($address,16) -ne 840){throw 'Unexpected result/completion layout'}
 Remove-Item 'work\cnn_asm\results.bin','work\cnn_asm\done.bin' -ErrorAction SilentlyContinue
 $load=@();if(!$Smoke){$load=@('-d',"0,in,work\cnn_asm\oracle.bin,binary,0x01000000,all,1")}
 & "$cc\CCES_runner.exe" --target 'ADSP-BF5xx Blackfin Family Simulators' --platform 'ADSP-BF5xx Single Processor Simulator' --processor ADSP-BF533 --timeout $Timeout -v1 -c '0,work\cnn_asm\bench.dxe' -b '0,_exit' @load -d "0,out,work\cnn_asm\results.bin,binary,$address,844,1,_exit" *> 'work\cnn_asm\sim.log'
 if($LASTEXITCODE -ne 0){Get-Content 'work\cnn_asm\sim.log';throw 'Simulation failed'}
 if(!(Test-Path 'work\cnn_asm\results.bin')){Get-Content 'work\cnn_asm\sim.log';throw 'No result dump: simulation may have timed out'}
 $doneBytes=[IO.File]::ReadAllBytes((Join-Path $root 'work\cnn_asm\results.bin'))
 if($doneBytes.Length -ne 844){throw 'Truncated result dump'}
 if([BitConverter]::ToUInt32($doneBytes,840) -ne 0x600dcafe){throw 'Incomplete run'}
 $bytes=[IO.File]::ReadAllBytes((Join-Path $root 'work\cnn_asm\results.bin'))
 $names='type','mode','bl','rs','macs','c_cycles','weight_cycles','prep_cycles','core_cycles','post_cycles','layer_cycles','raw_errors','final_errors','guard_errors'
 $rows=for($i=0;$i -lt 15;$i++) {
  $r=[ordered]@{case=$i}
  for($j=0;$j -lt 14;$j++){$r[$names[$j]]=[BitConverter]::ToUInt32($bytes,($i*14+$j)*4)}
  $r['core_cycles_per_mac']=if($r.macs){[math]::Round($r.core_cycles/$r.macs,4)}else{0}
  [pscustomobject]$r
 }
 $rows | Export-Csv 'work\cnn_asm\results.csv' -NoTypeInformation
 $rows | Format-Table case,type,mode,macs,core_cycles,layer_cycles,raw_errors,final_errors,guard_errors -AutoSize
 if(($rows | Measure-Object raw_errors -Sum).Sum -or ($rows | Measure-Object final_errors -Sum).Sum -or ($rows | Measure-Object guard_errors -Sum).Sum){throw 'Numerical tests FAILED'}
 if($Smoke){Write-Output 'PASS: signed lane/scale smoke tests.'}
 else {Write-Output "PASS: $Cases full-size layer cases; $($Cases*8000) raw and final outputs checked, plus timed-layer outputs."}
} finally {Pop-Location}







