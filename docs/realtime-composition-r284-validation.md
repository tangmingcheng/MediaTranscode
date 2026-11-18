# r284：原规格启动失败数值诊断

状态：已执行，启动失败，未通过。与r283相同三路H.264 1280×720 30fps约8Mbps输入，HEVC 1280×720 30fps 8Mbps CBR及AAC 192kbps独立RTP输出；仅增加失败数值诊断，未改变规格或校验。该诊断不替代完整验收。

## 启动与监控命令

~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r284-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r284-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r284-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r284-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r284-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r284-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r284 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout separate --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r284-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r284-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r284-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10 -and (Test-Path -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r284-output.sdp')) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r284-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r284-native- --snapshot-format=png file:///D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r284-output.sdp' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r284-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

## 实际结果

诊断版本Release全量构建 configure/build=0，729项完成，必需产物齐全；realtime SHA256 EB69C0CE26FD527096B81331BFC7313A45BAFA5B8A677CA953635F46587775A7。源PID13520、40276、41580；CLI36048自行失败退出1。VLC未启动，无快照/重入。

[CLI] realtime video runtime failed: InvalidArgument: datagram shaping plan requires complete scope, service, resource, and backlog facts session_empty=0 generation=1 endpoints=4 service_authority_empty=0 pacing_bytes_per_second=6250000 maximum_bytes_per_second=6250000 burst_bytes=1500 backlog_datagrams=3805 backlog_bytes=2936694 residence_ns=100000000 batch_datagrams=53 batch_bytes=79403 memory_total=15227668 memory_userspace=14473918 memory_socket=756580

[CLI] final runtime report: queued=0, peakQueued=465, threads=80, processThreads=88, workers=0, workerProgress=2549, workerProcessCalls=6600, workerWaits=4033, workerWakeups=3974, workerErrors=1, errors=1, stalledIntervals=0, cpuSamples=0, logicalProcessors=22, averageSystemMachineCpuPercent=0.000000, averageProcessMachineCpuPercent=0.000000, peakProcessMachineCpuPercent=0.000000, averageProcessSingleCoreCpuPercent=0.000000, peakProcessSingleCoreCpuPercent=0.000000, initialWorkingSetBytes=208371712, workingSetBytes=208371712, peakWorkingSetBytes=208371712, totalPushed=2414, totalPopped=2036, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=8546871, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=426, graphPayloadReservations=751, graphPayloadReleases=751, graphPayloadPressureFailures=0, lastInputReceivedAtNs=11097772777000

用户态14473918加socket756580等于15230498，大于部署预算15227668，差2830字节；打印的其余guard边界满足。仅5秒采样，不能作为长期CPU/内存或A/V证据。stderr仍有color range警告及AAC关闭时1 frame left。源未完成120秒，不是完整验收。

测试已失败，准备核对精确PID身份后清理三源，再删除本轮9个明确日志；指定源保留。

清理完成：三源身份核验后按PID13520/40276/41580执行Stop-Process -Force，源退出码均-1；启动监控session80101结束1。9个日志已逐项清理，r284文件残留0，未删除指定复用源。清理退出不记为自然结束。
