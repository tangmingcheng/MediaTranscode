# rk482：四屏RTCP上界与关闭流程实流复验（进行中）

源码eb7f6a9d，RKMPP 8核增量PID3199892退出0，CLI SHA256 55f364b014e0ec1979bf8d9eabf57ebcb11c7a49856c2bb1354303171497bed3。双独立源码审查通过不等同实流验收。回退out/rollback/rk482保留7个原源码及上一版CLI，恢复需删除新增SourceDrain/SourceRetirement并刷新恢复源码时间戳。

远端launcher3209843、source0=3209852、source1=3209853、CLI3209855、monitor3209861；本机VLC35256、source2=13324、source3=38276。VLC原生快照composition-rk482-native-2026-11-17-11h34m52s795.png已查看，四格视频，左下frame830/pts27.666667。

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
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk482
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk482 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://192.168.130.229:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 192.168.96.122 --rtp-port 62024 --sdp /home/tang/MediaTranscode-composition-09093919/acceptance/rk482/output.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
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
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -vf "drawtext=fontfile=''C\:/Windows/Fonts/consola.ttf'':text=''frame=%{frame_num} pts=%{pts}'':fontsize=48:fontcolor=white:box=1:boxcolor=black:x=16:y=16" -c:v h264_nvenc -rc cbr -b:v 8200k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61020?rtcpport=61021&pkt_size=1200&localaddr=192.168.96.122" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://192.168.130.229:61022?rtcpport=61023&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk482-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk482-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE2_EXIT=$($p.ExitCode)"; exit $p.ExitCode
~~~

~~~powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61050?rtcpport=61051&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk482-source3.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk482-source3.log' -PassThru; Write-Output "SOURCE3_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE3_EXIT=$($p.ExitCode)"; exit $p.ExitCode
~~~

~~~powershell
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk482-vlc.log --extraintf=rc --rc-host=127.0.0.1:42120 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk482-native- --snapshot-format=png rtp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "VLC_EXIT=$($p.ExitCode)"
~~~

远端启动：bash -n acceptance/rk482/run.sh && bash acceptance/rk482/run.sh >acceptance/rk482/launcher.log 2>&1 &。


## 结果：FAIL

约117秒sequence106752提交前超latest-start 28165ns，notBefore117241298670，latest-start117241370885，observed117241399050；等待目标超调95130ns。当前batch25次wait累计超调2593194ns。没有放宽100ms预算。四屏快照成立但连续/失源/重入门禁未通过。Windows两源完整120秒exit0；远端源已结束，CLI在失败后仍存活超过60秒，未自然退出。外部167样本CPU最大158%单核口径、RSS最大74772KiB，最后drift raw13606ns/filtered13605ns、0ppm，不能等同端到端A/V证明。gdb默认命令不在PATH，首次诊断exit127，改用已有工具链绝对路径。

~~~text
[2026-11-17 11:35:40.383] [info] [graph][runtime.node] datagram_reservation_trace stage=submit_deadline_missed generation=1 endpoint=1 sequence=106752 release_master_ns=117208256673 deadline_master_ns=117241587845 publication_role=media original_protocol_deadline_master_ns=117241587845 prefix_completion_limit_master_ns=117241587845 materialized_master_ns=117231002396 planning_generation=1 planning_cursor=11677 planning_cursor_revision=3 planning_publication=11749 planning_protocol_begin=170 planning_protocol_end=210 planning_committed_sequence=106725 planning_observed_master_ns=117230490511 planning_projected_available_master_ns=117230542351 mux_publication_ready_master_ns=117230616805 planning_submitted_service=available planning_submitted_generation=1 planning_submitted_sequence=106725 planning_submit_completed_master_ns=117230027627 planning_service_duration_ns=216960 planning_scope_available_master_ns=117230244587 activation_started_master_ns=117231832786 activation_completed_master_ns=117231838911 permit_started_master_ns=117241109128 permit_completed_master_ns=117241109711 snapshot_started_master_ns=117241110294 snapshot_sampled_master_ns=117241110878 snapshot_completed_master_ns=117241111461 reservation_completed_master_ns=117241118169 wait_started_master_ns=117241119336 wait_completed_master_ns=117241393800 submit_observed_master_ns=117241399050 wait_attempts=1 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=1 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=1 wait_call_total_ns=272131 wait_call_max_ns=272131 wait_before_call_count=1 wait_before_call_total_ns=291 wait_before_call_max_ns=291 wait_arm_count=0 wait_arm_total_ns=0 wait_arm_max_ns=0 wait_api_wall_count=0 wait_api_wall_total_ns=0 wait_api_wall_max_ns=0 wait_return_to_next_clock_count=1 wait_return_to_next_clock_total_ns=2041 wait_return_to_next_clock_max_ns=2041 wait_target_lateness_count=1 wait_target_lateness_total_ns=95130 wait_target_lateness_max_ns=95130 worst_wait_metric=call_wall worst_wait_sequence=106752 worst_wait_target_master_ns=117241298670 worst_wait_sampled_master_ns=117241119336 worst_wait_sampled_steady_ns=87089070139339 worst_wait_timeout_ns=179334 worst_wait_entered_steady_ns=87089070139630 worst_wait_returned_steady_ns=87089070411761 worst_wait_arm_before_steady_ns=unavailable worst_wait_arm_after_steady_ns=unavailable worst_wait_api_before_steady_ns=unavailable worst_wait_api_returned_steady_ns=unavailable worst_wait_windows_due_100ns=unavailable worst_wait_windows_result=unavailable worst_wait_windows_error=unavailable worst_wait_windows_pid=unavailable worst_wait_windows_tid=unavailable worst_wait_windows_qpc_frequency_hz=unavailable worst_wait_next_master_ns=117241393800 worst_wait_next_steady_ns=87089070413802 worst_wait_outcome=deadline original_error="IoFailure: scheduled datagram submit exceeded its original deadline generation=1 sequence=106752 endpoint=1 release_ns=117208256673 reserved_at_ns=117241110878 not_before_ns=117241298670 deadline_ns=117241370885 observed_at_ns=117241399050"
[2026-11-17 11:35:40.390] [info] [graph][runtime.node] datagram_batch_trace stage=last_completed generation=1 first_sequence=106726 last_sequence=106726 maximum_submit_interval_ns=51918 maximum_submit_interval_sequence=106726 maximum_start_lateness_ns=1323944 maximum_start_lateness_sequence=106726 first_release_master_ns=117208256673 first_deadline_master_ns=117241587845 last_release_master_ns=117208256673 last_deadline_master_ns=117241587845 first_submitted_master_ns=117231620449 last_submitted_master_ns=117231620449 worst_wait_started_master_ns=117231567365 worst_wait_completed_master_ns=117231567365 worst_submit_observed_master_ns=117231568531 wait_attempts=0 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=0 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=0 wait_call_total_ns=0 wait_call_max_ns=0 wait_before_call_count=0 wait_before_call_total_ns=0 wait_before_call_max_ns=0 wait_arm_count=0 wait_arm_total_ns=0 wait_arm_max_ns=0 wait_api_wall_count=0 wait_api_wall_total_ns=0 wait_api_wall_max_ns=0 wait_return_to_next_clock_count=0 wait_return_to_next_clock_total_ns=0 wait_return_to_next_clock_max_ns=0 wait_target_lateness_count=0 wait_target_lateness_total_ns=0 wait_target_lateness_max_ns=0
[2026-11-17 11:35:40.390] [info] [graph][runtime.node] datagram_batch_trace stage=active generation=1 first_sequence=106727 last_sequence=106766 maximum_submit_interval_ns=108502 maximum_submit_interval_sequence=106747 maximum_start_lateness_ns=200422 maximum_start_lateness_sequence=106737 first_release_master_ns=117208256673 first_deadline_master_ns=117241587845 last_release_master_ns=117208256673 last_deadline_master_ns=117241587845 first_submitted_master_ns=117231902496 last_submitted_master_ns=117241081710 worst_wait_started_master_ns=117235402270 worst_wait_completed_master_ns=117235787569 worst_submit_observed_master_ns=117235789902 wait_attempts=25 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=25 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=25 wait_call_total_ns=7155299 wait_call_max_ns=383549 wait_before_call_count=25 wait_before_call_total_ns=9334 wait_before_call_max_ns=584 wait_arm_count=0 wait_arm_total_ns=0 wait_arm_max_ns=0 wait_api_wall_count=0 wait_api_wall_total_ns=0 wait_api_wall_max_ns=0 wait_return_to_next_clock_count=25 wait_return_to_next_clock_total_ns=41711 wait_return_to_next_clock_max_ns=3500 wait_target_lateness_count=25 wait_target_lateness_total_ns=2593194 wait_target_lateness_max_ns=198089 worst_wait_metric=call_wall worst_wait_sequence=106737 worst_wait_target_master_ns=117235589480 worst_wait_sampled_master_ns=117235402270 worst_wait_sampled_steady_ns=87089064421981 worst_wait_timeout_ns=187210 worst_wait_entered_steady_ns=87089064422564 worst_wait_returned_steady_ns=87089064806113 worst_wait_arm_before_steady_ns=unavailable worst_wait_arm_after_steady_ns=unavailable worst_wait_api_before_steady_ns=unavailable worst_wait_api_returned_steady_ns=unavailable worst_wait_windows_due_100ns=unavailable worst_wait_windows_result=unavailable worst_wait_windows_error=unavailable worst_wait_windows_pid=unavailable worst_wait_windows_tid=unavailable worst_wait_windows_qpc_frequency_hz=unavailable worst_wait_next_master_ns=117235787569 worst_wait_next_steady_ns=87089064807280 worst_wait_outcome=deadline
[2026-11-17 11:35:40.390] [info] [graph][runtime.node] scheduled_datagram_sender stage=failed generation=1 service_scope=ifindex:2 execution_mode=1 committed_batches=11748 committed_datagrams=106751 committed_payload_bytes=132892188 would_block=0 writable_waits=0 deadline_misses=1 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=85 ordered_queue_maximum_wire_bytes=115260 pacing_reserved=106752 pacing_cancelled=0 pacing_submitted=106751 pacing_maximum_submit_lateness_ns=55579756 pacing_worst_late_global_sequence=34936 pacing_worst_success_sample_available=1 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=15 backlog_current_wire_bytes=19964 backlog_high_water_datagrams=85 backlog_high_water_wire_bytes=115260 backlog_max_residence_ns=50145265 last_materialized_sequence=106766 last_scheduled_sequence=106766 last_submitted_sequence=106751 last_committed_sequence=106751 submitted_service_generation=1 submitted_service_sequence=106751 submitted_service_completed_ns=117241081710 submitted_service_duration_ns=216960 submitted_service_available_ns=117241298670 submitted_scope_available_ns=117241298670 shared_scope_charged_datagrams=106751 shared_scope_charged_wire_bytes=135881216 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=1153 evidence_timestamp_tracked=0 evidence_timestamp_untracked=1153 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=554648 endpoint_1_target_effective_socket_bytes=277324 endpoint_1_api_requested_socket_bytes=138662 endpoint_1_effective_socket_bytes=277324 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=106726 endpoint_1_committed_payload_bytes=132890488 endpoint_2_target_effective_socket_bytes=277324 endpoint_2_api_requested_socket_bytes=138662 endpoint_2_effective_socket_bytes=277324 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=25 endpoint_2_committed_payload_bytes=1700
[CLI] runtime report: queued=406, peakQueued=1295, threads=102, processThreads=148, workers=95, workerProgress=311284, workerProcessCalls=1284657, workerWaits=973362, workerWakeups=962117, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=429, logicalProcessors=8, averageSystemMachineCpuPercent=38.916081, averageProcessMachineCpuPercent=22.177368, peakProcessMachineCpuPercent=40.000000, averageProcessSingleCoreCpuPercent=177.418946, peakProcessSingleCoreCpuPercent=320.000000, initialWorkingSetBytes=58720256, workingSetBytes=72081408, peakWorkingSetBytes=72081408, totalPushed=285769, totalPopped=285363, droppedBuffers=0, encodedPacketsPushed=36138, encodedPacketsPopped=36137, backpressureItems=147, graphPayloadCurrentBytes=2787836, graphPayloadHighWaterBytes=14303826, graphPayloadCurrentObjects=655, graphPayloadHighWaterObjects=668, graphPayloadReservations=156168, graphPayloadReleases=155513, graphPayloadPressureFailures=10, lastInputReceivedAtNs=87088699841950
[CLI] runtime report: queued=406, peakQueued=1295, threads=102, processThreads=148, workers=95, workerProgress=311991, workerProcessCalls=1288079, workerWaits=976076, workerWakeups=964807, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=430, logicalProcessors=8, averageSystemMachineCpuPercent=38.903098, averageProcessMachineCpuPercent=22.187566, peakProcessMachineCpuPercent=40.000000, averageProcessSingleCoreCpuPercent=177.500530, peakProcessSingleCoreCpuPercent=320.000000, initialWorkingSetBytes=58720256, workingSetBytes=72081408, peakWorkingSetBytes=72081408, totalPushed=286415, totalPopped=286009, droppedBuffers=0, encodedPacketsPushed=36222, encodedPacketsPopped=36221, backpressureItems=147, graphPayloadCurrentBytes=2317399, graphPayloadHighWaterBytes=14303826, graphPayloadCurrentObjects=656, graphPayloadHighWaterObjects=668, graphPayloadReservations=156514, graphPayloadReleases=155858, graphPayloadPressureFailures=10, lastInputReceivedAtNs=87088959657550
~~~

退出结论：四个源均完整120秒exit0，VLC exit0。CLI失败后挂起，线程栈主线程停在executor.abort→worker.join，两个decode worker停在VideoDecodeNode.submitPendingPacket→avcodec_send_packet→mpi_decode_get_frame→mpp_get_frame。与rk473同类，未修复。诊断后核对身份并kill -KILL 3209855，CLI_EXIT=137是清理退出，不能记自然退出。远端本轮所有PID查询为空。

~~~text
Thread 35 (Thread 0x7f1a7dbb40 (LWP 3210634) "mt-n23"):
#0  futex_abstimed_wait_cancelable (private=0, abstime=0x7f1a7da570, clockid=<optimized out>, expected=0, futex_word=0x3e308bb0) at ../sysdeps/nptl/futex-internal.h:320
#1  __pthread_cond_wait_common (abstime=0x7f1a7da570, clockid=<optimized out>, mutex=0x3e308b58, cond=0x3e308b88) at pthread_cond_wait.c:520
#2  __pthread_cond_timedwait (cond=0x3e308b88, mutex=0x3e308b58, abstime=0x7f1a7da570) at pthread_cond_wait.c:665
#3  0x0000007f9934c7ec in mpp_cond_timedwait () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#4  0x0000007f99350880 in mpp_list_wait_timed () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#5  0x0000007f992e917c in mpp_get_frame () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#6  0x0000007f992ec6fc in mpi_decode_get_frame () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#7  0x0000007f9a5c41e8 in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so.62
#8  0x0000007f9a5c538c in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so.62
#9  0x0000007f9a24bc24 in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so.62
#10 0x0000007f9a24c280 in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so.62
#11 0x0000007f9a24c4cc in avcodec_send_packet () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so.62
#12 0x0000000000b3b144 in media::ffmpeg::graph::VideoDecodeNode::submitPendingPacket(media::ffmpeg::graph::MediaGraphExecutionContext&) ()
#13 0x0000000000b3df94 in media::ffmpeg::graph::VideoDecodeNode::onProcess(media::ffmpeg::graph::MediaGraphExecutionContext&) ()
#14 0x00000000009d4aa0 in media::ffmpeg::graph::MediaNodeRuntime::process(media::ffmpeg::graph::MediaGraphExecutionContext&) ()
#15 0x00000000009d0dc8 in media::ffmpeg::graph::FFmpegNodeRuntime::processImpl(media::ffmpeg::graph::MediaGraphExecutionContext&) ()
#16 0x00000000009d1034 in media::ffmpeg::graph::FFmpegNodeRuntime::process(media::ffmpeg::graph::MediaGraphExecutionContext&) ()
#17 0x00000000008de368 in media::ffmpeg::graph::MediaGraphWorker::run() ()
#18 0x0000007f9986bb0c in std::execute_native_thread_routine (__p=0x3e3c2210) at /opt/mt-tools/src/gcc-12.4.0/libstdc++-v3/src/c++11/thread.cc:82
#19 0x0000007f999bf51c in start_thread (arg=0x7fd9fa223f) at pthread_create.c:477


Thread 34 (Thread 0x7f19fdab40 (LWP 3210635) "mt-n42"):
#0  futex_abstimed_wait_cancelable (private=0, abstime=0x7f19fd9570, clockid=<optimized out>, expected=0, futex_word=0x3e31ef34) at ../sysdeps/nptl/futex-internal.h:320
#1  __pthread_cond_wait_common (abstime=0x7f19fd9570, clockid=<optimized out>, mutex=0x3e31eed8, cond=0x3e31ef08) at pthread_cond_wait.c:520
#2  __pthread_cond_timedwait (cond=0x3e31ef08, mutex=0x3e31eed8, abstime=0x7f19fd9570) at pthread_cond_wait.c:665
#3  0x0000007f9934c7ec in mpp_cond_timedwait () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#4  0x0000007f99350880 in mpp_list_wait_timed () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#5  0x0000007f992e917c in mpp_get_frame () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#6  0x0000007f992ec6fc in mpi_decode_get_frame () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#7  0x0000007f9a5c41e8 in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so.62
#8  0x0000007f9a5c538c in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so.62
#9  0x0000007f9a24bc24 in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so.62
#10 0x0000007f9a24c280 in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so.62
#11 0x0000007f9a24c4cc in avcodec_send_packet () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so.62
#12 0x0000000000b3b144 in media::ffmpeg::graph::VideoDecodeNode::submitPendingPacket(media::ffmpeg::graph::MediaGraphExecutionContext&) ()
#13 0x0000000000b3df94 in media::ffmpeg::graph::VideoDecodeNode::onProcess(media::ffmpeg::graph::MediaGraphExecutionContext&) ()
#14 0x00000000009d4aa0 in media::ffmpeg::graph::MediaNodeRuntime::process(media::ffmpeg::graph::MediaGraphExecutionContext&) ()
#15 0x00000000009d0dc8 in media::ffmpeg::graph::FFmpegNodeRuntime::processImpl(media::ffmpeg::graph::MediaGraphExecutionContext&) ()
#16 0x00000000009d1034 in media::ffmpeg::graph::FFmpegNodeRuntime::process(media::ffmpeg::graph::MediaGraphExecutionContext&) ()
#17 0x00000000008de368 in media::ffmpeg::graph::MediaGraphWorker::run() ()
#18 0x0000007f9986bb0c in std::execute_native_thread_routine (__p=0x3e3c23b0) at /opt/mt-tools/src/gcc-12.4.0/libstdc++-v3/src/c++11/thread.cc:82
#19 0x0000007f999bf51c in start_thread (arg=0x7fd9fa223f) at pthread_create.c:477


Thread 1 (Thread 0x7f98bfc610 (LWP 3209855) "media_transcode"):
#0  __pthread_clockjoin_ex (threadid=545905294144, thread_return=0x0, clockid=0, abstime=<optimized out>, block=<optimized out>) at pthread_join_common.c:145
#1  0x0000007f9986bd80 in __gthread_join (__value_ptr=0x0, __threadid=<optimized out>) at /opt/mt-tools/build/gcc-12.4.0-native/aarch64-unknown-linux-gnu/libstdc++-v3/include/aarch64-unknown-linux-gnu/bits/gthr-default.h:669
#2  std::thread::join (this=0x3e3a87f8) at /opt/mt-tools/src/gcc-12.4.0/libstdc++-v3/src/c++11/thread.cc:112
#3  0x00000000008db464 in media::ffmpeg::graph::MediaGraphWorker::join() ()
#4  0x00000000008da274 in media::ffmpeg::graph::MediaGraphThreadedExecutor::abort(media::ffmpeg::graph::MediaGraphExecutionContext&, media::ffmpeg::graph::MediaGraphScheduler&) ()
#5  0x000000000087b558 in media::ffmpeg::graph::MediaGraphRuntimeLifecycleExecutor::synchronizeThreadedState(media::ffmpeg::graph::MediaGraphRuntime&) ()
#6  0x00000000007d2d38 in media::ffmpeg::graph::MediaGraphRuntime::synchronizeThreadedState() ()
#7  0x0000000000424fcc in media::ffmpeg::graph::MediaRealtimeVideoRunController::run(media::ffmpeg::graph::MediaRealtimeRtpTranscodeRequest const&, media::ffmpeg::graph::MediaRealtimeVideoRunPolicy const&, media::ffmpeg::graph::MediaRealtimeVideoRunControl&, media::ffmpeg::graph::MediaRealtimeVideoRunObserver const&)::{lambda()#1}::operator()() const ()
#8  0x0000000000427224 in media::ffmpeg::graph::MediaRealtimeVideoRunController::run(media::ffmpeg::graph::MediaRealtimeRtpTranscodeRequest const&, media::ffmpeg::graph::MediaRealtimeVideoRunPolicy const&, media::ffmpeg::graph::MediaRealtimeVideoRunControl&, media::ffmpeg::graph::MediaRealtimeVideoRunObserver const&) ()
#9  0x000000000041072c in (anonymous namespace)::runRealtimeVideoCli(int, char**) ()
#10 0x000000000040eeac in main ()
[Inferior 1 (process 3209855) detached]

~~~

清理清单：本机composition-rk482-native-2026-11-17-11h34m52s795.png、source2/3各log/out、vlc.log、rk482-cli.log、rk482-gdb.txt、rk482-run.sh、rk482-source.tar；远端acceptance/rk482内socket-monitor.log、udp-after.txt、output.sdp、cli-stack.txt、gdb-symbols.txt、source0.out、cli-error.log、monitor.log、result.txt、pids.txt、launcher.log、run.sh、cli.log、source1.out、source0.log、udp-before.txt、source1.log、cli-maps.txt；另out/acceptance/rk482-build.log、rk482-source.tar、rk482-run.sh。保留用户源、正式CLI及回退。

清理核验：本机10项及远端21项逐项删除，两个本轮目录查询无残留；本机与远端精确媒体PID查询为空。
