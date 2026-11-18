# r311：代际退休修复重入复验（已执行，初始链路超期失败）

三源H264→HEVC 1280×720/30、8000kbps CBR/GOP60，AAC192kbps CBR/44100双声道，MPEG-TS/UDP→VLC。指定源完整120秒自然结束后观察全黑，再启动同规格AV源完整120秒重入；VLC仅原生快照。须最新Release构建和双复审通过。旧崩溃、r310截止及完整TS存储仍未闭合。

## 初始执行命令
~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r311-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r311-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r311-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r311-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r311-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r311-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r311 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62020 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r311-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r311-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r311-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r311-native- --snapshot-format=png udp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r311-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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
9文件退休切片双复审PASS，Release739项exit0，11:24:58产物与cache/Ninja/两CLI核验。源PID10640/15704/24896，CLI29824 exit1，VLC36284；约21秒失败，未进入完整120秒/失源/重入，不能验收通过。快照请求在CLI报错后完成，不作为持续画面验收证据。

当前batch首次attempt即超期：mux出队20965869500ns，materializer首次出队21036122800ns，间隔70253300ns；截止21032089544ns，尝试21036123700ns，晚4034156ns。自身blocked_attempts=0，不能把这次直接等同r310的当前batch重试；需要前一batch等待/完成与节点排队关联。最终payload0B/0objects，预约释放20009一致，errors1，pressureFailures0。CPU平均2.495468%峰值6.490872%，WS峰值268959744B。
~~~~text
[2026-10-23 11:25:42.109] [info] [graph][runtime.node] protocol_batch_trace stage=materialization_failed generation=1 attempts=1 consumed_datagrams=0 remaining_datagrams=1 attempted_master_ns=21036123700 mux_dequeued_master_ns=20965869500 first_dequeued_master_ns=21036122800 last_wire_published_master_ns=21036108000 previous_attempt_started_master_ns=unavailable previous_attempt_completed_master_ns=unavailable last_attempt_completed_master_ns=21036136300 last_blocked_started_master_ns=unavailable last_blocked_completed_master_ns=unavailable blocked_attempts=0 remaining_front_release_master_ns=20965422876 remaining_front_deadline_master_ns=21032089544 original_error="InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=20965422876 materialized_ns=21036123700 protocol_deadline_ns=21032089544 deadline_before_release=0 materialized_after_deadline=1"
[2026-10-23 11:25:42.124] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=1669 committed_datagrams=11070 committed_payload_bytes=13298368 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=79296 pacing_reserved=11070 pacing_cancelled=0 pacing_submitted=11070 pacing_maximum_submit_lateness_ns=18278060 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=79296 backlog_max_residence_ns=85237200 last_materialized_sequence=11070 last_scheduled_sequence=11070 last_submitted_sequence=11070 last_committed_sequence=11070 shared_scope_charged_datagrams=11070 shared_scope_charged_wire_bytes=13608328 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2154 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2154 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=188010 endpoint_1_target_effective_socket_bytes=188010 endpoint_1_api_requested_socket_bytes=188010 endpoint_1_effective_socket_bytes=188010 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=11070 endpoint_1_committed_payload_bytes=13298368
[2026-10-23 11:25:42.151] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 datagrams=0 wire_bytes=0 immediate_deadlines=0 deferred_deadlines=0 planned_wait_ns=0 late_datagrams=0 maximum_lateness_ns=0 pending_bytes=0 peak_pending_bytes=110920 pressure_failures=0 access_units=1131 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=308 maximum_mux_input_after_emit_ns=22317632 maximum_mux_input_after_dispatch_ns=-77682368 worst_mux_input_ready_ns=20987740200 worst_mux_input_emit_ns=20965422568 worst_mux_input_dispatch_ns=21065422568 maximum_protocol_batch_produced_after_release_ns=22387324 worst_protocol_batch_produced_ns=20987810200 worst_protocol_batch_release_ns=20965422876 worst_protocol_batch_deadline_ns=21065422568 worst_protocol_batch_datagrams=23 exit_reason=aborted
[2026-10-23 11:25:42.162] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=between_calls_previous_return count=3336 maximum_ns=122716000 begin_steady_ns=9485364787100 end_steady_ns=9485487503100 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=waiting deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[CLI] final runtime report: queued=0, peakQueued=932, threads=81, processThreads=131, workers=0, workerProgress=47358, workerProcessCalls=181753, workerWaits=134388, workerWakeups=133130, workerErrors=1, errors=1, stalledIntervals=0, cpuSamples=47, logicalProcessors=22, averageSystemMachineCpuPercent=30.778266, averageProcessMachineCpuPercent=2.495468, peakProcessMachineCpuPercent=6.490872, averageProcessSingleCoreCpuPercent=54.900298, peakProcessSingleCoreCpuPercent=142.799189, initialWorkingSetBytes=211877888, workingSetBytes=268886016, peakWorkingSetBytes=268959744, totalPushed=43992, totalPopped=43347, droppedBuffers=0, encodedPacketsPushed=5193, encodedPacketsPopped=5192, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=17156736, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=744, graphPayloadReservations=20009, graphPayloadReleases=20009, graphPayloadPressureFailures=0, lastInputReceivedAtNs=9501027670400
[2026-10-23 11:25:40.550] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=623 source_master_ns=20110856379 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=680190
[2026-10-23 11:25:41.596] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=668 source_master_ns=21155754338 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=725536
[in @ 0000024a11cd3480] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000024a11cd3480] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 32423.892011
[in @ 0000024a0b6ad100] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000024a0b6ad100] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 44379.724922
[in @ 0000024a0b6adb80] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000024a0b6adb80] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 5393.961389
[in @ 0000024a7c7a3700] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000024a16153c40] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000024a16151500] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000024a16151500] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 5393.961389
[in @ 0000024a16153c40] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 32423.892011
[in @ 0000024a7c7a3700] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 44379.724922
[aac @ 0000024a11cf0ac0] Qavg: 661.882
[aac @ 0000024a11cf0ac0] 2 frames left in the queue on closing
[CLI] realtime video runtime failed: InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=20965422876 materialized_ns=21036123700 protocol_deadline_ns=21032089544 deadline_before_release=0 materialized_after_deadline=1

~~~~
## 临时产物清单
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r311-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r311-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r311-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r311-native-2026-10-23-11h25m42s890.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r311-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r311-source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r311-source1-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r311-source1.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r311-source2-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r311-source2.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r311-vlc.log

清理完成：五个精确PID无残留，按清单删除11项临时产物，保留指定120秒源；源进程为测试中止清理，非自然退出。未启动重入源或远程测试。
