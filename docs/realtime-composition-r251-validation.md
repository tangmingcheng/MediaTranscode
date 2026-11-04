# r251：共享源分配器绑定后 Release realtime 复验

状态：已运行，完整门禁未通过。沿r249原规格三源120秒H264720p30→HEVC720p30 CBR8000kbps/AAC192kbps MPEGTS/RTP，2GiB准备预算、逐源100/150/200ms、wire100ms、egress50Mbps。仅realtime。该修改是readiness前置，不宣称事件已接入。

## 启动命令
```powershell
$ErrorActionPreference='Stop'
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r251-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r251-native- --snapshot-format=png rtp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r251-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r251-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r251-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r251-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r251-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r251-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r251 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r251-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r251-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r251-cli-error.log' -PassThru
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
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r251-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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
Get-ChildItem -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance' -Filter 'composition-r251-native-*.png' | Select-Object -ExpandProperty FullName
```
## 重入命令
```powershell
$source=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r251-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r251-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($source.Id)"
$source.WaitForExit()
Write-Output "REENTRY_EXIT=$($source.ExitCode)"
exit $source.ExitCode
```


## 本轮结果：限定运行与视觉证据通过，完整门禁未通过
Release clean-first all-target78406退出0，cache/Ninja及两CLI存在；realtime6073856B，15:44:22，SHA256 EE99A8B800807CC48DADAFE0E95F398451B2EEC16555E22245593A3828D786A8。三文件前置源码双独立审查PASS，未运行local。
VLC41852，初始源35860/35548/35508均120秒3600帧自然exit0；重入41788同样120秒3600帧exit0；CLI6724 caller_stop退出0，非失源自然退出。
四张VLC原生PNG均实际查看，RC status/snapshot均0：15:45:27三格；15:47:27全黑；15:47:56仅左下重入；15:49:55再次全黑（已等源结束和播放器缓冲）。没有系统截图。
最终errors0/workers0/queued0/payload0，预约释放227159相等，encoded103924推入弹出相等。CPU均1.355112%、峰5.847953%；WS初始207089664、峰268935168、最终259698688B。sender176998包，deadline_misses0/would_block0；pacing最大迟12.32182ms、最大队列驻留49.4373ms。
canvas最大56.4342ms，对应源等待合计54.1703ms、批完成1.6557ms。仅此轮未复现迟到，不能推断前置PMR绑定修复r249。实际replay decoder=h264（三源）、same_device=1；canvas source/device stream均NULL，私有nonblocking=1。必须保留此后端事实供readiness设计，不能从codec名字猜cuvid。
颜色range变化警告仍保留。未证明内容级A/V、静音及线端交付；四源HEVC输入重排、非cadence、API/ABI、Windows完整后RKMPP仍未完成；没有成功提交/推送。
## 调用方停止
```powershell
$target=Get-CimInstance Win32_Process -Filter "ProcessId=6724"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r251*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R251Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R251Console]::FreeConsole() | Out-Null
if(-not [R251Console]::AttachConsole(6724)){throw 'AttachConsole failed'}
try {
[R251Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R251Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 6724 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R251Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R251Console]::FreeConsole() | Out-Null}
```
## 清理命令
```powershell
$p=Get-CimInstance Win32_Process -Filter 'ProcessId=41852'
if($p -and $p.Name -eq 'vlc.exe' -and $p.CommandLine -like '*composition-r251*'){Stop-Process -Id 41852 -Force}
$root=[IO.Path]::GetFullPath('D:/Code/MyCode/MediaTranscode/out/acceptance')
$names=@('composition-r251-cli-error.log','composition-r251-cli.log','composition-r251-monitor.log','composition-r251-native-2026-10-16-15h45m27s424.png','composition-r251-native-2026-10-16-15h47m27s104.png','composition-r251-native-2026-10-16-15h47m56s076.png','composition-r251-native-2026-10-16-15h49m55s744.png','composition-r251-output.sdp','composition-r251-reentry-stdout.log','composition-r251-reentry.log','composition-r251-source0-stdout.log','composition-r251-source0.log','composition-r251-source1-stdout.log','composition-r251-source1.log','composition-r251-source2-stdout.log','composition-r251-source2.log','composition-r251-vlc.log')
foreach($name in $names){$path=[IO.Path]::GetFullPath((Join-Path $root $name));if(-not $path.StartsWith($root+[IO.Path]::DirectorySeparatorChar)){throw 'Outside test directory'};if(Test-Path -LiteralPath $path){Remove-Item -LiteralPath $path -Force}}
Get-ChildItem -LiteralPath $root -Filter 'composition-r251-*'
Get-CimInstance Win32_Process | Where-Object {$_.ProcessId -in @(41852,35860,35548,35508,6724,41788)} | Select-Object ProcessId,Name
(Get-Item -LiteralPath (Join-Path $root 'test-continuous-120s.mp4')).Length
```

## 原始关键指标
[2026-10-16 15:45:07.854] [info] [graph][planner.capability] canvas_cuda_stream stage=prepared device_stream=0000000000000000 private_nonblocking=1
[2026-10-16 15:45:07.856] [info] [graph][runtime.node] canvas_cuda_stream stage=first_source device_stream=0000000000000000 readiness=device_stream
[2026-10-16 15:45:07.863] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-10-16 15:45:07.868] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-10-16 15:45:07.874] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-10-16 15:50:17.956] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 datagrams=0 wire_bytes=0 immediate_deadlines=0 deferred_deadlines=0 planned_wait_ns=0 late_datagrams=0 maximum_lateness_ns=0 pending_bytes=0 peak_pending_bytes=110920 pressure_failures=0 access_units=22643 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=27709750 maximum_mux_input_after_emit_ns=39048250 maximum_mux_input_after_dispatch_ns=-60951750 worst_mux_input_ready_ns=13911826900 worst_mux_input_emit_ns=13872778650 worst_mux_input_dispatch_ns=13972778650 maximum_protocol_batch_produced_after_release_ns=23911786 worst_protocol_batch_produced_ns=13966350300 worst_protocol_batch_release_ns=13942438514 worst_protocol_batch_deadline_ns=14042438514 worst_protocol_batch_datagrams=1 exit_reason=completed
[2026-10-16 15:50:17.984] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=33510 committed_datagrams=176998 committed_payload_bytes=206423984 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=80004 pacing_reserved=176998 pacing_cancelled=0 pacing_submitted=176998 pacing_maximum_submit_lateness_ns=12321820 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=80004 backlog_max_residence_ns=49437300 last_materialized_sequence=176998 last_scheduled_sequence=176998 last_submitted_sequence=176998 last_committed_sequence=176998 shared_scope_charged_datagrams=176998 shared_scope_charged_wire_bytes=211379928 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2155 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2155 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=379902 endpoint_1_target_effective_socket_bytes=189951 endpoint_1_api_requested_socket_bytes=189951 endpoint_1_effective_socket_bytes=189951 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=176917 endpoint_1_committed_payload_bytes=206418468 endpoint_2_target_effective_socket_bytes=189951 endpoint_2_api_requested_socket_bytes=189951 endpoint_2_effective_socket_bytes=189951 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=81 endpoint_2_committed_payload_bytes=5516
[2026-10-16 15:50:18.209] [info] [graph][runtime.node] continuous_audio_aggregate.summary reason=stop output_group=composition-r251.output audio_commits=13346 real_samples=10256384 silence_samples=3409920
[2026-10-16 15:50:18.209] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=canvas count=9297 maximum_ns=56434200 begin_steady_ns=25511603291700 end_steady_ns=25511659725900 video_cursor=254 audio_cursor=not_owned next_video_cursor=254 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-16 15:50:18.209] [info] [graph][runtime.node] continuous_aggregate.summary reason=stop output_group=composition-r251.output output_generation=1 video_commits=9297 source[0].group=composition-r251.source.0 source[0].real_frames=3423 source[0].black_frames=5874 source[0].contribution_generation=0 source[1].group=composition-r251.source.1 source[1].real_frames=3423 source[1].black_frames=5874 source[1].contribution_generation=0 source[2].group=composition-r251.source.2 source[2].real_frames=6977 source[2].black_frames=2320 source[2].contribution_generation=0
[2026-10-16 15:50:18.210] [info] [graph][runtime.node] canvas_compose_correlated scope=attempt begin_steady_ns=25511603292400 end_steady_ns=25511659725000 wall_ns=56432600 thread_cpu_ns=0 surface_count=1 surface_total_ns=900 surface_maximum_ns=900 surface_maximum_tile=none surface_maximum_result=unavailable validate_count=3 validate_total_ns=1800 validate_maximum_ns=900 validate_maximum_tile=0 validate_maximum_result=0 black_count=1 black_total_ns=296600 black_maximum_ns=296600 black_maximum_tile=none black_maximum_result=0 tile_count=3 tile_total_ns=54435600 tile_maximum_ns=48296500 tile_maximum_tile=1 tile_maximum_result=0 publish_count=1 publish_total_ns=15500 publish_maximum_ns=15500 publish_maximum_tile=none publish_maximum_result=0 context_push_count=1 context_push_total_ns=1300 context_push_maximum_ns=1300 context_push_maximum_tile=none context_push_maximum_result=0 source_wait_count=3 source_wait_total_ns=54170300 source_wait_maximum_ns=48197600 source_wait_maximum_tile=1 source_wait_maximum_result=0 plane_copy_count=8 plane_copy_total_ns=534600 plane_copy_maximum_ns=198100 plane_copy_maximum_tile=none plane_copy_maximum_result=0 destination_wait_count=0 destination_wait_total_ns=0 destination_wait_maximum_ns=0 destination_wait_maximum_tile=none destination_wait_maximum_result=unavailable context_pop_count=1 context_pop_total_ns=10800 context_pop_maximum_ns=10800 context_pop_maximum_tile=none context_pop_maximum_result=0 batch_completion_count=1 batch_completion_total_ns=1655700 batch_completion_maximum_ns=1655700 batch_completion_maximum_tile=none batch_completion_maximum_result=0
[CLI] final runtime report: queued=0, peakQueued=911, threads=82, processThreads=131, workers=0, workerProgress=561084, workerProcessCalls=2119107, workerWaits=1558005, workerWakeups=1534307, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=1034, logicalProcessors=22, averageSystemMachineCpuPercent=10.640572, averageProcessMachineCpuPercent=1.355112, peakProcessMachineCpuPercent=5.847953, averageProcessSingleCoreCpuPercent=29.812468, peakProcessSingleCoreCpuPercent=128.654971, initialWorkingSetBytes=207089664, workingSetBytes=259698688, peakWorkingSetBytes=268935168, totalPushed=500521, totalPopped=500485, droppedBuffers=0, encodedPacketsPushed=103924, encodedPacketsPopped=103924, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=15490819, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=669, graphPayloadReservations=227159, graphPayloadReleases=227159, graphPayloadPressureFailures=0, lastInputReceivedAtNs=25764325303900
漂移绝对值统计ns：

Count             : 230
Average           : 
Sum               : 
Maximum           : 13606
Minimum           : 
StandardDeviation : 
Property          : 


清理复核：17个清单产物逐项删除，前缀和六PID无残留；原源124427809B保留，无远程运行或产物。VLC为测试结束清理。
