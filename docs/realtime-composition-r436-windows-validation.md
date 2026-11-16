# r436：Windows共享合屏代码回归

状态：本轮Windows四路原规格、全失源、所选音视频源重入与caller-stop通过；整体可靠性与内容A/V仍未关闭。源码5b5565d4（最新文档提交403cf739），Windows FFmpeg保持原样，只运行realtime。四路原120秒H264/H264/H264+AAC/HEVC，2GiB准备预算、输出MPEG-TS/UDP HEVC1280×720/30fps/8Mbps CBR/GOP60及AAC192kbps，沿原规格核验。以下六条已实际执行，PID与结果见后文。

~~~powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r436-source0.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r436-source0.log' -PassThru; Write-Output "SOURCE0_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE0_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r436-source1.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r436-source1.log' -PassThru; Write-Output "SOURCE1_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE1_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r436-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r436-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE2_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61050?rtcpport=61051&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r436-source3.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r436-source3.log' -PassThru; Write-Output "SOURCE3_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE3_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r436 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --inputs "--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r436-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r436-cli-error.log' -PassThru; Write-Output "CLI_PID=$($p.Id)"; while(-not $p.WaitForExit(500)){ $p.Refresh(); Write-Output "CLI_SAMPLE utc=$([DateTime]::UtcNow.ToString('o')) cpu_ms=$($p.TotalProcessorTime.TotalMilliseconds) working_set=$($p.WorkingSet64) private=$($p.PrivateMemorySize64)" }; $p.WaitForExit(); Write-Output "CLI_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r436-vlc.log --extraintf=rc --rc-host=127.0.0.1:42120 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r436-native- --snapshot-format=png udp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"
~~~

## 构建与启动

VS2026 Release clean-first all-target全核心重建成功，configure/build exit0；产物14:07:51，realtime SHA256 b6662352721ce40554370a24beed05dd48a25469c9b0432a6ed95ff6cc1c73f5，必需cache/ninja/两个CLI产物存在，未运行local CLI。初始source0/1/2/3 PID25048/28960/13924/14208，CLI32116、VLC41572。VLC实际命令额外Wait-Process保持父会话，其他六条按上文执行。

原生快照composition-r436-native-2026-11-16-14h09m29s662.png已人工查看四格实际画面。随后四源自然结束后确认全黑，所选source2按同一120秒源完整重入，再caller-stop核验，结果见终态。

## 所选音视频源重入

初始四源均3600帧完整结束且exit0。14:11:17原生快照全黑，CLI仍持续输出。重入PID13400，实际执行：
~~~powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r436-reentry.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r436-reentry.log' -PassThru; Write-Output "REENTRY_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "REENTRY_EXIT=$($p.ExitCode)"; exit $p.ExitCode
~~~
14:12:03原生快照确认仅左下所选源恢复，generation2，随后完整运行120秒并自然结束。

## 终态与边界

五次源运行均完整3600帧/120秒（HEVC日志119.90秒）并exit0；14:13:47第四张原生快照确认再次全黑。CLI32116在核exe/命令行及独占控制台后正常Ctrl+C停止，exit0/caller_stop，workers/errors0，预约释放322785一致且归零。VLC四次原生快照已人工检查四格→全黑→左下恢复→全黑；并不证明内容音画同步或线端交付。

工作集样本数927，范围232759296–298045440B，最后100次292130816–292315136B。父进程最后private约3110707200B，不等于2GiB引擎准备存储；本轮不据此宣称物理/GPU内存硬上限成立。

~~~text
[2026-11-16 14:14:09.204] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=32656 committed_datagrams=180146 committed_payload_bytes=209367516 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=79296 pacing_reserved=180146 pacing_cancelled=0 pacing_submitted=180146 pacing_maximum_submit_lateness_ns=19821566 pacing_worst_late_global_sequence=156297 pacing_worst_success_sample_available=1 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=79296 backlog_max_residence_ns=55113400 last_materialized_sequence=180146 last_scheduled_sequence=180146 last_submitted_sequence=180146 last_committed_sequence=180146 submitted_service_generation=1 submitted_service_sequence=180146 submitted_service_completed_ns=307890480500 submitted_service_duration_ns=34560 submitted_service_available_ns=307890515060 submitted_scope_available_ns=307890515060 shared_scope_charged_datagrams=180146 shared_scope_charged_wire_bytes=214411604 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2154 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2154 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=188010 endpoint_1_target_effective_socket_bytes=188010 endpoint_1_api_requested_socket_bytes=188010 endpoint_1_effective_socket_bytes=188010 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=180146 endpoint_1_committed_payload_bytes=209367516
[2026-11-16 14:14:09.719] [info] [graph][runtime.lifecycle] stop.done state=Stopped
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=1940, threads=101, processThreads=170, workers=0, workerProgress=585455, workerProcessCalls=3246339, workerWaits=2660866, workerWakeups=2637885, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=925, logicalProcessors=22, averageSystemMachineCpuPercent=9.003303, averageProcessMachineCpuPercent=2.070061, peakProcessMachineCpuPercent=9.626719, averageProcessSingleCoreCpuPercent=45.541352, peakProcessSingleCoreCpuPercent=211.787819, initialWorkingSetBytes=232759296, workingSetBytes=292315136, peakWorkingSetBytes=298045440, totalPushed=518881, totalPopped=518556, droppedBuffers=0, encodedPacketsPushed=101322, encodedPacketsPopped=101322, backpressureItems=146, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=21312112, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=1541, graphPayloadReservations=322785, graphPayloadReleases=322785, graphPayloadPressureFailures=0, lastInputReceivedAtNs=19881117654200
[2026-11-16 14:13:32.575] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=5100 source_master_ns=271896726205 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=200705 effective_output_sample_index=5419035
STDERR
[in @ 0000022601581440] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000022601581440] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 5483.626922
[in @ 000002260132b540] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002260132b540] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 21992.318044
[in @ 000002260713fcc0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002260713fcc0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 33140.695133
[in @ 000002260713f340] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002260713f340] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 33445.162956
[hevc @ 0000022600a7aa00] Could not find ref with POC 44
[hevc @ 0000022600a7aa00] Error constructing the frame RPS.
[hevc @ 0000022600a7aa00] Skipping invalid undecodable NALU: 0
[hevc @ 0000022600a7aa00] Could not find ref with POC 46
[hevc @ 0000022600a7aa00] Error constructing the frame RPS.
[hevc @ 0000022600a7aa00] Skipping invalid undecodable NALU: 0
[hevc @ 0000022600a7aa00] Could not find ref with POC 48
[hevc @ 0000022600a7aa00] Error constructing the frame RPS.
[hevc @ 0000022600a7aa00] Skipping invalid undecodable NALU: 1
[hevc @ 0000022600a7aa00] Could not find ref with POC 48
[hevc @ 0000022600a7aa00] Error constructing the frame RPS.
[hevc @ 0000022600a7aa00] Skipping invalid undecodable NALU: 1
[hevc @ 0000022600a7aa00] Could not find ref with POC 48
[hevc @ 0000022600a7aa00] Error constructing the frame RPS.
[hevc @ 0000022600a7aa00] Skipping invalid undecodable NALU: 0
[hevc @ 0000022600a7aa00] Could not find ref with POC 50
[hevc @ 0000022600a7aa00] Error constructing the frame RPS.
[hevc @ 0000022600a7aa00] Skipping invalid undecodable NALU: 0
[hevc @ 0000022600a7aa00] Could not find ref with POC 52
[hevc @ 0000022600a7aa00] Error constructing the frame RPS.
[hevc @ 0000022600a7aa00] Skipping invalid undecodable NALU: 1
[hevc @ 0000022600a7aa00] Could not find ref with POC 52
[hevc @ 0000022600a7aa00] Error constructing the frame RPS.
[hevc @ 0000022600a7aa00] Skipping invalid undecodable NALU: 1
[hevc @ 0000022600a7aa00] Could not find ref with POC 52
[hevc @ 0000022600a7aa00] Error constructing the frame RPS.
[hevc @ 0000022600a7aa00] Skipping invalid undecodable NALU: 0
[hevc @ 0000022600a7aa00] Could not find ref with POC 54
[hevc @ 0000022600a7aa00] Error constructing the frame RPS.
[hevc @ 0000022600a7aa00] Skipping invalid undecodable NALU: 0
[hevc @ 0000022600a7aa00] Could not find ref with POC 56
[hevc @ 0000022600a7aa00] Error constructing the frame RPS.
[hevc @ 0000022600a7aa00] Skipping invalid undecodable NALU: 1
[hevc @ 0000022600a7aa00] Could not find ref with POC 56
[hevc @ 0000022600a7aa00] Error constructing the frame RPS.
[hevc @ 0000022600a7aa00] Skipping invalid undecodable NALU: 1
[hevc @ 0000022600a7aa00] Could not find ref with POC 56
[hevc @ 0000022600a7aa00] Error constructing the frame RPS.
[hevc @ 0000022600a7aa00] Skipping invalid undecodable NALU: 0
[in @ 0000022606a8f380] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000022606a8f380] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 33445.162956
[in @ 00000226010d9d00] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000226010d9d00] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 21992.318044
[in @ 000002266b1facc0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002266b1facc0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 5483.626922
[in @ 000002260c94dec0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002260c94dec0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 33140.695133
[aac @ 0000022600a7d640] Qavg: 16722.525

~~~

实际正常停止命令（未强杀CLI）：
~~~powershell
$target=Get-CimInstance Win32_Process -Filter "ProcessId=32116"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r436*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R436Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R436Console]::FreeConsole() | Out-Null
if(-not [R436Console]::AttachConsole(32116)){throw 'AttachConsole failed'}
try {
[R436Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R436Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 32116 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R436Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R436Console]::FreeConsole() | Out-Null}

~~~

## 清理

VLC以RC42120 quit退出，七个本轮PID均核验不存在。记录后按以下17项逐项清理，保留用户源与正式构建产物：

- composition-r436-cli-error.log
- composition-r436-cli.log
- composition-r436-native-2026-11-16-14h09m29s662.png
- composition-r436-native-2026-11-16-14h11m17s115.png
- composition-r436-native-2026-11-16-14h12m03s519.png
- composition-r436-native-2026-11-16-14h13m47s309.png
- composition-r436-reentry.log
- composition-r436-reentry.out
- composition-r436-source0.log
- composition-r436-source0.out
- composition-r436-source1.log
- composition-r436-source1.out
- composition-r436-source2.log
- composition-r436-source2.out
- composition-r436-source3.log
- composition-r436-source3.out
- composition-r436-vlc.log

清理后composition-r436前缀文件数为0，未留下本轮临时脚本或测试设施。
