# r499：Windows 四源共享回归（基础生命周期已观察，完整门禁未闭合）

源码86d817e8；VS2026 Release clean-first/all全量重建configure/build exit0，实际24编译进程，未运行local CLI。realtime SHA256 1c0c0066283204c2e8359d3cb4425ae7324c40f03d968a40283556be84e72b6f。本机FFmpeg未修改。

## 实际命令

~~~powershell
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r499-vlc.log --extraintf=rc --rc-host=127.0.0.1:42120 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r499-native- --snapshot-format=png rtp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"
~~~

~~~powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r499-source0.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r499-source0.log' -PassThru; Write-Output "SOURCE0_PID=$($p.Id)"
~~~

~~~powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r499-source1.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r499-source1.log' -PassThru; Write-Output "SOURCE1_PID=$($p.Id)"
~~~

~~~powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -vf "drawtext=fontfile=''C\:/Windows/Fonts/consola.ttf'':text=''frame=%{frame_num} pts=%{pts}'':fontsize=48:fontcolor=white:box=1:boxcolor=black:x=16:y=16" -c:v h264_nvenc -rc cbr -b:v 8200k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r499-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r499-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"
~~~

~~~powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61050?rtcpport=61051&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r499-source3.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r499-source3.log' -PassThru; Write-Output "SOURCE3_PID=$($p.Id)"
~~~

~~~powershell
$p=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r499 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --inputs "--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62024 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r499-output.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r499-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r499-cli-error.log' -PassThru; Write-Output "CLI_PID=$($p.Id)"
~~~

## 现场记录

实际正常停止命令：

~~~powershell
$target=Get-CimInstance Win32_Process -Filter "ProcessId=36280"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r499*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R499Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R499Console]::FreeConsole() | Out-Null
if(-not [R499Console]::AttachConsole(36280)){throw 'AttachConsole failed'}
try {
[R499Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R499Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 36280 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R499Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R499Console]::FreeConsole() | Out-Null}
~~~

VLC15148、源0=36544、源1=39484、源2=29836、源3=34192、CLI36280。后台monitor会话30617监控同一CLI CPU/RSS及退出。初始四路运行无worker error；FFmpeg色彩range变化警告保留，不据此宣称格式问题修复。


首轮四源3600帧/120秒自然结束，四组reason=SourceDrained old1→next2均purge_completed；VLC原生快照确认全黑，CLI同PID继续输出。第二轮同命令仅日志名sourceN加-reentry，PID0=38380、1=5888、2=33380、3=39492；generation2音频drift约9070ns、frequency/stretch0。

## 终态及边界

两轮各4源完整120秒/3600帧。四张VLC原生快照确认四格→全黑→四格恢复→全黑；同一CLI36280未重启。核独占控制台后正常CtrlC停止，日志Stopped/caller_stop/stopped successfully。monitor会话30617只取得进程结束，ExitCode空，未记录数值退出码，不宣称已核验exit0。进程结束并非强杀。

~~~text
[2026-11-18 13:15:52.363] [info] [graph][runtime.node] av_reacquisition group=composition-r499.source.0 phase=purging old=1 next=2 transition=1 reason=4
[2026-11-18 13:15:52.364] [info] [graph][runtime.node] av_reacquisition group=composition-r499.source.0 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=1 next=2 transition=1
[2026-11-18 13:15:58.369] [info] [graph][runtime.node] av_reacquisition group=composition-r499.source.1 phase=purging old=1 next=2 transition=1 reason=4
[2026-11-18 13:15:58.370] [info] [graph][runtime.node] av_reacquisition group=composition-r499.source.1 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=1 next=2 transition=1
[2026-11-18 13:16:04.401] [info] [graph][runtime.node] av_reacquisition group=composition-r499.source.2 phase=purging old=1 next=2 transition=1 reason=4
[2026-11-18 13:16:04.402] [info] [graph][runtime.node] av_reacquisition group=composition-r499.source.2 event=purge_completed purge_ack=complete continuation=acquiring ack_count=2 old=1 next=2 transition=1
[2026-11-18 13:16:10.370] [info] [graph][runtime.node] av_reacquisition group=composition-r499.source.3 phase=purging old=1 next=2 transition=1 reason=4
[2026-11-18 13:16:10.371] [info] [graph][runtime.node] av_reacquisition group=composition-r499.source.3 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=1 next=2 transition=1
[2026-11-18 13:18:54.663] [info] [graph][runtime.node] av_reacquisition group=composition-r499.source.0 phase=purging old=2 next=3 transition=2 reason=4
[2026-11-18 13:18:54.664] [info] [graph][runtime.node] av_reacquisition group=composition-r499.source.0 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=2 next=3 transition=2
[2026-11-18 13:19:09.282] [info] [graph][runtime.node] av_reacquisition group=composition-r499.source.1 phase=purging old=2 next=3 transition=2 reason=4
[2026-11-18 13:19:09.282] [info] [graph][runtime.node] av_reacquisition group=composition-r499.source.1 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=2 next=3 transition=2
[2026-11-18 13:19:17.057] [info] [graph][runtime.node] av_reacquisition group=composition-r499.source.2 phase=purging old=2 next=3 transition=2 reason=4
[2026-11-18 13:19:17.058] [info] [graph][runtime.node] av_reacquisition group=composition-r499.source.2 event=purge_completed purge_ack=complete continuation=acquiring ack_count=2 old=2 next=3 transition=2
[2026-11-18 13:19:24.898] [info] [graph][runtime.node] av_reacquisition group=composition-r499.source.3 phase=purging old=2 next=3 transition=2 reason=4
[2026-11-18 13:19:24.898] [info] [graph][runtime.node] av_reacquisition group=composition-r499.source.3 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=2 next=3 transition=2
[2026-11-18 13:20:00.527] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=36754 committed_datagrams=214510 committed_payload_bytes=252734884 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=55 ordered_queue_maximum_wire_bytes=74016 pacing_reserved=214510 pacing_cancelled=0 pacing_submitted=214510 pacing_maximum_submit_lateness_ns=7348980 pacing_worst_late_global_sequence=12913 pacing_worst_success_sample_available=1 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=55 backlog_high_water_wire_bytes=74016 backlog_max_residence_ns=38942500 last_materialized_sequence=214510 last_scheduled_sequence=214510 last_submitted_sequence=214510 last_committed_sequence=214510 submitted_service_generation=1 submitted_service_sequence=214510 submitted_service_completed_ns=344473537500 submitted_service_duration_ns=16640 submitted_service_available_ns=344473554140 submitted_scope_available_ns=344473554140 shared_scope_charged_datagrams=214510 shared_scope_charged_wire_bytes=258741164 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2155 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2155 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=554646 endpoint_1_target_effective_socket_bytes=277323 endpoint_1_api_requested_socket_bytes=277323 endpoint_1_effective_socket_bytes=277323 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=214425 endpoint_1_committed_payload_bytes=252729096 endpoint_2_target_effective_socket_bytes=277323 endpoint_2_api_requested_socket_bytes=277323 endpoint_2_effective_socket_bytes=277323 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=85 endpoint_2_committed_payload_bytes=5788
[2026-11-18 13:20:00.870] [info] [graph][runtime.lifecycle] stop.done state=Stopped
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=1554, threads=102, processThreads=111, workers=0, workerProgress=717642, workerProcessCalls=3196785, workerWaits=2479124, workerWakeups=2453517, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=1089, logicalProcessors=22, averageSystemMachineCpuPercent=11.961783, averageProcessMachineCpuPercent=2.052715, peakProcessMachineCpuPercent=7.789474, averageProcessSingleCoreCpuPercent=45.159734, peakProcessSingleCoreCpuPercent=171.368421, initialWorkingSetBytes=226004992, workingSetBytes=258973696, peakWorkingSetBytes=300445696, totalPushed=644449, totalPopped=644449, droppedBuffers=0, encodedPacketsPushed=114039, encodedPacketsPopped=114039, backpressureItems=147, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=17135342, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=1008, graphPayloadReservations=435825, graphPayloadReleases=435825, graphPayloadPressureFailures=0, lastInputReceivedAtNs=16953812318500
[CLI] realtime video validation stopped successfully
[2026-11-18 13:19:11.035] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4903 source_master_ns=296353512532 raw_phase_ns=9070 filtered_phase_ns=9070 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=200705 effective_output_sample_index=5218330
~~~

本轮共享四源基础流程有真实证据；不覆盖独立音频静音/恢复、内容A/V、NIC出线、C API/旧ABI及长稳；不关闭RKMPP期限故障。无retired_control日志，不能称窄旧控制分支直接覆盖。

清理清单：composition-r499-cli-error.log、composition-r499-cli.log、composition-r499-monitor.log、composition-r499-native-2026-11-18-13h15m01s119.png、composition-r499-native-2026-11-18-13h16m42s526.png、composition-r499-native-2026-11-18-13h17m54s685.png、composition-r499-native-2026-11-18-13h19m54s410.png、composition-r499-output.sdp、composition-r499-source0-reentry.log、composition-r499-source0-reentry.out、composition-r499-source0.log、composition-r499-source0.out、composition-r499-source1-reentry.log、composition-r499-source1-reentry.out、composition-r499-source1.log、composition-r499-source1.out、composition-r499-source2-reentry.log、composition-r499-source2-reentry.out、composition-r499-source2.log、composition-r499-source2.out、composition-r499-source3-reentry.log、composition-r499-source3-reentry.out、composition-r499-source3.log、composition-r499-source3.out、composition-r499-vlc.log。VLC15148按完整命令核对后清理；保留复用源及构建产物。

独立证据审查PASS：四张原生快照、八份源日志及CLI终态核对；数值ExitCode不可用，完整交付未通过。25项本轮临时产物按清单删除，本轮CLI和源PID均无残留，VLC已按精确PID清理。
