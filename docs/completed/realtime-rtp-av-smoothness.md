# Realtime RTP Audio+Video Smoothness

## Root Cause

- Realtime RTP plans owned low-latency policy, but builders and the CLI did not consistently consume it. RTP data edges could still become blocking queues, and CLI workers inherited the default `idleSleepMs=1`.
- Raw RTP AAC input always planned an audio decode/resample/encode branch, so AAC-to-AAC realtime audio could not use the existing packet-copy path.
- The VLC no-video symptom was reproduced as an SDP payload-type ordering mismatch: output RTP packets used video PT 96 and audio PT 97, while SDP could be emitted as audio PT 96 and video PT 97 when the audio mux context arrived first.

## Fix Summary

- Realtime RTP plans now expose planner-owned threading policy and lane-specific edge policies. Realtime execution uses per-node workers, high priority, and `idleSleepMs=0`.
- Realtime builders consume the planned edge policy set; RTP data lanes use `SpscRing`, and audio packet/frame lanes do not use video-only `DropNonKeyFrame`.
- Raw RTP AAC-to-AAC with matching sample rate, channels, and encode options plans `CopyPacket`; requested target changes still plan `TranscodeFrame`.
- SDP generation now orders separate RTP contexts by media type before calling `av_sdp_create`, so video and audio payload types match the packet streams.
- Graph fixed sleeps are covered by a unit audit. The remaining worker sleep is classified as non-realtime idle backoff, and realtime plans disable it.

## Test Commands And Results

```powershell
out\build\x64-debug\media_transcode_realtime_graph_tests.exe
```

Result: passed. The test suite includes coverage for raw RTP AAC copy/transcode decisions, realtime non-blocking data edges, planner threading policy, CLI policy application, SDP media ordering, and graph sleep classification.

```powershell
ctest --test-dir out/build/x64-debug --output-on-failure
```

Result: passed. CTest ran `media_transcode_realtime_graph_tests` and reported `100% tests passed, 0 tests failed out of 1`.

```powershell
git diff --check
```

Result: passed. No whitespace or line-ending errors were reported.

## Automated RTP Smoke

### Hardware RTP AV Payload Probe

```powershell
D:\mabs\local64\bin-video\ffmpeg.exe -hide_banner -loglevel warning -re -stream_loop -1 -i out\build\x64-debug\test.mp4 -map 0:v:0 -an -c:v copy -f rtp -payload_type 96 "rtp://127.0.0.1:7354?pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 "rtp://127.0.0.1:7356?pkt_size=1200"

out\build\x64-debug\media_transcode_realtime_video_cli.exe --quiet-graph --input-type rtp --input-layout separate --output-layout separate --video-rtp-url rtp://127.0.0.1:7354 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --video-rtp-fmtp "packetization-mode=1; sprop-parameter-sets=Z01AMpWQAoALWwEQAAA+gAAOpghA,aOuPIA==; profile-level-id=4D4032" --audio-rtp-url rtp://127.0.0.1:7356 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;config=1210;sizelength=13;indexlength=3;indexdeltalength=3" --open-timeout-ms 8000 --read-timeout-ms 8000 --analyze-duration-us 500000 --probe-size 524288 --rtp-host 127.0.0.1 --rtp-port 7360 --sdp out\build\x64-debug\codex_rtp_pt_probe_fixed_av_hw.sdp --packet-size 1200 --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --video-codec h264 --audio-codec aac --bitrate 8406 --rc auto --max-duration 6 --progress-timeout-ms 12000 --poll-interval-ms 500
```

Result: CLI exited 0, selected `cuda-nvenc`, reported `workerErrors=0`, and completed with `encodedPacketsPushed=411` / `encodedPacketsPopped=411`. Output SDP mapped video to PT 96 on port 7360 and audio to PT 97 on port 7362. UDP probe observed only PT 96 on the video port and only PT 97 on the audio port.

### FFmpeg SDP Receiver

```powershell
D:\mabs\local64\bin-video\ffmpeg.exe -hide_banner -protocol_whitelist file,udp,rtp -i out\build\x64-debug\codex_ffmpeg_recv_fixed_av_hw.sdp -t 5 -map 0:v:0 -map 0:a:0 -f null -
```

Result: receiver opened both streams, decoded H264 video and AAC audio to null output, and produced no H264 PPS/decode errors. A residual FFmpeg null-mux DTS warning appeared on the audio stream; RTP header timestamp probing below showed packet timestamps remained monotonic.

### RTP Timestamp Probe

```powershell
out\build\x64-debug\media_transcode_realtime_video_cli.exe --quiet-graph --input-type rtp --input-layout separate --output-layout separate --video-rtp-url rtp://127.0.0.1:7394 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --video-rtp-fmtp "packetization-mode=1; sprop-parameter-sets=Z01AMpWQAoALWwEQAAA+gAAOpghA,aOuPIA==; profile-level-id=4D4032" --audio-rtp-url rtp://127.0.0.1:7396 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;config=1210;sizelength=13;indexlength=3;indexdeltalength=3" --open-timeout-ms 8000 --read-timeout-ms 8000 --analyze-duration-us 500000 --probe-size 524288 --rtp-host 127.0.0.1 --rtp-port 7400 --sdp out\build\x64-debug\codex_rtp_timestamp_probe_fixed_av_hw.sdp --packet-size 1200 --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --video-codec h264 --audio-codec aac --bitrate 8406 --rc auto --max-duration 6 --progress-timeout-ms 12000 --poll-interval-ms 500
```

Result: CLI exited 0, selected `cuda-nvenc`, and reported `workerErrors=0`. RTP probe observed no backward timestamps: video port 7400 had `backward=0`, audio port 7402 had `backward=0`, and audio RTP timestamp deltas were consistently 1024.

### VLC Dummy Receiver

```powershell
D:\VideoLAN\VLC\vlc.exe -I dummy --play-and-exit --run-time=8 out\build\x64-debug\codex_vlc_dummy_fixed_av_hw.sdp
```

Result: VLC dummy opened the fixed SDP. Filtered logs showed no `no data`, decode, corrupt, late, lost, warning, or error lines.

## Manual VLC Check

```powershell
powershell -ExecutionPolicy Bypass -File out\build\x64-debug\codex_vlc_visible_smoke.ps1
```

Result: passed. The final visible VLC run used hardware encode output for 60 seconds. User observation: VLC played normally, playback was clearly smooth, audio started first, and video appeared about 1.5-2 seconds later. The generated SDP mapped video to PT 96 on port 7520 and audio to PT 97 on port 7522. VLC logs showed H264 decoding through `d3d11va`; playback-clock adjustment warnings and a final `no data received in 10s` appeared during shutdown/stream end, but no visible stutter or corruption was observed.

## Remaining Risks

- The audio null-mux DTS warning seen in FFmpeg receiver diagnostics needs a follow-up investigation even though RTP header timestamps were monotonic.
- Visible VLC startup still shows audio before video by about 1.5-2 seconds. Playback stays smooth after video starts, but initial A/V startup alignment needs follow-up work.
- The worker idle path still contains a fixed sleep for non-realtime backoff. Realtime plans disable it now; a future P1 runtime change should replace the fallback with event-driven wakeup or bounded scheduler wait.
- SDP media ordering now matches separate RTP output payload assignment. A future planner-level output payload-type contract would make this more explicit for multi-stream extensions.
