# r328：发布身份传播原规格复验

## 调用方停止命令

~~~~powershell
$target=Get-CimInstance Win32_Process -Filter "ProcessId=29796"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r328*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R328Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R328Console]::FreeConsole() | Out-Null
if(-not [R328Console]::AttachConsole(29796)){throw 'AttachConsole failed'}
try {
[R328Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R328Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 29796 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R328Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R328Console]::FreeConsole() | Out-Null}
~~~~

三路120秒 H264/AAC RTP→HEVC CBR 8000kbps 1280×720/30、AAC CBR 192kbps 44100Hz stereo→MPEG-TS/RTP。使用新Release、无调试器，仅realtime和VLC原生快照。已执行；本切片复验完成，不替代完整矩阵或宣称r326已修复。

## 启动命令

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r328-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r328-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r328-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r328-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r328-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r328-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r328 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62024 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r328-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r328-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r328-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r328-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r328-native- --snapshot-format=png rtp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r328-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

## 结果与证据边界

两名独立静态审查PASS、Release全量745项重建exit0。三路源35856/14104/9008完整120秒自然exit0；CLI29796在持续失源输出后经调用方Ctrl+C退出0，end_reason=caller_stop；VLC21596。两张原生快照已观察三格视频和全黑。未覆盖重入、完整矩阵或内容A/V，不计整体完成。

TS产生87280数据报、98410668字节；RTP媒体payload=98410668+12×87280=99458028字节，RTCP43个2932字节；总提交87323个99460960字节。期限错过/worker错误/pressure均0；payload及objects回零，reservations/releases=150240。CPU机器平均3.055868%、峰值8.465608%，工作集峰264138752字节。时钟phase约20409ns不等于内容AV漂移，socket提交不等于NIC交付。

~~~~text
[2026-10-26 09:34:49.692] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 produced_datagrams=87280 produced_ts_payload_bytes=98410668 pending_bytes=0 peak_pending_bytes=92872 access_units=12574 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=33151927 maximum_mux_input_after_emit_ns=39482021 maximum_mux_input_after_dispatch_ns=-60517979 worst_mux_input_ready_ns=65274716100 worst_mux_input_emit_ns=65235234079 worst_mux_input_dispatch_ns=65335234079 maximum_protocol_batch_produced_after_release_ns=33377632 worst_protocol_batch_produced_ns=115268251100 worst_protocol_batch_release_ns=115234873468 worst_protocol_batch_deadline_ns=115334871268 worst_protocol_batch_datagrams=1 exit_reason=completed
[2026-10-26 09:34:49.694] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=87281 committed_datagrams=87323 committed_payload_bytes=99460960 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=2 ordered_queue_maximum_wire_bytes=1452 pacing_reserved=87323 pacing_cancelled=0 pacing_submitted=87323 pacing_maximum_submit_lateness_ns=11190880 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=2 backlog_high_water_wire_bytes=1452 backlog_max_residence_ns=22833300 last_materialized_sequence=87323 last_scheduled_sequence=87323 last_submitted_sequence=87323 last_committed_sequence=87323 submitted_service_generation=1 submitted_service_sequence=87323 submitted_service_completed_ns=177302323800 submitted_service_duration_ns=16640 submitted_service_available_ns=177302340440 submitted_scope_available_ns=177302340440 shared_scope_charged_datagrams=87323 shared_scope_charged_wire_bytes=101906004 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2155 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2155 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=379902 endpoint_1_target_effective_socket_bytes=189951 endpoint_1_api_requested_socket_bytes=189951 endpoint_1_effective_socket_bytes=189951 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=87280 endpoint_1_committed_payload_bytes=99458028 endpoint_2_target_effective_socket_bytes=189951 endpoint_2_api_requested_socket_bytes=189951 endpoint_2_effective_socket_bytes=189951 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=43 endpoint_2_committed_payload_bytes=2932
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=821, threads=82, processThreads=130, workers=0, workerProgress=576207, workerProcessCalls=2148913, workerWaits=1572688, workerWakeups=1559535, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=465, logicalProcessors=22, averageSystemMachineCpuPercent=21.432735, averageProcessMachineCpuPercent=3.055868, peakProcessMachineCpuPercent=8.465608, averageProcessSingleCoreCpuPercent=67.229086, peakProcessSingleCoreCpuPercent=186.243386, initialWorkingSetBytes=205844480, workingSetBytes=239341568, peakWorkingSetBytes=264138752, totalPushed=462759, totalPopped=462735, droppedBuffers=0, encodedPacketsPushed=57713, encodedPacketsPopped=57713, backpressureItems=124, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=15002937, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=656, graphPayloadReservations=150240, graphPayloadReleases=150240, graphPayloadPressureFailures=0, lastInputReceivedAtNs=2896099254500
~~~~

## 原生快照与播放器结束命令

VLC RC端口63020，已核验监听PID21596；通过TCP发送snapshot换行两次、quit换行结束。没有系统屏幕截图。

## 清理清单

D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r328-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r328-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r328-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r328-native-2026-10-26-09h32m25s125.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r328-native-2026-10-26-09h34m31s307.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r328-output.sdp
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r328-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r328-source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r328-source1-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r328-source1.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r328-source2-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r328-source2.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r328-vlc.log

清理完成：13项按清单逐项删除，五PID无残留；指定120秒源保留。
