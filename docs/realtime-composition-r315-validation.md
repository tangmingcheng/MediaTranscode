# r315：原规格完整画面与发送诊断复验（失败）

三路120秒RTP H264/AAC→HEVC CBR8000kbps 1280×720/30、AAC目标192kbps→MPEG-TS/UDP。三格→全黑→AV120秒重入→全黑；只用VLC原生快照。不代表完整合屏矩阵或历史超期修复。

## 实际启动命令
~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r315-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r315-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r315-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r315-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r315-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r315-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r315 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62020 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r315-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r315-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r315-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r315-native- --snapshot-format=png udp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r315-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

启动前提升权限端口检查无占用。源PID37936/1104/33992，CLI24844，VLC4008；RC63020已核对归属4008。监控会话64272。12:43:42原生快照已人工查看：三格正常、第四格黑色。

## AV重入

初始三个源PID已自然消失，12:45:34原生快照已确认全黑。重入PID30384，会话64042。实际命令：
~~~~powershell
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r315-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r315-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($source0.Id)"
$source0.WaitForExit()
Write-Output "REENTRY_EXIT=$($source0.ExitCode)"
~~~~

## 失败结果及诊断

CLI退出1，初始三源均0。AV重入阶段失败，尚未完成重入/最终黑屏验证；重入源30384和VLC4008在保存证据后按精确PID清理，不能记为自然结束。已确认三格和初始失源全黑。

阻塞head16254序号87334–87374，与sender last_completed精确对应；首末提交间52.8605ms。sequence87338最大start lateness14.498660ms，wait开始155837768700、返回155852477500ns；实际submit观察155852478100ns。最大submit interval仅271200ns，但不能归因全部延迟；尚缺timer具体唤醒/OS调度证据。head自身deadline155912933101，后批deadline155879602776，更早约33.33ms；head最后提交155882283900时后批已超期2.681124ms。完成后21us即尝试，不能解释为完成后的长时间丢唤醒。

~~~~text
[2026-10-23 12:45:44.176] [info] [graph][runtime.node] protocol_batch_trace stage=materialization_failed generation=1 attempts=8 consumed_datagrams=0 remaining_datagrams=1 attempted_master_ns=155882304900 mux_dequeued_master_ns=155810774500 first_dequeued_master_ns=155829285000 last_wire_published_master_ns=155829276200 previous_attempt_started_master_ns=155879117600 previous_attempt_completed_master_ns=155879121800 last_attempt_completed_master_ns=155882319500 last_blocked_started_master_ns=155879117600 last_blocked_completed_master_ns=155879121800 blocked_attempts=7 last_blocked_code=WouldBlock last_blocked_native=0 last_blocked_original_message_bytes=291 last_blocked_message_truncated=0 last_blocked_message="wire global sequence reservation exceeds planner backlog capacity serial=1 count=0 bytes=0 request_count=1 request_bytes=216 used_count=4 used_bytes=5000 limit_count=2154 limit_bytes=2894412 generation=1 projected_sequence=87375 head_id=16254 head_first=87334 head_count=41 head_committed=37" previous_batch_first_dequeued_master_ns=155822575400 previous_batch_last_attempt_started_master_ns=155829234200 previous_batch_last_attempt_completed_master_ns=155829268500 previous_batch_last_blocked_started_master_ns=155824202800 previous_batch_last_blocked_completed_master_ns=155824215700 previous_batch_final_wire_published_master_ns=155829276200 previous_batch_attempts=3 previous_batch_blocked_attempts=2 previous_batch_observed_completed_reservation_id=16253 previous_batch_observed_completed_reservation_first_sequence=87333 previous_batch_observed_completed_reservation_last_sequence=87333 previous_batch_observed_completed_reservation_submitted_master_ns=155829119300 previous_batch_last_blocked_native=0 previous_batch_last_blocked_original_message_bytes=291 previous_batch_last_blocked_message_truncated=0 previous_batch_last_blocked_message="wire global sequence reservation exceeds planner backlog capacity serial=1 count=0 bytes=0 request_count=41 request_bytes=54728 used_count=1 used_bytes=216 limit_count=2154 limit_bytes=2894412 generation=1 projected_sequence=87334 head_id=16253 head_first=87333 head_count=1 head_committed=0" global_last_completed_reservation_id=16254 global_last_completed_reservation_first_sequence=87334 global_last_completed_reservation_last_sequence=87374 global_last_completed_reservation_submitted_master_ns=155882283900 remaining_front_release_master_ns=155812936108 remaining_front_deadline_master_ns=155879602776 original_error="InvalidArgument: MPEG-TS UDP prepared transaction exceeded its immutable emission deadline"
[2026-10-23 12:45:44.191] [info] [graph][runtime.node] datagram_batch_trace stage=last_completed generation=1 first_sequence=87334 last_sequence=87374 maximum_submit_interval_ns=271200 maximum_submit_interval_sequence=87348 maximum_start_lateness_ns=14498660 maximum_start_lateness_sequence=87338 first_release_master_ns=155812933101 first_deadline_master_ns=155912933101 last_release_master_ns=155812933101 last_deadline_master_ns=155912933101 first_submitted_master_ns=155829423400 last_submitted_master_ns=155882283900 worst_wait_started_master_ns=155837768700 worst_wait_completed_master_ns=155852477500 worst_submit_observed_master_ns=155852478100
[2026-10-23 12:45:44.191] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=16254 committed_datagrams=87374 committed_payload_bytes=101293460 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=79296 pacing_reserved=87374 pacing_cancelled=0 pacing_submitted=87374 pacing_maximum_submit_lateness_ns=20757560 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=79296 backlog_max_residence_ns=74871300 last_materialized_sequence=87374 last_scheduled_sequence=87374 last_submitted_sequence=87374 last_committed_sequence=87374 shared_scope_charged_datagrams=87374 shared_scope_charged_wire_bytes=103739932 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2154 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2154 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=188010 endpoint_1_target_effective_socket_bytes=188010 endpoint_1_api_requested_socket_bytes=188010 endpoint_1_effective_socket_bytes=188010 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=87374 endpoint_1_committed_payload_bytes=101293460
[CLI] final runtime report: queued=0, peakQueued=957, threads=81, processThreads=129, workers=0, workerProgress=346914, workerProcessCalls=1310656, workerWaits=963734, workerWakeups=952183, workerErrors=1, errors=1, stalledIntervals=0, cpuSamples=463, logicalProcessors=22, averageSystemMachineCpuPercent=9.568210, averageProcessMachineCpuPercent=1.852754, peakProcessMachineCpuPercent=6.167401, averageProcessSingleCoreCpuPercent=40.760581, peakProcessSingleCoreCpuPercent=135.682819, initialWorkingSetBytes=213360640, workingSetBytes=257384448, peakWorkingSetBytes=274444288, totalPushed=316525, totalPopped=316476, droppedBuffers=0, encodedPacketsPushed=50448, encodedPacketsPopped=50448, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=18793058, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=787, graphPayloadReservations=148420, graphPayloadReleases=148420, graphPayloadPressureFailures=0, lastInputReceivedAtNs=14303079602000
~~~~

## 清理清单

D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r315-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r315-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r315-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r315-native-2026-10-23-12h43m42s328.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r315-native-2026-10-23-12h45m34s763.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r315-reentry-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r315-reentry.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r315-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r315-source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r315-source1-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r315-source1.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r315-source2-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r315-source2.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r315-vlc.log

提升权限CIM确认六个测试PID均无残留；已删除14项产物，剩余0。120秒复用源保留。重入源强制清理退出，不是自然结束。
