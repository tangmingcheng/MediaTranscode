# r248：画布批处理 Release realtime 验证

## 状态
已运行，完整门禁未通过。原规格三路 H264 720p30 120秒→HEVC 720p30 CBR8000kbps/AAC192kbps MPEG-TS/RTP，输入等待100/150/200ms、wire residence100ms、egress50Mbps、准备存储2GiB。VLC仅使用原生快照。构建、双审查完成后执行；失败不计通过。

## 启动及监控命令
```powershell
$ErrorActionPreference='Stop'
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r248-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r248-native- --snapshot-format=png rtp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r248-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r248-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r248-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r248-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r248-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r248-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r248 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r248-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r248-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r248-cli-error.log' -PassThru
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
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r248-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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
Get-ChildItem -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance' -Filter 'composition-r248-native-*.png' | Select-Object -ExpandProperty FullName
```

## 重入命令（首阶段未失败时）
```powershell
$source=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r248-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r248-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($source.Id)"
$source.WaitForExit()
Write-Output "REENTRY_EXIT=$($source.ExitCode)"
exit $source.ExitCode
```


## 实际结果：限定运行证据，完整门禁未通过
Release全量构建22670退出0，717项；cache/Ninja和两CLI存在，未运行local。realtime6073856字节，15:13:19，SHA2565360F388524FCC2AD12E32C616177DEEE07C4062F4D80C660432202FC14D73FB。双独立源码审查限定PASS。
VLC36116，初始源41624/572/14280均完整120秒3600帧自然退出0；重入37920完整120秒3600帧自然退出0；CLI8972 caller_stop退出0，非源失活自然退出。最终errors0/workers0/queued0/payload0，预约释放217870相等，encoded pushed/popped90128相等。
四张VLC原生PNG均实际查看，RC status/snapshot均0：15:14:15三格；15:16:07全黑；15:16:38仅左下重入；15:18:18仍左下视频，此图距源BYE仅约0.37秒，不能当第二次失源全黑证明。停止前虽有约13秒继续运行，未再截图；本轮不补造该证据。
运行时CPU平均1.823259%、峰值7.627119%；WS初始210132992、峰值267436032、最终259923968B。发送172999包，deadline_misses0/would_block0，最大pacing lateness45.395742ms、队列驻留65.7692ms。内部数据不等于内容A/V或线端交付证明。
canvas最大79.4058ms；对应批完成79.2303ms，source_wait8.3us；合批确实仅一次context push/pop和batch completion。两次运行非受控同负载比较，不能宣称189→79ms为必然性能收益，也不能宣称历史迟到消失。仍有颜色range变化警告。完整矩阵、内容A/V/静音、C API/ABI、非cadence/HEVC输入重排和Windows后RKMPP未完成，无成功提交。

## 调用方停止命令
```powershell
$target=Get-CimInstance Win32_Process -Filter "ProcessId=8972"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r248*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R248Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R248Console]::FreeConsole() | Out-Null
if(-not [R248Console]::AttachConsole(8972)){throw 'AttachConsole failed'}
try {
[R248Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R248Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 8972 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R248Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R248Console]::FreeConsole() | Out-Null}
```
## 清理命令
```powershell
$p=Get-CimInstance Win32_Process -Filter 'ProcessId=36116'
if($p -and $p.Name -eq 'vlc.exe' -and $p.CommandLine -like '*composition-r248*'){Stop-Process -Id 36116 -Force}
$root=[IO.Path]::GetFullPath('D:/Code/MyCode/MediaTranscode/out/acceptance')
$names=@('composition-r248-cli-error.log','composition-r248-cli.log','composition-r248-monitor.log','composition-r248-native-2026-10-16-15h14m15s410.png','composition-r248-native-2026-10-16-15h16m07s704.png','composition-r248-native-2026-10-16-15h16m38s839.png','composition-r248-native-2026-10-16-15h18m18s344.png','composition-r248-output.sdp','composition-r248-reentry-stdout.log','composition-r248-reentry.log','composition-r248-source0-stdout.log','composition-r248-source0.log','composition-r248-source1-stdout.log','composition-r248-source1.log','composition-r248-source2-stdout.log','composition-r248-source2.log','composition-r248-vlc.log')
foreach($name in $names){$path=[IO.Path]::GetFullPath((Join-Path $root $name));if(-not $path.StartsWith($root+[IO.Path]::DirectorySeparatorChar)){throw 'Outside test directory'};if(Test-Path -LiteralPath $path){Remove-Item -LiteralPath $path -Force}}
Get-ChildItem -LiteralPath $root -Filter 'composition-r248-*'
Get-CimInstance Win32_Process | Where-Object {$_.ProcessId -in @(36116,41624,572,14280,8972,37920)} | Select-Object ProcessId,Name
(Get-Item -LiteralPath (Join-Path $root 'test-continuous-120s.mp4')).Length
```

## 原始关键指标
[2026-10-16 15:18:31.183] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 datagrams=0 wire_bytes=0 immediate_deadlines=0 deferred_deadlines=0 planned_wait_ns=0 late_datagrams=0 maximum_lateness_ns=0 pending_bytes=0 peak_pending_bytes=94564 pressure_failures=0 access_units=19637 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=26349206 maximum_mux_input_after_emit_ns=45240142 maximum_mux_input_after_dispatch_ns=-54759858 worst_mux_input_ready_ns=161114408600 worst_mux_input_emit_ns=161069168458 worst_mux_input_dispatch_ns=161169168458 maximum_protocol_batch_produced_after_release_ns=45263342 worst_protocol_batch_produced_ns=161114431800 worst_protocol_batch_release_ns=161069168458 worst_protocol_batch_deadline_ns=161169168458 worst_protocol_batch_datagrams=1 exit_reason=completed
[2026-10-16 15:18:31.211] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=29057 committed_datagrams=172999 committed_payload_bytes=206163636 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=80004 pacing_reserved=172999 pacing_cancelled=0 pacing_submitted=172999 pacing_maximum_submit_lateness_ns=45395742 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=80004 backlog_max_residence_ns=65769200 last_materialized_sequence=172999 last_scheduled_sequence=172999 last_submitted_sequence=172999 last_committed_sequence=172999 shared_scope_charged_datagrams=172999 shared_scope_charged_wire_bytes=211007608 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2155 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2155 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=379902 endpoint_1_target_effective_socket_bytes=189951 endpoint_1_api_requested_socket_bytes=189951 endpoint_1_effective_socket_bytes=189951 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=172930 endpoint_1_committed_payload_bytes=206158936 endpoint_2_target_effective_socket_bytes=189951 endpoint_2_api_requested_socket_bytes=189951 endpoint_2_effective_socket_bytes=189951 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=69 endpoint_2_committed_payload_bytes=4700
[2026-10-16 15:18:31.430] [info] [graph][runtime.node] continuous_audio_aggregate.summary reason=stop output_group=composition-r248.output audio_commits=11574 real_samples=10252288 silence_samples=1599488
[2026-10-16 15:18:31.431] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=canvas count=8063 maximum_ns=79405800 begin_steady_ns=23870917527400 end_steady_ns=23870996933200 video_cursor=7003 audio_cursor=not_owned next_video_cursor=7003 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-16 15:18:31.431] [info] [graph][runtime.node] continuous_aggregate.summary reason=stop output_group=composition-r248.output output_generation=1 video_commits=8063 source[0].group=composition-r248.source.0 source[0].real_frames=3421 source[0].black_frames=4642 source[0].contribution_generation=0 source[1].group=composition-r248.source.1 source[1].real_frames=3422 source[1].black_frames=4641 source[1].contribution_generation=0 source[2].group=composition-r248.source.2 source[2].real_frames=6973 source[2].black_frames=1090 source[2].contribution_generation=0
[2026-10-16 15:18:31.431] [info] [graph][runtime.node] cuda_canvas_call stage=batch_completion count=6976 operation=copy maximum_ns=79234200 begin_steady_ns=23870917678800 end_steady_ns=23870996913000 native_result=0 thread_cpu_ns=0
[2026-10-16 15:18:31.431] [info] [graph][runtime.node] canvas_compose_correlated scope=attempt begin_steady_ns=23870917528300 end_steady_ns=23870996932100 wall_ns=79403800 thread_cpu_ns=0 surface_count=1 surface_total_ns=900 surface_maximum_ns=900 surface_maximum_tile=none surface_maximum_result=unavailable validate_count=1 validate_total_ns=1600 validate_maximum_ns=1600 validate_maximum_tile=2 validate_maximum_result=0 black_count=1 black_total_ns=103000 black_maximum_ns=103000 black_maximum_tile=none black_maximum_result=0 tile_count=1 tile_total_ns=33900 tile_maximum_ns=33900 tile_maximum_tile=2 tile_maximum_result=0 publish_count=1 publish_total_ns=8600 publish_maximum_ns=8600 publish_maximum_tile=none publish_maximum_result=0 context_push_count=1 context_push_total_ns=5200 context_push_maximum_ns=5200 context_push_maximum_tile=none context_push_maximum_result=0 source_wait_count=1 source_wait_total_ns=8300 source_wait_maximum_ns=8300 source_wait_maximum_tile=2 source_wait_maximum_result=0 plane_copy_count=4 plane_copy_total_ns=122600 plane_copy_maximum_ns=79900 plane_copy_maximum_tile=none plane_copy_maximum_result=0 destination_wait_count=0 destination_wait_total_ns=0 destination_wait_maximum_ns=0 destination_wait_maximum_tile=none destination_wait_maximum_result=unavailable context_pop_count=1 context_pop_total_ns=5800 context_pop_maximum_ns=5800 context_pop_maximum_tile=none context_pop_maximum_result=0 batch_completion_count=1 batch_completion_total_ns=79230300 batch_completion_maximum_ns=79230300 batch_completion_maximum_tile=none batch_completion_maximum_result=0
[CLI] final runtime report: queued=0, peakQueued=977, threads=82, processThreads=130, workers=0, workerProgress=531419, workerProcessCalls=2059745, workerWaits=1528308, workerWakeups=1507740, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=847, logicalProcessors=22, averageSystemMachineCpuPercent=10.503339, averageProcessMachineCpuPercent=1.823259, peakProcessMachineCpuPercent=7.627119, averageProcessSingleCoreCpuPercent=40.111697, peakProcessSingleCoreCpuPercent=167.796610, initialWorkingSetBytes=210132992, workingSetBytes=259923968, peakWorkingSetBytes=267436032, totalPushed=475503, totalPopped=475467, droppedBuffers=0, encodedPacketsPushed=90128, encodedPacketsPopped=90128, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=18311818, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=733, graphPayloadReservations=217870, graphPayloadReleases=217870, graphPayloadPressureFailures=0, lastInputReceivedAtNs=23893048053100
漂移绝对值统计ns：

Count             : 230
Average           : 
Sum               : 
Maximum           : 20409
Minimum           : 
StandardDeviation : 
Property          : 


清理复核：上述17个文件逐项删除，本轮前缀和六个PID均无残留；原源124427809B保留。无远程运行/产物。VLC为清理结束。
