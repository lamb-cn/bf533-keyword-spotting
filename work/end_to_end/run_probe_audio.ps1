param([int]$Timeout=600000)
$ErrorActionPreference='Stop'
$cc='C:\Analog Devices\CrossCore Embedded Studio 2.10.0'
Push-Location (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent)
try {
 & "$cc\ccblkfn.exe" -proc ADSP-BF533 -si-revision 0.6 -O -g -T work\cnn_asm\bench.ldf work\end_to_end\dma_probe_audio.c -o work\end_to_end\probe_audio.dxe *> work\end_to_end\probe_audio_build.log
 if($LASTEXITCODE){Get-Content work\end_to_end\probe_audio_build.log;throw 'Build failed'}
 $syms=& "$cc\elfsyms.exe" work\end_to_end\probe_audio.dxe
 $address=[regex]::Match(($syms -join "`n"),'(?m)^_result = (0x[0-9a-fA-F]+);').Groups[1].Value
 if(!$address){throw 'Missing result'}
 Remove-Item work\end_to_end\probe_audio_result.bin -ErrorAction SilentlyContinue
 & "$cc\CCES_runner.exe" --target 'ADSP-BF5xx Blackfin Family Simulators' --platform 'ADSP-BF5xx Single Processor Simulator' --processor ADSP-BF533 --timeout $Timeout -v1 -c '0,work\end_to_end\probe_audio.dxe' -b '0,_exit' -d '0,in,work\end_to_end\image.bin,binary,0x01000000,all,1' -d "0,out,work\end_to_end\probe_audio_result.bin,binary,$address,48,1,_exit" *> work\end_to_end\probe_audio_sim.log
 if($LASTEXITCODE){Get-Content work\end_to_end\probe_audio_sim.log;throw 'Runner failed'}
 if(!(Test-Path work\end_to_end\probe_audio_result.bin)){Get-Content work\end_to_end\probe_audio_sim.log;throw 'No completion dump'}
 $bytes=[IO.File]::ReadAllBytes((Join-Path (Get-Location) 'work\end_to_end\probe_audio_result.bin'))
 $r=0..11 | ForEach-Object {[BitConverter]::ToUInt32($bytes,$_ * 4)}
 $r | ConvertTo-Json | Set-Content work\end_to_end\probe_audio_result.json
 Write-Output "done=$($r[0]) data_errors=$($r[1]) irq_count=$($r[2]) irq_status=$($r[3]) wait_cycles=$($r[4]) final_status=$($r[5])"
 if($r[0] -ne 0x600dcafe -or $r[1] -ne 0 -or $r[2] -ne 1 -or ($r[3] -band 2)){throw 'Numerical tests FAILED'}
}finally{Pop-Location}



