# r289：三源合屏 MPEG-TS/UDP 复验

本轮实流结束，整体门禁未通过，证据与限制如下。Release全量重建configure/build=0（732项），四项产物齐全，realtime SHA256 EB66B6E533394BD3522985222BA76B16C1EB4D89CBE73E2CC18C342ED340B834；NTP校验修复双审PASS。源PID32600、37960、25076；CLI2224。仅realtime。

## 实际启动命令

```powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r289-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r289-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r289-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r289-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r289-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r289-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r289 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62020 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r289-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r289-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r289-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r289-native- --snapshot-format=png udp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r289-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

重入PID19444，VLC PID41936。四次快照执行同一RC命令。

```powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r289-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r289-reentry.log' -PassThru
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
$target=Get-CimInstance Win32_Process -Filter "ProcessId=2224"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r289*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R289Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R289Console]::FreeConsole() | Out-Null
if(-not [R289Console]::AttachConsole(2224)){throw 'AttachConsole failed'}
try {
[R289Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R289Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 2224 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R289Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R289Console]::FreeConsole() | Out-Null}


```

```powershell
$p=Get-CimInstance Win32_Process -Filter 'ProcessId=41936'; if($p){if($p.Name -ne 'vlc.exe' -or $p.CommandLine -notlike '*composition-r289*'){throw 'VLC identity mismatch'}; $client=[Net.Sockets.TcpClient]::new('127.0.0.1',63020); try{$stream=$client.GetStream(); $bytes=[Text.Encoding]::ASCII.GetBytes("quit`n"); $stream.Write($bytes,0,$bytes.Length)}finally{$client.Dispose()}}
```

## 实测结果与限制

初始三源及重入源均完整120秒/3600帧、exit=0。四张VLC原生快照逐张查看：14:28:46三格、14:30:48全黑、14:31:30左下重入、14:33:14全黑。源全部结束后CLI仍黑屏输出；观测完成后调用方CtrlC，CLI exit=0/end_reason=caller_stop，不是自然EOF。VLC按本轮精确PID核验后RC退出。

内部filtered phase按generation列出(样本数,最小ns,最大ns)：{'1': (113, 9071, 9071), '2': (117, 13606, 13606)}；不是接收端持续A/V证据。

原r288的UDP无条件NTP校验启动失败已在同规格复验中消失。整体门禁仍未通过：delivery/接收端A/V、global WouldBlock分支覆盖未证明，MPEGTS总体资源上界、固定元数据及完整Windows/RKMPP矩阵未完成；不作成功验收提交。

### runtime与sender原文

~~~
[2026-10-21 14:33:38.000] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=34299 committed_datagrams=177644 committed_payload_bytes=204396232 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=79296 pacing_reserved=177644 pacing_cancelled=0 pacing_submitted=177644 pacing_maximum_submit_lateness_ns=16298860 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=79296 backlog_max_residence_ns=65942600 last_materialized_sequence=177644 last_scheduled_sequence=177644 last_submitted_sequence=177644 last_committed_sequence=177644 shared_scope_charged_datagrams=177644 shared_scope_charged_wire_bytes=209370264 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2154 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2154 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=188010 endpoint_1_target_effective_socket_bytes=188010 endpoint_1_api_requested_socket_bytes=188010 endpoint_1_effective_socket_bytes=188010 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=177644 endpoint_1_committed_payload_bytes=204396232
[2026-10-21 14:33:38.214] [info] [graph][runtime.node] mpegts_wire_materializer stage=stopped maximum_materialized_after_release_ns=54488700 worst_materialized_at_ns=227210556300 worst_release_ns=227156067600 worst_deadline_ns=227222734268 worst_global_sequence=134204
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=909, threads=81, processThreads=130, workers=0, workerProgress=579725, workerProcessCalls=2169463, workerWaits=1589721, workerWakeups=1565443, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=1005, logicalProcessors=22, averageSystemMachineCpuPercent=8.808531, averageProcessMachineCpuPercent=1.603738, peakProcessMachineCpuPercent=6.423983, averageProcessSingleCoreCpuPercent=35.282241, peakProcessSingleCoreCpuPercent=141.327623, initialWorkingSetBytes=207867904, workingSetBytes=262668288, peakWorkingSetBytes=272867328, totalPushed=526039, totalPopped=526003, droppedBuffers=0, encodedPacketsPushed=106407, encodedPacketsPopped=106407, backpressureItems=122, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=17055840, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=738, graphPayloadReservations=261524, graphPayloadReleases=261524, graphPayloadPressureFailures=0, lastInputReceivedAtNs=20959523538700
~~~

### stderr与VLC错误

~~~
[in @ 000001c15ca71b00] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001c15ca71b00] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 46228.7895
[in @ 000001c15ca71380] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001c15ca71380] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 38614.870333
[in @ 000001c161d8eec0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001c161d8eec0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 2748.950856
[in @ 000001c14def5dc0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001c14def5dc0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 2748.950856
[in @ 000001c161a20040] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001c167ac10c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001c167ac10c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 46228.7895
[in @ 000001c161a20040] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 38614.870333
[aac @ 000001c161ca8140] Qavg: 17980.879

main error: buffer deadlock prevented
main error: Failed to create video converter
main error: Failed to create video converter
main error: Failed to create video converter
main error: Failed to create video converter
main error: read error: No error
direct3d11 error: SetThumbNailClip failed: 0x800706f4
~~~

### 源最终记录

~~~
source0
[out#0/rtp @ 00000239ebefc3c0] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
[out#1/rtp @ 00000239edc5ddc0] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
source1
frame= 3590 fps= 30 q=-1.0 size=  118545KiB time=00:01:59.66 bitrate=8115.2kbits/s speed=   1x elapsed=0:01:59.15    
[out#0/rtp @ 0000022d15468400] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
source2
frame= 3590 fps= 30 q=-1.0 size=  118545KiB time=00:01:59.66 bitrate=8115.2kbits/s speed=   1x elapsed=0:01:59.15    
[out#0/rtp @ 0000016649a06fc0] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
reentry
[out#0/rtp @ 000002e1516a6580] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
[out#1/rtp @ 000002e15173a240] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.49    
~~~

### 本轮精确清理清单

- composition-r289-cli-error.log
- composition-r289-cli.log
- composition-r289-monitor.log
- composition-r289-native-2026-10-21-14h28m46s908.png
- composition-r289-native-2026-10-21-14h30m48s242.png
- composition-r289-native-2026-10-21-14h31m30s553.png
- composition-r289-native-2026-10-21-14h33m14s214.png
- composition-r289-reentry-stdout.log
- composition-r289-reentry.log
- composition-r289-source0-stdout.log
- composition-r289-source0.log
- composition-r289-source1-stdout.log
- composition-r289-source1.log
- composition-r289-source2-stdout.log
- composition-r289-source2.log
- composition-r289-vlc.log

16项临时产物已按清单逐项删除，r289残留0，六个精确PID已核验退出；指定连续源124427809字节保留。无本轮远程测试产物。
