# r249：Release realtime 重复验证

同r248冻结源码与产物SHA2565360F388524FCC2AD12E32C616177DEEE07C4062F4D80C660432202FC14D73FB。三路原120秒H264720p30→HEVC720p30 CBR8000kbps/AAC192kbps/MPEG-TS-RTP，输入等待100/150/200ms、wire100ms、egress50Mbps、准备2GiB。重点第二次失源全黑，不替代完整矩阵或内容A/V。

## 实际启动及监控命令
```powershell
$ErrorActionPreference='Stop'
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r249-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r249-native- --snapshot-format=png rtp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r249-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r249-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r249-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r249-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r249-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r249-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r249 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r249-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r249-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r249-cli-error.log' -PassThru
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
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r249-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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
## VLC原生快照
```powershell
$client=[Net.Sockets.TcpClient]::new('127.0.0.1',63020);try{$stream=$client.GetStream();$stream.ReadTimeout=2000;$bytes=[Text.Encoding]::ASCII.GetBytes("status`r`nsnapshot`r`n");$stream.Write($bytes,0,$bytes.Length);$buffer=[byte[]]::new(4096);try{while(($n=$stream.Read($buffer,0,4096))-gt 0){[Text.Encoding]::UTF8.GetString($buffer,0,$n)}}catch [IO.IOException]{}}finally{$client.Dispose()}
Get-ChildItem -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance' -Filter 'composition-r249-native-*.png' | Select-Object -ExpandProperty FullName
```
## 重入命令（首阶段未失败时）
```powershell
$source=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r249-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r249-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($source.Id)"
$source.WaitForExit()
Write-Output "REENTRY_EXIT=$($source.ExitCode)"
exit $source.ExitCode
```


## 结果：FAIL
初始源36200/43780/26692和重入13144均120秒3600帧自然退出0；CLI34028于15:24:07自然失败退出1；VLC21752。无caller_stop。三张原生快照已查看，RC status/snapshot均0：三格、首次全黑、仅左下重入；第二次失源黑屏未完成，CLI已失败。
原错：protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=212884944948 materialized_ns=212948276300 protocol_deadline_ns=212928432764。物化超过协议deadline19.843536ms；本轮late_mux音频9143，ready晚19.519236ms。
最大canvas137.3582ms，cursor6369对应输出6370，source_wait135.6477ms、batch completion1.4153ms。与失败相邻，但缺失败video head记录，不把该最大帧直接当作音频9143的因果证明。批处理未根治超期；下一调查生产者逐帧完成依赖。
最终errors1/workers0/queued0/payload0、reservation/release175402相等、pressureFailures2。CPU均2.081928%、峰5.405405%；WS初始189194240、峰249466880、最终246538240B。发送136836包、deadline_misses0/would_block0，pacing最大迟49.68044ms、队列驻留85.9973ms。sender无deadline miss不抵消前端协议失败。
AAC关闭残留2帧、颜色range变化警告保留。完整门禁FAIL，不创建成功提交；未执行RKMPP。
## 清理命令
```powershell
$p=Get-CimInstance Win32_Process -Filter 'ProcessId=21752'
if($p -and $p.Name -eq 'vlc.exe' -and $p.CommandLine -like '*composition-r249*'){Stop-Process -Id 21752 -Force}
$root=[IO.Path]::GetFullPath('D:/Code/MyCode/MediaTranscode/out/acceptance')
$names=@('composition-r249-cli-error.log','composition-r249-cli.log','composition-r249-monitor.log','composition-r249-native-2026-10-16-15h21m02s285.png','composition-r249-native-2026-10-16-15h22m55s781.png','composition-r249-native-2026-10-16-15h23m27s737.png','composition-r249-output.sdp','composition-r249-reentry-stdout.log','composition-r249-reentry.log','composition-r249-source0-stdout.log','composition-r249-source0.log','composition-r249-source1-stdout.log','composition-r249-source1.log','composition-r249-source2-stdout.log','composition-r249-source2.log','composition-r249-vlc.log')
foreach($name in $names){$path=[IO.Path]::GetFullPath((Join-Path $root $name));if(-not $path.StartsWith($root+[IO.Path]::DirectorySeparatorChar)){throw 'Outside test directory'};if(Test-Path -LiteralPath $path){Remove-Item -LiteralPath $path -Force}}
Get-ChildItem -LiteralPath $root -Filter 'composition-r249-*'
Get-CimInstance Win32_Process | Where-Object {$_.ProcessId -in @(21752,36200,43780,26692,34028,13144)} | Select-Object ProcessId,Name
(Get-Item -LiteralPath (Join-Path $root 'test-continuous-120s.mp4')).Length
```

## 原始诊断
[2026-10-16 15:24:06.726] [info] [graph][runtime.node] au_processing_trace stage=late_mux_input generation=1 stream=audio canonical_sequence=9143 dispatch_master_ns=212928432764 ready_master_ns=212947952000 scheduled_created_steady_ns=24241798903800 adapter_dequeued_steady_ns=24241798956700 adapted_created_steady_ns=24241798958000 write_entered_steady_ns=24241799378600 session_ready_steady_ns=24241799385400
[2026-10-16 15:24:06.742] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=22958 committed_datagrams=136836 committed_payload_bytes=163180880 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=80004 pacing_reserved=136836 pacing_cancelled=0 pacing_submitted=136836 pacing_maximum_submit_lateness_ns=49680440 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=80004 backlog_max_residence_ns=85997300 last_materialized_sequence=136836 last_scheduled_sequence=136836 last_submitted_sequence=136836 last_committed_sequence=136836 shared_scope_charged_datagrams=136836 shared_scope_charged_wire_bytes=167012288 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2155 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2155 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=379902 endpoint_1_target_effective_socket_bytes=189951 endpoint_1_api_requested_socket_bytes=189951 endpoint_1_effective_socket_bytes=189951 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=136785 endpoint_1_committed_payload_bytes=163177412 endpoint_2_target_effective_socket_bytes=189951 endpoint_2_api_requested_socket_bytes=189951 endpoint_2_effective_socket_bytes=189951 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=51 endpoint_2_committed_payload_bytes=3468
[2026-10-16 15:24:06.973] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 datagrams=0 wire_bytes=0 immediate_deadlines=0 deferred_deadlines=0 planned_wait_ns=0 late_datagrams=0 maximum_lateness_ns=0 pending_bytes=0 peak_pending_bytes=92872 pressure_failures=0 access_units=15513 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=56512184 maximum_mux_input_after_emit_ns=119519236 maximum_mux_input_after_dispatch_ns=19519236 worst_mux_input_ready_ns=212947952000 worst_mux_input_emit_ns=212828432764 worst_mux_input_dispatch_ns=212928432764 maximum_protocol_batch_produced_after_release_ns=63427752 worst_protocol_batch_produced_ns=212948372700 worst_protocol_batch_release_ns=212884944948 worst_protocol_batch_deadline_ns=212951607368 worst_protocol_batch_datagrams=23 exit_reason=aborted
[2026-10-16 15:24:06.975] [info] [graph][runtime.node] continuous_audio_aggregate.summary reason=abort output_group=composition-r249.output audio_commits=9151 real_samples=8213504 silence_samples=1157120
[2026-10-16 15:24:06.976] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=canvas count=6373 maximum_ns=137358200 begin_steady_ns=24241636908900 end_steady_ns=24241774267100 video_cursor=6369 audio_cursor=not_owned next_video_cursor=6369 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-16 15:24:06.976] [info] [graph][runtime.node] continuous_aggregate.summary reason=abort output_group=composition-r249.output output_generation=1 video_commits=6373 source[0].group=composition-r249.source.0 source[0].real_frames=3571 source[0].black_frames=2802 source[0].contribution_generation=0 source[1].group=composition-r249.source.1 source[1].real_frames=3571 source[1].black_frames=2802 source[1].contribution_generation=0 source[2].group=composition-r249.source.2 source[2].real_frames=5586 source[2].black_frames=787 source[2].contribution_generation=2
[2026-10-16 15:24:06.977] [info] [graph][runtime.node] cuda_canvas_call stage=batch_completion count=5588 operation=copy maximum_ns=47221400 begin_steady_ns=24112637476100 end_steady_ns=24112684697500 native_result=0 thread_cpu_ns=0
[2026-10-16 15:24:06.977] [info] [graph][runtime.node] canvas_compose_correlated scope=attempt begin_steady_ns=24241636909900 end_steady_ns=24241774266000 wall_ns=137356100 thread_cpu_ns=0 surface_count=1 surface_total_ns=1200 surface_maximum_ns=1200 surface_maximum_tile=none surface_maximum_result=unavailable validate_count=1 validate_total_ns=1600 validate_maximum_ns=1600 validate_maximum_tile=2 validate_maximum_result=0 black_count=1 black_total_ns=83200 black_maximum_ns=83200 black_maximum_tile=none black_maximum_result=0 tile_count=1 tile_total_ns=135807700 tile_maximum_ns=135807700 tile_maximum_tile=2 tile_maximum_result=0 publish_count=1 publish_total_ns=16900 publish_maximum_ns=16900 publish_maximum_tile=none publish_maximum_result=0 context_push_count=1 context_push_total_ns=6400 context_push_maximum_ns=6400 context_push_maximum_tile=none context_push_maximum_result=0 source_wait_count=1 source_wait_total_ns=135647700 source_wait_maximum_ns=135647700 source_wait_maximum_tile=2 source_wait_maximum_result=0 plane_copy_count=4 plane_copy_total_ns=223400 plane_copy_maximum_ns=114000 plane_copy_maximum_tile=2 plane_copy_maximum_result=0 destination_wait_count=0 destination_wait_total_ns=0 destination_wait_maximum_ns=0 destination_wait_maximum_tile=none destination_wait_maximum_result=unavailable context_pop_count=1 context_pop_total_ns=10000 context_pop_maximum_ns=10000 context_pop_maximum_tile=none context_pop_maximum_result=0 batch_completion_count=1 batch_completion_total_ns=1415300 batch_completion_maximum_ns=1415300 batch_completion_maximum_tile=none batch_completion_maximum_result=0
[2026-10-16 15:24:07.077] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=failed detail="protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=212884944948 materialized_ns=212948276300 protocol_deadline_ns=212928432764 deadline_before_release=0 materialized_after_deadline=1" description="D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r249-output.sdp" error=1 native=0 stage=3 reason="protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=212884944948 materialized_ns=212948276300 protocol_deadline_ns=212928432764 deadline_before_release=0 materialized_after_deadline=1"
[CLI] final runtime report: queued=0, peakQueued=378, threads=82, processThreads=130, workers=0, workerProgress=434609, workerProcessCalls=1713811, workerWaits=1279193, workerWakeups=1262914, workerErrors=1, errors=1, stalledIntervals=0, cpuSamples=629, logicalProcessors=22, averageSystemMachineCpuPercent=11.696528, averageProcessMachineCpuPercent=2.081928, peakProcessMachineCpuPercent=5.405405, averageProcessSingleCoreCpuPercent=45.802427, peakProcessSingleCoreCpuPercent=118.918919, initialWorkingSetBytes=189194240, workingSetBytes=246538240, peakWorkingSetBytes=249466880, totalPushed=397608, totalPopped=397555, droppedBuffers=0, encodedPacketsPushed=71207, encodedPacketsPopped=71202, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=4766219, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=159, graphPayloadReservations=175402, graphPayloadReleases=175402, graphPayloadPressureFailures=2, lastInputReceivedAtNs=24241778965500
漂移绝对值统计ns：

Count             : 185
Average           : 
Sum               : 
Maximum           : 22676
Minimum           : 
StandardDeviation : 
Property          : 


清理复核：16个清单文件已逐项删除，前缀和六PID均无残留；指定源124427809B保留。无远程产物。VLC为测试后清理。
