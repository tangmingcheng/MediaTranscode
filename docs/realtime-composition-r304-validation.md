# r304：TS prefix 预分配闭合复验（已执行，失败）

原规格三源H264→HEVC 1280×720/30、8000kbps CBR/GOP60，AAC192kbps CBR/44100双声道，MPEG-TS/UDP至VLC。须在prefix源码双审和Release全量重建完成后执行。本轮不代表完整TS预算或RTP路径已验收。

## 预定执行命令

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r304-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r304-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r304-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r304-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r304-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r304-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r304 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62020 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r304-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r304-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r304-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r304-native- --snapshot-format=png udp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r304-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

## 实际结果：FAIL
Release全量736项configure/build均0，2026-10-23 10:31:28产物，prefix双独立静态审查PASS。源PID30164/33580/7524，CLI12760，VLC5256。CLI在约70秒以-1073740791（0xc0000409）退出，未完成120秒。VLC原生快照三格画面正常；未执行失源/重入。不能记本切片媒体验收通过。
## 系统异常
~~~~text
出错应用程序名称： media_transcode_realtime_video_cli.exe，版本： 0.0.0.0，时间戳： 0x6adac700
出错模块名称： ucrtbase.dll， 版本： 10.0.26100.9711，时间戳： 0x3f426c5b
异常代码： 0xc0000409
错误偏移： 0x00000000000a527e
出错进程 ID： 0x31D8
出错应用程序开始时间： 0x1DD6296B193300B
Faulting 应用程序路径： D:\Code\MyCode\MediaTranscode\out\build\x64-release\media_transcode_realtime_video_cli.exe
Faulting 模块路径： C:\WINDOWS\System32\ucrtbase.dll
Report ID： 131a703f-a82f-417d-84ed-5bf6b07f0262
Faulting 包全名： 
Faulting 程序包相对应用程序 ID：  个
~~~~
## 最后运行证据
~~~~text
[2026-10-23 10:33:09.101] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2483 source_master_ns=62848673817 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2584722
[CLI] runtime report: queued=635, peakQueued=940, threads=81, processThreads=130, workers=75, workerProgress=160684, workerProcessCalls=629701, workerWaits=469010, workerWakeups=464565, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=184, logicalProcessors=22, averageSystemMachineCpuPercent=9.158616, averageProcessMachineCpuPercent=2.002012, peakProcessMachineCpuPercent=6.728538, averageProcessSingleCoreCpuPercent=44.044271, peakProcessSingleCoreCpuPercent=148.027842, initialWorkingSetBytes=209285120, workingSetBytes=269189120, peakWorkingSetBytes=269225984, totalPushed=148173, totalPopped=147538, droppedBuffers=0, encodedPacketsPushed=19195, encodedPacketsPopped=19194, backpressureItems=123, graphPayloadCurrentBytes=10902806, graphPayloadHighWaterBytes=16703378, graphPayloadCurrentObjects=729, graphPayloadHighWaterObjects=740, graphPayloadReservations=70452, graphPayloadReleases=69723, graphPayloadPressureFailures=0, lastInputReceivedAtNs=6348120541300
[CLI] runtime report: queued=658, peakQueued=940, threads=81, processThreads=130, workers=75, workerProgress=161387, workerProcessCalls=632589, workerWaits=471196, workerWakeups=466726, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=185, logicalProcessors=22, averageSystemMachineCpuPercent=9.161106, averageProcessMachineCpuPercent=1.994440, peakProcessMachineCpuPercent=6.728538, averageProcessSingleCoreCpuPercent=43.877688, peakProcessSingleCoreCpuPercent=148.027842, initialWorkingSetBytes=209285120, workingSetBytes=269189120, peakWorkingSetBytes=269225984, totalPushed=148812, totalPopped=148154, droppedBuffers=0, encodedPacketsPushed=19296, encodedPacketsPopped=19295, backpressureItems=123, graphPayloadCurrentBytes=11803237, graphPayloadHighWaterBytes=16703378, graphPayloadCurrentObjects=730, graphPayloadHighWaterObjects=740, graphPayloadReservations=70693, graphPayloadReleases=69963, graphPayloadPressureFailures=0, lastInputReceivedAtNs=6348422955700
[CLI] runtime report: queued=626, peakQueued=940, threads=81, processThreads=130, workers=75, workerProgress=162418, workerProcessCalls=636832, workerWaits=474407, workerWakeups=469913, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=186, logicalProcessors=22, averageSystemMachineCpuPercent=9.185166, averageProcessMachineCpuPercent=2.010728, peakProcessMachineCpuPercent=6.728538, averageProcessSingleCoreCpuPercent=44.236014, peakProcessSingleCoreCpuPercent=148.027842, initialWorkingSetBytes=209285120, workingSetBytes=269189120, peakWorkingSetBytes=269225984, totalPushed=149771, totalPopped=149145, droppedBuffers=0, encodedPacketsPushed=19406, encodedPacketsPopped=19405, backpressureItems=123, graphPayloadCurrentBytes=11484837, graphPayloadHighWaterBytes=16703378, graphPayloadCurrentObjects=728, graphPayloadHighWaterObjects=740, graphPayloadReservations=71239, graphPayloadReleases=70511, graphPayloadPressureFailures=0, lastInputReceivedAtNs=6348742602900
[2026-10-23 10:33:10.123] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2527 source_master_ns=63870351822 raw_phase_ns=0 filtered_phase_ns=1 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2630068
[CLI] runtime report: queued=652, peakQueued=940, threads=81, processThreads=130, workers=75, workerProgress=163115, workerProcessCalls=639905, workerWaits=476783, workerWakeups=472266, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=187, logicalProcessors=22, averageSystemMachineCpuPercent=9.175003, averageProcessMachineCpuPercent=2.007058, peakProcessMachineCpuPercent=6.728538, averageProcessSingleCoreCpuPercent=44.155282, peakProcessSingleCoreCpuPercent=148.027842, initialWorkingSetBytes=209285120, workingSetBytes=269189120, peakWorkingSetBytes=269225984, totalPushed=150408, totalPopped=149756, droppedBuffers=0, encodedPacketsPushed=19507, encodedPacketsPopped=19506, backpressureItems=123, graphPayloadCurrentBytes=12305471, graphPayloadHighWaterBytes=16703378, graphPayloadCurrentObjects=729, graphPayloadHighWaterObjects=740, graphPayloadReservations=71480, graphPayloadReleases=70751, graphPayloadPressureFailures=0, lastInputReceivedAtNs=6349053877700
[CLI] runtime report: queued=631, peakQueued=940, threads=81, processThreads=130, workers=75, workerProgress=164070, workerProcessCalls=643044, workerWaits=478965, workerWakeups=474425, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=188, logicalProcessors=22, averageSystemMachineCpuPercent=9.162727, averageProcessMachineCpuPercent=2.005514, peakProcessMachineCpuPercent=6.728538, averageProcessSingleCoreCpuPercent=44.121308, peakProcessSingleCoreCpuPercent=148.027842, initialWorkingSetBytes=209285120, workingSetBytes=269189120, peakWorkingSetBytes=269225984, totalPushed=151295, totalPopped=150664, droppedBuffers=0, encodedPacketsPushed=19616, encodedPacketsPopped=19616, backpressureItems=123, graphPayloadCurrentBytes=11661099, graphPayloadHighWaterBytes=16703378, graphPayloadCurrentObjects=731, graphPayloadHighWaterObjects=740, graphPayloadReservations=71947, graphPayloadReleases=71216, graphPayloadPressureFailures=0, lastInputReceivedAtNs=6349379000400
[CLI] runtime report: queued=650, peakQueued=944, threads=81, processThreads=130, workers=75, workerProgress=164775, workerProcessCalls=645736, workerWaits=480948, workerWakeups=476390, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=189, logicalProcessors=22, averageSystemMachineCpuPercent=9.191865, averageProcessMachineCpuPercent=2.009133, peakProcessMachineCpuPercent=6.728538, averageProcessSingleCoreCpuPercent=44.200924, peakProcessSingleCoreCpuPercent=148.027842, initialWorkingSetBytes=209285120, workingSetBytes=269189120, peakWorkingSetBytes=269225984, totalPushed=151940, totalPopped=151290, droppedBuffers=0, encodedPacketsPushed=19694, encodedPacketsPopped=19689, backpressureItems=123, graphPayloadCurrentBytes=11704118, graphPayloadHighWaterBytes=16703378, graphPayloadCurrentObjects=743, graphPayloadHighWaterObjects=747, graphPayloadReservations=72228, graphPayloadReleases=71485, graphPayloadPressureFailures=1, lastInputReceivedAtNs=6349678960800
~~~~
## 本轮清理清单
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r304-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r304-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r304-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r304-native-2026-10-23-10h32m40s059.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r304-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r304-source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r304-source1-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r304-source1.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r304-source2-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r304-source2.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r304-vlc.log

清理：记录诊断后按本轮身份停止源和VLC，三个源退出码均-1，非自然结束。五个PID无残留，按清单删除11项临时产物，保留120秒源。未运行远程测试。
