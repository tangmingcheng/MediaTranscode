# r280：视频 RTP 逐包写入实流验证

状态：本轮功能观察完成，整体门禁仍未通过；测试产物已清理。Release SHA256 EA52FE8F1A4AC7FA4A1C6237B51712088FD4FB84B4ECB634BEA3BC42B0FEBF45。修复后双独立静态审查 PASS，全量构建 configure/build 均为 0；预算接入尚未完成。

## 实际命令
~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r280-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r280-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r280-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r280-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r280-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r280-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r280 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout separate --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r280-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r280-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r280-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and (Test-Path -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r280-output.sdp')) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r280-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r280-native- --snapshot-format=png D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r280-output.sdp' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r280-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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


~~~

## VLC 显式打开 SDP
~~~powershell
$client=[Net.Sockets.TcpClient]::new('127.0.0.1',63020); try {$stream=$client.GetStream(); $stream.ReadTimeout=2000; $bytes=[Text.Encoding]::ASCII.GetBytes("add file:///D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r280-output.sdp`nplay`n"); $stream.Write($bytes,0,$bytes.Length); $stream.Flush()} finally {$client.Dispose()}
~~~

## VLC 原生快照（各阶段重复执行）
~~~powershell
$client=[Net.Sockets.TcpClient]::new('127.0.0.1',63020); try {$stream=$client.GetStream(); $bytes=[Text.Encoding]::ASCII.GetBytes("snapshot`n"); $stream.Write($bytes,0,$bytes.Length); $stream.Flush()} finally {$client.Dispose()}
~~~

## 所选音视频源重入
~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r280-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r280-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($source0.Id)"
$source0.WaitForExit()
Write-Output "REENTRY_EXIT=$($source0.ExitCode)"
exit $source0.ExitCode
~~~

初始源 PID：32588、21004、15204；CLI PID：31116；VLC PID：28940；重入源 PID：37860。VLC 最初 RC 状态为 stop，后以 add file:///.../composition-r280-output.sdp 显式打开，状态变为 play，原生快照成功；不把最初无快照阶段计为画面通过。

## 调用方停止实际命令
~~~powershell
$ErrorActionPreference='Stop'
$target=Get-CimInstance Win32_Process -Filter "ProcessId=31116"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r280*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R280Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R280Console]::FreeConsole() | Out-Null
if(-not [R280Console]::AttachConsole(31116)){throw 'AttachConsole failed'}
try {
[R280Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R280Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 31116 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R280Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R280Console]::FreeConsole() | Out-Null}


~~~
CLI_EXIT=0，SOURCE0/1/2_EXIT=0，REENTRY_EXIT=0。观察到三格画面→全黑→左下格恢复→全黑。此为功能运行证据；未测接收端 A/V，未证明网络交付时序，也未完成预算闭合。

## 最终指标与诊断
内部 raw_phase_ns 样本=230，min=11337，max=11339，不等同接收端 A/V。
~~~text
[2026-10-21 09:59:49.996] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=23601 committed_datagrams=156978 committed_payload_bytes=197319060 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=53 ordered_queue_maximum_wire_bytes=78147 pacing_reserved=156978 pacing_cancelled=0 pacing_submitted=156978 pacing_maximum_submit_lateness_ns=45772980 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=76 backlog_high_water_wire_bytes=111710 backlog_max_residence_ns=58190400 last_materialized_sequence=156978 last_scheduled_sequence=156978 last_submitted_sequence=156978 last_committed_sequence=156978 shared_scope_charged_datagrams=156978 shared_scope_charged_wire_bytes=201714444 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=3806 evidence_timestamp_tracked=0 evidence_timestamp_untracked=3806 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=756580 endpoint_1_target_effective_socket_bytes=189145 endpoint_1_api_requested_socket_bytes=189145 endpoint_1_effective_socket_bytes=189145 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=142913 endpoint_1_committed_payload_bytes=191459471 endpoint_2_target_effective_socket_bytes=189145 endpoint_2_api_requested_socket_bytes=189145 endpoint_2_effective_socket_bytes=189145 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=77 endpoint_2_committed_payload_bytes=5244 endpoint_3_target_effective_socket_bytes=189145 endpoint_3_api_requested_socket_bytes=189145 endpoint_3_effective_socket_bytes=189145 endpoint_3_timestamp_source=0 endpoint_3_committed_datagrams=13906 endpoint_3_committed_payload_bytes=5848761 endpoint_4_target_effective_socket_bytes=189145 endpoint_4_api_requested_socket_bytes=189145 endpoint_4_effective_socket_bytes=189145 endpoint_4_timestamp_source=0 endpoint_4_committed_datagrams=82 endpoint_4_committed_payload_bytes=5584
[2026-10-21 09:59:50.336] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=10355 completion_waits=10355 completed_datagrams=10462 drained_datagrams=150 would_block=44 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-21 09:59:50.337] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=133124 completion_waits=133124 completed_datagrams=212075 drained_datagrams=78995 would_block=44 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-21 09:59:50.338] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=77816 completion_waits=77816 completed_datagrams=105528 drained_datagrams=27816 would_block=104 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-21 09:59:50.338] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=78266 completion_waits=78266 completed_datagrams=105379 drained_datagrams=27216 would_block=104 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=914, threads=80, processThreads=129, workers=0, workerProgress=502829, workerProcessCalls=1958191, workerWaits=1455346, workerWakeups=1431706, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=1027, logicalProcessors=22, averageSystemMachineCpuPercent=10.388835, averageProcessMachineCpuPercent=1.465538, peakProcessMachineCpuPercent=5.206074, averageProcessSingleCoreCpuPercent=32.241838, peakProcessSingleCoreCpuPercent=114.533623, initialWorkingSetBytes=208134144, workingSetBytes=260222976, peakWorkingSetBytes=269979648, totalPushed=460922, totalPopped=460886, droppedBuffers=0, encodedPacketsPushed=84691, encodedPacketsPopped=84691, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=16943515, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=734, graphPayloadReservations=261902, graphPayloadReleases=261902, graphPayloadPressureFailures=0, lastInputReceivedAtNs=4541372464800
~~~
### stderr
~~~text
[in @ 00000221e2d22a40] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000221e2d22a40] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 28358.792044
[in @ 00000221e86d10c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000221e86d10c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 31268.778189
[in @ 00000221ceef4980] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000221ceef4980] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 41700.768111
[in @ 00000221e29d2240] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000221cef76180] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000221cef76180] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 28358.792044
[in @ 00000221e29d2780] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000221e29d2780] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 41700.768111
[in @ 00000221e29d2240] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 31268.778189
[aac @ 00000221cebad380] Qavg: 18806.514
~~~
### 源结束
~~~text
source0
[out#1/rtp @ 00000236d703c6c0] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
source1
[out#0/rtp @ 000001fbac458dc0] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.46    
source2
[out#0/rtp @ 000001508c9a88c0] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
reentry
[out#1/rtp @ 00000229502defc0] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.49    
~~~
### 清理清单
composition-r280-cli-error.log
composition-r280-cli.log
composition-r280-monitor.log
composition-r280-native-2026-10-21-09h56m04s224.png
composition-r280-native-2026-10-21-09h57m02s664.png
composition-r280-native-2026-10-21-09h57m37s507.png
composition-r280-native-2026-10-21-09h59m32s028.png
composition-r280-output.sdp
composition-r280-reentry-stdout.log
composition-r280-reentry.log
composition-r280-source0-stdout.log
composition-r280-source0.log
composition-r280-source1-stdout.log
composition-r280-source1.log
composition-r280-source2-stdout.log
composition-r280-source2.log
composition-r280-vlc.log

### VLC 诊断与采样边界
~~~text
main error: read error: No error
main error: write error: No error
main error: read error: No error
main error: Failed to create video converter
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r280-native-2026-10-21-09h56m04s224.png)
main error: Failed to create video converter
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r280-native-2026-10-21-09h57m02s664.png)
main error: read error: No error
main error: Failed to create video converter
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r280-native-2026-10-21-09h57m37s507.png)
main error: read error: No error
main error: Failed to create video converter
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r280-native-2026-10-21-09h59m32s028.png)
main error: read error: No error
direct3d11 error: SetThumbNailClip failed: 0x800706f4
main error: read error: No error
main error: write error: No error
-- logger module stopped --
seconds=1.013143 cpu=0.771115232499262 ws=142774272 private=569483264
seconds=328.5358093 cpu=0.691946057878151 ws=259883008 private=1125830656
~~~

## 清理结果与边界
按上述清单逐项删除 17 个文件，复查本轮残留为 0，指定 120 秒源保留。Win32_Process 精确查询确认六个测试 PID 均不存在。VLC 日志中的 converter/read/thumbnail 诊断已保留，不以画面快照掩盖；四个阶段原生快照确实生成并人工检查。CLI errors/workerErrors/deadline_misses/pressure_failures 均为 0，payload reserve/release 均 261902，最终 payload 0；delivery_evidence=not_proven。内部 raw_phase 不能替代接收端 A/V 测量。
