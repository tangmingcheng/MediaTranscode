# r237：reservation诊断实流复验

## 清理命令

```powershell
foreach($id in @(32544,22192,6724)){ $proc=Get-CimInstance Win32_Process -Filter "ProcessId=$id"; if($proc){if($proc.Name -ne 'ffmpeg.exe' -or $proc.CommandLine -notlike '*test-continuous-120s.mp4*'){throw 'Source identity mismatch'};Stop-Process -Id $id -Force;Write-Output "SOURCE_CLEANUP=$id"}}
$proc=Get-CimInstance Win32_Process -Filter 'ProcessId=27608';if($proc){if($proc.Name -ne 'vlc.exe' -or $proc.CommandLine -notlike '*composition-r237*'){throw 'VLC identity mismatch'};Stop-Process -Id 27608 -Force;Write-Output 'VLC_CLEANUP=27608'}
$root=[IO.Path]::GetFullPath('D:/Code/MyCode/MediaTranscode/out/acceptance')+[IO.Path]::DirectorySeparatorChar
$names=@('composition-r237-cli-error.log','composition-r237-cli.log','composition-r237-monitor.log','composition-r237-output.sdp','composition-r237-source0-stdout.log','composition-r237-source0.log','composition-r237-source1-stdout.log','composition-r237-source1.log','composition-r237-source2-stdout.log','composition-r237-source2.log','composition-r237-vlc.log')
foreach($name in $names){$path=[IO.Path]::GetFullPath((Join-Path $root $name));if(-not $path.StartsWith($root,[StringComparison]::OrdinalIgnoreCase)){throw 'Cleanup escaped acceptance directory'};if(Test-Path -LiteralPath $path){Remove-Item -LiteralPath $path -Force;Write-Output "REMOVED $name"}}
Get-ChildItem -LiteralPath $root -Filter 'composition-r237-*' | Select-Object Name
Get-CimInstance Win32_Process | Where-Object {$_.ProcessId -in @(27608,32544,22192,6724,24348)} | Select-Object ProcessId,Name
Get-Item -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4' | Select-Object Length
```

清理复核：11件本轮临时文件已删除，5个PID及本轮文件前缀均无残留，复用源124427809字节保留。没有远程测试。

## 结果：FAIL

Release realtime SHA256 FEB352AA7F649C2EA45653BE55982712DC08EAAD5B24216E1E486D1CC6140454。VLC27608，源32544/22192/6724，CLI24348。主会话1330确认CLI自行exit1，约21秒失败；未取得VLC快照，未验证失源或重入，不得判通过。

本次失败batch尝试1次、blocked_attempts=0，新诊断未触发。mux_dequeued21436989700已晚于deadline21425804655共11.185045ms；attempt21437032700晚11.228045ms，mux出队至attempt43us。不能当作r236的reservation阻塞复现。

## 实际启动监控命令

```powershell
$ErrorActionPreference='Stop'
$r237Vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r237-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r237-native- --snapshot-format=png rtp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($r237Vlc.Id)"
$r237Source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r237-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r237-source0.log' -PassThru
Write-Output "SOURCE0_PID=$($r237Source0.Id)"
$r237Source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r237-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r237-source1.log' -PassThru
Write-Output "SOURCE1_PID=$($r237Source1.Id)"
$r237Source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r237-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r237-source2.log' -PassThru
Write-Output "SOURCE2_PID=$($r237Source2.Id)"
$r237Cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r237 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r237-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r237-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r237-cli-error.log' -PassThru
Write-Output "CLI_PID=$($r237Cli.Id)"
$r237Clock=[Diagnostics.Stopwatch]::StartNew()
$r237LastTime=0.0
$r237LastCpu=0.0
$r237Cores=[Environment]::ProcessorCount
while(-not $r237Cli.WaitForExit(1000)) {
$r237Cli.Refresh()
$r237Time=$r237Clock.Elapsed.TotalSeconds
$r237Cpu=$r237Cli.TotalProcessorTime.TotalSeconds
$r237Usage=100*($r237Cpu-$r237LastCpu)/(($r237Time-$r237LastTime)*$r237Cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r237-monitor.log' -Value "seconds=$r237Time cpu=$r237Usage ws=$($r237Cli.WorkingSet64) private=$($r237Cli.PrivateMemorySize64)"
$r237LastTime=$r237Time
$r237LastCpu=$r237Cpu
}
Write-Output "CLI_EXIT=$($r237Cli.ExitCode)"
$r237Source0.WaitForExit()
Write-Output "SOURCE0_EXIT=$($r237Source0.ExitCode)"
$r237Source1.WaitForExit()
Write-Output "SOURCE1_EXIT=$($r237Source1.ExitCode)"
$r237Source2.WaitForExit()
Write-Output "SOURCE2_EXIT=$($r237Source2.ExitCode)"
exit $r237Cli.ExitCode
```


## 指标与诊断

CPU样本=21，均值=1.97022911456136，峰值=4.1797859186325，WS峰值=266903552；drift样本=17，maxAbs raw_phase_ns=2269，不是内容级AV证明。

```text
[2026-10-16 10:53:37.876] [info] [graph][planner.capability] canvas_cuda_stream stage=prepared device_stream=0000000000000000 private_nonblocking=1
[2026-10-16 10:53:37.878] [info] [graph][runtime.node] canvas_cuda_stream stage=first_source device_stream=0000000000000000 readiness=device_stream
[2026-10-16 10:53:37.950] [info] [graph][planner.capability] composition_preparation_storage stage=graph_bound scope=shared_reservations capacity=2147483648 retained=845284223 high_water=871410446
[2026-10-16 10:53:38.149] [info] [graph][runtime.node] mpegts_emission_plan generation=1 mode=canonical_access_unit_window access_unit_window_ns=100000000 video_initial_service_window_ns=13995843 audio_initial_service_window_ns=13995843 packet_size_bytes=188 maximum_payload_bytes=1316 per_datagram_overhead_bytes=12
[2026-10-16 10:53:54.000] [info] [graph][runtime.node] protocol_batch_trace stage=materialization_failed generation=1 attempts=1 consumed_datagrams=0 remaining_datagrams=1 attempted_master_ns=21437032700 mux_dequeued_master_ns=21436989700 first_dequeued_master_ns=21437032000 last_wire_published_master_ns=21399573200 previous_attempt_started_master_ns=unavailable previous_attempt_completed_master_ns=unavailable last_attempt_completed_master_ns=21437098600 last_blocked_started_master_ns=unavailable last_blocked_completed_master_ns=unavailable blocked_attempts=0 remaining_front_release_master_ns=21385668916 remaining_front_deadline_master_ns=21425804655 original_error="InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=21385668916 materialized_ns=21437032700 protocol_deadline_ns=21425804655 deadline_before_release=0 materialized_after_deadline=1"
[2026-10-16 10:53:54.001] [info] [graph][runtime.node] process.failed node=80 name=MediaMpegTsDatagramMaterializerNode kind=MpegTsDatagramMaterializer inputs=[channel=118 edge=118 stream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=120 edge=120 stream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=121 edge=121 stream=Metadata edge_kind=Unknown payload=Unknown queued=2] outputs=[channel=122 edge=122 stream=Metadata edge_kind=Unknown payload=Unknown queued=0] error=InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=21385668916 materialized_ns=21437032700 protocol_deadline_ns=21425804655 deadline_before_release=0 materialized_after_deadline=1
[2026-10-16 10:53:54.001] [info] [graph][runtime.node] worker.failed node=80 kind=MpegTsDatagramMaterializer name=MPEG-TS wire protocol materializer error=InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=21385668916 materialized_ns=21437032700 protocol_deadline_ns=21425804655 deadline_before_release=0 materialized_after_deadline=1
[2026-10-16 10:53:54.002] [info] [graph][runtime.node] process.failed node=22 name=VideoDecodeNode kind=VideoDecode inputs=[channel=22 edge=22 stream=Video edge_kind=Metadata payload=CodecContext queued=0; channel=23 edge=23 stream=Video edge_kind=InputPacket payload=Packet queued=112] outputs=[channel=25 edge=25 stream=Video edge_kind=RawFrame payload=Frame queued=6] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-16 10:53:54.001] [info] [graph][runtime.node] process.failed node=41 name=VideoDecodeNode kind=VideoDecode inputs=[channel=49 edge=49 stream=Video edge_kind=Metadata payload=CodecContext queued=0; channel=50 edge=50 stream=Video edge_kind=InputPacket payload=Packet queued=113] outputs=[channel=52 edge=52 stream=Video edge_kind=RawFrame payload=Frame queued=5] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-16 10:53:54.001] [info] [graph][runtime.node] process.failed node=2 name=VideoEncodeNode kind=VideoEncode inputs=[channel=1 edge=1 stream=Video edge_kind=Metadata payload=CodecContext queued=0; channel=104 edge=104 stream=Video edge_kind=RawFrame payload=Frame queued=2] outputs=[channel=106 edge=106 stream=Video edge_kind=EncodedPacket payload=Packet queued=0; channel=113 edge=113 stream=Video edge_kind=Metadata payload=CodecContext queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-16 10:53:54.001] [info] [graph][runtime.node] process.failed node=26 name=VideoFilterNode kind=VideoFilter inputs=[channel=24 edge=24 stream=Video edge_kind=Metadata payload=CodecContext queued=0; channel=27 edge=27 stream=Video edge_kind=RawFrame payload=Frame queued=5] outputs=[channel=28 edge=28 stream=Video edge_kind=RawFrame payload=Frame queued=6] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-16 10:53:54.002] [info] [graph][runtime.node] process.failed node=64 name=VideoDecodeNode kind=VideoDecode inputs=[channel=84 edge=84 stream=Video edge_kind=Metadata payload=CodecContext queued=0; channel=85 edge=85 stream=Video edge_kind=InputPacket payload=Packet queued=112] outputs=[channel=87 edge=87 stream=Video edge_kind=RawFrame payload=Frame queued=6] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-16 10:53:54.011] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=1703 committed_datagrams=11264 committed_payload_bytes=13657856 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=80004 pacing_reserved=11264 pacing_cancelled=0 pacing_submitted=11264 pacing_maximum_submit_lateness_ns=12779620 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=80004 backlog_max_residence_ns=47132600 last_materialized_sequence=11264 last_scheduled_sequence=11264 last_submitted_sequence=11264 last_committed_sequence=11264 shared_scope_charged_datagrams=11264 shared_scope_charged_wire_bytes=13973248 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2155 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2155 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=379902 endpoint_1_target_effective_socket_bytes=189951 endpoint_1_api_requested_socket_bytes=189951 endpoint_1_effective_socket_bytes=189951 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=11260 endpoint_1_committed_payload_bytes=13657584 endpoint_2_target_effective_socket_bytes=189951 endpoint_2_api_requested_socket_bytes=189951 endpoint_2_effective_socket_bytes=189951 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=4 endpoint_2_committed_payload_bytes=272
[2026-10-16 10:53:54.220] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 datagrams=0 wire_bytes=0 immediate_deadlines=0 deferred_deadlines=0 planned_wait_ns=0 late_datagrams=0 maximum_lateness_ns=0 pending_bytes=0 peak_pending_bytes=110920 pressure_failures=0 access_units=1149 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=83084215 maximum_mux_input_after_emit_ns=110916445 maximum_mux_input_after_dispatch_ns=10916445 worst_mux_input_ready_ns=21436721100 worst_mux_input_emit_ns=21325804655 worst_mux_input_dispatch_ns=21425804655 maximum_protocol_batch_produced_after_release_ns=51411584 worst_protocol_batch_produced_ns=21437080500 worst_protocol_batch_release_ns=21385668916 worst_protocol_batch_deadline_ns=21452335268 worst_protocol_batch_datagrams=11 exit_reason=aborted
[2026-10-16 10:53:54.221] [info] [graph][runtime.node] av_scheduler_correlated_wait generation=1 canonical_sequence=675 missing=video begin_steady_ns=8027762954400 end_steady_ns=8027878818100 observed_wait_ns=115863700 held_dispatch_master_ns=21402584701 begin_master_ns=21283246100 end_master_ns=21399109800 dispatch_remaining_at_end_ns=3474901 resumed_dispatch_master_ns=21419001934
[2026-10-16 10:53:54.223] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=process count=3312 maximum_ns=127704100 begin_steady_ns=8027785952400 end_steady_ns=8027913656500 video_cursor=471 audio_cursor=not_owned next_video_cursor=471 next_audio_cursor=not_owned error=None return_state=progress deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-10-16 10:53:54.223] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=purge count=3312 maximum_ns=466500 begin_steady_ns=8026669314700 end_steady_ns=8026669781200 video_cursor=439 audio_cursor=not_owned next_video_cursor=439 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-16 10:53:54.224] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=bind count=3312 maximum_ns=33700 begin_steady_ns=8025102397200 end_steady_ns=8025102430900 video_cursor=392 audio_cursor=not_owned next_video_cursor=392 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-16 10:53:54.224] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=consume count=3312 maximum_ns=1980100 begin_steady_ns=8018700235300 end_steady_ns=8018702215400 video_cursor=200 audio_cursor=not_owned next_video_cursor=200 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-16 10:53:54.224] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=activate count=32 maximum_ns=276600 begin_steady_ns=8012065376400 end_steady_ns=8012065653000 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-16 10:53:54.224] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=build_video count=476 maximum_ns=127639900 begin_steady_ns=8027785983800 end_steady_ns=8027913623700 video_cursor=471 audio_cursor=not_owned next_video_cursor=471 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-16 10:53:54.224] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=canvas count=476 maximum_ns=127565800 begin_steady_ns=8027786032600 end_steady_ns=8027913598400 video_cursor=471 audio_cursor=not_owned next_video_cursor=471 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-16 10:53:54.224] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=emit count=476 maximum_ns=877200 begin_steady_ns=8012070777100 end_steady_ns=8012071654300 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-16 10:53:54.224] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=between_calls_previous_return count=3311 maximum_ns=146135000 begin_steady_ns=8011914564700 end_steady_ns=8012060699700 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=waiting deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-10-16 10:53:54.224] [info] [graph][runtime.node] cuda_canvas_call stage=context_push count=1910 operation=copy maximum_ns=39600 begin_steady_ns=8018365877200 end_steady_ns=8018365916800 native_result=0 thread_cpu_ns=0
[2026-10-16 10:53:54.224] [info] [graph][runtime.node] cuda_canvas_call stage=source_wait count=1432 operation=copy maximum_ns=90360800 begin_steady_ns=8027822734600 end_steady_ns=8027913095400 native_result=0 thread_cpu_ns=0
[2026-10-16 10:53:54.224] [info] [graph][runtime.node] cuda_canvas_call stage=plane_copy count=3820 operation=copy maximum_ns=1575900 begin_steady_ns=8012068597800 end_steady_ns=8012070173700 native_result=0 thread_cpu_ns=0
[2026-10-16 10:53:54.224] [info] [graph][runtime.node] cuda_canvas_call stage=destination_wait count=1910 operation=copy maximum_ns=45883500 begin_steady_ns=8027739994700 end_steady_ns=8027785878200 native_result=0 thread_cpu_ns=0
[2026-10-16 10:53:54.224] [info] [graph][runtime.node] cuda_canvas_call stage=context_pop count=1910 operation=copy maximum_ns=241500 begin_steady_ns=8012067568800 end_steady_ns=8012067810300 native_result=0 thread_cpu_ns=0
[2026-10-16 10:53:54.225] [info] [graph][runtime.node] canvas_compose_correlated scope=attempt begin_steady_ns=8027786033000 end_steady_ns=8027913597700 wall_ns=127564700 thread_cpu_ns=0 surface_count=1 surface_total_ns=900 surface_maximum_ns=900 surface_maximum_tile=none surface_maximum_result=unavailable validate_count=3 validate_total_ns=1600 validate_maximum_ns=900 validate_maximum_tile=0 validate_maximum_result=0 black_count=1 black_total_ns=36691500 black_maximum_ns=36691500 black_maximum_tile=none black_maximum_result=0 tile_count=3 tile_total_ns=90856200 tile_maximum_ns=90661700 tile_maximum_tile=0 tile_maximum_result=0 publish_count=1 publish_total_ns=10800 publish_maximum_ns=10800 publish_maximum_tile=none publish_maximum_result=0 context_push_count=4 context_push_total_ns=5100 context_push_maximum_ns=2700 context_push_maximum_tile=1 context_push_maximum_result=0 source_wait_count=3 source_wait_total_ns=90364500 source_wait_maximum_ns=90358100 source_wait_maximum_tile=0 source_wait_maximum_result=0 plane_copy_count=8 plane_copy_total_ns=560600 plane_copy_maximum_ns=174600 plane_copy_maximum_tile=0 plane_copy_maximum_result=0 destination_wait_count=4 destination_wait_total_ns=36532700 destination_wait_maximum_ns=36435100 destination_wait_maximum_tile=none destination_wait_maximum_result=0 context_pop_count=4 context_pop_total_ns=13500 context_pop_maximum_ns=6900 context_pop_maximum_tile=none context_pop_maximum_result=0
[2026-10-16 10:53:54.261] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=900 completion_waits=900 completed_datagrams=916 drained_datagrams=17 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-16 10:53:54.262] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=11616 completion_waits=11616 completed_datagrams=18613 drained_datagrams=6997 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-16 10:53:54.262] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=12351 completion_waits=12351 completed_datagrams=18545 drained_datagrams=6195 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-16 10:53:54.263] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=10926 completion_waits=10926 completed_datagrams=18641 drained_datagrams=7716 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[CLI] final runtime report: queued=0, peakQueued=925, threads=82, processThreads=133, workers=0, workerProgress=46380, workerProcessCalls=184738, workerWaits=138346, workerWakeups=137071, workerErrors=1, errors=1, stalledIntervals=0, cpuSamples=50, logicalProcessors=22, averageSystemMachineCpuPercent=15.552779, averageProcessMachineCpuPercent=2.383167, peakProcessMachineCpuPercent=6.106870, averageProcessSingleCoreCpuPercent=52.429671, peakProcessSingleCoreCpuPercent=134.351145, initialWorkingSetBytes=208363520, workingSetBytes=264896512, peakWorkingSetBytes=266903552, totalPushed=41602, totalPopped=40962, droppedBuffers=0, encodedPacketsPushed=5281, encodedPacketsPopped=5278, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=15719875, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=700, graphPayloadReservations=16017, graphPayloadReleases=16017, graphPayloadPressureFailures=1, lastInputReceivedAtNs=8027903169300
[in @ 000001d0ffbe8440] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001d0ffbe8440] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 24742.959456
[in @ 000001d09db4b100] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001d09db4b100] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 16546.344611
[in @ 000001d0a322d8c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001d0a322d8c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 43519.530211
[in @ 000001d0a3684c40] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001d0a3684c40] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 16546.344611
[in @ 000001d09de5cf80] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001d09de5cf80] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 24742.959456
[in @ 000001d0a3681f80] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001d0a3681f80] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 43519.530211
[aac @ 000001d09d8e24c0] Qavg: 507.371
[aac @ 000001d09d8e24c0] 2 frames left in the queue on closing
[CLI] realtime video runtime failed: InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=21385668916 materialized_ns=21437032700 protocol_deadline_ns=21425804655 deadline_before_release=0 materialized_after_deadline=1

```

## 退出与调度补充证据

CLI已自行exit1，本轮失败判定后按精确PID清理三源及VLC；源exit=-1是清理结果，不记为自然结束。主监控会话1330最终exit1。未使用系统截图。

```text
[2026-10-16 10:53:38.164] [info] [graph][runtime.node] av_scheduler_trace stage=first_video_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=5752335268 dispatch_master_ns=5752335268 now_ns=5600923400
[2026-10-16 10:53:38.164] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=5601151500 present_generation=1 present_canonical_target_ns=0 present_canonical_dispatch_ns=0 present_target_master_ns=5752335268 present_dispatch_master_ns=5752335268
[2026-10-16 10:53:38.199] [info] [graph][runtime.node] av_scheduler_trace stage=first_audio_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=5752335268 dispatch_master_ns=5752335268 now_ns=5635538100
[2026-10-16 10:53:53.382] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=20818862900 present_generation=1 present_canonical_target_ns=15185850340 present_canonical_dispatch_ns=15185850340 present_target_master_ns=20938185608 present_dispatch_master_ns=20938185608
[2026-10-16 10:53:53.388] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=20824783600 present_generation=1 present_canonical_target_ns=15200000000 present_canonical_dispatch_ns=15200000000 present_target_master_ns=20952335268 present_dispatch_master_ns=20952335268
[2026-10-16 10:53:53.846] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=21283246600 present_generation=1 present_canonical_target_ns=15650249433 present_canonical_dispatch_ns=15650249433 present_target_master_ns=21402584701 present_dispatch_master_ns=21402584701
[2026-10-16 10:53:53.999] [info] [graph][runtime.node] av_scheduler_trace stage=late_output_commit stream=audio generation=1 canonical_dispatch_ns=15673469387 emit_ns=21325804655 dispatch_ns=21425804655 committed_at_ns=21436310000 head_arrived_ns=21399132800 scheduled_at_ns=21436295800
[2026-10-16 10:53:54.221] [info] [graph][runtime.node] av_scheduler_correlated_wait generation=1 canonical_sequence=675 missing=video begin_steady_ns=8027762954400 end_steady_ns=8027878818100 observed_wait_ns=115863700 held_dispatch_master_ns=21402584701 begin_master_ns=21283246100 end_master_ns=21399109800 dispatch_remaining_at_end_ns=3474901 resumed_dispatch_master_ns=21419001934
```

composition-r237-source0.log
```text
frame= 2694 fps= 30 q=-1.0 size=   88961KiB time=00:01:29.80 bitrate=8115.4kbits/s speed=1.01x elapsed=0:01:29.30    
frame= 2710 fps= 30 q=-1.0 size=   89525KiB time=00:01:30.33 bitrate=8118.7kbits/s speed=1.01x elapsed=0:01:29.82    
frame= 2726 fps= 30 q=-1.0 size=   89956KiB time=00:01:30.86 bitrate=8109.9kbits/s speed=1.01x elapsed=0:01:30.33    
frame= 2741 fps= 30 q=-1.0 size=   90560KiB time=00:01:31.36 bitrate=8119.7kbits/s speed=1.01x elapsed=0:01:30.85    
```

composition-r237-source1.log
```text
frame= 2694 fps= 30 q=-1.0 size=   88961KiB time=00:01:29.80 bitrate=8115.4kbits/s speed=1.01x elapsed=0:01:29.31    
frame= 2710 fps= 30 q=-1.0 size=   89525KiB time=00:01:30.33 bitrate=8118.7kbits/s speed=1.01x elapsed=0:01:29.82    
frame= 2725 fps= 30 q=-1.0 size=   89930KiB time=00:01:30.83 bitrate=8110.5kbits/s speed=1.01x elapsed=0:01:30.34    
frame= 2741 fps= 30 q=-1.0 size=   90560KiB time=00:01:31.36 bitrate=8119.7kbits/s speed=1.01x elapsed=0:01:30.85    
```

composition-r237-source2.log
```text
frame= 2694 fps= 30 q=-1.0 size=   88961KiB time=00:01:29.80 bitrate=8115.4kbits/s speed=1.01x elapsed=0:01:29.31    
frame= 2710 fps= 30 q=-1.0 size=   89525KiB time=00:01:30.33 bitrate=8118.7kbits/s speed=1.01x elapsed=0:01:29.82    
frame= 2726 fps= 30 q=-1.0 size=   89956KiB time=00:01:30.86 bitrate=8109.9kbits/s speed=1.01x elapsed=0:01:30.34    
frame= 2741 fps= 30 q=-1.0 size=   90560KiB time=00:01:31.36 bitrate=8119.7kbits/s speed=1.01x elapsed=0:01:30.85    
```

composition-r237-vlc.log
```text
d3d11va debug: - 'MPEG-4 Part 2 variable-length decoder, Simple&Advanced Profile, no GMC' is supported
d3d11va debug: - 'MPEG-4 Part 2 nVidia bitstream decoder' is supported
d3d11va debug: - 'Unknown decoder 0x33fcfe41-0xde46-0x4a49-0x861bac71110649d5' is supported
d3d11va debug: - 'VP
```

## 同事件关联边界

late_output_commit 的audio canonical_dispatch=15673469387，emit=21325804655、dispatch=21425804655，与mux最坏输入及失败数据报deadline一致；其head到达21399132800，scheduled21436295800，等待37.163ms后才调度，commit21436310000已晚10.505345ms，mux输入21436721100晚10.916445ms。由此定位超期在scheduler提交前已经发生。最长missing-video记录对应前一个audio675、held_dispatch21402584701，不能称为失败audio同一次等待。最长canvas与该等待重叠，但等待恢复早于canvas结束34.7796ms，不能拼接成同帧因果链。下一步应关注失败audio自己的缺失head及生产frontier，不能直接放行音频破坏全局封装顺序。
