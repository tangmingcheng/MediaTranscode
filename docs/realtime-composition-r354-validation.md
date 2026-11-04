# r354：最新接受状态修正后三源 TS/RTP 合屏复验

快照命令实际执行两次，分别在正常输出与全部失源后：

~~~~powershell
$c=Get-NetTCPConnection -LocalPort 63020 -State Listen -ErrorAction Stop; if($c.OwningProcess -ne 6508){throw 'VLC owner mismatch'}; $tcp=[Net.Sockets.TcpClient]::new('127.0.0.1',63020); try{$stream=$tcp.GetStream(); $bytes=[Text.Encoding]::ASCII.GetBytes("snapshot`n"); $stream.Write($bytes,0,$bytes.Length); $stream.Flush()}finally{$tcp.Dispose()}; Start-Sleep -Seconds 2; Get-ChildItem out/acceptance -Filter 'composition-r354-native-*' | Select-Object FullName,Length
~~~~

观察结束后的实际caller-stop与VLC退出命令：

~~~~powershell
$target=Get-CimInstance Win32_Process -Filter "ProcessId=11940"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r354*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R354Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R354Console]::FreeConsole() | Out-Null
if(-not [R354Console]::AttachConsole(11940)){throw 'AttachConsole failed'}
try {
[R354Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R354Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 11940 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R354Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R354Console]::FreeConsole() | Out-Null}
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 6508){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020);$s=$c.GetStream();$b=[Text.Encoding]::ASCII.GetBytes("quit`n");$s.Write($b,0,$b.Length);$c.Dispose()
~~~~

状态：三源正常输出、全部失源黑屏与正常停止观察通过；完整合屏门禁未完成。沿用r340完整三源120秒规格及逐源等待100/150/200ms，准备存储2GiB，1280×720/30 HEVC CBR8000kbps、AAC CBR192kbps 44100Hz双声道。使用r353已全量重建产物，不改变源码。

单源r353的ProgressTimeout来自MediaRealtimeVideoRunController既有无输入且无输出进展分支，不是SourceCompleted；其失败记录保持。合屏需另外验证持续输出黑屏/静音与caller-stop，不能用单源失源结束替代，也不为使测试成功修改单源策略。

## 执行命令

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r354-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r354-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r354-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r354-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r354-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r354-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r354 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62024 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r354-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r354-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r354-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r354-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r354-native- --snapshot-format=png rtp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r354-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

## 实测结果

源PID38064/29920/28540均3600帧120秒自然exit0；CLI11940在全部源结束后继续输出，最后经caller-stop exit0；VLC6508经RC quit退出。监控会话85204 exit0，五PID均无残留。两张1280×720 VLC原生快照实际查看，先三格实画面+空第四格，再全黑；无系统截图。sender86278报（86240媒体+38RTCP），reserved=submitted，deadline_misses/pressure/partial/ambiguous均0；payload回零，116929 reserve=release。4740视频提交，逐源real3424/3424/3423与black1316/1316/1317各和为4740；音频6805提交，real5031936+silence1936384=6968320样本。静音为生产内部样本证据，尚非外部声音内容验收；内部A/V phase约4535ns，不冒充内容同步证明。CPU平均2.195965%、峰6.896552%，WS峰270315520；采样后段private约1.264GB，停源后约1.247GB；仅本轮趋势，非长期稳定证明。全部失源未触发单源ProgressTimeout。未覆盖重入、全部codec/RC矩阵、直接C API、RKMPP或完整预算；不据此宣布总任务完成。

~~~~text
[2026-10-26 14:40:42.109] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 produced_datagrams=86240 produced_ts_payload_bytes=98773508 pending_bytes=0 peak_pending_bytes=110920 access_units=11545 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=32789115 maximum_mux_input_after_emit_ns=38410215 maximum_mux_input_after_dispatch_ns=-61589785 worst_mux_input_ready_ns=95465683800 worst_mux_input_emit_ns=95427273585 worst_mux_input_dispatch_ns=95527273585 maximum_protocol_batch_produced_after_release_ns=37312052 worst_protocol_batch_produced_ns=59464042500 worst_protocol_batch_release_ns=59426730448 worst_protocol_batch_deadline_ns=59493397116 worst_protocol_batch_datagrams=1 exit_reason=completed
[2026-10-26 14:40:42.110] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=17085 committed_datagrams=86278 committed_payload_bytes=99810980 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=80004 pacing_reserved=86278 pacing_cancelled=0 pacing_submitted=86278 pacing_maximum_submit_lateness_ns=5660920 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=80004 backlog_max_residence_ns=52817300 last_materialized_sequence=86278 last_scheduled_sequence=86278 last_submitted_sequence=86278 last_committed_sequence=86278 submitted_service_generation=1 submitted_service_sequence=86278 submitted_service_completed_ns=163417042500 submitted_service_duration_ns=16640 submitted_service_available_ns=163417059140 submitted_scope_available_ns=163417059140 shared_scope_charged_datagrams=86278 shared_scope_charged_wire_bytes=102226764 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2155 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2155 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=379902 endpoint_1_target_effective_socket_bytes=189951 endpoint_1_api_requested_socket_bytes=189951 endpoint_1_effective_socket_bytes=189951 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=86240 endpoint_1_committed_payload_bytes=99808388 endpoint_2_target_effective_socket_bytes=189951 endpoint_2_api_requested_socket_bytes=189951 endpoint_2_effective_socket_bytes=189951 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=38 endpoint_2_committed_payload_bytes=2592
[2026-10-26 14:40:42.330] [info] [graph][runtime.node] continuous_audio_aggregate.summary reason=stop output_group=composition-r354.output audio_commits=6805 real_samples=5031936 silence_samples=1936384
[2026-10-26 14:40:42.332] [info] [graph][runtime.node] continuous_aggregate.summary reason=stop output_group=composition-r354.output output_generation=1 video_commits=4740 source[0].group=composition-r354.source.0 source[0].real_frames=3424 source[0].black_frames=1316 source[0].contribution_generation=0 source[1].group=composition-r354.source.1 source[1].real_frames=3424 source[1].black_frames=1316 source[1].contribution_generation=0 source[2].group=composition-r354.source.2 source[2].real_frames=3423 source[2].black_frames=1317 source[2].contribution_generation=0
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=896, threads=82, processThreads=130, workers=0, workerProgress=347237, workerProcessCalls=1358546, workerWaits=1011291, workerWakeups=999227, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=478, logicalProcessors=22, averageSystemMachineCpuPercent=19.916492, averageProcessMachineCpuPercent=2.195965, peakProcessMachineCpuPercent=6.896552, averageProcessSingleCoreCpuPercent=48.311225, peakProcessSingleCoreCpuPercent=151.724138, initialWorkingSetBytes=209592320, workingSetBytes=245424128, peakWorkingSetBytes=270315520, totalPushed=297327, totalPopped=297303, droppedBuffers=0, encodedPacketsPushed=52991, encodedPacketsPopped=52991, backpressureItems=124, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=15305056, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=725, graphPayloadReservations=116929, graphPayloadReleases=116929, graphPayloadPressureFailures=0, lastInputReceivedAtNs=21263072839500
[2026-10-26 14:39:55.718] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4830 source_master_ns=117657693712 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4988060
[2026-10-26 14:39:56.763] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4875 source_master_ns=118702591672 raw_phase_ns=4536 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5033406
[2026-10-26 14:39:57.784] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4919 source_master_ns=119724269676 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5078752
[in @ 0000023cc91576c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000023cc91576c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 14202.066844
[in @ 0000023cc924f8c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000023cc924f8c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 39274.862633
[in @ 0000023ccf2b47c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000023ccf2b47c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 31054.9094
[in @ 0000023cc9021700] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000023ca06ea700] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000023cc901fd80] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000023cc901fd80] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 39274.862633
[in @ 0000023ca06ea700] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 14202.066844
[in @ 0000023cc9021700] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 31054.9094
[aac @ 0000023cc9999380] Qavg: 18673.031
[out#0/rtp @ 0000022e30fb8080] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
[out#1/rtp @ 0000022e32c90640] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
frame= 3590 fps= 30 q=-1.0 size=  118545KiB time=00:01:59.66 bitrate=8115.2kbits/s speed=   1x elapsed=0:01:59.15    
[out#0/rtp @ 00000280c6067ec0] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.47    
frame= 3590 fps= 30 q=-1.0 size=  118545KiB time=00:01:59.66 bitrate=8115.2kbits/s speed=   1x elapsed=0:01:59.15    
[out#0/rtp @ 0000023f76cabac0] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.47    
seconds=1.0142615 cpu=1.05036118307844 ws=142516224 private=568266752
seconds=2.0392216 cpu=0 ws=145371136 private=570912768
seconds=3.0624842 cpu=0.416448684469034 ws=149078016 private=574373888
seconds=4.0889413 cpu=0 ws=152838144 private=577470464
seconds=5.1181989 cpu=0.414023043051966 ws=176476160 private=632291328
seconds=6.1472989 cpu=3.79579243999611 ws=254148608 private=1255084032
seconds=7.176235 cpu=2.07076204069603 ws=254664704 private=1255460864
seconds=8.1931711 cpu=1.7459977886695 ws=256884736 private=1257345024
seconds=9.2167141 cpu=2.28984029005132 ws=258826240 private=1256960000
seconds=10.3065924 cpu=1.95497223697528 ws=258969600 private=1256964096
seconds=11.3323814 cpu=1.66169207755733 ws=259268608 private=1256964096
seconds=12.3567959 cpu=2.01057198127232 ws=261500928 private=1258991616
seconds=13.3856701 cpu=2.83021171896605 ws=261619712 private=1258995712
seconds=14.414174 cpu=1.86446899847792 ws=261746688 private=1259008000
seconds=15.4470208 cpu=2.33797764322136 ws=261865472 private=1259008000
seconds=16.4774623 cpu=1.792038567052 ws=262410240 private=1259446272
seconds=17.5021463 cpu=1.45554851322678 ws=264642560 private=1261793280
seconds=18.5313432 cpu=1.93222148612803 ws=264667136 private=1261793280
seconds=19.5686967 cpu=1.84856332615992 ws=264695808 private=1261830144
seconds=20.5868511 cpu=2.44147199535302 ws=262332416 private=1259270144
seconds=21.6274823 cpu=2.7982361264796 ws=262385664 private=1259270144
seconds=22.6554078 cpu=2.07279790041382 ws=262443008 private=1259270144
seconds=23.6943047 cpu=2.59781662296195 ws=263843840 private=1260691456
seconds=24.7454592 cpu=2.63508966915555 ws=263843840 private=1260691456
seconds=25.7783324 cpu=3.02554079242253 ws=263843840 private=1260691456
seconds=26.8052134 cpu=3.25068647858728 ws=263843840 private=1260691456
seconds=27.8348529 cpu=2.48321687524437 ws=263856128 private=1260691456
seconds=28.8672856 cpu=3.09562330529897 ws=264044544 private=1260879872
seconds=29.8941999 cpu=3.94219406093133 ws=264044544 private=1260879872
seconds=30.9205788 cpu=2.35271080423879 ws=264101888 private=1260879872
seconds=31.9600658 cpu=3.00629060296089 ws=264372224 private=1261088768
seconds=32.9879734 cpu=2.28011739576592 ws=265027584 private=1261756416
seconds=34.0250682 cpu=2.7392954731902 ws=266948608 private=1263837184
seconds=35.0578898 cpu=2.47556614019128 ws=267022336 private=1263837184
seconds=36.0969498 cpu=2.73411457558668 ws=267083776 private=1263837184
seconds=37.1368406 cpu=3.21001799594551 ws=267083776 private=1263837184
seconds=38.1633084 cpu=2.83684672639689 ws=267198464 private=1263837184
seconds=39.1994737 cpu=2.74175277912616 ws=265375744 private=1261957120
seconds=40.2426756 cpu=2.11052582003018 ws=266293248 private=1262874624
seconds=41.2801316 cpu=2.19067340950101 ws=265998336 private=1262534656
seconds=42.3165379 cpu=2.46700370483871 ws=267231232 private=1263554560
seconds=43.3518486 cpu=2.94981716380144 ws=267235328 private=1263558656
seconds=44.3904226 cpu=3.14570310304845 ws=267235328 private=1263558656
seconds=45.4207962 cpu=2.41251858019796 ws=267235328 private=1263558656
seconds=46.4493115 cpu=2.34782382651257 ws=267292672 private=1263558656
seconds=47.486277 cpu=2.53416426013296 ws=267251712 private=1263558656
seconds=48.5187008 cpu=3.2332344351401 ws=267251712 private=1263558656
seconds=49.5587682 cpu=2.73146633661345 ws=267251712 private=1263558656
seconds=50.5903699 cpu=1.79002313498603 ws=267255808 private=1263558656
seconds=51.6222148 cpu=2.13375532064416 ws=267259904 private=1263558656
seconds=52.6586445 cpu=2.19284267203774 ws=267255808 private=1263558656
seconds=53.6892176 cpu=2.5498830787364 ws=267255808 private=1263558656
seconds=54.7273776 cpu=1.91553938086265 ws=267255808 private=1263558656
seconds=55.7542229 cpu=2.69747192068401 ws=267255808 private=1263558656
seconds=56.7940075 cpu=2.45898831528971 ws=267255808 private=1263558656
seconds=57.8154475 cpu=3.26800221434268 ws=267255808 private=1263558656
seconds=58.8433988 cpu=3.45457646061282 ws=267255808 private=1263558656
seconds=59.8738855 cpu=2.61901840786848 ws=267255808 private=1263558656
seconds=60.9038592 cpu=1.86180835138182 ws=267194368 private=1263456256
seconds=61.9376264 cpu=2.06108475697606 ws=267202560 private=1263464448
seconds=62.9621666 cpu=2.35693311719025 ws=267202560 private=1263464448
seconds=63.9836823 cpu=2.57249194416779 ws=267259904 private=1263464448
seconds=65.015029 cpu=2.82342670818825 ws=267259904 private=1263464448
seconds=66.0491322 cpu=1.92305406620573 ws=267268096 private=1263464448
seconds=67.077073 cpu=4.21462633221328 ws=269123584 private=1265418240
seconds=68.113958 cpu=3.63030089687338 ws=269180928 private=1265418240
seconds=69.1457414 cpu=2.75339677970111 ws=269180928 private=1265418240
seconds=70.1858028 cpu=2.39004683237495 ws=267612160 private=1263788032
seconds=71.2170426 cpu=2.47936336613288 ws=267685888 private=1263788032
seconds=72.2441905 cpu=1.86693039664846 ws=267685888 private=1263788032
seconds=73.2791687 cpu=2.40178532701989 ws=267685888 private=1263788032
seconds=74.3013736 cpu=1.94543810505736 ws=267821056 private=1263796224
seconds=75.3346394 cpu=2.40576573283028 ws=269340672 private=1265229824
seconds=76.3687526 cpu=2.6785136904126 ws=269340672 private=1265229824
seconds=77.3927814 cpu=1.66454835503204 ws=269340672 private=1265229824
seconds=78.4274389 cpu=2.53981719466676 ws=268193792 private=1264082944
seconds=79.4480748 cpu=2.50512271988294 ws=268193792 private=1264082944
seconds=80.486672 cpu=1.57281641744531 ws=268193792 private=1264082944
seconds=81.523183 cpu=2.26119163231262 ws=268193792 private=1264082944
seconds=82.5600572 cpu=3.35635088264674 ws=268201984 private=1264082944
seconds=83.593363 cpu=3.36794164550674 ws=268337152 private=1264439296
seconds=84.6252057 cpu=2.0649289064911 ws=268337152 private=1264439296
seconds=85.6500673 cpu=3.39569131711405 ws=268337152 private=1264439296
seconds=86.6811699 cpu=3.51289880455068 ws=268337152 private=1264439296
seconds=87.7175955 cpu=2.26137795129722 ws=268337152 private=1264439296
seconds=88.7577157 cpu=3.68729236556242 ws=268341248 private=1264439296
seconds=89.7821341 cpu=1.94123452257045 ws=268435456 private=1264439296
seconds=90.8175708 cpu=2.46931384778825 ws=268664832 private=1264439296
seconds=91.8523237 cpu=2.53958303387302 ws=269320192 private=1265184768
seconds=92.8719268 cpu=2.57731749647575 ws=269324288 private=1265188864
seconds=93.9103488 cpu=1.84666121900696 ws=270237696 private=1266106368
seconds=94.9491098 cpu=2.87164664966679 ws=268128256 private=1263955968
seconds=95.9887072 cpu=1.77625579776452 ws=268275712 private=1263955968
seconds=97.0276253 cpu=2.66612581264717 ws=268275712 private=1263955968
seconds=98.0631055 cpu=1.57755090563075 ws=268361728 private=1263955968
seconds=99.0857718 cpu=3.81967216481076 ws=268365824 private=1263955968
seconds=100.1193105 cpu=3.43590072015338 ws=270245888 private=1265897472
seconds=101.1579035 cpu=2.05150797105488 ws=270254080 private=1265901568
seconds=102.1905153 cpu=2.88874729637464 ws=270311424 private=1265901568
seconds=103.2258016 cpu=2.74408063828248 ws=268099584 private=1263517696
seconds=104.2617333 cpu=2.33101538187577 ws=266838016 private=1262256128
seconds=105.2949045 cpu=2.2685010964301 ws=267202560 private=1262800896
seconds=106.3198319 cpu=1.80167971808629 ws=267366400 private=1263009792
seconds=107.3440732 cpu=2.98169705979174 ws=267771904 private=1263222784
seconds=108.3785128 cpu=3.22693387010531 ws=269983744 private=1265471488
seconds=109.4121641 cpu=2.47357903174714 ws=269987840 private=1265475584
seconds=110.4489728 cpu=3.01405649856138 ws=269987840 private=1265475584
seconds=111.4844336 cpu=2.60643728508472 ws=268943360 private=1264373760
seconds=112.5232772 cpu=3.6918235552756 ws=268943360 private=1264373760
seconds=113.5593418 cpu=2.12506492882253 ws=268943360 private=1264373760
seconds=114.5855666 cpu=2.35306409207096 ws=268943360 private=1264373760
seconds=115.6178467 cpu=2.68327013534057 ws=269012992 private=1264373760
seconds=116.6572929 cpu=2.25480645366735 ws=269131776 private=1264373760
seconds=117.6896429 cpu=2.61429131240728 ws=269131776 private=1264373760
seconds=118.723461 cpu=2.67927826339697 ws=269193216 private=1264373760
seconds=119.7422889 cpu=2.23072736104622 ws=245395456 private=1247064064
seconds=120.7701805 cpu=1.1746242148845 ws=245395456 private=1247145984
seconds=121.7981426 cpu=1.10545285313887 ws=245415936 private=1247145984
seconds=122.8233219 cpu=1.10845355184566 ws=245415936 private=1247145984
seconds=123.8540265 cpu=1.03360449646863 ws=245415936 private=1247145984
seconds=124.8813986 cpu=1.38260961676354 ws=245424128 private=1247145984
seconds=125.9123071 cpu=1.37786675098181 ws=245415936 private=1247145984
seconds=126.9502527 cpu=1.02639378122603 ws=245415936 private=1247158272
seconds=127.9921889 cpu=1.36328361127537 ws=245415936 private=1247158272
seconds=129.0288753 cpu=1.37018730587624 ws=245415936 private=1247158272
seconds=130.0627613 cpu=1.71737327115194 ws=245415936 private=1247158272
seconds=131.0861367 cpu=1.80441205552813 ws=245424128 private=1247158272
seconds=132.1140414 cpu=1.45098789092732 ws=245415936 private=1247158272
seconds=133.1538445 cpu=1.57099236121986 ws=245415936 private=1247158272
seconds=134.1935461 cpu=1.29790299272581 ws=245415936 private=1247158272
seconds=135.2341902 cpu=1.2284786805682 ws=245415936 private=1247158272
seconds=136.2763167 cpu=1.22673119905222 ws=245415936 private=1247158272
seconds=137.3155409 cpu=1.36684128935271 ws=245415936 private=1247158272
seconds=138.352633 cpu=1.36965130238149 ws=245415936 private=1247158272
seconds=139.3895617 cpu=1.09589370644639 ws=245415936 private=1247158272
seconds=140.4157129 cpu=1.38425462588219 ws=245415936 private=1247158272
seconds=141.4445925 cpu=1.10446706919218 ws=245415936 private=1247158272
seconds=142.4686277 cpu=1.17904771597338 ws=245415936 private=1247158272
seconds=143.49538 cpu=0.968410961259287 ws=245415936 private=1247158272
seconds=144.5348059 cpu=1.1615896463965 ws=245415936 private=1247158272
seconds=145.5839977 cpu=1.28616313831449 ws=245415936 private=1247158272
seconds=146.6250914 cpu=1.43260618398446 ws=245415936 private=1247158272
seconds=147.6534409 cpu=1.31223073301619 ws=245424128 private=1247158272
seconds=148.6802699 cpu=1.45250793727801 ws=245415936 private=1247158272
seconds=149.7077653 cpu=1.38244370286675 ws=245415936 private=1247158272
seconds=150.7351305 cpu=1.1060951221276 ws=245415936 private=1247158272
seconds=151.7589022 cpu=1.24872478005506 ws=245415936 private=1247158272
seconds=152.7994809 cpu=1.3650621000166 ws=245415936 private=1247158272
seconds=153.8252847 cpu=1.17701490639473 ws=245420032 private=1247162368
seconds=154.8574368 cpu=1.2385859515367 ws=245428224 private=1247162368
seconds=155.8950188 cpu=1.30055438334686 ws=245424128 private=1247166464
seconds=156.9213233 cpu=1.52245264441497 ws=245424128 private=1247166464
seconds=157.9473135 cpu=1.10757747624065 ws=245424128 private=1247166464
seconds=158.9752887 cpu=1.51997830297854 ws=245424128 private=1247166464
seconds=160.012058 cpu=1.57558940766545 ws=245424128 private=1247166464
seconds=161.0391609 cpu=1.31382339411348 ws=245424128 private=1247166464
seconds=162.0675987 cpu=1.24305922138327 ws=246341632 private=1248083968
seconds=163.0934439 cpu=1.59236766645952 ws=245432320 private=1247166464
wasapi debug: maximum latency: 000 ns
main error: buffer deadlock prevented
main warning: playback too late (144866): up-sampling
main warning: playback way too late (281886): flushing buffers
main warning: picture is too late to be displayed (missing 95 ms)
main warning: picture is too late to be displayed (missing 62 ms)
main warning: picture is too late to be displayed (missing 28 ms)
main error: Failed to create video converter
main error: write error: No error
main error: read error: No error
main error: Failed to create video converter
main error: read error: No error
main error: read error: No error
direct3d11 error: SetThumbNailClip failed: 0x800706f4
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r354-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r354-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r354-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r354-native-2026-10-26-14h38m45s333.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r354-native-2026-10-26-14h40m10s908.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r354-output.sdp
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r354-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r354-source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r354-source1-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r354-source1.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r354-source2-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r354-source2.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r354-vlc.log
~~~~

清理完成：以上13文件逐项删除，r354前缀无残留，五PID无残留。复用测试源与正式构建保留，无远程产物。
