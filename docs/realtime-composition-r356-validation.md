# r356：定容释放数组后的三源 TS/UDP 失源与重入


实际停止命令（观测结束后caller stop，不是源结束自动退出）：

~~~~powershell
$target=Get-CimInstance Win32_Process -Filter "ProcessId=24924"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r356*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R356Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R356Console]::FreeConsole() | Out-Null
if(-not [R356Console]::AttachConsole(24924)){throw 'AttachConsole failed'}
try {
[R356Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R356Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 24924 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R356Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R356Console]::FreeConsole() | Out-Null}
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 40152){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020);$s=$c.GetStream();$b=[Text.Encoding]::ASCII.GetBytes("quit`n");$s.Write($b,0,$b.Length);$c.Dispose()
~~~~

## 结果与边界

初始三源37720/6620/42020和重入34488各3600帧、120秒、自然exit0；CLI24924按调用方停止exit0，主会话17119 exit0、重入会话93517 exit0，VLC40152 RC quit。停止后六个PID查询均无进程。

最终workerErrors/errors/stalledIntervals为0；payload预约=释放=222831，最终字节/对象为0。sender173693个datagram、203347944 payload bytes全部提交，deadline/pressure/partial/ambiguous failures为0；最大提交迟到9527700ns、最大backlog residence55126800ns，delivery_evidence=not_proven，不能宣称NIC交付已验证。

video_commits8486；源0/1各real3415+black5071，源2 real6968+black1518；audio12182次，real10244096+silence2230272 samples。源重入generation2，output_generation始终1，本次不是输出代际purge验证。内部phase约13606ns不替代内容同步证明。

内部CPU均值2.430450%、峰值9.107143%；外部1秒采样279项均值2.406374%、峰值5.459380%，采样口径不同。WS峰值268873728 bytes，private峰值1261252608 bytes，末段约1254707200 bytes；一次短回归不证明长期内存上界。

FFmpeg stderr保留frame properties/range变化警告及AAC Qavg；VLC保留picture too late与SetThumbNailClip错误，不能记为零警告。本次四张原生快照全部实际查看，三格→全黑→左下重入→全黑。


状态：本轮三源正常输出、全部失源黑屏、选中AV源重入及caller-stop阶段观察通过；完整目标未通过。内容级A/V、音频实际静音、NIC交付、长时内存及全矩阵不由本次证明。

Release realtime 产物6524928字节，2026-10-26 15:09:02；本轮仅执行realtime。初始FFmpeg PID 37720/6620/42020，CLI 24924，VLC 40152，重入FFmpeg 34488。主监控会话17119，重入会话93517。

输出HEVC CBR8000kbps、1280×720/30、GOP60，AAC CBR192kbps、44100Hz双声道，MPEGTS/UDP；源为指定120秒连续H264/AAC文件，未循环或降规格。

VLC原生快照已实际查看：15:10:29三格；15:12:07全黑；15:12:43左下选中源重入，其余黑。未使用系统截图。内部generation=2的phase约13606ns仅是内部telemetry，不替代内容A/V证明。

## 实际命令

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r356-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r356-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r356-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r356-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r356-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r356-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r356 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r356-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r356-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r356-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r356-native- --snapshot-format=png udp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r356-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r356-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r356-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($source0.Id)"
$source0.WaitForExit()
Write-Output "REENTRY_EXIT=$($source0.ExitCode)"
exit $source0.ExitCode
~~~~

下面同一快照命令已实际执行四次；第四次15:14:28确认再次全黑：

~~~~powershell
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 40152){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
$s=$c.GetStream()
$b=[Text.Encoding]::ASCII.GetBytes("snapshot`n")
$s.Write($b,0,$b.Length)
$s.Flush()
$c.Dispose(); Start-Sleep -Seconds 2; Get-ChildItem 'D:/Code/MyCode/MediaTranscode/out/acceptance' -Filter 'composition-r356-native-*.png' | Select-Object FullName,Length
~~~~


## 关键原始日志与清理前清单

~~~~text
[2026-10-26 15:14:45.281] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=30578 committed_datagrams=173693 committed_payload_bytes=203347944 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=79296 pacing_reserved=173693 pacing_cancelled=0 pacing_submitted=173693 pacing_maximum_submit_lateness_ns=9527700 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=79296 backlog_max_residence_ns=55126800 last_materialized_sequence=173693 last_scheduled_sequence=173693 last_submitted_sequence=173693 last_committed_sequence=173693 submitted_service_generation=1 submitted_service_sequence=173693 submitted_service_completed_ns=288487091900 submitted_service_duration_ns=34560 submitted_service_available_ns=288487126460 submitted_scope_available_ns=288487126460 shared_scope_charged_datagrams=173693 shared_scope_charged_wire_bytes=208211348 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2154 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2154 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=188010 endpoint_1_target_effective_socket_bytes=188010 endpoint_1_api_requested_socket_bytes=188010 endpoint_1_effective_socket_bytes=188010 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=173693 endpoint_1_committed_payload_bytes=203347944
[2026-10-26 15:14:45.523] [info] [graph][runtime.node] continuous_audio_aggregate.summary reason=stop output_group=composition-r356.output audio_commits=12182 real_samples=10244096 silence_samples=2230272
[2026-10-26 15:14:45.527] [info] [graph][runtime.node] continuous_aggregate.summary reason=stop output_group=composition-r356.output output_generation=1 video_commits=8486 source[0].group=composition-r356.source.0 source[0].real_frames=3415 source[0].black_frames=5071 source[0].contribution_generation=0 source[1].group=composition-r356.source.1 source[1].real_frames=3415 source[1].black_frames=5071 source[1].contribution_generation=0 source[2].group=composition-r356.source.2 source[2].real_frames=6968 source[2].black_frames=1518 source[2].contribution_generation=0
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=941, threads=81, processThreads=129, workers=0, workerProgress=561858, workerProcessCalls=2321948, workerWaits=1760073, workerWakeups=1738537, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=827, logicalProcessors=22, averageSystemMachineCpuPercent=17.443242, averageProcessMachineCpuPercent=2.430450, peakProcessMachineCpuPercent=9.107143, averageProcessSingleCoreCpuPercent=53.469902, peakProcessSingleCoreCpuPercent=200.357143, initialWorkingSetBytes=207831040, workingSetBytes=260513792, peakWorkingSetBytes=268873728, totalPushed=483766, totalPopped=483730, droppedBuffers=0, encodedPacketsPushed=94860, encodedPacketsPopped=94860, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=16439016, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=760, graphPayloadReservations=222831, graphPayloadReleases=222831, graphPayloadPressureFailures=0, lastInputReceivedAtNs=23324048598200
[in @ 00000206fb917000] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000206fb917000] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 29477.680189
[in @ 00000206fb967840] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000206fb967840] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 38836.179356
[in @ 00000206ff855780] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000206ff855780] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 36763.425733
[in @ 00000206ffba2580] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000206ffba2580] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 29477.680189
[in @ 00000206ffba3e00] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000206ffba3e00] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 36763.425733
[in @ 00000206ff615e80] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000206ff615e80] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 38836.179356
[aac @ 00000206ff56dcc0] Qavg: 12226.048
faad warning: decoded zero sample
main error: buffer deadlock prevented
main warning: playback too late (113991): up-sampling
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
main error: read error: No error
main warning: buffer too late (-108097 us): dropped
main warning: picture is too late to be displayed (missing 450 ms)
main warning: picture is too late to be displayed (missing 417 ms)
main warning: picture is too late to be displayed (missing 384 ms)
main warning: picture is too late to be displayed (missing 351 ms)
main warning: picture is too late to be displayed (missing 318 ms)
direct3d11 error: SetThumbNailClip failed: 0x800706f4
~~~~

composition-r356-cli-error.log (1968 bytes)
composition-r356-cli.log (1039978 bytes)
composition-r356-monitor.log (20457 bytes)
composition-r356-native-2026-10-26-15h10m29s894.png (821804 bytes)
composition-r356-native-2026-10-26-15h12m07s094.png (12152 bytes)
composition-r356-native-2026-10-26-15h12m43s032.png (314437 bytes)
composition-r356-native-2026-10-26-15h14m28s228.png (12152 bytes)
composition-r356-reentry-stdout.log (540 bytes)
composition-r356-reentry.log (29521 bytes)
composition-r356-source0-stdout.log (540 bytes)
composition-r356-source0.log (29521 bytes)
composition-r356-source1-stdout.log (335 bytes)
composition-r356-source1.log (28926 bytes)
composition-r356-source2-stdout.log (335 bytes)
composition-r356-source2.log (28926 bytes)
composition-r356-vlc.log (42373 bytes)

清理完成：按上述16项精确清单逐项删除，r356目录残留检查为空；指定120秒源及Release产物保留。本轮未在远程创建文件或进程。六个测试PID均已退出，无强制终止。
