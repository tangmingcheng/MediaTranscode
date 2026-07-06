# DAG Realtime RTP Transcode Industrial v1 Plan

## Summary
- Build a DAG-based realtime transcode path under `src/internal/graph`.
- Support realtime input URLs and raw RTP only when an SDP or explicit codec hints are supplied.
- Output RTP plus SDP. Do not add public `include/media_transcode` API in v1.
- Work on branch `codex/realtime-rtp-transcode-dag`; commit and push all changes on this branch.

## Interfaces and Types
- Extend realtime graph configuration beyond plain `inputUrl` / `outputUrl`:
  - `MediaRealtimeInputConfig`: input mode, URL, SDP text/path, codec hints, read timeout, reconnect policy.
  - `MediaRtpCodecHint`: stream kind, codec name, payload type, clock rate, channels, fmtp.
  - `MediaRtpOutputConfig`: host, base RTP port, stream RTP/RTCP ports, SDP path, TTL, write timeout.
  - `MediaRealtimeTranscodeOptions`: input, output, transcode parameters, latency, queues, diagnostics.
- Add `MediaRealtimeGraphKind::RtpTranscode`.
- Prefer existing node kinds: `RealtimeInput`, `RtpMux`, `RtpOutput`, `SdpWriter`.
- Append any new `MediaNodeKind` values only if unavoidable.

## Implementation Steps
1. Baseline documentation
   - Create this `plan.md`.
   - Create `docs/completed/realtime-rtp-transcode.md`.
   - Keep existing untracked third-party FFmpeg files out of commits.

2. Realtime input modeling and validation
   - Add input/output/hint models in realtime builder code.
   - Validate non-empty input URL, raw RTP SDP-or-hint requirement, valid even RTP ports, and at least one media branch.
   - Default to low-latency video settings: H.264 preferred, B-frames disabled, realtime queue watermarks.

3. FFmpeg protocol runtime components
   - Add `FFmpegRealtimeInputSession` for open/read/timeout/reconnect with interrupt callback.
   - Add RTP output session support for RTP contexts and SDP generation.
   - Keep packet/frame data flowing through `FFmpegBufferFactory`.

4. Runtime nodes
   - Implement `RealtimeInputNode` to emit format metadata and packets.
   - Implement `RtpOutputNode` to create RTP output context metadata.
   - Implement `RtpMuxNode` by reusing shared mux writing logic rather than duplicating `FileMuxNode`.
   - Implement `SdpWriterNode` with clear missing-path errors.
   - Ensure stop/abort releases FFmpeg resources.

5. DAG construction and planner connection
   - Add `MediaRealtimeRtpTranscodeGraphBuilder`.
   - Build URL/RTP input as: `RealtimeInput -> Demux/StreamSplit -> PacketNormalize -> branch -> RtpMux -> RtpOutput/SdpWriter`.
   - Reuse existing audio/video branch builders and low-latency queue policy.
   - Upgrade `MediaPipelinePreset::RealtimeRtpSkeleton` to use the realtime RTP builder.

6. CLI and verification
   - Extend graph CLI with `--realtime-rtp --input ... --output-host ... --base-port ... --sdp ...`.
   - Add unit tests for config validation and graph shape.
   - Add a test CLI target that can build a realtime RTP graph without requiring network traffic.
   - Verify:
     - `cmake --build out/build/x64-debug --target media_transcode_core`
     - `cmake --build out/build/x64-release --target media_transcode_core`
     - `ctest --test-dir out/build/x64-debug -C Debug --output-on-failure` when tests are enabled.

7. Review, commit, push, PR
   - Run an implementation review and a test/verification review.
   - Commit only this task's files.
   - Push `codex/realtime-rtp-transcode-dag`.
   - Create a PR and have a fresh agent review it.

## Test Cases
- Validation rejects empty input URL.
- Raw RTP without SDP or codec hints fails.
- Raw RTP with complete video hint succeeds.
- Invalid output RTP port fails.
- Video-only URL-to-RTP graph contains realtime input, demux/split, video branch, RTP mux, RTP output, and SDP writer.
- CLI can create and dump a realtime RTP graph from explicit arguments.

## Assumptions and Risks
- Raw RTP without SDP or explicit hint remains unsupported in v1.
- RTP multi-stream output uses RTP contexts plus one generated SDP.
- Network loopback integration can be flaky; default tests should not require open network ports.
- Later work: RTCP stats, FEC/NACK, dynamic bitrate switching, RTP over TCP, SRT/RTSP-specific strategies.
