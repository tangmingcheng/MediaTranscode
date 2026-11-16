# rk429：四路合屏caller-stop修复复验

状态：本轮完整四路运行及caller-stop门禁通过；其他未闭合风险不由本轮覆盖。源码5b5565d4，cancelProducerWaiter在账户已取消时幂等退休修复已双独立SOURCE PASS。四路完整120秒原规格，无重入/录制。源全部自然结束后保持CLI等STOP；核exe SIGTERM进入requestStop，要求caller_stop/exit0、final ledger0，保留所有首错及终态原文，不以资源归零掩盖失败。

```bash
#!/usr/bin/env bash
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
export PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin:$PATH
export LD_LIBRARY_PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib:/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib:/opt/mt-tools/gcc-12.4-native/lib64
set -u
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk429
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200' -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye 'rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200' >source2.out 2>source2.log &
s2=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk429 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://192.168.96.122:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
cli=$!
cat /proc/$cli/maps >cli-maps.txt 2>/dev/null
printf 'SOURCE0=%s SOURCE1=%s SOURCE2=%s CLI=%s\n' "$s0" "$s1" "$s2" "$cli" >pids.txt
(while kill -0 "$cli" 2>/dev/null; do date -Ins; ps -p "$cli" -o pid=,etimes=,pcpu=,rss=,vsz=; { date -Ins; ls -l /proc/$cli/fd; cat /proc/$cli/net/udp; } >>socket-monitor.log 2>&1; sleep 1; done) >monitor.log &
mon=$!
printf 'MONITOR=%s\n' "$mon" >>pids.txt
wait "$cli"; printf 'CLI_EXIT=%s\n' "$?" >result.txt
wait "$mon"
cat /proc/net/snmp >udp-after.txt
wait "$s0"; printf 'SOURCE0_EXIT=%s\n' "$?" >>result.txt
wait "$s1"; printf 'SOURCE1_EXIT=%s\n' "$?" >>result.txt
wait "$s2"; printf 'SOURCE2_EXIT=%s\n' "$?" >>result.txt
printf 'RUN_FINISHED=1\n' >>result.txt
```

## 同步、构建与执行

精确小diff同步：旧规范化SHA256 a254143487f213936a4f8ddbc38e2f9f447a0bba8208b8280591ef29dcbe71e3，新201c5b44f898e879c040f8ca9fb204e31f7acd02bb04d8b4967796ba2ff28185，写前后核验并保持原换行。8核增量all wrapper425463 exit0、4/4完成。CLI SHA256 1355c5970bade0d0f6bef892cf25f029edab571966d05e14b8130f0600d03519；ldd六FFmpeg/MPP/GCC12.4仍既定隔离路径，无not found。

run.sh经bash -n通过，在acceptance/rk429执行nohup /bin/bash run.sh >runner.log 2>&1 &。runner433187、source0/1/2=433189/433190/433191、CLI433192、monitor433194；Windows source37656、VLC42116、父21949。三个远端源各完整3600帧/120秒自然exit0；Windows主线程核3600帧/119.90秒完整完成。

初始elapsed56秒encoded17574、errors0，generation1漂移4535/4536ns、0ppm；elapsed110秒encoded35708、errors0，工作集67,997,696B。随后SSH16531被remote host关闭，不能视作CLI退出；重新登录SSH12829后先核三源日志完整、PID不存在，CLI433192仍活elapsed190秒，工作集保持67,997,696B。

## caller-stop验证

经主线程授权，源全部结束后核/proc/433192/exe为项目CLI，再发送SIGTERM。monotonic起点4876887556715ns，观察PID消失4877535666772ns，约648.111ms（含10ms轮询精度）。stop.done state=Stopped、end_reason=caller_stop、validation stopped successfully，result CLI_EXIT=0，三个SOURCE_EXIT=0、RUN_FINISHED=1。与rk428的退休账户失败相比，本轮停止成功；没有把人工信号本身当成通过依据。

最终workers/workerErrors/errors/queued均0，payloadCurrentBytes/Objects0、reservations=releases220846；encoded62656/62656、droppedBuffers0。工作集初始54,636,544B、峰及最后67,997,696B，平均单核CPU119.01%、峰425.26%（8核归一均14.88%、峰53.16%）；pressureFailures1为硬容量wouldBlock。未见process.failed。此轮不代表已修复rk427偶发发送截止期问题或录制启动DTS问题。

完整终态原文：

```text
[2026-11-16 12:45:28.812] [info] [graph][runtime.node] codec_resolver.release slot=encoder_parameters bufferReferences=1 hasCredit=0 creditReferences=0
[2026-11-16 12:45:28.812] [info] [graph][runtime.lifecycle] stop.done state=Stopped
[2026-11-16 12:45:28.819] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=retired detail="" description=""
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=1689, threads=101, processThreads=147, workers=0, workerProgress=382279, workerProcessCalls=1515029, workerWaits=1132732, workerWakeups=1104461, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=744, logicalProcessors=8, averageSystemMachineCpuPercent=32.888917, averageProcessMachineCpuPercent=14.876376, peakProcessMachineCpuPercent=53.157895, averageProcessSingleCoreCpuPercent=119.011008, peakProcessSingleCoreCpuPercent=425.263158, initialWorkingSetBytes=54636544, workingSetBytes=67997696, peakWorkingSetBytes=67997696, totalPushed=341440, totalPopped=341137, droppedBuffers=0, encodedPacketsPushed=62656, encodedPacketsPopped=62656, backpressureItems=146, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=16088034, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=734, graphPayloadReservations=220846, graphPayloadReleases=220846, graphPayloadPressureFailures=1, lastInputReceivedAtNs=4805537071568
[CLI] realtime video validation stopped successfully

SENDER
[2026-11-16 12:45:28.501] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:2 execution_mode=1 committed_batches=20315 committed_datagrams=108111 committed_payload_bytes=124115532 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=61 ordered_queue_maximum_wire_bytes=81984 pacing_reserved=108111 pacing_cancelled=0 pacing_submitted=108111 pacing_maximum_submit_lateness_ns=33694721 pacing_worst_late_global_sequence=16414 pacing_worst_success_sample_available=1 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=61 backlog_high_water_wire_bytes=81984 backlog_max_residence_ns=46146176 last_materialized_sequence=108111 last_scheduled_sequence=108111 last_submitted_sequence=108111 last_committed_sequence=108111 submitted_service_generation=1 submitted_service_sequence=108111 submitted_service_completed_ns=191149194784 submitted_service_duration_ns=34560 submitted_service_available_ns=191149229344 submitted_scope_available_ns=191149229344 shared_scope_charged_datagrams=108111 shared_scope_charged_wire_bytes=127142640 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=1152 evidence_timestamp_tracked=0 evidence_timestamp_untracked=1152 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=196470 endpoint_1_target_effective_socket_bytes=196470 endpoint_1_api_requested_socket_bytes=98235 endpoint_1_effective_socket_bytes=196470 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=108111 endpoint_1_committed_payload_bytes=124115532

STDERR
mpp[433192]: H265d: current stream is no right skip it (nil)
[in @ 0x7ea4001a70] Changing video frame properties on the fly is not supported by all filters.
[in @ 0x7ea4001a70] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 1053.485267
mpp[433192]: mpp_enc: set prep cfg w:h [1280:720] stride [1280:720] fmt 0 rotate 0 mirror 0
[aac @ 0x7e94000b80] Qavg: 25477.730
mpp[433192]: mpp_buffer: mpp_buffer_service_deinit cleaning misc group
```

清理清单：17个本轮基础脚本/日志文件及project-build-rk429.log；原120秒源与正式产物保留。远端18项已精确删除，目录为空、六PID无残留。
