# rk435：四路原规格发送可靠性复验

状态：本轮运行与停止完成，rk427未复现但未关闭。复用5b5565d4产物（含4aef532e发送诊断），无源码变化不重编译、不改依赖。CLI SHA256已核验1355c5970bade0d0f6bef892cf25f029edab571966d05e14b8130f0600d03519。

沿rk429四路完整120秒原规格。若首错为deadline失败，保留previous_submitted_sequence/last_observed_ns/TAT及reservation全时序；运行成功则源自然结束后caller-stop，核exit/end_reason与ledger。单轮成功不能证明rk427偶发故障已修复。所有源由主线程协调后启动，失败先归档再精确清理。

```bash
#!/usr/bin/env bash
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
export PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin:$PATH
export LD_LIBRARY_PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib:/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib:/opt/mt-tools/gcc-12.4-native/lib64
set -u
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk435
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200' -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye 'rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200' >source2.out 2>source2.log &
s2=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk435 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://192.168.96.122:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
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

## 实际结果与接管

子智能体额度中断后主线程直接SSH接管，未重启CLI；接管时PID603625 elapsed2661秒仍存活、错误0、编码持续增长，全部源已自然结束。四个源均完整3600帧（Windows119.90秒、远端120秒），远端source exit均0。核/proc/603625/exe后发送SIGTERM，CLI caller_stop/exit0、所有worker/error0、账本预约释放1080358一致且归零。全失源延长运行来自接管中断，不宣称计划性长期验收或有源长稳通过；rk427本轮未复现，不能关闭。

完整终态与stderr原文：
```text
[2026-11-16 13:59:48.023] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:2 execution_mode=1 committed_batches=288781 committed_datagrams=374742 committed_payload_bytes=172532300 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=61 ordered_queue_maximum_wire_bytes=81984 pacing_reserved=374742 pacing_cancelled=0 pacing_submitted=374742 pacing_maximum_submit_lateness_ns=28898352 pacing_worst_late_global_sequence=22782 pacing_worst_success_sample_available=1 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=61 backlog_high_water_wire_bytes=81984 backlog_max_residence_ns=39743024 last_materialized_sequence=374742 last_scheduled_sequence=374742 last_submitted_sequence=374742 last_committed_sequence=374742 submitted_service_generation=1 submitted_service_sequence=374742 submitted_service_completed_ns=2677873386993 submitted_service_duration_ns=34560 submitted_service_available_ns=2677873421553 submitted_scope_available_ns=2677873421553 shared_scope_charged_datagrams=374742 shared_scope_charged_wire_bytes=183025076 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=1152 evidence_timestamp_tracked=0 evidence_timestamp_untracked=1152 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=196470 endpoint_1_target_effective_socket_bytes=196470 endpoint_1_api_requested_socket_bytes=98235 endpoint_1_effective_socket_bytes=196470 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=374742 endpoint_1_committed_payload_bytes=172532300
[2026-11-16 13:59:48.296] [info] [graph][runtime.lifecycle] stop.done state=Stopped
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=2040, threads=101, processThreads=147, workers=0, workerProgress=2396622, workerProcessCalls=6768042, workerWaits=4371402, workerWakeups=3854359, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=10605, logicalProcessors=8, averageSystemMachineCpuPercent=25.816235, averageProcessMachineCpuPercent=5.238160, peakProcessMachineCpuPercent=40.211640, averageProcessSingleCoreCpuPercent=41.905281, peakProcessSingleCoreCpuPercent=321.693122, initialWorkingSetBytes=63283200, workingSetBytes=79556608, peakWorkingSetBytes=79556608, totalPushed=1891267, totalPopped=1890953, droppedBuffers=0, encodedPacketsPushed=895809, encodedPacketsPopped=895809, backpressureItems=146, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=19717180, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=980, graphPayloadReservations=1080358, graphPayloadReleases=1080358, graphPayloadPressureFailures=2, lastInputReceivedAtNs=6778313097001
STDERR
mpp[603625]: mpp_platform: client 12 driver is not ready!
mpp[603625]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
[in @ 0x13ecc2b0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0x13ecc2b0] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 38593.600156
mpp[603625]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
[in @ 0x13ece740] Changing video frame properties on the fly is not supported by all filters.
[in @ 0x13ece740] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 31480.907467
mpp[603625]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
[in @ 0x13ece9e0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0x13ece9e0] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 6169.797944
mpp[603625]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
[in @ 0x13ecc2b0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0x13ecc2b0] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 46617.057344
mpp[603625]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
mpp[603625]: mpp_enc: set prep cfg w:h [1280:720] stride [1280:768] fmt 0 rotate 0 mirror 0
mpp[603625]: mpp_enc: set rc cbr bps [8000000:8500000:7500000] fps [30:1:fix] - [30:1:fix] gop 60
mpp[603625]: mpp_enc: mode cbr bps [7500000:8000000:8500000] fps fix [30/1] -> fix [30/1] gop i [60] v [0]
mpp[603625]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
mpp[603625]: mpp_enc: set prep cfg w:h [1280:720] stride [1280:768] fmt 0 rotate 0 mirror 0
mpp[603625]: mpp_enc: set rc cbr bps [8000000:8500000:7500000] fps [30:1:fix] - [30:1:fix] gop 60
mpp[603625]: mpp_enc: mode cbr bps [7500000:8000000:8500000] fps fix [30/1] -> fix [30/1] gop i [60] v [0]
mpp[603625]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
mpp[603625]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
mpp[603625]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
mpp[603625]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
mpp[603625]: h265d: h265d_decode_nal_sei ret -1006
mpp[603625]: h265d: pps: invalid pps id 0
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
mpp[603625]: h265d: nal: parse unit #1 failed ret -1004
mpp[603625]: H265d: current stream is no right skip it (nil)
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
mpp[603625]: h265d: h265d_decode_nal_sei ret -1006
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
mpp[603625]: h265d: pps: invalid pps id 0
mpp[603625]: h265d: nal: parse unit #1 failed ret -1004
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
mpp[603625]: H265d: current stream is no right skip it (nil)
mpp[603625]: h265d: h265d_decode_nal_sei ret -1006
mpp[603625]: h265d: pps: invalid pps id 0
mpp[603625]: h265d: nal: parse unit #1 failed ret -1004
mpp[603625]: H265d: current stream is no right skip it (nil)
mpp[603625]: h265d: h265d_decode_nal_sei ret -1006
mpp[603625]: h265d: pps: invalid pps id 0
mpp[603625]: h265d: nal: parse unit #1 failed ret -1004
mpp[603625]: H265d: current stream is no right skip it (nil)
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
[in @ 0x7ecc0013b0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0x7ecc0013b0] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 31480.907467
mpp[603625]: h265d: h265d_decode_nal_sei ret -1006
mpp[603625]: h265d: pps: invalid pps id 0
mpp[603625]: h265d: nal: parse unit #1 failed ret -1004
mpp[603625]: H265d: current stream is no right skip it (nil)
mpp[603625]: h265d: h265d_decode_nal_sei ret -1006
mpp[603625]: h265d: pps: invalid pps id 0
mpp[603625]: h265d: nal: parse unit #1 failed ret -1004
mpp[603625]: H265d: current stream is no right skip it (nil)
mpp[603625]: h265d: h265d_decode_nal_sei ret -1006
mpp[603625]: h265d: pps: invalid pps id 0
mpp[603625]: h265d: nal: parse unit #1 failed ret -1004
mpp[603625]: H265d: current stream is no right skip it (nil)
mpp[603625]: h265d: h265d_decode_nal_sei ret -1006
mpp[603625]: h265d: pps: invalid pps id 0
mpp[603625]: h265d: nal: parse unit #1 failed ret -1004
mpp[603625]: H265d: current stream is no right skip it (nil)
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
[in @ 0x7ec8002620] Changing video frame properties on the fly is not supported by all filters.
[in @ 0x7ec8002620] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 38593.600156
mpp[603625]: h265d: h265d_decode_nal_sei ret -1006
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
mpp[603625]: h265d: pps: invalid pps id 0
mpp[603625]: h265d: nal: parse unit #1 failed ret -1004
mpp[603625]: H265d: current stream is no right skip it (nil)
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
mpp[603625]: h265d: h265d_decode_nal_sei ret -1006
mpp[603625]: h265d: pps: invalid pps id 0
mpp[603625]: h265d: nal: parse unit #1 failed ret -1004
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
mpp[603625]: H265d: current stream is no right skip it (nil)
mpp[603625]: h265d: h265d_decode_nal_sei ret -1006
mpp[603625]: h265d: pps: invalid pps id 0
mpp[603625]: h265d: nal: parse unit #1 failed ret -1004
mpp[603625]: H265d: current stream is no right skip it (nil)
mpp[603625]: h265d: h265d_decode_nal_sei ret -1006
mpp[603625]: h265d: pps: invalid pps id 0
mpp[603625]: h265d: nal: parse unit #1 failed ret -1004
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
mpp[603625]: H265d: current stream is no right skip it (nil)
mpp[603625]: h265d: h265d_decode_nal_sei ret -1006
mpp[603625]: h265d: pps: invalid pps id 0
mpp[603625]: h265d: nal: parse unit #1 failed ret -1004
mpp[603625]: H265d: current stream is no right skip it (nil)
mpp[603625]: h265d: h265d_decode_nal_sei ret -1006
mpp[603625]: h265d: pps: invalid pps id 0
mpp[603625]: h265d: nal: parse unit #1 failed ret -1004
mpp[603625]: H265d: current stream is no right skip it (nil)
mpp[603625]: h265d: h265d_decode_nal_sei ret -1006
mpp[603625]: h265d: pps: invalid pps id 0
mpp[603625]: h265d: nal: parse unit #1 failed ret -1004
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
mpp[603625]: H265d: current stream is no right skip it (nil)
mpp[603625]: h265d: h265d_decode_nal_sei ret -1006
mpp[603625]: h265d: pps: invalid pps id 0
mpp[603625]: h265d: nal: parse unit #1 failed ret -1004
mpp[603625]: H265d: current stream is no right skip it (nil)
mpp[603625]: h265d: h265d_decode_nal_sei ret -1006
mpp[603625]: h265d: pps: invalid pps id 0
mpp[603625]: h265d: nal: parse unit #1 failed ret -1004
mpp[603625]: H265d: current stream is no right skip it (nil)
mpp[603625]: h265d: h265d_decode_nal_sei ret -1006
mpp[603625]: h265d: pps: invalid pps id 0
mpp[603625]: h265d: nal: parse unit #1 failed ret -1004
mpp[603625]: H265d: current stream is no right skip it (nil)
mpp[603625]: h265d: h265d_decode_nal_sei ret -1006
mpp[603625]: h265d: pps: invalid pps id 0
mpp[603625]: h265d: nal: parse unit #1 failed ret -1004
mpp[603625]: H265d: current stream is no right skip it (nil)
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
mpp[603625]: h265d: h265d_decode_nal_sei ret -1006
mpp[603625]: h265d: pps: invalid pps id 0
mpp[603625]: h265d: nal: parse unit #1 failed ret -1004
mpp[603625]: H265d: current stream is no right skip it (nil)
mpp[603625]: h265d: h265d_decode_nal_sei ret -1006
mpp[603625]: h265d: pps: invalid pps id 0
mpp[603625]: h265d: nal: parse unit #1 failed ret -1004
mpp[603625]: H265d: current stream is no right skip it (nil)
mpp[603625]: h265d: h265d_decode_nal_sei ret -1006
mpp[603625]: h265d: pps: invalid pps id 0
mpp[603625]: h265d: nal: parse unit #1 failed ret -1004
mpp[603625]: H265d: current stream is no right skip it (nil)
mpp[603625]: h265d: h265d_decode_nal_sei ret -1006
mpp[603625]: h265d: pps: invalid pps id 0
mpp[603625]: h265d: nal: parse unit #1 failed ret -1004
mpp[603625]: H265d: current stream is no right skip it (nil)
mpp[603625]: h265d: h265d_decode_nal_sei ret -1006
mpp[603625]: h265d: pps: invalid pps id 0
mpp[603625]: h265d: nal: parse unit #1 failed ret -1004
mpp[603625]: H265d: current stream is no right skip it (nil)
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
mpp[603625]: h265d: h265d_decode_nal_sei ret -1006
mpp[603625]: h265d: pps: invalid pps id 0
mpp[603625]: h265d: nal: parse unit #1 failed ret -1004
mpp[603625]: H265d: current stream is no right skip it (nil)
mpp[603625]: h265d: h265d_decode_nal_sei ret -1006
mpp[603625]: h265d: pps: invalid pps id 0
mpp[603625]: h265d: nal: parse unit #1 failed ret -1004
mpp[603625]: H265d: current stream is no right skip it (nil)
mpp[603625]: h265d: h265d_decode_nal_sei ret -1006
mpp[603625]: h265d: pps: invalid pps id 0
mpp[603625]: h265d: nal: parse unit #1 failed ret -1004
mpp[603625]: H265d: current stream is no right skip it (nil)
mpp[603625]: h265d: h265d_decode_nal_sei ret -1006
mpp[603625]: h265d: pps: invalid pps id 0
mpp[603625]: h265d: nal: parse unit #1 failed ret -1004
mpp[603625]: H265d: current stream is no right skip it (nil)
mpp[603625]: h264d_sei: seq_parameter_set_id 0 may be invalid
[in @ 0x7ec00013b0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0x7ec00013b0] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 6169.797944
mpp[603625]: h265d: h265d_decode_nal_sei ret -1006
mpp[603625]: h265d: pps: invalid pps id 0
mpp[603625]: h265d: nal: parse unit #1 failed ret -1004
mpp[603625]: H265d: current stream is no right skip it (nil)
mpp[603625]: h265d: h265d_decode_nal_sei ret -1006
mpp[603625]: h265d: pps: invalid pps id 0
mpp[603625]: h265d: nal: parse unit #1 failed ret -1004
mpp[603625]: H265d: current stream is no right skip it (nil)
mpp[603625]: h265d: h265d_decode_nal_sei ret -1006
mpp[603625]: h265d: pps: invalid pps id 0
mpp[603625]: h265d: nal: parse unit #1 failed ret -1004
mpp[603625]: H265d: current stream is no right skip it (nil)
mpp[603625]: h265d: h265d_decode_nal_sei ret -1006
mpp[603625]: h265d: pps: invalid pps id 0
mpp[603625]: h265d: nal: parse unit #1 failed ret -1004
mpp[603625]: H265d: current stream is no right skip it (nil)
mpp[603625]: h265d: h265d_decode_nal_sei ret -1006
mpp[603625]: h265d: pps: invalid pps id 0
mpp[603625]: h265d: nal: parse unit #1 failed ret -1004
mpp[603625]: H265d: current stream is no right skip it (nil)
mpp[603625]: h265d: h265d_decode_nal_sei ret -1006
mpp[603625]: h265d: pps: invalid pps id 0
mpp[603625]: h265d: nal: parse unit #1 failed ret -1004
mpp[603625]: H265d: current stream is no right skip it (nil)
mpp[603625]: h265d: h265d_decode_nal_sei ret -1006
mpp[603625]: h265d: pps: invalid pps id 0
mpp[603625]: h265d: nal: parse unit #1 failed ret -1004
mpp[603625]: H265d: current stream is no right skip it (nil)
mpp[603625]: h265d: h265d_decode_nal_sei ret -1006
mpp[603625]: h265d: pps: invalid pps id 0
mpp[603625]: h265d: nal: parse unit #1 failed ret -1004
mpp[603625]: H265d: current stream is no right skip it (nil)
mpp[603625]: h265d: h265d_decode_nal_sei ret -1006
mpp[603625]: h265d: pps: invalid pps id 0
mpp[603625]: h265d: nal: parse unit #1 failed ret -1004
mpp[603625]: H265d: current stream is no right skip it (nil)
mpp[603625]: h265d: h265d_decode_nal_sei ret -1006
mpp[603625]: h265d: pps: invalid pps id 0
mpp[603625]: h265d: nal: parse unit #1 failed ret -1004
mpp[603625]: H265d: current stream is no right skip it (nil)
mpp[603625]: h265d: h265d_decode_nal_sei ret -1006
mpp[603625]: h265d: pps: invalid pps id 0
mpp[603625]: h265d: nal: parse unit #1 failed ret -1004
mpp[603625]: H265d: current stream is no right skip it (nil)
[in @ 0x7e380028d0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0x7e380028d0] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 46617.057344
mpp[603625]: mpp_enc: set prep cfg w:h [1280:720] stride [1280:720] fmt 0 rotate 0 mirror 0
[aac @ 0x7ebc000b80] Qavg: 62789.914
mpp[603625]: mpp_buffer: mpp_buffer_service_deinit cleaning misc group

RESULT
CLI_EXIT=0
SOURCE0_EXIT=0
SOURCE1_EXIT=0
SOURCE2_EXIT=0
RUN_FINISHED=1

```

最后有源A/V telemetry：generation1 sequence4943 source_master121124942495ns，raw_phase0ns、filtered_phase1ns、frequency/integral/stretch均0ppm；不等于内容级同步。远端清理精确18项：cli-error.log、cli-maps.txt、cli.log、handoff-summary.txt、monitor.log、pids.txt、result.txt、run.sh、runner.log、socket-monitor.log、source0/1/2.log/out、udp-after.txt、udp-before.txt。全部六PID核验不存在，记录后删除上述清单，保留用户源和正式产物。

最终清理核验：远端18项删除后目录为空，本机6项删除后前缀无残留。工作集10607次采样范围63283200–79556608B，最后100次均79556608B；仅该运行窗口趋势，不证明长期设备内存稳定。
