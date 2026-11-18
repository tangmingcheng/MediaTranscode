# r363 Windows Release realtime 验证

已结束，证据不完整，不能判定完整验收通过。产物6594048 bytes、2026-10-26 18:08:15，来自全量Release85930 exit0（755项）；含独立音频事实贯通与取消路径预分配锁复用，均双独立源码PASS。源45920/41420/38584，CLI37824，VLC41664；主会话17948。18:13:03 VLC原生快照已查看三格。

## 实际启动命令

```powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r363-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r363-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r363-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r363-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r363-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r363-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r363 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r363-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r363-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r363-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r363-native- --snapshot-format=png udp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r363-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
$lastTime=$time
$lastCpu=$cpu
}
Write-Output "CLI_EXIT=$($cli.ExitCode)"
$source0.WaitForExit()
Write-Output "SOURCE0_EXIT=$($source0.ExitCode)"
$source1.WaitForExit()
Write-Output "SOURCE1_EXIT=$($source1.ExitCode)"
$source2.WaitForExit()
Write-Output "SOURCE2_EXIT=$($source2.ExitCode)"
exit $cli.ExitCode
```

## VLC原生快照命令

```powershell
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 41664){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
$s=$c.GetStream()
$b=[Text.Encoding]::ASCII.GetBytes("snapshot`n")
$s.Write($b,0,$b.Length)
$s.Flush()
$c.Dispose(); Start-Sleep -Seconds 2; Get-ChildItem 'D:/Code/MyCode/MediaTranscode/out/acceptance' -Filter 'composition-r363-native-*.png' | Select-Object FullName,Length
```


## 本轮收尾结果

CLI 37824 end_reason=caller_stop，退出0；三个初始FFmpeg源退出0。重入日志记录3600帧、120秒，但本轮未回收其工具会话退出码，不能补写exit0。18:19:00 VLC原生快照已实际查看，全黑。重入期间未取得画面快照，因此本轮证据不完整，不标记完整验收通过。普通停止不证明 populated reservation 取消路径已覆盖；noncadence、完整G和RKMPP仍未验证。

### 最终运行证据
`	ext
[2026-10-26 18:19:09.167] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 produced_datagrams=190581 produced_ts_payload_bytes=206733448 pending_bytes=0 peak_pending_bytes=110920 access_units=31978 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=32789115 maximum_mux_input_after_emit_ns=37743215 maximum_mux_input_after_dispatch_ns=-62256785 worst_mux_input_ready_ns=95722722800 worst_mux_input_emit_ns=95684979585 worst_mux_input_dispatch_ns=95784979585 maximum_protocol_batch_produced_after_release_ns=36949792 worst_protocol_batch_produced_ns=77721386600 worst_protocol_batch_release_ns=77684436808 worst_protocol_batch_deadline_ns=77751103476 worst_protocol_batch_datagrams=1 exit_reason=completed
[2026-10-26 18:19:09.381] [info] [graph][runtime.node] continuous_audio_aggregate.summary reason=stop output_group=composition-r363.output audio_commits=18848 real_samples=10243072 silence_samples=9057280
[2026-10-26 18:19:09.381] [info] [graph][runtime.lifecycle] video_encode.zero_copy_summary drm_prime=0 software_frame=0
[2026-10-26 18:19:09.383] [info] [graph][runtime.node] continuous_aggregate.summary reason=stop output_group=composition-r363.output output_generation=1 video_commits=13130 source[0].group=composition-r363.source.0 source[0].real_frames=3416 source[0].black_frames=9714 source[0].contribution_generation=0 source[1].group=composition-r363.source.1 source[1].real_frames=3416 source[1].black_frames=9714 source[1].contribution_generation=0 source[2].group=composition-r363.source.2 source[2].real_frames=6968 source[2].black_frames=6162 source[2].contribution_generation=0
[2026-10-26 18:19:09.386] [info] [graph][runtime.lifecycle] video_filter.zero_copy_summary drm_prime_input=0 drm_prime_output=0 rga=0 software_frame=0
[2026-10-26 18:19:09.390] [info] [graph][runtime.lifecycle] video_filter.zero_copy_summary drm_prime_input=0 drm_prime_output=0 rga=0 software_frame=0
[2026-10-26 18:19:09.395] [info] [graph][runtime.lifecycle] video_filter.zero_copy_summary drm_prime_input=0 drm_prime_output=0 rga=0 software_frame=0
[2026-10-26 18:19:09.404] [info] [graph][runtime.node] hardware_transfer.summary forwarded=3608 download=0 upload=0
[2026-10-26 18:19:09.404] [info] [graph][runtime.node] hardware_transfer.summary forwarded=40 download=0 upload=0
[2026-10-26 18:19:09.404] [info] [graph][runtime.node] hardware_transfer.summary forwarded=41 download=0 upload=0
[2026-10-26 18:19:09.405] [info] [graph][runtime.lifecycle] video_decode.zero_copy_summary drm_prime=0 software_frame=0
[2026-10-26 18:19:09.421] [info] [graph][runtime.lifecycle] video_decode.zero_copy_summary drm_prime=0 software_frame=0
[2026-10-26 18:19:09.438] [info] [graph][runtime.lifecycle] video_decode.zero_copy_summary drm_prime=0 software_frame=0
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=934, threads=81, processThreads=129, workers=0, workerProgress=687122, workerProcessCalls=2591385, workerWaits=1904246, workerWakeups=1870978, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=1339, logicalProcessors=22, averageSystemMachineCpuPercent=18.667886, averageProcessMachineCpuPercent=1.841735, peakProcessMachineCpuPercent=8.267717, averageProcessSingleCoreCpuPercent=40.518167, peakProcessSingleCoreCpuPercent=181.889764, initialWorkingSetBytes=209154048, workingSetBytes=266223616, peakWorkingSetBytes=274460672, totalPushed=580414, totalPopped=580378, droppedBuffers=0, encodedPacketsPushed=146766, encodedPacketsPopped=146766, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=16159704, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=760, graphPayloadReservations=256922, graphPayloadReleases=256922, graphPayloadPressureFailures=0, lastInputReceivedAtNs=34236508937700
` 
### FFmpeg警告
`	ext
[in @ 0000025b2d575400] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000025b2d575400] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 1266.697922
[in @ 0000025b2dda10c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000025b2dda10c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 44281.556278
[in @ 0000025b32da6a40] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000025b32da6a40] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 5008.636367
[in @ 0000025b32fc9a00] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000025b32fc9c40] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000025b32fc9c40] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 1266.697922
[in @ 0000025b32fc9a00] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 5008.636367
[in @ 0000025b32fcb180] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000025b32fcb180] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 44281.556278
[aac @ 0000025b2dcb88c0] Qavg: 31092.389

` 
### VLC错误与警告
`	ext
faad warning: decoded zero sample
main error: buffer deadlock prevented
main warning: playback too late (71230): up-sampling
d3d11va warning: surface dimensions (1280x768) differ from avcodec dimensions (1280x736)
d3d11va warning: not enough decoding slices in the texture (6/28)
main error: Failed to create video converter
main error: write error: No error
main error: read error: No error
main error: Failed to create video converter
main error: read error: No error
main error: Failed to create video converter
main error: read error: No error
main warning: buffer too late (-372126 us): dropped
main error: read error: No error
main warning: picture is too late to be displayed (missing 698 ms)
main warning: picture is too late to be displayed (missing 666 ms)
main warning: picture is too late to be displayed (missing 632 ms)
main warning: picture is too late to be displayed (missing 599 ms)
main warning: picture is too late to be displayed (missing 566 ms)
direct3d11 error: SetThumbNailClip failed: 0x800706f4
` 
### 本轮清理清单
- composition-r363-cli-error.log (1964 bytes)
- composition-r363-cli.log (1541152 bytes)
- composition-r363-monitor.log (31557 bytes)
- composition-r363-native-2026-10-26-18h13m03s988.png (825360 bytes)
- composition-r363-native-2026-10-26-18h14m01s622.png (12152 bytes)
- composition-r363-native-2026-10-26-18h19m00s046.png (12152 bytes)
- composition-r363-reentry-stdout.log (540 bytes)
- composition-r363-reentry.log (29521 bytes)
- composition-r363-source0-stdout.log (540 bytes)
- composition-r363-source0.log (29521 bytes)
- composition-r363-source1-stdout.log (335 bytes)
- composition-r363-source1.log (28926 bytes)
- composition-r363-source2-stdout.log (335 bytes)
- composition-r363-source2.log (28926 bytes)
- composition-r363-vlc.log (39123 bytes)## 重入命令

```powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r363-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r363-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($source0.Id)"
$source0.WaitForExit()
Write-Output "REENTRY_EXIT=$($source0.ExitCode)"
exit $source0.ExitCode
```

## 停止命令

```powershell
$target=Get-CimInstance Win32_Process -Filter "ProcessId=37824"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r363*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R363Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R363Console]::FreeConsole() | Out-Null
if(-not [R363Console]::AttachConsole(37824)){throw 'AttachConsole failed'}
try {
[R363Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R363Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 37824 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R363Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R363Console]::FreeConsole() | Out-Null}
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 41664){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020);$s=$c.GetStream();$b=[Text.Encoding]::ASCII.GetBytes("quit`n");$s.Write($b,0,$b.Length);$c.Dispose()
```

外部采样：count=429，CPU均值=1.83797079738327%，CPU峰值=5.26145403479%，WS峰值=275353600，private峰值=1262923776。

已按上述15项清单删除本轮临时产物，保留指定120秒源与构建产物。

清理复查：VLC退出尾部刷新重建日志，当前39123字节；进程核验已退出，补存错误后再次删除。
`	ext
faad warning: decoded zero sample
main error: buffer deadlock prevented
main warning: playback too late (71230): up-sampling
d3d11va warning: surface dimensions (1280x768) differ from avcodec dimensions (1280x736)
d3d11va warning: not enough decoding slices in the texture (6/28)
main error: Failed to create video converter
main error: write error: No error
main error: read error: No error
main error: Failed to create video converter
main error: read error: No error
main error: Failed to create video converter
main error: read error: No error
main warning: buffer too late (-372126 us): dropped
main error: read error: No error
main warning: picture is too late to be displayed (missing 698 ms)
main warning: picture is too late to be displayed (missing 666 ms)
main warning: picture is too late to be displayed (missing 632 ms)
main warning: picture is too late to be displayed (missing 599 ms)
main warning: picture is too late to be displayed (missing 566 ms)
direct3d11 error: SetThumbNailClip failed: 0x800706f4
` 
