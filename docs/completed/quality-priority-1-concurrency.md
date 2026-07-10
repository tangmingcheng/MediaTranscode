# QUALITY_SCORE Priority 1: Concurrency And Fault Injection

## Outcome

- Added deterministic queue, wakeup, lifecycle, stop-race, EOF, and fault-injection coverage.
- Made queue push outcomes explicit and preserved pending FFmpeg data across backpressure.
- Added fair multi-input arbitration and terminal-node retirement without busy waiting.
- Added runtime acceptance metrics for CPU, memory, threads, queue high-water marks, progress stalls, and errors.
- Reused a single prepared realtime input session from preflight through runtime, avoiding live UDP reopen and startup packet loss.
- Made runtime compilation transactional so a failed compile preserves the previous executable graph.

## Test Commands And Results

```powershell
out\build\x64-debug\media_transcode_event_runtime_tests.exe
```

Result: passed after a full rebuild; repeated 20 consecutive times with zero failures.

```powershell
out\build\x64-debug\media_transcode_realtime_graph_tests.exe
```

Result: passed after a full rebuild.

```powershell
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure
```

Result: passed, 2/2 tests.

```powershell
out\build\x64-debug\codex_av_scheduler_smoke.ps1
```

Result: separate RTP hardware smoke passed for 60 seconds; 1663 decoded frames, zero drops, duplicates, worker errors, decode errors, and progress stalls. `media_transcode_realtime_video_cli` average CPU was 0.90%.

```powershell
out\build\x64-debug\codex_mpegts_smoke.ps1
```

Result: MPEG-TS hardware and software smokes passed for 60 seconds. Hardware produced 1761 frames at 0.86% CLI CPU; software produced 1697 frames at 16.04% average CLI CPU and 24.85% P95. Both contained H264 and AAC with zero drops, duplicates, worker errors, decode errors, and progress stalls.

Local H264/AAC capacity-one regression passed for hardware and software. Both preserved 248 video frames and 357 audio frames; CLI CPU was 4.28% for hardware and 22.85% for software.

## Remaining Risks

- Raw RTP can still report a startup-only missed-packet warning when the sender begins before all receivers bind, although the validated run had no decode errors or frame loss.
- The prepared-input object currently crosses the planner/runtime boundary; Priority 2 will narrow this ownership boundary while splitting responsibilities.
