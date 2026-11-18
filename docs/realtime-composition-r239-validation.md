# r239：保证准备额度实流复验

## 清理

记录后按清单删除12件临时产物；5个PID及本轮前缀无残留，指定源124427809字节保留，无远程测试。VLC按身份核对强制清理，不记自然退出。

```powershell
$proc=Get-CimInstance Win32_Process -Filter 'ProcessId=6056';if($proc){if($proc.Name -ne 'vlc.exe' -or $proc.CommandLine -notlike '*composition-r239*'){throw 'VLC identity mismatch'};Stop-Process -Id 6056 -Force}
$root=[IO.Path]::GetFullPath('D:/Code/MyCode/MediaTranscode/out/acceptance')+[IO.Path]::DirectorySeparatorChar
$names=@('composition-r239-cli-error.log','composition-r239-cli.log','composition-r239-monitor.log','composition-r239-output.sdp','composition-r239-source0-stdout.log','composition-r239-source0.log','composition-r239-source1-stdout.log','composition-r239-source1.log','composition-r239-source2-stdout.log','composition-r239-source2.log','composition-r239-vlc.log','composition-r239-native-2026-10-16-11h17m51s995.png')
foreach($name in $names){$path=[IO.Path]::GetFullPath((Join-Path $root $name));if(-not $path.StartsWith($root,[StringComparison]::OrdinalIgnoreCase)){throw 'Cleanup escaped acceptance directory'};if(Test-Path -LiteralPath $path){Remove-Item -LiteralPath $path -Force;Write-Output "REMOVED $name"}}
Get-ChildItem -LiteralPath $root -Filter 'composition-r239-*' | Select-Object Name
Get-CimInstance Win32_Process | Where-Object {$_.ProcessId -in @(6056,29740,12888,21024,35072)} | Select-Object ProcessId,Name
Get-Item -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4' | Select-Object Length
```

## 结果：FAIL

Release SHA256 64F5E6F02C0C502EF6C39445251FE04468BB7B834A04201EE4795A905942F265。VLC6056，源29740/12888/21024，CLI35072。约99秒CLI自行exit1，未进入失源/重入。VLC 11:17:51.995 status play3、snapshot0，实际查看三格视频右下黑。

准备建图capacity2147483648、retained832927224、highWater850377827（包含保证额度，不是活跃像素字节）。物化10次尝试、9次阻塞；最后阻塞明确serial=1/count=0/bytes=0，request1包228B，used14包18984B，limit2155包2920356B，head10598、first69865、count55、committed41，projected69920。最后阻塞时是串行门控而非额度耗尽；不能推定此前8次条件相同。原deadline保持，失败attempt超期4.243904ms。下一步调查门控规划依据及前一reservation提交/唤醒，不直接扩大容量或关闭门控。

## 实际启动监控命令

```powershell
$ErrorActionPreference='Stop'
$r239Vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r239-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r239-native- --snapshot-format=png rtp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($r239Vlc.Id)"
$r239Source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r239-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r239-source0.log' -PassThru
Write-Output "SOURCE0_PID=$($r239Source0.Id)"
$r239Source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r239-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r239-source1.log' -PassThru
Write-Output "SOURCE1_PID=$($r239Source1.Id)"
$r239Source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r239-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r239-source2.log' -PassThru
Write-Output "SOURCE2_PID=$($r239Source2.Id)"
$r239Cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r239 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r239-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r239-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r239-cli-error.log' -PassThru
Write-Output "CLI_PID=$($r239Cli.Id)"
$r239Clock=[Diagnostics.Stopwatch]::StartNew()
$r239LastTime=0.0
$r239LastCpu=0.0
$r239Cores=[Environment]::ProcessorCount
while(-not $r239Cli.WaitForExit(1000)) {
$r239Cli.Refresh()
$r239Time=$r239Clock.Elapsed.TotalSeconds
$r239Cpu=$r239Cli.TotalProcessorTime.TotalSeconds
$r239Usage=100*($r239Cpu-$r239LastCpu)/(($r239Time-$r239LastTime)*$r239Cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r239-monitor.log' -Value "seconds=$r239Time cpu=$r239Usage ws=$($r239Cli.WorkingSet64) private=$($r239Cli.PrivateMemorySize64)"
$r239LastTime=$r239Time
$r239LastCpu=$r239Cpu
}
Write-Output "CLI_EXIT=$($r239Cli.ExitCode)"
$r239Source0.WaitForExit()
Write-Output "SOURCE0_EXIT=$($r239Source0.ExitCode)"
$r239Source1.WaitForExit()
Write-Output "SOURCE1_EXIT=$($r239Source1.ExitCode)"
$r239Source2.WaitForExit()
Write-Output "SOURCE2_EXIT=$($r239Source2.ExitCode)"
exit $r239Cli.ExitCode
```

## VLC状态与原生快照

```powershell
$client=[Net.Sockets.TcpClient]::new('127.0.0.1',63020);try{$stream=$client.GetStream();$stream.ReadTimeout=2000;$bytes=[Text.Encoding]::ASCII.GetBytes("status`r`nsnapshot`r`n");$stream.Write($bytes,0,$bytes.Length);$buffer=[byte[]]::new(4096);try{while(($n=$stream.Read($buffer,0,4096))-gt 0){[Text.Encoding]::UTF8.GetString($buffer,0,$n)}}catch [IO.IOException]{}}finally{$client.Dispose()}
Get-ChildItem -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance' -Filter 'composition-r239-native-*.png' | Select-Object -ExpandProperty FullName
```


## 指标与诊断

CPU样本=96，均值=1.97646427145645，峰值=3.45002342218142，WS峰值=249569280；drift样本=97，maxAbs raw_phase_ns=22675，非内容AV证明。
```text
[2026-10-16 11:17:28.064] [info] [graph][planner.capability] canvas_cuda_stream stage=prepared device_stream=0000000000000000 private_nonblocking=1
[2026-10-16 11:17:28.065] [info] [graph][runtime.node] canvas_cuda_stream stage=first_source device_stream=0000000000000000 readiness=device_stream
[2026-10-16 11:17:28.102] [info] [graph][planner.capability] composition_preparation_storage stage=graph_bound scope=shared_reservations capacity=2147483648 retained=832927224 high_water=850377827
[2026-10-16 11:17:28.210] [info] [graph][runtime.node] mpegts_emission_plan generation=1 mode=canonical_access_unit_window access_unit_window_ns=100000000 video_initial_service_window_ns=13995843 audio_initial_service_window_ns=13995843 packet_size_bytes=188 maximum_payload_bytes=1316 per_datagram_overhead_bytes=12
[2026-10-16 11:17:28.220] [info] [graph][runtime.node] av_scheduler_trace stage=first_video_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=683672868 dispatch_master_ns=683672868 now_ns=527223600
[2026-10-16 11:17:28.220] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=527391200 present_generation=1 present_canonical_target_ns=0 present_canonical_dispatch_ns=0 present_target_master_ns=683672868 present_dispatch_master_ns=683672868
[2026-10-16 11:17:28.257] [info] [graph][runtime.node] av_scheduler_trace stage=first_audio_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=683672868 dispatch_master_ns=683672868 now_ns=564294300
[2026-10-16 11:17:36.663] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=8970148400 present_generation=1 present_canonical_target_ns=8405623582 present_canonical_dispatch_ns=8405623582 present_target_master_ns=9089296450 present_dispatch_master_ns=9089296450
[2026-10-16 11:17:36.667] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=8974068700 present_generation=1 present_canonical_target_ns=8433333333 present_canonical_dispatch_ns=8433333333 present_target_master_ns=9117006201 present_dispatch_master_ns=9117006201
[2026-10-16 11:18:18.832] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=51139176800 present_generation=1 present_canonical_target_ns=50573061224 present_canonical_dispatch_ns=50573061224 present_target_master_ns=51256734092 present_dispatch_master_ns=51256734092
[2026-10-16 11:18:18.844] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=51150458700 present_generation=1 present_canonical_target_ns=50600000000 present_canonical_dispatch_ns=50600000000 present_target_master_ns=51283672868 present_dispatch_master_ns=51283672868
[2026-10-16 11:18:34.736] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=67043132300 present_generation=1 present_canonical_target_ns=66478730158 present_canonical_dispatch_ns=66478730158 present_target_master_ns=67162403026 present_dispatch_master_ns=67162403026
[2026-10-16 11:18:34.752] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=67058870700 present_generation=1 present_canonical_target_ns=66500000000 present_canonical_dispatch_ns=66500000000 present_target_master_ns=67183672868 present_dispatch_master_ns=67183672868
[2026-10-16 11:18:48.993] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=81300083200 present_generation=1 present_canonical_target_ns=80735782312 present_canonical_dispatch_ns=80735782312 present_target_master_ns=81419455180 present_dispatch_master_ns=81419455180
[2026-10-16 11:18:48.998] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=81304467600 present_generation=1 present_canonical_target_ns=80766666666 present_canonical_dispatch_ns=80766666666 present_target_master_ns=81450339534 present_dispatch_master_ns=81450339534
[2026-10-16 11:19:04.133] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=96439557300 present_generation=1 present_canonical_target_ns=95875192743 present_canonical_dispatch_ns=95875192743 present_target_master_ns=96558865611 present_dispatch_master_ns=96558865611
[2026-10-16 11:19:04.137] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=96443690900 present_generation=1 present_canonical_target_ns=95900000000 present_canonical_dispatch_ns=95900000000 present_target_master_ns=96583672868 present_dispatch_master_ns=96583672868
[2026-10-16 11:19:06.348] [info] [graph][runtime.node] protocol_batch_trace stage=materialization_failed generation=1 attempts=10 consumed_datagrams=0 remaining_datagrams=1 attempted_master_ns=98654585400 mux_dequeued_master_ns=98576110700 first_dequeued_master_ns=98576248700 last_wire_published_master_ns=98576243500 previous_attempt_started_master_ns=98645881800 previous_attempt_completed_master_ns=98645886900 last_attempt_completed_master_ns=98654606300 last_blocked_started_master_ns=98645881800 last_blocked_completed_master_ns=98645886900 blocked_attempts=9 last_blocked_code=WouldBlock last_blocked_native=0 last_blocked_original_message_bytes=293 last_blocked_message_truncated=0 last_blocked_message="wire global sequence reservation exceeds planner backlog capacity serial=1 count=0 bytes=0 request_count=1 request_bytes=228 used_count=14 used_bytes=18984 limit_count=2155 limit_bytes=2920356 generation=1 projected_sequence=69920 head_id=10598 head_first=69865 head_count=55 head_committed=41" remaining_front_release_master_ns=98583674828 remaining_front_deadline_master_ns=98650341496 original_error="InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=98583674828 materialized_ns=98654585400 protocol_deadline_ns=98650341496 deadline_before_release=0 materialized_after_deadline=1"
[2026-10-16 11:19:06.348] [info] [graph][runtime.node] process.failed node=80 name=MediaMpegTsDatagramMaterializerNode kind=MpegTsDatagramMaterializer inputs=[channel=118 edge=118 stream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=120 edge=120 stream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=121 edge=121 stream=Metadata edge_kind=Unknown payload=Unknown queued=9] outputs=[channel=122 edge=122 stream=Metadata edge_kind=Unknown payload=Unknown queued=0] error=InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=98583674828 materialized_ns=98654585400 protocol_deadline_ns=98650341496 deadline_before_release=0 materialized_after_deadline=1
[2026-10-16 11:19:06.349] [info] [graph][runtime.node] worker.failed node=80 kind=MpegTsDatagramMaterializer name=MPEG-TS wire protocol materializer error=InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=98583674828 materialized_ns=98654585400 protocol_deadline_ns=98650341496 deadline_before_release=0 materialized_after_deadline=1
[2026-10-16 11:19:06.352] [info] [graph][runtime.node] process.failed node=2 name=VideoEncodeNode kind=VideoEncode inputs=[channel=1 edge=1 stream=Video edge_kind=Metadata payload=CodecContext queued=0; channel=104 edge=104 stream=Video edge_kind=RawFrame payload=Frame queued=0] outputs=[channel=106 edge=106 stream=Video edge_kind=EncodedPacket payload=Packet queued=0; channel=113 edge=113 stream=Video edge_kind=Metadata payload=CodecContext queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-16 11:19:06.355] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=10602 committed_datagrams=69919 committed_payload_bytes=84764588 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=80004 pacing_reserved=69919 pacing_cancelled=0 pacing_submitted=69919 pacing_maximum_submit_lateness_ns=16609440 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=80004 backlog_max_residence_ns=78432500 last_materialized_sequence=69919 last_scheduled_sequence=69919 last_submitted_sequence=69919 last_committed_sequence=69919 shared_scope_charged_datagrams=69919 shared_scope_charged_wire_bytes=86722320 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2155 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2155 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=379902 endpoint_1_target_effective_socket_bytes=189951 endpoint_1_api_requested_socket_bytes=189951 endpoint_1_effective_socket_bytes=189951 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=69897 endpoint_1_committed_payload_bytes=84763092 endpoint_2_target_effective_socket_bytes=189951 endpoint_2_api_requested_socket_bytes=189951 endpoint_2_effective_socket_bytes=189951 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=22 endpoint_2_committed_payload_bytes=1496
[2026-10-16 11:19:06.350] [info] [graph][runtime.node] process.failed node=64 name=VideoDecodeNode kind=VideoDecode inputs=[channel=84 edge=84 stream=Video edge_kind=Metadata payload=CodecContext queued=0; channel=85 edge=85 stream=Video edge_kind=InputPacket payload=Packet queued=0] outputs=[channel=87 edge=87 stream=Video edge_kind=RawFrame payload=Frame queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-16 11:19:06.352] [info] [graph][runtime.node] process.failed node=41 name=VideoDecodeNode kind=VideoDecode inputs=[channel=49 edge=49 stream=Video edge_kind=Metadata payload=CodecContext queued=0; channel=50 edge=50 stream=Video edge_kind=InputPacket payload=Packet queued=0] outputs=[channel=52 edge=52 stream=Video edge_kind=RawFrame payload=Frame queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-16 11:19:06.351] [info] [graph][runtime.node] process.failed node=22 name=VideoDecodeNode kind=VideoDecode inputs=[channel=22 edge=22 stream=Video edge_kind=Metadata payload=CodecContext queued=0; channel=23 edge=23 stream=Video edge_kind=InputPacket payload=Packet queued=0] outputs=[channel=25 edge=25 stream=Video edge_kind=RawFrame payload=Frame queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-16 11:19:06.598] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 datagrams=0 wire_bytes=0 immediate_deadlines=0 deferred_deadlines=0 planned_wait_ns=0 late_datagrams=0 maximum_lateness_ns=0 pending_bytes=0 peak_pending_bytes=92872 pressure_failures=0 access_units=7168 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=1955 maximum_mux_input_after_emit_ns=8462666 maximum_mux_input_after_dispatch_ns=-91537334 worst_mux_input_ready_ns=91058802200 worst_mux_input_emit_ns=91050339534 worst_mux_input_dispatch_ns=91150339534 maximum_protocol_batch_produced_after_release_ns=8539556 worst_protocol_batch_produced_ns=91058880900 worst_protocol_batch_release_ns=91050341344 worst_protocol_batch_deadline_ns=91150339534 worst_protocol_batch_datagrams=22 exit_reason=aborted
[2026-10-16 11:19:06.599] [info] [graph][runtime.node] av_scheduler_correlated_wait generation=1 canonical_sequence=2556 missing=audio begin_steady_ns=9527323032300 end_steady_ns=9527362994400 observed_wait_ns=39962100 held_dispatch_master_ns=85850339534 begin_master_ns=85713339000 end_master_ns=85753300600 dispatch_remaining_at_end_ns=97038934 resumed_dispatch_master_ns=85854466518
[2026-10-16 11:19:06.602] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=process count=18213 maximum_ns=34420700 begin_steady_ns=9508627229200 end_steady_ns=9508661649900 video_cursor=1995 audio_cursor=not_owned next_video_cursor=1995 next_audio_cursor=not_owned error=None return_state=progress deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-10-16 11:19:06.602] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=purge count=18213 maximum_ns=207400 begin_steady_ns=9476727721900 end_steady_ns=9476727929300 video_cursor=1038 audio_cursor=not_owned next_video_cursor=1038 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-16 11:19:06.602] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=bind count=18213 maximum_ns=227100 begin_steady_ns=9487393516700 end_steady_ns=9487393743800 video_cursor=1358 audio_cursor=not_owned next_video_cursor=1358 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-16 11:19:06.602] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=consume count=18213 maximum_ns=8482700 begin_steady_ns=9499514017300 end_steady_ns=9499522500000 video_cursor=1722 audio_cursor=not_owned next_video_cursor=1722 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-16 11:19:06.602] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=activate count=22 maximum_ns=17300 begin_steady_ns=9442126699000 end_steady_ns=9442126716300 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-16 11:19:06.603] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=build_video count=2945 maximum_ns=34365600 begin_steady_ns=9508627261600 end_steady_ns=9508661627200 video_cursor=1995 audio_cursor=not_owned next_video_cursor=1995 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-16 11:19:06.603] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=canvas count=2945 maximum_ns=34287900 begin_steady_ns=9508627319500 end_steady_ns=9508661607400 video_cursor=1995 audio_cursor=not_owned next_video_cursor=1995 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-16 11:19:06.603] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=emit count=2945 maximum_ns=8312100 begin_steady_ns=9503133163800 end_steady_ns=9503141475900 video_cursor=1830 audio_cursor=not_owned next_video_cursor=1830 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-16 11:19:06.603] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=between_calls_previous_return count=18212 maximum_ns=83366600 begin_steady_ns=9442041468800 end_steady_ns=9442124835400 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=waiting deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-10-16 11:19:06.603] [info] [graph][runtime.node] cuda_canvas_call stage=context_push count=11786 operation=copy maximum_ns=484900 begin_steady_ns=9508560618700 end_steady_ns=9508561103600 native_result=0 thread_cpu_ns=0
[2026-10-16 11:19:06.603] [info] [graph][runtime.node] cuda_canvas_call stage=source_wait count=8839 operation=copy maximum_ns=3792800 begin_steady_ns=9505934722500 end_steady_ns=9505938515300 native_result=0 thread_cpu_ns=0
[2026-10-16 11:19:06.603] [info] [graph][runtime.node] cuda_canvas_call stage=plane_copy count=23572 operation=copy maximum_ns=22614700 begin_steady_ns=9508627336500 end_steady_ns=9508649951200 native_result=0 thread_cpu_ns=0
[2026-10-16 11:19:06.603] [info] [graph][runtime.node] cuda_canvas_call stage=destination_wait count=11786 operation=copy maximum_ns=14246200 begin_steady_ns=9499601000200 end_steady_ns=9499615246400 native_result=0 thread_cpu_ns=0
[2026-10-16 11:19:06.604] [info] [graph][runtime.node] cuda_canvas_call stage=context_pop count=11786 operation=copy maximum_ns=1104600 begin_steady_ns=9458398277200 end_steady_ns=9458399381800 native_result=0 thread_cpu_ns=0
[2026-10-16 11:19:06.604] [info] [graph][runtime.node] canvas_compose_correlated scope=attempt begin_steady_ns=9508627320500 end_steady_ns=9508661606600 wall_ns=34286100 thread_cpu_ns=0 surface_count=1 surface_total_ns=1600 surface_maximum_ns=1600 surface_maximum_tile=none surface_maximum_result=unavailable validate_count=3 validate_total_ns=2700 validate_maximum_ns=1500 validate_maximum_tile=0 validate_maximum_result=0 black_count=1 black_total_ns=23624400 black_maximum_ns=23624400 black_maximum_tile=none black_maximum_result=0 tile_count=3 tile_total_ns=10645600 tile_maximum_ns=5034000 tile_maximum_tile=1 tile_maximum_result=0 publish_count=1 publish_total_ns=6500 publish_maximum_ns=6500 publish_maximum_tile=none publish_maximum_result=0 context_push_count=4 context_push_total_ns=7000 context_push_maximum_ns=5200 context_push_maximum_tile=none context_push_maximum_result=0 source_wait_count=3 source_wait_total_ns=137400 source_wait_maximum_ns=128700 source_wait_maximum_tile=2 source_wait_maximum_result=0 plane_copy_count=8 plane_copy_total_ns=22879200 plane_copy_maximum_ns=22612000 plane_copy_maximum_tile=none plane_copy_maximum_result=0 destination_wait_count=4 destination_wait_total_ns=11055700 destination_wait_maximum_ns=4957200 destination_wait_maximum_tile=1 destination_wait_maximum_result=0 context_pop_count=4 context_pop_total_ns=136100 context_pop_maximum_ns=119700 context_pop_maximum_tile=2 context_pop_maximum_result=0
[2026-10-16 11:19:06.639] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=4276 completion_waits=4276 completed_datagrams=4319 drained_datagrams=42 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-16 11:19:06.639] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=56628 completion_waits=56628 completed_datagrams=87714 drained_datagrams=31087 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-16 11:19:06.640] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=60863 completion_waits=60863 completed_datagrams=87732 drained_datagrams=26868 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-16 11:19:06.641] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=66330 completion_waits=66330 completed_datagrams=87740 drained_datagrams=21411 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[CLI] final runtime report: queued=0, peakQueued=384, threads=82, processThreads=130, workers=0, workerProgress=248308, workerProcessCalls=1058975, workerWaits=810656, workerWakeups=803088, workerErrors=1, errors=1, stalledIntervals=0, cpuSamples=310, logicalProcessors=22, averageSystemMachineCpuPercent=11.686697, averageProcessMachineCpuPercent=1.971004, peakProcessMachineCpuPercent=6.053269, averageProcessSingleCoreCpuPercent=43.362077, peakProcessSingleCoreCpuPercent=133.171913, initialWorkingSetBytes=190345216, workingSetBytes=248791040, peakWorkingSetBytes=248791040, totalPushed=228905, totalPopped=228803, droppedBuffers=0, encodedPacketsPushed=32898, encodedPacketsPopped=32898, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=4858621, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=173, graphPayloadReservations=91732, graphPayloadReleases=91732, graphPayloadPressureFailures=0, lastInputReceivedAtNs=9540264715800
[in @ 000002aa0aab4700] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002aa0aab4700] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 23521.881933
[in @ 000002aa0ab51a00] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002aa0ab51a00] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 37499.142589
[in @ 000002aa0af42e00] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002aa0af42e00] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 31002.520056
[in @ 000002aa0a7ca8c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002aa0afd5a40] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002aa0afd4840] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002aa0a7ca8c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 31002.520056
[in @ 000002aa0afd5a40] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 37499.142589
[in @ 000002aa0afd4840] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 23521.881933
[aac @ 000002aa0aba4dc0] Qavg: 911.038
[aac @ 000002aa0aba4dc0] 2 frames left in the queue on closing
[CLI] realtime video runtime failed: InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=98583674828 materialized_ns=98654585400 protocol_deadline_ns=98650341496 deadline_before_release=0 materialized_after_deadline=1

```

结束：主会话25783 exit1，三个源自然exit0、均120秒3600帧。CLI失败清理不记自然drain通过。

composition-r239-source0.log
```text
[out#0/rtp @ 0000010a0bcf6480] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
[out#1/rtp @ 0000010a0bd8b300] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
```

composition-r239-source1.log
```text
frame= 3591 fps= 30 q=-1.0 size=  118561KiB time=00:01:59.70 bitrate=8114.0kbits/s speed=   1x elapsed=0:01:59.19    
[out#0/rtp @ 000001b90e66ddc0] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.47    
```

composition-r239-source2.log
```text
frame= 3591 fps= 30 q=-1.0 size=  118561KiB time=00:01:59.70 bitrate=8114.0kbits/s speed=   1x elapsed=0:01:59.20    
[out#0/rtp @ 000001614cba7500] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.47    
```

VLC告警
```text
faad warning: decoded zero sample
d3d11va warning: surface dimensions (1280x768) differ from avcodec dimensions (1280x736)
d3d11va warning: not enough decoding slices in the texture (6/28)
main warning: playback too late (73847): up-sampling
main warning: picture is too late to be displayed (missing 30 ms)
main error: Failed to create video converter
```
