param([int]$Timeout=1800000)
$ErrorActionPreference='Stop'
$cc='C:\Analog Devices\CrossCore Embedded Studio 2.10.0'
Push-Location (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent)
try{
 $sources=@('work\end_to_end\pipeline.c','work\mfcc_ops\frontend.c','work\mfcc_ops\complete.c','work\mfcc_ops\tables.c','work\mfcc_ops\dot.asm','work\mfcc_ops\dct.asm','work\mfcc_ops\window.asm','work\mfcc_ops\quantize.asm','work\cnn_asm\cnn_ops.c','work\cnn_asm\tail_ops.c','work\cnn_asm\tail_ops.asm','work\cnn_asm\kernels.asm')
 $sources+=@('work\end_to_end\frontend32.c','work\end_to_end\window32.c')
 & "$cc\ccblkfn.exe" -proc ADSP-BF533 -si-revision 0.6 -O -g -T work\cnn_asm\bench.ldf @sources -o work\end_to_end\pipeline.dxe *> work\end_to_end\build.log
 if($LASTEXITCODE){Get-Content work\end_to_end\build.log;throw 'Build failed'}
 Remove-Item work\end_to_end\target.bin -ErrorAction SilentlyContinue
 & "$cc\CCES_runner.exe" --target 'ADSP-BF5xx Blackfin Family Simulators' --platform 'ADSP-BF5xx Single Processor Simulator' --processor ADSP-BF533 --timeout $Timeout -v1 -c '0,work\end_to_end\pipeline.dxe' -b '0,_exit' -d '0,in,work\end_to_end\image.bin,binary,0x01000000,all,1' -d '0,out,work\end_to_end\target.bin,binary,0x01400000,73152,1,_exit' *> work\end_to_end\sim.log
 if($LASTEXITCODE -or !(Test-Path work\end_to_end\target.bin)){Get-Content work\end_to_end\sim.log;throw 'Run failed or timed out'}
 & 'C:\Users\lenovo\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' work\end_to_end\verify.py
 if($LASTEXITCODE){throw 'Target validation failed'}
}finally{Pop-Location}
