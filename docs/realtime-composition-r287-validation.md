# r287：三源 RTP 合屏复验

## 状态

本轮实流已结束，整体门禁未通过，证据与限制见下文。Release configure/build=0（731项），四项产物齐全，realtime SHA256 7EDC6A2B9CEA116F300196B622FC2A3F843134DE4953A44FC1F6723EFE999F3F。双独立审查通过终止 prepared owner 与 raw 容量切片。仅运行 realtime CLI。

初始源 PID 40216、38252、40220；CLI PID 32484。

## 实际启动命令

```powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r287-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r287-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r287-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r287-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r287-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r287-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r287 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout separate --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r287-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r287-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r287-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10 -and (Test-Path -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r287-output.sdp')) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r287-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r287-native- --snapshot-format=png file:///D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r287-output.sdp' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r287-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

## 实际重入与控制命令

重入 PID 32332；VLC PID 16832。四次快照均执行下列同一 RC 命令。

```powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r287-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r287-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($source0.Id)"
$source0.WaitForExit()
Write-Output "REENTRY_EXIT=$($source0.ExitCode)"
exit $source0.ExitCode
```

```powershell
$client=[Net.Sockets.TcpClient]::new('127.0.0.1',63020); try {$stream=$client.GetStream(); $stream.ReadTimeout=2000; $bytes=[Text.Encoding]::ASCII.GetBytes("status`nsnapshot`n"); $stream.Write($bytes,0,$bytes.Length); $buffer=New-Object byte[] 8192; try {while(($count=$stream.Read($buffer,0,$buffer.Length)) -gt 0){[Text.Encoding]::UTF8.GetString($buffer,0,$count)}} catch {Write-Output 'RC read complete'}} finally {$client.Dispose()}
```

```powershell
$ErrorActionPreference='Stop'
$target=Get-CimInstance Win32_Process -Filter "ProcessId=32484"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r287*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R287Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R287Console]::FreeConsole() | Out-Null
if(-not [R287Console]::AttachConsole(32484)){throw 'AttachConsole failed'}
try {
[R287Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R287Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 32484 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R287Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R287Console]::FreeConsole() | Out-Null}


```

```powershell
$p=Get-CimInstance Win32_Process -Filter 'ProcessId=16832'; if($p){if($p.Name -ne 'vlc.exe' -or $p.CommandLine -notlike '*composition-r287*'){throw 'VLC identity mismatch'}; $client=[Net.Sockets.TcpClient]::new('127.0.0.1',63020); try{$stream=$client.GetStream(); $bytes=[Text.Encoding]::ASCII.GetBytes("quit`n"); $stream.Write($bytes,0,$bytes.Length)}finally{$client.Dispose()}}
```

## 实测结果与限制

初始三源及重入源均完整120秒/3600帧，exit=0。VLC原生快照逐张查看：14:03:45三格、14:05:43全黑、14:06:46左下重入、14:08:51全黑。全部源结束后CLI继续黑屏输出；观测结束调用方CtrlC，CLI exit=0/end_reason=caller_stop，不能记为自然EOF退出。VLC按精确PID核验后RC退出。

errors/workerErrors=0；媒体额度最终0字节/0对象，预约与释放均267130。graphPayloadPressureFailures=1：MediaGraphPayloadCreditLedger.cpp仅在新需求登记waiter后递增并返回WouldBlock，不是硬失败；现有聚合日志不能定位具体producer，也不能证明网络owner重试。

内部filtered phase按generation列出(样本数,最小ns,最大ns)：{'1': (113, 20409, 20409), '2': (117, 13605, 13605)}；不是接收端持续A/V证据。

整体门禁未通过：delivery/接收端A/V、terminal/global WouldBlock分支覆盖未证明；MPEGTS总体存储上界、UDP owner、固定元数据及完整Windows/RKMPP矩阵未完成。VLC错误保留如下，不作成功验收提交。

### 最终runtime与sender原文

~~~
[2026-10-21 14:09:14.147] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=25727 committed_datagrams=158722 committed_payload_bytes=196817845 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=53 ordered_queue_maximum_wire_bytes=78147 pacing_reserved=158722 pacing_cancelled=0 pacing_submitted=158722 pacing_maximum_submit_lateness_ns=44587840 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=76 backlog_high_water_wire_bytes=111710 backlog_max_residence_ns=68172300 last_materialized_sequence=158722 last_scheduled_sequence=158722 last_submitted_sequence=158722 last_committed_sequence=158722 shared_scope_charged_datagrams=158722 shared_scope_charged_wire_bytes=201262061 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=3805 evidence_timestamp_tracked=0 evidence_timestamp_untracked=3805 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=756580 endpoint_1_target_effective_socket_bytes=189145 endpoint_1_api_requested_socket_bytes=189145 endpoint_1_effective_socket_bytes=189145 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=143393 endpoint_1_committed_payload_bytes=190933593 endpoint_2_target_effective_socket_bytes=189145 endpoint_2_api_requested_socket_bytes=189145 endpoint_2_effective_socket_bytes=189145 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=85 endpoint_2_committed_payload_bytes=5788 endpoint_3_target_effective_socket_bytes=189145 endpoint_3_api_requested_socket_bytes=189145 endpoint_3_effective_socket_bytes=189145 endpoint_3_timestamp_source=0 endpoint_3_committed_datagrams=15159 endpoint_3_committed_payload_bytes=5872676 endpoint_4_target_effective_socket_bytes=189145 endpoint_4_api_requested_socket_bytes=189145 endpoint_4_effective_socket_bytes=189145 endpoint_4_timestamp_source=0 endpoint_4_committed_datagrams=85 endpoint_4_committed_payload_bytes=5788
[CLI] final runtime report: queued=0, peakQueued=925, threads=80, processThreads=129, workers=0, workerProgress=515545, workerProcessCalls=2056374, workerWaits=1540813, workerWakeups=1515038, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=1114, logicalProcessors=22, averageSystemMachineCpuPercent=9.167795, averageProcessMachineCpuPercent=1.505369, peakProcessMachineCpuPercent=6.637168, averageProcessSingleCoreCpuPercent=33.118117, peakProcessSingleCoreCpuPercent=146.017699, initialWorkingSetBytes=208187392, workingSetBytes=261840896, peakWorkingSetBytes=270176256, totalPushed=471653, totalPopped=471617, droppedBuffers=0, encodedPacketsPushed=92322, encodedPacketsPopped=92322, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=17080417, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=734, graphPayloadReservations=267130, graphPayloadReleases=267130, graphPayloadPressureFailures=1, lastInputReceivedAtNs=19485801813800
~~~

### stderr与VLC错误

~~~
[in @ 00000170e578db00] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000170e578db00] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 32139.286756
[in @ 00000170e578e040] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000170e578e040] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 20479.285356
[in @ 00000170f0305000] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000170f0305000] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 14439.7845
[in @ 00000170c35a9ec0] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000170e5a8b240] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000170e5a8b240] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 14439.7845
[in @ 00000170c35a9ec0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 20479.285356
[in @ 00000170d6a61080] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000170d6a61080] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 32139.286756
[aac @ 00000170eb266540] Qavg: 22657.379

main error: buffer deadlock prevented
main error: Failed to create video converter
main error: Failed to create video converter
main error: Failed to create video converter
main error: Failed to create video converter
direct3d11 error: SetThumbNailClip failed: 0x800706f4
~~~

### 源最终记录

~~~
source0
[out#0/rtp @ 000002bfd6058980] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
[out#1/rtp @ 000002bfd7ce3640] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
source1
frame= 3590 fps= 30 q=-1.0 size=  118545KiB time=00:01:59.66 bitrate=8115.2kbits/s speed=   1x elapsed=0:01:59.17    
[out#0/rtp @ 00000199409f7840] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.47    
source2
frame= 3593 fps= 30 q=-1.0 size=  118601KiB time=00:01:59.76 bitrate=8112.3kbits/s speed=   1x elapsed=0:01:59.24    
[out#0/rtp @ 000001fd19d9cd00] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.47    
reentry
[out#0/rtp @ 0000018d1b7d7340] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
[out#1/rtp @ 0000018d1d449700] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.49    
~~~

### 本轮精确清理清单

- composition-r287-cli-error.log
- composition-r287-cli.log
- composition-r287-monitor.log
- composition-r287-native-2026-10-21-14h03m45s879.png
- composition-r287-native-2026-10-21-14h05m43s956.png
- composition-r287-native-2026-10-21-14h06m46s261.png
- composition-r287-native-2026-10-21-14h08m51s726.png
- composition-r287-output.sdp
- composition-r287-reentry-stdout.log
- composition-r287-reentry.log
- composition-r287-source0-stdout.log
- composition-r287-source0.log
- composition-r287-source1-stdout.log
- composition-r287-source1.log
- composition-r287-source2-stdout.log
- composition-r287-source2.log
- composition-r287-vlc.log

以上17项已按清单逐项删除，r287残留0；六个精确PID均已核验退出。指定连续源124427809字节保留。本轮未运行远程测试，无本轮远程产物。
