# r358 Release realtime 回归记录

状态：运行结束；阶段画面已观察，重入interval miss仍存在，完整验收未通过。验证第八 shared几何与Coordinator pending单项引用；cadence链路不证明非cadence Prepared入口通过。

产物：realtime 6566400 bytes，2026-10-26 16:34:17。源为指定120秒连续文件124427809 bytes。三源H264/AAC→HEVC CBR8000kbps 1280×720/30、AAC CBR192kbps 44100Hz双声道、MPEGTS/UDP；逐源等待100/150/200ms，准备存储2GiB，出口50Mbps/100ms。VLC原生RC快照；只运行realtime。端口预检为空，r358产物未存在。

## 实际启动命令

```powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r358-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r358-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r358-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r358-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r358-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r358-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r358 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r358-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r358-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r358-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r358-native- --snapshot-format=png udp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r358-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

## 实际选中AV源完整120秒重入命令

```powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r358-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r358-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($source0.Id)"
$source0.WaitForExit()
Write-Output "REENTRY_EXIT=$($source0.ExitCode)"
exit $source0.ExitCode
```

## 快照及停止命令

四次均使用同一快照命令，实际查看1280×720图像后记录时刻和内容；无系统截图。

```powershell
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 15516){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
$s=$c.GetStream()
$b=[Text.Encoding]::ASCII.GetBytes("snapshot`n")
$s.Write($b,0,$b.Length)
$s.Flush()
$c.Dispose(); Start-Sleep -Seconds 2; Get-ChildItem 'D:/Code/MyCode/MediaTranscode/out/acceptance' -Filter 'composition-r358-native-*.png' | Select-Object FullName,Length
```

```powershell
$target=Get-CimInstance Win32_Process -Filter "ProcessId=44860"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r358*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R358Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R358Console]::FreeConsole() | Out-Null
if(-not [R358Console]::AttachConsole(44860)){throw 'AttachConsole failed'}
try {
[R358Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R358Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 44860 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R358Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R358Console]::FreeConsole() | Out-Null}
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 15516){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020);$s=$c.GetStream();$b=[Text.Encoding]::ASCII.GetBytes("quit`n");$s.Write($b,0,$b.Length);$c.Dispose()
```



## 结果与边界

初始FFmpeg PID34924/44208/41192、重入44144均3600帧/120秒自然exit0。CLI44860 caller_stop exit0；主会话52247、重入会话26327均exit0；VLC15516经RC quit，六个精确PID查询无残留。全失源期间CLI按持续合屏契约继续运行，caller_stop不算源驱动自然结束。

VLC原生快照实际查看：16:36:53三格、16:38:35全黑、16:39:29左下重入、16:40:57全黑。不证明两帧之间连续性，也不替代内容级AV或静音。

最终payload224041预约=释放，字节/对象0；workerErrors/errors/stalledIntervals均0。内部CPU均值2.131042%、峰值8.879493%，WS峰值268087296。外部284个约1秒样本CPU均值2.103781%、峰值4.640454%，WS峰值268087296，private峰值1262432256、最后1170661376（最后样本含停止过程）；短程趋势不能替代长期稳定性。内部generation2 phase15873ns，不是内容同步证明。

source2 generation2 first_interval_miss_cursor4303，target149093607901ns、steady28394318044700ns，capacity7，before/after/expired/channel_depth均0，examined不可用。该指标仅在最终summary暴露；运行中查询未出现日志不等于没有miss。与r357一样，原因未定位，不判重入连续性PASS。

发送174289 datagrams、203570348 payload bytes，deadline/pressure/partial/ambiguous失败均0；最大提交迟到7921040ns、最大backlog residence51674900ns，delivery_evidence=not_proven。VLC存在Failed to create video converter、read error以及迟到/丢帧；FFmpeg range变化警告保留。

本轮只覆盖当前cadence链路和pending回归的有限观察，不证明新Prepared几何路径执行，不证明完整G、非cadence/四源、内容AV/静音、直接C API、NIC交付或后续RKMPP。未降低源规格，未运行local CLI，未修改外部FFmpeg。

## 关键原始证据

```text
[2026-10-26 16:41:13.436] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=31069 committed_datagrams=174289 committed_payload_bytes=203570348 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=79296 pacing_reserved=174289 pacing_cancelled=0 pacing_submitted=174289 pacing_maximum_submit_lateness_ns=7921040 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=79296 backlog_max_residence_ns=51674900 last_materialized_sequence=174289 last_scheduled_sequence=174289 last_submitted_sequence=174289 last_committed_sequence=174289 submitted_service_generation=1 submitted_service_sequence=174289 submitted_service_completed_ns=292960844800 submitted_service_duration_ns=34560 submitted_service_available_ns=292960879360 submitted_scope_available_ns=292960879360 shared_scope_charged_datagrams=174289 shared_scope_charged_wire_bytes=208450440 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2154 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2154 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=188010 endpoint_1_target_effective_socket_bytes=188010 endpoint_1_api_requested_socket_bytes=188010 endpoint_1_effective_socket_bytes=188010 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=174289 endpoint_1_committed_payload_bytes=203570348
[2026-10-26 16:41:13.651] [info] [graph][runtime.node] continuous_audio_aggregate.summary reason=stop output_group=composition-r358.output audio_commits=12378 real_samples=10245120 silence_samples=2429952
[2026-10-26 16:41:13.652] [info] [graph][runtime.node] continuous_aggregate.summary reason=stop output_group=composition-r358.output output_generation=1 video_commits=8623 source[0].group=composition-r358.source.0 source[0].real_frames=3417 source[0].black_frames=5206 source[0].contribution_generation=0 source[1].group=composition-r358.source.1 source[1].real_frames=3417 source[1].black_frames=5206 source[1].contribution_generation=0 source[2].group=composition-r358.source.2 source[2].real_frames=6970 source[2].black_frames=1653 source[2].contribution_generation=0 source[2].first_interval_miss_cursor=4303 source[2].first_interval_miss_target_ns=149093607901 source[2].first_interval_miss_steady_ns=28394318044700 source[2].first_interval_miss_generation=2 source[2].first_interval_miss_capacity=7 source[2].first_interval_miss_before=0 source[2].first_interval_miss_after=0 source[2].first_interval_miss_expired=0 source[2].first_interval_miss_channel_depth=0 source[2].first_interval_miss_examined_begin_ns=unavailable source[2].first_interval_miss_examined_end_ns=unavailable
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=928, threads=81, processThreads=129, workers=0, workerProgress=565550, workerProcessCalls=2255925, workerWaits=1690358, workerWakeups=1668472, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=863, logicalProcessors=22, averageSystemMachineCpuPercent=25.793951, averageProcessMachineCpuPercent=2.131042, peakProcessMachineCpuPercent=8.879493, averageProcessSingleCoreCpuPercent=46.882925, peakProcessSingleCoreCpuPercent=195.348837, initialWorkingSetBytes=210190336, workingSetBytes=258314240, peakWorkingSetBytes=268087296, totalPushed=486640, totalPopped=486604, droppedBuffers=0, encodedPacketsPushed=96388, encodedPacketsPopped=96388, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=16152512, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=753, graphPayloadReservations=224041, graphPayloadReleases=224041, graphPayloadPressureFailures=0, lastInputReceivedAtNs=28512756813400
[2026-10-26 16:38:49.397] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1 source_master_ns=149023428120 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-10-26 16:40:47.170] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=5096 source_master_ns=267329097054 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5260136
source0: [out#1/rtp @ 000001e5e7ae8b00] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
source1: [out#0/rtp @ 00000288b604bd80] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.47    
source2: [out#0/rtp @ 00000166ead48800] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.47    
reentry: [out#1/rtp @ 00000154e121a3c0] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
[in @ 000001b8e1da1600] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001b8e1da1600] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 32320.593444
[in @ 000001b8e22bd8c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001b8e22bd8c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 20602.060633
[in @ 000001b8e7dd5ac0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001b8e7dd5ac0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 46323.9105
[in @ 000001b8cdbe3100] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001b8cdbe3100] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 20602.060633
[in @ 000001b8e7cb41c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001b8e7cb2dc0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001b8e7cb41c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 46323.9105
[in @ 000001b8e7cb2dc0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 32320.593444
[aac @ 000001b8e1d93f40] Qavg: 13066.082

direct3d11 error: SetThumbNailClip failed: 0x800706f4
main error: buffer deadlock prevented
main error: Failed to create video converter
main error: read error: No error
main error: write error: No error
main warning: buffer too late (-509560 us): dropped
main warning: picture is too late to be displayed (missing 115 ms)
main warning: picture is too late to be displayed (missing 148 ms)
main warning: picture is too late to be displayed (missing 182 ms)
main warning: picture is too late to be displayed (missing 48 ms)
main warning: picture is too late to be displayed (missing 704 ms)
main warning: picture is too late to be displayed (missing 737 ms)
main warning: picture is too late to be displayed (missing 770 ms)
main warning: picture is too late to be displayed (missing 803 ms)
main warning: picture is too late to be displayed (missing 82 ms)
main warning: picture is too late to be displayed (missing 836 ms)
main warning: playback too late (144888): up-sampling
main warning: playback way too late (369673): flushing buffers
```

## 本轮清理清单（删除前归档）

- composition-r358-cli-error.log (1964 bytes)
- composition-r358-cli.log (1077982 bytes)
- composition-r358-monitor.log (20788 bytes)
- composition-r358-native-2026-10-26-16h36m53s921.png (879094 bytes)
- composition-r358-native-2026-10-26-16h38m35s457.png (12152 bytes)
- composition-r358-native-2026-10-26-16h39m29s497.png (327482 bytes)
- composition-r358-native-2026-10-26-16h40m57s626.png (12152 bytes)
- composition-r358-reentry-stdout.log (540 bytes)
- composition-r358-reentry.log (29521 bytes)
- composition-r358-source0-stdout.log (540 bytes)
- composition-r358-source0.log (29520 bytes)
- composition-r358-source1-stdout.log (335 bytes)
- composition-r358-source1.log (28926 bytes)
- composition-r358-source2-stdout.log (335 bytes)
- composition-r358-source2.log (28926 bytes)
- composition-r358-vlc.log (42847 bytes)

## 重入代际诊断

```text
[2026-10-26 16:38:48.358] [info] [graph][runtime.node] rtp_binder_boundary stage=deadline_established node=51 group=composition-r358.source.2 buffered=0 phase=2 preserves_output=1 raw_ssrc=3165709378 raw_generation=1 raw_sequence=5167 old_generation=1 next_generation=2 transition_sequence=1 published_generation=1
[2026-10-26 16:38:48.896] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=1 generation=2
[2026-10-26 16:40:47.841] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=2 reason=1 observed_at_ns=28512757631100
[2026-10-26 16:40:47.842] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=2 reason=1 observed_at_ns=28512758682100
[2026-10-26 16:40:47.842] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=51 group=composition-r358.source.2 buffered=0 phase=0 preserves_output=1 protocol_generation=2 evidence_revision=94 video_raw_generation=1 video_ssrc=1396658626 audio_raw_generation=1 audio_ssrc=3165709378
[2026-10-26 16:40:47.842] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=50 group=composition-r358.source.2 buffered=0 phase=0 preserves_output=1 protocol_generation=2 evidence_revision=94 video_raw_generation=1 video_ssrc=1396658626 audio_raw_generation=1 audio_ssrc=3165709378
[2026-10-26 16:41:13.652] [info] [graph][runtime.node] continuous_aggregate.summary reason=stop output_group=composition-r358.output output_generation=1 video_commits=8623 source[0].group=composition-r358.source.0 source[0].real_frames=3417 source[0].black_frames=5206 source[0].contribution_generation=0 source[1].group=composition-r358.source.1 source[1].real_frames=3417 source[1].black_frames=5206 source[1].contribution_generation=0 source[2].group=composition-r358.source.2 source[2].real_frames=6970 source[2].black_frames=1653 source[2].contribution_generation=0 source[2].first_interval_miss_cursor=4303 source[2].first_interval_miss_target_ns=149093607901 source[2].first_interval_miss_steady_ns=28394318044700 source[2].first_interval_miss_generation=2 source[2].first_interval_miss_capacity=7 source[2].first_interval_miss_before=0 source[2].first_interval_miss_after=0 source[2].first_interval_miss_expired=0 source[2].first_interval_miss_channel_depth=0 source[2].first_interval_miss_examined_begin_ns=unavailable source[2].first_interval_miss_examined_end_ns=unavailable
```

清理完成：已按上述16项清单逐文件删除，r358目录残留为0；指定复用源与正式Release产物保留。本轮未使用远程环境，无远程产物。
