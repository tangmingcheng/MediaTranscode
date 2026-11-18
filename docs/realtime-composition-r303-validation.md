# r303：存储释放通知接线复验（已执行）

原规格三源H264→HEVC 1280×720/30、8000kbps CBR/GOP60，AAC192kbps CBR/44100双声道，MPEG-TS/UDP至VLC。须在接线源码双审和Release全量重建完成后执行。此轮不证明全部TS分配已预算化，也不预先宣称崩溃或发送超期修复。

## 预定执行命令

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r303-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r303-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r303-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r303-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r303-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r303-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r303 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62020 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r303-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r303-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r303-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r303-native- --snapshot-format=png udp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r303-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

## 实际结果：本轮基础链路通过，整体门禁未完成
Release全量736项configure/build均0，10:48:49产物。源PID4708/34480/30928、CLI35156、VLC32484。三源120秒后均exit0，VLC原生快照确认三格→全黑。随后caller_stop，CLI exit0。无调试器。未执行重入、动态输出及存储压力路径，不证明旧崩溃或超期已修复；本轮压力失败计数0，不能证明新增通知实际触发。

## 实际调用方停止命令
~~~~powershell
$ErrorActionPreference='Stop'
$target=Get-CimInstance Win32_Process -Filter "ProcessId=35156"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r303*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R303Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R303Console]::FreeConsole() | Out-Null
if(-not [R303Console]::AttachConsole(35156)){throw 'AttachConsole failed'}
try {
[R303Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R303Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 35156 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R303Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R303Console]::FreeConsole() | Out-Null}

~~~~
## 最终指标及诊断
~~~~text
[2026-10-22 10:51:06.318] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4565 source_master_ns=111572478998 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4715984
[2026-10-22 10:51:07.340] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4609 source_master_ns=112594157003 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4761330
[2026-10-22 10:51:08.362] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4653 source_master_ns=113615835007 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4806676
[2026-10-22 10:51:09.383] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4697 source_master_ns=114637513012 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4852022
[2026-10-22 10:51:10.429] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4742 source_master_ns=115682410971 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4897368
[2026-10-22 10:51:11.450] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4786 source_master_ns=116704088975 raw_phase_ns=6802 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4942714
[2026-10-22 10:51:12.472] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4830 source_master_ns=117725766980 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4988060
[2026-10-22 10:51:13.517] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4875 source_master_ns=118770664939 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5033406
[2026-10-22 10:51:14.538] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4919 source_master_ns=119792342944 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5078752
[2026-10-22 10:51:40.212] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 datagrams=0 wire_bytes=0 immediate_deadlines=0 deferred_deadlines=0 planned_wait_ns=0 late_datagrams=0 maximum_lateness_ns=0 pending_bytes=0 peak_pending_bytes=110920 pressure_failures=0 access_units=10185 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=2782 maximum_mux_input_after_emit_ns=27639169 maximum_mux_input_after_dispatch_ns=-72360831 worst_mux_input_ready_ns=85259639300 worst_mux_input_emit_ns=85232000131 worst_mux_input_dispatch_ns=85332000131 maximum_protocol_batch_produced_after_release_ns=27666969 worst_protocol_batch_produced_ns=85259667100 worst_protocol_batch_release_ns=85232000131 worst_protocol_batch_deadline_ns=85332000131 worst_protocol_batch_datagrams=1 exit_reason=completed
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=907, threads=81, processThreads=128, workers=0, workerProgress=335181, workerProcessCalls=1271268, workerWaits=936070, workerWakeups=925348, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=441, logicalProcessors=22, averageSystemMachineCpuPercent=9.603949, averageProcessMachineCpuPercent=1.958941, peakProcessMachineCpuPercent=5.990783, averageProcessSingleCoreCpuPercent=43.096700, peakProcessSingleCoreCpuPercent=131.797235, initialWorkingSetBytes=208547840, workingSetBytes=245379072, peakWorkingSetBytes=270233600, totalPushed=306418, totalPopped=306394, droppedBuffers=0, encodedPacketsPushed=46749, encodedPacketsPopped=46749, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=17066546, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=737, graphPayloadReservations=143842, graphPayloadReleases=143842, graphPayloadPressureFailures=0, lastInputReceivedAtNs=7906718629900
~~~~
## 本轮清理清单
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r303-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r303-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r303-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r303-native-2026-10-22-10h49m44s892.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r303-native-2026-10-22-10h51m27s763.png
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r303-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r303-source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r303-source1-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r303-source1.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r303-source2-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r303-source2.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r303-vlc.log

清理：记录结果后停止本轮VLC；五个PID复查无残留，按清单删除12项临时文件。原120秒源保留，未运行远程测试。
