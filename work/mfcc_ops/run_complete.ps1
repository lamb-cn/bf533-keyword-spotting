param([int]$Timeout=600000,[switch]$Focused)
$ErrorActionPreference='Stop'
$cc='C:\Analog Devices\CrossCore Embedded Studio 2.10.0'
Push-Location (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent)
try {
 $extra=@();if($Focused){$extra+="-DFOCUSED"}
 & "$cc\ccblkfn.exe" -proc ADSP-BF533 -si-revision 0.6 -O -g @extra -T work\cnn_asm\bench.ldf work\mfcc_ops\complete_test.c work\mfcc_ops\frontend.c work\mfcc_ops\complete.c work\mfcc_ops\tables.c work\mfcc_ops\dot.asm work\mfcc_ops\dct.asm work\mfcc_ops\window.asm work\mfcc_ops\quantize.asm -o work\mfcc_ops\complete.dxe *> work\mfcc_ops\complete_build.log
 if($LASTEXITCODE){Get-Content work\mfcc_ops\complete_build.log;throw 'Build failed'}
 $syms=& "$cc\elfsyms.exe" work\mfcc_ops\complete.dxe
 $address=[regex]::Match(($syms -join "`n"),'(?m)^_result = (0x[0-9a-fA-F]+);').Groups[1].Value
 if(!$address){throw 'Missing result'}
 Remove-Item work\mfcc_ops\complete_result.bin -ErrorAction SilentlyContinue
 & "$cc\CCES_runner.exe" --target 'ADSP-BF5xx Blackfin Family Simulators' --platform 'ADSP-BF5xx Single Processor Simulator' --processor ADSP-BF533 --timeout $Timeout -v1 -c '0,work\mfcc_ops\complete.dxe' -b '0,_exit' -d "0,out,work\mfcc_ops\complete_result.bin,binary,$address,48,1,_exit" *> work\mfcc_ops\complete_sim.log
 if($LASTEXITCODE){Get-Content work\mfcc_ops\complete_sim.log;throw 'Runner failed'}
 if(!(Test-Path work\mfcc_ops\complete_result.bin)){Get-Content work\mfcc_ops\complete_sim.log;throw 'No completion dump'}
 $bytes=[IO.File]::ReadAllBytes((Join-Path (Get-Location) 'work\mfcc_ops\complete_result.bin'))
 $r=0..11 | ForEach-Object {[BitConverter]::ToUInt32($bytes,$_ * 4)}
 $r | ConvertTo-Json | Set-Content work\mfcc_ops\complete_result.json
 Write-Output "done=$($r[0]) log_errors=$($r[1]) quantize_errors=$($r[2]) frame_errors=$($r[3]) guard_errors=$($r[4]) log_cycles=$($r[5]) quantize_cycles=$($r[6]) frame0=$($r[7]) frame1=$($r[8]) frame2=$($r[9])"
 if($r[0] -ne 0x600dcafe -or ($r[1]+$r[2]+$r[3]+$r[4])){throw 'Numerical tests FAILED'}
}finally{Pop-Location}


