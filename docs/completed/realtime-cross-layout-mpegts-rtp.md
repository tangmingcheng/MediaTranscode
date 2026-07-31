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

## Remaining Seven Exact Gate Commands

The two MPEG-TS/UDP-input gates above and the following seven blocks form the
complete nine-path command record. Each block lists the direct source command
when one was required, the production CLI command, and the VLC command.

### Separate RTP to separate RTP

```powershell
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -loglevel warning -stream_loop -1 -re -i 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\test.mp4' -map 0:v:0 -c:v copy -an -payload_type 96 -ssrc 52001 -t 190 -f rtp 'rtp://127.0.0.1:52000?rtcpport=52001' -map 0:a:0 -c:a copy -vn -payload_type 97 -ssrc 52003 -t 190 -f rtp 'rtp://127.0.0.1:52002?rtcpport=52003'

& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe' --media-id formal-rtp-to-rtp --input-type rtp --input-layout separate --video-rtp-url 'rtp://127.0.0.1:52000' --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --video-rtp-fmtp 'packetization-mode=1;sprop-parameter-sets=Z01AMpWQAoALWwEQAAA+gAAOpghA,aOuPIA==;profile-level-id=4D4032' --audio-rtp-url 'rtp://127.0.0.1:52002' --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210' --open-timeout-ms 5000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --output-layout separate --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 52020 --packet-size 1200 --sdp 'D:\Code\MyCode\MediaTranscode\out\acceptance\realtime-cross-layout\formal-rtp-to-rtp\output.sdp' --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 --video-codec h264 --width 1280 --height 720 --fps 30 --bitrate 4000 --gop 30 --audio-codec aac --audio-bitrate 128 --sample-rate 48000 --channels 2 --max-duration 170 --progress-timeout-ms 8000 --first-output-timeout-ms 30000 --poll-interval-ms 1000

& 'D:\VideoLAN\VLC\vlc.exe' 'D:\Code\MyCode\MediaTranscode\out\acceptance\realtime-cross-layout\formal-rtp-to-rtp\output.sdp' --no-video-title-show
```

### URL/RTSP to separate RTP

```powershell
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -loglevel warning -re -stream_loop -1 -i 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\test.mp4' -map 0:v:0 -map 0:a:0 -c copy -t 165 -f mpegts -listen 1 'http://127.0.0.1:52300/live.ts'

& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe' --media-id formal-http-url-to-rtp-safe --input-type rtsp --input-layout session --input 'http://127.0.0.1:52300/live.ts' --rtsp-transport tcp --no-low-latency --open-timeout-ms 5000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --output-layout separate --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 52420 --packet-size 1200 --sdp 'D:\Code\MyCode\MediaTranscode\out\acceptance\realtime-cross-layout\formal-url-to-rtp\output-safe.sdp' --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 --video-codec h264 --width 1280 --height 720 --fps 30 --bitrate 4000 --gop 30 --audio-codec aac --audio-bitrate 128 --sample-rate 48000 --channels 2 --max-duration 145 --progress-timeout-ms 8000 --first-output-timeout-ms 30000 --poll-interval-ms 1000

& 'D:\VideoLAN\VLC\vlc.exe' 'D:\Code\MyCode\MediaTranscode\out\acceptance\realtime-cross-layout\formal-url-to-rtp\output-safe.sdp' --no-video-title-show
```

### URL/RTSP to MPEG-TS/UDP

```powershell
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -loglevel warning -re -stream_loop -1 -i 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\test.mp4' -map 0:v:0 -map 0:a:0 -c copy -t 165 -f mpegts -listen 1 'http://127.0.0.1:52300/live.ts'

& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe' --media-id formal-http-url-to-tsudp --input-type rtsp --input-layout session --input 'http://127.0.0.1:52300/live.ts' --rtsp-transport tcp --no-low-latency --open-timeout-ms 5000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --output-layout mpegts --output-transport udp --output 'udp://127.0.0.1:52440' --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 --video-codec h264 --width 1280 --height 720 --fps 30 --bitrate 4000 --gop 30 --audio-codec aac --audio-bitrate 128 --sample-rate 48000 --channels 2 --max-duration 145 --progress-timeout-ms 8000 --first-output-timeout-ms 30000 --poll-interval-ms 1000

& 'D:\VideoLAN\VLC\vlc.exe' 'udp://@127.0.0.1:52440' --no-video-title-show
```

### URL/RTSP to MPEG-TS/RTP

```powershell
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -loglevel warning -re -stream_loop -1 -i 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\test.mp4' -map 0:v:0 -map 0:a:0 -c copy -t 165 -f mpegts -listen 1 'http://127.0.0.1:52300/live.ts'

& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe' --media-id formal-url-to-tsrtp-blocking --input-type rtsp --input-layout session --input 'http://127.0.0.1:52300/live.ts' --rtsp-transport tcp --no-low-latency --open-timeout-ms 5000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 52460 --packet-size 1328 --sdp 'D:\Code\MyCode\MediaTranscode\out\acceptance\realtime-cross-layout\formal-url-to-tsrtp\output.sdp' --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 --video-codec h264 --width 1280 --height 720 --fps 30 --bitrate 4000 --gop 30 --audio-codec aac --audio-bitrate 128 --sample-rate 48000 --channels 2 --max-duration 145 --progress-timeout-ms 8000 --first-output-timeout-ms 30000 --poll-interval-ms 1000

& 'D:\VideoLAN\VLC\vlc.exe' 'rtp://@127.0.0.1:52460' --no-video-title-show
```

### Separate RTP to MPEG-TS/UDP

```powershell
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -loglevel warning -stream_loop -1 -re -i 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\test.mp4' -map 0:v:0 -c:v copy -an -payload_type 96 -ssrc 52001 -t 165 -f rtp 'rtp://127.0.0.1:52000?rtcpport=52001' -map 0:a:0 -c:a copy -vn -payload_type 97 -ssrc 52003 -t 165 -f rtp 'rtp://127.0.0.1:52002?rtcpport=52003'

& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe' --media-id formal-rtp-to-tsudp --input-type rtp --input-layout separate --video-rtp-url 'rtp://127.0.0.1:52000' --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --video-rtp-fmtp 'packetization-mode=1;sprop-parameter-sets=Z01AMpWQAoALWwEQAAA+gAAOpghA,aOuPIA==;profile-level-id=4D4032' --audio-rtp-url 'rtp://127.0.0.1:52002' --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210' --open-timeout-ms 5000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --output-layout mpegts --output-transport udp --output 'udp://127.0.0.1:52480' --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 --video-codec h264 --width 1280 --height 720 --fps 30 --bitrate 4000 --gop 30 --audio-codec aac --audio-bitrate 128 --sample-rate 48000 --channels 2 --max-duration 145 --progress-timeout-ms 8000 --first-output-timeout-ms 30000 --poll-interval-ms 1000

& 'D:\VideoLAN\VLC\vlc.exe' 'udp://@127.0.0.1:52480' --no-video-title-show
```

### Separate RTP to MPEG-TS/RTP

```powershell
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -loglevel warning -stream_loop -1 -re -i 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\test.mp4' -map 0:v:0 -c:v copy -an -payload_type 96 -ssrc 52001 -t 165 -f rtp 'rtp://127.0.0.1:52000?rtcpport=52001' -map 0:a:0 -c:a copy -vn -payload_type 97 -ssrc 52003 -t 165 -f rtp 'rtp://127.0.0.1:52002?rtcpport=52003'

& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe' --media-id formal-rtp-to-tsrtp --input-type rtp --input-layout separate --video-rtp-url 'rtp://127.0.0.1:52000' --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --video-rtp-fmtp 'packetization-mode=1;sprop-parameter-sets=Z01AMpWQAoALWwEQAAA+gAAOpghA,aOuPIA==;profile-level-id=4D4032' --audio-rtp-url 'rtp://127.0.0.1:52002' --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210' --open-timeout-ms 5000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --output-layout mpegts --output-transport rtp --rtp-host 192.168.96.185 --rtp-port 52500 --packet-size 1328 --sdp 'D:\Code\MyCode\MediaTranscode\out\acceptance\realtime-cross-layout\formal-rtp-to-tsrtp\output.sdp' --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 --video-codec h264 --width 1280 --height 720 --fps 30 --bitrate 4000 --gop 30 --audio-codec aac --audio-bitrate 128 --sample-rate 48000 --channels 2 --max-duration 145 --progress-timeout-ms 10000 --first-output-timeout-ms 30000 --poll-interval-ms 100

& 'D:\VideoLAN\VLC\vlc.exe' 'rtp://@192.168.96.185:52500' --no-video-title-show
```

### MPEG-TS/UDP to MPEG-TS/UDP

```powershell
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -loglevel warning -re -stream_loop -1 -i 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\test.mp4' -map 0:v:0 -map 0:a:0 -c copy -t 165 -f mpegts -mpegts_flags +resend_headers -muxdelay 0 'udp://127.0.0.1:64200?pkt_size=1316'

& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe' --media-id formal-tsudp-to-tsudp --input-type mpegts-udp --input-layout mpegts --input 'udp://127.0.0.1:64200?fifo_size=65536&overrun_nonfatal=1' --open-timeout-ms 5000 --read-timeout-ms 5000 --analyze-duration-us 5000000 --probe-size 5000000 --mpegts-max-pcr-gap-ms 1000 --output-layout mpegts --output-transport udp --output 'udp://127.0.0.1:64210' --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 --video-codec h264 --width 1280 --height 720 --fps 30 --bitrate 4000 --gop 30 --audio-codec aac --audio-bitrate 128 --sample-rate 48000 --channels 2 --max-duration 145 --progress-timeout-ms 10000 --first-output-timeout-ms 20000 --poll-interval-ms 100

& 'D:\VideoLAN\VLC\vlc.exe' 'udp://@127.0.0.1:64210' --no-video-title-show
```

## Review-Finding Fix Evidence

The MP2T RTP sender now keeps 64-bit cumulative packet/octet counters for the
session lifetime and serializes their low 32 bits into the RFC wire fields.
Therefore crossing 4 GiB no longer terminates output.

The planner now sizes the RTP socket send buffer from the larger of the
100-millisecond bitrate pressure window, two complete datagrams, and the
planner-owned maximum access-unit burst with encapsulation headroom. The exact
`192.168.96.185:52500` command above then ran for 30 seconds with
`workerErrors=0`, `errors=0`, `droppedBuffers=0`, 5.63% average process CPU, and
a stable 236.7 MiB late working set. The previous `WSAEWOULDBLOCK` failure did
not recur.

A forced-transition diagnostic recorded these first-packet states:

| Generation | First sequence | Cumulative packets | Cumulative octets |
|---:|---:|---:|---:|
| 1 | 18315 | 1 | 188 |
| 2 | 19035 | 721 | 780012 |
| 3 | 23064 | 4750 | 5055884 |
| 4 | 27164 | 8850 | 9381576 |

For every boundary, the modulo-65536 RTP sequence delta exactly equals the
64-bit cumulative packet-count delta: 720, 4029, and 4100. This directly proves
that neither RTP sequence nor RTCP session counters reset when the input
generation changes. The deliberately strict 120 ms PCR-gap injection later
ended with `hard_phase_error`; that run is continuity evidence only and is not
counted as a stability pass.

The final local separate-RTP to MPEG-TS/RTP technical rerun completed 60 seconds
with `workerErrors=0`, `errors=0`, `droppedBuffers=0`, 4.60% average process CPU,
a 240,246,784-byte final working set, and terminal counters of 31,687 packets
and 33,529,424 octets. A separate FFmpeg receiver decoded H.264 1280x720 at
30 fps plus AAC-LC 48 kHz stereo from the first output without RTP-loss,
transport-corruption, H.264, or AAC decode errors. Human observation passed:
CPU stayed below 7%, memory had no large fluctuation, playback had no corruption
or perceptible A/V drift, and first picture appeared about 2-3 seconds after
CLI startup. That bounded delay is consistent with capability probing,
source-clock locking, startup-window alignment, and waiting for a decodable key
frame; it is a non-blocking startup-latency optimization, not an A/V correctness
failure.

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
5. Strict clean startup currently takes about 2-3 seconds on the validated CUDA/RTP path; reducing it would require measured planner/startup optimization without weakening key-frame or A/V-lock gates.
