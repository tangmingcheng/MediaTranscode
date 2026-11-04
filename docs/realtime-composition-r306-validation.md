# r306：标准堆下捕获崩溃（已执行）

与r305相同原规格，仅cdb增加官方-hd选项使用标准堆。不能替代无调试器验收。

## 执行命令
~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r306-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r306-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r306-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r306-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r306-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r306-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$debugger=Start-Process -FilePath 'C:/Program Files (x86)/Windows Kits/10/Debuggers/x64/cdb.exe' -ArgumentList ('-G -hd -logo D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r306-cdb.log -y D:/Code/MyCode/MediaTranscode/out/build/x64-release -c "sxe -c2 \".exr -1;.ecxr;~* kb;.dump /ma D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r306-crash.dmp;qd\" 0xc0000409;g" D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe ' + '--media-id composition-r306 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62020 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2') -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r306-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r306-cli-error.log' -PassThru
Write-Output "CDB_PID=$($debugger.Id)"
$child=$null
$discovery=[Diagnostics.Stopwatch]::StartNew()
while(-not $child -and -not $debugger.HasExited -and $discovery.Elapsed.TotalSeconds -lt 15){
$child=Get-CimInstance Win32_Process -Filter "ParentProcessId=$($debugger.Id)" | Where-Object {$_.Name -eq 'media_transcode_realtime_video_cli.exe'}
if(-not $child){Start-Sleep -Milliseconds 100}
}
if(-not $child){throw 'Debugger did not create realtime CLI'}
$cli=[Diagnostics.Process]::GetProcessById($child.ProcessId)
$cliHandle=$cli.Handle
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r306-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r306-native- --snapshot-format=png udp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r306-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

## 诊断结果：未捕获致命异常，非验收通过
源PID17800/32540/31352，cdb3980，CLI34480，VLC6108。标准堆诊断超过120秒，未见fatal/dump。没有截图观察记录，不记画面验收。结束诊断后清理CLI和VLC，不记自然停止。
~~~~text
[CLI] runtime report: queued=0, peakQueued=1032, threads=81, processThreads=129, workers=75, workerProgress=334377, workerProcessCalls=1310764, workerWaits=976379, workerWakeups=965565, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=445, logicalProcessors=22, averageSystemMachineCpuPercent=14.011781, averageProcessMachineCpuPercent=2.164185, peakProcessMachineCpuPercent=5.225653, averageProcessSingleCoreCpuPercent=47.612063, peakProcessSingleCoreCpuPercent=114.964371, initialWorkingSetBytes=212578304, workingSetBytes=246030336, peakWorkingSetBytes=276955136, totalPushed=306187, totalPopped=306163, droppedBuffers=0, encodedPacketsPushed=47125, encodedPacketsPopped=47125, backpressureItems=123, graphPayloadCurrentBytes=66, graphPayloadHighWaterBytes=20641018, graphPayloadCurrentObjects=2, graphPayloadHighWaterObjects=864, graphPayloadReservations=143117, graphPayloadReleases=143115, graphPayloadPressureFailures=0, lastInputReceivedAtNs=6766782490300
[CLI] runtime report: queued=1, peakQueued=1032, threads=81, processThreads=129, workers=75, workerProgress=334602, workerProcessCalls=1311411, workerWaits=976802, workerWakeups=965963, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=446, logicalProcessors=22, averageSystemMachineCpuPercent=14.010340, averageProcessMachineCpuPercent=2.162330, peakProcessMachineCpuPercent=5.225653, averageProcessSingleCoreCpuPercent=47.571255, peakProcessSingleCoreCpuPercent=114.964371, initialWorkingSetBytes=212578304, workingSetBytes=246030336, peakWorkingSetBytes=276955136, totalPushed=306377, totalPopped=306352, droppedBuffers=0, encodedPacketsPushed=47227, encodedPacketsPopped=47226, backpressureItems=123, graphPayloadCurrentBytes=132, graphPayloadHighWaterBytes=20641018, graphPayloadCurrentObjects=2, graphPayloadHighWaterObjects=864, graphPayloadReservations=143185, graphPayloadReleases=143183, graphPayloadPressureFailures=0, lastInputReceivedAtNs=6766782490300
[CLI] runtime report: queued=1, peakQueued=1032, threads=81, processThreads=129, workers=75, workerProgress=334821, workerProcessCalls=1312051, workerWaits=977223, workerWakeups=966362, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=447, logicalProcessors=22, averageSystemMachineCpuPercent=14.008407, averageProcessMachineCpuPercent=2.159421, peakProcessMachineCpuPercent=5.225653, averageProcessSingleCoreCpuPercent=47.507260, peakProcessSingleCoreCpuPercent=114.964371, initialWorkingSetBytes=212578304, workingSetBytes=246030336, peakWorkingSetBytes=276955136, totalPushed=306564, totalPopped=306539, droppedBuffers=0, encodedPacketsPushed=47328, encodedPacketsPopped=47327, backpressureItems=123, graphPayloadCurrentBytes=132, graphPayloadHighWaterBytes=20641018, graphPayloadCurrentObjects=2, graphPayloadHighWaterObjects=864, graphPayloadReservations=143251, graphPayloadReleases=143249, graphPayloadPressureFailures=0, lastInputReceivedAtNs=6766782490300
[CLI] runtime report: queued=1, peakQueued=1032, threads=81, processThreads=129, workers=75, workerProgress=335046, workerProcessCalls=1312699, workerWaits=977647, workerWakeups=966762, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=448, logicalProcessors=22, averageSystemMachineCpuPercent=14.004162, averageProcessMachineCpuPercent=2.156763, peakProcessMachineCpuPercent=5.225653, averageProcessSingleCoreCpuPercent=47.448778, peakProcessSingleCoreCpuPercent=114.964371, initialWorkingSetBytes=212578304, workingSetBytes=246030336, peakWorkingSetBytes=276955136, totalPushed=306753, totalPopped=306728, droppedBuffers=0, encodedPacketsPushed=47429, encodedPacketsPopped=47428, backpressureItems=123, graphPayloadCurrentBytes=132, graphPayloadHighWaterBytes=20641018, graphPayloadCurrentObjects=2, graphPayloadHighWaterObjects=864, graphPayloadReservations=143317, graphPayloadReleases=143315, graphPayloadPressureFailures=0, lastInputReceivedAtNs=6766782490300
~~~~
## 清理清单
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r306-cdb.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r306-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r306-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r306-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r306-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r306-source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r306-source1-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r306-source1.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r306-source2-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r306-source2.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r306-vlc.log

原会话确认源退出码全部0，CLI=-1（结束诊断清理）。六PID无残留，清单11项已删除，原始120秒源保留。
