# r320：mux 发布统计修复后的原规格复验

三路120秒 H264/AAC RTP→HEVC CBR 8000kbps 1280×720/30、AAC CBR 192kbps 44100Hz stereo→MPEG-TS/UDP 62024。仅 realtime、VLC 原生快照。下列命令已实际执行，统计对账一致；不计合屏完整验收通过。

核对正常排空时 produced_datagrams 与 sender committed_datagrams、produced_ts_payload_bytes 与 UDP sender committed_payload_bytes；TS/RTP不直接套用payload字节等式，须分离RTP/RTCP开销。保留实际退出、CPU、内存与phase记录。

## 启动命令

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r320-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r320-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r320-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r320-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r320-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r320-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r320 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r320-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r320-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r320-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r320-native- --snapshot-format=png udp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r320-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

## 实际结果

Release全量745项重建成功（会话74315），configure/build exit0，四项必需产物核验；realtime CLI为2026-10-23 15:03:36。两名独立审查者对统计切片均明确静态PASS。

实际源PID41448/9508/36868，均120秒自然退出0；CLI41660，监控会话56845，约150秒后调用方Ctrl+C退出0（非源EOF自动退出）；VLC22704通过RC quit退出。原生快照15:04:31三格视频+右下空黑格、15:06:09全失源黑屏，均实际查看；本轮未重做AV重入。

mux produced_datagrams与sender committed_datagrams均84612，produced_ts_payload_bytes与UDP sender committed_payload_bytes均98319112；两项精确一致。sender deadline_misses/pressure_failures=0，workerErrors/errors=0；payload最终0，reservations/releases均144365，高水位17382863 bytes。进程机器CPU均值2.404300%、峰值6.25%；WS峰值273543168 bytes，末值247746560 bytes；末次private1130188800 bytes。内部phase约15874ns，不等价内容级A/V测量。delivery_evidence仍not_proven，不能证明NIC时序。

本轮只确认统计修复与原规格三源播放/失源/停止证据；声音/静音、内容A/V、出线时序、周期与债务闭环及完整矩阵仍未完成，不计合屏完整验收PASS。

## VLC快照与调用方停止命令

~~~~powershell
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 22704){throw 'VLC RC PID mismatch'}; $c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020); $s=$c.GetStream(); $b=[Text.Encoding]::ASCII.GetBytes("snapshot`n"); $s.Write($b,0,$b.Length); $s.Dispose(); $c.Dispose()
~~~~

VLC退出使用相同RC命令，将snapshot替换为quit。

~~~~powershell
$target=Get-CimInstance Win32_Process -Filter "ProcessId=41660"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r320*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R320Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R320Console]::FreeConsole() | Out-Null
if(-not [R320Console]::AttachConsole(41660)){throw 'AttachConsole failed'}
try {
[R320Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R320Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 41660 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R320Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R320Console]::FreeConsole() | Out-Null}

~~~~

## 关键指标与逐项清理清单

以下记录在清理前写入；120秒源保留，本轮未启动远程测试。

~~~~text
[2026-10-23 15:06:22.304] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 produced_datagrams=84612 produced_ts_payload_bytes=98319112 pending_bytes=0 peak_pending_bytes=110920 access_units=10531 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=32789115 maximum_mux_input_after_emit_ns=57758000 maximum_mux_input_after_dispatch_ns=-42242000 worst_mux_input_ready_ns=29278526500 worst_mux_input_emit_ns=29220768500 worst_mux_input_dispatch_ns=29320768500 maximum_protocol_batch_produced_after_release_ns=51651768 worst_protocol_batch_produced_ns=75501174500 worst_protocol_batch_release_ns=75449522732 worst_protocol_batch_deadline_ns=75549521334 worst_protocol_batch_datagrams=1 exit_reason=completed
[2026-10-23 15:06:22.305] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=84612 committed_datagrams=84612 committed_payload_bytes=98319112 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=1 ordered_queue_maximum_wire_bytes=1344 pacing_reserved=84612 pacing_cancelled=0 pacing_submitted=84612 pacing_maximum_submit_lateness_ns=29547460 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=1 backlog_high_water_wire_bytes=1344 backlog_max_residence_ns=29794700 last_materialized_sequence=84612 last_scheduled_sequence=84612 last_submitted_sequence=84612 last_committed_sequence=84612 shared_scope_charged_datagrams=84612 shared_scope_charged_wire_bytes=100688248 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2154 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2154 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=188010 endpoint_1_target_effective_socket_bytes=188010 endpoint_1_api_requested_socket_bytes=188010 endpoint_1_effective_socket_bytes=188010 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=84612 endpoint_1_committed_payload_bytes=98319112
[CLI] final runtime report: queued=0, peakQueued=925, threads=81, processThreads=129, workers=0, workerProgress=556376, workerProcessCalls=1949191, workerWaits=1392798, workerWakeups=1381794, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=448, logicalProcessors=22, averageSystemMachineCpuPercent=15.467459, averageProcessMachineCpuPercent=2.404300, peakProcessMachineCpuPercent=6.250000, averageProcessSingleCoreCpuPercent=52.894610, peakProcessSingleCoreCpuPercent=137.500000, initialWorkingSetBytes=210628608, workingSetBytes=247746560, peakWorkingSetBytes=273543168, totalPushed=446977, totalPopped=446953, droppedBuffers=0, encodedPacketsPushed=48337, encodedPacketsPopped=48337, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=17382863, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=748, graphPayloadReservations=144365, graphPayloadReleases=144365, graphPayloadPressureFailures=0, lastInputReceivedAtNs=22710979474200
[2026-10-23 15:03:58.133] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1 source_master_ns=5683720854 raw_phase_ns=15874 filtered_phase_ns=15874 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-10-23 15:05:51.865] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4919 source_master_ns=119879457814 raw_phase_ns=15873 filtered_phase_ns=15874 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5078752
seconds=149.5238329 cpu=1.30626131849306 ws=247746560 private=1130188800
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r320-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r320-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r320-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r320-native-2026-10-23-15h04m31s672.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r320-native-2026-10-23-15h06m09s216.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r320-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r320-source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r320-source1-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r320-source1.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r320-source2-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r320-source2.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r320-vlc.log

~~~~

清理完成：上述12项本轮产物逐项删除，前缀残留0；提升权限核验五个精确PID剩余0。复用源保留。未提交、推送、合并或部署。
