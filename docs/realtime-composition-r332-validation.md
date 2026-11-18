# r332：prepared 工作登记与范围绑定复验

状态：已执行完成，本切片 TS/RTP 正常及失源收尾观察通过；不代表完整合屏验收完成。

输入为三个120秒 H.264 源，其中一路附带 AAC；输出 HEVC CBR 1280×720 30fps 8000kbps、AAC CBR 192kbps，MPEG-TS/RTP。仅运行 realtime CLI。

## 实际执行命令

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r332-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r332-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r332-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r332-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r332-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r332-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r332 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62024 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r332-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r332-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r332-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r332-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r332-native- --snapshot-format=png rtp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r332-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

## 执行结果与边界

2026-10-26 Release 全量重建成功，configure/build exit0；realtime产物6450688字节，时间10:25:58。两名独立审查crash_release_audit、publication_identity_review_b均对第五切片静态PASS。

- 源PID6500、30304、6420均3600帧/120秒自然exit0；CLI37548经caller_stop正常exit0；VLC28724通过RC quit。
- 两张VLC原生快照核验三格（右下空格）→全黑；未使用系统截图。全失源后CLI持续输出符合连续模式，最终退出不是EOF自动结束。
- TS85892包/98528168字节；RTP媒体99558872=98528168+12×85892，RTCP36包/2456字节；总85928包/99561328字节一致。sender deadline/pressure/partial/ambiguous failure均0，socket提交不证明NIC交付。
- 最终worker/errors为0、payload字节/对象回零，reservation/release均146958。平均CPU2.486741%，峰值6.932773%；工作集峰269926400字节，结束245436416字节。源结束后回落，不足以证明长期内存无增长。
- 末段av_drift_trace raw_phase15873ns、filtered15874ns、frequency/stretch0；这是时钟控制遥测，不替代内容A/V同步。FFmpeg仍有range unknown→tv属性警告，未据此宣称色彩门禁通过。
- 本轮只验证TS/RTP三源正常及失源收尾；UDP、重入、完整矩阵、间歇崩溃、完整工作快照/周期与generation owner预算均未关闭。

## 原生快照与结束命令

~~~~powershell
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 28724){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020);$s=$c.GetStream();$b=[Text.Encoding]::ASCII.GetBytes("snapshot`n");$s.Write($b,0,$b.Length);$c.Dispose()
$target=Get-CimInstance Win32_Process -Filter "ProcessId=37548"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r332*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R332Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R332Console]::FreeConsole() | Out-Null
if(-not [R332Console]::AttachConsole(37548)){throw 'AttachConsole failed'}
try {
[R332Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R332Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 37548 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R332Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R332Console]::FreeConsole() | Out-Null}
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 28724){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020);$s=$c.GetStream();$b=[Text.Encoding]::ASCII.GetBytes("quit`n");$s.Write($b,0,$b.Length);$c.Dispose()
~~~~

## 关键原始记录

~~~~text
[2026-10-26 10:28:57.763] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 produced_datagrams=85892 produced_ts_payload_bytes=98528168 pending_bytes=0 peak_pending_bytes=110920 access_units=11410 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=32789115 maximum_mux_input_after_emit_ns=41379278 maximum_mux_input_after_dispatch_ns=-58620722 worst_mux_input_ready_ns=5690654200 worst_mux_input_emit_ns=5649274922 worst_mux_input_dispatch_ns=5749274922 maximum_protocol_batch_produced_after_release_ns=33331492 worst_protocol_batch_produced_ns=37659387100 worst_protocol_batch_release_ns=37626055608 worst_protocol_batch_deadline_ns=37726054968 worst_protocol_batch_datagrams=1 exit_reason=completed
[CLI] final runtime report: queued=0, peakQueued=933, threads=82, processThreads=130, workers=0, workerProgress=564976, workerProcessCalls=1987229, workerWaits=1422235, workerWakeups=1410318, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=478, logicalProcessors=22, averageSystemMachineCpuPercent=26.368381, averageProcessMachineCpuPercent=2.486741, peakProcessMachineCpuPercent=6.932773, averageProcessSingleCoreCpuPercent=54.708301, peakProcessSingleCoreCpuPercent=152.521008, initialWorkingSetBytes=208101376, workingSetBytes=245436416, peakWorkingSetBytes=269926400, totalPushed=454308, totalPopped=454284, droppedBuffers=0, encodedPacketsPushed=52371, encodedPacketsPopped=52371, backpressureItems=124, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=17366588, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=750, graphPayloadReservations=146958, graphPayloadReleases=146958, graphPayloadPressureFailures=0, lastInputReceivedAtNs=6160378836100
[2026-10-26 10:28:57.764] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=85893 committed_datagrams=85928 committed_payload_bytes=99561328 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=2 ordered_queue_maximum_wire_bytes=1452 pacing_reserved=85928 pacing_cancelled=0 pacing_submitted=85928 pacing_maximum_submit_lateness_ns=5174320 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=2 backlog_high_water_wire_bytes=1452 backlog_max_residence_ns=22928500 last_materialized_sequence=85928 last_scheduled_sequence=85928 last_submitted_sequence=85928 last_committed_sequence=85928 submitted_service_generation=1 submitted_service_sequence=85928 submitted_service_completed_ns=161760458500 submitted_service_duration_ns=16640 submitted_service_available_ns=161760475140 submitted_scope_available_ns=161760475140 shared_scope_charged_datagrams=85928 shared_scope_charged_wire_bytes=101967312 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2155 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2155 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=379902 endpoint_1_target_effective_socket_bytes=189951 endpoint_1_api_requested_socket_bytes=189951 endpoint_1_effective_socket_bytes=189951 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=85892 endpoint_1_committed_payload_bytes=99558872 endpoint_2_target_effective_socket_bytes=189951 endpoint_2_api_requested_socket_bytes=189951 endpoint_2_effective_socket_bytes=189951 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=36 endpoint_2_committed_payload_bytes=2456
~~~~

## 本轮清理清单

D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r332-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r332-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r332-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r332-native-2026-10-26-10h26m49s167.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r332-native-2026-10-26-10h28m39s877.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r332-output.sdp
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r332-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r332-source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r332-source1-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r332-source1.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r332-source2-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r332-source2.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r332-vlc.log



清理完成：五PID均无残留，以上13项临时产物已逐项删除；指定120秒源保留，未在远程执行本轮测试。
