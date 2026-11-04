# r281：RTP 固定记录预分配实流复验

状态：功能观察完成；完整门禁未通过；本轮已清理。三路 H.264 720p30 RTP 输入，HEVC 720p30 8Mbps CBR/AAC192kbps separate RTP 输出，与 r280 同规格。冻结版本双独立审查 PASS，Release 全量构建 configure/build 均为0，必需产物齐全。

## 实际启动命令
~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r281-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r281-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r281-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r281-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r281-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r281-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r281 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout separate --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r281-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r281-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r281-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10 -and (Test-Path -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r281-output.sdp')) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r281-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r281-native- --snapshot-format=png file:///D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r281-output.sdp' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r281-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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


~~~

Release SHA256：2F2E7D07293F2FBF769116C4A2994B06CA4709F5E146C36E856200AED720BAA7。
精确 PID：CLI 35968；VLC 33572；初始源 27864、37880、34692；重入源 6400。

## 原生快照（各阶段重复）
~~~powershell
$client=[Net.Sockets.TcpClient]::new('127.0.0.1',63020); try {$stream=$client.GetStream(); $stream.ReadTimeout=2000; $bytes=[Text.Encoding]::ASCII.GetBytes("status`nsnapshot`n"); $stream.Write($bytes,0,$bytes.Length); $buffer=New-Object byte[] 8192; try {while(($count=$stream.Read($buffer,0,$buffer.Length)) -gt 0){[Text.Encoding]::UTF8.GetString($buffer,0,$count)}} catch {Write-Output 'RC read complete'}} finally {$client.Dispose()}
~~~
## 所选音视频源重入
~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r281-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r281-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($source0.Id)"
$source0.WaitForExit()
Write-Output "REENTRY_EXIT=$($source0.ExitCode)"
exit $source0.ExitCode
~~~

## 调用方停止命令
~~~powershell
$ErrorActionPreference='Stop'
$target=Get-CimInstance Win32_Process -Filter "ProcessId=35968"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r281*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R281Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R281Console]::FreeConsole() | Out-Null
if(-not [R281Console]::AttachConsole(35968)){throw 'AttachConsole failed'}
try {
[R281Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R281Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 35968 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R281Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R281Console]::FreeConsole() | Out-Null}


~~~

## 结果与边界
CLI 调用方停止返回0；三路初始源及重入源均完整120秒3600帧退出0。VLC原生快照确认三格→全黑→左下格恢复→全黑。完整门禁未通过：graphPayloadPressureFailures=4，存在 late_output_commit，pacing_maximum_submit_lateness_ns=171836940；发送端deadline_misses=0不证明无调度迟交。delivery_evidence=not_proven，未证明接收端A/V时序。内部phase样本=235，min=15873，max=22675 ns。
~~~text
[2026-10-21 10:16:26.424] [info] [graph][runtime.node] av_scheduler_trace stage=late_video_head generation=1 mapping_epoch_generation=1 canonical_target_ns=198933333333 canonical_dispatch_ns=198933333333 target_master_ns=199605545266 dispatch_master_ns=199605545266 now_ns=199666877500 node=74 identity_kind=output identity="composition-r281.output" canonical_sequence=5969 canonical_created_steady_ns=5570341188900 head_observed_steady_ns=5570341213800 codec_receive_begin_steady_ns=5570341178900 codec_receive_end_steady_ns=5570341180800
[2026-10-21 10:16:26.425] [info] [graph][runtime.node] av_scheduler_trace stage=late_output_commit stream=audio generation=1 canonical_dispatch_ns=198902131519 emit_ns=199474343452 dispatch_ns=199574343452 committed_at_ns=199667150400 canonical_sequence=8567 head_missing_valid=1 head_missing_intervals=1 head_missing_completed_ns=174443300 head_missing_active=0 head_missing_last_begin_ns=199492689000 head_missing_last_end_ns=199667132300 head_resumed_generation=1 head_resumed_sequence=5969 head_arrived_ns=199492665300 scheduled_at_ns=199667136700
[2026-10-21 10:16:26.425] [info] [graph][runtime.node] av_scheduler_trace stage=late_audio_head generation=1 mapping_epoch_generation=1 canonical_target_ns=198925351473 canonical_dispatch_ns=198925351473 target_master_ns=199597563406 dispatch_master_ns=199597563406 now_ns=199667533600 node=74 identity_kind=output identity="composition-r281.output" canonical_sequence=8568 canonical_created_steady_ns=5570189738700 head_observed_steady_ns=5570341869900 codec_receive_begin_steady_ns=unavailable codec_receive_end_steady_ns=unavailable
[2026-10-21 10:16:26.425] [info] [graph][runtime.node] av_scheduler_trace stage=late_output_commit stream=audio generation=1 canonical_dispatch_ns=198925351473 emit_ns=199497563406 dispatch_ns=199597563406 committed_at_ns=199667813600 canonical_sequence=8568 head_missing_valid=1 head_missing_intervals=0 head_missing_completed_ns=0 head_missing_active=0 head_missing_last_begin_ns=unavailable head_missing_last_end_ns=unavailable head_resumed_generation=unavailable head_resumed_sequence=unavailable head_arrived_ns=199667451400 scheduled_at_ns=199667801500
[2026-10-21 10:16:26.426] [info] [graph][runtime.node] av_scheduler_trace stage=late_audio_head generation=1 mapping_epoch_generation=1 canonical_target_ns=198948571428 canonical_dispatch_ns=198948571428 target_master_ns=199620783361 dispatch_master_ns=199620783361 now_ns=199668068700 node=74 identity_kind=output identity="composition-r281.output" canonical_sequence=8569 canonical_created_steady_ns=5570213519800 head_observed_steady_ns=5570342405000 codec_receive_begin_steady_ns=unavailable codec_receive_end_steady_ns=unavailable
[2026-10-21 10:16:26.426] [info] [graph][runtime.node] av_scheduler_trace stage=late_output_commit stream=video generation=1 canonical_dispatch_ns=198933333333 emit_ns=199505545266 dispatch_ns=199605545266 committed_at_ns=199668384000 canonical_sequence=5969 head_missing_valid=1 head_missing_intervals=0 head_missing_completed_ns=0 head_missing_active=0 head_missing_last_begin_ns=unavailable head_missing_last_end_ns=unavailable head_resumed_generation=unavailable head_resumed_sequence=unavailable head_arrived_ns=199666876800 scheduled_at_ns=199668366700
[2026-10-21 10:16:26.426] [info] [graph][runtime.node] av_scheduler_trace stage=late_video_head generation=1 mapping_epoch_generation=1 canonical_target_ns=198966666666 canonical_dispatch_ns=198966666666 target_master_ns=199638878599 dispatch_master_ns=199638878599 now_ns=199668707100 node=74 identity_kind=output identity="composition-r281.output" canonical_sequence=5970 canonical_created_steady_ns=5570342853400 head_observed_steady_ns=5570343043500 codec_receive_begin_steady_ns=5570342846600 codec_receive_end_steady_ns=5570342847500
[2026-10-21 10:16:26.427] [info] [graph][runtime.node] av_scheduler_trace stage=late_output_commit stream=audio generation=1 canonical_dispatch_ns=198948571428 emit_ns=199520783361 dispatch_ns=199620783361 committed_at_ns=199669024500 canonical_sequence=8569 head_missing_valid=1 head_missing_intervals=0 head_missing_completed_ns=0 head_missing_active=0 head_missing_last_begin_ns=unavailable head_missing_last_end_ns=unavailable head_resumed_generation=unavailable head_resumed_sequence=unavailable head_arrived_ns=199667994900 scheduled_at_ns=199669011300
[2026-10-21 10:16:26.427] [info] [graph][runtime.node] av_scheduler_trace stage=late_audio_head generation=1 mapping_epoch_generation=1 canonical_target_ns=198971791383 canonical_dispatch_ns=198971791383 target_master_ns=199644003316 dispatch_master_ns=199644003316 now_ns=199669275800 node=74 identity_kind=output identity="composition-r281.output" canonical_sequence=8570 canonical_created_steady_ns=5570236378500 head_observed_steady_ns=5570343612100 codec_receive_begin_steady_ns=unavailable codec_receive_end_steady_ns=unavailable
[2026-10-21 10:16:26.427] [info] [graph][runtime.node] av_scheduler_trace stage=late_output_commit stream=video generation=1 canonical_dispatch_ns=198966666666 emit_ns=199538878599 dispatch_ns=199638878599 committed_at_ns=199669427900 canonical_sequence=5970 head_missing_valid=1 head_missing_intervals=0 head_missing_completed_ns=0 head_missing_active=0 head_missing_last_begin_ns=unavailable head_missing_last_end_ns=unavailable head_resumed_generation=unavailable head_resumed_sequence=unavailable head_arrived_ns=199668623400 scheduled_at_ns=199669416700
[2026-10-21 10:16:26.428] [info] [graph][runtime.node] av_scheduler_trace stage=late_output_commit stream=audio generation=1 canonical_dispatch_ns=198971791383 emit_ns=199544003316 dispatch_ns=199644003316 committed_at_ns=199670157000 canonical_sequence=8570 head_missing_valid=1 head_missing_intervals=1 head_missing_completed_ns=580100 head_missing_active=0 head_missing_last_begin_ns=199669567100 head_missing_last_end_ns=199670147200 head_resumed_generation=1 head_resumed_sequence=5971 head_arrived_ns=199669224900 scheduled_at_ns=199670148400
[2026-10-21 10:16:26.428] [info] [graph][runtime.node] av_scheduler_trace stage=late_audio_head generation=1 mapping_epoch_generation=1 canonical_target_ns=198995011337 canonical_dispatch_ns=198995011337 target_master_ns=199667223270 dispatch_master_ns=199667223270 now_ns=199670351700 node=74 identity_kind=output identity="composition-r281.output" canonical_sequence=8571 canonical_created_steady_ns=5570259342300 head_observed_steady_ns=5570344688000 codec_receive_begin_steady_ns=unavailable codec_receive_end_steady_ns=unavailable
[2026-10-21 10:16:26.428] [info] [graph][runtime.node] av_scheduler_trace stage=late_output_commit stream=audio generation=1 canonical_dispatch_ns=198995011337 emit_ns=199567223270 dispatch_ns=199667223270 committed_at_ns=199670486500 canonical_sequence=8571 head_missing_valid=1 head_missing_intervals=0 head_missing_completed_ns=0 head_missing_active=0 head_missing_last_begin_ns=unavailable head_missing_last_end_ns=unavailable head_resumed_generation=unavailable head_resumed_sequence=unavailable head_arrived_ns=199670321100 scheduled_at_ns=199670476900
[2026-10-21 10:18:05.297] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=21779 committed_datagrams=157577 committed_payload_bytes=200836434 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=53 ordered_queue_maximum_wire_bytes=78146 pacing_reserved=157577 pacing_cancelled=0 pacing_submitted=157577 pacing_maximum_submit_lateness_ns=171836940 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=64 backlog_high_water_wire_bytes=93587 backlog_max_residence_ns=59181800 last_materialized_sequence=157577 last_scheduled_sequence=157577 last_submitted_sequence=157577 last_committed_sequence=157577 shared_scope_charged_datagrams=157577 shared_scope_charged_wire_bytes=205248590 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=3806 evidence_timestamp_tracked=0 evidence_timestamp_untracked=3806 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=756580 endpoint_1_target_effective_socket_bytes=189145 endpoint_1_api_requested_socket_bytes=189145 endpoint_1_effective_socket_bytes=189145 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=144592 endpoint_1_committed_payload_bytes=194866453 endpoint_2_target_effective_socket_bytes=189145 endpoint_2_api_requested_socket_bytes=189145 endpoint_2_effective_socket_bytes=189145 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=73 endpoint_2_committed_payload_bytes=4972 endpoint_3_target_effective_socket_bytes=189145 endpoint_3_api_requested_socket_bytes=189145 endpoint_3_effective_socket_bytes=189145 endpoint_3_timestamp_source=0 endpoint_3_committed_datagrams=12833 endpoint_3_committed_payload_bytes=5959629 endpoint_4_target_effective_socket_bytes=189145 endpoint_4_api_requested_socket_bytes=189145 endpoint_4_effective_socket_bytes=189145 endpoint_4_timestamp_source=0 endpoint_4_committed_datagrams=79 endpoint_4_committed_payload_bytes=5380
[2026-10-21 10:18:05.609] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=10345 completion_waits=10345 completed_datagrams=10484 drained_datagrams=166 would_block=29 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-21 10:18:05.610] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=133075 completion_waits=133075 completed_datagrams=212845 drained_datagrams=79796 would_block=29 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-21 10:18:05.610] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=72840 completion_waits=72840 completed_datagrams=106111 drained_datagrams=33360 would_block=89 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-21 10:18:05.611] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=74957 completion_waits=74957 completed_datagrams=106180 drained_datagrams=31313 would_block=89 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=397, threads=80, processThreads=128, workers=0, workerProgress=493412, workerProcessCalls=1971263, workerWaits=1477835, workerWakeups=1456012, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=948, logicalProcessors=22, averageSystemMachineCpuPercent=9.814469, averageProcessMachineCpuPercent=1.585295, peakProcessMachineCpuPercent=5.719921, averageProcessSingleCoreCpuPercent=34.876496, peakProcessSingleCoreCpuPercent=125.838264, initialWorkingSetBytes=191012864, workingSetBytes=246358016, peakWorkingSetBytes=248643584, totalPushed=455394, totalPopped=455358, droppedBuffers=0, encodedPacketsPushed=78158, encodedPacketsPopped=78158, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=4744646, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=202, graphPayloadReservations=259844, graphPayloadReleases=259844, graphPayloadPressureFailures=4, lastInputReceivedAtNs=5639874865200
~~~
### 首次压力计数上下文
~~~text
[2026-10-21 10:16:26.428] [info] [graph][runtime.node] av_scheduler_trace stage=late_audio_head generation=1 mapping_epoch_generation=1 canonical_target_ns=198995011337 canonical_dispatch_ns=198995011337 target_master_ns=199667223270 dispatch_master_ns=199667223270 now_ns=199670351700 node=74 identity_kind=output identity="composition-r281.output" canonical_sequence=8571 canonical_created_steady_ns=5570259342300 head_observed_steady_ns=5570344688000 codec_receive_begin_steady_ns=unavailable codec_receive_end_steady_ns=unavailable
[2026-10-21 10:16:26.428] [info] [graph][runtime.node] av_scheduler_trace stage=late_output_commit stream=audio generation=1 canonical_dispatch_ns=198995011337 emit_ns=199567223270 dispatch_ns=199667223270 committed_at_ns=199670486500 canonical_sequence=8571 head_missing_valid=1 head_missing_intervals=0 head_missing_completed_ns=0 head_missing_active=0 head_missing_last_begin_ns=unavailable head_missing_last_end_ns=unavailable head_resumed_generation=unavailable head_resumed_sequence=unavailable head_arrived_ns=199670321100 scheduled_at_ns=199670476900
[2026-10-21 10:16:26.431] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=199673169300 present_generation=1 present_canonical_target_ns=199066666666 present_canonical_dispatch_ns=199066666666 present_target_master_ns=199738878599 present_dispatch_master_ns=199738878599
[2026-10-21 10:16:26.436] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2129 source_master_ns=200283060212 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2221954
[CLI] runtime report: queued=23, peakQueued=393, threads=80, processThreads=128, workers=74, workerProgress=379573, workerProcessCalls=1537953, workerWaits=1158374, workerWakeups=1143752, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=631, logicalProcessors=22, averageSystemMachineCpuPercent=10.173944, averageProcessMachineCpuPercent=1.707937, peakProcessMachineCpuPercent=5.719921, averageProcessSingleCoreCpuPercent=37.574625, peakProcessSingleCoreCpuPercent=125.838264, initialWorkingSetBytes=191012864, workingSetBytes=246054912, peakWorkingSetBytes=248643584, totalPushed=351700, totalPopped=351653, droppedBuffers=0, encodedPacketsPushed=52257, encodedPacketsPopped=52256, backpressureItems=123, graphPayloadCurrentBytes=290573, graphPayloadHighWaterBytes=4744646, graphPayloadCurrentObjects=54, graphPayloadHighWaterObjects=202, graphPayloadReservations=193448, graphPayloadReleases=193394, graphPayloadPressureFailures=4, lastInputReceivedAtNs=5570473857100
[2026-10-21 10:16:26.634] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=1552601095 ntp_seconds=4001537786 ntp_fraction=2723009265 arrival_ns=5570551186800
[2026-10-21 10:16:26.635] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=1552601095
[2026-10-21 10:16:26.635] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
~~~
### stderr
~~~text
[in @ 0000020b50120740] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000020b50120740] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 29693.375922
[in @ 0000020b59a05800] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000020b59a05800] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 21728.972933
[in @ 0000020b504a2180] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000020b504a2180] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 45719.189689
[in @ 0000020b594e39c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000020b594e2f40] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000020b594e4580] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000020b594e39c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 21728.972933
[in @ 0000020b594e2f40] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 45719.189689
[in @ 0000020b594e4580] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 29693.375922
[aac @ 0000020b59ba21c0] Qavg: 13859.349
~~~
### 源结束
~~~text
source0
[out#1/rtp @ 000001d983b378c0] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.49    
source1
[out#0/rtp @ 00000182894f8a00] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.47    
source2
[out#0/rtp @ 0000021ffc2c8c80] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
reentry
[out#1/rtp @ 0000025bb512b700] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
~~~
### 清理清单
composition-r281-cli-error.log
composition-r281-cli.log
composition-r281-monitor.log
composition-r281-native-2026-10-21-10h13m45s625.png
composition-r281-native-2026-10-21-10h15m23s869.png
composition-r281-native-2026-10-21-10h16m01s475.png
composition-r281-native-2026-10-21-10h17m43s668.png
composition-r281-output.sdp
composition-r281-reentry-stdout.log
composition-r281-reentry.log
composition-r281-source0-stdout.log
composition-r281-source0.log
composition-r281-source1-stdout.log
composition-r281-source1.log
composition-r281-source2-stdout.log
composition-r281-source2.log
composition-r281-vlc.log

### VLC与监控诊断
~~~text
main error: buffer deadlock prevented
main error: Failed to create video converter
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r281-native-2026-10-21-10h13m45s625.png)
main error: Failed to create video converter
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r281-native-2026-10-21-10h15m23s869.png)
main error: Failed to create video converter
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r281-native-2026-10-21-10h16m01s475.png)
main error: Failed to create video converter
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r281-native-2026-10-21-10h17m43s668.png)
direct3d11 error: SetThumbNailClip failed: 0x800706f4
-- logger module stopped --
seconds=1.0216968 cpu=3.0586373569928 ws=237006848 private=1038065664
seconds=298.8437163 cpu=0.834901515863219 ws=246026240 private=1050263552
~~~
压力计数源码边界：MediaGraphPayloadCreditLedger.cpp 的 reserve 路径在 planner hard bound 下成功挂入 waiter 后增加 pressureFailures 并返回 WouldBlock；因此4次计数不等同4次终止错误，也不证明发生丢帧。当前聚合遥测不能唯一定位 account，保留其与迟交日志的时间关联，未宣称二者因果或 r281 根因。

## 清理核验
六个精确PID查询均不存在。17个清单文件已逐项删除，本轮前缀残留0，指定120秒测试源保留。
