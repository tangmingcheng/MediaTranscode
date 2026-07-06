# Realtime RTP Transcode Completed Log

## 2026-07-06
- Created implementation plan in `plan.md`.
- Started branch `codex/realtime-rtp-transcode-dag`.
- Added realtime RTP graph validation and shape test target.
- Added realtime RTP graph builder path with `MediaRealtimeGraphKind::RtpTranscode`.
- Added realtime input/output configuration models and option propagation.
- Added minimal FFmpeg realtime input/output session wrappers.
- Implemented realtime input, RTP output, RTP mux, and SDP writer runtime node skeletons.
- Extended graph CLI with realtime RTP dry-run graph construction arguments.
- Fixed CLI `--help` exit handling so help tests return success.
- Extended realtime graph unit tests for `RtpTranscode` validation and graph shape:
  empty URL, raw RTP without SDP/hint, raw RTP with SDP, raw RTP with codec hint,
  invalid odd RTP port, missing media branch, and video-only URL-to-RTP DAG.
- Extracted shared `FFmpegMuxWriter` from file mux behavior and reused it from `FileMuxNode` and `RtpMuxNode`.
- Changed realtime RTP output to create one RTP output/mux chain per media branch and a shared SDP writer.
- Changed SDP writing to happen after RTP mux stream registration/header, with support for multiple RTP contexts.
- Wired raw RTP inline SDP and codec hints into runtime by synthesizing SDP text before `FFmpegRealtimeInputSession` opens FFmpeg.
- Added graph tests for raw RTP hint SDP propagation and audio+video dual RTP output/mux shape.
- Added explicit realtime source stream indexes for URL/SDP inputs, with codec-hint order inference only for raw RTP hint mode.
- Added CLI options for `--video-stream-index` and `--audio-stream-index`.
- Adjusted realtime RTP CLI branch selection so a single `--video-stream-index` builds a video-only graph unless an audio stream index is also supplied.
- Added validation that raw RTP codec-hint mode must use an input URL carrying a valid RTP port before SDP synthesis.

## Verification Evidence
- `cmake --build out/build/x64-release --target media_transcode_core media_transcode_graph_transcode_cli media_transcode_realtime_graph_tests` completed.
- `cmake --build out/build/x64-debug --target media_transcode_core media_transcode_graph_transcode_cli media_transcode_realtime_graph_tests` completed.
- `cmake --build out/build/x64-debug --target media_transcode_core` completed.
- `cmake --build out/build/x64-release --target media_transcode_core` completed.
- `ctest --test-dir out/build/x64-debug -C Debug --output-on-failure` passed 3/3 tests with PATH including local build FFmpeg DLL directories.
- `ctest --test-dir out/build/x64-release --output-on-failure` passed 3/3 tests with PATH including local build FFmpeg DLL directories.
- `out/build/x64-release/media_transcode_graph_transcode_cli.exe --help` returned 0 and printed `--realtime-rtp`.
- `cmake --build out/build/x64-debug --target media_transcode_realtime_graph_tests --clean-first` completed.
- `cmake --build out/build/x64-release --target media_transcode_realtime_graph_tests --clean-first` completed.
- `out/build/x64-release/media_transcode_graph_transcode_cli.exe --realtime-rtp --input rtp://127.0.0.1:5004 --output-host 127.0.0.1 --base-port 5006 --sdp realtime-cli.sdp --media-id video-main --video-stream-index 0 --queue-capacity 16 --dry-run` returned 0 and printed `realtime rtp graph build done: nodes=13 edges=18`.

## Open Items
- Add runtime dry-run tests that avoid external network dependencies.
- Configure CTest to discover local FFmpeg DLLs without requiring manual PATH setup.
- Add loopback RTP integration tests once port/timing skips are in place.
- Add true probe/planner stream selection before broad live source support; v1 URL/SDP mode requires explicit source stream indexes, and raw RTP hint mode infers only from explicit hint order.
