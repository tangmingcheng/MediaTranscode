# rk445：RKMPP 完成合同与共享水位验证

状态：本轮完成合同授权、水位放行、画面/重入及正常停止功能检查通过；整体验收未完成。源码50993608已获两位独立SOURCE PASS，目标8核增量构建508项exit0。CLI SHA256 0d95cbf3a32d82d51eb602ed7f5548143677af4eb2bf6cc689c37e4d84293222。原四源120秒→H264 1280×720/30fps VBR目标8/范围8–12Mbps、AAC192kbps MPEG-TS/RTP；全部失源后选中source2完整120秒重入。Windows FFmpeg不修改。

## 远端实际命令

临时脚本acceptance/rk445/run.sh，wrapper2095698，source0/1/2为2095703/2095704/2095705，CLI2095706，monitor2095708。

```bash
#!/usr/bin/env bash
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
export PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin:$PATH
export LD_LIBRARY_PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib:/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib:/opt/mt-tools/gcc-12.4-native/lib64
set -u
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk445
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200' -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye 'rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200' >source2.out 2>source2.log &
s2=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk445 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 192.168.96.122 --rtp-port 62024 --sdp /home/tang/MediaTranscode-composition-09093919/acceptance/rk445/output.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
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

## Windows 实际命令

```text
D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61050?rtcpport=61051&pkt_size=1200&localaddr=192.168.96.122"
D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk445-vlc.log --extraintf=rc --rc-host=127.0.0.1:42120 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk445-native- --snapshot-format=png rtp://@:62024
```


Windows HEVC源PID39192，VLC37992。实际prepared三次authorized=1、submission_order、time_base1/30、cadence30/1、split_known1，其余split/low_delay/drop/flex均0，in/out30/1。首完整AU generation1/order1/packet PTS=DTS=submitted PTS=0。首次实际水位放行audio106，canonical2438095238ns < video2466666666ns，observed_after_output_commit=1。

## 实际重入和停止

在acceptance/rk445执行与source2相同的FFmpeg完整命令，仅重定向为reentry.out/reentry.log，PID2117840。最初误在子shell wait返回127（非子进程），属于观测命令错误；原进程持续运行，随后在所属父shell wait 2117840实际返回0。未重启或截短源。原四源及重入源均3600帧/120秒、exit0。

四张VLC原生快照已逐张查看：四格实画面、全黑、仅左下source2恢复、再次全黑。名称见清理清单。CLI约355秒后核对cmdline含本轮media-id，再发送SIGTERM2095706（正常caller-stop桥接）；caller_stop/exit0，不是失源自动EOF。VLC RC quit后进程退出。

## 最终指标与限制

errors/workerErrors/droppedBuffers=0；payload预约=释放353683，current bytes/objects=0，峰8338286字节/387对象；pressureFailures=6，backpressureItems=147。编码推入/取出均118445。平均整机CPU12.975959%、单核103.807674%；WS初始44597248、最终/峰58978304字节。外部331次采样RSS峰57596KiB，最后354秒/96.3%单核CPU。有限时段不能证明长期内存稳定。

重入generation2最后raw/filtered phase=4535ns，frequency/integral/stretch=0ppm；不是内容级A/V证明。sender媒体245677、控制87/5924字节，总245764数据报；deadline_misses=0，最大lateness33813174ns、最大backlog residence59929841ns，delivery_evidence=not_proven。系统UDP InErrors/RcvbufErrors/SndbufErrors增量0、NoPorts+22、IgnoredMulti+54；十个输入socket已采样drop均0，不等于零网络丢包证明。

保留stderr：client 12 driver is not ready；8次视频range unknown→tv属性告警；h265d_decode_nal_sei ret -1006、invalid pps id 0、parse unit #1 ret -1004、current stream is no right skip；退出AAC Qavg22160.896与misc group清理。四格随后正常，但不能仅凭最终errors0宣称没有解码警告或确定其根因。无runtime reservation_failed/node.error。

### 完成合同实际证据

```text
[2026-11-16 16:43:34.418] [info] [graph][planner.capability] encoder_completion_prepared codec=h264_rkmpp authorized=1 reason=authorized lineage=submission_order time_base=1/30 cadence=30/1 split_known=1 split_mode=0 split_arg=0 split_out=0 low_delay=0 drop_mode=0 fps_in_flex=0 fps_in=30/1 fps_out_flex=0 fps_out=30/1
[2026-11-16 16:43:34.440] [info] [graph][planner.capability] encoder_completion_prepared codec=h264_rkmpp authorized=1 reason=authorized lineage=submission_order time_base=1/30 cadence=30/1 split_known=1 split_mode=0 split_arg=0 split_out=0 low_delay=0 drop_mode=0 fps_in_flex=0 fps_in=30/1 fps_out_flex=0 fps_out=30/1
[2026-11-16 16:43:34.464] [info] [graph][planner.capability] encoder_completion_prepared codec=h264_rkmpp authorized=1 reason=authorized lineage=submission_order time_base=1/30 cadence=30/1 split_known=1 split_mode=0 split_arg=0 split_out=0 low_delay=0 drop_mode=0 fps_in_flex=0 fps_in=30/1 fps_out_flex=0 fps_out=30/1
[2026-11-16 16:43:34.541] [info] [graph][runtime.node] encoder_completion_first_au generation=1 identity_kind=output identity="composition-rk445.output" canonical_sequence=1 order=1 lineage=submission_order packet_pts=0 packet_dts=0 submitted_pts=0 canonical_dispatch_ns=0
[2026-11-16 16:43:37.015] [info] [graph][runtime.node] av_scheduler_video_progress_release node=94 generation=1 audio_sequence=106 audio_canonical_dispatch_ns=2438095238 video_canonical_lower_bound_ns=2466666666 observed_after_output_commit=1
```

### 最差成功发送来源

```text
[2026-11-16 16:49:27.768] [info] [graph][runtime.node] datagram_successful_submit_trace stage=worst_success generation=1 endpoint=1 sequence=191374 pacing_lateness_ns=33813174 matches_pacing_worst=1 enqueued_master_ns=unavailable submit_interval_scope=pre_transport_checks_and_transport socket_only_ns=unavailable release_master_ns=257018437165 deadline_master_ns=257118432055 publication_role=media original_protocol_deadline_master_ns=257118432055 prefix_completion_limit_master_ns=257118432055 materialized_master_ns=257052533063 planning_generation=1 planning_cursor=27614 planning_cursor_revision=1 planning_publication=27731 planning_protocol_begin=0 planning_protocol_end=23 planning_committed_sequence=191373 planning_observed_master_ns=257052298851 planning_projected_available_master_ns=257052298851 mux_publication_ready_master_ns=257052434478 planning_submitted_service=available planning_submitted_generation=1 planning_submitted_sequence=191373 planning_submit_completed_master_ns=257019301750 planning_service_duration_ns=36480 planning_scope_available_master_ns=257019338230 reserved_at_master_ns=257053137696 not_before_master_ns=257019338230 not_after_master_ns=257118215095 service_duration_ns=216960 wire_bytes_per_second=6250000 submit_started_master_ns=257053151404 submit_completed_master_ns=257053255822 activation_started_master_ns=257053123112 activation_completed_master_ns=257053131862 permit_started_master_ns=257053134779 permit_completed_master_ns=257053135654 snapshot_started_master_ns=257053136821 snapshot_sampled_master_ns=257053137696 snapshot_completed_master_ns=257053139154 reservation_completed_master_ns=257053146446 wait_started_master_ns=257053149654 wait_completed_master_ns=257053149654 submit_observed_master_ns=257053151404 wait_attempts=0 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=0 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=0 wait_call_total_ns=0 wait_call_max_ns=0 wait_before_call_count=0 wait_before_call_total_ns=0 wait_before_call_max_ns=0 wait_arm_count=0 wait_arm_total_ns=0 wait_arm_max_ns=0 wait_api_wall_count=0 wait_api_wall_total_ns=0 wait_api_wall_max_ns=0 wait_return_to_next_clock_count=0 wait_return_to_next_clock_total_ns=0 wait_return_to_next_clock_max_ns=0 wait_target_lateness_count=0 wait_target_lateness_total_ns=0 wait_target_lateness_max_ns=0
```

本轮关闭实际RKMPP prepared授权、首完整AU校验和实际成功水位放行正例；r443之后新增共享校验仍需Windows回归。rk427历史期限根因、内容A/V、线端、长稳、C残项和最终双审不由本轮关闭。

## 清理清单

远端acceptance/rk445：cli-error.log、cli-maps.txt、cli.log、monitor.log、output.sdp、pids.txt、reentry-result.txt、reentry.log、reentry.out、result.txt、run.sh、runner.log、socket-monitor.log、source0.log、source0.out、source1.log、source1.out、source2.log、source2.out、udp-after.txt、udp-before.txt、evidence.json。
本机out/acceptance：composition-rk445-evidence.json、composition-rk445-native-2026-11-16-16h44m37s635.png、composition-rk445-native-2026-11-16-16h46m56s595.png、composition-rk445-native-2026-11-16-16h47m33s805.png、composition-rk445-native-2026-11-16-16h49m43s764.png、composition-rk445-source3.log、composition-rk445-source3.out、composition-rk445-vlc.log。
已逐项清理远端22、本机8文件，复查本轮目录无残留；远端七PID已核验无残留，Windows37992/39192已退出。保留原120秒源、正式构建产物及out/rollback/rk445回退。

独立智能体对本轮归档与plan摘要给出限定PASS：结论限记录一致性，未独立复测已清理产物，未外推剩余门禁。
