# Priority 5 Performance and Stability Baselines

## Completed tooling

- Added a versioned six-scenario configuration for local, separate RTP, and MPEG-TS hardware/software paths.
- Added a single serial baseline driver that invokes the existing separate local and realtime CLIs.
- Added structured JSON reports with machine, encoding path, CLI CPU, working set, threads, queue high-water mark, maximum progress gap, drops, errors, stalls, and A/V drift.
- Added a deterministic report-contract test to the performance CI tier.

## Commands

Run the deterministic report contract:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\acceptance\test_priority5_reporting.ps1
```

Run all six 60-second baselines:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\acceptance\run_priority5_baseline.ps1 -DurationSeconds 60 -ReportPath .\out\reports\priority5\smoke-all-60s.json
```

Run the configured stability matrix (local hardware/software 30 minutes, RTP hardware 60 minutes/software 30 minutes, and MPEG-TS hardware 60 minutes/software 30 minutes):

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\acceptance\run_priority5_baseline.ps1 -ReportPath .\out\reports\priority5\stability-final.json
```

## 60-second baseline

- Six scenarios passed with natural H264/AAC re-encoding and full decode validation.
- Hardware paths explicitly selected CUDA/NVENC; software paths explicitly selected libx264.
- Four realtime runs accumulated 240 seconds with zero drops, worker errors, runtime errors, or stalled intervals.
- Maximum A/V drift was 20 ms and maximum observed progress gap was 1.03 seconds.
- Peak working set was 539,549,696 bytes, peak thread count was 83, and queue high-water mark was 200.
- Average CLI CPU was 1.40%/5.17% for RTP and 1.67%/5.52% for MPEG-TS (hardware/software respectively).

Runtime FFmpeg binaries remain beside executables and outside CMake configuration.
