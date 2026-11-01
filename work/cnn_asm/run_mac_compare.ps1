$ErrorActionPreference='Stop'
$cc='C:\Analog Devices\CrossCore Embedded Studio 2.10.0'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Push-Location $root
try{
 & "$cc\ccblkfn.exe" -proc ADSP-BF533 -si-revision 0.6 -O -g 'work\cnn_asm\mac_compare.c' 'work\cnn_asm\kernels.asm' -o 'work\cnn_asm\mac_compare.dxe' *> 'work\cnn_asm\mac_compare.build.log'
 if($LASTEXITCODE -ne 0){Get-Content 'work\cnn_asm\mac_compare.build.log';throw 'MAC benchmark build failed'}
 $syms=& "$cc\elfsyms.exe" 'work\cnn_asm\mac_compare.dxe'
 $syms | Set-Content 'work\cnn_asm\mac_compare.symbols.txt'
 $addr=[regex]::Match(($syms -join "`n"),'(?m)^_mac_results = (0x[0-9A-Fa-f]+);').Groups[1].Value
 Remove-Item 'work\cnn_asm\mac_compare.bin' -ErrorAction SilentlyContinue
 & "$cc\CCES_runner.exe" --target 'ADSP-BF5xx Blackfin Family Simulators' --platform 'ADSP-BF5xx Single Processor Simulator' --processor ADSP-BF533 --timeout 30000 -v1 -c '0,work\cnn_asm\mac_compare.dxe' -b '0,_exit' -d "0,out,work\cnn_asm\mac_compare.bin,binary,$addr,88,1,_exit" *> 'work\cnn_asm\mac_compare.sim.log'
 if(!(Test-Path 'work\cnn_asm\mac_compare.bin')){Get-Content 'work\cnn_asm\mac_compare.sim.log';throw 'MAC benchmark incomplete'}
 $b=[IO.File]::ReadAllBytes((Join-Path $root 'work\cnn_asm\mac_compare.bin'))
 if([BitConverter]::ToUInt32($b,84) -ne 0x600dcafe){throw 'MAC benchmark completion missing'}
 $names='k','outputs','macs','call_cycles','empty_cycles','net_cycles','errors'
 $rows=for($i=0;$i -lt 3;$i++){
  $r=[ordered]@{}
  for($j=0;$j -lt 7;$j++){$r[$names[$j]]=[BitConverter]::ToUInt32($b,($i*7+$j)*4)}
  if($r.errors){throw 'MAC benchmark output mismatch'}
  $r['net_cycles_per_mac']=[math]::Round($r.net_cycles/$r.macs,6)
  $r['net_macs_per_cycle']=[math]::Round($r.macs/$r.net_cycles,6)
  [pscustomobject]$r
 }
 $rows | Export-Csv 'work\cnn_asm\mac_results.csv' -NoTypeInformation
 $rows | Format-Table -AutoSize
}finally{Pop-Location}
