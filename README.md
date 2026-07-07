# MediaTranscode

MediaTranscode is a C++20 and FFmpeg-backed media transcode framework centered on the graph DAG architecture.

The active code path is the graph runtime:

```text
src/internal/graph/        # graph model, planner, builder, runtime, and runtime nodes
tools/graph_transcode_cli/ # graph CLI entry point
include/media_transcode/
    Result.h               # shared Result<T> and ErrorInfo type
```

Only the graph architecture and `Result.h` are documented as active project surfaces.

## Build

Use the existing CMake flow:

```bash
cmake -S . -B out/build/x64-debug -DMEDIA_TRANSCODE_BUILD_TESTS=ON
cmake --build out/build/x64-debug --target media_transcode_core
cmake --build out/build/x64-debug --target media_transcode_graph_transcode_cli
cmake --build out/build/x64-debug --target media_transcode_realtime_rtp_input_cli
cmake --build out/build/x64-debug --target media_transcode_realtime_graph_tests
```

Useful CMake options:

```text
MEDIA_TRANSCODE_BUILD_GRAPH_TOOLS=ON
MEDIA_TRANSCODE_BUILD_TESTS=ON
MEDIA_TRANSCODE_TEST_SAMPLES_DIR=<path>
```

## Realtime RTP DAG Path

The current realtime path is video-only:

```text
RTSP, realtime URL, or raw H264 UDP/RTP input
    -> graph planner
    -> realtime DAG builder
    -> graph runtime nodes
    -> RTP output + SDP
```

Run the graph CLI in realtime RTP mode:

```powershell
out/build/x64-debug/media_transcode_graph_transcode_cli.exe --mode realtime-rtp --input-kind url --input rtsp://... --rtsp-transport tcp --open-timeout-ms 5000 --read-timeout-ms 5000 --analyze-duration-us 500000 --probe-size 524288 --low-latency --rtp-host 127.0.0.1 --rtp-port 5004 --sdp out/build/x64-debug/realtime-rtp.sdp --packet-size 1200 --video --no-audio --enable-hw --graph-diagnostics --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --video-codec h264 --rc auto --duration 15
```

Receiver validation can use the generated SDP while the CLI is running:

```powershell
ffplay -protocol_whitelist file,udp,rtp -i out/build/x64-debug/realtime-rtp.sdp
ffprobe -protocol_whitelist file,udp,rtp -i out/build/x64-debug/realtime-rtp.sdp
```

Raw H264 RTP input uses the dedicated validation tool:

```powershell
out/build/x64-debug/media_transcode_realtime_rtp_input_cli.exe --input rtp://127.0.0.1:5004 --rtp-codec h264 --rtp-payload-type 96 --rtp-clock-rate 90000 --open-timeout-ms 5000 --read-timeout-ms 5000 --analyze-duration-us 500000 --probe-size 524288 --low-latency --rtp-host 127.0.0.1 --rtp-port 5006 --sdp out/build/x64-debug/raw-rtp-output.sdp --packet-size 1200 --enable-hw --graph-diagnostics --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --video-codec h264 --rc auto --max-duration 15
```

`udp://host:port` is accepted for UDP-carried RTP. MPEG-TS over UDP and audio RTP are outside this path.

## Tests

Build and run the active graph test target:

```powershell
cmake --build out/build/x64-debug --target media_transcode_realtime_graph_tests
out/build/x64-debug/media_transcode_realtime_graph_tests.exe
```

`tests/samples/` is reserved for small media fixtures used by graph validation.
