# r371 Windows Release realtime 三源回归

2026-11-02：FAIL。三路 H.264 RTP 输入，所选 AAC 音源，输出 HEVC CBR 8000 kbps、1280×720/30fps、AAC CBR 192 kbps，MPEG-TS/UDP。音频前缀修复后已进入真实输出，但发送期限失败，未完成失源和重入验收。

初始源 PID 30768/21400/30524，CLI 34888，VLC 34696。三个源日志均记录3600帧/120秒；进程均已不存在，原监控句柄67867已失效，无法追溯进程退出码，不记exit0。CLI日志明确runtime failed。

VLC原生快照 composition-r371-native-2026-11-02-09h23m36s234.png 已观察三格游戏画面和第四格黑屏。没有全失源或重入快照。

错误发生于generation=1、sequence=74119：deadline_ns=104614731676，observed_at_ns=104615105400，超出373724 ns；reserved_at_ns=104614393900，not_before_ns=104614605940。此证据不足以认定根因，r366同类期限问题仍未关闭。

最终报告：errors=2、workerErrors=2、stalledIntervals=0；平均进程整机CPU 2.044187%，峰值6.679389%；初始工作集189550592 bytes、峰值252551168 bytes。媒体预约/释放97354次匹配，当前对象与bytes均0，高水位5124653 bytes/208对象，压力失败0。内部漂移不能证明内容级A/V同步。

## 诊断证据缺口与下一轮取证

本轮归档遗漏了已有 datagram_reservation_trace 和 datagram_batch_trace；原日志已清理，无法恢复等待 API、唤醒后调度和提交前延迟的分段证据。此遗漏不能由最终错误或r366历史数据补足。源码在 MediaScheduledDatagramSenderNode.cpp 的 TrySubmit 分支、socket提交前检查并失败，因此只能称提交前超期，不是已证实的网络提交耗时过长。

下一轮保持相同源参数及完整链路，归档所有 reservation/batch trace 原行、原始协议deadline及prefix completion limit、wait API/arm/return到clock耗时、完整最终报告、源结尾和可获取退出码；核对这些证据后再清理本轮文件。r366的29.7615ms等待API观测仅是历史证据，不代表r371根因。

## 实际媒体命令

```powershell
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r371-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r371-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r371-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r371-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r371-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r371-source2.log' -PassThru
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r371 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r371-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r371-cli-error.log' -PassThru
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r371-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r371-native- --snapshot-format=png udp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
```

## 原始失败与最终报告

```text
[in @ 000002517be5a600] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002517be5a600] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 9778.837167
[in @ 000002516e40bc40] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002516e40bc40] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 8303.475978
[in @ 000002516e40b1c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002516e40b1c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 3623.375167
[in @ 00000251062d0c00] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002516e3f4480] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002516e3f4480] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 3623.375167
[in @ 00000251062d0c00] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 9778.837167
[in @ 0000025106aa31c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000025106aa31c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 8303.475978
[aac @ 0000025177939780] Qavg: 603.614
[aac @ 0000025177939780] 2 frames left in the queue on closing
[CLI] realtime video runtime failed: IoFailure: scheduled datagram submit exceeded its original deadline generation=1 sequence=74119 endpoint=1 release_ns=104548252368 reserved_at_ns=104614393900 not_before_ns=104614605940 deadline_ns=104614731676 observed_at_ns=104615105400
[CLI] final runtime report: queued=0, peakQueued=371, threads=81, processThreads=129, workers=0, workerProgress=270907, workerProcessCalls=1095666, workerWaits=824752, workerWakeups=816758, workerErrors=2, errors=2, stalledIntervals=0, cpuSamples=334, logicalProcessors=22, averageSystemMachineCpuPercent=11.742055, averageProcessMachineCpuPercent=2.044187, peakProcessMachineCpuPercent=6.679389, averageProcessSingleCoreCpuPercent=44.972107, peakProcessSingleCoreCpuPercent=146.946565, initialWorkingSetBytes=189550592, workingSetBytes=252551168, peakWorkingSetBytes=252551168, totalPushed=242743, totalPopped=242651, droppedBuffers=0, encodedPacketsPushed=34908, encodedPacketsPopped=34902, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=5124653, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=208, graphPayloadReservations=97354, graphPayloadReleases=97354, graphPayloadPressureFailures=0, lastInputReceivedAtNs=2150718045000
```

## 临时产物清单

- composition-r371-cli-error.log (2301 bytes)
- composition-r371-cli.log (572850 bytes)
- composition-r371-monitor.log (5069 bytes)
- composition-r371-native-2026-11-02-09h23m36s234.png (896637 bytes)
- composition-r371-source0-stdout.log (540 bytes)
- composition-r371-source0.log (29521 bytes)
- composition-r371-source1-stdout.log (335 bytes)
- composition-r371-source1.log (28993 bytes)
- composition-r371-source2-stdout.log (335 bytes)
- composition-r371-source2.log (28926 bytes)
- composition-r371-vlc.log (4144 bytes)

以上文件归档后按清单删除；指定120秒源保留。本轮没有远程测试。

### 发送期限语义复核

独立只读审查确认：m_groupDeadline取pacing.notAfter，是扣除发送服务时间后的最晚提交时刻，不等于原始协议完成期限。r371允许提交窗口125736ns，预约采样至notBefore为212040ns，实际观测比notBefore晚499460ns。当前证据证明预约算术可行后、socket提交前过期；不能定位Windows定时器或调度根因。下一原规格运行复用现有诊断，无需修改发送策略。Microsoft waitable timer只保证到期置为signaled；RFC2212的时延保证依赖服务与误差界，容量上限和高精度计时器不构成线程调度保证。

依据：https://learn.microsoft.com/en-us/windows/win32/sync/waitable-timer-objects ，https://www.rfc-editor.org/rfc/rfc2212.html 。

## 后续清理核验更正

r374期间提升执行环境的CIM/UDP查询发现本轮VLC残留（r371 PID34696、r372 PID21416）；此前默认环境Get-Process未显示进程，不能证明无残留。两者已核对完整命令行后结束；r371占用UDP62024影响后续VLC播放。此前本报告的无残留断言撤回，媒体失败结论不变。后续统一在与启动相同执行环境核验进程及端口。
