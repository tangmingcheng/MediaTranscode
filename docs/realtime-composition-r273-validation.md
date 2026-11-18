# r273：直接序列化实流复验

状态：失败，归档后清理。2026-10-20 Release SHA256 5F6A4289712A527397441A4E58359138087090275546F575F9C6CC96909689B0。三路H264720p30约8Mbps完整120秒，所选AAC44100双声道；输出HEVC720p30 8000kbps CBR、AAC192kbps CBR、MPEGTS/RTP。仅realtime。

会话32788，VLC19160、CLI28084，源29340/29236/10252。

## 实际命令
~~~powershell
$ErrorActionPreference='Stop'
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r273-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r273-native- --snapshot-format=png rtp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r273-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r273-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r273-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r273-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r273-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r273-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r273 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r273-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r273-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r273-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r273-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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
~~~

## 结果
CLI28084退出1，三个原始源均退出0、完整120秒3600帧。09:09:49 VLC原生快照三格正常、右下黑；CLI当时存活。09:11:07发送调度GBRA固定完成deadline失败，sequence69263，now102308762800减deadline102302032308为6730492ns。未进入失源与重入阶段，不构成验收通过。VLC19160通过RC quit关闭。

最终errors/workerErrors=2，CPU均值2.068113%、峰值8.932462%，WS峰值269094912B；payload bytes/objects归零，reservations/releases=118632。最长canvas60.6733ms，batch_completion60.5413ms，三路query均Ready；该最长样本不等于失败瞬间，不能直接认定因果。

### 关键原始证据
~~~text
[2026-10-20 09:11:07.599] [info] [graph][runtime.node] scheduled_datagram_sender stage=failed generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=10493 committed_datagrams=69262 committed_payload_bytes=83992032 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=80004 pacing_reserved=69262 pacing_cancelled=0 pacing_submitted=69262 pacing_maximum_submit_lateness_ns=39730480 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=1 backlog_current_wire_bytes=228 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=80004 backlog_max_residence_ns=82468900 last_materialized_sequence=69263 last_scheduled_sequence=69263 last_submitted_sequence=69262 last_committed_sequence=69262 shared_scope_charged_datagrams=69262 shared_scope_charged_wire_bytes=85931368 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2155 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2155 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=379902 endpoint_1_target_effective_socket_bytes=189951 endpoint_1_api_requested_socket_bytes=189951 endpoint_1_effective_socket_bytes=189951 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=69238 endpoint_1_committed_payload_bytes=83990400 endpoint_2_target_effective_socket_bytes=189951 endpoint_2_api_requested_socket_bytes=189951 endpoint_2_effective_socket_bytes=189951 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=24 endpoint_2_committed_payload_bytes=1632
[2026-10-20 09:11:07.798] [info] [graph][runtime.node] canvas_compose_correlated scope=attempt begin_steady_ns=1629561220500 end_steady_ns=1629621893800 wall_ns=60673300 thread_cpu_ns=0 surface_count=1 surface_total_ns=1100 surface_maximum_ns=1100 surface_maximum_tile=none surface_maximum_result=unavailable validate_count=3 validate_total_ns=1700 validate_maximum_ns=900 validate_maximum_tile=0 validate_maximum_result=0 black_count=1 black_total_ns=51500 black_maximum_ns=51500 black_maximum_tile=none black_maximum_result=0 tile_count=3 tile_total_ns=42100 tile_maximum_ns=17600 tile_maximum_tile=0 tile_maximum_result=0 publish_count=1 publish_total_ns=9300 publish_maximum_ns=9300 publish_maximum_tile=none publish_maximum_result=0 context_push_count=1 context_push_total_ns=2000 context_push_maximum_ns=2000 context_push_maximum_tile=none context_push_maximum_result=0 source_wait_count=4 source_wait_total_ns=2200 source_wait_maximum_ns=1600 source_wait_maximum_tile=0 source_wait_maximum_result=0 plane_copy_count=8 plane_copy_total_ns=79800 plane_copy_maximum_ns=41500 plane_copy_maximum_tile=none plane_copy_maximum_result=0 destination_wait_count=0 destination_wait_total_ns=0 destination_wait_maximum_ns=0 destination_wait_maximum_tile=none destination_wait_maximum_result=unavailable context_pop_count=1 context_pop_total_ns=7200 context_pop_maximum_ns=7200 context_pop_maximum_tile=none context_pop_maximum_result=0 batch_completion_count=1 batch_completion_total_ns=60541300 batch_completion_maximum_ns=60541300 batch_completion_maximum_tile=none batch_completion_maximum_result=0 source_query_completed_count=0 source_query_ready_count=3 source_query_ready_maximum_ns=3600 source_query_ready_begin_steady_ns=1629561282200 source_query_ready_end_steady_ns=1629561285800 source_query_ready_tile=0 source_query_ready_operation=event_query source_query_ready_native=0 source_query_ready_restore_native=0 source_query_pending_count=0 source_query_unavailable_count=0 source_query_error_count=0
[2026-10-20 09:11:07.937] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=failed detail="Datagram GBRA reservation misses its immutable completion deadline sequence=69263 now_ns=102308762800 release_ns=102235365640 deadline_ns=102302032308 not_before_ns=102296074760 not_after_ns=102301995828 duration_ns=36480 rate=6250000 queue_wire_bytes=228 average_queue_residence_ns=12879700" description="D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r273-output.sdp" error=6 native=0 stage=3 reason="Datagram GBRA reservation misses its immutable completion deadline sequence=69263 now_ns=102308762800 release_ns=102235365640 deadline_ns=102302032308 not_before_ns=102296074760 not_after_ns=102301995828 duration_ns=36480 rate=6250000 queue_wire_bytes=228 average_queue_residence_ns=12879700"
[CLI] final runtime report: queued=0, peakQueued=932, threads=82, processThreads=130, workers=0, workerProgress=270179, workerProcessCalls=990533, workerWaits=720347, workerWakeups=712914, workerErrors=2, errors=2, stalledIntervals=0, cpuSamples=298, logicalProcessors=22, averageSystemMachineCpuPercent=7.171084, averageProcessMachineCpuPercent=2.068113, peakProcessMachineCpuPercent=8.932462, averageProcessSingleCoreCpuPercent=45.498492, peakProcessSingleCoreCpuPercent=196.514161, initialWorkingSetBytes=207224832, workingSetBytes=269004800, peakWorkingSetBytes=269094912, totalPushed=248830, totalPopped=248157, droppedBuffers=0, encodedPacketsPushed=32540, encodedPacketsPopped=32540, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=17604654, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=758, graphPayloadReservations=118632, graphPayloadReleases=118632, graphPayloadPressureFailures=0, lastInputReceivedAtNs=1661502274300
~~~
### 标准错误
~~~text
[in @ 000001e874b03a80] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001e874b03a80] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 41027.376211
[in @ 000001e874c06480] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001e874c06480] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 12564.715533
[in @ 000001e87a88ffc0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001e87a88ffc0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 268.574667
[in @ 000001e8751e3a40] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001e874f3ff80] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001e874f3ff80] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 41027.376211
[in @ 000001e8751e10c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001e8751e10c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 268.574667
[in @ 000001e8751e3a40] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 12564.715533
[aac @ 000001e87a4749c0] Qavg: 660.435
[aac @ 000001e87a4749c0] 2 frames left in the queue on closing
[CLI] realtime video runtime failed: IoFailure: Datagram GBRA reservation misses its immutable completion deadline sequence=69263 now_ns=102308762800 release_ns=102235365640 deadline_ns=102302032308 not_before_ns=102296074760 not_after_ns=102301995828 duration_ns=36480 rate=6250000 queue_wire_bytes=228 average_queue_residence_ns=12879700

~~~

源0 结束记录：
~~~text
[out#1/rtp @ 000002382c803c00] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.53    
~~~

源1 结束记录：
~~~text
[out#0/rtp @ 00000200a68346c0] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.51    
~~~

源2 结束记录：
~~~text
[out#0/rtp @ 00000202dd9e4040] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.50    
~~~

清理清单：
composition-r273-cli-error.log
composition-r273-cli.log
composition-r273-monitor.log
composition-r273-native-2026-10-20-09h09m49s273.png
composition-r273-output.sdp
composition-r273-source0-stdout.log
composition-r273-source0.log
composition-r273-source1-stdout.log
composition-r273-source1.log
composition-r273-source2-stdout.log
composition-r273-source2.log
composition-r273-vlc.log

清理确认：12个本轮临时文件已逐项删除，五个精确PID均无残留；复用120秒测试源保留，未运行远程测试。源码核对：此次在MediaDatagramPacingController::reserve进入时now已晚于deadline，且not_before仍早于not_after，不能归因为服务速率算出的时间窗口本身无解；还需追踪准备、发布到发送线程取件之间的延迟。
