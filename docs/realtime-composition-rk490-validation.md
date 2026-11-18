# rk490：音频关闭阶段诊断（FAIL，根因确证）

源码8b4f1e5c；CLI SHA256 108e3eeceeb41c8ea572bca061761fee9de1af1a80edb0ce5aa4b6dc9aa15bb0。远端launcher3548141；本机VLC35360、源2=12584、源3=42816。

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
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk490
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk490 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://192.168.130.229:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 192.168.96.122 --rtp-port 62024 --sdp /home/tang/MediaTranscode-composition-09093919/acceptance/rk490/output.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
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
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -vf "drawtext=fontfile=''C\:/Windows/Fonts/consola.ttf'':text=''frame=%{frame_num} pts=%{pts}'':fontsize=48:fontcolor=white:box=1:boxcolor=black:x=16:y=16" -c:v h264_nvenc -rc cbr -b:v 8200k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61020?rtcpport=61021&pkt_size=1200&localaddr=192.168.96.122" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://192.168.130.229:61022?rtcpport=61023&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk490-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk490-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE2_EXIT=$($p.ExitCode)"; exit $p.ExitCode
~~~

~~~powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61050?rtcpport=61051&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk490-source3.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk490-source3.log' -PassThru; Write-Output "SOURCE3_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE3_EXIT=$($p.ExitCode)"; exit $p.ExitCode
~~~

~~~powershell
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk490-vlc.log --extraintf=rc --rc-host=127.0.0.1:42120 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk490-native- --snapshot-format=png rtp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "VLC_EXIT=$($p.ExitCode)"
~~~

远端启动：bash -n acceptance/rk490/run.sh && bash acceptance/rk490/run.sh >acceptance/rk490/launcher.log 2>&1 &。



## 结果与根因

源0=3548145、源1=3548146、CLI3548147、monitor3548149。四源120秒exit0，CLI自然exit1。node74实际输出尾部17样本后，exhausted及settled成功；随后correction_close触发drain_rearm，prepare_failed。确证关闭标记错误重启排尽，不是首次drain缺命令；不得据此宣称其他场景均无窗口缺失。外部120样本CPU最大158%单核口径，RSS最大74072KiB。

VLC TCP42120与UDP62024均核对PID35360；原生快照composition-rk490-native-2026-11-17-12h51m05s851.png人工查看四格，左下frame1261/pts42.033333。两机墙钟不同，不能直接对齐。全黑/重入未完成。

~~~text
[2026-11-17 12:51:42.356] [info] [graph][runtime.node] prepared_rtp_input_sealed stream=1 generation=1 ingress_generation=0 frontier=3625
[2026-11-17 12:51:42.360] [info] [graph][runtime.node] prepared_rtp_input_sealed stream=1 generation=1 ingress_generation=0 frontier=3625
[2026-11-17 12:51:47.894] [info] [graph][runtime.node] prepared_rtp_input_sealed stream=1 generation=1 ingress_generation=0 frontier=3625
[2026-11-17 12:51:49.627] [info] [graph][runtime.node] prepared_rtp_input_sealed stream=2 generation=1 ingress_generation=0 frontier=5193
[2026-11-17 12:51:49.629] [info] [graph][runtime.node] audio_terminal_trace node=74 stage=first_drain generation=1 output_sample_index=5280416 draining_eof=1 correction_closed=0 requires_next_window=0
[2026-11-17 12:51:49.629] [info] [graph][runtime.node] audio_terminal_trace node=74 stage=exhausted generation=1 output_sample_index=5280433 draining_eof=0 correction_closed=0 requires_next_window=0
[2026-11-17 12:51:49.629] [info] [graph][runtime.node] audio_terminal_trace node=74 stage=settled generation=1 output_sample_index=5280433 draining_eof=0 correction_closed=0 requires_next_window=1
[2026-11-17 12:51:49.629] [info] [graph][runtime.node] audio_terminal_trace node=74 stage=correction_close generation=1 output_sample_index=5280433 draining_eof=0 correction_closed=1 requires_next_window=1
[2026-11-17 12:51:49.629] [info] [graph][runtime.node] audio_terminal_trace node=74 stage=drain_rearm generation=1 output_sample_index=5280433 draining_eof=0 correction_closed=1 requires_next_window=1
[2026-11-17 12:51:49.629] [info] [graph][runtime.node] audio_terminal_trace node=74 stage=prepare_failed generation=1 output_sample_index=5280433 draining_eof=1 correction_closed=1 requires_next_window=1
[2026-11-17 12:51:49.629] [info] [graph][runtime.node] process.failed node=74 name=AudioResampleNode kind=AudioResample inputs=[channel=91 edge=91 stream=Audio edge_kind=Metadata payload=CodecContext queued=0; channel=94 edge=94 stream=Audio edge_kind=RawFrame payload=Frame queued=0; channel=95 edge=95 stream=Audio edge_kind=Event payload=GraphEvent queued=0] outputs=[channel=98 edge=98 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0] error=NotInitialized: external audio correction requires a contiguous command
[2026-11-17 12:51:49.632] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:2 execution_mode=1 committed_batches=12896 committed_datagrams=116803 committed_payload_bytes=145367776 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=85 ordered_queue_maximum_wire_bytes=115260 pacing_reserved=116803 pacing_cancelled=0 pacing_submitted=116803 pacing_maximum_submit_lateness_ns=33196605 pacing_worst_late_global_sequence=81720 pacing_worst_success_sample_available=1 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=85 backlog_high_water_wire_bytes=115260 backlog_max_residence_ns=48036909 last_materialized_sequence=116803 last_scheduled_sequence=116803 last_submitted_sequence=116803 last_committed_sequence=116803 submitted_service_generation=1 submitted_service_sequence=116803 submitted_service_completed_ns=128757738793 submitted_service_duration_ns=96640 submitted_service_available_ns=128757835433 submitted_scope_available_ns=128757835433 shared_scope_charged_datagrams=116803 shared_scope_charged_wire_bytes=148638260 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=1153 evidence_timestamp_tracked=0 evidence_timestamp_untracked=1153 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=554648 endpoint_1_target_effective_socket_bytes=277324 endpoint_1_api_requested_socket_bytes=138662 endpoint_1_effective_socket_bytes=277324 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=116775 endpoint_1_committed_payload_bytes=145365872 endpoint_2_target_effective_socket_bytes=277324 endpoint_2_api_requested_socket_bytes=138662 endpoint_2_effective_socket_bytes=277324 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=28 endpoint_2_committed_payload_bytes=1904
[2026-11-17 12:51:49.645] [info] [graph][runtime.node] process.failed node=4 name=AudioEncodeNode kind=AudioEncode inputs=[channel=2 edge=2 stream=Audio edge_kind=Metadata payload=CodecContext queued=0; channel=128 edge=128 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0] outputs=[channel=3 edge=3 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0; channel=138 edge=138 stream=Audio edge_kind=Metadata payload=CodecContext queued=0] error=Cancelled: graph payload credit waiters were cancelled
[CLI] final runtime report: queued=0, peakQueued=1617, threads=102, processThreads=132, workers=0, workerProgress=336880, workerProcessCalls=1376395, workerWaits=1039506, workerWakeups=1026945, workerErrors=1, errors=1, stalledIntervals=0, cpuSamples=472, logicalProcessors=8, averageSystemMachineCpuPercent=37.686081, averageProcessMachineCpuPercent=22.289186, peakProcessMachineCpuPercent=59.116022, averageProcessSingleCoreCpuPercent=178.313491, peakProcessSingleCoreCpuPercent=472.928177, initialWorkingSetBytes=61595648, workingSetBytes=75849728, peakWorkingSetBytes=75849728, totalPushed=308837, totalPopped=308770, droppedBuffers=0, encodedPacketsPushed=39783, encodedPacketsPopped=39782, backpressureItems=147, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=15225703, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=857, graphPayloadReservations=169725, graphPayloadReleases=169725, graphPayloadPressureFailures=2, lastInputReceivedAtNs=91658081980630
[2026-11-17 12:51:43.387] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4975 source_master_ns=125702976598 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=231425 effective_output_sample_index=5322775
~~~

清理清单：本机composition-rk490-native-2026-11-17-12h51m05s851.png、composition-rk490-source2.log、composition-rk490-source2.out、composition-rk490-source3.log、composition-rk490-source3.out、composition-rk490-vlc.log、rk490-cli.log、rk490-run.sh；远端acceptance/rk490/{socket-monitor.log,udp-after.txt,output.sdp,source0.out,cli-error.log,monitor.log,result.txt,pids.txt,launcher.log,run.sh,cli.log,source1.out,source0.log,udp-before.txt,source1.log,cli-maps.txt}及out/acceptance/rk490-run.sh。VLC按已核对命令行的PID35360清理，不记自然退出。

清理复核：两侧临时文件清单为空；远端精确PID均结束，本机提权CIM复核无VLC/FFmpeg。复用源及回退保留。
