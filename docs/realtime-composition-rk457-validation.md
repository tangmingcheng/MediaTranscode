# rk457：BYE 前已绑定音频缓存边界取证

状态：诊断取证成功，证实BYE清理已绑定待播放媒体；尾部修复尚未实施。源码13a7d57c仅加有界诊断，双独立SOURCE PASS；RKMPP8核增量8项exit0，CLI SHA256=6b5cb59a7394d62c2560c06197f06ab881222c7caa73e07c5563e110081c2dd1。外部依赖均不变，Windows FFmpeg不改，回退out/rollback/rk457保留。

规格同rk456：四源120秒1280×720/30fps，H264复制两路、选中H264 CBR8200kbps/AAC复制、HEVC CBR8000kbps；输出H264 VBR目标8/范围8–12Mbps、AAC192kbps MPEG-TS/RTP。不得将诊断通过等同尾部修复。

## 真实命令

目标脚本已验证LF且bash语法检查通过：

```bash
#!/usr/bin/env bash
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
export PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin:$PATH
export LD_LIBRARY_PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib:/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib:/opt/mt-tools/gcc-12.4-native/lib64
set -u
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk457
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk457 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://192.168.130.229:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 192.168.96.122 --rtp-port 62024 --sdp /home/tang/MediaTranscode-composition-09093919/acceptance/rk457/output.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
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

```bash
bash acceptance/rk457/run.sh >acceptance/rk457/launcher.log 2>&1 &
```

```powershell
$p=Start-Process -FilePath 'D:/Wireshark/dumpcap.exe' -ArgumentList '-i 7 -f "udp and (dst port 61020 or dst port 61021 or dst port 61022 or dst port 61023 or dst port 62024 or dst port 62025)" -a duration:420 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk457-output.pcapng' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk457-capture.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk457-capture.log' -PassThru; Write-Output "CAPTURE_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "CAPTURE_EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -vf "drawtext=fontfile=''C\:/Windows/Fonts/consola.ttf'':text=''frame=%{frame_num} pts=%{pts}'':fontsize=48:fontcolor=white:box=1:boxcolor=black:x=16:y=16" -c:v h264_nvenc -rc cbr -b:v 8200k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61020?rtcpport=61021&pkt_size=1200&localaddr=192.168.96.122" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://192.168.130.229:61022?rtcpport=61023&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk457-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk457-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE2_EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61050?rtcpport=61051&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk457-source3.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk457-source3.log' -PassThru; Write-Output "SOURCE3_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE3_EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk457-vlc.log --extraintf=rc --rc-host=127.0.0.1:42120 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk457-native- --snapshot-format=png rtp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "VLC_EXIT=$($p.ExitCode)"
```

PID：目标wrapper3046341、source0/1=3046343/3046344、CLI3046345、monitor3046347；本机capture43628、source2/3=44484/38140、VLC33928。108秒RSS78036KiB、CPU151%单核口径；19h04m34s073原生VLC快照已观察四格画面。

## 取证结果

四路原规格源完整120秒均退出0，VLC两张原生快照已确认四格及全黑；CLI经kill -INT 3046345正常caller_stop/exit0。capture241107包、接口drop0，未发生sender期限失败，不关闭rk454。19:05:12.605聚合器在purge前拥有55个旧代候选/56320样本，映射区间5225635..5281955，next_output5226496，pending_cancelled0。存在尚未播放的已绑定媒体被真实清空；头部可能已消费，不能把56320都计成丢失量。没有新增永久状态或媒体行为修改。

~~~text
[2026-11-16 19:05:12.605] [info] [graph][runtime.node] continuous_audio_source_purge stage=before_clear node=7 source=composition-rk457.source.2 old_generation=1 next_generation=2 transition=1 published_generation=1 owner_candidates=55 owner_candidate_samples=56320 output_sample_rate=44100 next_output_sample=5226496 committed_real_samples=5225635 pending_cancelled=0 interval_semantics=envelope_not_continuity pending_may_overlap_candidates=1 old_original_count=55 old_original_sample_sum=56320 old_original_incomplete=0 old_original_envelope_begin=5225728 old_original_envelope_end=5282048 old_mapped_count=55 old_mapped_sample_sum=56320 old_mapped_incomplete=0 old_mapped_envelope_begin=5225635 old_mapped_envelope_end=5281955 pending_real_output_count=0 pending_real_output_sample_sum=0 pending_real_output_incomplete=0 pending_real_output_envelope=unavailable
[2026-11-16 19:06:41.400] [info] [graph][runtime.node] datagram_batch_trace stage=last_completed generation=1 first_sequence=126708 last_sequence=126708 maximum_submit_interval_ns=52209 maximum_submit_interval_sequence=126708 maximum_start_lateness_ns=251713 maximum_start_lateness_sequence=126708 first_release_master_ns=217162194854 first_deadline_master_ns=217262194854 last_release_master_ns=217162194854 last_deadline_master_ns=217262194854 first_submitted_master_ns=217162498776 last_submitted_master_ns=217162498776 worst_wait_started_master_ns=217162445692 worst_wait_completed_master_ns=217162445692 worst_submit_observed_master_ns=217162446567 wait_attempts=0 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=0 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=0 wait_call_total_ns=0 wait_call_max_ns=0 wait_before_call_count=0 wait_before_call_total_ns=0 wait_before_call_max_ns=0 wait_arm_count=0 wait_arm_total_ns=0 wait_arm_max_ns=0 wait_api_wall_count=0 wait_api_wall_total_ns=0 wait_api_wall_max_ns=0 wait_return_to_next_clock_count=0 wait_return_to_next_clock_total_ns=0 wait_return_to_next_clock_max_ns=0 wait_target_lateness_count=0 wait_target_lateness_total_ns=0 wait_target_lateness_max_ns=0
[2026-11-16 19:06:41.400] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:2 execution_mode=1 committed_batches=22475 committed_datagrams=126708 committed_payload_bytes=147652432 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=85 ordered_queue_maximum_wire_bytes=115260 pacing_reserved=126708 pacing_cancelled=0 pacing_submitted=126708 pacing_maximum_submit_lateness_ns=30612541 pacing_worst_late_global_sequence=61382 pacing_worst_success_sample_available=1 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=85 backlog_high_water_wire_bytes=115260 backlog_max_residence_ns=60319991 last_materialized_sequence=126708 last_scheduled_sequence=126708 last_submitted_sequence=126708 last_committed_sequence=126708 submitted_service_generation=1 submitted_service_sequence=126708 submitted_service_completed_ns=217162498776 submitted_service_duration_ns=16640 submitted_service_available_ns=217162515416 submitted_scope_available_ns=217162515416 shared_scope_charged_datagrams=126708 shared_scope_charged_wire_bytes=151200256 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=1153 evidence_timestamp_tracked=0 evidence_timestamp_untracked=1153 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=554648 endpoint_1_target_effective_socket_bytes=277324 endpoint_1_api_requested_socket_bytes=138662 endpoint_1_effective_socket_bytes=277324 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=126656 endpoint_1_committed_payload_bytes=147648888 endpoint_2_target_effective_socket_bytes=277324 endpoint_2_api_requested_socket_bytes=138662 endpoint_2_effective_socket_bytes=277324 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=52 endpoint_2_committed_payload_bytes=3544
[2026-11-16 19:06:41.591] [info] [graph][runtime.node] continuous_audio_aggregate.summary reason=stop output_group=composition-rk457.output audio_commits=8925 real_samples=5225635 silence_samples=3913565
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=1383, threads=102, processThreads=148, workers=0, workerProgress=408088, workerProcessCalls=1861726, workerWaits=1453619, workerWakeups=1423461, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=824, logicalProcessors=8, averageSystemMachineCpuPercent=34.586617, averageProcessMachineCpuPercent=14.181217, peakProcessMachineCpuPercent=40.816327, averageProcessSingleCoreCpuPercent=113.449739, peakProcessSingleCoreCpuPercent=326.530612, initialWorkingSetBytes=67145728, workingSetBytes=79908864, peakWorkingSetBytes=79908864, totalPushed=363263, totalPopped=363074, droppedBuffers=0, encodedPacketsPushed=69503, encodedPacketsPopped=69503, backpressureItems=147, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=16015171, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=782, graphPayloadReservations=199246, graphPayloadReleases=199246, graphPayloadPressureFailures=7, lastInputReceivedAtNs=27661306479909
[2026-11-16 19:05:07.396] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4938 source_master_ns=124668828043 raw_phase_ns=9071 filtered_phase_ns=9070 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=240641 effective_output_sample_index=5294102
~~~

## 接收完整性与清理

输出126656媒体RTP/777282 TS包/146129016字节，RTP序号/TS CC/TEI错误均0。选中输入3600视频AU/5168音频AU、50SR/2BYE、序号缺口0。外部monitor202样本，RSS17120–79156KiB；pressureFailures7保留，range变化警告仍有，短程不证明长期稳定。本轮构建日志8项/exit0已记录，源代码包与临时脚本一并清理，out/rollback/rk457保留。

~~~text
[2026-11-16 19:05:03.710] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=27652397335319
[2026-11-16 19:05:03.711] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=30 group=composition-rk457.source.1 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=24 video_raw_generation=0 video_ssrc=951348391
[2026-11-16 19:05:03.739] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=27652426472206
[2026-11-16 19:05:03.740] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=11 group=composition-rk457.source.0 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=24 video_raw_generation=0 video_ssrc=3571198653
[2026-11-16 19:05:03.741] [info] [graph][runtime.node] rtp_binder_purge node=30 transition=1 cutoff=3625
[2026-11-16 19:05:03.780] [info] [graph][runtime.node] rtp_binder_purge node=11 transition=1 cutoff=3625
[2026-11-16 19:05:10.104] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=27658790662641
[2026-11-16 19:05:10.104] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=78 group=composition-rk457.source.3 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=24 video_raw_generation=0 video_ssrc=1809010365
[2026-11-16 19:05:10.104] [info] [graph][runtime.node] rtp_binder_purge node=78 transition=1 cutoff=3625
[2026-11-16 19:05:12.603] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=27661289533462
[2026-11-16 19:05:12.603] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=50 group=composition-rk457.source.2 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=48 video_raw_generation=0 video_ssrc=1973230968 audio_raw_generation=0 audio_ssrc=2327336303
[2026-11-16 19:05:12.603] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=51 group=composition-rk457.source.2 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=48 video_raw_generation=0 video_ssrc=1973230968 audio_raw_generation=0 audio_ssrc=2327336303
[2026-11-16 19:05:12.603] [info] [graph][runtime.node] av_reacquisition group=composition-rk457.source.2 phase=purging old=1 next=2 transition=1
[2026-11-16 19:05:12.605] [info] [graph][runtime.node] rtp_binder_purge node=50 transition=1 cutoff=3612
[2026-11-16 19:05:12.606] [info] [graph][runtime.node] rtp_binder_purge node=51 transition=1 cutoff=5193
[2026-11-16 19:05:12.606] [info] [graph][runtime.node] av_reacquisition group=composition-rk457.source.2 event=purge_completed purge_ack=complete continuation=acquiring ack_count=2 old=1 next=2 transition=1
[2026-11-16 19:05:12.607] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=27661289541921
~~~

本机逐项清理清单（18项）：
~~~text
composition-rk457-capture.log
composition-rk457-capture.out
composition-rk457-cli.log
composition-rk457-input-map.json
composition-rk457-input.aac
composition-rk457-input.h264
composition-rk457-native-2026-11-16-19h04m34s073.png
composition-rk457-native-2026-11-16-19h07m07s730.png
composition-rk457-output.pcapng
composition-rk457-output.ts
composition-rk457-source2.log
composition-rk457-source2.out
composition-rk457-source3.log
composition-rk457-source3.out
composition-rk457-vlc.log
composition-rk457-wire.json
rk457-run.sh
rk457-source.tar
~~~
远端acceptance/rk457共16项：cli-error.log, cli-maps.txt, cli.log, launcher.log, monitor.log, output.sdp, pids.txt, result.txt, run.sh, socket-monitor.log, source0.log, source0.out, source1.log, source1.out, udp-after.txt, udp-before.txt；另清out/rk457-source.tar、out/rk457-run.sh、out/rk457-build.log、out/rk457-build.exit。保留指定120秒源、正式构建及回退。

清理复核：本机18项、远端20项共38项临时文件全部清除，5个目标和4个本机精确PID均无残留。原源、构建与rk457回退保留。
