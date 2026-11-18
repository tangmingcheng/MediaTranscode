# rk515：RKMPP 双源HEVC CBR左右布局及所选源重入

沿用21fc6bb8已构建产物37d49a6f17c5447f82f38237f1f99ebe0b5de8387bec5455584372b464feeedc，不改依赖。对照r500原规格：H264纯视频＋H264/AAC RTP输入，HEVC CBR8Mbps、1280×720/30、AAC192kbps MPEG-TS/RTP。初始两源及随后所选AV重入均完整120秒；通过本轮不等于Windows r500期限失败已修复。有限生命周期门禁完成，完整验收未通过。

```bash
#!/usr/bin/env bash
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
export PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin:$PATH
export LD_LIBRARY_PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib:/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib:/opt/mt-tools/gcc-12.4-native/lib64
set -u
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk515
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk515 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://192.168.130.229:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 1 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 192.168.96.122 --rtp-port 62024 --sdp /home/tang/MediaTranscode-composition-09093919/acceptance/rk515/output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
cli=$!
cat /proc/$cli/maps >cli-maps.txt 2>/dev/null
printf 'SOURCE0=%s CLI=%s\n' "$s0" "$cli" >pids.txt
(while kill -0 "$cli" 2>/dev/null; do date -Ins; ps -p "$cli" -o pid=,etimes=,pcpu=,rss=,vsz=; { date -Ins; ls -l /proc/$cli/fd; cat /proc/$cli/net/udp; } >>socket-monitor.log 2>&1; sleep 1; done) >monitor.log &
mon=$!
printf 'MONITOR=%s\n' "$mon" >>pids.txt
wait "$cli"; printf 'CLI_EXIT=%s\n' "$?" >result.txt
wait "$mon"
cat /proc/net/snmp >udp-after.txt
wait "$s0"; printf 'SOURCE0_EXIT=%s\n' "$?" >>result.txt
printf 'RUN_FINISHED=1\n' >>result.txt


```

```powershell
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk515-vlc.log --extraintf=rc --rc-host=127.0.0.1:42120 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk515-native- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk515-received.ts}}" --sout-keep --video-filter=scene --scene-format=png --scene-ratio=300 --scene-prefix=composition-rk515-frame- --scene-path=D:/Code/MyCode/MediaTranscode/out/acceptance rtp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -vf "drawtext=fontfile=''C\:/Windows/Fonts/consola.ttf'':text=''frame=%{frame_num} pts=%{pts}'':fontsize=48:fontcolor=white:box=1:boxcolor=black:x=16:y=16" -c:v h264_nvenc -rc cbr -b:v 8200k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61020?rtcpport=61021&pkt_size=1200&localaddr=192.168.96.122" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://192.168.130.229:61022?rtcpport=61023&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk515-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk515-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE2_EXIT=$($p.ExitCode)"; exit $p.ExitCode

```

## 运行观测
远端runner1712687、source0=1712689、CLI1712690、monitor1712692；本机初始AV25400、VLC38568。初始AV完整120秒exit0，远端纯视频自然结束。VLC00901双格、03601全黑，随后同AV命令仅输出日志改为composition-rk515-reentry启动PID7656；05401确认左黑右格恢复。恢复源随后完成120秒，终态见下。
实际encoder readback：hevc_rkmpp/drm_prime/cbr/8000000bps，min7500000 max8500000 buffer267000 fps30/1 gop60。恢复阶段错误0，pressureFailures1保留。

## 终态
重入7656完整120秒exit0；08401再次全黑。核实/proc/1712690/exe后kill -TERM 1712690，CLI caller_stop exit0；初始远端源exit0。最终errors/workerErrors0，pushed=popped420759，encoded101308相等，payload242950预约=释放、当前0，pressureFailures4。RSS初始40292352，终/峰49979392；平均整机CPU13.375597%、峰30.851064%。generation2漂移filtered_phase11339ns/frequency0，仅内部时钟证据。
实际离线命令：
~~~powershell
& D:/mabs/local64/bin-video/ffmpeg.exe -hide_banner -nostdin -i D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk515-received.ts -map 0:a:0 -af silencedetect=noise=-60dB:d=0.5 -f null -
~~~
exit0；低于-60dB区间95.660249–154.757846（59.097596秒）和272.40576–301.90263（29.496871秒），中间恢复非静音。保留null DTS警告45056>=42979/44003/45027，本轮没有原始wire对照，不能直接沿用rk513归因，不声明音频连续时序全部通过。

~~~text
[2026-11-18 14:58:29.003] [info] [graph][runtime.node] av_reacquisition group=composition-rk515.source.1 event=purge_completed purge_ack=complete continuation=acquiring ack_count=2 old=1 next=2 transition=1
[2026-11-18 14:58:52.371] [info] [graph][runtime.node] av_reacquisition group=composition-rk515.source.0 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=1 next=2 transition=1
[2026-11-18 15:01:26.324] [info] [graph][runtime.node] av_reacquisition group=composition-rk515.source.1 event=purge_completed purge_ack=complete continuation=acquiring ack_count=2 old=2 next=3 transition=2
[2026-11-18 15:01:55.158] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:2 execution_mode=1 committed_batches=32875 committed_datagrams=212463 committed_payload_bytes=254199020 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=61 ordered_queue_maximum_wire_bytes=82716 pacing_reserved=212463 pacing_cancelled=0 pacing_submitted=212463 pacing_maximum_submit_lateness_ns=33066521 pacing_worst_late_global_sequence=126981 pacing_worst_success_sample_available=1 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=61 backlog_high_water_wire_bytes=82716 backlog_max_residence_ns=46870713 last_materialized_sequence=212463 last_scheduled_sequence=212463 last_submitted_sequence=212463 last_committed_sequence=212463 submitted_service_generation=1 submitted_service_sequence=212463 submitted_service_completed_ns=305953995078 submitted_service_duration_ns=16640 submitted_service_available_ns=305954011718 submitted_scope_available_ns=305954011718 shared_scope_charged_datagrams=212463 shared_scope_charged_wire_bytes=260147984 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=1153 evidence_timestamp_tracked=0 evidence_timestamp_untracked=1153 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=396984 endpoint_1_target_effective_socket_bytes=198492 endpoint_1_api_requested_socket_bytes=99246 endpoint_1_effective_socket_bytes=198492 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=212386 endpoint_1_committed_payload_bytes=254193776 endpoint_2_target_effective_socket_bytes=198492 endpoint_2_api_requested_socket_bytes=99246 endpoint_2_effective_socket_bytes=198492 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=77 endpoint_2_committed_payload_bytes=5244
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=873, threads=64, processThreads=72, workers=0, workerProgress=487998, workerProcessCalls=1784141, workerWaits=1296126, workerWakeups=1260455, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=1203, logicalProcessors=8, averageSystemMachineCpuPercent=27.918316, averageProcessMachineCpuPercent=13.375597, peakProcessMachineCpuPercent=30.851064, averageProcessSingleCoreCpuPercent=107.004777, peakProcessSingleCoreCpuPercent=246.808511, initialWorkingSetBytes=40292352, workingSetBytes=49979392, peakWorkingSetBytes=49979392, totalPushed=420759, totalPopped=420759, droppedBuffers=0, encodedPacketsPushed=101308, encodedPacketsPopped=101308, backpressureItems=95, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=6535229, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=374, graphPayloadReservations=242950, graphPayloadReleases=242950, graphPayloadPressureFailures=4, lastInputReceivedAtNs=185833032968568
~~~

独立审查B限定PASS；仅右侧所选AV源重入，左侧纯视频未重入。sender提交212463包，deadline_misses0、pressure_failures0，delivery_evidence=not_proven。离线另有TS DTS119671<122999告警；所有时序告警保留，不能借用rk513具体归因。

独立审查A同样限定PASS；stderr还含filter输入同1280x720/fmt178的range unknown→tv变化告警（pts_time8477.028867），保留但不视为本轮致命错误。未覆盖左源重入/独立失音/内容AV/长稳，不支持更新包。

清理清单：
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-frame-00001.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-frame-00301.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-frame-00601.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-frame-00901.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-frame-01201.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-frame-01501.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-frame-01801.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-frame-02101.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-frame-02401.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-frame-02701.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-frame-03001.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-frame-03301.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-frame-03601.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-frame-03901.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-frame-04201.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-frame-04501.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-frame-04801.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-frame-05101.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-frame-05401.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-frame-05701.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-frame-06001.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-frame-06301.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-frame-06601.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-frame-06901.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-frame-07201.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-frame-07501.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-frame-07801.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-frame-08101.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-frame-08401.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-frame-08701.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-frame-09001.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-received.ts
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-reentry.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-reentry.out
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-silence.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-source2.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-source2.out
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-vlc.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-remote\cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-remote\cli-maps.txt
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-remote\cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-remote\monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-remote\output.sdp
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-remote\pids.txt
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-remote\result.txt
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-remote\runner.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-remote\socket-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-remote\source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-remote\source0.out
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-remote\udp-after.txt
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk515-remote\udp-before.txt

以上本机清单及远端13项文件（socket-monitor.log、udp-after.txt、output.sdp、runner.log、source0.out、cli-error.log、monitor.log、result.txt、pids.txt、cli.log、source0.log、udp-before.txt、cli-maps.txt）已逐项删除；临时run.sh及空目录移除。两端精确PID无残留，原120秒源保留；没有生成或更新库包。
