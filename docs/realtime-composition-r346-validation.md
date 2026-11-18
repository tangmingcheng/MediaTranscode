# r346：共同输出容量产品复验

状态：源码双独立审查PASS、Release全量重建成功；实流FAIL，已归档清理。沿用r345原120秒源与全部媒体、时限参数，无调试器。目标核对输出容量、准备耗时、首AU期限及真实播放；不以单源通过替代完整合屏验收。

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r346-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r346-source0.log' -PassThru
Write-Output "SOURCE_PID=$($source0.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r346 --input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62024 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r346-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r346-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r346-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r346-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r346-native- --snapshot-format=png rtp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r346-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
$lastTime=$time
$lastCpu=$cpu
}
Write-Output "CLI_EXIT=$($cli.ExitCode)"
$source0.WaitForExit()
Write-Output "SOURCE0_EXIT=$($source0.ExitCode)"
exit $cli.ExitCode
~~~~

## 实测结果：FAIL

双独立源码PASS后Release全量747项configure/build exit0；realtime6504960字节，2026-10-26 12:56:02。源39048、CLI30620；CLI自然exit1，尚未VLC启动/快照。packetizer0.1096ms、mux0.3856ms、queue59086/live61258；最后采样private695345152字节。已开始发送，随后accepted-work保守SR allowance准入失败，不判整链通过。源仍运行，归档后按精确PID清理，不记自然结束。

~~~~text
[2026-10-26 12:56:22.319] [info] [graph][planner.select] input_startup_retention window_ns=10000000000 video_replay_au_bound=20 video_units=320 video_bytes=2800000000 audio_replay_au_bound=27 audio_units=458 audio_bytes=3751478 future_arrival_rate_guarantee=not_proven
[2026-10-26 12:56:26.698] [info] [graph][runtime.node] output_preparation stage=transport_activate generation=1 elapsed_ns=259200 planned_bytes=180769377326 planned_items=2 event=scope_exit
[2026-10-26 12:56:26.698] [info] [graph][runtime.node] output_preparation stage=global_sequence generation=1 elapsed_ns=33700 planned_bytes=69000 planned_items=2155 event=scope_exit
[2026-10-26 12:56:26.699] [info] [graph][runtime.node] output_preparation stage=transport_buffer generation=1 elapsed_ns=1111500 planned_bytes=180769377326 planned_items=2 event=scope_exit
[2026-10-26 12:56:26.700] [info] [graph][runtime.node] output_preparation stage=ts_packetizer generation=1 elapsed_ns=109600 planned_bytes=180767093668 planned_items=61258 event=scope_exit
[2026-10-26 12:56:26.700] [info] [graph][runtime.node] output_preparation stage=ts_mux generation=1 elapsed_ns=385600 planned_bytes=945416 planned_items=59086 event=scope_exit
[2026-10-26 12:56:27.295] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 produced_datagrams=562 produced_ts_payload_bytes=689584 pending_bytes=15980 peak_pending_bytes=104528 access_units=47 current_scheduling_debt_ns=33333342 maximum_scheduling_debt_ns=33333342 maximum_mux_input_after_emit_ns=94200998 maximum_mux_input_after_dispatch_ns=-5799002 worst_mux_input_ready_ns=4963687500 worst_mux_input_emit_ns=4869486502 worst_mux_input_dispatch_ns=4969486502 maximum_protocol_batch_produced_after_release_ns=63075156 worst_protocol_batch_produced_ns=4965895000 worst_protocol_batch_release_ns=4902819844 worst_protocol_batch_deadline_ns=4969486502 worst_protocol_batch_datagrams=1 exit_reason=InvalidArgument: TS accepted-work admission check exceeds original deadline stage=unpublished_AU_conservative_possible_report_allowance publication=0 available_ns=4966812260 release_ns=4902819844 deadline_ns=4969486502 duration_ns=2839680
[2026-10-26 12:56:27.296] [info] [graph][runtime.node] process.failed node=34 name=FileMuxNode kind=FileMux inputs=[channel=53 edge=53 stream=Video edge_kind=Metadata payload=CodecContext queued=0; channel=55 edge=55 stream=Audio edge_kind=Metadata payload=CodecContext queued=0; channel=56 edge=56 stream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=59 edge=59 stream=Any edge_kind=EncodedPacket payload=Unknown queued=10] outputs=[channel=62 edge=62 stream=Metadata edge_kind=Unknown payload=Unknown queued=0] error=InvalidArgument: TS accepted-work admission check exceeds original deadline stage=unpublished_AU_conservative_possible_report_allowance publication=0 available_ns=4966812260 release_ns=4902819844 deadline_ns=4969486502 duration_ns=2839680
[2026-10-26 12:56:27.297] [info] [graph][runtime.node] worker.failed node=34 kind=FileMux name=Project MPEG-TS mux error=InvalidArgument: TS accepted-work admission check exceeds original deadline stage=unpublished_AU_conservative_possible_report_allowance publication=0 available_ns=4966812260 release_ns=4902819844 deadline_ns=4969486502 duration_ns=2839680
[2026-10-26 12:56:27.302] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=562 committed_datagrams=562 committed_payload_bytes=696328 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=1 ordered_queue_maximum_wire_bytes=1356 pacing_reserved=562 pacing_cancelled=0 pacing_submitted=562 pacing_maximum_submit_lateness_ns=5961800 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=1 backlog_high_water_wire_bytes=1356 backlog_max_residence_ns=28020900 last_materialized_sequence=562 last_scheduled_sequence=562 last_submitted_sequence=562 last_committed_sequence=562 submitted_service_generation=1 submitted_service_sequence=562 submitted_service_completed_ns=4966595300 submitted_service_duration_ns=216960 submitted_service_available_ns=4966812260 submitted_scope_available_ns=4966812260 shared_scope_charged_datagrams=562 shared_scope_charged_wire_bytes=712064 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=562 evidence_timestamp_tracked=0 evidence_timestamp_untracked=562 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=379902 endpoint_1_target_effective_socket_bytes=189951 endpoint_1_api_requested_socket_bytes=189951 endpoint_1_effective_socket_bytes=189951 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=562 endpoint_1_committed_payload_bytes=696328 endpoint_2_target_effective_socket_bytes=189951 endpoint_2_api_requested_socket_bytes=189951 endpoint_2_effective_socket_bytes=189951 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=0 endpoint_2_committed_payload_bytes=0
[2026-10-26 12:56:27.488] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=failed detail="TS accepted-work admission check exceeds original deadline stage=unpublished_AU_conservative_possible_report_allowance publication=0 available_ns=4966812260 release_ns=4902819844 deadline_ns=4969486502 duration_ns=2839680" description="D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r346-output.sdp" error=1 native=0 stage=3 reason="TS accepted-work admission check exceeds original deadline stage=unpublished_AU_conservative_possible_report_allowance publication=0 available_ns=4966812260 release_ns=4902819844 deadline_ns=4969486502 duration_ns=2839680"
[2026-10-26 12:56:27.488] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=retired detail="TS accepted-work admission check exceeds original deadline stage=unpublished_AU_conservative_possible_report_allowance publication=0 available_ns=4966812260 release_ns=4902819844 deadline_ns=4969486502 duration_ns=2839680" description="D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r346-output.sdp" error=1 native=0 stage=3 reason="TS accepted-work admission check exceeds original deadline stage=unpublished_AU_conservative_possible_report_allowance publication=0 available_ns=4966812260 release_ns=4902819844 deadline_ns=4969486502 duration_ns=2839680"
[CLI] final runtime report: queued=0, peakQueued=671, threads=40, processThreads=60, workers=0, workerProgress=9601, workerProcessCalls=19925, workerWaits=10318, workerWakeups=10252, workerErrors=1, errors=1, stalledIntervals=0, cpuSamples=16, logicalProcessors=22, averageSystemMachineCpuPercent=16.492658, averageProcessMachineCpuPercent=1.115931, peakProcessMachineCpuPercent=4.157044, averageProcessSingleCoreCpuPercent=24.550491, peakProcessSingleCoreCpuPercent=91.454965, initialWorkingSetBytes=60252160, workingSetBytes=196235264, peakWorkingSetBytes=196235264, totalPushed=5869, totalPopped=5481, droppedBuffers=0, encodedPacketsPushed=525, encodedPacketsPopped=286, backpressureItems=64, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=15665686, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=404, graphPayloadReservations=2423, graphPayloadReleases=2423, graphPayloadPressureFailures=0, lastInputReceivedAtNs=15052213073300
[in @ 0000028dbea71d80] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000028dbea71d80] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 28920.304178
[aac @ 0000028dbefdb0c0] Qavg: 346.894
[aac @ 0000028dbefdb0c0] 2 frames left in the queue on closing
[CLI] realtime video runtime failed: InvalidArgument: TS accepted-work admission check exceeds original deadline stage=unpublished_AU_conservative_possible_report_allowance publication=0 available_ns=4966812260 release_ns=4902819844 deadline_ns=4969486502 duration_ns=2839680
seconds=1.0156131 cpu=1.60841045401317 ws=160280576 private=492503040
seconds=2.0465933 cpu=0.619996916967509 ws=161325056 private=493301760
seconds=3.0678776 cpu=0.695425625095062 ws=163921920 private=495587328
seconds=4.0970456 cpu=0.759108328280708 ws=164237312 private=451985408
seconds=5.116222 cpu=1.324041469349 ws=195481600 private=695345152
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r346-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r346-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r346-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r346-output.sdp
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r346-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r346-source0.log
~~~~

补充时序证据（phase非内容A/V证明）：
~~~~text
[2026-10-26 12:56:26.699] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1 source_master_ns=4536611218 raw_phase_ns=4536 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-10-26 12:56:26.699] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3 source_master_ns=4583051128 raw_phase_ns=4536 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=45346
[2026-10-26 12:56:26.700] [info] [graph][runtime.node] mpegts_emission_plan generation=1 mode=canonical_access_unit_window access_unit_window_ns=100000000 video_initial_service_window_ns=13995843 audio_initial_service_window_ns=13995843 packet_size_bytes=188 maximum_payload_bytes=1316 per_datagram_overhead_bytes=12
[2026-10-26 12:56:26.704] [info] [graph][runtime.node] av_scheduler_trace stage=first_audio_head generation=1 mapping_epoch_generation=1 canonical_target_ns=-4022000000 canonical_dispatch_ns=-4022000000 target_master_ns=4536153168 dispatch_master_ns=4536153168 now_ns=4375857400 node=32 identity_kind=output identity="realtime.av" canonical_sequence=1 canonical_created_steady_ns=15051620312000 head_observed_steady_ns=15051620828400 codec_receive_begin_steady_ns=unavailable codec_receive_end_steady_ns=unavailable
[2026-10-26 12:56:26.704] [info] [graph][runtime.node] av_scheduler_trace stage=first_video_head generation=1 mapping_epoch_generation=1 canonical_target_ns=-4022000000 canonical_dispatch_ns=-4022000000 target_master_ns=4536153168 dispatch_master_ns=4536153168 now_ns=4376228800 node=32 identity_kind=source identity="composition-r346.input.video" canonical_sequence=14 canonical_created_steady_ns=15051620950800 head_observed_steady_ns=15051621199800 codec_receive_begin_steady_ns=15051620711400 codec_receive_end_steady_ns=15051620712500
[2026-10-26 12:56:26.728] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=48 source_master_ns=5627949087 raw_phase_ns=4536 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=90692
[2026-10-26 12:56:26.747] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=92 source_master_ns=6649627091 raw_phase_ns=4536 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=136038
[2026-10-26 12:56:26.775] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=136 source_master_ns=7671305096 raw_phase_ns=4536 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=181384
[2026-10-26 12:56:26.808] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=181 source_master_ns=8716203054 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=226730
[2026-10-26 12:56:27.299] [info] [graph][runtime.node] datagram_batch_trace stage=last_completed generation=1 first_sequence=562 last_sequence=562 maximum_submit_interval_ns=63200 maximum_submit_interval_sequence=562 maximum_start_lateness_ns=496640 maximum_start_lateness_sequence=562 first_release_master_ns=4902819844 first_deadline_master_ns=4969486502 last_release_master_ns=4902819844 last_deadline_master_ns=4969486502 first_submitted_master_ns=4966595300 last_submitted_master_ns=4966595300 worst_wait_started_master_ns=4965993700 worst_wait_completed_master_ns=4966531000 worst_submit_observed_master_ns=4966532100
[2026-10-26 12:56:27.409] [info] [graph][runtime.node] mpegts_wire_materializer stage=aborted maximum_materialized_after_release_ns=63102856 worst_materialized_at_ns=4965922700 worst_release_ns=4902819844 worst_deadline_ns=4969486502 worst_global_sequence=562
~~~~

清理完成：六产物按清单逐项删除，源39048精确清理、CLI30620已自然失败退出，两PID无残留；无VLC/快照。源强制结束不计自然完成。

源清理exit -1，启动/监控会话21419最终exit1。准入算术available4966812260 + duration2839680 - deadline4969486502 = 165438ns；该差值说明当前准入失败，不单独证明期限策略或实现错误，继续只读追踪。

## 独立诊断中间结论

两位审查员独立反算一致：pending15980字节=85 TS包=13数据报；含每报40字节媒体网络开销共16500字节，在6250000B/s峰率下为2640000ns。日志2839680ns另含13次可能SR额度，共199680ns（每次96wire字节/15360ns）。available与sender最后service/scopeAvailable完全一致，当前未见published重复计费。纯媒体峰率模型仅剩34242ns理想余量，加入保守SR后拒绝165438ns；不能据此声称13个SR真实发送，也不能证明剩余13次commit与维护能实际完成。需继续核对SR权威日程、到达时序及计费边界，不能删除保守额度来让样例通过。
