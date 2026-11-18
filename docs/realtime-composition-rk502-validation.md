# rk502：RKMPP 三源 H264 CBR（三格及所选源恢复基础生命周期通过；完整交付未通过）

沿用86d817e8产物，输入三H264 RTP/所选AAC，输出H264 CBR8000kbps 1280×720/30、AAC192kbps MPEGTS/RTP。对照Windows r501，不降低参数，不修改依赖。

## 实际命令与远程脚本

~~~bash
#!/usr/bin/env bash
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
export PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin:$PATH
export LD_LIBRARY_PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib:/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib:/opt/mt-tools/gcc-12.4-native/lib64
set -u
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk502
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk502 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://192.168.130.229:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 192.168.96.122 --rtp-port 62024 --sdp /home/tang/MediaTranscode-composition-09093919/acceptance/rk502/output.sdp --video-codec h264 --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
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
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -vf "drawtext=fontfile=''C\:/Windows/Fonts/consola.ttf'':text=''frame=%{frame_num} pts=%{pts}'':fontsize=48:fontcolor=white:box=1:boxcolor=black:x=16:y=16" -c:v h264_nvenc -rc cbr -b:v 8200k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61020?rtcpport=61021&pkt_size=1200&localaddr=192.168.96.122" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://192.168.130.229:61022?rtcpport=61023&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk502-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk502-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE2_EXIT=$($p.ExitCode)"; exit $p.ExitCode

$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk502-vlc.log --extraintf=rc --rc-host=127.0.0.1:42120 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk502-native- --snapshot-format=png rtp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "VLC_EXIT=$($p.ExitCode)"

~~~

现场PID：runner1349045、源1349047/1349048、CLI1349049、monitor1349051；Windows源34544、VLC37836。

三源全部结束后VLC原生全黑已查看；CLI1349049保持存活。仅所选源2同命令重入（日志增加-reentry），PID32280，完整120秒。当前pressureFailures2但errors0，保留计数，不判零压力。

## 结果与原文

四张VLC原生快照依次三格/全黑/仅左下恢复/再次全黑，均已查看。核对cmdline后执行kill -TERM 1349049，正常caller_stop exit0（非自动EOF）；远程两源exit0，各3600帧/120秒，Windows所选源两轮均3600帧/120秒。最终errors0、payload295417预约=释放回零、pressureFailures7；后者仍是可靠性待查项。CPU平均14.548025%（8核），RSS起52084736、末/峰67162112字节；内部drift不证明内容A/V或静音质量。只所选源重入，不冒充全部三源重入。

~~~text
[2026-11-18 13:37:44.585] [info] [graph][runtime.node] av_reacquisition group=composition-rk502.source.2 event=purge_completed purge_ack=complete continuation=acquiring ack_count=2 old=1 next=2 transition=1
[2026-11-18 13:38:00.001] [info] [graph][runtime.node] av_reacquisition group=composition-rk502.source.1 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=1 next=2 transition=1
[2026-11-18 13:38:00.001] [info] [graph][runtime.node] av_reacquisition group=composition-rk502.source.0 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=1 next=2 transition=1
[2026-11-18 13:40:25.463] [info] [graph][runtime.node] av_reacquisition group=composition-rk502.source.2 event=purge_completed purge_ack=complete continuation=acquiring ack_count=2 old=2 next=3 transition=2
[2026-11-18 13:40:54.232] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:2 execution_mode=1 committed_batches=32018 committed_datagrams=212388 committed_payload_bytes=255360904 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=61 ordered_queue_maximum_wire_bytes=82716 pacing_reserved=212388 pacing_cancelled=0 pacing_submitted=212388 pacing_maximum_submit_lateness_ns=44502742 pacing_worst_late_global_sequence=186200 pacing_worst_success_sample_available=1 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=61 backlog_high_water_wire_bytes=82716 backlog_max_residence_ns=50444567 last_materialized_sequence=212388 last_scheduled_sequence=212388 last_submitted_sequence=212388 last_committed_sequence=212388 submitted_service_generation=1 submitted_service_sequence=212388 submitted_service_completed_ns=299444871892 submitted_service_duration_ns=16640 submitted_service_available_ns=299444888532 submitted_scope_available_ns=299444888532 shared_scope_charged_datagrams=212388 shared_scope_charged_wire_bytes=261307768 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=1153 evidence_timestamp_tracked=0 evidence_timestamp_untracked=1153 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=396984 endpoint_1_target_effective_socket_bytes=198492 endpoint_1_api_requested_socket_bytes=99246 endpoint_1_effective_socket_bytes=198492 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=212321 endpoint_1_committed_payload_bytes=255356340 endpoint_2_target_effective_socket_bytes=198492 endpoint_2_api_requested_socket_bytes=99246 endpoint_2_effective_socket_bytes=198492 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=67 endpoint_2_committed_payload_bytes=4564
[2026-11-18 13:40:54.477] [info] [graph][runtime.lifecycle] stop.done state=Stopped
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=1716, threads=83, processThreads=94, workers=0, workerProgress=531109, workerProcessCalls=1967215, workerWaits=1436088, workerWakeups=1396267, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=1169, logicalProcessors=8, averageSystemMachineCpuPercent=30.708341, averageProcessMachineCpuPercent=14.548025, peakProcessMachineCpuPercent=44.897959, averageProcessSingleCoreCpuPercent=116.384201, peakProcessSingleCoreCpuPercent=359.183673, initialWorkingSetBytes=52084736, workingSetBytes=67162112, peakWorkingSetBytes=67162112, totalPushed=465096, totalPopped=465096, droppedBuffers=0, encodedPacketsPushed=98458, encodedPacketsPopped=98458, backpressureItems=121, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=16798863, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=767, graphPayloadReservations=295417, graphPayloadReleases=295417, graphPayloadPressureFailures=7, lastInputReceivedAtNs=180972175356118
[CLI] realtime video validation stopped successfully
[2026-11-18 13:40:21.763] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=5001 source_master_ns=268393308809 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=243713 effective_output_sample_index=5361686
CLI_EXIT=0
SOURCE0_EXIT=0
SOURCE1_EXIT=0
RUN_FINISHED=1

~~~

## 本轮清理清单

本机：composition-rk502-native-2026-11-18-13h37m07s293.png、composition-rk502-native-2026-11-18-13h38m47s401.png、composition-rk502-native-2026-11-18-13h39m27s908.png、composition-rk502-native-2026-11-18-13h41m18s066.png、composition-rk502-run.sh、composition-rk502-source2-reentry.log、composition-rk502-source2-reentry.out、composition-rk502-source2.log、composition-rk502-source2.out、composition-rk502-vlc.log。远程rk502目录及本机副本：cli-error.log、cli-maps.txt、cli.log、monitor.log、output.sdp、pids.txt、result.txt、runner.log、socket-monitor.log、source0.log、source0.out、source1.log、source1.out、udp-after.txt、udp-before.txt。远程另有acceptance/composition-rk502-run.sh；按清单逐项删除，不动指定源或rollback。

独立审查retired_control_review_b：有限生命周期PASS，完整交付未通过。清理完成：本机10项加远程副本15项、目标机15项及脚本逐项删除；本机三PID和远程五PID均无残留。指定源、正式产物和rollback保留。
