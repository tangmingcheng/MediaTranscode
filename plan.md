# Realtime Stream Layout and MPEG-TS UDP Plan

## Goal

Make realtime input type, input stream layout, and output stream layout explicit in the DAG realtime transcode request, while preserving existing realtime URL and raw RTP separate-stream behavior and adding MPEG-TS over UDP input to MPEG-TS muxed output.

## Implementation

- Add `RealtimeInputType`, `RealtimeInputStreamLayout`, and `RealtimeOutputStreamLayout` under `src/internal/graph/model`.
- Replace the old realtime input kind field with explicit input type, input layout, and output layout fields in `MediaRealtimeRtpTranscodeRequest`.
- Keep existing URL realtime input mapped to `Url + SessionDescribed + SeparateStreams`.
- Keep existing raw RTP separate-port input mapped to `RtpPort + SeparateStreams + SeparateStreams`.
- Add `MpegTsUdp + MuxedTransportStream + MuxedTransportStream` planning.
- Keep all support-matrix decisions in `MediaRealtimeRtpTranscodePlanner`; unsupported combinations must fail before graph build.
- Reuse `RealtimeInputNode` for MPEG-TS UDP input so FFmpeg opens the `udp://` URL directly.
- Reuse `MediaOutputSegmentBuilder::buildFileMuxOutput` for MPEG-TS output with `format=mpegts`.
- Keep RTP output graph construction on the existing `RtpOutput + RtpMux + SdpWriter` path.
- Update realtime CLI arguments to parse `--input-type`, `--input-layout`, and `--output-layout`.

## Verification

- Add failing tests in `tests/unit/test_realtime_rtp_graph.cpp` before production edits.
- Build `media_transcode_core`, `media_transcode_graph_transcode_cli`, `media_transcode_realtime_rtp_input_cli`, and `media_transcode_realtime_graph_tests`.
- Run `out/build/x64-debug/media_transcode_realtime_graph_tests.exe`.
- Run `ctest --test-dir out/build/x64-debug --output-on-failure`.
- If reachable, smoke MPEG-TS UDP with FFmpeg tools from `D:\mabs\local64\bin-video` and stream `192.168.96.154:15666`.

## Completion

- Write a short completion note under `docs/completed/`.
- Review the full diff against this plan and fix any missing items.
- Commit and push branch `codex/realtime-stream-layout-mpegts`.
- Open a PR and request a fresh agent review.
