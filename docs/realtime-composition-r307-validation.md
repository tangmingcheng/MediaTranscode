# r307：正常启动后附加调试器（已执行，失败）

原规格三源H264→HEVC 1280×720/30、8000kbps CBR/GOP60、AAC192kbps CBR/44100双声道，MPEG-TS/UDP到VLC。先正常启动，再附加cdb捕获异常；不代表无调试器验收。

WER方案尝试：确认无应用专属LocalDumps key后，New-Item HKLM应用key被Windows拒绝（Requested registry access is not allowed），未创建key。唯一创建的空目录composition-r307-dumps已删除，未启动媒体。没有自动批准审查拒绝，属于OS权限限制。

## 实际媒体启动命令
~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r307-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r307-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r307-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r307-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r307-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r307-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r307 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62020 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r307-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r307-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r307-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r307-native- --snapshot-format=png udp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r307-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

## 实际结果：诊断扰动导致截止失败
源32328/35148/24724，CLI7776，VLC7972，cdb34024。cdb日志确认附加初始断点，之后CLI因sequence18592 submit超期退出1，cdb退出1；没有0xc0000409或dump。截止26671009694ns，观测27139610800ns，晚468601106ns；aggregate调用间隔最大566679400ns。该观测与附加暂停重叠，不能作为独立产品回归根因。
## 实际附加命令
~~~~powershell
$target=Get-CimInstance Win32_Process -Filter 'ProcessId=7776'
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notmatch 'composition-r307'){throw 'CLI identity mismatch'}
$debugger=Start-Process -FilePath 'C:/Program Files (x86)/Windows Kits/10/Debuggers/x64/cdb.exe' -ArgumentList '-G -p 7776 -logo D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r307-cdb.log -y D:/Code/MyCode/MediaTranscode/out/build/x64-release -c "sxe -c2 \".exr -1;.ecxr;~* kb;.dump /ma D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r307-crash.dmp;qd\" 0xc0000409;g"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r307-cdb-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r307-cdb-error.log' -PassThru
Write-Output "CDB_PID=$($debugger.Id)"
$debugger.WaitForExit()
Write-Output "CDB_EXIT=$($debugger.ExitCode)"
~~~~
## 关键证据
~~~~text
[CLI] runtime report: queued=116, peakQueued=394, threads=81, processThreads=131, workers=75, workerProgress=69463, workerProcessCalls=273094, workerWaits=203624, workerWakeups=201633, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=80, logicalProcessors=22, averageSystemMachineCpuPercent=7.679468, averageProcessMachineCpuPercent=1.750847, peakProcessMachineCpuPercent=4.426559, averageProcessSingleCoreCpuPercent=38.518629, peakProcessSingleCoreCpuPercent=97.384306, initialWorkingSetBytes=192442368, workingSetBytes=247865344, peakWorkingSetBytes=247865344, totalPushed=64226, totalPopped=64110, droppedBuffers=0, encodedPacketsPushed=8410, encodedPacketsPopped=8409, backpressureItems=123, graphPayloadCurrentBytes=804008, graphPayloadHighWaterBytes=4654350, graphPayloadCurrentObjects=186, graphPayloadHighWaterObjects=195, graphPayloadReservations=31085, graphPayloadReleases=30899, graphPayloadPressureFailures=0, lastInputReceivedAtNs=6937865660400
[2026-10-23 10:42:59.146] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1110 source_master_ns=26376695427 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1178996
[CLI] runtime report: queued=105, peakQueued=394, threads=81, processThreads=131, workers=75, workerProgress=70448, workerProcessCalls=277899, workerWaits=207441, workerWakeups=205427, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=81, logicalProcessors=22, averageSystemMachineCpuPercent=7.829939, averageProcessMachineCpuPercent=1.748309, peakProcessMachineCpuPercent=4.426559, averageProcessSingleCoreCpuPercent=38.462789, peakProcessSingleCoreCpuPercent=97.384306, initialWorkingSetBytes=192442368, workingSetBytes=247943168, peakWorkingSetBytes=247943168, totalPushed=65144, totalPopped=65039, droppedBuffers=0, encodedPacketsPushed=8520, encodedPacketsPopped=8519, backpressureItems=123, graphPayloadCurrentBytes=465894, graphPayloadHighWaterBytes=4654350, graphPayloadCurrentObjects=190, graphPayloadHighWaterObjects=195, graphPayloadReservations=31534, graphPayloadReleases=31344, graphPayloadPressureFailures=0, lastInputReceivedAtNs=6938218906900
[CLI] runtime report: queued=116, peakQueued=394, threads=81, processThreads=131, workers=75, workerProgress=71218, workerProcessCalls=280731, workerWaits=209504, workerWakeups=207466, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=82, logicalProcessors=22, averageSystemMachineCpuPercent=7.998958, averageProcessMachineCpuPercent=1.763183, peakProcessMachineCpuPercent=4.426559, averageProcessSingleCoreCpuPercent=38.790033, peakProcessSingleCoreCpuPercent=97.384306, initialWorkingSetBytes=192442368, workingSetBytes=247943168, peakWorkingSetBytes=247943168, totalPushed=65851, totalPopped=65735, droppedBuffers=0, encodedPacketsPushed=8629, encodedPacketsPopped=8629, backpressureItems=123, graphPayloadCurrentBytes=402499, graphPayloadHighWaterBytes=4654350, graphPayloadCurrentObjects=188, graphPayloadHighWaterObjects=195, graphPayloadReservations=31857, graphPayloadReleases=31669, graphPayloadPressureFailures=0, lastInputReceivedAtNs=6938529030000
[CLI] runtime report: queued=101, peakQueued=394, threads=81, processThreads=131, workers=75, workerProgress=72176, workerProcessCalls=284382, workerWaits=212199, workerWakeups=210134, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=83, logicalProcessors=22, averageSystemMachineCpuPercent=8.046842, averageProcessMachineCpuPercent=1.749954, peakProcessMachineCpuPercent=4.426559, averageProcessSingleCoreCpuPercent=38.498998, peakProcessSingleCoreCpuPercent=97.384306, initialWorkingSetBytes=192442368, workingSetBytes=247943168, peakWorkingSetBytes=247943168, totalPushed=66745, totalPopped=66644, droppedBuffers=0, encodedPacketsPushed=8736, encodedPacketsPopped=8735, backpressureItems=123, graphPayloadCurrentBytes=593114, graphPayloadHighWaterBytes=4654350, graphPayloadCurrentObjects=188, graphPayloadHighWaterObjects=195, graphPayloadReservations=32323, graphPayloadReleases=32135, graphPayloadPressureFailures=0, lastInputReceivedAtNs=6938851761300
[2026-10-23 10:43:00.561] [info] [graph][runtime.node] datagram_reservation_trace stage=submit_deadline_missed generation=1 endpoint=1 sequence=18592 release_master_ns=26571224734 deadline_master_ns=26671224734 activation_started_master_ns=26561323500 activation_completed_master_ns=26561376100 permit_started_master_ns=26572905700 permit_completed_master_ns=26572906000 snapshot_started_master_ns=26572906100 snapshot_sampled_master_ns=26572906200 snapshot_completed_master_ns=26572906500 reservation_completed_master_ns=26572908400 wait_started_master_ns=26572908900 wait_completed_master_ns=27139608900 submit_observed_master_ns=27139610800 original_error="IoFailure: scheduled datagram submit exceeded its original deadline generation=1 sequence=18592 endpoint=1 release_ns=26571224734 reserved_at_ns=26572906200 not_before_ns=26573115440 deadline_ns=26671009694 observed_at_ns=27139610800"
[2026-10-23 10:43:00.561] [info] [graph][runtime.node] worker.failed node=81 kind=ScheduledDatagramSender name=Common GCRA-paced nonblocking datagram sender error=IoFailure: scheduled datagram submit exceeded its original deadline generation=1 sequence=18592 endpoint=1 release_ns=26571224734 reserved_at_ns=26572906200 not_before_ns=26573115440 deadline_ns=26671009694 observed_at_ns=27139610800
[2026-10-23 10:43:00.575] [info] [graph][runtime.node] worker.secondary_failure node=79 kind=MpegTsDatagramMaterializer name=MPEG-TS wire protocol materializer error=InvalidArgument: MPEG-TS UDP prepared transaction exceeded its immutable emission deadline
[2026-10-23 10:43:00.574] [info] [graph][runtime.node] worker.secondary_failure node=81 kind=ScheduledDatagramSender name=Common GCRA-paced nonblocking datagram sender error=IoFailure: scheduled datagram submit exceeded its original deadline generation=1 sequence=18592 endpoint=1 release_ns=26571224734 reserved_at_ns=26572906200 not_before_ns=26573115440 deadline_ns=26671009694 observed_at_ns=27139610800
[CLI] runtime report: queued=133, peakQueued=408, threads=81, processThreads=60, workers=0, workerProgress=72404, workerProcessCalls=285302, workerWaits=212886, workerWakeups=210819, workerErrors=3, errors=3, stalledIntervals=0, cpuSamples=84, logicalProcessors=22, averageSystemMachineCpuPercent=8.162849, averageProcessMachineCpuPercent=1.744946, peakProcessMachineCpuPercent=4.426559, averageProcessSingleCoreCpuPercent=38.388812, peakProcessSingleCoreCpuPercent=97.384306, initialWorkingSetBytes=192442368, workingSetBytes=248430592, peakWorkingSetBytes=248430592, totalPushed=66947, totalPopped=66814, droppedBuffers=0, encodedPacketsPushed=8753, encodedPacketsPopped=8753, backpressureItems=123, graphPayloadCurrentBytes=393038, graphPayloadHighWaterBytes=4654350, graphPayloadCurrentObjects=209, graphPayloadHighWaterObjects=214, graphPayloadReservations=32413, graphPayloadReleases=32204, graphPayloadPressureFailures=1, lastInputReceivedAtNs=6939479740900
[2026-10-23 10:43:00.742] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=failed detail="scheduled datagram submit exceeded its original deadline generation=1 sequence=18592 endpoint=1 release_ns=26571224734 reserved_at_ns=26572906200 not_before_ns=26573115440 deadline_ns=26671009694 observed_at_ns=27139610800" description="" error=6 native=0 stage=3 reason="scheduled datagram submit exceeded its original deadline generation=1 sequence=18592 endpoint=1 release_ns=26571224734 reserved_at_ns=26572906200 not_before_ns=26573115440 deadline_ns=26671009694 observed_at_ns=27139610800"
[CLI] final runtime report: queued=0, peakQueued=408, threads=81, processThreads=60, workers=0, workerProgress=72404, workerProcessCalls=285302, workerWaits=212886, workerWakeups=210819, workerErrors=3, errors=3, stalledIntervals=0, cpuSamples=84, logicalProcessors=22, averageSystemMachineCpuPercent=8.162849, averageProcessMachineCpuPercent=1.744946, peakProcessMachineCpuPercent=4.426559, averageProcessSingleCoreCpuPercent=38.388812, peakProcessSingleCoreCpuPercent=97.384306, initialWorkingSetBytes=192442368, workingSetBytes=248430592, peakWorkingSetBytes=248430592, totalPushed=66947, totalPopped=66814, droppedBuffers=0, encodedPacketsPushed=8753, encodedPacketsPopped=8753, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=4654350, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=214, graphPayloadReservations=32413, graphPayloadReleases=32413, graphPayloadPressureFailures=1, lastInputReceivedAtNs=6939479740900
~~~~
## 清理清单
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r307-cdb-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r307-cdb-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r307-cdb.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r307-cli-error.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r307-cli.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r307-monitor.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r307-source0-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r307-source0.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r307-source1-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r307-source1.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r307-source2-stdout.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r307-source2.log
D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r307-vlc.log

原会话确认CLI1、源均-1（失败后清理）；六PID无残留。按清单删除13项临时产物；原源保留，无系统注册表配置残留。
