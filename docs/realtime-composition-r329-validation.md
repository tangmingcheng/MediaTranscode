# r329：TS/RTP 身份传播后的失源重入

## 快照与结束命令

~~~~powershell
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 37900){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020);$s=$c.GetStream();$b=[Text.Encoding]::ASCII.GetBytes("snapshot`n");$s.Write($b,0,$b.Length);$c.Dispose()
$target=Get-CimInstance Win32_Process -Filter "ProcessId=36372"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r329*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R329Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R329Console]::FreeConsole() | Out-Null
if(-not [R329Console]::AttachConsole(36372)){throw 'AttachConsole failed'}
try {
[R329Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R329Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 36372 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R329Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R329Console]::FreeConsole() | Out-Null}
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 37900){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020);$s=$c.GetStream();$b=[Text.Encoding]::ASCII.GetBytes("quit`n");$s.Write($b,0,$b.Length);$c.Dispose()
~~~~

## 音视频源重入命令

~~~~powershell
$reentry=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r329-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r329-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($reentry.Id)"
$reentry.WaitForExit()
Write-Output "REENTRY_EXIT=$($reentry.ExitCode)"
exit $reentry.ExitCode
~~~~

三路连续120秒 H264/AAC RTP 输入→HEVC CBR 8000kbps 1280×720/30、AAC CBR 192kbps 44100Hz stereo→MPEG-TS/RTP62024。三路源自然结束后恢复音视频源61020/61022一次，仍使用同一120秒源。仅realtime及VLC原生快照。已执行；重入画面与收尾已核验，未判定完整验收通过。

## 启动命令

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r329-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r329-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r329-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r329-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r329-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r329-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r329 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62024 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r329-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r329-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r329-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r329-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r329-native- --snapshot-format=png rtp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r329-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

初始源31204/30824/31528、重入音视频源19608均完整120秒并自然exit0；CLI36372持续失源输出后经调用方Ctrl+C exit0，end_reason=caller_stop；VLC37900。四张原生快照确认三格→全黑→左下重入→全黑，未使用系统截图。

TS176073数据报/205239788字节，RTP媒体207352664字节=TS+12×176073，RTCP73个/4972字节；总176146个/207357636字节对账。worker错误、deadline miss、sender pressure均0；payload pressure计数1首次见于首轮失源附近09:41:49，早于重入，不归因重入。该计数对应额度等待路径，不能由聚合日志确定具体account；保留为未闭合的压力定位项。最终payload与objects回零，reservation/release256125。CPU机器平均2.482283%、峰7.216495%，工作集峰271331328字节。输入时钟generation2 phase11338ns不证明内容AV；输出sender仍generation1，不证明输出跨代purge。间歇崩溃未复现，不代表修复。

~~~~text
[2026-10-26 09:44:48.666] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 produced_datagrams=176073 produced_ts_payload_bytes=205239788 pending_bytes=0 peak_pending_bytes=94564 access_units=21420 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=59684820 maximum_mux_input_after_emit_ns=80731027 maximum_mux_input_after_dispatch_ns=-19268973 worst_mux_input_ready_ns=119507429700 worst_mux_input_emit_ns=119426698673 worst_mux_input_dispatch_ns=119526698673 maximum_protocol_batch_produced_after_release_ns=66322620 worst_protocol_batch_produced_ns=119509259400 worst_protocol_batch_release_ns=119442936780 worst_protocol_batch_deadline_ns=119542934501 worst_protocol_batch_datagrams=1 exit_reason=completed
[2026-10-26 09:44:48.667] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=176074 committed_datagrams=176146 committed_payload_bytes=207357636 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=2 ordered_queue_maximum_wire_bytes=1452 pacing_reserved=176146 pacing_cancelled=0 pacing_submitted=176146 pacing_maximum_submit_lateness_ns=58547640 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=2 backlog_high_water_wire_bytes=1452 backlog_max_residence_ns=24319500 last_materialized_sequence=176146 last_scheduled_sequence=176146 last_submitted_sequence=176146 last_committed_sequence=176146 submitted_service_generation=1 submitted_service_sequence=176146 submitted_service_completed_ns=298645088400 submitted_service_duration_ns=16640 submitted_service_available_ns=298645105040 submitted_scope_available_ns=298645105040 shared_scope_charged_datagrams=176146 shared_scope_charged_wire_bytes=212289724 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2155 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2155 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=379902 endpoint_1_target_effective_socket_bytes=189951 endpoint_1_api_requested_socket_bytes=189951 endpoint_1_effective_socket_bytes=189951 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=176073 endpoint_1_committed_payload_bytes=207352664 endpoint_2_target_effective_socket_bytes=189951 endpoint_2_api_requested_socket_bytes=189951 endpoint_2_effective_socket_bytes=189951 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=73 endpoint_2_committed_payload_bytes=4972
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=973, threads=82, processThreads=130, workers=0, workerProgress=1017584, workerProcessCalls=3454030, workerWaits=2436428, workerWakeups=2414088, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=900, logicalProcessors=22, averageSystemMachineCpuPercent=20.855588, averageProcessMachineCpuPercent=2.482283, peakProcessMachineCpuPercent=7.216495, averageProcessSingleCoreCpuPercent=54.610232, peakProcessSingleCoreCpuPercent=158.762887, initialWorkingSetBytes=211263488, workingSetBytes=260853760, peakWorkingSetBytes=271331328, totalPushed=800082, totalPopped=800046, droppedBuffers=0, encodedPacketsPushed=98311, encodedPacketsPopped=98311, backpressureItems=124, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=18980205, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=792, graphPayloadReservations=256125, graphPayloadReleases=256125, graphPayloadPressureFailures=1, lastInputReceivedAtNs=3528795603500
[2026-10-26 09:41:49.567] [info] [graph][runtime.node] rtp_binder_purge node=11 transition=1 cutoff=3625
[2026-10-26 09:41:49.567] [info] [graph][runtime.node] av_reacquisition group=composition-r329.source.0 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=1 next=2 transition=1
[CLI] runtime report: queued=2, peakQueued=971, threads=82, processThreads=130, workers=76, workerProgress=534981, workerProcessCalls=1906570, workerWaits=1371582, workerWakeups=1362810, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=345, logicalProcessors=22, averageSystemMachineCpuPercent=19.983883, averageProcessMachineCpuPercent=3.244748, peakProcessMachineCpuPercent=7.216495, averageProcessSingleCoreCpuPercent=71.384445, peakProcessSingleCoreCpuPercent=158.762887, initialWorkingSetBytes=211263488, workingSetBytes=238850048, peakWorkingSetBytes=271331328, totalPushed=430340, totalPopped=430314, droppedBuffers=0, encodedPacketsPushed=38318, encodedPacketsPopped=38317, backpressureItems=124, graphPayloadCurrentBytes=90, graphPayloadHighWaterBytes=18980205, graphPayloadCurrentObjects=2, graphPayloadHighWaterObjects=792, graphPayloadReservations=138307, graphPayloadReleases=138305, graphPayloadPressureFailures=1, lastInputReceivedAtNs=3374416920100
~~~~

## 清理清单

D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r329-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r329-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r329-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r329-native-2026-10-26-09h40m31s605.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r329-native-2026-10-26-09h42m05s315.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r329-native-2026-10-26-09h42m47s952.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r329-native-2026-10-26-09h44m34s977.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r329-output.sdp
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r329-reentry-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r329-reentry.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r329-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r329-source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r329-source1-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r329-source1.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r329-source2-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r329-source2.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r329-vlc.log

清理完成：17项临时产物逐项删除，六PID无残留；指定120秒源保留。未运行local或远程测试。
