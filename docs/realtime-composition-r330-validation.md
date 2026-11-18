# r330：协议预约工作事实原规格复验

三路连续120秒 H264/AAC RTP→HEVC CBR 8000kbps 1280×720/30、AAC CBR 192kbps 44100Hz stereo→MPEG-TS/RTP62024。仅realtime、VLC原生快照，使用第四切片新Release。已执行但收尾证据缺失，不计验收通过。

## 启动命令

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r330-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r330-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r330-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r330-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r330-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r330-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r330 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62024 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r330-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r330-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r330-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r330-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r330-native- --snapshot-format=png rtp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r330-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

## 结果：证据不完整

Release745项全量重建成功、configure/build exit0，四项产物齐全。源37904/2968/3484日志均记录3600帧/120秒；CLI23772、VLC11580曾正常输出，原生快照已确认三格。续接时监控句柄92088不存在，五PID无残留。日志停在09:53:14、监控约151.46秒；没有end_reason、最终runtime或sender报告，退出码未知。09:50–10:00应用事件1000未查到该CLI记录，不能据此断言自然退出或已复现崩溃。没有失源原生快照、最终计数或完整释放证据，需同构建重跑；不把该轮记为成功。

~~~~text
[CLI] runtime report: queued=1, peakQueued=940, threads=82, processThreads=129, workers=76, workerProgress=557589, workerProcessCalls=1941942, workerWaits=1384346, workerWakeups=1373171, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=454, logicalProcessors=22, averageSystemMachineCpuPercent=21.394119, averageProcessMachineCpuPercent=2.398157, peakProcessMachineCpuPercent=5.482456, averageProcessSingleCoreCpuPercent=52.759461, peakProcessSingleCoreCpuPercent=120.614035, initialWorkingSetBytes=208658432, workingSetBytes=247246848, peakWorkingSetBytes=271937536, totalPushed=448403, totalPopped=448378, droppedBuffers=0, encodedPacketsPushed=49040, encodedPacketsPopped=49039, backpressureItems=124, graphPayloadCurrentBytes=132, graphPayloadHighWaterBytes=18086973, graphPayloadCurrentObjects=2, graphPayloadHighWaterObjects=773, graphPayloadReservations=144916, graphPayloadReleases=144914, graphPayloadPressureFailures=0, lastInputReceivedAtNs=4026763864000
[CLI] runtime report: queued=1, peakQueued=940, threads=82, processThreads=129, workers=76, workerProgress=557834, workerProcessCalls=1942628, workerWaits=1384788, workerWakeups=1373589, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=455, logicalProcessors=22, averageSystemMachineCpuPercent=21.378209, averageProcessMachineCpuPercent=2.395396, peakProcessMachineCpuPercent=5.482456, averageProcessSingleCoreCpuPercent=52.698702, peakProcessSingleCoreCpuPercent=120.614035, initialWorkingSetBytes=208658432, workingSetBytes=247246848, peakWorkingSetBytes=271937536, totalPushed=448590, totalPopped=448565, droppedBuffers=0, encodedPacketsPushed=49141, encodedPacketsPopped=49140, backpressureItems=124, graphPayloadCurrentBytes=132, graphPayloadHighWaterBytes=18086973, graphPayloadCurrentObjects=2, graphPayloadHighWaterObjects=773, graphPayloadReservations=144982, graphPayloadReleases=144980, graphPayloadPressureFailures=0, lastInputReceivedAtNs=4026763864000
~~~~

## 清理清单

D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r330-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r330-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r330-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r330-native-2026-10-26-09h51m16s706.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r330-output.sdp
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r330-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r330-source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r330-source1-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r330-source1.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r330-source2-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r330-source2.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r330-vlc.log

以上12项已清理，五PID无残留；指定120秒源保留。
