# r301：发送等待分阶段诊断复验

原规格三源H264→HEVC 1280×720/30，8000kbps CBR/GOP60，AAC192kbps CBR/44100双声道，MPEG-TS/UDP至VLC。已执行并崩溃失败。诊断只增加观察，不视为超期修复。

## 执行命令

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r301-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r301-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r301-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r301-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r301-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r301-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r301 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62020 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r301-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r301-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r301-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r301-native- --snapshot-format=png udp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r301-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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
Release全量重建exit0，两个CLI产物10:31:05，仅运行realtime。源PID32752/27544/32160，CLI22992，VLC21436。CLI异常退出-1073740791（0xC0000409），非自然正常退出；没有完成120秒验收。VLC原生快照已观察到左上、右上、左下三格画面及右下黑格。没有执行失源/重入，不证明发送时序修复。

### 系统异常
~~~~text
出错应用程序名称： media_transcode_realtime_video_cli.exe，版本： 0.0.0.0，时间戳： 0x6ad97569
出错模块名称： ucrtbase.dll， 版本： 10.0.26100.9711，时间戳： 0x3f426c5b
异常代码： 0xc0000409
错误偏移： 0x00000000000a527e
出错进程 ID： 0x59D0
出错应用程序开始时间： 0x1DD61CDA12A44E0
Faulting 应用程序路径： D:\Code\MyCode\MediaTranscode\out\build\x64-release\media_transcode_realtime_video_cli.exe
Faulting 模块路径： C:\WINDOWS\System32\ucrtbase.dll
Report ID： 2e75c05a-49ad-4bb5-88b9-86b11a9a14c9
Faulting 包全名： 
Faulting 程序包相对应用程序 ID：  个
~~~~
### 最后运行证据
~~~~text
[CLI] runtime report: queued=628, peakQueued=914, threads=81, processThreads=131, workers=75, workerProgress=33481, workerProcessCalls=124211, workerWaits=90722, workerWakeups=89859, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=33, logicalProcessors=22, averageSystemMachineCpuPercent=14.448068, averageProcessMachineCpuPercent=1.902815, peakProcessMachineCpuPercent=6.912442, averageProcessSingleCoreCpuPercent=41.861938, peakProcessSingleCoreCpuPercent=152.073733, initialWorkingSetBytes=208416768, workingSetBytes=262041600, peakWorkingSetBytes=262041600, totalPushed=31110, totalPopped=30482, droppedBuffers=0, encodedPacketsPushed=3457, encodedPacketsPopped=3457, backpressureItems=123, graphPayloadCurrentBytes=11816139, graphPayloadHighWaterBytes=17092487, graphPayloadCurrentObjects=732, graphPayloadHighWaterObjects=741, graphPayloadReservations=13819, graphPayloadReleases=13087, graphPayloadPressureFailures=0, lastInputReceivedAtNs=6816923024000
[CLI] runtime report: queued=650, peakQueued=914, threads=81, processThreads=131, workers=75, workerProgress=34204, workerProcessCalls=127352, workerWaits=93141, workerWakeups=92254, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=34, logicalProcessors=22, averageSystemMachineCpuPercent=14.560902, averageProcessMachineCpuPercent=1.933160, peakProcessMachineCpuPercent=6.912442, averageProcessSingleCoreCpuPercent=42.529523, peakProcessSingleCoreCpuPercent=152.073733, initialWorkingSetBytes=208416768, workingSetBytes=262041600, peakWorkingSetBytes=262041600, totalPushed=31768, totalPopped=31118, droppedBuffers=0, encodedPacketsPushed=3559, encodedPacketsPopped=3558, backpressureItems=123, graphPayloadCurrentBytes=12695081, graphPayloadHighWaterBytes=17092487, graphPayloadCurrentObjects=733, graphPayloadHighWaterObjects=741, graphPayloadReservations=14082, graphPayloadReleases=13349, graphPayloadPressureFailures=0, lastInputReceivedAtNs=6817246852200
[2026-10-22 10:33:05.572] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=490 source_master_ns=17027395705 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=544152
[CLI] runtime report: queued=662, peakQueued=914, threads=81, processThreads=131, workers=75, workerProgress=35010, workerProcessCalls=130104, workerWaits=95087, workerWakeups=94176, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=35, logicalProcessors=22, averageSystemMachineCpuPercent=14.627681, averageProcessMachineCpuPercent=1.924223, peakProcessMachineCpuPercent=6.912442, averageProcessSingleCoreCpuPercent=42.332912, peakProcessSingleCoreCpuPercent=152.073733, initialWorkingSetBytes=208416768, workingSetBytes=262041600, peakWorkingSetBytes=262041600, totalPushed=32509, totalPopped=31847, droppedBuffers=0, encodedPacketsPushed=3668, encodedPacketsPopped=3668, backpressureItems=123, graphPayloadCurrentBytes=12685589, graphPayloadHighWaterBytes=17092487, graphPayloadCurrentObjects=734, graphPayloadHighWaterObjects=741, graphPayloadReservations=14403, graphPayloadReleases=13669, graphPayloadPressureFailures=0, lastInputReceivedAtNs=6817555177900
[CLI] runtime report: queued=648, peakQueued=914, threads=81, processThreads=131, workers=75, workerProgress=35907, workerProcessCalls=135095, workerWaits=99181, workerWakeups=98248, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=36, logicalProcessors=22, averageSystemMachineCpuPercent=14.549245, averageProcessMachineCpuPercent=1.901706, peakProcessMachineCpuPercent=6.912442, averageProcessSingleCoreCpuPercent=41.837522, peakProcessSingleCoreCpuPercent=152.073733, initialWorkingSetBytes=208416768, workingSetBytes=264093696, peakWorkingSetBytes=264093696, totalPushed=33343, totalPopped=32695, droppedBuffers=0, encodedPacketsPushed=3769, encodedPacketsPopped=3769, backpressureItems=123, graphPayloadCurrentBytes=12318907, graphPayloadHighWaterBytes=17092487, graphPayloadCurrentObjects=736, graphPayloadHighWaterObjects=741, graphPayloadReservations=14839, graphPayloadReleases=14103, graphPayloadPressureFailures=0, lastInputReceivedAtNs=6817861942800
[CLI] runtime report: queued=668, peakQueued=914, threads=81, processThreads=131, workers=75, workerProgress=36628, workerProcessCalls=138760, workerWaits=102125, workerWakeups=101168, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=37, logicalProcessors=22, averageSystemMachineCpuPercent=14.568750, averageProcessMachineCpuPercent=1.963475, peakProcessMachineCpuPercent=6.912442, averageProcessSingleCoreCpuPercent=43.196460, peakProcessSingleCoreCpuPercent=152.073733, initialWorkingSetBytes=208416768, workingSetBytes=264093696, peakWorkingSetBytes=264093696, totalPushed=33999, totalPopped=33331, droppedBuffers=0, encodedPacketsPushed=3871, encodedPacketsPopped=3870, backpressureItems=123, graphPayloadCurrentBytes=13599129, graphPayloadHighWaterBytes=17092487, graphPayloadCurrentObjects=733, graphPayloadHighWaterObjects=741, graphPayloadReservations=15108, graphPayloadReleases=14375, graphPayloadPressureFailures=0, lastInputReceivedAtNs=6818168350800
[2026-10-22 10:33:06.574] [info] [graph][runtime.node] av_scheduler_trace stage=late_video_head generation=1 mapping_epoch_generation=1 canonical_target_ns=11700000000 canonical_dispatch_ns=11700000000 target_master_ns=17372427668 dispatch_master_ns=17372427668 now_ns=17398978300 node=74 identity_kind=output identity="composition-r301.output" canonical_sequence=352 canonical_created_steady_ns=6818491317800 head_observed_steady_ns=6818491358800 codec_receive_begin_steady_ns=6818491309100 codec_receive_end_steady_ns=6818491310900
~~~~
### 本轮清理清单
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r301-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r301-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r301-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r301-native-2026-10-22-10h33m14s493.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r301-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r301-source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r301-source1-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r301-source1.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r301-source2-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r301-source2.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r301-vlc.log

清理结果：诊断记录后按精确PID停止三个源和VLC；源退出码均-1，仅属失败后清理。五个PID复查无残留。按上述清单删除11个临时文件，保留原始120秒源。本轮未运行远程测试。

## WER 补充证据（2026-10-22）

只读检查系统 ReportArchive，匹配 IntegratorReportIdentifier=2e75c05a-49ad-4bb5-88b9-86b11a9a14c9；ReportIdentifier=fe93ed5a-daef-4230-bf1f-8b1e66cc5a32。异常代码c0000409，异常数据0000000000000007，故障模块ucrtbase.dll、偏移a527e。目录仅有Report.wer，无转储；未修改或删除系统报告。

本机Windows SDK 10.0.26100.0/um/winnt.h:23048定义FAST_FAIL_FATAL_APP_EXIT=7。微软说明该类可由abort/terminate触发，不能仅按0xc0000409名称认定栈溢出，也不能由子码确定具体调用者：https://devblogs.microsoft.com/oldnewthing/20190108-00/?p=100655 。下一次现场捕获须保留异常子码及abort/terminate上方调用栈，不能以调试器下未复现关闭问题。
