# rk488：滤镜收尾修正后原规格四屏（FAIL，已归档）

源码9debd459，CLI SHA256 4cb8f05eccba1ab2beb73b9db256b15271ddc7e5cbd90d67c019e5dc1b2eb72a。远端launcher3451674；本机VLC29180、源2=42708、源3=41980。

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
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk488
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk488 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://192.168.130.229:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 192.168.96.122 --rtp-port 62024 --sdp /home/tang/MediaTranscode-composition-09093919/acceptance/rk488/output.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
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
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -vf "drawtext=fontfile=''C\:/Windows/Fonts/consola.ttf'':text=''frame=%{frame_num} pts=%{pts}'':fontsize=48:fontcolor=white:box=1:boxcolor=black:x=16:y=16" -c:v h264_nvenc -rc cbr -b:v 8200k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61020?rtcpport=61021&pkt_size=1200&localaddr=192.168.96.122" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://192.168.130.229:61022?rtcpport=61023&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk488-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk488-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE2_EXIT=$($p.ExitCode)"; exit $p.ExitCode
~~~

~~~powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61050?rtcpport=61051&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk488-source3.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk488-source3.log' -PassThru; Write-Output "SOURCE3_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE3_EXIT=$($p.ExitCode)"; exit $p.ExitCode
~~~

~~~powershell
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk488-vlc.log --extraintf=rc --rc-host=127.0.0.1:42120 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk488-native- --snapshot-format=png rtp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "VLC_EXIT=$($p.ExitCode)"
~~~

远端启动：bash -n acceptance/rk488/run.sh && bash acceptance/rk488/run.sh >acceptance/rk488/launcher.log 2>&1 &。



## 结果与诊断

远端源0=3451681、源1=3451682、CLI3451683、monitor3451685。四源120秒均exit0；CLI自然exit1，无强制结束或挂起。首错VideoDecodeNode90（HEVC源）在收尾时Codec flush left live lineage leases；旧VideoFilter26/45错误未复现。取消后的AudioEncode错误不是首错。发送117481个datagram、146271732 payload bytes，deadline_misses=0、would_block=0、最终backlog=0，delivery_evidence=not_proven。全失源黑屏与重入未完成，不计通过。

VLC原生快照composition-rk488-native-2026-11-17-12h28m15s095.png已人工查看四格，左下frame1016/pts33.866667；两机时钟不同，快照墙钟不能直接对齐远端日志。

~~~text
[2026-11-17 12:28:59.833] [info] [graph][runtime.node] prepared_rtp_source_closing generation=1 video_deadline_ns=90288619922620 audio_deadline_ns=none
[2026-11-17 12:28:59.834] [info] [graph][runtime.node] prepared_rtp_source_closing generation=1 video_deadline_ns=90288670865009 audio_deadline_ns=none
[2026-11-17 12:28:59.933] [info] [graph][runtime.node] prepared_rtp_input_sealed stream=1 generation=1 ingress_generation=0 frontier=3625
[2026-11-17 12:29:01.831] [info] [graph][runtime.node] prepared_rtp_input_sealed stream=1 generation=1 ingress_generation=0 frontier=3625
[2026-11-17 12:29:05.748] [info] [graph][runtime.node] prepared_rtp_source_closing generation=1 video_deadline_ns=90294635237807 audio_deadline_ns=90294635237807
[2026-11-17 12:29:05.949] [info] [graph][runtime.node] prepared_rtp_input_sealed stream=1 generation=1 ingress_generation=0 frontier=3625
[2026-11-17 12:29:06.968] [info] [graph][runtime.node] prepared_rtp_source_closing generation=1 video_deadline_ns=90295905519871 audio_deadline_ns=none
[2026-11-17 12:29:07.219] [info] [graph][runtime.node] prepared_rtp_input_sealed stream=1 generation=1 ingress_generation=0 frontier=3625
[2026-11-17 12:29:07.221] [info] [graph][runtime.node] process.failed node=90 name=VideoDecodeNode kind=VideoDecode inputs=[channel=116 edge=116 stream=Video edge_kind=Metadata payload=CodecContext queued=0; channel=117 edge=117 stream=Video edge_kind=InputPacket payload=Packet queued=0] outputs=[channel=118 edge=118 stream=Video edge_kind=RawFrame payload=Frame queued=0] error=InvalidArgument: Codec flush left live lineage leases
[2026-11-17 12:29:07.228] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:2 execution_mode=1 committed_batches=12902 committed_datagrams=117481 committed_payload_bytes=146271732 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=85 ordered_queue_maximum_wire_bytes=115260 pacing_reserved=117481 pacing_cancelled=0 pacing_submitted=117481 pacing_maximum_submit_lateness_ns=32399270 pacing_worst_late_global_sequence=111424 pacing_worst_success_sample_available=1 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=85 backlog_high_water_wire_bytes=115260 backlog_max_residence_ns=48053011 last_materialized_sequence=117481 last_scheduled_sequence=117481 last_submitted_sequence=117481 last_committed_sequence=117481 submitted_service_generation=1 submitted_service_sequence=117481 submitted_service_completed_ns=126901135243 submitted_service_duration_ns=36480 submitted_service_available_ns=126901171723 submitted_scope_available_ns=126901171723 shared_scope_charged_datagrams=117481 shared_scope_charged_wire_bytes=149561200 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=1153 evidence_timestamp_tracked=0 evidence_timestamp_untracked=1153 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=554648 endpoint_1_target_effective_socket_bytes=277324 endpoint_1_api_requested_socket_bytes=138662 endpoint_1_effective_socket_bytes=277324 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=117453 endpoint_1_committed_payload_bytes=146269828 endpoint_2_target_effective_socket_bytes=277324 endpoint_2_api_requested_socket_bytes=138662 endpoint_2_effective_socket_bytes=277324 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=28 endpoint_2_committed_payload_bytes=1904
[2026-11-17 12:29:07.234] [info] [graph][runtime.node] process.failed node=4 name=AudioEncodeNode kind=AudioEncode inputs=[channel=2 edge=2 stream=Audio edge_kind=Metadata payload=CodecContext queued=0; channel=128 edge=128 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0] outputs=[channel=3 edge=3 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0; channel=138 edge=138 stream=Audio edge_kind=Metadata payload=CodecContext queued=0] error=Cancelled: graph payload credit waiters were cancelled
[CLI] final runtime report: queued=0, peakQueued=1424, threads=102, processThreads=132, workers=0, workerProgress=337143, workerProcessCalls=1390880, workerWaits=1053728, workerWakeups=1041030, workerErrors=1, errors=1, stalledIntervals=0, cpuSamples=473, logicalProcessors=8, averageSystemMachineCpuPercent=37.967802, averageProcessMachineCpuPercent=22.018452, peakProcessMachineCpuPercent=42.564103, averageProcessSingleCoreCpuPercent=176.147619, peakProcessSingleCoreCpuPercent=340.512821, initialWorkingSetBytes=57196544, workingSetBytes=71864320, peakWorkingSetBytes=71864320, totalPushed=309082, totalPopped=309080, droppedBuffers=0, encodedPacketsPushed=39788, encodedPacketsPopped=39787, backpressureItems=147, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=14245826, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=701, graphPayloadReservations=172634, graphPayloadReleases=172634, graphPayloadPressureFailures=2, lastInputReceivedAtNs=90295655712374
[2026-11-17 12:29:03.478] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=5063 source_master_ns=125703170213 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=225281 effective_output_sample_index=5406744
~~~

外部监控119样本，CPU最大159%（单核口径），RSS最大75004KiB。清理清单：本机composition-rk488-native-2026-11-17-12h28m15s095.png、composition-rk488-source2.log/.out、composition-rk488-source3.log/.out、composition-rk488-vlc.log、rk488-cli.log、rk488-run.sh；远端acceptance/rk488中的socket-monitor.log、udp-after.txt、output.sdp、source0.out、cli-error.log、monitor.log、result.txt、pids.txt、launcher.log、run.sh、cli.log、source1.out、source0.log、udp-before.txt、source1.log、cli-maps.txt及out/acceptance/rk488-run.sh。远端记录PID均已结束；本机源进程自然exit0，VLC测试结束后清理，不算自然退出。

清理复核：上述两侧临时文件清单及本轮PID均无残留，复用源和回退产物保留。
