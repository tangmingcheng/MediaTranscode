# Priority 4 Capability Boundaries

## Completed

- Added explicit stable and unsupported capability maturity states.
- Rejected incomplete optimizer, generic GPU, mesh, remote, and multi-node distributed execution paths.
- Removed defaults and success reports that implied unapplied optimization or execution.
- Added fixed-size audio encoder framing so sample-rate conversion naturally drives AAC re-encoding.

## Verification

Executed test commands:

```powershell
ctest --preset deterministic
ctest --preset integration
ctest --preset hardware
ctest --preset performance
```

Executed local gates (the script loops the sample for at least 60 seconds, probes both streams, and fully decodes every output):

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\acceptance\run_priority4_local_gate.ps1 -ExecutableDirectory .\out\build\x64-debug -InputPath .\out\build\x64-debug\test.mp4 -DurationSeconds 60
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\acceptance\run_priority4_local_gate.ps1 -ExecutableDirectory .\out\build\x64-debug -InputPath .\out\build\x64-debug\test.mp4 -DurationSeconds 60 -DisableHw
```

Executed realtime gates. The script starts the sender and receiver, samples CPU only from `media_transcode_realtime_video_cli`, probes H264/AAC, fully decodes the capture, rejects worker/decode/drop/stall errors, and gates final A/V timestamp drift:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\acceptance\run_priority4_realtime_gate.ps1 -Mode rtp -ExecutableDirectory .\out\build\x64-debug -InputPath .\out\build\x64-debug\test.mp4 -InputPort 34100 -OutputPort 35100 -DurationSeconds 60
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\acceptance\run_priority4_realtime_gate.ps1 -Mode rtp -ExecutableDirectory .\out\build\x64-debug -InputPath .\out\build\x64-debug\test.mp4 -InputPort 34200 -OutputPort 35200 -DurationSeconds 60 -DisableHw
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\acceptance\run_priority4_realtime_gate.ps1 -Mode mpegts -ExecutableDirectory .\out\build\x64-debug -InputPath .\out\build\x64-debug\test.mp4 -InputPort 34300 -OutputPort 35300 -DurationSeconds 60
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\acceptance\run_priority4_realtime_gate.ps1 -Mode mpegts -ExecutableDirectory .\out\build\x64-debug -InputPath .\out\build\x64-debug\test.mp4 -InputPort 34400 -OutputPort 35400 -DurationSeconds 60 -DisableHw
```

Results:

- Deterministic 6/6, integration 1/1, hardware 1/1, and performance 1/1 passed.
- Local H264/AAC 48 kHz hardware: 9 loops, 65.02 seconds, all probes and full decodes passed.
- Local H264/AAC 48 kHz software: 12 loops, 63.29 seconds, all probes and full decodes passed.
- Separate RTP hardware: average CLI CPU 1.58%, P95 5.51%, final A/V drift 31 ms.
- Separate RTP software: average CLI CPU 6.01%, P95 8.15%, final A/V drift 28 ms.
- MPEG-TS UDP hardware: average CLI CPU 1.68%, P95 5.10%, final A/V drift 7 ms.
- MPEG-TS UDP software: average CLI CPU 5.94%, P95 8.20%, final A/V drift 7 ms.
- All realtime paths ran for 60 seconds; H264/AAC probes, full decode, zero worker/decode/drop/stall errors, and the 100 ms drift gate passed.

Runtime FFmpeg binaries were copied manually beside executables. No external runtime path or dependency-copy behavior was added to CMake.
