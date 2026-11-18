# r327：原规格 TS/RTP 致命异常定位

三路连续120秒 H264/AAC RTP 输入，HEVC CBR 8000kbps 1280×720/30 与 AAC CBR 192kbps 44100Hz stereo，MPEG-TS/RTP 输出。只运行 realtime。使用现有 Release 与匹配 PDB，CDB 从启动捕获 0xc0000409，标准堆，保持原期限。诊断运行不替代正式验收；未修改注册表或外部依赖。

## 实际启动命令

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r327-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r327-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r327-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r327-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r327-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r327-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$debugger=Start-Process -FilePath 'C:/Program Files (x86)/Windows Kits/10/Debuggers/x64/cdb.exe' -ArgumentList ('-G -hd -logo D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r327-cdb.log -y D:/Code/MyCode/MediaTranscode/out/build/x64-release -c "sxe -c2 \".exr -1;.ecxr;~* kb;.dump /ma D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r327-crash.dmp;qd\" 0xc0000409;g" D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe ' + '--media-id composition-r327 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62024 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r327-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2') -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r327-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r327-cli-error.log' -PassThru
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
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r327-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r327-native- --snapshot-format=png rtp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r327-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

## 诊断结果：未复现，不是完整验收通过

源 PID 30852、30624、6436 均已完成 3600 帧/120 秒，CLI 4164 在源结束后仍运行。CDB 33144、VLC 5444。两张 VLC 原生快照确认三格视频及全黑；未进行重入。本轮未捕获 0xc0000409，无 dump，不能证明 r326 已修复。结束诊断后按精确 PID 清理，不记自然退出或正常释放通过。

最后运行证据：
~~~~text
[CLI] runtime report: queued=0, peakQueued=957, threads=82, processThreads=130, workers=76, workerProgress=585411, workerProcessCalls=1960442, workerWaits=1375023, workerWakeups=1361202, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=613, logicalProcessors=22, averageSystemMachineCpuPercent=12.304467, averageProcessMachineCpuPercent=1.649820, peakProcessMachineCpuPercent=5.365854, averageProcessSingleCoreCpuPercent=36.296040, peakProcessSingleCoreCpuPercent=118.048780, initialWorkingSetBytes=207388672, workingSetBytes=250159104, peakWorkingSetBytes=275378176, totalPushed=470064, totalPopped=470040, droppedBuffers=0, encodedPacketsPushed=60797, encodedPacketsPopped=60797, backpressureItems=124, graphPayloadCurrentBytes=66, graphPayloadHighWaterBytes=18281511, graphPayloadCurrentObjects=2, graphPayloadHighWaterObjects=793, graphPayloadReservations=153613, graphPayloadReleases=153611, graphPayloadPressureFailures=0, lastInputReceivedAtNs=902496637200
[CLI] runtime report: queued=1, peakQueued=957, threads=82, processThreads=130, workers=76, workerProgress=585648, workerProcessCalls=1961111, workerWaits=1375456, workerWakeups=1361612, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=614, logicalProcessors=22, averageSystemMachineCpuPercent=12.290942, averageProcessMachineCpuPercent=1.649666, peakProcessMachineCpuPercent=5.365854, averageProcessSingleCoreCpuPercent=36.292662, peakProcessSingleCoreCpuPercent=118.048780, initialWorkingSetBytes=207388672, workingSetBytes=250159104, peakWorkingSetBytes=275378176, totalPushed=470248, totalPopped=470223, droppedBuffers=0, encodedPacketsPushed=60898, encodedPacketsPopped=60897, backpressureItems=124, graphPayloadCurrentBytes=72, graphPayloadHighWaterBytes=18281511, graphPayloadCurrentObjects=2, graphPayloadHighWaterObjects=793, graphPayloadReservations=153678, graphPayloadReleases=153676, graphPayloadPressureFailures=0, lastInputReceivedAtNs=902496637200
[CLI] runtime report: queued=1, peakQueued=957, threads=82, processThreads=130, workers=76, workerProgress=585878, workerProcessCalls=1961759, workerWaits=1375875, workerWakeups=1362009, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=615, logicalProcessors=22, averageSystemMachineCpuPercent=12.278942, averageProcessMachineCpuPercent=1.647710, peakProcessMachineCpuPercent=5.365854, averageProcessSingleCoreCpuPercent=36.249619, peakProcessSingleCoreCpuPercent=118.048780, initialWorkingSetBytes=207388672, workingSetBytes=250159104, peakWorkingSetBytes=275378176, totalPushed=470424, totalPopped=470399, droppedBuffers=0, encodedPacketsPushed=60991, encodedPacketsPopped=60990, backpressureItems=124, graphPayloadCurrentBytes=132, graphPayloadHighWaterBytes=18281511, graphPayloadCurrentObjects=2, graphPayloadHighWaterObjects=793, graphPayloadReservations=153741, graphPayloadReleases=153739, graphPayloadPressureFailures=0, lastInputReceivedAtNs=902496637200
seconds=187.9592268 cpu=0.894800271460694 ws=250159104 private=1136431104
~~~~

## 清理清单

D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r327-cdb.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r327-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r327-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r327-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r327-native-2026-10-26-08h59m15s777.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r327-native-2026-10-26-09h00m59s111.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r327-output.sdp
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r327-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r327-source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r327-source1-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r327-source1.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r327-source2-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r327-source2.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r327-vlc.log

监控会话返回 CLI_EXIT=-1（诊断清理），三个 SOURCE_EXIT 均为0。六 PID 无残留，按上述清单删除14项临时产物；指定120秒源保留。未触及系统 WER 配置或远程目标。
