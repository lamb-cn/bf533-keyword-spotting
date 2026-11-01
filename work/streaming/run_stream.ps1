param([ValidateSet('Control','Native')][string]$Mode='Control',[int]$Timeout=172800000)
$ErrorActionPreference='Stop'
$cc='C:\Analog Devices\CrossCore Embedded Studio 2.10.0'
$python='C:\Users\lenovo\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
$root=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$tag=$Mode.ToLowerInvariant();$statusFile=Join-Path $PSScriptRoot "$tag.status.json"
$status=[ordered]@{mode=$Mode;state='Building';parentPid=$PID;runnerPid=$null;startedUtc=[DateTime]::UtcNow.ToString('o')}
function Save-Status {$status | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $statusFile -Encoding utf8}
Push-Location $root
try{
 Save-Status
 $sources=@('work\streaming\stream_target.c','work\streaming\stream_core.c','work\mfcc_ops\frontend.c','work\mfcc_ops\complete.c','work\mfcc_ops\tables.c','work\mfcc_ops\dot.asm','work\mfcc_ops\dct.asm','work\mfcc_ops\window.asm','work\mfcc_ops\quantize.asm','work\cnn_asm\cnn_ops.c','work\cnn_asm\tail_ops.c','work\cnn_asm\tail_ops.asm','work\cnn_asm\kernels.asm','work\end_to_end\frontend32.c','work\end_to_end\window32.c')
 $extra=@('-DSTREAM_INSTRUMENT','-DSTREAM_PROGRESS_FLUSH','-DSTREAM_EMBEDDED');if($Mode -eq 'Control'){$extra+='-DSTREAM_REPLAY'}
 $image=if($Mode -eq 'Control'){'control_image.bin'}else{'native_image.bin'}
 Copy-Item (Join-Path $PSScriptRoot $image) (Join-Path $PSScriptRoot "$tag.input.bin")
 Copy-Item (Join-Path $PSScriptRoot 'manifest.json') (Join-Path $PSScriptRoot "$tag.manifest.json")
 Copy-Item (Join-Path $PSScriptRoot 'features_float.bin') (Join-Path $PSScriptRoot "$tag.features_float.bin")
 Copy-Item 'work\end_to_end\weights.npz' (Join-Path $PSScriptRoot "$tag.weights.npz")
 Copy-Item (Join-Path $PSScriptRoot 'long_keywords.wav') (Join-Path $PSScriptRoot "$tag.long_keywords.wav")
 & $python work\streaming\prepare_embedded.py --tag $tag
 if($LASTEXITCODE){throw 'Embedding failed'}
 $sources+="work\streaming\fixture_$tag.c"
 & "$cc\ccblkfn.exe" -proc ADSP-BF533 -si-revision 0.6 -O -g -T work\streaming\embedded.ldf @extra @sources -o "work\streaming\stream_$tag.dxe" *> "work\streaming\$tag.build.log"
 if($LASTEXITCODE){Get-Content "work\streaming\$tag.build.log";throw 'Build failed'}
 $elf=Join-Path $PSScriptRoot "stream_$tag.dxe"
 $status['elfSha256']=(Get-FileHash -LiteralPath $elf -Algorithm SHA256).Hash
 $syms=& "$cc\elfsyms.exe" $elf
 $syms | Set-Content "work\streaming\$tag.symbols.txt"
 $fatal=[regex]::Match(($syms -join "`n"),'(?m)^_adi_fatal_exception = (0x[0-9a-fA-F]+);').Groups[1].Value
 $fatalError=[regex]::Match(($syms -join "`n"),'(?m)^_adi_fatal_error = (0x[0-9a-fA-F]+);').Groups[1].Value
 if(!$fatal -or !$fatalError){throw 'Missing fatal breakpoint symbols'}
 $status['imageSha256']=(Get-FileHash (Join-Path $PSScriptRoot "$tag.input.bin") -Algorithm SHA256).Hash
 $bin=Join-Path $PSScriptRoot "stream_$tag.bin";Remove-Item -LiteralPath $bin -ErrorAction SilentlyContinue
 $runnerArgs=@('--target','ADSP-BF5xx Blackfin Family Simulators','--platform','ADSP-BF5xx Single Processor Simulator','--processor','ADSP-BF533','--timeout',"$Timeout",'-v1','-c',"0,work\streaming\stream_$tag.dxe",'-b',"0,_exit,$fatal,$fatalError",'-d',"0,out,work\streaming\stream_$tag.bin,binary,0x01400000,196608,1,_exit")
 $quoted=($runnerArgs | ForEach-Object {'"'+$_+'"'}) -join ' '
 $proc=Start-Process -FilePath "$cc\CCES_runner.exe" -ArgumentList $quoted -WorkingDirectory $root -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $PSScriptRoot "$tag.sim.log") -RedirectStandardError (Join-Path $PSScriptRoot "$tag.sim.err.log")
 $status.state='Running';$status.runnerPid=$proc.Id;$status['runnerStartedUtc']=$proc.StartTime.ToUniversalTime().ToString('o');Save-Status
 while(!$proc.WaitForExit(30000)){$status['heartbeatUtc']=[DateTime]::UtcNow.ToString('o');Save-Status}
 $proc.WaitForExit();$status['runnerExitCode']=$proc.ExitCode
 if($proc.ExitCode -ne 0 -or !(Test-Path -LiteralPath $bin)){throw 'Runner failed or no completion dump'}
 $status.state='Validating';Save-Status
 & $python work\streaming\validate.py --mode $tag --manifest "$tag.manifest.json" --features "$tag.features_float.bin" --weights "$tag.weights.npz" *> "work\streaming\$tag.validation.log"
 if($LASTEXITCODE){Get-Content "work\streaming\$tag.validation.log";throw 'Strict streaming validation failed'}
 $status.state='Passed';$status['completedUtc']=[DateTime]::UtcNow.ToString('o');Save-Status
 Get-Content "work\streaming\$tag.validation.log"
}catch{
 $status.state='Failed';$status['error']=$_.Exception.Message;$status['completedUtc']=[DateTime]::UtcNow.ToString('o');Save-Status;throw
}finally{Pop-Location}
