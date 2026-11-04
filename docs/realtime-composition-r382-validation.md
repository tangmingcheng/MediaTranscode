# r382：持有者贡献接线后的 Release realtime 复验

状态：本轮画面/停止行为复验通过，完整合屏验收未完成。产物2026-11-04 11:43:43，7014912字节；本轮四文件局部双审通过，完整费用准入未完成。

## 实际启动命令

```powershell
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r382-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r382-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r382-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r382-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r382-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r382-source2.log' -PassThru
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r382 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r382-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r382-cli-error.log' -PassThru
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r382-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r382-native- --snapshot-format=png udp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
Write-Output "SOURCE0_PID=$($source0.Id) SOURCE1_PID=$($source1.Id) SOURCE2_PID=$($source2.Id) CLI_PID=$($cli.Id) VLC_PID=$($vlc.Id)"
$samples=[Collections.Generic.List[object]]::new()
$previousAt=Get-Date
$cli.Refresh()
$previousCpu=$cli.TotalProcessorTime.TotalSeconds
while(-not $cli.HasExited){
 Start-Sleep -Milliseconds 1000
 $cli.Refresh()
 if($cli.HasExited){break}
 $at=Get-Date
 $cpu=$cli.TotalProcessorTime.TotalSeconds
 $elapsed=($at-$previousAt).TotalSeconds
 $samples.Add([pscustomobject]@{Time=$at.ToString('o');CpuPercent=(100.0*($cpu-$previousCpu)/$elapsed/[Environment]::ProcessorCount);WorkingSet=$cli.WorkingSet64;PrivateBytes=$cli.PrivateMemorySize64})
 $previousAt=$at
 $previousCpu=$cpu
}
$cli.WaitForExit()
Write-Output "CLI_EXIT=$($cli.ExitCode)"
$samples | Export-Csv -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r382-monitor.csv' -NoTypeInformation -Encoding UTF8
$source0.WaitForExit()
$source1.WaitForExit()
$source2.WaitForExit()
Write-Output "SOURCE0_EXIT=$($source0.ExitCode) SOURCE1_EXIT=$($source1.ExitCode) SOURCE2_EXIT=$($source2.ExitCode)"


```

初始PID：source0=9072，source1=11400，source2=24108，CLI=21924，VLC=6392；监控会话51290。11:45:00.844 VLC原生快照已查看，三格游戏画面、右下黑格；CLI存活，真实建图通过，尚未全轮验收。

## 重入、快照与停止命令

```powershell
$reentry=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r382-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r382-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($reentry.Id)"
$reentry.WaitForExit()
Write-Output "REENTRY_EXIT=$($reentry.ExitCode)"
```

```powershell
$cli=Get-CimInstance Win32_Process -Filter 'ProcessId=21924'
if(-not $cli -or $cli.Name -ne 'media_transcode_realtime_video_cli.exe' -or $cli.CommandLine -notlike '*composition-r382*'){throw 'r382 CLI is not alive'}
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 6392){throw 'VLC owner mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
try{$s=$c.GetStream();$s.ReadTimeout=2000;$b=[Text.Encoding]::ASCII.GetBytes('snapshot'+[char]10);$s.Write($b,0,$b.Length);$buf=[byte[]]::new(4096);try{$n=$s.Read($buf,0,$buf.Length);[Text.Encoding]::UTF8.GetString($buf,0,$n)}catch{Write-Output $_.Exception.Message}}finally{$c.Dispose()}
Get-ChildItem -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance' -Filter 'composition-r382-native-*.png' | Select-Object FullName,LastWriteTime
```

```powershell
$target=Get-CimInstance Win32_Process -Filter "ProcessId=21924"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r382*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R382Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R382Console]::FreeConsole() | Out-Null
if(-not [R382Console]::AttachConsole(21924)){throw 'AttachConsole failed'}
try {
[R382Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R382Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 21924 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R382Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R382Console]::FreeConsole() | Out-Null}
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 6392){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
try{$s=$c.GetStream();$b=[Text.Encoding]::ASCII.GetBytes('quit'+[char]10);$s.Write($b,0,$b.Length)}finally{$c.Dispose()}


```

## 结果与边界

本轮行为复验通过，不是整项合屏验收PASS。三初始源PID9072/11400/24108和重入PID7312均完整120秒/3600帧退出0；CLI21924 caller-stop退出0，非全失源自动结束；VLC6392已quit。启动监控会话51290及重入84571均终态退出0。

四张VLC原生图已在CLI存活时查看：11:45:00.844三格/右下黑；11:46:43.020全黑；11:47:13.627左下重入其余黑；11:48:55.183全黑。

最终errors/workerErrors/pressureFailures/stalledIntervals/droppedBuffers均0，workers0，graphPayloadCurrentBytes/Objects均0，218146次预约/释放相等。CPU平均机器占比1.595898%，峰7.484407%；working set初始211783680、峰270077952、末261439488字节。此短轮不能证明长期内存稳定。

sender提交171871 datagrams/203245296 payload bytes，deadline_misses/pressure/partial/ambiguous失败均0；最差提交迟到29.440240ms（seq132813），最大backlog residence52.899ms。delivery_evidence=not_proven，无本轮抓包，不声明线速交付或硬实时已证明。保留stderr中的frame属性变化警告，不因退出0省略。内容级A/V、静音与完整矩阵仍未验收；本轮没有修改媒体源码、没有local CLI测试或RKMPP测试。

## 本轮临时文件清单及归档证据

composition-r382-cli-error.log | 1959 bytes
composition-r382-cli.log | 1145143 bytes
composition-r382-monitor.csv | 21943 bytes
composition-r382-native-2026-11-04-11h45m00s844.png | 900725 bytes
composition-r382-native-2026-11-04-11h46m43s020.png | 12152 bytes
composition-r382-native-2026-11-04-11h47m13s627.png | 335489 bytes
composition-r382-native-2026-11-04-11h48m55s183.png | 12152 bytes
composition-r382-reentry-stdout.log | 540 bytes
composition-r382-reentry.log | 29521 bytes
composition-r382-source0-stdout.log | 540 bytes
composition-r382-source0.log | 29521 bytes
composition-r382-source1-stdout.log | 335 bytes
composition-r382-source1.log | 28926 bytes
composition-r382-source2-stdout.log | 335 bytes
composition-r382-source2.log | 28926 bytes
composition-r382-vlc.log | 40572 bytes

### composition-r382-cli-error.log

~~~~text
[in @ 000002f84383f380] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002f84383f380] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 3101.6789
[in @ 000002f848c410c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002f848c410c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 5610.696889
[in @ 000002f84937a5c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002f84937a5c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 35225.267411
[in @ 000002f843b81ac0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002f843b81ac0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 5610.696889
[in @ 000002f843b7e6c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002f843b7e6c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 35225.267411
[in @ 000002f843b7ff00] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002f843b7ff00] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 3101.6789
[aac @ 000002f8490a1240] Qavg: 8356.500

~~~~

### composition-r382-cli.log

~~~~text
[CLI] composition_inputs=3 grid=fixed audio_source=2 audio=on max_duration=source_driven hw=planner-highest-score
[2026-11-04 11:44:35.519] [info] [graph][planner.capability] composition_protocol_installed source=0
[2026-11-04 11:44:35.520] [info] [graph][planner.capability] composition_protocol_installed source=1
[2026-11-04 11:44:35.522] [info] [graph][planner.capability] composition_protocol_installed source=2
[2026-11-04 11:44:35.620] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-04 11:44:35.622] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-04 11:44:35.622] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-r382.source.0 decoder=h264
[2026-11-04 11:44:40.014] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-11-04 11:44:40.021] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-11-04 11:44:40.024] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-11-04 11:44:40.097] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-04 11:44:40.097] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-04 11:44:40.097] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-r382.source.1 decoder=h264
[2026-11-04 11:44:40.148] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-04 11:44:40.148] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-04 11:44:40.148] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-r382.source.2 decoder=h264
[2026-11-04 11:44:40.234] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-11-04 11:44:40.275] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-11-04 11:44:40.275] [info] [graph][planner.capability] canvas_cuda_stream stage=prepared device_stream=0000000000000000 private_nonblocking=1
[2026-11-04 11:44:40.282] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-04 11:44:40.282] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-04 11:44:40.282] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-11-04 11:44:40.288] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-04 11:44:40.288] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-04 11:44:40.288] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-11-04 11:44:40.295] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-04 11:44:40.295] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-04 11:44:40.295] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-11-04 11:44:40.299] [info] [graph][planner.capability] composition_preparation_storage stage=graph_bound scope=shared_reservations capacity=2147483648 retained=832265478 high_water=840992944
[CLI] audio_plan branch=transcode_frame reason=transcode_required_by_output_frame_contract codec=aac sample_rate=44100 channels=2 access_unit_samples=1024 encoder=aac bitrate_kbps=192
[CLI] prepared_output_id=1
[CLI] selected_chain=fixed-grid-composition score=0 decoder=h264,h264,h264 filter=scale_cuda=640:360,scale_cuda=640:360,scale_cuda=640:360 encoder=hevc_nvenc
[2026-11-04 11:44:40.301] [info] [graph][runtime.lifecycle] compile.begin
[2026-11-04 11:44:40.302] [info] [graph][runtime.channel] create channel=1 edge=1 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 11:44:40.302] [info] [graph][runtime.channel] create channel=2 edge=2 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 11:44:40.302] [info] [graph][runtime.channel] create channel=3 edge=3 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-04 11:44:40.302] [info] [graph][runtime.channel] create channel=4 edge=4 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.302] [info] [graph][runtime.channel] create channel=5 edge=5 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.302] [info] [graph][runtime.channel] create channel=6 edge=6 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.303] [info] [graph][runtime.channel] create channel=7 edge=7 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.303] [info] [graph][runtime.channel] create channel=8 edge=8 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.303] [info] [graph][runtime.channel] create channel=9 edge=9 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 11:44:40.303] [info] [graph][runtime.channel] create channel=10 edge=10 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.303] [info] [graph][runtime.channel] create channel=11 edge=11 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.303] [info] [graph][runtime.channel] create channel=12 edge=12 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 11:44:40.303] [info] [graph][runtime.channel] create channel=13 edge=13 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 11:44:40.303] [info] [graph][runtime.channel] create channel=14 edge=14 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.303] [info] [graph][runtime.channel] create channel=15 edge=15 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.304] [info] [graph][runtime.channel] create channel=16 edge=16 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.304] [info] [graph][runtime.channel] create channel=17 edge=17 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.304] [info] [graph][runtime.channel] create channel=18 edge=18 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.304] [info] [graph][runtime.channel] create channel=19 edge=19 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.304] [info] [graph][runtime.channel] create channel=20 edge=20 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.304] [info] [graph][runtime.channel] create channel=21 edge=21 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-04 11:44:40.304] [info] [graph][runtime.channel] create channel=22 edge=22 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 11:44:40.304] [info] [graph][runtime.channel] create channel=23 edge=23 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 11:44:40.304] [info] [graph][runtime.channel] create channel=24 edge=24 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 11:44:40.304] [info] [graph][runtime.channel] create channel=25 edge=25 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 11:44:40.304] [info] [graph][runtime.channel] create channel=26 edge=26 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 11:44:40.304] [info] [graph][runtime.channel] create channel=27 edge=27 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 11:44:40.305] [info] [graph][runtime.channel] create channel=28 edge=28 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 11:44:40.305] [info] [graph][runtime.channel] create channel=29 edge=29 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 11:44:40.305] [info] [graph][runtime.channel] create channel=30 edge=30 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.305] [info] [graph][runtime.channel] create channel=31 edge=31 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.305] [info] [graph][runtime.channel] create channel=32 edge=32 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.305] [info] [graph][runtime.channel] create channel=33 edge=33 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.305] [info] [graph][runtime.channel] create channel=34 edge=34 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.305] [info] [graph][runtime.channel] create channel=35 edge=35 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.305] [info] [graph][runtime.channel] create channel=36 edge=36 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 11:44:40.305] [info] [graph][runtime.channel] create channel=37 edge=37 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.305] [info] [graph][runtime.channel] create channel=38 edge=38 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.306] [info] [graph][runtime.channel] create channel=39 edge=39 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 11:44:40.306] [info] [graph][runtime.channel] create channel=40 edge=40 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 11:44:40.306] [info] [graph][runtime.channel] create channel=41 edge=41 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.306] [info] [graph][runtime.channel] create channel=42 edge=42 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.306] [info] [graph][runtime.channel] create channel=43 edge=43 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.306] [info] [graph][runtime.channel] create channel=44 edge=44 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.306] [info] [graph][runtime.channel] create channel=45 edge=45 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.306] [info] [graph][runtime.channel] create channel=46 edge=46 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.306] [info] [graph][runtime.channel] create channel=47 edge=47 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.306] [info] [graph][runtime.channel] create channel=48 edge=48 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-04 11:44:40.306] [info] [graph][runtime.channel] create channel=49 edge=49 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 11:44:40.306] [info] [graph][runtime.channel] create channel=50 edge=50 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 11:44:40.306] [info] [graph][runtime.channel] create channel=51 edge=51 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 11:44:40.306] [info] [graph][runtime.channel] create channel=52 edge=52 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 11:44:40.306] [info] [graph][runtime.channel] create channel=53 edge=53 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 11:44:40.307] [info] [graph][runtime.channel] create channel=54 edge=54 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 11:44:40.307] [info] [graph][runtime.channel] create channel=55 edge=55 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 11:44:40.307] [info] [graph][runtime.channel] create channel=56 edge=56 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 11:44:40.307] [info] [graph][runtime.channel] create channel=57 edge=57 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.307] [info] [graph][runtime.channel] create channel=58 edge=58 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.307] [info] [graph][runtime.channel] create channel=59 edge=59 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.307] [info] [graph][runtime.channel] create channel=60 edge=60 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.307] [info] [graph][runtime.channel] create channel=61 edge=61 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.307] [info] [graph][runtime.channel] create channel=62 edge=62 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.307] [info] [graph][runtime.channel] create channel=63 edge=63 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.307] [info] [graph][runtime.channel] create channel=64 edge=64 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.307] [info] [graph][runtime.channel] create channel=65 edge=65 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.307] [info] [graph][runtime.channel] create channel=66 edge=66 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 11:44:40.307] [info] [graph][runtime.channel] create channel=67 edge=67 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 11:44:40.307] [info] [graph][runtime.channel] create channel=68 edge=68 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.307] [info] [graph][runtime.channel] create channel=69 edge=69 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.308] [info] [graph][runtime.channel] create channel=70 edge=70 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.308] [info] [graph][runtime.channel] create channel=71 edge=71 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 11:44:40.308] [info] [graph][runtime.channel] create channel=72 edge=72 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 11:44:40.308] [info] [graph][runtime.channel] create channel=73 edge=73 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 11:44:40.308] [info] [graph][runtime.channel] create channel=74 edge=74 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 11:44:40.308] [info] [graph][runtime.channel] create channel=75 edge=75 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.308] [info] [graph][runtime.channel] create channel=76 edge=76 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.308] [info] [graph][runtime.channel] create channel=77 edge=77 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.308] [info] [graph][runtime.channel] create channel=78 edge=78 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.308] [info] [graph][runtime.channel] create channel=79 edge=79 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.308] [info] [graph][runtime.channel] create channel=80 edge=80 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.308] [info] [graph][runtime.channel] create channel=81 edge=81 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.308] [info] [graph][runtime.channel] create channel=82 edge=82 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.309] [info] [graph][runtime.channel] create channel=83 edge=83 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-04 11:44:40.309] [info] [graph][runtime.channel] create channel=84 edge=84 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 11:44:40.309] [info] [graph][runtime.channel] create channel=85 edge=85 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 11:44:40.309] [info] [graph][runtime.channel] create channel=86 edge=86 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 11:44:40.309] [info] [graph][runtime.channel] create channel=87 edge=87 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 11:44:40.309] [info] [graph][runtime.channel] create channel=88 edge=88 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 11:44:40.309] [info] [graph][runtime.channel] create channel=89 edge=89 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 11:44:40.309] [info] [graph][runtime.channel] create channel=90 edge=90 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 11:44:40.309] [info] [graph][runtime.channel] create channel=91 edge=91 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-04 11:44:40.310] [info] [graph][runtime.channel] create channel=92 edge=92 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 11:44:40.310] [info] [graph][runtime.channel] create channel=93 edge=93 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 11:44:40.310] [info] [graph][runtime.channel] create channel=94 edge=94 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 11:44:40.310] [info] [graph][runtime.channel] create channel=95 edge=95 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 11:44:40.310] [info] [graph][runtime.channel] create channel=96 edge=96 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 11:44:40.310] [info] [graph][runtime.channel] create channel=97 edge=97 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 11:44:40.310] [info] [graph][runtime.channel] create channel=98 edge=98 stream=Audio edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.310] [info] [graph][runtime.channel] create channel=99 edge=99 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 11:44:40.310] [info] [graph][runtime.channel] create channel=100 edge=100 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.310] [info] [graph][runtime.channel] create channel=101 edge=101 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0
[2026-11-04 11:44:40.311] [info] [graph][runtime.channel] create channel=102 edge=102 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 11:44:40.311] [info] [graph][runtime.channel] create channel=103 edge=103 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 11:44:40.311] [info] [graph][runtime.channel] create channel=104 edge=104 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 11:44:40.311] [info] [graph][runtime.channel] create channel=105 edge=105 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0
[2026-11-04 11:44:40.311] [info] [graph][runtime.channel] create channel=106 edge=106 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.311] [info] [graph][runtime.channel] create channel=107 edge=107 stream=Video edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-04 11:44:40.311] [info] [graph][runtime.channel] create channel=108 edge=108 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-04 11:44:40.311] [info] [graph][runtime.channel] create channel=109 edge=109 stream=Any edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-04 11:44:40.311] [info] [graph][runtime.channel] create channel=110 edge=110 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.311] [info] [graph][runtime.channel] create channel=111 edge=111 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-04 11:44:40.312] [info] [graph][runtime.channel] create channel=112 edge=112 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:44:40.312] [info] [graph][runtime.channel] create channel=113 edge=113 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 11:44:40.312] [info] [graph][runtime.channel] create channel=114 edge=114 stream=Any edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-04 11:44:40.312] [info] [graph][runtime.channel] create channel=115 edge=115 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 11:44:40.312] [info] [graph][runtime.channel] create channel=116 edge=116 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-04 11:44:40.312] [info] [graph][runtime.channel] create channel=117 edge=117 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-04 11:44:40.312] [info] [graph][runtime.channel] create channel=118 edge=118 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-04 11:44:40.312] [info] [graph][runtime.channel] create channel=119 edge=119 stream=Any edge_kind=EncodedPacket payload=Unknown queued=0
[2026-11-04 11:44:40.312] [info] [graph][runtime.channel] create channel=120 edge=120 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-04 11:44:40.312] [info] [graph][runtime.channel] create channel=121 edge=121 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-04 11:44:40.313] [info] [graph][runtime.channel] create channel=122 edge=122 stream=Metadata edge_kind=Unknown payload=Unknown queued=0
[2026-11-04 11:44:40.313] [info] [graph][runtime.channel] create channel=123 edge=123 stream=Metadata edge_kind=Unknown payload=Unknown queued=0
[2026-11-04 11:44:40.313] [info] [graph][runtime.lifecycle] distribution_registration scope=capability_probe stage=enter node=none pid=21924 tid=29584 native=0
[2026-11-04 11:44:40.313] [info] [graph][runtime.lifecycle] distribution_registration scope=capability_probe stage=leave node=none pid=21924 tid=29584 native=0
[2026-11-04 11:44:40.314] [info] [graph][runtime.lifecycle] compiled nodes=81 edges=123 channels=123 execution_order=1->3->8->27->46->47->9->21->28->40->63->48->69->10->29->49->11->12->30->31->50->51->52->13->32->53->14->17->33->36->54->55->59->15->34->56->57->16->35->58->18->37->60->19->38->61->20->39->62->22->41->64->70->23->42->65->71->24->43->66->72->26->45->68->73->25->44->67->6->2->7->80->4->77->5->74->75->78->76->79->81
[2026-11-04 11:44:40.314] [info] [graph][runtime.lifecycle] compile.done state=DefaultRegistrationPending
[2026-11-04 11:44:40.314] [info] [graph][runtime.node] register node=1 name=composition-r382.output.video.codec_resolver kind=CodecResolver
[2026-11-04 11:44:40.314] [info] [graph][runtime.node] register node=2 name=composition-r382.output.video.encode kind=VideoEncode
[2026-11-04 11:44:40.314] [info] [graph][runtime.node] register node=3 name=composition-r382.output.audio.codec_resolver kind=AudioCodecResolver
[2026-11-04 11:44:40.315] [info] [graph][runtime.node] register node=4 name=composition-r382.output.audio.encode kind=AudioEncode
[2026-11-04 11:44:40.315] [info] [graph][runtime.node] register node=5 name=composition-r382.output.audio.canonicalizer kind=EncodedAudioCanonicalizer
[2026-11-04 11:44:40.315] [info] [graph][runtime.node] register node=8 name=composition-r382.source_0.input kind=RawRtpInput
[2026-11-04 11:44:40.315] [info] [graph][runtime.node] register node=9 name=composition-r382.source_0.rtp.clock_group kind=RtpClockGroup
[2026-11-04 11:44:40.315] [info] [graph][runtime.node] register node=10 name=composition-r382.source_0.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-11-04 11:44:40.315] [info] [graph][runtime.node] register node=11 name=composition-r382.source_0.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-11-04 11:44:40.315] [info] [graph][runtime.node] register node=12 name=composition-r382.source_0.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-11-04 11:44:40.315] [info] [graph][runtime.node] register node=13 name=composition-r382.source_0.av_sync.source_clock kind=SourceClockStateFanout
[2026-11-04 11:44:40.315] [info] [graph][runtime.node] register node=14 name=composition-r382.source_0.av_sync.video.generation_gate kind=LockedPacketGate
[2026-11-04 11:44:40.315] [info] [graph][runtime.node] register node=15 name=composition-r382.source_0.av_sync.video.canonical_input kind=CanonicalInput
[2026-11-04 11:44:40.316] [info] [graph][runtime.node] register node=16 name=composition-r382.source_0.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-11-04 11:44:40.316] [info] [graph][runtime.node] register node=17 name=composition-r382.source_0.av_sync.startup.clock kind=AvStartupClock
[2026-11-04 11:44:40.316] [info] [graph][runtime.node] register node=18 name=composition-r382.source_0.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-11-04 11:44:40.316] [info] [graph][runtime.node] register node=20 name=composition-r382.source_0.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-11-04 11:44:40.316] [info] [graph][runtime.node] register node=21 name=composition-r382.source_0.video.codec_resolver kind=CodecResolver
[2026-11-04 11:44:40.316] [info] [graph][runtime.node] register node=22 name=composition-r382.source_0.video.decode kind=VideoDecode
[2026-11-04 11:44:40.316] [info] [graph][runtime.node] register node=23 name=composition-r382.source_0.video.hwtransfer kind=HardwareTransfer
[2026-11-04 11:44:40.316] [info] [graph][runtime.node] register node=24 name=composition-r382.source_0.video.framerate kind=VideoFrameRate
[2026-11-04 11:44:40.316] [info] [graph][runtime.node] register node=25 name=composition-r382.source_0.video.preparation kind=VideoPreparation
[2026-11-04 11:44:40.316] [info] [graph][runtime.node] register node=26 name=composition-r382.source_0.video.filter kind=VideoFilter
[2026-11-04 11:44:40.316] [info] [graph][runtime.node] register node=27 name=composition-r382.source_1.input kind=RawRtpInput
[2026-11-04 11:44:40.316] [info] [graph][runtime.node] register node=28 name=composition-r382.source_1.rtp.clock_group kind=RtpClockGroup
[2026-11-04 11:44:40.316] [info] [graph][runtime.node] register node=29 name=composition-r382.source_1.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-11-04 11:44:40.316] [info] [graph][runtime.node] register node=30 name=composition-r382.source_1.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-11-04 11:44:40.317] [info] [graph][runtime.node] register node=31 name=composition-r382.source_1.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-11-04 11:44:40.317] [info] [graph][runtime.node] register node=32 name=composition-r382.source_1.av_sync.source_clock kind=SourceClockStateFanout
[2026-11-04 11:44:40.317] [info] [graph][runtime.node] register node=33 name=composition-r382.source_1.av_sync.video.generation_gate kind=LockedPacketGate
[2026-11-04 11:44:40.317] [info] [graph][runtime.node] register node=34 name=composition-r382.source_1.av_sync.video.canonical_input kind=CanonicalInput
[2026-11-04 11:44:40.317] [info] [graph][runtime.node] register node=35 name=composition-r382.source_1.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-11-04 11:44:40.317] [info] [graph][runtime.node] register node=36 name=composition-r382.source_1.av_sync.startup.clock kind=AvStartupClock
[2026-11-04 11:44:40.317] [info] [graph][runtime.node] register node=37 name=composition-r382.source_1.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-11-04 11:44:40.317] [info] [graph][runtime.node] register node=39 name=composition-r382.source_1.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-11-04 11:44:40.317] [info] [graph][runtime.node] register node=40 name=composition-r382.source_1.video.codec_resolver kind=CodecResolver
[2026-11-04 11:44:40.317] [info] [graph][runtime.node] register node=41 name=composition-r382.source_1.video.decode kind=VideoDecode
[2026-11-04 11:44:40.317] [info] [graph][runtime.node] register node=42 name=composition-r382.source_1.video.hwtransfer kind=HardwareTransfer
[2026-11-04 11:44:40.317] [info] [graph][runtime.node] register node=43 name=composition-r382.source_1.video.framerate kind=VideoFrameRate
[2026-11-04 11:44:40.317] [info] [graph][runtime.node] register node=44 name=composition-r382.source_1.video.preparation kind=VideoPreparation
[2026-11-04 11:44:40.317] [info] [graph][runtime.node] register node=45 name=composition-r382.source_1.video.filter kind=VideoFilter
[2026-11-04 11:44:40.317] [info] [graph][runtime.node] register node=46 name=composition-r382.source_2.video.input kind=RawRtpInput
[2026-11-04 11:44:40.317] [info] [graph][runtime.node] register node=47 name=composition-r382.source_2.audio.input kind=RawRtpInput
[2026-11-04 11:44:40.317] [info] [graph][runtime.node] register node=48 name=composition-r382.source_2.rtp.clock_group kind=RtpClockGroup
[2026-11-04 11:44:40.318] [info] [graph][runtime.node] register node=49 name=composition-r382.source_2.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-11-04 11:44:40.318] [info] [graph][runtime.node] register node=50 name=composition-r382.source_2.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-11-04 11:44:40.318] [info] [graph][runtime.node] register node=51 name=composition-r382.source_2.av_sync.audio.protocol_binder kind=RtpPacketClockBinder
[2026-11-04 11:44:40.318] [info] [graph][runtime.node] register node=52 name=composition-r382.source_2.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-11-04 11:44:40.318] [info] [graph][runtime.node] register node=53 name=composition-r382.source_2.av_sync.source_clock kind=SourceClockStateFanout
[2026-11-04 11:44:40.318] [info] [graph][runtime.node] register node=54 name=composition-r382.source_2.av_sync.video.generation_gate kind=LockedPacketGate
[2026-11-04 11:44:40.318] [info] [graph][runtime.node] register node=55 name=composition-r382.source_2.av_sync.audio.generation_gate kind=LockedPacketGate
[2026-11-04 11:44:40.318] [info] [graph][runtime.node] register node=56 name=composition-r382.source_2.av_sync.video.canonical_input kind=CanonicalInput
[2026-11-04 11:44:40.318] [info] [graph][runtime.node] register node=57 name=composition-r382.source_2.av_sync.audio.canonical_input kind=CanonicalInput
[2026-11-04 11:44:40.318] [info] [graph][runtime.node] register node=58 name=composition-r382.source_2.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-11-04 11:44:40.318] [info] [graph][runtime.node] register node=59 name=composition-r382.source_2.av_sync.startup.clock kind=AvStartupClock
[2026-11-04 11:44:40.318] [info] [graph][runtime.node] register node=60 name=composition-r382.source_2.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-11-04 11:44:40.318] [info] [graph][runtime.node] register node=62 name=composition-r382.source_2.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-11-04 11:44:40.318] [info] [graph][runtime.node] register node=63 name=composition-r382.source_2.video.codec_resolver kind=CodecResolver
[2026-11-04 11:44:40.318] [info] [graph][runtime.node] register node=64 name=composition-r382.source_2.video.decode kind=VideoDecode
[2026-11-04 11:44:40.318] [info] [graph][runtime.node] register node=65 name=composition-r382.source_2.video.hwtransfer kind=HardwareTransfer
[2026-11-04 11:44:40.318] [info] [graph][runtime.node] register node=66 name=composition-r382.source_2.video.framerate kind=VideoFrameRate
[2026-11-04 11:44:40.318] [info] [graph][runtime.node] register node=67 name=composition-r382.source_2.video.preparation kind=VideoPreparation
[2026-11-04 11:44:40.319] [info] [graph][runtime.node] register node=68 name=composition-r382.source_2.video.filter kind=VideoFilter
[2026-11-04 11:44:40.319] [info] [graph][runtime.node] register node=69 name=composition-r382.source_2.audio.encode.codec_resolver kind=AudioCodecResolver
[2026-11-04 11:44:40.319] [info] [graph][runtime.node] register node=70 name=composition-r382.source_2.audio.encode.decode kind=AudioDecode
[2026-11-04 11:44:40.319] [info] [graph][runtime.node] register node=71 name=composition-r382.source_2.audio.encode.startup_trim kind=Unknown
[2026-11-04 11:44:40.319] [info] [graph][runtime.node] register node=72 name=composition-r382.source_2.audio.encode.drift_controller kind=AudioDriftController
[2026-11-04 11:44:40.319] [info] [graph][runtime.node] register node=73 name=composition-r382.source_2.audio.encode.resample kind=AudioResample
[2026-11-04 11:44:40.319] [info] [graph][runtime.node] register node=74 name=composition-r382.output.scheduler.scheduler kind=AvOutputScheduler
[2026-11-04 11:44:40.319] [info] [graph][runtime.node] register node=75 name=composition-r382.output.scheduler.router kind=ScheduledOutputRouter
[2026-11-04 11:44:40.319] [info] [graph][runtime.node] register node=76 name=composition-r382.output.mpegts.mux kind=FileMux
[2026-11-04 11:44:40.319] [info] [graph][runtime.node] register node=77 name=composition-r382.output.mpegts.plan.source kind=Unknown
[2026-11-04 11:44:40.319] [info] [graph][runtime.node] register node=78 name=composition-r382.output.mpegts.scheduled.adapter kind=Unknown
[2026-11-04 11:44:40.319] [info] [graph][runtime.node] register node=79 name=composition-r382.output.mpegts.datagram.materializer kind=MpegTsDatagramMaterializer
[2026-11-04 11:44:40.320] [info] [graph][runtime.node] register node=80 name=composition-r382.output.mpegts.transport.plan kind=DatagramTransportPlanSource
[2026-11-04 11:44:40.320] [info] [graph][runtime.node] register node=81 name=composition-r382.output.mpegts.transport.sender kind=ScheduledDatagramSender
[2026-11-04 11:44:40.320] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=waiting_for_random_access detail="" description=""
[2026-11-04 11:44:40.321] [info] [graph][runtime.lifecycle] start.begin mode=threaded
[2026-11-04 11:44:40.339] [info] [graph][runtime.lifecycle] distribution_registration scope=worker stage=enter node=81 pid=21924 tid=220 native=0
[2026-11-04 11:44:40.340] [info] [graph][runtime.lifecycle] start.done state=ThreadedRunning
[2026-11-04 11:44:40.340] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:44:40.340] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:44:40.340] [info] [graph][runtime.lifecycle] worker.finished node=1
[2026-11-04 11:44:40.341] [info] [graph][runtime.node] worker.thread_cpu node=1 available=1 cpu_ns=0 process_calls=1
[2026-11-04 11:44:40.340] [info] [graph][runtime.node] video_encode.bind_encoder codec=hevc_nvenc pix_fmt=cuda rate_control=cbr bitrate=8000000 min_bitrate=8000000 max_bitrate=8000000 buffer_size=267000 fps=30/1 gop=60 frame_kind=hardware hwaccel=cuda hw_device_ctx=set hw_frames_ctx=set
[2026-11-04 11:44:40.340] [info] [graph][runtime.lifecycle] worker.finished node=63
[2026-11-04 11:44:40.342] [info] [graph][runtime.node] worker.thread_cpu node=63 available=1 cpu_ns=0 process_calls=2
[2026-11-04 11:44:40.340] [info] [graph][runtime.lifecycle] worker.finished node=40
[2026-11-04 11:44:40.342] [info] [graph][runtime.node] worker.thread_cpu node=40 available=1 cpu_ns=0 process_calls=2
[2026-11-04 11:44:40.340] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:44:40.341] [info] [graph][runtime.lifecycle] worker.finished node=21
[2026-11-04 11:44:40.342] [info] [graph][runtime.node] worker.thread_cpu node=21 available=1 cpu_ns=0 process_calls=2
[2026-11-04 11:44:40.340] [info] [graph][runtime.node] audio_encode_trace stage=codec_bound
[2026-11-04 11:44:40.342] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-11-04 11:44:40.342] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-11-04 11:44:40.343] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=15 generation=1
[2026-11-04 11:44:40.340] [info] [graph][runtime.lifecycle] worker.finished node=3
[2026-11-04 11:44:40.343] [info] [graph][runtime.node] worker.thread_cpu node=3 available=1 cpu_ns=0 process_calls=1
[2026-11-04 11:44:40.341] [info] [graph][runtime.lifecycle] worker.finished node=69
[2026-11-04 11:44:40.344] [info] [graph][runtime.node] worker.thread_cpu node=69 available=1 cpu_ns=0 process_calls=2
[2026-11-04 11:44:40.343] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=17 generation=1
[2026-11-04 11:44:40.344] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-11-04 11:44:40.344] [info] [graph][runtime.node] source_video_trace node_id=20 node=MediaAvBoundReleaseExtractorNode stage=release source_identity=composition-r382.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4020000000 duration_ns=33333333 decoded_sequence=unavailable pts=279151101 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11245261256500 group=composition-r382.source.0 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-4020000000 epoch_master_release_ns=5434177100 video_count=15 min_presentation_ns=-4020000000 max_end_ns=-3520000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.344] [info] [graph][runtime.node] source_video_trace node_id=20 node=MediaAvBoundReleaseExtractorNode stage=published source_identity=composition-r382.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4020000000 duration_ns=33333333 decoded_sequence=unavailable pts=279151101 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11245261490300 group=composition-r382.source.0 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.344] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=25 generation=1
[2026-11-04 11:44:40.344] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=279151101 dts=279151101 size=317016 generation=1 sequence=15
[2026-11-04 11:44:40.345] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=release source_identity=composition-r382.source.2.input.video identity_link=direct generation=1 source_sequence=17 presentation_ns=-4026000000 duration_ns=33333333 decoded_sequence=unavailable pts=3170274067 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11245261981600 group=composition-r382.source.2 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=-4026000000 epoch_master_release_ns=5434872100 video_count=15 min_presentation_ns=-4026000000 max_end_ns=-3526000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.345] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=input source_identity=composition-r382.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4020000000 duration_ns=33333333 decoded_sequence=unavailable pts=279151101 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11245261999600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.345] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=3170274067 dts=3170274067 size=317016 generation=1 sequence=17
[2026-11-04 11:44:40.345] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=input source_identity=composition-r382.source.2.input.video identity_link=direct generation=1 source_sequence=17 presentation_ns=-4026000000 duration_ns=33333333 decoded_sequence=unavailable pts=3170274067 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11245262411500 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.345] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=published source_identity=composition-r382.source.2.input.video identity_link=direct generation=1 source_sequence=17 presentation_ns=-4026000000 duration_ns=33333333 decoded_sequence=unavailable pts=3170274067 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11245262123600 group=composition-r382.source.2 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.345] [info] [graph][runtime.node] source_video_trace node_id=39 node=MediaAvBoundReleaseExtractorNode stage=release source_identity=composition-r382.source.1.input.video identity_link=direct generation=1 source_sequence=25 presentation_ns=-4020000000 duration_ns=33333333 decoded_sequence=unavailable pts=504962720 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11245262607400 group=composition-r382.source.1 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-4020000000 epoch_master_release_ns=5435422000 video_count=15 min_presentation_ns=-4020000000 max_end_ns=-3520000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.346] [info] [graph][runtime.node] source_video_trace node_id=39 node=MediaAvBoundReleaseExtractorNode stage=published source_identity=composition-r382.source.1.input.video identity_link=direct generation=1 source_sequence=25 presentation_ns=-4020000000 duration_ns=33333333 decoded_sequence=unavailable pts=504962720 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11245262904700 group=composition-r382.source.1 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.346] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=504962720 dts=504962720 size=317016 generation=1 sequence=25
[2026-11-04 11:44:40.346] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=input source_identity=composition-r382.source.1.input.video identity_link=direct generation=1 source_sequence=25 presentation_ns=-4020000000 duration_ns=33333333 decoded_sequence=unavailable pts=504962720 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11245263205900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.373] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-11-04 11:44:40.373] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=accepted source_identity=composition-r382.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4020000000 duration_ns=33333333 decoded_sequence=unavailable pts=279151101 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11245290315000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.373] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=279151101 format=cuda size=1280x720 buffer_identity=0xb08600000 drm_prime=0 software_frame=0
[2026-11-04 11:44:40.373] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=resolved source_identity=composition-r382.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4020000000 duration_ns=33333333 decoded_sequence=unavailable pts=279151101 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11245290700000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.374] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=published source_identity=composition-r382.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4020000000 duration_ns=33333333 decoded_sequence=unavailable pts=279151101 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11245290828300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.374] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=000002F8431FCDB0 stream=Video payload=Frame pts=279151101 dts=279151101 duration=3000 tb=1/90000 flags=68
[2026-11-04 11:44:40.374] [info] [graph][runtime.node] source_video_trace node_id=23 node=HardwareTransferNode stage=published source_identity=composition-r382.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4020000000 duration_ns=33333333 decoded_sequence=unavailable pts=279151101 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11245291078300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.374] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=000002F8431FCDB0 stream=Video payload=Frame pts=279151101 dts=279151101 duration=3000 tb=1/90000 flags=68
[2026-11-04 11:44:40.374] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=000002F8431FCDB0 stream=Video payload=Frame pts=279151101 dts=279151101 duration=3000 tb=1/90000 flags=68
[2026-11-04 11:44:40.374] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-11-04 11:44:40.374] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=000002F8431FD1A0 stream=Video payload=Frame pts=279151101 dts=279151101 duration=3000 tb=1/90000 flags=68
[2026-11-04 11:44:40.374] [info] [graph][runtime.node] source_video_trace node_id=24 node=VideoFrameRateNode stage=published source_identity=composition-r382.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4020000000 duration_ns=33333333 decoded_sequence=unavailable pts=279151101 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11245291477000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.374] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=000002F8431FD1A0 stream=Video payload=Frame pts=279151101 dts=279151101 duration=3000 tb=1/90000 flags=68
[2026-11-04 11:44:40.374] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-11-04 11:44:40.374] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-11-04 11:44:40.375] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=accepted source_identity=composition-r382.source.1.input.video identity_link=direct generation=1 source_sequence=25 presentation_ns=-4020000000 duration_ns=33333333 decoded_sequence=unavailable pts=504962720 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11245291897800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.375] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=504962720 format=cuda size=1280x720 buffer_identity=0xb08800000 drm_prime=0 software_frame=0
[2026-11-04 11:44:40.375] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=resolved source_identity=composition-r382.source.1.input.video identity_link=direct generation=1 source_sequence=25 presentation_ns=-4020000000 duration_ns=33333333 decoded_sequence=unavailable pts=504962720 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11245292109600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.375] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=published source_identity=composition-r382.source.1.input.video identity_link=direct generation=1 source_sequence=25 presentation_ns=-4020000000 duration_ns=33333333 decoded_sequence=unavailable pts=504962720 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11245292210200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.375] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=000002F8431FA260 stream=Video payload=Frame pts=504962720 dts=504962720 duration=3000 tb=1/90000 flags=68
[2026-11-04 11:44:40.375] [info] [graph][runtime.node] source_video_trace node_id=42 node=HardwareTransferNode stage=published source_identity=composition-r382.source.1.input.video identity_link=direct generation=1 source_sequence=25 presentation_ns=-4020000000 duration_ns=33333333 decoded_sequence=unavailable pts=504962720 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11245292488700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.375] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=000002F8431FA260 stream=Video payload=Frame pts=504962720 dts=504962720 duration=3000 tb=1/90000 flags=68
[2026-11-04 11:44:40.375] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-11-04 11:44:40.375] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=000002F8431FA260 stream=Video payload=Frame pts=504962720 dts=504962720 duration=3000 tb=1/90000 flags=68
[2026-11-04 11:44:40.376] [info] [graph][runtime.node] source_video_trace node_id=43 node=VideoFrameRateNode stage=published source_identity=composition-r382.source.1.input.video identity_link=direct generation=1 source_sequence=25 presentation_ns=-4020000000 duration_ns=33333333 decoded_sequence=unavailable pts=504962720 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11245292801400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.376] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=000002F8431FB610 stream=Video payload=Frame pts=504962720 dts=504962720 duration=3000 tb=1/90000 flags=68
[2026-11-04 11:44:40.376] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=000002F8431FB610 stream=Video payload=Frame pts=504962720 dts=504962720 duration=3000 tb=1/90000 flags=68
[2026-11-04 11:44:40.376] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-11-04 11:44:40.377] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-11-04 11:44:40.377] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=accepted source_identity=composition-r382.source.2.input.video identity_link=direct generation=1 source_sequence=17 presentation_ns=-4026000000 duration_ns=33333333 decoded_sequence=unavailable pts=3170274067 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11245294269000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.378] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=3170274067 format=cuda size=1280x720 buffer_identity=0xb08a00000 drm_prime=0 software_frame=0
[2026-11-04 11:44:40.378] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=resolved source_identity=composition-r382.source.2.input.video identity_link=direct generation=1 source_sequence=17 presentation_ns=-4026000000 duration_ns=33333333 decoded_sequence=unavailable pts=3170274067 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11245295162100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.378] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=published source_identity=composition-r382.source.2.input.video identity_link=direct generation=1 source_sequence=17 presentation_ns=-4026000000 duration_ns=33333333 decoded_sequence=unavailable pts=3170274067 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11245295324100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.378] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=000002F8431F9E70 stream=Video payload=Frame pts=3170274067 dts=3170274067 duration=3000 tb=1/90000 flags=68
[2026-11-04 11:44:40.378] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=000002F8431F9E70 stream=Video payload=Frame pts=3170274067 dts=3170274067 duration=3000 tb=1/90000 flags=68
[2026-11-04 11:44:40.378] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-11-04 11:44:40.378] [info] [graph][runtime.node] source_video_trace node_id=65 node=HardwareTransferNode stage=published source_identity=composition-r382.source.2.input.video identity_link=direct generation=1 source_sequence=17 presentation_ns=-4026000000 duration_ns=33333333 decoded_sequence=unavailable pts=3170274067 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11245295625400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.379] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=000002F8431F9E70 stream=Video payload=Frame pts=3170274067 dts=3170274067 duration=3000 tb=1/90000 flags=68
[2026-11-04 11:44:40.379] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=000002F8431FD980 stream=Video payload=Frame pts=3170274067 dts=3170274067 duration=3000 tb=1/90000 flags=68
[2026-11-04 11:44:40.379] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-11-04 11:44:40.379] [info] [graph][runtime.node] source_video_trace node_id=66 node=VideoFrameRateNode stage=published source_identity=composition-r382.source.2.input.video identity_link=direct generation=1 source_sequence=17 presentation_ns=-4026000000 duration_ns=33333333 decoded_sequence=unavailable pts=3170274067 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11245296080000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.380] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=000002F8431FD980 stream=Video payload=Frame pts=3170274067 dts=3170274067 duration=3000 tb=1/90000 flags=68
[2026-11-04 11:44:40.450] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=74
[2026-11-04 11:44:40.481] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=101
[2026-11-04 11:44:40.482] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=input source_identity=composition-r382.source.1.input.video identity_link=direct generation=1 source_sequence=25 presentation_ns=-4020000000 duration_ns=33333333 decoded_sequence=unavailable pts=504962720 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11245398906100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.483] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=108
[2026-11-04 11:44:40.483] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=input source_identity=composition-r382.source.2.input.video identity_link=direct generation=1 source_sequence=17 presentation_ns=-4026000000 duration_ns=33333333 decoded_sequence=unavailable pts=3170274067 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11245400197700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.483] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=input source_identity=composition-r382.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4020000000 duration_ns=33333333 decoded_sequence=unavailable pts=279151101 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11245400496200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.484] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=accepted source_identity=composition-r382.source.1.input.video identity_link=direct generation=1 source_sequence=25 presentation_ns=-4020000000 duration_ns=33333333 decoded_sequence=unavailable pts=504962720 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11245400880300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.485] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=accepted source_identity=composition-r382.source.2.input.video identity_link=direct generation=1 source_sequence=17 presentation_ns=-4026000000 duration_ns=33333333 decoded_sequence=unavailable pts=3170274067 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11245402441300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.486] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=resolved source_identity=composition-r382.source.1.input.video identity_link=direct generation=1 source_sequence=25 presentation_ns=-4020000000 duration_ns=33333333 decoded_sequence=unavailable pts=168321 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=11245402859900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.486] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=published source_identity=composition-r382.source.1.input.video identity_link=direct generation=1 source_sequence=25 presentation_ns=-4020000000 duration_ns=33333333 decoded_sequence=unavailable pts=168321 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=11245403188500 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.486] [info] [graph][runtime.node] source_video_trace node_id=44 node=VideoPreparationNode stage=input source_identity=composition-r382.source.1.input.video identity_link=direct generation=1 source_sequence=25 presentation_ns=-4020000000 duration_ns=33333333 decoded_sequence=unavailable pts=168321 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=11245403204000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.486] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb0b290000 drm_prime=0 software_frame=0
[2026-11-04 11:44:40.486] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=accepted source_identity=composition-r382.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4020000000 duration_ns=33333333 decoded_sequence=unavailable pts=279151101 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11245403460400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.486] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=resolved source_identity=composition-r382.source.2.input.video identity_link=direct generation=1 source_sequence=17 presentation_ns=-4026000000 duration_ns=33333333 decoded_sequence=unavailable pts=1056758 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=11245403511300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.487] [info] [graph][runtime.node] source_video_trace node_id=44 node=VideoPreparationNode stage=output_attempt source_identity=composition-r382.source.1.input.video identity_link=direct generation=1 source_sequence=25 presentation_ns=-4020000000 duration_ns=33333333 decoded_sequence=unavailable pts=168321 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=11245403812100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.488] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=published source_identity=composition-r382.source.2.input.video identity_link=direct generation=1 source_sequence=17 presentation_ns=-4026000000 duration_ns=33333333 decoded_sequence=unavailable pts=1056758 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=11245404764500 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.488] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=input source_identity=composition-r382.source.2.input.video identity_link=direct generation=1 source_sequence=17 presentation_ns=-4026000000 duration_ns=33333333 decoded_sequence=unavailable pts=1056758 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=11245404854700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.488] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=15 audio_count=0 first_video_sequence=25
[2026-11-04 11:44:40.488] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=resolved source_identity=composition-r382.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4020000000 duration_ns=33333333 decoded_sequence=unavailable pts=93050 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=11245405188900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.488] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb0b320000 drm_prime=0 software_frame=0
[2026-11-04 11:44:40.488] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=output_attempt source_identity=composition-r382.source.2.input.video identity_link=direct generation=1 source_sequence=17 presentation_ns=-4026000000 duration_ns=33333333 decoded_sequence=unavailable pts=1056758 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=11245405461700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.489] [info] [graph][runtime.node] source_video_trace node_id=38 node=ActivationReleaseSequencer stage=activation source_identity=composition-r382.source.1.input.video identity_link=direct generation=1 source_sequence=25 presentation_ns=-4020000000 duration_ns=33333333 decoded_sequence=unavailable pts=504962720 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11245405789100 group=composition-r382.source.1 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-4020000000 epoch_master_release_ns=5645423468 video_count=15 min_presentation_ns=-4020000000 max_end_ns=-3520000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.489] [info] [graph][runtime.node] source_video_trace node_id=44 node=VideoPreparationNode stage=published source_identity=unavailable identity_link=node_generation_input_source_sequence generation=1 source_sequence=25 presentation_ns=-4020000000 duration_ns=33333333 decoded_sequence=unavailable pts=168321 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=11245405810100 group=composition-r382.source.1 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.489] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=published source_identity=composition-r382.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4020000000 duration_ns=33333333 decoded_sequence=unavailable pts=93050 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=11245406087500 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.489] [info] [graph][runtime.node] source_video_trace node_id=25 node=VideoPreparationNode stage=input source_identity=composition-r382.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4020000000 duration_ns=33333333 decoded_sequence=unavailable pts=93050 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=11245406117900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.489] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=15 audio_count=37 first_video_sequence=17
[2026-11-04 11:44:40.490] [info] [graph][runtime.node] source_video_trace node_id=38 node=ActivationReleaseSequencer stage=release_published source_identity=composition-r382.source.1.input.video identity_link=direct generation=1 source_sequence=25 presentation_ns=-4020000000 duration_ns=33333333 decoded_sequence=unavailable pts=504962720 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11245406831700 group=composition-r382.source.1 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-4020000000 epoch_master_release_ns=5645423468 video_count=15 min_presentation_ns=-4020000000 max_end_ns=-3520000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.490] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_candidate source_identity=composition-r382.source.1.input.video generation=1 steady_ns=11245407016100 source_sequence=25 presentation_ns=-4020000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-04 11:44:40.490] [info] [graph][runtime.node] source_video_trace node_id=39 node=MediaAvBoundReleaseExtractorNode stage=release_committed source_identity=composition-r382.source.1.input.video identity_link=direct generation=1 source_sequence=25 presentation_ns=-4020000000 duration_ns=33333333 decoded_sequence=unavailable pts=504962720 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11245407027100 group=composition-r382.source.1 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-4020000000 epoch_master_release_ns=5435422000 video_count=15 min_presentation_ns=-4020000000 max_end_ns=-3520000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=17 release_committed_count=1 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.490] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb0b200000 drm_prime=0 software_frame=0
[2026-11-04 11:44:40.490] [info] [graph][runtime.node] source_video_trace node_id=25 node=VideoPreparationNode stage=output_attempt source_identity=composition-r382.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4020000000 duration_ns=33333333 decoded_sequence=unavailable pts=93050 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=11245407402700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.490] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=published source_identity=unavailable identity_link=node_generation_input_source_sequence generation=1 source_sequence=17 presentation_ns=-4026000000 duration_ns=33333333 decoded_sequence=unavailable pts=1056758 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=11245407777400 group=composition-r382.source.2 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.491] [info] [graph][runtime.node] source_video_trace node_id=61 node=ActivationReleaseSequencer stage=activation source_identity=composition-r382.source.2.input.video identity_link=direct generation=1 source_sequence=17 presentation_ns=-4026000000 duration_ns=33333333 decoded_sequence=unavailable pts=3170274067 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11245407761900 group=composition-r382.source.2 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=-4026000000 epoch_master_release_ns=5647444268 video_count=15 min_presentation_ns=-4026000000 max_end_ns=-3526000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.491] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=0
[2026-11-04 11:44:40.491] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=15 audio_count=0 first_video_sequence=15
[2026-11-04 11:44:40.492] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_candidate source_identity=composition-r382.source.2.input.video generation=1 steady_ns=11245408822700 source_sequence=17 presentation_ns=-4026000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-04 11:44:40.492] [info] [graph][runtime.node] source_video_trace node_id=61 node=ActivationReleaseSequencer stage=release_published source_identity=composition-r382.source.2.input.video identity_link=direct generation=1 source_sequence=17 presentation_ns=-4026000000 duration_ns=33333333 decoded_sequence=unavailable pts=3170274067 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11245408828200 group=composition-r382.source.2 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=-4026000000 epoch_master_release_ns=5647444268 video_count=15 min_presentation_ns=-4026000000 max_end_ns=-3526000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.492] [info] [graph][runtime.node] audio_decode_trace stage=first_packet
[2026-11-04 11:44:40.492] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=release_committed source_identity=composition-r382.source.2.input.video identity_link=direct generation=1 source_sequence=17 presentation_ns=-4026000000 duration_ns=33333333 decoded_sequence=unavailable pts=3170274067 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11245408847700 group=composition-r382.source.2 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=-4026000000 epoch_master_release_ns=5434872100 video_count=15 min_presentation_ns=-4026000000 max_end_ns=-3526000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=17 release_committed_count=1 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.493] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=37
[2026-11-04 11:44:40.492] [info] [graph][runtime.node] source_video_trace node_id=25 node=VideoPreparationNode stage=published source_identity=unavailable identity_link=node_generation_input_source_sequence generation=1 source_sequence=15 presentation_ns=-4020000000 duration_ns=33333333 decoded_sequence=unavailable pts=93050 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=11245409331500 group=composition-r382.source.0 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.493] [info] [graph][runtime.node] audio_decode_trace stage=first_frame samples=1024 rate=44100
[2026-11-04 11:44:40.493] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1 source_master_ns=5652900051 raw_phase_ns=13607 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-11-04 11:44:40.493] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3 source_master_ns=5699339960 raw_phase_ns=13606 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=45346
[2026-11-04 11:44:40.492] [info] [graph][runtime.node] source_video_trace node_id=19 node=ActivationReleaseSequencer stage=activation source_identity=composition-r382.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4020000000 duration_ns=33333333 decoded_sequence=unavailable pts=279151101 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11245409285300 group=composition-r382.source.0 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-4020000000 epoch_master_release_ns=5649009268 video_count=15 min_presentation_ns=-4020000000 max_end_ns=-3520000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.494] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_candidate source_identity=composition-r382.source.0.input.video generation=1 steady_ns=11245411139300 source_sequence=15 presentation_ns=-4020000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-04 11:44:40.494] [info] [graph][runtime.node] audio_encode_trace stage=first_frame
[2026-11-04 11:44:40.494] [info] [graph][runtime.node] output_preparation stage=transport_activate generation=1 elapsed_ns=238800 planned_bytes=180767114966 planned_items=1 event=scope_exit
[2026-11-04 11:44:40.495] [info] [graph][runtime.node] output_preparation stage=global_sequence generation=1 elapsed_ns=31300 planned_bytes=68968 planned_items=2154 event=scope_exit
[2026-11-04 11:44:40.494] [info] [graph][runtime.node] source_video_trace node_id=20 node=MediaAvBoundReleaseExtractorNode stage=release_committed source_identity=composition-r382.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4020000000 duration_ns=33333333 decoded_sequence=unavailable pts=279151101 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11245411148000 group=composition-r382.source.0 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-4020000000 epoch_master_release_ns=5434177100 video_count=15 min_presentation_ns=-4020000000 max_end_ns=-3520000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=17 release_committed_count=1 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.495] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=0
[2026-11-04 11:44:40.495] [info] [graph][runtime.node] output_preparation stage=transport_buffer generation=1 elapsed_ns=530000 planned_bytes=180767114966 planned_items=1 event=scope_exit
[2026-11-04 11:44:40.494] [info] [graph][runtime.node] source_video_trace node_id=19 node=ActivationReleaseSequencer stage=release_published source_identity=composition-r382.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4020000000 duration_ns=33333333 decoded_sequence=unavailable pts=279151101 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11245411143300 group=composition-r382.source.0 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-4020000000 epoch_master_release_ns=5649009268 video_count=15 min_presentation_ns=-4020000000 max_end_ns=-3520000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:44:40.495] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-r382.source.0.input.video generation=1 steady_ns=11245412549700 source_sequence=15 presentation_ns=-4020000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-04 11:44:40.495] [info] [graph][runtime.node] video_encode.input_contract format=cuda size=1280x720 buffer_identity=0xb0a000000 drm_prime=0 software_frame=0
[2026-11-04 11:44:40.495] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-r382.source.1.input.video generation=1 steady_ns=11245412781800 source_sequence=25 presentation_ns=-4020000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-04 11:44:40.496] [info] [graph][runtime.node] video_encode.generation_start_key_frame pts=0 generation=1
[2026-11-04 11:44:40.496] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-r382.source.2.input.video generation=1 steady_ns=11245413007700 source_sequence=17 presentation_ns=-4026000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-04 11:44:40.496] [info] [graph][runtime.node] video_encode.first_frame pts=0 generation=attached
[2026-11-04 11:44:40.496] [info] [graph][runtime.node] output_preparation stage=ts_packetizer generation=1 elapsed_ns=61900 planned_bytes=180765122912 planned_items=61257 event=scope_exit
[2026-11-04 11:44:40.496] [info] [graph][runtime.node] output_preparation stage=ts_mux generation=1 elapsed_ns=188900 planned_bytes=945416 planned_items=59086 event=scope_exit
[2026-11-04 11:44:40.496] [info] [graph][runtime.node] mpegts_emission_plan generation=1 mode=canonical_access_unit_window access_unit_window_ns=100000000 video_initial_service_window_ns=13990650 audio_initial_service_window_ns=13990650 packet_size_bytes=188 maximum_payload_bytes=1316 per_datagram_overhead_bytes=0
[2026-11-04 11:44:40.501] [info] [graph][runtime.node] video_encode.first_submit result=accepted
[2026-11-04 11:44:40.501] [info] [graph][runtime.node] video_encode.first_packet pts=0 dts=0 duration=0 size=108357 side_data_elems=1 received_footprint=108365 side_data[0]=Quality stats:8
[2026-11-04 11:44:40.502] [info] [graph][runtime.node] video_encode.first_packet_footprint bytes=108365 wrapped_size=108357 wrapped_side_data_elems=1
[2026-11-04 11:44:40.502] [info] [graph][runtime.node] av_scheduler_trace stage=first_video_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=5651155768 dispatch_master_ns=5651155768 now_ns=5492314600 node=74 identity_kind=output identity="composition-r382.output" canonical_sequence=1 canonical_created_steady_ns=11245419131800 head_observed_steady_ns=11245419201800 codec_receive_begin_steady_ns=11245418733800 codec_receive_end_steady_ns=11245418734600
[2026-11-04 11:44:40.502] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=5492495100 present_generation=1 present_canonical_target_ns=0 present_canonical_dispatch_ns=0 present_target_master_ns=5651155768 present_dispatch_master_ns=5651155768
[2026-11-04 11:44:40.543] [info] [graph][runtime.node] audio_encode_trace stage=first_packet
[2026-11-04 11:44:40.543] [info] [graph][runtime.node] av_scheduler_trace stage=first_audio_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=5651155768 dispatch_master_ns=5651155768 now_ns=5533687600 node=74 identity_kind=output identity="composition-r382.output" canonical_sequence=1 canonical_created_steady_ns=11245460539300 head_observed_steady_ns=11245460574800 codec_receive_begin_steady_ns=unavailable codec_receive_end_steady_ns=unavailable
[2026-11-04 11:44:40.685] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=running detail="" description=""
[2026-11-04 11:44:41.122] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=48 source_master_ns=6744237919 raw_phase_ns=13606 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=90692
[2026-11-04 11:44:42.144] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=92 source_master_ns=7765915924 raw_phase_ns=13607 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=136038
[2026-11-04 11:44:43.165] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=136 source_master_ns=8787593928 raw_phase_ns=13606 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=181384
[2026-11-04 11:44:44.187] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=180 source_master_ns=9809271932 raw_phase_ns=13606 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=226730
[2026-11-04 11:44:45.028] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1368956446 ntp_seconds=4002752685 ntp_fraction=120259084 arrival_ns=11249945014000
[2026-11-04 11:44:45.028] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1368956446
[2026-11-04 11:44:45.028] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:44:45.044] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2296715498 ntp_seconds=4002752685 ntp_fraction=188978561 arrival_ns=11249960873400
[2026-11-04 11:44:45.044] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2296715498
[2026-11-04 11:44:45.044] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:44:45.044] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2003518237 ntp_seconds=4002752685 ntp_fraction=188978561 arrival_ns=11249961410300
[2026-11-04 11:44:45.044] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2003518237
[2026-11-04 11:44:45.045] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:44:45.045] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1029797657 ntp_seconds=4002752685 ntp_fraction=188978561 arrival_ns=11249961743600
[2026-11-04 11:44:45.045] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1029797657
[2026-11-04 11:44:45.045] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:44:45.232] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=225 source_master_ns=10854169891 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=272076
[2026-11-04 11:44:46.253] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=269 source_master_ns=11875847896 raw_phase_ns=13606 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=317422
[2026-11-04 11:44:47.275] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=313 source_master_ns=12897525900 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=362768
[2026-11-04 11:44:48.320] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=358 source_master_ns=13942423859 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=408114
[2026-11-04 11:44:49.341] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=402 source_master_ns=14964101864 raw_phase_ns=13606 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=453460
[2026-11-04 11:44:50.054] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2296715498 ntp_seconds=4002752690 ntp_fraction=231928233 arrival_ns=11254971112800
[2026-11-04 11:44:50.054] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2296715498
[2026-11-04 11:44:50.055] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:44:50.055] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2003518237 ntp_seconds=4002752690 ntp_fraction=236223201 arrival_ns=11254972122300
[2026-11-04 11:44:50.055] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2003518237
[2026-11-04 11:44:50.055] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:44:50.056] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1368956446 ntp_seconds=4002752690 ntp_fraction=240518168 arrival_ns=11254973465900
[2026-11-04 11:44:50.057] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1368956446
[2026-11-04 11:44:50.057] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:44:50.085] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1029797657 ntp_seconds=4002752690 ntp_fraction=360777252 arrival_ns=11255001926500
[2026-11-04 11:44:50.085] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1029797657
[2026-11-04 11:44:50.085] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:44:50.363] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=446 source_master_ns=15985779868 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=498806
[2026-11-04 11:44:51.385] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=490 source_master_ns=17007457873 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=544152
[2026-11-04 11:44:52.430] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=535 source_master_ns=18052355832 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=589498
[2026-11-04 11:44:53.452] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=579 source_master_ns=19074033837 raw_phase_ns=13606 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=634844
[2026-11-04 11:44:54.474] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=623 source_master_ns=20095711841 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=680190
[2026-11-04 11:44:55.070] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1368956446 ntp_seconds=4002752695 ntp_fraction=300647710 arrival_ns=11259986862600
[2026-11-04 11:44:55.070] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1368956446
[2026-11-04 11:44:55.070] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:44:55.086] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2003518237 ntp_seconds=4002752695 ntp_fraction=365072220 arrival_ns=11260002777200
[2026-11-04 11:44:55.086] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2003518237
[2026-11-04 11:44:55.086] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:44:55.086] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2296715498 ntp_seconds=4002752695 ntp_fraction=369367187 arrival_ns=11260002920300
[2026-11-04 11:44:55.086] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2296715498
[2026-11-04 11:44:55.086] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:44:55.116] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1029797657 ntp_seconds=4002752695 ntp_fraction=498216206 arrival_ns=11260033378300
[2026-11-04 11:44:55.117] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1029797657
[2026-11-04 11:44:55.117] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:44:55.518] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=668 source_master_ns=21140609800 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=725536
[2026-11-04 11:44:56.539] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=712 source_master_ns=22162287805 raw_phase_ns=13606 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=770882
[2026-11-04 11:44:57.561] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=756 source_master_ns=23183965809 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=816228
[2026-11-04 11:44:58.583] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=800 source_master_ns=24205643814 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=861574
[2026-11-04 11:44:59.628] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=845 source_master_ns=25250541773 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=906920
[2026-11-04 11:45:00.104] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1368956446 ntp_seconds=4002752700 ntp_fraction=446676598 arrival_ns=11265020907600
[2026-11-04 11:45:00.104] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1368956446
[2026-11-04 11:45:00.105] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:00.105] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2003518237 ntp_seconds=4002752700 ntp_fraction=446676598 arrival_ns=11265021017500
[2026-11-04 11:45:00.105] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2003518237
[2026-11-04 11:45:00.104] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2296715498 ntp_seconds=4002752700 ntp_fraction=446676598 arrival_ns=11265021346000
[2026-11-04 11:45:00.105] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:00.105] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2296715498
[2026-11-04 11:45:00.106] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:00.148] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1029797657 ntp_seconds=4002752700 ntp_fraction=635655159 arrival_ns=11265065515800
[2026-11-04 11:45:00.149] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1029797657
[2026-11-04 11:45:00.149] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:00.650] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=889 source_master_ns=26272219778 raw_phase_ns=13606 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=952266
[2026-11-04 11:45:01.671] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=933 source_master_ns=27293897782 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=997612
[2026-11-04 11:45:02.693] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=977 source_master_ns=28315575787 raw_phase_ns=13606 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1042958
[2026-11-04 11:45:03.738] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1022 source_master_ns=29360473746 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1088304
[2026-11-04 11:45:04.760] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1066 source_master_ns=30382151750 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1133650
[2026-11-04 11:45:05.118] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1368956446 ntp_seconds=4002752705 ntp_fraction=502511173 arrival_ns=11270034733600
[2026-11-04 11:45:05.118] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2296715498 ntp_seconds=4002752705 ntp_fraction=502511173 arrival_ns=11270034716800
[2026-11-04 11:45:05.118] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2296715498
[2026-11-04 11:45:05.118] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1368956446
[2026-11-04 11:45:05.119] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:05.119] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:05.120] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2003518237 ntp_seconds=4002752705 ntp_fraction=502511173 arrival_ns=11270034729700
[2026-11-04 11:45:05.120] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2003518237
[2026-11-04 11:45:05.120] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:05.179] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1029797657 ntp_seconds=4002752705 ntp_fraction=768799145 arrival_ns=11270096197600
[2026-11-04 11:45:05.180] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1029797657
[2026-11-04 11:45:05.180] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:05.781] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1110 source_master_ns=31403829755 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1178996
[2026-11-04 11:45:06.014] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=31004455800 present_generation=1 present_canonical_target_ns=25472290249 present_canonical_dispatch_ns=25472290249 present_target_master_ns=31123446017 present_dispatch_master_ns=31123446017
[2026-11-04 11:45:06.017] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=31007534700 present_generation=1 present_canonical_target_ns=25500000000 present_canonical_dispatch_ns=25500000000 present_target_master_ns=31151155768 present_dispatch_master_ns=31151155768
[2026-11-04 11:45:06.826] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1155 source_master_ns=32448727714 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1224342
[2026-11-04 11:45:07.848] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1199 source_master_ns=33470405719 raw_phase_ns=13606 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1269688
[2026-11-04 11:45:08.869] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1243 source_master_ns=34492083723 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1315034
[2026-11-04 11:45:09.893] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1287 source_master_ns=35513761728 raw_phase_ns=13606 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1360380
[2026-11-04 11:45:10.140] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1368956446 ntp_seconds=4002752710 ntp_fraction=601295421 arrival_ns=11275057585300
[2026-11-04 11:45:10.141] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1368956446
[2026-11-04 11:45:10.141] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:10.156] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2003518237 ntp_seconds=4002752710 ntp_fraction=665719930 arrival_ns=11275072818400
[2026-11-04 11:45:10.156] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2003518237
[2026-11-04 11:45:10.156] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:10.156] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2296715498 ntp_seconds=4002752710 ntp_fraction=665719930 arrival_ns=11275072841700
[2026-11-04 11:45:10.157] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2296715498
[2026-11-04 11:45:10.157] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:10.218] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1029797657 ntp_seconds=4002752710 ntp_fraction=932007903 arrival_ns=11275134549900
[2026-11-04 11:45:10.218] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1029797657
[2026-11-04 11:45:10.218] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:10.936] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1332 source_master_ns=36558659687 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1405726
[2026-11-04 11:45:11.958] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1376 source_master_ns=37580337692 raw_phase_ns=13606 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1451072
[2026-11-04 11:45:12.979] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1420 source_master_ns=38602015696 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1496418
[2026-11-04 11:45:14.025] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1465 source_master_ns=39646913655 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1541764
[2026-11-04 11:45:15.053] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1509 source_master_ns=40668591660 raw_phase_ns=13606 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1587110
[2026-11-04 11:45:15.154] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1368956446 ntp_seconds=4002752715 ntp_fraction=661424963 arrival_ns=11280071612900
[2026-11-04 11:45:15.155] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1368956446
[2026-11-04 11:45:15.155] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:15.185] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2003518237 ntp_seconds=4002752715 ntp_fraction=790273982 arrival_ns=11280101724900
[2026-11-04 11:45:15.185] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2003518237
[2026-11-04 11:45:15.185] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:15.185] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2296715498 ntp_seconds=4002752715 ntp_fraction=794568949 arrival_ns=11280101930700
[2026-11-04 11:45:15.185] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2296715498
[2026-11-04 11:45:15.186] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:15.247] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1029797657 ntp_seconds=4002752715 ntp_fraction=1056561954 arrival_ns=11280163484400
[2026-11-04 11:45:15.247] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1029797657
[2026-11-04 11:45:15.247] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:16.068] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1553 source_master_ns=41690269664 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1632456
[2026-11-04 11:45:17.089] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1597 source_master_ns=42711947669 raw_phase_ns=13606 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1677802
[2026-11-04 11:45:18.134] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1642 source_master_ns=43756845628 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1723148
[2026-11-04 11:45:19.156] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1686 source_master_ns=44778523633 raw_phase_ns=13606 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1768494
[2026-11-04 11:45:20.165] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1368956446 ntp_seconds=4002752720 ntp_fraction=708669603 arrival_ns=11285081921200
[2026-11-04 11:45:20.165] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1368956446
[2026-11-04 11:45:20.165] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:20.178] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1730 source_master_ns=45800201637 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1813840
[2026-11-04 11:45:20.227] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2296715498 ntp_seconds=4002752720 ntp_fraction=970662608 arrival_ns=11285143344800
[2026-11-04 11:45:20.227] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2296715498
[2026-11-04 11:45:20.227] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:20.228] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2003518237 ntp_seconds=4002752720 ntp_fraction=979252543 arrival_ns=11285144940100
[2026-11-04 11:45:20.228] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2003518237
[2026-11-04 11:45:20.228] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:20.289] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1029797657 ntp_seconds=4002752720 ntp_fraction=1236950581 arrival_ns=11285205761000
[2026-11-04 11:45:20.290] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1029797657
[2026-11-04 11:45:20.290] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:21.222] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1775 source_master_ns=46845099596 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1859186
[2026-11-04 11:45:22.244] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1819 source_master_ns=47866777601 raw_phase_ns=13606 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1904532
[2026-11-04 11:45:23.265] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1863 source_master_ns=48888455605 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1949878
[2026-11-04 11:45:24.288] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1907 source_master_ns=49910133610 raw_phase_ns=13606 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1995224
[2026-11-04 11:45:25.175] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1368956446 ntp_seconds=4002752725 ntp_fraction=751619276 arrival_ns=11290092399300
[2026-11-04 11:45:25.176] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1368956446
[2026-11-04 11:45:25.176] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:25.252] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2296715498 ntp_seconds=4002752725 ntp_fraction=1082331758 arrival_ns=11290169443500
[2026-11-04 11:45:25.252] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2296715498
[2026-11-04 11:45:25.252] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2003518237 ntp_seconds=4002752725 ntp_fraction=1082331758 arrival_ns=11290169679800
[2026-11-04 11:45:25.253] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2003518237
[2026-11-04 11:45:25.253] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:25.253] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:25.314] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1029797657 ntp_seconds=4002752725 ntp_fraction=1348619730 arrival_ns=11290231432800
[2026-11-04 11:45:25.315] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1029797657
[2026-11-04 11:45:25.315] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:25.332] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1952 source_master_ns=50955031569 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2040570
[2026-11-04 11:45:26.354] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1996 source_master_ns=51976709574 raw_phase_ns=13606 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2085916
[2026-11-04 11:45:26.845] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=51835250000 present_generation=1 present_canonical_target_ns=46300589569 present_canonical_dispatch_ns=46300589569 present_target_master_ns=51951745337 present_dispatch_master_ns=51951745337
[2026-11-04 11:45:26.851] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=51841593400 present_generation=1 present_canonical_target_ns=46333333333 present_canonical_dispatch_ns=46333333333 present_target_master_ns=51984489101 present_dispatch_master_ns=51984489101
[2026-11-04 11:45:27.376] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2040 source_master_ns=52998387578 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2131262
[2026-11-04 11:45:28.421] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2085 source_master_ns=54043285537 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2176608
[2026-11-04 11:45:29.443] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2129 source_master_ns=55064963542 raw_phase_ns=13606 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2221954
[2026-11-04 11:45:30.201] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1368956446 ntp_seconds=4002752730 ntp_fraction=863288426 arrival_ns=11295118153300
[2026-11-04 11:45:30.201] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1368956446
[2026-11-04 11:45:30.201] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:30.278] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2296715498 ntp_seconds=4002752730 ntp_fraction=1194000908 arrival_ns=11295195264700
[2026-11-04 11:45:30.278] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2296715498
[2026-11-04 11:45:30.279] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:30.279] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2003518237 ntp_seconds=4002752730 ntp_fraction=1198295875 arrival_ns=11295196118100
[2026-11-04 11:45:30.279] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2003518237
[2026-11-04 11:45:30.280] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:30.342] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1029797657 ntp_seconds=4002752730 ntp_fraction=1464583847 arrival_ns=11295258764500
[2026-11-04 11:45:30.342] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1029797657
[2026-11-04 11:45:30.342] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:30.464] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2173 source_master_ns=56086641546 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2267300
[2026-11-04 11:45:31.488] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2217 source_master_ns=57108319551 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2312646
[2026-11-04 11:45:32.531] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2262 source_master_ns=58153217510 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2357992
[2026-11-04 11:45:33.552] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2306 source_master_ns=59174895515 raw_phase_ns=13606 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2403338
[2026-11-04 11:45:34.574] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2350 source_master_ns=60196573519 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2448684
[2026-11-04 11:45:35.218] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1368956446 ntp_seconds=4002752735 ntp_fraction=932007903 arrival_ns=11300134802600
[2026-11-04 11:45:35.218] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1368956446
[2026-11-04 11:45:35.218] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:35.280] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2296715498 ntp_seconds=4002752735 ntp_fraction=1198295875 arrival_ns=11300196702200
[2026-11-04 11:45:35.280] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2296715498
[2026-11-04 11:45:35.280] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:35.311] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2003518237 ntp_seconds=4002752735 ntp_fraction=1335734829 arrival_ns=11300228128900
[2026-11-04 11:45:35.311] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2003518237
[2026-11-04 11:45:35.312] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:35.373] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1029797657 ntp_seconds=4002752735 ntp_fraction=1602022801 arrival_ns=11300290379100
[2026-11-04 11:45:35.374] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1029797657
[2026-11-04 11:45:35.374] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:35.620] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2395 source_master_ns=61241471478 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2494030
[2026-11-04 11:45:36.641] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2439 source_master_ns=62263149483 raw_phase_ns=13606 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2539376
[2026-11-04 11:45:37.662] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2483 source_master_ns=63284827487 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2584722
[2026-11-04 11:45:38.686] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2527 source_master_ns=64306505492 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2630068
[2026-11-04 11:45:39.729] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2572 source_master_ns=65351403451 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2675414
[2026-11-04 11:45:40.224] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1368956446 ntp_seconds=4002752740 ntp_fraction=962072674 arrival_ns=11305141307300
[CLI] [2026-11-04 11:45:40.224] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1368956446
runtime report: queued=617, peakQueued=886, threads=81, processThreads=129, workers=75, workerProgress=165417, workerProcessCalls=648509, workerWaits=483083, workerWakeups=478467, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=188, logicalProcessors=22, averageSystemMachineCpuPercent=9.417869, averageProcessMachineCpuPercent=1.925181, peakProcessMachineCpuPercent=7.484407, averageProcessSingleCoreCpuPercent=42.353986, peakProcessSingleCoreCpuPercent=164.656965, initialWorkingSetBytes=211783680, workingSetBytes=265949184, peakWorkingSetBytes=268066816, totalPushed=143781, totalPopped=143164, droppedBuffers=0, encodedPacketsPushed=20020, encodedPacketsPopped=20019, backpressureItems=123, graphPayloadCurrentBytes=10942075, graphPayloadHighWaterBytes=15160246, graphPayloadCurrentObjects=732, graphPayloadHighWaterObjects=737, graphPayloadReservations=56750, graphPayloadReleases=56018, graphPayloadPressureFailures=0, lastInputReceivedAtNs=11305141405500[2026-11-04 11:45:40.225] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1

[2026-11-04 11:45:40.285] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2296715498 ntp_seconds=4002752740 ntp_fraction=1224065679 arrival_ns=11305202336500
[2026-11-04 11:45:40.285] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2296715498
[2026-11-04 11:45:40.286] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:40.315] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2003518237 ntp_seconds=4002752740 ntp_fraction=1352914698 arrival_ns=11305232546400
[2026-11-04 11:45:40.316] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2003518237
[2026-11-04 11:45:40.316] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:40.378] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1029797657 ntp_seconds=4002752740 ntp_fraction=1619202670 arrival_ns=11305294499600
[2026-11-04 11:45:40.378] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1029797657
[2026-11-04 11:45:40.378] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:40.750] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2616 source_master_ns=66373081456 raw_phase_ns=13606 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2720760
[2026-11-04 11:45:41.772] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2660 source_master_ns=67394759460 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2766106
[2026-11-04 11:45:42.817] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2705 source_master_ns=68439657419 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2811452
[2026-11-04 11:45:43.839] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2749 source_master_ns=69461335424 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2856798
[2026-11-04 11:45:44.861] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2793 source_master_ns=70483013428 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2902144
[2026-11-04 11:45:45.238] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1368956446 ntp_seconds=4002752745 ntp_fraction=1022202216 arrival_ns=11310155077100
[2026-11-04 11:45:45.238] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1368956446
[2026-11-04 11:45:45.239] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:45.315] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2296715498 ntp_seconds=4002752745 ntp_fraction=1348619730 arrival_ns=11310231667600
[2026-11-04 11:45:45.316] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2296715498
[2026-11-04 11:45:45.316] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:45.346] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2003518237 ntp_seconds=4002752745 ntp_fraction=1481763717 arrival_ns=11310262628800
[2026-11-04 11:45:45.346] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2003518237
[2026-11-04 11:45:45.346] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:45.407] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1029797657 ntp_seconds=4002752745 ntp_fraction=1748051689 arrival_ns=11310324014500
[2026-11-04 11:45:45.407] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1029797657
[2026-11-04 11:45:45.408] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:45.882] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2837 source_master_ns=71504691433 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2947490
[2026-11-04 11:45:46.927] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2882 source_master_ns=72549589392 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2992836
[2026-11-04 11:45:47.949] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2926 source_master_ns=73571267397 raw_phase_ns=13606 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3038182
[2026-11-04 11:45:48.970] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2970 source_master_ns=74592945401 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3083528
[2026-11-04 11:45:50.022] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3015 source_master_ns=75637843360 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3128874
[2026-11-04 11:45:50.261] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1368956446 ntp_seconds=4002752750 ntp_fraction=1120986464 arrival_ns=11315178587800
[2026-11-04 11:45:50.262] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1368956446
[2026-11-04 11:45:50.262] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:50.355] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2003518237 ntp_seconds=4002752750 ntp_fraction=1524713390 arrival_ns=11315271891300
[2026-11-04 11:45:50.355] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2296715498 ntp_seconds=4002752750 ntp_fraction=1524713390 arrival_ns=11315272025400
[2026-11-04 11:45:50.355] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2003518237
[2026-11-04 11:45:50.356] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2296715498
[2026-11-04 11:45:50.356] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:50.356] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:50.417] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1029797657 ntp_seconds=4002752750 ntp_fraction=1791001362 arrival_ns=11315334390600
[2026-11-04 11:45:50.418] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1029797657
[2026-11-04 11:45:50.418] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:51.037] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3059 source_master_ns=76659521365 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3174220
[2026-11-04 11:45:52.058] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3103 source_master_ns=77681199370 raw_phase_ns=13606 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3219566
[2026-11-04 11:45:53.080] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3147 source_master_ns=78702877374 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3264912
[2026-11-04 11:45:54.125] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3192 source_master_ns=79747775333 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3310258
[2026-11-04 11:45:55.147] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3236 source_master_ns=80769453338 raw_phase_ns=13606 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3355604
[2026-11-04 11:45:55.279] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1368956446 ntp_seconds=4002752755 ntp_fraction=1198295875 arrival_ns=11320195927900
[2026-11-04 11:45:55.279] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1368956446
[2026-11-04 11:45:55.280] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:55.357] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2296715498 ntp_seconds=4002752755 ntp_fraction=1529008357 arrival_ns=11320273567100
[2026-11-04 11:45:55.357] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2296715498
[2026-11-04 11:45:55.357] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:55.357] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2003518237 ntp_seconds=4002752755 ntp_fraction=1529008357 arrival_ns=11320273528400
[2026-11-04 11:45:55.357] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2003518237
[2026-11-04 11:45:55.357] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:55.449] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1029797657 ntp_seconds=4002752755 ntp_fraction=1928440315 arrival_ns=11320366068800
[2026-11-04 11:45:55.449] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1029797657
[2026-11-04 11:45:55.449] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:45:56.168] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3280 source_master_ns=81791131342 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3400950
[2026-11-04 11:45:57.215] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3325 source_master_ns=82836029302 raw_phase_ns=13606 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3446296
[2026-11-04 11:45:58.235] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3369 source_master_ns=83857707306 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3491642
[2026-11-04 11:45:59.257] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3413 source_master_ns=84879385311 raw_phase_ns=13606 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3536988
[2026-11-04 11:46:00.278] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3457 source_master_ns=85901063315 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3582334
[2026-11-04 11:46:00.293] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1368956446 ntp_seconds=4002752760 ntp_fraction=1254130450 arrival_ns=11325209818800
[2026-11-04 11:46:00.293] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1368956446
[2026-11-04 11:46:00.293] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:46:00.386] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2003518237 ntp_seconds=4002752760 ntp_fraction=1657857376 arrival_ns=11325303655400
[2026-11-04 11:46:00.387] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2296715498 ntp_seconds=4002752760 ntp_fraction=1657857376 arrival_ns=11325303530100
[2026-11-04 11:46:00.387] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2003518237
[2026-11-04 11:46:00.387] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:46:00.387] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2296715498
[2026-11-04 11:46:00.387] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:46:00.479] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1029797657 ntp_seconds=4002752760 ntp_fraction=2057289334 arrival_ns=11325396240500
[2026-11-04 11:46:00.480] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1029797657
[2026-11-04 11:46:00.480] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:46:01.323] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3502 source_master_ns=86945961274 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3627680
[2026-11-04 11:46:02.345] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3546 source_master_ns=87967639279 raw_phase_ns=13606 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3673026
[2026-11-04 11:46:03.367] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3590 source_master_ns=88989317283 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3718372
[2026-11-04 11:46:04.389] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3634 source_master_ns=90010995288 raw_phase_ns=13606 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3763718
[2026-11-04 11:46:05.301] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1368956446 ntp_seconds=4002752765 ntp_fraction=1292785156 arrival_ns=11330218508300
[2026-11-04 11:46:05.302] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1368956446
[2026-11-04 11:46:05.302] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:46:05.410] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2003518237 ntp_seconds=4002752765 ntp_fraction=1760936591 arrival_ns=11330327426800
[2026-11-04 11:46:05.411] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2003518237
[2026-11-04 11:46:05.410] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2296715498 ntp_seconds=4002752765 ntp_fraction=1760936591 arrival_ns=11330327474300
[2026-11-04 11:46:05.411] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:46:05.411] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2296715498
[2026-11-04 11:46:05.412] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:46:05.433] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3679 source_master_ns=91055893247 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3809064
[2026-11-04 11:46:05.505] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1029797657 ntp_seconds=4002752765 ntp_fraction=2168958484 arrival_ns=11330422182500
[2026-11-04 11:46:05.505] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1029797657
[2026-11-04 11:46:05.505] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:46:06.455] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3723 source_master_ns=92077571252 raw_phase_ns=13606 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3854410
[2026-11-04 11:46:07.477] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3767 source_master_ns=93099249256 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3899756
[2026-11-04 11:46:08.521] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3812 source_master_ns=94144147215 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3945102
[2026-11-04 11:46:09.543] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3856 source_master_ns=95165825220 raw_phase_ns=13606 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3990448
[2026-11-04 11:46:10.314] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1368956446 ntp_seconds=4002752770 ntp_fraction=1344324763 arrival_ns=11335230768300
[2026-11-04 11:46:10.314] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1368956446
[2026-11-04 11:46:10.314] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:46:10.423] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2296715498 ntp_seconds=4002752770 ntp_fraction=1812476198 arrival_ns=11335339673800
[2026-11-04 11:46:10.423] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2296715498
[2026-11-04 11:46:10.424] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:46:10.453] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2003518237 ntp_seconds=4002752770 ntp_fraction=1941325217 arrival_ns=11335369811100
[2026-11-04 11:46:10.453] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2003518237
[2026-11-04 11:46:10.454] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:46:10.515] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1029797657 ntp_seconds=4002752770 ntp_fraction=2207613190 arrival_ns=11335431813000
[2026-11-04 11:46:10.516] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1029797657
[2026-11-04 11:46:10.517] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:46:10.565] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3900 source_master_ns=96187503224 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4035794
[2026-11-04 11:46:11.587] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3944 source_master_ns=97209181229 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4081140
[2026-11-04 11:46:12.631] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3989 source_master_ns=98254079188 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4126486
[2026-11-04 11:46:13.655] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4033 source_master_ns=99275757193 raw_phase_ns=13606 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4171832
[2026-11-04 11:46:14.675] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4077 source_master_ns=100297435197 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4217178
[2026-11-04 11:46:15.318] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1368956446 ntp_seconds=4002752775 ntp_fraction=1361504632 arrival_ns=11340234536900
[2026-11-04 11:46:15.318] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1368956446
[2026-11-04 11:46:15.318] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:46:15.457] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2296715498 ntp_seconds=4002752775 ntp_fraction=1962800054 arrival_ns=11340373983700
[2026-11-04 11:46:15.457] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2296715498
[2026-11-04 11:46:15.457] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:46:15.488] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2003518237 ntp_seconds=4002752775 ntp_fraction=2091649073 arrival_ns=11340404667300
[2026-11-04 11:46:15.488] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2003518237
[2026-11-04 11:46:15.488] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:46:15.519] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1029797657 ntp_seconds=4002752775 ntp_fraction=2229088026 arrival_ns=11340436148500
[2026-11-04 11:46:15.520] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1029797657
[2026-11-04 11:46:15.520] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:46:15.720] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4122 source_master_ns=101342333156 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4262524
[2026-11-04 11:46:16.741] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4166 source_master_ns=102364011161 raw_phase_ns=13606 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4307870
[2026-11-04 11:46:17.769] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4210 source_master_ns=103385689165 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4353216
[2026-11-04 11:46:18.785] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4254 source_master_ns=104407367170 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4398562
[2026-11-04 11:46:19.830] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4299 source_master_ns=105452265129 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4443908
[2026-11-04 11:46:19.880] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=104870667200 present_generation=1 present_canonical_target_ns=99334965986 present_canonical_dispatch_ns=99334965986 present_target_master_ns=104986121754 present_dispatch_master_ns=104986121754
[2026-11-04 11:46:19.884] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=104874861700 present_generation=1 present_canonical_target_ns=99366666666 present_canonical_dispatch_ns=99366666666 present_target_master_ns=105017822434 present_dispatch_master_ns=105017822434
[2026-11-04 11:46:20.328] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1368956446 ntp_seconds=4002752780 ntp_fraction=1408749273 arrival_ns=11345245288800
[2026-11-04 11:46:20.328] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1368956446
[2026-11-04 11:46:20.329] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:46:20.486] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2296715498 ntp_seconds=4002752780 ntp_fraction=2087354105 arrival_ns=11345403289300
[2026-11-04 11:46:20.486] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2296715498
[2026-11-04 11:46:20.487] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:46:20.515] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2003518237 ntp_seconds=4002752780 ntp_fraction=2211908157 arrival_ns=11345431930700
[2026-11-04 11:46:20.515] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2003518237
[2026-11-04 11:46:20.516] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:46:20.546] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1029797657 ntp_seconds=4002752780 ntp_fraction=2345052143 arrival_ns=11345462956100
[2026-11-04 11:46:20.546] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1029797657
[2026-11-04 11:46:20.546] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:46:20.851] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4343 source_master_ns=106473943134 raw_phase_ns=13606 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4489254
[CLI] [2026-11-04 11:46:21.875] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4387 source_master_ns=107495621138 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4534600
runtime report: queued=615, peakQueued=886, threads=81, processThreads=129, workers=75, workerProgress=277065, workerProcessCalls=1099347, workerWaits=822275, workerWakeups=814496, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=320, logicalProcessors=22, averageSystemMachineCpuPercent=10.139810, averageProcessMachineCpuPercent=1.908658, peakProcessMachineCpuPercent=7.484407, averageProcessSingleCoreCpuPercent=41.990480, peakProcessSingleCoreCpuPercent=164.656965, initialWorkingSetBytes=211783680, workingSetBytes=269221888, peakWorkingSetBytes=269996032, totalPushed=240438, totalPopped=239823, droppedBuffers=0, encodedPacketsPushed=33982, encodedPacketsPopped=33981, backpressureItems=123, graphPayloadCurrentBytes=11496222, graphPayloadHighWaterBytes=15211982, graphPayloadCurrentObjects=729, graphPayloadHighWaterObjects=737, graphPayloadReservations=95384, graphPayloadReleases=94655, graphPayloadPressureFailures=0, lastInputReceivedAtNs=11346777326700
[2026-11-04 11:46:22.918] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4432 source_master_ns=108540519097 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4579946
[2026-11-04 11:46:23.939] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4476 source_master_ns=109562197102 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4625292
[2026-11-04 11:46:24.961] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4520 source_master_ns=110583875107 raw_phase_ns=13606 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4670638
[2026-11-04 11:46:25.338] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1368956446 ntp_seconds=4002752785 ntp_fraction=1451698946 arrival_ns=11350255728500
[2026-11-04 11:46:25.339] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1368956446
[2026-11-04 11:46:25.339] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:46:25.525] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2296715498 ntp_seconds=4002752785 ntp_fraction=2254857830 arrival_ns=11350441928700
[2026-11-04 11:46:25.526] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2296715498
[2026-11-04 11:46:25.526] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:46:25.527] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2003518237 ntp_seconds=4002752785 ntp_fraction=2263447764 arrival_ns=11350444487400
[2026-11-04 11:46:25.528] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2003518237
[2026-11-04 11:46:25.528] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:46:25.571] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1029797657 ntp_seconds=4002752785 ntp_fraction=2452426326 arrival_ns=11350488085600
[2026-11-04 11:46:25.571] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1029797657
[2026-11-04 11:46:25.571] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:46:25.986] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4564 source_master_ns=111605553111 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4715984
[2026-11-04 11:46:27.028] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4609 source_master_ns=112650451070 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4761330
[2026-11-04 11:46:28.049] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4653 source_master_ns=113672129075 raw_phase_ns=13606 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4806676
[2026-11-04 11:46:29.071] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4697 source_master_ns=114693807079 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4852022
[2026-11-04 11:46:30.116] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4742 source_master_ns=115738705038 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4897368
[2026-11-04 11:46:30.345] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1368956446 ntp_seconds=4002752790 ntp_fraction=1477468749 arrival_ns=11355261848000
[2026-11-04 11:46:30.345] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1368956446
[2026-11-04 11:46:30.345] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:46:30.547] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2003518237 ntp_seconds=4002752790 ntp_fraction=2349347110 arrival_ns=11355464359000
[2026-11-04 11:46:30.547] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2296715498 ntp_seconds=4002752790 ntp_fraction=2349347110 arrival_ns=11355464398900
[2026-11-04 11:46:30.548] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2003518237
[2026-11-04 11:46:30.548] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2296715498
[2026-11-04 11:46:30.548] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:46:30.548] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:46:30.578] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1029797657 ntp_seconds=4002752790 ntp_fraction=2482491097 arrival_ns=11355495347500
[2026-11-04 11:46:30.578] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1029797657
[2026-11-04 11:46:30.579] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:46:31.137] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4786 source_master_ns=116760383043 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4942714
[2026-11-04 11:46:32.159] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4830 source_master_ns=117782061048 raw_phase_ns=13606 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4988060
[2026-11-04 11:46:33.181] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4874 source_master_ns=118803739052 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5033406
[2026-11-04 11:46:34.226] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4919 source_master_ns=119848637011 raw_phase_ns=13605 filtered_phase_ns=13607 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5078752
[2026-11-04 11:46:34.482] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=11359399408200
[2026-11-04 11:46:34.483] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=3 generation=2 invalidated_generation=1 locked=0
[2026-11-04 11:46:34.482] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=11359399646200
[2026-11-04 11:46:34.483] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=30 group=composition-r382.source.1 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=23 video_raw_generation=0 video_ssrc=2296715498
[2026-11-04 11:46:34.483] [info] [graph][runtime.node] av_reacquisition group=composition-r382.source.1 phase=purging old=1 next=2 transition=1
[2026-11-04 11:46:34.483] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=3 generation=2 invalidated_generation=1 locked=0
[2026-11-04 11:46:34.483] [info] [graph][runtime.node] rtp_ingress_purge node=27 transition=1 cutoff=0
[2026-11-04 11:46:34.484] [info] [graph][runtime.node] rtp_binder_purge node=30 transition=1 cutoff=3618
[2026-11-04 11:46:34.484] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=11 group=composition-r382.source.0 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=23 video_raw_generation=0 video_ssrc=1029797657
[2026-11-04 11:46:34.484] [info] [graph][runtime.node] av_reacquisition group=composition-r382.source.0 phase=purging old=1 next=2 transition=1
[2026-11-04 11:46:34.484] [info] [graph][runtime.node] av_reacquisition group=composition-r382.source.1 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=1 next=2 transition=1
[2026-11-04 11:46:34.485] [info] [graph][runtime.node] rtp_ingress_purge node=8 transition=1 cutoff=0
[2026-11-04 11:46:34.486] [info] [graph][runtime.node] rtp_binder_purge node=11 transition=1 cutoff=3608
[2026-11-04 11:46:34.486] [info] [graph][runtime.node] av_reacquisition group=composition-r382.source.0 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=1 next=2 transition=1
[2026-11-04 11:46:34.497] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=11359414722000
[2026-11-04 11:46:34.498] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=3 generation=2 invalidated_generation=1 locked=0
[2026-11-04 11:46:34.498] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=50 group=composition-r382.source.2 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=46 video_raw_generation=0 video_ssrc=2003518237 audio_raw_generation=0 audio_ssrc=1368956446
[2026-11-04 11:46:34.498] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=51 group=composition-r382.source.2 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=46 video_raw_generation=0 video_ssrc=2003518237 audio_raw_generation=0 audio_ssrc=1368956446
[2026-11-04 11:46:34.498] [info] [graph][runtime.node] av_reacquisition group=composition-r382.source.2 phase=purging old=1 next=2 transition=1
[2026-11-04 11:46:34.499] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=11359416178100
[2026-11-04 11:46:34.499] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=3 generation=2 invalidated_generation=1 locked=0
[2026-11-04 11:46:34.502] [info] [graph][runtime.node] rtp_ingress_purge node=46 transition=1 cutoff=0
[2026-11-04 11:46:34.502] [info] [graph][runtime.node] rtp_ingress_purge node=47 transition=1 cutoff=0
[2026-11-04 11:46:34.503] [info] [graph][runtime.node] rtp_binder_purge node=50 transition=1 cutoff=3610
[2026-11-04 11:46:34.503] [info] [graph][runtime.node] rtp_binder_purge node=51 transition=1 cutoff=5170
[2026-11-04 11:46:34.504] [info] [graph][runtime.node] av_reacquisition group=composition-r382.source.2 event=purge_completed purge_ack=complete continuation=acquiring ack_count=2 old=1 next=2 transition=1
[2026-11-04 11:46:48.911] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1210056190 ntp_seconds=4002752808 ntp_fraction=3908420239 arrival_ns=11373827600400
[2026-11-04 11:46:48.911] [info] [graph][runtime.node] rtp_binder_boundary stage=deadline_established node=51 group=composition-r382.source.2 buffered=0 phase=2 preserves_output=1 raw_ssrc=2436933788 raw_generation=1 raw_sequence=5172 old_generation=1 next_generation=2 transition_sequence=1 published_generation=1
[2026-11-04 11:46:48.911] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1210056190
[2026-11-04 11:46:48.912] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=0 generation=2 invalidated_generation=1 locked=0
[2026-11-04 11:46:48.912] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2436933788 ntp_seconds=4002752808 ntp_fraction=3908420239 arrival_ns=11373827601100
[2026-11-04 11:46:48.912] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2436933788
[2026-11-04 11:46:48.912] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:46:49.446] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=1 generation=2
[2026-11-04 11:46:49.944] [info] [graph][runtime.node] source_video_trace_summary node_id=61 node=ActivationReleaseSequencer generation=1 reason=generation_change steady_ns=11374861080600 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3555 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:46:49.944] [info] [graph][runtime.node] source_video_trace node_id=61 node=ActivationReleaseSequencer stage=activation source_identity=composition-r382.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=2923235356 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11374861220400 group=composition-r382.source.2 release_identity=15755 transition_sequence=1 epoch_source_start_ns=994000000 epoch_master_release_ns=135033939900 video_count=16 min_presentation_ns=994000000 max_end_ns=1527333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:46:49.944] [info] [graph][runtime.node] source_video_trace node_id=61 node=ActivationReleaseSequencer stage=release_published source_identity=composition-r382.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=2923235356 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11374861337100 group=composition-r382.source.2 release_identity=15755 transition_sequence=1 epoch_source_start_ns=994000000 epoch_master_release_ns=135033939900 video_count=16 min_presentation_ns=994000000 max_end_ns=1527333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:46:49.944] [info] [graph][runtime.node] source_video_trace_summary node_id=62 node=MediaAvBoundReleaseExtractorNode generation=1 reason=generation_change steady_ns=11374861353100 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=3571 release_committed_count=3555 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:46:49.944] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=release source_identity=composition-r382.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=2923235356 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11374861529400 group=composition-r382.source.2 release_identity=15755 transition_sequence=1 epoch_source_start_ns=994000000 epoch_master_release_ns=135033939900 video_count=16 min_presentation_ns=994000000 max_end_ns=1527333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:46:49.944] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=release_committed source_identity=composition-r382.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=2923235356 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11374861640900 group=composition-r382.source.2 release_identity=15755 transition_sequence=1 epoch_source_start_ns=994000000 epoch_master_release_ns=135033939900 video_count=16 min_presentation_ns=994000000 max_end_ns=1527333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=1 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:46:49.944] [info] [graph][runtime.node] source_video_trace_summary node_id=64 node=VideoDecodeNode generation=1 reason=generation_change steady_ns=11374861654100 input_count=3463 accepted_count=3463 would_block_count=0 resolved_count=3463 output_attempt_count=0 published_count=3462 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=106 incomplete=0
[2026-11-04 11:46:49.944] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1 source_master_ns=135035404752 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-11-04 11:46:49.945] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=input source_identity=composition-r382.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=2923235356 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11374861873900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=106 incomplete=0
[2026-11-04 11:46:49.945] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3 source_master_ns=135081844661 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=45346
[2026-11-04 11:46:49.949] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=accepted source_identity=composition-r382.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=2923235356 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11374866710500 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=106 incomplete=0
[2026-11-04 11:46:49.950] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=resolved source_identity=composition-r382.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=2923235356 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11374866896500 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=106 incomplete=0
[2026-11-04 11:46:49.950] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=published source_identity=composition-r382.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=2923235356 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11374866996000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=106 incomplete=0
[2026-11-04 11:46:49.950] [info] [graph][runtime.node] source_video_trace_summary node_id=65 node=HardwareTransferNode generation=1 reason=generation_change steady_ns=11374867178100 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3462 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:46:49.950] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-11-04 11:46:49.950] [info] [graph][runtime.node] source_video_trace node_id=65 node=HardwareTransferNode stage=published source_identity=composition-r382.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=2923235356 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11374867297100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:46:49.950] [info] [graph][runtime.node] source_video_trace_summary node_id=66 node=VideoFrameRateNode generation=1 reason=generation_change steady_ns=11374867586200 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3451 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:46:49.950] [info] [graph][runtime.node] source_video_trace_summary node_id=68 node=VideoFilterNode generation=1 reason=generation_change steady_ns=11374867626300 input_count=3451 accepted_count=3451 would_block_count=0 resolved_count=3451 output_attempt_count=0 published_count=3451 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:46:49.951] [info] [graph][runtime.node] source_video_trace node_id=66 node=VideoFrameRateNode stage=published source_identity=composition-r382.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=2923235356 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11374867964000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:46:49.951] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=input source_identity=composition-r382.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=2923235356 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11374868172700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:46:49.951] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=accepted source_identity=composition-r382.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=2923235356 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11374868707800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:46:49.952] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=resolved source_identity=composition-r382.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=974412 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=11374869183700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:46:49.952] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=published source_identity=composition-r382.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=974412 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=11374869287900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:46:49.952] [info] [graph][runtime.node] source_video_trace_summary node_id=67 node=VideoPreparationNode generation=1 reason=generation_change steady_ns=11374869349300 input_count=3438 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3438 published_count=3434 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=13 incomplete=0
[2026-11-04 11:46:49.952] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=input source_identity=composition-r382.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=974412 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=11374869460300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=13 incomplete=0
[2026-11-04 11:46:49.952] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=output_attempt source_identity=composition-r382.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=974412 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=11374869545000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=13 incomplete=0
[2026-11-04 11:46:49.952] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=published source_identity=composition-r382.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=974412 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=11374869635000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=13 incomplete=0
[2026-11-04 11:46:49.954] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_candidate source_identity=composition-r382.source.2.input.video generation=2 steady_ns=11374871548200 source_sequence=3 presentation_ns=1060666666 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-04 11:46:49.962] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-r382.source.2.input.video generation=2 steady_ns=11374878804400 source_sequence=3 presentation_ns=1060666666 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-04 11:46:50.503] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=48 source_master_ns=136126742621 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=90692
[2026-11-04 11:46:51.525] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=92 source_master_ns=137148420625 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=136038
[2026-11-04 11:46:52.546] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=136 source_master_ns=138170098630 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=181384
[2026-11-04 11:46:53.591] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=181 source_master_ns=139214996589 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=226730
[2026-11-04 11:46:53.923] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1210056190 ntp_seconds=4002752813 ntp_fraction=3959959846 arrival_ns=11378839668500
[2026-11-04 11:46:53.923] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1210056190
[2026-11-04 11:46:53.923] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:46:53.923] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2436933788 ntp_seconds=4002752813 ntp_fraction=3959959846 arrival_ns=11378839681000
[2026-11-04 11:46:53.923] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2436933788
[2026-11-04 11:46:53.923] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:46:54.613] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=225 source_master_ns=140236674593 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=272076
[2026-11-04 11:46:55.635] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=269 source_master_ns=141258352598 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=317422
[2026-11-04 11:46:56.657] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=313 source_master_ns=142280030602 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=362768
[2026-11-04 11:46:57.702] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=358 source_master_ns=143324928562 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=408114
[2026-11-04 11:46:58.723] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=402 source_master_ns=144346606566 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=453460
[2026-11-04 11:46:58.936] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2436933788 ntp_seconds=4002752818 ntp_fraction=4020089389 arrival_ns=11383852909600
[2026-11-04 11:46:58.936] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2436933788
[2026-11-04 11:46:58.936] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:46:58.951] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1210056190 ntp_seconds=4002752818 ntp_fraction=4084513898 arrival_ns=11383868226000
[2026-11-04 11:46:58.951] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1210056190
[2026-11-04 11:46:58.951] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:46:59.745] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=446 source_master_ns=145368284571 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=498806
[2026-11-04 11:47:00.792] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=491 source_master_ns=146413182530 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=544152
[2026-11-04 11:47:01.820] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=535 source_master_ns=147434860534 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=589498
[2026-11-04 11:47:02.833] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=579 source_master_ns=148456538539 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=634844
[2026-11-04 11:47:03.856] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=623 source_master_ns=149478216543 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=680190
[2026-11-04 11:47:03.948] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2436933788 ntp_seconds=4002752823 ntp_fraction=4071628996 arrival_ns=11388865495600
[2026-11-04 11:47:03.949] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2436933788
[2026-11-04 11:47:03.949] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:47:03.980] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1210056190 ntp_seconds=4002752823 ntp_fraction=4209067950 arrival_ns=11388896958000
[2026-11-04 11:47:03.980] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1210056190
[2026-11-04 11:47:03.980] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:47:04.904] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=668 source_master_ns=150523114503 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=725536
[2026-11-04 11:47:05.922] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=712 source_master_ns=151544792507 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=770882
[2026-11-04 11:47:06.943] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=756 source_master_ns=152566470512 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=816228
[2026-11-04 11:47:07.988] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=801 source_master_ns=153611368471 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=861574
[2026-11-04 11:47:08.968] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2436933788 ntp_seconds=4002752828 ntp_fraction=4157528342 arrival_ns=11393885065600
[2026-11-04 11:47:08.968] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2436933788
[2026-11-04 11:47:08.968] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:47:08.983] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1210056190 ntp_seconds=4002752828 ntp_fraction=4221952851 arrival_ns=11393900386000
[2026-11-04 11:47:08.984] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1210056190
[2026-11-04 11:47:08.984] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:47:09.009] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=845 source_master_ns=154633046475 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=906920
[2026-11-04 11:47:10.032] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=889 source_master_ns=155654724480 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=952266
[2026-11-04 11:47:11.053] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=933 source_master_ns=156676402485 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=997612
[2026-11-04 11:47:12.098] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=978 source_master_ns=157721300444 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1042958
[2026-11-04 11:47:13.119] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1022 source_master_ns=158742978448 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1088304
[2026-11-04 11:47:13.981] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2436933788 ntp_seconds=4002752833 ntp_fraction=4213362917 arrival_ns=11398898341000
[2026-11-04 11:47:13.981] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2436933788
[2026-11-04 11:47:13.982] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:47:14.013] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1210056190 ntp_seconds=4002752834 ntp_fraction=55834574 arrival_ns=11398930023200
[2026-11-04 11:47:14.013] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1210056190
[2026-11-04 11:47:14.014] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:47:14.141] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1066 source_master_ns=159764656453 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1133650
[2026-11-04 11:47:15.186] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1111 source_master_ns=160809554412 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1178996
[2026-11-04 11:47:16.207] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1155 source_master_ns=161831232417 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1224342
[2026-11-04 11:47:17.231] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1199 source_master_ns=162852910421 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1269688
[2026-11-04 11:47:18.252] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1243 source_master_ns=163874588426 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1315034
[2026-11-04 11:47:18.996] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2436933788 ntp_seconds=4002752838 ntp_fraction=4273492459 arrival_ns=11403912521700
[2026-11-04 11:47:18.996] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2436933788
[2026-11-04 11:47:18.996] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:47:19.017] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1210056190 ntp_seconds=4002752839 ntp_fraction=68719476 arrival_ns=11403933786200
[2026-11-04 11:47:19.017] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1210056190
[2026-11-04 11:47:19.017] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:47:19.297] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1288 source_master_ns=164919486385 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1360380
[2026-11-04 11:47:20.318] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1332 source_master_ns=165941164389 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1405726
[2026-11-04 11:47:21.341] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1376 source_master_ns=166962842394 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1451072
[2026-11-04 11:47:22.363] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1420 source_master_ns=167984520398 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1496418
[2026-11-04 11:47:23.408] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1465 source_master_ns=169029418358 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1541764
[2026-11-04 11:47:24.018] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2436933788 ntp_seconds=4002752844 ntp_fraction=73014444 arrival_ns=11408934857800
[2026-11-04 11:47:24.018] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2436933788
[2026-11-04 11:47:24.018] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:47:24.018] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1210056190 ntp_seconds=4002752844 ntp_fraction=77309411 arrival_ns=11408934878800
[2026-11-04 11:47:24.018] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1210056190
[2026-11-04 11:47:24.018] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:47:24.428] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1509 source_master_ns=170051096362 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1587110
[2026-11-04 11:47:25.449] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1553 source_master_ns=171072774367 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1632456
[2026-11-04 11:47:26.495] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1598 source_master_ns=172117672326 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1677802
[2026-11-04 11:47:27.516] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1642 source_master_ns=173139350330 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1723148
[2026-11-04 11:47:28.538] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1686 source_master_ns=174161028335 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1768494
[2026-11-04 11:47:29.031] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2436933788 ntp_seconds=4002752849 ntp_fraction=133143986 arrival_ns=11413948476700
[2026-11-04 11:47:29.031] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2436933788
[2026-11-04 11:47:29.032] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:47:29.047] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1210056190 ntp_seconds=4002752849 ntp_fraction=201863462 arrival_ns=11413963919100
[2026-11-04 11:47:29.047] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1210056190
[2026-11-04 11:47:29.047] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:47:29.560] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1730 source_master_ns=175182706339 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1813840
[2026-11-04 11:47:30.605] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1775 source_master_ns=176227604299 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1859186
[2026-11-04 11:47:31.626] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1819 source_master_ns=177249282303 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1904532
[2026-11-04 11:47:32.647] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1863 source_master_ns=178270960308 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1949878
[2026-11-04 11:47:33.693] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1908 source_master_ns=179315858267 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1995224
[2026-11-04 11:47:34.051] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2436933788 ntp_seconds=4002752854 ntp_fraction=219043332 arrival_ns=11418968329500
[2026-11-04 11:47:34.051] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2436933788
[2026-11-04 11:47:34.052] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:47:34.052] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1210056190 ntp_seconds=4002752854 ntp_fraction=219043332 arrival_ns=11418968541200
[2026-11-04 11:47:34.052] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1210056190
[2026-11-04 11:47:34.052] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:47:34.714] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1952 source_master_ns=180337536271 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2040570
[2026-11-04 11:47:35.736] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1996 source_master_ns=181359214276 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2085916
[2026-11-04 11:47:36.759] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2040 source_master_ns=182380892280 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2131262
[2026-11-04 11:47:37.802] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2085 source_master_ns=183425790240 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2176608
[2026-11-04 11:47:38.825] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2129 source_master_ns=184447468244 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2221954
[2026-11-04 11:47:39.065] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2436933788 ntp_seconds=4002752859 ntp_fraction=279172874 arrival_ns=11423982244600
[2026-11-04 11:47:39.065] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2436933788
[2026-11-04 11:47:39.065] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:47:39.084] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1210056190 ntp_seconds=4002752859 ntp_fraction=352187318 arrival_ns=11423998902600
[2026-11-04 11:47:39.084] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1210056190
[2026-11-04 11:47:39.084] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:47:39.847] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2173 source_master_ns=185469146249 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2267300
[2026-11-04 11:47:40.890] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2218 source_master_ns=186514044208 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2312646
[2026-11-04 11:47:41.913] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2262 source_master_ns=187535722212 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2357992
[2026-11-04 11:47:42.935] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2306 source_master_ns=188557400217 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2403338
[2026-11-04 11:47:43.956] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2350 source_master_ns=189579078221 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2448684
[2026-11-04 11:47:44.071] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2436933788 ntp_seconds=4002752864 ntp_fraction=300647710 arrival_ns=11428987655500
[2026-11-04 11:47:44.071] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2436933788
[2026-11-04 11:47:44.071] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:47:44.085] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1210056190 ntp_seconds=4002752864 ntp_fraction=365072220 arrival_ns=11429002561300
[2026-11-04 11:47:44.086] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1210056190
[2026-11-04 11:47:44.086] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:47:45.001] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2395 source_master_ns=190623976181 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2494030
[2026-11-04 11:47:45.883] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=190873556900 present_generation=1 present_canonical_target_ns=185341678004 present_canonical_dispatch_ns=185341678004 present_target_master_ns=190992833772 present_dispatch_master_ns=190992833772
[2026-11-04 11:47:45.910] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=190900026500 present_generation=1 present_canonical_target_ns=185366666666 present_canonical_dispatch_ns=185366666666 present_target_master_ns=191017822434 present_dispatch_master_ns=191017822434
[2026-11-04 11:47:46.022] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2439 source_master_ns=191645654185 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2539376
[2026-11-04 11:47:46.185] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=191175548800 present_generation=1 present_canonical_target_ns=185643537414 present_canonical_dispatch_ns=185643537414 present_target_master_ns=191294693182 present_dispatch_master_ns=191294693182
[2026-11-04 11:47:46.218] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=191208566200 present_generation=1 present_canonical_target_ns=185700000000 present_canonical_dispatch_ns=185700000000 present_target_master_ns=191351155768 present_dispatch_master_ns=191351155768
[2026-11-04 11:47:47.044] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2483 source_master_ns=192667332190 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2584722
[2026-11-04 11:47:48.089] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2528 source_master_ns=193712230149 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2630068
[2026-11-04 11:47:49.094] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2436933788 ntp_seconds=4002752869 ntp_fraction=403726925 arrival_ns=11434011688500
[2026-11-04 11:47:49.095] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2436933788
[2026-11-04 11:47:49.095] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:47:49.110] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1210056190 ntp_seconds=4002752869 ntp_fraction=472446402 arrival_ns=11434027066400
[2026-11-04 11:47:49.110] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1210056190
[2026-11-04 11:47:49.110] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:47:49.110] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2572 source_master_ns=194733908153 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2675414
[2026-11-04 11:47:50.133] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2616 source_master_ns=195755586158 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2720760
[2026-11-04 11:47:51.154] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2660 source_master_ns=196777264163 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2766106
[2026-11-04 11:47:52.203] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2705 source_master_ns=197822162122 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2811452
[2026-11-04 11:47:53.220] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2749 source_master_ns=198843840126 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2856798
[2026-11-04 11:47:54.104] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2436933788 ntp_seconds=4002752874 ntp_fraction=446676598 arrival_ns=11439021516000
[2026-11-04 11:47:54.105] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2436933788
[2026-11-04 11:47:54.105] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:47:54.120] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1210056190 ntp_seconds=4002752874 ntp_fraction=511101108 arrival_ns=11439036768600
[2026-11-04 11:47:54.120] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1210056190
[2026-11-04 11:47:54.120] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:47:54.242] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2793 source_master_ns=199865518131 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2902144
[2026-11-04 11:47:55.287] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2838 source_master_ns=200910416090 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2947490
[2026-11-04 11:47:56.309] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2882 source_master_ns=201932094095 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2992836
[2026-11-04 11:47:57.331] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2926 source_master_ns=202953772099 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3038182
[2026-11-04 11:47:58.353] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2970 source_master_ns=203975450104 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3083528
[2026-11-04 11:47:59.132] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1210056190 ntp_seconds=4002752879 ntp_fraction=562640715 arrival_ns=11444048429500
[2026-11-04 11:47:59.132] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1210056190
[2026-11-04 11:47:59.132] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:47:59.132] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2436933788 ntp_seconds=4002752879 ntp_fraction=566935683 arrival_ns=11444049579000
[2026-11-04 11:47:59.132] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2436933788
[2026-11-04 11:47:59.133] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:47:59.403] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3015 source_master_ns=205020348063 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3128874
[2026-11-04 11:48:00.418] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3059 source_master_ns=206042026067 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3174220
[2026-11-04 11:48:01.441] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3103 source_master_ns=207063704072 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3219566
[2026-11-04 11:48:02.485] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3148 source_master_ns=208108602031 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3264912
[2026-11-04 11:48:03.508] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3192 source_master_ns=209130280036 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3310258
[2026-11-04 11:48:04.134] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2436933788 ntp_seconds=4002752884 ntp_fraction=575525617 arrival_ns=11449051151200
[2026-11-04 11:48:04.134] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2436933788
[2026-11-04 11:48:04.134] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:48:04.149] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1210056190 ntp_seconds=4002752884 ntp_fraction=639950127 arrival_ns=11449066012400
[2026-11-04 11:48:04.149] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1210056190
[2026-11-04 11:48:04.149] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:48:04.529] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3236 source_master_ns=210151958040 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3355604
[2026-11-04 11:48:05.550] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3280 source_master_ns=211173636045 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3400950
[2026-11-04 11:48:06.597] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3325 source_master_ns=212218534004 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3446296
[2026-11-04 11:48:07.617] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3369 source_master_ns=213240212008 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3491642
[2026-11-04 11:48:08.639] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3413 source_master_ns=214261890013 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3536988
[2026-11-04 11:48:09.150] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2436933788 ntp_seconds=4002752889 ntp_fraction=644245094 arrival_ns=11454067545300
[2026-11-04 11:48:09.151] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2436933788
[2026-11-04 11:48:09.151] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:48:09.152] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1210056190 ntp_seconds=4002752889 ntp_fraction=648540061 arrival_ns=11454068664200
[2026-11-04 11:48:09.152] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1210056190
[2026-11-04 11:48:09.152] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:48:09.684] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3458 source_master_ns=215306787972 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3582334
[2026-11-04 11:48:10.705] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3502 source_master_ns=216328465977 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3627680
[2026-11-04 11:48:11.745] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3546 source_master_ns=217350143981 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3673026
[2026-11-04 11:48:12.749] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3590 source_master_ns=218371821986 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3718372
[2026-11-04 11:48:13.794] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3635 source_master_ns=219416719945 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3763718
[2026-11-04 11:48:14.162] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2436933788 ntp_seconds=4002752894 ntp_fraction=695784701 arrival_ns=11459079542900
[2026-11-04 11:48:14.163] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2436933788
[2026-11-04 11:48:14.163] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:48:14.177] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1210056190 ntp_seconds=4002752894 ntp_fraction=755914244 arrival_ns=11459093635300
[2026-11-04 11:48:14.177] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1210056190
[2026-11-04 11:48:14.177] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:48:14.815] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3679 source_master_ns=220438397949 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3809064
[2026-11-04 11:48:15.836] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3723 source_master_ns=221460075954 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3854410
[2026-11-04 11:48:16.881] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3768 source_master_ns=222504973913 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3899756
[2026-11-04 11:48:17.903] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3812 source_master_ns=223526651918 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3945102
[2026-11-04 11:48:18.925] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3856 source_master_ns=224548329922 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3990448
[2026-11-04 11:48:19.190] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2436933788 ntp_seconds=4002752899 ntp_fraction=816043786 arrival_ns=11464107087800
[2026-11-04 11:48:19.190] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2436933788
[2026-11-04 11:48:19.191] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:48:19.191] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1210056190 ntp_seconds=4002752899 ntp_fraction=816043786 arrival_ns=11464107087900
[2026-11-04 11:48:19.191] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1210056190
[2026-11-04 11:48:19.191] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:48:19.947] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3900 source_master_ns=225570007927 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4035794
[2026-11-04 11:48:20.992] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3945 source_master_ns=226614905886 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4081140
[2026-11-04 11:48:22.013] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3989 source_master_ns=227636583890 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4126486
[2026-11-04 11:48:23.034] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4033 source_master_ns=228658261895 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4171832
[2026-11-04 11:48:24.057] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4077 source_master_ns=229679939900 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4217178
[2026-11-04 11:48:24.195] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2436933788 ntp_seconds=4002752904 ntp_fraction=833223655 arrival_ns=11469111870600
[2026-11-04 11:48:24.195] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2436933788
[2026-11-04 11:48:24.195] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:48:24.210] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1210056190 ntp_seconds=4002752904 ntp_fraction=901943132 arrival_ns=11469127031600
[2026-11-04 11:48:24.210] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1210056190
[2026-11-04 11:48:24.210] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:48:25.101] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4122 source_master_ns=230724837859 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4262524
[2026-11-04 11:48:26.123] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4166 source_master_ns=231746515863 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4307870
[2026-11-04 11:48:27.145] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4210 source_master_ns=232768193868 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4353216
[2026-11-04 11:48:28.189] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4255 source_master_ns=233813091827 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4398562
[2026-11-04 11:48:29.207] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2436933788 ntp_seconds=4002752909 ntp_fraction=889058230 arrival_ns=11474124328800
[2026-11-04 11:48:29.207] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2436933788
[2026-11-04 11:48:29.208] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:48:29.211] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4299 source_master_ns=234834769831 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4443908
[2026-11-04 11:48:29.224] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1210056190 ntp_seconds=4002752909 ntp_fraction=957777707 arrival_ns=11474140712400
[2026-11-04 11:48:29.224] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1210056190
[2026-11-04 11:48:29.224] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:48:30.234] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4343 source_master_ns=235856447836 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4489254
[2026-11-04 11:48:31.255] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4387 source_master_ns=236878125841 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4534600
[2026-11-04 11:48:32.300] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4432 source_master_ns=237923023800 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4579946
[2026-11-04 11:48:33.322] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4476 source_master_ns=238944701804 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4625292
[2026-11-04 11:48:34.238] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2436933788 ntp_seconds=4002752914 ntp_fraction=1022202216 arrival_ns=11479155075900
[2026-11-04 11:48:34.238] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2436933788
[2026-11-04 11:48:34.239] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:48:34.254] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1210056190 ntp_seconds=4002752914 ntp_fraction=1086626725 arrival_ns=11479170497700
[2026-11-04 11:48:34.254] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1210056190
[2026-11-04 11:48:34.254] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:48:34.342] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4520 source_master_ns=239966379809 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4670638
[2026-11-04 11:48:35.388] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4565 source_master_ns=241011277768 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4715984
[2026-11-04 11:48:36.409] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4609 source_master_ns=242032955773 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4761330
[2026-11-04 11:48:37.431] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4653 source_master_ns=243054633777 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4806676
[2026-11-04 11:48:38.453] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4697 source_master_ns=244076311782 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4852022
[2026-11-04 11:48:39.248] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2436933788 ntp_seconds=4002752919 ntp_fraction=1065151889 arrival_ns=11484165629900
[2026-11-04 11:48:39.249] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2436933788
[2026-11-04 11:48:39.249] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:48:39.279] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1210056190 ntp_seconds=4002752919 ntp_fraction=1198295875 arrival_ns=11484195945900
[2026-11-04 11:48:39.279] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1210056190
[2026-11-04 11:48:39.279] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:48:39.498] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4742 source_master_ns=245121209741 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4897368
[2026-11-04 11:48:40.520] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4786 source_master_ns=246142887745 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4942714
[2026-11-04 11:48:41.541] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4830 source_master_ns=247164565750 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4988060
[2026-11-04 11:48:42.586] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4875 source_master_ns=248209463709 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5033406
[2026-11-04 11:48:43.608] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4919 source_master_ns=249231141714 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5078752
[2026-11-04 11:48:44.256] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2436933788 ntp_seconds=4002752924 ntp_fraction=1099511627 arrival_ns=11489172945900
[2026-11-04 11:48:44.256] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2436933788
[2026-11-04 11:48:44.256] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:48:44.284] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=1210056190 ntp_seconds=4002752924 ntp_fraction=1219770712 arrival_ns=11489201130800
[2026-11-04 11:48:44.284] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=1210056190
[2026-11-04 11:48:44.284] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:48:44.629] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4963 source_master_ns=250252819718 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5124098
[2026-11-04 11:48:45.651] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=5007 source_master_ns=251274497723 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5169444
[2026-11-04 11:48:46.696] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=5052 source_master_ns=252319395682 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5214790
[2026-11-04 11:48:47.718] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=5096 source_master_ns=253341073686 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5260136
[2026-11-04 11:48:48.398] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=2 reason=1 observed_at_ns=11493315586700
[2026-11-04 11:48:48.399] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=3 generation=3 invalidated_generation=2 locked=0
[2026-11-04 11:48:48.399] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=51 group=composition-r382.source.2 buffered=0 phase=0 preserves_output=1 protocol_generation=2 evidence_revision=94 video_raw_generation=1 video_ssrc=1210056190 audio_raw_generation=1 audio_ssrc=2436933788
[2026-11-04 11:48:48.399] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=50 group=composition-r382.source.2 buffered=0 phase=0 preserves_output=1 protocol_generation=2 evidence_revision=94 video_raw_generation=1 video_ssrc=1210056190 audio_raw_generation=1 audio_ssrc=2436933788
[2026-11-04 11:48:48.399] [info] [graph][runtime.node] rtp_binder_purge node=50 transition=2 cutoff=7205
[2026-11-04 11:48:48.399] [info] [graph][runtime.node] rtp_ingress_purge node=46 transition=2 cutoff=0
[2026-11-04 11:48:48.399] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=2 reason=1 observed_at_ns=11493316270600
[2026-11-04 11:48:48.400] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=3 generation=3 invalidated_generation=2 locked=0
[2026-11-04 11:48:48.399] [info] [graph][runtime.node] rtp_binder_purge node=51 transition=2 cutoff=10363
[2026-11-04 11:48:48.399] [info] [graph][runtime.node] av_reacquisition group=composition-r382.source.2 phase=purging old=2 next=3 transition=2
[2026-11-04 11:48:48.399] [info] [graph][runtime.node] rtp_ingress_purge node=47 transition=2 cutoff=0
[2026-11-04 11:48:48.400] [info] [graph][runtime.node] av_reacquisition group=composition-r382.source.2 event=purge_completed purge_ack=complete continuation=acquiring ack_count=2 old=2 next=3 transition=2
[2026-11-04 11:49:04.350] [info] [graph][runtime.lifecycle] continuous_output_drain stage=requested
[2026-11-04 11:49:04.351] [info] [graph][runtime.lifecycle] worker.finished node=4
[2026-11-04 11:49:04.350] [info] [graph][runtime.lifecycle] worker.finished node=2
[2026-11-04 11:49:04.350] [info] [graph][runtime.lifecycle] worker.finished node=77
[2026-11-04 11:49:04.351] [info] [graph][runtime.node] worker.thread_cpu node=77 available=1 cpu_ns=0 process_calls=8
[2026-11-04 11:49:04.351] [info] [graph][runtime.lifecycle] worker.finished node=5
[2026-11-04 11:49:04.351] [info] [graph][runtime.node] worker.thread_cpu node=5 available=1 cpu_ns=453125000 process_calls=34100
[2026-11-04 11:49:04.351] [info] [graph][runtime.node] worker.thread_cpu node=2 available=1 cpu_ns=2734375000 process_calls=23752
[2026-11-04 11:49:04.350] [info] [graph][runtime.lifecycle] worker.finished node=80
[2026-11-04 11:49:04.352] [info] [graph][runtime.node] worker.thread_cpu node=80 available=1 cpu_ns=0 process_calls=7
[2026-11-04 11:49:04.351] [info] [graph][runtime.node] worker.thread_cpu node=4 available=1 cpu_ns=5390625000 process_calls=35444
[2026-11-04 11:49:04.382] [info] [graph][runtime.lifecycle] worker.finished node=74
[2026-11-04 11:49:04.382] [info] [graph][runtime.node] worker.thread_cpu node=74 available=1 cpu_ns=812500000 process_calls=57325
[2026-11-04 11:49:04.382] [info] [graph][runtime.lifecycle] worker.finished node=78
[2026-11-04 11:49:04.382] [info] [graph][runtime.lifecycle] worker.finished node=75
[2026-11-04 11:49:04.382] [info] [graph][runtime.node] worker.thread_cpu node=75 available=1 cpu_ns=328125000 process_calls=57851
[2026-11-04 11:49:04.382] [info] [graph][runtime.node] worker.thread_cpu node=78 available=1 cpu_ns=437500000 process_calls=57196
[2026-11-04 11:49:04.410] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 produced_datagrams=171871 produced_ts_payload_bytes=203245296 pending_bytes=0 peak_pending_bytes=110920 access_units=19280 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=32789115 maximum_mux_input_after_emit_ns=36593578 maximum_mux_input_after_dispatch_ns=-63406422 worst_mux_input_ready_ns=5610969300 worst_mux_input_emit_ns=5574375722 worst_mux_input_dispatch_ns=5674375722 maximum_protocol_batch_produced_after_release_ns=37833356 worst_protocol_batch_produced_ns=202722326400 worst_protocol_batch_release_ns=202684493044 worst_protocol_batch_deadline_ns=202751159712 worst_protocol_batch_datagrams=1 exit_reason=completed
[2026-11-04 11:49:04.410] [info] [graph][runtime.lifecycle] worker.finished node=76
[2026-11-04 11:49:04.410] [info] [graph][runtime.node] worker.thread_cpu node=76 available=1 cpu_ns=2718750000 process_calls=116872
[2026-11-04 11:49:04.410] [info] [graph][runtime.lifecycle] worker.finished node=79
[2026-11-04 11:49:04.410] [info] [graph][runtime.node] worker.thread_cpu node=79 available=1 cpu_ns=1515625000 process_calls=87625
[2026-11-04 11:49:04.410] [info] [graph][runtime.node] datagram_batch_trace stage=last_completed generation=1 first_sequence=171871 last_sequence=171871 maximum_submit_interval_ns=88300 maximum_submit_interval_sequence=171871 maximum_start_lateness_ns=428861 maximum_start_lateness_sequence=171871 first_release_master_ns=269399500439 first_deadline_master_ns=269484459820 last_release_master_ns=269399500439 last_deadline_master_ns=269484459820 first_submitted_master_ns=269400017600 last_submitted_master_ns=269400017600 worst_wait_started_master_ns=269385503500 worst_wait_completed_master_ns=269399926800 worst_submit_observed_master_ns=269399929300 wait_attempts=1 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=1 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=1 wait_call_total_ns=14419600 wait_call_max_ns=14419600 wait_before_call_count=1 wait_before_call_total_ns=100 wait_before_call_max_ns=100 wait_arm_count=1 wait_arm_total_ns=1400 wait_arm_max_ns=1400 wait_api_wall_count=1 wait_api_wall_total_ns=14416300 wait_api_wall_max_ns=14416300 wait_return_to_next_clock_count=1 wait_return_to_next_clock_total_ns=3700 wait_return_to_next_clock_max_ns=3700 wait_target_lateness_count=1 wait_target_lateness_total_ns=426361 wait_target_lateness_max_ns=426361 worst_wait_metric=call_wall worst_wait_sequence=171871 worst_wait_target_master_ns=269399500439 worst_wait_sampled_master_ns=269385503500 worst_wait_sampled_steady_ns=11509312390600 worst_wait_timeout_ns=13996939 worst_wait_entered_steady_ns=11509312390700 worst_wait_returned_steady_ns=11509326810300 worst_wait_arm_before_steady_ns=11509312390900 worst_wait_arm_after_steady_ns=11509312392300 worst_wait_api_before_steady_ns=11509312392300 worst_wait_api_returned_steady_ns=11509326808600 worst_wait_windows_due_100ns=-139970 worst_wait_windows_result=1 worst_wait_windows_error=unavailable worst_wait_windows_pid=21924 worst_wait_windows_tid=220 worst_wait_windows_qpc_frequency_hz=10000000 worst_wait_next_master_ns=269399926800 worst_wait_next_steady_ns=11509326814000 worst_wait_outcome=deadline
[2026-11-04 11:49:04.411] [info] [graph][runtime.node] datagram_successful_submit_trace stage=worst_success generation=1 endpoint=1 sequence=132813 pacing_lateness_ns=29440240 matches_pacing_worst=1 materialized_master_ns=unavailable enqueued_master_ns=unavailable submit_interval_scope=pre_transport_checks_and_transport socket_only_ns=unavailable release_master_ns=202684489100 deadline_master_ns=202784489100 original_protocol_deadline_master_ns=202784489100 prefix_completion_limit_master_ns=202784489100 reserved_at_master_ns=202714148000 not_before_master_ns=202684711760 not_after_master_ns=202784454540 service_duration_ns=34560 wire_bytes_per_second=6250000 submit_started_master_ns=202714152000 submit_completed_master_ns=202714224500 activation_started_master_ns=202714107300 activation_completed_master_ns=202714146300 permit_started_master_ns=202714147500 permit_completed_master_ns=202714147600 snapshot_started_master_ns=202714147900 snapshot_sampled_master_ns=202714148000 snapshot_completed_master_ns=202714148300 reservation_completed_master_ns=202714150700 wait_started_master_ns=202714151500 wait_completed_master_ns=202714151500 submit_observed_master_ns=202714152000 wait_attempts=0 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=0 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=0 wait_call_total_ns=0 wait_call_max_ns=0 wait_before_call_count=0 wait_before_call_total_ns=0 wait_before_call_max_ns=0 wait_arm_count=0 wait_arm_total_ns=0 wait_arm_max_ns=0 wait_api_wall_count=0 wait_api_wall_total_ns=0 wait_api_wall_max_ns=0 wait_return_to_next_clock_count=0 wait_return_to_next_clock_total_ns=0 wait_return_to_next_clock_max_ns=0 wait_target_lateness_count=0 wait_target_lateness_total_ns=0 wait_target_lateness_max_ns=0
[2026-11-04 11:49:04.411] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=28525 committed_datagrams=171871 committed_payload_bytes=203245296 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=79296 pacing_reserved=171871 pacing_cancelled=0 pacing_submitted=171871 pacing_maximum_submit_lateness_ns=29440240 pacing_worst_late_global_sequence=132813 pacing_worst_success_sample_available=1 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=79296 backlog_max_residence_ns=52899000 last_materialized_sequence=171871 last_scheduled_sequence=171871 last_submitted_sequence=171871 last_committed_sequence=171871 submitted_service_generation=1 submitted_service_sequence=171871 submitted_service_completed_ns=269400017600 submitted_service_duration_ns=34560 submitted_service_available_ns=269400052160 submitted_scope_available_ns=269400052160 shared_scope_charged_datagrams=171871 shared_scope_charged_wire_bytes=208057684 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2154 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2154 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=188010 endpoint_1_target_effective_socket_bytes=188010 endpoint_1_api_requested_socket_bytes=188010 endpoint_1_effective_socket_bytes=188010 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=171871 endpoint_1_committed_payload_bytes=203245296
[2026-11-04 11:49:04.411] [info] [graph][runtime.lifecycle] worker.finished node=81
[2026-11-04 11:49:04.411] [info] [graph][runtime.node] worker.thread_cpu node=81 available=1 cpu_ns=10375000000 process_calls=57182
[2026-11-04 11:49:04.411] [info] [graph][runtime.lifecycle] distribution_registration scope=worker stage=leave node=81 pid=21924 tid=220 native=0
[2026-11-04 11:49:04.603] [info] [graph][runtime.lifecycle] continuous_output_drain stage=completed
[2026-11-04 11:49:04.604] [info] [graph][runtime.lifecycle] stop.begin
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=8 available=1 cpu_ns=156250000 process_calls=91780
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=33 available=1 cpu_ns=15625000 process_calls=10822
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=46 available=1 cpu_ns=671875000 process_calls=168469
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=47 available=1 cpu_ns=687500000 process_calls=163675
[2026-11-04 11:49:04.605] [info] [graph][runtime.node] worker.thread_cpu node=7 available=1 cpu_ns=1515625000 process_calls=45668
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=9 available=1 cpu_ns=0 process_calls=79
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=48 available=1 cpu_ns=46875000 process_calls=305
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=72 available=1 cpu_ns=453125000 process_calls=31364
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=10 available=1 cpu_ns=0 process_calls=105
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=11 available=1 cpu_ns=78125000 process_calls=10639
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=49 available=1 cpu_ns=0 process_calls=465
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=29 available=1 cpu_ns=0 process_calls=105
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=12 available=1 cpu_ns=0 process_calls=82
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=30 available=1 cpu_ns=93750000 process_calls=10639
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=14 available=1 cpu_ns=46875000 process_calls=10811
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=31 available=1 cpu_ns=15625000 process_calls=80
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=50 available=1 cpu_ns=187500000 process_calls=21447
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=17 available=1 cpu_ns=0 process_calls=79
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=52 available=1 cpu_ns=0 process_calls=311
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=13 available=1 cpu_ns=0 process_calls=105
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=51 available=1 cpu_ns=328125000 process_calls=30786
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=27 available=1 cpu_ns=203125000 process_calls=90087
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=32 available=1 cpu_ns=0 process_calls=106
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=53 available=1 cpu_ns=0 process_calls=456
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=36 available=1 cpu_ns=0 process_calls=79
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=54 available=1 cpu_ns=93750000 process_calls=21721
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=55 available=1 cpu_ns=328125000 process_calls=31178
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=59 available=1 cpu_ns=15625000 process_calls=304
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=15 available=1 cpu_ns=62500000 process_calls=10751
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=16 available=1 cpu_ns=109375000 process_calls=10832
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=35 available=1 cpu_ns=125000000 process_calls=10895
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=58 available=1 cpu_ns=562500000 process_calls=52678
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=18 available=1 cpu_ns=93750000 process_calls=10658
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=37 available=1 cpu_ns=93750000 process_calls=10675
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=34 available=1 cpu_ns=78125000 process_calls=10786
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=60 available=1 cpu_ns=375000000 process_calls=51801
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=19 available=1 cpu_ns=78125000 process_calls=10674
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=38 available=1 cpu_ns=62500000 process_calls=10678
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=56 available=1 cpu_ns=156250000 process_calls=21507
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=61 available=1 cpu_ns=421875000 process_calls=51655
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=20 available=1 cpu_ns=171875000 process_calls=10670
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=39 available=1 cpu_ns=78125000 process_calls=10661
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=64 available=1 cpu_ns=2625000000 process_calls=35003
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=70 available=1 cpu_ns=656250000 process_calls=42465
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=23 available=1 cpu_ns=46875000 process_calls=10535
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=42 available=1 cpu_ns=62500000 process_calls=10489
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=65 available=1 cpu_ns=93750000 process_calls=21313
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=66 available=1 cpu_ns=359375000 process_calls=22457
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=6 available=1 cpu_ns=2140625000 process_calls=44813
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=57 available=1 cpu_ns=265625000 process_calls=30902
[2026-11-04 11:49:04.605] [info] [graph][runtime.node] worker.thread_cpu node=26 available=1 cpu_ns=890625000 process_calls=13608
[2026-11-04 11:49:04.605] [info] [graph][runtime.node] worker.thread_cpu node=45 available=1 cpu_ns=796875000 process_calls=13584
[2026-11-04 11:49:04.605] [info] [graph][runtime.node] worker.thread_cpu node=68 available=1 cpu_ns=1750000000 process_calls=24992
[2026-11-04 11:49:04.605] [info] [graph][runtime.node] worker.thread_cpu node=73 available=1 cpu_ns=796875000 process_calls=50245
[2026-11-04 11:49:04.605] [info] [graph][runtime.node] worker.thread_cpu node=25 available=1 cpu_ns=171875000 process_calls=10547
[2026-11-04 11:49:04.605] [info] [graph][runtime.node] worker.thread_cpu node=44 available=1 cpu_ns=125000000 process_calls=10522
[2026-11-04 11:49:04.605] [info] [graph][runtime.node] worker.thread_cpu node=67 available=1 cpu_ns=265625000 process_calls=21197
[2026-11-04 11:49:04.605] [info] [graph][runtime.node] worker.thread_cpu node=62 available=1 cpu_ns=578125000 process_calls=51887
[2026-11-04 11:49:04.605] [info] [graph][runtime.node] worker.thread_cpu node=22 available=1 cpu_ns=1203125000 process_calls=13856
[2026-11-04 11:49:04.605] [info] [graph][runtime.node] worker.thread_cpu node=41 available=1 cpu_ns=953125000 process_calls=13873
[2026-11-04 11:49:04.605] [info] [graph][runtime.node] worker.thread_cpu node=71 available=1 cpu_ns=203125000 process_calls=31292
[2026-11-04 11:49:04.605] [info] [graph][runtime.node] worker.thread_cpu node=24 available=1 cpu_ns=93750000 process_calls=11453
[2026-11-04 11:49:04.605] [info] [graph][runtime.node] worker.thread_cpu node=43 available=1 cpu_ns=109375000 process_calls=11430
[2026-11-04 11:49:04.604] [info] [graph][runtime.node] worker.thread_cpu node=28 available=1 cpu_ns=0 process_calls=79
[2026-11-04 11:49:04.612] [info] [graph][runtime.node] mpegts_wire_materializer stage=stopped maximum_materialized_after_release_ns=37860356 worst_materialized_at_ns=202722353400 worst_release_ns=202684493044 worst_deadline_ns=202751159712 worst_global_sequence=132826
[2026-11-04 11:49:04.612] [info] [graph][runtime.node] audio_stage_observation node=74 stage=scheduler_input count=11364 last_generation=1 last_begin=11635712 last_end=11636736 last_rate=44100 last_steady_ns=11509275329400 maximum_inter_event_gap_ns=46438300 gap_begin_generation=1 gap_begin_begin=2039808 gap_begin_end=2040832 gap_begin_rate=44100 gap_begin_steady_ns=11291713212200 gap_end_generation=1 gap_end_begin=2040832 gap_end_end=2041856 gap_end_rate=44100 gap_end_steady_ns=11291759650500
[2026-11-04 11:49:04.612] [info] [graph][runtime.node] av_scheduler_correlated_wait generation=1 canonical_sequence=1389 missing=audio begin_steady_ns=11291713229800 end_steady_ns=11291759653300 observed_wait_ns=46423500 held_dispatch_master_ns=51917822434 begin_master_ns=51786342600 end_master_ns=51832765500 dispatch_remaining_at_end_ns=85056934 resumed_dispatch_master_ns=51928525382
[2026-11-04 11:49:04.613] [info] [graph][runtime.node] audio_stage_observation node=5 stage=canonicalizer_input count=11364 last_generation=1 last_begin=11635712 last_end=11636736 last_rate=44100 last_steady_ns=11509267922600 maximum_inter_event_gap_ns=46370900 gap_begin_generation=1 gap_begin_begin=2039808 gap_begin_end=2040832 gap_begin_rate=44100 gap_begin_steady_ns=11291713187200 gap_end_generation=1 gap_end_begin=2040832 gap_end_end=2041856 gap_end_rate=44100 gap_end_steady_ns=11291759558100
[2026-11-04 11:49:04.613] [info] [graph][runtime.node] audio_stage_observation node=5 stage=canonicalizer_committed count=11364 last_generation=1 last_begin=11635712 last_end=11636736 last_rate=44100 last_steady_ns=11509267940700 maximum_inter_event_gap_ns=46380300 gap_begin_generation=1 gap_begin_begin=2039808 gap_begin_end=2040832 gap_begin_rate=44100 gap_begin_steady_ns=11291713193500 gap_end_generation=1 gap_end_begin=2040832 gap_end_end=2041856 gap_end_rate=44100 gap_end_steady_ns=11291759573800
[2026-11-04 11:49:04.613] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_input count=11364 last_generation=1 last_begin=11635712 last_end=11636736 last_rate=44100 last_steady_ns=11509260342900 maximum_inter_event_gap_ns=45768800 gap_begin_generation=1 gap_begin_begin=2041856 gap_begin_end=2042880 gap_begin_rate=44100 gap_begin_steady_ns=11291712838500 gap_end_generation=1 gap_end_begin=2042880 gap_end_end=2043904 gap_end_rate=44100 gap_end_steady_ns=11291758607300
[2026-11-04 11:49:04.613] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_submitted count=11364 last_generation=1 last_begin=11635712 last_end=11636736 last_rate=44100 last_steady_ns=11509260482900 maximum_inter_event_gap_ns=46352000 gap_begin_generation=1 gap_begin_begin=2041856 gap_begin_end=2042880 gap_begin_rate=44100 gap_begin_steady_ns=11291713162900 gap_end_generation=1 gap_end_begin=2042880 gap_end_end=2043904 gap_end_rate=44100 gap_end_steady_ns=11291759514900
[2026-11-04 11:49:04.613] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_packet count=11364 last_generation=1 last_begin=11635712 last_end=11636736 last_rate=44100 last_steady_ns=11509267899000 maximum_inter_event_gap_ns=46357500 gap_begin_generation=1 gap_begin_begin=2039808 gap_begin_end=2040832 gap_begin_rate=44100 gap_begin_steady_ns=11291713173700 gap_end_generation=1 gap_end_begin=2040832 gap_end_end=2041856 gap_end_rate=44100 gap_end_steady_ns=11291759531200
[2026-11-04 11:49:04.613] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_committed count=11364 last_generation=1 last_begin=11635712 last_end=11636736 last_rate=44100 last_steady_ns=11509267906900 maximum_inter_event_gap_ns=46370900 gap_begin_generation=1 gap_begin_begin=2039808 gap_begin_end=2040832 gap_begin_rate=44100 gap_begin_steady_ns=11291713179000 gap_end_generation=1 gap_end_begin=2040832 gap_end_end=2041856 gap_end_rate=44100 gap_end_steady_ns=11291759549900
[2026-11-04 11:49:04.613] [info] [graph][runtime.lifecycle] codec_owner_release node=4 kind=AudioEncode owner_references=4
[2026-11-04 11:49:04.613] [info] [graph][runtime.node] audio_stage_observation node=7 stage=aggregate_built count=11364 last_generation=1 last_begin=11635712 last_end=11636736 last_rate=44100 last_steady_ns=11509260302700 maximum_inter_event_gap_ns=45739200 gap_begin_generation=1 gap_begin_begin=2041856 gap_begin_end=2042880 gap_begin_rate=44100 gap_begin_steady_ns=11291712806800 gap_end_generation=1 gap_end_begin=2042880 gap_end_end=2043904 gap_end_rate=44100 gap_end_steady_ns=11291758546000
[2026-11-04 11:49:04.613] [info] [graph][runtime.node] audio_stage_observation node=7 stage=aggregate_committed count=11364 last_generation=1 last_begin=11635712 last_end=11636736 last_rate=44100 last_steady_ns=11509260315300 maximum_inter_event_gap_ns=45362300 gap_begin_generation=1 gap_begin_begin=2041856 gap_begin_end=2042880 gap_begin_rate=44100 gap_begin_steady_ns=11291713204100 gap_end_generation=1 gap_end_begin=2042880 gap_end_end=2043904 gap_end_rate=44100 gap_end_steady_ns=11291758566400
[2026-11-04 11:49:04.613] [info] [graph][runtime.node] continuous_audio_aggregate.summary reason=stop output_group=composition-r382.output audio_commits=11364 real_samples=10252288 silence_samples=1384448
[2026-11-04 11:49:04.613] [info] [graph][runtime.lifecycle] video_encode.zero_copy_summary drm_prime=0 software_frame=0
[2026-11-04 11:49:04.613] [info] [graph][runtime.lifecycle] codec_owner_release node=2 kind=VideoEncode owner_references=4
[2026-11-04 11:49:04.614] [info] [graph][runtime.node] video_encoder_execution_observation node=2 count=7916 maximum_call_ns=19789300 maximum_begin_steady_ns=11270914545100 maximum_end_steady_ns=11270934334400 maximum_encoder_pts=765 maximum_native_result=0 maximum_between_submissions_ns=74312900 before_gap_begin_steady_ns=11430745851700 before_gap_end_steady_ns=11430748836100 before_gap_encoder_pts=5560 before_gap_native_result=0 after_gap_begin_steady_ns=11430823149000 after_gap_end_steady_ns=11430826849000 after_gap_encoder_pts=5561 after_gap_native_result=0
[2026-11-04 11:49:04.614] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=process count=44813 maximum_ns=44797300 begin_steady_ns=11430778328300 end_steady_ns=11430823125600 video_cursor=5561 audio_cursor=not_owned next_video_cursor=5561 next_audio_cursor=not_owned error=None return_state=progress deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-11-04 11:49:04.614] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=purge count=44813 maximum_ns=683800 begin_steady_ns=11359402706900 end_steady_ns=11359403390700 video_cursor=3420 audio_cursor=not_owned next_video_cursor=3420 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-04 11:49:04.614] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=bind count=44809 maximum_ns=206700 begin_steady_ns=11323579298100 end_steady_ns=11323579504800 video_cursor=2346 audio_cursor=not_owned next_video_cursor=2346 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-04 11:49:04.614] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=consume count=44809 maximum_ns=12482900 begin_steady_ns=11276816158900 end_steady_ns=11276828641800 video_cursor=943 audio_cursor=not_owned next_video_cursor=943 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-04 11:49:04.614] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=activate count=3 maximum_ns=16100 begin_steady_ns=11245411375700 end_steady_ns=11245411391800 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-04 11:49:04.614] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=build_video count=7916 maximum_ns=44751700 begin_steady_ns=11430778352400 end_steady_ns=11430823104100 video_cursor=5561 audio_cursor=not_owned next_video_cursor=5561 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-04 11:49:04.614] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=canvas count=7916 maximum_ns=44719000 begin_steady_ns=11430778365400 end_steady_ns=11430823084400 video_cursor=5561 audio_cursor=not_owned next_video_cursor=5561 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-04 11:49:04.614] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=emit count=7917 maximum_ns=5924400 begin_steady_ns=11342681477800 end_steady_ns=11342687402200 video_cursor=2918 audio_cursor=not_owned next_video_cursor=2918 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-04 11:49:04.614] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=between_calls_previous_return count=44812 maximum_ns=148236300 begin_steady_ns=11245257597400 end_steady_ns=11245405833700 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=waiting deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-11-04 11:49:04.614] [info] [graph][runtime.node] continuous_aggregate.summary reason=stop output_group=composition-r382.output output_generation=1 video_commits=7916 source[0].group=composition-r382.source.0 source[0].identity=composition-r382.source.0.input.video source[0].real_frames=3420 source[0].black_frames=4496 source[0].contribution_generation=0 source[1].group=composition-r382.source.1 source[1].identity=composition-r382.source.1.input.video source[1].real_frames=3420 source[1].black_frames=4496 source[1].contribution_generation=0 source[2].group=composition-r382.source.2 source[2].identity=composition-r382.source.2.input.video source[2].real_frames=6975 source[2].black_frames=941 source[2].contribution_generation=0
[2026-11-04 11:49:04.614] [info] [graph][runtime.node] cuda_canvas_call stage=context_push count=6979 operation=copy maximum_ns=269300 begin_steady_ns=11292412453800 end_steady_ns=11292412723100 native_result=0 thread_cpu_ns=0
[2026-11-04 11:49:04.614] [info] [graph][runtime.node] cuda_canvas_call stage=source_wait count=20796 operation=copy maximum_ns=323700 begin_steady_ns=11337378538100 end_steady_ns=11337378861800 native_result=0 thread_cpu_ns=0
[2026-11-04 11:49:04.614] [info] [graph][runtime.node] cuda_canvas_call stage=plane_copy count=41594 operation=copy maximum_ns=7154000 begin_steady_ns=11262280946000 end_steady_ns=11262288100000 native_result=0 thread_cpu_ns=0
[2026-11-04 11:49:04.615] [info] [graph][runtime.node] cuda_canvas_call stage=destination_wait count=2 operation=upload maximum_ns=79300 begin_steady_ns=11245193237600 end_steady_ns=11245193316900 native_result=0 thread_cpu_ns=0
[2026-11-04 11:49:04.615] [info] [graph][runtime.node] cuda_canvas_call stage=context_pop count=6979 operation=copy maximum_ns=248600 begin_steady_ns=11377945723600 end_steady_ns=11377945972200 native_result=0 thread_cpu_ns=0
[2026-11-04 11:49:04.615] [info] [graph][runtime.node] cuda_canvas_call stage=batch_completion count=6977 operation=copy maximum_ns=44580600 begin_steady_ns=11430778478000 end_steady_ns=11430823058600 native_result=0 thread_cpu_ns=0
[2026-11-04 11:49:04.615] [info] [graph][runtime.node] canvas_compose_correlated scope=attempt begin_steady_ns=11430778366200 end_steady_ns=11430823083700 wall_ns=44717500 thread_cpu_ns=0 batch_pid=21924 batch_tid=36952 batch_context=3264970126400 batch_stream=3265403917968 batch_submit_begin_steady_ns=11430778381400 batch_submit_end_steady_ns=11430778478000 batch_wait_begin_steady_ns=11430778478200 batch_wait_end_steady_ns=11430823055400 batch_timing_preparation=ready batch_timing_preparation_native=0 batch_start_event_native=0 batch_end_event_native=0 batch_sync_native=0 batch_elapsed_native=0 batch_gpu_elapsed_available=1 batch_gpu_elapsed_ms=20.0417 batch_gpu_interval=event_execution_interval_excludes_pre_start_queue surface_count=1 surface_total_ns=1000 surface_maximum_ns=1000 surface_maximum_tile=none surface_maximum_result=unavailable validate_count=1 validate_total_ns=1500 validate_maximum_ns=1500 validate_maximum_tile=2 validate_maximum_result=0 black_count=1 black_total_ns=39700 black_maximum_ns=39700 black_maximum_tile=none black_maximum_result=0 tile_count=1 tile_total_ns=19300 tile_maximum_ns=19300 tile_maximum_tile=2 tile_maximum_result=0 publish_count=1 publish_total_ns=8100 publish_maximum_ns=8100 publish_maximum_tile=none publish_maximum_result=0 context_push_count=1 context_push_total_ns=4600 context_push_maximum_ns=4600 context_push_maximum_tile=none context_push_maximum_result=0 source_wait_count=2 source_wait_total_ns=2900 source_wait_maximum_ns=2900 source_wait_maximum_tile=2 source_wait_maximum_result=0 plane_copy_count=4 plane_copy_total_ns=48000 plane_copy_maximum_ns=31900 plane_copy_maximum_tile=none plane_copy_maximum_result=0 destination_wait_count=0 destination_wait_total_ns=0 destination_wait_maximum_ns=0 destination_wait_maximum_tile=none destination_wait_maximum_result=unavailable context_pop_count=1 context_pop_total_ns=5300 context_pop_maximum_ns=5300 context_pop_maximum_tile=none context_pop_maximum_result=0 batch_completion_count=1 batch_completion_total_ns=44577500 batch_completion_maximum_ns=44577500 batch_completion_maximum_tile=none batch_completion_maximum_result=0 source_query_completed_count=0 source_query_ready_count=1 source_query_ready_maximum_ns=4600 source_query_ready_begin_steady_ns=11430778454700 source_query_ready_end_steady_ns=11430778459300 source_query_ready_tile=2 source_query_ready_operation=event_query source_query_ready_native=0 source_query_ready_restore_native=0 source_query_pending_count=0 source_query_unavailable_count=0 source_query_error_count=0
[2026-11-04 11:49:04.615] [info] [graph][runtime.node] source_video_trace_summary node_id=67 node=VideoPreparationNode generation=2 reason=stop steady_ns=11509532236600 input_count=3569 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3569 published_count=3569 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=13 incomplete=0
[2026-11-04 11:49:04.615] [info] [graph][runtime.node] source_video_trace_summary node_id=44 node=VideoPreparationNode generation=1 reason=stop steady_ns=11509532344500 input_count=3435 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3435 published_count=3433 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=5 incomplete=0
[2026-11-04 11:49:04.615] [info] [graph][runtime.node] source_video_trace_summary node_id=25 node=VideoPreparationNode generation=1 reason=stop steady_ns=11509532427200 input_count=3435 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3435 published_count=3433 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-04 11:49:04.615] [info] [graph][runtime.lifecycle] codec_owner_release node=73 kind=AudioResample owner_references=1
[2026-11-04 11:49:04.615] [info] [graph][runtime.lifecycle] video_filter.zero_copy_summary drm_prime_input=0 drm_prime_output=0 rga=0 software_frame=0
[2026-11-04 11:49:04.615] [info] [graph][runtime.node] source_video_trace_summary node_id=68 node=VideoFilterNode generation=2 reason=stop steady_ns=11509532782300 input_count=3569 accepted_count=3569 would_block_count=0 resolved_count=3569 output_attempt_count=0 published_count=3569 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:49:04.619] [info] [graph][runtime.lifecycle] video_filter.zero_copy_summary drm_prime_input=0 drm_prime_output=0 rga=0 software_frame=0
[2026-11-04 11:49:04.619] [info] [graph][runtime.node] source_video_trace_summary node_id=45 node=VideoFilterNode generation=1 reason=stop steady_ns=11509536580100 input_count=3441 accepted_count=3441 would_block_count=0 resolved_count=3441 output_attempt_count=0 published_count=3440 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-04 11:49:04.623] [info] [graph][runtime.lifecycle] video_filter.zero_copy_summary drm_prime_input=0 drm_prime_output=0 rga=0 software_frame=0
[2026-11-04 11:49:04.623] [info] [graph][runtime.node] source_video_trace_summary node_id=26 node=VideoFilterNode generation=1 reason=stop steady_ns=11509540533100 input_count=3442 accepted_count=3442 would_block_count=0 resolved_count=3442 output_attempt_count=0 published_count=3441 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-04 11:49:04.627] [info] [graph][runtime.node] source_video_trace_summary node_id=66 node=VideoFrameRateNode generation=2 reason=stop steady_ns=11509544778200 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3569 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:49:04.628] [info] [graph][runtime.node] source_video_trace_summary node_id=43 node=VideoFrameRateNode generation=1 reason=stop steady_ns=11509544959400 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3447 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:49:04.628] [info] [graph][runtime.node] source_video_trace_summary node_id=24 node=VideoFrameRateNode generation=1 reason=stop steady_ns=11509545064100 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3448 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:49:04.628] [info] [graph][runtime.node] hardware_transfer.summary forwarded=3607 download=0 upload=0
[2026-11-04 11:49:04.628] [info] [graph][runtime.node] source_video_trace_summary node_id=65 node=HardwareTransferNode generation=2 reason=stop steady_ns=11509545207300 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3569 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:49:04.628] [info] [graph][runtime.node] hardware_transfer.summary forwarded=42 download=0 upload=0
[2026-11-04 11:49:04.628] [info] [graph][runtime.node] source_video_trace_summary node_id=42 node=HardwareTransferNode generation=1 reason=stop steady_ns=11509545323600 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3461 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:49:04.628] [info] [graph][runtime.node] hardware_transfer.summary forwarded=40 download=0 upload=0
[2026-11-04 11:49:04.628] [info] [graph][runtime.node] source_video_trace_summary node_id=23 node=HardwareTransferNode generation=1 reason=stop steady_ns=11509545578300 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3461 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:49:04.628] [info] [graph][runtime.lifecycle] codec_owner_release node=70 kind=AudioDecode owner_references=2
[2026-11-04 11:49:04.629] [info] [graph][runtime.lifecycle] video_decode.zero_copy_summary drm_prime=0 software_frame=0
[2026-11-04 11:49:04.629] [info] [graph][runtime.node] source_video_trace_summary node_id=64 node=VideoDecodeNode generation=2 reason=stop steady_ns=11509545974000 input_count=3569 accepted_count=3569 would_block_count=0 resolved_count=3569 output_attempt_count=0 published_count=3569 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=106 incomplete=0
[2026-11-04 11:49:04.629] [info] [graph][runtime.lifecycle] codec_owner_release node=64 kind=VideoDecode owner_references=2
[2026-11-04 11:49:04.641] [info] [graph][runtime.lifecycle] video_decode.zero_copy_summary drm_prime=0 software_frame=0
[2026-11-04 11:49:04.642] [info] [graph][runtime.node] source_video_trace_summary node_id=41 node=VideoDecodeNode generation=1 reason=stop steady_ns=11509558831100 input_count=3462 accepted_count=3462 would_block_count=0 resolved_count=3462 output_attempt_count=0 published_count=3461 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=107 incomplete=0
[2026-11-04 11:49:04.642] [info] [graph][runtime.lifecycle] codec_owner_release node=41 kind=VideoDecode owner_references=2
[2026-11-04 11:49:04.655] [info] [graph][runtime.lifecycle] video_decode.zero_copy_summary drm_prime=0 software_frame=0
[2026-11-04 11:49:04.655] [info] [graph][runtime.node] source_video_trace_summary node_id=22 node=VideoDecodeNode generation=1 reason=stop steady_ns=11509572233000 input_count=3462 accepted_count=3462 would_block_count=0 resolved_count=3462 output_attempt_count=0 published_count=3461 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=107 incomplete=0
[2026-11-04 11:49:04.655] [info] [graph][runtime.lifecycle] codec_owner_release node=22 kind=VideoDecode owner_references=2
[2026-11-04 11:49:04.670] [info] [graph][runtime.node] source_video_trace_summary node_id=62 node=MediaAvBoundReleaseExtractorNode generation=2 reason=stop steady_ns=11509586946300 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=3554 release_committed_count=3554 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:49:04.670] [info] [graph][runtime.node] source_video_trace_summary node_id=39 node=MediaAvBoundReleaseExtractorNode generation=1 reason=stop steady_ns=11509587175200 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=3571 release_committed_count=3555 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:49:04.670] [info] [graph][runtime.node] source_video_trace_summary node_id=20 node=MediaAvBoundReleaseExtractorNode generation=1 reason=stop steady_ns=11509587291200 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=3571 release_committed_count=3555 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:49:04.670] [info] [graph][runtime.node] source_video_trace_summary node_id=61 node=ActivationReleaseSequencer generation=2 reason=stop steady_ns=11509587480500 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3554 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:49:04.670] [info] [graph][runtime.node] source_video_trace_summary node_id=38 node=ActivationReleaseSequencer generation=1 reason=stop steady_ns=11509587591900 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3555 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:49:04.670] [info] [graph][runtime.node] source_video_trace_summary node_id=19 node=ActivationReleaseSequencer generation=1 reason=stop steady_ns=11509587723100 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3555 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:49:04.671] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-11-04 11:49:04.671] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-11-04 11:49:04.671] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-11-04 11:49:04.671] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=10314 completion_waits=10314 completed_datagrams=10462 drained_datagrams=159 would_block=15 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-04 11:49:04.671] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=142466 completion_waits=142466 completed_datagrams=212075 drained_datagrams=69622 would_block=15 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-04 11:49:04.672] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=75196 completion_waits=75196 completed_datagrams=105509 drained_datagrams=30387 would_block=74 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-04 11:49:04.672] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=75164 completion_waits=75164 completed_datagrams=105415 drained_datagrams=30324 would_block=74 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-04 11:49:04.673] [info] [graph][runtime.node] codec_resolver.release slot=encoder_parameters bufferReferences=1 hasCredit=0 creditReferences=0
[2026-11-04 11:49:04.673] [info] [graph][runtime.lifecycle] stop.done state=Stopped
[2026-11-04 11:49:04.730] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=retired detail="" description=""
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=892, threads=81, processThreads=129, workers=0, workerProgress=546708, workerProcessCalls=2050582, workerWaits=1503857, workerWakeups=1483730, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=847, logicalProcessors=22, averageSystemMachineCpuPercent=9.290698, averageProcessMachineCpuPercent=1.595898, peakProcessMachineCpuPercent=7.484407, averageProcessSingleCoreCpuPercent=35.109763, peakProcessSingleCoreCpuPercent=164.656965, initialWorkingSetBytes=211783680, workingSetBytes=261439488, peakWorkingSetBytes=270077952, totalPushed=472086, totalPopped=472050, droppedBuffers=0, encodedPacketsPushed=88490, encodedPacketsPopped=88490, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=15211982, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=737, graphPayloadReservations=218146, graphPayloadReleases=218146, graphPayloadPressureFailures=0, lastInputReceivedAtNs=11493315231600
[CLI] realtime video validation stopped successfully
~~~~

### composition-r382-monitor.csv

~~~~text
"Time","CpuPercent","WorkingSet","PrivateBytes"
"2026-11-04T11:44:36.0256859+08:00","0.563160454307035","144007168","569434112"
"2026-11-04T11:44:37.0530290+08:00","0","146800640","572342272"
"2026-11-04T11:44:38.0622078+08:00","0.21113025939326296","150749184","575897600"
"2026-11-04T11:44:39.0859376+08:00","0.06937643826791726","154054656","578830336"
"2026-11-04T11:44:40.1025698+08:00","0.48902552064462534","186093568","668917760"
"2026-11-04T11:44:41.1186972+08:00","3.704461217613604","255594496","1259970560"
"2026-11-04T11:44:42.1431397+08:00","2.495814242203132","256229376","1260347392"
"2026-11-04T11:44:43.1578883+08:00","1.67977118130092","258211840","1262112768"
"2026-11-04T11:44:44.1808349+08:00","1.73573887612333","259792896","1260998656"
"2026-11-04T11:44:45.1978442+08:00","2.37438608208669","260206592","1261174784"
"2026-11-04T11:44:46.2186706+08:00","1.530622640637037","260927488","1261727744"
"2026-11-04T11:44:47.2316241+08:00","1.4022899821704997","263102464","1263747072"
"2026-11-04T11:44:48.2493553+08:00","2.2331311771981373","263917568","1264500736"
"2026-11-04T11:44:49.2660651+08:00","2.025808240374088","263294976","1263755264"
"2026-11-04T11:44:50.2866610+08:00","1.391789390349839","263352320","1263755264"
"2026-11-04T11:44:51.3021594+08:00","2.3079800027257553","263352320","1263755264"
"2026-11-04T11:44:52.3213875+08:00","2.0904857491486135","265781248","1266081792"
"2026-11-04T11:44:53.3390107+08:00","2.5823319565541634","265900032","1266081792"
"2026-11-04T11:44:54.3460758+08:00","1.3399648326427143","265908224","1266094080"
"2026-11-04T11:44:55.3652305+08:00","1.3937575379425178","263766016","1263759360"
"2026-11-04T11:44:56.3850429+08:00","2.367859742902447","263843840","1263759360"
"2026-11-04T11:44:57.4004616+08:00","1.818551213495388","263843840","1263759360"
"2026-11-04T11:44:58.4205510+08:00","1.670976538473446","263852032","1263759360"
"2026-11-04T11:44:59.4373614+08:00","2.1653049038980576","263860224","1263759360"
"2026-11-04T11:45:00.4562797+08:00","2.857865854585022","263946240","1263796224"
"2026-11-04T11:45:01.4815561+08:00","2.1474253630089852","264003584","1263796224"
"2026-11-04T11:45:02.4956775+08:00","1.6808100633173253","264060928","1263796224"
"2026-11-04T11:45:03.5105457+08:00","1.6095910062732555","264077312","1263804416"
"2026-11-04T11:45:04.5314177+08:00","2.156690109489285","264081408","1263808512"
"2026-11-04T11:45:05.5517086+08:00","1.8098670772138703","264048640","1263771648"
"2026-11-04T11:45:06.5747881+08:00","3.6098678726157827","264069120","1263771648"
"2026-11-04T11:45:07.5935743+08:00","2.230818667083705","265359360","1265020928"
"2026-11-04T11:45:08.6091691+08:00","2.3776930792405864","266809344","1266724864"
"2026-11-04T11:45:09.6330473+08:00","1.8728923385258485","266809344","1266724864"
"2026-11-04T11:45:10.6517617+08:00","1.533795929457756","266809344","1266724864"
"2026-11-04T11:45:11.6696887+08:00","2.0931577786833615","266809344","1266724864"
"2026-11-04T11:45:12.6893084+08:00","1.323465815913343","264888320","1264537600"
"2026-11-04T11:45:13.7116431+08:00","1.3199511062099507","264945664","1264537600"
"2026-11-04T11:45:14.7344718+08:00","2.7775023236140037","264970240","1264537600"
"2026-11-04T11:45:15.7429880+08:00","1.338036828939206","264966144","1264537600"
"2026-11-04T11:45:16.7595097+08:00","1.6069728046855543","267866112","1267691520"
"2026-11-04T11:45:17.7794921+08:00","1.8104144827311814","268062720","1267691520"
"2026-11-04T11:45:18.8017362+08:00","2.014840771308038","268066816","1267691520"
"2026-11-04T11:45:19.8219427+08:00","1.531552680756298","265965568","1265569792"
"2026-11-04T11:45:20.8449028+08:00","1.4580014144513287","264282112","1263796224"
"2026-11-04T11:45:21.8701003+08:00","1.3855423422848236","264278016","1263796224"
"2026-11-04T11:45:22.8931291+08:00","2.985230985410452","264286208","1263796224"
"2026-11-04T11:45:23.9124142+08:00","1.3935792306338486","264478720","1263988736"
"2026-11-04T11:45:24.9287326+08:00","2.1663531285614286","266821632","1266405376"
"2026-11-04T11:45:25.9465913+08:00","1.674638586422118","266821632","1266405376"
"2026-11-04T11:45:26.9683523+08:00","1.668242822485351","266821632","1266405376"
"2026-11-04T11:45:27.9927009+08:00","1.8720322714002209","266940416","1266405376"
"2026-11-04T11:45:29.0241386+08:00","1.859165741531104","266940416","1266405376"
"2026-11-04T11:45:30.0459325+08:00","1.3206497104570873","265572352","1264967680"
"2026-11-04T11:45:31.0687432+08:00","1.6665307222005545","265580544","1264975872"
"2026-11-04T11:45:32.0816429+08:00","1.40236446457092","265580544","1264975872"
"2026-11-04T11:45:33.1005395+08:00","1.2546995356634727","265822208","1265217536"
"2026-11-04T11:45:34.1180680+08:00","2.163776784094544","265822208","1265217536"
"2026-11-04T11:45:35.1411488+08:00","1.8049316428290991","265777152","1265143808"
"2026-11-04T11:45:36.1542385+08:00","2.2433623327996255","265777152","1265143808"
"2026-11-04T11:45:37.1717247+08:00","1.9544602802832742","265834496","1265143808"
"2026-11-04T11:45:38.1943432+08:00","1.5973921137479197","265949184","1265143808"
"2026-11-04T11:45:39.2175179+08:00","2.6377349208924303","265949184","1265143808"
"2026-11-04T11:45:40.2399410+08:00","1.597697398731237","265949184","1265143808"
"2026-11-04T11:45:41.2565328+08:00","2.445224774137913","267137024","1266331648"
"2026-11-04T11:45:42.2758340+08:00","1.811624384520404","269099008","1268318208"
"2026-11-04T11:45:43.2930610+08:00","2.1644181145944272","269099008","1268318208"
"2026-11-04T11:45:44.3109204+08:00","1.6048608749624231","269217792","1268318208"
"2026-11-04T11:45:45.3302837+08:00","1.741840403532462","268165120","1267232768"
"2026-11-04T11:45:46.3379140+08:00","2.185032095059612","268156928","1267232768"
"2026-11-04T11:45:47.3571329+08:00","2.0905046189604786","268156928","1267232768"
"2026-11-04T11:45:48.3756063+08:00","1.9525658339593","268156928","1267232768"
"2026-11-04T11:45:49.3939501+08:00","1.3251240083965927","268279808","1267232768"
"2026-11-04T11:45:50.4175777+08:00","2.1508843113008536","268271616","1267232768"
"2026-11-04T11:45:51.4385878+08:00","2.017275922059038","268271616","1267232768"
"2026-11-04T11:45:52.4549863+08:00","2.4456898101930045","268271616","1267232768"
"2026-11-04T11:45:53.4727147+08:00","2.5820650274581207","268279808","1267232768"
"2026-11-04T11:45:54.4866529+08:00","2.591717038662622","268271616","1267232768"
"2026-11-04T11:45:55.4955033+08:00","1.4783929041682222","268275712","1267236864"
"2026-11-04T11:45:56.5160396+08:00","1.6006512725443742","268275712","1267236864"
"2026-11-04T11:45:57.5404467+08:00","1.8719253667449558","268275712","1267236864"
"2026-11-04T11:45:58.5608172+08:00","1.6705162042076425","268279808","1267236864"
"2026-11-04T11:45:59.5817079+08:00","1.2522487381940994","268275712","1267236864"
"2026-11-04T11:46:00.6044159+08:00","1.8055895808881024","268275712","1267236864"
"2026-11-04T11:46:01.6238563+08:00","1.8810453621061483","268275712","1267236864"
"2026-11-04T11:46:02.6428268+08:00","2.648618028062281","268283904","1267236864"
"2026-11-04T11:46:03.6640707+08:00","2.0863594075634806","268275712","1267236864"
"2026-11-04T11:46:04.6841259+08:00","1.949537989352305","268275712","1267236864"
"2026-11-04T11:46:05.7065993+08:00","2.709005792851299","268275712","1267236864"
"2026-11-04T11:46:06.7256579+08:00","2.1605279082621407","268275712","1267236864"
"2026-11-04T11:46:07.7331560+08:00","2.6082837368039793","268292096","1267236864"
"2026-11-04T11:46:08.7413258+08:00","1.4793909445881765","268308480","1267236864"
"2026-11-04T11:46:09.7592374+08:00","1.3954596307327134","268324864","1267236864"
"2026-11-04T11:46:10.7771667+08:00","1.814065975987634","268324864","1267236864"
"2026-11-04T11:46:11.7860237+08:00","1.3375848293482806","268324864","1267236864"
"2026-11-04T11:46:12.8029805+08:00","1.6761237591856946","268324864","1267236864"
"2026-11-04T11:46:13.8232167+08:00","1.6011221002280913","268382208","1267236864"
"2026-11-04T11:46:14.8413175+08:00","1.5347203341751623","269869056","1268940800"
"2026-11-04T11:46:15.8566878+08:00","1.4688998414935641","270635008","1269678080"
"2026-11-04T11:46:16.8726025+08:00","2.0973038564968283","269926400","1268948992"
"2026-11-04T11:46:17.8939903+08:00","1.3211747958824436","269996032","1268948992"
"2026-11-04T11:46:18.9041455+08:00","1.6171007457791904","269164544","1267814400"
"2026-11-04T11:46:19.9113932+08:00","1.5512569549674822","269221888","1267814400"
"2026-11-04T11:46:20.9328020+08:00","1.599283976477124","269221888","1267814400"
"2026-11-04T11:46:21.9538952+08:00","1.7388894391013296","269221888","1267814400"
"2026-11-04T11:46:22.9650405+08:00","2.1071964812394604","269959168","1268555776"
"2026-11-04T11:46:23.9846010+08:00","2.5774251837835114","270016512","1268555776"
"2026-11-04T11:46:25.0027494+08:00","2.6507566444770094","270024704","1268555776"
"2026-11-04T11:46:26.0179574+08:00","1.8189286422988282","270016512","1268555776"
"2026-11-04T11:46:27.0366529+08:00","2.3007365792820327","270016512","1268555776"
"2026-11-04T11:46:28.0442240+08:00","2.1851604769673774","270020608","1268555776"
"2026-11-04T11:46:29.0552954+08:00","1.2644102987277563","270016512","1268555776"
"2026-11-04T11:46:30.0643410+08:00","2.463511514787294","270016512","1268555776"
"2026-11-04T11:46:31.0742675+08:00","1.8987655402285577","270020608","1268559872"
"2026-11-04T11:46:32.0950728+08:00","1.9481054454129145","270024704","1268559872"
"2026-11-04T11:46:33.1103289+08:00","2.3784863023947627","270077952","1268559872"
"2026-11-04T11:46:34.1255942+08:00","1.818825984785365","270077952","1268559872"
"2026-11-04T11:46:35.1477052+08:00","1.8761305145562823","244416512","1249255424"
"2026-11-04T11:46:36.1588759+08:00","1.123809893189781","244416512","1249255424"
"2026-11-04T11:46:37.1708646+08:00","1.3334455396407272","244416512","1249255424"
"2026-11-04T11:46:38.1815864+08:00","1.054039706169303","244416512","1249255424"
"2026-11-04T11:46:39.1917180+08:00","0.773414077928064","244416512","1249255424"
"2026-11-04T11:46:40.2045896+08:00","1.1920428646991026","244424704","1249255424"
"2026-11-04T11:46:41.2235164+08:00","1.1152554200788873","244416512","1249255424"
"2026-11-04T11:46:42.2328540+08:00","1.1258508910830591","244416512","1249255424"
"2026-11-04T11:46:43.2429744+08:00","0.8437338036859046","244416512","1249255424"
"2026-11-04T11:46:44.2646557+08:00","1.1122486399267915","245145600","1249984512"
"2026-11-04T11:46:45.2722471+08:00","1.1982896674548469","244416512","1249255424"
"2026-11-04T11:46:46.2816148+08:00","1.055453735136273","244416512","1249255424"
"2026-11-04T11:46:47.2931392+08:00","0.7723491395758717","244416512","1249255424"
"2026-11-04T11:46:48.3062504+08:00","1.1216573623543362","244416512","1249255424"
"2026-11-04T11:46:49.3207690+08:00","1.120101333148191","246521856","1249988608"
"2026-11-04T11:46:50.3468491+08:00","1.9380907627351547","251936768","1255292928"
"2026-11-04T11:46:51.3677579+08:00","1.5304991004093609","253620224","1256456192"
"2026-11-04T11:46:52.3745630+08:00","1.693024255186485","254332928","1257099264"
"2026-11-04T11:46:53.3978105+08:00","1.5270010432471126","255324160","1257897984"
"2026-11-04T11:46:54.4140317+08:00","1.677337035032781","255897600","1258790912"
"2026-11-04T11:46:55.4201940+08:00","2.1176323324595034","256405504","1259114496"
"2026-11-04T11:46:56.4405243+08:00","1.5313668524790456","256638976","1259315200"
"2026-11-04T11:46:57.4601945+08:00","1.602010853384484","257589248","1260228608"
"2026-11-04T11:46:58.4718087+08:00","1.404146507091879","258031616","1260498944"
"2026-11-04T11:46:59.4926964+08:00","1.6696698907680585","258191360","1261252608"
"2026-11-04T11:47:00.5121380+08:00","1.4630335594773383","258355200","1261404160"
"2026-11-04T11:47:01.5330506+08:00","1.6000612856308438","258519040","1261510656"
"2026-11-04T11:47:02.5457356+08:00","1.1922625136507043","258527232","1261510656"
"2026-11-04T11:47:03.5620879+08:00","1.607240646056222","258785280","1261629440"
"2026-11-04T11:47:04.5684899+08:00","1.4114186433001379","259022848","1261707264"
"2026-11-04T11:47:05.5894102+08:00","1.460914503049134","259297280","1261707264"
"2026-11-04T11:47:06.6129571+08:00","0.9020548589863879","259297280","1261723648"
"2026-11-04T11:47:07.6215467+08:00","1.4083573194236243","259424256","1261850624"
"2026-11-04T11:47:08.6436394+08:00","2.1541143434979486","259436544","1261871104"
"2026-11-04T11:47:09.6593338+08:00","1.2586552519233056","259432448","1261879296"
"2026-11-04T11:47:10.6809103+08:00","1.2514080843765403","259555328","1261940736"
"2026-11-04T11:47:11.7041983+08:00","1.6657533896082575","259567616","1261953024"
"2026-11-04T11:47:12.7110579+08:00","1.5518548961543395","259592192","1261953024"
"2026-11-04T11:47:13.7180248+08:00","1.9043462464989032","259608576","1260867584"
"2026-11-04T11:47:14.7397534+08:00","1.5987834022388403","259715072","1260867584"
"2026-11-04T11:47:15.7589110+08:00","1.811879643630101","259715072","1260867584"
"2026-11-04T11:47:16.7799430+08:00","1.0433962002081318","259981312","1261125632"
"2026-11-04T11:47:17.8012150+08:00","1.8081284017293229","259985408","1261125632"
"2026-11-04T11:47:18.8178394+08:00","2.0958397400080284","259981312","1261125632"
"2026-11-04T11:47:19.8328386+08:00","1.609383265792453","260059136","1261203456"
"2026-11-04T11:47:20.8494355+08:00","1.9561700056692715","260063232","1261203456"
"2026-11-04T11:47:21.8576898+08:00","2.254121081087651","260800512","1261932544"
"2026-11-04T11:47:22.8780514+08:00","1.2528980813361565","260186112","1261203456"
"2026-11-04T11:47:23.8940539+08:00","2.306834874914186","260186112","1261203456"
"2026-11-04T11:47:24.9164457+08:00","1.806148004210234","260268032","1261203456"
"2026-11-04T11:47:25.9363736+08:00","1.3230658933654214","260268032","1261203456"
"2026-11-04T11:47:26.9602353+08:00","1.040512511690699","260268032","1261203456"
"2026-11-04T11:47:27.9805145+08:00","2.366776395395228","260268032","1261359104"
"2026-11-04T11:47:29.0010086+08:00","1.252735406220468","260415488","1261359104"
"2026-11-04T11:47:30.0177082+08:00","1.257410832962943","260505600","1261436928"
"2026-11-04T11:47:31.0384144+08:00","0.8349833941174526","260538368","1261436928"
"2026-11-04T11:47:32.0576986+08:00","1.881333622520232","260538368","1261436928"
"2026-11-04T11:47:33.0775248+08:00","1.9499757543357523","260743168","1261633536"
"2026-11-04T11:47:34.0986188+08:00","1.460665984451258","260734976","1261633536"
"2026-11-04T11:47:35.1199407+08:00","1.3908000459547039","260734976","1261633536"
"2026-11-04T11:47:36.1420548+08:00","1.5981804059573461","260808704","1261633536"
"2026-11-04T11:47:37.1564286+08:00","1.6803918383395298","260800512","1261633536"
"2026-11-04T11:47:38.1750047+08:00","0.7670020924307963","260800512","1261633536"
"2026-11-04T11:47:39.1899448+08:00","1.6094769802402402","261406720","1261912064"
"2026-11-04T11:47:40.2137103+08:00","1.3181063614488064","261443584","1261912064"
"2026-11-04T11:47:41.2324594+08:00","1.1851655757402522","261443584","1261912064"
"2026-11-04T11:47:42.2421593+08:00","1.6178299386508082","261443584","1261912064"
"2026-11-04T11:47:43.2537130+08:00","1.685076585202995","262328320","1262641152"
"2026-11-04T11:47:44.2698736+08:00","1.677437065111021","261599232","1261912064"
"2026-11-04T11:47:45.2881434+08:00","1.1857234336482962","261599232","1261912064"
"2026-11-04T11:47:46.3063976+08:00","1.4647396227064642","261599232","1261912064"
"2026-11-04T11:47:47.3263614+08:00","1.1837541328784058","261623808","1261912064"
"2026-11-04T11:47:48.3498061+08:00","0.9715407015329522","261632000","1261912064"
"2026-11-04T11:47:49.3582456+08:00","2.1832787643230414","261758976","1262014464"
"2026-11-04T11:47:50.3694431+08:00","1.54519764932172","261754880","1262014464"
"2026-11-04T11:47:51.3813761+08:00","1.1229633151242586","261754880","1262014464"
"2026-11-04T11:47:52.4018732+08:00","1.4615203440825775","261754880","1262014464"
"2026-11-04T11:47:53.4267827+08:00","1.3859316802649848","261771264","1262014464"
"2026-11-04T11:47:54.4439627+08:00","1.2568169752738856","261767168","1262014464"
"2026-11-04T11:47:55.4628269+08:00","1.9518168992848737","261779456","1262014464"
"2026-11-04T11:47:56.4714542+08:00","1.1970589767264517","261763072","1261977600"
"2026-11-04T11:47:57.4935542+08:00","1.0423059476478906","262492160","1262706688"
"2026-11-04T11:47:58.5124154+08:00","1.3941590330994502","261787648","1261977600"
"2026-11-04T11:47:59.5327488+08:00","1.3225400816848867","261787648","1261977600"
"2026-11-04T11:48:00.5514799+08:00","1.5337707860297976","261808128","1262014464"
"2026-11-04T11:48:01.5607586+08:00","1.5481353168356768","262537216","1262743552"
"2026-11-04T11:48:02.5820154+08:00","1.0431665268626942","261808128","1262014464"
"2026-11-04T11:48:03.6049850+08:00","1.5968438624889023","262381568","1262616576"
"2026-11-04T11:48:04.6288555+08:00","1.3179711869634083","262381568","1262616576"
"2026-11-04T11:48:05.6464037+08:00","1.465755895128381","262381568","1262616576"
"2026-11-04T11:48:06.6527508+08:00","0.9880469490280063","262381568","1262616576"
"2026-11-04T11:48:07.6748345+08:00","1.0423225701485201","262381568","1262616576"
"2026-11-04T11:48:08.6861407+08:00","1.1938880268274472","262381568","1262616576"
"2026-11-04T11:48:09.7002925+08:00","1.7507913330313882","262381568","1262616576"
"2026-11-04T11:48:10.7215653+08:00","1.7385836397661643","262381568","1262616576"
"2026-11-04T11:48:11.7448694+08:00","1.2492953862972804","262381568","1262616576"
"2026-11-04T11:48:12.7644547+08:00","1.3235104686011248","262381568","1262616576"
"2026-11-04T11:48:13.7724342+08:00","1.2682887805844176","262381568","1262616576"
"2026-11-04T11:48:14.7926073+08:00","1.2531295825277993","262381568","1262616576"
"2026-11-04T11:48:15.8090611+08:00","1.5372071017885909","262385664","1262616576"
"2026-11-04T11:48:16.8254857+08:00","1.5372512629072537","262381568","1262616576"
"2026-11-04T11:48:17.8313291+08:00","0.9885417370320089","262381568","1262616576"
"2026-11-04T11:48:18.8503103+08:00","1.7424935630001632","262381568","1262616576"
"2026-11-04T11:48:19.8704521+08:00","1.1139271387209468","262381568","1262616576"
"2026-11-04T11:48:20.8896175+08:00","1.2543686146616542","262381568","1262616576"
"2026-11-04T11:48:21.9064682+08:00","1.1175324326015967","262381568","1262616576"
"2026-11-04T11:48:22.9272476+08:00","1.2523852762987684","262381568","1262616576"
"2026-11-04T11:48:23.9481807+08:00","2.0174280674307563","262381568","1262616576"
"2026-11-04T11:48:24.9682256+08:00","1.3925411964263","262381568","1262616576"
"2026-11-04T11:48:25.9777910+08:00","1.125596852233284","262389760","1262616576"
"2026-11-04T11:48:26.9871037+08:00","1.3369809160053354","262381568","1262616576"
"2026-11-04T11:48:28.0049057+08:00","1.3956098980494687","262381568","1262616576"
"2026-11-04T11:48:29.0240559+08:00","1.533140061200008","262402048","1262616576"
"2026-11-04T11:48:30.0333748+08:00","1.125871750111522","262402048","1262616576"
"2026-11-04T11:48:31.0481984+08:00","1.469691158864726","262402048","1262616576"
"2026-11-04T11:48:32.0697904+08:00","0.9037810148723312","262402048","1262616576"
"2026-11-04T11:48:33.0923670+08:00","1.8058215972191314","262402048","1262616576"
"2026-11-04T11:48:34.1137237+08:00","1.1821397594360166","262402048","1262616576"
"2026-11-04T11:48:35.1331560+08:00","1.2540402054252064","262402048","1262616576"
"2026-11-04T11:48:36.1422739+08:00","1.7595250087409828","262402048","1262616576"
"2026-11-04T11:48:37.1496777+08:00","1.4100150758360703","262402048","1262616576"
"2026-11-04T11:48:38.1707315+08:00","1.2520487078242997","262402048","1262616576"
"2026-11-04T11:48:39.1876578+08:00","1.3269711071311836","262402048","1262616576"
"2026-11-04T11:48:40.2029496+08:00","1.5389664330983466","262402048","1262616576"
"2026-11-04T11:48:41.2153935+08:00","1.402995805944947","262402048","1262616576"
"2026-11-04T11:48:42.2301652+08:00","0.9798442169979531","262402048","1262616576"
"2026-11-04T11:48:43.2477815+08:00","1.1166916610549933","262402048","1262616576"
"2026-11-04T11:48:44.2561686+08:00","1.1269121117908354","262402048","1262616576"
"2026-11-04T11:48:45.2746694+08:00","0.9065240346845624","262402048","1262616576"
"2026-11-04T11:48:46.2842734+08:00","1.8290249534380898","262402048","1262616576"
"2026-11-04T11:48:47.2922514+08:00","1.5501330386179062","262402048","1262616576"
"2026-11-04T11:48:48.3042676+08:00","0.9123326825652144","262520832","1262616576"
"2026-11-04T11:48:49.3150477+08:00","1.0539789110320918","261443584","1261518848"
"2026-11-04T11:48:50.3351898+08:00","0.9050655340520254","261439488","1261518848"
"2026-11-04T11:48:51.3524695+08:00","0.907612188216726","262168576","1262247936"
"2026-11-04T11:48:52.3653454+08:00","0.9115583207631404","261439488","1261518848"
"2026-11-04T11:48:53.3832281+08:00","0.6977496254993555","261439488","1261518848"
"2026-11-04T11:48:54.3982266+08:00","0.8396788047201323","261439488","1261518848"
"2026-11-04T11:48:55.4052849+08:00","0.3526247054054729","261439488","1261518848"
"2026-11-04T11:48:56.4222185+08:00","1.0476012485878223","261439488","1261518848"
"2026-11-04T11:48:57.4327942+08:00","0.4919563085764787","261439488","1261518848"
"2026-11-04T11:48:58.4509263+08:00","1.0463680588117288","261439488","1261518848"
"2026-11-04T11:48:59.4674727+08:00","0.48906679607452347","261439488","1261518848"
"2026-11-04T11:49:00.4864361+08:00","0.9061124811209652","261439488","1261518848"
"2026-11-04T11:49:01.5042577+08:00","0.5582332092203763","261439488","1261518848"
"2026-11-04T11:49:02.5218476+08:00","1.1167206321167658","261439488","1261518848"
"2026-11-04T11:49:03.5327996+08:00","0.4917731909221119","261439488","1261518848"
"2026-11-04T11:49:04.5439569+08:00","0.5619123930389646","261087232","1261228032"

~~~~

### composition-r382-reentry-stdout.log

~~~~text
SDP:
v=0
o=- 0 0 IN IP4 127.0.0.1
s=Naraka
t=0 0
a=tool:libavformat 62.19.101
m=video 61020 RTP/AVP 96
c=IN IP4 127.0.0.1
b=AS:8013
a=framerate:30
a=rtpmap:96 H264/90000
a=fmtp:96 packetization-mode=1; sprop-parameter-sets=Z01AH5WQBQBbsBEAAAMD6AAA6mDgAAAehIAAA9CQbvKI+OFSQA==,aOuPIA==; profile-level-id=4D401F
m=audio 61022 RTP/AVP 97
c=IN IP4 127.0.0.1
b=AS:274
a=rtpmap:97 MPEG4-GENERIC/44100/2
a=fmtp:97 profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3; config=121056E500


~~~~

### composition-r382-reentry.log

~~~~text
frame= 3527 fps= 30 q=-1.0 size=  116664KiB time=00:01:57.58 bitrate=8127.8kbits/s speed=   1x elapsed=0:01:57.07    
frame= 3543 fps= 30 q=-1.0 size=  117108KiB time=00:01:58.10 bitrate=8123.2kbits/s speed=   1x elapsed=0:01:57.58    
frame= 3558 fps= 30 q=-1.0 size=  117493KiB time=00:01:58.60 bitrate=8115.0kbits/s speed=   1x elapsed=0:01:58.10    
frame= 3574 fps= 30 q=-1.0 size=  118047KiB time=00:01:59.13 bitrate=8117.3kbits/s speed=   1x elapsed=0:01:58.61    
frame= 3589 fps= 30 q=-1.0 size=  118528KiB time=00:01:59.63 bitrate=8116.3kbits/s speed=   1x elapsed=0:01:59.13    
[out#0/rtp @ 000002093688c1c0] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
[out#1/rtp @ 00000209386bdb80] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.49    
~~~~

### composition-r382-source0-stdout.log

~~~~text
SDP:
v=0
o=- 0 0 IN IP4 127.0.0.1
s=Naraka
t=0 0
a=tool:libavformat 62.19.101
m=video 61020 RTP/AVP 96
c=IN IP4 127.0.0.1
b=AS:8013
a=framerate:30
a=rtpmap:96 H264/90000
a=fmtp:96 packetization-mode=1; sprop-parameter-sets=Z01AH5WQBQBbsBEAAAMD6AAA6mDgAAAehIAAA9CQbvKI+OFSQA==,aOuPIA==; profile-level-id=4D401F
m=audio 61022 RTP/AVP 97
c=IN IP4 127.0.0.1
b=AS:274
a=rtpmap:97 MPEG4-GENERIC/44100/2
a=fmtp:97 profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3; config=121056E500


~~~~

### composition-r382-source0.log

~~~~text
frame= 3530 fps= 30 q=-1.0 size=  116709KiB time=00:01:57.66 bitrate=8125.3kbits/s speed=   1x elapsed=0:01:57.16    
frame= 3546 fps= 30 q=-1.0 size=  117146KiB time=00:01:58.20 bitrate=8119.0kbits/s speed=   1x elapsed=0:01:57.68    
frame= 3561 fps= 30 q=-1.0 size=  117603KiB time=00:01:58.70 bitrate=8116.3kbits/s speed=   1x elapsed=0:01:58.19    
frame= 3577 fps= 30 q=-1.0 size=  118184KiB time=00:01:59.23 bitrate=8119.9kbits/s speed=   1x elapsed=0:01:58.71    
frame= 3592 fps= 30 q=-1.0 size=  118576KiB time=00:01:59.74 bitrate=8112.0kbits/s speed=   1x elapsed=0:01:59.22    
[out#0/rtp @ 000001e1928bb740] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
[out#1/rtp @ 000001e19294bc40] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.49    
~~~~

### composition-r382-source1-stdout.log

~~~~text
SDP:
v=0
o=- 0 0 IN IP4 127.0.0.1
s=Naraka
c=IN IP4 127.0.0.1
t=0 0
a=tool:libavformat 62.19.101
m=video 61030 RTP/AVP 96
b=AS:8013
a=framerate:30
a=rtpmap:96 H264/90000
a=fmtp:96 packetization-mode=1; sprop-parameter-sets=Z01AH5WQBQBbsBEAAAMD6AAA6mDgAAAehIAAA9CQbvKI+OFSQA==,aOuPIA==; profile-level-id=4D401F


~~~~

### composition-r382-source1.log

~~~~text
frame= 3512 fps= 30 q=-1.0 size=  116092KiB time=00:01:57.06 bitrate=8123.8kbits/s speed=   1x elapsed=0:01:56.54    
frame= 3527 fps= 30 q=-1.0 size=  116664KiB time=00:01:57.56 bitrate=8129.1kbits/s speed=   1x elapsed=0:01:57.06    
frame= 3542 fps= 30 q=-1.0 size=  117090KiB time=00:01:58.10 bitrate=8122.0kbits/s speed=   1x elapsed=0:01:57.58    
frame= 3558 fps= 30 q=-1.0 size=  117493KiB time=00:01:58.60 bitrate=8115.6kbits/s speed=   1x elapsed=0:01:58.09    
frame= 3573 fps= 30 q=-1.0 size=  118036KiB time=00:01:59.10 bitrate=8118.8kbits/s speed=   1x elapsed=0:01:58.61    
frame= 3589 fps= 30 q=-1.0 size=  118528KiB time=00:01:59.63 bitrate=8116.3kbits/s speed=   1x elapsed=0:01:59.12    
[out#0/rtp @ 0000020ad04d7cc0] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
~~~~

### composition-r382-source2-stdout.log

~~~~text
SDP:
v=0
o=- 0 0 IN IP4 127.0.0.1
s=Naraka
c=IN IP4 127.0.0.1
t=0 0
a=tool:libavformat 62.19.101
m=video 61040 RTP/AVP 96
b=AS:8013
a=framerate:30
a=rtpmap:96 H264/90000
a=fmtp:96 packetization-mode=1; sprop-parameter-sets=Z01AH5WQBQBbsBEAAAMD6AAA6mDgAAAehIAAA9CQbvKI+OFSQA==,aOuPIA==; profile-level-id=4D401F


~~~~

### composition-r382-source2.log

~~~~text
frame= 3514 fps= 30 q=-1.0 size=  116207KiB time=00:01:57.13 bitrate=8127.2kbits/s speed=   1x elapsed=0:01:56.63    
frame= 3529 fps= 30 q=-1.0 size=  116696KiB time=00:01:57.63 bitrate=8126.7kbits/s speed=   1x elapsed=0:01:57.14    
frame= 3546 fps= 30 q=-1.0 size=  117146KiB time=00:01:58.20 bitrate=8119.0kbits/s speed=   1x elapsed=0:01:57.67    
frame= 3561 fps= 30 q=-1.0 size=  117603KiB time=00:01:58.70 bitrate=8116.3kbits/s speed=   1x elapsed=0:01:58.19    
frame= 3577 fps= 30 q=-1.0 size=  118184KiB time=00:01:59.23 bitrate=8119.9kbits/s speed=   1x elapsed=0:01:58.71    
frame= 3592 fps= 30 q=-1.0 size=  118576KiB time=00:01:59.73 bitrate=8112.8kbits/s speed=   1x elapsed=0:01:59.22    
[out#0/rtp @ 000001a409f51680] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.47    
~~~~

### composition-r382-vlc.log

~~~~text
-- logger module started --
main debug: VLC media player - 3.0.23 Vetinari
main debug: Copyright © 1996-2025 the VideoLAN team
main debug: revision 3.0.23-2-0-g79128878dd
main debug: configured with /builds/videolan/vlc/extras/package/win32/../../../configure  '--enable-update-check' '--enable-lua' '--enable-faad' '--enable-flac' '--enable-theora' '--enable-avcodec' '--enable-merge-ffmpeg' '--enable-mpc' '--enable-libass' '--enable-schroedinger' '--enable-realrtsp' '--enable-live555' '--enable-shout' '--enable-goom' '--enable-sse' '--enable-mmx' '--enable-libcddb' '--enable-zvbi' '--disable-telx' '--enable-nls' '--host=i686-w64-mingw32' '--with-contrib=../contrib/i686-w64-mingw32' '--with-breakpad=https://win.crashes.videolan.org' '--enable-qt' '--enable-skins2' '--enable-dvdread' '--enable-caca' 'host_alias=i686-w64-mingw32' 'CFLAGS= -D__MSVCRT_VERSION__=0x700 -D_WIN32_WINNT=0x0502 -DWINVER=0x0502 ' 'CPPFLAGS= -D__MSVCRT_VERSION__=0x700 -D_WIN32_WINNT=0x0502 -DWINVER=0x0502' 'CXXFLAGS= -D__MSVCRT_VERSION__=0x700 -D_WIN32_WINNT=0x0502 -DWINVER=0x0502 ' 'PKG_CONFIG=pkg-config' 'PKG_CONFIG_LIBDIR=/usr/i686-w64-mingw32/lib/pkgconfig:/usr/lib/i686-w64-mingw32/pkgconfig'
main debug: using multimedia timers as clock source
main debug:  min period: 1 ms, max period: 1000000 ms
main debug: searching plug-in modules
main debug: loading plugins cache file D:\VideoLAN\VLC\plugins\plugins.dat
main debug: recursively browsing `D:\VideoLAN\VLC\plugins'
main debug: plug-ins loaded: 495 modules
main debug: opening config file (C:\Users\ROG\AppData\Roaming\vlc\vlcrc)
main debug: looking for logger module matching "any": 2 candidates
file debug: opening logfile `D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r382-vlc.log'
main debug: using logger module "file"
main debug: translation test: code is "C"
main debug: looking for keystore module matching "memory": 3 candidates
main debug: using keystore module "memory"
main debug: CPU has capabilities MMX MMXEXT SSE SSE2 SSE3 SSSE3 SSE4.1 SSE4.2 FPU 
main debug: Creating an input for 'Media Library'
main debug: Input is a meta file: disabling unneeded options
main debug: using timeshift granularity of 50 MiB
main debug: using timeshift path: C:\Users\ROG\AppData\Local\Temp
main debug: `file/directory:///C:/Users/ROG/AppData/Roaming/vlc/ml.xspf' gives access `file' demux `directory' path `/C:/Users/ROG/AppData/Roaming/vlc/ml.xspf'
main debug: creating demux: access='file' demux='directory' location='/C:/Users/ROG/AppData/Roaming/vlc/ml.xspf' file='C:\Users\ROG\AppData\Roaming\vlc\ml.xspf'
main debug: looking for access_demux module matching "file": 15 candidates
main debug: no access_demux modules matched
main debug: creating access: file:///C:/Users/ROG/AppData/Roaming/vlc/ml.xspf
main debug:  (path: C:\Users\ROG\AppData\Roaming\vlc\ml.xspf)
main debug: looking for access module matching "file": 27 candidates
main debug: using access module "filesystem"
main debug: looking for stream_filter module matching "prefetch,cache_read": 24 candidates
cache_read debug: Using stream method for AStream*
cache_read debug: starting pre-buffering
cache_read debug: received first data after 0 ms
cache_read debug: pre-buffering done 304 bytes in 0s - 296875 KiB/s
main debug: using stream_filter module "cache_read"
main debug: looking for stream_filter module matching "any": 24 candidates
playlist debug: using XSPF playlist reader
main debug: using stream_filter module "playlist"
main debug: stream filter added to 0190e660
main debug: looking for stream_filter module matching "any": 24 candidates
main debug: no stream_filter modules matched
main debug: looking for stream_directory module matching "any": 1 candidates
main debug: no stream_directory modules matched
main debug: attachment of directory-extractor failed for file:///C:/Users/ROG/AppData/Roaming/vlc/ml.xspf
main debug: looking for stream_filter module matching "record": 24 candidates
main debug: using stream_filter module "record"
main debug: creating demux: access='file' demux='directory' location='/C:/Users/ROG/AppData/Roaming/vlc/ml.xspf' file='C:\Users\ROG\AppData\Roaming\vlc\ml.xspf'
main debug: looking for demux module matching "directory": 56 candidates
main debug: using demux module "directory"
main debug: looking for meta reader module matching "any": 2 candidates
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\meta\reader
lua debug: Trying Lua scripts in D:\VideoLAN\VLC\lua\meta\reader
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\reader\filename.luac
main debug: no meta reader modules matched
main debug: `file/directory:///C:/Users/ROG/AppData/Roaming/vlc/ml.xspf' successfully opened
main debug: looking for xml reader module matching "any": 1 candidates
main debug: using xml reader module "xml"
main debug: EOF reached
main debug: removing module "directory"
main debug: removing module "record"
main debug: removing module "playlist"
main debug: removing module "cache_read"
main debug: removing module "filesystem"
main debug: creating audio output
main debug: looking for audio output module matching "any": 6 candidates
mmdevice debug: using default device
mmdevice debug: display name changed: VLC media player (LibVLC 3.0.23)
mmdevice debug: version 2 session control unavailable
mmdevice debug: volume from -65.250000 dB to +0.000000 dB with 0.031250 dB increments
main debug: using audio output module "mmdevice"
main debug: keeping audio output
main debug: looking for interface module matching "rc,none": 16 candidates
oldrc debug: base: 127.0.0.1, port: 63020
main debug: net: listening to 127.0.0.1 port 63020
main debug: using interface module "oldrc"
main debug: looking for interface module matching "hotkeys,none": 16 candidates
main debug: using interface module "hotkeys"
main debug: looking for interface module matching "globalhotkeys,none": 16 candidates
main debug: using interface module "win32"
main: Running vlc with the default interface. Use 'cvlc' to use vlc without interface.
main debug: looking for interface module matching "any": 16 candidates
main debug: looking for extension module matching "any": 1 candidates
lua debug: Opening Lua Extension module
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\extensions
lua debug: Trying Lua scripts in D:\VideoLAN\VLC\lua\extensions
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\extensions\VLSub.luac
lua debug: Scanning Lua script D:\VideoLAN\VLC\lua\extensions\VLSub.luac
lua debug: Script D:\VideoLAN\VLC\lua\extensions\VLSub.luac has the following capability flags: 0x5
main debug: using extension module "lua"
main debug: using interface module "qt"
main debug: processing request item: null, node: Playlist, skip: 0
main debug: rebuilding array of current - root Playlist
main debug: rebuild done - 1 items, index -1
main debug: starting playback of new item
main debug: resyncing on udp://127.0.0.1:62024
main debug: udp://127.0.0.1:62024 is at 0
main debug: creating new input thread
main debug: Creating an input for 'udp://127.0.0.1:62024'
main debug: requesting art for new input thread
main debug: using timeshift granularity of 50 MiB
main debug: using timeshift path: C:\Users\ROG\AppData\Local\Temp
main debug: `udp://@127.0.0.1:62024' gives access `udp' demux `any' path `@127.0.0.1:62024'
main debug: creating demux: access='udp' demux='any' location='@127.0.0.1:62024' file='\\@127.0.0.1:62024'
qt debug: IM: Setting an input
main debug: looking for access_demux module matching "udp": 15 candidates
main debug: no access_demux modules matched
main debug: creating access: udp://@127.0.0.1:62024
main debug:  (path: \\@127.0.0.1:62024)
main debug: looking for access module matching "udp": 27 candidates
main debug: looking for meta fetcher module matching "any": 1 candidates
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\meta\fetcher
lua debug: Trying Lua scripts in D:\VideoLAN\VLC\lua\meta\fetcher
main debug: no meta fetcher modules matched
main debug: looking for art finder module matching "any": 2 candidates
udp debug: opening server=:0 local=127.0.0.1:62024
main debug: net: opening 127.0.0.1 datagram port 62024
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\meta\art
lua debug: Trying Lua scripts in D:\VideoLAN\VLC\lua\meta\art
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\art\00_musicbrainz.luac
main debug: using access module "udp"
main debug: looking for stream_filter module matching "prefetch,cache_block": 24 candidates
lua debug: skipping script (unmatched scope) D:\VideoLAN\VLC\lua\meta\art\00_musicbrainz.luac
prefetch debug: using 16777216 bytes buffer, 16777216 bytes read
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\art\01_googleimage.luac
main debug: using stream_filter module "prefetch"
main debug: looking for stream_filter module matching "any": 24 candidates
lua debug: skipping script (unmatched scope) D:\VideoLAN\VLC\lua\meta\art\01_googleimage.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\art\02_frenchtv.luac
lua debug: skipping script (unmatched scope) D:\VideoLAN\VLC\lua\meta\art\02_frenchtv.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\art\03_lastfm.luac
lua debug: skipping script (unmatched scope) D:\VideoLAN\VLC\lua\meta\art\03_lastfm.luac
main debug: no art finder modules matched
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\playlist
lua debug: Trying Lua scripts in D:\VideoLAN\VLC\lua\playlist
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\anevia_streams.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\anevia_xml.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\appletrailers.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\bbc_co_uk.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\cue.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\dailymotion.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\jamendo.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\koreus.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\liveleak.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\newgrounds.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\rockbox_fm_presets.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\soundcloud.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\twitch.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\vimeo.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\vocaroo.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\playlist\youtube.luac
main debug: no stream_filter modules matched
main debug: looking for stream_directory module matching "any": 1 candidates
main debug: no stream_directory modules matched
main debug: attachment of directory-extractor failed for udp://@127.0.0.1:62024
main debug: looking for stream_filter module matching "record": 24 candidates
main debug: using stream_filter module "record"
main debug: creating demux: access='udp' demux='any' location='@127.0.0.1:62024' file='\\@127.0.0.1:62024'
main debug: looking for demux module matching "any": 56 candidates
main debug: looking for xml reader module matching "any": 1 candidates
main debug: using xml reader module "xml"
webvtt debug: subtitle demux discarded
ts debug: Standard set to Auto
main debug: using demux module "ts"
ts debug: DEMUX_SET_GROUP 0 00000000
main debug: looking for meta reader module matching "any": 2 candidates
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\meta\reader
lua debug: Trying Lua scripts in D:\VideoLAN\VLC\lua\meta\reader
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\reader\filename.luac
main debug: no meta reader modules matched
main debug: `udp://@127.0.0.1:62024' successfully opened
ts debug: PATCallBack called
ts debug: new PAT ts_id=1 version=0 current_next=1
ts debug:   * number=1 pid=256
ts debug: temporary receiving program 1
ts debug: PMTCallBack called for program 1
ts debug: new PMT program number=1 version=0 pid_pcr=257
ts debug:   * pid=257 type=0x24 ISO/IEC Reserved
ts debug:    => pid 257 has now es fcc=hevc
main debug: selecting program id=1
main debug: looking for packetizer module matching "any": 25 candidates
main debug: using packetizer module "hevc"
main debug: looking for video decoder module matching "any": 19 candidates
avcodec debug: using ffmpeg Lavc58.134.100
avcodec debug: CPU flags: 0x000fd3db
avcodec debug: allowing 10 thread(s) for decoding
avcodec debug: codec (hevc) started
avcodec debug: using frame thread mode with 10 threads
main debug: using video decoder module "avcodec"
ts debug: Default program is 1
ts debug:   * pid=258 type=0xf ISO/IEC 13818-7 Audio with ADTS transport
ts debug:      - SL/FMC descriptor not found/matched
ts debug:    => pid 258 has now es fcc=mp4a
main debug: looking for packetizer module matching "any": 25 candidates
mpeg4audio debug: running MPEG4 audio packetizer
mpeg4audio debug: ADTS Mode
main debug: using packetizer module "mpeg4audio"
main debug: looking for audio decoder module matching "any": 21 candidates
main debug: using audio decoder module "faad"
ts debug:   * pid=17 listening for SDT
ts debug: enabling pid 257 from program 1
ts debug: enabling pid 258 from program 1
ts debug: enabling pcr pid 257 from program 1
main debug: Buffering 0%
main debug: Buffering 6%
mpeg4audio: AAC channels: 2 samplerate: 44100
main debug: restarting module due to input format change
main debug: removing module "faad"
main debug: looking for audio decoder module matching "any": 21 candidates
main debug: using audio decoder module "faad"
faad warning: decoded zero sample
main debug: Buffering 13%
main debug: reusing audio output
main debug: looking for aout stream module matching "any": 2 candidates
main debug: restarting module due to input format change
main debug: removing module "avcodec"
main debug: looking for video decoder module matching "any": 19 candidates
avcodec debug: using ffmpeg Lavc58.134.100
avcodec debug: CPU flags: 0x000fd3db
avcodec debug: allowing 10 thread(s) for decoding
avcodec debug: codec (hevc) started
avcodec debug: using frame thread mode with 10 threads
main debug: using video decoder module "avcodec"
avcodec debug: available hardware decoder output format 53 (dxva2_vld)
avcodec debug: available hardware decoder output format 118 (d3d11va_vld)
avcodec debug: available hardware decoder output format 174 (d3d11)
avcodec debug: available software decoder output format 0 (yuv420p)
avcodec debug: trying format d3d11va_vld
main debug: looking for text renderer module matching "any": 3 candidates
freetype debug: Using DWrite backend
freetype debug: DWrite_GetFamily(): family name: Arial
freetype debug: DWrite_ParseFamily(): using font at index 0 with weight 400 for bold: 0, italic: 0
freetype debug: DWrite_ParseFamily(): using font at index 1 with weight 700 for bold: 1, italic: 0
freetype debug: DWrite_ParseFamily(): using font at index 2 with weight 400 for bold: 0, italic: 1
freetype debug: DWrite_ParseFamily(): using font at index 3 with weight 700 for bold: 1, italic: 1
main debug: using text renderer module "freetype"
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 32x32 (32x32) chroma: YUVA -> 16x16 (16x16) chroma: RGBA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: looking for video converter module matching "any": 24 candidates
yuvp debug: YUVP to YUVA converter
main debug: using video converter module "yuvp"
main debug: Deinterlacing available
main debug: deinterlace -1, mode auto, is_needed 0
main debug: looking for vout window module matching "qt,any": 3 candidates
qt debug: requesting video window...
main debug: resized to 84590848x64528928
main debug: resized to 84590848x64528928
main debug: resized to 84590592x64528928
main debug: resized to 84590592x64528928
main debug: resized to 84590080x64528928
main debug: resized to 100x30
main debug: resized to 100x30
main debug: resized to 100x30
main debug: resized to 100x30
main debug: resized to 100x30
main debug: resized to 100x30
main debug: resized to 1121x833
main debug: resized to 1121x833
main debug: resized to 1121x833
main debug: resized to 1121x833
main debug: resized to 1121x833
main debug: resized to 1121x833
main debug: resized to 1121x833
main debug: resized to 1280x720
main debug: resized to 1280x720
main debug: resized to 1280x720
main debug: resized to 1280x720
main debug: resized to 1280x720
main debug: resized to 1280x720
main debug: resized to 1280x720
main debug: resized to 1280x720
main debug: resized to 1280x720
main debug: using vout window module "qt"
main debug: looking for inhibit module matching "any": 1 candidates
main debug: using inhibit module "inhibit"
main debug: Opening vout display wrapper
main debug: looking for vout display module matching "any": 12 candidates
direct3d11 debug: creating Vout EventThread
direct3d11 debug: Win32VoutCreateWindow
main debug: VoutDisplayEvent 'resize' 1280x720
qt debug: Logical video size: 1280x720
main debug: resized to 1280x720
main debug: VoutDisplayEvent 'resize' 1280x720
main debug: Buffering 19%
direct3d11 debug: inclinometer not found. (hr=0x80070490)
direct3d11 debug: created video sub-window
direct3d11 debug: Vout EventThread running
wasapi debug: modified format
main debug: Buffering 26%
main debug: Buffering 33%
direct3d11 debug: Created the D3D11 device type 1 level b100.
direct3d11 debug: supports colorspace RGB Rec.709 gamma:22 range:FULL
direct3d11 debug: supports colorspace RGB Rec.709 gamma:22 range:STUDIO
direct3d11 debug: supports colorspace RGB Rec.2020 gamma:22 range:STUDIO
direct3d11 debug: supports colorspace RGB Rec.2020 gamma:22 range:FULL
direct3d11 debug: supports colorspace RGB Rec.2020 gamma:2084 range:FULL
direct3d11 debug: supports colorspace RGB Rec.2020 gamma:2084 range:STUDIO
direct3d11 debug: Output max luminance: 400.0, colorspace RGB Rec.709 gamma:22 range:FULL, bits per pixel 10
direct3d11 debug: using colorspace RGB Rec.709 gamma:22 range:FULL
direct3d11 debug: Using pixel format VA_NV12 for chroma DX11
main debug: VoutDisplayEvent 'resize' 1302x776
main debug: Buffering 39%
direct3d11 debug: using GPU render fence
direct3d11 debug: Direct3D11 resources created
direct3d11 debug: Direct3D11 device adapter successfully initialized
direct3d11 debug: Direct3D11 Open Succeeded
main debug: using vout display module "direct3d11"
direct3d11 debug: D3D11 pool succeed with 48 surfaces (1280x736) context 0x0b2c00e0
main debug: original format sz 1280x736, of (0,0), vsz 1280x720, 4cc DX11, sar 1:1, msk r0x0 g0x0 b0x0
main debug: removing module "freetype"
main debug: looking for text renderer module matching "any": 3 candidates
main debug: VoutDisplayEvent 'resize' 1280x720
direct3d11 debug: Detected size change 1280x720
freetype debug: Using DWrite backend
freetype debug: DWrite_GetFamily(): family name: Arial
freetype debug: DWrite_ParseFamily(): using font at index 0 with weight 400 for bold: 0, italic: 0
freetype debug: DWrite_ParseFamily(): using font at index 1 with weight 700 for bold: 1, italic: 0
freetype debug: DWrite_ParseFamily(): using font at index 2 with weight 400 for bold: 0, italic: 1
freetype debug: DWrite_ParseFamily(): using font at index 3 with weight 700 for bold: 1, italic: 1
main debug: using text renderer module "freetype"
main debug: looking for hw decoder module matching "any": 2 candidates
d3d11va debug: Reusing Direct3D11 device
d3d11va debug: CreateDevice succeed
d3d11va warning: surface dimensions (1280x768) differ from avcodec dimensions (1280x736)
d3d11va debug: - 'MPEG-2 & MPEG-1 variable-length decoder' is supported
d3d11va debug: - 'MPEG-2 variable-length decoder' is supported
d3d11va debug: - 'MPEG-1 variable-length decoder, no D pictures' is supported
d3d11va debug: - 'VC-1 variable-length decoder' is supported
d3d11va debug: - 'VC-1 variable-length decoder' is supported
d3d11va debug: - 'Unknown decoder 0x32fcfe3f-0xde46-0x4a49-0x861bac71110649d5' is supported
d3d11va debug: - 'H.264 stereo high profile, mbs flag set' is supported
d3d11va debug: - 'H.264 stereo high profile' is supported
d3d11va debug: - 'H.264 variable-length decoder, no film grain technology' is supported
d3d11va debug: - 'HEVC Main profile' is supported
d3d11va debug: - 'HEVC Main 10 profile' is supported
d3d11va debug: - 'Unknown decoder 0x1a72925f-0x0c2c-0x4f15-0x96fbb17d1473603f' is supported
qt debug: Logical video size: 1280x720
d3d11va debug: - 'Unknown decoder 0x15df9b21-0x06c4-0x47f1-0x841ea67c97d7f312' is supported
d3d11va debug: - 'Unknown decoder 0x4008018f-0xf537-0x4b36-0x98cf61af8a2c1a33' is supported
main debug: resized to 1280x720
d3d11va debug: - 'Unknown decoder 0x0dabeffa-0x4458-0x4602-0xbc030795659d617c' is supported
main debug: VoutDisplayEvent 'resize' 1280x720
d3d11va debug: - 'Unknown decoder 0x9798634d-0xfe9d-0x48e5-0xb4dadbec45b3df01' is supported
d3d11va debug: - 'MPEG-4 Part 2 variable-length decoder, Simple Profile' is supported
d3d11va debug: - 'MPEG-4 Part 2 variable-length decoder, Simple&Advanced Profile, no GMC' is supported
d3d11va debug: - 'MPEG-4 Part 2 nVidia bitstream decoder' is supported
d3d11va debug: - 'Unknown decoder 0x33fcfe41-0xde46-0x4a49-0x861bac71110649d5' is supported
d3d11va debug: - 'VP9 profile 0' is supported
d3d11va debug: - 'VP9 profile 2' is supported
d3d11va debug: - 'Unknown decoder 0xdda19dc7-0x93b5-0x49f5-0xa9b32bda28a2ce6e' is supported
d3d11va debug: - 'AV1 Main profile 8' is supported
d3d11va debug: - 'Unknown decoder 0x6affd11e-0x1d96-0x42b1-0xa21593a31f09a53d' is supported
d3d11va debug: - 'Unknown decoder 0x914c84a3-0x4078-0x4fa9-0x984ce2f262cb5c9c' is supported
d3d11va debug: - 'Unknown decoder 0x8a1a1031-0x29bc-0x46d0-0xa007e9b092ca6767' is supported
d3d11va debug: Trying to use 'HEVC Main profile' as input
d3d11va debug: NV12 output is supported for decoder HEVC Main profile.
d3d11va debug: Using output format NV12 for decoder HEVC Main profile
d3d11va debug: va_pool_SetupDecoder id 173 1280x736 count: 28
d3d11va warning: not enough decoding slices in the texture (6/28)
d3d11va debug: ID3D11VideoDecoderOutputView succeed with 28 surfaces (1280x768)
d3d11va debug: we got 2 decoder configurations
d3d11va debug: configuration[0] ConfigBitstreamRaw 1
d3d11va debug: configuration[1] ConfigBitstreamRaw 1
d3d11va debug: DxCreateDecoderSurfaces succeed
main debug: using hw decoder module "d3d11va"
avcodec: Using D3D11VA (NVIDIA GeForce RTX 4060 Laptop GPU, vendor 10de(NVIDIA), device 28e0, revision a1) for hardware decoding
main debug: Received first picture
main debug: Buffering 46%
main debug: Buffering 53%
main debug: Buffering 59%
wasapi debug: buffer size    : 96000 frames
wasapi debug: maximum latency: 000 ns
wasapi debug: default period : 10000000 ns
wasapi debug: minimum period : 3000000 ns
main debug: using aout stream module "wasapi"
main debug: output 'f32l' 48000 Hz Stereo frame=1 samples/8 bytes
main debug: looking for audio volume module matching "any": 2 candidates
main debug: using audio volume module "float_mixer"
main debug: input 'f32l' 44100 Hz Stereo frame=1 samples/8 bytes
main debug: looking for audio filter module matching "scaletempo": 16 candidates
scaletempo debug: format: 44100 rate, 2 nch, 4 bps, fl32
scaletempo debug: params: 30 stride, 0.200 overlap, 14 search
scaletempo debug: 1.000 scale, 1323.000 stride_in, 1323 stride_out, 1059 standing, 264 overlap, 617 search, 2204 queue, fl32 mode
main debug: using audio filter module "scaletempo"
main debug: conversion: 'f32l'->'f32l' 44100 Hz->44100 Hz Stereo->Stereo
main debug: conversion pipeline complete
main debug: conversion: 'f32l'->'f32l' 44100 Hz->44100 Hz Stereo->Stereo
main debug: conversion pipeline complete
main debug: looking for audio resampler module matching "any": 3 candidates
main debug: using audio resampler module "samplerate"
main debug: Buffering 66%
main debug: Buffering 73%
main debug: Buffering 79%
main debug: Buffering 86%
main debug: Buffering 93%
main debug: Buffering 99%
main debug: Stream buffering done (1066 ms in 932 ms)
main debug: Decoder wait done in 0 ms
main debug: inserting 9594 zeroes
mmdevice debug: state changed: 1
main warning: playback too late (82200): up-sampling
main debug: auto hiding mouse cursor
main warning: picture is too late to be displayed (missing 33 ms)
main debug: picture might be displayed late (missing 0 ms)
main debug: resampling stopped (drift: -14225 us)
main debug: accepted socket 3280 (from socket 1176)
main debug: looking for encoder module matching "any": 24 candidates
main debug: using encoder module "png"
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to build resize+chroma
main debug: looking for video converter module matching "any": 24 candidates
main debug: no video converter modules matched
main error: Failed to create video converter
chain debug: Trying to build chroma+resize
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (1b1873c8) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (05cfeda0) appended to chain
main debug: using video converter module "chain"
main debug: Filter 'chain' (05c20ac8) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: RV24 -> 1280x720 (1280x720) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (05946490) appended to chain
main debug: using video converter module "chain"
main debug: removing module "png"
main debug: removing module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 1b1873c8 removed from chain
main debug: removing module "swscale"
main debug: Filter 05cfeda0 removed from chain
main debug: Filter 05c20ac8 removed from chain
main debug: removing module "swscale"
main debug: Filter 05946490 removed from chain
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r382-native-2026-11-04-11h45m00s844.png)
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (05c20ac8) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: YUVA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (1b1873c8) appended to chain
main debug: using video converter module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 05c20ac8 removed from chain
main debug: removing module "swscale"
main debug: Filter 1b1873c8 removed from chain
main error: read error: No error
main debug: accepted socket 3484 (from socket 1176)
main debug: looking for encoder module matching "any": 24 candidates
main debug: using encoder module "png"
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to build resize+chroma
main debug: looking for video converter module matching "any": 24 candidates
main debug: no video converter modules matched
main error: Failed to create video converter
chain debug: Trying to build chroma+resize
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (05ad7c70) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (05ad61e0) appended to chain
main debug: using video converter module "chain"
main debug: Filter 'chain' (05ad79c8) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: RV24 -> 1280x720 (1280x720) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (05ad7f18) appended to chain
main debug: using video converter module "chain"
main debug: removing module "png"
main debug: removing module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 05ad7c70 removed from chain
main debug: removing module "swscale"
main debug: Filter 05ad61e0 removed from chain
main debug: Filter 05ad79c8 removed from chain
main debug: removing module "swscale"
main debug: Filter 05ad7f18 removed from chain
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r382-native-2026-11-04-11h46m43s020.png)
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (05ad7478) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: YUVA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (05ad6f28) appended to chain
main debug: using video converter module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 05ad7478 removed from chain
main debug: removing module "swscale"
main debug: Filter 05ad6f28 removed from chain
main debug: accepted socket 3380 (from socket 1176)
main debug: looking for encoder module matching "any": 24 candidates
main debug: using encoder module "png"
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to build resize+chroma
main debug: looking for video converter module matching "any": 24 candidates
main debug: no video converter modules matched
main error: Failed to create video converter
chain debug: Trying to build chroma+resize
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (053a6af0) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (053a6d98) appended to chain
main debug: using video converter module "chain"
main debug: Filter 'chain' (053a55b0) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: RV24 -> 1280x720 (1280x720) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (053a5da8) appended to chain
main debug: using video converter module "chain"
main debug: removing module "png"
main debug: removing module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 053a6af0 removed from chain
main debug: removing module "swscale"
main debug: Filter 053a6d98 removed from chain
main debug: Filter 053a55b0 removed from chain
main debug: removing module "swscale"
main debug: Filter 053a5da8 removed from chain
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r382-native-2026-11-04-11h47m13s627.png)
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (053a55b0) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: YUVA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (053a65a0) appended to chain
main debug: using video converter module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 053a55b0 removed from chain
main debug: removing module "swscale"
main debug: Filter 053a65a0 removed from chain
main debug: accepted socket 3800 (from socket 1176)
main debug: looking for encoder module matching "any": 24 candidates
main debug: using encoder module "png"
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to build resize+chroma
main debug: looking for video converter module matching "any": 24 candidates
main debug: no video converter modules matched
main error: Failed to create video converter
chain debug: Trying to build chroma+resize
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (1be84320) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (1be83880) appended to chain
main debug: using video converter module "chain"
main debug: Filter 'chain' (1be84870) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: RV24 -> 1280x720 (1280x720) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (1be84b18) appended to chain
main debug: using video converter module "chain"
main debug: removing module "png"
main debug: removing module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 1be84320 removed from chain
main debug: removing module "swscale"
main debug: Filter 1be83880 removed from chain
main debug: Filter 1be84870 removed from chain
main debug: removing module "swscale"
main debug: Filter 1be84b18 removed from chain
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r382-native-2026-11-04-11h48m55s183.png)
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (1be84dc0) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: YUVA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (1be84b18) appended to chain
main debug: using video converter module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 1be84dc0 removed from chain
main debug: removing module "swscale"
main debug: Filter 1be84b18 removed from chain
main debug: accepted socket 1064 (from socket 1176)
main debug: exiting
main debug: exiting
main debug: no exit handler
main debug: removing all interfaces
main debug: removing module "qt"
main debug: deactivating the playlist
main debug: incoming request - stopping current input
prefetch debug: end of stream
ts debug: Can't read TS packet at 203245296
main debug: EOF reached
main error: read error: No error
main warning: picture is too late to be displayed (missing 36 ms)
main debug: picture might be displayed late (missing 3 ms)
main debug: killing decoder fourcc `hevc'
main debug: removing module "avcodec"
main debug: saving a free vout
main debug: reusing provided vout
main debug: removing module "hevc"
mmdevice debug: state changed: 0
wasapi debug: reset
main debug: killing decoder fourcc `mp4a'
main debug: removing module "faad"
wasapi debug: reset
main debug: removing module "samplerate"
main debug: removing module "scaletempo"
main debug: removing module "float_mixer"
main debug: keeping audio output
main debug: removing module "mpeg4audio"
main debug: removing module "ts"
main debug: Program doesn't contain anymore ES
main debug: removing module "record"
main debug: removing module "prefetch"
main debug: removing module "udp"
main debug: dead input
qt debug: IM: Deleting the input
main debug: nothing to play
main debug: destroying useless vout
main debug: removing module "direct3d11"
direct3d11 debug: Direct3D11 resources destroyed
direct3d11 debug: Direct3D11 device adapter closed
direct3d11 error: SetThumbNailClip failed: 0x800706f4
direct3d11 debug: Win32 Vout EventThread terminating
direct3d11 debug: Win32VoutCloseWindow
direct3d11 debug: WinProc WM_DESTROY
main debug: removing module "inhibit"
qt debug: releasing video...
qt debug: video widget is released
main debug: removing module "freetype"
main debug: removing module "yuvp"
main debug: removing module "swscale"
main debug: removing module "mmdevice"
qt debug: requesting exit...
qt debug: waiting for UI thread...
qt debug: video widget is orphaned
main debug: exiting
main debug: no exit handler
qt debug: QApp exec() finished
qt debug: video widget is orphaned
qt debug: Killing extension dialog provider
qt debug: ExtensionsDialogProvider is quitting...
main debug: removing module "lua"
main debug: removing module "win32"
main debug: removing module "hotkeys"
main debug: removing module "oldrc"
main debug: destroying
main debug: saving media library to file C:\Users\ROG\AppData\Roaming\vlc\ml.xspf.tmp6392
main debug: looking for playlist export module matching "export-xspf": 4 candidates
main debug: using playlist export module "export"
main debug: removing module "export"
main debug: deleting item `Media Library'
main debug: deleting item `udp://127.0.0.1:62024'
main debug: deleting item `Playlist'
main debug: removing module "memory"
-- logger module stopped --

~~~~

## 清理结果

已先归档上述证据，再核对16项文件路径及长度并逐项删除；本轮临时文件残留0。六个本轮PID均已退出，UDP62024及TCP63020无监听残留。指定120秒源保留，长度124427809字节。未运行远程测试，无本轮远程产物。
