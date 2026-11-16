# rk438：四路原规格规划来源诊断复验

状态：本轮四路实流、全失源黑屏、调用方停止与诊断接线验证通过；rk427未复现，根因未关闭。生产源码e127a744经双独立SOURCE PASS，RKMPP 8核增量44/44构建成功。CLI SHA256 006f127663332c47dd04ebcb3d9fb62c278ceafc130d713e5be027c62ce565dc。沿rk435四源120秒、HEVC1280×720/30fps/8Mbps CBR、AAC192kbps，选中source2。只测realtime，不修改本机FFmpeg。新增诊断不代表rk427修复。

## 本轮远端脚本

路径acceptance/rk438/run.sh，由本机SSH调用，结束后删除。

```bash
#!/usr/bin/env bash
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
export PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin:$PATH
export LD_LIBRARY_PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib:/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib:/opt/mt-tools/gcc-12.4-native/lib64
set -u
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk438
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200' -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye 'rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200' >source2.out 2>source2.log &
s2=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk438 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://192.168.96.122:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
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

## Windows源与VLC命令

```text
D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61050?rtcpport=61051&pkt_size=1200&localaddr=192.168.96.122"
D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk438-vlc.log --extraintf=rc --rc-host=127.0.0.1:42120 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk438-native- --snapshot-format=png udp://@:62024
```

## 实际进程与首轮观察

远端wrapper1332697、source0/1/2为1332700/1332701/1332702、CLI1332703、monitor1332705；Windows HEVC源39308、VLC38664。约41秒CLI仍运行，CPU约152%单核计、RSS56208KiB，错误/丢弃/pressure均0；内部generation1 phase约22676ns、频率0ppm，不能替代内容同步。VLC RC42120发送snapshot，原生PNG为composition-rk438-native-2026-11-16-14h53m13s477.png。

## 结果与边界

三路远端源和Windows HEVC源均完整3600帧/120秒退出0；VLC四格实画面、全失源黑屏原生快照已人工检查。正常停止命令为先readlink /proc/1332703/exe核身份，再kill -TERM 1332703，CLI退出0、end_reason=caller_stop，不记为源结束自动退出。VLC经RC42120 quit退出0。远端六PID及本机两个PID均无残留。

终态记录：
```text
CLI_EXIT=0
SOURCE0_EXIT=0
SOURCE1_EXIT=0
SOURCE2_EXIT=0
RUN_FINISHED=1

[CLI] final runtime report: queued=0, peakQueued=1166, threads=101, processThreads=147, workers=0, workerProgress=354787, workerProcessCalls=1492888, workerWaits=1138083, workerWakeups=1118021, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=586, logicalProcessors=8, averageSystemMachineCpuPercent=38.460129, averageProcessMachineCpuPercent=17.208722, peakProcessMachineCpuPercent=30.729167, averageProcessSingleCoreCpuPercent=137.669779, peakProcessSingleCoreCpuPercent=245.833333, initialWorkingSetBytes=46186496, workingSetBytes=58470400, peakWorkingSetBytes=58470400, totalPushed=321471, totalPopped=321446, droppedBuffers=0, encodedPacketsPushed=49241, encodedPacketsPopped=49241, backpressureItems=146, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=8778766, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=401, graphPayloadReservations=211778, graphPayloadReleases=211778, graphPayloadPressureFailures=2, lastInputReceivedAtNs=12596849504976
[2026-11-16 14:54:37.778] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:2 execution_mode=1 committed_batches=15991 committed_datagrams=105346 committed_payload_bytes=125434164 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=61 ordered_queue_maximum_wire_bytes=81984 pacing_reserved=105346 pacing_cancelled=0 pacing_submitted=105346 pacing_maximum_submit_lateness_ns=33153692 pacing_worst_late_global_sequence=32681 pacing_worst_success_sample_available=1 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=61 backlog_high_water_wire_bytes=81984 backlog_max_residence_ns=42142716 last_materialized_sequence=105346 last_scheduled_sequence=105346 last_submitted_sequence=105346 last_committed_sequence=105346 submitted_service_generation=1 submitted_service_sequence=105346 submitted_service_completed_ns=149115645726 submitted_service_duration_ns=34560 submitted_service_available_ns=149115680286 submitted_scope_available_ns=149115680286 shared_scope_charged_datagrams=105346 shared_scope_charged_wire_bytes=128383852 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=1152 evidence_timestamp_tracked=0 evidence_timestamp_untracked=1152 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=196470 endpoint_1_target_effective_socket_bytes=196470 endpoint_1_api_requested_socket_bytes=98235 endpoint_1_effective_socket_bytes=196470 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=105346 endpoint_1_committed_payload_bytes=125434164
[2026-11-16 14:54:07.321] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=5133 source_master_ns=120903669424 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=194561 effective_output_sample_index=5447708
```

进程监控140个样本，RSS范围12972–57872KiB（含启动阶段），末样本elapsed149秒/CPU129%单核计/RSS57872KiB。CLI报告工作集初始46186496、峰值/最终58470400字节。内部phase约22.676微秒/0ppm不等于内容同步。pressureFailures=2但workerErrors/errors/droppedBuffers=0，保留压力事件而不描述为零压力。UDP全机InErrors/RcvbufErrors/SndbufErrors前后均0，NoPorts增加33；全机统计不等价独立链路抓包。sender delivery_evidence=not_proven，不能证明NIC出线期限。

## 新增来源证据

```text
[2026-11-16 14:54:37.778] [info] [graph][runtime.node] datagram_successful_submit_trace stage=worst_success generation=1 endpoint=1 sequence=32681 pacing_lateness_ns=33153692 matches_pacing_worst=1 enqueued_master_ns=unavailable submit_interval_scope=pre_transport_checks_and_transport socket_only_ns=unavailable release_master_ns=39982189967 deadline_master_ns=40082189213 publication_role=media original_protocol_deadline_master_ns=40082189213 prefix_completion_limit_master_ns=40082189213 materialized_master_ns=40015658731 planning_generation=1 planning_cursor=4077 planning_cursor_revision=1 planning_publication=4114 planning_protocol_begin=0 planning_protocol_end=27 planning_committed_sequence=32680 planning_observed_master_ns=40015508520 planning_projected_available_master_ns=40015508520 mux_publication_ready_master_ns=40015587854 planning_submitted_service=available planning_submitted_generation=1 planning_submitted_sequence=32680 planning_submit_completed_master_ns=39982941529 planning_service_duration_ns=34560 planning_scope_available_master_ns=39982976089 reserved_at_master_ns=40016115780 not_before_master_ns=39982976089 not_after_master_ns=40081974173 service_duration_ns=215040 wire_bytes_per_second=6250000 submit_started_master_ns=40016129781 submit_completed_master_ns=40016372451 activation_started_master_ns=40016103530 activation_completed_master_ns=40016110822 permit_started_master_ns=40016113155 permit_completed_master_ns=40016114030 snapshot_started_master_ns=40016115197 snapshot_sampled_master_ns=40016115780 snapshot_completed_master_ns=40016117239 reservation_completed_master_ns=40016126280 wait_started_master_ns=40016128322 wait_completed_master_ns=40016128322 submit_observed_master_ns=40016129781 wait_attempts=0 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=0 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=0 wait_call_total_ns=0 wait_call_max_ns=0 wait_before_call_count=0 wait_before_call_total_ns=0 wait_before_call_max_ns=0 wait_arm_count=0 wait_arm_total_ns=0 wait_arm_max_ns=0 wait_api_wall_count=0 wait_api_wall_total_ns=0 wait_api_wall_max_ns=0 wait_return_to_next_clock_count=0 wait_return_to_next_clock_total_ns=0 wait_return_to_next_clock_max_ns=0 wait_target_lateness_count=0 wait_target_lateness_total_ns=0 wait_target_lateness_max_ns=0
```

该成功样本sequence32681消费committed/submitted sequence32680，generation1、publication4114、protocol范围[0,27)。最后提交完成39982941529ns，duration34560ns，scope frontier39982976089ns；planning观察40015508520ns，projectedAvailable与观察时刻相等；mux ready40015587854ns、materialized40015658731ns、sender reserve40016115780ns、submit completed40016372451ns。同条记录无重复key。证明实际消费事实已随本包保留，不能用成功样本解释rk427的另一失败包。新增字段未触发生产失败，deadline_misses=0。本轮UDP媒体路径通过，RTP leading SR运行接线尚待对应链路覆盖；Windows共享源码尚未重建复验。

## 原始stderr分类计数

仅把地址统一为ADDRESS；其余文本保留，不以最终errors=0隐藏输入警告。
```text
1 x mpp[1332703]: mpp_platform: client 12 driver is not ready!
12 x mpp[1332703]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
8 x [in @ ADDRESS] Changing video frame properties on the fly is not supported by all filters.
2 x [in @ ADDRESS] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 26403.370756
2 x [in @ ADDRESS] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 34303.9976
2 x [in @ ADDRESS] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 2461.744278
2 x [in @ ADDRESS] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 33403.498644
2 x mpp[1332703]: mpp_enc: set prep cfg w:h [1280:720] stride [1280:768] fmt 0 rotate 0 mirror 0
2 x mpp[1332703]: mpp_enc: set rc cbr bps [8000000:8500000:7500000] fps [30:1:fix] - [30:1:fix] gop 60
2 x mpp[1332703]: mpp_enc: mode cbr bps [7500000:8000000:8500000] fps fix [30/1] -> fix [30/1] gop i [60] v [0]
31 x mpp[1332703]: h265d: h265d_decode_nal_sei ret -1006
31 x mpp[1332703]: h265d: pps: invalid pps id 0
31 x mpp[1332703]: h265d: nal: parse unit #1 failed ret -1004
31 x mpp[1332703]: H265d: current stream is no right skip it (nil)
1 x mpp[1332703]: mpp_enc: set prep cfg w:h [1280:720] stride [1280:720] fmt 0 rotate 0 mirror 0
1 x [aac @ ADDRESS] Qavg: 13683.131
1 x mpp[1332703]: mpp_buffer: mpp_buffer_service_deinit cleaning misc group
```

## 清理清单

远端acceptance/rk438下逐项清理：
- cli-error.log
- cli-maps.txt
- cli.log
- monitor.log
- pids.txt
- result.txt
- run.sh
- runner.log
- socket-monitor.log
- source0.log
- source0.out
- source1.log
- source1.out
- source2.log
- source2.out
- udp-after.txt
- udp-before.txt
- evidence.json

本机out/acceptance下逐项清理：
- composition-rk438-evidence.json
- composition-rk438-native-2026-11-16-14h53m13s477.png
- composition-rk438-native-2026-11-16-14h54m53s646.png
- composition-rk438-source3.log
- composition-rk438-source3.out
- composition-rk438-vlc.log

原120秒源、构建产物及out/rollback/rk438源码/旧CLI回退保留。

清理结果：远端18项删除后目录残留0；本机6项删除后composition-rk438-*残留0。所有本轮PID均已退出。
