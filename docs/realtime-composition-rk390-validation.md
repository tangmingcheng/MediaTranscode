# rk390：RKMPP 单视频源失源与重入

代码：6d983c7c；文档基线 7d5fafa5。2026-11-04。
输入三路 RTP H.264（源 2 带 AAC），输出 MPEG-TS/UDP HEVC 1280×720、30fps、8000kbps CBR 目标，AAC 192kbps/44100Hz 双声道。使用指定 120 秒连续源，无循环、降规格或 local CLI 测试。

## 已观测阶段

- 初始三格：VLC 原生快照 16:34:37；第四格为空黑格。
- 16:34:49.545 核对 exe 后 SIGINT 源 0 PID 1603540。实际约第 46 秒，非原先预计的 30 秒。源 1 PID 1603541、源 2 PID 1603542 保持运行。
- 16:35:06 原生快照：左上格黑色，其余两个视频格仍有画面。
- 16:35:16.806 同源同端口新进程 PID 1644158 启动。16:35:17.964 source.0 generation=2 aggregate_contribution；16:35:44 原生快照左上格恢复，另两格仍有画面。
- CLI PID 1603543；监控 PID 1603552；VLC PID 34824。
- VLC 初始 video decoded/displayed/lost=1702/836/1，重入后 5672/2821/1；audio decoded/played/lost=2468/1233/0、8166/4082/0；demux corrupted/discontinuities=0。
- 后段与退出结果见下文。上述画面证据不替代内容 A/V、wire、异构矩阵与长期可靠性验收。

## 压力计数解释

源码 MediaGraphPayloadCreditLedger.cpp 在容量不满足、成功登记 waiter 后递增 pressureFailures，并返回 WouldBlock；释放 credit 后可唤醒生产者。因此 rk389 的 4 次计数表示硬边界背压，不等于 4 次丢数据或运行错误。应结合停滞、队列、错误及最终回收判断，不能仅因非零计数宣称验收失败，也不能据平衡回收证明长期物理内存稳定。

## 实际命令

执行：/bin/bash /home/tang/MediaTranscode-composition-09093919/acceptance/rk390/run.sh

~~~bash
#!/usr/bin/env bash
set -u
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk390
/home/tang/ffmpeg-ab1e61a/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/ffmpeg-ab1e61a/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/ffmpeg-ab1e61a/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200' -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye 'rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200' >source2.out 2>source2.log &
s2=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk390 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://192.168.96.122:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
cli=$!
printf 'SOURCE0=%s SOURCE1=%s SOURCE2=%s CLI=%s\n' "$s0" "$s1" "$s2" "$cli" | tee pids.txt
(while kill -0 "$cli" 2>/dev/null; do date -Ins; ps -p "$cli" -o pid=,etimes=,pcpu=,rss=,vsz=; sleep 1; done) >monitor.log &
mon=$!
printf 'MONITOR=%s\n' "$mon" | tee -a pids.txt
wait "$cli"; code=$?
printf 'CLI_EXIT=%s\n' "$code" | tee result.txt
wait "$mon"
wait "$s0"; printf 'SOURCE0_EXIT=%s\n' "$?" | tee -a result.txt
wait "$s1"; printf 'SOURCE1_EXIT=%s\n' "$?" | tee -a result.txt
wait "$s2"; printf 'SOURCE2_EXIT=%s\n' "$?" | tee -a result.txt
printf 'RK390_RUN_FINISHED=1\n'

~~~

源 0 停止与重入（在本轮 acceptance/rk390 目录）：

~~~bash
test "$(readlink /proc/1603540/exe)" = /home/tang/ffmpeg-ab1e61a/bin/ffmpeg && kill -INT 1603540
/home/tang/ffmpeg-ab1e61a/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0-reentry.out 2>source0-reentry.log &
r0=$!; printf 'REENTRY0=%s\n' "$r0" | tee reentry-pid.txt
~~~

VLC 实际启动命令：

~~~powershell
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk390-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk390-native- --snapshot-format=png udp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"
~~~

快照与 stats 通过已核对 PID 的 127.0.0.1:63020 RC 获取。


## 最终结果与边界

16:36:41 快照仅左上重入格有画面；16:37:32 全部源结束后全黑，持续输出。重入源 FFmpeg 发送端完整 120 秒/3600 帧、exit 0；CLI generation 2 解码发布 3568 帧；原源 1/2 自然结束 exit 0；原源 0 主动 SIGINT exit 255。CLI 观测结束后核对 exe 并 SIGINT，caller_stop、exit 0，不能记自然结束。

最终 queued=0、peakQueued=979；工作集 50,290,688→63,320,064 bytes，后段未继续增长；workerErrors/errors/droppedBuffers=0。编码包入/出均 73992。payload 预约/释放均 163349，归零；pressureFailures=1，为已登记 waiter 的背压计数。VLC 最终 video decoded/displayed/lost=12152/6060/1，audio decoded/played/lost=17472/8735/0，demux corrupted/discontinuities=0。

本轮匹配 BYE 后纯视频源重入及其余源隔离的可见功能验证通过；完整合屏验收仍未完成。未覆盖无 BYE 突断、快速 SSRC 切换、所选音频重入与内容静音、线端时序、异构 2–4 源矩阵或长期内存/A/V。不能以本轮替代 Windows r385 的调度问题复验。

## 关键诊断原文

~~~text
mpp[1603543]: mpp_platform: client 12 driver is not ready!
mpp[1603543]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
[in @ 0x153ddd00] Changing video frame properties on the fly is not supported by all filters.
[in @ 0x153ddd00] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 37371.306611
mpp[1603543]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
[in @ 0x153de6d0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0x153de6d0] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 46371.455356
mpp[1603543]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
[in @ 0x153de970] Changing video frame properties on the fly is not supported by all filters.
[in @ 0x153de970] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 45567.362067
mpp[1603543]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
mpp[1603543]: mpp_enc: set prep cfg w:h [1280:720] stride [1280:768] fmt 0 rotate 0 mirror 0
mpp[1603543]: mpp_enc: set rc cbr bps [8000000:8500000:7500000] fps [30:1:fix] - [30:1:fix] gop 60
mpp[1603543]: mpp_enc: mode cbr bps [7500000:8000000:8500000] fps fix [30/1] -> fix [30/1] gop i [60] v [0]
mpp[1603543]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
mpp[1603543]: mpp_enc: set prep cfg w:h [1280:720] stride [1280:768] fmt 0 rotate 0 mirror 0
mpp[1603543]: mpp_enc: set rc cbr bps [8000000:8500000:7500000] fps [30:1:fix] - [30:1:fix] gop 60
mpp[1603543]: mpp_enc: mode cbr bps [7500000:8000000:8500000] fps fix [30/1] -> fix [30/1] gop i [60] v [0]
mpp[1603543]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
mpp[1603543]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
mpp[1603543]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
[in @ 0x7efc002620] Changing video frame properties on the fly is not supported by all filters.
[in @ 0x7efc002620] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 45567.362067
[in @ 0x7ef8002620] Changing video frame properties on the fly is not supported by all filters.
[in @ 0x7ef8002620] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 46371.455356
[in @ 0x7edc002620] Changing video frame properties on the fly is not supported by all filters.
[in @ 0x7edc002620] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 37371.306611
mpp[1603543]: mpp_enc: set prep cfg w:h [1280:720] stride [1280:720] fmt 0 rotate 0 mirror 0
[aac @ 0x7ec8000b80] Qavg: 31911.744
mpp[1603543]: mpp_buffer: mpp_buffer_service_deinit cleaning misc group
[2026-11-04 16:35:17.942] [info] [graph][runtime.node] source_video_trace_summary node_id=19 node=ActivationReleaseSequencer generation=1 reason=generation_change steady_ns=198024320589468 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1352 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:35:17.942] [info] [graph][runtime.node] source_video_trace_summary node_id=20 node=MediaAvBoundReleaseExtractorNode generation=1 reason=generation_change steady_ns=198024320870349 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=1368 release_committed_count=1352 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:35:17.942] [info] [graph][runtime.node] source_video_trace_summary node_id=22 node=VideoDecodeNode generation=1 reason=generation_change steady_ns=198024321247483 input_count=1269 accepted_count=1269 would_block_count=0 resolved_count=1268 output_attempt_count=0 published_count=1267 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=97 incomplete=0
[2026-11-04 16:35:17.947] [info] [graph][runtime.node] source_video_trace_summary node_id=23 node=HardwareTransferNode generation=1 reason=generation_change steady_ns=198024326100634 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1267 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:35:17.947] [info] [graph][runtime.node] source_video_trace_summary node_id=24 node=VideoFrameRateNode generation=1 reason=generation_change steady_ns=198024326322889 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1254 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:35:17.947] [info] [graph][runtime.node] source_video_trace_summary node_id=26 node=VideoFilterNode generation=1 reason=generation_change steady_ns=198024326417683 input_count=1251 accepted_count=1251 would_block_count=0 resolved_count=1251 output_attempt_count=0 published_count=1251 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=3 incomplete=0
[2026-11-04 16:35:17.949] [info] [graph][runtime.node] source_video_trace_summary node_id=25 node=VideoPreparationNode generation=1 reason=generation_change steady_ns=198024328150805 input_count=1247 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1247 published_count=1239 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=4 incomplete=0
[2026-11-04 16:35:17.964] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-rk390.source.0.input.video generation=2 steady_ns=198024343042765 source_sequence=3 presentation_ns=1065666666 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-04 16:37:49.876] [info] [graph][runtime.lifecycle] video_encode.zero_copy_summary drm_prime=6619 software_frame=0
[2026-11-04 16:37:49.877] [info] [graph][runtime.node] source_video_trace_summary node_id=67 node=VideoPreparationNode generation=1 reason=stop steady_ns=198176256011163 input_count=3450 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3450 published_count=3442 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=7 incomplete=0
[2026-11-04 16:37:49.877] [info] [graph][runtime.node] source_video_trace_summary node_id=44 node=VideoPreparationNode generation=1 reason=stop steady_ns=198176256040622 input_count=3446 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3446 published_count=3441 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=2 incomplete=0
[2026-11-04 16:37:49.877] [info] [graph][runtime.node] source_video_trace_summary node_id=25 node=VideoPreparationNode generation=2 reason=stop steady_ns=198176256067164 input_count=3567 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3567 published_count=3567 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=5 incomplete=0
[2026-11-04 16:37:49.877] [info] [graph][runtime.lifecycle] video_filter.zero_copy_summary drm_prime_input=3457 drm_prime_output=3457 rga=3457 software_frame=0
[2026-11-04 16:37:49.877] [info] [graph][runtime.node] source_video_trace_summary node_id=68 node=VideoFilterNode generation=1 reason=stop steady_ns=198176256227876 input_count=3457 accepted_count=3457 would_block_count=0 resolved_count=3457 output_attempt_count=0 published_count=3457 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:37:49.879] [info] [graph][runtime.lifecycle] video_filter.zero_copy_summary drm_prime_input=3449 drm_prime_output=3449 rga=3449 software_frame=0
[2026-11-04 16:37:49.879] [info] [graph][runtime.node] source_video_trace_summary node_id=45 node=VideoFilterNode generation=1 reason=stop steady_ns=198176258442840 input_count=3449 accepted_count=3449 would_block_count=0 resolved_count=3449 output_attempt_count=0 published_count=3448 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-04 16:37:49.881] [info] [graph][runtime.lifecycle] video_filter.zero_copy_summary drm_prime_input=4819 drm_prime_output=4819 rga=4819 software_frame=0
[2026-11-04 16:37:49.881] [info] [graph][runtime.node] source_video_trace_summary node_id=26 node=VideoFilterNode generation=2 reason=stop steady_ns=198176260271046 input_count=3568 accepted_count=3568 would_block_count=0 resolved_count=3568 output_attempt_count=0 published_count=3568 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=3 incomplete=0
[2026-11-04 16:37:49.883] [info] [graph][runtime.node] source_video_trace_summary node_id=66 node=VideoFrameRateNode generation=1 reason=stop steady_ns=198176261989291 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3457 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:37:49.883] [info] [graph][runtime.node] source_video_trace_summary node_id=43 node=VideoFrameRateNode generation=1 reason=stop steady_ns=198176262063668 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3455 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:37:49.883] [info] [graph][runtime.node] source_video_trace_summary node_id=24 node=VideoFrameRateNode generation=2 reason=stop steady_ns=198176262083793 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3568 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:37:49.883] [info] [graph][runtime.node] source_video_trace_summary node_id=65 node=HardwareTransferNode generation=1 reason=stop steady_ns=198176262118211 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3470 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:37:49.883] [info] [graph][runtime.node] source_video_trace_summary node_id=42 node=HardwareTransferNode generation=1 reason=stop steady_ns=198176262146211 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3469 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:37:49.883] [info] [graph][runtime.node] source_video_trace_summary node_id=23 node=HardwareTransferNode generation=2 reason=stop steady_ns=198176262246255 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3568 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:37:49.883] [info] [graph][runtime.lifecycle] video_decode.zero_copy_summary drm_prime=3471 software_frame=0 lineage_admission_receive_retries=64
[2026-11-04 16:37:49.883] [info] [graph][runtime.node] source_video_trace_summary node_id=64 node=VideoDecodeNode generation=1 reason=stop steady_ns=198176262397925 input_count=3473 accepted_count=3473 would_block_count=0 resolved_count=3471 output_attempt_count=0 published_count=3470 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=126 incomplete=0
[2026-11-04 16:37:49.891] [info] [graph][runtime.lifecycle] video_decode.zero_copy_summary drm_prime=3470 software_frame=0 lineage_admission_receive_retries=67
[2026-11-04 16:37:49.891] [info] [graph][runtime.node] source_video_trace_summary node_id=41 node=VideoDecodeNode generation=1 reason=stop steady_ns=198176269692664 input_count=3471 accepted_count=3471 would_block_count=0 resolved_count=3470 output_attempt_count=0 published_count=3469 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=127 incomplete=0
[2026-11-04 16:37:49.898] [info] [graph][runtime.lifecycle] video_decode.zero_copy_summary drm_prime=4836 software_frame=0 lineage_admission_receive_retries=36
[2026-11-04 16:37:49.898] [info] [graph][runtime.node] source_video_trace_summary node_id=22 node=VideoDecodeNode generation=2 reason=stop steady_ns=198176277350245 input_count=3568 accepted_count=3568 would_block_count=0 resolved_count=3568 output_attempt_count=0 published_count=3568 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=97 incomplete=0
[2026-11-04 16:37:49.905] [info] [graph][runtime.node] source_video_trace_summary node_id=62 node=MediaAvBoundReleaseExtractorNode generation=1 reason=stop steady_ns=198176283816342 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=16 release_count=3601 release_committed_count=3584 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:37:49.905] [info] [graph][runtime.node] source_video_trace_summary node_id=39 node=MediaAvBoundReleaseExtractorNode generation=1 reason=stop steady_ns=198176283925136 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=16 release_count=3601 release_committed_count=3583 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:37:49.905] [info] [graph][runtime.node] source_video_trace_summary node_id=20 node=MediaAvBoundReleaseExtractorNode generation=2 reason=stop steady_ns=198176283949636 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=3553 release_committed_count=3553 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:37:49.905] [info] [graph][runtime.node] source_video_trace_summary node_id=61 node=ActivationReleaseSequencer generation=1 reason=stop steady_ns=198176284073014 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3584 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:37:49.905] [info] [graph][runtime.node] source_video_trace_summary node_id=38 node=ActivationReleaseSequencer generation=1 reason=stop steady_ns=198176284108306 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3584 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:37:49.905] [info] [graph][runtime.node] source_video_trace_summary node_id=19 node=ActivationReleaseSequencer generation=2 reason=stop steady_ns=198176284133099 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3553 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=979, threads=81, processThreads=112, workers=0, workerProgress=415564, workerProcessCalls=1199762, workerWaits=784181, workerWakeups=757811, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=878, logicalProcessors=8, averageSystemMachineCpuPercent=45.269477, averageProcessMachineCpuPercent=9.658170, peakProcessMachineCpuPercent=27.979275, averageProcessSingleCoreCpuPercent=77.265361, peakProcessSingleCoreCpuPercent=223.834197, initialWorkingSetBytes=50290688, workingSetBytes=63320064, peakWorkingSetBytes=63320064, totalPushed=355384, totalPopped=355355, droppedBuffers=0, encodedPacketsPushed=73992, encodedPacketsPopped=73992, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=16801599, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=742, graphPayloadReservations=163349, graphPayloadReleases=163349, graphPayloadPressureFailures=1, lastInputReceivedAtNs=198142752319788
~~~

## 清理复核
双端 rk390 下 cli-error.log、cli.log、monitor.log、pids.txt、result.txt、run.sh、source0/1/2 的 log/out、source0-reentry.log/out、reentry-pid.txt、reentry-result.txt 共 16 项已逐项删除；本地 composition-rk390-vlc.log 与 16:34:37、16:35:06、16:35:44、16:36:41、16:37:32 五张原生快照已删除。双端本轮目录仅留空目录；源 1603540/1603541/1603542/1644158、CLI 1603543、监控 1603552、VLC 34824 均复核不存在。指定源、正式构建产物及其他轮次文件未删除。

独立证据边界复核：progress_frame_review_a、progress_frame_review_b 均明确 PASS。准备节点 generation 2 发布 3567 帧；droppedBuffers=0 不表示 stale_node_total=0，不宣称零内容丢失。
