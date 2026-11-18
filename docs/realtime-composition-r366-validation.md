# r366 Windows Release realtime 验证

已判失败，CLI exit1；不是完整验收通过。Release5545 configure/build均0（756项），realtime6613504 bytes、2026-10-28 17:59:41；四项构建产物已核验，local未运行。本轮联合覆盖authorize scratch及reservation封印消费链的cadence回归，不替代Prepared/noncadence或完整G。初始源32912/17760/12196，CLI33116，VLC3128；主会话32137。

## 实际启动命令
```powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r366-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r366-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r366-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r366-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r366-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r366-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r366 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r366-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r366-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r366-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r366-native- --snapshot-format=png udp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r366-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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
```

## VLC快照命令
```powershell
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 3128){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
$s=$c.GetStream()
$b=[Text.Encoding]::ASCII.GetBytes("snapshot`n")
$s.Write($b,0,$b.Length)
$s.Flush()
$c.Dispose(); Start-Sleep -Seconds 2; Get-ChildItem 'D:/Code/MyCode/MediaTranscode/out/acceptance' -Filter 'composition-r366-native-*.png' | Select-Object FullName,Length
```

18:00:40.813原生快照已实际查看：左上、右上、左下三格正常视频，右下黑。尚未观察失源/重入。

18:02:32.895原生快照已实际查看全黑；初始三源日志均3600帧/120秒。选中AV源重入PID25808，会话66145；18:03:19.421原生快照已查看左下恢复，其余格黑；generation2 drift trace已出现。

## 重入命令
```powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r366-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r366-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($source0.Id)"
$source0.WaitForExit()
Write-Output "REENTRY_EXIT=$($source0.ExitCode)"
exit $source0.ExitCode
```

## 失败结论与必要诊断

CLI在18:03:13.870发送预约失败，主会话32137退出1；初始三源均exit0。第三张18:03:19.421快照晚于CLI失败，只证明VLC保留恢复画面，不证明持续播放。未取得重入结束后再次黑屏证据。当前重入源PID25808与VLC3128将按精确身份清理；不会调用CLI caller-stop。

发送sequence100856：not_before184908173840晚于not_after184908124664，差49176ns，无法在不可变期限内安排215040ns服务。activation_completed到permit_started相隔65862000ns；这段日志不能单独证明是Windows调度、锁竞争或其他原因。generation2首帧interval miss同时存在，不能以恢复截图掩盖。

```text
[2026-10-28 18:03:13.870] [info] [graph][runtime.node] datagram_reservation_trace stage=reservation_failed generation=1 endpoint=1 sequence=100856 release_master_ns=184841704001 deadline_master_ns=184908339704 original_protocol_deadline_master_ns=184941704001 prefix_completion_limit_master_ns=184908339704 activation_started_master_ns=184842105200 activation_completed_master_ns=184842105600 permit_started_master_ns=184907967600 permit_completed_master_ns=184907967900 snapshot_started_master_ns=184907968300 snapshot_sampled_master_ns=184907968400 snapshot_completed_master_ns=184907968600 reservation_completed_master_ns=184908001700 wait_started_master_ns=unavailable wait_completed_master_ns=unavailable submit_observed_master_ns=unavailable wait_attempts=0 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=0 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=0 wait_call_total_ns=0 wait_call_max_ns=0 wait_before_call_count=0 wait_before_call_total_ns=0 wait_before_call_max_ns=0 wait_arm_count=0 wait_arm_total_ns=0 wait_arm_max_ns=0 wait_api_wall_count=0 wait_api_wall_total_ns=0 wait_api_wall_max_ns=0 wait_return_to_next_clock_count=0 wait_return_to_next_clock_total_ns=0 wait_return_to_next_clock_max_ns=0 wait_target_lateness_count=0 wait_target_lateness_total_ns=0 wait_target_lateness_max_ns=0 original_error="IoFailure: Datagram GBRA reservation misses its immutable completion deadline sequence=100856 now_ns=184907968400 release_ns=184841704001 deadline_ns=184908339704 not_before_ns=184908173840 not_after_ns=184908124664 duration_ns=215040 rate=6250000 queue_wire_bytes=10752 average_queue_residence_ns=65961700"
[2026-10-28 18:03:13.871] [info] [graph][runtime.node] process.failed node=81 name=MediaScheduledDatagramSenderNode kind=ScheduledDatagramSender inputs=[channel=111 edge=111 stream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=123 edge=123 stream=Metadata edge_kind=Unknown payload=Unknown queued=0] outputs=[] error=IoFailure: Datagram GBRA reservation misses its immutable completion deadline sequence=100856 now_ns=184907968400 release_ns=184841704001 deadline_ns=184908339704 not_before_ns=184908173840 not_after_ns=184908124664 duration_ns=215040 rate=6250000 queue_wire_bytes=10752 average_queue_residence_ns=65961700
[2026-10-28 18:03:13.886] [info] [graph][runtime.node] scheduled_datagram_sender stage=failed generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=19434 committed_datagrams=100855 committed_payload_bytes=116129668 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=79296 pacing_reserved=100855 pacing_cancelled=0 pacing_submitted=100855 pacing_maximum_submit_lateness_ns=37143045 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=8 backlog_current_wire_bytes=10752 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=79296 backlog_max_residence_ns=69202400 last_materialized_sequence=100863 last_scheduled_sequence=100863 last_submitted_sequence=100855 last_committed_sequence=100855 submitted_service_generation=1 submitted_service_sequence=100855 submitted_service_completed_ns=184907958800 submitted_service_duration_ns=215040 submitted_service_available_ns=184908173840 submitted_scope_available_ns=184908173840 shared_scope_charged_datagrams=100855 shared_scope_charged_wire_bytes=118953608 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2154 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2154 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=188010 endpoint_1_target_effective_socket_bytes=188010 endpoint_1_api_requested_socket_bytes=188010 endpoint_1_effective_socket_bytes=188010 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=100855 endpoint_1_committed_payload_bytes=116129668
[2026-10-28 18:03:13.961] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 produced_datagrams=100863 produced_ts_payload_bytes=116140196 pending_bytes=0 peak_pending_bytes=110920 access_units=13134 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=32789115 maximum_mux_input_after_emit_ns=50550868 maximum_mux_input_after_dispatch_ns=-49449132 worst_mux_input_ready_ns=79160917000 worst_mux_input_emit_ns=79110366132 worst_mux_input_dispatch_ns=79210366132 maximum_protocol_batch_produced_after_release_ns=55051924 worst_protocol_batch_produced_ns=25563423000 worst_protocol_batch_release_ns=25508371076 worst_protocol_batch_deadline_ns=25575037744 worst_protocol_batch_datagrams=1 exit_reason=aborted
[2026-10-28 18:03:13.966] [info] [graph][runtime.node] continuous_aggregate.summary reason=abort output_group=composition-r366.output output_generation=1 video_commits=5397 source[0].group=composition-r366.source.0 source[0].real_frames=3419 source[0].black_frames=1978 source[0].contribution_generation=0 source[1].group=composition-r366.source.1 source[1].real_frames=3418 source[1].black_frames=1979 source[1].contribution_generation=0 source[2].group=composition-r366.source.2 source[2].real_frames=4008 source[2].black_frames=1389 source[2].contribution_generation=2 source[2].first_interval_miss_cursor=4808 source[2].first_interval_miss_target_ns=165475037334 source[2].first_interval_miss_steady_ns=30362188387700 source[2].first_interval_miss_generation=2 source[2].first_interval_miss_capacity=7 source[2].first_interval_miss_before=0 source[2].first_interval_miss_after=0 source[2].first_interval_miss_expired=0 source[2].first_interval_miss_channel_depth=0 source[2].first_interval_miss_examined_begin_ns=unavailable source[2].first_interval_miss_examined_end_ns=unavailable source[2].first_interval_miss_epoch_source_start_ns=994000000 source[2].first_interval_miss_epoch_master_release_ns=165405558300 source[2].first_interval_miss_prepare_expired=0 source[2].first_interval_miss_prepare_old_generation=0 source[2].first_interval_miss_prepare_incomplete=0 source[2].first_interval_miss_first_real_cursor_at_miss=unavailable source[2].first_interval_miss_first_real_steady_ns_at_miss=unavailable source[2].first_interval_miss_first_candidate_steady_ns_at_miss=unavailable source[2].first_interval_miss_first_candidate_begin_ns_at_miss=unavailable source[2].first_interval_miss_first_candidate_end_ns_at_miss=unavailable source[2].first_interval_miss_prepare_last_begin_ns=unavailable source[2].first_interval_miss_prepare_last_end_ns=unavailable source[2].first_interval_miss_first_candidate_steady_ns_final=30362200715100 source[2].first_interval_miss_first_candidate_begin_ns_final=165505558300 source[2].first_interval_miss_first_candidate_end_ns_final=165538891633 source[2].first_interval_miss_first_real_cursor=4809 source[2].first_interval_miss_first_real_steady_ns=30362222281900 source[2].first_interval_miss_cursor_committed_generation=0 source[2].first_interval_miss_cursor_committed_steady_ns=30362188411300
[2026-10-28 18:03:14.093] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=failed detail="Datagram GBRA reservation misses its immutable completion deadline sequence=100856 now_ns=184907968400 release_ns=184841704001 deadline_ns=184908339704 not_before_ns=184908173840 not_after_ns=184908124664 duration_ns=215040 rate=6250000 queue_wire_bytes=10752 average_queue_residence_ns=65961700" description="" error=6 native=0 stage=3 reason="Datagram GBRA reservation misses its immutable completion deadline sequence=100856 now_ns=184907968400 release_ns=184841704001 deadline_ns=184908339704 not_before_ns=184908173840 not_after_ns=184908124664 duration_ns=215040 rate=6250000 queue_wire_bytes=10752 average_queue_residence_ns=65961700"
[CLI] final runtime report: queued=0, peakQueued=919, threads=81, processThreads=129, workers=0, workerProgress=383958, workerProcessCalls=1441052, workerWaits=1057087, workerWakeups=1043378, workerErrors=2, errors=2, stalledIntervals=0, cpuSamples=573, logicalProcessors=22, averageSystemMachineCpuPercent=8.226484, averageProcessMachineCpuPercent=1.753084, peakProcessMachineCpuPercent=5.947137, averageProcessSingleCoreCpuPercent=38.567853, peakProcessSingleCoreCpuPercent=130.837004, initialWorkingSetBytes=206557184, workingSetBytes=255496192, peakWorkingSetBytes=267894784, totalPushed=329169, totalPopped=329120, droppedBuffers=0, encodedPacketsPushed=60306, encodedPacketsPopped=60300, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=15883100, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=744, graphPayloadReservations=134880, graphPayloadReleases=134880, graphPayloadPressureFailures=0, lastInputReceivedAtNs=30381756627300
```

## 收尾结果

重入源最终自然完成3600帧/120秒，会话66145 exit0；初始三个源均exit0。CLI为内部错误exit1，不是caller_stop或清理退出。VLC通过RC quit关闭；六个精确PID32912/17760/12196/33116/3128/25808已核对全部不存在。没有再获取黑屏快照，失败后的VLC画面不能补算第四阶段。

外部监控180样本：CPU均值1.70849874994612%、峰3.48328612977177%；WS峰268513280，private峰1274142720。内部CPU均值1.753084%、峰5.947137%，最终WS255496192；134条drift的raw_phase绝对最大13606ns。仅为内部时序，不证明内容级A/V。最终payload134880预约=释放、0字节/0对象，pressure0；workerErrors/errors均2，stalled0。

发送100855个datagram、116129668 payload字节；实际delivery仍not_proven。deadline_misses字段为0不否定本轮明确的预约期限错误。video5397 commits；source0 real3419/black1978、source1 real3418/black1979、source2 real4008/black1389。audio7747 commits，real5892096/silence2040832 samples。

独立只读诊断确认：activation_completed为整批时间，65.862ms并非单次停顿。batch100831–100863在失败前已发送25包，等待调用累计64.7587ms；最坏100855请求211140ns（Windows due=-2112），API wall29761500ns，target lateness29555260ns，PID33116/TID15248/QPC10MHz/result1。与r359属于同类等待区间异常长现象，不能认定同一根因。最早完成超prefix deadline49176ns，拒绝发送算术正确；不能使用晚33.364297ms的原protocol deadline放宽约束。现有采样不能区分timer信号迟到与线程调度迟到，也不能归因到本轮存储改动。

### VLC关闭实际命令
```powershell
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 3128){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
$s=$c.GetStream()
$b=[Text.Encoding]::ASCII.GetBytes("quit`n")
$s.Write($b,0,$b.Length)
$c.Dispose()
```

### 等待诊断、音频汇总与stderr
```text
[2026-10-28 18:03:13.886] [info] [graph][runtime.node] datagram_batch_trace stage=active generation=1 first_sequence=100831 last_sequence=100863 maximum_submit_interval_ns=100900 maximum_submit_interval_sequence=100838 maximum_start_lateness_ns=29557160 maximum_start_lateness_sequence=100855 first_release_master_ns=184841704001 first_deadline_master_ns=184908339704 last_release_master_ns=184841704001 last_deadline_master_ns=184908339704 first_submitted_master_ns=184842186400 last_submitted_master_ns=184907958800 worst_wait_started_master_ns=184878093200 worst_wait_completed_master_ns=184907859600 worst_submit_observed_master_ns=184907861500 wait_attempts=25 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=25 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=25 wait_call_total_ns=64758700 wait_call_max_ns=29765500 wait_before_call_count=25 wait_before_call_total_ns=2000 wait_before_call_max_ns=200 wait_arm_count=25 wait_arm_total_ns=80100 wait_arm_max_ns=6300 wait_api_wall_count=25 wait_api_wall_total_ns=64655900 wait_api_wall_max_ns=29761500 wait_return_to_next_clock_count=25 wait_return_to_next_clock_total_ns=21600 wait_return_to_next_clock_max_ns=2600 wait_target_lateness_count=25 wait_target_lateness_total_ns=59745980 wait_target_lateness_max_ns=29555260 worst_wait_metric=call_wall worst_wait_sequence=100855 worst_wait_target_master_ns=184878304340 worst_wait_sampled_master_ns=184878093200 worst_wait_sampled_steady_ns=30381757642400 worst_wait_timeout_ns=211140 worst_wait_entered_steady_ns=30381757642400 worst_wait_returned_steady_ns=30381787407900 worst_wait_arm_before_steady_ns=30381757642700 worst_wait_arm_after_steady_ns=30381757646000 worst_wait_api_before_steady_ns=30381757646000 worst_wait_api_returned_steady_ns=30381787407500 worst_wait_windows_due_100ns=-2112 worst_wait_windows_result=1 worst_wait_windows_error=unavailable worst_wait_windows_pid=33116 worst_wait_windows_tid=15248 worst_wait_windows_qpc_frequency_hz=10000000 worst_wait_next_master_ns=184907859600 worst_wait_next_steady_ns=30381787408800 worst_wait_outcome=deadline
[2026-10-28 18:03:13.964] [info] [graph][runtime.node] continuous_audio_aggregate.summary reason=abort output_group=composition-r366.output audio_commits=7747 real_samples=5892096 silence_samples=2040832
[in @ 00000201476bde00] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000201476bde00] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 39060.110967
[in @ 000002014d0a3780] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002014d0a3780] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 37097.934622
[in @ 0000020147a4c380] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000020147a4c380] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 10838.285389
[in @ 0000020146a358c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002014dbf8940] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002014dbf8940] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 37097.934622
[in @ 0000020146a358c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 10838.285389
[in @ 000002014dbf9940] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002014dbf9940] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 39060.110967
[aac @ 000002014cf87f00] Qavg: 17322.709
[aac @ 000002014cf87f00] 2 frames left in the queue on closing
[CLI] realtime video runtime failed: IoFailure: Datagram GBRA reservation misses its immutable completion deadline sequence=100856 now_ns=184907968400 release_ns=184841704001 deadline_ns=184908339704 not_before_ns=184908173840 not_after_ns=184908124664 duration_ns=215040 rate=6250000 queue_wire_bytes=10752 average_queue_residence_ns=65961700
```

### VLC错误与警告
```text
faad warning: decoded zero sample
d3d11va warning: surface dimensions (1280x768) differ from avcodec dimensions (1280x736)
d3d11va warning: not enough decoding slices in the texture (6/28)
main warning: playback too late (143886): up-sampling
main warning: picture is too late to be displayed (missing 108 ms)
main warning: picture is too late to be displayed (missing 75 ms)
main warning: picture is too late to be displayed (missing 41 ms)
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
main warning: picture is too late to be displayed (missing 1283 ms)
main warning: picture is too late to be displayed (missing 1250 ms)
main warning: picture is too late to be displayed (missing 1217 ms)
main warning: picture is too late to be displayed (missing 1183 ms)
main warning: picture is too late to be displayed (missing 1150 ms)
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
main warning: buffer too late (-452386 us): dropped
main warning: buffer too late (-429166 us): dropped
main warning: buffer too late (-405946 us): dropped
main warning: buffer too late (-382726 us): dropped
main warning: buffer too late (-359506 us): dropped
main warning: buffer too late (-336286 us): dropped
main warning: buffer too late (-313067 us): dropped
main warning: buffer too late (-289847 us): dropped
main warning: buffer too late (-266627 us): dropped
main warning: buffer too late (-243407 us): dropped
main warning: buffer too late (-220187 us): dropped
main warning: buffer too late (-196967 us): dropped
main warning: buffer too late (-173747 us): dropped
main warning: buffer too late (-150527 us): dropped
main warning: buffer too late (-127307 us): dropped
main warning: buffer too late (-106087 us): dropped
main warning: buffer too late (-82867 us): dropped
main warning: playback too late (62558): up-sampling
main error: Failed to create video converter
main error: write error: No error
main error: read error: No error
main error: Failed to create video converter
main error: read error: No error
main error: Failed to create video converter
main error: read error: No error
main warning: buffer too late (-111706736 us): dropped
main warning: picture is too late to be displayed (missing 112029 ms)
main error: read error: No error
main warning: picture is too late to be displayed (missing 112000 ms)
main warning: picture is too late to be displayed (missing 111967 ms)
main warning: picture is too late to be displayed (missing 111934 ms)
main warning: picture is too late to be displayed (missing 111900 ms)
direct3d11 error: SetThumbNailClip failed: 0x800706f4
```

### 源结尾
```text
source0
[out#0/rtp @ 000002203e688fc0] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
[out#1/rtp @ 000002204023adc0] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.50    
source1
frame= 3591 fps= 30 q=-1.0 size=  118561KiB time=00:01:59.70 bitrate=8114.0kbits/s speed=   1x elapsed=0:01:59.19    
[out#0/rtp @ 000001e560a55d80] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.49    
source2
frame= 3590 fps= 30 q=-1.0 size=  118545KiB time=00:01:59.66 bitrate=8115.2kbits/s speed=   1x elapsed=0:01:59.17    
[out#0/rtp @ 0000024d25cc5980] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.47    
reentry
[out#0/rtp @ 00000297aca777c0] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
[out#1/rtp @ 00000297ae775f00] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.49    
```

### 清理清单（已完成）
- composition-r366-cli-error.log (2377 bytes)
- composition-r366-cli.log (771526 bytes)
- composition-r366-monitor.log (13202 bytes)
- composition-r366-native-2026-10-28-18h00m40s813.png (877435 bytes)
- composition-r366-native-2026-10-28-18h02m32s895.png (12152 bytes)
- composition-r366-native-2026-10-28-18h03m19s421.png (351699 bytes)
- composition-r366-reentry-stdout.log (540 bytes)
- composition-r366-reentry.log (29521 bytes)
- composition-r366-source0-stdout.log (540 bytes)
- composition-r366-source0.log (29589 bytes)
- composition-r366-source1-stdout.log (335 bytes)
- composition-r366-source1.log (28926 bytes)
- composition-r366-source2-stdout.log (335 bytes)
- composition-r366-source2.log (28926 bytes)
- composition-r366-vlc.log (40855 bytes)

以上15项产物已在记录命令、结果与必要诊断后逐项删除；本轮文件匹配0，六媒体PID均不存在，指定120秒源124427809 bytes保留。本轮未产生远程产物；没有修改外部FFmpeg、运行local或提交/推送/部署。

### 重入首帧缺口的独立源码分析

cursor4808的target比epoch release晚69.479034ms，当时候选与本代first-real均为空，最终提交黑屏。首候选在miss后12.3274ms到达aggregate，但其区间从epoch release后100ms才开始，比target晚30.520966ms；即使更早到达也不能覆盖该cursor。cursor4809 target165508370667ns位于首候选区间，随后恢复。

当前miss条件使用跨代累计committedRealFrames，因此此次记录表示generation2尚未首帧的启动缺口，不是本代已恢复后中途断帧。NextAtomicRelease没有初次启动的OutputReady/reanchor握手，是后续调查点，尚不能直接认定为根因或改状态机。下一证据需关联首批AU、首decoded、首filter/preparation与首aggregate帧，区分上游区间本来缺失或中间丢弃。现有first-frame诊断仅生命周期一次，不能覆盖generation2；releaseIdentity仅在transaction上，应经sourceIdentity/generation/sourceSequence关联，不新增或伪造外部字段。发送期限失败与该首帧缺口分别保留，不混作一个原因。