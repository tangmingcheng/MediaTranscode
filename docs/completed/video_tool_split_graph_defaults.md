# Video Tool Split and Graph Default Cleanup

## Completed Changes

- Split the old mixed graph CLI into `media_transcode_local_video_cli` and `media_transcode_realtime_video_cli`.
- Removed the old `media_transcode_graph_transcode_cli` and `media_transcode_realtime_rtp_input_cli` targets and entry files.
- Moved common video transcode CLI parsing into `tools/common/VideoCliTranscodeOptions.h`.
- Realtime video input now requires explicit input type, input layout, output layout, realtime open/probe options, queue capacities, and output settings.
- Optional transcode codec arguments now mean "inherit from source"; planner no longer requires an explicit realtime video codec.
- Queue capacities now default to invalid `0` in graph parameters and must be supplied by upstream request construction.
- Realtime video planner reuses the same resize rule as local planning: filter score is included only when resize is requested.
- Realtime plan summaries report `filter=not_required` when the selected plan does not require a filter stage.
- Review follow-up removed planner/audio planner default construction, made missing enabled audio a planning error, and removed encoder buffer-size fallback synthesis for bitrate-based modes.
- Second review follow-up removed the internal video-disable branch path and removed audio sample-rate fallback to the first encoder-supported rate.

## Validation

- Added regression coverage in `tests/unit/test_realtime_rtp_graph.cpp` for tool target split, legacy switch removal, source codec inheritance, unknown source codec rejection, and realtime filter scoring.
- Added regression coverage for planner option default construction removal, missing realtime planner option rejection, missing audio source rejection, encoder buffer default removal, video-disable path removal, and audio sample-rate fallback removal.

### Commands and Results

```powershell
cmd.exe /c ""D:\VisualStudio2026\Common7\Tools\VsDevCmd.bat" -arch=x64 && cmake --build out/build/x64-debug --target media_transcode_core media_transcode_local_video_cli media_transcode_realtime_video_cli media_transcode_realtime_graph_tests && out\build\x64-debug\media_transcode_realtime_graph_tests.exe"
```

Result: exit code `0`; debug build completed and `realtime graph tests passed`.

```powershell
cmd.exe /c ""D:\VisualStudio2026\Common7\Tools\VsDevCmd.bat" -arch=x64 && cmake --build out/build/x64-release --target media_transcode_core media_transcode_local_video_cli media_transcode_realtime_video_cli media_transcode_realtime_graph_tests"
```

Result: exit code `0`; release targets built successfully.

```powershell
out\build\x64-debug\media_transcode_local_video_cli.exe --input out\build\x64-debug\test.mp4 --output out\build\x64-debug\codex_local_video_tool_smoke.mp4 --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --disable-hw --no-audio --quiet-graph
```

Result: exit code `0`; `completed=true`, `total_pushed=1109`, `total_popped=1109`.

```powershell
$ffmpeg = 'D:\mabs\local64\bin-video\ffmpeg.exe'
$senderArgs = @('-hide_banner','-loglevel','error','-re','-stream_loop','-1','-i','out\build\x64-debug\test.mp4','-map','0:v:0','-an','-c:v','copy','-f','rtp','-payload_type','96','rtp://127.0.0.1:5004?pkt_size=1200')
$sender = Start-Process -FilePath $ffmpeg -ArgumentList $senderArgs -WindowStyle Hidden -PassThru
try {
    Start-Sleep -Seconds 1
    out\build\x64-debug\media_transcode_realtime_video_cli.exe --input-type rtp --input-layout separate --output-layout separate --video-rtp-url rtp://127.0.0.1:5004 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --video-rtp-fmtp "packetization-mode=1;sprop-parameter-sets=Z01AMpWQAoALWwEQAAA+gAAOpghhA,aOuPIA==;profile-level-id=4D4032" --open-timeout-ms 8000 --read-timeout-ms 8000 --analyze-duration-us 500000 --probe-size 524288 --rtp-host 127.0.0.1 --rtp-port 5010 --sdp out\build\x64-debug\codex_realtime_video_tool_smoke.sdp --packet-size 1200 --disable-hw --no-audio --quiet-graph --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --video-codec h264 --rc auto --max-duration 5 --progress-timeout-ms 12000 --poll-interval-ms 250
} finally {
    if ($sender -and -not $sender.HasExited) { Stop-Process -Id $sender.Id -Force }
}
```

Result: exit code `0`; selected `filter=not_required`, final report had `encodedPacketsPushed=36`, `encodedPacketsPopped=36`, `workerErrors=0`.

```powershell
out\build\x64-debug\media_transcode_realtime_video_cli.exe --input-type rtp --input-layout separate --output-layout separate --video-rtp-url rtp://192.168.96.154:15666 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --video-rtp-fmtp "packetization-mode=1;sprop-parameter-sets=Z01AMpWQAoALWwEQAAA+gAAOpghhA,aOuPIA==;profile-level-id=4D4032" --open-timeout-ms 8000 --read-timeout-ms 8000 --analyze-duration-us 500000 --probe-size 524288 --rtp-host 127.0.0.1 --rtp-port 5014 --sdp out\build\x64-debug\codex_external_rtp_smoke.sdp --packet-size 1200 --disable-hw --no-audio --quiet-graph --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --video-codec h264 --rc auto --max-duration 5 --progress-timeout-ms 12000 --poll-interval-ms 250
```

Result: exit code `1`; FFmpeg reported H264 bitstream decode errors such as `illegal memory management control operation`, runtime reported `workerErrors=1` and `encodedPacketsPushed=0`. This was treated as external stream or explicit RTP metadata mismatch and did not drive a code change.

```powershell
rg --pcre2 "(?<![-\w])--video(?![-\w])|(?<![-\w])--audio(?![-\w])|(?<![-\w])--mode(?![-\w])|(?<![-\w])--enable-hw(?![-\w])" tools src/internal/graph README.md CMakeLists.txt
rg "media_transcode_graph_transcode_cli|media_transcode_realtime_rtp_input_cli|tools/graph_transcode_cli|tools/realtime_rtp_input_cli" tools src/internal/graph README.md CMakeLists.txt tests/unit/test_realtime_rtp_graph.cpp
rg "includeVideo|requires video branch|supported_samplerates\[0\]" src/internal/graph tools
rg "defaultBufferSizeFromRate|rc_min_rate = encoderContext->bit_rate|rc_max_rate = encoderContext->bit_rate|plan.reason = \"no_audio\"" src/internal/graph tools
git diff --check
```

Result: legacy switch/target/default scans produced no active-code matches; `git diff --check` exited `0`.
