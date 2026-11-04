# r322：共享服务域观察复验

三路120秒 H264/AAC RTP→HEVC CBR 8000kbps 1280×720/30、AAC CBR 192kbps 44100Hz stereo→MPEG-TS/UDP 62024。仅 realtime，VLC 原生快照。已执行下列命令并完成切片核对，不计整体合屏验收通过。

核验 scope 观察与 sender 提交事实同次保存、实际发布/提交数量及资源回收；不证明完整周期可调度、NIC 出线或内容级 A/V 同步。

## 结果

源 PID 33884、37388、34720 均完整120秒自然 exit 0；CLI 37968 在约149秒由调用方 Ctrl+C 正常停止 exit 0，不记为源结束后自然退出；VLC 42036 经 RC quit 退出。本轮没有重入。两张 VLC 原生快照已查看：三格有画面/第四格黑，随后全黑。FFmpeg 报告输入 range unknown→tv 的属性变化警告，未将其作为修复已完成。

发布/提交均84541数据报、98299936 TS payload bytes；scope available=sender available=149012608760ns，completed149012574200+duration34560精确一致，代1/序号84541与实际提交一致。错误/deadline miss为0。CPU机器占比平均2.300123%、峰值6.976744%；WS峰273588224 bytes。payload最终0/objects0，reserve/release均144345。末期phase13606ns仅时钟telemetry，不证明内容级A/V或声音。此结果仅验证服务观察切片，整体合屏门禁未完成。

## 原生快照与停止命令

~~~~powershell
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 42036){throw 'VLC RC PID mismatch'}; $c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020); $s=$c.GetStream(); $b=[Text.Encoding]::ASCII.GetBytes("snapshot`n"); $s.Write($b,0,$b.Length); $s.Dispose(); $c.Dispose()
$target=Get-CimInstance Win32_Process -Filter "ProcessId=37968"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r322*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R322Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R322Console]::FreeConsole() | Out-Null
if(-not [R322Console]::AttachConsole(37968)){throw 'AttachConsole failed'}
try {
[R322Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R322Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 37968 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R322Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R322Console]::FreeConsole() | Out-Null}

if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 42036){throw 'VLC RC PID mismatch'}; $c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020); $s=$c.GetStream(); $b=[Text.Encoding]::ASCII.GetBytes("quit`n"); $s.Write($b,0,$b.Length); $s.Dispose(); $c.Dispose()
~~~~

## 启动命令

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r322-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r322-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r322-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r322-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r322-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r322-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r322 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r322-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r322-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r322-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r322-native- --snapshot-format=png udp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r322-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

## 清理前原始指标及逐项产物清单

~~~~text
[2026-10-23 15:43:06.456] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 produced_datagrams=84541 produced_ts_payload_bytes=98299936 pending_bytes=0 peak_pending_bytes=110920 access_units=10482 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=32789115 maximum_mux_input_after_emit_ns=43093315 maximum_mux_input_after_dispatch_ns=-56906685 worst_mux_input_ready_ns=95622423200 worst_mux_input_emit_ns=95579329885 worst_mux_input_dispatch_ns=95679329885 maximum_protocol_batch_produced_after_release_ns=33334132 worst_protocol_batch_produced_ns=55612120800 worst_protocol_batch_release_ns=55578786668 worst_protocol_batch_deadline_ns=55678785668 worst_protocol_batch_datagrams=1 exit_reason=completed
[2026-10-23 15:43:06.458] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=84541 committed_datagrams=84541 committed_payload_bytes=98299936 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=1 ordered_queue_maximum_wire_bytes=1344 pacing_reserved=84541 pacing_cancelled=0 pacing_submitted=84541 pacing_maximum_submit_lateness_ns=6243040 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=1 backlog_high_water_wire_bytes=1344 backlog_max_residence_ns=22974100 last_materialized_sequence=84541 last_scheduled_sequence=84541 last_submitted_sequence=84541 last_committed_sequence=84541 submitted_service_generation=1 submitted_service_sequence=84541 submitted_service_completed_ns=149012574200 submitted_service_duration_ns=34560 submitted_service_available_ns=149012608760 submitted_scope_available_ns=149012608760 shared_scope_charged_datagrams=84541 shared_scope_charged_wire_bytes=100667084 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2154 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2154 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=188010 endpoint_1_target_effective_socket_bytes=188010 endpoint_1_api_requested_socket_bytes=188010 endpoint_1_effective_socket_bytes=188010 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=84541 endpoint_1_committed_payload_bytes=98299936
[CLI] final runtime report: queued=0, peakQueued=950, threads=81, processThreads=128, workers=0, workerProgress=555008, workerProcessCalls=1872922, workerWaits=1317897, workerWakeups=1306954, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=448, logicalProcessors=22, averageSystemMachineCpuPercent=14.491279, averageProcessMachineCpuPercent=2.300123, peakProcessMachineCpuPercent=6.976744, averageProcessSingleCoreCpuPercent=50.602708, peakProcessSingleCoreCpuPercent=153.488372, initialWorkingSetBytes=210792448, workingSetBytes=247926784, peakWorkingSetBytes=273588224, totalPushed=446065, totalPopped=446041, droppedBuffers=0, encodedPacketsPushed=48112, encodedPacketsPopped=48112, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=17199446, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=748, graphPayloadReservations=144345, graphPayloadReleases=144345, graphPayloadPressureFailures=0, lastInputReceivedAtNs=24915792616500
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r322-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r322-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r322-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r322-native-2026-10-23-15h41m16s255.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r322-native-2026-10-23-15h42m55s238.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r322-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r322-source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r322-source1-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r322-source1.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r322-source2-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r322-source2.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r322-vlc.log
~~~~

清理完成：清单12项日志/快照已逐项删除，本轮前缀残留0；提升权限核验5个精确PID均已退出，复用源124427809 bytes保留。未启动远程测试。
