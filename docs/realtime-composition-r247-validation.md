# r247：Release realtime 成功取包区间定位

## 状态

已执行，重入阶段失败。保持r246同规格三路120秒H264 720p30输入→HEVC720p30 CBR8000kbps/AAC192kbps/MPEG-TS-RTP，wire residence100ms、egress50Mbps、逐源输入等待100/150/200ms、准备存储2GiB。仅realtime，VLC原生snapshot。

## 启动和后台监控命令

```powershell
$ErrorActionPreference='Stop'
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r247-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r247-native- --snapshot-format=png rtp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r247-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r247-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r247-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r247-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r247-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r247-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r247 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r247-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r247-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r247-cli-error.log' -PassThru
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
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r247-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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
Get-ChildItem -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance' -Filter 'composition-r247-native-*.png' | Select-Object -ExpandProperty FullName
```

## 本轮结果：FAIL（2026-10-16）

Release 全量构建会话99961退出0；CLI 6072832字节，14:55:36，SHA256 A558D909A447BE7AE1D34EB60268951BE36E8CC8673759A373E6611C4B3EF0D7。
VLC PID43448；初始源40148/28756/39744均完整3600帧120秒自然退出0；CLI40316在15:01:21失败自然退出1。重入源35264在测试判定失败后核对身份并强制清理，退出-1，未完成120秒，不能记为通过。
三次VLC原生RC status/snapshot均返回0，图片已实际查看：14:59:00三视频格加右下黑格；15:00:56全黑；15:01:34仅左下重入画面（CLI已失败，可能为最后保留帧，不能证明持续播放）。没有系统截图。

迟到视频generation1 sequence4609：receive区间22876746796400→22876746797500，共1100ns；head22876746842100，receive结束到head44600ns。该次成功取包不是长等待点，不能排除此前send/EAGAIN或GPU工作。canvas cursor4608对应输出4609，最大调用189287400ns；相关copy区间189285900ns，source_wait104332800ns，destination_wait84612200ns，包含黑底等待。此关联支持继续调查该帧canvas之前及同步边界，不能据此直接删除同步。
音频6615 mux到达比dispatch晚59680287ns，原错为MPEG-TS access unit has no transport service window；剩余窗口-56556363ns。最终errors1、workers0、queued0、payload0，reservations/releases均120390；峰值WS269144064B，内部CPU平均1.915051%、峰值5.447471%。发送89846包、deadline_misses0、would_block0，最大submit lateness46764120ns、最大队列驻留56816000ns；这些不构成线速交付证明。
完整功能验收仍FAIL；没有成功提交。颜色range unknown→tv警告仍存在。未执行RKMPP。

## 重入命令
```powershell
$source=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r247-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r247-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($source.Id)"
$source.WaitForExit()
Write-Output "REENTRY_EXIT=$($source.ExitCode)"
exit $source.ExitCode
```
## 清理命令
```powershell
$p=Get-CimInstance Win32_Process -Filter 'ProcessId=43448'
if($p -and $p.Name -eq 'vlc.exe' -and $p.CommandLine -like '*composition-r247*'){Stop-Process -Id 43448 -Force}
$root=[IO.Path]::GetFullPath('D:/Code/MyCode/MediaTranscode/out/acceptance')
$names=@('composition-r247-cli-error.log','composition-r247-cli.log','composition-r247-monitor.log','composition-r247-native-2026-10-16-14h59m00s805.png','composition-r247-native-2026-10-16-15h00m56s731.png','composition-r247-native-2026-10-16-15h01m34s341.png','composition-r247-output.sdp','composition-r247-reentry-stdout.log','composition-r247-reentry.log','composition-r247-source0-stdout.log','composition-r247-source0.log','composition-r247-source1-stdout.log','composition-r247-source1.log','composition-r247-source2-stdout.log','composition-r247-source2.log','composition-r247-vlc.log')
foreach($name in $names){$path=[IO.Path]::GetFullPath((Join-Path $root $name));if(-not $path.StartsWith($root+[IO.Path]::DirectorySeparatorChar)){throw 'Outside test directory'};if(Test-Path -LiteralPath $path){Remove-Item -LiteralPath $path -Force}}
Get-ChildItem -LiteralPath $root -Filter 'composition-r247-*'
Get-CimInstance Win32_Process | Where-Object {$_.ProcessId -in @(43448,40148,28756,39744,40316,35264)} | Select-Object ProcessId,Name
(Get-Item -LiteralPath (Join-Path $root 'test-continuous-120s.mp4')).Length
```

## 关键原始诊断
[2026-10-16 15:01:21.674] [info] [graph][runtime.node] av_scheduler_trace stage=late_video_head generation=1 mapping_epoch_generation=1 canonical_target_ns=153600000000 canonical_dispatch_ns=153600000000 target_master_ns=159251852968 dispatch_master_ns=159251852968 now_ns=159287331700 node=74 identity_kind=output identity="composition-r247.output" canonical_sequence=4609 canonical_created_steady_ns=22876746807100 head_observed_steady_ns=22876746842100 codec_receive_begin_steady_ns=22876746796400 codec_receive_end_steady_ns=22876746797500
[2026-10-16 15:01:21.674] [info] [graph][runtime.node] av_scheduler_trace stage=late_audio_head generation=1 mapping_epoch_generation=1 canonical_target_ns=153600000000 canonical_dispatch_ns=153600000000 target_master_ns=159251852968 dispatch_master_ns=159251852968 now_ns=159288269700 node=74 identity_kind=output identity="composition-r247.output" canonical_sequence=6616 canonical_created_steady_ns=22876592393800 head_observed_steady_ns=22876747780000 codec_receive_begin_steady_ns=unavailable codec_receive_end_steady_ns=unavailable
[2026-10-16 15:01:21.675] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 datagrams=0 wire_bytes=0 immediate_deadlines=0 deferred_deadlines=0 planned_wait_ns=0 late_datagrams=0 maximum_lateness_ns=0 pending_bytes=752 peak_pending_bytes=110920 pressure_failures=0 access_units=11222 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=19593939 maximum_mux_input_after_emit_ns=159680287 maximum_mux_input_after_dispatch_ns=59680287 worst_mux_input_ready_ns=159288313300 worst_mux_input_emit_ns=159128633013 worst_mux_input_dispatch_ns=159228633013 maximum_protocol_batch_produced_after_release_ns=48165548 worst_protocol_batch_produced_ns=110670313300 worst_protocol_batch_release_ns=110622147752 worst_protocol_batch_deadline_ns=110722147752 worst_protocol_batch_datagrams=1 exit_reason=InvalidArgument: MPEG-TS access unit has no transport service window stream=audio emit_ns=159128633013 dispatch_ns=159228633013 available_ns=159285189376 last_committed_kind=maintenance service_start_ns=159285189376 actual_now_ns=159288327100 remaining_ns=-56556363 payload_bytes=752 wire_bytes=764
[2026-10-16 15:01:21.675] [info] [graph][runtime.node] au_processing_trace stage=late_mux_input generation=1 stream=audio canonical_sequence=6615 dispatch_master_ns=159228633013 ready_master_ns=159288313300 scheduled_created_steady_ns=22876747457200 adapter_dequeued_steady_ns=22876747556600 adapted_created_steady_ns=22876747557500 write_entered_steady_ns=22876747820400 session_ready_steady_ns=22876747823700
[2026-10-16 15:01:21.688] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=16616 committed_datagrams=89846 committed_payload_bytes=105324664 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=80004 pacing_reserved=89846 pacing_cancelled=0 pacing_submitted=89846 pacing_maximum_submit_lateness_ns=46764120 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=80004 backlog_max_residence_ns=56816000 last_materialized_sequence=89846 last_scheduled_sequence=89846 last_submitted_sequence=89846 last_committed_sequence=89846 shared_scope_charged_datagrams=89846 shared_scope_charged_wire_bytes=107840352 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2155 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2155 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=379902 endpoint_1_target_effective_socket_bytes=189951 endpoint_1_api_requested_socket_bytes=189951 endpoint_1_effective_socket_bytes=189951 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=89810 endpoint_1_committed_payload_bytes=105322216 endpoint_2_target_effective_socket_bytes=189951 endpoint_2_api_requested_socket_bytes=189951 endpoint_2_effective_socket_bytes=189951 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=36 endpoint_2_committed_payload_bytes=2448
[2026-10-16 15:01:21.779] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=canvas count=4612 maximum_ns=189287400 begin_steady_ns=22876545050500 end_steady_ns=22876734337900 video_cursor=4608 audio_cursor=not_owned next_video_cursor=4608 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-16 15:01:21.780] [info] [graph][runtime.node] canvas_compose_correlated scope=attempt begin_steady_ns=22876545051400 end_steady_ns=22876734337300 wall_ns=189285900 thread_cpu_ns=0 surface_count=1 surface_total_ns=1200 surface_maximum_ns=1200 surface_maximum_tile=none surface_maximum_result=unavailable validate_count=1 validate_total_ns=1700 validate_maximum_ns=1700 validate_maximum_tile=2 validate_maximum_result=0 black_count=1 black_total_ns=79830400 black_maximum_ns=79830400 black_maximum_tile=none black_maximum_result=0 tile_count=1 tile_total_ns=109433200 tile_maximum_ns=109433200 tile_maximum_tile=2 tile_maximum_result=0 publish_count=1 publish_total_ns=14300 publish_maximum_ns=14300 publish_maximum_tile=none publish_maximum_result=0 context_push_count=2 context_push_total_ns=4200 context_push_maximum_ns=3200 context_push_maximum_tile=none context_push_maximum_result=0 source_wait_count=1 source_wait_total_ns=104332800 source_wait_maximum_ns=104332800 source_wait_maximum_tile=2 source_wait_maximum_result=0 plane_copy_count=4 plane_copy_total_ns=262700 plane_copy_maximum_ns=130100 plane_copy_maximum_tile=none plane_copy_maximum_result=0 destination_wait_count=2 destination_wait_total_ns=84612200 destination_wait_maximum_ns=79629300 destination_wait_maximum_tile=none destination_wait_maximum_result=0 context_pop_count=2 context_pop_total_ns=17700 context_pop_maximum_ns=9000 context_pop_maximum_tile=2 context_pop_maximum_result=0
[CLI] final runtime report: queued=0, peakQueued=923, threads=82, processThreads=130, workers=0, workerProgress=338985, workerProcessCalls=1353167, workerWaits=1014173, workerWakeups=1002351, workerErrors=1, errors=1, stalledIntervals=0, cpuSamples=474, logicalProcessors=22, averageSystemMachineCpuPercent=12.120986, averageProcessMachineCpuPercent=1.915051, peakProcessMachineCpuPercent=5.447471, averageProcessSingleCoreCpuPercent=42.131133, peakProcessSingleCoreCpuPercent=119.844358, initialWorkingSetBytes=208134144, workingSetBytes=254341120, peakWorkingSetBytes=269144064, totalPushed=300757, totalPopped=300699, droppedBuffers=0, encodedPacketsPushed=51523, encodedPacketsPopped=51517, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=15740304, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=688, graphPayloadReservations=120390, graphPayloadReleases=120390, graphPayloadPressureFailures=1, lastInputReceivedAtNs=22876746031800
phase绝对值统计（ns）：

Count             : 121
Average           : 
Sum               : 
Maximum           : 15873
Minimum           : 
StandardDeviation : 
Property          : 


清理复核：上述16个临时文件已逐项删除；本轮前缀文件与六个PID均无残留。指定源保留124427809字节。VLC为测试后清理退出，不计自然退出。本轮没有远程测试或远程产物。
