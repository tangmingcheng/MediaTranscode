# r361 Windows Release realtime 验证

## 测试完成后的调用方停止命令

```powershell
$target=Get-CimInstance Win32_Process -Filter "ProcessId=36528"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r361*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R361Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R361Console]::FreeConsole() | Out-Null
if(-not [R361Console]::AttachConsole(36528)){throw 'AttachConsole failed'}
try {
[R361Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R361Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 36528 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R361Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R361Console]::FreeConsole() | Out-Null}
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 40320){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020);$s=$c.GetStream();$b=[Text.Encoding]::ASCII.GetBytes("quit`n");$s.Write($b,0,$b.Length);$c.Dispose()
```

## 重入命令（PID 6464）

```powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r361-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r361-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($source0.Id)"
$source0.WaitForExit()
Write-Output "REENTRY_EXIT=$($source0.ExitCode)"
exit $source0.ExitCode
```

已结束：画面与生命周期观察完成，但未通过全部合屏验收门禁。产物6581760 bytes、2026-10-26 17:17:27；源25024/23540/28164，CLI36528，VLC40320。r360普通执行环境零输入包；本轮同二进制/媒体参数在具备本机访问权限的执行方式下正常输出，ETW未启用（0x5拒绝）。17:21:28 VLC原生快照已查看三格图像，右下空格黑色。

## 实际启动命令

```powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r361-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r361-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r361-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r361-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r361-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r361-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r361 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r361-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r361-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r361-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r361-native- --snapshot-format=png udp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r361-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

## VLC原生快照命令

```powershell
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 40320){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
$s=$c.GetStream()
$b=[Text.Encoding]::ASCII.GetBytes("snapshot`n")
$s.Write($b,0,$b.Length)
$s.Flush()
$c.Dispose(); Start-Sleep -Seconds 2; Get-ChildItem 'D:/Code/MyCode/MediaTranscode/out/acceptance' -Filter 'composition-r361-native-*.png' | Select-Object FullName,Length
```


## 最终结果与证据边界

CLI caller_stop exit0；三个初始源及重入源均3600帧/120秒、exit0。四张VLC原生快照均已实际查看：17:21:28三格、17:23:05黑、17:23:52左下恢复、17:25:36黑。无first_interval_miss记录、发送deadline_misses=0，仅表示本轮未复现，未证明r357/r358/r359问题修复。ETW无权限未开启。VLC转换器/read错误与迟到警告仍存在，不能记完整通过；NIC delivery未证明、内容级A/V与完整G未完成。

```text
[2026-10-26 17:25:50.927] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=31481 committed_datagrams=175208 committed_payload_bytes=204366716 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=79296 pacing_reserved=175208 pacing_cancelled=0 pacing_submitted=175208 pacing_maximum_submit_lateness_ns=9777740 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=79296 backlog_max_residence_ns=52658600 last_materialized_sequence=175208 last_scheduled_sequence=175208 last_submitted_sequence=175208 last_committed_sequence=175208 submitted_service_generation=1 submitted_service_sequence=175208 submitted_service_completed_ns=296708801500 submitted_service_duration_ns=34560 submitted_service_available_ns=296708836060 submitted_scope_available_ns=296708836060 shared_scope_charged_datagrams=175208 shared_scope_charged_wire_bytes=209272540 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2154 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2154 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=188010 endpoint_1_target_effective_socket_bytes=188010 endpoint_1_api_requested_socket_bytes=188010 endpoint_1_effective_socket_bytes=188010 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=175208 endpoint_1_committed_payload_bytes=204366716
[2026-10-26 17:25:51.137] [info] [graph][runtime.node] continuous_audio_aggregate.summary reason=stop output_group=composition-r361.output audio_commits=12543 real_samples=10254336 silence_samples=2589696
[2026-10-26 17:25:51.138] [info] [graph][runtime.node] continuous_aggregate.summary reason=stop output_group=composition-r361.output output_generation=1 video_commits=8738 source[0].group=composition-r361.source.0 source[0].real_frames=3423 source[0].black_frames=5315 source[0].contribution_generation=0 source[1].group=composition-r361.source.1 source[1].real_frames=3423 source[1].black_frames=5315 source[1].contribution_generation=0 source[2].group=composition-r361.source.2 source[2].real_frames=6976 source[2].black_frames=1762 source[2].contribution_generation=0
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=957, threads=81, processThreads=129, workers=0, workerProgress=569312, workerProcessCalls=2255545, workerWaits=1686216, workerWakeups=1664021, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=889, logicalProcessors=22, averageSystemMachineCpuPercent=19.198627, averageProcessMachineCpuPercent=1.937994, peakProcessMachineCpuPercent=6.547619, averageProcessSingleCoreCpuPercent=42.635862, peakProcessSingleCoreCpuPercent=144.047619, initialWorkingSetBytes=211079168, workingSetBytes=261623808, peakWorkingSetBytes=270327808, totalPushed=489625, totalPopped=489589, droppedBuffers=0, encodedPacketsPushed=97673, encodedPacketsPopped=97673, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=18069623, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=787, graphPayloadReservations=224514, graphPayloadReleases=224514, graphPayloadPressureFailures=0, lastInputReceivedAtNs=31190251858500
```

外部监控 samples=287 CPU平均=1.92266142352748% CPU最大=4.4537853982521% WS最大=270327808 private最大=1269514240 private末值=1262555136；内部raw_phase绝对最大=11338ns，样本=230，不替代内容级A/V证据。

## 原始警告（保留失败信息）
```text
[in @ 0000025c822a2c80] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000025c822a2c80] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 30827.353389
[in @ 0000025c827701c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000025c827701c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 15699.763467
[in @ 0000025c827701c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000025c827701c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 15532.663767
[in @ 0000025c8c6eab00] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000025c9080b180] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000025c9080b180] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 15532.663767
[in @ 0000025c8c6eab00] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 30827.353389
[in @ 0000025c8bd12980] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000025c8bd12980] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 15699.763467
[aac @ 0000025c8bc29040] Qavg: 13699.666
d3d11va warning: not enough decoding slices in the texture (6/28)
d3d11va warning: surface dimensions (1280x768) differ from avcodec dimensions (1280x736)
direct3d11 error: SetThumbNailClip failed: 0x800706f4
faad warning: decoded zero sample
main error: buffer deadlock prevented
main error: Failed to create video converter
main error: read error: No error
main error: write error: No error
main warning: picture is too late to be displayed (missing 187 ms)
main warning: picture is too late to be displayed (missing 221 ms)
main warning: picture is too late to be displayed (missing 254 ms)
main warning: picture is too late to be displayed (missing 287 ms)
main warning: picture is too late to be displayed (missing 321 ms)
main warning: picture is too late to be displayed (missing 45 ms)
main warning: picture is too late to be displayed (missing 78 ms)
main warning: playback too late (147887): up-sampling
main warning: playback way too late (263961): flushing buffers
```

## 删除前文件清单
- composition-r361-cli-error.log (1968 bytes)
- composition-r361-cli.log (1100025 bytes)
- composition-r361-monitor.log (21072 bytes)
- composition-r361-native-2026-10-26-17h21m28s139.png (913931 bytes)
- composition-r361-native-2026-10-26-17h23m05s150.png (12152 bytes)
- composition-r361-native-2026-10-26-17h23m52s322.png (343440 bytes)
- composition-r361-native-2026-10-26-17h25m36s479.png (12152 bytes)
- composition-r361-reentry-stdout.log (540 bytes)
- composition-r361-reentry.log (29521 bytes)
- composition-r361-source0-stdout.log (540 bytes)
- composition-r361-source0.log (29520 bytes)
- composition-r361-source1-stdout.log (335 bytes)
- composition-r361-source1.log (28926 bytes)
- composition-r361-source2-stdout.log (335 bytes)
- composition-r361-source2.log (28926 bytes)
- composition-r361-vlc.log (42634 bytes)

清理完成：六个精确PID均不存在，16个本轮文件按已归档清单逐项删除，无远程测试；复用源与Release产物保留。
