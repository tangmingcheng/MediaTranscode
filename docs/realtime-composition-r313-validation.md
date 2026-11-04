# r313：TS series 实际存储接线复验（失败）

三源原120秒RTP H264/AAC→HEVC CBR8000kbps 1280×720/30、AAC CBR192kbps→MPEG-TS/UDP；三格→全黑→AV源原120秒重入→全黑，VLC原生快照。完整TS预算、历史超期/崩溃及全合屏仍未闭合。

## 初始执行命令
~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r313-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r313-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r313-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r313-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r313-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r313-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r313 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62020 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r313-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r313-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r313-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r313-native- --snapshot-format=png udp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r313-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

## 实际结果

初始源PID：41048、41388、33972；CLI34556；VLC39068；AV重入源39972。初始三个源及重入源退出码均0；CLI退出码1（会话46144、59186已取回），不通过。VLC原生快照已观察三格、全黑、AV重入；重入结束后的全黑未完成验证。重入复用上方source0完整参数，仅日志前缀替换为composition-r313-reentry。

前批reservation26282最后提交248282654500ns，下一批期限248280964832ns，相差1689668ns；下一次materialization尝试248282681400ns，晚1716568ns。serial门禁阻塞期间余量11个datagram/14220字节，未达到全局容量上限；不能直接判定丢唤醒或根因已修复。无须WER即可获得此次失败证据。

~~~~text
[2026-10-23 12:26:09.292] [info] [graph][runtime.node] protocol_batch_trace stage=materialization_failed generation=1 attempts=10 consumed_datagrams=0 remaining_datagrams=1 attempted_master_ns=248282681400 mux_dequeued_master_ns=248198627600 first_dequeued_master_ns=248215092900 last_wire_published_master_ns=248215085000 previous_attempt_started_master_ns=248271872400 previous_attempt_completed_master_ns=248271888800 last_attempt_completed_master_ns=248282687100 last_blocked_started_master_ns=248271872400 last_blocked_completed_master_ns=248271888800 blocked_attempts=9 last_blocked_code=WouldBlock last_blocked_native=0 last_blocked_original_message_bytes=295 last_blocked_message_truncated=0 last_blocked_message="wire global sequence reservation exceeds planner backlog capacity serial=1 count=0 bytes=0 request_count=1 request_bytes=216 used_count=11 used_bytes=14220 limit_count=2154 limit_bytes=2894412 generation=1 projected_sequence=154286 head_id=26282 head_first=154261 head_count=25 head_committed=14" previous_batch_first_dequeued_master_ns=248214898200 previous_batch_last_attempt_started_master_ns=248215072100 previous_batch_last_attempt_completed_master_ns=248215078200 previous_batch_last_blocked_started_master_ns=248214899100 previous_batch_last_blocked_completed_master_ns=248214976200 previous_batch_final_wire_published_master_ns=248215085000 previous_batch_attempts=2 previous_batch_blocked_attempts=1 previous_batch_observed_completed_reservation_id=26281 previous_batch_observed_completed_reservation_first_sequence=154260 previous_batch_observed_completed_reservation_last_sequence=154260 previous_batch_observed_completed_reservation_submitted_master_ns=248215028800 previous_batch_last_blocked_native=0 previous_batch_last_blocked_original_message_bytes=293 previous_batch_last_blocked_message_truncated=0 previous_batch_last_blocked_message="wire global sequence reservation exceeds planner backlog capacity serial=1 count=0 bytes=0 request_count=25 request_bytes=33036 used_count=1 used_bytes=216 limit_count=2154 limit_bytes=2894412 generation=1 projected_sequence=154261 head_id=26281 head_first=154260 head_count=1 head_committed=0" global_last_completed_reservation_id=26282 global_last_completed_reservation_first_sequence=154261 global_last_completed_reservation_last_sequence=154285 global_last_completed_reservation_submitted_master_ns=248282654500 remaining_front_release_master_ns=248214298164 remaining_front_deadline_master_ns=248280964832 original_error="InvalidArgument: MPEG-TS UDP prepared transaction exceeded its immutable emission deadline"
[2026-10-23 12:26:09.298] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=26282 committed_datagrams=154285 committed_payload_bytes=181657068 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=79296 pacing_reserved=154285 pacing_cancelled=0 pacing_submitted=154285 pacing_maximum_submit_lateness_ns=22621260 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=79296 backlog_max_residence_ns=69124300 last_materialized_sequence=154285 last_scheduled_sequence=154285 last_submitted_sequence=154285 last_committed_sequence=154285 shared_scope_charged_datagrams=154285 shared_scope_charged_wire_bytes=185977048 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2154 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2154 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=188010 endpoint_1_target_effective_socket_bytes=188010 endpoint_1_api_requested_socket_bytes=188010 endpoint_1_effective_socket_bytes=188010 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=154285 endpoint_1_committed_payload_bytes=181657068
[CLI] final runtime report: queued=0, peakQueued=876, threads=81, processThreads=129, workers=0, workerProgress=501905, workerProcessCalls=1902897, workerWaits=1400985, workerWakeups=1382311, workerErrors=1, errors=1, stalledIntervals=0, cpuSamples=756, logicalProcessors=22, averageSystemMachineCpuPercent=8.034231, averageProcessMachineCpuPercent=1.701284, peakProcessMachineCpuPercent=5.361305, averageProcessSingleCoreCpuPercent=37.428257, peakProcessSingleCoreCpuPercent=117.948718, initialWorkingSetBytes=208134144, workingSetBytes=260997120, peakWorkingSetBytes=269926400, totalPushed=457712, totalPopped=457661, droppedBuffers=0, encodedPacketsPushed=81564, encodedPacketsPopped=81564, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=15936218, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=711, graphPayloadReservations=229196, graphPayloadReleases=229196, graphPayloadPressureFailures=0, lastInputReceivedAtNs=13128199101300
~~~~

## 本轮清理清单

D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r313-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r313-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r313-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r313-native-2026-10-23-12h22m31s219.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r313-native-2026-10-23-12h24m18s581.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r313-native-2026-10-23-12h24m55s222.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r313-reentry-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r313-reentry.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r313-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r313-source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r313-source1-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r313-source1.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r313-source2-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r313-source2.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r313-vlc.log

上述15项临时产物已逐项清理，剩余=0；本机精确PID检查未发现34556/39068/39972。120秒复用源保留。

纠正：上述普通权限进程查询未覆盖交互会话，不能证明PID全部不存在。r314时经提升权限CIM发现VLC39068仍占63020，已核对其r313命令行并强制清理；前述无残留结论撤回。r314首次snapshot请求命中了旧VLC，不算r314画面证据。
