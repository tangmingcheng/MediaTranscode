# rk456：原规格发送批次与失败收尾取证

状态：有限内容对应与SR归因 PASS；完整音频尾部未覆盖，整体合屏未完成。目标临时脚本已经转换 LF 并校验不含 CR。核心与两端 FFmpeg 不变。四源完整120秒，1280×720/30fps；H264复制两路、选中H264 CBR8200kbps及AAC复制、HEVC CBR8000kbps；输出H264 VBR目标8/范围8–12Mbps、AAC192kbps MPEG-TS/RTP。与rk454相同启动顺序。归档现有batch last_completed/active诊断；仅失败后使用目标工具链GDB抓栈，暂停后的计时不用于媒体期限分析。

## 真实命令

远端脚本（上传至out/rk456-run.sh后复制到acceptance/rk456/run.sh）：

```bash
#!/usr/bin/env bash
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on
export PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin:$PATH
export LD_LIBRARY_PATH=/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/lib:/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-mpp/lib:/opt/mt-tools/gcc-12.4-native/lib64
set -u
cd /home/tang/MediaTranscode-composition-09093919/acceptance/rk456
cat /proc/net/snmp >udp-before.txt
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200' >source0.out 2>source0.log &
s0=$!
/home/tang/MediaTranscode-composition-09093919/out/dependency-work/prefix-ffmpeg/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200' >source1.out 2>source1.log &
s1=$!
/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --media-id composition-rk456 --inputs '--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --inputs '--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://192.168.130.229:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp '"'"'profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210'"'"'' --inputs '--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://192.168.130.229:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000' --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 192.168.96.122 --rtp-port 62024 --sdp /home/tang/MediaTranscode-composition-09093919/acceptance/rk456/output.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2 >cli.log 2>cli-error.log &
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

启动：
```bash
bash acceptance/rk456/run.sh >acceptance/rk456/launcher.log 2>&1 &
```

```powershell
$p=Start-Process -FilePath 'D:/Wireshark/dumpcap.exe' -ArgumentList '-i 7 -f "udp and (dst port 61020 or dst port 61021 or dst port 61022 or dst port 61023 or dst port 62024 or dst port 62025)" -a duration:420 -w D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk456-output.pcapng' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk456-capture.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk456-capture.log' -PassThru; Write-Output "CAPTURE_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "CAPTURE_EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -vf "drawtext=fontfile=''C\:/Windows/Fonts/consola.ttf'':text=''frame=%{frame_num} pts=%{pts}'':fontsize=48:fontcolor=white:box=1:boxcolor=black:x=16:y=16" -c:v h264_nvenc -rc cbr -b:v 8200k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61020?rtcpport=61021&pkt_size=1200&localaddr=192.168.96.122" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://192.168.130.229:61022?rtcpport=61023&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk456-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk456-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE2_EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://192.168.130.229:61050?rtcpport=61051&pkt_size=1200&localaddr=192.168.96.122"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk456-source3.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk456-source3.log' -PassThru; Write-Output "SOURCE3_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE3_EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-rk456-vlc.log --extraintf=rc --rc-host=127.0.0.1:42120 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-rk456-native- --snapshot-format=png rtp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "VLC_EXIT=$($p.ExitCode)"
```


PID：目标wrapper2919428，source0/1=2919435/2919436，CLI2919438，monitor2919441；本机capture40800，source2/3=2452/43876，VLC33320。CLI SHA256=ab5329a1ffa9d810e4f525ae467bf7c14c5cb205d1b1131ef462d32efc4eacfe。

18h45m24s874 VLC原生快照已观察：四格有画面，左下frame989/pts32.966667。源结束前89秒RSS79632KiB，CPU149%单核口径；内部av_drift raw_phase9070ns/frequency0，不能代替内容同步。

## 运行与抓包结果

四路源完整120秒退出0；全黑原生快照18h47m05s174已观察。CLI2919438通过kill -INT请求停止，caller_stop/exit0。没有发生运行失败，因此未attach GDB，rk454根因仍未关闭。capture236041包，接口drop0；输出121601媒体RTP/768869 TS包/144547372字节，RTP序号、TS CC、TEI错误均0。输入3600视频AU/5168音频AU、50SR、2BYE，无序号丢失。

~~~text
[2026-11-16 18:47:06.981] [info] [graph][runtime.node] datagram_batch_trace stage=last_completed generation=1 first_sequence=121642 last_sequence=121642 maximum_submit_interval_ns=121044 maximum_submit_interval_sequence=121642 maximum_start_lateness_ns=770597 maximum_start_lateness_sequence=121642 first_release_master_ns=176143627664 first_deadline_master_ns=176243627664 last_release_master_ns=176143627664 last_deadline_master_ns=176243627664 first_submitted_master_ns=176144519305 last_submitted_master_ns=176144519305 worst_wait_started_master_ns=176144395928 worst_wait_completed_master_ns=176144395928 worst_submit_observed_master_ns=176144398261 wait_attempts=0 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=0 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=0 wait_call_total_ns=0 wait_call_max_ns=0 wait_before_call_count=0 wait_before_call_total_ns=0 wait_before_call_max_ns=0 wait_arm_count=0 wait_arm_total_ns=0 wait_arm_max_ns=0 wait_api_wall_count=0 wait_api_wall_total_ns=0 wait_api_wall_max_ns=0 wait_return_to_next_clock_count=0 wait_return_to_next_clock_total_ns=0 wait_return_to_next_clock_max_ns=0 wait_target_lateness_count=0 wait_target_lateness_total_ns=0 wait_target_lateness_max_ns=0
[2026-11-16 18:47:06.982] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:2 execution_mode=1 committed_batches=18013 committed_datagrams=121642 committed_payload_bytes=146009380 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=85 ordered_queue_maximum_wire_bytes=115260 pacing_reserved=121642 pacing_cancelled=0 pacing_submitted=121642 pacing_maximum_submit_lateness_ns=32404244 pacing_worst_late_global_sequence=2400 pacing_worst_success_sample_available=1 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=85 backlog_high_water_wire_bytes=115260 backlog_max_residence_ns=51320215 last_materialized_sequence=121642 last_scheduled_sequence=121642 last_submitted_sequence=121642 last_committed_sequence=121642 submitted_service_generation=1 submitted_service_sequence=121642 submitted_service_completed_ns=176144519305 submitted_service_duration_ns=16640 submitted_service_available_ns=176144535945 submitted_scope_available_ns=176144535945 shared_scope_charged_datagrams=121642 shared_scope_charged_wire_bytes=149415356 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=1153 evidence_timestamp_tracked=0 evidence_timestamp_untracked=1153 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=554648 endpoint_1_target_effective_socket_bytes=277324 endpoint_1_api_requested_socket_bytes=138662 endpoint_1_effective_socket_bytes=277324 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=121601 endpoint_1_committed_payload_bytes=146006584 endpoint_2_target_effective_socket_bytes=277324 endpoint_2_api_requested_socket_bytes=138662 endpoint_2_effective_socket_bytes=277324 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=41 endpoint_2_committed_payload_bytes=2796
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=1598, threads=102, processThreads=148, workers=0, workerProgress=374395, workerProcessCalls=1767438, workerWaits=1393024, workerWakeups=1370909, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=659, logicalProcessors=8, averageSystemMachineCpuPercent=36.012713, averageProcessMachineCpuPercent=16.681658, peakProcessMachineCpuPercent=43.979058, averageProcessSingleCoreCpuPercent=133.453268, peakProcessSingleCoreCpuPercent=351.832461, initialWorkingSetBytes=68403200, workingSetBytes=81543168, peakWorkingSetBytes=81543168, totalPushed=337251, totalPopped=337038, droppedBuffers=0, encodedPacketsPushed=55671, encodedPacketsPopped=55671, backpressureItems=147, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=15921662, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=832, graphPayloadReservations=185099, graphPayloadReleases=185099, graphPayloadPressureFailures=8, lastInputReceivedAtNs=26528128326161
~~~

## 内容候选与输入时钟（后续消歧见下一节）

输出PTS 1/10/30/60/90/105秒的左下标记分别为frame30/300/900/1800/2700/3150，即视频源PTS与输出PTS在这六点相同。六个一秒音频窗口最高候选均为源时间领先输出213.582766ms；110秒完整窗口全参考valid搜索最高峰0.998854044（源起点1.213582766），但9.480249433秒另有0.995999697近峰，不能仅依最高峰宣称无歧义。

实际源ffprobe首AAC packet PTS=-1024、Skip Samples=1024；原始输入AAC解码5292032样本，剔除实际1024后5291008样本与参考逐位相等，参考末992样本未覆盖。输入122个fragment packets已重组，无RTP序号缺失；25视频/25音频SR映射得到输入基准偏差210.770974856–210.791383369ms。仅在最高候选成立的前提下，内容残差为2.791383071–2.811791584ms，该范围来自SR变化，不是统计置信区间。

运行pressureFailures=8、MPP task no send to hal que must clr current frame hal status以及Changing video frame properties/range unknown→tv警告保留；未证明警告导致媒体损坏。外部monitor164样本，RSS范围13084–80772KiB；短程不是长期内存证明。

## 独立留出段复核：有限内容对应与SR归因 PASS

110秒拟合使用输出PTS1..111，原全源搜索保留周期竞争峰；随后独立检查未参与拟合的输出112..118秒，不按视频限制lag。最高候选偏移9419样本，将此段映射到原源112.213582766..118.213582766秒，264600样本相关0.9989444625454587、RMSE0.008659722789568966。竞争候选373979样本要求原源120.480249433..126.480249433秒，超出实际完整120秒有限源；输出留出段却有真实内容，RMS0.18410561005223147。因此竞争峰不能解释全段统一内容映射；不是新增相关阈值或按视频选峰，也不证明中间所有时刻绝无跳变。

结合前述输入AU/实际Skip Samples及SR证据，本轮有限内容对应与SR基准归因判PASS：输出相对差213.582766440ms，输入基准差210.770974856..210.791383369ms，扣除后采样残差2.791383071..2.811791584ms。此区间仅反映SR映射变化，不是完整统计置信区间；不等于首次有效输出、全程分位数、音频尾部完整、rk454期限故障修复或长期可靠性通过。

末尾边界必须保留：参考最后非零样本119.999977324秒，输出最后非零样本118.518979592秒。按最高候选映射，参考末约1.267414966秒未在输出连续内容中体现，原因未定位；这与输入解码对参考末992样本未覆盖是不同边界，不能混同或宣称完整音频收全。

独立复核实际使用D:/Python/python.exe -c直接内存计算，未创建脚本或其他文件。以下单条只读命令复现留出段与尾部数值，输入为本轮已记录并随后清理的PCM：

```powershell
& D:/Python/python.exe -c 'import json,numpy as np;from pathlib import Path;p=Path("out/acceptance");b="composition-rk456-";r=np.fromfile(p/(b+"reference.f32"),dtype="<f4").astype(float);o=np.fromfile(p/(b+"received.f32"),dtype="<f4").astype(float);j=json.loads((p/(b+"long-audio-match.json")).read_text());lo=112*44100;hi=118*44100;y=o[lo:hi];d=round((j["other_peaks"][0]["source_start_pts"]-1)*44100);x=r[lo+d:hi+d];other=round((j["other_peaks"][1]["source_start_pts"]-1)*44100);print({"samples":len(x),"correlation":float(np.dot(x,y)/(np.linalg.norm(x)*np.linalg.norm(y))),"rmse":float(np.sqrt(np.mean((x-y)**2))),"output_rms":float(np.sqrt(np.mean(y*y))),"reference_interval":((lo+d)/44100,(hi+d)/44100),"other_reference_interval":((lo+other)/44100,(hi+other)/44100),"reference_last_nonzero":np.flatnonzero(r)[-1]/44100,"output_last_nonzero":np.flatnonzero(o)[-1]/44100,"unrepresented_tail_seconds":(np.flatnonzero(r)[-1]-np.flatnonzero(o)[-1]-d)/44100})'
```

## 音频尾部原始边界记录

~~~text
[2026-11-16 18:46:19.441] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=26528127831194
[2026-11-16 18:46:19.441] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=3 generation=2 invalidated_generation=1 locked=0
[2026-11-16 18:46:19.441] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=26528127798527
[2026-11-16 18:46:19.441] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=3 generation=2 invalidated_generation=1 locked=0
[2026-11-16 18:46:19.441] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=51 group=composition-rk456.source.2 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=48 video_raw_generation=0 video_ssrc=3456992425 audio_raw_generation=0 audio_ssrc=95021349
[2026-11-16 18:46:19.441] [info] [graph][runtime.node] av_reacquisition group=composition-rk456.source.2 phase=purging old=1 next=2 transition=1
[2026-11-16 18:46:19.443] [info] [graph][runtime.node] rtp_ingress_purge node=46 transition=1 cutoff=0
[2026-11-16 18:46:19.443] [info] [graph][runtime.node] rtp_ingress_purge node=47 transition=1 cutoff=0
[2026-11-16 18:46:19.443] [info] [graph][runtime.node] rtp_binder_purge node=51 transition=1 cutoff=5193
[2026-11-16 18:46:19.444] [info] [graph][runtime.node] rtp_binder_purge node=50 transition=1 cutoff=3623
[2026-11-16 18:46:19.444] [info] [graph][runtime.node] av_reacquisition group=composition-rk456.source.2 event=purge_completed purge_ack=complete continuation=acquiring ack_count=2 old=1 next=2 transition=1
[CLI] runtime report: queued=2, peakQueued=1596, threads=102, processThreads=148, workers=95, workerProgress=335890, workerProcessCalls=1666711, workerWaits=1330813, workerWakeups=1318043, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=472, logicalProcessors=8, averageSystemMachineCpuPercent=40.357243, averageProcessMachineCpuPercent=21.387616, peakProcessMachineCpuPercent=43.979058, averageProcessSingleCoreCpuPercent=171.100931, peakProcessSingleCoreCpuPercent=351.832461, initialWorkingSetBytes=68403200, workingSetBytes=81543168, peakWorkingSetBytes=81543168, totalPushed=307615, totalPopped=307400, droppedBuffers=0, encodedPacketsPushed=39759, encodedPacketsPopped=39758, backpressureItems=147, graphPayloadCurrentBytes=56, graphPayloadHighWaterBytes=15921662, graphPayloadCurrentObjects=6, graphPayloadHighWaterObjects=832, graphPayloadReservations=169026, graphPayloadReleases=169020, graphPayloadPressureFailures=8, lastInputReceivedAtNs=26528128326161
[2026-11-16 18:46:14.259] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4938 source_master_ns=124921597686 raw_phase_ns=9070 filtered_phase_ns=9070 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=240641 effective_output_sample_index=5294102
[2026-11-16 18:47:07.181] [info] [graph][runtime.node] continuous_audio_aggregate.summary reason=stop output_group=composition-rk456.output audio_commits=7149 real_samples=5226496 silence_samples=2094080
[2026-11-16 18:47:07.277] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=linux_receive_multiple_messages channel=rtp socket_capacity_bytes=4194304 batch_capacity_bytes=4192448 descriptor_capacity=64 maximum_datagram_bytes=65507 receive_calls=4987 receive_syscalls=4963 completed_datagrams=5185 would_block=23 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
~~~

## 最终边界与清理清单

源2的SenderLeft/BYE在18:46:19.441触发clock invalidation和generation purge，比最终caller stop早约48秒；共享tracker/aggregate会撤销播放授权并清队列，是尾部缺失的待证候选，不能直接把64AU差额全归于purge。Windows复用相同路径，后续若修共享代码需同规格回归，不能修改Windows FFmpeg替代。

目标全局UDP增量NoPorts/InErrors/RcvbufErrors/SndbufErrors/InCsumErrors均0，IgnoredMulti28；全局计数不能证明逐源网络零丢包。VLC与capture正常退出0。目标5个PID均已不存在；本机4个PID最终复核均不存在。

本机本轮37项：
~~~text
composition-rk456-attribution.json
composition-rk456-audio-map.json
composition-rk456-capture.log
composition-rk456-capture.out
composition-rk456-cli.log
composition-rk456-content-result.json
composition-rk456-input-audio.log
composition-rk456-input-map.json
composition-rk456-input-marker-01.png
composition-rk456-input-marker-02.png
composition-rk456-input-marker-03.png
composition-rk456-input-video.log
composition-rk456-input.aac
composition-rk456-input.f32
composition-rk456-input.h264
composition-rk456-long-audio-match.json
composition-rk456-marker-01.png
composition-rk456-marker-02.png
composition-rk456-marker-03.png
composition-rk456-marker-04.png
composition-rk456-marker-05.png
composition-rk456-marker-06.png
composition-rk456-native-2026-11-16-18h45m24s874.png
composition-rk456-native-2026-11-16-18h47m05s174.png
composition-rk456-output.pcapng
composition-rk456-output.ts
composition-rk456-received-audio.log
composition-rk456-received.f32
composition-rk456-reference-audio.log
composition-rk456-reference.f32
composition-rk456-source2.log
composition-rk456-source2.out
composition-rk456-source3.log
composition-rk456-source3.out
composition-rk456-video-analysis.log
composition-rk456-vlc.log
composition-rk456-wire.json
~~~
远端16项：cli-error.log, cli-maps.txt, cli.log, launcher.log, monitor.log, output.sdp, pids.txt, result.txt, run.sh, socket-monitor.log, source0.log, source0.out, source1.log, source1.out, udp-after.txt, udp-before.txt。逐项清除这些临时媒体、截图、日志、脚本，保留原120秒源、所有构建与回退目录。

清理复核：本机37项和远端16项共53项均已删除；本机4/远端5个精确PID均无残留。
