# rk450：RKMPP 最新共享停止版本活动输出回归

状态：本轮四源功能与正常停止回归通过；整体交付未完成。源码 ce71265d；CLI SHA256 ab5329a1ffa9d810e4f525ae467bf7c14c5cb205d1b1131ef462d32efc4eacfe。原四源120秒 H264/H264/H264+AAC/HEVC RTP → H264 1280×720/30fps VBR 8–12Mbps、AAC192kbps MPEG-TS/RTP。检查四格、全失源连续黑屏及调用方停止；不将此轮代替完整内容同步或长期可靠性门禁。Windows FFmpeg未修改。

## 远端临时脚本与启动命令

```bash
#!/usr/bin/env bash
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
export PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin:$PATH
export LD_LIBRARY_PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib:/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib:/opt/mt-tools/gcc-12.4-native/lib64
set -u
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk450
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200' -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye 'rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200' >source2.out 2>source2.log &
s2=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk450 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 192.168.96.122 --rtp-port 62024 --sdp /home/tang/MediaTranscode-composition-09093919/acceptance/rk450/output.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
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

bash acceptance/rk450/run.sh >acceptance/rk450/launcher.log 2>&1 &
```

## Windows 实际命令

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61050?rtcpport=61051&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk450-source3.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk450-source3.log' -PassThru; Write-Output "SOURCE3_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE3_EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk450-vlc.log --extraintf=rc --rc-host=127.0.0.1:42120 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk450-native- --snapshot-format=png rtp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "VLC_EXIT=$($p.ExitCode)"
```

启动身份：远端启动shell2521871，source0/1/2=2521885/2521886/2521887，CLI2521888，monitor2521890；Windows HEVC源33092，VLC44268。首次VLC原生快照17h47m19s424已观察四格有画面。运行49秒时errors/workerErrors=0，pressureFailures=3，需保留此计数；内部相位22676ns、频率0ppm，不是内容同步结论。

## 实测结果

全部4源完整3600帧/120秒并退出0；VLC两张原生快照确认四格有画面及全失源黑屏。对身份匹配composition-rk450的CLI2521888发送kill -INT，caller_stop、exit0；VLC经RC quit退出0。没有将失源等同永久EOF，没有本轮重入验证。errors/workerErrors/droppedBuffers=0；pressureFailures=5保留，backpressure147；ledger208140次预留与释放平衡，当前字节/对象0。进程CPU整机口径18.538437%，单核148.307495%；WS47730688→59150336字节，峰值59150336。内部相位不代替内容同步，短程不证明长期稳定。

### 原始证据摘要

{
  "selected": [
    "[2026-11-16 17:46:15.932] [info] [graph][planner.capability] encoder_completion_prepared codec=h264_rkmpp authorized=1 reason=authorized lineage=submission_order time_base=1/30 cadence=30/1 split_known=1 split_mode=0 split_arg=0 split_out=0 low_delay=0 drop_mode=0 fps_in_flex=0 fps_in=30/1 fps_out_flex=0 fps_out=30/1",
    "[2026-11-16 17:46:15.955] [info] [graph][planner.capability] encoder_completion_prepared codec=h264_rkmpp authorized=1 reason=authorized lineage=submission_order time_base=1/30 cadence=30/1 split_known=1 split_mode=0 split_arg=0 split_out=0 low_delay=0 drop_mode=0 fps_in_flex=0 fps_in=30/1 fps_out_flex=0 fps_out=30/1",
    "[2026-11-16 17:46:15.991] [info] [graph][planner.capability] encoder_completion_prepared codec=h264_rkmpp authorized=1 reason=authorized lineage=submission_order time_base=1/30 cadence=30/1 split_known=1 split_mode=0 split_arg=0 split_out=0 low_delay=0 drop_mode=0 fps_in_flex=0 fps_in=30/1 fps_out_flex=0 fps_out=30/1",
    "[2026-11-16 17:46:16.238] [info] [graph][runtime.node] encoder_completion_first_au generation=1 identity_kind=output identity=\"composition-rk450.output\" canonical_sequence=1 order=1 lineage=submission_order packet_pts=0 packet_dts=0 submitted_pts=0 canonical_dispatch_ns=0",
    "[2026-11-16 17:46:17.189] [info] [graph][runtime.node] av_scheduler_video_progress_release node=94 generation=1 audio_sequence=40 audio_canonical_dispatch_ns=905578231 video_canonical_lower_bound_ns=933333333 observed_after_output_commit=1",
    "[2026-11-16 17:48:32.324] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:2 execution_mode=1 committed_batches=14787 committed_datagrams=118322 committed_payload_bytes=145189812 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=85 ordered_queue_maximum_wire_bytes=115260 pacing_reserved=118322 pacing_cancelled=0 pacing_submitted=118322 pacing_maximum_submit_lateness_ns=33082320 pacing_worst_late_global_sequence=2397 pacing_worst_success_sample_available=1 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=85 backlog_high_water_wire_bytes=115260 backlog_max_residence_ns=57029938 last_materialized_sequence=118322 last_scheduled_sequence=118322 last_submitted_sequence=118322 last_committed_sequence=118322 submitted_service_generation=1 submitted_service_sequence=118322 submitted_service_completed_ns=138122080508 submitted_service_duration_ns=16640 submitted_service_available_ns=138122097148 submitted_scope_available_ns=138122097148 shared_scope_charged_datagrams=118322 shared_scope_charged_wire_bytes=148502828 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=1153 evidence_timestamp_tracked=0 evidence_timestamp_untracked=1153 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=554648 endpoint_1_target_effective_socket_bytes=277324 endpoint_1_api_requested_socket_bytes=138662 endpoint_1_effective_socket_bytes=277324 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=118286 endpoint_1_committed_payload_bytes=145187356 endpoint_2_target_effective_socket_bytes=277324 endpoint_2_api_requested_socket_bytes=138662 endpoint_2_effective_socket_bytes=277324 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=36 endpoint_2_committed_payload_bytes=2456",
    "[CLI] end_reason=caller_stop",
    "[CLI] final runtime report: queued=0, peakQueued=1286, threads=102, processThreads=148, workers=0, workerProgress=346357, workerProcessCalls=1469499, workerWaits=1123123, workerWakeups=1105578, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=541, logicalProcessors=8, averageSystemMachineCpuPercent=37.719583, averageProcessMachineCpuPercent=18.538437, peakProcessMachineCpuPercent=31.550802, averageProcessSingleCoreCpuPercent=148.307495, peakProcessSingleCoreCpuPercent=252.406417, initialWorkingSetBytes=47730688, workingSetBytes=59150336, peakWorkingSetBytes=59150336, totalPushed=315032, totalPopped=314943, droppedBuffers=0, encodedPacketsPushed=45629, encodedPacketsPopped=45629, backpressureItems=147, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=9628426, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=416, graphPayloadReservations=208140, graphPayloadReleases=208140, graphPayloadPressureFailures=5, lastInputReceivedAtNs=23042372436155"
  ],
  "last_drift": [
    "[2026-11-16 17:48:12.840] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=5133 source_master_ns=121191146436 raw_phase_ns=22675 filtered_phase_ns=22676 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=194561 effective_output_sample_index=5447708"
  ],
  "stderr": "mpp[2521888]: mpp_platform: client 12 driver is not ready!\nmpp[2521888]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md\n[in @ 0x2fdd1b90] Changing video frame properties on the fly is not supported by all filters.\n[in @ 0x2fdd1b90] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 30956.844078\nmpp[2521888]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md\n[in @ 0x2fdd1650] Changing video frame properties on the fly is not supported by all filters.\n[in @ 0x2fdd1650] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 40091.209078\nmpp[2521888]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md\n[in @ 0x2fdf4d30] Changing video frame properties on the fly is not supported by all filters.\n[in @ 0x2fdf4d30] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 24110.234511\nmpp[2521888]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md\n[in @ 0x2fe0bf60] Changing video frame properties on the fly is not supported by all filters.\n[in @ 0x2fe0bf60] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 16180.000789\nmpp[2521888]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md\nmpp[2521888]: mpp_enc: set prep cfg w:h [1280:720] stride [1280:768] fmt 0 rotate 0 mirror 0\nmpp[2521888]: mpp_enc: set rc vbr bps [8000000:12000000:8000000] fps [30:1:fix] - [30:1:fix] gop 60\nmpp[2521888]: mpp_enc: mode vbr bps [8000000:8000000:12000000] fps fix [30/1] -> fix [30/1] gop i [60] v [0]\nmpp[2521888]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md\nmpp[2521888]: mpp_enc: set prep cfg w:h [1280:720] stride [1280:768] fmt 0 rotate 0 mirror 0\nmpp[2521888]: mpp_enc: set rc vbr bps [8000000:12000000:8000000] fps [30:1:fix] - [30:1:fix] gop 60\nmpp[2521888]: mpp_enc: mode vbr bps [8000000:8000000:12000000] fps fix [30/1] -> fix [30/1] gop i [60] v [0]\nmpp[2521888]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md\nmpp[2521888]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md\nmpp[2521888]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md\nmpp[2521888]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md\nmpp[2521888]: h265d: h265d_decode_nal_sei ret -1006\nmpp[2521888]: h265d: pps: invalid pps id 0\nmpp[2521888]: h265d: nal: parse unit #1 failed ret -1004\nmpp[2521888]: H265d: current stream is no right skip it (nil)\nmpp[2521888]: h265d: h265d_decode_nal_sei ret -1006\nmpp[2521888]: h265d: pps: invalid pps id 0\nmpp[2521888]: h265d: nal: parse unit #1 failed ret -1004\nmpp[2521888]: H265d: current stream is no right skip it (nil)\nmpp[2521888]: h265d: h265d_decode_nal_sei ret -1006\nmpp[2521888]: h265d: pps: invalid pps id 0\nmpp[2521888]: h265d: nal: parse unit #1 failed ret -1004\nmpp[2521888]: H265d: current stream is no right skip it (nil)\nmpp[2521888]: h265d: h265d_decode_nal_sei ret -1006\nmpp[2521888]: h265d: pps: invalid pps id 0\nmpp[2521888]: h265d: nal: parse unit #1 failed ret -1004\nmpp[2521888]: H265d: current stream is no right skip it (nil)\n[in @ 0x7ea4002620] Changing video frame properties on the fly is not supported by all filters.\n[in @ 0x7ea4002620] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 30956.844078\n[in @ 0x7ec4002620] Changing video frame properties on the fly is not supported by all filters.\n[in @ 0x7ec4002620] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 40091.209078\n[in @ 0x7eb8002620] Changing video frame properties on the fly is not supported by all filters.\n[in @ 0x7eb8002620] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 24110.234511\nmpp[2521888]: h265d: h265d_decode_nal_sei ret -1006\nmpp[2521888]: h265d: pps: invalid pps id 0\nmpp[2521888]: h265d: nal: parse unit #1 failed ret -1004\nmpp[2521888]: H265d: current stream is no right skip it (nil)\nmpp[2521888]: h265d: h265d_decode_nal_sei ret -1006\nmpp[2521888]: h265d: pps: invalid pps id 0\nmpp[2521888]: h265d: nal: parse unit #1 failed ret -1004\nmpp[2521888]: H265d: current stream is no right skip it (nil)\nmpp[2521888]: h265d: h265d_decode_nal_sei ret -1006\nmpp[2521888]: h265d: pps: invalid pps id 0\nmpp[2521888]: h265d: nal: parse unit #1 failed ret -1004\nmpp[2521888]: H265d: current stream is no right skip it (nil)\nmpp[2521888]: h265d: h265d_decode_nal_sei ret -1006\nmpp[2521888]: h265d: pps: invalid pps id 0\nmpp[2521888]: h265d: nal: parse unit #1 failed ret -1004\nmpp[2521888]: H265d: current stream is no right skip it (nil)\nmpp[2521888]: h265d: h265d_decode_nal_sei ret -1006\nmpp[2521888]: h265d: pps: invalid pps id 0\nmpp[2521888]: h265d: nal: parse unit #1 failed ret -1004\nmpp[2521888]: H265d: current stream is no right skip it (nil)\nmpp[2521888]: h265d: h265d_decode_nal_sei ret -1006\nmpp[2521888]: h265d: pps: invalid pps id 0\nmpp[2521888]: h265d: nal: parse unit #1 failed ret -1004\nmpp[2521888]: H265d: current stream is no right skip it (nil)\nmpp[2521888]: h265d: h265d_decode_nal_sei ret -1006\nmpp[2521888]: h265d: pps: invalid pps id 0\nmpp[2521888]: h265d: nal: parse unit #1 failed ret -1004\nmpp[2521888]: H265d: current stream is no right skip it (nil)\n[in @ 0x7e60002ce0] Changing video frame properties on the fly is not supported by all filters.\n[in @ 0x7e60002ce0] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 16180.000789\nmpp[2521888]: mpp_enc: set prep cfg w:h [1280:720] stride [1280:720] fmt 0 rotate 0 mirror 0\n[aac @ 0x7ef0000b80] Qavg: 9476.673\nmpp[2521888]: mpp_buffer: mpp_buffer_service_deinit cleaning misc group\n",
  "result": "CLI_EXIT=0\nSOURCE0_EXIT=0\nSOURCE1_EXIT=0\nSOURCE2_EXIT=0\nRUN_FINISHED=1\n",
  "pids": "SOURCE0=2521885 SOURCE1=2521886 SOURCE2=2521887 CLI=2521888\nMONITOR=2521890\n",
  "sources": {
    "source0": [
      "[out#0/rtp @ 0x41d0740] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%",
      "frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.46    "
    ],
    "source1": [
      "[out#0/rtp @ 0x328af740] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%",
      "frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.46    "
    ],
    "source2": [
      "[out#1/rtp @ 0x8a7fd90] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%",
      "frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.46    "
    ]
  },
  "files": [
    "cli-error.log",
    "cli-maps.txt",
    "cli.log",
    "launcher.log",
    "monitor.log",
    "output.sdp",
    "pids.txt",
    "result.txt",
    "run.sh",
    "socket-monitor.log",
    "source0.log",
    "source0.out",
    "source1.log",
    "source1.out",
    "source2.log",
    "source2.out",
    "udp-after.txt",
    "udp-before.txt"
  ]
}

## 清理清单

远端：上方files清单加evidence.json；本机：
composition-rk450-evidence.json
composition-rk450-native-2026-11-16-17h47m19s424.png
composition-rk450-native-2026-11-16-17h48m58s358.png
composition-rk450-source3.log
composition-rk450-source3.out
composition-rk450-vlc.log

外部监测130个样本，RSS最大58924KiB。UDP系统全局计数差：InDatagrams419809、NoPorts33、InErrors0、OutDatagrams445339、RcvbufErrors0、SndbufErrors0、InCsumErrors0、IgnoredMulti20；不能据此证明端到端无丢包。6个远端PID和2个本机PID核查均不存在。归档一致性独立复审PASS。

清理结果：远端19项、本机6项均按上述清单删除；目录无本轮残留，复用源与正式产物保留。
