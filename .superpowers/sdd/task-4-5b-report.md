# Task 4.5B Dedicated MPEG-TS Demux Report

## Result

- Added explicit `Acquiring`, `Locked`, and `ReacquireRequired` source-clock readiness to packet timing and projection history.
- Added `MpegTsDemuxNode` as the exclusive owner of the prepared MPEG-TS session. It replays preflight evidence before position observation, advances raw and projected retention before incremental replay, and queries immutable historical calibration by packet position.
- Video and audio use independent PTS/DTS mappers while sharing the selected PCR projection. Missing timestamps remain absent and the owned `AVPacket` timestamps are not rewritten.
- Mapper state is retained independently per stream and source generation, so legal read-ahead rollback cannot reset a newer generation. Signed packet timestamps are normalized into the MPEG-TS 33-bit domain.
- Synchronized MPEG-TS graphs use `RealtimeInput -> MpegTsDemux`; generic URL input retains `Demux`, and raw RTP remains direct.
- Planner output serializes selected program identities, PCR policy, capacities, regression, timestamp time base, and initial source/raw generations. Validation rejects incomplete plans.
- Added a dedicated `MediaTsDemuxSession` injection boundary. Prepared buffers transfer it exclusively and declare Metadata/FormatContext descriptors. The node validates the transferred session contract and rejects duplicate bindings.

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
- Final clean-first rebuilt all 260 steps; deterministic passed 6/6 and integration passed 1/1 in 89.82 seconds.
- Deterministic demux tests cover wrong/missing inputs and options, duplicate binding, complete and incremental evidence, read-ahead rollback, discontinuity/reacquisition, cross-generation history, negative positions, stream/program mismatch, missing evidence, projection overflow, Waiting/EOF/failure/cancellation, stop/abort, missing PTS/DTS, and raw timestamp preservation.

## Remaining Risk

- Initial generation parsing currently accepts the planner-produced non-negative integer range; a future externally supplied 64-bit generation would need a dedicated unsigned node-option parser.
