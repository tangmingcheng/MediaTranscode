# r384：runtime registry 共同账本修正后 Windows realtime 复验

日期：2026-11-04。修后全量 Release 构建63644配置/构建退出0，792目标，产物已核验。两名独立审查者对修正后四文件均PASS。此前费用身份冲突FAIL保留在完整准入计划。

## 规格与结果

三路H.264输入（其中一路AAC所选音频）→ HEVC CBR 8000kbps、1280×720、30fps、GOP60，AAC CBR 192kbps、44100Hz双声道 → MPEG-TS/UDP。全部使用指定120秒源，无循环、降规格或额外FFmpeg监控。初始FFmpeg PID40148/34112/38820，CLI34096，VLC2688；重入FFmpeg35152。三个初始源与重入源均3600帧、120秒退出0；CLI caller-stop退出0，VLC通过RC quit退出。全失源期间CLI持续运行，不能记作自然EOF。

VLC原生PNG已逐张查看：13:29:31.429三格动态游戏画面、右下黑；13:31:17.337全黑；13:31:49.511仅左下所选源恢复；13:33:35.802再次全黑。

最终workers/errors/workerErrors/stalledIntervals/droppedBuffers/pressureFailures均0，graphPayloadCurrentBytes/Objects归零，220555次预约与释放匹配。内部CPU均值1.730000%、峰值5.816135%；工作集初始209924096、峰值269582336、最终259751936字节。外部1秒采样271点，CPU均值1.710685%、峰值3.888822%，工作集峰值269582336字节；采样窗口不同，不能混用两套峰值。generation2观察到raw_phase_ns=15873，不等于内容级A/V证据。

sender提交172601数据报、203415248载荷字节，deadline/pressure/partial/ambiguous failures均0；最差lateness26130860ns（seq149940），backlog最大50.5901ms。delivery_evidence=not_proven。本轮四阶段行为和排空复验通过，但不构成wire、内容级静音/A/V、长期内存、完整准入或RKMPP验收，也不关闭r380历史发送失败。stderr帧属性变化警告原样保留。

## 实际启动及后台采样命令

```powershell
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r384-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r384-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r384-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r384-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r384-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r384-source2.log' -PassThru
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r384 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r384-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r384-cli-error.log' -PassThru
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r384-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r384-native- --snapshot-format=png udp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
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
$samples | Export-Csv -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r384-monitor.csv' -NoTypeInformation -Encoding UTF8
$source0.WaitForExit()
$source1.WaitForExit()
$source2.WaitForExit()
Write-Output "SOURCE0_EXIT=$($source0.ExitCode) SOURCE1_EXIT=$($source1.ExitCode) SOURCE2_EXIT=$($source2.ExitCode)"


```

## VLC原生快照命令（四阶段各执行一次）

```powershell
$cli=Get-CimInstance Win32_Process -Filter 'ProcessId=34096'
if(-not $cli -or $cli.Name -ne 'media_transcode_realtime_video_cli.exe' -or $cli.CommandLine -notlike '*composition-r384*'){throw 'r384 CLI is not alive'}
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 2688){throw 'VLC owner mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
try{$s=$c.GetStream();$s.ReadTimeout=2000;$b=[Text.Encoding]::ASCII.GetBytes('snapshot'+[char]10);$s.Write($b,0,$b.Length);$buf=[byte[]]::new(4096);try{$n=$s.Read($buf,0,$buf.Length);[Text.Encoding]::UTF8.GetString($buf,0,$n)}catch{Write-Output $_.Exception.Message}}finally{$c.Dispose()}
Get-ChildItem -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance' -Filter 'composition-r384-native-*.png' | Select-Object FullName,LastWriteTime
```

## 重入命令

```powershell
$reentry=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r384-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r384-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($reentry.Id)"
$reentry.WaitForExit()
Write-Output "REENTRY_EXIT=$($reentry.ExitCode)"
```

## 观察结束后的caller-stop及VLC退出命令

```powershell
$target=Get-CimInstance Win32_Process -Filter "ProcessId=34096"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r384*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R384Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R384Console]::FreeConsole() | Out-Null
if(-not [R384Console]::AttachConsole(34096)){throw 'AttachConsole failed'}
try {
[R384Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R384Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 34096 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R384Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R384Console]::FreeConsole() | Out-Null}
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 2688){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
try{$s=$c.GetStream();$b=[Text.Encoding]::ASCII.GetBytes('quit'+[char]10);$s.Write($b,0,$b.Length)}finally{$c.Dispose()}


```

## 清理前证据归档

以下保留非周期运行诊断、首尾漂移、错误日志、源尾部、采样及VLC日志。PNG观察结果见上；按清单删除原始临时文件，不删除复用源。
### CLI非周期诊断
```text
[CLI] composition_inputs=3 grid=fixed audio_source=2 audio=on max_duration=source_driven hw=planner-highest-score
[2026-11-04 13:29:12.970] [info] [graph][planner.capability] composition_protocol_installed source=2
[2026-11-04 13:29:12.984] [info] [graph][planner.capability] composition_protocol_installed source=1
[2026-11-04 13:29:12.984] [info] [graph][planner.capability] composition_protocol_installed source=0
[2026-11-04 13:29:13.071] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-04 13:29:13.072] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-04 13:29:13.072] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-r384.source.0 decoder=h264
[2026-11-04 13:29:17.486] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-11-04 13:29:17.486] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-11-04 13:29:17.516] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-11-04 13:29:17.589] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-04 13:29:17.590] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-04 13:29:17.590] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-r384.source.1 decoder=h264
[2026-11-04 13:29:17.650] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-04 13:29:17.650] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-04 13:29:17.650] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-r384.source.2 decoder=h264
[2026-11-04 13:29:17.741] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-11-04 13:29:17.779] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-11-04 13:29:17.779] [info] [graph][planner.capability] canvas_cuda_stream stage=prepared device_stream=0000000000000000 private_nonblocking=1
[2026-11-04 13:29:17.786] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-04 13:29:17.787] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-04 13:29:17.787] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-11-04 13:29:17.791] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-04 13:29:17.791] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-04 13:29:17.792] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-11-04 13:29:17.796] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-04 13:29:17.797] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-04 13:29:17.797] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-11-04 13:29:17.800] [info] [graph][planner.capability] composition_preparation_storage stage=graph_bound scope=shared_reservations capacity=2147483648 retained=831813013 high_water=840542807
[CLI] audio_plan branch=transcode_frame reason=transcode_required_by_output_frame_contract codec=aac sample_rate=44100 channels=2 access_unit_samples=1024 encoder=aac bitrate_kbps=192
[CLI] prepared_output_id=1
[CLI] selected_chain=fixed-grid-composition score=0 decoder=h264,h264,h264 filter=scale_cuda=640:360,scale_cuda=640:360,scale_cuda=640:360 encoder=hevc_nvenc
[2026-11-04 13:29:17.802] [info] [graph][runtime.lifecycle] compile.begin
[2026-11-04 13:29:17.803] [info] [graph][runtime.channel] create channel=1 edge=1 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 13:29:17.804] [info] [graph][runtime.channel] create channel=2 edge=2 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 13:29:17.804] [info] [graph][runtime.channel] create channel=3 edge=3 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-04 13:29:17.804] [info] [graph][runtime.channel] create channel=4 edge=4 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.804] [info] [graph][runtime.channel] create channel=5 edge=5 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.804] [info] [graph][runtime.channel] create channel=6 edge=6 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.804] [info] [graph][runtime.channel] create channel=7 edge=7 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.804] [info] [graph][runtime.channel] create channel=8 edge=8 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.804] [info] [graph][runtime.channel] create channel=9 edge=9 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 13:29:17.804] [info] [graph][runtime.channel] create channel=10 edge=10 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.805] [info] [graph][runtime.channel] create channel=11 edge=11 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.805] [info] [graph][runtime.channel] create channel=12 edge=12 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 13:29:17.805] [info] [graph][runtime.channel] create channel=13 edge=13 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 13:29:17.805] [info] [graph][runtime.channel] create channel=14 edge=14 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.805] [info] [graph][runtime.channel] create channel=15 edge=15 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.805] [info] [graph][runtime.channel] create channel=16 edge=16 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.805] [info] [graph][runtime.channel] create channel=17 edge=17 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.805] [info] [graph][runtime.channel] create channel=18 edge=18 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.805] [info] [graph][runtime.channel] create channel=19 edge=19 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.806] [info] [graph][runtime.channel] create channel=20 edge=20 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.806] [info] [graph][runtime.channel] create channel=21 edge=21 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-04 13:29:17.806] [info] [graph][runtime.channel] create channel=22 edge=22 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 13:29:17.806] [info] [graph][runtime.channel] create channel=23 edge=23 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 13:29:17.806] [info] [graph][runtime.channel] create channel=24 edge=24 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 13:29:17.806] [info] [graph][runtime.channel] create channel=25 edge=25 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 13:29:17.806] [info] [graph][runtime.channel] create channel=26 edge=26 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 13:29:17.806] [info] [graph][runtime.channel] create channel=27 edge=27 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 13:29:17.806] [info] [graph][runtime.channel] create channel=28 edge=28 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 13:29:17.807] [info] [graph][runtime.channel] create channel=29 edge=29 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 13:29:17.807] [info] [graph][runtime.channel] create channel=30 edge=30 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.807] [info] [graph][runtime.channel] create channel=31 edge=31 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.807] [info] [graph][runtime.channel] create channel=32 edge=32 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.807] [info] [graph][runtime.channel] create channel=33 edge=33 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.807] [info] [graph][runtime.channel] create channel=34 edge=34 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.807] [info] [graph][runtime.channel] create channel=35 edge=35 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.807] [info] [graph][runtime.channel] create channel=36 edge=36 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 13:29:17.808] [info] [graph][runtime.channel] create channel=37 edge=37 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.808] [info] [graph][runtime.channel] create channel=38 edge=38 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.808] [info] [graph][runtime.channel] create channel=39 edge=39 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 13:29:17.808] [info] [graph][runtime.channel] create channel=40 edge=40 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 13:29:17.808] [info] [graph][runtime.channel] create channel=41 edge=41 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.808] [info] [graph][runtime.channel] create channel=42 edge=42 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.808] [info] [graph][runtime.channel] create channel=43 edge=43 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.808] [info] [graph][runtime.channel] create channel=44 edge=44 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.808] [info] [graph][runtime.channel] create channel=45 edge=45 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.808] [info] [graph][runtime.channel] create channel=46 edge=46 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.808] [info] [graph][runtime.channel] create channel=47 edge=47 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.809] [info] [graph][runtime.channel] create channel=48 edge=48 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-04 13:29:17.809] [info] [graph][runtime.channel] create channel=49 edge=49 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 13:29:17.809] [info] [graph][runtime.channel] create channel=50 edge=50 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 13:29:17.809] [info] [graph][runtime.channel] create channel=51 edge=51 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 13:29:17.809] [info] [graph][runtime.channel] create channel=52 edge=52 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 13:29:17.809] [info] [graph][runtime.channel] create channel=53 edge=53 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 13:29:17.809] [info] [graph][runtime.channel] create channel=54 edge=54 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 13:29:17.809] [info] [graph][runtime.channel] create channel=55 edge=55 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 13:29:17.809] [info] [graph][runtime.channel] create channel=56 edge=56 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 13:29:17.810] [info] [graph][runtime.channel] create channel=57 edge=57 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.810] [info] [graph][runtime.channel] create channel=58 edge=58 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.810] [info] [graph][runtime.channel] create channel=59 edge=59 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.810] [info] [graph][runtime.channel] create channel=60 edge=60 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.810] [info] [graph][runtime.channel] create channel=61 edge=61 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.810] [info] [graph][runtime.channel] create channel=62 edge=62 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.810] [info] [graph][runtime.channel] create channel=63 edge=63 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.810] [info] [graph][runtime.channel] create channel=64 edge=64 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.810] [info] [graph][runtime.channel] create channel=65 edge=65 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.810] [info] [graph][runtime.channel] create channel=66 edge=66 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 13:29:17.811] [info] [graph][runtime.channel] create channel=67 edge=67 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 13:29:17.811] [info] [graph][runtime.channel] create channel=68 edge=68 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.811] [info] [graph][runtime.channel] create channel=69 edge=69 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.811] [info] [graph][runtime.channel] create channel=70 edge=70 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.811] [info] [graph][runtime.channel] create channel=71 edge=71 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 13:29:17.811] [info] [graph][runtime.channel] create channel=72 edge=72 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 13:29:17.811] [info] [graph][runtime.channel] create channel=73 edge=73 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 13:29:17.811] [info] [graph][runtime.channel] create channel=74 edge=74 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 13:29:17.811] [info] [graph][runtime.channel] create channel=75 edge=75 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.811] [info] [graph][runtime.channel] create channel=76 edge=76 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.812] [info] [graph][runtime.channel] create channel=77 edge=77 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.812] [info] [graph][runtime.channel] create channel=78 edge=78 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.812] [info] [graph][runtime.channel] create channel=79 edge=79 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.812] [info] [graph][runtime.channel] create channel=80 edge=80 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.812] [info] [graph][runtime.channel] create channel=81 edge=81 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.812] [info] [graph][runtime.channel] create channel=82 edge=82 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.812] [info] [graph][runtime.channel] create channel=83 edge=83 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-04 13:29:17.812] [info] [graph][runtime.channel] create channel=84 edge=84 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 13:29:17.812] [info] [graph][runtime.channel] create channel=85 edge=85 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 13:29:17.813] [info] [graph][runtime.channel] create channel=86 edge=86 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 13:29:17.813] [info] [graph][runtime.channel] create channel=87 edge=87 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 13:29:17.813] [info] [graph][runtime.channel] create channel=88 edge=88 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 13:29:17.813] [info] [graph][runtime.channel] create channel=89 edge=89 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 13:29:17.813] [info] [graph][runtime.channel] create channel=90 edge=90 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 13:29:17.813] [info] [graph][runtime.channel] create channel=91 edge=91 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-04 13:29:17.813] [info] [graph][runtime.channel] create channel=92 edge=92 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 13:29:17.813] [info] [graph][runtime.channel] create channel=93 edge=93 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 13:29:17.814] [info] [graph][runtime.channel] create channel=94 edge=94 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 13:29:17.814] [info] [graph][runtime.channel] create channel=95 edge=95 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 13:29:17.814] [info] [graph][runtime.channel] create channel=96 edge=96 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 13:29:17.814] [info] [graph][runtime.channel] create channel=97 edge=97 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 13:29:17.814] [info] [graph][runtime.channel] create channel=98 edge=98 stream=Audio edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.814] [info] [graph][runtime.channel] create channel=99 edge=99 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 13:29:17.814] [info] [graph][runtime.channel] create channel=100 edge=100 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.815] [info] [graph][runtime.channel] create channel=101 edge=101 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0
[2026-11-04 13:29:17.815] [info] [graph][runtime.channel] create channel=102 edge=102 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 13:29:17.815] [info] [graph][runtime.channel] create channel=103 edge=103 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 13:29:17.815] [info] [graph][runtime.channel] create channel=104 edge=104 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 13:29:17.815] [info] [graph][runtime.channel] create channel=105 edge=105 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0
[2026-11-04 13:29:17.815] [info] [graph][runtime.channel] create channel=106 edge=106 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.816] [info] [graph][runtime.channel] create channel=107 edge=107 stream=Video edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-04 13:29:17.816] [info] [graph][runtime.channel] create channel=108 edge=108 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-04 13:29:17.816] [info] [graph][runtime.channel] create channel=109 edge=109 stream=Any edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-04 13:29:17.816] [info] [graph][runtime.channel] create channel=110 edge=110 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.816] [info] [graph][runtime.channel] create channel=111 edge=111 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-04 13:29:17.816] [info] [graph][runtime.channel] create channel=112 edge=112 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 13:29:17.816] [info] [graph][runtime.channel] create channel=113 edge=113 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 13:29:17.817] [info] [graph][runtime.channel] create channel=114 edge=114 stream=Any edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-04 13:29:17.817] [info] [graph][runtime.channel] create channel=115 edge=115 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 13:29:17.817] [info] [graph][runtime.channel] create channel=116 edge=116 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-04 13:29:17.817] [info] [graph][runtime.channel] create channel=117 edge=117 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-04 13:29:17.817] [info] [graph][runtime.channel] create channel=118 edge=118 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-04 13:29:17.817] [info] [graph][runtime.channel] create channel=119 edge=119 stream=Any edge_kind=EncodedPacket payload=Unknown queued=0
[2026-11-04 13:29:17.818] [info] [graph][runtime.channel] create channel=120 edge=120 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-04 13:29:17.818] [info] [graph][runtime.channel] create channel=121 edge=121 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-04 13:29:17.818] [info] [graph][runtime.channel] create channel=122 edge=122 stream=Metadata edge_kind=Unknown payload=Unknown queued=0
[2026-11-04 13:29:17.818] [info] [graph][runtime.channel] create channel=123 edge=123 stream=Metadata edge_kind=Unknown payload=Unknown queued=0
[2026-11-04 13:29:17.819] [info] [graph][runtime.lifecycle] distribution_registration scope=capability_probe stage=enter node=none pid=34096 tid=8524 native=0
[2026-11-04 13:29:17.819] [info] [graph][runtime.lifecycle] distribution_registration scope=capability_probe stage=leave node=none pid=34096 tid=8524 native=0
[2026-11-04 13:29:17.819] [info] [graph][runtime.lifecycle] compiled nodes=81 edges=123 channels=123 execution_order=1->3->8->27->46->47->9->21->28->40->63->48->69->10->29->49->11->12->30->31->50->51->52->13->32->53->14->17->33->36->54->55->59->15->34->56->57->16->35->58->18->37->60->19->38->61->20->39->62->22->41->64->70->23->42->65->71->24->43->66->72->26->45->68->73->25->44->67->6->2->7->80->4->77->5->74->75->78->76->79->81
[2026-11-04 13:29:17.820] [info] [graph][runtime.lifecycle] compile.done state=DefaultRegistrationPending
[2026-11-04 13:29:17.820] [info] [graph][runtime.node] register node=1 name=composition-r384.output.video.codec_resolver kind=CodecResolver
[2026-11-04 13:29:17.820] [info] [graph][runtime.node] register node=2 name=composition-r384.output.video.encode kind=VideoEncode
[2026-11-04 13:29:17.820] [info] [graph][runtime.node] register node=3 name=composition-r384.output.audio.codec_resolver kind=AudioCodecResolver
[2026-11-04 13:29:17.821] [info] [graph][runtime.node] register node=4 name=composition-r384.output.audio.encode kind=AudioEncode
[2026-11-04 13:29:17.821] [info] [graph][runtime.node] register node=5 name=composition-r384.output.audio.canonicalizer kind=EncodedAudioCanonicalizer
[2026-11-04 13:29:17.821] [info] [graph][runtime.node] register node=8 name=composition-r384.source_0.input kind=RawRtpInput
[2026-11-04 13:29:17.821] [info] [graph][runtime.node] register node=9 name=composition-r384.source_0.rtp.clock_group kind=RtpClockGroup
[2026-11-04 13:29:17.821] [info] [graph][runtime.node] register node=10 name=composition-r384.source_0.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-11-04 13:29:17.821] [info] [graph][runtime.node] register node=11 name=composition-r384.source_0.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-11-04 13:29:17.821] [info] [graph][runtime.node] register node=12 name=composition-r384.source_0.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-11-04 13:29:17.821] [info] [graph][runtime.node] register node=13 name=composition-r384.source_0.av_sync.source_clock kind=SourceClockStateFanout
[2026-11-04 13:29:17.822] [info] [graph][runtime.node] register node=14 name=composition-r384.source_0.av_sync.video.generation_gate kind=LockedPacketGate
[2026-11-04 13:29:17.822] [info] [graph][runtime.node] register node=15 name=composition-r384.source_0.av_sync.video.canonical_input kind=CanonicalInput
[2026-11-04 13:29:17.822] [info] [graph][runtime.node] register node=16 name=composition-r384.source_0.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-11-04 13:29:17.822] [info] [graph][runtime.node] register node=17 name=composition-r384.source_0.av_sync.startup.clock kind=AvStartupClock
[2026-11-04 13:29:17.822] [info] [graph][runtime.node] register node=18 name=composition-r384.source_0.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-11-04 13:29:17.822] [info] [graph][runtime.node] register node=20 name=composition-r384.source_0.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-11-04 13:29:17.822] [info] [graph][runtime.node] register node=21 name=composition-r384.source_0.video.codec_resolver kind=CodecResolver
[2026-11-04 13:29:17.822] [info] [graph][runtime.node] register node=22 name=composition-r384.source_0.video.decode kind=VideoDecode
[2026-11-04 13:29:17.822] [info] [graph][runtime.node] register node=23 name=composition-r384.source_0.video.hwtransfer kind=HardwareTransfer
[2026-11-04 13:29:17.822] [info] [graph][runtime.node] register node=24 name=composition-r384.source_0.video.framerate kind=VideoFrameRate
[2026-11-04 13:29:17.822] [info] [graph][runtime.node] register node=25 name=composition-r384.source_0.video.preparation kind=VideoPreparation
[2026-11-04 13:29:17.822] [info] [graph][runtime.node] register node=26 name=composition-r384.source_0.video.filter kind=VideoFilter
[2026-11-04 13:29:17.822] [info] [graph][runtime.node] register node=27 name=composition-r384.source_1.input kind=RawRtpInput
[2026-11-04 13:29:17.822] [info] [graph][runtime.node] register node=28 name=composition-r384.source_1.rtp.clock_group kind=RtpClockGroup
[2026-11-04 13:29:17.823] [info] [graph][runtime.node] register node=29 name=composition-r384.source_1.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-11-04 13:29:17.823] [info] [graph][runtime.node] register node=30 name=composition-r384.source_1.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-11-04 13:29:17.823] [info] [graph][runtime.node] register node=31 name=composition-r384.source_1.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-11-04 13:29:17.823] [info] [graph][runtime.node] register node=32 name=composition-r384.source_1.av_sync.source_clock kind=SourceClockStateFanout
[2026-11-04 13:29:17.823] [info] [graph][runtime.node] register node=33 name=composition-r384.source_1.av_sync.video.generation_gate kind=LockedPacketGate
[2026-11-04 13:29:17.823] [info] [graph][runtime.node] register node=34 name=composition-r384.source_1.av_sync.video.canonical_input kind=CanonicalInput
[2026-11-04 13:29:17.823] [info] [graph][runtime.node] register node=35 name=composition-r384.source_1.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-11-04 13:29:17.823] [info] [graph][runtime.node] register node=36 name=composition-r384.source_1.av_sync.startup.clock kind=AvStartupClock
[2026-11-04 13:29:17.823] [info] [graph][runtime.node] register node=37 name=composition-r384.source_1.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-11-04 13:29:17.823] [info] [graph][runtime.node] register node=39 name=composition-r384.source_1.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-11-04 13:29:17.823] [info] [graph][runtime.node] register node=40 name=composition-r384.source_1.video.codec_resolver kind=CodecResolver
[2026-11-04 13:29:17.823] [info] [graph][runtime.node] register node=41 name=composition-r384.source_1.video.decode kind=VideoDecode
[2026-11-04 13:29:17.823] [info] [graph][runtime.node] register node=42 name=composition-r384.source_1.video.hwtransfer kind=HardwareTransfer
[2026-11-04 13:29:17.823] [info] [graph][runtime.node] register node=43 name=composition-r384.source_1.video.framerate kind=VideoFrameRate
[2026-11-04 13:29:17.823] [info] [graph][runtime.node] register node=44 name=composition-r384.source_1.video.preparation kind=VideoPreparation
[2026-11-04 13:29:17.823] [info] [graph][runtime.node] register node=45 name=composition-r384.source_1.video.filter kind=VideoFilter
[2026-11-04 13:29:17.823] [info] [graph][runtime.node] register node=46 name=composition-r384.source_2.video.input kind=RawRtpInput
[2026-11-04 13:29:17.823] [info] [graph][runtime.node] register node=47 name=composition-r384.source_2.audio.input kind=RawRtpInput
[2026-11-04 13:29:17.824] [info] [graph][runtime.node] register node=48 name=composition-r384.source_2.rtp.clock_group kind=RtpClockGroup
[2026-11-04 13:29:17.824] [info] [graph][runtime.node] register node=49 name=composition-r384.source_2.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-11-04 13:29:17.824] [info] [graph][runtime.node] register node=50 name=composition-r384.source_2.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-11-04 13:29:17.824] [info] [graph][runtime.node] register node=51 name=composition-r384.source_2.av_sync.audio.protocol_binder kind=RtpPacketClockBinder
[2026-11-04 13:29:17.824] [info] [graph][runtime.node] register node=52 name=composition-r384.source_2.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-11-04 13:29:17.824] [info] [graph][runtime.node] register node=53 name=composition-r384.source_2.av_sync.source_clock kind=SourceClockStateFanout
[2026-11-04 13:29:17.824] [info] [graph][runtime.node] register node=54 name=composition-r384.source_2.av_sync.video.generation_gate kind=LockedPacketGate
[2026-11-04 13:29:17.824] [info] [graph][runtime.node] register node=55 name=composition-r384.source_2.av_sync.audio.generation_gate kind=LockedPacketGate
[2026-11-04 13:29:17.824] [info] [graph][runtime.node] register node=56 name=composition-r384.source_2.av_sync.video.canonical_input kind=CanonicalInput
[2026-11-04 13:29:17.824] [info] [graph][runtime.node] register node=57 name=composition-r384.source_2.av_sync.audio.canonical_input kind=CanonicalInput
[2026-11-04 13:29:17.824] [info] [graph][runtime.node] register node=58 name=composition-r384.source_2.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-11-04 13:29:17.824] [info] [graph][runtime.node] register node=59 name=composition-r384.source_2.av_sync.startup.clock kind=AvStartupClock
[2026-11-04 13:29:17.824] [info] [graph][runtime.node] register node=60 name=composition-r384.source_2.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-11-04 13:29:17.824] [info] [graph][runtime.node] register node=62 name=composition-r384.source_2.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-11-04 13:29:17.825] [info] [graph][runtime.node] register node=63 name=composition-r384.source_2.video.codec_resolver kind=CodecResolver
[2026-11-04 13:29:17.825] [info] [graph][runtime.node] register node=64 name=composition-r384.source_2.video.decode kind=VideoDecode
[2026-11-04 13:29:17.825] [info] [graph][runtime.node] register node=65 name=composition-r384.source_2.video.hwtransfer kind=HardwareTransfer
[2026-11-04 13:29:17.825] [info] [graph][runtime.node] register node=66 name=composition-r384.source_2.video.framerate kind=VideoFrameRate
[2026-11-04 13:29:17.825] [info] [graph][runtime.node] register node=67 name=composition-r384.source_2.video.preparation kind=VideoPreparation
[2026-11-04 13:29:17.825] [info] [graph][runtime.node] register node=68 name=composition-r384.source_2.video.filter kind=VideoFilter
[2026-11-04 13:29:17.825] [info] [graph][runtime.node] register node=69 name=composition-r384.source_2.audio.encode.codec_resolver kind=AudioCodecResolver
[2026-11-04 13:29:17.825] [info] [graph][runtime.node] register node=70 name=composition-r384.source_2.audio.encode.decode kind=AudioDecode
[2026-11-04 13:29:17.825] [info] [graph][runtime.node] register node=71 name=composition-r384.source_2.audio.encode.startup_trim kind=Unknown
[2026-11-04 13:29:17.826] [info] [graph][runtime.node] register node=72 name=composition-r384.source_2.audio.encode.drift_controller kind=AudioDriftController
[2026-11-04 13:29:17.826] [info] [graph][runtime.node] register node=73 name=composition-r384.source_2.audio.encode.resample kind=AudioResample
[2026-11-04 13:29:17.826] [info] [graph][runtime.node] register node=74 name=composition-r384.output.scheduler.scheduler kind=AvOutputScheduler
[2026-11-04 13:29:17.826] [info] [graph][runtime.node] register node=75 name=composition-r384.output.scheduler.router kind=ScheduledOutputRouter
[2026-11-04 13:29:17.826] [info] [graph][runtime.node] register node=76 name=composition-r384.output.mpegts.mux kind=FileMux
[2026-11-04 13:29:17.826] [info] [graph][runtime.node] register node=77 name=composition-r384.output.mpegts.plan.source kind=Unknown
[2026-11-04 13:29:17.826] [info] [graph][runtime.node] register node=78 name=composition-r384.output.mpegts.scheduled.adapter kind=Unknown
[2026-11-04 13:29:17.826] [info] [graph][runtime.node] register node=79 name=composition-r384.output.mpegts.datagram.materializer kind=MpegTsDatagramMaterializer
[2026-11-04 13:29:17.827] [info] [graph][runtime.node] register node=80 name=composition-r384.output.mpegts.transport.plan kind=DatagramTransportPlanSource
[2026-11-04 13:29:17.827] [info] [graph][runtime.node] register node=81 name=composition-r384.output.mpegts.transport.sender kind=ScheduledDatagramSender
[2026-11-04 13:29:17.827] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=waiting_for_random_access detail="" description=""
[2026-11-04 13:29:17.828] [info] [graph][runtime.lifecycle] start.begin mode=threaded
[2026-11-04 13:29:17.832] [info] [graph][runtime.lifecycle] distribution_registration scope=worker stage=enter node=81 pid=34096 tid=33144 native=0
[2026-11-04 13:29:17.859] [info] [graph][runtime.lifecycle] start.done state=ThreadedRunning
[2026-11-04 13:29:17.859] [info] [graph][runtime.lifecycle] worker.finished node=63
[2026-11-04 13:29:17.860] [info] [graph][runtime.node] worker.thread_cpu node=63 available=1 cpu_ns=0 process_calls=1
[2026-11-04 13:29:17.859] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:29:17.861] [info] [graph][runtime.lifecycle] worker.finished node=69
[2026-11-04 13:29:17.861] [info] [graph][runtime.node] worker.thread_cpu node=69 available=1 cpu_ns=0 process_calls=2
[2026-11-04 13:29:17.860] [info] [graph][runtime.lifecycle] worker.finished node=21
[2026-11-04 13:29:17.862] [info] [graph][runtime.node] worker.thread_cpu node=21 available=1 cpu_ns=0 process_calls=2
[2026-11-04 13:29:17.860] [info] [graph][runtime.lifecycle] worker.finished node=40
[2026-11-04 13:29:17.860] [info] [graph][runtime.lifecycle] worker.finished node=3
[2026-11-04 13:29:17.860] [info] [graph][runtime.node] audio_encode_trace stage=codec_bound
[2026-11-04 13:29:17.859] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:29:17.860] [info] [graph][runtime.lifecycle] worker.finished node=1
[2026-11-04 13:29:17.864] [info] [graph][runtime.node] worker.thread_cpu node=1 available=1 cpu_ns=0 process_calls=1
[2026-11-04 13:29:17.862] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-11-04 13:29:17.859] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:29:17.863] [info] [graph][runtime.node] worker.thread_cpu node=40 available=1 cpu_ns=0 process_calls=2
[2026-11-04 13:29:17.863] [info] [graph][runtime.node] worker.thread_cpu node=3 available=1 cpu_ns=0 process_calls=1
[2026-11-04 13:29:17.860] [info] [graph][runtime.node] video_encode.bind_encoder codec=hevc_nvenc pix_fmt=cuda rate_control=cbr bitrate=8000000 min_bitrate=8000000 max_bitrate=8000000 buffer_size=267000 fps=30/1 gop=60 frame_kind=hardware hwaccel=cuda hw_device_ctx=set hw_frames_ctx=set
[2026-11-04 13:29:17.865] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-11-04 13:29:17.865] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=15 generation=1
[2026-11-04 13:29:17.866] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-11-04 13:29:17.867] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=15 generation=1
[2026-11-04 13:29:17.867] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=15 generation=1
[2026-11-04 13:29:17.868] [info] [graph][runtime.node] source_video_trace node_id=20 node=MediaAvBoundReleaseExtractorNode stage=release source_identity=composition-r384.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4053000000 duration_ns=33333333 decoded_sequence=unavailable pts=1027089733 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=17527606590700 group=composition-r384.source.0 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-4053000000 epoch_master_release_ns=5482591000 video_count=15 min_presentation_ns=-4053000000 max_end_ns=-3553000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:17.868] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=1027089733 dts=1027089733 size=317016 generation=1 sequence=15
[2026-11-04 13:29:17.868] [info] [graph][runtime.node] source_video_trace node_id=20 node=MediaAvBoundReleaseExtractorNode stage=published source_identity=composition-r384.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4053000000 duration_ns=33333333 decoded_sequence=unavailable pts=1027089733 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=17527606919700 group=composition-r384.source.0 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:17.868] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=input source_identity=composition-r384.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4053000000 duration_ns=33333333 decoded_sequence=unavailable pts=1027089733 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=17527607163000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:17.869] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=release source_identity=composition-r384.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4031000000 duration_ns=33333333 decoded_sequence=unavailable pts=1500770815 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=17527607265600 group=composition-r384.source.2 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=-4031000000 epoch_master_release_ns=5483175600 video_count=15 min_presentation_ns=-4031000000 max_end_ns=-3531000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:17.869] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=1500770815 dts=1500770815 size=317016 generation=1 sequence=15
[2026-11-04 13:29:17.869] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=published source_identity=composition-r384.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4031000000 duration_ns=33333333 decoded_sequence=unavailable pts=1500770815 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=17527607900000 group=composition-r384.source.2 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:17.869] [info] [graph][runtime.node] source_video_trace node_id=39 node=MediaAvBoundReleaseExtractorNode stage=release source_identity=composition-r384.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4021000000 duration_ns=33333333 decoded_sequence=unavailable pts=3924063461 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=17527607471300 group=composition-r384.source.1 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-4021000000 epoch_master_release_ns=5483522500 video_count=15 min_presentation_ns=-4021000000 max_end_ns=-3521000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:17.869] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=input source_identity=composition-r384.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4031000000 duration_ns=33333333 decoded_sequence=unavailable pts=1500770815 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=17527608169600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:17.870] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=3924063461 dts=3924063461 size=317016 generation=1 sequence=15
[2026-11-04 13:29:17.870] [info] [graph][runtime.node] source_video_trace node_id=39 node=MediaAvBoundReleaseExtractorNode stage=published source_identity=composition-r384.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4021000000 duration_ns=33333333 decoded_sequence=unavailable pts=3924063461 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=17527608781400 group=composition-r384.source.1 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:17.871] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=input source_identity=composition-r384.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4021000000 duration_ns=33333333 decoded_sequence=unavailable pts=3924063461 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=17527609334300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:17.898] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-11-04 13:29:17.898] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=accepted source_identity=composition-r384.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4053000000 duration_ns=33333333 decoded_sequence=unavailable pts=1027089733 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=17527637222500 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:17.899] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=1027089733 format=cuda size=1280x720 buffer_identity=0xb08600000 drm_prime=0 software_frame=0
[2026-11-04 13:29:17.899] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=resolved source_identity=composition-r384.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4053000000 duration_ns=33333333 decoded_sequence=unavailable pts=1027089733 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=17527637482000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:17.899] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=published source_identity=composition-r384.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4053000000 duration_ns=33333333 decoded_sequence=unavailable pts=1027089733 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=17527637583000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:17.899] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=000002033E06FCA0 stream=Video payload=Frame pts=1027089733 dts=1027089733 duration=3000 tb=1/90000 flags=68
[2026-11-04 13:29:17.899] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=000002033E06FCA0 stream=Video payload=Frame pts=1027089733 dts=1027089733 duration=3000 tb=1/90000 flags=68
[2026-11-04 13:29:17.899] [info] [graph][runtime.node] source_video_trace node_id=23 node=HardwareTransferNode stage=published source_identity=composition-r384.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4053000000 duration_ns=33333333 decoded_sequence=unavailable pts=1027089733 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=17527637838200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:17.899] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-11-04 13:29:17.899] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=000002033E06FCA0 stream=Video payload=Frame pts=1027089733 dts=1027089733 duration=3000 tb=1/90000 flags=68
[2026-11-04 13:29:17.899] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=000002033E070C60 stream=Video payload=Frame pts=1027089733 dts=1027089733 duration=3000 tb=1/90000 flags=68
[2026-11-04 13:29:17.899] [info] [graph][runtime.node] source_video_trace node_id=24 node=VideoFrameRateNode stage=published source_identity=composition-r384.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4053000000 duration_ns=33333333 decoded_sequence=unavailable pts=1027089733 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=17527638161500 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:17.900] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-11-04 13:29:17.900] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=000002033E070C60 stream=Video payload=Frame pts=1027089733 dts=1027089733 duration=3000 tb=1/90000 flags=68
[2026-11-04 13:29:17.900] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-11-04 13:29:17.900] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=accepted source_identity=composition-r384.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4021000000 duration_ns=33333333 decoded_sequence=unavailable pts=3924063461 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=17527639131100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:17.900] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=3924063461 format=cuda size=1280x720 buffer_identity=0xb08800000 drm_prime=0 software_frame=0
[2026-11-04 13:29:17.901] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=resolved source_identity=composition-r384.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4021000000 duration_ns=33333333 decoded_sequence=unavailable pts=3924063461 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=17527639327400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:17.901] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=published source_identity=composition-r384.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4021000000 duration_ns=33333333 decoded_sequence=unavailable pts=3924063461 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=17527639421100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:17.901] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=000002033E06DD20 stream=Video payload=Frame pts=3924063461 dts=3924063461 duration=3000 tb=1/90000 flags=68
[2026-11-04 13:29:17.901] [info] [graph][runtime.node] source_video_trace node_id=42 node=HardwareTransferNode stage=published source_identity=composition-r384.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4021000000 duration_ns=33333333 decoded_sequence=unavailable pts=3924063461 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=17527639648000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:17.901] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=000002033E06DD20 stream=Video payload=Frame pts=3924063461 dts=3924063461 duration=3000 tb=1/90000 flags=68
[2026-11-04 13:29:17.901] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=000002033E06DD20 stream=Video payload=Frame pts=3924063461 dts=3924063461 duration=3000 tb=1/90000 flags=68
[2026-11-04 13:29:17.901] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-11-04 13:29:17.901] [info] [graph][runtime.node] source_video_trace node_id=43 node=VideoFrameRateNode stage=published source_identity=composition-r384.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4021000000 duration_ns=33333333 decoded_sequence=unavailable pts=3924063461 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=17527640044000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:17.901] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=000002033E06F0D0 stream=Video payload=Frame pts=3924063461 dts=3924063461 duration=3000 tb=1/90000 flags=68
[2026-11-04 13:29:17.901] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=000002033E06F0D0 stream=Video payload=Frame pts=3924063461 dts=3924063461 duration=3000 tb=1/90000 flags=68
[2026-11-04 13:29:17.901] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-11-04 13:29:17.903] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-11-04 13:29:17.903] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=accepted source_identity=composition-r384.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4031000000 duration_ns=33333333 decoded_sequence=unavailable pts=1500770815 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=17527641999400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:17.903] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=1500770815 format=cuda size=1280x720 buffer_identity=0xb08c00000 drm_prime=0 software_frame=0
[2026-11-04 13:29:17.904] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=resolved source_identity=composition-r384.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4031000000 duration_ns=33333333 decoded_sequence=unavailable pts=1500770815 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=17527642392600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:17.904] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=published source_identity=composition-r384.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4031000000 duration_ns=33333333 decoded_sequence=unavailable pts=1500770815 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=17527642529100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:17.904] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=000002033E06DD20 stream=Video payload=Frame pts=1500770815 dts=1500770815 duration=3000 tb=1/90000 flags=68
[2026-11-04 13:29:17.904] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=000002033E06DD20 stream=Video payload=Frame pts=1500770815 dts=1500770815 duration=3000 tb=1/90000 flags=68
[2026-11-04 13:29:17.904] [info] [graph][runtime.node] source_video_trace node_id=65 node=HardwareTransferNode stage=published source_identity=composition-r384.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4031000000 duration_ns=33333333 decoded_sequence=unavailable pts=1500770815 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=17527642867600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:17.904] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=000002033E06DD20 stream=Video payload=Frame pts=1500770815 dts=1500770815 duration=3000 tb=1/90000 flags=68
[2026-11-04 13:29:17.904] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-11-04 13:29:17.905] [info] [graph][runtime.node] source_video_trace node_id=66 node=VideoFrameRateNode stage=published source_identity=composition-r384.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4031000000 duration_ns=33333333 decoded_sequence=unavailable pts=1500770815 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=17527643428600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:17.905] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=000002033E06F4C0 stream=Video payload=Frame pts=1500770815 dts=1500770815 duration=3000 tb=1/90000 flags=68
[2026-11-04 13:29:17.905] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=000002033E06F4C0 stream=Video payload=Frame pts=1500770815 dts=1500770815 duration=3000 tb=1/90000 flags=68
[2026-11-04 13:29:17.905] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-11-04 13:29:17.944] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=43
[2026-11-04 13:29:17.999] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=input source_identity=composition-r384.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4053000000 duration_ns=33333333 decoded_sequence=unavailable pts=1027089733 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=17527737365900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:17.999] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=97
[2026-11-04 13:29:17.999] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=94
[2026-11-04 13:29:18.000] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=input source_identity=composition-r384.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4021000000 duration_ns=33333333 decoded_sequence=unavailable pts=3924063461 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=17527738233800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:18.000] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=input source_identity=composition-r384.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4031000000 duration_ns=33333333 decoded_sequence=unavailable pts=1500770815 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=17527738716700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:18.000] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=accepted source_identity=composition-r384.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4053000000 duration_ns=33333333 decoded_sequence=unavailable pts=1027089733 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=17527739038700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:18.001] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=accepted source_identity=composition-r384.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4021000000 duration_ns=33333333 decoded_sequence=unavailable pts=3924063461 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=17527740144800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:18.002] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=resolved source_identity=composition-r384.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4053000000 duration_ns=33333333 decoded_sequence=unavailable pts=342363 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=17527740317200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:18.002] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=resolved source_identity=composition-r384.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4021000000 duration_ns=33333333 decoded_sequence=unavailable pts=1308021 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=17527740978800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:18.002] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=published source_identity=composition-r384.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4053000000 duration_ns=33333333 decoded_sequence=unavailable pts=342363 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=17527740987600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:18.003] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb08a00000 drm_prime=0 software_frame=0
[2026-11-04 13:29:18.003] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=accepted source_identity=composition-r384.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4031000000 duration_ns=33333333 decoded_sequence=unavailable pts=1500770815 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=17527741271300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:18.003] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=published source_identity=composition-r384.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4021000000 duration_ns=33333333 decoded_sequence=unavailable pts=1308021 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=17527741371000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:18.003] [info] [graph][runtime.node] source_video_trace node_id=44 node=VideoPreparationNode stage=input source_identity=composition-r384.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4021000000 duration_ns=33333333 decoded_sequence=unavailable pts=1308021 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=17527741608000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:18.004] [info] [graph][runtime.node] source_video_trace node_id=44 node=VideoPreparationNode stage=output_attempt source_identity=composition-r384.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4021000000 duration_ns=33333333 decoded_sequence=unavailable pts=1308021 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=17527742773300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:18.003] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=resolved source_identity=composition-r384.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4031000000 duration_ns=33333333 decoded_sequence=unavailable pts=500257 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=17527742052600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:18.004] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb08a90000 drm_prime=0 software_frame=0
[2026-11-04 13:29:18.002] [info] [graph][runtime.node] source_video_trace node_id=25 node=VideoPreparationNode stage=input source_identity=composition-r384.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4053000000 duration_ns=33333333 decoded_sequence=unavailable pts=342363 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=17527741008700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:18.004] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=15 audio_count=0 first_video_sequence=15
[2026-11-04 13:29:18.005] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=published source_identity=composition-r384.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4031000000 duration_ns=33333333 decoded_sequence=unavailable pts=500257 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=17527743339000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:18.005] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=input source_identity=composition-r384.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4031000000 duration_ns=33333333 decoded_sequence=unavailable pts=500257 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=17527743355900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:18.005] [info] [graph][runtime.node] source_video_trace node_id=25 node=VideoPreparationNode stage=output_attempt source_identity=composition-r384.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4053000000 duration_ns=33333333 decoded_sequence=unavailable pts=342363 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=17527743858100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:18.005] [info] [graph][runtime.node] source_video_trace node_id=44 node=VideoPreparationNode stage=published source_identity=unavailable identity_link=node_generation_input_source_sequence generation=1 source_sequence=15 presentation_ns=-4021000000 duration_ns=33333333 decoded_sequence=unavailable pts=1308021 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=17527744169900 group=composition-r384.source.1 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:18.006] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb08b20000 drm_prime=0 software_frame=0
[2026-11-04 13:29:18.006] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=output_attempt source_identity=composition-r384.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4031000000 duration_ns=33333333 decoded_sequence=unavailable pts=500257 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=17527744464100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:18.006] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=15 audio_count=0 first_video_sequence=15
[2026-11-04 13:29:18.006] [info] [graph][runtime.node] source_video_trace node_id=38 node=ActivationReleaseSequencer stage=activation source_identity=composition-r384.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4021000000 duration_ns=33333333 decoded_sequence=unavailable pts=3924063461 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=17527745012300 group=composition-r384.source.1 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-4021000000 epoch_master_release_ns=5686985668 video_count=15 min_presentation_ns=-4021000000 max_end_ns=-3521000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:18.007] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=15 audio_count=55 first_video_sequence=15
[2026-11-04 13:29:18.007] [info] [graph][runtime.node] source_video_trace node_id=19 node=ActivationReleaseSequencer stage=activation source_identity=composition-r384.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4053000000 duration_ns=33333333 decoded_sequence=unavailable pts=1027089733 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=17527745737500 group=composition-r384.source.0 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-4053000000 epoch_master_release_ns=5688542268 video_count=15 min_presentation_ns=-4053000000 max_end_ns=-3553000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:18.007] [info] [graph][runtime.node] source_video_trace node_id=25 node=VideoPreparationNode stage=published source_identity=unavailable identity_link=node_generation_input_source_sequence generation=1 source_sequence=15 presentation_ns=-4053000000 duration_ns=33333333 decoded_sequence=unavailable pts=342363 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=17527745728600 group=composition-r384.source.0 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:18.007] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_candidate source_identity=composition-r384.source.1.input.video generation=1 steady_ns=17527745806200 source_sequence=15 presentation_ns=-4021000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-04 13:29:18.007] [info] [graph][runtime.node] source_video_trace node_id=38 node=ActivationReleaseSequencer stage=release_published source_identity=composition-r384.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4021000000 duration_ns=33333333 decoded_sequence=unavailable pts=3924063461 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=17527745802800 group=composition-r384.source.1 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-4021000000 epoch_master_release_ns=5686985668 video_count=15 min_presentation_ns=-4021000000 max_end_ns=-3521000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:18.007] [info] [graph][runtime.node] source_video_trace node_id=39 node=MediaAvBoundReleaseExtractorNode stage=release_committed source_identity=composition-r384.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4021000000 duration_ns=33333333 decoded_sequence=unavailable pts=3924063461 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=17527745815400 group=composition-r384.source.1 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-4021000000 epoch_master_release_ns=5483522500 video_count=15 min_presentation_ns=-4021000000 max_end_ns=-3521000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=17 release_committed_count=1 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:18.007] [info] [graph][runtime.node] source_video_trace node_id=19 node=ActivationReleaseSequencer stage=release_published source_identity=composition-r384.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4053000000 duration_ns=33333333 decoded_sequence=unavailable pts=1027089733 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=17527746086500 group=composition-r384.source.0 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-4053000000 epoch_master_release_ns=5688542268 video_count=15 min_presentation_ns=-4053000000 max_end_ns=-3553000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:18.007] [info] [graph][runtime.node] source_video_trace node_id=61 node=ActivationReleaseSequencer stage=activation source_identity=composition-r384.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4031000000 duration_ns=33333333 decoded_sequence=unavailable pts=1500770815 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=17527746188400 group=composition-r384.source.2 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=-4031000000 epoch_master_release_ns=5688903368 video_count=15 min_presentation_ns=-4031000000 max_end_ns=-3531000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:18.007] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=published source_identity=unavailable identity_link=node_generation_input_source_sequence generation=1 source_sequence=15 presentation_ns=-4031000000 duration_ns=33333333 decoded_sequence=unavailable pts=500257 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=17527746189700 group=composition-r384.source.2 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:18.008] [info] [graph][runtime.node] source_video_trace node_id=20 node=MediaAvBoundReleaseExtractorNode stage=release_committed source_identity=composition-r384.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4053000000 duration_ns=33333333 decoded_sequence=unavailable pts=1027089733 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=17527746240600 group=composition-r384.source.0 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-4053000000 epoch_master_release_ns=5482591000 video_count=15 min_presentation_ns=-4053000000 max_end_ns=-3553000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=17 release_committed_count=1 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:18.008] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=0
[2026-11-04 13:29:18.008] [info] [graph][runtime.node] source_video_trace node_id=61 node=ActivationReleaseSequencer stage=release_published source_identity=composition-r384.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4031000000 duration_ns=33333333 decoded_sequence=unavailable pts=1500770815 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=17527747076600 group=composition-r384.source.2 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=-4031000000 epoch_master_release_ns=5688903368 video_count=15 min_presentation_ns=-4031000000 max_end_ns=-3531000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:18.008] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_candidate source_identity=composition-r384.source.2.input.video generation=1 steady_ns=17527747209900 source_sequence=15 presentation_ns=-4031000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-04 13:29:18.008] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=release_committed source_identity=composition-r384.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4031000000 duration_ns=33333333 decoded_sequence=unavailable pts=1500770815 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=17527747215300 group=composition-r384.source.2 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=-4031000000 epoch_master_release_ns=5483175600 video_count=15 min_presentation_ns=-4031000000 max_end_ns=-3531000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=17 release_committed_count=1 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:29:18.009] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=55
[2026-11-04 13:29:18.009] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=0
[2026-11-04 13:29:18.009] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_candidate source_identity=composition-r384.source.0.input.video generation=1 steady_ns=17527747735200 source_sequence=15 presentation_ns=-4053000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-04 13:29:18.008] [info] [graph][runtime.node] audio_decode_trace stage=first_packet
[2026-11-04 13:29:18.010] [info] [graph][runtime.node] audio_decode_trace stage=first_frame samples=1024 rate=44100
[2026-11-04 13:29:18.011] [info] [graph][runtime.node] audio_encode_trace stage=first_frame
[2026-11-04 13:29:18.011] [info] [graph][runtime.node] output_preparation stage=transport_activate generation=1 elapsed_ns=362300 planned_bytes=180767114966 planned_items=1 event=scope_exit
[2026-11-04 13:29:18.011] [info] [graph][runtime.node] output_preparation stage=global_sequence generation=1 elapsed_ns=44100 planned_bytes=68968 planned_items=2154 event=scope_exit
[2026-11-04 13:29:18.012] [info] [graph][runtime.node] output_preparation stage=transport_buffer generation=1 elapsed_ns=744700 planned_bytes=180767114966 planned_items=1 event=scope_exit
[2026-11-04 13:29:18.012] [info] [graph][runtime.node] output_preparation stage=ts_packetizer generation=1 elapsed_ns=98800 planned_bytes=180765122912 planned_items=61257 event=scope_exit
[2026-11-04 13:29:18.012] [info] [graph][runtime.node] output_preparation stage=ts_mux generation=1 elapsed_ns=282900 planned_bytes=945416 planned_items=59086 event=scope_exit
[2026-11-04 13:29:18.012] [info] [graph][runtime.node] mpegts_emission_plan generation=1 mode=canonical_access_unit_window access_unit_window_ns=100000000 video_initial_service_window_ns=13990650 audio_initial_service_window_ns=13990650 packet_size_bytes=188 maximum_payload_bytes=1316 per_datagram_overhead_bytes=0
[2026-11-04 13:29:18.014] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-r384.source.0.input.video generation=1 steady_ns=17527753099600 source_sequence=15 presentation_ns=-4053000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-04 13:29:18.014] [info] [graph][runtime.node] video_encode.input_contract format=cuda size=1280x720 buffer_identity=0xb0a000000 drm_prime=0 software_frame=0
[2026-11-04 13:29:18.015] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-r384.source.1.input.video generation=1 steady_ns=17527753261500 source_sequence=15 presentation_ns=-4021000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-04 13:29:18.015] [info] [graph][runtime.node] video_encode.generation_start_key_frame pts=0 generation=1
[2026-11-04 13:29:18.015] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-r384.source.2.input.video generation=1 steady_ns=17527753510300 source_sequence=15 presentation_ns=-4031000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-04 13:29:18.015] [info] [graph][runtime.node] video_encode.first_frame pts=0 generation=attached
[2026-11-04 13:29:18.020] [info] [graph][runtime.node] video_encode.first_submit result=accepted
[2026-11-04 13:29:18.021] [info] [graph][runtime.node] video_encode.first_packet pts=0 dts=0 duration=0 size=108357 side_data_elems=1 received_footprint=108365 side_data[0]=Quality stats:8
[2026-11-04 13:29:18.021] [info] [graph][runtime.node] video_encode.first_packet_footprint bytes=108365 wrapped_size=108357 wrapped_side_data_elems=1
[2026-11-04 13:29:18.021] [info] [graph][runtime.node] av_scheduler_trace stage=first_video_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=5692205868 dispatch_master_ns=5692205868 now_ns=5535880400 node=74 identity_kind=output identity="composition-r384.output" canonical_sequence=1 canonical_created_steady_ns=17527759638800 head_observed_steady_ns=17527759675200 codec_receive_begin_steady_ns=17527759368600 codec_receive_end_steady_ns=17527759369500
[2026-11-04 13:29:18.021] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=5536050100 present_generation=1 present_canonical_target_ns=0 present_canonical_dispatch_ns=0 present_target_master_ns=5692205868 present_dispatch_master_ns=5692205868
[2026-11-04 13:29:18.059] [info] [graph][runtime.node] audio_encode_trace stage=first_packet
[2026-11-04 13:29:18.059] [info] [graph][runtime.node] av_scheduler_trace stage=first_audio_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=5692205868 dispatch_master_ns=5692205868 now_ns=5574404100 node=74 identity_kind=output identity="composition-r384.output" canonical_sequence=1 canonical_created_steady_ns=17527798167200 head_observed_steady_ns=17527798198900 codec_receive_begin_steady_ns=unavailable codec_receive_end_steady_ns=unavailable
[2026-11-04 13:29:18.235] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=running detail="" description=""
[2026-11-04 13:29:22.493] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2557172439 ntp_seconds=4002758962 ntp_fraction=2117418876 arrival_ns=17532231935300
[2026-11-04 13:29:22.494] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2557172439
[2026-11-04 13:29:22.494] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:29:22.509] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=4048231488 ntp_seconds=4002758962 ntp_fraction=2181843386 arrival_ns=17532247338800
[2026-11-04 13:29:22.509] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4048231488
[2026-11-04 13:29:22.509] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=4073710244 ntp_seconds=4002758962 ntp_fraction=2186138353 arrival_ns=17532247338800
[2026-11-04 13:29:22.509] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:29:22.510] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4073710244
[2026-11-04 13:29:22.510] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:29:22.537] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=4211499259 ntp_seconds=4002758962 ntp_fraction=2306397437 arrival_ns=17532275893700
[2026-11-04 13:29:22.538] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4211499259
[2026-11-04 13:29:22.538] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:29:27.497] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2557172439 ntp_seconds=4002758967 ntp_fraction=2134598746 arrival_ns=17537235638000
[2026-11-04 13:29:27.497] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2557172439
[2026-11-04 13:29:27.497] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:29:27.511] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=4048231488 ntp_seconds=4002758967 ntp_fraction=2190433320 arrival_ns=17537249116400
[2026-11-04 13:29:27.511] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4048231488
[2026-11-04 13:29:27.511] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:29:27.540] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=4073710244 ntp_seconds=4002758967 ntp_fraction=2314987372 arrival_ns=17537278005100
[2026-11-04 13:29:27.540] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4073710244
[2026-11-04 13:29:27.540] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:29:27.541] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=4211499259 ntp_seconds=4002758967 ntp_fraction=2323577307 arrival_ns=17537279587000
[2026-11-04 13:29:27.542] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4211499259
[2026-11-04 13:29:27.542] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:29:32.522] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2557172439 ntp_seconds=4002758972 ntp_fraction=2241972928 arrival_ns=17542260791600
[2026-11-04 13:29:32.522] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2557172439
[2026-11-04 13:29:32.523] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:29:32.537] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=4048231488 ntp_seconds=4002758972 ntp_fraction=2302102470 arrival_ns=17542274923500
[2026-11-04 13:29:32.537] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4048231488
[2026-11-04 13:29:32.537] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:29:32.565] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=4073710244 ntp_seconds=4002758972 ntp_fraction=2422361554 arrival_ns=17542303201700
[2026-11-04 13:29:32.565] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4073710244
[2026-11-04 13:29:32.565] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:29:32.579] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=4211499259 ntp_seconds=4002758972 ntp_fraction=2486786064 arrival_ns=17542317443800
[2026-11-04 13:29:32.580] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4211499259
[2026-11-04 13:29:32.580] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:29:37.539] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2557172439 ntp_seconds=4002758977 ntp_fraction=2314987372 arrival_ns=17547277478100
[2026-11-04 13:29:37.539] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2557172439
[2026-11-04 13:29:37.540] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:29:37.539] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=4048231488 ntp_seconds=4002758977 ntp_fraction=2314987372 arrival_ns=17547277482600
[2026-11-04 13:29:37.540] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4048231488
[2026-11-04 13:29:37.540] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:29:37.569] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=4073710244 ntp_seconds=4002758977 ntp_fraction=2443836391 arrival_ns=17547307619900
[2026-11-04 13:29:37.570] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4073710244
[2026-11-04 13:29:37.570] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:29:37.616] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=4211499259 ntp_seconds=4002758977 ntp_fraction=2641404887 arrival_ns=17547354082900
[2026-11-04 13:29:37.616] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4211499259
[2026-11-04 13:29:37.616] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:29:42.552] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2557172439 ntp_seconds=4002758982 ntp_fraction=2366526980 arrival_ns=17552290212100
[2026-11-04 13:29:42.552] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2557172439
[2026-11-04 13:29:42.552] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:29:42.581] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=4048231488 ntp_seconds=4002758982 ntp_fraction=2495375998 arrival_ns=17552319943400
[2026-11-04 13:29:42.582] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4048231488
[2026-11-04 13:29:42.582] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:29:42.597] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=4073710244 ntp_seconds=4002758982 ntp_fraction=2559800508 arrival_ns=17552335195100
[2026-11-04 13:29:42.597] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4073710244
[2026-11-04 13:29:42.597] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:29:42.647] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=4211499259 ntp_seconds=4002758982 ntp_fraction=2774548873 arrival_ns=17552384822500
[2026-11-04 13:29:42.647] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4211499259
[2026-11-04 13:29:42.647] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:29:47.568] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2557172439 ntp_seconds=4002758987 ntp_fraction=2439541424 arrival_ns=17557306765100
[2026-11-04 13:29:47.569] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2557172439
[2026-11-04 13:29:47.569] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:29:47.583] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=4048231488 ntp_seconds=4002758987 ntp_fraction=2499670966 arrival_ns=17557321020300
[2026-11-04 13:29:47.583] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4048231488
[2026-11-04 13:29:47.583] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:29:47.656] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=4211499259 ntp_seconds=4002758987 ntp_fraction=2817498546 arrival_ns=17557394340700
[2026-11-04 13:29:47.656] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4211499259
[2026-11-04 13:29:47.656] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:29:47.657] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=4073710244 ntp_seconds=4002758987 ntp_fraction=2817498546 arrival_ns=17557395201500
[2026-11-04 13:29:47.657] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4073710244
[2026-11-04 13:29:47.657] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:29:52.573] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2557172439 ntp_seconds=4002758992 ntp_fraction=2461016260 arrival_ns=17562311430900
[2026-11-04 13:29:52.573] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2557172439
[2026-11-04 13:29:52.574] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:29:52.603] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=4048231488 ntp_seconds=4002758992 ntp_fraction=2589865279 arrival_ns=17562341609300
[2026-11-04 13:29:52.604] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4048231488
[2026-11-04 13:29:52.605] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:29:52.673] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=4073710244 ntp_seconds=4002758992 ntp_fraction=2890512990 arrival_ns=17562411708400
[2026-11-04 13:29:52.673] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=4211499259 ntp_seconds=4002758992 ntp_fraction=2890512990 arrival_ns=17562411625300
[2026-11-04 13:29:52.674] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4073710244
[2026-11-04 13:29:52.674] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:29:52.674] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4211499259
[2026-11-04 13:29:52.676] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:29:57.581] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2557172439 ntp_seconds=4002758997 ntp_fraction=2495375998 arrival_ns=17567319359600
[2026-11-04 13:29:57.581] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2557172439
[2026-11-04 13:29:57.582] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:29:57.625] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=4048231488 ntp_seconds=4002758997 ntp_fraction=2680059592 arrival_ns=17567362659200
[2026-11-04 13:29:57.626] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4048231488
[2026-11-04 13:29:57.626] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1

[2026-11-04 13:29:57.676] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=4073710244 ntp_seconds=4002758997 ntp_fraction=2903397892 arrival_ns=17567414406200
[2026-11-04 13:29:57.677] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4073710244
[2026-11-04 13:29:57.677] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:29:57.685] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=4211499259 ntp_seconds=4002758997 ntp_fraction=2937757630 arrival_ns=17567423087900
[2026-11-04 13:29:57.685] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4211499259
[2026-11-04 13:29:57.685] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:02.587] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2557172439 ntp_seconds=4002759002 ntp_fraction=2521145802 arrival_ns=17572325910800
[2026-11-04 13:30:02.588] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2557172439
[2026-11-04 13:30:02.588] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:02.647] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=4048231488 ntp_seconds=4002759002 ntp_fraction=2778843840 arrival_ns=17572385424900
[2026-11-04 13:30:02.647] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4048231488
[2026-11-04 13:30:02.648] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:02.707] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=4211499259 ntp_seconds=4002759002 ntp_fraction=3036541878 arrival_ns=17572445765100
[2026-11-04 13:30:02.708] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4211499259
[2026-11-04 13:30:02.708] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:02.708] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=4073710244 ntp_seconds=4002759002 ntp_fraction=3040836845 arrival_ns=17572446578400
[2026-11-04 13:30:02.709] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4073710244
[2026-11-04 13:30:02.709] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:07.603] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2557172439 ntp_seconds=4002759007 ntp_fraction=2589865279 arrival_ns=17577341508600
[2026-11-04 13:30:07.604] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2557172439
[2026-11-04 13:30:07.604] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:07.673] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=4048231488 ntp_seconds=4002759007 ntp_fraction=2886218022 arrival_ns=17577411180200
[2026-11-04 13:30:07.674] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4048231488
[2026-11-04 13:30:07.674] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:07.717] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=4073710244 ntp_seconds=4002759007 ntp_fraction=3075196583 arrival_ns=17577454783800
[2026-11-04 13:30:07.717] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4073710244
[2026-11-04 13:30:07.717] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=4211499259 ntp_seconds=4002759007 ntp_fraction=3079491551 arrival_ns=17577455357400
[2026-11-04 13:30:07.718] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4211499259
[2026-11-04 13:30:07.718] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:07.718] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:12.640] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2557172439 ntp_seconds=4002759012 ntp_fraction=2748779069 arrival_ns=17582378863100
[2026-11-04 13:30:12.641] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2557172439
[2026-11-04 13:30:12.642] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:12.680] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=4048231488 ntp_seconds=4002759012 ntp_fraction=2920577761 arrival_ns=17582419015600
[2026-11-04 13:30:12.681] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4048231488
[2026-11-04 13:30:12.681] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:12.743] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=4211499259 ntp_seconds=4002759012 ntp_fraction=3186865733 arrival_ns=17582480452600
[2026-11-04 13:30:12.743] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4211499259
[2026-11-04 13:30:12.743] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:12.748] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=4073710244 ntp_seconds=4002759012 ntp_fraction=3204045602 arrival_ns=17582484673400
[2026-11-04 13:30:12.748] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4073710244
[2026-11-04 13:30:12.748] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:17.661] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2557172439 ntp_seconds=4002759017 ntp_fraction=2838973382 arrival_ns=17587399939700
[2026-11-04 13:30:17.662] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2557172439
[2026-11-04 13:30:17.662] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:17.707] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=4048231488 ntp_seconds=4002759017 ntp_fraction=3032246910 arrival_ns=17587444966800
[2026-11-04 13:30:17.707] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4048231488
[2026-11-04 13:30:17.707] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:17.765] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=4073710244 ntp_seconds=4002759017 ntp_fraction=3285649981 arrival_ns=17587503469900
[2026-11-04 13:30:17.765] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4073710244
[2026-11-04 13:30:17.765] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:17.780] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=4211499259 ntp_seconds=4002759017 ntp_fraction=3350074490 arrival_ns=17587519009500
[2026-11-04 13:30:17.781] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4211499259
[2026-11-04 13:30:17.781] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:22.680] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2557172439 ntp_seconds=4002759022 ntp_fraction=2920577761 arrival_ns=17592418408000
[2026-11-04 13:30:22.680] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2557172439
[2026-11-04 13:30:22.680] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:22.737] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=4048231488 ntp_seconds=4002759022 ntp_fraction=3165390897 arrival_ns=17592476053000
[2026-11-04 13:30:22.738] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4048231488
[2026-11-04 13:30:22.738] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:22.777] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=4073710244 ntp_seconds=4002759022 ntp_fraction=3337189588 arrival_ns=17592515586200
[2026-11-04 13:30:22.778] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4073710244
[2026-11-04 13:30:22.778] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:22.808] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=4211499259 ntp_seconds=4002759022 ntp_fraction=3470333575 arrival_ns=17592546925100
[2026-11-04 13:30:22.809] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4211499259
[2026-11-04 13:30:22.809] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:27.690] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2557172439 ntp_seconds=4002759027 ntp_fraction=2963527434 arrival_ns=17597428429900
[2026-11-04 13:30:27.690] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2557172439
[2026-11-04 13:30:27.690] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:27.750] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=4048231488 ntp_seconds=4002759027 ntp_fraction=3216930504 arrival_ns=17597488195700
[2026-11-04 13:30:27.750] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4048231488
[2026-11-04 13:30:27.750] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:27.803] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=4073710244 ntp_seconds=4002759027 ntp_fraction=3444563771 arrival_ns=17597541217400
[2026-11-04 13:30:27.804] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4073710244
[2026-11-04 13:30:27.804] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:27.846] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=4211499259 ntp_seconds=4002759027 ntp_fraction=3629247365 arrival_ns=17597584093400
[2026-11-04 13:30:27.846] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4211499259
[2026-11-04 13:30:27.846] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:32.701] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2557172439 ntp_seconds=4002759032 ntp_fraction=3010772074 arrival_ns=17602439867900
[2026-11-04 13:30:32.702] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2557172439
[2026-11-04 13:30:32.702] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:32.773] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=4048231488 ntp_seconds=4002759032 ntp_fraction=3320009719 arrival_ns=17602512129500
[2026-11-04 13:30:32.774] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4048231488
[2026-11-04 13:30:32.774] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:32.829] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=4073710244 ntp_seconds=4002759032 ntp_fraction=3560527888 arrival_ns=17602568083100
[2026-11-04 13:30:32.830] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4073710244
[2026-11-04 13:30:32.830] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:32.888] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=4211499259 ntp_seconds=4002759032 ntp_fraction=3813930958 arrival_ns=17602626732200
[2026-11-04 13:30:32.889] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4211499259
[2026-11-04 13:30:32.889] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:37.727] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2557172439 ntp_seconds=4002759037 ntp_fraction=3122441224 arrival_ns=17607465790400
[2026-11-04 13:30:37.728] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2557172439
[2026-11-04 13:30:37.728] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:37.782] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=4048231488 ntp_seconds=4002759037 ntp_fraction=3354369458 arrival_ns=17607520218600
[2026-11-04 13:30:37.782] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4048231488
[2026-11-04 13:30:37.782] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:37.836] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=4073710244 ntp_seconds=4002759037 ntp_fraction=3590592659 arrival_ns=17607574585600
[2026-11-04 13:30:37.836] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4073710244
[2026-11-04 13:30:37.837] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:37.910] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=4211499259 ntp_seconds=4002759037 ntp_fraction=3904125272 arrival_ns=17607648196400
[2026-11-04 13:30:37.910] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4211499259
[2026-11-04 13:30:37.910] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:42.738] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2557172439 ntp_seconds=4002759042 ntp_fraction=3169685864 arrival_ns=17612476694600
[2026-11-04 13:30:42.738] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2557172439
[2026-11-04 13:30:42.738] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:42.816] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=4048231488 ntp_seconds=4002759042 ntp_fraction=3504693313 arrival_ns=17612554411400
[2026-11-04 13:30:42.816] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4048231488
[2026-11-04 13:30:42.816] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:42.876] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=4073710244 ntp_seconds=4002759042 ntp_fraction=3758096384 arrival_ns=17612614074700
[2026-11-04 13:30:42.876] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4073710244
[2026-11-04 13:30:42.876] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:42.920] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=4211499259 ntp_seconds=4002759042 ntp_fraction=3951369912 arrival_ns=17612658438200
[2026-11-04 13:30:42.920] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4211499259
[2026-11-04 13:30:42.920] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:47.753] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2557172439 ntp_seconds=4002759047 ntp_fraction=3234110373 arrival_ns=17617491626100
[2026-11-04 13:30:47.753] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2557172439
[2026-11-04 13:30:47.753] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:47.841] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=4048231488 ntp_seconds=4002759047 ntp_fraction=3607772528 arrival_ns=17617579130800
[2026-11-04 13:30:47.842] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4048231488
[2026-11-04 13:30:47.842] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:47.899] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=4073710244 ntp_seconds=4002759047 ntp_fraction=3861175599 arrival_ns=17617637991900
[2026-11-04 13:30:47.900] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4073710244
[2026-11-04 13:30:47.900] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:47.939] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=4211499259 ntp_seconds=4002759047 ntp_fraction=4028679323 arrival_ns=17617677007100
[2026-11-04 13:30:47.939] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4211499259
[2026-11-04 13:30:47.939] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:52.774] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2557172439 ntp_seconds=4002759052 ntp_fraction=3320009719 arrival_ns=17622512180800
[2026-11-04 13:30:52.774] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2557172439
[2026-11-04 13:30:52.774] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:52.848] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=4048231488 ntp_seconds=4002759052 ntp_fraction=3637837299 arrival_ns=17622586003100
[2026-11-04 13:30:52.848] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4048231488
[2026-11-04 13:30:52.848] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:52.902] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=4073710244 ntp_seconds=4002759052 ntp_fraction=3869765533 arrival_ns=17622640110600
[2026-11-04 13:30:52.902] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4073710244
[2026-11-04 13:30:52.902] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:52.947] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=4211499259 ntp_seconds=4002759052 ntp_fraction=4067334029 arrival_ns=17622685894800
[2026-11-04 13:30:52.948] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4211499259
[2026-11-04 13:30:52.948] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:57.778] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2557172439 ntp_seconds=4002759057 ntp_fraction=3337189588 arrival_ns=17627516262800
[2026-11-04 13:30:57.778] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2557172439
[2026-11-04 13:30:57.778] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:57.882] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=4048231488 ntp_seconds=4002759057 ntp_fraction=3783866187 arrival_ns=17627620293400
[2026-11-04 13:30:57.883] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4048231488
[2026-11-04 13:30:57.883] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:57.933] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=4073710244 ntp_seconds=4002759057 ntp_fraction=4002909519 arrival_ns=17627671064900
[2026-11-04 13:30:57.933] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4073710244
[2026-11-04 13:30:57.933] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:30:57.950] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=4211499259 ntp_seconds=4002759057 ntp_fraction=4075923963 arrival_ns=17627688278700
[2026-11-04 13:30:57.950] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4211499259
[2026-11-04 13:30:57.950] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:31:02.799] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2557172439 ntp_seconds=4002759062 ntp_fraction=3431678869 arrival_ns=17632537481700
[2026-11-04 13:31:02.799] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2557172439
[2026-11-04 13:31:02.799] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:31:02.914] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=4048231488 ntp_seconds=4002759062 ntp_fraction=3921305141 arrival_ns=17632652226200
[2026-11-04 13:31:02.914] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4048231488
[2026-11-04 13:31:02.914] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:31:02.972] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=4211499259 ntp_seconds=4002759062 ntp_fraction=4174708211 arrival_ns=17632711146900
[2026-11-04 13:31:02.974] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4211499259
[2026-11-04 13:31:02.974] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:31:02.974] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=4073710244 ntp_seconds=4002759062 ntp_fraction=4174708211 arrival_ns=17632711279600
[2026-11-04 13:31:02.974] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4073710244
[2026-11-04 13:31:02.975] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:31:07.826] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=2557172439 ntp_seconds=4002759067 ntp_fraction=3547642986 arrival_ns=17637565135500
[2026-11-04 13:31:07.827] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=2557172439
[2026-11-04 13:31:07.827] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:31:07.940] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=4048231488 ntp_seconds=4002759067 ntp_fraction=4037269258 arrival_ns=17637678368100
[2026-11-04 13:31:07.940] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4048231488
[2026-11-04 13:31:07.941] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:31:07.983] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=4211499259 ntp_seconds=4002759067 ntp_fraction=4221952851 arrival_ns=17637721752200
[2026-11-04 13:31:07.983] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4211499259
[2026-11-04 13:31:07.984] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:31:07.983] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=4073710244 ntp_seconds=4002759067 ntp_fraction=4221952851 arrival_ns=17637722008500
[2026-11-04 13:31:07.984] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4073710244
[2026-11-04 13:31:07.985] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 13:31:11.943] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=17641681965200
[2026-11-04 13:31:11.944] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=3 generation=2 invalidated_generation=1 locked=0
[2026-11-04 13:31:11.943] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=17641681987500
[2026-11-04 13:31:11.944] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=3 generation=2 invalidated_generation=1 locked=0
[2026-11-04 13:31:11.944] [info] [graph][runtime.node] av_reacquisition group=composition-r384.source.0 phase=purging old=1 next=2 transition=1
[2026-11-04 13:31:11.944] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=11 group=composition-r384.source.0 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=23 video_raw_generation=0 video_ssrc=4211499259
[2026-11-04 13:31:11.944] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=30 group=composition-r384.source.1 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=23 video_raw_generation=0 video_ssrc=4048231488
[2026-11-04 13:31:11.945] [info] [graph][runtime.node] rtp_ingress_purge node=8 transition=1 cutoff=0
[2026-11-04 13:31:11.945] [info] [graph][runtime.node] av_reacquisition group=composition-r384.source.1 phase=purging old=1 next=2 transition=1
[2026-11-04 13:31:11.946] [info] [graph][runtime.node] rtp_binder_purge node=11 transition=1 cutoff=3608
[2026-11-04 13:31:11.947] [info] [graph][runtime.node] av_reacquisition group=composition-r384.source.0 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=1 next=2 transition=1
[2026-11-04 13:31:11.948] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=17641686303700
[2026-11-04 13:31:11.948] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=3 generation=2 invalidated_generation=1 locked=0
[2026-11-04 13:31:11.948] [info] [graph][runtime.node] rtp_ingress_purge node=27 transition=1 cutoff=0
[2026-11-04 13:31:11.948] [info] [graph][runtime.node] rtp_binder_purge node=30 transition=1 cutoff=3608
[2026-11-04 13:31:11.948] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=50 group=composition-r384.source.2 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=46 video_raw_generation=0 video_ssrc=4073710244 audio_raw_generation=0 audio_ssrc=2557172439
[2026-11-04 13:31:11.949] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=17641687336100
[2026-11-04 13:31:11.949] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=3 generation=2 invalidated_generation=1 locked=0
[2026-11-04 13:31:11.949] [info] [graph][runtime.node] av_reacquisition group=composition-r384.source.1 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=1 next=2 transition=1
[2026-11-04 13:31:11.948] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=51 group=composition-r384.source.2 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=46 video_raw_generation=0 video_ssrc=4073710244 audio_raw_generation=0 audio_ssrc=2557172439
[2026-11-04 13:31:11.949] [info] [graph][runtime.node] av_reacquisition group=composition-r384.source.2 phase=purging old=1 next=2 transition=1
[2026-11-04 13:31:11.952] [info] [graph][runtime.node] rtp_ingress_purge node=46 transition=1 cutoff=0
[2026-11-04 13:31:11.952] [info] [graph][runtime.node] rtp_ingress_purge node=47 transition=1 cutoff=0
[2026-11-04 13:31:11.952] [info] [graph][runtime.node] rtp_binder_purge node=51 transition=1 cutoff=5169
[2026-11-04 13:31:11.952] [info] [graph][runtime.node] rtp_binder_purge node=50 transition=1 cutoff=3608
[2026-11-04 13:31:11.953] [info] [graph][runtime.node] av_reacquisition group=composition-r384.source.2 event=purge_completed purge_ack=complete continuation=acquiring ack_count=2 old=1 next=2 transition=1
[2026-11-04 13:31:13.397] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=120911953700 present_generation=1 present_canonical_target_ns=115333514739 present_canonical_dispatch_ns=115333514739 present_target_master_ns=121025720607 present_dispatch_master_ns=121025720607
[2026-11-04 13:31:13.398] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=120913177100 present_generation=1 present_canonical_target_ns=115366666666 present_canonical_dispatch_ns=115366666666 present_target_master_ns=121058872534 present_dispatch_master_ns=121058872534
[2026-11-04 13:31:19.428] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=126943314800 present_generation=1 present_canonical_target_ns=121370702947 present_canonical_dispatch_ns=121370702947 present_target_master_ns=127062908815 present_dispatch_master_ns=127062908815
[2026-11-04 13:31:19.429] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=126944082200 present_generation=1 present_canonical_target_ns=121400000000 present_canonical_dispatch_ns=121400000000 present_target_master_ns=127092205868 present_dispatch_master_ns=127092205868
[2026-11-04 13:31:29.795] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3991523466 ntp_seconds=4002759089 ntp_fraction=3414499000 arrival_ns=17659533775100
[2026-11-04 13:31:29.795] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3991523466
[2026-11-04 13:31:29.795] [info] [graph][runtime.node] rtp_binder_boundary stage=deadline_established node=51 group=composition-r384.source.2 buffered=0 phase=2 preserves_output=1 raw_ssrc=3036838618 raw_generation=1 raw_sequence=5171 old_generation=1 next_generation=2 transition_sequence=1 published_generation=1
[2026-11-04 13:31:29.796] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=0 generation=2 invalidated_generation=1 locked=0
[2026-11-04 13:31:29.796] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3036838618 ntp_seconds=4002759089 ntp_fraction=3414499000 arrival_ns=17659533766900
[2026-11-04 13:31:29.796] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3036838618
[2026-11-04 13:31:29.796] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:31:30.340] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=1 generation=2
[2026-11-04 13:31:30.834] [info] [graph][runtime.node] source_video_trace_summary node_id=61 node=ActivationReleaseSequencer generation=1 reason=generation_change steady_ns=17660572608300 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3555 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:31:30.834] [info] [graph][runtime.node] source_video_trace node_id=61 node=ActivationReleaseSequencer stage=activation source_identity=composition-r384.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=1612879703 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=17660572848400 group=composition-r384.source.2 release_identity=15737 transition_sequence=1 epoch_source_start_ns=993000000 epoch_master_release_ns=138448580300 video_count=16 min_presentation_ns=993000000 max_end_ns=1526333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:31:30.834] [info] [graph][runtime.node] source_video_trace node_id=61 node=ActivationReleaseSequencer stage=release_published source_identity=composition-r384.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=1612879703 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=17660573007200 group=composition-r384.source.2 release_identity=15737 transition_sequence=1 epoch_source_start_ns=993000000 epoch_master_release_ns=138448580300 video_count=16 min_presentation_ns=993000000 max_end_ns=1526333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:31:30.834] [info] [graph][runtime.node] source_video_trace_summary node_id=62 node=MediaAvBoundReleaseExtractorNode generation=1 reason=generation_change steady_ns=17660573025700 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=3571 release_committed_count=3555 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:31:30.834] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=release source_identity=composition-r384.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=1612879703 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=17660573241500 group=composition-r384.source.2 release_identity=15737 transition_sequence=1 epoch_source_start_ns=993000000 epoch_master_release_ns=138448580300 video_count=16 min_presentation_ns=993000000 max_end_ns=1526333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:31:30.835] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=release_committed source_identity=composition-r384.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=1612879703 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=17660573376300 group=composition-r384.source.2 release_identity=15737 transition_sequence=1 epoch_source_start_ns=993000000 epoch_master_release_ns=138448580300 video_count=16 min_presentation_ns=993000000 max_end_ns=1526333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=1 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:31:30.835] [info] [graph][runtime.node] source_video_trace_summary node_id=64 node=VideoDecodeNode generation=1 reason=generation_change steady_ns=17660573418200 input_count=3461 accepted_count=3461 would_block_count=0 resolved_count=3461 output_attempt_count=0 published_count=3460 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=108 incomplete=0
[2026-11-04 13:31:30.835] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=input source_identity=composition-r384.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=1612879703 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=17660573627200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=108 incomplete=0
[2026-11-04 13:31:30.838] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=accepted source_identity=composition-r384.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=1612879703 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=17660576925500 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=108 incomplete=0
[2026-11-04 13:31:30.838] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=resolved source_identity=composition-r384.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=1612879703 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=17660577207900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=108 incomplete=0
[2026-11-04 13:31:30.839] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=published source_identity=composition-r384.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=1612879703 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=17660577319400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=108 incomplete=0
[2026-11-04 13:31:30.839] [info] [graph][runtime.node] source_video_trace_summary node_id=65 node=HardwareTransferNode generation=1 reason=generation_change steady_ns=17660577345000 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3460 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:31:30.839] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-11-04 13:31:30.839] [info] [graph][runtime.node] source_video_trace node_id=65 node=HardwareTransferNode stage=published source_identity=composition-r384.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=1612879703 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=17660577547900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:31:30.839] [info] [graph][runtime.node] source_video_trace_summary node_id=66 node=VideoFrameRateNode generation=1 reason=generation_change steady_ns=17660577721700 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3451 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:31:30.839] [info] [graph][runtime.node] source_video_trace_summary node_id=68 node=VideoFilterNode generation=1 reason=generation_change steady_ns=17660577765800 input_count=3451 accepted_count=3451 would_block_count=0 resolved_count=3451 output_attempt_count=0 published_count=3451 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:31:30.839] [info] [graph][runtime.node] source_video_trace node_id=66 node=VideoFrameRateNode stage=published source_identity=composition-r384.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=1612879703 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=17660577901200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:31:30.839] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=input source_identity=composition-r384.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=1612879703 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=17660577994200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:31:30.840] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=accepted source_identity=composition-r384.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=1612879703 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=17660578269100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:31:30.840] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=resolved source_identity=composition-r384.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=537627 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=17660578482700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:31:30.840] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=published source_identity=composition-r384.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=537627 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=17660578623400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:31:30.840] [info] [graph][runtime.node] source_video_trace_summary node_id=67 node=VideoPreparationNode generation=1 reason=generation_change steady_ns=17660578643000 input_count=3441 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3441 published_count=3432 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=10 incomplete=0
[2026-11-04 13:31:30.840] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=input source_identity=composition-r384.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=537627 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=17660578836600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=10 incomplete=0
[2026-11-04 13:31:30.840] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=output_attempt source_identity=composition-r384.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=537627 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=17660578962500 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=10 incomplete=0
[2026-11-04 13:31:30.840] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=published source_identity=composition-r384.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=537627 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=17660579062100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=10 incomplete=0
[2026-11-04 13:31:30.842] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_candidate source_identity=composition-r384.source.2.input.video generation=2 steady_ns=17660580310700 source_sequence=3 presentation_ns=1059666666 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-04 13:31:30.844] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-r384.source.2.input.video generation=2 steady_ns=17660582822200 source_sequence=3 presentation_ns=1059666666 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-04 13:31:34.807] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3991523466 ntp_seconds=4002759094 ntp_fraction=3461743640 arrival_ns=17664544772200
[2026-11-04 13:31:34.807] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3991523466
[2026-11-04 13:31:34.807] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:31:34.807] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3036838618 ntp_seconds=4002759094 ntp_fraction=3466038607 arrival_ns=17664545683900
[2026-11-04 13:31:34.807] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3036838618
[2026-11-04 13:31:34.808] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:31:39.811] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3036838618 ntp_seconds=4002759099 ntp_fraction=3483218477 arrival_ns=17669549857200
[2026-11-04 13:31:39.812] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3036838618
[2026-11-04 13:31:39.812] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:31:39.812] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3991523466 ntp_seconds=4002759099 ntp_fraction=3483218477 arrival_ns=17669549982600
[2026-11-04 13:31:39.812] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3991523466
[2026-11-04 13:31:39.813] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:31:44.817] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3036838618 ntp_seconds=4002759104 ntp_fraction=3508988280 arrival_ns=17674556107400
[2026-11-04 13:31:44.818] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3036838618
[2026-11-04 13:31:44.818] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:31:44.832] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3991523466 ntp_seconds=4002759104 ntp_fraction=3573412790 arrival_ns=17674570862300
[2026-11-04 13:31:44.833] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3991523466
[2026-11-04 13:31:44.833] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:31:46.526] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=154041010800 present_generation=1 present_canonical_target_ns=148468390022 present_canonical_dispatch_ns=148468390022 present_target_master_ns=154160595890 present_dispatch_master_ns=154160595890
[2026-11-04 13:31:46.542] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=154056967100 present_generation=1 present_canonical_target_ns=148500000000 present_canonical_dispatch_ns=148500000000 present_target_master_ns=154192205868 present_dispatch_master_ns=154192205868
[2026-11-04 13:31:49.821] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3036838618 ntp_seconds=4002759109 ntp_fraction=3526168150 arrival_ns=17679559534000
[2026-11-04 13:31:49.821] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3036838618
[2026-11-04 13:31:49.821] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:31:49.834] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3991523466 ntp_seconds=4002759109 ntp_fraction=3582002724 arrival_ns=17679572443100
[2026-11-04 13:31:49.834] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3991523466
[2026-11-04 13:31:49.834] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:31:54.824] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3036838618 ntp_seconds=4002759114 ntp_fraction=3534758084 arrival_ns=17684562152900
[2026-11-04 13:31:54.824] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3036838618
[2026-11-04 13:31:54.824] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:31:54.839] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3991523466 ntp_seconds=4002759114 ntp_fraction=3603477561 arrival_ns=17684577633100
[2026-11-04 13:31:54.839] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3991523466
[2026-11-04 13:31:54.840] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[CLI] [2026-11-04 13:31:59.838] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3036838618 ntp_seconds=4002759119 ntp_fraction=3599182594 arrival_ns=17689576339100
runtime report: queued=20, peakQueued=915, threads=81, processThreads=129, workers=75, workerProgress=378473, workerProcessCalls=1451017, workerWaits=1072537, workerWakeups=1060171, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=516, logicalProcessors=22, averageSystemMachineCpuPercent=10.971125, averageProcessMachineCpuPercent=1.930984, peakProcessMachineCpuPercent=5.816135, averageProcessSingleCoreCpuPercent=42.481640, peakProcessSingleCoreCpuPercent=127.954972, initialWorkingSetBytes=209924096, workingSetBytes=258990080, peakWorkingSetBytes=269582336, totalPushed=326551, totalPopped=326507, droppedBuffers=0, encodedPacketsPushed=54257, encodedPacketsPopped=54256, backpressureItems=123, graphPayloadCurrentBytes=237999, graphPayloadHighWaterBytes=16106896, graphPayloadCurrentObjects=51, graphPayloadHighWaterObjects=745, graphPayloadReservations=137664, graphPayloadReleases=137613, graphPayloadPressureFailures=0, lastInputReceivedAtNs=17689549889700[2026-11-04 13:31:59.838] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3036838618

[2026-11-04 13:31:59.838] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:31:59.868] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3991523466 ntp_seconds=4002759119 ntp_fraction=3728031612 arrival_ns=17689606745800
[2026-11-04 13:31:59.868] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3991523466
[2026-11-04 13:31:59.868] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:32:04.844] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3036838618 ntp_seconds=4002759124 ntp_fraction=3624952397 arrival_ns=17694582919700
[2026-11-04 13:32:04.845] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3036838618
[2026-11-04 13:32:04.845] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:32:04.889] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3991523466 ntp_seconds=4002759124 ntp_fraction=3818225926 arrival_ns=17694627693300
[2026-11-04 13:32:04.889] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3991523466
[2026-11-04 13:32:04.890] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:32:09.848] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3036838618 ntp_seconds=4002759129 ntp_fraction=3642132267 arrival_ns=17699586946800
[2026-11-04 13:32:09.849] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3036838618
[2026-11-04 13:32:09.849] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:32:09.895] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3991523466 ntp_seconds=4002759129 ntp_fraction=3843995729 arrival_ns=17699633809300
[2026-11-04 13:32:09.896] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3991523466
[2026-11-04 13:32:09.896] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:32:14.864] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3036838618 ntp_seconds=4002759134 ntp_fraction=3710851743 arrival_ns=17704602454500
[2026-11-04 13:32:14.864] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3036838618
[2026-11-04 13:32:14.864] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:32:14.938] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3991523466 ntp_seconds=4002759134 ntp_fraction=4028679323 arrival_ns=17704677044800
[2026-11-04 13:32:14.939] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3991523466
[2026-11-04 13:32:14.939] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:32:19.879] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3036838618 ntp_seconds=4002759139 ntp_fraction=3775276253 arrival_ns=17709618002800
[2026-11-04 13:32:19.880] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3036838618
[2026-11-04 13:32:19.880] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:32:19.943] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3991523466 ntp_seconds=4002759139 ntp_fraction=4050154160 arrival_ns=17709681591800
[2026-11-04 13:32:19.944] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3991523466
[2026-11-04 13:32:19.944] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:32:24.887] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3036838618 ntp_seconds=4002759144 ntp_fraction=3805341024 arrival_ns=17714625112800
[2026-11-04 13:32:24.887] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3036838618
[2026-11-04 13:32:24.887] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:32:24.966] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3991523466 ntp_seconds=4002759144 ntp_fraction=4144643440 arrival_ns=17714704054000
[2026-11-04 13:32:24.966] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3991523466
[2026-11-04 13:32:24.967] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:32:29.906] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3036838618 ntp_seconds=4002759149 ntp_fraction=3891240370 arrival_ns=17719644337600
[2026-11-04 13:32:29.907] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3036838618
[2026-11-04 13:32:29.907] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:32:29.969] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3991523466 ntp_seconds=4002759149 ntp_fraction=4148938407 arrival_ns=17719704842400
[2026-11-04 13:32:29.969] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3991523466
[2026-11-04 13:32:29.969] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:32:31.436] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=198950727800 present_generation=1 present_canonical_target_ns=193375782312 present_canonical_dispatch_ns=193375782312 present_target_master_ns=199067988180 present_dispatch_master_ns=199067988180
[2026-11-04 13:32:31.436] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=198951229100 present_generation=1 present_canonical_target_ns=193400000000 present_canonical_dispatch_ns=193400000000 present_target_master_ns=199092205868 present_dispatch_master_ns=199092205868
[2026-11-04 13:32:32.204] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=199719095500 present_generation=1 present_canonical_target_ns=194142040816 present_canonical_dispatch_ns=194142040816 present_target_master_ns=199834246684 present_dispatch_master_ns=199834246684
[2026-11-04 13:32:32.206] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=199720489700 present_generation=1 present_canonical_target_ns=194166666666 present_canonical_dispatch_ns=194166666666 present_target_master_ns=199858872534 present_dispatch_master_ns=199858872534
[2026-11-04 13:32:34.925] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3036838618 ntp_seconds=4002759154 ntp_fraction=3972844748 arrival_ns=17724663934900
[2026-11-04 13:32:34.926] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3036838618
[2026-11-04 13:32:34.926] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:32:34.973] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3991523466 ntp_seconds=4002759154 ntp_fraction=4174708211 arrival_ns=17724711049600
[2026-11-04 13:32:34.974] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3991523466
[2026-11-04 13:32:34.974] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:32:39.944] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3036838618 ntp_seconds=4002759159 ntp_fraction=4050154160 arrival_ns=17729682233200
[2026-11-04 13:32:39.944] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3036838618
[2026-11-04 13:32:39.945] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:32:39.974] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3991523466 ntp_seconds=4002759159 ntp_fraction=4183298146 arrival_ns=17729713035400
[2026-11-04 13:32:39.975] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3991523466
[2026-11-04 13:32:39.975] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:32:40.399] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=207914190400 present_generation=1 present_canonical_target_ns=202338684807 present_canonical_dispatch_ns=202338684807 present_target_master_ns=208030890675 present_dispatch_master_ns=208030890675
[2026-11-04 13:32:40.402] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=207917279300 present_generation=1 present_canonical_target_ns=202366666666 present_canonical_dispatch_ns=202366666666 present_target_master_ns=208058872534 present_dispatch_master_ns=208058872534
[2026-11-04 13:32:44.955] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3036838618 ntp_seconds=4002759164 ntp_fraction=4101693767 arrival_ns=17734693593200
[2026-11-04 13:32:44.955] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3036838618
[2026-11-04 13:32:44.955] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:32:45.002] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3991523466 ntp_seconds=4002759165 ntp_fraction=4294967 arrival_ns=17734740238900
[2026-11-04 13:32:45.002] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3991523466
[2026-11-04 13:32:45.003] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:32:49.963] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3036838618 ntp_seconds=4002759169 ntp_fraction=4131758538 arrival_ns=17739701019300
[2026-11-04 13:32:49.963] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3036838618
[2026-11-04 13:32:49.963] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:32:50.009] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3991523466 ntp_seconds=4002759170 ntp_fraction=38654705 arrival_ns=17739747855200
[2026-11-04 13:32:50.010] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3991523466
[2026-11-04 13:32:50.010] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:32:54.988] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3036838618 ntp_seconds=4002759174 ntp_fraction=4239132721 arrival_ns=17744726234300
[2026-11-04 13:32:54.988] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3036838618
[2026-11-04 13:32:54.988] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:32:55.034] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3991523466 ntp_seconds=4002759175 ntp_fraction=146028888 arrival_ns=17744772668800
[2026-11-04 13:32:55.034] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3991523466
[2026-11-04 13:32:55.035] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:32:57.395] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=224910389700 present_generation=1 present_canonical_target_ns=219335691609 present_canonical_dispatch_ns=219335691609 present_target_master_ns=225027897477 present_dispatch_master_ns=225027897477
[2026-11-04 13:32:57.401] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=224916288600 present_generation=1 present_canonical_target_ns=219366666666 present_canonical_dispatch_ns=219366666666 present_target_master_ns=225058872534 present_dispatch_master_ns=225058872534
[2026-11-04 13:33:00.004] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3036838618 ntp_seconds=4002759180 ntp_fraction=12884901 arrival_ns=17749742226700
[2026-11-04 13:33:00.004] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3036838618
[2026-11-04 13:33:00.004] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:33:00.065] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3991523466 ntp_seconds=4002759180 ntp_fraction=279172874 arrival_ns=17749803621400
[2026-11-04 13:33:00.065] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3991523466
[2026-11-04 13:33:00.066] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:33:05.018] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3036838618 ntp_seconds=4002759185 ntp_fraction=73014444 arrival_ns=17754756409700
[2026-11-04 13:33:05.018] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3036838618
[2026-11-04 13:33:05.018] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:33:05.096] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3991523466 ntp_seconds=4002759185 ntp_fraction=412316860 arrival_ns=17754834355600
[2026-11-04 13:33:05.096] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3991523466
[2026-11-04 13:33:05.097] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:33:10.019] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3036838618 ntp_seconds=4002759190 ntp_fraction=81604378 arrival_ns=17759757470100
[2026-11-04 13:33:10.019] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3036838618
[2026-11-04 13:33:10.020] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:33:10.108] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3991523466 ntp_seconds=4002759190 ntp_fraction=463856467 arrival_ns=17759846551300
[2026-11-04 13:33:10.108] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3991523466
[2026-11-04 13:33:10.108] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:33:15.026] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3036838618 ntp_seconds=4002759195 ntp_fraction=107374182 arrival_ns=17764764280700
[2026-11-04 13:33:15.026] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3036838618
[2026-11-04 13:33:15.026] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:33:15.135] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3991523466 ntp_seconds=4002759195 ntp_fraction=579820584 arrival_ns=17764873388300
[2026-11-04 13:33:15.136] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3991523466
[2026-11-04 13:33:15.136] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:33:16.968] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=244483132600 present_generation=1 present_canonical_target_ns=238910113378 present_canonical_dispatch_ns=238910113378 present_target_master_ns=244602319246 present_dispatch_master_ns=244602319246
[2026-11-04 13:33:16.975] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=244489795900 present_generation=1 present_canonical_target_ns=238933333333 present_canonical_dispatch_ns=238933333333 present_target_master_ns=244625539201 present_dispatch_master_ns=244625539201
[2026-11-04 13:33:20.036] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3036838618 ntp_seconds=4002759200 ntp_fraction=150323855 arrival_ns=17769774267000
[2026-11-04 13:33:20.036] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3036838618
[2026-11-04 13:33:20.036] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:33:20.175] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3991523466 ntp_seconds=4002759200 ntp_fraction=747324309 arrival_ns=17769913181100
[2026-11-04 13:33:20.175] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3991523466
[2026-11-04 13:33:20.175] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:33:25.047] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3036838618 ntp_seconds=4002759205 ntp_fraction=197568495 arrival_ns=17774785168100
[2026-11-04 13:33:25.047] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3036838618
[2026-11-04 13:33:25.047] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:33:25.209] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3991523466 ntp_seconds=4002759205 ntp_fraction=893353197 arrival_ns=17774947001100
[2026-11-04 13:33:25.209] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3991523466
[2026-11-04 13:33:25.209] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 13:33:29.281] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=2 reason=1 observed_at_ns=17779019577700
[2026-11-04 13:33:29.282] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=3 generation=3 invalidated_generation=2 locked=0
[2026-11-04 13:33:29.282] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=50 group=composition-r384.source.2 buffered=0 phase=0 preserves_output=1 protocol_generation=2 evidence_revision=94 video_raw_generation=1 video_ssrc=3991523466 audio_raw_generation=1 audio_ssrc=3036838618
[2026-11-04 13:33:29.282] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=51 group=composition-r384.source.2 buffered=0 phase=0 preserves_output=1 protocol_generation=2 evidence_revision=94 video_raw_generation=1 video_ssrc=3991523466 audio_raw_generation=1 audio_ssrc=3036838618
[2026-11-04 13:33:29.283] [info] [graph][runtime.node] rtp_ingress_purge node=46 transition=2 cutoff=0
[2026-11-04 13:33:29.283] [info] [graph][runtime.node] rtp_binder_purge node=50 transition=2 cutoff=7203
[2026-11-04 13:33:29.283] [info] [graph][runtime.node] rtp_ingress_purge node=47 transition=2 cutoff=0
[2026-11-04 13:33:29.283] [info] [graph][runtime.node] rtp_binder_purge node=51 transition=2 cutoff=10361
[2026-11-04 13:33:29.284] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=2 reason=1 observed_at_ns=17779022231000
[2026-11-04 13:33:29.285] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=3 generation=3 invalidated_generation=2 locked=0
[2026-11-04 13:33:29.284] [info] [graph][runtime.node] av_reacquisition group=composition-r384.source.2 event=purge_completed purge_ack=complete continuation=acquiring ack_count=2 old=2 next=3 transition=2
[2026-11-04 13:33:29.282] [info] [graph][runtime.node] av_reacquisition group=composition-r384.source.2 phase=purging old=2 next=3 transition=2
[2026-11-04 13:33:31.295] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=258809661300 present_generation=1 present_canonical_target_ns=253236825396 present_canonical_dispatch_ns=253236825396 present_target_master_ns=258929031264 present_dispatch_master_ns=258929031264
[2026-11-04 13:33:31.298] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=258813451600 present_generation=1 present_canonical_target_ns=253266666666 present_canonical_dispatch_ns=253266666666 present_target_master_ns=258958872534 present_dispatch_master_ns=258958872534
[2026-11-04 13:33:48.321] [info] [graph][runtime.lifecycle] continuous_output_drain stage=requested
[2026-11-04 13:33:48.321] [info] [graph][runtime.lifecycle] worker.finished node=4
[2026-11-04 13:33:48.321] [info] [graph][runtime.lifecycle] worker.finished node=80
[2026-11-04 13:33:48.321] [info] [graph][runtime.lifecycle] worker.finished node=77
[2026-11-04 13:33:48.321] [info] [graph][runtime.lifecycle] worker.finished node=2
[2026-11-04 13:33:48.322] [info] [graph][runtime.node] worker.thread_cpu node=2 available=1 cpu_ns=3203125000 process_calls=24335
[2026-11-04 13:33:48.321] [info] [graph][runtime.node] worker.thread_cpu node=4 available=1 cpu_ns=5593750000 process_calls=36566
[2026-11-04 13:33:48.321] [info] [graph][runtime.node] worker.thread_cpu node=80 available=1 cpu_ns=15625000 process_calls=6
[2026-11-04 13:33:48.321] [info] [graph][runtime.node] worker.thread_cpu node=77 available=1 cpu_ns=0 process_calls=7
[2026-11-04 13:33:48.321] [info] [graph][runtime.lifecycle] worker.finished node=5
[2026-11-04 13:33:48.322] [info] [graph][runtime.node] worker.thread_cpu node=5 available=1 cpu_ns=421875000 process_calls=34932
[2026-11-04 13:33:48.353] [info] [graph][runtime.lifecycle] worker.finished node=74
[2026-11-04 13:33:48.353] [info] [graph][runtime.lifecycle] worker.finished node=75
[2026-11-04 13:33:48.353] [info] [graph][runtime.lifecycle] worker.finished node=78
[2026-11-04 13:33:48.353] [info] [graph][runtime.node] worker.thread_cpu node=74 available=1 cpu_ns=890625000 process_calls=58860
[2026-11-04 13:33:48.353] [info] [graph][runtime.node] worker.thread_cpu node=75 available=1 cpu_ns=296875000 process_calls=59216
[2026-11-04 13:33:48.353] [info] [graph][runtime.node] worker.thread_cpu node=78 available=1 cpu_ns=359375000 process_calls=58448
[2026-11-04 13:33:48.381] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 produced_datagrams=172601 produced_ts_payload_bytes=203415248 pending_bytes=0 peak_pending_bytes=110920 access_units=19752 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=32789115 maximum_mux_input_after_emit_ns=36365215 maximum_mux_input_after_dispatch_ns=-63634785 worst_mux_input_ready_ns=95629115300 worst_mux_input_emit_ns=95592750085 worst_mux_input_dispatch_ns=95692750085 maximum_protocol_batch_produced_after_release_ns=46781392 worst_protocol_batch_produced_ns=87638988900 worst_protocol_batch_release_ns=87592207508 worst_protocol_batch_deadline_ns=87658874176 worst_protocol_batch_datagrams=1 exit_reason=completed
[2026-11-04 13:33:48.382] [info] [graph][runtime.lifecycle] worker.finished node=76
[2026-11-04 13:33:48.382] [info] [graph][runtime.node] datagram_batch_trace stage=last_completed generation=1 first_sequence=172601 last_sequence=172601 maximum_submit_interval_ns=60200 maximum_submit_interval_sequence=172601 maximum_start_lateness_ns=486569 maximum_start_lateness_sequence=172601 first_release_master_ns=275895697931 first_deadline_master_ns=275992107596 last_release_master_ns=275895697931 last_deadline_master_ns=275992107596 first_submitted_master_ns=275896244700 last_submitted_master_ns=275896244700 worst_wait_started_master_ns=275893013000 worst_wait_completed_master_ns=275896182500 worst_submit_observed_master_ns=275896184500 wait_attempts=1 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=1 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=1 wait_call_total_ns=3167000 wait_call_max_ns=3167000 wait_before_call_count=1 wait_before_call_total_ns=200 wait_before_call_max_ns=200 wait_arm_count=1 wait_arm_total_ns=2400 wait_arm_max_ns=2400 wait_api_wall_count=1 wait_api_wall_total_ns=3162900 wait_api_wall_max_ns=3162900 wait_return_to_next_clock_count=1 wait_return_to_next_clock_total_ns=2300 wait_return_to_next_clock_max_ns=2300 wait_target_lateness_count=1 wait_target_lateness_total_ns=484569 wait_target_lateness_max_ns=484569 worst_wait_metric=call_wall worst_wait_sequence=172601 worst_wait_target_master_ns=275895697931 worst_wait_sampled_master_ns=275893013000 worst_wait_sampled_steady_ns=17798116807800 worst_wait_timeout_ns=2684931 worst_wait_entered_steady_ns=17798116808000 worst_wait_returned_steady_ns=17798119975000 worst_wait_arm_before_steady_ns=17798116808500 worst_wait_arm_after_steady_ns=17798116810900 worst_wait_api_before_steady_ns=17798116811000 worst_wait_api_returned_steady_ns=17798119973900 worst_wait_windows_due_100ns=-26850 worst_wait_windows_result=1 worst_wait_windows_error=unavailable worst_wait_windows_pid=34096 worst_wait_windows_tid=33144 worst_wait_windows_qpc_frequency_hz=10000000 worst_wait_next_master_ns=275896182500 worst_wait_next_steady_ns=17798119977300 worst_wait_outcome=deadline
[2026-11-04 13:33:48.382] [info] [graph][runtime.node] worker.thread_cpu node=76 available=1 cpu_ns=3500000000 process_calls=119694
[2026-11-04 13:33:48.382] [info] [graph][runtime.lifecycle] worker.finished node=79
[2026-11-04 13:33:48.382] [info] [graph][runtime.node] worker.thread_cpu node=79 available=1 cpu_ns=3468750000 process_calls=87729
[2026-11-04 13:33:48.382] [info] [graph][runtime.node] datagram_successful_submit_trace stage=worst_success generation=1 endpoint=1 sequence=149940 pacing_lateness_ns=26130860 matches_pacing_worst=1 materialized_master_ns=unavailable enqueued_master_ns=unavailable submit_interval_scope=pre_transport_checks_and_transport socket_only_ns=unavailable release_master_ns=228258876988 deadline_master_ns=228325543656 original_protocol_deadline_master_ns=228325543656 prefix_completion_limit_master_ns=228325543656 reserved_at_master_ns=228292819200 not_before_master_ns=228266691540 not_after_master_ns=228325509096 service_duration_ns=34560 wire_bytes_per_second=6250000 submit_started_master_ns=228292822400 submit_completed_master_ns=228292884900 activation_started_master_ns=228292793400 activation_completed_master_ns=228292818500 permit_started_master_ns=228292818900 permit_completed_master_ns=228292819000 snapshot_started_master_ns=228292819100 snapshot_sampled_master_ns=228292819200 snapshot_completed_master_ns=228292819700 reservation_completed_master_ns=228292821200 wait_started_master_ns=228292822000 wait_completed_master_ns=228292822000 submit_observed_master_ns=228292822400 wait_attempts=0 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=0 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=0 wait_call_total_ns=0 wait_call_max_ns=0 wait_before_call_count=0 wait_before_call_total_ns=0 wait_before_call_max_ns=0 wait_arm_count=0 wait_arm_total_ns=0 wait_arm_max_ns=0 wait_api_wall_count=0 wait_api_wall_total_ns=0 wait_api_wall_max_ns=0 wait_return_to_next_clock_count=0 wait_return_to_next_clock_total_ns=0 wait_return_to_next_clock_max_ns=0 wait_target_lateness_count=0 wait_target_lateness_total_ns=0 wait_target_lateness_max_ns=0
[2026-11-04 13:33:48.383] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=29222 committed_datagrams=172601 committed_payload_bytes=203415248 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=79296 pacing_reserved=172601 pacing_cancelled=0 pacing_submitted=172601 pacing_maximum_submit_lateness_ns=26130860 pacing_worst_late_global_sequence=149940 pacing_worst_success_sample_available=1 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=79296 backlog_max_residence_ns=50590100 last_materialized_sequence=172601 last_scheduled_sequence=172601 last_submitted_sequence=172601 last_committed_sequence=172601 submitted_service_generation=1 submitted_service_sequence=172601 submitted_service_completed_ns=275896244700 submitted_service_duration_ns=34560 submitted_service_available_ns=275896279260 submitted_scope_available_ns=275896279260 shared_scope_charged_datagrams=172601 shared_scope_charged_wire_bytes=208248076 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2154 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2154 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=188010 endpoint_1_target_effective_socket_bytes=188010 endpoint_1_api_requested_socket_bytes=188010 endpoint_1_effective_socket_bytes=188010 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=172601 endpoint_1_committed_payload_bytes=203415248
[2026-11-04 13:33:48.383] [info] [graph][runtime.lifecycle] worker.finished node=81
[2026-11-04 13:33:48.383] [info] [graph][runtime.node] worker.thread_cpu node=81 available=1 cpu_ns=11671875000 process_calls=58558
[2026-11-04 13:33:48.383] [info] [graph][runtime.lifecycle] distribution_registration scope=worker stage=leave node=81 pid=34096 tid=33144 native=0
[2026-11-04 13:33:48.586] [info] [graph][runtime.lifecycle] continuous_output_drain stage=completed
[2026-11-04 13:33:48.586] [info] [graph][runtime.lifecycle] stop.begin
[2026-11-04 13:33:48.586] [info] [graph][runtime.node] worker.thread_cpu node=8 available=1 cpu_ns=328125000 process_calls=87682
[2026-11-04 13:33:48.586] [info] [graph][runtime.node] worker.thread_cpu node=27 available=1 cpu_ns=187500000 process_calls=88367
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=60 available=1 cpu_ns=375000000 process_calls=51723
[2026-11-04 13:33:48.586] [info] [graph][runtime.node] worker.thread_cpu node=48 available=1 cpu_ns=93750000 process_calls=319
[2026-11-04 13:33:48.586] [info] [graph][runtime.node] worker.thread_cpu node=10 available=1 cpu_ns=0 process_calls=104
[2026-11-04 13:33:48.586] [info] [graph][runtime.node] worker.thread_cpu node=29 available=1 cpu_ns=0 process_calls=105
[2026-11-04 13:33:48.586] [info] [graph][runtime.node] worker.thread_cpu node=49 available=1 cpu_ns=0 process_calls=472
[2026-11-04 13:33:48.586] [info] [graph][runtime.node] worker.thread_cpu node=11 available=1 cpu_ns=78125000 process_calls=10600
[2026-11-04 13:33:48.586] [info] [graph][runtime.node] worker.thread_cpu node=12 available=1 cpu_ns=0 process_calls=79
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=30 available=1 cpu_ns=156250000 process_calls=10599
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=32 available=1 cpu_ns=0 process_calls=106
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=53 available=1 cpu_ns=0 process_calls=466
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=54 available=1 cpu_ns=234375000 process_calls=21680
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=55 available=1 cpu_ns=437500000 process_calls=31028
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=59 available=1 cpu_ns=0 process_calls=301
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=15 available=1 cpu_ns=125000000 process_calls=10728
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=34 available=1 cpu_ns=78125000 process_calls=10767
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=56 available=1 cpu_ns=156250000 process_calls=21536
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=57 available=1 cpu_ns=406250000 process_calls=30858
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=16 available=1 cpu_ns=109375000 process_calls=10830
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=35 available=1 cpu_ns=93750000 process_calls=10827
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=58 available=1 cpu_ns=343750000 process_calls=52590
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=18 available=1 cpu_ns=78125000 process_calls=10672
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=46 available=1 cpu_ns=796875000 process_calls=165259
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=20 available=1 cpu_ns=46875000 process_calls=10586
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=39 available=1 cpu_ns=265625000 process_calls=10597
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=47 available=1 cpu_ns=781250000 process_calls=159902
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=70 available=1 cpu_ns=859375000 process_calls=42455
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=23 available=1 cpu_ns=62500000 process_calls=10578
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=9 available=1 cpu_ns=0 process_calls=78
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=42 available=1 cpu_ns=62500000 process_calls=10564
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=65 available=1 cpu_ns=234375000 process_calls=21340
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=31 available=1 cpu_ns=0 process_calls=82
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=71 available=1 cpu_ns=218750000 process_calls=31232
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=24 available=1 cpu_ns=140625000 process_calls=11368
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=50 available=1 cpu_ns=187500000 process_calls=21388
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=43 available=1 cpu_ns=93750000 process_calls=11235
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=66 available=1 cpu_ns=359375000 process_calls=22158
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=51 available=1 cpu_ns=281250000 process_calls=30702
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=72 available=1 cpu_ns=343750000 process_calls=31106
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=26 available=1 cpu_ns=1140625000 process_calls=13707
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=52 available=1 cpu_ns=0 process_calls=316
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=45 available=1 cpu_ns=953125000 process_calls=13726
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=6 available=1 cpu_ns=3000000000 process_calls=46317
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=13 available=1 cpu_ns=0 process_calls=105
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=14 available=1 cpu_ns=109375000 process_calls=10765
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=17 available=1 cpu_ns=0 process_calls=79
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=33 available=1 cpu_ns=93750000 process_calls=10746
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=36 available=1 cpu_ns=0 process_calls=79
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=37 available=1 cpu_ns=78125000 process_calls=10669
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=19 available=1 cpu_ns=156250000 process_calls=10710
[2026-11-04 13:33:48.586] [info] [graph][runtime.node] worker.thread_cpu node=28 available=1 cpu_ns=15625000 process_calls=79
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=38 available=1 cpu_ns=125000000 process_calls=10727
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=61 available=1 cpu_ns=578125000 process_calls=51399
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=62 available=1 cpu_ns=484375000 process_calls=51612
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=22 available=1 cpu_ns=1234375000 process_calls=14287
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=41 available=1 cpu_ns=1031250000 process_calls=14519
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=64 available=1 cpu_ns=2390625000 process_calls=35111
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=68 available=1 cpu_ns=1453125000 process_calls=25608
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=73 available=1 cpu_ns=781250000 process_calls=50204
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=25 available=1 cpu_ns=156250000 process_calls=10591
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=44 available=1 cpu_ns=93750000 process_calls=10592
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=67 available=1 cpu_ns=250000000 process_calls=21269
[2026-11-04 13:33:48.587] [info] [graph][runtime.node] worker.thread_cpu node=7 available=1 cpu_ns=1578125000 process_calls=46849
[2026-11-04 13:33:48.599] [info] [graph][runtime.node] mpegts_wire_materializer stage=stopped maximum_materialized_after_release_ns=46945992 worst_materialized_at_ns=87639153500 worst_release_ns=87592207508 worst_deadline_ns=87658874176 worst_global_sequence=58538
[2026-11-04 13:33:48.599] [info] [graph][runtime.node] audio_stage_observation node=74 stage=scheduler_input count=11642 last_generation=1 last_begin=11920384 last_end=11921408 last_rate=44100 last_steady_ns=17798068465400 maximum_inter_event_gap_ns=48550900 gap_begin_generation=1 gap_begin_begin=8559616 gap_begin_end=8560640 gap_begin_rate=44100 gap_begin_steady_ns=17721892215500 gap_end_generation=1 gap_end_begin=8560640 gap_end_end=8561664 gap_end_rate=44100 gap_end_steady_ns=17721940766400
[2026-11-04 13:33:48.600] [info] [graph][runtime.node] av_scheduler_correlated_wait generation=1 canonical_sequence=5824 missing=audio begin_steady_ns=17721892227300 end_steady_ns=17721940769700 observed_wait_ns=48542400 held_dispatch_master_ns=199792205868 begin_master_ns=199668432500 end_master_ns=199716974100 dispatch_remaining_at_end_ns=75231768 resumed_dispatch_master_ns=199811026729
[2026-11-04 13:33:48.600] [info] [graph][runtime.node] audio_stage_observation node=5 stage=canonicalizer_input count=11642 last_generation=1 last_begin=11920384 last_end=11921408 last_rate=44100 last_steady_ns=17798059795500 maximum_inter_event_gap_ns=47707900 gap_begin_generation=1 gap_begin_begin=8559616 gap_begin_end=8560640 gap_begin_rate=44100 gap_begin_steady_ns=17721892191300 gap_end_generation=1 gap_end_begin=8560640 gap_end_end=8561664 gap_end_rate=44100 gap_end_steady_ns=17721939899200
[2026-11-04 13:33:48.600] [info] [graph][runtime.node] audio_stage_observation node=5 stage=canonicalizer_committed count=11642 last_generation=1 last_begin=11920384 last_end=11921408 last_rate=44100 last_steady_ns=17798059798100 maximum_inter_event_gap_ns=47730000 gap_begin_generation=1 gap_begin_begin=8559616 gap_begin_end=8560640 gap_begin_rate=44100 gap_begin_steady_ns=17721892197700 gap_end_generation=1 gap_end_begin=8560640 gap_end_end=8561664 gap_end_rate=44100 gap_end_steady_ns=17721939927700
[2026-11-04 13:33:48.600] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_input count=11642 last_generation=1 last_begin=11920384 last_end=11921408 last_rate=44100 last_steady_ns=17798052982100 maximum_inter_event_gap_ns=46607200 gap_begin_generation=1 gap_begin_begin=8561664 gap_begin_end=8562688 gap_begin_rate=44100 gap_begin_steady_ns=17721891859000 gap_end_generation=1 gap_end_begin=8562688 gap_end_end=8563712 gap_end_rate=44100 gap_end_steady_ns=17721938466200
[2026-11-04 13:33:48.600] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_submitted count=11642 last_generation=1 last_begin=11920384 last_end=11921408 last_rate=44100 last_steady_ns=17798053062200 maximum_inter_event_gap_ns=47242300 gap_begin_generation=1 gap_begin_begin=8561664 gap_begin_end=8562688 gap_begin_rate=44100 gap_begin_steady_ns=17721892172100 gap_end_generation=1 gap_end_begin=8562688 gap_end_end=8563712 gap_end_rate=44100 gap_end_steady_ns=17721939414400
[2026-11-04 13:33:48.600] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_packet count=11642 last_generation=1 last_begin=11920384 last_end=11921408 last_rate=44100 last_steady_ns=17798059644900 maximum_inter_event_gap_ns=47253700 gap_begin_generation=1 gap_begin_begin=8559616 gap_begin_end=8560640 gap_begin_rate=44100 gap_begin_steady_ns=17721892178200 gap_end_generation=1 gap_end_begin=8560640 gap_end_end=8561664 gap_end_rate=44100 gap_end_steady_ns=17721939431900
[2026-11-04 13:33:48.600] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_committed count=11642 last_generation=1 last_begin=11920384 last_end=11921408 last_rate=44100 last_steady_ns=17798059646900 maximum_inter_event_gap_ns=47283700 gap_begin_generation=1 gap_begin_begin=8559616 gap_begin_end=8560640 gap_begin_rate=44100 gap_begin_steady_ns=17721892183800 gap_end_generation=1 gap_end_begin=8560640 gap_end_end=8561664 gap_end_rate=44100 gap_end_steady_ns=17721939467500
[2026-11-04 13:33:48.601] [info] [graph][runtime.lifecycle] codec_owner_release node=4 kind=AudioEncode owner_references=4
[2026-11-04 13:33:48.601] [info] [graph][runtime.node] audio_stage_observation node=7 stage=aggregate_built count=11642 last_generation=1 last_begin=11920384 last_end=11921408 last_rate=44100 last_steady_ns=17798052949300 maximum_inter_event_gap_ns=46552800 gap_begin_generation=1 gap_begin_begin=8561664 gap_begin_end=8562688 gap_begin_rate=44100 gap_begin_steady_ns=17721891829000 gap_end_generation=1 gap_end_begin=8562688 gap_end_end=8563712 gap_end_rate=44100 gap_end_steady_ns=17721938381800
[2026-11-04 13:33:48.601] [info] [graph][runtime.node] audio_stage_observation node=7 stage=aggregate_committed count=11642 last_generation=1 last_begin=11920384 last_end=11921408 last_rate=44100 last_steady_ns=17798052965600 maximum_inter_event_gap_ns=47668700 gap_begin_generation=1 gap_begin_begin=8561664 gap_begin_end=8562688 gap_begin_rate=44100 gap_begin_steady_ns=17721892208200 gap_end_generation=1 gap_end_begin=8562688 gap_end_end=8563712 gap_end_rate=44100 gap_end_steady_ns=17721939876900
[2026-11-04 13:33:48.601] [info] [graph][runtime.node] continuous_audio_aggregate.summary reason=stop output_group=composition-r384.output audio_commits=11642 real_samples=10248192 silence_samples=1673216
[2026-11-04 13:33:48.601] [info] [graph][runtime.lifecycle] video_encode.zero_copy_summary drm_prime=0 software_frame=0
[2026-11-04 13:33:48.603] [info] [graph][runtime.lifecycle] codec_owner_release node=2 kind=VideoEncode owner_references=4
[2026-11-04 13:33:48.603] [info] [graph][runtime.node] video_encoder_execution_observation node=2 count=8110 maximum_call_ns=30779300 maximum_begin_steady_ns=17750484818100 maximum_end_steady_ns=17750515597400 maximum_encoder_pts=6682 maximum_native_result=0 maximum_between_submissions_ns=53362800 before_gap_begin_steady_ns=17721884312900 before_gap_end_steady_ns=17721887405200 before_gap_encoder_pts=5824 before_gap_native_result=0 after_gap_begin_steady_ns=17721940768000 after_gap_end_steady_ns=17721944181500 after_gap_encoder_pts=5825 after_gap_native_result=0
[2026-11-04 13:33:48.603] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=process count=46317 maximum_ns=30557400 begin_steady_ns=17734183732100 end_steady_ns=17734214289500 video_cursor=6193 audio_cursor=not_owned next_video_cursor=6193 next_audio_cursor=not_owned error=None return_state=progress deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-11-04 13:33:48.603] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=purge count=46317 maximum_ns=625100 begin_steady_ns=17620186242500 end_steady_ns=17620186867600 video_cursor=2774 audio_cursor=not_owned next_video_cursor=2774 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-04 13:33:48.603] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=bind count=46312 maximum_ns=546700 begin_steady_ns=17622150254400 end_steady_ns=17622150801100 video_cursor=2832 audio_cursor=not_owned next_video_cursor=2832 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-04 13:33:48.603] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=consume count=46312 maximum_ns=17726100 begin_steady_ns=17619119350700 end_steady_ns=17619137076800 video_cursor=2742 audio_cursor=not_owned next_video_cursor=2742 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-04 13:33:48.603] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=activate count=9 maximum_ns=187400 begin_steady_ns=17527749333400 end_steady_ns=17527749520800 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-04 13:33:48.603] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=build_video count=8111 maximum_ns=24035800 begin_steady_ns=17766683316300 end_steady_ns=17766707352100 video_cursor=7168 audio_cursor=not_owned next_video_cursor=7168 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-04 13:33:48.604] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=canvas count=8111 maximum_ns=24008800 begin_steady_ns=17766683321600 end_steady_ns=17766707330400 video_cursor=7168 audio_cursor=not_owned next_video_cursor=7168 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-04 13:33:48.604] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=emit count=8112 maximum_ns=28213600 begin_steady_ns=17734186074400 end_steady_ns=17734214288000 video_cursor=6193 audio_cursor=not_owned next_video_cursor=6193 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-04 13:33:48.604] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=between_calls_previous_return count=46316 maximum_ns=145845100 begin_steady_ns=17527598332200 end_steady_ns=17527744177300 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=waiting deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-11-04 13:33:48.604] [info] [graph][runtime.node] continuous_aggregate.summary reason=stop output_group=composition-r384.output output_generation=1 video_commits=8110 source[0].group=composition-r384.source.0 source[0].identity=composition-r384.source.0.input.video source[0].real_frames=3418 source[0].black_frames=4692 source[0].contribution_generation=0 source[1].group=composition-r384.source.1 source[1].identity=composition-r384.source.1.input.video source[1].real_frames=3418 source[1].black_frames=4692 source[1].contribution_generation=0 source[2].group=composition-r384.source.2 source[2].identity=composition-r384.source.2.input.video source[2].real_frames=6973 source[2].black_frames=1137 source[2].contribution_generation=0
[2026-11-04 13:33:48.604] [info] [graph][runtime.node] cuda_canvas_call stage=context_push count=6978 operation=copy maximum_ns=12928700 begin_steady_ns=17607083523200 end_steady_ns=17607096451900 native_result=0 thread_cpu_ns=0
[2026-11-04 13:33:48.604] [info] [graph][runtime.node] cuda_canvas_call stage=source_wait count=20791 operation=copy maximum_ns=1974500 begin_steady_ns=17696617056000 end_steady_ns=17696619030500 native_result=0 thread_cpu_ns=0
[2026-11-04 13:33:48.604] [info] [graph][runtime.node] cuda_canvas_call stage=plane_copy count=41584 operation=copy maximum_ns=8901200 begin_steady_ns=17721153928800 end_steady_ns=17721162830000 native_result=0 thread_cpu_ns=0
[2026-11-04 13:33:48.604] [info] [graph][runtime.node] cuda_canvas_call stage=destination_wait count=2 operation=upload maximum_ns=72900 begin_steady_ns=17527518774900 end_steady_ns=17527518847800 native_result=0 thread_cpu_ns=0
[2026-11-04 13:33:48.604] [info] [graph][runtime.node] cuda_canvas_call stage=context_pop count=6978 operation=copy maximum_ns=393900 begin_steady_ns=17721883878900 end_steady_ns=17721884272800 native_result=0 thread_cpu_ns=0
[2026-11-04 13:33:48.605] [info] [graph][runtime.node] cuda_canvas_call stage=batch_completion count=6976 operation=copy maximum_ns=23853400 begin_steady_ns=17766683457600 end_steady_ns=17766707311000 native_result=0 thread_cpu_ns=0
[2026-11-04 13:33:48.605] [info] [graph][runtime.node] canvas_compose_correlated scope=attempt begin_steady_ns=17766683322500 end_steady_ns=17766707330000 wall_ns=24007500 thread_cpu_ns=0 batch_pid=34096 batch_tid=39740 batch_context=2212687222560 batch_stream=2213117157648 batch_submit_begin_steady_ns=17766683327400 batch_submit_end_steady_ns=17766683457600 batch_wait_begin_steady_ns=17766683457800 batch_wait_end_steady_ns=17766707308900 batch_timing_preparation=ready batch_timing_preparation_native=0 batch_start_event_native=0 batch_end_event_native=0 batch_sync_native=0 batch_elapsed_native=0 batch_gpu_elapsed_available=1 batch_gpu_elapsed_ms=23.0605 batch_gpu_interval=event_execution_interval_excludes_pre_start_queue surface_count=1 surface_total_ns=600 surface_maximum_ns=600 surface_maximum_tile=none surface_maximum_result=unavailable validate_count=1 validate_total_ns=800 validate_maximum_ns=800 validate_maximum_tile=2 validate_maximum_result=0 black_count=1 black_total_ns=22100 black_maximum_ns=22100 black_maximum_tile=none black_maximum_result=0 tile_count=1 tile_total_ns=14300 tile_maximum_ns=14300 tile_maximum_tile=2 tile_maximum_result=0 publish_count=1 publish_total_ns=6100 publish_maximum_ns=6100 publish_maximum_tile=none publish_maximum_result=0 context_push_count=1 context_push_total_ns=500 context_push_maximum_ns=500 context_push_maximum_tile=none context_push_maximum_result=0 source_wait_count=2 source_wait_total_ns=900 source_wait_maximum_ns=900 source_wait_maximum_tile=2 source_wait_maximum_result=0 plane_copy_count=4 plane_copy_total_ns=30100 plane_copy_maximum_ns=15800 plane_copy_maximum_tile=none plane_copy_maximum_result=0 destination_wait_count=0 destination_wait_total_ns=0 destination_wait_maximum_ns=0 destination_wait_maximum_tile=none destination_wait_maximum_result=unavailable context_pop_count=1 context_pop_total_ns=3800 context_pop_maximum_ns=3800 context_pop_maximum_tile=none context_pop_maximum_result=0 batch_completion_count=1 batch_completion_total_ns=23851200 batch_completion_maximum_ns=23851200 batch_completion_maximum_tile=none batch_completion_maximum_result=0 source_query_completed_count=0 source_query_ready_count=1 source_query_ready_maximum_ns=2800 source_query_ready_begin_steady_ns=17766683439900 source_query_ready_end_steady_ns=17766683442700 source_query_ready_tile=2 source_query_ready_operation=event_query source_query_ready_native=0 source_query_ready_restore_native=0 source_query_pending_count=0 source_query_unavailable_count=0 source_query_error_count=0
[2026-11-04 13:33:48.605] [info] [graph][runtime.node] source_video_trace_summary node_id=67 node=VideoPreparationNode generation=2 reason=stop steady_ns=17798343698800 input_count=3569 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3569 published_count=3569 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=10 incomplete=0
[2026-11-04 13:33:48.605] [info] [graph][runtime.node] source_video_trace_summary node_id=44 node=VideoPreparationNode generation=1 reason=stop steady_ns=17798343827300 input_count=3437 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3437 published_count=3431 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=13 incomplete=0
[2026-11-04 13:33:48.605] [info] [graph][runtime.node] source_video_trace_summary node_id=25 node=VideoPreparationNode generation=1 reason=stop steady_ns=17798343958300 input_count=3432 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3432 published_count=3431 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-04 13:33:48.605] [info] [graph][runtime.lifecycle] codec_owner_release node=73 kind=AudioResample owner_references=1
[2026-11-04 13:33:48.606] [info] [graph][runtime.lifecycle] video_filter.zero_copy_summary drm_prime_input=0 drm_prime_output=0 rga=0 software_frame=0
[2026-11-04 13:33:48.606] [info] [graph][runtime.node] source_video_trace_summary node_id=68 node=VideoFilterNode generation=2 reason=stop steady_ns=17798344494700 input_count=3569 accepted_count=3569 would_block_count=0 resolved_count=3569 output_attempt_count=0 published_count=3569 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:33:48.613] [info] [graph][runtime.lifecycle] video_filter.zero_copy_summary drm_prime_input=0 drm_prime_output=0 rga=0 software_frame=0
[2026-11-04 13:33:48.613] [info] [graph][runtime.node] source_video_trace_summary node_id=45 node=VideoFilterNode generation=1 reason=stop steady_ns=17798352235300 input_count=3450 accepted_count=3450 would_block_count=0 resolved_count=3450 output_attempt_count=0 published_count=3450 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:33:48.616] [info] [graph][runtime.lifecycle] video_filter.zero_copy_summary drm_prime_input=0 drm_prime_output=0 rga=0 software_frame=0
[2026-11-04 13:33:48.616] [info] [graph][runtime.node] source_video_trace_summary node_id=26 node=VideoFilterNode generation=1 reason=stop steady_ns=17798355109100 input_count=3439 accepted_count=3439 would_block_count=0 resolved_count=3439 output_attempt_count=0 published_count=3438 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-04 13:33:48.623] [info] [graph][runtime.node] source_video_trace_summary node_id=66 node=VideoFrameRateNode generation=2 reason=stop steady_ns=17798362243500 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3569 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:33:48.624] [info] [graph][runtime.node] source_video_trace_summary node_id=43 node=VideoFrameRateNode generation=1 reason=stop steady_ns=17798362536200 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3450 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:33:48.624] [info] [graph][runtime.node] source_video_trace_summary node_id=24 node=VideoFrameRateNode generation=1 reason=stop steady_ns=17798362708500 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3445 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:33:48.624] [info] [graph][runtime.node] hardware_transfer.summary forwarded=3607 download=0 upload=0
[2026-11-04 13:33:48.624] [info] [graph][runtime.node] source_video_trace_summary node_id=65 node=HardwareTransferNode generation=2 reason=stop steady_ns=17798362865900 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3569 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:33:48.624] [info] [graph][runtime.node] hardware_transfer.summary forwarded=41 download=0 upload=0
[2026-11-04 13:33:48.624] [info] [graph][runtime.node] source_video_trace_summary node_id=42 node=HardwareTransferNode generation=1 reason=stop steady_ns=17798363003800 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3459 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:33:48.624] [info] [graph][runtime.node] hardware_transfer.summary forwarded=42 download=0 upload=0
[2026-11-04 13:33:48.624] [info] [graph][runtime.node] source_video_trace_summary node_id=23 node=HardwareTransferNode generation=1 reason=stop steady_ns=17798363137300 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3459 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:33:48.624] [info] [graph][runtime.lifecycle] codec_owner_release node=70 kind=AudioDecode owner_references=2
[2026-11-04 13:33:48.625] [info] [graph][runtime.lifecycle] video_decode.zero_copy_summary drm_prime=0 software_frame=0
[2026-11-04 13:33:48.625] [info] [graph][runtime.node] source_video_trace_summary node_id=64 node=VideoDecodeNode generation=2 reason=stop steady_ns=17798363455500 input_count=3569 accepted_count=3569 would_block_count=0 resolved_count=3569 output_attempt_count=0 published_count=3569 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=108 incomplete=0
[2026-11-04 13:33:48.625] [info] [graph][runtime.lifecycle] codec_owner_release node=64 kind=VideoDecode owner_references=2
[2026-11-04 13:33:48.640] [info] [graph][runtime.lifecycle] video_decode.zero_copy_summary drm_prime=0 software_frame=0
[2026-11-04 13:33:48.640] [info] [graph][runtime.node] source_video_trace_summary node_id=41 node=VideoDecodeNode generation=1 reason=stop steady_ns=17798378457100 input_count=3460 accepted_count=3460 would_block_count=0 resolved_count=3460 output_attempt_count=0 published_count=3459 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=109 incomplete=0
[2026-11-04 13:33:48.640] [info] [graph][runtime.lifecycle] codec_owner_release node=41 kind=VideoDecode owner_references=2
[2026-11-04 13:33:48.655] [info] [graph][runtime.lifecycle] video_decode.zero_copy_summary drm_prime=0 software_frame=0
[2026-11-04 13:33:48.656] [info] [graph][runtime.node] source_video_trace_summary node_id=22 node=VideoDecodeNode generation=1 reason=stop steady_ns=17798394345400 input_count=3460 accepted_count=3460 would_block_count=0 resolved_count=3460 output_attempt_count=0 published_count=3459 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=109 incomplete=0
[2026-11-04 13:33:48.656] [info] [graph][runtime.lifecycle] codec_owner_release node=22 kind=VideoDecode owner_references=2
[2026-11-04 13:33:48.670] [info] [graph][runtime.node] source_video_trace_summary node_id=62 node=MediaAvBoundReleaseExtractorNode generation=2 reason=stop steady_ns=17798408318600 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=3554 release_committed_count=3554 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:33:48.670] [info] [graph][runtime.node] source_video_trace_summary node_id=39 node=MediaAvBoundReleaseExtractorNode generation=1 reason=stop steady_ns=17798408609100 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=3571 release_committed_count=3555 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:33:48.670] [info] [graph][runtime.node] source_video_trace_summary node_id=20 node=MediaAvBoundReleaseExtractorNode generation=1 reason=stop steady_ns=17798408698800 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=3571 release_committed_count=3555 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:33:48.670] [info] [graph][runtime.node] source_video_trace_summary node_id=61 node=ActivationReleaseSequencer generation=2 reason=stop steady_ns=17798408783400 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3554 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:33:48.670] [info] [graph][runtime.node] source_video_trace_summary node_id=38 node=ActivationReleaseSequencer generation=1 reason=stop steady_ns=17798408871400 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3555 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:33:48.670] [info] [graph][runtime.node] source_video_trace_summary node_id=19 node=ActivationReleaseSequencer generation=1 reason=stop steady_ns=17798408958600 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3555 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 13:33:48.670] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-11-04 13:33:48.670] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-11-04 13:33:48.670] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-11-04 13:33:48.671] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=10292 completion_waits=10292 completed_datagrams=10462 drained_datagrams=182 would_block=17 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-04 13:33:48.671] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=115597 completion_waits=115597 completed_datagrams=212131 drained_datagrams=96546 would_block=17 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-04 13:33:48.671] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=76556 completion_waits=76556 completed_datagrams=105562 drained_datagrams=29083 would_block=77 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-04 13:33:48.672] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=77495 completion_waits=77495 completed_datagrams=105571 drained_datagrams=28151 would_block=77 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-04 13:33:48.672] [info] [graph][runtime.node] codec_resolver.release slot=encoder_parameters bufferReferences=1 hasCredit=0 creditReferences=0
[2026-11-04 13:33:48.673] [info] [graph][runtime.lifecycle] stop.done state=Stopped
[2026-11-04 13:33:48.750] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=retired detail="" description=""
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=919, threads=81, processThreads=129, workers=0, workerProgress=551799, workerProcessCalls=2051495, workerWaits=1499679, workerWakeups=1479089, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=863, logicalProcessors=22, averageSystemMachineCpuPercent=9.860235, averageProcessMachineCpuPercent=1.730000, peakProcessMachineCpuPercent=5.816135, averageProcessSingleCoreCpuPercent=38.060004, peakProcessSingleCoreCpuPercent=127.954972, initialWorkingSetBytes=209924096, workingSetBytes=259751936, peakWorkingSetBytes=269582336, totalPushed=476003, totalPopped=475967, droppedBuffers=0, encodedPacketsPushed=90656, encodedPacketsPopped=90656, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=16106896, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=745, graphPayloadReservations=220555, graphPayloadReleases=220555, graphPayloadPressureFailures=0, lastInputReceivedAtNs=17779018610400
[CLI] realtime video validation stopped successfully
```
### 漂移首尾
```text
[2026-11-04 13:29:18.010] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1 source_master_ns=5691354616 raw_phase_ns=2269 filtered_phase_ns=2269 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-11-04 13:29:18.011] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3 source_master_ns=5737794525 raw_phase_ns=2269 filtered_phase_ns=2269 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=45346
[2026-11-04 13:33:25.514] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4963 source_master_ns=253667462386 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5124098
[2026-11-04 13:33:26.534] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=5007 source_master_ns=254689140390 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5169444
[2026-11-04 13:33:27.581] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=5052 source_master_ns=255734038349 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5214790
[2026-11-04 13:33:28.601] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=5096 source_master_ns=256755716354 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5260136
```
### composition-r384-cli-error.log
```text
[in @ 000002034218ee00] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002034218ee00] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 11412.108144
[in @ 000002034278d600] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002034278d600] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 43600.705122
[in @ 000002034278e400] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002034278e400] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 16675.231278
[in @ 000002033deaf2c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002033deaf2c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 11412.108144
[in @ 000002033deaff80] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002033deaff80] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 43600.705122
[in @ 000002034239e900] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002034239e900] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 16675.231278
[aac @ 0000020342764d00] Qavg: 9718.746

```
### composition-r384-monitor.csv
```text
"Time","CpuPercent","WorkingSet","PrivateBytes"
"2026-11-04T13:29:13.5101568+08:00","0.3494569762884552","142626816","567914496"
"2026-11-04T13:29:14.5384360+08:00","0.06906949714895261","145403904","570040320"
"2026-11-04T13:29:15.5523947+08:00","0.21013497080125826","149094400","573796352"
"2026-11-04T13:29:16.5726061+08:00","0.0696156965828134","152182784","576647168"
"2026-11-04T13:29:17.5951226+08:00","0.6945875912293569","176656384","640339968"
"2026-11-04T13:29:18.6116290+08:00","3.772949459764614","252420096","1255432192"
"2026-11-04T13:29:19.6404277+08:00","2.968488658400591","253087744","1255628800"
"2026-11-04T13:29:20.6535595+08:00","2.7339842295310084","254062592","1256697856"
"2026-11-04T13:29:21.6725114+08:00","2.369859389116137","255913984","1256312832"
"2026-11-04T13:29:22.6855104+08:00","2.103340495086193","256049152","1256325120"
"2026-11-04T13:29:23.7036613+08:00","2.3019672231296955","256188416","1256325120"
"2026-11-04T13:29:24.7277969+08:00","3.467447439222271","258240512","1258352640"
"2026-11-04T13:29:25.7423885+08:00","2.100038890704219","258301952","1258352640"
"2026-11-04T13:29:26.7640934+08:00","2.3634737655390783","258306048","1258356736"
"2026-11-04T13:29:27.7758935+08:00","1.5442773725758672","258310144","1258356736"
"2026-11-04T13:29:28.7922462+08:00","1.3976000117425234","258772992","1258766336"
"2026-11-04T13:29:29.8132136+08:00","3.8260281376271177","261029888","1260707840"
"2026-11-04T13:29:30.8328391+08:00","2.2289823790472805","261033984","1260711936"
"2026-11-04T13:29:31.8490537+08:00","2.026795413989418","261214208","1260720128"
"2026-11-04T13:29:32.8665059+08:00","2.1639390483941607","261214208","1260720128"
"2026-11-04T13:29:33.8856638+08:00","2.857193981601691","261959680","1261469696"
"2026-11-04T13:29:34.9028332+08:00","2.3041884665425445","261218304","1260724224"
"2026-11-04T13:29:35.9179469+08:00","1.119444685224558","261218304","1260724224"
"2026-11-04T13:29:36.9384374+08:00","2.64467296497482","261509120","1260724224"
"2026-11-04T13:29:37.9518943+08:00","2.5929478689137238","261623808","1260724224"
"2026-11-04T13:29:38.9665719+08:00","1.6798887198706807","261623808","1260724224"
"2026-11-04T13:29:39.9803623+08:00","1.6813588435493714","261623808","1260724224"
"2026-11-04T13:29:41.0040050+08:00","1.3182644864089963","261632000","1260732416"
"2026-11-04T13:29:42.0209948+08:00","2.234759161524799","261689344","1260732416"
"2026-11-04T13:29:43.0402090+08:00","2.229881876378167","261787648","1260699648"
"2026-11-04T13:29:44.0562254+08:00","2.4466095769177096","261820416","1260699648"
"2026-11-04T13:29:45.0771866+08:00","1.9478079711906424","262635520","1261404160"
"2026-11-04T13:29:46.0954258+08:00","2.092516000348266","263667712","1262436352"
"2026-11-04T13:29:47.1134624+08:00","1.9534036041890472","263667712","1262436352"
"2026-11-04T13:29:48.1260256+08:00","2.5250949094517576","264536064","1263185920"
"2026-11-04T13:29:49.1444301+08:00","1.115827391143339","263790592","1262436352"
"2026-11-04T13:29:50.1572061+08:00","1.6830428984745438","262553600","1261199360"
"2026-11-04T13:29:51.1743658+08:00","2.2343858813195925","262553600","1261199360"
"2026-11-04T13:29:52.1880670+08:00","2.662385756634831","262553600","1261199360"
"2026-11-04T13:29:53.2062805+08:00","3.487614693417798","262750208","1261395968"
"2026-11-04T13:29:54.2188286+08:00","2.314704852046041","264421376","1263239168"
"2026-11-04T13:29:55.2336912+08:00","2.7293215491795277","264585216","1263239168"
"2026-11-04T13:29:56.2542973+08:00","2.01807444704582","264609792","1263271936"
"2026-11-04T13:29:57.2650044+08:00","1.756758394017596","264609792","1263271936"
"2026-11-04T13:29:58.2830269+08:00","1.6743691367778752","262950912","1261547520"
"2026-11-04T13:29:59.2910064+08:00","1.831972683066381","262938624","1261547520"
"2026-11-04T13:30:00.3065299+08:00","1.6784894239724186","263057408","1261547520"
"2026-11-04T13:30:01.3207836+08:00","2.170763138901584","263675904","1262317568"
"2026-11-04T13:30:02.3420537+08:00","2.086305883411076","264851456","1263370240"
"2026-11-04T13:30:03.3541429+08:00","1.9648825060442932","264884224","1263370240"
"2026-11-04T13:30:04.3672231+08:00","2.383594830175071","264888320","1263370240"
"2026-11-04T13:30:05.3752988+08:00","1.690890331495397","264888320","1263370240"
"2026-11-04T13:30:06.3988396+08:00","2.2204559629936322","264888320","1263370240"
"2026-11-04T13:30:07.4136905+08:00","2.799336425586351","264953856","1263370240"
"2026-11-04T13:30:08.4282491+08:00","1.4700750382750416","264945664","1263370240"
"2026-11-04T13:30:09.4448132+08:00","1.6767712479178187","264945664","1263370240"
"2026-11-04T13:30:10.4817456+08:00","1.7123278063431926","264945664","1263370240"
"2026-11-04T13:30:11.4971556+08:00","2.3081809318403406","265682944","1264111616"
"2026-11-04T13:30:12.5094187+08:00","1.8242203129709154","264900608","1263296512"
"2026-11-04T13:30:13.5219453+08:00","2.3147540025121316","264900608","1263296512"
"2026-11-04T13:30:14.5325834+08:00","2.8812804684306066","264900608","1263296512"
"2026-11-04T13:30:15.5417328+08:00","1.900227693108311","264900608","1263296512"
"2026-11-04T13:30:16.5549210+08:00","2.45343901019125","264896512","1263296512"
"2026-11-04T13:30:17.5746751+08:00","1.5322321332172137","265060352","1263296512"
"2026-11-04T13:30:18.5940633+08:00","2.5778608277895594","265056256","1263296512"
"2026-11-04T13:30:19.6128462+08:00","2.7188190571674924","266579968","1264877568"
"2026-11-04T13:30:20.6337825+08:00","1.9478554770129768","266592256","1264877568"
"2026-11-04T13:30:21.6530950+08:00","1.4632188585220653","266592256","1264877568"
"2026-11-04T13:30:22.6659770+08:00","1.6127473163435893","266592256","1264877568"
"2026-11-04T13:30:23.6782509+08:00","1.824200850274722","265428992","1263652864"
"2026-11-04T13:30:24.6940154+08:00","3.0065800416605155","265433088","1263652864"
"2026-11-04T13:30:25.7051734+08:00","2.669082019193475","265433088","1263652864"
"2026-11-04T13:30:26.7243703+08:00","2.160234735265134","265506816","1263661056"
"2026-11-04T13:30:27.7351581+08:00","2.880853744160563","266158080","1264283648"
"2026-11-04T13:30:28.7490701+08:00","2.0313982780646556","266178560","1264283648"
"2026-11-04T13:30:29.7633115+08:00","2.240814930969366","266350592","1264283648"
"2026-11-04T13:30:30.7781488+08:00","2.379467848957392","266350592","1264283648"
"2026-11-04T13:30:31.7950775+08:00","2.095212592762716","266350592","1264283648"
"2026-11-04T13:30:32.8012973+08:00","2.2586787426835295","266350592","1264283648"
"2026-11-04T13:30:33.8218581+08:00","1.3918372579610596","266350592","1264283648"
"2026-11-04T13:30:34.8345081+08:00","1.6832523127886778","266350592","1264283648"
"2026-11-04T13:30:35.8526449+08:00","1.81369626271333","267055104","1265012736"
"2026-11-04T13:30:36.8635865+08:00","2.2481291428973473","267137024","1265012736"
"2026-11-04T13:30:37.8808457+08:00","1.6058077698119881","266047488","1263927296"
"2026-11-04T13:30:38.8893380+08:00","2.3240137777948333","266117120","1263927296"
"2026-11-04T13:30:39.8980777+08:00","1.8305920834591014","266117120","1263927296"
"2026-11-04T13:30:40.9150565+08:00","2.0252723959526895","266174464","1263927296"
"2026-11-04T13:30:41.9267068+08:00","1.8955301415554728","266174464","1263927296"
"2026-11-04T13:30:42.9498567+08:00","1.8742255033828734","266231808","1263927296"
"2026-11-04T13:30:43.9678927+08:00","1.953404755466765","267206656","1264947200"
"2026-11-04T13:30:44.9773605+08:00","2.3921245702663594","267448320","1265168384"
"2026-11-04T13:30:45.9883178+08:00","2.9506236766424707","267509760","1265168384"
"2026-11-04T13:30:47.0031992+08:00","2.3793644531003597","267505664","1265168384"
"2026-11-04T13:30:48.0257676+08:00","2.5003884158929437","267505664","1265168384"
"2026-11-04T13:30:49.0472374+08:00","1.3905986701266602","267513856","1265168384"
"2026-11-04T13:30:50.0648712+08:00","1.9541768007669986","267579392","1265168384"
"2026-11-04T13:30:51.0888193+08:00","1.5259562471965133","267694080","1265168384"
"2026-11-04T13:30:52.0977425+08:00","1.9006537230620097","269574144","1267109888"
"2026-11-04T13:30:53.1091205+08:00","2.036487931227583","269574144","1267109888"
"2026-11-04T13:30:54.1208948+08:00","1.6145129672425236","269582336","1267118080"
"2026-11-04T13:30:55.1387436+08:00","2.5119823119290228","268500992","1266032640"
"2026-11-04T13:30:56.1512503+08:00","2.1043631792084123","266653696","1264107520"
"2026-11-04T13:30:57.1595765+08:00","1.620034000180425","266653696","1264107520"
"2026-11-04T13:30:58.1730532+08:00","2.3826623022243405","266653696","1264107520"
"2026-11-04T13:30:59.1838085+08:00","1.826941604056797","266657792","1264111616"
"2026-11-04T13:31:00.1999134+08:00","2.8657787381812825","269549568","1267105792"
"2026-11-04T13:31:01.2222791+08:00","2.2230081395798713","269557760","1267113984"
"2026-11-04T13:31:02.2281445+08:00","1.6239973333139077","269561856","1267113984"
"2026-11-04T13:31:03.2383240+08:00","2.0389040669594767","269561856","1267113984"
"2026-11-04T13:31:04.2610688+08:00","3.8888222431174695","268464128","1265934336"
"2026-11-04T13:31:05.2841200+08:00","2.6380533411853055","268464128","1265934336"
"2026-11-04T13:31:06.2988608+08:00","1.6797840931846386","268476416","1265934336"
"2026-11-04T13:31:07.3067380+08:00","2.114029187466308","268480512","1265934336"
"2026-11-04T13:31:08.3344242+08:00","2.142389909930235","268480512","1265934336"
"2026-11-04T13:31:09.3482815+08:00","2.311715859815775","268480512","1265934336"
"2026-11-04T13:31:10.3575236+08:00","1.7593084769434228","268484608","1265934336"
"2026-11-04T13:31:11.3661735+08:00","1.5491004361374545","268484608","1265934336"
"2026-11-04T13:31:12.3865076+08:00","2.157827073950136","243879936","1245569024"
"2026-11-04T13:31:13.3961520+08:00","1.4068859743633952","243879936","1245569024"
"2026-11-04T13:31:14.4062914+08:00","1.3358867282890048","243884032","1245569024"
"2026-11-04T13:31:15.4296496+08:00","1.2492293420906686","243884032","1245569024"
"2026-11-04T13:31:16.4454936+08:00","1.3982998821222012","243884032","1245614080"
"2026-11-04T13:31:17.4623474+08:00","0.8381467692530896","243884032","1245614080"
"2026-11-04T13:31:18.4864220+08:00","1.6644739109293938","243884032","1245614080"
"2026-11-04T13:31:19.5006582+08:00","1.2604648610541518","243884032","1245614080"
"2026-11-04T13:31:20.5133642+08:00","1.3325010597170532","243884032","1245614080"
"2026-11-04T13:31:21.5306123+08:00","1.3265513282175885","243884032","1245614080"
"2026-11-04T13:31:22.5540977+08:00","1.0408950719677184","243884032","1245614080"
"2026-11-04T13:31:23.5733989+08:00","1.3238793579187567","243892224","1245614080"
"2026-11-04T13:31:24.5865741+08:00","1.6122806078087257","243884032","1245614080"
"2026-11-04T13:31:25.6060457+08:00","1.1843256483421056","243884032","1245614080"
"2026-11-04T13:31:26.6238165+08:00","1.1165221446357434","243884032","1245614080"
"2026-11-04T13:31:27.6427529+08:00","0.9061364914880403","243884032","1245614080"
"2026-11-04T13:31:28.6628711+08:00","0.905086738522511","243884032","1245614080"
"2026-11-04T13:31:29.6832118+08:00","1.6705649931885054","243888128","1245614080"
"2026-11-04T13:31:30.6963174+08:00","1.4721834256244093","247861248","1248993280"
"2026-11-04T13:31:31.7058370+08:00","1.6884718776588932","252203008","1252655104"
"2026-11-04T13:31:32.7229682+08:00","1.3965303054852172","253272064","1253584896"
"2026-11-04T13:31:33.7330089+08:00","1.4063339679822262","254005248","1254010880"
"2026-11-04T13:31:34.7450262+08:00","1.7544840209927062","254652416","1254334464"
"2026-11-04T13:31:35.7679345+08:00","1.52750740217867","254832640","1254465536"
"2026-11-04T13:31:36.7773749+08:00","1.0553777212512092","255225856","1254477824"
"2026-11-04T13:31:37.7876272+08:00","1.757549259544553","255545344","1254686720"
"2026-11-04T13:31:38.7961411+08:00","1.8310019416598116","256036864","1254998016"
"2026-11-04T13:31:39.8117821+08:00","1.7482242069965488","256966656","1255940096"
"2026-11-04T13:31:40.8347029+08:00","1.666351348555484","256626688","1255550976"
"2026-11-04T13:31:41.8447140+08:00","1.2657376645752614","256724992","1255567360"
"2026-11-04T13:31:42.8623306+08:00","1.1166913318470202","256999424","1255739392"
"2026-11-04T13:31:43.8827404+08:00","1.3920432217081269","257085440","1255739392"
"2026-11-04T13:31:44.8921084+08:00","1.5479983514436755","257093632","1255739392"
"2026-11-04T13:31:45.9100529+08:00","1.4651852559027263","257687552","1256226816"
"2026-11-04T13:31:46.9210135+08:00","0.8430325843289317","257949696","1256312832"
"2026-11-04T13:31:47.9315623+08:00","0.9136574646820168","257970176","1256312832"
"2026-11-04T13:31:48.9493479+08:00","1.4654140053929559","258084864","1256497152"
"2026-11-04T13:31:49.9593639+08:00","1.0547762699708807","258879488","1257295872"
"2026-11-04T13:31:50.9678787+08:00","1.4084617751316546","258338816","1256554496"
"2026-11-04T13:31:51.9885833+08:00","1.4612232302345582","258342912","1256554496"
"2026-11-04T13:31:53.0020860+08:00","1.8219891363791225","258445312","1256656896"
"2026-11-04T13:31:54.0222561+08:00","2.2277924757128957","258449408","1256656896"
"2026-11-04T13:31:55.0371448+08:00","2.169404926327927","259579904","1257795584"
"2026-11-04T13:31:56.0451459+08:00","1.5500975147745373","258789376","1256996864"
"2026-11-04T13:31:57.0588489+08:00","1.5413784905440744","258846720","1256996864"
"2026-11-04T13:31:58.0711494+08:00","1.6838334610577141","258887680","1256996864"
"2026-11-04T13:31:59.0840333+08:00","1.3322670230831175","258904064","1256996864"
"2026-11-04T13:32:00.0909195+08:00","0.8464439449788141","259784704","1257795584"
"2026-11-04T13:32:01.1121779+08:00","1.738608154232251","259002368","1256996864"
"2026-11-04T13:32:02.1240425+08:00","1.1932291767459438","259002368","1256996864"
"2026-11-04T13:32:03.1412873+08:00","1.5360117839874925","259022848","1257009152"
"2026-11-04T13:32:04.1595488+08:00","1.74372514508128","259174400","1257009152"
"2026-11-04T13:32:05.1732391+08:00","1.541397801675719","259174400","1257009152"
"2026-11-04T13:32:06.1840633+08:00","1.545768294823175","259174400","1257009152"
"2026-11-04T13:32:07.2008429+08:00","1.4668638835075691","259178496","1257009152"
"2026-11-04T13:32:08.2143243+08:00","1.6818714724764108","259182592","1257009152"
"2026-11-04T13:32:09.2309417+08:00","1.7465451425661038","259178496","1257009152"
"2026-11-04T13:32:10.2512280+08:00","1.1137693766579404","259178496","1257009152"
"2026-11-04T13:32:11.2650557+08:00","1.6112429432266717","259198976","1257271296"
"2026-11-04T13:32:12.2843508+08:00","1.3238872807117568","259444736","1257271296"
"2026-11-04T13:32:13.3010553+08:00","1.117693131449341","259444736","1257271296"
"2026-11-04T13:32:14.3117981+08:00","1.2648213679178233","259444736","1257271296"
"2026-11-04T13:32:15.3295046+08:00","1.8842501608898403","259444736","1257271296"
"2026-11-04T13:32:16.3423614+08:00","1.402423862341197","259444736","1257271296"
"2026-11-04T13:32:17.3577053+08:00","2.378280627157683","259444736","1257271296"
"2026-11-04T13:32:18.3781358+08:00","2.7840299666749386","259444736","1257271296"
"2026-11-04T13:32:19.3962605+08:00","1.5346843073348482","259641344","1257271296"
"2026-11-04T13:32:20.4028019+08:00","1.622906645740282","259649536","1257271296"
"2026-11-04T13:32:21.4180074+08:00","1.399179324239817","259653632","1257271296"
"2026-11-04T13:32:22.4343118+08:00","1.9567330060131232","259710976","1257271296"
"2026-11-04T13:32:23.4525733+08:00","1.8832231566877824","259710976","1257730048"
"2026-11-04T13:32:24.4706775+08:00","1.534715208914765","259710976","1257730048"
"2026-11-04T13:32:25.4904632+08:00","1.114316112065149","259710976","1257730048"
"2026-11-04T13:32:26.5093903+08:00","2.1608067402020668","259710976","1257730048"
"2026-11-04T13:32:27.5224240+08:00","1.191852120651429","259719168","1257730048"
"2026-11-04T13:32:28.5344608+08:00","1.052670129278806","259821568","1257828352"
"2026-11-04T13:32:29.5538933+08:00","1.6023843925642232","259817472","1257828352"
"2026-11-04T13:32:30.5714285+08:00","1.1865794555671034","259817472","1257828352"
"2026-11-04T13:32:31.5885161+08:00","1.6759082054932677","260759552","1258774528"
"2026-11-04T13:32:32.6020729+08:00","2.032110179625938","260046848","1258000384"
"2026-11-04T13:32:33.6184928+08:00","1.18788146870832","260026368","1257967616"
"2026-11-04T13:32:34.6321332+08:00","1.2612057401314023","260026368","1257967616"
"2026-11-04T13:32:35.6476364+08:00","1.2588922328448506","260087808","1257967616"
"2026-11-04T13:32:36.6665097+08:00","1.8820923429474854","260087808","1257967616"
"2026-11-04T13:32:37.6853340+08:00","1.6033409561125773","260087808","1257967616"
"2026-11-04T13:32:38.7093038+08:00","1.4565637313983997","260087808","1257967616"
"2026-11-04T13:32:39.7281195+08:00","1.3245102310278671","260091904","1257967616"
"2026-11-04T13:32:40.7480488+08:00","1.880143688747481","260087808","1257967616"
"2026-11-04T13:32:41.7669004+08:00","1.53358938632476","260087808","1257967616"
"2026-11-04T13:32:42.7854363+08:00","1.8129855895024505","260087808","1257967616"
"2026-11-04T13:32:43.8005552+08:00","1.469263622938429","260087808","1257967616"
"2026-11-04T13:32:44.8164709+08:00","1.3282911349650546","261144576","1259036672"
"2026-11-04T13:32:45.8361236+08:00","1.8806537131354986","260374528","1258262528"
"2026-11-04T13:32:46.8563953+08:00","1.1137853146016268","260423680","1258262528"
"2026-11-04T13:32:47.8789779+08:00","1.6669024629848528","260423680","1258262528"
"2026-11-04T13:32:48.9009924+08:00","0.8339145161567935","260423680","1258262528"
"2026-11-04T13:32:49.9172792+08:00","1.7471132969730412","260423680","1258262528"
"2026-11-04T13:32:50.9377302+08:00","1.67038442271648","260423680","1258262528"
"2026-11-04T13:32:51.9536751+08:00","1.3981610079981162","260423680","1258262528"
"2026-11-04T13:32:52.9757120+08:00","1.528809771936806","260423680","1258262528"
"2026-11-04T13:32:53.9955328+08:00","1.4624895596631022","260431872","1258262528"
"2026-11-04T13:32:55.0032823+08:00","2.74858619491884","260423680","1258262528"
"2026-11-04T13:32:56.0213677+08:00","2.092832112298063","260448256","1258295296"
"2026-11-04T13:32:57.0419828+08:00","1.1134105662003593","260448256","1258295296"
"2026-11-04T13:32:58.0624877+08:00","1.3223178234438837","260448256","1258295296"
"2026-11-04T13:32:59.0799579+08:00","1.4658682610333678","260448256","1258295296"
"2026-11-04T13:33:00.0965316+08:00","1.1178369422341305","260448256","1258295296"
"2026-11-04T13:33:01.1191238+08:00","1.3890723452169353","260751360","1258545152"
"2026-11-04T13:33:02.1414221+08:00","1.3894716888940783","261529600","1259319296"
"2026-11-04T13:33:03.1592418+08:00","1.8840405981173645","260759552","1258545152"
"2026-11-04T13:33:04.1675728+08:00","0.8452311069209686","260784128","1258545152"
"2026-11-04T13:33:05.1899845+08:00","1.4587834555563801","260788224","1258545152"
"2026-11-04T13:33:06.2079542+08:00","1.534917984297568","260796416","1258545152"
"2026-11-04T13:33:07.2279848+08:00","1.2533046468498996","260796416","1258545152"
"2026-11-04T13:33:08.2483762+08:00","1.5312751557882591","260796416","1258545152"
"2026-11-04T13:33:09.2577279+08:00","1.3369292568505289","260800512","1258545152"
"2026-11-04T13:33:10.2747428+08:00","1.466524504928367","260923392","1258545152"
"2026-11-04T13:33:11.2956121+08:00","1.7392708173496665","260923392","1258545152"
"2026-11-04T13:33:12.3044351+08:00","1.7600393545926112","260931584","1258545152"
"2026-11-04T13:33:13.3199920+08:00","1.3986951843412667","260927488","1258545152"
"2026-11-04T13:33:14.3345249+08:00","1.1900908917161421","260927488","1258545152"
"2026-11-04T13:33:15.3518985+08:00","1.3961975673976064","260927488","1258545152"
"2026-11-04T13:33:16.3721769+08:00","0.9745557504874962","260931584","1258545152"
"2026-11-04T13:33:17.3921752+08:00","1.7407560206896244","260927488","1258545152"
"2026-11-04T13:33:18.4141489+08:00","1.042434760396387","260927488","1258545152"
"2026-11-04T13:33:19.4373399+08:00","1.3188464501562447","260927488","1258545152"
"2026-11-04T13:33:20.4532667+08:00","0.9787301425832865","260927488","1258545152"
"2026-11-04T13:33:21.4756175+08:00","1.5283403700569316","260927488","1258545152"
"2026-11-04T13:33:22.4939433+08:00","1.4646366346873199","260927488","1258545152"
"2026-11-04T13:33:23.5014256+08:00","1.2689146905202116","260927488","1258545152"
"2026-11-04T13:33:24.5220079+08:00","1.9485311117352941","260927488","1258545152"
"2026-11-04T13:33:25.5415436+08:00","1.8112076988485142","260927488","1258545152"
"2026-11-04T13:33:26.5648460+08:00","1.4575137053595035","260927488","1258545152"
"2026-11-04T13:33:27.5833553+08:00","1.185444613648951","260927488","1258545152"
"2026-11-04T13:33:28.6003141+08:00","1.396767052366866","260927488","1258545152"
"2026-11-04T13:33:29.6172058+08:00","1.047644413943893","259751936","1257369600"
"2026-11-04T13:33:30.6270571+08:00","1.1956080698577738","259751936","1257369600"
"2026-11-04T13:33:31.6348377+08:00","1.1275903072192859","259751936","1257369600"
"2026-11-04T13:33:32.6561588+08:00","0.8344806812203598","259751936","1257369600"
"2026-11-04T13:33:33.6744398+08:00","0.9764673816148801","260497408","1258119168"
"2026-11-04T13:33:34.6978019+08:00","1.179823215689113","259751936","1257369600"
"2026-11-04T13:33:35.7168709+08:00","1.1150997983096693","259751936","1257369600"
"2026-11-04T13:33:36.7363062+08:00","1.0450304291904635","259751936","1257369600"
"2026-11-04T13:33:37.7605293+08:00","0.9014593154025277","260497408","1258119168"
"2026-11-04T13:33:38.7791988+08:00","1.0458160464124127","259751936","1257369600"
"2026-11-04T13:33:39.7876029+08:00","0.915600655080096","259751936","1257369600"
"2026-11-04T13:33:40.8079015+08:00","0.9745364561101837","259751936","1257369600"
"2026-11-04T13:33:41.8270783+08:00","1.0452954866034128","259751936","1257369600"
"2026-11-04T13:33:42.8487229+08:00","1.042770557482425","259751936","1257369600"
"2026-11-04T13:33:43.8556503+08:00","0.9169434206929463","259751936","1257369600"
"2026-11-04T13:33:44.8756733+08:00","1.044428320823069","259751936","1257369600"
"2026-11-04T13:33:45.9015518+08:00","1.1769292013005084","259751936","1257369600"
"2026-11-04T13:33:46.9221376+08:00","1.4613933220776465","259751936","1257369600"
"2026-11-04T13:33:47.9303222+08:00","1.127138458932656","259751936","1257369600"

```
### composition-r384-vlc.log
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
file debug: opening logfile `D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r384-vlc.log'
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
main debug: stream filter added to 0109e660
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
main debug: `udp://@127.0.0.1:62024' gives access `udp' demux `any' path `@127.0.0.1:62024'
qt debug: IM: Setting an input
main debug: creating demux: access='udp' demux='any' location='@127.0.0.1:62024' file='\\@127.0.0.1:62024'
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
main debug: using access module "udp"
main debug: looking for stream_filter module matching "prefetch,cache_block": 24 candidates
prefetch debug: using 16777216 bytes buffer, 16777216 bytes read
main debug: using stream_filter module "prefetch"
main debug: looking for stream_filter module matching "any": 24 candidates
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
main debug: resized to 75743744x55484384
main debug: resized to 75743744x55484384
main debug: resized to 75743232x55484384
main debug: resized to 75743232x55484384
main debug: resized to 75742976x55484384
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
main debug: Buffering 19%
direct3d11 debug: creating Vout EventThread
direct3d11 debug: Win32VoutCreateWindow
main debug: VoutDisplayEvent 'resize' 1280x720
qt debug: Logical video size: 1280x720
main debug: resized to 1280x720
main debug: VoutDisplayEvent 'resize' 1280x720
direct3d11 debug: inclinometer not found. (hr=0x80070490)
direct3d11 debug: created video sub-window
direct3d11 debug: Vout EventThread running
main debug: Buffering 26%
wasapi debug: modified format
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
direct3d11 debug: using GPU render fence
direct3d11 debug: Direct3D11 resources created
direct3d11 debug: Direct3D11 device adapter successfully initialized
direct3d11 debug: Direct3D11 Open Succeeded
main debug: using vout display module "direct3d11"
direct3d11 debug: D3D11 pool succeed with 48 surfaces (1280x736) context 0x0aad00e0
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
qt debug: Logical video size: 1280x720
main debug: resized to 1280x720
main debug: VoutDisplayEvent 'resize' 1280x720
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
d3d11va debug: ID3D11VideoDecoderOutputView succeed with 28 surfaces (1280x768)
d3d11va debug: we got 2 decoder configurations
d3d11va debug: configuration[0] ConfigBitstreamRaw 1
d3d11va debug: configuration[1] ConfigBitstreamRaw 1
d3d11va debug: DxCreateDecoderSurfaces succeed
main debug: using hw decoder module "d3d11va"
main debug: Buffering 39%
avcodec: Using D3D11VA (NVIDIA GeForce RTX 4060 Laptop GPU, vendor 10de(NVIDIA), device 28e0, revision a1) for hardware decoding
main debug: Received first picture
main debug: Buffering 46%
main debug: Buffering 53%
main debug: Buffering 59%
main debug: Buffering 66%
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
main debug: Buffering 73%
main debug: Buffering 79%
main debug: Buffering 86%
main debug: Buffering 93%
main debug: Buffering 99%
main debug: Stream buffering done (1066 ms in 936 ms)
main debug: Decoder wait done in 0 ms
main debug: inserting 9594 zeroes
mmdevice debug: state changed: 1
main warning: playback too late (80763): up-sampling
main debug: auto hiding mouse cursor
main warning: picture is too late to be displayed (missing 35 ms)
main debug: picture might be displayed late (missing 2 ms)
main debug: accepted socket 3212 (from socket 1172)
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
main debug: Filter 'd3d11_filters' (04acb838) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (04acbae0) appended to chain
main debug: using video converter module "chain"
main debug: Filter 'chain' (04acc580) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: RV24 -> 1280x720 (1280x720) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (04acb040) appended to chain
main debug: using video converter module "chain"
main debug: removing module "png"
main debug: removing module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 04acb838 removed from chain
main debug: removing module "swscale"
main debug: Filter 04acbae0 removed from chain
main debug: Filter 04acc580 removed from chain
main debug: removing module "swscale"
main debug: Filter 04acb040 removed from chain
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r384-native-2026-11-04-13h29m31s429.png)
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (04acaaf0) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: YUVA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (04acc030) appended to chain
main debug: using video converter module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 04acaaf0 removed from chain
main debug: removing module "swscale"
main debug: Filter 04acc030 removed from chain
main error: read error: No error
main debug: resampling stopped (drift: -10341 us)
main debug: accepted socket 3996 (from socket 1172)
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
main debug: Filter 'd3d11_filters' (04acb2e8) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (04aca848) appended to chain
main debug: using video converter module "chain"
main debug: Filter 'chain' (04acb040) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: RV24 -> 1280x720 (1280x720) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (04acb590) appended to chain
main debug: using video converter module "chain"
main debug: removing module "png"
main debug: removing module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 04acb2e8 removed from chain
main debug: removing module "swscale"
main debug: Filter 04aca848 removed from chain
main debug: Filter 04acb040 removed from chain
main debug: removing module "swscale"
main debug: Filter 04acb590 removed from chain
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r384-native-2026-11-04-13h31m17s337.png)
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (04acb838) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: YUVA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (04acc580) appended to chain
main debug: using video converter module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 04acb838 removed from chain
main debug: removing module "swscale"
main debug: Filter 04acc580 removed from chain
main debug: accepted socket 3728 (from socket 1172)
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
main debug: Filter 'd3d11_filters' (1b1be468) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (1b1bd1d0) appended to chain
main debug: using video converter module "chain"
main debug: Filter 'chain' (1b1bdf18) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: RV24 -> 1280x720 (1280x720) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (1b1be1c0) appended to chain
main debug: using video converter module "chain"
main debug: removing module "png"
main debug: removing module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 1b1be468 removed from chain
main debug: removing module "swscale"
main debug: Filter 1b1bd1d0 removed from chain
main debug: Filter 1b1bdf18 removed from chain
main debug: removing module "swscale"
main debug: Filter 1b1be1c0 removed from chain
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r384-native-2026-11-04-13h31m49s511.png)
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (1b1bc730) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: YUVA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (1b1bd9c8) appended to chain
main debug: using video converter module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 1b1bc730 removed from chain
main debug: removing module "swscale"
main debug: Filter 1b1bd9c8 removed from chain
main debug: accepted socket 1064 (from socket 1172)
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
main debug: Filter 'd3d11_filters' (1af6dea0) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (1af6f138) appended to chain
main debug: using video converter module "chain"
main debug: Filter 'chain' (1af6dbf8) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: RV24 -> 1280x720 (1280x720) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (1af6e940) appended to chain
main debug: using video converter module "chain"
main debug: removing module "png"
main debug: removing module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 1af6dea0 removed from chain
main debug: removing module "swscale"
main debug: Filter 1af6f138 removed from chain
main debug: Filter 1af6dbf8 removed from chain
main debug: removing module "swscale"
main debug: Filter 1af6e940 removed from chain
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r384-native-2026-11-04-13h33m35s802.png)
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (1af6dea0) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: YUVA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (1af6dbf8) appended to chain
main debug: using video converter module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 1af6dea0 removed from chain
main debug: removing module "swscale"
main debug: Filter 1af6dbf8 removed from chain
main debug: accepted socket 1064 (from socket 1172)
main debug: exiting
main debug: exiting
main debug: no exit handler
main debug: removing all interfaces
main debug: removing module "qt"
main debug: deactivating the playlist
main debug: incoming request - stopping current input
prefetch debug: end of stream
ts debug: Can't read TS packet at 203415248
main debug: EOF reached
main error: read error: No error
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
main debug: saving media library to file C:\Users\ROG\AppData\Roaming\vlc\ml.xspf.tmp2688
main debug: looking for playlist export module matching "export-xspf": 4 candidates
main debug: using playlist export module "export"
main debug: removing module "export"
main debug: deleting item `Media Library'
main debug: deleting item `udp://127.0.0.1:62024'
main debug: deleting item `Playlist'
main debug: removing module "memory"
-- logger module stopped --

```
### composition-r384-source0-stdout.log
```text
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


```
### composition-r384-source1-stdout.log
```text
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


```
### composition-r384-source2-stdout.log
```text
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


```
### composition-r384-reentry-stdout.log
```text
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


```
### source0结束证据
```text
frame= 3574 fps= 30 q=-1.0 size=  118047KiB time=00:01:59.13 bitrate=8117.3kbits/s speed=   1x elapsed=0:01:58.63    
frame= 3590 fps= 30 q=-1.0 size=  118545KiB time=00:01:59.66 bitrate=8115.2kbits/s speed=   1x elapsed=0:01:59.15    
[out#0/rtp @ 000001f9e64bd180] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
[out#1/rtp @ 000001f9e82adc40] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
```
### source1结束证据
```text
frame= 3558 fps= 30 q=-1.0 size=  117493KiB time=00:01:58.60 bitrate=8115.6kbits/s speed=   1x elapsed=0:01:58.09    
frame= 3574 fps= 30 q=-1.0 size=  118047KiB time=00:01:59.13 bitrate=8117.3kbits/s speed=   1x elapsed=0:01:58.62    
frame= 3590 fps= 30 q=-1.0 size=  118545KiB time=00:01:59.66 bitrate=8115.2kbits/s speed=   1x elapsed=0:01:59.15    
[out#0/rtp @ 00000248c0d4c6c0] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.47    
```
### source2结束证据
```text
frame= 3559 fps= 30 q=-1.0 size=  117543KiB time=00:01:58.63 bitrate=8116.7kbits/s speed=   1x elapsed=0:01:58.12    
frame= 3575 fps= 30 q=-1.0 size=  118060KiB time=00:01:59.16 bitrate=8115.9kbits/s speed=   1x elapsed=0:01:58.64    
frame= 3590 fps= 30 q=-1.0 size=  118545KiB time=00:01:59.66 bitrate=8115.2kbits/s speed=   1x elapsed=0:01:59.16    
[out#0/rtp @ 000001cd0371b5c0] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.47    
```
### reentry结束证据
```text
frame= 3574 fps= 30 q=-1.0 size=  118047KiB time=00:01:59.13 bitrate=8117.3kbits/s speed=   1x elapsed=0:01:58.60    
frame= 3588 fps= 30 q=-1.0 size=  118513KiB time=00:01:59.60 bitrate=8117.1kbits/s speed=   1x elapsed=0:01:59.11    
[out#0/rtp @ 0000016fbe04e080] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
[out#1/rtp @ 0000016fbe0dce80] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.49    
```
### 原始产物清单
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r384-cli-error.log | 1967 bytes
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r384-cli.log | 1163455 bytes
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r384-monitor.csv | 22448 bytes
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r384-native-2026-11-04-13h29m31s429.png | 739663 bytes
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r384-native-2026-11-04-13h31m17s337.png | 12152 bytes
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r384-native-2026-11-04-13h31m49s511.png | 326464 bytes
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r384-native-2026-11-04-13h33m35s802.png | 12152 bytes
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r384-reentry-stdout.log | 540 bytes
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r384-reentry.log | 29521 bytes
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r384-source0-stdout.log | 540 bytes
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r384-source0.log | 29520 bytes
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r384-source1-stdout.log | 335 bytes
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r384-source1.log | 28926 bytes
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r384-source2-stdout.log | 335 bytes
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r384-source2.log | 28926 bytes
- D:\Code\MyCode\MediaTranscode\out\acceptance\composition-r384-vlc.log | 40445 bytes

## 清理核验
归档完成后按清单逐项核对长度并删除16项原始产物，r384残留0；六个测试PID均不存在，UDP62024/TCP63020无监听。复用源仍为124427809字节。本轮无远程测试产物。
