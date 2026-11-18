# r305：prefix修改后崩溃现场捕获（已执行）

沿用r304原规格120秒三源，H264→HEVC 1280×720/30、8000kbps CBR/GOP60、AAC192kbps CBR/44100双声道，MPEG-TS/UDP至VLC。cdb捕获0xc0000409；调试运行不能替代无调试器验收。

## 执行命令
~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r305-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r305-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r305-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r305-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r305-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r305-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$debugger=Start-Process -FilePath 'C:/Program Files (x86)/Windows Kits/10/Debuggers/x64/cdb.exe' -ArgumentList ('-G -logo D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r305-cdb.log -y D:/Code/MyCode/MediaTranscode/out/build/x64-release -c "sxe -c2 \".exr -1;.ecxr;~* kb;.dump /ma D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r305-crash.dmp;qd\" 0xc0000409;g" D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe ' + '--media-id composition-r305 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62020 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2') -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r305-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r305-cli-error.log' -PassThru
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
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r305-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r305-native- --snapshot-format=png udp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r305-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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
源PID30592/10352/29532，cdb25528，CLI33272，VLC20948。VLC原生快照确认三格。观察超过120秒未捕获fatal/dump；结束诊断后清理CLI和VLC，清理不记正常停止。
微软说明cdb创建进程默认调试堆，-hd使用标准堆；本命令未使用-hd，因此下一轮按官方能力减少此差异，不据此推断根因：https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/debugging-a-user-mode-process-using-cdb 。
~~~~text
[CLI] runtime report: queued=1, peakQueued=1008, threads=81, processThreads=129, workers=75, workerProgress=336124, workerProcessCalls=1251078, workerWaits=914947, workerWakeups=904085, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=455, logicalProcessors=22, averageSystemMachineCpuPercent=10.536096, averageProcessMachineCpuPercent=1.830901, peakProcessMachineCpuPercent=5.135135, averageProcessSingleCoreCpuPercent=40.279818, peakProcessSingleCoreCpuPercent=112.972973, initialWorkingSetBytes=210608128, workingSetBytes=245342208, peakWorkingSetBytes=276049920, totalPushed=307459, totalPopped=307434, droppedBuffers=0, encodedPacketsPushed=47342, encodedPacketsPopped=47341, backpressureItems=123, graphPayloadCurrentBytes=132, graphPayloadHighWaterBytes=19799672, graphPayloadCurrentObjects=2, graphPayloadHighWaterObjects=841, graphPayloadReservations=144298, graphPayloadReleases=144296, graphPayloadPressureFailures=0, lastInputReceivedAtNs=6583707840200
[CLI] runtime report: queued=1, peakQueued=1008, threads=81, processThreads=129, workers=75, workerProgress=336344, workerProcessCalls=1251728, workerWaits=915377, workerWakeups=904492, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=456, logicalProcessors=22, averageSystemMachineCpuPercent=10.542092, averageProcessMachineCpuPercent=1.830523, peakProcessMachineCpuPercent=5.135135, averageProcessSingleCoreCpuPercent=40.271514, peakProcessSingleCoreCpuPercent=112.972973, initialWorkingSetBytes=210608128, workingSetBytes=245342208, peakWorkingSetBytes=276049920, totalPushed=307646, totalPopped=307621, droppedBuffers=0, encodedPacketsPushed=47443, encodedPacketsPopped=47442, backpressureItems=123, graphPayloadCurrentBytes=132, graphPayloadHighWaterBytes=19799672, graphPayloadCurrentObjects=2, graphPayloadHighWaterObjects=841, graphPayloadReservations=144364, graphPayloadReleases=144362, graphPayloadPressureFailures=0, lastInputReceivedAtNs=6583707840200
[CLI] runtime report: queued=1, peakQueued=1008, threads=81, processThreads=129, workers=75, workerProgress=336584, workerProcessCalls=1252428, workerWaits=915837, workerWakeups=904927, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=457, logicalProcessors=22, averageSystemMachineCpuPercent=10.549498, averageProcessMachineCpuPercent=1.828516, peakProcessMachineCpuPercent=5.135135, averageProcessSingleCoreCpuPercent=40.227356, peakProcessSingleCoreCpuPercent=112.972973, initialWorkingSetBytes=210608128, workingSetBytes=245342208, peakWorkingSetBytes=276049920, totalPushed=307850, totalPopped=307825, droppedBuffers=0, encodedPacketsPushed=47553, encodedPacketsPopped=47552, backpressureItems=123, graphPayloadCurrentBytes=132, graphPayloadHighWaterBytes=19799672, graphPayloadCurrentObjects=2, graphPayloadHighWaterObjects=841, graphPayloadReservations=144436, graphPayloadReleases=144434, graphPayloadPressureFailures=0, lastInputReceivedAtNs=6583707840200
[CLI] runtime report: queued=0, peakQueued=1008, threads=81, processThreads=129, workers=75, workerProgress=336819, workerProcessCalls=1253108, workerWaits=916282, workerWakeups=905349, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=458, logicalProcessors=22, averageSystemMachineCpuPercent=10.573148, averageProcessMachineCpuPercent=1.826955, peakProcessMachineCpuPercent=5.135135, averageProcessSingleCoreCpuPercent=40.193014, peakProcessSingleCoreCpuPercent=112.972973, initialWorkingSetBytes=210608128, workingSetBytes=245342208, peakWorkingSetBytes=276049920, totalPushed=308051, totalPopped=308027, droppedBuffers=0, encodedPacketsPushed=47662, encodedPacketsPopped=47662, backpressureItems=123, graphPayloadCurrentBytes=66, graphPayloadHighWaterBytes=19799672, graphPayloadCurrentObjects=1, graphPayloadHighWaterObjects=841, graphPayloadReservations=144505, graphPayloadReleases=144504, graphPayloadPressureFailures=0, lastInputReceivedAtNs=6583707840200
~~~~
## 清理清单
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r305-cdb.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r305-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r305-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r305-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r305-native-2026-10-23-10h35m40s470.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r305-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r305-source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r305-source1-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r305-source1.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r305-source2-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r305-source2.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r305-vlc.log

原会话确认：源退出码全部0，CLI=-1（结束诊断清理）；六PID无残留，清单12项已删除。原始120秒源保留。
