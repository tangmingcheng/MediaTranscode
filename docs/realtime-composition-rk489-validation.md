# rk489：解码器收尾修正后原规格四屏（FAIL，已归档）

源码b144507f，CLI SHA256 4f1249bd8958e13bc87715ae3bc744caeffe3da6a7fc6a51e558ca6d23bd68f4。远端launcher3497859；本机VLC39504、源2=26752、源3=37324。

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
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk489
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk489 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://192.168.130.229:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 192.168.96.122 --rtp-port 62024 --sdp /home/tang/MediaTranscode-composition-09093919/acceptance/rk489/output.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
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
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -vf "drawtext=fontfile=''C\:/Windows/Fonts/consola.ttf'':text=''frame=%{frame_num} pts=%{pts}'':fontsize=48:fontcolor=white:box=1:boxcolor=black:x=16:y=16" -c:v h264_nvenc -rc cbr -b:v 8200k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61020?rtcpport=61021&pkt_size=1200&localaddr=192.168.96.122" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://192.168.130.229:61022?rtcpport=61023&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk489-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk489-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE2_EXIT=$($p.ExitCode)"; exit $p.ExitCode
~~~

~~~powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61050?rtcpport=61051&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk489-source3.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk489-source3.log' -PassThru; Write-Output "SOURCE3_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE3_EXIT=$($p.ExitCode)"; exit $p.ExitCode
~~~

~~~powershell
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk489-vlc.log --extraintf=rc --rc-host=127.0.0.1:42120 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk489-native- --snapshot-format=png rtp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "VLC_EXIT=$($p.ExitCode)"
~~~

远端启动：bash -n acceptance/rk489/run.sh && bash acceptance/rk489/run.sh >acceptance/rk489/launcher.log 2>&1 &。



## 结果与诊断

远端源0=3497863、源1=3497864、CLI3497865、monitor3497867。四源120秒均exit0；CLI自然exit1。旧滤镜及解码器租约错误未复现；新首错AudioResampleNode74 external audio correction requires a contiguous command，取消后AudioEncode错误不是首错。未通过全黑/重入验收。外部120样本，CPU最大157%单核口径，RSS最大74876KiB。

播放器证据受污染：普通工具环境未看见上轮VLC29180，原rk488无进程残留声明不成立。本轮39504与其同时存在；snapshot命令实际生成rk488前缀的12h39m06s416、12h39m38s253两张图片，不能作为本轮新播放器证据。已在提权环境核对CommandLine后按PID清理29180及39504，并复核无VLC/FFmpeg残留；不将清理算自然退出。后续必须同一可见真实进程环境执行启动前后端口/PID检查。

~~~text
[2026-11-17 12:39:56.697] [info] [graph][runtime.node] prepared_rtp_source_closing generation=1 video_deadline_ns=90945533769822 audio_deadline_ns=none
[2026-11-17 12:39:56.699] [info] [graph][runtime.node] prepared_rtp_source_closing generation=1 video_deadline_ns=90945486589701 audio_deadline_ns=none
[2026-11-17 12:39:58.693] [info] [graph][runtime.node] prepared_rtp_input_sealed stream=1 generation=1 ingress_generation=0 frontier=3625
[2026-11-17 12:39:58.697] [info] [graph][runtime.node] prepared_rtp_input_sealed stream=1 generation=1 ingress_generation=0 frontier=3625
[2026-11-17 12:40:04.207] [info] [graph][runtime.node] prepared_rtp_source_closing generation=1 video_deadline_ns=90953094389691 audio_deadline_ns=90953094389691
[2026-11-17 12:40:04.408] [info] [graph][runtime.node] prepared_rtp_input_sealed stream=1 generation=1 ingress_generation=0 frontier=3625
[2026-11-17 12:40:05.978] [info] [graph][runtime.node] prepared_rtp_source_closing generation=1 video_deadline_ns=90954914964733 audio_deadline_ns=none
[2026-11-17 12:40:06.132] [info] [graph][runtime.node] prepared_rtp_input_sealed stream=2 generation=1 ingress_generation=0 frontier=5193
[2026-11-17 12:40:06.135] [info] [graph][runtime.node] process.failed node=74 name=AudioResampleNode kind=AudioResample inputs=[channel=91 edge=91 stream=Audio edge_kind=Metadata payload=CodecContext queued=0; channel=94 edge=94 stream=Audio edge_kind=RawFrame payload=Frame queued=0; channel=95 edge=95 stream=Audio edge_kind=Event payload=GraphEvent queued=0] outputs=[channel=98 edge=98 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0] error=NotInitialized: external audio correction requires a contiguous command
[2026-11-17 12:40:06.142] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:2 execution_mode=1 committed_batches=12896 committed_datagrams=117086 committed_payload_bytes=145747528 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=85 ordered_queue_maximum_wire_bytes=115260 pacing_reserved=117086 pacing_cancelled=0 pacing_submitted=117086 pacing_maximum_submit_lateness_ns=43018710 pacing_worst_late_global_sequence=10728 pacing_worst_success_sample_available=1 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=85 backlog_high_water_wire_bytes=115260 backlog_max_residence_ns=57323337 last_materialized_sequence=117086 last_scheduled_sequence=117086 last_submitted_sequence=117086 last_committed_sequence=117086 submitted_service_generation=1 submitted_service_sequence=117086 submitted_service_completed_ns=128931117569 submitted_service_duration_ns=96640 submitted_service_available_ns=128931214209 submitted_scope_available_ns=128931214209 shared_scope_charged_datagrams=117086 shared_scope_charged_wire_bytes=149025936 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=1153 evidence_timestamp_tracked=0 evidence_timestamp_untracked=1153 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=554648 endpoint_1_target_effective_socket_bytes=277324 endpoint_1_api_requested_socket_bytes=138662 endpoint_1_effective_socket_bytes=277324 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=117055 endpoint_1_committed_payload_bytes=145745420 endpoint_2_target_effective_socket_bytes=277324 endpoint_2_api_requested_socket_bytes=138662 endpoint_2_effective_socket_bytes=277324 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=31 endpoint_2_committed_payload_bytes=2108
[2026-11-17 12:40:06.154] [info] [graph][runtime.node] process.failed node=4 name=AudioEncodeNode kind=AudioEncode inputs=[channel=2 edge=2 stream=Audio edge_kind=Metadata payload=CodecContext queued=0; channel=128 edge=128 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0] outputs=[channel=3 edge=3 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0; channel=138 edge=138 stream=Audio edge_kind=Metadata payload=CodecContext queued=0] error=Cancelled: graph payload credit waiters were cancelled
[CLI] final runtime report: queued=0, peakQueued=1559, threads=102, processThreads=132, workers=0, workerProgress=336821, workerProcessCalls=1386655, workerWaits=1049825, workerWakeups=1037742, workerErrors=1, errors=1, stalledIntervals=0, cpuSamples=472, logicalProcessors=8, averageSystemMachineCpuPercent=38.086001, averageProcessMachineCpuPercent=22.098987, peakProcessMachineCpuPercent=47.783251, averageProcessSingleCoreCpuPercent=176.791898, peakProcessSingleCoreCpuPercent=382.266010, initialWorkingSetBytes=61677568, workingSetBytes=76673024, peakWorkingSetBytes=76673024, totalPushed=308789, totalPopped=308711, droppedBuffers=0, encodedPacketsPushed=39760, encodedPacketsPopped=39759, backpressureItems=147, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=15219131, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=865, graphPayloadReservations=168861, graphPayloadReleases=168861, graphPayloadPressureFailures=9, lastInputReceivedAtNs=90954665177069
[2026-11-17 12:39:59.911] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4975 source_master_ns=125869165857 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=231425 effective_output_sample_index=5322775
~~~

清理清单：本机composition-rk488-native-2026-11-17-12h39m06s416.png、composition-rk488-native-2026-11-17-12h39m38s253.png、composition-rk489-source2.log、composition-rk489-source2.out、composition-rk489-source3.log、composition-rk489-source3.out、composition-rk489-vlc.log、rk489-cli.log、rk489-run.sh；远端acceptance/rk489/{socket-monitor.log,udp-after.txt,output.sdp,source0.out,cli-error.log,monitor.log,result.txt,pids.txt,launcher.log,run.sh,cli.log,source1.out,source0.log,udp-before.txt,source1.log,cli-maps.txt}及out/acceptance/rk489-run.sh。

清理复核：本机与远端上述临时清单已空，远端精确PID无残留；本机以提权CIM复核无VLC/FFmpeg。保留复用源及回退。
