# r364 Windows Release realtime 验证

已结束，cadence回归证据已收集，不判完整目标通过。产物6593536 bytes、2026-10-26 18:27:31；全量Release49648 configure/build退出0（755项）。端口类型化切片双独立源码PASS，不代表完整G。初始源46540/31444/10412，CLI14540，主会话35275。保留原测试参数，local CLI未执行。

## 实际启动命令

```powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r364-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r364-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r364-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r364-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r364-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r364-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r364 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r364-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r364-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r364-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r364-native- --snapshot-format=png udp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r364-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

VLC PID30340；18:32:23原生快照已实际查看三格，当前仍在测试。

## VLC原生快照命令
```powershell
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 30340){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
$s=$c.GetStream()
$b=[Text.Encoding]::ASCII.GetBytes("snapshot`n")
$s.Write($b,0,$b.Length)
$s.Flush()
$c.Dispose(); Start-Sleep -Seconds 2; Get-ChildItem 'D:/Code/MyCode/MediaTranscode/out/acceptance' -Filter 'composition-r364-native-*.png' | Select-Object FullName,Length
```

18:34:01原生快照已实际查看全黑；重入PID42056、会话77537，18:34:35原生快照已查看左下格恢复，其余格黑，generation2漂移telemetry持续输出。

## 重入命令
```powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r364-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r364-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($source0.Id)"
$source0.WaitForExit()
Write-Output "REENTRY_EXIT=$($source0.ExitCode)"
exit $source0.ExitCode
```

## 停止命令
```powershell
$target=Get-CimInstance Win32_Process -Filter "ProcessId=14540"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r364*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R364Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R364Console]::FreeConsole() | Out-Null
if(-not [R364Console]::AttachConsole(14540)){throw 'AttachConsole failed'}
try {
[R364Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R364Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 14540 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R364Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R364Console]::FreeConsole() | Out-Null}
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 30340){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020);$s=$c.GetStream();$b=[Text.Encoding]::ASCII.GetBytes("quit`n");$s.Write($b,0,$b.Length);$c.Dispose()
```

## 收尾结论

四张原生快照均实际查看：三格→全黑→左下恢复→全黑（最后18:36:26）。重入会话77537退出0；主会话35275返回CLI/三初始源均退出0。end_reason=caller_stop，不是源结束自动退出。六精确PID已核验不存在。此为cadence回归证据，完整G、noncadence、populated reservation取消、内容级A/V及RKMPP仍未闭合；VLC存在错误/警告，不计完整目标验收通过，亦不证明历史间歇故障修复。

外部采样count=284，CPU均值=2.06728925947182%，CPU峰值=4.2270916248082%，WS峰值=269848576，private峰值=1270476800；内部raw_phase绝对最大=15874ns，仅为内部漂移证据。

### 运行证据
`	ext
[2026-10-26 18:36:39.008] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=31157 committed_datagrams=174367 committed_payload_bytes=203633516 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=79296 pacing_reserved=174367 pacing_cancelled=0 pacing_submitted=174367 pacing_maximum_submit_lateness_ns=7908040 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=79296 backlog_max_residence_ns=51952500 last_materialized_sequence=174367 last_scheduled_sequence=174367 last_submitted_sequence=174367 last_committed_sequence=174367 submitted_service_generation=1 submitted_service_sequence=174367 submitted_service_completed_ns=293781368400 submitted_service_duration_ns=34560 submitted_service_available_ns=293781402960 submitted_scope_available_ns=293781402960 shared_scope_charged_datagrams=174367 shared_scope_charged_wire_bytes=208515792 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2154 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2154 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=188010 endpoint_1_target_effective_socket_bytes=188010 endpoint_1_api_requested_socket_bytes=188010 endpoint_1_effective_socket_bytes=188010 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=174367 endpoint_1_committed_payload_bytes=203633516
[2026-10-26 18:36:39.218] [info] [graph][runtime.node] continuous_audio_aggregate.summary reason=stop output_group=composition-r364.output audio_commits=12413 real_samples=10248192 silence_samples=2462720
[2026-10-26 18:36:39.218] [info] [graph][runtime.lifecycle] video_encode.zero_copy_summary drm_prime=0 software_frame=0
[2026-10-26 18:36:39.219] [info] [graph][runtime.node] continuous_aggregate.summary reason=stop output_group=composition-r364.output output_generation=1 video_commits=8647 source[0].group=composition-r364.source.0 source[0].real_frames=3418 source[0].black_frames=5229 source[0].contribution_generation=0 source[1].group=composition-r364.source.1 source[1].real_frames=3418 source[1].black_frames=5229 source[1].contribution_generation=0 source[2].group=composition-r364.source.2 source[2].real_frames=6971 source[2].black_frames=1676 source[2].contribution_generation=0
[2026-10-26 18:36:39.221] [info] [graph][runtime.lifecycle] video_filter.zero_copy_summary drm_prime_input=0 drm_prime_output=0 rga=0 software_frame=0
[2026-10-26 18:36:39.227] [info] [graph][runtime.lifecycle] video_filter.zero_copy_summary drm_prime_input=0 drm_prime_output=0 rga=0 software_frame=0
[2026-10-26 18:36:39.232] [info] [graph][runtime.lifecycle] video_filter.zero_copy_summary drm_prime_input=0 drm_prime_output=0 rga=0 software_frame=0
[2026-10-26 18:36:39.240] [info] [graph][runtime.node] hardware_transfer.summary forwarded=3610 download=0 upload=0
[2026-10-26 18:36:39.240] [info] [graph][runtime.node] hardware_transfer.summary forwarded=40 download=0 upload=0
[2026-10-26 18:36:39.240] [info] [graph][runtime.node] hardware_transfer.summary forwarded=42 download=0 upload=0
[2026-10-26 18:36:39.241] [info] [graph][runtime.lifecycle] video_decode.zero_copy_summary drm_prime=0 software_frame=0
[2026-10-26 18:36:39.259] [info] [graph][runtime.lifecycle] video_decode.zero_copy_summary drm_prime=0 software_frame=0
[2026-10-26 18:36:39.281] [info] [graph][runtime.lifecycle] video_decode.zero_copy_summary drm_prime=0 software_frame=0
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=922, threads=81, processThreads=129, workers=0, workerProgress=566290, workerProcessCalls=2277885, workerWaits=1711578, workerWakeups=1689617, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=875, logicalProcessors=22, averageSystemMachineCpuPercent=20.080667, averageProcessMachineCpuPercent=2.088844, peakProcessMachineCpuPercent=5.833333, averageProcessSingleCoreCpuPercent=45.954563, peakProcessSingleCoreCpuPercent=128.333333, initialWorkingSetBytes=210264064, workingSetBytes=259096576, peakWorkingSetBytes=269848576, totalPushed=487221, totalPopped=487185, droppedBuffers=0, encodedPacketsPushed=96659, encodedPacketsPopped=96659, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=15982061, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=747, graphPayloadReservations=223142, graphPayloadReleases=223142, graphPayloadPressureFailures=0, lastInputReceivedAtNs=35434718588900
` 
### 源结尾
`	ext
composition-r364-reentry.log
[out#1/rtp @ 000001ea60ebbfc0] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
composition-r364-source0.log
[out#1/rtp @ 000001c2362a8440] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.49    
composition-r364-source1.log
[out#0/rtp @ 00000211d43a8c80] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
composition-r364-source2.log
[out#0/rtp @ 00000234d3e2c440] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.47    
` 
### CLI stderr
`	ext
[in @ 000002bb71e4bec0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002bb71e4bec0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 46721.4365
[in @ 000002bb7200be80] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002bb7200be80] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 40740.982556
[in @ 000002bb77601fc0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002bb77601fc0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 34154.486733
[in @ 000002bb776ca100] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002bb776ca100] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 46721.4365
[in @ 000002bb776c9200] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002bb776cc600] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002bb776cc600] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 40740.982556
[in @ 000002bb776c9200] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 34154.486733
[aac @ 000002bb77996600] Qavg: 13224.226

` 
### VLC错误与警告
`	ext
faad warning: decoded zero sample
d3d11va warning: surface dimensions (1280x768) differ from avcodec dimensions (1280x736)
d3d11va warning: not enough decoding slices in the texture (6/28)
main error: buffer deadlock prevented
main warning: playback too late (144887): up-sampling
main warning: playback way too late (283510): flushing buffers
main warning: picture is too late to be displayed (missing 101 ms)
main warning: picture is too late to be displayed (missing 68 ms)
main warning: picture is too late to be displayed (missing 34 ms)
main error: Failed to create video converter
main error: write error: No error
main error: read error: No error
main error: Failed to create video converter
main error: read error: No error
main error: Failed to create video converter
main error: read error: No error
main error: Failed to create video converter
main error: read error: No error
main warning: buffer too late (-128561 us): dropped
main warning: picture is too late to be displayed (missing 468 ms)
main error: read error: No error
main warning: picture is too late to be displayed (missing 447 ms)
main warning: picture is too late to be displayed (missing 414 ms)
main warning: picture is too late to be displayed (missing 380 ms)
main warning: picture is too late to be displayed (missing 347 ms)
direct3d11 error: SetThumbNailClip failed: 0x800706f4
` 
### 清理清单
- composition-r364-cli-error.log (1964 bytes)
- composition-r364-cli.log (1087502 bytes)
- composition-r364-monitor.log (20846 bytes)
- composition-r364-native-2026-10-26-18h32m23s722.png (869885 bytes)
- composition-r364-native-2026-10-26-18h34m01s687.png (12152 bytes)
- composition-r364-native-2026-10-26-18h34m35s931.png (280286 bytes)
- composition-r364-native-2026-10-26-18h36m26s556.png (12152 bytes)
- composition-r364-reentry-stdout.log (540 bytes)
- composition-r364-reentry.log (29521 bytes)
- composition-r364-source0-stdout.log (540 bytes)
- composition-r364-source0.log (29521 bytes)
- composition-r364-source1-stdout.log (335 bytes)
- composition-r364-source1.log (28926 bytes)
- composition-r364-source2-stdout.log (335 bytes)
- composition-r364-source2.log (28926 bytes)
- composition-r364-vlc.log (42748 bytes)
本轮16项产物已按清单逐项删除，指定120秒源及构建产物保留。
