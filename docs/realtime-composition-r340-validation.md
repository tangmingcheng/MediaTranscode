# r340：generation 固定控制存储 TS/RTP 复验

状态：本轮TS/RTP正常链路观察通过；完整存储单元验收尚未完成。20文件双独立源码PASS；Release全量747项configure/build exit0，四产物齐全。realtime为6511104字节，2026-10-26 12:12:08。仅realtime，不执行local。

原120秒三源H264/AAC输入，HEVC CBR 8000kbps 1280×720/30、AAC CBR 192kbps 44100Hz双声道，TS/RTP。VLC原生快照、CPU/内存/时钟遥测、退出及资源释放；正常start覆盖空Data换槽，不等同已持事务输出purge。

## 本轮启动及监控命令

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r340-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r340-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r340-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r340-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r340-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r340-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r340 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62024 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r340-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r340-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r340-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r340-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r340-native- --snapshot-format=png rtp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r340-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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
~~~~

## 本轮结果

三源PID37960/13912/41860均3600帧、120秒自然exit0；CLI38988 caller_stop exit0，VLC33652 RC退出。约169.64秒监控；CPU平均1.912160%、峰5.156951%，WS初始209252352、峰271671296、结束246018048字节，末次private1250033664。payload bytes/objects均0，reserve/release149083；worker/errors/pressure/deadline/partial/ambiguous均0，backlog0。

TS产生86936数据报/98887624字节；RTP媒体99930856=98887624+12×86936；RTCP41数据报/2796字节；合计86977/99933652与sender一致。仅证明socket提交对账，delivery_evidence仍not_proven。末段输入时钟filtered_phase_ns=6804，frequency/stretch=0；不作为内容A/V完整证明。

两张VLC原生1280×720快照已人工查看：12:17:38三格活动画面且右下黑；12:19:29全黑。stderr保留FFmpeg range unknown→tv的frame property提示，无本轮新fatal。正常启动换槽得到实际覆盖；已持事务输出purge、UDP、重入及RKMPP仍未覆盖，不能宣称本单元全部验收完成。

## 原生快照与停止命令（已执行）

~~~~powershell
$ErrorActionPreference='Stop'; $listener=Get-NetTCPConnection -State Listen -LocalPort 63020; if($listener.OwningProcess -ne 33652){throw 'Unexpected VLC RC owner'}; $client=[Net.Sockets.TcpClient]::new('127.0.0.1',63020); try{$bytes=[Text.Encoding]::ASCII.GetBytes("snapshot`n"); $client.GetStream().Write($bytes,0,$bytes.Length)}finally{$client.Dispose()}; Start-Sleep -Seconds 2; Get-ChildItem 'D:/Code/MyCode/MediaTranscode/out/acceptance' -Filter 'composition-r340-native-*.png' | Select-Object FullName,Length
~~~~

快照命令执行两次。停止命令：

~~~~powershell
$target=Get-CimInstance Win32_Process -Filter "ProcessId=38988"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r340*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R340Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R340Console]::FreeConsole() | Out-Null
if(-not [R340Console]::AttachConsole(38988)){throw 'AttachConsole failed'}
try {
[R340Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R340Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 38988 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R340Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R340Console]::FreeConsole() | Out-Null}
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 33652){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020);$s=$c.GetStream();$b=[Text.Encoding]::ASCII.GetBytes("quit`n");$s.Write($b,0,$b.Length);$c.Dispose()
~~~~

## 最终遥测与清理清单

以下13项临时文件在记录完成后逐项清理，五个精确PID已核查无残留。本轮未运行远程进程或产生远程文件。

~~~~text
[2026-10-26 12:19:42.977] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 produced_datagrams=86936 produced_ts_payload_bytes=98887624 pending_bytes=0 peak_pending_bytes=110920 access_units=12024 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=32789115 maximum_mux_input_after_emit_ns=38086478 maximum_mux_input_after_dispatch_ns=-61913522 worst_mux_input_ready_ns=5473093800 worst_mux_input_emit_ns=5435007322 worst_mux_input_dispatch_ns=5535007322 maximum_protocol_batch_produced_after_release_ns=33342852 worst_protocol_batch_produced_ns=29445130700 worst_protocol_batch_release_ns=29411787848 worst_protocol_batch_deadline_ns=29511787368 worst_protocol_batch_datagrams=1 exit_reason=completed
[2026-10-26 12:19:42.978] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=86937 committed_datagrams=86977 committed_payload_bytes=99933652 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=2 ordered_queue_maximum_wire_bytes=1452 pacing_reserved=86977 pacing_cancelled=0 pacing_submitted=86977 pacing_maximum_submit_lateness_ns=5069060 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=2 backlog_high_water_wire_bytes=1452 backlog_max_residence_ns=23266300 last_materialized_sequence=86977 last_scheduled_sequence=86977 last_submitted_sequence=86977 last_committed_sequence=86977 submitted_service_generation=1 submitted_service_sequence=86977 submitted_service_completed_ns=169949383800 submitted_service_duration_ns=16640 submitted_service_available_ns=169949400440 submitted_scope_available_ns=169949400440 shared_scope_charged_datagrams=86977 shared_scope_charged_wire_bytes=102369008 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2155 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2155 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=379902 endpoint_1_target_effective_socket_bytes=189951 endpoint_1_api_requested_socket_bytes=189951 endpoint_1_effective_socket_bytes=189951 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=86936 endpoint_1_committed_payload_bytes=99930856 endpoint_2_target_effective_socket_bytes=189951 endpoint_2_api_requested_socket_bytes=189951 endpoint_2_effective_socket_bytes=189951 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=41 endpoint_2_committed_payload_bytes=2796
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=899, threads=82, processThreads=130, workers=0, workerProgress=572575, workerProcessCalls=1989071, workerWaits=1416478, workerWakeups=1403940, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=540, logicalProcessors=22, averageSystemMachineCpuPercent=18.819928, averageProcessMachineCpuPercent=1.912160, peakProcessMachineCpuPercent=5.156951, averageProcessSingleCoreCpuPercent=42.067528, peakProcessSingleCoreCpuPercent=113.452915, initialWorkingSetBytes=209252352, workingSetBytes=246018048, peakWorkingSetBytes=271671296, totalPushed=459790, totalPopped=459766, droppedBuffers=0, encodedPacketsPushed=55189, encodedPacketsPopped=55189, backpressureItems=124, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=16781185, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=728, graphPayloadReservations=149083, graphPayloadReleases=149083, graphPayloadPressureFailures=0, lastInputReceivedAtNs=12797369049200
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r340-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r340-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r340-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r340-native-2026-10-26-12h17m38s716.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r340-native-2026-10-26-12h19m29s655.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r340-output.sdp
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r340-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r340-source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r340-source1-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r340-source1.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r340-source2-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r340-source2.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r340-vlc.log

~~~~



清理实际结果：R340_CLEAN_FILES=13，R340_REMAINING_PIDS=0；会话17054已返回exit0。
