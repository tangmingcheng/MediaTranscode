# r235：缺视频等待与生产阶段关联复验

## 独立审查与下一实现边界

审查B确认同attempt同步调用耗时，支持调查共享stream过宽同步，但不认为已证明它导致seq168644超期。adapter每次copy等待sourceDevice->stream、向destinationDevice->stream提交、再等待destination；它们是设备context字段而非帧完成事件。本机FFmpeg源码初始化stream=NULL，实际运行值仍需readback。[NVIDIA官方stream同步语义](https://docs.nvidia.com/cuda/cuda-driver-api/stream-sync-behavior.html)提供non-blocking stream隔离机制；不能把调用耗时全归于GPU或无关工作。

黑底prepare已upload同步并download/readback校验，compose只读；该ready事实可封闭为资源契约。下一步先probe实际context/stream及Driver API能力，再细化私有non-blocking stream的RAII实现：不改共享AVCUDADeviceContext::stream；动态tile保留生产完成依赖；publish前及部分失败后等待本adapter已提交工作，保留原始错误。不能只删source sync把等待移到共享destination。此为调查/设计方向，尚未实施或验证修复。

## 临时产物清理

```powershell
$r235Root=[IO.Path]::GetFullPath('D:/Code/MyCode/MediaTranscode/out/acceptance')+[IO.Path]::DirectorySeparatorChar
$r235Names=@('composition-r235-cli-error.log','composition-r235-cli.log','composition-r235-monitor.log','composition-r235-native-2026-10-16-10h23m13s020.png','composition-r235-native-2026-10-16-10h25m04s357.png','composition-r235-native-2026-10-16-10h25m42s230.png','composition-r235-output.sdp','composition-r235-reentry-stdout.log','composition-r235-reentry.log','composition-r235-source0-stdout.log','composition-r235-source0.log','composition-r235-source1-stdout.log','composition-r235-source1.log','composition-r235-source2-stdout.log','composition-r235-source2.log','composition-r235-vlc.log')
foreach($r235Name in $r235Names){$r235Path=[IO.Path]::GetFullPath((Join-Path $r235Root $r235Name));if(-not $r235Path.StartsWith($r235Root,[StringComparison]::OrdinalIgnoreCase)){throw 'Cleanup path escaped acceptance directory'};if(Test-Path -LiteralPath $r235Path){Remove-Item -LiteralPath $r235Path -Force;Write-Output "REMOVED $r235Name"}}
Get-ChildItem -LiteralPath $r235Root -Filter 'composition-r235-*' | Select-Object Name
Get-CimInstance Win32_Process | Where-Object {$_.ProcessId -in @(28872,11500,12096,30032,32556,32860)} | Select-Object ProcessId,Name
Get-Item -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4' | Select-Object Length
```

## 实流结果：FAIL（2026-10-16）

VLC PID28872，初始源11500/12096/30032，CLI32556，重入源32860。四个源均完成120秒/3600帧exit0；重入会话98093 exit0。CLI在10:27:14.017 reservation失败，自行exit1；主监控会话7913 exit1。这不是caller_stop，也没有正常drain通过结论。未抓包、录制或离线解码。

VLC原生快照三次返回0并实际查看：10:23:13.020三格视频/右下黑；10:25:04.357全黑且CLI活着；10:25:42.230左下格重入，其余黑。失败后未再截图，不能补充重入结束后的全黑证明。外部CPU262样本均值1.562765%、峰3.153876%；外部WS峰251576320字节。内部CPU均值1.565151%、峰6.218905%，WS初始190078976、最终248655872、峰250830848。drift generation1有118样本maxAbs22675ns，generation2有107样本maxAbs11338ns，不等于内容级AV对齐。最终queued/workers/payload bytes/objects为0，reservations=releases=214494；workerErrors/errors=2，payloadPressureFailures=1。FFmpeg仍有range变更警告、AAC关闭剩2帧；VLC存在late及两条Failed to create video converter。

### 同事件与关联边界

- seq168644 activation开始270179314300已经超completion deadline270179098473共0.215827ms，超latest start270178971753共0.342547ms。activation开始至snapshot锁后采样9.8us；permit起止差0.2us。snapshot开始与锁后采样显示相同，不能称严格零耗时，但本次记录不支持将超期归因该锁等待。
- 同一canvas scope6427786878800～6427921069200，134.1904ms，位于aggregate cursor8086的canvas区间内。黑底copy117.3218ms；最大source_wait91.8757ms、tile=none，是黑底路径源stream同步调用耗时，不是生成黑色像素或tile2的耗时。嵌套阶段不能与外层重复相加，线程CPU=0也不证明全为GPU执行。
- audio11606缺video等待115.9594ms，在6427884038800恢复，比上述compose结束早37.0304ms；恢复video不能直接当作cursor8086输出。等待结束还有3.313464ms dispatch余量，与r233模式不同。
- materializer最大样本属于seq167977，不能嫁接到168644。当前没有168644→AU identity→cursor8086的完整身份关联，不能把所有峰值拼为根因。独立审查A对上述限定证据明确PASS。

下一步分两条有证据的路径：补seq168644来源AU及协议/物化/发布/入队时刻；核对黑底source、canvas和encoder的真实CUDA stream身份及同步语义。不得直接删除sync、放宽deadline或修外部FFmpeg依赖。

### 原始关键观测（保留完整关联字段）

```text
[2026-10-16 10:22:44.227] [info] [graph][planner.capability] composition_preparation_storage stage=graph_bound scope=shared_reservations capacity=2147483648 retained=830086359 high_water=856293936
[2026-10-16 10:27:14.017] [info] [graph][runtime.node] datagram_reservation_trace stage=reservation_failed generation=1 endpoint=1 sequence=168644 release_master_ns=270165090260 deadline_master_ns=270179098473 activation_started_master_ns=270179314300 activation_completed_master_ns=270179323100 permit_started_master_ns=270179323700 permit_completed_master_ns=270179323900 snapshot_started_master_ns=270179324100 snapshot_sampled_master_ns=270179324100 snapshot_completed_master_ns=270179324900 reservation_completed_master_ns=270179350900 original_error="IoFailure: Datagram GBRA reservation misses its immutable completion deadline sequence=168644 now_ns=270179324100 release_ns=270165090260 deadline_ns=270179098473 not_before_ns=270165925780 not_after_ns=270178971753 duration_ns=126720 rate=6250000 queue_wire_bytes=792 average_queue_residence_ns=324100"
[2026-10-16 10:27:14.022] [info] [graph][runtime.node] scheduled_datagram_sender stage=failed generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=29143 committed_datagrams=168643 committed_payload_bytes=200057156 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=80004 pacing_reserved=168643 pacing_cancelled=0 pacing_submitted=168643 pacing_maximum_submit_lateness_ns=30121120 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=1 backlog_current_wire_bytes=792 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=80004 backlog_max_residence_ns=69295800 last_materialized_sequence=168644 last_scheduled_sequence=168644 last_submitted_sequence=168643 last_committed_sequence=168643 shared_scope_charged_datagrams=168643 shared_scope_charged_wire_bytes=204779160 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2155 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2155 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=379902 endpoint_1_target_effective_socket_bytes=189951 endpoint_1_api_requested_socket_bytes=189951 endpoint_1_effective_socket_bytes=189951 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=168578 endpoint_1_committed_payload_bytes=200052736 endpoint_2_target_effective_socket_bytes=189951 endpoint_2_api_requested_socket_bytes=189951 endpoint_2_effective_socket_bytes=189951 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=65 endpoint_2_committed_payload_bytes=4420
[2026-10-16 10:27:14.030] [info] [graph][runtime.node] mpegts_wire_materializer stage=aborted maximum_materialized_after_release_ns=57674540 worst_materialized_at_ns=269324074600 worst_release_ns=269266400060 worst_deadline_ns=269366400060 worst_global_sequence=167977
[2026-10-16 10:27:14.030] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 datagrams=0 wire_bytes=0 immediate_deadlines=0 deferred_deadlines=0 planned_wait_ns=0 late_datagrams=0 maximum_lateness_ns=0 pending_bytes=0 peak_pending_bytes=92872 pressure_failures=0 access_units=19695 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=85991787 maximum_mux_input_after_emit_ns=99814727 maximum_mux_input_after_dispatch_ns=-185273 worst_mux_input_ready_ns=270178913200 worst_mux_input_emit_ns=270079098473 worst_mux_input_dispatch_ns=270179098473 maximum_protocol_batch_produced_after_release_ns=31114008 worst_protocol_batch_produced_ns=270129537600 worst_protocol_batch_release_ns=270098423592 worst_protocol_batch_deadline_ns=270165084868 worst_protocol_batch_datagrams=18 exit_reason=aborted
[2026-10-16 10:27:14.030] [info] [graph][runtime.node] av_scheduler_correlated_wait generation=1 canonical_sequence=11606 missing=video begin_steady_ns=6427768079400 end_steady_ns=6427884038800 observed_wait_ns=115959400 held_dispatch_master_ns=270132658564 begin_master_ns=270013385600 end_master_ns=270129345100 dispatch_remaining_at_end_ns=3313464 resumed_dispatch_master_ns=270165084868
[2026-10-16 10:27:14.032] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=process count=48490 maximum_ns=134375500 begin_steady_ns=6427786741200 end_steady_ns=6427921116700 video_cursor=8086 audio_cursor=not_owned next_video_cursor=8086 next_audio_cursor=not_owned error=None return_state=progress deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-10-16 10:27:14.032] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=purge count=48490 maximum_ns=1410600 begin_steady_ns=6217720889900 end_steady_ns=6217722300500 video_cursor=1784 audio_cursor=not_owned next_video_cursor=1784 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-16 10:27:14.032] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=bind count=48490 maximum_ns=680900 begin_steady_ns=6408721311700 end_steady_ns=6408721992600 video_cursor=7514 audio_cursor=not_owned next_video_cursor=7514 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-16 10:27:14.032] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=consume count=48490 maximum_ns=12187300 begin_steady_ns=6217123727500 end_steady_ns=6217135914800 video_cursor=1767 audio_cursor=not_owned next_video_cursor=1767 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-16 10:27:14.032] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=activate count=16 maximum_ns=45000 begin_steady_ns=6158253110900 end_steady_ns=6158253155900 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-16 10:27:14.032] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=build_video count=8089 maximum_ns=134325600 begin_steady_ns=6427786762000 end_steady_ns=6427921087600 video_cursor=8086 audio_cursor=not_owned next_video_cursor=8086 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-16 10:27:14.032] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=canvas count=8089 maximum_ns=134192000 begin_steady_ns=6427786878000 end_steady_ns=6427921070000 video_cursor=8086 audio_cursor=not_owned next_video_cursor=8086 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-16 10:27:14.032] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=emit count=8089 maximum_ns=11051900 begin_steady_ns=6232122075800 end_steady_ns=6232133127700 video_cursor=2216 audio_cursor=not_owned next_video_cursor=2216 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-16 10:27:14.033] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=between_calls_previous_return count=48489 maximum_ns=83463700 begin_steady_ns=6158166348600 end_steady_ns=6158249812300 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=waiting deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-10-16 10:27:14.033] [info] [graph][runtime.node] canvas_compose_correlated scope=attempt begin_steady_ns=6427786878800 end_steady_ns=6427921069200 wall_ns=134190400 thread_cpu_ns=0 surface_count=1 surface_total_ns=1000 surface_maximum_ns=1000 surface_maximum_tile=none surface_maximum_result=unavailable validate_count=1 validate_total_ns=1000 validate_maximum_ns=1000 validate_maximum_tile=2 validate_maximum_result=0 black_count=1 black_total_ns=117321800 black_maximum_ns=117321800 black_maximum_tile=none black_maximum_result=0 tile_count=1 tile_total_ns=16855200 tile_maximum_ns=16855200 tile_maximum_tile=2 tile_maximum_result=0 publish_count=1 publish_total_ns=6300 publish_maximum_ns=6300 publish_maximum_tile=none publish_maximum_result=0 context_push_count=2 context_push_total_ns=5000 context_push_maximum_ns=3500 context_push_maximum_tile=none context_push_maximum_result=0 source_wait_count=2 source_wait_total_ns=108192400 source_wait_maximum_ns=91875700 source_wait_maximum_tile=none source_wait_maximum_result=0 plane_copy_count=4 plane_copy_total_ns=95900 plane_copy_maximum_ns=52700 plane_copy_maximum_tile=none plane_copy_maximum_result=0 destination_wait_count=2 destination_wait_total_ns=25846200 destination_wait_maximum_ns=25361500 destination_wait_maximum_tile=none destination_wait_maximum_result=0 context_pop_count=2 context_pop_total_ns=11600 context_pop_maximum_ns=8500 context_pop_maximum_tile=none context_pop_maximum_result=0
[CLI] final runtime report: queued=0, peakQueued=391, threads=82, processThreads=130, workers=0, workerProgress=516090, workerProcessCalls=2064707, workerWaits=1548609, workerWakeups=1527947, workerErrors=2, errors=2, stalledIntervals=0, cpuSamples=872, logicalProcessors=22, averageSystemMachineCpuPercent=12.609898, averageProcessMachineCpuPercent=1.565151, peakProcessMachineCpuPercent=6.218905, averageProcessSingleCoreCpuPercent=34.433329, peakProcessSingleCoreCpuPercent=136.815920, initialWorkingSetBytes=190078976, workingSetBytes=248655872, peakWorkingSetBytes=250830848, totalPushed=470965, totalPopped=470913, droppedBuffers=0, encodedPacketsPushed=90398, encodedPacketsPopped=90394, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=4341138, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=163, graphPayloadReservations=214494, graphPayloadReleases=214494, graphPayloadPressureFailures=1, lastInputReceivedAtNs=6427916352300
```

### VLC快照命令（三次）

```powershell
$r235Client=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
try {$r235Stream=$r235Client.GetStream(); $r235Stream.ReadTimeout=2000; $r235Bytes=[Text.Encoding]::ASCII.GetBytes("snapshot`r`n"); $r235Stream.Write($r235Bytes,0,$r235Bytes.Length); $r235Buffer=[byte[]]::new(4096); try {while(($r235Read=$r235Stream.Read($r235Buffer,0,4096)) -gt 0){[Text.Encoding]::UTF8.GetString($r235Buffer,0,$r235Read)}}catch [IO.IOException] {}} finally {$r235Client.Dispose()}
Get-ChildItem -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance' -Filter 'composition-r235-native-*.png' | Select-Object -ExpandProperty FullName
```

### VLC清理命令

```powershell
$r235VlcProcess=Get-CimInstance Win32_Process -Filter 'ProcessId=28872'
if($r235VlcProcess){if($r235VlcProcess.Name -ne 'vlc.exe' -or $r235VlcProcess.CommandLine -notlike '*composition-r235*'){throw 'VLC identity mismatch'};try{$r235Client=[Net.Sockets.TcpClient]::new('127.0.0.1',63020);try{$r235Bytes=[Text.Encoding]::ASCII.GetBytes("shutdown`r`n");$r235Client.GetStream().Write($r235Bytes,0,$r235Bytes.Length)}finally{$r235Client.Dispose()}}catch{Write-Output $_.Exception.Message};$r235VlcWait=Get-Process -Id 28872 -ErrorAction SilentlyContinue;if($r235VlcWait -and -not $r235VlcWait.WaitForExit(5000)){Stop-Process -Id 28872 -Force;Write-Output 'VLC_FORCE_CLEANUP=28872'}}
```

RC shutdown后5秒仍未退出，核对本轮身份后强制清理28872。六个本轮PID复核无残留；VLC不记为自然退出。无远程测试。


## 实际重入命令

```powershell
$r235Reentry=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r235-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r235-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($r235Reentry.Id)"
$r235Reentry.WaitForExit()
Write-Output "REENTRY_EXIT=$($r235Reentry.ExitCode)"
exit $r235Reentry.ExitCode
```

沿用r233 Release产物SHA256 70B526936D87FC096691CC117D38D552AEB096059BE229F31B42DB4ED15C5397，无源码改动。重跑原因是r233未归档完整aggregate/canvas阶段证据，无法回溯关联。本次保持原规格和失败语义，观察范围不构成完整验收。

## 实际启动与监控命令

```powershell
$ErrorActionPreference='Stop'
$r235Vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r235-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r235-native- --snapshot-format=png rtp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($r235Vlc.Id)"
$r235Source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r235-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r235-source0.log' -PassThru
Write-Output "SOURCE0_PID=$($r235Source0.Id)"
$r235Source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r235-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r235-source1.log' -PassThru
Write-Output "SOURCE1_PID=$($r235Source1.Id)"
$r235Source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r235-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r235-source2.log' -PassThru
Write-Output "SOURCE2_PID=$($r235Source2.Id)"
$r235Cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r235 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r235-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r235-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r235-cli-error.log' -PassThru
Write-Output "CLI_PID=$($r235Cli.Id)"
$r235Clock=[Diagnostics.Stopwatch]::StartNew()
$r235LastTime=0.0
$r235LastCpu=0.0
$r235Cores=[Environment]::ProcessorCount
while(-not $r235Cli.WaitForExit(1000)) {
$r235Cli.Refresh()
$r235Time=$r235Clock.Elapsed.TotalSeconds
$r235Cpu=$r235Cli.TotalProcessorTime.TotalSeconds
$r235Usage=100*($r235Cpu-$r235LastCpu)/(($r235Time-$r235LastTime)*$r235Cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r235-monitor.log' -Value "seconds=$r235Time cpu=$r235Usage ws=$($r235Cli.WorkingSet64) private=$($r235Cli.PrivateMemorySize64)"
$r235LastTime=$r235Time
$r235LastCpu=$r235Cpu
}
Write-Output "CLI_EXIT=$($r235Cli.ExitCode)"
$r235Source0.WaitForExit()
Write-Output "SOURCE0_EXIT=$($r235Source0.ExitCode)"
$r235Source1.WaitForExit()
Write-Output "SOURCE1_EXIT=$($r235Source1.ExitCode)"
$r235Source2.WaitForExit()
Write-Output "SOURCE2_EXIT=$($r235Source2.ExitCode)"
exit $r235Cli.ExitCode
```

清理复核：16件清单文件已删除，本轮前缀与6个PID无残留，指定源124427809字节保留。
