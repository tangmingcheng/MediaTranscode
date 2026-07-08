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
- Added raw RTP video fmtp parsing and planner validation for H264/HEVC SDP parameter sets.
- Added planner-controlled timestamp synthesis for raw RTP video frames that decode without PTS.
- Removed redundant realtime CLI enable switches for hardware planning, low latency input, and graph diagnostics; these default on, with only `--disable-hw`, `--no-low-latency`, and `--quiet-graph` as overrides.
- Added planner-owned RTP output URL local port parameters so FFmpeg uses ephemeral local RTP/RTCP source ports while preserving the public `basePort` and `basePort + 2` destination ports.

## Verification

- Built debug targets: `media_transcode_core`, `media_transcode_graph_transcode_cli`, `media_transcode_realtime_rtp_input_cli`, and `media_transcode_realtime_graph_tests`.
- Ran `out/build/x64-debug/media_transcode_realtime_graph_tests.exe`.
- Ran `ctest --test-dir out/build/x64-debug --output-on-failure`.
- Ran remote raw RTP smoke from `root@192.168.96.211` using `/home/firefly/Downloads/test16s.mp4` to local `192.168.96.154`; output SDP contained audio and video media sections, and runtime reported `rtp_mux.first_packet_written stream=video`, `rtp_mux.stop packets_written=15`, `rtp_mux.stop packets_written=233`, `encodedPacketsPushed=249`, and `raw RTP realtime validation stopped successfully`.
- Ran local raw RTP smoke from `out/build/x64-debug/test.mp4` through `D:\mabs\local64\bin-video\ffmpeg.exe` using default hardware, low latency, and diagnostics CLI behavior; planner selected `cuda-nvenc`, runtime reported `rtp_mux.first_packet_written stream=audio`, `rtp_mux.first_packet_written stream=video`, `rtp_mux.stop packets_written=150`, `rtp_mux.stop packets_written=259`, `encodedPacketsPushed=410`, and `raw RTP realtime validation stopped successfully`; output SDP preserved `m=video 25700` and `m=audio 25702` and no longer emitted `bind failed: Error number -10048 occurred`.

## Notes

- The remote `h264_rkmpp` RTP sender emits H264 fmtp without `sprop-parameter-sets`; the successful raw RTP smoke used `-c:v copy` from the MP4 so FFmpeg could publish complete H264 SDP metadata.
- The earlier Windows UDP `bind failed: Error number -10048 occurred` warning was caused by RTP output opening naked `rtp://host:port` URLs and leaving local source port selection to FFmpeg on the same host. The planner now appends `localrtpport=0&localrtcpport=0` to RTP output URLs so FFmpeg uses ephemeral local source ports without changing the advertised media ports.
