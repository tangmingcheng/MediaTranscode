# r317：本机网络权限下原规格前缀复验（发送窗口失败）

与r316媒体规格及端口相同，以提升权限执行本机进程链验证。三路120秒H264/AAC RTP→HEVC CBR8000kbps 1280×720/30、AAC192kbps→MPEG-TS/UDP62024。仅realtime与VLC原生快照。

## 实际命令

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r317-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r317-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r317-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r317-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r317-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r317-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r317 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r317-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r317-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r317-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r317-native- --snapshot-format=png udp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r317-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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


## 失败结果

源PID36968/39236/40672，CLI13880，监控会话42062。CLI约6秒退出1，VLC尚未到10秒启动点，未产生快照。提升权限运行已接收真实RTP并进入生产DAG；不能仅由此断言r316零包根因已完全定位。

sequence74：release5547802434ns，deadline5547802436ns，窗口仅2ns；GBRA duration215040ns、rate6250000bytes/s、queue_wire_bytes1344，因此not_before5547802434大于not_after5547587396，发送器正确拒绝。说明正发布窗口不能代替基于现有速率的必要发送时长约束。须由planner在发布前推导可行性并选择维护边界，不能放宽AU期限或假设OS服务保证。

最终payload余额0，reservations/releases各1441，highWater17026718bytes，workerErrors2。CLI内部CPU samples0，短运行不足以评价持续CPU/内存趋势或A/V漂移。原规格失源/重入验收未完成。

关键原始日志：
~~~~text
[2026-10-23 14:18:13.371] [info] [graph][runtime.node] datagram_reservation_trace stage=reservation_failed generation=1 endpoint=1 sequence=74 release_master_ns=5547802434 deadline_master_ns=5547802436 original_protocol_deadline_master_ns=5647802434 prefix_completion_limit_master_ns=5547802436 activation_started_master_ns=5537105000 activation_completed_master_ns=5537105800 permit_started_master_ns=5537106100 permit_completed_master_ns=5537106100 snapshot_started_master_ns=5537106200 snapshot_sampled_master_ns=5537106300 snapshot_completed_master_ns=5537106500 reservation_completed_master_ns=5537127000 wait_started_master_ns=unavailable wait_completed_master_ns=unavailable submit_observed_master_ns=unavailable original_error="IoFailure: Datagram GBRA reservation misses its immutable completion deadline sequence=74 now_ns=5537106300 release_ns=5547802434 deadline_ns=5547802436 not_before_ns=5547802434 not_after_ns=5547587396 duration_ns=215040 rate=6250000 queue_wire_bytes=1344 average_queue_residence_ns=41600"
[2026-10-23 14:18:13.372] [info] [graph][runtime.node] process.failed node=81 name=MediaScheduledDatagramSenderNode kind=ScheduledDatagramSender inputs=[channel=111 edge=111 stream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=123 edge=123 stream=Metadata edge_kind=Unknown payload=Unknown queued=0] outputs=[] error=IoFailure: Datagram GBRA reservation misses its immutable completion deadline sequence=74 now_ns=5537106300 release_ns=5547802434 deadline_ns=5547802436 not_before_ns=5547802434 not_after_ns=5547587396 duration_ns=215040 rate=6250000 queue_wire_bytes=1344 average_queue_residence_ns=41600
[2026-10-23 14:18:13.372] [info] [graph][runtime.node] worker.failed node=81 kind=ScheduledDatagramSender name=Common GCRA-paced nonblocking datagram sender error=IoFailure: Datagram GBRA reservation misses its immutable completion deadline sequence=74 now_ns=5537106300 release_ns=5547802434 deadline_ns=5547802436 not_before_ns=5547802434 not_after_ns=5547587396 duration_ns=215040 rate=6250000 queue_wire_bytes=1344 average_queue_residence_ns=41600
[2026-10-23 14:18:13.385] [info] [graph][runtime.node] worker.secondary_failure node=81 kind=ScheduledDatagramSender name=Common GCRA-paced nonblocking datagram sender error=IoFailure: Datagram GBRA reservation misses its immutable completion deadline sequence=74 now_ns=5537106300 release_ns=5547802434 deadline_ns=5547802436 not_before_ns=5547802434 not_after_ns=5547587396 duration_ns=215040 rate=6250000 queue_wire_bytes=1344 average_queue_residence_ns=41600
[2026-10-23 14:18:13.559] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=failed detail="Datagram GBRA reservation misses its immutable completion deadline sequence=74 now_ns=5537106300 release_ns=5547802434 deadline_ns=5547802436 not_before_ns=5547802434 not_after_ns=5547587396 duration_ns=215040 rate=6250000 queue_wire_bytes=1344 average_queue_residence_ns=41600" description="" error=6 native=0 stage=3 reason="Datagram GBRA reservation misses its immutable completion deadline sequence=74 now_ns=5537106300 release_ns=5547802434 deadline_ns=5547802436 not_before_ns=5547802434 not_after_ns=5547587396 duration_ns=215040 rate=6250000 queue_wire_bytes=1344 average_queue_residence_ns=41600"
[2026-10-23 14:18:13.559] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=retired detail="Datagram GBRA reservation misses its immutable completion deadline sequence=74 now_ns=5537106300 release_ns=5547802434 deadline_ns=5547802436 not_before_ns=5547802434 not_after_ns=5547587396 duration_ns=215040 rate=6250000 queue_wire_bytes=1344 average_queue_residence_ns=41600" description="" error=6 native=0 stage=3 reason="Datagram GBRA reservation misses its immutable completion deadline sequence=74 now_ns=5537106300 release_ns=5547802434 deadline_ns=5547802436 not_before_ns=5547802434 not_after_ns=5547587396 duration_ns=215040 rate=6250000 queue_wire_bytes=1344 average_queue_residence_ns=41600"
[CLI] final runtime report: queued=0, peakQueued=936, threads=81, processThreads=86, workers=0, workerProgress=6036, workerProcessCalls=16410, workerWaits=10367, workerWakeups=10285, workerErrors=2, errors=2, stalledIntervals=0, cpuSamples=0, logicalProcessors=22, averageSystemMachineCpuPercent=0.000000, averageProcessMachineCpuPercent=0.000000, peakProcessMachineCpuPercent=0.000000, averageProcessSingleCoreCpuPercent=0.000000, peakProcessSingleCoreCpuPercent=0.000000, initialWorkingSetBytes=212717568, workingSetBytes=212717568, peakWorkingSetBytes=212717568, totalPushed=5855, totalPopped=5180, droppedBuffers=0, encodedPacketsPushed=33, encodedPacketsPopped=32, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=17026718, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=784, graphPayloadReservations=1441, graphPayloadReleases=1441, graphPayloadPressureFailures=0, lastInputReceivedAtNs=19852282834600
~~~~

## 清理

三个源强制清理退出-1，非自然结束。监控末次约5.14秒CPU0.208%、WS203907072、private718524416；短时数据不能外推稳定趋势。清理文件清单：
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r317-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r317-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r317-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r317-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r317-source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r317-source1-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r317-source1.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r317-source2-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r317-source2.log

清理后已提升权限复核本轮精确PID均无残留，按本轮前缀查询临时产物剩余0；复用120秒源保留。
