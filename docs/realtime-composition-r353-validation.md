# r353：滤镜成功接受后背压恢复复验

状态：VideoFilterNode接受状态已修正，双独立源码审查PASS，Release全量构建成功，实流结束但整体FAIL。保持r352的原源、媒体规格、期限和真实链路；本轮验证不替代完整合屏验收。

## 预定执行命令

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r353-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r353-source0.log' -PassThru
Write-Output "SOURCE_PID=$($source0.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r353 --input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62024 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r353-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r353-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r353-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r353-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r353-native- --snapshot-format=png rtp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r353-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
$lastTime=$time
$lastCpu=$cpu
}
Write-Output "CLI_EXIT=$($cli.ExitCode)"
$source0.WaitForExit()
Write-Output "SOURCE0_EXIT=$($source0.ExitCode)"
exit $cli.ExitCode
~~~~

## 实测与边界

Release747项clean-first all-target configure/build exit0，realtime6522880字节/2026-10-26 14:28:31，四产物齐全。源42604自然exit0、3600帧120秒；CLI20748无人工停止，自然exit1；VLC43708。此前约5秒的TS原期限准入失败未复现，持续播放至源结束，但随后CLI报NotInitialized: realtime runtime made no progress before timeout，因此不标完整验收PASS。sender提交82931报，purge取消1报/792wire bytes，backlog回零，purge ack7/complete，进入generation2 acquiring后无源；结束超时语义待核对。资源75581 reserve=release、payload0，平均CPU1.798698%、峰5.050505%，WS峰202248192。稳定段private约711–713MB；内部phase末段0/filtered1ns不等于内容A/V证明。VLC原生快照两张1280×720，已实际查看，画面正常且内容不同；有late picture与video converter等VLC日志，保留原文，不宣称播放器零错误或NIC交付证明。

~~~~text
[2026-10-26 14:34:23.143] [info] [graph][runtime.node] av_reacquisition group=realtime.av phase=purging old=1 next=2 transition=1
[2026-10-26 14:34:23.146] [info] [graph][runtime.node] datagram_batch_trace stage=last_completed generation=1 first_sequence=82931 last_sequence=82931 maximum_submit_interval_ns=21800 maximum_submit_interval_sequence=82931 maximum_start_lateness_ns=250420 maximum_start_lateness_sequence=82931 first_release_master_ns=118752629857 first_deadline_master_ns=118852629857 last_release_master_ns=118752629857 last_deadline_master_ns=118852629857 first_submitted_master_ns=118766007900 last_submitted_master_ns=118766007900 worst_wait_started_master_ns=118765734800 worst_wait_completed_master_ns=118765985400 worst_submit_observed_master_ns=118765986100 wait_attempts=1 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=1 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=1 wait_call_total_ns=250100 wait_call_max_ns=250100 wait_before_call_count=1 wait_before_call_total_ns=200 wait_before_call_max_ns=200 wait_arm_count=1 wait_arm_total_ns=10600 wait_arm_max_ns=10600 wait_api_wall_count=1 wait_api_wall_total_ns=238900 wait_api_wall_max_ns=238900 wait_return_to_next_clock_count=1 wait_return_to_next_clock_total_ns=400 wait_return_to_next_clock_max_ns=400 wait_target_lateness_count=1 wait_target_lateness_total_ns=249720 wait_target_lateness_max_ns=249720 worst_wait_metric=call_wall worst_wait_sequence=82931 worst_wait_target_master_ns=118765735680 worst_wait_sampled_master_ns=118765734800 worst_wait_sampled_steady_ns=20928058268500 worst_wait_timeout_ns=880 worst_wait_entered_steady_ns=20928058268700 worst_wait_returned_steady_ns=20928058518800 worst_wait_arm_before_steady_ns=20928058268900 worst_wait_arm_after_steady_ns=20928058279500 worst_wait_api_before_steady_ns=20928058279600 worst_wait_api_returned_steady_ns=20928058518500 worst_wait_windows_due_100ns=-9 worst_wait_windows_result=1 worst_wait_windows_error=unavailable worst_wait_next_master_ns=118765985400 worst_wait_next_steady_ns=20928058519200 worst_wait_outcome=deadline
[2026-10-26 14:34:23.146] [info] [graph][runtime.node] datagram_batch_trace stage=active generation=1 first_sequence=82932 last_sequence=82932 maximum_submit_interval_ns=0 maximum_submit_interval_sequence=0 maximum_start_lateness_ns=0 maximum_start_lateness_sequence=0 first_release_master_ns=118775849811 first_deadline_master_ns=118875849811 last_release_master_ns=118775849811 last_deadline_master_ns=118875849811 first_submitted_master_ns=unavailable last_submitted_master_ns=unavailable worst_wait_started_master_ns=unavailable worst_wait_completed_master_ns=unavailable worst_submit_observed_master_ns=unavailable wait_attempts=1 wait_incomplete=1 wait_notified_count=1 wait_deadline_count=0 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=1 wait_call_total_ns=3729000 wait_call_max_ns=3729000 wait_before_call_count=1 wait_before_call_total_ns=100 wait_before_call_max_ns=100 wait_arm_count=1 wait_arm_total_ns=1300 wait_arm_max_ns=1300 wait_api_wall_count=1 wait_api_wall_total_ns=3726200 wait_api_wall_max_ns=3726200 wait_return_to_next_clock_count=0 wait_return_to_next_clock_total_ns=0 wait_return_to_next_clock_max_ns=0 wait_target_lateness_count=0 wait_target_lateness_total_ns=0 wait_target_lateness_max_ns=0 worst_wait_metric=call_wall worst_wait_sequence=82932 worst_wait_target_master_ns=118775849811 worst_wait_sampled_master_ns=118766142900 worst_wait_sampled_steady_ns=20928058676600 worst_wait_timeout_ns=9706911 worst_wait_entered_steady_ns=20928058676700 worst_wait_returned_steady_ns=20928062405700 worst_wait_arm_before_steady_ns=20928058676800 worst_wait_arm_after_steady_ns=20928058678100 worst_wait_api_before_steady_ns=20928058678100 worst_wait_api_returned_steady_ns=20928062404300 worst_wait_windows_due_100ns=-97070 worst_wait_windows_result=0 worst_wait_windows_error=unavailable worst_wait_next_master_ns=unavailable worst_wait_next_steady_ns=unavailable worst_wait_outcome=notified
[2026-10-26 14:34:23.146] [info] [graph][runtime.node] scheduled_datagram_sender stage=generation_purged generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=12398 committed_datagrams=82931 committed_payload_bytes=100717448 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=80004 pacing_reserved=82932 pacing_cancelled=1 pacing_submitted=82931 pacing_maximum_submit_lateness_ns=4524817 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=1 backlog_cancelled_wire_bytes=792 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=80004 backlog_max_residence_ns=57654500 last_materialized_sequence=82932 last_scheduled_sequence=82932 last_submitted_sequence=82931 last_committed_sequence=82931 submitted_service=absent shared_scope_charged_datagrams=82931 shared_scope_charged_wire_bytes=103039516 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2155 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2155 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=379902 endpoint_1_target_effective_socket_bytes=189951 endpoint_1_api_requested_socket_bytes=189951 endpoint_1_effective_socket_bytes=189951 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=82902 endpoint_1_committed_payload_bytes=100715476 endpoint_2_target_effective_socket_bytes=189951 endpoint_2_api_requested_socket_bytes=189951 endpoint_2_effective_socket_bytes=189951 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=29 endpoint_2_committed_payload_bytes=1972
[2026-10-26 14:34:23.147] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 produced_datagrams=82903 produced_ts_payload_bytes=99721404 pending_bytes=0 peak_pending_bytes=108288 access_units=8357 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=33242630 maximum_mux_input_after_emit_ns=48860068 maximum_mux_input_after_dispatch_ns=-51139932 worst_mux_input_ready_ns=78468428700 worst_mux_input_emit_ns=78419568632 worst_mux_input_dispatch_ns=78519568632 maximum_protocol_batch_produced_after_release_ns=39666912 worst_protocol_batch_produced_ns=110457242200 worst_protocol_batch_release_ns=110417575288 worst_protocol_batch_deadline_ns=110484241956 worst_protocol_batch_datagrams=1 exit_reason=aborted
[2026-10-26 14:34:25.149] [info] [graph][runtime.node] rtp_ingress_purge node=2 transition=1 cutoff=5166
[2026-10-26 14:34:25.149] [info] [graph][runtime.node] rtp_ingress_purge node=1 transition=1 cutoff=3606
[2026-10-26 14:34:25.150] [info] [graph][runtime.node] rtp_binder_purge node=5 transition=1 cutoff=3606
[2026-10-26 14:34:25.150] [info] [graph][runtime.node] rtp_binder_purge node=6 transition=1 cutoff=5166
[2026-10-26 14:34:25.151] [info] [graph][runtime.node] av_reacquisition group=realtime.av event=purge_completed purge_ack=complete continuation=acquiring ack_count=7 old=1 next=2 transition=1
[2026-10-26 14:34:28.475] [info] [graph][runtime.node] scheduled_datagram_sender stage=aborted generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=12398 committed_datagrams=82931 committed_payload_bytes=100717448 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=80004 pacing_reserved=82932 pacing_cancelled=1 pacing_submitted=82931 pacing_maximum_submit_lateness_ns=4524817 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven shared_scope_charged_datagrams=82931 shared_scope_charged_wire_bytes=103039516 shared_scope_waiting_members=0 shared_scope_failed=0
[2026-10-26 14:34:28.563] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=failed detail="realtime runtime made no progress before timeout" description="D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r353-output.sdp" error=2 native=0 stage=3 reason="realtime runtime made no progress before timeout"
[CLI] final runtime report: queued=0, peakQueued=649, threads=40, processThreads=58, workers=0, workerProgress=251368, workerProcessCalls=632040, workerWaits=380668, workerWakeups=372428, workerErrors=0, errors=0, stalledIntervals=1, cpuSamples=377, logicalProcessors=22, averageSystemMachineCpuPercent=22.596738, averageProcessMachineCpuPercent=1.798698, peakProcessMachineCpuPercent=5.050505, averageProcessSingleCoreCpuPercent=39.571353, peakProcessSingleCoreCpuPercent=111.111111, initialWorkingSetBytes=61448192, workingSetBytes=193077248, peakWorkingSetBytes=202248192, totalPushed=171429, totalPopped=171429, droppedBuffers=0, encodedPacketsPushed=38577, encodedPacketsPopped=38577, backpressureItems=64, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=15572504, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=365, graphPayloadReservations=75581, graphPayloadReleases=75581, graphPayloadPressureFailures=0, lastInputReceivedAtNs=20928058876000
[2026-10-26 14:34:20.385] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=5007 source_master_ns=120757119653 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5169444
[2026-10-26 14:34:21.448] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=5052 source_master_ns=121802017612 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5214790
[2026-10-26 14:34:22.446] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=5096 source_master_ns=122823695616 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5260136
[in @ 00000133891d6a80] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000133891d6a80] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 1212.759033
[aac @ 00000133eec08f00] Ignoring attempt to flush encoder that doesn't support it
[aac @ 00000133eec08f00] Qavg: 612.473
[aac @ 00000133eec08f00] 2 frames left in the queue on closing
[CLI] realtime video runtime failed: NotInitialized: realtime runtime made no progress before timeout
frame= 3589 fps= 30 q=-1.0 size=  118528KiB time=00:01:59.63 bitrate=8116.3kbits/s speed=   1x elapsed=0:01:59.12    
[out#0/rtp @ 000001d2c4477640] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
[out#1/rtp @ 000001d2c62f1540] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
seconds=1.0230571 cpu=2.01323962358415 ws=160301056 private=447836160
seconds=2.0467976 cpu=1.04063569731871 ws=161288192 private=448622592
seconds=3.0763053 cpu=0.620883695629033 ws=162611200 private=449871872
seconds=4.103242 cpu=1.17571644253863 ws=163807232 private=449875968
seconds=5.1305606 cpu=2.2122905909883 ws=195444736 private=708157440
seconds=6.1603351 cpu=2.75876814866662 ws=196018176 private=708599808
seconds=7.1874657 cpu=1.79781510656085 ws=196395008 private=708976640
seconds=8.2186562 cpu=1.30861544804943 ws=197271552 private=709885952
seconds=9.2490528 cpu=1.86104421963702 ws=198504448 private=711090176
seconds=10.3685609 cpu=2.47419948425238 ws=198524928 private=711090176
seconds=11.4025005 cpu=1.99204972022456 ws=198516736 private=711090176
seconds=12.4289824 cpu=2.14490342738098 ws=198537216 private=711098368
seconds=13.4622086 cpu=2.33711913932567 ws=198537216 private=711098368
seconds=14.4911637 cpu=1.79462729626483 ws=198651904 private=711098368
seconds=15.5267526 cpu=1.23447546696289 ws=200192000 private=712474624
seconds=16.5563752 cpu=2.13836074058062 ws=200138752 private=712388608
seconds=17.5879573 cpu=2.13429890403735 ws=200204288 private=712388608
seconds=18.6221214 cpu=1.44220561584691 ws=200204288 private=712388608
seconds=19.6526644 cpu=1.92969760954794 ws=199008256 private=711114752
seconds=20.6918831 cpu=1.64021822792975 ws=199008256 private=711114752
seconds=21.7226402 cpu=1.65368296230552 ws=199012352 private=711114752
seconds=22.7473114 cpu=2.21800639339456 ws=199086080 private=711114752
seconds=23.7836214 cpu=1.71335621755863 ws=199086080 private=711114752
seconds=24.8187506 cpu=1.98976039986998 ws=199086080 private=711114752
seconds=25.8518429 cpu=2.06243122534339 ws=199307264 private=711114752
seconds=26.8892649 cpu=1.84844126726023 ws=199307264 private=711114752
seconds=27.9112899 cpu=2.01527270948274 ws=199307264 private=711114752
seconds=28.9418635 cpu=2.34313466527061 ws=199307264 private=711114752
seconds=29.9683693 cpu=1.7297205547384 ws=199364608 private=711114752
seconds=30.9979086 cpu=1.51766911666218 ws=199364608 private=711114752
seconds=32.0289009 cpu=1.65330570804986 ws=199364608 private=711114752
seconds=33.0528839 cpu=1.59526352221934 ws=199364608 private=711114752
seconds=34.0909834 cpu=1.91565101768796 ws=199766016 private=711458816
seconds=35.1259352 cpu=1.85285308587669 ws=199766016 private=711458816
seconds=36.1564806 cpu=2.34319878316154 ws=199766016 private=711458816
seconds=37.1860137 cpu=2.20753200914792 ws=199766016 private=711458816
seconds=38.2151901 cpu=2.41532496717321 ws=199766016 private=711458816
seconds=39.247818 cpu=1.8570228795519 ws=199766016 private=711458816
seconds=40.2785112 cpu=1.51597002871466 ws=200675328 private=712368128
seconds=41.3055796 cpu=1.86707490597864 ws=199823360 private=711458816
seconds=42.3332618 cpu=1.65863090218499 ws=200163328 private=711802880
seconds=43.3714021 cpu=1.91557573059862 ws=200167424 private=711802880
seconds=44.3955447 cpu=1.66436339484897 ws=200167424 private=711802880
seconds=45.4203632 cpu=2.56420127963237 ws=200167424 private=711802880
seconds=46.4320406 cpu=2.2464940629565 ws=200167424 private=711802880
seconds=47.4573181 cpu=1.80106450116276 ws=200167424 private=711802880
seconds=48.4909425 cpu=1.85523255484646 ws=200167424 private=711802880
seconds=49.5219148 cpu=1.5155596323975 ws=200167424 private=711802880
seconds=50.5500546 cpu=1.45065610019889 ws=200224768 private=711802880
seconds=51.583687 cpu=1.92392998094523 ws=200224768 private=711802880
seconds=52.618976 cpu=1.57784225204046 ws=200224768 private=711802880
seconds=53.640493 cpu=1.80769474134147 ws=200232960 private=711802880
seconds=54.6641478 cpu=1.3182489040073 ws=200224768 private=711802880
seconds=55.6983542 cpu=1.23612568140083 ws=200224768 private=711802880
seconds=56.7337756 cpu=1.64623355722168 ws=200224768 private=711802880
seconds=57.7675014 cpu=1.71763941832368 ws=201125888 private=712704000
seconds=58.8037387 cpu=1.8505545364596 ws=200237056 private=711811072
seconds=59.8476838 cpu=1.22459417732703 ws=200232960 private=711811072
seconds=60.8827638 cpu=1.7153922226477 ws=200232960 private=711708672
seconds=61.9232804 cpu=1.97945817578411 ws=200232960 private=711708672
seconds=62.9550839 cpu=1.65200588537009 ws=200232960 private=711708672
seconds=63.9929444 cpu=1.77922843107615 ws=200232960 private=711708672
seconds=65.0307543 cpu=2.05305597699716 ws=200241152 private=711708672
seconds=66.0571533 cpu=1.24552838702015 ws=200232960 private=711708672
seconds=67.0881237 cpu=1.30889482198695 ws=201191424 private=712548352
seconds=68.114946 cpu=1.5908524067628 ws=201195520 private=712548352
seconds=69.1408965 cpu=1.73065677322463 ws=201199616 private=712548352
seconds=70.1795091 cpu=1.77794002219008 ws=200101888 private=711397376
seconds=71.2019942 cpu=1.59760051982443 ws=200110080 private=711397376
seconds=72.2336097 cpu=2.06538368043308 ws=200110080 private=711397376
seconds=73.2582957 cpu=2.07935096037402 ws=200101888 private=711397376
seconds=74.2870257 cpu=1.58790229435589 ws=200101888 private=711397376
seconds=75.323307 cpu=2.12462054989755 ws=201043968 private=712282112
seconds=76.3620098 cpu=1.23077466519691 ws=201043968 private=712282112
seconds=77.4009694 cpu=1.50390833291304 ws=201109504 private=712282112
seconds=78.4435541 cpu=1.63492275931677 ws=201113600 private=712282112
seconds=79.4756444 cpu=1.58273237067797 ws=201117696 private=712286208
seconds=80.5136742 cpu=2.05262105016813 ws=201248768 private=712286208
seconds=81.5490681 cpu=2.05784660135801 ws=201240576 private=712286208
seconds=82.5733616 cpu=1.80279471566588 ws=201302016 private=712286208
seconds=83.6081363 cpu=2.19634986507427 ws=201302016 private=712286208
seconds=84.6356231 cpu=1.65894632860047 ws=201326592 private=712318976
seconds=85.6662599 cpu=1.72278748616214 ws=201326592 private=712318976
seconds=86.7038615 cpu=1.30052981624336 ws=202227712 private=713220096
seconds=87.7303973 cpu=2.00641720523442 ws=201326592 private=712318976
seconds=88.7590569 cpu=1.65705492326661 ws=201326592 private=712318976
seconds=89.7970134 cpu=1.23165960318096 ws=201326592 private=712318976
seconds=90.8292524 cpu=1.85772252004006 ws=201326592 private=712318976
seconds=91.8564988 cpu=1.65933456135301 ws=201383936 private=712318976
seconds=92.8857864 cpu=1.58704207383118 ws=201383936 private=712318976
seconds=93.91445 cpu=2.00226691302102 ws=201396224 private=712327168
seconds=94.9441543 cpu=2.13819107626778 ws=202293248 private=713228288
seconds=95.9872723 cpu=1.90643471173573 ws=201515008 private=712327168
seconds=97.0289224 cpu=1.90912127175562 ws=201515008 private=712327168
seconds=98.0656533 cpu=2.67175056095692 ws=201510912 private=712327168
seconds=99.1013779 cpu=1.1657407419273 ws=201510912 private=712327168
seconds=100.1388558 cpu=1.57451327616013 ws=201859072 private=712695808
seconds=101.1748406 cpu=2.19378438055007 ws=201859072 private=712695808
seconds=102.2034978 cpu=2.2094117192076 ws=201867264 private=712695808
seconds=103.2332399 cpu=2.27605533463183 ws=201867264 private=712695808
seconds=104.2685494 cpu=2.74401914684362 ws=201871360 private=712695808
seconds=105.2938866 cpu=2.21656570416765 ws=201879552 private=712695808
seconds=106.3320776 cpu=1.84707210557943 ws=201879552 private=712695808
seconds=107.3673673 cpu=2.74407162643375 ws=201883648 private=712695808
seconds=108.3863615 cpu=1.88186903945441 ws=202256384 private=713064448
seconds=109.4231592 cpu=1.84955429237897 ws=202248192 private=713064448
seconds=110.4634128 cpu=2.4578796764733 ws=200994816 private=711811072
seconds=111.4988848 cpu=1.85192225030096 ws=200994816 private=711811072
seconds=112.5219286 cpu=2.15211171354985 ws=200994816 private=711811072
seconds=113.5577607 cpu=2.05697604677612 ws=200994816 private=711811072
seconds=114.595982 cpu=1.98383436258636 ws=201457664 private=712351744
seconds=115.6356781 cpu=2.45919762690096 ws=201457664 private=712351744
seconds=116.6635136 cpu=1.45108558006342 ws=201535488 private=712351744
seconds=117.6927047 cpu=1.65619917870012 ws=201535488 private=712351744
seconds=118.7238782 cpu=1.58413955291977 ws=201535488 private=712351744
seconds=119.7539327 cpu=1.65481093917406 ws=193077248 private=596279296
seconds=120.7839342 cpu=1.10326405967722 ws=193081344 private=596279296
seconds=121.8116038 cpu=1.03665702390235 ws=193077248 private=597307392
seconds=122.8426903 cpu=1.23986599660561 ws=193077248 private=597409792
seconds=123.8564547 cpu=1.05087622833363 ws=193077248 private=597409792
seconds=124.886547 cpu=1.17211473538475 ws=85463040 private=470507520
wasapi debug: maximum latency: 000 ns
main warning: playback too late (92893): up-sampling
main warning: picture is too late to be displayed (missing 49 ms)
main debug: picture might be displayed late (missing 16 ms)
main debug: picture might be displayed late (missing 3 ms)
main error: Failed to create video converter
main error: write error: No error
main error: read error: No error
main error: Failed to create video converter
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r353-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r353-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r353-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r353-native-2026-10-26-14h32m59s621.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r353-native-2026-10-26-14h34m02s148.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r353-output.sdp
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r353-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r353-source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r353-vlc.log
~~~~

快照通过核验TCP63020归属43708后发送RC snapshot换行命令两次，时间14:32:59、14:34:02；没有系统截图。CLI/source均已结束；待RC quit关闭本轮VLC后按以上九文件逐项清理。未执行caller-stop命令。

VLC退出后日志尾部：
~~~~text
main debug: destroying
main debug: saving media library to file C:\Users\ROG\AppData\Roaming\vlc\ml.xspf.tmp43708
main debug: looking for playlist export module matching "export-xspf": 4 candidates
main debug: using playlist export module "export"
main debug: removing module "export"
main debug: deleting item `Media Library'
main debug: deleting item `rtp://127.0.0.1:62024'
main debug: deleting item `Playlist'
main debug: removing module "memory"
-- logger module stopped --
~~~~

清理完成：九项产物已按清单删除，r353前缀无残留；42604/20748/43708均无进程残留。监控会话30059 exit1（CLI1、源0），VLC经RC quit结束。未生成远程产物，复用源保留。
