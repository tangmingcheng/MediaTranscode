# r449：最终共享版本 Windows 四源合屏回归

状态：本轮功能回归通过，完整交付门禁尚未通过。生产源码ce71265d，包含编码完成合同及两项停止生命周期修复；此前各修改已双独立SOURCE PASS。仅测试realtime，Windows FFmpeg保持不变。

原四路120秒H264/H264/H264+AAC/HEVC RTP→H2641280×720/30fps VBR目标8/范围8–12Mbps、AAC192kbps MPEG-TS/RTP。全部失源后选中source2再完整120秒重入，VLC原生快照检查四格/全黑/重入/再全黑，再显式停止。若任何阶段失败即保留原错，不将旧通过记录覆盖本轮。

## 实际执行命令

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r449-source0.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r449-source0.log' -PassThru; Write-Output "SOURCE0_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE0_EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r449-source1.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r449-source1.log' -PassThru; Write-Output "SOURCE1_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE1_EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r449-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r449-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE2_EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61050?rtcpport=61051&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r449-source3.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r449-source3.log' -PassThru; Write-Output "SOURCE3_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE3_EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r449 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --inputs "--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62024 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r449-output.sdp --video-codec h264 --rc vbr --min-bitrate 8000 --max-bitrate 12000 --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r449-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r449-cli-error.log' -PassThru; Write-Output "CLI_PID=$($p.Id)"; while(-not $p.WaitForExit(500)){ $p.Refresh(); Write-Output "CLI_SAMPLE utc=$([DateTime]::UtcNow.ToString('o')) cpu_ms=$($p.TotalProcessorTime.TotalMilliseconds) working_set=$($p.WorkingSet64) private=$($p.PrivateMemorySize64)" }; $p.WaitForExit(); Write-Output "CLI_EXIT=$($p.ExitCode)"; exit $p.ExitCode
```

```powershell
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r449-vlc.log --extraintf=rc --rc-host=127.0.0.1:42120 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r449-native- --snapshot-format=png rtp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "VLC_EXIT=$($p.ExitCode)"
```

## 实际重入

```powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r449-reentry.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r449-reentry.log' -PassThru; Write-Output "REENTRY_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "REENTRY_EXIT=$($p.ExitCode)"; exit $p.ExitCode
```


全核心Release clean-first/all-target构建configure/build exit0，观察24个cl并行，四必需产物存在。CLI SHA256 8B4836F09F3E46EDB72971BA516C8F3D17CE0795ED28CC65950D8A477D52A53F，7599616字节。源0/1/2/3 PID24716/38168/43668/42884，CLI38304，VLC37244。实际NVENC prepared三次authorized1，CodecCopyOpaque；首AU order1/packet PTS=DTS=submittedPTS0，当前errors0。

## 实测结论与边界

本轮功能回归 PASS：四路原源和 source2 重入源均完整120秒、exit0；重入 PID32012。四张 VLC 原生快照依次确认四格有画面、全部黑屏、左下 source2 恢复、再次全部黑屏。停止源后 CLI 按合屏语义继续输出；随后发送调用方停止，CLI38304 exit0、end_reason=caller_stop；VLC37244 quit 后 exit0。这不是源结束导致 CLI 自然退出。

最终 errors/workerErrors/droppedBuffers/pressureFailures=0，ledger reservations=releases=329191，当前字节/对象=0，峰值20710839字节/1872对象。平均进程整机CPU1.942121%，单核口径42.726663%，WS初始240771072、末次307589120、峰值311468032字节。长时间失源输出不能替代持续有源可靠性测试。generation2末次内部相位15873ns，频率/补偿0ppm，不作为接收端内容A/V同步证明。

发送224513个数据报，其中媒体224378、控制135；deadline_misses=0，最大提交迟到18694732ns、最大队列停留42130300ns；delivery_evidence=not_proven。实际观察到 av_scheduler_video_progress_release：17:34:21.903，node94 generation1 audio3287，audio_dispatch76300770975 < bound76333333333，observed_after_output_commit=1；本轮存在提交后水位放行正例。历史rk427拒绝未复现，根因仍未确定，不能据此宣称已修复。

保留以下原始stderr，HEVC参考帧/解码告警与色彩范围告警未消失；本轮画面观察通过不证明其根因已消除，也不构成修改 Windows FFmpeg 的依据。尚待接收端内容A/V、wire证据、RKMPP最新停止版本活动输出回归与最终整体双审。

### 实际停止命令

```powershell
$target=Get-CimInstance Win32_Process -Filter 'ProcessId=38304'
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r449*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R449Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R449Console]::FreeConsole() | Out-Null
if(-not [R449Console]::AttachConsole(38304)){throw 'AttachConsole failed'}
try {
[R449Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R449Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 38304 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R449Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R449Console]::FreeConsole() | Out-Null}
```

VLC经本轮RC127.0.0.1:42120发送`quit`结束。

### 原始诊断与最终报告

[in @ 000001d869985700] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001d869985700] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 28407.947967
[in @ 000001d85f2ecf00] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001d85f2ecf00] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 8391.720467
[in @ 000001d875b139c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001d875b139c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 4110.159422
[in @ 000001d876061a00] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001d876061a00] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 47703.502722
[hevc @ 000001d85c0e8880] Could not find ref with POC 40
[hevc @ 000001d85c0e8880] Error constructing the frame RPS.
[hevc @ 000001d85c0e8880] Skipping invalid undecodable NALU: 0
[hevc @ 000001d85c0e8880] Could not find ref with POC 42
[hevc @ 000001d85c0e8880] Error constructing the frame RPS.
[hevc @ 000001d85c0e8880] Skipping invalid undecodable NALU: 0
[hevc @ 000001d85c0e8880] Could not find ref with POC 44
[hevc @ 000001d85c0e8880] Error constructing the frame RPS.
[hevc @ 000001d85c0e8880] Skipping invalid undecodable NALU: 1
[hevc @ 000001d85c0e8880] Could not find ref with POC 44
[hevc @ 000001d85c0e8880] Error constructing the frame RPS.
[hevc @ 000001d85c0e8880] Skipping invalid undecodable NALU: 1
[hevc @ 000001d85c0e8880] Could not find ref with POC 44
[hevc @ 000001d85c0e8880] Error constructing the frame RPS.
[hevc @ 000001d85c0e8880] Skipping invalid undecodable NALU: 0
[hevc @ 000001d85c0e8880] Could not find ref with POC 46
[hevc @ 000001d85c0e8880] Error constructing the frame RPS.
[hevc @ 000001d85c0e8880] Skipping invalid undecodable NALU: 0
[hevc @ 000001d85c0e8880] Could not find ref with POC 48
[hevc @ 000001d85c0e8880] Error constructing the frame RPS.
[hevc @ 000001d85c0e8880] Skipping invalid undecodable NALU: 1
[hevc @ 000001d85c0e8880] Could not find ref with POC 48
[hevc @ 000001d85c0e8880] Error constructing the frame RPS.
[hevc @ 000001d85c0e8880] Skipping invalid undecodable NALU: 1
[hevc @ 000001d85c0e8880] Could not find ref with POC 48
[hevc @ 000001d85c0e8880] Error constructing the frame RPS.
[hevc @ 000001d85c0e8880] Skipping invalid undecodable NALU: 0
[hevc @ 000001d85c0e8880] Could not find ref with POC 50
[hevc @ 000001d85c0e8880] Error constructing the frame RPS.
[hevc @ 000001d85c0e8880] Skipping invalid undecodable NALU: 0
[hevc @ 000001d85c0e8880] Could not find ref with POC 52
[hevc @ 000001d85c0e8880] Error constructing the frame RPS.
[hevc @ 000001d85c0e8880] Skipping invalid undecodable NALU: 1
[hevc @ 000001d85c0e8880] Could not find ref with POC 52
[hevc @ 000001d85c0e8880] Error constructing the frame RPS.
[hevc @ 000001d85c0e8880] Skipping invalid undecodable NALU: 1
[hevc @ 000001d85c0e8880] Could not find ref with POC 52
[hevc @ 000001d85c0e8880] Error constructing the frame RPS.
[hevc @ 000001d85c0e8880] Skipping invalid undecodable NALU: 0
[hevc @ 000001d85c0e8880] Could not find ref with POC 54
[hevc @ 000001d85c0e8880] Error constructing the frame RPS.
[hevc @ 000001d85c0e8880] Skipping invalid undecodable NALU: 0
[hevc @ 000001d85c0e8880] Could not find ref with POC 56
[hevc @ 000001d85c0e8880] Error constructing the frame RPS.
[hevc @ 000001d85c0e8880] Skipping invalid undecodable NALU: 1
[hevc @ 000001d85c0e8880] Could not find ref with POC 56
[hevc @ 000001d85c0e8880] Error constructing the frame RPS.
[hevc @ 000001d85c0e8880] Skipping invalid undecodable NALU: 1
[hevc @ 000001d85c0e8880] Could not find ref with POC 56
[hevc @ 000001d85c0e8880] Error constructing the frame RPS.
[hevc @ 000001d85c0e8880] Skipping invalid undecodable NALU: 0
[in @ 000001d87a56a840] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001d87a56a840] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 28407.947967
[in @ 000001d876485980] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001d876485980] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 4110.159422
[in @ 000001d879ab6940] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001d879ab6940] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 8391.720467
[in @ 000001d876482e00] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001d876482e00] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 47703.502722
[aac @ 000001d85c0e84c0] Qavg: 39214.102

[2026-11-16 17:42:32.623] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=61279 committed_datagrams=224513 committed_payload_bytes=238191088 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=51 ordered_queue_maximum_wire_bytes=68028 pacing_reserved=224513 pacing_cancelled=0 pacing_submitted=224513 pacing_maximum_submit_lateness_ns=18694732 pacing_worst_late_global_sequence=211636 pacing_worst_success_sample_available=1 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=51 backlog_high_water_wire_bytes=68028 backlog_max_residence_ns=42130300 last_materialized_sequence=224513 last_scheduled_sequence=224513 last_submitted_sequence=224513 last_committed_sequence=224513 submitted_service_generation=1 submitted_service_sequence=224513 submitted_service_completed_ns=577011599500 submitted_service_duration_ns=16640 submitted_service_available_ns=577011616140 submitted_scope_available_ns=577011616140 shared_scope_charged_datagrams=224513 shared_scope_charged_wire_bytes=244477452 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2155 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2155 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=554646 endpoint_1_target_effective_socket_bytes=277323 endpoint_1_api_requested_socket_bytes=277323 endpoint_1_effective_socket_bytes=277323 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=224378 endpoint_1_committed_payload_bytes=238181900 endpoint_2_target_effective_socket_bytes=277323 endpoint_2_api_requested_socket_bytes=277323 endpoint_2_effective_socket_bytes=277323 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=135 endpoint_2_committed_payload_bytes=9188
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=2261, threads=102, processThreads=170, workers=0, workerProgress=811644, workerProcessCalls=3778855, workerWaits=2967192, workerWakeups=2924328, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=1653, logicalProcessors=22, averageSystemMachineCpuPercent=7.984563, averageProcessMachineCpuPercent=1.942121, peakProcessMachineCpuPercent=11.293634, averageProcessSingleCoreCpuPercent=42.726663, peakProcessSingleCoreCpuPercent=248.459959, initialWorkingSetBytes=240771072, workingSetBytes=307589120, peakWorkingSetBytes=311468032, totalPushed=690840, totalPopped=690573, droppedBuffers=0, encodedPacketsPushed=190145, encodedPacketsPopped=190145, backpressureItems=147, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=20710839, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=1872, graphPayloadReservations=329191, graphPayloadReleases=329191, graphPayloadPressureFailures=0, lastInputReceivedAtNs=32121357471500

### 本轮清理清单

composition-r449-cli-error.log
composition-r449-cli.log
composition-r449-native-2026-11-16-17h33m43s516.png
composition-r449-native-2026-11-16-17h35m16s509.png
composition-r449-native-2026-11-16-17h36m11s688.png
composition-r449-native-2026-11-16-17h37m43s414.png
composition-r449-output.sdp
composition-r449-reentry.log
composition-r449-reentry.out
composition-r449-source0.log
composition-r449-source0.out
composition-r449-source1.log
composition-r449-source1.out
composition-r449-source2.log
composition-r449-source2.out
composition-r449-source3.log
composition-r449-source3.out
composition-r449-vlc.log
### 水位放行原始证据
[2026-11-16 17:34:21.903] [info] [graph][runtime.node] av_scheduler_video_progress_release node=94 generation=1 audio_sequence=3287 audio_canonical_dispatch_ns=76300770975 video_canonical_lower_bound_ns=76333333333 observed_after_output_commit=1

清理结果：以上18项本轮产物已逐项删除；7个精确PID均无残留。复用120秒源及正式构建产物保持不变。
