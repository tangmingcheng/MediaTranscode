# Priority 4 Capability Boundaries

## Completed

- Added explicit stable and unsupported capability maturity states.
- Rejected incomplete optimizer, generic GPU, mesh, remote, and multi-node distributed execution paths.
- Removed defaults and success reports that implied unapplied optimization or execution.
- Added fixed-size audio encoder framing so sample-rate conversion naturally drives AAC re-encoding.

## Verification

Executed test commands:

```powershell
ctest --preset deterministic
ctest --preset integration
ctest --preset hardware
ctest --preset performance
```

Executed local transcode command (repeated until 60 seconds, with and without `--disable-hw`):

```powershell
.\out\build\x64-debug\media_transcode_local_video_cli.exe --input .\out\build\x64-debug\test.mp4 --output .\out\build\x64-debug\p4_local_hw.mp4 --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --video-codec h264 --width 1280 --height 720 --bitrate 2000 --audio-codec aac --audio-bitrate 128 --sample-rate 48000 --channels 2 --quiet-graph
```

Executed realtime CLI commands after starting FFmpeg senders and receivers on the listed ports:

```powershell
.\out\build\x64-debug\ffmpeg.exe -re -stream_loop -1 -i .\out\build\x64-debug\test.mp4 -map 0:v:0 -c:v copy -an -f rtp -payload_type 96 "rtp://127.0.0.1:30100?pkt_size=1200" -map 0:a:0 -c:a copy -vn -f rtp -payload_type 97 "rtp://127.0.0.1:30102?pkt_size=1200"
.\out\build\x64-debug\media_transcode_realtime_video_cli.exe --input-type rtp --input-layout separate --output-layout separate --video-rtp-url rtp://127.0.0.1:30100 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --video-rtp-fmtp "packetization-mode=1;sprop-parameter-sets=Z01AMpWQAoALWwEQAAA+gAAOpghA,aOuPIA==;profile-level-id=4D4032" --audio-rtp-url rtp://127.0.0.1:30102 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --rtp-host 127.0.0.1 --rtp-port 31100 --sdp .\out\build\x64-debug\p4_rtp_hw.sdp --packet-size 1200 --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --video-codec h264 --width 1280 --height 720 --bitrate 2000 --audio-codec aac --sample-rate 48000 --audio-bitrate 128 --max-duration 60 --progress-timeout-ms 5000 --poll-interval-ms 250 --quiet-graph
.\out\build\x64-debug\ffmpeg.exe -protocol_whitelist file,udp,rtp -i .\out\build\x64-debug\p4_rtp_hw.sdp -t 50 -map 0:v:0 -map 0:a:0 -c copy .\out\build\x64-debug\p4_rtp_hw.capture.mkv
.\out\build\x64-debug\ffmpeg.exe -re -stream_loop -1 -i .\out\build\x64-debug\test.mp4 -map 0:v:0 -map 0:a:0 -c copy -f mpegts "udp://127.0.0.1:30300?pkt_size=1316"
.\out\build\x64-debug\media_transcode_realtime_video_cli.exe --input-type mpegts-udp --input-layout mpegts --output-layout mpegts --input "udp://127.0.0.1:30300?fifo_size=1000000&overrun_nonfatal=1" --output "udp://127.0.0.1:31300?pkt_size=1316" --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --video-codec h264 --width 1280 --height 720 --bitrate 2000 --audio-codec aac --sample-rate 48000 --audio-bitrate 128 --max-duration 60 --progress-timeout-ms 5000 --poll-interval-ms 250 --quiet-graph
.\out\build\x64-debug\ffmpeg.exe -i "udp://127.0.0.1:31300?fifo_size=1000000&overrun_nonfatal=1" -t 50 -map 0:v:0 -map 0:a:0 -c copy .\out\build\x64-debug\p4_mpegts_hw.capture.mkv
```

Results:

- Deterministic 6/6, integration 1/1, hardware 1/1, and performance 1/1 passed.
- Local H264/AAC 48 kHz hardware: 9 loops, 63.35 seconds, decode passed.
- Local H264/AAC 48 kHz software: 13 loops, 63.33 seconds, decode passed.
- Separate RTP hardware/software: 60 seconds, stream probe and decode passed; average CLI CPU 1.4707%/5.4218%.
- MPEG-TS UDP hardware/software: 60 seconds, stream probe and decode passed; average CLI CPU 1.7568%/5.6632%.

Runtime FFmpeg binaries were copied manually beside executables. No external runtime path or dependency-copy behavior was added to CMake.
