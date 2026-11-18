# r380：共同容量事实与同批次CUDA计时回归

日期：2026-11-02。结果 **FAIL**：所选源重入接近结束时发送预约窗口不可行，CLI退出1，未取得再次全黑画面。

共同容量/aggregate事实17文件与CUDA timing5文件均双独立局部PASS；首次全量构建61336因domainRole optional映射C2665失败，修复后重新双审，第二次73836全量785目标构建配置/编译exit0，四产物核验，realtime产物11:53:24。本轮仍走CanonicalIntervals，不能替代SourcePresentation、early费用及联合solver验证。

## 实际启动和监控命令

```powershell
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r380-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r380-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r380-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r380-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r380-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r380-source2.log' -PassThru
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r380 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r380-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r380-cli-error.log' -PassThru
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r380-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r380-native- --snapshot-format=png udp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
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
$samples | Export-Csv -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r380-monitor.csv' -NoTypeInformation -Encoding UTF8
$source0.WaitForExit()
$source1.WaitForExit()
$source2.WaitForExit()
Write-Output "SOURCE0_EXIT=$($source0.ExitCode) SOURCE1_EXIT=$($source1.ExitCode) SOURCE2_EXIT=$($source2.ExitCode)"
```

初始PID：source0=35624、source1=35208、source2=24132、CLI=34612、VLC=3204。启动前同环境媒体进程/所需端口无占用；UDP62024与TCP63020启动后均归VLC3204。

## 画面与重入

已逐张查看本轮VLC原生快照：
- 11h54m39s335：运行中三格游戏画面、右下黑格。
- 11h56m42s265：三原源完整结束后全黑，CLI仍持续运行。
- 11h57m16s744：所选源重入，左下恢复游戏画面，其余黑格。
- 再次全黑阶段未取得快照：调用前CLI存活检查失败，明确返回“CLI already ended before snapshot”；没有把VLC留帧算作画面恢复。

实际快照命令（每次相同）：
```powershell
if(-not (Get-CimInstance Win32_Process -Filter 'ProcessId=34612')){throw 'CLI already ended before snapshot'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
try{$s=$c.GetStream();$s.ReadTimeout=2000;$b=[Text.Encoding]::ASCII.GetBytes('snapshot'+[char]10);$s.Write($b,0,$b.Length);$buf=[byte[]]::new(4096);try{$n=$s.Read($buf,0,$buf.Length);[Text.Encoding]::UTF8.GetString($buf,0,$n)}catch{Write-Output $_.Exception.Message}}finally{$c.Dispose()}
Get-ChildItem -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance' -Filter 'composition-r380-native-*.png' | Select-Object FullName,LastWriteTime
Get-CimInstance Win32_Process -Filter 'ProcessId=34612' | Select-Object ProcessId,Name
```

所选音视频源重入PID11272，实际命令：
```powershell
$reentry=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r380-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r380-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($reentry.Id)"
$reentry.WaitForExit()
Write-Output "REENTRY_EXIT=$($reentry.ExitCode)"
```

## 结果和失败边界

启动对象WaitForExit取得CLI_EXIT=1，三初始源EXIT=0；重入对象取得REENTRY_EXIT=0。四个源均完整120秒/3600帧，但CLI在重入结束前已失败，不计完整处理或caller-stop成功。

11:58:53.347首个worker错误来自sender node81，seq171936：not_before286365532340 > not_after286365442084，相差90256ns；deadline/prefix286365657124，原protocol deadline286399019401。失败active batch171892..171942，已提交44包，43次wait总63.2617ms、累计target lateness54.38428ms；seq171904原等待211440ns，WaitForMultipleObjects区间33.9256ms，result=1、TID4036，单次target lateness33.72156ms。此为失败batch的直接证据；不以更早全局worst-success82663替代首因。

最终errors/workerErrors=2、pressure=0、stalledIntervals=0；219474预约/释放匹配归零。CPU平均整机进程1.567348%、峰5.567929%；WS初始209342464、峰271364096、最终261853184bytes。sender提交171935datagrams/201216588payload bytes；delivery=not_proven。原r379合成等待失败未关闭，本轮又复现发送等待失败。

## 新诊断有效性与限制

同一最大compose样本：wall47.0356ms，batch_completion46.2865ms，GPU event区间46.8623ms，PID34612/TID18808；start/end record、sync、elapsed原码均0，availability=1。sample包含同批context/stream和steady时间，没有将独立最大值拼接。GPU区间包含event之间设备时间，不能解释start前排队、纯拷贝时间或精确CPU Ready时间；该样本早于首错，不是seq171904等待的根因证明。诊断可用不等于r379修复。

## 清理命令与状态

CLI自行失败，无caller-stop。VLC退出命令：
```powershell
$v=Get-CimInstance Win32_Process -Filter "ProcessId=3204"
if(-not $v -or $v.Name -ne 'vlc.exe' -or $v.CommandLine -notlike '*composition-r380*'){throw 'VLC identity mismatch'}
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 3204){throw 'VLC port mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
try{$s=$c.GetStream();$b=[Text.Encoding]::ASCII.GetBytes('quit'+[char]10);$s.Write($b,0,$b.Length)}finally{$c.Dispose()}
```

同执行环境复查六PID及UDP62024/TCP63020均无残留。按文末精确清单清理本轮文件，保留指定测试源。


## CLI诊断（全部非周期行）

~~~text
[CLI] composition_inputs=3 grid=fixed audio_source=2 audio=on max_duration=source_driven hw=planner-highest-score
[2026-11-02 11:54:07.487] [info] [graph][planner.capability] composition_protocol_installed source=0
[2026-11-02 11:54:07.488] [info] [graph][planner.capability] composition_protocol_installed source=1
[2026-11-02 11:54:07.490] [info] [graph][planner.capability] composition_protocol_installed source=2
[2026-11-02 11:54:07.590] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 11:54:07.591] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 11:54:07.591] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-r380.source.0 decoder=h264
[2026-11-02 11:54:11.975] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-11-02 11:54:11.976] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-11-02 11:54:11.982] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-11-02 11:54:12.089] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 11:54:12.089] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 11:54:12.089] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-r380.source.1 decoder=h264
[2026-11-02 11:54:12.151] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 11:54:12.151] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 11:54:12.151] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-r380.source.2 decoder=h264
[2026-11-02 11:54:12.248] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-11-02 11:54:12.294] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-11-02 11:54:12.295] [info] [graph][planner.capability] canvas_cuda_stream stage=prepared device_stream=0000000000000000 private_nonblocking=1
[2026-11-02 11:54:12.301] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 11:54:12.302] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 11:54:12.302] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-11-02 11:54:12.309] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 11:54:12.309] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 11:54:12.309] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-11-02 11:54:12.315] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 11:54:12.315] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 11:54:12.315] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-11-02 11:54:12.319] [info] [graph][planner.capability] composition_preparation_storage stage=graph_bound scope=shared_reservations capacity=2147483648 retained=831943242 high_water=849317021
[CLI] audio_plan branch=transcode_frame reason=transcode_required_by_output_frame_contract codec=aac sample_rate=44100 channels=2 access_unit_samples=1024 encoder=aac bitrate_kbps=192
[CLI] prepared_output_id=1
[CLI] selected_chain=fixed-grid-composition score=0 decoder=h264,h264,h264 filter=scale_cuda=640:360,scale_cuda=640:360,scale_cuda=640:360 encoder=hevc_nvenc
[2026-11-02 11:54:12.323] [info] [graph][runtime.lifecycle] compile.begin
[2026-11-02 11:54:12.324] [info] [graph][runtime.channel] create channel=1 edge=1 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 11:54:12.324] [info] [graph][runtime.channel] create channel=2 edge=2 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 11:54:12.325] [info] [graph][runtime.channel] create channel=3 edge=3 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-02 11:54:12.325] [info] [graph][runtime.channel] create channel=4 edge=4 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.325] [info] [graph][runtime.channel] create channel=5 edge=5 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.325] [info] [graph][runtime.channel] create channel=6 edge=6 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.325] [info] [graph][runtime.channel] create channel=7 edge=7 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.325] [info] [graph][runtime.channel] create channel=8 edge=8 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.325] [info] [graph][runtime.channel] create channel=9 edge=9 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:54:12.325] [info] [graph][runtime.channel] create channel=10 edge=10 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.325] [info] [graph][runtime.channel] create channel=11 edge=11 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.325] [info] [graph][runtime.channel] create channel=12 edge=12 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:54:12.326] [info] [graph][runtime.channel] create channel=13 edge=13 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:54:12.326] [info] [graph][runtime.channel] create channel=14 edge=14 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.326] [info] [graph][runtime.channel] create channel=15 edge=15 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.326] [info] [graph][runtime.channel] create channel=16 edge=16 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.326] [info] [graph][runtime.channel] create channel=17 edge=17 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.326] [info] [graph][runtime.channel] create channel=18 edge=18 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.326] [info] [graph][runtime.channel] create channel=19 edge=19 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.326] [info] [graph][runtime.channel] create channel=20 edge=20 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.326] [info] [graph][runtime.channel] create channel=21 edge=21 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-02 11:54:12.326] [info] [graph][runtime.channel] create channel=22 edge=22 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 11:54:12.327] [info] [graph][runtime.channel] create channel=23 edge=23 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:54:12.327] [info] [graph][runtime.channel] create channel=24 edge=24 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 11:54:12.327] [info] [graph][runtime.channel] create channel=25 edge=25 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:54:12.327] [info] [graph][runtime.channel] create channel=26 edge=26 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:54:12.327] [info] [graph][runtime.channel] create channel=27 edge=27 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:54:12.327] [info] [graph][runtime.channel] create channel=28 edge=28 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:54:12.327] [info] [graph][runtime.channel] create channel=29 edge=29 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:54:12.327] [info] [graph][runtime.channel] create channel=30 edge=30 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.327] [info] [graph][runtime.channel] create channel=31 edge=31 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.327] [info] [graph][runtime.channel] create channel=32 edge=32 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.328] [info] [graph][runtime.channel] create channel=33 edge=33 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.328] [info] [graph][runtime.channel] create channel=34 edge=34 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.328] [info] [graph][runtime.channel] create channel=35 edge=35 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.328] [info] [graph][runtime.channel] create channel=36 edge=36 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:54:12.328] [info] [graph][runtime.channel] create channel=37 edge=37 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.328] [info] [graph][runtime.channel] create channel=38 edge=38 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.328] [info] [graph][runtime.channel] create channel=39 edge=39 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:54:12.328] [info] [graph][runtime.channel] create channel=40 edge=40 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:54:12.328] [info] [graph][runtime.channel] create channel=41 edge=41 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.328] [info] [graph][runtime.channel] create channel=42 edge=42 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.329] [info] [graph][runtime.channel] create channel=43 edge=43 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.329] [info] [graph][runtime.channel] create channel=44 edge=44 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.329] [info] [graph][runtime.channel] create channel=45 edge=45 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.329] [info] [graph][runtime.channel] create channel=46 edge=46 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.329] [info] [graph][runtime.channel] create channel=47 edge=47 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.329] [info] [graph][runtime.channel] create channel=48 edge=48 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-02 11:54:12.329] [info] [graph][runtime.channel] create channel=49 edge=49 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 11:54:12.329] [info] [graph][runtime.channel] create channel=50 edge=50 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:54:12.329] [info] [graph][runtime.channel] create channel=51 edge=51 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 11:54:12.329] [info] [graph][runtime.channel] create channel=52 edge=52 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:54:12.330] [info] [graph][runtime.channel] create channel=53 edge=53 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:54:12.330] [info] [graph][runtime.channel] create channel=54 edge=54 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:54:12.330] [info] [graph][runtime.channel] create channel=55 edge=55 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:54:12.330] [info] [graph][runtime.channel] create channel=56 edge=56 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:54:12.330] [info] [graph][runtime.channel] create channel=57 edge=57 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.330] [info] [graph][runtime.channel] create channel=58 edge=58 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.330] [info] [graph][runtime.channel] create channel=59 edge=59 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.330] [info] [graph][runtime.channel] create channel=60 edge=60 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.330] [info] [graph][runtime.channel] create channel=61 edge=61 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.330] [info] [graph][runtime.channel] create channel=62 edge=62 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.331] [info] [graph][runtime.channel] create channel=63 edge=63 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.331] [info] [graph][runtime.channel] create channel=64 edge=64 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.331] [info] [graph][runtime.channel] create channel=65 edge=65 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.331] [info] [graph][runtime.channel] create channel=66 edge=66 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:54:12.331] [info] [graph][runtime.channel] create channel=67 edge=67 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:54:12.331] [info] [graph][runtime.channel] create channel=68 edge=68 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.331] [info] [graph][runtime.channel] create channel=69 edge=69 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.331] [info] [graph][runtime.channel] create channel=70 edge=70 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.331] [info] [graph][runtime.channel] create channel=71 edge=71 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:54:12.331] [info] [graph][runtime.channel] create channel=72 edge=72 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:54:12.332] [info] [graph][runtime.channel] create channel=73 edge=73 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:54:12.332] [info] [graph][runtime.channel] create channel=74 edge=74 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:54:12.332] [info] [graph][runtime.channel] create channel=75 edge=75 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.332] [info] [graph][runtime.channel] create channel=76 edge=76 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.332] [info] [graph][runtime.channel] create channel=77 edge=77 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.332] [info] [graph][runtime.channel] create channel=78 edge=78 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.332] [info] [graph][runtime.channel] create channel=79 edge=79 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.332] [info] [graph][runtime.channel] create channel=80 edge=80 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.332] [info] [graph][runtime.channel] create channel=81 edge=81 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.333] [info] [graph][runtime.channel] create channel=82 edge=82 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.333] [info] [graph][runtime.channel] create channel=83 edge=83 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-02 11:54:12.333] [info] [graph][runtime.channel] create channel=84 edge=84 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 11:54:12.333] [info] [graph][runtime.channel] create channel=85 edge=85 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:54:12.333] [info] [graph][runtime.channel] create channel=86 edge=86 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 11:54:12.333] [info] [graph][runtime.channel] create channel=87 edge=87 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:54:12.333] [info] [graph][runtime.channel] create channel=88 edge=88 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:54:12.333] [info] [graph][runtime.channel] create channel=89 edge=89 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:54:12.333] [info] [graph][runtime.channel] create channel=90 edge=90 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:54:12.333] [info] [graph][runtime.channel] create channel=91 edge=91 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-02 11:54:12.334] [info] [graph][runtime.channel] create channel=92 edge=92 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 11:54:12.334] [info] [graph][runtime.channel] create channel=93 edge=93 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:54:12.334] [info] [graph][runtime.channel] create channel=94 edge=94 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 11:54:12.334] [info] [graph][runtime.channel] create channel=95 edge=95 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:54:12.334] [info] [graph][runtime.channel] create channel=96 edge=96 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:54:12.334] [info] [graph][runtime.channel] create channel=97 edge=97 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:54:12.334] [info] [graph][runtime.channel] create channel=98 edge=98 stream=Audio edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.334] [info] [graph][runtime.channel] create channel=99 edge=99 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:54:12.334] [info] [graph][runtime.channel] create channel=100 edge=100 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.334] [info] [graph][runtime.channel] create channel=101 edge=101 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0
[2026-11-02 11:54:12.334] [info] [graph][runtime.channel] create channel=102 edge=102 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 11:54:12.335] [info] [graph][runtime.channel] create channel=103 edge=103 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 11:54:12.335] [info] [graph][runtime.channel] create channel=104 edge=104 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:54:12.335] [info] [graph][runtime.channel] create channel=105 edge=105 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0
[2026-11-02 11:54:12.335] [info] [graph][runtime.channel] create channel=106 edge=106 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.335] [info] [graph][runtime.channel] create channel=107 edge=107 stream=Video edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-02 11:54:12.335] [info] [graph][runtime.channel] create channel=108 edge=108 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-02 11:54:12.335] [info] [graph][runtime.channel] create channel=109 edge=109 stream=Any edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-02 11:54:12.336] [info] [graph][runtime.channel] create channel=110 edge=110 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.336] [info] [graph][runtime.channel] create channel=111 edge=111 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-02 11:54:12.336] [info] [graph][runtime.channel] create channel=112 edge=112 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:54:12.337] [info] [graph][runtime.channel] create channel=113 edge=113 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 11:54:12.337] [info] [graph][runtime.channel] create channel=114 edge=114 stream=Any edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-02 11:54:12.338] [info] [graph][runtime.channel] create channel=115 edge=115 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 11:54:12.338] [info] [graph][runtime.channel] create channel=116 edge=116 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-02 11:54:12.338] [info] [graph][runtime.channel] create channel=117 edge=117 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-02 11:54:12.338] [info] [graph][runtime.channel] create channel=118 edge=118 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-02 11:54:12.338] [info] [graph][runtime.channel] create channel=119 edge=119 stream=Any edge_kind=EncodedPacket payload=Unknown queued=0
[2026-11-02 11:54:12.338] [info] [graph][runtime.channel] create channel=120 edge=120 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-02 11:54:12.339] [info] [graph][runtime.channel] create channel=121 edge=121 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-02 11:54:12.339] [info] [graph][runtime.channel] create channel=122 edge=122 stream=Metadata edge_kind=Unknown payload=Unknown queued=0
[2026-11-02 11:54:12.339] [info] [graph][runtime.channel] create channel=123 edge=123 stream=Metadata edge_kind=Unknown payload=Unknown queued=0
[2026-11-02 11:54:12.339] [info] [graph][runtime.lifecycle] compiled nodes=81 edges=123 channels=123 execution_order=1->3->8->27->46->47->9->21->28->40->63->48->69->10->29->49->11->12->30->31->50->51->52->13->32->53->14->17->33->36->54->55->59->15->34->56->57->16->35->58->18->37->60->19->38->61->20->39->62->22->41->64->70->23->42->65->71->24->43->66->72->26->45->68->73->25->44->67->6->2->7->80->4->77->5->74->75->78->76->79->81
[2026-11-02 11:54:12.339] [info] [graph][runtime.lifecycle] compile.done state=DefaultRegistrationPending
[2026-11-02 11:54:12.340] [info] [graph][runtime.node] register node=1 name=composition-r380.output.video.codec_resolver kind=CodecResolver
[2026-11-02 11:54:12.340] [info] [graph][runtime.node] register node=2 name=composition-r380.output.video.encode kind=VideoEncode
[2026-11-02 11:54:12.340] [info] [graph][runtime.node] register node=3 name=composition-r380.output.audio.codec_resolver kind=AudioCodecResolver
[2026-11-02 11:54:12.340] [info] [graph][runtime.node] register node=4 name=composition-r380.output.audio.encode kind=AudioEncode
[2026-11-02 11:54:12.340] [info] [graph][runtime.node] register node=5 name=composition-r380.output.audio.canonicalizer kind=EncodedAudioCanonicalizer
[2026-11-02 11:54:12.340] [info] [graph][runtime.node] register node=8 name=composition-r380.source_0.input kind=RawRtpInput
[2026-11-02 11:54:12.341] [info] [graph][runtime.node] register node=9 name=composition-r380.source_0.rtp.clock_group kind=RtpClockGroup
[2026-11-02 11:54:12.341] [info] [graph][runtime.node] register node=10 name=composition-r380.source_0.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-11-02 11:54:12.341] [info] [graph][runtime.node] register node=11 name=composition-r380.source_0.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-11-02 11:54:12.341] [info] [graph][runtime.node] register node=12 name=composition-r380.source_0.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-11-02 11:54:12.341] [info] [graph][runtime.node] register node=13 name=composition-r380.source_0.av_sync.source_clock kind=SourceClockStateFanout
[2026-11-02 11:54:12.341] [info] [graph][runtime.node] register node=14 name=composition-r380.source_0.av_sync.video.generation_gate kind=LockedPacketGate
[2026-11-02 11:54:12.341] [info] [graph][runtime.node] register node=15 name=composition-r380.source_0.av_sync.video.canonical_input kind=CanonicalInput
[2026-11-02 11:54:12.341] [info] [graph][runtime.node] register node=16 name=composition-r380.source_0.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-11-02 11:54:12.341] [info] [graph][runtime.node] register node=17 name=composition-r380.source_0.av_sync.startup.clock kind=AvStartupClock
[2026-11-02 11:54:12.341] [info] [graph][runtime.node] register node=18 name=composition-r380.source_0.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-11-02 11:54:12.342] [info] [graph][runtime.node] register node=20 name=composition-r380.source_0.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-11-02 11:54:12.342] [info] [graph][runtime.node] register node=21 name=composition-r380.source_0.video.codec_resolver kind=CodecResolver
[2026-11-02 11:54:12.342] [info] [graph][runtime.node] register node=22 name=composition-r380.source_0.video.decode kind=VideoDecode
[2026-11-02 11:54:12.342] [info] [graph][runtime.node] register node=23 name=composition-r380.source_0.video.hwtransfer kind=HardwareTransfer
[2026-11-02 11:54:12.342] [info] [graph][runtime.node] register node=24 name=composition-r380.source_0.video.framerate kind=VideoFrameRate
[2026-11-02 11:54:12.342] [info] [graph][runtime.node] register node=25 name=composition-r380.source_0.video.preparation kind=VideoPreparation
[2026-11-02 11:54:12.342] [info] [graph][runtime.node] register node=26 name=composition-r380.source_0.video.filter kind=VideoFilter
[2026-11-02 11:54:12.342] [info] [graph][runtime.node] register node=27 name=composition-r380.source_1.input kind=RawRtpInput
[2026-11-02 11:54:12.342] [info] [graph][runtime.node] register node=28 name=composition-r380.source_1.rtp.clock_group kind=RtpClockGroup
[2026-11-02 11:54:12.342] [info] [graph][runtime.node] register node=29 name=composition-r380.source_1.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-11-02 11:54:12.343] [info] [graph][runtime.node] register node=30 name=composition-r380.source_1.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-11-02 11:54:12.343] [info] [graph][runtime.node] register node=31 name=composition-r380.source_1.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-11-02 11:54:12.343] [info] [graph][runtime.node] register node=32 name=composition-r380.source_1.av_sync.source_clock kind=SourceClockStateFanout
[2026-11-02 11:54:12.343] [info] [graph][runtime.node] register node=33 name=composition-r380.source_1.av_sync.video.generation_gate kind=LockedPacketGate
[2026-11-02 11:54:12.343] [info] [graph][runtime.node] register node=34 name=composition-r380.source_1.av_sync.video.canonical_input kind=CanonicalInput
[2026-11-02 11:54:12.343] [info] [graph][runtime.node] register node=35 name=composition-r380.source_1.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-11-02 11:54:12.343] [info] [graph][runtime.node] register node=36 name=composition-r380.source_1.av_sync.startup.clock kind=AvStartupClock
[2026-11-02 11:54:12.343] [info] [graph][runtime.node] register node=37 name=composition-r380.source_1.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-11-02 11:54:12.343] [info] [graph][runtime.node] register node=39 name=composition-r380.source_1.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-11-02 11:54:12.343] [info] [graph][runtime.node] register node=40 name=composition-r380.source_1.video.codec_resolver kind=CodecResolver
[2026-11-02 11:54:12.343] [info] [graph][runtime.node] register node=41 name=composition-r380.source_1.video.decode kind=VideoDecode
[2026-11-02 11:54:12.344] [info] [graph][runtime.node] register node=42 name=composition-r380.source_1.video.hwtransfer kind=HardwareTransfer
[2026-11-02 11:54:12.344] [info] [graph][runtime.node] register node=43 name=composition-r380.source_1.video.framerate kind=VideoFrameRate
[2026-11-02 11:54:12.344] [info] [graph][runtime.node] register node=44 name=composition-r380.source_1.video.preparation kind=VideoPreparation
[2026-11-02 11:54:12.344] [info] [graph][runtime.node] register node=45 name=composition-r380.source_1.video.filter kind=VideoFilter
[2026-11-02 11:54:12.344] [info] [graph][runtime.node] register node=46 name=composition-r380.source_2.video.input kind=RawRtpInput
[2026-11-02 11:54:12.344] [info] [graph][runtime.node] register node=47 name=composition-r380.source_2.audio.input kind=RawRtpInput
[2026-11-02 11:54:12.344] [info] [graph][runtime.node] register node=48 name=composition-r380.source_2.rtp.clock_group kind=RtpClockGroup
[2026-11-02 11:54:12.344] [info] [graph][runtime.node] register node=49 name=composition-r380.source_2.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-11-02 11:54:12.344] [info] [graph][runtime.node] register node=50 name=composition-r380.source_2.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-11-02 11:54:12.344] [info] [graph][runtime.node] register node=51 name=composition-r380.source_2.av_sync.audio.protocol_binder kind=RtpPacketClockBinder
[2026-11-02 11:54:12.344] [info] [graph][runtime.node] register node=52 name=composition-r380.source_2.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-11-02 11:54:12.345] [info] [graph][runtime.node] register node=53 name=composition-r380.source_2.av_sync.source_clock kind=SourceClockStateFanout
[2026-11-02 11:54:12.345] [info] [graph][runtime.node] register node=54 name=composition-r380.source_2.av_sync.video.generation_gate kind=LockedPacketGate
[2026-11-02 11:54:12.345] [info] [graph][runtime.node] register node=55 name=composition-r380.source_2.av_sync.audio.generation_gate kind=LockedPacketGate
[2026-11-02 11:54:12.345] [info] [graph][runtime.node] register node=56 name=composition-r380.source_2.av_sync.video.canonical_input kind=CanonicalInput
[2026-11-02 11:54:12.345] [info] [graph][runtime.node] register node=57 name=composition-r380.source_2.av_sync.audio.canonical_input kind=CanonicalInput
[2026-11-02 11:54:12.345] [info] [graph][runtime.node] register node=58 name=composition-r380.source_2.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-11-02 11:54:12.345] [info] [graph][runtime.node] register node=59 name=composition-r380.source_2.av_sync.startup.clock kind=AvStartupClock
[2026-11-02 11:54:12.345] [info] [graph][runtime.node] register node=60 name=composition-r380.source_2.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-11-02 11:54:12.345] [info] [graph][runtime.node] register node=62 name=composition-r380.source_2.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-11-02 11:54:12.345] [info] [graph][runtime.node] register node=63 name=composition-r380.source_2.video.codec_resolver kind=CodecResolver
[2026-11-02 11:54:12.345] [info] [graph][runtime.node] register node=64 name=composition-r380.source_2.video.decode kind=VideoDecode
[2026-11-02 11:54:12.346] [info] [graph][runtime.node] register node=65 name=composition-r380.source_2.video.hwtransfer kind=HardwareTransfer
[2026-11-02 11:54:12.346] [info] [graph][runtime.node] register node=66 name=composition-r380.source_2.video.framerate kind=VideoFrameRate
[2026-11-02 11:54:12.346] [info] [graph][runtime.node] register node=67 name=composition-r380.source_2.video.preparation kind=VideoPreparation
[2026-11-02 11:54:12.346] [info] [graph][runtime.node] register node=68 name=composition-r380.source_2.video.filter kind=VideoFilter
[2026-11-02 11:54:12.346] [info] [graph][runtime.node] register node=69 name=composition-r380.source_2.audio.encode.codec_resolver kind=AudioCodecResolver
[2026-11-02 11:54:12.346] [info] [graph][runtime.node] register node=70 name=composition-r380.source_2.audio.encode.decode kind=AudioDecode
[2026-11-02 11:54:12.346] [info] [graph][runtime.node] register node=71 name=composition-r380.source_2.audio.encode.startup_trim kind=Unknown
[2026-11-02 11:54:12.346] [info] [graph][runtime.node] register node=72 name=composition-r380.source_2.audio.encode.drift_controller kind=AudioDriftController
[2026-11-02 11:54:12.346] [info] [graph][runtime.node] register node=73 name=composition-r380.source_2.audio.encode.resample kind=AudioResample
[2026-11-02 11:54:12.347] [info] [graph][runtime.node] register node=74 name=composition-r380.output.scheduler.scheduler kind=AvOutputScheduler
[2026-11-02 11:54:12.347] [info] [graph][runtime.node] register node=75 name=composition-r380.output.scheduler.router kind=ScheduledOutputRouter
[2026-11-02 11:54:12.347] [info] [graph][runtime.node] register node=76 name=composition-r380.output.mpegts.mux kind=FileMux
[2026-11-02 11:54:12.347] [info] [graph][runtime.node] register node=77 name=composition-r380.output.mpegts.plan.source kind=Unknown
[2026-11-02 11:54:12.347] [info] [graph][runtime.node] register node=78 name=composition-r380.output.mpegts.scheduled.adapter kind=Unknown
[2026-11-02 11:54:12.347] [info] [graph][runtime.node] register node=79 name=composition-r380.output.mpegts.datagram.materializer kind=MpegTsDatagramMaterializer
[2026-11-02 11:54:12.348] [info] [graph][runtime.node] register node=80 name=composition-r380.output.mpegts.transport.plan kind=DatagramTransportPlanSource
[2026-11-02 11:54:12.348] [info] [graph][runtime.node] register node=81 name=composition-r380.output.mpegts.transport.sender kind=ScheduledDatagramSender
[2026-11-02 11:54:12.348] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=waiting_for_random_access detail="" description=""
[2026-11-02 11:54:12.348] [info] [graph][runtime.lifecycle] start.begin mode=threaded
[2026-11-02 11:54:12.350] [info] [graph][runtime.lifecycle] worker.finished node=1
[2026-11-02 11:54:12.350] [info] [graph][runtime.node] worker.thread_cpu node=1 available=1 cpu_ns=0 process_calls=1
[2026-11-02 11:54:12.351] [info] [graph][runtime.lifecycle] worker.finished node=3
[2026-11-02 11:54:12.351] [info] [graph][runtime.node] worker.thread_cpu node=3 available=1 cpu_ns=0 process_calls=1
[2026-11-02 11:54:12.351] [info] [graph][runtime.lifecycle] start.done state=ThreadedRunning
[2026-11-02 11:54:12.352] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:54:12.352] [info] [graph][runtime.lifecycle] worker.finished node=69
[2026-11-02 11:54:12.353] [info] [graph][runtime.node] worker.thread_cpu node=69 available=1 cpu_ns=0 process_calls=1
[2026-11-02 11:54:12.354] [info] [graph][runtime.lifecycle] worker.finished node=21
[2026-11-02 11:54:12.354] [info] [graph][runtime.node] worker.thread_cpu node=21 available=1 cpu_ns=0 process_calls=2
[2026-11-02 11:54:12.355] [info] [graph][runtime.lifecycle] worker.finished node=63
[2026-11-02 11:54:12.355] [info] [graph][runtime.node] worker.thread_cpu node=63 available=1 cpu_ns=0 process_calls=2
[2026-11-02 11:54:12.356] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-11-02 11:54:12.356] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=15 generation=1
[2026-11-02 11:54:12.358] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:54:12.358] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=release source_identity=composition-r380.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4014000000 duration_ns=33333333 decoded_sequence=unavailable pts=3911470040 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11128075419500 group=composition-r380.source.2 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-4014000000 epoch_master_release_ns=5476490100 video_count=57 min_presentation_ns=-4014000000 max_end_ns=-2114000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.359] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=published source_identity=composition-r380.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4014000000 duration_ns=33333333 decoded_sequence=unavailable pts=3911470040 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11128075649700 group=composition-r380.source.2 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.359] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=3911470040 dts=3911470040 size=317016 generation=1 sequence=15
[2026-11-02 11:54:12.359] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=input source_identity=composition-r380.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4014000000 duration_ns=33333333 decoded_sequence=unavailable pts=3911470040 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11128076287800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.359] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-11-02 11:54:12.360] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=15 generation=1
[2026-11-02 11:54:12.360] [info] [graph][runtime.node] source_video_trace node_id=20 node=MediaAvBoundReleaseExtractorNode stage=release source_identity=composition-r380.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4015000000 duration_ns=33333333 decoded_sequence=unavailable pts=2873075785 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11128077407600 group=composition-r380.source.0 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=-4015000000 epoch_master_release_ns=5479362300 video_count=15 min_presentation_ns=-4015000000 max_end_ns=-3515000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.361] [info] [graph][runtime.node] source_video_trace node_id=20 node=MediaAvBoundReleaseExtractorNode stage=published source_identity=composition-r380.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4015000000 duration_ns=33333333 decoded_sequence=unavailable pts=2873075785 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11128077707100 group=composition-r380.source.0 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.361] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=2873075785 dts=2873075785 size=317016 generation=1 sequence=15
[2026-11-02 11:54:12.361] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=input source_identity=composition-r380.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4015000000 duration_ns=33333333 decoded_sequence=unavailable pts=2873075785 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11128078343000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.364] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:54:12.364] [info] [graph][runtime.lifecycle] worker.finished node=40
[2026-11-02 11:54:12.364] [info] [graph][runtime.node] worker.thread_cpu node=40 available=1 cpu_ns=0 process_calls=1
[2026-11-02 11:54:12.365] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-11-02 11:54:12.365] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=15 generation=1
[2026-11-02 11:54:12.366] [info] [graph][runtime.node] source_video_trace node_id=39 node=MediaAvBoundReleaseExtractorNode stage=release source_identity=composition-r380.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4014000000 duration_ns=33333333 decoded_sequence=unavailable pts=4252255561 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11128082835700 group=composition-r380.source.1 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-4014000000 epoch_master_release_ns=5484780900 video_count=15 min_presentation_ns=-4014000000 max_end_ns=-3514000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.366] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=4252255561 dts=4252255561 size=317016 generation=1 sequence=15
[2026-11-02 11:54:12.366] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=input source_identity=composition-r380.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4014000000 duration_ns=33333333 decoded_sequence=unavailable pts=4252255561 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11128083143200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.366] [info] [graph][runtime.node] source_video_trace node_id=39 node=MediaAvBoundReleaseExtractorNode stage=published source_identity=composition-r380.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4014000000 duration_ns=33333333 decoded_sequence=unavailable pts=4252255561 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11128083024100 group=composition-r380.source.1 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.366] [info] [graph][runtime.node] video_encode.bind_encoder codec=hevc_nvenc pix_fmt=cuda rate_control=cbr bitrate=8000000 min_bitrate=8000000 max_bitrate=8000000 buffer_size=267000 fps=30/1 gop=60 frame_kind=hardware hwaccel=cuda hw_device_ctx=set hw_frames_ctx=set
[2026-11-02 11:54:12.366] [info] [graph][runtime.node] audio_encode_trace stage=codec_bound
[2026-11-02 11:54:12.390] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-11-02 11:54:12.391] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=accepted source_identity=composition-r380.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4014000000 duration_ns=33333333 decoded_sequence=unavailable pts=3911470040 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11128107707200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.391] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=3911470040 format=cuda size=1280x720 buffer_identity=0xb08600000 drm_prime=0 software_frame=0
[2026-11-02 11:54:12.391] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=resolved source_identity=composition-r380.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4014000000 duration_ns=33333333 decoded_sequence=unavailable pts=3911470040 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11128107991500 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.391] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=published source_identity=composition-r380.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4014000000 duration_ns=33333333 decoded_sequence=unavailable pts=3911470040 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11128108102400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.391] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=000002125994A5C0 stream=Video payload=Frame pts=3911470040 dts=3911470040 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:54:12.391] [info] [graph][runtime.node] source_video_trace node_id=65 node=HardwareTransferNode stage=published source_identity=composition-r380.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4014000000 duration_ns=33333333 decoded_sequence=unavailable pts=3911470040 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11128108341900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.391] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=000002125994A5C0 stream=Video payload=Frame pts=3911470040 dts=3911470040 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:54:12.391] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=000002125994A5C0 stream=Video payload=Frame pts=3911470040 dts=3911470040 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:54:12.391] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-11-02 11:54:12.392] [info] [graph][runtime.node] source_video_trace node_id=66 node=VideoFrameRateNode stage=published source_identity=composition-r380.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4014000000 duration_ns=33333333 decoded_sequence=unavailable pts=3911470040 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11128108772800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.392] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=000002125994BD60 stream=Video payload=Frame pts=3911470040 dts=3911470040 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:54:12.392] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=000002125994BD60 stream=Video payload=Frame pts=3911470040 dts=3911470040 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:54:12.392] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-11-02 11:54:12.392] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-11-02 11:54:12.393] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=accepted source_identity=composition-r380.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4015000000 duration_ns=33333333 decoded_sequence=unavailable pts=2873075785 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11128109579500 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.393] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=2873075785 format=cuda size=1280x720 buffer_identity=0xb08800000 drm_prime=0 software_frame=0
[2026-11-02 11:54:12.393] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=resolved source_identity=composition-r380.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4015000000 duration_ns=33333333 decoded_sequence=unavailable pts=2873075785 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11128109885700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.393] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=published source_identity=composition-r380.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4015000000 duration_ns=33333333 decoded_sequence=unavailable pts=2873075785 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11128110002200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.393] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=000002125994B190 stream=Video payload=Frame pts=2873075785 dts=2873075785 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:54:12.393] [info] [graph][runtime.node] source_video_trace node_id=23 node=HardwareTransferNode stage=published source_identity=composition-r380.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4015000000 duration_ns=33333333 decoded_sequence=unavailable pts=2873075785 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11128110244000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.393] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=000002125994B190 stream=Video payload=Frame pts=2873075785 dts=2873075785 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:54:12.393] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=000002125994B190 stream=Video payload=Frame pts=2873075785 dts=2873075785 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:54:12.393] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-11-02 11:54:12.394] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=000002125994C150 stream=Video payload=Frame pts=2873075785 dts=2873075785 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:54:12.394] [info] [graph][runtime.node] source_video_trace node_id=24 node=VideoFrameRateNode stage=published source_identity=composition-r380.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4015000000 duration_ns=33333333 decoded_sequence=unavailable pts=2873075785 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11128110663700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.394] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-11-02 11:54:12.394] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=000002125994C150 stream=Video payload=Frame pts=2873075785 dts=2873075785 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:54:12.394] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-11-02 11:54:12.395] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=accepted source_identity=composition-r380.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4014000000 duration_ns=33333333 decoded_sequence=unavailable pts=4252255561 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11128111645200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.395] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=4252255561 format=cuda size=1280x720 buffer_identity=0xb08a00000 drm_prime=0 software_frame=0
[2026-11-02 11:54:12.395] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=resolved source_identity=composition-r380.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4014000000 duration_ns=33333333 decoded_sequence=unavailable pts=4252255561 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11128112154700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.396] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=published source_identity=composition-r380.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4014000000 duration_ns=33333333 decoded_sequence=unavailable pts=4252255561 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11128112551000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.396] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=000002125994B970 stream=Video payload=Frame pts=4252255561 dts=4252255561 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:54:12.396] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=000002125994B970 stream=Video payload=Frame pts=4252255561 dts=4252255561 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:54:12.396] [info] [graph][runtime.node] source_video_trace node_id=42 node=HardwareTransferNode stage=published source_identity=composition-r380.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4014000000 duration_ns=33333333 decoded_sequence=unavailable pts=4252255561 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11128113275900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.396] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-11-02 11:54:12.397] [info] [graph][runtime.node] source_video_trace node_id=43 node=VideoFrameRateNode stage=published source_identity=composition-r380.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4014000000 duration_ns=33333333 decoded_sequence=unavailable pts=4252255561 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11128113681100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.397] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=000002125994C540 stream=Video payload=Frame pts=4252255561 dts=4252255561 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:54:12.397] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-11-02 11:54:12.397] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=000002125994C540 stream=Video payload=Frame pts=4252255561 dts=4252255561 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:54:12.396] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=000002125994B970 stream=Video payload=Frame pts=4252255561 dts=4252255561 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:54:12.469] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=76
[2026-11-02 11:54:12.469] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=75
[2026-11-02 11:54:12.469] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=input source_identity=composition-r380.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4014000000 duration_ns=33333333 decoded_sequence=unavailable pts=3911470040 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11128186421700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.470] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=input source_identity=composition-r380.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4015000000 duration_ns=33333333 decoded_sequence=unavailable pts=2873075785 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11128186686100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.470] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=72
[2026-11-02 11:54:12.470] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=input source_identity=composition-r380.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4014000000 duration_ns=33333333 decoded_sequence=unavailable pts=4252255561 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11128187155800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.471] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=accepted source_identity=composition-r380.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4015000000 duration_ns=33333333 decoded_sequence=unavailable pts=2873075785 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11128187996800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.471] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=accepted source_identity=composition-r380.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4014000000 duration_ns=33333333 decoded_sequence=unavailable pts=3911470040 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11128188284300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.472] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=resolved source_identity=composition-r380.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4014000000 duration_ns=33333333 decoded_sequence=unavailable pts=1303823 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=11128188811500 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.472] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=resolved source_identity=composition-r380.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4015000000 duration_ns=33333333 decoded_sequence=unavailable pts=957692 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=11128188863500 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.472] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=published source_identity=composition-r380.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4014000000 duration_ns=33333333 decoded_sequence=unavailable pts=1303823 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=11128189053100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.472] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=accepted source_identity=composition-r380.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4014000000 duration_ns=33333333 decoded_sequence=unavailable pts=4252255561 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11128189114900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.472] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=input source_identity=composition-r380.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4014000000 duration_ns=33333333 decoded_sequence=unavailable pts=1303823 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=11128189088500 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.472] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=published source_identity=composition-r380.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4015000000 duration_ns=33333333 decoded_sequence=unavailable pts=957692 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=11128189233200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.472] [info] [graph][runtime.node] source_video_trace node_id=25 node=VideoPreparationNode stage=input source_identity=composition-r380.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4015000000 duration_ns=33333333 decoded_sequence=unavailable pts=957692 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=11128189246800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.472] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb08c00000 drm_prime=0 software_frame=0
[2026-11-02 11:54:12.473] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=resolved source_identity=composition-r380.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4014000000 duration_ns=33333333 decoded_sequence=unavailable pts=1417419 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=11128190042800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.473] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=output_attempt source_identity=composition-r380.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4014000000 duration_ns=33333333 decoded_sequence=unavailable pts=1303823 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=11128190147900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.473] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb08c90000 drm_prime=0 software_frame=0
[2026-11-02 11:54:12.474] [info] [graph][runtime.node] source_video_trace node_id=25 node=VideoPreparationNode stage=output_attempt source_identity=composition-r380.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4015000000 duration_ns=33333333 decoded_sequence=unavailable pts=957692 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=11128190730500 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.474] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=published source_identity=composition-r380.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4014000000 duration_ns=33333333 decoded_sequence=unavailable pts=1417419 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=11128191059400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.474] [info] [graph][runtime.node] source_video_trace node_id=44 node=VideoPreparationNode stage=input source_identity=composition-r380.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4014000000 duration_ns=33333333 decoded_sequence=unavailable pts=1417419 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=11128191073900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.474] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=57 audio_count=23 first_video_sequence=15
[2026-11-02 11:54:12.474] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=15 audio_count=0 first_video_sequence=15
[2026-11-02 11:54:12.475] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb08d20000 drm_prime=0 software_frame=0
[2026-11-02 11:54:12.475] [info] [graph][runtime.node] source_video_trace node_id=44 node=VideoPreparationNode stage=output_attempt source_identity=composition-r380.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4014000000 duration_ns=33333333 decoded_sequence=unavailable pts=1417419 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=11128191782000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.475] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=published source_identity=unavailable identity_link=node_generation_input_source_sequence generation=1 source_sequence=15 presentation_ns=-4014000000 duration_ns=33333333 decoded_sequence=unavailable pts=1303823 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=11128192188600 group=composition-r380.source.2 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.475] [info] [graph][runtime.node] source_video_trace node_id=19 node=ActivationReleaseSequencer stage=activation source_identity=composition-r380.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4015000000 duration_ns=33333333 decoded_sequence=unavailable pts=2873075785 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11128192220200 group=composition-r380.source.0 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=-4015000000 epoch_master_release_ns=5660929968 video_count=15 min_presentation_ns=-4015000000 max_end_ns=-3515000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.475] [info] [graph][runtime.node] source_video_trace node_id=61 node=ActivationReleaseSequencer stage=activation source_identity=composition-r380.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4014000000 duration_ns=33333333 decoded_sequence=unavailable pts=3911470040 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11128192234000 group=composition-r380.source.2 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-4014000000 epoch_master_release_ns=5660856668 video_count=57 min_presentation_ns=-4014000000 max_end_ns=-2114000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.475] [info] [graph][runtime.node] source_video_trace node_id=25 node=VideoPreparationNode stage=published source_identity=unavailable identity_link=node_generation_input_source_sequence generation=1 source_sequence=15 presentation_ns=-4015000000 duration_ns=33333333 decoded_sequence=unavailable pts=957692 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=11128192237400 group=composition-r380.source.0 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.475] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=15 audio_count=0 first_video_sequence=15
[2026-11-02 11:54:12.476] [info] [graph][runtime.node] source_video_trace node_id=20 node=MediaAvBoundReleaseExtractorNode stage=release_committed source_identity=composition-r380.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4015000000 duration_ns=33333333 decoded_sequence=unavailable pts=2873075785 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11128193466000 group=composition-r380.source.0 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=-4015000000 epoch_master_release_ns=5479362300 video_count=15 min_presentation_ns=-4015000000 max_end_ns=-3515000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=17 release_committed_count=1 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.477] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=0
[2026-11-02 11:54:12.476] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=release_committed source_identity=composition-r380.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4014000000 duration_ns=33333333 decoded_sequence=unavailable pts=3911470040 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11128193227200 group=composition-r380.source.2 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-4014000000 epoch_master_release_ns=5476490100 video_count=57 min_presentation_ns=-4014000000 max_end_ns=-2114000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=57 release_count=59 release_committed_count=1 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.476] [info] [graph][runtime.node] audio_decode_trace stage=first_packet
[2026-11-02 11:54:12.476] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_candidate source_identity=composition-r380.source.0.input.video generation=1 steady_ns=11128193389100 source_sequence=15 presentation_ns=-4015000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 11:54:12.476] [info] [graph][runtime.node] source_video_trace node_id=19 node=ActivationReleaseSequencer stage=release_published source_identity=composition-r380.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4015000000 duration_ns=33333333 decoded_sequence=unavailable pts=2873075785 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11128192994800 group=composition-r380.source.0 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=-4015000000 epoch_master_release_ns=5660929968 video_count=15 min_presentation_ns=-4015000000 max_end_ns=-3515000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.477] [info] [graph][runtime.node] source_video_trace node_id=38 node=ActivationReleaseSequencer stage=activation source_identity=composition-r380.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4014000000 duration_ns=33333333 decoded_sequence=unavailable pts=4252255561 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11128193632400 group=composition-r380.source.1 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-4014000000 epoch_master_release_ns=5662293668 video_count=15 min_presentation_ns=-4014000000 max_end_ns=-3514000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.477] [info] [graph][runtime.node] source_video_trace node_id=44 node=VideoPreparationNode stage=published source_identity=unavailable identity_link=node_generation_input_source_sequence generation=1 source_sequence=15 presentation_ns=-4014000000 duration_ns=33333333 decoded_sequence=unavailable pts=1417419 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=11128193664800 group=composition-r380.source.1 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.476] [info] [graph][runtime.node] source_video_trace node_id=61 node=ActivationReleaseSequencer stage=release_published source_identity=composition-r380.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4014000000 duration_ns=33333333 decoded_sequence=unavailable pts=3911470040 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11128193217400 group=composition-r380.source.2 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-4014000000 epoch_master_release_ns=5660856668 video_count=57 min_presentation_ns=-4014000000 max_end_ns=-2114000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.477] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=23
[2026-11-02 11:54:12.479] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_candidate source_identity=composition-r380.source.1.input.video generation=1 steady_ns=11128195626800 source_sequence=15 presentation_ns=-4014000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 11:54:12.479] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_candidate source_identity=composition-r380.source.2.input.video generation=1 steady_ns=11128195775800 source_sequence=15 presentation_ns=-4014000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 11:54:12.478] [info] [graph][runtime.node] source_video_trace node_id=39 node=MediaAvBoundReleaseExtractorNode stage=release_committed source_identity=composition-r380.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4014000000 duration_ns=33333333 decoded_sequence=unavailable pts=4252255561 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11128195210000 group=composition-r380.source.1 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-4014000000 epoch_master_release_ns=5484780900 video_count=15 min_presentation_ns=-4014000000 max_end_ns=-3514000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=17 release_committed_count=1 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.479] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=0
[2026-11-02 11:54:12.478] [info] [graph][runtime.node] source_video_trace node_id=38 node=ActivationReleaseSequencer stage=release_published source_identity=composition-r380.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4014000000 duration_ns=33333333 decoded_sequence=unavailable pts=4252255561 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11128194997800 group=composition-r380.source.1 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-4014000000 epoch_master_release_ns=5662293668 video_count=15 min_presentation_ns=-4014000000 max_end_ns=-3514000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:54:12.477] [info] [graph][runtime.node] audio_decode_trace stage=first_frame samples=1024 rate=44100
[2026-11-02 11:54:12.479] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1 source_master_ns=5663314718 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-11-02 11:54:12.480] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3 source_master_ns=5709754628 raw_phase_ns=9072 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=45346
[2026-11-02 11:54:12.480] [info] [graph][runtime.node] audio_encode_trace stage=first_frame
[2026-11-02 11:54:12.480] [info] [graph][runtime.node] output_preparation stage=transport_activate generation=1 elapsed_ns=235400 planned_bytes=180767114966 planned_items=1 event=scope_exit
[2026-11-02 11:54:12.480] [info] [graph][runtime.node] output_preparation stage=global_sequence generation=1 elapsed_ns=28500 planned_bytes=68968 planned_items=2154 event=scope_exit
[2026-11-02 11:54:12.480] [info] [graph][runtime.node] output_preparation stage=transport_buffer generation=1 elapsed_ns=506100 planned_bytes=180767114966 planned_items=1 event=scope_exit
[2026-11-02 11:54:12.481] [info] [graph][runtime.node] output_preparation stage=ts_packetizer generation=1 elapsed_ns=93900 planned_bytes=180765122912 planned_items=61257 event=scope_exit
[2026-11-02 11:54:12.481] [info] [graph][runtime.node] output_preparation stage=ts_mux generation=1 elapsed_ns=383300 planned_bytes=945416 planned_items=59086 event=scope_exit
[2026-11-02 11:54:12.481] [info] [graph][runtime.node] mpegts_emission_plan generation=1 mode=canonical_access_unit_window access_unit_window_ns=100000000 video_initial_service_window_ns=13990650 audio_initial_service_window_ns=13990650 packet_size_bytes=188 maximum_payload_bytes=1316 per_datagram_overhead_bytes=0
[2026-11-02 11:54:12.482] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-r380.source.0.input.video generation=1 steady_ns=11128198842800 source_sequence=15 presentation_ns=-4015000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 11:54:12.482] [info] [graph][runtime.node] video_encode.input_contract format=cuda size=1280x720 buffer_identity=0xb0a000000 drm_prime=0 software_frame=0
[2026-11-02 11:54:12.482] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-r380.source.1.input.video generation=1 steady_ns=11128199104400 source_sequence=15 presentation_ns=-4014000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 11:54:12.482] [info] [graph][runtime.node] video_encode.generation_start_key_frame pts=0 generation=1
[2026-11-02 11:54:12.483] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-r380.source.2.input.video generation=1 steady_ns=11128199811400 source_sequence=15 presentation_ns=-4014000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 11:54:12.483] [info] [graph][runtime.node] video_encode.first_frame pts=0 generation=attached
[2026-11-02 11:54:12.488] [info] [graph][runtime.node] video_encode.first_submit result=accepted
[2026-11-02 11:54:12.488] [info] [graph][runtime.node] video_encode.first_packet pts=0 dts=0 duration=0 size=108357 side_data_elems=1 received_footprint=108365 side_data[0]=Quality stats:8
[2026-11-02 11:54:12.488] [info] [graph][runtime.node] video_encode.first_packet_footprint bytes=108365 wrapped_size=108357 wrapped_side_data_elems=1
[2026-11-02 11:54:12.489] [info] [graph][runtime.node] av_scheduler_trace stage=first_video_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=5665686068 dispatch_master_ns=5665686068 now_ns=5507806100 node=74 identity_kind=output identity="composition-r380.output" canonical_sequence=1 canonical_created_steady_ns=11128205658800 head_observed_steady_ns=11128205700100 codec_receive_begin_steady_ns=11128205406300 codec_receive_end_steady_ns=11128205406900
[2026-11-02 11:54:12.489] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=5507956900 present_generation=1 present_canonical_target_ns=0 present_canonical_dispatch_ns=0 present_target_master_ns=5665686068 present_dispatch_master_ns=5665686068
[2026-11-02 11:54:12.528] [info] [graph][runtime.node] audio_encode_trace stage=first_packet
[2026-11-02 11:54:12.528] [info] [graph][runtime.node] av_scheduler_trace stage=first_audio_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=5665686068 dispatch_master_ns=5665686068 now_ns=5547159800 node=74 identity_kind=output identity="composition-r380.output" canonical_sequence=1 canonical_created_steady_ns=11128245033100 head_observed_steady_ns=11128245053700 codec_receive_begin_steady_ns=unavailable codec_receive_end_steady_ns=unavailable
[2026-11-02 11:54:12.679] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=running detail="" description=""
[2026-11-02 11:54:13.084] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=48 source_master_ns=6754652587 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=90692
[2026-11-02 11:54:14.106] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=92 source_master_ns=7776330591 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=136038
[2026-11-02 11:54:15.128] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=136 source_master_ns=8798008596 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=181384
[2026-11-02 11:54:16.172] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=181 source_master_ns=9842906554 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=226730
[2026-11-02 11:54:17.002] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2691546262 ntp_seconds=4002580457 ntp_fraction=4294967 arrival_ns=11132718644900
[2026-11-02 11:54:17.002] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2691546262
[2026-11-02 11:54:17.002] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:54:17.002] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3852923137 ntp_seconds=4002580457 ntp_fraction=4294967 arrival_ns=11132718579500
[2026-11-02 11:54:17.003] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3852923137
[2026-11-02 11:54:17.003] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:54:17.002] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1543734501 ntp_seconds=4002580457 ntp_fraction=4294967 arrival_ns=11132718587300
[2026-11-02 11:54:17.004] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1543734501
[2026-11-02 11:54:17.004] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:54:17.015] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2216222031 ntp_seconds=4002580457 ntp_fraction=64424509 arrival_ns=11132732274500
[2026-11-02 11:54:17.015] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2216222031
[2026-11-02 11:54:17.016] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:54:17.194] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=225 source_master_ns=10864584559 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=272076
[2026-11-02 11:54:18.216] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=269 source_master_ns=11886262563 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=317422
[2026-11-02 11:54:19.237] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=313 source_master_ns=12907940568 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=362768
[2026-11-02 11:54:20.282] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=358 source_master_ns=13952838527 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=408114
[2026-11-02 11:54:21.305] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=402 source_master_ns=14974516531 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=453460
[2026-11-02 11:54:22.011] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3852923137 ntp_seconds=4002580462 ntp_fraction=47244640 arrival_ns=11137728038000
[2026-11-02 11:54:22.011] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2691546262 ntp_seconds=4002580462 ntp_fraction=47244640 arrival_ns=11137728138600
[2026-11-02 11:54:22.011] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1543734501 ntp_seconds=4002580462 ntp_fraction=47244640 arrival_ns=11137728004000
[2026-11-02 11:54:22.011] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3852923137
[2026-11-02 11:54:22.011] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2691546262
[2026-11-02 11:54:22.012] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:54:22.012] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:54:22.012] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1543734501
[2026-11-02 11:54:22.012] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:54:22.036] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2216222031 ntp_seconds=4002580462 ntp_fraction=154618822 arrival_ns=11137753190700
[2026-11-02 11:54:22.036] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2216222031
[2026-11-02 11:54:22.037] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:54:22.326] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=446 source_master_ns=15996194536 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=498806
[2026-11-02 11:54:23.370] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=491 source_master_ns=17041092495 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=544152
[2026-11-02 11:54:24.392] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=535 source_master_ns=18062770500 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=589498
[2026-11-02 11:54:25.414] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=579 source_master_ns=19084448504 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=634844
[2026-11-02 11:54:26.436] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=623 source_master_ns=20106126509 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=680190
[2026-11-02 11:54:27.014] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2691546262 ntp_seconds=4002580467 ntp_fraction=60129542 arrival_ns=11142730732600
[2026-11-02 11:54:27.015] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2691546262
[2026-11-02 11:54:27.015] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:54:27.014] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3852923137 ntp_seconds=4002580467 ntp_fraction=60129542 arrival_ns=11142730754600
[2026-11-02 11:54:27.015] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3852923137
[2026-11-02 11:54:27.015] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:54:27.028] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1543734501 ntp_seconds=4002580467 ntp_fraction=120259084 arrival_ns=11142745228000
[2026-11-02 11:54:27.029] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1543734501
[2026-11-02 11:54:27.029] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:54:27.043] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2216222031 ntp_seconds=4002580467 ntp_fraction=184683593 arrival_ns=11142759870900
[2026-11-02 11:54:27.044] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2216222031
[2026-11-02 11:54:27.044] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:54:27.480] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=668 source_master_ns=21151024468 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=725536
[2026-11-02 11:54:28.502] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=712 source_master_ns=22172702472 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=770882
[2026-11-02 11:54:29.523] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=756 source_master_ns=23194380477 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=816228
[2026-11-02 11:54:30.546] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=800 source_master_ns=24216058482 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=861574
[2026-11-02 11:54:31.590] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=845 source_master_ns=25260956441 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=906920
[2026-11-02 11:54:32.020] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3852923137 ntp_seconds=4002580472 ntp_fraction=85899345 arrival_ns=11147737475700
[2026-11-02 11:54:32.021] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3852923137
[2026-11-02 11:54:32.021] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:54:32.021] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2691546262 ntp_seconds=4002580472 ntp_fraction=90194313 arrival_ns=11147738005800
[2026-11-02 11:54:32.021] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2691546262
[2026-11-02 11:54:32.022] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:54:32.035] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1543734501 ntp_seconds=4002580472 ntp_fraction=150323855 arrival_ns=11147751916800
[2026-11-02 11:54:32.035] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1543734501
[2026-11-02 11:54:32.036] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:54:32.050] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2216222031 ntp_seconds=4002580472 ntp_fraction=210453397 arrival_ns=11147766662400
[2026-11-02 11:54:32.050] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2216222031
[2026-11-02 11:54:32.050] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:54:32.612] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=889 source_master_ns=26282634445 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=952266
[2026-11-02 11:54:33.634] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=933 source_master_ns=27304312450 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=997612
[2026-11-02 11:54:34.679] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=978 source_master_ns=28349210409 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1042958
[2026-11-02 11:54:35.700] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1022 source_master_ns=29370888414 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1088304
[2026-11-02 11:54:36.722] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1066 source_master_ns=30392566418 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1133650
[2026-11-02 11:54:37.042] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2691546262 ntp_seconds=4002580477 ntp_fraction=180388626 arrival_ns=11152758945000
[2026-11-02 11:54:37.042] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3852923137 ntp_seconds=4002580477 ntp_fraction=180388626 arrival_ns=11152759028500
[2026-11-02 11:54:37.043] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2691546262
[2026-11-02 11:54:37.043] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3852923137
[2026-11-02 11:54:37.043] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:54:37.043] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:54:37.056] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1543734501 ntp_seconds=4002580477 ntp_fraction=240518168 arrival_ns=11152773179500
[2026-11-02 11:54:37.057] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1543734501
[2026-11-02 11:54:37.057] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:54:37.071] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2216222031 ntp_seconds=4002580477 ntp_fraction=304942678 arrival_ns=11152787724000
[2026-11-02 11:54:37.071] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2216222031
[2026-11-02 11:54:37.072] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:54:37.744] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1110 source_master_ns=31414244423 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1178996
[2026-11-02 11:54:38.788] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1155 source_master_ns=32459142382 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1224342
[2026-11-02 11:54:39.810] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1199 source_master_ns=33480820386 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1269688
[2026-11-02 11:54:40.832] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1243 source_master_ns=34502498391 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1315034
[2026-11-02 11:54:41.879] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1288 source_master_ns=35547396350 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1360380
[2026-11-02 11:54:42.056] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3852923137 ntp_seconds=4002580482 ntp_fraction=240518168 arrival_ns=11157772837100
[2026-11-02 11:54:42.056] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3852923137
[2026-11-02 11:54:42.056] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:54:42.057] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1543734501 ntp_seconds=4002580482 ntp_fraction=244813135 arrival_ns=11157773934000
[2026-11-02 11:54:42.057] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1543734501
[2026-11-02 11:54:42.057] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:54:42.079] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2216222031 ntp_seconds=4002580482 ntp_fraction=335007449 arrival_ns=11157795295100
[2026-11-02 11:54:42.079] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2216222031
[2026-11-02 11:54:42.079] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:54:42.081] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2691546262 ntp_seconds=4002580482 ntp_fraction=343597383 arrival_ns=11157796794500
[2026-11-02 11:54:42.081] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2691546262
[2026-11-02 11:54:42.081] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:54:42.900] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1332 source_master_ns=36569074355 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1405726
[2026-11-02 11:54:43.920] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1376 source_master_ns=37590752359 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1451072
[2026-11-02 11:54:44.942] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1420 source_master_ns=38612430364 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1496418
[2026-11-02 11:54:45.987] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1465 source_master_ns=39657328323 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1541764
[2026-11-02 11:54:47.009] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1509 source_master_ns=40679006327 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1587110
[2026-11-02 11:54:47.072] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1543734501 ntp_seconds=4002580487 ntp_fraction=309237645 arrival_ns=11162788905100
[2026-11-02 11:54:47.072] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1543734501
[2026-11-02 11:54:47.072] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:54:47.074] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3852923137 ntp_seconds=4002580487 ntp_fraction=313532612 arrival_ns=11162790494100
[2026-11-02 11:54:47.074] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3852923137
[2026-11-02 11:54:47.074] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:54:47.102] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2691546262 ntp_seconds=4002580487 ntp_fraction=433791696 arrival_ns=11162818287400
[2026-11-02 11:54:47.102] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2691546262
[2026-11-02 11:54:47.102] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:54:47.118] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2216222031 ntp_seconds=4002580487 ntp_fraction=502511173 arrival_ns=11162834017800
[2026-11-02 11:54:47.118] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2216222031
[2026-11-02 11:54:47.118] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:54:48.031] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1553 source_master_ns=41700684332 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1632456
[2026-11-02 11:54:49.075] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1598 source_master_ns=42745582291 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1677802
[2026-11-02 11:54:50.097] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1642 source_master_ns=43767260296 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1723148
[2026-11-02 11:54:51.119] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1686 source_master_ns=44788938300 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1768494
[2026-11-02 11:54:52.086] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1543734501 ntp_seconds=4002580492 ntp_fraction=369367187 arrival_ns=11167803352900
[2026-11-02 11:54:52.087] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1543734501
[2026-11-02 11:54:52.087] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:54:52.103] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3852923137 ntp_seconds=4002580492 ntp_fraction=438086664 arrival_ns=11167819177000
[2026-11-02 11:54:52.103] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3852923137
[2026-11-02 11:54:52.103] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:54:52.103] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2691546262 ntp_seconds=4002580492 ntp_fraction=438086664 arrival_ns=11167819262000
[2026-11-02 11:54:52.104] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2691546262
[2026-11-02 11:54:52.104] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:54:52.142] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1730 source_master_ns=45810616305 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1813840
[2026-11-02 11:54:52.163] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2216222031 ntp_seconds=4002580492 ntp_fraction=700079669 arrival_ns=11167879747200
[2026-11-02 11:54:52.163] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2216222031
[2026-11-02 11:54:52.163] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:54:53.187] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1775 source_master_ns=46855514264 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1859186
[2026-11-02 11:54:54.210] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1819 source_master_ns=47877192268 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1904532
[2026-11-02 11:54:55.228] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1863 source_master_ns=48898870273 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1949878
[2026-11-02 11:54:56.273] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1908 source_master_ns=49943768232 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1995224
[2026-11-02 11:54:57.092] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1543734501 ntp_seconds=4002580497 ntp_fraction=395136991 arrival_ns=11172809376100
[2026-11-02 11:54:57.093] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1543734501
[2026-11-02 11:54:57.093] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:54:57.108] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2691546262 ntp_seconds=4002580497 ntp_fraction=463856467 arrival_ns=11172825047000
[2026-11-02 11:54:57.108] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2691546262
[2026-11-02 11:54:57.108] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:54:57.108] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3852923137 ntp_seconds=4002580497 ntp_fraction=463856467 arrival_ns=11172825291700
[2026-11-02 11:54:57.109] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3852923137
[2026-11-02 11:54:57.109] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:54:57.183] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2216222031 ntp_seconds=4002580497 ntp_fraction=785979015 arrival_ns=11172900256700
[2026-11-02 11:54:57.184] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2216222031
[2026-11-02 11:54:57.184] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:54:57.295] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1952 source_master_ns=50965446237 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2040570
[2026-11-02 11:54:58.317] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1996 source_master_ns=51987124241 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2085916
[2026-11-02 11:54:59.340] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2040 source_master_ns=53008802246 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2131262
[2026-11-02 11:55:00.383] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2085 source_master_ns=54053700205 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2176608
[2026-11-02 11:55:01.405] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2129 source_master_ns=55075378209 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2221954
[2026-11-02 11:55:02.114] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3852923137 ntp_seconds=4002580502 ntp_fraction=489626271 arrival_ns=11177831243500
[2026-11-02 11:55:02.115] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3852923137
[2026-11-02 11:55:02.115] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:02.115] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2691546262 ntp_seconds=4002580502 ntp_fraction=489626271 arrival_ns=11177831466300
[2026-11-02 11:55:02.115] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2691546262
[2026-11-02 11:55:02.116] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:02.114] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1543734501 ntp_seconds=4002580502 ntp_fraction=489626271 arrival_ns=11177831191000
[2026-11-02 11:55:02.116] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1543734501
[2026-11-02 11:55:02.116] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:02.203] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2216222031 ntp_seconds=4002580502 ntp_fraction=867583393 arrival_ns=11177919582600
[2026-11-02 11:55:02.203] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2216222031
[2026-11-02 11:55:02.203] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:02.426] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2173 source_master_ns=56097056214 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2267300
[2026-11-02 11:55:03.474] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2218 source_master_ns=57141954173 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2312646
[2026-11-02 11:55:04.493] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2262 source_master_ns=58163632178 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2357992
[2026-11-02 11:55:05.514] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2306 source_master_ns=59185310182 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2403338
[2026-11-02 11:55:06.538] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2350 source_master_ns=60206988187 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2448684
[2026-11-02 11:55:07.124] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1543734501 ntp_seconds=4002580507 ntp_fraction=532575944 arrival_ns=11182840772500
[2026-11-02 11:55:07.124] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1543734501
[2026-11-02 11:55:07.124] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:07.141] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3852923137 ntp_seconds=4002580507 ntp_fraction=605590388 arrival_ns=11182857871300
[2026-11-02 11:55:07.141] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3852923137
[2026-11-02 11:55:07.141] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:07.141] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2691546262 ntp_seconds=4002580507 ntp_fraction=605590388 arrival_ns=11182857885500
[2026-11-02 11:55:07.141] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2691546262
[2026-11-02 11:55:07.142] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:07.216] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2216222031 ntp_seconds=4002580507 ntp_fraction=923417968 arrival_ns=11182932525700
[2026-11-02 11:55:07.217] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2216222031
[2026-11-02 11:55:07.217] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:07.582] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2395 source_master_ns=61251886146 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2494030
[2026-11-02 11:55:08.603] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2439 source_master_ns=62273564150 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2539376
[2026-11-02 11:55:09.625] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2483 source_master_ns=63295242155 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2584722
[2026-11-02 11:55:10.671] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2528 source_master_ns=64340140114 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2630068
[2026-11-02 11:55:11.692] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2572 source_master_ns=65361818119 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2675414
[2026-11-02 11:55:12.139] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1543734501 ntp_seconds=4002580512 ntp_fraction=597000454 arrival_ns=11187855862800
[2026-11-02 11:55:12.139] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1543734501
[2026-11-02 11:55:12.139] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:12.170] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3852923137 ntp_seconds=4002580512 ntp_fraction=725849473 arrival_ns=11187886053000
[2026-11-02 11:55:12.170] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2691546262 ntp_seconds=4002580512 ntp_fraction=725849473 arrival_ns=11187886021400
[2026-11-02 11:55:12.170] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2691546262
[2026-11-02 11:55:12.170] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3852923137
[2026-11-02 11:55:12.171] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:12.171] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:12.242] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2216222031 ntp_seconds=4002580512 ntp_fraction=1035087118 arrival_ns=11187958363700
[2026-11-02 11:55:12.242] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2216222031
[2026-11-02 11:55:12.242] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:12.713] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2616 source_master_ns=66383496123 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2720760
[2026-11-02 11:55:13.735] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2660 source_master_ns=67405174128 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2766106
[2026-11-02 11:55:14.779] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2705 source_master_ns=68450072087 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2811452
[2026-11-02 11:55:15.801] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2749 source_master_ns=69471750092 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2856798
[2026-11-02 11:55:16.823] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2793 source_master_ns=70493428096 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2902144
[2026-11-02 11:55:17.161] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1543734501 ntp_seconds=4002580517 ntp_fraction=691489734 arrival_ns=11192877869400
[2026-11-02 11:55:17.161] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1543734501
[2026-11-02 11:55:17.161] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:17.174] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2691546262 ntp_seconds=4002580517 ntp_fraction=747324309 arrival_ns=11192891087500
[2026-11-02 11:55:17.174] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2691546262
[2026-11-02 11:55:17.174] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:17.189] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3852923137 ntp_seconds=4002580517 ntp_fraction=807453851 arrival_ns=11192905330900
[2026-11-02 11:55:17.189] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3852923137
[2026-11-02 11:55:17.189] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:17.248] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2216222031 ntp_seconds=4002580517 ntp_fraction=1060856922 arrival_ns=11192964595200
[2026-11-02 11:55:17.248] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2216222031
[2026-11-02 11:55:17.249] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:17.868] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2838 source_master_ns=71538326055 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2947490
[2026-11-02 11:55:18.890] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2882 source_master_ns=72560004060 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2992836
[2026-11-02 11:55:19.911] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2926 source_master_ns=73581682064 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3038182
[2026-11-02 11:55:20.933] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2970 source_master_ns=74603360069 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3083528
[2026-11-02 11:55:21.978] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3015 source_master_ns=75648258028 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3128874
[2026-11-02 11:55:22.180] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1543734501 ntp_seconds=4002580522 ntp_fraction=768799145 arrival_ns=11197896511100
[2026-11-02 11:55:22.180] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1543734501
[2026-11-02 11:55:22.180] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2691546262 ntp_seconds=4002580522 ntp_fraction=768799145 arrival_ns=11197896390800
[2026-11-02 11:55:22.181] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2691546262
[2026-11-02 11:55:22.181] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:22.182] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:22.219] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3852923137 ntp_seconds=4002580522 ntp_fraction=940597837 arrival_ns=11197935960200
[2026-11-02 11:55:22.220] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3852923137
[2026-11-02 11:55:22.220] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:22.250] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2216222031 ntp_seconds=4002580522 ntp_fraction=1073741824 arrival_ns=11197966870900
[2026-11-02 11:55:22.250] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2216222031
[2026-11-02 11:55:22.251] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:22.999] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3059 source_master_ns=76669936033 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3174220
[2026-11-02 11:55:24.021] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3103 source_master_ns=77691614037 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3219566
[2026-11-02 11:55:25.066] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3148 source_master_ns=78736511996 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3264912
[2026-11-02 11:55:26.087] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3192 source_master_ns=79758190001 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3310258
[2026-11-02 11:55:27.111] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3236 source_master_ns=80779868005 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3355604
[2026-11-02 11:55:27.184] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1543734501 ntp_seconds=4002580527 ntp_fraction=790273982 arrival_ns=11202900928400
[2026-11-02 11:55:27.184] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1543734501
[2026-11-02 11:55:27.184] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:27.215] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2691546262 ntp_seconds=4002580527 ntp_fraction=919123001 arrival_ns=11202931590300
[2026-11-02 11:55:27.215] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2691546262
[2026-11-02 11:55:27.215] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:27.247] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3852923137 ntp_seconds=4002580527 ntp_fraction=1060856922 arrival_ns=11202964069300
[2026-11-02 11:55:27.248] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3852923137
[2026-11-02 11:55:27.248] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:27.275] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2216222031 ntp_seconds=4002580527 ntp_fraction=1176821039 arrival_ns=11202991214100
[2026-11-02 11:55:27.275] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2216222031
[2026-11-02 11:55:27.276] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:28.132] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3280 source_master_ns=81801546010 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3400950
[2026-11-02 11:55:29.176] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3325 source_master_ns=82846443969 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3446296
[2026-11-02 11:55:30.198] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3369 source_master_ns=83868121974 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3491642
[2026-11-02 11:55:31.220] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3413 source_master_ns=84889799978 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3536988
[2026-11-02 11:55:32.205] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1543734501 ntp_seconds=4002580532 ntp_fraction=880468295 arrival_ns=11207921782900
[2026-11-02 11:55:32.205] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1543734501
[2026-11-02 11:55:32.206] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:32.241] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3457 source_master_ns=85911477983 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3582334
[2026-11-02 11:55:32.247] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2691546262 ntp_seconds=4002580532 ntp_fraction=1060856922 arrival_ns=11207964246000
[2026-11-02 11:55:32.248] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2691546262
[2026-11-02 11:55:32.248] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:32.277] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3852923137 ntp_seconds=4002580532 ntp_fraction=1189705940 arrival_ns=11207994063600
[2026-11-02 11:55:32.278] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2216222031 ntp_seconds=4002580532 ntp_fraction=1189705940 arrival_ns=11207994297100
[2026-11-02 11:55:32.278] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2216222031
[2026-11-02 11:55:32.278] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:32.278] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3852923137
[2026-11-02 11:55:32.279] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:33.286] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3502 source_master_ns=86956375942 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3627680
[2026-11-02 11:55:34.308] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3546 source_master_ns=87978053946 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3673026
[2026-11-02 11:55:35.329] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3590 source_master_ns=88999731951 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3718372
[2026-11-02 11:55:36.375] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3635 source_master_ns=90044629910 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3763718
[2026-11-02 11:55:37.214] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1543734501 ntp_seconds=4002580537 ntp_fraction=919123001 arrival_ns=11212931038200
[2026-11-02 11:55:37.215] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1543734501
[2026-11-02 11:55:37.216] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:37.276] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2691546262 ntp_seconds=4002580537 ntp_fraction=1181116006 arrival_ns=11212992315400
[2026-11-02 11:55:37.276] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2691546262
[2026-11-02 11:55:37.276] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:37.320] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3852923137 ntp_seconds=4002580537 ntp_fraction=1370094567 arrival_ns=11213035891700
[2026-11-02 11:55:37.321] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3852923137
[2026-11-02 11:55:37.321] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:37.320] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2216222031 ntp_seconds=4002580537 ntp_fraction=1370094567 arrival_ns=11213036594100
[2026-11-02 11:55:37.322] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2216222031
[2026-11-02 11:55:37.322] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:37.396] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3679 source_master_ns=91066307915 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3809064
[2026-11-02 11:55:38.418] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3723 source_master_ns=92087985919 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3854410
[2026-11-02 11:55:39.439] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3767 source_master_ns=93109663924 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3899756
[2026-11-02 11:55:40.484] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3812 source_master_ns=94154561883 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3945102
[2026-11-02 11:55:41.506] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3856 source_master_ns=95176239887 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3990448
[2026-11-02 11:55:42.217] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1543734501 ntp_seconds=4002580542 ntp_fraction=927712935 arrival_ns=11217933584400
[2026-11-02 11:55:42.217] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1543734501
[2026-11-02 11:55:42.217] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:42.278] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2691546262 ntp_seconds=4002580542 ntp_fraction=1194000908 arrival_ns=11217994806000
[2026-11-02 11:55:42.278] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2691546262
[2026-11-02 11:55:42.279] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:42.336] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2216222031 ntp_seconds=4002580542 ntp_fraction=1443109011 arrival_ns=11218053175300
[2026-11-02 11:55:42.336] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3852923137 ntp_seconds=4002580542 ntp_fraction=1443109011 arrival_ns=11218053218200
[2026-11-02 11:55:42.337] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2216222031
[2026-11-02 11:55:42.337] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3852923137
[2026-11-02 11:55:42.337] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:42.337] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:42.528] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3900 source_master_ns=96197917892 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4035794
[2026-11-02 11:55:43.572] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3945 source_master_ns=97242815851 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4081140
[2026-11-02 11:55:44.594] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3989 source_master_ns=98264493856 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4126486
[2026-11-02 11:55:45.616] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4033 source_master_ns=99286171860 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4171832
[2026-11-02 11:55:46.637] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4077 source_master_ns=100307849865 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4217178
[2026-11-02 11:55:47.239] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1543734501 ntp_seconds=4002580547 ntp_fraction=1022202216 arrival_ns=11222955333900
[2026-11-02 11:55:47.239] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1543734501
[2026-11-02 11:55:47.239] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:47.283] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2691546262 ntp_seconds=4002580547 ntp_fraction=1211180777 arrival_ns=11222999540400
[2026-11-02 11:55:47.283] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2691546262
[2026-11-02 11:55:47.283] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:47.338] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3852923137 ntp_seconds=4002580547 ntp_fraction=1451698946 arrival_ns=11223054708500
[2026-11-02 11:55:47.338] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3852923137
[2026-11-02 11:55:47.340] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:47.339] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2216222031 ntp_seconds=4002580547 ntp_fraction=1455993913 arrival_ns=11223056008400
[2026-11-02 11:55:47.340] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2216222031
[2026-11-02 11:55:47.340] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:47.683] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4122 source_master_ns=101352747824 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4262524
[2026-11-02 11:55:48.704] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4166 source_master_ns=102374425828 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4307870
[2026-11-02 11:55:49.726] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4210 source_master_ns=103396103833 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4353216
[2026-11-02 11:55:50.770] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4255 source_master_ns=104441001792 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4398562
[2026-11-02 11:55:51.792] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4299 source_master_ns=105462679797 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4443908
[2026-11-02 11:55:52.248] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1543734501 ntp_seconds=4002580552 ntp_fraction=1065151889 arrival_ns=11227964749900
[2026-11-02 11:55:52.248] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1543734501
[2026-11-02 11:55:52.248] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:52.319] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2691546262 ntp_seconds=4002580552 ntp_fraction=1365799600 arrival_ns=11228035489600
[2026-11-02 11:55:52.319] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2691546262
[2026-11-02 11:55:52.319] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:52.348] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2216222031 ntp_seconds=4002580552 ntp_fraction=1490353651 arrival_ns=11228064073100
[2026-11-02 11:55:52.348] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2216222031
[2026-11-02 11:55:52.348] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:52.349] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3852923137 ntp_seconds=4002580552 ntp_fraction=1494648619 arrival_ns=11228065205600
[2026-11-02 11:55:52.349] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3852923137
[2026-11-02 11:55:52.349] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:52.815] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4343 source_master_ns=106484357801 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4489254
[2026-11-02 11:55:53.302] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=106321105800 present_generation=1 present_canonical_target_ns=100774603174 present_canonical_dispatch_ns=100774603174 present_target_master_ns=106440289242 present_dispatch_master_ns=106440289242
[2026-11-02 11:55:53.471] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=106490579800 present_generation=1 present_canonical_target_ns=100966666666 present_canonical_dispatch_ns=100966666666 present_target_master_ns=106632352734 present_dispatch_master_ns=106632352734
[2026-11-02 11:55:53.835] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4387 source_master_ns=107506035806 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4534600
[2026-11-02 11:55:54.881] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4432 source_master_ns=108550933765 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4579946
[2026-11-02 11:55:55.902] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4476 source_master_ns=109572611770 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4625292
[2026-11-02 11:55:56.924] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4520 source_master_ns=110594289774 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4670638
[2026-11-02 11:55:57.261] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1543734501 ntp_seconds=4002580557 ntp_fraction=1116691496 arrival_ns=11232977462200
[2026-11-02 11:55:57.261] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1543734501
[2026-11-02 11:55:57.262] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:57.349] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3852923137 ntp_seconds=4002580557 ntp_fraction=1498943586 arrival_ns=11233066219900
[2026-11-02 11:55:57.350] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3852923137
[2026-11-02 11:55:57.349] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2216222031 ntp_seconds=4002580557 ntp_fraction=1498943586 arrival_ns=11233066145000
[2026-11-02 11:55:57.350] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2691546262 ntp_seconds=4002580557 ntp_fraction=1498943586 arrival_ns=11233066444000
[2026-11-02 11:55:57.351] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2691546262
[2026-11-02 11:55:57.351] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2216222031
[2026-11-02 11:55:57.351] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:57.351] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:57.350] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:55:57.970] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4565 source_master_ns=111639187733 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4715984
[2026-11-02 11:55:58.991] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4609 source_master_ns=112660865738 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4761330
[2026-11-02 11:56:00.012] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4653 source_master_ns=113682543742 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4806676
[2026-11-02 11:56:01.034] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4697 source_master_ns=114704221747 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4852022
[2026-11-02 11:56:02.078] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4742 source_master_ns=115749119706 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4897368
[2026-11-02 11:56:02.268] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1543734501 ntp_seconds=4002580562 ntp_fraction=1151051235 arrival_ns=11237985184900
[2026-11-02 11:56:02.269] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1543734501
[2026-11-02 11:56:02.269] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:56:02.382] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3852923137 ntp_seconds=4002580562 ntp_fraction=1636382539 arrival_ns=11238097880600
[2026-11-02 11:56:02.382] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3852923137
[2026-11-02 11:56:02.383] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:56:02.383] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2216222031 ntp_seconds=4002580562 ntp_fraction=1644972474 arrival_ns=11238100055400
[2026-11-02 11:56:02.383] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2216222031
[2026-11-02 11:56:02.383] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:56:02.383] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2691546262 ntp_seconds=4002580562 ntp_fraction=1644972474 arrival_ns=11238100155100
[2026-11-02 11:56:02.384] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2691546262
[2026-11-02 11:56:02.384] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:56:03.101] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4786 source_master_ns=116770797711 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4942714
[2026-11-02 11:56:04.122] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4830 source_master_ns=117792475715 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4988060
[2026-11-02 11:56:05.167] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4875 source_master_ns=118837373674 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5033406
[2026-11-02 11:56:06.189] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4919 source_master_ns=119859051679 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5078752
[2026-11-02 11:56:06.450] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=11242166975300
[2026-11-02 11:56:06.450] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=3 generation=2 invalidated_generation=1 locked=0
[2026-11-02 11:56:06.451] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=30 group=composition-r380.source.1 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=23 video_raw_generation=0 video_ssrc=3852923137
[2026-11-02 11:56:06.451] [info] [graph][runtime.node] av_reacquisition group=composition-r380.source.1 phase=purging old=1 next=2 transition=1
[2026-11-02 11:56:06.451] [info] [graph][runtime.node] rtp_ingress_purge node=27 transition=1 cutoff=0
[2026-11-02 11:56:06.451] [info] [graph][runtime.node] rtp_binder_purge node=30 transition=1 cutoff=3608
[2026-11-02 11:56:06.454] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=11242170955500
[2026-11-02 11:56:06.454] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=3 generation=2 invalidated_generation=1 locked=0
[2026-11-02 11:56:06.454] [info] [graph][runtime.node] av_reacquisition group=composition-r380.source.0 phase=purging old=1 next=2 transition=1
[2026-11-02 11:56:06.454] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=11 group=composition-r380.source.0 buffered=0 phase=1 preserves_output=1 old_generation=1 next_generation=2 transition_sequence=1 published_generation=1 protocol_generation=1 evidence_revision=23 video_raw_generation=0 video_ssrc=2691546262
[2026-11-02 11:56:06.455] [info] [graph][runtime.node] rtp_binder_purge node=11 transition=1 cutoff=3608
[2026-11-02 11:56:06.454] [info] [graph][runtime.node] rtp_ingress_purge node=8 transition=1 cutoff=0
[2026-11-02 11:56:06.457] [info] [graph][runtime.node] av_reacquisition group=composition-r380.source.1 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=1 next=2 transition=1
[2026-11-02 11:56:06.457] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=11242174319300
[2026-11-02 11:56:06.458] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=3 generation=2 invalidated_generation=1 locked=0
[2026-11-02 11:56:06.458] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=50 group=composition-r380.source.2 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=46 video_raw_generation=0 video_ssrc=2216222031 audio_raw_generation=0 audio_ssrc=1543734501
[2026-11-02 11:56:06.458] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=51 group=composition-r380.source.2 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=46 video_raw_generation=0 video_ssrc=2216222031 audio_raw_generation=0 audio_ssrc=1543734501
[2026-11-02 11:56:06.459] [info] [graph][runtime.node] rtp_binder_purge node=51 transition=1 cutoff=5170
[2026-11-02 11:56:06.459] [info] [graph][runtime.node] rtp_ingress_purge node=46 transition=1 cutoff=0
[2026-11-02 11:56:06.459] [info] [graph][runtime.node] rtp_ingress_purge node=47 transition=1 cutoff=0
[2026-11-02 11:56:06.459] [info] [graph][runtime.node] rtp_binder_purge node=50 transition=1 cutoff=3608
[2026-11-02 11:56:06.458] [info] [graph][runtime.node] av_reacquisition group=composition-r380.source.2 phase=purging old=1 next=2 transition=1
[2026-11-02 11:56:06.460] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=11242175612200
[2026-11-02 11:56:06.461] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=3 generation=2 invalidated_generation=1 locked=0
[2026-11-02 11:56:06.461] [info] [graph][runtime.node] av_reacquisition group=composition-r380.source.2 event=purge_completed purge_ack=complete continuation=acquiring ack_count=2 old=1 next=2 transition=1
[2026-11-02 11:56:06.462] [info] [graph][runtime.node] av_reacquisition group=composition-r380.source.0 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=1 next=2 transition=1
[2026-11-02 11:56:18.449] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=131468088300 present_generation=1 present_canonical_target_ns=125921814058 present_canonical_dispatch_ns=125921814058 present_target_master_ns=131587500126 present_dispatch_master_ns=131587500126
[2026-11-02 11:56:18.452] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=131471240800 present_generation=1 present_canonical_target_ns=125933333333 present_canonical_dispatch_ns=125933333333 present_target_master_ns=131599019401 present_dispatch_master_ns=131599019401
[2026-11-02 11:56:57.447] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=4118520917 ntp_seconds=4002580617 ntp_fraction=1915555414 arrival_ns=11293163718600
[2026-11-02 11:56:57.447] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=4118520917
[2026-11-02 11:56:57.447] [info] [graph][runtime.node] rtp_binder_boundary stage=deadline_established node=51 group=composition-r380.source.2 buffered=0 phase=2 preserves_output=1 raw_ssrc=4118520917 raw_generation=1 raw_sequence=5172 old_generation=1 next_generation=2 transition_sequence=1 published_generation=1
[2026-11-02 11:56:57.448] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=0 generation=2 invalidated_generation=1 locked=0
[2026-11-02 11:56:57.448] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2459306278 ntp_seconds=4002580617 ntp_fraction=1915555414 arrival_ns=11293163759000
[2026-11-02 11:56:57.448] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2459306278
[2026-11-02 11:56:57.448] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:56:57.983] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=1 generation=2
[2026-11-02 11:56:58.489] [info] [graph][runtime.node] source_video_trace_summary node_id=61 node=ActivationReleaseSequencer generation=1 reason=generation_change steady_ns=11294205825200 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3513 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:56:58.489] [info] [graph][runtime.node] source_video_trace node_id=61 node=ActivationReleaseSequencer stage=activation source_identity=composition-r380.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=996000000 duration_ns=33333333 decoded_sequence=unavailable pts=3161430346 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11294206019900 group=composition-r380.source.2 release_identity=15727 transition_sequence=1 epoch_source_start_ns=996000000 epoch_master_release_ns=171607729500 video_count=16 min_presentation_ns=996000000 max_end_ns=1529333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:56:58.489] [info] [graph][runtime.node] source_video_trace node_id=61 node=ActivationReleaseSequencer stage=release_published source_identity=composition-r380.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=996000000 duration_ns=33333333 decoded_sequence=unavailable pts=3161430346 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11294206138700 group=composition-r380.source.2 release_identity=15727 transition_sequence=1 epoch_source_start_ns=996000000 epoch_master_release_ns=171607729500 video_count=16 min_presentation_ns=996000000 max_end_ns=1529333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:56:58.489] [info] [graph][runtime.node] source_video_trace_summary node_id=62 node=MediaAvBoundReleaseExtractorNode generation=1 reason=generation_change steady_ns=11294206166300 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=57 release_count=3571 release_committed_count=3513 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:56:58.489] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=release source_identity=composition-r380.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=996000000 duration_ns=33333333 decoded_sequence=unavailable pts=3161430346 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11294206345500 group=composition-r380.source.2 release_identity=15727 transition_sequence=1 epoch_source_start_ns=996000000 epoch_master_release_ns=171607729500 video_count=16 min_presentation_ns=996000000 max_end_ns=1529333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:56:58.489] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=release_committed source_identity=composition-r380.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=996000000 duration_ns=33333333 decoded_sequence=unavailable pts=3161430346 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11294206464500 group=composition-r380.source.2 release_identity=15727 transition_sequence=1 epoch_source_start_ns=996000000 epoch_master_release_ns=171607729500 video_count=16 min_presentation_ns=996000000 max_end_ns=1529333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=1 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:56:58.489] [info] [graph][runtime.node] source_video_trace_summary node_id=64 node=VideoDecodeNode generation=1 reason=generation_change steady_ns=11294206483100 input_count=3461 accepted_count=3461 would_block_count=0 resolved_count=3461 output_attempt_count=0 published_count=3460 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=108 incomplete=0
[2026-11-02 11:56:58.490] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1 source_master_ns=171608192085 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-11-02 11:56:58.490] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=input source_identity=composition-r380.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=996000000 duration_ns=33333333 decoded_sequence=unavailable pts=3161430346 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11294206645400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=108 incomplete=0
[2026-11-02 11:56:58.490] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3 source_master_ns=171654631994 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=45346
[2026-11-02 11:56:58.496] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=accepted source_identity=composition-r380.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=996000000 duration_ns=33333333 decoded_sequence=unavailable pts=3161430346 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=11294212645100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=108 incomplete=0
[2026-11-02 11:56:58.496] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=resolved source_identity=composition-r380.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=996000000 duration_ns=33333333 decoded_sequence=unavailable pts=3161430346 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11294212814300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=108 incomplete=0
[2026-11-02 11:56:58.496] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=published source_identity=composition-r380.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=996000000 duration_ns=33333333 decoded_sequence=unavailable pts=3161430346 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11294212922800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=108 incomplete=0
[2026-11-02 11:56:58.496] [info] [graph][runtime.node] source_video_trace_summary node_id=65 node=HardwareTransferNode generation=1 reason=generation_change steady_ns=11294212954800 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3460 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:56:58.496] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-11-02 11:56:58.496] [info] [graph][runtime.node] source_video_trace node_id=65 node=HardwareTransferNode stage=published source_identity=composition-r380.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=996000000 duration_ns=33333333 decoded_sequence=unavailable pts=3161430346 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11294213199300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:56:58.496] [info] [graph][runtime.node] source_video_trace_summary node_id=66 node=VideoFrameRateNode generation=1 reason=generation_change steady_ns=11294213358700 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3446 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:56:58.496] [info] [graph][runtime.node] source_video_trace_summary node_id=68 node=VideoFilterNode generation=1 reason=generation_change steady_ns=11294213396500 input_count=3440 accepted_count=3440 would_block_count=0 resolved_count=3440 output_attempt_count=0 published_count=3439 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 11:56:58.496] [info] [graph][runtime.node] source_video_trace node_id=66 node=VideoFrameRateNode stage=published source_identity=composition-r380.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=996000000 duration_ns=33333333 decoded_sequence=unavailable pts=3161430346 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11294213497200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:56:58.496] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=input source_identity=composition-r380.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=996000000 duration_ns=33333333 decoded_sequence=unavailable pts=3161430346 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11294213566900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 11:56:58.497] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=accepted source_identity=composition-r380.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=996000000 duration_ns=33333333 decoded_sequence=unavailable pts=3161430346 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=11294213717700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 11:56:58.497] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=resolved source_identity=composition-r380.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=996000000 duration_ns=33333333 decoded_sequence=unavailable pts=1053810 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=11294213945600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 11:56:58.497] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=published source_identity=composition-r380.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=996000000 duration_ns=33333333 decoded_sequence=unavailable pts=1053810 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=11294214041300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 11:56:58.497] [info] [graph][runtime.node] source_video_trace_summary node_id=67 node=VideoPreparationNode generation=1 reason=generation_change steady_ns=11294214060800 input_count=3433 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3433 published_count=3432 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 11:56:58.497] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=input source_identity=composition-r380.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=996000000 duration_ns=33333333 decoded_sequence=unavailable pts=1053810 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=11294214248900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 11:56:58.497] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=output_attempt source_identity=composition-r380.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=996000000 duration_ns=33333333 decoded_sequence=unavailable pts=1053810 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=11294214327900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 11:56:58.497] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=published source_identity=composition-r380.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=996000000 duration_ns=33333333 decoded_sequence=unavailable pts=1053810 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=11294214404800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 11:56:58.501] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_candidate source_identity=composition-r380.source.2.input.video generation=2 steady_ns=11294218289400 source_sequence=3 presentation_ns=1062666666 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 11:56:58.517] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-r380.source.2.input.video generation=2 steady_ns=11294234331100 source_sequence=3 presentation_ns=1062666666 duration_ns=33333333 decoded_sequence=unavailable
[CLI] [2026-11-02 11:56:59.037] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=48 source_master_ns=172699529953 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=90692
runtime report: queued=19, peakQueued=916, threads=81, processThreads=129, workers=75, workerProgress=355150, workerProcessCalls=1314365, workerWaits=959207, workerWakeups=946505, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=528, logicalProcessors=22, averageSystemMachineCpuPercent=7.271676, averageProcessMachineCpuPercent=1.660824, peakProcessMachineCpuPercent=5.567929, averageProcessSingleCoreCpuPercent=36.538122, peakProcessSingleCoreCpuPercent=122.494432, initialWorkingSetBytes=209342464, workingSetBytes=253173760, peakWorkingSetBytes=271364096, totalPushed=303495, totalPopped=303452, droppedBuffers=0, encodedPacketsPushed=55837, encodedPacketsPopped=55836, backpressureItems=123, graphPayloadCurrentBytes=257915, graphPayloadHighWaterBytes=15849484, graphPayloadCurrentObjects=53, graphPayloadHighWaterObjects=742, graphPayloadReservations=119486, graphPayloadReleases=119433, graphPayloadPressureFailures=0, lastInputReceivedAtNs=11294736317400
[2026-11-02 11:57:00.059] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=92 source_master_ns=173721207958 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=136038
[2026-11-02 11:57:01.081] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=136 source_master_ns=174742885962 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=181384
[2026-11-02 11:57:02.135] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=181 source_master_ns=175787783921 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=226730
[2026-11-02 11:57:02.453] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2459306278 ntp_seconds=4002580622 ntp_fraction=1945620185 arrival_ns=11298169765800
[2026-11-02 11:57:02.454] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2459306278
[2026-11-02 11:57:02.454] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:57:02.455] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=4118520917 ntp_seconds=4002580622 ntp_fraction=1949915152 arrival_ns=11298171514100
[2026-11-02 11:57:02.455] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=4118520917
[2026-11-02 11:57:02.455] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:57:03.147] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=225 source_master_ns=176809461926 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=272076
[2026-11-02 11:57:04.169] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=269 source_master_ns=177831139930 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=317422
[2026-11-02 11:57:05.195] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=313 source_master_ns=178852817935 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=362768
[2026-11-02 11:57:06.236] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=358 source_master_ns=179897715894 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=408114
[2026-11-02 11:57:07.257] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=402 source_master_ns=180919393899 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=453460
[2026-11-02 11:57:07.480] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2459306278 ntp_seconds=4002580627 ntp_fraction=2057289334 arrival_ns=11303196487800
[2026-11-02 11:57:07.480] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2459306278
[2026-11-02 11:57:07.480] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:57:07.481] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=4118520917 ntp_seconds=4002580627 ntp_fraction=2057289334 arrival_ns=11303196416800
[2026-11-02 11:57:07.481] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=4118520917
[2026-11-02 11:57:07.481] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:57:08.279] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=446 source_master_ns=181941071903 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=498806
[2026-11-02 11:57:09.324] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=491 source_master_ns=182985969862 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=544152
[2026-11-02 11:57:10.345] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=535 source_master_ns=184007647867 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=589498
[2026-11-02 11:57:11.367] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=579 source_master_ns=185029325871 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=634844
[2026-11-02 11:57:12.390] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=623 source_master_ns=186051003876 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=680190
[2026-11-02 11:57:12.494] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2459306278 ntp_seconds=4002580632 ntp_fraction=2117418876 arrival_ns=11308210054900
[2026-11-02 11:57:12.494] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2459306278
[2026-11-02 11:57:12.494] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:57:12.494] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=4118520917 ntp_seconds=4002580632 ntp_fraction=2121713844 arrival_ns=11308211411700
[2026-11-02 11:57:12.495] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=4118520917
[2026-11-02 11:57:12.495] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:57:13.434] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=668 source_master_ns=187095901835 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=725536
[2026-11-02 11:57:14.455] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=712 source_master_ns=188117579840 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=770882
[2026-11-02 11:57:15.478] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=756 source_master_ns=189139257844 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=816228
[2026-11-02 11:57:16.522] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=801 source_master_ns=190184155803 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=861574
[2026-11-02 11:57:17.497] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=4118520917 ntp_seconds=4002580637 ntp_fraction=2134598746 arrival_ns=11313213836300
[2026-11-02 11:57:17.497] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=4118520917
[2026-11-02 11:57:17.497] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:57:17.513] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2459306278 ntp_seconds=4002580637 ntp_fraction=2203318222 arrival_ns=11313230165500
[2026-11-02 11:57:17.513] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2459306278
[2026-11-02 11:57:17.514] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:57:17.543] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=845 source_master_ns=191205833808 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=906920
[2026-11-02 11:57:18.565] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=889 source_master_ns=192227511812 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=952266
[2026-11-02 11:57:19.587] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=933 source_master_ns=193249189817 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=997612
[2026-11-02 11:57:20.632] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=978 source_master_ns=194294087776 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1042958
[2026-11-02 11:57:21.653] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1022 source_master_ns=195315765781 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1088304
[2026-11-02 11:57:22.500] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=4118520917 ntp_seconds=4002580642 ntp_fraction=2147483648 arrival_ns=11318217365000
[2026-11-02 11:57:22.501] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=4118520917
[2026-11-02 11:57:22.501] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:57:22.517] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2459306278 ntp_seconds=4002580642 ntp_fraction=2216203124 arrival_ns=11318233394400
[2026-11-02 11:57:22.517] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2459306278
[2026-11-02 11:57:22.517] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:57:22.675] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1066 source_master_ns=196337443785 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1133650
[2026-11-02 11:57:23.720] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1111 source_master_ns=197382341744 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1178996
[2026-11-02 11:57:24.745] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1155 source_master_ns=198404019749 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1224342
[2026-11-02 11:57:25.763] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1199 source_master_ns=199425697753 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1269688
[2026-11-02 11:57:26.786] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1243 source_master_ns=200447375758 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1315034
[2026-11-02 11:57:27.515] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=4118520917 ntp_seconds=4002580647 ntp_fraction=2211908157 arrival_ns=11323232413300
[2026-11-02 11:57:27.516] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=4118520917
[2026-11-02 11:57:27.516] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:57:27.562] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2459306278 ntp_seconds=4002580647 ntp_fraction=2409476653 arrival_ns=11323278300200
[2026-11-02 11:57:27.563] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2459306278
[2026-11-02 11:57:27.563] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:57:27.830] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1288 source_master_ns=201492273717 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1360380
[2026-11-02 11:57:28.852] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1332 source_master_ns=202513951722 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1405726
[2026-11-02 11:57:29.873] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1376 source_master_ns=203535629726 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1451072
[2026-11-02 11:57:30.924] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1421 source_master_ns=204580527685 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1496418
[2026-11-02 11:57:31.940] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1465 source_master_ns=205602205690 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1541764
[2026-11-02 11:57:32.524] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=4118520917 ntp_seconds=4002580652 ntp_fraction=2250562863 arrival_ns=11328240795200
[2026-11-02 11:57:32.525] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=4118520917
[2026-11-02 11:57:32.525] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:57:32.568] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2459306278 ntp_seconds=4002580652 ntp_fraction=2439541424 arrival_ns=11328284955200
[2026-11-02 11:57:32.568] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2459306278
[2026-11-02 11:57:32.568] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:57:32.962] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1509 source_master_ns=206623883695 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1587110
[2026-11-02 11:57:33.991] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1553 source_master_ns=207645561699 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1632456
[2026-11-02 11:57:35.029] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1598 source_master_ns=208690459658 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1677802
[2026-11-02 11:57:36.050] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1642 source_master_ns=209712137663 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1723148
[2026-11-02 11:57:37.077] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1686 source_master_ns=210733815667 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1768494
[2026-11-02 11:57:37.536] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=4118520917 ntp_seconds=4002580657 ntp_fraction=2302102470 arrival_ns=11333253224400
[2026-11-02 11:57:37.537] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=4118520917
[2026-11-02 11:57:37.537] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:57:37.583] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2459306278 ntp_seconds=4002580657 ntp_fraction=2499670966 arrival_ns=11333298932900
[2026-11-02 11:57:37.583] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2459306278
[2026-11-02 11:57:37.584] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:57:38.116] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1731 source_master_ns=211778713626 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1813840
[2026-11-02 11:57:39.138] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1775 source_master_ns=212800391631 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1859186
[2026-11-02 11:57:40.160] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1819 source_master_ns=213822069636 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1904532
[2026-11-02 11:57:41.184] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1863 source_master_ns=214843747640 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1949878
[2026-11-02 11:57:42.226] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1908 source_master_ns=215888645599 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1995224
[2026-11-02 11:57:42.541] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=4118520917 ntp_seconds=4002580662 ntp_fraction=2323577307 arrival_ns=11338258442800
[2026-11-02 11:57:42.542] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=4118520917
[2026-11-02 11:57:42.542] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:57:42.588] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2459306278 ntp_seconds=4002580662 ntp_fraction=2521145802 arrival_ns=11338304549400
[2026-11-02 11:57:42.588] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2459306278
[2026-11-02 11:57:42.588] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:57:43.248] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1952 source_master_ns=216910323604 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2040570
[2026-11-02 11:57:44.271] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1996 source_master_ns=217932001608 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2085916
[2026-11-02 11:57:45.315] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2041 source_master_ns=218976899568 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2131262
[2026-11-02 11:57:46.336] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2085 source_master_ns=219998577572 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2176608
[2026-11-02 11:57:47.358] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2129 source_master_ns=221020255577 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2221954
[2026-11-02 11:57:47.567] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=4118520917 ntp_seconds=4002580667 ntp_fraction=2435246456 arrival_ns=11343284231300
[2026-11-02 11:57:47.568] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=4118520917
[2026-11-02 11:57:47.568] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:57:47.624] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2459306278 ntp_seconds=4002580667 ntp_fraction=2680059592 arrival_ns=11343341001600
[2026-11-02 11:57:47.624] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2459306278
[2026-11-02 11:57:47.624] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:57:48.380] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2173 source_master_ns=222041933581 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2267300
[2026-11-02 11:57:49.425] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2218 source_master_ns=223086831540 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2312646
[2026-11-02 11:57:50.447] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2262 source_master_ns=224108509545 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2357992
[2026-11-02 11:57:51.468] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2306 source_master_ns=225130187549 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2403338
[2026-11-02 11:57:52.490] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2350 source_master_ns=226151865554 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2448684
[2026-11-02 11:57:52.571] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=4118520917 ntp_seconds=4002580672 ntp_fraction=2452426326 arrival_ns=11348288098400
[2026-11-02 11:57:52.572] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=4118520917
[2026-11-02 11:57:52.572] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:57:52.631] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2459306278 ntp_seconds=4002580672 ntp_fraction=2710124363 arrival_ns=11348348060600
[2026-11-02 11:57:52.632] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2459306278
[2026-11-02 11:57:52.632] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:57:53.534] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2395 source_master_ns=227196763513 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2494030

[2026-11-02 11:57:55.579] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2483 source_master_ns=229240119522 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2584722
[2026-11-02 11:57:56.623] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2528 source_master_ns=230285017481 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2630068
[2026-11-02 11:57:57.581] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=4118520917 ntp_seconds=4002580677 ntp_fraction=2495375998 arrival_ns=11353297913400
[2026-11-02 11:57:57.582] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=4118520917
[2026-11-02 11:57:57.582] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:57:57.644] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2572 source_master_ns=231306695486 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2675414
[2026-11-02 11:57:57.660] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2459306278 ntp_seconds=4002580677 ntp_fraction=2834678415 arrival_ns=11353376811600
[2026-11-02 11:57:57.660] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2459306278
[2026-11-02 11:57:57.660] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:57:58.666] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2616 source_master_ns=232328373490 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2720760
[2026-11-02 11:57:59.688] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2660 source_master_ns=233350051495 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2766106
[2026-11-02 11:58:00.732] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2705 source_master_ns=234394949454 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2811452
[2026-11-02 11:58:01.754] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2749 source_master_ns=235416627459 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2856798
[2026-11-02 11:58:02.616] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=4118520917 ntp_seconds=4002580682 ntp_fraction=2645699854 arrival_ns=11358333031800
[2026-11-02 11:58:02.617] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=4118520917
[2026-11-02 11:58:02.617] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:58:02.661] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2459306278 ntp_seconds=4002580682 ntp_fraction=2838973382 arrival_ns=11358378049800
[2026-11-02 11:58:02.661] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2459306278
[2026-11-02 11:58:02.662] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:58:02.776] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2793 source_master_ns=236438305463 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2902144
[2026-11-02 11:58:03.821] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2838 source_master_ns=237483203422 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2947490
[2026-11-02 11:58:04.843] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2882 source_master_ns=238504881427 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2992836
[2026-11-02 11:58:05.864] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2926 source_master_ns=239526559431 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3038182
[2026-11-02 11:58:06.886] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2970 source_master_ns=240548237436 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3083528
[2026-11-02 11:58:07.638] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=4118520917 ntp_seconds=4002580687 ntp_fraction=2740189134 arrival_ns=11363355506500
[2026-11-02 11:58:07.639] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=4118520917
[2026-11-02 11:58:07.639] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:58:07.683] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2459306278 ntp_seconds=4002580687 ntp_fraction=2933462663 arrival_ns=11363399805100
[2026-11-02 11:58:07.683] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2459306278
[2026-11-02 11:58:07.683] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:58:07.932] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3015 source_master_ns=241593135395 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3128874
[2026-11-02 11:58:08.953] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3059 source_master_ns=242614813400 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3174220
[2026-11-02 11:58:09.974] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3103 source_master_ns=243636491404 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3219566
[2026-11-02 11:58:11.019] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3148 source_master_ns=244681389363 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3264912
[2026-11-02 11:58:12.040] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3192 source_master_ns=245703067368 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3310258
[2026-11-02 11:58:12.650] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=4118520917 ntp_seconds=4002580692 ntp_fraction=2791728742 arrival_ns=11368366713000
[2026-11-02 11:58:12.650] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=4118520917
[2026-11-02 11:58:12.650] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:58:12.709] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2459306278 ntp_seconds=4002580692 ntp_fraction=3032246910 arrival_ns=11368423660900
[2026-11-02 11:58:12.709] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2459306278
[2026-11-02 11:58:12.709] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:58:13.064] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3236 source_master_ns=246724745373 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3355604
[2026-11-02 11:58:14.084] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3280 source_master_ns=247746423377 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3400950
[2026-11-02 11:58:15.129] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3325 source_master_ns=248791321336 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3446296
[2026-11-02 11:58:16.150] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3369 source_master_ns=249812999341 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3491642
[2026-11-02 11:58:17.172] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3413 source_master_ns=250834677345 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3536988
[2026-11-02 11:58:17.668] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=4118520917 ntp_seconds=4002580697 ntp_fraction=2869038153 arrival_ns=11373385067200
[2026-11-02 11:58:17.668] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=4118520917
[2026-11-02 11:58:17.669] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:58:17.726] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2459306278 ntp_seconds=4002580697 ntp_fraction=3118146256 arrival_ns=11373443082800
[2026-11-02 11:58:17.726] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2459306278
[2026-11-02 11:58:17.727] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:58:18.217] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3458 source_master_ns=251879575304 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3582334
[2026-11-02 11:58:19.239] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3502 source_master_ns=252901253309 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3627680
[2026-11-02 11:58:20.261] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3546 source_master_ns=253922931314 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3673026
[2026-11-02 11:58:21.283] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3590 source_master_ns=254944609318 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3718372
[2026-11-02 11:58:22.327] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3635 source_master_ns=255989507277 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3763718
[2026-11-02 11:58:22.699] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=4118520917 ntp_seconds=4002580702 ntp_fraction=3002182139 arrival_ns=11378416072200
[2026-11-02 11:58:22.700] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=4118520917
[2026-11-02 11:58:22.700] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:58:22.700] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=255719290300 present_generation=1 present_canonical_target_ns=250171791383 present_canonical_dispatch_ns=250171791383 present_target_master_ns=255837477451 present_dispatch_master_ns=255837477451
[2026-11-02 11:58:22.702] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=255721613300 present_generation=1 present_canonical_target_ns=250200000000 present_canonical_dispatch_ns=250200000000 present_target_master_ns=255865686068 present_dispatch_master_ns=255865686068
[2026-11-02 11:58:22.761] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2459306278 ntp_seconds=4002580702 ntp_fraction=3268470112 arrival_ns=11378478151400
[2026-11-02 11:58:22.761] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2459306278
[2026-11-02 11:58:22.762] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:58:23.349] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3679 source_master_ns=257011185282 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3809064
[2026-11-02 11:58:24.372] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3723 source_master_ns=258032863286 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3854410
[2026-11-02 11:58:25.416] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3768 source_master_ns=259077761246 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3899756
[2026-11-02 11:58:26.437] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3812 source_master_ns=260099439250 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3945102
[2026-11-02 11:58:27.458] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3856 source_master_ns=261121117255 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3990448
[2026-11-02 11:58:27.701] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=4118520917 ntp_seconds=4002580707 ntp_fraction=3010772074 arrival_ns=11383417644500
[2026-11-02 11:58:27.701] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=4118520917
[2026-11-02 11:58:27.701] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:58:27.791] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2459306278 ntp_seconds=4002580707 ntp_fraction=3393024163 arrival_ns=11383506997600
[2026-11-02 11:58:27.791] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2459306278
[2026-11-02 11:58:27.791] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:58:28.482] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3900 source_master_ns=262142795259 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4035794
[2026-11-02 11:58:29.526] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3945 source_master_ns=263187693218 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4081140
[2026-11-02 11:58:30.547] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3989 source_master_ns=264209371223 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4126486
[2026-11-02 11:58:31.569] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4033 source_master_ns=265231049227 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4171832
[2026-11-02 11:58:32.619] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4078 source_master_ns=266275947187 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4217178
[2026-11-02 11:58:32.709] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=4118520917 ntp_seconds=4002580712 ntp_fraction=3040836845 arrival_ns=11388425506800
[2026-11-02 11:58:32.709] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=4118520917
[2026-11-02 11:58:32.709] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:58:32.827] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2459306278 ntp_seconds=4002580712 ntp_fraction=3547642986 arrival_ns=11388543561100
[2026-11-02 11:58:32.827] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2459306278
[2026-11-02 11:58:32.828] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:58:33.635] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4122 source_master_ns=267297625191 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4262524
[2026-11-02 11:58:34.658] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4166 source_master_ns=268319303196 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4307870
[2026-11-02 11:58:35.679] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4210 source_master_ns=269340981200 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4353216
[2026-11-02 11:58:36.724] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4255 source_master_ns=270385879159 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4398562
[2026-11-02 11:58:37.724] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=4118520917 ntp_seconds=4002580717 ntp_fraction=3105261355 arrival_ns=11393440519400
[2026-11-02 11:58:37.724] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=4118520917
[2026-11-02 11:58:37.724] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:58:37.746] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4299 source_master_ns=271407557164 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4443908
[2026-11-02 11:58:37.855] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2459306278 ntp_seconds=4002580717 ntp_fraction=3672197038 arrival_ns=11393572196200
[2026-11-02 11:58:37.856] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2459306278
[2026-11-02 11:58:37.856] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:58:38.768] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4343 source_master_ns=272429235168 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4489254
[2026-11-02 11:58:39.814] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4388 source_master_ns=273474133128 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4534600
[2026-11-02 11:58:40.833] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4432 source_master_ns=274495811132 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4579946
[2026-11-02 11:58:41.855] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4476 source_master_ns=275517489137 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4625292
[2026-11-02 11:58:42.726] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=4118520917 ntp_seconds=4002580722 ntp_fraction=3118146256 arrival_ns=11398442816000
[2026-11-02 11:58:42.726] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=4118520917
[2026-11-02 11:58:42.726] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:58:42.877] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4520 source_master_ns=276539167141 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4670638
[2026-11-02 11:58:42.884] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2459306278 ntp_seconds=4002580722 ntp_fraction=3792456122 arrival_ns=11398600653600
[2026-11-02 11:58:42.884] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2459306278
[2026-11-02 11:58:42.884] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:58:43.922] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4565 source_master_ns=277584065100 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4715984
[2026-11-02 11:58:44.943] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4609 source_master_ns=278605743105 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4761330
[2026-11-02 11:58:45.965] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4653 source_master_ns=279627421109 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4806676
[2026-11-02 11:58:46.987] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4697 source_master_ns=280649099114 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4852022
[2026-11-02 11:58:47.732] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=4118520917 ntp_seconds=4002580727 ntp_fraction=3143916060 arrival_ns=11403449038400
[2026-11-02 11:58:47.733] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=4118520917
[2026-11-02 11:58:47.734] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:58:47.892] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2459306278 ntp_seconds=4002580727 ntp_fraction=3826815860 arrival_ns=11403608110100
[2026-11-02 11:58:47.892] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2459306278
[2026-11-02 11:58:47.892] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:58:48.031] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4742 source_master_ns=281693997073 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4897368
[2026-11-02 11:58:49.054] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4786 source_master_ns=282715675078 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4942714
[2026-11-02 11:58:50.075] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4830 source_master_ns=283737353082 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4988060
[2026-11-02 11:58:51.120] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4875 source_master_ns=284782251041 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5033406
[2026-11-02 11:58:52.142] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4919 source_master_ns=285803929046 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5078752
[2026-11-02 11:58:52.746] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=4118520917 ntp_seconds=4002580732 ntp_fraction=3204045602 arrival_ns=11408463021400
[2026-11-02 11:58:52.747] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=4118520917
[2026-11-02 11:58:52.747] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:58:52.918] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=2459306278 ntp_seconds=4002580732 ntp_fraction=3942779977 arrival_ns=11408634775100
[2026-11-02 11:58:52.918] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=2459306278
[2026-11-02 11:58:52.918] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 11:58:53.164] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4963 source_master_ns=286825607051 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5124098
[2026-11-02 11:58:53.346] [info] [graph][runtime.node] datagram_reservation_trace stage=reservation_failed generation=1 endpoint=1 sequence=171936 release_master_ns=286299019401 deadline_master_ns=286365657124 original_protocol_deadline_master_ns=286399019401 prefix_completion_limit_master_ns=286365657124 activation_started_master_ns=286299191700 activation_completed_master_ns=286299192200 permit_started_master_ns=286365319200 permit_completed_master_ns=286365319400 snapshot_started_master_ns=286365319400 snapshot_sampled_master_ns=286365319500 snapshot_completed_master_ns=286365319600 reservation_completed_master_ns=286365340900 wait_started_master_ns=unavailable wait_completed_master_ns=unavailable submit_observed_master_ns=unavailable wait_attempts=0 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=0 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=0 wait_call_total_ns=0 wait_call_max_ns=0 wait_before_call_count=0 wait_before_call_total_ns=0 wait_before_call_max_ns=0 wait_arm_count=0 wait_arm_total_ns=0 wait_arm_max_ns=0 wait_api_wall_count=0 wait_api_wall_total_ns=0 wait_api_wall_max_ns=0 wait_return_to_next_clock_count=0 wait_return_to_next_clock_total_ns=0 wait_return_to_next_clock_max_ns=0 wait_target_lateness_count=0 wait_target_lateness_total_ns=0 wait_target_lateness_max_ns=0 original_error="IoFailure: Datagram GBRA reservation misses its immutable completion deadline sequence=171936 now_ns=286365319500 release_ns=286299019401 deadline_ns=286365657124 not_before_ns=286365532340 not_after_ns=286365442084 duration_ns=215040 rate=6250000 queue_wire_bytes=8656 average_queue_residence_ns=66236800"
[2026-11-02 11:58:53.346] [info] [graph][runtime.node] process.failed node=81 name=MediaScheduledDatagramSenderNode kind=ScheduledDatagramSender inputs=[channel=111 edge=111 stream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=123 edge=123 stream=Metadata edge_kind=Unknown payload=Unknown queued=0] outputs=[] error=IoFailure: Datagram GBRA reservation misses its immutable completion deadline sequence=171936 now_ns=286365319500 release_ns=286299019401 deadline_ns=286365657124 not_before_ns=286365532340 not_after_ns=286365442084 duration_ns=215040 rate=6250000 queue_wire_bytes=8656 average_queue_residence_ns=66236800
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.failed node=81 kind=ScheduledDatagramSender name=Common GCRA-paced nonblocking datagram sender error=IoFailure: Datagram GBRA reservation misses its immutable completion deadline sequence=171936 now_ns=286365319500 release_ns=286299019401 deadline_ns=286365657124 not_before_ns=286365532340 not_after_ns=286365442084 duration_ns=215040 rate=6250000 queue_wire_bytes=8656 average_queue_residence_ns=66236800
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=7 available=1 cpu_ns=1203125000 process_calls=47630
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=8 available=1 cpu_ns=171875000 process_calls=66926
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=27 available=1 cpu_ns=265625000 process_calls=75351
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=46 available=1 cpu_ns=593750000 process_calls=175205
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=47 available=1 cpu_ns=500000000 process_calls=177656
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=9 available=1 cpu_ns=15625000 process_calls=80
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=28 available=1 cpu_ns=0 process_calls=80
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=48 available=1 cpu_ns=15625000 process_calls=295
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=10 available=1 cpu_ns=0 process_calls=104
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=29 available=1 cpu_ns=0 process_calls=105
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=49 available=1 cpu_ns=0 process_calls=435
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=11 available=1 cpu_ns=93750000 process_calls=10612
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=12 available=1 cpu_ns=0 process_calls=80
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=30 available=1 cpu_ns=78125000 process_calls=10606
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=31 available=1 cpu_ns=0 process_calls=80
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=50 available=1 cpu_ns=343750000 process_calls=21061
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=4 available=1 cpu_ns=6265625000 process_calls=37721
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=51 available=1 cpu_ns=312500000 process_calls=30301
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=52 available=1 cpu_ns=0 process_calls=295
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=13 available=1 cpu_ns=0 process_calls=103
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=32 available=1 cpu_ns=0 process_calls=105
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=53 available=1 cpu_ns=0 process_calls=439
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=14 available=1 cpu_ns=140625000 process_calls=10740
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=17 available=1 cpu_ns=0 process_calls=80
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=33 available=1 cpu_ns=78125000 process_calls=10762
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=77 available=1 cpu_ns=0 process_calls=6
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=36 available=1 cpu_ns=0 process_calls=79
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=54 available=1 cpu_ns=187500000 process_calls=21257
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=55 available=1 cpu_ns=359375000 process_calls=30655
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=5 available=1 cpu_ns=328125000 process_calls=36286
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=59 available=1 cpu_ns=0 process_calls=293
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=74 available=1 cpu_ns=921875000 process_calls=61542
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=15 available=1 cpu_ns=109375000 process_calls=10714
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=34 available=1 cpu_ns=187500000 process_calls=10724
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=75 available=1 cpu_ns=296875000 process_calls=61600
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=56 available=1 cpu_ns=203125000 process_calls=21210
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=57 available=1 cpu_ns=312500000 process_calls=30417
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=16 available=1 cpu_ns=46875000 process_calls=10834
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=35 available=1 cpu_ns=156250000 process_calls=10833
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=58 available=1 cpu_ns=500000000 process_calls=51694
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=18 available=1 cpu_ns=31250000 process_calls=10662
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=37 available=1 cpu_ns=78125000 process_calls=10668
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=60 available=1 cpu_ns=359375000 process_calls=50911
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=78 available=1 cpu_ns=375000000 process_calls=60964
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=19 available=1 cpu_ns=46875000 process_calls=10716
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=38 available=1 cpu_ns=125000000 process_calls=10711
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=76 available=1 cpu_ns=2234375000 process_calls=124215
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=61 available=1 cpu_ns=375000000 process_calls=50679
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=20 available=1 cpu_ns=125000000 process_calls=10590
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=79 available=1 cpu_ns=1500000000 process_calls=98477
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=39 available=1 cpu_ns=125000000 process_calls=10591
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=62 available=1 cpu_ns=500000000 process_calls=50853
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=22 available=1 cpu_ns=1109375000 process_calls=13982
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=41 available=1 cpu_ns=1109375000 process_calls=14044
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=64 available=1 cpu_ns=2125000000 process_calls=34502
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=70 available=1 cpu_ns=796875000 process_calls=41789
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=23 available=1 cpu_ns=93750000 process_calls=10499
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=42 available=1 cpu_ns=15625000 process_calls=10473
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=65 available=1 cpu_ns=203125000 process_calls=20992
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=71 available=1 cpu_ns=265625000 process_calls=30658
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=24 available=1 cpu_ns=156250000 process_calls=11046
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=43 available=1 cpu_ns=156250000 process_calls=10963
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=66 available=1 cpu_ns=203125000 process_calls=21518
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=72 available=1 cpu_ns=484375000 process_calls=30535
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=26 available=1 cpu_ns=781250000 process_calls=13699
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=45 available=1 cpu_ns=890625000 process_calls=13699
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=68 available=1 cpu_ns=1359375000 process_calls=24729
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=73 available=1 cpu_ns=859375000 process_calls=49517
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=6 available=1 cpu_ns=1984375000 process_calls=46265
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=25 available=1 cpu_ns=93750000 process_calls=10544
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=44 available=1 cpu_ns=203125000 process_calls=10529
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=67 available=1 cpu_ns=171875000 process_calls=20875
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=2 available=1 cpu_ns=2843750000 process_calls=25270
[2026-11-02 11:58:53.347] [info] [graph][runtime.node] worker.thread_cpu node=80 available=1 cpu_ns=0 process_calls=5
[2026-11-02 11:58:53.348] [info] [graph][runtime.node] worker.thread_cpu node=81 available=1 cpu_ns=9390625000 process_calls=59510
[2026-11-02 11:58:53.360] [info] [graph][runtime.node] datagram_batch_trace stage=last_completed generation=1 first_sequence=171891 last_sequence=171891 maximum_submit_interval_ns=9300 maximum_submit_interval_sequence=171891 maximum_start_lateness_ns=16640 maximum_start_lateness_sequence=171891 first_release_master_ns=286299019400 first_deadline_master_ns=286399019400 last_release_master_ns=286299019400 last_deadline_master_ns=286399019400 first_submitted_master_ns=286299123300 last_submitted_master_ns=286299123300 worst_wait_started_master_ns=286299113800 worst_wait_completed_master_ns=286299113800 worst_submit_observed_master_ns=286299114000 wait_attempts=0 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=0 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=0 wait_call_total_ns=0 wait_call_max_ns=0 wait_before_call_count=0 wait_before_call_total_ns=0 wait_before_call_max_ns=0 wait_arm_count=0 wait_arm_total_ns=0 wait_arm_max_ns=0 wait_api_wall_count=0 wait_api_wall_total_ns=0 wait_api_wall_max_ns=0 wait_return_to_next_clock_count=0 wait_return_to_next_clock_total_ns=0 wait_return_to_next_clock_max_ns=0 wait_target_lateness_count=0 wait_target_lateness_total_ns=0 wait_target_lateness_max_ns=0
[2026-11-02 11:58:53.360] [info] [graph][runtime.node] datagram_batch_trace stage=active generation=1 first_sequence=171892 last_sequence=171942 maximum_submit_interval_ns=792700 maximum_submit_interval_sequence=171910 maximum_start_lateness_ns=33723860 maximum_start_lateness_sequence=171904 first_release_master_ns=286299019401 first_deadline_master_ns=286365657124 last_release_master_ns=286299019401 last_deadline_master_ns=286365657124 first_submitted_master_ns=286299205100 last_submitted_master_ns=286365317300 worst_wait_started_master_ns=286306596100 worst_wait_completed_master_ns=286340529100 worst_submit_observed_master_ns=286340531400 wait_attempts=43 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=43 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=43 wait_call_total_ns=63261700 wait_call_max_ns=33930200 wait_before_call_count=43 wait_before_call_total_ns=3900 wait_before_call_max_ns=500 wait_arm_count=43 wait_arm_total_ns=183400 wait_arm_max_ns=12200 wait_api_wall_count=43 wait_api_wall_total_ns=63038600 wait_api_wall_max_ns=33925600 wait_return_to_next_clock_count=43 wait_return_to_next_clock_total_ns=35800 wait_return_to_next_clock_max_ns=2800 wait_target_lateness_count=43 wait_target_lateness_total_ns=54384280 wait_target_lateness_max_ns=33721560 worst_wait_metric=call_wall worst_wait_sequence=171904 worst_wait_target_master_ns=286306807540 worst_wait_sampled_master_ns=286306596100 worst_wait_sampled_steady_ns=11409004490100 worst_wait_timeout_ns=211440 worst_wait_entered_steady_ns=11409004490100 worst_wait_returned_steady_ns=11409038420300 worst_wait_arm_before_steady_ns=11409004490400 worst_wait_arm_after_steady_ns=11409004493000 worst_wait_api_before_steady_ns=11409004493000 worst_wait_api_returned_steady_ns=11409038418600 worst_wait_windows_due_100ns=-2115 worst_wait_windows_result=1 worst_wait_windows_error=unavailable worst_wait_windows_pid=34612 worst_wait_windows_tid=4036 worst_wait_windows_qpc_frequency_hz=10000000 worst_wait_next_master_ns=286340529100 worst_wait_next_steady_ns=11409038423100 worst_wait_outcome=deadline
[2026-11-02 11:58:53.360] [info] [graph][runtime.node] datagram_successful_submit_trace stage=worst_success generation=1 endpoint=1 sequence=82663 pacing_lateness_ns=34553566 matches_pacing_worst=1 materialized_master_ns=unavailable enqueued_master_ns=unavailable submit_interval_scope=pre_transport_checks_and_transport socket_only_ns=unavailable release_master_ns=131432352734 deadline_master_ns=131498987360 original_protocol_deadline_master_ns=131532352734 prefix_completion_limit_master_ns=131498987360 reserved_at_master_ns=131422714000 not_before_master_ns=131432352734 not_after_master_ns=131498952800 service_duration_ns=34560 wire_bytes_per_second=6250000 submit_started_master_ns=131466906300 submit_completed_master_ns=131467102900 activation_started_master_ns=131422711300 activation_completed_master_ns=131422713000 permit_started_master_ns=131422713500 permit_completed_master_ns=131422713700 snapshot_started_master_ns=131422713900 snapshot_sampled_master_ns=131422714000 snapshot_completed_master_ns=131422714400 reservation_completed_master_ns=131422716800 wait_started_master_ns=131422717600 wait_completed_master_ns=131466904000 submit_observed_master_ns=131466906300 wait_attempts=1 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=1 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=1 wait_call_total_ns=44182900 wait_call_max_ns=44182900 wait_before_call_count=1 wait_before_call_total_ns=200 wait_before_call_max_ns=200 wait_arm_count=1 wait_arm_total_ns=3400 wait_arm_max_ns=3400 wait_api_wall_count=1 wait_api_wall_total_ns=44176900 wait_api_wall_max_ns=44176900 wait_return_to_next_clock_count=1 wait_return_to_next_clock_total_ns=3300 wait_return_to_next_clock_max_ns=3300 wait_target_lateness_count=1 wait_target_lateness_total_ns=34551266 wait_target_lateness_max_ns=34551266 worst_wait_metric=call_wall worst_wait_sequence=82663 worst_wait_target_master_ns=131432352734 worst_wait_sampled_master_ns=131422717600 worst_wait_sampled_steady_ns=11254120611600 worst_wait_timeout_ns=9635134 worst_wait_entered_steady_ns=11254120611800 worst_wait_returned_steady_ns=11254164794700 worst_wait_arm_before_steady_ns=11254120612500 worst_wait_arm_after_steady_ns=11254120615900 worst_wait_api_before_steady_ns=11254120615900 worst_wait_api_returned_steady_ns=11254164792800 worst_wait_windows_due_100ns=-96352 worst_wait_windows_result=1 worst_wait_windows_error=unavailable worst_wait_windows_pid=34612 worst_wait_windows_tid=4036 worst_wait_windows_qpc_frequency_hz=10000000 worst_wait_next_master_ns=131466904000 worst_wait_next_steady_ns=11254164798000 worst_wait_outcome=deadline
[2026-11-02 11:58:53.360] [info] [graph][runtime.node] scheduled_datagram_sender stage=failed generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=30349 committed_datagrams=171935 committed_payload_bytes=201216588 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=79296 pacing_reserved=171935 pacing_cancelled=0 pacing_submitted=171935 pacing_maximum_submit_lateness_ns=34553566 pacing_worst_late_global_sequence=82663 pacing_worst_success_sample_available=1 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=7 backlog_current_wire_bytes=8656 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=79296 backlog_max_residence_ns=75129000 last_materialized_sequence=171942 last_scheduled_sequence=171942 last_submitted_sequence=171935 last_committed_sequence=171935 submitted_service_generation=1 submitted_service_sequence=171935 submitted_service_completed_ns=286365317300 submitted_service_duration_ns=215040 submitted_service_available_ns=286365532340 submitted_scope_available_ns=286365532340 shared_scope_charged_datagrams=171935 shared_scope_charged_wire_bytes=206030768 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2154 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2154 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=188010 endpoint_1_target_effective_socket_bytes=188010 endpoint_1_api_requested_socket_bytes=188010 endpoint_1_effective_socket_bytes=188010 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=171935 endpoint_1_committed_payload_bytes=201216588
[2026-11-02 11:58:53.361] [info] [graph][runtime.node] worker.secondary_failure node=81 kind=ScheduledDatagramSender name=Common GCRA-paced nonblocking datagram sender error=IoFailure: Datagram GBRA reservation misses its immutable completion deadline sequence=171936 now_ns=286365319500 release_ns=286299019401 deadline_ns=286365657124 not_before_ns=286365532340 not_after_ns=286365442084 duration_ns=215040 rate=6250000 queue_wire_bytes=8656 average_queue_residence_ns=66236800
[2026-11-02 11:58:53.491] [info] [graph][runtime.lifecycle] node_abort_payload node=81 before_objects=58 after_objects=58 before_bytes=219254 after_bytes=219254 before_reservations=219474 after_reservations=219474 before_releases=219416 after_releases=219416
[2026-11-02 11:58:53.492] [info] [graph][runtime.node] mpegts_wire_materializer stage=aborted maximum_materialized_after_release_ns=54270748 worst_materialized_at_ns=106486625500 worst_release_ns=106432354752 worst_deadline_ns=106532352734 worst_global_sequence=71966
[2026-11-02 11:58:53.492] [info] [graph][runtime.lifecycle] node_abort_payload node=79 before_objects=58 after_objects=58 before_bytes=219254 after_bytes=219254 before_reservations=219474 after_reservations=219474 before_releases=219416 after_releases=219416
[2026-11-02 11:58:53.492] [info] [graph][runtime.node] au_processing_trace stage=pending_au_abort generation=1 stream=video canonical_sequence=8423 emit_master_ns=286299019401 dispatch_master_ns=286399019401 ready_master_ns=286298877900 scheduled_created_steady_ns=11408996725300 adapter_dequeued_steady_ns=11408996756500 adapted_created_steady_ns=11408996756900 write_entered_steady_ns=11408996770800 session_ready_steady_ns=11408996771900 codec_receive_begin_steady_ns=11408939772000 codec_receive_end_steady_ns=11408939772400 codec_encoder_node=2 codec_selected_token_id=8423 codec_selected_token_generation=1 codec_packet_token_id=8423 codec_packet_token_generation=1 codec_token_mapping=copy_opaque codec_input_pts=8422 codec_timebase_num=1 codec_timebase_den=30 codec_accepted_send_count=1 codec_token_output_ordinal=1 codec_packet_pts=8422 codec_packet_dts=8422 codec_packet_bytes=64916 codec_identity_incomplete=0
[2026-11-02 11:58:53.493] [info] [graph][runtime.node] ts_prefix_commit_summary stage=current_au_abort generation=1 stream=video canonical_sequence=8423 emit_master_ns=286299019401 dispatch_master_ns=286399019401 published=1 completed=0 samples=0 would_block=8 incomplete=0 publish_to_protocol_commit_completed_total_ns=0 publish_to_protocol_commit_completed_max_ns=0 publish_to_protocol_commit_completed_max_prefix_offset=0 protocol_commit_completed_to_mux_resume_total_ns=0 protocol_commit_completed_to_mux_resume_max_ns=0 protocol_commit_completed_to_mux_resume_max_prefix_offset=0
[2026-11-02 11:58:53.493] [info] [graph][runtime.node] au_processing_trace stage=previous_completed_au_trace_on_abort generation=1 stream=audio canonical_sequence=12091 emit_master_ns=286294937768 dispatch_master_ns=286394937768 ready_master_ns=286289660300 scheduled_created_steady_ns=11408987043700 adapter_dequeued_steady_ns=11408987182000 adapted_created_steady_ns=11408987182500 write_entered_steady_ns=11408987549400 session_ready_steady_ns=11408987554300 codec_identity=unavailable
[2026-11-02 11:58:53.493] [info] [graph][runtime.node] ts_prefix_commit_summary stage=previous_completed_au_on_abort generation=1 stream=audio canonical_sequence=12091 emit_master_ns=286294937768 dispatch_master_ns=286394937768 published=1 completed=1 samples=1 would_block=2 incomplete=0 publish_to_protocol_commit_completed_total_ns=6239500 publish_to_protocol_commit_completed_max_ns=6239500 publish_to_protocol_commit_completed_max_prefix_offset=0 protocol_commit_completed_to_mux_resume_total_ns=104900 protocol_commit_completed_to_mux_resume_max_ns=104900 protocol_commit_completed_to_mux_resume_max_prefix_offset=0
[2026-11-02 11:58:53.493] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 produced_datagrams=171942 produced_ts_payload_bytes=201225048 pending_bytes=0 peak_pending_bytes=110920 access_units=20514 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=45625601 maximum_mux_input_after_emit_ns=76038794 maximum_mux_input_after_dispatch_ns=-23961206 worst_mux_input_ready_ns=106485987900 worst_mux_input_emit_ns=106409949106 worst_mux_input_dispatch_ns=106509949106 maximum_protocol_batch_produced_after_release_ns=54254548 worst_protocol_batch_produced_ns=106486609300 worst_protocol_batch_release_ns=106432354752 worst_protocol_batch_deadline_ns=106532352734 worst_protocol_batch_datagrams=22 exit_reason=aborted
[2026-11-02 11:58:53.493] [info] [graph][runtime.lifecycle] node_abort_payload node=76 before_objects=58 after_objects=58 before_bytes=219254 after_bytes=219254 before_reservations=219474 after_reservations=219474 before_releases=219416 after_releases=219416
[2026-11-02 11:58:53.494] [info] [graph][runtime.lifecycle] node_abort_payload node=78 before_objects=58 after_objects=58 before_bytes=219254 after_bytes=219254 before_reservations=219474 after_reservations=219474 before_releases=219416 after_releases=219416
[2026-11-02 11:58:53.494] [info] [graph][runtime.lifecycle] node_abort_payload node=75 before_objects=58 after_objects=58 before_bytes=219254 after_bytes=219254 before_reservations=219474 after_reservations=219474 before_releases=219416 after_releases=219416
[2026-11-02 11:58:53.494] [info] [graph][runtime.node] audio_stage_observation node=74 stage=scheduler_input count=12094 last_generation=1 last_begin=12383232 last_end=12384256 last_rate=44100 last_steady_ns=11409043118000 maximum_inter_event_gap_ns=66782100 gap_begin_generation=1 gap_begin_begin=4446208 gap_begin_end=4447232 gap_begin_rate=44100 gap_begin_steady_ns=11229074390300 gap_end_generation=1 gap_end_begin=4447232 gap_end_end=4448256 gap_end_rate=44100 gap_end_steady_ns=11229141172400
[2026-11-02 11:58:53.494] [info] [graph][runtime.node] av_scheduler_correlated_wait generation=1 canonical_sequence=4343 missing=video begin_steady_ns=11229074402700 end_steady_ns=11229141120800 observed_wait_ns=66718100 held_dispatch_master_ns=106486729151 begin_master_ns=106376508600 end_master_ns=106443226000 dispatch_remaining_at_end_ns=43503151 resumed_dispatch_master_ns=106499019401
[2026-11-02 11:58:53.494] [info] [graph][runtime.lifecycle] node_abort_payload node=74 before_objects=58 after_objects=57 before_bytes=219254 after_bytes=212672 before_reservations=219474 after_reservations=219474 before_releases=219416 after_releases=219417
[2026-11-02 11:58:53.494] [info] [graph][runtime.node] audio_stage_observation node=5 stage=canonicalizer_input count=12094 last_generation=1 last_begin=12383232 last_end=12384256 last_rate=44100 last_steady_ns=11409043085900 maximum_inter_event_gap_ns=58536500 gap_begin_generation=1 gap_begin_begin=5910528 gap_begin_end=5911552 gap_begin_rate=44100 gap_begin_steady_ns=11262269170700 gap_end_generation=1 gap_end_begin=5911552 gap_end_end=5912576 gap_end_rate=44100 gap_end_steady_ns=11262327707200
[2026-11-02 11:58:53.495] [info] [graph][runtime.node] audio_stage_observation node=5 stage=canonicalizer_committed count=12094 last_generation=1 last_begin=12383232 last_end=12384256 last_rate=44100 last_steady_ns=11409043093000 maximum_inter_event_gap_ns=58538100 gap_begin_generation=1 gap_begin_begin=5910528 gap_begin_end=5911552 gap_begin_rate=44100 gap_begin_steady_ns=11262269178700 gap_end_generation=1 gap_end_begin=5911552 gap_end_end=5912576 gap_end_rate=44100 gap_end_steady_ns=11262327716800
[2026-11-02 11:58:53.495] [info] [graph][runtime.lifecycle] node_abort_payload node=5 before_objects=57 after_objects=57 before_bytes=212672 after_bytes=212672 before_reservations=219474 after_reservations=219474 before_releases=219417 after_releases=219417
[2026-11-02 11:58:53.495] [info] [graph][runtime.lifecycle] node_abort_payload node=77 before_objects=57 after_objects=57 before_bytes=212672 after_bytes=212672 before_reservations=219474 after_reservations=219474 before_releases=219417 after_releases=219417
[2026-11-02 11:58:53.495] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_input count=12096 last_generation=1 last_begin=12385280 last_end=12386304 last_rate=44100 last_steady_ns=11409042753300 maximum_inter_event_gap_ns=58499400 gap_begin_generation=1 gap_begin_begin=5912576 gap_begin_end=5913600 gap_begin_rate=44100 gap_begin_steady_ns=11262269066100 gap_end_generation=1 gap_end_begin=5913600 gap_end_end=5914624 gap_end_rate=44100 gap_end_steady_ns=11262327565500
[2026-11-02 11:58:53.495] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_submitted count=12096 last_generation=1 last_begin=12385280 last_end=12386304 last_rate=44100 last_steady_ns=11409043014500 maximum_inter_event_gap_ns=58538500 gap_begin_generation=1 gap_begin_begin=5912576 gap_begin_end=5913600 gap_begin_rate=44100 gap_begin_steady_ns=11262269145300 gap_end_generation=1 gap_end_begin=5913600 gap_end_end=5914624 gap_end_rate=44100 gap_end_steady_ns=11262327683800
[2026-11-02 11:58:53.495] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_packet count=12094 last_generation=1 last_begin=12383232 last_end=12384256 last_rate=44100 last_steady_ns=11409043025400 maximum_inter_event_gap_ns=58540000 gap_begin_generation=1 gap_begin_begin=5910528 gap_begin_end=5911552 gap_begin_rate=44100 gap_begin_steady_ns=11262269150800 gap_end_generation=1 gap_end_begin=5911552 gap_end_end=5912576 gap_end_rate=44100 gap_end_steady_ns=11262327690800
[2026-11-02 11:58:53.496] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_committed count=12094 last_generation=1 last_begin=12383232 last_end=12384256 last_rate=44100 last_steady_ns=11409043032300 maximum_inter_event_gap_ns=58542600 gap_begin_generation=1 gap_begin_begin=5910528 gap_begin_end=5911552 gap_begin_rate=44100 gap_begin_steady_ns=11262269156200 gap_end_generation=1 gap_end_begin=5911552 gap_end_end=5912576 gap_end_rate=44100 gap_end_steady_ns=11262327698800
[2026-11-02 11:58:53.496] [info] [graph][runtime.lifecycle] codec_owner_release node=4 kind=AudioEncode owner_references=4
[2026-11-02 11:58:53.496] [info] [graph][runtime.lifecycle] node_abort_payload node=4 before_objects=57 after_objects=57 before_bytes=212672 after_bytes=212672 before_reservations=219474 after_reservations=219474 before_releases=219417 after_releases=219417
[2026-11-02 11:58:53.496] [info] [graph][runtime.lifecycle] node_abort_payload node=80 before_objects=57 after_objects=57 before_bytes=212672 after_bytes=212672 before_reservations=219474 after_reservations=219474 before_releases=219417 after_releases=219417
[2026-11-02 11:58:53.496] [info] [graph][runtime.node] audio_stage_observation node=7 stage=aggregate_built count=12096 last_generation=1 last_begin=12385280 last_end=12386304 last_rate=44100 last_steady_ns=11409042702900 maximum_inter_event_gap_ns=58485000 gap_begin_generation=1 gap_begin_begin=5912576 gap_begin_end=5913600 gap_begin_rate=44100 gap_begin_steady_ns=11262269045900 gap_end_generation=1 gap_end_begin=5913600 gap_end_end=5914624 gap_end_rate=44100 gap_end_steady_ns=11262327530900
[2026-11-02 11:58:53.496] [info] [graph][runtime.node] audio_stage_observation node=7 stage=aggregate_committed count=12096 last_generation=1 last_begin=12385280 last_end=12386304 last_rate=44100 last_steady_ns=11409042723600 maximum_inter_event_gap_ns=58672900 gap_begin_generation=1 gap_begin_begin=5912576 gap_begin_end=5913600 gap_begin_rate=44100 gap_begin_steady_ns=11262269055300 gap_end_generation=1 gap_end_begin=5913600 gap_end_end=5914624 gap_end_rate=44100 gap_end_steady_ns=11262327728200
[2026-11-02 11:58:53.496] [info] [graph][runtime.node] continuous_audio_aggregate.summary reason=abort output_group=composition-r380.output audio_commits=12096 real_samples=10091520 silence_samples=2294784
[2026-11-02 11:58:53.496] [info] [graph][runtime.lifecycle] node_abort_payload node=7 before_objects=57 after_objects=51 before_bytes=212672 after_bytes=163520 before_reservations=219474 after_reservations=219474 before_releases=219417 after_releases=219423
[2026-11-02 11:58:53.497] [info] [graph][runtime.lifecycle] codec_owner_release node=2 kind=VideoEncode owner_references=4
[2026-11-02 11:58:53.497] [info] [graph][runtime.node] video_encoder_execution_observation node=2 count=8426 maximum_call_ns=66578500 maximum_begin_steady_ns=11229074329200 maximum_end_steady_ns=11229140907700 maximum_encoder_pts=3025 maximum_native_result=0 maximum_between_submissions_ns=63218000 before_gap_begin_steady_ns=11254098216100 before_gap_end_steady_ns=11254102491700 before_gap_encoder_pts=3777 before_gap_native_result=0 after_gap_begin_steady_ns=11254165709700 after_gap_end_steady_ns=11254168954100 after_gap_encoder_pts=3778 after_gap_native_result=0
[2026-11-02 11:58:53.497] [info] [graph][runtime.lifecycle] node_abort_payload node=2 before_objects=51 after_objects=51 before_bytes=163520 after_bytes=163520 before_reservations=219474 after_reservations=219474 before_releases=219423 after_releases=219423
[2026-11-02 11:58:53.497] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=process count=46265 maximum_ns=47222500 begin_steady_ns=11229068738100 end_steady_ns=11229115960600 video_cursor=3026 audio_cursor=not_owned next_video_cursor=3026 next_audio_cursor=not_owned error=None return_state=progress deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-11-02 11:58:53.497] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=purge count=46265 maximum_ns=8228400 begin_steady_ns=11242170339600 end_steady_ns=11242178568000 video_cursor=3419 audio_cursor=not_owned next_video_cursor=3419 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 11:58:53.497] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=bind count=46265 maximum_ns=652000 begin_steady_ns=11187765392500 end_steady_ns=11187766044500 video_cursor=1787 audio_cursor=not_owned next_video_cursor=1787 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 11:58:53.497] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=consume count=46265 maximum_ns=14289500 begin_steady_ns=11208375119500 end_steady_ns=11208389409000 video_cursor=2406 audio_cursor=not_owned next_video_cursor=2406 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 11:58:53.497] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=activate count=13 maximum_ns=77900 begin_steady_ns=11128196913000 end_steady_ns=11128196990900 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 11:58:53.498] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=build_video count=8428 maximum_ns=47073100 begin_steady_ns=11229068860500 end_steady_ns=11229115933600 video_cursor=3026 audio_cursor=not_owned next_video_cursor=3026 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 11:58:53.498] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=canvas count=8428 maximum_ns=47036900 begin_steady_ns=11229068871400 end_steady_ns=11229115908300 video_cursor=3026 audio_cursor=not_owned next_video_cursor=3026 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 11:58:53.498] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=emit count=8428 maximum_ns=9338300 begin_steady_ns=11256034264000 end_steady_ns=11256043602300 video_cursor=3835 audio_cursor=not_owned next_video_cursor=3835 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 11:58:53.498] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=between_calls_previous_return count=46264 maximum_ns=109228800 begin_steady_ns=11128082966900 end_steady_ns=11128192195700 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=waiting deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-11-02 11:58:53.498] [info] [graph][runtime.node] continuous_aggregate.summary reason=abort output_group=composition-r380.output output_generation=1 video_commits=8427 source[0].group=composition-r380.source.0 source[0].identity=composition-r380.source.0.input.video source[0].real_frames=3419 source[0].black_frames=5008 source[0].contribution_generation=0 source[1].group=composition-r380.source.1 source[1].identity=composition-r380.source.1.input.video source[1].real_frames=3419 source[1].black_frames=5008 source[1].contribution_generation=0 source[2].group=composition-r380.source.2 source[2].identity=composition-r380.source.2.input.video source[2].real_frames=6865 source[2].black_frames=1562 source[2].contribution_generation=2
[2026-11-02 11:58:53.498] [info] [graph][runtime.node] cuda_canvas_call stage=context_push count=6870 operation=copy maximum_ns=490600 begin_steady_ns=11386764132300 end_steady_ns=11386764622900 native_result=0 thread_cpu_ns=0
[2026-11-02 11:58:53.498] [info] [graph][runtime.node] cuda_canvas_call stage=source_wait count=20578 operation=copy maximum_ns=553200 begin_steady_ns=11180197890700 end_steady_ns=11180198443900 native_result=0 thread_cpu_ns=0
[2026-11-02 11:58:53.498] [info] [graph][runtime.node] cuda_canvas_call stage=plane_copy count=41158 operation=copy maximum_ns=2952100 begin_steady_ns=11159733967700 end_steady_ns=11159736919800 native_result=0 thread_cpu_ns=0
[2026-11-02 11:58:53.498] [info] [graph][runtime.node] cuda_canvas_call stage=destination_wait count=2 operation=upload maximum_ns=71000 begin_steady_ns=11128012449000 end_steady_ns=11128012520000 native_result=0 thread_cpu_ns=0
[2026-11-02 11:58:53.498] [info] [graph][runtime.node] cuda_canvas_call stage=context_pop count=6870 operation=copy maximum_ns=1920400 begin_steady_ns=11232202795200 end_steady_ns=11232204715600 native_result=0 thread_cpu_ns=0
[2026-11-02 11:58:53.499] [info] [graph][runtime.node] cuda_canvas_call stage=batch_completion count=6868 operation=copy maximum_ns=46290000 begin_steady_ns=11229069585100 end_steady_ns=11229115875100 native_result=0 thread_cpu_ns=0
[2026-11-02 11:58:53.499] [info] [graph][runtime.node] canvas_compose_correlated scope=attempt begin_steady_ns=11229068871800 end_steady_ns=11229115907400 wall_ns=47035600 thread_cpu_ns=0 batch_pid=34612 batch_tid=18808 batch_context=2277832300144 batch_stream=2278276490080 batch_submit_begin_steady_ns=11229068880100 batch_submit_end_steady_ns=11229069585000 batch_wait_begin_steady_ns=11229069585400 batch_wait_end_steady_ns=11229115871500 batch_timing_preparation=ready batch_timing_preparation_native=0 batch_start_event_native=0 batch_end_event_native=0 batch_sync_native=0 batch_elapsed_native=0 batch_gpu_elapsed_available=1 batch_gpu_elapsed_ms=46.8623 batch_gpu_interval=event_execution_interval_excludes_pre_start_queue surface_count=1 surface_total_ns=1000 surface_maximum_ns=1000 surface_maximum_tile=none surface_maximum_result=unavailable validate_count=3 validate_total_ns=3400 validate_maximum_ns=2200 validate_maximum_tile=2 validate_maximum_result=0 black_count=1 black_total_ns=80800 black_maximum_ns=80800 black_maximum_tile=none black_maximum_result=0 tile_count=3 tile_total_ns=545300 tile_maximum_ns=362800 tile_maximum_tile=1 tile_maximum_result=0 publish_count=1 publish_total_ns=8800 publish_maximum_ns=8800 publish_maximum_tile=none publish_maximum_result=0 context_push_count=1 context_push_total_ns=300 context_push_maximum_ns=300 context_push_maximum_tile=none context_push_maximum_result=0 source_wait_count=4 source_wait_total_ns=7700 source_wait_maximum_ns=3600 source_wait_maximum_tile=2 source_wait_maximum_result=0 plane_copy_count=8 plane_copy_total_ns=453700 plane_copy_maximum_ns=194000 plane_copy_maximum_tile=1 plane_copy_maximum_result=0 destination_wait_count=0 destination_wait_total_ns=0 destination_wait_maximum_ns=0 destination_wait_maximum_tile=none destination_wait_maximum_result=unavailable context_pop_count=1 context_pop_total_ns=6300 context_pop_maximum_ns=6300 context_pop_maximum_tile=none context_pop_maximum_result=0 batch_completion_count=1 batch_completion_total_ns=46286500 batch_completion_maximum_ns=46286500 batch_completion_maximum_tile=none batch_completion_maximum_result=0 source_query_completed_count=0 source_query_ready_count=3 source_query_ready_maximum_ns=143600 source_query_ready_begin_steady_ns=11229069166100 source_query_ready_end_steady_ns=11229069309700 source_query_ready_tile=1 source_query_ready_operation=event_query source_query_ready_native=0 source_query_ready_restore_native=0 source_query_pending_count=0 source_query_unavailable_count=0 source_query_error_count=0
[2026-11-02 11:58:53.499] [info] [graph][runtime.lifecycle] node_abort_payload node=6 before_objects=51 after_objects=37 before_bytes=163520 after_bytes=163520 before_reservations=219474 after_reservations=219474 before_releases=219423 after_releases=219437
[2026-11-02 11:58:53.499] [info] [graph][runtime.node] source_video_trace_summary node_id=67 node=VideoPreparationNode generation=2 reason=abort steady_ns=11409216004800 input_count=3461 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3461 published_count=3460 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 11:58:53.499] [info] [graph][runtime.lifecycle] node_abort_payload node=67 before_objects=37 after_objects=36 before_bytes=163520 after_bytes=163520 before_reservations=219474 after_reservations=219474 before_releases=219437 after_releases=219438
[2026-11-02 11:58:53.499] [info] [graph][runtime.node] source_video_trace_summary node_id=44 node=VideoPreparationNode generation=1 reason=abort steady_ns=11409216198200 input_count=3433 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3433 published_count=3432 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 11:58:53.499] [info] [graph][runtime.lifecycle] node_abort_payload node=44 before_objects=36 after_objects=36 before_bytes=163520 after_bytes=163520 before_reservations=219474 after_reservations=219474 before_releases=219438 after_releases=219438
[2026-11-02 11:58:53.499] [info] [graph][runtime.node] source_video_trace_summary node_id=25 node=VideoPreparationNode generation=1 reason=abort steady_ns=11409216357200 input_count=3433 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3433 published_count=3432 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 11:58:53.499] [info] [graph][runtime.lifecycle] node_abort_payload node=25 before_objects=36 after_objects=36 before_bytes=163520 after_bytes=163520 before_reservations=219474 after_reservations=219474 before_releases=219438 after_releases=219438
[2026-11-02 11:58:53.499] [info] [graph][runtime.lifecycle] codec_owner_release node=73 kind=AudioResample owner_references=1
[2026-11-02 11:58:53.500] [info] [graph][runtime.lifecycle] node_abort_payload node=73 before_objects=36 after_objects=34 before_bytes=163520 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219438 after_releases=219440
[2026-11-02 11:58:53.500] [info] [graph][runtime.node] source_video_trace_summary node_id=68 node=VideoFilterNode generation=2 reason=abort steady_ns=11409217045000 input_count=3461 accepted_count=3461 would_block_count=0 resolved_count=3461 output_attempt_count=0 published_count=3461 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 11:58:53.508] [info] [graph][runtime.lifecycle] node_abort_payload node=68 before_objects=34 after_objects=34 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219440 after_releases=219440
[2026-11-02 11:58:53.508] [info] [graph][runtime.node] source_video_trace_summary node_id=45 node=VideoFilterNode generation=1 reason=abort steady_ns=11409225543100 input_count=3440 accepted_count=3440 would_block_count=0 resolved_count=3440 output_attempt_count=0 published_count=3439 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 11:58:53.514] [info] [graph][runtime.lifecycle] node_abort_payload node=45 before_objects=34 after_objects=34 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219440 after_releases=219440
[2026-11-02 11:58:53.515] [info] [graph][runtime.node] source_video_trace_summary node_id=26 node=VideoFilterNode generation=1 reason=abort steady_ns=11409231748000 input_count=3440 accepted_count=3440 would_block_count=0 resolved_count=3440 output_attempt_count=0 published_count=3439 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 11:58:53.518] [info] [graph][runtime.lifecycle] node_abort_payload node=26 before_objects=34 after_objects=34 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219440 after_releases=219440
[2026-11-02 11:58:53.518] [info] [graph][runtime.lifecycle] node_abort_payload node=72 before_objects=34 after_objects=34 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219440 after_releases=219440
[2026-11-02 11:58:53.518] [info] [graph][runtime.node] source_video_trace_summary node_id=66 node=VideoFrameRateNode generation=2 reason=abort steady_ns=11409235067800 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3461 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:58:53.518] [info] [graph][runtime.lifecycle] node_abort_payload node=66 before_objects=34 after_objects=33 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219440 after_releases=219441
[2026-11-02 11:58:53.518] [info] [graph][runtime.node] source_video_trace_summary node_id=43 node=VideoFrameRateNode generation=1 reason=abort steady_ns=11409235239000 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3446 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:58:53.518] [info] [graph][runtime.lifecycle] node_abort_payload node=43 before_objects=33 after_objects=33 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219441 after_releases=219441
[2026-11-02 11:58:53.518] [info] [graph][runtime.node] source_video_trace_summary node_id=24 node=VideoFrameRateNode generation=1 reason=abort steady_ns=11409235366300 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3446 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:58:53.518] [info] [graph][runtime.lifecycle] node_abort_payload node=24 before_objects=33 after_objects=33 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219441 after_releases=219441
[2026-11-02 11:58:53.518] [info] [graph][runtime.lifecycle] node_abort_payload node=71 before_objects=33 after_objects=33 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219441 after_releases=219441
[2026-11-02 11:58:53.518] [info] [graph][runtime.node] source_video_trace_summary node_id=65 node=HardwareTransferNode generation=2 reason=abort steady_ns=11409235560000 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3461 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:58:53.519] [info] [graph][runtime.lifecycle] node_abort_payload node=65 before_objects=33 after_objects=33 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219441 after_releases=219441
[2026-11-02 11:58:53.519] [info] [graph][runtime.node] source_video_trace_summary node_id=42 node=HardwareTransferNode generation=1 reason=abort steady_ns=11409235681400 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3460 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:58:53.519] [info] [graph][runtime.lifecycle] node_abort_payload node=42 before_objects=33 after_objects=33 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219441 after_releases=219441
[2026-11-02 11:58:53.519] [info] [graph][runtime.node] source_video_trace_summary node_id=23 node=HardwareTransferNode generation=1 reason=abort steady_ns=11409235798400 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3460 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:58:53.519] [info] [graph][runtime.lifecycle] node_abort_payload node=23 before_objects=33 after_objects=33 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219441 after_releases=219441
[2026-11-02 11:58:53.519] [info] [graph][runtime.lifecycle] codec_owner_release node=70 kind=AudioDecode owner_references=2
[2026-11-02 11:58:53.519] [info] [graph][runtime.lifecycle] node_abort_payload node=70 before_objects=33 after_objects=33 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219441 after_releases=219441
[2026-11-02 11:58:53.519] [info] [graph][runtime.node] source_video_trace_summary node_id=64 node=VideoDecodeNode generation=2 reason=abort steady_ns=11409236110500 input_count=3461 accepted_count=3461 would_block_count=0 resolved_count=3461 output_attempt_count=0 published_count=3461 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=108 incomplete=0
[2026-11-02 11:58:53.519] [info] [graph][runtime.lifecycle] codec_owner_release node=64 kind=VideoDecode owner_references=2
[2026-11-02 11:58:53.526] [info] [graph][runtime.lifecycle] node_abort_payload node=64 before_objects=33 after_objects=33 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219441 after_releases=219441
[2026-11-02 11:58:53.526] [info] [graph][runtime.node] source_video_trace_summary node_id=41 node=VideoDecodeNode generation=1 reason=abort steady_ns=11409243372500 input_count=3461 accepted_count=3461 would_block_count=0 resolved_count=3461 output_attempt_count=0 published_count=3460 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=108 incomplete=0
[2026-11-02 11:58:53.526] [info] [graph][runtime.lifecycle] codec_owner_release node=41 kind=VideoDecode owner_references=2
[2026-11-02 11:58:53.537] [info] [graph][runtime.lifecycle] node_abort_payload node=41 before_objects=33 after_objects=33 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219441 after_releases=219441
[2026-11-02 11:58:53.541] [info] [graph][runtime.node] source_video_trace_summary node_id=22 node=VideoDecodeNode generation=1 reason=abort steady_ns=11409258045600 input_count=3461 accepted_count=3461 would_block_count=0 resolved_count=3461 output_attempt_count=0 published_count=3460 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=108 incomplete=0
[2026-11-02 11:58:53.541] [info] [graph][runtime.lifecycle] codec_owner_release node=22 kind=VideoDecode owner_references=2
[2026-11-02 11:58:53.555] [info] [graph][runtime.lifecycle] node_abort_payload node=22 before_objects=33 after_objects=33 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219441 after_releases=219441
[2026-11-02 11:58:53.555] [info] [graph][runtime.node] source_video_trace_summary node_id=62 node=MediaAvBoundReleaseExtractorNode generation=2 reason=abort steady_ns=11409272075900 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=3446 release_committed_count=3446 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:58:53.555] [info] [graph][runtime.lifecycle] node_abort_payload node=62 before_objects=33 after_objects=33 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219441 after_releases=219441
[2026-11-02 11:58:53.555] [info] [graph][runtime.node] source_video_trace_summary node_id=39 node=MediaAvBoundReleaseExtractorNode generation=1 reason=abort steady_ns=11409272225900 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=3571 release_committed_count=3555 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:58:53.555] [info] [graph][runtime.lifecycle] node_abort_payload node=39 before_objects=33 after_objects=33 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219441 after_releases=219441
[2026-11-02 11:58:53.555] [info] [graph][runtime.node] source_video_trace_summary node_id=20 node=MediaAvBoundReleaseExtractorNode generation=1 reason=abort steady_ns=11409272360400 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=3571 release_committed_count=3555 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:58:53.555] [info] [graph][runtime.lifecycle] node_abort_payload node=20 before_objects=33 after_objects=33 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219441 after_releases=219441
[2026-11-02 11:58:53.555] [info] [graph][runtime.node] source_video_trace_summary node_id=61 node=ActivationReleaseSequencer generation=2 reason=abort steady_ns=11409272486700 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3446 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:58:53.555] [info] [graph][runtime.lifecycle] node_abort_payload node=61 before_objects=33 after_objects=33 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219441 after_releases=219441
[2026-11-02 11:58:53.556] [info] [graph][runtime.node] source_video_trace_summary node_id=38 node=ActivationReleaseSequencer generation=1 reason=abort steady_ns=11409272618200 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3555 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:58:53.556] [info] [graph][runtime.lifecycle] node_abort_payload node=38 before_objects=33 after_objects=33 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219441 after_releases=219441
[2026-11-02 11:58:53.556] [info] [graph][runtime.node] source_video_trace_summary node_id=19 node=ActivationReleaseSequencer generation=1 reason=abort steady_ns=11409272757100 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3555 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:58:53.556] [info] [graph][runtime.lifecycle] node_abort_payload node=19 before_objects=33 after_objects=33 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219441 after_releases=219441
[2026-11-02 11:58:53.556] [info] [graph][runtime.lifecycle] node_abort_payload node=60 before_objects=33 after_objects=33 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219441 after_releases=219441
[2026-11-02 11:58:53.556] [info] [graph][runtime.lifecycle] node_abort_payload node=37 before_objects=33 after_objects=33 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219441 after_releases=219441
[2026-11-02 11:58:53.556] [info] [graph][runtime.lifecycle] node_abort_payload node=18 before_objects=33 after_objects=33 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219441 after_releases=219441
[2026-11-02 11:58:53.556] [info] [graph][runtime.lifecycle] node_abort_payload node=58 before_objects=33 after_objects=33 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219441 after_releases=219441
[2026-11-02 11:58:53.556] [info] [graph][runtime.lifecycle] node_abort_payload node=35 before_objects=33 after_objects=33 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219441 after_releases=219441
[2026-11-02 11:58:53.556] [info] [graph][runtime.lifecycle] node_abort_payload node=16 before_objects=33 after_objects=33 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219441 after_releases=219441
[2026-11-02 11:58:53.556] [info] [graph][runtime.lifecycle] node_abort_payload node=57 before_objects=33 after_objects=33 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219441 after_releases=219441
[2026-11-02 11:58:53.556] [info] [graph][runtime.lifecycle] node_abort_payload node=56 before_objects=33 after_objects=33 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219441 after_releases=219441
[2026-11-02 11:58:53.556] [info] [graph][runtime.lifecycle] node_abort_payload node=34 before_objects=33 after_objects=33 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219441 after_releases=219441
[2026-11-02 11:58:53.556] [info] [graph][runtime.lifecycle] node_abort_payload node=15 before_objects=33 after_objects=33 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219441 after_releases=219441
[2026-11-02 11:58:53.556] [info] [graph][runtime.lifecycle] node_abort_payload node=59 before_objects=33 after_objects=33 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219441 after_releases=219441
[2026-11-02 11:58:53.556] [info] [graph][runtime.lifecycle] node_abort_payload node=55 before_objects=33 after_objects=33 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219441 after_releases=219441
[2026-11-02 11:58:53.557] [info] [graph][runtime.lifecycle] node_abort_payload node=54 before_objects=33 after_objects=33 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219441 after_releases=219441
[2026-11-02 11:58:53.557] [info] [graph][runtime.lifecycle] node_abort_payload node=36 before_objects=33 after_objects=33 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219441 after_releases=219441
[2026-11-02 11:58:53.557] [info] [graph][runtime.lifecycle] node_abort_payload node=33 before_objects=33 after_objects=33 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219441 after_releases=219441
[2026-11-02 11:58:53.557] [info] [graph][runtime.lifecycle] node_abort_payload node=17 before_objects=33 after_objects=33 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219441 after_releases=219441
[2026-11-02 11:58:53.557] [info] [graph][runtime.lifecycle] node_abort_payload node=14 before_objects=33 after_objects=33 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219441 after_releases=219441
[2026-11-02 11:58:53.557] [info] [graph][runtime.lifecycle] node_abort_payload node=53 before_objects=33 after_objects=33 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219441 after_releases=219441
[2026-11-02 11:58:53.557] [info] [graph][runtime.lifecycle] node_abort_payload node=32 before_objects=33 after_objects=33 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219441 after_releases=219441
[2026-11-02 11:58:53.557] [info] [graph][runtime.lifecycle] node_abort_payload node=13 before_objects=33 after_objects=33 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219441 after_releases=219441
[2026-11-02 11:58:53.557] [info] [graph][runtime.lifecycle] node_abort_payload node=52 before_objects=33 after_objects=33 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219441 after_releases=219441
[2026-11-02 11:58:53.557] [info] [graph][runtime.lifecycle] node_abort_payload node=51 before_objects=33 after_objects=33 before_bytes=147136 after_bytes=147136 before_reservations=219474 after_reservations=219474 before_releases=219441 after_releases=219441
[2026-11-02 11:58:53.557] [info] [graph][runtime.lifecycle] node_abort_payload node=50 before_objects=33 after_objects=32 before_bytes=147136 after_bytes=139261 before_reservations=219474 after_reservations=219474 before_releases=219441 after_releases=219442
[2026-11-02 11:58:53.557] [info] [graph][runtime.lifecycle] node_abort_payload node=31 before_objects=32 after_objects=32 before_bytes=139261 after_bytes=139261 before_reservations=219474 after_reservations=219474 before_releases=219442 after_releases=219442
[2026-11-02 11:58:53.557] [info] [graph][runtime.lifecycle] node_abort_payload node=30 before_objects=32 after_objects=32 before_bytes=139261 after_bytes=139261 before_reservations=219474 after_reservations=219474 before_releases=219442 after_releases=219442
[2026-11-02 11:58:53.557] [info] [graph][runtime.lifecycle] node_abort_payload node=12 before_objects=32 after_objects=32 before_bytes=139261 after_bytes=139261 before_reservations=219474 after_reservations=219474 before_releases=219442 after_releases=219442
[2026-11-02 11:58:53.557] [info] [graph][runtime.lifecycle] node_abort_payload node=11 before_objects=32 after_objects=32 before_bytes=139261 after_bytes=139261 before_reservations=219474 after_reservations=219474 before_releases=219442 after_releases=219442
[2026-11-02 11:58:53.557] [info] [graph][runtime.lifecycle] node_abort_payload node=49 before_objects=32 after_objects=32 before_bytes=139261 after_bytes=139261 before_reservations=219474 after_reservations=219474 before_releases=219442 after_releases=219442
[2026-11-02 11:58:53.558] [info] [graph][runtime.lifecycle] node_abort_payload node=29 before_objects=32 after_objects=32 before_bytes=139261 after_bytes=139261 before_reservations=219474 after_reservations=219474 before_releases=219442 after_releases=219442
[2026-11-02 11:58:53.558] [info] [graph][runtime.lifecycle] node_abort_payload node=10 before_objects=32 after_objects=32 before_bytes=139261 after_bytes=139261 before_reservations=219474 after_reservations=219474 before_releases=219442 after_releases=219442
[2026-11-02 11:58:53.558] [info] [graph][runtime.lifecycle] node_abort_payload node=69 before_objects=32 after_objects=32 before_bytes=139261 after_bytes=139261 before_reservations=219474 after_reservations=219474 before_releases=219442 after_releases=219442
[2026-11-02 11:58:53.558] [info] [graph][runtime.lifecycle] node_abort_payload node=48 before_objects=32 after_objects=32 before_bytes=139261 after_bytes=139261 before_reservations=219474 after_reservations=219474 before_releases=219442 after_releases=219442
[2026-11-02 11:58:53.558] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-11-02 11:58:53.558] [info] [graph][runtime.lifecycle] node_abort_payload node=63 before_objects=32 after_objects=32 before_bytes=139261 after_bytes=139261 before_reservations=219474 after_reservations=219474 before_releases=219442 after_releases=219442
[2026-11-02 11:58:53.558] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-11-02 11:58:53.558] [info] [graph][runtime.lifecycle] node_abort_payload node=40 before_objects=32 after_objects=32 before_bytes=139261 after_bytes=139261 before_reservations=219474 after_reservations=219474 before_releases=219442 after_releases=219442
[2026-11-02 11:58:53.558] [info] [graph][runtime.lifecycle] node_abort_payload node=28 before_objects=32 after_objects=32 before_bytes=139261 after_bytes=139261 before_reservations=219474 after_reservations=219474 before_releases=219442 after_releases=219442
[2026-11-02 11:58:53.558] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-11-02 11:58:53.558] [info] [graph][runtime.lifecycle] node_abort_payload node=21 before_objects=32 after_objects=32 before_bytes=139261 after_bytes=139261 before_reservations=219474 after_reservations=219474 before_releases=219442 after_releases=219442
[2026-11-02 11:58:53.558] [info] [graph][runtime.lifecycle] node_abort_payload node=9 before_objects=32 after_objects=32 before_bytes=139261 after_bytes=139261 before_reservations=219474 after_reservations=219474 before_releases=219442 after_releases=219442
[2026-11-02 11:58:53.558] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=10160 completion_waits=10160 completed_datagrams=10306 drained_datagrams=166 would_block=25 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-02 11:58:53.559] [info] [graph][runtime.lifecycle] node_abort_payload node=47 before_objects=32 after_objects=32 before_bytes=139261 after_bytes=139261 before_reservations=219474 after_reservations=219474 before_releases=219442 after_releases=219442
[2026-11-02 11:58:53.559] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=150939 completion_waits=150939 completed_datagrams=208811 drained_datagrams=57892 would_block=25 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-02 11:58:53.559] [info] [graph][runtime.lifecycle] node_abort_payload node=46 before_objects=32 after_objects=32 before_bytes=139261 after_bytes=139261 before_reservations=219474 after_reservations=219474 before_releases=219442 after_releases=219442
[2026-11-02 11:58:53.559] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=71510 completion_waits=71510 completed_datagrams=105379 drained_datagrams=33950 would_block=83 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-02 11:58:53.560] [info] [graph][runtime.lifecycle] node_abort_payload node=27 before_objects=32 after_objects=32 before_bytes=139261 after_bytes=139261 before_reservations=219474 after_reservations=219474 before_releases=219442 after_releases=219442
[2026-11-02 11:58:53.560] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=78659 completion_waits=78659 completed_datagrams=105438 drained_datagrams=26863 would_block=83 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-02 11:58:53.561] [info] [graph][runtime.lifecycle] node_abort_payload node=8 before_objects=32 after_objects=32 before_bytes=139261 after_bytes=139261 before_reservations=219474 after_reservations=219474 before_releases=219442 after_releases=219442
[2026-11-02 11:58:53.561] [info] [graph][runtime.lifecycle] node_abort_payload node=3 before_objects=32 after_objects=32 before_bytes=139261 after_bytes=139261 before_reservations=219474 after_reservations=219474 before_releases=219442 after_releases=219442
[2026-11-02 11:58:53.561] [info] [graph][runtime.node] codec_resolver.release slot=encoder_parameters bufferReferences=1 hasCredit=0 creditReferences=0
[2026-11-02 11:58:53.561] [info] [graph][runtime.lifecycle] node_abort_payload node=1 before_objects=32 after_objects=32 before_bytes=139261 after_bytes=139261 before_reservations=219474 after_reservations=219474 before_releases=219442 after_releases=219442
[2026-11-02 11:58:53.630] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=failed detail="Datagram GBRA reservation misses its immutable completion deadline sequence=171936 now_ns=286365319500 release_ns=286299019401 deadline_ns=286365657124 not_before_ns=286365532340 not_after_ns=286365442084 duration_ns=215040 rate=6250000 queue_wire_bytes=8656 average_queue_residence_ns=66236800" description="" error=6 native=0 stage=3 reason="Datagram GBRA reservation misses its immutable completion deadline sequence=171936 now_ns=286365319500 release_ns=286299019401 deadline_ns=286365657124 not_before_ns=286365532340 not_after_ns=286365442084 duration_ns=215040 rate=6250000 queue_wire_bytes=8656 average_queue_residence_ns=66236800"
[2026-11-02 11:58:53.630] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=retired detail="Datagram GBRA reservation misses its immutable completion deadline sequence=171936 now_ns=286365319500 release_ns=286299019401 deadline_ns=286365657124 not_before_ns=286365532340 not_after_ns=286365442084 duration_ns=215040 rate=6250000 queue_wire_bytes=8656 average_queue_residence_ns=66236800" description="" error=6 native=0 stage=3 reason="Datagram GBRA reservation misses its immutable completion deadline sequence=171936 now_ns=286365319500 release_ns=286299019401 deadline_ns=286365657124 not_before_ns=286365532340 not_after_ns=286365442084 duration_ns=215040 rate=6250000 queue_wire_bytes=8656 average_queue_residence_ns=66236800"
[CLI] final runtime report: queued=0, peakQueued=916, threads=81, processThreads=129, workers=0, workerProgress=556827, workerProcessCalls=2060684, workerWaits=1503850, workerWakeups=1482474, workerErrors=2, errors=2, stalledIntervals=0, cpuSamples=895, logicalProcessors=22, averageSystemMachineCpuPercent=7.152828, averageProcessMachineCpuPercent=1.567348, peakProcessMachineCpuPercent=5.567929, averageProcessSingleCoreCpuPercent=34.481652, peakProcessSingleCoreCpuPercent=122.494432, initialWorkingSetBytes=209342464, workingSetBytes=261853184, peakWorkingSetBytes=271364096, totalPushed=479205, totalPopped=479155, droppedBuffers=0, encodedPacketsPushed=94168, encodedPacketsPopped=94163, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=15849484, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=742, graphPayloadReservations=219474, graphPayloadReleases=219474, graphPayloadPressureFailures=0, lastInputReceivedAtNs=11409049052600
~~~

## cli-error.log

~~~text
[in @ 00000212599ce380] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000212599ce380] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 31923.064278
[in @ 00000212599ce680] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000212599ce680] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 47247.284011
[in @ 0000021272e66ec0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000021272e66ec0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 43460.778222
[in @ 0000021259a16500] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000021273c4c280] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000021273c4c280] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 31923.064278
[in @ 0000021259a16500] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 43460.778222
[in @ 0000021278383400] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000021278383400] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 47247.284011
[aac @ 000002126d7c45c0] Qavg: 12670.907
[aac @ 000002126d7c45c0] 2 frames left in the queue on closing
[CLI] realtime video runtime failed: IoFailure: Datagram GBRA reservation misses its immutable completion deadline sequence=171936 now_ns=286365319500 release_ns=286299019401 deadline_ns=286365657124 not_before_ns=286365532340 not_after_ns=286365442084 duration_ns=215040 rate=6250000 queue_wire_bytes=8656 average_queue_residence_ns=66236800

~~~

## monitor.csv

~~~text
"Time","CpuPercent","WorkingSet","PrivateBytes"
"2026-11-02T11:54:08.0143759+08:00","0.9092157038420382","142737408","569262080"
"2026-11-02T11:54:09.0459154+08:00","0.13770239001555884","145203200","571723776"
"2026-11-02T11:54:10.0687177+08:00","0","149565440","575377408"
"2026-11-02T11:54:11.0743199+08:00","0","152428544","578162688"
"2026-11-02T11:54:12.0851226+08:00","0.7729005868306446","174608384","632836096"
"2026-11-02T11:54:13.1004524+08:00","3.6374208835216084","254754816","1256415232"
"2026-11-02T11:54:14.1292816+08:00","1.7258143351862307","255455232","1257000960"
"2026-11-02T11:54:15.1438579+08:00","1.6100540957567482","256905216","1258336256"
"2026-11-02T11:54:16.1551712+08:00","2.5984439333398552","259538944","1254658048"
"2026-11-02T11:54:17.1630303+08:00","2.536880583623427","259780608","1254662144"
"2026-11-02T11:54:18.1838374+08:00","1.7393767949088341","260145152","1254670336"
"2026-11-02T11:54:19.2095331+08:00","2.0080605689475846","262221824","1256685568"
"2026-11-02T11:54:20.2285105+08:00","2.1607000758353867","262545408","1256685568"
"2026-11-02T11:54:21.2444432+08:00","1.6778133576618357","262631424","1256685568"
"2026-11-02T11:54:22.2608725+08:00","1.8167430918125924","262688768","1256685568"
"2026-11-02T11:54:23.2746177+08:00","2.0317325210606083","262750208","1256685568"
"2026-11-02T11:54:24.2875095+08:00","2.243800643590236","263774208","1257426944"
"2026-11-02T11:54:25.2987150+08:00","1.6856568269708327","263770112","1257426944"
"2026-11-02T11:54:26.3152872+08:00","2.026082447374708","263839744","1257426944"
"2026-11-02T11:54:27.3383568+08:00","1.943793817777758","263962624","1257435136"
"2026-11-02T11:54:28.3510870+08:00","1.893508889498542","264740864","1258213376"
"2026-11-02T11:54:29.3626427+08:00","2.948878193711474","264024064","1257435136"
"2026-11-02T11:54:30.3807190+08:00","2.371897594780202","264089600","1257435136"
"2026-11-02T11:54:31.4028872+08:00","2.1539552350137146","264540160","1257885696"
"2026-11-02T11:54:32.4166472+08:00","2.1017615788567494","264605696","1257885696"
"2026-11-02T11:54:33.4334277+08:00","2.235219177322217","264617984","1257885696"
"2026-11-02T11:54:34.4538682+08:00","2.227202147236681","264798208","1257885696"
"2026-11-02T11:54:35.4742175+08:00","2.2274012171393394","265043968","1257885696"
"2026-11-02T11:54:36.4944795+08:00","2.0883673195530346","265043968","1257885696"
"2026-11-02T11:54:37.5144240+08:00","2.2979191514832427","265048064","1257889792"
"2026-11-02T11:54:38.5342372+08:00","2.2285721274516477","265048064","1257889792"
"2026-11-02T11:54:39.5424894+08:00","2.5358914979983993","265056256","1257889792"
"2026-11-02T11:54:40.5561427+08:00","2.3121810978171724","267288576","1260056576"
"2026-11-02T11:54:41.5671955+08:00","1.8264040306212583","267288576","1260056576"
"2026-11-02T11:54:42.5864791+08:00","1.5329394096010176","267345920","1260056576"
"2026-11-02T11:54:43.6027044+08:00","1.6773302677520965","267341824","1260056576"
"2026-11-02T11:54:44.6195450+08:00","1.53662235752585","267415552","1260068864"
"2026-11-02T11:54:45.6325218+08:00","2.103386591066862","267411456","1260068864"
"2026-11-02T11:54:46.6508838+08:00","1.7435530605208973","267411456","1260068864"
"2026-11-02T11:54:47.6606668+08:00","1.5473621560275819","267411456","1260068864"
"2026-11-02T11:54:48.6793905+08:00","1.7429340083264793","268877824","1261535232"
"2026-11-02T11:54:49.6957139+08:00","1.9566964252091053","268169216","1260756992"
"2026-11-02T11:54:50.7090401+08:00","1.6821290661836776","268226560","1260756992"
"2026-11-02T11:54:51.7222039+08:00","2.243198259479141","268226560","1260756992"
"2026-11-02T11:54:52.7423392+08:00","1.6012804647312249","266883072","1259401216"
"2026-11-02T11:54:53.7671345+08:00","2.425650717314428","266940416","1259401216"
"2026-11-02T11:54:54.7801365+08:00","1.4723339862382039","266940416","1259401216"
"2026-11-02T11:54:55.7931247+08:00","1.262017751943301","266940416","1259401216"
"2026-11-02T11:54:56.8177798+08:00","1.8714722996680897","267767808","1260224512"
"2026-11-02T11:54:57.8252311+08:00","2.678902331421515","267763712","1260224512"
"2026-11-02T11:54:58.8433413+08:00","1.9532623910814015","267784192","1260257280"
"2026-11-02T11:54:59.8553525+08:00","1.9650339478815682","267841536","1260257280"
"2026-11-02T11:55:00.8775839+08:00","2.2233001967336095","267841536","1260257280"
"2026-11-02T11:55:01.8964351+08:00","2.2306763467788744","267894784","1260257280"
"2026-11-02T11:55:02.9188304+08:00","1.5977408417983996","267952128","1260257280"
"2026-11-02T11:55:03.9320926+08:00","1.261676485029335","267960320","1260257280"
"2026-11-02T11:55:04.9496237+08:00","1.8147758914601324","268148736","1260257280"
"2026-11-02T11:55:05.9627471+08:00","2.3133904517455623","268148736","1260257280"
"2026-11-02T11:55:06.9815612+08:00","1.254801136840461","268869632","1260933120"
"2026-11-02T11:55:07.9942557+08:00","2.1039729337740236","268091392","1260154880"
"2026-11-02T11:55:09.0087991+08:00","1.6801109292568996","268091392","1260154880"
"2026-11-02T11:55:10.0287915+08:00","1.671135446249849","268091392","1260154880"
"2026-11-02T11:55:11.0435362+08:00","1.8897498418702126","268148736","1260154880"
"2026-11-02T11:55:12.0631577+08:00","1.8807112603683194","268206080","1260154880"
"2026-11-02T11:55:13.0723340+08:00","2.0409308967215054","268296192","1260154880"
"2026-11-02T11:55:14.0867677+08:00","1.8203169996135862","269844480","1261740032"
"2026-11-02T11:55:15.0955504+08:00","1.5488965066510358","269844480","1261740032"
"2026-11-02T11:55:16.1053540+08:00","1.6879970070867787","270622720","1262518272"
"2026-11-02T11:55:17.1194915+08:00","2.100979224396907","269905920","1261740032"
"2026-11-02T11:55:18.1332904+08:00","1.7514007776277738","268693504","1260527616"
"2026-11-02T11:55:19.1467444+08:00","2.3827156706399375","268701696","1260527616"
"2026-11-02T11:55:20.1626671+08:00","1.9574681849675803","268783616","1260527616"
"2026-11-02T11:55:21.1747831+08:00","1.964830477570124","268845056","1260527616"
"2026-11-02T11:55:22.1944608+08:00","1.8109554706265611","269443072","1261125632"
"2026-11-02T11:55:23.2004896+08:00","2.117913342224217","269443072","1261125632"
"2026-11-02T11:55:24.2199554+08:00","1.8809984958432506","269430784","1261125632"
"2026-11-02T11:55:25.2360015+08:00","2.0970326230097416","269496320","1261125632"
"2026-11-02T11:55:26.2438891+08:00","2.043540461167585","269500416","1261125632"
"2026-11-02T11:55:27.2614510+08:00","1.675127040964736","269504512","1261125632"
"2026-11-02T11:55:28.2790847+08:00","1.4656327445988402","269500416","1261125632"
"2026-11-02T11:55:29.3025662+08:00","1.4572586536515537","269500416","1261125632"
"2026-11-02T11:55:30.3212967+08:00","3.0675433787444275","270229504","1261854720"
"2026-11-02T11:55:31.3366282+08:00","1.6088565431809485","270225408","1261854720"
"2026-11-02T11:55:32.3519630+08:00","2.588152114052339","270225408","1261854720"
"2026-11-02T11:55:33.3590355+08:00","1.9041465598193141","268886016","1260449792"
"2026-11-02T11:55:34.3729567+08:00","1.4009516177929267","268886016","1260449792"
"2026-11-02T11:55:35.3905302+08:00","2.024088766962869","268886016","1260449792"
"2026-11-02T11:55:36.3965110+08:00","1.4120095984481469","268886016","1260449792"
"2026-11-02T11:55:37.4087744+08:00","1.5435705765910337","268886016","1260449792"
"2026-11-02T11:55:38.4330906+08:00","2.2881118154726052","270721024","1262288896"
"2026-11-02T11:55:39.4448585+08:00","2.3164897799188924","270741504","1262288896"
"2026-11-02T11:55:40.4553651+08:00","1.8273912402857229","271511552","1263058944"
"2026-11-02T11:55:41.4677450+08:00","1.7538556245715484","270741504","1262288896"
"2026-11-02T11:55:42.4905044+08:00","1.805498838818699","270782464","1262288896"
"2026-11-02T11:55:43.4981472+08:00","2.1145209574085366","270786560","1262288896"
"2026-11-02T11:55:44.5136103+08:00","1.7485304801505654","270790656","1262288896"
"2026-11-02T11:55:45.5306174+08:00","2.095051075043447","270790656","1262288896"
"2026-11-02T11:55:46.5529329+08:00","2.501006960980423","270852096","1262288896"
"2026-11-02T11:55:47.5687962+08:00","1.5381006479907287","270852096","1262288896"
"2026-11-02T11:55:48.5860978+08:00","1.8850001183165703","270856192","1262288896"
"2026-11-02T11:55:49.5942355+08:00","2.3248312209730875","270909440","1262288896"
"2026-11-02T11:55:50.6149018+08:00","1.322108722686169","270942208","1262288896"
"2026-11-02T11:55:51.6345823+08:00","2.15921020893755","270942208","1262288896"
"2026-11-02T11:55:52.6537350+08:00","1.0453202048043528","270942208","1262288896"
"2026-11-02T11:55:53.6667514+08:00","2.103304367216383","269774848","1261121536"
"2026-11-02T11:55:54.6782599+08:00","2.5277278261311515","271290368","1262694400"
"2026-11-02T11:55:55.6909360+08:00","1.9637437514683753","271360000","1262694400"
"2026-11-02T11:55:56.6982286+08:00","1.692204881228607","271364096","1262694400"
"2026-11-02T11:55:57.7204486+08:00","1.6674937435634745","271372288","1262694400"
"2026-11-02T11:55:58.7298468+08:00","1.6886749496338063","271364096","1262694400"
"2026-11-02T11:55:59.7480903+08:00","2.4412583576968125","271364096","1262694400"
"2026-11-02T11:56:00.7629659+08:00","2.4493597585215907","271364096","1262694400"
"2026-11-02T11:56:01.7799597+08:00","1.7458987280140563","271360000","1262694400"
"2026-11-02T11:56:02.7979134+08:00","1.883792589352184","270184448","1261510656"
"2026-11-02T11:56:03.8135489+08:00","1.468516286332324","270233600","1261510656"
"2026-11-02T11:56:04.8256965+08:00","1.9647691341029345","270233600","1261510656"
"2026-11-02T11:56:05.8409169+08:00","1.5390746679243248","270225408","1261510656"
"2026-11-02T11:56:06.8585701+08:00","2.2333023398612344","246370304","1242742784"
"2026-11-02T11:56:07.8770288+08:00","1.1855035099963933","246362112","1242742784"
"2026-11-02T11:56:08.8939063+08:00","1.2571908522994075","246366208","1242742784"
"2026-11-02T11:56:09.9153689+08:00","0.8343650832372397","246366208","1242820608"
"2026-11-02T11:56:10.9380586+08:00","1.1805989281366223","247128064","1243582464"
"2026-11-02T11:56:11.9537632+08:00","1.0488688434520321","246366208","1242820608"
"2026-11-02T11:56:12.9767105+08:00","0.9720131054827378","246366208","1242820608"
"2026-11-02T11:56:13.9933285+08:00","0.9082029381197801","246366208","1242820608"
"2026-11-02T11:56:15.0116887+08:00","1.046133685400224","246366208","1242820608"
"2026-11-02T11:56:16.0285054+08:00","0.8381773502271623","246366208","1242820608"
"2026-11-02T11:56:17.0356825+08:00","0.9167160914852558","246366208","1242820608"
"2026-11-02T11:56:18.0520554+08:00","0.9783005645055883","246366208","1242820608"
"2026-11-02T11:56:19.0634777+08:00","0.9830890438328104","246366208","1242820608"
"2026-11-02T11:56:20.0868732+08:00","1.0409865092145796","246366208","1243295744"
"2026-11-02T11:56:21.1060046+08:00","0.9059631118670807","246345728","1243262976"
"2026-11-02T11:56:22.1252102+08:00","0.9755815527487112","246345728","1243262976"
"2026-11-02T11:56:23.1452832+08:00","0.9051268434175342","246345728","1243262976"
"2026-11-02T11:56:24.1571365+08:00","0.9826702959986213","246345728","1243262976"
"2026-11-02T11:56:25.1775246+08:00","0.9744509778369445","246345728","1243262976"
"2026-11-02T11:56:26.1970096+08:00","1.0449794838481283","246345728","1243262976"
"2026-11-02T11:56:27.2139596+08:00","0.9079064403809967","246345728","1243262976"
"2026-11-02T11:56:28.2339490+08:00","1.3926169678376514","246353920","1243262976"
"2026-11-02T11:56:29.2403199+08:00","0.9880235823772149","246366208","1243295744"
"2026-11-02T11:56:30.2636262+08:00","1.041077250370597","246366208","1243295744"
"2026-11-02T11:56:31.2783474+08:00","0.9099006254579627","246366208","1243295744"
"2026-11-02T11:56:32.2983010+08:00","0.9052328013210155","246370304","1243295744"
"2026-11-02T11:56:33.3214463+08:00","1.1800732150520201","246366208","1243295744"
"2026-11-02T11:56:34.3375242+08:00","0.8387867970287782","246366208","1243295744"
"2026-11-02T11:56:35.3541292+08:00","0.9780772097502785","246366208","1243295744"
"2026-11-02T11:56:36.3685295+08:00","1.120231960069054","246370304","1243295744"
"2026-11-02T11:56:37.3886030+08:00","1.114001722781384","246366208","1243295744"
"2026-11-02T11:56:38.4065717+08:00","0.9069978817084009","246366208","1243295744"
"2026-11-02T11:56:39.4202203+08:00","1.0509962812466855","246366208","1243295744"
"2026-11-02T11:56:40.4462510+08:00","1.1767546172218468","246366208","1243295744"
"2026-11-02T11:56:41.4639234+08:00","0.9072619583133577","246366208","1243295744"
"2026-11-02T11:56:42.4846753+08:00","0.9741036796680778","246366208","1243295744"
"2026-11-02T11:56:43.4936000+08:00","1.3374950758781288","246366208","1243295744"
"2026-11-02T11:56:44.5162563+08:00","0.8333911669763608","246366208","1243295744"
"2026-11-02T11:56:45.5240346+08:00","1.0571183256187486","246370304","1243295744"
"2026-11-02T11:56:46.5304990+08:00","0.7762321250508215","246366208","1243295744"
"2026-11-02T11:56:47.5511003+08:00","0.8350692158365145","246366208","1243295744"
"2026-11-02T11:56:48.5616800+08:00","0.7730711392678874","246366208","1243295744"
"2026-11-02T11:56:49.5726720+08:00","1.194259067961333","246366208","1243295744"
"2026-11-02T11:56:50.5848334+08:00","0.8420324340295205","246366208","1243295744"
"2026-11-02T11:56:51.5930122+08:00","0.9862518253886928","246366208","1243295744"
"2026-11-02T11:56:52.6136475+08:00","0.7654546143955634","246366208","1243295744"
"2026-11-02T11:56:53.6237232+08:00","0.914085404237974","246374400","1243295744"
"2026-11-02T11:56:54.6361664+08:00","1.473146614770362","246366208","1243295744"
"2026-11-02T11:56:55.6480557+08:00","1.1932000502785864","246366208","1243295744"
"2026-11-02T11:56:56.6575679+08:00","1.1256561697457803","246366208","1243295744"
"2026-11-02T11:56:57.6688494+08:00","0.9129954958589223","248463360","1244844032"
"2026-11-02T11:56:58.6813476+08:00","1.4029205636657383","253177856","1249103872"
"2026-11-02T11:56:59.6963175+08:00","2.0292809578974613","254099456","1248833536"
"2026-11-02T11:57:00.7058263+08:00","2.0402586791804995","254910464","1249652736"
"2026-11-02T11:57:01.7236325+08:00","0.9769228973238537","255303680","1250574336"
"2026-11-02T11:57:02.7327470+08:00","1.0557185622552339","255938560","1251037184"
"2026-11-02T11:57:03.7501110+08:00","1.4660212792346423","256196608","1251397632"
"2026-11-02T11:57:04.7718082+08:00","0.9036879562217206","256614400","1251475456"
"2026-11-02T11:57:05.7864174+08:00","0.9800011490317473","256946176","1251774464"
"2026-11-02T11:57:06.8009771+08:00","1.1200559576372255","257802240","1252302848"
"2026-11-02T11:57:07.8129259+08:00","1.0527616704431182","257904640","1252524032"
"2026-11-02T11:57:08.8237144+08:00","1.2647641825259102","258482176","1252966400"
"2026-11-02T11:57:09.8430541+08:00","1.3935045848352081","259477504","1253871616"
"2026-11-02T11:57:10.8566409+08:00","1.8919086518921089","258949120","1253343232"
"2026-11-02T11:57:11.8691408+08:00","1.1924804769228756","259039232","1253404672"
"2026-11-02T11:57:12.8860440+08:00","2.1651073036789987","259289088","1253437440"
"2026-11-02T11:57:13.9014960+08:00","1.6086656260194743","259772416","1253933056"
"2026-11-02T11:57:14.9217217+08:00","1.3922944162792072","260136960","1254141952"
"2026-11-02T11:57:15.9361405+08:00","1.4702776335841496","260157440","1254260736"
"2026-11-02T11:57:16.9512749+08:00","1.6791327872895003","260325376","1254273024"
"2026-11-02T11:57:17.9620766+08:00","1.5458027029436139","260435968","1254293504"
"2026-11-02T11:57:18.9766274+08:00","1.6800986747489177","260497408","1254293504"
"2026-11-02T11:57:19.9944400+08:00","1.1164762907863748","260591616","1254293504"
"2026-11-02T11:57:21.0143651+08:00","0.9748933346362215","260591616","1254293504"
"2026-11-02T11:57:22.0302518+08:00","1.538065219280851","260603904","1254293504"
"2026-11-02T11:57:23.0443114+08:00","1.260684372899868","260608000","1254293504"
"2026-11-02T11:57:24.0621963+08:00","1.5350458583283826","260608000","1254293504"
"2026-11-02T11:57:25.0778018+08:00","1.32869683965065","260636672","1254293504"
"2026-11-02T11:57:26.0959389+08:00","1.5346656162514853","260694016","1254354944"
"2026-11-02T11:57:27.1056845+08:00","1.1957332259099358","260694016","1254354944"
"2026-11-02T11:57:28.1198038+08:00","1.7508474415368902","260694016","1254354944"
"2026-11-02T11:57:29.1275648+08:00","1.2685637675094503","260722688","1254371328"
"2026-11-02T11:57:30.1467468+08:00","1.4634062147165792","260730880","1254371328"
"2026-11-02T11:57:31.1583910+08:00","1.333899624177965","261496832","1254727680"
"2026-11-02T11:57:32.1759561+08:00","1.046951108180606","261496832","1254727680"
"2026-11-02T11:57:33.1899149+08:00","1.3308546838212936","261496832","1254727680"
"2026-11-02T11:57:34.2092838+08:00","1.1844449675052513","261496832","1254727680"
"2026-11-02T11:57:35.2166360+08:00","0.9870611111170273","261496832","1254727680"
"2026-11-02T11:57:36.2284105+08:00","1.614512648097701","261496832","1254727680"
"2026-11-02T11:57:37.2413787+08:00","1.3321561507871797","261496832","1254727680"
"2026-11-02T11:57:38.2624528+08:00","1.4606944517809948","261496832","1254727680"
"2026-11-02T11:57:39.2790108+08:00","1.1877200943147008","261500928","1254727680"
"2026-11-02T11:57:40.3001890+08:00","1.251896183162832","260976640","1253257216"
"2026-11-02T11:57:41.3142001+08:00","1.8210756362439318","260976640","1253257216"
"2026-11-02T11:57:42.3358373+08:00","1.6684449768914584","261009408","1253257216"
"2026-11-02T11:57:43.3471244+08:00","1.545060744866616","261009408","1253257216"
"2026-11-02T11:57:44.3595578+08:00","1.9642144990834596","261009408","1253257216"
"2026-11-02T11:57:45.3808150+08:00","1.1822549340522286","261021696","1253257216"
"2026-11-02T11:57:46.4016527+08:00","1.5306056976539955","261021696","1253257216"
"2026-11-02T11:57:47.4148998+08:00","1.4018836525212313","261304320","1253552128"
"2026-11-02T11:57:48.4240983+08:00","1.2667568282246662","261312512","1253552128"
"2026-11-02T11:57:49.4367939+08:00","1.472779453892436","261312512","1253552128"
"2026-11-02T11:57:50.4528306+08:00","1.5378381509250603","261308416","1253552128"
"2026-11-02T11:57:51.4626848+08:00","1.898901481385765","261308416","1253552128"
"2026-11-02T11:57:52.4686473+08:00","1.4120352850673312","261308416","1253552128"
"2026-11-02T11:57:53.4864029+08:00","1.325889848389749","261308416","1253552128"
"2026-11-02T11:57:54.4970620+08:00","1.4054734632622863","262070272","1254313984"
"2026-11-02T11:57:55.5071936+08:00","1.4765177851353948","261550080","1253797888"
"2026-11-02T11:57:56.5188737+08:00","1.2636495379409864","261550080","1253797888"
"2026-11-02T11:57:57.5355657+08:00","1.3272769119672607","261550080","1253797888"
"2026-11-02T11:57:58.5481626+08:00","1.7534797724723254","261550080","1253797888"
"2026-11-02T11:57:59.5584962+08:00","1.4762225790840497","261566464","1253814272"
"2026-11-02T11:58:00.5749746+08:00","1.397427181388749","261566464","1253814272"
"2026-11-02T11:58:01.5976283+08:00","1.597337131105796","261566464","1253814272"
"2026-11-02T11:58:02.6164458+08:00","1.5336407158298717","261566464","1253814272"
"2026-11-02T11:58:03.6275814+08:00","1.0536083479712406","261566464","1253814272"
"2026-11-02T11:58:04.6443034+08:00","1.6066562219296203","261701632","1253814272"
"2026-11-02T11:58:05.6619657+08:00","1.395801480957431","261701632","1253814272"
"2026-11-02T11:58:06.6783812+08:00","1.5372650259662513","261709824","1253814272"
"2026-11-02T11:58:07.6989174+08:00","1.2526837273475364","261701632","1253814272"
"2026-11-02T11:58:08.7131561+08:00","1.0503847951087937","261701632","1253814272"
"2026-11-02T11:58:09.7355985+08:00","1.8060586191367933","261701632","1253814272"
"2026-11-02T11:58:10.7527138+08:00","1.3965521366698008","261701632","1253814272"
"2026-11-02T11:58:11.7715509+08:00","1.6730304133462106","261701632","1253814272"
"2026-11-02T11:58:12.7831256+08:00","1.4042013362478771","261713920","1253814272"
"2026-11-02T11:58:13.8037805+08:00","1.8092216175035354","261713920","1253814272"
"2026-11-02T11:58:14.8145182+08:00","1.194559541646031","261775360","1253814272"
"2026-11-02T11:58:15.8340834+08:00","1.184216922700347","261775360","1253814272"
"2026-11-02T11:58:16.8482714+08:00","1.2605247655356708","261775360","1253814272"
"2026-11-02T11:58:17.8600535+08:00","1.9654788947505233","261775360","1253814272"
"2026-11-02T11:58:18.8774583+08:00","1.605577963926185","261775360","1253814272"
"2026-11-02T11:58:19.8989076+08:00","1.5296892366561905","261775360","1253814272"
"2026-11-02T11:58:20.9070793+08:00","1.2680469913102015","261783552","1253814272"
"2026-11-02T11:58:21.9228548+08:00","1.3983941780979612","261787648","1253814272"
"2026-11-02T11:58:22.9388022+08:00","1.6777890809558196","261787648","1253814272"
"2026-11-02T11:58:23.9521588+08:00","1.3316455610806879","261787648","1253814272"
"2026-11-02T11:58:24.9658724+08:00","0.7706811864810732","261787648","1253814272"
"2026-11-02T11:58:25.9813115+08:00","1.3289145731948062","262549504","1254576128"
"2026-11-02T11:58:27.0030228+08:00","1.5988104734407138","261787648","1253814272"
"2026-11-02T11:58:28.0156125+08:00","1.4729334820680804","261849088","1253814272"
"2026-11-02T11:58:29.0376897+08:00","1.1118178121609956","261849088","1253814272"
"2026-11-02T11:58:30.0516694+08:00","1.4008707920430217","262619136","1255411712"
"2026-11-02T11:58:31.0631872+08:00","1.8957784394536965","261849088","1254641664"
"2026-11-02T11:58:32.0706843+08:00","1.1984018253118185","261849088","1254641664"
"2026-11-02T11:58:33.0760592+08:00","1.4835035892852233","261849088","1254641664"
"2026-11-02T11:58:34.0871501+08:00","1.7560915460896565","261849088","1254641664"
"2026-11-02T11:58:35.0979499+08:00","1.2647500433904824","261849088","1254641664"
"2026-11-02T11:58:36.1107607+08:00","1.1219900462787682","261853184","1254641664"
"2026-11-02T11:58:37.1256751+08:00","1.119664511966365","261853184","1254641664"
"2026-11-02T11:58:38.1418351+08:00","1.3279717939909248","261853184","1254641664"
"2026-11-02T11:58:39.1527509+08:00","1.896907374841343","261853184","1254641664"
"2026-11-02T11:58:40.1685437+08:00","1.3983703620015275","261861376","1254641664"
"2026-11-02T11:58:41.1745120+08:00","1.4120271438518943","261853184","1254641664"
"2026-11-02T11:58:42.1954530+08:00","2.0869784034354755","261853184","1254641664"
"2026-11-02T11:58:43.2134722+08:00","1.5348433506951542","261853184","1254641664"
"2026-11-02T11:58:44.2346485+08:00","1.8778477686601582","262615040","1255403520"
"2026-11-02T11:58:45.2428664+08:00","1.4793203658924057","261853184","1254641664"
"2026-11-02T11:58:46.2642899+08:00","1.59926096009415","261853184","1254641664"
"2026-11-02T11:58:47.2793269+08:00","1.3994115933257067","261853184","1254641664"
"2026-11-02T11:58:48.2949344+08:00","1.7482818724932436","261861376","1254641664"
"2026-11-02T11:58:49.3168650+08:00","1.5289687969026469","261853184","1254641664"
"2026-11-02T11:58:50.3279405+08:00","0.8429367809552574","261853184","1254641664"
"2026-11-02T11:58:51.3341884+08:00","2.046870449030593","261857280","1254641664"
"2026-11-02T11:58:52.3508834+08:00","1.257416522073081","261853184","1254641664"
"2026-11-02T11:58:53.3594924+08:00","0.7745816267750931","259514368","1251594240"

~~~

## vlc.log

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
file debug: opening logfile `D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r380-vlc.log'
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
main debug: stream filter added to 0315bc38
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
qt debug: IM: Setting an input
main debug: using timeshift granularity of 50 MiB
main debug: using timeshift path: C:\Users\ROG\AppData\Local\Temp
main debug: looking for meta fetcher module matching "any": 1 candidates
main debug: `udp://@127.0.0.1:62024' gives access `udp' demux `any' path `@127.0.0.1:62024'
main debug: creating demux: access='udp' demux='any' location='@127.0.0.1:62024' file='\\@127.0.0.1:62024'
main debug: looking for access_demux module matching "udp": 15 candidates
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\meta\fetcher
main debug: no access_demux modules matched
main debug: creating access: udp://@127.0.0.1:62024
main debug:  (path: \\@127.0.0.1:62024)
main debug: looking for access module matching "udp": 27 candidates
lua debug: Trying Lua scripts in D:\VideoLAN\VLC\lua\meta\fetcher
main debug: no meta fetcher modules matched
main debug: looking for art finder module matching "any": 2 candidates
udp debug: opening server=:0 local=127.0.0.1:62024
main debug: net: opening 127.0.0.1 datagram port 62024
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
main debug: resized to 81706752x51648968
main debug: resized to 81706752x51648968
main debug: resized to 81706496x51648968
main debug: resized to 81706496x51648968
main debug: resized to 81705984x51648968
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
main debug: Buffering 39%
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
direct3d11 debug: D3D11 pool succeed with 48 surfaces (1280x736) context 0x0b2413d0
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
d3d11va debug: - 'Unknown decoder 0x15df9b21-0x06c4-0x47f1-0x841ea67c97d7f312' is supported
d3d11va debug: - 'Unknown decoder 0x4008018f-0xf537-0x4b36-0x98cf61af8a2c1a33' is supported
d3d11va debug: - 'Unknown decoder 0x0dabeffa-0x4458-0x4602-0xbc030795659d617c' is supported
d3d11va debug: - 'Unknown decoder 0x9798634d-0xfe9d-0x48e5-0xb4dadbec45b3df01' is supported
d3d11va debug: - 'MPEG-4 Part 2 variable-length decoder, Simple Profile' is supported
d3d11va debug: - 'MPEG-4 Part 2 variable-length decoder, Simple&Advanced Profile, no GMC' is supported
qt debug: Logical video size: 1280x720
d3d11va debug: - 'MPEG-4 Part 2 nVidia bitstream decoder' is supported
main debug: resized to 1280x720
d3d11va debug: - 'Unknown decoder 0x33fcfe41-0xde46-0x4a49-0x861bac71110649d5' is supported
main debug: VoutDisplayEvent 'resize' 1280x720
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
main debug: Buffering 46%
main debug: Received first picture
wasapi debug: buffer size    : 96000 frames
wasapi debug: maximum latency: 000 ns
wasapi debug: default period : 10000000 ns
wasapi debug: minimum period : 3000000 ns
main debug: using aout stream module "wasapi"
main debug: Buffering 53%
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
main debug: Buffering 59%
main debug: Buffering 66%
main debug: Buffering 73%
main debug: Buffering 79%
main debug: Buffering 86%
main debug: Buffering 93%
main debug: Buffering 99%
main debug: Stream buffering done (1066 ms in 931 ms)
main debug: Decoder wait done in 0 ms
main debug: inserting 9594 zeroes
mmdevice debug: state changed: 1
main warning: playback too late (78076): up-sampling
main debug: auto hiding mouse cursor
main warning: picture is too late to be displayed (missing 29 ms)
main debug: resampling stopped (drift: -13639 us)
main debug: accepted socket 3324 (from socket 1172)
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
main debug: Filter 'd3d11_filters' (04f38388) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (09118ec8) appended to chain
main debug: using video converter module "chain"
main debug: Filter 'chain' (090faaa8) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: RV24 -> 1280x720 (1280x720) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (09023050) appended to chain
main debug: using video converter module "chain"
main debug: removing module "png"
main debug: removing module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 04f38388 removed from chain
main debug: removing module "swscale"
main debug: Filter 09118ec8 removed from chain
main debug: Filter 090faaa8 removed from chain
main debug: removing module "swscale"
main debug: Filter 09023050 removed from chain
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r380-native-2026-11-02-11h54m39s335.png)
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (090faaa8) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: YUVA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (09118ec8) appended to chain
main debug: using video converter module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 090faaa8 removed from chain
main debug: removing module "swscale"
main debug: Filter 09118ec8 removed from chain
main error: read error: No error
main debug: accepted socket 3172 (from socket 1172)
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
main debug: Filter 'd3d11_filters' (09023050) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (08fa8f88) appended to chain
main debug: using video converter module "chain"
main debug: Filter 'chain' (09118ec8) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: RV24 -> 1280x720 (1280x720) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (0919d560) appended to chain
main debug: using video converter module "chain"
main debug: removing module "png"
main debug: removing module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 09023050 removed from chain
main debug: removing module "swscale"
main debug: Filter 08fa8f88 removed from chain
main debug: Filter 09118ec8 removed from chain
main debug: removing module "swscale"
main debug: Filter 0919d560 removed from chain
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r380-native-2026-11-02-11h56m42s265.png)
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (090faaa8) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: YUVA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (09118ec8) appended to chain
main debug: using video converter module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 090faaa8 removed from chain
main debug: removing module "swscale"
main debug: Filter 09118ec8 removed from chain
main debug: accepted socket 3172 (from socket 1172)
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
main debug: Filter 'd3d11_filters' (09023050) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (0919d560) appended to chain
main debug: using video converter module "chain"
main debug: Filter 'chain' (09118ec8) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: RV24 -> 1280x720 (1280x720) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (08fa8f88) appended to chain
main debug: using video converter module "chain"
main debug: removing module "png"
main debug: removing module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 09023050 removed from chain
main debug: removing module "swscale"
main debug: Filter 0919d560 removed from chain
main debug: Filter 09118ec8 removed from chain
main debug: removing module "swscale"
main debug: Filter 08fa8f88 removed from chain
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r380-native-2026-11-02-11h57m16s744.png)
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (090faaa8) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: YUVA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (09118ec8) appended to chain
main debug: using video converter module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 090faaa8 removed from chain
main debug: removing module "swscale"
main debug: Filter 09118ec8 removed from chain
main debug: accepted socket 2736 (from socket 1172)
main debug: exiting
main debug: exiting
main debug: no exit handler
main debug: removing all interfaces
main debug: removing module "qt"
main debug: deactivating the playlist
main debug: incoming request - stopping current input
prefetch debug: end of stream
main debug: incoming request - stopping current input
ts debug: Can't read TS packet at 201216588
main debug: EOF reached
main debug: incoming request - stopping current input
main warning: buffer too late (-92381609 us): dropped
main warning: picture is too late to be displayed (missing 92710 ms)
main error: read error: No error
main warning: picture is too late to be displayed (missing 92677 ms)
main warning: picture is too late to be displayed (missing 92644 ms)
main warning: picture is too late to be displayed (missing 92610 ms)
main warning: picture is too late to be displayed (missing 92577 ms)
main debug: incoming request - stopping current input
main debug: killing decoder fourcc `hevc'
main debug: removing module "avcodec"
main debug: saving a free vout
main debug: reusing provided vout
main debug: removing module "hevc"
main debug: killing decoder fourcc `mp4a'
main debug: removing module "faad"
wasapi debug: reset
main debug: removing module "samplerate"
main debug: removing module "scaletempo"
mmdevice debug: state changed: 0
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
main debug: saving media library to file C:\Users\ROG\AppData\Roaming\vlc\ml.xspf.tmp3204
main debug: looking for playlist export module matching "export-xspf": 4 candidates
main debug: using playlist export module "export"
main debug: removing module "export"
main debug: deleting item `Media Library'
main debug: deleting item `udp://127.0.0.1:62024'
main debug: deleting item `Playlist'
main debug: removing module "memory"
-- logger module stopped --

~~~

## source0最终记录

~~~text
frame= 3560 fps= 30 q=-1.0 size=  117571KiB time=00:01:58.66 bitrate=8116.4kbits/s speed=   1x elapsed=0:01:58.16    
frame= 3576 fps= 30 q=-1.0 size=  118130KiB time=00:01:59.20 bitrate=8118.5kbits/s speed=   1x elapsed=0:01:58.69    
frame= 3591 fps= 30 q=-1.0 size=  118561KiB time=00:01:59.70 bitrate=8114.0kbits/s speed=   1x elapsed=0:01:59.20    
[out#0/rtp @ 000001e0afb06200] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
[out#1/rtp @ 000001e0b18e2fc0] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.49    
~~~

## source1最终记录

~~~text
frame= 3546 fps= 30 q=-1.0 size=  117146KiB time=00:01:58.20 bitrate=8119.0kbits/s speed=   1x elapsed=0:01:57.71    
frame= 3562 fps= 30 q=-1.0 size=  117635KiB time=00:01:58.73 bitrate=8116.2kbits/s speed=   1x elapsed=0:01:58.22    
frame= 3578 fps= 30 q=-1.0 size=  118214KiB time=00:01:59.26 bitrate=8119.7kbits/s speed=   1x elapsed=0:01:58.75    
frame= 3593 fps= 30 q=-1.0 size=  118601KiB time=00:01:59.76 bitrate=8112.3kbits/s speed=   1x elapsed=0:01:59.26    
[out#0/rtp @ 000001d724918900] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
~~~

## source2最终记录

~~~text
frame= 3544 fps= 30 q=-1.0 size=  117123KiB time=00:01:58.13 bitrate=8121.9kbits/s speed=   1x elapsed=0:01:57.63    
frame= 3560 fps= 30 q=-1.0 size=  117571KiB time=00:01:58.66 bitrate=8116.4kbits/s speed=   1x elapsed=0:01:58.15    
frame= 3575 fps= 30 q=-1.0 size=  118060KiB time=00:01:59.16 bitrate=8115.9kbits/s speed=   1x elapsed=0:01:58.66    
frame= 3591 fps= 30 q=-1.0 size=  118561KiB time=00:01:59.70 bitrate=8114.0kbits/s speed=   1x elapsed=0:01:59.18    
[out#0/rtp @ 0000021911a0c840] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
~~~

## reentry最终记录

~~~text
frame= 3558 fps= 30 q=-1.0 size=  117493KiB time=00:01:58.60 bitrate=8115.6kbits/s speed=   1x elapsed=0:01:58.08    
frame= 3573 fps= 30 q=-1.0 size=  118036KiB time=00:01:59.10 bitrate=8118.8kbits/s speed=   1x elapsed=0:01:58.60    
frame= 3589 fps= 30 q=-1.0 size=  118528KiB time=00:01:59.63 bitrate=8116.3kbits/s speed=   1x elapsed=0:01:59.11    
[out#0/rtp @ 0000021d361d6a00] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
[out#1/rtp @ 0000021d37ef1280] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
~~~

## 本轮临时文件清单

- composition-r380-cli-error.log (2376 bytes)
- composition-r380-cli.log (1213892 bytes)
- composition-r380-monitor.csv (23350 bytes)
- composition-r380-native-2026-11-02-11h54m39s335.png (896680 bytes)
- composition-r380-native-2026-11-02-11h56m42s265.png (12152 bytes)
- composition-r380-native-2026-11-02-11h57m16s744.png (317415 bytes)
- composition-r380-reentry-stdout.log (540 bytes)
- composition-r380-reentry.log (29521 bytes)
- composition-r380-source0-stdout.log (540 bytes)
- composition-r380-source0.log (29520 bytes)
- composition-r380-source1-stdout.log (335 bytes)
- composition-r380-source1.log (28926 bytes)
- composition-r380-source2-stdout.log (335 bytes)
- composition-r380-source2.log (28925 bytes)
- composition-r380-vlc.log (37772 bytes)

清理结果：15项本轮产物已逐项删除，文件残留0；指定120秒测试源保留。
