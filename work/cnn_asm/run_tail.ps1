param([int]$Timeout=600000)
$ErrorActionPreference='Stop'
$cc='C:\Analog Devices\CrossCore Embedded Studio 2.10.0'
Push-Location (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent)
try {
 & "$cc\ccblkfn.exe" -proc ADSP-BF533 -si-revision 0.6 -O -g -T work\cnn_asm\bench.ldf work\cnn_asm\tail_test.c work\cnn_asm\tail_ops.c work\cnn_asm\tail_ops.asm work\cnn_asm\kernels.asm -o work\cnn_asm\tail.dxe *> work\cnn_asm\tail_build.log
 if($LASTEXITCODE){Get-Content work\cnn_asm\tail_build.log;throw 'Build failed'}
 $syms=& "$cc\elfsyms.exe" work\cnn_asm\tail.dxe
 $address=[regex]::Match(($syms -join "`n"),'(?m)^_result = (0x[0-9a-fA-F]+);').Groups[1].Value
 if(!$address){throw 'Missing result'}
 Remove-Item work\cnn_asm\tail_result.bin -ErrorAction SilentlyContinue
 & "$cc\CCES_runner.exe" --target 'ADSP-BF5xx Blackfin Family Simulators' --platform 'ADSP-BF5xx Single Processor Simulator' --processor ADSP-BF533 --timeout $Timeout -v1 -c '0,work\cnn_asm\tail.dxe' -b '0,_exit' -d "0,out,work\cnn_asm\tail_result.bin,binary,$address,48,1,_exit" *> work\cnn_asm\tail_sim.log
 if($LASTEXITCODE){Get-Content work\cnn_asm\tail_sim.log;throw 'Runner failed'}
 if(!(Test-Path work\cnn_asm\tail_result.bin)){Get-Content work\cnn_asm\tail_sim.log;throw 'No completion dump'}
 $bytes=[IO.File]::ReadAllBytes((Join-Path (Get-Location) 'work\cnn_asm\tail_result.bin'))
 $r=0..11 | ForEach-Object {[BitConverter]::ToUInt32($bytes,$_ * 4)}
 $r | ConvertTo-Json | Set-Content work\cnn_asm\tail_result.json
 Write-Output "done=$($r[0]) post_errors=$($r[1]) pool_errors=$($r[2]) fc_errors=$($r[3]) guards=$($r[4]) post_cycles=$($r[5]) pool_cycles=$($r[6]) fc_core_cycles=$($r[7]) fc_full_cycles=$($r[8])"
 if($r[0] -ne 0x600dcafe -or ($r[1]+$r[2]+$r[3]+$r[4])){throw 'Numerical tests FAILED'}
}finally{Pop-Location}
