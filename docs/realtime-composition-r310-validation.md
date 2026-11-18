# r310：TS 队列 owner 修复复验（已执行，运行截止失败）

三源H264→HEVC 1280×720/30、8000kbps CBR/GOP60，AAC192kbps CBR/44100双声道，MPEG-TS/UDP→VLC。原规格120秒源；在允许本机网络的执行环境运行，VLC原生快照。两位独立复审PASS，重建结果待确认。

## 执行命令
~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r310-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r310-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r310-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r310-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r310-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r310-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r310 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62020 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r310-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r310-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r310-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r310-native- --snapshot-format=png udp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r310-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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
修复后Release738项exit0，两个CLI/cache/Ninja核验完成，未运行local。源PID37136/35928/37204均120秒自然exit0；CLI29248约103秒因不可变emission deadline失败exit1；VLC24420，原生快照确认三格视频及第四格黑色。未完成全程/失源/重入，不能记通过。

失败前8次WouldBlock：上一global reservation尚有21个datagram未提交，serial=1阻止下一预约；最后尝试103695523600ns，当前batch deadline103686578196ns，晚8945404ns。前次阻塞完成103681849400ns，距deadline4728796ns。mux出队103612436400ns，说明应继续调查全局序列串行提交与唤醒/调度等待，不能仅据错误认定mux生产过晚，不能扩大截止或丢包绕过。

A/V raw phase样本97，范围4535..4536ns；这是内部遥测，不是VLC端A/V测量。最终payload0B/0objects，预约释放118880一致，errors1，压力失败0。平均进程CPU2.459535%，峰值5.648536%，最终报告WS峰值270880768B；不证明完整TS预算闭合。

~~~~text
[2026-10-23 11:07:58.984] [info] [graph][runtime.node] protocol_batch_trace stage=materialization_failed generation=1 attempts=9 consumed_datagrams=0 remaining_datagrams=1 attempted_master_ns=103695523600 mux_dequeued_master_ns=103612436400 first_dequeued_master_ns=103612489500 last_wire_published_master_ns=103612487100 previous_attempt_started_master_ns=103681845200 previous_attempt_completed_master_ns=103681849400 last_attempt_completed_master_ns=103695526300 last_blocked_started_master_ns=103681845200 last_blocked_completed_master_ns=103681849400 blocked_attempts=8 last_blocked_code=WouldBlock last_blocked_native=0 last_blocked_original_message_bytes=293 last_blocked_message_truncated=0 last_blocked_message="wire global sequence reservation exceeds planner backlog capacity serial=1 count=0 bytes=0 request_count=1 request_bytes=216 used_count=21 used_bytes=27660 limit_count=2154 limit_bytes=2894412 generation=1 projected_sequence=70068 head_id=10600 head_first=70011 head_count=57 head_committed=36" remaining_front_release_master_ns=103619911528 remaining_front_deadline_master_ns=103686578196 original_error="InvalidArgument: MPEG-TS UDP prepared transaction exceeded its immutable emission deadline"
[2026-10-23 11:07:58.990] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=10600 committed_datagrams=70067 committed_payload_bytes=84163652 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=79296 pacing_reserved=70067 pacing_cancelled=0 pacing_submitted=70067 pacing_maximum_submit_lateness_ns=11803960 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=79296 backlog_max_residence_ns=82999200 last_materialized_sequence=70067 last_scheduled_sequence=70067 last_submitted_sequence=70067 last_committed_sequence=70067 shared_scope_charged_datagrams=70067 shared_scope_charged_wire_bytes=86125528 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2154 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2154 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=188010 endpoint_1_target_effective_socket_bytes=188010 endpoint_1_api_requested_socket_bytes=188010 endpoint_1_effective_socket_bytes=188010 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=70067 endpoint_1_committed_payload_bytes=84163652
[2026-10-23 11:07:58.993] [info] [graph][runtime.node] mpegts_wire_materializer stage=aborted maximum_materialized_after_release_ns=40803232 worst_materialized_at_ns=5660712800 worst_release_ns=5619909568 worst_deadline_ns=5719909568 worst_global_sequence=65
[2026-10-23 11:07:58.993] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 datagrams=0 wire_bytes=0 immediate_deadlines=0 deferred_deadlines=0 planned_wait_ns=0 late_datagrams=0 maximum_lateness_ns=0 pending_bytes=0 peak_pending_bytes=110920 pressure_failures=0 access_units=7168 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=1955 maximum_mux_input_after_emit_ns=6134232 maximum_mux_input_after_dispatch_ns=-93865768 worst_mux_input_ready_ns=23926043800 worst_mux_input_emit_ns=23919909568 worst_mux_input_dispatch_ns=24019909568 maximum_protocol_batch_produced_after_release_ns=10643284 worst_protocol_batch_produced_ns=83030554400 worst_protocol_batch_release_ns=83019911116 worst_protocol_batch_deadline_ns=83086577784 worst_protocol_batch_datagrams=1 exit_reason=aborted
[2026-10-23 11:07:58.996] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=between_calls_previous_return count=21467 maximum_ns=124845800 begin_steady_ns=8339632661700 end_steady_ns=8339757507500 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=waiting deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[CLI] final runtime report: queued=0, peakQueued=935, threads=81, processThreads=129, workers=0, workerProgress=271778, workerProcessCalls=1115015, workerWaits=843230, workerWakeups=835650, workerErrors=1, errors=1, stalledIntervals=0, cpuSamples=301, logicalProcessors=22, averageSystemMachineCpuPercent=14.638148, averageProcessMachineCpuPercent=2.459535, peakProcessMachineCpuPercent=5.648536, averageProcessSingleCoreCpuPercent=54.109779, peakProcessSingleCoreCpuPercent=124.267782, initialWorkingSetBytes=210739200, workingSetBytes=270880768, peakWorkingSetBytes=270880768, totalPushed=250655, totalPopped=249960, droppedBuffers=0, encodedPacketsPushed=32899, encodedPacketsPopped=32898, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=17561938, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=755, graphPayloadReservations=118880, graphPayloadReleases=118880, graphPayloadPressureFailures=0, lastInputReceivedAtNs=8437897737200
[2026-10-23 11:07:57.089] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4166 source_master_ns=102432918226 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4307870
[2026-10-23 11:07:58.111] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4210 source_master_ns=103454596230 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4353216
~~~~

## VLC快照命令
~~~~powershell
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020); try{$s=$c.GetStream();$b=[Text.Encoding]::ASCII.GetBytes("snapshot`n");$s.Write($b,0,$b.Length)}finally{$c.Dispose()}
~~~~

## 临时产物清单
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r310-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r310-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r310-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r310-native-2026-10-23-11h06m37s419.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r310-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r310-source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r310-source1-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r310-source1.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r310-source2-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r310-source2.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r310-vlc.log

清理完成：五个精确PID无残留，按清单删除11项临时产物（含VLC原生快照），保留指定120秒源；未运行远程测试。
