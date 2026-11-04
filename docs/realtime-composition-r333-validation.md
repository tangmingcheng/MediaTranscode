# r333：prepared 登记 UDP 与输入重入复验

状态：执行完成；本切片UDP正常/失源/输入重入观察通过，不代表完整合屏验收。

三路120秒H264/AAC RTP→HEVC CBR 8000kbps 1280×720/30、AAC CBR 192kbps 44100Hz stereo→MPEG-TS/UDP 62024；随后音视频源完整120秒重入。仅realtime、VLC原生快照。

## 启动命令

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r333-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r333-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r333-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r333-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r333-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r333-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r333 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r333-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r333-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r333-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r333-native- --snapshot-format=png udp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r333-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

## 实际重入命令

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r333-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r333-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($source0.Id)"
$source0.WaitForExit()
Write-Output "REENTRY_EXIT=$($source0.ExitCode)"
exit $source0.ExitCode
~~~~

## 结果与范围

2026-10-26 使用10:25:58构建的Release产物，本轮未改生产代码。初始源29924、37852、11644及重入源18196均完整120秒、3600帧自然exit0。CLI34824在源结束后持续输出，经caller_stop正常exit0；VLC22988经RC quit。六PID均已复核退出。

四张VLC原生快照已查看：三格→全黑→左下格重入→全黑。未使用系统截图。TS生成与UDP提交均178846数据报/207961088字节；sender deadline miss、压力、partial/ambiguous failure均0。worker/errors为0，payload字节/对象归零，reservation/release均262871。

CPU机器平均2.055202%、峰5.630631%；工作集峰249339904字节、结束245952512字节，最后private1149800448字节。重入时钟generation2末段phase约13605ns、frequency/stretch0；输出generation仍1，不能作为输出purge证据。约303秒观察不证明长期内存无增长；内容A/V、音频听感、NIC出线、完整矩阵、间歇崩溃仍未关闭。此结果仅支持本切片UDP正常/失源/输入重入路径。

## 快照与结束命令

~~~~powershell
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 22988){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020);$s=$c.GetStream();$b=[Text.Encoding]::ASCII.GetBytes("snapshot`n");$s.Write($b,0,$b.Length);$c.Dispose()
$target=Get-CimInstance Win32_Process -Filter "ProcessId=34824"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r333*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R333Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R333Console]::FreeConsole() | Out-Null
if(-not [R333Console]::AttachConsole(34824)){throw 'AttachConsole failed'}
try {
[R333Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R333Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 34824 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R333Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R333Console]::FreeConsole() | Out-Null}
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 22988){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020);$s=$c.GetStream();$b=[Text.Encoding]::ASCII.GetBytes("quit`n");$s.Write($b,0,$b.Length);$c.Dispose()
~~~~

## 关键记录

~~~~text
[2026-10-26 10:36:15.056] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=5052 source_master_ns=273074127382 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5214790
[2026-10-26 10:36:16.078] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=5096 source_master_ns=274095805386 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5260136
[2026-10-26 10:36:45.541] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 produced_datagrams=178846 produced_ts_payload_bytes=207961088 pending_bytes=0 peak_pending_bytes=92872 access_units=22093 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=61769891 maximum_mux_input_after_emit_ns=69294907 maximum_mux_input_after_dispatch_ns=-30705093 worst_mux_input_ready_ns=59712160100 worst_mux_input_emit_ns=59642865193 worst_mux_input_dispatch_ns=59742865193 maximum_protocol_batch_produced_after_release_ns=39108016 worst_protocol_batch_produced_ns=59743743100 worst_protocol_batch_release_ns=59704635084 worst_protocol_batch_deadline_ns=59804633901 worst_protocol_batch_datagrams=1 exit_reason=completed
[2026-10-26 10:36:45.542] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=178846 committed_datagrams=178846 committed_payload_bytes=207961088 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=1 ordered_queue_maximum_wire_bytes=1344 pacing_reserved=178846 pacing_cancelled=0 pacing_submitted=178846 pacing_maximum_submit_lateness_ns=7489461 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=1 backlog_high_water_wire_bytes=1344 backlog_max_residence_ns=24875600 last_materialized_sequence=178846 last_scheduled_sequence=178846 last_submitted_sequence=178846 last_committed_sequence=178846 submitted_service_generation=1 submitted_service_sequence=178846 submitted_service_completed_ns=302918719400 submitted_service_duration_ns=34560 submitted_service_available_ns=302918753960 submitted_scope_available_ns=302918753960 shared_scope_charged_datagrams=178846 shared_scope_charged_wire_bytes=212968776 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2154 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2154 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=188010 endpoint_1_target_effective_socket_bytes=188010 endpoint_1_api_requested_socket_bytes=188010 endpoint_1_effective_socket_bytes=188010 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=178846 endpoint_1_committed_payload_bytes=207961088
[CLI] final runtime report: queued=0, peakQueued=402, threads=81, processThreads=130, workers=0, workerProgress=1032723, workerProcessCalls=3532538, workerWaits=2499798, workerWakeups=2476804, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=946, logicalProcessors=22, averageSystemMachineCpuPercent=19.687013, averageProcessMachineCpuPercent=2.055202, peakProcessMachineCpuPercent=5.630631, averageProcessSingleCoreCpuPercent=45.214440, peakProcessSingleCoreCpuPercent=123.873874, initialWorkingSetBytes=190099456, workingSetBytes=245952512, peakWorkingSetBytes=249339904, totalPushed=814284, totalPopped=814248, droppedBuffers=0, encodedPacketsPushed=101400, encodedPacketsPopped=101400, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=5077081, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=210, graphPayloadReservations=262871, graphPayloadReleases=262871, graphPayloadPressureFailures=0, lastInputReceivedAtNs=6641676788700
seconds=302.7287693 cpu=0.961836017665595 ws=245952512 private=1149800448
~~~~

## 清理清单

D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r333-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r333-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r333-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r333-native-2026-10-26-10h32m18s171.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r333-native-2026-10-26-10h34m01s408.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r333-native-2026-10-26-10h34m45s984.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r333-native-2026-10-26-10h36m29s774.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r333-reentry-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r333-reentry.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r333-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r333-source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r333-source1-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r333-source1.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r333-source2-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r333-source2.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r333-vlc.log



清理完成：以上16项临时产物已逐项删除，六PID无残留；指定120秒源保留，远程未运行本轮测试。
