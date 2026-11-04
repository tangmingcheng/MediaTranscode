# r341：固定控制存储 UDP 与输入重入复验

状态：本轮UDP与输入重入观察通过；完整合屏门禁仍未完成。同r340 Release6511104字节/2026-10-26 12:12:08，20文件双独立源码PASS。仅realtime；三源原120秒H264/AAC输入，HEVC CBR 8000kbps 1280×720/30、AAC CBR 192kbps 44100Hz双声道，TS/UDP。源自然结束后选中音视频源再完整120秒重入；VLC原生快照与CPU/内存/时钟遥测。输入重入不等于输出purge。

## 启动命令

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r341-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r341-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r341-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r341-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r341-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r341-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r341 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r341-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r341-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r341-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r341-native- --snapshot-format=png udp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r341-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

## 重入命令（已执行）

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r341-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r341-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($source0.Id)"
$source0.WaitForExit()
Write-Output "REENTRY_EXIT=$($source0.ExitCode)"
exit $source0.ExitCode
~~~~

## 实际结果与原生快照/停止命令

三源PID40084/29404/3848、重入PID12180均3600帧120秒自然exit0；CLI24936 caller_stop exit0，VLC21820 RC退出。四张原生1280×720快照人工查看：12:22:48三格，12:24:41全黑，12:25:11左下重入，12:27:00全黑。

TS产出与UDP提交均178692数据报/207928564字节；deadline/pressure/partial/ambiguous/worker/errors均0，backlog0。payload bytes/objects归零，reserve/release262569。监控301.50秒，CPU平均2.000655%、峰7.510730%，WS初始190623744、峰251101184、末247312384字节，末private1159823360。重入源generation2时filtered_phase_ns=13605，frequency/stretch=0；输出sender generation仍1，不作为输出purge覆盖，也不替代内容A/V或NIC交付证据。

以下快照命令执行四次：

~~~~powershell
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 21820){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
$s=$c.GetStream()
$b=[Text.Encoding]::ASCII.GetBytes("snapshot`n")
$s.Write($b,0,$b.Length)
$s.Flush()
$c.Dispose(); Start-Sleep -Seconds 2; Get-ChildItem 'D:/Code/MyCode/MediaTranscode/out/acceptance' -Filter 'composition-r341-native-*.png' | Select-Object FullName,Length
~~~~

停止命令已执行：

~~~~powershell
$target=Get-CimInstance Win32_Process -Filter "ProcessId=24936"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r341*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R341Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R341Console]::FreeConsole() | Out-Null
if(-not [R341Console]::AttachConsole(24936)){throw 'AttachConsole failed'}
try {
[R341Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R341Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 24936 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R341Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R341Console]::FreeConsole() | Out-Null}
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 21820){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020);$s=$c.GetStream();$b=[Text.Encoding]::ASCII.GetBytes("quit`n");$s.Write($b,0,$b.Length);$c.Dispose()
~~~~

## 原始最终遥测与本轮清理清单

记录后按以下16项清理，保留指定复用源。未启动远程验证，不产生远程文件。

~~~~text
[2026-10-26 12:27:17.277] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 produced_datagrams=178692 produced_ts_payload_bytes=207928564 pending_bytes=0 peak_pending_bytes=92872 access_units=21996 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=32789115 maximum_mux_input_after_emit_ns=42036720 maximum_mux_input_after_dispatch_ns=-57963280 worst_mux_input_ready_ns=58558955700 worst_mux_input_emit_ns=58516918980 worst_mux_input_dispatch_ns=58616918980 maximum_protocol_batch_produced_after_release_ns=35384012 worst_protocol_batch_produced_ns=173215526400 worst_protocol_batch_release_ns=173180142388 worst_protocol_batch_deadline_ns=173280138934 worst_protocol_batch_datagrams=1 exit_reason=completed
[2026-10-26 12:27:17.278] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=178692 committed_datagrams=178692 committed_payload_bytes=207928564 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=1 ordered_queue_maximum_wire_bytes=1344 pacing_reserved=178692 pacing_cancelled=0 pacing_submitted=178692 pacing_maximum_submit_lateness_ns=7222900 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=1 backlog_high_water_wire_bytes=1344 backlog_max_residence_ns=22946700 last_materialized_sequence=178692 last_scheduled_sequence=178692 last_submitted_sequence=178692 last_committed_sequence=178692 submitted_service_generation=1 submitted_service_sequence=178692 submitted_service_completed_ns=301537228300 submitted_service_duration_ns=34560 submitted_service_available_ns=301537262860 submitted_scope_available_ns=301537262860 shared_scope_charged_datagrams=178692 shared_scope_charged_wire_bytes=212931940 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2154 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2154 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=188010 endpoint_1_target_effective_socket_bytes=188010 endpoint_1_api_requested_socket_bytes=188010 endpoint_1_effective_socket_bytes=188010 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=178692 endpoint_1_committed_payload_bytes=207928564
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=396, threads=81, processThreads=130, workers=0, workerProgress=1031386, workerProcessCalls=3488702, workerWaits=2457299, workerWakeups=2434402, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=980, logicalProcessors=22, averageSystemMachineCpuPercent=22.565947, averageProcessMachineCpuPercent=2.000655, peakProcessMachineCpuPercent=7.510730, averageProcessSingleCoreCpuPercent=44.014420, peakProcessSingleCoreCpuPercent=165.236052, initialWorkingSetBytes=190623744, workingSetBytes=247312384, peakWorkingSetBytes=251101184, totalPushed=813540, totalPopped=813504, droppedBuffers=0, encodedPacketsPushed=100955, encodedPacketsPopped=100955, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=4552822, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=200, graphPayloadReservations=262569, graphPayloadReleases=262569, graphPayloadPressureFailures=0, lastInputReceivedAtNs=13276696400700
seconds=301.5009746 cpu=0.757867734158315 ws=247312384 private=1159823360
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r341-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r341-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r341-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r341-native-2026-10-26-12h22m48s502.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r341-native-2026-10-26-12h24m41s902.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r341-native-2026-10-26-12h25m11s175.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r341-native-2026-10-26-12h27m00s167.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r341-reentry-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r341-reentry.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r341-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r341-source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r341-source1-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r341-source1.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r341-source2-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r341-source2.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r341-vlc.log

~~~~



清理实测：R341_CLEAN_FILES=16，R341_REMAINING_PIDS=0；会话68616和77987均已返回exit0。
