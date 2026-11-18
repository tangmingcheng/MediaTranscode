# r246：Release realtime 同AU迟到定位

## 状态

已结束，实流失败，详见结果。三路原120秒H264 1280×720@30源，输出HEVC 1280×720@30 CBR8000kbps/AAC192kbps/MPEG-TS-RTP。逐源input latency100/150/200ms，wire residence100ms、egress50Mbps、preparation2GiB均保持；不执行local，不增加FFmpeg监控。VLC仅原生snapshot。

## 启动与后台监控命令

```powershell
$ErrorActionPreference='Stop'
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r246-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r246-native- --snapshot-format=png rtp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r246-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r246-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r246-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r246-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r246-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r246-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r246 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r246-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r246-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r246-cli-error.log' -PassThru
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
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r246-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

## VLC原生快照命令

```powershell
$client=[Net.Sockets.TcpClient]::new('127.0.0.1',63020);try{$stream=$client.GetStream();$stream.ReadTimeout=2000;$bytes=[Text.Encoding]::ASCII.GetBytes("status`r`nsnapshot`r`n");$stream.Write($bytes,0,$bytes.Length);$buffer=[byte[]]::new(4096);try{while(($n=$stream.Read($buffer,0,4096))-gt 0){[Text.Encoding]::UTF8.GetString($buffer,0,$n)}}catch [IO.IOException]{}}finally{$client.Dispose()}
Get-ChildItem -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance' -Filter 'composition-r246-native-*.png' | Select-Object -ExpandProperty FullName
```

## 运行结果：失败

Release修正后会话52713 exit0，configure/build均0；缓存/Ninja和两CLI存在。realtime6071296字节，2026-10-16 14:39:25，SHA256 D0DF075FB870AEF339AA898FC935D07BB17807D7AB36AC13858D79A957EB5FCB。local未执行。

VLC38752；三路源38032/39056/35560；CLI39576，监控会话32678。CLI约9秒自然exit1；三源均完整120秒3600帧自然exit0；会话最终exit1。未执行重入或caller-stop。VLC14:40:49.074原生snapshot已实际查看三格视频及右下黑；RC play state3、snapshot0。CLI已失败，该图可能是末帧，不证明持续输出。

外部9样本，末9.2764864秒，CPU均值2.6204994691496335%、峰4.46557040307105%；WS首237920256、末175783936、峰241311744字节（末值含退出回收，不能解释为稳态）。漂移10样本，最大绝对raw_phase22675ns；不作为内容A/V同步证明。最终workers/queued0、workerErrors/errors1、payload current bytes/objects0、reservations=releases8531、pressureFailures1。

### 同AU证据与结论

输出identity为composition-r246.output，generation1。video sequence251：canonical_created_steady_ns=21597627464000，head_observed_steady_ns=21597627529500，差65500ns。此差值是构造到观察，不是纯通道耗时；但可排除该AU构造后的长时间滞留是本次146.6ms等待的主要来源。

audio359缺video head等待146561900ns，在video251到达后释放；mux接收时晚于dispatch27828668ns。该audio的scheduled_created至session_ready差508000ns，说明本次主要等待也不在这段下游路径。audio360构造至head观察124025900ns，符合它在前一个head等待期间已准备好并排队，但不替代audio359自身证据。

同轮最大canvas为cursor251，即下一output sequence252，wall147006600ns；source_wait总139285000ns、单次最大85942600ns。其时间段与video251生成重叠，仍不能把不同frame拼成同帧因果。下一直接定位点为video251在encoder输入接受、codec receive输出及canonical构造前的实际时刻；检查NVENC与共享CUDA stream的依赖，不能直接删除同步或放宽100ms。

### 必要原始诊断

```text
[2026-10-16 14:40:02.554] [info] [graph][runtime.node] av_scheduler_trace stage=late_video_head generation=1 mapping_epoch_generation=1 canonical_target_ns=8333333333 canonical_dispatch_ns=8333333333 target_master_ns=9011525301 dispatch_master_ns=9011525301 now_ns=9017792700 node=74 identity_kind=output identity="composition-r246.output" canonical_sequence=251 canonical_created_steady_ns=21597627464000 head_observed_steady_ns=21597627529500
[2026-10-16 14:40:02.555] [info] [graph][runtime.node] av_scheduler_trace stage=late_output_commit stream=audio generation=1 canonical_dispatch_ns=8312743764 emit_ns=8890935732 dispatch_ns=8990935732 committed_at_ns=9018277900 canonical_sequence=359 head_missing_valid=1 head_missing_intervals=1 head_missing_completed_ns=146561900 head_missing_active=0 head_missing_last_begin_ns=8871682500 head_missing_last_end_ns=9018244400 head_resumed_generation=1 head_resumed_sequence=251 head_arrived_ns=8871657000 scheduled_at_ns=9018252900
[2026-10-16 14:40:02.555] [info] [graph][runtime.node] av_scheduler_trace stage=late_audio_head generation=1 mapping_epoch_generation=1 canonical_target_ns=8335963718 canonical_dispatch_ns=8335963718 target_master_ns=9014155686 dispatch_master_ns=9014155686 now_ns=9018755900 node=74 identity_kind=output identity="composition-r246.output" canonical_sequence=360 canonical_created_steady_ns=21597504466600 head_observed_steady_ns=21597628492500
[2026-10-16 14:40:02.555] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 datagrams=0 wire_bytes=0 immediate_deadlines=0 deferred_deadlines=0 planned_wait_ns=0 late_datagrams=0 maximum_lateness_ns=0 pending_bytes=564 peak_pending_bytes=92872 pressure_failures=0 access_units=608 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=166 maximum_mux_input_after_emit_ns=127828668 maximum_mux_input_after_dispatch_ns=27828668 worst_mux_input_ready_ns=9018764400 worst_mux_input_emit_ns=8890935732 worst_mux_input_dispatch_ns=8990935732 maximum_protocol_batch_produced_after_release_ns=8333032 worst_protocol_batch_produced_ns=586525000 worst_protocol_batch_release_ns=578191968 worst_protocol_batch_deadline_ns=678191968 worst_protocol_batch_datagrams=1 exit_reason=InvalidArgument: MPEG-TS access unit has no transport service window stream=audio emit_ns=8890935732 dispatch_ns=8990935732 available_ns=9011525300 last_committed_kind=maintenance service_start_ns=9011525300 actual_now_ns=9018775400 remaining_ns=-20589568 payload_bytes=564 wire_bytes=576
[2026-10-16 14:40:02.556] [info] [graph][runtime.node] av_scheduler_trace stage=late_output_commit stream=video generation=1 canonical_dispatch_ns=8333333333 emit_ns=8911525301 dispatch_ns=9011525301 committed_at_ns=9019224900 canonical_sequence=251 head_missing_valid=1 head_missing_intervals=0 head_missing_completed_ns=0 head_missing_active=0 head_missing_last_begin_ns=unavailable head_missing_last_end_ns=unavailable head_resumed_generation=unavailable head_resumed_sequence=unavailable head_arrived_ns=9017790800 scheduled_at_ns=9019217200
[2026-10-16 14:40:02.556] [info] [graph][runtime.node] au_processing_trace stage=late_mux_input generation=1 stream=audio canonical_sequence=359 dispatch_master_ns=8990935732 ready_master_ns=9018764400 scheduled_created_steady_ns=21597627993000 adapter_dequeued_steady_ns=21597628039300 adapted_created_steady_ns=21597628040000 write_entered_steady_ns=21597628497700 session_ready_steady_ns=21597628501000
[2026-10-16 14:40:02.566] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=911 committed_datagrams=6072 committed_payload_bytes=7365928 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=80004 pacing_reserved=6072 pacing_cancelled=0 pacing_submitted=6072 pacing_maximum_submit_lateness_ns=7830780 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=80004 backlog_max_residence_ns=42657000 last_materialized_sequence=6072 last_scheduled_sequence=6072 last_submitted_sequence=6072 last_committed_sequence=6072 shared_scope_charged_datagrams=6072 shared_scope_charged_wire_bytes=7535944 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2155 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2155 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=379902 endpoint_1_target_effective_socket_bytes=189951 endpoint_1_api_requested_socket_bytes=189951 endpoint_1_effective_socket_bytes=189951 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=6069 endpoint_1_committed_payload_bytes=7365724 endpoint_2_target_effective_socket_bytes=189951 endpoint_2_api_requested_socket_bytes=189951 endpoint_2_effective_socket_bytes=189951 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=3 endpoint_2_committed_payload_bytes=204
[2026-10-16 14:40:02.616] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=canvas count=252 maximum_ns=147007800 begin_steady_ns=21597533936500 end_steady_ns=21597680944300 video_cursor=251 audio_cursor=not_owned next_video_cursor=251 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-16 14:40:02.617] [info] [graph][runtime.node] cuda_canvas_call stage=source_wait count=760 operation=copy maximum_ns=85944200 begin_steady_ns=21597536832400 end_steady_ns=21597622776600 native_result=0 thread_cpu_ns=0
[2026-10-16 14:40:02.618] [info] [graph][runtime.node] canvas_compose_correlated scope=attempt begin_steady_ns=21597533937100 end_steady_ns=21597680943700 wall_ns=147006600 thread_cpu_ns=0 surface_count=1 surface_total_ns=1000 surface_maximum_ns=1000 surface_maximum_tile=none surface_maximum_result=unavailable validate_count=3 validate_total_ns=2300 validate_maximum_ns=1300 validate_maximum_tile=0 validate_maximum_result=0 black_count=1 black_total_ns=2886900 black_maximum_ns=2886900 black_maximum_tile=none black_maximum_result=0 tile_count=3 tile_total_ns=144100300 tile_maximum_ns=87873800 tile_maximum_tile=0 tile_maximum_result=0 publish_count=1 publish_total_ns=11800 publish_maximum_ns=11800 publish_maximum_tile=none publish_maximum_result=0 context_push_count=4 context_push_total_ns=2600 context_push_maximum_ns=1000 context_push_maximum_tile=2 context_push_maximum_result=0 source_wait_count=3 source_wait_total_ns=139285000 source_wait_maximum_ns=85942600 source_wait_maximum_tile=0 source_wait_maximum_result=0 plane_copy_count=8 plane_copy_total_ns=635500 plane_copy_maximum_ns=194400 plane_copy_maximum_tile=0 plane_copy_maximum_result=0 destination_wait_count=4 destination_wait_total_ns=6996800 destination_wait_maximum_ns=2615000 destination_wait_maximum_tile=none destination_wait_maximum_result=0 context_pop_count=4 context_pop_total_ns=15300 context_pop_maximum_ns=5600 context_pop_maximum_tile=1 context_pop_maximum_result=0
[CLI] final runtime report: queued=0, peakQueued=388, threads=82, processThreads=132, workers=0, workerProgress=22557, workerProcessCalls=97059, workerWaits=74493, workerWakeups=73772, workerErrors=1, errors=1, stalledIntervals=0, cpuSamples=26, logicalProcessors=22, averageSystemMachineCpuPercent=22.619608, averageProcessMachineCpuPercent=2.635186, peakProcessMachineCpuPercent=7.424594, averageProcessSingleCoreCpuPercent=57.974090, peakProcessSingleCoreCpuPercent=163.341067, initialWorkingSetBytes=189775872, workingSetBytes=243847168, peakWorkingSetBytes=243847168, totalPushed=20817, totalPopped=20707, droppedBuffers=0, encodedPacketsPushed=2809, encodedPacketsPopped=2804, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=4849087, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=169, graphPayloadReservations=8531, graphPayloadReleases=8531, graphPayloadPressureFailures=1, lastInputReceivedAtNs=21597619181500
[in @ 000001eae1e01e80] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 31298.964789
[aac @ 000001eae1a7dd00] Qavg: 1620.800
[aac @ 000001eae1a7dd00] 2 frames left in the queue on closing
[CLI] realtime video runtime failed: InvalidArgument: MPEG-TS access unit has no transport service window stream=audio emit_ns=8890935732 dispatch_ns=8990935732 available_ns=9011525300 last_committed_kind=maintenance service_start_ns=9011525300 actual_now_ns=9018775400 remaining_ns=-20589568 payload_bytes=564 wire_bytes=576
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.49    
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.47    
```

## 精确清理命令

```powershell
$proc=Get-CimInstance Win32_Process -Filter 'ProcessId=38752'
if($proc){if($proc.Name -ne 'vlc.exe' -or $proc.CommandLine -notlike '*composition-r246*'){throw 'VLC identity mismatch'};Stop-Process -Id 38752 -Force}
$root=[IO.Path]::GetFullPath('D:/Code/MyCode/MediaTranscode/out/acceptance')+[IO.Path]::DirectorySeparatorChar
$names=@('composition-r246-cli-error.log','composition-r246-cli.log','composition-r246-monitor.log','composition-r246-native-2026-10-16-14h40m49s074.png','composition-r246-output.sdp','composition-r246-source0-stdout.log','composition-r246-source0.log','composition-r246-source1-stdout.log','composition-r246-source1.log','composition-r246-source2-stdout.log','composition-r246-source2.log','composition-r246-vlc.log')
foreach($name in $names){$path=[IO.Path]::GetFullPath((Join-Path $root $name));if(-not $path.StartsWith($root,[StringComparison]::OrdinalIgnoreCase)){throw 'Cleanup escaped acceptance directory'};if(Test-Path -LiteralPath $path){Remove-Item -LiteralPath $path -Force;Write-Output "REMOVED $name"}}
Get-ChildItem -LiteralPath $root -Filter 'composition-r246-*' | Select-Object Name
Get-CimInstance Win32_Process | Where-Object {$_.ProcessId -in @(38752,38032,39056,35560,39576)} | Select-Object ProcessId,Name
Get-Item -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4' | Select-Object Length
```

清理核验：12项精确清单均删除，本轮前缀无残留，五个PID均无残留；指定复用源124427809字节保留。VLC按PID强制清理，不记自然退出。未执行远程测试。

后续源码核验边界补充：EncodedVideoOutputFanoutNode会重新包装canonical AU。上述65.5us仅是被scheduler读取的wrapper构造后区间，不能自动视为encoder出包至scheduler；r247将成功receive区间跨fanout保留，以进一步区分。
