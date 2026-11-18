# r312：前批与预约完成关联诊断复验（已执行，场景完成；完整验收未闭合）

原规格三源120秒→失源全黑→AV源120秒重入；H264转HEVC 1280×720/30 8000kbps CBR/GOP60，AAC192kbps CBR/44100双声道，MPEG-TS/UDP→VLC。仅VLC原生快照。诊断双审PASS不代表问题修复。

## 初始执行命令
~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r312-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r312-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r312-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r312-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r312-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r312-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r312 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62020 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r312-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r312-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r312-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r312-native- --snapshot-format=png udp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r312-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

## 重入与正常停止命令
~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r312-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r312-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($source0.Id)"
$source0.WaitForExit()
Write-Output "REENTRY_EXIT=$($source0.ExitCode)"
~~~~
~~~~powershell
$ErrorActionPreference='Stop'
$target=Get-CimInstance Win32_Process -Filter "ProcessId=4348"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r312*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R312Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R312Console]::FreeConsole() | Out-Null
if(-not [R312Console]::AttachConsole(4348)){throw 'AttachConsole failed'}
try {
[R312Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R312Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 4348 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R312Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R312Console]::FreeConsole() | Out-Null}
~~~~

## 结果与边界
Release全量739项exit0，产物11:38:08；4文件诊断双审PASS。初始源29268/40088/32656，重入40608，均完整120秒自然exit0。CLI4348调用方停止exit0，非源EOF自动退出；VLC31540。原生快照依次确认三格、全黑、第三格重入、再次全黑。未发生本轮deadline错误或崩溃，但不证明历史问题修复；没有线速到达证据及VLC音频测量，因此不记完整验收通过。内部generation2已出现。

## 关键遥测
~~~~text
[2026-10-23 11:43:47.369] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 datagrams=0 wire_bytes=0 immediate_deadlines=0 deferred_deadlines=0 planned_wait_ns=0 late_datagrams=0 maximum_lateness_ns=0 pending_bytes=0 peak_pending_bytes=110920 pressure_failures=0 access_units=22416 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=6134 maximum_mux_input_after_emit_ns=12939399 maximum_mux_input_after_dispatch_ns=-87060601 worst_mux_input_ready_ns=21495574500 worst_mux_input_emit_ns=21482635101 worst_mux_input_dispatch_ns=21582635101 maximum_protocol_batch_produced_after_release_ns=13078480 worst_protocol_batch_produced_ns=21495713900 worst_protocol_batch_release_ns=21482635420 worst_protocol_batch_deadline_ns=21582635101 worst_protocol_batch_datagrams=20 exit_reason=completed
[2026-10-23 11:43:47.397] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=33163 committed_datagrams=176589 committed_payload_bytes=204299976 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=79296 pacing_reserved=176589 pacing_cancelled=0 pacing_submitted=176589 pacing_maximum_submit_lateness_ns=15015144 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=79296 backlog_max_residence_ns=54234500 last_materialized_sequence=176589 last_scheduled_sequence=176589 last_submitted_sequence=176589 last_committed_sequence=176589 shared_scope_charged_datagrams=176589 shared_scope_charged_wire_bytes=209244468 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2154 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2154 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=188010 endpoint_1_target_effective_socket_bytes=188010 endpoint_1_api_requested_socket_bytes=188010 endpoint_1_effective_socket_bytes=188010 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=176589 endpoint_1_committed_payload_bytes=204299976
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=916, threads=81, processThreads=130, workers=0, workerProgress=572490, workerProcessCalls=2119778, workerWaits=1547271, workerWakeups=1523807, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=997, logicalProcessors=22, averageSystemMachineCpuPercent=10.284860, averageProcessMachineCpuPercent=1.491333, peakProcessMachineCpuPercent=7.171315, averageProcessSingleCoreCpuPercent=32.809324, peakProcessSingleCoreCpuPercent=157.768924, initialWorkingSetBytes=212221952, workingSetBytes=266661888, peakWorkingSetBytes=272498688, totalPushed=520093, totalPopped=520057, droppedBuffers=0, encodedPacketsPushed=102882, encodedPacketsPopped=102882, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=17164314, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=742, graphPayloadReservations=261506, graphPayloadReleases=261506, graphPayloadPressureFailures=0, lastInputReceivedAtNs=10557678499600
[2026-10-23 11:38:40.562] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1 source_master_ns=5651751483 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-10-23 11:43:18.089] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=5096 source_master_ns=283649554054 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5260136
~~~~
## 临时产物清单
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r312-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r312-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r312-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r312-native-2026-10-23-11h39m05s846.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r312-native-2026-10-23-11h40m52s052.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r312-native-2026-10-23-11h41m53s997.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r312-native-2026-10-23-11h43m33s588.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r312-reentry-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r312-reentry.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r312-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r312-source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r312-source1-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r312-source1.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r312-source2-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r312-source2.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r312-vlc.log
清理完成：按上述清单删除16项临时产物，六个精确PID无残留，指定复用源保留。未启动远程测试。
