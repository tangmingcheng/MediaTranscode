# r357：启动快照及IDs/selections定容后的TS/UDP回归

状态：运行结束；三格→全黑→选中源重入→再次全黑四张VLC原生快照均实际查看，第四张15:33:18。重入interval miss待定位，不判完整验收通过。

三路初始源及重入源均3600帧/120秒自然exit0；CLI caller_stop exit0、主会话36742 exit0、重入12093 exit0，VLC RC quit，六个精确PID查询无残留。

payload预约=释放=223823，最终字节/对象0，workerErrors/errors/stalledIntervals均0。sender174297 datagrams、203599676 payload bytes全部提交，deadline/pressure/partial/ambiguous failures为0；最大提交迟到10341980ns、backlog residence53644800ns，delivery_evidence=not_proven。

video_commits8628；源0/1各real3415+black5213，源2 real6969+black1659。audio12386次，real10244096+silence2439168 samples；源generation2、output_generation1，未覆盖输出代际purge。

source2 first_interval_miss cursor4277、target148341059834ns、steady24333189051700ns、generation2、capacity7、before/after/expired/channel_depth均0，examined range不可用。原因尚未定位；画面恢复不能证明重入连续无间隙。

内部CPU均值2.252160%、峰值7.489879%、WS峰值271015936；外部1秒采样284项CPU均值2.245242%、峰值5.011381%，WS峰值271810560、private峰值1279725568、末段private1273049088，采样口径不同。phase11338ns仅内部数据，不替代内容同步。保留FFmpeg range警告、AAC Qavg、VLC迟到与SetThumbNailClip错误。

完整预算、内容AV/静音、NIC交付、长期内存、非cadence/四源、直接C API及Windows后RKMPP仍未通过。

## 实际停止命令

~~~~powershell
$target=Get-CimInstance Win32_Process -Filter "ProcessId=23828"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r357*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R357Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R357Console]::FreeConsole() | Out-Null
if(-not [R357Console]::AttachConsole(23828)){throw 'AttachConsole failed'}
try {
[R357Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R357Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 23828 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R357Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R357Console]::FreeConsole() | Out-Null}
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 32168){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020);$s=$c.GetStream();$b=[Text.Encoding]::ASCII.GetBytes("quit`n");$s.Write($b,0,$b.Length);$c.Dispose()
~~~~

Release全量重建85439 exit0，configure/build0。realtime产物6527488字节、2026-10-26 15:28:16，全部四项构建产物已核对。本轮仅运行realtime。源H264/AAC指定120秒连续文件，输出HEVC CBR8000kbps 1280×720/30 GOP60、AAC CBR192kbps 44100Hz双声道、MPEGTS/UDP。

初始源30684/40228/44928，CLI23828，VLC32168，重入27088；主会话36742、重入12093。VLC原生快照15:29:16三格、15:30:55全黑、15:31:36左下重入均实际查看；未用系统截图。generation2 phase11338ns仅内部遥测。

## 实际命令

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r357-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r357-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r357-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r357-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r357-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r357-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r357 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r357-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r357-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r357-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r357-native- --snapshot-format=png udp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r357-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r357-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r357-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($source0.Id)"
$source0.WaitForExit()
Write-Output "REENTRY_EXIT=$($source0.ExitCode)"
exit $source0.ExitCode
~~~~

原生快照命令：

~~~~powershell
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 32168){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
$s=$c.GetStream()
$b=[Text.Encoding]::ASCII.GetBytes("snapshot`n")
$s.Write($b,0,$b.Length)
$s.Flush()
$c.Dispose(); Start-Sleep -Seconds 2; Get-ChildItem 'D:/Code/MyCode/MediaTranscode/out/acceptance' -Filter 'composition-r357-native-*.png' | Select-Object FullName,Length
~~~~


## 关键原始日志及清理清单

~~~~text
[2026-10-26 15:30:39.534] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=3 generation=2 invalidated_generation=1 locked=0
[2026-10-26 15:30:39.534] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=3 generation=2 invalidated_generation=1 locked=0
[2026-10-26 15:30:39.548] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=3 generation=2 invalidated_generation=1 locked=0
[2026-10-26 15:30:39.550] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=3 generation=2 invalidated_generation=1 locked=0
[2026-10-26 15:30:39.553] [info] [graph][runtime.node] audio_correction_stale node=73 command_generation=1 executor_generation=2 sequence=4919
[2026-10-26 15:31:07.233] [info] [graph][runtime.node] rtp_binder_boundary stage=deadline_established node=51 group=composition-r357.source.2 buffered=0 phase=2 preserves_output=1 raw_ssrc=1144715015 raw_generation=1 raw_sequence=5171 old_generation=1 next_generation=2 transition_sequence=1 published_generation=1
[2026-10-26 15:31:07.233] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=0 generation=2 invalidated_generation=1 locked=0
[2026-10-26 15:31:07.234] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:31:07.773] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=1 generation=2
[2026-10-26 15:31:08.271] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1 source_master_ns=148272384452 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-10-26 15:31:08.271] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3 source_master_ns=148318824361 raw_phase_ns=11337 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=45346
[2026-10-26 15:31:08.816] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=48 source_master_ns=149363722321 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=90692
[2026-10-26 15:31:09.832] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=92 source_master_ns=150385400325 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=136038
[2026-10-26 15:31:10.854] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=136 source_master_ns=151407078330 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=181384
[2026-10-26 15:31:11.899] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=181 source_master_ns=152451976289 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=226730
[2026-10-26 15:31:12.236] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:31:12.238] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:31:12.922] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=225 source_master_ns=153473654293 raw_phase_ns=11337 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=272076
[2026-10-26 15:31:13.942] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=269 source_master_ns=154495332298 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=317422
[2026-10-26 15:31:14.964] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=313 source_master_ns=155517010302 raw_phase_ns=11337 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=362768
[2026-10-26 15:31:16.011] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=358 source_master_ns=156561908262 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=408114
[2026-10-26 15:31:17.030] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=402 source_master_ns=157583586266 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=453460
[2026-10-26 15:31:17.264] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:31:17.279] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:31:18.052] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=446 source_master_ns=158605264271 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=498806
[2026-10-26 15:31:19.099] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=491 source_master_ns=159650162230 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=544152
[2026-10-26 15:31:20.119] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=535 source_master_ns=160671840234 raw_phase_ns=11337 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=589498
[2026-10-26 15:31:21.141] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=579 source_master_ns=161693518239 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=634844
[2026-10-26 15:31:22.163] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=623 source_master_ns=162715196243 raw_phase_ns=11337 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=680190
[2026-10-26 15:31:22.278] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:31:22.307] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:31:23.207] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=668 source_master_ns=163760094203 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=725536
[2026-10-26 15:31:24.229] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=712 source_master_ns=164781772207 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=770882
[2026-10-26 15:31:25.250] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=756 source_master_ns=165803450212 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=816228
[2026-10-26 15:31:26.307] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=801 source_master_ns=166848348171 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=861574
[2026-10-26 15:31:27.291] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:31:27.317] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=845 source_master_ns=167870026175 raw_phase_ns=11337 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=906920
[2026-10-26 15:31:27.346] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:31:28.349] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=889 source_master_ns=168891704180 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=952266
[2026-10-26 15:31:29.360] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=933 source_master_ns=169913382185 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=997612
[2026-10-26 15:31:30.405] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=978 source_master_ns=170958280144 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1042958
[2026-10-26 15:31:31.427] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1022 source_master_ns=171979958148 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1088304
[2026-10-26 15:31:32.298] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:31:32.374] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:31:32.448] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1066 source_master_ns=173001636153 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1133650
[2026-10-26 15:31:33.493] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1111 source_master_ns=174046534112 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1178996
[2026-10-26 15:31:34.515] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1155 source_master_ns=175068212117 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1224342
[2026-10-26 15:31:35.537] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1199 source_master_ns=176089890121 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1269688
[2026-10-26 15:31:36.558] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1243 source_master_ns=177111568126 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1315034
[2026-10-26 15:31:37.322] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:31:37.377] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:31:37.603] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1288 source_master_ns=178156466085 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1360380
[2026-10-26 15:31:38.625] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1332 source_master_ns=179178144089 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1405726
[2026-10-26 15:31:39.646] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1376 source_master_ns=180199822094 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1451072
[2026-10-26 15:31:40.692] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1421 source_master_ns=181244720053 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1496418
[2026-10-26 15:31:41.713] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1465 source_master_ns=182266398058 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1541764
[2026-10-26 15:31:42.342] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:31:42.384] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:31:42.735] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1509 source_master_ns=183288076062 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1587110
[2026-10-26 15:31:43.757] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1553 source_master_ns=184309754067 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1632456
[2026-10-26 15:31:44.802] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1598 source_master_ns=185354652026 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1677802
[2026-10-26 15:31:45.823] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1642 source_master_ns=186376330030 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1723148
[2026-10-26 15:31:46.845] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1686 source_master_ns=187398008035 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1768494
[2026-10-26 15:31:47.350] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:31:47.407] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:31:47.890] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1731 source_master_ns=188442905994 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1813840
[2026-10-26 15:31:48.911] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1775 source_master_ns=189464583999 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1859186
[2026-10-26 15:31:49.934] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1819 source_master_ns=190486262003 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1904532
[2026-10-26 15:31:50.956] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1863 source_master_ns=191507940008 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1949878
[2026-10-26 15:31:52.000] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1908 source_master_ns=192552837967 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1995224
[2026-10-26 15:31:52.370] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:31:52.443] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:31:53.022] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1952 source_master_ns=193574515971 raw_phase_ns=11337 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2040570
[2026-10-26 15:31:54.043] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1996 source_master_ns=194596193976 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2085916
[2026-10-26 15:31:55.105] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2041 source_master_ns=195641091935 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2131262
[2026-10-26 15:31:56.110] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2085 source_master_ns=196662769940 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2176608
[2026-10-26 15:31:57.132] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2129 source_master_ns=197684447944 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2221954
[2026-10-26 15:31:57.385] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:31:57.446] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:31:58.155] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2173 source_master_ns=198706125949 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2267300
[2026-10-26 15:31:59.212] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2218 source_master_ns=199751023908 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2312646
[2026-10-26 15:32:00.220] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2262 source_master_ns=200772701912 raw_phase_ns=11337 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2357992
[2026-10-26 15:32:01.241] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2306 source_master_ns=201794379917 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2403338
[2026-10-26 15:32:02.263] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2350 source_master_ns=202816057921 raw_phase_ns=11337 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2448684
[2026-10-26 15:32:02.397] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:32:02.480] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:32:03.321] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2395 source_master_ns=203860955881 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2494030
[2026-10-26 15:32:04.329] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2439 source_master_ns=204882633885 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2539376
[2026-10-26 15:32:05.351] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2483 source_master_ns=205904311890 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2584722
[2026-10-26 15:32:06.396] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2528 source_master_ns=206949209849 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2630068
[2026-10-26 15:32:07.412] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:32:07.417] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2572 source_master_ns=207970887853 raw_phase_ns=11337 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2675414
[2026-10-26 15:32:07.512] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:32:08.440] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2616 source_master_ns=208992565858 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2720760
[2026-10-26 15:32:09.461] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2660 source_master_ns=210014243863 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2766106
[2026-10-26 15:32:10.506] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2705 source_master_ns=211059141822 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2811452
[2026-10-26 15:32:11.528] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2749 source_master_ns=212080819826 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2856798
[2026-10-26 15:32:12.431] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:32:12.536] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:32:12.550] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2793 source_master_ns=213102497831 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2902144
[2026-10-26 15:32:13.596] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2838 source_master_ns=214147395790 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2947490
[2026-10-26 15:32:14.618] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2882 source_master_ns=215169073795 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2992836
[2026-10-26 15:32:15.638] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2926 source_master_ns=216190751799 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3038182
[2026-10-26 15:32:16.659] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2970 source_master_ns=217212429804 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3083528
[2026-10-26 15:32:17.439] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:32:17.544] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:32:17.704] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3015 source_master_ns=218257327763 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3128874
[2026-10-26 15:32:18.726] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3059 source_master_ns=219279005767 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3174220
[2026-10-26 15:32:19.748] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3103 source_master_ns=220300683772 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3219566
[2026-10-26 15:32:20.794] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3148 source_master_ns=221345581731 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3264912
[2026-10-26 15:32:21.815] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3192 source_master_ns=222367259736 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3310258
[2026-10-26 15:32:22.447] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:32:22.579] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:32:22.836] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3236 source_master_ns=223388937740 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3355604
[2026-10-26 15:32:23.857] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3280 source_master_ns=224410615745 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3400950
[2026-10-26 15:32:24.903] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3325 source_master_ns=225455513704 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3446296
[2026-10-26 15:32:25.924] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3369 source_master_ns=226477191708 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3491642
[2026-10-26 15:32:26.946] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3413 source_master_ns=227498869713 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3536988
[2026-10-26 15:32:27.464] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:32:27.614] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:32:27.990] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3458 source_master_ns=228543767672 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3582334
[2026-10-26 15:32:29.013] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3502 source_master_ns=229565445677 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3627680
[2026-10-26 15:32:30.034] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3546 source_master_ns=230587123681 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3673026
[2026-10-26 15:32:31.056] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3590 source_master_ns=231608801686 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3718372
[2026-10-26 15:32:32.101] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3635 source_master_ns=232653699645 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3763718
[2026-10-26 15:32:32.467] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:32:32.640] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:32:33.122] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3679 source_master_ns=233675377649 raw_phase_ns=11337 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3809064
[2026-10-26 15:32:34.144] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3723 source_master_ns=234697055654 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3854410
[2026-10-26 15:32:35.189] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3768 source_master_ns=235741953613 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3899756
[2026-10-26 15:32:36.210] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3812 source_master_ns=236763631618 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3945102
[2026-10-26 15:32:37.232] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3856 source_master_ns=237785309622 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3990448
[2026-10-26 15:32:37.478] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:32:37.642] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:32:38.254] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3900 source_master_ns=238806987627 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4035794
[2026-10-26 15:32:39.300] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3945 source_master_ns=239851885586 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4081140
[2026-10-26 15:32:40.320] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3989 source_master_ns=240873563590 raw_phase_ns=11337 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4126486
[2026-10-26 15:32:41.342] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4033 source_master_ns=241895241595 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4171832
[2026-10-26 15:32:42.389] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4078 source_master_ns=242940139554 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4217178
[2026-10-26 15:32:42.506] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:32:42.667] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:32:43.409] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4122 source_master_ns=243961817559 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4262524
[2026-10-26 15:32:44.431] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4166 source_master_ns=244983495563 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4307870
[2026-10-26 15:32:45.452] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4210 source_master_ns=246005173568 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4353216
[2026-10-26 15:32:46.497] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4255 source_master_ns=247050071527 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4398562
[CLI] [2026-10-26 15:32:47.519] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4299 source_master_ns=248071749531 raw_phase_ns=11337 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4443908
[2026-10-26 15:32:47.520] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:32:47.705] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:32:48.544] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4343 source_master_ns=249093427536 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4489254
[2026-10-26 15:32:49.585] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4388 source_master_ns=250138325495 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4534600
[2026-10-26 15:32:50.607] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4432 source_master_ns=251160003500 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4579946
[2026-10-26 15:32:51.629] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4476 source_master_ns=252181681504 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4625292
[2026-10-26 15:32:52.525] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:32:52.650] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4520 source_master_ns=253203359509 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4670638
[2026-10-26 15:32:52.733] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:32:53.695] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4565 source_master_ns=254248257468 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4715984
[2026-10-26 15:32:54.717] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4609 source_master_ns=255269935473 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4761330
[2026-10-26 15:32:55.738] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4653 source_master_ns=256291613477 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4806676
[2026-10-26 15:32:56.760] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4697 source_master_ns=257313291482 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4852022
[2026-10-26 15:32:57.541] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:32:57.742] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:32:57.805] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4742 source_master_ns=258358189441 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4897368
[2026-10-26 15:32:58.826] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4786 source_master_ns=259379867445 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4942714
[2026-10-26 15:32:59.848] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4830 source_master_ns=260401545450 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4988060
[2026-10-26 15:33:00.894] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4875 source_master_ns=261446443409 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5033406
[2026-10-26 15:33:01.915] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4919 source_master_ns=262468121414 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5078752
[2026-10-26 15:33:02.564] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:33:02.769] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-26 15:33:02.937] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4963 source_master_ns=263489799418 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5124098
[2026-10-26 15:33:03.964] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=5007 source_master_ns=264511477423 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5169444
[2026-10-26 15:33:05.004] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=5052 source_master_ns=265556375382 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5214790
[2026-10-26 15:33:06.027] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=5096 source_master_ns=266578053386 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5260136
[2026-10-26 15:33:06.712] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=2 reason=1 observed_at_ns=24451628679100
[2026-10-26 15:33:06.713] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=3 generation=3 invalidated_generation=2 locked=0
[2026-10-26 15:33:06.713] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=50 group=composition-r357.source.2 buffered=0 phase=0 preserves_output=1 protocol_generation=2 evidence_revision=94 video_raw_generation=1 video_ssrc=1958445999 audio_raw_generation=1 audio_ssrc=1144715015
[2026-10-26 15:33:06.713] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=51 group=composition-r357.source.2 buffered=0 phase=0 preserves_output=1 protocol_generation=2 evidence_revision=94 video_raw_generation=1 video_ssrc=1958445999 audio_raw_generation=1 audio_ssrc=1144715015
[2026-10-26 15:33:06.713] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=2 reason=1 observed_at_ns=24451629719600
[2026-10-26 15:33:06.716] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=3 generation=3 invalidated_generation=2 locked=0
[2026-10-26 15:33:33.353] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=31089 committed_datagrams=174297 committed_payload_bytes=203599676 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=79296 pacing_reserved=174297 pacing_cancelled=0 pacing_submitted=174297 pacing_maximum_submit_lateness_ns=10341980 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=79296 backlog_max_residence_ns=53644800 last_materialized_sequence=174297 last_scheduled_sequence=174297 last_submitted_sequence=174297 last_committed_sequence=174297 submitted_service_generation=1 submitted_service_sequence=174297 submitted_service_completed_ns=293254162000 submitted_service_duration_ns=34560 submitted_service_available_ns=293254196560 submitted_scope_available_ns=293254196560 shared_scope_charged_datagrams=174297 shared_scope_charged_wire_bytes=208479992 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2154 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2154 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=188010 endpoint_1_target_effective_socket_bytes=188010 endpoint_1_api_requested_socket_bytes=188010 endpoint_1_effective_socket_bytes=188010 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=174297 endpoint_1_committed_payload_bytes=203599676
[2026-10-26 15:33:33.575] [info] [graph][runtime.node] continuous_audio_aggregate.summary reason=stop output_group=composition-r357.output audio_commits=12386 real_samples=10244096 silence_samples=2439168
[2026-10-26 15:33:33.576] [info] [graph][runtime.node] continuous_aggregate.summary reason=stop output_group=composition-r357.output output_generation=1 video_commits=8628 source[0].group=composition-r357.source.0 source[0].real_frames=3415 source[0].black_frames=5213 source[0].contribution_generation=0 source[1].group=composition-r357.source.1 source[1].real_frames=3415 source[1].black_frames=5213 source[1].contribution_generation=0 source[2].group=composition-r357.source.2 source[2].real_frames=6969 source[2].black_frames=1659 source[2].contribution_generation=0 source[2].first_interval_miss_cursor=4277 source[2].first_interval_miss_target_ns=148341059834 source[2].first_interval_miss_steady_ns=24333189051700 source[2].first_interval_miss_generation=2 source[2].first_interval_miss_capacity=7 source[2].first_interval_miss_before=0 source[2].first_interval_miss_after=0 source[2].first_interval_miss_expired=0 source[2].first_interval_miss_channel_depth=0 source[2].first_interval_miss_examined_begin_ns=unavailable source[2].first_interval_miss_examined_end_ns=unavailable
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=938, threads=81, processThreads=129, workers=0, workerProgress=565768, workerProcessCalls=2308580, workerWaits=1742795, workerWakeups=1720878, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=858, logicalProcessors=22, averageSystemMachineCpuPercent=17.336117, averageProcessMachineCpuPercent=2.252160, peakProcessMachineCpuPercent=7.489879, averageProcessSingleCoreCpuPercent=49.547523, peakProcessSingleCoreCpuPercent=164.777328, initialWorkingSetBytes=210235392, workingSetBytes=262594560, peakWorkingSetBytes=271015936, totalPushed=486799, totalPopped=486764, droppedBuffers=0, encodedPacketsPushed=96448, encodedPacketsPopped=96448, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=16357763, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=759, graphPayloadReservations=223823, graphPayloadReleases=223823, graphPayloadPressureFailures=0, lastInputReceivedAtNs=24451628418700
[in @ 0000020f266053c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000020f266053c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 7594.116811
[in @ 0000020f26721480] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000020f26721480] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 4237.563133
[in @ 0000020f7f266480] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000020f7f266480] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 8817.130156
[in @ 0000020f7f15a040] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000020f7f15a040] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 7594.116811
[in @ 0000020f2bfa8f40] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000020f2bfa8f40] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 8817.130156
[in @ 0000020f2bfa75c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000020f2bfa75c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 4237.563133
[aac @ 0000020f2c1a9080] Qavg: 13135.504
faad warning: decoded zero sample
main error: buffer deadlock prevented
main warning: playback too late (73128): up-sampling
d3d11va warning: surface dimensions (1280x768) differ from avcodec dimensions (1280x736)
d3d11va warning: not enough decoding slices in the texture (6/28)
main error: Failed to create video converter
main error: write error: No error
main error: read error: No error
main error: Failed to create video converter
main error: read error: No error
main error: Failed to create video converter
main error: read error: No error
main error: Failed to create video converter
main error: read error: No error
main warning: buffer too late (-475729 us): dropped
main error: read error: No error
main warning: picture is too late to be displayed (missing 821 ms)
main warning: picture is too late to be displayed (missing 790 ms)
main warning: picture is too late to be displayed (missing 756 ms)
main warning: picture is too late to be displayed (missing 723 ms)
main warning: picture is too late to be displayed (missing 690 ms)
direct3d11 error: SetThumbNailClip failed: 0x800706f4
~~~~

composition-r357-cli-error.log (1962 bytes)
composition-r357-cli.log (1070391 bytes)
composition-r357-monitor.log (20816 bytes)
composition-r357-native-2026-10-26-15h29m16s487.png (809322 bytes)
composition-r357-native-2026-10-26-15h30m55s258.png (12152 bytes)
composition-r357-native-2026-10-26-15h31m36s138.png (347733 bytes)
composition-r357-native-2026-10-26-15h33m18s177.png (12152 bytes)
composition-r357-reentry-stdout.log (540 bytes)
composition-r357-reentry.log (29521 bytes)
composition-r357-source0-stdout.log (540 bytes)
composition-r357-source0.log (29521 bytes)
composition-r357-source1-stdout.log (335 bytes)
composition-r357-source1.log (28926 bytes)
composition-r357-source2-stdout.log (335 bytes)
composition-r357-source2.log (28926 bytes)
composition-r357-vlc.log (41972 bytes)

清理完成：上述16项按清单逐项删除，本轮文件残留为空，六个PID均退出；指定源和Release产物保留。本轮未在远程创建产物或进程。

## 后续源码复核：interval miss 的证据边界

root与publication_identity_review_b独立核对：MediaAvContinuousAggregateVideo.cpp的记录条件使用会话累计committedRealFrames，代际purge不清该计数；记录发生在reserve/compose/最终输出提交前。因此该条不能证明gen2已出真实帧后中断，也不能单独证明cursor4277最终提交黑帧。expired只统计build循环，未包含Node prepare阶段丢弃；channel depth是另一个瞬时读数。

MediaActivatedStartupReleaseSequencerNode先activateNext发布epoch，随后commitReserved释放已预约输出；实际视频仍经过extractor/decoder/filter才能到aggregate。epoch可见与候选可取存在异步窗口，但现有日志不足以证明r357正好由此导致。没有找到定容改造改变选中集合、顺序或purge语义的源码证据；耗时变化仍可能影响调度，不能无证据排除。

若继续对该观察归因，需要同gen2的release identity、activation epoch与发布steady、首帧提取/解码/滤镜/候选入队时间与区间、prepare阶段过期丢弃数及cursor4277最终commit类型。保留本轮原始诊断，不将源码解释记为运行修复或完整连续性通过。本次复核不修改生产代码，也不重复运行相同测试来代替缺失证据。