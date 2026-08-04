# Realtime Termination Correctness Completion

Completed on `codex/realtime-termination-correctness` on 2026-08-03.

## Result

- Only `AVERROR_EOF` is a clean input end. Active interrupt is `Cancelled`; every other negative FFmpeg read result is one `IoFailure` with the native code preserved.
- The first worker failure is permanent. The failure supervisor interrupts peers; only coordinated `Cancelled` is excluded from worker error totals.
- Every normally completed worker records `Finished`. The CLI rechecks completion at the zero-active-worker boundary before reporting an error.
- RTP clock degradation fails at the raw-input boundary with stream kind and the planner-provided 7/9-second observation facts.
- Audio terminal residue is settled only from exact SWR-exhaustion proof. Decoder discard padding and encoder priming packets retain exact lineage.
- Video decoder drain releases FFmpeg opaque leases before generation completion.
- An earlier full-PR review passed at `c1433dd4`. The 2026-08-04 direct-VLC follow-up review then found interrupt/EOF precedence and secondary-cancellation accounting gaps; both are corrected in the current review candidate.

## P0 Gates

The clean local EOF command was run again after the review fixes and final rebuild:

```powershell
& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe' --media-id p0-clean-eof-post-review-fix --input-type rtsp --input-layout session --input 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\test.mp4' --rtsp-transport tcp --no-low-latency --open-timeout-ms 5000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --output-layout mpegts --output-transport udp --output 'udp://127.0.0.1:65030' --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 --video-codec h264 --width 1280 --height 720 --fps 30 --bitrate 4000 --gop 30 --audio-codec aac --audio-bitrate 128 --sample-rate 48000 --channels 2 --progress-timeout-ms 10000 --first-output-timeout-ms 30000 --poll-interval-ms 250 --quiet-graph
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
& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe' --media-id formal-url-to-rtp --input-type rtsp --input-layout session --input 'http://127.0.0.1:64000/live.ts' --rtsp-transport tcp --no-low-latency --open-timeout-ms 5000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --output-layout separate --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 64020 --packet-size 1200 --sdp 'D:\Code\MyCode\MediaTranscode\out\acceptance\realtime-termination-correctness\approved-url-to-rtp-source-driven-1\output.sdp' --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 --video-codec h264 --width 1280 --height 720 --fps 30 --bitrate 4000 --gop 30 --audio-codec aac --audio-bitrate 128 --sample-rate 48000 --channels 2 --progress-timeout-ms 10000 --first-output-timeout-ms 30000 --poll-interval-ms 100

& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe' --media-id formal-url-to-tsudp --input-type rtsp --input-layout session --input 'http://127.0.0.1:64000/live.ts' --rtsp-transport tcp --no-low-latency --open-timeout-ms 5000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --output-layout mpegts --output-transport udp --output 'udp://127.0.0.1:1234' --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 --video-codec h264 --width 1280 --height 720 --fps 30 --bitrate 4000 --gop 30 --audio-codec aac --audio-bitrate 128 --sample-rate 48000 --channels 2 --progress-timeout-ms 10000 --first-output-timeout-ms 30000 --poll-interval-ms 100

& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe' --media-id formal-url-to-tsrtp --input-type rtsp --input-layout session --input 'http://127.0.0.1:64000/live.ts' --rtsp-transport tcp --no-low-latency --open-timeout-ms 5000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 1234 --packet-size 1328 --sdp 'D:\Code\MyCode\MediaTranscode\out\acceptance\realtime-termination-correctness\approved-url-to-tsrtp-source-driven-1\output.sdp' --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 --video-codec h264 --width 1280 --height 720 --fps 30 --bitrate 4000 --gop 30 --audio-codec aac --audio-bitrate 128 --sample-rate 48000 --channels 2 --progress-timeout-ms 10000 --first-output-timeout-ms 30000 --poll-interval-ms 100

& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe' --media-id formal-rtp-to-rtp --input-type rtp --input-layout separate --video-rtp-url 'rtp://127.0.0.1:52000' --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --video-rtp-fmtp 'packetization-mode=1;sprop-parameter-sets=Z01AMpWQAoALWwEQAAA+gAAOpghA,aOuPIA==;profile-level-id=4D4032' --audio-rtp-url 'rtp://127.0.0.1:52002' --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210' --open-timeout-ms 5000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --output-layout separate --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 64020 --packet-size 1200 --sdp 'D:\Code\MyCode\MediaTranscode\out\acceptance\realtime-termination-correctness\approved-rtp-to-rtp-source-driven-1\output.sdp' --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 --video-codec h264 --width 1280 --height 720 --fps 30 --bitrate 4000 --gop 30 --audio-codec aac --audio-bitrate 128 --sample-rate 48000 --channels 2 --progress-timeout-ms 10000 --first-output-timeout-ms 30000 --poll-interval-ms 100

& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe' --media-id formal-rtp-to-tsudp --input-type rtp --input-layout separate --video-rtp-url 'rtp://127.0.0.1:52000' --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --video-rtp-fmtp 'packetization-mode=1;sprop-parameter-sets=Z01AMpWQAoALWwEQAAA+gAAOpghA,aOuPIA==;profile-level-id=4D4032' --audio-rtp-url 'rtp://127.0.0.1:52002' --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210' --open-timeout-ms 5000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --output-layout mpegts --output-transport udp --output 'udp://127.0.0.1:1234' --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 --video-codec h264 --width 1280 --height 720 --fps 30 --bitrate 4000 --gop 30 --audio-codec aac --audio-bitrate 128 --sample-rate 48000 --channels 2 --progress-timeout-ms 10000 --first-output-timeout-ms 30000 --poll-interval-ms 100

& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe' --media-id formal-rtp-to-tsrtp --input-type rtp --input-layout separate --video-rtp-url 'rtp://127.0.0.1:52000' --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --video-rtp-fmtp 'packetization-mode=1;sprop-parameter-sets=Z01AMpWQAoALWwEQAAA+gAAOpghA,aOuPIA==;profile-level-id=4D4032' --audio-rtp-url 'rtp://127.0.0.1:52002' --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210' --open-timeout-ms 5000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 1234 --packet-size 1328 --sdp 'D:\Code\MyCode\MediaTranscode\out\acceptance\realtime-cross-layout\formal-rtp-to-tsrtp\output.sdp' --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 --video-codec h264 --width 1280 --height 720 --fps 30 --bitrate 4000 --gop 30 --audio-codec aac --audio-bitrate 128 --sample-rate 48000 --channels 2 --progress-timeout-ms 10000 --first-output-timeout-ms 30000 --poll-interval-ms 100 --max-duration 120

& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe' --media-id formal-tsudp-to-rtp --input-type mpegts-udp --input-layout mpegts --input 'udp://127.0.0.1:64200?fifo_size=65536&overrun_nonfatal=1' --open-timeout-ms 5000 --read-timeout-ms 5000 --analyze-duration-us 5000000 --probe-size 5000000 --mpegts-max-pcr-gap-ms 1000 --output-layout separate --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 64020 --packet-size 1200 --sdp 'D:\Code\MyCode\MediaTranscode\out\acceptance\realtime-termination-correctness\approved-tsudp-to-rtp-source-driven-1\output.sdp' --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 --video-codec h264 --width 1280 --height 720 --fps 30 --bitrate 4000 --gop 30 --audio-codec aac --audio-bitrate 128 --sample-rate 48000 --channels 2 --progress-timeout-ms 10000 --first-output-timeout-ms 30000 --poll-interval-ms 100

& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe' --media-id formal-tsudp-to-tsudp --input-type mpegts-udp --input-layout mpegts --input 'udp://127.0.0.1:64200?fifo_size=65536&overrun_nonfatal=1' --open-timeout-ms 5000 --read-timeout-ms 5000 --analyze-duration-us 5000000 --probe-size 5000000 --mpegts-max-pcr-gap-ms 1000 --output-layout mpegts --output-transport udp --output 'udp://127.0.0.1:1234' --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 --video-codec h264 --width 1280 --height 720 --fps 30 --bitrate 4000 --gop 30 --audio-codec aac --audio-bitrate 128 --sample-rate 48000 --channels 2 --progress-timeout-ms 10000 --first-output-timeout-ms 30000 --poll-interval-ms 100

& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe' --media-id formal-tsudp-to-tsrtp --input-type mpegts-udp --input-layout mpegts --input 'udp://127.0.0.1:64200?fifo_size=65536&overrun_nonfatal=1' --open-timeout-ms 5000 --read-timeout-ms 5000 --analyze-duration-us 5000000 --probe-size 5000000 --mpegts-max-pcr-gap-ms 1000 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 1234 --packet-size 1328 --sdp 'D:\Code\MyCode\MediaTranscode\out\acceptance\realtime-termination-correctness\approved-tsudp-to-tsrtp-source-driven-1\output.sdp' --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 --video-codec h264 --width 1280 --height 720 --fps 30 --bitrate 4000 --gop 30 --audio-codec aac --audio-bitrate 128 --sample-rate 48000 --channels 2 --progress-timeout-ms 10000 --first-output-timeout-ms 30000 --poll-interval-ms 100
```

Input sources were run directly with these absolute commands, one route at a time:

```powershell
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -loglevel warning -re -stream_loop -1 -i 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\test.mp4' -map 0:v:0 -map 0:a:0 -c copy -f mpegts -listen 1 'http://127.0.0.1:64000/live.ts'

& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -loglevel warning -stream_loop -1 -re -i 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\test.mp4' -map 0:v:0 -c:v copy -an -payload_type 96 -ssrc 52001 -f rtp 'rtp://127.0.0.1:52000?rtcpport=52001' -map 0:a:0 -c:a copy -vn -payload_type 97 -ssrc 52003 -f rtp 'rtp://127.0.0.1:52002?rtcpport=52003'

& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -loglevel warning -re -stream_loop -1 -i 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\test.mp4' -map 0:v:0 -map 0:a:0 -c copy -f mpegts 'udp://127.0.0.1:64200?pkt_size=1316'
```

One visible VLC was opened per route. The earlier FFmpeg observation/remux chain was invalidated because it introduced non-monotonic DTS, corruption, and playback stalls that were not produced by the production DAG. MPEG-TS outputs are observed directly by URL. Separate RTP is observed by opening the generated SDP directly; the SDP is signaling only and does not add a media relay.

```powershell
& 'D:\VideoLAN\VLC\vlc.exe' 'udp://@127.0.0.1:1234' --no-video-title-show
& 'D:\VideoLAN\VLC\vlc.exe' 'rtp://@127.0.0.1:1234' --no-video-title-show
& 'D:\VideoLAN\VLC\vlc.exe' 'D:\Code\MyCode\MediaTranscode\out\acceptance\realtime-termination-correctness\approved-url-to-rtp-source-driven-1\output.sdp' --no-video-title-show
& 'D:\VideoLAN\VLC\vlc.exe' 'D:\Code\MyCode\MediaTranscode\out\acceptance\realtime-termination-correctness\approved-rtp-to-rtp-source-driven-1\output.sdp' --no-video-title-show
& 'D:\VideoLAN\VLC\vlc.exe' 'D:\Code\MyCode\MediaTranscode\out\acceptance\realtime-termination-correctness\approved-tsudp-to-rtp-source-driven-1\output.sdp' --no-video-title-show
```

The matching per-route results, including CLI process CPU and working-set observations, are recorded in the direct-VLC revalidation table below. CPU evidence always refers to the realtime CLI process, never machine-wide CPU.

## 2026-08-04 Direct-VLC Revalidation

The reviewed branch was rebuilt clean-first and the full 3x3 matrix was repeated sequentially with the production CLI, one FFmpeg source, and one visible VLC. MPEG-TS outputs were opened directly by URL; separate RTP outputs were opened directly from the generated SDP. No observer, receiver, remux, or second VLC was present.

| Input | Output | Playback result | Source-stop result | CLI process evidence |
|---|---|---|---|---|
| Separate RTP | MPEG-TS/RTP | Picture and sound passed | This first rerun retained the initially approved `--max-duration 120`; it is not counted as a fresh source-stop gate | Zero playback errors/drops |
| Separate RTP | MPEG-TS/UDP | Picture, sound, and A/V sync passed | One RTP video clock-degraded primary with 7/9/9-second facts | Stable memory; zero playback errors/drops |
| URL/session | MPEG-TS/RTP | Picture, sound, and A/V sync passed | One demux source-loss primary, `native=-10054` | 4.81% average CPU; about 252 MB late working set |
| URL/session | MPEG-TS/UDP | Picture, sound, and A/V sync passed | One demux source-loss primary, `native=-10054` | 5.18% average CPU; about 253 MB late working set |
| MPEG-TS/UDP | MPEG-TS/RTP | Picture, sound, and A/V sync passed | One MPEG-TS source-loss primary, `native=-5` | 5.72% average CPU; 246 MB final working set |
| MPEG-TS/UDP | MPEG-TS/UDP | Picture, sound, and A/V sync passed | One MPEG-TS source-loss primary, `native=-5` | 5.48% average CPU; 246 MB final working set |
| MPEG-TS/UDP | Separate RTP | Picture, sound, and A/V sync passed | One MPEG-TS source-loss primary, `native=-5` | 5.10% average CPU; 245 MB final working set |
| URL/session | Separate RTP | Picture, sound, and A/V sync passed | Demux source loss preserved `native=-10054`; the terminal capture truncated the final counter line | 3.98% late average CPU; zero playback errors/drops |
| Separate RTP | Separate RTP | Picture, sound, and A/V sync passed | Exactly one RTP video clock-degraded primary with 7/9/9-second facts | 4.86% average CPU; 235 MB final working set; `workerErrors=1`, `errors=1` only after source stop |

The eight reruns after the first route omitted `--max-duration` and were source-driven. During active playback all nine had zero worker/runtime errors and zero drops, no sustained memory growth, no automatic exit, and no visible black frame, corruption, stall, or A/V desynchronization. The earlier exact P0 source-loss gates remain the authoritative proof for all three termination classes, including the separate-RTP route omitted from the latest first rerun.

## Remaining Risks

1. Connectionless UDP cannot prove that an absent sender ended normally; sender disappearance remains source loss.
2. The repository intentionally has no automated regression safety net; concurrency and termination gates remain real-media manual work.
3. Long-duration fault injection and repeated generation transitions still depend on manual live-stream acceptance.
4. A receiver joining between parameter-set/key-frame boundaries may log temporary missing-PPS warnings.
5. One stalled interval was observed in several RTP/MPEG-TS input routes without errors, drops, or visible drift; longer observation may refine that bound.
6. Windows dynamically excluded UDP ports caused local bind/loading failures; acceptance used the verified source ports 52000/52002, 64000, and 64200 plus output port 1234.
