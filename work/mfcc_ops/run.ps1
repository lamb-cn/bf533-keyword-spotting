param([int]$Timeout=600000)
$ErrorActionPreference='Stop'
$cc='C:\Analog Devices\CrossCore Embedded Studio 2.10.0'
Push-Location (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent)
try {
 & "$cc\ccblkfn.exe" -proc ADSP-BF533 -si-revision 0.6 -O -g -T work\cnn_asm\bench.ldf work\mfcc_ops\test.c work\mfcc_ops\frontend.c work\mfcc_ops\tables.c work\mfcc_ops\dot.asm work\mfcc_ops\dct.asm work\mfcc_ops\window.asm -o work\mfcc_ops\fe.dxe *> work\mfcc_ops\fe_build.log
 if($LASTEXITCODE){Get-Content work\mfcc_ops\fe_build.log;throw 'Build failed'}
 $syms=& "$cc\elfsyms.exe" work\mfcc_ops\fe.dxe
 $address=[regex]::Match(($syms -join "`n"),'(?m)^_result = (0x[0-9a-fA-F]+);').Groups[1].Value
 if(!$address){throw 'Missing result'}
 Remove-Item work\mfcc_ops\fe_result.bin -ErrorAction SilentlyContinue
 & "$cc\CCES_runner.exe" --target 'ADSP-BF5xx Blackfin Family Simulators' --platform 'ADSP-BF5xx Single Processor Simulator' --processor ADSP-BF533 --timeout $Timeout -v1 -c '0,work\mfcc_ops\fe.dxe' -b '0,_exit' -d "0,out,work\mfcc_ops\fe_result.bin,binary,$address,48,1,_exit" *> work\mfcc_ops\fe_sim.log
 if($LASTEXITCODE){Get-Content work\mfcc_ops\fe_sim.log;throw 'Runner failed'}
 if(!(Test-Path work\mfcc_ops\fe_result.bin)){Get-Content work\mfcc_ops\fe_sim.log;throw 'No completion dump'}
 $bytes=[IO.File]::ReadAllBytes((Join-Path (Get-Location) 'work\mfcc_ops\fe_result.bin'))
 $r=0..11 | ForEach-Object {[BitConverter]::ToUInt32($bytes,$_ * 4)}
 $r | ConvertTo-Json | Set-Content work\mfcc_ops\fe_result.json
 Write-Output "done=$($r[0]) window_mag_errors=$($r[1]) fft_errors=$($r[2]) mel_dct_errors=$($r[3]) window_cycles=$($r[4]) fft_cycles=$($r[5]) magnitude_cycles=$($r[6]) mel_cycles=$($r[7]) dct_cycles=$($r[8]) guard_errors=$($r[9])"
 if($r[0] -ne 0x600dcafe -or ($r[1]+$r[2]+$r[3]+$r[9])){throw 'Numerical tests FAILED'}
}finally{Pop-Location}

