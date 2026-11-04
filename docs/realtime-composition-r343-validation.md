# r343：修正顶层fmtp后的输出 purge 诊断

状态：首AU超期失败，未进入purge；已记录并清理。仅修正r342顶层fmtp引用，其他同规格参数及CDB一次性只读断点保持；不改生产代码。输入新SSRC触发共享realtime.av输出换代，实际占用仍须断点证据。

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r343-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r343-source0.log' -PassThru
Write-Output "SOURCE_PID=$($source0.Id)"
$debugger=Start-Process -FilePath 'C:/Program Files (x86)/Windows Kits/10/Debuggers/x64/cdb.exe' -ArgumentList ('-G -hd -lines -logo D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r343-cdb.log -y D:/Code/MyCode/MediaTranscode/out/build/x64-release -c "bp /1 `media_transcode_realtime_video_cli!MediaMpegTsDatagramMaterializerNode.cpp:248` \".echo R343_PURGE_BEFORE;.time;dv /t /v;dx -r2 this->m_generationSession->data;.echo R343_PURGE_INSPECT_END;gc\";sxe -c2 \".exr -1;.ecxr;~* kb;.dump /ma D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r343-crash.dmp;qd\" 0xc0000409;g" D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe ' + '--media-id composition-r343 --input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp "profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210" --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62024 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r343-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2') -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r343-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r343-cli-error.log' -PassThru
Write-Output "CDB_PID=$($debugger.Id)"
$child=$null
$discovery=[Diagnostics.Stopwatch]::StartNew()
while(-not $child -and -not $debugger.HasExited -and $discovery.Elapsed.TotalSeconds -lt 15){
$child=Get-CimInstance Win32_Process -Filter "ParentProcessId=$($debugger.Id)" | Where-Object {$_.Name -eq 'media_transcode_realtime_video_cli.exe'}
if(-not $child){Start-Sleep -Milliseconds 100}
}
if(-not $child){throw 'Debugger did not create realtime CLI'}
$cli=[Diagnostics.Process]::GetProcessById($child.ProcessId)
$cliHandle=$cli.Handle
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 1) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r343-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r343-native- --snapshot-format=png rtp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r343-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
$lastTime=$time
$lastCpu=$cpu
}
Write-Output "CLI_EXIT=$($cli.ExitCode)"
$source0.WaitForExit()
Write-Output "SOURCE0_EXIT=$($source0.ExitCode)"
exit $cli.ExitCode
~~~~

## 实际失败

源38916、CDB41860、CLI3848、VLC11580。CLI已正常exit1，首次AU超原期限，媒体/数据报产出均0；无R343_PURGE_BEFORE命中。payload回零且reserve/release572。CPU平均1.030681%、峰3.213611%，WS末及峰191295488；末次private27152715776字节异常增长仍需定位，不能归因为CDB或本轮双槽。

本轮未触发源切换，未形成可验收画面，不获取快照或宣称purge通过。保存以下原始证据后清理仍运行源及VLC，再按八项清单删除产物。下一步去掉调试器，同规格区分生产启动与调试干预，不修改原期限。

~~~~text
[2026-10-26 12:30:52.218] [info] [graph][planner.select] input_startup_retention window_ns=10000000000 video_replay_au_bound=15 video_units=315 video_bytes=2756250000 audio_replay_au_bound=22 audio_units=453 audio_bytes=3710523 future_arrival_rate_guarantee=not_proven
[2026-10-26 12:30:52.265] [info] [graph][runtime.node] register node=13 name=realtime.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-10-26 12:30:52.265] [info] [graph][runtime.node] register node=14 name=realtime.av_sync.startup.clock kind=AvStartupClock
[2026-10-26 12:30:52.265] [info] [graph][runtime.node] register node=15 name=realtime.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-10-26 12:30:52.265] [info] [graph][runtime.node] register node=17 name=realtime.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-10-26 12:30:52.266] [info] [graph][runtime.node] register node=27 name=realtime.audio.encode.startup_trim kind=Unknown
[2026-10-26 12:30:56.503] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-10-26 12:30:56.503] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=8 generation=1
[2026-10-26 12:30:56.944] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 produced_datagrams=0 produced_ts_payload_bytes=0 pending_bytes=0 peak_pending_bytes=0 access_units=0 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=0 maximum_mux_input_after_emit_ns=0 maximum_mux_input_after_dispatch_ns=0 worst_mux_input_ready_ns=0 worst_mux_input_emit_ns=0 worst_mux_input_dispatch_ns=0 maximum_protocol_batch_produced_after_release_ns=0 worst_protocol_batch_produced_ns=0 worst_protocol_batch_release_ns=0 worst_protocol_batch_deadline_ns=0 worst_protocol_batch_datagrams=0 exit_reason=InvalidArgument: MPEG-TS access unit expired before prefix publication prefix_release_ns=4334571500 au_deadline_ns=4401238168 observed_ns=4683752900
[2026-10-26 12:30:56.951] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=0 committed_datagrams=0 committed_payload_bytes=0 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=0 ordered_queue_maximum_datagrams=0 ordered_queue_maximum_wire_bytes=0 pacing_reserved=0 pacing_cancelled=0 pacing_submitted=0 pacing_maximum_submit_lateness_ns=0 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=0 backlog_high_water_wire_bytes=0 backlog_max_residence_ns=0 last_materialized_sequence=0 last_scheduled_sequence=0 last_submitted_sequence=0 last_committed_sequence=0 submitted_service=absent shared_scope_charged_datagrams=0 shared_scope_charged_wire_bytes=0 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=0 evidence_timestamp_tracked=0 evidence_timestamp_untracked=0 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=379902 endpoint_1_target_effective_socket_bytes=189951 endpoint_1_api_requested_socket_bytes=189951 endpoint_1_effective_socket_bytes=189951 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=0 endpoint_1_committed_payload_bytes=0 endpoint_2_target_effective_socket_bytes=189951 endpoint_2_api_requested_socket_bytes=189951 endpoint_2_effective_socket_bytes=189951 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=0 endpoint_2_committed_payload_bytes=0
[CLI] final runtime report: queued=0, peakQueued=189, threads=40, processThreads=24, workers=0, workerProgress=3329, workerProcessCalls=4576, workerWaits=1240, workerWakeups=1208, workerErrors=1, errors=1, stalledIntervals=0, cpuSamples=14, logicalProcessors=22, averageSystemMachineCpuPercent=22.400383, averageProcessMachineCpuPercent=1.030681, peakProcessMachineCpuPercent=3.213611, averageProcessSingleCoreCpuPercent=22.674991, peakProcessSingleCoreCpuPercent=70.699433, initialWorkingSetBytes=60358656, workingSetBytes=191295488, peakWorkingSetBytes=191295488, totalPushed=1062, totalPopped=942, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=64, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=13921057, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=373, graphPayloadReservations=572, graphPayloadReleases=572, graphPayloadPressureFailures=0, lastInputReceivedAtNs=13521857622900
[in @ 0000021d6d60c040] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000021d6d60c040] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 36229.251644
[aac @ 0000021d5fedf980] Qavg: 197.833
[aac @ 0000021d5fedf980] 2 frames left in the queue on closing
[CLI] realtime video runtime failed: InvalidArgument: MPEG-TS access unit expired before prefix publication prefix_release_ns=4334571500 au_deadline_ns=4401238168 observed_ns=4683752900
seconds=1.0541779 cpu=2.29066908656758 ws=160661504 private=491188224
seconds=2.0780547 cpu=0.693664777566278 ws=162316288 private=492384256
seconds=3.1041209 cpu=1.03827697383552 ws=163409920 private=493408256
seconds=4.1313118 cpu=0.829712108306964 ws=165429248 private=452157440
seconds=5.160625 cpu=0.897001471025005 ws=190414848 private=27152715776
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r343-cdb.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r343-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r343-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r343-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r343-output.sdp
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r343-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r343-source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r343-vlc.log

~~~~



独立只读定位：compile12:30:52.238至52.260；clock locked56.502；filter初始化56.521至56.593；first_release56.594；emission_plan56.941，首AU失败56.944。中间有C++ first-chance异常，尚无栈证其原因。private突增与activation后transport/mux初始化重合；payload高水13921057，不能据此归因为媒体payload泄漏或CDB。下一步比较无调试器的同阶段private及固定网络数组容量。

实际清理8文件、四PID无残留；源清理exit-1，父56928exit1。
