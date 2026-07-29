# MediaTranscode

MediaTranscode is a C++20 and FFmpeg-backed media transcode framework centered on the graph DAG architecture.

The active code path is the graph runtime:

```text
src/internal/graph/        # graph model, planner, builder, runtime, and runtime nodes
tools/local_video_cli/     # local-file video transcode CLI
tools/realtime_video_cli/  # realtime video transcode CLI
include/media_transcode/
    Result.h               # shared Result<T> and ErrorInfo type
```

Only the graph architecture and `Result.h` are documented as active project surfaces.

## Build

Use the existing CMake flow:

```bash
cmake -S . -B out/build/x64-debug
cmake --build out/build/x64-debug --target media_transcode_core
cmake --build out/build/x64-debug --target media_transcode_local_video_cli
cmake --build out/build/x64-debug --target media_transcode_realtime_video_cli
```

Useful CMake options:

```text
MEDIA_TRANSCODE_BUILD_GRAPH_TOOLS=ON
```

## Realtime DAG Path

The current realtime path supports URL/RTSP input, explicit raw RTP input with video and optional audio, and MPEG-TS over UDP input:

```text
RTSP, realtime URL, raw RTP input, or MPEG-TS UDP input
    -> graph planner
    -> realtime DAG builder
    -> graph runtime nodes
    -> per-media RTP output + aggregated SDP, or MPEG-TS muxed output
```

Run the realtime video CLI with an RTSP input:

```powershell
out/build/x64-debug/media_transcode_realtime_video_cli.exe --input-type rtsp --input-layout session --output-layout separate --input rtsp://... --rtsp-transport tcp --open-timeout-ms 5000 --read-timeout-ms 5000 --analyze-duration-us 500000 --probe-size 524288 --rtp-host 127.0.0.1 --rtp-port 5004 --sdp out/build/x64-debug/realtime-rtp.sdp --packet-size 1200 --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --video-codec h264 --rc auto --audio-codec aac --max-duration 15
```

Receiver validation can use the generated SDP while the CLI is running:

```powershell
ffplay -protocol_whitelist file,udp,rtp -i out/build/x64-debug/realtime-rtp.sdp
ffprobe -protocol_whitelist file,udp,rtp -i out/build/x64-debug/realtime-rtp.sdp
```

Raw RTP input uses explicit video and optional audio endpoint metadata. Hardware planning, low latency input, and graph diagnostics are enabled by default; use `--disable-hw`, `--no-low-latency`, or `--quiet-graph` only when overriding defaults.

```powershell
out/build/x64-debug/media_transcode_realtime_video_cli.exe --input-type rtp --input-layout separate --output-layout separate --video-rtp-url rtp://127.0.0.1:5004 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --video-rtp-fmtp "packetization-mode=1;sprop-parameter-sets=...;profile-level-id=..." --open-timeout-ms 5000 --read-timeout-ms 5000 --analyze-duration-us 500000 --probe-size 524288 --rtp-host 127.0.0.1 --rtp-port 5008 --sdp out/build/x64-debug/raw-rtp-output.sdp --packet-size 1200 --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --video-codec h264 --rc auto --max-duration 15
```

`udp://host:port` is accepted for UDP-carried RTP in RTP-port mode. MPEG-TS over UDP uses explicit MPEG-TS classification:

```powershell
out/build/x64-debug/media_transcode_realtime_video_cli.exe --input-type mpegts-udp --input-layout mpegts --output-layout mpegts --input udp://0.0.0.0:15000 --output udp://127.0.0.1:15002 --open-timeout-ms 5000 --read-timeout-ms 5000 --analyze-duration-us 500000 --probe-size 524288 --no-audio --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --video-codec h264 --rc auto --max-duration 15
```

## Production acceptance

Project acceptance is based on the real local and realtime CLIs, real media streams,
FFmpeg/VLC observation, runtime memory metrics, and continuous A/V drift telemetry.
The current absolute-path PowerShell commands and evidence are recorded under
`docs/completed/`.
