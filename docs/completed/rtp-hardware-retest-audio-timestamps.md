# RTP Hardware Retest and Audio Timestamp Fix

## Completed Changes

- Retested local file video transcode and realtime RTP video transcode with hardware enabled.
- Reproduced realtime RTP video+audio failure with a looped `test.mp4` sender before the fix.
- Added `AudioMonotonicTimestamp` and applied it in `AudioResampleNode` so audio frames submitted to the encoder have monotonic PTS in the encoder sample-rate time base.
- Added unit coverage for clamping backward RTP audio timestamps and rejecting missing audio source PTS.
- Added node-level `AudioResampleNode` coverage for cloned frames, resampled frames, timestamp writeback, and missing frame PTS failure.

## Root Cause

The video path already normalizes timestamps before encoding, but the audio path forwarded RTP-derived decoded frame PTS through resample/encode without monotonic normalization. RTP AAC input could produce a backward decoded audio timestamp before the file loop boundary, which then reached the RTP mux as non-monotonic DTS.

## Commands and Results

```powershell
cmd /c "call D:\VisualStudio2026\VC\Auxiliary\Build\vcvars64.bat && cmake -S . -B out/build/x64-debug -G Ninja -DCMAKE_MAKE_PROGRAM=D:/VisualStudio2026/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe -DMEDIA_TRANSCODE_BUILD_TESTS=ON"
```

Result: exit code `0`; CMake configured `out/build/x64-debug`.

```powershell
cmd /c "call D:\VisualStudio2026\VC\Auxiliary\Build\vcvars64.bat && cmake --build out/build/x64-debug --target media_transcode_core media_transcode_local_video_cli media_transcode_realtime_video_cli media_transcode_realtime_graph_tests"
```

Result: exit code `0`; all requested debug targets built.

```powershell
out\build\x64-debug\media_transcode_realtime_graph_tests.exe
```

Result: exit code `0`; output ended with `realtime graph tests passed`.

```powershell
out\build\x64-debug\media_transcode_local_video_cli.exe --input out\build\x64-debug\test.mp4 --output out\build\x64-debug\codex_retest_local_hw_resize_fixed.mp4 --width 1280 --height 720 --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256
```

Result: exit code `0`; selected `cuda-nvenc`, decoder `h264_cuvid`, filter `scale_cuda=1280:720`, encoder `h264_nvenc`; final `completed=true`, `total_pushed=3077`, `total_popped=3077`.

```powershell
$ffmpeg = 'D:\mabs\local64\bin-video\ffmpeg.exe'
$senderArgs = @('-hide_banner','-loglevel','warning','-re','-stream_loop','-1','-i','out\build\x64-debug\test.mp4','-map','0:v:0','-an','-c:v','copy','-f','rtp','-payload_type','96','rtp://127.0.0.1:5184?pkt_size=1200')
$sender = Start-Process -FilePath $ffmpeg -ArgumentList $senderArgs -WindowStyle Hidden -PassThru
try {
  Start-Sleep -Seconds 1
  out\build\x64-debug\media_transcode_realtime_video_cli.exe --input-type rtp --input-layout separate --output-layout separate --video-rtp-url rtp://127.0.0.1:5184 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --video-rtp-fmtp 'packetization-mode=1;sprop-parameter-sets=Z01AMpWQAoALWwEQAAA+gAAOpghhA,aOuPIA==;profile-level-id=4D4032' --open-timeout-ms 8000 --read-timeout-ms 8000 --analyze-duration-us 500000 --probe-size 524288 --rtp-host 127.0.0.1 --rtp-port 5190 --sdp out\build\x64-debug\codex_retest_rtp_video_only_fixed.sdp --packet-size 1200 --no-audio --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --video-codec h264 --rc auto --max-duration 5 --progress-timeout-ms 12000 --poll-interval-ms 250
} finally {
  if ($sender -and -not $sender.HasExited) { Stop-Process -Id $sender.Id -Force }
}
```

Result: exit code `0`; selected `cuda-nvenc`, decoder `h264_cuvid`, encoder `h264_nvenc`; final `encodedPacketsPushed=99`, `encodedPacketsPopped=99`, `workerErrors=0`. SDP written to `out\build\x64-debug\codex_retest_rtp_video_only_fixed.sdp` and contains one video RTP media line.

```powershell
Get-Content out\build\x64-debug\codex_retest_rtp_video_only_fixed.sdp
```

Result: exit code `0`; relevant SDP line: `m=video 5190 RTP/AVP 96`.

```powershell
$ffmpeg = 'D:\mabs\local64\bin-video\ffmpeg.exe'
$senderArgs = @('-hide_banner','-loglevel','warning','-re','-stream_loop','-1','-i','out\build\x64-debug\test.mp4','-map','0:v:0','-an','-c:v','copy','-f','rtp','-payload_type','96','rtp://127.0.0.1:5164?pkt_size=1200','-map','0:a:0','-vn','-c:a','aac','-ar','44100','-ac','2','-f','rtp','-payload_type','97','rtp://127.0.0.1:5166?pkt_size=1200')
$sender = Start-Process -FilePath $ffmpeg -ArgumentList $senderArgs -WindowStyle Hidden -PassThru
try {
  Start-Sleep -Seconds 1
  out\build\x64-debug\media_transcode_realtime_video_cli.exe --input-type rtp --input-layout separate --output-layout separate --video-rtp-url rtp://127.0.0.1:5164 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --video-rtp-fmtp 'packetization-mode=1;sprop-parameter-sets=Z01AMpWQAoALWwEQAAA+gAAOpghhA,aOuPIA==;profile-level-id=4D4032' --audio-rtp-url rtp://127.0.0.1:5166 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;config=1210;sizelength=13;indexlength=3;indexdeltalength=3' --open-timeout-ms 8000 --read-timeout-ms 8000 --analyze-duration-us 500000 --probe-size 524288 --rtp-host 127.0.0.1 --rtp-port 5170 --sdp out\build\x64-debug\codex_retest_rtp_av_fixed.sdp --packet-size 1200 --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --video-codec h264 --audio-codec aac --rc auto --max-duration 5 --progress-timeout-ms 12000 --poll-interval-ms 250
} finally {
  if ($sender -and -not $sender.HasExited) { Stop-Process -Id $sender.Id -Force }
}
```

Result: exit code `0`; selected video chain `cuda-nvenc`, decoder `h264_cuvid`, encoder `h264_nvenc`; final `encodedPacketsPushed=331`, `encodedPacketsPopped=331`, `workerErrors=0`. SDP written to `out\build\x64-debug\codex_retest_rtp_av_fixed.sdp` and contains both audio and video RTP media lines.

```powershell
Get-Content out\build\x64-debug\codex_retest_rtp_av_fixed.sdp
```

Result: exit code `0`; relevant SDP lines: `m=audio 5172 RTP/AVP 97` and `m=video 5170 RTP/AVP 96`.
