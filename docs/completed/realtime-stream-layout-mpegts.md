# Realtime Stream Layout and MPEG-TS UDP

## Completed

- Added explicit realtime input type, input stream layout, and output stream layout models.
- Mapped existing realtime URL input to `Url + SessionDescribed + SeparateStreams`.
- Mapped existing RTP port input to `RtpPort + SeparateStreams + SeparateStreams`.
- Added `MpegTsUdp + MuxedTransportStream + MuxedTransportStream` planning and graph construction.
- Added planner validation that `MpegTsUdp` input must use a `udp://` URL.
- Reused `RealtimeInputNode` for FFmpeg-managed input and `FileOutput + FileMux` for MPEG-TS output.
- Updated realtime CLIs to require explicit stream type and layout arguments.

## Verification

Reverified on 2026-07-08.

| Command | Result |
| --- | --- |
| `cmd /c "call D:\VisualStudio2026\Common7\Tools\VsDevCmd.bat -arch=x64 -host_arch=x64 && cmake --build out/build/x64-debug --target media_transcode_core media_transcode_graph_transcode_cli media_transcode_realtime_rtp_input_cli media_transcode_realtime_graph_tests"` | Success. Build completed with no work to do. |
| `out\build\x64-debug\media_transcode_realtime_graph_tests.exe` | Success. Unit tests passed. The MPEG-TS graph test starts local FFmpeg, remuxes the MP4 sample to MPEG-TS over UDP, and lets the planner probe a loopback `udp://` URL on an OS-assigned test port. |
| `ctest --test-dir out/build/x64-debug --output-on-failure` | Success. `media_transcode_realtime_graph_tests` passed. |
| `D:\mabs\local64\bin-video\ffmpeg.exe -hide_banner -loglevel error -re -stream_loop -1 -i D:\Code\MyCode\MediaTranscode\tests\samples\sample_h264_aac_2560x1440.mp4 -map 0:v:0 -map 0:a:0? -c copy -f mpegts udp://127.0.0.1:15100?pkt_size=1316` | Success. Used as the local MPEG-TS UDP source for CLI smoke. |
| `out\build\x64-debug\media_transcode_graph_transcode_cli.exe --mode realtime-rtp --input-type mpegts-udp --input-layout mpegts --output-layout mpegts --input udp://127.0.0.1:15100?fifo_size=1000000&overrun_nonfatal=1 --output udp://127.0.0.1:15102 --open-timeout-ms 5000 --read-timeout-ms 5000 --analyze-duration-us 500000 --probe-size 524288 --video --no-audio --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --video-codec h264 --rc auto --duration 3` | Success. CLI exited with code 0, planned and ran `MpegTsUdp + MuxedTransportStream + MuxedTransportStream`; runtime registered `RealtimeInput`, `FileOutput`, and `FileMux`, then stopped after duration. |
| `D:\mabs\local64\bin-video\ffprobe.exe udp://192.168.96.154:15666` | Failed due external stream payload state. FFmpeg reported invalid data. |
| `D:\mabs\local64\bin-video\ffprobe.exe udp://@192.168.96.154:15666` | Failed due external stream payload state. FFmpeg reported invalid data. |
| `D:\mabs\local64\bin-video\ffprobe.exe -f mpegts udp://@192.168.96.154:15666` | Failed due external stream payload state. Forced MPEG-TS probing still did not finish stream discovery after 30 seconds and was stopped. |

## Remaining Risks

- Live MPEG-TS runtime behavior still needs verification once a readable MPEG-TS UDP source is available.
- SDP input, external packet push input, RTP bundled input, and RTP bundled output remain intentionally unsupported.
