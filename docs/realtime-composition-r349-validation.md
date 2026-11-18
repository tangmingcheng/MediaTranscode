# r349：逐prefix提交与恢复分段观测

状态：固定汇总双独立源码PASS、Release全量成功，实流FAIL；归档后清理。使用r348相同120秒源、完整H264/AAC到HEVC CBR 1280x720/30fps/8000kbps与AAC CBR192kbps、TS/RTP链路及原期限，仅增加当前和最近完成AU的固定诊断摘要。不修改SR或调度策略。

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r349-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r349-source0.log' -PassThru
Write-Output "SOURCE_PID=$($source0.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r349 --input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62024 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r349-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r349-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r349-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r349-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r349-native- --snapshot-format=png rtp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r349-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
$lastTime=$time
$lastCpu=$cpu
}
Write-Output "CLI_EXIT=$($cli.ExitCode)"
$source0.WaitForExit()
Write-Output "SOURCE0_EXIT=$($source0.ExitCode)"
exit $cli.ExitCode
~~~~

## 实测结果

Release全量747项configure/build exit0，realtime6508032字节/2026-10-26 13:17:00。源38712、CLI31104；CLI自然exit1，无VLC/快照。此次下游sender sequence596原期限4998186775、observed4998228100，超41325ns；mux直接abort且pending_bytes0，未走poison，故无ts_prefix_commit_summary输出。诊断未覆盖下游失败清理，不能声称取得提交/恢复分段证据，需补abort的一次性摘要。累计产生596数据报，payload回零/2444 reserve=release。
~~~~text
[2026-10-26 13:17:29.460] [info] [graph][runtime.node] output_preparation stage=transport_activate generation=1 elapsed_ns=215400 planned_bytes=180770357454 planned_items=2 event=scope_exit
[2026-10-26 13:17:29.461] [info] [graph][runtime.node] output_preparation stage=global_sequence generation=1 elapsed_ns=33300 planned_bytes=69000 planned_items=2155 event=scope_exit
[2026-10-26 13:17:29.461] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1 source_master_ns=4533186048 raw_phase_ns=20409 filtered_phase_ns=20409 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-10-26 13:17:29.461] [info] [graph][runtime.node] output_preparation stage=transport_buffer generation=1 elapsed_ns=893500 planned_bytes=180770357454 planned_items=2 event=scope_exit
[2026-10-26 13:17:29.462] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3 source_master_ns=4579625957 raw_phase_ns=20409 filtered_phase_ns=20409 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=45346
[2026-10-26 13:17:29.463] [info] [graph][runtime.node] output_preparation stage=ts_packetizer generation=1 elapsed_ns=209600 planned_bytes=180768073796 planned_items=61258 event=scope_exit
[2026-10-26 13:17:29.464] [info] [graph][runtime.node] output_preparation stage=ts_mux generation=1 elapsed_ns=630100 planned_bytes=945416 planned_items=59086 event=scope_exit
[2026-10-26 13:17:29.509] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=48 source_master_ns=5624523917 raw_phase_ns=20410 filtered_phase_ns=20409 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=90692
[2026-10-26 13:17:29.557] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=92 source_master_ns=6646201921 raw_phase_ns=20409 filtered_phase_ns=20409 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=136038
[2026-10-26 13:17:29.618] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=136 source_master_ns=7667879926 raw_phase_ns=20410 filtered_phase_ns=20409 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=181384
[2026-10-26 13:17:29.685] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=181 source_master_ns=8712777884 raw_phase_ns=20408 filtered_phase_ns=20409 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=226730
[2026-10-26 13:17:30.069] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=225 source_master_ns=9734455888 raw_phase_ns=20408 filtered_phase_ns=20409 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=272076
[2026-10-26 13:17:30.094] [info] [graph][runtime.node] process.failed node=40 name=MediaScheduledDatagramSenderNode kind=ScheduledDatagramSender inputs=[channel=51 edge=51 stream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=63 edge=63 stream=Metadata edge_kind=Unknown payload=Unknown queued=0] outputs=[] error=IoFailure: scheduled datagram submit exceeded its original deadline generation=1 sequence=596 endpoint=1 release_ns=4965070412 reserved_at_ns=4997701800 not_before_ns=4997794560 deadline_ns=4998186775 observed_at_ns=4998228100
[2026-10-26 13:17:30.094] [info] [graph][runtime.node] worker.failed node=40 kind=ScheduledDatagramSender name=Common GCRA-paced nonblocking datagram sender error=IoFailure: scheduled datagram submit exceeded its original deadline generation=1 sequence=596 endpoint=1 release_ns=4965070412 reserved_at_ns=4997701800 not_before_ns=4997794560 deadline_ns=4998186775 observed_at_ns=4998228100
[2026-10-26 13:17:30.094] [info] [graph][runtime.node] process.failed node=1 name=RawRtpInputNode kind=RawRtpInput inputs=[] outputs=[channel=1 edge=1 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=2 edge=2 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=9 edge=9 stream=Video edge_kind=InputPacket payload=Packet queued=0; channel=26 edge=26 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0] error=Cancelled: Windows RTP ingress batch receive was cancelled
[2026-10-26 13:17:30.094] [info] [graph][runtime.node] process.failed node=2 name=RawRtpInputNode kind=RawRtpInput inputs=[] outputs=[channel=3 edge=3 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=4 edge=4 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=10 edge=10 stream=Audio edge_kind=InputPacket payload=Packet queued=0; channel=36 edge=36 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0] error=Cancelled: Windows RTP ingress batch receive was cancelled
[2026-10-26 13:17:30.103] [info] [graph][runtime.node] datagram_batch_trace stage=last_completed generation=1 first_sequence=595 last_sequence=595 maximum_submit_interval_ns=51500 maximum_submit_interval_sequence=595 maximum_start_lateness_ns=480740 maximum_start_lateness_sequence=595 first_release_master_ns=4965070412 first_deadline_master_ns=4998403735 last_release_master_ns=4965070412 last_deadline_master_ns=4998403735 first_submitted_master_ns=4997577600 last_submitted_master_ns=4997577600 worst_wait_started_master_ns=4996987200 worst_wait_completed_master_ns=4997525300 worst_submit_observed_master_ns=4997526100
[2026-10-26 13:17:30.104] [info] [graph][runtime.node] datagram_batch_trace stage=active generation=1 first_sequence=596 last_sequence=596 maximum_submit_interval_ns=0 maximum_submit_interval_sequence=0 maximum_start_lateness_ns=0 maximum_start_lateness_sequence=0 first_release_master_ns=4965070412 first_deadline_master_ns=4998403735 last_release_master_ns=4965070412 last_deadline_master_ns=4998403735 first_submitted_master_ns=unavailable last_submitted_master_ns=unavailable worst_wait_started_master_ns=unavailable worst_wait_completed_master_ns=unavailable worst_submit_observed_master_ns=unavailable
[2026-10-26 13:17:30.259] [info] [graph][runtime.node] mpegts_wire_materializer stage=aborted maximum_materialized_after_release_ns=66566456 worst_materialized_at_ns=4964970200 worst_release_ns=4898403744 worst_deadline_ns=4998403735 worst_global_sequence=543
[2026-10-26 13:17:30.259] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 produced_datagrams=596 produced_ts_payload_bytes=726808 pending_bytes=0 peak_pending_bytes=104528 access_units=50 current_scheduling_debt_ns=66666677 maximum_scheduling_debt_ns=66666677 maximum_mux_input_after_emit_ns=88448265 maximum_mux_input_after_dispatch_ns=-11551735 worst_mux_input_ready_ns=4986852000 worst_mux_input_emit_ns=4898403735 worst_mux_input_dispatch_ns=4998403735 maximum_protocol_batch_produced_after_release_ns=66539756 worst_protocol_batch_produced_ns=4964943500 worst_protocol_batch_release_ns=4898403744 worst_protocol_batch_deadline_ns=4998403735 worst_protocol_batch_datagrams=1 exit_reason=aborted
[CLI] final runtime report: queued=0, peakQueued=673, threads=40, processThreads=60, workers=0, workerProgress=10269, workerProcessCalls=21098, workerWaits=10824, workerWakeups=10754, workerErrors=2, errors=2, stalledIntervals=0, cpuSamples=15, logicalProcessors=22, averageSystemMachineCpuPercent=19.892474, averageProcessMachineCpuPercent=1.668980, peakProcessMachineCpuPercent=6.168224, averageProcessSingleCoreCpuPercent=36.717554, peakProcessSingleCoreCpuPercent=135.700935, initialWorkingSetBytes=61493248, workingSetBytes=196980736, peakWorkingSetBytes=196980736, totalPushed=6019, totalPopped=5629, droppedBuffers=0, encodedPacketsPushed=540, encodedPacketsPopped=300, backpressureItems=64, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=15956073, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=405, graphPayloadReservations=2444, graphPayloadReleases=2444, graphPayloadPressureFailures=0, lastInputReceivedAtNs=16315001117900
[in @ 00000201a8c39d80] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000201a8c39d80] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 46832.398344
[aac @ 000002019fb04c40] Qavg: 383.296
[aac @ 000002019fb04c40] 2 frames left in the queue on closing
[CLI] realtime video runtime failed: IoFailure: scheduled datagram submit exceeded its original deadline generation=1 sequence=596 endpoint=1 release_ns=4965070412 reserved_at_ns=4997701800 not_before_ns=4997794560 deadline_ns=4998186775 observed_at_ns=4998228100
seconds=1.0085145 cpu=1.61973152321829 ws=160821248 private=492965888
seconds=2.0267117 cpu=0.697534105109769 ws=162091008 private=493858816
seconds=3.0539046 cpu=0.760567951744993 ws=163672064 private=495165440
seconds=4.091491 cpu=1.57434862992877 ws=164798464 private=450301952
seconds=5.1287547 cpu=1.78025212787347 ws=195715072 private=686702592
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r349-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r349-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r349-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r349-output.sdp
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r349-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r349-source0.log
~~~~

清理完成：六产物按清单删除；源38712清理exit-1，CLI31104自然失败exit1，两个PID无残留；监控会话66235 exit1，无VLC/快照。
