param([switch]$Test)
$ErrorActionPreference='Stop'
$root=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$cc='C:\Analog Devices\CrossCore Embedded Studio 2.10.0'
Push-Location $root
try{
 $name=if($Test){'test'}else{'capture'};$entry=if($Test){'test.c'}else{'main.c'}
 $extra=@();if($Test){$extra+='-DBOARD_CAPTURE_TEST'}
 & "$cc\ccblkfn.exe" -proc ADSP-BF533 -si-revision 0.6 -O -g @extra -T work\cnn_asm\bench.ldf "board\codec_capture\$entry" board\codec_capture\capture.c work\mfcc_ops\dot.asm -o "board\codec_capture\$name.dxe" *> "board\codec_capture\$name.build.log"
 if($LASTEXITCODE){Get-Content "board\codec_capture\$name.build.log";throw 'Build failed'}
 if($Test){
  $syms=& "$cc\elfsyms.exe" "board\codec_capture\$name.dxe"
  $addr=[regex]::Match(($syms -join "`n"),'(?m)^_result = (0x[0-9a-fA-F]+);').Groups[1].Value
  Remove-Item board\codec_capture\test.bin -ErrorAction SilentlyContinue
  & "$cc\CCES_runner.exe" --target 'ADSP-BF5xx Blackfin Family Simulators' --platform 'ADSP-BF5xx Single Processor Simulator' --processor ADSP-BF533 --timeout 180000 -v1 -c '0,board\codec_capture\test.dxe' -b '0,_exit' -d "0,out,board\codec_capture\test.bin,binary,$addr,48,1,_exit" *> board\codec_capture\test.sim.log
  if($LASTEXITCODE -or !(Test-Path board\codec_capture\test.bin)){throw 'No completed test'}
  $bytes=[IO.File]::ReadAllBytes((Join-Path $root 'board\codec_capture\test.bin'))
  $r=0..11|ForEach-Object {[BitConverter]::ToUInt32($bytes,$_ *4)}
  $r|ConvertTo-Json|Set-Content board\codec_capture\test_result.json
  if($r[0] -ne 0x600dcafe -or ($r[1..6]|Measure-Object -Sum).Sum){throw 'Test failed'}
  Write-Output 'PASS:conversion,FIR/3x decimation,32-row wraps,overrun and sequence faults'
 }
}finally{Pop-Location}
