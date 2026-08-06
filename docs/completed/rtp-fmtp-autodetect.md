# Separate RTP Video FMTP Auto-Detection

## Delivered

- Optional video fmtp: explicit value is manual/no-I/O mode; omitted H.264/HEVC fmtp triggers preflight detection.
- Shared H.264/HEVC RTP NAL parsing for the probe and depacketizers.
- Typed H.264 SPS/PPS and HEVC VPS/SPS/PPS facts, ambiguity rejection, SSRC epoch reset, and canonical fmtp generation.
- Same-socket prepared RTP/RTCP queue handoff through a move-only RAII owner; the planner product selects the exact prepared-input kind and automatic mode has no runtime fallback.
- Preflight capture stops at runtime activation; the bounded queue drains once,
  logs its completion, and reception continues directly from the same transport.
- AAC missing or invalid fmtp fails the request-only validation before any video probe I/O; codec, PT, clock, URL, and probe limits remain caller-owned.

## Real-Media Acceptance

Final reruns use the hardware pipeline, 1280x720, 30 fps, 4000 kbps, GOP 30, explicit AAC fmtp, source-driven CLI lifetime, and the saved continuous 120-second source. The direct source command is:

```powershell
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -loglevel warning -re -i 'D:\Code\MyCode\MediaTranscode\out\acceptance\test-continuous-120s.mp4' -map 0:v:0 -c:v copy -bsf:v 'h264_mp4toannexb' -an -payload_type 96 -ssrc 52001 -f rtp 'rtp://127.0.0.1:52000?rtcpport=52001' -map 0:a:0 -c:a copy -vn -payload_type 97 -ssrc 52003 -f rtp 'rtp://127.0.0.1:52002?rtcpport=52003'
```

Representative automatic H.264 to UDP-TS CLI and direct VLC command:

```powershell
& 'D:\VideoLAN\VLC\vlc.exe' 'udp://@127.0.0.1:1234'
& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe' --media-id formal-auto-rtp-to-tsudp --input-type rtp --input-layout separate --video-rtp-url 'rtp://127.0.0.1:52000' --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url 'rtp://127.0.0.1:52002' --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210' --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --output-layout mpegts --output-transport udp --output 'udp://127.0.0.1:1234' --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 --video-codec h264 --width 1280 --height 720 --fps 30 --bitrate 4000 --gop 30 --audio-codec aac --audio-bitrate 128 --sample-rate 48000 --channels 2 --progress-timeout-ms 10000 --first-output-timeout-ms 30000 --poll-interval-ms 100
```

Manual H.264 used the same CLI with the following authoritative option and performed no probe I/O:

```powershell
--video-rtp-fmtp 'packetization-mode=1;sprop-parameter-sets=Z01AH5WQBQBbsBEAAAMD6AAA6mDgAAAehIAAA9CQbvKI+OFSQA==,aOuPIA==;profile-level-id=4D401F'
```

RTP/AVP MP2T routes used direct VLC URL `rtp://@127.0.0.1:1234`, `--output-transport rtp`, `--rtp-port 1234`, and `--packet-size 1328`. UDP-TS routes used `udp://@127.0.0.1:1234` and `--output 'udp://127.0.0.1:1234'`.

HEVC automatic detection used NVIDIA HEVC source encoding and hardware HEVC decode to H.264 NVENC:

```powershell
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -loglevel warning -re -hwaccel cuda -hwaccel_output_format cuda -i 'D:\Code\MyCode\MediaTranscode\out\acceptance\test-continuous-120s.mp4' -map 0:v:0 -c:v hevc_nvenc -preset p5 -tune hq -rc vbr -cq 20 -b:v 4000k -maxrate 5000k -bufsize 10000k -g 30 -bf 0 -an -payload_type 96 -ssrc 52001 -f rtp 'rtp://127.0.0.1:52000?rtcpport=52001' -map 0:a:0 -c:a copy -vn -payload_type 97 -ssrc 52003 -f rtp 'rtp://127.0.0.1:52002?rtcpport=52003'
```

Results:

| Route | Source duration | Avg process CPU | Working set | Peak queue | Drops | Human result |
|---|---:|---:|---:|---:|---:|---|
| H.264 manual -> RTP/MP2T | 120 s | 3.61% | 199.8 MiB final, stable | 832 | 0 | Picture/audio normal; no perceptible A/V drift |
| H.264 auto -> RTP/MP2T | 120 s | 3.71% | 200.6 MiB final, stable | 847 | 0 | Picture/audio normal; no perceptible A/V drift |
| H.264 manual -> UDP-TS | 119.9 s | 3.58% | 200.2 MiB final, stable | 792 | 0 | Picture/audio normal; no perceptible A/V drift |
| H.264 auto -> UDP-TS | 119.9 s | 3.78% | 201.2 MiB final, stable | 856 | 0 | Picture/audio normal; no perceptible A/V drift |
| HEVC auto -> H.264 RTP/MP2T | 120.2 s | 4.00% | 201.7 MiB final, stable | 970 | 0 | Picture/audio normal; no perceptible A/V drift |

After the final marker-completion, aggregate-budget, explicit prepared-ownership,
and initial-generation fixes, the directly affected routes were rerun on the
same commands and source:

| Route | Source duration | Avg process CPU | Working set | Peak queue | Drops | Human result |
|---|---:|---:|---:|---:|---:|---|
| H.264 auto -> RTP/MP2T | 120 s | 3.83% | 200.3 MiB final, stable | 914 | 0 | Picture/audio normal; no perceptible A/V drift |
| H.264 manual -> RTP/MP2T | 120 s | 3.18% | 185.6 MiB final, stable | 195 | 0 | Picture/audio normal; no perceptible A/V drift |
| HEVC auto -> H.264 RTP/MP2T | 120 s | 3.30% | 187.7 MiB final, stable | 271 | 0 | Picture/audio normal; no perceptible A/V drift |

All three CLIs remained active for the full source lifetime with zero drops and
no runtime worker error while the source was present. Each then exposed the
real video/audio RTP clock-evidence expiry after the finite sender stopped.

Each CLI remained active while its source was active. After FFmpeg ended, the strict RTP sender-report/CNAME evidence gate stopped the CLI; this was recorded as real source-loss failure, not suppressed or converted to success.

Direct failure gates all stopped in preflight before `compile.begin`: no sender, PT 96/98 conflict, missing SPS/PPS (`2106` matching packets and `parameter_sets=none`), conflicting SPS, one-byte probe capacity, and missing AAC fmtp. On the final code, no-sender returned the bounded open-timeout diagnostic in 6.8 seconds; missing AAC fmtp returned `Raw RTP AAC requires fmtp` in 0.5 seconds without hardware or network probing. No test script or tracked test artifact was created.

Two independent final-head reviews returned PASS with no unresolved P0-P3
findings. They separately checked planner ownership, RAII and exact prepared
binding, aggregate capacity enforcement, error visibility, and the absence of
fallback, queue accumulation, threshold widening, or error-to-success behavior.

## Residual Risks

- AAC continues to require authoritative fmtp; bare RTP cannot identify codec, PT, or clock rate.
- The supported HEVC packetization contract is non-interleaved SRST without DONL.
- Runtime parameter-set switching is outside this first delivery.
- FFmpeg/driver capability calls are checked for deadline overrun after return
  but cannot be interrupted at an exact wall-clock deadline by the current API.
