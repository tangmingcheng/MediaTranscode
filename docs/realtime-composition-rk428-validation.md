# rk428：发送诊断与停止门禁复验

状态：完整源运行结束后requestStop失败，停止门禁FAIL。源码4aef532e，仅发送失败诊断增加previous_submitted_sequence/last_observed_ns/TAT，双独立SOURCE PASS。四路完整120秒规格保持，无重入/录制。先等源自然结束，保持CLI等待STOP，SIGTERM进入requestStop后记录完整首错、submitted_service终态、final report和ledger，不能把exit1仅归为清理。

```bash
#!/usr/bin/env bash
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
export PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin:$PATH
export LD_LIBRARY_PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib:/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib:/opt/mt-tools/gcc-12.4-native/lib64
set -u
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk428
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200' -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye 'rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200' >source2.out 2>source2.log &
s2=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk428 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://192.168.96.122:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
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

## 构建与真实运行

旧文件规范化SHA256 6e40ccc8a452ba86e80be1c2836da61cffe5848b0891daab9b32ddf7c33dda19核验后同步新b4a114fb144450ca5f6d4beb898a9b66d57a480477d4ecf020017d2314392c34。8核增量all wrapper374126 exit0、4/4完成；CLI SHA256 6d9dc8ec022d644d773fb20e54013b70b00e33676b15703fd308d794b20485cc。ldd六FFmpeg/MPP/GCC12.4均既定隔离路径，无not found。

在acceptance/rk428执行nohup /bin/bash run.sh >runner.log 2>&1 &：runner383121、source0/1/2=383126/383127/383128、CLI383129、monitor383132；Windows source35516、VLC17500、父31467。三个远端源完整3600帧/120秒且exit0；Windows完整3600帧/119.90秒。所有源自然结束后CLI仍输出，未复现rk427发送预约失败，但单轮无复现不代表修复。

## STOP结果：不能把exit1仅解释为清理

主线程STOP后核/proc/383129/exe为项目CLI，发送SIGTERM。monotonic开始4378499707749ns，观察PID消失4379078621564ns，约578.915ms；此值含10ms轮询观测精度。CLI_EXIT=1、RUN_FINISHED=1，三个SOURCE_EXIT=0。无process.failed，但output failed/retired明确error9/native0/stage5，stderr原始错误Cancelled: Payload producer retirement requires an active account。SIGTERM进入requestStop，此次是停止语义失败，不是可忽略的人工结束码。

停止末尾先node_abort_payload全部0，再abort.done state=Aborted，随后output failed/retired。最终workers0、workerErrors/errors0、ledger bytes/objects0、reservations=releases214615；资源归零不抵消停止错误。没有独立end_reason/failStatus字段输出，不杜撰。

发送终态deadline_misses0、pacing_submitted107498、submitted_service_completed169389155734ns/duration34560ns/available169389190294ns；delivery_evidence=not_proven。完整原文保留如下。

```text
[2026-11-16 12:37:10.361] [info] [graph][runtime.lifecycle] node_abort_payload node=8 before_objects=0 after_objects=0 before_bytes=0 after_bytes=0 before_reservations=214615 after_reservations=214615 before_releases=214615 after_releases=214615
[2026-11-16 12:37:10.361] [info] [graph][runtime.lifecycle] node_abort_payload node=3 before_objects=0 after_objects=0 before_bytes=0 after_bytes=0 before_reservations=214615 after_reservations=214615 before_releases=214615 after_releases=214615
[2026-11-16 12:37:10.361] [info] [graph][runtime.node] codec_resolver.release slot=encoder_parameters bufferReferences=1 hasCredit=0 creditReferences=0
[2026-11-16 12:37:10.361] [info] [graph][runtime.lifecycle] node_abort_payload node=1 before_objects=0 after_objects=0 before_bytes=0 after_bytes=0 before_reservations=214615 after_reservations=214615 before_releases=214615 after_releases=214615
[2026-11-16 12:37:10.362] [info] [graph][runtime.lifecycle] abort.done state=Aborted
[2026-11-16 12:37:10.368] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=failed detail="Payload producer retirement requires an active account" description="" error=9 native=0 stage=5 reason="Payload producer retirement requires an active account"
[2026-11-16 12:37:10.368] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=retired detail="Payload producer retirement requires an active account" description="" error=9 native=0 stage=5 reason="Payload producer retirement requires an active account"
[CLI] final runtime report: queued=0, peakQueued=1251, threads=101, processThreads=147, workers=0, workerProgress=366068, workerProcessCalls=1471680, workerWaits=1105594, workerWakeups=1081537, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=666, logicalProcessors=8, averageSystemMachineCpuPercent=33.959979, averageProcessMachineCpuPercent=15.588475, peakProcessMachineCpuPercent=29.281768, averageProcessSingleCoreCpuPercent=124.707796, peakProcessSingleCoreCpuPercent=234.254144, initialWorkingSetBytes=47267840, workingSetBytes=58658816, peakWorkingSetBytes=58658816, totalPushed=328915, totalPopped=328874, droppedBuffers=0, encodedPacketsPushed=56006, encodedPacketsPopped=56006, backpressureItems=146, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=9934308, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=419, graphPayloadReservations=214615, graphPayloadReleases=214615, graphPayloadPressureFailures=4, lastInputReceivedAtNs=4328837544199

SUBMITTED_SERVICE
[2026-11-16 12:37:10.049] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:2 execution_mode=1 committed_batches=18175 committed_datagrams=107498 committed_payload_bytes=125758652 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=61 ordered_queue_maximum_wire_bytes=81984 pacing_reserved=107498 pacing_cancelled=0 pacing_submitted=107498 pacing_maximum_submit_lateness_ns=53821584 pacing_worst_late_global_sequence=73687 pacing_worst_success_sample_available=1 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=61 backlog_high_water_wire_bytes=81984 backlog_max_residence_ns=39499076 last_materialized_sequence=107498 last_scheduled_sequence=107498 last_submitted_sequence=107498 last_committed_sequence=107498 submitted_service_generation=1 submitted_service_sequence=107498 submitted_service_completed_ns=169389155734 submitted_service_duration_ns=34560 submitted_service_available_ns=169389190294 submitted_scope_available_ns=169389190294 shared_scope_charged_datagrams=107498 shared_scope_charged_wire_bytes=128768596 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=1152 evidence_timestamp_tracked=0 evidence_timestamp_untracked=1152 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=196470 endpoint_1_target_effective_socket_bytes=196470 endpoint_1_api_requested_socket_bytes=98235 endpoint_1_effective_socket_bytes=196470 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=107498 endpoint_1_committed_payload_bytes=125758652

STDERR_TAIL
mpp[383129]: h265d: nal: parse unit #1 failed ret -1004
mpp[383129]: H265d: current stream is no right skip it (nil)
[in @ 0x7f60001d10] Changing video frame properties on the fly is not supported by all filters.
[in @ 0x7f60001d10] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 42194.1599
mpp[383129]: mpp_enc: set prep cfg w:h [1280:720] stride [1280:720] fmt 0 rotate 0 mirror 0
[aac @ 0x7ec0000b80] Qavg: 19945.027
[CLI] realtime video runtime failed: Cancelled: Payload producer retirement requires an active account
mpp[383129]: mpp_buffer: mpp_buffer_service_deinit cleaning misc group
```

清理清单：本轮17个基础日志/脚本文件及project-build-rk428.log，保留原源和正式产物；远端18项已精确删除，目录为空，六PID无残留。WindowsVLC通过RC quit、父31467exit0，无快照。
