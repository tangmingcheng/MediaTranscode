# Realtime RTP H264 Global Header and VLC Retest

## Completed Changes

- Added an explicit `video.global_header` transcode parameter and graph option.
- Made the realtime RTP planner set `video.global_header=1` only when the resolved output is separate RTP H264.
- Kept the responsibility split: planner decides protocol requirements, video segment builder propagates explicit options, and encoder context builder applies `AV_CODEC_FLAG_GLOBAL_HEADER`.
- Added realtime graph coverage proving separate RTP H264 output requests the global-header option.
- Rebuilt cleanly after the parameter layout change and retested realtime RTP video+audio with VLC.

## Root Cause

The VLC video+audio RTP path showed artifacts and stutter after the bitrate fix. Graph logs showed hardware encode was active and runtime queues were not growing enough to explain corruption. The missing protocol detail was H264 parameter-set signaling for RTP: FFmpeg/NVENC RTP output only produced SDP `sprop-parameter-sets` when the encoder context used `AV_CODEC_FLAG_GLOBAL_HEADER`.

This is a planner-level RTP output requirement, not a VLC workaround and not a runtime fallback. The planner now derives it from the resolved output layout and output codec. If the output is separate RTP H264, the encoder context receives an explicit `video.global_header=1` option before opening the encoder.

The repeated `media_transcode_realtime_graph_tests.exe` access violation after this change was caused by stale incremental build objects after adding a field to `MediaVideoTranscodeParameters`. A clean rebuild removed the ABI/layout mismatch and the test process stopped crashing.

## Commands and Results

```powershell
cmd /c "call D:\VisualStudio2026\VC\Auxiliary\Build\vcvars64.bat && cmake --build out/build/x64-debug --clean-first --target media_transcode_realtime_graph_tests"
```

Result: exit code `0`; CMake cleaned `173` files and rebuilt `170` steps. This removed the stale-object access violation seen in `media_transcode_realtime_graph_tests.exe`.

```powershell
cmd /c "call D:\VisualStudio2026\VC\Auxiliary\Build\vcvars64.bat && cmake --build out/build/x64-debug --target media_transcode_realtime_graph_tests && out\build\x64-debug\media_transcode_realtime_graph_tests.exe"
```

Result: exit code `0`; output ended with `realtime graph tests passed`.

```powershell
D:\mabs\local64\bin-video\ffmpeg.exe -hide_banner -loglevel warning -re -stream_loop -1 -i out\build\x64-debug\test.mp4 -map 0:v:0 -an -c:v copy -f rtp -payload_type 96 "rtp://127.0.0.1:5704?pkt_size=1200" -map 0:a:0 -vn -c:a aac -ar 44100 -ac 2 -f rtp -payload_type 97 "rtp://127.0.0.1:5706?pkt_size=1200"

out\build\x64-debug\media_transcode_realtime_video_cli.exe --input-type rtp --input-layout separate --output-layout separate --video-rtp-url rtp://127.0.0.1:5704 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --video-rtp-fmtp "packetization-mode=1;sprop-parameter-sets=Z01AMpWQAoALWwEQAAA+gAAOpghhA,aOuPIA==;profile-level-id=4D4032" --audio-rtp-url rtp://127.0.0.1:5706 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;config=1210;sizelength=13;indexlength=3;indexdeltalength=3" --open-timeout-ms 8000 --read-timeout-ms 8000 --analyze-duration-us 500000 --probe-size 524288 --rtp-host 127.0.0.1 --rtp-port 5710 --sdp out\build\x64-debug\codex_av_global_header.sdp --packet-size 1200 --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --video-codec h264 --audio-codec aac --bitrate 8406 --rc auto --max-duration 10 --progress-timeout-ms 12000 --poll-interval-ms 250
```

Result: graph exit code `0`; sender was stopped by the harness after the graph completed. The graph selected `cuda-nvenc`, `h264_cuvid`, `filter=not_required`, and `h264_nvenc`. Final runtime summary reported `workerErrors=0`, `encodedPacketsPushed=673`, `encodedPacketsPopped=673`. The SDP included `sprop-parameter-sets=...; profile-level-id=4D4032`.

```powershell
D:\mabs\local64\bin-video\ffmpeg.exe -hide_banner -loglevel warning -re -stream_loop -1 -i out\build\x64-debug\test.mp4 -map 0:v:0 -an -c:v copy -f rtp -payload_type 96 "rtp://127.0.0.1:5724?pkt_size=1200" -map 0:a:0 -vn -c:a aac -ar 44100 -ac 2 -f rtp -payload_type 97 "rtp://127.0.0.1:5726?pkt_size=1200"

out\build\x64-debug\media_transcode_realtime_video_cli.exe --input-type rtp --input-layout separate --output-layout separate --video-rtp-url rtp://127.0.0.1:5724 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --video-rtp-fmtp "packetization-mode=1;sprop-parameter-sets=Z01AMpWQAoALWwEQAAA+gAAOpghhA,aOuPIA==;profile-level-id=4D4032" --audio-rtp-url rtp://127.0.0.1:5726 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;config=1210;sizelength=13;indexlength=3;indexdeltalength=3" --open-timeout-ms 8000 --read-timeout-ms 8000 --analyze-duration-us 500000 --probe-size 524288 --rtp-host 127.0.0.1 --rtp-port 5730 --sdp out\build\x64-debug\codex_av_global_header_vlc.sdp --packet-size 1200 --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --video-codec h264 --audio-codec aac --bitrate 8406 --rc auto --max-duration 14 --progress-timeout-ms 12000 --poll-interval-ms 250

D:\VideoLAN\VLC\vlc.exe --intf dummy --play-and-exit --run-time=8 --verbose=2 --file-logging --logfile out\build\x64-debug\codex_av_global_header_vlc.log out\build\x64-debug\codex_av_global_header_vlc.sdp
```

Result: `vlc_exit=0`, graph exit code `0`, sender was stopped by the harness after the graph completed. VLC log included `RTP subsession 'audio/MPEG4-GENERIC'`, `RTP subsession 'video/H264'`, `found NAL_PPS`, and `Received first picture`. `Select-String` found no `waiting for SPS/PPS` entry. The graph selected the hardware path and final runtime summary reported `workerErrors=0`, `encodedPacketsPushed=851`, `encodedPacketsPopped=851`.

```powershell
Select-String -Path out\build\x64-debug\codex_av_global_header_vlc.log -Pattern 'waiting for SPS/PPS','Received first picture','found NAL_PPS','RTP subsession','lost ','playback too late','hardware acceleration picture allocation failed'
```

Result: found `found NAL_PPS` and `Received first picture`; did not find `waiting for SPS/PPS`. Remaining VLC diagnostics included `live555 debug: lost 161750 bytes`, repeated `playback too late` messages, and `avcodec error: hardware acceleration picture allocation failed`.

## Remaining Risks

- VLC still reports receiver-side RTP loss and playback timing pressure in the video+audio test.
- VLC also reports hardware acceleration picture allocation failures. That is outside the graph encoder path because graph logs confirm CUDA/NVENC encode succeeds with `workerErrors=0`.
- If visible stutter remains after the SDP parameter-set fix, the next investigation should focus on receiver jitter buffering, local VLC hardware decode, RTP packet pacing, and whether separate audio/video RTP sessions need stricter synchronized pacing policy in the DAG scheduler.
