# Active goal ledger —21.8s streaming keyword test

User objective: >=15s merged multi-keyword audio, inference every100ms,
one new MFCC update per20ms hop retaining prior48 rows, exactly one event
per spoken occurrence with no duplicates, omissions or wrong labels.

## Authoritative artifacts

- `manifest.json`, `long_keywords.wav`:21.8s,12 spoken occurrences spanning10
  classes; sequence go,left,go,go,right,on,off,stop,yes,no,up,down. First
  filename-sorted source per class, no prediction-based source selection.
- `prepare.py`:1090 incoming320-sample blocks,1090 overlapping640-sample
  frames (first left-padded),218 inference ticks. Native input contains PCM
  and model only; control input separately contains PC features/scores.
- `stream_core.c`:49x10 feature ring, retained48 hash check, VAD/voting/quiet
  rearm; no source-label, expected sequence or ground-truth timestamp lookup.
- `stream_target.c`:real MemDMA320-sample arrivals, one new feature update,
  native218 NN calls, FFT twiddle restore via DMA after each CNN overlay,
  per-frame/inference/event traces and20ms CPU deadline check at513MHz.
- `validate.py`:strictly checks all1090 rows,218 snapshots/times, actual
  network scores vs independent golden, DMA/call counts, twiddle CRC, no
  ring corruption, guards/deadlines, and exact12 ground-truth events/times.
- `native.status.json` and `control.status.json`:parent/runner PIDs, start
  time, ELF/image SHA256 and final validation state. OS process checks are
  authoritative for whether workers are still alive; heartbeat alone is not.

## Progress and failures

1. PC rolling-window scores and decoder calibration achieved all12 events.
   This is development evidence, NOT completion of native acceptance.
2. First BF533 control run faulted on an unaligned DWORD store in a
   10-byte-stride feature journal. RETX=0xFFA116B4. Failed artifacts retained
   in `archive_unaligned/`. This was a real failure, not a passing wait.
3. Replaced strided memcpy with explicit volatile byte copies throughout
   the new streaming controller. Corrected control has passed first50 rows,
   10 ticks with zero errors; full validation still pending.
4. Bulk embedded ELF fixture was probed independently:all sampled addresses
   matched the input image. `embedded.ldf` reserves0x01000000..0x011FFFFF;
   `prepare_embedded.py` builds native/control fixture sections. The
   fixture change preserves data and model arithmetic, not precomputed NN.
5. Corrected native run launched with fatal-exception breakpoints and
   observable stdout progress. It may take hours; DO NOT restart a live run
   because observations time out or logs have not advanced.

Current corrected workers (check current JSON and OS before trusting these):
- Native parent73740, runner82144, started2026-11-01 15:50:21 China time,
  ELF SHA256 6AC3097E12D8CBB016C62391BD7614F97EE3C5C7515563E77768C68147A5CF87.
- Control parent66432, runner81068, started15:48:51. Corrected control has
  reached virtual8s,400 rows/80 ticks, events go,left,go,go,right, errors0.
- Old stopped native runner83356 and failed control81252 are archived,
  not current jobs. They had the demonstrated unaligned journaling defect.
- Current workers use bulk embedded ELF fixtures. Full mode contains no
  PC feature/score fixture. Do NOT revert to legacy datafile workers.

## Continuation protocol

Read `native.status.json`; confirm its runnerPid and parentPid with actual
Get-Process/Get-CimInstance, matching CCES_runner command line and start time.
Poll that same live run. Parse only STREAM/PROGRESS/EVENT lines from
`native.sim.log`; dot lines only flush CCES_runner's host stdout buffer.
The test is accelerated causal event-driven simulation, not wall-clock
SPORT pacing:20ms arrivals and100ms inference stamps, idle cycles omitted,
all measured jobs checked against configured20ms CPU budget.

If status becomes Failed, inspect fatal PC/RETX in sim logs or strict
validation log and fix the demonstrated cause. Do not treat missing final
dump during a live process as terminal.

Native success requires native_report.json goal_verified=true AND a fresh
strict validator pass over stream_native.bin. Control replay cannot satisfy
the goal. Recheck original requirements before update_goal complete.

At16:05 native runner82144 was live with522.7 CPU seconds and no final
dump/exception. An independent short probe of the SAME native ELF reached
the first `cnn_dw3` entry normally (first_layer_probe.log), proving the first
normal conv returns; it did not replace/restart the main run. Native can be
much slower than Control because it executes all218 real CNNs. Keep waiting
on the existing live handle; do not infer a hang from sparse1-second virtual
progress lines. Bulk ELF and strided-copy fixes are already in this live ELF.

## Not yet proved

- Native full21.8s completion and all218 actual network outputs.
- Native exactly12 events/no duplicate/no omission/no wrong label.
- Control replay validation is now PASSED, see control_report.json:
  1090 blocks/updates,218 snapshots/scores match independent golden,
  exactly12 events, duplicate/missing/wrong=0,deadline misses=0.
  It explicitly has goal_verified=false and actual_native_network_calls=0.

This goal turn is PROGRESS:created the21.8s fixture, implemented and tested
the controller, fixed a demonstrated alignment exception, and launched the
corrected full native calculation. At2026-11-01 16:00 China time native
runner82144 and parent73740 were confirmed live matching this ELF/job;
Native remains unproven. Do not mark the goal complete from Control alone.

Keep goal active until these gates pass. Physical SPORT, board cache/SDRAM
timings and actual hardware timer pacing remain outside this software
simulation; never describe the simulation as an on-board measurement.

## Resume audit2026-11-01

After the human pause/resume, SAME native runner82144 remained live. No
restart occurred. A read-only memory inspection of that owned simulator
process (`peek_native.py`, no write/debug/stop rights) found the actual app
header advancing beyond the buffered log:100 completed frames,20 actual
NNs,errors0,maxjob3697997 cycles,CRC restores20,50 FFT frames plus50 exact
zero shortcuts. This is non-atomic live progress, never final acceptance.

An independent `STREAM_SWITCH_PROBE` regression passed:nonzero PCM MFCC,
one real CNN, DMA restore with CRC, then MFCC of SAME frame matched exactly.
Evidence:`switch_probe.bin`, `switch_probe_report.json`, `switch_probe.sim.log`.
It has mode2 and whole_stream_acceptance=false, not a replacement long run.

If stdout progress seems old, inspect `peek_native.py` in addition to OS
process checks. Do not halt/restart the live job merely for sparse stdout.
Current native final dump/report still pending. Previous goal turn was a
verified wait with progress evidence; this resumed turn added the verified
transition test and live actual counters. No blocker requiring human input.

2026-11-01 16:06 China: verified Native runner82144 live. First virtual1000ms completed:50 feature updates,10 actual NN calls,events0 (leading silence),errors0,maxjob2882390 cycles versus10260000 budget. This is verified progress, not whole-stream success. Continue same process; native final dump/report still absent.

2026-11-01 17:02 China live snapshot:114 completed blocks/115 feature updates (non-atomic job in progress),22 completed actual NN calls,events1,errors0,deadline_misses0,maxjob3697997 cycles. Native stdout confirms first true event EVENT t=2100 label=11(go) votes=8 peak=126, matching first occurrence. Current runner82144 unchanged, final dump still absent; all12/native whole21.8s remain required.

2026-11-01 19:13 China resume: same runner82144 live,325 feature updates/64 completed real NN calls; native events go@2100,left@3800,go@5400, all matching first3 occurrences,errors0/deadline0. Full21.8s and all12 remain unproven; do not restart.

USER STOPPED2026-11-01 22:10 China. Goal cleared; do not resume automatically.464 completed frames/92 real NN calls,5 correct events,error/deadline0; whole stream unverified. See停止时进展.md.
