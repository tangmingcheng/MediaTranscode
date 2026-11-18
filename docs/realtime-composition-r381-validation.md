# r381：共同源规划与 worker 执行契约回归

日期：2026-11-04。结果：完成本轮三格→全失源黑屏→所选源重入→再次全黑及 caller-stop 观察，未达到完整合屏验收 PASS。

共同源规划7文件与worker/cleanup43文件均双独立源码PASS；Release全量789目标，session83850配置/构建退出0。realtime产物7006720字节，2026-11-04 11:20:45。原各轮审查FAIL仍保留。当前仍走CanonicalIntervals，不代表SourcePresentation及完整联合预算已完成。

## 实际启动与监控命令
```powershell
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r381-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r381-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r381-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r381-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r381-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r381-source2.log' -PassThru
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r381 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r381-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r381-cli-error.log' -PassThru
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r381-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r381-native- --snapshot-format=png udp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
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
$samples | Export-Csv -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r381-monitor.csv' -NoTypeInformation -Encoding UTF8
$source0.WaitForExit()
$source1.WaitForExit()
$source2.WaitForExit()
Write-Output "SOURCE0_EXIT=$($source0.ExitCode) SOURCE1_EXIT=$($source1.ExitCode) SOURCE2_EXIT=$($source2.ExitCode)"


```

初始PID：source0=19016，source1=34948，source2=12224，CLI=25140，VLC=36284。启动前端口/文件前缀无冲突；UDP62024/TCP63020均由VLC36284持有。启动会话16119最终CLI_EXIT=0，三初始源EXIT=0。

## 原生快照与重入
四张VLC原生快照均已实际查看，并在请求前核验CLI仍存活：
- 11h22m12s762：三格游戏画面，右下空格黑色。
- 11h23m48s640：三原源结束后全黑，CLI持续运行。
- 11h24m22s245：所选源重入，左下恢复，其余黑色。
- 11h26m05s073：重入源结束后再次全黑，CLI持续运行。

```powershell
$cli=Get-CimInstance Win32_Process -Filter 'ProcessId=25140'
if(-not $cli -or $cli.Name -ne 'media_transcode_realtime_video_cli.exe' -or $cli.CommandLine -notlike '*composition-r381*'){throw 'r381 CLI is not alive'}
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 36284){throw 'VLC owner mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
try{$s=$c.GetStream();$s.ReadTimeout=2000;$b=[Text.Encoding]::ASCII.GetBytes('snapshot'+[char]10);$s.Write($b,0,$b.Length);$buf=[byte[]]::new(4096);try{$n=$s.Read($buf,0,$buf.Length);[Text.Encoding]::UTF8.GetString($buf,0,$n)}catch{Write-Output $_.Exception.Message}}finally{$c.Dispose()}
Get-ChildItem -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance' -Filter 'composition-r381-native-*.png' | Select-Object FullName,LastWriteTime
```

重入PID19628，会话54809终态REENTRY_EXIT=0。三初始源和重入均完整120秒/3600帧。
```powershell
$reentry=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r381-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r381-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($reentry.Id)"
$reentry.WaitForExit()
Write-Output "REENTRY_EXIT=$($reentry.ExitCode)"
```

## 正常停止
所有源自然结束并完成最终黑屏观察后，发送caller-stop。CLI_EXIT=0，end_reason=caller_stop，不能记为全失源自动结束。
```powershell
$target=Get-CimInstance Win32_Process -Filter "ProcessId=25140"
if(-not $target -or $target.Name -ne 'media_transcode_realtime_video_cli.exe' -or $target.CommandLine -notlike '*composition-r381*'){throw 'CLI identity mismatch'}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class R381Console {
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeConsole();
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool AttachConsole(uint id);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler,bool add);
[DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetConsoleProcessList(uint[] ids,uint count);
[DllImport("kernel32.dll", SetLastError=true)] public static extern bool GenerateConsoleCtrlEvent(uint kind,uint group);
}
'@
[R381Console]::FreeConsole() | Out-Null
if(-not [R381Console]::AttachConsole(25140)){throw 'AttachConsole failed'}
try {
[R381Console]::SetConsoleCtrlHandler([IntPtr]::Zero,$true) | Out-Null
$ids=[uint32[]]::new(16)
$count=[R381Console]::GetConsoleProcessList($ids,16)
if($count -ne 2 -or $ids[0..1] -notcontains 25140 -or $ids[0..1] -notcontains $PID){throw 'Unexpected shared console process'}
if(-not [R381Console]::GenerateConsoleCtrlEvent(0,0)){throw 'CtrlC failed'}
Write-Output 'CLI caller-stop requested'
} finally {[R381Console]::FreeConsole() | Out-Null}
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 36284){throw 'VLC PID mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
try{$s=$c.GetStream();$b=[Text.Encoding]::ASCII.GetBytes('quit'+[char]10);$s.Write($b,0,$b.Length)}finally{$c.Dispose()}


```

## 关键结果与限制
- 最终workers/errors/workerErrors/stalledIntervals/pressureFailures均0；graphPayloadCurrentBytes/Objects=0，223612次预约/释放相等。
- 内部整机口径进程CPU平均1.597490%、峰7.127430%；WS初始209694720、峰272232448、最终263340032字节。短轮观察及计数归零不证明长期物理内存稳定。
- 生产MMCSS node81 PID25140 TID12652，enter/leave均native0且同线程；能力probe独立TID33436。不能由注册成功推导硬实时保证。
- sender174235datagrams、203709468payload bytes，deadline_misses/pressure/partial/ambiguous failures=0；最大提交迟到19.503940ms，最长backlog residence50.8492ms；delivery_evidence=not_proven，没有本轮sender wire抓包证据。
- selected source.2 generation2在11:23:58.491 activation并发布，11:23:58.516贡献恢复。已采样音频raw phase gen1约4535–4537ns、gen2约15873–15874ns，未观察持续增长；这不是内容级A/V或静音验证。
- 最大canvas wall39.2855ms，batch completion38.9709ms，同批GPU event29.6161ms、各native0；不能据此宣布此前r379等待首因已修复。
- 本轮未复现r380发送预约失败，但单轮观察不能证明稳定性修复。静态/动态取消故障矩阵、完整2–4源/异构与HEVC重排、内容A/V与静音、长期内存、直接C API和RKMPP仍待完成。


## CLI非周期诊断
~~~text
[CLI] composition_inputs=3 grid=fixed audio_source=2 audio=on max_duration=source_driven hw=planner-highest-score
[2026-11-04 11:21:29.795] [info] [graph][planner.capability] composition_protocol_installed source=0
[2026-11-04 11:21:29.796] [info] [graph][planner.capability] composition_protocol_installed source=1
[2026-11-04 11:21:29.798] [info] [graph][planner.capability] composition_protocol_installed source=2
[2026-11-04 11:21:29.927] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-04 11:21:29.927] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-04 11:21:29.927] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-r381.source.0 decoder=h264
[2026-11-04 11:21:34.178] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-11-04 11:21:34.193] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-11-04 11:21:34.194] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-11-04 11:21:34.305] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-04 11:21:34.305] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-04 11:21:34.305] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-r381.source.1 decoder=h264
[2026-11-04 11:21:34.363] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-04 11:21:34.363] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-04 11:21:34.363] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-r381.source.2 decoder=h264
[2026-11-04 11:21:34.483] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-11-04 11:21:34.548] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-11-04 11:21:34.550] [info] [graph][planner.capability] canvas_cuda_stream stage=prepared device_stream=0000000000000000 private_nonblocking=1
[2026-11-04 11:21:34.559] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-04 11:21:34.559] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-04 11:21:34.559] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-11-04 11:21:34.569] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-04 11:21:34.570] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-04 11:21:34.570] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-11-04 11:21:34.578] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-04 11:21:34.578] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-04 11:21:34.578] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-11-04 11:21:34.583] [info] [graph][planner.capability] composition_preparation_storage stage=graph_bound scope=shared_reservations capacity=2147483648 retained=831012131 high_water=857063841
[CLI] audio_plan branch=transcode_frame reason=transcode_required_by_output_frame_contract codec=aac sample_rate=44100 channels=2 access_unit_samples=1024 encoder=aac bitrate_kbps=192
[CLI] prepared_output_id=1
[CLI] selected_chain=fixed-grid-composition score=0 decoder=h264,h264,h264 filter=scale_cuda=640:360,scale_cuda=640:360,scale_cuda=640:360 encoder=hevc_nvenc
[2026-11-04 11:21:34.587] [info] [graph][runtime.lifecycle] compile.begin
[2026-11-04 11:21:34.588] [info] [graph][runtime.channel] create channel=1 edge=1 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 11:21:34.588] [info] [graph][runtime.channel] create channel=2 edge=2 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 11:21:34.588] [info] [graph][runtime.channel] create channel=3 edge=3 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-04 11:21:34.588] [info] [graph][runtime.channel] create channel=4 edge=4 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.589] [info] [graph][runtime.channel] create channel=5 edge=5 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.589] [info] [graph][runtime.channel] create channel=6 edge=6 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.589] [info] [graph][runtime.channel] create channel=7 edge=7 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.589] [info] [graph][runtime.channel] create channel=8 edge=8 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.590] [info] [graph][runtime.channel] create channel=9 edge=9 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 11:21:34.590] [info] [graph][runtime.channel] create channel=10 edge=10 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.590] [info] [graph][runtime.channel] create channel=11 edge=11 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.590] [info] [graph][runtime.channel] create channel=12 edge=12 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 11:21:34.590] [info] [graph][runtime.channel] create channel=13 edge=13 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 11:21:34.591] [info] [graph][runtime.channel] create channel=14 edge=14 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.591] [info] [graph][runtime.channel] create channel=15 edge=15 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.591] [info] [graph][runtime.channel] create channel=16 edge=16 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.592] [info] [graph][runtime.channel] create channel=17 edge=17 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.592] [info] [graph][runtime.channel] create channel=18 edge=18 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.592] [info] [graph][runtime.channel] create channel=19 edge=19 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.592] [info] [graph][runtime.channel] create channel=20 edge=20 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.593] [info] [graph][runtime.channel] create channel=21 edge=21 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-04 11:21:34.593] [info] [graph][runtime.channel] create channel=22 edge=22 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 11:21:34.593] [info] [graph][runtime.channel] create channel=23 edge=23 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 11:21:34.593] [info] [graph][runtime.channel] create channel=24 edge=24 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 11:21:34.594] [info] [graph][runtime.channel] create channel=25 edge=25 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 11:21:34.594] [info] [graph][runtime.channel] create channel=26 edge=26 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 11:21:34.594] [info] [graph][runtime.channel] create channel=27 edge=27 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 11:21:34.595] [info] [graph][runtime.channel] create channel=28 edge=28 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 11:21:34.595] [info] [graph][runtime.channel] create channel=29 edge=29 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 11:21:34.595] [info] [graph][runtime.channel] create channel=30 edge=30 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.595] [info] [graph][runtime.channel] create channel=31 edge=31 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.596] [info] [graph][runtime.channel] create channel=32 edge=32 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.596] [info] [graph][runtime.channel] create channel=33 edge=33 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.596] [info] [graph][runtime.channel] create channel=34 edge=34 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.596] [info] [graph][runtime.channel] create channel=35 edge=35 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.597] [info] [graph][runtime.channel] create channel=36 edge=36 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 11:21:34.597] [info] [graph][runtime.channel] create channel=37 edge=37 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.597] [info] [graph][runtime.channel] create channel=38 edge=38 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.598] [info] [graph][runtime.channel] create channel=39 edge=39 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 11:21:34.598] [info] [graph][runtime.channel] create channel=40 edge=40 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 11:21:34.598] [info] [graph][runtime.channel] create channel=41 edge=41 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.598] [info] [graph][runtime.channel] create channel=42 edge=42 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.598] [info] [graph][runtime.channel] create channel=43 edge=43 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.598] [info] [graph][runtime.channel] create channel=44 edge=44 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.599] [info] [graph][runtime.channel] create channel=45 edge=45 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.599] [info] [graph][runtime.channel] create channel=46 edge=46 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.599] [info] [graph][runtime.channel] create channel=47 edge=47 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.599] [info] [graph][runtime.channel] create channel=48 edge=48 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-04 11:21:34.599] [info] [graph][runtime.channel] create channel=49 edge=49 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 11:21:34.600] [info] [graph][runtime.channel] create channel=50 edge=50 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 11:21:34.600] [info] [graph][runtime.channel] create channel=51 edge=51 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 11:21:34.600] [info] [graph][runtime.channel] create channel=52 edge=52 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 11:21:34.600] [info] [graph][runtime.channel] create channel=53 edge=53 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 11:21:34.600] [info] [graph][runtime.channel] create channel=54 edge=54 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 11:21:34.600] [info] [graph][runtime.channel] create channel=55 edge=55 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 11:21:34.601] [info] [graph][runtime.channel] create channel=56 edge=56 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 11:21:34.601] [info] [graph][runtime.channel] create channel=57 edge=57 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.601] [info] [graph][runtime.channel] create channel=58 edge=58 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.601] [info] [graph][runtime.channel] create channel=59 edge=59 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.601] [info] [graph][runtime.channel] create channel=60 edge=60 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.601] [info] [graph][runtime.channel] create channel=61 edge=61 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.601] [info] [graph][runtime.channel] create channel=62 edge=62 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.602] [info] [graph][runtime.channel] create channel=63 edge=63 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.602] [info] [graph][runtime.channel] create channel=64 edge=64 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.602] [info] [graph][runtime.channel] create channel=65 edge=65 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.602] [info] [graph][runtime.channel] create channel=66 edge=66 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 11:21:34.602] [info] [graph][runtime.channel] create channel=67 edge=67 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 11:21:34.602] [info] [graph][runtime.channel] create channel=68 edge=68 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.602] [info] [graph][runtime.channel] create channel=69 edge=69 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.603] [info] [graph][runtime.channel] create channel=70 edge=70 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.603] [info] [graph][runtime.channel] create channel=71 edge=71 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 11:21:34.603] [info] [graph][runtime.channel] create channel=72 edge=72 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 11:21:34.603] [info] [graph][runtime.channel] create channel=73 edge=73 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 11:21:34.603] [info] [graph][runtime.channel] create channel=74 edge=74 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 11:21:34.603] [info] [graph][runtime.channel] create channel=75 edge=75 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.604] [info] [graph][runtime.channel] create channel=76 edge=76 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.604] [info] [graph][runtime.channel] create channel=77 edge=77 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.604] [info] [graph][runtime.channel] create channel=78 edge=78 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.604] [info] [graph][runtime.channel] create channel=79 edge=79 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.604] [info] [graph][runtime.channel] create channel=80 edge=80 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.604] [info] [graph][runtime.channel] create channel=81 edge=81 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.605] [info] [graph][runtime.channel] create channel=82 edge=82 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.605] [info] [graph][runtime.channel] create channel=83 edge=83 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-04 11:21:34.605] [info] [graph][runtime.channel] create channel=84 edge=84 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 11:21:34.605] [info] [graph][runtime.channel] create channel=85 edge=85 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 11:21:34.605] [info] [graph][runtime.channel] create channel=86 edge=86 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 11:21:34.605] [info] [graph][runtime.channel] create channel=87 edge=87 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 11:21:34.605] [info] [graph][runtime.channel] create channel=88 edge=88 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 11:21:34.606] [info] [graph][runtime.channel] create channel=89 edge=89 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 11:21:34.606] [info] [graph][runtime.channel] create channel=90 edge=90 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 11:21:34.606] [info] [graph][runtime.channel] create channel=91 edge=91 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-04 11:21:34.606] [info] [graph][runtime.channel] create channel=92 edge=92 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 11:21:34.606] [info] [graph][runtime.channel] create channel=93 edge=93 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-04 11:21:34.606] [info] [graph][runtime.channel] create channel=94 edge=94 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 11:21:34.606] [info] [graph][runtime.channel] create channel=95 edge=95 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 11:21:34.607] [info] [graph][runtime.channel] create channel=96 edge=96 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 11:21:34.607] [info] [graph][runtime.channel] create channel=97 edge=97 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 11:21:34.607] [info] [graph][runtime.channel] create channel=98 edge=98 stream=Audio edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.607] [info] [graph][runtime.channel] create channel=99 edge=99 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 11:21:34.607] [info] [graph][runtime.channel] create channel=100 edge=100 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.607] [info] [graph][runtime.channel] create channel=101 edge=101 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0
[2026-11-04 11:21:34.608] [info] [graph][runtime.channel] create channel=102 edge=102 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 11:21:34.608] [info] [graph][runtime.channel] create channel=103 edge=103 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 11:21:34.609] [info] [graph][runtime.channel] create channel=104 edge=104 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-04 11:21:34.609] [info] [graph][runtime.channel] create channel=105 edge=105 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0
[2026-11-04 11:21:34.609] [info] [graph][runtime.channel] create channel=106 edge=106 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.609] [info] [graph][runtime.channel] create channel=107 edge=107 stream=Video edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-04 11:21:34.610] [info] [graph][runtime.channel] create channel=108 edge=108 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-04 11:21:34.610] [info] [graph][runtime.channel] create channel=109 edge=109 stream=Any edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-04 11:21:34.610] [info] [graph][runtime.channel] create channel=110 edge=110 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.610] [info] [graph][runtime.channel] create channel=111 edge=111 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-04 11:21:34.610] [info] [graph][runtime.channel] create channel=112 edge=112 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-04 11:21:34.610] [info] [graph][runtime.channel] create channel=113 edge=113 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 11:21:34.611] [info] [graph][runtime.channel] create channel=114 edge=114 stream=Any edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-04 11:21:34.611] [info] [graph][runtime.channel] create channel=115 edge=115 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-04 11:21:34.611] [info] [graph][runtime.channel] create channel=116 edge=116 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-04 11:21:34.611] [info] [graph][runtime.channel] create channel=117 edge=117 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-04 11:21:34.611] [info] [graph][runtime.channel] create channel=118 edge=118 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-04 11:21:34.611] [info] [graph][runtime.channel] create channel=119 edge=119 stream=Any edge_kind=EncodedPacket payload=Unknown queued=0
[2026-11-04 11:21:34.612] [info] [graph][runtime.channel] create channel=120 edge=120 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-04 11:21:34.612] [info] [graph][runtime.channel] create channel=121 edge=121 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-04 11:21:34.612] [info] [graph][runtime.channel] create channel=122 edge=122 stream=Metadata edge_kind=Unknown payload=Unknown queued=0
[2026-11-04 11:21:34.612] [info] [graph][runtime.channel] create channel=123 edge=123 stream=Metadata edge_kind=Unknown payload=Unknown queued=0
[2026-11-04 11:21:34.613] [info] [graph][runtime.lifecycle] distribution_registration scope=capability_probe stage=enter node=none pid=25140 tid=33436 native=0
[2026-11-04 11:21:34.613] [info] [graph][runtime.lifecycle] distribution_registration scope=capability_probe stage=leave node=none pid=25140 tid=33436 native=0
[2026-11-04 11:21:34.613] [info] [graph][runtime.lifecycle] compiled nodes=81 edges=123 channels=123 execution_order=1->3->8->27->46->47->9->21->28->40->63->48->69->10->29->49->11->12->30->31->50->51->52->13->32->53->14->17->33->36->54->55->59->15->34->56->57->16->35->58->18->37->60->19->38->61->20->39->62->22->41->64->70->23->42->65->71->24->43->66->72->26->45->68->73->25->44->67->6->2->7->80->4->77->5->74->75->78->76->79->81
[2026-11-04 11:21:34.614] [info] [graph][runtime.lifecycle] compile.done state=DefaultRegistrationPending
[2026-11-04 11:21:34.614] [info] [graph][runtime.node] register node=1 name=composition-r381.output.video.codec_resolver kind=CodecResolver
[2026-11-04 11:21:34.614] [info] [graph][runtime.node] register node=2 name=composition-r381.output.video.encode kind=VideoEncode
[2026-11-04 11:21:34.615] [info] [graph][runtime.node] register node=3 name=composition-r381.output.audio.codec_resolver kind=AudioCodecResolver
[2026-11-04 11:21:34.615] [info] [graph][runtime.node] register node=4 name=composition-r381.output.audio.encode kind=AudioEncode
[2026-11-04 11:21:34.615] [info] [graph][runtime.node] register node=5 name=composition-r381.output.audio.canonicalizer kind=EncodedAudioCanonicalizer
[2026-11-04 11:21:34.615] [info] [graph][runtime.node] register node=8 name=composition-r381.source_0.input kind=RawRtpInput
[2026-11-04 11:21:34.615] [info] [graph][runtime.node] register node=9 name=composition-r381.source_0.rtp.clock_group kind=RtpClockGroup
[2026-11-04 11:21:34.615] [info] [graph][runtime.node] register node=10 name=composition-r381.source_0.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-11-04 11:21:34.615] [info] [graph][runtime.node] register node=11 name=composition-r381.source_0.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-11-04 11:21:34.616] [info] [graph][runtime.node] register node=12 name=composition-r381.source_0.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-11-04 11:21:34.616] [info] [graph][runtime.node] register node=13 name=composition-r381.source_0.av_sync.source_clock kind=SourceClockStateFanout
[2026-11-04 11:21:34.616] [info] [graph][runtime.node] register node=14 name=composition-r381.source_0.av_sync.video.generation_gate kind=LockedPacketGate
[2026-11-04 11:21:34.616] [info] [graph][runtime.node] register node=15 name=composition-r381.source_0.av_sync.video.canonical_input kind=CanonicalInput
[2026-11-04 11:21:34.616] [info] [graph][runtime.node] register node=16 name=composition-r381.source_0.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-11-04 11:21:34.617] [info] [graph][runtime.node] register node=17 name=composition-r381.source_0.av_sync.startup.clock kind=AvStartupClock
[2026-11-04 11:21:34.617] [info] [graph][runtime.node] register node=18 name=composition-r381.source_0.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-11-04 11:21:34.617] [info] [graph][runtime.node] register node=20 name=composition-r381.source_0.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-11-04 11:21:34.617] [info] [graph][runtime.node] register node=21 name=composition-r381.source_0.video.codec_resolver kind=CodecResolver
[2026-11-04 11:21:34.617] [info] [graph][runtime.node] register node=22 name=composition-r381.source_0.video.decode kind=VideoDecode
[2026-11-04 11:21:34.617] [info] [graph][runtime.node] register node=23 name=composition-r381.source_0.video.hwtransfer kind=HardwareTransfer
[2026-11-04 11:21:34.617] [info] [graph][runtime.node] register node=24 name=composition-r381.source_0.video.framerate kind=VideoFrameRate
[2026-11-04 11:21:34.618] [info] [graph][runtime.node] register node=25 name=composition-r381.source_0.video.preparation kind=VideoPreparation
[2026-11-04 11:21:34.618] [info] [graph][runtime.node] register node=26 name=composition-r381.source_0.video.filter kind=VideoFilter
[2026-11-04 11:21:34.618] [info] [graph][runtime.node] register node=27 name=composition-r381.source_1.input kind=RawRtpInput
[2026-11-04 11:21:34.618] [info] [graph][runtime.node] register node=28 name=composition-r381.source_1.rtp.clock_group kind=RtpClockGroup
[2026-11-04 11:21:34.618] [info] [graph][runtime.node] register node=29 name=composition-r381.source_1.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-11-04 11:21:34.618] [info] [graph][runtime.node] register node=30 name=composition-r381.source_1.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-11-04 11:21:34.618] [info] [graph][runtime.node] register node=31 name=composition-r381.source_1.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-11-04 11:21:34.619] [info] [graph][runtime.node] register node=32 name=composition-r381.source_1.av_sync.source_clock kind=SourceClockStateFanout
[2026-11-04 11:21:34.619] [info] [graph][runtime.node] register node=33 name=composition-r381.source_1.av_sync.video.generation_gate kind=LockedPacketGate
[2026-11-04 11:21:34.619] [info] [graph][runtime.node] register node=34 name=composition-r381.source_1.av_sync.video.canonical_input kind=CanonicalInput
[2026-11-04 11:21:34.619] [info] [graph][runtime.node] register node=35 name=composition-r381.source_1.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-11-04 11:21:34.619] [info] [graph][runtime.node] register node=36 name=composition-r381.source_1.av_sync.startup.clock kind=AvStartupClock
[2026-11-04 11:21:34.619] [info] [graph][runtime.node] register node=37 name=composition-r381.source_1.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-11-04 11:21:34.619] [info] [graph][runtime.node] register node=39 name=composition-r381.source_1.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-11-04 11:21:34.620] [info] [graph][runtime.node] register node=40 name=composition-r381.source_1.video.codec_resolver kind=CodecResolver
[2026-11-04 11:21:34.620] [info] [graph][runtime.node] register node=41 name=composition-r381.source_1.video.decode kind=VideoDecode
[2026-11-04 11:21:34.620] [info] [graph][runtime.node] register node=42 name=composition-r381.source_1.video.hwtransfer kind=HardwareTransfer
[2026-11-04 11:21:34.620] [info] [graph][runtime.node] register node=43 name=composition-r381.source_1.video.framerate kind=VideoFrameRate
[2026-11-04 11:21:34.620] [info] [graph][runtime.node] register node=44 name=composition-r381.source_1.video.preparation kind=VideoPreparation
[2026-11-04 11:21:34.620] [info] [graph][runtime.node] register node=45 name=composition-r381.source_1.video.filter kind=VideoFilter
[2026-11-04 11:21:34.621] [info] [graph][runtime.node] register node=46 name=composition-r381.source_2.video.input kind=RawRtpInput
[2026-11-04 11:21:34.621] [info] [graph][runtime.node] register node=47 name=composition-r381.source_2.audio.input kind=RawRtpInput
[2026-11-04 11:21:34.621] [info] [graph][runtime.node] register node=48 name=composition-r381.source_2.rtp.clock_group kind=RtpClockGroup
[2026-11-04 11:21:34.621] [info] [graph][runtime.node] register node=49 name=composition-r381.source_2.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-11-04 11:21:34.622] [info] [graph][runtime.node] register node=50 name=composition-r381.source_2.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-11-04 11:21:34.623] [info] [graph][runtime.node] register node=51 name=composition-r381.source_2.av_sync.audio.protocol_binder kind=RtpPacketClockBinder
[2026-11-04 11:21:34.623] [info] [graph][runtime.node] register node=52 name=composition-r381.source_2.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-11-04 11:21:34.623] [info] [graph][runtime.node] register node=53 name=composition-r381.source_2.av_sync.source_clock kind=SourceClockStateFanout
[2026-11-04 11:21:34.623] [info] [graph][runtime.node] register node=54 name=composition-r381.source_2.av_sync.video.generation_gate kind=LockedPacketGate
[2026-11-04 11:21:34.623] [info] [graph][runtime.node] register node=55 name=composition-r381.source_2.av_sync.audio.generation_gate kind=LockedPacketGate
[2026-11-04 11:21:34.623] [info] [graph][runtime.node] register node=56 name=composition-r381.source_2.av_sync.video.canonical_input kind=CanonicalInput
[2026-11-04 11:21:34.623] [info] [graph][runtime.node] register node=57 name=composition-r381.source_2.av_sync.audio.canonical_input kind=CanonicalInput
[2026-11-04 11:21:34.623] [info] [graph][runtime.node] register node=58 name=composition-r381.source_2.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-11-04 11:21:34.623] [info] [graph][runtime.node] register node=59 name=composition-r381.source_2.av_sync.startup.clock kind=AvStartupClock
[2026-11-04 11:21:34.623] [info] [graph][runtime.node] register node=60 name=composition-r381.source_2.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-11-04 11:21:34.624] [info] [graph][runtime.node] register node=62 name=composition-r381.source_2.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-11-04 11:21:34.624] [info] [graph][runtime.node] register node=63 name=composition-r381.source_2.video.codec_resolver kind=CodecResolver
[2026-11-04 11:21:34.624] [info] [graph][runtime.node] register node=64 name=composition-r381.source_2.video.decode kind=VideoDecode
[2026-11-04 11:21:34.624] [info] [graph][runtime.node] register node=65 name=composition-r381.source_2.video.hwtransfer kind=HardwareTransfer
[2026-11-04 11:21:34.624] [info] [graph][runtime.node] register node=66 name=composition-r381.source_2.video.framerate kind=VideoFrameRate
[2026-11-04 11:21:34.624] [info] [graph][runtime.node] register node=67 name=composition-r381.source_2.video.preparation kind=VideoPreparation
[2026-11-04 11:21:34.624] [info] [graph][runtime.node] register node=68 name=composition-r381.source_2.video.filter kind=VideoFilter
[2026-11-04 11:21:34.624] [info] [graph][runtime.node] register node=69 name=composition-r381.source_2.audio.encode.codec_resolver kind=AudioCodecResolver
[2026-11-04 11:21:34.625] [info] [graph][runtime.node] register node=70 name=composition-r381.source_2.audio.encode.decode kind=AudioDecode
[2026-11-04 11:21:34.625] [info] [graph][runtime.node] register node=71 name=composition-r381.source_2.audio.encode.startup_trim kind=Unknown
[2026-11-04 11:21:34.625] [info] [graph][runtime.node] register node=72 name=composition-r381.source_2.audio.encode.drift_controller kind=AudioDriftController
[2026-11-04 11:21:34.625] [info] [graph][runtime.node] register node=73 name=composition-r381.source_2.audio.encode.resample kind=AudioResample
[2026-11-04 11:21:34.626] [info] [graph][runtime.node] register node=74 name=composition-r381.output.scheduler.scheduler kind=AvOutputScheduler
[2026-11-04 11:21:34.626] [info] [graph][runtime.node] register node=75 name=composition-r381.output.scheduler.router kind=ScheduledOutputRouter
[2026-11-04 11:21:34.626] [info] [graph][runtime.node] register node=76 name=composition-r381.output.mpegts.mux kind=FileMux
[2026-11-04 11:21:34.627] [info] [graph][runtime.node] register node=77 name=composition-r381.output.mpegts.plan.source kind=Unknown
[2026-11-04 11:21:34.627] [info] [graph][runtime.node] register node=78 name=composition-r381.output.mpegts.scheduled.adapter kind=Unknown
[2026-11-04 11:21:34.627] [info] [graph][runtime.node] register node=79 name=composition-r381.output.mpegts.datagram.materializer kind=MpegTsDatagramMaterializer
[2026-11-04 11:21:34.628] [info] [graph][runtime.node] register node=80 name=composition-r381.output.mpegts.transport.plan kind=DatagramTransportPlanSource
[2026-11-04 11:21:34.628] [info] [graph][runtime.node] register node=81 name=composition-r381.output.mpegts.transport.sender kind=ScheduledDatagramSender
[2026-11-04 11:21:34.628] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=waiting_for_random_access detail="" description=""
[2026-11-04 11:21:34.628] [info] [graph][runtime.lifecycle] start.begin mode=threaded
[2026-11-04 11:21:34.638] [info] [graph][runtime.lifecycle] distribution_registration scope=worker stage=enter node=81 pid=25140 tid=12652 native=0
[2026-11-04 11:21:34.639] [info] [graph][runtime.lifecycle] start.done state=ThreadedRunning
[2026-11-04 11:21:34.639] [info] [graph][runtime.lifecycle] worker.finished node=63
[2026-11-04 11:21:34.639] [info] [graph][runtime.node] worker.thread_cpu node=63 available=1 cpu_ns=0 process_calls=2
[2026-11-04 11:21:34.639] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:21:34.639] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:21:34.639] [info] [graph][runtime.node] video_encode.bind_encoder codec=hevc_nvenc pix_fmt=cuda rate_control=cbr bitrate=8000000 min_bitrate=8000000 max_bitrate=8000000 buffer_size=267000 fps=30/1 gop=60 frame_kind=hardware hwaccel=cuda hw_device_ctx=set hw_frames_ctx=set
[2026-11-04 11:21:34.640] [info] [graph][runtime.lifecycle] worker.finished node=69
[2026-11-04 11:21:34.640] [info] [graph][runtime.node] worker.thread_cpu node=69 available=1 cpu_ns=0 process_calls=1
[2026-11-04 11:21:34.639] [info] [graph][runtime.lifecycle] worker.finished node=40
[2026-11-04 11:21:34.641] [info] [graph][runtime.node] worker.thread_cpu node=40 available=1 cpu_ns=0 process_calls=2
[2026-11-04 11:21:34.640] [info] [graph][runtime.node] audio_encode_trace stage=codec_bound
[2026-11-04 11:21:34.640] [info] [graph][runtime.lifecycle] worker.finished node=3
[2026-11-04 11:21:34.641] [info] [graph][runtime.node] worker.thread_cpu node=3 available=1 cpu_ns=0 process_calls=1
[2026-11-04 11:21:34.639] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:21:34.640] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-11-04 11:21:34.642] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-11-04 11:21:34.639] [info] [graph][runtime.lifecycle] worker.finished node=1
[2026-11-04 11:21:34.642] [info] [graph][runtime.node] worker.thread_cpu node=1 available=1 cpu_ns=0 process_calls=1
[2026-11-04 11:21:34.642] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=3 generation=1
[2026-11-04 11:21:34.639] [info] [graph][runtime.lifecycle] worker.finished node=21
[2026-11-04 11:21:34.643] [info] [graph][runtime.node] worker.thread_cpu node=21 available=1 cpu_ns=0 process_calls=2
[2026-11-04 11:21:34.640] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-11-04 11:21:34.642] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=3 generation=1
[2026-11-04 11:21:34.643] [info] [graph][runtime.node] source_video_trace node_id=39 node=MediaAvBoundReleaseExtractorNode stage=release source_identity=composition-r381.source.1.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4008000000 duration_ns=33333333 decoded_sequence=unavailable pts=957667630 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=9859559966800 group=composition-r381.source.1 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-4008000000 epoch_master_release_ns=5147042800 video_count=15 min_presentation_ns=-4008000000 max_end_ns=-3508000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.643] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=3 generation=1
[2026-11-04 11:21:34.643] [info] [graph][runtime.node] source_video_trace node_id=39 node=MediaAvBoundReleaseExtractorNode stage=published source_identity=composition-r381.source.1.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4008000000 duration_ns=33333333 decoded_sequence=unavailable pts=957667630 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=9859560464400 group=composition-r381.source.1 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.643] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=957667630 dts=957667630 size=317016 generation=1 sequence=3
[2026-11-04 11:21:34.643] [info] [graph][runtime.node] source_video_trace node_id=20 node=MediaAvBoundReleaseExtractorNode stage=release source_identity=composition-r381.source.0.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=3076002418 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=9859560767500 group=composition-r381.source.0 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=-4023000000 epoch_master_release_ns=5147865300 video_count=15 min_presentation_ns=-4023000000 max_end_ns=-3523000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.644] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=input source_identity=composition-r381.source.1.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4008000000 duration_ns=33333333 decoded_sequence=unavailable pts=957667630 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=9859560826100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.644] [info] [graph][runtime.node] source_video_trace node_id=20 node=MediaAvBoundReleaseExtractorNode stage=published source_identity=composition-r381.source.0.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=3076002418 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=9859560921100 group=composition-r381.source.0 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.644] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=3076002418 dts=3076002418 size=317016 generation=1 sequence=3
[2026-11-04 11:21:34.644] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=release source_identity=composition-r381.source.2.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4022000000 duration_ns=33333333 decoded_sequence=unavailable pts=2937944857 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=9859561252900 group=composition-r381.source.2 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-4022000000 epoch_master_release_ns=5148171000 video_count=15 min_presentation_ns=-4022000000 max_end_ns=-3522000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.644] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=input source_identity=composition-r381.source.0.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=3076002418 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=9859561634200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.644] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=2937944857 dts=2937944857 size=317016 generation=1 sequence=3
[2026-11-04 11:21:34.644] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=published source_identity=composition-r381.source.2.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4022000000 duration_ns=33333333 decoded_sequence=unavailable pts=2937944857 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=9859561754000 group=composition-r381.source.2 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.645] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=input source_identity=composition-r381.source.2.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4022000000 duration_ns=33333333 decoded_sequence=unavailable pts=2937944857 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=9859562174600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.677] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-11-04 11:21:34.677] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=accepted source_identity=composition-r381.source.1.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4008000000 duration_ns=33333333 decoded_sequence=unavailable pts=957667630 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=9859594325900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.677] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=957667630 format=cuda size=1280x720 buffer_identity=0xb08600000 drm_prime=0 software_frame=0
[2026-11-04 11:21:34.678] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=resolved source_identity=composition-r381.source.1.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4008000000 duration_ns=33333333 decoded_sequence=unavailable pts=957667630 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=9859594944200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.678] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=0000023912399000 stream=Video payload=Frame pts=957667630 dts=957667630 duration=3000 tb=1/90000 flags=68
[2026-11-04 11:21:34.678] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=published source_identity=composition-r381.source.1.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4008000000 duration_ns=33333333 decoded_sequence=unavailable pts=957667630 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=9859595158800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.678] [info] [graph][runtime.node] source_video_trace node_id=42 node=HardwareTransferNode stage=published source_identity=composition-r381.source.1.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4008000000 duration_ns=33333333 decoded_sequence=unavailable pts=957667630 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=9859595355200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.678] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=0000023912399000 stream=Video payload=Frame pts=957667630 dts=957667630 duration=3000 tb=1/90000 flags=68
[2026-11-04 11:21:34.678] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=0000023912399000 stream=Video payload=Frame pts=957667630 dts=957667630 duration=3000 tb=1/90000 flags=68
[2026-11-04 11:21:34.679] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-11-04 11:21:34.680] [info] [graph][runtime.node] source_video_trace node_id=43 node=VideoFrameRateNode stage=published source_identity=composition-r381.source.1.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4008000000 duration_ns=33333333 decoded_sequence=unavailable pts=957667630 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=9859596840600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.680] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=00000239123993F0 stream=Video payload=Frame pts=957667630 dts=957667630 duration=3000 tb=1/90000 flags=68
[2026-11-04 11:21:34.680] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=00000239123993F0 stream=Video payload=Frame pts=957667630 dts=957667630 duration=3000 tb=1/90000 flags=68
[2026-11-04 11:21:34.680] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-11-04 11:21:34.680] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-11-04 11:21:34.680] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=accepted source_identity=composition-r381.source.0.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=3076002418 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=9859597668400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.681] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=3076002418 format=cuda size=1280x720 buffer_identity=0xb08800000 drm_prime=0 software_frame=0
[2026-11-04 11:21:34.681] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=resolved source_identity=composition-r381.source.0.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=3076002418 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=9859598382100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.681] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=published source_identity=composition-r381.source.0.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=3076002418 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=9859598593100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.681] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=0000023912398430 stream=Video payload=Frame pts=3076002418 dts=3076002418 duration=3000 tb=1/90000 flags=68
[2026-11-04 11:21:34.682] [info] [graph][runtime.node] source_video_trace node_id=23 node=HardwareTransferNode stage=published source_identity=composition-r381.source.0.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=3076002418 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=9859598897700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.682] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=0000023912398430 stream=Video payload=Frame pts=3076002418 dts=3076002418 duration=3000 tb=1/90000 flags=68
[2026-11-04 11:21:34.682] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=0000023912398430 stream=Video payload=Frame pts=3076002418 dts=3076002418 duration=3000 tb=1/90000 flags=68
[2026-11-04 11:21:34.682] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-11-04 11:21:34.682] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-11-04 11:21:34.682] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=accepted source_identity=composition-r381.source.2.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4022000000 duration_ns=33333333 decoded_sequence=unavailable pts=2937944857 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=9859599396800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.682] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=0000023912398C10 stream=Video payload=Frame pts=3076002418 dts=3076002418 duration=3000 tb=1/90000 flags=68
[2026-11-04 11:21:34.682] [info] [graph][runtime.node] source_video_trace node_id=24 node=VideoFrameRateNode stage=published source_identity=composition-r381.source.0.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=3076002418 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=9859599578800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.682] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=2937944857 format=cuda size=1280x720 buffer_identity=0xb08a00000 drm_prime=0 software_frame=0
[2026-11-04 11:21:34.683] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-11-04 11:21:34.683] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=0000023912398C10 stream=Video payload=Frame pts=3076002418 dts=3076002418 duration=3000 tb=1/90000 flags=68
[2026-11-04 11:21:34.683] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=resolved source_identity=composition-r381.source.2.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4022000000 duration_ns=33333333 decoded_sequence=unavailable pts=2937944857 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=9859600364700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.683] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=0000023912398820 stream=Video payload=Frame pts=2937944857 dts=2937944857 duration=3000 tb=1/90000 flags=68
[2026-11-04 11:21:34.683] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=published source_identity=composition-r381.source.2.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4022000000 duration_ns=33333333 decoded_sequence=unavailable pts=2937944857 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=9859600622400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.683] [info] [graph][runtime.node] source_video_trace node_id=65 node=HardwareTransferNode stage=published source_identity=composition-r381.source.2.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4022000000 duration_ns=33333333 decoded_sequence=unavailable pts=2937944857 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=9859600754000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.683] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=0000023912398820 stream=Video payload=Frame pts=2937944857 dts=2937944857 duration=3000 tb=1/90000 flags=68
[2026-11-04 11:21:34.684] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=0000023912398820 stream=Video payload=Frame pts=2937944857 dts=2937944857 duration=3000 tb=1/90000 flags=68
[2026-11-04 11:21:34.684] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-11-04 11:21:34.684] [info] [graph][runtime.node] source_video_trace node_id=66 node=VideoFrameRateNode stage=published source_identity=composition-r381.source.2.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4022000000 duration_ns=33333333 decoded_sequence=unavailable pts=2937944857 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=9859601433300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.684] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=000002390CBA5A70 stream=Video payload=Frame pts=2937944857 dts=2937944857 duration=3000 tb=1/90000 flags=68
[2026-11-04 11:21:34.684] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=000002390CBA5A70 stream=Video payload=Frame pts=2937944857 dts=2937944857 duration=3000 tb=1/90000 flags=68
[2026-11-04 11:21:34.685] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-11-04 11:21:34.770] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=84
[2026-11-04 11:21:34.771] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=89
[2026-11-04 11:21:34.771] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=input source_identity=composition-r381.source.2.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4022000000 duration_ns=33333333 decoded_sequence=unavailable pts=2937944857 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=9859687926700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.772] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=88
[2026-11-04 11:21:34.772] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=input source_identity=composition-r381.source.1.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4008000000 duration_ns=33333333 decoded_sequence=unavailable pts=957667630 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=9859689220800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.772] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=input source_identity=composition-r381.source.0.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=3076002418 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=9859689280500 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.772] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=accepted source_identity=composition-r381.source.2.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4022000000 duration_ns=33333333 decoded_sequence=unavailable pts=2937944857 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=9859689608500 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.773] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=resolved source_identity=composition-r381.source.2.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4022000000 duration_ns=33333333 decoded_sequence=unavailable pts=979315 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=9859690256500 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.773] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=accepted source_identity=composition-r381.source.1.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4008000000 duration_ns=33333333 decoded_sequence=unavailable pts=957667630 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=9859690402800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.773] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=published source_identity=composition-r381.source.2.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4022000000 duration_ns=33333333 decoded_sequence=unavailable pts=979315 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=9859690491000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.774] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb0af20000 drm_prime=0 software_frame=0
[2026-11-04 11:21:34.773] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=input source_identity=composition-r381.source.2.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4022000000 duration_ns=33333333 decoded_sequence=unavailable pts=979315 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=9859690533200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.774] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=resolved source_identity=composition-r381.source.1.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4008000000 duration_ns=33333333 decoded_sequence=unavailable pts=319223 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=9859690945600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.774] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=accepted source_identity=composition-r381.source.0.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=3076002418 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=9859691226100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.774] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=output_attempt source_identity=composition-r381.source.2.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4022000000 duration_ns=33333333 decoded_sequence=unavailable pts=979315 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=9859691382900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.774] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=published source_identity=composition-r381.source.1.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4008000000 duration_ns=33333333 decoded_sequence=unavailable pts=319223 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=9859691602500 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.774] [info] [graph][runtime.node] source_video_trace node_id=44 node=VideoPreparationNode stage=input source_identity=composition-r381.source.1.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4008000000 duration_ns=33333333 decoded_sequence=unavailable pts=319223 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=9859691612600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.775] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=15 audio_count=166 first_video_sequence=3
[2026-11-04 11:21:34.775] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb0ae00000 drm_prime=0 software_frame=0
[2026-11-04 11:21:34.775] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=resolved source_identity=composition-r381.source.0.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=1025334 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=9859692495500 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.775] [info] [graph][runtime.node] source_video_trace node_id=44 node=VideoPreparationNode stage=output_attempt source_identity=composition-r381.source.1.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4008000000 duration_ns=33333333 decoded_sequence=unavailable pts=319223 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=9859692593300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.776] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=published source_identity=composition-r381.source.0.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=1025334 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=9859693180200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.776] [info] [graph][runtime.node] source_video_trace node_id=25 node=VideoPreparationNode stage=input source_identity=composition-r381.source.0.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=1025334 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=9859693190400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.776] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=published source_identity=unavailable identity_link=node_generation_input_source_sequence generation=1 source_sequence=3 presentation_ns=-4022000000 duration_ns=33333333 decoded_sequence=unavailable pts=979315 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=9859693373700 group=composition-r381.source.2 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.776] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=15 audio_count=0 first_video_sequence=3
[2026-11-04 11:21:34.776] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb0ae90000 drm_prime=0 software_frame=0
[2026-11-04 11:21:34.777] [info] [graph][runtime.node] source_video_trace node_id=25 node=VideoPreparationNode stage=output_attempt source_identity=composition-r381.source.0.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=1025334 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=9859693789200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.777] [info] [graph][runtime.node] source_video_trace node_id=61 node=ActivationReleaseSequencer stage=activation source_identity=composition-r381.source.2.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4022000000 duration_ns=33333333 decoded_sequence=unavailable pts=2937944857 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=9859693983400 group=composition-r381.source.2 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-4022000000 epoch_master_release_ns=5346856868 video_count=15 min_presentation_ns=-4022000000 max_end_ns=-3522000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.777] [info] [graph][runtime.node] source_video_trace node_id=38 node=ActivationReleaseSequencer stage=activation source_identity=composition-r381.source.1.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4008000000 duration_ns=33333333 decoded_sequence=unavailable pts=957667630 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=9859694231400 group=composition-r381.source.1 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-4008000000 epoch_master_release_ns=5347988068 video_count=15 min_presentation_ns=-4008000000 max_end_ns=-3508000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.777] [info] [graph][runtime.node] source_video_trace node_id=44 node=VideoPreparationNode stage=published source_identity=unavailable identity_link=node_generation_input_source_sequence generation=1 source_sequence=3 presentation_ns=-4008000000 duration_ns=33333333 decoded_sequence=unavailable pts=319223 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=9859694270700 group=composition-r381.source.1 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.777] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=15 audio_count=0 first_video_sequence=3
[2026-11-04 11:21:34.777] [info] [graph][runtime.node] audio_decode_trace stage=first_packet
[2026-11-04 11:21:34.777] [info] [graph][runtime.node] source_video_trace node_id=61 node=ActivationReleaseSequencer stage=release_published source_identity=composition-r381.source.2.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4022000000 duration_ns=33333333 decoded_sequence=unavailable pts=2937944857 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=9859694756600 group=composition-r381.source.2 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-4022000000 epoch_master_release_ns=5346856868 video_count=15 min_presentation_ns=-4022000000 max_end_ns=-3522000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.777] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_candidate source_identity=composition-r381.source.2.input.video generation=1 steady_ns=9859694762900 source_sequence=3 presentation_ns=-4022000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-04 11:21:34.778] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=release_committed source_identity=composition-r381.source.2.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4022000000 duration_ns=33333333 decoded_sequence=unavailable pts=2937944857 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=9859694774300 group=composition-r381.source.2 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-4022000000 epoch_master_release_ns=5148171000 video_count=15 min_presentation_ns=-4022000000 max_end_ns=-3522000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=17 release_committed_count=1 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.778] [info] [graph][runtime.node] source_video_trace node_id=38 node=ActivationReleaseSequencer stage=release_published source_identity=composition-r381.source.1.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4008000000 duration_ns=33333333 decoded_sequence=unavailable pts=957667630 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=9859694955700 group=composition-r381.source.1 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-4008000000 epoch_master_release_ns=5347988068 video_count=15 min_presentation_ns=-4008000000 max_end_ns=-3508000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.778] [info] [graph][runtime.node] source_video_trace node_id=39 node=MediaAvBoundReleaseExtractorNode stage=release_committed source_identity=composition-r381.source.1.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4008000000 duration_ns=33333333 decoded_sequence=unavailable pts=957667630 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=9859695141500 group=composition-r381.source.1 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-4008000000 epoch_master_release_ns=5147042800 video_count=15 min_presentation_ns=-4008000000 max_end_ns=-3508000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=17 release_committed_count=1 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.778] [info] [graph][runtime.node] source_video_trace node_id=25 node=VideoPreparationNode stage=published source_identity=unavailable identity_link=node_generation_input_source_sequence generation=1 source_sequence=3 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=1025334 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=9859695434200 group=composition-r381.source.0 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.778] [info] [graph][runtime.node] source_video_trace node_id=19 node=ActivationReleaseSequencer stage=activation source_identity=composition-r381.source.0.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=3076002418 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=9859695444300 group=composition-r381.source.0 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=-4023000000 epoch_master_release_ns=5349200568 video_count=15 min_presentation_ns=-4023000000 max_end_ns=-3523000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.779] [info] [graph][runtime.node] audio_decode_trace stage=first_frame samples=1024 rate=44100
[2026-11-04 11:21:34.779] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=166
[2026-11-04 11:21:34.779] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=0
[2026-11-04 11:21:34.780] [info] [graph][runtime.node] source_video_trace node_id=20 node=MediaAvBoundReleaseExtractorNode stage=release_committed source_identity=composition-r381.source.0.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=3076002418 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=9859697089700 group=composition-r381.source.0 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=-4023000000 epoch_master_release_ns=5147865300 video_count=15 min_presentation_ns=-4023000000 max_end_ns=-3523000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=17 release_committed_count=1 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.780] [info] [graph][runtime.node] source_video_trace node_id=19 node=ActivationReleaseSequencer stage=release_published source_identity=composition-r381.source.0.input.video identity_link=direct generation=1 source_sequence=3 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable pts=3076002418 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=9859697078100 group=composition-r381.source.0 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=-4023000000 epoch_master_release_ns=5349200568 video_count=15 min_presentation_ns=-4023000000 max_end_ns=-3523000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:21:34.780] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_candidate source_identity=composition-r381.source.0.input.video generation=1 steady_ns=9859697091900 source_sequence=3 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-04 11:21:34.780] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1 source_master_ns=5352303581 raw_phase_ns=4537 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-11-04 11:21:34.781] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=0
[2026-11-04 11:21:34.781] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_candidate source_identity=composition-r381.source.1.input.video generation=1 steady_ns=9859698393600 source_sequence=3 presentation_ns=-4008000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-04 11:21:34.781] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3 source_master_ns=5398743490 raw_phase_ns=4536 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=45346
[2026-11-04 11:21:34.782] [info] [graph][runtime.node] audio_encode_trace stage=first_frame
[2026-11-04 11:21:34.782] [info] [graph][runtime.node] output_preparation stage=transport_activate generation=1 elapsed_ns=383500 planned_bytes=180767114966 planned_items=1 event=scope_exit
[2026-11-04 11:21:34.783] [info] [graph][runtime.node] output_preparation stage=global_sequence generation=1 elapsed_ns=59200 planned_bytes=68968 planned_items=2154 event=scope_exit
[2026-11-04 11:21:34.783] [info] [graph][runtime.node] output_preparation stage=transport_buffer generation=1 elapsed_ns=1132900 planned_bytes=180767114966 planned_items=1 event=scope_exit
[2026-11-04 11:21:34.784] [info] [graph][runtime.node] output_preparation stage=ts_packetizer generation=1 elapsed_ns=172900 planned_bytes=180765122912 planned_items=61257 event=scope_exit
[2026-11-04 11:21:34.784] [info] [graph][runtime.node] output_preparation stage=ts_mux generation=1 elapsed_ns=465000 planned_bytes=945416 planned_items=59086 event=scope_exit
[2026-11-04 11:21:34.784] [info] [graph][runtime.node] mpegts_emission_plan generation=1 mode=canonical_access_unit_window access_unit_window_ns=100000000 video_initial_service_window_ns=13990650 audio_initial_service_window_ns=13990650 packet_size_bytes=188 maximum_payload_bytes=1316 per_datagram_overhead_bytes=0
[2026-11-04 11:21:34.788] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-r381.source.0.input.video generation=1 steady_ns=9859705284900 source_sequence=3 presentation_ns=-4023000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-04 11:21:34.788] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-r381.source.1.input.video generation=1 steady_ns=9859705656400 source_sequence=3 presentation_ns=-4008000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-04 11:21:34.789] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-r381.source.2.input.video generation=1 steady_ns=9859705909800 source_sequence=3 presentation_ns=-4022000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-04 11:21:34.788] [info] [graph][runtime.node] video_encode.input_contract format=cuda size=1280x720 buffer_identity=0xb0a000000 drm_prime=0 software_frame=0
[2026-11-04 11:21:34.789] [info] [graph][runtime.node] video_encode.generation_start_key_frame pts=0 generation=1
[2026-11-04 11:21:34.789] [info] [graph][runtime.node] video_encode.first_frame pts=0 generation=attached
[2026-11-04 11:21:34.803] [info] [graph][runtime.node] video_encode.first_submit result=accepted
[2026-11-04 11:21:34.803] [info] [graph][runtime.node] video_encode.first_packet pts=0 dts=0 duration=0 size=108357 side_data_elems=1 received_footprint=108365 side_data[0]=Quality stats:8
[2026-11-04 11:21:34.803] [info] [graph][runtime.node] video_encode.first_packet_footprint bytes=108365 wrapped_size=108357 wrapped_side_data_elems=1
[2026-11-04 11:21:34.804] [info] [graph][runtime.node] av_scheduler_trace stage=first_video_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=5352861768 dispatch_master_ns=5352861768 now_ns=5207989700 node=74 identity_kind=output identity="composition-r381.output" canonical_sequence=1 canonical_created_steady_ns=9859720753700 head_observed_steady_ns=9859720815500 codec_receive_begin_steady_ns=9859720222600 codec_receive_end_steady_ns=9859720224100
[2026-11-04 11:21:34.804] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=5208220400 present_generation=1 present_canonical_target_ns=0 present_canonical_dispatch_ns=0 present_target_master_ns=5352861768 present_dispatch_master_ns=5352861768
[2026-11-04 11:21:34.831] [info] [graph][runtime.node] audio_encode_trace stage=first_packet
[2026-11-04 11:21:34.831] [info] [graph][runtime.node] av_scheduler_trace stage=first_audio_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=5352861768 dispatch_master_ns=5352861768 now_ns=5235754500 node=74 identity_kind=output identity="composition-r381.output" canonical_sequence=1 canonical_created_steady_ns=9859748549100 head_observed_steady_ns=9859748580300 codec_receive_begin_steady_ns=unavailable codec_receive_end_steady_ns=unavailable
[2026-11-04 11:21:34.968] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=running detail="" description=""
[2026-11-04 11:21:35.386] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=48 source_master_ns=6443641449 raw_phase_ns=4536 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=90692
[2026-11-04 11:21:36.408] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=92 source_master_ns=7465319454 raw_phase_ns=4537 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=136038
[2026-11-04 11:21:37.430] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=136 source_master_ns=8486997458 raw_phase_ns=4536 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=181384
[2026-11-04 11:21:38.452] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=180 source_master_ns=9508675462 raw_phase_ns=4536 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=226730
[2026-11-04 11:21:39.179] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=1146775736 ntp_seconds=4002751299 ntp_fraction=764504178 arrival_ns=9864095720600
[2026-11-04 11:21:39.179] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1146775736
[2026-11-04 11:21:39.180] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:21:39.210] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3456789512 ntp_seconds=4002751299 ntp_fraction=901943132 arrival_ns=9864127048300
[2026-11-04 11:21:39.210] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3456789512
[2026-11-04 11:21:39.210] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:21:39.225] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1069630075 ntp_seconds=4002751299 ntp_fraction=966367641 arrival_ns=9864142004200
[2026-11-04 11:21:39.225] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1069630075
[2026-11-04 11:21:39.225] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:21:39.225] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2723479215 ntp_seconds=4002751299 ntp_fraction=966367641 arrival_ns=9864141860000
[2026-11-04 11:21:39.226] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2723479215
[2026-11-04 11:21:39.226] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:21:39.496] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=225 source_master_ns=10553573421 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=272076
[2026-11-04 11:21:40.518] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=269 source_master_ns=11575251425 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=317422
[2026-11-04 11:21:41.540] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=313 source_master_ns=12596929430 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=362768
[2026-11-04 11:21:42.584] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=358 source_master_ns=13641827389 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=408114
[2026-11-04 11:21:43.606] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=402 source_master_ns=14663505394 raw_phase_ns=4536 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=453460
[2026-11-04 11:21:44.197] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=1146775736 ntp_seconds=4002751304 ntp_fraction=846108557 arrival_ns=9869114289200
[2026-11-04 11:21:44.197] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1146775736
[2026-11-04 11:21:44.198] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:21:44.228] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2723479215 ntp_seconds=4002751304 ntp_fraction=979252543 arrival_ns=9869145025000
[2026-11-04 11:21:44.228] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2723479215
[2026-11-04 11:21:44.228] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:21:44.229] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3456789512 ntp_seconds=4002751304 ntp_fraction=979252543 arrival_ns=9869145253900
[2026-11-04 11:21:44.229] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3456789512
[2026-11-04 11:21:44.229] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:21:44.258] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1069630075 ntp_seconds=4002751304 ntp_fraction=1108101562 arrival_ns=9869175389900
[2026-11-04 11:21:44.258] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1069630075
[2026-11-04 11:21:44.259] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:21:44.628] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=446 source_master_ns=15685183398 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=498806
[2026-11-04 11:21:45.650] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=490 source_master_ns=16706861403 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=544152
[2026-11-04 11:21:46.694] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=535 source_master_ns=17751759362 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=589498
[2026-11-04 11:21:47.716] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=579 source_master_ns=18773437366 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=634844
[2026-11-04 11:21:48.737] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=623 source_master_ns=19795115371 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=680190
[2026-11-04 11:21:49.212] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=1146775736 ntp_seconds=4002751309 ntp_fraction=910533066 arrival_ns=9874129133700
[2026-11-04 11:21:49.212] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1146775736
[2026-11-04 11:21:49.213] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:21:49.243] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3456789512 ntp_seconds=4002751309 ntp_fraction=1043677052 arrival_ns=9874160502300
[2026-11-04 11:21:49.244] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3456789512
[2026-11-04 11:21:49.244] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:21:49.258] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2723479215 ntp_seconds=4002751309 ntp_fraction=1108101562 arrival_ns=9874175256700
[2026-11-04 11:21:49.258] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2723479215
[2026-11-04 11:21:49.259] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:21:49.289] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1069630075 ntp_seconds=4002751309 ntp_fraction=1241245548 arrival_ns=9874206291900
[2026-11-04 11:21:49.290] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1069630075
[2026-11-04 11:21:49.290] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:21:49.782] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=668 source_master_ns=20840013330 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=725536
[2026-11-04 11:21:50.804] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=712 source_master_ns=21861691335 raw_phase_ns=4536 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=770882
[2026-11-04 11:21:51.826] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=756 source_master_ns=22883369339 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=816228
[2026-11-04 11:21:52.847] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=800 source_master_ns=23905047344 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=861574
[2026-11-04 11:21:53.892] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=845 source_master_ns=24949945303 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=906920
[2026-11-04 11:21:54.257] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3456789512 ntp_seconds=4002751314 ntp_fraction=1099511627 arrival_ns=9879173725100
[2026-11-04 11:21:54.257] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3456789512
[2026-11-04 11:21:54.257] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:21:54.257] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=1146775736 ntp_seconds=4002751314 ntp_fraction=1103806595 arrival_ns=9879174386200
[2026-11-04 11:21:54.258] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1146775736
[2026-11-04 11:21:54.258] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:21:54.288] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2723479215 ntp_seconds=4002751314 ntp_fraction=1236950581 arrival_ns=9879205579200
[2026-11-04 11:21:54.289] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2723479215
[2026-11-04 11:21:54.289] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:21:54.319] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1069630075 ntp_seconds=4002751314 ntp_fraction=1370094567 arrival_ns=9879236124200
[2026-11-04 11:21:54.319] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1069630075
[2026-11-04 11:21:54.319] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:21:54.914] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=889 source_master_ns=25971623307 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=952266
[2026-11-04 11:21:55.936] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=933 source_master_ns=26993301312 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=997612
[2026-11-04 11:21:56.957] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=977 source_master_ns=28014979316 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1042958
[2026-11-04 11:21:58.002] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1022 source_master_ns=29059877276 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1088304
[2026-11-04 11:21:59.024] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1066 source_master_ns=30081555280 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1133650
[2026-11-04 11:21:59.266] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3456789512 ntp_seconds=4002751319 ntp_fraction=1138166333 arrival_ns=9884182791800
[2026-11-04 11:21:59.266] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3456789512
[2026-11-04 11:21:59.266] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:21:59.282] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=1146775736 ntp_seconds=4002751319 ntp_fraction=1206885810 arrival_ns=9884198645700
[2026-11-04 11:21:59.282] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1146775736
[2026-11-04 11:21:59.282] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:21:59.297] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2723479215 ntp_seconds=4002751319 ntp_fraction=1271310319 arrival_ns=9884213758600
[2026-11-04 11:21:59.297] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2723479215
[2026-11-04 11:21:59.297] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:21:59.359] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1069630075 ntp_seconds=4002751319 ntp_fraction=1537598291 arrival_ns=9884275309700
[2026-11-04 11:21:59.359] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1069630075
[2026-11-04 11:21:59.360] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:00.046] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1110 source_master_ns=31103233285 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1178996
[2026-11-04 11:22:01.090] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1155 source_master_ns=32148131244 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1224342
[2026-11-04 11:22:02.112] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1199 source_master_ns=33169809248 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1269688
[2026-11-04 11:22:03.134] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1243 source_master_ns=34191487253 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1315034
[2026-11-04 11:22:04.156] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1287 source_master_ns=35213165258 raw_phase_ns=4536 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1360380
[2026-11-04 11:22:04.266] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3456789512 ntp_seconds=4002751324 ntp_fraction=1142461300 arrival_ns=9889183413400
[2026-11-04 11:22:04.266] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3456789512
[2026-11-04 11:22:04.267] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:04.282] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=1146775736 ntp_seconds=4002751324 ntp_fraction=1211180777 arrival_ns=9889199202000
[2026-11-04 11:22:04.283] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1146775736
[2026-11-04 11:22:04.283] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:04.297] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2723479215 ntp_seconds=4002751324 ntp_fraction=1275605286 arrival_ns=9889214324700
[2026-11-04 11:22:04.297] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2723479215
[2026-11-04 11:22:04.298] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:04.389] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1069630075 ntp_seconds=4002751324 ntp_fraction=1670742278 arrival_ns=9889306226400
[2026-11-04 11:22:04.389] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1069630075
[2026-11-04 11:22:04.390] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:05.201] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1332 source_master_ns=36258063217 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1405726
[2026-11-04 11:22:06.222] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1376 source_master_ns=37279741221 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1451072
[2026-11-04 11:22:07.244] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1420 source_master_ns=38301419226 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1496418
[2026-11-04 11:22:08.289] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1465 source_master_ns=39346317185 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1541764
[2026-11-04 11:22:09.272] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3456789512 ntp_seconds=4002751329 ntp_fraction=1168231104 arrival_ns=9894188911200
[2026-11-04 11:22:09.272] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3456789512
[2026-11-04 11:22:09.272] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:09.288] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=1146775736 ntp_seconds=4002751329 ntp_fraction=1232655613 arrival_ns=9894204634000
[2026-11-04 11:22:09.289] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1146775736
[2026-11-04 11:22:09.289] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:09.310] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1509 source_master_ns=40367995189 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1587110
[2026-11-04 11:22:09.318] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2723479215 ntp_seconds=4002751329 ntp_fraction=1365799600 arrival_ns=9894235491900
[2026-11-04 11:22:09.319] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2723479215
[2026-11-04 11:22:09.319] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:09.412] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1069630075 ntp_seconds=4002751329 ntp_fraction=1769526525 arrival_ns=9894329170200
[2026-11-04 11:22:09.412] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1069630075
[2026-11-04 11:22:09.413] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:10.332] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1553 source_master_ns=41389673194 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1632456
[2026-11-04 11:22:11.354] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1597 source_master_ns=42411351199 raw_phase_ns=4536 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1677802
[2026-11-04 11:22:12.399] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1642 source_master_ns=43456249158 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1723148
[2026-11-04 11:22:13.420] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1686 source_master_ns=44477927162 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1768494
[2026-11-04 11:22:14.277] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3456789512 ntp_seconds=4002751334 ntp_fraction=1189705940 arrival_ns=9899194134500
[2026-11-04 11:22:14.278] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3456789512
[2026-11-04 11:22:14.278] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:14.322] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2723479215 ntp_seconds=4002751334 ntp_fraction=1382979469 arrival_ns=9899239686600
[2026-11-04 11:22:14.323] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=1146775736 ntp_seconds=4002751334 ntp_fraction=1382979469 arrival_ns=9899239600600
[2026-11-04 11:22:14.323] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1146775736
[2026-11-04 11:22:14.324] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:14.323] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2723479215
[2026-11-04 11:22:14.325] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:14.415] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1069630075 ntp_seconds=4002751334 ntp_fraction=1782411427 arrival_ns=9899332200000
[2026-11-04 11:22:14.415] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1069630075
[2026-11-04 11:22:14.416] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:14.442] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1730 source_master_ns=45499605167 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1813840
[2026-11-04 11:22:15.487] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1775 source_master_ns=46544503126 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1859186
[2026-11-04 11:22:16.509] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1819 source_master_ns=47566181131 raw_phase_ns=4536 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1904532
[2026-11-04 11:22:17.530] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1863 source_master_ns=48587859135 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1949878
[2026-11-04 11:22:18.552] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1907 source_master_ns=49609537140 raw_phase_ns=4536 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1995224
[2026-11-04 11:22:19.290] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3456789512 ntp_seconds=4002751339 ntp_fraction=1245540515 arrival_ns=9904207502500
[2026-11-04 11:22:19.291] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3456789512
[2026-11-04 11:22:19.291] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:19.351] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=1146775736 ntp_seconds=4002751339 ntp_fraction=1507533520 arrival_ns=9904268065200
[2026-11-04 11:22:19.351] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2723479215 ntp_seconds=4002751339 ntp_fraction=1507533520 arrival_ns=9904268205600
[2026-11-04 11:22:19.351] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2723479215
[2026-11-04 11:22:19.351] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1146775736
[2026-11-04 11:22:19.352] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:19.352] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:19.460] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1069630075 ntp_seconds=4002751339 ntp_fraction=1975684956 arrival_ns=9904377045800
[2026-11-04 11:22:19.460] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1069630075
[2026-11-04 11:22:19.461] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:19.597] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1952 source_master_ns=50654435099 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2040570
[2026-11-04 11:22:20.618] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1996 source_master_ns=51676113103 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2085916
[2026-11-04 11:22:21.640] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2040 source_master_ns=52697791108 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2131262
[2026-11-04 11:22:22.685] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2085 source_master_ns=53742689067 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2176608
[2026-11-04 11:22:23.707] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2129 source_master_ns=54764367072 raw_phase_ns=4536 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2221954
[2026-11-04 11:22:24.308] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3456789512 ntp_seconds=4002751344 ntp_fraction=1318554959 arrival_ns=9909224690300
[2026-11-04 11:22:24.308] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3456789512
[2026-11-04 11:22:24.309] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:24.353] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=1146775736 ntp_seconds=4002751344 ntp_fraction=1516123455 arrival_ns=9909270627400
[2026-11-04 11:22:24.354] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2723479215 ntp_seconds=4002751344 ntp_fraction=1516123455 arrival_ns=9909270707300
[2026-11-04 11:22:24.354] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2723479215
[2026-11-04 11:22:24.354] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1146775736
[2026-11-04 11:22:24.355] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:24.354] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:24.478] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1069630075 ntp_seconds=4002751344 ntp_fraction=2052994367 arrival_ns=9909395410200
[2026-11-04 11:22:24.478] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1069630075
[2026-11-04 11:22:24.479] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:24.729] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2173 source_master_ns=55786045076 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2267300
[2026-11-04 11:22:25.750] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2217 source_master_ns=56807723081 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2312646
[2026-11-04 11:22:26.795] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2262 source_master_ns=57852621040 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2357992
[2026-11-04 11:22:27.818] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2306 source_master_ns=58874299044 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2403338
[2026-11-04 11:22:28.838] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2350 source_master_ns=59895977049 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2448684
[2026-11-04 11:22:29.320] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3456789512 ntp_seconds=4002751349 ntp_fraction=1374389534 arrival_ns=9914237602100
[2026-11-04 11:22:29.321] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3456789512
[2026-11-04 11:22:29.321] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:29.365] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2723479215 ntp_seconds=4002751349 ntp_fraction=1563368095 arrival_ns=9914281898800
[2026-11-04 11:22:29.365] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2723479215
[2026-11-04 11:22:29.366] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:29.380] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=1146775736 ntp_seconds=4002751349 ntp_fraction=1632087572 arrival_ns=9914297536800
[2026-11-04 11:22:29.381] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1146775736
[2026-11-04 11:22:29.382] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:29.488] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1069630075 ntp_seconds=4002751349 ntp_fraction=2091649073 arrival_ns=9914404704200
[2026-11-04 11:22:29.488] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1069630075
[2026-11-04 11:22:29.488] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:29.883] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2395 source_master_ns=60940875008 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2494030
[2026-11-04 11:22:30.905] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2439 source_master_ns=61962553013 raw_phase_ns=4536 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2539376
[2026-11-04 11:22:31.927] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2483 source_master_ns=62984231017 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2584722
[2026-11-04 11:22:32.948] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2527 source_master_ns=64005909022 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2630068
[2026-11-04 11:22:33.993] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2572 source_master_ns=65050806981 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2675414
[2026-11-04 11:22:34.329] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3456789512 ntp_seconds=4002751354 ntp_fraction=1413044240 arrival_ns=9919245872000
[2026-11-04 11:22:34.329] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3456789512
[2026-11-04 11:22:34.329] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:34.391] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=1146775736 ntp_seconds=4002751354 ntp_fraction=1675037245 arrival_ns=9919307396000
[2026-11-04 11:22:34.391] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1146775736
[2026-11-04 11:22:34.391] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:34.391] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2723479215 ntp_seconds=4002751354 ntp_fraction=1675037245 arrival_ns=9919307343800
[2026-11-04 11:22:34.392] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2723479215
[2026-11-04 11:22:34.392] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:34.515] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1069630075 ntp_seconds=4002751354 ntp_fraction=2207613190 arrival_ns=9919431528100
[2026-11-04 11:22:34.515] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1069630075
[2026-11-04 11:22:34.515] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:35.015] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2616 source_master_ns=66072484985 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2720760
[2026-11-04 11:22:36.036] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2660 source_master_ns=67094162990 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2766106
[2026-11-04 11:22:37.082] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2705 source_master_ns=68139060949 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2811452
[2026-11-04 11:22:38.103] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2749 source_master_ns=69160738954 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2856798
[2026-11-04 11:22:39.124] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2793 source_master_ns=70182416958 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2902144
[2026-11-04 11:22:39.338] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3456789512 ntp_seconds=4002751359 ntp_fraction=1447403978 arrival_ns=9924254717700
[2026-11-04 11:22:39.338] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3456789512
[2026-11-04 11:22:39.338] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:39.417] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2723479215 ntp_seconds=4002751359 ntp_fraction=1786706395 arrival_ns=9924333458800
[2026-11-04 11:22:39.417] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2723479215
[2026-11-04 11:22:39.417] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:39.418] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=1146775736 ntp_seconds=4002751359 ntp_fraction=1795296329 arrival_ns=9924335385400
[2026-11-04 11:22:39.419] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1146775736
[2026-11-04 11:22:39.419] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:39.524] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1069630075 ntp_seconds=4002751359 ntp_fraction=2250562863 arrival_ns=9924441421500
[2026-11-04 11:22:39.525] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1069630075
[2026-11-04 11:22:39.525] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:40.146] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2837 source_master_ns=71204094963 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2947490
[2026-11-04 11:22:41.192] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2882 source_master_ns=72248992922 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2992836
[2026-11-04 11:22:42.213] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2926 source_master_ns=73270670926 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3038182
[2026-11-04 11:22:43.235] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2970 source_master_ns=74292348931 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3083528
[2026-11-04 11:22:44.280] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3015 source_master_ns=75337246890 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3128874
[2026-11-04 11:22:44.353] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3456789512 ntp_seconds=4002751364 ntp_fraction=1511828488 arrival_ns=9929269783000
[2026-11-04 11:22:44.353] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3456789512
[2026-11-04 11:22:44.353] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:44.430] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2723479215 ntp_seconds=4002751364 ntp_fraction=1846835937 arrival_ns=9929347045700
[2026-11-04 11:22:44.430] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2723479215
[2026-11-04 11:22:44.431] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:44.445] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=1146775736 ntp_seconds=4002751364 ntp_fraction=1906965479 arrival_ns=9929361724500
[2026-11-04 11:22:44.445] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1146775736
[2026-11-04 11:22:44.445] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:44.553] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1069630075 ntp_seconds=4002751364 ntp_fraction=2375116914 arrival_ns=9929470263900
[2026-11-04 11:22:44.553] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1069630075
[2026-11-04 11:22:44.554] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:45.301] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3059 source_master_ns=76358924895 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3174220
[2026-11-04 11:22:46.323] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3103 source_master_ns=77380602899 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3219566
[2026-11-04 11:22:47.345] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3147 source_master_ns=78402280904 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3264912
[2026-11-04 11:22:48.390] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3192 source_master_ns=79447178863 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3310258
[2026-11-04 11:22:49.364] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3456789512 ntp_seconds=4002751369 ntp_fraction=1563368095 arrival_ns=9934281499600
[2026-11-04 11:22:49.365] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3456789512
[2026-11-04 11:22:49.365] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:49.411] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3236 source_master_ns=80468856868 raw_phase_ns=4536 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3355604
[2026-11-04 11:22:49.457] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=1146775736 ntp_seconds=4002751369 ntp_fraction=1958505086 arrival_ns=9934373909800
[2026-11-04 11:22:49.457] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1146775736
[2026-11-04 11:22:49.457] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:49.459] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2723479215 ntp_seconds=4002751369 ntp_fraction=1971389988 arrival_ns=9934376086800
[2026-11-04 11:22:49.460] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2723479215
[2026-11-04 11:22:49.460] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:49.580] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1069630075 ntp_seconds=4002751369 ntp_fraction=2491081031 arrival_ns=9934497266400
[2026-11-04 11:22:49.580] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1069630075
[2026-11-04 11:22:49.581] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:50.433] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3280 source_master_ns=81490534872 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3400950
[2026-11-04 11:22:51.477] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3325 source_master_ns=82535432831 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3446296
[2026-11-04 11:22:52.499] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3369 source_master_ns=83557110836 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3491642
[2026-11-04 11:22:53.521] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3413 source_master_ns=84578788840 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3536988
[2026-11-04 11:22:54.379] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3456789512 ntp_seconds=4002751374 ntp_fraction=1627792605 arrival_ns=9939296041800
[2026-11-04 11:22:54.379] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3456789512
[2026-11-04 11:22:54.380] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:54.487] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=1146775736 ntp_seconds=4002751374 ntp_fraction=2091649073 arrival_ns=9939404370500
[2026-11-04 11:22:54.488] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1146775736
[2026-11-04 11:22:54.488] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:54.488] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2723479215 ntp_seconds=4002751374 ntp_fraction=2095944040 arrival_ns=9939405435100
[2026-11-04 11:22:54.488] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2723479215
[2026-11-04 11:22:54.489] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:54.543] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3457 source_master_ns=85600466845 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3582334
[2026-11-04 11:22:54.626] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1069630075 ntp_seconds=4002751374 ntp_fraction=2688649527 arrival_ns=9939543240800
[2026-11-04 11:22:54.627] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1069630075
[2026-11-04 11:22:54.627] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:55.588] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3502 source_master_ns=86645364804 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3627680
[2026-11-04 11:22:56.609] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3546 source_master_ns=87667042809 raw_phase_ns=4536 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3673026
[2026-11-04 11:22:57.631] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3590 source_master_ns=88688720813 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3718372
[2026-11-04 11:22:58.653] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3634 source_master_ns=89710398818 raw_phase_ns=4536 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3763718
[2026-11-04 11:22:59.390] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3456789512 ntp_seconds=4002751379 ntp_fraction=1675037245 arrival_ns=9944307224000
[2026-11-04 11:22:59.391] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3456789512
[2026-11-04 11:22:59.391] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:59.498] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2723479215 ntp_seconds=4002751379 ntp_fraction=2138893713 arrival_ns=9944415017800
[2026-11-04 11:22:59.498] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2723479215
[2026-11-04 11:22:59.498] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:59.514] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=1146775736 ntp_seconds=4002751379 ntp_fraction=2203318222 arrival_ns=9944430745500
[2026-11-04 11:22:59.514] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1146775736
[2026-11-04 11:22:59.514] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:59.654] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1069630075 ntp_seconds=4002751379 ntp_fraction=2804613644 arrival_ns=9944570048700
[2026-11-04 11:22:59.655] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1069630075
[2026-11-04 11:22:59.655] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:22:59.698] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3679 source_master_ns=90755296777 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3809064
[2026-11-04 11:23:00.719] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3723 source_master_ns=91776974781 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3854410
[2026-11-04 11:23:01.741] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3767 source_master_ns=92798652786 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3899756
[2026-11-04 11:23:02.786] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3812 source_master_ns=93843550745 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3945102
[2026-11-04 11:23:03.808] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3856 source_master_ns=94865228750 raw_phase_ns=4536 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3990448
[2026-11-04 11:23:04.407] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3456789512 ntp_seconds=4002751384 ntp_fraction=1748051689 arrival_ns=9949324110400
[2026-11-04 11:23:04.407] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3456789512
[2026-11-04 11:23:04.408] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:23:04.516] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=1146775736 ntp_seconds=4002751384 ntp_fraction=2211908157 arrival_ns=9949432919600
[2026-11-04 11:23:04.516] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2723479215 ntp_seconds=4002751384 ntp_fraction=2216203124 arrival_ns=9949433397500
[CLI] [2026-11-04 11:23:04.517] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2723479215
[2026-11-04 11:23:04.517] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
runtime report: queued=631, peakQueued=920, threads=81, processThreads=129, workers=75, workerProgress=245303, workerProcessCalls=960254, workerWaits=714942, workerWakeups=708045, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=285, logicalProcessors=22, averageSystemMachineCpuPercent=10.179595, averageProcessMachineCpuPercent=2.057697, peakProcessMachineCpuPercent=7.127430, averageProcessSingleCoreCpuPercent=45.269335, peakProcessSingleCoreCpuPercent=156.803456, initialWorkingSetBytes=209694720, workingSetBytes=269766656, peakWorkingSetBytes=270876672, totalPushed=212924, totalPopped=212293, droppedBuffers=0, encodedPacketsPushed=30080, encodedPacketsPopped=30079, backpressureItems=123, graphPayloadCurrentBytes=10959450, graphPayloadHighWaterBytes=15972617, graphPayloadCurrentObjects=745, graphPayloadHighWaterObjects=752, graphPayloadReservations=84523, graphPayloadReleases=83778, graphPayloadPressureFailures=0, lastInputReceivedAtNs=9949433105400
[2026-11-04 11:23:04.516] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1146775736
[2026-11-04 11:23:04.518] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:23:04.655] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1069630075 ntp_seconds=4002751384 ntp_fraction=2813203578 arrival_ns=9949571864400
[2026-11-04 11:23:04.655] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1069630075
[2026-11-04 11:23:04.655] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:23:04.829] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3900 source_master_ns=95886906754 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4035794
[2026-11-04 11:23:05.851] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3944 source_master_ns=96908584759 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4081140
[2026-11-04 11:23:06.896] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3989 source_master_ns=97953482718 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4126486
[2026-11-04 11:23:07.920] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4033 source_master_ns=98975160722 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4171832
[2026-11-04 11:23:08.939] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4077 source_master_ns=99996838727 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4217178
[2026-11-04 11:23:09.412] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3456789512 ntp_seconds=4002751389 ntp_fraction=1769526525 arrival_ns=9954329095300
[2026-11-04 11:23:09.412] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3456789512
[2026-11-04 11:23:09.413] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:23:09.520] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=1146775736 ntp_seconds=4002751389 ntp_fraction=2229088026 arrival_ns=9954436824900
[2026-11-04 11:23:09.520] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1146775736
[2026-11-04 11:23:09.520] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:23:09.521] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2723479215 ntp_seconds=4002751389 ntp_fraction=2237677961 arrival_ns=9954438034700
[2026-11-04 11:23:09.521] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2723479215
[2026-11-04 11:23:09.521] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:23:09.657] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1069630075 ntp_seconds=4002751389 ntp_fraction=2821793513 arrival_ns=9954574277600
[2026-11-04 11:23:09.658] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1069630075
[2026-11-04 11:23:09.658] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:23:09.984] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4122 source_master_ns=101041736686 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4262524
[2026-11-04 11:23:11.006] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4166 source_master_ns=102063414691 raw_phase_ns=4536 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4307870
[2026-11-04 11:23:12.027] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4210 source_master_ns=103085092695 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4353216
[2026-11-04 11:23:13.049] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4254 source_master_ns=104106770700 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4398562
[2026-11-04 11:23:14.094] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4299 source_master_ns=105151668659 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4443908
[2026-11-04 11:23:14.418] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3456789512 ntp_seconds=4002751394 ntp_fraction=1795296329 arrival_ns=9959334955500
[2026-11-04 11:23:14.418] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3456789512
[2026-11-04 11:23:14.418] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:23:14.525] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2723479215 ntp_seconds=4002751394 ntp_fraction=2254857830 arrival_ns=9959442483300
[2026-11-04 11:23:14.526] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2723479215
[2026-11-04 11:23:14.526] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:23:14.557] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=1146775736 ntp_seconds=4002751394 ntp_fraction=2392296783 arrival_ns=9959474010300
[2026-11-04 11:23:14.557] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1146775736
[2026-11-04 11:23:14.557] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:23:14.665] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1069630075 ntp_seconds=4002751394 ntp_fraction=2856153251 arrival_ns=9959582364700
[2026-11-04 11:23:14.666] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1069630075
[2026-11-04 11:23:14.666] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:23:15.116] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4343 source_master_ns=106173346663 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4489254
[2026-11-04 11:23:16.137] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4387 source_master_ns=107195024668 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4534600
[2026-11-04 11:23:17.182] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4432 source_master_ns=108239922627 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4579946
[2026-11-04 11:23:18.204] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4476 source_master_ns=109261600632 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4625292
[2026-11-04 11:23:19.226] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4520 source_master_ns=110283278636 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4670638
[2026-11-04 11:23:19.428] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3456789512 ntp_seconds=4002751399 ntp_fraction=1838246002 arrival_ns=9964345242000
[2026-11-04 11:23:19.428] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3456789512
[2026-11-04 11:23:19.429] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:23:19.558] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2723479215 ntp_seconds=4002751399 ntp_fraction=2392296783 arrival_ns=9964473874300
[2026-11-04 11:23:19.558] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2723479215
[2026-11-04 11:23:19.559] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:23:19.582] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=1146775736 ntp_seconds=4002751399 ntp_fraction=2491081031 arrival_ns=9964497651800
[2026-11-04 11:23:19.582] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1146775736
[2026-11-04 11:23:19.582] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:23:19.690] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1069630075 ntp_seconds=4002751399 ntp_fraction=2959232466 arrival_ns=9964606364400
[2026-11-04 11:23:19.691] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1069630075
[2026-11-04 11:23:19.691] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:23:20.247] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4564 source_master_ns=111304956641 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4715984
[2026-11-04 11:23:21.292] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4609 source_master_ns=112349854600 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4761330
[2026-11-04 11:23:22.314] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4653 source_master_ns=113371532604 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4806676
[2026-11-04 11:23:23.336] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4697 source_master_ns=114393210609 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4852022
[2026-11-04 11:23:24.381] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4742 source_master_ns=115438108568 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4897368
[2026-11-04 11:23:24.450] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3456789512 ntp_seconds=4002751404 ntp_fraction=1932735283 arrival_ns=9969366883400
[2026-11-04 11:23:24.450] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3456789512
[2026-11-04 11:23:24.451] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:23:24.587] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=1146775736 ntp_seconds=4002751404 ntp_fraction=2521145802 arrival_ns=9969504075300
[2026-11-04 11:23:24.587] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1146775736
[2026-11-04 11:23:24.587] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2723479215 ntp_seconds=4002751404 ntp_fraction=2521145802 arrival_ns=9969504289700
[2026-11-04 11:23:24.587] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:23:24.588] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2723479215
[2026-11-04 11:23:24.588] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:23:24.726] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1069630075 ntp_seconds=4002751404 ntp_fraction=3118146256 arrival_ns=9969643025100
[2026-11-04 11:23:24.726] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1069630075
[2026-11-04 11:23:24.726] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-04 11:23:25.402] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4786 source_master_ns=116459786573 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4942714
[2026-11-04 11:23:26.424] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4830 source_master_ns=117481464577 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4988060
[2026-11-04 11:23:27.446] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4874 source_master_ns=118503142582 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5033406
[2026-11-04 11:23:28.490] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4919 source_master_ns=119548040541 raw_phase_ns=4535 filtered_phase_ns=4537 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5078752
[2026-11-04 11:23:28.649] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=9973566021000
[2026-11-04 11:23:28.649] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=3 generation=2 invalidated_generation=1 locked=0
[2026-11-04 11:23:28.649] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=9973565933100
[2026-11-04 11:23:28.650] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=3 generation=2 invalidated_generation=1 locked=0
[2026-11-04 11:23:28.650] [info] [graph][runtime.node] av_reacquisition group=composition-r381.source.0 phase=purging old=1 next=2 transition=1
[2026-11-04 11:23:28.650] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=11 group=composition-r381.source.0 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=23 video_raw_generation=0 video_ssrc=1069630075
[2026-11-04 11:23:28.650] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=30 group=composition-r381.source.1 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=23 video_raw_generation=0 video_ssrc=1146775736
[2026-11-04 11:23:28.650] [info] [graph][runtime.node] rtp_ingress_purge node=8 transition=1 cutoff=0
[2026-11-04 11:23:28.651] [info] [graph][runtime.node] av_reacquisition group=composition-r381.source.1 phase=purging old=1 next=2 transition=1
[2026-11-04 11:23:28.654] [info] [graph][runtime.node] rtp_binder_purge node=11 transition=1 cutoff=3596
[2026-11-04 11:23:28.654] [info] [graph][runtime.node] av_reacquisition group=composition-r381.source.0 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=1 next=2 transition=1
[2026-11-04 11:23:28.659] [info] [graph][runtime.node] rtp_ingress_purge node=27 transition=1 cutoff=0
[2026-11-04 11:23:28.660] [info] [graph][runtime.node] rtp_binder_purge node=30 transition=1 cutoff=3596
[2026-11-04 11:23:28.660] [info] [graph][runtime.node] av_reacquisition group=composition-r381.source.1 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=1 next=2 transition=1
[2026-11-04 11:23:28.664] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=9973581011000
[2026-11-04 11:23:28.664] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=3 generation=2 invalidated_generation=1 locked=0
[2026-11-04 11:23:28.664] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=50 group=composition-r381.source.2 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=46 video_raw_generation=0 video_ssrc=2723479215 audio_raw_generation=0 audio_ssrc=3456789512
[2026-11-04 11:23:28.664] [info] [graph][runtime.node] av_reacquisition group=composition-r381.source.2 phase=purging old=1 next=2 transition=1
[2026-11-04 11:23:28.664] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=51 group=composition-r381.source.2 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=46 video_raw_generation=0 video_ssrc=2723479215 audio_raw_generation=0 audio_ssrc=3456789512
[2026-11-04 11:23:28.666] [info] [graph][runtime.node] rtp_ingress_purge node=46 transition=1 cutoff=0
[2026-11-04 11:23:28.666] [info] [graph][runtime.node] rtp_ingress_purge node=47 transition=1 cutoff=0
[2026-11-04 11:23:28.667] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=9973583874200
[2026-11-04 11:23:28.667] [info] [graph][runtime.node] rtp_binder_purge node=50 transition=1 cutoff=3596
[2026-11-04 11:23:28.667] [info] [graph][runtime.node] rtp_binder_purge node=51 transition=1 cutoff=5151
[2026-11-04 11:23:28.667] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=3 generation=2 invalidated_generation=1 locked=0
[2026-11-04 11:23:28.670] [info] [graph][runtime.node] av_reacquisition group=composition-r381.source.2 event=purge_completed purge_ack=complete continuation=acquiring ack_count=2 old=1 next=2 transition=1
[2026-11-04 11:23:57.445] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2373829209 ntp_seconds=4002751437 ntp_fraction=1911260446 arrival_ns=10002362112400
[2026-11-04 11:23:57.445] [info] [graph][runtime.node] rtp_binder_boundary stage=deadline_established node=51 group=composition-r381.source.2 buffered=0 phase=2 preserves_output=1 raw_ssrc=2373829209 raw_generation=1 raw_sequence=5154 old_generation=1 next_generation=2 transition_sequence=1 published_generation=1
[2026-11-04 11:23:57.445] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2373829209
[2026-11-04 11:23:57.446] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=0 generation=2 invalidated_generation=1 locked=0
[2026-11-04 11:23:57.446] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2514713460 ntp_seconds=4002751437 ntp_fraction=1911260446 arrival_ns=10002362121600
[2026-11-04 11:23:57.446] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2514713460
[2026-11-04 11:23:57.447] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:23:57.986] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=1 generation=2
[2026-11-04 11:23:58.491] [info] [graph][runtime.node] source_video_trace_summary node_id=61 node=ActivationReleaseSequencer generation=1 reason=generation_change steady_ns=10003408627600 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3555 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:23:58.491] [info] [graph][runtime.node] source_video_trace node_id=61 node=ActivationReleaseSequencer stage=activation source_identity=composition-r381.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=1661230445 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=10003408779000 group=composition-r381.source.2 release_identity=15626 transition_sequence=1 epoch_source_start_ns=993000000 epoch_master_release_ns=148995565000 video_count=16 min_presentation_ns=993000000 max_end_ns=1526333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:23:58.492] [info] [graph][runtime.node] source_video_trace node_id=61 node=ActivationReleaseSequencer stage=release_published source_identity=composition-r381.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=1661230445 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=10003408887100 group=composition-r381.source.2 release_identity=15626 transition_sequence=1 epoch_source_start_ns=993000000 epoch_master_release_ns=148995565000 video_count=16 min_presentation_ns=993000000 max_end_ns=1526333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:23:58.492] [info] [graph][runtime.node] source_video_trace_summary node_id=62 node=MediaAvBoundReleaseExtractorNode generation=1 reason=generation_change steady_ns=10003408910900 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=3571 release_committed_count=3555 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:23:58.492] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=release source_identity=composition-r381.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=1661230445 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=10003409221800 group=composition-r381.source.2 release_identity=15626 transition_sequence=1 epoch_source_start_ns=993000000 epoch_master_release_ns=148995565000 video_count=16 min_presentation_ns=993000000 max_end_ns=1526333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:23:58.492] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=release_committed source_identity=composition-r381.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=1661230445 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=10003409397900 group=composition-r381.source.2 release_identity=15626 transition_sequence=1 epoch_source_start_ns=993000000 epoch_master_release_ns=148995565000 video_count=16 min_presentation_ns=993000000 max_end_ns=1526333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=1 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:23:58.492] [info] [graph][runtime.node] source_video_trace_summary node_id=64 node=VideoDecodeNode generation=1 reason=generation_change steady_ns=10003409421900 input_count=3459 accepted_count=3459 would_block_count=0 resolved_count=3459 output_attempt_count=0 published_count=3458 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=110 incomplete=0
[2026-11-04 11:23:58.492] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1 source_master_ns=148996011712 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-11-04 11:23:58.492] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=input source_identity=composition-r381.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=1661230445 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=10003409724800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=110 incomplete=0
[2026-11-04 11:23:58.493] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3 source_master_ns=149042451621 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=45346
[2026-11-04 11:23:58.498] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=accepted source_identity=composition-r381.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=1661230445 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=10003415514400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=110 incomplete=0
[2026-11-04 11:23:58.499] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=resolved source_identity=composition-r381.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=1661230445 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=10003415830900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=110 incomplete=0
[2026-11-04 11:23:58.499] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=published source_identity=composition-r381.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=1661230445 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=10003415990500 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=110 incomplete=0
[2026-11-04 11:23:58.499] [info] [graph][runtime.node] source_video_trace_summary node_id=65 node=HardwareTransferNode generation=1 reason=generation_change steady_ns=10003416029800 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3458 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:23:58.499] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-11-04 11:23:58.499] [info] [graph][runtime.node] source_video_trace node_id=65 node=HardwareTransferNode stage=published source_identity=composition-r381.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=1661230445 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=10003416282700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:23:58.499] [info] [graph][runtime.node] source_video_trace_summary node_id=66 node=VideoFrameRateNode generation=1 reason=generation_change steady_ns=10003416416600 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3444 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:23:58.499] [info] [graph][runtime.node] source_video_trace_summary node_id=68 node=VideoFilterNode generation=1 reason=generation_change steady_ns=10003416472000 input_count=3438 accepted_count=3438 would_block_count=0 resolved_count=3438 output_attempt_count=0 published_count=3437 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-04 11:23:58.499] [info] [graph][runtime.node] source_video_trace node_id=66 node=VideoFrameRateNode stage=published source_identity=composition-r381.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=1661230445 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=10003416668700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:23:58.500] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=input source_identity=composition-r381.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=1661230445 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=10003416801300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-04 11:23:58.500] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=accepted source_identity=composition-r381.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=1661230445 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=10003417017800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-04 11:23:58.500] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=resolved source_identity=composition-r381.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=553743 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=10003417189300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-04 11:23:58.500] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=published source_identity=composition-r381.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=553743 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=10003417292000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-04 11:23:58.500] [info] [graph][runtime.node] source_video_trace_summary node_id=67 node=VideoPreparationNode generation=1 reason=generation_change steady_ns=10003417323600 input_count=3431 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3431 published_count=3430 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-04 11:23:58.500] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=input source_identity=composition-r381.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=553743 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=10003417444200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-04 11:23:58.500] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=output_attempt source_identity=composition-r381.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=553743 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=10003417523000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-04 11:23:58.500] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=published source_identity=composition-r381.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=553743 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=10003417605300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-04 11:23:58.505] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_candidate source_identity=composition-r381.source.2.input.video generation=2 steady_ns=10003422175200 source_sequence=3 presentation_ns=1059666666 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-04 11:23:58.516] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-r381.source.2.input.video generation=2 steady_ns=10003432986200 source_sequence=3 presentation_ns=1059666666 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-04 11:23:59.048] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=48 source_master_ns=150087349580 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=90692
[2026-11-04 11:24:00.070] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=92 source_master_ns=151109027585 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=136038
[2026-11-04 11:24:01.091] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=136 source_master_ns=152130705589 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=181384
[2026-11-04 11:24:02.136] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=181 source_master_ns=153175603548 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=226730
[2026-11-04 11:24:02.462] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2373829209 ntp_seconds=4002751442 ntp_fraction=1979979923 arrival_ns=10007378674300
[2026-11-04 11:24:02.463] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2373829209
[2026-11-04 11:24:02.463] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:24:02.463] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2514713460 ntp_seconds=4002751442 ntp_fraction=1979979923 arrival_ns=10007378626900
[2026-11-04 11:24:02.464] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2514713460
[2026-11-04 11:24:02.464] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:24:03.158] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=225 source_master_ns=154197281553 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=272076
[2026-11-04 11:24:04.179] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=269 source_master_ns=155218959557 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=317422
[2026-11-04 11:24:05.201] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=313 source_master_ns=156240637562 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=362768
[2026-11-04 11:24:06.246] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=358 source_master_ns=157285535521 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=408114
[2026-11-04 11:24:07.268] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=402 source_master_ns=158307213526 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=453460
[2026-11-04 11:24:07.478] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2373829209 ntp_seconds=4002751447 ntp_fraction=2052994367 arrival_ns=10012395415800
[2026-11-04 11:24:07.479] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2373829209
[2026-11-04 11:24:07.479] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:24:07.479] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2514713460 ntp_seconds=4002751447 ntp_fraction=2052994367 arrival_ns=10012395489900
[2026-11-04 11:24:07.479] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2514713460
[2026-11-04 11:24:07.480] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:24:08.290] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=446 source_master_ns=159328891530 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=498806
[2026-11-04 11:24:09.334] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=491 source_master_ns=160373789489 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=544152
[2026-11-04 11:24:10.356] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=535 source_master_ns=161395467494 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=589498
[2026-11-04 11:24:11.378] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=579 source_master_ns=162417145498 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=634844
[2026-11-04 11:24:12.399] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=623 source_master_ns=163438823503 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=680190
[2026-11-04 11:24:12.487] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2373829209 ntp_seconds=4002751452 ntp_fraction=2091649073 arrival_ns=10017404073300
[2026-11-04 11:24:12.487] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2373829209
[2026-11-04 11:24:12.487] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:24:12.487] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2514713460 ntp_seconds=4002751452 ntp_fraction=2087354105 arrival_ns=10017403819000
[2026-11-04 11:24:12.488] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2514713460
[2026-11-04 11:24:12.488] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:24:13.444] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=668 source_master_ns=164483721462 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=725536
[2026-11-04 11:24:14.466] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=712 source_master_ns=165505399467 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=770882
[2026-11-04 11:24:15.487] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=756 source_master_ns=166527077471 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=816228
[2026-11-04 11:24:16.533] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=801 source_master_ns=167571975430 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=861574
[2026-11-04 11:24:17.498] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2373829209 ntp_seconds=4002751457 ntp_fraction=2138893713 arrival_ns=10022415483800
[2026-11-04 11:24:17.499] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2373829209
[2026-11-04 11:24:17.499] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:24:17.513] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2514713460 ntp_seconds=4002751457 ntp_fraction=2203318222 arrival_ns=10022430519900
[2026-11-04 11:24:17.514] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2514713460
[2026-11-04 11:24:17.514] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:24:17.554] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=845 source_master_ns=168593653435 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=906920
[2026-11-04 11:24:18.576] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=889 source_master_ns=169615331439 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=952266
[2026-11-04 11:24:19.597] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=933 source_master_ns=170637009444 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=997612
[2026-11-04 11:24:20.643] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=978 source_master_ns=171681907403 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1042958
[2026-11-04 11:24:21.664] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1022 source_master_ns=172703585408 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1088304
[2026-11-04 11:24:21.665] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=172069164100 present_generation=1 present_canonical_target_ns=166835374149 present_canonical_dispatch_ns=166835374149 present_target_master_ns=172188235917 present_dispatch_master_ns=172188235917
[2026-11-04 11:24:21.670] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=172074195200 present_generation=1 present_canonical_target_ns=166866666666 present_canonical_dispatch_ns=166866666666 present_target_master_ns=172219528434 present_dispatch_master_ns=172219528434
[2026-11-04 11:24:22.499] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2373829209 ntp_seconds=4002751462 ntp_fraction=2143188680 arrival_ns=10027416225500
[2026-11-04 11:24:22.499] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2373829209
[2026-11-04 11:24:22.500] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:24:22.515] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2514713460 ntp_seconds=4002751462 ntp_fraction=2211908157 arrival_ns=10027432305200
[2026-11-04 11:24:22.515] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2514713460
[2026-11-04 11:24:22.516] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:24:22.685] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1066 source_master_ns=173725263412 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1133650
[2026-11-04 11:24:23.731] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1111 source_master_ns=174770161371 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1178996
[2026-11-04 11:24:24.729] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=175133873200 present_generation=1 present_canonical_target_ns=169900408163 present_canonical_dispatch_ns=169900408163 present_target_master_ns=175253269931 present_dispatch_master_ns=175253269931
[2026-11-04 11:24:24.738] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=175142491900 present_generation=1 present_canonical_target_ns=169933333333 present_canonical_dispatch_ns=169933333333 present_target_master_ns=175286195101 present_dispatch_master_ns=175286195101
[2026-11-04 11:24:24.752] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1155 source_master_ns=175791839376 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1224342
[2026-11-04 11:24:24.799] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=175203559500 present_generation=1 present_canonical_target_ns=169970068027 present_canonical_dispatch_ns=169970068027 present_target_master_ns=175322929795 present_dispatch_master_ns=175322929795
[2026-11-04 11:24:24.866] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=175270562200 present_generation=1 present_canonical_target_ns=170033333333 present_canonical_dispatch_ns=170033333333 present_target_master_ns=175386195101 present_dispatch_master_ns=175386195101
[2026-11-04 11:24:24.869] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=175273384200 present_generation=1 present_canonical_target_ns=170039727891 present_canonical_dispatch_ns=170039727891 present_target_master_ns=175392589659 present_dispatch_master_ns=175392589659
[2026-11-04 11:24:24.873] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=175277363500 present_generation=1 present_canonical_target_ns=170066666666 present_canonical_dispatch_ns=170066666666 present_target_master_ns=175419528434 present_dispatch_master_ns=175419528434
[2026-11-04 11:24:25.774] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1199 source_master_ns=176813517380 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1269688
[2026-11-04 11:24:26.796] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1243 source_master_ns=177835195385 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1315034
[2026-11-04 11:24:27.511] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2373829209 ntp_seconds=4002751467 ntp_fraction=2190433320 arrival_ns=10032427791000
[2026-11-04 11:24:27.511] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2373829209
[2026-11-04 11:24:27.511] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:24:27.555] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2514713460 ntp_seconds=4002751467 ntp_fraction=2383706849 arrival_ns=10032472319500
[2026-11-04 11:24:27.555] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2514713460
[2026-11-04 11:24:27.556] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:24:27.841] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1288 source_master_ns=178880093344 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1360380
[2026-11-04 11:24:28.862] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1332 source_master_ns=179901771349 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1405726
[2026-11-04 11:24:29.884] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1376 source_master_ns=180923449353 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1451072
[2026-11-04 11:24:30.929] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1421 source_master_ns=181968347312 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1496418
[2026-11-04 11:24:31.950] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1465 source_master_ns=182990025317 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1541764
[2026-11-04 11:24:32.532] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2373829209 ntp_seconds=4002751472 ntp_fraction=2284922601 arrival_ns=10037448903700
[2026-11-04 11:24:32.532] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2373829209
[2026-11-04 11:24:32.532] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:24:32.590] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2514713460 ntp_seconds=4002751472 ntp_fraction=2534030704 arrival_ns=10037507569900
[2026-11-04 11:24:32.591] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2514713460
[2026-11-04 11:24:32.591] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:24:32.972] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1509 source_master_ns=184011703321 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1587110
[2026-11-04 11:24:33.994] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1553 source_master_ns=185033381326 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1632456
[2026-11-04 11:24:35.039] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1598 source_master_ns=186078279285 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1677802
[2026-11-04 11:24:36.060] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1642 source_master_ns=187099957290 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1723148
[2026-11-04 11:24:37.082] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1686 source_master_ns=188121635294 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1768494
[2026-11-04 11:24:37.544] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2373829209 ntp_seconds=4002751477 ntp_fraction=2336462209 arrival_ns=10042461051600
[2026-11-04 11:24:37.544] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2373829209
[2026-11-04 11:24:37.544] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:24:37.600] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2514713460 ntp_seconds=4002751477 ntp_fraction=2576980377 arrival_ns=10042516952100
[2026-11-04 11:24:37.600] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2514713460
[2026-11-04 11:24:37.600] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:24:38.127] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1731 source_master_ns=189166533253 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1813840
[2026-11-04 11:24:39.149] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1775 source_master_ns=190188211258 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1859186
[2026-11-04 11:24:40.170] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1819 source_master_ns=191209889263 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1904532
[2026-11-04 11:24:41.192] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1863 source_master_ns=192231567267 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1949878
[2026-11-04 11:24:42.237] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1908 source_master_ns=193276465226 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1995224
[2026-11-04 11:24:42.553] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2373829209 ntp_seconds=4002751482 ntp_fraction=2375116914 arrival_ns=10047470032100
[2026-11-04 11:24:42.553] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2373829209
[2026-11-04 11:24:42.553] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:24:42.624] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2514713460 ntp_seconds=4002751482 ntp_fraction=2675764625 arrival_ns=10047540696300
[2026-11-04 11:24:42.624] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2514713460
[2026-11-04 11:24:42.624] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:24:43.259] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1952 source_master_ns=194298143231 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2040570
[2026-11-04 11:24:44.280] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1996 source_master_ns=195319821235 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2085916
[2026-11-04 11:24:45.325] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2041 source_master_ns=196364719195 raw_phase_ns=15874 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2131262
[2026-11-04 11:24:46.347] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2085 source_master_ns=197386397199 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2176608
[2026-11-04 11:24:47.369] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2129 source_master_ns=198408075204 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2221954
[2026-11-04 11:24:47.555] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2373829209 ntp_seconds=4002751487 ntp_fraction=2383706849 arrival_ns=10052472288800
[2026-11-04 11:24:47.556] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2373829209
[2026-11-04 11:24:47.556] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:24:47.648] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2514713460 ntp_seconds=4002751487 ntp_fraction=2778843840 arrival_ns=10052564676700
[2026-11-04 11:24:47.648] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2514713460
[2026-11-04 11:24:47.648] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:24:48.390] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2173 source_master_ns=199429753208 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2267300
[2026-11-04 11:24:49.435] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2218 source_master_ns=200474651167 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2312646
[2026-11-04 11:24:50.457] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2262 source_master_ns=201496329172 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2357992
[2026-11-04 11:24:51.479] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2306 source_master_ns=202518007176 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2403338
[2026-11-04 11:24:52.500] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2350 source_master_ns=203539685181 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2448684
[2026-11-04 11:24:52.557] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2373829209 ntp_seconds=4002751492 ntp_fraction=2392296783 arrival_ns=10057474223700
[2026-11-04 11:24:52.557] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2373829209
[2026-11-04 11:24:52.557] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:24:52.660] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2514713460 ntp_seconds=4002751492 ntp_fraction=2830383448 arrival_ns=10057576574400
[2026-11-04 11:24:52.660] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2514713460
[2026-11-04 11:24:52.660] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:24:53.545] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2395 source_master_ns=204584583140 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2494030
[2026-11-04 11:24:54.567] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2439 source_master_ns=205606261145 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2539376
[2026-11-04 11:24:55.588] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2483 source_master_ns=206627939149 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2584722
[2026-11-04 11:24:56.633] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2528 source_master_ns=207672837108 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2630068
[2026-11-04 11:24:57.583] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2373829209 ntp_seconds=4002751497 ntp_fraction=2503965933 arrival_ns=10062500471900
[2026-11-04 11:24:57.584] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2373829209
[2026-11-04 11:24:57.584] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:24:57.655] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2572 source_master_ns=208694515113 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2675414
[2026-11-04 11:24:57.680] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2514713460 ntp_seconds=4002751497 ntp_fraction=2920577761 arrival_ns=10062597632400
[2026-11-04 11:24:57.681] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2514713460
[2026-11-04 11:24:57.681] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:24:58.677] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2616 source_master_ns=209716193117 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2720760
[2026-11-04 11:24:59.699] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2660 source_master_ns=210737871122 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2766106
[2026-11-04 11:25:00.743] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2705 source_master_ns=211782769081 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2811452
[2026-11-04 11:25:01.765] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2749 source_master_ns=212804447086 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2856798
[2026-11-04 11:25:02.599] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2373829209 ntp_seconds=4002751502 ntp_fraction=2572685410 arrival_ns=10067516624500
[2026-11-04 11:25:02.600] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2373829209
[2026-11-04 11:25:02.600] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:25:02.686] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2514713460 ntp_seconds=4002751502 ntp_fraction=2946347565 arrival_ns=10067602856200
[2026-11-04 11:25:02.686] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2514713460
[2026-11-04 11:25:02.686] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:25:02.787] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2793 source_master_ns=213826125090 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2902144
[2026-11-04 11:25:03.831] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2838 source_master_ns=214871023049 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2947490
[2026-11-04 11:25:04.853] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2882 source_master_ns=215892701054 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2992836
[2026-11-04 11:25:05.875] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2926 source_master_ns=216914379058 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3038182
[2026-11-04 11:25:06.896] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2970 source_master_ns=217936057063 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3083528
[2026-11-04 11:25:07.603] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2373829209 ntp_seconds=4002751507 ntp_fraction=2589865279 arrival_ns=10072519979700
[2026-11-04 11:25:07.603] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2373829209
[2026-11-04 11:25:07.603] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:25:07.689] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2514713460 ntp_seconds=4002751507 ntp_fraction=2959232466 arrival_ns=10072606466600
[2026-11-04 11:25:07.690] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2514713460
[2026-11-04 11:25:07.690] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:25:07.941] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3015 source_master_ns=218980955022 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3128874
[2026-11-04 11:25:08.963] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3059 source_master_ns=220002633027 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3174220
[2026-11-04 11:25:09.985] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3103 source_master_ns=221024311031 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3219566
[2026-11-04 11:25:11.030] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3148 source_master_ns=222069208990 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3264912
[2026-11-04 11:25:12.052] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3192 source_master_ns=223090886995 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3310258
[2026-11-04 11:25:12.401] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=222805009600 present_generation=1 present_canonical_target_ns=217570975056 present_canonical_dispatch_ns=217570975056 present_target_master_ns=222923836824 present_dispatch_master_ns=222923836824
[2026-11-04 11:25:12.425] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=222829329900 present_generation=1 present_canonical_target_ns=217600000000 present_canonical_dispatch_ns=217600000000 present_target_master_ns=222952861768 present_dispatch_master_ns=222952861768
[2026-11-04 11:25:12.447] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=222851056100 present_generation=1 present_canonical_target_ns=217617414965 present_canonical_dispatch_ns=217617414965 present_target_master_ns=222970276733 present_dispatch_master_ns=222970276733
[2026-11-04 11:25:12.467] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=222871263700 present_generation=1 present_canonical_target_ns=217633333333 present_canonical_dispatch_ns=217633333333 present_target_master_ns=222986195101 present_dispatch_master_ns=222986195101
[2026-11-04 11:25:12.470] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=222874493100 present_generation=1 present_canonical_target_ns=217640634920 present_canonical_dispatch_ns=217640634920 present_target_master_ns=222993496688 present_dispatch_master_ns=222993496688
[2026-11-04 11:25:12.477] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=222881449700 present_generation=1 present_canonical_target_ns=217666666666 present_canonical_dispatch_ns=217666666666 present_target_master_ns=223019528434 present_dispatch_master_ns=223019528434
[2026-11-04 11:25:12.630] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2373829209 ntp_seconds=4002751512 ntp_fraction=2705829396 arrival_ns=10077547275400
[2026-11-04 11:25:12.631] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2373829209
[2026-11-04 11:25:12.631] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:25:12.692] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2514713460 ntp_seconds=4002751512 ntp_fraction=2972117368 arrival_ns=10077609398300
[2026-11-04 11:25:12.693] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2514713460
[2026-11-04 11:25:12.693] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:25:13.073] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3236 source_master_ns=224112565000 raw_phase_ns=15874 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3355604
[2026-11-04 11:25:14.095] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3280 source_master_ns=225134243004 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3400950
[2026-11-04 11:25:15.140] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3325 source_master_ns=226179140963 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3446296
[2026-11-04 11:25:16.161] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3369 source_master_ns=227200818968 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3491642
[2026-11-04 11:25:17.183] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3413 source_master_ns=228222496972 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3536988
[2026-11-04 11:25:17.645] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2373829209 ntp_seconds=4002751517 ntp_fraction=2770253905 arrival_ns=10082562101300
[2026-11-04 11:25:17.645] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2373829209
[2026-11-04 11:25:17.646] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:25:17.722] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2514713460 ntp_seconds=4002751517 ntp_fraction=3100966387 arrival_ns=10082638910200
[2026-11-04 11:25:17.722] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2514713460
[2026-11-04 11:25:17.722] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:25:18.228] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3458 source_master_ns=229267394931 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3582334
[2026-11-04 11:25:19.250] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3502 source_master_ns=230289072936 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3627680
[2026-11-04 11:25:20.271] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3546 source_master_ns=231310750941 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3673026
[2026-11-04 11:25:21.293] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3590 source_master_ns=232332428945 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3718372
[2026-11-04 11:25:22.338] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3635 source_master_ns=233377326904 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3763718
[2026-11-04 11:25:22.654] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2373829209 ntp_seconds=4002751522 ntp_fraction=2804613644 arrival_ns=10087570512900
[2026-11-04 11:25:22.654] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2373829209
[2026-11-04 11:25:22.654] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:25:22.745] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2514713460 ntp_seconds=4002751522 ntp_fraction=3199750635 arrival_ns=10087662075300
[2026-11-04 11:25:22.745] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2514713460
[2026-11-04 11:25:22.745] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:25:23.360] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3679 source_master_ns=234399004909 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3809064
[2026-11-04 11:25:24.381] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3723 source_master_ns=235420682913 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3854410
[2026-11-04 11:25:25.426] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3768 source_master_ns=236465580873 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3899756
[2026-11-04 11:25:26.448] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3812 source_master_ns=237487258877 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3945102
[2026-11-04 11:25:27.469] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3856 source_master_ns=238508936882 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3990448
[2026-11-04 11:25:27.665] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2373829209 ntp_seconds=4002751527 ntp_fraction=2856153251 arrival_ns=10092581913400
[2026-11-04 11:25:27.665] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2373829209
[2026-11-04 11:25:27.665] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:25:27.758] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2514713460 ntp_seconds=4002751527 ntp_fraction=3251290243 arrival_ns=10092674723400
[2026-11-04 11:25:27.758] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2514713460
[2026-11-04 11:25:27.758] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:25:28.491] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3900 source_master_ns=239530614886 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4035794
[2026-11-04 11:25:29.536] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3945 source_master_ns=240575512845 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4081140
[2026-11-04 11:25:30.558] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3989 source_master_ns=241597190850 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4126486
[2026-11-04 11:25:31.579] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4033 source_master_ns=242618868854 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4171832
[2026-11-04 11:25:32.624] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4078 source_master_ns=243663766814 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4217178
[2026-11-04 11:25:32.691] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2373829209 ntp_seconds=4002751532 ntp_fraction=2967822401 arrival_ns=10097608044000
[2026-11-04 11:25:32.692] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2373829209
[2026-11-04 11:25:32.692] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:25:32.785] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2514713460 ntp_seconds=4002751532 ntp_fraction=3367254360 arrival_ns=10097701708200
[2026-11-04 11:25:32.785] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2514713460
[2026-11-04 11:25:32.785] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:25:33.646] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4122 source_master_ns=244685444818 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4262524
[2026-11-04 11:25:34.667] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4166 source_master_ns=245707122823 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4307870
[2026-11-04 11:25:35.690] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4210 source_master_ns=246728800827 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4353216
[2026-11-04 11:25:36.734] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4255 source_master_ns=247773698786 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4398562
[2026-11-04 11:25:37.702] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2373829209 ntp_seconds=4002751537 ntp_fraction=3015067041 arrival_ns=10102619415600
[2026-11-04 11:25:37.703] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2373829209
[2026-11-04 11:25:37.703] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:25:37.756] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4299 source_master_ns=248795376791 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4443908
[2026-11-04 11:25:37.826] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2514713460 ntp_seconds=4002751537 ntp_fraction=3543348019 arrival_ns=10102742329900
[2026-11-04 11:25:37.826] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2514713460
[2026-11-04 11:25:37.827] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:25:38.778] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4343 source_master_ns=249817054795 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4489254
[2026-11-04 11:25:39.822] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4388 source_master_ns=250861952755 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4534600
[2026-11-04 11:25:40.845] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4432 source_master_ns=251883630759 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4579946
[2026-11-04 11:25:41.867] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4476 source_master_ns=252905308764 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4625292
[2026-11-04 11:25:42.714] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2373829209 ntp_seconds=4002751542 ntp_fraction=3062311682 arrival_ns=10107630664200
[2026-11-04 11:25:42.715] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2373829209
[2026-11-04 11:25:42.715] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:25:42.869] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2514713460 ntp_seconds=4002751542 ntp_fraction=3732326580 arrival_ns=10107786266900
[2026-11-04 11:25:42.870] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2514713460
[2026-11-04 11:25:42.870] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:25:42.887] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4520 source_master_ns=253926986768 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4670638
[2026-11-04 11:25:43.932] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4565 source_master_ns=254971884727 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4715984
[2026-11-04 11:25:44.954] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4609 source_master_ns=255993562732 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4761330
[2026-11-04 11:25:45.976] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4653 source_master_ns=257015240736 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4806676
[2026-11-04 11:25:46.997] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4697 source_master_ns=258036918741 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4852022
[2026-11-04 11:25:47.717] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2373829209 ntp_seconds=4002751547 ntp_fraction=3079491551 arrival_ns=10112634574000
[2026-11-04 11:25:47.718] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2373829209
[2026-11-04 11:25:47.718] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:25:47.888] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2514713460 ntp_seconds=4002751547 ntp_fraction=3809635991 arrival_ns=10112804534600
[2026-11-04 11:25:47.888] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2514713460
[2026-11-04 11:25:47.889] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:25:48.044] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4742 source_master_ns=259081816700 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4897368
[2026-11-04 11:25:49.065] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4786 source_master_ns=260103494705 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4942714
[2026-11-04 11:25:50.086] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4830 source_master_ns=261125172709 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4988060
[2026-11-04 11:25:51.131] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4875 source_master_ns=262170070668 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5033406
[2026-11-04 11:25:52.152] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4919 source_master_ns=263191748673 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5078752
[2026-11-04 11:25:52.729] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=2373829209 ntp_seconds=4002751552 ntp_fraction=3131031158 arrival_ns=10117646585400
[2026-11-04 11:25:52.730] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=2373829209
[2026-11-04 11:25:52.731] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:25:52.930] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2514713460 ntp_seconds=4002751552 ntp_fraction=3994319585 arrival_ns=10117847049500
[2026-11-04 11:25:52.930] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2514713460
[2026-11-04 11:25:52.930] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-04 11:25:53.174] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4963 source_master_ns=264213426678 raw_phase_ns=15874 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5124098
[2026-11-04 11:25:54.196] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=5007 source_master_ns=265235104682 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5169444
[2026-11-04 11:25:55.241] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=5052 source_master_ns=266280002641 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5214790
[2026-11-04 11:25:56.262] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=5096 source_master_ns=267301680646 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5260136
[2026-11-04 11:25:56.931] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=2 reason=1 observed_at_ns=10121847717800
[2026-11-04 11:25:56.931] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=3 generation=3 invalidated_generation=2 locked=0
[2026-11-04 11:25:56.932] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=50 group=composition-r381.source.2 buffered=0 phase=0 preserves_output=1 protocol_generation=2 evidence_revision=94 video_raw_generation=1 video_ssrc=2514713460 audio_raw_generation=1 audio_ssrc=2373829209
[2026-11-04 11:25:56.932] [info] [graph][runtime.node] rtp_ingress_purge node=46 transition=2 cutoff=0
[2026-11-04 11:25:56.933] [info] [graph][runtime.node] rtp_binder_purge node=50 transition=2 cutoff=7191
[2026-11-04 11:25:56.932] [info] [graph][runtime.node] rtp_ingress_purge node=47 transition=2 cutoff=0
[2026-11-04 11:25:56.932] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=51 group=composition-r381.source.2 buffered=0 phase=0 preserves_output=1 protocol_generation=2 evidence_revision=94 video_raw_generation=1 video_ssrc=2514713460 audio_raw_generation=1 audio_ssrc=2373829209
[2026-11-04 11:25:56.933] [info] [graph][runtime.node] rtp_binder_purge node=51 transition=2 cutoff=10345
[2026-11-04 11:25:56.933] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=2 reason=1 observed_at_ns=10121849138400
[2026-11-04 11:25:56.934] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=3 generation=3 invalidated_generation=2 locked=0
[2026-11-04 11:25:56.934] [info] [graph][runtime.node] av_reacquisition group=composition-r381.source.2 event=purge_completed purge_ack=complete continuation=acquiring ack_count=2 old=2 next=3 transition=2
[2026-11-04 11:25:56.932] [info] [graph][runtime.node] av_reacquisition group=composition-r381.source.2 phase=purging old=2 next=3 transition=2
[2026-11-04 11:26:21.006] [info] [graph][runtime.lifecycle] continuous_output_drain stage=requested
[2026-11-04 11:26:21.006] [info] [graph][runtime.lifecycle] worker.finished node=80
[2026-11-04 11:26:21.006] [info] [graph][runtime.lifecycle] worker.finished node=77
[2026-11-04 11:26:21.006] [info] [graph][runtime.node] worker.thread_cpu node=77 available=1 cpu_ns=0 process_calls=8
[2026-11-04 11:26:21.006] [info] [graph][runtime.lifecycle] worker.finished node=4
[2026-11-04 11:26:21.006] [info] [graph][runtime.node] worker.thread_cpu node=4 available=1 cpu_ns=6062500000 process_calls=38978
[2026-11-04 11:26:21.006] [info] [graph][runtime.node] worker.thread_cpu node=80 available=1 cpu_ns=0 process_calls=6
[2026-11-04 11:26:21.006] [info] [graph][runtime.lifecycle] worker.finished node=2
[2026-11-04 11:26:21.006] [info] [graph][runtime.lifecycle] worker.finished node=5
[2026-11-04 11:26:21.007] [info] [graph][runtime.node] worker.thread_cpu node=5 available=1 cpu_ns=406250000 process_calls=36990
[2026-11-04 11:26:21.007] [info] [graph][runtime.node] worker.thread_cpu node=2 available=1 cpu_ns=3218750000 process_calls=25757
[2026-11-04 11:26:21.030] [info] [graph][runtime.lifecycle] worker.finished node=74
[2026-11-04 11:26:21.030] [info] [graph][runtime.node] worker.thread_cpu node=74 available=1 cpu_ns=703125000 process_calls=62338
[2026-11-04 11:26:21.030] [info] [graph][runtime.lifecycle] worker.finished node=78
[2026-11-04 11:26:21.030] [info] [graph][runtime.node] worker.thread_cpu node=78 available=1 cpu_ns=453125000 process_calls=61791
[2026-11-04 11:26:21.030] [info] [graph][runtime.lifecycle] worker.finished node=75
[2026-11-04 11:26:21.031] [info] [graph][runtime.node] worker.thread_cpu node=75 available=1 cpu_ns=359375000 process_calls=62744
[2026-11-04 11:26:21.058] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 produced_datagrams=174235 produced_ts_payload_bytes=203709468 pending_bytes=0 peak_pending_bytes=110920 access_units=20914 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=32789115 maximum_mux_input_after_emit_ns=41721505 maximum_mux_input_after_dispatch_ns=-58278495 worst_mux_input_ready_ns=175264651300 worst_mux_input_emit_ns=175222929795 worst_mux_input_dispatch_ns=175322929795 maximum_protocol_batch_produced_after_release_ns=37288232 worst_protocol_batch_produced_ns=55290151000 worst_protocol_batch_release_ns=55252862768 worst_protocol_batch_deadline_ns=55319529436 worst_protocol_batch_datagrams=1 exit_reason=completed
[2026-11-04 11:26:21.058] [info] [graph][runtime.lifecycle] worker.finished node=76
[2026-11-04 11:26:21.058] [info] [graph][runtime.lifecycle] worker.finished node=79
[2026-11-04 11:26:21.059] [info] [graph][runtime.node] worker.thread_cpu node=79 available=1 cpu_ns=2031250000 process_calls=95128
[2026-11-04 11:26:21.059] [info] [graph][runtime.node] datagram_batch_trace stage=last_completed generation=1 first_sequence=174235 last_sequence=174235 maximum_submit_interval_ns=73600 maximum_submit_interval_sequence=174235 maximum_start_lateness_ns=520535 maximum_start_lateness_sequence=174235 first_release_master_ns=291462022765 first_deadline_master_ns=291562022765 last_release_master_ns=291462022765 last_deadline_master_ns=291562022765 first_submitted_master_ns=291462616900 last_submitted_master_ns=291462616900 worst_wait_started_master_ns=291453773400 worst_wait_completed_master_ns=291462541000 worst_submit_observed_master_ns=291462543300 wait_attempts=1 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=1 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=1 wait_call_total_ns=8764800 wait_call_max_ns=8764800 wait_before_call_count=1 wait_before_call_total_ns=100 wait_before_call_max_ns=100 wait_arm_count=1 wait_arm_total_ns=1700 wait_arm_max_ns=1700 wait_api_wall_count=1 wait_api_wall_total_ns=8761600 wait_api_wall_max_ns=8761600 wait_return_to_next_clock_count=1 wait_return_to_next_clock_total_ns=2800 wait_return_to_next_clock_max_ns=2800 wait_target_lateness_count=1 wait_target_lateness_total_ns=518235 wait_target_lateness_max_ns=518235 worst_wait_metric=call_wall worst_wait_sequence=174235 worst_wait_target_master_ns=291462022765 worst_wait_sampled_master_ns=291453773400 worst_wait_sampled_steady_ns=10145966599100 worst_wait_timeout_ns=8249365 worst_wait_entered_steady_ns=10145966599200 worst_wait_returned_steady_ns=10145975364000 worst_wait_arm_before_steady_ns=10145966599400 worst_wait_arm_after_steady_ns=10145966601100 worst_wait_api_before_steady_ns=10145966601100 worst_wait_api_returned_steady_ns=10145975362700 worst_wait_windows_due_100ns=-82494 worst_wait_windows_result=1 worst_wait_windows_error=unavailable worst_wait_windows_pid=25140 worst_wait_windows_tid=12652 worst_wait_windows_qpc_frequency_hz=10000000 worst_wait_next_master_ns=291462541000 worst_wait_next_steady_ns=10145975366800 worst_wait_outcome=deadline
[2026-11-04 11:26:21.059] [info] [graph][runtime.node] worker.thread_cpu node=76 available=1 cpu_ns=3218750000 process_calls=126812
[2026-11-04 11:26:21.059] [info] [graph][runtime.node] datagram_successful_submit_trace stage=worst_success generation=1 endpoint=1 sequence=164661 pacing_lateness_ns=19503940 matches_pacing_worst=1 materialized_master_ns=unavailable enqueued_master_ns=unavailable submit_interval_scope=pre_transport_checks_and_transport socket_only_ns=unavailable release_master_ns=257886195101 deadline_master_ns=257986165596 original_protocol_deadline_master_ns=257986195101 prefix_completion_limit_master_ns=257986165596 reserved_at_master_ns=257908709000 not_before_master_ns=257889208060 not_after_master_ns=257985950556 service_duration_ns=215040 wire_bytes_per_second=6250000 submit_started_master_ns=257908712000 submit_completed_master_ns=257908743500 activation_started_master_ns=257908698700 activation_completed_master_ns=257908708600 permit_started_master_ns=257908708800 permit_completed_master_ns=257908708900 snapshot_started_master_ns=257908708900 snapshot_sampled_master_ns=257908709000 snapshot_completed_master_ns=257908709200 reservation_completed_master_ns=257908711000 wait_started_master_ns=257908711600 wait_completed_master_ns=257908711600 submit_observed_master_ns=257908712000 wait_attempts=0 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=0 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=0 wait_call_total_ns=0 wait_call_max_ns=0 wait_before_call_count=0 wait_before_call_total_ns=0 wait_before_call_max_ns=0 wait_arm_count=0 wait_arm_total_ns=0 wait_arm_max_ns=0 wait_api_wall_count=0 wait_api_wall_total_ns=0 wait_api_wall_max_ns=0 wait_return_to_next_clock_count=0 wait_return_to_next_clock_total_ns=0 wait_return_to_next_clock_max_ns=0 wait_target_lateness_count=0 wait_target_lateness_total_ns=0 wait_target_lateness_max_ns=0
[2026-11-04 11:26:21.059] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=30941 committed_datagrams=174235 committed_payload_bytes=203709468 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=79296 pacing_reserved=174235 pacing_cancelled=0 pacing_submitted=174235 pacing_maximum_submit_lateness_ns=19503940 pacing_worst_late_global_sequence=164661 pacing_worst_success_sample_available=1 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=79296 backlog_max_residence_ns=50849200 last_materialized_sequence=174235 last_scheduled_sequence=174235 last_submitted_sequence=174235 last_committed_sequence=174235 submitted_service_generation=1 submitted_service_sequence=174235 submitted_service_completed_ns=291462616900 submitted_service_duration_ns=34560 submitted_service_available_ns=291462651460 submitted_scope_available_ns=291462651460 shared_scope_charged_datagrams=174235 shared_scope_charged_wire_bytes=208588048 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2154 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2154 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=188010 endpoint_1_target_effective_socket_bytes=188010 endpoint_1_api_requested_socket_bytes=188010 endpoint_1_effective_socket_bytes=188010 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=174235 endpoint_1_committed_payload_bytes=203709468
[2026-11-04 11:26:21.059] [info] [graph][runtime.lifecycle] worker.finished node=81
[2026-11-04 11:26:21.059] [info] [graph][runtime.node] worker.thread_cpu node=81 available=1 cpu_ns=10187500000 process_calls=62018
[2026-11-04 11:26:21.059] [info] [graph][runtime.lifecycle] distribution_registration scope=worker stage=leave node=81 pid=25140 tid=12652 native=0
[2026-11-04 11:26:21.268] [info] [graph][runtime.lifecycle] continuous_output_drain stage=completed
[2026-11-04 11:26:21.268] [info] [graph][runtime.lifecycle] stop.begin
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=46 available=1 cpu_ns=750000000 process_calls=182365
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=8 available=1 cpu_ns=234375000 process_calls=80441
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=27 available=1 cpu_ns=421875000 process_calls=79771
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=47 available=1 cpu_ns=734375000 process_calls=171641
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=9 available=1 cpu_ns=0 process_calls=78
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=28 available=1 cpu_ns=15625000 process_calls=80
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=48 available=1 cpu_ns=15625000 process_calls=303
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=10 available=1 cpu_ns=0 process_calls=104
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=29 available=1 cpu_ns=0 process_calls=106
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=49 available=1 cpu_ns=15625000 process_calls=471
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=11 available=1 cpu_ns=93750000 process_calls=10560
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=12 available=1 cpu_ns=0 process_calls=81
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=30 available=1 cpu_ns=140625000 process_calls=10578
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=41 available=1 cpu_ns=1203125000 process_calls=14436
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=31 available=1 cpu_ns=0 process_calls=80
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=50 available=1 cpu_ns=156250000 process_calls=21389
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=64 available=1 cpu_ns=2312500000 process_calls=34922
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=51 available=1 cpu_ns=390625000 process_calls=30688
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=52 available=1 cpu_ns=15625000 process_calls=306
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=70 available=1 cpu_ns=1078125000 process_calls=42428
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=32 available=1 cpu_ns=0 process_calls=104
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=23 available=1 cpu_ns=15625000 process_calls=10531
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=13 available=1 cpu_ns=0 process_calls=106
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=53 available=1 cpu_ns=15625000 process_calls=454
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=42 available=1 cpu_ns=15625000 process_calls=10541
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=14 available=1 cpu_ns=62500000 process_calls=10718
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=17 available=1 cpu_ns=0 process_calls=79
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=65 available=1 cpu_ns=203125000 process_calls=21319
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=33 available=1 cpu_ns=78125000 process_calls=10686
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=36 available=1 cpu_ns=0 process_calls=79
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=54 available=1 cpu_ns=281250000 process_calls=21616
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=71 available=1 cpu_ns=234375000 process_calls=31396
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=55 available=1 cpu_ns=250000000 process_calls=31008
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=59 available=1 cpu_ns=15625000 process_calls=301
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=73 available=1 cpu_ns=750000000 process_calls=50138
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=15 available=1 cpu_ns=93750000 process_calls=10669
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=34 available=1 cpu_ns=125000000 process_calls=10682
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=25 available=1 cpu_ns=109375000 process_calls=10605
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=56 available=1 cpu_ns=171875000 process_calls=21516
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=57 available=1 cpu_ns=375000000 process_calls=30761
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=44 available=1 cpu_ns=46875000 process_calls=10575
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=16 available=1 cpu_ns=78125000 process_calls=10741
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=35 available=1 cpu_ns=62500000 process_calls=10747
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=67 available=1 cpu_ns=312500000 process_calls=21262
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=18 available=1 cpu_ns=93750000 process_calls=10630
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=58 available=1 cpu_ns=656250000 process_calls=52305
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=60 available=1 cpu_ns=578125000 process_calls=51354
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=19 available=1 cpu_ns=203125000 process_calls=10615
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=37 available=1 cpu_ns=156250000 process_calls=10648
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=38 available=1 cpu_ns=171875000 process_calls=10662
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=61 available=1 cpu_ns=687500000 process_calls=51046
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=20 available=1 cpu_ns=78125000 process_calls=10588
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=39 available=1 cpu_ns=109375000 process_calls=10573
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=62 available=1 cpu_ns=500000000 process_calls=51361
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=22 available=1 cpu_ns=1515625000 process_calls=14177
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=24 available=1 cpu_ns=109375000 process_calls=11513
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=43 available=1 cpu_ns=140625000 process_calls=11341
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=66 available=1 cpu_ns=234375000 process_calls=22356
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=72 available=1 cpu_ns=390625000 process_calls=31354
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=26 available=1 cpu_ns=1093750000 process_calls=13702
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=45 available=1 cpu_ns=953125000 process_calls=13699
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=68 available=1 cpu_ns=1406250000 process_calls=25465
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=6 available=1 cpu_ns=2234375000 process_calls=49605
[2026-11-04 11:26:21.269] [info] [graph][runtime.node] worker.thread_cpu node=7 available=1 cpu_ns=1625000000 process_calls=49017
[2026-11-04 11:26:21.276] [info] [graph][runtime.node] mpegts_wire_materializer stage=stopped maximum_materialized_after_release_ns=37468732 worst_materialized_at_ns=55290331500 worst_release_ns=55252862768 worst_deadline_ns=55319529436 worst_global_sequence=35711
[2026-11-04 11:26:21.276] [info] [graph][runtime.node] audio_stage_observation node=74 stage=scheduler_input count=12327 last_generation=1 last_begin=12621824 last_end=12622848 last_rate=44100 last_steady_ns=10145923834500 maximum_inter_event_gap_ns=61037900 gap_begin_generation=1 gap_begin_begin=7495680 gap_begin_end=7496704 gap_begin_rate=44100 gap_begin_steady_ns=10029716348600 gap_end_generation=1 gap_end_begin=7496704 gap_end_end=7497728 gap_end_rate=44100 gap_end_steady_ns=10029777386500
[2026-11-04 11:26:21.276] [info] [graph][runtime.node] av_scheduler_correlated_wait generation=1 canonical_sequence=7321 missing=video begin_steady_ns=10029716384800 end_steady_ns=10029777345700 observed_wait_ns=60960900 held_dispatch_master_ns=175322929795 begin_master_ns=175203558900 end_master_ns=175264519000 dispatch_remaining_at_end_ns=58410795 resumed_dispatch_master_ns=175352861768
[2026-11-04 11:26:21.276] [info] [graph][runtime.node] audio_stage_observation node=5 stage=canonicalizer_input count=12327 last_generation=1 last_begin=12621824 last_end=12622848 last_rate=44100 last_steady_ns=10145923252000 maximum_inter_event_gap_ns=40197700 gap_begin_generation=1 gap_begin_begin=5386240 gap_begin_end=5387264 gap_begin_rate=44100 gap_begin_steady_ns=9981883116400 gap_end_generation=1 gap_end_begin=5387264 gap_end_end=5388288 gap_end_rate=44100 gap_end_steady_ns=9981923314100
[2026-11-04 11:26:21.277] [info] [graph][runtime.node] audio_stage_observation node=5 stage=canonicalizer_committed count=12327 last_generation=1 last_begin=12621824 last_end=12622848 last_rate=44100 last_steady_ns=10145923258100 maximum_inter_event_gap_ns=40200200 gap_begin_generation=1 gap_begin_begin=5386240 gap_begin_end=5387264 gap_begin_rate=44100 gap_begin_steady_ns=9981883131000 gap_end_generation=1 gap_end_begin=5387264 gap_end_end=5388288 gap_end_rate=44100 gap_end_steady_ns=9981923331200
[2026-11-04 11:26:21.277] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_input count=12327 last_generation=1 last_begin=12621824 last_end=12622848 last_rate=44100 last_steady_ns=10145908613800 maximum_inter_event_gap_ns=40152200 gap_begin_generation=1 gap_begin_begin=5388288 gap_begin_end=5389312 gap_begin_rate=44100 gap_begin_steady_ns=9981882979700 gap_end_generation=1 gap_end_begin=5389312 gap_end_end=5390336 gap_end_rate=44100 gap_end_steady_ns=9981923131900
[2026-11-04 11:26:21.277] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_submitted count=12327 last_generation=1 last_begin=12621824 last_end=12622848 last_rate=44100 last_steady_ns=10145908691100 maximum_inter_event_gap_ns=40190300 gap_begin_generation=1 gap_begin_begin=5388288 gap_begin_end=5389312 gap_begin_rate=44100 gap_begin_steady_ns=9981883086000 gap_end_generation=1 gap_end_begin=5389312 gap_end_end=5390336 gap_end_rate=44100 gap_end_steady_ns=9981923276300
[2026-11-04 11:26:21.277] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_packet count=12327 last_generation=1 last_begin=12621824 last_end=12622848 last_rate=44100 last_steady_ns=10145923221700 maximum_inter_event_gap_ns=40192200 gap_begin_generation=1 gap_begin_begin=5386240 gap_begin_end=5387264 gap_begin_rate=44100 gap_begin_steady_ns=9981883093900 gap_end_generation=1 gap_end_begin=5387264 gap_end_end=5388288 gap_end_rate=44100 gap_end_steady_ns=9981923286100
[2026-11-04 11:26:21.277] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_committed count=12327 last_generation=1 last_begin=12621824 last_end=12622848 last_rate=44100 last_steady_ns=10145923225800 maximum_inter_event_gap_ns=40194700 gap_begin_generation=1 gap_begin_begin=5386240 gap_begin_end=5387264 gap_begin_rate=44100 gap_begin_steady_ns=9981883102700 gap_end_generation=1 gap_end_begin=5387264 gap_end_end=5388288 gap_end_rate=44100 gap_end_steady_ns=9981923297400
[2026-11-04 11:26:21.277] [info] [graph][runtime.lifecycle] codec_owner_release node=4 kind=AudioEncode owner_references=4
[2026-11-04 11:26:21.277] [info] [graph][runtime.node] audio_stage_observation node=7 stage=aggregate_built count=12327 last_generation=1 last_begin=12621824 last_end=12622848 last_rate=44100 last_steady_ns=10145908586400 maximum_inter_event_gap_ns=40141700 gap_begin_generation=1 gap_begin_begin=5388288 gap_begin_end=5389312 gap_begin_rate=44100 gap_begin_steady_ns=9981882913900 gap_end_generation=1 gap_end_begin=5389312 gap_end_end=5390336 gap_end_rate=44100 gap_end_steady_ns=9981923055600
[2026-11-04 11:26:21.277] [info] [graph][runtime.node] audio_stage_observation node=7 stage=aggregate_committed count=12327 last_generation=1 last_begin=12621824 last_end=12622848 last_rate=44100 last_steady_ns=10145908597800 maximum_inter_event_gap_ns=40204500 gap_begin_generation=1 gap_begin_begin=5388288 gap_begin_end=5389312 gap_begin_rate=44100 gap_begin_steady_ns=9981883152100 gap_end_generation=1 gap_end_begin=5389312 gap_end_end=5390336 gap_end_rate=44100 gap_end_steady_ns=9981923356600
[2026-11-04 11:26:21.277] [info] [graph][runtime.node] continuous_audio_aggregate.summary reason=stop output_group=composition-r381.output audio_commits=12327 real_samples=10245120 silence_samples=2377728
[2026-11-04 11:26:21.277] [info] [graph][runtime.lifecycle] video_encode.zero_copy_summary drm_prime=0 software_frame=0
[2026-11-04 11:26:21.277] [info] [graph][runtime.lifecycle] codec_owner_release node=2 kind=VideoEncode owner_references=4
[2026-11-04 11:26:21.277] [info] [graph][runtime.node] video_encoder_execution_observation node=2 count=8587 maximum_call_ns=41641200 maximum_begin_steady_ns=10077342324300 maximum_end_steady_ns=10077383965500 maximum_encoder_pts=6529 maximum_native_result=0 maximum_between_submissions_ns=66378800 before_gap_begin_steady_ns=10029667236400 before_gap_end_steady_ns=10029672648000 before_gap_encoder_pts=5099 before_gap_native_result=0 after_gap_begin_steady_ns=10029739026800 after_gap_end_steady_ns=10029777285100 after_gap_encoder_pts=5100 after_gap_native_result=0
[2026-11-04 11:26:21.277] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=process count=49605 maximum_ns=39445600 begin_steady_ns=10029699530100 end_steady_ns=10029738975700 video_cursor=5100 audio_cursor=not_owned next_video_cursor=5100 next_audio_cursor=not_owned error=None return_state=progress deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-11-04 11:26:21.278] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=purge count=49605 maximum_ns=2605400 begin_steady_ns=9973568487500 end_steady_ns=9973571092900 video_cursor=3417 audio_cursor=not_owned next_video_cursor=3417 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-04 11:26:21.278] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=bind count=49600 maximum_ns=219600 begin_steady_ns=9926367370000 end_steady_ns=9926367589600 video_cursor=2001 audio_cursor=not_owned next_video_cursor=2001 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-04 11:26:21.278] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=consume count=49600 maximum_ns=10132900 begin_steady_ns=9911934573000 end_steady_ns=9911944705900 video_cursor=1568 audio_cursor=not_owned next_video_cursor=1568 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-04 11:26:21.278] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=activate count=5 maximum_ns=291700 begin_steady_ns=9859699020200 end_steady_ns=9859699311900 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-04 11:26:21.278] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=build_video count=8588 maximum_ns=39337300 begin_steady_ns=10029699587000 end_steady_ns=10029738924300 video_cursor=5100 audio_cursor=not_owned next_video_cursor=5100 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-04 11:26:21.278] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=canvas count=8588 maximum_ns=39287600 begin_steady_ns=10029699604300 end_steady_ns=10029738891900 video_cursor=5100 audio_cursor=not_owned next_video_cursor=5100 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-04 11:26:21.278] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=emit count=8589 maximum_ns=8498400 begin_steady_ns=9976300567800 end_steady_ns=9976309066200 video_cursor=3498 audio_cursor=not_owned next_video_cursor=3498 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-04 11:26:21.278] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=between_calls_previous_return count=49604 maximum_ns=136741900 begin_steady_ns=9859556638500 end_steady_ns=9859693380400 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=waiting deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-11-04 11:26:21.278] [info] [graph][runtime.node] continuous_aggregate.summary reason=stop output_group=composition-r381.output output_generation=1 video_commits=8587 source[0].group=composition-r381.source.0 source[0].identity=composition-r381.source.0.input.video source[0].real_frames=3416 source[0].black_frames=5171 source[0].contribution_generation=0 source[1].group=composition-r381.source.1 source[1].identity=composition-r381.source.1.input.video source[1].real_frames=3417 source[1].black_frames=5170 source[1].contribution_generation=0 source[2].group=composition-r381.source.2 source[2].identity=composition-r381.source.2.input.video source[2].real_frames=6970 source[2].black_frames=1617 source[2].contribution_generation=0
[2026-11-04 11:26:21.278] [info] [graph][runtime.node] cuda_canvas_call stage=context_push count=6975 operation=copy maximum_ns=472400 begin_steady_ns=10106465937700 end_steady_ns=10106466410100 native_result=0 thread_cpu_ns=0
[2026-11-04 11:26:21.278] [info] [graph][runtime.node] cuda_canvas_call stage=source_wait count=20783 operation=copy maximum_ns=340400 begin_steady_ns=10088532744600 end_steady_ns=10088533085000 native_result=0 thread_cpu_ns=0
[2026-11-04 11:26:21.278] [info] [graph][runtime.node] cuda_canvas_call stage=plane_copy count=41568 operation=copy maximum_ns=19424300 begin_steady_ns=10111837430300 end_steady_ns=10111856854600 native_result=0 thread_cpu_ns=0
[2026-11-04 11:26:21.278] [info] [graph][runtime.node] cuda_canvas_call stage=destination_wait count=2 operation=upload maximum_ns=79600 begin_steady_ns=9859468234300 end_steady_ns=9859468313900 native_result=0 thread_cpu_ns=0
[2026-11-04 11:26:21.278] [info] [graph][runtime.node] cuda_canvas_call stage=context_pop count=6975 operation=copy maximum_ns=185600 begin_steady_ns=10114800993900 end_steady_ns=10114801179500 native_result=0 thread_cpu_ns=0
[2026-11-04 11:26:21.278] [info] [graph][runtime.node] cuda_canvas_call stage=batch_completion count=6973 operation=copy maximum_ns=38974000 begin_steady_ns=10029699877100 end_steady_ns=10029738851100 native_result=0 thread_cpu_ns=0
[2026-11-04 11:26:21.279] [info] [graph][runtime.node] canvas_compose_correlated scope=attempt begin_steady_ns=10029699605400 end_steady_ns=10029738890900 wall_ns=39285500 thread_cpu_ns=0 batch_pid=25140 batch_tid=8332 batch_context=2445857407872 batch_stream=2444151997120 batch_submit_begin_steady_ns=10029699632400 batch_submit_end_steady_ns=10029699877100 batch_wait_begin_steady_ns=10029699877400 batch_wait_end_steady_ns=10029738848100 batch_timing_preparation=ready batch_timing_preparation_native=0 batch_start_event_native=0 batch_end_event_native=0 batch_sync_native=0 batch_elapsed_native=0 batch_gpu_elapsed_available=1 batch_gpu_elapsed_ms=29.6161 batch_gpu_interval=event_execution_interval_excludes_pre_start_queue surface_count=1 surface_total_ns=1600 surface_maximum_ns=1600 surface_maximum_tile=none surface_maximum_result=unavailable validate_count=1 validate_total_ns=1800 validate_maximum_ns=1800 validate_maximum_tile=2 validate_maximum_result=0 black_count=1 black_total_ns=81000 black_maximum_ns=81000 black_maximum_tile=none black_maximum_result=0 tile_count=1 tile_total_ns=35100 tile_maximum_ns=35100 tile_maximum_tile=2 tile_maximum_result=0 publish_count=1 publish_total_ns=15200 publish_maximum_ns=15200 publish_maximum_tile=none publish_maximum_result=0 context_push_count=1 context_push_total_ns=5400 context_push_maximum_ns=5400 context_push_maximum_tile=none context_push_maximum_result=0 source_wait_count=2 source_wait_total_ns=3800 source_wait_maximum_ns=3600 source_wait_maximum_tile=2 source_wait_maximum_result=0 plane_copy_count=4 plane_copy_total_ns=96500 plane_copy_maximum_ns=66700 plane_copy_maximum_tile=none plane_copy_maximum_result=0 destination_wait_count=0 destination_wait_total_ns=0 destination_wait_maximum_ns=0 destination_wait_maximum_tile=none destination_wait_maximum_result=unavailable context_pop_count=1 context_pop_total_ns=7200 context_pop_maximum_ns=7200 context_pop_maximum_tile=none context_pop_maximum_result=0 batch_completion_count=1 batch_completion_total_ns=38970900 batch_completion_maximum_ns=38970900 batch_completion_maximum_tile=none batch_completion_maximum_result=0 source_query_completed_count=0 source_query_ready_count=1 source_query_ready_maximum_ns=9600 source_query_ready_begin_steady_ns=10029699834400 source_query_ready_end_steady_ns=10029699844000 source_query_ready_tile=2 source_query_ready_operation=event_query source_query_ready_native=0 source_query_ready_restore_native=0 source_query_pending_count=0 source_query_unavailable_count=0 source_query_error_count=0
[2026-11-04 11:26:21.279] [info] [graph][runtime.node] source_video_trace_summary node_id=67 node=VideoPreparationNode generation=2 reason=stop steady_ns=10146195996300 input_count=3569 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3569 published_count=3568 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-04 11:26:21.279] [info] [graph][runtime.node] source_video_trace_summary node_id=44 node=VideoPreparationNode generation=1 reason=stop steady_ns=10146196079000 input_count=3438 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3438 published_count=3429 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=7 incomplete=0
[2026-11-04 11:26:21.279] [info] [graph][runtime.node] source_video_trace_summary node_id=25 node=VideoPreparationNode generation=1 reason=stop steady_ns=10146196153000 input_count=3430 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3430 published_count=3429 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-04 11:26:21.279] [info] [graph][runtime.lifecycle] codec_owner_release node=73 kind=AudioResample owner_references=1
[2026-11-04 11:26:21.279] [info] [graph][runtime.lifecycle] video_filter.zero_copy_summary drm_prime_input=0 drm_prime_output=0 rga=0 software_frame=0
[2026-11-04 11:26:21.279] [info] [graph][runtime.node] source_video_trace_summary node_id=68 node=VideoFilterNode generation=2 reason=stop steady_ns=10146196551600 input_count=3569 accepted_count=3569 would_block_count=0 resolved_count=3569 output_attempt_count=0 published_count=3569 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-04 11:26:21.286] [info] [graph][runtime.lifecycle] video_filter.zero_copy_summary drm_prime_input=0 drm_prime_output=0 rga=0 software_frame=0
[2026-11-04 11:26:21.286] [info] [graph][runtime.node] source_video_trace_summary node_id=45 node=VideoFilterNode generation=1 reason=stop steady_ns=10146203513500 input_count=3445 accepted_count=3445 would_block_count=0 resolved_count=3445 output_attempt_count=0 published_count=3445 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:26:21.291] [info] [graph][runtime.lifecycle] video_filter.zero_copy_summary drm_prime_input=0 drm_prime_output=0 rga=0 software_frame=0
[2026-11-04 11:26:21.291] [info] [graph][runtime.node] source_video_trace_summary node_id=26 node=VideoFilterNode generation=1 reason=stop steady_ns=10146208377100 input_count=3437 accepted_count=3437 would_block_count=0 resolved_count=3437 output_attempt_count=0 published_count=3436 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-04 11:26:21.296] [info] [graph][runtime.node] source_video_trace_summary node_id=66 node=VideoFrameRateNode generation=2 reason=stop steady_ns=10146213688800 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3569 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:26:21.297] [info] [graph][runtime.node] source_video_trace_summary node_id=43 node=VideoFrameRateNode generation=1 reason=stop steady_ns=10146213825600 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3445 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:26:21.297] [info] [graph][runtime.node] source_video_trace_summary node_id=24 node=VideoFrameRateNode generation=1 reason=stop steady_ns=10146213907500 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3443 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:26:21.297] [info] [graph][runtime.node] hardware_transfer.summary forwarded=3611 download=0 upload=0
[2026-11-04 11:26:21.297] [info] [graph][runtime.node] source_video_trace_summary node_id=65 node=HardwareTransferNode generation=2 reason=stop steady_ns=10146214084000 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3569 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:26:21.297] [info] [graph][runtime.node] hardware_transfer.summary forwarded=38 download=0 upload=0
[2026-11-04 11:26:21.297] [info] [graph][runtime.node] source_video_trace_summary node_id=42 node=HardwareTransferNode generation=1 reason=stop steady_ns=10146214218600 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3457 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:26:21.297] [info] [graph][runtime.node] hardware_transfer.summary forwarded=41 download=0 upload=0
[2026-11-04 11:26:21.297] [info] [graph][runtime.node] source_video_trace_summary node_id=23 node=HardwareTransferNode generation=1 reason=stop steady_ns=10146214369900 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3457 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:26:21.297] [info] [graph][runtime.lifecycle] codec_owner_release node=70 kind=AudioDecode owner_references=2
[2026-11-04 11:26:21.298] [info] [graph][runtime.lifecycle] video_decode.zero_copy_summary drm_prime=0 software_frame=0
[2026-11-04 11:26:21.298] [info] [graph][runtime.node] source_video_trace_summary node_id=64 node=VideoDecodeNode generation=2 reason=stop steady_ns=10146214909700 input_count=3569 accepted_count=3569 would_block_count=0 resolved_count=3569 output_attempt_count=0 published_count=3569 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=110 incomplete=0
[2026-11-04 11:26:21.298] [info] [graph][runtime.lifecycle] codec_owner_release node=64 kind=VideoDecode owner_references=2
[2026-11-04 11:26:21.310] [info] [graph][runtime.lifecycle] video_decode.zero_copy_summary drm_prime=0 software_frame=0
[2026-11-04 11:26:21.311] [info] [graph][runtime.node] source_video_trace_summary node_id=41 node=VideoDecodeNode generation=1 reason=stop steady_ns=10146227801300 input_count=3458 accepted_count=3458 would_block_count=0 resolved_count=3458 output_attempt_count=0 published_count=3457 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=111 incomplete=0
[2026-11-04 11:26:21.311] [info] [graph][runtime.lifecycle] codec_owner_release node=41 kind=VideoDecode owner_references=2
[2026-11-04 11:26:21.325] [info] [graph][runtime.lifecycle] video_decode.zero_copy_summary drm_prime=0 software_frame=0
[2026-11-04 11:26:21.325] [info] [graph][runtime.node] source_video_trace_summary node_id=22 node=VideoDecodeNode generation=1 reason=stop steady_ns=10146242078200 input_count=3458 accepted_count=3458 would_block_count=0 resolved_count=3458 output_attempt_count=0 published_count=3457 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=111 incomplete=0
[2026-11-04 11:26:21.325] [info] [graph][runtime.lifecycle] codec_owner_release node=22 kind=VideoDecode owner_references=2
[2026-11-04 11:26:21.338] [info] [graph][runtime.node] source_video_trace_summary node_id=62 node=MediaAvBoundReleaseExtractorNode generation=2 reason=stop steady_ns=10146254967100 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=3554 release_committed_count=3554 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:26:21.338] [info] [graph][runtime.node] source_video_trace_summary node_id=39 node=MediaAvBoundReleaseExtractorNode generation=1 reason=stop steady_ns=10146255230400 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=3571 release_committed_count=3555 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:26:21.338] [info] [graph][runtime.node] source_video_trace_summary node_id=20 node=MediaAvBoundReleaseExtractorNode generation=1 reason=stop steady_ns=10146255326200 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=3571 release_committed_count=3555 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:26:21.338] [info] [graph][runtime.node] source_video_trace_summary node_id=61 node=ActivationReleaseSequencer generation=2 reason=stop steady_ns=10146255405200 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3554 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:26:21.338] [info] [graph][runtime.node] source_video_trace_summary node_id=38 node=ActivationReleaseSequencer generation=1 reason=stop steady_ns=10146255491300 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3555 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:26:21.338] [info] [graph][runtime.node] source_video_trace_summary node_id=19 node=ActivationReleaseSequencer generation=1 reason=stop steady_ns=10146255563400 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3555 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-04 11:26:21.338] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-11-04 11:26:21.339] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-11-04 11:26:21.339] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-11-04 11:26:21.339] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=10294 completion_waits=10294 completed_datagrams=10458 drained_datagrams=182 would_block=26 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-04 11:26:21.339] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=114750 completion_waits=114750 completed_datagrams=211895 drained_datagrams=97166 would_block=26 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-04 11:26:21.339] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=66823 completion_waits=66823 completed_datagrams=105199 drained_datagrams=38461 would_block=85 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-04 11:26:21.340] [info] [graph][runtime.node] rtp_ingress_adapter stage=stopped adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=64545 completion_waits=64545 completed_datagrams=105199 drained_datagrams=40739 would_block=85 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-04 11:26:21.340] [info] [graph][runtime.node] codec_resolver.release slot=encoder_parameters bufferReferences=1 hasCredit=0 creditReferences=0
[2026-11-04 11:26:21.341] [info] [graph][runtime.lifecycle] stop.done state=Stopped
[2026-11-04 11:26:21.394] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=retired detail="" description=""
[CLI] end_reason=caller_stop
[CLI] final runtime report: queued=0, peakQueued=928, threads=81, processThreads=129, workers=0, workerProgress=564057, workerProcessCalls=2102052, workerWaits=1537978, workerWakeups=1516179, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=925, logicalProcessors=22, averageSystemMachineCpuPercent=9.248156, averageProcessMachineCpuPercent=1.597490, peakProcessMachineCpuPercent=7.127430, averageProcessSingleCoreCpuPercent=35.144788, peakProcessSingleCoreCpuPercent=156.803456, initialWorkingSetBytes=209694720, workingSetBytes=263340032, peakWorkingSetBytes=272232448, totalPushed=485373, totalPopped=485338, droppedBuffers=0, encodedPacketsPushed=95989, encodedPacketsPopped=95989, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=15972617, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=752, graphPayloadReservations=223612, graphPayloadReleases=223612, graphPayloadPressureFailures=0, lastInputReceivedAtNs=10121846654500
[CLI] realtime video validation stopped successfully
~~~

## composition-r381-cli-error.log
~~~text
[in @ 000002390c50c9c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002390c50c9c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 34177.804644
[in @ 000002390c50e140] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002390c50e140] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 10640.751444
[in @ 000002390c50cd00] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002390c50cd00] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 32643.831744
[in @ 0000023912e75e40] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000023912e75e40] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 32643.831744
[in @ 00000239082c09c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000239082c09c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 10640.751444
[in @ 000002390c50e780] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002390c50e780] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 34177.804644
[aac @ 000002390c70b040] Qavg: 12845.128

~~~

## composition-r381-monitor.csv
~~~text
"Time","CpuPercent","WorkingSet","PrivateBytes"
"2026-11-04T11:21:30.4481393+08:00","0.7009491591817633","141242368","568594432"
"2026-11-04T11:21:31.4860776+08:00","0.13685346667085563","144420864","571375616"
"2026-11-04T11:21:32.5042396+08:00","0.2790232881318583","147750912","574480384"
"2026-11-04T11:21:33.5241436+08:00","0.34818339408771454","153653248","579022848"
"2026-11-04T11:21:34.5503417+08:00","1.3841913617405308","213860352","758312960"
"2026-11-04T11:21:35.5718015+08:00","4.241367466087617","254910464","1265782784"
"2026-11-04T11:21:36.5866812+08:00","2.7992569867237376","256114688","1266970624"
"2026-11-04T11:21:37.6046244+08:00","3.209457516436531","258646016","1266806784"
"2026-11-04T11:21:38.6224791+08:00","2.302637105276421","260608000","1268789248"
"2026-11-04T11:21:39.6390686+08:00","1.2575470147085828","260632576","1268801536"
"2026-11-04T11:21:40.6567365+08:00","1.3957938001724781","260808704","1268801536"
"2026-11-04T11:21:41.6754498+08:00","1.952106018088076","263135232","1270820864"
"2026-11-04T11:21:42.6958492+08:00","2.0880861143017313","263872512","1271558144"
"2026-11-04T11:21:43.7171108+08:00","2.781764330421403","263192576","1270820864"
"2026-11-04T11:21:44.7379771+08:00","1.5305628170897598","263249920","1270820864"
"2026-11-04T11:21:45.7530709+08:00","2.4488332551587395","264224768","1271656448"
"2026-11-04T11:21:46.7687530+08:00","1.9579318800994558","264282112","1271656448"
"2026-11-04T11:21:47.7854643+08:00","1.8860945446004544","264282112","1271656448"
"2026-11-04T11:21:48.7946715+08:00","2.533491815970181","264282112","1271656448"
"2026-11-04T11:21:49.8179812+08:00","1.5269082272942394","264290304","1271664640"
"2026-11-04T11:21:50.8388155+08:00","2.3654894112322906","264294400","1271668736"
"2026-11-04T11:21:51.8591329+08:00","2.9235554989599764","265109504","1272410112"
"2026-11-04T11:21:52.8764593+08:00","2.513272221991076","264384512","1271681024"
"2026-11-04T11:21:53.8991282+08:00","1.9445554310259787","264441856","1271681024"
"2026-11-04T11:21:54.9067037+08:00","2.3966171540224304","264499200","1271681024"
"2026-11-04T11:21:55.9268314+08:00","1.6709138027968995","264560640","1271681024"
"2026-11-04T11:21:56.9466793+08:00","1.4624506975278104","264622080","1271681024"
"2026-11-04T11:21:57.9681648+08:00","2.01633708056462","264679424","1271681024"
"2026-11-04T11:21:58.9910338+08:00","1.6664357357056032","264679424","1271681024"
"2026-11-04T11:22:00.0079040+08:00","1.7461109410209696","265465856","1272410112"
"2026-11-04T11:22:01.0268223+08:00","2.3002334927147743","264798208","1271681024"
"2026-11-04T11:22:02.0473571+08:00","3.549275429812967","265125888","1272090624"
"2026-11-04T11:22:03.0659553+08:00","2.231230403438051","267145216","1273946112"
"2026-11-04T11:22:04.0825617+08:00","1.606838917473594","267218944","1273946112"
"2026-11-04T11:22:05.1006435+08:00","2.0230781955920345","267231232","1273946112"
"2026-11-04T11:22:06.1213278+08:00","2.0179198317335643","267235328","1273946112"
"2026-11-04T11:22:07.1409463+08:00","2.3683100368154633","265895936","1272610816"
"2026-11-04T11:22:08.1625862+08:00","1.2513304256314683","265953280","1272610816"
"2026-11-04T11:22:09.1795064+08:00","1.6761840845972522","265953280","1272610816"
"2026-11-04T11:22:10.2000110+08:00","2.2270622520734085","266563584","1273163776"
"2026-11-04T11:22:11.2174667+08:00","2.3035400951609","268521472","1275002880"
"2026-11-04T11:22:12.2383347+08:00","2.226269481193722","268517376","1275002880"
"2026-11-04T11:22:13.2577532+08:00","1.9507556157126478","268517376","1275002880"
"2026-11-04T11:22:14.2772736+08:00","2.577526559636187","269258752","1275744256"
"2026-11-04T11:22:15.2929079+08:00","2.307671176524857","266211328","1272639488"
"2026-11-04T11:22:16.3009012+08:00","2.1842452181522884","266268672","1272639488"
"2026-11-04T11:22:17.3110252+08:00","1.6874615933741348","266268672","1272639488"
"2026-11-04T11:22:18.3320328+08:00","2.643333542633411","267026432","1273409536"
"2026-11-04T11:22:19.3513670+08:00","1.741890129673057","268152832","1274544128"
"2026-11-04T11:22:20.3701152+08:00","2.858343031361251","268214272","1274544128"
"2026-11-04T11:22:21.3880909+08:00","1.6046775254779924","268214272","1274544128"
"2026-11-04T11:22:22.4081181+08:00","2.6458741848880467","268386304","1274544128"
"2026-11-04T11:22:23.4401836+08:00","1.8580348208167372","268386304","1274544128"
"2026-11-04T11:22:24.4476566+08:00","1.4804141378749334","268386304","1274544128"
"2026-11-04T11:22:25.4681608+08:00","1.8094887890622195","268390400","1274548224"
"2026-11-04T11:22:26.4909720+08:00","1.6665299075190558","268394496","1274552320"
"2026-11-04T11:22:27.5076460+08:00","1.3971583275017807","268591104","1274552320"
"2026-11-04T11:22:28.5302189+08:00","2.1531027718948406","268529664","1274441728"
"2026-11-04T11:22:29.5493313+08:00","1.6028876964628507","268578816","1274441728"
"2026-11-04T11:22:30.5675178+08:00","2.2321325933188785","268599296","1274478592"
"2026-11-04T11:22:31.5880172+08:00","1.8790933501417406","268599296","1274478592"
"2026-11-04T11:22:32.6037454+08:00","1.8179970873023992","268599296","1274478592"
"2026-11-04T11:22:33.6219001+08:00","2.371714953800957","268599296","1274478592"
"2026-11-04T11:22:34.6378819+08:00","2.167070852503997","268599296","1274478592"
"2026-11-04T11:22:35.6573529+08:00","1.9506551570729953","268599296","1274478592"
"2026-11-04T11:22:36.6647355+08:00","1.7625559363623928","270979072","1276801024"
"2026-11-04T11:22:37.6850105+08:00","2.227563424299598","269205504","1274908672"
"2026-11-04T11:22:38.7047811+08:00","2.1590194357971737","269205504","1274908672"
"2026-11-04T11:22:39.7128624+08:00","1.1272539589452124","268107776","1273810944"
"2026-11-04T11:22:40.7292424+08:00","2.2360999554568886","268845056","1274548224"
"2026-11-04T11:22:41.7483936+08:00","2.02095537041912","268107776","1273810944"
"2026-11-04T11:22:42.7704712+08:00","1.4592603073653827","268238848","1274007552"
"2026-11-04T11:22:43.7850950+08:00","2.519966692894629","268984320","1274761216"
"2026-11-04T11:22:44.8023037+08:00","2.0946358580906925","269127680","1274761216"
"2026-11-04T11:22:45.8194693+08:00","1.6059555369083727","269197312","1274761216"
"2026-11-04T11:22:46.8264843+08:00","1.7631993384588929","269197312","1274761216"
"2026-11-04T11:22:47.8328862+08:00","1.199705966012548","269197312","1274761216"
"2026-11-04T11:22:48.8550720+08:00","2.5013243011379944","269197312","1274761216"
"2026-11-04T11:22:49.8738203+08:00","1.8823232749086662","269258752","1274761216"
"2026-11-04T11:22:50.8963393+08:00","1.5280889646060367","269377536","1274761216"
"2026-11-04T11:22:51.9114793+08:00","1.6791235243862468","270708736","1275854848"
"2026-11-04T11:22:52.9325648+08:00","3.3386928999490335","270708736","1275854848"
"2026-11-04T11:22:53.9454256+08:00","1.8932647372310554","270766080","1275854848"
"2026-11-04T11:22:54.9671007+08:00","2.1549948172902966","270766080","1275854848"
"2026-11-04T11:22:55.9880522+08:00","1.947826477199322","269619200","1274707968"
"2026-11-04T11:22:57.0059467+08:00","1.8839021493520562","269799424","1274707968"
"2026-11-04T11:22:58.0254079+08:00","1.8113400579550345","269799424","1274707968"
"2026-11-04T11:22:59.0338717+08:00","1.7606662547710508","269856768","1274707968"
"2026-11-04T11:23:00.0548948+08:00","2.2954916495033264","269856768","1274707968"
"2026-11-04T11:23:01.0734252+08:00","2.370840111667484","270802944","1275764736"
"2026-11-04T11:23:02.0871534+08:00","1.8215838417939927","270876672","1275764736"
"2026-11-04T11:23:03.0990580+08:00","1.4037435401069878","270876672","1275764736"
"2026-11-04T11:23:04.1168051+08:00","1.3956851809791895","269758464","1274646528"
"2026-11-04T11:23:05.1317464+08:00","2.3092468500395045","269836288","1274654720"
"2026-11-04T11:23:06.1458100+08:00","2.171170077946339","269828096","1274654720"
"2026-11-04T11:23:07.1630373+08:00","1.6756780461411667","269828096","1274654720"
"2026-11-04T11:23:08.1765462+08:00","2.172358373423801","269860864","1274654720"
"2026-11-04T11:23:09.1963209+08:00","1.8107832142638067","271609856","1276411904"
"2026-11-04T11:23:10.2105189+08:00","1.6806831156691835","271609856","1276411904"
"2026-11-04T11:23:11.2306895+08:00","1.8100804993703106","271609856","1276411904"
"2026-11-04T11:23:12.2530534+08:00","1.7367281667693684","269459456","1274191872"
"2026-11-04T11:23:13.2697924+08:00","2.584577663580239","269520896","1274191872"
"2026-11-04T11:23:14.2939180+08:00","2.3578872818653562","269582336","1274191872"
"2026-11-04T11:23:15.3152087+08:00","1.6690110411711911","269582336","1274191872"
"2026-11-04T11:23:16.3339281+08:00","1.673223710616932","270229504","1274834944"
"2026-11-04T11:23:17.3585647+08:00","1.7328760087412276","272232448","1276788736"
"2026-11-04T11:23:18.3788800+08:00","1.9490410107898641","272232448","1276788736"
"2026-11-04T11:23:19.3982288+08:00","1.8115397880400792","271151104","1275707392"
"2026-11-04T11:23:20.4132904+08:00","2.099066517915581","270045184","1274601472"
"2026-11-04T11:23:21.4335277+08:00","1.8099621618332413","270045184","1274601472"
"2026-11-04T11:23:22.4415527+08:00","2.677377680477802","270045184","1274601472"
"2026-11-04T11:23:23.4637075+08:00","1.5981167698598364","270045184","1274601472"
"2026-11-04T11:23:24.4800374+08:00","1.6072760697808137","270106624","1274601472"
"2026-11-04T11:23:25.4994579+08:00","2.2294306154597368","270106624","1274601472"
"2026-11-04T11:23:26.5200262+08:00","1.809375138431116","270106624","1274601472"
"2026-11-04T11:23:27.5357285+08:00","2.0977424371115614","270106624","1274601472"
"2026-11-04T11:23:28.5555147+08:00","2.2286311314344838","270106624","1274601472"
"2026-11-04T11:23:29.5673978+08:00","0.9124526880085799","246878208","1256501248"
"2026-11-04T11:23:30.5897641+08:00","1.0420344538849813","247607296","1257230336"
"2026-11-04T11:23:31.6006403+08:00","1.3349130370087041","246878208","1256583168"
"2026-11-04T11:23:32.6075480+08:00","0.9874968498286207","246878208","1256583168"
"2026-11-04T11:23:33.6312696+08:00","0.69376993972509","246878208","1256583168"
"2026-11-04T11:23:34.6529520+08:00","1.2512783727204178","246878208","1256583168"
"2026-11-04T11:23:35.6758440+08:00","0.7637658716658259","246878208","1256583168"
"2026-11-04T11:23:36.6941504+08:00","1.0461889555942192","246878208","1256583168"
"2026-11-04T11:23:37.7103032+08:00","0.9785124656628233","246878208","1256583168"
"2026-11-04T11:23:38.7341168+08:00","0.6937075974838318","246882304","1256583168"
"2026-11-04T11:23:39.7542029+08:00","0.9747394674019983","246878208","1256583168"
"2026-11-04T11:23:40.7738504+08:00","0.9751587502722086","246878208","1256583168"
"2026-11-04T11:23:41.7954892+08:00","0.83422118196052","246857728","1256546304"
"2026-11-04T11:23:42.8178542+08:00","1.1115048308222957","246865920","1256546304"
"2026-11-04T11:23:43.8415921+08:00","1.3181418976300654","246857728","1256640512"
"2026-11-04T11:23:44.8507190+08:00","1.2668467076926506","246857728","1256640512"
"2026-11-04T11:23:45.8601743+08:00","0.9850046671885143","246857728","1256718336"
"2026-11-04T11:23:46.8650115+08:00","0.848169959544419","246857728","1256718336"
"2026-11-04T11:23:47.8857278+08:00","1.6699502638935568","246857728","1256718336"
"2026-11-04T11:23:48.8920492+08:00","1.0586487667766074","246857728","1256718336"
"2026-11-04T11:23:49.9106627+08:00","1.0458735419184106","246857728","1256718336"
"2026-11-04T11:23:50.9206468+08:00","1.125130223697221","246857728","1256718336"
"2026-11-04T11:23:51.9370303+08:00","0.9782903616776362","246857728","1256718336"
"2026-11-04T11:23:52.9538379+08:00","1.1874285397122952","246857728","1256718336"
"2026-11-04T11:23:53.9746618+08:00","0.9740349749042728","246857728","1256718336"
"2026-11-04T11:23:54.9842505+08:00","0.7738299765042934","246857728","1256718336"
"2026-11-04T11:23:55.9910958+08:00","1.0580979114576083","246857728","1256718336"
"2026-11-04T11:23:57.0121706+08:00","0.6260114787423462","246857728","1256718336"
"2026-11-04T11:23:58.0320673+08:00","1.3927435449634706","250064896","1259352064"
"2026-11-04T11:23:59.0469308+08:00","1.6795810023175084","254349312","1263951872"
"2026-11-04T11:24:00.0542792+08:00","1.4805972518815465","255512576","1264918528"
"2026-11-04T11:24:01.0712290+08:00","1.8158132378716325","256184320","1265557504"
"2026-11-04T11:24:02.0915163+08:00","1.461820874107982","256864256","1266122752"
"2026-11-04T11:24:03.1123865+08:00","1.8088400553673807","257814528","1266999296"
"2026-11-04T11:24:04.1295136+08:00","2.024977105525053","257495040","1266307072"
"2026-11-04T11:24:05.1451527+08:00","1.2587237837821434","257691648","1266307072"
"2026-11-04T11:24:06.1651552+08:00","1.1140792658485017","259137536","1267478528"
"2026-11-04T11:24:07.1851550+08:00","1.2533424917427347","259256320","1267552256"
"2026-11-04T11:24:08.2056358+08:00","1.8095302813055463","259272704","1267568640"
"2026-11-04T11:24:09.2207500+08:00","1.9590272342130213","259424256","1267970048"
"2026-11-04T11:24:10.2414738+08:00","1.5307764940917414","259534848","1268015104"
"2026-11-04T11:24:11.2622085+08:00","1.7395001676911561","259645440","1268076544"
"2026-11-04T11:24:12.2696610+08:00","1.4099469160625888","259883008","1268269056"
"2026-11-04T11:24:13.2880128+08:00","1.8133133452417025","260382720","1268490240"
"2026-11-04T11:24:14.3084576+08:00","1.600794797790853","260911104","1268846592"
"2026-11-04T11:24:15.3240663+08:00","1.6084174222539913","261136384","1268850688"
"2026-11-04T11:24:16.3380480+08:00","1.3308246274876738","261169152","1268883456"
"2026-11-04T11:24:17.3574822+08:00","1.7417192613492676","261185536","1268887552"
"2026-11-04T11:24:18.3637879+08:00","1.129242968974176","261201920","1268887552"
"2026-11-04T11:24:19.3709135+08:00","1.4104045666742515","261226496","1268887552"
"2026-11-04T11:24:20.3891415+08:00","1.8832851152822714","261226496","1268887552"
"2026-11-04T11:24:21.4028752+08:00","2.382058253832074","261226496","1268887552"
"2026-11-04T11:24:22.4240671+08:00","1.8778190821564842","261279744","1268916224"
"2026-11-04T11:24:23.4414812+08:00","1.2565277903157532","261492736","1269104640"
"2026-11-04T11:24:24.4613838+08:00","1.253461939315667","261496832","1269104640"
"2026-11-04T11:24:25.4792889+08:00","1.6745622500029271","261746688","1269104640"
"2026-11-04T11:24:26.5055485+08:00","0.8996704679259074","261787648","1269133312"
"2026-11-04T11:24:27.5109915+08:00","1.4834031096017106","261849088","1269133312"
"2026-11-04T11:24:28.5213075+08:00","1.1950581438246684","261857280","1269133312"
"2026-11-04T11:24:29.5332958+08:00","1.6843529263583923","262008832","1269133312"
"2026-11-04T11:24:30.5433701+08:00","1.4062871864520712","262365184","1269366784"
"2026-11-04T11:24:31.5572910+08:00","0.8405712193847935","262737920","1269366784"
"2026-11-04T11:24:32.5753410+08:00","1.5347969156721184","262733824","1269366784"
"2026-11-04T11:24:33.5905794+08:00","1.2592205839624375","262766592","1269366784"
"2026-11-04T11:24:34.6018955+08:00","2.4579806991557387","262766592","1269366784"
"2026-11-04T11:24:35.6191900+08:00","1.7453826613809296","262840320","1269366784"
"2026-11-04T11:24:36.6310573+08:00","1.1932259928118674","262840320","1269366784"
"2026-11-04T11:24:37.6436098+08:00","1.192418530038061","262840320","1269366784"
"2026-11-04T11:24:38.6579761+08:00","1.190286352806046","263577600","1270104064"
"2026-11-04T11:24:39.6680553+08:00","1.4765943826259096","262901760","1269366784"
"2026-11-04T11:24:40.6783432+08:00","0.9841929036447747","262901760","1269366784"
"2026-11-04T11:24:41.6916905+08:00","1.6120067890571448","262991872","1269469184"
"2026-11-04T11:24:42.7008960+08:00","1.266748041810207","263049216","1269469184"
"2026-11-04T11:24:43.7207002+08:00","1.671443846324083","263049216","1269469184"
"2026-11-04T11:24:44.7294045+08:00","1.2673774573074497","263049216","1269469184"
"2026-11-04T11:24:45.7429074+08:00","0.910994388418084","263122944","1269469184"
"2026-11-04T11:24:46.7577465+08:00","0.9797791411645271","263122944","1269469184"
"2026-11-04T11:24:47.7678473+08:00","1.2656252632500549","263548928","1269903360"
"2026-11-04T11:24:48.7732028+08:00","1.342243433473849","263540736","1269903360"
"2026-11-04T11:24:49.7882255+08:00","0.8396587852397066","263540736","1269903360"
"2026-11-04T11:24:50.8015702+08:00","1.1914863359292882","263540736","1269903360"
"2026-11-04T11:24:51.8110300+08:00","1.1960717639636207","263540736","1269903360"
"2026-11-04T11:24:52.8233625+08:00","1.1225201565331906","263540736","1269903360"
"2026-11-04T11:24:53.8339416+08:00","1.1947470154848479","263540736","1269903360"
"2026-11-04T11:24:54.8481881+08:00","1.1204018316687672","263544832","1269903360"
"2026-11-04T11:24:55.8628240+08:00","1.1899700805346662","264024064","1270386688"
"2026-11-04T11:24:56.8700838+08:00","1.1281733236684681","264069120","1270386688"
"2026-11-04T11:24:57.8822239+08:00","0.8420501541957751","264060928","1270386688"
"2026-11-04T11:24:58.8899511+08:00","1.0571719301522366","263864320","1270386688"
"2026-11-04T11:24:59.8987959+08:00","1.8304013750092274","263864320","1270386688"
"2026-11-04T11:25:00.9117532+08:00","1.0517135412232175","263864320","1270386688"
"2026-11-04T11:25:01.9199187+08:00","1.4089497661391361","263864320","1270386688"
"2026-11-04T11:25:02.9388443+08:00","1.0455531876821125","263864320","1270386688"
"2026-11-04T11:25:03.9544556+08:00","1.1188962119303285","263868416","1270386688"
"2026-11-04T11:25:04.9689203+08:00","1.0501507929166083","263868416","1270386688"
"2026-11-04T11:25:05.9774954+08:00","0.7746076618389647","263868416","1270386688"
"2026-11-04T11:25:06.9898382+08:00","1.1225087355425813","263868416","1270386688"
"2026-11-04T11:25:07.9959082+08:00","1.4118844071034276","263868416","1270386688"
"2026-11-04T11:25:09.0025306+08:00","0.9877767292066836","263868416","1270386688"
"2026-11-04T11:25:10.0103510+08:00","1.127545777366321","263872512","1270386688"
"2026-11-04T11:25:11.0303237+08:00","1.6711677229649915","263868416","1270386688"
"2026-11-04T11:25:12.0484540+08:00","1.0463699087345786","264601600","1271107584"
"2026-11-04T11:25:13.0636571+08:00","1.888896553176046","263880704","1270386688"
"2026-11-04T11:25:14.0813391+08:00","1.2561970152848245","263880704","1270386688"
"2026-11-04T11:25:15.0993737+08:00","1.604584684324803","263880704","1270386688"
"2026-11-04T11:25:16.1160299+08:00","2.0957741842147013","263876608","1270386688"
"2026-11-04T11:25:17.1374255+08:00","2.3641894749426444","263876608","1270386688"
"2026-11-04T11:25:18.1540666+08:00","0.978042479118916","263876608","1270386688"
"2026-11-04T11:25:19.1758923+08:00","1.5291257599021046","263876608","1270386688"
"2026-11-04T11:25:20.1968289+08:00","1.113059945508503","263876608","1270386688"
"2026-11-04T11:25:21.2067810+08:00","1.476780208415105","263876608","1270386688"
"2026-11-04T11:25:22.2226306+08:00","1.747865217270531","263876608","1270386688"
"2026-11-04T11:25:23.2406823+08:00","1.1162140747504634","263876608","1270386688"
"2026-11-04T11:25:24.2631376+08:00","1.4587212494543993","263884800","1270386688"
"2026-11-04T11:25:25.2839053+08:00","1.3219773883732981","263876608","1270386688"
"2026-11-04T11:25:26.3025432+08:00","1.2550181874335236","263876608","1270386688"
"2026-11-04T11:25:27.3245894+08:00","1.6677773025773737","264605696","1271115776"
"2026-11-04T11:25:28.3474100+08:00","1.6665145916551296","263880704","1270386688"
"2026-11-04T11:25:29.3570213+08:00","1.6179719138174535","263880704","1270386688"
"2026-11-04T11:25:30.3747566+08:00","1.465486431223593","263880704","1270386688"
"2026-11-04T11:25:31.3989698+08:00","1.178842806982339","263880704","1270386688"
"2026-11-04T11:25:32.4132458+08:00","1.7505769453464166","263880704","1270386688"
"2026-11-04T11:25:33.4347145+08:00","2.364020284980565","263880704","1270386688"
"2026-11-04T11:25:34.4469993+08:00","1.6838595764210373","263880704","1270386688"
"2026-11-04T11:25:35.4647450+08:00","1.7446088760858254","263880704","1270386688"
"2026-11-04T11:25:36.4861053+08:00","1.321210368350736","263880704","1270386688"
"2026-11-04T11:25:37.5007880+08:00","1.3299052188253708","263880704","1270386688"
"2026-11-04T11:25:38.5208445+08:00","1.531777896616511","264609792","1271115776"
"2026-11-04T11:25:39.5299904+08:00","1.4779599983781062","263880704","1270386688"
"2026-11-04T11:25:40.5498157+08:00","1.1142728429699051","263880704","1270386688"
"2026-11-04T11:25:41.5695523+08:00","1.0447216556617749","263880704","1270386688"
"2026-11-04T11:25:42.5893961+08:00","1.7410197344124478","263278592","1269305344"
"2026-11-04T11:25:43.6084126+08:00","1.1848545765808145","263278592","1269305344"
"2026-11-04T11:25:44.6260844+08:00","1.6051567187699682","263278592","1269305344"
"2026-11-04T11:25:45.6343199+08:00","1.7610649315742024","264015872","1270042624"
"2026-11-04T11:25:46.6511878+08:00","1.4667365079842454","263278592","1269305344"
"2026-11-04T11:25:47.6713352+08:00","1.1835410879215726","263278592","1269305344"
"2026-11-04T11:25:48.6778389+08:00","1.1290208236329748","263286784","1269305344"
"2026-11-04T11:25:49.6997070+08:00","2.015582139132331","263278592","1269305344"
"2026-11-04T11:25:50.7199110+08:00","1.1834754261268956","263278592","1269305344"
"2026-11-04T11:25:51.7407479+08:00","1.113168652469005","263278592","1269305344"
"2026-11-04T11:25:52.7611742+08:00","2.0880310691539585","263340032","1269305344"
"2026-11-04T11:25:53.7800302+08:00","2.3003741451196245","263340032","1269305344"
"2026-11-04T11:25:54.7875205+08:00","1.4803887171194332","263340032","1269305344"
"2026-11-04T11:25:55.8036713+08:00","1.6075593576000011","263340032","1269305344"
"2026-11-04T11:25:56.8214330+08:00","1.5352316755484117","263348224","1269305344"
"2026-11-04T11:25:57.8313055+08:00","1.265911380802122","263340032","1269305344"
"2026-11-04T11:25:58.8505003+08:00","0.975591890596559","263340032","1269305344"
"2026-11-04T11:25:59.8742673+08:00","0.8324870085407394","263340032","1269305344"
"2026-11-04T11:26:00.8862661+08:00","0.98252901270059","263348224","1269305344"
"2026-11-04T11:26:01.8979034+08:00","1.1934972777658195","263340032","1269305344"
"2026-11-04T11:26:02.9149743+08:00","0.6983065514186599","263340032","1269305344"
"2026-11-04T11:26:03.9376014+08:00","1.041768704438704","263340032","1269305344"
"2026-11-04T11:26:04.9509618+08:00","1.1213815305627064","263340032","1269305344"
"2026-11-04T11:26:05.9622658+08:00","1.0534329035491892","263340032","1269305344"
"2026-11-04T11:26:06.9726867+08:00","1.1246438354191175","263340032","1269305344"
"2026-11-04T11:26:07.9939307+08:00","0.9040889880826272","263340032","1269305344"
"2026-11-04T11:26:09.0042021+08:00","1.1248102602564385","263340032","1269305344"
"2026-11-04T11:26:10.0266922+08:00","0.9724477350129668","263340032","1269305344"
"2026-11-04T11:26:11.0327616+08:00","0.9883196743864606","263340032","1269305344"
"2026-11-04T11:26:12.0523665+08:00","1.1145137065971695","263348224","1269305344"
"2026-11-04T11:26:13.0617564+08:00","1.1257925568342186","263340032","1269305344"
"2026-11-04T11:26:14.0801744+08:00","0.6973828749366887","263340032","1269305344"
"2026-11-04T11:26:15.1025256+08:00","1.180989823884751","263340032","1269305344"
"2026-11-04T11:26:16.1236815+08:00","1.0432696017238006","263340032","1269305344"
"2026-11-04T11:26:17.1457603+08:00","1.181304576160237","263340032","1269305344"
"2026-11-04T11:26:18.1667487+08:00","0.7651898885432978","263340032","1269305344"
"2026-11-04T11:26:19.1756724+08:00","1.267101854093715","263344128","1269305344"
"2026-11-04T11:26:20.1980214+08:00","1.2504625043982935","263340032","1269305344"
"2026-11-04T11:26:21.2076119+08:00","0.9145247053587119","262995968","1268760576"

~~~

## composition-r381-vlc.log
~~~text
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
file debug: opening logfile `D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r381-vlc.log'
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
main debug: stream filter added to 018fe660
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
main debug: creating access: http://update.videolan.org/vlc/status-win-x86
main debug:  (path: \\update.videolan.org\vlc\status-win-x86)
main debug: looking for access module matching "http": 27 candidates
main debug: looking for extension module matching "any": 1 candidates
lua debug: Opening Lua Extension module
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\extensions
lua debug: Trying Lua scripts in D:\VideoLAN\VLC\lua\extensions
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\extensions\VLSub.luac
lua debug: Scanning Lua script D:\VideoLAN\VLC\lua\extensions\VLSub.luac
lua debug: Script D:\VideoLAN\VLC\lua\extensions\VLSub.luac has the following capability flags: 0x5
main debug: using extension module "lua"
http debug: resolving update.videolan.org ...
http debug: outgoing request:
GET /vlc/status-win-x86 HTTP/1.1
Host: update.videolan.org
Accept: */*
Accept-Language: en_US
User-Agent: VLC/3.0.23 LibVLC/3.0.23
Range: bytes=0-


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
qt debug: IM: Setting an input
udp debug: opening server=:0 local=127.0.0.1:62024
main debug: net: opening 127.0.0.1 datagram port 62024
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\meta\art
lua debug: Trying Lua scripts in D:\VideoLAN\VLC\lua\meta\art
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\art\00_musicbrainz.luac
main debug: using access module "udp"
main debug: looking for stream_filter module matching "prefetch,cache_block": 24 candidates
prefetch debug: using 16777216 bytes buffer, 16777216 bytes read
main debug: using stream_filter module "prefetch"
main debug: looking for stream_filter module matching "any": 24 candidates
lua debug: skipping script (unmatched scope) D:\VideoLAN\VLC\lua\meta\art\00_musicbrainz.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\art\01_googleimage.luac
lua debug: skipping script (unmatched scope) D:\VideoLAN\VLC\lua\meta\art\01_googleimage.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\art\02_frenchtv.luac
lua debug: skipping script (unmatched scope) D:\VideoLAN\VLC\lua\meta\art\02_frenchtv.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\art\03_lastfm.luac
lua debug: skipping script (unmatched scope) D:\VideoLAN\VLC\lua\meta\art\03_lastfm.luac
main debug: no art finder modules matched
http debug: incoming response:
HTTP/1.1 206 Partial Content
Server: nginx/1.31.3
Date: Wed, 04 Nov 2026 03:21:26 GMT
Content-Type: application/octet-stream
Content-Length: 518
Connection: keep-alive
Last-Modified: Wed, 23 Sep 2026 11:23:15 GMT
ETag: "6ab3b6a3-206"
Content-Range: bytes 0-517/518
X-Clacks-Overhead: GNU Terry Pratchett
Strict-Transport-Security: max-age=31536000


main debug: using access module "access"
main debug: looking for stream_filter module matching "prefetch,cache_block": 24 candidates
prefetch debug: using 518 bytes buffer, 518 bytes read
main debug: using stream_filter module "prefetch"
main debug: looking for stream_filter module matching "any": 24 candidates
prefetch debug: end of stream
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
main debug: removing module "prefetch"
main debug: removing module "access"
main debug: creating access: http://update.videolan.org/vlc/status-win-x86.asc
main debug:  (path: \\update.videolan.org\vlc\status-win-x86.asc)
main debug: looking for access module matching "http": 27 candidates
http debug: resolving update.videolan.org ...
http debug: outgoing request:
GET /vlc/status-win-x86.asc HTTP/1.1
Host: update.videolan.org
Accept: */*
Accept-Language: en_US
User-Agent: VLC/3.0.23 LibVLC/3.0.23
Range: bytes=0-


http debug: incoming response:
HTTP/1.1 206 Partial Content
Server: nginx/1.31.3
Date: Wed, 04 Nov 2026 03:21:27 GMT
Content-Type: application/octet-stream
Content-Length: 866
Connection: keep-alive
Last-Modified: Wed, 23 Sep 2026 11:23:15 GMT
ETag: "6ab3b6a3-362"
Content-Range: bytes 0-865/866
X-Clacks-Overhead: GNU Terry Pratchett
Strict-Transport-Security: max-age=31536000


main debug: using access module "access"
main debug: looking for stream_filter module matching "prefetch,cache_block": 24 candidates
prefetch debug: using 866 bytes buffer, 866 bytes read
main debug: using stream_filter module "prefetch"
main debug: looking for stream_filter module matching "any": 24 candidates
prefetch debug: end of stream
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
main debug: Downloading signature (866 bytes)
main debug: removing module "prefetch"
main debug: removing module "access"
main debug: Unarmoring signature
main debug: Need to download the GPG key
main debug: creating access: http://download.videolan.org/pub/keys/1D77C0AE835B911E.asc
main debug:  (path: \\download.videolan.org\pub\keys\1D77C0AE835B911E.asc)
main debug: looking for access module matching "http": 27 candidates
http debug: resolving download.videolan.org ...
http debug: outgoing request:
GET /pub/keys/1D77C0AE835B911E.asc HTTP/1.1
Host: download.videolan.org
Accept: */*
Accept-Language: en_US
User-Agent: VLC/3.0.23 LibVLC/3.0.23
Range: bytes=0-


http debug: incoming response:
HTTP/1.1 206 Partial Content
Server: nginx/1.31.8
Date: Wed, 04 Nov 2026 03:21:29 GMT
Content-Type: application/octet-stream
Content-Length: 1818
Connection: keep-alive
Last-Modified: Tue, 15 Sep 2026 12:00:12 GMT
ETag: "6aa9334c-71a"
X-Clacks-Overhead: GNU Terry Pratchett
Content-Range: bytes 0-1817/1818


main debug: using access module "access"
main debug: looking for stream_filter module matching "prefetch,cache_block": 24 candidates
prefetch debug: using 1818 bytes buffer, 1818 bytes read
main debug: using stream_filter module "prefetch"
main debug: looking for stream_filter module matching "any": 24 candidates
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\playlist
lua debug: Trying Lua scripts in D:\VideoLAN\VLC\lua\playlist
prefetch debug: end of stream
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
main debug: removing module "prefetch"
main debug: removing module "access"
main: Key authenticated
main: Status file authenticated
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
main debug: Buffering 13%
faad warning: decoded zero sample
main debug: reusing audio output
main debug: looking for aout stream module matching "any": 2 candidates
main debug: restarting module due to input format change
main debug: Buffering 19%
main debug: Buffering 26%
main debug: removing module "avcodec"
main debug: Buffering 33%
main debug: Buffering 39%
main debug: Buffering 46%
main debug: Buffering 53%
main debug: Buffering 59%
main debug: Buffering 66%
main debug: Buffering 73%
main debug: Buffering 79%
main debug: Buffering 86%
main debug: Buffering 93%
main debug: Buffering 99%
main debug: Stream buffering done (1066 ms in 6 ms)
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
wasapi debug: modified format
swscale debug: 32x32 (32x32) chroma: YUVA -> 16x16 (16x16) chroma: RGBA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: looking for video converter module matching "any": 24 candidates
yuvp debug: YUVP to YUVA converter
main debug: using video converter module "yuvp"
main debug: Deinterlacing available
main debug: deinterlace -1, mode auto, is_needed 0
main debug: looking for vout window module matching "qt,any": 3 candidates
qt debug: requesting video window...
main debug: resized to 84985088x64593696
main debug: resized to 84985088x64593696
main debug: resized to 84984576x64593696
main debug: resized to 84984576x64593696
main debug: resized to 84984320x64593696
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
direct3d11 debug: creating Vout EventThread
main debug: using audio resampler module "samplerate"
direct3d11 debug: Win32VoutCreateWindow
main debug: VoutDisplayEvent 'resize' 1280x720
qt debug: Logical video size: 1280x720
main debug: resized to 1280x720
main debug: VoutDisplayEvent 'resize' 1280x720
direct3d11 debug: inclinometer not found. (hr=0x80070490)
direct3d11 debug: created video sub-window
direct3d11 debug: Vout EventThread running
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
direct3d11 debug: D3D11 pool succeed with 48 surfaces (1280x736) context 0x0b9ccef8
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
avcodec: Using D3D11VA (NVIDIA GeForce RTX 4060 Laptop GPU, vendor 10de(NVIDIA), device 28e0, revision a1) for hardware decoding
main debug: Received first picture
main debug: Decoder wait done in 937 ms
main debug: inserting 9594 zeroes
mmdevice debug: state changed: 1
main warning: playback too late (144869): up-sampling
main warning: picture is too late to be displayed (missing 108 ms)
main warning: picture is too late to be displayed (missing 74 ms)
main warning: picture is too late to be displayed (missing 41 ms)
main debug: picture might be displayed late (missing 8 ms)
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
main debug: auto hiding mouse cursor
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
main warning: picture is too late to be displayed (missing 1130 ms)
main warning: picture is too late to be displayed (missing 1096 ms)
main warning: picture is too late to be displayed (missing 1063 ms)
main warning: picture is too late to be displayed (missing 1030 ms)
main warning: picture is too late to be displayed (missing 996 ms)
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
avcodec warning: More than 11 late frames, dropping frame
main warning: buffer too late (-291745 us): dropped
main warning: buffer too late (-268525 us): dropped
main warning: buffer too late (-245305 us): dropped
main warning: buffer too late (-222085 us): dropped
main warning: buffer too late (-198865 us): dropped
main warning: buffer too late (-175645 us): dropped
main warning: buffer too late (-152425 us): dropped
main warning: buffer too late (-129205 us): dropped
main warning: buffer too late (-105985 us): dropped
main warning: buffer too late (-82765 us): dropped
main debug: playback too late (1089251): flushing buffers
mmdevice debug: state changed: 0
wasapi debug: reset
main warning: playback too late (61543): up-sampling
mmdevice debug: state changed: 1
main debug: resized to 1280x720
main debug: VoutDisplayEvent 'resize' 1280x720
main debug: resized to 1280x720
main debug: VoutDisplayEvent 'resize' 1280x720
main debug: resampling stopped (drift: -5049 us)
main debug: auto hiding mouse cursor
main debug: resized to 1280x720
main debug: VoutDisplayEvent 'resize' 1280x720
main debug: resized to 1280x720
main debug: VoutDisplayEvent 'resize' 1280x720
main debug: resized to 1280x720
main debug: VoutDisplayEvent 'resize' 1280x720
main debug: resized to 1280x720
main debug: VoutDisplayEvent 'resize' 1280x720
main debug: resized to 1280x720
main debug: VoutDisplayEvent 'resize' 1280x720
main debug: resized to 1280x720
main debug: VoutDisplayEvent 'resize' 1280x720
main debug: resized to 2560x1382
main debug: VoutDisplayEvent 'resize' 2560x1382
main debug: resized to 2560x1382
main debug: VoutDisplayEvent 'resize' 2560x1382
main debug: resized to 2560x1382
main debug: VoutDisplayEvent 'resize' 2560x1382
main debug: VoutDisplayEvent 'resize' 2560x1382
direct3d11 debug: Detected size change 2456x1382
main debug: auto hiding mouse cursor
main debug: resized to 2560x1382
main debug: VoutDisplayEvent 'resize' 2560x1382
main debug: resized to 2560x1382
main debug: VoutDisplayEvent 'resize' 2560x1382
main debug: resized to 2560x1382
main debug: VoutDisplayEvent 'resize' 2560x1382
main debug: resized to 2560x1382
main debug: VoutDisplayEvent 'resize' 2560x1382
main debug: resized to 1280x720
main debug: VoutDisplayEvent 'resize' 1280x720
main debug: resized to 1280x720
main debug: VoutDisplayEvent 'resize' 1280x720
main debug: resized to 1280x720
main debug: VoutDisplayEvent 'resize' 1280x720
main debug: resized to 1280x720
main debug: VoutDisplayEvent 'resize' 1280x720
main debug: resized to 1280x720
main debug: VoutDisplayEvent 'resize' 1280x720
main debug: VoutDisplayEvent 'resize' 1280x720
direct3d11 debug: Detected size change 1280x720
main debug: auto hiding mouse cursor
main debug: accepted socket 3408 (from socket 1180)
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
main debug: Filter 'd3d11_filters' (0736b068) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (0736b5b8) appended to chain
main debug: using video converter module "chain"
main debug: Filter 'chain' (05412308) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: RV24 -> 1280x720 (1280x720) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (07369088) appended to chain
main debug: using video converter module "chain"
main debug: removing module "png"
main debug: removing module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 0736b068 removed from chain
main debug: removing module "swscale"
main debug: Filter 0736b5b8 removed from chain
main debug: Filter 05412308 removed from chain
main debug: removing module "swscale"
main debug: Filter 07369088 removed from chain
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r381-native-2026-11-04-11h22m12s762.png)
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (05412308) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: YUVA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (052450a8) appended to chain
main debug: using video converter module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 05412308 removed from chain
main debug: removing module "swscale"
main debug: Filter 052450a8 removed from chain
main error: read error: No error
main debug: accepted socket 3476 (from socket 1180)
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
main debug: Filter 'd3d11_filters' (073207f8) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (0731f560) appended to chain
main debug: using video converter module "chain"
main debug: Filter 'chain' (05412308) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: RV24 -> 1280x720 (1280x720) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (0731fd58) appended to chain
main debug: using video converter module "chain"
main debug: removing module "png"
main debug: removing module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 073207f8 removed from chain
main debug: removing module "swscale"
main debug: Filter 0731f560 removed from chain
main debug: Filter 05412308 removed from chain
main debug: removing module "swscale"
main debug: Filter 0731fd58 removed from chain
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r381-native-2026-11-04-11h23m48s640.png)
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (05412308) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: YUVA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (07320550) appended to chain
main debug: using video converter module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 05412308 removed from chain
main debug: removing module "swscale"
main debug: Filter 07320550 removed from chain
main debug: accepted socket 3304 (from socket 1180)
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
main debug: Filter 'd3d11_filters' (054047b0) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (05403d10) appended to chain
main debug: using video converter module "chain"
main debug: Filter 'chain' (05412308) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: RV24 -> 1280x720 (1280x720) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (054037c0) appended to chain
main debug: using video converter module "chain"
main debug: removing module "png"
main debug: removing module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 054047b0 removed from chain
main debug: removing module "swscale"
main debug: Filter 05403d10 removed from chain
main debug: Filter 05412308 removed from chain
main debug: removing module "swscale"
main debug: Filter 054037c0 removed from chain
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r381-native-2026-11-04-11h24m22s245.png)
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (05412308) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: YUVA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (054047b0) appended to chain
main debug: using video converter module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 05412308 removed from chain
main debug: removing module "swscale"
main debug: Filter 054047b0 removed from chain
main debug: accepted socket 3868 (from socket 1180)
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
main debug: Filter 'd3d11_filters' (1e1e4350) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (1e1e7b18) appended to chain
main debug: using video converter module "chain"
main debug: Filter 'chain' (05412308) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: RV24 -> 1280x720 (1280x720) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (1e1e8068) appended to chain
main debug: using video converter module "chain"
main debug: removing module "png"
main debug: removing module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 1e1e4350 removed from chain
main debug: removing module "swscale"
main debug: Filter 1e1e7b18 removed from chain
main debug: Filter 05412308 removed from chain
main debug: removing module "swscale"
main debug: Filter 1e1e8068 removed from chain
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r381-native-2026-11-04-11h26m05s073.png)
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (05412308) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: YUVA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (1e1e6880) appended to chain
main debug: using video converter module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 05412308 removed from chain
main debug: removing module "swscale"
main debug: Filter 1e1e6880 removed from chain
main debug: accepted socket 1060 (from socket 1180)
main debug: exiting
main debug: exiting
main debug: no exit handler
main debug: removing all interfaces
main debug: removing module "qt"
main debug: deactivating the playlist
main debug: incoming request - stopping current input
prefetch debug: end of stream
ts debug: Can't read TS packet at 203709468
main debug: EOF reached
main error: read error: No error
main warning: picture is too late to be displayed (missing 17 ms)
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
qt debug: QApp exec() finished
qt debug: video widget is orphaned
qt debug: Killing extension dialog provider
qt debug: ExtensionsDialogProvider is quitting...
main debug: removing module "lua"
main debug: removing module "win32"
main debug: removing module "hotkeys"
main debug: removing module "oldrc"
main debug: destroying
main debug: saving media library to file C:\Users\ROG\AppData\Roaming\vlc\ml.xspf.tmp36284
main debug: looking for playlist export module matching "export-xspf": 4 candidates
main debug: using playlist export module "export"
main debug: removing module "export"
main debug: deleting item `Media Library'
main debug: deleting item `udp://127.0.0.1:62024'
main debug: deleting item `Playlist'
main debug: removing module "memory"
-- logger module stopped --

~~~

## source0结尾
~~~text
frame= 3561 fps= 30 q=-1.0 size=  117603KiB time=00:01:58.70 bitrate=8116.3kbits/s speed=   1x elapsed=0:01:58.21    
frame= 3577 fps= 30 q=-1.0 size=  118184KiB time=00:01:59.23 bitrate=8119.9kbits/s speed=   1x elapsed=0:01:58.72    
frame= 3593 fps= 30 q=-1.0 size=  118601KiB time=00:01:59.76 bitrate=8112.1kbits/s speed=   1x elapsed=0:01:59.24    
[out#0/rtp @ 0000014e7d25a600] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
[out#1/rtp @ 0000014e01ac2140] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
~~~

## source1结尾
~~~text
frame= 3546 fps= 30 q=-1.0 size=  117146KiB time=00:01:58.20 bitrate=8119.0kbits/s speed=   1x elapsed=0:01:57.69    
frame= 3562 fps= 30 q=-1.0 size=  117635KiB time=00:01:58.73 bitrate=8116.2kbits/s speed=   1x elapsed=0:01:58.21    
frame= 3577 fps= 30 q=-1.0 size=  118184KiB time=00:01:59.23 bitrate=8119.9kbits/s speed=   1x elapsed=0:01:58.72    
frame= 3592 fps= 30 q=-1.0 size=  118576KiB time=00:01:59.73 bitrate=8112.8kbits/s speed=   1x elapsed=0:01:59.25    
[out#0/rtp @ 0000020838dbafc0] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.47    
~~~

## source2结尾
~~~text
frame= 3546 fps= 30 q=-1.0 size=  117146KiB time=00:01:58.20 bitrate=8119.0kbits/s speed=   1x elapsed=0:01:57.69    
frame= 3562 fps= 30 q=-1.0 size=  117635KiB time=00:01:58.73 bitrate=8116.2kbits/s speed=   1x elapsed=0:01:58.22    
frame= 3577 fps= 30 q=-1.0 size=  118184KiB time=00:01:59.23 bitrate=8119.9kbits/s speed=   1x elapsed=0:01:58.73    
frame= 3592 fps= 30 q=-1.0 size=  118576KiB time=00:01:59.73 bitrate=8112.8kbits/s speed=   1x elapsed=0:01:59.25    
[out#0/rtp @ 0000024a93fa9540] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.47    
~~~

## reentry结尾
~~~text
frame= 3560 fps= 30 q=-1.0 size=  117571KiB time=00:01:58.67 bitrate=8115.6kbits/s speed=   1x elapsed=0:01:58.17    
frame= 3576 fps= 30 q=-1.0 size=  118130KiB time=00:01:59.20 bitrate=8118.5kbits/s speed=   1x elapsed=0:01:58.68    
frame= 3591 fps= 30 q=-1.0 size=  118561KiB time=00:01:59.70 bitrate=8114.0kbits/s speed=   1x elapsed=0:01:59.19    
[out#0/rtp @ 000001e7de32d040] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
[out#1/rtp @ 000001e7de3b9f40] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
~~~

## 本轮临时文件精确清单
- composition-r381-cli-error.log (1968 bytes)
- composition-r381-cli.log (1221656 bytes)
- composition-r381-monitor.csv (23790 bytes)
- composition-r381-native-2026-11-04-11h22m12s762.png (901602 bytes)
- composition-r381-native-2026-11-04-11h23m48s640.png (12152 bytes)
- composition-r381-native-2026-11-04-11h24m22s245.png (324403 bytes)
- composition-r381-native-2026-11-04-11h26m05s073.png (12152 bytes)
- composition-r381-reentry-stdout.log (540 bytes)
- composition-r381-reentry.log (29521 bytes)
- composition-r381-source0-stdout.log (540 bytes)
- composition-r381-source0.log (29521 bytes)
- composition-r381-source1-stdout.log (335 bytes)
- composition-r381-source1.log (28926 bytes)
- composition-r381-source2-stdout.log (335 bytes)
- composition-r381-source2.log (28926 bytes)
- composition-r381-vlc.log (53124 bytes)

清理结果：同环境复查六个本轮PID及UDP62024/TCP63020均无残留；按上述16项清单验证路径和文件长度后逐项删除，r381临时产物残留0。指定复用源仍保留，长度124427809字节。未运行远程测试，无本轮远程产物。
