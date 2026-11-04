# rk388：RKMPP 三路 RTP 合屏验证（FAIL）

代码：f7aacdea；日期：2026-11-04。RKMPP 8 核 all-target 增量构建 163/163、退出 0；Windows Release 全量构建退出 0。构建通过不代表媒体验收通过。后续 RKMPP 继续增量，不执行 clean-first。

输入为指定 120 秒连续源，三路 H.264 RTP，选源 2 AAC；输出 HEVC 1280×720、30fps、目标 8000kbps CBR，AAC 192kbps，MPEG-TS/UDP 至 Windows VLC。未执行 local CLI。

## 结果与诊断边界

- 已越过旧 CUDA-only 准备门禁，进入 RKMPP 解码、RGA tile 和 HEVC 编码；三路均有首次 aggregate_contribution。
- VLC 原生快照 16:09:25 与 16:11:05 均为 1280×720 全黑；前者发生在源仍发送期间。没有系统截图。
- VLC 两次统计：视频 decoded/displayed 2164/1066、8156/4062，lost=1；音频 decoded/played 3130/1564、11732/5865，lost=0。不能据此认定可见内容或音画同步通过。
- 三路 VideoDecodeNode 最终仅发布 1、4、1 帧，对应 VideoFilterNode 同数。下一步先核查解码输入/调度停滞，尚不能归因于 RGA 像素合成。
- 队列峰值 3015；工作集 35,893,248 → 426,672,128 bytes；graph payload 高水位 97,505,240 bytes，pressureFailures=1。释放计数平衡不能证明内存稳定。
- 进程平均单核 CPU 44.55%，折算整机 5.57%；内部 A/V phase 末段约 20408ns，不代表内容同步验收。
- FFmpeg PID 772287/772288/772289 均自然结束、退出 0。CLI 772290 在源结束后仍输出；记录结果后发送 SIGINT，退出 0，end_reason=caller_stop，不能记为自然结束。监控 PID 772293。
- VLC PID 31232，经 RC quit 退出。清理前确认以上本轮进程均不存在。
- MPP client 12 driver 警告与色彩 range 变化警告保留如下；未修改外部依赖。

## 实际远程脚本和执行命令

执行：/bin/bash /home/tang/MediaTranscode-composition-09093919/acceptance/rk388/run.sh

~~~bash
#!/usr/bin/env bash
set -u
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk388
/home/tang/ffmpeg-ab1e61a/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/ffmpeg-ab1e61a/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/ffmpeg-ab1e61a/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200' -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye 'rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200' >source2.out 2>source2.log &
s2=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk388 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://192.168.96.122:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
cli=$!
printf 'SOURCE0=%s SOURCE1=%s SOURCE2=%s CLI=%s
' "$s0" "$s1" "$s2" "$cli" | tee pids.txt
(while kill -0 "$cli" 2>/dev/null; do date -Ins; ps -p "$cli" -o pid=,etimes=,pcpu=,rss=,vsz=; sleep 1; done) >monitor.log &
mon=$!
printf 'MONITOR=%s
' "$mon" | tee -a pids.txt
wait "$cli"; code=$?
printf 'CLI_EXIT=%s
' "$code" | tee result.txt
wait "$mon"
wait "$s0"; printf 'SOURCE0_EXIT=%s
' "$?" | tee -a result.txt
wait "$s1"; printf 'SOURCE1_EXIT=%s
' "$?" | tee -a result.txt
wait "$s2"; printf 'SOURCE2_EXIT=%s
' "$?" | tee -a result.txt
printf 'RK388_RUN_FINISHED=1
'


~~~

VLC 实际参数（Start-Process Hidden 启动）：

~~~text
D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk388-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk388-native- --snapshot-format=png udp://@:62024
~~~

原生快照通过已核对 PID 的 127.0.0.1:63020 RC 发送 snapshot、stats；退出发送 quit。

## 必要诊断原文

~~~text
mpp[772290]: mpp_platform: client 12 driver is not ready!
mpp[772290]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
[in @ 0x7201290] Changing video frame properties on the fly is not supported by all filters.
[in @ 0x7201290] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 21271.494878
mpp[772290]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
[in @ 0x724cf30] Changing video frame properties on the fly is not supported by all filters.
[in @ 0x724cf30] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 40501.981389
mpp[772290]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
[in @ 0x72019d0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0x72019d0] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 19671.803478
mpp[772290]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
mpp[772290]: mpp_enc: set prep cfg w:h [1280:720] stride [1280:768] fmt 0 rotate 0 mirror 0
mpp[772290]: mpp_enc: set rc cbr bps [8000000:8500000:7500000] fps [30:1:fix] - [30:1:fix] gop 60
mpp[772290]: mpp_enc: mode cbr bps [7500000:8000000:8500000] fps fix [30/1] -> fix [30/1] gop i [60] v [0]
mpp[772290]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
mpp[772290]: mpp_enc: set prep cfg w:h [1280:720] stride [1280:768] fmt 0 rotate 0 mirror 0
mpp[772290]: mpp_enc: set rc cbr bps [8000000:8500000:7500000] fps [30:1:fix] - [30:1:fix] gop 60
mpp[772290]: mpp_enc: mode cbr bps [7500000:8000000:8500000] fps fix [30/1] -> fix [30/1] gop i [60] v [0]
mpp[772290]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
mpp[772290]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
mpp[772290]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
[in @ 0x7f00002620] Changing video frame properties on the fly is not supported by all filters.
[in @ 0x7f00002620] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 40501.981389
[in @ 0x7edc002620] Changing video frame properties on the fly is not supported by all filters.
[in @ 0x7edc002620] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 21271.494878
[in @ 0x7f08002620] Changing video frame properties on the fly is not supported by all filters.
[in @ 0x7f08002620] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 19671.803478
mpp[772290]: mpp_enc: set prep cfg w:h [1280:720] stride [1280:720] fmt 0 rotate 0 mirror 0
[aac @ 0x7f34000b80] Qavg: 52745.570
mpp[772290]: mpp_buffer: mpp_buffer_service_deinit cleaning misc group
[2026-11-04 16:08:49.354] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-rk388.source.0.input.video generation=1 steady_ns=196435733250543 source_sequence=1 presentation_ns=-1000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-04 16:08:49.354] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-rk388.source.1.input.video generation=1 steady_ns=196435733301878 source_sequence=1 presentation_ns=-1000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-04 16:08:49.354] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-rk388.source.2.input.video generation=1 steady_ns=196435733336295 source_sequence=1 presentation_ns=-2000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-04 16:11:24.513] [info] [graph][runtime.node] source_video_trace_summary node_id=67 node=VideoPreparationNode generation=1 reason=stop steady_ns=196590892056471 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:11:24.513] [info] [graph][runtime.node] source_video_trace_summary node_id=44 node=VideoPreparationNode generation=1 reason=stop steady_ns=196590892107222 input_count=4 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=4 published_count=4 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:11:24.513] [info] [graph][runtime.node] source_video_trace_summary node_id=25 node=VideoPreparationNode generation=1 reason=stop steady_ns=196590892196765 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:11:24.513] [info] [graph][runtime.node] source_video_trace_summary node_id=68 node=VideoFilterNode generation=1 reason=stop steady_ns=196590892421644 input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:11:24.515] [info] [graph][runtime.node] source_video_trace_summary node_id=45 node=VideoFilterNode generation=1 reason=stop steady_ns=196590894113338 input_count=4 accepted_count=4 would_block_count=0 resolved_count=4 output_attempt_count=0 published_count=4 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:11:24.517] [info] [graph][runtime.node] source_video_trace_summary node_id=26 node=VideoFilterNode generation=1 reason=stop steady_ns=196590896448751 input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:11:24.518] [info] [graph][runtime.node] source_video_trace_summary node_id=66 node=VideoFrameRateNode generation=1 reason=stop steady_ns=196590896979884 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:11:24.518] [info] [graph][runtime.node] source_video_trace_summary node_id=43 node=VideoFrameRateNode generation=1 reason=stop steady_ns=196590897197763 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=4 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:11:24.518] [info] [graph][runtime.node] source_video_trace_summary node_id=24 node=VideoFrameRateNode generation=1 reason=stop steady_ns=196590897266306 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:11:24.518] [info] [graph][runtime.node] source_video_trace_summary node_id=65 node=HardwareTransferNode generation=1 reason=stop steady_ns=196590897348849 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:11:24.518] [info] [graph][runtime.node] source_video_trace_summary node_id=42 node=HardwareTransferNode generation=1 reason=stop steady_ns=196590897477767 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=4 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:11:24.518] [info] [graph][runtime.node] source_video_trace_summary node_id=23 node=HardwareTransferNode generation=1 reason=stop steady_ns=196590897529977 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:11:24.519] [info] [graph][runtime.node] source_video_trace_summary node_id=64 node=VideoDecodeNode generation=1 reason=stop steady_ns=196590897744355 input_count=8 accepted_count=7 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:11:24.522] [info] [graph][runtime.node] source_video_trace_summary node_id=41 node=VideoDecodeNode generation=1 reason=stop steady_ns=196590901381497 input_count=11 accepted_count=10 would_block_count=0 resolved_count=4 output_attempt_count=0 published_count=4 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:11:24.526] [info] [graph][runtime.node] source_video_trace_summary node_id=22 node=VideoDecodeNode generation=1 reason=stop steady_ns=196590905399563 input_count=8 accepted_count=7 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:11:24.530] [info] [graph][runtime.node] source_video_trace_summary node_id=62 node=MediaAvBoundReleaseExtractorNode generation=1 reason=stop steady_ns=196590909011621 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=19 release_count=926 release_committed_count=905 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:11:24.530] [info] [graph][runtime.node] source_video_trace_summary node_id=39 node=MediaAvBoundReleaseExtractorNode generation=1 reason=stop steady_ns=196590909113998 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=16 release_count=917 release_committed_count=899 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:11:24.530] [info] [graph][runtime.node] source_video_trace_summary node_id=20 node=MediaAvBoundReleaseExtractorNode generation=1 reason=stop steady_ns=196590909245542 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=16 release_count=925 release_committed_count=907 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:11:24.530] [info] [graph][runtime.node] source_video_trace_summary node_id=61 node=ActivationReleaseSequencer generation=1 reason=stop steady_ns=196590909274126 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=906 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:11:24.530] [info] [graph][runtime.node] source_video_trace_summary node_id=38 node=ActivationReleaseSequencer generation=1 reason=stop steady_ns=196590909377377 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=901 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:11:24.530] [info] [graph][runtime.node] source_video_trace_summary node_id=19 node=ActivationReleaseSequencer generation=1 reason=stop steady_ns=196590909418211 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=909 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=3015, threads=81, processThreads=112, workers=0, workerProgress=165541, workerProcessCalls=676611, workerWaits=511053, workerWakeups=499019, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=616, logicalProcessors=8, averageSystemMachineCpuPercent=42.235558, averageProcessMachineCpuPercent=5.568609, peakProcessMachineCpuPercent=13.440860, averageProcessSingleCoreCpuPercent=44.548874, peakProcessSingleCoreCpuPercent=107.526882, initialWorkingSetBytes=35893248, workingSetBytes=426672128, peakWorkingSetBytes=426672128, totalPushed=135242, totalPopped=132377, droppedBuffers=0, encodedPacketsPushed=51958, encodedPacketsPopped=51958, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=97505240, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=2910, graphPayloadReservations=44929, graphPayloadReleases=44929, graphPayloadPressureFailures=1, lastInputReceivedAtNs=196555044706315
~~~

## 本轮清理清单

本地与远程 rk388 目录内：cli-error.log、cli.log、monitor.log、pids.txt、result.txt、run.sh、source0.log/out、source1.log/out、source2.log/out。本地另有 composition-rk388-vlc.log 及两张 composition-rk388-native 原生快照。归档后逐项删除，保留指定源和正式构建产物。

清理复核：上述文件已逐项删除；本地与远程 rk388 目录均无文件，仅保留空目录。本轮精确 PID 均已退出。
