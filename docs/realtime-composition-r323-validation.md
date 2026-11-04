# r323：planner 消费服务观察复验

三路120秒 H264/AAC RTP→HEVC CBR 8000kbps 1280×720/30、AAC CBR 192kbps 44100Hz stereo→MPEG-TS/UDP 62024。仅 realtime，VLC 原生快照。已实际执行，不计整体合屏验收通过。

## 结果

源PID2568、38400、29508均完整120秒自然exit0；CLI43784约153秒经调用方Ctrl+C退出0，非源结束自然退出；VLC16528经RC quit退出。无重入。两张原生快照已查看，三格视频/第四格黑→全黑。输入range unknown→tv警告仍存在。

发布与提交均84905数据报、98337160 TS payload bytes；代1/序号84905一致，sender/scope available均152618834260ns=completed152618799700+duration34560。deadline miss/运行错误0；payload/objects回零，reserve/release均144986。CPU机器平均2.395337%、峰6.551060%，WS峰273338368 bytes。phase约11338ns仅时钟证据，声音及内容级A/V未证明。服务观察planner接线实流完成，不等于完整周期或整体验收通过。

## 快照和停止命令

~~~~powershell
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 16528){throw 'VLC RC PID mismatch'}; $c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020); $s=$c.GetStream(); $b=[Text.Encoding]::ASCII.GetBytes("snapshot`n"); $s.Write($b,0,$b.Length); $s.Dispose(); $c.Dispose()
$target=Get-CimInstance Win32_Process -Filter "ProcessId=43784"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r323*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R323Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R323Console]::FreeConsole() | Out-Null
if(-not [R323Console]::AttachConsole(43784)){throw 'AttachConsole failed'}
try {
[R323Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R323Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 43784 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R323Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R323Console]::FreeConsole() | Out-Null}

if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 16528){throw 'VLC RC PID mismatch'}; $c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020); $s=$c.GetStream(); $b=[Text.Encoding]::ASCII.GetBytes("quit`n"); $s.Write($b,0,$b.Length); $s.Dispose(); $c.Dispose()
~~~~

## 执行命令

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r323-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r323-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r323-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r323-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r323-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r323-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r323 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r323-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r323-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r323-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r323-native- --snapshot-format=png udp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r323-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

## 清理前原始指标与产物清单

~~~~text
[2026-10-23 15:54:12.872] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 produced_datagrams=84905 produced_ts_payload_bytes=98337160 pending_bytes=0 peak_pending_bytes=110920 access_units=10743 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=32789115 maximum_mux_input_after_emit_ns=43338915 maximum_mux_input_after_dispatch_ns=-56661085 worst_mux_input_ready_ns=95656799000 worst_mux_input_emit_ns=95613460085 worst_mux_input_dispatch_ns=95713460085 maximum_protocol_batch_produced_after_release_ns=33341512 worst_protocol_batch_produced_ns=91646259100 worst_protocol_batch_release_ns=91612917588 worst_protocol_batch_deadline_ns=91712915868 worst_protocol_batch_datagrams=1 exit_reason=completed
[2026-10-23 15:54:12.873] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=84905 committed_datagrams=84905 committed_payload_bytes=98337160 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=1 ordered_queue_maximum_wire_bytes=1344 pacing_reserved=84905 pacing_cancelled=0 pacing_submitted=84905 pacing_maximum_submit_lateness_ns=4976100 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=1 backlog_high_water_wire_bytes=1344 backlog_max_residence_ns=22971600 last_materialized_sequence=84905 last_scheduled_sequence=84905 last_submitted_sequence=84905 last_committed_sequence=84905 submitted_service_generation=1 submitted_service_sequence=84905 submitted_service_completed_ns=152618799700 submitted_service_duration_ns=34560 submitted_service_available_ns=152618834260 submitted_scope_available_ns=152618834260 shared_scope_charged_datagrams=84905 shared_scope_charged_wire_bytes=100714500 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2154 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2154 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=188010 endpoint_1_target_effective_socket_bytes=188010 endpoint_1_api_requested_socket_bytes=188010 endpoint_1_effective_socket_bytes=188010 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=84905 endpoint_1_committed_payload_bytes=98337160
[CLI] final runtime report: queued=0, peakQueued=930, threads=81, processThreads=129, workers=0, workerProgress=557104, workerProcessCalls=1942872, workerWaits=1385751, workerWakeups=1374534, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=452, logicalProcessors=22, averageSystemMachineCpuPercent=15.566392, averageProcessMachineCpuPercent=2.395337, peakProcessMachineCpuPercent=6.551060, averageProcessSingleCoreCpuPercent=52.697412, peakProcessSingleCoreCpuPercent=144.123314, initialWorkingSetBytes=212111360, workingSetBytes=246693888, peakWorkingSetBytes=273338368, totalPushed=448668, totalPopped=448644, droppedBuffers=0, encodedPacketsPushed=49310, encodedPacketsPopped=49310, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=17532346, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=751, graphPayloadReservations=144986, graphPayloadReleases=144986, graphPayloadPressureFailures=0, lastInputReceivedAtNs=25578609546900
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r323-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r323-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r323-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r323-native-2026-10-23-15h52m17s875.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r323-native-2026-10-23-15h53m56s682.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r323-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r323-source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r323-source1-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r323-source1.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r323-source2-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r323-source2.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r323-vlc.log
~~~~

清理：12项临时产物逐项删除，前缀残留0，提升权限核验5个精确PID残留0。复用源124427809 bytes保留，无远程测试。
