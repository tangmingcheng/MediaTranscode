# rk498：四路失源控制身份复验（基础生命周期实测通过，完整交付门禁未闭合）

源码 86d817e8；目标 CLI SHA256 b4c9d8b39118505b079ef1445b37341e8ef7cf26fc1ec7d64a8318f3e4e44d4a。只运行 realtime，未修改 Windows FFmpeg。

输入三路 H264、一路 HEVC，所选源2 AAC；输出 MPEGTS/RTP H264 1280×720 30fps VBR 目标/最低8000kbps、最高12000kbps，AAC 192kbps。

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
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk498
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk498 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://192.168.130.229:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 192.168.96.122 --rtp-port 62024 --sdp /home/tang/MediaTranscode-composition-09093919/acceptance/rk498/output.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
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
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -vf "drawtext=fontfile=''C\:/Windows/Fonts/consola.ttf'':text=''frame=%{frame_num} pts=%{pts}'':fontsize=48:fontcolor=white:box=1:boxcolor=black:x=16:y=16" -c:v h264_nvenc -rc cbr -b:v 8200k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61020?rtcpport=61021&pkt_size=1200&localaddr=192.168.96.122" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://192.168.130.229:61022?rtcpport=61023&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk498-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk498-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"
~~~

~~~powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61050?rtcpport=61051&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk498-source3.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk498-source3.log' -PassThru; Write-Output "SOURCE3_PID=$($p.Id)"
~~~

~~~powershell
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk498-vlc.log --extraintf=rc --rc-host=127.0.0.1:42120 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk498-native- --snapshot-format=png rtp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"
~~~

远端启动：bash -n acceptance/rk498/run.sh && bash acceptance/rk498/run.sh >acceptance/rk498/launcher.log 2>&1 &。




## 现场阶段证据

launcher1188927；源0=1188931、源1=1188932、CLI=1188933、monitor=1188935；本机源2=27372、源3=35928、VLC=35924。首轮源完整3600帧结束，CLI持续运行；13:01:11 source0/1 HardDiscontinuity、13:01:18 source2 HardDiscontinuity及source3 SourceDrained均purge_completed。VLC原生快照确认四格→全黑，输出encodedPackets持续增加（全黑时50838），workerErrors=0。

同端口重入沿用上述四源完整命令，仅日志名sourceN增加-reentry；远端重入0=1203148、1=1203154，本机2=39728、3=10584。CLI未重启。generation2音频drift约9071ns、frequency/stretch0；重入VLC四格快照已查看。第二轮源结束、再次全黑及调用方停止结果见下文。

## 终态与验收边界

两轮各四源完整120秒，第二轮远端两源wait exit0，本机源日志均3600帧。CLI保持同一PID。第二轮失源四组SourceDrained old2→next3均完成，VLC再次全黑。核对PID后执行kill -TERM 1188933，正常进入requestStop，CLI_EXIT=0/end_reason=caller_stop；此退出是调用方停止，不是全失源自动退出。

~~~text
[2026-11-18 13:01:11.047] [info] [graph][runtime.node] av_reacquisition group=composition-rk498.source.1 phase=purging old=1 next=2 transition=1 reason=1
[2026-11-18 13:01:11.049] [info] [graph][runtime.node] av_reacquisition group=composition-rk498.source.1 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=1 next=2 transition=1
[2026-11-18 13:01:11.072] [info] [graph][runtime.node] av_reacquisition group=composition-rk498.source.0 phase=purging old=1 next=2 transition=1 reason=1
[2026-11-18 13:01:11.099] [info] [graph][runtime.node] av_reacquisition group=composition-rk498.source.0 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=1 next=2 transition=1
[2026-11-18 13:01:18.081] [info] [graph][runtime.node] av_reacquisition group=composition-rk498.source.2 phase=purging old=1 next=2 transition=1 reason=1
[2026-11-18 13:01:18.107] [info] [graph][runtime.node] av_reacquisition group=composition-rk498.source.2 event=purge_completed purge_ack=complete continuation=acquiring ack_count=2 old=1 next=2 transition=1
[2026-11-18 13:01:18.458] [info] [graph][runtime.node] av_reacquisition group=composition-rk498.source.3 phase=purging old=1 next=2 transition=1 reason=4
[2026-11-18 13:01:18.459] [info] [graph][runtime.node] av_reacquisition group=composition-rk498.source.3 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=1 next=2 transition=1
[2026-11-18 13:04:12.633] [info] [graph][runtime.node] av_reacquisition group=composition-rk498.source.0 phase=purging old=2 next=3 transition=2 reason=4
[2026-11-18 13:04:12.636] [info] [graph][runtime.node] av_reacquisition group=composition-rk498.source.0 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=2 next=3 transition=2
[2026-11-18 13:04:12.650] [info] [graph][runtime.node] av_reacquisition group=composition-rk498.source.1 phase=purging old=2 next=3 transition=2 reason=4
[2026-11-18 13:04:12.657] [info] [graph][runtime.node] av_reacquisition group=composition-rk498.source.1 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=2 next=3 transition=2
[2026-11-18 13:04:19.786] [info] [graph][runtime.node] av_reacquisition group=composition-rk498.source.2 phase=purging old=2 next=3 transition=2 reason=4
[2026-11-18 13:04:19.789] [info] [graph][runtime.node] av_reacquisition group=composition-rk498.source.2 event=purge_completed purge_ack=complete continuation=acquiring ack_count=2 old=2 next=3 transition=2
[2026-11-18 13:04:26.222] [info] [graph][runtime.node] av_reacquisition group=composition-rk498.source.3 phase=purging old=2 next=3 transition=2 reason=4
[2026-11-18 13:04:26.223] [info] [graph][runtime.node] av_reacquisition group=composition-rk498.source.3 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=2 next=3 transition=2
[2026-11-18 13:05:18.297] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:2 execution_mode=1 committed_batches=39051 committed_datagrams=264384 committed_payload_bytes=316926044 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=85 ordered_queue_maximum_wire_bytes=115260 pacing_reserved=264384 pacing_cancelled=0 pacing_submitted=264384 pacing_maximum_submit_lateness_ns=37373236 pacing_worst_late_global_sequence=162454 pacing_worst_success_sample_available=1 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=86 backlog_high_water_wire_bytes=115356 backlog_max_residence_ns=52347766 last_materialized_sequence=264384 last_scheduled_sequence=264384 last_submitted_sequence=264384 last_committed_sequence=264384 submitted_service_generation=1 submitted_service_sequence=264384 submitted_service_completed_ns=373741183325 submitted_service_duration_ns=16640 submitted_service_available_ns=373741199965 submitted_scope_available_ns=373741199965 shared_scope_charged_datagrams=264384 shared_scope_charged_wire_bytes=324328796 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=1153 evidence_timestamp_tracked=0 evidence_timestamp_untracked=1153 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=554648 endpoint_1_target_effective_socket_bytes=277324 endpoint_1_api_requested_socket_bytes=138662 endpoint_1_effective_socket_bytes=277324 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=264296 endpoint_1_committed_payload_bytes=316920052 endpoint_2_target_effective_socket_bytes=277324 endpoint_2_api_requested_socket_bytes=138662 endpoint_2_effective_socket_bytes=277324 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=88 endpoint_2_committed_payload_bytes=5992
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=2123, threads=102, processThreads=116, workers=0, workerProgress=770395, workerProcessCalls=3047141, workerWaits=2276727, workerWakeups=2225702, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=1429, logicalProcessors=8, averageSystemMachineCpuPercent=32.247341, averageProcessMachineCpuPercent=17.020849, peakProcessMachineCpuPercent=48.947368, averageProcessSingleCoreCpuPercent=136.166790, peakProcessSingleCoreCpuPercent=391.578947, initialWorkingSetBytes=73854976, workingSetBytes=95444992, peakWorkingSetBytes=95444992, totalPushed=691165, totalPopped=691015, droppedBuffers=0, encodedPacketsPushed=120648, encodedPacketsPopped=120648, backpressureItems=147, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=19577824, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=1383, graphPayloadReservations=407865, graphPayloadReleases=407865, graphPayloadPressureFailures=9, lastInputReceivedAtNs=178813465061367
[2026-11-18 13:04:15.566] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4975 source_master_ns=312389096222 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=231425 effective_output_sample_index=5322775
~~~

基础生命周期观察通过：四格→全黑→重入四格→全黑→正常停止。worker error0、payload全部释放；pressureFailures=9需继续分析，不能称无背压或长期内存稳定。静音/实际听音、内容级A/V、NIC出线证据、长稳矩阵及Windows共享回归未完成；delivery_evidence=not_proven。rk495等发送期限失败未因本轮未复现关闭。没有retired_control日志，不将该窄分支宣称已被本轮直接覆盖。

本机清理清单：composition-rk498-native-2026-11-18-13h00m24s161.png、composition-rk498-native-2026-11-18-13h02m18s991.png、composition-rk498-native-2026-11-18-13h03m24s289.png、composition-rk498-native-2026-11-18-13h05m46s890.png、composition-rk498-source2-reentry.log、composition-rk498-source2-reentry.out、composition-rk498-source2.log、composition-rk498-source2.out、composition-rk498-source3-reentry.log、composition-rk498-source3-reentry.out、composition-rk498-source3.log、composition-rk498-source3.out、composition-rk498-vlc.log、rk498-cli.log。远端清理清单：run.sh、launcher.log、pids.txt、result.txt、cli.log、cli-error.log、cli-maps.txt、monitor.log、socket-monitor.log、udp-before.txt、udp-after.txt、output.sdp、source0.out/log、source1.out/log、source0-reentry.out/log、source1-reentry.out/log。保留源与rollback。

快照时序：本机发送RC snapshot命令并返回成功后，才执行远端kill -TERM；Windows文件名与RKMPP日志使用各自主机墙钟，未校时，不能直接比较13:05:46与13:05:18。末图已人工查看为全黑，持续输出另由全失源后encoded计数继续增长证明。独立智能体已核对四张原生快照、四源第二代发布及8次purge，基础生命周期实测PASS，完整交付NOT PASS。

停止前最后两份runtime：lastInputReceivedAtNs同为178813465061367，encodedPacketsPushed从120511增至120597，workers95/errors0；终态120648、workers0。清理复核：远端本轮文件与精确PID均无残留，本机14项临时产物已按清单删除（VLC按PID35924核对后清理）。
