# r365 Windows Release realtime 验证

已结束，仅为cadence回归证据，不判完整目标通过。产物6602752 bytes、2026-10-26 18:58:39；Release91686 configure/build均0（755项），cache/ninja/两CLI存在，local未运行。PreparedPush显式域和reservation常驻实账均双独立源码PASS，本轮联合复验，不代表完整G/noncadence通过。源46240/40048/33628，CLI43068，主会话57894。

## 实际启动命令
```powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r365-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r365-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r365-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r365-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r365-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r365-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r365 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r365-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r365-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r365-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r365-native- --snapshot-format=png udp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r365-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

VLC PID44288；19:04:53原生快照已实际查看三格输出。尚未完成后续失源/重入观察。

## VLC快照命令
```powershell
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 44288){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
$s=$c.GetStream()
$b=[Text.Encoding]::ASCII.GetBytes("snapshot`n")
$s.Write($b,0,$b.Length)
$s.Flush()
$c.Dispose(); Start-Sleep -Seconds 2; Get-ChildItem 'D:/Code/MyCode/MediaTranscode/out/acceptance' -Filter 'composition-r365-native-*.png' | Select-Object FullName,Length
```

19:06:31原生快照已实际查看全黑；重入PID46392、会话14971。19:07:09原生快照已查看左下格恢复，其余格黑，generation2时序记录已出现。

## 重入命令
```powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r365-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r365-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($source0.Id)"
$source0.WaitForExit()
Write-Output "REENTRY_EXIT=$($source0.ExitCode)"
exit $source0.ExitCode
```

## 停止命令
```powershell
$target=Get-CimInstance Win32_Process -Filter "ProcessId=43068"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r365*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R365Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R365Console]::FreeConsole() | Out-Null
if(-not [R365Console]::AttachConsole(43068)){throw 'AttachConsole failed'}
try {
[R365Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R365Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 43068 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R365Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R365Console]::FreeConsole() | Out-Null}
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 44288){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020);$s=$c.GetStream();$b=[Text.Encoding]::ASCII.GetBytes("quit`n");$s.Write($b,0,$b.Length);$c.Dispose()
```

## 收尾结论

四张VLC原生快照均实际查看：三格→全黑→左下恢复→全黑，最后19:08:56。重入会话14971退出0，主会话57894返回CLI及三初始源均退出0。CLI end_reason=caller_stop，不是源结束自动退出；六精确PID已核验不存在。payload预约/释放224456匹配回零，errors/pressure/deadline_misses均0，无interval_miss记录；这些不证明历史间歇故障修复。VLC错误/警告保留；本轮是cadence回归，不替代Prepared/noncadence、完整G、populated取消、内容级A/V和RKMPP验收，不判完整目标通过。

外部采样count=285，CPU均值=2.02513512954504%，CPU峰值=5.02754479672992%，WS峰值=269271040，private峰值=1266413568；内部raw_phase绝对最大=13606ns。

### 运行证据
`	ext
[2026-10-26 19:09:11.950] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=31233 committed_datagrams=174631 committed_payload_bytes=203812680 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=79296 pacing_reserved=174631 pacing_cancelled=0 pacing_submitted=174631 pacing_maximum_submit_lateness_ns=6371066 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=79296 backlog_max_residence_ns=50252600 last_materialized_sequence=174631 last_scheduled_sequence=174631 last_submitted_sequence=174631 last_committed_sequence=174631 submitted_service_generation=1 submitted_service_sequence=174631 submitted_service_completed_ns=294524477700 submitted_service_duration_ns=34560 submitted_service_available_ns=294524512260 submitted_scope_available_ns=294524512260 shared_scope_charged_datagrams=174631 shared_scope_charged_wire_bytes=208702348 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2154 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2154 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=188010 endpoint_1_target_effective_socket_bytes=188010 endpoint_1_api_requested_socket_bytes=188010 endpoint_1_effective_socket_bytes=188010 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=174631 endpoint_1_committed_payload_bytes=203812680
[2026-10-26 19:09:12.161] [info] [graph][runtime.node] continuous_audio_aggregate.summary reason=stop output_group=composition-r365.output audio_commits=12444 real_samples=10247168 silence_samples=2495488
[2026-10-26 19:09:12.161] [info] [graph][runtime.lifecycle] video_encode.zero_copy_summary drm_prime=0 software_frame=0
[2026-10-26 19:09:12.163] [info] [graph][runtime.node] continuous_aggregate.summary reason=stop output_group=composition-r365.output output_generation=1 video_commits=8668 source[0].group=composition-r365.source.0 source[0].real_frames=3417 source[0].black_frames=5251 source[0].contribution_generation=0 source[1].group=composition-r365.source.1 source[1].real_frames=3417 source[1].black_frames=5251 source[1].contribution_generation=0 source[2].group=composition-r365.source.2 source[2].real_frames=6971 source[2].black_frames=1697 source[2].contribution_generation=0
[2026-10-26 19:09:12.165] [info] [graph][runtime.lifecycle] video_filter.zero_copy_summary drm_prime_input=0 drm_prime_output=0 rga=0 software_frame=0
[2026-10-26 19:09:12.174] [info] [graph][runtime.lifecycle] video_filter.zero_copy_summary drm_prime_input=0 drm_prime_output=0 rga=0 software_frame=0
[2026-10-26 19:09:12.180] [info] [graph][runtime.lifecycle] video_filter.zero_copy_summary drm_prime_input=0 drm_prime_output=0 rga=0 software_frame=0
[2026-10-26 19:09:12.188] [info] [graph][runtime.node] hardware_transfer.summary forwarded=3609 download=0 upload=0
[2026-10-26 19:09:12.189] [info] [graph][runtime.node] hardware_transfer.summary forwarded=37 download=0 upload=0
[2026-10-26 19:09:12.189] [info] [graph][runtime.node] hardware_transfer.summary forwarded=42 download=0 upload=0
[2026-10-26 19:09:12.190] [info] [graph][runtime.lifecycle] video_decode.zero_copy_summary drm_prime=0 software_frame=0
[2026-10-26 19:09:12.219] [info] [graph][runtime.lifecycle] video_decode.zero_copy_summary drm_prime=0 software_frame=0
[2026-10-26 19:09:12.240] [info] [graph][runtime.lifecycle] video_decode.zero_copy_summary drm_prime=0 software_frame=0
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=933, threads=81, processThreads=129, workers=0, workerProgress=566853, workerProcessCalls=2262890, workerWaits=1696020, workerWakeups=1673991, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=878, logicalProcessors=22, averageSystemMachineCpuPercent=19.280086, averageProcessMachineCpuPercent=2.040408, peakProcessMachineCpuPercent=8.429119, averageProcessSingleCoreCpuPercent=44.888979, peakProcessSingleCoreCpuPercent=185.440613, initialWorkingSetBytes=209633280, workingSetBytes=258899968, peakWorkingSetBytes=269271040, totalPushed=487633, totalPopped=487597, droppedBuffers=0, encodedPacketsPushed=96898, encodedPacketsPopped=96898, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=16354700, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=749, graphPayloadReservations=224456, graphPayloadReleases=224456, graphPayloadPressureFailures=0, lastInputReceivedAtNs=37389296581900
` 
### 源结尾
`	ext
composition-r365-reentry.log
[out#1/rtp @ 0000022a571641c0] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.49    
composition-r365-source0.log
[out#1/rtp @ 000001ad6fc9df00] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.49    
composition-r365-source1.log
[out#0/rtp @ 000001991d019f00] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
composition-r365-source2.log
[out#0/rtp @ 000001eeee44b600] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.47    
` 
### CLI stderr
`	ext
[in @ 0000022ef7b2a140] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000022ef7b2a140] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 41228.057089
[in @ 0000022ef80caa80] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000022ef80caa80] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 11681.247656
[in @ 0000022ef7f3af40] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000022ef7f3af40] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 292.982333
[in @ 0000022ee3b7a400] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000022ee3b7a400] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 292.982333
[in @ 0000022ef792e540] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000022ef792e540] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 41228.057089
[in @ 0000022ee3b7a200] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000022ee3b7a200] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 11681.247656
[aac @ 0000022ef7fc4680] Qavg: 13334.874

` 
### VLC错误与警告
`	ext
faad warning: decoded zero sample
main error: buffer deadlock prevented
main warning: playback too late (113949): up-sampling
d3d11va warning: surface dimensions (1280x768) differ from avcodec dimensions (1280x736)
d3d11va warning: not enough decoding slices in the texture (6/28)
main error: Failed to create video converter
main error: write error: No error
main error: read error: No error
main error: Failed to create video converter
main error: read error: No error
main error: Failed to create video converter
main error: read error: No error
main error: Failed to create video converter
main error: read error: No error
main warning: picture is too late to be displayed (missing 315 ms)
main error: read error: No error
main warning: picture is too late to be displayed (missing 282 ms)
main warning: picture is too late to be displayed (missing 249 ms)
main warning: picture is too late to be displayed (missing 215 ms)
main warning: picture is too late to be displayed (missing 182 ms)
direct3d11 error: SetThumbNailClip failed: 0x800706f4
` 
### 清理清单
- composition-r365-cli-error.log (1964 bytes)
- composition-r365-cli.log (1089341 bytes)
- composition-r365-monitor.log (20914 bytes)
- composition-r365-native-2026-10-26-19h04m53s201.png (801944 bytes)
- composition-r365-native-2026-10-26-19h06m31s679.png (12152 bytes)
- composition-r365-native-2026-10-26-19h07m09s184.png (317347 bytes)
- composition-r365-native-2026-10-26-19h08m56s844.png (12152 bytes)
- composition-r365-reentry-stdout.log (540 bytes)
- composition-r365-reentry.log (29521 bytes)
- composition-r365-source0-stdout.log (540 bytes)
- composition-r365-source0.log (29589 bytes)
- composition-r365-source1-stdout.log (335 bytes)
- composition-r365-source1.log (28926 bytes)
- composition-r365-source2-stdout.log (335 bytes)
- composition-r365-source2.log (28925 bytes)
- composition-r365-vlc.log (42274 bytes)
本轮16项产物已按清单删除，指定120秒源及构建产物保留。
