# r254：CUDA 消费输入所有权修复后 Release realtime 复验

状态：待执行。命令已沿 r251 原规格核对，尚无本轮 PID、画面或运行结果；不得以本文存在判定已运行。

三路 RTP H.264 720p30，所选 AAC 44.1kHz 双声道；输出 HEVC 1280×720/30 CBR8000kbps、AAC CBR192kbps、MPEG-TS/RTP。每路使用指定连续120秒源，不循环。逐源输入等待100/150/200ms，准备存储2147483648字节，出口50Mbps、wire100ms。先观察三格、全部失源黑屏，再让选定AV源完整120秒重入。VLC原生快照；记录CPU、内存、漂移、退出原因和资源账本。

构建产物：realtime 6118400字节，SHA256 3F0F2CFF6B47379D3BA375BB2848DB7636FBE74581A4F40495D3EF1A86F48CB2。完整目标还包括四源、HEVC输入重排、非固定节奏、C API/ABI及Windows后RKMPP，本轮不替代这些门禁。

## 启动命令（待执行）
```powershell
$ErrorActionPreference='Stop'
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r254-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r254-native- --snapshot-format=png rtp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r254-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r254-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r254-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r254-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r254-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r254-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r254 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r254-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r254-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r254-cli-error.log' -PassThru
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
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r254-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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
Get-ChildItem -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance' -Filter 'composition-r254-native-*.png' | Select-Object -ExpandProperty FullName
```

## 重入命令（待执行）
```powershell
$source=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r254-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r254-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($source.Id)"
$source.WaitForExit()
Write-Output "REENTRY_EXIT=$($source.ExitCode)"
exit $source.ExitCode
```

## 结果与清理

待运行后填写实际PID、命令结果、快照观察和关键指标；先记录，再按本轮精确文件/PID清单清理。保留指定源。

## 本轮执行进度

2026-10-19：A/B2均明确Windows局部静态PASS后启动。监控会话35498；VLC34100，初始源28304/3936/33068，CLI17492。命令按上文实际执行。09:09:21 VLC status/snapshot返回0，原生PNG已查看。实流进行中，尚无最终验收结论。

## 最终结果：FAIL

监控会话35498退出1：CLI17492自然错误退出1，三个初始源均自然退出0且3600帧/120秒。未执行重入和失源黑屏检查。唯一VLC原生快照已实际查看：三格画面、右下黑底。CPU平均2.261834%、峰6.570842%；工作集峰268120064字节。账本预约/释放86169相等，最终0对象/0字节；errors1、workerErrors1、payloadPressureFailures1。音频AU到mux已晚于dispatch约121.222146ms，服务可用时间晚于dispatch88.119330ms。画布最大134.13ms，其中源event wait提交仅0.0037ms，batch completion达133.9036ms；事件化未消除总GPU等待，不能宣称此前迟到已修复。该相关性仍需调用链核验，不能单凭最大值断言唯一根因。

关键原始诊断：
```text
[2026-10-19 09:10:11.831] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 datagrams=0 wire_bytes=0 immediate_deadlines=0 deferred_deadlines=0 planned_wait_ns=0 late_datagrams=0 maximum_lateness_ns=0 pending_bytes=564 peak_pending_bytes=110920 pressure_failures=0 access_units=5160 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=78005949 maximum_mux_input_after_emit_ns=221222146 maximum_mux_input_after_dispatch_ns=121222146 worst_mux_input_ready_ns=76060121400 worst_mux_input_emit_ns=75838899254 worst_mux_input_dispatch_ns=75938899254 maximum_protocol_batch_produced_after_release_ns=56802167 worst_protocol_batch_produced_ns=50818150400 worst_protocol_batch_release_ns=50761348233 worst_protocol_batch_deadline_ns=50861348233 worst_protocol_batch_datagrams=1 exit_reason=InvalidArgument: MPEG-TS access unit has no transport service window stream=audio emit_ns=75838899254 dispatch_ns=75938899254 available_ns=76027018584 last_committed_kind=maintenance service_start_ns=76027018584 actual_now_ns=76060130900 remaining_ns=-88119330 payload_bytes=564 wire_bytes=576
[2026-10-19 09:10:11.840] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=7653 committed_datagrams=47726 committed_payload_bytes=57497112 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=80004 pacing_reserved=47726 pacing_cancelled=0 pacing_submitted=47726 pacing_maximum_submit_lateness_ns=43724320 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=80004 backlog_max_residence_ns=67081400 last_materialized_sequence=47726 last_scheduled_sequence=47726 last_submitted_sequence=47726 last_committed_sequence=47726 shared_scope_charged_datagrams=47726 shared_scope_charged_wire_bytes=58833440 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2155 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2155 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=379902 endpoint_1_target_effective_socket_bytes=189951 endpoint_1_api_requested_socket_bytes=189951 endpoint_1_effective_socket_bytes=189951 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=47708 endpoint_1_committed_payload_bytes=57495888 endpoint_2_target_effective_socket_bytes=189951 endpoint_2_api_requested_socket_bytes=189951 endpoint_2_effective_socket_bytes=189951 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=18 endpoint_2_committed_payload_bytes=1224
[2026-10-19 09:10:11.957] [info] [graph][runtime.node] continuous_audio_aggregate.summary reason=abort output_group=composition-r254.output audio_commits=3054 real_samples=3127296 silence_samples=0
[2026-10-19 09:10:11.958] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=canvas count=2124 maximum_ns=134130000 begin_steady_ns=1625605658300 end_steady_ns=1625739788300 video_cursor=2120 audio_cursor=not_owned next_video_cursor=2120 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 09:10:11.959] [info] [graph][runtime.node] canvas_compose_correlated scope=attempt begin_steady_ns=1625605658600 end_steady_ns=1625739786600 wall_ns=134128000 thread_cpu_ns=0 surface_count=1 surface_total_ns=700 surface_maximum_ns=700 surface_maximum_tile=none surface_maximum_result=unavailable validate_count=3 validate_total_ns=1300 validate_maximum_ns=800 validate_maximum_tile=0 validate_maximum_result=0 black_count=1 black_total_ns=31600 black_maximum_ns=31600 black_maximum_tile=none black_maximum_result=0 tile_count=3 tile_total_ns=32600 tile_maximum_ns=12100 tile_maximum_tile=0 tile_maximum_result=0 publish_count=1 publish_total_ns=145200 publish_maximum_ns=145200 publish_maximum_tile=none publish_maximum_result=0 context_push_count=1 context_push_total_ns=0 context_push_maximum_ns=0 context_push_maximum_tile=none context_push_maximum_result=unavailable source_wait_count=4 source_wait_total_ns=3700 source_wait_maximum_ns=1900 source_wait_maximum_tile=0 source_wait_maximum_result=0 plane_copy_count=8 plane_copy_total_ns=55500 plane_copy_maximum_ns=25100 plane_copy_maximum_tile=none plane_copy_maximum_result=0 destination_wait_count=0 destination_wait_total_ns=0 destination_wait_maximum_ns=0 destination_wait_maximum_tile=none destination_wait_maximum_result=unavailable context_pop_count=1 context_pop_total_ns=5400 context_pop_maximum_ns=5400 context_pop_maximum_tile=none context_pop_maximum_result=0 batch_completion_count=1 batch_completion_total_ns=133903600 batch_completion_maximum_ns=133903600 batch_completion_maximum_tile=none batch_completion_maximum_result=0
[CLI] final runtime report: queued=0, peakQueued=982, threads=82, processThreads=129, workers=0, workerProgress=197842, workerProcessCalls=796278, workerWaits=598424, workerWakeups=592933, workerErrors=1, errors=1, stalledIntervals=0, cpuSamples=223, logicalProcessors=22, averageSystemMachineCpuPercent=19.995608, averageProcessMachineCpuPercent=2.261834, peakProcessMachineCpuPercent=6.570842, averageProcessSingleCoreCpuPercent=49.760345, peakProcessSingleCoreCpuPercent=144.558522, initialWorkingSetBytes=207659008, workingSetBytes=267960320, peakWorkingSetBytes=268120064, totalPushed=181723, totalPopped=181039, droppedBuffers=0, encodedPacketsPushed=23700, encodedPacketsPopped=23695, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=17587804, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=777, graphPayloadReservations=86169, graphPayloadReleases=86169, graphPayloadPressureFailures=1, lastInputReceivedAtNs=1625747111300
```

本轮清理清单：
composition-r254-cli-error.log
composition-r254-cli.log
composition-r254-monitor.log
composition-r254-native-2026-10-19-09h09m21s742.png
composition-r254-output.sdp
composition-r254-source0-stdout.log
composition-r254-source0.log
composition-r254-source1-stdout.log
composition-r254-source1.log
composition-r254-source2-stdout.log
composition-r254-source2.log
composition-r254-vlc.log

清理复核：上述12件产物已逐项删除，前缀无残留，五PID无残留；指定120秒源保留。VLC为测试结束清理，CLI退出非清理导致。

## 调用链初查（未完成根因判定）

MediaTsDatagramEmissionSchedule::beginAccessUnit 使用 max(emit, availableAt) 计算服务起点；r254 的 availableAt 已晚于音频 dispatch，故按既有约束拒绝。ProjectMpegTsMuxSessionAdapter::poll 在媒体不可用时仍按真实时钟产生 PCR/PSI maintenance，不能仅因 last_committed_kind=maintenance 就移除维护或放宽期限。视频 canvas 同步返回后才生成下游帧；音频有独立 aggregate 节点，因此还需核验共享 scheduler 的双流头部等待，不能直接声称 canvas 锁住音频线程。

[NVIDIA 异步执行指南](https://docs.nvidia.com/cuda/cuda-programming-guide/02-basics/asynchronous-execution.html) 区分设备依赖与主机等待：event query 可非阻塞检查完成，异步提交本身不保证并发执行。下一设计须覆盖 completion owner、有限在途/背压、错误传播及编码器消费时机，不能单独删除 batch completion 同步。
