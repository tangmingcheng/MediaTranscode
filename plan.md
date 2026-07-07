# Raw UDP/RTP Realtime Transcode Plan

## Goal

Implement H264-only raw UDP/RTP input in the active graph runtime and add a dedicated tool that validates raw RTP input to realtime RTP output.

## Implementation

- Add planner-owned raw RTP input support for `rtp://host:port` and `udp://host:port`.
- Keep `RealtimeUrl` input rejecting RTP, UDP, and SDP URLs.
- Require raw RTP input metadata: `h264`, dynamic payload type, and `90000` clock rate.
- Generate the minimal H264 SDP in the planner and pass it to a raw RTP input node.
- Add `RawRtpInput` as an appended node kind and register its runtime node.
- Use `RawRtpInput -> Demux -> PacketNormalize -> VideoDecode -> VideoEncode -> RtpMux`.
- Extract shared FFmpeg realtime input option handling and one RTP URL parser helper.
- Add `media_transcode_realtime_rtp_input_cli` under `tools/realtime_rtp_input_cli`.
- Replace fixed realtime sleep behavior in the new tool with a runtime report polling controller.

## Verification

- Build `media_transcode_core`, `media_transcode_graph_transcode_cli`, `media_transcode_realtime_rtp_input_cli`, and `media_transcode_realtime_graph_tests` in debug.
- Run `out/build/x64-debug/media_transcode_realtime_graph_tests.exe`.
- Repeat available release builds.
- Run a live H264 RTP sender, transcode it with `media_transcode_realtime_rtp_input_cli`, and validate the generated SDP with `ffplay` when GUI playback is available.

## Completion

- Review the full diff after tests.
- Fix any review findings in the same branch.
- Commit and push branch `codex/raw-rtp-realtime-transcode`.
- Open a PR and request a fresh review agent.
