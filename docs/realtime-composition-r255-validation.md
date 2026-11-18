# r255：CUDA 消费输入所有权修复后 Release realtime 复验

状态：待执行。命令已沿 r251 原规格核对，尚无本轮 PID、画面或运行结果；不得以本文存在判定已运行。

三路 RTP H.264 720p30，所选 AAC 44.1kHz 双声道；输出 HEVC 1280×720/30 CBR8000kbps、AAC CBR192kbps、MPEG-TS/RTP。每路使用指定连续120秒源，不循环。逐源输入等待100/150/200ms，准备存储2147483648字节，出口50Mbps、wire100ms。先观察三格、全部失源黑屏，再让选定AV源完整120秒重入。VLC原生快照；记录CPU、内存、漂移、退出原因和资源账本。

构建产物：realtime 6118400字节，SHA256 3F0F2CFF6B47379D3BA375BB2848DB7636FBE74581A4F40495D3EF1A86F48CB2。完整目标还包括四源、HEVC输入重排、非固定节奏、C API/ABI及Windows后RKMPP，本轮不替代这些门禁。

## 启动命令（待执行）
```powershell
$ErrorActionPreference='Stop'
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r255-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r255-native- --snapshot-format=png rtp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r255-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r255-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r255-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r255-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r255-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r255-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r255 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r255-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r255-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r255-cli-error.log' -PassThru
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
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r255-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

## VLC原生快照命令（待执行）
```powershell
$client=[Net.Sockets.TcpClient]::new('127.0.0.1',63020);try{$stream=$client.GetStream();$stream.ReadTimeout=2000;$bytes=[Text.Encoding]::ASCII.GetBytes("status`r`nsnapshot`r`n");$stream.Write($bytes,0,$bytes.Length);$buffer=[byte[]]::new(4096);try{while(($n=$stream.Read($buffer,0,4096))-gt 0){[Text.Encoding]::UTF8.GetString($buffer,0,$n)}}catch [IO.IOException]{}}finally{$client.Dispose()}
Get-ChildItem -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance' -Filter 'composition-r255-native-*.png' | Select-Object -ExpandProperty FullName
```

## 重入命令（待执行）
```powershell
$source=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r255-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r255-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($source.Id)"
$source.WaitForExit()
Write-Output "REENTRY_EXIT=$($source.ExitCode)"
exit $source.ExitCode
```

## 结果与清理

待运行后填写实际PID、命令结果、快照观察和关键指标；先记录，再按本轮精确文件/PID清单清理。保留指定源。


本轮目的：复用现有 State 诊断，完整归档迟到AU、head等待、编码输出及canvas关联行。r254遗漏关联证据，不对其补称已验证。使用相同已双审和构建二进制，不修改媒体规格。

## 实际结果：FAIL（启动期）

监控会话54600；VLC12016，源29624/27872/2904，CLI8304自然exit1。实际按上文启动命令执行。协议数据报release1349821884、materialized1417974300、deadline1416488552ns，超过deadline1485748ns。此轮未复现r254音频AU窗口失败；未完成120秒、视觉、失源或重入，源流在测试判定失败后按精确PID中止。最终payload1751预约=1751释放且0对象/0字节。

关联诊断（现有State输出匹配行全部归档）：
```text
[2026-10-19 09:13:33.512] [info] [graph][runtime.node] audio_encode_trace stage=codec_bound
[2026-10-19 09:13:33.599] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1 source_master_ns=672207422 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-10-19 09:13:33.599] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2 source_master_ns=695427377 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=45346
[2026-10-19 09:13:33.599] [info] [graph][runtime.node] audio_encode_trace stage=first_frame
[2026-10-19 09:13:33.600] [info] [graph][runtime.node] mpegts_emission_plan generation=1 mode=canonical_access_unit_window access_unit_window_ns=100000000 video_initial_service_window_ns=13995843 audio_initial_service_window_ns=13995843 packet_size_bytes=188 maximum_payload_bytes=1316 per_datagram_overhead_bytes=12
[2026-10-19 09:13:33.611] [info] [graph][runtime.node] av_scheduler_trace stage=first_video_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=649821868 dispatch_master_ns=649821868 now_ns=494445200 node=74 identity_kind=output identity="composition-r255.output" canonical_sequence=1 canonical_created_steady_ns=1827527945400 head_observed_steady_ns=1827527978600 codec_receive_begin_steady_ns=1827527624000 codec_receive_end_steady_ns=1827527625200
[2026-10-19 09:13:33.611] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=494549200 present_generation=1 present_canonical_target_ns=0 present_canonical_dispatch_ns=0 present_target_master_ns=649821868 present_dispatch_master_ns=649821868
[2026-10-19 09:13:33.646] [info] [graph][runtime.node] audio_encode_trace stage=first_packet
[2026-10-19 09:13:33.646] [info] [graph][runtime.node] av_scheduler_trace stage=first_audio_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=649821868 dispatch_master_ns=649821868 now_ns=530388500 node=74 identity_kind=output identity="composition-r255.output" canonical_sequence=1 canonical_created_steady_ns=1827563889800 head_observed_steady_ns=1827563921900 codec_receive_begin_steady_ns=unavailable codec_receive_end_steady_ns=unavailable
[2026-10-19 09:13:34.203] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=47 source_master_ns=1740325336 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=90692
[2026-10-19 09:13:34.535] [info] [graph][runtime.node] process.failed node=80 name=MediaMpegTsDatagramMaterializerNode kind=MpegTsDatagramMaterializer inputs=[channel=118 edge=118 stream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=120 edge=120 stream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=121 edge=121 stream=Metadata edge_kind=Unknown payload=Unknown queued=9] outputs=[channel=122 edge=122 stream=Metadata edge_kind=Unknown payload=Unknown queued=0] error=InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=1349821884 materialized_ns=1417974300 protocol_deadline_ns=1416488552 deadline_before_release=0 materialized_after_deadline=1
[2026-10-19 09:13:34.535] [info] [graph][runtime.node] worker.failed node=80 kind=MpegTsDatagramMaterializer name=MPEG-TS wire protocol materializer error=InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=1349821884 materialized_ns=1417974300 protocol_deadline_ns=1416488552 deadline_before_release=0 materialized_after_deadline=1
[2026-10-19 09:13:34.536] [info] [graph][runtime.node] process.failed node=82 name=MediaScheduledDatagramSenderNode kind=ScheduledDatagramSender inputs=[channel=111 edge=111 stream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=122 edge=122 stream=Metadata edge_kind=Unknown payload=Unknown queued=0] outputs=[] error=Cancelled: scheduled datagram sender reservation wait was interrupted
[2026-10-19 09:13:34.543] [info] [graph][runtime.node] scheduled_datagram_sender stage=failed generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=93 committed_datagrams=609 committed_payload_bytes=739004 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=80004 pacing_reserved=610 pacing_cancelled=0 pacing_submitted=609 pacing_maximum_submit_lateness_ns=9187080 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=56 backlog_current_wire_bytes=75748 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=80004 backlog_max_residence_ns=40264300 last_materialized_sequence=665 last_scheduled_sequence=665 last_submitted_sequence=609 last_committed_sequence=609 shared_scope_charged_datagrams=609 shared_scope_charged_wire_bytes=756056 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=609 evidence_timestamp_tracked=0 evidence_timestamp_untracked=609 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=379902 endpoint_1_target_effective_socket_bytes=189951 endpoint_1_api_requested_socket_bytes=189951 endpoint_1_effective_socket_bytes=189951 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=609 endpoint_1_committed_payload_bytes=739004 endpoint_2_target_effective_socket_bytes=189951 endpoint_2_api_requested_socket_bytes=189951 endpoint_2_effective_socket_bytes=189951 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=0 endpoint_2_committed_payload_bytes=0
[2026-10-19 09:13:34.694] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 datagrams=0 wire_bytes=0 immediate_deadlines=0 deferred_deadlines=0 planned_wait_ns=0 late_datagrams=0 maximum_lateness_ns=0 pending_bytes=0 peak_pending_bytes=231428 pressure_failures=0 access_units=66 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=10 maximum_mux_input_after_emit_ns=9659532 maximum_mux_input_after_dispatch_ns=-90340468 worst_mux_input_ready_ns=559481400 worst_mux_input_emit_ns=549821868 worst_mux_input_dispatch_ns=649821868 maximum_protocol_batch_produced_after_release_ns=9693932 worst_protocol_batch_produced_ns=559515800 worst_protocol_batch_release_ns=549821868 worst_protocol_batch_deadline_ns=649821868 worst_protocol_batch_datagrams=1 exit_reason=aborted
[2026-10-19 09:13:34.696] [info] [graph][runtime.node] continuous_audio_aggregate.summary reason=abort output_group=composition-r255.output audio_commits=41 real_samples=41984 silence_samples=0
[2026-10-19 09:13:34.696] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=process count=247 maximum_ns=3387100 begin_steady_ns=1827516872900 end_steady_ns=1827520260000 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=progress deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-10-19 09:13:34.697] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=purge count=247 maximum_ns=1800 begin_steady_ns=1827750167000 end_steady_ns=1827750168800 video_cursor=7 audio_cursor=not_owned next_video_cursor=7 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 09:13:34.697] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=bind count=247 maximum_ns=8100 begin_steady_ns=1827428516800 end_steady_ns=1827428524900 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 09:13:34.697] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=consume count=247 maximum_ns=221100 begin_steady_ns=1827983889900 end_steady_ns=1827984111000 video_cursor=15 audio_cursor=not_owned next_video_cursor=15 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 09:13:34.697] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=activate count=16 maximum_ns=183600 begin_steady_ns=1827516688000 end_steady_ns=1827516871600 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 09:13:34.697] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=build_video count=29 maximum_ns=1108700 begin_steady_ns=1827516890300 end_steady_ns=1827517999000 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 09:13:34.697] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=canvas count=29 maximum_ns=1078500 begin_steady_ns=1827516908400 end_steady_ns=1827517986900 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 09:13:34.697] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=emit count=29 maximum_ns=2259900 begin_steady_ns=1827517999500 end_steady_ns=1827520259400 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 09:13:34.697] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=between_calls_previous_return count=246 maximum_ns=86510500 begin_steady_ns=1827428536700 end_steady_ns=1827515047200 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=waiting deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-10-19 09:13:34.697] [info] [graph][runtime.node] continuous_aggregate.summary reason=abort output_group=composition-r255.output output_generation=1 video_commits=29 source[0].group=composition-r255.source.0 source[0].real_frames=26 source[0].black_frames=3 source[0].contribution_generation=1 source[1].group=composition-r255.source.1 source[1].real_frames=26 source[1].black_frames=3 source[1].contribution_generation=1 source[2].group=composition-r255.source.2 source[2].real_frames=26 source[2].black_frames=3 source[2].contribution_generation=1
[2026-10-19 09:13:34.698] [info] [graph][runtime.node] canvas_compose_correlated scope=attempt begin_steady_ns=1827516909100 end_steady_ns=1827517986100 wall_ns=1077000 thread_cpu_ns=0 surface_count=1 surface_total_ns=900 surface_maximum_ns=900 surface_maximum_tile=none surface_maximum_result=unavailable validate_count=3 validate_total_ns=1500 validate_maximum_ns=1000 validate_maximum_tile=0 validate_maximum_result=0 black_count=1 black_total_ns=524700 black_maximum_ns=524700 black_maximum_tile=none black_maximum_result=0 tile_count=3 tile_total_ns=239800 tile_maximum_ns=150400 tile_maximum_tile=0 tile_maximum_result=0 publish_count=1 publish_total_ns=4100 publish_maximum_ns=4100 publish_maximum_tile=none publish_maximum_result=0 context_push_count=1 context_push_total_ns=2800 context_push_maximum_ns=2800 context_push_maximum_tile=none context_push_maximum_result=0 source_wait_count=4 source_wait_total_ns=16400 source_wait_maximum_ns=9000 source_wait_maximum_tile=1 source_wait_maximum_result=0 plane_copy_count=8 plane_copy_total_ns=734800 plane_copy_maximum_ns=332200 plane_copy_maximum_tile=none plane_copy_maximum_result=0 destination_wait_count=0 destination_wait_total_ns=0 destination_wait_maximum_ns=0 destination_wait_maximum_tile=none destination_wait_maximum_result=unavailable context_pop_count=1 context_pop_total_ns=6200 context_pop_maximum_ns=6200 context_pop_maximum_tile=none context_pop_maximum_result=0 batch_completion_count=1 batch_completion_total_ns=285900 batch_completion_maximum_ns=285900 batch_completion_maximum_tile=none batch_completion_maximum_result=0
[CLI] final runtime report: queued=0, peakQueued=388, threads=82, processThreads=132, workers=0, workerProgress=3684, workerProcessCalls=14859, workerWaits=11167, workerWakeups=11023, workerErrors=1, errors=1, stalledIntervals=0, cpuSamples=3, logicalProcessors=22, averageSystemMachineCpuPercent=19.181864, averageProcessMachineCpuPercent=2.664699, peakProcessMachineCpuPercent=4.347826, averageProcessSingleCoreCpuPercent=58.623387, peakProcessSingleCoreCpuPercent=95.652174, initialWorkingSetBytes=190087168, workingSetBytes=236457984, peakWorkingSetBytes=236457984, totalPushed=3439, totalPopped=3325, droppedBuffers=0, encodedPacketsPushed=305, encodedPacketsPopped=304, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=4150393, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=210, graphPayloadReservations=1751, graphPayloadReleases=1751, graphPayloadPressureFailures=0, lastInputReceivedAtNs=1828440920500
```

原始stderr尾部：
```text
[in @ 000001b17e5c6b80] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 8392.475733
[aac @ 000001b12101d380] Qavg: 15712.221
[aac @ 000001b12101d380] 2 frames left in the queue on closing
[CLI] realtime video runtime failed: InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=1349821884 materialized_ns=1417974300 protocol_deadline_ns=1416488552 deadline_before_release=0 materialized_after_deadline=1
```
源进程为失败后清理，退出码均-1，不计自然结束。清理清单：
composition-r255-cli-error.log
composition-r255-cli.log
composition-r255-monitor.log
composition-r255-output.sdp
composition-r255-source0-stdout.log
composition-r255-source0.log
composition-r255-source1-stdout.log
composition-r255-source1.log
composition-r255-source2-stdout.log
composition-r255-source2.log
composition-r255-vlc.log

清理复核：11件产物逐项删除，前缀及五PID无残留；指定源保留。

## 诊断完整性更正

本轮筛选保留了AU/scheduler/canvas及失败汇总，但遗漏实际前缀 protocol_batch_trace 与 mpegts_wire_materializer；因此本文的关联诊断不是全部运行时证据。原日志已清理，不能恢复或宣称批次等待原因已确定。MediaProtocolBatchMaterializationObservation 现有代码已记录mux出队、首次出队、前次尝试、WouldBlock及最后wire发布，无需先增加生产诊断。后续必须保留所有 graph runtime.node/runtime.lifecycle 诊断及原始错误，仅压缩周期重复CLI runtime report，并在清理前检查具体失败类型需要的记录是否存在。

静态调用链：materializer取protocol batch后立即读取当前master时间并尝试物化，无按release预先睡眠；绝对deadline校验位于MediaDatagramShapingPlan。尚需用batch诊断区分上游排队、worker未获调度、背压重试及单次工作过长，不能将1.49ms超期直接称为定时器精度问题。
