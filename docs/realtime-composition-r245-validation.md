# r245：Release realtime 封装基础回归验证

## 状态

已结束，实流失败并完成清理。保持r244原规格：三路120秒H264 1280×720@30输入，HEVC 1280×720@30 CBR8000kbps、AAC192kbps输出MPEG-TS/RTP；覆盖三格、失源黑屏、所选音频源重入及受控停止。VLC仅原生snapshot。新单AU上界尚未接入planner，本轮不能证明完整容量契约或r239根因已修复。

## 启动与后台监控命令

```powershell
$ErrorActionPreference='Stop'
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r245-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r245-native- --snapshot-format=png rtp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r245-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r245-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r245-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r245-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r245-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r245-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r245 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r245-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r245-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r245-cli-error.log' -PassThru
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
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r245-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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
Get-ChildItem -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance' -Filter 'composition-r245-native-*.png' | Select-Object -ExpandProperty FullName
```

## 重入命令

```powershell
$source=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r245-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r245-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($source.Id)"
$source.WaitForExit()
Write-Output "REENTRY_EXIT=$($source.ExitCode)"
exit $source.ExitCode
```

## 本轮进程和产物

2026-10-16 14:25启动；VLC40152，初始源39540/10944/33544，CLI38600，监控会话64514。CLI SHA256 27554F2E72C98FB4E3779CCD6CC36C5075C765D99D83C413927F6E885966EB0F。已结束，详见失败结果。

## 结果：失败，已结束

监控会话64514最终exit1；CLI自然exit1，三路初始FFmpeg均exit0，完整120秒/3600帧。CLI运行约25秒即报错，不是源结束自然退出。未执行重入及caller-stop，上述重入命令仅为原定步骤，不能记为已执行。

VLC RC status为play state3、snapshot返回0。14:26:33.991原生PNG已实际查看：三个视频格、右下黑；截图发生在CLI失败后，可能显示末帧，不能证明该时刻仍持续输出。未使用系统截图。

外部24样本，最后24.6809185秒；CPU均值1.7219080690925344%、峰4.4831427784117%；WS首142368768、末265068544、峰265609216字节。首值含启动，不作泄漏判定。漂移21样本最大绝对raw_phase15874ns，不是内容同步证明。最终workers/queued均0，workerErrors/errors各1，payload current bytes/objects均0，reservations=releases19467，pressureFailures1；不能因资源回收成功而判定实流通过。

直接失败：audio AU的emit=24944803954、dispatch=25044803954，而maintenance已将available推进到25066255592，剩余窗口-21451638ns。scheduler等待缺失video head170736600ns，audio最终晚52.7ms进入mux。sender未报deadline miss或would-block；单AU上界新方法尚未被planner调用，不能归因于新上界容量公式，也不能直接宣布源码抽取导致/不导致此次失败。

同轮canvas最大137.778ms，source_wait最大131.1472ms，destination_wait最大53.2469ms；这些最大事件与late head的序号并不直接一致（cursor584与output sequence584差一），不能把它们当作已经完成的同帧因果链。下一步针对实际失败核对missing-peer等待、maintenance推进以及CUDA readiness等待。不得扩大外部FFmpeg修复范围或放宽验收预算。

### 关键原始诊断

```text
[2026-10-16 14:26:15.333] [info] [graph][runtime.node] av_scheduler_trace stage=late_video_head generation=1 mapping_epoch_generation=1 canonical_target_ns=19433333333 canonical_dispatch_ns=19433333333 target_master_ns=25066255201 dispatch_master_ns=25066255201 now_ns=25096730600
[2026-10-16 14:26:15.333] [info] [graph][runtime.node] av_scheduler_trace stage=late_output_commit stream=audio generation=1 canonical_dispatch_ns=19411882086 emit_ns=24944803954 dispatch_ns=25044803954 committed_at_ns=25097071000 canonical_sequence=837 head_missing_valid=1 head_missing_intervals=1 head_missing_completed_ns=170736600 head_missing_active=0 head_missing_last_begin_ns=24926307700 head_missing_last_end_ns=25097044300 head_resumed_generation=1 head_resumed_sequence=584 head_arrived_ns=24926274200 scheduled_at_ns=25097051500
[2026-10-16 14:26:15.333] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 datagrams=0 wire_bytes=0 immediate_deadlines=0 deferred_deadlines=0 planned_wait_ns=0 late_datagrams=0 maximum_lateness_ns=0 pending_bytes=752 peak_pending_bytes=110920 pressure_failures=0 access_units=1419 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=383 maximum_mux_input_after_emit_ns=152705946 maximum_mux_input_after_dispatch_ns=52705946 worst_mux_input_ready_ns=25097509900 worst_mux_input_emit_ns=24944803954 worst_mux_input_dispatch_ns=25044803954 maximum_protocol_batch_produced_after_release_ns=5807032 worst_protocol_batch_produced_ns=23838728900 worst_protocol_batch_release_ns=23832921868 worst_protocol_batch_deadline_ns=23932921868 worst_protocol_batch_datagrams=23 exit_reason=InvalidArgument: MPEG-TS access unit has no transport service window stream=audio emit_ns=24944803954 dispatch_ns=25044803954 available_ns=25066255592 last_committed_kind=maintenance service_start_ns=25066255592 actual_now_ns=25097524900 remaining_ns=-21451638 payload_bytes=752 wire_bytes=764
[2026-10-16 14:26:15.334] [info] [graph][runtime.node] av_scheduler_trace stage=late_output_commit stream=video generation=1 canonical_dispatch_ns=19433333333 emit_ns=24966255201 dispatch_ns=25066255201 committed_at_ns=25098140600 canonical_sequence=584 head_missing_valid=1 head_missing_intervals=0 head_missing_completed_ns=0 head_missing_active=0 head_missing_last_begin_ns=unavailable head_missing_last_end_ns=unavailable head_resumed_generation=unavailable head_resumed_sequence=unavailable head_arrived_ns=25096729200 scheduled_at_ns=25098124200
[2026-10-16 14:26:15.349] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=2110 committed_datagrams=13856 committed_payload_bytes=16789888 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=80004 pacing_reserved=13856 pacing_cancelled=0 pacing_submitted=13856 pacing_maximum_submit_lateness_ns=5913932 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=80004 backlog_max_residence_ns=46914600 last_materialized_sequence=13856 last_scheduled_sequence=13856 last_submitted_sequence=13856 last_committed_sequence=13856 shared_scope_charged_datagrams=13856 shared_scope_charged_wire_bytes=17177856 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2155 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2155 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=379902 endpoint_1_target_effective_socket_bytes=189951 endpoint_1_api_requested_socket_bytes=189951 endpoint_1_effective_socket_bytes=189951 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=13851 endpoint_1_committed_payload_bytes=16789548 endpoint_2_target_effective_socket_bytes=189951 endpoint_2_api_requested_socket_bytes=189951 endpoint_2_effective_socket_bytes=189951 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=5 endpoint_2_committed_payload_bytes=340
[2026-10-16 14:26:15.505] [info] [graph][runtime.node] cuda_canvas_call stage=source_wait count=1759 operation=copy maximum_ns=131147200 begin_steady_ns=20770284071400 end_steady_ns=20770415218600 native_result=0 thread_cpu_ns=0
[2026-10-16 14:26:15.506] [info] [graph][runtime.node] cuda_canvas_call stage=destination_wait count=2346 operation=copy maximum_ns=53246900 begin_steady_ns=20770209074100 end_steady_ns=20770262321000 native_result=0 thread_cpu_ns=0
[2026-10-16 14:26:15.506] [info] [graph][runtime.node] canvas_compose_correlated scope=attempt begin_steady_ns=20770278909400 end_steady_ns=20770416685800 wall_ns=137776400 thread_cpu_ns=0 surface_count=1 surface_total_ns=600 surface_maximum_ns=600 surface_maximum_tile=none surface_maximum_result=unavailable validate_count=3 validate_total_ns=1200 validate_maximum_ns=500 validate_maximum_tile=0 validate_maximum_result=0 black_count=1 black_total_ns=5155700 black_maximum_ns=5155700 black_maximum_tile=none black_maximum_result=0 tile_count=3 tile_total_ns=132603400 tile_maximum_ns=131412000 tile_maximum_tile=0 tile_maximum_result=0 publish_count=1 publish_total_ns=11700 publish_maximum_ns=11700 publish_maximum_tile=none publish_maximum_result=0 context_push_count=4 context_push_total_ns=2700 context_push_maximum_ns=1000 context_push_maximum_tile=1 context_push_maximum_result=0 source_wait_count=3 source_wait_total_ns=131150500 source_wait_maximum_ns=131145100 source_wait_maximum_tile=0 source_wait_maximum_result=0 plane_copy_count=8 plane_copy_total_ns=646000 plane_copy_maximum_ns=317800 plane_copy_maximum_tile=none plane_copy_maximum_result=0 destination_wait_count=4 destination_wait_total_ns=5883100 destination_wait_maximum_ns=4759300 destination_wait_maximum_tile=none destination_wait_maximum_result=0 context_pop_count=4 context_pop_total_ns=19100 context_pop_maximum_ns=6000 context_pop_maximum_tile=1 context_pop_maximum_result=0
[CLI] final runtime report: queued=0, peakQueued=911, threads=82, processThreads=132, workers=0, workerProgress=56254, workerProcessCalls=227674, workerWaits=171412, workerWakeups=169838, workerErrors=1, errors=1, stalledIntervals=0, cpuSamples=61, logicalProcessors=22, averageSystemMachineCpuPercent=19.672978, averageProcessMachineCpuPercent=2.063981, peakProcessMachineCpuPercent=6.106870, averageProcessSingleCoreCpuPercent=45.407586, peakProcessSingleCoreCpuPercent=134.351145, initialWorkingSetBytes=207142912, workingSetBytes=265068544, peakWorkingSetBytes=265068544, totalPushed=50529, totalPopped=49891, droppedBuffers=0, encodedPacketsPushed=6531, encodedPacketsPopped=6526, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=15474881, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=690, graphPayloadReservations=19467, graphPayloadReleases=19467, graphPayloadPressureFailures=1, lastInputReceivedAtNs=20770380311900
[in @ 000002519374b500] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 10792.521689
[aac @ 000002518d99cc00] Qavg: 586.678
[aac @ 000002518d99cc00] 2 frames left in the queue on closing
[CLI] realtime video runtime failed: InvalidArgument: MPEG-TS access unit has no transport service window stream=audio emit_ns=24944803954 dispatch_ns=25044803954 available_ns=25066255592 last_committed_kind=maintenance service_start_ns=25066255592 actual_now_ns=25097524900 remaining_ns=-21451638 payload_bytes=752 wire_bytes=764
```

## 精确清理命令

```powershell
$proc=Get-CimInstance Win32_Process -Filter 'ProcessId=40152'
if($proc){if($proc.Name -ne 'vlc.exe' -or $proc.CommandLine -notlike '*composition-r245*'){throw 'VLC identity mismatch'};Stop-Process -Id 40152 -Force}
$root=[IO.Path]::GetFullPath('D:/Code/MyCode/MediaTranscode/out/acceptance')+[IO.Path]::DirectorySeparatorChar
$names=@('composition-r245-cli-error.log','composition-r245-cli.log','composition-r245-monitor.log','composition-r245-native-2026-10-16-14h26m33s991.png','composition-r245-output.sdp','composition-r245-source0-stdout.log','composition-r245-source0.log','composition-r245-source1-stdout.log','composition-r245-source1.log','composition-r245-source2-stdout.log','composition-r245-source2.log','composition-r245-vlc.log')
foreach($name in $names){$path=[IO.Path]::GetFullPath((Join-Path $root $name));if(-not $path.StartsWith($root,[StringComparison]::OrdinalIgnoreCase)){throw 'Cleanup escaped acceptance directory'};if(Test-Path -LiteralPath $path){Remove-Item -LiteralPath $path -Force;Write-Output "REMOVED $name"}}
Get-ChildItem -LiteralPath $root -Filter 'composition-r245-*' | Select-Object Name
Get-CimInstance Win32_Process | Where-Object {$_.ProcessId -in @(40152,39540,10944,33544,38600)} | Select-Object ProcessId,Name
Get-Item -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4' | Select-Object Length
```

## 清理核验与后续定位

12项清单文件均删除，本轮前缀无残留，五个PID均无残留；指定源124427809字节保留。VLC为精确PID强制清理，不记自然退出。未执行远程测试。

独立源码调查确认：scheduler任一非EOF流缺head就等待，保留跨流排序约束；mux空闲仍发送PCR/PSI，并且不回拨已推进的clock。因此last_committed_kind=maintenance不是首因证明。下一定位应沿同一generation/output lineage sequence记录aggregate生成与提交、encoder send/receive/commit、scheduler head到达及受阻audio837释放，区别GPU完成、编码滞留、背压与跨流frontier问题；不能直接放行缺少排序证据的音频、冻结maintenance或增大100ms预算。
