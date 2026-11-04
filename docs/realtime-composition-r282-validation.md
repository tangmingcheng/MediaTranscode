# r282：Windows Release realtime 跨重试准备对象验证

状态：以下命令已执行，功能画面观察完成，完整验收门禁未通过。原规格三路 H.264 1280×720 30fps 约8 Mbps，输出 HEVC 1280×720 30fps 8 Mbps CBR 与 AAC 192 kbps/44100 Hz/双声道独立 RTP。指定120秒连续源，不循环、不降规格。画面只使用 VLC 原生 snapshot。

## 本轮命令
~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r282-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r282-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r282-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r282-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r282-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r282-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r282 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout separate --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r282-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r282-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r282-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10 -and (Test-Path -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r282-output.sdp')) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r282-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r282-native- --snapshot-format=png file:///D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r282-output.sdp' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r282-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
$lastTime=$time
$lastCpu=$cpu
}
Write-Output "CLI_EXIT=$($cli.ExitCode)"
$source0.WaitForExit()
Write-Output "SOURCE0_EXIT=$($source0.ExitCode)"
$source1.WaitForExit()
Write-Output "SOURCE1_EXIT=$($source1.ExitCode)"
$source2.WaitForExit()
Write-Output "SOURCE2_EXIT=$($source2.ExitCode)"
exit $cli.ExitCode


~~~

## 重入、快照与停止命令
~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r282-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r282-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($source0.Id)"
$source0.WaitForExit()
Write-Output "REENTRY_EXIT=$($source0.ExitCode)"
exit $source0.ExitCode
$client=[Net.Sockets.TcpClient]::new('127.0.0.1',63020); try {$stream=$client.GetStream(); $stream.ReadTimeout=2000; $bytes=[Text.Encoding]::ASCII.GetBytes("status`nsnapshot`n"); $stream.Write($bytes,0,$bytes.Length); $buffer=New-Object byte[] 8192; try {while(($count=$stream.Read($buffer,0,$buffer.Length)) -gt 0){[Text.Encoding]::UTF8.GetString($buffer,0,$count)}} catch {Write-Output 'RC read complete'}} finally {$client.Dispose()}
$ErrorActionPreference='Stop'
$target=Get-CimInstance Win32_Process -Filter "ProcessId=41312"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r282*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R282Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R282Console]::FreeConsole() | Out-Null
if(-not [R282Console]::AttachConsole(41312)){throw 'AttachConsole failed'}
try {
[R282Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R282Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 41312 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R282Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R282Console]::FreeConsole() | Out-Null}


~~~

## 本轮结果

最终构建 configure/build 均0，727项完成，必需产物齐全；realtime SHA256 724C69C9F8CB2106C0A37DA31403E9ED54098B38B241CF6211331A0967C797BF。时钟修复后两独立审查均 PASS（仅此RTP owner接线）。
PID：source0=7912、source1=23980、source2=40296、CLI=41312、VLC=6680、重入=33916。四次源退出码均0；CLI caller_stop=0，不是自然EOF。启动监控session30424及重入session3295均完成0。
VLC四张原生快照已逐张查看：三格→全黑→左下格重入→全黑。所有源完整120秒/3600帧。未做系统截图。
整体门禁未通过：delivery_evidence=not_proven，接收端A/V和wire交付证据仍缺失；没有据此创建成功提交。内部phase只代表引擎遥测。UDP/terminal跨重试owner、C+W/ledger及MPEG-TS实际复验仍未完成。
本轮没有逐AU preparation/global WouldBlock专用观测，因此不能由全程错误0证明触发了每条存储保留路径。静态所有权证明与本次生产运行观察分别记录。
### 最终遥测
~~~text
[2026-10-21 10:58:19.957] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=22768 committed_datagrams=155888 committed_payload_bytes=196949148 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=53 ordered_queue_maximum_wire_bytes=78147 pacing_reserved=155888 pacing_cancelled=0 pacing_submitted=155888 pacing_maximum_submit_lateness_ns=52573580 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=76 backlog_high_water_wire_bytes=111710 backlog_max_residence_ns=59719100 last_materialized_sequence=155888 last_scheduled_sequence=155888 last_submitted_sequence=155888 last_committed_sequence=155888 shared_scope_charged_datagrams=155888 shared_scope_charged_wire_bytes=201314012 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=3806 evidence_timestamp_tracked=0 evidence_timestamp_untracked=3806 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=756580 endpoint_1_target_effective_socket_bytes=189145 endpoint_1_api_requested_socket_bytes=189145 endpoint_1_effective_socket_bytes=189145 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=142321 endpoint_1_committed_payload_bytes=191095302 endpoint_2_target_effective_socket_bytes=189145 endpoint_2_api_requested_socket_bytes=189145 endpoint_2_effective_socket_bytes=189145 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=79 endpoint_2_committed_payload_bytes=5380 endpoint_3_target_effective_socket_bytes=189145 endpoint_3_api_requested_socket_bytes=189145 endpoint_3_effective_socket_bytes=189145 endpoint_3_timestamp_source=0 endpoint_3_committed_datagrams=13415 endpoint_3_committed_payload_bytes=5843494 endpoint_4_target_effective_socket_bytes=189145 endpoint_4_api_requested_socket_bytes=189145 endpoint_4_effective_socket_bytes=189145 endpoint_4_timestamp_source=0 endpoint_4_committed_datagrams=73 endpoint_4_committed_payload_bytes=4972
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=961, threads=80, processThreads=129, workers=0, workerProgress=495187, workerProcessCalls=2023429, workerWaits=1528226, workerWakeups=1505405, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=984, logicalProcessors=22, averageSystemMachineCpuPercent=9.710781, averageProcessMachineCpuPercent=1.610284, peakProcessMachineCpuPercent=5.360825, averageProcessSingleCoreCpuPercent=35.426246, peakProcessSingleCoreCpuPercent=117.938144, initialWorkingSetBytes=208388096, workingSetBytes=261820416, peakWorkingSetBytes=269873152, totalPushed=454277, totalPopped=454241, droppedBuffers=0, encodedPacketsPushed=81701, encodedPacketsPopped=81701, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=16957016, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=743, graphPayloadReservations=257963, graphPayloadReleases=257963, graphPayloadPressureFailures=0, lastInputReceivedAtNs=8067034873200
~~~
### 内部音频相位与事件计数
~~~json
{
  "phase_by_generation": {
    "1": {
      "samples": 113,
      "minimum_ns": 2267,
      "maximum_ns": 2269
    },
    "2": {
      "samples": 117,
      "minimum_ns": 11337,
      "maximum_ns": 11338
    }
  },
  "event_counts": {
    "late_output_commit": 0,
    "immutable emission deadline": 0,
    "[error]": 0
  }
}
~~~

### source0 完成
~~~text
[out#0/rtp @ 00000270a5345fc0] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
[out#1/rtp @ 00000270a7059e80] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.49    
~~~

### source1 完成
~~~text
frame= 3591 fps= 30 q=-1.0 size=  118561KiB time=00:01:59.70 bitrate=8114.0kbits/s speed=   1x elapsed=0:01:59.21    
[out#0/rtp @ 000002e24da5b9c0] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.47    
~~~

### source2 完成
~~~text
frame= 3592 fps= 30 q=-1.0 size=  118576KiB time=00:01:59.73 bitrate=8112.8kbits/s speed=   1x elapsed=0:01:59.24    
[out#0/rtp @ 0000012d8e9b4340] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.47    
~~~

### reentry 完成
~~~text
[out#0/rtp @ 0000022e941d7740] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
[out#1/rtp @ 0000022e96013940] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.49    
~~~

### VLC诊断（不隐藏错误）
~~~text
main error: buffer deadlock prevented
main error: Failed to create video converter
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r282-native-2026-10-21-10h53m32s676.png)
main error: Failed to create video converter
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r282-native-2026-10-21-10h55m41s256.png)
main error: Failed to create video converter
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r282-native-2026-10-21-10h56m24s031.png)
main error: Failed to create video converter
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r282-native-2026-10-21-10h58m05s392.png)
direct3d11 error: SetThumbNailClip failed: 0x800706f4
main error: read error: No error
~~~

### CLI stderr
~~~text
[in @ 000001ff80ed5dc0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001ff80ed5dc0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 19068.232656
[in @ 000001ff80ed5880] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001ff80ed5880] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 37937.905933
[in @ 000001ff8ae57480] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001ff8ae57480] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 34667.0334
[in @ 000001ff8adf77c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001ff8ae85340] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001ff8ae85340] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 37937.905933
[in @ 000001ff8adf77c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 19068.232656
[in @ 000001ff84ed1f40] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001ff84ed1f40] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 34667.0334
[aac @ 000001ff84fddd00] Qavg: 17100.893

~~~

### 监控首尾
~~~text
seconds=1.0166143 cpu=0.209586056204582 ws=5373952 private=17932288
seconds=317.8634826 cpu=0.973130408069361 ws=261820416 private=1131294720
~~~

### 本轮清理清单

- composition-r282-cli-error.log
- composition-r282-cli.log
- composition-r282-monitor.log
- composition-r282-native-2026-10-21-10h53m32s676.png
- composition-r282-native-2026-10-21-10h55m41s256.png
- composition-r282-native-2026-10-21-10h56m24s031.png
- composition-r282-native-2026-10-21-10h58m05s392.png
- composition-r282-output.sdp
- composition-r282-reentry-stdout.log
- composition-r282-reentry.log
- composition-r282-source0-stdout.log
- composition-r282-source0.log
- composition-r282-source1-stdout.log
- composition-r282-source1.log
- composition-r282-source2-stdout.log
- composition-r282-source2.log
- composition-r282-vlc.log

## 清理核验

VLC 在核对 PID=6680、命令行含 composition-r282 后，经 RC 127.0.0.1:63020 发送 `quit`。六个精确 PID 的 CIM 查询均为空。17个清单文件已逐项删除，composition-r282 前缀残留0，指定120秒源保留。归档解析初次因 Python 默认GBK读取中文文档失败，改为显式UTF-8后成功；失败解析未删除任何产物。
