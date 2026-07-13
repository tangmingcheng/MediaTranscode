# Task 4.5B Dedicated MPEG-TS Demux Report

## Result

- Added explicit `Acquiring`, `Locked`, and `ReacquireRequired` source-clock readiness to packet timing and projection history.
- Added `MpegTsDemuxNode` as the exclusive owner of the prepared MPEG-TS session. It replays preflight evidence before position observation, advances raw and projected retention before incremental replay, and queries immutable historical calibration by packet position.
- Video and audio use independent PTS/DTS mappers while sharing the selected PCR projection. Missing timestamps remain absent and the owned `AVPacket` timestamps are not rewritten.
- Synchronized MPEG-TS graphs use `RealtimeInput -> MpegTsDemux`; generic URL input retains `Demux`, and raw RTP remains direct.
- Planner output serializes selected program identities, PCR policy, capacities, regression, timestamp time base, and initial source/raw generations. Validation rejects incomplete plans.

## TDD Evidence

- RED: packet source timing did not expose explicit readiness.
- GREEN: readiness is required at construction and preserved by `FFmpegPacketBuffer`.
- RED: projection checkpoints did not expose acquisition/reacquisition state.
- GREEN: initial unlocked, locked, and post-discontinuity checkpoints report explicit readiness.
- RED: MPEG-TS executable topology had no `MpegTsDemux` kind.
- GREEN: the executable contains one dedicated MPEG-TS demux and no generic demux.

## Verification

```powershell
cmd /d /s /c "call D:\VisualStudio2026\Common7\Tools\VsDevCmd.bat -arch=x64 -host_arch=x64 && cmake --build out\build\x64-debug --config Debug --clean-first --target media_transcode_node_tests media_transcode_builder_tests media_transcode_integration_tests"
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -R media_transcode_node_tests
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -R media_transcode_builder_tests
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -R media_transcode_integration_tests
```

- Clean-first rebuilt 246 compilation/link steps successfully.
- Node tests passed 1/1.
- Builder tests passed 1/1.
- Integration tests passed; `LastTest.log` records `realtime graph tests passed` in 77.15 seconds.

## Remaining Risk

- Deterministic node-level injected-session coverage is still thinner than the complete lifecycle matrix in the brief; timeout, cancellation, duplicate bind, overflow, rollback, and timestamp preservation are covered primarily by the underlying session/projection/mapper components and the live integration path.
- Initial generation parsing currently accepts the planner-produced non-negative integer range; a future externally supplied 64-bit generation would need a dedicated unsigned node-option parser.
