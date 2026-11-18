# r362 Windows Release realtime 验证

## 重入命令（PID 43076）

```powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r362-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r362-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($source0.Id)"
$source0.WaitForExit()
Write-Output "REENTRY_EXIT=$($source0.ExitCode)"
exit $source0.ExitCode
```

## 观测完成后的调用方停止命令

```powershell
$target=Get-CimInstance Win32_Process -Filter "ProcessId=40704"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r362*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R362Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R362Console]::FreeConsole() | Out-Null
if(-not [R362Console]::AttachConsole(40704)){throw 'AttachConsole failed'}
try {
[R362Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R362Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 40704 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R362Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R362Console]::FreeConsole() | Out-Null}
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 6748){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020);$s=$c.GetStream();$b=[Text.Encoding]::ASCII.GetBytes("quit`n");$s.Write($b,0,$b.Length);$c.Dispose()
```


已结束：完成本规格画面/生命周期观察，未通过全部合屏门禁。Release全量重建48554 exit0（755项），configure/build均0；realtime6591488 bytes、2026-10-26 17:42:50，cache/ninja/两个CLI产物已核验，local仅构建未运行。本版本含Extractor精确staging与shared对象封印工厂接线，均双独立源码PASS。源32420/7988/43580、CLI40704、VLC6748。17:43:56 VLC原生快照已查看三格。

## 实际启动命令

```powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r362-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r362-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r362-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r362-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r362-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r362-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r362 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r362-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r362-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r362-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r362-native- --snapshot-format=png udp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r362-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 6748){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
$s=$c.GetStream()
$b=[Text.Encoding]::ASCII.GetBytes("snapshot`n")
$s.Write($b,0,$b.Length)
$s.Flush()
$c.Dispose(); Start-Sleep -Seconds 2; Get-ChildItem 'D:/Code/MyCode/MediaTranscode/out/acceptance' -Filter 'composition-r362-native-*.png' | Select-Object FullName,Length
```


## 最终结果
四源各3600帧/120秒、exit0；CLI caller_stop exit0（主会话40010，重入9293），不是失源自然退出。四张原生快照已实际查看：17:43:56三格、17:45:39黑、17:46:16左下恢复、17:47:54黑。最终无first_interval_miss字段，deadline_misses0；只代表本轮未复现，不证明历史间歇故障修复。此规格仍走cadence，不能当作Prepared/noncadence域完整验收。
```text
[2026-10-26 17:48:12.104] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=30354 committed_datagrams=173911 committed_payload_bytes=203890700 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=79296 pacing_reserved=173911 pacing_cancelled=0 pacing_submitted=173911 pacing_maximum_submit_lateness_ns=8843040 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=79296 backlog_max_residence_ns=54273200 last_materialized_sequence=173911 last_scheduled_sequence=173911 last_submitted_sequence=173911 last_committed_sequence=173911 submitted_service_generation=1 submitted_service_sequence=173911 submitted_service_completed_ns=286372774200 submitted_service_duration_ns=34560 submitted_service_available_ns=286372808760 submitted_scope_available_ns=286372808760 shared_scope_charged_datagrams=173911 shared_scope_charged_wire_bytes=208760208 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2154 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2154 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=188010 endpoint_1_target_effective_socket_bytes=188010 endpoint_1_api_requested_socket_bytes=188010 endpoint_1_effective_socket_bytes=188010 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=173911 endpoint_1_committed_payload_bytes=203890700
[2026-10-26 17:48:12.322] [info] [graph][runtime.node] continuous_audio_aggregate.summary reason=stop output_group=composition-r362.output audio_commits=12093 real_samples=10247168 silence_samples=2136064
[2026-10-26 17:48:12.324] [info] [graph][runtime.node] continuous_aggregate.summary reason=stop output_group=composition-r362.output output_generation=1 video_commits=8424 source[0].group=composition-r362.source.0 source[0].real_frames=3418 source[0].black_frames=5006 source[0].contribution_generation=0 source[1].group=composition-r362.source.1 source[1].real_frames=3418 source[1].black_frames=5006 source[1].contribution_generation=0 source[2].group=composition-r362.source.2 source[2].real_frames=6970 source[2].black_frames=1454 source[2].contribution_generation=0
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=924, threads=81, processThreads=129, workers=0, workerProgress=560274, workerProcessCalls=2245653, workerWaits=1685362, workerWakeups=1663974, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=860, logicalProcessors=22, averageSystemMachineCpuPercent=20.205588, averageProcessMachineCpuPercent=2.070462, peakProcessMachineCpuPercent=6.625259, averageProcessSingleCoreCpuPercent=45.550155, peakProcessSingleCoreCpuPercent=145.755694, initialWorkingSetBytes=209649664, workingSetBytes=260145152, peakWorkingSetBytes=269643776, totalPushed=482548, totalPopped=482512, droppedBuffers=0, encodedPacketsPushed=94167, encodedPacketsPopped=94167, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=16194458, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=747, graphPayloadReservations=220671, graphPayloadReleases=220671, graphPayloadPressureFailures=0, lastInputReceivedAtNs=32534507951100
```

外部监控samples=277，CPU平均=2.04772257715844%、最大=4.15688247382527%；WS峰值=269651968，private峰值=1275580416，末值=1272250368。内部raw_phase绝对最大=13606ns，样本=230，不替代内容级A/V证据。NIC delivery仍not_proven，完整G与矩阵未完成。
## 原始警告
```text
[in @ 0000015e31e22080] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000015e31e22080] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 36601.388256
[in @ 0000015e36df29c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000015e36df29c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 28754.316856
[in @ 0000015e36df3780] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000015e36df3780] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 17503.392478
[in @ 0000015e22e906c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000015e22e8f7c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000015e22e8f7c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 28754.316856
[in @ 0000015e22e906c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 17503.392478
[in @ 0000015e3cf47180] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000015e3cf47180] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 36601.388256
[aac @ 0000015e36e3f540] Qavg: 11821.741
d3d11va warning: not enough decoding slices in the texture (6/28)
d3d11va warning: surface dimensions (1280x768) differ from avcodec dimensions (1280x736)
direct3d11 error: SetThumbNailClip failed: 0x800706f4
faad warning: decoded zero sample
main error: buffer deadlock prevented
main error: Failed to create video converter
main error: read error: No error
main error: write error: No error
main warning: buffer too late (-75626 us): dropped
main warning: picture is too late to be displayed (missing 286 ms)
main warning: picture is too late to be displayed (missing 319 ms)
main warning: picture is too late to be displayed (missing 353 ms)
main warning: picture is too late to be displayed (missing 386 ms)
main warning: picture is too late to be displayed (missing 418 ms)
main warning: playback too late (113178): up-sampling
```
## 删除前清单
- composition-r362-cli-error.log (1968 bytes)
- composition-r362-cli.log (1072693 bytes)
- composition-r362-monitor.log (20357 bytes)
- composition-r362-native-2026-10-26-17h43m56s141.png (864532 bytes)
- composition-r362-native-2026-10-26-17h45m39s681.png (12152 bytes)
- composition-r362-native-2026-10-26-17h46m16s456.png (296441 bytes)
- composition-r362-native-2026-10-26-17h47m54s382.png (12152 bytes)
- composition-r362-reentry-stdout.log (540 bytes)
- composition-r362-reentry.log (29521 bytes)
- composition-r362-source0-stdout.log (540 bytes)
- composition-r362-source0.log (29521 bytes)
- composition-r362-source1-stdout.log (335 bytes)
- composition-r362-source1.log (28926 bytes)
- composition-r362-source2-stdout.log (335 bytes)
- composition-r362-source2.log (28926 bytes)
- composition-r362-vlc.log (42123 bytes)

清理完成：六个精确PID均不存在，16项产物按归档清单逐项删除；复用源与Release产物保留，无远程测试。
