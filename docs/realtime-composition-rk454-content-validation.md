# rk454：先等待输入的RKMPP内容联合取证

状态：FAIL。发送序号 45337 发生不可变期限冲突，CLI 失败后未自行退出，已归档并清理本轮 27 个临时文件及进程；内容联合验收未完成。复用rk453全部源规格/输出规格、隔离依赖及源码ce71265d；唯一诊断变量为先启动目标CLI及目标两路源，再启动Windows选中标记源和HEVC源，争取完整长窗口。所有源仍原120秒、1280×720/30fps，选中标记H264 CBR8200kbps，HEVC CBR8000kbps，输出H264 VBR8–12Mbps/AAC192kbps TS/RTP。Windows FFmpeg不改。

## 实际脚本与启动命令

```bash
#!/usr/bin/env bash
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
export PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin:$PATH
export LD_LIBRARY_PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib:/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib:/opt/mt-tools/gcc-12.4-native/lib64
set -u
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk454
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk454 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://192.168.130.229:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 192.168.96.122 --rtp-port 62024 --sdp /home/tang/MediaTranscode-composition-09093919/acceptance/rk454/output.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
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

bash acceptance/rk454/run.sh >acceptance/rk454/launcher.log 2>&1 &
```

```powershell
$p=Start-Process -FilePath 'D:/Wireshark/dumpcap.exe' -ArgumentList '-i 7 -f "udp and (dst port 61020 or dst port 61021 or dst port 61022 or dst port 61023 or dst port 62024 or dst port 62025)" -a duration:420 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk454-output.pcapng' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk454-capture.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk454-capture.log' -PassThru; Write-Output "CAPTURE_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "CAPTURE_EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -vf "drawtext=fontfile=''C\:/Windows/Fonts/consola.ttf'':text=''frame=%{frame_num} pts=%{pts}'':fontsize=48:fontcolor=white:box=1:boxcolor=black:x=16:y=16" -c:v h264_nvenc -rc cbr -b:v 8200k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61020?rtcpport=61021&pkt_size=1200&localaddr=192.168.96.122" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://192.168.130.229:61022?rtcpport=61023&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk454-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk454-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE2_EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61050?rtcpport=61051&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk454-source3.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk454-source3.log' -PassThru; Write-Output "SOURCE3_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE3_EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk454-vlc.log --extraintf=rc --rc-host=127.0.0.1:42120 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk454-native- --snapshot-format=png rtp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "VLC_EXIT=$($p.ExitCode)"
```

PID：capture24712；Windows source2/3=16788/14192，VLC41848；远端wrapper2807669，source0/1=2807674/2807675，CLI2807676，monitor2807678。

## 本轮结果：FAIL（未完成内容时差验收）

Windows FFmpeg 保持不变：Windows 既有验证正常，本轮证据仅表明 RKMPP 运行中共享发送节点出现期限冲突，不能归因 Windows 依赖。

序号 45337 的 not_before 比 not_after 晚 96359 ns；规划时已提交序号 45258，失败时实际提交已推进至 45336。不能把两个不同服务进度的快照差异直接当成 planner 错误，尚需核对期间的服务时间和背压。有效期限 66666668 ns，未放宽任何预算。

源 0/1/2/3 均输出 3600 帧，完整 120 秒；抓包 159735 包、接口丢包 0。已有 VLC 原生快照显示四格画面，但本轮失败，不能记为内容时差验收通过。源结束后 CLI 仍存活超过 337 秒，未产生 result.txt；45 个存活线程的 wchan 均为 futex_wait_queue_me。目标机未发现 gdb，未取得用户态线程栈，不能据此确定具体锁环。后续精确 PID 清理属于强制中止，不是自然退出。

目标 stderr 保留诊断：client 12 driver is not ready；滤镜 range 从 unknown 变为 tv 的 Changing video frame properties 警告；编码器 readback 为 VBR 8000000/12000000、固定 30 fps、GOP 60。没有证据证明这些警告导致本次发送期限失败。

### 原始因果记录

~~~text
[2026-11-16 18:30:59.139] [info] [graph][runtime.node] datagram_reservation_trace stage=reservation_failed generation=1 endpoint=1 sequence=45337 release_master_ns=67523291451 deadline_master_ns=67589958119 publication_role=media original_protocol_deadline_master_ns=67589958119 prefix_completion_limit_master_ns=67589958119 materialized_master_ns=67546399160 planning_generation=1 planning_cursor=4977 planning_cursor_revision=2 planning_publication=5005 planning_protocol_begin=85 planning_protocol_end=170 planning_committed_sequence=45258 planning_observed_master_ns=67545345641 planning_projected_available_master_ns=67545449321 mux_publication_ready_master_ns=67545488560 planning_submitted_service=available planning_submitted_generation=1 planning_submitted_sequence=45258 planning_submit_completed_master_ns=67544904050 planning_service_duration_ns=216960 planning_scope_available_master_ns=67545121010 activation_started_master_ns=67547662974 activation_completed_master_ns=67547670266 permit_started_master_ns=67589633975 permit_completed_master_ns=67589634559 snapshot_started_master_ns=67589635434 snapshot_sampled_master_ns=67589636017 snapshot_completed_master_ns=67589636892 reservation_completed_master_ns=67589676851 wait_started_master_ns=unavailable wait_completed_master_ns=unavailable submit_observed_master_ns=unavailable wait_attempts=0 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=0 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=0 wait_call_total_ns=0 wait_call_max_ns=0 wait_before_call_count=0 wait_before_call_total_ns=0 wait_before_call_max_ns=0 wait_arm_count=0 wait_arm_total_ns=0 wait_arm_max_ns=0 wait_api_wall_count=0 wait_api_wall_total_ns=0 wait_api_wall_max_ns=0 wait_return_to_next_clock_count=0 wait_return_to_next_clock_total_ns=0 wait_return_to_next_clock_max_ns=0 wait_target_lateness_count=0 wait_target_lateness_total_ns=0 wait_target_lateness_max_ns=0 original_error="IoFailure: Datagram GBRA reservation misses its immutable completion deadline sequence=45337 now_ns=67589636017 release_ns=67523291451 deadline_ns=67589958119 not_before_ns=67589837518 not_after_ns=67589741159 duration_ns=216960 rate=6250000 queue_wire_bytes=12204 average_queue_residence_ns=43236857 previous_submitted_sequence=45336 last_observed_ns=67589620558 theoretical_arrival_ns=67589837518"
[2026-11-16 18:30:59.144] [info] [graph][runtime.node] datagram_successful_submit_trace stage=worst_success generation=1 endpoint=1 sequence=20370 pacing_lateness_ns=39274891 matches_pacing_worst=1 enqueued_master_ns=unavailable submit_interval_scope=pre_transport_checks_and_transport socket_only_ns=unavailable release_master_ns=42556625207 deadline_master_ns=42656624785 publication_role=media original_protocol_deadline_master_ns=42656624785 prefix_completion_limit_master_ns=42656624785 materialized_master_ns=42596259135 planning_generation=1 planning_cursor=2283 planning_cursor_revision=1 planning_publication=2293 planning_protocol_begin=0 planning_protocol_end=27 planning_committed_sequence=20369 planning_observed_master_ns=42596088215 planning_projected_available_master_ns=42596088215 mux_publication_ready_master_ns=42596176592 planning_submitted_service=available planning_submitted_generation=1 planning_submitted_sequence=20369 planning_submit_completed_master_ns=42557458481 planning_service_duration_ns=36480 planning_scope_available_master_ns=42557494961 reserved_at_master_ns=42596755269 not_before_master_ns=42557494961 not_after_master_ns=42656407825 service_duration_ns=216960 wire_bytes_per_second=6250000 submit_started_master_ns=42596769852 submit_completed_master_ns=42596871354 activation_started_master_ns=42596741269 activation_completed_master_ns=42596748560 permit_started_master_ns=42596751769 permit_completed_master_ns=42596752935 snapshot_started_master_ns=42596754685 snapshot_sampled_master_ns=42596755269 snapshot_completed_master_ns=42596756727 reservation_completed_master_ns=42596764894 wait_started_master_ns=42596768102 wait_completed_master_ns=42596768102 submit_observed_master_ns=42596769852 wait_attempts=0 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=0 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=0 wait_call_total_ns=0 wait_call_max_ns=0 wait_before_call_count=0 wait_before_call_total_ns=0 wait_before_call_max_ns=0 wait_arm_count=0 wait_arm_total_ns=0 wait_arm_max_ns=0 wait_api_wall_count=0 wait_api_wall_total_ns=0 wait_api_wall_max_ns=0 wait_return_to_next_clock_count=0 wait_return_to_next_clock_total_ns=0 wait_return_to_next_clock_max_ns=0 wait_target_lateness_count=0 wait_target_lateness_total_ns=0 wait_target_lateness_max_ns=0
[2026-11-16 18:30:59.144] [info] [graph][runtime.node] scheduled_datagram_sender stage=failed generation=1 service_scope=ifindex:2 execution_mode=1 committed_batches=5004 committed_datagrams=45336 committed_payload_bytes=56428776 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=85 ordered_queue_maximum_wire_bytes=115260 pacing_reserved=45336 pacing_cancelled=0 pacing_submitted=45336 pacing_maximum_submit_lateness_ns=39274891 pacing_worst_late_global_sequence=20370 pacing_worst_success_sample_available=1 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=9 backlog_current_wire_bytes=12204 backlog_high_water_datagrams=85 backlog_high_water_wire_bytes=115260 backlog_max_residence_ns=59352816 last_materialized_sequence=45345 last_scheduled_sequence=45345 last_submitted_sequence=45336 last_committed_sequence=45336 submitted_service_generation=1 submitted_service_sequence=45336 submitted_service_completed_ns=67589620558 submitted_service_duration_ns=216960 submitted_service_available_ns=67589837518 submitted_scope_available_ns=67589837518 shared_scope_charged_datagrams=45336 shared_scope_charged_wire_bytes=57698184 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=1153 evidence_timestamp_tracked=0 evidence_timestamp_untracked=1153 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=554648 endpoint_1_target_effective_socket_bytes=277324 endpoint_1_api_requested_socket_bytes=138662 endpoint_1_effective_socket_bytes=277324 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=45325 endpoint_1_committed_payload_bytes=56428028 endpoint_2_target_effective_socket_bytes=277324 endpoint_2_api_requested_socket_bytes=138662 endpoint_2_effective_socket_bytes=277324 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=11 endpoint_2_committed_payload_bytes=748
~~~

### 清理记录

CLI 精确 PID 2807676 强制清理后退出 137；source0/1 退出 0，wrapper/monitor 随后退出，五个目标 PID 均不存在。本机 source2/3 与抓包已结束，VLC41848 和旧 SCP10296 核对身份后清理，五个本机 PID 均不存在。

本机逐项清理清单：composition-rk454-capture.log, composition-rk454-capture.out, composition-rk454-cli.log, composition-rk454-native-2026-11-16-18h31m14s446.png, composition-rk454-output.pcapng, composition-rk454-source2.log, composition-rk454-source2.out, composition-rk454-source3.log, composition-rk454-source3.out, composition-rk454-vlc.log。
远端 rk454 逐项清理清单：cli-error.log, cli-maps.txt, cli.log, launcher.log, monitor.log, output.sdp, pids.txt, result.txt, run.sh, socket-monitor.log, source0.log, source0.out, source1.log, source1.out, thread-wchan.txt, udp-after.txt, udp-before.txt。保留原始 120 秒源及所有构建和回退目录。
