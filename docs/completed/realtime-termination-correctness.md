# Realtime Termination Correctness Completion

Completed on `codex/realtime-termination-correctness` on 2026-08-03.

## Result

- Only `AVERROR_EOF` is a clean input end. Active interrupt is `Cancelled`; every other negative FFmpeg read result is one `IoFailure` with the native code preserved.
- The first worker failure is permanent. The failure supervisor interrupts peers; only coordinated `Cancelled` is excluded from worker error totals.
- Every normally completed worker records `Finished`. The CLI rechecks completion at the zero-active-worker boundary before reporting an error.
- RTP clock degradation fails at the raw-input boundary with stream kind and the planner-provided 7/9-second observation facts.
- Audio terminal residue is settled only from exact SWR-exhaustion proof. Decoder discard padding and encoder priming packets retain exact lineage.
- Video decoder drain releases FFmpeg opaque leases before generation completion.

## P0 Gates

The clean local EOF command was run again after the review fixes and final rebuild:

```powershell
& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe' --media-id p0-clean-eof-reviewed-head --input-type rtsp --input-layout session --input 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\test.mp4' --rtsp-transport tcp --no-low-latency --open-timeout-ms 5000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --output-layout mpegts --output-transport udp --output 'udp://127.0.0.1:65030' --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 --video-codec h264 --width 1280 --height 720 --fps 30 --bitrate 4000 --gop 30 --audio-codec aac --audio-bitrate 128 --sample-rate 48000 --channels 2 --progress-timeout-ms 10000 --first-output-timeout-ms 30000 --poll-interval-ms 250 --quiet-graph
```

It exited 0 with `workerErrors=0`, `errors=0`, `droppedBuffers=0`, and `encodedPacketsPushed=encodedPacketsPopped=2938`. No `audio correction exhaustion` or `Codec flush left live lineage leases` message occurred. Earlier clean-EOF stress runs also exited 0 with the same counters; the result above is the retained post-review gate.

The HTTP source-loss gate used these direct commands and then stopped the FFmpeg source process:

```powershell
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -loglevel warning -re -stream_loop -1 -i 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\test.mp4' -map 0:v:0 -map 0:a:0 -c copy -f mpegts -listen 1 'http://127.0.0.1:64000/live.ts'

& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe' --media-id p0-http-exact-pid-reviewed --input-type rtsp --input-layout session --input 'http://127.0.0.1:64000/live.ts' --rtsp-transport tcp --no-low-latency --open-timeout-ms 5000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --output-layout mpegts --output-transport udp --output 'udp://127.0.0.1:65034' --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 --video-codec h264 --width 1280 --height 720 --fps 30 --bitrate 4000 --gop 30 --audio-codec aac --audio-bitrate 128 --sample-rate 48000 --channels 2 --progress-timeout-ms 10000 --first-output-timeout-ms 30000 --poll-interval-ms 1000 --quiet-graph

& 'C:\Windows\System32\taskkill.exe' /PID 58136 /T /F
```

The MPEG-TS/UDP loss gate used the MPEG-TS/UDP source command in the next section, the following CLI, and the exact sender PID stop command:

```powershell
& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe' --media-id p0-tsudp-exact-pid-reviewed --input-type mpegts-udp --input-layout mpegts --input 'udp://127.0.0.1:64200?fifo_size=65536&overrun_nonfatal=1' --open-timeout-ms 5000 --read-timeout-ms 5000 --analyze-duration-us 5000000 --probe-size 5000000 --mpegts-max-pcr-gap-ms 1000 --output-layout mpegts --output-transport udp --output 'udp://127.0.0.1:65035' --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 --video-codec h264 --width 1280 --height 720 --fps 30 --bitrate 4000 --gop 30 --audio-codec aac --audio-bitrate 128 --sample-rate 48000 --channels 2 --progress-timeout-ms 10000 --first-output-timeout-ms 30000 --poll-interval-ms 1000 --quiet-graph

& 'C:\Windows\System32\taskkill.exe' /PID 42540 /T /F
```

The separate-RTP loss gate used the separate-RTP source command in the next section, the following CLI, and the exact sender PID stop command:

```powershell
& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe' --media-id p0-rtp-secondary-policy-reviewed --input-type rtp --input-layout separate --video-rtp-url 'rtp://127.0.0.1:64300' --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --video-rtp-fmtp 'packetization-mode=1;sprop-parameter-sets=Z01AMpWQAoALWwEQAAA+gAAOpghA,aOuPIA==;profile-level-id=4D4032' --audio-rtp-url 'rtp://127.0.0.1:64302' --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210' --open-timeout-ms 5000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --output-layout mpegts --output-transport udp --output 'udp://127.0.0.1:65036' --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 --video-codec h264 --width 1280 --height 720 --fps 30 --bitrate 4000 --gop 30 --audio-codec aac --audio-bitrate 128 --sample-rate 48000 --channels 2 --progress-timeout-ms 10000 --first-output-timeout-ms 30000 --poll-interval-ms 1000 --quiet-graph

& 'C:\Windows\System32\taskkill.exe' /PID 54432 /T /F
```

Each gate exited 1 with exactly `workerErrors=1` and `errors=1`. HTTP preserved `native=-10054`; MPEG-TS/UDP preserved `native=-5`; separate RTP reported `RTP video source clock evidence degraded` with planner-owned 7/9/9-second facts. The video clock is the degraded-loss authority; the planner assigns the audio clock the later expired-loss boundary, allowing the supervisor interrupt to produce `Cancelled` instead of a competing native failure. No gate, startup-clock, channel-abort, SWR, lineage, or peer raw-input failure became a secondary worker error. Earlier retained logs are under `out/acceptance/realtime-termination-correctness-final-head/`; the exact-PID command results above were rerun directly after the final review fix.

## Nine-Route Regression

The nine routes were invoked as the following single absolute-path commands, never through a test script. These are intentionally repeated in full so that no prefix/suffix synthesis or variable substitution is required.

```powershell
& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe' --media-id formal-url-to-rtp --input-type rtsp --input-layout session --input 'http://127.0.0.1:64000/live.ts' --rtsp-transport tcp --no-low-latency --open-timeout-ms 5000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --output-layout separate --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 64020 --packet-size 1200 --sdp 'D:\Code\MyCode\MediaTranscode\out\acceptance\realtime-termination-correctness\formal-url-to-rtp\output.sdp' --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 --video-codec h264 --width 1280 --height 720 --fps 30 --bitrate 4000 --gop 30 --audio-codec aac --audio-bitrate 128 --sample-rate 48000 --channels 2 --progress-timeout-ms 10000 --first-output-timeout-ms 30000 --poll-interval-ms 100 --max-duration 145

& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe' --media-id formal-url-to-tsudp --input-type rtsp --input-layout session --input 'http://127.0.0.1:64000/live.ts' --rtsp-transport tcp --no-low-latency --open-timeout-ms 5000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --output-layout mpegts --output-transport udp --output 'udp://127.0.0.1:64120' --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 --video-codec h264 --width 1280 --height 720 --fps 30 --bitrate 4000 --gop 30 --audio-codec aac --audio-bitrate 128 --sample-rate 48000 --channels 2 --progress-timeout-ms 10000 --first-output-timeout-ms 30000 --poll-interval-ms 100 --max-duration 145

& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe' --media-id formal-url-to-tsrtp --input-type rtsp --input-layout session --input 'http://127.0.0.1:64000/live.ts' --rtsp-transport tcp --no-low-latency --open-timeout-ms 5000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 64140 --packet-size 1328 --sdp 'D:\Code\MyCode\MediaTranscode\out\acceptance\realtime-termination-correctness\formal-url-to-tsrtp\output.sdp' --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 --video-codec h264 --width 1280 --height 720 --fps 30 --bitrate 4000 --gop 30 --audio-codec aac --audio-bitrate 128 --sample-rate 48000 --channels 2 --progress-timeout-ms 10000 --first-output-timeout-ms 30000 --poll-interval-ms 100 --max-duration 145

& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe' --media-id formal-rtp-to-rtp --input-type rtp --input-layout separate --video-rtp-url 'rtp://127.0.0.1:64300' --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --video-rtp-fmtp 'packetization-mode=1;sprop-parameter-sets=Z01AMpWQAoALWwEQAAA+gAAOpghA,aOuPIA==;profile-level-id=4D4032' --audio-rtp-url 'rtp://127.0.0.1:64302' --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210' --open-timeout-ms 5000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --output-layout separate --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 64020 --packet-size 1200 --sdp 'D:\Code\MyCode\MediaTranscode\out\acceptance\realtime-termination-correctness\formal-rtp-to-rtp\output.sdp' --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 --video-codec h264 --width 1280 --height 720 --fps 30 --bitrate 4000 --gop 30 --audio-codec aac --audio-bitrate 128 --sample-rate 48000 --channels 2 --progress-timeout-ms 10000 --first-output-timeout-ms 30000 --poll-interval-ms 100 --max-duration 170

& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe' --media-id formal-rtp-to-tsudp --input-type rtp --input-layout separate --video-rtp-url 'rtp://127.0.0.1:64300' --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --video-rtp-fmtp 'packetization-mode=1;sprop-parameter-sets=Z01AMpWQAoALWwEQAAA+gAAOpghA,aOuPIA==;profile-level-id=4D4032' --audio-rtp-url 'rtp://127.0.0.1:64302' --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210' --open-timeout-ms 5000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --output-layout mpegts --output-transport udp --output 'udp://127.0.0.1:64120' --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 --video-codec h264 --width 1280 --height 720 --fps 30 --bitrate 4000 --gop 30 --audio-codec aac --audio-bitrate 128 --sample-rate 48000 --channels 2 --progress-timeout-ms 10000 --first-output-timeout-ms 30000 --poll-interval-ms 100 --max-duration 145

& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe' --media-id formal-rtp-to-tsrtp --input-type rtp --input-layout separate --video-rtp-url 'rtp://127.0.0.1:64300' --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --video-rtp-fmtp 'packetization-mode=1;sprop-parameter-sets=Z01AMpWQAoALWwEQAAA+gAAOpghA,aOuPIA==;profile-level-id=4D4032' --audio-rtp-url 'rtp://127.0.0.1:64302' --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210' --open-timeout-ms 5000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 64140 --packet-size 1328 --sdp 'D:\Code\MyCode\MediaTranscode\out\acceptance\realtime-termination-correctness\formal-rtp-to-tsrtp\output.sdp' --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 --video-codec h264 --width 1280 --height 720 --fps 30 --bitrate 4000 --gop 30 --audio-codec aac --audio-bitrate 128 --sample-rate 48000 --channels 2 --progress-timeout-ms 10000 --first-output-timeout-ms 30000 --poll-interval-ms 100 --max-duration 145

& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe' --media-id formal-tsudp-to-rtp --input-type mpegts-udp --input-layout mpegts --input 'udp://127.0.0.1:64200?fifo_size=65536&overrun_nonfatal=1' --open-timeout-ms 5000 --read-timeout-ms 5000 --analyze-duration-us 5000000 --probe-size 5000000 --mpegts-max-pcr-gap-ms 1000 --output-layout separate --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 64020 --packet-size 1200 --sdp 'D:\Code\MyCode\MediaTranscode\out\acceptance\realtime-termination-correctness\formal-tsudp-to-rtp\output.sdp' --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 --video-codec h264 --width 1280 --height 720 --fps 30 --bitrate 4000 --gop 30 --audio-codec aac --audio-bitrate 128 --sample-rate 48000 --channels 2 --progress-timeout-ms 10000 --first-output-timeout-ms 30000 --poll-interval-ms 100 --max-duration 145

& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe' --media-id formal-tsudp-to-tsudp --input-type mpegts-udp --input-layout mpegts --input 'udp://127.0.0.1:64200?fifo_size=65536&overrun_nonfatal=1' --open-timeout-ms 5000 --read-timeout-ms 5000 --analyze-duration-us 5000000 --probe-size 5000000 --mpegts-max-pcr-gap-ms 1000 --output-layout mpegts --output-transport udp --output 'udp://127.0.0.1:64120' --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 --video-codec h264 --width 1280 --height 720 --fps 30 --bitrate 4000 --gop 30 --audio-codec aac --audio-bitrate 128 --sample-rate 48000 --channels 2 --progress-timeout-ms 10000 --first-output-timeout-ms 30000 --poll-interval-ms 100 --max-duration 145

& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe' --media-id formal-tsudp-to-tsrtp --input-type mpegts-udp --input-layout mpegts --input 'udp://127.0.0.1:64200?fifo_size=65536&overrun_nonfatal=1' --open-timeout-ms 5000 --read-timeout-ms 5000 --analyze-duration-us 5000000 --probe-size 5000000 --mpegts-max-pcr-gap-ms 1000 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 64140 --packet-size 1328 --sdp 'D:\Code\MyCode\MediaTranscode\out\acceptance\realtime-termination-correctness\formal-tsudp-to-tsrtp\output.sdp' --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 --video-codec h264 --width 1280 --height 720 --fps 30 --bitrate 4000 --gop 30 --audio-codec aac --audio-bitrate 128 --sample-rate 48000 --channels 2 --progress-timeout-ms 10000 --first-output-timeout-ms 30000 --poll-interval-ms 100 --max-duration 125
```

Input sources were run directly with these absolute commands, one route at a time:

```powershell
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -loglevel warning -re -stream_loop -1 -i 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\test.mp4' -map 0:v:0 -map 0:a:0 -c copy -f mpegts -listen 1 'http://127.0.0.1:64000/live.ts'

& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -loglevel warning -stream_loop -1 -re -i 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\test.mp4' -map 0:v:0 -c:v copy -an -payload_type 96 -ssrc 64301 -f rtp 'rtp://127.0.0.1:64300?rtcpport=64301' -map 0:a:0 -c:a copy -vn -payload_type 97 -ssrc 64303 -f rtp 'rtp://127.0.0.1:64302?rtcpport=64303'

& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -loglevel warning -re -stream_loop -1 -i 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\test.mp4' -map 0:v:0 -map 0:a:0 -c copy -f mpegts 'udp://127.0.0.1:64200?pkt_size=1316'
```

One visible VLC was opened per route. MPEG-TS/UDP output used the direct command below. Local VLC remained loading when opening local SDP through its Lua playlist path, so separate RTP and MPEG-TS/RTP used the corresponding absolute FFmpeg receiver to forward one observation stream to port 64400, followed by the same visible VLC command. Separate RTP audio was converted from LATM to AAC only at this observation boundary, which restored audible playback without changing the production DAG.

```powershell
& 'D:\VideoLAN\VLC\vlc.exe' 'udp://@127.0.0.1:64120' --no-video-title-show

& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -loglevel warning -protocol_whitelist file,udp,rtp -i 'D:\Code\MyCode\MediaTranscode\out\acceptance\realtime-termination-correctness\formal-url-to-rtp\output.sdp' -map 0:v:0 -map 0:a:0 -c:v copy -c:a aac -f mpegts 'udp://127.0.0.1:64400?pkt_size=1316'
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -loglevel warning -protocol_whitelist file,udp,rtp -i 'D:\Code\MyCode\MediaTranscode\out\acceptance\realtime-termination-correctness\formal-rtp-to-rtp\output.sdp' -map 0:v:0 -map 0:a:0 -c:v copy -c:a aac -f mpegts 'udp://127.0.0.1:64400?pkt_size=1316'
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -loglevel warning -protocol_whitelist file,udp,rtp -i 'D:\Code\MyCode\MediaTranscode\out\acceptance\realtime-termination-correctness\formal-tsudp-to-rtp\output.sdp' -map 0:v:0 -map 0:a:0 -c:v copy -c:a aac -f mpegts 'udp://127.0.0.1:64400?pkt_size=1316'

& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -loglevel warning -protocol_whitelist file,udp,rtp -i 'D:\Code\MyCode\MediaTranscode\out\acceptance\realtime-termination-correctness\formal-url-to-tsrtp\output.sdp' -map 0:v:0 -map 0:a:0 -c copy -f mpegts 'udp://127.0.0.1:64400?pkt_size=1316'
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -loglevel warning -protocol_whitelist file,udp,rtp -i 'D:\Code\MyCode\MediaTranscode\out\acceptance\realtime-termination-correctness\formal-rtp-to-tsrtp\output.sdp' -map 0:v:0 -map 0:a:0 -c copy -f mpegts 'udp://127.0.0.1:64400?pkt_size=1316'
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -loglevel warning -protocol_whitelist file,udp,rtp -i 'D:\Code\MyCode\MediaTranscode\out\acceptance\realtime-termination-correctness\formal-tsudp-to-tsrtp\output.sdp' -map 0:v:0 -map 0:a:0 -c copy -f mpegts 'udp://127.0.0.1:64400?pkt_size=1316'

& 'D:\VideoLAN\VLC\vlc.exe' 'udp://@127.0.0.1:64400' --no-video-title-show
```

| Input | Separate RTP | MPEG-TS/UDP | MPEG-TS/RTP |
|---|---:|---:|---:|
| URL/session | 145 s, 3.664% | 145 s, 3.662% | 145 s, 3.674% |
| Separate RTP | 170 s, 3.498% | 145 s, 4.216% | 145 s, 3.944% |
| MPEG-TS/UDP | 145 s, 4.367% | 145 s, 4.365% | 125 s, 4.345% |

Every percentage is the realtime CLI process `averageProcessCpuPercent`, not machine CPU. During active playback every route recorded `workerErrors=0`, `errors=0`, and `droppedBuffers=0`; late working sets were about 244-258 MB without sustained growth. Visible VLC had picture and sound with no perceptible continuous A/V drift. Network-source termination was accepted only as source-loss failure; local finite input EOF was accepted only as success.

The nine-route matrix completed before the final review-only changes to planner ownership, destructor order, and post-primary coordination. After those fixes and the final rebuild, one direct visible VLC URL/session-to-MPEG-TS/UDP gate was repeated for 20 seconds. It exited 0 with `workerErrors=0`, `errors=0`, `droppedBuffers=0`, realtime CLI `averageProcessCpuPercent=5.243545`, and a 246,734,848-byte late working set. The VLC command was `& 'D:\VideoLAN\VLC\vlc.exe' 'udp://@127.0.0.1:64120' --no-video-title-show`.

## Remaining Risks

1. Connectionless UDP cannot prove that an absent sender ended normally; sender disappearance remains source loss.
2. The repository intentionally has no automated regression safety net; concurrency and termination gates remain real-media manual work.
3. Long-duration fault injection and repeated generation transitions still depend on manual live-stream acceptance.
4. A receiver joining between parameter-set/key-frame boundaries may log temporary missing-PPS warnings.
5. One stalled interval was observed in several RTP/MPEG-TS input routes without errors, drops, or visible drift; longer observation may refine that bound.
6. Windows dynamically excluded UDP ports caused local bind/loading failures; acceptance used verified allowed ports 64000-64400.
