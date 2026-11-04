# r348：当前AU失败时间链复验

状态：诊断源码双独立PASS、Release全量成功；实流FAIL，已归档清理。原r346完整转码参数、120秒源、原期限不变，仅新增当前pending AU失败时现有trace汇总。不运行local CLI，不以音频直通替代。

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r348-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r348-source0.log' -PassThru
Write-Output "SOURCE_PID=$($source0.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r348 --input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62024 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r348-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r348-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r348-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r348-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r348-native- --snapshot-format=png rtp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r348-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
$lastTime=$time
$lastCpu=$cpu
}
Write-Output "CLI_EXIT=$($cli.ExitCode)"
$source0.WaitForExit()
Write-Output "SOURCE0_EXIT=$($source0.ExitCode)"
exit $cli.ExitCode
~~~~

## 实测结果

Release全量747项configure/build exit0；realtime6505472字节/2026-10-26 13:08:02。源34488、CLI13024；CLI自然exit1，未启动VLC。pending_au_failure记录当前video sequence27：creation到adapter dequeue41.036ms，adapted creation到write entry77.6414ms，write到session ready0.0026ms；ready较emit晚93.636398ms，尚距原dispatch6.363602ms。失败时剩3报3948B，纯媒体3×216960=650880ns；deadline-available=564942ns，即不计SR仍差85938ns，不能只修报告上界宣称解决。原保守模型696960ns超132018ns。
~~~~text
[2026-10-26 13:08:30.468] [info] [graph][runtime.node] output_preparation stage=transport_activate generation=1 elapsed_ns=225100 planned_bytes=180769377326 planned_items=2 event=scope_exit
[2026-10-26 13:08:30.468] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1 source_master_ns=4546046781 raw_phase_ns=18142 filtered_phase_ns=18142 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-10-26 13:08:30.469] [info] [graph][runtime.node] output_preparation stage=global_sequence generation=1 elapsed_ns=95600 planned_bytes=69000 planned_items=2155 event=scope_exit
[2026-10-26 13:08:30.469] [info] [graph][runtime.node] output_preparation stage=transport_buffer generation=1 elapsed_ns=1789900 planned_bytes=180769377326 planned_items=2 event=scope_exit
[2026-10-26 13:08:30.469] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3 source_master_ns=4592486690 raw_phase_ns=18142 filtered_phase_ns=18142 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=45346
[2026-10-26 13:08:30.470] [info] [graph][runtime.node] output_preparation stage=ts_packetizer generation=1 elapsed_ns=74400 planned_bytes=180767093668 planned_items=61258 event=scope_exit
[2026-10-26 13:08:30.471] [info] [graph][runtime.node] output_preparation stage=ts_mux generation=1 elapsed_ns=204000 planned_bytes=945416 planned_items=59086 event=scope_exit
[2026-10-26 13:08:30.536] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=48 source_master_ns=5637384649 raw_phase_ns=18142 filtered_phase_ns=18142 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=90692
[2026-10-26 13:08:30.583] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=92 source_master_ns=6659062654 raw_phase_ns=18142 filtered_phase_ns=18142 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=136038
[2026-10-26 13:08:30.630] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=136 source_master_ns=7680740658 raw_phase_ns=18142 filtered_phase_ns=18142 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=181384
[2026-10-26 13:08:30.676] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=181 source_master_ns=8725638616 raw_phase_ns=18140 filtered_phase_ns=18142 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=226730
[2026-10-26 13:08:31.067] [info] [graph][runtime.node] au_processing_trace stage=pending_au_failure generation=1 stream=video canonical_sequence=27 emit_master_ns=4877933402 dispatch_master_ns=4977933402 ready_master_ns=4971569800 scheduled_created_steady_ns=15775859163600 adapter_dequeued_steady_ns=15775900199600 adapted_created_steady_ns=15775900201200 write_entered_steady_ns=15775977842600 session_ready_steady_ns=15775977845200
[2026-10-26 13:08:31.067] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 produced_datagrams=555 produced_ts_payload_bytes=680184 pending_bytes=3948 peak_pending_bytes=104528 access_units=46 current_scheduling_debt_ns=33333342 maximum_scheduling_debt_ns=33333342 maximum_mux_input_after_emit_ns=93636398 maximum_mux_input_after_dispatch_ns=-6363602 worst_mux_input_ready_ns=4971569800 worst_mux_input_emit_ns=4877933402 worst_mux_input_dispatch_ns=4977933402 maximum_protocol_batch_produced_after_release_ns=65242656 worst_protocol_batch_produced_ns=4976509400 worst_protocol_batch_release_ns=4911266744 worst_protocol_batch_deadline_ns=4977933402 worst_protocol_batch_datagrams=1 exit_reason=InvalidArgument: TS accepted-work admission check exceeds original deadline stage=unpublished_AU_conservative_possible_report_allowance publication=0 available_ns=4977368460 release_ns=4911266744 deadline_ns=4977933402 duration_ns=696960
[2026-10-26 13:08:31.072] [info] [graph][runtime.node] datagram_batch_trace stage=last_completed generation=1 first_sequence=555 last_sequence=555 maximum_submit_interval_ns=20200 maximum_submit_interval_sequence=555 maximum_start_lateness_ns=450240 maximum_start_lateness_sequence=555 first_release_master_ns=4911266744 first_deadline_master_ns=4977933402 last_release_master_ns=4911266744 last_deadline_master_ns=4977933402 first_submitted_master_ns=4977151500 last_submitted_master_ns=4977151500 worst_wait_started_master_ns=4976609300 worst_wait_completed_master_ns=4977131000 worst_submit_observed_master_ns=4977131300
[2026-10-26 13:08:31.076] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=555 committed_datagrams=555 committed_payload_bytes=686844 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=1 ordered_queue_maximum_wire_bytes=1356 pacing_reserved=555 pacing_cancelled=0 pacing_submitted=555 pacing_maximum_submit_lateness_ns=7118300 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=1 backlog_high_water_wire_bytes=1356 backlog_max_residence_ns=27999000 last_materialized_sequence=555 last_scheduled_sequence=555 last_submitted_sequence=555 last_committed_sequence=555 submitted_service_generation=1 submitted_service_sequence=555 submitted_service_completed_ns=4977151500 submitted_service_duration_ns=216960 submitted_service_available_ns=4977368460 submitted_scope_available_ns=4977368460 shared_scope_charged_datagrams=555 shared_scope_charged_wire_bytes=702384 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=555 evidence_timestamp_tracked=0 evidence_timestamp_untracked=555 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=379902 endpoint_1_target_effective_socket_bytes=189951 endpoint_1_api_requested_socket_bytes=189951 endpoint_1_effective_socket_bytes=189951 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=555 endpoint_1_committed_payload_bytes=686844 endpoint_2_target_effective_socket_bytes=189951 endpoint_2_api_requested_socket_bytes=189951 endpoint_2_effective_socket_bytes=189951 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=0 endpoint_2_committed_payload_bytes=0
[2026-10-26 13:08:31.252] [info] [graph][runtime.node] mpegts_wire_materializer stage=aborted maximum_materialized_after_release_ns=65278856 worst_materialized_at_ns=4976545600 worst_release_ns=4911266744 worst_deadline_ns=4977933402 worst_global_sequence=555
[CLI] final runtime report: queued=0, peakQueued=677, threads=40, processThreads=60, workers=0, workerProgress=10401, workerProcessCalls=20928, workerWaits=10522, workerWakeups=10455, workerErrors=1, errors=1, stalledIntervals=0, cpuSamples=15, logicalProcessors=22, averageSystemMachineCpuPercent=16.831609, averageProcessMachineCpuPercent=1.492267, peakProcessMachineCpuPercent=6.172840, averageProcessSingleCoreCpuPercent=32.829864, peakProcessSingleCoreCpuPercent=135.802469, initialWorkingSetBytes=61153280, workingSetBytes=195301376, peakWorkingSetBytes=195301376, totalPushed=5856, totalPopped=5467, droppedBuffers=0, encodedPacketsPushed=522, encodedPacketsPopped=282, backpressureItems=64, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=15950667, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=404, graphPayloadReservations=2422, graphPayloadReleases=2422, graphPayloadPressureFailures=0, lastInputReceivedAtNs=15775979071800
[in @ 000001c77b0bb640] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001c77b0bb640] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 29179.788067
[aac @ 000001c776327180] Qavg: 382.577
[aac @ 000001c776327180] 2 frames left in the queue on closing
[CLI] realtime video runtime failed: InvalidArgument: TS accepted-work admission check exceeds original deadline stage=unpublished_AU_conservative_possible_report_allowance publication=0 available_ns=4977368460 release_ns=4911266744 deadline_ns=4977933402 duration_ns=696960
seconds=1.0144456 cpu=1.47023879124447 ws=160169984 private=494825472
seconds=2.0346586 cpu=0.765771461449717 ws=161275904 private=495673344
seconds=3.0609605 cpu=0.484417977701387 ws=162951168 private=497000448
seconds=4.1023058 cpu=1.43226005123111 ws=164069376 private=452685824
seconds=5.1419332 cpu=1.77620454125286 ws=194707456 private=691486720
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r348-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r348-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r348-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r348-output.sdp
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r348-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r348-source0.log
~~~~

清理完成：六产物按清单删除；源34488清理exit-1，CLI13024自然失败exit1，两个PID无残留，监控会话39042 exit1。无VLC/快照。

## 双独立调用链核验

两位审查员确认adapter.packet直接连接FileMux.packet；adapter创建后仍有generation提交/框架发布，不能将77.6414ms全称FIFO驻留。FileMux在取输入之前优先poll所有pending output，当前AU每次仅发布一个datagram并等待真实protocol commit，当前AU全部完成才读取下一AU。因此串行往返能积累到后AU等待，但缺乏完整提交/恢复时间分布，尚不能量化归因。writeEntered到sessionReady仅2.6us，排除把全部77ms归于本次write内部锁等待。下一观测只保留当前及最近完成AU固定汇总，分别测mux内部批次入队到protocol commit完成、commit到mux恢复，不改变调度。

waiter独立源码核验未发现已证丢通知：series同锁保护committed检查/注册与提交/取waiter，unlock后notify；worker在process前捕获sequence，commit WouldBlock使用InputOrDeadline，sequence差覆盖先通知后等待。维护DeadlineOrCancellation不是该分支。该证据不保证OS即时调度，仍需分段实测。
