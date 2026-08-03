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

The clean local EOF command was run twice after the final lifecycle change:

```powershell
& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe' --media-id p0-clean-eof-final --input-type rtsp --input-layout session --input 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\test.mp4' --rtsp-transport tcp --no-low-latency --open-timeout-ms 5000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --output-layout mpegts --output-transport udp --output 'udp://127.0.0.1:65030' --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 --video-codec h264 --width 1280 --height 720 --fps 30 --bitrate 4000 --gop 30 --audio-codec aac --audio-bitrate 128 --sample-rate 48000 --channels 2 --progress-timeout-ms 10000 --first-output-timeout-ms 30000 --poll-interval-ms 250 --quiet-graph
```

Both runs exited 0 with `workerErrors=0`, `errors=0`, `droppedBuffers=0`, and `encodedPacketsPushed=encodedPacketsPopped=2938`. No `audio correction exhaustion` or `Codec flush left live lineage leases` message occurred.

The HTTP source-loss gate used these direct commands and then stopped the FFmpeg source process:

```powershell
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -loglevel warning -re -stream_loop -1 -i 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\test.mp4' -map 0:v:0 -map 0:a:0 -c copy -f mpegts -listen 1 'http://127.0.0.1:64000/live.ts'

& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe' --media-id p0-http-source-loss-final --input-type rtsp --input-layout session --input 'http://127.0.0.1:64000/live.ts' --rtsp-transport tcp --no-low-latency --open-timeout-ms 5000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --output-layout mpegts --output-transport udp --output 'udp://127.0.0.1:65034' --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 --video-codec h264 --width 1280 --height 720 --fps 30 --bitrate 4000 --gop 30 --audio-codec aac --audio-bitrate 128 --sample-rate 48000 --channels 2 --progress-timeout-ms 10000 --first-output-timeout-ms 30000 --poll-interval-ms 250 --quiet-graph
```

It exited 1 with one worker/runtime error and primary `IoFailure: DemuxNode av_read_frame lost its input source (native=-10054)`. MPEG-TS/UDP sender loss likewise produced one `IoFailure` preserving `native=-5`. Separate RTP sender loss produced one raw-input clock `IoFailure` with stream and planned timeout facts. None produced gate, startup-clock, channel-abort, SWR, or lineage secondary worker errors.

## Nine-Route Regression

The nine routes were invoked as single absolute-path commands, never through a test script. Each CLI invocation used this exact common tail after the route-specific arguments below:

```text
--metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 --video-codec h264 --width 1280 --height 720 --fps 30 --bitrate 4000 --gop 30 --audio-codec aac --audio-bitrate 128 --sample-rate 48000 --channels 2 --progress-timeout-ms 10000 --first-output-timeout-ms 30000 --poll-interval-ms 100
```

The three exact input prefixes were:

```text
URL/session:
D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe --input-type rtsp --input-layout session --input http://127.0.0.1:64000/live.ts --rtsp-transport tcp --no-low-latency --open-timeout-ms 5000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000

Separate RTP:
D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe --input-type rtp --input-layout separate --video-rtp-url rtp://127.0.0.1:64300 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --video-rtp-fmtp packetization-mode=1;sprop-parameter-sets=Z01AMpWQAoALWwEQAAA+gAAOpghA,aOuPIA==;profile-level-id=4D4032 --audio-rtp-url rtp://127.0.0.1:64302 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210 --open-timeout-ms 5000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000

MPEG-TS/UDP:
D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe --input-type mpegts-udp --input-layout mpegts --input udp://127.0.0.1:64200?fifo_size=65536&overrun_nonfatal=1 --open-timeout-ms 5000 --read-timeout-ms 5000 --analyze-duration-us 5000000 --probe-size 5000000 --mpegts-max-pcr-gap-ms 1000
```

For each input prefix, the three exact output suffixes were appended before the common tail. Route-specific `--media-id` values were `formal-url-to-*`, `formal-rtp-to-*`, and `formal-tsudp-to-*`; `--max-duration` was 145 seconds except RTP-to-RTP at 170 seconds and MPEG-TS/UDP-to-MPEG-TS/RTP at 125 seconds.

```text
Separate RTP:
--output-layout separate --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 64020 --packet-size 1200 --sdp D:\Code\MyCode\MediaTranscode\out\acceptance\realtime-termination-correctness\<media-id>\output.sdp

MPEG-TS/UDP:
--output-layout mpegts --output-transport udp --output udp://127.0.0.1:64120

MPEG-TS/RTP:
--output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 64140 --packet-size 1328 --sdp D:\Code\MyCode\MediaTranscode\out\acceptance\realtime-termination-correctness\<media-id>\output.sdp
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

& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -loglevel warning -protocol_whitelist file,udp,rtp -i 'D:\Code\MyCode\MediaTranscode\out\acceptance\realtime-termination-correctness\<media-id>\output.sdp' -map 0:v:0 -map 0:a:0 -c:v copy -c:a aac -f mpegts 'udp://127.0.0.1:64400?pkt_size=1316'

& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -loglevel warning -protocol_whitelist file,udp,rtp -i 'D:\Code\MyCode\MediaTranscode\out\acceptance\realtime-termination-correctness\<media-id>\output.sdp' -map 0:v:0 -map 0:a:0 -c copy -f mpegts 'udp://127.0.0.1:64400?pkt_size=1316'

& 'D:\VideoLAN\VLC\vlc.exe' 'udp://@127.0.0.1:64400' --no-video-title-show
```

| Input | Separate RTP | MPEG-TS/UDP | MPEG-TS/RTP |
|---|---:|---:|---:|
| URL/session | 145 s, 3.664% | 145 s, 3.662% | 145 s, 3.674% |
| Separate RTP | 170 s, 3.498% | 145 s, 4.216% | 145 s, 3.944% |
| MPEG-TS/UDP | 145 s, 4.367% | 145 s, 4.365% | 125 s, 4.345% |

Every percentage is the realtime CLI process `averageProcessCpuPercent`, not machine CPU. During active playback every route recorded `workerErrors=0`, `errors=0`, and `droppedBuffers=0`; late working sets were about 244-258 MB without sustained growth. Visible VLC had picture and sound with no perceptible continuous A/V drift. Network-source termination was accepted only as source-loss failure; local finite input EOF was accepted only as success.

## Remaining Risks

1. Connectionless UDP cannot prove that an absent sender ended normally; sender disappearance remains source loss.
2. The repository intentionally has no automated regression safety net; concurrency and termination gates remain real-media manual work.
3. Long-duration fault injection and repeated generation transitions still depend on manual live-stream acceptance.
4. A receiver joining between parameter-set/key-frame boundaries may log temporary missing-PPS warnings.
5. One stalled interval was observed in several RTP/MPEG-TS input routes without errors, drops, or visible drift; longer observation may refine that bound.
6. Windows dynamically excluded UDP ports caused local bind/loading failures; acceptance used verified allowed ports 64000-64400.
