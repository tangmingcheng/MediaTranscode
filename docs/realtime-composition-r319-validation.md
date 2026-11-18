# r319：维护释放与完成期限分离后原规格复验（画面与生命周期证据齐备，完整门禁未通过）

三路120秒 H264/AAC RTP→HEVC CBR 8000kbps 1280×720/30、AAC CBR 192kbps 44100Hz stereo→MPEG-TS/UDP 62024。仅 realtime，VLC 原生快照。以下命令已实际执行，结果见末尾；不代表完整验收通过。

## 启动命令

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r319-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r319-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r319-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r319-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r319-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r319-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r319 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r319-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r319-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r319-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r319-native- --snapshot-format=png udp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r319-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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
~~~~

## AV 源重入命令

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r319-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r319-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($source0.Id)"
$source0.WaitForExit()
Write-Output "REENTRY_EXIT=$($source0.ExitCode)"
exit $source0.ExitCode
~~~~

## VLC 原生快照与调用方停止

快照命令执行四次，分别对应三格、全黑、AV重入左下格、再次全黑。VLC退出使用同一RC连接发送quit。

~~~~powershell
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 40576){throw 'VLC RC PID mismatch'}; $c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020); $s=$c.GetStream(); $b=[Text.Encoding]::ASCII.GetBytes("snapshot`n"); $s.Write($b,0,$b.Length); $s.Dispose(); $c.Dispose()
~~~~

~~~~powershell
$target=Get-CimInstance Win32_Process -Filter "ProcessId=28256"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r319*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R319Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R319Console]::FreeConsole() | Out-Null
if(-not [R319Console]::AttachConsole(28256)){throw 'AttachConsole failed'}
try {
[R319Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R319Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 28256 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R319Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R319Console]::FreeConsole() | Out-Null}

~~~~

## 结果与验收边界

Release全量745项重建成功，configure/build exit0，四项必需产物核验；CLI产物2026-10-23 14:50:20。两名独立审查者对最终六文件局部静态筛查均PASS。

源PID38536/41876/43720、重入36940，四者均完成120秒并退出0；CLI28256约337秒后接受调用方Ctrl+C退出0，监控会话78233，重入会话60104。VLC40576使用RC退出。源EOF后连续黑屏输出符合本场景预期，CLI退出不是源EOF自动退出。

四张VLC原生快照实际查看：14:51:23三格视频+右下空黑格；14:53:14全黑；14:54:23仅左下AV源恢复；14:55:55再次全黑。未使用系统截图。

sender实际提交179159个datagram、204628412 payload bytes，deadline_misses=0，pressure_failures=0，partial/ambiguous failures=0；最大提交迟到38910340ns仍在有效期限内。delivery_evidence=not_proven，不能将socket提交冒充NIC出线证明。mux旧汇总datagrams/wire_bytes仍为0而access_units=24192，与sender不一致；该诊断缺口待修，不能据零值声称无输出。

最终workerErrors/errors=0，graphPayloadCurrentBytes/Objects=0，reservations/releases均264736，高水位17877197 bytes。内部平均进程机器CPU1.774164%、峰值9.111617%；外部327次采样平均CPU1.760486%、峰值4.712504%，WS峰值273367040 bytes、private峰值1154928640 bytes。初始三源稳态WS约271MB、全失源约247MB、重入约263MB；本轮有限时长不能证明长期无泄漏。

内部phase：generation1约11339ns，generation2约15873ns，最后generation2 sequence5096；未观察frequency correction。它不是内容级A/V偏差，也没有真实声音/静音验收证据。

本轮确认三源画面、失源黑屏、AV源视频重入及调用方正常停止；r318提交期限错误未复现。尚缺内容级声音/静音与持续A/V、出线时序、完整矩阵、周期/debt规划闭环，故不记完整验收PASS、不创建成功测试提交。

## 诊断摘录与清理清单

以下记录先于逐项清理；120秒复用源保留。本轮未启动远程测试。

~~~~text

[2026-10-23 14:56:20.190] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 datagrams=0 wire_bytes=
0 immediate_deadlines=0 deferred_deadlines=0 planned_wait_ns=0 late_datagrams=0 maximum_lateness_ns=0 pending_bytes=0 p
eak_pending_bytes=110920 pressure_failures=0 access_units=24192 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns
=56734693 maximum_mux_input_after_emit_ns=66269157 maximum_mux_input_after_dispatch_ns=-33730843 worst_mux_input_ready_
ns=26827681200 worst_mux_input_emit_ns=26761412043 worst_mux_input_dispatch_ns=26861412043 maximum_protocol_batch_produ
ced_after_release_ns=56255776 worst_protocol_batch_produced_ns=26844062800 worst_protocol_batch_release_ns=26787807024 
worst_protocol_batch_deadline_ns=26887806601 worst_protocol_batch_datagrams=1 exit_reason=completed
[2026-10-23 14:56:20.191] [info] [graph][runtime.node] datagram_batch_trace stage=last_completed generation=1 first_seq
uence=179159 last_sequence=179159 maximum_submit_interval_ns=50600 maximum_submit_interval_sequence=179159 maximum_star
t_lateness_ns=345854 maximum_start_lateness_sequence=179159 first_release_master_ns=336724586646 first_deadline_master_
ns=336824586646 last_release_master_ns=336724586646 last_deadline_master_ns=336824586646 first_submitted_master_ns=3367
24983100 last_submitted_master_ns=336724983100 worst_wait_started_master_ns=336721875700 worst_wait_completed_master_ns
=336724931600 worst_submit_observed_master_ns=336724932500
[2026-10-23 14:56:20.191] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_sc
ope=ifindex:1 execution_mode=1 committed_batches=179159 committed_datagrams=179159 committed_payload_bytes=204628412 wo
uld_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_fai
lures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=1 ordered_queue_maximum_wire_bytes=1344 pacing_
reserved=179159 pacing_cancelled=0 pacing_submitted=179159 pacing_maximum_submit_lateness_ns=38910340 pacing_rate_adapt
ations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_prov
en backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=
0 backlog_high_water_datagrams=1 backlog_high_water_wire_bytes=1344 backlog_max_residence_ns=27175700 last_materialized
_sequence=179159 last_scheduled_sequence=179159 last_submitted_sequence=179159 last_committed_sequence=179159 shared_sc
ope_charged_datagrams=179159 shared_scope_charged_wire_bytes=209644864 shared_scope_waiting_members=0 shared_scope_fail
ed=0 evidence_submitted=2154 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2154 evidence_observed=0 evidenc
e_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_soc
ket_bytes=188010 endpoint_1_target_effective_socket_bytes=188010 endpoint_1_api_requested_socket_bytes=188010 endpoint_
1_effective_socket_bytes=188010 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=179159 endpoint_1_committe
d_payload_bytes=204628412
[CLI] final runtime report: queued=0, peakQueued=952, threads=81, processThreads=130, workers=0, workerProgress=1044425
, workerProcessCalls=3373821, workerWaits=2329379, workerWakeups=2304217, workerErrors=0, errors=0, stalledIntervals=0,
 cpuSamples=1053, logicalProcessors=22, averageSystemMachineCpuPercent=15.020237, averageProcessMachineCpuPercent=1.774
164, peakProcessMachineCpuPercent=9.111617, averageProcessSingleCoreCpuPercent=39.031613, peakProcessSingleCoreCpuPerce
nt=200.455581, initialWorkingSetBytes=213123072, workingSetBytes=263303168, peakWorkingSetBytes=273301504, totalPushed=
821002, totalPopped=820967, droppedBuffers=0, encodedPacketsPushed=111033, encodedPacketsPopped=111033, backpressureIte
ms=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=17877197, graphPayloadCurrentObjects=0, graphPayloadHigh
WaterObjects=760, graphPayloadReservations=264736, graphPayloadReleases=264736, graphPayloadPressureFailures=0, lastInp
utReceivedAtNs=22101412099800

Count             : 327
Average           : 1.76048608601032
Sum               : 
Maximum           : 4.71250393118444
Minimum           : 0
StandardDeviation : 
Property          : cpu


Count             : 327
Average           : 260641669.87156
Sum               : 
Maximum           : 273367040
Minimum           : 144601088
StandardDeviation : 
Property          : ws


Count             : 327
Average           : 1136451959.77982
Sum               : 
Maximum           : 1154928640
Minimum           : 568512512
StandardDeviation : 
Property          : private


seconds : 337.0711934
cpu     : 1.24878686738518
ws      : 177307648
private : 649568256

[2026-10-23 14:50:49.052] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1 source_master_ns=57569779
51 raw_phase_ns=11339 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample
_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-10-23 14:55:41.810] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=5096 source_master_ns=29899
2721486 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 s
ample_delta=0 compensation_distance=45346 effective_output_sample_index=5260136
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r319-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r319-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r319-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r319-native-2026-10-23-14h51m23s898.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r319-native-2026-10-23-14h53m14s301.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r319-native-2026-10-23-14h54m23s043.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r319-native-2026-10-23-14h55m55s401.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r319-reentry-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r319-reentry.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r319-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r319-source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r319-source1-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r319-source1.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r319-source2-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r319-source2.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r319-vlc.log


~~~~

清理核验：按上述16项文件清单逐项删除，r319前缀残留0；提升权限核验六个精确PID剩余0。120秒源保留（124427809 bytes）。未修改任何外部依赖，未提交、推送或部署。
