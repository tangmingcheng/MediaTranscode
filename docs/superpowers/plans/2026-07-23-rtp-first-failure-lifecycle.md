# RTP First-Failure Lifecycle Plan

1. Add failing runtime tests proving the first worker `ErrorInfo` survives graph abort and peer channel-abort races.
2. Add a failing diagnostic test for the `RawRtpInput` node-kind name.
3. Add a testable realtime CLI lifecycle component and failing tests for state-aware stop/error precedence.
4. Preserve the primary worker failure through worker, threaded executor, runtime synchronization, and CLI reporting.
5. Reproduce RTP startup with complete runtime logs and fix the actual first failure if it is deterministic; the observed H264 FU-A mid-fragment startup must discard the orphaned continuation and resynchronize instead of aborting the graph. Apply the same packet-loss recovery contract to HEVC FU input. The A/V planner must explicitly select planned stream-pair association with RFC5506 reduced-size RTCP for FFmpeg-compatible sender reports; raw transports and the clock group must derive their CNAME policy from that immutable plan. Do not treat `output channel aborted` or AAC close diagnostics as the cause.
6. Run clean-first focused runtime/CLI builds and tests with `/showIncludes` disabled.

## Completion evidence

- `media_transcode_node_tests.exe`: passed, including RTP clock-group and depacketizer recovery coverage.
- `media_transcode_integration_tests.exe --task5-av-sync-input`: passed.
- Direct FFmpeg-to-realtime-CLI RTP run after the planner-owned RTCP policy fix: completed with 3,139 encoded packets pushed, `workerErrors=0`, `errors=0`, and `droppedBuffers=0`.
- The five-second idle-progress gate observes worker progress until encoded output starts, then requires encoded-output progress. A separate absolute first-output deadline prevents infinite startup without misclassifying initial GPU setup as an idle stall.
- The full runtime test executable still has nine pre-existing failures in FFmpeg ownership and A/V-sync bootstrap cases; long-run post-write drift telemetry and automatic group reacquisition remain follow-up work.
