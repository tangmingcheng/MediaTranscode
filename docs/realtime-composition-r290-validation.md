# r290：统一预算后 MPEG-TS/UDP 复验

整体门禁未通过。三源10436/29708/27084及重入14972均完整120秒退出0，CLI31476 caller_stop=0，VLC28256。四张VLC原生快照已检查：三格、全黑、左下重入、全黑。未运行local CLI。

## 实际命令
~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r290-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r290-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r290-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r290-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r290-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r290-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r290 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62020 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r290-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r290-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r290-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r290-native- --snapshot-format=png udp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r290-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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



$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r290-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r290-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($source0.Id)"
$source0.WaitForExit()
Write-Output "REENTRY_EXIT=$($source0.ExitCode)"
exit $source0.ExitCode

$ErrorActionPreference='Stop'
$target=Get-CimInstance Win32_Process -Filter "ProcessId=31476"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r290*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R290Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R290Console]::FreeConsole() | Out-Null
if(-not [R290Console]::AttachConsole(31476)){throw 'AttachConsole failed'}
try {
[R290Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R290Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 31476 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R290Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R290Console]::FreeConsole() | Out-Null}



$p=Get-CimInstance Win32_Process -Filter 'ProcessId=28256'; if($p){if($p.Name -ne 'vlc.exe' -or $p.CommandLine -notlike '*composition-r290*'){throw 'VLC identity mismatch'}; $client=[Net.Sockets.TcpClient]::new('127.0.0.1',63020); try{$stream=$client.GetStream(); $bytes=[Text.Encoding]::ASCII.GetBytes("quit`n"); $stream.Write($bytes,0,$bytes.Length)}finally{$client.Dispose()}}
~~~~
## 指标和错误证据
~~~~text
[2026-10-21 15:38:50.034] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=35560 committed_datagrams=178584 committed_payload_bytes=204235492 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=79296 pacing_reserved=178584 pacing_cancelled=0 pacing_submitted=178584 pacing_maximum_submit_lateness_ns=27924935 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=79296 backlog_max_residence_ns=77250500 last_materialized_sequence=178584 last_scheduled_sequence=178584 last_submitted_sequence=178584 last_committed_sequence=178584 shared_scope_charged_datagrams=178584 shared_scope_charged_wire_bytes=209235844 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2154 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2154 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=188010 endpoint_1_target_effective_socket_bytes=188010 endpoint_1_api_requested_socket_bytes=188010 endpoint_1_effective_socket_bytes=188010 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=178584 endpoint_1_committed_payload_bytes=204235492
[2026-10-21 15:38:50.254] [info] [graph][runtime.node] mpegts_wire_materializer stage=stopped maximum_materialized_after_release_ns=64663860 worst_materialized_at_ns=209174296500 worst_release_ns=209109632640 worst_deadline_ns=209176299308 worst_global_sequence=103801
[2026-10-21 15:38:50.312] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=10361 completion_waits=10361 completed_datagrams=10462 drained_datagrams=146 would_block=46 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-21 15:38:50.312] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=138738 completion_waits=138738 completed_datagrams=212075 drained_datagrams=73383 would_block=46 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-21 15:38:50.313] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=74748 completion_waits=74748 completed_datagrams=105395 drained_datagrams=30755 would_block=107 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-21 15:38:50.313] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=73219 completion_waits=73219 completed_datagrams=105399 drained_datagrams=32286 would_block=107 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=910, threads=81, processThreads=130, workers=0, workerProgress=588286, workerProcessCalls=2204406, workerWaits=1616103, workerWakeups=1590946, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=1044, logicalProcessors=22, averageSystemMachineCpuPercent=7.337850, averageProcessMachineCpuPercent=1.538758, peakProcessMachineCpuPercent=6.250000, averageProcessSingleCoreCpuPercent=33.852676, peakProcessSingleCoreCpuPercent=137.500000, initialWorkingSetBytes=208457728, workingSetBytes=260476928, peakWorkingSetBytes=267513856, totalPushed=533331, totalPopped=533295, droppedBuffers=0, encodedPacketsPushed=110317, encodedPacketsPopped=110317, backpressureItems=122, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=16688154, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=733, graphPayloadReservations=263894, graphPayloadReleases=263894, graphPayloadPressureFailures=0, lastInputReceivedAtNs=24886713146000
[in @ 00000274aff08100] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000274aff08100] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 40410.894622
[in @ 00000274bab8f2c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000274bab8f2c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 26021.485422
[in @ 00000274baff6e00] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000274baff6e00] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 3189.294111
[in @ 00000274a10adf00] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000274a10adf00] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 26021.485422
[in @ 00000274b55d0600] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000274b55d0600] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 3189.294111
[in @ 00000274ba862900] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000274ba862900] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 40410.894622
[aac @ 00000274b59c3a00] Qavg: 19675.059
faad warning: decoded zero sample
d3d11va warning: surface dimensions (1280x768) differ from avcodec dimensions (1280x736)
d3d11va warning: not enough decoding slices in the texture (6/28)
main error: buffer deadlock prevented
main warning: playback too late (144881): up-sampling
main warning: playback way too late (223570): flushing buffers
main warning: picture is too late to be displayed (missing 37 ms)
main error: Failed to create video converter
main error: Failed to create video converter
main error: Failed to create video converter
main error: Failed to create video converter
main warning: buffer too late (-12056801 us): dropped
main warning: picture is too late to be displayed (missing 12390 ms)
main warning: picture is too late to be displayed (missing 12358 ms)
main warning: picture is too late to be displayed (missing 12325 ms)
main warning: picture is too late to be displayed (missing 12291 ms)
main warning: picture is too late to be displayed (missing 12258 ms)
main error: read error: No error
direct3d11 error: SetThumbNailClip failed: 0x800706f4
~~~~
## 限制及清理
未证明global WouldBlock分支覆盖、发送交付、接收端持续A/V同步。归档后逐项清理以下产物，保留复用源：
composition-r290-cli-error.log
composition-r290-cli.log
composition-r290-monitor.log
composition-r290-native-2026-10-21-15h33m47s612.png
composition-r290-native-2026-10-21-15h36m09s387.png
composition-r290-native-2026-10-21-15h37m02s631.png
composition-r290-native-2026-10-21-15h38m42s456.png
composition-r290-reentry-stdout.log
composition-r290-reentry.log
composition-r290-source0-stdout.log
composition-r290-source0.log
composition-r290-source1-stdout.log
composition-r290-source1.log
composition-r290-source2-stdout.log
composition-r290-source2.log
composition-r290-vlc.log
