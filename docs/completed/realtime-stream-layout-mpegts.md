# Realtime Stream Layout and MPEG-TS UDP

## Completed

- Added explicit realtime input type, input stream layout, and output stream layout models.
- Mapped existing realtime URL input to `Url + SessionDescribed + SeparateStreams`.
- Mapped existing RTP port input to `RtpPort + SeparateStreams + SeparateStreams`.
- Added `MpegTsUdp + MuxedTransportStream + MuxedTransportStream` planning and graph construction.
- Reused `RealtimeInputNode` for FFmpeg-managed input and `FileOutput + FileMux` for MPEG-TS output.
- Updated realtime CLIs to require explicit stream type and layout arguments.

## Verification

- Built `media_transcode_core`, `media_transcode_graph_transcode_cli`, `media_transcode_realtime_rtp_input_cli`, and `media_transcode_realtime_graph_tests`.
- Ran `out/build/x64-debug/media_transcode_realtime_graph_tests.exe`.
- Ran `ctest --test-dir out/build/x64-debug --output-on-failure`.
- Checked the configured MPEG-TS source with `ffprobe`; `udp://192.168.96.154:15666` and `udp://@192.168.96.154:15666` returned invalid data, and forced `-f mpegts` probing did not finish stream discovery, so live smoke is blocked by the external stream payload state.

## Remaining Risks

- Live MPEG-TS runtime behavior still needs verification once a readable MPEG-TS UDP source is available.
- SDP input, external packet push input, RTP bundled input, and RTP bundled output remain intentionally unsupported.
