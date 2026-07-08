# Realtime RTP Audio Transcode and Codec Plan

## Goal

Add realtime RTP audio transcoding in the active DAG graph path and remove the raw RTP video H264-only limitation without introducing fallback logic outside the planner.

## Implementation

- Keep realtime work under `src/internal/graph`; do not use the older `src/realtime` path.
- Extend `MediaRealtimeRtpTranscodeRequest` so raw RTP input has explicit video and audio endpoint metadata.
- Add planner-owned RTP codec descriptors for H264, HEVC, AAC, and Opus; generate raw RTP SDP from descriptors.
- Add realtime audio planning for known URL/raw RTP input and require enabled audio to have a real source.
- Split RTP output into one `RtpOutput + RtpMux` chain per media stream; derive video from `basePort` and audio from `basePort + 2`.
- Reuse `MediaAudioBranchSegmentBuilder` for realtime audio decode, resample, and encode.
- Update `RtpMuxNode` so each instance handles exactly one media kind, video or audio.
- Update `SdpWriterNode` so it waits for all expected RTP format contexts and writes one aggregated SDP.
- Update realtime CLIs to parse explicit raw RTP video/audio endpoint arguments and realtime audio transcode arguments.

## Verification

- Add failing tests in `tests/unit/test_realtime_rtp_graph.cpp` before production edits.
- Build `media_transcode_core`, `media_transcode_graph_transcode_cli`, `media_transcode_realtime_rtp_input_cli`, and `media_transcode_realtime_graph_tests`.
- Run `out/build/x64-debug/media_transcode_realtime_graph_tests.exe`.
- Run `ctest --test-dir out/build/x64-debug --output-on-failure`.
- If a reachable live source is available, run an RTSP/URL audio+video RTP smoke using FFmpeg from `D:\mabs\local64\bin-video`.

## Completion

- Write a short completion note under `docs/completed/`.
- Review the full diff against this plan and fix any missing items.
- Commit and push branch `codex/realtime-audio-rtp-codecs`.
- Open a PR and request a fresh agent review.
