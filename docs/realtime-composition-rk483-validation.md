# rk483：Linux发送等待事实诊断（进行中）

基线2fe70532，新增3文件仅诊断实际等待线程TID、scheduler policy、timer slack及条件变量API起止，不设置策略、不放宽期限、不修改FFmpeg。两位独立审查者source_drain_review_a/b明确SOURCE PASS。

rk482窗口72.215微秒、等待目标超调95.130微秒、其后检查5.250微秒，最终晚28.165微秒；仅证明等待返回时已过截止，不能单独归因timer slack。Linux官方PR_SET_TIMERSLACK说明条件变量/futex受slack影响，按线程继承，零值恢复默认。实际shell读取50000ns仍不是sender线程证据：https://man7.org/linux/man-pages/man2/PR_SET_TIMERSLACK.2const.html 。

测量限制：新增3个系统调用位于相对wait_for之前，会增加总目标偏差；API区间包括重新获取mutex，不能当纯内核睡眠。Windows等待实现不变，但诊断结构变大，仍保留最终共享链路回归。

RKMPP源码核验：现用ffmpeg-lineage HEAD92aec19cedcfc55cd5ef0a8b2fff1ca998bcb696，rkmppdec.c输入满后取帧100ms仍EAGAIN则同调用继续while循环；上游fork同结构，不能直接指认本地补丁引入：https://github.com/nyanmisaka/ffmpeg-rockchip/blob/master/libavcodec/rkmppdec.c 。FFmpeg send/receive合同不允许双端EAGAIN，因此不能简单改return EAGAIN或外层超时后并发flush/free：https://github.com/FFmpeg/FFmpeg/blob/master/libavcodec/avcodec.h 。本轮尚未修改依赖或确认其可取消接口。

暂停恢复后重连成功，无残留CLI/build。8核增量构建PID3275117，回退out/rollback/rk483含3个原源码和CLI。临时产物清单：本机/远端out/acceptance/rk483-source.tar、rk483-run.sh，远端rk483-build.log；实流尚未开始。

构建10/10退出0，CLI SHA256 2bb0f837529f6c790b98cedb8184e9cac8916e4dd0e5365548fe33ddbd794327。

本轮源码06916131。远端launcher3285043、源0=3285047、源1=3285048、CLI3285049、monitor3285051；本机VLC36572、源2=38632、源3=40388。VLC原生快照composition-rk483-native-2026-11-17-11h52m49s282.png已查看四格，左下frame1129/pts37.633333。

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
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk483
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk483 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://192.168.130.229:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 192.168.96.122 --rtp-port 62024 --sdp /home/tang/MediaTranscode-composition-09093919/acceptance/rk483/output.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
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
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -vf "drawtext=fontfile=''C\:/Windows/Fonts/consola.ttf'':text=''frame=%{frame_num} pts=%{pts}'':fontsize=48:fontcolor=white:box=1:boxcolor=black:x=16:y=16" -c:v h264_nvenc -rc cbr -b:v 8200k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61020?rtcpport=61021&pkt_size=1200&localaddr=192.168.96.122" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://192.168.130.229:61022?rtcpport=61023&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk483-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk483-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE2_EXIT=$($p.ExitCode)"; exit $p.ExitCode
~~~

~~~powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61050?rtcpport=61051&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk483-source3.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk483-source3.log' -PassThru; Write-Output "SOURCE3_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE3_EXIT=$($p.ExitCode)"; exit $p.ExitCode
~~~

~~~powershell
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk483-vlc.log --extraintf=rc --rc-host=127.0.0.1:42120 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk483-native- --snapshot-format=png rtp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "VLC_EXIT=$($p.ExitCode)"
~~~

远端启动：bash -n acceptance/rk483/run.sh && bash acceptance/rk483/run.sh >acceptance/rk483/launcher.log 2>&1 &。


## 结果：FAIL，首个关闭控制消息被通道拒绝

四源120秒均exit0；发送持续到source master约121秒，deadline_misses=0，但不能据单次未复现判定此前等待失败修复。实际sender TID3286147、SCHED_OTHER优先级0、/proc/3286147/timerslack_ns=50000，与新增内部诊断一致。外部187样本CPU最大156%单核口径、RSS最大88892KiB。

首个prepared_rtp_input_sealed后Raw8报output channel aborted；源码调查：EndGeneration既非isEof也非isFlush，MediaControlBuffer未覆盖payloadFootprintBytes，继承nullopt；MediaChannel硬字节准入将缺计费事实返回Aborted，不是先有线程abort。统一控制消息payload契约修复进行中。两个decode worker再次阻塞sendPacket→mpi_decode_get_frame→mpp_get_frame，main等worker.join，未自然退出；诊断后精确PID3285049强制清理，CLI_EXIT=137不能记为通过。全黑/重入未覆盖。

~~~text
[2026-11-17 11:53:25.613] [info] [graph][runtime.node] prepared_rtp_source_closing generation=1 video_deadline_ns=88154400341957 audio_deadline_ns=none
[2026-11-17 11:53:25.619] [info] [graph][runtime.node] prepared_rtp_source_closing generation=1 video_deadline_ns=88154455768520 audio_deadline_ns=none
[2026-11-17 11:53:27.611] [info] [graph][runtime.node] prepared_rtp_input_sealed stream=1 generation=1 ingress_generation=0 frontier=3625
[2026-11-17 11:53:27.612] [info] [graph][runtime.node] process.failed node=8 name=RawRtpInputNode kind=RawRtpInput inputs=[] outputs=[channel=9 edge=9 stream=Video edge_kind=InputPacket payload=Packet queued=0; channel=20 edge=20 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0] error=InternalError: FFmpegNodeRuntime output channel aborted
[2026-11-17 11:53:27.612] [info] [graph][runtime.node] worker.failed node=8 error=InternalError: FFmpegNodeRuntime output channel aborted
[2026-11-17 11:53:27.612] [info] [graph][runtime.node] process.failed node=102 name=MediaScheduledDatagramSenderNode kind=ScheduledDatagramSender inputs=[channel=134 edge=134 stream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=146 edge=146 stream=Metadata edge_kind=Unknown payload=Unknown queued=0] outputs=[] error=Cancelled: scheduled datagram sender reservation wait was interrupted
[2026-11-17 11:53:27.612] [info] [graph][runtime.node] datagram_batch_trace stage=last_completed generation=1 first_sequence=107876 last_sequence=107876 maximum_submit_interval_ns=80501 maximum_submit_interval_sequence=107876 maximum_start_lateness_ns=68100 maximum_start_lateness_sequence=107876 first_release_master_ns=121460695105 first_deadline_master_ns=121541824581 last_release_master_ns=121460695105 last_deadline_master_ns=121541824581 first_submitted_master_ns=121460843706 last_submitted_master_ns=121460843706 worst_wait_started_master_ns=121459213550 worst_wait_completed_master_ns=121460759705 worst_submit_observed_master_ns=121460763205 wait_attempts=1 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=1 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=1 wait_call_total_ns=1544988 wait_call_max_ns=1544988 wait_before_call_count=1 wait_before_call_total_ns=292 wait_before_call_max_ns=292 wait_arm_count=0 wait_arm_total_ns=0 wait_arm_max_ns=0 wait_api_wall_count=1 wait_api_wall_total_ns=1542363 wait_api_wall_max_ns=1542363 wait_return_to_next_clock_count=1 wait_return_to_next_clock_total_ns=1166 wait_return_to_next_clock_max_ns=1166 wait_target_lateness_count=1 wait_target_lateness_total_ns=64600 wait_target_lateness_max_ns=64600 worst_wait_metric=call_wall worst_wait_sequence=107876 worst_wait_target_master_ns=121460695105 worst_wait_sampled_master_ns=121459213550 worst_wait_sampled_steady_ns=88156276290600 worst_wait_timeout_ns=1481555 worst_wait_entered_steady_ns=88156276290892 worst_wait_returned_steady_ns=88156277835880 worst_wait_arm_before_steady_ns=unavailable worst_wait_arm_after_steady_ns=unavailable worst_wait_api_before_steady_ns=88156276292642 worst_wait_api_returned_steady_ns=88156277835005 worst_wait_windows_due_100ns=unavailable worst_wait_windows_result=unavailable worst_wait_windows_error=unavailable worst_wait_windows_pid=unavailable worst_wait_windows_tid=unavailable worst_wait_windows_qpc_frequency_hz=unavailable worst_wait_linux_tid=3286147 worst_wait_linux_scheduler_policy=0 worst_wait_linux_timer_slack_ns=50000 worst_wait_linux_timer_slack_error=unavailable worst_wait_next_master_ns=121460759705 worst_wait_next_steady_ns=88156277837046 worst_wait_outcome=deadline
[2026-11-17 11:53:27.614] [info] [graph][runtime.node] datagram_batch_trace stage=active generation=1 first_sequence=107877 last_sequence=107926 maximum_submit_interval_ns=337465 maximum_submit_interval_sequence=107896 maximum_start_lateness_ns=257007 maximum_start_lateness_sequence=107889 first_release_master_ns=121475207577 first_deadline_master_ns=121541824581 last_release_master_ns=121475207577 last_deadline_master_ns=121541824581 first_submitted_master_ns=121475408361 last_submitted_master_ns=121482044488 worst_wait_started_master_ns=121478950137 worst_wait_completed_master_ns=121479412146 worst_submit_observed_master_ns=121479413312 wait_attempts=21 wait_incomplete=1 wait_notified_count=0 wait_deadline_count=20 wait_interrupted_count=1 wait_unavailable_count=0 wait_call_count=21 wait_call_total_ns=14673450 wait_call_max_ns=9126134 wait_before_call_count=21 wait_before_call_total_ns=6123 wait_before_call_max_ns=875 wait_arm_count=0 wait_arm_total_ns=0 wait_arm_max_ns=0 wait_api_wall_count=21 wait_api_wall_total_ns=14605489 wait_api_wall_max_ns=9120883 wait_return_to_next_clock_count=20 wait_return_to_next_clock_total_ns=18956 wait_return_to_next_clock_max_ns=3208 wait_target_lateness_count=20 wait_target_lateness_total_ns=1666692 wait_target_lateness_max_ns=255841 worst_wait_metric=call_wall worst_wait_sequence=107877 worst_wait_target_master_ns=121475207577 worst_wait_sampled_master_ns=121466164100 worst_wait_sampled_steady_ns=88156283241442 worst_wait_timeout_ns=9043477 worst_wait_entered_steady_ns=88156283241733 worst_wait_returned_steady_ns=88156292367867 worst_wait_arm_before_steady_ns=unavailable worst_wait_arm_after_steady_ns=unavailable worst_wait_api_before_steady_ns=88156283245817 worst_wait_api_returned_steady_ns=88156292366700 worst_wait_windows_due_100ns=unavailable worst_wait_windows_result=unavailable worst_wait_windows_error=unavailable worst_wait_windows_pid=unavailable worst_wait_windows_tid=unavailable worst_wait_windows_qpc_frequency_hz=unavailable worst_wait_linux_tid=3286147 worst_wait_linux_scheduler_policy=0 worst_wait_linux_timer_slack_ns=50000 worst_wait_linux_timer_slack_error=unavailable worst_wait_next_master_ns=121475293733 worst_wait_next_steady_ns=88156292371075 worst_wait_outcome=deadline
[2026-11-17 11:53:27.614] [info] [graph][runtime.node] scheduled_datagram_sender stage=failed generation=1 service_scope=ifindex:2 execution_mode=1 committed_batches=11829 committed_datagrams=107896 committed_payload_bytes=134340424 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=85 ordered_queue_maximum_wire_bytes=115260 pacing_reserved=107897 pacing_cancelled=0 pacing_submitted=107896 pacing_maximum_submit_lateness_ns=54210816 pacing_worst_late_global_sequence=84293 pacing_worst_success_sample_available=1 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=30 backlog_current_wire_bytes=40304 backlog_high_water_datagrams=85 backlog_high_water_wire_bytes=115260 backlog_max_residence_ns=49061529 last_materialized_sequence=107926 last_scheduled_sequence=107926 last_submitted_sequence=107896 last_committed_sequence=107896 submitted_service_generation=1 submitted_service_sequence=107896 submitted_service_completed_ns=121482044488 submitted_service_duration_ns=216960 submitted_service_available_ns=121482261448 submitted_scope_available_ns=121482261448 shared_scope_charged_datagrams=107896 shared_scope_charged_wire_bytes=137361512 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=1153 evidence_timestamp_tracked=0 evidence_timestamp_untracked=1153 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=554648 endpoint_1_target_effective_socket_bytes=277324 endpoint_1_api_requested_socket_bytes=138662 endpoint_1_effective_socket_bytes=277324 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=107870 endpoint_1_committed_payload_bytes=134338656 endpoint_2_target_effective_socket_bytes=277324 endpoint_2_api_requested_socket_bytes=138662 endpoint_2_effective_socket_bytes=277324 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=26 endpoint_2_committed_payload_bytes=1768
[2026-11-17 11:53:21.976] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4503 source_master_ns=117312873676 raw_phase_ns=9071 filtered_phase_ns=9070 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=256001 effective_output_sample_index=4864019
Thread 35 (Thread 0x7f057d9b40 (LWP 3286093) "mt-n23"):
#0  futex_abstimed_wait_cancelable (private=0, abstime=0x7f057d8570, clockid=<optimized out>, expected=0, futex_word=0x2f4c1f70) at ../sysdeps/nptl/futex-internal.h:320
#1  __pthread_cond_wait_common (abstime=0x7f057d8570, clockid=<optimized out>, mutex=0x2f4c1f18, cond=0x2f4c1f48) at pthread_cond_wait.c:520
#2  __pthread_cond_timedwait (cond=0x2f4c1f48, mutex=0x2f4c1f18, abstime=0x7f057d8570) at pthread_cond_wait.c:665
#3  0x0000007f889bd7ec in mpp_cond_timedwait () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#4  0x0000007f889c1880 in mpp_list_wait_timed () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#5  0x0000007f8895a17c in mpp_get_frame () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#6  0x0000007f8895d6fc in mpi_decode_get_frame () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#7  0x0000007f89c351e8 in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so.62
#8  0x0000007f89c3638c in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so.62
#9  0x0000007f898bcc24 in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so.62
#10 0x0000007f898bd280 in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so.62
#11 0x0000007f898bd4cc in avcodec_send_packet () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so.62
#12 0x0000000000b3b1b4 in media::ffmpeg::graph::VideoDecodeNode::submitPendingPacket(media::ffmpeg::graph::MediaGraphExecutionContext&) ()
#13 0x0000000000b3e004 in media::ffmpeg::graph::VideoDecodeNode::onProcess(media::ffmpeg::graph::MediaGraphExecutionContext&) ()
#14 0x00000000009d4b10 in media::ffmpeg::graph::MediaNodeRuntime::process(media::ffmpeg::graph::MediaGraphExecutionContext&) ()
#15 0x00000000009d0e38 in media::ffmpeg::graph::FFmpegNodeRuntime::processImpl(media::ffmpeg::graph::MediaGraphExecutionContext&) ()
#16 0x00000000009d10a4 in media::ffmpeg::graph::FFmpegNodeRuntime::process(media::ffmpeg::graph::MediaGraphExecutionContext&) ()
#17 0x00000000008de318 in media::ffmpeg::graph::MediaGraphWorker::run() ()
#18 0x0000007f88edcb0c in std::execute_native_thread_routine (__p=0x2fb284c0) at /opt/mt-tools/src/gcc-12.4.0/libstdc++-v3/src/c++11/thread.cc:82
#19 0x0000007f8903051c in start_thread (arg=0x7fed47352f) at pthread_create.c:477


Thread 34 (Thread 0x7f04fd8b40 (LWP 3286094) "mt-n42"):
#0  futex_abstimed_wait_cancelable (private=0, abstime=0x7f04fd7570, clockid=<optimized out>, expected=0, futex_word=0x2f47f9b4) at ../sysdeps/nptl/futex-internal.h:320
#1  __pthread_cond_wait_common (abstime=0x7f04fd7570, clockid=<optimized out>, mutex=0x2f47f958, cond=0x2f47f988) at pthread_cond_wait.c:520
#2  __pthread_cond_timedwait (cond=0x2f47f988, mutex=0x2f47f958, abstime=0x7f04fd7570) at pthread_cond_wait.c:665
#3  0x0000007f889bd7ec in mpp_cond_timedwait () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#4  0x0000007f889c1880 in mpp_list_wait_timed () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#5  0x0000007f8895a17c in mpp_get_frame () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#6  0x0000007f8895d6fc in mpi_decode_get_frame () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib/librockchip_mpp.so.1
#7  0x0000007f89c351e8 in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so.62
#8  0x0000007f89c3638c in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so.62
#9  0x0000007f898bcc24 in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so.62
#10 0x0000007f898bd280 in ?? () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so.62
#11 0x0000007f898bd4cc in avcodec_send_packet () from /home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib/libavcodec.so.62
#12 0x0000000000b3b1b4 in media::ffmpeg::graph::VideoDecodeNode::submitPendingPacket(media::ffmpeg::graph::MediaGraphExecutionContext&) ()
#13 0x0000000000b3e004 in media::ffmpeg::graph::VideoDecodeNode::onProcess(media::ffmpeg::graph::MediaGraphExecutionContext&) ()
#14 0x00000000009d4b10 in media::ffmpeg::graph::MediaNodeRuntime::process(media::ffmpeg::graph::MediaGraphExecutionContext&) ()
#15 0x00000000009d0e38 in media::ffmpeg::graph::FFmpegNodeRuntime::processImpl(media::ffmpeg::graph::MediaGraphExecutionContext&) ()
#16 0x00000000009d10a4 in media::ffmpeg::graph::FFmpegNodeRuntime::process(media::ffmpeg::graph::MediaGraphExecutionContext&) ()
#17 0x00000000008de318 in media::ffmpeg::graph::MediaGraphWorker::run() ()
#18 0x0000007f88edcb0c in std::execute_native_thread_routine (__p=0x2fb28660) at /opt/mt-tools/src/gcc-12.4.0/libstdc++-v3/src/c++11/thread.cc:82
#19 0x0000007f8903051c in start_thread (arg=0x7fed47352f) at pthread_create.c:477


Thread 1 (Thread 0x7f8826d610 (LWP 3285049) "media_transcode"):
#0  __pthread_clockjoin_ex (threadid=545552964416, thread_return=0x0, clockid=0, abstime=<optimized out>, block=<optimized out>) at pthread_join_common.c:145
#1  0x0000007f88edcd80 in __gthread_join (__value_ptr=0x0, __threadid=<optimized out>) at /opt/mt-tools/build/gcc-12.4.0-native/aarch64-unknown-linux-gnu/libstdc++-v3/include/aarch64-unknown-linux-gnu/bits/gthr-default.h:669
#2  std::thread::join (this=0x2f6df7b8) at /opt/mt-tools/src/gcc-12.4.0/libstdc++-v3/src/c++11/thread.cc:112
#3  0x00000000008db414 in media::ffmpeg::graph::MediaGraphWorker::join() ()
#4  0x00000000008da224 in media::ffmpeg::graph::MediaGraphThreadedExecutor::abort(media::ffmpeg::graph::MediaGraphExecutionContext&, media::ffmpeg::graph::MediaGraphScheduler&) ()
#5  0x000000000087b508 in media::ffmpeg::graph::MediaGraphRuntimeLifecycleExecutor::synchronizeThreadedState(media::ffmpeg::graph::MediaGraphRuntime&) ()
#6  0x00000000007d2ce8 in media::ffmpeg::graph::MediaGraphRuntime::synchronizeThreadedState() ()
#7  0x000000000042502c in media::ffmpeg::graph::MediaRealtimeVideoRunController::run(media::ffmpeg::graph::MediaRealtimeRtpTranscodeRequest const&, media::ffmpeg::graph::MediaRealtimeVideoRunPolicy const&, media::ffmpeg::graph::MediaRealtimeVideoRunControl&, media::ffmpeg::graph::MediaRealtimeVideoRunObserver const&)::{lambda()#1}::operator()() const ()
#8  0x0000000000427284 in media::ffmpeg::graph::MediaRealtimeVideoRunController::run(media::ffmpeg::graph::MediaRealtimeRtpTranscodeRequest const&, media::ffmpeg::graph::MediaRealtimeVideoRunPolicy const&, media::ffmpeg::graph::MediaRealtimeVideoRunControl&, media::ffmpeg::graph::MediaRealtimeVideoRunObserver const&) ()
#9  0x000000000041078c in (anonymous namespace)::runRealtimeVideoCli(int, char**) ()
#10 0x000000000040ef0c in main ()
[Inferior 1 (process 3285049) detached]

~~~

清理清单：本机composition-rk483-native-2026-11-17-11h52m49s282.png、composition-rk483-source2/3各log/out、composition-rk483-vlc.log、rk483-cli.log、rk483-gdb.txt、rk483-run.sh、rk483-source.tar；远端acceptance/rk483内socket-monitor.log、udp-after.txt、output.sdp、gdb-symbols.txt、source0.out、cli-error.log、monitor.log、result.txt、pids.txt、launcher.log、run.sh、cli.log、source1.out、source0.log、udp-before.txt、source1.log、cli-maps.txt；另out/acceptance/rk483-build.log、rk483-source.tar、rk483-run.sh。保留源、正式CLI与回退。

清理核验：本机10项及远端20项已逐项删除，两端本轮文件查询及精确PID查询均为空。
