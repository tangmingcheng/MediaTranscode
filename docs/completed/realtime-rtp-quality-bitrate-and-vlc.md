# Realtime RTP Quality, Bitrate, and VLC Verification

## Completed Changes

- Added planner coverage and implementation so raw RTP video transcode rejects omitted `--bitrate` when the input bitrate is not observable.
- Added input video bitrate capture for capability-scanned file/realtime URL inputs.
- Propagated the resolved realtime video bitrate into graph node options before building runtime nodes.
- Retested local video transcode, realtime RTP video-only, realtime RTP video+audio, and VLC-side playback diagnostics with hardware enabled.

## Root Cause

The local `test.mp4` source video is about `8405 kb/s`, but raw RTP input metadata only carries codec/payload/fmtp information and does not expose source bitrate. Before this change, raw RTP realtime transcode could omit `--bitrate` and still reach the encoder, allowing FFmpeg/NVENC behavior to produce a low bitrate output SDP such as `b=AS:2000`. This caused the no-audio realtime output to be visibly less clear than the source.

For the video+audio VLC stutter report, the graph path with explicit `--bitrate 8406` produced both audio and video RTP packets with `workerErrors=0`. VLC still logged RTP loss and playback early/late events. The same VLC symptoms also appeared when VLC played the same FFmpeg RTP sender directly, bypassing this project. That points to the test sender/player timing and VLC/D3D11 receiver behavior as the remaining playback instability source, not a graph-only code path. FFmpeg's own `h264_nvenc` RTP SDP also omits H264 `sprop-parameter-sets`, while stream-copy RTP SDP includes it.

## Commands and Results

```powershell
D:\mabs\local64\bin-video\ffprobe.exe -hide_banner -v error -select_streams v:0 -show_entries stream=codec_name,width,height,r_frame_rate,avg_frame_rate,bit_rate -of default=nw=1 D:\Code\MyCode\MediaTranscode\out\build\x64-debug\test.mp4
```

Result: exit code `0`; source video is H264, `2560x1440`, `30/1`, `bit_rate=8405831`.

```powershell
cmd /c "call D:\VisualStudio2026\VC\Auxiliary\Build\vcvars64.bat && cmake --build out/build/x64-debug --target media_transcode_core media_transcode_local_video_cli media_transcode_realtime_video_cli media_transcode_realtime_graph_tests && out\build\x64-debug\media_transcode_realtime_graph_tests.exe"
```

Result: exit code `0`; build completed and output ended with `realtime graph tests passed`.

```powershell
out\build\x64-debug\media_transcode_realtime_video_cli.exe --input-type rtp --input-layout separate --output-layout separate --video-rtp-url rtp://127.0.0.1:5364 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --video-rtp-fmtp "packetization-mode=1;sprop-parameter-sets=Z01AMpWQAoALWwEQAAA+gAAOpghhA,aOuPIA==;profile-level-id=4D4032" --open-timeout-ms 8000 --read-timeout-ms 8000 --analyze-duration-us 500000 --probe-size 524288 --rtp-host 127.0.0.1 --rtp-port 5370 --sdp out\build\x64-debug\codex_missing_bitrate_reject.sdp --packet-size 1200 --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --video-codec h264 --max-duration 1 --progress-timeout-ms 1000 --poll-interval-ms 250 --no-audio
```

Result: exit code `1`; planner/build rejected the request with `InvalidArgument: Realtime RTP video bitrate is required when input bitrate is not observable`.

```powershell
out\build\x64-debug\media_transcode_local_video_cli.exe --input out\build\x64-debug\test.mp4 --output out\build\x64-debug\codex_quality_local_hw_resize.mp4 --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --video-codec h264 --audio-codec aac --width 1280 --height 720 --bitrate 4000
```

Result: exit code `0`; selected `cuda-nvenc`, decoder `h264_cuvid`, filter `scale_cuda=1280:720`, encoder `h264_nvenc`; final summary reported `completed=true`, `total_pushed=3077`, `total_popped=3077`.

```powershell
$ffmpeg='D:\mabs\local64\bin-video\ffmpeg.exe'
$senderArgs=@('-hide_banner','-loglevel','warning','-re','-stream_loop','-1','-i','D:\Code\MyCode\MediaTranscode\out\build\x64-debug\test.mp4','-map','0:v:0','-an','-c:v','copy','-f','rtp','-payload_type','96','rtp://127.0.0.1:5364?pkt_size=1200')
# Start the sender, then run:
out\build\x64-debug\media_transcode_realtime_video_cli.exe --input-type rtp --input-layout separate --output-layout separate --video-rtp-url rtp://127.0.0.1:5364 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --video-rtp-fmtp 'packetization-mode=1;sprop-parameter-sets=Z01AMpWQAoALWwEQAAA+gAAOpghhA,aOuPIA==;profile-level-id=4D4032' --open-timeout-ms 8000 --read-timeout-ms 8000 --analyze-duration-us 500000 --probe-size 524288 --rtp-host 127.0.0.1 --rtp-port 5370 --sdp out\build\x64-debug\codex_quality_video_only.sdp --packet-size 1200 --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --video-codec h264 --bitrate 8406 --rc auto --max-duration 5 --progress-timeout-ms 12000 --poll-interval-ms 250 --no-audio
```

Result: exit code `0`; selected `cuda-nvenc`, decoder `h264_cuvid`, encoder `h264_nvenc`; final summary reported `encodedPacketsPushed=100`, `encodedPacketsPopped=100`, `workerErrors=0`. `codex_quality_video_only.sdp` contains `m=video 5370 RTP/AVP 96` and `b=AS:8406`.

```powershell
$ffmpeg='D:\mabs\local64\bin-video\ffmpeg.exe'
$senderArgs=@('-hide_banner','-loglevel','warning','-re','-stream_loop','-1','-i','D:\Code\MyCode\MediaTranscode\out\build\x64-debug\test.mp4','-map','0:v:0','-an','-c:v','copy','-f','rtp','-payload_type','96','rtp://127.0.0.1:5384?pkt_size=1200','-map','0:a:0','-vn','-c:a','aac','-ar','44100','-ac','2','-f','rtp','-payload_type','97','rtp://127.0.0.1:5386?pkt_size=1200')
# Start the sender, then run:
out\build\x64-debug\media_transcode_realtime_video_cli.exe --input-type rtp --input-layout separate --output-layout separate --video-rtp-url rtp://127.0.0.1:5384 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --video-rtp-fmtp 'packetization-mode=1;sprop-parameter-sets=Z01AMpWQAoALWwEQAAA+gAAOpghhA,aOuPIA==;profile-level-id=4D4032' --audio-rtp-url rtp://127.0.0.1:5386 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;config=1210;sizelength=13;indexlength=3;indexdeltalength=3' --open-timeout-ms 8000 --read-timeout-ms 8000 --analyze-duration-us 500000 --probe-size 524288 --rtp-host 127.0.0.1 --rtp-port 5390 --sdp out\build\x64-debug\codex_quality_av.sdp --packet-size 1200 --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --video-codec h264 --audio-codec aac --bitrate 8406 --rc auto --max-duration 5 --progress-timeout-ms 12000 --poll-interval-ms 250
```

Result: exit code `0`; selected `cuda-nvenc`, decoder `h264_cuvid`, encoder `h264_nvenc`; logs included `rtp_mux.first_packet_written stream=audio` and `rtp_mux.first_packet_written stream=video`; final summary reported `encodedPacketsPushed=320`, `encodedPacketsPopped=320`, `workerErrors=0`. `codex_quality_av.sdp` contains `m=audio 5392 RTP/AVP 97`, `m=video 5390 RTP/AVP 96`, audio `b=AS:128`, and video `b=AS:8406`.

```powershell
D:\VideoLAN\VLC\vlc.exe --intf dummy --play-and-exit --run-time=15 --no-audio-time-stretch --file-logging --logfile out\build\x64-debug\codex_vlc_av.log -vv out\build\x64-debug\codex_vlc_av.sdp
```

Result: VLC was started against the live graph output SDP. The graph process exited `0` with `workerErrors=0`. VLC was stopped by the harness after the run window and wrote `codex_vlc_av.log`; relevant VLC diagnostics included `live555 debug: lost 161750 bytes`, repeated playback early/late warnings, and one `avcodec error: hardware acceleration picture allocation failed`.

```powershell
D:\mabs\local64\bin-video\ffmpeg.exe -hide_banner -loglevel warning -re -stream_loop -1 -i out\build\x64-debug\test.mp4 -map 0:v:0 -an -c:v copy -f rtp -payload_type 96 -sdp_file out\build\x64-debug\codex_vlc_direct.sdp "rtp://127.0.0.1:5504?pkt_size=1200" -map 0:a:0 -vn -c:a aac -ar 44100 -ac 2 -f rtp -payload_type 97 "rtp://127.0.0.1:5506?pkt_size=1200"
D:\VideoLAN\VLC\vlc.exe --intf dummy --play-and-exit --run-time=15 --no-audio-time-stretch --file-logging --logfile out\build\x64-debug\codex_vlc_direct.log -vv out\build\x64-debug\codex_vlc_direct.sdp
```

Result: FFmpeg sender direct-to-VLC, bypassing this project, reproduced VLC-side receiver issues: `live555 debug: lost 37075 bytes`, `main error: buffer deadlock prevented`, and the FFmpeg sender logged `Resumed reading ... after a lag`. This supports treating the remaining VLC stutter diagnostics as sender/player timing or local VLC/D3D11 behavior unless a future graph-specific receiver trace shows packet corruption before VLC.

```powershell
D:\mabs\local64\bin-video\ffmpeg.exe -hide_banner -loglevel warning -y -t 3 -i out\build\x64-debug\test.mp4 -map 0:v:0 -an -c:v h264_nvenc -b:v 8406k -f rtp -payload_type 96 -sdp_file out\build\x64-debug\codex_ffmpeg_nvenc_rtp.sdp "rtp://127.0.0.1:5520?pkt_size=1200"
```

Result: exit code `0`; FFmpeg's own NVENC RTP SDP contained `b=AS:8406` and `a=fmtp:96 packetization-mode=1`, but no H264 `sprop-parameter-sets`. This matches the graph's NVENC RTP SDP behavior and differs from stream-copy RTP SDP, which includes source `sprop-parameter-sets`.
