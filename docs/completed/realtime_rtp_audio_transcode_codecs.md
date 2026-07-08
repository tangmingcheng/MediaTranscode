# Realtime RTP Audio Transcode and Codec Completion

## Completed

- Added explicit raw RTP video and audio endpoint metadata to realtime RTP requests.
- Added planner-owned RTP codec descriptors for H264, HEVC, AAC, and Opus.
- Added realtime audio planning from URL probe results and raw RTP metadata.
- Split realtime RTP output into one `RtpOutput + RtpMux` chain per media stream.
- Reused the graph audio branch segment for realtime audio decode, resample, and encode.
- Updated `RtpMuxNode` to handle one configured media kind per instance.
- Updated `SdpWriterNode` to aggregate multiple RTP format contexts before writing SDP.
- Updated realtime CLIs to parse audio transcode parameters and explicit raw RTP endpoints.

## Verification

- Built debug targets: `media_transcode_core`, `media_transcode_graph_transcode_cli`, `media_transcode_realtime_rtp_input_cli`, and `media_transcode_realtime_graph_tests`.
- Ran `out/build/x64-debug/media_transcode_realtime_graph_tests.exe`.
- Ran `ctest --test-dir out/build/x64-debug --output-on-failure`.

## Not Run

- Live RTSP/URL audio+video smoke was not run because no reachable live source was provided in this session.
