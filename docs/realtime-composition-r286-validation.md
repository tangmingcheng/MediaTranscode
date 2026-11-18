# r286：raw RTP 联合存储预约实流复验

状态：已实际执行并完成本轮功能观察；整体验收门禁未通过，delivery_evidence=not_proven。临时产物归档后按清单清理。

规格：三个 H.264 RTP 输入，1280×720、30fps、约8Mbps；选源2附带AAC 44100Hz双声道。合屏输出HEVC 1280×720、30fps、8Mbps CBR及AAC 192kbps，separate RTP。统一120秒连续源，选源重入另完整120秒；VLC原生快照。

## 启动命令

```powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r286-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r286-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r286-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r286-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r286-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r286-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r286 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout separate --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r286-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r286-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r286-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10 -and (Test-Path -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r286-output.sdp')) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r286-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r286-native- --snapshot-format=png file:///D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r286-output.sdp' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r286-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

## 所选源重入命令（已执行）

```powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r286-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r286-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($source0.Id)"
$source0.WaitForExit()
Write-Output "REENTRY_EXIT=$($source0.ExitCode)"
exit $source0.ExitCode
```


## 执行结果

Release全量重建configure/build=0，731项，四项必需产物齐全；realtime SHA256 4227AAB23454121B9358A6DA4C2B699C08825AB285F08B37D8A135EA0E645D82。两名未参与实现的独立审查者对本轮raw RTP C+W接线均明确PASS，仅为源码审查。

本轮PID：初始源35748/31912/39712；CLI38988；VLC36984；重入源6308。初始三个源及重入源均完整120秒、3600帧、退出0。CLI调用方CtrlC停止，end_reason=caller_stop，退出0；不记为自然EOF。六个精确PID已通过Win32_Process复核，残留0。

VLC原生快照已逐张查看：13:39:09.027三格正常；13:40:48.868全黑；13:41:44.809左下所选源重入；13:43:26.403再次全黑。没有系统截图。

最终引擎errors/workerErrors、graphPayloadPressureFailures、sender deadline_misses均0；媒体额度最终0字节/0对象，预约/释放均258997。CPU机器平均1.696645%、峰值7.399103%；工作集初始210034688、最终262508544、峰值271843328字节。媒体额度高水位17080050字节/743对象。内部phase：gen1共113点13605..13607ns，gen2共117点13605..13606ns；这些不是接收端A/V证据。

sender最大submit lateness=65539840ns，delivery_evidence=not_proven，evidence_observed=0；未见late_output_commit。不能证明全部global/root WouldBlock分支实际触发，也不能用媒体额度计数代替网络root逐项存储观测。仍须补完整Windows矩阵、接收端音画同步/发送证据和RKMPP。VLC转换器错误及FFmpeg色域警告完整保留下列诊断，不标记整体验收通过。

### 最终报告与必要诊断

~~~text
[2026-10-21 13:38:26.373] [info] [graph][planner.capability] composition_preparation_storage stage=graph_bound scope=shared_reservations capacity=2147483648 retained=848052775 high_water=874176529
[2026-10-21 13:43:43.131] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=23140 committed_datagrams=156249 committed_payload_bytes=196942817 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=53 ordered_queue_maximum_wire_bytes=78147 pacing_reserved=156249 pacing_cancelled=0 pacing_submitted=156249 pacing_maximum_submit_lateness_ns=65539840 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=76 backlog_high_water_wire_bytes=111710 backlog_max_residence_ns=46352400 last_materialized_sequence=156249 last_scheduled_sequence=156249 last_submitted_sequence=156249 last_committed_sequence=156249 shared_scope_charged_datagrams=156249 shared_scope_charged_wire_bytes=201317789 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=3805 evidence_timestamp_tracked=0 evidence_timestamp_untracked=3805 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=756580 endpoint_1_target_effective_socket_bytes=189145 endpoint_1_api_requested_socket_bytes=189145 endpoint_1_effective_socket_bytes=189145 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=142465 endpoint_1_committed_payload_bytes=191093649 endpoint_2_target_effective_socket_bytes=189145 endpoint_2_api_requested_socket_bytes=189145 endpoint_2_effective_socket_bytes=189145 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=74 endpoint_2_committed_payload_bytes=5040 endpoint_3_target_effective_socket_bytes=189145 endpoint_3_api_requested_socket_bytes=189145 endpoint_3_effective_socket_bytes=189145 endpoint_3_timestamp_source=0 endpoint_3_committed_datagrams=13634 endpoint_3_committed_payload_bytes=5838952 endpoint_4_target_effective_socket_bytes=189145 endpoint_4_api_requested_socket_bytes=189145 endpoint_4_effective_socket_bytes=189145 endpoint_4_timestamp_source=0 endpoint_4_committed_datagrams=76 endpoint_4_committed_payload_bytes=5176
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=962, threads=80, processThreads=129, workers=0, workerProgress=498280, workerProcessCalls=1948896, workerWaits=1450600, workerWakeups=1427406, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=1006, logicalProcessors=22, averageSystemMachineCpuPercent=13.987693, averageProcessMachineCpuPercent=1.696645, peakProcessMachineCpuPercent=7.399103, averageProcessSingleCoreCpuPercent=37.326199, peakProcessSingleCoreCpuPercent=162.780269, initialWorkingSetBytes=210034688, workingSetBytes=262508544, peakWorkingSetBytes=271843328, totalPushed=456353, totalPopped=456317, droppedBuffers=0, encodedPacketsPushed=83036, encodedPacketsPopped=83036, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=17080050, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=743, graphPayloadReservations=258997, graphPayloadReleases=258997, graphPayloadPressureFailures=0, lastInputReceivedAtNs=17973110018300
[in @ 000001d0c14c33c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001d0c14c33c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 46901.7985
[in @ 000001d0c15547c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001d0c15547c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 46336.455422
[in @ 000001d0c1e99c40] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001d0c1e99c40] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 19906.933878
[in @ 000001d0c187f940] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001d0c187f940] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 46336.455422
[in @ 000001d0c2091900] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001d0c2091900] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 19906.933878
[in @ 000001d0c2090b00] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001d0c2090b00] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 46901.7985
[aac @ 000001d0bd486800] Qavg: 17877.311
main error: buffer deadlock prevented
main error: Failed to create video converter
main error: Failed to create video converter
main error: Failed to create video converter
main error: Failed to create video converter
direct3d11 error: SetThumbNailClip failed: 0x800706f4
SOURCE source0
[out#0/rtp @ 000002321992a640] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
[out#1/rtp @ 000002321b4ca800] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
SOURCE source1
frame= 3591 fps= 30 q=-1.0 size=  118561KiB time=00:01:59.70 bitrate=8114.0kbits/s speed=   1x elapsed=0:01:59.20    
[out#0/rtp @ 000002a753cd6d00] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.46    
SOURCE source2
frame= 3591 fps= 30 q=-1.0 size=  118561KiB time=00:01:59.70 bitrate=8114.0kbits/s speed=   1x elapsed=0:01:59.19    
[out#0/rtp @ 000002270617c2c0] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.47    
SOURCE reentry
[out#0/rtp @ 000002858e43aa40] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
[out#1/rtp @ 000002859013c480] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
~~~


## 实际控制命令

以下命令均已执行。四次快照均使用相同RC命令；停止前核对CLI身份与独占控制台，VLC退出前核对PID与本轮命令行。

```powershell
$client=[Net.Sockets.TcpClient]::new('127.0.0.1',63020); try {$stream=$client.GetStream(); $stream.ReadTimeout=2000; $bytes=[Text.Encoding]::ASCII.GetBytes("status`nsnapshot`n"); $stream.Write($bytes,0,$bytes.Length); $buffer=New-Object byte[] 8192; try {while(($count=$stream.Read($buffer,0,$buffer.Length)) -gt 0){[Text.Encoding]::UTF8.GetString($buffer,0,$count)}} catch {Write-Output 'RC read complete'}} finally {$client.Dispose()}
```

```powershell
$ErrorActionPreference='Stop'
$target=Get-CimInstance Win32_Process -Filter "ProcessId=38988"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r286*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R286Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R286Console]::FreeConsole() | Out-Null
if(-not [R286Console]::AttachConsole(38988)){throw 'AttachConsole failed'}
try {
[R286Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R286Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 38988 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R286Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R286Console]::FreeConsole() | Out-Null}


```

```powershell
$p=Get-CimInstance Win32_Process -Filter 'ProcessId=36984'; if($p){if($p.Name -ne 'vlc.exe' -or $p.CommandLine -notlike '*composition-r286*'){throw 'VLC identity mismatch'}; $client=[Net.Sockets.TcpClient]::new('127.0.0.1',63020); try{$stream=$client.GetStream(); $bytes=[Text.Encoding]::ASCII.GetBytes("quit`n"); $stream.Write($bytes,0,$bytes.Length)}finally{$client.Dispose()}}
```

### 本轮清理清单

- composition-r286-cli-error.log
- composition-r286-cli.log
- composition-r286-monitor.log
- composition-r286-native-2026-10-21-13h39m09s027.png
- composition-r286-native-2026-10-21-13h40m48s868.png
- composition-r286-native-2026-10-21-13h41m44s809.png
- composition-r286-native-2026-10-21-13h43m26s403.png
- composition-r286-output.sdp
- composition-r286-reentry-stdout.log
- composition-r286-reentry.log
- composition-r286-source0-stdout.log
- composition-r286-source0.log
- composition-r286-source1-stdout.log
- composition-r286-source1.log
- composition-r286-source2-stdout.log
- composition-r286-source2.log
- composition-r286-vlc.log

以上17项已按清单逐项删除，r286文件残留0；指定连续源124427809字节保留。本轮未运行远程测试，无本轮远程产物。六个精确PID均已核验退出。
