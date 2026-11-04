# r359 原规格重入时序复验

状态：FAIL。重入期间发送期限失败，CLI退出1。Release realtime 6577152 bytes，2026-10-26 16:53:34；仅新增Aggregate固定诊断，原选帧策略未改。三源H264/AAC→HEVC CBR8000kbps 1280×720/30、AAC CBR192kbps、MPEGTS/UDP；指定120秒连续源。仅realtime与VLC原生快照。

## 实际启动命令

```powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r359-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r359-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r359-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r359-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r359-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r359-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r359 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r359-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r359-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r359-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r359-native- --snapshot-format=png udp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r359-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

## 重入命令

```powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r359-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r359-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($source0.Id)"
$source0.WaitForExit()
Write-Output "REENTRY_EXIT=$($source0.ExitCode)"
exit $source0.ExitCode
```

## 实际快照命令

```powershell
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 46908){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
$s=$c.GetStream()
$b=[Text.Encoding]::ASCII.GetBytes("snapshot`n")
$s.Write($b,0,$b.Length)
$s.Flush()
$c.Dispose(); Start-Sleep -Seconds 2; Get-ChildItem 'D:/Code/MyCode/MediaTranscode/out/acceptance' -Filter 'composition-r359-native-*.png' | Select-Object FullName,Length
```

## VLC 停止命令

CLI已自行失败退出，未发caller stop。核对RC端口属于46908后执行：

```powershell
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 46908){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020);$s=$c.GetStream();$b=[Text.Encoding]::ASCII.GetBytes("quit`n");$s.Write($b,0,$b.Length);$c.Dispose()
```


## 结果及失败边界

初始FFmpeg39156/41744/45260与重入10284均3600帧/120秒自然exit0；CLI39580在16:57:53.861记录submit_deadline_missed并退出1，主会话61462 exit1；重入会话95599 exit0。VLC46908经RC quit；六个精确PID查询无残留。

实际查看快照：16:55:04三格、16:56:49全黑、16:57:37左下重入。16:59:13第四张仍显示左下旧画面；此时CLI早已失败，该张不是再次失源黑屏证据。后台监测记录了CPU/内存并最终输出失败退出码，但主控制在等待重入源结束期间未及时轮询主会话，延迟发现失败；不能把源继续运行或播放器停留画面当作链路仍健康。

最终workerErrors/errors各2，stalledIntervals0，payload160209预约=释放、字节/对象归零，graphPayloadPressureFailures1。内部CPU均值2.312727%、峰值5.450734%，WS峰值272592896；外部195个约1秒样本CPU均值2.271115%、峰值3.990949%，WS峰值272592896，private峰值1269305344、最后1257766912。

sender committed122799 datagrams/144375916 payload bytes，sequence122800失败。原协议deadline201230891101ns，当前prefix完成约束deadline201197527120ns，实际提交检查deadline201197312080ns，observed201221337000ns。等待目标not_before201183696140ns，timeout201740ns，实际Windows等待API wall37827300ns，wait总37831000ns、target lateness37631160ns、Windows result1；该证据将延误定位到等待调用区间，不能只凭它断言Windows bug或修改FFmpeg。原错误完整保留，不能放宽deadline或降低源规格掩盖。

最终Aggregate summary未出现first_interval_miss，不能外推r357/r358已修复，也不能证明完整连续性，因为本轮发送链提前失败。video_commits5871；源0/1各real3418 black2453，源2 real4982 black889；audio_commits8428、real7322624/silence1307648 samples。所有A/V指标仅内部数据，不替代内容同步。

VLC迟到及音频flushing、FFmpeg range警告、AAC关闭时2帧残留全部保留。完整G、非cadence/四源、C API、全Windows矩阵和RKMPP仍未完成。本轮没有local CLI或外部FFmpeg修改。

## 关键原始证据

```text
[2026-10-26 16:57:53.861] [info] [graph][runtime.node] datagram_reservation_trace stage=submit_deadline_missed generation=1 endpoint=1 sequence=122800 release_master_ns=201130891101 deadline_master_ns=201197527120 original_protocol_deadline_master_ns=201230891101 prefix_completion_limit_master_ns=201197527120 activation_started_master_ns=201131662000 activation_completed_master_ns=201131664000 permit_started_master_ns=201183490900 permit_completed_master_ns=201183491400 snapshot_started_master_ns=201183491800 snapshot_sampled_master_ns=201183491900 snapshot_completed_master_ns=201183492000 reservation_completed_master_ns=201183493800 wait_started_master_ns=201183494400 wait_completed_master_ns=201221327300 submit_observed_master_ns=201221337000 wait_attempts=1 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=1 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=1 wait_call_total_ns=37831000 wait_call_max_ns=37831000 wait_before_call_count=1 wait_before_call_total_ns=500 wait_before_call_max_ns=500 wait_arm_count=1 wait_arm_total_ns=3100 wait_arm_max_ns=3100 wait_api_wall_count=1 wait_api_wall_total_ns=37827300 wait_api_wall_max_ns=37827300 wait_return_to_next_clock_count=1 wait_return_to_next_clock_total_ns=1600 wait_return_to_next_clock_max_ns=1600 wait_target_lateness_count=1 wait_target_lateness_total_ns=37631160 wait_target_lateness_max_ns=37631160 worst_wait_metric=call_wall worst_wait_sequence=122800 worst_wait_target_master_ns=201183696140 worst_wait_sampled_master_ns=201183494400 worst_wait_sampled_steady_ns=29538740247300 worst_wait_timeout_ns=201740 worst_wait_entered_steady_ns=29538740247800 worst_wait_returned_steady_ns=29538778078800 worst_wait_arm_before_steady_ns=29538740247800 worst_wait_arm_after_steady_ns=29538740250900 worst_wait_api_before_steady_ns=29538740250900 worst_wait_api_returned_steady_ns=29538778078200 worst_wait_windows_due_100ns=-2018 worst_wait_windows_result=1 worst_wait_windows_error=unavailable worst_wait_next_master_ns=201221327300 worst_wait_next_steady_ns=29538778080400 worst_wait_outcome=deadline original_error="IoFailure: scheduled datagram submit exceeded its original deadline generation=1 sequence=122800 endpoint=1 release_ns=201130891101 reserved_at_ns=201183491900 not_before_ns=201183696140 deadline_ns=201197312080 observed_at_ns=201221337000"
[2026-10-26 16:57:53.862] [info] [graph][runtime.node] process.failed node=81 name=MediaScheduledDatagramSenderNode kind=ScheduledDatagramSender inputs=[channel=111 edge=111 stream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=123 edge=123 stream=Metadata edge_kind=Unknown payload=Unknown queued=0] outputs=[] error=IoFailure: scheduled datagram submit exceeded its original deadline generation=1 sequence=122800 endpoint=1 release_ns=201130891101 reserved_at_ns=201183491900 not_before_ns=201183696140 deadline_ns=201197312080 observed_at_ns=201221337000
[2026-10-26 16:57:53.862] [info] [graph][runtime.node] worker.failed node=81 kind=ScheduledDatagramSender name=Common GCRA-paced nonblocking datagram sender error=IoFailure: scheduled datagram submit exceeded its original deadline generation=1 sequence=122800 endpoint=1 release_ns=201130891101 reserved_at_ns=201183491900 not_before_ns=201183696140 deadline_ns=201197312080 observed_at_ns=201221337000
[2026-10-26 16:57:53.863] [info] [graph][runtime.node] process.failed node=2 name=VideoEncodeNode kind=VideoEncode inputs=[channel=1 edge=1 stream=Video edge_kind=Metadata payload=CodecContext queued=0; channel=104 edge=104 stream=Video edge_kind=RawFrame payload=Frame queued=0] outputs=[channel=106 edge=106 stream=Video edge_kind=EncodedPacket payload=Packet queued=1; channel=113 edge=113 stream=Video edge_kind=Metadata payload=CodecContext queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-26 16:57:53.864] [info] [graph][runtime.node] process.failed node=64 name=VideoDecodeNode kind=VideoDecode inputs=[channel=84 edge=84 stream=Video edge_kind=Metadata payload=CodecContext queued=0; channel=85 edge=85 stream=Video edge_kind=InputPacket payload=Packet queued=0] outputs=[channel=87 edge=87 stream=Video edge_kind=RawFrame payload=Frame queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-26 16:57:53.878] [info] [graph][runtime.node] scheduled_datagram_sender stage=failed generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=21141 committed_datagrams=122799 committed_payload_bytes=144375916 would_block=0 writable_waits=0 deadline_misses=1 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=79296 pacing_reserved=122800 pacing_cancelled=0 pacing_submitted=122799 pacing_maximum_submit_lateness_ns=32704760 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=6 backlog_current_wire_bytes=6936 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=79296 backlog_max_residence_ns=53646500 last_materialized_sequence=122805 last_scheduled_sequence=122805 last_submitted_sequence=122799 last_committed_sequence=122799 submitted_service_generation=1 submitted_service_sequence=122799 submitted_service_completed_ns=201183481100 submitted_service_duration_ns=215040 submitted_service_available_ns=201183696140 submitted_scope_available_ns=201183696140 shared_scope_charged_datagrams=122799 shared_scope_charged_wire_bytes=147814288 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2154 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2154 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=188010 endpoint_1_target_effective_socket_bytes=188010 endpoint_1_api_requested_socket_bytes=188010 endpoint_1_effective_socket_bytes=188010 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=122799 endpoint_1_committed_payload_bytes=144375916
[2026-10-26 16:57:54.052] [info] [graph][runtime.node] continuous_audio_aggregate.summary reason=abort output_group=composition-r359.output audio_commits=8428 real_samples=7322624 silence_samples=1307648
[2026-10-26 16:57:54.053] [info] [graph][runtime.node] video_encoder_execution_observation node=2 count=5871 maximum_call_ns=24273300 maximum_begin_steady_ns=29427794518000 maximum_end_steady_ns=29427818791300 maximum_encoder_pts=2541 maximum_native_result=0 maximum_between_submissions_ns=39312700 before_gap_begin_steady_ns=29427522710900 before_gap_end_steady_ns=29427527215300 before_gap_encoder_pts=2533 before_gap_native_result=0 after_gap_begin_steady_ns=29427566528000 after_gap_end_steady_ns=29427583111900 after_gap_encoder_pts=2534 after_gap_native_result=0 payload_wait_active_begin_steady_ns=29538765970200
[2026-10-26 16:57:54.056] [info] [graph][runtime.node] continuous_aggregate.summary reason=abort output_group=composition-r359.output output_generation=1 video_commits=5871 source[0].group=composition-r359.source.0 source[0].real_frames=3418 source[0].black_frames=2453 source[0].contribution_generation=0 source[1].group=composition-r359.source.1 source[1].real_frames=3418 source[1].black_frames=2453 source[1].contribution_generation=0 source[2].group=composition-r359.source.2 source[2].real_frames=4982 source[2].black_frames=889 source[2].contribution_generation=2
[CLI] final runtime report: queued=0, peakQueued=927, threads=81, processThreads=129, workers=0, workerProgress=428009, workerProcessCalls=1735013, workerWaits=1306995, workerWakeups=1292058, workerErrors=2, errors=2, stalledIntervals=0, cpuSamples=584, logicalProcessors=22, averageSystemMachineCpuPercent=27.547760, averageProcessMachineCpuPercent=2.312727, peakProcessMachineCpuPercent=5.450734, averageProcessSingleCoreCpuPercent=50.879984, peakProcessSingleCoreCpuPercent=119.916143, initialWorkingSetBytes=209788928, workingSetBytes=262365184, peakWorkingSetBytes=272592896, totalPushed=368898, totalPopped=368846, droppedBuffers=0, encodedPacketsPushed=65608, encodedPacketsPopped=65600, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=16185990, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=748, graphPayloadReservations=160209, graphPayloadReleases=160209, graphPayloadPressureFailures=1, lastInputReceivedAtNs=29538778321500
[2026-10-26 16:57:01.721] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1 source_master_ns=149181568052 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-10-26 16:57:53.684] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2262 source_master_ns=201681885512 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2357992
[2026-10-26 16:57:00.680] [info] [graph][runtime.node] rtp_binder_boundary stage=deadline_established node=51 group=composition-r359.source.2 buffered=0 phase=2 preserves_output=1 raw_ssrc=770620457 raw_generation=1 raw_sequence=5171 old_generation=1 next_generation=2 transition_sequence=1 published_generation=1
[2026-10-26 16:57:01.215] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=1 generation=2
[2026-10-26 16:57:54.056] [info] [graph][runtime.node] continuous_aggregate.summary reason=abort output_group=composition-r359.output output_generation=1 video_commits=5871 source[0].group=composition-r359.source.0 source[0].real_frames=3418 source[0].black_frames=2453 source[0].contribution_generation=0 source[1].group=composition-r359.source.1 source[1].real_frames=3418 source[1].black_frames=2453 source[1].contribution_generation=0 source[2].group=composition-r359.source.2 source[2].real_frames=4982 source[2].black_frames=889 source[2].contribution_generation=2
source0: [out#1/rtp @ 000001bcdc9b8b00] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
source1: [out#0/rtp @ 000002da72128c40] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.47    
source2: [out#0/rtp @ 000001bfe27caf80] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.47    
reentry: [out#1/rtp @ 0000021553b9db40] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.49    
[in @ 000001577891ed80] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001577891ed80] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 28739.573611
[in @ 000001570c56dd00] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001570c56dd00] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 1106.475311
[in @ 000001570c56d380] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001570c56d380] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 3079.283322
[in @ 000001570ff0d8c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001570ff0d8c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 1106.475311
[in @ 000001570ff0d7c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001570ff0d7c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 3079.283322
[in @ 000001570ff18000] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001570ff18000] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 28739.573611
[aac @ 00000157100cd980] Qavg: 10445.977
[aac @ 00000157100cd980] 2 frames left in the queue on closing
[CLI] realtime video runtime failed: IoFailure: scheduled datagram submit exceeded its original deadline generation=1 sequence=122800 endpoint=1 release_ns=201130891101 reserved_at_ns=201183491900 not_before_ns=201183696140 deadline_ns=201197312080 observed_at_ns=201221337000

direct3d11 error: SetThumbNailClip failed: 0x800706f4
main error: buffer deadlock prevented
main error: Failed to create video converter
main error: read error: No error
main error: write error: No error
main warning: buffer too late (-108668642 us): dropped
main warning: picture is too late to be displayed (missing 108 ms)
main warning: picture is too late to be displayed (missing 108848 ms)
main warning: picture is too late to be displayed (missing 108880 ms)
main warning: picture is too late to be displayed (missing 108913 ms)
main warning: picture is too late to be displayed (missing 108947 ms)
main warning: picture is too late to be displayed (missing 108980 ms)
main warning: picture is too late to be displayed (missing 142 ms)
main warning: picture is too late to be displayed (missing 42 ms)
main warning: picture is too late to be displayed (missing 75 ms)
main warning: playback too late (145866): up-sampling
main warning: playback way too late (328834): flushing buffers
```

## 本轮清理清单（删除前归档）

- composition-r359-cli-error.log (2306 bytes)
- composition-r359-cli.log (795592 bytes)
- composition-r359-monitor.log (14284 bytes)
- composition-r359-native-2026-10-26-16h55m04s653.png (907286 bytes)
- composition-r359-native-2026-10-26-16h56m49s094.png (12152 bytes)
- composition-r359-native-2026-10-26-16h57m37s233.png (330174 bytes)
- composition-r359-native-2026-10-26-16h59m13s161.png (345619 bytes)
- composition-r359-reentry-stdout.log (540 bytes)
- composition-r359-reentry.log (29520 bytes)
- composition-r359-source0-stdout.log (540 bytes)
- composition-r359-source0.log (29624 bytes)
- composition-r359-source1-stdout.log (335 bytes)
- composition-r359-source1.log (29062 bytes)
- composition-r359-source2-stdout.log (335 bytes)
- composition-r359-source2.log (28926 bytes)
- composition-r359-vlc.log (42813 bytes)

清理完成：16项已按清单删除，r359产物残留0；复用测试源与正式Release产物保留。未使用远程环境，无远程产物。
## 后续调度取证准备（未执行新验收）

本机源码核查确认高精度 waitable timer 已开启，201740 ns 向上换算为 -2018 个 100 ns 单位正确；Windows 返回 1 对应第二个等待句柄（timer）。37.8273 ms 的 API 前后区间不能区分 timer 信号迟到与线程就绪后调度迟到，也可能包含采样点之间的抢占。不据此修改期限或添加经验自旋。

本机只读工具查询已确认：`wpr -profiledetails CPU` 与 `CPU.Light` 均含 CSwitch/ReadyThread，但系统 collector 配置为 1608 个 1024 KiB buffer；Light 仅移除相关栈，未减小该缓冲。`xperf -providers K` 提供 PROC_THREAD、CSWITCH、DISPATCHER、DPC、INTERRUPT、TIMER；`xperf -help start` 支持显式 buffer/file 上限和 PerfCounter 时钟。`xperf -help readythread timer` 确认可导出 ReadyThread 列表与指定进程的 timer expiration history。工具查询成功不等于采集成功或问题定位完成。

下一轮同规格 realtime 链路需关联实际 sender TID、日志 steady 时间与 ETW 时间，在失败窗口检查 ready→running、timer expiration 和 DPC/ISR，并核对事件丢失计数；采集上限属于诊断配置，不改变媒体部署契约。启动前检查已有追踪会话，不能停止他人会话；最终记录实际命令与结果后清理本轮 ETL/导出文件。尚未启动采集，尚无 ETW 结论。无需为此配置 HKLM WER 转储；当前是带原始错误的 exit1，不是已证实的无诊断崩溃。

官方方法依据：https://learn.microsoft.com/en-us/windows-hardware/test/wpt/cpu-analysis ；等待返回值：https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitformultipleobjects 。

ETW对时约束：官方EVENT_TRACE_LOGFILE说明ProcessTrace默认把原始QPC转换为系统时间，只有PROCESS_TRACE_MODE_RAW_TIMESTAMP保留原始格式；不能直接将xperf相对时间与steady_ns相减。下一轮必须确认导出起点或原始QPC映射，线程事件须按payload中的目标TID区分事件记录者。依据：https://learn.microsoft.com/en-us/windows/win32/api/evntrace/ns-evntrace-event_trace_logfilew 。本机xperf dumper支持-add_rawdata，但尚未验证其输出是否包含所需时间基准。

纯诊断接线已冻结：MediaNodeWaitObservation追加可选Windows PID/TID/QPC frequency；MediaNodeWakeup仅Windows且启用observation时读取实际调用线程，frequency失败留空，不覆盖原arm/wait错误；既有worst样本按值保存并在汇总输出。root与prepared_wire_registration两名未参与实现者均源码PASS。尚未重新构建、未实流验证，不代表调度问题修复。

追踪会话查询权限边界：普通权限xperf -loggers返回No Active Loggers，不能作为全机无会话证明。较高权限同命令列出系统/DiagTrack等既有会话，未见NT Kernel Logger；wpr -status报告未录制。未停止、修改任何既有会话。下一轮启动前须再次按较高权限核对，并只清理本轮拥有的logger。

后续构建核验：Release clean-first/all-target会话62105 exit0，configure/build均0（754项），realtime6581760 bytes、2026-10-26 17:17:27；CMakeCache/build.ninja及两个CLI产物均存在。仅构建local产物，未运行local。此次源码尚未新实流验证，不覆盖r359 FAIL。
