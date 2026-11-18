# r236：CUDA canvas stream 隔离实流复验

清理记录：VLC RC shutdown后5秒仍存活，核对本轮身份后强制清理PID2644，不记为自然退出。12件本轮临时产物按以下清单删除；指定120秒源保留。无远程测试。

```powershell
$root=[IO.Path]::GetFullPath('D:/Code/MyCode/MediaTranscode/out/acceptance')+[IO.Path]::DirectorySeparatorChar
$names=@('composition-r236-cli-error.log','composition-r236-cli.log','composition-r236-monitor.log','composition-r236-native-2026-10-16-10h44m51s891.png','composition-r236-output.sdp','composition-r236-source0-stdout.log','composition-r236-source0.log','composition-r236-source1-stdout.log','composition-r236-source1.log','composition-r236-source2-stdout.log','composition-r236-source2.log','composition-r236-vlc.log')
foreach($name in $names){$path=[IO.Path]::GetFullPath((Join-Path $root $name));if(-not $path.StartsWith($root,[StringComparison]::OrdinalIgnoreCase)){throw 'Cleanup escaped acceptance directory'};if(Test-Path -LiteralPath $path){Remove-Item -LiteralPath $path -Force;Write-Output "REMOVED $name"}}
Get-ChildItem -LiteralPath $root -Filter 'composition-r236-*' | Select-Object Name
Get-CimInstance Win32_Process | Where-Object {$_.ProcessId -in @(2644,34324,27040,30952,28848)} | Select-Object ProcessId,Name
Get-Item -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4' | Select-Object Length
```

## 结果

FAIL。Release realtime CLI SHA256 E5DCE25592C2B8FDA3C3B3F3C3B6E0ED7C64F1238DB8AFD5CD105C0C59962289。VLC PID2644，源34324/27040/30952，CLI28848；CLI约73秒自行exit1，物化前超期。未进入失源/重入，不能判定完整验收通过。VLC原生快照10:44:51.891返回0，实际查看三格视频、右下黑底。

生产设备/首个动态源stream均为0；私有nonblocking已准备成功。最长canvas97.6142ms中黑底51.6541ms，黑底私有destination等待51.3976ms；动态tile0 source等待45.1452ms。此峰值与最终失败相隔约40秒，不能拼为根因；隔离不提供GPU/驱动/OS执行时限保证。

物化失败attempts=7、blocked=6，原因为wire global sequence reservation exceeds planner backlog capacity。release72777833844，deadline72844500512，失败attempt72845538300，超期1.037788ms。下一步核对同batch/backlog占用及sender释放时间，不扩大容量或放宽deadline绕过错误。

## 实际启动与监控命令

```powershell
$ErrorActionPreference='Stop'
$r236Vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r236-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r236-native- --snapshot-format=png rtp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($r236Vlc.Id)"
$r236Source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r236-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r236-source0.log' -PassThru
Write-Output "SOURCE0_PID=$($r236Source0.Id)"
$r236Source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r236-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r236-source1.log' -PassThru
Write-Output "SOURCE1_PID=$($r236Source1.Id)"
$r236Source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r236-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r236-source2.log' -PassThru
Write-Output "SOURCE2_PID=$($r236Source2.Id)"
$r236Cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r236 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r236-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r236-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r236-cli-error.log' -PassThru
Write-Output "CLI_PID=$($r236Cli.Id)"
$r236Clock=[Diagnostics.Stopwatch]::StartNew()
$r236LastTime=0.0
$r236LastCpu=0.0
$r236Cores=[Environment]::ProcessorCount
while(-not $r236Cli.WaitForExit(1000)) {
$r236Cli.Refresh()
$r236Time=$r236Clock.Elapsed.TotalSeconds
$r236Cpu=$r236Cli.TotalProcessorTime.TotalSeconds
$r236Usage=100*($r236Cpu-$r236LastCpu)/(($r236Time-$r236LastTime)*$r236Cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r236-monitor.log' -Value "seconds=$r236Time cpu=$r236Usage ws=$($r236Cli.WorkingSet64) private=$($r236Cli.PrivateMemorySize64)"
$r236LastTime=$r236Time
$r236LastCpu=$r236Cpu
}
Write-Output "CLI_EXIT=$($r236Cli.ExitCode)"
$r236Source0.WaitForExit()
Write-Output "SOURCE0_EXIT=$($r236Source0.ExitCode)"
$r236Source1.WaitForExit()
Write-Output "SOURCE1_EXIT=$($r236Source1.ExitCode)"
$r236Source2.WaitForExit()
Write-Output "SOURCE2_EXIT=$($r236Source2.ExitCode)"
exit $r236Cli.ExitCode
```

## VLC原生快照命令

```powershell
$r236Client=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
try {$r236Stream=$r236Client.GetStream(); $r236Stream.ReadTimeout=2000; $r236Bytes=[Text.Encoding]::ASCII.GetBytes("snapshot`r`n"); $r236Stream.Write($r236Bytes,0,$r236Bytes.Length); $r236Buffer=[byte[]]::new(4096); try {while(($r236Read=$r236Stream.Read($r236Buffer,0,4096)) -gt 0){[Text.Encoding]::UTF8.GetString($r236Buffer,0,$r236Read)}}catch [IO.IOException] {}} finally {$r236Client.Dispose()}
Get-ChildItem -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance' -Filter 'composition-r236-native-*.png' | Select-Object -ExpandProperty FullName
```


## 指标与诊断

外部CPU样本=71，均值=2.10102116839122，峰值=6.45865333807739；WS峰值=268447744。drift样本=67，maxAbs raw_phase_ns=2269，不等于内容级AV证明。

```text
[2026-10-16 10:44:30.157] [info] [graph][planner.capability] canvas_cuda_stream stage=prepared device_stream=0000000000000000 private_nonblocking=1
[2026-10-16 10:44:30.159] [info] [graph][runtime.node] canvas_cuda_stream stage=first_source device_stream=0000000000000000 readiness=device_stream
[2026-10-16 10:44:30.245] [info] [graph][planner.capability] composition_preparation_storage stage=graph_bound scope=shared_reservations capacity=2147483648 retained=845363342 high_water=871279754
[2026-10-16 10:44:30.442] [info] [graph][runtime.node] mpegts_emission_plan generation=1 mode=canonical_access_unit_window access_unit_window_ns=100000000 video_initial_service_window_ns=13995843 audio_initial_service_window_ns=13995843 packet_size_bytes=188 maximum_payload_bytes=1316 per_datagram_overhead_bytes=12
[2026-10-16 10:45:37.710] [info] [graph][runtime.node] protocol_batch_trace stage=materialization_failed generation=1 attempts=7 consumed_datagrams=0 remaining_datagrams=1 attempted_master_ns=72845538300 mux_dequeued_master_ns=72777874200 first_dequeued_master_ns=72781950900 last_wire_published_master_ns=72781947200 previous_attempt_started_master_ns=72823790600 previous_attempt_completed_master_ns=72823798800 last_attempt_completed_master_ns=72845547500 last_blocked_started_master_ns=72823790600 last_blocked_completed_master_ns=72823798800 blocked_attempts=6 last_blocked_code=WouldBlock last_blocked_native=0 last_blocked_original_message_bytes=65 last_blocked_message_truncated=0 last_blocked_message="wire global sequence reservation exceeds planner backlog capacity" remaining_front_release_master_ns=72777833844 remaining_front_deadline_master_ns=72844500512 original_error="InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=72777833844 materialized_ns=72845538300 protocol_deadline_ns=72844500512 deadline_before_release=0 materialized_after_deadline=1"
[2026-10-16 10:45:37.710] [info] [graph][runtime.node] process.failed node=80 name=MediaMpegTsDatagramMaterializerNode kind=MpegTsDatagramMaterializer inputs=[channel=118 edge=118 stream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=120 edge=120 stream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=121 edge=121 stream=Metadata edge_kind=Unknown payload=Unknown queued=5] outputs=[channel=122 edge=122 stream=Metadata edge_kind=Unknown payload=Unknown queued=0] error=InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=72777833844 materialized_ns=72845538300 protocol_deadline_ns=72844500512 deadline_before_release=0 materialized_after_deadline=1
[2026-10-16 10:45:37.710] [info] [graph][runtime.node] worker.failed node=80 kind=MpegTsDatagramMaterializer name=MPEG-TS wire protocol materializer error=InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=72777833844 materialized_ns=72845538300 protocol_deadline_ns=72844500512 deadline_before_release=0 materialized_after_deadline=1
[2026-10-16 10:45:37.728] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=7269 committed_datagrams=47881 committed_payload_bytes=58057164 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=80004 pacing_reserved=47881 pacing_cancelled=0 pacing_submitted=47881 pacing_maximum_submit_lateness_ns=27086760 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=80004 backlog_max_residence_ns=64624300 last_materialized_sequence=47881 last_scheduled_sequence=47881 last_submitted_sequence=47881 last_committed_sequence=47881 shared_scope_charged_datagrams=47881 shared_scope_charged_wire_bytes=59397832 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2155 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2155 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=379902 endpoint_1_target_effective_socket_bytes=189951 endpoint_1_api_requested_socket_bytes=189951 endpoint_1_effective_socket_bytes=189951 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=47866 endpoint_1_committed_payload_bytes=58056144 endpoint_2_target_effective_socket_bytes=189951 endpoint_2_api_requested_socket_bytes=189951 endpoint_2_effective_socket_bytes=189951 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=15 endpoint_2_committed_payload_bytes=1020
[2026-10-16 10:45:37.839] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 datagrams=0 wire_bytes=0 immediate_deadlines=0 deferred_deadlines=0 planned_wait_ns=0 late_datagrams=0 maximum_lateness_ns=0 pending_bytes=0 peak_pending_bytes=110920 pressure_failures=0 access_units=4911 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=53605977 maximum_mux_input_after_emit_ns=91279487 maximum_mux_input_after_dispatch_ns=-8720513 worst_mux_input_ready_ns=32438726500 worst_mux_input_emit_ns=32347447013 worst_mux_input_dispatch_ns=32447447013 maximum_protocol_batch_produced_after_release_ns=61152864 worst_protocol_batch_produced_ns=32438985900 worst_protocol_batch_release_ns=32377833036 worst_protocol_batch_deadline_ns=32477832501 worst_protocol_batch_datagrams=21 exit_reason=aborted
[2026-10-16 10:45:37.840] [info] [graph][runtime.node] av_scheduler_correlated_wait generation=1 canonical_sequence=1150 missing=video begin_steady_ns=7491102816600 end_steady_ns=7491180389600 observed_wait_ns=77573000 held_dispatch_master_ns=32424227059 begin_master_ns=32322124400 end_master_ns=32399697400 dispatch_remaining_at_end_ns=24529659 resumed_dispatch_master_ns=32444499168
[2026-10-16 10:45:37.842] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=process count=14050 maximum_ns=97775800 begin_steady_ns=7491118671300 end_steady_ns=7491216447100 video_cursor=802 audio_cursor=not_owned next_video_cursor=802 next_audio_cursor=not_owned error=None return_state=progress deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-10-16 10:45:37.842] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=purge count=14050 maximum_ns=115300 begin_steady_ns=7506330097000 end_steady_ns=7506330212300 video_cursor=1260 audio_cursor=not_owned next_video_cursor=1260 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-16 10:45:37.842] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=bind count=14050 maximum_ns=139500 begin_steady_ns=7509227354200 end_steady_ns=7509227493700 video_cursor=1347 audio_cursor=not_owned next_video_cursor=1347 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-16 10:45:37.842] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=consume count=14050 maximum_ns=2712900 begin_steady_ns=7477129858100 end_steady_ns=7477132571000 video_cursor=384 audio_cursor=not_owned next_video_cursor=384 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-16 10:45:37.842] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=activate count=22 maximum_ns=14000 begin_steady_ns=7464358524300 end_steady_ns=7464358538300 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-16 10:45:37.843] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=build_video count=2019 maximum_ns=97704800 begin_steady_ns=7491118709600 end_steady_ns=7491216414400 video_cursor=802 audio_cursor=not_owned next_video_cursor=802 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-16 10:45:37.843] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=canvas count=2019 maximum_ns=97615700 begin_steady_ns=7491118771000 end_steady_ns=7491216386700 video_cursor=802 audio_cursor=not_owned next_video_cursor=802 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-16 10:45:37.843] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=emit count=2019 maximum_ns=1919000 begin_steady_ns=7470407450200 end_steady_ns=7470409369200 video_cursor=181 audio_cursor=not_owned next_video_cursor=181 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-16 10:45:37.843] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=between_calls_previous_return count=14049 maximum_ns=135011700 begin_steady_ns=7464220796700 end_steady_ns=7464355808400 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=waiting deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-10-16 10:45:37.843] [info] [graph][runtime.node] cuda_canvas_call stage=context_push count=8082 operation=copy maximum_ns=346700 begin_steady_ns=7500558835400 end_steady_ns=7500559182100 native_result=0 thread_cpu_ns=0
[2026-10-16 10:45:37.844] [info] [graph][runtime.node] cuda_canvas_call stage=source_wait count=6061 operation=copy maximum_ns=45148500 begin_steady_ns=7491170437300 end_steady_ns=7491215585800 native_result=0 thread_cpu_ns=0
[2026-10-16 10:45:37.844] [info] [graph][runtime.node] cuda_canvas_call stage=plane_copy count=16164 operation=copy maximum_ns=828700 begin_steady_ns=7464360368100 end_steady_ns=7464361196800 native_result=0 thread_cpu_ns=0
[2026-10-16 10:45:37.844] [info] [graph][runtime.node] cuda_canvas_call stage=destination_wait count=8082 operation=copy maximum_ns=51399900 begin_steady_ns=7491119021800 end_steady_ns=7491170421700 native_result=0 thread_cpu_ns=0
[2026-10-16 10:45:37.844] [info] [graph][runtime.node] cuda_canvas_call stage=context_pop count=8082 operation=copy maximum_ns=397500 begin_steady_ns=7464359040300 end_steady_ns=7464359437800 native_result=0 thread_cpu_ns=15625000
[2026-10-16 10:45:37.844] [info] [graph][runtime.node] canvas_compose_correlated scope=attempt begin_steady_ns=7491118771800 end_steady_ns=7491216386000 wall_ns=97614200 thread_cpu_ns=0 surface_count=1 surface_total_ns=1100 surface_maximum_ns=1100 surface_maximum_tile=none surface_maximum_result=unavailable validate_count=3 validate_total_ns=2300 validate_maximum_ns=1100 validate_maximum_tile=0 validate_maximum_result=0 black_count=1 black_total_ns=51654100 black_maximum_ns=51654100 black_maximum_tile=none black_maximum_result=0 tile_count=3 tile_total_ns=45941600 tile_maximum_ns=45466800 tile_maximum_tile=0 tile_maximum_result=0 publish_count=1 publish_total_ns=10800 publish_maximum_ns=10800 publish_maximum_tile=none publish_maximum_result=0 context_push_count=4 context_push_total_ns=4000 context_push_maximum_ns=1300 context_push_maximum_tile=none context_push_maximum_result=0 source_wait_count=3 source_wait_total_ns=45150100 source_wait_maximum_ns=45145200 source_wait_maximum_tile=0 source_wait_maximum_result=0 plane_copy_count=8 plane_copy_total_ns=503700 plane_copy_maximum_ns=162600 plane_copy_maximum_tile=none plane_copy_maximum_result=0 destination_wait_count=4 destination_wait_total_ns=51864300 destination_wait_maximum_ns=51397600 destination_wait_maximum_tile=none destination_wait_maximum_result=0 context_pop_count=4 context_pop_total_ns=19400 context_pop_maximum_ns=8100 context_pop_maximum_tile=0 context_pop_maximum_result=0
[2026-10-16 10:45:37.884] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=3112 completion_waits=3112 completed_datagrams=3166 drained_datagrams=54 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-16 10:45:37.885] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=43190 completion_waits=43190 completed_datagrams=64111 drained_datagrams=20921 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-16 10:45:37.885] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=43370 completion_waits=43370 completed_datagrams=64113 drained_datagrams=20743 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-16 10:45:37.886] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=45092 completion_waits=45092 completed_datagrams=64174 drained_datagrams=19083 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[CLI] final runtime report: queued=0, peakQueued=929, threads=82, processThreads=130, workers=0, workerProgress=180729, workerProcessCalls=778539, workerWaits=597803, workerWakeups=592588, workerErrors=1, errors=1, stalledIntervals=0, cpuSamples=211, logicalProcessors=22, averageSystemMachineCpuPercent=13.893159, averageProcessMachineCpuPercent=2.224012, peakProcessMachineCpuPercent=11.776447, averageProcessSingleCoreCpuPercent=48.928274, peakProcessSingleCoreCpuPercent=259.081836, initialWorkingSetBytes=209186816, workingSetBytes=267571200, peakWorkingSetBytes=268447744, totalPushed=161315, totalPopped=160674, droppedBuffers=0, encodedPacketsPushed=22541, encodedPacketsPopped=22540, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=15990495, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=703, graphPayloadReservations=63735, graphPayloadReleases=63735, graphPayloadPressureFailures=0, lastInputReceivedAtNs=7531624275500
[in @ 0000024f7f4cd080] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000024f7f4cd080] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 38400.081078
[in @ 0000024f7f4cd7c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000024f7f4cd7c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 31650.415278
[in @ 0000024f189963c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000024f189963c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 406.828622
[in @ 0000024f186d82c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000024f1e3b9540] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000024f1e3b9540] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 38400.081078
[in @ 0000024f186d82c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 406.828622
[in @ 0000024f181eb740] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000024f181eb740] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 31650.415278
[aac @ 0000024f18251dc0] Qavg: 672.484
[aac @ 0000024f18251dc0] 2 frames left in the queue on closing
[CLI] realtime video runtime failed: InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=72777833844 materialized_ns=72845538300 protocol_deadline_ns=72844500512 deadline_before_release=0 materialized_after_deadline=1

```

## 结束与清理

主监控会话76920最终exit1，CLI自行exit1。三个FFmpeg源均exit0；均完成120秒3600帧。VLC存在playback too late、picture missing34ms及Failed to create video converter，未将播放器警告忽略为通过。

composition-r236-source0.log
```text
[out#0/rtp @ 000002b6d4519500] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
[out#1/rtp @ 000002b6d6258a00] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.49    
```

composition-r236-source1.log
```text
frame= 3594 fps= 30 q=-1.0 size=  118662KiB time=00:01:59.80 bitrate=8114.2kbits/s speed=   1x elapsed=0:01:59.28    
[out#0/rtp @ 0000025aaee9ad00] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.47    
```

composition-r236-source2.log
```text
frame= 3594 fps= 30 q=-1.0 size=  118662KiB time=00:01:59.80 bitrate=8114.2kbits/s speed=   1x elapsed=0:01:59.27    
[out#0/rtp @ 0000022fe7a45400] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
```

清理复核：12件清单文件已删除，本轮前缀与5个PID无残留，指定源124427809字节保留。

## 源码核对后的证据边界

MediaWireGlobalSequenceState.cpp 的同一WouldBlock同时覆盖三种条件：commitBeforeNextReservation且已有reservation、datagram数量不足、wire字节不足。因此错误文案不能证明容量不足。sender最终记录backlog current=0、highWater=59/80004、最大residence=64624300ns、deadline_misses=0；终态不能反推失败瞬间占用。后续需要同锁记录具体阻断条件和reservation/occupancy/limit事实，并与sender提交释放关联；不得按文案扩大容量。
