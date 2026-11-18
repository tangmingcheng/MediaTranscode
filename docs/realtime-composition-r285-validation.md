# r285：AAC 部署与激活窗口对齐复验

## 原生快照、停止与 VLC 退出命令

~~~powershell
$client=[Net.Sockets.TcpClient]::new('127.0.0.1',63020); try {$stream=$client.GetStream(); $stream.ReadTimeout=2000; $bytes=[Text.Encoding]::ASCII.GetBytes("status`nsnapshot`n"); $stream.Write($bytes,0,$bytes.Length); $buffer=New-Object byte[] 8192; try {while(($count=$stream.Read($buffer,0,$buffer.Length)) -gt 0){[Text.Encoding]::UTF8.GetString($buffer,0,$count)}} catch {Write-Output 'RC read complete'}} finally {$client.Dispose()}
$ErrorActionPreference='Stop'
$target=Get-CimInstance Win32_Process -Filter "ProcessId=39924"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r285*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R285Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R285Console]::FreeConsole() | Out-Null
if(-not [R285Console]::AttachConsole(39924)){throw 'AttachConsole failed'}
try {
[R285Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R285Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 39924 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R285Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R285Console]::FreeConsole() | Out-Null}


$p=Get-CimInstance Win32_Process -Filter 'ProcessId=5592'; if($p){if($p.Name -ne 'vlc.exe' -or $p.CommandLine -notlike '*composition-r285*'){throw 'VLC identity mismatch'}; $client=[Net.Sockets.TcpClient]::new('127.0.0.1',63020); try{$stream=$client.GetStream(); $bytes=[Text.Encoding]::ASCII.GetBytes("quit`n"); $stream.Write($bytes,0,$bytes.Length)}finally{$client.Dispose()}}
~~~

状态：已执行，启动及功能画面复验完成；整体验收门禁未通过。三路H.264 1280×720 30fps约8Mbps输入，HEVC 1280×720 30fps 8Mbps CBR及AAC 192kbps/44100Hz/双声道独立RTP输出；统一120秒连续源、不循环、不降低规格，仅realtime及VLC原生快照。

## 启动与监控命令

~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r285-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r285-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r285-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r285-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r285-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r285-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r285 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout separate --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r285-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r285-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r285-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10 -and (Test-Path -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r285-output.sdp')) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r285-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r285-native- --snapshot-format=png file:///D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r285-output.sdp' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r285-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

## 重入命令

~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r285-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r285-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($source0.Id)"
$source0.WaitForExit()
Write-Output "REENTRY_EXIT=$($source0.ExitCode)"
exit $source0.ExitCode
~~~

## 本轮结果

Release全量重建configure/build=0，729项完成，四项必需产物齐全，realtime SHA256 2C6D5AA99EF1BC6347826C10CCFD06E949DDE873B8ED707469D3D176DC8DDD80。音频窗口对齐双独立源码审查PASS。

PID：初始源40196/17624/17096，CLI39924，VLC5592，重入39132。四次源均完整120秒/3600帧且退出0；CLI调用方Ctrl+C停止后退出0，不是失源自然EOF。启动session3149和重入session9362均完成0。

VLC四张原生快照已逐张检查：11:53:35.974三格正常；11:55:26.150全黑；11:55:59.928左下所选源恢复；11:57:42.786再次全黑。没有系统截图。

[CLI] final runtime report: queued=0, peakQueued=911, threads=80, processThreads=128, workers=0, workerProgress=481774, workerProcessCalls=1966472, workerWaits=1484682, workerWakeups=1464118, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=878, logicalProcessors=22, averageSystemMachineCpuPercent=10.643357, averageProcessMachineCpuPercent=1.709034, peakProcessMachineCpuPercent=8.235294, averageProcessSingleCoreCpuPercent=37.598747, peakProcessSingleCoreCpuPercent=181.176471, initialWorkingSetBytes=209448960, workingSetBytes=261611520, peakWorkingSetBytes=269672448, totalPushed=442912, totalPopped=442876, droppedBuffers=0, encodedPacketsPushed=73600, encodedPacketsPopped=73600, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=17231321, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=745, graphPayloadReservations=252004, graphPayloadReleases=252004, graphPayloadPressureFailures=0, lastInputReceivedAtNs=11642792097200

[2026-10-21 11:57:56.902] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=20511 committed_datagrams=153903 committed_payload_bytes=197269253 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=53 ordered_queue_maximum_wire_bytes=78147 pacing_reserved=153903 pacing_cancelled=0 pacing_submitted=153903 pacing_maximum_submit_lateness_ns=29709520 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=76 backlog_high_water_wire_bytes=111710 backlog_max_residence_ns=67406700 last_materialized_sequence=153903 last_scheduled_sequence=153903 last_submitted_sequence=153903 last_committed_sequence=153903 shared_scope_charged_datagrams=153903 shared_scope_charged_wire_bytes=201578537 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=3805 evidence_timestamp_tracked=0 evidence_timestamp_untracked=3805 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=756580 endpoint_1_target_effective_socket_bytes=189145 endpoint_1_api_requested_socket_bytes=189145 endpoint_1_effective_socket_bytes=189145 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=141678 endpoint_1_committed_payload_bytes=191448376 endpoint_2_target_effective_socket_bytes=189145 endpoint_2_api_requested_socket_bytes=189145 endpoint_2_effective_socket_bytes=189145 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=70 endpoint_2_committed_payload_bytes=4768 endpoint_3_target_effective_socket_bytes=189145 endpoint_3_api_requested_socket_bytes=189145 endpoint_3_effective_socket_bytes=189145 endpoint_3_timestamp_source=0 endpoint_3_committed_datagrams=12085 endpoint_3_committed_payload_bytes=5811341 endpoint_4_target_effective_socket_bytes=189145 endpoint_4_api_requested_socket_bytes=189145 endpoint_4_effective_socket_bytes=189145 endpoint_4_timestamp_source=0 endpoint_4_committed_datagrams=70 endpoint_4_committed_payload_bytes=4768

内部A/V generation=1，样本113，raw phase范围6802..6804 ns；不是接收端A/V证据。

内部A/V generation=2，样本117，raw phase范围15873..15874 ns；不是接收端A/V证据。

late_output_commit出现次数=0。最大submit lateness=29709520ns，delivery_evidence=not_proven，observed=0，因此整体门禁未通过。此轮证明预算失败在同规格复验中不再发生，不代表C+W或完整Windows矩阵完成。

### stderr与VLC错误

~~~text
[in @ 0000024ad57dfac0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000024ad57dfac0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 6299.020889
[in @ 0000024ad57df880] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000024ad57df880] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 3585.500178
[in @ 0000024ad5abf5c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000024ad5abf5c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 15971.604856
[in @ 0000024ae1013c00] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000024ae05bfa00] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000024ae1013c00] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 6299.020889
[in @ 0000024adaff20c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000024ae05bfa00] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 15971.604856
[in @ 0000024adaff20c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 3585.500178
[aac @ 0000024adab306c0] Qavg: 11750.090

main error: buffer deadlock prevented
main error: Failed to create video converter
direct3d11 error: SetThumbNailClip failed: 0x800706f4
~~~

### 本轮清理清单

- composition-r285-cli-error.log
- composition-r285-cli.log
- composition-r285-monitor.log
- composition-r285-native-2026-10-21-11h53m35s974.png
- composition-r285-native-2026-10-21-11h55m26s150.png
- composition-r285-native-2026-10-21-11h55m59s928.png
- composition-r285-native-2026-10-21-11h57m42s786.png
- composition-r285-output.sdp
- composition-r285-reentry-stdout.log
- composition-r285-reentry.log
- composition-r285-source0-stdout.log
- composition-r285-source0.log
- composition-r285-source1-stdout.log
- composition-r285-source1.log
- composition-r285-source2-stdout.log
- composition-r285-source2.log
- composition-r285-vlc.log

以上17项已按清单逐项删除，r285文件残留0；指定连续源保留。本轮未运行远程测试，无本轮远程产物。

六个精确PID已经Win32_Process复核，残留0。
