# r331：协议工作事实同构建收尾复验

## 原生快照及结束命令

~~~~powershell
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 37708){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020);$s=$c.GetStream();$b=[Text.Encoding]::ASCII.GetBytes("snapshot`n");$s.Write($b,0,$b.Length);$c.Dispose()
$target=Get-CimInstance Win32_Process -Filter "ProcessId=26520"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r331*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R331Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R331Console]::FreeConsole() | Out-Null
if(-not [R331Console]::AttachConsole(26520)){throw 'AttachConsole failed'}
try {
[R331Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R331Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 26520 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R331Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R331Console]::FreeConsole() | Out-Null}
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 37708){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020);$s=$c.GetStream();$b=[Text.Encoding]::ASCII.GetBytes("quit`n");$s.Write($b,0,$b.Length);$c.Dispose()
~~~~

沿用r330新Release与同规格三路120秒H264/AAC RTP→HEVC CBR 8000kbps 1280×720/30、AAC CBR 192kbps 44100Hz stereo→MPEG-TS/RTP。仅realtime、VLC原生快照。已执行，本切片收尾证据完整，整体合屏门禁仍未通过。

## 启动命令

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r331-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r331-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r331-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r331-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r331-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r331-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r331 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62024 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r331-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r331-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r331-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r331-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r331-native- --snapshot-format=png rtp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r331-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

## 结果

源22700/19340/29988完整120秒自然exit0，CLI26520经调用方Ctrl+C exit0，VLC37708。两张原生快照确认三格及失源全黑。TS88128个/102417136字节，RTP媒体103474672=TS+12×88128，RTCP39个/2660，总88167个/103477332字节对账。worker错误、deadline miss及压力失败均0；payload与objects回零，reservation/release150197。CPU机器平均2.428115%、峰7.389163%，工作集峰254418944字节。未覆盖重入、完整矩阵、内容AV、NIC交付或间歇崩溃修复；r330未知退出仍单列为证据不完整。

~~~~text
[2026-10-26 10:01:59.909] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 produced_datagrams=88128 produced_ts_payload_bytes=102417136 pending_bytes=0 peak_pending_bytes=92872 access_units=10970 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=32789115 maximum_mux_input_after_emit_ns=81172087 maximum_mux_input_after_dispatch_ns=-18827913 worst_mux_input_ready_ns=78498582100 worst_mux_input_emit_ns=78417410013 worst_mux_input_dispatch_ns=78517410013 maximum_protocol_batch_produced_after_release_ns=63437940 worst_protocol_batch_produced_ns=78511235000 worst_protocol_batch_release_ns=78447797060 worst_protocol_batch_deadline_ns=78547795501 worst_protocol_batch_datagrams=1 exit_reason=completed
[2026-10-26 10:01:59.911] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=88129 committed_datagrams=88167 committed_payload_bytes=103477332 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=2 ordered_queue_maximum_wire_bytes=1452 pacing_reserved=88167 pacing_cancelled=0 pacing_submitted=88167 pacing_maximum_submit_lateness_ns=49974320 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=2 backlog_high_water_wire_bytes=1452 backlog_max_residence_ns=22753100 last_materialized_sequence=88167 last_scheduled_sequence=88167 last_submitted_sequence=88167 last_committed_sequence=88167 submitted_service_generation=1 submitted_service_sequence=88167 submitted_service_completed_ns=150632544700 submitted_service_duration_ns=16640 submitted_service_available_ns=150632561340 submitted_scope_available_ns=150632561340 shared_scope_charged_datagrams=88167 shared_scope_charged_wire_bytes=105946008 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2155 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2155 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=379902 endpoint_1_target_effective_socket_bytes=189951 endpoint_1_api_requested_socket_bytes=189951 endpoint_1_effective_socket_bytes=189951 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=88128 endpoint_1_committed_payload_bytes=103474672 endpoint_2_target_effective_socket_bytes=189951 endpoint_2_api_requested_socket_bytes=189951 endpoint_2_effective_socket_bytes=189951 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=39 endpoint_2_committed_payload_bytes=2660
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=396, threads=82, processThreads=130, workers=0, workerProgress=572539, workerProcessCalls=2043486, workerWaits=1470929, workerWakeups=1459462, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=469, logicalProcessors=22, averageSystemMachineCpuPercent=19.041409, averageProcessMachineCpuPercent=2.428115, peakProcessMachineCpuPercent=7.389163, averageProcessSingleCoreCpuPercent=53.418540, peakProcessSingleCoreCpuPercent=162.561576, initialWorkingSetBytes=190574592, workingSetBytes=243703808, peakWorkingSetBytes=254418944, totalPushed=462096, totalPopped=462072, droppedBuffers=0, encodedPacketsPushed=50352, encodedPacketsPopped=50352, backpressureItems=124, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=4296691, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=210, graphPayloadReservations=150197, graphPayloadReleases=150197, graphPayloadPressureFailures=0, lastInputReceivedAtNs=4553694946100
~~~~

## 清理清单

D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r331-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r331-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r331-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r331-native-2026-10-26-10h00m05s336.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r331-native-2026-10-26-10h01m49s612.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r331-output.sdp
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r331-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r331-source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r331-source1-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r331-source1.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r331-source2-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r331-source2.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r331-vlc.log

13项临时产物已逐项清理，五PID无残留，指定120秒源保留。
