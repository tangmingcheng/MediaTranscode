# r324：预约身份与源重入复验

三路120秒H264/AAC RTP→HEVC CBR 8000kbps 1280×720/30、AAC CBR 192kbps 44100Hz stereo→MPEG-TS/UDP 62024。随后音视频源完整120秒重入。仅realtime、VLC原生快照。已执行，不计整体合屏验收通过。

## 结果

原始源35028/5020/30768及重入源564均完整120秒自然exit0；CLI7700约318秒调用方Ctrl+C停止exit0，非源结束自动退出；VLC35556经RC quit退出。4张原生快照均已查看：三格→全黑→左下格重入→全黑。输入range变化警告仍存在。

发布/提交177116数据报、204204848 TS payload bytes一致，sender/scope available均318365096360ns。deadline miss、worker errors、pressure失败均0；最大submit lateness54037240ns，本轮未越期限，不宣称OS硬界。payload/objects最终0，reserve/release均260259。CPU机器平均1.882328%、峰6.666667%，WS峰273551360 bytes，最终private1148461056。输入时钟generation1 phase约9072ns、generation2约11338ns；输出sender generation仍1，不将输入重入夸大为输出purge已验证。声音、内容级A/V、NIC出线仍未证明。

本切片只验证预约身份改动在当前实流和输入重入下未破坏提交；身份映射及完整周期门禁未完成。

## 原生快照与停止

~~~~powershell
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 35556){throw 'VLC RC PID mismatch'}; $c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020); $s=$c.GetStream(); $b=[Text.Encoding]::ASCII.GetBytes("snapshot`n"); $s.Write($b,0,$b.Length); $s.Dispose(); $c.Dispose()
$target=Get-CimInstance Win32_Process -Filter "ProcessId=7700"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r324*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R324Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R324Console]::FreeConsole() | Out-Null
if(-not [R324Console]::AttachConsole(7700)){throw 'AttachConsole failed'}
try {
[R324Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R324Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 7700 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R324Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R324Console]::FreeConsole() | Out-Null}

if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 35556){throw 'VLC RC PID mismatch'}; $c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020); $s=$c.GetStream(); $b=[Text.Encoding]::ASCII.GetBytes("quit`n"); $s.Write($b,0,$b.Length); $s.Dispose(); $c.Dispose()
~~~~

## 音视频重入命令

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r324-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r324-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($source0.Id)"
$source0.WaitForExit()
Write-Output "REENTRY_EXIT=$($source0.ExitCode)"
exit $source0.ExitCode
~~~~

## 启动命令

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r324-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r324-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r324-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r324-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r324-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r324-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r324 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r324-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r324-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r324-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r324-native- --snapshot-format=png udp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r324-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

## 清理前原始指标与产物清单

~~~~text
[2026-10-23 16:11:56.932] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 produced_datagrams=177116 produced_ts_payload_bytes=204204848 pending_bytes=0 peak_pending_bytes=110920 access_units=22857 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=65397614 maximum_mux_input_after_emit_ns=83883194 maximum_mux_input_after_dispatch_ns=-16116806 worst_mux_input_ready_ns=45052251300 worst_mux_input_emit_ns=44968368106 worst_mux_input_dispatch_ns=45068368106 maximum_protocol_batch_produced_after_release_ns=65976512 worst_protocol_batch_produced_ns=44963415700 worst_protocol_batch_release_ns=44897439188 worst_protocol_batch_deadline_ns=44997438401 worst_protocol_batch_datagrams=1 exit_reason=completed
[2026-10-23 16:11:56.934] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=177116 committed_datagrams=177116 committed_payload_bytes=204204848 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=1 ordered_queue_maximum_wire_bytes=1344 pacing_reserved=177116 pacing_cancelled=0 pacing_submitted=177116 pacing_maximum_submit_lateness_ns=54037240 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=1 backlog_high_water_wire_bytes=1344 backlog_max_residence_ns=27336400 last_materialized_sequence=177116 last_scheduled_sequence=177116 last_submitted_sequence=177116 last_committed_sequence=177116 submitted_service_generation=1 submitted_service_sequence=177116 submitted_service_completed_ns=318365061800 submitted_service_duration_ns=34560 submitted_service_available_ns=318365096360 submitted_scope_available_ns=318365096360 shared_scope_charged_datagrams=177116 shared_scope_charged_wire_bytes=209164096 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2154 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2154 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=188010 endpoint_1_target_effective_socket_bytes=188010 endpoint_1_api_requested_socket_bytes=188010 endpoint_1_effective_socket_bytes=188010 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=177116 endpoint_1_committed_payload_bytes=204204848
[CLI] final runtime report: queued=0, peakQueued=934, threads=81, processThreads=130, workers=0, workerProgress=1029444, workerProcessCalls=3414499, workerWaits=2385038, workerWakeups=2361257, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=966, logicalProcessors=22, averageSystemMachineCpuPercent=12.153619, averageProcessMachineCpuPercent=1.882328, peakProcessMachineCpuPercent=6.666667, averageProcessSingleCoreCpuPercent=41.411213, peakProcessSingleCoreCpuPercent=146.666667, initialWorkingSetBytes=211464192, workingSetBytes=264581120, peakWorkingSetBytes=273551360, totalPushed=809563, totalPopped=809527, droppedBuffers=0, encodedPacketsPushed=104906, encodedPacketsPopped=104906, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=17177551, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=752, graphPayloadReservations=260259, graphPayloadReleases=260259, graphPayloadPressureFailures=0, lastInputReceivedAtNs=26641933608700
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r324-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r324-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r324-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r324-native-2026-10-23-16h07m16s270.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r324-native-2026-10-23-16h09m02s812.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r324-native-2026-10-23-16h09m51s920.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r324-native-2026-10-23-16h11m41s212.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r324-reentry-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r324-reentry.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r324-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r324-source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r324-source1-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r324-source1.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r324-source2-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r324-source2.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r324-vlc.log
~~~~

清理完成：清单16项日志/快照逐项删除，前缀残留0；提升权限核验6个精确PID残留0。复用源124427809 bytes保留，未启动远程测试。
