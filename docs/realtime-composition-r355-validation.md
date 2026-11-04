# r355：TS/UDP三源合屏与选中音视频源重入

以下VLC原生快照命令实际执行四次：

~~~~powershell
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 29680){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
$s=$c.GetStream()
$b=[Text.Encoding]::ASCII.GetBytes("snapshot`n")
$s.Write($b,0,$b.Length)
$s.Flush()
$c.Dispose(); Start-Sleep -Seconds 2; Get-ChildItem 'D:/Code/MyCode/MediaTranscode/out/acceptance' -Filter 'composition-r355-native-*.png' | Select-Object FullName,Length
~~~~

全部源自然结束且末次黑屏核验后的实际停止命令：

~~~~powershell
$target=Get-CimInstance Win32_Process -Filter "ProcessId=44220"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r355*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R355Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R355Console]::FreeConsole() | Out-Null
if(-not [R355Console]::AttachConsole(44220)){throw 'AttachConsole failed'}
try {
[R355Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R355Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 44220 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R355Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R355Console]::FreeConsole() | Out-Null}
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 29680){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020);$s=$c.GetStream();$b=[Text.Encoding]::ASCII.GetBytes("quit`n");$s.Write($b,0,$b.Length);$c.Dispose()
~~~~

状态：本轮TS/UDP三源失源/选中AV重入观察通过，完整门禁仍未完成；沿用r341原规格，使用r353全量重建6522880字节/2026-10-26 14:28:31的Release realtime。原120秒源、逐源等待100/150/200ms、2GiB准备存储、HEVC CBR8000kbps 1280×720/30、AAC CBR192kbps 44100Hz双声道。三源自然结束后，同一选中AV源完整120秒重入。只realtime，不改源码，不用旧运行替代当前证据。

## 启动命令

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r355-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r355-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r355-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r355-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r355-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r355-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r355 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r355-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r355-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r355-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r355-native- --snapshot-format=png udp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r355-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r355-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r355-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($source0.Id)"
$source0.WaitForExit()
Write-Output "REENTRY_EXIT=$($source0.ExitCode)"
exit $source0.ExitCode
~~~~

## 实测结果

源43476/37780/44424与重入26476均3600帧120秒自然exit0；CLI44220 caller_stop exit0，VLC29680 RC退出，六PID无残留。监控55902及重入70021均exit0。四张1280×720 VLC原生快照已实际查看：三格、全黑、左下格恢复、再次全黑；最后快照在CLI仍运行时取得。UDP产出=提交179014报/207833436字节，sender deadline/pressure/partial/ambiguous为0，worker/errors/stalled为0。payload230773 reserve=release并回零；graphPayloadPressureFailures=2如实保留，MediaGraphPayloadCreditLedger.cpp:668在硬边界登记waiter并返回WouldBlock计数，非worker失败。video_commits9162，各源real+black均9162；audio_commits13152，real10473412+silence2994236=13467648样本。重入输入generation2，输出generation1保持，不等于输出purge验证。内部phase约13606ns不等于内容A/V证明。CPU平均2.081806%、峰6.696429%，WS峰248893440；private后段约1.159GB，非长期无泄漏证明。保留VLC迟到/转换日志及pacing最大lateness40.18796ms，不宣称零抖动或NIC交付保证。

~~~~text
[2026-10-26 14:48:01.197] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 produced_datagrams=179014 produced_ts_payload_bytes=207833436 pending_bytes=0 peak_pending_bytes=92872 access_units=22314 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=33336562 maximum_mux_input_after_emit_ns=65977025 maximum_mux_input_after_dispatch_ns=-34022975 worst_mux_input_ready_ns=161966856900 worst_mux_input_emit_ns=161900879875 worst_mux_input_dispatch_ns=162000879875 maximum_protocol_batch_produced_after_release_ns=39092612 worst_protocol_batch_produced_ns=116561290000 worst_protocol_batch_release_ns=116522197388 worst_protocol_batch_deadline_ns=116588864056 worst_protocol_batch_datagrams=1 exit_reason=completed
[2026-10-26 14:48:01.199] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=33010 committed_datagrams=179014 committed_payload_bytes=207833436 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=79296 pacing_reserved=179014 pacing_cancelled=0 pacing_submitted=179014 pacing_maximum_submit_lateness_ns=40187960 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=79296 backlog_max_residence_ns=74302900 last_materialized_sequence=179014 last_scheduled_sequence=179014 last_submitted_sequence=179014 last_committed_sequence=179014 submitted_service_generation=1 submitted_service_sequence=179014 submitted_service_completed_ns=305889281700 submitted_service_duration_ns=34560 submitted_service_available_ns=305889316260 submitted_scope_available_ns=305889316260 shared_scope_charged_datagrams=179014 shared_scope_charged_wire_bytes=212845828 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2154 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2154 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=188010 endpoint_1_target_effective_socket_bytes=188010 endpoint_1_api_requested_socket_bytes=188010 endpoint_1_effective_socket_bytes=188010 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=179014 endpoint_1_committed_payload_bytes=207833436
[2026-10-26 14:48:01.427] [info] [graph][runtime.node] continuous_audio_aggregate.summary reason=stop output_group=composition-r355.output audio_commits=13152 real_samples=10473412 silence_samples=2994236
[2026-10-26 14:48:01.428] [info] [graph][runtime.node] continuous_aggregate.summary reason=stop output_group=composition-r355.output output_generation=1 video_commits=9162 source[0].group=composition-r355.source.0 source[0].real_frames=3572 source[0].black_frames=5590 source[0].contribution_generation=0 source[1].group=composition-r355.source.1 source[1].real_frames=3572 source[1].black_frames=5590 source[1].contribution_generation=0 source[2].group=composition-r355.source.2 source[2].real_frames=7125 source[2].black_frames=2037 source[2].contribution_generation=0
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=373, threads=81, processThreads=130, workers=0, workerProgress=574353, workerProcessCalls=2221149, workerWaits=1646779, workerWakeups=1623524, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=925, logicalProcessors=22, averageSystemMachineCpuPercent=19.567270, averageProcessMachineCpuPercent=2.081806, peakProcessMachineCpuPercent=6.696429, averageProcessSingleCoreCpuPercent=45.799724, peakProcessSingleCoreCpuPercent=147.321429, initialWorkingSetBytes=190341120, workingSetBytes=245858304, peakWorkingSetBytes=248893440, totalPushed=502497, totalPopped=502461, droppedBuffers=0, encodedPacketsPushed=102414, encodedPacketsPopped=102414, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=4464633, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=200, graphPayloadReservations=230773, graphPayloadReleases=230773, graphPayloadPressureFailures=2, lastInputReceivedAtNs=21706463334000
[2026-10-26 14:47:18.800] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=5007 source_master_ns=264136643290 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5169444
[2026-10-26 14:47:19.845] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=5052 source_master_ns=265181541249 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5214790
[2026-10-26 14:47:20.866] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=5096 source_master_ns=266203219254 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5260136
[in @ 0000022e51aa3300] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000022e51aa3300] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 5253.471422
[in @ 0000022e51aa3400] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000022e51aa3400] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 47638.023222
[in @ 0000022e520542c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000022e520542c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 16505.503611
[in @ 0000022e51f82380] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000022e51f82380] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 5253.471422
[in @ 0000022e5b0926c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000022e5b0926c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 47638.023222
[in @ 0000022e51f96d80] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000022e51f96d80] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 16505.503611
[aac @ 0000022e52440e40] Qavg: 15127.373
[out#0/rtp @ 000001c613266b80] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
[out#1/rtp @ 000001c615043500] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.49    
frame= 3589 fps= 30 q=-1.0 size=  118528KiB time=00:01:59.63 bitrate=8116.3kbits/s speed=   1x elapsed=0:01:59.12    
[out#0/rtp @ 0000025f7606c940] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
frame= 3589 fps= 30 q=-1.0 size=  118528KiB time=00:01:59.63 bitrate=8116.3kbits/s speed=   1x elapsed=0:01:59.12    
[out#0/rtp @ 0000025bed49b300] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
[out#0/rtp @ 0000027bd9d98b80] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
[out#1/rtp @ 0000027bdbbe4500] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
seconds=1.0097731 cpu=4.1497846487403 ws=237686784 private=1154478080
seconds=2.0308871 cpu=1.599745696634 ws=238280704 private=1154478080
seconds=3.0628677 cpu=1.44525708402588 ws=238538752 private=1154490368
seconds=4.0940515 cpu=3.71924701758045 ws=238624768 private=1154502656
seconds=5.1431942 cpu=3.11401438006999 ws=238747648 private=1154506752
seconds=6.1775695 cpu=2.74649741820893 ws=238747648 private=1154506752
seconds=7.2115264 cpu=2.40415771155012 ws=239312896 private=1155076096
seconds=8.2413068 cpu=2.6897835340781 ws=239734784 private=1155502080
seconds=9.2754019 cpu=2.88460369404569 ws=241684480 private=1157472256
seconds=10.3929573 cpu=2.22431519237924 ws=241684480 private=1157472256
seconds=11.4182812 cpu=2.84001164722857 ws=241741824 private=1157472256
seconds=12.4570284 cpu=3.21355203828051 ws=241807360 private=1157480448
seconds=13.4997461 cpu=1.97527968587192 ws=241864704 private=1157480448
seconds=14.5369961 cpu=2.12263634172528 ws=241934336 private=1157484544
seconds=15.5708723 cpu=2.61043211591836 ws=241934336 private=1157484544
seconds=16.6077755 cpu=3.3562570125771 ws=243036160 private=1158590464
seconds=17.6403226 cpu=2.47622426310449 ws=243228672 private=1158598656
seconds=18.6725969 cpu=1.72005462290225 ws=243294208 private=1158602752
seconds=19.7034179 cpu=2.75596741908545 ws=243322880 private=1158602752
seconds=20.7314731 cpu=1.93436730210243 ws=243322880 private=1158602752
seconds=21.7564791 cpu=1.87083162085259 ws=244289536 private=1159512064
seconds=22.781904 cpu=2.2163761312284 ws=243494912 private=1158602752
seconds=23.8216897 cpu=2.32237539646172 ws=243503104 private=1158610944
seconds=24.862133 cpu=2.79874147700487 ws=243503104 private=1158610944
seconds=25.8900647 cpu=2.14187824488198 ws=243503104 private=1158610944
seconds=26.9151358 cpu=1.93999846804418 ws=243503104 private=1158610944
seconds=27.9539516 cpu=2.05106797391975 ws=243515392 private=1158623232
seconds=28.9873593 cpu=2.54288884153941 ws=243515392 private=1158623232
seconds=30.0151585 cpu=3.04047716713538 ws=243515392 private=1158623232
seconds=31.0448619 cpu=2.06921897915634 ws=243511296 private=1158623232
seconds=32.0772368 cpu=2.88941017982377 ws=243568640 private=1158623232
seconds=33.1071747 cpu=2.41353916051196 ws=244109312 private=1159176192
seconds=34.1250166 cpu=3.1399991759749 ws=245899264 private=1161019392
seconds=35.1537295 cpu=2.55449397892349 ws=245960704 private=1161019392
seconds=36.1784926 cpu=2.00988803256976 ws=245964800 private=1161019392
seconds=37.2168542 cpu=2.73595353575198 ws=245964800 private=1161019392
seconds=38.2430416 cpu=2.56078072006235 ws=246083584 private=1161019392
seconds=39.2787345 cpu=2.88015351409143 ws=246136832 private=1161019392
seconds=40.3023265 cpu=2.42850222993678 ws=246136832 private=1161019392
seconds=41.3301344 cpu=2.28033857299599 ws=246255616 private=1161019392
seconds=42.3655094 cpu=3.29261466530571 ws=246317056 private=1161019392
seconds=43.3976735 cpu=2.82119075656847 ws=246317056 private=1161019392
seconds=44.4275721 cpu=2.55155304521331 ws=246317056 private=1161019392
seconds=45.4608914 cpu=2.19944335959588 ws=246317056 private=1161019392
seconds=46.4960446 cpu=2.40137928815316 ws=246374400 private=1161019392
seconds=47.5187997 cpu=3.3332426395047 ws=246374400 private=1161019392
seconds=48.5479876 cpu=2.20827243764455 ws=246374400 private=1161019392
seconds=49.5835596 cpu=2.88048976358432 ws=246652928 private=1161019392
seconds=50.6175526 cpu=1.92325901977708 ws=246648832 private=1161019392
seconds=51.644453 cpu=3.45811177367969 ws=246648832 private=1161019392
seconds=52.6881031 cpu=2.51793288678927 ws=246648832 private=1161019392
seconds=53.720082 cpu=2.47758765399 ws=246648832 private=1161019392
seconds=54.7483682 cpu=2.76276107849068 ws=246648832 private=1161019392
seconds=55.7750038 cpu=3.18228342612068 ws=246706176 private=1161019392
seconds=56.7991893 cpu=2.84316836957935 ws=246706176 private=1161019392
seconds=57.8382621 cpu=2.73408089491813 ws=246706176 private=1161019392
seconds=58.8642838 cpu=2.56119427989769 ws=246763520 private=1161019392
seconds=59.8866395 cpu=2.57037830286554 ws=246767616 private=1161019392
seconds=60.9184926 cpu=3.23502268086241 ws=246706176 private=1160921088
seconds=61.9571673 cpu=2.46161592442579 ws=246706176 private=1160921088
seconds=62.9955705 cpu=2.87263612578866 ws=246824960 private=1160921088
seconds=64.0353382 cpu=1.91257755327115 ws=246824960 private=1160921088
seconds=65.068198 cpu=1.85660593660789 ws=246882304 private=1160921088
seconds=66.1012663 cpu=2.06247913926097 ws=246943744 private=1160921088
seconds=67.1304763 cpu=2.96730237048542 ws=246947840 private=1160921088
seconds=68.1713161 cpu=2.5929673676618 ws=247005184 private=1160921088
seconds=69.2029108 cpu=2.82274794372424 ws=247066624 private=1160921088
seconds=70.2311628 cpu=3.3154235626003 ws=247119872 private=1160921088
seconds=71.262068 cpu=3.72025213640139 ws=247123968 private=1160921088
seconds=72.2955477 cpu=3.02376524667103 ws=247181312 private=1160921088
seconds=73.3286906 cpu=3.29972834260479 ws=247238656 private=1160921088
seconds=74.3545065 cpu=2.07706062869743 ws=247296000 private=1160921088
seconds=75.3821715 cpu=2.418877216355 ws=247353344 private=1160921088
seconds=76.4185999 cpu=3.01516245598826 ws=247353344 private=1160921088
seconds=77.4610647 cpu=2.18014773518232 ws=248262656 private=1161830400
seconds=78.4860104 cpu=2.28670650552511 ws=247353344 private=1160921088
seconds=79.5134003 cpu=2.14300777675013 ws=247410688 private=1160921088
seconds=80.5497429 cpu=3.15247627043939 ws=247410688 private=1160921088
seconds=81.5772912 cpu=2.35003330478257 ws=247410688 private=1160921088
seconds=82.6158987 cpu=3.00883635059443 ws=247492608 private=1160953856
seconds=83.6448419 cpu=3.10612162777569 ws=247492608 private=1160953856
seconds=84.6749785 cpu=1.79256897492131 ws=247488512 private=1160953856
seconds=85.6998872 cpu=1.94030586688974 ws=247488512 private=1160953856
seconds=86.7287086 cpu=2.41615838720448 ws=247500800 private=1160953856
seconds=87.7502367 cpu=2.57246071751811 ws=247676928 private=1160953856
seconds=88.7845359 cpu=2.47202954601357 ws=247668736 private=1160953856
seconds=89.8219149 cpu=2.53315414047412 ws=247726080 private=1160953856
seconds=90.857719 cpu=2.19416709465357 ws=247971840 private=1161199616
seconds=91.8878917 cpu=2.75770178234106 ws=248029184 private=1161199616
seconds=92.9206581 cpu=2.68200666059272 ws=248029184 private=1161199616
seconds=93.9566335 cpu=2.05669151814012 ws=248029184 private=1161199616
seconds=94.9863435 cpu=3.37970267003681 ws=248037376 private=1161207808
seconds=96.0024231 cpu=2.37655861536117 ws=248037376 private=1161207808
seconds=97.0371295 cpu=2.60833762733431 ws=248037376 private=1161207808
seconds=98.0687077 cpu=2.34085280909652 ws=248037376 private=1161207808
seconds=99.1002929 cpu=3.16701466301132 ws=248098816 private=1161207808
seconds=100.1235674 cpu=2.7068849694157 ws=248463360 private=1161580544
seconds=101.1592391 cpu=2.19444759640269 ws=248459264 private=1161580544
seconds=102.185473 cpu=2.49145753401653 ws=248459264 private=1161580544
seconds=103.2152532 cpu=2.7587528784386 ws=248459264 private=1161580544
seconds=104.2424744 cpu=2.1433597217956 ws=248459264 private=1161580544
seconds=105.2818519 cpu=2.66494739749164 ws=248455168 private=1161580544
seconds=106.3178195 cpu=2.33093460381651 ws=248459264 private=1161580544
seconds=107.3544821 cpu=3.35703596943078 ws=248459264 private=1161580544
seconds=108.3759646 cpu=2.22493020950164 ws=248889344 private=1161949184
seconds=109.4080076 cpu=2.68388658576858 ws=248893440 private=1161949184
seconds=110.4457081 cpu=1.91638759317969 ws=248893440 private=1161949184
seconds=111.4726381 cpu=2.76640967827321 ws=248893440 private=1161949184
seconds=112.5115602 cpu=2.93956329615786 ws=248893440 private=1161949184
seconds=113.5424256 cpu=2.2735751922608 ws=248893440 private=1161949184
seconds=114.5700346 cpu=2.28077994645824 ws=248893440 private=1161949184
seconds=115.6111178 cpu=3.47922153667361 ws=248893440 private=1161949184
seconds=116.6483095 cpu=3.6292274084478 ws=249802752 private=1162858496
seconds=117.6827247 cpu=3.02103062677347 ws=248893440 private=1161949184
seconds=118.7223158 cpu=2.32281011954864 ws=248893440 private=1161949184
seconds=119.7495578 cpu=2.62729097560617 ws=239779840 private=1155461120
seconds=120.7840033 cpu=1.37315551709061 ws=239771648 private=1155461120
seconds=121.8084026 cpu=1.45595303777275 ws=239771648 private=1155461120
seconds=122.8427081 cpu=1.23600724438678 ws=239771648 private=1155461120
seconds=123.8862985 cpu=1.36112266407831 ws=239771648 private=1155461120
seconds=124.9153051 cpu=1.10433075586069 ws=239771648 private=1155461120
seconds=125.9469398 cpu=1.30805198601968 ws=239771648 private=1155461120
seconds=126.9753856 cpu=1.31210786040627 ws=239771648 private=1155461120
seconds=127.9882843 cpu=1.12189267926164 ws=239771648 private=1155461120
seconds=129.0233317 cpu=1.30373915067254 ws=239771648 private=1155461120
seconds=130.0444729 cpu=1.32149385235052 ws=239771648 private=1155461120
seconds=131.0736123 cpu=1.38023531647367 ws=239771648 private=1155461120
seconds=132.1025973 cpu=1.58750878513556 ws=239771648 private=1155461120
seconds=133.1281411 cpu=1.45432820395121 ws=239771648 private=1155461120
seconds=134.1613442 cpu=1.44354703613187 ws=239771648 private=1155461120
seconds=135.1985269 cpu=1.09562532846301 ws=239771648 private=1155461120
seconds=136.2385405 cpu=1.22922343602918 ws=239771648 private=1155461120
seconds=137.2731749 cpu=1.44155005161948 ws=239775744 private=1155461120
seconds=138.2985238 cpu=1.45460464504059 ws=239771648 private=1155461120
seconds=139.3320152 cpu=1.23698086980606 ws=239771648 private=1155461120
seconds=140.3686394 cpu=0.959188664337738 ws=239771648 private=1155461120
seconds=141.3938156 cpu=1.52412824254015 ws=239771648 private=1155461120
seconds=142.4184033 cpu=1.317048621784 ws=239771648 private=1155461120
seconds=143.4577511 cpu=1.16167693204947 ws=239775744 private=1155461120
seconds=144.4940921 cpu=1.2335795755539 ws=239779840 private=1155461120
seconds=145.5383708 cpu=1.15619169828548 ws=239771648 private=1155461120
seconds=146.5754438 cpu=1.36967652754874 ws=239771648 private=1155461120
seconds=147.6066783 cpu=1.23968805437471 ws=242233344 private=1156943872
seconds=148.6423319 cpu=2.26306363440442 ws=243863552 private=1158483968
seconds=149.6736912 cpu=2.27248641671235 ws=244985856 private=1159446528
seconds=150.7085325 cpu=1.78441941686222 ws=244170752 private=1158569984
seconds=151.7433507 cpu=1.64719315387517 ws=244215808 private=1158569984
seconds=152.7777269 cpu=2.12853364709528 ws=244219904 private=1158574080
seconds=153.8132683 cpu=1.37170232445998 ws=244232192 private=1158586368
seconds=154.8437261 cpu=1.92985716022177 ws=244285440 private=1158586368
seconds=155.8771943 cpu=2.33657187252853 ws=244580352 private=1158586368
seconds=156.9066623 cpu=2.20767160584624 ws=244580352 private=1158586368
seconds=157.941621 cpu=1.50972207876507 ws=244645888 private=1158586368
seconds=158.9713223 cpu=1.6553785593409 ws=244645888 private=1158586368
seconds=160.0033731 cpu=2.06451253967518 ws=244654080 private=1158586368
seconds=161.0329192 cpu=2.96633368115062 ws=244645888 private=1158586368
seconds=162.0690903 cpu=2.12484650986171 ws=244645888 private=1158586368
seconds=163.1085853 cpu=1.77643077560828 ws=244707328 private=1158586368
seconds=164.1442285 cpu=1.78303773837446 ws=244715520 private=1158586368
seconds=165.1794009 cpu=1.78384867012579 ws=244715520 private=1158586368
seconds=166.2119121 cpu=2.13237836592427 ws=244715520 private=1158586368
seconds=167.2438725 cpu=1.85822405236057 ws=244715520 private=1158586368
seconds=168.2831875 cpu=2.39176328114718 ws=244723712 private=1158586368
seconds=169.3157468 cpu=2.269845421953 ws=244715520 private=1158586368
seconds=170.3235071 cpu=2.04379860062863 ws=244723712 private=1158586368
seconds=171.3443842 cpu=2.36539023871993 ws=244723712 private=1158586368
seconds=172.378213 cpu=1.78616702213262 ws=244781056 private=1158586368
seconds=173.4084585 cpu=2.6196315697216 ws=244781056 private=1158586368
seconds=174.4412953 cpu=2.06294142325466 ws=244781056 private=1158586368
seconds=175.4832809 cpu=1.6358627744428 ws=244781056 private=1158586368
seconds=176.5167934 cpu=2.06159269305579 ws=244781056 private=1158586368
seconds=177.5459522 cpu=2.20833487769558 ws=244781056 private=1158586368
seconds=178.585677 cpu=2.11758394668908 ws=244781056 private=1158586368
seconds=179.6154526 cpu=2.06907390132552 ws=244781056 private=1158586368
seconds=180.6563602 cpu=2.661030012305 ws=244781056 private=1158586368
seconds=181.6925875 cpu=1.7134929583675 ws=244781056 private=1158586368
seconds=182.7286192 cpu=1.43960582743489 ws=244781056 private=1158598656
seconds=183.7571682 cpu=1.86438724490874 ws=245772288 private=1159516160
seconds=184.7837356 cpu=2.35227879559853 ws=244854784 private=1158598656
seconds=185.8162321 cpu=2.33877086002007 ws=244862976 private=1158598656
seconds=186.8485462 cpu=1.92638690456364 ws=244862976 private=1158598656
seconds=187.8915624 cpu=2.24708877963734 ws=244854784 private=1158598656
seconds=188.9136856 cpu=1.73713714923814 ws=244977664 private=1158598656
seconds=189.9606097 cpu=2.03518270157488 ws=244977664 private=1158598656
seconds=190.9885353 cpu=1.72733141563765 ws=244977664 private=1158598656
seconds=192.0335827 cpu=2.51456623794372 ws=244998144 private=1158619136
seconds=193.0572385 cpu=2.01206215107567 ws=245215232 private=1159036928
seconds=194.0888157 cpu=2.6162497933879 ws=245211136 private=1159036928
seconds=195.1116807 cpu=1.94418262785057 ws=245211136 private=1159036928
seconds=196.1440816 cpu=2.20139993361811 ws=245215232 private=1159036928
seconds=197.1777406 cpu=2.13001052131752 ws=245280768 private=1159036928
seconds=198.2287828 cpu=2.02720862985501 ws=245280768 private=1159036928
seconds=199.2531043 cpu=2.35743633934537 ws=245280768 private=1159036928
seconds=200.2774827 cpu=1.5946477661699 ws=245391360 private=1159036928
seconds=201.315463 cpu=2.18956686627608 ws=245391360 private=1159036928
seconds=202.3415217 cpu=2.56110192242503 ws=245391360 private=1159036928
seconds=203.3699796 cpu=2.20983987067167 ws=245407744 private=1159036928
seconds=204.4062235 cpu=1.71346550924754 ws=245415936 private=1159036928
seconds=205.4384945 cpu=1.58245531190236 ws=245415936 private=1159036928
seconds=206.4765484 cpu=2.05257339544871 ws=245424128 private=1159036928
seconds=207.5045195 cpu=2.14179615113168 ws=245485568 private=1159036928
seconds=208.5398092 cpu=1.71504476652105 ws=246403072 private=1159954432
seconds=209.5755582 cpu=2.19428382043074 ws=245485568 private=1159036928
seconds=210.6089025 cpu=1.9244663793436 ws=245547008 private=1159036928
seconds=211.637886 cpu=2.48479998155282 ws=245547008 private=1159036928
seconds=212.6812636 cpu=1.7017503364249 ws=245547008 private=1159036928
seconds=213.7185386 cpu=1.84870322370022 ws=245665792 private=1159036928
seconds=214.756489 cpu=1.91592619804989 ws=245661696 private=1159036928
seconds=215.7833485 cpu=1.65995976523123 ws=245665792 private=1159036928
seconds=216.814724 cpu=2.47903715166602 ws=245669888 private=1159036928
seconds=217.84241 cpu=2.0041715960995 ws=245665792 private=1159036928
seconds=218.8729558 cpu=2.48103304270241 ws=245665792 private=1159036928
seconds=219.8996608 cpu=1.38350796524275 ws=245665792 private=1159036928
seconds=220.9296039 cpu=2.0687374071265 ws=245661696 private=1159036928
seconds=221.9669242 cpu=1.36934999291399 ws=245886976 private=1159036928
seconds=222.9940228 cpu=2.14361556471259 ws=245895168 private=1159036928
seconds=224.0251615 cpu=1.85970484510344 ws=245886976 private=1159036928
seconds=225.0581867 cpu=2.06256519026043 ws=245886976 private=1159036928
seconds=226.0776832 cpu=2.43825795826219 ws=245886976 private=1159036928
seconds=227.1096141 cpu=1.85827717375616 ws=245895168 private=1159036928
seconds=228.1449581 cpu=1.37196385496466 ws=245895168 private=1159036928
seconds=229.1797109 cpu=2.19639634966657 ws=245903360 private=1159036928
seconds=230.2166814 cpu=2.12320846683156 ws=245895168 private=1159036928
seconds=231.2538228 cpu=1.43806550652329 ws=245895168 private=1159036928
seconds=232.2876052 cpu=1.99235263717885 ws=245895168 private=1159036928
seconds=233.3251013 cpu=1.57448565567883 ws=245895168 private=1159036928
seconds=234.3520631 cpu=2.07474301203978 ws=245895168 private=1159036928
seconds=235.3813232 cpu=2.20811753290281 ws=245899264 private=1159036928
seconds=236.4215336 cpu=2.32142721056501 ws=245895168 private=1159036928
seconds=237.4498274 cpu=1.93391846147119 ws=245956608 private=1159036928
seconds=238.4813258 cpu=1.65249452112136 ws=245956608 private=1159036928
seconds=239.5108082 cpu=1.86269686238796 ws=245956608 private=1159036928
seconds=240.5472349 cpu=1.7131632963703 ws=245956608 private=1159036928
seconds=241.5794561 cpu=1.51372593393744 ws=245956608 private=1159036928
seconds=242.6041372 cpu=1.80211278327559 ws=245956608 private=1159036928
seconds=243.648224 cpu=1.83664196919608 ws=245956608 private=1159036928
seconds=244.6729853 cpu=2.28711798542745 ws=245956608 private=1159036928
seconds=245.7011588 cpu=2.00322133463766 ws=245956608 private=1159036928
seconds=246.726945 cpu=2.42330756111308 ws=245956608 private=1159036928
seconds=247.7443637 cpu=1.67536281232637 ws=245956608 private=1159036928
seconds=248.7818267 cpu=2.19065862852677 ws=245628928 private=1159036928
seconds=249.8147964 cpu=2.13143187593456 ws=245628928 private=1159036928
seconds=250.8462374 cpu=2.41002195428091 ws=245628928 private=1159036928
seconds=251.8903674 cpu=2.17667079073227 ws=245637120 private=1159036928
seconds=252.9294165 cpu=1.57213237302523 ws=245628928 private=1159036928
seconds=253.963116 cpu=1.99251241865655 ws=245628928 private=1159036928
seconds=254.9974161 cpu=1.71668569095003 ws=245628928 private=1159036928
seconds=256.0189683 cpu=2.15525407850387 ws=245628928 private=1159036928
seconds=257.0574037 cpu=1.9150313670319 ws=245628928 private=1159036928
seconds=258.0933852 cpu=1.64534352644856 ws=245628928 private=1159036928
seconds=259.1203055 cpu=2.07482685675011 ws=245628928 private=1159036928
seconds=260.1505387 cpu=1.99921638218316 ws=246554624 private=1159962624
seconds=261.1809832 cpu=2.13665514780716 ws=245690368 private=1159036928
seconds=262.2131904 cpu=1.78897309483114 ws=245690368 private=1159036928
seconds=263.240873 cpu=2.00417822672973 ws=245690368 private=1159036928
seconds=264.2712182 cpu=1.65434405337692 ws=245690368 private=1159036928
seconds=265.3085876 cpu=1.71160647481813 ws=245686272 private=1159036928
seconds=266.338361 cpu=1.93113976690051 ws=246611968 private=1159962624
seconds=267.3640272 cpu=1.38490918922215 ws=245686272 private=1159036928
seconds=268.3937622 cpu=1.51738068532193 ws=245686272 private=1159036928
seconds=269.4308494 cpu=1.2326919963038 ws=245686272 private=1159036928
seconds=270.4657678 cpu=1.44115446466823 ws=245686272 private=1159036928
seconds=271.4974324 cpu=1.37685692176948 ws=245686272 private=1159036928
seconds=272.5206523 cpu=1.31880920042876 ws=245686272 private=1159036928
seconds=273.5523541 cpu=1.37680727653527 ws=246603776 private=1159954432
seconds=274.5864817 cpu=1.23621987355247 ws=245694464 private=1159036928
seconds=275.6257555 cpu=1.23009845038818 ws=245686272 private=1159036928
seconds=276.6545011 cpu=1.51884003197684 ws=245686272 private=1159036928
seconds=277.6864987 cpu=1.30759201201805 ws=245686272 private=1159036928
seconds=278.7223807 cpu=1.23412617548055 ws=245686272 private=1159036928
seconds=279.7535254 cpu=1.44642868525362 ws=245686272 private=1159036928
seconds=280.793326 cpu=1.63930031829708 ws=245686272 private=1159036928
seconds=281.8203829 cpu=1.45218563131916 ws=245686272 private=1159036928
seconds=282.8475496 cpu=1.5211747031909 ws=245686272 private=1159036928
seconds=283.8740427 cpu=1.31460388597037 ws=246595584 private=1159946240
seconds=284.8964021 cpu=1.59779694623317 ws=245690368 private=1159036928
seconds=285.9170339 cpu=1.25256639163027 ws=245694464 private=1159036928
seconds=286.954795 cpu=1.50564518172823 ws=245686272 private=1159036928
seconds=287.9717644 cpu=1.60626536774139 ws=245686272 private=1159036928
seconds=288.9987469 cpu=1.59060424814705 ws=245686272 private=1159036928
seconds=290.0233054 cpu=1.10912518549563 ws=245686272 private=1159036928
seconds=291.0571825 cpu=1.16782387736065 ws=245686272 private=1159036928
seconds=292.0916036 cpu=1.44184730254179 ws=245686272 private=1159036928
seconds=293.1215021 cpu=1.3792179961953 ws=245690368 private=1159036928
seconds=294.1650796 cpu=1.76948133616417 ws=245686272 private=1159036928
seconds=295.2002325 cpu=1.30360627708409 ws=245686272 private=1159036928
seconds=296.2293758 cpu=0.966161060192681 ws=245686272 private=1159036928
seconds=297.2642406 cpu=1.16670927800075 ws=245686272 private=1159036928
seconds=298.29684 cpu=1.51317151646614 ws=245686272 private=1159036928
seconds=299.3360282 cpu=1.29854420804798 ws=245686272 private=1159036928
seconds=300.3639033 cpu=1.4510296754219 ws=245866496 private=1159073792
seconds=301.4050463 cpu=1.29610612392519 ws=245858304 private=1159073792
seconds=302.4348309 cpu=1.03452790912865 ws=245858304 private=1159073792
seconds=303.4738873 cpu=1.1620027205803 ws=245858304 private=1159073792
seconds=304.5012137 cpu=1.31353756525855 ws=245858304 private=1159073792
seconds=305.5354106 cpu=1.37348559588079 ws=245858304 private=1159073792
wasapi debug: maximum latency: 000 ns
main error: buffer deadlock prevented
main warning: playback too late (89693): up-sampling
main error: Failed to create video converter
main error: write error: No error
main error: read error: No error
main error: Failed to create video converter
main error: read error: No error
main error: Failed to create video converter
main error: read error: No error
main error: Failed to create video converter
main error: read error: No error
main warning: buffer too late (-565743 us): dropped
main warning: picture is too late to be displayed (missing 898 ms)
main error: read error: No error
main warning: picture is too late to be displayed (missing 864 ms)
main warning: picture is too late to be displayed (missing 831 ms)
main warning: picture is too late to be displayed (missing 798 ms)
main warning: picture is too late to be displayed (missing 764 ms)
direct3d11 error: SetThumbNailClip failed: 0x800706f4
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r355-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r355-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r355-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r355-native-2026-10-26-14h43m41s524.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r355-native-2026-10-26-14h45m11s154.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r355-native-2026-10-26-14h45m47s896.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r355-native-2026-10-26-14h47m26s422.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r355-reentry-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r355-reentry.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r355-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r355-source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r355-source1-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r355-source1.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r355-source2-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r355-source2.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r355-vlc.log
~~~~

清理完成：以上16文件逐项删除，本轮前缀无残留；六个精确PID无残留，源与正式构建保留，无远程产物。
