# Realtime Cross-Layout and MPEG-TS/RTP

## Completed

- Input clock planning is independent from output layout and transport.
- URL/RTSP, separate RTP, and MPEG-TS/UDP A/V inputs share one canonical startup, drift-control, recovery, and scheduling path.
- Exactly one planner-selected output is assembled: separate RTP, MPEG-TS/UDP, or MPEG-TS/RTP.
- MPEG-TS/RTP uses RTP/AVP MP2T payload type 33, a 90 kHz clock, adjacent RTP/RTCP ports, atomic SDP publication, persistent outer RTP continuity, and explicit inner TS discontinuity at a real generation transition.
- Generation-start video encoding requests a key frame. Audio drift state and output-generation permission are committed atomically.

## Final Post-Fix Gates

The final MPEG-TS/UDP to MPEG-TS/RTP run used these direct PowerShell commands.
The UDP source was started before the CLI so that MPEG-TS probing began from an
active stream:

```powershell
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -loglevel warning -re -stream_loop -1 -i 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\test.mp4' -map 0:v:0 -map 0:a:0 -c copy -t 145 -f mpegts -mpegts_flags +resend_headers -muxdelay 0 'udp://127.0.0.1:52620?pkt_size=1316'

& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe' --media-id formal-tsudp-to-tsrtp-counter-final-source-first --input-type mpegts-udp --input-layout mpegts --input 'udp://127.0.0.1:52620?fifo_size=65536&overrun_nonfatal=1' --open-timeout-ms 5000 --read-timeout-ms 5000 --analyze-duration-us 5000000 --probe-size 5000000 --mpegts-max-pcr-gap-ms 1000 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 52630 --sdp 'D:\Code\MyCode\MediaTranscode\out\acceptance\realtime-cross-layout\formal-tsudp-to-tsrtp\output.sdp' --packet-size 1328 --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 --video-codec h264 --width 1280 --height 720 --fps 30 --bitrate 4000 --gop 30 --audio-codec aac --audio-bitrate 128 --sample-rate 48000 --channels 2 --max-duration 125 --progress-timeout-ms 10000 --first-output-timeout-ms 20000 --poll-interval-ms 100

& 'D:\VideoLAN\VLC\vlc.exe' 'rtp://@127.0.0.1:52630' --no-video-title-show
```

Result: the CLI logged 136.2 seconds and stopped successfully. It remained in
generation 1 with `workerErrors=0`, `errors=0`, and `droppedBuffers=0`. Average
process CPU was 5.48%; final working set was 251,850,752 bytes and remained
stable through the latter half. The last drift sample had raw phase 18,680 ns,
filtered phase 13,914 ns, and no recovery state. VLC observation found normal
CPU, no sustained memory growth, and no perceptible A/V drift.

The final MPEG-TS/UDP to separate RTP gate used the following commands. The CLI
published the current SDP before VLC opened it; on Windows, opening the old SDP
first prevents atomic replacement while VLC holds the file:

```powershell
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -loglevel warning -re -stream_loop -1 -i 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\test.mp4' -map 0:v:0 -map 0:a:0 -c copy -t 165 -f mpegts -mpegts_flags +resend_headers -muxdelay 0 'udp://127.0.0.1:52520?pkt_size=1316'

& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe' --media-id formal-tsudp-to-rtp-final-145 --input-type mpegts-udp --input-layout mpegts --input 'udp://127.0.0.1:52520?fifo_size=65536&overrun_nonfatal=1' --open-timeout-ms 5000 --read-timeout-ms 5000 --analyze-duration-us 5000000 --probe-size 5000000 --mpegts-max-pcr-gap-ms 1000 --output-layout separate --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 52530 --sdp 'D:\Code\MyCode\MediaTranscode\out\acceptance\realtime-cross-layout\formal-tsudp-to-rtp\output.sdp' --packet-size 1200 --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 --video-codec h264 --width 1280 --height 720 --fps 30 --bitrate 4000 --gop 30 --audio-codec aac --audio-bitrate 128 --sample-rate 48000 --channels 2 --max-duration 145 --progress-timeout-ms 10000 --first-output-timeout-ms 20000 --poll-interval-ms 100

& 'D:\VideoLAN\VLC\vlc.exe' 'D:\Code\MyCode\MediaTranscode\out\acceptance\realtime-cross-layout\formal-tsudp-to-rtp\output.sdp' --no-video-title-show
```

Result: the CLI logged 155.5 seconds and stopped successfully in generation 1
with zero worker errors, runtime errors, or drops. Average process CPU was
5.24%; the final working set was 245,547,008 bytes after remaining stable near
250 MB. The last raw/filtered A/V phase was 20,098/14,849 ns without recovery.
VLC observation stayed below 6% process CPU with no sustained memory growth and
no perceptible A/V drift.

The 1000 ms PCR-gap value is an explicit caller policy, not a runtime default. A 120 ms diagnostic value repeatedly interpreted normal 0.3-0.6 second FFmpeg `-re` source lag as an excessive PCR gap and caused frequent generation reacquisition. The 1000 ms A/B run stayed in one generation for the full gate.

## Nine-Path Evidence

All artifacts remain local under `out/acceptance/realtime-cross-layout/` and are not versioned.

| Input to output | Final log | Result |
|---|---|---|
| separate RTP to separate RTP | `formal-rtp-to-rtp/cli.all.log` | Success; 181.4 seconds; generation 1; 0 worker errors, errors, or drops; 4.01% average process CPU |
| URL/RTSP to separate RTP | `formal-url-to-rtp/cli-safe.all.log` | Success; 154.2 seconds; generation 1; 0 worker errors, errors, or drops; 2.68% average process CPU |
| URL/RTSP to MPEG-TS/UDP | `formal-url-to-tsudp/cli.all.log` | Success; 155.6 seconds; generation 1; 0 worker errors or runtime errors; 42 bounded drops; 3.11% average process CPU |
| URL/RTSP to MPEG-TS/RTP | `formal-url-to-tsrtp/cli-blocking-formal.all.log` | Success; 154.8 seconds; generation 1; 0 worker errors, errors, or drops; 4.66% average process CPU |
| separate RTP to MPEG-TS/UDP | `formal-rtp-to-tsudp/cli-visible.all.log` | Success; 155.5 seconds; generation 1; 0 worker errors, errors, or drops; 4.06% average process CPU |
| separate RTP to MPEG-TS/RTP | `formal-rtp-to-tsrtp/cli.all.log` | Success; 155.7 seconds; generation 1; 0 worker errors, errors, or drops; 4.94% average process CPU |
| MPEG-TS/UDP to separate RTP | `formal-tsudp-to-rtp/formal-final-145.all.log` | Success; 155.5 seconds; generation 1; 0 worker errors, errors, or drops; 5.24% average process CPU |
| MPEG-TS/UDP to MPEG-TS/UDP | `manual-tsudp-tsudp.stdout.log` | Success; 156.2 seconds; generation 1; 0 worker errors, errors, or drops; 4.26% average process CPU |
| MPEG-TS/UDP to MPEG-TS/RTP | `formal-tsudp-to-tsrtp/formal-final-counter-120-source-first.all.log` | Success; 136.2 seconds; generation 1; 0 worker errors, errors, or drops; 5.48% average process CPU |

Manual VLC observation supplemented the runtime evidence. Accepted paths had stable video, stable memory, and no perceptible A/V drift. Whenever audio and video were simultaneously present during fault investigation, they remained synchronized.

## Remaining Risks

1. URL/RTSP to MPEG-TS/UDP recorded 42 bounded drops despite completing without runtime or worker errors; a longer source-specific soak should determine whether queue planning needs adjustment.
2. MPEG-TS PCR-gap policy is source dependent. Values materially below observed source scheduling lag can intentionally cause repeated reacquisition.
3. Real generation transitions now preserve RTP continuity, force a key frame, and mark inner TS discontinuity, but long-duration fault-injection coverage remains manual.
4. The repository intentionally has no automated test or CI safety net; concurrency and protocol regressions depend on repeated real-media gates.
