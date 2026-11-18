# r379：全图持有量预算消费与显式账目身份回归

日期：2026-11-02。结果 **FAIL**：音频访问单元到达 MPEG-TS 时已错过传输服务窗口；未执行全失源/重入阶段。

本轮冻结20文件获两位未参与实现者局部PASS；Release全量784目标构建成功，配置/构建退出码0，四产物核验，realtime产物11:33:21。纯residence已实际决定source预算，并通过真实图逐项readback进入输出；这不证明早期候选、联合solver或SourcePresentation已完成。

## 实际启动与监控命令

```powershell
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r379-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r379-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r379-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r379-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r379-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r379-source2.log' -PassThru
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r379 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r379-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r379-cli-error.log' -PassThru
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r379-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r379-native- --snapshot-format=png udp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
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
$samples | Export-Csv -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r379-monitor.csv' -NoTypeInformation -Encoding UTF8
$source0.WaitForExit()
$source1.WaitForExit()
$source2.WaitForExit()
Write-Output "SOURCE0_EXIT=$($source0.ExitCode) SOURCE1_EXIT=$($source1.ExitCode) SOURCE2_EXIT=$($source2.ExitCode)"
```

PID：source0=36048，source1=17804，source2=36316，CLI=33524，VLC=6784。启动前同执行环境未见媒体进程或所需端口占用；启动后UDP62024、TCP63020均归VLC6784。

## VLC原生画面

已查看 composition-r379-native-2026-11-02-11h34m13s760.png：三个游戏画面及右下黑格。快照前后确认CLI33524存在；不同于r378退出后的留帧，本轮是运行期间原生快照。未取得全失源或重入快照。

```powershell
if(-not (Get-CimInstance Win32_Process -Filter 'ProcessId=33524')){throw 'CLI already ended before snapshot'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
try{$s=$c.GetStream();$s.ReadTimeout=2000;$b=[Text.Encoding]::ASCII.GetBytes('snapshot'+[char]10);$s.Write($b,0,$b.Length);$buf=[byte[]]::new(4096);try{$n=$s.Read($buf,0,$buf.Length);[Text.Encoding]::UTF8.GetString($buf,0,$n)}catch{Write-Output $_.Exception.Message}}finally{$c.Dispose()}
Get-ChildItem -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance' -Filter 'composition-r379-native-*.png' | Select-Object FullName,LastWriteTime
Get-CimInstance Win32_Process -Filter 'ProcessId=33524' | Select-Object ProcessId,Name
```

## 失败与指标

CLI自然失败退出1，三个初始源各自完整120秒/3600帧退出0；不等于CLI处理了全长。11:35:30.080首个worker失败为FileMux node76：audio canonical_sequence4486，emit109588462266、dispatch109688462266、service_start109713634420ns，remaining=-25172154ns。音频head等待缺失video144.5322ms后才提交；该区间canvas batch completion记录132.5144ms，整个compose132.6865ms。时间相关性尚不能区分GPU完成、驱动等待和线程调度根因，不能据此直接归因sender或宣称MMCSS可修复。

最终workerErrors/errors=1、stalledIntervals=0；98183预约/释放匹配归零。pressure计数1在首错前出现，后续Cancelled是清理结果，不把pressure计数自动解释为首因。内部平均进程整机CPU1.994781%、峰5.625%；WS初始208842752、峰269869056、最终269451264bytes。sender提交74390datagrams/89335532payload bytes，deadline_misses/partial/ambiguous为0，delivery仍not_proven。worst-success序号24983发生在约40秒，与本次约110秒失败不同，不能混作根因。

## 进程退出与清理

CLI和源退出码由启动进程对象WaitForExit取得。VLC按精确PID和本轮commandline/RC端口核验后退出：

```powershell
$v=Get-CimInstance Win32_Process -Filter "ProcessId=6784"
if(-not $v -or $v.Name -ne 'vlc.exe' -or $v.CommandLine -notlike '*composition-r379*'){throw 'VLC identity mismatch'}
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 6784){throw 'VLC port mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
try{$s=$c.GetStream();$b=[Text.Encoding]::ASCII.GetBytes('quit'+[char]10);$s.Write($b,0,$b.Length)}finally{$c.Dispose()}
```

同执行环境复查五个本轮PID和UDP62024/TCP63020均无残留。后续按文末清单逐项删除临时产物，保留指定120秒测试源。


## CLI诊断（保留全部非周期行）

~~~text
[CLI] composition_inputs=3 grid=fixed audio_source=2 audio=on max_duration=source_driven hw=planner-highest-score
[2026-11-02 11:33:40.846] [info] [graph][planner.capability] composition_protocol_installed source=2
[2026-11-02 11:33:40.858] [info] [graph][planner.capability] composition_protocol_installed source=1
[2026-11-02 11:33:40.860] [info] [graph][planner.capability] composition_protocol_installed source=0
[2026-11-02 11:33:40.946] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 11:33:40.947] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 11:33:40.948] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-r379.source.0 decoder=h264
[2026-11-02 11:33:45.347] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-11-02 11:33:45.347] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-11-02 11:33:45.362] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-11-02 11:33:45.400] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 11:33:45.400] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 11:33:45.400] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-r379.source.1 decoder=h264
[2026-11-02 11:33:45.453] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 11:33:45.453] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 11:33:45.453] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-r379.source.2 decoder=h264
[2026-11-02 11:33:45.535] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-11-02 11:33:45.582] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-11-02 11:33:45.582] [info] [graph][planner.capability] canvas_cuda_stream stage=prepared device_stream=0000000000000000 private_nonblocking=1
[2026-11-02 11:33:45.589] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 11:33:45.590] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 11:33:45.590] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-11-02 11:33:45.595] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 11:33:45.596] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 11:33:45.596] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-11-02 11:33:45.601] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 11:33:45.601] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 11:33:45.601] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-11-02 11:33:45.605] [info] [graph][planner.capability] composition_preparation_storage stage=graph_bound scope=shared_reservations capacity=2147483648 retained=831666548 high_water=857843433
[CLI] audio_plan branch=transcode_frame reason=transcode_required_by_output_frame_contract codec=aac sample_rate=44100 channels=2 access_unit_samples=1024 encoder=aac bitrate_kbps=192
[CLI] prepared_output_id=1
[CLI] selected_chain=fixed-grid-composition score=0 decoder=h264,h264,h264 filter=scale_cuda=640:360,scale_cuda=640:360,scale_cuda=640:360 encoder=hevc_nvenc
[2026-11-02 11:33:45.607] [info] [graph][runtime.lifecycle] compile.begin
[2026-11-02 11:33:45.608] [info] [graph][runtime.channel] create channel=1 edge=1 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 11:33:45.608] [info] [graph][runtime.channel] create channel=2 edge=2 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 11:33:45.608] [info] [graph][runtime.channel] create channel=3 edge=3 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-02 11:33:45.608] [info] [graph][runtime.channel] create channel=4 edge=4 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.608] [info] [graph][runtime.channel] create channel=5 edge=5 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.608] [info] [graph][runtime.channel] create channel=6 edge=6 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.608] [info] [graph][runtime.channel] create channel=7 edge=7 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.608] [info] [graph][runtime.channel] create channel=8 edge=8 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.608] [info] [graph][runtime.channel] create channel=9 edge=9 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:33:45.609] [info] [graph][runtime.channel] create channel=10 edge=10 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.609] [info] [graph][runtime.channel] create channel=11 edge=11 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.609] [info] [graph][runtime.channel] create channel=12 edge=12 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:33:45.609] [info] [graph][runtime.channel] create channel=13 edge=13 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:33:45.609] [info] [graph][runtime.channel] create channel=14 edge=14 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.609] [info] [graph][runtime.channel] create channel=15 edge=15 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.609] [info] [graph][runtime.channel] create channel=16 edge=16 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.609] [info] [graph][runtime.channel] create channel=17 edge=17 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.609] [info] [graph][runtime.channel] create channel=18 edge=18 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.609] [info] [graph][runtime.channel] create channel=19 edge=19 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.609] [info] [graph][runtime.channel] create channel=20 edge=20 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.609] [info] [graph][runtime.channel] create channel=21 edge=21 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-02 11:33:45.609] [info] [graph][runtime.channel] create channel=22 edge=22 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 11:33:45.609] [info] [graph][runtime.channel] create channel=23 edge=23 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:33:45.609] [info] [graph][runtime.channel] create channel=24 edge=24 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 11:33:45.610] [info] [graph][runtime.channel] create channel=25 edge=25 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:33:45.610] [info] [graph][runtime.channel] create channel=26 edge=26 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:33:45.610] [info] [graph][runtime.channel] create channel=27 edge=27 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:33:45.610] [info] [graph][runtime.channel] create channel=28 edge=28 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:33:45.610] [info] [graph][runtime.channel] create channel=29 edge=29 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:33:45.610] [info] [graph][runtime.channel] create channel=30 edge=30 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.610] [info] [graph][runtime.channel] create channel=31 edge=31 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.610] [info] [graph][runtime.channel] create channel=32 edge=32 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.610] [info] [graph][runtime.channel] create channel=33 edge=33 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.610] [info] [graph][runtime.channel] create channel=34 edge=34 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.610] [info] [graph][runtime.channel] create channel=35 edge=35 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.610] [info] [graph][runtime.channel] create channel=36 edge=36 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:33:45.611] [info] [graph][runtime.channel] create channel=37 edge=37 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.611] [info] [graph][runtime.channel] create channel=38 edge=38 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.611] [info] [graph][runtime.channel] create channel=39 edge=39 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:33:45.611] [info] [graph][runtime.channel] create channel=40 edge=40 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:33:45.611] [info] [graph][runtime.channel] create channel=41 edge=41 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.611] [info] [graph][runtime.channel] create channel=42 edge=42 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.611] [info] [graph][runtime.channel] create channel=43 edge=43 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.611] [info] [graph][runtime.channel] create channel=44 edge=44 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.611] [info] [graph][runtime.channel] create channel=45 edge=45 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.611] [info] [graph][runtime.channel] create channel=46 edge=46 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.612] [info] [graph][runtime.channel] create channel=47 edge=47 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.612] [info] [graph][runtime.channel] create channel=48 edge=48 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-02 11:33:45.612] [info] [graph][runtime.channel] create channel=49 edge=49 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 11:33:45.612] [info] [graph][runtime.channel] create channel=50 edge=50 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:33:45.612] [info] [graph][runtime.channel] create channel=51 edge=51 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 11:33:45.612] [info] [graph][runtime.channel] create channel=52 edge=52 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:33:45.612] [info] [graph][runtime.channel] create channel=53 edge=53 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:33:45.612] [info] [graph][runtime.channel] create channel=54 edge=54 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:33:45.612] [info] [graph][runtime.channel] create channel=55 edge=55 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:33:45.612] [info] [graph][runtime.channel] create channel=56 edge=56 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:33:45.612] [info] [graph][runtime.channel] create channel=57 edge=57 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.612] [info] [graph][runtime.channel] create channel=58 edge=58 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.612] [info] [graph][runtime.channel] create channel=59 edge=59 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.612] [info] [graph][runtime.channel] create channel=60 edge=60 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.612] [info] [graph][runtime.channel] create channel=61 edge=61 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.612] [info] [graph][runtime.channel] create channel=62 edge=62 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.613] [info] [graph][runtime.channel] create channel=63 edge=63 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.613] [info] [graph][runtime.channel] create channel=64 edge=64 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.613] [info] [graph][runtime.channel] create channel=65 edge=65 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.613] [info] [graph][runtime.channel] create channel=66 edge=66 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:33:45.613] [info] [graph][runtime.channel] create channel=67 edge=67 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:33:45.613] [info] [graph][runtime.channel] create channel=68 edge=68 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.613] [info] [graph][runtime.channel] create channel=69 edge=69 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.613] [info] [graph][runtime.channel] create channel=70 edge=70 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.613] [info] [graph][runtime.channel] create channel=71 edge=71 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:33:45.613] [info] [graph][runtime.channel] create channel=72 edge=72 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:33:45.613] [info] [graph][runtime.channel] create channel=73 edge=73 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:33:45.613] [info] [graph][runtime.channel] create channel=74 edge=74 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:33:45.613] [info] [graph][runtime.channel] create channel=75 edge=75 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.613] [info] [graph][runtime.channel] create channel=76 edge=76 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.613] [info] [graph][runtime.channel] create channel=77 edge=77 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.613] [info] [graph][runtime.channel] create channel=78 edge=78 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.614] [info] [graph][runtime.channel] create channel=79 edge=79 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.614] [info] [graph][runtime.channel] create channel=80 edge=80 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.614] [info] [graph][runtime.channel] create channel=81 edge=81 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.614] [info] [graph][runtime.channel] create channel=82 edge=82 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.614] [info] [graph][runtime.channel] create channel=83 edge=83 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-02 11:33:45.614] [info] [graph][runtime.channel] create channel=84 edge=84 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 11:33:45.614] [info] [graph][runtime.channel] create channel=85 edge=85 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:33:45.614] [info] [graph][runtime.channel] create channel=86 edge=86 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 11:33:45.614] [info] [graph][runtime.channel] create channel=87 edge=87 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:33:45.614] [info] [graph][runtime.channel] create channel=88 edge=88 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:33:45.614] [info] [graph][runtime.channel] create channel=89 edge=89 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:33:45.614] [info] [graph][runtime.channel] create channel=90 edge=90 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:33:45.614] [info] [graph][runtime.channel] create channel=91 edge=91 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-02 11:33:45.615] [info] [graph][runtime.channel] create channel=92 edge=92 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 11:33:45.615] [info] [graph][runtime.channel] create channel=93 edge=93 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:33:45.615] [info] [graph][runtime.channel] create channel=94 edge=94 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 11:33:45.615] [info] [graph][runtime.channel] create channel=95 edge=95 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:33:45.615] [info] [graph][runtime.channel] create channel=96 edge=96 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:33:45.615] [info] [graph][runtime.channel] create channel=97 edge=97 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:33:45.615] [info] [graph][runtime.channel] create channel=98 edge=98 stream=Audio edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.615] [info] [graph][runtime.channel] create channel=99 edge=99 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:33:45.615] [info] [graph][runtime.channel] create channel=100 edge=100 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.615] [info] [graph][runtime.channel] create channel=101 edge=101 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0
[2026-11-02 11:33:45.615] [info] [graph][runtime.channel] create channel=102 edge=102 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 11:33:45.616] [info] [graph][runtime.channel] create channel=103 edge=103 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 11:33:45.616] [info] [graph][runtime.channel] create channel=104 edge=104 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:33:45.616] [info] [graph][runtime.channel] create channel=105 edge=105 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0
[2026-11-02 11:33:45.616] [info] [graph][runtime.channel] create channel=106 edge=106 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.616] [info] [graph][runtime.channel] create channel=107 edge=107 stream=Video edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-02 11:33:45.616] [info] [graph][runtime.channel] create channel=108 edge=108 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-02 11:33:45.616] [info] [graph][runtime.channel] create channel=109 edge=109 stream=Any edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-02 11:33:45.616] [info] [graph][runtime.channel] create channel=110 edge=110 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.616] [info] [graph][runtime.channel] create channel=111 edge=111 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-02 11:33:45.616] [info] [graph][runtime.channel] create channel=112 edge=112 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:33:45.616] [info] [graph][runtime.channel] create channel=113 edge=113 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 11:33:45.616] [info] [graph][runtime.channel] create channel=114 edge=114 stream=Any edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-02 11:33:45.616] [info] [graph][runtime.channel] create channel=115 edge=115 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 11:33:45.616] [info] [graph][runtime.channel] create channel=116 edge=116 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-02 11:33:45.617] [info] [graph][runtime.channel] create channel=117 edge=117 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-02 11:33:45.617] [info] [graph][runtime.channel] create channel=118 edge=118 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-02 11:33:45.617] [info] [graph][runtime.channel] create channel=119 edge=119 stream=Any edge_kind=EncodedPacket payload=Unknown queued=0
[2026-11-02 11:33:45.617] [info] [graph][runtime.channel] create channel=120 edge=120 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-02 11:33:45.617] [info] [graph][runtime.channel] create channel=121 edge=121 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-02 11:33:45.617] [info] [graph][runtime.channel] create channel=122 edge=122 stream=Metadata edge_kind=Unknown payload=Unknown queued=0
[2026-11-02 11:33:45.617] [info] [graph][runtime.channel] create channel=123 edge=123 stream=Metadata edge_kind=Unknown payload=Unknown queued=0
[2026-11-02 11:33:45.617] [info] [graph][runtime.lifecycle] compiled nodes=81 edges=123 channels=123 execution_order=1->3->8->27->46->47->9->21->28->40->63->48->69->10->29->49->11->12->30->31->50->51->52->13->32->53->14->17->33->36->54->55->59->15->34->56->57->16->35->58->18->37->60->19->38->61->20->39->62->22->41->64->70->23->42->65->71->24->43->66->72->26->45->68->73->25->44->67->6->2->7->80->4->77->5->74->75->78->76->79->81
[2026-11-02 11:33:45.618] [info] [graph][runtime.lifecycle] compile.done state=DefaultRegistrationPending
[2026-11-02 11:33:45.618] [info] [graph][runtime.node] register node=1 name=composition-r379.output.video.codec_resolver kind=CodecResolver
[2026-11-02 11:33:45.618] [info] [graph][runtime.node] register node=2 name=composition-r379.output.video.encode kind=VideoEncode
[2026-11-02 11:33:45.618] [info] [graph][runtime.node] register node=3 name=composition-r379.output.audio.codec_resolver kind=AudioCodecResolver
[2026-11-02 11:33:45.618] [info] [graph][runtime.node] register node=4 name=composition-r379.output.audio.encode kind=AudioEncode
[2026-11-02 11:33:45.618] [info] [graph][runtime.node] register node=5 name=composition-r379.output.audio.canonicalizer kind=EncodedAudioCanonicalizer
[2026-11-02 11:33:45.618] [info] [graph][runtime.node] register node=8 name=composition-r379.source_0.input kind=RawRtpInput
[2026-11-02 11:33:45.618] [info] [graph][runtime.node] register node=9 name=composition-r379.source_0.rtp.clock_group kind=RtpClockGroup
[2026-11-02 11:33:45.619] [info] [graph][runtime.node] register node=10 name=composition-r379.source_0.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-11-02 11:33:45.619] [info] [graph][runtime.node] register node=11 name=composition-r379.source_0.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-11-02 11:33:45.619] [info] [graph][runtime.node] register node=12 name=composition-r379.source_0.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-11-02 11:33:45.619] [info] [graph][runtime.node] register node=13 name=composition-r379.source_0.av_sync.source_clock kind=SourceClockStateFanout
[2026-11-02 11:33:45.619] [info] [graph][runtime.node] register node=14 name=composition-r379.source_0.av_sync.video.generation_gate kind=LockedPacketGate
[2026-11-02 11:33:45.619] [info] [graph][runtime.node] register node=15 name=composition-r379.source_0.av_sync.video.canonical_input kind=CanonicalInput
[2026-11-02 11:33:45.619] [info] [graph][runtime.node] register node=16 name=composition-r379.source_0.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-11-02 11:33:45.619] [info] [graph][runtime.node] register node=17 name=composition-r379.source_0.av_sync.startup.clock kind=AvStartupClock
[2026-11-02 11:33:45.619] [info] [graph][runtime.node] register node=18 name=composition-r379.source_0.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-11-02 11:33:45.619] [info] [graph][runtime.node] register node=20 name=composition-r379.source_0.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-11-02 11:33:45.619] [info] [graph][runtime.node] register node=21 name=composition-r379.source_0.video.codec_resolver kind=CodecResolver
[2026-11-02 11:33:45.619] [info] [graph][runtime.node] register node=22 name=composition-r379.source_0.video.decode kind=VideoDecode
[2026-11-02 11:33:45.619] [info] [graph][runtime.node] register node=23 name=composition-r379.source_0.video.hwtransfer kind=HardwareTransfer
[2026-11-02 11:33:45.620] [info] [graph][runtime.node] register node=24 name=composition-r379.source_0.video.framerate kind=VideoFrameRate
[2026-11-02 11:33:45.620] [info] [graph][runtime.node] register node=25 name=composition-r379.source_0.video.preparation kind=VideoPreparation
[2026-11-02 11:33:45.620] [info] [graph][runtime.node] register node=26 name=composition-r379.source_0.video.filter kind=VideoFilter
[2026-11-02 11:33:45.620] [info] [graph][runtime.node] register node=27 name=composition-r379.source_1.input kind=RawRtpInput
[2026-11-02 11:33:45.620] [info] [graph][runtime.node] register node=28 name=composition-r379.source_1.rtp.clock_group kind=RtpClockGroup
[2026-11-02 11:33:45.620] [info] [graph][runtime.node] register node=29 name=composition-r379.source_1.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-11-02 11:33:45.620] [info] [graph][runtime.node] register node=30 name=composition-r379.source_1.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-11-02 11:33:45.620] [info] [graph][runtime.node] register node=31 name=composition-r379.source_1.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-11-02 11:33:45.620] [info] [graph][runtime.node] register node=32 name=composition-r379.source_1.av_sync.source_clock kind=SourceClockStateFanout
[2026-11-02 11:33:45.620] [info] [graph][runtime.node] register node=33 name=composition-r379.source_1.av_sync.video.generation_gate kind=LockedPacketGate
[2026-11-02 11:33:45.620] [info] [graph][runtime.node] register node=34 name=composition-r379.source_1.av_sync.video.canonical_input kind=CanonicalInput
[2026-11-02 11:33:45.620] [info] [graph][runtime.node] register node=35 name=composition-r379.source_1.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-11-02 11:33:45.620] [info] [graph][runtime.node] register node=36 name=composition-r379.source_1.av_sync.startup.clock kind=AvStartupClock
[2026-11-02 11:33:45.620] [info] [graph][runtime.node] register node=37 name=composition-r379.source_1.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-11-02 11:33:45.620] [info] [graph][runtime.node] register node=39 name=composition-r379.source_1.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-11-02 11:33:45.620] [info] [graph][runtime.node] register node=40 name=composition-r379.source_1.video.codec_resolver kind=CodecResolver
[2026-11-02 11:33:45.621] [info] [graph][runtime.node] register node=41 name=composition-r379.source_1.video.decode kind=VideoDecode
[2026-11-02 11:33:45.621] [info] [graph][runtime.node] register node=42 name=composition-r379.source_1.video.hwtransfer kind=HardwareTransfer
[2026-11-02 11:33:45.621] [info] [graph][runtime.node] register node=43 name=composition-r379.source_1.video.framerate kind=VideoFrameRate
[2026-11-02 11:33:45.621] [info] [graph][runtime.node] register node=44 name=composition-r379.source_1.video.preparation kind=VideoPreparation
[2026-11-02 11:33:45.621] [info] [graph][runtime.node] register node=45 name=composition-r379.source_1.video.filter kind=VideoFilter
[2026-11-02 11:33:45.621] [info] [graph][runtime.node] register node=46 name=composition-r379.source_2.video.input kind=RawRtpInput
[2026-11-02 11:33:45.621] [info] [graph][runtime.node] register node=47 name=composition-r379.source_2.audio.input kind=RawRtpInput
[2026-11-02 11:33:45.621] [info] [graph][runtime.node] register node=48 name=composition-r379.source_2.rtp.clock_group kind=RtpClockGroup
[2026-11-02 11:33:45.621] [info] [graph][runtime.node] register node=49 name=composition-r379.source_2.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-11-02 11:33:45.621] [info] [graph][runtime.node] register node=50 name=composition-r379.source_2.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-11-02 11:33:45.621] [info] [graph][runtime.node] register node=51 name=composition-r379.source_2.av_sync.audio.protocol_binder kind=RtpPacketClockBinder
[2026-11-02 11:33:45.621] [info] [graph][runtime.node] register node=52 name=composition-r379.source_2.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-11-02 11:33:45.621] [info] [graph][runtime.node] register node=53 name=composition-r379.source_2.av_sync.source_clock kind=SourceClockStateFanout
[2026-11-02 11:33:45.621] [info] [graph][runtime.node] register node=54 name=composition-r379.source_2.av_sync.video.generation_gate kind=LockedPacketGate
[2026-11-02 11:33:45.621] [info] [graph][runtime.node] register node=55 name=composition-r379.source_2.av_sync.audio.generation_gate kind=LockedPacketGate
[2026-11-02 11:33:45.621] [info] [graph][runtime.node] register node=56 name=composition-r379.source_2.av_sync.video.canonical_input kind=CanonicalInput
[2026-11-02 11:33:45.621] [info] [graph][runtime.node] register node=57 name=composition-r379.source_2.av_sync.audio.canonical_input kind=CanonicalInput
[2026-11-02 11:33:45.622] [info] [graph][runtime.node] register node=58 name=composition-r379.source_2.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-11-02 11:33:45.622] [info] [graph][runtime.node] register node=59 name=composition-r379.source_2.av_sync.startup.clock kind=AvStartupClock
[2026-11-02 11:33:45.622] [info] [graph][runtime.node] register node=60 name=composition-r379.source_2.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-11-02 11:33:45.622] [info] [graph][runtime.node] register node=62 name=composition-r379.source_2.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-11-02 11:33:45.622] [info] [graph][runtime.node] register node=63 name=composition-r379.source_2.video.codec_resolver kind=CodecResolver
[2026-11-02 11:33:45.622] [info] [graph][runtime.node] register node=64 name=composition-r379.source_2.video.decode kind=VideoDecode
[2026-11-02 11:33:45.622] [info] [graph][runtime.node] register node=65 name=composition-r379.source_2.video.hwtransfer kind=HardwareTransfer
[2026-11-02 11:33:45.622] [info] [graph][runtime.node] register node=66 name=composition-r379.source_2.video.framerate kind=VideoFrameRate
[2026-11-02 11:33:45.622] [info] [graph][runtime.node] register node=67 name=composition-r379.source_2.video.preparation kind=VideoPreparation
[2026-11-02 11:33:45.622] [info] [graph][runtime.node] register node=68 name=composition-r379.source_2.video.filter kind=VideoFilter
[2026-11-02 11:33:45.622] [info] [graph][runtime.node] register node=69 name=composition-r379.source_2.audio.encode.codec_resolver kind=AudioCodecResolver
[2026-11-02 11:33:45.622] [info] [graph][runtime.node] register node=70 name=composition-r379.source_2.audio.encode.decode kind=AudioDecode
[2026-11-02 11:33:45.622] [info] [graph][runtime.node] register node=71 name=composition-r379.source_2.audio.encode.startup_trim kind=Unknown
[2026-11-02 11:33:45.622] [info] [graph][runtime.node] register node=72 name=composition-r379.source_2.audio.encode.drift_controller kind=AudioDriftController
[2026-11-02 11:33:45.622] [info] [graph][runtime.node] register node=73 name=composition-r379.source_2.audio.encode.resample kind=AudioResample
[2026-11-02 11:33:45.622] [info] [graph][runtime.node] register node=74 name=composition-r379.output.scheduler.scheduler kind=AvOutputScheduler
[2026-11-02 11:33:45.623] [info] [graph][runtime.node] register node=75 name=composition-r379.output.scheduler.router kind=ScheduledOutputRouter
[2026-11-02 11:33:45.623] [info] [graph][runtime.node] register node=76 name=composition-r379.output.mpegts.mux kind=FileMux
[2026-11-02 11:33:45.623] [info] [graph][runtime.node] register node=77 name=composition-r379.output.mpegts.plan.source kind=Unknown
[2026-11-02 11:33:45.623] [info] [graph][runtime.node] register node=78 name=composition-r379.output.mpegts.scheduled.adapter kind=Unknown
[2026-11-02 11:33:45.624] [info] [graph][runtime.node] register node=79 name=composition-r379.output.mpegts.datagram.materializer kind=MpegTsDatagramMaterializer
[2026-11-02 11:33:45.624] [info] [graph][runtime.node] register node=80 name=composition-r379.output.mpegts.transport.plan kind=DatagramTransportPlanSource
[2026-11-02 11:33:45.624] [info] [graph][runtime.node] register node=81 name=composition-r379.output.mpegts.transport.sender kind=ScheduledDatagramSender
[2026-11-02 11:33:45.624] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=waiting_for_random_access detail="" description=""
[2026-11-02 11:33:45.624] [info] [graph][runtime.lifecycle] start.begin mode=threaded
[2026-11-02 11:33:45.626] [info] [graph][runtime.lifecycle] worker.finished node=1
[2026-11-02 11:33:45.626] [info] [graph][runtime.node] worker.thread_cpu node=1 available=1 cpu_ns=0 process_calls=1
[2026-11-02 11:33:45.626] [info] [graph][runtime.lifecycle] worker.finished node=3
[2026-11-02 11:33:45.627] [info] [graph][runtime.node] worker.thread_cpu node=3 available=1 cpu_ns=0 process_calls=1
[2026-11-02 11:33:45.627] [info] [graph][runtime.node] audio_encode_trace stage=codec_bound
[2026-11-02 11:33:45.627] [info] [graph][runtime.lifecycle] start.done state=ThreadedRunning
[2026-11-02 11:33:45.627] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:33:45.628] [info] [graph][runtime.lifecycle] worker.finished node=69
[2026-11-02 11:33:45.628] [info] [graph][runtime.node] worker.thread_cpu node=69 available=1 cpu_ns=0 process_calls=1
[2026-11-02 11:33:45.628] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:33:45.628] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:33:45.628] [info] [graph][runtime.lifecycle] worker.finished node=21
[2026-11-02 11:33:45.628] [info] [graph][runtime.node] worker.thread_cpu node=21 available=1 cpu_ns=0 process_calls=1
[2026-11-02 11:33:45.631] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-11-02 11:33:45.631] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=15 generation=1
[2026-11-02 11:33:45.632] [info] [graph][runtime.lifecycle] worker.finished node=40
[2026-11-02 11:33:45.632] [info] [graph][runtime.node] worker.thread_cpu node=40 available=1 cpu_ns=0 process_calls=2
[2026-11-02 11:33:45.632] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=release source_identity=composition-r379.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4016000000 duration_ns=33333333 decoded_sequence=unavailable pts=599676260 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=9901349046500 group=composition-r379.source.2 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-4016000000 epoch_master_release_ns=5366104500 video_count=15 min_presentation_ns=-4016000000 max_end_ns=-3516000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.632] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=published source_identity=composition-r379.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4016000000 duration_ns=33333333 decoded_sequence=unavailable pts=599676260 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=9901349474200 group=composition-r379.source.2 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.634] [info] [graph][runtime.node] video_encode.bind_encoder codec=hevc_nvenc pix_fmt=cuda rate_control=cbr bitrate=8000000 min_bitrate=8000000 max_bitrate=8000000 buffer_size=267000 fps=30/1 gop=60 frame_kind=hardware hwaccel=cuda hw_device_ctx=set hw_frames_ctx=set
[2026-11-02 11:33:45.634] [info] [graph][runtime.lifecycle] worker.finished node=63
[2026-11-02 11:33:45.635] [info] [graph][runtime.node] worker.thread_cpu node=63 available=1 cpu_ns=0 process_calls=1
[2026-11-02 11:33:45.634] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=599676260 dts=599676260 size=317016 generation=1 sequence=15
[2026-11-02 11:33:45.635] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-11-02 11:33:45.635] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=input source_identity=composition-r379.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4016000000 duration_ns=33333333 decoded_sequence=unavailable pts=599676260 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=9901352050400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.635] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=15 generation=1
[2026-11-02 11:33:45.636] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-11-02 11:33:45.636] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=15 generation=1
[2026-11-02 11:33:45.637] [info] [graph][runtime.node] source_video_trace node_id=20 node=MediaAvBoundReleaseExtractorNode stage=release source_identity=composition-r379.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4009000000 duration_ns=33333333 decoded_sequence=unavailable pts=614936447 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=9901353631600 group=composition-r379.source.0 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=-4009000000 epoch_master_release_ns=5371179900 video_count=15 min_presentation_ns=-4009000000 max_end_ns=-3509000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.637] [info] [graph][runtime.node] source_video_trace node_id=20 node=MediaAvBoundReleaseExtractorNode stage=published source_identity=composition-r379.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4009000000 duration_ns=33333333 decoded_sequence=unavailable pts=614936447 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=9901353760300 group=composition-r379.source.0 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.637] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=614936447 dts=614936447 size=317016 generation=1 sequence=15
[2026-11-02 11:33:45.637] [info] [graph][runtime.node] source_video_trace node_id=39 node=MediaAvBoundReleaseExtractorNode stage=release source_identity=composition-r379.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4018000000 duration_ns=33333333 decoded_sequence=unavailable pts=3011764430 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=9901353943300 group=composition-r379.source.1 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-4018000000 epoch_master_release_ns=5371594700 video_count=15 min_presentation_ns=-4018000000 max_end_ns=-3518000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.637] [info] [graph][runtime.node] source_video_trace node_id=39 node=MediaAvBoundReleaseExtractorNode stage=published source_identity=composition-r379.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4018000000 duration_ns=33333333 decoded_sequence=unavailable pts=3011764430 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=9901354347000 group=composition-r379.source.1 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.637] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=input source_identity=composition-r379.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4009000000 duration_ns=33333333 decoded_sequence=unavailable pts=614936447 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=9901354235100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.637] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=3011764430 dts=3011764430 size=317016 generation=1 sequence=15
[2026-11-02 11:33:45.638] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=input source_identity=composition-r379.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4018000000 duration_ns=33333333 decoded_sequence=unavailable pts=3011764430 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=9901355215700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.667] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-11-02 11:33:45.667] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=accepted source_identity=composition-r379.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4009000000 duration_ns=33333333 decoded_sequence=unavailable pts=614936447 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=9901383870100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.667] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=614936447 format=cuda size=1280x720 buffer_identity=0xb08600000 drm_prime=0 software_frame=0
[2026-11-02 11:33:45.667] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=resolved source_identity=composition-r379.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4009000000 duration_ns=33333333 decoded_sequence=unavailable pts=614936447 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=9901384167400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.667] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=0000019FE265FE50 stream=Video payload=Frame pts=614936447 dts=614936447 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:33:45.667] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=published source_identity=composition-r379.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4009000000 duration_ns=33333333 decoded_sequence=unavailable pts=614936447 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=9901384351400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.667] [info] [graph][runtime.node] source_video_trace node_id=23 node=HardwareTransferNode stage=published source_identity=composition-r379.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4009000000 duration_ns=33333333 decoded_sequence=unavailable pts=614936447 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=9901384470100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.667] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=0000019FE265FE50 stream=Video payload=Frame pts=614936447 dts=614936447 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:33:45.668] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=0000019FE265FE50 stream=Video payload=Frame pts=614936447 dts=614936447 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:33:45.668] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-11-02 11:33:45.668] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=0000019FE2660240 stream=Video payload=Frame pts=614936447 dts=614936447 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:33:45.668] [info] [graph][runtime.node] source_video_trace node_id=24 node=VideoFrameRateNode stage=published source_identity=composition-r379.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4009000000 duration_ns=33333333 decoded_sequence=unavailable pts=614936447 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=9901384922700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.668] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-11-02 11:33:45.668] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=0000019FE2660240 stream=Video payload=Frame pts=614936447 dts=614936447 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:33:45.669] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-11-02 11:33:45.670] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=accepted source_identity=composition-r379.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4016000000 duration_ns=33333333 decoded_sequence=unavailable pts=599676260 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=9901386695200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.670] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=599676260 format=cuda size=1280x720 buffer_identity=0xb08800000 drm_prime=0 software_frame=0
[2026-11-02 11:33:45.670] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=resolved source_identity=composition-r379.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4016000000 duration_ns=33333333 decoded_sequence=unavailable pts=599676260 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=9901386977600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.670] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=published source_identity=composition-r379.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4016000000 duration_ns=33333333 decoded_sequence=unavailable pts=599676260 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=9901387088200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.670] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=0000019FE265E6B0 stream=Video payload=Frame pts=599676260 dts=599676260 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:33:45.670] [info] [graph][runtime.node] source_video_trace node_id=65 node=HardwareTransferNode stage=published source_identity=composition-r379.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4016000000 duration_ns=33333333 decoded_sequence=unavailable pts=599676260 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=9901387288600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.670] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=0000019FE265E6B0 stream=Video payload=Frame pts=599676260 dts=599676260 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:33:45.670] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=0000019FE265E6B0 stream=Video payload=Frame pts=599676260 dts=599676260 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:33:45.670] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-11-02 11:33:45.671] [info] [graph][runtime.node] source_video_trace node_id=66 node=VideoFrameRateNode stage=published source_identity=composition-r379.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4016000000 duration_ns=33333333 decoded_sequence=unavailable pts=599676260 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=9901387647400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.671] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=0000019FE265EE90 stream=Video payload=Frame pts=599676260 dts=599676260 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:33:45.671] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-11-02 11:33:45.671] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=0000019FE265EE90 stream=Video payload=Frame pts=599676260 dts=599676260 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:33:45.672] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-11-02 11:33:45.673] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=accepted source_identity=composition-r379.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4018000000 duration_ns=33333333 decoded_sequence=unavailable pts=3011764430 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=9901389591700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.673] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=3011764430 format=cuda size=1280x720 buffer_identity=0xb08c00000 drm_prime=0 software_frame=0
[2026-11-02 11:33:45.673] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=resolved source_identity=composition-r379.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4018000000 duration_ns=33333333 decoded_sequence=unavailable pts=3011764430 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=9901389896300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.673] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=0000019FE265F670 stream=Video payload=Frame pts=3011764430 dts=3011764430 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:33:45.673] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=published source_identity=composition-r379.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4018000000 duration_ns=33333333 decoded_sequence=unavailable pts=3011764430 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=9901390038700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.673] [info] [graph][runtime.node] source_video_trace node_id=42 node=HardwareTransferNode stage=published source_identity=composition-r379.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4018000000 duration_ns=33333333 decoded_sequence=unavailable pts=3011764430 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=9901390154200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.673] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=0000019FE265F670 stream=Video payload=Frame pts=3011764430 dts=3011764430 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:33:45.673] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=0000019FE265F670 stream=Video payload=Frame pts=3011764430 dts=3011764430 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:33:45.673] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-11-02 11:33:45.673] [info] [graph][runtime.node] source_video_trace node_id=43 node=VideoFrameRateNode stage=published source_identity=composition-r379.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4018000000 duration_ns=33333333 decoded_sequence=unavailable pts=3011764430 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=9901390566000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.674] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=0000019FE265E6B0 stream=Video payload=Frame pts=3011764430 dts=3011764430 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:33:45.674] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-11-02 11:33:45.674] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=0000019FE265E6B0 stream=Video payload=Frame pts=3011764430 dts=3011764430 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:33:45.736] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=65
[2026-11-02 11:33:45.737] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=62
[2026-11-02 11:33:45.737] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=input source_identity=composition-r379.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4016000000 duration_ns=33333333 decoded_sequence=unavailable pts=599676260 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=9901453894200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.737] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=68
[2026-11-02 11:33:45.737] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=input source_identity=composition-r379.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4018000000 duration_ns=33333333 decoded_sequence=unavailable pts=3011764430 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=9901454257900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.737] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=input source_identity=composition-r379.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4009000000 duration_ns=33333333 decoded_sequence=unavailable pts=614936447 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=9901454485300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.738] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=accepted source_identity=composition-r379.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4018000000 duration_ns=33333333 decoded_sequence=unavailable pts=3011764430 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=9901455100600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.739] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=accepted source_identity=composition-r379.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4016000000 duration_ns=33333333 decoded_sequence=unavailable pts=599676260 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=9901455731100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.739] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=resolved source_identity=composition-r379.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4018000000 duration_ns=33333333 decoded_sequence=unavailable pts=1003921 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=9901456112000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.739] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=published source_identity=composition-r379.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4018000000 duration_ns=33333333 decoded_sequence=unavailable pts=1003921 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=9901456288700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.739] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=resolved source_identity=composition-r379.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4016000000 duration_ns=33333333 decoded_sequence=unavailable pts=199892 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=9901456290400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.739] [info] [graph][runtime.node] source_video_trace node_id=44 node=VideoPreparationNode stage=input source_identity=composition-r379.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4018000000 duration_ns=33333333 decoded_sequence=unavailable pts=1003921 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=9901456339300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.740] [info] [graph][runtime.node] source_video_trace node_id=44 node=VideoPreparationNode stage=output_attempt source_identity=composition-r379.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4018000000 duration_ns=33333333 decoded_sequence=unavailable pts=1003921 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=9901456972400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.739] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb08b20000 drm_prime=0 software_frame=0
[2026-11-02 11:33:45.740] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=published source_identity=composition-r379.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4016000000 duration_ns=33333333 decoded_sequence=unavailable pts=199892 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=9901456797200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.740] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb08a90000 drm_prime=0 software_frame=0
[2026-11-02 11:33:45.739] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=accepted source_identity=composition-r379.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4009000000 duration_ns=33333333 decoded_sequence=unavailable pts=614936447 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=9901456216200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.740] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=15 audio_count=0 first_video_sequence=15
[2026-11-02 11:33:45.740] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=input source_identity=composition-r379.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4016000000 duration_ns=33333333 decoded_sequence=unavailable pts=199892 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=9901456847300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.741] [info] [graph][runtime.node] source_video_trace node_id=44 node=VideoPreparationNode stage=published source_identity=unavailable identity_link=node_generation_input_source_sequence generation=1 source_sequence=15 presentation_ns=-4018000000 duration_ns=33333333 decoded_sequence=unavailable pts=1003921 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=9901458041700 group=composition-r379.source.1 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.741] [info] [graph][runtime.node] source_video_trace node_id=38 node=ActivationReleaseSequencer stage=activation source_identity=composition-r379.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4018000000 duration_ns=33333333 decoded_sequence=unavailable pts=3011764430 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=9901458047900 group=composition-r379.source.1 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-4018000000 epoch_master_release_ns=5542355168 video_count=15 min_presentation_ns=-4018000000 max_end_ns=-3518000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.741] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=resolved source_identity=composition-r379.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4009000000 duration_ns=33333333 decoded_sequence=unavailable pts=204979 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=9901458050300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.741] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=output_attempt source_identity=composition-r379.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4016000000 duration_ns=33333333 decoded_sequence=unavailable pts=199892 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=9901458139300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.741] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_candidate source_identity=composition-r379.source.1.input.video generation=1 steady_ns=9901458477800 source_sequence=15 presentation_ns=-4018000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 11:33:45.741] [info] [graph][runtime.node] source_video_trace node_id=38 node=ActivationReleaseSequencer stage=release_published source_identity=composition-r379.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4018000000 duration_ns=33333333 decoded_sequence=unavailable pts=3011764430 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=9901458475100 group=composition-r379.source.1 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-4018000000 epoch_master_release_ns=5542355168 video_count=15 min_presentation_ns=-4018000000 max_end_ns=-3518000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.741] [info] [graph][runtime.node] source_video_trace node_id=39 node=MediaAvBoundReleaseExtractorNode stage=release_committed source_identity=composition-r379.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4018000000 duration_ns=33333333 decoded_sequence=unavailable pts=3011764430 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=9901458479200 group=composition-r379.source.1 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-4018000000 epoch_master_release_ns=5371594700 video_count=15 min_presentation_ns=-4018000000 max_end_ns=-3518000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=17 release_committed_count=1 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.742] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=published source_identity=composition-r379.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4009000000 duration_ns=33333333 decoded_sequence=unavailable pts=204979 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=9901458649100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.742] [info] [graph][runtime.node] source_video_trace node_id=25 node=VideoPreparationNode stage=input source_identity=composition-r379.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4009000000 duration_ns=33333333 decoded_sequence=unavailable pts=204979 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=9901458656600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.742] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=15 audio_count=24 first_video_sequence=15
[2026-11-02 11:33:45.742] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=0
[2026-11-02 11:33:45.742] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb08a00000 drm_prime=0 software_frame=0
[2026-11-02 11:33:45.742] [info] [graph][runtime.node] source_video_trace node_id=25 node=VideoPreparationNode stage=output_attempt source_identity=composition-r379.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4009000000 duration_ns=33333333 decoded_sequence=unavailable pts=204979 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=9901459558800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.743] [info] [graph][runtime.node] source_video_trace node_id=61 node=ActivationReleaseSequencer stage=activation source_identity=composition-r379.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4016000000 duration_ns=33333333 decoded_sequence=unavailable pts=599676260 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=9901459979800 group=composition-r379.source.2 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-4016000000 epoch_master_release_ns=5544292968 video_count=15 min_presentation_ns=-4016000000 max_end_ns=-3516000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.743] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=published source_identity=unavailable identity_link=node_generation_input_source_sequence generation=1 source_sequence=15 presentation_ns=-4016000000 duration_ns=33333333 decoded_sequence=unavailable pts=199892 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=9901460010600 group=composition-r379.source.2 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.743] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=15 audio_count=0 first_video_sequence=15
[2026-11-02 11:33:45.743] [info] [graph][runtime.node] source_video_trace node_id=61 node=ActivationReleaseSequencer stage=release_published source_identity=composition-r379.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4016000000 duration_ns=33333333 decoded_sequence=unavailable pts=599676260 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=9901460397600 group=composition-r379.source.2 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-4016000000 epoch_master_release_ns=5544292968 video_count=15 min_presentation_ns=-4016000000 max_end_ns=-3516000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.743] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_candidate source_identity=composition-r379.source.2.input.video generation=1 steady_ns=9901460572000 source_sequence=15 presentation_ns=-4016000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 11:33:45.743] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=release_committed source_identity=composition-r379.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4016000000 duration_ns=33333333 decoded_sequence=unavailable pts=599676260 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=9901460580100 group=composition-r379.source.2 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-4016000000 epoch_master_release_ns=5366104500 video_count=15 min_presentation_ns=-4016000000 max_end_ns=-3516000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=17 release_committed_count=1 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.744] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=24
[2026-11-02 11:33:45.744] [info] [graph][runtime.node] source_video_trace node_id=19 node=ActivationReleaseSequencer stage=activation source_identity=composition-r379.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4009000000 duration_ns=33333333 decoded_sequence=unavailable pts=614936447 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=9901460748700 group=composition-r379.source.0 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=-4009000000 epoch_master_release_ns=5545091568 video_count=15 min_presentation_ns=-4009000000 max_end_ns=-3509000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.744] [info] [graph][runtime.node] source_video_trace node_id=19 node=ActivationReleaseSequencer stage=release_published source_identity=composition-r379.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4009000000 duration_ns=33333333 decoded_sequence=unavailable pts=614936447 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=9901461314000 group=composition-r379.source.0 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=-4009000000 epoch_master_release_ns=5545091568 video_count=15 min_presentation_ns=-4009000000 max_end_ns=-3509000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.744] [info] [graph][runtime.node] audio_decode_trace stage=first_packet
[2026-11-02 11:33:45.744] [info] [graph][runtime.node] source_video_trace node_id=25 node=VideoPreparationNode stage=published source_identity=unavailable identity_link=node_generation_input_source_sequence generation=1 source_sequence=15 presentation_ns=-4009000000 duration_ns=33333333 decoded_sequence=unavailable pts=204979 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=9901460781300 group=composition-r379.source.0 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.745] [info] [graph][runtime.node] audio_decode_trace stage=first_frame samples=1024 rate=44100
[2026-11-02 11:33:45.745] [info] [graph][runtime.node] source_video_trace node_id=20 node=MediaAvBoundReleaseExtractorNode stage=release_committed source_identity=composition-r379.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4009000000 duration_ns=33333333 decoded_sequence=unavailable pts=614936447 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=9901461867800 group=composition-r379.source.0 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=-4009000000 epoch_master_release_ns=5371179900 video_count=15 min_presentation_ns=-4009000000 max_end_ns=-3509000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=17 release_committed_count=1 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:33:45.745] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=0
[2026-11-02 11:33:45.745] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_candidate source_identity=composition-r379.source.0.input.video generation=1 steady_ns=9901461827800 source_sequence=15 presentation_ns=-4009000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 11:33:45.745] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1 source_master_ns=5547753286 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-11-02 11:33:45.745] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3 source_master_ns=5594193195 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=45346
[2026-11-02 11:33:45.746] [info] [graph][runtime.node] audio_encode_trace stage=first_frame
[2026-11-02 11:33:45.746] [info] [graph][runtime.node] output_preparation stage=transport_activate generation=1 elapsed_ns=221100 planned_bytes=180767114966 planned_items=1 event=scope_exit
[2026-11-02 11:33:45.746] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-r379.source.0.input.video generation=1 steady_ns=9901462930300 source_sequence=15 presentation_ns=-4009000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 11:33:45.746] [info] [graph][runtime.node] video_encode.input_contract format=cuda size=1280x720 buffer_identity=0xb0a000000 drm_prime=0 software_frame=0
[2026-11-02 11:33:45.746] [info] [graph][runtime.node] video_encode.generation_start_key_frame pts=0 generation=1
[2026-11-02 11:33:45.746] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-r379.source.1.input.video generation=1 steady_ns=9901463471400 source_sequence=15 presentation_ns=-4018000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 11:33:45.746] [info] [graph][runtime.node] output_preparation stage=global_sequence generation=1 elapsed_ns=30500 planned_bytes=68968 planned_items=2154 event=scope_exit
[2026-11-02 11:33:45.747] [info] [graph][runtime.node] video_encode.first_frame pts=0 generation=attached
[2026-11-02 11:33:45.747] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-r379.source.2.input.video generation=1 steady_ns=9901463851500 source_sequence=15 presentation_ns=-4016000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 11:33:45.747] [info] [graph][runtime.node] output_preparation stage=transport_buffer generation=1 elapsed_ns=1289800 planned_bytes=180767114966 planned_items=1 event=scope_exit
[2026-11-02 11:33:45.748] [info] [graph][runtime.node] output_preparation stage=ts_packetizer generation=1 elapsed_ns=85700 planned_bytes=180765122912 planned_items=61257 event=scope_exit
[2026-11-02 11:33:45.748] [info] [graph][runtime.node] output_preparation stage=ts_mux generation=1 elapsed_ns=309500 planned_bytes=945416 planned_items=59086 event=scope_exit
[2026-11-02 11:33:45.748] [info] [graph][runtime.node] mpegts_emission_plan generation=1 mode=canonical_access_unit_window access_unit_window_ns=100000000 video_initial_service_window_ns=13990650 audio_initial_service_window_ns=13990650 packet_size_bytes=188 maximum_payload_bytes=1316 per_datagram_overhead_bytes=0
[2026-11-02 11:33:45.753] [info] [graph][runtime.node] video_encode.first_submit result=accepted
[2026-11-02 11:33:45.753] [info] [graph][runtime.node] video_encode.first_packet pts=0 dts=0 duration=0 size=108357 side_data_elems=1 received_footprint=108365 side_data[0]=Quality stats:8
[2026-11-02 11:33:45.753] [info] [graph][runtime.node] video_encode.first_packet_footprint bytes=108365 wrapped_size=108357 wrapped_side_data_elems=1
[2026-11-02 11:33:45.753] [info] [graph][runtime.node] av_scheduler_trace stage=first_video_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=5546965668 dispatch_master_ns=5546965668 now_ns=5387907200 node=74 identity_kind=output identity="composition-r379.output" canonical_sequence=1 canonical_created_steady_ns=9901470145900 head_observed_steady_ns=9901470174700 codec_receive_begin_steady_ns=9901469881700 codec_receive_end_steady_ns=9901469882400
[2026-11-02 11:33:45.753] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=5388034300 present_generation=1 present_canonical_target_ns=0 present_canonical_dispatch_ns=0 present_target_master_ns=5546965668 present_dispatch_master_ns=5546965668
[2026-11-02 11:33:45.794] [info] [graph][runtime.node] audio_encode_trace stage=first_packet
[2026-11-02 11:33:45.794] [info] [graph][runtime.node] av_scheduler_trace stage=first_audio_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=5546965668 dispatch_master_ns=5546965668 now_ns=5429183900 node=74 identity_kind=output identity="composition-r379.output" canonical_sequence=1 canonical_created_steady_ns=9901511411600 head_observed_steady_ns=9901511451300 codec_receive_begin_steady_ns=unavailable codec_receive_end_steady_ns=unavailable
[2026-11-02 11:33:45.960] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=running detail="" description=""
[2026-11-02 11:33:46.373] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=48 source_master_ns=6639091154 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=90692
[2026-11-02 11:33:47.395] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=92 source_master_ns=7660769159 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=136038
[2026-11-02 11:33:48.416] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=136 source_master_ns=8682447164 raw_phase_ns=13607 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=181384
[2026-11-02 11:33:49.438] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=180 source_master_ns=9704125167 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=226730
[2026-11-02 11:33:50.362] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3683248664 ntp_seconds=4002579230 ntp_fraction=1554778161 arrival_ns=9906078913600
[2026-11-02 11:33:50.362] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3683248664
[2026-11-02 11:33:50.362] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:33:50.376] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=653832613 ntp_seconds=4002579230 ntp_fraction=1610612736 arrival_ns=9906092529400
[2026-11-02 11:33:50.376] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=653832613
[2026-11-02 11:33:50.376] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:33:50.391] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1330409089 ntp_seconds=4002579230 ntp_fraction=1679332212 arrival_ns=9906107888800
[2026-11-02 11:33:50.391] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1330409089
[2026-11-02 11:33:50.391] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:33:50.391] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=872284855 ntp_seconds=4002579230 ntp_fraction=1679332212 arrival_ns=9906108070100
[2026-11-02 11:33:50.392] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=872284855
[2026-11-02 11:33:50.392] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:33:50.483] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=225 source_master_ns=10749023126 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=272076
[2026-11-02 11:33:51.505] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=269 source_master_ns=11770701131 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=317422
[2026-11-02 11:33:52.526] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=313 source_master_ns=12792379135 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=362768
[2026-11-02 11:33:53.571] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=358 source_master_ns=13837277094 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=408114
[2026-11-02 11:33:54.593] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=402 source_master_ns=14858955099 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=453460
[2026-11-02 11:33:55.386] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3683248664 ntp_seconds=4002579235 ntp_fraction=1657857376 arrival_ns=9911103453700
[2026-11-02 11:33:55.387] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3683248664
[2026-11-02 11:33:55.387] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:33:55.387] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=653832613 ntp_seconds=4002579235 ntp_fraction=1657857376 arrival_ns=9911103411900
[2026-11-02 11:33:55.388] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=653832613
[2026-11-02 11:33:55.388] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:33:55.417] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=872284855 ntp_seconds=4002579235 ntp_fraction=1791001362 arrival_ns=9911133913000
[2026-11-02 11:33:55.417] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1330409089 ntp_seconds=4002579235 ntp_fraction=1791001362 arrival_ns=9911134255400
[2026-11-02 11:33:55.417] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=872284855
[2026-11-02 11:33:55.418] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1330409089
[2026-11-02 11:33:55.418] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:33:55.418] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:33:55.615] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=446 source_master_ns=15880633104 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=498806
[2026-11-02 11:33:56.636] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=490 source_master_ns=16902311108 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=544152
[2026-11-02 11:33:57.681] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=535 source_master_ns=17947209067 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=589498
[2026-11-02 11:33:58.703] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=579 source_master_ns=18968887072 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=634844
[2026-11-02 11:33:59.725] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=623 source_master_ns=19990565076 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=680190
[2026-11-02 11:34:00.396] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3683248664 ntp_seconds=4002579240 ntp_fraction=1696512081 arrival_ns=9916112674100
[2026-11-02 11:34:00.396] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3683248664
[2026-11-02 11:34:00.396] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:00.411] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=653832613 ntp_seconds=4002579240 ntp_fraction=1765231558 arrival_ns=9916127884700
[2026-11-02 11:34:00.412] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=653832613
[2026-11-02 11:34:00.412] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:00.425] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=872284855 ntp_seconds=4002579240 ntp_fraction=1825361100 arrival_ns=9916142241400
[2026-11-02 11:34:00.425] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1330409089 ntp_seconds=4002579240 ntp_fraction=1825361100 arrival_ns=9916142280500
[2026-11-02 11:34:00.426] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1330409089
[2026-11-02 11:34:00.426] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:00.426] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=872284855
[2026-11-02 11:34:00.427] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:00.769] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=668 source_master_ns=21035463036 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=725536
[2026-11-02 11:34:01.791] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=712 source_master_ns=22057141040 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=770882
[2026-11-02 11:34:02.813] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=756 source_master_ns=23078819045 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=816228
[2026-11-02 11:34:03.834] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=800 source_master_ns=24100497049 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=861574
[2026-11-02 11:34:04.879] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=845 source_master_ns=25145395008 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=906920
[2026-11-02 11:34:05.409] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3683248664 ntp_seconds=4002579245 ntp_fraction=1756641624 arrival_ns=9921126455600
[2026-11-02 11:34:05.410] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3683248664
[2026-11-02 11:34:05.410] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:05.440] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=653832613 ntp_seconds=4002579245 ntp_fraction=1885490642 arrival_ns=9921156656800
[2026-11-02 11:34:05.440] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=653832613
[2026-11-02 11:34:05.440] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:05.455] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=872284855 ntp_seconds=4002579245 ntp_fraction=1954210119 arrival_ns=9921171843400
[2026-11-02 11:34:05.455] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1330409089 ntp_seconds=4002579245 ntp_fraction=1954210119 arrival_ns=9921171943200
[2026-11-02 11:34:05.455] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1330409089
[2026-11-02 11:34:05.456] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:05.455] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=872284855
[2026-11-02 11:34:05.456] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:05.901] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=889 source_master_ns=26167073013 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=952266
[2026-11-02 11:34:06.923] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=933 source_master_ns=27188751017 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=997612
[2026-11-02 11:34:07.967] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=978 source_master_ns=28233648977 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1042958
[2026-11-02 11:34:08.989] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1022 source_master_ns=29255326981 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1088304
[2026-11-02 11:34:09.361] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=28995634800 present_generation=1 present_canonical_target_ns=23568253968 present_canonical_dispatch_ns=23568253968 present_target_master_ns=29115219636 present_dispatch_master_ns=29115219636
[2026-11-02 11:34:09.364] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=28999142400 present_generation=1 present_canonical_target_ns=23600000000 present_canonical_dispatch_ns=23600000000 present_target_master_ns=29146965668 present_dispatch_master_ns=29146965668
[2026-11-02 11:34:10.011] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1066 source_master_ns=30277004986 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1133650
[2026-11-02 11:34:10.420] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3683248664 ntp_seconds=4002579250 ntp_fraction=1803886264 arrival_ns=9926136804300
[2026-11-02 11:34:10.420] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3683248664
[2026-11-02 11:34:10.420] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:10.444] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=653832613 ntp_seconds=4002579250 ntp_fraction=1906965479 arrival_ns=9926161140200
[2026-11-02 11:34:10.444] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=653832613
[2026-11-02 11:34:10.445] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:10.460] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1330409089 ntp_seconds=4002579250 ntp_fraction=1975684956 arrival_ns=9926176941400
[2026-11-02 11:34:10.460] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1330409089
[2026-11-02 11:34:10.460] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:10.461] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=872284855 ntp_seconds=4002579250 ntp_fraction=1979979923 arrival_ns=9926177709400
[2026-11-02 11:34:10.461] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=872284855
[2026-11-02 11:34:10.461] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:11.033] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1110 source_master_ns=31298682990 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1178996
[2026-11-02 11:34:12.078] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1155 source_master_ns=32343580949 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1224342
[2026-11-02 11:34:13.099] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1199 source_master_ns=33365258954 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1269688
[2026-11-02 11:34:14.121] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1243 source_master_ns=34386936958 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1315034
[2026-11-02 11:34:15.165] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1288 source_master_ns=35431834918 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1360380
[2026-11-02 11:34:15.424] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3683248664 ntp_seconds=4002579255 ntp_fraction=1821066133 arrival_ns=9931140796800
[2026-11-02 11:34:15.424] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3683248664
[2026-11-02 11:34:15.424] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:15.482] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1330409089 ntp_seconds=4002579255 ntp_fraction=2070174236 arrival_ns=9931199403300
[2026-11-02 11:34:15.483] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1330409089
[2026-11-02 11:34:15.483] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:15.483] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=653832613 ntp_seconds=4002579255 ntp_fraction=2070174236 arrival_ns=9931199654800
[2026-11-02 11:34:15.483] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=653832613
[2026-11-02 11:34:15.483] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:15.497] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=872284855 ntp_seconds=4002579255 ntp_fraction=2134598746 arrival_ns=9931214322100
[2026-11-02 11:34:15.498] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=872284855
[2026-11-02 11:34:15.498] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:16.188] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1332 source_master_ns=36453512922 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1405726
[2026-11-02 11:34:17.209] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1376 source_master_ns=37475190927 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1451072
[2026-11-02 11:34:18.231] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1420 source_master_ns=38496868931 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1496418
[2026-11-02 11:34:19.275] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1465 source_master_ns=39541766890 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1541764
[2026-11-02 11:34:20.297] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1509 source_master_ns=40563444895 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1587110
[2026-11-02 11:34:20.443] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3683248664 ntp_seconds=4002579260 ntp_fraction=1902670512 arrival_ns=9936159691500
[2026-11-02 11:34:20.443] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3683248664
[2026-11-02 11:34:20.443] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:20.489] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1330409089 ntp_seconds=4002579260 ntp_fraction=2100239007 arrival_ns=9936205834900
[2026-11-02 11:34:20.489] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1330409089
[2026-11-02 11:34:20.489] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:20.505] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=653832613 ntp_seconds=4002579260 ntp_fraction=2168958484 arrival_ns=9936221680600
[2026-11-02 11:34:20.505] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=653832613
[2026-11-02 11:34:20.505] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:20.519] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=872284855 ntp_seconds=4002579260 ntp_fraction=2224793059 arrival_ns=9936235634100
[2026-11-02 11:34:20.519] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=872284855
[2026-11-02 11:34:20.519] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:20.809] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=40443582300 present_generation=1 present_canonical_target_ns=35015691609 present_canonical_dispatch_ns=35015691609 present_target_master_ns=40562657277 present_dispatch_master_ns=40562657277
[2026-11-02 11:34:21.004] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=40638563100 present_generation=1 present_canonical_target_ns=35233333333 present_canonical_dispatch_ns=35233333333 present_target_master_ns=40780299001 present_dispatch_master_ns=40780299001
[2026-11-02 11:34:21.319] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1553 source_master_ns=41585122899 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1632456
[2026-11-02 11:34:22.364] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1598 source_master_ns=42630020859 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1677802
[2026-11-02 11:34:23.386] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1642 source_master_ns=43651698863 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1723148
[2026-11-02 11:34:24.407] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1686 source_master_ns=44673376868 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1768494
[2026-11-02 11:34:25.429] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1730 source_master_ns=45695054872 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1813840
[2026-11-02 11:34:25.450] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3683248664 ntp_seconds=4002579265 ntp_fraction=1928440315 arrival_ns=9941166599300
[2026-11-02 11:34:25.450] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3683248664
[2026-11-02 11:34:25.450] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:25.507] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=653832613 ntp_seconds=4002579265 ntp_fraction=2173253451 arrival_ns=9941223489100
[2026-11-02 11:34:25.507] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=653832613
[2026-11-02 11:34:25.507] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:25.522] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1330409089 ntp_seconds=4002579265 ntp_fraction=2237677961 arrival_ns=9941238621700
[2026-11-02 11:34:25.522] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1330409089
[2026-11-02 11:34:25.522] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:25.522] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=872284855 ntp_seconds=4002579265 ntp_fraction=2237677961 arrival_ns=9941238607000
[2026-11-02 11:34:25.522] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=872284855
[2026-11-02 11:34:25.523] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:26.474] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1775 source_master_ns=46739952831 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1859186
[2026-11-02 11:34:27.495] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1819 source_master_ns=47761630836 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1904532
[2026-11-02 11:34:28.517] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1863 source_master_ns=48783308841 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1949878
[2026-11-02 11:34:29.562] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1908 source_master_ns=49828206800 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1995224
[2026-11-02 11:34:30.460] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3683248664 ntp_seconds=4002579270 ntp_fraction=1971389988 arrival_ns=9946176480600
[2026-11-02 11:34:30.460] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3683248664
[2026-11-02 11:34:30.460] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:30.548] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1330409089 ntp_seconds=4002579270 ntp_fraction=2353642078 arrival_ns=9946264796800
[2026-11-02 11:34:30.548] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=653832613 ntp_seconds=4002579270 ntp_fraction=2353642078 arrival_ns=9946264826400
[2026-11-02 11:34:30.548] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=653832613
[2026-11-02 11:34:30.548] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1330409089
[2026-11-02 11:34:30.549] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:30.549] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:30.563] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=872284855 ntp_seconds=4002579270 ntp_fraction=2418066587 arrival_ns=9946280222800
[2026-11-02 11:34:30.564] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=872284855
[2026-11-02 11:34:30.564] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:30.584] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1952 source_master_ns=50849884804 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2040570
[2026-11-02 11:34:30.770] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=50404367500 present_generation=1 present_canonical_target_ns=44977052154 present_canonical_dispatch_ns=44977052154 present_target_master_ns=50524017822 present_dispatch_master_ns=50524017822
[2026-11-02 11:34:30.780] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=50414942900 present_generation=1 present_canonical_target_ns=45000000000 present_canonical_dispatch_ns=45000000000 present_target_master_ns=50546965668 present_dispatch_master_ns=50546965668
[2026-11-02 11:34:30.793] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=50428078800 present_generation=1 present_canonical_target_ns=45000272108 present_canonical_dispatch_ns=45000272108 present_target_master_ns=50547237776 present_dispatch_master_ns=50547237776
[2026-11-02 11:34:30.894] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=50528543000 present_generation=1 present_canonical_target_ns=45100000000 present_canonical_dispatch_ns=45100000000 present_target_master_ns=50646965668 present_dispatch_master_ns=50646965668
[2026-11-02 11:34:31.605] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1996 source_master_ns=51871562809 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2085916
[2026-11-02 11:34:32.627] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2040 source_master_ns=52893240813 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2131262
[2026-11-02 11:34:33.672] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2085 source_master_ns=53938138772 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2176608
[2026-11-02 11:34:34.160] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=53794989600 present_generation=1 present_canonical_target_ns=48367165532 present_canonical_dispatch_ns=48367165532 present_target_master_ns=53914131200 present_dispatch_master_ns=53914131200
[2026-11-02 11:34:34.283] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=53917437600 present_generation=1 present_canonical_target_ns=48500000000 present_canonical_dispatch_ns=48500000000 present_target_master_ns=54046965668 present_dispatch_master_ns=54046965668
[2026-11-02 11:34:34.693] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2129 source_master_ns=54959816777 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2221954
[2026-11-02 11:34:35.482] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3683248664 ntp_seconds=4002579275 ntp_fraction=2065879269 arrival_ns=9951198507900
[2026-11-02 11:34:35.482] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3683248664
[2026-11-02 11:34:35.482] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:35.557] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1330409089 ntp_seconds=4002579275 ntp_fraction=2392296783 arrival_ns=9951273738900
[2026-11-02 11:34:35.557] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1330409089
[2026-11-02 11:34:35.557] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:35.573] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=653832613 ntp_seconds=4002579275 ntp_fraction=2461016260 arrival_ns=9951289734800
[2026-11-02 11:34:35.573] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=653832613
[2026-11-02 11:34:35.573] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:35.588] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=872284855 ntp_seconds=4002579275 ntp_fraction=2525440770 arrival_ns=9951304970100
[2026-11-02 11:34:35.588] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=872284855
[2026-11-02 11:34:35.588] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:35.715] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2173 source_master_ns=55981494782 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2267300
[2026-11-02 11:34:36.760] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2218 source_master_ns=57026392741 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2312646
[2026-11-02 11:34:37.782] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2262 source_master_ns=58048070745 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2357992
[2026-11-02 11:34:38.804] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2306 source_master_ns=59069748750 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2403338
[2026-11-02 11:34:39.825] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2350 source_master_ns=60091426754 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2448684
[2026-11-02 11:34:40.494] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3683248664 ntp_seconds=4002579280 ntp_fraction=2117418876 arrival_ns=9956210591400
[2026-11-02 11:34:40.494] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3683248664
[2026-11-02 11:34:40.494] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:40.585] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=653832613 ntp_seconds=4002579280 ntp_fraction=2512555868 arrival_ns=9956301667500
[2026-11-02 11:34:40.585] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=653832613
[2026-11-02 11:34:40.585] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1330409089 ntp_seconds=4002579280 ntp_fraction=2512555868 arrival_ns=9956302011500
[2026-11-02 11:34:40.585] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:40.585] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1330409089
[2026-11-02 11:34:40.586] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:40.643] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=872284855 ntp_seconds=4002579280 ntp_fraction=2761663971 arrival_ns=9956360343000
[2026-11-02 11:34:40.643] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=872284855
[2026-11-02 11:34:40.644] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:40.870] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2395 source_master_ns=61136324714 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2494030
[2026-11-02 11:34:41.892] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2439 source_master_ns=62158002718 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2539376
[2026-11-02 11:34:42.914] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2483 source_master_ns=63179680723 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2584722
[2026-11-02 11:34:43.935] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2527 source_master_ns=64201358727 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2630068
[2026-11-02 11:34:44.980] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2572 source_master_ns=65246256686 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2675414
[2026-11-02 11:34:45.513] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3683248664 ntp_seconds=4002579285 ntp_fraction=2199023255 arrival_ns=9961229591300
[2026-11-02 11:34:45.513] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3683248664
[2026-11-02 11:34:45.513] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:45.590] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1330409089 ntp_seconds=4002579285 ntp_fraction=2534030704 arrival_ns=9961307318000
[2026-11-02 11:34:45.591] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1330409089
[2026-11-02 11:34:45.591] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:45.618] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=653832613 ntp_seconds=4002579285 ntp_fraction=2654289788 arrival_ns=9961335460500
[2026-11-02 11:34:45.619] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=653832613
[2026-11-02 11:34:45.619] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:45.677] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=872284855 ntp_seconds=4002579285 ntp_fraction=2907692859 arrival_ns=9961394278500
[2026-11-02 11:34:45.678] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=872284855
[2026-11-02 11:34:45.678] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:46.002] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2616 source_master_ns=66267934691 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2720760
[2026-11-02 11:34:47.023] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2660 source_master_ns=67289612695 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2766106
[2026-11-02 11:34:48.068] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2705 source_master_ns=68334510655 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2811452
[2026-11-02 11:34:49.090] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2749 source_master_ns=69356188659 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2856798
[2026-11-02 11:34:50.111] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2793 source_master_ns=70377866664 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2902144
[2026-11-02 11:34:50.525] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3683248664 ntp_seconds=4002579290 ntp_fraction=2254857830 arrival_ns=9966242229700
[2026-11-02 11:34:50.526] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3683248664
[2026-11-02 11:34:50.526] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:50.614] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1330409089 ntp_seconds=4002579290 ntp_fraction=2637109919 arrival_ns=9966331199800
[2026-11-02 11:34:50.614] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1330409089
[2026-11-02 11:34:50.615] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:50.639] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=653832613 ntp_seconds=4002579290 ntp_fraction=2744484102 arrival_ns=9966356279200
[2026-11-02 11:34:50.640] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=653832613
[2026-11-02 11:34:50.640] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:50.686] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=872284855 ntp_seconds=4002579290 ntp_fraction=2946347565 arrival_ns=9966402934900
[2026-11-02 11:34:50.686] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=872284855
[2026-11-02 11:34:50.687] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:51.133] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2837 source_master_ns=71399544668 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2947490
[2026-11-02 11:34:52.179] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2882 source_master_ns=72444442627 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2992836
[2026-11-02 11:34:53.200] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2926 source_master_ns=73466120632 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3038182
[2026-11-02 11:34:54.222] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2970 source_master_ns=74487798636 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3083528
[2026-11-02 11:34:55.267] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3015 source_master_ns=75532696596 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3128874
[2026-11-02 11:34:55.543] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3683248664 ntp_seconds=4002579295 ntp_fraction=2332167241 arrival_ns=9971260221700
[2026-11-02 11:34:55.544] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3683248664
[2026-11-02 11:34:55.544] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:55.621] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1330409089 ntp_seconds=4002579295 ntp_fraction=2667174690 arrival_ns=9971338099600
[2026-11-02 11:34:55.621] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1330409089
[2026-11-02 11:34:55.621] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:55.648] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=653832613 ntp_seconds=4002579295 ntp_fraction=2783138807 arrival_ns=9971365390900
[2026-11-02 11:34:55.648] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=653832613
[2026-11-02 11:34:55.649] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:55.694] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=872284855 ntp_seconds=4002579295 ntp_fraction=2980707303 arrival_ns=9971411514200
[2026-11-02 11:34:55.695] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=872284855
[2026-11-02 11:34:55.695] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:34:56.288] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3059 source_master_ns=76554374600 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3174220
[2026-11-02 11:34:57.310] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3103 source_master_ns=77576052605 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3219566
[2026-11-02 11:34:58.331] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3147 source_master_ns=78597730609 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3264912
[2026-11-02 11:34:59.376] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3192 source_master_ns=79642628568 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3310258
[2026-11-02 11:35:00.398] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3236 source_master_ns=80664306573 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3355604
[2026-11-02 11:35:00.556] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3683248664 ntp_seconds=4002579300 ntp_fraction=2388001816 arrival_ns=9976273279500
[2026-11-02 11:35:00.557] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3683248664
[2026-11-02 11:35:00.557] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:35:00.648] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1330409089 ntp_seconds=4002579300 ntp_fraction=2783138807 arrival_ns=9976365164600
[2026-11-02 11:35:00.649] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1330409089
[2026-11-02 11:35:00.649] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:35:00.680] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=653832613 ntp_seconds=4002579300 ntp_fraction=2916282793 arrival_ns=9976396551700
[2026-11-02 11:35:00.680] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=653832613
[2026-11-02 11:35:00.680] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:35:00.739] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=872284855 ntp_seconds=4002579300 ntp_fraction=3173980831 arrival_ns=9976456291800
[2026-11-02 11:35:00.739] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=872284855
[2026-11-02 11:35:00.740] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:35:01.420] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3280 source_master_ns=81685984577 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3400950
[2026-11-02 11:35:02.465] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3325 source_master_ns=82730882537 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3446296
[2026-11-02 11:35:02.535] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=82169690000 present_generation=1 present_canonical_target_ns=76741950113 present_canonical_dispatch_ns=76741950113 present_target_master_ns=82288915781 present_dispatch_master_ns=82288915781
[2026-11-02 11:35:02.542] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=82176648000 present_generation=1 present_canonical_target_ns=76766666666 present_canonical_dispatch_ns=76766666666 present_target_master_ns=82313632334 present_dispatch_master_ns=82313632334
[2026-11-02 11:35:02.581] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=82216205900 present_generation=1 present_canonical_target_ns=76788390022 present_canonical_dispatch_ns=76788390022 present_target_master_ns=82335355690 present_dispatch_master_ns=82335355690
[2026-11-02 11:35:02.683] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=82318206000 present_generation=1 present_canonical_target_ns=76900000000 present_canonical_dispatch_ns=76900000000 present_target_master_ns=82446965668 present_dispatch_master_ns=82446965668
[2026-11-02 11:35:03.486] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3369 source_master_ns=83752560541 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3491642
[2026-11-02 11:35:04.508] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3413 source_master_ns=84774238546 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3536988
[2026-11-02 11:35:05.530] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3457 source_master_ns=85795916550 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3582334
[2026-11-02 11:35:05.573] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3683248664 ntp_seconds=4002579305 ntp_fraction=2461016260 arrival_ns=9981289912300
[2026-11-02 11:35:05.573] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3683248664
[2026-11-02 11:35:05.574] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:35:05.649] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1330409089 ntp_seconds=4002579305 ntp_fraction=2787433775 arrival_ns=9981365947000
[2026-11-02 11:35:05.649] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1330409089
[2026-11-02 11:35:05.650] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:35:05.680] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=653832613 ntp_seconds=4002579305 ntp_fraction=2920577761 arrival_ns=9981396689700
[2026-11-02 11:35:05.680] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=653832613
[2026-11-02 11:35:05.680] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:35:05.759] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=872284855 ntp_seconds=4002579305 ntp_fraction=3255585210 arrival_ns=9981475532200
[2026-11-02 11:35:05.759] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=872284855
[2026-11-02 11:35:05.759] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:35:06.575] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3502 source_master_ns=86840814509 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3627680
[2026-11-02 11:35:07.597] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3546 source_master_ns=87862492514 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3673026
[2026-11-02 11:35:08.618] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3590 source_master_ns=88884170519 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3718372
[2026-11-02 11:35:09.663] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3635 source_master_ns=89929068478 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3763718
[2026-11-02 11:35:10.583] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3683248664 ntp_seconds=4002579310 ntp_fraction=2503965933 arrival_ns=9986299725900
[2026-11-02 11:35:10.583] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3683248664
[2026-11-02 11:35:10.583] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:35:10.657] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1330409089 ntp_seconds=4002579310 ntp_fraction=2821793513 arrival_ns=9986374088800
[2026-11-02 11:35:10.657] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1330409089
[2026-11-02 11:35:10.658] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:35:10.685] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3679 source_master_ns=90950746482 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3809064
[2026-11-02 11:35:10.719] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=653832613 ntp_seconds=4002579310 ntp_fraction=3088081485 arrival_ns=9986435958300
[2026-11-02 11:35:10.719] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=653832613
[2026-11-02 11:35:10.720] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:35:10.761] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=872284855 ntp_seconds=4002579310 ntp_fraction=3268470112 arrival_ns=9986477955100
[2026-11-02 11:35:10.761] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=872284855
[2026-11-02 11:35:10.761] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:35:11.706] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3723 source_master_ns=91972424487 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3854410
[2026-11-02 11:35:12.728] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3767 source_master_ns=92994102491 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3899756
[2026-11-02 11:35:13.773] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3812 source_master_ns=94039000450 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3945102
[2026-11-02 11:35:14.098] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=93732999500 present_generation=1 present_canonical_target_ns=88305487528 present_canonical_dispatch_ns=88305487528 present_target_master_ns=93852453196 present_dispatch_master_ns=93852453196
[2026-11-02 11:35:14.100] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=93734897100 present_generation=1 present_canonical_target_ns=88333333333 present_canonical_dispatch_ns=88333333333 present_target_master_ns=93880299001 present_dispatch_master_ns=93880299001
[2026-11-02 11:35:14.261] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=93895727600 present_generation=1 present_canonical_target_ns=88468027210 present_canonical_dispatch_ns=88468027210 present_target_master_ns=94014992878 present_dispatch_master_ns=94014992878
[2026-11-02 11:35:14.263] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=93897585600 present_generation=1 present_canonical_target_ns=88500000000 present_canonical_dispatch_ns=88500000000 present_target_master_ns=94046965668 present_dispatch_master_ns=94046965668
[2026-11-02 11:35:14.794] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3856 source_master_ns=95060678455 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3990448
[2026-11-02 11:35:15.587] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3683248664 ntp_seconds=4002579315 ntp_fraction=2516850835 arrival_ns=9991303268400
[2026-11-02 11:35:15.587] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3683248664
[2026-11-02 11:35:15.587] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:35:15.688] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1330409089 ntp_seconds=4002579315 ntp_fraction=2954937499 arrival_ns=9991405410900
[2026-11-02 11:35:15.689] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1330409089
[2026-11-02 11:35:15.689] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[CLI] [2026-11-02 11:35:15.749] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=653832613 ntp_seconds=4002579315 ntp_fraction=3216930504 arrival_ns=9991465736500
runtime report: queued=601, peakQueued=902, threads=81, processThreads=129, workers=75, workerProgress=246440, workerProcessCalls=969819, workerWaits=723369, workerWakeups=716462, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=294, logicalProcessors=22, averageSystemMachineCpuPercent=12.087191, averageProcessMachineCpuPercent=2.026490, peakProcessMachineCpuPercent=5.625000, averageProcessSingleCoreCpuPercent=44.582791, peakProcessSingleCoreCpuPercent=123.750000, initialWorkingSetBytes=208842752, workingSetBytes=267452416, peakWorkingSetBytes=268812288, totalPushed=213948, totalPopped=213347, droppedBuffers=0, encodedPacketsPushed=30164, encodedPacketsPopped=30163, backpressureItems=123, graphPayloadCurrentBytes=9828881, graphPayloadHighWaterBytes=16030500, graphPayloadCurrentObjects=717, graphPayloadHighWaterObjects=730, graphPayloadReservations=84941, graphPayloadReleases=84224, graphPayloadPressureFailures=0, lastInputReceivedAtNs=9991436708500
[2026-11-02 11:35:15.749] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=653832613
[2026-11-02 11:35:15.749] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:35:15.765] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=872284855 ntp_seconds=4002579315 ntp_fraction=3285649981 arrival_ns=9991481960600
[2026-11-02 11:35:15.765] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=872284855
[2026-11-02 11:35:15.765] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:35:15.816] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3900 source_master_ns=96082356460 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4035794
[2026-11-02 11:35:16.861] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3945 source_master_ns=97127254419 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4081140
[2026-11-02 11:35:17.883] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3989 source_master_ns=98148932423 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4126486
[2026-11-02 11:35:18.905] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4033 source_master_ns=99170610428 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4171832
[2026-11-02 11:35:19.926] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4077 source_master_ns=100192288432 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4217178
[2026-11-02 11:35:20.589] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3683248664 ntp_seconds=4002579320 ntp_fraction=2529735737 arrival_ns=9996306402200
[2026-11-02 11:35:20.590] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3683248664
[2026-11-02 11:35:20.590] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:35:20.740] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1330409089 ntp_seconds=4002579320 ntp_fraction=3173980831 arrival_ns=9996456670500
[2026-11-02 11:35:20.740] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1330409089
[2026-11-02 11:35:20.741] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:35:20.754] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=653832613 ntp_seconds=4002579320 ntp_fraction=3238405341 arrival_ns=9996471110700
[2026-11-02 11:35:20.754] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=653832613
[2026-11-02 11:35:20.755] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:35:20.785] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=872284855 ntp_seconds=4002579320 ntp_fraction=3371549327 arrival_ns=9996501794200
[2026-11-02 11:35:20.785] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=872284855
[2026-11-02 11:35:20.785] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:35:20.971] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4122 source_master_ns=101237186392 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4262524
[2026-11-02 11:35:21.993] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4166 source_master_ns=102258864396 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4307870
[2026-11-02 11:35:23.014] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4210 source_master_ns=103280542401 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4353216
[2026-11-02 11:35:24.059] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4255 source_master_ns=104325440360 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4398562
[2026-11-02 11:35:25.081] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4299 source_master_ns=105347118364 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4443908
[2026-11-02 11:35:25.621] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3683248664 ntp_seconds=4002579325 ntp_fraction=2662879723 arrival_ns=10001337615500
[2026-11-02 11:35:25.621] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3683248664
[2026-11-02 11:35:25.621] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:35:25.753] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1330409089 ntp_seconds=4002579325 ntp_fraction=3229815406 arrival_ns=10001469600800
[2026-11-02 11:35:25.753] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1330409089
[2026-11-02 11:35:25.753] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:35:25.781] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=653832613 ntp_seconds=4002579325 ntp_fraction=3354369458 arrival_ns=10001498443700
[2026-11-02 11:35:25.782] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=653832613
[2026-11-02 11:35:25.782] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:35:25.794] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=872284855 ntp_seconds=4002579325 ntp_fraction=3410204033 arrival_ns=10001511184400
[2026-11-02 11:35:25.794] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=872284855
[2026-11-02 11:35:25.795] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:35:26.103] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4343 source_master_ns=106368796369 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4489254
[2026-11-02 11:35:27.124] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4387 source_master_ns=107390474373 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4534600
[2026-11-02 11:35:28.169] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4432 source_master_ns=108435372333 raw_phase_ns=13606 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4579946
[2026-11-02 11:35:29.191] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4476 source_master_ns=109457050337 raw_phase_ns=13605 filtered_phase_ns=13606 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4625292
[2026-11-02 11:35:29.911] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=109545536200 present_generation=1 present_canonical_target_ns=104118276643 present_canonical_dispatch_ns=104118276643 present_target_master_ns=109665242311 present_dispatch_master_ns=109665242311
[2026-11-02 11:35:29.934] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=109568404100 present_generation=1 present_canonical_target_ns=104133333333 present_canonical_dispatch_ns=104133333333 present_target_master_ns=109680299001 present_dispatch_master_ns=109680299001
[2026-11-02 11:35:29.934] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=109569067200 present_generation=1 present_canonical_target_ns=104141496598 present_canonical_dispatch_ns=104141496598 present_target_master_ns=109688462266 present_dispatch_master_ns=109688462266
[2026-11-02 11:35:30.079] [info] [graph][runtime.node] av_scheduler_trace stage=late_output_commit stream=audio generation=1 canonical_dispatch_ns=104141496598 emit_ns=109588462266 dispatch_ns=109688462266 committed_at_ns=109713616000 canonical_sequence=4486 head_missing_valid=1 head_missing_intervals=1 head_missing_completed_ns=144532200 head_missing_active=0 head_missing_last_begin_ns=109569066800 head_missing_last_end_ns=109713599000 head_resumed_generation=1 head_resumed_sequence=3126 other_generation=1 other_sequence=3126 other_canonical_dispatch_ns=104166666666 other_arrived_ns=109713597500 other_dispatch_master_ns=109713632334 head_arrived_ns=109569057300 scheduled_at_ns=109713603700
[2026-11-02 11:35:30.079] [info] [graph][runtime.node] av_scheduler_trace stage=late_audio_head generation=1 mapping_epoch_generation=1 canonical_target_ns=104164716553 canonical_dispatch_ns=104164716553 target_master_ns=109711682221 dispatch_master_ns=109711682221 now_ns=109713949800 node=74 identity_kind=output identity="composition-r379.output" canonical_sequence=4487 canonical_created_steady_ns=10005674648700 head_observed_steady_ns=10005796217300 codec_receive_begin_steady_ns=unavailable codec_receive_end_steady_ns=unavailable
[2026-11-02 11:35:30.079] [info] [graph][runtime.node] au_processing_trace stage=pending_au_failure generation=1 stream=audio canonical_sequence=4486 emit_master_ns=109588462266 dispatch_master_ns=109688462266 ready_master_ns=109713963800 scheduled_created_steady_ns=10005795873100 adapter_dequeued_steady_ns=10005795915500 adapted_created_steady_ns=10005795916500 write_entered_steady_ns=10005796229600 session_ready_steady_ns=10005796231300 codec_identity=unavailable
[2026-11-02 11:35:30.079] [info] [graph][runtime.node] ts_prefix_commit_summary stage=current_au_failure generation=1 stream=audio canonical_sequence=4486 emit_master_ns=109588462266 dispatch_master_ns=109688462266 published=0 completed=0 samples=0 would_block=0 incomplete=0 publish_to_protocol_commit_completed_total_ns=0 publish_to_protocol_commit_completed_max_ns=0 publish_to_protocol_commit_completed_max_prefix_offset=0 protocol_commit_completed_to_mux_resume_total_ns=0 protocol_commit_completed_to_mux_resume_max_ns=0 protocol_commit_completed_to_mux_resume_max_prefix_offset=0
[2026-11-02 11:35:30.079] [info] [graph][runtime.node] au_processing_trace stage=previous_completed_au_trace generation=1 stream=video canonical_sequence=3125 emit_master_ns=109580299001 dispatch_master_ns=109680299001 ready_master_ns=109569309200 scheduled_created_steady_ns=10005651327200 adapter_dequeued_steady_ns=10005651552300 adapted_created_steady_ns=10005651552900 write_entered_steady_ns=10005651575200 session_ready_steady_ns=10005651576700 codec_receive_begin_steady_ns=10005650590000 codec_receive_end_steady_ns=10005650591400 codec_encoder_node=2 codec_selected_token_id=3125 codec_selected_token_generation=1 codec_packet_token_id=3125 codec_packet_token_generation=1 codec_token_mapping=copy_opaque codec_input_pts=3124 codec_timebase_num=1 codec_timebase_den=30 codec_accepted_send_count=1 codec_token_output_ordinal=1 codec_packet_pts=3124 codec_packet_dts=3124 codec_packet_bytes=29162 codec_identity_incomplete=0
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] ts_prefix_commit_summary stage=previous_completed_au generation=1 stream=video canonical_sequence=3125 emit_master_ns=109580299001 dispatch_master_ns=109680299001 published=1 completed=1 samples=1 would_block=3 incomplete=0 publish_to_protocol_commit_completed_total_ns=24892800 publish_to_protocol_commit_completed_max_ns=24892800 publish_to_protocol_commit_completed_max_prefix_offset=0 protocol_commit_completed_to_mux_resume_total_ns=32700 protocol_commit_completed_to_mux_resume_max_ns=32700 protocol_commit_completed_to_mux_resume_max_prefix_offset=0
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 produced_datagrams=74390 produced_ts_payload_bytes=89335532 pending_bytes=752 peak_pending_bytes=110920 access_units=7610 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=33061224 maximum_mux_input_after_emit_ns=125501534 maximum_mux_input_after_dispatch_ns=25501534 worst_mux_input_ready_ns=109713963800 worst_mux_input_emit_ns=109588462266 worst_mux_input_dispatch_ns=109688462266 maximum_protocol_batch_produced_after_release_ns=39279059 worst_protocol_batch_produced_ns=40571596200 worst_protocol_batch_release_ns=40532317141 worst_protocol_batch_deadline_ns=40632317141 worst_protocol_batch_datagrams=1 exit_reason=InvalidArgument: MPEG-TS access unit has no transport service window stream=audio emit_ns=109588462266 dispatch_ns=109688462266 available_ns=109713634420 last_committed_kind=maintenance service_start_ns=109713634420 actual_now_ns=109713972800 remaining_ns=-25172154 payload_bytes=752 wire_bytes=752
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] au_processing_trace stage=late_mux_input generation=1 stream=audio canonical_sequence=4486 emit_master_ns=109588462266 dispatch_master_ns=109688462266 ready_master_ns=109713963800 scheduled_created_steady_ns=10005795873100 adapter_dequeued_steady_ns=10005795915500 adapted_created_steady_ns=10005795916500 write_entered_steady_ns=10005796229600 session_ready_steady_ns=10005796231300 codec_identity=unavailable
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] av_scheduler_trace stage=late_output_commit stream=audio generation=1 canonical_dispatch_ns=104164716553 emit_ns=109611682221 dispatch_ns=109711682221 committed_at_ns=109714619700 canonical_sequence=4487 head_missing_valid=1 head_missing_intervals=0 head_missing_completed_ns=0 head_missing_active=0 head_missing_last_begin_ns=unavailable head_missing_last_end_ns=unavailable head_resumed_generation=unavailable head_resumed_sequence=unavailable other_generation=1 other_sequence=3126 other_canonical_dispatch_ns=104166666666 other_arrived_ns=109713597500 other_dispatch_master_ns=109713632334 head_arrived_ns=109713903400 scheduled_at_ns=109714594100
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] process.failed node=76 name=FileMuxNode kind=FileMux inputs=[channel=113 edge=113 stream=Video edge_kind=Metadata payload=CodecContext queued=0; channel=115 edge=115 stream=Audio edge_kind=Metadata payload=CodecContext queued=0; channel=116 edge=116 stream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=119 edge=119 stream=Any edge_kind=EncodedPacket payload=Unknown queued=0] outputs=[channel=122 edge=122 stream=Metadata edge_kind=Unknown payload=Unknown queued=0] error=InvalidArgument: MPEG-TS access unit has no transport service window stream=audio emit_ns=109588462266 dispatch_ns=109688462266 available_ns=109713634420 last_committed_kind=maintenance service_start_ns=109713634420 actual_now_ns=109713972800 remaining_ns=-25172154 payload_bytes=752 wire_bytes=752
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.failed node=76 kind=FileMux name=Project MPEG-TS mux error=InvalidArgument: MPEG-TS access unit has no transport service window stream=audio emit_ns=109588462266 dispatch_ns=109688462266 available_ns=109713634420 last_committed_kind=maintenance service_start_ns=109713634420 actual_now_ns=109713972800 remaining_ns=-25172154 payload_bytes=752 wire_bytes=752
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=27 available=1 cpu_ns=312500000 process_calls=77843
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=8 available=1 cpu_ns=296875000 process_calls=76913
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=46 available=1 cpu_ns=296875000 process_calls=80185
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=47 available=1 cpu_ns=359375000 process_calls=83887
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=28 available=1 cpu_ns=0 process_calls=63
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=51 available=1 cpu_ns=156250000 process_calls=13940
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=29 available=1 cpu_ns=0 process_calls=86
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] av_scheduler_trace stage=late_output_commit stream=video generation=1 canonical_dispatch_ns=104166666666 emit_ns=109613632334 dispatch_ns=109713632334 committed_at_ns=109715061200 canonical_sequence=3126 head_missing_valid=1 head_missing_intervals=0 head_missing_completed_ns=0 head_missing_active=0 head_missing_last_begin_ns=unavailable head_missing_last_end_ns=unavailable head_resumed_generation=unavailable head_resumed_sequence=unavailable other_generation=1 other_sequence=4488 other_canonical_dispatch_ns=104187936507 other_arrived_ns=109714803100 other_dispatch_master_ns=109734902175 head_arrived_ns=109713597500 scheduled_at_ns=109714807400
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=12 available=1 cpu_ns=0 process_calls=64
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=10 available=1 cpu_ns=0 process_calls=85
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=50 available=1 cpu_ns=140625000 process_calls=9746
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=53 available=1 cpu_ns=0 process_calls=192
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=13 available=1 cpu_ns=0 process_calls=86
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=14 available=1 cpu_ns=109375000 process_calls=9862
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=33 available=1 cpu_ns=78125000 process_calls=9881
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=17 available=1 cpu_ns=0 process_calls=64
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=54 available=1 cpu_ns=156250000 process_calls=9932
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=55 available=1 cpu_ns=140625000 process_calls=14165
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=59 available=1 cpu_ns=0 process_calls=124
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=36 available=1 cpu_ns=0 process_calls=64
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=15 available=1 cpu_ns=15625000 process_calls=9824
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=34 available=1 cpu_ns=109375000 process_calls=9842
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=56 available=1 cpu_ns=62500000 process_calls=9903
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=35 available=1 cpu_ns=125000000 process_calls=9908
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=58 available=1 cpu_ns=328125000 process_calls=23928
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=57 available=1 cpu_ns=203125000 process_calls=14093
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=18 available=1 cpu_ns=78125000 process_calls=9783
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=16 available=1 cpu_ns=46875000 process_calls=9920
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=20 available=1 cpu_ns=78125000 process_calls=9704
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=39 available=1 cpu_ns=62500000 process_calls=9706
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=62 available=1 cpu_ns=218750000 process_calls=23481
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=70 available=1 cpu_ns=500000000 process_calls=21429
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=64 available=1 cpu_ns=1125000000 process_calls=17369
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=37 available=1 cpu_ns=62500000 process_calls=9778
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=23 available=1 cpu_ns=93750000 process_calls=9681
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=42 available=1 cpu_ns=46875000 process_calls=9690
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=65 available=1 cpu_ns=62500000 process_calls=9717
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=71 available=1 cpu_ns=156250000 process_calls=15299
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=72 available=1 cpu_ns=234375000 process_calls=14705
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=24 available=1 cpu_ns=234375000 process_calls=11348
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=44 available=1 cpu_ns=78125000 process_calls=9800
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=32 available=1 cpu_ns=0 process_calls=84
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=66 available=1 cpu_ns=171875000 process_calls=11314
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=43 available=1 cpu_ns=265625000 process_calls=11083
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=7 available=1 cpu_ns=359375000 process_calls=19024
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=5 available=1 cpu_ns=109375000 process_calls=13471
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=75 available=1 cpu_ns=203125000 process_calls=22633
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=81 available=1 cpu_ns=4218750000 process_calls=22611
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=80 available=1 cpu_ns=0 process_calls=5
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=30 available=1 cpu_ns=187500000 process_calls=9721
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=79 available=1 cpu_ns=828125000 process_calls=36922
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=77 available=1 cpu_ns=0 process_calls=6
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=60 available=1 cpu_ns=250000000 process_calls=23639
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=19 available=1 cpu_ns=109375000 process_calls=9788
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=38 available=1 cpu_ns=93750000 process_calls=9811
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=61 available=1 cpu_ns=218750000 process_calls=23478
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=52 available=1 cpu_ns=0 process_calls=124
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=49 available=1 cpu_ns=0 process_calls=194
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=67 available=1 cpu_ns=109375000 process_calls=9739
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=48 available=1 cpu_ns=15625000 process_calls=126
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=9 available=1 cpu_ns=15625000 process_calls=64
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=73 available=1 cpu_ns=359375000 process_calls=22454
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=31 available=1 cpu_ns=0 process_calls=64
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=11 available=1 cpu_ns=265625000 process_calls=9717
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=25 available=1 cpu_ns=156250000 process_calls=9867
[2026-11-02 11:35:30.080] [info] [graph][runtime.node] worker.thread_cpu node=76 available=1 cpu_ns=1343750000 process_calls=46190
[2026-11-02 11:35:30.081] [info] [graph][runtime.node] process.failed node=4 name=AudioEncodeNode kind=AudioEncode inputs=[channel=2 edge=2 stream=Audio edge_kind=Metadata payload=CodecContext queued=0; channel=105 edge=105 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0] outputs=[channel=3 edge=3 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0; channel=115 edge=115 stream=Audio edge_kind=Metadata payload=CodecContext queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-11-02 11:35:30.082] [info] [graph][runtime.node] worker.thread_cpu node=74 available=1 cpu_ns=390625000 process_calls=22465
[2026-11-02 11:35:30.082] [info] [graph][runtime.node] worker.thread_cpu node=78 available=1 cpu_ns=203125000 process_calls=22248
[2026-11-02 11:35:30.084] [info] [graph][runtime.node] process.failed node=22 name=VideoDecodeNode kind=VideoDecode inputs=[channel=22 edge=22 stream=Video edge_kind=Metadata payload=CodecContext queued=0; channel=23 edge=23 stream=Video edge_kind=InputPacket payload=Packet queued=107] outputs=[channel=25 edge=25 stream=Video edge_kind=RawFrame payload=Frame queued=5] error=Cancelled: graph payload credit waiters were cancelled
[2026-11-02 11:35:30.087] [info] [graph][runtime.node] datagram_batch_trace stage=last_completed generation=1 first_sequence=74390 last_sequence=74390 maximum_submit_interval_ns=80700 maximum_submit_interval_sequence=74390 maximum_start_lateness_ns=1170780 maximum_start_lateness_sequence=74390 first_release_master_ns=109713634420 first_deadline_master_ns=109780301088 last_release_master_ns=109713634420 last_deadline_master_ns=109780301088 first_submitted_master_ns=109714885900 last_submitted_master_ns=109714885900 worst_wait_started_master_ns=109714804200 worst_wait_completed_master_ns=109714804200 worst_submit_observed_master_ns=109714805200 wait_attempts=0 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=0 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=0 wait_call_total_ns=0 wait_call_max_ns=0 wait_before_call_count=0 wait_before_call_total_ns=0 wait_before_call_max_ns=0 wait_arm_count=0 wait_arm_total_ns=0 wait_arm_max_ns=0 wait_api_wall_count=0 wait_api_wall_total_ns=0 wait_api_wall_max_ns=0 wait_return_to_next_clock_count=0 wait_return_to_next_clock_total_ns=0 wait_return_to_next_clock_max_ns=0 wait_target_lateness_count=0 wait_target_lateness_total_ns=0 wait_target_lateness_max_ns=0
[2026-11-02 11:35:30.089] [info] [graph][runtime.node] worker.thread_cpu node=4 available=1 cpu_ns=2765625000 process_calls=14359
[2026-11-02 11:35:30.089] [info] [graph][runtime.node] worker.thread_cpu node=6 available=1 cpu_ns=906250000 process_calls=19345
[2026-11-02 11:35:30.089] [info] [graph][runtime.node] worker.thread_cpu node=22 available=1 cpu_ns=1156250000 process_calls=12956
[2026-11-02 11:35:30.090] [info] [graph][runtime.node] datagram_successful_submit_trace stage=worst_success generation=1 endpoint=1 sequence=24983 pacing_lateness_ns=27653140 matches_pacing_worst=1 materialized_master_ns=unavailable enqueued_master_ns=unavailable submit_interval_scope=pre_transport_checks_and_transport socket_only_ns=unavailable release_master_ns=40480299000 deadline_master_ns=40562657277 original_protocol_deadline_master_ns=40562657277 prefix_completion_limit_master_ns=40562657277 reserved_at_master_ns=40509470600 not_before_master_ns=40481821260 not_after_master_ns=40562532477 service_duration_ns=124800 wire_bytes_per_second=6250000 submit_started_master_ns=40509474400 submit_completed_master_ns=40509533800 activation_started_master_ns=40509468300 activation_completed_master_ns=40509469900 permit_started_master_ns=40509470100 permit_completed_master_ns=40509470300 snapshot_started_master_ns=40509470600 snapshot_sampled_master_ns=40509470600 snapshot_completed_master_ns=40509471100 reservation_completed_master_ns=40509472800 wait_started_master_ns=40509473600 wait_completed_master_ns=40509473600 submit_observed_master_ns=40509474400 wait_attempts=0 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=0 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=0 wait_call_total_ns=0 wait_call_max_ns=0 wait_before_call_count=0 wait_before_call_total_ns=0 wait_before_call_max_ns=0 wait_arm_count=0 wait_arm_total_ns=0 wait_arm_max_ns=0 wait_api_wall_count=0 wait_api_wall_total_ns=0 wait_api_wall_max_ns=0 wait_return_to_next_clock_count=0 wait_return_to_next_clock_total_ns=0 wait_return_to_next_clock_max_ns=0 wait_target_lateness_count=0 wait_target_lateness_total_ns=0 wait_target_lateness_max_ns=0
[2026-11-02 11:35:30.090] [info] [graph][runtime.node] worker.thread_cpu node=68 available=1 cpu_ns=921875000 process_calls=12521
[2026-11-02 11:35:30.090] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=11269 committed_datagrams=74390 committed_payload_bytes=89335532 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=79296 pacing_reserved=74390 pacing_cancelled=0 pacing_submitted=74390 pacing_maximum_submit_lateness_ns=27653140 pacing_worst_late_global_sequence=24983 pacing_worst_success_sample_available=1 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=79296 backlog_max_residence_ns=49439400 last_materialized_sequence=74390 last_scheduled_sequence=74390 last_submitted_sequence=74390 last_committed_sequence=74390 submitted_service_generation=1 submitted_service_sequence=74390 submitted_service_completed_ns=109714885900 submitted_service_duration_ns=34560 submitted_service_available_ns=109714920460 submitted_scope_available_ns=109714920460 shared_scope_charged_datagrams=74390 shared_scope_charged_wire_bytes=91418452 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2154 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2154 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=188010 endpoint_1_target_effective_socket_bytes=188010 endpoint_1_api_requested_socket_bytes=188010 endpoint_1_effective_socket_bytes=188010 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=74390 endpoint_1_committed_payload_bytes=89335532
[2026-11-02 11:35:30.091] [info] [graph][runtime.node] process.failed node=41 name=VideoDecodeNode kind=VideoDecode inputs=[channel=49 edge=49 stream=Video edge_kind=Metadata payload=CodecContext queued=0; channel=50 edge=50 stream=Video edge_kind=InputPacket payload=Packet queued=106] outputs=[channel=52 edge=52 stream=Video edge_kind=RawFrame payload=Frame queued=6] error=Cancelled: graph payload credit waiters were cancelled
[2026-11-02 11:35:30.092] [info] [graph][runtime.node] worker.thread_cpu node=41 available=1 cpu_ns=1078125000 process_calls=12664
[2026-11-02 11:35:30.092] [info] [graph][runtime.node] worker.thread_cpu node=26 available=1 cpu_ns=1281250000 process_calls=12523
[2026-11-02 11:35:30.092] [info] [graph][runtime.node] worker.thread_cpu node=45 available=1 cpu_ns=1078125000 process_calls=12522
[2026-11-02 11:35:30.093] [info] [graph][runtime.node] process.failed node=2 name=VideoEncodeNode kind=VideoEncode inputs=[channel=1 edge=1 stream=Video edge_kind=Metadata payload=CodecContext queued=0; channel=104 edge=104 stream=Video edge_kind=RawFrame payload=Frame queued=2] outputs=[channel=107 edge=107 stream=Video edge_kind=EncodedPacket payload=Packet queued=0; channel=113 edge=113 stream=Video edge_kind=Metadata payload=CodecContext queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-11-02 11:35:30.093] [info] [graph][runtime.node] worker.thread_cpu node=2 available=1 cpu_ns=1859375000 process_calls=9356
[2026-11-02 11:35:30.326] [info] [graph][runtime.lifecycle] node_abort_payload node=81 before_objects=727 after_objects=727 before_bytes=11472255 after_bytes=11472255 before_reservations=98183 after_reservations=98183 before_releases=97456 after_releases=97456
[2026-11-02 11:35:30.326] [info] [graph][runtime.node] mpegts_wire_materializer stage=aborted maximum_materialized_after_release_ns=39303959 worst_materialized_at_ns=40571621100 worst_release_ns=40532317141 worst_deadline_ns=40632317141 worst_global_sequence=25028
[2026-11-02 11:35:30.326] [info] [graph][runtime.lifecycle] node_abort_payload node=79 before_objects=727 after_objects=727 before_bytes=11472255 after_bytes=11472255 before_reservations=98183 after_reservations=98183 before_releases=97456 after_releases=97456
[2026-11-02 11:35:30.326] [info] [graph][runtime.lifecycle] node_abort_payload node=76 before_objects=727 after_objects=727 before_bytes=11472255 after_bytes=11472255 before_reservations=98183 after_reservations=98183 before_releases=97456 after_releases=97456
[2026-11-02 11:35:30.326] [info] [graph][runtime.lifecycle] node_abort_payload node=78 before_objects=727 after_objects=727 before_bytes=11472255 after_bytes=11472255 before_reservations=98183 after_reservations=98183 before_releases=97456 after_releases=97456
[2026-11-02 11:35:30.326] [info] [graph][runtime.lifecycle] node_abort_payload node=75 before_objects=727 after_objects=727 before_bytes=11472255 after_bytes=11472255 before_reservations=98183 after_reservations=98183 before_releases=97456 after_releases=97456
[2026-11-02 11:35:30.326] [info] [graph][runtime.node] audio_stage_observation node=74 stage=scheduler_input count=4488 last_generation=1 last_begin=4594688 last_end=4595712 last_rate=44100 last_steady_ns=10005797070300 maximum_inter_event_gap_ns=144846000 gap_begin_generation=1 gap_begin_begin=4592640 gap_begin_end=4593664 gap_begin_rate=44100 gap_begin_steady_ns=10005651324600 gap_end_generation=1 gap_end_begin=4593664 gap_end_end=4594688 gap_end_rate=44100 gap_end_steady_ns=10005796170600
[2026-11-02 11:35:30.326] [info] [graph][runtime.node] av_scheduler_correlated_wait generation=1 canonical_sequence=4486 missing=video begin_steady_ns=10005651334200 end_steady_ns=10005795867000 observed_wait_ns=144532800 held_dispatch_master_ns=109688462266 begin_master_ns=109569066800 end_master_ns=109713599000 dispatch_remaining_at_end_ns=-25136734 resumed_dispatch_master_ns=109713632334
[2026-11-02 11:35:30.327] [info] [graph][runtime.lifecycle] node_abort_payload node=74 before_objects=727 after_objects=726 before_bytes=11472255 after_bytes=11471667 before_reservations=98183 after_reservations=98183 before_releases=97456 after_releases=97457
[2026-11-02 11:35:30.327] [info] [graph][runtime.node] audio_stage_observation node=5 stage=canonicalizer_input count=4491 last_generation=1 last_begin=4597760 last_end=4598784 last_rate=44100 last_steady_ns=10005767636600 maximum_inter_event_gap_ns=27608700 gap_begin_generation=1 gap_begin_begin=47104 gap_begin_end=48128 gap_begin_rate=44100 gap_begin_steady_ns=9902578466700 gap_end_generation=1 gap_end_begin=48128 gap_end_end=49152 gap_end_rate=44100 gap_end_steady_ns=9902606075400
[2026-11-02 11:35:30.327] [info] [graph][runtime.node] audio_stage_observation node=5 stage=canonicalizer_committed count=4491 last_generation=1 last_begin=4597760 last_end=4598784 last_rate=44100 last_steady_ns=10005767650400 maximum_inter_event_gap_ns=27613700 gap_begin_generation=1 gap_begin_begin=47104 gap_begin_end=48128 gap_begin_rate=44100 gap_begin_steady_ns=9902578482600 gap_end_generation=1 gap_end_begin=48128 gap_end_end=49152 gap_end_rate=44100 gap_end_steady_ns=9902606096300
[2026-11-02 11:35:30.327] [info] [graph][runtime.lifecycle] node_abort_payload node=5 before_objects=726 after_objects=726 before_bytes=11471667 after_bytes=11471667 before_reservations=98183 after_reservations=98183 before_releases=97457 after_releases=97457
[2026-11-02 11:35:30.327] [info] [graph][runtime.lifecycle] node_abort_payload node=77 before_objects=726 after_objects=726 before_bytes=11471667 after_bytes=11471667 before_reservations=98183 after_reservations=98183 before_releases=97457 after_releases=97457
[2026-11-02 11:35:30.327] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_input count=4494 last_generation=1 last_begin=4600832 last_end=4601856 last_rate=44100 last_steady_ns=10005796980900 maximum_inter_event_gap_ns=29697600 gap_begin_generation=1 gap_begin_begin=4599808 gap_begin_end=4600832 gap_begin_rate=44100 gap_begin_steady_ns=10005767283300 gap_end_generation=1 gap_end_begin=4600832 gap_end_end=4601856 gap_end_rate=44100 gap_end_steady_ns=10005796980900
[2026-11-02 11:35:30.327] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_submitted count=4494 last_generation=1 last_begin=4600832 last_end=4601856 last_rate=44100 last_steady_ns=10005797587300 maximum_inter_event_gap_ns=29992000 gap_begin_generation=1 gap_begin_begin=4599808 gap_begin_end=4600832 gap_begin_rate=44100 gap_begin_steady_ns=10005767595300 gap_end_generation=1 gap_end_begin=4600832 gap_end_end=4601856 gap_end_rate=44100 gap_end_steady_ns=10005797587300
[2026-11-02 11:35:30.327] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_packet count=4491 last_generation=1 last_begin=4597760 last_end=4598784 last_rate=44100 last_steady_ns=10005767605400 maximum_inter_event_gap_ns=27600800 gap_begin_generation=1 gap_begin_begin=47104 gap_begin_end=48128 gap_begin_rate=44100 gap_begin_steady_ns=9902578442400 gap_end_generation=1 gap_end_begin=48128 gap_end_end=49152 gap_end_rate=44100 gap_end_steady_ns=9902606043200
[2026-11-02 11:35:30.327] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_committed count=4491 last_generation=1 last_begin=4597760 last_end=4598784 last_rate=44100 last_steady_ns=10005767617600 maximum_inter_event_gap_ns=27605700 gap_begin_generation=1 gap_begin_begin=47104 gap_begin_end=48128 gap_begin_rate=44100 gap_begin_steady_ns=9902578456600 gap_end_generation=1 gap_end_begin=48128 gap_end_end=49152 gap_end_rate=44100 gap_end_steady_ns=9902606062300
[2026-11-02 11:35:30.327] [info] [graph][runtime.lifecycle] codec_owner_release node=4 kind=AudioEncode owner_references=4
[2026-11-02 11:35:30.327] [info] [graph][runtime.lifecycle] node_abort_payload node=4 before_objects=726 after_objects=726 before_bytes=11471667 after_bytes=11471667 before_reservations=98183 after_reservations=98183 before_releases=97457 after_releases=97457
[2026-11-02 11:35:30.327] [info] [graph][runtime.lifecycle] node_abort_payload node=80 before_objects=726 after_objects=726 before_bytes=11471667 after_bytes=11471667 before_reservations=98183 after_reservations=98183 before_releases=97457 after_releases=97457
[2026-11-02 11:35:30.327] [info] [graph][runtime.node] audio_stage_observation node=7 stage=aggregate_built count=4494 last_generation=1 last_begin=4600832 last_end=4601856 last_rate=44100 last_steady_ns=10005790120000 maximum_inter_event_gap_ns=26325400 gap_begin_generation=1 gap_begin_begin=845824 gap_begin_end=846848 gap_begin_rate=44100 gap_begin_steady_ns=9920642800100 gap_end_generation=1 gap_end_begin=846848 gap_end_end=847872 gap_end_rate=44100 gap_end_steady_ns=9920669125500
[2026-11-02 11:35:30.327] [info] [graph][runtime.node] audio_stage_observation node=7 stage=aggregate_committed count=4494 last_generation=1 last_begin=4600832 last_end=4601856 last_rate=44100 last_steady_ns=10005790138200 maximum_inter_event_gap_ns=26325500 gap_begin_generation=1 gap_begin_begin=845824 gap_begin_end=846848 gap_begin_rate=44100 gap_begin_steady_ns=9920642811200 gap_end_generation=1 gap_end_begin=846848 gap_end_end=847872 gap_end_rate=44100 gap_end_steady_ns=9920669136700
[2026-11-02 11:35:30.327] [info] [graph][runtime.node] continuous_audio_aggregate.summary reason=abort output_group=composition-r379.output audio_commits=4494 real_samples=4601856 silence_samples=0
[2026-11-02 11:35:30.328] [info] [graph][runtime.lifecycle] node_abort_payload node=7 before_objects=726 after_objects=720 before_bytes=11471667 after_bytes=11422515 before_reservations=98183 after_reservations=98183 before_releases=97457 after_releases=97463
[2026-11-02 11:35:30.328] [info] [graph][runtime.lifecycle] codec_owner_release node=2 kind=VideoEncode owner_references=4
[2026-11-02 11:35:30.328] [info] [graph][runtime.node] video_encoder_execution_observation node=2 count=3127 maximum_call_ns=78751700 maximum_begin_steady_ns=9936512577000 maximum_end_steady_ns=9936591328700 maximum_encoder_pts=1051 maximum_native_result=0 maximum_between_submissions_ns=111685400 before_gap_begin_steady_ns=10005615121600 before_gap_end_steady_ns=10005650578300 before_gap_encoder_pts=3124 before_gap_native_result=0 after_gap_begin_steady_ns=10005762263700 after_gap_end_steady_ns=10005795827700 after_gap_encoder_pts=3125 after_gap_native_result=0
[2026-11-02 11:35:30.328] [info] [graph][runtime.lifecycle] node_abort_payload node=2 before_objects=720 after_objects=720 before_bytes=11422515 after_bytes=11422515 before_reservations=98183 after_reservations=98183 before_releases=97463 after_releases=97463
[2026-11-02 11:35:30.328] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=process count=19345 maximum_ns=132797400 begin_steady_ns=10005629385700 end_steady_ns=10005762183100 video_cursor=3125 audio_cursor=not_owned next_video_cursor=3125 next_audio_cursor=not_owned error=None return_state=progress deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-11-02 11:35:30.328] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=purge count=19345 maximum_ns=249100 begin_steady_ns=9981463991300 end_steady_ns=9981464240400 video_cursor=2401 audio_cursor=not_owned next_video_cursor=2401 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 11:35:30.328] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=bind count=19345 maximum_ns=209600 begin_steady_ns=9940762998300 end_steady_ns=9940763207900 video_cursor=1179 audio_cursor=not_owned next_video_cursor=1179 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 11:35:30.328] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=consume count=19345 maximum_ns=3807200 begin_steady_ns=9907237016600 end_steady_ns=9907240823800 video_cursor=174 audio_cursor=not_owned next_video_cursor=174 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 11:35:30.328] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=activate count=11 maximum_ns=79200 begin_steady_ns=9901462566000 end_steady_ns=9901462645200 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 11:35:30.328] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=build_video count=3129 maximum_ns=132737200 begin_steady_ns=10005629412600 end_steady_ns=10005762149800 video_cursor=3125 audio_cursor=not_owned next_video_cursor=3125 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 11:35:30.328] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=canvas count=3129 maximum_ns=132687900 begin_steady_ns=10005629430500 end_steady_ns=10005762118400 video_cursor=3125 audio_cursor=not_owned next_video_cursor=3125 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 11:35:30.328] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=emit count=3129 maximum_ns=4366700 begin_steady_ns=9920666589100 end_steady_ns=9920670955800 video_cursor=576 audio_cursor=not_owned next_video_cursor=576 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 11:35:30.328] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=between_calls_previous_return count=19344 maximum_ns=107251000 begin_steady_ns=9901350795500 end_steady_ns=9901458046500 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=waiting deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-11-02 11:35:30.328] [info] [graph][runtime.node] continuous_aggregate.summary reason=abort output_group=composition-r379.output output_generation=1 video_commits=3129 source[0].group=composition-r379.source.0 source[0].identity=composition-r379.source.0.input.video source[0].real_frames=3129 source[0].black_frames=0 source[0].contribution_generation=1 source[1].group=composition-r379.source.1 source[1].identity=composition-r379.source.1.input.video source[1].real_frames=3129 source[1].black_frames=0 source[1].contribution_generation=1 source[2].group=composition-r379.source.2 source[2].identity=composition-r379.source.2.input.video source[2].real_frames=3129 source[2].black_frames=0 source[2].contribution_generation=1
[2026-11-02 11:35:30.329] [info] [graph][runtime.node] cuda_canvas_call stage=context_push count=3133 operation=copy maximum_ns=354200 begin_steady_ns=9922429535300 end_steady_ns=9922429889500 native_result=0 thread_cpu_ns=0
[2026-11-02 11:35:30.329] [info] [graph][runtime.node] cuda_canvas_call stage=source_wait count=12522 operation=copy maximum_ns=305800 begin_steady_ns=9978397822100 end_steady_ns=9978398127900 native_result=0 thread_cpu_ns=0
[2026-11-02 11:35:30.329] [info] [graph][runtime.node] cuda_canvas_call stage=plane_copy count=25046 operation=copy maximum_ns=1823200 begin_steady_ns=9958663166900 end_steady_ns=9958664990100 native_result=0 thread_cpu_ns=0
[2026-11-02 11:35:30.329] [info] [graph][runtime.node] cuda_canvas_call stage=destination_wait count=2 operation=upload maximum_ns=73100 begin_steady_ns=9901299759800 end_steady_ns=9901299832900 native_result=0 thread_cpu_ns=0
[2026-11-02 11:35:30.329] [info] [graph][runtime.node] cuda_canvas_call stage=context_pop count=3133 operation=copy maximum_ns=247300 begin_steady_ns=9953797201700 end_steady_ns=9953797449000 native_result=0 thread_cpu_ns=0
[2026-11-02 11:35:30.329] [info] [graph][runtime.node] cuda_canvas_call stage=batch_completion count=3131 operation=copy maximum_ns=132517000 begin_steady_ns=10005629572700 end_steady_ns=10005762089700 native_result=0 thread_cpu_ns=0
[2026-11-02 11:35:30.329] [info] [graph][runtime.node] canvas_compose_correlated scope=attempt begin_steady_ns=10005629431300 end_steady_ns=10005762117800 wall_ns=132686500 thread_cpu_ns=0 surface_count=1 surface_total_ns=1500 surface_maximum_ns=1500 surface_maximum_tile=none surface_maximum_result=unavailable validate_count=3 validate_total_ns=1900 validate_maximum_ns=900 validate_maximum_tile=0 validate_maximum_result=0 black_count=1 black_total_ns=82300 black_maximum_ns=82300 black_maximum_tile=none black_maximum_result=0 tile_count=3 tile_total_ns=43500 tile_maximum_ns=19400 tile_maximum_tile=0 tile_maximum_result=0 publish_count=1 publish_total_ns=12900 publish_maximum_ns=12900 publish_maximum_tile=none publish_maximum_result=0 context_push_count=1 context_push_total_ns=3700 context_push_maximum_ns=3700 context_push_maximum_tile=none context_push_maximum_result=0 source_wait_count=4 source_wait_total_ns=2300 source_wait_maximum_ns=1600 source_wait_maximum_tile=0 source_wait_maximum_result=0 plane_copy_count=8 plane_copy_total_ns=109400 plane_copy_maximum_ns=64100 plane_copy_maximum_tile=none plane_copy_maximum_result=0 destination_wait_count=0 destination_wait_total_ns=0 destination_wait_maximum_ns=0 destination_wait_maximum_tile=none destination_wait_maximum_result=unavailable context_pop_count=1 context_pop_total_ns=10700 context_pop_maximum_ns=10700 context_pop_maximum_tile=none context_pop_maximum_result=0 batch_completion_count=1 batch_completion_total_ns=132514400 batch_completion_maximum_ns=132514400 batch_completion_maximum_tile=none batch_completion_maximum_result=0 source_query_completed_count=0 source_query_ready_count=3 source_query_ready_maximum_ns=4100 source_query_ready_begin_steady_ns=10005629529000 source_query_ready_end_steady_ns=10005629533100 source_query_ready_tile=0 source_query_ready_operation=event_query source_query_ready_native=0 source_query_ready_restore_native=0 source_query_pending_count=0 source_query_unavailable_count=0 source_query_error_count=0
[2026-11-02 11:35:30.329] [info] [graph][runtime.lifecycle] node_abort_payload node=6 before_objects=720 after_objects=678 before_bytes=11422515 after_bytes=11422515 before_reservations=98183 after_reservations=98183 before_releases=97463 after_releases=97505
[2026-11-02 11:35:30.329] [info] [graph][runtime.node] source_video_trace_summary node_id=67 node=VideoPreparationNode generation=1 reason=abort steady_ns=10006046309700 input_count=3142 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3142 published_count=3141 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:35:30.329] [info] [graph][runtime.lifecycle] node_abort_payload node=67 before_objects=678 after_objects=676 before_bytes=11422515 after_bytes=11422515 before_reservations=98183 after_reservations=98183 before_releases=97505 after_releases=97507
[2026-11-02 11:35:30.329] [info] [graph][runtime.node] source_video_trace_summary node_id=44 node=VideoPreparationNode generation=1 reason=abort steady_ns=10006046460100 input_count=3142 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3142 published_count=3141 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:35:30.329] [info] [graph][runtime.lifecycle] node_abort_payload node=44 before_objects=676 after_objects=674 before_bytes=11422515 after_bytes=11422515 before_reservations=98183 after_reservations=98183 before_releases=97507 after_releases=97509
[2026-11-02 11:35:30.330] [info] [graph][runtime.node] source_video_trace_summary node_id=25 node=VideoPreparationNode generation=1 reason=abort steady_ns=10006046606700 input_count=3142 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3142 published_count=3141 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:35:30.330] [info] [graph][runtime.lifecycle] node_abort_payload node=25 before_objects=674 after_objects=672 before_bytes=11422515 after_bytes=11422515 before_reservations=98183 after_reservations=98183 before_releases=97509 after_releases=97511
[2026-11-02 11:35:30.330] [info] [graph][runtime.lifecycle] codec_owner_release node=73 kind=AudioResample owner_references=1
[2026-11-02 11:35:30.330] [info] [graph][runtime.lifecycle] node_abort_payload node=73 before_objects=672 after_objects=670 before_bytes=11422515 after_bytes=11406131 before_reservations=98183 after_reservations=98183 before_releases=97511 after_releases=97513
[2026-11-02 11:35:30.330] [info] [graph][runtime.node] source_video_trace_summary node_id=68 node=VideoFilterNode generation=1 reason=abort steady_ns=10006047304700 input_count=3148 accepted_count=3148 would_block_count=0 resolved_count=3148 output_attempt_count=0 published_count=3147 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:35:30.333] [info] [graph][runtime.lifecycle] node_abort_payload node=68 before_objects=670 after_objects=668 before_bytes=11406131 after_bytes=11406131 before_reservations=98183 after_reservations=98183 before_releases=97513 after_releases=97515
[2026-11-02 11:35:30.333] [info] [graph][runtime.node] source_video_trace_summary node_id=45 node=VideoFilterNode generation=1 reason=abort steady_ns=10006050578000 input_count=3149 accepted_count=3149 would_block_count=0 resolved_count=3149 output_attempt_count=0 published_count=3148 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:35:30.335] [info] [graph][runtime.lifecycle] node_abort_payload node=45 before_objects=668 after_objects=666 before_bytes=11406131 after_bytes=11406131 before_reservations=98183 after_reservations=98183 before_releases=97515 after_releases=97517
[2026-11-02 11:35:30.335] [info] [graph][runtime.node] source_video_trace_summary node_id=26 node=VideoFilterNode generation=1 reason=abort steady_ns=10006052361300 input_count=3149 accepted_count=3149 would_block_count=0 resolved_count=3149 output_attempt_count=0 published_count=3148 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:35:30.337] [info] [graph][runtime.lifecycle] node_abort_payload node=26 before_objects=666 after_objects=664 before_bytes=11406131 after_bytes=11406131 before_reservations=98183 after_reservations=98183 before_releases=97517 after_releases=97519
[2026-11-02 11:35:30.337] [info] [graph][runtime.lifecycle] node_abort_payload node=72 before_objects=664 after_objects=664 before_bytes=11406131 after_bytes=11406131 before_reservations=98183 after_reservations=98183 before_releases=97519 after_releases=97519
[2026-11-02 11:35:30.337] [info] [graph][runtime.node] source_video_trace_summary node_id=66 node=VideoFrameRateNode generation=1 reason=abort steady_ns=10006054290100 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3154 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:35:30.337] [info] [graph][runtime.lifecycle] node_abort_payload node=66 before_objects=664 after_objects=663 before_bytes=11406131 after_bytes=11406131 before_reservations=98183 after_reservations=98183 before_releases=97519 after_releases=97520
[2026-11-02 11:35:30.337] [info] [graph][runtime.node] source_video_trace_summary node_id=43 node=VideoFrameRateNode generation=1 reason=abort steady_ns=10006054531900 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3155 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:35:30.338] [info] [graph][runtime.lifecycle] node_abort_payload node=43 before_objects=663 after_objects=662 before_bytes=11406131 after_bytes=11406131 before_reservations=98183 after_reservations=98183 before_releases=97520 after_releases=97521
[2026-11-02 11:35:30.338] [info] [graph][runtime.node] source_video_trace_summary node_id=24 node=VideoFrameRateNode generation=1 reason=abort steady_ns=10006054825000 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3155 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:35:30.338] [info] [graph][runtime.lifecycle] node_abort_payload node=24 before_objects=662 after_objects=661 before_bytes=11406131 after_bytes=11406131 before_reservations=98183 after_reservations=98183 before_releases=97521 after_releases=97522
[2026-11-02 11:35:30.338] [info] [graph][runtime.lifecycle] node_abort_payload node=71 before_objects=661 after_objects=660 before_bytes=11406131 after_bytes=11397939 before_reservations=98183 after_reservations=98183 before_releases=97522 after_releases=97523
[2026-11-02 11:35:30.338] [info] [graph][runtime.node] source_video_trace_summary node_id=65 node=HardwareTransferNode generation=1 reason=abort steady_ns=10006055130600 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3161 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:35:30.338] [info] [graph][runtime.lifecycle] node_abort_payload node=65 before_objects=660 after_objects=659 before_bytes=11397939 after_bytes=11397939 before_reservations=98183 after_reservations=98183 before_releases=97523 after_releases=97524
[2026-11-02 11:35:30.338] [info] [graph][runtime.node] source_video_trace_summary node_id=42 node=HardwareTransferNode generation=1 reason=abort steady_ns=10006055324000 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3162 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:35:30.338] [info] [graph][runtime.lifecycle] node_abort_payload node=42 before_objects=659 after_objects=658 before_bytes=11397939 after_bytes=11397939 before_reservations=98183 after_reservations=98183 before_releases=97524 after_releases=97525
[2026-11-02 11:35:30.338] [info] [graph][runtime.node] source_video_trace_summary node_id=23 node=HardwareTransferNode generation=1 reason=abort steady_ns=10006055457700 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3162 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:35:30.339] [info] [graph][runtime.lifecycle] node_abort_payload node=23 before_objects=658 after_objects=657 before_bytes=11397939 after_bytes=11397939 before_reservations=98183 after_reservations=98183 before_releases=97525 after_releases=97526
[2026-11-02 11:35:30.339] [info] [graph][runtime.lifecycle] codec_owner_release node=70 kind=AudioDecode owner_references=2
[2026-11-02 11:35:30.339] [info] [graph][runtime.lifecycle] node_abort_payload node=70 before_objects=657 after_objects=656 before_bytes=11397939 after_bytes=11389747 before_reservations=98183 after_reservations=98183 before_releases=97526 after_releases=97527
[2026-11-02 11:35:30.339] [info] [graph][runtime.node] source_video_trace_summary node_id=64 node=VideoDecodeNode generation=1 reason=abort steady_ns=10006055910300 input_count=3169 accepted_count=3169 would_block_count=0 resolved_count=3169 output_attempt_count=0 published_count=3168 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:35:30.339] [info] [graph][runtime.lifecycle] codec_owner_release node=64 kind=VideoDecode owner_references=2
[2026-11-02 11:35:30.345] [info] [graph][runtime.lifecycle] node_abort_payload node=64 before_objects=656 after_objects=655 before_bytes=11389747 after_bytes=11389747 before_reservations=98183 after_reservations=98183 before_releases=97527 after_releases=97528
[2026-11-02 11:35:30.345] [info] [graph][runtime.node] source_video_trace_summary node_id=41 node=VideoDecodeNode generation=1 reason=abort steady_ns=10006062365000 input_count=3170 accepted_count=3170 would_block_count=0 resolved_count=3169 output_attempt_count=0 published_count=3169 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:35:30.345] [info] [graph][runtime.lifecycle] codec_owner_release node=41 kind=VideoDecode owner_references=2
[2026-11-02 11:35:30.350] [info] [graph][runtime.lifecycle] node_abort_payload node=41 before_objects=655 after_objects=655 before_bytes=11389747 after_bytes=11389747 before_reservations=98183 after_reservations=98183 before_releases=97528 after_releases=97528
[2026-11-02 11:35:30.351] [info] [graph][runtime.node] source_video_trace_summary node_id=22 node=VideoDecodeNode generation=1 reason=abort steady_ns=10006067639500 input_count=3169 accepted_count=3169 would_block_count=0 resolved_count=3168 output_attempt_count=0 published_count=3168 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:35:30.351] [info] [graph][runtime.lifecycle] codec_owner_release node=22 kind=VideoDecode owner_references=2
[2026-11-02 11:35:30.356] [info] [graph][runtime.lifecycle] node_abort_payload node=22 before_objects=655 after_objects=655 before_bytes=11389747 after_bytes=11389747 before_reservations=98183 after_reservations=98183 before_releases=97528 after_releases=97528
[2026-11-02 11:35:30.356] [info] [graph][runtime.node] source_video_trace_summary node_id=62 node=MediaAvBoundReleaseExtractorNode generation=1 reason=abort steady_ns=10006073207100 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=3278 release_committed_count=3262 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:35:30.356] [info] [graph][runtime.lifecycle] node_abort_payload node=62 before_objects=655 after_objects=655 before_bytes=11389747 after_bytes=11389747 before_reservations=98183 after_reservations=98183 before_releases=97528 after_releases=97528
[2026-11-02 11:35:30.357] [info] [graph][runtime.node] source_video_trace_summary node_id=39 node=MediaAvBoundReleaseExtractorNode generation=1 reason=abort steady_ns=10006073645900 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=3278 release_committed_count=3262 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:35:30.357] [info] [graph][runtime.lifecycle] node_abort_payload node=39 before_objects=655 after_objects=655 before_bytes=11389747 after_bytes=11389747 before_reservations=98183 after_reservations=98183 before_releases=97528 after_releases=97528
[2026-11-02 11:35:30.357] [info] [graph][runtime.node] source_video_trace_summary node_id=20 node=MediaAvBoundReleaseExtractorNode generation=1 reason=abort steady_ns=10006073784000 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=3278 release_committed_count=3262 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:35:30.357] [info] [graph][runtime.lifecycle] node_abort_payload node=20 before_objects=655 after_objects=655 before_bytes=11389747 after_bytes=11389747 before_reservations=98183 after_reservations=98183 before_releases=97528 after_releases=97528
[2026-11-02 11:35:30.357] [info] [graph][runtime.node] source_video_trace_summary node_id=61 node=ActivationReleaseSequencer generation=1 reason=abort steady_ns=10006073899800 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3262 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:35:30.357] [info] [graph][runtime.lifecycle] node_abort_payload node=61 before_objects=655 after_objects=655 before_bytes=11389747 after_bytes=11389747 before_reservations=98183 after_reservations=98183 before_releases=97528 after_releases=97528
[2026-11-02 11:35:30.357] [info] [graph][runtime.node] source_video_trace_summary node_id=38 node=ActivationReleaseSequencer generation=1 reason=abort steady_ns=10006074020700 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3262 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:35:30.357] [info] [graph][runtime.lifecycle] node_abort_payload node=38 before_objects=655 after_objects=655 before_bytes=11389747 after_bytes=11389747 before_reservations=98183 after_reservations=98183 before_releases=97528 after_releases=97528
[2026-11-02 11:35:30.357] [info] [graph][runtime.node] source_video_trace_summary node_id=19 node=ActivationReleaseSequencer generation=1 reason=abort steady_ns=10006074142000 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3262 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:35:30.357] [info] [graph][runtime.lifecycle] node_abort_payload node=19 before_objects=655 after_objects=655 before_bytes=11389747 after_bytes=11389747 before_reservations=98183 after_reservations=98183 before_releases=97528 after_releases=97528
[2026-11-02 11:35:30.357] [info] [graph][runtime.lifecycle] node_abort_payload node=60 before_objects=655 after_objects=655 before_bytes=11389747 after_bytes=11389747 before_reservations=98183 after_reservations=98183 before_releases=97528 after_releases=97528
[2026-11-02 11:35:30.357] [info] [graph][runtime.lifecycle] node_abort_payload node=37 before_objects=655 after_objects=655 before_bytes=11389747 after_bytes=11389747 before_reservations=98183 after_reservations=98183 before_releases=97528 after_releases=97528
[2026-11-02 11:35:30.357] [info] [graph][runtime.lifecycle] node_abort_payload node=18 before_objects=655 after_objects=655 before_bytes=11389747 after_bytes=11389747 before_reservations=98183 after_reservations=98183 before_releases=97528 after_releases=97528
[2026-11-02 11:35:30.357] [info] [graph][runtime.lifecycle] node_abort_payload node=58 before_objects=655 after_objects=655 before_bytes=11389747 after_bytes=11389747 before_reservations=98183 after_reservations=98183 before_releases=97528 after_releases=97528
[2026-11-02 11:35:30.357] [info] [graph][runtime.lifecycle] node_abort_payload node=35 before_objects=655 after_objects=655 before_bytes=11389747 after_bytes=11389747 before_reservations=98183 after_reservations=98183 before_releases=97528 after_releases=97528
[2026-11-02 11:35:30.358] [info] [graph][runtime.lifecycle] node_abort_payload node=16 before_objects=655 after_objects=655 before_bytes=11389747 after_bytes=11389747 before_reservations=98183 after_reservations=98183 before_releases=97528 after_releases=97528
[2026-11-02 11:35:30.358] [info] [graph][runtime.lifecycle] node_abort_payload node=57 before_objects=655 after_objects=655 before_bytes=11389747 after_bytes=11389747 before_reservations=98183 after_reservations=98183 before_releases=97528 after_releases=97528
[2026-11-02 11:35:30.358] [info] [graph][runtime.lifecycle] node_abort_payload node=56 before_objects=655 after_objects=655 before_bytes=11389747 after_bytes=11389747 before_reservations=98183 after_reservations=98183 before_releases=97528 after_releases=97528
[2026-11-02 11:35:30.358] [info] [graph][runtime.lifecycle] node_abort_payload node=34 before_objects=655 after_objects=655 before_bytes=11389747 after_bytes=11389747 before_reservations=98183 after_reservations=98183 before_releases=97528 after_releases=97528
[2026-11-02 11:35:30.358] [info] [graph][runtime.lifecycle] node_abort_payload node=15 before_objects=655 after_objects=655 before_bytes=11389747 after_bytes=11389747 before_reservations=98183 after_reservations=98183 before_releases=97528 after_releases=97528
[2026-11-02 11:35:30.358] [info] [graph][runtime.lifecycle] node_abort_payload node=59 before_objects=655 after_objects=655 before_bytes=11389747 after_bytes=11389747 before_reservations=98183 after_reservations=98183 before_releases=97528 after_releases=97528
[2026-11-02 11:35:30.358] [info] [graph][runtime.lifecycle] node_abort_payload node=55 before_objects=655 after_objects=655 before_bytes=11389747 after_bytes=11389747 before_reservations=98183 after_reservations=98183 before_releases=97528 after_releases=97528
[2026-11-02 11:35:30.358] [info] [graph][runtime.lifecycle] node_abort_payload node=54 before_objects=655 after_objects=655 before_bytes=11389747 after_bytes=11389747 before_reservations=98183 after_reservations=98183 before_releases=97528 after_releases=97528
[2026-11-02 11:35:30.358] [info] [graph][runtime.lifecycle] node_abort_payload node=36 before_objects=655 after_objects=655 before_bytes=11389747 after_bytes=11389747 before_reservations=98183 after_reservations=98183 before_releases=97528 after_releases=97528
[2026-11-02 11:35:30.358] [info] [graph][runtime.lifecycle] node_abort_payload node=33 before_objects=655 after_objects=655 before_bytes=11389747 after_bytes=11389747 before_reservations=98183 after_reservations=98183 before_releases=97528 after_releases=97528
[2026-11-02 11:35:30.358] [info] [graph][runtime.lifecycle] node_abort_payload node=17 before_objects=655 after_objects=655 before_bytes=11389747 after_bytes=11389747 before_reservations=98183 after_reservations=98183 before_releases=97528 after_releases=97528
[2026-11-02 11:35:30.358] [info] [graph][runtime.lifecycle] node_abort_payload node=14 before_objects=655 after_objects=655 before_bytes=11389747 after_bytes=11389747 before_reservations=98183 after_reservations=98183 before_releases=97528 after_releases=97528
[2026-11-02 11:35:30.358] [info] [graph][runtime.lifecycle] node_abort_payload node=53 before_objects=655 after_objects=655 before_bytes=11389747 after_bytes=11389747 before_reservations=98183 after_reservations=98183 before_releases=97528 after_releases=97528
[2026-11-02 11:35:30.358] [info] [graph][runtime.lifecycle] node_abort_payload node=32 before_objects=655 after_objects=655 before_bytes=11389747 after_bytes=11389747 before_reservations=98183 after_reservations=98183 before_releases=97528 after_releases=97528
[2026-11-02 11:35:30.358] [info] [graph][runtime.lifecycle] node_abort_payload node=13 before_objects=655 after_objects=655 before_bytes=11389747 after_bytes=11389747 before_reservations=98183 after_reservations=98183 before_releases=97528 after_releases=97528
[2026-11-02 11:35:30.358] [info] [graph][runtime.lifecycle] node_abort_payload node=52 before_objects=655 after_objects=655 before_bytes=11389747 after_bytes=11389747 before_reservations=98183 after_reservations=98183 before_releases=97528 after_releases=97528
[2026-11-02 11:35:30.359] [info] [graph][runtime.lifecycle] node_abort_payload node=51 before_objects=655 after_objects=655 before_bytes=11389747 after_bytes=11389747 before_reservations=98183 after_reservations=98183 before_releases=97528 after_releases=97528
[2026-11-02 11:35:30.359] [info] [graph][runtime.lifecycle] node_abort_payload node=50 before_objects=655 after_objects=654 before_bytes=11389747 after_bytes=11359506 before_reservations=98183 after_reservations=98183 before_releases=97528 after_releases=97529
[2026-11-02 11:35:30.359] [info] [graph][runtime.lifecycle] node_abort_payload node=31 before_objects=654 after_objects=654 before_bytes=11359506 after_bytes=11359506 before_reservations=98183 after_reservations=98183 before_releases=97529 after_releases=97529
[2026-11-02 11:35:30.359] [info] [graph][runtime.lifecycle] node_abort_payload node=30 before_objects=654 after_objects=653 before_bytes=11359506 after_bytes=11329265 before_reservations=98183 after_reservations=98183 before_releases=97529 after_releases=97530
[2026-11-02 11:35:30.359] [info] [graph][runtime.lifecycle] node_abort_payload node=12 before_objects=653 after_objects=653 before_bytes=11329265 after_bytes=11329265 before_reservations=98183 after_reservations=98183 before_releases=97530 after_releases=97530
[2026-11-02 11:35:30.359] [info] [graph][runtime.lifecycle] node_abort_payload node=11 before_objects=653 after_objects=652 before_bytes=11329265 after_bytes=11299024 before_reservations=98183 after_reservations=98183 before_releases=97530 after_releases=97531
[2026-11-02 11:35:30.359] [info] [graph][runtime.lifecycle] node_abort_payload node=49 before_objects=652 after_objects=652 before_bytes=11299024 after_bytes=11299024 before_reservations=98183 after_reservations=98183 before_releases=97531 after_releases=97531
[2026-11-02 11:35:30.359] [info] [graph][runtime.lifecycle] node_abort_payload node=29 before_objects=652 after_objects=652 before_bytes=11299024 after_bytes=11299024 before_reservations=98183 after_reservations=98183 before_releases=97531 after_releases=97531
[2026-11-02 11:35:30.359] [info] [graph][runtime.lifecycle] node_abort_payload node=10 before_objects=652 after_objects=652 before_bytes=11299024 after_bytes=11299024 before_reservations=98183 after_reservations=98183 before_releases=97531 after_releases=97531
[2026-11-02 11:35:30.359] [info] [graph][runtime.lifecycle] node_abort_payload node=69 before_objects=652 after_objects=652 before_bytes=11299024 after_bytes=11299024 before_reservations=98183 after_reservations=98183 before_releases=97531 after_releases=97531
[2026-11-02 11:35:30.359] [info] [graph][runtime.lifecycle] node_abort_payload node=48 before_objects=652 after_objects=652 before_bytes=11299024 after_bytes=11299024 before_reservations=98183 after_reservations=98183 before_releases=97531 after_releases=97531
[2026-11-02 11:35:30.359] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-11-02 11:35:30.360] [info] [graph][runtime.lifecycle] node_abort_payload node=63 before_objects=652 after_objects=652 before_bytes=11299024 after_bytes=11299024 before_reservations=98183 after_reservations=98183 before_releases=97531 after_releases=97531
[2026-11-02 11:35:30.360] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-11-02 11:35:30.360] [info] [graph][runtime.lifecycle] node_abort_payload node=40 before_objects=652 after_objects=652 before_bytes=11299024 after_bytes=11299024 before_reservations=98183 after_reservations=98183 before_releases=97531 after_releases=97531
[2026-11-02 11:35:30.360] [info] [graph][runtime.lifecycle] node_abort_payload node=28 before_objects=652 after_objects=652 before_bytes=11299024 after_bytes=11299024 before_reservations=98183 after_reservations=98183 before_releases=97531 after_releases=97531
[2026-11-02 11:35:30.360] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-11-02 11:35:30.360] [info] [graph][runtime.lifecycle] node_abort_payload node=21 before_objects=652 after_objects=652 before_bytes=11299024 after_bytes=11299024 before_reservations=98183 after_reservations=98183 before_releases=97531 after_releases=97531
[2026-11-02 11:35:30.360] [info] [graph][runtime.lifecycle] node_abort_payload node=9 before_objects=652 after_objects=652 before_bytes=11299024 after_bytes=11299024 before_reservations=98183 after_reservations=98183 before_releases=97531 after_releases=97531
[2026-11-02 11:35:30.360] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=4702 completion_waits=4702 completed_datagrams=4780 drained_datagrams=79 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-02 11:35:30.360] [info] [graph][runtime.lifecycle] node_abort_payload node=47 before_objects=652 after_objects=652 before_bytes=11299024 after_bytes=11299024 before_reservations=98183 after_reservations=98183 before_releases=97531 after_releases=97531
[2026-11-02 11:35:30.360] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=45874 completion_waits=45874 completed_datagrams=96835 drained_datagrams=50960 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-02 11:35:30.361] [info] [graph][runtime.lifecycle] node_abort_payload node=46 before_objects=652 after_objects=652 before_bytes=11299024 after_bytes=11299024 before_reservations=98183 after_reservations=98183 before_releases=97531 after_releases=97531
[2026-11-02 11:35:30.361] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=46955 completion_waits=46955 completed_datagrams=97083 drained_datagrams=50129 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-02 11:35:30.361] [info] [graph][runtime.lifecycle] node_abort_payload node=27 before_objects=652 after_objects=652 before_bytes=11299024 after_bytes=11299024 before_reservations=98183 after_reservations=98183 before_releases=97531 after_releases=97531
[2026-11-02 11:35:30.362] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=49465 completion_waits=49465 completed_datagrams=97027 drained_datagrams=47562 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-02 11:35:30.362] [info] [graph][runtime.lifecycle] node_abort_payload node=8 before_objects=652 after_objects=652 before_bytes=11299024 after_bytes=11299024 before_reservations=98183 after_reservations=98183 before_releases=97531 after_releases=97531
[2026-11-02 11:35:30.362] [info] [graph][runtime.lifecycle] node_abort_payload node=3 before_objects=652 after_objects=652 before_bytes=11299024 after_bytes=11299024 before_reservations=98183 after_reservations=98183 before_releases=97531 after_releases=97531
[2026-11-02 11:35:30.362] [info] [graph][runtime.node] codec_resolver.release slot=encoder_parameters bufferReferences=1 hasCredit=0 creditReferences=0
[2026-11-02 11:35:30.362] [info] [graph][runtime.lifecycle] node_abort_payload node=1 before_objects=652 after_objects=652 before_bytes=11299024 after_bytes=11299024 before_reservations=98183 after_reservations=98183 before_releases=97531 after_releases=97531
[2026-11-02 11:35:30.424] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=failed detail="MPEG-TS access unit has no transport service window stream=audio emit_ns=109588462266 dispatch_ns=109688462266 available_ns=109713634420 last_committed_kind=maintenance service_start_ns=109713634420 actual_now_ns=109713972800 remaining_ns=-25172154 payload_bytes=752 wire_bytes=752" description="" error=1 native=0 stage=3 reason="MPEG-TS access unit has no transport service window stream=audio emit_ns=109588462266 dispatch_ns=109688462266 available_ns=109713634420 last_committed_kind=maintenance service_start_ns=109713634420 actual_now_ns=109713972800 remaining_ns=-25172154 payload_bytes=752 wire_bytes=752"
[2026-11-02 11:35:30.424] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=retired detail="MPEG-TS access unit has no transport service window stream=audio emit_ns=109588462266 dispatch_ns=109688462266 available_ns=109713634420 last_committed_kind=maintenance service_start_ns=109713634420 actual_now_ns=109713972800 remaining_ns=-25172154 payload_bytes=752 wire_bytes=752" description="" error=1 native=0 stage=3 reason="MPEG-TS access unit has no transport service window stream=audio emit_ns=109588462266 dispatch_ns=109688462266 available_ns=109713634420 last_committed_kind=maintenance service_start_ns=109713634420 actual_now_ns=109713972800 remaining_ns=-25172154 payload_bytes=752 wire_bytes=752"
[CLI] final runtime report: queued=0, peakQueued=904, threads=81, processThreads=129, workers=0, workerProgress=284729, workerProcessCalls=1119185, workerWaits=834445, workerWakeups=826451, workerErrors=1, errors=1, stalledIntervals=0, cpuSamples=342, logicalProcessors=22, averageSystemMachineCpuPercent=11.943387, averageProcessMachineCpuPercent=1.994781, peakProcessMachineCpuPercent=5.625000, averageProcessSingleCoreCpuPercent=43.885183, peakProcessSingleCoreCpuPercent=123.750000, initialWorkingSetBytes=208842752, workingSetBytes=269451264, peakWorkingSetBytes=269869056, totalPushed=247106, totalPopped=246489, droppedBuffers=0, encodedPacketsPushed=34947, encodedPacketsPopped=34942, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=16030500, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=737, graphPayloadReservations=98183, graphPayloadReleases=98183, graphPayloadPressureFailures=1, lastInputReceivedAtNs=10005785130100
~~~

## 压力计数首次变化前后

~~~text
[CLI] runtime report: queued=604, peakQueued=902, threads=81, processThreads=129, workers=75, workerProgress=284026, workerProcessCalls=1116521, workerWaits=832488, workerWakeups=824508, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=341, logicalProcessors=22, averageSystemMachineCpuPercent=11.918427, averageProcessMachineCpuPercent=1.994572, peakProcessMachineCpuPercent=5.625000, averageProcessSingleCoreCpuPercent=43.880580, peakProcessSingleCoreCpuPercent=123.750000, initialWorkingSetBytes=208842752, workingSetBytes=269451264, peakWorkingSetBytes=269869056, totalPushed=246488, totalPopped=245884, droppedBuffers=0, encodedPacketsPushed=34876, encodedPacketsPopped=34875, backpressureItems=123, graphPayloadCurrentBytes=11143259, graphPayloadHighWaterBytes=16030500, graphPayloadCurrentObjects=718, graphPayloadHighWaterObjects=730, graphPayloadReservations=97944, graphPayloadReleases=97226, graphPayloadPressureFailures=0, lastInputReceivedAtNs=10005478509100
[2026-11-02 11:35:29.911] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=109545536200 present_generation=1 present_canonical_target_ns=104118276643 present_canonical_dispatch_ns=104118276643 present_target_master_ns=109665242311 present_dispatch_master_ns=109665242311
[2026-11-02 11:35:29.934] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=109568404100 present_generation=1 present_canonical_target_ns=104133333333 present_canonical_dispatch_ns=104133333333 present_target_master_ns=109680299001 present_dispatch_master_ns=109680299001
[2026-11-02 11:35:29.934] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=109569067200 present_generation=1 present_canonical_target_ns=104141496598 present_canonical_dispatch_ns=104141496598 present_target_master_ns=109688462266 present_dispatch_master_ns=109688462266
[CLI] runtime report: queued=617, peakQueued=904, threads=81, processThreads=129, workers=75, workerProgress=284661, workerProcessCalls=1118982, workerWaits=834309, workerWakeups=826315, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=342, logicalProcessors=22, averageSystemMachineCpuPercent=11.943387, averageProcessMachineCpuPercent=1.994781, peakProcessMachineCpuPercent=5.625000, averageProcessSingleCoreCpuPercent=43.885183, peakProcessSingleCoreCpuPercent=123.750000, initialWorkingSetBytes=208842752, workingSetBytes=269451264, peakWorkingSetBytes=269869056, totalPushed=247045, totalPopped=246428, droppedBuffers=0, encodedPacketsPushed=34937, encodedPacketsPopped=34932, backpressureItems=123, graphPayloadCurrentBytes=11510925, graphPayloadHighWaterBytes=16030500, graphPayloadCurrentObjects=731, graphPayloadHighWaterObjects=737, graphPayloadReservations=98155, graphPayloadReleases=97424, graphPayloadPressureFailures=1, lastInputReceivedAtNs=10005769190400
~~~

## cli-error.log

~~~text
[in @ 0000019fdc6d0140] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000019fdc6d0140] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 6832.627189
[in @ 0000019fdcdd9bc0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000019fdcdd9bc0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 33464.049222
[in @ 0000019fdcd1c640] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000019fdcd1c640] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 6663.069556
[in @ 0000019fc86a8fc0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000019fe21cbe80] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000019fe21cbe80] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 33464.049222
[in @ 0000019fc86a8dc0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000019fc86a8fc0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 6663.069556
[in @ 0000019fc86a8dc0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 6832.627189
[aac @ 0000019fdc6c9f40] Qavg: 634.624
[aac @ 0000019fdc6c9f40] 2 frames left in the queue on closing
[CLI] realtime video runtime failed: InvalidArgument: MPEG-TS access unit has no transport service window stream=audio emit_ns=109588462266 dispatch_ns=109688462266 available_ns=109713634420 last_committed_kind=maintenance service_start_ns=109713634420 actual_now_ns=109713972800 remaining_ns=-25172154 payload_bytes=752 wire_bytes=752

~~~

## monitor.csv

~~~text
"Time","CpuPercent","WorkingSet","PrivateBytes"
"2026-11-02T11:33:41.3871114+08:00","0.7738279836603187","142647296","567775232"
"2026-11-02T11:33:42.4215594+08:00","0.1373152198519931","145498112","570265600"
"2026-11-02T11:33:43.4320031+08:00","0.07028865366049318","149573632","574058496"
"2026-11-02T11:33:44.4504941+08:00","0.06973328902535934","152420352","576897024"
"2026-11-02T11:33:45.4752418+08:00","0.4851526779802393","202776576","717549568"
"2026-11-02T11:33:46.4862743+08:00","4.425606316495086","251965440","1250217984"
"2026-11-02T11:33:47.5090588+08:00","2.3609790012194427","252829696","1250594816"
"2026-11-02T11:33:48.5235323+08:00","1.8202455846218841","254210048","1251954688"
"2026-11-02T11:33:49.5406231+08:00","2.2345372436042803","255856640","1251524608"
"2026-11-02T11:33:50.5529665+08:00","2.525643158060972","256667648","1252335616"
"2026-11-02T11:33:51.5632680+08:00","2.5307476845458328","256176128","1251532800"
"2026-11-02T11:33:52.5775068+08:00","2.3808719675018617","258240512","1253560320"
"2026-11-02T11:33:53.5916307+08:00","2.1010073997682315","258424832","1253564416"
"2026-11-02T11:33:54.6034474+08:00","1.4740587625478732","258433024","1253572608"
"2026-11-02T11:33:55.6241473+08:00","1.8787242326208087","258605056","1253572608"
"2026-11-02T11:33:56.6442078+08:00","1.879901865000788","259555328","1254576128"
"2026-11-02T11:33:57.6570489+08:00","2.734768922426592","261193728","1256054784"
"2026-11-02T11:33:58.6667469+08:00","2.3915791922661302","261218304","1256091648"
"2026-11-02T11:33:59.6835696+08:00","2.444669512733591","261218304","1256091648"
"2026-11-02T11:34:00.6986129+08:00","2.9387461061558113","261345280","1256091648"
"2026-11-02T11:34:01.7205248+08:00","1.8764960427250492","261402624","1256091648"
"2026-11-02T11:34:02.7396134+08:00","3.484619849183245","261406720","1256091648"
"2026-11-02T11:34:03.7512858+08:00","2.1060985929652905","261443584","1256099840"
"2026-11-02T11:34:04.7626580+08:00","2.3876202324650877","261615616","1256099840"
"2026-11-02T11:34:05.7802052+08:00","1.3959593672456132","261746688","1256099840"
"2026-11-02T11:34:06.7936156+08:00","3.363978610334875","261746688","1256099840"
"2026-11-02T11:34:07.8035765+08:00","2.0393453755576982","261746688","1256099840"
"2026-11-02T11:34:08.8217186+08:00","2.0927155631633525","261861376","1256099840"
"2026-11-02T11:34:09.8354780+08:00","1.8915865405180325","261918720","1256099840"
"2026-11-02T11:34:10.8436323+08:00","2.1134481281107647","261918720","1256099840"
"2026-11-02T11:34:11.8559550+08:00","2.736169369348691","262381568","1256509440"
"2026-11-02T11:34:12.8741366+08:00","1.5345985431282594","262856704","1256980480"
"2026-11-02T11:34:13.8875632+08:00","2.3827800920882947","263720960","1257668608"
"2026-11-02T11:34:14.9018779+08:00","1.820530560279674","263733248","1257676800"
"2026-11-02T11:34:15.9209883+08:00","2.717945340991873","263737344","1257680896"
"2026-11-02T11:34:16.9303107+08:00","2.3924691726575444","263720960","1257680896"
"2026-11-02T11:34:17.9436613+08:00","1.5419145160618641","263720960","1257680896"
"2026-11-02T11:34:18.9528647+08:00","2.1816261671874524","263786496","1257689088"
"2026-11-02T11:34:19.9621930+08:00","1.8998908842282896","263966720","1257897984"
"2026-11-02T11:34:20.9793837+08:00","1.3964486162275624","264388608","1258295296"
"2026-11-02T11:34:21.9917254+08:00","1.8942355494825873","267481088","1261494272"
"2026-11-02T11:34:23.0064208+08:00","1.9598357927279102","266956800","1260683264"
"2026-11-02T11:34:24.0187518+08:00","1.4733098884922742","266956800","1260683264"
"2026-11-02T11:34:25.0285619+08:00","1.8989844094088943","267014144","1260683264"
"2026-11-02T11:34:26.0393526+08:00","2.248464763998395","267132928","1260683264"
"2026-11-02T11:34:27.0576290+08:00","1.5344556743139683","264335360","1257709568"
"2026-11-02T11:34:28.0715611+08:00","1.8912643522812191","264699904","1258262528"
"2026-11-02T11:34:29.0834975+08:00","1.8248092558889168","265719808","1259073536"
"2026-11-02T11:34:30.0920997+08:00","1.6900076705617486","267177984","1260519424"
"2026-11-02T11:34:31.1006325+08:00","1.9013894603761388","267329536","1260519424"
"2026-11-02T11:34:32.1173262+08:00","1.3971312554160074","267399168","1260519424"
"2026-11-02T11:34:33.1313809+08:00","1.680920619514366","267464704","1260519424"
"2026-11-02T11:34:34.1383576+08:00","2.256980993430407","267517952","1260519424"
"2026-11-02T11:34:35.1721567+08:00","2.4045246842887122","267517952","1260519424"
"2026-11-02T11:34:36.1886507+08:00","2.0262383161229587","267575296","1260519424"
"2026-11-02T11:34:37.1959399+08:00","1.1986491701056297","266354688","1259266048"
"2026-11-02T11:34:38.2107962+08:00","1.4696438034894919","266354688","1259266048"
"2026-11-02T11:34:39.2208484+08:00","1.968845138534784","266354688","1259266048"
"2026-11-02T11:34:40.2357256+08:00","1.5395951352538022","266416128","1259266048"
"2026-11-02T11:34:41.2411554+08:00","1.8366184382946569","266346496","1259155456"
"2026-11-02T11:34:42.2581564+08:00","1.4665445488522357","266346496","1259155456"
"2026-11-02T11:34:43.2782127+08:00","1.2532730702306245","266346496","1259155456"
"2026-11-02T11:34:44.2898738+08:00","2.3167343293124545","266403840","1259155456"
"2026-11-02T11:34:45.3107369+08:00","1.4609963595777662","266403840","1259155456"
"2026-11-02T11:34:46.3316417+08:00","2.434894472575165","266403840","1259155456"
"2026-11-02T11:34:47.3490133+08:00","1.7452503901408116","268009472","1260736512"
"2026-11-02T11:34:48.3677708+08:00","1.394300945469894","268812288","1261539328"
"2026-11-02T11:34:49.3877383+08:00","2.2978673340081914","266825728","1259552768"
"2026-11-02T11:34:50.4000082+08:00","1.0524277261340174","266825728","1259552768"
"2026-11-02T11:34:51.4146661+08:00","1.4699311686503131","264433664","1257152512"
"2026-11-02T11:34:52.4210950+08:00","2.328778515799775","265342976","1258172416"
"2026-11-02T11:34:53.4365692+08:00","1.4687495484644244","266117120","1259028480"
"2026-11-02T11:34:54.4430950+08:00","2.0463053117059604","266932224","1259823104"
"2026-11-02T11:34:55.4531752+08:00","1.546906869375323","268206080","1260941312"
"2026-11-02T11:34:56.4678043+08:00","1.9599638563849227","268210176","1260941312"
"2026-11-02T11:34:57.4776963+08:00","2.1801386142820673","268210176","1260941312"
"2026-11-02T11:34:58.4955850+08:00","2.1630110889869836","268210176","1260941312"
"2026-11-02T11:34:59.5109340+08:00","2.0984723658385622","268222464","1260949504"
"2026-11-02T11:35:00.5280029+08:00","1.3966158491863683","268238848","1260949504"
"2026-11-02T11:35:01.5416564+08:00","1.821718081268312","268238848","1260949504"
"2026-11-02T11:35:02.5554743+08:00","2.101641545470659","268300288","1260949504"
"2026-11-02T11:35:03.5715925+08:00","2.865741227921927","268619776","1261416448"
"2026-11-02T11:35:04.5865042+08:00","1.6795012359651134","268619776","1261416448"
"2026-11-02T11:35:05.6062013+08:00","2.1591750584115084","268619776","1261416448"
"2026-11-02T11:35:06.6191713+08:00","2.3137407820567244","267489280","1260285952"
"2026-11-02T11:35:07.6321327+08:00","1.822962759578903","267567104","1260285952"
"2026-11-02T11:35:08.6440732+08:00","2.456464045608862","267829248","1260285952"
"2026-11-02T11:35:09.6515032+08:00","1.5509762464885897","267833344","1260285952"
"2026-11-02T11:35:10.6732601+08:00","2.919436654114639","267952128","1260285952"
"2026-11-02T11:35:11.6867471+08:00","1.9621725425549252","268734464","1261088768"
"2026-11-02T11:35:12.7047441+08:00","1.7441782066334006","268791808","1261088768"
"2026-11-02T11:35:13.7204937+08:00","1.9578017688969445","268791808","1261088768"
"2026-11-02T11:35:14.7436479+08:00","2.2907104325037224","268791808","1261088768"
"2026-11-02T11:35:15.7656212+08:00","2.362853048384657","267452416","1259728896"
"2026-11-02T11:35:16.7838626+08:00","1.95301071399804","267452416","1259728896"
"2026-11-02T11:35:17.8049126+08:00","1.321611887940667","268255232","1260531712"
"2026-11-02T11:35:18.8230704+08:00","1.9531710739105115","267452416","1259728896"
"2026-11-02T11:35:19.8420377+08:00","1.184911786311851","269131776","1261477888"
"2026-11-02T11:35:20.8605297+08:00","1.9525301756286388","269860864","1262219264"
"2026-11-02T11:35:21.8817082+08:00","1.8082939555532251","269869056","1262219264"
"2026-11-02T11:35:22.8917938+08:00","1.617212172188899","269869056","1262219264"
"2026-11-02T11:35:23.9096796+08:00","2.0234677514010815","267714560","1259876352"
"2026-11-02T11:35:24.9292857+08:00","2.5076528885205587","267714560","1259876352"
"2026-11-02T11:35:25.9427990+08:00","1.8219700807980603","267776000","1259876352"
"2026-11-02T11:35:26.9512059+08:00","2.324210593957657","267776000","1259876352"
"2026-11-02T11:35:27.9619021+08:00","1.6162351528310162","269455360","1261641728"
"2026-11-02T11:35:28.9672203+08:00","1.342293234303147","269447168","1261641728"
"2026-11-02T11:35:29.9860890+08:00","1.3941487705477118","269451264","1261641728"

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
file debug: opening logfile `D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r379-vlc.log'
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
cache_read debug: pre-buffering done 300 bytes in 0s - 292968 KiB/s
main debug: using stream_filter module "cache_read"
main debug: looking for stream_filter module matching "any": 24 candidates
playlist debug: using XSPF playlist reader
main debug: using stream_filter module "playlist"
main debug: stream filter added to 030abc38
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
lua debug: skipping script (unmatched scope) D:\VideoLAN\VLC\lua\meta\art\00_musicbrainz.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\art\01_googleimage.luac
main debug: using access module "udp"
main debug: looking for stream_filter module matching "prefetch,cache_block": 24 candidates
prefetch debug: using 16777216 bytes buffer, 16777216 bytes read
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
main debug: resized to 83083008x50927688
main debug: resized to 83083008x50927688
main debug: resized to 83082496x50927688
main debug: resized to 83082496x50927688
main debug: resized to 83082240x50927688
main debug: resized to 100x30
main debug: resized to 100x30
main debug: resized to 100x30
main debug: resized to 100x30
main debug: resized to 100x30
main debug: resized to 100x30
main debug: resized to 1121x833
main debug: Buffering 19%
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
wasapi debug: modified format
direct3d11 debug: inclinometer not found. (hr=0x80070490)
direct3d11 debug: created video sub-window
direct3d11 debug: Vout EventThread running
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
direct3d11 debug: D3D11 pool succeed with 48 surfaces (1280x736) context 0x0b1e00e0
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
qt debug: Logical video size: 1280x720
d3d11va debug: - 'Unknown decoder 0x4008018f-0xf537-0x4b36-0x98cf61af8a2c1a33' is supported
d3d11va debug: - 'Unknown decoder 0x0dabeffa-0x4458-0x4602-0xbc030795659d617c' is supported
main debug: resized to 1280x720
d3d11va debug: - 'Unknown decoder 0x9798634d-0xfe9d-0x48e5-0xb4dadbec45b3df01' is supported
main debug: VoutDisplayEvent 'resize' 1280x720
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
main debug: Buffering 46%
main debug: Received first picture
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
main debug: Stream buffering done (1066 ms in 930 ms)
main debug: Decoder wait done in 0 ms
main debug: inserting 9594 zeroes
mmdevice debug: state changed: 1
main warning: playback too late (80430): up-sampling
main debug: auto hiding mouse cursor
main warning: picture is too late to be displayed (missing 33 ms)
main debug: picture might be displayed late (missing 0 ms)
main debug: resampling stopped (drift: -13446 us)
main debug: accepted socket 3276 (from socket 1176)
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
main debug: Filter 'd3d11_filters' (05411a58) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (054117b0) appended to chain
main debug: using video converter module "chain"
main debug: Filter 'chain' (05411508) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: RV24 -> 1280x720 (1280x720) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (05412250) appended to chain
main debug: using video converter module "chain"
main debug: removing module "png"
main debug: removing module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 05411a58 removed from chain
main debug: removing module "swscale"
main debug: Filter 054117b0 removed from chain
main debug: Filter 05411508 removed from chain
main debug: removing module "swscale"
main debug: Filter 05412250 removed from chain
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r379-native-2026-11-02-11h34m13s760.png)
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (054124f8) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: YUVA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (054117b0) appended to chain
main debug: using video converter module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 054124f8 removed from chain
main debug: removing module "swscale"
main debug: Filter 054117b0 removed from chain
main error: read error: No error
mmdevice debug: volume ducked by {0.0.0.00000000}.{e03f8b3b-e487-4c80-becd-d8743a6e82f9}|\Device\HarddiskVolume3\Windows\System32\svchost.exe%b{1DCF6A6F-E0E5-45F6-BE45-18CC8E958ADF}|0%b3544 of 1 sessions
main debug: corks count: 0 -> 1
main debug: corked (effective)
main debug: toggling resume
mmdevice debug: state changed: 0
main debug: toggling resume
mmdevice debug: volume unducked by {0.0.0.00000000}.{e03f8b3b-e487-4c80-becd-d8743a6e82f9}|\Device\HarddiskVolume3\Windows\System32\svchost.exe%b{1DCF6A6F-E0E5-45F6-BE45-18CC8E958ADF}|0%b3544
main debug: corks count: 1 -> 0
main debug: uncorked (effective)
main debug: toggling pause
main debug: toggling pause
mmdevice debug: state changed: 1
main debug: accepted socket 3196 (from socket 1176)
main debug: exiting
main debug: exiting
main debug: no exit handler
main debug: removing all interfaces
main debug: removing module "qt"
main debug: deactivating the playlist
main debug: incoming request - stopping current input
prefetch debug: end of stream
ts debug: Can't read TS packet at 89335532
main debug: EOF reached
main warning: es out timeshift: auto stop
main error: read error: No error
main warning: buffer too late (-31303019 us): dropped
main warning: picture is too late to be displayed (missing 31621 ms)
main warning: picture is too late to be displayed (missing 31588 ms)
main warning: picture is too late to be displayed (missing 31555 ms)
main warning: picture is too late to be displayed (missing 31522 ms)
main warning: picture is too late to be displayed (missing 31488 ms)
main debug: killing decoder fourcc `hevc'
main debug: removing module "avcodec"
main debug: saving a free vout
main debug: reusing provided vout
main debug: removing module "hevc"
main debug: killing decoder fourcc `mp4a'
main debug: removing module "faad"
mmdevice debug: state changed: 0
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
main debug: saving media library to file C:\Users\ROG\AppData\Roaming\vlc\ml.xspf.tmp6784
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
frame= 3559 fps= 30 q=-1.0 size=  117543KiB time=00:01:58.63 bitrate=8116.7kbits/s speed=   1x elapsed=0:01:58.13    
frame= 3575 fps= 30 q=-1.0 size=  118060KiB time=00:01:59.16 bitrate=8115.9kbits/s speed=   1x elapsed=0:01:58.65    
frame= 3591 fps= 30 q=-1.0 size=  118561KiB time=00:01:59.70 bitrate=8114.0kbits/s speed=   1x elapsed=0:01:59.17    
[out#0/rtp @ 0000027b6e53a440] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
[out#1/rtp @ 0000027b6e5c8740] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.49    
~~~

## source1最终记录

~~~text
frame= 3544 fps= 30 q=-1.0 size=  117123KiB time=00:01:58.13 bitrate=8121.9kbits/s speed=   1x elapsed=0:01:57.62    
frame= 3559 fps= 30 q=-1.0 size=  117543KiB time=00:01:58.63 bitrate=8116.7kbits/s speed=   1x elapsed=0:01:58.13    
frame= 3575 fps= 30 q=-1.0 size=  118060KiB time=00:01:59.16 bitrate=8115.9kbits/s speed=   1x elapsed=0:01:58.65    
frame= 3590 fps= 30 q=-1.0 size=  118545KiB time=00:01:59.66 bitrate=8115.2kbits/s speed=   1x elapsed=0:01:59.16    
[out#0/rtp @ 000002297f08a740] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.47    
~~~

## source2最终记录

~~~text
frame= 3544 fps= 30 q=-1.0 size=  117123KiB time=00:01:58.13 bitrate=8121.9kbits/s speed=   1x elapsed=0:01:57.61    
frame= 3559 fps= 30 q=-1.0 size=  117543KiB time=00:01:58.63 bitrate=8116.7kbits/s speed=   1x elapsed=0:01:58.12    
frame= 3575 fps= 30 q=-1.0 size=  118060KiB time=00:01:59.16 bitrate=8115.9kbits/s speed=   1x elapsed=0:01:58.64    
frame= 3590 fps= 30 q=-1.0 size=  118545KiB time=00:01:59.66 bitrate=8115.2kbits/s speed=   1x elapsed=0:01:59.16    
[out#0/rtp @ 000001f93cfd8fc0] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.47    
~~~

## 本轮临时文件清单

- composition-r379-cli-error.log (2363 bytes)
- composition-r379-cli.log (587349 bytes)
- composition-r379-monitor.csv (8978 bytes)
- composition-r379-native-2026-11-02-11h34m13s760.png (877680 bytes)
- composition-r379-source0-stdout.log (540 bytes)
- composition-r379-source0.log (29521 bytes)
- composition-r379-source1-stdout.log (335 bytes)
- composition-r379-source1.log (28926 bytes)
- composition-r379-source2-stdout.log (335 bytes)
- composition-r379-source2.log (28926 bytes)
- composition-r379-vlc.log (32313 bytes)

清理结果：上述11项产物已按清单逐项删除，本轮文件残留0；指定120秒测试源保留。

## 同帧依赖核验

后续源码及原始日志核对：长compose对应video_cursor=3125；MediaAvContinuousAggregateVideo在compose成功后才发布，canonical序号明确为cursor+1，即3126。scheduler首错记录的head_resumed_sequence也为3126，且持有的audio为4486。因此已建立具体帧的合成完成→视频head→音频晚提交依赖，不仅是两个独立最大值的时间接近。仍无法把cuStreamSynchronize壁钟132.5144ms解释为GPU执行时长；仅sender MMCSS不覆盖此上游依赖。下一诊断保留同步和期限，补同batch CUDA事件时间及主机调用区间，不直接删除等待。
