# r325：实际wire范围绑定的TS/RTP复验

三路120秒H264/AAC RTP→HEVC CBR 8000kbps 1280×720/30、AAC CBR 192kbps 44100Hz stereo→MPEG-TS/RTP 62024，RTCP 62025。仅realtime、VLC原生快照，原规格不降低。已执行，画面证据不完整，不计完整验收通过。

## 结果：画面证据不完整，未通过完整验收

两名独立审查者最终均静态PASS。源30156/36652/44936完整120秒自然exit0；CLI34860约181秒经调用方Ctrl+C退出0，VLC43872经RC quit退出。

初始VLC以SDP路径启动但RC status为stop state:5，端口未监听、无快照；日志未提供足以确认原因的错误，不能断言SDP无效。后以RC add rtp://@127.0.0.1:62024打开同一输出成功解码，但源已结束，仅取得全黑原生快照。未取得三格播放证据，不计成功验收；下一轮从启动时使用该RTP URI，不改媒体规格。

TS产生88034数据报、99001740 bytes；RTP媒体88034个、100058148 bytes=TS bytes+12×88034；RTCP43个、2932 bytes；总提交88077个、100061080 bytes。deadline miss/worker errors/pressure失败0，payload与objects回零，reserve/release151663。CPU机器平均2.213533%、峰6.666667%，WS峰273448960 bytes。submitted service/scope available均181019987740ns。NIC出线、内容级A/V、完整周期未证明。

## 播放器诊断与停止命令

~~~~powershell
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 43872){throw 'VLC RC PID mismatch'}; $c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020); $s=$c.GetStream(); $b=[Text.Encoding]::ASCII.GetBytes("add rtp://@127.0.0.1:62024`n"); $s.Write($b,0,$b.Length); $s.Dispose(); $c.Dispose()
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 43872){throw 'VLC RC PID mismatch'}; $c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020); $s=$c.GetStream(); $b=[Text.Encoding]::ASCII.GetBytes("snapshot`n"); $s.Write($b,0,$b.Length); $s.Dispose(); $c.Dispose()
$target=Get-CimInstance Win32_Process -Filter "ProcessId=34860"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r325*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R325Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R325Console]::FreeConsole() | Out-Null
if(-not [R325Console]::AttachConsole(34860)){throw 'AttachConsole failed'}
try {
[R325Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R325Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 34860 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R325Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R325Console]::FreeConsole() | Out-Null}

if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 43872){throw 'VLC RC PID mismatch'}; $c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020); $s=$c.GetStream(); $b=[Text.Encoding]::ASCII.GetBytes("quit`n"); $s.Write($b,0,$b.Length); $s.Dispose(); $c.Dispose()
~~~~

## 启动命令

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r325-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r325-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r325-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r325-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r325-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r325-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r325 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62024 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r325-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r325-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r325-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r325-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r325-native- --snapshot-format=png D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r325-output.sdp' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r325-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

## 清理前原始指标、SDP与文件清单

~~~~text
[2026-10-23 16:27:42.466] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 produced_datagrams=88034 produced_ts_payload_bytes=99001740 pending_bytes=0 peak_pending_bytes=110920 access_units=12823 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=32789115 maximum_mux_input_after_emit_ns=38655215 maximum_mux_input_after_dispatch_ns=-61344785 worst_mux_input_ready_ns=95584746100 worst_mux_input_emit_ns=95546090885 worst_mux_input_dispatch_ns=95646090885 maximum_protocol_batch_produced_after_release_ns=33334592 worst_protocol_batch_produced_ns=107578883300 worst_protocol_batch_release_ns=107545548708 worst_protocol_batch_deadline_ns=107645546668 worst_protocol_batch_datagrams=1 exit_reason=completed
[2026-10-23 16:27:42.467] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=88035 committed_datagrams=88077 committed_payload_bytes=100061080 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=2 ordered_queue_maximum_wire_bytes=1452 pacing_reserved=88077 pacing_cancelled=0 pacing_submitted=88077 pacing_maximum_submit_lateness_ns=14760272 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=2 backlog_high_water_wire_bytes=1452 backlog_max_residence_ns=34424400 last_materialized_sequence=88077 last_scheduled_sequence=88077 last_submitted_sequence=88077 last_committed_sequence=88077 submitted_service_generation=1 submitted_service_sequence=88077 submitted_service_completed_ns=181019971100 submitted_service_duration_ns=16640 submitted_service_available_ns=181019987740 submitted_scope_available_ns=181019987740 shared_scope_charged_datagrams=88077 shared_scope_charged_wire_bytes=102527236 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2155 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2155 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=379902 endpoint_1_target_effective_socket_bytes=189951 endpoint_1_api_requested_socket_bytes=189951 endpoint_1_effective_socket_bytes=189951 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=88034 endpoint_1_committed_payload_bytes=100058148 endpoint_2_target_effective_socket_bytes=189951 endpoint_2_api_requested_socket_bytes=189951 endpoint_2_effective_socket_bytes=189951 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=43 endpoint_2_committed_payload_bytes=2932
[CLI] final runtime report: queued=0, peakQueued=922, threads=82, processThreads=130, workers=0, workerProgress=581325, workerProcessCalls=1925716, workerWaits=1344373, workerWakeups=1330996, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=525, logicalProcessors=22, averageSystemMachineCpuPercent=11.200368, averageProcessMachineCpuPercent=2.213533, peakProcessMachineCpuPercent=6.666667, averageProcessSingleCoreCpuPercent=48.697715, peakProcessSingleCoreCpuPercent=146.666667, initialWorkingSetBytes=211886080, workingSetBytes=247279616, peakWorkingSetBytes=273448960, totalPushed=466774, totalPopped=466750, droppedBuffers=0, encodedPacketsPushed=58856, encodedPacketsPopped=58856, backpressureItems=124, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=17085611, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=741, graphPayloadReservations=151663, graphPayloadReleases=151663, graphPayloadPressureFailures=0, lastInputReceivedAtNs=27559825579100
v=0
o=composition-r325 17187311017116199878 1 IN IP4 127.0.0.1
s=composition-r325
c=IN IP4 127.0.0.1
t=0 0
m=video 62024 RTP/AVP 33
a=rtcp:62025 IN IP4 127.0.0.1
a=rtpmap:33 MP2T/90000
a=ssrc:255037526 cname:mt-657211514@media-transcode

D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r325-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r325-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r325-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r325-native-2026-10-23-16h27m20s077.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r325-output.sdp
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r325-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r325-source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r325-source1-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r325-source1.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r325-source2-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r325-source2.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r325-vlc.log
~~~~

已按清单删除12项临时产物，前缀残留0，提升权限核验5个PID残留0；复用源124427809 bytes保留，未启动远程测试。
