# r318：服务时长约束修复后原规格复验（提交期限失败）

三路120秒H264/AAC RTP→HEVC CBR8000kbps 1280×720/30、AAC192kbps→MPEG-TS/UDP62024。沿用原规格、本机网络权限、VLC原生快照；仅realtime。

## 实际启动命令

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r318-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r318-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r318-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r318-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r318-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r318-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r318 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r318-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r318-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r318-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r318-native- --snapshot-format=png udp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r318-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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


## 结果

源PID3296/41892/35704，CLI7728，监控会话34115。最终Release14:30:43。CLI约6秒退出1，未到VLC启动点；无画面/失源/重入通过证据。

r317的2ns窗口未再出现，但sequence47提交迟到：原AUdeadline5601864268ns；prefix limit5535197600ns；GBRA实际允许提交截至5534982560ns；observed5535032300ns，迟到49740ns。wait起5534167000、返回5535031600；not_before5534274540。说明必要发送时长检查仍不能覆盖调度迟到。必须继续审查既有部署jitter预算如何约束发布窗口，不允许增加经验margin或放宽原AUdeadline。

原始关键日志：
~~~~text
[2026-10-23 14:31:01.209] [info] [graph][runtime.node] datagram_reservation_trace stage=submit_deadline_missed generation=1 endpoint=1 sequence=47 release_master_ns=5501864268 deadline_master_ns=5535197600 original_protocol_deadline_master_ns=5601864268 prefix_completion_limit_master_ns=5535197600 activation_started_master_ns=5534164300 activation_completed_master_ns=5534165000 permit_started_master_ns=5534165200 permit_completed_master_ns=5534165300 snapshot_started_master_ns=5534165400 snapshot_sampled_master_ns=5534165500 snapshot_completed_master_ns=5534165700 reservation_completed_master_ns=5534166600 wait_started_master_ns=5534167000 wait_completed_master_ns=5535031600 submit_observed_master_ns=5535032300 original_error="IoFailure: scheduled datagram submit exceeded its original deadline generation=1 sequence=47 endpoint=1 release_ns=5501864268 reserved_at_ns=5534165500 not_before_ns=5534274540 deadline_ns=5534982560 observed_at_ns=5535032300"
[2026-10-23 14:31:01.211] [info] [graph][runtime.node] datagram_batch_trace stage=last_completed generation=1 first_sequence=46 last_sequence=46 maximum_submit_interval_ns=71300 maximum_submit_interval_sequence=46 maximum_start_lateness_ns=263660 maximum_start_lateness_sequence=46 first_release_master_ns=5501864268 first_deadline_master_ns=5535197600 last_release_master_ns=5501864268 last_deadline_master_ns=5535197600 first_submitted_master_ns=5534059500 last_submitted_master_ns=5534059500 worst_wait_started_master_ns=5533652900 worst_wait_completed_master_ns=5533987700 worst_submit_observed_master_ns=5533988200
[2026-10-23 14:31:01.423] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=failed detail="scheduled datagram submit exceeded its original deadline generation=1 sequence=47 endpoint=1 release_ns=5501864268 reserved_at_ns=5534165500 not_before_ns=5534274540 deadline_ns=5534982560 observed_at_ns=5535032300" description="" error=6 native=0 stage=3 reason="scheduled datagram submit exceeded its original deadline generation=1 sequence=47 endpoint=1 release_ns=5501864268 reserved_at_ns=5534165500 not_before_ns=5534274540 deadline_ns=5534982560 observed_at_ns=5535032300"
[CLI] final runtime report: queued=0, peakQueued=895, threads=81, processThreads=86, workers=0, workerProgress=5592, workerProcessCalls=14459, workerWaits=8859, workerWakeups=8779, workerErrors=2, errors=2, stalledIntervals=0, cpuSamples=0, logicalProcessors=22, averageSystemMachineCpuPercent=0.000000, averageProcessMachineCpuPercent=0.000000, peakProcessMachineCpuPercent=0.000000, averageProcessSingleCoreCpuPercent=0.000000, peakProcessSingleCoreCpuPercent=0.000000, initialWorkingSetBytes=212291584, workingSetBytes=212291584, peakWorkingSetBytes=212291584, totalPushed=5426, totalPopped=4802, droppedBuffers=0, encodedPacketsPushed=24, encodedPacketsPopped=21, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=15677912, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=735, graphPayloadReservations=1429, graphPayloadReleases=1429, graphPayloadPressureFailures=0, lastInputReceivedAtNs=20620127714100
~~~~

监控末值：
seconds=4.1047963 cpu=0 ws=155398144 private=576724992
seconds=5.131392 cpu=0.345913816279999 ws=200941568 private=684523520

## 清理

CLI退出1；三个源清理退出-1，非自然结束。文件清单：
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r318-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r318-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r318-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r318-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r318-source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r318-source1-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r318-source1.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r318-source2-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r318-source2.log

清理后提升权限核验四个精确PID均无残留；本轮9项临时产物已删除，前缀查询剩余0。120秒复用源保留。
