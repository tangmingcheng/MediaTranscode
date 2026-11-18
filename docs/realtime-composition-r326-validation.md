# r326：TS/RTP范围绑定画面复验

三路120秒H264/AAC RTP→HEVC CBR 8000kbps 1280×720/30、AAC CBR 192kbps 44100Hz stereo→MPEG-TS/RTP62024、RTCP62025。VLC从启动使用RTP URI；仅realtime及原生快照。执行失败：CLI 在源流结束前发生 0xC0000409 崩溃，不计整体合屏门禁通过。

## 启动命令

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r326-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r326-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r326-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r326-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r326-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r326-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r326 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62024 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r326-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r326-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r326-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r326-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r326-native- --snapshot-format=png rtp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r326-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

## 结果与边界

- 源 PID 28080、42488、42452：原监控会话记录均自然运行 120 秒并退出 0；CLI PID 42212 退出 -1073740791（0xC0000409）。VLC PID 37364 已通过 RC quit 关闭。本轮复核五 PID 均无残留。
- Windows 事件时间 2026-10-23 16:31:10；崩溃发生在约第 99–103 秒，早于源自然结束。故障模块 ucrtbase.dll 10.0.26100.9711，偏移 0xa527e，WER 参数 7。模块名与异常码不足以定位根因，不宣称栈缓冲区溢出或外部库缺陷。
- 原生 VLC 快照已观察到三格视频与右下黑格；缺少失源全黑及重入验证。未得到正常收尾、最终发送计数和资源释放证据。
- 最后运行报告 errors=0，平均进程机器 CPU 2.696712%，峰值 5.487805%；工作集 254287872，峰值 254361600 字节。payload 当前 406801、高水位 4542457 字节，reservation 118012、release 117820，不能据此声明释放完整。
- 输入时钟 phase 22676ns 不是内容 A/V 漂移证据。
- WER 归档仅有 Report.wer，事件引用的临时 mdmp 已不存在；仓库 acceptance 无转储。未创建 HKLM LocalDumps 配置。现有 CDB 可用于后续诊断，需区分调试暂停与产品实时期限失败。

## 最后日志

~~~~text
[2026-10-23 16:31:09.483] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4209 source_master_ns=98373819783 raw_phase_ns=22676 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4353216
[CLI] runtime report: queued=109, peakQueued=390, threads=82, processThreads=130, workers=76, workerProgress=447704, workerProcessCalls=1520468, workerWaits=1072756, workerWakeups=1065294, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=302, logicalProcessors=22, averageSystemMachineCpuPercent=12.829779, averageProcessMachineCpuPercent=2.693885, peakProcessMachineCpuPercent=5.487805, averageProcessSingleCoreCpuPercent=59.265477, peakProcessSingleCoreCpuPercent=120.731707, initialWorkingSetBytes=192868352, workingSetBytes=254287872, peakWorkingSetBytes=254361600, totalPushed=361953, totalPopped=361844, droppedBuffers=0, encodedPacketsPushed=32601, encodedPacketsPopped=32600, backpressureItems=124, graphPayloadCurrentBytes=409992, graphPayloadHighWaterBytes=4542457, graphPayloadCurrentObjects=196, graphPayloadHighWaterObjects=201, graphPayloadReservations=117261, graphPayloadReleases=117065, graphPayloadPressureFailures=0, lastInputReceivedAtNs=27828423308100
[CLI] runtime report: queued=123, peakQueued=390, threads=82, processThreads=130, workers=76, workerProgress=449081, workerProcessCalls=1525389, workerWaits=1076300, workerWakeups=1068814, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=303, logicalProcessors=22, averageSystemMachineCpuPercent=12.836638, averageProcessMachineCpuPercent=2.692564, peakProcessMachineCpuPercent=5.487805, averageProcessSingleCoreCpuPercent=59.236412, peakProcessSingleCoreCpuPercent=120.731707, initialWorkingSetBytes=192868352, workingSetBytes=254287872, peakWorkingSetBytes=254361600, totalPushed=363069, totalPopped=362946, droppedBuffers=0, encodedPacketsPushed=32711, encodedPacketsPopped=32708, backpressureItems=124, graphPayloadCurrentBytes=442301, graphPayloadHighWaterBytes=4542457, graphPayloadCurrentObjects=197, graphPayloadHighWaterObjects=201, graphPayloadReservations=117603, graphPayloadReleases=117406, graphPayloadPressureFailures=0, lastInputReceivedAtNs=27828729476500
[CLI] runtime report: queued=106, peakQueued=390, threads=82, processThreads=130, workers=76, workerProgress=450539, workerProcessCalls=1530275, workerWaits=1079729, workerWakeups=1072219, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=304, logicalProcessors=22, averageSystemMachineCpuPercent=12.872442, averageProcessMachineCpuPercent=2.696712, peakProcessMachineCpuPercent=5.487805, averageProcessSingleCoreCpuPercent=59.327663, peakProcessSingleCoreCpuPercent=120.731707, initialWorkingSetBytes=192868352, workingSetBytes=254287872, peakWorkingSetBytes=254361600, totalPushed=364247, totalPopped=364141, droppedBuffers=0, encodedPacketsPushed=32812, encodedPacketsPopped=32811, backpressureItems=124, graphPayloadCurrentBytes=406801, graphPayloadHighWaterBytes=4542457, graphPayloadCurrentObjects=192, graphPayloadHighWaterObjects=201, graphPayloadReservations=118012, graphPayloadReleases=117820, graphPayloadPressureFailures=0, lastInputReceivedAtNs=27829023319900
[2026-10-23 16:31:10.204] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=98441722100 present_generation=1 present_canonical_target_ns=97918548752 present_canonical_dispatch_ns=97918548752 present_target_master_ns=98561012320 present_dispatch_master_ns=98561012320
[2026-10-23 16:31:10.382] [info] [graph][runtime.node] av_scheduler_trace stage=late_video_head generation=1 mapping_epoch_generation=1 canonical_target_ns=97966666666 canonical_dispatch_ns=97966666666 target_master_ns=98609130234 dispatch_master_ns=98609130234 now_ns=98620164300 node=74 identity_kind=output identity="composition-r326.output" canonical_sequence=2940 canonical_created_steady_ns=27829300002300 head_observed_steady_ns=27829300050500 codec_receive_begin_steady_ns=27829299989800 codec_receive_end_steady_ns=27829299991700
~~~~

## Windows 事件

~~~~text
出错应用程序名称： media_transcode_realtime_video_cli.exe，版本： 0.0.0.0，时间戳： 0x6adb194c
出错模块名称： ucrtbase.dll， 版本： 10.0.26100.9711，时间戳： 0x3f426c5b
异常代码： 0xc0000409
错误偏移： 0x00000000000a527e
出错进程 ID： 0xA4E4
出错应用程序开始时间： 0x1DD62C8A080E8B8
Faulting 应用程序路径： D:\Code\MyCode\MediaTranscode\out\build\x64-release\media_transcode_realtime_video_cli.exe
Faulting 模块路径： C:\WINDOWS\System32\ucrtbase.dll
Report ID： 118efea0-dfc4-495e-9cd4-6ff638578f6f
Faulting 包全名： 
Faulting 程序包相对应用程序 ID：  个
~~~~

## 本轮临时产物清单

D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r326-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r326-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r326-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r326-native-2026-10-23-16h30m08s683.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r326-output.sdp
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r326-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r326-source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r326-source1-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r326-source1.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r326-source2-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r326-source2.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r326-vlc.log

清理结果：以上 12 项已逐项删除，本轮文件与五个进程均无残留。指定 120 秒源保留；未进行远程测试。

## 后续定位依据

Microsoft 的 [__fastfail 文档](https://learn.microsoft.com/en-us/cpp/intrinsics/fastfail?view=msvc-170) 明确用户态 fast fail 表现为第二次机会、不可继续的 0xC0000409，首参数表示失败类别。因此必须取得异常线程调用栈，不能仅凭错误名称认定越界。

当前源码存在明确的主动终止候选：MediaGraphPayloadCreditLease::release 的字节/对象下溢保护、MediaSharedAllocationAllocator::deallocate 的资源及几何校验、MediaRtpProtocolStorageRegistry::Entry 移动已占用条目的保护。它们尚未被运行证据证明触发。初查 granted waiter 转移到 lease 与移除 waiter 在同一 account mutex 下完成，不能将取消与释放并发直接认定为重复扣账。

本机 Release exe 与 PDB 同为 2026-10-23 16:22:36，均存在。下一轮优先从进程启动时配置 CDB 的致命异常捕获，避免 r307 的运行中附加暂停；保留原规格与原期限。调试运行不能替代无调试器验收。未配置 WER 注册表，不要求用户执行管理员操作。
独立只读审查 crash_release_audit：上述三项尚无已证明缺陷。MSVC 14.51 memory:2540–2543 的 _Delete_this 先复制 rebind allocator，再析构控制块，再 deallocate，resource 所有权延寿成立；RTP reservations 先于 storagePermit 析构，permit 强持有 registry。建议捕获 terminate/abort 汇合栈，不在高频释放入口停顿。此结论不等于运行验证通过。
