# r302：Release 崩溃现场诊断

沿用r301原规格三源及全部输出参数，cdb捕获0xc0000409异常。调试器影响时序，本轮用于诊断，不能替代无调试器验收。已执行。

## 执行命令

~~~~powershell

$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r302-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r302-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r302-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r302-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r302-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r302-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$debugger=Start-Process -FilePath 'C:/Program Files (x86)/Windows Kits/10/Debuggers/x64/cdb.exe' -ArgumentList ('-G -logo D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r302-cdb.log -y D:/Code/MyCode/MediaTranscode/out/build/x64-release -c "sxe -c2 \".exr -1;.ecxr;~* kb;.dump /ma D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r302-crash.dmp;qd\" 0xc0000409;g" D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe ' + '--media-id composition-r302 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62020 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2') -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r302-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r302-cli-error.log' -PassThru
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
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r302-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r302-native- --snapshot-format=png udp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r302-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

## 诊断结果：未复现，非验收通过
源PID5956/30244/18456，cdb3276，CLI13824，VLC22588。三源已自然结束，源0日志3600帧/120秒；源结束后CLI仍运行，VLC原生快照三格转全黑。未捕获fatal、没有dump；不能证明r301崩溃修复。停止本轮诊断后按精确身份清理CLI和VLC，不记自然退出或caller_stop通过。

新增诊断路径独立审查未发现明确崩溃原因；既有noexcept错误字符串复制可能在bad_alloc时terminate，但本轮没有内存耗尽或对应栈证据，不据此修改。

### 最后指标
~~~~text
[CLI] runtime report: queued=1, peakQueued=1038, threads=81, processThreads=129, workers=75, workerProgress=341155, workerProcessCalls=1348057, workerWaits=1006895, workerWakeups=995401, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=472, logicalProcessors=22, averageSystemMachineCpuPercent=13.648467, averageProcessMachineCpuPercent=1.975073, peakProcessMachineCpuPercent=4.575163, averageProcessSingleCoreCpuPercent=43.451612, peakProcessSingleCoreCpuPercent=100.653595, initialWorkingSetBytes=209223680, workingSetBytes=243503104, peakWorkingSetBytes=275156992, totalPushed=311344, totalPopped=311319, droppedBuffers=0, encodedPacketsPushed=50183, encodedPacketsPopped=50182, backpressureItems=123, graphPayloadCurrentBytes=132, graphPayloadHighWaterBytes=20556511, graphPayloadCurrentObjects=2, graphPayloadHighWaterObjects=866, graphPayloadReservations=144854, graphPayloadReleases=144852, graphPayloadPressureFailures=0, lastInputReceivedAtNs=7065876356000
[CLI] runtime report: queued=1, peakQueued=1038, threads=81, processThreads=129, workers=75, workerProgress=341402, workerProcessCalls=1348774, workerWaits=1007365, workerWakeups=995846, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=473, logicalProcessors=22, averageSystemMachineCpuPercent=13.650563, averageProcessMachineCpuPercent=1.973755, peakProcessMachineCpuPercent=4.575163, averageProcessSingleCoreCpuPercent=43.422601, peakProcessSingleCoreCpuPercent=100.653595, initialWorkingSetBytes=209223680, workingSetBytes=243503104, peakWorkingSetBytes=275156992, totalPushed=311552, totalPopped=311527, droppedBuffers=0, encodedPacketsPushed=50293, encodedPacketsPopped=50292, backpressureItems=123, graphPayloadCurrentBytes=340, graphPayloadHighWaterBytes=20556511, graphPayloadCurrentObjects=2, graphPayloadHighWaterObjects=866, graphPayloadReservations=144926, graphPayloadReleases=144924, graphPayloadPressureFailures=0, lastInputReceivedAtNs=7065876356000
[CLI] runtime report: queued=1, peakQueued=1038, threads=81, processThreads=129, workers=75, workerProgress=341622, workerProcessCalls=1349410, workerWaits=1007781, workerWakeups=996239, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=474, logicalProcessors=22, averageSystemMachineCpuPercent=13.646393, averageProcessMachineCpuPercent=1.971914, peakProcessMachineCpuPercent=4.575163, averageProcessSingleCoreCpuPercent=43.382109, peakProcessSingleCoreCpuPercent=100.653595, initialWorkingSetBytes=209223680, workingSetBytes=243503104, peakWorkingSetBytes=275156992, totalPushed=311739, totalPopped=311714, droppedBuffers=0, encodedPacketsPushed=50394, encodedPacketsPopped=50393, backpressureItems=123, graphPayloadCurrentBytes=132, graphPayloadHighWaterBytes=20556511, graphPayloadCurrentObjects=2, graphPayloadHighWaterObjects=866, graphPayloadReservations=144992, graphPayloadReleases=144990, graphPayloadPressureFailures=0, lastInputReceivedAtNs=7065876356000
[CLI] runtime report: queued=0, peakQueued=1038, threads=81, processThreads=129, workers=75, workerProgress=341861, workerProcessCalls=1350105, workerWaits=1008236, workerWakeups=996670, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=475, logicalProcessors=22, averageSystemMachineCpuPercent=13.654506, averageProcessMachineCpuPercent=1.971112, peakProcessMachineCpuPercent=4.575163, averageProcessSingleCoreCpuPercent=43.364462, peakProcessSingleCoreCpuPercent=100.653595, initialWorkingSetBytes=209223680, workingSetBytes=243503104, peakWorkingSetBytes=275156992, totalPushed=311942, totalPopped=311918, droppedBuffers=0, encodedPacketsPushed=50503, encodedPacketsPopped=50503, backpressureItems=123, graphPayloadCurrentBytes=66, graphPayloadHighWaterBytes=20556511, graphPayloadCurrentObjects=2, graphPayloadHighWaterObjects=866, graphPayloadReservations=145062, graphPayloadReleases=145060, graphPayloadPressureFailures=0, lastInputReceivedAtNs=7065876356000
~~~~
### 清理清单
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r302-cdb.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r302-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r302-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r302-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r302-native-2026-10-22-10h36m49s911.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r302-native-2026-10-22-10h37m28s284.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r302-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r302-source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r302-source1-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r302-source1.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r302-source2-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r302-source2.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r302-vlc.log

原工具句柄确认：SOURCE0/1/2_EXIT均0；CLI_EXIT=-1（诊断结束强制清理）。六个PID无残留，按清单删除13个本轮临时文件，原120秒源保留。未运行远程测试。
