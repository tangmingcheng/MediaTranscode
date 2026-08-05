# Separate RTP Video FMTP Auto-Detection

## Delivered

- Optional video fmtp: explicit value is manual/no-I/O mode; omitted H.264/HEVC fmtp triggers preflight detection.
- Shared H.264/HEVC RTP NAL parsing for the probe and depacketizers.
- Typed H.264 SPS/PPS and HEVC VPS/SPS/PPS facts, ambiguity rejection, SSRC epoch reset, and canonical fmtp generation.
- Same-socket prepared RTP/RTCP queue handoff through a move-only RAII owner; automatic mode has no runtime fallback.
- AAC missing fmtp remains invalid; codec, PT, clock, URL, and probe limits remain caller-owned.

## Machine Acceptance

Both final-head automatic routes used 1280x720, 30 fps, 4000 kbps, GOP 30, 1200-byte RTP packets, explicit AAC fmtp, software decode/encode, and 120 seconds of FFmpeg output decoding.

H.264 sender:

```powershell
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -loglevel warning -re -f lavfi -i 'testsrc2=size=1280x720:rate=30' -re -f lavfi -i 'sine=frequency=1000:sample_rate=44100' -map 0:v:0 -an -c:v libx264 -preset ultrafast -tune zerolatency -b:v 4000k -x264-params 'repeat-headers=1:keyint=30' -payload_type 96 -ssrc 53201 -t 138 -f rtp 'rtp://127.0.0.1:53200?rtcpport=53201&pkt_size=1200' -map 1:a:0 -vn -c:a aac -ar 44100 -ac 2 -payload_type 97 -ssrc 53203 -t 138 -f rtp 'rtp://127.0.0.1:53202?rtcpport=53203&pkt_size=1200'
```

HEVC sender used the same audio output and replaced the video branch with:

```powershell
-map 0:v:0 -an -c:v libx265 -preset ultrafast -tune zerolatency -b:v 4000k -x265-params 'repeat-headers=1:keyint=30:min-keyint=30:scenecut=0' -payload_type 98 -ssrc 53301 -t 138 -f rtp 'rtp://127.0.0.1:53300?rtcpport=53301&pkt_size=1200'
```

The H.264 CLI omitted only video fmtp; the HEVC CLI used codec `hevc` and PT 98:

```powershell
& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe' --media-id fmtp-auto-h264-av --input-type rtp --input-layout separate --video-rtp-url 'rtp://127.0.0.1:53200' --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url 'rtp://127.0.0.1:53202' --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210' --open-timeout-ms 8000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --output-layout separate --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 53220 --packet-size 1200 --sdp 'D:\Code\MyCode\MediaTranscode\out\acceptance\rtp-fmtp-autodetect\h264-av-zerolatency-120s\output.sdp' --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 --video-codec h264 --preset ultrafast --tune zerolatency --width 1280 --height 720 --fps 30 --bitrate 4000 --gop 30 --audio-codec aac --audio-bitrate 128 --sample-rate 48000 --channels 2 --disable-hw --max-duration 130 --progress-timeout-ms 12000 --first-output-timeout-ms 30000 --poll-interval-ms 1000
```

FFmpeg receiver for each generated SDP:

```powershell
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -loglevel warning -protocol_whitelist file,udp,rtp -i '<generated-output.sdp>' -t 120 -f null NUL
```

Results:

- H.264: probe `parameter_sets=sps,pps`, 45 packets, 47,989 bytes, 392 ms; receiver/CLI exit 0; `workerErrors=0`, `errors=0`, `droppedBuffers=0`, `stalledIntervals=0`; late drift 5,291 ns; final working set 84,860,928 bytes.
- HEVC: probe `parameter_sets=vps,sps,pps`, 41 packets, 47,874 bytes, 415 ms; receiver/CLI exit 0; `workerErrors=0`, `errors=0`, `droppedBuffers=0`, `stalledIntervals=0`; late drift 18,775 ns; final working set 69,627,904 bytes.
- Manual H.264 regression: 25 seconds, receiver/CLI exit 0, logs contain only `mode=manual`, and final errors/drops are zero.
- Failure gates: no sender, expected PT 98/observed 99, incomplete SPS-only evidence, conflicting SPS, 100-byte total probe limit, and missing AAC fmtp all exited 1 in preflight; no runtime lifecycle started.

## Visual Acceptance

VLC launch and human picture/quality observation are intentionally pending explicit user approval. No visual PASS is claimed in this report.

## Residual Risks

- AAC continues to require authoritative fmtp; bare RTP cannot identify codec, PT, or clock rate.
- The supported HEVC packetization contract is non-interleaved SRST without DONL.
- Runtime parameter-set switching is outside this first delivery.
