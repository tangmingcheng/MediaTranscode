# rk520：四源两代音频联合取证

状态：本轮FAIL。首代音频尾部存在实际清理，重入阶段发送期限失败；CLI自主退出1，无需强制清理CLI。源码d4d2bae1；实时CLI SHA256=f011a0549227ddec06a31973a2c9f3b10cad7df13a41aea91b02db6b4c887061。沿用rk519已构建候选库，无生产源码、Windows FFmpeg或打包变更。

## 实际命令

首包前目标机确认tcpdump listening后启动媒体。目标eth0的输入与输出协议内容用于对照，不声称NIC实际出线服务已验证。

```bash
/usr/sbin/tcpdump -i eth0 -s 0 -B 4096 -U -w acceptance/rk520/joint.pcap 'udp and (dst port 61020 or dst port 61021 or dst port 61022 or dst port 61023 or dst port 62024 or dst port 62025)' >acceptance/rk520/capture.out 2>acceptance/rk520/capture.log &
#!/usr/bin/env bash
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
export PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg-cancel-20261118/bin:$PATH
export LD_LIBRARY_PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg-cancel-20261118/lib:/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp-cancel-eos-20261118/lib:/opt/mt-tools/gcc-12.4-native/lib64
set -u
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk520
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg-cancel-20261118/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg-cancel-20261118/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk520 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://192.168.130.229:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 192.168.96.122 --rtp-port 62024 --sdp /home/tang/MediaTranscode-composition-09093919/acceptance/rk520/output.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
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
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -vf "drawtext=fontfile=''C\:/Windows/Fonts/consola.ttf'':text=''frame=%{frame_num} pts=%{pts}'':fontsize=48:fontcolor=white:box=1:boxcolor=black:x=16:y=16" -c:v h264_nvenc -rc cbr -b:v 8200k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61020?rtcpport=61021&pkt_size=1200&localaddr=192.168.96.122" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://192.168.130.229:61022?rtcpport=61023&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk520-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk520-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE2_EXIT=$($p.ExitCode)"; exit $p.ExitCode

$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61050?rtcpport=61051&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk520-source3.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk520-source3.log' -PassThru; Write-Output "SOURCE3_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE3_EXIT=$($p.ExitCode)"; exit $p.ExitCode

$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk520-vlc.log --extraintf=rc --rc-host=127.0.0.1:42120 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk520-native- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk520-received.ts}}" --sout-keep --video-filter=scene --scene-format=png --scene-ratio=300 --scene-prefix=composition-rk520-frame- --scene-path=D:/Code/MyCode/MediaTranscode/out/acceptance rtp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"
```

远程脚本通过bash -n后由nohup运行，runner3059481；capture3057346；初始源3059486/3059487，CLI3059488，monitor3059490；Windows初始源9768/16156、VLC15672。初始Windows两源exit0。原生快照00301四格、03901全黑。同一CLI四路重入命令保持不变，仅sourceN日志改为reentryN，PID3072148/3072152/14968/30600；非同步起播。重入未完成，详见后文失败证据。

参考源ffprobe首包PTS=-1024、Skip Samples=1024、discard_padding=0，次包PTS0；不猜测延迟，不重复从已裁剪PCM扣除。


## 真实结果及失败边界

初始四源均exit0；重入时11:21:54.400发送node102失败，CLI自主exit1、workers0、媒体额度174834预约/释放一致且归零。11:21:54.499已记录failed/retired及终态；不是caller-stop，也不等于rk516具体解码阻塞被触发。重入远程源核对exe后TERM，exit255；Windows两源核对Path后清理，exit-1，不能算完整120秒。tcpdump精确PID SIGINT正常exit0，捕获299210包、filter收到299302、kernel drop0；captured与received by filter计数差92、原因未单独确认，不把drop0等同全捕获。

最终workerErrors/errors=2，payloadPressureFailures=5；RSS初始104796160B、峰/最终125841408B，平均整机进程CPU17.530104%、峰50.239234%。totalPushed312410/Popped312114、encoded49016/49008保留差值。首代音频aggregate real4852736、silence1595392；源2在排空中被cancelled并清理72项/72704样本，不把全部缺口都归于这一个队列。

发送失败序号122156：now168623782616、release168557424713、deadline168624088458、not_before168623989076、not_after168623871498、duration216960ns、rate6250000B/s。最早合法start比最晚start晚117578ns；deadline-release66.663745ms。独立只读审查确认当前最大速率/硬期限下拒绝正确，完整链路FAIL；不能通过抬速、放宽期限或清空服务债务补救。具体上游预算消耗仍待因果分析，不仅凭同时抓包归因。

## 原始协议与首代内容

输入首代SSRC：视频3370658339，音频1362614934；输出696190908。首代完整3600视频AU、5168 AAC AU；视频到达乱序17次，序号重排后缺口0。第二代视频2258760168有16次乱序、1971完整AU及未完成fragment，音频1828201138有2872AU；属于中止段，不计完整。输出122115 RTP包无序号缺口，TS CC错误0、TEI0。共118 SR、40 SDES、2 BYE；初代音视频最后SR及BYE均捕获于1795058471232218000ns（目标墙钟）。

原始TS逐流DTS非递增0：视频4382包、音频6290包。全解码exit0但视频有161字节告警（MB13 21/bytestream -80、corrupt decoded frame），不能写无告警通过；发生在失败终止输出中的具体AU尚未定位。音频PCM连续无PTS缺口，共6440960样本。参考5292000样本；输入5292032样本，剔除实际Skip1024后5291008样本与参考逐样本相等，参考末992样本未在输入中交付。

原始输出静音检测：110.039501–146.053515秒；最后非零样本4852927（110.043696秒）。参考111–120秒RMS0.19682784，不能将输出静音解释为原源尾部无声。

输出1/10/30/60/90/105秒视频标记分别frame32/302/902/1802/2702/3152，即源PTS领先输出2帧。音频1/10/30/60/90/100/105秒最高相关候选均领先14761样本（334.716553ms）；105秒整段相关0.998745573，106..109秒留出最高候选一致。输入SR共同NTP基准差238.770974878–238.791383307ms；按主候选扣除视频2帧后的内容A/V差268.049887ms，扣输入基准后残差29.258503–29.278912ms。只是有限候选证据，不设新通过阈值。周期竞争峰领先8.601383秒、相关0.995918638仍保留；本轮尾部过早静音使rk456用完整尾部消歧的方法无法等价完成，不宣称唯一内容对应或精确缺失时长。

实际离线程序见[rk520取证参数](realtime-composition-rk520-offline-evidence.md)。静音命令沿rk519，将输入改成本轮原始TS、d=0.5；视频提取直接执行FFmpeg select帧30/300/900/1800/2700/3150、crop=640:70:0:360、showinfo、fps_mode passthrough、frames:v6，六张离线标记图均已检查；这些是内容分析图，现场画面仍仅VLC原生快照。

## 已确认的共享机制缺口

独立源码审查确认：closing且input sealed后，MediaPreparedRtpSourceSession::tickLocked仍prepareClockTransition(now)。MediaRtpInputClockSession老化产生ClockEvidenceExpired（原始reason0）；11:21:18.234两条该事件使group失锁，gate发HardDiscontinuity（reason1），继而cancelled1与purge。validator.snapshot也会独立老化，不能仅跳过一个tick。Windows/RKMPP共用此路径。

修复方向需限定已封闭代、已接纳的有限媒体范围，保留校准到有序排空完成，不全局延长SR超时或忽略真正新源/硬不连续。尚未实施。行业对照：[RFC3550源成员/RTCP合同](https://www.rfc-editor.org/rfc/rfc3550)、[GStreamer EOS与FLUSH区分](https://gstreamer.freedesktop.org/documentation/additional/design/events.html#eos)：EOS按数据顺序处理、FLUSH丢弃待处理数据；这里应保留既有有序排空合同，而非把停止SR更新等同取消已接纳数据。

## 保留的原始诊断

```text
[CLI] [2026-11-19 11:21:11.232] [info] [graph][runtime.node] prepared_rtp_source_closing generation=1 video_deadline_ns=259020118991607 audio_deadline_ns=259020118991607
[2026-11-19 11:21:11.433] [info] [graph][runtime.node] prepared_rtp_input_sealed stream=1 generation=1 ingress_generation=0 frontier=3625
[2026-11-19 11:21:13.160] [info] [graph][runtime.node] prepared_rtp_input_sealed stream=2 generation=1 ingress_generation=0 frontier=5193
[2026-11-19 11:21:18.234] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=0 observed_at_ns=259026920377386
[2026-11-19 11:21:18.234] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=0 observed_at_ns=259026920881686
[2026-11-19 11:21:18.278] [info] [graph][runtime.node] prepared_rtp_generation_released generation=1 cancelled=1
[2026-11-19 11:21:18.278] [info] [graph][runtime.node] continuous_audio_source_purge stage=before_clear node=7 source=composition-rk520.source.2 old_generation=1 next_generation=2 transition=1 published_generation=1 owner_candidates=72 owner_candidate_samples=72704 output_sample_rate=44100 next_output_sample=4854784 committed_real_samples=4852736 pending_cancelled=0 interval_semantics=envelope_not_continuity pending_may_overlap_candidates=1 old_original_count=72 old_original_sample_sum=72704 old_original_incomplete=0 old_original_envelope_begin=4856524 old_original_envelope_end=4929228 old_mapped_count=72 old_mapped_sample_sum=72704 old_mapped_incomplete=0 old_mapped_envelope_begin=4852294 old_mapped_envelope_end=4924998 pending_real_output_count=0 pending_real_output_sample_sum=0 pending_real_output_incomplete=0 pending_real_output_envelope=unavailable
[2026-11-19 11:21:54.400] [info] [graph][runtime.node] datagram_reservation_trace stage=reservation_failed generation=1 endpoint=1 sequence=122156 release_master_ns=168557424713 deadline_master_ns=168624088458 publication_role=media original_protocol_deadline_master_ns=168624088458 prefix_completion_limit_master_ns=168624088458 materialized_master_ns=168617416003 planning_generation=1 planning_cursor=15789 planning_cursor_revision=3 planning_publication=15865 planning_protocol_begin=170 planning_protocol_end=200 planning_committed_sequence=122136 planning_observed_master_ns=168617199582 planning_projected_available_master_ns=168617199582 mux_publication_ready_master_ns=168617312168 planning_submitted_service=available planning_submitted_generation=1 planning_submitted_sequence=122136 planning_submit_completed_master_ns=168616911119 planning_service_duration_ns=216960 planning_scope_available_master_ns=168617128079 activation_started_master_ns=168618341477 activation_completed_master_ns=168618347311 permit_started_master_ns=168623781157 permit_completed_master_ns=168623781741 snapshot_started_master_ns=168623782324 snapshot_sampled_master_ns=168623782616 snapshot_completed_master_ns=168623782907 reservation_completed_master_ns=168623814700 wait_started_master_ns=unavailable wait_completed_master_ns=unavailable submit_observed_master_ns=unavailable wait_attempts=0 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=0 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=0 wait_call_total_ns=0 wait_call_max_ns=0 wait_before_call_count=0 wait_before_call_total_ns=0 wait_before_call_max_ns=0 wait_arm_count=0 wait_arm_total_ns=0 wait_arm_max_ns=0 wait_api_wall_count=0 wait_api_wall_total_ns=0 wait_api_wall_max_ns=0 wait_linux_slack_set_count=0 wait_linux_slack_set_total_ns=0 wait_linux_slack_set_max_ns=0 wait_linux_slack_restore_count=0 wait_linux_slack_restore_total_ns=0 wait_linux_slack_restore_max_ns=0 wait_return_to_next_clock_count=0 wait_return_to_next_clock_total_ns=0 wait_return_to_next_clock_max_ns=0 wait_target_lateness_count=0 wait_target_lateness_total_ns=0 wait_target_lateness_max_ns=0 original_error="IoFailure: Datagram GBRA reservation misses its immutable completion deadline sequence=122156 now_ns=168623782616 release_ns=168557424713 deadline_ns=168624088458 not_before_ns=168623989076 not_after_ns=168623871498 duration_ns=216960 rate=6250000 queue_wire_bytes=14352 average_queue_residence_ns=6366613 previous_submitted_sequence=122155 last_observed_ns=168623772116 theoretical_arrival_ns=168623989076"
```


发送批次原始诊断：

```text
[2026-11-19 11:21:54.402] [info] [graph][runtime.node] datagram_batch_trace stage=last_completed generation=1 first_sequence=122052 last_sequence=122136 maximum_submit_interval_ns=88960 maximum_submit_interval_sequence=122127 maximum_start_lateness_ns=1103439 maximum_start_lateness_sequence=122059 first_release_master_ns=168557424713 first_deadline_master_ns=168624088458 last_release_master_ns=168557424713 last_deadline_master_ns=168624088458 first_submitted_master_ns=168592763023 last_submitted_master_ns=168616911119 worst_wait_started_master_ns=168595130815 worst_wait_completed_master_ns=168596429047 worst_submit_observed_master_ns=168596431964 wait_attempts=84 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=84 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=84 wait_call_total_ns=20189237 wait_call_max_ns=1296482 wait_before_call_count=84 wait_before_call_total_ns=43455 wait_before_call_max_ns=1167 wait_arm_count=0 wait_arm_total_ns=0 wait_arm_max_ns=0 wait_api_wall_count=84 wait_api_wall_total_ns=19744433 wait_api_wall_max_ns=1290939 wait_linux_slack_set_count=84 wait_linux_slack_set_total_ns=52217 wait_linux_slack_set_max_ns=1167 wait_linux_slack_restore_count=84 wait_linux_slack_restore_total_ns=73212 wait_linux_slack_restore_max_ns=2625 wait_return_to_next_clock_count=84 wait_return_to_next_clock_total_ns=64166 wait_return_to_next_clock_max_ns=3208 wait_target_lateness_count=84 wait_target_lateness_total_ns=3238321 wait_target_lateness_max_ns=1100522 worst_wait_metric=call_wall worst_wait_sequence=122059 worst_wait_target_master_ns=168595328525 worst_wait_sampled_master_ns=168595130815 worst_wait_sampled_steady_ns=259063058343515 worst_wait_timeout_ns=197710 worst_wait_entered_steady_ns=259063058344098 worst_wait_returned_steady_ns=259063059640580 worst_wait_arm_before_steady_ns=unavailable worst_wait_arm_after_steady_ns=unavailable worst_wait_api_before_steady_ns=259063058347307 worst_wait_api_returned_steady_ns=259063059638246 worst_wait_windows_due_100ns=unavailable worst_wait_windows_result=unavailable worst_wait_windows_error=unavailable worst_wait_windows_pid=unavailable worst_wait_windows_tid=unavailable worst_wait_windows_qpc_frequency_hz=unavailable worst_wait_linux_tid=3061199 worst_wait_linux_scheduler_policy=0 worst_wait_linux_timer_slack_ns=50000 worst_wait_linux_timer_slack_error=unavailable worst_wait_linux_slack_set_before_ns=259063058346723 worst_wait_linux_slack_set_after_ns=259063058347307 worst_wait_linux_slack_restore_before_ns=259063059638246 worst_wait_linux_slack_restore_after_ns=259063059639413 worst_wait_linux_slack_set_error=unavailable worst_wait_linux_slack_restore_error=unavailable worst_wait_next_master_ns=168596429047 worst_wait_next_steady_ns=259063059641746 worst_wait_outcome=deadline
[2026-11-19 11:21:54.402] [info] [graph][runtime.node] datagram_batch_trace stage=active generation=1 first_sequence=122137 last_sequence=122166 maximum_submit_interval_ns=51626 maximum_submit_interval_sequence=122139 maximum_start_lateness_ns=1237024 maximum_start_lateness_sequence=122137 first_release_master_ns=168557424713 first_deadline_master_ns=168624088458 last_release_master_ns=168557424713 last_deadline_master_ns=168624088458 first_submitted_master_ns=168618412645 last_submitted_master_ns=168623772116 worst_wait_started_master_ns=168618364228 worst_wait_completed_master_ns=168618364228 worst_submit_observed_master_ns=168618365103 wait_attempts=18 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=18 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=18 wait_call_total_ns=4190740 wait_call_max_ns=288172 wait_before_call_count=18 wait_before_call_total_ns=9916 wait_before_call_max_ns=584 wait_arm_count=0 wait_arm_total_ns=0 wait_arm_max_ns=0 wait_api_wall_count=18 wait_api_wall_total_ns=4090116 wait_api_wall_max_ns=282630 wait_linux_slack_set_count=18 wait_linux_slack_set_total_ns=9917 wait_linux_slack_set_max_ns=584 wait_linux_slack_restore_count=18 wait_linux_slack_restore_total_ns=17208 wait_linux_slack_restore_max_ns=1459 wait_return_to_next_clock_count=18 wait_return_to_next_clock_total_ns=14586 wait_return_to_next_clock_max_ns=1750 wait_target_lateness_count=18 wait_target_lateness_total_ns=656471 wait_target_lateness_max_ns=87546 worst_wait_metric=call_wall worst_wait_sequence=122151 worst_wait_target_master_ns=168622492507 worst_wait_sampled_master_ns=168622290423 worst_wait_sampled_steady_ns=259063085502831 worst_wait_timeout_ns=202084 worst_wait_entered_steady_ns=259063085503414 worst_wait_returned_steady_ns=259063085791586 worst_wait_arm_before_steady_ns=unavailable worst_wait_arm_after_steady_ns=unavailable worst_wait_api_before_steady_ns=259063085506331 worst_wait_api_returned_steady_ns=259063085788961 worst_wait_windows_due_100ns=unavailable worst_wait_windows_result=unavailable worst_wait_windows_error=unavailable worst_wait_windows_pid=unavailable worst_wait_windows_tid=unavailable worst_wait_windows_qpc_frequency_hz=unavailable worst_wait_linux_tid=3061199 worst_wait_linux_scheduler_policy=0 worst_wait_linux_timer_slack_ns=50000 worst_wait_linux_timer_slack_error=unavailable worst_wait_linux_slack_set_before_ns=259063085505747 worst_wait_linux_slack_set_after_ns=259063085506331 worst_wait_linux_slack_restore_before_ns=259063085788961 worst_wait_linux_slack_restore_after_ns=259063085790127 worst_wait_linux_slack_set_error=unavailable worst_wait_linux_slack_restore_error=unavailable worst_wait_next_master_ns=168622580053 worst_wait_next_steady_ns=259063085792461 worst_wait_outcome=deadline
```


## 本轮清理清单

本机 53 项：

composition-rk520-61020-2258760168.h264
composition-rk520-61020-3370658339.h264
composition-rk520-61022-1362614934.aac
composition-rk520-61022-1828201138.aac
composition-rk520-62024-696190908.ts
composition-rk520-audio-map.json
composition-rk520-cli.log
composition-rk520-content-preliminary.json
composition-rk520-frame-00001.png
composition-rk520-frame-00301.png
composition-rk520-frame-00601.png
composition-rk520-frame-00901.png
composition-rk520-frame-01201.png
composition-rk520-frame-01501.png
composition-rk520-frame-01801.png
composition-rk520-frame-02101.png
composition-rk520-frame-02401.png
composition-rk520-frame-02701.png
composition-rk520-frame-03001.png
composition-rk520-frame-03301.png
composition-rk520-frame-03601.png
composition-rk520-frame-03901.png
composition-rk520-frame-04201.png
composition-rk520-frame-04501.png
composition-rk520-frame-04801.png
composition-rk520-frame-05101.png
composition-rk520-input1-audio.log
composition-rk520-input1.f32
composition-rk520-joint.pcap
composition-rk520-long-match.json
composition-rk520-marker-01.png
composition-rk520-marker-02.png
composition-rk520-marker-03.png
composition-rk520-marker-04.png
composition-rk520-marker-05.png
composition-rk520-marker-06.png
composition-rk520-output-audio.log
composition-rk520-output.f32
composition-rk520-packet-map.json
composition-rk520-received.ts
composition-rk520-reentry2.log
composition-rk520-reentry2.out
composition-rk520-reentry3.log
composition-rk520-reentry3.out
composition-rk520-reference-audio.log
composition-rk520-reference.f32
composition-rk520-source2.log
composition-rk520-source2.out
composition-rk520-source3.log
composition-rk520-source3.out
composition-rk520-vlc.log
composition-rk520-wire-decode.log
composition-rk520-wire-packets.json

远程23项：capture.out、socket-monitor.log、udp-after.txt、capture.log、output.sdp、runner.log、source0.out、reentry1.out、reentry0.out、cli-error.log、monitor.log、result.txt、reentry0.log、pids.txt、run.sh、cli.log、source1.out、source0.log、reentry1.log、udp-before.txt、joint.pcap、source1.log、cli-maps.txt。所列本机/远程媒体PID均已不存在；清理完成：远程23项及空目录、本机53项均删除，复查无本轮残留。两名独立审查者复算后限定EVIDENCE/DOC PASS；这是失败记录审查通过，不是功能验收通过。
