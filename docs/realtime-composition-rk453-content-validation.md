# rk453：RKMPP 选中源与输出内容时钟联合取证

状态：实流及离线取证结束，内容同步仍未无条件通过。当前目标CLI ab5329a1ffa9d810e4f525ae467bf7c14c5cb205d1b1131ef462d32efc4eacfe，源码ce71265d。沿用rk450四源120秒、H264720p30 VBR8–12Mbps/AAC192kbps MPEG-TS/RTP输出。选中source2从Windows发送带标记H264 CBR8200kbps+原AAC copy；source0/1目标copy，source3 Windows HEVC CBR8000kbps。与Windows r452相同标记测量方法，保留未标记RKMPP功能记录；不修改Windows FFmpeg。物理以太网接口7旁路捕获选中输入及返回输出，所见入站/出站点不同，不直接宣称硬件wire时刻证明。

## 远端临时脚本

```bash
#!/usr/bin/env bash
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
export PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin:$PATH
export LD_LIBRARY_PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib:/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib:/opt/mt-tools/gcc-12.4-native/lib64
set -u
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk453
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk453 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://192.168.130.229:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 192.168.96.122 --rtp-port 62024 --sdp /home/tang/MediaTranscode-composition-09093919/acceptance/rk453/output.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
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

bash acceptance/rk453/run.sh >acceptance/rk453/launcher.log 2>&1 &
```

## Windows 实际命令

```powershell
$p=Start-Process -FilePath 'D:/Wireshark/dumpcap.exe' -ArgumentList '-i 7 -f "udp and (dst port 61020 or dst port 61021 or dst port 61022 or dst port 61023 or dst port 62024 or dst port 62025)" -a duration:420 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk453-output.pcapng' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk453-capture.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk453-capture.log' -PassThru; Write-Output "CAPTURE_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "CAPTURE_EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -vf "drawtext=fontfile=''C\:/Windows/Fonts/consola.ttf'':text=''frame=%{frame_num} pts=%{pts}'':fontsize=48:fontcolor=white:box=1:boxcolor=black:x=16:y=16" -c:v h264_nvenc -rc cbr -b:v 8200k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61020?rtcpport=61021&pkt_size=1200&localaddr=192.168.96.122" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://192.168.130.229:61022?rtcpport=61023&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk453-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk453-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE2_EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61050?rtcpport=61051&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk453-source3.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk453-source3.log' -PassThru; Write-Output "SOURCE3_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE3_EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk453-vlc.log --extraintf=rc --rc-host=127.0.0.1:42120 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk453-native- --snapshot-format=png rtp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "VLC_EXIT=$($p.ExitCode)"
```

PID：capture5860；Windows source2/3=32328/11944，VLC9344；远端wrapper2730745，source0/1=2730749/2730750，CLI2730753，monitor2730755。

## 结果：功能收尾通过，内容归因仍为条件证据

四个源完整120秒/3600帧并exit0；VLC已观察四格及全黑。远端精确核CLI身份后kill -INT，caller_stop/exit0；VLC quit0、dumpcap CtrlC0；远端5个及Windows4个PID均无残留。

物理网卡捕获234709包、drop0。选中输入恢复3600视频AU、5168 AAC AU、122个AAC分片包，输入RTP缺口0；25+25 SR、2个BYE、无SDES。输出120270 RTP媒体包，765017 TS包、143823196字节，RTP缺口/CC错误/TEI=0；sender媒体提交也为120270。此为本轮有限接收交付证据，不是NIC实际发送服务曲线或所有输入无丢包证明。

输入有效5291008音频样本与参考在真实Skip Samples=1024对齐后逐位一致，参考末992样本未覆盖。五个有效视频标记：输出PTS1/10/30/60/90秒分别对应源帧630/900/1500/2400/3300；105秒已黑，排除内容分析。输入解码帧0/300/900标记0/10/30秒；输入视频RTP呈现排序3000步长、AAC1024步长。

最高音频匹配候选下，输入SR基准差236.770975–236.791383ms；输出内容相对差240.566893ms；残差3.775510–3.795919ms。该范围仅SR变化，不是完整误差置信区间。98秒整段（输出PTS1..99）全源相关最高0.994672@原源21.2405669秒，其他0.992980@4.70723356、0.991867@12.97390023仍较高；独立复算一致，但周期歧义未消除，不能判RKMPP内容同步无条件PASS，也不从此修改核心。

本轮Windows源先于目标CLI约十余秒启动，准备后输出首秒已对应原源21秒，缺少前段限制了有效匹配长度；长窗口仅98秒，不能套用Windows r452的110秒结论。下一次将先启动等待输入的目标CLI，再启动完整120秒源，争取保留更长连续输出；这是诊断启动顺序调整，不缩小源规格、不移除转码步骤，仍保留本轮不确定结论。

最终errors/workerErrors/droppedBuffers=0，pressureFailures=5保留，ledger199069次预留=释放，当前0字节/0对象；CPU整机均值16.479235%、单核131.833883%；WS54083584→65355776，峰65355776字节；外部146样本RSS最大64636KiB。原告警及sender delivery not_proven详见下方，不能由正常停止掩盖。

## 离线命令及方法复用

通过D:/Python/python.exe -c直接执行r452文档中AAC分片完成后的输入解析、音频逐块映射、输出TS恢复及Fraction SR归因程序，文件前缀r452/r451统一替换rk453，未落地测试脚本。输出视频沿用r452六个采样点，已逐张查看；相关程序仅保留有效五点及上述源帧列表。长匹配程序window_seconds由110改98，其他全源valid搜索及峰报告不变；合法参考起点范围0..22秒，未按视频筛选。

### evidence.json
{
  "selected": [
    "[2026-11-16 18:18:15.388] [info] [graph][planner.capability] encoder_completion_prepared codec=h264_rkmpp authorized=1 reason=authorized lineage=submission_order time_base=1/30 cadence=30/1 split_known=1 split_mode=0 split_arg=0 split_out=0 low_delay=0 drop_mode=0 fps_in_flex=0 fps_in=30/1 fps_out_flex=0 fps_out=30/1",
    "[2026-11-16 18:18:15.423] [info] [graph][planner.capability] encoder_completion_prepared codec=h264_rkmpp authorized=1 reason=authorized lineage=submission_order time_base=1/30 cadence=30/1 split_known=1 split_mode=0 split_arg=0 split_out=0 low_delay=0 drop_mode=0 fps_in_flex=0 fps_in=30/1 fps_out_flex=0 fps_out=30/1",
    "[2026-11-16 18:18:15.494] [info] [graph][planner.capability] encoder_completion_prepared codec=h264_rkmpp authorized=1 reason=authorized lineage=submission_order time_base=1/30 cadence=30/1 split_known=1 split_mode=0 split_arg=0 split_out=0 low_delay=0 drop_mode=0 fps_in_flex=0 fps_in=30/1 fps_out_flex=0 fps_out=30/1",
    "[2026-11-16 18:18:16.091] [info] [graph][runtime.node] encoder_completion_first_au generation=1 identity_kind=output identity=\"composition-rk453.output\" canonical_sequence=1 order=1 lineage=submission_order packet_pts=0 packet_dts=0 submitted_pts=0 canonical_dispatch_ns=0",
    "[2026-11-16 18:18:19.256] [info] [graph][runtime.node] av_scheduler_video_progress_release node=94 generation=1 audio_sequence=136 audio_canonical_dispatch_ns=3134693877 video_canonical_lower_bound_ns=3166666666 observed_after_output_commit=1",
    "[2026-11-16 18:20:49.305] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:2 execution_mode=1 committed_batches=16636 committed_datagrams=120310 committed_payload_bytes=145269164 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=85 ordered_queue_maximum_wire_bytes=115260 pacing_reserved=120310 pacing_cancelled=0 pacing_submitted=120310 pacing_maximum_submit_lateness_ns=30964083 pacing_worst_late_global_sequence=7316 pacing_worst_success_sample_available=1 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=85 backlog_high_water_wire_bytes=115260 backlog_max_residence_ns=58756861 last_materialized_sequence=120310 last_scheduled_sequence=120310 last_submitted_sequence=120310 last_committed_sequence=120310 submitted_service_generation=1 submitted_service_sequence=120310 submitted_service_completed_ns=156375572792 submitted_service_duration_ns=16640 submitted_service_available_ns=156375589432 submitted_scope_available_ns=156375589432 shared_scope_charged_datagrams=120310 shared_scope_charged_wire_bytes=148637844 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=1153 evidence_timestamp_tracked=0 evidence_timestamp_untracked=1153 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=554648 endpoint_1_target_effective_socket_bytes=277324 endpoint_1_api_requested_socket_bytes=138662 endpoint_1_effective_socket_bytes=277324 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=120270 endpoint_1_committed_payload_bytes=145266436 endpoint_2_target_effective_socket_bytes=277324 endpoint_2_api_requested_socket_bytes=138662 endpoint_2_effective_socket_bytes=277324 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=40 endpoint_2_committed_payload_bytes=2728",
    "[CLI] end_reason=caller_stop",
    "[CLI] final runtime report: queued=0, peakQueued=1481, threads=102, processThreads=148, workers=0, workerProgress=340155, workerProcessCalls=1573400, workerWaits=1233226, workerWakeups=1211351, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=611, logicalProcessors=8, averageSystemMachineCpuPercent=36.202823, averageProcessMachineCpuPercent=16.479235, peakProcessMachineCpuPercent=34.010152, averageProcessSingleCoreCpuPercent=131.833883, peakProcessSingleCoreCpuPercent=272.081218, initialWorkingSetBytes=54083584, workingSetBytes=65355776, peakWorkingSetBytes=65355776, totalPushed=306658, totalPopped=306483, droppedBuffers=0, encodedPacketsPushed=51375, encodedPacketsPopped=51375, backpressureItems=147, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=11238130, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=507, graphPayloadReservations=199069, graphPayloadReleases=199069, graphPayloadPressureFailures=5, lastInputReceivedAtNs=24961098963745"
  ],
  "last_drift": [
    "[2026-11-16 18:19:51.812] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4249 source_master_ns=101953847656 raw_phase_ns=0 filtered_phase_ns=0 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=197633 effective_output_sample_index=4545559"
  ],
  "stderr": "mpp[2730753]: mpp_platform: client 12 driver is not ready!\nmpp[2730753]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md\n[in @ 0x18554930] Changing video frame properties on the fly is not supported by all filters.\n[in @ 0x18554930] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 2515.144867\nmpp[2730753]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md\n[in @ 0x18555c00] Changing video frame properties on the fly is not supported by all filters.\n[in @ 0x18555c00] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 31948.725467\nmpp[2730753]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md\nmpp[2730753]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md\n[in @ 0x1857fec0] Changing video frame properties on the fly is not supported by all filters.\n[in @ 0x1857fec0] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 27927.476278\nmpp[2730753]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md\n[in @ 0x185545e0] Changing video frame properties on the fly is not supported by all filters.\n[in @ 0x185545e0] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 317.874567\nmpp[2730753]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md\nmpp[2730753]: mpp_enc: set prep cfg w:h [1280:720] stride [1280:768] fmt 0 rotate 0 mirror 0\nmpp[2730753]: mpp_enc: set rc vbr bps [8000000:12000000:8000000] fps [30:1:fix] - [30:1:fix] gop 60\nmpp[2730753]: mpp_enc: mode vbr bps [8000000:8000000:12000000] fps fix [30/1] -> fix [30/1] gop i [60] v [0]\nmpp[2730753]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md\nmpp[2730753]: mpp_enc: set prep cfg w:h [1280:720] stride [1280:768] fmt 0 rotate 0 mirror 0\nmpp[2730753]: mpp_enc: set rc vbr bps [8000000:12000000:8000000] fps [30:1:fix] - [30:1:fix] gop 60\nmpp[2730753]: mpp_enc: mode vbr bps [8000000:8000000:12000000] fps fix [30/1] -> fix [30/1] gop i [60] v [0]\nmpp[2730753]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md\nmpp[2730753]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md\nmpp[2730753]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md\nmpp[2730753]: mpp_info: mpp version: c08762ebf author: Herman Chen  2026-08-11 docs: Update 1.1.0 CHANGELOG.md\nmpp[2730753]: h264d_sei: seq_parameter_set_id 0 may be invalid\nmpp[2730753]: h264d_sei: seq_parameter_set_id 0 may be invalid\nmpp[2730753]: h264d_sei: seq_parameter_set_id 0 may be invalid\nmpp[2730753]: h264d_sei: seq_parameter_set_id 0 may be invalid\nmpp[2730753]: h265d: h265d_decode_nal_sei ret -1006\nmpp[2730753]: h265d: pps: invalid pps id 0\nmpp[2730753]: h265d: nal: parse unit #1 failed ret -1004\nmpp[2730753]: H265d: current stream is no right skip it (nil)\nmpp[2730753]: h265d: h265d_decode_nal_sei ret -1006\nmpp[2730753]: h265d: pps: invalid pps id 0\nmpp[2730753]: h265d: nal: parse unit #1 failed ret -1004\nmpp[2730753]: H265d: current stream is no right skip it (nil)\nmpp[2730753]: h265d: h265d_decode_nal_sei ret -1006\nmpp[2730753]: h265d: pps: invalid pps id 0\nmpp[2730753]: h265d: nal: parse unit #1 failed ret -1004\nmpp[2730753]: H265d: current stream is no right skip it (nil)\n[in @ 0x7e78001610] Changing video frame properties on the fly is not supported by all filters.\n[in @ 0x7e78001610] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 2515.144867\n[in @ 0x7e40002cf0] Changing video frame properties on the fly is not supported by all filters.\n[in @ 0x7e40002cf0] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 31948.725467\n[in @ 0x7f9c02c870] Changing video frame properties on the fly is not supported by all filters.\n[in @ 0x7f9c02c870] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 317.874567\nmpp[2730753]: h264d_sei: seq_parameter_set_id 0 may be invalid\nmpp[2730753]: h264d_sei: seq_parameter_set_id 0 may be invalid\nmpp[2730753]: h264d_sei: seq_parameter_set_id 0 may be invalid\nmpp[2730753]: h264d_sei: seq_parameter_set_id 0 may be invalid\nmpp[2730753]: h264d_sei: seq_parameter_set_id 0 may be invalid\nmpp[2730753]: h264d_sei: seq_parameter_set_id 0 may be invalid\nmpp[2730753]: h264d_sei: seq_parameter_set_id 0 may be invalid\nmpp[2730753]: h264d_sei: seq_parameter_set_id 0 may be invalid\nmpp[2730753]: h264d_sei: seq_parameter_set_id 0 may be invalid\nmpp[2730753]: h264d_sei: seq_parameter_set_id 0 may be invalid\nmpp[2730753]: h264d_sei: seq_parameter_set_id 0 may be invalid\nmpp[2730753]: h264d_sei: seq_parameter_set_id 0 may be invalid\nmpp[2730753]: h264d_sei: seq_parameter_set_id 0 may be invalid\nmpp[2730753]: h264d_sei: seq_parameter_set_id 0 may be invalid\nmpp[2730753]: h264d_sei: seq_parameter_set_id 0 may be invalid\nmpp[2730753]: h264d_sei: seq_parameter_set_id 0 may be invalid\nmpp[2730753]: h264d_sei: seq_parameter_set_id 0 may be invalid\nmpp[2730753]: h264d_sei: seq_parameter_set_id 0 may be invalid\nmpp[2730753]: h264d_sei: seq_parameter_set_id 0 may be invalid\nmpp[2730753]: h264d_sei: seq_parameter_set_id 0 may be invalid\nmpp[2730753]: h264d_sei: seq_parameter_set_id 0 may be invalid\nmpp[2730753]: h264d_sei: seq_parameter_set_id 0 may be invalid\nmpp[2730753]: h264d_sei: seq_parameter_set_id 0 may be invalid\nmpp[2730753]: h264d_sei: seq_parameter_set_id 0 may be invalid\n[in @ 0x7e840028f0] Changing video frame properties on the fly is not supported by all filters.\n[in @ 0x7e840028f0] filter context - w: 1280 h: 720 fmt: 178 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 178 csp: unknown range: tv alpha: unspecified pts_time: 27927.476278\nmpp[2730753]: mpp_enc: set prep cfg w:h [1280:720] stride [1280:720] fmt 0 rotate 0 mirror 0\n[aac @ 0x7f4c001300] Qavg: 24473.613\nmpp[2730753]: mpp_buffer: mpp_buffer_service_deinit cleaning misc group\n",
  "result": "CLI_EXIT=0\nSOURCE0_EXIT=0\nSOURCE1_EXIT=0\nRUN_FINISHED=1\n",
  "pids": "SOURCE0=2730749 SOURCE1=2730750 CLI=2730753\nMONITOR=2730755\n",
  "monitor_samples": 146,
  "rss_max_kib": 64636,
  "udp": {
    "udp-before.txt": "Ip: Forwarding DefaultTTL InReceives InHdrErrors InAddrErrors ForwDatagrams InUnknownProtos InDiscards InDelivers OutRequests OutDiscards OutNoRoutes ReasmTimeout ReasmReqds ReasmOKs ReasmFails FragOKs FragFails FragCreates\nIp: 1 64 11289221 0 0 0 0 0 11240209 13110358 95 1168 0 64 32 0 0 0 0\nIcmp: InMsgs InErrors InCsumErrors InDestUnreachs InTimeExcds InParmProbs InSrcQuenchs InRedirects InEchos InEchoReps InTimestamps InTimestampReps InAddrMasks InAddrMaskReps OutMsgs OutErrors OutDestUnreachs OutTimeExcds OutParmProbs OutSrcQuenchs OutRedirects OutEchos OutEchoReps OutTimestamps OutTimestampReps OutAddrMasks OutAddrMaskReps\nIcmp: 532116 45913 0 499157 0 0 0 0 32959 0 0 0 0 0 533126 0 500167 0 0 0 0 0 32959 0 0 0 0\nIcmpMsg: InType3 InType8 OutType0 OutType3\nIcmpMsg: 499157 32959 32959 500167\nTcp: RtoAlgorithm RtoMin RtoMax MaxConn ActiveOpens PassiveOpens AttemptFails EstabResets CurrEstab InSegs OutSegs RetransSegs InErrs OutRsts InCsumErrors\nTcp: 1 200 120000 -1 56488 24341 28063 349 46 2994613 4813679 2141 0 14827 0\nUdp: InDatagrams NoPorts InErrors OutDatagrams RcvbufErrors SndbufErrors InCsumErrors IgnoredMulti\nUdp: 6732668 904570 60075 8519271 60075 0 0 3634\nUdpLite: InDatagrams NoPorts InErrors OutDatagrams RcvbufErrors SndbufErrors InCsumErrors IgnoredMulti\nUdpLite: 0 0 0 0 0 0 0 0\n",
    "udp-after.txt": "Ip: Forwarding DefaultTTL InReceives InHdrErrors InAddrErrors ForwDatagrams InUnknownProtos InDiscards InDelivers OutRequests OutDiscards OutNoRoutes ReasmTimeout ReasmReqds ReasmOKs ReasmFails FragOKs FragFails FragCreates\nIp: 1 64 11708383 0 0 0 0 0 11659045 13475446 95 1174 0 64 32 0 0 0 0\nIcmp: InMsgs InErrors InCsumErrors InDestUnreachs InTimeExcds InParmProbs InSrcQuenchs InRedirects InEchos InEchoReps InTimestamps InTimestampReps InAddrMasks InAddrMaskReps OutMsgs OutErrors OutDestUnreachs OutTimeExcds OutParmProbs OutSrcQuenchs OutRedirects OutEchos OutEchoReps OutTimestamps OutTimestampReps OutAddrMasks OutAddrMaskReps\nIcmp: 533669 46190 0 500408 0 0 0 0 33261 0 0 0 0 0 534678 0 501417 0 0 0 0 0 33261 0 0 0 0\nIcmpMsg: InType3 InType8 OutType0 OutType3\nIcmpMsg: 500408 33261 33261 501417\nTcp: RtoAlgorithm RtoMin RtoMax MaxConn ActiveOpens PassiveOpens AttemptFails EstabResets CurrEstab InSegs OutSegs RetransSegs InErrs OutRsts InCsumErrors\nTcp: 1 200 120000 -1 56690 24360 28164 349 49 3011849 4844069 2142 0 14843 0\nUdp: InDatagrams NoPorts InErrors OutDatagrams RcvbufErrors SndbufErrors InCsumErrors IgnoredMulti\nUdp: 7132643 904601 60075 8854882 60075 0 0 3675\nUdpLite: InDatagrams NoPorts InErrors OutDatagrams RcvbufErrors SndbufErrors InCsumErrors IgnoredMulti\nUdpLite: 0 0 0 0 0 0 0 0\n"
  },
  "sources": {
    "source0": [
      "[out#0/rtp @ 0x233e8740] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%",
      "frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.46    "
    ],
    "source1": [
      "[out#0/rtp @ 0x3e743740] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%",
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
    "udp-after.txt",
    "udp-before.txt"
  ]
}
### attribution.json
{
  "input_audio_exact_reference_after_verified_skip": true,
  "exact_equal_samples": 5291008,
  "verified_skip_samples": 1024,
  "raw_input_minus_reference_samples": 32,
  "reference_unmatched_tail_samples": 992,
  "input_video_frames": 3600,
  "input_audio_units": 5168,
  "video_epoch": [
    "1074765396156665036747/268435456000",
    "1074765396156665036761/268435456000",
    "2149530792313330073523/536870912000",
    "2149530792313330073477/536870912000",
    "429906158462666014703/107374182400",
    "1074765396156665036753/268435456000",
    "26869134903916625919/6710886400",
    "537382698078332518393/134217728000",
    "1074765396156665036789/268435456000",
    "2149530792313330073551/536870912000",
    "2149530792313330073533/536870912000",
    "429906158462666014717/107374182400",
    "268691349039166259189/67108864000",
    "537382698078332518383/134217728000",
    "1074765396156665036799/268435456000",
    "2149530792313330073599/536870912000",
    "1074765396156665036751/268435456000",
    "2149530792313330073503/536870912000",
    "1074765396156665036759/268435456000",
    "2149530792313330073583/536870912000",
    "2149530792313330073523/536870912000",
    "4299061584626660147/1073741824",
    "2149530792313330073483/536870912000",
    "537382698078332518371/134217728000",
    "1074765396156665036753/268435456000"
  ],
  "audio_epoch": [
    "11849288491857788639377/2959500902400",
    "214953079217374363647/53687091200",
    "189588615869724618232237/47352014438400",
    "94794307934862309116339/23676007219200",
    "429906158434748727273/107374182400",
    "94794307934861825931107/23676007219200",
    "17196246337389949091/4294967296",
    "189588615869724618224299/47352014438400",
    "429906158434748727279/107374182400",
    "189588615869724618225181/47352014438400",
    "2369857698371553701273/591900180480",
    "7899525661238521285631/1973000601600",
    "47397153967431020340221/11838003609600",
    "44142039462394/11025",
    "189588615869724081362207/47352014438400",
    "12639241057981591107319/3156800962560",
    "53738269804343590911/13421772800",
    "189588615869724081353387/47352014438400",
    "189588615869724618230473/47352014438400",
    "11849288491857728241223/2959500902400",
    "85981231686949745459/21474836480",
    "12639241057981591107613/3156800962560",
    "94794307934862040678237/23676007219200",
    "189588615869724403480697/47352014438400",
    "94794307934861986994321/23676007219200"
  ],
  "input_bias_min_ms": 236.7709748611116,
  "input_bias_max_ms": 236.7913833671036,
  "output_candidate_ms": 240.5668934240346,
  "residual_min_ms": 3.775510056930983,
  "residual_max_ms": 3.795918562922992,
  "fragment_packets": 122
}
### content-result.json
[
  {
    "output_pts": 1,
    "video_frame": 630,
    "source_video_pts": 21.0,
    "source_audio_pts": 21.240566893424035,
    "audio_offset": -20.240566893424035,
    "video_offset": -20.0,
    "video_minus_audio_ms": 240.5668934240346,
    "correlation": 0.9979653899221472,
    "other_peaks": [
      {
        "source_time": 21.240566893424035,
        "correlation": 0.9979653899221472
      },
      {
        "source_time": 37.77390022675737,
        "correlation": 0.9972189332301968
      },
      {
        "source_time": 54.3072335600907,
        "correlation": 0.9966867913555244
      },
      {
        "source_time": 103.9072335600907,
        "correlation": 0.9965376279736913
      },
      {
        "source_time": 70.84056689342404,
        "correlation": 0.9964308914839308
      }
    ]
  },
  {
    "output_pts": 10,
    "video_frame": 900,
    "source_video_pts": 30.0,
    "source_audio_pts": 30.240566893424035,
    "audio_offset": -20.240566893424035,
    "video_offset": -20.0,
    "video_minus_audio_ms": 240.5668934240346,
    "correlation": 0.9986307732106369,
    "other_peaks": [
      {
        "source_time": 30.240566893424035,
        "correlation": 0.9986307732106369
      },
      {
        "source_time": 79.84056689342404,
        "correlation": 0.9957471583650759
      },
      {
        "source_time": 46.77390022675737,
        "correlation": 0.9956223455622896
      },
      {
        "source_time": 96.37390022675737,
        "correlation": 0.9954576939307193
      },
      {
        "source_time": 63.3072335600907,
        "correlation": 0.9953813336212386
      }
    ]
  },
  {
    "output_pts": 30,
    "video_frame": 1500,
    "source_video_pts": 50.0,
    "source_audio_pts": 50.24056689342404,
    "audio_offset": -20.240566893424038,
    "video_offset": -20.0,
    "video_minus_audio_ms": 240.56689342403814,
    "correlation": 0.999080063391555,
    "other_peaks": [
      {
        "source_time": 50.24056689342404,
        "correlation": 0.999080063391555
      },
      {
        "source_time": 116.37390022675737,
        "correlation": 0.9987932033167032
      },
      {
        "source_time": 66.77390022675738,
        "correlation": 0.9987853271194148
      },
      {
        "source_time": 17.173900226757368,
        "correlation": 0.9987653034127364
      },
      {
        "source_time": 99.84056689342404,
        "correlation": 0.9982328644269787
      }
    ]
  },
  {
    "output_pts": 60,
    "video_frame": 2400,
    "source_video_pts": 80.0,
    "source_audio_pts": 80.24056689342403,
    "audio_offset": -20.24056689342403,
    "video_offset": -20.0,
    "video_minus_audio_ms": 240.56689342403104,
    "correlation": 0.9987610779804129,
    "other_peaks": [
      {
        "source_time": 80.24056689342403,
        "correlation": 0.9987610779804129
      },
      {
        "source_time": 47.17390022675737,
        "correlation": 0.9981425387653653
      },
      {
        "source_time": 63.7072335600907,
        "correlation": 0.9979246480484221
      },
      {
        "source_time": 96.77390022675738,
        "correlation": 0.9977279442813595
      },
      {
        "source_time": 14.107233560090704,
        "correlation": 0.997692416303256
      }
    ]
  },
  {
    "output_pts": 90,
    "video_frame": 3300,
    "source_video_pts": 110.0,
    "source_audio_pts": 110.24056689342403,
    "audio_offset": -20.24056689342403,
    "video_offset": -20.0,
    "video_minus_audio_ms": 240.56689342403104,
    "correlation": 0.9982203147712518,
    "other_peaks": [
      {
        "source_time": 110.24056689342403,
        "correlation": 0.9982203147712518
      },
      {
        "source_time": 27.57390022675737,
        "correlation": 0.9968994899781944
      },
      {
        "source_time": 77.17390022675737,
        "correlation": 0.9968549689606977
      },
      {
        "source_time": 60.64056689342404,
        "correlation": 0.996840068558527
      },
      {
        "source_time": 44.107233560090705,
        "correlation": 0.9963436960353338
      }
    ]
  }
]
### long-audio-match.json
{
  "output_start_pts": 1,
  "window_seconds": 98,
  "source_start_pts": 21.240566893424035,
  "correlation": 0.9946721713085741,
  "candidate_lag_samples": 892609,
  "other_peaks": [
    {
      "source_start_pts": 21.240566893424035,
      "correlation": 0.9946721713085741
    },
    {
      "source_start_pts": 4.707233560090703,
      "correlation": 0.9929795724932938
    },
    {
      "source_start_pts": 12.973900226757369,
      "correlation": 0.9918669102199716
    },
    {
      "source_start_pts": 3.3872562358276643,
      "correlation": 0.07192624573151499
    },
    {
      "source_start_pts": 19.92061224489796,
      "correlation": 0.07188059940359326
    }
  ],
  "valid_reference_start_range_seconds": [
    0,
    22.0
  ]
}
### wire.json
{
  "datagrams": 120270,
  "rtp_gaps": 0,
  "ts_packets": 765017,
  "tei": 0,
  "cc_errors": 0,
  "stderr": "",
  "ts_bytes": 143823196
}
### SR实际记录
[
  {
    "port": 61023,
    "ssrc": 1932352396,
    "seconds": 4003813103,
    "fraction": 1413044240,
    "rtp": 1698875428
  },
  {
    "port": 61021,
    "ssrc": 4088266843,
    "seconds": 4003813103,
    "fraction": 2740189134,
    "rtp": 2511692035
  },
  {
    "port": 61023,
    "ssrc": 1932352396,
    "seconds": 4003813108,
    "fraction": 1524713390,
    "rtp": 1699097075
  },
  {
    "port": 61021,
    "ssrc": 4088266843,
    "seconds": 4003813108,
    "fraction": 2765958938,
    "rtp": 2512142575
  },
  {
    "port": 61023,
    "ssrc": 1932352396,
    "seconds": 4003813113,
    "fraction": 1627792605,
    "rtp": 1699318633
  },
  {
    "port": 61021,
    "ssrc": 4088266843,
    "seconds": 4003813113,
    "fraction": 2881923055,
    "rtp": 2512595005
  },
  {
    "port": 61023,
    "ssrc": 1932352396,
    "seconds": 4003813118,
    "fraction": 1670742278,
    "rtp": 1699539574
  },
  {
    "port": 61021,
    "ssrc": 4088266843,
    "seconds": 4003813118,
    "fraction": 2916282793,
    "rtp": 2513045725
  },
  {
    "port": 61023,
    "ssrc": 1932352396,
    "seconds": 4003813123,
    "fraction": 1696512081,
    "rtp": 1699760339
  },
  {
    "port": 61021,
    "ssrc": 4088266843,
    "seconds": 4003813123,
    "fraction": 3027951943,
    "rtp": 2513498065
  },
  {
    "port": 61023,
    "ssrc": 1932352396,
    "seconds": 4003813128,
    "fraction": 1717986918,
    "rtp": 1699981060
  },
  {
    "port": 61021,
    "ssrc": 4088266843,
    "seconds": 4003813128,
    "fraction": 3058016714,
    "rtp": 2513948695
  },
  {
    "port": 61023,
    "ssrc": 1932352396,
    "seconds": 4003813133,
    "fraction": 1782411427,
    "rtp": 1700202221
  },
  {
    "port": 61021,
    "ssrc": 4088266843,
    "seconds": 4003813133,
    "fraction": 3070901616,
    "rtp": 2514398965
  },
  {
    "port": 61023,
    "ssrc": 1932352396,
    "seconds": 4003813138,
    "fraction": 1928440315,
    "rtp": 1700424220
  },
  {
    "port": 61021,
    "ssrc": 4088266843,
    "seconds": 4003813138,
    "fraction": 3195455668,
    "rtp": 2514851575
  },
  {
    "port": 61023,
    "ssrc": 1932352396,
    "seconds": 4003813143,
    "fraction": 1954210119,
    "rtp": 1700644985
  },
  {
    "port": 61021,
    "ssrc": 4088266843,
    "seconds": 4003813143,
    "fraction": 3354369458,
    "rtp": 2515304905
  },
  {
    "port": 61023,
    "ssrc": 1932352396,
    "seconds": 4003813148,
    "fraction": 2014339661,
    "rtp": 1700866102
  },
  {
    "port": 61021,
    "ssrc": 4088266843,
    "seconds": 4003813148,
    "fraction": 3444563771,
    "rtp": 2515756795
  },
  {
    "port": 61023,
    "ssrc": 1932352396,
    "seconds": 4003813153,
    "fraction": 2044404432,
    "rtp": 1701086911
  },
  {
    "port": 61021,
    "ssrc": 4088266843,
    "seconds": 4003813153,
    "fraction": 3504693313,
    "rtp": 2516208055
  },
  {
    "port": 61023,
    "ssrc": 1932352396,
    "seconds": 4003813158,
    "fraction": 2095944040,
    "rtp": 1701307940
  },
  {
    "port": 61021,
    "ssrc": 4088266843,
    "seconds": 4003813158,
    "fraction": 3629247365,
    "rtp": 2516660665
  },
  {
    "port": 61023,
    "ssrc": 1932352396,
    "seconds": 4003813163,
    "fraction": 2121713844,
    "rtp": 1701528705
  },
  {
    "port": 61021,
    "ssrc": 4088266843,
    "seconds": 4003813163,
    "fraction": 3753801416,
    "rtp": 2517113275
  },
  {
    "port": 61023,
    "ssrc": 1932352396,
    "seconds": 4003813168,
    "fraction": 2147483648,
    "rtp": 1701749470
  },
  {
    "port": 61021,
    "ssrc": 4088266843,
    "seconds": 4003813168,
    "fraction": 3925600108,
    "rtp": 2517566875
  },
  {
    "port": 61023,
    "ssrc": 1932352396,
    "seconds": 4003813173,
    "fraction": 2250562863,
    "rtp": 1701971028
  },
  {
    "port": 61021,
    "ssrc": 4088266843,
    "seconds": 4003813173,
    "fraction": 4063039062,
    "rtp": 2518019755
  },
  {
    "port": 61023,
    "ssrc": 1932352396,
    "seconds": 4003813178,
    "fraction": 2284922601,
    "rtp": 1702191881
  },
  {
    "port": 61021,
    "ssrc": 4088266843,
    "seconds": 4003813178,
    "fraction": 4179003179,
    "rtp": 2518472185
  },
  {
    "port": 61023,
    "ssrc": 1932352396,
    "seconds": 4003813183,
    "fraction": 2340757176,
    "rtp": 1702412954
  },
  {
    "port": 61021,
    "ssrc": 4088266843,
    "seconds": 4003813183,
    "fraction": 4204772982,
    "rtp": 2518922725
  },
  {
    "port": 61023,
    "ssrc": 1932352396,
    "seconds": 4003813188,
    "fraction": 2465311227,
    "rtp": 1702634733
  },
  {
    "port": 61021,
    "ssrc": 4088266843,
    "seconds": 4003813189,
    "fraction": 25769803,
    "rtp": 2519375155
  },
  {
    "port": 61023,
    "ssrc": 1932352396,
    "seconds": 4003813193,
    "fraction": 2529735737,
    "rtp": 1702855894
  },
  {
    "port": 61021,
    "ssrc": 4088266843,
    "seconds": 4003813194,
    "fraction": 154618822,
    "rtp": 2519827855
  },
  {
    "port": 61023,
    "ssrc": 1932352396,
    "seconds": 4003813198,
    "fraction": 2534030704,
    "rtp": 1703076439
  },
  {
    "port": 61021,
    "ssrc": 4088266843,
    "seconds": 4003813199,
    "fraction": 176093659,
    "rtp": 2520278305
  },
  {
    "port": 61023,
    "ssrc": 1932352396,
    "seconds": 4003813203,
    "fraction": 2641404887,
    "rtp": 1703298041
  },
  {
    "port": 61021,
    "ssrc": 4088266843,
    "seconds": 4003813204,
    "fraction": 197568495,
    "rtp": 2520728755
  },
  {
    "port": 61023,
    "ssrc": 1932352396,
    "seconds": 4003813208,
    "fraction": 2714419331,
    "rtp": 1703519291
  },
  {
    "port": 61021,
    "ssrc": 4088266843,
    "seconds": 4003813209,
    "fraction": 214748364,
    "rtp": 2521179115
  },
  {
    "port": 61023,
    "ssrc": 1932352396,
    "seconds": 4003813213,
    "fraction": 2765958938,
    "rtp": 1703740320
  },
  {
    "port": 61021,
    "ssrc": 4088266843,
    "seconds": 4003813214,
    "fraction": 390842023,
    "rtp": 2521632805
  },
  {
    "port": 61023,
    "ssrc": 1932352396,
    "seconds": 4003813218,
    "fraction": 2821793513,
    "rtp": 1703961393
  },
  {
    "port": 61021,
    "ssrc": 4088266843,
    "seconds": 4003813219,
    "fraction": 506806140,
    "rtp": 2522085235
  },
  {
    "port": 61021,
    "ssrc": 4088266843,
    "seconds": 4003813222,
    "fraction": 3058016714,
    "rtp": 2522408695
  },
  {
    "port": 61023,
    "ssrc": 1932352396,
    "seconds": 4003813222,
    "fraction": 3062311682,
    "rtp": 1704140263
  }
]
### 清理清单
远端：evidence.json中的files加evidence.json。
本机：
composition-rk453-attribution.json
composition-rk453-audio-map.json
composition-rk453-capture.log
composition-rk453-capture.out
composition-rk453-content-result.json
composition-rk453-evidence.json
composition-rk453-input-audio.log
composition-rk453-input-map.json
composition-rk453-input-marker-01.png
composition-rk453-input-marker-02.png
composition-rk453-input-marker-03.png
composition-rk453-input-video.log
composition-rk453-input.aac
composition-rk453-input.f32
composition-rk453-input.h264
composition-rk453-long-audio-match.json
composition-rk453-marker-01.png
composition-rk453-marker-02.png
composition-rk453-marker-03.png
composition-rk453-marker-04.png
composition-rk453-marker-05.png
composition-rk453-marker-06.png
composition-rk453-native-2026-11-16-18h19m21s329.png
composition-rk453-native-2026-11-16-18h21m11s874.png
composition-rk453-output.pcapng
composition-rk453-output.ts
composition-rk453-received-audio.log
composition-rk453-received.f32
composition-rk453-reference-audio.log
composition-rk453-reference.f32
composition-rk453-source2.log
composition-rk453-source2.out
composition-rk453-source3.log
composition-rk453-source3.out
composition-rk453-video-analysis.log
composition-rk453-vlc.log
composition-rk453-wire.json

清理完成：远端17项、本机37项逐文件删除，5个远端及4个本机精确PID无残留，指定源和正式产物保持。
