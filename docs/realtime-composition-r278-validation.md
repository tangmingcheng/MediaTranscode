# r278：三路RTP合屏转HEVC MPEG-TS/RTP验证

状态：FAIL，重入阶段截止时间超限，已归档并清理。全量Release构建57699成功，configure/build均0，四项产物存在。realtime SHA256：281D126DAA59D382863ABE5880BA81DF96F968D03CA489BFFF8B84FB153D62B4。未运行local。

进程：CLI 12000，VLC 3132，FFmpeg源5508/31516/25772；启动会话27441。输入三路H.264 1280×720 30fps约8Mbps，所选AAC44100Hz双声道；输出HEVC 1280×720 30fps 8Mbps CBR及AAC192kbps，MPEG-TS/RTP。

重入源PID：13000，会话82563。第一阶段三源均达到120秒/3600帧；VLC原生快照先确认三格画面，后确认全部失源全黑。仍需完整重入及最终结果。

## 重入实际命令
~~~powershell
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r278-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r278-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($source0.Id)"
$source0.WaitForExit()
Write-Output "REENTRY_EXIT=$($source0.ExitCode)"
exit $source0.ExitCode
~~~

## 实际命令
~~~powershell
$ErrorActionPreference='Stop'
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r278-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r278-native- --snapshot-format=png rtp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r278-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r278-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r278-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r278-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r278-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r278-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r278 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r278-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r278-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r278-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r278-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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
~~~

## 结果：FAIL
CLI自然错误退出1；初始三源完整120秒3600帧且退出0。三格画面及失源全黑均由VLC原生RC snapshot观察。重入generation2已建立，约2秒后发生截止时间失败，未取得有效重入画面快照。重入源将按精确PID13000清理，不将该退出算自然EOF。

失败在materializer进入时已经迟到1775520ns；之前7次WouldBlock明确serial=1且count=0/bytes=0，头预约24个数据报已提交19个，剩5个，非计数/字节容量耗尽。mux出队到本次进入75591600ns。此证据再次指向整笔串行等待，不能宣称去副本已解决。

A/V内部raw_phase采样范围：0..13606 ns；仅为内部telemetry，不代表播放器端测量。最终平均机器CPU1.639757%，峰值9.018036%，工作集峰值268238848；payload当前字节/对象均0，reserve/release均147166，pressure failures0，errors1。

### 失败前后
~~~text
[CLI] runtime report: queued=1, peakQueued=938, threads=82, processThreads=130, workers=76, workerProgress=335111, workerProcessCalls=1223233, workerWaits=888115, workerWakeups=877612, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=440, logicalProcessors=22, averageSystemMachineCpuPercent=7.606940, averageProcessMachineCpuPercent=1.646131, peakProcessMachineCpuPercent=9.018036, averageProcessSingleCoreCpuPercent=36.214873, peakProcessSingleCoreCpuPercent=198.396794, initialWorkingSetBytes=207290368, workingSetBytes=246595584, peakWorkingSetBytes=268238848, totalPushed=306689, totalPopped=306664, droppedBuffers=0, encodedPacketsPushed=45809, encodedPacketsPopped=45808, backpressureItems=123, graphPayloadCurrentBytes=787918, graphPayloadHighWaterBytes=17408952, graphPayloadCurrentObjects=67, graphPayloadHighWaterObjects=749, graphPayloadReservations=145011, graphPayloadReleases=144944, graphPayloadPressureFailures=0, lastInputReceivedAtNs=4117311311100
[2026-10-20 09:52:03.659] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1 source_master_ns=142546296652 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-10-20 09:52:03.660] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3 source_master_ns=142592736561 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=45346
[2026-10-20 09:52:03.665] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[CLI] runtime report: queued=19, peakQueued=938, threads=82, processThreads=130, workers=76, workerProgress=335695, workerProcessCalls=1225020, workerWaits=889317, workerWakeups=878791, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=441, logicalProcessors=22, averageSystemMachineCpuPercent=7.595917, averageProcessMachineCpuPercent=1.644992, peakProcessMachineCpuPercent=9.018036, averageProcessSingleCoreCpuPercent=36.189831, peakProcessSingleCoreCpuPercent=198.396794, initialWorkingSetBytes=207290368, workingSetBytes=249131008, peakWorkingSetBytes=268238848, totalPushed=307239, totalPopped=307196, droppedBuffers=0, encodedPacketsPushed=45918, encodedPacketsPopped=45918, backpressureItems=123, graphPayloadCurrentBytes=266682, graphPayloadHighWaterBytes=17408952, graphPayloadCurrentObjects=51, graphPayloadHighWaterObjects=749, graphPayloadReservations=145392, graphPayloadReleases=145341, graphPayloadPressureFailures=0, lastInputReceivedAtNs=4117642919300
[CLI] runtime report: queued=22, peakQueued=938, threads=82, processThreads=130, workers=76, workerProgress=336202, workerProcessCalls=1226772, workerWaits=890563, workerWakeups=880013, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=442, logicalProcessors=22, averageSystemMachineCpuPercent=7.589971, averageProcessMachineCpuPercent=1.645668, peakProcessMachineCpuPercent=9.018036, averageProcessSingleCoreCpuPercent=36.204706, peakProcessSingleCoreCpuPercent=198.396794, initialWorkingSetBytes=207290368, workingSetBytes=249634816, peakWorkingSetBytes=268238848, totalPushed=307702, totalPopped=307656, droppedBuffers=0, encodedPacketsPushed=46019, encodedPacketsPopped=46019, backpressureItems=123, graphPayloadCurrentBytes=239974, graphPayloadHighWaterBytes=17408952, graphPayloadCurrentObjects=52, graphPayloadHighWaterObjects=749, graphPayloadReservations=145663, graphPayloadReleases=145611, graphPayloadPressureFailures=0, lastInputReceivedAtNs=4117945306200
[2026-10-20 09:52:04.210] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=48 source_master_ns=143637634521 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=90692
[CLI] runtime report: queued=20, peakQueued=938, threads=82, processThreads=130, workers=76, workerProgress=336688, workerProcessCalls=1228460, workerWaits=891764, workerWakeups=881192, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=443, logicalProcessors=22, averageSystemMachineCpuPercent=7.580861, averageProcessMachineCpuPercent=1.643558, peakProcessMachineCpuPercent=9.018036, averageProcessSingleCoreCpuPercent=36.158284, peakProcessSingleCoreCpuPercent=198.396794, initialWorkingSetBytes=207290368, workingSetBytes=250343424, peakWorkingSetBytes=268238848, totalPushed=308142, totalPopped=308098, droppedBuffers=0, encodedPacketsPushed=46120, encodedPacketsPopped=46120, backpressureItems=123, graphPayloadCurrentBytes=213439, graphPayloadHighWaterBytes=17408952, graphPayloadCurrentObjects=49, graphPayloadHighWaterObjects=749, graphPayloadReservations=145905, graphPayloadReleases=145856, graphPayloadPressureFailures=0, lastInputReceivedAtNs=4118220506400
[CLI] runtime report: queued=20, peakQueued=938, threads=82, processThreads=130, workers=76, workerProgress=337182, workerProcessCalls=1230089, workerWaits=892900, workerWakeups=882305, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=444, logicalProcessors=22, averageSystemMachineCpuPercent=7.572577, averageProcessMachineCpuPercent=1.643153, peakProcessMachineCpuPercent=9.018036, averageProcessSingleCoreCpuPercent=36.149358, peakProcessSingleCoreCpuPercent=198.396794, initialWorkingSetBytes=207290368, workingSetBytes=250601472, peakWorkingSetBytes=268238848, totalPushed=308590, totalPopped=308546, droppedBuffers=0, encodedPacketsPushed=46213, encodedPacketsPopped=46212, backpressureItems=123, graphPayloadCurrentBytes=281576, graphPayloadHighWaterBytes=17408952, graphPayloadCurrentObjects=51, graphPayloadHighWaterObjects=749, graphPayloadReservations=146162, graphPayloadReleases=146111, graphPayloadPressureFailures=0, lastInputReceivedAtNs=4118529898500
[CLI] runtime report: queued=20, peakQueued=938, threads=82, processThreads=130, workers=76, workerProgress=337693, workerProcessCalls=1231871, workerWaits=894171, workerWakeups=883553, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=445, logicalProcessors=22, averageSystemMachineCpuPercent=7.562661, averageProcessMachineCpuPercent=1.640982, peakProcessMachineCpuPercent=9.018036, averageProcessSingleCoreCpuPercent=36.101603, peakProcessSingleCoreCpuPercent=198.396794, initialWorkingSetBytes=207290368, workingSetBytes=250929152, peakWorkingSetBytes=268238848, totalPushed=309053, totalPopped=309009, droppedBuffers=0, encodedPacketsPushed=46314, encodedPacketsPopped=46313, backpressureItems=123, graphPayloadCurrentBytes=293540, graphPayloadHighWaterBytes=17408952, graphPayloadCurrentObjects=51, graphPayloadHighWaterObjects=749, graphPayloadReservations=146422, graphPayloadReleases=146371, graphPayloadPressureFailures=0, lastInputReceivedAtNs=4118831336500
[CLI] runtime report: queued=20, peakQueued=938, threads=82, processThreads=130, workers=76, workerProgress=338211, workerProcessCalls=1233927, workerWaits=895710, workerWakeups=885069, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=446, logicalProcessors=22, averageSystemMachineCpuPercent=7.558801, averageProcessMachineCpuPercent=1.640970, peakProcessMachineCpuPercent=9.018036, averageProcessSingleCoreCpuPercent=36.101334, peakProcessSingleCoreCpuPercent=198.396794, initialWorkingSetBytes=207290368, workingSetBytes=251158528, peakWorkingSetBytes=268238848, totalPushed=309525, totalPopped=309481, droppedBuffers=0, encodedPacketsPushed=46415, encodedPacketsPopped=46414, backpressureItems=123, graphPayloadCurrentBytes=225656, graphPayloadHighWaterBytes=17408952, graphPayloadCurrentObjects=53, graphPayloadHighWaterObjects=749, graphPayloadReservations=146692, graphPayloadReleases=146639, graphPayloadPressureFailures=0, lastInputReceivedAtNs=4119143139500
[2026-10-20 09:52:05.232] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=92 source_master_ns=144659312525 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=136038
[CLI] runtime report: queued=22, peakQueued=938, threads=82, processThreads=130, workers=76, workerProgress=338730, workerProcessCalls=1235860, workerWaits=897124, workerWakeups=886459, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=447, logicalProcessors=22, averageSystemMachineCpuPercent=7.561558, averageProcessMachineCpuPercent=1.639757, peakProcessMachineCpuPercent=9.018036, averageProcessSingleCoreCpuPercent=36.074655, peakProcessSingleCoreCpuPercent=198.396794, initialWorkingSetBytes=207290368, workingSetBytes=251277312, peakWorkingSetBytes=268238848, totalPushed=309997, totalPopped=309951, droppedBuffers=0, encodedPacketsPushed=46516, encodedPacketsPopped=46515, backpressureItems=123, graphPayloadCurrentBytes=321359, graphPayloadHighWaterBytes=17408952, graphPayloadCurrentObjects=55, graphPayloadHighWaterObjects=749, graphPayloadReservations=146958, graphPayloadReleases=146903, graphPayloadPressureFailures=0, lastInputReceivedAtNs=4119443901900
[2026-10-20 09:52:05.720] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=144506675100 present_generation=1 present_canonical_target_ns=138901768707 present_canonical_dispatch_ns=138901768707 present_target_master_ns=144625999975 present_dispatch_master_ns=144625999975
[2026-10-20 09:52:05.731] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=144517826600 present_generation=1 present_canonical_target_ns=138933333333 present_canonical_dispatch_ns=138933333333 present_target_master_ns=144657564601 present_dispatch_master_ns=144657564601
[2026-10-20 09:52:05.773] [info] [graph][runtime.node] protocol_batch_trace stage=materialization_failed generation=1 attempts=8 consumed_datagrams=0 remaining_datagrams=1 attempted_master_ns=144559342900 mux_dequeued_master_ns=144483751300 first_dequeued_master_ns=144485321600 last_wire_published_master_ns=144485317900 previous_attempt_started_master_ns=144554549200 previous_attempt_completed_master_ns=144554576000 last_attempt_completed_master_ns=144559374300 last_blocked_started_master_ns=144554549200 last_blocked_completed_master_ns=144554576000 blocked_attempts=7 last_blocked_code=WouldBlock last_blocked_native=0 last_blocked_original_message_bytes=291 last_blocked_message_truncated=0 last_blocked_message="wire global sequence reservation exceeds planner backlog capacity serial=1 count=0 bytes=0 request_count=1 request_bytes=228 used_count=5 used_bytes=6028 limit_count=2155 limit_bytes=2920356 generation=1 projected_sequence=85482 head_id=15018 head_first=85458 head_count=24 head_committed=19" remaining_front_release_master_ns=144490900712 remaining_front_deadline_master_ns=144557567380 original_error="InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=144490900712 materialized_ns=144559342900 protocol_deadline_ns=144557567380 deadline_before_release=0 materialized_after_deadline=1"
[2026-10-20 09:52:05.773] [info] [graph][runtime.node] process.failed node=80 name=MediaMpegTsDatagramMaterializerNode kind=MpegTsDatagramMaterializer inputs=[channel=118 edge=118 stream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=120 edge=120 stream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=121 edge=121 stream=Metadata edge_kind=Unknown payload=Unknown queued=9] outputs=[channel=122 edge=122 stream=Metadata edge_kind=Unknown payload=Unknown queued=0] error=InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=144490900712 materialized_ns=144559342900 protocol_deadline_ns=144557567380 deadline_before_release=0 materialized_after_deadline=1
[2026-10-20 09:52:05.773] [info] [graph][runtime.node] worker.failed node=80 kind=MpegTsDatagramMaterializer name=MPEG-TS wire protocol materializer error=InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=144490900712 materialized_ns=144559342900 protocol_deadline_ns=144557567380 deadline_before_release=0 materialized_after_deadline=1
[2026-10-20 09:52:05.773] [info] [graph][runtime.node] worker.thread_cpu node=80 available=1 cpu_ns=1031250000 process_calls=54237
~~~
### 诊断与漂移
~~~text
[2026-10-20 09:49:46.770] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1 source_master_ns=5730010886 raw_phase_ns=1 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-10-20 09:49:46.771] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3 source_master_ns=5776450795 raw_phase_ns=1 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=45346
[2026-10-20 09:49:47.375] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=47 source_master_ns=6798128800 raw_phase_ns=1 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=90692
[2026-10-20 09:49:48.420] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=92 source_master_ns=7843026759 raw_phase_ns=1 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=136038
[2026-10-20 09:49:49.442] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=136 source_master_ns=8864704764 raw_phase_ns=1 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=181384
[2026-10-20 09:49:50.463] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=180 source_master_ns=9886382767 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=226730
[2026-10-20 09:49:51.508] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=225 source_master_ns=10931280726 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=272076
[2026-10-20 09:49:52.530] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=269 source_master_ns=11952958731 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=317422
[2026-10-20 09:49:53.552] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=313 source_master_ns=12974636735 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=362768
[2026-10-20 09:49:54.573] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=357 source_master_ns=13996314740 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=408114
[2026-10-20 09:49:55.618] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=402 source_master_ns=15041212699 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=453460
[2026-10-20 09:49:56.640] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=446 source_master_ns=16062890704 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=498806
[2026-10-20 09:49:57.662] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=490 source_master_ns=17084568708 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=544152
[2026-10-20 09:49:58.706] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=535 source_master_ns=18129466667 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=589498
[2026-10-20 09:49:59.728] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=579 source_master_ns=19151144672 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=634844
[2026-10-20 09:50:00.750] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=623 source_master_ns=20172822676 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=680190
[2026-10-20 09:50:01.771] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=667 source_master_ns=21194500681 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=725536
[2026-10-20 09:50:02.816] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=712 source_master_ns=22239398640 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=770882
[2026-10-20 09:50:03.838] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=756 source_master_ns=23261076645 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=816228
[2026-10-20 09:50:04.860] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=800 source_master_ns=24282754649 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=861574
[2026-10-20 09:50:05.906] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=845 source_master_ns=25327652608 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=906920
[2026-10-20 09:50:06.927] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=889 source_master_ns=26349330613 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=952266
[2026-10-20 09:50:07.948] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=933 source_master_ns=27371008617 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=997612
[2026-10-20 09:50:08.969] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=977 source_master_ns=28392686622 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1042958
[2026-10-20 09:50:10.015] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1022 source_master_ns=29437584581 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1088304
[2026-10-20 09:50:11.037] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1066 source_master_ns=30459262586 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1133650
[2026-10-20 09:50:12.058] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1110 source_master_ns=31480940590 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1178996
[2026-10-20 09:50:13.103] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1155 source_master_ns=32525838549 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1224342
[2026-10-20 09:50:14.126] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1199 source_master_ns=33547516554 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1269688
[2026-10-20 09:50:15.146] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1243 source_master_ns=34569194558 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1315034
[2026-10-20 09:50:16.168] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1287 source_master_ns=35590872563 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1360380
[2026-10-20 09:50:17.212] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1332 source_master_ns=36635770522 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1405726
[2026-10-20 09:50:18.234] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1376 source_master_ns=37657448527 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1451072
[2026-10-20 09:50:19.256] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1420 source_master_ns=38679126531 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1496418
[2026-10-20 09:50:20.301] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1465 source_master_ns=39724024490 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1541764
[2026-10-20 09:50:21.323] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1509 source_master_ns=40745702495 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1587110
[2026-10-20 09:50:22.344] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1553 source_master_ns=41767380499 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1632456
[2026-10-20 09:50:23.367] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1597 source_master_ns=42789058504 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1677802
[2026-10-20 09:50:24.411] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1642 source_master_ns=43833956463 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1723148
[2026-10-20 09:50:25.433] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1686 source_master_ns=44855634468 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1768494
[2026-10-20 09:50:26.454] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1730 source_master_ns=45877312472 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1813840
[2026-10-20 09:50:27.499] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1775 source_master_ns=46922210431 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1859186
[2026-10-20 09:50:28.521] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1819 source_master_ns=47943888436 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1904532
[2026-10-20 09:50:29.543] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1863 source_master_ns=48965566441 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1949878
[2026-10-20 09:50:30.564] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1907 source_master_ns=49987244445 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1995224
[2026-10-20 09:50:31.609] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1952 source_master_ns=51032142404 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2040570
[2026-10-20 09:50:32.630] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1996 source_master_ns=52053820409 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2085916
[2026-10-20 09:50:33.652] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2040 source_master_ns=53075498413 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2131262
[2026-10-20 09:50:34.674] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2084 source_master_ns=54097176418 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2176608
[2026-10-20 09:50:35.719] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2129 source_master_ns=55142074377 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2221954
[2026-10-20 09:50:36.741] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2173 source_master_ns=56163752382 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2267300
[2026-10-20 09:50:37.762] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2217 source_master_ns=57185430386 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2312646
[2026-10-20 09:50:38.808] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2262 source_master_ns=58230328345 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2357992
[2026-10-20 09:50:39.829] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2306 source_master_ns=59252006350 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2403338
[2026-10-20 09:50:40.851] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2350 source_master_ns=60273684354 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2448684
[2026-10-20 09:50:41.872] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2394 source_master_ns=61295362359 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2494030
[2026-10-20 09:50:42.917] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2439 source_master_ns=62340260318 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2539376
[2026-10-20 09:50:43.939] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2483 source_master_ns=63361938323 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2584722
[2026-10-20 09:50:44.960] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2527 source_master_ns=64383616327 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2630068
[2026-10-20 09:50:46.005] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2572 source_master_ns=65428514286 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2675414
[2026-10-20 09:50:47.027] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2616 source_master_ns=66450192291 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2720760
[2026-10-20 09:50:48.049] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2660 source_master_ns=67471870295 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2766106
[2026-10-20 09:50:49.071] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2704 source_master_ns=68493548300 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2811452
[2026-10-20 09:50:50.115] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2749 source_master_ns=69538446259 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2856798
[2026-10-20 09:50:51.137] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2793 source_master_ns=70560124264 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2902144
[2026-10-20 09:50:52.159] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2837 source_master_ns=71581802268 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2947490
[2026-10-20 09:50:53.203] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2882 source_master_ns=72626700227 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2992836
[2026-10-20 09:50:54.225] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2926 source_master_ns=73648378232 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3038182
[2026-10-20 09:50:55.247] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2970 source_master_ns=74670056236 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3083528
[2026-10-20 09:50:56.269] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3014 source_master_ns=75691734241 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3128874
[2026-10-20 09:50:57.314] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3059 source_master_ns=76736632200 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3174220
[2026-10-20 09:50:58.335] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3103 source_master_ns=77758310205 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3219566
[2026-10-20 09:50:59.357] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3147 source_master_ns=78779988209 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3264912
[2026-10-20 09:51:00.402] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3192 source_master_ns=79824886168 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3310258
[2026-10-20 09:51:01.423] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3236 source_master_ns=80846564173 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3355604
[2026-10-20 09:51:02.445] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3280 source_master_ns=81868242177 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3400950
[2026-10-20 09:51:03.467] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3324 source_master_ns=82889920182 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3446296
[2026-10-20 09:51:04.512] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3369 source_master_ns=83934818141 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3491642
[2026-10-20 09:51:05.533] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3413 source_master_ns=84956496146 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3536988
[2026-10-20 09:51:06.555] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3457 source_master_ns=85978174150 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3582334
[2026-10-20 09:51:07.600] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3502 source_master_ns=87023072109 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3627680
[2026-10-20 09:51:08.621] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3546 source_master_ns=88044750114 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3673026
[2026-10-20 09:51:09.643] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3590 source_master_ns=89066428119 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3718372
[2026-10-20 09:51:10.665] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3634 source_master_ns=90088106123 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3763718
[2026-10-20 09:51:11.710] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3679 source_master_ns=91133004082 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3809064
[2026-10-20 09:51:12.732] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3723 source_master_ns=92154682087 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3854410
[2026-10-20 09:51:13.753] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3767 source_master_ns=93176360091 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3899756
[2026-10-20 09:51:14.799] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3812 source_master_ns=94221258050 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3945102
[2026-10-20 09:51:15.820] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3856 source_master_ns=95242936055 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3990448
[2026-10-20 09:51:16.842] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3900 source_master_ns=96264614060 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4035794
[2026-10-20 09:51:17.863] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3944 source_master_ns=97286292064 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4081140
[2026-10-20 09:51:18.908] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3989 source_master_ns=98331190023 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4126486
[2026-10-20 09:51:19.931] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4033 source_master_ns=99352868028 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4171832
[2026-10-20 09:51:20.952] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4077 source_master_ns=100374546032 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4217178
[2026-10-20 09:51:21.996] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4122 source_master_ns=101419443992 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4262524
[2026-10-20 09:51:23.018] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4166 source_master_ns=102441121996 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4307870
[2026-10-20 09:51:24.040] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4210 source_master_ns=103462800001 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4353216
[2026-10-20 09:51:25.062] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4254 source_master_ns=104484478005 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4398562
[2026-10-20 09:51:26.106] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4299 source_master_ns=105529375964 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4443908
[2026-10-20 09:51:27.130] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4343 source_master_ns=106551053969 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4489254
[2026-10-20 09:51:28.150] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4387 source_master_ns=107572731973 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4534600
[2026-10-20 09:51:29.172] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4431 source_master_ns=108594409978 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4579946
[2026-10-20 09:51:30.217] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4476 source_master_ns=109639307937 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4625292
[2026-10-20 09:51:31.239] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4520 source_master_ns=110660985942 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4670638
[2026-10-20 09:51:32.260] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4564 source_master_ns=111682663946 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4715984
[2026-10-20 09:51:33.306] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4609 source_master_ns=112727561905 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4761330
[2026-10-20 09:51:34.326] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4653 source_master_ns=113749239910 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4806676
[2026-10-20 09:51:35.348] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4697 source_master_ns=114770917914 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4852022
[2026-10-20 09:51:36.369] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4741 source_master_ns=115792595919 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4897368
[2026-10-20 09:51:37.414] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4786 source_master_ns=116837493878 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4942714
[2026-10-20 09:51:38.437] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4830 source_master_ns=117859171883 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4988060
[2026-10-20 09:51:39.458] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4874 source_master_ns=118880849887 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5033406
[2026-10-20 09:51:40.502] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4919 source_master_ns=119925747846 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5078752
[2026-10-20 09:52:03.659] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1 source_master_ns=142546296652 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-10-20 09:52:03.660] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3 source_master_ns=142592736561 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=45346
[2026-10-20 09:52:04.210] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=48 source_master_ns=143637634521 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=90692
[2026-10-20 09:52:05.232] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=92 source_master_ns=144659312525 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=136038
[2026-10-20 09:52:05.779] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=15022 committed_datagrams=85481 committed_payload_bytes=101164748 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=80004 pacing_reserved=85481 pacing_cancelled=0 pacing_submitted=85481 pacing_maximum_submit_lateness_ns=29690670 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=80004 backlog_max_residence_ns=80613000 last_materialized_sequence=85481 last_scheduled_sequence=85481 last_submitted_sequence=85481 last_committed_sequence=85481 shared_scope_charged_datagrams=85481 shared_scope_charged_wire_bytes=103558216 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2155 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2155 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=379902 endpoint_1_target_effective_socket_bytes=189951 endpoint_1_api_requested_socket_bytes=189951 endpoint_1_effective_socket_bytes=189951 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=85447 endpoint_1_committed_payload_bytes=101162436 endpoint_2_target_effective_socket_bytes=189951 endpoint_2_api_requested_socket_bytes=189951 endpoint_2_effective_socket_bytes=189951 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=34 endpoint_2_committed_payload_bytes=2312
[2026-10-20 09:52:05.789] [info] [graph][runtime.node] mpegts_wire_materializer stage=aborted maximum_materialized_after_release_ns=65644972 worst_materialized_at_ns=63689877400 worst_release_ns=63624232428 worst_deadline_ns=63690899096 worst_global_sequence=41423
[2026-10-20 09:52:05.792] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=canvas count=4171 maximum_ns=20306900 begin_steady_ns=4035924762700 end_steady_ns=4035945069600 video_cursor=1657 audio_cursor=not_owned next_video_cursor=1657 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-20 09:52:05.792] [info] [graph][runtime.node] canvas_compose_correlated scope=attempt begin_steady_ns=4035924763700 end_steady_ns=4035945068600 wall_ns=20304900 thread_cpu_ns=0 surface_count=1 surface_total_ns=800 surface_maximum_ns=800 surface_maximum_tile=none surface_maximum_result=unavailable validate_count=3 validate_total_ns=2100 validate_maximum_ns=1200 validate_maximum_tile=0 validate_maximum_result=0 black_count=1 black_total_ns=105700 black_maximum_ns=105700 black_maximum_tile=none black_maximum_result=0 tile_count=3 tile_total_ns=73500 tile_maximum_ns=44600 tile_maximum_tile=0 tile_maximum_result=0 publish_count=1 publish_total_ns=12300 publish_maximum_ns=12300 publish_maximum_tile=none publish_maximum_result=0 context_push_count=1 context_push_total_ns=3300 context_push_maximum_ns=3300 context_push_maximum_tile=none context_push_maximum_result=0 source_wait_count=4 source_wait_total_ns=4400 source_wait_maximum_ns=3100 source_wait_maximum_tile=0 source_wait_maximum_result=0 plane_copy_count=8 plane_copy_total_ns=156100 plane_copy_maximum_ns=74200 plane_copy_maximum_tile=none plane_copy_maximum_result=0 destination_wait_count=0 destination_wait_total_ns=0 destination_wait_maximum_ns=0 destination_wait_maximum_tile=none destination_wait_maximum_result=unavailable context_pop_count=1 context_pop_total_ns=7300 context_pop_maximum_ns=7300 context_pop_maximum_tile=none context_pop_maximum_result=0 batch_completion_count=1 batch_completion_total_ns=20087800 batch_completion_maximum_ns=20087800 batch_completion_maximum_tile=none batch_completion_maximum_result=0 source_query_completed_count=0 source_query_ready_count=3 source_query_ready_maximum_ns=7300 source_query_ready_begin_steady_ns=4035924881000 source_query_ready_end_steady_ns=4035924888300 source_query_ready_tile=0 source_query_ready_operation=event_query source_query_ready_native=0 source_query_ready_restore_native=0 source_query_pending_count=0 source_query_unavailable_count=0 source_query_error_count=0
[CLI] final runtime report: queued=0, peakQueued=938, threads=82, processThreads=130, workers=0, workerProgress=339121, workerProcessCalls=1237168, workerWaits=898039, workerWakeups=887356, workerErrors=1, errors=1, stalledIntervals=0, cpuSamples=447, logicalProcessors=22, averageSystemMachineCpuPercent=7.561558, averageProcessMachineCpuPercent=1.639757, peakProcessMachineCpuPercent=9.018036, averageProcessSingleCoreCpuPercent=36.074655, peakProcessSingleCoreCpuPercent=198.396794, initialWorkingSetBytes=207290368, workingSetBytes=251277312, peakWorkingSetBytes=268238848, totalPushed=310356, totalPopped=310305, droppedBuffers=0, encodedPacketsPushed=46602, encodedPacketsPopped=46602, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=17408952, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=749, graphPayloadReservations=147166, graphPayloadReleases=147166, graphPayloadPressureFailures=0, lastInputReceivedAtNs=4119679480300
~~~
### stderr
~~~text
[in @ 0000022f97424dc0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000022f97424dc0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 32428.753067
[in @ 0000022f9cfec200] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000022f9cfec200] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 29351.780044
[in @ 0000022f9cfebfc0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000022f9cfebfc0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 26749.666722
[in @ 0000022fa2b0f1c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000022fa2b0f1c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 29351.780044
[in @ 0000022fa2b234c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000022fa2b234c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 32428.753067
[in @ 0000022fa2795280] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000022fa2795280] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 26749.666722
[aac @ 0000022fa22e15c0] Qavg: 11330.934
[aac @ 0000022fa22e15c0] 2 frames left in the queue on closing
[CLI] realtime video runtime failed: InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=144490900712 materialized_ns=144559342900 protocol_deadline_ns=144557567380 deadline_before_release=0 materialized_after_deadline=1

~~~
### 源结束/清理前状态
~~~text
source0
[out#1/rtp @ 00000237b8ff3ac0] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.49    
source1
[out#0/rtp @ 000002352abf8340] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
source2
[out#0/rtp @ 000002354f019800] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
reentry
frame= 1795 fps= 30 q=-1.0 size=   59620KiB time=00:00:59.83 bitrate=8162.1kbits/s speed=1.01x elapsed=0:00:59.34    
frame= 1811 fps= 30 q=-1.0 size=   60029KiB time=00:01:00.37 bitrate=8145.5kbits/s speed=1.01x elapsed=0:00:59.86    
~~~

清理文件清单：
composition-r278-cli-error.log
composition-r278-cli.log
composition-r278-monitor.log
composition-r278-native-2026-10-20-09h50m17s586.png
composition-r278-native-2026-10-20-09h51m50s766.png
composition-r278-output.sdp
composition-r278-reentry-stdout.log
composition-r278-reentry.log
composition-r278-source0-stdout.log
composition-r278-source0.log
composition-r278-source1-stdout.log
composition-r278-source1.log
composition-r278-source2-stdout.log
composition-r278-source2.log
composition-r278-vlc.log

### 清理记录
CLI会话27441确认退出1，初始三源退出0。确认13000为本轮61020/61022连续源后Stop-Process -Id 13000 -Force，重入会话82563退出-1，为测试失败后的人工清理；VLC通过RC quit退出。六个精确PID均已不存在。重入FFmpeg stderr清理前为空，不能报告其完整源帧数或自然EOF。
VLC诊断：
~~~text
-- logger module started --
main debug: VLC media player - 3.0.23 Vetinari
main debug: Copyright © 1996-2025 the VideoLAN team
main debug: revision 3.0.23-2-0-g79128878dd
main debug: configured with /builds/videolan/vlc/extras/package/win32/../../../configure  '--enable-update-check' '--enable-lua' '--enable-faad' '--enable-flac' '--enable-theora' '--enable-avcodec' '--enable-merge-ffmpeg' '--enable-mpc' '--enable-libass' '--enable-schroedinger' '--enable-realrtsp' '--enable-live555' '--enable-shout' '--enable-goom' '--enable-sse' '--enable-mmx' '--enable-libcddb' '--enable-zvbi' '--disable-telx' '--enable-nls' '--host=i686-w64-mingw32' '--with-contrib=../contrib/i686-w64-mingw32' '--with-breakpad=https://win.crashes.videolan.org' '--enable-qt' '--enable-skins2' '--enable-dvdread' '--enable-caca' 'host_alias=i686-w64-mingw32' 'CFLAGS= -D__MSVCRT_VERSION__=0x700 -D_WIN32_WINNT=0x0502 -DWINVER=0x0502 ' 'CPPFLAGS= -D__MSVCRT_VERSION__=0x700 -D_WIN32_WINNT=0x0502 -DWINVER=0x0502' 'CXXFLAGS= -D__MSVCRT_VERSION__=0x700 -D_WIN32_WINNT=0x0502 -DWINVER=0x0502 ' 'PKG_CONFIG=pkg-config' 'PKG_CONFIG_LIBDIR=/usr/i686-w64-mingw32/lib/pkgconfig:/usr/lib/i686-w64-mingw32/pkgconfig'
main debug: using multimedia timers as clock source
main debug:  min period: 1 ms, max period: 1000000 ms
main debug: searching plug-in modules
main debug: loading plugins cache file D:\VideoLAN\VLC\plugins\plugins.dat
main debug: recursively browsing `D:\VideoLAN\VLC\plugins'
main debug: plug-ins loaded: 495 modules
main debug: opening config file (C:\Users\ROG\AppData\Roaming\vlc\vlcrc)
main debug: looking for logger module matching "any": 2 candidates
file debug: opening logfile `D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r278-vlc.log'
main debug: using logger module "file"
main debug: translation test: code is "C"
main debug: looking for keystore module matching "memory": 3 candidates
main debug: using keystore module "memory"
main debug: CPU has capabilities MMX MMXEXT SSE SSE2 SSE3 SSSE3 SSE4.1 SSE4.2 FPU 
main debug: Creating an input for 'Media Library'
main debug: Input is a meta file: disabling unneeded options
main debug: using timeshift granularity of 50 MiB
main debug: using timeshift path: C:\Users\ROG\AppData\Local\Temp
main debug: `file/directory:///C:/Users/ROG/AppData/Roaming/vlc/ml.xspf' gives access `file' demux `directory' path `/C:/Users/ROG/AppData/Roaming/vlc/ml.xspf'
main debug: creating demux: access='file' demux='directory' location='/C:/Users/ROG/AppData/Roaming/vlc/ml.xspf' file='C:\Users\ROG\AppData\Roaming\vlc\ml.xspf'
main debug: looking for access_demux module matching "file": 15 candidates
main debug: no access_demux modules matched
main debug: creating access: file:///C:/Users/ROG/AppData/Roaming/vlc/ml.xspf
main debug:  (path: C:\Users\ROG\AppData\Roaming\vlc\ml.xspf)
main debug: looking for access module matching "file": 27 candidates
main debug: using access module "filesystem"
main debug: looking for stream_filter module matching "prefetch,cache_read": 24 candidates
cache_read debug: Using stream method for AStream*
cache_read debug: starting pre-buffering
cache_read debug: received first data after 0 ms
cache_read debug: pre-buffering done 304 bytes in 0s - 296875 KiB/s
main debug: using stream_filter module "cache_read"
main debug: looking for stream_filter module matching "any": 24 candidates
playlist debug: using XSPF playlist reader
main debug: using stream_filter module "playlist"
main debug: stream filter added to 033723d8
main debug: looking for stream_filter module matching "any": 24 candidates
main debug: no stream_filter modules matched
main debug: looking for stream_directory module matching "any": 1 candidates
main debug: no stream_directory modules matched
main debug: attachment of directory-extractor failed for file:///C:/Users/ROG/AppData/Roaming/vlc/ml.xspf
main debug: looking for stream_filter module matching "record": 24 candidates
main debug: using stream_filter module "record"
main debug: creating demux: access='file' demux='directory' location='/C:/Users/ROG/AppData/Roaming/vlc/ml.xspf' file='C:\Users\ROG\AppData\Roaming\vlc\ml.xspf'
main debug: looking for demux module matching "directory": 56 candidates
main debug: using demux module "directory"
main debug: looking for meta reader module matching "any": 2 candidates
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\meta\reader
lua debug: Trying Lua scripts in D:\VideoLAN\VLC\lua\meta\reader
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\reader\filename.luac
main debug: no meta reader modules matched
main debug: `file/directory:///C:/Users/ROG/AppData/Roaming/vlc/ml.xspf' successfully opened
main debug: looking for xml reader module matching "any": 1 candidates
main debug: using xml reader module "xml"
main debug: EOF reached
main debug: removing module "directory"
main debug: removing module "record"
main debug: removing module "playlist"
main debug: removing module "cache_read"
main debug: removing module "filesystem"
main debug: creating audio output
main debug: looking for audio output module matching "any": 6 candidates
mmdevice debug: using default device
mmdevice debug: display name changed: VLC media player (LibVLC 3.0.23)
mmdevice debug: version 2 session control unavailable
mmdevice debug: volume from -65.250000 dB to +0.000000 dB with 0.031250 dB increments
main debug: using audio output module "mmdevice"
main debug: keeping audio output
main debug: looking for interface module matching "rc,none": 16 candidates
oldrc debug: base: 127.0.0.1, port: 63020
main debug: net: listening to 127.0.0.1 port 63020
main debug: using interface module "oldrc"
main debug: looking for interface module matching "hotkeys,none": 16 candidates
main debug: using interface module "hotkeys"
main debug: looking for interface module matching "globalhotkeys,none": 16 candidates
main debug: using interface module "win32"
main: Running vlc with the default interface. Use 'cvlc' to use vlc without interface.
main debug: looking for interface module matching "any": 16 candidates
main debug: looking for extension module matching "any": 1 candidates
lua debug: Opening Lua Extension module
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\extensions
lua debug: Trying Lua scripts in D:\VideoLAN\VLC\lua\extensions
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\extensions\VLSub.luac
lua debug: Scanning Lua script D:\VideoLAN\VLC\lua\extensions\VLSub.luac
lua debug: Script D:\VideoLAN\VLC\lua\extensions\VLSub.luac has the following capability flags: 0x5
main debug: using extension module "lua"
main debug: using interface module "qt"
main debug: processing request item: null, node: Playlist, skip: 0
main debug: rebuilding array of current - root Playlist
main debug: rebuild done - 1 items, index -1
main debug: starting playback of new item
main debug: resyncing on rtp://127.0.0.1:62020
main debug: rtp://127.0.0.1:62020 is at 0
main debug: creating new input thread
main debug: Creating an input for 'rtp://127.0.0.1:62020'
main debug: requesting art for new input thread
main debug: using timeshift granularity of 50 MiB
main debug: using timeshift path: C:\Users\ROG\AppData\Local\Temp
main debug: `rtp://@127.0.0.1:62020' gives access `rtp' demux `any' path `@127.0.0.1:62020'
main debug: creating demux: access='rtp' demux='any' location='@127.0.0.1:62020' file='\\@127.0.0.1:62020'
main debug: looking for access_demux module matching "rtp": 15 candidates
main debug: looking for meta fetcher module matching "any": 1 candidates
qt debug: IM: Setting an input
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\meta\fetcher
lua debug: Trying Lua scripts in D:\VideoLAN\VLC\lua\meta\fetcher
main debug: no meta fetcher modules matched
main debug: looking for art finder module matching "any": 2 candidates
main debug: net: opening 127.0.0.1 datagram port 62020
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\meta\art
lua debug: Trying Lua scripts in D:\VideoLAN\VLC\lua\meta\art
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\art\00_musicbrainz.luac
lua debug: skipping script (unmatched scope) D:\VideoLAN\VLC\lua\meta\art\00_musicbrainz.luac
main debug: using access_demux module "rtp"
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\art\01_googleimage.luac
main debug: looking for meta reader module matching "any": 2 candidates
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\meta\reader
lua debug: skipping script (unmatched scope) D:\VideoLAN\VLC\lua\meta\art\01_googleimage.luac
lua debug: Trying Lua scripts in D:\VideoLAN\VLC\lua\meta\reader
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\art\02_frenchtv.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\reader\filename.luac
lua debug: skipping script (unmatched scope) D:\VideoLAN\VLC\lua\meta\art\02_frenchtv.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\art\03_lastfm.luac
main debug: no meta reader modules matched
main debug: `rtp://@127.0.0.1:62020' successfully opened
lua debug: skipping script (unmatched scope) D:\VideoLAN\VLC\lua\meta\art\03_lastfm.luac
main debug: no art finder modules matched
rtp debug: detected MPEG2 TS
rtp debug: added payload type 33 (f = 90000 Hz)
rtp debug: added RTP source (0844cc73)
main debug: creating demux: access='' demux='ts' location='' file='(null)'
main debug: looking for demux module matching "ts": 56 candidates
ts debug: Standard set to Auto
main debug: using demux module "ts"
ts debug: DEMUX_SET_GROUP -1 00000000
ts debug: PATCallBack called
ts debug: new PAT ts_id=1 version=0 current_next=1
ts debug:   * number=1 pid=256
ts debug: temporary receiving program 1
ts debug: PMTCallBack called for program 1
ts debug: new PMT program number=1 version=0 pid_pcr=257
ts debug:   * pid=257 type=0x24 ISO/IEC Reserved
ts debug:    => pid 257 has now es fcc=hevc
main debug: selecting program id=1
main debug: looking for packetizer module matching "any": 25 candidates
main debug: using packetizer module "hevc"
main debug: looking for video decoder module matching "any": 19 candidates
avcodec debug: using ffmpeg Lavc58.134.100
avcodec debug: CPU flags: 0x000fd3db
avcodec debug: allowing 10 thread(s) for decoding
avcodec debug: codec (hevc) started
avcodec debug: using frame thread mode with 10 threads
main debug: using video decoder module "avcodec"
ts debug: Default program is 1
ts debug:   * pid=258 type=0xf ISO/IEC 13818-7 Audio with ADTS transport
ts debug:      - SL/FMC descriptor not found/matched
ts debug:    => pid 258 has now es fcc=mp4a
main debug: looking for packetizer module matching "any": 25 candidates
mpeg4audio debug: running MPEG4 audio packetizer
mpeg4audio debug: ADTS Mode
main debug: using packetizer module "mpeg4audio"
main debug: looking for audio decoder module matching "any": 21 candidates
main debug: using audio decoder module "faad"
ts debug:   * pid=17 listening for SDT
ts debug: enabling pid 257 from program 1
ts debug: enabling pid 258 from program 1
ts debug: enabling pcr pid 257 from program 1
main debug: Buffering 0%
main debug: Buffering 6%
mpeg4audio: AAC channels: 2 samplerate: 44100
main debug: restarting module due to input format change
main debug: removing module "faad"
main debug: looking for audio decoder module matching "any": 21 candidates
main debug: using audio decoder module "faad"
faad warning: decoded zero sample
main debug: reusing audio output
main debug: looking for aout stream module matching "any": 2 candidates
main debug: restarting module due to input format change
main debug: removing module "avcodec"
main debug: looking for video decoder module matching "any": 19 candidates
avcodec debug: using ffmpeg Lavc58.134.100
avcodec debug: CPU flags: 0x000fd3db
avcodec debug: allowing 10 thread(s) for decoding
avcodec debug: codec (hevc) started
avcodec debug: using frame thread mode with 10 threads
main debug: using video decoder module "avcodec"
avcodec debug: available hardware decoder output format 53 (dxva2_vld)
avcodec debug: available hardware decoder output format 118 (d3d11va_vld)
avcodec debug: available hardware decoder output format 174 (d3d11)
avcodec debug: available software decoder output format 0 (yuv420p)
main debug: Buffering 13%
avcodec debug: trying format d3d11va_vld
main debug: looking for text renderer module matching "any": 3 candidates
freetype debug: Using DWrite backend
freetype debug: DWrite_GetFamily(): family name: Arial
freetype debug: DWrite_ParseFamily(): using font at index 0 with weight 400 for bold: 0, italic: 0
freetype debug: DWrite_ParseFamily(): using font at index 1 with weight 700 for bold: 1, italic: 0
freetype debug: DWrite_ParseFamily(): using font at index 2 with weight 400 for bold: 0, italic: 1
freetype debug: DWrite_ParseFamily(): using font at index 3 with weight 700 for bold: 1, italic: 1
main debug: using text renderer module "freetype"
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 32x32 (32x32) chroma: YUVA -> 16x16 (16x16) chroma: RGBA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: looking for video converter module matching "any": 24 candidates
yuvp debug: YUVP to YUVA converter
main debug: using video converter module "yuvp"
main debug: Deinterlacing available
main debug: deinterlace -1, mode auto, is_needed 0
main debug: looking for vout window module matching "qt,any": 3 candidates
qt debug: requesting video window...
main debug: resized to 73581056x53937752
main debug: resized to 73581056x53937752
main debug: resized to 73580800x53937752
main debug: resized to 73580800x53937752
main debug: resized to 73580288x53937752
main debug: resized to 100x30
main debug: resized to 100x30
main debug: resized to 100x30
main debug: resized to 100x30
main debug: resized to 100x30
main debug: resized to 100x30
main debug: resized to 2560x1382
main debug: resized to 2560x1382
main debug: resized to 2560x1382
main debug: resized to 2560x1382
main debug: Buffering 19%
main debug: resized to 2560x1382
main debug: resized to 2560x1382
main debug: resized to 2560x1382
main debug: resized to 1280x720
main debug: resized to 1280x720
main debug: resized to 1280x720
main debug: resized to 1280x720
main debug: resized to 1280x720
main debug: resized to 1280x720
main debug: resized to 1280x720
main debug: resized to 1280x720
main debug: resized to 1280x720
main debug: using vout window module "qt"
main debug: looking for inhibit module matching "any": 1 candidates
main debug: using inhibit module "inhibit"
main debug: Opening vout display wrapper
main debug: looking for vout display module matching "any": 12 candidates
direct3d11 debug: creating Vout EventThread
direct3d11 debug: Win32VoutCreateWindow
main debug: VoutDisplayEvent 'resize' 1280x720
qt debug: Logical video size: 1280x720
main debug: resized to 1280x720
main debug: VoutDisplayEvent 'resize' 1280x720
wasapi debug: modified format
direct3d11 debug: inclinometer not found. (hr=0x80070490)
direct3d11 debug: created video sub-window
direct3d11 debug: Vout EventThread running
main debug: Buffering 26%
main debug: Buffering 33%
main debug: Buffering 39%
direct3d11 debug: Created the D3D11 device type 1 level b100.
direct3d11 debug: supports colorspace RGB Rec.709 gamma:22 range:FULL
direct3d11 debug: supports colorspace RGB Rec.709 gamma:22 range:STUDIO
direct3d11 debug: supports colorspace RGB Rec.2020 gamma:22 range:STUDIO
direct3d11 debug: supports colorspace RGB Rec.2020 gamma:22 range:FULL
direct3d11 debug: supports colorspace RGB Rec.2020 gamma:2084 range:FULL
direct3d11 debug: supports colorspace RGB Rec.2020 gamma:2084 range:STUDIO
direct3d11 debug: Output max luminance: 400.0, colorspace RGB Rec.709 gamma:22 range:FULL, bits per pixel 10
direct3d11 debug: using colorspace RGB Rec.709 gamma:22 range:FULL
direct3d11 debug: Using pixel format VA_NV12 for chroma DX11
main debug: VoutDisplayEvent 'resize' 1302x776
main debug: Buffering 46%
direct3d11 debug: using GPU render fence
direct3d11 debug: Direct3D11 resources created
direct3d11 debug: Direct3D11 device adapter successfully initialized
direct3d11 debug: Direct3D11 Open Succeeded
main debug: using vout display module "direct3d11"
direct3d11 debug: D3D11 pool succeed with 48 surfaces (1280x736) context 0x09b13c00
main debug: original format sz 1280x736, of (0,0), vsz 1280x720, 4cc DX11, sar 1:1, msk r0x0 g0x0 b0x0
main debug: removing module "freetype"
main debug: VoutDisplayEvent 'resize' 1280x720
main debug: looking for text renderer module matching "any": 3 candidates
direct3d11 debug: Detected size change 1280x720
freetype debug: Using DWrite backend
freetype debug: DWrite_GetFamily(): family name: Arial
freetype debug: DWrite_ParseFamily(): using font at index 0 with weight 400 for bold: 0, italic: 0
freetype debug: DWrite_ParseFamily(): using font at index 1 with weight 700 for bold: 1, italic: 0
freetype debug: DWrite_ParseFamily(): using font at index 2 with weight 400 for bold: 0, italic: 1
freetype debug: DWrite_ParseFamily(): using font at index 3 with weight 700 for bold: 1, italic: 1
qt debug: Logical video size: 1280x720
main debug: resized to 1280x720
main debug: VoutDisplayEvent 'resize' 1280x720
main debug: using text renderer module "freetype"
main debug: looking for hw decoder module matching "any": 2 candidates
d3d11va debug: Reusing Direct3D11 device
d3d11va debug: CreateDevice succeed
d3d11va warning: surface dimensions (1280x768) differ from avcodec dimensions (1280x736)
d3d11va debug: - 'MPEG-2 & MPEG-1 variable-length decoder' is supported
d3d11va debug: - 'MPEG-2 variable-length decoder' is supported
d3d11va debug: - 'MPEG-1 variable-length decoder, no D pictures' is supported
d3d11va debug: - 'VC-1 variable-length decoder' is supported
d3d11va debug: - 'VC-1 variable-length decoder' is supported
d3d11va debug: - 'Unknown decoder 0x32fcfe3f-0xde46-0x4a49-0x861bac71110649d5' is supported
d3d11va debug: - 'H.264 stereo high profile, mbs flag set' is supported
d3d11va debug: - 'H.264 stereo high profile' is supported
d3d11va debug: - 'H.264 variable-length decoder, no film grain technology' is supported
d3d11va debug: - 'HEVC Main profile' is supported
d3d11va debug: - 'HEVC Main 10 profile' is supported
d3d11va debug: - 'Unknown decoder 0x1a72925f-0x0c2c-0x4f15-0x96fbb17d1473603f' is supported
d3d11va debug: - 'Unknown decoder 0x15df9b21-0x06c4-0x47f1-0x841ea67c97d7f312' is supported
d3d11va debug: - 'Unknown decoder 0x4008018f-0xf537-0x4b36-0x98cf61af8a2c1a33' is supported
d3d11va debug: - 'Unknown decoder 0x0dabeffa-0x4458-0x4602-0xbc030795659d617c' is supported
d3d11va debug: - 'Unknown decoder 0x9798634d-0xfe9d-0x48e5-0xb4dadbec45b3df01' is supported
d3d11va debug: - 'MPEG-4 Part 2 variable-length decoder, Simple Profile' is supported
d3d11va debug: - 'MPEG-4 Part 2 variable-length decoder, Simple&Advanced Profile, no GMC' is supported
d3d11va debug: - 'MPEG-4 Part 2 nVidia bitstream decoder' is supported
d3d11va debug: - 'Unknown decoder 0x33fcfe41-0xde46-0x4a49-0x861bac71110649d5' is supported
d3d11va debug: - 'VP9 profile 0' is supported
d3d11va debug: - 'VP9 profile 2' is supported
d3d11va debug: - 'Unknown decoder 0xdda19dc7-0x93b5-0x49f5-0xa9b32bda28a2ce6e' is supported
d3d11va debug: - 'AV1 Main profile 8' is supported
d3d11va debug: - 'Unknown decoder 0x6affd11e-0x1d96-0x42b1-0xa21593a31f09a53d' is supported
d3d11va debug: - 'Unknown decoder 0x914c84a3-0x4078-0x4fa9-0x984ce2f262cb5c9c' is supported
d3d11va debug: - 'Unknown decoder 0x8a1a1031-0x29bc-0x46d0-0xa007e9b092ca6767' is supported
d3d11va debug: Trying to use 'HEVC Main profile' as input
d3d11va debug: NV12 output is supported for decoder HEVC Main profile.
d3d11va debug: Using output format NV12 for decoder HEVC Main profile
d3d11va debug: va_pool_SetupDecoder id 173 1280x736 count: 28
d3d11va warning: not enough decoding slices in the texture (6/28)
d3d11va debug: ID3D11VideoDecoderOutputView succeed with 28 surfaces (1280x768)
d3d11va debug: we got 2 decoder configurations
d3d11va debug: configuration[0] ConfigBitstreamRaw 1
d3d11va debug: configuration[1] ConfigBitstreamRaw 1
d3d11va debug: DxCreateDecoderSurfaces succeed
main debug: using hw decoder module "d3d11va"
avcodec: Using D3D11VA (NVIDIA GeForce RTX 4060 Laptop GPU, vendor 10de(NVIDIA), device 28e0, revision a1) for hardware decoding
main debug: Buffering 53%
main debug: Received first picture
main debug: Buffering 59%
wasapi debug: buffer size    : 96000 frames
wasapi debug: maximum latency: 000 ns
wasapi debug: default period : 10000000 ns
wasapi debug: minimum period : 3000000 ns
main debug: using aout stream module "wasapi"
main debug: output 'f32l' 48000 Hz Stereo frame=1 samples/8 bytes
main debug: looking for audio volume module matching "any": 2 candidates
main debug: using audio volume module "float_mixer"
main debug: input 'f32l' 44100 Hz Stereo frame=1 samples/8 bytes
main debug: looking for audio filter module matching "scaletempo": 16 candidates
scaletempo debug: format: 44100 rate, 2 nch, 4 bps, fl32
scaletempo debug: params: 30 stride, 0.200 overlap, 14 search
scaletempo debug: 1.000 scale, 1323.000 stride_in, 1323 stride_out, 1059 standing, 264 overlap, 617 search, 2204 queue, fl32 mode
main debug: using audio filter module "scaletempo"
main debug: conversion: 'f32l'->'f32l' 44100 Hz->44100 Hz Stereo->Stereo
main debug: conversion pipeline complete
main debug: conversion: 'f32l'->'f32l' 44100 Hz->44100 Hz Stereo->Stereo
main debug: conversion pipeline complete
main debug: looking for audio resampler module matching "any": 3 candidates
main debug: using audio resampler module "samplerate"
main debug: Buffering 66%
main debug: Buffering 73%
main debug: Buffering 79%
main debug: Buffering 86%
main debug: Buffering 93%
main debug: Buffering 99%
main debug: Stream buffering done (1066 ms in 955 ms)
main debug: Decoder wait done in 0 ms
main debug: inserting 9594 zeroes
mmdevice debug: state changed: 1
main warning: playback too late (78576): up-sampling
main debug: auto hiding mouse cursor
main warning: picture is too late to be displayed (missing 32 ms)
main debug: resampling stopped (drift: -12778 us)
main debug: accepted socket 2224 (from socket 1172)
main debug: looking for encoder module matching "any": 24 candidates
main debug: using encoder module "png"
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to build resize+chroma
main debug: looking for video converter module matching "any": 24 candidates
main debug: no video converter modules matched
main error: Failed to create video converter
chain debug: Trying to build chroma+resize
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (19d347a0) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (048c1800) appended to chain
main debug: using video converter module "chain"
main debug: Filter 'chain' (199c6298) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: RV24 -> 1280x720 (1280x720) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (05d7b310) appended to chain
main debug: using video converter module "chain"
main debug: removing module "png"
main debug: removing module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 19d347a0 removed from chain
main debug: removing module "swscale"
main debug: Filter 048c1800 removed from chain
main debug: Filter 199c6298 removed from chain
main debug: removing module "swscale"
main debug: Filter 05d7b310 removed from chain
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r278-native-2026-10-20-09h50m17s586.png)
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (05f3f788) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: YUVA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (199c6298) appended to chain
main debug: using video converter module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 05f3f788 removed from chain
main debug: removing module "swscale"
main debug: Filter 199c6298 removed from chain
main error: write error: No error
main error: read error: No error
main debug: accepted socket 3176 (from socket 1172)
main debug: looking for encoder module matching "any": 24 candidates
main debug: using encoder module "png"
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to build resize+chroma
main debug: looking for video converter module matching "any": 24 candidates
main debug: no video converter modules matched
main error: Failed to create video converter
chain debug: Trying to build chroma+resize
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (199c6298) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (048ce410) appended to chain
main debug: using video converter module "chain"
main debug: Filter 'chain' (1a508cc8) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: RV24 -> 1280x720 (1280x720) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (048bfac0) appended to chain
main debug: using video converter module "chain"
main debug: removing module "png"
main debug: removing module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 199c6298 removed from chain
main debug: removing module "swscale"
main debug: Filter 048ce410 removed from chain
main debug: Filter 1a508cc8 removed from chain
main debug: removing module "swscale"
main debug: Filter 048bfac0 removed from chain
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r278-native-2026-10-20-09h51m50s766.png)
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (1a00ce98) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: YUVA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (1a508cc8) appended to chain
main debug: using video converter module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 1a00ce98 removed from chain
main debug: removing module "swscale"
main debug: Filter 1a508cc8 removed from chain
main error: read error: No error
main debug: accepted socket 3160 (from socket 1172)
main debug: exiting
main debug: exiting
main debug: no exit handler
main debug: removing all interfaces
main debug: removing module "qt"
main debug: deactivating the playlist
main debug: incoming request - stopping current input
main debug: killing decoder fourcc `hevc'
main debug: removing module "avcodec"
main error: read error: No error
main debug: saving a free vout
main debug: reusing provided vout
main debug: removing module "hevc"
main debug: killing decoder fourcc `mp4a'
main debug: removing module "faad"
mmdevice debug: state changed: 0
wasapi debug: reset
main debug: removing module "samplerate"
main debug: removing module "scaletempo"
main debug: removing module "float_mixer"
main debug: keeping audio output
main debug: removing module "mpeg4audio"
main debug: removing module "rtp"
rtp debug: removing RTP source (0844cc73)
ts debug: Can't read TS packet at 100137072
main debug: removing module "ts"
main debug: Program doesn't contain anymore ES
main debug: dead input
qt debug: IM: Deleting the input
main debug: nothing to play
main debug: destroying useless vout
main debug: removing module "direct3d11"
direct3d11 debug: Direct3D11 resources destroyed
direct3d11 debug: Direct3D11 device adapter closed
direct3d11 error: SetThumbNailClip failed: 0x800706f4
direct3d11 debug: Win32 Vout EventThread terminating
direct3d11 debug: Win32VoutCloseWindow
direct3d11 debug: WinProc WM_DESTROY
main debug: removing module "inhibit"
qt debug: releasing video...
qt debug: video widget is released
main debug: removing module "freetype"
main debug: removing module "yuvp"
main debug: removing module "swscale"
main debug: removing module "mmdevice"
qt debug: requesting exit...
qt debug: waiting for UI thread...
qt debug: video widget is orphaned
main debug: exiting
main debug: no exit handler
qt debug: QApp exec() finished
qt debug: video widget is orphaned
qt debug: Killing extension dialog provider
qt debug: ExtensionsDialogProvider is quitting...
main debug: removing module "lua"
main debug: removing module "win32"
main debug: removing module "hotkeys"
main debug: removing module "oldrc"
main debug: destroying
main debug: saving media library to file C:\Users\ROG\AppData\Roaming\vlc\ml.xspf.tmp3132
main debug: looking for playlist export module matching "export-xspf": 4 candidates
main debug: using playlist export module "export"
main debug: removing module "export"
main debug: deleting item `Media Library'
main debug: deleting item `rtp://127.0.0.1:62020'
main debug: deleting item `Playlist'
main debug: removing module "memory"
-- logger module stopped --

~~~

清理完成：按清单删除15个文件，r278文件与六个PID均无残留；复用120秒源保留。未启动远程测试。
