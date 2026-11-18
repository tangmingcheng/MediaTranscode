# r383：registry slot 生命周期 Windows realtime 复验

规格沿用 r382：三路 H.264 输入，选择源2 AAC；HEVC CBR 8000 kbps、1280×720/30fps、AAC CBR 192 kbps → MPEG-TS/UDP。指定120秒连续源，三个初始源和重入源均不得循环或降规格。

本轮目标：验证移除共享 lease 控制块、opaque 内存释放后归还 slot 对已启用生产链路的影响。SourcePresentation 早期费用入口、完整准入、内容级A/V/静音及线速交付不因此自动通过。

## 启动命令

```powershell
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r383-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r383-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r383-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r383-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r383-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r383-source2.log' -PassThru
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r383 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r383-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r383-cli-error.log' -PassThru
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r383-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r383-native- --snapshot-format=png udp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
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
$samples | Export-Csv -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r383-monitor.csv' -NoTypeInformation -Encoding UTF8
$source0.WaitForExit()
$source1.WaitForExit()
$source2.WaitForExit()
Write-Output "SOURCE0_EXIT=$($source0.ExitCode) SOURCE1_EXIT=$($source1.ExitCode) SOURCE2_EXIT=$($source2.ExitCode)"


```

## 进程与当前观察

初始PID：源AV=37452、源纯视频1=34652、源纯视频2=36096、CLI=38996、VLC=13708。启动/采样会话44490。12:57:26.236 VLC原生快照已查看：左上、右上、左下三格源画面，右下黑。测试仍在进行，不能判定整轮通过。


## 原生快照与重入

快照均使用下列RC命令，每阶段运行一次；确认VLC PID后执行。12:59:27.478已查看全黑；13:00:04.846已查看左下所选源恢复、其余格黑。重入源PID40464，执行会话56629。

```powershell
$cli=Get-CimInstance Win32_Process -Filter 'ProcessId=38996'
if(-not $cli -or $cli.Name -ne 'media_transcode_realtime_video_cli.exe' -or $cli.CommandLine -notlike '*composition-r383*'){throw 'r383 CLI is not alive'}
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 13708){throw 'VLC owner mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
try{$s=$c.GetStream();$s.ReadTimeout=2000;$b=[Text.Encoding]::ASCII.GetBytes('snapshot'+[char]10);$s.Write($b,0,$b.Length);$buf=[byte[]]::new(4096);try{$n=$s.Read($buf,0,$buf.Length);[Text.Encoding]::UTF8.GetString($buf,0,$n)}catch{Write-Output $_.Exception.Message}}finally{$c.Dispose()}
Get-ChildItem -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance' -Filter 'composition-r383-native-*.png' | Select-Object FullName,LastWriteTime
```

重入实际命令：
```powershell
$reentry=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r383-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r383-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($reentry.Id)"
$reentry.WaitForExit()
Write-Output "REENTRY_EXIT=$($reentry.ExitCode)"
```

## 结束命令与结果

```powershell
$target=Get-CimInstance Win32_Process -Filter "ProcessId=38996"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r383*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R383Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R383Console]::FreeConsole() | Out-Null
if(-not [R383Console]::AttachConsole(38996)){throw 'AttachConsole failed'}
try {
[R383Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R383Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 38996 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R383Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R383Console]::FreeConsole() | Out-Null}
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 13708){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
try{$s=$c.GetStream();$b=[Text.Encoding]::ASCII.GetBytes('quit'+[char]10);$s.Write($b,0,$b.Length)}finally{$c.Dispose()}


```

四阶段原生快照均已查看；最后全黑为13:01:46.776。三初始源及重入源均完整120秒/3600帧退出0；CLI退出0，end_reason=caller_stop，不记为自然EOF。最终workers/errors/workerErrors/stalledIntervals/pressureFailures/droppedBuffers均0，payload当前字节/对象归零，222990次预约与释放匹配。

CLI内置CPU平均1.549571%、峰值4.556355%；工作集初始210739200、峰值271073280、最终262553600字节。外部1秒采样288条，CPU平均1.528120%、峰值3.332976%，采样工作集峰值271077376字节；采样窗口不同，不混用两种峰值。重入generation2 raw_phase观察为13606ns，不等于内容级A/V证明。

sender提交174438个datagram、203955748 payload字节，deadline_misses/pressure/partial/ambiguous均0；最差提交lateness=5267840ns，sequence=2760，backlog最大49.1073ms。delivery_evidence仍not_proven，未抓包，不能声称wire交付或长期稳定性已通过。stderr帧属性警告如下保留。本轮通过四阶段行为及caller-stop资源排空复验，不是整项合屏或完整存储准入PASS。

## 归档运行证据

```text
[CLI] composition_inputs=3 grid=fixed audio_source=2 audio=on max_duration=source_driven hw=planner-highest-score
[2026-11-04 12:57:07.870] [info] [graph][planner.capability] composition_protocol_installed source=0
[2026-11-04 12:57:07.886] [info] [graph][planner.capability] composition_protocol_installed source=1
[2026-11-04 12:57:07.887] [info] [graph][planner.capability] composition_protocol_installed source=2
[2026-11-04 12:57:07.996] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-04 12:57:08.014] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-04 12:57:08.014] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-r383.source.0 decoder=h264
[2026-11-04 12:57:12.376] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-11-04 12:57:12.391] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-11-04 12:57:12.391] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-11-04 12:57:12.423] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-04 12:57:12.423] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-04 12:57:12.423] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-r383.source.1 decoder=h264
[2026-11-04 12:57:12.463] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-04 12:57:12.463] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-04 12:57:12.464] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-r383.source.2 decoder=h264
[2026-11-04 12:57:12.530] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-11-04 12:57:12.568] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-11-04 12:57:12.569] [info] [graph][planner.capability] canvas_cuda_stream stage=prepared device_stream=0000000000000000 private_nonblocking=1
[2026-11-04 12:57:12.575] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-04 12:57:12.576] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-04 12:57:12.576] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-11-04 12:57:12.581] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-04 12:57:12.581] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-04 12:57:12.581] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-11-04 12:57:12.587] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-04 12:57:12.587] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-04 12:57:12.587] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-11-04 12:57:12.591] [info] [graph][planner.capability] composition_preparation_storage stage=graph_bound scope=shared_reservations capacity=2147483648 retained=831586643 high_water=849033017
[CLI] audio_plan branch=transcode_frame reason=transcode_required_by_output_frame_contract codec=aac sample_rate=44100 channels=2 access_unit_samples=1024 encoder=aac bitrate_kbps=192
[CLI] prepared_output_id=1
[CLI] selected_chain=fixed-grid-composition score=0 decoder=h264,h264,h264 filter=scale_cuda=640:360,scale_cuda=640:360,scale_cuda=640:360 encoder=hevc_nvenc
[2026-11-04 12:57:12.594] [info] [graph][runtime.lifecycle] compile.begin
[2026-11-04 12:57:12.595] [info] [graph][runtime.channel] create channel=1 edge=1 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 12:57:12.596] [info] [graph][runtime.channel] create channel=2 edge=2 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 12:57:12.596] [info] [graph][runtime.channel] create channel=3 edge=3 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-04 12:57:12.596] [info] [graph][runtime.channel] create channel=4 edge=4 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.596] [info] [graph][runtime.channel] create channel=5 edge=5 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.596] [info] [graph][runtime.channel] create channel=6 edge=6 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.596] [info] [graph][runtime.channel] create channel=7 edge=7 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.596] [info] [graph][runtime.channel] create channel=8 edge=8 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.596] [info] [graph][runtime.channel] create channel=9 edge=9 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 12:57:12.596] [info] [graph][runtime.channel] create channel=10 edge=10 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.596] [info] [graph][runtime.channel] create channel=11 edge=11 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.596] [info] [graph][runtime.channel] create channel=12 edge=12 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 12:57:12.597] [info] [graph][runtime.channel] create channel=13 edge=13 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 12:57:12.597] [info] [graph][runtime.channel] create channel=14 edge=14 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.597] [info] [graph][runtime.channel] create channel=15 edge=15 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.597] [info] [graph][runtime.channel] create channel=16 edge=16 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.597] [info] [graph][runtime.channel] create channel=17 edge=17 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.597] [info] [graph][runtime.channel] create channel=18 edge=18 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.597] [info] [graph][runtime.channel] create channel=19 edge=19 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.597] [info] [graph][runtime.channel] create channel=20 edge=20 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.597] [info] [graph][runtime.channel] create channel=21 edge=21 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-04 12:57:12.597] [info] [graph][runtime.channel] create channel=22 edge=22 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 12:57:12.597] [info] [graph][runtime.channel] create channel=23 edge=23 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 12:57:12.597] [info] [graph][runtime.channel] create channel=24 edge=24 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 12:57:12.597] [info] [graph][runtime.channel] create channel=25 edge=25 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 12:57:12.598] [info] [graph][runtime.channel] create channel=26 edge=26 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 12:57:12.598] [info] [graph][runtime.channel] create channel=27 edge=27 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 12:57:12.598] [info] [graph][runtime.channel] create channel=28 edge=28 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 12:57:12.598] [info] [graph][runtime.channel] create channel=29 edge=29 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 12:57:12.598] [info] [graph][runtime.channel] create channel=30 edge=30 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.598] [info] [graph][runtime.channel] create channel=31 edge=31 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.598] [info] [graph][runtime.channel] create channel=32 edge=32 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.598] [info] [graph][runtime.channel] create channel=33 edge=33 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.598] [info] [graph][runtime.channel] create channel=34 edge=34 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.598] [info] [graph][runtime.channel] create channel=35 edge=35 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.598] [info] [graph][runtime.channel] create channel=36 edge=36 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 12:57:12.599] [info] [graph][runtime.channel] create channel=37 edge=37 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.599] [info] [graph][runtime.channel] create channel=38 edge=38 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.599] [info] [graph][runtime.channel] create channel=39 edge=39 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 12:57:12.599] [info] [graph][runtime.channel] create channel=40 edge=40 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 12:57:12.599] [info] [graph][runtime.channel] create channel=41 edge=41 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.599] [info] [graph][runtime.channel] create channel=42 edge=42 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.599] [info] [graph][runtime.channel] create channel=43 edge=43 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.599] [info] [graph][runtime.channel] create channel=44 edge=44 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.599] [info] [graph][runtime.channel] create channel=45 edge=45 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.599] [info] [graph][runtime.channel] create channel=46 edge=46 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.599] [info] [graph][runtime.channel] create channel=47 edge=47 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.599] [info] [graph][runtime.channel] create channel=48 edge=48 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-04 12:57:12.599] [info] [graph][runtime.channel] create channel=49 edge=49 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 12:57:12.599] [info] [graph][runtime.channel] create channel=50 edge=50 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 12:57:12.599] [info] [graph][runtime.channel] create channel=51 edge=51 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 12:57:12.599] [info] [graph][runtime.channel] create channel=52 edge=52 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 12:57:12.600] [info] [graph][runtime.channel] create channel=53 edge=53 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 12:57:12.600] [info] [graph][runtime.channel] create channel=54 edge=54 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 12:57:12.600] [info] [graph][runtime.channel] create channel=55 edge=55 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 12:57:12.600] [info] [graph][runtime.channel] create channel=56 edge=56 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 12:57:12.600] [info] [graph][runtime.channel] create channel=57 edge=57 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.600] [info] [graph][runtime.channel] create channel=58 edge=58 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.600] [info] [graph][runtime.channel] create channel=59 edge=59 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.600] [info] [graph][runtime.channel] create channel=60 edge=60 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.600] [info] [graph][runtime.channel] create channel=61 edge=61 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.600] [info] [graph][runtime.channel] create channel=62 edge=62 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.600] [info] [graph][runtime.channel] create channel=63 edge=63 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.600] [info] [graph][runtime.channel] create channel=64 edge=64 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.600] [info] [graph][runtime.channel] create channel=65 edge=65 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.600] [info] [graph][runtime.channel] create channel=66 edge=66 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 12:57:12.600] [info] [graph][runtime.channel] create channel=67 edge=67 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 12:57:12.601] [info] [graph][runtime.channel] create channel=68 edge=68 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.601] [info] [graph][runtime.channel] create channel=69 edge=69 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.601] [info] [graph][runtime.channel] create channel=70 edge=70 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.601] [info] [graph][runtime.channel] create channel=71 edge=71 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 12:57:12.601] [info] [graph][runtime.channel] create channel=72 edge=72 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 12:57:12.601] [info] [graph][runtime.channel] create channel=73 edge=73 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 12:57:12.601] [info] [graph][runtime.channel] create channel=74 edge=74 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 12:57:12.601] [info] [graph][runtime.channel] create channel=75 edge=75 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.601] [info] [graph][runtime.channel] create channel=76 edge=76 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.601] [info] [graph][runtime.channel] create channel=77 edge=77 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.601] [info] [graph][runtime.channel] create channel=78 edge=78 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.602] [info] [graph][runtime.channel] create channel=79 edge=79 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.602] [info] [graph][runtime.channel] create channel=80 edge=80 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.602] [info] [graph][runtime.channel] create channel=81 edge=81 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.602] [info] [graph][runtime.channel] create channel=82 edge=82 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.602] [info] [graph][runtime.channel] create channel=83 edge=83 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-04 12:57:12.602] [info] [graph][runtime.channel] create channel=84 edge=84 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 12:57:12.602] [info] [graph][runtime.channel] create channel=85 edge=85 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 12:57:12.602] [info] [graph][runtime.channel] create channel=86 edge=86 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 12:57:12.602] [info] [graph][runtime.channel] create channel=87 edge=87 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 12:57:12.602] [info] [graph][runtime.channel] create channel=88 edge=88 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 12:57:12.602] [info] [graph][runtime.channel] create channel=89 edge=89 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 12:57:12.602] [info] [graph][runtime.channel] create channel=90 edge=90 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 12:57:12.603] [info] [graph][runtime.channel] create channel=91 edge=91 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-04 12:57:12.603] [info] [graph][runtime.channel] create channel=92 edge=92 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 12:57:12.603] [info] [graph][runtime.channel] create channel=93 edge=93 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 12:57:12.603] [info] [graph][runtime.channel] create channel=94 edge=94 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 12:57:12.603] [info] [graph][runtime.channel] create channel=95 edge=95 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 12:57:12.603] [info] [graph][runtime.channel] create channel=96 edge=96 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 12:57:12.603] [info] [graph][runtime.channel] create channel=97 edge=97 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 12:57:12.603] [info] [graph][runtime.channel] create channel=98 edge=98 stream=Audio edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.603] [info] [graph][runtime.channel] create channel=99 edge=99 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 12:57:12.603] [info] [graph][runtime.channel] create channel=100 edge=100 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.603] [info] [graph][runtime.channel] create channel=101 edge=101 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0
[2026-11-04 12:57:12.604] [info] [graph][runtime.channel] create channel=102 edge=102 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 12:57:12.604] [info] [graph][runtime.channel] create channel=103 edge=103 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 12:57:12.604] [info] [graph][runtime.channel] create channel=104 edge=104 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 12:57:12.604] [info] [graph][runtime.channel] create channel=105 edge=105 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0
[2026-11-04 12:57:12.604] [info] [graph][runtime.channel] create channel=106 edge=106 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.604] [info] [graph][runtime.channel] create channel=107 edge=107 stream=Video edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-04 12:57:12.604] [info] [graph][runtime.channel] create channel=108 edge=108 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-04 12:57:12.604] [info] [graph][runtime.channel] create channel=109 edge=109 stream=Any edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-04 12:57:12.604] [info] [graph][runtime.channel] create channel=110 edge=110 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.604] [info] [graph][runtime.channel] create channel=111 edge=111 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-04 12:57:12.604] [info] [graph][runtime.channel] create channel=112 edge=112 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 12:57:12.604] [info] [graph][runtime.channel] create channel=113 edge=113 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 12:57:12.604] [info] [graph][runtime.channel] create channel=114 edge=114 stream=Any edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-04 12:57:12.605] [info] [graph][runtime.channel] create channel=115 edge=115 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 12:57:12.605] [info] [graph][runtime.channel] create channel=116 edge=116 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-04 12:57:12.605] [info] [graph][runtime.channel] create channel=117 edge=117 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-04 12:57:12.605] [info] [graph][runtime.channel] create channel=118 edge=118 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-04 12:57:12.605] [info] [graph][runtime.channel] create channel=119 edge=119 stream=Any edge_kind=EncodedPacket payload=Unknown queued=0
[2026-11-04 12:57:12.605] [info] [graph][runtime.channel] create channel=120 edge=120 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-04 12:57:12.605] [info] [graph][runtime.channel] create channel=121 edge=121 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-04 12:57:12.605] [info] [graph][runtime.channel] create channel=122 edge=122 stream=Metadata edge_kind=Unknown payload=Unknown queued=0
[2026-11-04 12:57:12.605] [info] [graph][runtime.channel] create channel=123 edge=123 stream=Metadata edge_kind=Unknown payload=Unknown queued=0
[2026-11-04 12:57:12.606] [info] [graph][runtime.lifecycle] distribution_registration scope=capability_probe stage=enter node=none pid=38996 tid=13896 native=0
[2026-11-04 12:57:12.606] [info] [graph][runtime.lifecycle] distribution_registration scope=capability_probe stage=leave node=none pid=38996 tid=13896 native=0
[2026-11-04 12:57:12.606] [info] [graph][runtime.lifecycle] compiled nodes=81 edges=123 channels=123 execution_order=1->3->8->27->46->47->9->21->28->40->63->48->69->10->29->49->11->12->30->31->50->51->52->13->32->53->14->17->33->36->54->55->59->15->34->56->57->16->35->58->18->37->60->19->38->61->20->39->62->22->41->64->70->23->42->65->71->24->43->66->72->26->45->68->73->25->44->67->6->2->7->80->4->77->5->74->75->78->76->79->81
[2026-11-04 12:57:12.606] [info] [graph][runtime.lifecycle] compile.done state=DefaultRegistrationPending
[2026-11-04 12:57:12.606] [info] [graph][runtime.node] register node=1 name=composition-r383.output.video.codec_resolver kind=CodecResolver
[2026-11-04 12:57:12.606] [info] [graph][runtime.node] register node=2 name=composition-r383.output.video.encode kind=VideoEncode
[2026-11-04 12:57:12.607] [info] [graph][runtime.node] register node=3 name=composition-r383.output.audio.codec_resolver kind=AudioCodecResolver
[2026-11-04 12:57:12.607] [info] [graph][runtime.node] register node=4 name=composition-r383.output.audio.encode kind=AudioEncode
[2026-11-04 12:57:12.607] [info] [graph][runtime.node] register node=5 name=composition-r383.output.audio.canonicalizer kind=EncodedAudioCanonicalizer
[2026-11-04 12:57:12.607] [info] [graph][runtime.node] register node=8 name=composition-r383.source_0.input kind=RawRtpInput
[2026-11-04 12:57:12.607] [info] [graph][runtime.node] register node=9 name=composition-r383.source_0.rtp.clock_group kind=RtpClockGroup
[2026-11-04 12:57:12.607] [info] [graph][runtime.node] register node=10 name=composition-r383.source_0.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-11-04 12:57:12.607] [info] [graph][runtime.node] register node=11 name=composition-r383.source_0.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-11-04 12:57:12.607] [info] [graph][runtime.node] register node=12 name=composition-r383.source_0.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-11-04 12:57:12.607] [info] [graph][runtime.node] register node=13 name=composition-r383.source_0.av_sync.source_clock kind=SourceClockStateFanout
[2026-11-04 12:57:12.607] [info] [graph][runtime.node] register node=14 name=composition-r383.source_0.av_sync.video.generation_gate kind=LockedPacketGate
[2026-11-04 12:57:12.607] [info] [graph][runtime.node] register node=15 name=composition-r383.source_0.av_sync.video.canonical_input kind=CanonicalInput
[2026-11-04 12:57:12.607] [info] [graph][runtime.node] register node=16 name=composition-r383.source_0.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-11-04 12:57:12.607] [info] [graph][runtime.node] register node=17 name=composition-r383.source_0.av_sync.startup.clock kind=AvStartupClock
[2026-11-04 12:57:12.608] [info] [graph][runtime.node] register node=18 name=composition-r383.source_0.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-11-04 12:57:12.608] [info] [graph][runtime.node] register node=20 name=composition-r383.source_0.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-11-04 12:57:12.608] [info] [graph][runtime.node] register node=21 name=composition-r383.source_0.video.codec_resolver kind=CodecResolver
[2026-11-04 12:57:12.608] [info] [graph][runtime.node] register node=22 name=composition-r383.source_0.video.decode kind=VideoDecode
[2026-11-04 12:57:12.608] [info] [graph][runtime.node] register node=23 name=composition-r383.source_0.video.hwtransfer kind=HardwareTransfer
[2026-11-04 12:57:12.608] [info] [graph][runtime.node] register node=24 name=composition-r383.source_0.video.framerate kind=VideoFrameRate
[2026-11-04 12:57:12.608] [info] [graph][runtime.node] register node=25 name=composition-r383.source_0.video.preparation kind=VideoPreparation
[2026-11-04 12:57:12.608] [info] [graph][runtime.node] register node=26 name=composition-r383.source_0.video.filter kind=VideoFilter
[2026-11-04 12:57:12.608] [info] [graph][runtime.node] register node=27 name=composition-r383.source_1.input kind=RawRtpInput
[2026-11-04 12:57:12.608] [info] [graph][runtime.node] register node=28 name=composition-r383.source_1.rtp.clock_group kind=RtpClockGroup
[2026-11-04 12:57:12.608] [info] [graph][runtime.node] register node=29 name=composition-r383.source_1.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-11-04 12:57:12.608] [info] [graph][runtime.node] register node=30 name=composition-r383.source_1.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-11-04 12:57:12.608] [info] [graph][runtime.node] register node=31 name=composition-r383.source_1.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-11-04 12:57:12.608] [info] [graph][runtime.node] register node=32 name=composition-r383.source_1.av_sync.source_clock kind=SourceClockStateFanout
[2026-11-04 12:57:12.608] [info] [graph][runtime.node] register node=33 name=composition-r383.source_1.av_sync.video.generation_gate kind=LockedPacketGate
[2026-11-04 12:57:12.609] [info] [graph][runtime.node] register node=34 name=composition-r383.source_1.av_sync.video.canonical_input kind=CanonicalInput
[2026-11-04 12:57:12.609] [info] [graph][runtime.node] register node=35 name=composition-r383.source_1.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-11-04 12:57:12.609] [info] [graph][runtime.node] register node=36 name=composition-r383.source_1.av_sync.startup.clock kind=AvStartupClock
[2026-11-04 12:57:12.609] [info] [graph][runtime.node] register node=37 name=composition-r383.source_1.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-11-04 12:57:12.609] [info] [graph][runtime.node] register node=39 name=composition-r383.source_1.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-11-04 12:57:12.609] [info] [graph][runtime.node] register node=40 name=composition-r383.source_1.video.codec_resolver kind=CodecResolver
[2026-11-04 12:57:12.609] [info] [graph][runtime.node] register node=41 name=composition-r383.source_1.video.decode kind=VideoDecode
[2026-11-04 12:57:12.609] [info] [graph][runtime.node] register node=42 name=composition-r383.source_1.video.hwtransfer kind=HardwareTransfer
[2026-11-04 12:57:12.609] [info] [graph][runtime.node] register node=43 name=composition-r383.source_1.video.framerate kind=VideoFrameRate
[2026-11-04 12:57:12.609] [info] [graph][runtime.node] register node=44 name=composition-r383.source_1.video.preparation kind=VideoPreparation
[2026-11-04 12:57:12.609] [info] [graph][runtime.node] register node=45 name=composition-r383.source_1.video.filter kind=VideoFilter
[2026-11-04 12:57:12.609] [info] [graph][runtime.node] register node=46 name=composition-r383.source_2.video.input kind=RawRtpInput
[2026-11-04 12:57:12.609] [info] [graph][runtime.node] register node=47 name=composition-r383.source_2.audio.input kind=RawRtpInput
[2026-11-04 12:57:12.609] [info] [graph][runtime.node] register node=48 name=composition-r383.source_2.rtp.clock_group kind=RtpClockGroup
[2026-11-04 12:57:12.609] [info] [graph][runtime.node] register node=49 name=composition-r383.source_2.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-11-04 12:57:12.610] [info] [graph][runtime.node] register node=50 name=composition-r383.source_2.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-11-04 12:57:12.610] [info] [graph][runtime.node] register node=51 name=composition-r383.source_2.av_sync.audio.protocol_binder kind=RtpPacketClockBinder
[2026-11-04 12:57:12.610] [info] [graph][runtime.node] register node=52 name=composition-r383.source_2.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-11-04 12:57:12.610] [info] [graph][runtime.node] register node=53 name=composition-r383.source_2.av_sync.source_clock kind=SourceClockStateFanout
[2026-11-04 12:57:12.610] [info] [graph][runtime.node] register node=54 name=composition-r383.source_2.av_sync.video.generation_gate kind=LockedPacketGate
[2026-11-04 12:57:12.610] [info] [graph][runtime.node] register node=55 name=composition-r383.source_2.av_sync.audio.generation_gate kind=LockedPacketGate
[2026-11-04 12:57:12.610] [info] [graph][runtime.node] register node=56 name=composition-r383.source_2.av_sync.video.canonical_input kind=CanonicalInput
[2026-11-04 12:57:12.610] [info] [graph][runtime.node] register node=57 name=composition-r383.source_2.av_sync.audio.canonical_input kind=CanonicalInput
[2026-11-04 12:57:12.610] [info] [graph][runtime.node] register node=58 name=composition-r383.source_2.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-11-04 12:57:12.610] [info] [graph][runtime.node] register node=59 name=composition-r383.source_2.av_sync.startup.clock kind=AvStartupClock
[2026-11-04 12:57:12.610] [info] [graph][runtime.node] register node=60 name=composition-r383.source_2.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-11-04 12:57:12.610] [info] [graph][runtime.node] register node=62 name=composition-r383.source_2.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-11-04 12:57:12.610] [info] [graph][runtime.node] register node=63 name=composition-r383.source_2.video.codec_resolver kind=CodecResolver
[2026-11-04 12:57:12.610] [info] [graph][runtime.node] register node=64 name=composition-r383.source_2.video.decode kind=VideoDecode
[2026-11-04 12:57:12.611] [info] [graph][runtime.node] register node=65 name=composition-r383.source_2.video.hwtransfer kind=HardwareTransfer
[2026-11-04 12:57:12.611] [info] [graph][runtime.node] register node=66 name=composition-r383.source_2.video.framerate kind=VideoFrameRate
[2026-11-04 12:57:12.611] [info] [graph][runtime.node] register node=67 name=composition-r383.source_2.video.preparation kind=VideoPreparation
[2026-11-04 12:57:12.611] [info] [graph][runtime.node] register node=68 name=composition-r383.source_2.video.filter kind=VideoFilter
[2026-11-04 12:57:12.611] [info] [graph][runtime.node] register node=69 name=composition-r383.source_2.audio.encode.codec_resolver kind=AudioCodecResolver
[2026-11-04 12:57:12.611] [info] [graph][runtime.node] register node=70 name=composition-r383.source_2.audio.encode.decode kind=AudioDecode
[2026-11-04 12:57:12.611] [info] [graph][runtime.node] register node=71 name=composition-r383.source_2.audio.encode.startup_trim kind=Unknown
[2026-11-04 12:57:12.611] [info] [graph][runtime.node] register node=72 name=composition-r383.source_2.audio.encode.drift_controller kind=AudioDriftController
[2026-11-04 12:57:12.611] [info] [graph][runtime.node] register node=73 name=composition-r383.source_2.audio.encode.resample kind=AudioResample
[2026-11-04 12:57:12.611] [info] [graph][runtime.node] register node=74 name=composition-r383.output.scheduler.scheduler kind=AvOutputScheduler
[2026-11-04 12:57:12.611] [info] [graph][runtime.node] register node=75 name=composition-r383.output.scheduler.router kind=ScheduledOutputRouter
[2026-11-04 12:57:12.611] [info] [graph][runtime.node] register node=76 name=composition-r383.output.mpegts.mux kind=FileMux
[2026-11-04 12:57:12.611] [info] [graph][runtime.node] register node=77 name=composition-r383.output.mpegts.plan.source kind=Unknown
[2026-11-04 12:57:12.611] [info] [graph][runtime.node] register node=78 name=composition-r383.output.mpegts.scheduled.adapter kind=Unknown
[2026-11-04 12:57:12.611] [info] [graph][runtime.node] register node=79 name=composition-r383.output.mpegts.datagram.materializer kind=MpegTsDatagramMaterializer
[2026-11-04 12:57:12.612] [info] [graph][runtime.node] register node=80 name=composition-r383.output.mpegts.transport.plan kind=DatagramTransportPlanSource
[2026-11-04 12:57:12.612] [info] [graph][runtime.node] register node=81 name=composition-r383.output.mpegts.transport.sender kind=ScheduledDatagramSender
[2026-11-04 12:57:12.612] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=waiting_for_random_access detail="" description=""
[2026-11-04 12:57:12.612] [info] [graph][runtime.lifecycle] start.begin mode=threaded
[2026-11-04 12:57:12.623] [info] [graph][runtime.lifecycle] distribution_registration scope=worker stage=enter node=81 pid=38996 tid=36124 native=0
[2026-11-04 12:57:12.623] [info] [graph][runtime.lifecycle] start.done state=ThreadedRunning
[2026-11-04 12:57:12.623] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:57:12.623] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:57:12.623] [info] [graph][runtime.lifecycle] worker.finished node=1
[2026-11-04 12:57:12.624] [info] [graph][runtime.node] worker.thread_cpu node=1 available=1 cpu_ns=0 process_calls=1
[2026-11-04 12:57:12.623] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:57:12.624] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-11-04 12:57:12.623] [info] [graph][runtime.node] video_encode.bind_encoder codec=hevc_nvenc pix_fmt=cuda rate_control=cbr bitrate=8000000 min_bitrate=8000000 max_bitrate=8000000 buffer_size=267000 fps=30/1 gop=60 frame_kind=hardware hwaccel=cuda hw_device_ctx=set hw_frames_ctx=set
[2026-11-04 12:57:12.623] [info] [graph][runtime.lifecycle] worker.finished node=21
[2026-11-04 12:57:12.624] [info] [graph][runtime.node] worker.thread_cpu node=21 available=1 cpu_ns=0 process_calls=1
[2026-11-04 12:57:12.624] [info] [graph][runtime.node] audio_encode_trace stage=codec_bound
[2026-11-04 12:57:12.624] [info] [graph][runtime.lifecycle] worker.finished node=69
[2026-11-04 12:57:12.623] [info] [graph][runtime.lifecycle] worker.finished node=63
[2026-11-04 12:57:12.624] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-11-04 12:57:12.623] [info] [graph][runtime.lifecycle] worker.finished node=40
[2026-11-04 12:57:12.625] [info] [graph][runtime.node] worker.thread_cpu node=40 available=1 cpu_ns=0 process_calls=1
[2026-11-04 12:57:12.624] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-11-04 12:57:12.624] [info] [graph][runtime.lifecycle] worker.finished node=3
[2026-11-04 12:57:12.625] [info] [graph][runtime.node] worker.thread_cpu node=69 available=1 cpu_ns=0 process_calls=1
[2026-11-04 12:57:12.625] [info] [graph][runtime.node] worker.thread_cpu node=63 available=1 cpu_ns=0 process_calls=1
[2026-11-04 12:57:12.625] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=15 generation=1
[2026-11-04 12:57:12.624] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=15 generation=1
[2026-11-04 12:57:12.625] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=15 generation=1
[2026-11-04 12:57:12.625] [info] [graph][runtime.node] worker.thread_cpu node=3 available=1 cpu_ns=0 process_calls=1
[2026-11-04 12:57:12.626] [info] [graph][runtime.node] source_video_trace node_id=39 node=MediaAvBoundReleaseExtractorNode stage=release source_identity=composition-r383.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=1296559725 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=15602364271400 group=composition-r383.source.1 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-4023000000 epoch_master_release_ns=5342561200 video_count=15 min_presentation_ns=-4023000000 max_end_ns=-3523000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.626] [info] [graph][runtime.node] source_video_trace node_id=39 node=MediaAvBoundReleaseExtractorNode stage=published source_identity=composition-r383.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=1296559725 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=15602364577000 group=composition-r383.source.1 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.626] [info] [graph][runtime.node] source_video_trace node_id=20 node=MediaAvBoundReleaseExtractorNode stage=release source_identity=composition-r383.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4021000000 duration_ns=33333333 decoded_sequence=unavailable pts=1365850933 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=15602364502500 group=composition-r383.source.0 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=-4021000000 epoch_master_release_ns=5342772000 video_count=15 min_presentation_ns=-4021000000 max_end_ns=-3521000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.626] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=1296559725 dts=1296559725 size=317016 generation=1 sequence=15
[2026-11-04 12:57:12.626] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=1365850933 dts=1365850933 size=317016 generation=1 sequence=15
[2026-11-04 12:57:12.626] [info] [graph][runtime.node] source_video_trace node_id=20 node=MediaAvBoundReleaseExtractorNode stage=published source_identity=composition-r383.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4021000000 duration_ns=33333333 decoded_sequence=unavailable pts=1365850933 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=15602364852700 group=composition-r383.source.0 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.626] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=release source_identity=composition-r383.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4022000000 duration_ns=33333333 decoded_sequence=unavailable pts=3245662357 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=15602364882700 group=composition-r383.source.2 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-4022000000 epoch_master_release_ns=5343168000 video_count=20 min_presentation_ns=-4022000000 max_end_ns=-3355333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.626] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=input source_identity=composition-r383.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=1296559725 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=15602364951300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.626] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=input source_identity=composition-r383.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4021000000 duration_ns=33333333 decoded_sequence=unavailable pts=1365850933 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=15602365058500 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.626] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=published source_identity=composition-r383.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4022000000 duration_ns=33333333 decoded_sequence=unavailable pts=3245662357 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=15602365206700 group=composition-r383.source.2 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.626] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=3245662357 dts=3245662357 size=317016 generation=1 sequence=15
[2026-11-04 12:57:12.627] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=input source_identity=composition-r383.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4022000000 duration_ns=33333333 decoded_sequence=unavailable pts=3245662357 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=15602365543600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.652] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-11-04 12:57:12.652] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=accepted source_identity=composition-r383.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=1296559725 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=15602391183500 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.653] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=1296559725 format=cuda size=1280x720 buffer_identity=0xb08600000 drm_prime=0 software_frame=0
[2026-11-04 12:57:12.653] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=resolved source_identity=composition-r383.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=1296559725 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=15602391486600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.653] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=published source_identity=composition-r383.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=1296559725 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=15602391673600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.653] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=00000211FBD11CD0 stream=Video payload=Frame pts=1296559725 dts=1296559725 duration=3000 tb=1/90000 flags=68
[2026-11-04 12:57:12.653] [info] [graph][runtime.node] source_video_trace node_id=42 node=HardwareTransferNode stage=published source_identity=composition-r383.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=1296559725 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=15602392146800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.653] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-11-04 12:57:12.653] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=00000211FBD11CD0 stream=Video payload=Frame pts=1296559725 dts=1296559725 duration=3000 tb=1/90000 flags=68
[2026-11-04 12:57:12.654] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=00000211FBD11CD0 stream=Video payload=Frame pts=1296559725 dts=1296559725 duration=3000 tb=1/90000 flags=68
[2026-11-04 12:57:12.654] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=accepted source_identity=composition-r383.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4021000000 duration_ns=33333333 decoded_sequence=unavailable pts=1365850933 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=15602392608900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.654] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-11-04 12:57:12.654] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=1365850933 format=cuda size=1280x720 buffer_identity=0xb08800000 drm_prime=0 software_frame=0
[2026-11-04 12:57:12.654] [info] [graph][runtime.node] source_video_trace node_id=43 node=VideoFrameRateNode stage=published source_identity=composition-r383.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=1296559725 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=15602393093100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.654] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=00000211FB341540 stream=Video payload=Frame pts=1296559725 dts=1296559725 duration=3000 tb=1/90000 flags=68
[2026-11-04 12:57:12.654] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=resolved source_identity=composition-r383.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4021000000 duration_ns=33333333 decoded_sequence=unavailable pts=1365850933 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=15602393208700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.655] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=00000211FB341540 stream=Video payload=Frame pts=1296559725 dts=1296559725 duration=3000 tb=1/90000 flags=68
[2026-11-04 12:57:12.655] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-11-04 12:57:12.655] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=published source_identity=composition-r383.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4021000000 duration_ns=33333333 decoded_sequence=unavailable pts=1365850933 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=15602393468800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.655] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=00000211FB342110 stream=Video payload=Frame pts=1365850933 dts=1365850933 duration=3000 tb=1/90000 flags=68
[2026-11-04 12:57:12.655] [info] [graph][runtime.node] source_video_trace node_id=23 node=HardwareTransferNode stage=published source_identity=composition-r383.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4021000000 duration_ns=33333333 decoded_sequence=unavailable pts=1365850933 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=15602394228100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.655] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=00000211FB342110 stream=Video payload=Frame pts=1365850933 dts=1365850933 duration=3000 tb=1/90000 flags=68
[2026-11-04 12:57:12.656] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=00000211FB342110 stream=Video payload=Frame pts=1365850933 dts=1365850933 duration=3000 tb=1/90000 flags=68
[2026-11-04 12:57:12.656] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-11-04 12:57:12.656] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=00000211FB340D60 stream=Video payload=Frame pts=1365850933 dts=1365850933 duration=3000 tb=1/90000 flags=68
[2026-11-04 12:57:12.656] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-11-04 12:57:12.656] [info] [graph][runtime.node] source_video_trace node_id=24 node=VideoFrameRateNode stage=published source_identity=composition-r383.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4021000000 duration_ns=33333333 decoded_sequence=unavailable pts=1365850933 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=15602394640100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.656] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=00000211FB340D60 stream=Video payload=Frame pts=1365850933 dts=1365850933 duration=3000 tb=1/90000 flags=68
[2026-11-04 12:57:12.657] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-11-04 12:57:12.657] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=accepted source_identity=composition-r383.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4022000000 duration_ns=33333333 decoded_sequence=unavailable pts=3245662357 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=15602396170000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.658] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=3245662357 format=cuda size=1280x720 buffer_identity=0xb08c00000 drm_prime=0 software_frame=0
[2026-11-04 12:57:12.658] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=resolved source_identity=composition-r383.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4022000000 duration_ns=33333333 decoded_sequence=unavailable pts=3245662357 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=15602396632200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.658] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=00000211FB342110 stream=Video payload=Frame pts=3245662357 dts=3245662357 duration=3000 tb=1/90000 flags=68
[2026-11-04 12:57:12.658] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=00000211FB342110 stream=Video payload=Frame pts=3245662357 dts=3245662357 duration=3000 tb=1/90000 flags=68
[2026-11-04 12:57:12.658] [info] [graph][runtime.node] source_video_trace node_id=65 node=HardwareTransferNode stage=published source_identity=composition-r383.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4022000000 duration_ns=33333333 decoded_sequence=unavailable pts=3245662357 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=15602397050800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.659] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=00000211FB342110 stream=Video payload=Frame pts=3245662357 dts=3245662357 duration=3000 tb=1/90000 flags=68
[2026-11-04 12:57:12.658] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-11-04 12:57:12.658] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=published source_identity=composition-r383.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4022000000 duration_ns=33333333 decoded_sequence=unavailable pts=3245662357 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=15602396932000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.659] [info] [graph][runtime.node] source_video_trace node_id=66 node=VideoFrameRateNode stage=published source_identity=composition-r383.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4022000000 duration_ns=33333333 decoded_sequence=unavailable pts=3245662357 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=15602397460500 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.659] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=00000211FB342500 stream=Video payload=Frame pts=3245662357 dts=3245662357 duration=3000 tb=1/90000 flags=68
[2026-11-04 12:57:12.659] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=00000211FB342500 stream=Video payload=Frame pts=3245662357 dts=3245662357 duration=3000 tb=1/90000 flags=68
[2026-11-04 12:57:12.660] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-11-04 12:57:12.711] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=55
[2026-11-04 12:57:12.711] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=56
[2026-11-04 12:57:12.712] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=input source_identity=composition-r383.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=1296559725 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=15602450714600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.712] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=51
[2026-11-04 12:57:12.712] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=input source_identity=composition-r383.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4021000000 duration_ns=33333333 decoded_sequence=unavailable pts=1365850933 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=15602450578700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.712] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=input source_identity=composition-r383.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4022000000 duration_ns=33333333 decoded_sequence=unavailable pts=3245662357 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=15602450929500 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.712] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=accepted source_identity=composition-r383.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=1296559725 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=15602451176400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.713] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=accepted source_identity=composition-r383.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4021000000 duration_ns=33333333 decoded_sequence=unavailable pts=1365850933 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=15602451417800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.713] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=resolved source_identity=composition-r383.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=432187 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=15602451473000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.713] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=resolved source_identity=composition-r383.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4021000000 duration_ns=33333333 decoded_sequence=unavailable pts=455284 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=15602451813400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.713] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=accepted source_identity=composition-r383.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4022000000 duration_ns=33333333 decoded_sequence=unavailable pts=3245662357 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=15602451936400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.713] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=published source_identity=composition-r383.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=432187 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=15602451982200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.713] [info] [graph][runtime.node] source_video_trace node_id=44 node=VideoPreparationNode stage=input source_identity=composition-r383.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=432187 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=15602452010600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.713] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=published source_identity=composition-r383.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4021000000 duration_ns=33333333 decoded_sequence=unavailable pts=455284 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=15602452056100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.713] [info] [graph][runtime.node] source_video_trace node_id=25 node=VideoPreparationNode stage=input source_identity=composition-r383.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4021000000 duration_ns=33333333 decoded_sequence=unavailable pts=455284 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=15602452069700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.713] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=resolved source_identity=composition-r383.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4022000000 duration_ns=33333333 decoded_sequence=unavailable pts=1081887 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=15602452213700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.714] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb0ae00000 drm_prime=0 software_frame=0
[2026-11-04 12:57:12.714] [info] [graph][runtime.node] source_video_trace node_id=44 node=VideoPreparationNode stage=output_attempt source_identity=composition-r383.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=432187 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=15602452384000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.714] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb0ae90000 drm_prime=0 software_frame=0
[2026-11-04 12:57:12.714] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=15 audio_count=0 first_video_sequence=15
[2026-11-04 12:57:12.714] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=published source_identity=composition-r383.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4022000000 duration_ns=33333333 decoded_sequence=unavailable pts=1081887 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=15602452757800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.715] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb0af20000 drm_prime=0 software_frame=0
[2026-11-04 12:57:12.714] [info] [graph][runtime.node] source_video_trace node_id=25 node=VideoPreparationNode stage=output_attempt source_identity=composition-r383.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4021000000 duration_ns=33333333 decoded_sequence=unavailable pts=455284 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=15602452544000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.714] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=input source_identity=composition-r383.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4022000000 duration_ns=33333333 decoded_sequence=unavailable pts=1081887 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=15602452768000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.715] [info] [graph][runtime.node] source_video_trace node_id=44 node=VideoPreparationNode stage=published source_identity=unavailable identity_link=node_generation_input_source_sequence generation=1 source_sequence=15 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=432187 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=15602453375300 group=composition-r383.source.1 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.715] [info] [graph][runtime.node] source_video_trace node_id=38 node=ActivationReleaseSequencer stage=activation source_identity=composition-r383.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=1296559725 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=15602453372800 group=composition-r383.source.1 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-4023000000 epoch_master_release_ns=5498275968 video_count=15 min_presentation_ns=-4023000000 max_end_ns=-3523000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.715] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=15 audio_count=0 first_video_sequence=15
[2026-11-04 12:57:12.715] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=output_attempt source_identity=composition-r383.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4022000000 duration_ns=33333333 decoded_sequence=unavailable pts=1081887 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=15602453887700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.715] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_candidate source_identity=composition-r383.source.1.input.video generation=1 steady_ns=15602454189300 source_sequence=15 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-04 12:57:12.715] [info] [graph][runtime.node] source_video_trace node_id=38 node=ActivationReleaseSequencer stage=release_published source_identity=composition-r383.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=1296559725 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=15602454188600 group=composition-r383.source.1 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-4023000000 epoch_master_release_ns=5498275968 video_count=15 min_presentation_ns=-4023000000 max_end_ns=-3523000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.715] [info] [graph][runtime.node] source_video_trace node_id=39 node=MediaAvBoundReleaseExtractorNode stage=release_committed source_identity=composition-r383.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=1296559725 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=15602454196100 group=composition-r383.source.1 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-4023000000 epoch_master_release_ns=5342561200 video_count=15 min_presentation_ns=-4023000000 max_end_ns=-3523000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=17 release_committed_count=1 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.716] [info] [graph][runtime.node] source_video_trace node_id=25 node=VideoPreparationNode stage=published source_identity=unavailable identity_link=node_generation_input_source_sequence generation=1 source_sequence=15 presentation_ns=-4021000000 duration_ns=33333333 decoded_sequence=unavailable pts=455284 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=15602454353000 group=composition-r383.source.0 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.716] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=20 audio_count=23 first_video_sequence=15
[2026-11-04 12:57:12.716] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=0
[2026-11-04 12:57:12.716] [info] [graph][runtime.node] source_video_trace node_id=19 node=ActivationReleaseSequencer stage=activation source_identity=composition-r383.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4021000000 duration_ns=33333333 decoded_sequence=unavailable pts=1365850933 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=15602454803600 group=composition-r383.source.0 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=-4021000000 epoch_master_release_ns=5499379468 video_count=15 min_presentation_ns=-4021000000 max_end_ns=-3521000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.717] [info] [graph][runtime.node] source_video_trace node_id=61 node=ActivationReleaseSequencer stage=activation source_identity=composition-r383.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4022000000 duration_ns=33333333 decoded_sequence=unavailable pts=3245662357 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=15602455385700 group=composition-r383.source.2 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-4022000000 epoch_master_release_ns=5500409768 video_count=20 min_presentation_ns=-4022000000 max_end_ns=-3355333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.717] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=published source_identity=unavailable identity_link=node_generation_input_source_sequence generation=1 source_sequence=15 presentation_ns=-4022000000 duration_ns=33333333 decoded_sequence=unavailable pts=1081887 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=15602455385600 group=composition-r383.source.2 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.717] [info] [graph][runtime.node] source_video_trace node_id=19 node=ActivationReleaseSequencer stage=release_published source_identity=composition-r383.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4021000000 duration_ns=33333333 decoded_sequence=unavailable pts=1365850933 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=15602455461700 group=composition-r383.source.0 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=-4021000000 epoch_master_release_ns=5499379468 video_count=15 min_presentation_ns=-4021000000 max_end_ns=-3521000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.717] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_candidate source_identity=composition-r383.source.0.input.video generation=1 steady_ns=15602455481800 source_sequence=15 presentation_ns=-4021000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-04 12:57:12.717] [info] [graph][runtime.node] source_video_trace node_id=20 node=MediaAvBoundReleaseExtractorNode stage=release_committed source_identity=composition-r383.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4021000000 duration_ns=33333333 decoded_sequence=unavailable pts=1365850933 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=15602455500500 group=composition-r383.source.0 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=-4021000000 epoch_master_release_ns=5342772000 video_count=15 min_presentation_ns=-4021000000 max_end_ns=-3521000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=17 release_committed_count=1 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.717] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=0
[2026-11-04 12:57:12.717] [info] [graph][runtime.node] audio_decode_trace stage=first_packet
[2026-11-04 12:57:12.717] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=release_committed source_identity=composition-r383.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4022000000 duration_ns=33333333 decoded_sequence=unavailable pts=3245662357 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=15602455666900 group=composition-r383.source.2 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-4022000000 epoch_master_release_ns=5343168000 video_count=20 min_presentation_ns=-4022000000 max_end_ns=-3355333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=20 release_count=22 release_committed_count=1 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.718] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=23
[2026-11-04 12:57:12.717] [info] [graph][runtime.node] source_video_trace node_id=61 node=ActivationReleaseSequencer stage=release_published source_identity=composition-r383.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4022000000 duration_ns=33333333 decoded_sequence=unavailable pts=3245662357 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=15602455570200 group=composition-r383.source.2 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-4022000000 epoch_master_release_ns=5500409768 video_count=20 min_presentation_ns=-4022000000 max_end_ns=-3355333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:57:12.718] [info] [graph][runtime.node] audio_decode_trace stage=first_frame samples=1024 rate=44100
[2026-11-04 12:57:12.717] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_candidate source_identity=composition-r383.source.2.input.video generation=1 steady_ns=15602455900600 source_sequence=15 presentation_ns=-4022000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-04 12:57:12.719] [info] [graph][runtime.node] audio_encode_trace stage=first_frame
[2026-11-04 12:57:12.719] [info] [graph][runtime.node] output_preparation stage=transport_activate generation=1 elapsed_ns=190800 planned_bytes=180767114966 planned_items=1 event=scope_exit
[2026-11-04 12:57:12.720] [info] [graph][runtime.node] output_preparation stage=global_sequence generation=1 elapsed_ns=54100 planned_bytes=68968 planned_items=2154 event=scope_exit
[2026-11-04 12:57:12.720] [info] [graph][runtime.node] output_preparation stage=transport_buffer generation=1 elapsed_ns=762400 planned_bytes=180767114966 planned_items=1 event=scope_exit
[2026-11-04 12:57:12.720] [info] [graph][runtime.node] output_preparation stage=ts_packetizer generation=1 elapsed_ns=104400 planned_bytes=180765122912 planned_items=61257 event=scope_exit
[2026-11-04 12:57:12.720] [info] [graph][runtime.node] output_preparation stage=ts_mux generation=1 elapsed_ns=286500 planned_bytes=945416 planned_items=59086 event=scope_exit
[2026-11-04 12:57:12.721] [info] [graph][runtime.node] mpegts_emission_plan generation=1 mode=canonical_access_unit_window access_unit_window_ns=100000000 video_initial_service_window_ns=13990650 audio_initial_service_window_ns=13990650 packet_size_bytes=188 maximum_payload_bytes=1316 per_datagram_overhead_bytes=0
[2026-11-04 12:57:12.721] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-r383.source.0.input.video generation=1 steady_ns=15602460043100 source_sequence=15 presentation_ns=-4021000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-04 12:57:12.721] [info] [graph][runtime.node] video_encode.input_contract format=cuda size=1280x720 buffer_identity=0xb0a000000 drm_prime=0 software_frame=0
[2026-11-04 12:57:12.721] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-r383.source.1.input.video generation=1 steady_ns=15602460229700 source_sequence=15 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-04 12:57:12.722] [info] [graph][runtime.node] video_encode.generation_start_key_frame pts=0 generation=1
[2026-11-04 12:57:12.722] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-r383.source.2.input.video generation=1 steady_ns=15602460436400 source_sequence=15 presentation_ns=-4022000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-04 12:57:12.722] [info] [graph][runtime.node] video_encode.first_frame pts=0 generation=attached
[2026-11-04 12:57:12.727] [info] [graph][runtime.node] video_encode.first_submit result=accepted
[2026-11-04 12:57:12.727] [info] [graph][runtime.node] video_encode.first_packet pts=0 dts=0 duration=0 size=108357 side_data_elems=1 received_footprint=108365 side_data[0]=Quality stats:8
[2026-11-04 12:57:12.727] [info] [graph][runtime.node] video_encode.first_packet_footprint bytes=108365 wrapped_size=108357 wrapped_side_data_elems=1
[2026-11-04 12:57:12.727] [info] [graph][runtime.node] av_scheduler_trace stage=first_video_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=5502912268 dispatch_master_ns=5502912268 now_ns=5344639700 node=74 identity_kind=output identity="composition-r383.output" canonical_sequence=1 canonical_created_steady_ns=15602466190500 head_observed_steady_ns=15602466223000 codec_receive_begin_steady_ns=15602465876400 codec_receive_end_steady_ns=15602465877200
[2026-11-04 12:57:12.728] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=5344779400 present_generation=1 present_canonical_target_ns=0 present_canonical_dispatch_ns=0 present_target_master_ns=5502912268 present_dispatch_master_ns=5502912268
[2026-11-04 12:57:12.767] [info] [graph][runtime.node] audio_encode_trace stage=first_packet
[2026-11-04 12:57:12.767] [info] [graph][runtime.node] av_scheduler_trace stage=first_audio_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=5502912268 dispatch_master_ns=5502912268 now_ns=5384636700 node=74 identity_kind=output identity="composition-r383.output" canonical_sequence=1 canonical_created_steady_ns=15602506196600 head_observed_steady_ns=15602506220000 codec_receive_begin_steady_ns=unavailable codec_receive_end_steady_ns=unavailable
[2026-11-04 12:57:12.936] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=running detail="" description=""
[2026-11-04 12:57:17.405] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2890748373 ntp_seconds=4002757037 ntp_fraction=1739461754 arrival_ns=15607144002400
[2026-11-04 12:57:17.406] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2890748373
[2026-11-04 12:57:17.405] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1655860580 ntp_seconds=4002757037 ntp_fraction=1739461754 arrival_ns=15607144030700
[2026-11-04 12:57:17.406] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1655860580
[2026-11-04 12:57:17.406] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:57:17.406] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:57:17.421] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3842679504 ntp_seconds=4002757037 ntp_fraction=1808181231 arrival_ns=15607159630600
[2026-11-04 12:57:17.421] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3842679504
[2026-11-04 12:57:17.421] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=834527275 ntp_seconds=4002757037 ntp_fraction=1808181231 arrival_ns=15607159599500
[2026-11-04 12:57:17.421] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:57:17.422] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=834527275
[2026-11-04 12:57:17.422] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:57:22.417] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1655860580 ntp_seconds=4002757042 ntp_fraction=1786706395 arrival_ns=15612155054400
[2026-11-04 12:57:22.417] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2890748373 ntp_seconds=4002757042 ntp_fraction=1786706395 arrival_ns=15612155054500
[2026-11-04 12:57:22.417] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2890748373
[2026-11-04 12:57:22.417] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1655860580
[2026-11-04 12:57:22.417] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:57:22.417] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:57:22.446] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=834527275 ntp_seconds=4002757042 ntp_fraction=1915555414 arrival_ns=15612185101800
[2026-11-04 12:57:22.447] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=834527275
[2026-11-04 12:57:22.448] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:57:22.447] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3842679504 ntp_seconds=4002757042 ntp_fraction=1915555414 arrival_ns=15612184935700
[2026-11-04 12:57:22.448] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3842679504
[2026-11-04 12:57:22.448] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:57:27.422] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2890748373 ntp_seconds=4002757047 ntp_fraction=1812476198 arrival_ns=15617160695600
[2026-11-04 12:57:27.422] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1655860580 ntp_seconds=4002757047 ntp_fraction=1812476198 arrival_ns=15617160857100
[2026-11-04 12:57:27.422] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1655860580
[2026-11-04 12:57:27.422] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2890748373
[2026-11-04 12:57:27.423] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:57:27.423] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:57:27.450] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=834527275 ntp_seconds=4002757047 ntp_fraction=1932735283 arrival_ns=15617188598100
[2026-11-04 12:57:27.450] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=834527275
[2026-11-04 12:57:27.450] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:57:27.466] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3842679504 ntp_seconds=4002757047 ntp_fraction=2001454759 arrival_ns=15617204390700
[2026-11-04 12:57:27.466] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3842679504
[2026-11-04 12:57:27.466] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:57:32.433] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2890748373 ntp_seconds=4002757052 ntp_fraction=1859720839 arrival_ns=15622171990700
[2026-11-04 12:57:32.434] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2890748373
[2026-11-04 12:57:32.433] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1655860580 ntp_seconds=4002757052 ntp_fraction=1859720839 arrival_ns=15622172035300
[2026-11-04 12:57:32.434] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1655860580
[2026-11-04 12:57:32.434] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:57:32.434] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:57:32.486] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=834527275 ntp_seconds=4002757052 ntp_fraction=2087354105 arrival_ns=15622224376300
[2026-11-04 12:57:32.486] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3842679504 ntp_seconds=4002757052 ntp_fraction=2083059138 arrival_ns=15622224290600
[2026-11-04 12:57:32.486] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3842679504
[2026-11-04 12:57:32.486] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=834527275
[2026-11-04 12:57:32.486] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:57:32.486] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:57:37.440] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2890748373 ntp_seconds=4002757057 ntp_fraction=1889785610 arrival_ns=15627179009300
[2026-11-04 12:57:37.440] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2890748373
[2026-11-04 12:57:37.441] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:57:37.440] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1655860580 ntp_seconds=4002757057 ntp_fraction=1889785610 arrival_ns=15627179134400
[2026-11-04 12:57:37.441] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1655860580
[2026-11-04 12:57:37.441] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:57:37.499] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=834527275 ntp_seconds=4002757057 ntp_fraction=2138893713 arrival_ns=15627237237100
[2026-11-04 12:57:37.499] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=834527275
[2026-11-04 12:57:37.499] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:57:37.512] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3842679504 ntp_seconds=4002757057 ntp_fraction=2194728288 arrival_ns=15627250352400
[2026-11-04 12:57:37.512] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3842679504
[2026-11-04 12:57:37.512] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:57:42.458] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2890748373 ntp_seconds=4002757062 ntp_fraction=1967095021 arrival_ns=15632196859100
[2026-11-04 12:57:42.458] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2890748373
[2026-11-04 12:57:42.459] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:57:42.473] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1655860580 ntp_seconds=4002757062 ntp_fraction=2031519531 arrival_ns=15632211575300
[2026-11-04 12:57:42.473] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1655860580
[2026-11-04 12:57:42.473] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:57:42.515] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=834527275 ntp_seconds=4002757062 ntp_fraction=2211908157 arrival_ns=15632253727200
[2026-11-04 12:57:42.515] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3842679504 ntp_seconds=4002757062 ntp_fraction=2211908157 arrival_ns=15632254005500
[2026-11-04 12:57:42.516] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3842679504
[2026-11-04 12:57:42.515] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=834527275
[2026-11-04 12:57:42.516] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:57:42.516] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:57:47.473] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2890748373 ntp_seconds=4002757067 ntp_fraction=2031519531 arrival_ns=15637211611300
[2026-11-04 12:57:47.473] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2890748373
[2026-11-04 12:57:47.473] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:57:47.502] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1655860580 ntp_seconds=4002757067 ntp_fraction=2156073582 arrival_ns=15637240670700
[2026-11-04 12:57:47.502] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1655860580
[2026-11-04 12:57:47.502] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:57:47.517] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=834527275 ntp_seconds=4002757067 ntp_fraction=2216203124 arrival_ns=15637255303400
[2026-11-04 12:57:47.517] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=834527275
[2026-11-04 12:57:47.517] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:57:47.517] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3842679504 ntp_seconds=4002757067 ntp_fraction=2220498092 arrival_ns=15637255639600
[2026-11-04 12:57:47.517] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3842679504
[2026-11-04 12:57:47.518] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:57:52.496] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2890748373 ntp_seconds=4002757072 ntp_fraction=2126008811 arrival_ns=15642234171900
[2026-11-04 12:57:52.496] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2890748373
[2026-11-04 12:57:52.496] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:57:52.522] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3842679504 ntp_seconds=4002757072 ntp_fraction=2241972928 arrival_ns=15642261104900
[2026-11-04 12:57:52.522] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=834527275 ntp_seconds=4002757072 ntp_fraction=2241972928 arrival_ns=15642261167000
[2026-11-04 12:57:52.523] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=834527275
[2026-11-04 12:57:52.523] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3842679504
[2026-11-04 12:57:52.523] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:57:52.523] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:57:52.534] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1655860580 ntp_seconds=4002757072 ntp_fraction=2293512536 arrival_ns=15642273137300
[2026-11-04 12:57:52.535] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1655860580
[2026-11-04 12:57:52.535] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:57:57.506] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2890748373 ntp_seconds=4002757077 ntp_fraction=2173253451 arrival_ns=15647245024700
[2026-11-04 12:57:57.507] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2890748373
[2026-11-04 12:57:57.507] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:57:57.538] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1655860580 ntp_seconds=4002757077 ntp_fraction=2310692405 arrival_ns=15647276623200
[2026-11-04 12:57:57.538] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1655860580
[2026-11-04 12:57:57.539] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:57:57.551] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=834527275 ntp_seconds=4002757077 ntp_fraction=2366526980 arrival_ns=15647290128300
[2026-11-04 12:57:57.552] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=834527275
[2026-11-04 12:57:57.552] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:57:57.552] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3842679504 ntp_seconds=4002757077 ntp_fraction=2366526980 arrival_ns=15647290181600
[2026-11-04 12:57:57.552] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3842679504
[2026-11-04 12:57:57.553] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:02.531] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2890748373 ntp_seconds=4002757082 ntp_fraction=2280627634 arrival_ns=15652269550700
[2026-11-04 12:58:02.531] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2890748373
[2026-11-04 12:58:02.532] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:02.547] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1655860580 ntp_seconds=4002757082 ntp_fraction=2345052143 arrival_ns=15652285112900
[2026-11-04 12:58:02.547] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1655860580
[2026-11-04 12:58:02.547] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:02.588] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=834527275 ntp_seconds=4002757082 ntp_fraction=2525440770 arrival_ns=15652326550200
[2026-11-04 12:58:02.588] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=834527275
[2026-11-04 12:58:02.588] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3842679504 ntp_seconds=4002757082 ntp_fraction=2525440770 arrival_ns=15652326482200
[2026-11-04 12:58:02.588] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:02.589] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3842679504
[2026-11-04 12:58:02.589] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:07.540] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2890748373 ntp_seconds=4002757087 ntp_fraction=2319282339 arrival_ns=15657279131500
[2026-11-04 12:58:07.541] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2890748373
[2026-11-04 12:58:07.541] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:07.571] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1655860580 ntp_seconds=4002757087 ntp_fraction=2452426326 arrival_ns=15657309439300
[2026-11-04 12:58:07.571] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1655860580
[2026-11-04 12:58:07.571] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:07.615] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3842679504 ntp_seconds=4002757087 ntp_fraction=2641404887 arrival_ns=15657353495500
[2026-11-04 12:58:07.615] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=834527275 ntp_seconds=4002757087 ntp_fraction=2641404887 arrival_ns=15657353552400
[2026-11-04 12:58:07.615] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=834527275
[2026-11-04 12:58:07.615] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3842679504
[2026-11-04 12:58:07.615] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:07.615] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:12.548] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2890748373 ntp_seconds=4002757092 ntp_fraction=2353642078 arrival_ns=15662287044600
[2026-11-04 12:58:12.549] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2890748373
[2026-11-04 12:58:12.549] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:12.606] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1655860580 ntp_seconds=4002757092 ntp_fraction=2602750181 arrival_ns=15662344805000
[2026-11-04 12:58:12.607] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1655860580
[2026-11-04 12:58:12.607] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:12.622] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=834527275 ntp_seconds=4002757092 ntp_fraction=2671469658 arrival_ns=15662360379600
[2026-11-04 12:58:12.622] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=834527275
[2026-11-04 12:58:12.622] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:12.622] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3842679504 ntp_seconds=4002757092 ntp_fraction=2671469658 arrival_ns=15662360568700
[2026-11-04 12:58:12.623] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3842679504
[2026-11-04 12:58:12.623] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:17.559] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2890748373 ntp_seconds=4002757097 ntp_fraction=2400886718 arrival_ns=15667297747300
[2026-11-04 12:58:17.559] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2890748373
[2026-11-04 12:58:17.559] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:17.632] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1655860580 ntp_seconds=4002757097 ntp_fraction=2714419331 arrival_ns=15667370740200
[2026-11-04 12:58:17.632] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1655860580
[2026-11-04 12:58:17.633] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:17.644] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=834527275 ntp_seconds=4002757097 ntp_fraction=2765958938 arrival_ns=15667382399300
[2026-11-04 12:58:17.644] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=834527275
[2026-11-04 12:58:17.644] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:17.659] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3842679504 ntp_seconds=4002757097 ntp_fraction=2830383448 arrival_ns=15667397665300
[2026-11-04 12:58:17.659] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3842679504
[2026-11-04 12:58:17.659] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:22.582] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2890748373 ntp_seconds=4002757102 ntp_fraction=2499670966 arrival_ns=15672321170600
[2026-11-04 12:58:22.583] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2890748373
[2026-11-04 12:58:22.583] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:22.653] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=834527275 ntp_seconds=4002757102 ntp_fraction=2804613644 arrival_ns=15672391892700
[2026-11-04 12:58:22.653] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=834527275
[2026-11-04 12:58:22.654] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:22.668] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3842679504 ntp_seconds=4002757102 ntp_fraction=2869038153 arrival_ns=15672406904400
[2026-11-04 12:58:22.669] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3842679504
[2026-11-04 12:58:22.669] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:22.680] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1655860580 ntp_seconds=4002757102 ntp_fraction=2920577761 arrival_ns=15672418492300
[2026-11-04 12:58:22.680] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1655860580
[2026-11-04 12:58:22.680] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:27.591] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2890748373 ntp_seconds=4002757107 ntp_fraction=2538325671 arrival_ns=15677329709700
[2026-11-04 12:58:27.591] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2890748373
[2026-11-04 12:58:27.591] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:27.680] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3842679504 ntp_seconds=4002757107 ntp_fraction=2920577761 arrival_ns=15677418739300
[2026-11-04 12:58:27.680] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3842679504
[2026-11-04 12:58:27.680] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=834527275 ntp_seconds=4002757107 ntp_fraction=2920577761 arrival_ns=15677418961700
[2026-11-04 12:58:27.681] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=834527275
[2026-11-04 12:58:27.680] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:27.681] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:27.696] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1655860580 ntp_seconds=4002757107 ntp_fraction=2989297238 arrival_ns=15677434477200
[2026-11-04 12:58:27.696] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1655860580
[2026-11-04 12:58:27.696] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:32.614] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2890748373 ntp_seconds=4002757112 ntp_fraction=2637109919 arrival_ns=15682353090300
[2026-11-04 12:58:32.615] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2890748373
[2026-11-04 12:58:32.615] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:32.688] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=834527275 ntp_seconds=4002757112 ntp_fraction=2954937499 arrival_ns=15682427070800
[2026-11-04 12:58:32.689] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=834527275
[2026-11-04 12:58:32.689] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:32.689] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3842679504 ntp_seconds=4002757112 ntp_fraction=2954937499 arrival_ns=15682427268800
[2026-11-04 12:58:32.690] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3842679504
[2026-11-04 12:58:32.690] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:32.705] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1655860580 ntp_seconds=4002757112 ntp_fraction=3023656976 arrival_ns=15682443324600
[2026-11-04 12:58:32.705] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1655860580
[2026-11-04 12:58:32.705] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:37.629] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2890748373 ntp_seconds=4002757117 ntp_fraction=2697239461 arrival_ns=15687367212900
[2026-11-04 12:58:37.629] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2890748373
[2026-11-04 12:58:37.629] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:37.699] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=834527275 ntp_seconds=4002757117 ntp_fraction=3002182139 arrival_ns=15687437339300
[2026-11-04 12:58:37.699] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=834527275
[2026-11-04 12:58:37.699] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:37.713] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3842679504 ntp_seconds=4002757117 ntp_fraction=3058016714 arrival_ns=15687451201200
[2026-11-04 12:58:37.713] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3842679504
[2026-11-04 12:58:37.713] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:37.728] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1655860580 ntp_seconds=4002757117 ntp_fraction=3126736191 arrival_ns=15687466700900
[2026-11-04 12:58:37.728] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1655860580
[2026-11-04 12:58:37.729] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:42.655] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2890748373 ntp_seconds=4002757122 ntp_fraction=2813203578 arrival_ns=15692393614900
[2026-11-04 12:58:42.656] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2890748373
[2026-11-04 12:58:42.656] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:42.716] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3842679504 ntp_seconds=4002757122 ntp_fraction=3075196583 arrival_ns=15692454469400
[2026-11-04 12:58:42.716] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3842679504
[2026-11-04 12:58:42.717] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:42.716] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=834527275 ntp_seconds=4002757122 ntp_fraction=3075196583 arrival_ns=15692454455400
[2026-11-04 12:58:42.717] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=834527275
[2026-11-04 12:58:42.717] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:42.730] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1655860580 ntp_seconds=4002757122 ntp_fraction=3135326126 arrival_ns=15692468622100
[2026-11-04 12:58:42.730] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1655860580
[2026-11-04 12:58:42.730] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:47.688] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2890748373 ntp_seconds=4002757127 ntp_fraction=2954937499 arrival_ns=15697427054900
[2026-11-04 12:58:47.689] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2890748373
[2026-11-04 12:58:47.689] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:47.721] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3842679504 ntp_seconds=4002757127 ntp_fraction=3096671420 arrival_ns=15697459858100
[2026-11-04 12:58:47.722] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3842679504
[2026-11-04 12:58:47.722] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:47.722] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=834527275 ntp_seconds=4002757127 ntp_fraction=3100966387 arrival_ns=15697460632400
[2026-11-04 12:58:47.722] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=834527275
[2026-11-04 12:58:47.722] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:47.736] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1655860580 ntp_seconds=4002757127 ntp_fraction=3161095929 arrival_ns=15697474751000
[2026-11-04 12:58:47.736] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1655860580
[2026-11-04 12:58:47.736] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:52.694] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2890748373 ntp_seconds=4002757132 ntp_fraction=2980707303 arrival_ns=15702432471100
[2026-11-04 12:58:52.694] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2890748373
[2026-11-04 12:58:52.694] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:52.724] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3842679504 ntp_seconds=4002757132 ntp_fraction=3109556322 arrival_ns=15702462948400
[2026-11-04 12:58:52.725] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3842679504
[2026-11-04 12:58:52.725] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:52.740] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1655860580 ntp_seconds=4002757132 ntp_fraction=3173980831 arrival_ns=15702478219600
[2026-11-04 12:58:52.740] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1655860580
[2026-11-04 12:58:52.740] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:52.754] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=834527275 ntp_seconds=4002757132 ntp_fraction=3238405341 arrival_ns=15702493104100
[2026-11-04 12:58:52.755] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=834527275
[2026-11-04 12:58:52.755] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:57.710] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2890748373 ntp_seconds=4002757137 ntp_fraction=3049426780 arrival_ns=15707449018600
[2026-11-04 12:58:57.711] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2890748373
[2026-11-04 12:58:57.711] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:57.756] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3842679504 ntp_seconds=4002757137 ntp_fraction=3242700308 arrival_ns=15707493896000
[2026-11-04 12:58:57.756] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3842679504
[2026-11-04 12:58:57.756] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:57.756] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=834527275 ntp_seconds=4002757137 ntp_fraction=3242700308 arrival_ns=15707493836000
[2026-11-04 12:58:57.756] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=834527275
[2026-11-04 12:58:57.757] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:58:57.770] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1655860580 ntp_seconds=4002757137 ntp_fraction=3307124817 arrival_ns=15707508394700
[2026-11-04 12:58:57.770] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1655860580
[2026-11-04 12:58:57.770] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:59:02.720] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2890748373 ntp_seconds=4002757142 ntp_fraction=3092376453 arrival_ns=15712458367100
[2026-11-04 12:59:02.720] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2890748373
[2026-11-04 12:59:02.720] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:59:02.778] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3842679504 ntp_seconds=4002757142 ntp_fraction=3341484556 arrival_ns=15712516903800
[2026-11-04 12:59:02.779] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3842679504
[2026-11-04 12:59:02.779] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:59:02.793] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=834527275 ntp_seconds=4002757142 ntp_fraction=3405909065 arrival_ns=15712531909600
[2026-11-04 12:59:02.793] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=834527275
[2026-11-04 12:59:02.794] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:59:02.809] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1655860580 ntp_seconds=4002757142 ntp_fraction=3474628542 arrival_ns=15712547543900
[2026-11-04 12:59:02.809] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1655860580
[2026-11-04 12:59:02.809] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 12:59:06.835] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=15716573516900
[2026-11-04 12:59:06.835] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=3 generation=2 invalidated_generation=1 locked=0
[2026-11-04 12:59:06.835] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=11 group=composition-r383.source.0 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=23 video_raw_generation=0 video_ssrc=1655860580
[2026-11-04 12:59:06.835] [info] [graph][runtime.node] av_reacquisition group=composition-r383.source.0 phase=purging old=1 next=2 transition=1
[2026-11-04 12:59:06.836] [info] [graph][runtime.node] rtp_ingress_purge node=8 transition=1 cutoff=0
[2026-11-04 12:59:06.836] [info] [graph][runtime.node] rtp_binder_purge node=11 transition=1 cutoff=3608
[2026-11-04 12:59:06.836] [info] [graph][runtime.node] av_reacquisition group=composition-r383.source.0 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=1 next=2 transition=1
[2026-11-04 12:59:06.852] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=15716590804600
[2026-11-04 12:59:06.852] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=3 generation=2 invalidated_generation=1 locked=0
[2026-11-04 12:59:06.852] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=30 group=composition-r383.source.1 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=23 video_raw_generation=0 video_ssrc=834527275
[2026-11-04 12:59:06.852] [info] [graph][runtime.node] av_reacquisition group=composition-r383.source.1 phase=purging old=1 next=2 transition=1
[2026-11-04 12:59:06.853] [info] [graph][runtime.node] rtp_ingress_purge node=27 transition=1 cutoff=0
[2026-11-04 12:59:06.853] [info] [graph][runtime.node] rtp_binder_purge node=30 transition=1 cutoff=3607
[2026-11-04 12:59:06.854] [info] [graph][runtime.node] av_reacquisition group=composition-r383.source.1 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=1 next=2 transition=1
[2026-11-04 12:59:06.866] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=15716604736300
[2026-11-04 12:59:06.867] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=3 generation=2 invalidated_generation=1 locked=0
[2026-11-04 12:59:06.867] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=50 group=composition-r383.source.2 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=46 video_raw_generation=0 video_ssrc=3842679504 audio_raw_generation=0 audio_ssrc=2890748373
[2026-11-04 12:59:06.867] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=51 group=composition-r383.source.2 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=46 video_raw_generation=0 video_ssrc=3842679504 audio_raw_generation=0 audio_ssrc=2890748373
[2026-11-04 12:59:06.867] [info] [graph][runtime.node] av_reacquisition group=composition-r383.source.2 phase=purging old=1 next=2 transition=1
[2026-11-04 12:59:06.868] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=15716606403400
[2026-11-04 12:59:06.868] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=3 generation=2 invalidated_generation=1 locked=0
[2026-11-04 12:59:06.868] [info] [graph][runtime.node] rtp_ingress_purge node=46 transition=1 cutoff=0
[2026-11-04 12:59:06.868] [info] [graph][runtime.node] rtp_ingress_purge node=47 transition=1 cutoff=0
[2026-11-04 12:59:06.868] [info] [graph][runtime.node] rtp_binder_purge node=51 transition=1 cutoff=5170
[2026-11-04 12:59:06.868] [info] [graph][runtime.node] rtp_binder_purge node=50 transition=1 cutoff=3608
[2026-11-04 12:59:06.869] [info] [graph][runtime.node] av_reacquisition group=composition-r383.source.2 event=purge_completed purge_ack=complete continuation=acquiring ack_count=2 old=1 next=2 transition=1
[2026-11-04 12:59:39.376] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2244863054 ntp_seconds=4002757179 ntp_fraction=1614907703 arrival_ns=15749114691800
[2026-11-04 12:59:39.376] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2244863054
[2026-11-04 12:59:39.376] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=0 generation=2 invalidated_generation=1 locked=0
[2026-11-04 12:59:39.376] [info] [graph][runtime.node] rtp_binder_boundary stage=deadline_established node=51 group=composition-r383.source.2 buffered=0 phase=2 preserves_output=1 raw_ssrc=3624253372 raw_generation=1 raw_sequence=5172 old_generation=1 next_generation=2 transition_sequence=1 published_generation=1
[2026-11-04 12:59:39.376] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3624253372 ntp_seconds=4002757179 ntp_fraction=1614907703 arrival_ns=15749114695800
[2026-11-04 12:59:39.377] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3624253372
[2026-11-04 12:59:39.377] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 12:59:39.914] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=1 generation=2
[2026-11-04 12:59:40.420] [info] [graph][runtime.node] source_video_trace_summary node_id=61 node=ActivationReleaseSequencer generation=1 reason=generation_change steady_ns=15750158906800 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3550 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:59:40.420] [info] [graph][runtime.node] source_video_trace node_id=61 node=ActivationReleaseSequencer stage=activation source_identity=composition-r383.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=2552021758 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=15750159236300 group=composition-r383.source.2 release_identity=15763 transition_sequence=1 epoch_source_start_ns=994000000 epoch_master_release_ns=153137096200 video_count=16 min_presentation_ns=994000000 max_end_ns=1527333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:59:40.421] [info] [graph][runtime.node] source_video_trace node_id=61 node=ActivationReleaseSequencer stage=release_published source_identity=composition-r383.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=2552021758 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=15750159351900 group=composition-r383.source.2 release_identity=15763 transition_sequence=1 epoch_source_start_ns=994000000 epoch_master_release_ns=153137096200 video_count=16 min_presentation_ns=994000000 max_end_ns=1527333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:59:40.421] [info] [graph][runtime.node] source_video_trace_summary node_id=62 node=MediaAvBoundReleaseExtractorNode generation=1 reason=generation_change steady_ns=15750159388000 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=20 release_count=3571 release_committed_count=3550 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:59:40.421] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=release source_identity=composition-r383.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=2552021758 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=15750159698000 group=composition-r383.source.2 release_identity=15763 transition_sequence=1 epoch_source_start_ns=994000000 epoch_master_release_ns=153137096200 video_count=16 min_presentation_ns=994000000 max_end_ns=1527333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:59:40.421] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=release_committed source_identity=composition-r383.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=2552021758 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=15750159870100 group=composition-r383.source.2 release_identity=15763 transition_sequence=1 epoch_source_start_ns=994000000 epoch_master_release_ns=153137096200 video_count=16 min_presentation_ns=994000000 max_end_ns=1527333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=1 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:59:40.421] [info] [graph][runtime.node] source_video_trace_summary node_id=64 node=VideoDecodeNode generation=1 reason=generation_change steady_ns=15750159909800 input_count=3467 accepted_count=3467 would_block_count=0 resolved_count=3467 output_attempt_count=0 published_count=3466 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=102 incomplete=0
[2026-11-04 12:59:40.421] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=input source_identity=composition-r383.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=2552021758 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=15750160149500 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=102 incomplete=0
[2026-11-04 12:59:40.427] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=accepted source_identity=composition-r383.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=2552021758 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=15750165936000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=102 incomplete=0
[2026-11-04 12:59:40.427] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=resolved source_identity=composition-r383.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=2552021758 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=15750166111600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=102 incomplete=0
[2026-11-04 12:59:40.427] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=published source_identity=composition-r383.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=2552021758 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=15750166228100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=102 incomplete=0
[2026-11-04 12:59:40.428] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-11-04 12:59:40.428] [info] [graph][runtime.node] source_video_trace_summary node_id=65 node=HardwareTransferNode generation=1 reason=generation_change steady_ns=15750166274800 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3466 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:59:40.428] [info] [graph][runtime.node] source_video_trace_summary node_id=66 node=VideoFrameRateNode generation=1 reason=generation_change steady_ns=15750166547200 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3452 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:59:40.428] [info] [graph][runtime.node] source_video_trace_summary node_id=68 node=VideoFilterNode generation=1 reason=generation_change steady_ns=15750166609100 input_count=3446 accepted_count=3446 would_block_count=0 resolved_count=3446 output_attempt_count=0 published_count=3445 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-04 12:59:40.428] [info] [graph][runtime.node] source_video_trace node_id=65 node=HardwareTransferNode stage=published source_identity=composition-r383.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=2552021758 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=15750166708600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:59:40.428] [info] [graph][runtime.node] source_video_trace node_id=66 node=VideoFrameRateNode stage=published source_identity=composition-r383.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=2552021758 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=15750166788100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 12:59:40.428] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=input source_identity=composition-r383.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=2552021758 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=15750166863600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-04 12:59:40.428] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=accepted source_identity=composition-r383.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=2552021758 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=15750167146700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-04 12:59:40.429] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=resolved source_identity=composition-r383.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=850674 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=15750167311800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-04 12:59:40.429] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=published source_identity=composition-r383.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=850674 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=15750167402400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-04 12:59:40.429] [info] [graph][runtime.node] source_video_trace_summary node_id=67 node=VideoPreparationNode generation=1 reason=generation_change steady_ns=15750167430400 input_count=3439 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3439 published_count=3438 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-04 12:59:40.429] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=input source_identity=composition-r383.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=850674 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=15750167556700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-04 12:59:40.429] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=output_attempt source_identity=composition-r383.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=850674 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=15750167622700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-04 12:59:40.429] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=published source_identity=composition-r383.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=994000000 duration_ns=33333333 decoded_sequence=unavailable pts=850674 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=15750167698100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-04 12:59:40.433] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_candidate source_identity=composition-r383.source.2.input.video generation=2 steady_ns=15750171591700 source_sequence=3 presentation_ns=1060666666 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-04 12:59:40.453] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-r383.source.2.input.video generation=2 steady_ns=15750192218100 source_sequence=3 presentation_ns=1060666666 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-04 12:59:44.386] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2244863054 ntp_seconds=4002757184 ntp_fraction=1657857376 arrival_ns=15754125126900
[2026-11-04 12:59:44.387] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2244863054
[2026-11-04 12:59:44.387] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 12:59:44.387] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3624253372 ntp_seconds=4002757184 ntp_fraction=1657857376 arrival_ns=15754125080700
[2026-11-04 12:59:44.387] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3624253372
[2026-11-04 12:59:44.387] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 12:59:49.397] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3624253372 ntp_seconds=4002757189 ntp_fraction=1705102016 arrival_ns=15759135449600
[2026-11-04 12:59:49.397] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3624253372
[2026-11-04 12:59:49.397] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 12:59:49.413] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2244863054 ntp_seconds=4002757189 ntp_fraction=1769526525 arrival_ns=15759151264900
[2026-11-04 12:59:49.413] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2244863054
[2026-11-04 12:59:49.413] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 12:59:54.421] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2244863054 ntp_seconds=4002757194 ntp_fraction=1808181231 arrival_ns=15764159808900
[2026-11-04 12:59:54.421] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2244863054
[2026-11-04 12:59:54.422] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 12:59:54.422] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3624253372 ntp_seconds=4002757194 ntp_fraction=1808181231 arrival_ns=15764159772900
[2026-11-04 12:59:54.422] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3624253372
[2026-11-04 12:59:54.422] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 12:59:59.427] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3624253372 ntp_seconds=4002757199 ntp_fraction=1833951035 arrival_ns=15769165530000
[2026-11-04 12:59:59.427] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3624253372
[2026-11-04 12:59:59.427] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 12:59:59.442] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2244863054 ntp_seconds=4002757199 ntp_fraction=1898375544 arrival_ns=15769180684700
[2026-11-04 12:59:59.442] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2244863054
[2026-11-04 12:59:59.442] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:00:04.431] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3624253372 ntp_seconds=4002757204 ntp_fraction=1851130904 arrival_ns=15774169809800
[2026-11-04 13:00:04.431] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3624253372
[2026-11-04 13:00:04.432] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:00:04.444] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2244863054 ntp_seconds=4002757204 ntp_fraction=1906965479 arrival_ns=15774182472600
[2026-11-04 13:00:04.444] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2244863054
[2026-11-04 13:00:04.444] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:00:09.439] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3624253372 ntp_seconds=4002757209 ntp_fraction=1885490642 arrival_ns=15779177536600
[2026-11-04 13:00:09.439] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3624253372
[2026-11-04 13:00:09.439] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:00:09.454] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2244863054 ntp_seconds=4002757209 ntp_fraction=1949915152 arrival_ns=15779192945800
[2026-11-04 13:00:09.454] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2244863054
[2026-11-04 13:00:09.455] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:00:14.449] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3624253372 ntp_seconds=4002757214 ntp_fraction=1928440315 arrival_ns=15784187349800
[2026-11-04 13:00:14.449] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3624253372
[2026-11-04 13:00:14.449] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:00:14.477] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2244863054 ntp_seconds=4002757214 ntp_fraction=2044404432 arrival_ns=15784215024100
[2026-11-04 13:00:14.477] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2244863054
[2026-11-04 13:00:14.477] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:00:19.451] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3624253372 ntp_seconds=4002757219 ntp_fraction=1937030250 arrival_ns=15789189742300
[2026-11-04 13:00:19.452] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3624253372
[2026-11-04 13:00:19.452] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:00:19.482] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2244863054 ntp_seconds=4002757219 ntp_fraction=2070174236 arrival_ns=15789220348300
[2026-11-04 13:00:19.482] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2244863054
[2026-11-04 13:00:19.482] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:00:24.458] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3624253372 ntp_seconds=4002757224 ntp_fraction=1967095021 arrival_ns=15794196498100
[2026-11-04 13:00:24.458] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3624253372
[2026-11-04 13:00:24.458] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:00:24.486] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2244863054 ntp_seconds=4002757224 ntp_fraction=2087354105 arrival_ns=15794224608500
[2026-11-04 13:00:24.486] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2244863054
[2026-11-04 13:00:24.487] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:00:29.478] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3624253372 ntp_seconds=4002757229 ntp_fraction=2052994367 arrival_ns=15799216408800
[2026-11-04 13:00:29.478] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3624253372
[2026-11-04 13:00:29.478] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:00:29.519] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2244863054 ntp_seconds=4002757229 ntp_fraction=2229088026 arrival_ns=15799257715200
[2026-11-04 13:00:29.519] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2244863054
[2026-11-04 13:00:29.520] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:00:34.487] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3624253372 ntp_seconds=4002757234 ntp_fraction=2091649073 arrival_ns=15804225851300
[2026-11-04 13:00:34.488] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3624253372
[2026-11-04 13:00:34.488] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:00:34.531] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2244863054 ntp_seconds=4002757234 ntp_fraction=2276332666 arrival_ns=15804269271700
[2026-11-04 13:00:34.531] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2244863054
[2026-11-04 13:00:34.531] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:00:39.503] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3624253372 ntp_seconds=4002757239 ntp_fraction=2160368549 arrival_ns=15809241608100
[2026-11-04 13:00:39.503] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3624253372
[2026-11-04 13:00:39.503] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:00:39.550] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2244863054 ntp_seconds=4002757239 ntp_fraction=2362232012 arrival_ns=15809288404500
[2026-11-04 13:00:39.550] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2244863054
[2026-11-04 13:00:39.551] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:00:44.506] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3624253372 ntp_seconds=4002757244 ntp_fraction=2173253451 arrival_ns=15814244746800
[2026-11-04 13:00:44.506] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3624253372
[2026-11-04 13:00:44.507] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:00:44.577] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2244863054 ntp_seconds=4002757244 ntp_fraction=2478196129 arrival_ns=15814315931300
[2026-11-04 13:00:44.578] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2244863054
[2026-11-04 13:00:44.578] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:00:49.520] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3624253372 ntp_seconds=4002757249 ntp_fraction=2233382993 arrival_ns=15819258920200
[2026-11-04 13:00:49.521] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3624253372
[2026-11-04 13:00:49.521] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:00:49.582] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2244863054 ntp_seconds=4002757249 ntp_fraction=2499670966 arrival_ns=15819320621200
[2026-11-04 13:00:49.582] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2244863054
[2026-11-04 13:00:49.582] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:00:54.533] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3624253372 ntp_seconds=4002757254 ntp_fraction=2289217568 arrival_ns=15824271928400
[2026-11-04 13:00:54.533] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3624253372
[2026-11-04 13:00:54.534] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:00:54.611] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2244863054 ntp_seconds=4002757254 ntp_fraction=2624225017 arrival_ns=15824349379800
[2026-11-04 13:00:54.611] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2244863054
[2026-11-04 13:00:54.611] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:00:59.554] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3624253372 ntp_seconds=4002757259 ntp_fraction=2379411881 arrival_ns=15829292903100
[2026-11-04 13:00:59.555] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3624253372
[2026-11-04 13:00:59.555] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:00:59.615] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2244863054 ntp_seconds=4002757259 ntp_fraction=2641404887 arrival_ns=15829353919700
[2026-11-04 13:00:59.615] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2244863054
[2026-11-04 13:00:59.616] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:01:04.576] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3624253372 ntp_seconds=4002757264 ntp_fraction=2473901162 arrival_ns=15834314662000
[2026-11-04 13:01:04.576] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3624253372
[2026-11-04 13:01:04.576] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:01:04.622] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2244863054 ntp_seconds=4002757264 ntp_fraction=2671469658 arrival_ns=15834360619300
[2026-11-04 13:01:04.622] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2244863054
[2026-11-04 13:01:04.622] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:01:09.605] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3624253372 ntp_seconds=4002757269 ntp_fraction=2598455214 arrival_ns=15839343718500
[2026-11-04 13:01:09.605] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3624253372
[2026-11-04 13:01:09.605] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:01:09.647] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2244863054 ntp_seconds=4002757269 ntp_fraction=2778843840 arrival_ns=15839385883600
[2026-11-04 13:01:09.648] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2244863054
[2026-11-04 13:01:09.648] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:01:14.616] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3624253372 ntp_seconds=4002757274 ntp_fraction=2645699854 arrival_ns=15844354688600
[2026-11-04 13:01:14.616] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3624253372
[2026-11-04 13:01:14.617] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:01:14.700] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2244863054 ntp_seconds=4002757274 ntp_fraction=3002182139 arrival_ns=15844438153400
[2026-11-04 13:01:14.700] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2244863054
[2026-11-04 13:01:14.700] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:01:19.624] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3624253372 ntp_seconds=4002757279 ntp_fraction=2680059592 arrival_ns=15849362871700
[2026-11-04 13:01:19.625] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3624253372
[2026-11-04 13:01:19.625] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:01:19.709] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2244863054 ntp_seconds=4002757279 ntp_fraction=3045131812 arrival_ns=15849448101400
[2026-11-04 13:01:19.710] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2244863054
[2026-11-04 13:01:19.710] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:01:24.627] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3624253372 ntp_seconds=4002757284 ntp_fraction=2692944494 arrival_ns=15854365427300
[2026-11-04 13:01:24.627] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3624253372
[2026-11-04 13:01:24.627] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:01:24.713] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2244863054 ntp_seconds=4002757284 ntp_fraction=3062311682 arrival_ns=15854451400000
[2026-11-04 13:01:24.713] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2244863054
[2026-11-04 13:01:24.713] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:01:29.641] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3624253372 ntp_seconds=4002757289 ntp_fraction=2753074036 arrival_ns=15859379782300
[2026-11-04 13:01:29.642] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3624253372
[2026-11-04 13:01:29.642] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:01:29.716] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2244863054 ntp_seconds=4002757289 ntp_fraction=3075196583 arrival_ns=15859454811900
[2026-11-04 13:01:29.716] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2244863054
[2026-11-04 13:01:29.717] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:01:34.658] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3624253372 ntp_seconds=4002757294 ntp_fraction=2826088480 arrival_ns=15864396605900
[2026-11-04 13:01:34.658] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3624253372
[2026-11-04 13:01:34.658] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:01:34.742] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2244863054 ntp_seconds=4002757294 ntp_fraction=3186865733 arrival_ns=15864480879700
[2026-11-04 13:01:34.742] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2244863054
[2026-11-04 13:01:34.743] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:01:38.854] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=2 reason=1 observed_at_ns=15868592531600
[2026-11-04 13:01:38.854] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=3 generation=3 invalidated_generation=2 locked=0
[2026-11-04 13:01:38.854] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=50 group=composition-r383.source.2 buffered=0 phase=0 preserves_output=1 protocol_generation=2 evidence_revision=94 video_raw_generation=1 video_ssrc=2244863054 audio_raw_generation=1 audio_ssrc=3624253372
[2026-11-04 13:01:38.854] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=51 group=composition-r383.source.2 buffered=0 phase=0 preserves_output=1 protocol_generation=2 evidence_revision=94 video_raw_generation=1 video_ssrc=2244863054 audio_raw_generation=1 audio_ssrc=3624253372
[2026-11-04 13:01:38.854] [info] [graph][runtime.node] av_reacquisition group=composition-r383.source.2 phase=purging old=2 next=3 transition=2
[2026-11-04 13:01:38.855] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=2 reason=1 observed_at_ns=15868593238300
[2026-11-04 13:01:38.855] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=3 generation=3 invalidated_generation=2 locked=0
[2026-11-04 13:01:38.856] [info] [graph][runtime.node] rtp_ingress_purge node=46 transition=2 cutoff=0
[2026-11-04 13:01:38.856] [info] [graph][runtime.node] rtp_binder_purge node=51 transition=2 cutoff=10363
[2026-11-04 13:01:38.856] [info] [graph][runtime.node] rtp_ingress_purge node=47 transition=2 cutoff=0
[2026-11-04 13:01:38.856] [info] [graph][runtime.node] rtp_binder_purge node=50 transition=2 cutoff=7203
[2026-11-04 13:01:38.856] [info] [graph][runtime.node] av_reacquisition group=composition-r383.source.2 event=purge_completed purge_ack=complete continuation=acquiring ack_count=2 old=2 next=3 transition=2
[2026-11-04 13:01:59.291] [info] [graph][runtime.lifecycle] continuous_output_drain stage=requested
[2026-11-04 13:01:59.292] [info] [graph][runtime.lifecycle] worker.finished node=80
[2026-11-04 13:01:59.292] [info] [graph][runtime.lifecycle] worker.finished node=2
[2026-11-04 13:01:59.292] [info] [graph][runtime.node] worker.thread_cpu node=2 available=1 cpu_ns=4203125000 process_calls=25798
[2026-11-04 13:01:59.292] [info] [graph][runtime.lifecycle] worker.finished node=4
[2026-11-04 13:01:59.292] [info] [graph][runtime.node] worker.thread_cpu node=4 available=1 cpu_ns=5765625000 process_calls=39514
[2026-11-04 13:01:59.292] [info] [graph][runtime.node] worker.thread_cpu node=80 available=1 cpu_ns=0 process_calls=6
[2026-11-04 13:01:59.292] [info] [graph][runtime.lifecycle] worker.finished node=77
[2026-11-04 13:01:59.292] [info] [graph][runtime.lifecycle] worker.finished node=5
[2026-11-04 13:01:59.293] [info] [graph][runtime.node] worker.thread_cpu node=77 available=1 cpu_ns=15625000 process_calls=7
[2026-11-04 13:01:59.293] [info] [graph][runtime.node] worker.thread_cpu node=5 available=1 cpu_ns=421875000 process_calls=37045
[2026-11-04 13:01:59.325] [info] [graph][runtime.lifecycle] worker.finished node=74
[2026-11-04 13:01:59.325] [info] [graph][runtime.lifecycle] worker.finished node=75
[2026-11-04 13:01:59.325] [info] [graph][runtime.lifecycle] worker.finished node=78
[2026-11-04 13:01:59.325] [info] [graph][runtime.node] worker.thread_cpu node=74 available=1 cpu_ns=781250000 process_calls=62264
[2026-11-04 13:01:59.325] [info] [graph][runtime.node] worker.thread_cpu node=75 available=1 cpu_ns=296875000 process_calls=62246
[2026-11-04 13:01:59.325] [info] [graph][runtime.node] worker.thread_cpu node=78 available=1 cpu_ns=468750000 process_calls=60921
[2026-11-04 13:01:59.353] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 produced_datagrams=174438 produced_ts_payload_bytes=203955748 pending_bytes=0 peak_pending_bytes=110920 access_units=20940 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=32789115 maximum_mux_input_after_emit_ns=36210315 maximum_mux_input_after_dispatch_ns=-63789685 worst_mux_input_ready_ns=95439666800 worst_mux_input_emit_ns=95403456485 worst_mux_input_dispatch_ns=95503456485 maximum_protocol_batch_produced_after_release_ns=35628252 worst_protocol_batch_produced_ns=29438541000 worst_protocol_batch_release_ns=29402912748 worst_protocol_batch_deadline_ns=29469579416 worst_protocol_batch_datagrams=1 exit_reason=completed
[2026-11-04 13:01:59.353] [info] [graph][runtime.lifecycle] worker.finished node=76
[2026-11-04 13:01:59.353] [info] [graph][runtime.lifecycle] worker.finished node=79
[2026-11-04 13:01:59.354] [info] [graph][runtime.node] datagram_batch_trace stage=last_completed generation=1 first_sequence=174438 last_sequence=174438 maximum_submit_interval_ns=71600 maximum_submit_interval_sequence=174438 maximum_start_lateness_ns=497566 maximum_start_lateness_sequence=174438 first_release_master_ns=291969578934 first_deadline_master_ns=292069550108 last_release_master_ns=291969578934 last_deadline_master_ns=292069550108 first_submitted_master_ns=291970148100 last_submitted_master_ns=291970148100 worst_wait_started_master_ns=291961021800 worst_wait_completed_master_ns=291970074700 worst_submit_observed_master_ns=291970076500 wait_attempts=1 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=1 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=1 wait_call_total_ns=9047300 wait_call_max_ns=9047300 wait_before_call_count=1 wait_before_call_total_ns=500 wait_before_call_max_ns=500 wait_arm_count=1 wait_arm_total_ns=2600 wait_arm_max_ns=2600 wait_api_wall_count=1 wait_api_wall_total_ns=9043000 wait_api_wall_max_ns=9043000 wait_return_to_next_clock_count=1 wait_return_to_next_clock_total_ns=5000 wait_return_to_next_clock_max_ns=5000 wait_target_lateness_count=1 wait_target_lateness_total_ns=495766 wait_target_lateness_max_ns=495766 worst_wait_metric=call_wall worst_wait_sequence=174438 worst_wait_target_master_ns=291969578934 worst_wait_sampled_master_ns=291961021800 worst_wait_sampled_steady_ns=15889082605100 worst_wait_timeout_ns=8557134 worst_wait_entered_steady_ns=15889082605600 worst_wait_returned_steady_ns=15889091652900 worst_wait_arm_before_steady_ns=15889082606100 worst_wait_arm_after_steady_ns=15889082608700 worst_wait_api_before_steady_ns=15889082608800 worst_wait_api_returned_steady_ns=15889091651800 worst_wait_windows_due_100ns=-85572 worst_wait_windows_result=1 worst_wait_windows_error=unavailable worst_wait_windows_pid=38996 worst_wait_windows_tid=36124 worst_wait_windows_qpc_frequency_hz=10000000 worst_wait_next_master_ns=291970074700 worst_wait_next_steady_ns=15889091657900 worst_wait_outcome=deadline
[2026-11-04 13:01:59.354] [info] [graph][runtime.node] worker.thread_cpu node=76 available=1 cpu_ns=3437500000 process_calls=125632
[2026-11-04 13:01:59.354] [info] [graph][runtime.node] worker.thread_cpu node=79 available=1 cpu_ns=1859375000 process_calls=100551
[2026-11-04 13:01:59.354] [info] [graph][runtime.node] datagram_successful_submit_trace stage=worst_success generation=1 endpoint=1 sequence=2760 pacing_lateness_ns=5267840 matches_pacing_worst=1 materialized_master_ns=unavailable enqueued_master_ns=unavailable submit_interval_scope=pre_transport_checks_and_transport socket_only_ns=unavailable release_master_ns=9236245601 deadline_master_ns=9336211120 original_protocol_deadline_master_ns=9336245601 prefix_completion_limit_master_ns=9336211120 reserved_at_master_ns=9242188000 not_before_master_ns=9236922260 not_after_master_ns=9335996080 service_duration_ns=215040 wire_bytes_per_second=6250000 submit_started_master_ns=9242190100 submit_completed_master_ns=9242222800 activation_started_master_ns=9242186500 activation_completed_master_ns=9242187400 permit_started_master_ns=9242187700 permit_completed_master_ns=9242187800 snapshot_started_master_ns=9242187800 snapshot_sampled_master_ns=9242188000 snapshot_completed_master_ns=9242188100 reservation_completed_master_ns=9242189400 wait_started_master_ns=9242189800 wait_completed_master_ns=9242189800 submit_observed_master_ns=9242190100 wait_attempts=0 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=0 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=0 wait_call_total_ns=0 wait_call_max_ns=0 wait_before_call_count=0 wait_before_call_total_ns=0 wait_before_call_max_ns=0 wait_arm_count=0 wait_arm_total_ns=0 wait_arm_max_ns=0 wait_api_wall_count=0 wait_api_wall_total_ns=0 wait_api_wall_max_ns=0 wait_return_to_next_clock_count=0 wait_return_to_next_clock_total_ns=0 wait_return_to_next_clock_max_ns=0 wait_target_lateness_count=0 wait_target_lateness_total_ns=0 wait_target_lateness_max_ns=0
[2026-11-04 13:01:59.354] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=30980 committed_datagrams=174438 committed_payload_bytes=203955748 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=79296 pacing_reserved=174438 pacing_cancelled=0 pacing_submitted=174438 pacing_maximum_submit_lateness_ns=5267840 pacing_worst_late_global_sequence=2760 pacing_worst_success_sample_available=1 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=79296 backlog_max_residence_ns=49107300 last_materialized_sequence=174438 last_scheduled_sequence=174438 last_submitted_sequence=174438 last_committed_sequence=174438 submitted_service_generation=1 submitted_service_sequence=174438 submitted_service_completed_ns=291970148100 submitted_service_duration_ns=34560 submitted_service_available_ns=291970182660 submitted_scope_available_ns=291970182660 shared_scope_charged_datagrams=174438 shared_scope_charged_wire_bytes=208840012 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2154 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2154 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=188010 endpoint_1_target_effective_socket_bytes=188010 endpoint_1_api_requested_socket_bytes=188010 endpoint_1_effective_socket_bytes=188010 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=174438 endpoint_1_committed_payload_bytes=203955748
[2026-11-04 13:01:59.355] [info] [graph][runtime.lifecycle] worker.finished node=81
[2026-11-04 13:01:59.355] [info] [graph][runtime.node] worker.thread_cpu node=81 available=1 cpu_ns=10390625000 process_calls=62110
[2026-11-04 13:01:59.355] [info] [graph][runtime.lifecycle] distribution_registration scope=worker stage=leave node=81 pid=38996 tid=36124 native=0
[2026-11-04 13:01:59.552] [info] [graph][runtime.lifecycle] continuous_output_drain stage=completed
[2026-11-04 13:01:59.553] [info] [graph][runtime.lifecycle] stop.begin
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=8 available=1 cpu_ns=296875000 process_calls=83393
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=7 available=1 cpu_ns=1609375000 process_calls=49259
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=6 available=1 cpu_ns=4843750000 process_calls=51163
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=30 available=1 cpu_ns=125000000 process_calls=10601
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=19 available=1 cpu_ns=46875000 process_calls=10633
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=35 available=1 cpu_ns=109375000 process_calls=10800
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=12 available=1 cpu_ns=0 process_calls=80
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=46 available=1 cpu_ns=906250000 process_calls=153768
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=58 available=1 cpu_ns=640625000 process_calls=52686
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=48 available=1 cpu_ns=31250000 process_calls=309
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=14 available=1 cpu_ns=46875000 process_calls=10753
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=62 available=1 cpu_ns=687500000 process_calls=51820
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=27 available=1 cpu_ns=359375000 process_calls=73582
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=65 available=1 cpu_ns=140625000 process_calls=21444
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=26 available=1 cpu_ns=1625000000 process_calls=13744
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=15 available=1 cpu_ns=78125000 process_calls=10690
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=18 available=1 cpu_ns=125000000 process_calls=10631
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=11 available=1 cpu_ns=93750000 process_calls=10583
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=45 available=1 cpu_ns=1406250000 process_calls=13742
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=47 available=1 cpu_ns=984375000 process_calls=148150
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=9 available=1 cpu_ns=15625000 process_calls=79
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=10 available=1 cpu_ns=0 process_calls=104
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=28 available=1 cpu_ns=0 process_calls=81
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=29 available=1 cpu_ns=0 process_calls=105
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=49 available=1 cpu_ns=0 process_calls=466
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=31 available=1 cpu_ns=0 process_calls=81
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=67 available=1 cpu_ns=265625000 process_calls=21412
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=50 available=1 cpu_ns=218750000 process_calls=21458
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=52 available=1 cpu_ns=0 process_calls=309
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=51 available=1 cpu_ns=375000000 process_calls=30801
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=13 available=1 cpu_ns=0 process_calls=104
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=53 available=1 cpu_ns=0 process_calls=461
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=17 available=1 cpu_ns=0 process_calls=80
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=33 available=1 cpu_ns=62500000 process_calls=10743
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=36 available=1 cpu_ns=0 process_calls=80
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=54 available=1 cpu_ns=171875000 process_calls=21665
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=55 available=1 cpu_ns=390625000 process_calls=31144
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=59 available=1 cpu_ns=0 process_calls=304
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=32 available=1 cpu_ns=0 process_calls=105
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=34 available=1 cpu_ns=93750000 process_calls=10694
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=56 available=1 cpu_ns=265625000 process_calls=21591
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=57 available=1 cpu_ns=500000000 process_calls=30873
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=37 available=1 cpu_ns=140625000 process_calls=10627
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=38 available=1 cpu_ns=93750000 process_calls=10641
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=60 available=1 cpu_ns=515625000 process_calls=51753
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=61 available=1 cpu_ns=515625000 process_calls=51607
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=39 available=1 cpu_ns=140625000 process_calls=10656
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=16 available=1 cpu_ns=62500000 process_calls=10751
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=41 available=1 cpu_ns=1296875000 process_calls=14380
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=22 available=1 cpu_ns=1484375000 process_calls=13881
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=70 available=1 cpu_ns=1203125000 process_calls=42432
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=23 available=1 cpu_ns=109375000 process_calls=10698
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=64 available=1 cpu_ns=2640625000 process_calls=35151
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=72 available=1 cpu_ns=578125000 process_calls=31816
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=42 available=1 cpu_ns=78125000 process_calls=10579
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=71 available=1 cpu_ns=359375000 process_calls=32415
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=43 available=1 cpu_ns=171875000 process_calls=11823
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=66 available=1 cpu_ns=312500000 process_calls=22870
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=24 available=1 cpu_ns=93750000 process_calls=12081
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=20 available=1 cpu_ns=140625000 process_calls=10665
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=73 available=1 cpu_ns=703125000 process_calls=50107
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=25 available=1 cpu_ns=234375000 process_calls=10908
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=68 available=1 cpu_ns=1750000000 process_calls=25033
[2026-11-04 13:01:59.553] [info] [graph][runtime.node] worker.thread_cpu node=44 available=1 cpu_ns=187500000 process_calls=10777
[2026-11-04 13:01:59.561] [info] [graph][runtime.node] mpegts_wire_materializer stage=stopped maximum_materialized_after_release_ns=35724752 worst_materialized_at_ns=29438637500 worst_release_ns=29402912748 worst_deadline_ns=29469579416 worst_global_sequence=17194
[2026-11-04 13:01:59.561] [info] [graph][runtime.node] audio_stage_observation node=74 stage=scheduler_input count=12342 last_generation=1 last_begin=12637184 last_end=12638208 last_rate=44100 last_steady_ns=15889031285000 maximum_inter_event_gap_ns=27924800 gap_begin_generation=1 gap_begin_begin=2264064 gap_begin_end=2265088 gap_begin_rate=44100 gap_begin_steady_ns=15653844235800 gap_end_generation=1 gap_end_begin=2265088 gap_end_end=2266112 gap_end_rate=44100 gap_end_steady_ns=15653872160600
[2026-11-04 13:01:59.562] [info] [graph][runtime.node] av_scheduler_correlated_wait generation=1 canonical_sequence=1 missing=audio begin_steady_ns=15602466360400 end_steady_ns=15602506385300 observed_wait_ns=40024900 held_dispatch_master_ns=5502912268 begin_master_ns=5344776700 end_master_ns=5384800600 dispatch_remaining_at_end_ns=118111668 resumed_dispatch_master_ns=5502912268
[2026-11-04 13:01:59.562] [info] [graph][runtime.node] audio_stage_observation node=5 stage=canonicalizer_input count=12342 last_generation=1 last_begin=12637184 last_end=12638208 last_rate=44100 last_steady_ns=15889030557100 maximum_inter_event_gap_ns=27915500 gap_begin_generation=1 gap_begin_begin=2264064 gap_begin_end=2265088 gap_begin_rate=44100 gap_begin_steady_ns=15653844216300 gap_end_generation=1 gap_end_begin=2265088 gap_end_end=2266112 gap_end_rate=44100 gap_end_steady_ns=15653872131800
[2026-11-04 13:01:59.562] [info] [graph][runtime.node] audio_stage_observation node=5 stage=canonicalizer_committed count=12342 last_generation=1 last_begin=12637184 last_end=12638208 last_rate=44100 last_steady_ns=15889030564900 maximum_inter_event_gap_ns=27920600 gap_begin_generation=1 gap_begin_begin=2264064 gap_begin_end=2265088 gap_begin_rate=44100 gap_begin_steady_ns=15653844225500 gap_end_generation=1 gap_end_begin=2265088 gap_end_end=2266112 gap_end_rate=44100 gap_end_steady_ns=15653872146100
[2026-11-04 13:01:59.562] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_input count=12342 last_generation=1 last_begin=12637184 last_end=12638208 last_rate=44100 last_steady_ns=15889016129100 maximum_inter_event_gap_ns=26008900 gap_begin_generation=1 gap_begin_begin=4506624 gap_begin_end=4507648 gap_begin_rate=44100 gap_begin_steady_ns=15704649189100 gap_end_generation=1 gap_end_begin=4507648 gap_end_end=4508672 gap_end_rate=44100 gap_end_steady_ns=15704675198000
[2026-11-04 13:01:59.562] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_submitted count=12342 last_generation=1 last_begin=12637184 last_end=12638208 last_rate=44100 last_steady_ns=15889016213700 maximum_inter_event_gap_ns=27900400 gap_begin_generation=1 gap_begin_begin=2266112 gap_begin_end=2267136 gap_begin_rate=44100 gap_begin_steady_ns=15653844194500 gap_end_generation=1 gap_end_begin=2267136 gap_end_end=2268160 gap_end_rate=44100 gap_end_steady_ns=15653872094900
[2026-11-04 13:01:59.562] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_packet count=12342 last_generation=1 last_begin=12637184 last_end=12638208 last_rate=44100 last_steady_ns=15889030551100 maximum_inter_event_gap_ns=27906500 gap_begin_generation=1 gap_begin_begin=2264064 gap_begin_end=2265088 gap_begin_rate=44100 gap_begin_steady_ns=15653844202200 gap_end_generation=1 gap_end_begin=2265088 gap_end_end=2266112 gap_end_rate=44100 gap_end_steady_ns=15653872108700
[2026-11-04 13:01:59.563] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_committed count=12342 last_generation=1 last_begin=12637184 last_end=12638208 last_rate=44100 last_steady_ns=15889030553000 maximum_inter_event_gap_ns=27912900 gap_begin_generation=1 gap_begin_begin=2264064 gap_begin_end=2265088 gap_begin_rate=44100 gap_begin_steady_ns=15653844210200 gap_end_generation=1 gap_end_begin=2265088 gap_end_end=2266112 gap_end_rate=44100 gap_end_steady_ns=15653872123100
[2026-11-04 13:01:59.563] [info] [graph][runtime.lifecycle] codec_owner_release node=4 kind=AudioEncode owner_references=4
[2026-11-04 13:01:59.563] [info] [graph][runtime.node] audio_stage_observation node=7 stage=aggregate_built count=12342 last_generation=1 last_begin=12637184 last_end=12638208 last_rate=44100 last_steady_ns=15889016071200 maximum_inter_event_gap_ns=26015500 gap_begin_generation=1 gap_begin_begin=4506624 gap_begin_end=4507648 gap_begin_rate=44100 gap_begin_steady_ns=15704649158500 gap_end_generation=1 gap_end_begin=4507648 gap_end_end=4508672 gap_end_rate=44100 gap_end_steady_ns=15704675174000
[2026-11-04 13:01:59.563] [info] [graph][runtime.node] audio_stage_observation node=7 stage=aggregate_committed count=12342 last_generation=1 last_begin=12637184 last_end=12638208 last_rate=44100 last_steady_ns=15889016088800 maximum_inter_event_gap_ns=26008800 gap_begin_generation=1 gap_begin_begin=4506624 gap_begin_end=4507648 gap_begin_rate=44100 gap_begin_steady_ns=15704649179500 gap_end_generation=1 gap_end_begin=4507648 gap_end_end=4508672 gap_end_rate=44100 gap_end_steady_ns=15704675188300
[2026-11-04 13:01:59.563] [info] [graph][runtime.node] continuous_audio_aggregate.summary reason=stop output_group=composition-r383.output audio_commits=12342 real_samples=10256384 silence_samples=2381824
[2026-11-04 13:01:59.563] [info] [graph][runtime.lifecycle] video_encode.zero_copy_summary drm_prime=0 software_frame=0
[2026-11-04 13:01:59.563] [info] [graph][runtime.lifecycle] codec_owner_release node=2 kind=VideoEncode owner_references=4
[2026-11-04 13:01:59.563] [info] [graph][runtime.node] video_encoder_execution_observation node=2 count=8598 maximum_call_ns=8751400 maximum_begin_steady_ns=15886224955900 maximum_end_steady_ns=15886233707300 maximum_encoder_pts=8513 maximum_native_result=0 maximum_between_submissions_ns=36830300 before_gap_begin_steady_ns=15602758577300 before_gap_end_steady_ns=15602760442300 before_gap_encoder_pts=9 before_gap_native_result=0 after_gap_begin_steady_ns=15602797272600 after_gap_end_steady_ns=15602799103600 after_gap_encoder_pts=10 after_gap_native_result=0
[2026-11-04 13:01:59.563] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=process count=51163 maximum_ns=5764500 begin_steady_ns=15602791482400 end_steady_ns=15602797246900 video_cursor=10 audio_cursor=not_owned next_video_cursor=10 next_audio_cursor=not_owned error=None return_state=progress deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-11-04 13:01:59.563] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=purge count=51163 maximum_ns=249000 begin_steady_ns=15716592132600 end_steady_ns=15716592381600 video_cursor=3425 audio_cursor=not_owned next_video_cursor=3425 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-04 13:01:59.563] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=bind count=51159 maximum_ns=151500 begin_steady_ns=15767225807300 end_steady_ns=15767225958800 video_cursor=4944 audio_cursor=not_owned next_video_cursor=4944 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-04 13:01:59.563] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=consume count=51159 maximum_ns=3335200 begin_steady_ns=15602454493000 end_steady_ns=15602457828200 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-04 13:01:59.563] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=activate count=5 maximum_ns=18600 begin_steady_ns=15602457828400 end_steady_ns=15602457847000 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-04 13:01:59.563] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=build_video count=8598 maximum_ns=5713800 begin_steady_ns=15602791508500 end_steady_ns=15602797222300 video_cursor=10 audio_cursor=not_owned next_video_cursor=10 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-04 13:01:59.564] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=canvas count=8598 maximum_ns=5675100 begin_steady_ns=15602791523200 end_steady_ns=15602797198300 video_cursor=10 audio_cursor=not_owned next_video_cursor=10 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-04 13:01:59.564] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=emit count=8599 maximum_ns=695600 begin_steady_ns=15602460011100 end_steady_ns=15602460706700 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-04 13:01:59.564] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=between_calls_previous_return count=51162 maximum_ns=91163900 begin_steady_ns=15602362219800 end_steady_ns=15602453383700 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=waiting deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-11-04 13:01:59.564] [info] [graph][runtime.node] continuous_aggregate.summary reason=stop output_group=composition-r383.output output_generation=1 video_commits=8598 source[0].group=composition-r383.source.0 source[0].identity=composition-r383.source.0.input.video source[0].real_frames=3424 source[0].black_frames=5174 source[0].contribution_generation=0 source[1].group=composition-r383.source.1 source[1].identity=composition-r383.source.1.input.video source[1].real_frames=3424 source[1].black_frames=5174 source[1].contribution_generation=0 source[2].group=composition-r383.source.2 source[2].identity=composition-r383.source.2.input.video source[2].real_frames=6978 source[2].black_frames=1620 source[2].contribution_generation=0
[2026-11-04 13:01:59.564] [info] [graph][runtime.node] cuda_canvas_call stage=context_push count=6982 operation=copy maximum_ns=62600 begin_steady_ns=15832624571200 end_steady_ns=15832624633800 native_result=0 thread_cpu_ns=0
[2026-11-04 13:01:59.564] [info] [graph][runtime.node] cuda_canvas_call stage=source_wait count=20810 operation=copy maximum_ns=159100 begin_steady_ns=15766658038400 end_steady_ns=15766658197500 native_result=0 thread_cpu_ns=0
[2026-11-04 13:01:59.564] [info] [graph][runtime.node] cuda_canvas_call stage=plane_copy count=41622 operation=copy maximum_ns=334000 begin_steady_ns=15861158477200 end_steady_ns=15861158811200 native_result=0 thread_cpu_ns=0
[2026-11-04 13:01:59.564] [info] [graph][runtime.node] cuda_canvas_call stage=destination_wait count=2 operation=upload maximum_ns=83300 begin_steady_ns=15602307928700 end_steady_ns=15602308012000 native_result=0 thread_cpu_ns=0
[2026-11-04 13:01:59.564] [info] [graph][runtime.node] cuda_canvas_call stage=context_pop count=6982 operation=copy maximum_ns=194200 begin_steady_ns=15602459799400 end_steady_ns=15602459993600 native_result=0 thread_cpu_ns=0
[2026-11-04 13:01:59.564] [info] [graph][runtime.node] cuda_canvas_call stage=batch_completion count=6980 operation=copy maximum_ns=5513600 begin_steady_ns=15602791659300 end_steady_ns=15602797172900 native_result=0 thread_cpu_ns=0
[2026-11-04 13:01:59.564] [info] [graph][runtime.node] canvas_compose_correlated scope=attempt begin_steady_ns=15602791524000 end_steady_ns=15602797197300 wall_ns=5673300 thread_cpu_ns=0 batch_pid=38996 batch_tid=9172 batch_context=2275572003280 batch_stream=2274202473584 batch_submit_begin_steady_ns=15602791538000 batch_submit_end_steady_ns=15602791659300 batch_wait_begin_steady_ns=15602791659600 batch_wait_end_steady_ns=15602797171300 batch_timing_preparation=ready batch_timing_preparation_native=0 batch_start_event_native=0 batch_end_event_native=0 batch_sync_native=0 batch_elapsed_native=0 batch_gpu_elapsed_available=1 batch_gpu_elapsed_ms=0.04096 batch_gpu_interval=event_execution_interval_excludes_pre_start_queue surface_count=1 surface_total_ns=1500 surface_maximum_ns=1500 surface_maximum_tile=none surface_maximum_result=unavailable validate_count=3 validate_total_ns=2200 validate_maximum_ns=1200 validate_maximum_tile=0 validate_maximum_result=0 black_count=1 black_total_ns=39600 black_maximum_ns=39600 black_maximum_tile=none black_maximum_result=0 tile_count=3 tile_total_ns=43400 tile_maximum_ns=20600 tile_maximum_tile=0 tile_maximum_result=0 publish_count=1 publish_total_ns=11700 publish_maximum_ns=11700 publish_maximum_tile=none publish_maximum_result=0 context_push_count=1 context_push_total_ns=5400 context_push_maximum_ns=5400 context_push_maximum_tile=none context_push_maximum_result=0 source_wait_count=4 source_wait_total_ns=2700 source_wait_maximum_ns=2100 source_wait_maximum_tile=0 source_wait_maximum_result=0 plane_copy_count=8 plane_copy_total_ns=67200 plane_copy_maximum_ns=32000 plane_copy_maximum_tile=none plane_copy_maximum_result=0 destination_wait_count=0 destination_wait_total_ns=0 destination_wait_maximum_ns=0 destination_wait_maximum_tile=none destination_wait_maximum_result=unavailable context_pop_count=1 context_pop_total_ns=3000 context_pop_maximum_ns=3000 context_pop_maximum_tile=none context_pop_maximum_result=0 batch_completion_count=1 batch_completion_total_ns=5511900 batch_completion_maximum_ns=5511900 batch_completion_maximum_tile=none batch_completion_maximum_result=0 source_query_completed_count=0 source_query_ready_count=3 source_query_ready_maximum_ns=6100 source_query_ready_begin_steady_ns=15602791612100 source_query_ready_end_steady_ns=15602791618200 source_query_ready_tile=0 source_query_ready_operation=event_query source_query_ready_native=0 source_query_ready_restore_native=0 source_query_pending_count=0 source_query_unavailable_count=0 source_query_error_count=0
[2026-11-04 13:01:59.565] [info] [graph][runtime.node] source_video_trace_summary node_id=67 node=VideoPreparationNode generation=2 reason=stop steady_ns=15889303388600 input_count=3568 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3568 published_count=3568 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=7 incomplete=0
[2026-11-04 13:01:59.565] [info] [graph][runtime.node] source_video_trace_summary node_id=44 node=VideoPreparationNode generation=1 reason=stop steady_ns=15889303470800 input_count=3438 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3438 published_count=3437 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-04 13:01:59.565] [info] [graph][runtime.node] source_video_trace_summary node_id=25 node=VideoPreparationNode generation=1 reason=stop steady_ns=15889303544100 input_count=3438 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3438 published_count=3437 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-04 13:01:59.565] [info] [graph][runtime.lifecycle] codec_owner_release node=73 kind=AudioResample owner_references=1
[2026-11-04 13:01:59.565] [info] [graph][runtime.lifecycle] video_filter.zero_copy_summary drm_prime_input=0 drm_prime_output=0 rga=0 software_frame=0
[2026-11-04 13:01:59.565] [info] [graph][runtime.node] source_video_trace_summary node_id=68 node=VideoFilterNode generation=2 reason=stop steady_ns=15889303956100 input_count=3569 accepted_count=3569 would_block_count=0 resolved_count=3569 output_attempt_count=0 published_count=3569 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-04 13:01:59.570] [info] [graph][runtime.lifecycle] video_filter.zero_copy_summary drm_prime_input=0 drm_prime_output=0 rga=0 software_frame=0
[2026-11-04 13:01:59.570] [info] [graph][runtime.node] source_video_trace_summary node_id=45 node=VideoFilterNode generation=1 reason=stop steady_ns=15889308712900 input_count=3445 accepted_count=3445 would_block_count=0 resolved_count=3445 output_attempt_count=0 published_count=3444 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-04 13:01:59.573] [info] [graph][runtime.lifecycle] video_filter.zero_copy_summary drm_prime_input=0 drm_prime_output=0 rga=0 software_frame=0
[2026-11-04 13:01:59.573] [info] [graph][runtime.node] source_video_trace_summary node_id=26 node=VideoFilterNode generation=1 reason=stop steady_ns=15889311707600 input_count=3445 accepted_count=3445 would_block_count=0 resolved_count=3445 output_attempt_count=0 published_count=3444 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-04 13:01:59.578] [info] [graph][runtime.node] source_video_trace_summary node_id=66 node=VideoFrameRateNode generation=2 reason=stop steady_ns=15889316599200 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3569 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:01:59.578] [info] [graph][runtime.node] source_video_trace_summary node_id=43 node=VideoFrameRateNode generation=1 reason=stop steady_ns=15889316928700 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3451 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:01:59.578] [info] [graph][runtime.node] source_video_trace_summary node_id=24 node=VideoFrameRateNode generation=1 reason=stop steady_ns=15889317078100 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3451 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:01:59.578] [info] [graph][runtime.node] hardware_transfer.summary forwarded=3609 download=0 upload=0
[2026-11-04 13:01:59.578] [info] [graph][runtime.node] source_video_trace_summary node_id=65 node=HardwareTransferNode generation=2 reason=stop steady_ns=15889317234700 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3569 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:01:59.579] [info] [graph][runtime.node] hardware_transfer.summary forwarded=41 download=0 upload=0
[2026-11-04 13:01:59.579] [info] [graph][runtime.node] source_video_trace_summary node_id=42 node=HardwareTransferNode generation=1 reason=stop steady_ns=15889317389200 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3465 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:01:59.579] [info] [graph][runtime.node] hardware_transfer.summary forwarded=42 download=0 upload=0
[2026-11-04 13:01:59.579] [info] [graph][runtime.node] source_video_trace_summary node_id=23 node=HardwareTransferNode generation=1 reason=stop steady_ns=15889317519700 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3465 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:01:59.579] [info] [graph][runtime.lifecycle] codec_owner_release node=70 kind=AudioDecode owner_references=2
[2026-11-04 13:01:59.579] [info] [graph][runtime.lifecycle] video_decode.zero_copy_summary drm_prime=0 software_frame=0
[2026-11-04 13:01:59.579] [info] [graph][runtime.node] source_video_trace_summary node_id=64 node=VideoDecodeNode generation=2 reason=stop steady_ns=15889317857800 input_count=3569 accepted_count=3569 would_block_count=0 resolved_count=3569 output_attempt_count=0 published_count=3569 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=102 incomplete=0
[2026-11-04 13:01:59.579] [info] [graph][runtime.lifecycle] codec_owner_release node=64 kind=VideoDecode owner_references=2
[2026-11-04 13:01:59.591] [info] [graph][runtime.lifecycle] video_decode.zero_copy_summary drm_prime=0 software_frame=0
[2026-11-04 13:01:59.592] [info] [graph][runtime.node] source_video_trace_summary node_id=41 node=VideoDecodeNode generation=1 reason=stop steady_ns=15889330418100 input_count=3466 accepted_count=3466 would_block_count=0 resolved_count=3466 output_attempt_count=0 published_count=3465 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=102 incomplete=0
[2026-11-04 13:01:59.592] [info] [graph][runtime.lifecycle] codec_owner_release node=41 kind=VideoDecode owner_references=2
[2026-11-04 13:01:59.605] [info] [graph][runtime.lifecycle] video_decode.zero_copy_summary drm_prime=0 software_frame=0
[2026-11-04 13:01:59.605] [info] [graph][runtime.node] source_video_trace_summary node_id=22 node=VideoDecodeNode generation=1 reason=stop steady_ns=15889344139800 input_count=3466 accepted_count=3466 would_block_count=0 resolved_count=3466 output_attempt_count=0 published_count=3465 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=103 incomplete=0
[2026-11-04 13:01:59.605] [info] [graph][runtime.lifecycle] codec_owner_release node=22 kind=VideoDecode owner_references=2
[2026-11-04 13:01:59.620] [info] [graph][runtime.node] source_video_trace_summary node_id=62 node=MediaAvBoundReleaseExtractorNode generation=2 reason=stop steady_ns=15889359124500 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=3554 release_committed_count=3554 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:01:59.621] [info] [graph][runtime.node] source_video_trace_summary node_id=39 node=MediaAvBoundReleaseExtractorNode generation=1 reason=stop steady_ns=15889359467200 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=3570 release_committed_count=3554 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:01:59.621] [info] [graph][runtime.node] source_video_trace_summary node_id=20 node=MediaAvBoundReleaseExtractorNode generation=1 reason=stop steady_ns=15889359608800 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=3571 release_committed_count=3555 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:01:59.621] [info] [graph][runtime.node] source_video_trace_summary node_id=61 node=ActivationReleaseSequencer generation=2 reason=stop steady_ns=15889359680500 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3554 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:01:59.621] [info] [graph][runtime.node] source_video_trace_summary node_id=38 node=ActivationReleaseSequencer generation=1 reason=stop steady_ns=15889359752700 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3554 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:01:59.621] [info] [graph][runtime.node] source_video_trace_summary node_id=19 node=ActivationReleaseSequencer generation=1 reason=stop steady_ns=15889359827900 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3555 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:01:59.621] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-11-04 13:01:59.621] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-11-04 13:01:59.621] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-11-04 13:01:59.622] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=10269 completion_waits=10269 completed_datagrams=10462 drained_datagrams=218 would_block=26 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-04 13:01:59.622] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=71638 completion_waits=71638 completed_datagrams=212075 drained_datagrams=140464 would_block=26 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-04 13:01:59.623] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=46976 completion_waits=46976 completed_datagrams=105648 drained_datagrams=58753 would_block=85 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-04 13:01:59.623] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=53744 completion_waits=53744 completed_datagrams=105379 drained_datagrams=51719 would_block=85 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-04 13:01:59.624] [info] [graph][runtime.node] codec_resolver.release slot=encoder_parameters bufferReferences=1 hasCredit=0 creditReferences=0
[2026-11-04 13:01:59.624] [info] [graph][runtime.lifecycle] stop.done state=Stopped
[2026-11-04 13:01:59.685] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=retired detail="" description=""
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=881, threads=81, processThreads=129, workers=0, workerProgress=565081, workerProcessCalls=2058322, workerWaits=1493224, workerWakeups=1471415, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=967, logicalProcessors=22, averageSystemMachineCpuPercent=9.644072, averageProcessMachineCpuPercent=1.549571, peakProcessMachineCpuPercent=4.556355, averageProcessSingleCoreCpuPercent=34.090555, peakProcessSingleCoreCpuPercent=100.239808, initialWorkingSetBytes=210739200, workingSetBytes=262553600, peakWorkingSetBytes=271073280, totalPushed=486339, totalPopped=486303, droppedBuffers=0, encodedPacketsPushed=96108, encodedPacketsPopped=96108, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=14993595, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=719, graphPayloadReservations=222990, graphPayloadReleases=222990, graphPayloadPressureFailures=0, lastInputReceivedAtNs=15868592211800
[CLI] realtime video validation stopped successfully
[2026-11-04 12:57:12.719] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1 source_master_ns=5503861016 raw_phase_ns=4536 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-11-04 12:57:12.719] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3 source_master_ns=5550300925 raw_phase_ns=4536 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=45346
[2026-11-04 13:01:35.105] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4963 source_master_ns=268355976018 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5124098
[2026-11-04 13:01:36.127] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=5007 source_master_ns=269377654023 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5169444
[2026-11-04 13:01:37.172] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=5052 source_master_ns=270422551982 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5214790
[2026-11-04 13:01:38.194] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=5096 source_master_ns=271444229986 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5260136
```

### composition-r383-cli-error.log

```text
[in @ 00000211fb48d3c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000211fb48d3c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 15176.121478
[in @ 00000211fb531000] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000211fb531000] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 14406.219167
[in @ 00000211fb9c9000] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000211fb9c9000] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 36062.915078
[in @ 00000211817e1200] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000211817e1200] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 14406.219167
[in @ 00000211817e2c80] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000211817e2c80] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 15176.121478
[in @ 00000211817e3300] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000211817e3300] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 36062.915078
[aac @ 00000211fb5dff80] Qavg: 12849.738

```

### composition-r383-monitor.csv

```text
"Time","CpuPercent","WorkingSet","PrivateBytes"
"2026-11-04T12:57:08.3946328+08:00","0.7034934335810351","143568896","568905728"
"2026-11-04T12:57:09.4235514+08:00","0","146268160","571416576"
"2026-11-04T12:57:10.4446686+08:00","0.13910788550565456","149950464","575164416"
"2026-11-04T12:57:11.4608162+08:00","0.0698941052192883","152649728","577847296"
"2026-11-04T12:57:12.4733523+08:00","0.2805736102553866","198320128","711819264"
"2026-11-04T12:57:13.4961893+08:00","3.332975742069273","254492672","1252753408"
"2026-11-04T12:57:14.5191400+08:00","2.2217368566513254","255967232","1253724160"
"2026-11-04T12:57:15.5286680+08:00","1.969867466416349","256704512","1254461440"
"2026-11-04T12:57:16.5489372+08:00","2.0187408292920055","258801664","1256087552"
"2026-11-04T12:57:17.5623851+08:00","1.89216795097571","258805760","1256087552"
"2026-11-04T12:57:18.5806501+08:00","1.9529654496976363","259006464","1256280064"
"2026-11-04T12:57:19.5959193+08:00","2.168591882285551","261292032","1258299392"
"2026-11-04T12:57:20.6113452+08:00","2.4480323522823815","261591040","1258299392"
"2026-11-04T12:57:21.6249620+08:00","2.6626074433293097","261656576","1258299392"
"2026-11-04T12:57:22.6414815+08:00","2.4453986908716008","261713920","1258299392"
"2026-11-04T12:57:23.6522072+08:00","1.7567260650621448","261775360","1258303488"
"2026-11-04T12:57:24.6784144+08:00","2.422313402737239","263815168","1260347392"
"2026-11-04T12:57:25.6877581+08:00","1.5480356195813179","263933952","1260351488"
"2026-11-04T12:57:26.6980880+08:00","2.7415662583442932","263991296","1260351488"
"2026-11-04T12:57:27.7207269+08:00","2.0140629218281165","263991296","1260351488"
"2026-11-04T12:57:28.7360375+08:00","2.16850345643446","264278016","1260351488"
"2026-11-04T12:57:29.7482069+08:00","1.543713927727908","264273920","1260351488"
"2026-11-04T12:57:30.7604980+08:00","1.8943302340242212","264273920","1260351488"
"2026-11-04T12:57:31.7725564+08:00","2.6667074117102696","264429568","1260359680"
"2026-11-04T12:57:32.7840790+08:00","1.614914711023488","264486912","1260359680"
"2026-11-04T12:57:33.8006091+08:00","1.1877526928483118","264634368","1260363776"
"2026-11-04T12:57:34.8119313+08:00","1.8259175059055452","264687616","1260363776"
"2026-11-04T12:57:35.8188781+08:00","2.962375515225378","264687616","1260363776"
"2026-11-04T12:57:36.8329746+08:00","2.731383417294472","264859648","1260363776"
"2026-11-04T12:57:37.8487932+08:00","2.167419011085784","264916992","1260363776"
"2026-11-04T12:57:38.8660016+08:00","2.443742555159252","265572352","1260949504"
"2026-11-04T12:57:39.8744623+08:00","2.6762209339081195","265973760","1261363200"
"2026-11-04T12:57:40.8888959+08:00","1.750304979860862","267890688","1263292416"
"2026-11-04T12:57:41.9065560+08:00","2.652028547020401","268034048","1263292416"
"2026-11-04T12:57:42.9161400+08:00","1.1255761148786394","268046336","1263292416"
"2026-11-04T12:57:43.9365092+08:00","1.4617035409607353","268046336","1263292416"
"2026-11-04T12:57:44.9434258+08:00","1.410697316395961","266842112","1262030848"
"2026-11-04T12:57:45.9607240+08:00","1.9548214708689782","266842112","1262030848"
"2026-11-04T12:57:46.9744517+08:00","1.6814628371558304","266903552","1262030848"
"2026-11-04T12:57:47.9868021+08:00","2.104688078536659","267100160","1262227456"
"2026-11-04T12:57:49.0081886+08:00","1.8079257059799685","269185024","1264193536"
"2026-11-04T12:57:50.0194065+08:00","2.387984555329497","268447744","1263456256"
"2026-11-04T12:57:51.0351460+08:00","1.4683659272158587","268623872","1263456256"
"2026-11-04T12:57:52.0440518+08:00","2.3934570772342942","268632064","1263456256"
"2026-11-04T12:57:53.0549470+08:00","3.0913194562601545","266940416","1261772800"
"2026-11-04T12:57:54.0742759+08:00","1.532871284234166","266940416","1261772800"
"2026-11-04T12:57:55.0829934+08:00","2.675539619728652","266973184","1261772800"
"2026-11-04T12:57:56.1014048+08:00","1.882945965023208","266973184","1261772800"
"2026-11-04T12:57:57.1166069+08:00","1.6090616117448213","268107776","1262907392"
"2026-11-04T12:57:58.1375785+08:00","1.9477881300874222","268107776","1262907392"
"2026-11-04T12:57:59.1519893+08:00","1.6103167743016213","268107776","1262907392"
"2026-11-04T12:58:00.1659686+08:00","2.171350584232386","268169216","1262907392"
"2026-11-04T12:58:01.1800395+08:00","1.2606703248353648","268226560","1262907392"
"2026-11-04T12:58:02.1945232+08:00","1.6802097998671193","268226560","1262907392"
"2026-11-04T12:58:03.2020386+08:00","1.8328165595194965","268226560","1262907392"
"2026-11-04T12:58:04.2129941+08:00","2.0373390232399853","268226560","1262907392"
"2026-11-04T12:58:05.2424862+08:00","1.793691189170766","268406784","1262907392"
"2026-11-04T12:58:06.2495651+08:00","1.4809934680661792","268533760","1262907392"
"2026-11-04T12:58:07.2588506+08:00","1.4777555733509227","268537856","1262907392"
"2026-11-04T12:58:08.2667662+08:00","1.9025537816496105","268578816","1262796800"
"2026-11-04T12:58:09.2806377+08:00","2.7319895703117836","268640256","1262796800"
"2026-11-04T12:58:10.2988908+08:00","2.0924874357680014","268640256","1262796800"
"2026-11-04T12:58:11.3072235+08:00","1.8313309774550695","268640256","1262796800"
"2026-11-04T12:58:12.3248935+08:00","1.4655804658949094","267563008","1261711360"
"2026-11-04T12:58:13.3321461+08:00","1.9743174290504326","267554816","1261711360"
"2026-11-04T12:58:14.3494988+08:00","1.605660187733052","269606912","1264128000"
"2026-11-04T12:58:15.3642212+08:00","1.4698377336769868","269606912","1264128000"
"2026-11-04T12:58:16.3791448+08:00","1.6794815437787185","268492800","1263009792"
"2026-11-04T12:58:17.3888437+08:00","0.9847670249201834","267157504","1261670400"
"2026-11-04T12:58:18.4021608+08:00","2.032590874968054","267157504","1261670400"
"2026-11-04T12:58:19.4103531+08:00","2.113368469667759","267374592","1261670400"
"2026-11-04T12:58:20.4220100+08:00","1.8955177752097934","267399168","1261670400"
"2026-11-04T12:58:21.4324081+08:00","1.6870038201234292","267735040","1262026752"
"2026-11-04T12:58:22.4504963+08:00","1.9533045993818254","268734464","1262886912"
"2026-11-04T12:58:23.4606725+08:00","1.5467598622893706","268734464","1262886912"
"2026-11-04T12:58:24.4683210+08:00","1.550639930491635","268734464","1262886912"
"2026-11-04T12:58:25.4803131+08:00","2.1756143604822067","268759040","1262923776"
"2026-11-04T12:58:26.4959645+08:00","1.468493296742635","268759040","1262923776"
"2026-11-04T12:58:27.5164410+08:00","1.6703426826050913","268759040","1262923776"
"2026-11-04T12:58:28.5353112+08:00","1.6729760616665934","268759040","1262923776"
"2026-11-04T12:58:29.5517762+08:00","2.864763487362396","268759040","1262923776"
"2026-11-04T12:58:30.5687189+08:00","2.3047021233349727","269549568","1263783936"
"2026-11-04T12:58:31.5779104+08:00","2.3224036270618607","269549568","1263783936"
"2026-11-04T12:58:32.5844314+08:00","1.7640647158064082","269549568","1263783936"
"2026-11-04T12:58:33.5985967+08:00","1.9608601907759649","268062720","1262223360"
"2026-11-04T12:58:34.6119824+08:00","2.1726224728201173","268115968","1262223360"
"2026-11-04T12:58:35.6222898+08:00","1.8277515428382578","268115968","1262223360"
"2026-11-04T12:58:36.6355422+08:00","2.172908295558486","268115968","1262223360"
"2026-11-04T12:58:37.6429095+08:00","1.3395628567473037","268115968","1262223360"
"2026-11-04T12:58:38.6553733+08:00","1.8940071105392968","268124160","1262223360"
"2026-11-04T12:58:39.6727845+08:00","2.3036408484593056","268115968","1262223360"
"2026-11-04T12:58:40.6829183+08:00","2.320237180460648","268177408","1262223360"
"2026-11-04T12:58:41.6998236+08:00","1.955576751971264","268177408","1262223360"
"2026-11-04T12:58:42.7165256+08:00","1.3971198497244477","268177408","1262223360"
"2026-11-04T12:58:43.7291509+08:00","2.1041167134396286","268177408","1262223360"
"2026-11-04T12:58:44.7452419+08:00","2.9357159402598243","268177408","1262223360"
"2026-11-04T12:58:45.7538728+08:00","1.8307895475846607","268922880","1262968832"
"2026-11-04T12:58:46.7599632+08:00","1.2000774121653122","268681216","1262739456"
"2026-11-04T12:58:47.7688840+08:00","2.3230267430307707","270114816","1264308224"
"2026-11-04T12:58:48.7887941+08:00","1.1838164595451734","270290944","1264308224"
"2026-11-04T12:58:49.8017576+08:00","1.7528451734126471","269185024","1263132672"
"2026-11-04T12:58:50.8110979+08:00","2.462792236221475","269201408","1263132672"
"2026-11-04T12:58:51.8172044+08:00","2.258933097765766","269201408","1263132672"
"2026-11-04T12:58:52.8268307+08:00","1.547602315827153","269201408","1263132672"
"2026-11-04T12:58:53.8396994+08:00","1.5426481240855798","269205504","1263136768"
"2026-11-04T12:58:54.8505471+08:00","2.03755629152551","269205504","1263136768"
"2026-11-04T12:58:55.8684624+08:00","2.0931818376065454","271077376","1265172480"
"2026-11-04T12:58:56.8842707+08:00","1.8877711831687498","271073280","1265172480"
"2026-11-04T12:58:57.8928481+08:00","1.7604679440746756","271073280","1265172480"
"2026-11-04T12:58:58.8999064+08:00","2.045223291351743","269959168","1264058368"
"2026-11-04T12:58:59.9062018+08:00","1.5527249751911816","269959168","1264058368"
"2026-11-04T12:59:00.9164649+08:00","1.5466268143417294","269963264","1264058368"
"2026-11-04T12:59:01.9346227+08:00","2.162439403258066","270024704","1264058368"
"2026-11-04T12:59:02.9406388+08:00","1.9061460709859774","270815232","1264787456"
"2026-11-04T12:59:03.9494072+08:00","1.9009453868337238","268881920","1262759936"
"2026-11-04T12:59:04.9665556+08:00","1.5361573591424813","268881920","1262759936"
"2026-11-04T12:59:05.9804876+08:00","2.031358208350354","268943360","1262759936"
"2026-11-04T12:59:06.9924713+08:00","2.3861774920611145","245407744","1244913664"
"2026-11-04T12:59:08.0081229+08:00","1.0489235768357075","245407744","1244913664"
"2026-11-04T12:59:09.0162480+08:00","0.6340528030246896","245346304","1244344320"
"2026-11-04T12:59:10.0320097+08:00","0.8390479058943915","245350400","1244344320"
"2026-11-04T12:59:11.0444425+08:00","0.9821078315698404","245346304","1244344320"
"2026-11-04T12:59:12.0551144+08:00","0.9838189642139866","245346304","1244344320"
"2026-11-04T12:59:13.0665618+08:00","1.0532835509695404","245346304","1244344320"
"2026-11-04T12:59:14.0736568+08:00","0.9873131947017726","245346304","1244344320"
"2026-11-04T12:59:15.0890591+08:00","1.1191265140561886","245346304","1244344320"
"2026-11-04T12:59:16.1064397+08:00","1.2565691648819437","245346304","1244344320"
"2026-11-04T12:59:17.1115265+08:00","0.777296050450568","245350400","1244344320"
"2026-11-04T12:59:18.1211202+08:00","0.9848696379723664","245346304","1244344320"
"2026-11-04T12:59:19.1274348+08:00","0.5646164908884539","246075392","1245073408"
"2026-11-04T12:59:20.1439304+08:00","1.0480526517683981","245346304","1244344320"
"2026-11-04T12:59:21.1535223+08:00","0.8441754804814967","245346304","1244344320"
"2026-11-04T12:59:22.1683958+08:00","1.259673339494125","245346304","1244344320"
"2026-11-04T12:59:23.1780849+08:00","1.125458952031508","245346304","1244344320"
"2026-11-04T12:59:24.1833639+08:00","1.1303962744309155","245346304","1244344320"
"2026-11-04T12:59:25.2013921+08:00","0.8371798809431087","245346304","1244344320"
"2026-11-04T12:59:26.2090476+08:00","0.9162808663729365","245346304","1244344320"
"2026-11-04T12:59:27.2207454+08:00","1.0530228582990977","245346304","1244344320"
"2026-11-04T12:59:28.2320939+08:00","0.9831607816872047","245346304","1244344320"
"2026-11-04T12:59:29.2448001+08:00","1.262369175688952","245346304","1244344320"
"2026-11-04T12:59:30.2633222+08:00","0.48811811830994234","245346304","1244344320"
"2026-11-04T12:59:31.2739384+08:00","0.9838731872873023","245346304","1244344320"
"2026-11-04T12:59:32.2894006+08:00","0.6994128119463952","245346304","1244344320"
"2026-11-04T12:59:33.3035050+08:00","0.840419119838872","245346304","1244344320"
"2026-11-04T12:59:34.3118759+08:00","0.7747645236489866","245346304","1244344320"
"2026-11-04T12:59:35.3310436+08:00","0.7665568679227178","245350400","1244344320"
"2026-11-04T12:59:36.3404632+08:00","0.7035996455064601","245346304","1244344320"
"2026-11-04T12:59:37.3466089+08:00","0.35294454507298134","245346304","1244344320"
"2026-11-04T12:59:38.3590648+08:00","0.9820854239855602","245346304","1244344320"
"2026-11-04T12:59:39.3674765+08:00","0.9860240433725451","245370880","1244344320"
"2026-11-04T12:59:40.3755730+08:00","0.9863323420110891","249446400","1247481856"
"2026-11-04T12:59:41.3933048+08:00","1.8842033199352093","253497344","1251053568"
"2026-11-04T12:59:42.4064305+08:00","1.26184647266286","254509056","1251885056"
"2026-11-04T12:59:43.4174769+08:00","0.9834545494827754","255008768","1252167680"
"2026-11-04T12:59:44.4284327+08:00","1.6158201251456565","255819776","1252573184"
"2026-11-04T12:59:45.4393799+08:00","1.826594810382688","256208896","1253801984"
"2026-11-04T12:59:46.4545186+08:00","1.5391985351361344","256339968","1253801984"
"2026-11-04T12:59:47.4655655+08:00","1.4751810946923163","256897024","1254166528"
"2026-11-04T12:59:48.4793486+08:00","1.2610282129472183","258154496","1255002112"
"2026-11-04T12:59:49.4883652+08:00","1.266985192224876","258289664","1255088128"
"2026-11-04T12:59:50.4973796+08:00","1.478152613805385","258375680","1255211008"
"2026-11-04T12:59:51.5115626+08:00","1.1905014811295038","258793472","1255391232"
"2026-11-04T12:59:52.5314551+08:00","1.810574064512592","258842624","1255403520"
"2026-11-04T12:59:53.5493466+08:00","1.3257128271351302","259072000","1255649280"
"2026-11-04T12:59:54.5596698+08:00","1.2653466642249638","259985408","1256513536"
"2026-11-04T12:59:55.5727644+08:00","1.4020946765036013","259514368","1255923712"
"2026-11-04T12:59:56.5832377+08:00","1.476018488293825","259780608","1257041920"
"2026-11-04T12:59:57.6013611+08:00","1.5346862669102783","259874816","1257107456"
"2026-11-04T12:59:58.6068245+08:00","1.3420993923615898","259932160","1257132032"
"2026-11-04T12:59:59.6121162+08:00","1.6249241163263632","259997696","1257132032"
"2026-11-04T13:00:00.6185009+08:00","1.129154324746428","260059136","1257410560"
"2026-11-04T13:00:01.6341046+08:00","1.5384938042269836","260059136","1257410560"
"2026-11-04T13:00:02.6446470+08:00","1.8273265021743859","260083712","1257435136"
"2026-11-04T13:00:03.6595701+08:00","0.9796980498504584","260112384","1257435136"
"2026-11-04T13:00:04.6775251+08:00","1.5349401496136863","260116480","1257435136"
"2026-11-04T13:00:05.6867902+08:00","0.9851902952139947","260157440","1257435136"
"2026-11-04T13:00:06.7027898+08:00","0.6990428664807277","261025792","1258172416"
"2026-11-04T13:00:07.7162326+08:00","1.4716935901338217","260431872","1257435136"
"2026-11-04T13:00:08.7315497+08:00","1.6088793612091505","260497408","1257435136"
"2026-11-04T13:00:09.7406893+08:00","1.6187281990249192","260538368","1257435136"
"2026-11-04T13:00:10.7570820+08:00","1.0481587570344701","260550656","1257435136"
"2026-11-04T13:00:11.7653126+08:00","1.761073490348519","260558848","1257435136"
"2026-11-04T13:00:12.7838214+08:00","1.1854451956000416","260562944","1256493056"
"2026-11-04T13:00:13.7991161+08:00","1.1192451180564975","261316608","1256837120"
"2026-11-04T13:00:14.8171701+08:00","1.3255012191709066","261320704","1256837120"
"2026-11-04T13:00:15.8246303+08:00","1.3394393328707361","261320704","1256837120"
"2026-11-04T13:00:16.8390185+08:00","1.6803679839192278","261373952","1256837120"
"2026-11-04T13:00:17.8586515+08:00","1.2537933657591418","261373952","1256837120"
"2026-11-04T13:00:18.8775222+08:00","1.7426825423659567","261373952","1256837120"
"2026-11-04T13:00:19.8926924+08:00","0.9794595840364323","261373952","1256837120"
"2026-11-04T13:00:20.9002454+08:00","1.4802965925636398","262103040","1257566208"
"2026-11-04T13:00:21.9166574+08:00","2.0962777084310478","261373952","1256837120"
"2026-11-04T13:00:22.9340699+08:00","1.3961441848360872","261373952","1256837120"
"2026-11-04T13:00:23.9444838+08:00","2.10872180022644","261373952","1256837120"
"2026-11-04T13:00:24.9611622+08:00","1.7464403510669468","261373952","1256837120"
"2026-11-04T13:00:25.9676112+08:00","1.4113527316878904","261373952","1256837120"
"2026-11-04T13:00:26.9752859+08:00","1.4096360119536053","261382144","1256837120"
"2026-11-04T13:00:27.9921897+08:00","1.6063689871871139","261382144","1256837120"
"2026-11-04T13:00:29.0012289+08:00","2.1115946914468915","261738496","1256841216"
"2026-11-04T13:00:30.0169209+08:00","0.9090309410189846","261832704","1256935424"
"2026-11-04T13:00:31.0319964+08:00","1.9591019226021744","261828608","1256935424"
"2026-11-04T13:00:32.0396583+08:00","1.6915847017193508","261828608","1256935424"
"2026-11-04T13:00:33.0501819+08:00","1.475945017738599","261832704","1256939520"
"2026-11-04T13:00:34.0672535+08:00","1.6759345699412456","261832704","1256939520"
"2026-11-04T13:00:35.0763812+08:00","1.5483669708006231","261832704","1256939520"
"2026-11-04T13:00:36.0825711+08:00","1.623473588109687","261832704","1256939520"
"2026-11-04T13:00:37.0937547+08:00","1.3345072232004336","261844992","1256939520"
"2026-11-04T13:00:38.1116310+08:00","1.6048342291423106","261906432","1256939520"
"2026-11-04T13:00:39.1271840+08:00","1.6085056390682981","261906432","1256939520"
"2026-11-04T13:00:40.1385114+08:00","1.8961353527686844","261906432","1256939520"
"2026-11-04T13:00:41.1503863+08:00","1.684541690425817","261906432","1256939520"
"2026-11-04T13:00:42.1675217+08:00","1.2568720849840551","262635520","1257668608"
"2026-11-04T13:00:43.1831176+08:00","1.3986414729072314","261906432","1256939520"
"2026-11-04T13:00:44.2014904+08:00","1.8132759526677353","262635520","1257668608"
"2026-11-04T13:00:45.2211639+08:00","1.6020056687486017","261910528","1256943616"
"2026-11-04T13:00:46.2422511+08:00","1.1128957804618806","261918720","1256943616"
"2026-11-04T13:00:47.2517957+08:00","1.4773763068291117","261918720","1256943616"
"2026-11-04T13:00:48.2679610+08:00","1.6075364188018695","262053888","1256943616"
"2026-11-04T13:00:49.2834424+08:00","2.1681387226339597","262053888","1256943616"
"2026-11-04T13:00:50.2981052+08:00","1.4699240700726122","262053888","1256943616"
"2026-11-04T13:00:51.3101580+08:00","1.263184184569314","262053888","1256943616"
"2026-11-04T13:00:52.3287173+08:00","1.4643008735252556","262053888","1256943616"
"2026-11-04T13:00:53.3482656+08:00","1.1842365522421678","262062080","1256943616"
"2026-11-04T13:00:54.3626039+08:00","1.1903192097117536","262123520","1256943616"
"2026-11-04T13:00:55.3839076+08:00","1.2517423474614757","262123520","1256943616"
"2026-11-04T13:00:56.3925553+08:00","1.4082761953995884","262123520","1256943616"
"2026-11-04T13:00:57.4129364+08:00","1.2528741378187922","262139904","1256960000"
"2026-11-04T13:00:58.4204486+08:00","1.127890695878061","262139904","1256960000"
"2026-11-04T13:00:59.4336266+08:00","1.2617813364572574","262139904","1256960000"
"2026-11-04T13:01:00.4407588+08:00","1.0577964929439343","262868992","1257689088"
"2026-11-04T13:01:01.4613340+08:00","1.4614085005468218","262139904","1256960000"
"2026-11-04T13:01:02.4702479+08:00","1.337509393201757","262164480","1256960000"
"2026-11-04T13:01:03.4879407+08:00","1.9540635087880778","262164480","1256960000"
"2026-11-04T13:01:04.4974096+08:00","1.336774038488772","262164480","1256960000"
"2026-11-04T13:01:05.5104914+08:00","1.2619011524134487","262164480","1256960000"
"2026-11-04T13:01:06.5245584+08:00","1.750937740620868","262164480","1256960000"
"2026-11-04T13:01:07.5313320+08:00","1.1992630355388376","262164480","1256960000"
"2026-11-04T13:01:08.5432455+08:00","1.4037311938763002","262164480","1256960000"
"2026-11-04T13:01:09.5595682+08:00","1.5374053929918126","262164480","1256960000"
"2026-11-04T13:01:10.5652672+08:00","1.6242660351384735","262365184","1256960000"
"2026-11-04T13:01:11.5776545+08:00","1.1926131072924004","262422528","1256960000"
"2026-11-04T13:01:12.5967454+08:00","1.6029215129609413","262422528","1256960000"
"2026-11-04T13:01:13.6151686+08:00","1.3250206968790756","262483968","1256960000"
"2026-11-04T13:01:14.6317217+08:00","1.0479933700373438","263213056","1257689088"
"2026-11-04T13:01:15.6492917+08:00","1.3261316844854094","262483968","1256960000"
"2026-11-04T13:01:16.6582796+08:00","1.8301417778061653","262483968","1256960000"
"2026-11-04T13:01:17.6793400+08:00","1.3215984266766374","262483968","1256960000"
"2026-11-04T13:01:18.6969583+08:00","1.535448016215903","262483968","1256960000"
"2026-11-04T13:01:19.7099719+08:00","1.3320964478481019","262483968","1256960000"
"2026-11-04T13:01:20.7258680+08:00","1.1884939450366663","262483968","1256960000"
"2026-11-04T13:01:21.7333962+08:00","1.1983648334968326","262479872","1256960000"
"2026-11-04T13:01:22.7536006+08:00","1.2530911363537454","262479872","1256960000"
"2026-11-04T13:01:23.7719144+08:00","1.8133810119149019","262479872","1256960000"
"2026-11-04T13:01:24.7877746+08:00","1.1885359458283369","262479872","1256960000"
"2026-11-04T13:01:25.8053836+08:00","1.8844306962336577","263208960","1257689088"
"2026-11-04T13:01:26.8255806+08:00","1.7404169800716742","262479872","1256960000"
"2026-11-04T13:01:27.8405390+08:00","0.9096879778968817","262541312","1256960000"
"2026-11-04T13:01:28.8595317+08:00","1.6727749419063103","262541312","1256960000"
"2026-11-04T13:01:29.8654777+08:00","1.4826613682317669","262541312","1256960000"
"2026-11-04T13:01:30.8798493+08:00","1.330313090569391","262541312","1256960000"
"2026-11-04T13:01:31.8864135+08:00","1.4817507643598617","262541312","1256960000"
"2026-11-04T13:01:32.9007297+08:00","1.0503045392461534","262541312","1256960000"
"2026-11-04T13:01:33.9208403+08:00","1.6013192366325057","262541312","1256960000"
"2026-11-04T13:01:34.9301508+08:00","1.2666162602183282","263270400","1257689088"
"2026-11-04T13:01:35.9503918+08:00","1.2530461831166273","262541312","1256960000"
"2026-11-04T13:01:36.9715877+08:00","1.1823259020491206","262541312","1256960000"
"2026-11-04T13:01:37.9907290+08:00","1.4634646566940939","262553600","1256972288"
"2026-11-04T13:01:39.0079917+08:00","1.3265322892324845","262553600","1256972288"
"2026-11-04T13:01:40.0155288+08:00","0.9163885424620638","262553600","1256972288"
"2026-11-04T13:01:41.0264528+08:00","0.7025525882531949","262553600","1256972288"
"2026-11-04T13:01:42.0441470+08:00","0.6978788645226364","262553600","1256972288"
"2026-11-04T13:01:43.0637344+08:00","1.0448745336504837","262553600","1256972288"
"2026-11-04T13:01:44.0811897+08:00","1.186672636759928","262553600","1256972288"
"2026-11-04T13:01:45.0997853+08:00","0.9064396651089545","262553600","1256972288"
"2026-11-04T13:01:46.1192509+08:00","0.8359994954932538","263282688","1257701376"
"2026-11-04T13:01:47.1265959+08:00","0.9165632971280491","262553600","1256972288"
"2026-11-04T13:01:48.1433119+08:00","0.9779704281413707","262553600","1256972288"
"2026-11-04T13:01:49.1569377+08:00","0.980951927050576","262553600","1256972288"
"2026-11-04T13:01:50.1623113+08:00","0.8477174328754278","262553600","1256972288"
"2026-11-04T13:01:51.1814521+08:00","0.8362659283905888","262553600","1256972288"
"2026-11-04T13:01:52.1993180+08:00","0.9768655987180449","262553600","1256972288"
"2026-11-04T13:01:53.2095747+08:00","1.0545249629038926","263282688","1257701376"
"2026-11-04T13:01:54.2295296+08:00","0.7659652402277786","262553600","1256972288"
"2026-11-04T13:01:55.2461833+08:00","0.9081710463901863","262553600","1256972288"
"2026-11-04T13:01:56.2594322+08:00","0.8411286972753953","262553600","1256972288"
"2026-11-04T13:01:57.2748195+08:00","0.6295179636918301","262553600","1256972288"
"2026-11-04T13:01:58.2912939+08:00","0.9782028763520084","262553600","1256972288"
"2026-11-04T13:01:59.3057356+08:00","0.9801629623646011","262332416","1256648704"

```

### composition-r383-vlc.log

```text
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
file debug: opening logfile `D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r383-vlc.log'
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
main debug: stream filter added to 01bce5d8
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
mmdevice debug: version 2 session control unavailable
mmdevice debug: display name changed: VLC media player (LibVLC 3.0.23)
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
qt debug: IM: Setting an input
main debug: `udp://@127.0.0.1:62024' gives access `udp' demux `any' path `@127.0.0.1:62024'
main debug: creating demux: access='udp' demux='any' location='@127.0.0.1:62024' file='\\@127.0.0.1:62024'
main debug: looking for access_demux module matching "udp": 15 candidates
main debug: no access_demux modules matched
main debug: creating access: udp://@127.0.0.1:62024
main debug:  (path: \\@127.0.0.1:62024)
main debug: looking for access module matching "udp": 27 candidates
udp debug: opening server=:0 local=127.0.0.1:62024
main debug: net: opening 127.0.0.1 datagram port 62024
main debug: looking for meta fetcher module matching "any": 1 candidates
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\meta\fetcher
lua debug: Trying Lua scripts in D:\VideoLAN\VLC\lua\meta\fetcher
main debug: no meta fetcher modules matched
main debug: looking for art finder module matching "any": 2 candidates
main debug: using access module "udp"
main debug: looking for stream_filter module matching "prefetch,cache_block": 24 candidates
prefetch debug: using 16777216 bytes buffer, 16777216 bytes read
main debug: using stream_filter module "prefetch"
main debug: looking for stream_filter module matching "any": 24 candidates
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\meta\art
lua debug: Trying Lua scripts in D:\VideoLAN\VLC\lua\meta\art
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\art\00_musicbrainz.luac
lua debug: skipping script (unmatched scope) D:\VideoLAN\VLC\lua\meta\art\00_musicbrainz.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\art\01_googleimage.luac
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
main debug: resized to 86623744x66584976
main debug: resized to 86623744x66584976
main debug: resized to 86623232x66584976
main debug: resized to 86623232x66584976
main debug: resized to 86622976x66584976
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
direct3d11 debug: inclinometer not found. (hr=0x80070490)
direct3d11 debug: created video sub-window
direct3d11 debug: Vout EventThread running
main debug: Buffering 19%
wasapi debug: modified format
main debug: Buffering 26%
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
direct3d11 debug: using GPU render fence
main debug: Buffering 33%
direct3d11 debug: Direct3D11 resources created
direct3d11 debug: Direct3D11 device adapter successfully initialized
direct3d11 debug: Direct3D11 Open Succeeded
main debug: using vout display module "direct3d11"
direct3d11 debug: D3D11 pool succeed with 48 surfaces (1280x736) context 0x0b5400e0
main debug: original format sz 1280x736, of (0,0), vsz 1280x720, 4cc DX11, sar 1:1, msk r0x0 g0x0 b0x0
main debug: removing module "freetype"
main debug: VoutDisplayEvent 'resize' 1280x720
main debug: looking for text renderer module matching "any": 3 candidates
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
d3d11va debug: - 'Unknown decoder 0x15df9b21-0x06c4-0x47f1-0x841ea67c97d7f312' is supported
d3d11va debug: - 'Unknown decoder 0x4008018f-0xf537-0x4b36-0x98cf61af8a2c1a33' is supported
d3d11va debug: - 'Unknown decoder 0x0dabeffa-0x4458-0x4602-0xbc030795659d617c' is supported
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
qt debug: Logical video size: 1280x720
main debug: resized to 1280x720
main debug: VoutDisplayEvent 'resize' 1280x720
d3d11va debug: ID3D11VideoDecoderOutputView succeed with 28 surfaces (1280x768)
d3d11va debug: we got 2 decoder configurations
d3d11va debug: configuration[0] ConfigBitstreamRaw 1
d3d11va debug: configuration[1] ConfigBitstreamRaw 1
d3d11va debug: DxCreateDecoderSurfaces succeed
main debug: using hw decoder module "d3d11va"
avcodec: Using D3D11VA (NVIDIA GeForce RTX 4060 Laptop GPU, vendor 10de(NVIDIA), device 28e0, revision a1) for hardware decoding
main debug: Buffering 39%
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
main debug: Stream buffering done (1066 ms in 943 ms)
main debug: Decoder wait done in 0 ms
main debug: inserting 9594 zeroes
mmdevice debug: state changed: 1
main warning: playback too late (78702): up-sampling
main debug: auto hiding mouse cursor
main warning: picture is too late to be displayed (missing 36 ms)
main debug: picture might be displayed late (missing 3 ms)
main debug: accepted socket 3364 (from socket 1188)
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
main debug: Filter 'd3d11_filters' (1b7223a0) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (1b7238e0) appended to chain
main debug: using video converter module "chain"
main debug: Filter 'chain' (1b7240d8) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: RV24 -> 1280x720 (1280x720) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (1b723b88) appended to chain
main debug: using video converter module "chain"
main debug: removing module "png"
main debug: removing module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 1b7223a0 removed from chain
main debug: removing module "swscale"
main debug: Filter 1b7238e0 removed from chain
main debug: Filter 1b7240d8 removed from chain
main debug: removing module "swscale"
main debug: Filter 1b723b88 removed from chain
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r383-native-2026-11-04-12h57m26s236.png)
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (1b722648) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: YUVA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (1b723b88) appended to chain
main debug: using video converter module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 1b722648 removed from chain
main debug: removing module "swscale"
main debug: Filter 1b723b88 removed from chain
main error: read error: No error
main debug: resampling stopped (drift: -16163 us)
main debug: accepted socket 1304 (from socket 1188)
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
main debug: Filter 'd3d11_filters' (1bd38ba0) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (1bd39640) appended to chain
main debug: using video converter module "chain"
main debug: Filter 'chain' (1bd38100) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: RV24 -> 1280x720 (1280x720) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (1bd38e48) appended to chain
main debug: using video converter module "chain"
main debug: removing module "png"
main debug: removing module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 1bd38ba0 removed from chain
main debug: removing module "swscale"
main debug: Filter 1bd39640 removed from chain
main debug: Filter 1bd38100 removed from chain
main debug: removing module "swscale"
main debug: Filter 1bd38e48 removed from chain
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r383-native-2026-11-04-12h59m27s478.png)
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (1bd38e48) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: YUVA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (1bd38ba0) appended to chain
main debug: using video converter module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 1bd38e48 removed from chain
main debug: removing module "swscale"
main debug: Filter 1bd38ba0 removed from chain
main debug: accepted socket 2184 (from socket 1188)
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
main debug: Filter 'd3d11_filters' (05593560) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (05592ac0) appended to chain
main debug: using video converter module "chain"
main debug: Filter 'chain' (05592818) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: RV24 -> 1280x720 (1280x720) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (05594000) appended to chain
main debug: using video converter module "chain"
main debug: removing module "png"
main debug: removing module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 05593560 removed from chain
main debug: removing module "swscale"
main debug: Filter 05592ac0 removed from chain
main debug: Filter 05592818 removed from chain
main debug: removing module "swscale"
main debug: Filter 05594000 removed from chain
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r383-native-2026-11-04-13h00m04s846.png)
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (05593d58) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: YUVA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (055932b8) appended to chain
main debug: using video converter module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 05593d58 removed from chain
main debug: removing module "swscale"
main debug: Filter 055932b8 removed from chain
main debug: accepted socket 3176 (from socket 1188)
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
main debug: Filter 'd3d11_filters' (064f6040) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (064f7030) appended to chain
main debug: using video converter module "chain"
main debug: Filter 'chain' (064f6d88) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: RV24 -> 1280x720 (1280x720) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (064f62e8) appended to chain
main debug: using video converter module "chain"
main debug: removing module "png"
main debug: removing module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 064f6040 removed from chain
main debug: removing module "swscale"
main debug: Filter 064f7030 removed from chain
main debug: Filter 064f6d88 removed from chain
main debug: removing module "swscale"
main debug: Filter 064f62e8 removed from chain
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r383-native-2026-11-04-13h01m46s776.png)
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (064f5848) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: YUVA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (064f6ae0) appended to chain
main debug: using video converter module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 064f5848 removed from chain
main debug: removing module "swscale"
main debug: Filter 064f6ae0 removed from chain
main debug: accepted socket 4016 (from socket 1188)
main debug: exiting
main debug: exiting
main debug: no exit handler
main debug: removing all interfaces
main debug: removing module "qt"
main debug: deactivating the playlist
main debug: incoming request - stopping current input
prefetch debug: end of stream
main debug: incoming request - stopping current input
ts debug: Can't read TS packet at 203955748
main debug: EOF reached
main debug: incoming request - stopping current input
main warning: picture is too late to be displayed (missing 222 ms)
main error: read error: No error
main warning: picture is too late to be displayed (missing 191 ms)
main warning: picture is too late to be displayed (missing 157 ms)
main warning: picture is too late to be displayed (missing 124 ms)
main warning: picture is too late to be displayed (missing 91 ms)
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
main debug: saving media library to file C:\Users\ROG\AppData\Roaming\vlc\ml.xspf.tmp13708
main debug: looking for playlist export module matching "export-xspf": 4 candidates
main debug: using playlist export module "export"
main debug: removing module "export"
main debug: deleting item `Media Library'
main debug: deleting item `udp://127.0.0.1:62024'
main debug: deleting item `Playlist'
main debug: removing module "memory"
-- logger module stopped --

```

### source0 源结果

```text
frame= 3575 fps= 30 q=-1.0 size=  118060KiB time=00:01:59.16 bitrate=8115.9kbits/s speed=   1x elapsed=0:01:58.67    
frame= 3591 fps= 30 q=-1.0 size=  118561KiB time=00:01:59.70 bitrate=8114.0kbits/s speed=   1x elapsed=0:01:59.18    
[out#0/rtp @ 000001f862f6df00] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
[out#1/rtp @ 000001f862ffe040] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.49    
```

### source1 源结果

```text
frame= 3562 fps= 30 q=-1.0 size=  117635KiB time=00:01:58.73 bitrate=8116.2kbits/s speed=   1x elapsed=0:01:58.20    
frame= 3577 fps= 30 q=-1.0 size=  118184KiB time=00:01:59.23 bitrate=8119.9kbits/s speed=   1x elapsed=0:01:58.72    
frame= 3592 fps= 30 q=-1.0 size=  118576KiB time=00:01:59.73 bitrate=8112.8kbits/s speed=   1x elapsed=0:01:59.24    
[out#0/rtp @ 000001bb8c3fc1c0] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.47    
```

### source2 源结果

```text
frame= 3561 fps= 30 q=-1.0 size=  117603KiB time=00:01:58.70 bitrate=8116.3kbits/s speed=   1x elapsed=0:01:58.19    
frame= 3576 fps= 30 q=-1.0 size=  118130KiB time=00:01:59.20 bitrate=8118.5kbits/s speed=   1x elapsed=0:01:58.70    
frame= 3592 fps= 30 q=-1.0 size=  118576KiB time=00:01:59.73 bitrate=8112.8kbits/s speed=   1x elapsed=0:01:59.23    
[out#0/rtp @ 000001d3f1ecdfc0] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.47    
```

### reentry 源结果

```text
frame= 3573 fps= 30 q=-1.0 size=  118036KiB time=00:01:59.11 bitrate=8117.6kbits/s speed=   1x elapsed=0:01:58.61    
frame= 3589 fps= 30 q=-1.0 size=  118528KiB time=00:01:59.63 bitrate=8116.3kbits/s speed=   1x elapsed=0:01:59.12    
[out#0/rtp @ 000002a0d268cf80] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
[out#1/rtp @ 000002a0d446e740] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.47    
```

## 本轮清理清单

```json
[
  {
    "FullName": "D:\\Code\\MyCode\\MediaTranscode\\out\\acceptance\\composition-r383-cli-error.log",
    "Length": 1968
  },
  {
    "FullName": "D:\\Code\\MyCode\\MediaTranscode\\out\\acceptance\\composition-r383-cli.log",
    "Length": 1257935
  },
  {
    "FullName": "D:\\Code\\MyCode\\MediaTranscode\\out\\acceptance\\composition-r383-monitor.csv",
    "Length": 23865
  },
  {
    "FullName": "D:\\Code\\MyCode\\MediaTranscode\\out\\acceptance\\composition-r383-native-2026-11-04-12h57m26s236.png",
    "Length": 793308
  },
  {
    "FullName": "D:\\Code\\MyCode\\MediaTranscode\\out\\acceptance\\composition-r383-native-2026-11-04-12h59m27s478.png",
    "Length": 12152
  },
  {
    "FullName": "D:\\Code\\MyCode\\MediaTranscode\\out\\acceptance\\composition-r383-native-2026-11-04-13h00m04s846.png",
    "Length": 324894
  },
  {
    "FullName": "D:\\Code\\MyCode\\MediaTranscode\\out\\acceptance\\composition-r383-native-2026-11-04-13h01m46s776.png",
    "Length": 12152
  },
  {
    "FullName": "D:\\Code\\MyCode\\MediaTranscode\\out\\acceptance\\composition-r383-reentry-stdout.log",
    "Length": 540
  },
  {
    "FullName": "D:\\Code\\MyCode\\MediaTranscode\\out\\acceptance\\composition-r383-reentry.log",
    "Length": 29521
  },
  {
    "FullName": "D:\\Code\\MyCode\\MediaTranscode\\out\\acceptance\\composition-r383-source0-stdout.log",
    "Length": 540
  },
  {
    "FullName": "D:\\Code\\MyCode\\MediaTranscode\\out\\acceptance\\composition-r383-source0.log",
    "Length": 29521
  },
  {
    "FullName": "D:\\Code\\MyCode\\MediaTranscode\\out\\acceptance\\composition-r383-source1-stdout.log",
    "Length": 335
  },
  {
    "FullName": "D:\\Code\\MyCode\\MediaTranscode\\out\\acceptance\\composition-r383-source1.log",
    "Length": 28926
  },
  {
    "FullName": "D:\\Code\\MyCode\\MediaTranscode\\out\\acceptance\\composition-r383-source2-stdout.log",
    "Length": 335
  },
  {
    "FullName": "D:\\Code\\MyCode\\MediaTranscode\\out\\acceptance\\composition-r383-source2.log",
    "Length": 28926
  },
  {
    "FullName": "D:\\Code\\MyCode\\MediaTranscode\\out\\acceptance\\composition-r383-vlc.log",
    "Length": 40895
  }
]
```

## 清理结果

归档核验后按上述16项绝对路径及文件长度逐项删除，r383文件零残留；六个测试PID均不存在，UDP62024与TCP63020监听均不存在。指定复用源保留124427809字节。本轮未创建RKMPP远程产物。清理未改变caller-stop退出语义。
