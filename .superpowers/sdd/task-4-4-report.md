# Task 4.4 MPEG-TS Preflight and Planner Contract Report

## Result

- Added immutable public `AVProgram` snapshots and cross-validated them against parser PAT/PMT inventory through `MediaTsProgramSelector`.
- Added a tagged move-only prepared input binding for generic FFmpeg input or one MPEG-TS session.
- Added planner-owned `MediaRealtimeTsInputPlan` with exact `mpegts` demux identity, 188-byte packets, AVIO/datagram sizing, evidence capacity, and packet-position regression.
- Defined evidence capacity as worst-case checkpoints for the probe window plus runtime rollback plus one retained predecessor, with checked arithmetic.
- Split generic and MPEG-TS preflight openers. Production opens `MediaTsInputSession` once and transfers that same session to runtime.
- Removed fixed program/PMT/video/audio/PCR identities. The cross-validated selected program now supplies `MediaAvSyncPlanner`, including a distinct PCR PID.

## TDD Evidence

RED:

```powershell
cmd.exe /d /s /c "call D:\VisualStudio2026\Common7\Tools\VsDevCmd.bat -arch=x64 -host_arch=x64 >nul && cmake --build out\build\x64-debug --config Debug --target media_transcode_planner_tests"
```

Result: compilation failed because `MediaRealtimeTsInputPlan` and its capacity contract did not exist.

GREEN:

```powershell
ctest --test-dir out/build/x64-debug -C Debug -L deterministic --output-on-failure
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -R "^media_transcode_integration_tests$"
```

- Deterministic: 6/6 passed.
- Integration: 1/1 passed in 86.95 seconds with the exactly-once opener coverage enabled.

## Verification Notes

- A first clean attempt failed because two integration processes started during diagnosis still held the executable. After terminating only those owned processes, a second `--clean-first` removed 255 files and rebuilt all targets.
- Protocol timeout options and demux probe/analyze options are separate dictionaries.
- The runtime demux node remains Task 4.5; this task transfers but does not consume the MPEG-TS session.

## Remaining Risks

- The live UDP integration duration is sensitive to sender-before-bind timing and should remain monitored for variance.
- Task 4.5 must validate and consume every serialized MPEG-TS session field without downstream defaults.
