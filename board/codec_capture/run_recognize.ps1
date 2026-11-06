param(
 [string]$Wav,
 [string]$Cces = 'C:\Analog Devices\CrossCore Embedded Studio 2.10.0',
 [string]$Python = 'C:\Users\lenovo\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe',
 [switch]$BuildOnly,
 [switch]$Listen
)
$ErrorActionPreference = 'Stop'
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$verifySequence = !$Wav
if (!$Listen -and !$BuildOnly) {
 if (!$Wav) { $Wav = Join-Path $root 'board/codec_capture/keywords13.wav' }
 if (!$verifySequence) { $Wav = (Resolve-Path -LiteralPath $Wav).Path }
}

Push-Location $root
try {
 & $Python board/codec_capture/prepare_model.py
 if ($LASTEXITCODE) { throw '13-class model preparation failed' }
 $model = Get-Content board/codec_capture/keywords13.json -Raw | ConvertFrom-Json
 $dxe = 'board/codec_capture/recognize.dxe'
 $log = 'board/codec_capture/recognize.build.log'
 $defines = @('-DSTREAM_BOARD')
 if ($Listen) {
  $defines += '-DBOARD_CONTINUOUS'
  $dxe = 'board/codec_capture/listen.dxe'
  $log = 'board/codec_capture/listen.build.log'
 }
 $sources = @(
  'work/streaming/stream_target.c', 'work/streaming/stream_core.c',
  'board/codec_capture/fixture13.c', 'board/codec_capture/capture.c',
  'work/mfcc_ops/frontend.c', 'work/mfcc_ops/complete.c', 'work/mfcc_ops/tables.c',
  'work/mfcc_ops/dot.asm', 'work/mfcc_ops/dct.asm', 'work/mfcc_ops/window.asm',
  'work/mfcc_ops/quantize.asm', 'work/cnn_asm/cnn_ops.c', 'work/cnn_asm/tail_ops.c',
  'work/cnn_asm/tail_ops.asm', 'work/cnn_asm/kernels.asm',
  'work/end_to_end/frontend32.c', 'work/end_to_end/window32.c'
 )
 $previousErrorPreference = $ErrorActionPreference
 try {
  # Windows PowerShell 5.1 reports redirected native stderr as error records.
  # CCES writes normal progress to stderr, so check its exit code explicitly.
  $ErrorActionPreference = 'Continue'
  & "$Cces\ccblkfn.exe" -proc ADSP-BF533 -si-revision 0.6 -O -g -T work/streaming/embedded.ldf @defines @sources -o $dxe *> $log
  $buildExitCode = $LASTEXITCODE
 } finally {
  $ErrorActionPreference = $previousErrorPreference
 }
 if ($buildExitCode -ne 0) { throw "Board build failed (exit $buildExitCode). See $log" }
 if ($BuildOnly) { return }

 if ($Listen) {
  Write-Host 'Starting BF533. Play audio through the connected headphones. Press Ctrl+C to exit.'
  & "$Cces\CCES_runner.exe" --target 'Blackfin Emulators/EZ-KIT Lites' --platform 'ADSP-BF533 EZ-KIT Lite via Debug Agent' --processor ADSP-BF533 -v1 -c "0,$dxe"
  throw "BF533 listening session ended (runner exit $LASTEXITCODE)."
 }

 $symbols = (& "$Cces\elfsyms.exe" $dxe) -join "`n"
 function Get-SymbolAddress($Name) {
  $match = [regex]::Match($symbols, "(?m)^${Name} = (0x[0-9a-fA-F]+);")
  if (!$match.Success) { throw "Missing symbol $Name" }
  $match.Groups[1].Value.ToLowerInvariant()
 }

 $exit = Get-SymbolAddress '___lib_prog_term'
 $result = Get-SymbolAddress '_board_result'
 $dump = Join-Path $env:TEMP ("bf533-kws-" + [guid]::NewGuid() + ".bin")
 $player = [System.Media.SoundPlayer]::new($Wav)
 $player.Load()
 try {
  $player.PlayLooping()
  & "$Cces\CCES_runner.exe" --target 'Blackfin Emulators/EZ-KIT Lites' --platform 'ADSP-BF533 EZ-KIT Lite via Debug Agent' --processor ADSP-BF533 --timeout 65000 -v1 -c "0,$dxe" -b "0,$exit" -d "0,out,$dump,binary,$result,944,1,$exit"
  if ($LASTEXITCODE -or !(Test-Path -LiteralPath $dump)) { throw 'No hardware result dump' }
 } finally {
  $player.Stop()
  $player.Dispose()
 }

 $bytes = [IO.File]::ReadAllBytes($dump)
 if ($bytes.Length -ne 944) { throw "Incomplete result dump at $dump" }
 $r = 0..235 | ForEach-Object { [BitConverter]::ToUInt32($bytes, $_ * 4) }
 $labels = $model.labels
 $selected = 0
 $manualMask = 0
 $digits = @("four", "five", "six", "seven", "eight", "nine")
 $detected = for ($i = 0; $i -lt [Math]::Min($r[3], 32); $i++) {
  $j = 12 + $i * 4
  $led = $r[140 + $i]
  $word = $labels[$r[$j+1]]
  $digit = [Array]::IndexOf($digits, $word)
  if ($digit -ge 0) {
   $selected = $digit
   $manualMask = $manualMask -bxor (1 -shl $selected)
  } elseif ($word -eq 'left' -or $word -eq 'right') {
   $delta = if ($word -eq 'left') { 5 } else { 1 }
   $selected = ($selected + $delta) % 6
   $manualMask = $manualMask -bxor (1 -shl $selected)
  } elseif ($word -eq 'off') {
   $manualMask = 0
  }
  $expectedMask = if ($word -eq 'go') { 1 -shl $selected } else { $manualMask }
  if ($led -ne $expectedMask) { throw "LED output readback mismatch for event $i ($word)" }
  $word
 }
 Write-Output ("blocks={0} inference={1} events={2} fault={3} dma_errors={4} rows={5} overruns={6} pcm_peak={7} max_job_ms={8:F2} voiced_frames={9}" -f $r[1],$r[2],$r[3],$r[4],$r[5],$r[6],$r[7],$r[8],(1000.0*$r[9]/[Math]::Max(1,$r[10])),$r[11])
 Write-Output "Board result dump $dump"
 if ($r[0] -ne 0x600dcafe -or $r[1] -ne 1750 -or $r[2] -ne 350 -or $r[4] -or $r[5] -or $r[7]) {
  throw 'Hardware run did not complete cleanly'
 }
 if (!$r[3]) { throw 'No keyword detected' }
 if ($verifySequence) {
  $expected = $model.sequence
  $matched = $false
  if ($detected.Count -ge $expected.Count) {
   for ($phase = 0; $phase -lt $expected.Count; $phase++) {
    $window = for ($i = 0; $i -lt $detected.Count; $i++) {
     $expected[($phase + $i) % $expected.Count]
    }
    if (($window -join ',') -ceq ($detected -join ',')) { $matched = $true; break }
   }
  }
  if (!$matched) { throw 'Detected words differ from the looped test sequence' }
  Write-Output 'PASS: all detected words match the cyclic test sequence'
 }
} finally {
 Pop-Location
}
