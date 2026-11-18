# r321：提交服务事实接线原规格复验

三路120秒 H264/AAC RTP→HEVC CBR 8000kbps 1280×720/30、AAC CBR 192kbps 44100Hz stereo→MPEG-TS/UDP 62024。仅 realtime、VLC 原生快照。下列命令已实际执行，反馈事实核对一致；不计合屏完整验收通过。

核对ledger服务观察generation/sequence与最终实际提交一致，available_ns=completed_ns+reservation duration；它仅表示sender局部服务观察，不代表共享scope、OS调度或NIC出线保证。保持发布/提交计数对账及退出后payload回收检查。

## 启动命令

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r321-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r321-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r321-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r321-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r321-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r321-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r321 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r321-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r321-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r321-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r321-native- --snapshot-format=png udp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r321-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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
~~~~

## 结果

Release全量745项构建成功（会话85901，configure/build exit0）；四项必需产物核验，realtime CLI 2026-10-23 15:19:03。两名独立审查者对反馈及诊断切片均明确静态PASS。

源PID40488/32516/16224均跑满120秒自然退出0；CLI30120约164秒后调用方Ctrl+C退出0，监控会话11713；VLC43916通过RC quit退出。两张VLC原生快照实际查看：15:25:26三格视频+空黑格，15:27:17全失源黑屏。持续合屏不因源EOF自动退出；本轮未重做AV重入。

确认新事实：generation=1、service sequence=86110，与last_submitted/last_committed及总提交量86110一致；163525754400ns+34560ns=163525788960ns，与available_ns完全一致。该观察来自真实pacing reservation及提交，尚未接入scope/planner，不证明服务债务全闭环。

mux/sender均86110 datagrams与98593592 TS payload bytes；sender期限/压力/partial/ambiguous错误均0，workerErrors/errors=0。最终graphPayloadCurrentBytes/Objects=0，reservations/releases均147687，高水位17130182 bytes。内部机器CPU均值2.201850%、峰值5.487805%，WS峰值274272256 bytes；末次外部WS249237504、private1138569216 bytes。内部phase约2269ns，不能代替声音/内容A/V证明；delivery_evidence仍not_proven。

本轮完成sender反馈切片运行验证，不计合屏完整门禁通过。共享scope、planner消费、完整周期、owner预算、声音/静音、内容A/V、NIC时序及剩余矩阵仍未完成。

## 快照及停止命令

~~~~powershell
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 43916){throw 'VLC RC PID mismatch'}; $c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020); $s=$c.GetStream(); $b=[Text.Encoding]::ASCII.GetBytes("snapshot`n"); $s.Write($b,0,$b.Length); $s.Dispose(); $c.Dispose()
~~~~

VLC退出使用相同RC命令，将snapshot替换为quit。

~~~~powershell
$target=Get-CimInstance Win32_Process -Filter "ProcessId=30120"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r321*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R321Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R321Console]::FreeConsole() | Out-Null
if(-not [R321Console]::AttachConsole(30120)){throw 'AttachConsole failed'}
try {
[R321Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R321Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 30120 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R321Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R321Console]::FreeConsole() | Out-Null}

~~~~

## 诊断与逐项清理清单

以下记录在清理前写入；保留复用源，本轮未启动远程测试。

~~~~text
[2026-10-23 15:27:31.261] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 produced_datagrams=86110 produced_ts_payload_bytes=98593592 pending_bytes=0 peak_pending_bytes=110920 access_units=11543 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=32789115 maximum_mux_input_after_emit_ns=36996978 maximum_mux_input_after_dispatch_ns=-63003022 worst_mux_input_ready_ns=5620366400 worst_mux_input_emit_ns=5583369422 worst_mux_input_dispatch_ns=5683369422 maximum_protocol_batch_produced_after_release_ns=33329472 worst_protocol_batch_produced_ns=33593479500 worst_protocol_batch_release_ns=33560150028 worst_protocol_batch_deadline_ns=33660149468 worst_protocol_batch_datagrams=1 exit_reason=completed
[2026-10-23 15:27:31.262] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=86110 committed_datagrams=86110 committed_payload_bytes=98593592 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=1 ordered_queue_maximum_wire_bytes=1344 pacing_reserved=86110 pacing_cancelled=0 pacing_submitted=86110 pacing_maximum_submit_lateness_ns=7890820 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=1 backlog_high_water_wire_bytes=1344 backlog_max_residence_ns=25049800 last_materialized_sequence=86110 last_scheduled_sequence=86110 last_submitted_sequence=86110 last_committed_sequence=86110 submitted_service_generation=1 submitted_service_sequence=86110 submitted_service_completed_ns=163525754400 submitted_service_duration_ns=34560 submitted_service_available_ns=163525788960 shared_scope_charged_datagrams=86110 shared_scope_charged_wire_bytes=101004672 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2154 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2154 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=188010 endpoint_1_target_effective_socket_bytes=188010 endpoint_1_api_requested_socket_bytes=188010 endpoint_1_effective_socket_bytes=188010 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=86110 endpoint_1_committed_payload_bytes=98593592
[CLI] final runtime report: queued=0, peakQueued=946, threads=81, processThreads=129, workers=0, workerProgress=566573, workerProcessCalls=1941479, workerWaits=1374889, workerWakeups=1362835, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=468, logicalProcessors=22, averageSystemMachineCpuPercent=11.584963, averageProcessMachineCpuPercent=2.201850, peakProcessMachineCpuPercent=5.487805, averageProcessSingleCoreCpuPercent=48.440690, peakProcessSingleCoreCpuPercent=120.731707, initialWorkingSetBytes=211738624, workingSetBytes=248401920, peakWorkingSetBytes=274272256, totalPushed=455295, totalPopped=455271, droppedBuffers=0, encodedPacketsPushed=52982, encodedPacketsPopped=52982, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=17130182, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=747, graphPayloadReservations=147687, graphPayloadReleases=147687, graphPayloadPressureFailures=0, lastInputReceivedAtNs=23966070364700
[2026-10-23 15:24:53.228] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1 source_master_ns=5662139924 raw_phase_ns=2269 filtered_phase_ns=2269 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-10-23 15:26:46.961] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4919 source_master_ns=119857876884 raw_phase_ns=2267 filtered_phase_ns=2269 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5078752
seconds=162.9559467 cpu=1.36637847755128 ws=249237504 private=1138569216
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r321-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r321-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r321-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r321-native-2026-10-23-15h25m26s238.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r321-native-2026-10-23-15h27m17s244.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r321-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r321-source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r321-source1-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r321-source1.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r321-source2-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r321-source2.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r321-vlc.log

~~~~

清理核验：上述12项本轮产物已逐项删除，前缀残留0，提升权限核验五个精确PID剩余0；120秒源保留。未提交、推送、合并或部署。
