# rk441：RKMPP四路H264 VBR RTP矩阵验证

状态：本轮画面、失源重入与显式停止功能检查通过；合屏整体验收未完成。复用e127a744 CLI SHA256 006f127663332c47dd04ebcb3d9fb62c278ceafc130d713e5be027c62ce565dc。原四源120秒→H2641280×720/30fps VBR目标8/范围8–12Mbps、AAC192kbps MPEG-TS/RTP；计划全失源后source2完整120秒重入。Windows r440对应四源新版本仍FAIL，本轮取得独立RKMPP证据，不代替其修复，不改本机FFmpeg或目标依赖。

## 远端实际脚本

acceptance/rk441/run.sh由SSH调用，完成后删除。
```bash
#!/usr/bin/env bash
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
export PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin:$PATH
export LD_LIBRARY_PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib:/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib:/opt/mt-tools/gcc-12.4-native/lib64
set -u
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk441
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200' -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye 'rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200' >source2.out 2>source2.log &
s2=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk441 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 192.168.96.122 --rtp-port 62024 --sdp /home/tang/MediaTranscode-composition-09093919/acceptance/rk441/output.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
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

## Windows源与VLC实际命令

```text
D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61050?rtcpport=61051&pkt_size=1200&localaddr=192.168.96.122"
D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk441-vlc.log --extraintf=rc --rc-host=127.0.0.1:42120 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk441-native- --snapshot-format=png rtp://@:62024
```

## 实际进程

远端wrapper1527448、source0/1/2为1527453/1527454/1527455、CLI1527456、monitor1527458；Windows源33324、VLC40548。原生快照composition-rk441-native-2026-11-16-15h23m24s630.png已查看四格实画面，当前errors0，pressureFailures5保留，不表述为零压力。

## source2重入

首轮三远端源均自然结束，CLI继续输出；全黑原生PNG composition-rk441-native-2026-11-16-15h24m39s357.png已检查。实际在acceptance/rk441目录执行以下完整120秒重入命令：
```bash
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200' -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye 'rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200' >reentry.out 2>reentry.log &
```

## 最终结果与边界

原四源与重入源完整3600帧/120秒、exit0；重入PID1546573。CLI约355秒后执行kill -TERM 1527456，caller_stop/exit0；不是失源自然EOF。VLC RC quit后exit0。四张原生快照依次确认四格、全黑、仅source2左下恢复、再次全黑。generation=2末条raw/filtered phase均2268ns，frequency/integral/stretch均0ppm；不是内容级A/V证明。

errors/workerErrors/droppedBuffers均0；payload预约/释放374096相等，current bytes/objects均0，峰值19318458字节；pressureFailures=19、backpressureItems=147。平均进程整机CPU12.364279%（单核98.914232%）；内部working set初始59297792、最终/峰值73302016字节。外部328次采样RSS峰值72536KiB，末次355秒/95.1%单核CPU；不能宣称长期内存稳定。

sender提交242806数据报，媒体242720、控制86/5856字节，deadline_misses=0，最大提交lateness36658834ns、最大backlog residence60288995ns。delivery_evidence=not_proven。最差成功来源诊断：

```text
[2026-11-16 15:27:40.669] [info] [graph][runtime.node] datagram_successful_submit_trace stage=worst_success generation=1 endpoint=1 sequence=52169 pacing_lateness_ns=36658834 matches_pacing_worst=1 enqueued_master_ns=unavailable submit_interval_scope=pre_transport_checks_and_transport socket_only_ns=unavailable release_master_ns=57326320224 deadline_master_ns=57426319169 publication_role=media original_protocol_deadline_master_ns=57426319169 prefix_completion_limit_master_ns=57426319169 materialized_master_ns=57365493300 planning_generation=1 planning_cursor=5706 planning_cursor_revision=1 planning_publication=5740 planning_protocol_begin=0 planning_protocol_end=24 planning_committed_sequence=52168 planning_observed_master_ns=57365268421 planning_projected_available_master_ns=57365268421 mux_publication_ready_master_ns=57365388298 planning_submitted_service=available planning_submitted_generation=1 planning_submitted_sequence=52168 planning_submit_completed_master_ns=57329363245 planning_service_duration_ns=36480 planning_scope_available_master_ns=57329399725 reserved_at_master_ns=57366043976 not_before_master_ns=57329399725 not_after_master_ns=57426102209 service_duration_ns=216960 wire_bytes_per_second=6250000 submit_started_master_ns=57366058559 submit_completed_master_ns=57366146936 activation_started_master_ns=57366031142 activation_completed_master_ns=57366038726 permit_started_master_ns=57366041351 permit_completed_master_ns=57366042517 snapshot_started_master_ns=57366043392 snapshot_sampled_master_ns=57366043976 snapshot_completed_master_ns=57366045434 reservation_completed_master_ns=57366054768 wait_started_master_ns=57366057101 wait_completed_master_ns=57366057101 submit_observed_master_ns=57366058559 wait_attempts=0 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=0 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=0 wait_call_total_ns=0 wait_call_max_ns=0 wait_before_call_count=0 wait_before_call_total_ns=0 wait_before_call_max_ns=0 wait_arm_count=0 wait_arm_total_ns=0 wait_arm_max_ns=0 wait_api_wall_count=0 wait_api_wall_total_ns=0 wait_api_wall_max_ns=0 wait_return_to_next_clock_count=0 wait_return_to_next_clock_total_ns=0 wait_return_to_next_clock_max_ns=0 wait_target_lateness_count=0 wait_target_lateness_total_ns=0 wait_target_lateness_max_ns=0
```

stderr：client 12 driver is not ready一次；Changing video frame properties八次（range unknown→tv）；MPP退出清理misc group一次。编码readback为VBR bps[8000000:8000000:12000000]、30fps/GOP60。无runtime reservation_failed/node.error。系统UDP InErrors/RcvbufErrors由35701升至54743（+19042），NoPorts+62；属于全系统计数，不能归因本会话。socket-monitor十个输入端口61020/21/22/23/30/31/40/41/50/51所有已采样drop均0；仍不证明零网络丢包。

r440共享调度FAIL、rk427历史sender根因、内容级A/V及最终双审未关闭。本轮仅补RKMPP功能矩阵，不宣称完整验收通过。

## 清理清单

远端acceptance/rk441：cli-error.log、cli-maps.txt、cli.log、monitor.log、output.sdp、pids.txt、reentry-result.txt、reentry.log、reentry.out、result.txt、run.sh、runner.log、socket-monitor.log、source0.log、source0.out、source1.log、source1.out、source2.log、source2.out、udp-after.txt、udp-before.txt、evidence.json。
本机out/acceptance：composition-rk441-evidence.json、composition-rk441-native-2026-11-16-15h23m24s630.png、composition-rk441-native-2026-11-16-15h24m39s357.png、composition-rk441-native-2026-11-16-15h25m50s200.png、composition-rk441-native-2026-11-16-15h27m48s691.png、composition-rk441-source3.log、composition-rk441-source3.out、composition-rk441-vlc.log。
远端七PID1527448/1527453/1527454/1527455/1527456/1527458/1546573与本机33324/40548已无进程。遗留SCP30024核对为本轮传输后终止，新SCP传输exit0，不计入媒体结果。已逐项清理远端22、本机8文件，目录复查无本轮残留；保留原120秒源、构建产物及回退副本。
