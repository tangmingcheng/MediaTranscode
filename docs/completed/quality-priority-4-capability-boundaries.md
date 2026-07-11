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

Executed local gates (the script loops the sample for at least 60 seconds, confirms CUDA/NVENC for hardware runs, and probes and fully decodes every loop output):

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\acceptance\run_priority4_local_gate.ps1 -ExecutableDirectory .\out\build\x64-debug -InputPath .\out\build\x64-debug\test.mp4 -DurationSeconds 60
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\acceptance\run_priority4_local_gate.ps1 -ExecutableDirectory .\out\build\x64-debug -InputPath .\out\build\x64-debug\test.mp4 -DurationSeconds 60 -DisableHw
```

Executed realtime gates. The script starts the sender and receiver, confirms CUDA/NVENC for hardware runs, samples CPU only from `media_transcode_realtime_video_cli`, probes H264/AAC, fully decodes the capture, rejects nonzero runtime queue-drop/error/stall metrics, and gates final A/V timestamp drift:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\acceptance\run_priority4_realtime_gate.ps1 -Mode rtp -ExecutableDirectory .\out\build\x64-debug -InputPath .\out\build\x64-debug\test.mp4 -InputPort 34100 -OutputPort 35100 -DurationSeconds 60
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\acceptance\run_priority4_realtime_gate.ps1 -Mode rtp -ExecutableDirectory .\out\build\x64-debug -InputPath .\out\build\x64-debug\test.mp4 -InputPort 34200 -OutputPort 35200 -DurationSeconds 60 -DisableHw
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\acceptance\run_priority4_realtime_gate.ps1 -Mode mpegts -ExecutableDirectory .\out\build\x64-debug -InputPath .\out\build\x64-debug\test.mp4 -InputPort 34300 -OutputPort 35300 -DurationSeconds 60
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\acceptance\run_priority4_realtime_gate.ps1 -Mode mpegts -ExecutableDirectory .\out\build\x64-debug -InputPath .\out\build\x64-debug\test.mp4 -InputPort 34400 -OutputPort 35400 -DurationSeconds 60 -DisableHw
```

Results:

- Deterministic 6/6, integration 1/1, hardware 1/1, and performance 1/1 passed.
- Local H264/AAC 48 kHz hardware: 9 loops, 67.44 seconds; CUDA/NVENC and every loop probe/full decode passed.
- Local H264/AAC 48 kHz software: 11 loops, 60.54 seconds; every loop probe/full decode passed.
- Separate RTP hardware: average CLI CPU 1.57%, P95 5.38%, final A/V drift 24 ms.
- Separate RTP software: average CLI CPU 5.88%, P95 8.66%, final A/V drift 24 ms.
- MPEG-TS UDP hardware: average CLI CPU 1.51%, P95 5.23%, final A/V drift 7 ms.
- MPEG-TS UDP software: average CLI CPU 5.49%, P95 8.59%, final A/V drift 7 ms.
- All realtime paths ran for 60 seconds; H264/AAC probes, full decode, zero worker/error/queue-drop/stall metrics, and the 100 ms drift gate passed. Hardware paths explicitly selected CUDA/NVENC.

Runtime FFmpeg binaries were copied manually beside executables. No external runtime path or dependency-copy behavior was added to CMake.
