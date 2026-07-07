# Raw UDP/RTP Realtime Transcode

## Completed

- Added H264-only raw RTP input planning for `rtp://host:port` and `udp://host:port`.
- Kept realtime URL input separate; `--input-kind url` still rejects raw RTP, UDP, and SDP URLs.
- Added planner-owned raw RTP SDP generation from explicit codec, payload type, and clock rate metadata.
- Added `RawRtpInputNode` and wired raw RTP through `RawRtpInput -> Demux -> PacketNormalize -> VideoDecode -> VideoEncode -> RtpMux`.
- Added shared FFmpeg realtime input option handling and shared RTP/UDP URL parsing.
- Added `media_transcode_realtime_rtp_input_cli` for raw RTP validation with runtime report based progress checks.

## Validation

```powershell
cmake --build out/build/x64-debug --target media_transcode_core media_transcode_graph_transcode_cli media_transcode_realtime_rtp_input_cli media_transcode_realtime_graph_tests
out/build/x64-debug/media_transcode_realtime_graph_tests.exe
cmake --build out/build/x64-release --target media_transcode_core media_transcode_graph_transcode_cli media_transcode_realtime_rtp_input_cli media_transcode_realtime_graph_tests
out/build/x64-release/media_transcode_realtime_graph_tests.exe
```

## Manual Smoke

Run the raw RTP CLI first, then start an H264 RTP sender to `rtp://127.0.0.1:5004`.
The local validation used `D:\mabs\local64\bin-video\ffmpeg.exe` with `testsrc2`
encoded as H264 RTP and repeated H264 headers:

```powershell
out/build/x64-debug/media_transcode_realtime_rtp_input_cli.exe --input rtp://127.0.0.1:5004 --rtp-codec h264 --rtp-payload-type 96 --rtp-clock-rate 90000 --open-timeout-ms 8000 --read-timeout-ms 8000 --analyze-duration-us 500000 --probe-size 524288 --low-latency --rtp-host 127.0.0.1 --rtp-port 5006 --sdp out/build/x64-debug/raw-rtp-output.sdp --packet-size 1200 --disable-hw --quiet-graph --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --video-codec h264 --rc auto --max-duration 8 --progress-timeout-ms 12000
```

Validate the output while the CLI is running:

```powershell
D:\mabs\local64\bin-video\ffmpeg.exe -hide_banner -loglevel error -protocol_whitelist file,udp,rtp -i out/build/x64-debug/raw-rtp-output.sdp -t 4 -f null -
```

The local smoke completed with receiver exit code 0, CLI progress counters increasing,
`workerErrors=0`, and `raw RTP realtime validation stopped successfully`.

## Remaining Risks

- Raw RTP v1 is H264-only.
- RTP over UDP is supported; MPEG-TS over UDP is intentionally out of scope.
- Audio RTP input/output remains out of scope.
- Live smoke depends on a reachable sender and local UDP playback tooling.
