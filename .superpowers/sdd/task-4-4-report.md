# Task 4.4 MPEG-TS Preflight and Planner Contract Report

## Result

- Closed the Task 4.4 review findings with one `MediaRealtimeTsInputPlanValidator`, invoked after planning and again at executable graph construction.
- Replaced positional public-program stream inference with immutable `{streamIndex, elementaryPid}` bindings copied from `AVStream::id`; selection now cross-validates exact PID membership independent of vector order.
- Made prepared buffer transfer explicit and fallible. Generic and MPEG-TS prepared buffers reject a second release, MPEG-TS sessions reject a second transfer, and runtime rejects a planner/prepared kind mismatch before output.
- Required planner-owned expected binding kind in every runtime binding and explicitly reject empty injected generic openers.
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

Task 4.4C RED: planner test compilation failed because the focused
`MediaRealtimeTsInputPlanValidator` API did not exist.

GREEN:

```powershell
ctest --test-dir out/build/x64-debug -C Debug -L deterministic --output-on-failure
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -R "^media_transcode_integration_tests$"
```

- Deterministic: 6/6 passed.
- Integration: 1/1 passed in 86.95 seconds with the exactly-once opener coverage enabled.
- Task 4.4C focused integration: 1/1 passed in 86.50 seconds after ownership,
  expected-kind, and executable-transfer coverage was added.
- Task 4.4C clean-first verification removed 256 build outputs and rebuilt all
  257 steps; deterministic tests passed 6/6 and integration passed 1/1 in
  89.22 seconds in the final rerun.

## Verification Notes

- A first clean attempt failed because two integration processes started during diagnosis still held the executable. After terminating only those owned processes, a second `--clean-first` removed 255 files and rebuilt all targets.
- Protocol timeout options and demux probe/analyze options are separate dictionaries.
- The runtime demux node remains Task 4.5; this task transfers but does not consume the MPEG-TS session.

## Remaining Risks

- The live UDP integration duration is sensitive to sender-before-bind timing and should remain monitored for variance.
- Task 4.5 must validate and consume every serialized MPEG-TS session field without downstream defaults.
- Task 4.5 remains responsible for consuming the transferred MPEG-TS session; Task 4.4C proves the transfer boundary and exactly-once ownership only.
