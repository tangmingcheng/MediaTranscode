# rk389：RKMPP 三路 RTP→HEVC 720p30、8Mbps CBR 验证

代码：6d983c7c。2026-11-04。基础三格可见输出与全失源黑屏持续输出已观测；完整合屏验收仍未完成，不作全门禁成功提交。

## 结果

- 输入：指定 120 秒连续源，三路 H.264 RTP，选源 2 的 AAC；输出 HEVC 1280×720、30fps、8000kbps CBR 目标，AAC 192kbps、44100Hz 双声道，MPEG-TS/UDP。
- VLC 原生快照 16:28:53、16:29:59 为三个可见视频格与一个空黑格；16:30:51 在全部源结束后为全黑，仍持续接收音视频。没有系统截图。
- 三路解码发布 3599/3598/3599 帧，admission receive retries 2/4/4；RKMPP/RGA/encoder 均 DRM PRIME，software_frame=0。此前 1/4/1 帧停滞在同规格复验中消失。
- 工作集 35,020,800→44,965,888 bytes，约 48 秒至结束维持末值；队列峰值 350，结束清零。有限时长不能证明长稳。
- payload 高水位 3,196,538 bytes；pressureFailures=4，需进一步定位其归属与影响，不能隐去或宣称容量门禁通过。workerErrors/errors/droppedBuffers 均 0。
- A/V 内部 phase 约 23 微秒（80 秒样本）；非内容级 A/V 同步证明。
- VLC 三次 video decoded/displayed/lost：1690/830/1、5628/2797/3、8756/4361/3；audio decoded/played/lost：2450/1224/0、8102/4050/0、12592/6295/0；demux corrupted/discontinuities 均 0。播放器丢帧不能遗漏。
- FFmpeg PID 1416840/1416841/1416842 自然结束、退出 0。CLI PID 1416844 在全失源后继续输出；核对 exe 后 SIGINT，退出 0、caller_stop。监控 PID 1416847。VLC PID 18788 经所属 RC 端口 quit。
- 本轮未验证单源失源/重入、异构 2–4 源、内容音频静音及长期可靠性；Windows r385 调度问题仍待处理。未修改外部依赖，未推送或部署。

## 实际命令

远程执行：`/bin/bash /home/tang/MediaTranscode-composition-09093919/acceptance/rk389/run.sh`。脚本如下：

~~~bash
#!/usr/bin/env bash
set -u
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk389
/home/tang/ffmpeg-ab1e61a/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/ffmpeg-ab1e61a/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/ffmpeg-ab1e61a/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200' -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye 'rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200' >source2.out 2>source2.log &
s2=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk389 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp 'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://192.168.96.122:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
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
printf 'RK389_RUN_FINISHED=1\n'

~~~

Windows VLC（实际 Start-Process 命令）：

~~~powershell
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk389-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk389-native- --snapshot-format=png udp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"
~~~

原生快照与 stats 经已核对 PID 的 TCP 127.0.0.1:63020 RC 发送；退出发送 quit。

## 后续

先完成单源失源和重新加入，补充所选音频内容证据；保留 pressureFailures 与 VLC lost 的独立调查项。现有结果证明本次接收推进修复有效，不替代完整验收。


## 必要诊断原文
~~~text
mpp[1416844]: mpp_platform: client 12 driver is not ready!
mpp[1416844]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
[in @ 0x3e81e3b0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0x3e81e3b0] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 37522.658656
mpp[1416844]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
[in @ 0x3e81e8e0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0x3e81e8e0] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 10881.913389
mpp[1416844]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
[in @ 0x3e81eb80] Changing video frame properties on the fly is not supported by all filters.
[in @ 0x3e81eb80] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 44098.930089
mpp[1416844]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
mpp[1416844]: mpp_enc: set prep cfg w:h [1280:720] stride [1280:768] fmt 0 rotate 0 mirror 0
mpp[1416844]: mpp_enc: set rc cbr bps [8000000:8500000:7500000] fps [30:1:fix] - [30:1:fix] gop 60
mpp[1416844]: mpp_enc: mode cbr bps [7500000:8000000:8500000] fps fix [30/1] -> fix [30/1] gop i [60] v [0]
mpp[1416844]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
mpp[1416844]: mpp_enc: set prep cfg w:h [1280:720] stride [1280:768] fmt 0 rotate 0 mirror 0
mpp[1416844]: mpp_enc: set rc cbr bps [8000000:8500000:7500000] fps [30:1:fix] - [30:1:fix] gop 60
mpp[1416844]: mpp_enc: mode cbr bps [7500000:8000000:8500000] fps fix [30/1] -> fix [30/1] gop i [60] v [0]
mpp[1416844]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
mpp[1416844]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
mpp[1416844]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
[in @ 0x7ec4002620] Changing video frame properties on the fly is not supported by all filters.
[in @ 0x7ec4002620] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 10881.913389
[in @ 0x7ebc002620] Changing video frame properties on the fly is not supported by all filters.
[in @ 0x7ebc002620] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 37522.658656
[in @ 0x7e80002620] Changing video frame properties on the fly is not supported by all filters.
[in @ 0x7e80002620] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 44098.930089
mpp[1416844]: mpp_enc: set prep cfg w:h [1280:720] stride [1280:720] fmt 0 rotate 0 mirror 0
[aac @ 0x7f60001400] Qavg: 15905.815
mpp[1416844]: mpp_buffer: mpp_buffer_service_deinit cleaning misc group
[2026-11-04 16:31:01.130] [info] [graph][runtime.lifecycle] video_encode.zero_copy_summary drm_prime=4679 software_frame=0
[2026-11-04 16:31:01.131] [info] [graph][runtime.lifecycle] video_filter.zero_copy_summary drm_prime_input=3599 drm_prime_output=3599 rga=3599 software_frame=0
[2026-11-04 16:31:01.134] [info] [graph][runtime.lifecycle] video_filter.zero_copy_summary drm_prime_input=3597 drm_prime_output=3597 rga=3597 software_frame=0
[2026-11-04 16:31:01.136] [info] [graph][runtime.lifecycle] video_filter.zero_copy_summary drm_prime_input=3599 drm_prime_output=3599 rga=3599 software_frame=0
[2026-11-04 16:31:01.139] [info] [graph][runtime.lifecycle] video_decode.zero_copy_summary drm_prime=3599 software_frame=0 lineage_admission_receive_retries=2
[2026-11-04 16:31:01.139] [info] [graph][runtime.node] source_video_trace_summary node_id=64 node=VideoDecodeNode generation=1 reason=stop steady_ns=197767518390719 input_count=3599 accepted_count=3599 would_block_count=0 resolved_count=3599 output_attempt_count=0 published_count=3599 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:31:01.146] [info] [graph][runtime.lifecycle] video_decode.zero_copy_summary drm_prime=3598 software_frame=0 lineage_admission_receive_retries=4
[2026-11-04 16:31:01.146] [info] [graph][runtime.node] source_video_trace_summary node_id=41 node=VideoDecodeNode generation=1 reason=stop steady_ns=197767524839050 input_count=3598 accepted_count=3598 would_block_count=0 resolved_count=3598 output_attempt_count=0 published_count=3598 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 16:31:01.151] [info] [graph][runtime.lifecycle] video_decode.zero_copy_summary drm_prime=3599 software_frame=0 lineage_admission_receive_retries=4
[2026-11-04 16:31:01.151] [info] [graph][runtime.node] source_video_trace_summary node_id=22 node=VideoDecodeNode generation=1 reason=stop steady_ns=197767530050974 input_count=3599 accepted_count=3599 would_block_count=0 resolved_count=3599 output_attempt_count=0 published_count=3599 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=350, threads=81, processThreads=112, workers=0, workerProgress=335562, workerProcessCalls=1010905, workerWaits=675326, workerWakeups=657976, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=620, logicalProcessors=8, averageSystemMachineCpuPercent=47.071001, averageProcessMachineCpuPercent=11.308210, peakProcessMachineCpuPercent=21.808511, averageProcessSingleCoreCpuPercent=90.465678, peakProcessSingleCoreCpuPercent=174.468085, initialWorkingSetBytes=35020800, workingSetBytes=44965888, peakWorkingSetBytes=44965888, totalPushed=300884, totalPopped=300860, droppedBuffers=0, encodedPacketsPushed=52302, encodedPacketsPopped=52302, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=3196538, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=98, graphPayloadReservations=197431, graphPayloadReleases=197431, graphPayloadPressureFailures=4, lastInputReceivedAtNs=197730679433658
~~~

压力计数变化时的运行快照（供后续定位）：
~~~text
[CLI] runtime report: queued=20, peakQueued=23, threads=81, processThreads=93, workers=26, workerProgress=17, workerProcessCalls=48, workerWaits=27, workerWakeups=3, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=0, logicalProcessors=8, averageSystemMachineCpuPercent=0.000000, averageProcessMachineCpuPercent=0.000000, peakProcessMachineCpuPercent=0.000000, averageProcessSingleCoreCpuPercent=0.000000, peakProcessSingleCoreCpuPercent=0.000000, initialWorkingSetBytes=35020800, workingSetBytes=35020800, peakWorkingSetBytes=35020800, totalPushed=23, totalPopped=3, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=123, graphPayloadCurrentBytes=514144, graphPayloadHighWaterBytes=514144, graphPayloadCurrentObjects=14, graphPayloadHighWaterObjects=14, graphPayloadReservations=14, graphPayloadReleases=0, graphPayloadPressureFailures=0, lastInputReceivedAtNs=197611300414244
[CLI] runtime report: queued=46, peakQueued=342, threads=81, processThreads=112, workers=75, workerProgress=108565, workerProcessCalls=331597, workerWaits=223025, workerWakeups=219017, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=168, logicalProcessors=8, averageSystemMachineCpuPercent=48.628623, averageProcessMachineCpuPercent=13.853519, peakProcessMachineCpuPercent=21.808511, averageProcessSingleCoreCpuPercent=110.828154, peakProcessSingleCoreCpuPercent=174.468085, initialWorkingSetBytes=35020800, workingSetBytes=44965888, peakWorkingSetBytes=44965888, totalPushed=98696, totalPopped=98650, droppedBuffers=0, encodedPacketsPushed=14133, encodedPacketsPopped=14132, backpressureItems=123, graphPayloadCurrentBytes=408600, graphPayloadHighWaterBytes=3196538, graphPayloadCurrentObjects=87, graphPayloadHighWaterObjects=97, graphPayloadReservations=66022, graphPayloadReleases=65935, graphPayloadPressureFailures=1, lastInputReceivedAtNs=197653493056072
[CLI] runtime report: queued=48, peakQueued=343, threads=81, processThreads=112, workers=75, workerProgress=186906, workerProcessCalls=570642, workerWaits=383729, workerWakeups=376844, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=290, logicalProcessors=8, averageSystemMachineCpuPercent=49.008351, averageProcessMachineCpuPercent=13.829605, peakProcessMachineCpuPercent=21.808511, averageProcessSingleCoreCpuPercent=110.636838, peakProcessSingleCoreCpuPercent=174.468085, initialWorkingSetBytes=35020800, workingSetBytes=44965888, peakWorkingSetBytes=44965888, totalPushed=169895, totalPopped=169847, droppedBuffers=0, encodedPacketsPushed=24412, encodedPacketsPopped=24411, backpressureItems=123, graphPayloadCurrentBytes=321101, graphPayloadHighWaterBytes=3196538, graphPayloadCurrentObjects=89, graphPayloadHighWaterObjects=98, graphPayloadReservations=113647, graphPayloadReleases=113558, graphPayloadPressureFailures=2, lastInputReceivedAtNs=197684145243490
[CLI] runtime report: queued=47, peakQueued=345, threads=81, processThreads=112, workers=75, workerProgress=256864, workerProcessCalls=785328, workerWaits=528457, workerWakeups=518998, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=399, logicalProcessors=8, averageSystemMachineCpuPercent=49.159455, averageProcessMachineCpuPercent=13.787694, peakProcessMachineCpuPercent=21.808511, averageProcessSingleCoreCpuPercent=110.301553, peakProcessSingleCoreCpuPercent=174.468085, initialWorkingSetBytes=35020800, workingSetBytes=44965888, peakWorkingSetBytes=44965888, totalPushed=233482, totalPopped=233435, droppedBuffers=0, encodedPacketsPushed=33592, encodedPacketsPopped=33590, backpressureItems=123, graphPayloadCurrentBytes=414697, graphPayloadHighWaterBytes=3196538, graphPayloadCurrentObjects=88, graphPayloadHighWaterObjects=98, graphPayloadReservations=156242, graphPayloadReleases=156154, graphPayloadPressureFailures=3, lastInputReceivedAtNs=197711526364834
[CLI] runtime report: queued=48, peakQueued=345, threads=81, processThreads=112, workers=75, workerProgress=268419, workerProcessCalls=821064, workerWaits=552638, workerWakeups=542759, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=417, logicalProcessors=8, averageSystemMachineCpuPercent=49.239896, averageProcessMachineCpuPercent=13.737264, peakProcessMachineCpuPercent=21.808511, averageProcessSingleCoreCpuPercent=109.898109, peakProcessSingleCoreCpuPercent=174.468085, initialWorkingSetBytes=35020800, workingSetBytes=44965888, peakWorkingSetBytes=44965888, totalPushed=243985, totalPopped=243937, droppedBuffers=0, encodedPacketsPushed=35110, encodedPacketsPopped=35109, backpressureItems=123, graphPayloadCurrentBytes=346627, graphPayloadHighWaterBytes=3196538, graphPayloadCurrentObjects=89, graphPayloadHighWaterObjects=98, graphPayloadReservations=163301, graphPayloadReleases=163212, graphPayloadPressureFailures=4, lastInputReceivedAtNs=197716046153658
~~~

清理复核：双端本轮 12 个脚本/日志/结果文件、本地 VLC 日志和 3 张原生快照已逐项删除；双端 rk389 仅留空目录。本轮精确 PID 均已退出。指定 120 秒源与正式产物保留。
