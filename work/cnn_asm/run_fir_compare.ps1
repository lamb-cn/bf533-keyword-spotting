$ErrorActionPreference='Stop'
$cc='C:\Analog Devices\CrossCore Embedded Studio 2.10.0'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Push-Location $root
try {
 $rows=foreach($variant in @('original','split_banks')){
  $extra=@();if($variant -eq 'split_banks'){$extra+='-DFIR_SPLIT'}
  $base="work\cnn_asm\fir_$variant"
  & "$cc\ccblkfn.exe" -proc ADSP-BF533 -si-revision 0.6 -O -g -I fir @extra 'work\cnn_asm\fir_compare.c' 'fir\fir.asm' -o "$base.dxe" *> "$base.build.log"
  if($LASTEXITCODE -ne 0){Get-Content "$base.build.log";throw 'FIR build failed'}
  $syms=& "$cc\elfsyms.exe" "$base.dxe"
  $syms | Set-Content "$base.symbols.txt"
  $addr=[regex]::Match(($syms -join "`n"),'(?m)^_fir_result = (0x[0-9A-Fa-f]+);').Groups[1].Value
  if(!$addr){throw 'FIR result symbol missing'}
  Remove-Item "$base.bin" -ErrorAction SilentlyContinue
  & "$cc\CCES_runner.exe" --target 'ADSP-BF5xx Blackfin Family Simulators' --platform 'ADSP-BF5xx Single Processor Simulator' --processor ADSP-BF533 --timeout 15000 -v1 -c "0,$base.dxe" -b '0,_exit' -d "0,out,$base.bin,binary,$addr,552,1,_exit" *> "$base.sim.log"
  if(!(Test-Path "$base.bin")){Get-Content "$base.sim.log";throw 'FIR simulation incomplete'}
  $b=[IO.File]::ReadAllBytes((Join-Path $root "$base.bin"))
  $m=for($i=0;$i -lt 8;$i++){[BitConverter]::ToUInt32($b,520+$i*4)}
  if($m[6] -ne 0x600dcafe){throw 'FIR completion marker missing'}
  $reference=[IO.File]::ReadAllBytes((Join-Path $root 'work\fir_cli\fir_output.bin'))
  $errors=0;for($i=0;$i -lt 520;$i++){if($b[$i] -ne $reference[$i]){$errors++}}
  if($errors){throw "FIR $variant differs from verified output"}
  $empty=[long]$m[1]-$m[0];$cycles=[long]$m[3]-$m[2];$macs=$m[4]*$m[5]
  [pscustomobject]@{variant=$variant;macs=$macs;empty_cycles=$empty;call_cycles=$cycles;net_cycles=$cycles-$empty;cycles_per_mac=[math]::Round($cycles/$macs,6);net_cycles_per_mac=[math]::Round(($cycles-$empty)/$macs,6);macs_per_cycle=[math]::Round($macs/$cycles,6);output_errors=$errors}
 }
 $rows | Export-Csv 'work\cnn_asm\fir_results.csv' -NoTypeInformation
 $rows | Format-Table -AutoSize
}finally{Pop-Location}
