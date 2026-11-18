# rk485：控制消息计费修正后四屏复验（进行中）

源码aad0d945；CLI SHA256 1bdbe4e9c195b4b4896aa8704dd794857928655037525683b61c3af2961568b5。双源码审查及RKMPP构建见rk484，不等于实流通过。远端launcher3351255、源0=3351266、源1=3351267、CLI3351268、monitor3351270；本机VLC30032、源2=40820、源3=38032。

## 实际命令

~~~bash
#!/usr/bin/env bash
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
export PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin:$PATH
export LD_LIBRARY_PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib:/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib:/opt/mt-tools/gcc-12.4-native/lib64
set -u
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk485
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk485 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://192.168.130.229:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 192.168.96.122 --rtp-port 62024 --sdp /home/tang/MediaTranscode-composition-09093919/acceptance/rk485/output.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
cli=$!
cat /proc/$cli/maps >cli-maps.txt 2>/dev/null
printf 'SOURCE0=%s SOURCE1=%s CLI=%s\n' "$s0" "$s1" "$cli" >pids.txt
(while kill -0 "$cli" 2>/dev/null; do date -Ins; ps -p "$cli" -o pid=,etimes=,pcpu=,rss=,vsz=; { date -Ins; ls -l /proc/$cli/fd; cat /proc/$cli/net/udp; } >>socket-monitor.log 2>&1; sleep 1; done) >monitor.log &
mon=$!
printf 'MONITOR=%s\n' "$mon" >>pids.txt
wait "$cli"; printf 'CLI_EXIT=%s\n' "$?" >result.txt
wait "$mon"
cat /proc/net/snmp >udp-after.txt
wait "$s0"; printf 'SOURCE0_EXIT=%s\n' "$?" >>result.txt
wait "$s1"; printf 'SOURCE1_EXIT=%s\n' "$?" >>result.txt
printf 'RUN_FINISHED=1\n' >>result.txt

~~~

~~~powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -vf "drawtext=fontfile=''C\:/Windows/Fonts/consola.ttf'':text=''frame=%{frame_num} pts=%{pts}'':fontsize=48:fontcolor=white:box=1:boxcolor=black:x=16:y=16" -c:v h264_nvenc -rc cbr -b:v 8200k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61020?rtcpport=61021&pkt_size=1200&localaddr=192.168.96.122" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://192.168.130.229:61022?rtcpport=61023&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk485-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk485-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE2_EXIT=$($p.ExitCode)"; exit $p.ExitCode
~~~

~~~powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61050?rtcpport=61051&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk485-source3.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk485-source3.log' -PassThru; Write-Output "SOURCE3_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE3_EXIT=$($p.ExitCode)"; exit $p.ExitCode
~~~

~~~powershell
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk485-vlc.log --extraintf=rc --rc-host=127.0.0.1:42120 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk485-native- --snapshot-format=png rtp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "VLC_EXIT=$($p.ExitCode)"
~~~

远端启动：bash -n acceptance/rk485/run.sh && bash acceptance/rk485/run.sh >acceptance/rk485/launcher.log 2>&1 &。


## 结果：FAIL，发送预留无可行窗口

VLC原生快照composition-rk485-native-2026-11-17-12h04m35s504.png已查看四格，左下frame1140/pts38。约63秒sequence49244预留失败：notBefore63149005277 > notAfter63148977746，差27531ns；媒体deadline63149194706、service216960ns不变。本包尚未wait/submit，不能混同rk482提交检查超时；累计等待压力仍在。此轮未到源关闭，不能验证aad0d945关闭修正。

Windows两源完整120秒exit0；外部199样本CPU最大144%单核口径、RSS最大90048KiB。CLI失败后仍挂起，两个decode worker停在sendPacket→mpp_get_frame，main等join；诊断后精确PID3351268清理，不能记自然退出。

~~~text
[2026-11-17 12:04:14.866] [info] [graph][runtime.node] datagram_reservation_trace stage=reservation_failed generation=1 endpoint=1 sequence=49244 release_master_ns=63115862374 deadline_master_ns=63149194706 publication_role=media original_protocol_deadline_master_ns=63149194706 prefix_completion_limit_master_ns=63149194706 materialized_master_ns=63136091428 planning_generation=1 planning_cursor=5409 planning_cursor_revision=3 planning_publication=5443 planning_protocol_begin=170 planning_protocol_end=210 planning_committed_sequence=49207 planning_observed_master_ns=63135685713 planning_projected_available_master_ns=63135737553 mux_publication_ready_master_ns=63135780798 planning_submitted_service=available planning_submitted_generation=1 planning_submitted_sequence=49207 planning_submit_completed_master_ns=63135294289 planning_service_duration_ns=216960 planning_scope_available_master_ns=63135511249 activation_started_master_ns=63136948652 activation_completed_master_ns=63136956235 permit_started_master_ns=63148798234 permit_completed_master_ns=63148798818 snapshot_started_master_ns=63148799401 snapshot_sampled_master_ns=63148799693 snapshot_completed_master_ns=63148800276 reservation_completed_master_ns=63148830901 wait_started_master_ns=unavailable wait_completed_master_ns=unavailable submit_observed_master_ns=unavailable wait_attempts=0 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=0 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=0 wait_call_total_ns=0 wait_call_max_ns=0 wait_before_call_count=0 wait_before_call_total_ns=0 wait_before_call_max_ns=0 wait_arm_count=0 wait_arm_total_ns=0 wait_arm_max_ns=0 wait_api_wall_count=0 wait_api_wall_total_ns=0 wait_api_wall_max_ns=0 wait_return_to_next_clock_count=0 wait_return_to_next_clock_total_ns=0 wait_return_to_next_clock_max_ns=0 wait_target_lateness_count=0 wait_target_lateness_total_ns=0 wait_target_lateness_max_ns=0 original_error="IoFailure: Datagram GBRA reservation misses its immutable completion deadline sequence=49244 now_ns=63148799693 release_ns=63115862374 deadline_ns=63149194706 not_before_ns=63149005277 not_after_ns=63148977746 duration_ns=216960 rate=6250000 queue_wire_bytes=6592 average_queue_residence_ns=12708265 previous_submitted_sequence=49243 last_observed_ns=63148788317 theoretical_arrival_ns=63149005277"
[2026-11-17 12:04:14.866] [info] [graph][runtime.node] process.failed node=102 name=MediaScheduledDatagramSenderNode kind=ScheduledDatagramSender inputs=[channel=134 edge=134 stream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=146 edge=146 stream=Metadata edge_kind=Unknown payload=Unknown queued=0] outputs=[] error=IoFailure: Datagram GBRA reservation misses its immutable completion deadline sequence=49244 now_ns=63148799693 release_ns=63115862374 deadline_ns=63149194706 not_before_ns=63149005277 not_after_ns=63148977746 duration_ns=216960 rate=6250000 queue_wire_bytes=6592 average_queue_residence_ns=12708265 previous_submitted_sequence=49243 last_observed_ns=63148788317 theoretical_arrival_ns=63149005277
[2026-11-17 12:04:14.869] [info] [graph][runtime.node] datagram_batch_trace stage=last_completed generation=1 first_sequence=49208 last_sequence=49208 maximum_submit_interval_ns=53376 maximum_submit_interval_sequence=49208 maximum_start_lateness_ns=1039854 maximum_start_lateness_sequence=49208 first_release_master_ns=63115862374 first_deadline_master_ns=63149194706 last_release_master_ns=63115862374 last_deadline_master_ns=63149194706 first_submitted_master_ns=63136604479 last_submitted_master_ns=63136604479 worst_wait_started_master_ns=63136549936 worst_wait_completed_master_ns=63136549936 worst_submit_observed_master_ns=63136551103 wait_attempts=0 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=0 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=0 wait_call_total_ns=0 wait_call_max_ns=0 wait_before_call_count=0 wait_before_call_total_ns=0 wait_before_call_max_ns=0 wait_arm_count=0 wait_arm_total_ns=0 wait_arm_max_ns=0 wait_api_wall_count=0 wait_api_wall_total_ns=0 wait_api_wall_max_ns=0 wait_return_to_next_clock_count=0 wait_return_to_next_clock_total_ns=0 wait_return_to_next_clock_max_ns=0 wait_target_lateness_count=0 wait_target_lateness_total_ns=0 wait_target_lateness_max_ns=0
[2026-11-17 12:04:14.869] [info] [graph][runtime.node] datagram_batch_trace stage=active generation=1 first_sequence=49209 last_sequence=49248 maximum_submit_interval_ns=99752 maximum_submit_interval_sequence=49209 maximum_start_lateness_ns=333651 maximum_start_lateness_sequence=49209 first_release_master_ns=63115862374 first_deadline_master_ns=63149194706 last_release_master_ns=63115862374 last_deadline_master_ns=63149194706 first_submitted_master_ns=63137074362 last_submitted_master_ns=63148788317 worst_wait_started_master_ns=63136972860 worst_wait_completed_master_ns=63136972860 worst_submit_observed_master_ns=63136974610 wait_attempts=34 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=34 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=34 wait_call_total_ns=9501041 wait_call_max_ns=336297 wait_before_call_count=34 wait_before_call_total_ns=13414 wait_before_call_max_ns=584 wait_arm_count=0 wait_arm_total_ns=0 wait_arm_max_ns=0 wait_api_wall_count=34 wait_api_wall_total_ns=9358414 wait_api_wall_max_ns=332798 wait_return_to_next_clock_count=34 wait_return_to_next_clock_total_ns=33250 wait_return_to_next_clock_max_ns=1750 wait_target_lateness_count=34 wait_target_lateness_total_ns=2930290 wait_target_lateness_max_ns=137421 worst_wait_metric=call_wall worst_wait_sequence=49223 worst_wait_target_master_ns=63141728942 worst_wait_sampled_master_ns=63141528898 worst_wait_sampled_steady_ns=88803545602980 worst_wait_timeout_ns=200044 worst_wait_entered_steady_ns=88803545603272 worst_wait_returned_steady_ns=88803545939569 worst_wait_arm_before_steady_ns=unavailable worst_wait_arm_after_steady_ns=unavailable worst_wait_api_before_steady_ns=88803545605605 worst_wait_api_returned_steady_ns=88803545938403 worst_wait_windows_due_100ns=unavailable worst_wait_windows_result=unavailable worst_wait_windows_error=unavailable worst_wait_windows_pid=unavailable worst_wait_windows_tid=unavailable worst_wait_windows_qpc_frequency_hz=unavailable worst_wait_linux_tid=3352357 worst_wait_linux_scheduler_policy=0 worst_wait_linux_timer_slack_ns=50000 worst_wait_linux_timer_slack_error=unavailable worst_wait_next_master_ns=63141866363 worst_wait_next_steady_ns=88803545940153 worst_wait_outcome=deadline
[2026-11-17 12:04:14.871] [info] [graph][runtime.node] scheduled_datagram_sender stage=failed generation=1 service_scope=ifindex:2 execution_mode=1 committed_batches=5442 committed_datagrams=49243 committed_payload_bytes=61247964 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=85 ordered_queue_maximum_wire_bytes=115260 pacing_reserved=49243 pacing_cancelled=0 pacing_submitted=49243 pacing_maximum_submit_lateness_ns=30665339 pacing_worst_late_global_sequence=20106 pacing_worst_success_sample_available=1 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=5 backlog_current_wire_bytes=6592 backlog_high_water_datagrams=85 backlog_high_water_wire_bytes=115260 backlog_max_residence_ns=50784640 last_materialized_sequence=49248 last_scheduled_sequence=49248 last_submitted_sequence=49243 last_committed_sequence=49243 submitted_service_generation=1 submitted_service_sequence=49243 submitted_service_completed_ns=63148788317 submitted_service_duration_ns=216960 submitted_service_available_ns=63149005277 submitted_scope_available_ns=63149005277 shared_scope_charged_datagrams=49243 shared_scope_charged_wire_bytes=62626768 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=1153 evidence_timestamp_tracked=0 evidence_timestamp_untracked=1153 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=554648 endpoint_1_target_effective_socket_bytes=277324 endpoint_1_api_requested_socket_bytes=138662 endpoint_1_effective_socket_bytes=277324 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=49230 endpoint_1_committed_payload_bytes=61247080 endpoint_2_target_effective_socket_bytes=277324 endpoint_2_api_requested_socket_bytes=138662 endpoint_2_effective_socket_bytes=277324 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=13 endpoint_2_committed_payload_bytes=884
[CLI] runtime report: queued=817, peakQueued=1854, threads=102, processThreads=148, workers=95, workerProgress=150480, workerProcessCalls=608357, workerWaits=457864, workerWakeups=452504, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=199, logicalProcessors=8, averageSystemMachineCpuPercent=38.520292, averageProcessMachineCpuPercent=23.073468, peakProcessMachineCpuPercent=51.813472, averageProcessSingleCoreCpuPercent=184.587745, peakProcessSingleCoreCpuPercent=414.507772, initialWorkingSetBytes=69439488, workingSetBytes=87121920, peakWorkingSetBytes=87121920, totalPushed=138486, totalPopped=137669, droppedBuffers=0, encodedPacketsPushed=16711, encodedPacketsPopped=16710, backpressureItems=147, graphPayloadCurrentBytes=11357334, graphPayloadHighWaterBytes=18352654, graphPayloadCurrentObjects=1157, graphPayloadHighWaterObjects=1165, graphPayloadReservations=76263, graphPayloadReleases=75106, graphPayloadPressureFailures=5, lastInputReceivedAtNs=88803256104197
[CLI] runtime report: queued=821, peakQueued=1854, threads=102, processThreads=148, workers=95, workerProgress=151188, workerProcessCalls=611379, workerWaits=460181, workerWakeups=454790, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=200, logicalProcessors=8, averageSystemMachineCpuPercent=38.497847, averageProcessMachineCpuPercent=23.091609, peakProcessMachineCpuPercent=51.813472, averageProcessSingleCoreCpuPercent=184.732869, peakProcessSingleCoreCpuPercent=414.507772, initialWorkingSetBytes=69439488, workingSetBytes=87121920, peakWorkingSetBytes=87121920, totalPushed=139138, totalPopped=138317, droppedBuffers=0, encodedPacketsPushed=16795, encodedPacketsPopped=16790, backpressureItems=147, graphPayloadCurrentBytes=10570211, graphPayloadHighWaterBytes=18352654, graphPayloadCurrentObjects=1159, graphPayloadHighWaterObjects=1165, graphPayloadReservations=76624, graphPayloadReleases=75465, graphPayloadPressureFailures=5, lastInputReceivedAtNs=88803502964524
Thread 35 (Thread 0x7f1e7dbb40 (LWP 3352303) "mt-n23"):
#0  futex_abstimed_wait_cancelable (private=0, abstime=0x7f1e7da570, clockid=<optimized out>, expected=0, futex_word=0x8dc0f74) at ../sysdeps/nptl/futex-internal.h:320
#1  __pthread_cond_wait_common (abstime=0x7f1e7da570, clockid=<optimized out>, mutex=0x8dc0f18, cond=0x8dc0f48) at pthread_cond_wait.c:520
#2  __pthread_cond_timedwait (cond=0x8dc0f48, mutex=0x8dc0f18, abstime=0x7f1e7da570) at pthread_cond_wait.c:665
#3  0x0000007fa51c97ec in mpp_cond_timedwait () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#4  0x0000007fa51cd880 in mpp_list_wait_timed () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#5  0x0000007fa516617c in mpp_get_frame () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#6  0x0000007fa51696fc in mpi_decode_get_frame () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#7  0x0000007fa64411e8 in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so.62
#8  0x0000007fa644238c in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so.62
#9  0x0000007fa60c8c24 in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so.62
#10 0x0000007fa60c9280 in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so.62
#11 0x0000007fa60c94cc in avcodec_send_packet () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so.62
#12 0x0000000000b3b224 in media::ffmpeg::graph::VideoDecodeNode::submitPendingPacket(media::ffmpeg::graph::MediaGraphExecutionContext&) ()
#13 0x0000000000b3e074 in media::ffmpeg::graph::VideoDecodeNode::onProcess(media::ffmpeg::graph::MediaGraphExecutionContext&) ()
#14 0x00000000009d4b80 in media::ffmpeg::graph::MediaNodeRuntime::process(media::ffmpeg::graph::MediaGraphExecutionContext&) ()
#15 0x00000000009d0ea8 in media::ffmpeg::graph::FFmpegNodeRuntime::processImpl(media::ffmpeg::graph::MediaGraphExecutionContext&) ()
#16 0x00000000009d1114 in media::ffmpeg::graph::FFmpegNodeRuntime::process(media::ffmpeg::graph::MediaGraphExecutionContext&) ()
#17 0x00000000008de388 in media::ffmpeg::graph::MediaGraphWorker::run() ()
#18 0x0000007fa56e8b0c in std::execute_native_thread_routine (__p=0x8eb1e00) at /opt/mt-tools/src/gcc-12.4.0/libstdc++-v3/src/c++11/thread.cc:82
#19 0x0000007fa583c51c in start_thread (arg=0x7fd8f87f9f) at pthread_create.c:477


Thread 34 (Thread 0x7f1dfdab40 (LWP 3352304) "mt-n42"):
#0  futex_abstimed_wait_cancelable (private=0, abstime=0x7f1dfd9570, clockid=<optimized out>, expected=0, futex_word=0x8de6fb0) at ../sysdeps/nptl/futex-internal.h:320
#1  __pthread_cond_wait_common (abstime=0x7f1dfd9570, clockid=<optimized out>, mutex=0x8de6f58, cond=0x8de6f88) at pthread_cond_wait.c:520
#2  __pthread_cond_timedwait (cond=0x8de6f88, mutex=0x8de6f58, abstime=0x7f1dfd9570) at pthread_cond_wait.c:665
#3  0x0000007fa51c97ec in mpp_cond_timedwait () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#4  0x0000007fa51cd880 in mpp_list_wait_timed () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#5  0x0000007fa516617c in mpp_get_frame () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#6  0x0000007fa51696fc in mpi_decode_get_frame () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#7  0x0000007fa64411e8 in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so.62
#8  0x0000007fa644238c in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so.62
#9  0x0000007fa60c8c24 in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so.62
#10 0x0000007fa60c9280 in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so.62
#11 0x0000007fa60c94cc in avcodec_send_packet () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so.62
#12 0x0000000000b3b224 in media::ffmpeg::graph::VideoDecodeNode::submitPendingPacket(media::ffmpeg::graph::MediaGraphExecutionContext&) ()
#13 0x0000000000b3e074 in media::ffmpeg::graph::VideoDecodeNode::onProcess(media::ffmpeg::graph::MediaGraphExecutionContext&) ()
#14 0x00000000009d4b80 in media::ffmpeg::graph::MediaNodeRuntime::process(media::ffmpeg::graph::MediaGraphExecutionContext&) ()
#15 0x00000000009d0ea8 in media::ffmpeg::graph::FFmpegNodeRuntime::processImpl(media::ffmpeg::graph::MediaGraphExecutionContext&) ()
#16 0x00000000009d1114 in media::ffmpeg::graph::FFmpegNodeRuntime::process(media::ffmpeg::graph::MediaGraphExecutionContext&) ()
#17 0x00000000008de388 in media::ffmpeg::graph::MediaGraphWorker::run() ()
#18 0x0000007fa56e8b0c in std::execute_native_thread_routine (__p=0x8eb1fa0) at /opt/mt-tools/src/gcc-12.4.0/libstdc++-v3/src/c++11/thread.cc:82
#19 0x0000007fa583c51c in start_thread (arg=0x7fd8f87f9f) at pthread_create.c:477


Thread 1 (Thread 0x7fa4a79610 (LWP 3351268) "media_transcode"):
#0  __pthread_clockjoin_ex (threadid=545972403008, thread_return=0x0, clockid=0, abstime=<optimized out>, block=<optimized out>) at pthread_join_common.c:145
#1  0x0000007fa56e8d80 in __gthread_join (__value_ptr=0x0, __threadid=<optimized out>) at /opt/mt-tools/build/gcc-12.4.0-native/aarch64-unknown-linux-gnu/libstdc++-v3/include/aarch64-unknown-linux-gnu/bits/gthr-default.h:669
#2  std::thread::join (this=0x8e97778) at /opt/mt-tools/src/gcc-12.4.0/libstdc++-v3/src/c++11/thread.cc:112
#3  0x00000000008db484 in media::ffmpeg::graph::MediaGraphWorker::join() ()
#4  0x00000000008da294 in media::ffmpeg::graph::MediaGraphThreadedExecutor::abort(media::ffmpeg::graph::MediaGraphExecutionContext&, media::ffmpeg::graph::MediaGraphScheduler&) ()
#5  0x000000000087b578 in media::ffmpeg::graph::MediaGraphRuntimeLifecycleExecutor::synchronizeThreadedState(media::ffmpeg::graph::MediaGraphRuntime&) ()
#6  0x00000000007d2ce8 in media::ffmpeg::graph::MediaGraphRuntime::synchronizeThreadedState() ()
#7  0x000000000042502c in media::ffmpeg::graph::MediaRealtimeVideoRunController::run(media::ffmpeg::graph::MediaRealtimeRtpTranscodeRequest const&, media::ffmpeg::graph::MediaRealtimeVideoRunPolicy const&, media::ffmpeg::graph::MediaRealtimeVideoRunControl&, media::ffmpeg::graph::MediaRealtimeVideoRunObserver const&)::{lambda()#1}::operator()() const ()
#8  0x0000000000427284 in media::ffmpeg::graph::MediaRealtimeVideoRunController::run(media::ffmpeg::graph::MediaRealtimeRtpTranscodeRequest const&, media::ffmpeg::graph::MediaRealtimeVideoRunPolicy const&, media::ffmpeg::graph::MediaRealtimeVideoRunControl&, media::ffmpeg::graph::MediaRealtimeVideoRunObserver const&) ()
#9  0x000000000041078c in (anonymous namespace)::runRealtimeVideoCli(int, char**) ()
#10 0x000000000040ef0c in main ()
[Inferior 1 (process 3351268) detached]

~~~

清理清单：本机composition-rk485-native-2026-11-17-12h04m35s504.png、composition-rk485-source2/3各log/out、composition-rk485-vlc.log、rk485-cli.log、rk485-gdb.txt、rk485-run.sh；远端acceptance/rk485内socket-monitor.log、udp-after.txt、output.sdp、gdb-symbols.txt、source0.out、cli-error.log、monitor.log、result.txt、pids.txt、launcher.log、run.sh、cli.log、source1.out、source0.log、udp-before.txt、source1.log、cli-maps.txt，以及out/acceptance/rk485-run.sh。保留复用源与正式二进制。

最终四源均exit0，CLI清理退出137。清理核验：本机9项、远端18项已逐项删除，文件与精确PID查询无残留。
