# Realtime RTSP to RTP Video Transcode

## Completed

- Added the realtime RTP graph builder for video-only URL input.
- Added planner probing for realtime URLs with RTSP transport, open/read timeout, probe size, analyze duration, and low-latency options.
- Added automatic chain selection through the existing planner. Hardware is preferred by default and `--disable-hw` forces software selection.
- Added explicit rejection for raw RTP and SDP input in this phase.
- Added URL userinfo redaction for CLI and planner diagnostics.
- Added runtime nodes for realtime input, RTP output, RTP muxing, and SDP writing.
- Added RTP mux diagnostics for first packet output and final written packet count.
- Added monotonic timestamp rescaling after video filtering so RTSP jitter or time-base quantization does not stop realtime RTP output with duplicate encoder PTS values.
- Added CLI mode:

```powershell
media_transcode_graph_transcode_cli.exe --mode realtime-rtp --input rtsp://... --rtp-host 127.0.0.1 --rtp-port 5004 --sdp realtime-rtp.sdp --duration 15
```

## Validation Commands

```powershell
cmake --build out/build/x64-debug --target media_transcode_core
cmake --build out/build/x64-debug --target media_transcode_graph_transcode_cli
cmake --build out/build/x64-debug --target media_transcode_realtime_graph_tests
out/build/x64-debug/media_transcode_realtime_graph_tests.exe
```

RTSP smoke test:

```powershell
out/build/x64-debug/media_transcode_graph_transcode_cli.exe --mode realtime-rtp --input rtsp://<redacted>@example.invalid:554/Streaming/Channels/301 --rtp-host 127.0.0.1 --rtp-port 5004 --sdp out/build/x64-debug/realtime-rtp.sdp --duration 15
```

Receiver validation can use the generated SDP while the CLI is running:

```powershell
ffplay -protocol_whitelist file,udp,rtp -i out/build/x64-debug/realtime-rtp.sdp
ffprobe -protocol_whitelist file,udp,rtp -i out/build/x64-debug/realtime-rtp.sdp
```

Latest local smoke result with the redacted Channel 301 RTSP source selected `cuda-nvenc`, wrote 312 RTP mux packets, and an independent UDP receiver observed 3138 RTP datagrams with H264 payloads including FU-A IDR and non-IDR fragments. No duplicate timestamp failure was observed.

## Risks

- The RTSP smoke test depends on the camera being reachable from the development machine.
- Hardware availability depends on the installed FFmpeg build and local GPU driver stack.
- RTP playback requires SDP metadata or explicit payload details.
- The current CLI and runtime diagnostics confirm selected chain, SDP generation, and RTP mux packet output. Detailed decoded/encoded frame counters are still deferred.
