# r344：无调试器单源AV启动对照

状态：无调试器复现首AU超期，已记录并清理。与r343同Release及单源AV参数，移除CDB及断点，原120秒源、HEVC CBR8000kbps 1280×720/30、AAC CBR192kbps 44100Hz双声道、TS/RTP。用于区分首次AU期限和private增长是否在无调试器情况下复现，不降低期限或媒体规格。

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r344-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r344-source0.log' -PassThru
Write-Output "SOURCE_PID=$($source0.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r344 --input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62024 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r344-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r344-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r344-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r344-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r344-native- --snapshot-format=png rtp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r344-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
$lastTime=$time
$lastCpu=$cpu
}
Write-Output "CLI_EXIT=$($cli.ExitCode)"
$source0.WaitForExit()
Write-Output "SOURCE0_EXIT=$($source0.ExitCode)"
exit $cli.ExitCode
~~~~

## 实际结果：无调试器仍失败

源37296、CLI8732；CLI在首次AU正常exit1，VLC未及启动。release4364463900、原deadline4431130568、observed4772054300ns，超原期限340923732ns。first_release12:32:53.745至emission_plan54.152约407ms；数据报/媒体均0，worker/errors1，payload回零res/release480。说明首AU失败不只发生于CDB下，尚不确定具体初始化根因。

CPU均0.747885%、峰1.590457%，WS峰及末193376256；1秒采样末private511557632，未观察r343的27GB峰，不能排除采样间短暂峰值。原期限未放宽，媒体规格未降低。保存证据后清理源与六项产物，不记为通过；下一步调查epoch起跑后的transport/mux准备耗时。

~~~~text
[in @ 0000015fbb740880] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000015fbb740880] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 29661.356644
[aac @ 0000015fa196fcc0] Qavg: nan
[CLI] realtime video runtime failed: InvalidArgument: MPEG-TS access unit expired before prefix publication prefix_release_ns=4364463900 au_deadline_ns=4431130568 observed_ns=4772054300
[2026-10-26 12:32:49.371] [info] [graph][planner.select] input_startup_retention window_ns=10000000000 video_replay_au_bound=21 video_units=321 video_bytes=2808750000 audio_replay_au_bound=30 audio_units=461 audio_bytes=3776051 future_arrival_rate_guarantee=not_proven
[2026-10-26 12:32:53.745] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=15 audio_count=33 first_video_sequence=15
[2026-10-26 12:32:54.152] [info] [graph][runtime.node] mpegts_emission_plan generation=1 mode=canonical_access_unit_window access_unit_window_ns=100000000 video_initial_service_window_ns=13995843 audio_initial_service_window_ns=13995843 packet_size_bytes=188 maximum_payload_bytes=1316 per_datagram_overhead_bytes=12
[2026-10-26 12:32:54.152] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 produced_datagrams=0 produced_ts_payload_bytes=0 pending_bytes=0 peak_pending_bytes=0 access_units=0 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=0 maximum_mux_input_after_emit_ns=0 maximum_mux_input_after_dispatch_ns=0 worst_mux_input_ready_ns=0 worst_mux_input_emit_ns=0 worst_mux_input_dispatch_ns=0 maximum_protocol_batch_produced_after_release_ns=0 worst_protocol_batch_produced_ns=0 worst_protocol_batch_release_ns=0 worst_protocol_batch_deadline_ns=0 worst_protocol_batch_datagrams=0 exit_reason=InvalidArgument: MPEG-TS access unit expired before prefix publication prefix_release_ns=4364463900 au_deadline_ns=4431130568 observed_ns=4772054300
[2026-10-26 12:32:54.159] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=0 committed_datagrams=0 committed_payload_bytes=0 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=0 ordered_queue_maximum_datagrams=0 ordered_queue_maximum_wire_bytes=0 pacing_reserved=0 pacing_cancelled=0 pacing_submitted=0 pacing_maximum_submit_lateness_ns=0 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=0 backlog_high_water_wire_bytes=0 backlog_max_residence_ns=0 last_materialized_sequence=0 last_scheduled_sequence=0 last_submitted_sequence=0 last_committed_sequence=0 submitted_service=absent shared_scope_charged_datagrams=0 shared_scope_charged_wire_bytes=0 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=0 evidence_timestamp_tracked=0 evidence_timestamp_untracked=0 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=379902 endpoint_1_target_effective_socket_bytes=189951 endpoint_1_api_requested_socket_bytes=189951 endpoint_1_effective_socket_bytes=189951 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=0 endpoint_1_committed_payload_bytes=0 endpoint_2_target_effective_socket_bytes=189951 endpoint_2_api_requested_socket_bytes=189951 endpoint_2_effective_socket_bytes=189951 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=0 endpoint_2_committed_payload_bytes=0
[CLI] final runtime report: queued=0, peakQueued=178, threads=40, processThreads=22, workers=0, workerProgress=3501, workerProcessCalls=4524, workerWaits=1016, workerWakeups=990, workerErrors=1, errors=1, stalledIntervals=0, cpuSamples=15, logicalProcessors=22, averageSystemMachineCpuPercent=19.023120, averageProcessMachineCpuPercent=0.747885, peakProcessMachineCpuPercent=1.590457, averageProcessSingleCoreCpuPercent=16.453476, peakProcessSingleCoreCpuPercent=34.990060, initialWorkingSetBytes=63221760, workingSetBytes=193376256, peakWorkingSetBytes=193376256, totalPushed=972, totalPopped=856, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=64, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=14157870, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=372, graphPayloadReservations=480, graphPayloadReleases=480, graphPayloadPressureFailures=0, lastInputReceivedAtNs=13639069338900
seconds=1.0097376 cpu=1.19574269952546 ws=160473088 private=494727168
seconds=2.0303298 cpu=0.835076661640886 ws=161456128 private=495419392
seconds=3.0559551 cpu=0.553985766714041 ws=163201024 private=496775168
seconds=4.0762664 cpu=0.904915445458121 ws=164466688 private=451977216
seconds=5.0953049 cpu=0.487870763380471 ws=184893440 private=511557632
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r344-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r344-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r344-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r344-output.sdp
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r344-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r344-source0.log

~~~~



清理完成：六文件逐项删除、两个精确PID无残留；源清理exit-1，父78797exit1。
