# rk439：四路原规格MPEG-TS/RTP诊断复验

状态：本轮四源原规格RTP实流及诊断接线通过；rk427未复现，未认定根因修复。复用e127a744目标产物SHA256 006f127663332c47dd04ebcb3d9fb62c278ceafc130d713e5be027c62ce565dc，原四路120秒RTP源→HEVC1280×720/30fps/8Mbps CBR、AAC192kbps MPEG-TS/RTP。本轮覆盖RTP媒体及leading SR固定诊断值传递，原规格不降，本机FFmpeg不改。

## 远端实际脚本

acceptance/rk439/run.sh由本机SSH调用，事后删除。
```bash
#!/usr/bin/env bash
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
export PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin:$PATH
export LD_LIBRARY_PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib:/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib:/opt/mt-tools/gcc-12.4-native/lib64
set -u
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk439
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200' -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye 'rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200' >source2.out 2>source2.log &
s2=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk439 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 192.168.96.122 --rtp-port 62024 --sdp /home/tang/MediaTranscode-composition-09093919/acceptance/rk439/output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
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

## Windows实际源与VLC命令

```text
D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61050?rtcpport=61051&pkt_size=1200&localaddr=192.168.96.122"
D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk439-vlc.log --extraintf=rc --rc-host=127.0.0.1:42120 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk439-native- --snapshot-format=png rtp://@:62024
```

## 实际PID与首轮观察

远端wrapper1389581，source0/1/2为1389584/1389585/1389586，CLI1389587、monitor1389592；Windows源540、VLC32616。SDP为MP2T/90000 PT33，媒体62024、RTCP62025。41秒进程CPU150%单核计/RSS56808KiB、errors0，内部phase22676ns/0ppm。VLC RC snapshot保存composition-rk439-native-2026-11-16-15h01m48s940.png，四格实画面已查看。

## 结果

四源完整3600帧/120秒，Windows源540退出0，远端三源exit0。VLC原生快照已确认四格及全黑；全黑文件composition-rk439-native-2026-11-16-15h03m34s018.png。停止前readlink /proc/1389587/exe核身份，再kill -TERM 1389587；CLI caller_stop/exit0，不是源结束自动退出。VLC经RC42120 quit退出0。

```text
CLI_EXIT=0
SOURCE0_EXIT=0
SOURCE1_EXIT=0
SOURCE2_EXIT=0
RUN_FINISHED=1

[CLI] final runtime report: queued=0, peakQueued=1291, threads=102, processThreads=148, workers=0, workerProgress=361636, workerProcessCalls=1458906, workerWaits=1097251, workerWakeups=1075511, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=619, logicalProcessors=8, averageSystemMachineCpuPercent=38.005737, averageProcessMachineCpuPercent=15.964837, peakProcessMachineCpuPercent=31.914894, averageProcessSingleCoreCpuPercent=127.718698, peakProcessSingleCoreCpuPercent=255.319149, initialWorkingSetBytes=46084096, workingSetBytes=58941440, peakWorkingSetBytes=58941440, totalPushed=326732, totalPopped=326693, droppedBuffers=0, encodedPacketsPushed=52091, encodedPacketsPopped=52091, backpressureItems=147, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=8900818, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=404, graphPayloadReservations=215159, graphPayloadReleases=215159, graphPayloadPressureFailures=2, lastInputReceivedAtNs=13111097629644
[2026-11-16 15:03:20.543] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:2 execution_mode=1 committed_batches=16909 committed_datagrams=106351 committed_payload_bytes=126936796 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=61 ordered_queue_maximum_wire_bytes=82716 pacing_reserved=106351 pacing_cancelled=0 pacing_submitted=106351 pacing_maximum_submit_lateness_ns=50739262 pacing_worst_late_global_sequence=81002 pacing_worst_success_sample_available=1 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=61 backlog_high_water_wire_bytes=82716 backlog_max_residence_ns=49724587 last_materialized_sequence=106351 last_scheduled_sequence=106351 last_submitted_sequence=106351 last_committed_sequence=106351 submitted_service_generation=1 submitted_service_sequence=106351 submitted_service_completed_ns=157639935122 submitted_service_duration_ns=16640 submitted_service_available_ns=157639951762 submitted_scope_available_ns=157639951762 shared_scope_charged_datagrams=106351 shared_scope_charged_wire_bytes=129914624 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=1153 evidence_timestamp_tracked=0 evidence_timestamp_untracked=1153 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=396984 endpoint_1_target_effective_socket_bytes=198492 endpoint_1_api_requested_socket_bytes=99246 endpoint_1_effective_socket_bytes=198492 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=106313 endpoint_1_committed_payload_bytes=126934204 endpoint_2_target_effective_socket_bytes=198492 endpoint_2_api_requested_socket_bytes=99246 endpoint_2_effective_socket_bytes=198492 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=38 endpoint_2_committed_payload_bytes=2592
[2026-11-16 15:02:41.565] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=5133 source_master_ns=121030988184 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=194561 effective_output_sample_index=5447708
```

148个进程监控样本，RSS18284–58396KiB（含启动），末样本elapsed158秒/CPU121%单核计/RSS58396KiB；CLI工作集初始46084096、峰值/最终58941440字节。pressureFailures=2保留，workers/errors/droppedBuffers为0，payload账本215159预约释放相等且归零。内部phase不能作为内容同步；delivery_evidence=not_proven不证明NIC出线时限。

## SDP与诊断

```text
v=0
o=composition-rk439 17196195404973535132 1 IN IP4 192.168.96.122
s=composition-rk439
c=IN IP4 192.168.96.122
t=0 0
m=video 62024 RTP/AVP 33
a=rtcp:62025 IN IP4 192.168.96.122
a=rtpmap:33 MP2T/90000
a=ssrc:1607922533 cname:mt-2088109345@media-transcode

[2026-11-16 15:03:20.542] [info] [graph][runtime.node] datagram_successful_submit_trace stage=worst_success generation=1 endpoint=1 sequence=81002 pacing_lateness_ns=50739262 matches_pacing_worst=1 enqueued_master_ns=unavailable submit_interval_scope=pre_transport_checks_and_transport socket_only_ns=unavailable release_master_ns=95405575302 deadline_master_ns=95505573440 publication_role=media original_protocol_deadline_master_ns=95505573440 prefix_completion_limit_master_ns=95505573440 materialized_master_ns=95456621657 planning_generation=1 planning_cursor=10064 planning_cursor_revision=1 planning_publication=10157 planning_protocol_begin=0 planning_protocol_end=22 planning_committed_sequence=81001 planning_observed_master_ns=95456472905 planning_projected_available_master_ns=95456472905 mux_publication_ready_master_ns=95456553697 planning_submitted_service=available planning_submitted_generation=1 planning_submitted_sequence=81001 planning_submit_completed_master_ns=95406267379 planning_service_duration_ns=36480 planning_scope_available_master_ns=95406303859 reserved_at_master_ns=95457027954 not_before_master_ns=95406303859 not_after_master_ns=95505356480 service_duration_ns=216960 wire_bytes_per_second=6250000 submit_started_master_ns=95457043121 submit_completed_master_ns=95457134123 activation_started_master_ns=95457014246 activation_completed_master_ns=95457022121 permit_started_master_ns=95457025038 permit_completed_master_ns=95457026204 snapshot_started_master_ns=95457027079 snapshot_sampled_master_ns=95457027954 snapshot_completed_master_ns=95457029413 reservation_completed_master_ns=95457039038 wait_started_master_ns=95457041080 wait_completed_master_ns=95457041080 submit_observed_master_ns=95457043121 wait_attempts=0 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=0 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=0 wait_call_total_ns=0 wait_call_max_ns=0 wait_before_call_count=0 wait_before_call_total_ns=0 wait_before_call_max_ns=0 wait_arm_count=0 wait_arm_total_ns=0 wait_arm_max_ns=0 wait_api_wall_count=0 wait_api_wall_total_ns=0 wait_api_wall_max_ns=0 wait_return_to_next_clock_count=0 wait_return_to_next_clock_total_ns=0 wait_return_to_next_clock_max_ns=0 wait_target_lateness_count=0 wait_target_lateness_total_ns=0 wait_target_lateness_max_ns=0
```

媒体endpoint1提交106313个数据报、控制endpoint2提交38个/2592字节；所有RTP protocol registration身份/物化时间校验未报错，正常媒体样本记录同次planning snapshot与publication范围，字段无重复。leading SR路径在媒体发布携带控制包时执行相同新增校验，控制端点已实际提交；最坏成功样本恰为媒体，不声称本轮取得SR独立最坏样本。RTP控制和媒体均无时限/partial/ambiguous错误。单轮未复现仍不关闭rk427。

## 输入stderr分类

仅地址归一为ADDRESS，保留启动警告与计数：
```text
1 x mpp[1389587]: mpp_platform: client 12 driver is not ready!
12 x mpp[1389587]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md
8 x [in @ ADDRESS] Changing video frame properties on the fly is not supported by all filters.
2 x [in @ ADDRESS] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 23265.895956
2 x [in @ ADDRESS] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 7184.665367
2 x [in @ ADDRESS] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 43761.032233
2 x [in @ ADDRESS] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 34638.673744
2 x mpp[1389587]: mpp_enc: set prep cfg w:h [1280:720] stride [1280:768] fmt 0 rotate 0 mirror 0
2 x mpp[1389587]: mpp_enc: set rc cbr bps [8000000:8500000:7500000] fps [30:1:fix] - [30:1:fix] gop 60
2 x mpp[1389587]: mpp_enc: mode cbr bps [7500000:8000000:8500000] fps fix [30/1] -> fix [30/1] gop i [60] v [0]
33 x mpp[1389587]: h265d: h265d_decode_nal_sei ret -1006
33 x mpp[1389587]: h265d: pps: invalid pps id 0
33 x mpp[1389587]: h265d: nal: parse unit #1 failed ret -1004
33 x mpp[1389587]: H265d: current stream is no right skip it (nil)
1 x mpp[1389587]: mpp_enc: set prep cfg w:h [1280:720] stride [1280:720] fmt 0 rotate 0 mirror 0
1 x [aac @ ADDRESS] Qavg: 16561.383
1 x mpp[1389587]: mpp_buffer: mpp_buffer_service_deinit cleaning misc group
```

UDP全机统计（不是独立链路丢包证明）：
```text
Udp: InDatagrams NoPorts InErrors OutDatagrams RcvbufErrors SndbufErrors InCsumErrors IgnoredMulti
Udp: 4581278 402495 0 5744415 0 0 0 1833
Udp: InDatagrams NoPorts InErrors OutDatagrams RcvbufErrors SndbufErrors InCsumErrors IgnoredMulti
Udp: 5028524 402514 0 6206423 0 0 0 1855
```

## 清理清单

远端acceptance/rk439：
- cli-error.log
- cli-maps.txt
- cli.log
- monitor.log
- output.sdp
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

本机out/acceptance：
- composition-rk439-evidence.json
- composition-rk439-native-2026-11-16-15h01m48s940.png
- composition-rk439-native-2026-11-16-15h03m34s018.png
- composition-rk439-source3.log
- composition-rk439-source3.out
- composition-rk439-vlc.log

原120秒源、正式构建和rk438回退产物保留。

清理完成：远端19项逐项删除后残留0，本机6项删除后残留0；远端六PID和Windows两个PID均不存在。
