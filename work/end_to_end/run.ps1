param([int]$Timeout=1800000,[switch]$Legacy,[switch]$Accuracy)
$oldModel=$env:BF533_KWS13
$env:BF533_KWS13=if($Legacy){'0'}else{'1'}
$ErrorActionPreference='Stop'
$cc='C:\Analog Devices\CrossCore Embedded Studio 2.10.0'
Push-Location (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent)
try{
 $python='C:\Users\lenovo\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
 $folder='data/speech_commands_test_small'
 $fixtures=if($Legacy){
  if($Accuracy){throw 'Accuracy mode requires the 13-class model'}
  @([pscustomobject]@{file=$null;label='go'})
 }else{
  $entries=Get-Content "$folder/manifest.json" -Raw | ConvertFrom-Json
  if($Accuracy){$entries}else{$entries | Where-Object label -eq 'go' | Select-Object -First 1}
 }
 $tag=if($Legacy){''}else{'13'}
 $defines=@(if(!$Legacy){'-DKWS13'})
 $image="work/end_to_end/image$tag.bin"
 $binary="target$tag.bin"
 $report="target_report$tag.json"
 $results=@()
 foreach($fixture in $fixtures){
  if(!$Legacy){
   & $python work/end_to_end/prepare.py --audio "$folder/$($fixture.file)"
   if($LASTEXITCODE){throw 'Fixture preparation failed'}
  }
 $sources=@('work\end_to_end\pipeline.c','work\mfcc_ops\frontend.c','work\mfcc_ops\complete.c','work\mfcc_ops\tables.c','work\mfcc_ops\dot.asm','work\mfcc_ops\dct.asm','work\mfcc_ops\window.asm','work\mfcc_ops\quantize.asm','work\cnn_asm\cnn_ops.c','work\cnn_asm\tail_ops.c','work\cnn_asm\tail_ops.asm','work\cnn_asm\kernels.asm')
 $sources+=@('work\end_to_end\frontend32.c','work\end_to_end\window32.c')
 if(!$results.Count){
 & "$cc\ccblkfn.exe" -proc ADSP-BF533 -si-revision 0.6 -O -g @defines -T work\cnn_asm\bench.ldf @sources -o work\end_to_end\pipeline.dxe *> work\end_to_end\build.log
 if($LASTEXITCODE){Get-Content work\end_to_end\build.log;throw 'Build failed'}
 }
 Remove-Item work/end_to_end/$binary -ErrorAction SilentlyContinue
 & "$cc\CCES_runner.exe" --target 'ADSP-BF5xx Blackfin Family Simulators' --platform 'ADSP-BF5xx Single Processor Simulator' --processor ADSP-BF533 --timeout $Timeout -v1 -c '0,work\end_to_end\pipeline.dxe' -b '0,_exit' -d "0,in,$image,binary,0x01000000,all,1" -d "0,out,work/end_to_end/$binary,binary,0x01400000,73152,1,_exit" *> work\end_to_end\sim.log
 if($LASTEXITCODE -or !(Test-Path work/end_to_end/$binary)){Get-Content work\end_to_end\sim.log;throw 'Run failed or timed out'}
 $verified=& $python work/end_to_end/verify.py --binary $binary --report $report
 if($LASTEXITCODE){throw 'Target validation failed'}
 $v=$verified | Out-String | ConvertFrom-Json
 $results+= [pscustomobject]@{audio=$v.audio;label=$fixture.label;predicted=$v.host_predicted;correct=$v.host_predicted -ceq $fixture.label}
 Write-Output "$($v.audio): $($v.host_predicted) (expected $($fixture.label))"
 }
 if($Accuracy){
  $correct=@($results | Where-Object correct).Count
  $summary=[ordered]@{samples=$results.Count;correct=$correct;accuracy=$correct/$results.Count;results=$results}
  $summary | ConvertTo-Json -Depth 4 | Set-Content work/kws13/simulator_accuracy.json
  Write-Output ("BF533 simulator accuracy {0}/{1} ({2:P2})" -f $correct,$results.Count,$summary.accuracy)
 }
}finally{
 Pop-Location
 $env:BF533_KWS13=$oldModel
}
