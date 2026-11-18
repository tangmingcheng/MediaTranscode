# r345：输出准备分阶段诊断

状态：实测定位到大容量packetizer准备耗时，运行失败；已记录并清理。诊断字段类型修正后双独立复审PASS，Release全量747项configure/build exit0。原r344单源AV参数、原120秒源、无调试器；仅新增output_preparation五scope记录，不改原deadline或容量策略。planned_bytes非实际内存高水，nested时间不能相加。

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r345-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r345-source0.log' -PassThru
Write-Output "SOURCE_PID=$($source0.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r345 --input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62024 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r345-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r345-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r345-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r345-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r345-native- --snapshot-format=png rtp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r345-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
$lastTime=$time
$lastCpu=$cpu
}
Write-Output "CLI_EXIT=$($cli.ExitCode)"
$source0.WaitForExit()
Write-Output "SOURCE0_EXIT=$($source0.ExitCode)"
exit $cli.ExitCode
~~~~

## 实测失败与定位

Release realtime6512128字节/2026-10-26 12:44:39；源41868、CLI21660。CLI正常exit1，尚无VLC/快照，尚未源切换。transport_activate0.1979ms、global_sequence0.0421ms、transport_buffer1.4713ms；ts_packetizer353.0966ms，父ts_mux379.1029ms。packetizer主导本次已观测准备耗时；父子不可相加。

输入startup2800000000+3767860=2803767860字节；floor(/188)=14913658，正好等于实测mux slots。series live14915830为加入其他holder后的真实总量。mux queue allocation238618568字节；series planned44014648262164字节与transport预算44014888218974均为准入上界，不能说物理分配44TB。末次private16932675584、WS189902848，说明无调试器也观察到GB级private增长。

首AU原deadline4418168668、observed4733619100ns，超315450432ns；错误/worker1，payload回零、reserve/release603。结果不通过；先修源startup与输出容量域混用，不能用减小验收参数、放宽deadline或将全部407ms归global替代。源仍在运行，记录后精确清理，不记为自然结束。

~~~~text
[2026-10-26 12:45:10.377] [info] [graph][planner.select] input_startup_retention window_ns=10000000000 video_replay_au_bound=20 video_units=320 video_bytes=2800000000 audio_replay_au_bound=29 audio_units=460 audio_bytes=3767860 future_arrival_rate_guarantee=not_proven
[2026-10-26 12:45:14.738] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=24 audio_count=23 first_video_sequence=15
[2026-10-26 12:45:14.738] [info] [graph][runtime.node] output_preparation stage=transport_activate generation=1 elapsed_ns=197900 planned_bytes=44014888218974 planned_items=2 event=scope_exit
[2026-10-26 12:45:14.739] [info] [graph][runtime.node] output_preparation stage=global_sequence generation=1 elapsed_ns=42100 planned_bytes=69000 planned_items=2155 event=scope_exit
[2026-10-26 12:45:14.739] [info] [graph][runtime.node] output_preparation stage=transport_buffer generation=1 elapsed_ns=1471300 planned_bytes=44014888218974 planned_items=2 event=scope_exit
[2026-10-26 12:45:15.093] [info] [graph][runtime.node] output_preparation stage=ts_packetizer generation=1 elapsed_ns=353096600 planned_bytes=44014648262164 planned_items=14915830 event=scope_exit
[2026-10-26 12:45:15.119] [info] [graph][runtime.node] output_preparation stage=ts_mux generation=1 elapsed_ns=379102900 planned_bytes=238618568 planned_items=14913658 event=scope_exit
[2026-10-26 12:45:15.120] [info] [graph][runtime.node] mpegts_emission_plan generation=1 mode=canonical_access_unit_window access_unit_window_ns=100000000 video_initial_service_window_ns=13995843 audio_initial_service_window_ns=13995843 packet_size_bytes=188 maximum_payload_bytes=1316 per_datagram_overhead_bytes=12
[2026-10-26 12:45:15.120] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 produced_datagrams=0 produced_ts_payload_bytes=0 pending_bytes=0 peak_pending_bytes=0 access_units=0 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=0 maximum_mux_input_after_emit_ns=0 maximum_mux_input_after_dispatch_ns=0 worst_mux_input_ready_ns=0 worst_mux_input_emit_ns=0 worst_mux_input_dispatch_ns=0 maximum_protocol_batch_produced_after_release_ns=0 worst_protocol_batch_produced_ns=0 worst_protocol_batch_release_ns=0 worst_protocol_batch_deadline_ns=0 worst_protocol_batch_datagrams=0 exit_reason=InvalidArgument: MPEG-TS access unit expired before prefix publication prefix_release_ns=4351502000 au_deadline_ns=4418168668 observed_ns=4733619100
[2026-10-26 12:45:15.137] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=0 committed_datagrams=0 committed_payload_bytes=0 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=0 ordered_queue_maximum_datagrams=0 ordered_queue_maximum_wire_bytes=0 pacing_reserved=0 pacing_cancelled=0 pacing_submitted=0 pacing_maximum_submit_lateness_ns=0 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=0 backlog_high_water_wire_bytes=0 backlog_max_residence_ns=0 last_materialized_sequence=0 last_scheduled_sequence=0 last_submitted_sequence=0 last_committed_sequence=0 submitted_service=absent shared_scope_charged_datagrams=0 shared_scope_charged_wire_bytes=0 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=0 evidence_timestamp_tracked=0 evidence_timestamp_untracked=0 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=379902 endpoint_1_target_effective_socket_bytes=189951 endpoint_1_api_requested_socket_bytes=189951 endpoint_1_effective_socket_bytes=189951 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=0 endpoint_1_committed_payload_bytes=0 endpoint_2_target_effective_socket_bytes=189951 endpoint_2_api_requested_socket_bytes=189951 endpoint_2_effective_socket_bytes=189951 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=0 endpoint_2_committed_payload_bytes=0
[CLI] final runtime report: queued=0, peakQueued=215, threads=40, processThreads=22, workers=0, workerProgress=4038, workerProcessCalls=5414, workerWaits=1369, workerWakeups=1342, workerErrors=1, errors=1, stalledIntervals=0, cpuSamples=15, logicalProcessors=22, averageSystemMachineCpuPercent=18.234426, averageProcessMachineCpuPercent=0.707793, peakProcessMachineCpuPercent=1.489362, averageProcessSingleCoreCpuPercent=15.571449, peakProcessSingleCoreCpuPercent=32.765957, initialWorkingSetBytes=60895232, workingSetBytes=193368064, peakWorkingSetBytes=193368064, totalPushed=1269, totalPopped=1128, droppedBuffers=0, encodedPacketsPushed=6, encodedPacketsPopped=4, backpressureItems=64, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=14157342, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=369, graphPayloadReservations=603, graphPayloadReleases=603, graphPayloadPressureFailures=0, lastInputReceivedAtNs=14380049692800
[in @ 00000112f638d480] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000112f638d480] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 13199.193678
[aac @ 00000112e4474900] Qavg: 223.719
[aac @ 00000112e4474900] 2 frames left in the queue on closing
[CLI] realtime video runtime failed: InvalidArgument: MPEG-TS access unit expired before prefix publication prefix_release_ns=4351502000 au_deadline_ns=4418168668 observed_ns=4733619100
seconds=1.0089399 cpu=1.61904859474061 ws=160161792 private=493219840
seconds=2.0336073 cpu=0.69312956841144 ws=161169408 private=493912064
seconds=3.0613144 cpu=0.483755625420016 ws=162680832 private=495308800
seconds=4.090387 cpu=0.828194946860627 ws=163991552 private=451223552
seconds=5.1236721 cpu=0.893553438973865 ws=189902848 private=16932675584
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r345-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r345-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r345-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r345-output.sdp
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r345-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r345-source0.log

~~~~



清理完成：六产物逐项删除，两个PID无残留，源清理exit-1，父64275exit1。
