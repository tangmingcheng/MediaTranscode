# r314：发送批次诊断复验（运行结束，验收证据不完整）

沿r313原规格：三源原120秒RTP H264/AAC→HEVC CBR8000kbps 1280×720/30、AAC目标192kbps→MPEG-TS/UDP。只测试realtime；VLC原生快照，源EOF后观察黑屏，再AV源120秒重入。新增诊断不代表超期已修复。

## 实际启动命令（运行结束，验收证据不完整）
~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r314-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r314-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r314-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r314-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r314-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r314-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r314 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62020 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r314-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r314-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r314-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r314-native- --snapshot-format=png udp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r314-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

Release全量742项构建exit0，产物12:36:25。启动源PID25404/28080/41764，CLI37812，监控会话47916。

## 运行记录

旧VLC39068仍占RC端口，已核实并清理；初次本轮VLC40492同时清理后重启为40732，日志为composition-r314-vlc-restarted.log，其余VLC参数同初始命令。初始三格快照证据缺失，不能将本轮记完整通过。12:39:14原生快照确认全黑。

AV重入PID10008，会话85924，实际命令：
~~~~powershell
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r314-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r314-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($source0.Id)"
$source0.WaitForExit()
Write-Output "REENTRY_EXIT=$($source0.ExitCode)"
~~~~

## 实际调用方停止命令
~~~~powershell
$ErrorActionPreference='Stop'
$target=Get-CimInstance Win32_Process -Filter "ProcessId=37812"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r314*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R314Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R314Console]::FreeConsole() | Out-Null
if(-not [R314Console]::AttachConsole(37812)){throw 'AttachConsole failed'}
try {
[R314Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R314Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 37812 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R314Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R314Console]::FreeConsole() | Out-Null}



~~~~

## 最终结果

四源退出0；CLI调用方停止退出0，不是源EOF自动退出。三张本轮原生快照：初始源结束全黑、AV重入左下格恢复、重入源结束全黑。初始三格证据缺失，不记完整通过。历史超期未复现，不代表修复。内部phase不能代替内容A/V测量。

~~~~text
[2026-10-23 12:41:49.400] [info] [graph][runtime.node] datagram_batch_trace stage=last_completed generation=1 first_sequence=175617 last_sequence=175617 maximum_submit_interval_ns=64000 maximum_submit_interval_sequence=175617 maximum_start_lateness_ns=430866 maximum_start_lateness_sequence=175617 first_release_master_ns=303419828934 first_deadline_master_ns=303519828934 last_release_master_ns=303419828934 last_deadline_master_ns=303519828934 first_submitted_master_ns=303420323800 last_submitted_master_ns=303420323800 worst_wait_started_master_ns=303419223700 worst_wait_completed_master_ns=303420259000 worst_submit_observed_master_ns=303420259800
[2026-10-23 12:41:49.400] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=32201 committed_datagrams=175617 committed_payload_bytes=204049560 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=79296 pacing_reserved=175617 pacing_cancelled=0 pacing_submitted=175617 pacing_maximum_submit_lateness_ns=30588100 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=79296 backlog_max_residence_ns=82945300 last_materialized_sequence=175617 last_scheduled_sequence=175617 last_submitted_sequence=175617 last_committed_sequence=175617 shared_scope_charged_datagrams=175617 shared_scope_charged_wire_bytes=208966836 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2154 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2154 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=188010 endpoint_1_target_effective_socket_bytes=188010 endpoint_1_api_requested_socket_bytes=188010 endpoint_1_effective_socket_bytes=188010 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=175617 endpoint_1_committed_payload_bytes=204049560
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=929, threads=81, processThreads=130, workers=0, workerProgress=565726, workerProcessCalls=2103716, workerWaits=1537973, workerWakeups=1515150, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=926, logicalProcessors=22, averageSystemMachineCpuPercent=8.254607, averageProcessMachineCpuPercent=1.627690, peakProcessMachineCpuPercent=5.906314, averageProcessSingleCoreCpuPercent=35.809169, peakProcessSingleCoreCpuPercent=129.938900, initialWorkingSetBytes=211259392, workingSetBytes=264744960, peakWorkingSetBytes=273571840, totalPushed=514205, totalPopped=514169, droppedBuffers=0, encodedPacketsPushed=99899, encodedPacketsPopped=99899, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=17048733, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=743, graphPayloadReservations=257190, graphPayloadReleases=257190, graphPayloadPressureFailures=0, lastInputReceivedAtNs=14037663596800
[2026-10-23 12:36:51.466] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1 source_master_ns=5654224351 raw_phase_ns=6804 filtered_phase_ns=6804 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-10-23 12:41:18.079] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=5096 source_master_ns=272740443154 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5260136
~~~~

## 清理

提升权限CIM已确认8个精确测试PID均无残留。以下本轮及旧VLC补生成产物逐项清理：
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r313-native-2026-10-23-12h37m21s646.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r313-native-2026-10-23-12h38m20s205.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r314-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r314-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r314-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r314-native-2026-10-23-12h39m14s284.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r314-native-2026-10-23-12h39m54s938.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r314-native-2026-10-23-12h41m37s959.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r314-reentry-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r314-reentry.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r314-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r314-source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r314-source1-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r314-source1.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r314-source2-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r314-source2.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r314-vlc-restarted.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r314-vlc.log

已清理18项，剩余0。复用120秒源保留；未产生远程测试产物。
