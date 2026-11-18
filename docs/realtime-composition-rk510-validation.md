# rk510：同机音视频四源失音/恢复及晚BYE复验（有限音频门禁已完成）

修复源码21fc6bb8，CLI hash沿rk509。恢复所选音视频均从Windows发送，保持原四源/720p30/H264 VBR8000–12000kbps/AAC192kbps。仅故障注入停音频，视频保持完整120秒；恢复音频再完整120秒，晚于视频结束，验证Acquiring时BYE。VLC原生scene及TS接收录制。

Windows视频23964/39144、音频28428、VLC37956；远端runner1532123。

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
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk510
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk510 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://192.168.130.229:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 192.168.96.122 --rtp-port 62024 --sdp /home/tang/MediaTranscode-composition-09093919/acceptance/rk510/output.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
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
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk510-vlc.log --extraintf=rc --rc-host=127.0.0.1:42120 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk510-native- --snapshot-format=png --sout "#duplicate{dst=display,dst=std{access=file,mux=ts,dst=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk510-received.ts}}" --sout-keep --video-filter=scene --scene-format=png --scene-ratio=900 --scene-prefix=composition-rk510-frame- --scene-path=D:/Code/MyCode/MediaTranscode/out/acceptance rtp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"
~~~

~~~powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -vf "drawtext=fontfile=''C\:/Windows/Fonts/consola.ttf'':text=''frame=%{frame_num} pts=%{pts}'':fontsize=48:fontcolor=white:box=1:boxcolor=black:x=16:y=16" -c:v h264_nvenc -rc cbr -b:v 8200k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61020?rtcpport=61021&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk510-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk510-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE2_EXIT=$($p.ExitCode)"; exit $p.ExitCode

~~~

~~~powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://192.168.130.229:61022?rtcpport=61023&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk510-audio.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk510-audio.log' -PassThru; Write-Output "AUDIO_PID=$($p.Id)"
~~~

~~~powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61050?rtcpport=61051&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk510-source3.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk510-source3.log' -PassThru; Write-Output "SOURCE3_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE3_EXIT=$($p.ExitCode)"; exit $p.ExitCode

~~~

## 结果

远端源1532125/1532126、CLI1532127、monitor1532129。UTC06:18:48.0329112仅核身份后Stop-Process音频28428，视频继续；同命令仅日志audio-reentry重启13052并完整120秒自然结束。视频四路均完整120秒，远端两源exit0。所选组由1→2恢复，视频先SourceDrained转Acquiring3，音频稍后BYE令clock state3，未再触发rk508身份错误。核对CLI后kill -TERM1532127，正常caller_stop exit0。

VLC原生scene四格及全黑已查看，30秒采样未捕获音频失源期间左下黑的瞬态，不作该瞬态图像证明。录制205.68秒；离线ffmpeg -hide_banner -nostdin -i D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk510-received.ts -map 0:a:0 -af silencedetect=noise=-60dB:d=0.5 -f null - exit0。低于-60dB区间33.280454–58.028231（24.747778秒）、103.896644–205.676871（101.780227秒）；中间恢复非静音。仅验证失源静音/音频恢复及晚BYE生命周期，不证明内容级A/V、数学零静音或长稳。

~~~text
[2026-11-18 14:18:16.997] [info] [graph][runtime.node] av_reacquisition group=composition-rk510.source.2 event=purge_completed purge_ack=complete continuation=acquiring ack_count=2 old=1 next=2 transition=1
[2026-11-18 14:19:26.304] [info] [graph][runtime.node] av_reacquisition group=composition-rk510.source.3 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=1 next=2 transition=1
[2026-11-18 14:19:26.333] [info] [graph][runtime.node] av_reacquisition group=composition-rk510.source.2 event=purge_completed purge_ack=complete continuation=acquiring ack_count=2 old=2 next=3 transition=2
[2026-11-18 14:19:41.398] [info] [graph][runtime.node] av_reacquisition group=composition-rk510.source.0 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=1 next=2 transition=1
[2026-11-18 14:19:41.398] [info] [graph][runtime.node] av_reacquisition group=composition-rk510.source.1 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=1 next=2 transition=1
[2026-11-18 14:21:08.320] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:2 execution_mode=1 committed_batches=22345 committed_datagrams=127940 committed_payload_bytes=148568468 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=85 ordered_queue_maximum_wire_bytes=115260 pacing_reserved=127940 pacing_cancelled=0 pacing_submitted=127940 pacing_maximum_submit_lateness_ns=36549482 pacing_worst_late_global_sequence=303 pacing_worst_success_sample_available=1 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=85 backlog_high_water_wire_bytes=115260 backlog_max_residence_ns=46295826 last_materialized_sequence=127940 last_scheduled_sequence=127940 last_submitted_sequence=127940 last_committed_sequence=127940 submitted_service_generation=1 submitted_service_sequence=127940 submitted_service_completed_ns=208910884559 submitted_service_duration_ns=16640 submitted_service_available_ns=208910901199 submitted_scope_available_ns=208910901199 shared_scope_charged_datagrams=127940 shared_scope_charged_wire_bytes=152150788 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=1153 evidence_timestamp_tracked=0 evidence_timestamp_untracked=1153 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=554648 endpoint_1_target_effective_socket_bytes=277324 endpoint_1_api_requested_socket_bytes=138662 endpoint_1_effective_socket_bytes=277324 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=127885 endpoint_1_committed_payload_bytes=148564720 endpoint_2_target_effective_socket_bytes=277324 endpoint_2_api_requested_socket_bytes=138662 endpoint_2_effective_socket_bytes=277324 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=55 endpoint_2_committed_payload_bytes=3748
[2026-11-18 14:21:08.569] [info] [graph][runtime.lifecycle] stop.done state=Stopped
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=1379, threads=102, processThreads=116, workers=0, workerProgress=374536, workerProcessCalls=1437819, workerWaits=1063264, workerWakeups=1031438, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=821, logicalProcessors=8, averageSystemMachineCpuPercent=31.000576, averageProcessMachineCpuPercent=14.374747, peakProcessMachineCpuPercent=31.770833, averageProcessSingleCoreCpuPercent=114.997977, peakProcessSingleCoreCpuPercent=254.166667, initialWorkingSetBytes=46129152, workingSetBytes=65536000, peakWorkingSetBytes=65536000, totalPushed=328243, totalPopped=328243, droppedBuffers=0, encodedPacketsPushed=69072, encodedPacketsPopped=69072, backpressureItems=147, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=9147026, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=437, graphPayloadReservations=208828, graphPayloadReleases=208828, graphPayloadPressureFailures=4, lastInputReceivedAtNs=183386330445157
[CLI] realtime video validation stopped successfully
~~~

仍保留pressureFailures4和历史期限风险。

独立审查A：有限静音/恢复及late-BYE门禁PASS。离线解码输出null mux同时报三条DTS非单调：45056 >= 42935、43959、44982，故不声明时间戳无错，来源待定位。Windows同源码Release clean-first/all-target构建exit0；尚未完成对应真实流回归。

独立审查B同样有限PASS；TS输入还记录 DTS 119581 < 122999 out of order。现有证据跨生产输出、RTP接收、VLC重封装和离线解码，不足以定位生产音频缺陷；完整连续时序门禁未通过。

## 本轮清理清单

D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk510-audio-reentry.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk510-audio-reentry.out
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk510-audio.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk510-audio.out
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk510-frame-00001.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk510-frame-00901.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk510-frame-01801.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk510-frame-02701.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk510-frame-03601.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk510-frame-04501.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk510-frame-05401.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk510-received.ts
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk510-run.sh
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk510-silence.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk510-source2.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk510-source2.out
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk510-source3.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk510-source3.out
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk510-vlc.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk510-remote\cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk510-remote\cli-maps.txt
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk510-remote\cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk510-remote\monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk510-remote\output.sdp
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk510-remote\pids.txt
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk510-remote\result.txt
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk510-remote\runner.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk510-remote\socket-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk510-remote\source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk510-remote\source0.out
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk510-remote\source1.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk510-remote\source1.out
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk510-remote\udp-after.txt
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-rk510-remote\udp-before.txt

远端清理15项日志/SDP及acceptance/composition-rk510-run.sh，rk510目录已移除。两端本轮PID无残留；指定120秒源、正式库和回退产物保留。
