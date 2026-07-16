# OpenAI Build Week Demo

This branch is isolated for the OpenAI Build Week presentation. It is not intended to be merged into `master`.

## Build on Windows

```powershell
cmake --build out/build/x64-debug --target media_transcode_build_week_cli media_transcode_build_week_cli_tests --clean-first
```

## 1. Inspect the planned DAG

```powershell
out\build\x64-debug\media_transcode_build_week_cli.exe inspect --input out\build\x64-debug\test.mp4
```

This probes the real input, invokes the existing MediaTranscode planner and builder, and prints:

- DAG node and edge counts
- selected decode/filter/encode chain
- planner score
- zero-copy status
- all-hardware status

It does not execute media processing.

## 2. Run the one-command file demo

```powershell
out\build\x64-debug\media_transcode_build_week_cli.exe demo --input out\build\x64-debug\test.mp4
```

The default output is:

```text
build-week-output.mp4
```

Optional resize example:

```powershell
out\build\x64-debug\media_transcode_build_week_cli.exe demo --input out\build\x64-debug\test.mp4 --output build-week-720p.mp4 --width 1280 --height 720 --bitrate 4000
```

Hardware planning is enabled by default. Use `--disable-hw` only to demonstrate the software path.

## 3. Run the realtime RTSP-to-MPEG-TS demo

Start VLC first with:

```powershell
D:\VideoLAN\VLC\vlc.exe udp://@:7354
```

Then run:

```powershell
out\build\x64-debug\media_transcode_build_week_cli.exe live --input rtsp://YOUR_CAMERA_OR_STREAM --duration 15
```

The default output is:

```text
udp://127.0.0.1:7354?pkt_size=1316
```

The CLI prints runtime metrics every 500 ms and fails when workers report errors or the threaded runtime stops unexpectedly.

## Design boundary

The Build Week CLI does not invoke an external FFmpeg executable and does not hard-code CUDA, NVENC, CUVID, QSV, VAAPI, or software codec implementations. It submits high-level media goals to the existing planner, materializes the selected plan through existing builders, and executes it through `MediaGraphRuntime`.

## Tests

```powershell
out\build\x64-debug\media_transcode_build_week_cli_tests.exe
out\build\x64-debug\media_transcode_event_runtime_tests.exe
out\build\x64-debug\media_transcode_realtime_graph_tests.exe
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure
```