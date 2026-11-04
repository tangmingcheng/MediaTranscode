# r376：普通音视频源节点声明回归

日期：2026-11-02。结果：FAIL，重入阶段发送预约失败。

本轮版本：普通Video/Audio source节点声明8文件双独立局部PASS后，Release clean-first/all-target 778目标重建成功；realtime产物10:28:06。后续Input/Protocol位置投影与最差成功提交样本正在源码实施，均不属于本次运行版本。

## 实际启动命令

```powershell
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r376-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r376-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r376-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r376-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r376-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r376-source2.log' -PassThru
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r376 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r376-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r376-cli-error.log' -PassThru
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r376-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r376-native- --snapshot-format=png udp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
```

PID：source0=17296、source1=18416、source2=12540、CLI=33268、VLC=28056。启动前同环境CIM和端口检查无残留；启动后UDP62024和TCP63020均归VLC28056。

## 画面观察

- 已实际查看 composition-r376-native-2026-11-02-10h31m03s759.png：三个游戏画面，右下黑格；来自本轮独占VLC原生snapshot。

实际快照命令：
```powershell
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
try{$s=$c.GetStream();$s.ReadTimeout=2000;$b=[Text.Encoding]::ASCII.GetBytes('snapshot'+[char]10);$s.Write($b,0,$b.Length);$buf=[byte[]]::new(4096);try{$n=$s.Read($buf,0,$buf.Length);[Text.Encoding]::UTF8.GetString($buf,0,$n)}catch{Write-Output $_.Exception.Message}}finally{$c.Dispose()}
```

- 已查看 composition-r376-native-2026-11-02-10h33m03s732.png：初始源结束后全黑，CLI持续输出。
- 重入源PID14420，实际命令：
```powershell
$reentry=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r376-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r376-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($reentry.Id)"
$reentry.WaitForExit()
Write-Output "REENTRY_EXIT=$($reentry.ExitCode)"
```
- 已查看 composition-r376-native-2026-11-02-10h33m54s745.png：仅左下所选源恢复，其余黑格。

## 结果：FAIL

三初始源PID17296/18416/12540及重入PID14420均完整120秒/3600帧退出0；CLI PID33268退出1，由原发送预约失败终止，不是caller-stop。第二次全黑阶段未完成，不以VLC最后静止画面冒充连续输出。

首因：seq155923 not_before=263429099540 > not_after=263429068224，差31316ns；原协议deadline=263462646001、prefix completion limit=263429283264，原期限未放宽。最终workerErrors/errors=2，后续decoder/encoder credit取消属于失败排空。201377预约/释放匹配归零仍不能代表通过。平均进程整机CPU1.673312%、峰5.615551%；WS初始208199680、峰268324864、最终259301376 bytes。

另一次graphPayloadPressureFailures=1经源码核验是新增waiter的WouldBlock背压计数，不等于终止错误。node2 encoder payload wait6315103237800→6315104039500，0.8017ms后恢复，与samples642/643唯一0→1高度一致；发生于发送失败约55秒前，不将两者混作因果。当前缺owner/account当时bytes/objects占用，尚不能进一步归因。

全部六个本轮PID及UDP62024/TCP63020在同执行环境已核验无残留。VLC退出命令如下：
```powershell
$v=Get-CimInstance Win32_Process -Filter "ProcessId=28056"
if(-not $v -or $v.Name -ne 'vlc.exe' -or $v.CommandLine -notlike '*composition-r376*'){throw 'VLC identity mismatch'}
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 28056){throw 'VLC port mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
try{$s=$c.GetStream();$b=[Text.Encoding]::ASCII.GetBytes('quit'+[char]10);$s.Write($b,0,$b.Length)}finally{$c.Dispose()}
```

## 非周期CLI诊断及背压变化采样

~~~text
[CLI] composition_inputs=3 grid=fixed audio_source=2 audio=on max_duration=source_driven hw=planner-highest-score
[2026-11-02 10:30:31.485] [info] [graph][planner.capability] composition_protocol_installed source=0
[2026-11-02 10:30:31.485] [info] [graph][planner.capability] composition_protocol_installed source=1
[2026-11-02 10:30:31.488] [info] [graph][planner.capability] composition_protocol_installed source=2
[2026-11-02 10:30:31.577] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 10:30:31.578] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 10:30:31.578] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-r376.source.0 decoder=h264
[2026-11-02 10:30:35.970] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-11-02 10:30:35.970] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-11-02 10:30:35.971] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-11-02 10:30:36.050] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 10:30:36.051] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 10:30:36.051] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-r376.source.1 decoder=h264
[2026-11-02 10:30:36.126] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 10:30:36.126] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 10:30:36.127] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-r376.source.2 decoder=h264
[2026-11-02 10:30:36.247] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-11-02 10:30:36.310] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-11-02 10:30:36.312] [info] [graph][planner.capability] canvas_cuda_stream stage=prepared device_stream=0000000000000000 private_nonblocking=1
[2026-11-02 10:30:36.321] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 10:30:36.321] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 10:30:36.322] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-11-02 10:30:36.327] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 10:30:36.327] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 10:30:36.327] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-11-02 10:30:36.333] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 10:30:36.333] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 10:30:36.333] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-11-02 10:30:36.375] [info] [graph][planner.capability] composition_preparation_storage stage=graph_bound scope=shared_reservations capacity=2147483648 retained=832070464 high_water=858196799
[CLI] audio_plan branch=transcode_frame reason=transcode_required_by_output_frame_contract codec=aac sample_rate=44100 channels=2 access_unit_samples=1024 encoder=aac bitrate_kbps=192
[CLI] prepared_output_id=1
[CLI] selected_chain=fixed-grid-composition score=0 decoder=h264,h264,h264 filter=scale_cuda=640:360,scale_cuda=640:360,scale_cuda=640:360 encoder=hevc_nvenc
[2026-11-02 10:30:36.378] [info] [graph][runtime.lifecycle] compile.begin
[2026-11-02 10:30:36.380] [info] [graph][runtime.channel] create channel=1 edge=1 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:30:36.381] [info] [graph][runtime.channel] create channel=2 edge=2 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:30:36.381] [info] [graph][runtime.channel] create channel=3 edge=3 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-02 10:30:36.381] [info] [graph][runtime.channel] create channel=4 edge=4 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.381] [info] [graph][runtime.channel] create channel=5 edge=5 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.381] [info] [graph][runtime.channel] create channel=6 edge=6 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.381] [info] [graph][runtime.channel] create channel=7 edge=7 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.382] [info] [graph][runtime.channel] create channel=8 edge=8 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.382] [info] [graph][runtime.channel] create channel=9 edge=9 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:30:36.382] [info] [graph][runtime.channel] create channel=10 edge=10 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.382] [info] [graph][runtime.channel] create channel=11 edge=11 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.382] [info] [graph][runtime.channel] create channel=12 edge=12 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:30:36.382] [info] [graph][runtime.channel] create channel=13 edge=13 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:30:36.382] [info] [graph][runtime.channel] create channel=14 edge=14 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.382] [info] [graph][runtime.channel] create channel=15 edge=15 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.382] [info] [graph][runtime.channel] create channel=16 edge=16 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.382] [info] [graph][runtime.channel] create channel=17 edge=17 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.382] [info] [graph][runtime.channel] create channel=18 edge=18 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.382] [info] [graph][runtime.channel] create channel=19 edge=19 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.382] [info] [graph][runtime.channel] create channel=20 edge=20 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.382] [info] [graph][runtime.channel] create channel=21 edge=21 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-02 10:30:36.382] [info] [graph][runtime.channel] create channel=22 edge=22 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:30:36.382] [info] [graph][runtime.channel] create channel=23 edge=23 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:30:36.383] [info] [graph][runtime.channel] create channel=24 edge=24 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:30:36.383] [info] [graph][runtime.channel] create channel=25 edge=25 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:30:36.383] [info] [graph][runtime.channel] create channel=26 edge=26 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:30:36.383] [info] [graph][runtime.channel] create channel=27 edge=27 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:30:36.383] [info] [graph][runtime.channel] create channel=28 edge=28 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:30:36.383] [info] [graph][runtime.channel] create channel=29 edge=29 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:30:36.383] [info] [graph][runtime.channel] create channel=30 edge=30 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.383] [info] [graph][runtime.channel] create channel=31 edge=31 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.383] [info] [graph][runtime.channel] create channel=32 edge=32 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.383] [info] [graph][runtime.channel] create channel=33 edge=33 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.383] [info] [graph][runtime.channel] create channel=34 edge=34 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.383] [info] [graph][runtime.channel] create channel=35 edge=35 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.383] [info] [graph][runtime.channel] create channel=36 edge=36 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:30:36.383] [info] [graph][runtime.channel] create channel=37 edge=37 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.384] [info] [graph][runtime.channel] create channel=38 edge=38 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.384] [info] [graph][runtime.channel] create channel=39 edge=39 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:30:36.384] [info] [graph][runtime.channel] create channel=40 edge=40 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:30:36.384] [info] [graph][runtime.channel] create channel=41 edge=41 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.384] [info] [graph][runtime.channel] create channel=42 edge=42 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.384] [info] [graph][runtime.channel] create channel=43 edge=43 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.384] [info] [graph][runtime.channel] create channel=44 edge=44 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.384] [info] [graph][runtime.channel] create channel=45 edge=45 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.384] [info] [graph][runtime.channel] create channel=46 edge=46 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.384] [info] [graph][runtime.channel] create channel=47 edge=47 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.384] [info] [graph][runtime.channel] create channel=48 edge=48 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-02 10:30:36.384] [info] [graph][runtime.channel] create channel=49 edge=49 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:30:36.384] [info] [graph][runtime.channel] create channel=50 edge=50 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:30:36.384] [info] [graph][runtime.channel] create channel=51 edge=51 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:30:36.384] [info] [graph][runtime.channel] create channel=52 edge=52 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:30:36.384] [info] [graph][runtime.channel] create channel=53 edge=53 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:30:36.385] [info] [graph][runtime.channel] create channel=54 edge=54 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:30:36.385] [info] [graph][runtime.channel] create channel=55 edge=55 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:30:36.385] [info] [graph][runtime.channel] create channel=56 edge=56 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:30:36.385] [info] [graph][runtime.channel] create channel=57 edge=57 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.385] [info] [graph][runtime.channel] create channel=58 edge=58 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.385] [info] [graph][runtime.channel] create channel=59 edge=59 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.385] [info] [graph][runtime.channel] create channel=60 edge=60 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.385] [info] [graph][runtime.channel] create channel=61 edge=61 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.385] [info] [graph][runtime.channel] create channel=62 edge=62 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.385] [info] [graph][runtime.channel] create channel=63 edge=63 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.385] [info] [graph][runtime.channel] create channel=64 edge=64 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.385] [info] [graph][runtime.channel] create channel=65 edge=65 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.385] [info] [graph][runtime.channel] create channel=66 edge=66 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:30:36.385] [info] [graph][runtime.channel] create channel=67 edge=67 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:30:36.385] [info] [graph][runtime.channel] create channel=68 edge=68 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.385] [info] [graph][runtime.channel] create channel=69 edge=69 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.386] [info] [graph][runtime.channel] create channel=70 edge=70 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.386] [info] [graph][runtime.channel] create channel=71 edge=71 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:30:36.386] [info] [graph][runtime.channel] create channel=72 edge=72 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:30:36.386] [info] [graph][runtime.channel] create channel=73 edge=73 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:30:36.386] [info] [graph][runtime.channel] create channel=74 edge=74 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:30:36.386] [info] [graph][runtime.channel] create channel=75 edge=75 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.386] [info] [graph][runtime.channel] create channel=76 edge=76 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.386] [info] [graph][runtime.channel] create channel=77 edge=77 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.386] [info] [graph][runtime.channel] create channel=78 edge=78 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.386] [info] [graph][runtime.channel] create channel=79 edge=79 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.386] [info] [graph][runtime.channel] create channel=80 edge=80 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.387] [info] [graph][runtime.channel] create channel=81 edge=81 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.387] [info] [graph][runtime.channel] create channel=82 edge=82 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.387] [info] [graph][runtime.channel] create channel=83 edge=83 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-02 10:30:36.387] [info] [graph][runtime.channel] create channel=84 edge=84 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:30:36.387] [info] [graph][runtime.channel] create channel=85 edge=85 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:30:36.387] [info] [graph][runtime.channel] create channel=86 edge=86 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:30:36.387] [info] [graph][runtime.channel] create channel=87 edge=87 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:30:36.387] [info] [graph][runtime.channel] create channel=88 edge=88 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:30:36.388] [info] [graph][runtime.channel] create channel=89 edge=89 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:30:36.388] [info] [graph][runtime.channel] create channel=90 edge=90 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:30:36.388] [info] [graph][runtime.channel] create channel=91 edge=91 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-02 10:30:36.388] [info] [graph][runtime.channel] create channel=92 edge=92 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:30:36.388] [info] [graph][runtime.channel] create channel=93 edge=93 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 10:30:36.388] [info] [graph][runtime.channel] create channel=94 edge=94 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:30:36.388] [info] [graph][runtime.channel] create channel=95 edge=95 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:30:36.388] [info] [graph][runtime.channel] create channel=96 edge=96 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:30:36.388] [info] [graph][runtime.channel] create channel=97 edge=97 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:30:36.388] [info] [graph][runtime.channel] create channel=98 edge=98 stream=Audio edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.388] [info] [graph][runtime.channel] create channel=99 edge=99 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:30:36.389] [info] [graph][runtime.channel] create channel=100 edge=100 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.389] [info] [graph][runtime.channel] create channel=101 edge=101 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0
[2026-11-02 10:30:36.389] [info] [graph][runtime.channel] create channel=102 edge=102 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:30:36.389] [info] [graph][runtime.channel] create channel=103 edge=103 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:30:36.389] [info] [graph][runtime.channel] create channel=104 edge=104 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 10:30:36.389] [info] [graph][runtime.channel] create channel=105 edge=105 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0
[2026-11-02 10:30:36.389] [info] [graph][runtime.channel] create channel=106 edge=106 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.389] [info] [graph][runtime.channel] create channel=107 edge=107 stream=Video edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-02 10:30:36.389] [info] [graph][runtime.channel] create channel=108 edge=108 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-02 10:30:36.389] [info] [graph][runtime.channel] create channel=109 edge=109 stream=Any edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-02 10:30:36.390] [info] [graph][runtime.channel] create channel=110 edge=110 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.390] [info] [graph][runtime.channel] create channel=111 edge=111 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-02 10:30:36.390] [info] [graph][runtime.channel] create channel=112 edge=112 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 10:30:36.390] [info] [graph][runtime.channel] create channel=113 edge=113 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:30:36.390] [info] [graph][runtime.channel] create channel=114 edge=114 stream=Any edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-02 10:30:36.390] [info] [graph][runtime.channel] create channel=115 edge=115 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 10:30:36.390] [info] [graph][runtime.channel] create channel=116 edge=116 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-02 10:30:36.390] [info] [graph][runtime.channel] create channel=117 edge=117 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-02 10:30:36.390] [info] [graph][runtime.channel] create channel=118 edge=118 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-02 10:30:36.390] [info] [graph][runtime.channel] create channel=119 edge=119 stream=Any edge_kind=EncodedPacket payload=Unknown queued=0
[2026-11-02 10:30:36.391] [info] [graph][runtime.channel] create channel=120 edge=120 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-02 10:30:36.391] [info] [graph][runtime.channel] create channel=121 edge=121 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-02 10:30:36.391] [info] [graph][runtime.channel] create channel=122 edge=122 stream=Metadata edge_kind=Unknown payload=Unknown queued=0
[2026-11-02 10:30:36.391] [info] [graph][runtime.channel] create channel=123 edge=123 stream=Metadata edge_kind=Unknown payload=Unknown queued=0
[2026-11-02 10:30:36.391] [info] [graph][runtime.lifecycle] compiled nodes=81 edges=123 channels=123 execution_order=1->3->8->27->46->47->9->21->28->40->63->48->69->10->29->49->11->12->30->31->50->51->52->13->32->53->14->17->33->36->54->55->59->15->34->56->57->16->35->58->18->37->60->19->38->61->20->39->62->22->41->64->70->23->42->65->71->24->43->66->72->26->45->68->73->25->44->67->6->2->7->80->4->77->5->74->75->78->76->79->81
[2026-11-02 10:30:36.391] [info] [graph][runtime.lifecycle] compile.done state=DefaultRegistrationPending
[2026-11-02 10:30:36.392] [info] [graph][runtime.node] register node=1 name=composition-r376.output.video.codec_resolver kind=CodecResolver
[2026-11-02 10:30:36.392] [info] [graph][runtime.node] register node=2 name=composition-r376.output.video.encode kind=VideoEncode
[2026-11-02 10:30:36.392] [info] [graph][runtime.node] register node=3 name=composition-r376.output.audio.codec_resolver kind=AudioCodecResolver
[2026-11-02 10:30:36.392] [info] [graph][runtime.node] register node=4 name=composition-r376.output.audio.encode kind=AudioEncode
[2026-11-02 10:30:36.392] [info] [graph][runtime.node] register node=5 name=composition-r376.output.audio.canonicalizer kind=EncodedAudioCanonicalizer
[2026-11-02 10:30:36.392] [info] [graph][runtime.node] register node=8 name=composition-r376.source_0.input kind=RawRtpInput
[2026-11-02 10:30:36.392] [info] [graph][runtime.node] register node=9 name=composition-r376.source_0.rtp.clock_group kind=RtpClockGroup
[2026-11-02 10:30:36.393] [info] [graph][runtime.node] register node=10 name=composition-r376.source_0.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-11-02 10:30:36.393] [info] [graph][runtime.node] register node=11 name=composition-r376.source_0.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-11-02 10:30:36.393] [info] [graph][runtime.node] register node=12 name=composition-r376.source_0.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-11-02 10:30:36.393] [info] [graph][runtime.node] register node=13 name=composition-r376.source_0.av_sync.source_clock kind=SourceClockStateFanout
[2026-11-02 10:30:36.393] [info] [graph][runtime.node] register node=14 name=composition-r376.source_0.av_sync.video.generation_gate kind=LockedPacketGate
[2026-11-02 10:30:36.393] [info] [graph][runtime.node] register node=15 name=composition-r376.source_0.av_sync.video.canonical_input kind=CanonicalInput
[2026-11-02 10:30:36.393] [info] [graph][runtime.node] register node=16 name=composition-r376.source_0.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-11-02 10:30:36.393] [info] [graph][runtime.node] register node=17 name=composition-r376.source_0.av_sync.startup.clock kind=AvStartupClock
[2026-11-02 10:30:36.393] [info] [graph][runtime.node] register node=18 name=composition-r376.source_0.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-11-02 10:30:36.393] [info] [graph][runtime.node] register node=20 name=composition-r376.source_0.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-11-02 10:30:36.394] [info] [graph][runtime.node] register node=21 name=composition-r376.source_0.video.codec_resolver kind=CodecResolver
[2026-11-02 10:30:36.394] [info] [graph][runtime.node] register node=22 name=composition-r376.source_0.video.decode kind=VideoDecode
[2026-11-02 10:30:36.394] [info] [graph][runtime.node] register node=23 name=composition-r376.source_0.video.hwtransfer kind=HardwareTransfer
[2026-11-02 10:30:36.394] [info] [graph][runtime.node] register node=24 name=composition-r376.source_0.video.framerate kind=VideoFrameRate
[2026-11-02 10:30:36.394] [info] [graph][runtime.node] register node=25 name=composition-r376.source_0.video.preparation kind=VideoPreparation
[2026-11-02 10:30:36.394] [info] [graph][runtime.node] register node=26 name=composition-r376.source_0.video.filter kind=VideoFilter
[2026-11-02 10:30:36.394] [info] [graph][runtime.node] register node=27 name=composition-r376.source_1.input kind=RawRtpInput
[2026-11-02 10:30:36.395] [info] [graph][runtime.node] register node=28 name=composition-r376.source_1.rtp.clock_group kind=RtpClockGroup
[2026-11-02 10:30:36.395] [info] [graph][runtime.node] register node=29 name=composition-r376.source_1.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-11-02 10:30:36.395] [info] [graph][runtime.node] register node=30 name=composition-r376.source_1.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-11-02 10:30:36.395] [info] [graph][runtime.node] register node=31 name=composition-r376.source_1.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-11-02 10:30:36.395] [info] [graph][runtime.node] register node=32 name=composition-r376.source_1.av_sync.source_clock kind=SourceClockStateFanout
[2026-11-02 10:30:36.395] [info] [graph][runtime.node] register node=33 name=composition-r376.source_1.av_sync.video.generation_gate kind=LockedPacketGate
[2026-11-02 10:30:36.395] [info] [graph][runtime.node] register node=34 name=composition-r376.source_1.av_sync.video.canonical_input kind=CanonicalInput
[2026-11-02 10:30:36.396] [info] [graph][runtime.node] register node=35 name=composition-r376.source_1.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-11-02 10:30:36.396] [info] [graph][runtime.node] register node=36 name=composition-r376.source_1.av_sync.startup.clock kind=AvStartupClock
[2026-11-02 10:30:36.396] [info] [graph][runtime.node] register node=37 name=composition-r376.source_1.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-11-02 10:30:36.396] [info] [graph][runtime.node] register node=39 name=composition-r376.source_1.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-11-02 10:30:36.396] [info] [graph][runtime.node] register node=40 name=composition-r376.source_1.video.codec_resolver kind=CodecResolver
[2026-11-02 10:30:36.396] [info] [graph][runtime.node] register node=41 name=composition-r376.source_1.video.decode kind=VideoDecode
[2026-11-02 10:30:36.396] [info] [graph][runtime.node] register node=42 name=composition-r376.source_1.video.hwtransfer kind=HardwareTransfer
[2026-11-02 10:30:36.396] [info] [graph][runtime.node] register node=43 name=composition-r376.source_1.video.framerate kind=VideoFrameRate
[2026-11-02 10:30:36.396] [info] [graph][runtime.node] register node=44 name=composition-r376.source_1.video.preparation kind=VideoPreparation
[2026-11-02 10:30:36.396] [info] [graph][runtime.node] register node=45 name=composition-r376.source_1.video.filter kind=VideoFilter
[2026-11-02 10:30:36.396] [info] [graph][runtime.node] register node=46 name=composition-r376.source_2.video.input kind=RawRtpInput
[2026-11-02 10:30:36.397] [info] [graph][runtime.node] register node=47 name=composition-r376.source_2.audio.input kind=RawRtpInput
[2026-11-02 10:30:36.397] [info] [graph][runtime.node] register node=48 name=composition-r376.source_2.rtp.clock_group kind=RtpClockGroup
[2026-11-02 10:30:36.397] [info] [graph][runtime.node] register node=49 name=composition-r376.source_2.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-11-02 10:30:36.397] [info] [graph][runtime.node] register node=50 name=composition-r376.source_2.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-11-02 10:30:36.397] [info] [graph][runtime.node] register node=51 name=composition-r376.source_2.av_sync.audio.protocol_binder kind=RtpPacketClockBinder
[2026-11-02 10:30:36.397] [info] [graph][runtime.node] register node=52 name=composition-r376.source_2.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-11-02 10:30:36.397] [info] [graph][runtime.node] register node=53 name=composition-r376.source_2.av_sync.source_clock kind=SourceClockStateFanout
[2026-11-02 10:30:36.397] [info] [graph][runtime.node] register node=54 name=composition-r376.source_2.av_sync.video.generation_gate kind=LockedPacketGate
[2026-11-02 10:30:36.397] [info] [graph][runtime.node] register node=55 name=composition-r376.source_2.av_sync.audio.generation_gate kind=LockedPacketGate
[2026-11-02 10:30:36.397] [info] [graph][runtime.node] register node=56 name=composition-r376.source_2.av_sync.video.canonical_input kind=CanonicalInput
[2026-11-02 10:30:36.397] [info] [graph][runtime.node] register node=57 name=composition-r376.source_2.av_sync.audio.canonical_input kind=CanonicalInput
[2026-11-02 10:30:36.398] [info] [graph][runtime.node] register node=58 name=composition-r376.source_2.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-11-02 10:30:36.398] [info] [graph][runtime.node] register node=59 name=composition-r376.source_2.av_sync.startup.clock kind=AvStartupClock
[2026-11-02 10:30:36.398] [info] [graph][runtime.node] register node=60 name=composition-r376.source_2.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-11-02 10:30:36.398] [info] [graph][runtime.node] register node=62 name=composition-r376.source_2.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-11-02 10:30:36.398] [info] [graph][runtime.node] register node=63 name=composition-r376.source_2.video.codec_resolver kind=CodecResolver
[2026-11-02 10:30:36.398] [info] [graph][runtime.node] register node=64 name=composition-r376.source_2.video.decode kind=VideoDecode
[2026-11-02 10:30:36.398] [info] [graph][runtime.node] register node=65 name=composition-r376.source_2.video.hwtransfer kind=HardwareTransfer
[2026-11-02 10:30:36.398] [info] [graph][runtime.node] register node=66 name=composition-r376.source_2.video.framerate kind=VideoFrameRate
[2026-11-02 10:30:36.398] [info] [graph][runtime.node] register node=67 name=composition-r376.source_2.video.preparation kind=VideoPreparation
[2026-11-02 10:30:36.398] [info] [graph][runtime.node] register node=68 name=composition-r376.source_2.video.filter kind=VideoFilter
[2026-11-02 10:30:36.398] [info] [graph][runtime.node] register node=69 name=composition-r376.source_2.audio.encode.codec_resolver kind=AudioCodecResolver
[2026-11-02 10:30:36.399] [info] [graph][runtime.node] register node=70 name=composition-r376.source_2.audio.encode.decode kind=AudioDecode
[2026-11-02 10:30:36.399] [info] [graph][runtime.node] register node=71 name=composition-r376.source_2.audio.encode.startup_trim kind=Unknown
[2026-11-02 10:30:36.399] [info] [graph][runtime.node] register node=72 name=composition-r376.source_2.audio.encode.drift_controller kind=AudioDriftController
[2026-11-02 10:30:36.399] [info] [graph][runtime.node] register node=73 name=composition-r376.source_2.audio.encode.resample kind=AudioResample
[2026-11-02 10:30:36.399] [info] [graph][runtime.node] register node=74 name=composition-r376.output.scheduler.scheduler kind=AvOutputScheduler
[2026-11-02 10:30:36.399] [info] [graph][runtime.node] register node=75 name=composition-r376.output.scheduler.router kind=ScheduledOutputRouter
[2026-11-02 10:30:36.399] [info] [graph][runtime.node] register node=76 name=composition-r376.output.mpegts.mux kind=FileMux
[2026-11-02 10:30:36.399] [info] [graph][runtime.node] register node=77 name=composition-r376.output.mpegts.plan.source kind=Unknown
[2026-11-02 10:30:36.400] [info] [graph][runtime.node] register node=78 name=composition-r376.output.mpegts.scheduled.adapter kind=Unknown
[2026-11-02 10:30:36.400] [info] [graph][runtime.node] register node=79 name=composition-r376.output.mpegts.datagram.materializer kind=MpegTsDatagramMaterializer
[2026-11-02 10:30:36.400] [info] [graph][runtime.node] register node=80 name=composition-r376.output.mpegts.transport.plan kind=DatagramTransportPlanSource
[2026-11-02 10:30:36.400] [info] [graph][runtime.node] register node=81 name=composition-r376.output.mpegts.transport.sender kind=ScheduledDatagramSender
[2026-11-02 10:30:36.400] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=waiting_for_random_access detail="" description=""
[2026-11-02 10:30:36.401] [info] [graph][runtime.lifecycle] start.begin mode=threaded
[2026-11-02 10:30:36.402] [info] [graph][runtime.lifecycle] worker.finished node=1
[2026-11-02 10:30:36.402] [info] [graph][runtime.node] worker.thread_cpu node=1 available=1 cpu_ns=0 process_calls=1
[2026-11-02 10:30:36.403] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:30:36.404] [info] [graph][runtime.lifecycle] start.done state=ThreadedRunning
[2026-11-02 10:30:36.404] [info] [graph][runtime.lifecycle] worker.finished node=40
[2026-11-02 10:30:36.405] [info] [graph][runtime.node] worker.thread_cpu node=40 available=1 cpu_ns=0 process_calls=2
[2026-11-02 10:30:36.405] [info] [graph][runtime.lifecycle] worker.finished node=69
[2026-11-02 10:30:36.405] [info] [graph][runtime.node] worker.thread_cpu node=69 available=1 cpu_ns=0 process_calls=1
[2026-11-02 10:30:36.404] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:30:36.407] [info] [graph][runtime.lifecycle] worker.finished node=3
[2026-11-02 10:30:36.407] [info] [graph][runtime.node] worker.thread_cpu node=3 available=1 cpu_ns=0 process_calls=1
[2026-11-02 10:30:36.408] [info] [graph][runtime.lifecycle] worker.finished node=21
[2026-11-02 10:30:36.408] [info] [graph][runtime.node] worker.thread_cpu node=21 available=1 cpu_ns=0 process_calls=2
[2026-11-02 10:30:36.409] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:30:36.409] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-11-02 10:30:36.410] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=15 generation=1
[2026-11-02 10:30:36.414] [info] [graph][runtime.lifecycle] worker.finished node=63
[2026-11-02 10:30:36.415] [info] [graph][runtime.node] worker.thread_cpu node=63 available=1 cpu_ns=0 process_calls=1
[2026-11-02 10:30:36.415] [info] [graph][runtime.node] video_encode.bind_encoder codec=hevc_nvenc pix_fmt=cuda rate_control=cbr bitrate=8000000 min_bitrate=8000000 max_bitrate=8000000 buffer_size=267000 fps=30/1 gop=60 frame_kind=hardware hwaccel=cuda hw_device_ctx=set hw_frames_ctx=set
[2026-11-02 10:30:36.416] [info] [graph][runtime.node] audio_encode_trace stage=codec_bound
[2026-11-02 10:30:36.417] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-11-02 10:30:36.417] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=15 generation=1
[2026-11-02 10:30:36.417] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-11-02 10:30:36.418] [info] [graph][runtime.node] source_video_trace node_id=39 node=MediaAvBoundReleaseExtractorNode stage=release source_identity=composition-r376.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4007000000 duration_ns=33333333 decoded_sequence=unavailable pts=2132187654 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6112134641400 group=composition-r376.source.1 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-4007000000 epoch_master_release_ns=5531600700 video_count=15 min_presentation_ns=-4007000000 max_end_ns=-3507000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.418] [info] [graph][runtime.node] source_video_trace node_id=39 node=MediaAvBoundReleaseExtractorNode stage=published source_identity=composition-r376.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4007000000 duration_ns=33333333 decoded_sequence=unavailable pts=2132187654 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6112135157100 group=composition-r376.source.1 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.418] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=2132187654 dts=2132187654 size=317016 generation=1 sequence=15
[2026-11-02 10:30:36.418] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=input source_identity=composition-r376.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4007000000 duration_ns=33333333 decoded_sequence=unavailable pts=2132187654 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6112135506500 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.418] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=15 generation=1
[2026-11-02 10:30:36.419] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=release source_identity=composition-r376.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4012000000 duration_ns=33333333 decoded_sequence=unavailable pts=2259382538 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6112136247000 group=composition-r376.source.2 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=-4012000000 epoch_master_release_ns=5533046700 video_count=148 min_presentation_ns=-4012000000 max_end_ns=921333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.419] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=published source_identity=composition-r376.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4012000000 duration_ns=33333333 decoded_sequence=unavailable pts=2259382538 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6112136401200 group=composition-r376.source.2 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.419] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=2259382538 dts=2259382538 size=317016 generation=1 sequence=15
[2026-11-02 10:30:36.420] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=input source_identity=composition-r376.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4012000000 duration_ns=33333333 decoded_sequence=unavailable pts=2259382538 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6112136657000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.419] [info] [graph][runtime.node] source_video_trace node_id=20 node=MediaAvBoundReleaseExtractorNode stage=release source_identity=composition-r376.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4008000000 duration_ns=33333333 decoded_sequence=unavailable pts=575090638 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6112136443100 group=composition-r376.source.0 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-4008000000 epoch_master_release_ns=5533358700 video_count=15 min_presentation_ns=-4008000000 max_end_ns=-3508000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.421] [info] [graph][runtime.node] source_video_trace node_id=20 node=MediaAvBoundReleaseExtractorNode stage=published source_identity=composition-r376.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4008000000 duration_ns=33333333 decoded_sequence=unavailable pts=575090638 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6112138532700 group=composition-r376.source.0 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.422] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=575090638 dts=575090638 size=317016 generation=1 sequence=15
[2026-11-02 10:30:36.422] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=input source_identity=composition-r376.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4008000000 duration_ns=33333333 decoded_sequence=unavailable pts=575090638 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6112138788700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.440] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-11-02 10:30:36.441] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=accepted source_identity=composition-r376.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4007000000 duration_ns=33333333 decoded_sequence=unavailable pts=2132187654 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6112157668400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.441] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=2132187654 format=cuda size=1280x720 buffer_identity=0xb08200000 drm_prime=0 software_frame=0
[2026-11-02 10:30:36.441] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=resolved source_identity=composition-r376.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4007000000 duration_ns=33333333 decoded_sequence=unavailable pts=2132187654 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6112158264800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.442] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=published source_identity=composition-r376.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4007000000 duration_ns=33333333 decoded_sequence=unavailable pts=2132187654 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6112158582900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.442] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=000001A0FFCB3C70 stream=Video payload=Frame pts=2132187654 dts=2132187654 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:30:36.442] [info] [graph][runtime.node] source_video_trace node_id=42 node=HardwareTransferNode stage=published source_identity=composition-r376.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4007000000 duration_ns=33333333 decoded_sequence=unavailable pts=2132187654 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6112158995700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.443] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=000001A0FFCB3C70 stream=Video payload=Frame pts=2132187654 dts=2132187654 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:30:36.442] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=000001A0FFCB3C70 stream=Video payload=Frame pts=2132187654 dts=2132187654 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:30:36.444] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-11-02 10:30:36.444] [info] [graph][runtime.node] source_video_trace node_id=43 node=VideoFrameRateNode stage=published source_identity=composition-r376.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4007000000 duration_ns=33333333 decoded_sequence=unavailable pts=2132187654 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6112161346100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.444] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=000001A0FFCB4450 stream=Video payload=Frame pts=2132187654 dts=2132187654 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:30:36.445] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=000001A0FFCB4450 stream=Video payload=Frame pts=2132187654 dts=2132187654 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:30:36.445] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-11-02 10:30:36.450] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-11-02 10:30:36.450] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=accepted source_identity=composition-r376.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4008000000 duration_ns=33333333 decoded_sequence=unavailable pts=575090638 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6112167195800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.450] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=575090638 format=cuda size=1280x720 buffer_identity=0xb08e00000 drm_prime=0 software_frame=0
[2026-11-02 10:30:36.450] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=resolved source_identity=composition-r376.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4008000000 duration_ns=33333333 decoded_sequence=unavailable pts=575090638 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6112167425300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.450] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=published source_identity=composition-r376.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4008000000 duration_ns=33333333 decoded_sequence=unavailable pts=575090638 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6112167536900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.451] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=000001A08A6B1A30 stream=Video payload=Frame pts=575090638 dts=575090638 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:30:36.451] [info] [graph][runtime.node] source_video_trace node_id=23 node=HardwareTransferNode stage=published source_identity=composition-r376.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4008000000 duration_ns=33333333 decoded_sequence=unavailable pts=575090638 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6112168113800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.451] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=000001A08A6B1A30 stream=Video payload=Frame pts=575090638 dts=575090638 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:30:36.451] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=000001A08A6B1A30 stream=Video payload=Frame pts=575090638 dts=575090638 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:30:36.451] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-11-02 10:30:36.452] [info] [graph][runtime.node] source_video_trace node_id=24 node=VideoFrameRateNode stage=published source_identity=composition-r376.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4008000000 duration_ns=33333333 decoded_sequence=unavailable pts=575090638 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6112168806400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.452] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=000001A08A6B1640 stream=Video payload=Frame pts=575090638 dts=575090638 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:30:36.452] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=000001A08A6B1640 stream=Video payload=Frame pts=575090638 dts=575090638 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:30:36.452] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-11-02 10:30:36.452] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-11-02 10:30:36.453] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=accepted source_identity=composition-r376.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4012000000 duration_ns=33333333 decoded_sequence=unavailable pts=2259382538 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6112169594100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.453] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=2259382538 format=cuda size=1280x720 buffer_identity=0xb0ae00000 drm_prime=0 software_frame=0
[2026-11-02 10:30:36.453] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=resolved source_identity=composition-r376.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4012000000 duration_ns=33333333 decoded_sequence=unavailable pts=2259382538 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6112170156800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.453] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=published source_identity=composition-r376.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4012000000 duration_ns=33333333 decoded_sequence=unavailable pts=2259382538 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6112170305300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.453] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=000001A08A6B1E20 stream=Video payload=Frame pts=2259382538 dts=2259382538 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:30:36.454] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=000001A08A6B1E20 stream=Video payload=Frame pts=2259382538 dts=2259382538 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:30:36.454] [info] [graph][runtime.node] source_video_trace node_id=65 node=HardwareTransferNode stage=published source_identity=composition-r376.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4012000000 duration_ns=33333333 decoded_sequence=unavailable pts=2259382538 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6112170705700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.454] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-11-02 10:30:36.454] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=000001A08A6B1E20 stream=Video payload=Frame pts=2259382538 dts=2259382538 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:30:36.454] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=000001A08A6B2210 stream=Video payload=Frame pts=2259382538 dts=2259382538 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:30:36.455] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-11-02 10:30:36.454] [info] [graph][runtime.node] source_video_trace node_id=66 node=VideoFrameRateNode stage=published source_identity=composition-r376.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4012000000 duration_ns=33333333 decoded_sequence=unavailable pts=2259382538 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6112171286500 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.455] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=000001A08A6B2210 stream=Video payload=Frame pts=2259382538 dts=2259382538 duration=3000 tb=1/90000 flags=68
[2026-11-02 10:30:36.511] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=58
[2026-11-02 10:30:36.537] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=92
[2026-11-02 10:30:36.537] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=input source_identity=composition-r376.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4008000000 duration_ns=33333333 decoded_sequence=unavailable pts=575090638 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6112254236900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.537] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=82
[2026-11-02 10:30:36.538] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=input source_identity=composition-r376.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4007000000 duration_ns=33333333 decoded_sequence=unavailable pts=2132187654 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6112255262400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.538] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=input source_identity=composition-r376.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4012000000 duration_ns=33333333 decoded_sequence=unavailable pts=2259382538 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6112255277400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.539] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=accepted source_identity=composition-r376.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4008000000 duration_ns=33333333 decoded_sequence=unavailable pts=575090638 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6112255638000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.540] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=resolved source_identity=composition-r376.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4008000000 duration_ns=33333333 decoded_sequence=unavailable pts=191697 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=6112257158900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.541] [info] [graph][runtime.node] source_video_trace node_id=25 node=VideoPreparationNode stage=input source_identity=composition-r376.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4008000000 duration_ns=33333333 decoded_sequence=unavailable pts=191697 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=6112257728700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.541] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=accepted source_identity=composition-r376.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4012000000 duration_ns=33333333 decoded_sequence=unavailable pts=2259382538 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6112257819900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.541] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=published source_identity=composition-r376.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4008000000 duration_ns=33333333 decoded_sequence=unavailable pts=191697 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=6112257704900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.542] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb08a90000 drm_prime=0 software_frame=0
[2026-11-02 10:30:36.542] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=accepted source_identity=composition-r376.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4007000000 duration_ns=33333333 decoded_sequence=unavailable pts=2132187654 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6112258579600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.541] [info] [graph][runtime.node] source_video_trace node_id=25 node=VideoPreparationNode stage=output_attempt source_identity=composition-r376.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4008000000 duration_ns=33333333 decoded_sequence=unavailable pts=191697 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=6112258028600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.542] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=15 audio_count=0 first_video_sequence=15
[2026-11-02 10:30:36.542] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=resolved source_identity=composition-r376.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4012000000 duration_ns=33333333 decoded_sequence=unavailable pts=753128 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=6112258766400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.543] [info] [graph][runtime.node] source_video_trace node_id=19 node=ActivationReleaseSequencer stage=activation source_identity=composition-r376.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4008000000 duration_ns=33333333 decoded_sequence=unavailable pts=575090638 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6112259903400 group=composition-r376.source.0 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-4008000000 epoch_master_release_ns=5723558768 video_count=15 min_presentation_ns=-4008000000 max_end_ns=-3508000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.543] [info] [graph][runtime.node] source_video_trace node_id=25 node=VideoPreparationNode stage=published source_identity=unavailable identity_link=node_generation_input_source_sequence generation=1 source_sequence=15 presentation_ns=-4008000000 duration_ns=33333333 decoded_sequence=unavailable pts=191697 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=6112259901400 group=composition-r376.source.0 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.543] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=resolved source_identity=composition-r376.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4007000000 duration_ns=33333333 decoded_sequence=unavailable pts=710729 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=6112259681100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.543] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=published source_identity=composition-r376.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4012000000 duration_ns=33333333 decoded_sequence=unavailable pts=753128 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=6112260027800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.543] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=input source_identity=composition-r376.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4012000000 duration_ns=33333333 decoded_sequence=unavailable pts=753128 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=6112260050000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.543] [info] [graph][runtime.node] source_video_trace node_id=19 node=ActivationReleaseSequencer stage=release_published source_identity=composition-r376.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4008000000 duration_ns=33333333 decoded_sequence=unavailable pts=575090638 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6112260143500 group=composition-r376.source.0 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-4008000000 epoch_master_release_ns=5723558768 video_count=15 min_presentation_ns=-4008000000 max_end_ns=-3508000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.543] [info] [graph][runtime.node] source_video_trace node_id=20 node=MediaAvBoundReleaseExtractorNode stage=release_committed source_identity=composition-r376.source.0.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4008000000 duration_ns=33333333 decoded_sequence=unavailable pts=575090638 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6112260347700 group=composition-r376.source.0 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-4008000000 epoch_master_release_ns=5533358700 video_count=15 min_presentation_ns=-4008000000 max_end_ns=-3508000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=17 release_committed_count=1 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.543] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_candidate source_identity=composition-r376.source.0.input.video generation=1 steady_ns=6112260360400 source_sequence=15 presentation_ns=-4008000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 10:30:36.543] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=published source_identity=composition-r376.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4007000000 duration_ns=33333333 decoded_sequence=unavailable pts=710729 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=6112260501300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.543] [info] [graph][runtime.node] source_video_trace node_id=44 node=VideoPreparationNode stage=input source_identity=composition-r376.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4007000000 duration_ns=33333333 decoded_sequence=unavailable pts=710729 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=6112260514200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.544] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb08b20000 drm_prime=0 software_frame=0
[2026-11-02 10:30:36.544] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=output_attempt source_identity=composition-r376.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4012000000 duration_ns=33333333 decoded_sequence=unavailable pts=753128 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=6112260841800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.544] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=0
[2026-11-02 10:30:36.545] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb08a00000 drm_prime=0 software_frame=0
[2026-11-02 10:30:36.545] [info] [graph][runtime.node] source_video_trace node_id=44 node=VideoPreparationNode stage=output_attempt source_identity=composition-r376.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4007000000 duration_ns=33333333 decoded_sequence=unavailable pts=710729 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=6112261987900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.545] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=148 audio_count=23 first_video_sequence=15
[2026-11-02 10:30:36.546] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=15 audio_count=0 first_video_sequence=15
[2026-11-02 10:30:36.546] [info] [graph][runtime.node] source_video_trace node_id=61 node=ActivationReleaseSequencer stage=activation source_identity=composition-r376.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4012000000 duration_ns=33333333 decoded_sequence=unavailable pts=2259382538 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6112263194000 group=composition-r376.source.2 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=-4012000000 epoch_master_release_ns=5726765068 video_count=148 min_presentation_ns=-4012000000 max_end_ns=921333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.546] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=published source_identity=unavailable identity_link=node_generation_input_source_sequence generation=1 source_sequence=15 presentation_ns=-4012000000 duration_ns=33333333 decoded_sequence=unavailable pts=753128 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=6112263140200 group=composition-r376.source.2 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.546] [info] [graph][runtime.node] source_video_trace node_id=44 node=VideoPreparationNode stage=published source_identity=unavailable identity_link=node_generation_input_source_sequence generation=1 source_sequence=15 presentation_ns=-4007000000 duration_ns=33333333 decoded_sequence=unavailable pts=710729 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=6112263294600 group=composition-r376.source.1 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.546] [info] [graph][runtime.node] source_video_trace node_id=38 node=ActivationReleaseSequencer stage=activation source_identity=composition-r376.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4007000000 duration_ns=33333333 decoded_sequence=unavailable pts=2132187654 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6112263291400 group=composition-r376.source.1 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-4007000000 epoch_master_release_ns=5726983768 video_count=15 min_presentation_ns=-4007000000 max_end_ns=-3507000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.546] [info] [graph][runtime.node] source_video_trace node_id=61 node=ActivationReleaseSequencer stage=release_published source_identity=composition-r376.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4012000000 duration_ns=33333333 decoded_sequence=unavailable pts=2259382538 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6112263442900 group=composition-r376.source.2 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=-4012000000 epoch_master_release_ns=5726765068 video_count=148 min_presentation_ns=-4012000000 max_end_ns=921333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.546] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_candidate source_identity=composition-r376.source.2.input.video generation=1 steady_ns=6112263566200 source_sequence=15 presentation_ns=-4012000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 10:30:36.547] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_candidate source_identity=composition-r376.source.1.input.video generation=1 steady_ns=6112264054300 source_sequence=15 presentation_ns=-4007000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 10:30:36.547] [info] [graph][runtime.node] audio_decode_trace stage=first_packet
[2026-11-02 10:30:36.547] [info] [graph][runtime.node] source_video_trace node_id=38 node=ActivationReleaseSequencer stage=release_published source_identity=composition-r376.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4007000000 duration_ns=33333333 decoded_sequence=unavailable pts=2132187654 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6112263864200 group=composition-r376.source.1 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-4007000000 epoch_master_release_ns=5726983768 video_count=15 min_presentation_ns=-4007000000 max_end_ns=-3507000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.547] [info] [graph][runtime.node] source_video_trace node_id=39 node=MediaAvBoundReleaseExtractorNode stage=release_committed source_identity=composition-r376.source.1.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4007000000 duration_ns=33333333 decoded_sequence=unavailable pts=2132187654 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6112263889900 group=composition-r376.source.1 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-4007000000 epoch_master_release_ns=5531600700 video_count=15 min_presentation_ns=-4007000000 max_end_ns=-3507000000 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=17 release_committed_count=1 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.547] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=0
[2026-11-02 10:30:36.547] [info] [graph][runtime.node] audio_decode_trace stage=first_frame samples=1024 rate=44100
[2026-11-02 10:30:36.547] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=release_committed source_identity=composition-r376.source.2.input.video identity_link=direct generation=1 source_sequence=15 presentation_ns=-4012000000 duration_ns=33333333 decoded_sequence=unavailable pts=2259382538 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6112263600500 group=composition-r376.source.2 release_identity=4 transition_sequence=unavailable epoch_source_start_ns=-4012000000 epoch_master_release_ns=5533046700 video_count=148 min_presentation_ns=-4012000000 max_end_ns=921333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=148 release_count=150 release_committed_count=1 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:30:36.548] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=23
[2026-11-02 10:30:36.548] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1 source_master_ns=5729218583 raw_phase_ns=4536 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-11-02 10:30:36.548] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3 source_master_ns=5775658493 raw_phase_ns=4537 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=45346
[2026-11-02 10:30:36.549] [info] [graph][runtime.node] audio_encode_trace stage=first_frame
[2026-11-02 10:30:36.549] [info] [graph][runtime.node] output_preparation stage=transport_activate generation=1 elapsed_ns=358000 planned_bytes=180767114966 planned_items=1 event=scope_exit
[2026-11-02 10:30:36.549] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-r376.source.0.input.video generation=1 steady_ns=6112266365700 source_sequence=15 presentation_ns=-4008000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 10:30:36.549] [info] [graph][runtime.node] video_encode.input_contract format=cuda size=1280x720 buffer_identity=0xb0a000000 drm_prime=0 software_frame=0
[2026-11-02 10:30:36.550] [info] [graph][runtime.node] output_preparation stage=global_sequence generation=1 elapsed_ns=47700 planned_bytes=68968 planned_items=2154 event=scope_exit
[2026-11-02 10:30:36.550] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-r376.source.1.input.video generation=1 steady_ns=6112266896100 source_sequence=15 presentation_ns=-4007000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 10:30:36.550] [info] [graph][runtime.node] video_encode.generation_start_key_frame pts=0 generation=1
[2026-11-02 10:30:36.551] [info] [graph][runtime.node] output_preparation stage=transport_buffer generation=1 elapsed_ns=1742800 planned_bytes=180767114966 planned_items=1 event=scope_exit
[2026-11-02 10:30:36.551] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-r376.source.2.input.video generation=1 steady_ns=6112267893300 source_sequence=15 presentation_ns=-4012000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 10:30:36.551] [info] [graph][runtime.node] video_encode.first_frame pts=0 generation=attached
[2026-11-02 10:30:36.552] [info] [graph][runtime.node] output_preparation stage=ts_packetizer generation=1 elapsed_ns=106100 planned_bytes=180765122912 planned_items=61257 event=scope_exit
[2026-11-02 10:30:36.552] [info] [graph][runtime.node] output_preparation stage=ts_mux generation=1 elapsed_ns=545200 planned_bytes=945416 planned_items=59086 event=scope_exit
[2026-11-02 10:30:36.553] [info] [graph][runtime.node] mpegts_emission_plan generation=1 mode=canonical_access_unit_window access_unit_window_ns=100000000 video_initial_service_window_ns=13990650 audio_initial_service_window_ns=13990650 packet_size_bytes=188 maximum_payload_bytes=1316 per_datagram_overhead_bytes=0
[2026-11-02 10:30:36.557] [info] [graph][runtime.node] video_encode.first_submit result=accepted
[2026-11-02 10:30:36.557] [info] [graph][runtime.node] video_encode.first_packet pts=0 dts=0 duration=0 size=108357 side_data_elems=1 received_footprint=108365 side_data[0]=Quality stats:8
[2026-11-02 10:30:36.558] [info] [graph][runtime.node] video_encode.first_packet_footprint bytes=108365 wrapped_size=108357 wrapped_side_data_elems=1
[2026-11-02 10:30:36.558] [info] [graph][runtime.node] av_scheduler_trace stage=first_video_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=5729312668 dispatch_master_ns=5729312668 now_ns=5572142800 node=74 identity_kind=output identity="composition-r376.output" canonical_sequence=1 canonical_created_steady_ns=6112275040700 head_observed_steady_ns=6112275066500 codec_receive_begin_steady_ns=6112274495100 codec_receive_end_steady_ns=6112274495800
[2026-11-02 10:30:36.558] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=5572253800 present_generation=1 present_canonical_target_ns=0 present_canonical_dispatch_ns=0 present_target_master_ns=5729312668 present_dispatch_master_ns=5729312668
[2026-11-02 10:30:36.597] [info] [graph][runtime.node] audio_encode_trace stage=first_packet
[2026-11-02 10:30:36.597] [info] [graph][runtime.node] av_scheduler_trace stage=first_audio_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=5729312668 dispatch_master_ns=5729312668 now_ns=5611517700 node=74 identity_kind=output identity="composition-r376.output" canonical_sequence=1 canonical_created_steady_ns=6112314417800 head_observed_steady_ns=6112314441300 codec_receive_begin_steady_ns=unavailable codec_receive_end_steady_ns=unavailable
[2026-11-02 10:30:36.754] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=running detail="" description=""
[2026-11-02 10:30:37.153] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=48 source_master_ns=6820556452 raw_phase_ns=4536 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=90692
[2026-11-02 10:30:38.174] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=92 source_master_ns=7842234456 raw_phase_ns=4536 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=136038
[2026-11-02 10:30:39.196] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=136 source_master_ns=8863912461 raw_phase_ns=4536 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=181384
[2026-11-02 10:30:40.241] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=181 source_master_ns=9908810419 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=226730
[2026-11-02 10:30:40.977] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2108050542 ntp_seconds=4002575440 ntp_fraction=4191888080 arrival_ns=6116693655400
[2026-11-02 10:30:40.978] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2108050542
[2026-11-02 10:30:40.978] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:30:40.977] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=465521080 ntp_seconds=4002575440 ntp_fraction=4196183048 arrival_ns=6116694183500
[2026-11-02 10:30:40.979] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=465521080
[2026-11-02 10:30:40.979] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:30:40.978] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=938262771 ntp_seconds=4002575440 ntp_fraction=4196183048 arrival_ns=6116694061000
[2026-11-02 10:30:40.979] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=938262771
[2026-11-02 10:30:40.979] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:30:40.979] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3374178149 ntp_seconds=4002575440 ntp_fraction=4196183048 arrival_ns=6116694000300
[2026-11-02 10:30:40.980] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3374178149
[2026-11-02 10:30:40.980] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:30:41.269] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=225 source_master_ns=10930488424 raw_phase_ns=4536 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=272076
[2026-11-02 10:30:42.284] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=269 source_master_ns=11952166428 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=317422
[2026-11-02 10:30:43.306] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=313 source_master_ns=12973844433 raw_phase_ns=4536 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=362768
[2026-11-02 10:30:44.352] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=358 source_master_ns=14018742392 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=408114
[2026-11-02 10:30:45.372] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=402 source_master_ns=15040420396 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=453460
[2026-11-02 10:30:45.984] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3374178149 ntp_seconds=4002575445 ntp_fraction=4226247819 arrival_ns=6121701046900
[2026-11-02 10:30:45.984] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3374178149
[2026-11-02 10:30:45.984] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:30:46.013] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2108050542 ntp_seconds=4002575446 ntp_fraction=55834574 arrival_ns=6121729745100
[2026-11-02 10:30:46.014] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2108050542
[2026-11-02 10:30:46.014] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:30:46.014] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=465521080 ntp_seconds=4002575446 ntp_fraction=55834574 arrival_ns=6121730353800
[2026-11-02 10:30:46.014] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=465521080
[2026-11-02 10:30:46.015] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:30:46.015] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=938262771 ntp_seconds=4002575446 ntp_fraction=64424509 arrival_ns=6121732003400
[2026-11-02 10:30:46.016] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=938262771
[2026-11-02 10:30:46.016] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:30:46.398] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=446 source_master_ns=16062098401 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=498806
[2026-11-02 10:30:47.439] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=491 source_master_ns=17106996360 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=544152
[2026-11-02 10:30:48.461] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=535 source_master_ns=18128674365 raw_phase_ns=4536 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=589498
[2026-11-02 10:30:49.482] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=579 source_master_ns=19150352369 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=634844
[2026-11-02 10:30:50.504] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=623 source_master_ns=20172030374 raw_phase_ns=4536 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=680190
[2026-11-02 10:30:51.008] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3374178149 ntp_seconds=4002575451 ntp_fraction=34359738 arrival_ns=6126724854700
[2026-11-02 10:30:51.009] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3374178149
[2026-11-02 10:30:51.009] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:30:51.037] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=465521080 ntp_seconds=4002575451 ntp_fraction=158913789 arrival_ns=6126753788000
[2026-11-02 10:30:51.037] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=465521080
[2026-11-02 10:30:51.037] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:30:51.049] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2108050542 ntp_seconds=4002575451 ntp_fraction=210453397 arrival_ns=6126766071100
[2026-11-02 10:30:51.049] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2108050542
[2026-11-02 10:30:51.049] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=938262771 ntp_seconds=4002575451 ntp_fraction=210453397 arrival_ns=6126766169700
[2026-11-02 10:30:51.050] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=938262771
[2026-11-02 10:30:51.050] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:30:51.050] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:30:51.550] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=668 source_master_ns=21216928333 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=725536
[2026-11-02 10:30:52.572] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=712 source_master_ns=22238606337 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=770882
[2026-11-02 10:30:53.592] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=756 source_master_ns=23260284342 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=816228
[2026-11-02 10:30:54.614] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=800 source_master_ns=24281962346 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=861574
[2026-11-02 10:30:55.659] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=845 source_master_ns=25326860306 raw_phase_ns=4536 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=906920
[2026-11-02 10:30:56.009] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3374178149 ntp_seconds=4002575456 ntp_fraction=38654705 arrival_ns=6131726414400
[2026-11-02 10:30:56.010] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3374178149
[2026-11-02 10:30:56.010] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:30:56.041] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=465521080 ntp_seconds=4002575456 ntp_fraction=176093659 arrival_ns=6131758274700
[2026-11-02 10:30:56.042] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=465521080
[2026-11-02 10:30:56.042] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:30:56.071] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2108050542 ntp_seconds=4002575456 ntp_fraction=304942678 arrival_ns=6131788363500
[2026-11-02 10:30:56.072] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=938262771 ntp_seconds=4002575456 ntp_fraction=309237645 arrival_ns=6131788725700
[2026-11-02 10:30:56.072] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2108050542
[2026-11-02 10:30:56.072] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=938262771
[2026-11-02 10:30:56.072] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:30:56.072] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:30:56.681] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=889 source_master_ns=26348538310 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=952266
[2026-11-02 10:30:57.702] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=933 source_master_ns=27370216315 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=997612
[2026-11-02 10:30:58.748] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=978 source_master_ns=28415114274 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1042958
[2026-11-02 10:30:59.769] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1022 source_master_ns=29436792278 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1088304
[2026-11-02 10:31:00.791] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1066 source_master_ns=30458470283 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1133650
[2026-11-02 10:31:01.015] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3374178149 ntp_seconds=4002575461 ntp_fraction=64424509 arrival_ns=6136732345400
[2026-11-02 10:31:01.016] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3374178149
[2026-11-02 10:31:01.016] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:01.043] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=465521080 ntp_seconds=4002575461 ntp_fraction=184683593 arrival_ns=6136760369700
[2026-11-02 10:31:01.044] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=465521080
[2026-11-02 10:31:01.044] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:01.075] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2108050542 ntp_seconds=4002575461 ntp_fraction=322122547 arrival_ns=6136792043900
[2026-11-02 10:31:01.075] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2108050542
[2026-11-02 10:31:01.076] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:01.075] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=938262771 ntp_seconds=4002575461 ntp_fraction=322122547 arrival_ns=6136792401700
[2026-11-02 10:31:01.076] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=938262771
[2026-11-02 10:31:01.076] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:01.812] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1110 source_master_ns=31480148287 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1178996
[2026-11-02 10:31:02.857] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1155 source_master_ns=32525046247 raw_phase_ns=4536 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1224342
[2026-11-02 10:31:03.879] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1199 source_master_ns=33546724251 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1269688
[2026-11-02 10:31:04.901] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1243 source_master_ns=34568402256 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1315034
[2026-11-02 10:31:05.945] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1288 source_master_ns=35613300215 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1360380
[2026-11-02 10:31:06.018] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3374178149 ntp_seconds=4002575466 ntp_fraction=77309411 arrival_ns=6141734756100
[2026-11-02 10:31:06.018] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3374178149
[2026-11-02 10:31:06.018] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:06.048] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=465521080 ntp_seconds=4002575466 ntp_fraction=206158430 arrival_ns=6141764781500
[2026-11-02 10:31:06.048] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=465521080
[2026-11-02 10:31:06.049] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:06.077] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2108050542 ntp_seconds=4002575466 ntp_fraction=330712481 arrival_ns=6141793824400
[2026-11-02 10:31:06.077] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2108050542
[2026-11-02 10:31:06.077] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:06.077] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=938262771 ntp_seconds=4002575466 ntp_fraction=330712481 arrival_ns=6141794344000
[2026-11-02 10:31:06.078] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=938262771
[2026-11-02 10:31:06.078] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:06.968] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1332 source_master_ns=36634978219 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1405726
[2026-11-02 10:31:07.989] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1376 source_master_ns=37656656224 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1451072
[2026-11-02 10:31:09.011] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1420 source_master_ns=38678334228 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1496418
[2026-11-02 10:31:10.061] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1465 source_master_ns=39723232188 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1541764
[2026-11-02 10:31:11.043] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3374178149 ntp_seconds=4002575471 ntp_fraction=184683593 arrival_ns=6146760410200
[2026-11-02 10:31:11.044] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3374178149
[2026-11-02 10:31:11.044] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:11.073] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=465521080 ntp_seconds=4002575471 ntp_fraction=313532612 arrival_ns=6146789655000
[2026-11-02 10:31:11.073] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=465521080
[2026-11-02 10:31:11.073] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:11.077] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1509 source_master_ns=40744910192 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1587110
[2026-11-02 10:31:11.103] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=938262771 ntp_seconds=4002575471 ntp_fraction=442381631 arrival_ns=6146819894500
[2026-11-02 10:31:11.103] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=938262771
[2026-11-02 10:31:11.103] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:11.103] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2108050542 ntp_seconds=4002575471 ntp_fraction=442381631 arrival_ns=6146819847500
[2026-11-02 10:31:11.104] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2108050542
[2026-11-02 10:31:11.104] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:12.099] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1553 source_master_ns=41766588197 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1632456
[2026-11-02 10:31:13.143] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1598 source_master_ns=42811486156 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1677802
[2026-11-02 10:31:14.165] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1642 source_master_ns=43833164160 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1723148
[2026-11-02 10:31:15.187] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1686 source_master_ns=44854842165 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1768494
[2026-11-02 10:31:16.059] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3374178149 ntp_seconds=4002575476 ntp_fraction=253403070 arrival_ns=6151775777100
[2026-11-02 10:31:16.059] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3374178149
[2026-11-02 10:31:16.059] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:16.076] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=465521080 ntp_seconds=4002575476 ntp_fraction=326417514 arrival_ns=6151792954000
[2026-11-02 10:31:16.077] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=465521080
[2026-11-02 10:31:16.077] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:16.105] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2108050542 ntp_seconds=4002575476 ntp_fraction=450971566 arrival_ns=6151821775400
[2026-11-02 10:31:16.105] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2108050542
[2026-11-02 10:31:16.106] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:16.106] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=938262771 ntp_seconds=4002575476 ntp_fraction=450971566 arrival_ns=6151822316500
[2026-11-02 10:31:16.106] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=938262771
[2026-11-02 10:31:16.107] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:16.208] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1730 source_master_ns=45876520170 raw_phase_ns=4536 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1813840
[2026-11-02 10:31:17.254] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1775 source_master_ns=46921418129 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1859186
[2026-11-02 10:31:18.275] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1819 source_master_ns=47943096133 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1904532
[2026-11-02 10:31:19.297] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1863 source_master_ns=48964774138 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1949878
[2026-11-02 10:31:20.343] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1908 source_master_ns=50009672097 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1995224
[2026-11-02 10:31:21.070] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3374178149 ntp_seconds=4002575481 ntp_fraction=300647710 arrival_ns=6156786880100
[2026-11-02 10:31:21.070] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3374178149
[2026-11-02 10:31:21.071] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:21.099] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=465521080 ntp_seconds=4002575481 ntp_fraction=425201762 arrival_ns=6156816257700
[2026-11-02 10:31:21.099] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=465521080
[2026-11-02 10:31:21.100] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:21.114] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=938262771 ntp_seconds=4002575481 ntp_fraction=489626271 arrival_ns=6156830695100
[2026-11-02 10:31:21.114] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=938262771
[2026-11-02 10:31:21.114] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:21.129] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2108050542 ntp_seconds=4002575481 ntp_fraction=549755813 arrival_ns=6156845457300
[2026-11-02 10:31:21.129] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2108050542
[2026-11-02 10:31:21.129] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:21.363] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1952 source_master_ns=51031350102 raw_phase_ns=4536 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2040570
[2026-11-02 10:31:22.389] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1996 source_master_ns=52053028106 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2085916
[2026-11-02 10:31:23.408] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2040 source_master_ns=53074706111 raw_phase_ns=4536 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2131262
[2026-11-02 10:31:24.452] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2085 source_master_ns=54119604070 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2176608
[2026-11-02 10:31:25.477] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2129 source_master_ns=55141282074 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2221954
[2026-11-02 10:31:26.081] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3374178149 ntp_seconds=4002575486 ntp_fraction=347892350 arrival_ns=6161798263000
[2026-11-02 10:31:26.082] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3374178149
[2026-11-02 10:31:26.082] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:26.111] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=465521080 ntp_seconds=4002575486 ntp_fraction=476741369 arrival_ns=6161828222600
[2026-11-02 10:31:26.112] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=465521080
[2026-11-02 10:31:26.112] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:26.140] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=938262771 ntp_seconds=4002575486 ntp_fraction=601295421 arrival_ns=6161857135700
[2026-11-02 10:31:26.140] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=938262771
[2026-11-02 10:31:26.141] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:26.140] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2108050542 ntp_seconds=4002575486 ntp_fraction=601295421 arrival_ns=6161857152700
[2026-11-02 10:31:26.141] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2108050542
[2026-11-02 10:31:26.141] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:26.495] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2173 source_master_ns=56162960079 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2267300
[2026-11-02 10:31:27.540] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2218 source_master_ns=57207858038 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2312646
[2026-11-02 10:31:28.562] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2262 source_master_ns=58229536043 raw_phase_ns=4536 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2357992
[2026-11-02 10:31:29.583] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2306 source_master_ns=59251214047 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2403338
[2026-11-02 10:31:30.605] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2350 source_master_ns=60272892052 raw_phase_ns=4536 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2448684
[2026-11-02 10:31:31.103] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3374178149 ntp_seconds=4002575491 ntp_fraction=438086664 arrival_ns=6166819739100
[2026-11-02 10:31:31.103] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3374178149
[2026-11-02 10:31:31.104] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:31.146] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=938262771 ntp_seconds=4002575491 ntp_fraction=627065225 arrival_ns=6166863187200
[2026-11-02 10:31:31.147] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=938262771
[2026-11-02 10:31:31.147] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:31.161] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=465521080 ntp_seconds=4002575491 ntp_fraction=687194767 arrival_ns=6166877638800
[2026-11-02 10:31:31.161] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=465521080
[2026-11-02 10:31:31.161] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:31.162] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2108050542 ntp_seconds=4002575491 ntp_fraction=695784701 arrival_ns=6166878899400
[2026-11-02 10:31:31.162] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2108050542
[2026-11-02 10:31:31.162] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:31.651] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2395 source_master_ns=61317790011 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2494030
[2026-11-02 10:31:32.671] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2439 source_master_ns=62339468015 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2539376
[2026-11-02 10:31:33.693] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2483 source_master_ns=63361146020 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2584722
[2026-11-02 10:31:34.738] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2528 source_master_ns=64406043979 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2630068
[2026-11-02 10:31:35.760] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2572 source_master_ns=65427721984 raw_phase_ns=4536 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2675414
[2026-11-02 10:31:36.113] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3374178149 ntp_seconds=4002575496 ntp_fraction=481036337 arrival_ns=6171829059000
[2026-11-02 10:31:36.113] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3374178149
[2026-11-02 10:31:36.113] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:36.171] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2108050542 ntp_seconds=4002575496 ntp_fraction=734439407 arrival_ns=6171888460400
[2026-11-02 10:31:36.172] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=465521080 ntp_seconds=4002575496 ntp_fraction=734439407 arrival_ns=6171888650000
[2026-11-02 10:31:36.172] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=938262771 ntp_seconds=4002575496 ntp_fraction=734439407 arrival_ns=6171888655200
[2026-11-02 10:31:36.172] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2108050542
[2026-11-02 10:31:36.172] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=465521080
[2026-11-02 10:31:36.173] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:36.172] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:36.172] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=938262771
[2026-11-02 10:31:36.173] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:36.782] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2616 source_master_ns=66449399988 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2720760
[2026-11-02 10:31:37.803] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2660 source_master_ns=67471077993 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2766106
[2026-11-02 10:31:38.848] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2705 source_master_ns=68515975952 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2811452
[2026-11-02 10:31:39.870] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2749 source_master_ns=69537653956 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2856798
[2026-11-02 10:31:40.891] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2793 source_master_ns=70559331961 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2902144
[2026-11-02 10:31:41.132] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3374178149 ntp_seconds=4002575501 ntp_fraction=566935683 arrival_ns=6176848814900
[2026-11-02 10:31:41.132] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3374178149
[2026-11-02 10:31:41.132] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:41.175] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2108050542 ntp_seconds=4002575501 ntp_fraction=751619276 arrival_ns=6176892163200
[2026-11-02 10:31:41.176] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2108050542
[2026-11-02 10:31:41.176] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:41.175] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=465521080 ntp_seconds=4002575501 ntp_fraction=751619276 arrival_ns=6176892172900
[2026-11-02 10:31:41.176] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=465521080
[2026-11-02 10:31:41.176] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=938262771 ntp_seconds=4002575501 ntp_fraction=755914244 arrival_ns=6176892693100
[2026-11-02 10:31:41.177] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=938262771
[2026-11-02 10:31:41.177] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:41.177] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:41.941] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2838 source_master_ns=71604229920 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2947490
[2026-11-02 10:31:42.958] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2882 source_master_ns=72625907925 raw_phase_ns=4536 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2992836
[2026-11-02 10:31:43.980] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2926 source_master_ns=73647585929 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3038182
[2026-11-02 10:31:45.002] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2970 source_master_ns=74669263934 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3083528
[2026-11-02 10:31:46.047] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3015 source_master_ns=75714161893 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3128874
[2026-11-02 10:31:46.150] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3374178149 ntp_seconds=4002575506 ntp_fraction=639950127 arrival_ns=6181866502100
[2026-11-02 10:31:46.150] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3374178149
[2026-11-02 10:31:46.150] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:46.179] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=465521080 ntp_seconds=4002575506 ntp_fraction=764504178 arrival_ns=6181895556900
[2026-11-02 10:31:46.179] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2108050542 ntp_seconds=4002575506 ntp_fraction=764504178 arrival_ns=6181895620600
[2026-11-02 10:31:46.179] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=938262771 ntp_seconds=4002575506 ntp_fraction=764504178 arrival_ns=6181895605000
[2026-11-02 10:31:46.180] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=938262771
[2026-11-02 10:31:46.180] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:46.179] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=465521080
[2026-11-02 10:31:46.181] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:46.179] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2108050542
[2026-11-02 10:31:46.182] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:47.068] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3059 source_master_ns=76735839897 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3174220
[2026-11-02 10:31:48.090] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3103 source_master_ns=77757517902 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3219566
[2026-11-02 10:31:49.135] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3148 source_master_ns=78802415861 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3264912
[2026-11-02 10:31:50.156] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3192 source_master_ns=79824093866 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3310258
[2026-11-02 10:31:51.167] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3374178149 ntp_seconds=4002575511 ntp_fraction=712964571 arrival_ns=6186883549000
[2026-11-02 10:31:51.167] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3374178149
[2026-11-02 10:31:51.167] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:51.178] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3236 source_master_ns=80845771870 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3355604
[2026-11-02 10:31:51.183] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2108050542 ntp_seconds=4002575511 ntp_fraction=781684047 arrival_ns=6186899523800
[2026-11-02 10:31:51.183] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2108050542
[2026-11-02 10:31:51.183] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:51.214] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=938262771 ntp_seconds=4002575511 ntp_fraction=919123001 arrival_ns=6186931204200
[2026-11-02 10:31:51.215] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=938262771
[2026-11-02 10:31:51.214] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=465521080 ntp_seconds=4002575511 ntp_fraction=919123001 arrival_ns=6186931234400
[2026-11-02 10:31:51.216] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=465521080
[2026-11-02 10:31:51.216] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:51.215] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:52.200] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3280 source_master_ns=81867449875 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3400950
[2026-11-02 10:31:53.245] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3325 source_master_ns=82912347834 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3446296
[2026-11-02 10:31:54.266] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3369 source_master_ns=83934025838 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3491642
[2026-11-02 10:31:55.288] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3413 source_master_ns=84955703843 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3536988
[2026-11-02 10:31:56.171] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3374178149 ntp_seconds=4002575516 ntp_fraction=734439407 arrival_ns=6191887927100
[2026-11-02 10:31:56.171] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3374178149
[2026-11-02 10:31:56.172] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:56.213] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2108050542 ntp_seconds=4002575516 ntp_fraction=910533066 arrival_ns=6191929072400
[2026-11-02 10:31:56.213] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2108050542
[2026-11-02 10:31:56.213] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:56.243] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=938262771 ntp_seconds=4002575516 ntp_fraction=1043677052 arrival_ns=6191959784100
[2026-11-02 10:31:56.243] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=938262771
[2026-11-02 10:31:56.244] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:56.243] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=465521080 ntp_seconds=4002575516 ntp_fraction=1043677052 arrival_ns=6191960167300
[2026-11-02 10:31:56.245] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=465521080
[2026-11-02 10:31:56.245] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:31:56.309] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3457 source_master_ns=85977381848 raw_phase_ns=4536 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3582334
[2026-11-02 10:31:57.033] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=86047603100 present_generation=1 present_canonical_target_ns=80433922902 present_canonical_dispatch_ns=80433922902 present_target_master_ns=86163235570 present_dispatch_master_ns=86163235570
[2026-11-02 10:31:57.035] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=86048641600 present_generation=1 present_canonical_target_ns=80466666666 present_canonical_dispatch_ns=80466666666 present_target_master_ns=86195979334 present_dispatch_master_ns=86195979334
[2026-11-02 10:31:57.354] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3502 source_master_ns=87022279807 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3627680
[2026-11-02 10:31:58.376] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3546 source_master_ns=88043957811 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3673026
[2026-11-02 10:31:59.398] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3590 source_master_ns=89065635816 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3718372
[2026-11-02 10:32:00.443] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3635 source_master_ns=90110533775 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3763718
[2026-11-02 10:32:01.173] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3374178149 ntp_seconds=4002575521 ntp_fraction=743029342 arrival_ns=6196890166700
[2026-11-02 10:32:01.174] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3374178149
[2026-11-02 10:32:01.174] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:32:01.247] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=465521080 ntp_seconds=4002575521 ntp_fraction=1060856922 arrival_ns=6196963834700
[2026-11-02 10:32:01.247] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=465521080
[2026-11-02 10:32:01.248] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:32:01.248] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2108050542 ntp_seconds=4002575521 ntp_fraction=1060856922 arrival_ns=6196964605100
[2026-11-02 10:32:01.248] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=938262771 ntp_seconds=4002575521 ntp_fraction=1060856922 arrival_ns=6196964102600
[2026-11-02 10:32:01.248] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=938262771
[2026-11-02 10:32:01.248] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2108050542
[2026-11-02 10:32:01.248] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:32:01.249] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:32:01.464] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3679 source_master_ns=91132211780 raw_phase_ns=4536 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3809064
[2026-11-02 10:32:02.486] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3723 source_master_ns=92153889784 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3854410
[2026-11-02 10:32:03.507] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3767 source_master_ns=93175567789 raw_phase_ns=4536 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3899756
[2026-11-02 10:32:04.553] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3812 source_master_ns=94220465748 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3945102
[2026-11-02 10:32:05.576] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3856 source_master_ns=95242143752 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3990448
[2026-11-02 10:32:06.178] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3374178149 ntp_seconds=4002575526 ntp_fraction=764504178 arrival_ns=6201895446400
[2026-11-02 10:32:06.179] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3374178149
[2026-11-02 10:32:06.179] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:32:06.273] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=465521080 ntp_seconds=4002575526 ntp_fraction=1172526071 arrival_ns=6201989747000
[2026-11-02 10:32:06.273] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=938262771 ntp_seconds=4002575526 ntp_fraction=1172526071 arrival_ns=6201989795600
[2026-11-02 10:32:06.273] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2108050542 ntp_seconds=4002575526 ntp_fraction=1172526071 arrival_ns=6201989866400
[2026-11-02 10:32:06.273] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=465521080
[2026-11-02 10:32:06.274] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:32:06.274] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2108050542
[2026-11-02 10:32:06.275] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:32:06.273] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=938262771
[2026-11-02 10:32:06.276] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:32:06.596] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3900 source_master_ns=96263821757 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4035794
[2026-11-02 10:32:07.641] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3945 source_master_ns=97308719716 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4081140
[2026-11-02 10:32:08.663] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3989 source_master_ns=98330397721 raw_phase_ns=4536 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4126486
[2026-11-02 10:32:09.685] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4033 source_master_ns=99352075725 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4171832
[2026-11-02 10:32:10.706] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4077 source_master_ns=100373753730 raw_phase_ns=4536 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4217178
[2026-11-02 10:32:11.202] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3374178149 ntp_seconds=4002575531 ntp_fraction=867583393 arrival_ns=6206919328200
[2026-11-02 10:32:11.203] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3374178149
[2026-11-02 10:32:11.203] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:32:11.275] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=938262771 ntp_seconds=4002575531 ntp_fraction=1181116006 arrival_ns=6206992036500
[2026-11-02 10:32:11.275] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=938262771
[2026-11-02 10:32:11.275] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:32:11.275] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=465521080 ntp_seconds=4002575531 ntp_fraction=1181116006 arrival_ns=6206992288000
[2026-11-02 10:32:11.276] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=465521080
[2026-11-02 10:32:11.276] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:32:11.276] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2108050542 ntp_seconds=4002575531 ntp_fraction=1185410973 arrival_ns=6206993069800
[2026-11-02 10:32:11.276] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2108050542
[2026-11-02 10:32:11.276] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:32:11.751] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4122 source_master_ns=101418651689 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4262524
[2026-11-02 10:32:12.773] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4166 source_master_ns=102440329693 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4307870
[2026-11-02 10:32:13.794] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4210 source_master_ns=103462007698 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4353216
[2026-11-02 10:32:14.839] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4255 source_master_ns=104506905657 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4398562
[2026-11-02 10:32:15.861] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4299 source_master_ns=105528583662 raw_phase_ns=4536 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4443908
[2026-11-02 10:32:16.226] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3374178149 ntp_seconds=4002575536 ntp_fraction=970662608 arrival_ns=6211943429000
[2026-11-02 10:32:16.227] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3374178149
[2026-11-02 10:32:16.227] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:32:16.301] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=938262771 ntp_seconds=4002575536 ntp_fraction=1288490188 arrival_ns=6212017303600
[2026-11-02 10:32:16.301] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=938262771
[2026-11-02 10:32:16.301] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:32:16.316] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2108050542 ntp_seconds=4002575536 ntp_fraction=1352914698 arrival_ns=6212032462000
[2026-11-02 10:32:16.316] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2108050542
[2026-11-02 10:32:16.316] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:32:16.320] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=465521080 ntp_seconds=4002575536 ntp_fraction=1374389534 arrival_ns=6212036976100
[2026-11-02 10:32:16.321] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=465521080
[2026-11-02 10:32:16.321] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:32:16.883] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4343 source_master_ns=106550261666 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4489254
[2026-11-02 10:32:17.904] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4387 source_master_ns=107571939671 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4534600
[2026-11-02 10:32:18.954] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4432 source_master_ns=108616837630 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4579946
[2026-11-02 10:32:19.971] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4476 source_master_ns=109638515634 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4625292
[2026-11-02 10:32:20.992] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4520 source_master_ns=110660193639 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4670638
[2026-11-02 10:32:21.237] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3374178149 ntp_seconds=4002575541 ntp_fraction=1017907249 arrival_ns=6216954151500
[2026-11-02 10:32:21.237] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3374178149
[2026-11-02 10:32:21.238] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:32:21.306] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=938262771 ntp_seconds=4002575541 ntp_fraction=1314259992 arrival_ns=6217022927500
[2026-11-02 10:32:21.306] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=938262771
[2026-11-02 10:32:21.307] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:32:21.337] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=465521080 ntp_seconds=4002575541 ntp_fraction=1438814044 arrival_ns=6217052227300
[2026-11-02 10:32:21.337] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=465521080
[2026-11-02 10:32:21.337] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:32:21.352] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2108050542 ntp_seconds=4002575541 ntp_fraction=1511828488 arrival_ns=6217069031700
[2026-11-02 10:32:21.352] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2108050542
[2026-11-02 10:32:21.352] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:32:22.037] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4565 source_master_ns=111705091598 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4715984
[2026-11-02 10:32:23.059] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4609 source_master_ns=112726769603 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4761330
[2026-11-02 10:32:24.081] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4653 source_master_ns=113748447607 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4806676
[2026-11-02 10:32:25.102] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4697 source_master_ns=114770125612 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4852022
[2026-11-02 10:32:26.148] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4742 source_master_ns=115815023571 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4897368
[2026-11-02 10:32:26.247] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3374178149 ntp_seconds=4002575546 ntp_fraction=1056561954 arrival_ns=6221963544500
[2026-11-02 10:32:26.247] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3374178149
[2026-11-02 10:32:26.248] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:32:26.308] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=938262771 ntp_seconds=4002575546 ntp_fraction=1322849927 arrival_ns=6222025005600
[2026-11-02 10:32:26.309] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=938262771
[2026-11-02 10:32:26.309] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:32:26.349] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=465521080 ntp_seconds=4002575546 ntp_fraction=1498943586 arrival_ns=6222066276600
[2026-11-02 10:32:26.350] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=465521080
[2026-11-02 10:32:26.350] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:32:26.380] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2108050542 ntp_seconds=4002575546 ntp_fraction=1627792605 arrival_ns=6222096512400
[2026-11-02 10:32:26.380] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2108050542
[2026-11-02 10:32:26.380] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 10:32:27.169] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4786 source_master_ns=116836701575 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4942714
[2026-11-02 10:32:28.191] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4830 source_master_ns=117858379580 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4988060
[2026-11-02 10:32:29.236] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4875 source_master_ns=118903277539 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5033406
[2026-11-02 10:32:30.258] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4919 source_master_ns=119924955544 raw_phase_ns=4535 filtered_phase_ns=4536 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5078752
[2026-11-02 10:32:30.444] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=6226161448600
[2026-11-02 10:32:30.445] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=3 generation=2 invalidated_generation=1 locked=0
[2026-11-02 10:32:30.445] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=11 group=composition-r376.source.0 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=23 video_raw_generation=0 video_ssrc=938262771
[2026-11-02 10:32:30.445] [info] [graph][runtime.node] av_reacquisition group=composition-r376.source.0 phase=purging old=1 next=2 transition=1
[2026-11-02 10:32:30.446] [info] [graph][runtime.node] rtp_ingress_purge node=8 transition=1 cutoff=0
[2026-11-02 10:32:30.446] [info] [graph][runtime.node] rtp_binder_purge node=11 transition=1 cutoff=3608
[2026-11-02 10:32:30.447] [info] [graph][runtime.node] av_reacquisition group=composition-r376.source.0 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=1 next=2 transition=1
[2026-11-02 10:32:30.457] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=6226174520700
[2026-11-02 10:32:30.458] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=3 generation=2 invalidated_generation=1 locked=0
[2026-11-02 10:32:30.458] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=6226174718600
[2026-11-02 10:32:30.458] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=30 group=composition-r376.source.1 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=23 video_raw_generation=0 video_ssrc=2108050542
[2026-11-02 10:32:30.458] [info] [graph][runtime.node] av_reacquisition group=composition-r376.source.1 phase=purging old=1 next=2 transition=1
[2026-11-02 10:32:30.459] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=3 generation=2 invalidated_generation=1 locked=0
[2026-11-02 10:32:30.460] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=6226176259200
[2026-11-02 10:32:30.460] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=51 group=composition-r376.source.2 buffered=0 phase=1 preserves_output=1 old_generation=1 next_generation=2 transition_sequence=1 published_generation=1 protocol_generation=1 evidence_revision=46 video_raw_generation=0 video_ssrc=465521080 audio_raw_generation=0 audio_ssrc=3374178149
[2026-11-02 10:32:30.459] [info] [graph][runtime.node] rtp_ingress_purge node=27 transition=1 cutoff=0
[2026-11-02 10:32:30.460] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=50 group=composition-r376.source.2 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=46 video_raw_generation=0 video_ssrc=465521080 audio_raw_generation=0 audio_ssrc=3374178149
[2026-11-02 10:32:30.460] [info] [graph][runtime.node] av_reacquisition group=composition-r376.source.2 phase=purging old=1 next=2 transition=1
[2026-11-02 10:32:30.460] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=3 generation=2 invalidated_generation=1 locked=0
[2026-11-02 10:32:30.459] [info] [graph][runtime.node] rtp_binder_purge node=30 transition=1 cutoff=3608
[2026-11-02 10:32:30.463] [info] [graph][runtime.node] rtp_ingress_purge node=46 transition=1 cutoff=0
[2026-11-02 10:32:30.463] [info] [graph][runtime.node] rtp_ingress_purge node=47 transition=1 cutoff=0
[2026-11-02 10:32:30.463] [info] [graph][runtime.node] rtp_binder_purge node=50 transition=1 cutoff=3608
[2026-11-02 10:32:30.463] [info] [graph][runtime.node] av_reacquisition group=composition-r376.source.1 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=1 next=2 transition=1
[2026-11-02 10:32:30.463] [info] [graph][runtime.node] rtp_binder_purge node=51 transition=1 cutoff=5169
[2026-11-02 10:32:30.464] [info] [graph][runtime.node] av_reacquisition group=composition-r376.source.2 event=purge_completed purge_ack=complete continuation=acquiring ack_count=2 old=1 next=2 transition=1
[2026-11-02 10:33:18.711] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=955926049 ntp_seconds=4002575598 ntp_fraction=3053721747 arrival_ns=6274427908500
[2026-11-02 10:33:18.711] [info] [graph][runtime.node] rtp_binder_boundary stage=deadline_established node=51 group=composition-r376.source.2 buffered=0 phase=2 preserves_output=1 raw_ssrc=955926049 raw_generation=1 raw_sequence=5171 old_generation=1 next_generation=2 transition_sequence=1 published_generation=1
[2026-11-02 10:33:18.711] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=955926049
[2026-11-02 10:33:18.712] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=0 generation=2 invalidated_generation=1 locked=0
[2026-11-02 10:33:18.712] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=4282593782 ntp_seconds=4002575598 ntp_fraction=3053721747 arrival_ns=6274427961200
[2026-11-02 10:33:18.712] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=4282593782
[2026-11-02 10:33:18.712] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:33:19.247] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=1 generation=2
[2026-11-02 10:33:19.748] [info] [graph][runtime.node] source_video_trace_summary node_id=61 node=ActivationReleaseSequencer generation=1 reason=generation_change steady_ns=6275464681300 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3422 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:33:19.748] [info] [graph][runtime.node] source_video_trace node_id=61 node=ActivationReleaseSequencer stage=activation source_identity=composition-r376.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=744864195 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6275464938900 group=composition-r376.source.2 release_identity=15636 transition_sequence=1 epoch_source_start_ns=993000000 epoch_master_release_ns=168861440400 video_count=16 min_presentation_ns=993000000 max_end_ns=1526333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:33:19.748] [info] [graph][runtime.node] source_video_trace node_id=61 node=ActivationReleaseSequencer stage=release_published source_identity=composition-r376.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=744864195 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6275465158500 group=composition-r376.source.2 release_identity=15636 transition_sequence=1 epoch_source_start_ns=993000000 epoch_master_release_ns=168861440400 video_count=16 min_presentation_ns=993000000 max_end_ns=1526333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:33:19.748] [info] [graph][runtime.node] source_video_trace_summary node_id=62 node=MediaAvBoundReleaseExtractorNode generation=1 reason=generation_change steady_ns=6275465190900 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=148 release_count=3571 release_committed_count=3422 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:33:19.748] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=release source_identity=composition-r376.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=744864195 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6275465490300 group=composition-r376.source.2 release_identity=15636 transition_sequence=1 epoch_source_start_ns=993000000 epoch_master_release_ns=168861440400 video_count=16 min_presentation_ns=993000000 max_end_ns=1526333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:33:19.749] [info] [graph][runtime.node] source_video_trace_summary node_id=64 node=VideoDecodeNode generation=1 reason=generation_change steady_ns=6275465733400 input_count=3460 accepted_count=3460 would_block_count=0 resolved_count=3460 output_attempt_count=0 published_count=3459 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=109 incomplete=0
[2026-11-02 10:33:19.749] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=input source_identity=composition-r376.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=744864195 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6275465962900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=109 incomplete=0
[2026-11-02 10:33:19.749] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1 source_master_ns=168863905252 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-11-02 10:33:19.749] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3 source_master_ns=168910345161 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=45346
[2026-11-02 10:33:19.749] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=release_committed source_identity=composition-r376.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=744864195 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6275465712400 group=composition-r376.source.2 release_identity=15636 transition_sequence=1 epoch_source_start_ns=993000000 epoch_master_release_ns=168861440400 video_count=16 min_presentation_ns=993000000 max_end_ns=1526333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=1 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:33:19.755] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=accepted source_identity=composition-r376.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=744864195 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=6275472279000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=109 incomplete=0
[2026-11-02 10:33:19.755] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=resolved source_identity=composition-r376.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=744864195 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6275472520300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=109 incomplete=0
[2026-11-02 10:33:19.756] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=published source_identity=composition-r376.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=744864195 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6275472697600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=109 incomplete=0
[2026-11-02 10:33:19.756] [info] [graph][runtime.node] source_video_trace_summary node_id=65 node=HardwareTransferNode generation=1 reason=generation_change steady_ns=6275472735800 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3459 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:33:19.756] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-11-02 10:33:19.756] [info] [graph][runtime.node] source_video_trace node_id=65 node=HardwareTransferNode stage=published source_identity=composition-r376.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=744864195 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6275473026800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:33:19.756] [info] [graph][runtime.node] source_video_trace_summary node_id=66 node=VideoFrameRateNode generation=1 reason=generation_change steady_ns=6275473294800 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3445 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:33:19.756] [info] [graph][runtime.node] source_video_trace_summary node_id=68 node=VideoFilterNode generation=1 reason=generation_change steady_ns=6275473344000 input_count=3445 accepted_count=3445 would_block_count=0 resolved_count=3445 output_attempt_count=0 published_count=3445 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:33:19.756] [info] [graph][runtime.node] source_video_trace node_id=66 node=VideoFrameRateNode stage=published source_identity=composition-r376.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=744864195 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6275473538100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:33:19.757] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=input source_identity=composition-r376.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=744864195 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6275473669200 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:33:19.757] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=accepted source_identity=composition-r376.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=744864195 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=6275473911800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:33:19.757] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=resolved source_identity=composition-r376.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=248288 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=6275474164400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:33:19.757] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=published source_identity=composition-r376.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=248288 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=6275474294300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:33:19.757] [info] [graph][runtime.node] source_video_trace_summary node_id=67 node=VideoPreparationNode generation=1 reason=generation_change steady_ns=6275474317000 input_count=3436 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3436 published_count=3431 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=9 incomplete=0
[2026-11-02 10:33:19.757] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=input source_identity=composition-r376.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=248288 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=6275474506500 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=9 incomplete=0
[2026-11-02 10:33:19.758] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=output_attempt source_identity=composition-r376.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=248288 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=6275474734100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=9 incomplete=0
[2026-11-02 10:33:19.758] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=published source_identity=composition-r376.source.2.input.video identity_link=direct generation=2 source_sequence=1 presentation_ns=993000000 duration_ns=33333333 decoded_sequence=unavailable pts=248288 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=6275474928500 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=9 incomplete=0
[2026-11-02 10:33:19.763] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_candidate source_identity=composition-r376.source.2.input.video generation=2 steady_ns=6275480507800 source_sequence=4 presentation_ns=1093000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 10:33:19.783] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-r376.source.2.input.video generation=2 steady_ns=6275500184600 source_sequence=4 presentation_ns=1093000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 10:33:20.297] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=48 source_master_ns=169955243121 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=90692
[2026-11-02 10:33:21.319] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=92 source_master_ns=170976921125 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=136038
[2026-11-02 10:33:22.341] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=136 source_master_ns=171998599130 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=181384
[2026-11-02 10:33:23.385] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=181 source_master_ns=173043497089 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=226730
[2026-11-02 10:33:23.726] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=955926049 ntp_seconds=4002575603 ntp_fraction=3118146256 arrival_ns=6279442729200
[2026-11-02 10:33:23.726] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=955926049
[2026-11-02 10:33:23.726] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:33:23.758] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=4282593782 ntp_seconds=4002575603 ntp_fraction=3251290243 arrival_ns=6279474609700
[2026-11-02 10:33:23.758] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=4282593782
[2026-11-02 10:33:23.759] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:33:24.406] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=225 source_master_ns=174065175093 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=272076
[2026-11-02 10:33:25.428] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=269 source_master_ns=175086853098 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=317422
[2026-11-02 10:33:26.449] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=313 source_master_ns=176108531102 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=362768
[2026-11-02 10:33:27.495] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=358 source_master_ns=177153429062 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=408114
[2026-11-02 10:33:28.516] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=402 source_master_ns=178175107066 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=453460
[2026-11-02 10:33:28.736] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=955926049 ntp_seconds=4002575608 ntp_fraction=3156800962 arrival_ns=6284452639700
[2026-11-02 10:33:28.736] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=955926049
[2026-11-02 10:33:28.736] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:33:28.785] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=4282593782 ntp_seconds=4002575608 ntp_fraction=3362959392 arrival_ns=6284500136100
[2026-11-02 10:33:28.785] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=4282593782
[2026-11-02 10:33:28.786] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:33:29.538] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=446 source_master_ns=179196785071 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=498806
[2026-11-02 10:33:30.583] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=491 source_master_ns=180241683030 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=544152
[2026-11-02 10:33:31.612] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=535 source_master_ns=181263361034 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=589498
[2026-11-02 10:33:32.626] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=579 source_master_ns=182285039039 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=634844
[2026-11-02 10:33:33.648] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=623 source_master_ns=183306717043 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=680190
[2026-11-02 10:33:33.755] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=955926049 ntp_seconds=4002575613 ntp_fraction=3242700308 arrival_ns=6289472222600
[2026-11-02 10:33:33.756] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=955926049
[2026-11-02 10:33:33.756] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:33:33.800] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=4282593782 ntp_seconds=4002575613 ntp_fraction=3435973836 arrival_ns=6289517185000
[2026-11-02 10:33:33.800] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=4282593782
[2026-11-02 10:33:33.801] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:33:34.693] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=668 source_master_ns=184351615003 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=725536
[2026-11-02 10:33:35.717] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=712 source_master_ns=185373293007 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=770882
[2026-11-02 10:33:36.737] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=756 source_master_ns=186394971012 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=816228
[2026-11-02 10:33:37.761] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=800 source_master_ns=187416649016 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=861574
[2026-11-02 10:33:38.770] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=955926049 ntp_seconds=4002575618 ntp_fraction=3307124817 arrival_ns=6294487172200
[2026-11-02 10:33:38.770] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=955926049
[2026-11-02 10:33:38.771] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:33:38.803] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=845 source_master_ns=188461546975 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=906920
[2026-11-02 10:33:38.817] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=4282593782 ntp_seconds=4002575618 ntp_fraction=3504693313 arrival_ns=6294533490200
[2026-11-02 10:33:38.818] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=4282593782
[2026-11-02 10:33:38.818] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:33:39.824] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=889 source_master_ns=189483224980 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=952266
[2026-11-02 10:33:40.846] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=933 source_master_ns=190504902985 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=997612
[2026-11-02 10:33:41.891] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=978 source_master_ns=191549800944 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1042958
[2026-11-02 10:33:42.912] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1022 source_master_ns=192571478948 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1088304
[2026-11-02 10:33:43.780] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=955926049 ntp_seconds=4002575623 ntp_fraction=3350074490 arrival_ns=6299497234200
[2026-11-02 10:33:43.781] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=955926049
[2026-11-02 10:33:43.781] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:33:43.821] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=4282593782 ntp_seconds=4002575623 ntp_fraction=3521873182 arrival_ns=6299537523500
[2026-11-02 10:33:43.821] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=4282593782
[2026-11-02 10:33:43.821] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:33:43.938] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1066 source_master_ns=193593156953 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1133650
[2026-11-02 10:33:44.956] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1110 source_master_ns=194614834957 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1178996
[2026-11-02 10:33:46.001] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1155 source_master_ns=195659732917 raw_phase_ns=15874 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1224342
[2026-11-02 10:33:47.022] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1199 source_master_ns=196681410921 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1269688
[2026-11-02 10:33:48.044] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1243 source_master_ns=197703088926 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1315034
[2026-11-02 10:33:48.782] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=955926049 ntp_seconds=4002575628 ntp_fraction=3354369458 arrival_ns=6304498513900
[2026-11-02 10:33:48.782] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=955926049
[2026-11-02 10:33:48.782] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:33:48.858] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=4282593782 ntp_seconds=4002575628 ntp_fraction=3685081939 arrival_ns=6304574903600
[2026-11-02 10:33:48.859] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=4282593782
[2026-11-02 10:33:48.859] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:33:49.089] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1288 source_master_ns=198747986885 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1360380
[2026-11-02 10:33:50.112] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1332 source_master_ns=199769664889 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1405726
[2026-11-02 10:33:51.133] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1376 source_master_ns=200791342894 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1451072
[2026-11-02 10:33:52.161] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1420 source_master_ns=201813020898 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1496418
[2026-11-02 10:33:53.199] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1465 source_master_ns=202857918858 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1541764
[2026-11-02 10:33:53.793] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=955926049 ntp_seconds=4002575633 ntp_fraction=3405909065 arrival_ns=6309510382600
[2026-11-02 10:33:53.794] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=955926049
[2026-11-02 10:33:53.794] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:33:53.880] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=4282593782 ntp_seconds=4002575633 ntp_fraction=3779571220 arrival_ns=6309597380600
[2026-11-02 10:33:53.881] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=4282593782
[2026-11-02 10:33:53.881] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:33:54.221] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1509 source_master_ns=203879596862 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1587110
[2026-11-02 10:33:55.242] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1553 source_master_ns=204901274867 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1632456
[2026-11-02 10:33:56.287] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1598 source_master_ns=205946172826 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1677802
[2026-11-02 10:33:57.309] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1642 source_master_ns=206967850830 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1723148
[2026-11-02 10:33:58.333] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1686 source_master_ns=207989528835 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1768494
[2026-11-02 10:33:58.806] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=955926049 ntp_seconds=4002575638 ntp_fraction=3457448673 arrival_ns=6314522621600
[2026-11-02 10:33:58.806] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=955926049
[2026-11-02 10:33:58.806] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:33:58.897] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=4282593782 ntp_seconds=4002575638 ntp_fraction=3848290697 arrival_ns=6314613374100
[2026-11-02 10:33:58.897] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=4282593782
[2026-11-02 10:33:58.897] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[CLI] runtime report: queued=20, peakQueued=931, threads=81, processThreads=129, workers=75, workerProgress=421190, workerProcessCalls=1623773, workerWaits=1202576, workerWakeups=1187128, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=642, logicalProcessors=22, averageSystemMachineCpuPercent=8.978018, averageProcessMachineCpuPercent=1.692689, peakProcessMachineCpuPercent=5.615551, averageProcessSingleCoreCpuPercent=37.239158, peakProcessSingleCoreCpuPercent=123.542117, initialWorkingSetBytes=208199680, workingSetBytes=258498560, peakWorkingSetBytes=268324864, totalPushed=361167, totalPopped=361123, droppedBuffers=0, encodedPacketsPushed=67929, encodedPacketsPopped=67928, backpressureItems=123, graphPayloadCurrentBytes=242254, graphPayloadHighWaterBytes=16114003, graphPayloadCurrentObjects=50, graphPayloadHighWaterObjects=750, graphPayloadReservations=152933, graphPayloadReleases=152883, graphPayloadPressureFailures=0, lastInputReceivedAtNs=6314854144300
[2026-11-02 10:33:59.330] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=208344142500 present_generation=1 present_canonical_target_ns=202733424036 present_canonical_dispatch_ns=202733424036 present_target_master_ns=208462736704 present_dispatch_master_ns=208462736704
[2026-11-02 10:33:59.334] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=208348136100 present_generation=1 present_canonical_target_ns=202766666666 present_canonical_dispatch_ns=202766666666 present_target_master_ns=208495979334 present_dispatch_master_ns=208495979334
[2026-11-02 10:33:59.379] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1730 source_master_ns=209011206839 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1813840
[CLI] runtime report: queued=22, peakQueued=933, threads=81, processThreads=129, workers=75, workerProgress=421750, workerProcessCalls=1625987, workerWaits=1204230, workerWakeups=1188760, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=643, logicalProcessors=22, averageSystemMachineCpuPercent=8.975286, averageProcessMachineCpuPercent=1.693800, peakProcessMachineCpuPercent=5.615551, averageProcessSingleCoreCpuPercent=37.263598, peakProcessSingleCoreCpuPercent=123.542117, initialWorkingSetBytes=208199680, workingSetBytes=258498560, peakWorkingSetBytes=268324864, totalPushed=361657, totalPopped=361611, droppedBuffers=0, encodedPacketsPushed=68031, encodedPacketsPopped=68029, backpressureItems=123, graphPayloadCurrentBytes=272528, graphPayloadHighWaterBytes=16114003, graphPayloadCurrentObjects=52, graphPayloadHighWaterObjects=750, graphPayloadReservations=153213, graphPayloadReleases=153161, graphPayloadPressureFailures=1, lastInputReceivedAtNs=6315170409800
[2026-11-02 10:34:00.398] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1775 source_master_ns=210056104799 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1859186
[2026-11-02 10:34:01.419] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1819 source_master_ns=211077782803 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1904532
[2026-11-02 10:34:02.440] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1863 source_master_ns=212099460808 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1949878
[2026-11-02 10:34:03.486] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1908 source_master_ns=213144358767 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1995224
[2026-11-02 10:34:03.821] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=955926049 ntp_seconds=4002575643 ntp_fraction=3526168150 arrival_ns=6319538469900
[2026-11-02 10:34:03.822] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=955926049
[2026-11-02 10:34:03.822] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:34:03.920] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=4282593782 ntp_seconds=4002575643 ntp_fraction=3951369912 arrival_ns=6319636842800
[2026-11-02 10:34:03.920] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=4282593782
[2026-11-02 10:34:03.921] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:34:04.507] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1952 source_master_ns=214166036771 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2040570
[2026-11-02 10:34:05.529] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1996 source_master_ns=215187714776 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2085916
[2026-11-02 10:34:06.551] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2040 source_master_ns=216209392780 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2131262
[2026-11-02 10:34:07.595] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2085 source_master_ns=217254290740 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2176608
[2026-11-02 10:34:08.617] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2129 source_master_ns=218275968744 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2221954
[2026-11-02 10:34:08.840] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=955926049 ntp_seconds=4002575648 ntp_fraction=3607772528 arrival_ns=6324557185900
[2026-11-02 10:34:08.841] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=955926049
[2026-11-02 10:34:08.841] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:34:08.955] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=4282593782 ntp_seconds=4002575648 ntp_fraction=4097398800 arrival_ns=6324671607200
[2026-11-02 10:34:08.955] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=4282593782
[2026-11-02 10:34:08.955] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:34:09.639] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2173 source_master_ns=219297646749 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2267300
[2026-11-02 10:34:10.684] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2218 source_master_ns=220342544708 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2312646
[2026-11-02 10:34:11.705] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2262 source_master_ns=221364222712 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2357992
[2026-11-02 10:34:12.727] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2306 source_master_ns=222385900717 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2403338
[2026-11-02 10:34:13.749] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2350 source_master_ns=223407578721 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2448684
[2026-11-02 10:34:13.863] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=955926049 ntp_seconds=4002575653 ntp_fraction=3706556776 arrival_ns=6329580435900
[2026-11-02 10:34:13.864] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=955926049
[2026-11-02 10:34:13.864] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:34:13.981] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=4282593782 ntp_seconds=4002575653 ntp_fraction=4209067950 arrival_ns=6329697637900
[2026-11-02 10:34:13.981] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=4282593782
[2026-11-02 10:34:13.981] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:34:14.794] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2395 source_master_ns=224452476681 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2494030
[2026-11-02 10:34:15.815] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2439 source_master_ns=225474154685 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2539376
[2026-11-02 10:34:16.837] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2483 source_master_ns=226495832690 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2584722
[2026-11-02 10:34:17.883] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2528 source_master_ns=227540730649 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2630068
[2026-11-02 10:34:18.868] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=955926049 ntp_seconds=4002575658 ntp_fraction=3728031612 arrival_ns=6334585474000
[2026-11-02 10:34:18.869] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=955926049
[2026-11-02 10:34:18.869] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:34:18.904] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2572 source_master_ns=228562408653 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2675414
[2026-11-02 10:34:18.989] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=4282593782 ntp_seconds=4002575658 ntp_fraction=4247722655 arrival_ns=6334705668800
[2026-11-02 10:34:18.989] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=4282593782
[2026-11-02 10:34:18.990] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:34:19.927] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2616 source_master_ns=229584086658 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2720760
[2026-11-02 10:34:20.947] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2660 source_master_ns=230605764663 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2766106
[2026-11-02 10:34:21.994] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2705 source_master_ns=231650662622 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2811452
[2026-11-02 10:34:23.013] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2749 source_master_ns=232672340626 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2856798
[2026-11-02 10:34:23.890] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=955926049 ntp_seconds=4002575663 ntp_fraction=3818225926 arrival_ns=6339606685200
[2026-11-02 10:34:23.890] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=955926049
[2026-11-02 10:34:23.890] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:34:24.024] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=4282593782 ntp_seconds=4002575664 ntp_fraction=98784247 arrival_ns=6339740704400
[2026-11-02 10:34:24.024] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=4282593782
[2026-11-02 10:34:24.025] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:34:24.035] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2793 source_master_ns=233694018631 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2902144
[2026-11-02 10:34:25.081] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2838 source_master_ns=234738916590 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2947490
[2026-11-02 10:34:26.102] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2882 source_master_ns=235760594595 raw_phase_ns=15874 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2992836
[2026-11-02 10:34:27.129] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2926 source_master_ns=236782272599 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3038182
[2026-11-02 10:34:28.146] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2970 source_master_ns=237803950604 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3083528
[2026-11-02 10:34:28.893] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=955926049 ntp_seconds=4002575668 ntp_fraction=3835405795 arrival_ns=6344609782400
[2026-11-02 10:34:28.893] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=955926049
[2026-11-02 10:34:28.893] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:34:29.058] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=4282593782 ntp_seconds=4002575669 ntp_fraction=249108103 arrival_ns=6344774801000
[2026-11-02 10:34:29.059] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=4282593782
[2026-11-02 10:34:29.059] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:34:29.190] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3015 source_master_ns=238848848563 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3128874
[2026-11-02 10:34:30.211] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3059 source_master_ns=239870526567 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3174220
[2026-11-02 10:34:31.234] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3103 source_master_ns=240892204572 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3219566
[2026-11-02 10:34:32.278] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3148 source_master_ns=241937102531 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3264912
[2026-11-02 10:34:33.300] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3192 source_master_ns=242958780536 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3310258
[2026-11-02 10:34:33.907] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=955926049 ntp_seconds=4002575673 ntp_fraction=3895535337 arrival_ns=6349624276200
[2026-11-02 10:34:33.908] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=955926049
[2026-11-02 10:34:33.908] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:34:34.062] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=4282593782 ntp_seconds=4002575674 ntp_fraction=266287972 arrival_ns=6349779121600
[2026-11-02 10:34:34.062] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=4282593782
[2026-11-02 10:34:34.063] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:34:34.321] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3236 source_master_ns=243980458540 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3355604
[2026-11-02 10:34:35.343] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3280 source_master_ns=245002136545 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3400950
[2026-11-02 10:34:36.388] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3325 source_master_ns=246047034504 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3446296
[2026-11-02 10:34:37.410] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3369 source_master_ns=247068712508 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3491642
[2026-11-02 10:34:38.432] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3413 source_master_ns=248090390513 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3536988
[2026-11-02 10:34:38.911] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=955926049 ntp_seconds=4002575678 ntp_fraction=3908420239 arrival_ns=6354627645300
[2026-11-02 10:34:38.911] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=955926049
[2026-11-02 10:34:38.912] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:34:39.088] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=4282593782 ntp_seconds=4002575679 ntp_fraction=377957122 arrival_ns=6354805469800
[2026-11-02 10:34:39.089] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=4282593782
[2026-11-02 10:34:39.090] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:34:39.453] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3457 source_master_ns=249112068517 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3582334
[2026-11-02 10:34:40.498] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3502 source_master_ns=250156966477 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3627680
[2026-11-02 10:34:41.520] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3546 source_master_ns=251178644481 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3673026
[2026-11-02 10:34:42.541] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3590 source_master_ns=252200322486 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3718372
[2026-11-02 10:34:43.587] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3635 source_master_ns=253245220445 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3763718
[2026-11-02 10:34:43.924] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=955926049 ntp_seconds=4002575683 ntp_fraction=3968549781 arrival_ns=6359641121100
[2026-11-02 10:34:43.924] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=955926049
[2026-11-02 10:34:43.925] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:34:44.099] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=4282593782 ntp_seconds=4002575684 ntp_fraction=420906795 arrival_ns=6359815151900
[2026-11-02 10:34:44.099] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=4282593782
[2026-11-02 10:34:44.100] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:34:44.608] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3679 source_master_ns=254266898449 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3809064
[2026-11-02 10:34:45.630] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3723 source_master_ns=255288576454 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3854410
[2026-11-02 10:34:46.652] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3767 source_master_ns=256310254458 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3899756
[2026-11-02 10:34:47.696] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3812 source_master_ns=257355152418 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3945102
[2026-11-02 10:34:48.718] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3856 source_master_ns=258376830422 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3990448
[2026-11-02 10:34:48.941] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=955926049 ntp_seconds=4002575688 ntp_fraction=4041564225 arrival_ns=6364658082200
[2026-11-02 10:34:48.941] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=955926049
[2026-11-02 10:34:48.941] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:34:49.115] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=4282593782 ntp_seconds=4002575689 ntp_fraction=489626271 arrival_ns=6364831084500
[2026-11-02 10:34:49.115] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=4282593782
[2026-11-02 10:34:49.115] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:34:49.739] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3900 source_master_ns=259398508427 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4035794
[2026-11-02 10:34:50.785] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3945 source_master_ns=260443406386 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4081140
[2026-11-02 10:34:51.806] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3989 source_master_ns=261465084390 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4126486
[2026-11-02 10:34:52.828] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4033 source_master_ns=262486762395 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4171832
[2026-11-02 10:34:53.855] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=4077 source_master_ns=263508440400 raw_phase_ns=15874 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4217178
[2026-11-02 10:34:53.950] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=955926049 ntp_seconds=4002575693 ntp_fraction=4075923963 arrival_ns=6369666653000
[2026-11-02 10:34:53.951] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=955926049
[2026-11-02 10:34:53.951] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:34:54.150] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=4282593782 ntp_seconds=4002575694 ntp_fraction=639950127 arrival_ns=6369866171500
[2026-11-02 10:34:54.150] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=4282593782
[2026-11-02 10:34:54.150] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-11-02 10:34:54.414] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=263428572200 present_generation=1 present_canonical_target_ns=257811156462 present_canonical_dispatch_ns=257811156462 present_target_master_ns=263540469130 present_dispatch_master_ns=263540469130
[2026-11-02 10:34:54.415] [info] [graph][runtime.node] datagram_reservation_trace stage=reservation_failed generation=1 endpoint=1 sequence=155923 release_master_ns=263362646001 deadline_master_ns=263429283264 original_protocol_deadline_master_ns=263462646001 prefix_completion_limit_master_ns=263429283264 activation_started_master_ns=263374179600 activation_completed_master_ns=263374182100 permit_started_master_ns=263428899100 permit_completed_master_ns=263428899500 snapshot_started_master_ns=263428899800 snapshot_sampled_master_ns=263428900000 snapshot_completed_master_ns=263428900300 reservation_completed_master_ns=263428926700 wait_started_master_ns=unavailable wait_completed_master_ns=unavailable submit_observed_master_ns=unavailable wait_attempts=0 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=0 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=0 wait_call_total_ns=0 wait_call_max_ns=0 wait_before_call_count=0 wait_before_call_total_ns=0 wait_before_call_max_ns=0 wait_arm_count=0 wait_arm_total_ns=0 wait_arm_max_ns=0 wait_api_wall_count=0 wait_api_wall_total_ns=0 wait_api_wall_max_ns=0 wait_return_to_next_clock_count=0 wait_return_to_next_clock_total_ns=0 wait_return_to_next_clock_max_ns=0 wait_target_lateness_count=0 wait_target_lateness_total_ns=0 wait_target_lateness_max_ns=0 original_error="IoFailure: Datagram GBRA reservation misses its immutable completion deadline sequence=155923 now_ns=263428900000 release_ns=263362646001 deadline_ns=263429283264 not_before_ns=263429099540 not_after_ns=263429068224 duration_ns=215040 rate=6250000 queue_wire_bytes=4624 average_queue_residence_ns=62658100"
[2026-11-02 10:34:54.416] [info] [graph][runtime.node] process.failed node=81 name=MediaScheduledDatagramSenderNode kind=ScheduledDatagramSender inputs=[channel=111 edge=111 stream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=123 edge=123 stream=Metadata edge_kind=Unknown payload=Unknown queued=0] outputs=[] error=IoFailure: Datagram GBRA reservation misses its immutable completion deadline sequence=155923 now_ns=263428900000 release_ns=263362646001 deadline_ns=263429283264 not_before_ns=263429099540 not_after_ns=263429068224 duration_ns=215040 rate=6250000 queue_wire_bytes=4624 average_queue_residence_ns=62658100
[2026-11-02 10:34:54.416] [info] [graph][runtime.node] worker.failed node=81 kind=ScheduledDatagramSender name=Common GCRA-paced nonblocking datagram sender error=IoFailure: Datagram GBRA reservation misses its immutable completion deadline sequence=155923 now_ns=263428900000 release_ns=263362646001 deadline_ns=263429283264 not_before_ns=263429099540 not_after_ns=263429068224 duration_ns=215040 rate=6250000 queue_wire_bytes=4624 average_queue_residence_ns=62658100
[2026-11-02 10:34:54.416] [info] [graph][runtime.node] worker.thread_cpu node=41 available=1 cpu_ns=1328125000 process_calls=13965
[2026-11-02 10:34:54.416] [info] [graph][runtime.node] worker.thread_cpu node=8 available=1 cpu_ns=375000000 process_calls=83284
[2026-11-02 10:34:54.416] [info] [graph][runtime.node] worker.thread_cpu node=27 available=1 cpu_ns=250000000 process_calls=91374
[2026-11-02 10:34:54.416] [info] [graph][runtime.node] worker.thread_cpu node=46 available=1 cpu_ns=656250000 process_calls=179434
[2026-11-02 10:34:54.416] [info] [graph][runtime.node] worker.thread_cpu node=47 available=1 cpu_ns=734375000 process_calls=178717
[2026-11-02 10:34:54.418] [info] [graph][runtime.node] process.failed node=64 name=VideoDecodeNode kind=VideoDecode inputs=[channel=84 edge=84 stream=Video edge_kind=Metadata payload=CodecContext queued=0; channel=85 edge=85 stream=Video edge_kind=InputPacket payload=Packet queued=0] outputs=[channel=87 edge=87 stream=Video edge_kind=RawFrame payload=Frame queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-11-02 10:34:54.418] [info] [graph][runtime.node] worker.thread_cpu node=64 available=1 cpu_ns=2703125000 process_calls=31966
[2026-11-02 10:34:54.416] [info] [graph][runtime.node] worker.thread_cpu node=9 available=1 cpu_ns=15625000 process_calls=80
[2026-11-02 10:34:54.416] [info] [graph][runtime.node] worker.thread_cpu node=10 available=1 cpu_ns=0 process_calls=104
[2026-11-02 10:34:54.416] [info] [graph][runtime.node] worker.thread_cpu node=29 available=1 cpu_ns=0 process_calls=104
[2026-11-02 10:34:54.416] [info] [graph][runtime.node] worker.thread_cpu node=49 available=1 cpu_ns=0 process_calls=411
[2026-11-02 10:34:54.416] [info] [graph][runtime.node] worker.thread_cpu node=11 available=1 cpu_ns=171875000 process_calls=10601
[2026-11-02 10:34:54.416] [info] [graph][runtime.node] worker.thread_cpu node=70 available=1 cpu_ns=656250000 process_calls=38553
[2026-11-02 10:34:54.416] [info] [graph][runtime.node] worker.thread_cpu node=12 available=1 cpu_ns=0 process_calls=81
[2026-11-02 10:34:54.416] [info] [graph][runtime.node] worker.thread_cpu node=30 available=1 cpu_ns=140625000 process_calls=10586
[2026-11-02 10:34:54.416] [info] [graph][runtime.node] worker.thread_cpu node=31 available=1 cpu_ns=0 process_calls=80
[2026-11-02 10:34:54.416] [info] [graph][runtime.node] worker.thread_cpu node=23 available=1 cpu_ns=78125000 process_calls=10573
[2026-11-02 10:34:54.416] [info] [graph][runtime.node] worker.thread_cpu node=50 available=1 cpu_ns=265625000 process_calls=19182
[2026-11-02 10:34:54.416] [info] [graph][runtime.node] worker.thread_cpu node=51 available=1 cpu_ns=375000000 process_calls=27686
[2026-11-02 10:34:54.416] [info] [graph][runtime.node] worker.thread_cpu node=42 available=1 cpu_ns=46875000 process_calls=10565
[2026-11-02 10:34:54.416] [info] [graph][runtime.node] worker.thread_cpu node=52 available=1 cpu_ns=0 process_calls=271
[2026-11-02 10:34:54.416] [info] [graph][runtime.node] worker.thread_cpu node=65 available=1 cpu_ns=187500000 process_calls=19237
[2026-11-02 10:34:54.416] [info] [graph][runtime.node] worker.thread_cpu node=13 available=1 cpu_ns=0 process_calls=105
[2026-11-02 10:34:54.416] [info] [graph][runtime.node] worker.thread_cpu node=32 available=1 cpu_ns=15625000 process_calls=104
[2026-11-02 10:34:54.416] [info] [graph][runtime.node] worker.thread_cpu node=71 available=1 cpu_ns=281250000 process_calls=28504
[2026-11-02 10:34:54.416] [info] [graph][runtime.node] worker.thread_cpu node=53 available=1 cpu_ns=0 process_calls=404
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=14 available=1 cpu_ns=62500000 process_calls=10756
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=24 available=1 cpu_ns=46875000 process_calls=11544
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=17 available=1 cpu_ns=0 process_calls=80
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=33 available=1 cpu_ns=46875000 process_calls=10756
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=25 available=1 cpu_ns=93750000 process_calls=10660
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=36 available=1 cpu_ns=0 process_calls=79
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=54 available=1 cpu_ns=218750000 process_calls=19371
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=44 available=1 cpu_ns=125000000 process_calls=10619
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=55 available=1 cpu_ns=375000000 process_calls=28024
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=59 available=1 cpu_ns=0 process_calls=267
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=67 available=1 cpu_ns=390625000 process_calls=19170
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=15 available=1 cpu_ns=109375000 process_calls=10722
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=34 available=1 cpu_ns=125000000 process_calls=10723
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=6 available=1 cpu_ns=2000000000 process_calls=43398
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=56 available=1 cpu_ns=140625000 process_calls=19273
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=57 available=1 cpu_ns=296875000 process_calls=27739
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=4 available=1 cpu_ns=5796875000 process_calls=34827
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=16 available=1 cpu_ns=62500000 process_calls=10801
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=77 available=1 cpu_ns=0 process_calls=6
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=35 available=1 cpu_ns=203125000 process_calls=10795
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=58 available=1 cpu_ns=421875000 process_calls=47122
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=5 available=1 cpu_ns=281250000 process_calls=33318
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=18 available=1 cpu_ns=62500000 process_calls=10660
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=74 available=1 cpu_ns=781250000 process_calls=56375
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=37 available=1 cpu_ns=156250000 process_calls=10654
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=60 available=1 cpu_ns=500000000 process_calls=46228
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=75 available=1 cpu_ns=250000000 process_calls=56445
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=19 available=1 cpu_ns=78125000 process_calls=10701
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=78 available=1 cpu_ns=359375000 process_calls=55796
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=38 available=1 cpu_ns=203125000 process_calls=10686
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=61 available=1 cpu_ns=328125000 process_calls=46052
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=76 available=1 cpu_ns=2812500000 process_calls=113911
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=20 available=1 cpu_ns=109375000 process_calls=10614
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=39 available=1 cpu_ns=125000000 process_calls=10598
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=79 available=1 cpu_ns=1312500000 process_calls=90738
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=62 available=1 cpu_ns=578125000 process_calls=46434
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=22 available=1 cpu_ns=1187500000 process_calls=13902
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=43 available=1 cpu_ns=78125000 process_calls=11382
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=66 available=1 cpu_ns=421875000 process_calls=20211
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=72 available=1 cpu_ns=375000000 process_calls=28094
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=26 available=1 cpu_ns=1078125000 process_calls=13726
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=45 available=1 cpu_ns=890625000 process_calls=13714
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=68 available=1 cpu_ns=1515625000 process_calls=22931
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=73 available=1 cpu_ns=671875000 process_calls=45091
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=7 available=1 cpu_ns=1437500000 process_calls=43932
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=80 available=1 cpu_ns=0 process_calls=6
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] worker.thread_cpu node=81 available=1 cpu_ns=9078125000 process_calls=55160
[2026-11-02 10:34:54.427] [info] [graph][runtime.node] datagram_batch_trace stage=last_completed generation=1 first_sequence=155904 last_sequence=155904 maximum_submit_interval_ns=50000 maximum_submit_interval_sequence=155904 maximum_start_lateness_ns=5302740 maximum_start_lateness_sequence=155904 first_release_master_ns=263362646000 first_deadline_master_ns=263462646000 last_release_master_ns=263362646000 last_deadline_master_ns=263462646000 first_submitted_master_ns=263368806700 last_submitted_master_ns=263368806700 worst_wait_started_master_ns=263368756200 worst_wait_completed_master_ns=263368756200 worst_submit_observed_master_ns=263368756700 wait_attempts=0 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=0 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=0 wait_call_total_ns=0 wait_call_max_ns=0 wait_before_call_count=0 wait_before_call_total_ns=0 wait_before_call_max_ns=0 wait_arm_count=0 wait_arm_total_ns=0 wait_arm_max_ns=0 wait_api_wall_count=0 wait_api_wall_total_ns=0 wait_api_wall_max_ns=0 wait_return_to_next_clock_count=0 wait_return_to_next_clock_total_ns=0 wait_return_to_next_clock_max_ns=0 wait_target_lateness_count=0 wait_target_lateness_total_ns=0 wait_target_lateness_max_ns=0
[2026-11-02 10:34:54.427] [info] [graph][runtime.node] datagram_batch_trace stage=active generation=1 first_sequence=155905 last_sequence=155926 maximum_submit_interval_ns=144000 maximum_submit_interval_sequence=155920 maximum_start_lateness_ns=34406460 maximum_start_lateness_sequence=155918 first_release_master_ns=263362646001 first_deadline_master_ns=263429283264 last_release_master_ns=263362646001 last_deadline_master_ns=263429283264 first_submitted_master_ns=263374241800 last_submitted_master_ns=263428884500 worst_wait_started_master_ns=263389940600 worst_wait_completed_master_ns=263424555700 worst_submit_observed_master_ns=263424557900 wait_attempts=17 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=17 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=17 wait_call_total_ns=52737400 wait_call_max_ns=34612400 wait_before_call_count=17 wait_before_call_total_ns=2700 wait_before_call_max_ns=400 wait_arm_count=17 wait_arm_total_ns=381300 wait_arm_max_ns=250600 wait_api_wall_count=17 wait_api_wall_total_ns=52333500 wait_api_wall_max_ns=34602600 wait_return_to_next_clock_count=17 wait_return_to_next_clock_total_ns=20900 wait_return_to_next_clock_max_ns=2700 wait_target_lateness_count=17 wait_target_lateness_total_ns=49485020 wait_target_lateness_max_ns=34404260 worst_wait_metric=call_wall worst_wait_sequence=155918 worst_wait_target_master_ns=263390151440 worst_wait_sampled_master_ns=263389940600 worst_wait_sampled_steady_ns=6370092864200 worst_wait_timeout_ns=210840 worst_wait_entered_steady_ns=6370092864300 worst_wait_returned_steady_ns=6370127476700 worst_wait_arm_before_steady_ns=6370092864500 worst_wait_arm_after_steady_ns=6370092872000 worst_wait_api_before_steady_ns=6370092872100 worst_wait_api_returned_steady_ns=6370127474700 worst_wait_windows_due_100ns=-2109 worst_wait_windows_result=1 worst_wait_windows_error=unavailable worst_wait_windows_pid=33268 worst_wait_windows_tid=556 worst_wait_windows_qpc_frequency_hz=10000000 worst_wait_next_master_ns=263424555700 worst_wait_next_steady_ns=6370127479400 worst_wait_outcome=deadline
[2026-11-02 10:34:54.427] [info] [graph][runtime.node] scheduled_datagram_sender stage=failed generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=27863 committed_datagrams=155922 committed_payload_bytes=182057884 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=79296 pacing_reserved=155922 pacing_cancelled=0 pacing_submitted=155922 pacing_maximum_submit_lateness_ns=34406460 pacing_worst_late_global_sequence=155918 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=4 backlog_current_wire_bytes=4624 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=79296 backlog_max_residence_ns=79094000 last_materialized_sequence=155926 last_scheduled_sequence=155926 last_submitted_sequence=155922 last_committed_sequence=155922 submitted_service_generation=1 submitted_service_sequence=155922 submitted_service_completed_ns=263428884500 submitted_service_duration_ns=215040 submitted_service_available_ns=263429099540 submitted_scope_available_ns=263429099540 shared_scope_charged_datagrams=155922 shared_scope_charged_wire_bytes=186423700 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2154 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2154 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=188010 endpoint_1_target_effective_socket_bytes=188010 endpoint_1_api_requested_socket_bytes=188010 endpoint_1_effective_socket_bytes=188010 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=155922 endpoint_1_committed_payload_bytes=182057884
[2026-11-02 10:34:54.428] [info] [graph][runtime.node] worker.secondary_failure node=81 kind=ScheduledDatagramSender name=Common GCRA-paced nonblocking datagram sender error=IoFailure: Datagram GBRA reservation misses its immutable completion deadline sequence=155923 now_ns=263428900000 release_ns=263362646001 deadline_ns=263429283264 not_before_ns=263429099540 not_after_ns=263429068224 duration_ns=215040 rate=6250000 queue_wire_bytes=4624 average_queue_residence_ns=62658100
[2026-11-02 10:34:54.416] [info] [graph][runtime.node] worker.thread_cpu node=28 available=1 cpu_ns=0 process_calls=80
[2026-11-02 10:34:54.416] [info] [graph][runtime.node] worker.thread_cpu node=48 available=1 cpu_ns=15625000 process_calls=266
[2026-11-02 10:34:54.417] [info] [graph][runtime.node] process.failed node=2 name=VideoEncodeNode kind=VideoEncode inputs=[channel=1 edge=1 stream=Video edge_kind=Metadata payload=CodecContext queued=0; channel=104 edge=104 stream=Video edge_kind=RawFrame payload=Frame queued=1] outputs=[channel=107 edge=107 stream=Video edge_kind=EncodedPacket payload=Packet queued=0; channel=113 edge=113 stream=Video edge_kind=Metadata payload=CodecContext queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-11-02 10:34:54.428] [info] [graph][runtime.node] worker.thread_cpu node=2 available=1 cpu_ns=3156250000 process_calls=23210
[2026-11-02 10:34:54.473] [info] [graph][runtime.lifecycle] node_abort_payload node=81 before_objects=53 after_objects=53 before_bytes=276163 after_bytes=276163 before_reservations=201377 after_reservations=201377 before_releases=201324 after_releases=201324
[2026-11-02 10:34:54.474] [info] [graph][runtime.node] mpegts_wire_materializer stage=aborted maximum_materialized_after_release_ns=52723912 worst_materialized_at_ns=41682037300 worst_release_ns=41629313388 worst_deadline_ns=41695980056 worst_global_sequence=25713
[2026-11-02 10:34:54.474] [info] [graph][runtime.lifecycle] node_abort_payload node=79 before_objects=53 after_objects=53 before_bytes=276163 after_bytes=276163 before_reservations=201377 after_reservations=201377 before_releases=201324 after_releases=201324
[2026-11-02 10:34:54.474] [info] [graph][runtime.node] au_processing_trace stage=pending_au_abort generation=1 stream=video canonical_sequence=7733 emit_master_ns=263362646001 dispatch_master_ns=263462646001 ready_master_ns=263351549700 scheduled_created_steady_ns=6370054413700 adapter_dequeued_steady_ns=6370054451400 adapted_created_steady_ns=6370054452000 write_entered_steady_ns=6370054471800 session_ready_steady_ns=6370054473400 codec_receive_begin_steady_ns=6370003489300 codec_receive_end_steady_ns=6370003489900 codec_encoder_node=2 codec_selected_token_id=7733 codec_selected_token_generation=1 codec_packet_token_id=7733 codec_packet_token_generation=1 codec_token_mapping=copy_opaque codec_input_pts=7732 codec_timebase_num=1 codec_timebase_den=30 codec_accepted_send_count=1 codec_token_output_ordinal=1 codec_packet_pts=7732 codec_packet_dts=7732 codec_packet_bytes=27464 codec_identity_incomplete=0
[2026-11-02 10:34:54.474] [info] [graph][runtime.node] ts_prefix_commit_summary stage=current_au_abort generation=1 stream=video canonical_sequence=7733 emit_master_ns=263362646001 dispatch_master_ns=263462646001 published=1 completed=0 samples=0 would_block=9 incomplete=0 publish_to_protocol_commit_completed_total_ns=0 publish_to_protocol_commit_completed_max_ns=0 publish_to_protocol_commit_completed_max_prefix_offset=0 protocol_commit_completed_to_mux_resume_total_ns=0 protocol_commit_completed_to_mux_resume_max_ns=0 protocol_commit_completed_to_mux_resume_max_prefix_offset=0
[2026-11-02 10:34:54.475] [info] [graph][runtime.node] au_processing_trace stage=previous_completed_au_trace_on_abort generation=1 stream=audio canonical_sequence=11100 emit_master_ns=263347589311 dispatch_master_ns=263447589311 ready_master_ns=263340162700 scheduled_created_steady_ns=6370031515200 adapter_dequeued_steady_ns=6370031579500 adapted_created_steady_ns=6370031579800 write_entered_steady_ns=6370043085100 session_ready_steady_ns=6370043086300 codec_identity=unavailable
[2026-11-02 10:34:54.475] [info] [graph][runtime.node] ts_prefix_commit_summary stage=previous_completed_au_on_abort generation=1 stream=audio canonical_sequence=11100 emit_master_ns=263347589311 dispatch_master_ns=263447589311 published=1 completed=1 samples=1 would_block=2 incomplete=0 publish_to_protocol_commit_completed_total_ns=7930500 publish_to_protocol_commit_completed_max_ns=7930500 publish_to_protocol_commit_completed_max_prefix_offset=0 protocol_commit_completed_to_mux_resume_total_ns=35100 protocol_commit_completed_to_mux_resume_max_ns=35100 protocol_commit_completed_to_mux_resume_max_prefix_offset=0
[2026-11-02 10:34:54.475] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 produced_datagrams=155926 produced_ts_payload_bytes=182062396 pending_bytes=0 peak_pending_bytes=110920 access_units=18833 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=32789115 maximum_mux_input_after_emit_ns=61444125 maximum_mux_input_after_dispatch_ns=-38555875 worst_mux_input_ready_ns=246992525500 worst_mux_input_emit_ns=246931081375 worst_mux_input_dispatch_ns=247031081375 maximum_protocol_batch_produced_after_release_ns=52675412 worst_protocol_batch_produced_ns=41681988800 worst_protocol_batch_release_ns=41629313388 worst_protocol_batch_deadline_ns=41695980056 worst_protocol_batch_datagrams=1 exit_reason=aborted
[2026-11-02 10:34:54.475] [info] [graph][runtime.lifecycle] node_abort_payload node=76 before_objects=53 after_objects=53 before_bytes=276163 after_bytes=276163 before_reservations=201377 after_reservations=201377 before_releases=201324 after_releases=201324
[2026-11-02 10:34:54.475] [info] [graph][runtime.lifecycle] node_abort_payload node=78 before_objects=53 after_objects=53 before_bytes=276163 after_bytes=276163 before_reservations=201377 after_reservations=201377 before_releases=201324 after_releases=201324
[2026-11-02 10:34:54.476] [info] [graph][runtime.lifecycle] node_abort_payload node=75 before_objects=53 after_objects=53 before_bytes=276163 after_bytes=276163 before_reservations=201377 after_reservations=201377 before_releases=201324 after_releases=201324
[2026-11-02 10:34:54.476] [info] [graph][runtime.node] audio_stage_observation node=74 stage=scheduler_input count=11104 last_generation=1 last_begin=11369472 last_end=11370496 last_rate=44100 last_steady_ns=6370131426800 maximum_inter_event_gap_ns=49027000 gap_begin_generation=1 gap_begin_begin=8940544 gap_begin_end=8941568 gap_begin_rate=44100 gap_begin_steady_ns=6315047041700 gap_end_generation=1 gap_end_begin=8941568 gap_end_end=8942592 gap_end_rate=44100 gap_end_steady_ns=6315096068700
[2026-11-02 10:34:54.476] [info] [graph][runtime.node] av_scheduler_correlated_wait generation=1 canonical_sequence=7734 missing=audio begin_steady_ns=6370079560800 end_steady_ns=6370128469400 observed_wait_ns=48908600 held_dispatch_master_ns=263495979334 begin_master_ns=263376636900 end_master_ns=263425545100 dispatch_remaining_at_end_ns=70434234 resumed_dispatch_master_ns=263517249175
[2026-11-02 10:34:54.476] [info] [graph][runtime.lifecycle] node_abort_payload node=74 before_objects=53 after_objects=52 before_bytes=276163 after_bytes=275471 before_reservations=201377 after_reservations=201377 before_releases=201324 after_releases=201325
[2026-11-02 10:34:54.476] [info] [graph][runtime.node] audio_stage_observation node=5 stage=canonicalizer_input count=11104 last_generation=1 last_begin=11369472 last_end=11370496 last_rate=44100 last_steady_ns=6370131402400 maximum_inter_event_gap_ns=49039800 gap_begin_generation=1 gap_begin_begin=8940544 gap_begin_end=8941568 gap_begin_rate=44100 gap_begin_steady_ns=6315046971600 gap_end_generation=1 gap_end_begin=8941568 gap_end_end=8942592 gap_end_rate=44100 gap_end_steady_ns=6315096011400
[2026-11-02 10:34:54.476] [info] [graph][runtime.node] audio_stage_observation node=5 stage=canonicalizer_committed count=11104 last_generation=1 last_begin=11369472 last_end=11370496 last_rate=44100 last_steady_ns=6370131409300 maximum_inter_event_gap_ns=49026700 gap_begin_generation=1 gap_begin_begin=8940544 gap_begin_end=8941568 gap_begin_rate=44100 gap_begin_steady_ns=6315047000500 gap_end_generation=1 gap_end_begin=8941568 gap_end_end=8942592 gap_end_rate=44100 gap_end_steady_ns=6315096027200
[2026-11-02 10:34:54.476] [info] [graph][runtime.lifecycle] node_abort_payload node=5 before_objects=52 after_objects=52 before_bytes=275471 after_bytes=275471 before_reservations=201377 after_reservations=201377 before_releases=201325 after_releases=201325
[2026-11-02 10:34:54.477] [info] [graph][runtime.lifecycle] node_abort_payload node=77 before_objects=52 after_objects=52 before_bytes=275471 after_bytes=275471 before_reservations=201377 after_reservations=201377 before_releases=201325 after_releases=201325
[2026-11-02 10:34:54.477] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_input count=11106 last_generation=1 last_begin=11371520 last_end=11372544 last_rate=44100 last_steady_ns=6370130624300 maximum_inter_event_gap_ns=49094500 gap_begin_generation=1 gap_begin_begin=11369472 gap_begin_end=11370496 gap_begin_rate=44100 gap_begin_steady_ns=6370078988400 gap_end_generation=1 gap_end_begin=11370496 gap_end_end=11371520 gap_end_rate=44100 gap_end_steady_ns=6370128082900
[2026-11-02 10:34:54.477] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_submitted count=11106 last_generation=1 last_begin=11371520 last_end=11372544 last_rate=44100 last_steady_ns=6370131252900 maximum_inter_event_gap_ns=49088700 gap_begin_generation=1 gap_begin_begin=11369472 gap_begin_end=11370496 gap_begin_rate=44100 gap_begin_steady_ns=6370079326800 gap_end_generation=1 gap_end_begin=11370496 gap_end_end=11371520 gap_end_rate=44100 gap_end_steady_ns=6370128415500
[2026-11-02 10:34:54.477] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_packet count=11104 last_generation=1 last_begin=11369472 last_end=11370496 last_rate=44100 last_steady_ns=6370131277400 maximum_inter_event_gap_ns=49085400 gap_begin_generation=1 gap_begin_begin=11367424 gap_begin_end=11368448 gap_begin_rate=44100 gap_begin_steady_ns=6370079336500 gap_end_generation=1 gap_end_begin=11368448 gap_end_end=11369472 gap_end_rate=44100 gap_end_steady_ns=6370128421900
[2026-11-02 10:34:54.477] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_committed count=11104 last_generation=1 last_begin=11369472 last_end=11370496 last_rate=44100 last_steady_ns=6370131295400 maximum_inter_event_gap_ns=49083400 gap_begin_generation=1 gap_begin_begin=11367424 gap_begin_end=11368448 gap_begin_rate=44100 gap_begin_steady_ns=6370079345700 gap_end_generation=1 gap_end_begin=11368448 gap_end_end=11369472 gap_end_rate=44100 gap_end_steady_ns=6370128429100
[2026-11-02 10:34:54.477] [info] [graph][runtime.lifecycle] codec_owner_release node=4 kind=AudioEncode owner_references=4
[2026-11-02 10:34:54.477] [info] [graph][runtime.lifecycle] node_abort_payload node=4 before_objects=52 after_objects=52 before_bytes=275471 after_bytes=275471 before_reservations=201377 after_reservations=201377 before_releases=201325 after_releases=201325
[2026-11-02 10:34:54.477] [info] [graph][runtime.lifecycle] node_abort_payload node=80 before_objects=52 after_objects=52 before_bytes=275471 after_bytes=275471 before_reservations=201377 after_reservations=201377 before_releases=201325 after_releases=201325
[2026-11-02 10:34:54.478] [info] [graph][runtime.node] audio_stage_observation node=7 stage=aggregate_built count=11106 last_generation=1 last_begin=11371520 last_end=11372544 last_rate=44100 last_steady_ns=6370130413200 maximum_inter_event_gap_ns=49239400 gap_begin_generation=1 gap_begin_begin=11369472 gap_begin_end=11370496 gap_begin_rate=44100 gap_begin_steady_ns=6370078783600 gap_end_generation=1 gap_end_begin=11370496 gap_end_end=11371520 gap_end_rate=44100 gap_end_steady_ns=6370128023000
[2026-11-02 10:34:54.478] [info] [graph][runtime.node] audio_stage_observation node=7 stage=aggregate_committed count=11106 last_generation=1 last_begin=11371520 last_end=11372544 last_rate=44100 last_steady_ns=6370130580500 maximum_inter_event_gap_ns=49642500 gap_begin_generation=1 gap_begin_begin=11369472 gap_begin_end=11370496 gap_begin_rate=44100 gap_begin_steady_ns=6370078816600 gap_end_generation=1 gap_end_begin=11370496 gap_end_end=11371520 gap_end_rate=44100 gap_end_steady_ns=6370128459100
[2026-11-02 10:34:54.478] [info] [graph][runtime.node] continuous_audio_aggregate.summary reason=abort output_group=composition-r376.output audio_commits=11106 real_samples=9198592 silence_samples=2173952
[2026-11-02 10:34:54.478] [info] [graph][runtime.lifecycle] node_abort_payload node=7 before_objects=52 after_objects=46 before_bytes=275471 after_bytes=226319 before_reservations=201377 after_reservations=201377 before_releases=201325 after_releases=201331
[2026-11-02 10:34:54.478] [info] [graph][runtime.lifecycle] codec_owner_release node=2 kind=VideoEncode owner_references=4
[2026-11-02 10:34:54.478] [info] [graph][runtime.node] video_encoder_execution_observation node=2 count=7736 maximum_call_ns=18414100 maximum_begin_steady_ns=6315070825400 maximum_end_steady_ns=6315089239500 maximum_encoder_pts=6084 maximum_native_result=0 maximum_between_submissions_ns=51986000 before_gap_begin_steady_ns=6370071790400 before_gap_end_steady_ns=6370077224900 before_gap_encoder_pts=7734 before_gap_native_result=0 after_gap_begin_steady_ns=6370129210900 after_gap_end_steady_ns=6370134198000 after_gap_encoder_pts=7735 after_gap_native_result=0 payload_wait_maximum_ns=801700 payload_wait_begin_steady_ns=6315103237800 payload_wait_end_steady_ns=6315104039500
[2026-11-02 10:34:54.478] [info] [graph][runtime.lifecycle] node_abort_payload node=2 before_objects=46 after_objects=46 before_bytes=226319 after_bytes=226319 before_reservations=201377 after_reservations=201377 before_releases=201331 after_releases=201331
[2026-11-02 10:34:54.479] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=process count=43398 maximum_ns=20786000 begin_steady_ns=6319100099500 end_steady_ns=6319120885500 video_cursor=6205 audio_cursor=not_owned next_video_cursor=6205 next_audio_cursor=not_owned error=None return_state=progress deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-11-02 10:34:54.479] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=purge count=43398 maximum_ns=856300 begin_steady_ns=6282701957300 end_steady_ns=6282702813600 video_cursor=5114 audio_cursor=not_owned next_video_cursor=5114 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 10:34:54.479] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=bind count=43398 maximum_ns=313700 begin_steady_ns=6353265985300 end_steady_ns=6353266299000 video_cursor=7230 audio_cursor=not_owned next_video_cursor=7230 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 10:34:54.479] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=consume count=43398 maximum_ns=11648900 begin_steady_ns=6213367164100 end_steady_ns=6213378813000 video_cursor=3034 audio_cursor=not_owned next_video_cursor=3034 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 10:34:54.479] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=activate count=17 maximum_ns=251400 begin_steady_ns=6112265569200 end_steady_ns=6112265820600 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 10:34:54.479] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=build_video count=7737 maximum_ns=20736500 begin_steady_ns=6319100114300 end_steady_ns=6319120850800 video_cursor=6205 audio_cursor=not_owned next_video_cursor=6205 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 10:34:54.479] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=canvas count=7737 maximum_ns=20706000 begin_steady_ns=6319100122000 end_steady_ns=6319120828000 video_cursor=6205 audio_cursor=not_owned next_video_cursor=6205 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 10:34:54.479] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=emit count=7737 maximum_ns=8774600 begin_steady_ns=6269866515400 end_steady_ns=6269875290000 video_cursor=4728 audio_cursor=not_owned next_video_cursor=4728 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 10:34:54.480] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=between_calls_previous_return count=43397 maximum_ns=127569800 begin_steady_ns=6112132418800 end_steady_ns=6112259988600 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=waiting deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-11-02 10:34:54.480] [info] [graph][runtime.node] continuous_aggregate.summary reason=abort output_group=composition-r376.output output_generation=1 video_commits=7737 source[0].group=composition-r376.source.0 source[0].identity=composition-r376.source.0.input.video source[0].real_frames=3417 source[0].black_frames=4320 source[0].contribution_generation=0 source[1].group=composition-r376.source.1 source[1].identity=composition-r376.source.1.input.video source[1].real_frames=3418 source[1].black_frames=4319 source[1].contribution_generation=0 source[2].group=composition-r376.source.2 source[2].identity=composition-r376.source.2.input.video source[2].real_frames=6258 source[2].black_frames=1479 source[2].contribution_generation=2  source[2].first_interval_miss_first_candidate_at_miss_source_sequence=unavailable  source[2].first_interval_miss_first_candidate_at_miss_presentation_ns=unavailable  source[2].first_interval_miss_first_candidate_at_miss_duration_ns=unavailable  source[2].first_interval_miss_first_candidate_at_miss_decoded_sequence=unavailable  source[2].first_interval_miss_first_real_at_miss_source_sequence=unavailable  source[2].first_interval_miss_first_real_at_miss_presentation_ns=unavailable  source[2].first_interval_miss_first_real_at_miss_duration_ns=unavailable  source[2].first_interval_miss_first_real_at_miss_decoded_sequence=unavailable  source[2].first_interval_miss_first_candidate_final_source_sequence=4  source[2].first_interval_miss_first_candidate_final_presentation_ns=1093000000  source[2].first_interval_miss_first_candidate_final_duration_ns=33333333  source[2].first_interval_miss_first_candidate_final_decoded_sequence=unavailable  source[2].first_interval_miss_first_real_final_source_sequence=4  source[2].first_interval_miss_first_real_final_presentation_ns=1093000000  source[2].first_interval_miss_first_real_final_duration_ns=33333333  source[2].first_interval_miss_first_real_final_decoded_sequence=unavailable source[2].first_interval_miss_cursor=4896 source[2].first_interval_miss_target_ns=168929312668 source[2].first_interval_miss_steady_ns=6275465659700 source[2].first_interval_miss_generation=2 source[2].first_interval_miss_capacity=7 source[2].first_interval_miss_before=0 source[2].first_interval_miss_after=0 source[2].first_interval_miss_expired=0 source[2].first_interval_miss_channel_depth=0 source[2].first_interval_miss_examined_begin_ns=unavailable source[2].first_interval_miss_examined_end_ns=unavailable source[2].first_interval_miss_epoch_source_start_ns=993000000 source[2].first_interval_miss_epoch_master_release_ns=168861440400 source[2].first_interval_miss_prepare_expired=0 source[2].first_interval_miss_prepare_old_generation=0 source[2].first_interval_miss_prepare_incomplete=0 source[2].first_interval_miss_first_real_cursor_at_miss=unavailable source[2].first_interval_miss_first_real_steady_ns_at_miss=unavailable source[2].first_interval_miss_first_candidate_steady_ns_at_miss=unavailable source[2].first_interval_miss_first_candidate_begin_ns_at_miss=unavailable source[2].first_interval_miss_first_candidate_end_ns_at_miss=unavailable source[2].first_interval_miss_prepare_last_begin_ns=unavailable source[2].first_interval_miss_prepare_last_end_ns=unavailable source[2].first_interval_miss_first_candidate_steady_ns_final=6275480507800 source[2].first_interval_miss_first_candidate_begin_ns_final=168961440400 source[2].first_interval_miss_first_candidate_end_ns_final=168994773733 source[2].first_interval_miss_first_real_cursor=4897 source[2].first_interval_miss_first_real_steady_ns=6275500184600 source[2].first_interval_miss_cursor_committed_generation=0 source[2].first_interval_miss_cursor_committed_steady_ns=6275465802400
[2026-11-02 10:34:54.480] [info] [graph][runtime.node] cuda_canvas_call stage=context_push count=6262 operation=copy maximum_ns=283500 begin_steady_ns=6163365958900 end_steady_ns=6163366242400 native_result=0 thread_cpu_ns=0
[2026-11-02 10:34:54.480] [info] [graph][runtime.node] cuda_canvas_call stage=source_wait count=19357 operation=copy maximum_ns=624400 begin_steady_ns=6183934045300 end_steady_ns=6183934669700 native_result=0 thread_cpu_ns=0
[2026-11-02 10:34:54.480] [info] [graph][runtime.node] cuda_canvas_call stage=plane_copy count=38716 operation=copy maximum_ns=5406000 begin_steady_ns=6329901449800 end_steady_ns=6329906855800 native_result=0 thread_cpu_ns=0
[2026-11-02 10:34:54.480] [info] [graph][runtime.node] cuda_canvas_call stage=destination_wait count=2 operation=upload maximum_ns=73300 begin_steady_ns=6112029250300 end_steady_ns=6112029323600 native_result=0 thread_cpu_ns=0
[2026-11-02 10:34:54.481] [info] [graph][runtime.node] cuda_canvas_call stage=context_pop count=6262 operation=copy maximum_ns=747700 begin_steady_ns=6334500399900 end_steady_ns=6334501147600 native_result=0 thread_cpu_ns=0
[2026-11-02 10:34:54.481] [info] [graph][runtime.node] cuda_canvas_call stage=batch_completion count=6260 operation=copy maximum_ns=20548600 begin_steady_ns=6319100257500 end_steady_ns=6319120806100 native_result=0 thread_cpu_ns=0
[2026-11-02 10:34:54.481] [info] [graph][runtime.node] canvas_compose_correlated scope=attempt begin_steady_ns=6319100122800 end_steady_ns=6319120827400 wall_ns=20704600 thread_cpu_ns=0 surface_count=1 surface_total_ns=800 surface_maximum_ns=800 surface_maximum_tile=none surface_maximum_result=unavailable validate_count=1 validate_total_ns=700 validate_maximum_ns=700 validate_maximum_tile=2 validate_maximum_result=0 black_count=1 black_total_ns=104900 black_maximum_ns=104900 black_maximum_tile=none black_maximum_result=0 tile_count=1 tile_total_ns=21700 tile_maximum_ns=21700 tile_maximum_tile=2 tile_maximum_result=0 publish_count=1 publish_total_ns=11100 publish_maximum_ns=11100 publish_maximum_tile=none publish_maximum_result=0 context_push_count=1 context_push_total_ns=2400 context_push_maximum_ns=2400 context_push_maximum_tile=none context_push_maximum_result=0 source_wait_count=2 source_wait_total_ns=2100 source_wait_maximum_ns=2100 source_wait_maximum_tile=2 source_wait_maximum_result=0 plane_copy_count=4 plane_copy_total_ns=114600 plane_copy_maximum_ns=87100 plane_copy_maximum_tile=none plane_copy_maximum_result=0 destination_wait_count=0 destination_wait_total_ns=0 destination_wait_maximum_ns=0 destination_wait_maximum_tile=none destination_wait_maximum_result=unavailable context_pop_count=1 context_pop_total_ns=6100 context_pop_maximum_ns=6100 context_pop_maximum_tile=none context_pop_maximum_result=0 batch_completion_count=1 batch_completion_total_ns=20546300 batch_completion_maximum_ns=20546300 batch_completion_maximum_tile=none batch_completion_maximum_result=0 source_query_completed_count=0 source_query_ready_count=1 source_query_ready_maximum_ns=5200 source_query_ready_begin_steady_ns=6319100235600 source_query_ready_end_steady_ns=6319100240800 source_query_ready_tile=2 source_query_ready_operation=event_query source_query_ready_native=0 source_query_ready_restore_native=0 source_query_pending_count=0 source_query_unavailable_count=0 source_query_error_count=0
[2026-11-02 10:34:54.481] [info] [graph][runtime.lifecycle] node_abort_payload node=6 before_objects=46 after_objects=32 before_bytes=226319 after_bytes=226319 before_reservations=201377 after_reservations=201377 before_releases=201331 after_releases=201345
[2026-11-02 10:34:54.481] [info] [graph][runtime.node] source_video_trace_summary node_id=67 node=VideoPreparationNode generation=2 reason=abort steady_ns=6370198313900 input_count=2855 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=2855 published_count=2855 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=9 incomplete=0
[2026-11-02 10:34:54.481] [info] [graph][runtime.lifecycle] node_abort_payload node=67 before_objects=32 after_objects=32 before_bytes=226319 after_bytes=226319 before_reservations=201377 after_reservations=201377 before_releases=201345 after_releases=201345
[2026-11-02 10:34:54.482] [info] [graph][runtime.node] source_video_trace_summary node_id=44 node=VideoPreparationNode generation=1 reason=abort steady_ns=6370198593600 input_count=3432 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3432 published_count=3431 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 10:34:54.482] [info] [graph][runtime.lifecycle] node_abort_payload node=44 before_objects=32 after_objects=32 before_bytes=226319 after_bytes=226319 before_reservations=201377 after_reservations=201377 before_releases=201345 after_releases=201345
[2026-11-02 10:34:54.482] [info] [graph][runtime.node] source_video_trace_summary node_id=25 node=VideoPreparationNode generation=1 reason=abort steady_ns=6370198852200 input_count=3431 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=3431 published_count=3430 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 10:34:54.482] [info] [graph][runtime.lifecycle] node_abort_payload node=25 before_objects=32 after_objects=32 before_bytes=226319 after_bytes=226319 before_reservations=201377 after_reservations=201377 before_releases=201345 after_releases=201345
[2026-11-02 10:34:54.482] [info] [graph][runtime.lifecycle] codec_owner_release node=73 kind=AudioResample owner_references=1
[2026-11-02 10:34:54.483] [info] [graph][runtime.lifecycle] node_abort_payload node=73 before_objects=32 after_objects=30 before_bytes=226319 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201345 after_releases=201347
[2026-11-02 10:34:54.483] [info] [graph][runtime.node] source_video_trace_summary node_id=68 node=VideoFilterNode generation=2 reason=abort steady_ns=6370199945600 input_count=2855 accepted_count=2855 would_block_count=0 resolved_count=2855 output_attempt_count=0 published_count=2855 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:34:54.490] [info] [graph][runtime.lifecycle] node_abort_payload node=68 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.490] [info] [graph][runtime.node] source_video_trace_summary node_id=45 node=VideoFilterNode generation=1 reason=abort steady_ns=6370207132700 input_count=3439 accepted_count=3439 would_block_count=0 resolved_count=3439 output_attempt_count=0 published_count=3438 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 10:34:54.495] [info] [graph][runtime.lifecycle] node_abort_payload node=45 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.495] [info] [graph][runtime.node] source_video_trace_summary node_id=26 node=VideoFilterNode generation=1 reason=abort steady_ns=6370212243800 input_count=3438 accepted_count=3438 would_block_count=0 resolved_count=3438 output_attempt_count=0 published_count=3437 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=6 incomplete=0
[2026-11-02 10:34:54.499] [info] [graph][runtime.lifecycle] node_abort_payload node=26 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.499] [info] [graph][runtime.lifecycle] node_abort_payload node=72 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.499] [info] [graph][runtime.node] source_video_trace_summary node_id=66 node=VideoFrameRateNode generation=2 reason=abort steady_ns=6370216090700 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=2855 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:34:54.499] [info] [graph][runtime.lifecycle] node_abort_payload node=66 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.499] [info] [graph][runtime.node] source_video_trace_summary node_id=43 node=VideoFrameRateNode generation=1 reason=abort steady_ns=6370216290000 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3445 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:34:54.499] [info] [graph][runtime.lifecycle] node_abort_payload node=43 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.499] [info] [graph][runtime.node] source_video_trace_summary node_id=24 node=VideoFrameRateNode generation=1 reason=abort steady_ns=6370216451200 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3444 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:34:54.499] [info] [graph][runtime.lifecycle] node_abort_payload node=24 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.500] [info] [graph][runtime.lifecycle] node_abort_payload node=71 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.500] [info] [graph][runtime.node] source_video_trace_summary node_id=65 node=HardwareTransferNode generation=2 reason=abort steady_ns=6370216670400 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=2855 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:34:54.500] [info] [graph][runtime.lifecycle] node_abort_payload node=65 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.500] [info] [graph][runtime.node] source_video_trace_summary node_id=42 node=HardwareTransferNode generation=1 reason=abort steady_ns=6370216815300 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3459 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:34:54.500] [info] [graph][runtime.lifecycle] node_abort_payload node=42 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.500] [info] [graph][runtime.node] source_video_trace_summary node_id=23 node=HardwareTransferNode generation=1 reason=abort steady_ns=6370216942200 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=3458 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:34:54.500] [info] [graph][runtime.lifecycle] node_abort_payload node=23 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.500] [info] [graph][runtime.lifecycle] codec_owner_release node=70 kind=AudioDecode owner_references=2
[2026-11-02 10:34:54.500] [info] [graph][runtime.lifecycle] node_abort_payload node=70 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.500] [info] [graph][runtime.node] source_video_trace_summary node_id=64 node=VideoDecodeNode generation=2 reason=abort steady_ns=6370217320200 input_count=2856 accepted_count=2856 would_block_count=0 resolved_count=2855 output_attempt_count=0 published_count=2855 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=109 incomplete=0
[2026-11-02 10:34:54.500] [info] [graph][runtime.lifecycle] codec_owner_release node=64 kind=VideoDecode owner_references=2
[2026-11-02 10:34:54.507] [info] [graph][runtime.lifecycle] node_abort_payload node=64 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.508] [info] [graph][runtime.node] source_video_trace_summary node_id=41 node=VideoDecodeNode generation=1 reason=abort steady_ns=6370224715300 input_count=3460 accepted_count=3460 would_block_count=0 resolved_count=3460 output_attempt_count=0 published_count=3459 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=109 incomplete=0
[2026-11-02 10:34:54.508] [info] [graph][runtime.lifecycle] codec_owner_release node=41 kind=VideoDecode owner_references=2
[2026-11-02 10:34:54.517] [info] [graph][runtime.lifecycle] node_abort_payload node=41 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.517] [info] [graph][runtime.node] source_video_trace_summary node_id=22 node=VideoDecodeNode generation=1 reason=abort steady_ns=6370234443600 input_count=3459 accepted_count=3459 would_block_count=0 resolved_count=3459 output_attempt_count=0 published_count=3458 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=110 incomplete=0
[2026-11-02 10:34:54.518] [info] [graph][runtime.lifecycle] codec_owner_release node=22 kind=VideoDecode owner_references=2
[2026-11-02 10:34:54.528] [info] [graph][runtime.lifecycle] node_abort_payload node=22 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.528] [info] [graph][runtime.node] source_video_trace_summary node_id=62 node=MediaAvBoundReleaseExtractorNode generation=2 reason=abort steady_ns=6370244981600 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=2841 release_committed_count=2841 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:34:54.528] [info] [graph][runtime.lifecycle] node_abort_payload node=62 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.528] [info] [graph][runtime.node] source_video_trace_summary node_id=39 node=MediaAvBoundReleaseExtractorNode generation=1 reason=abort steady_ns=6370245131100 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=3571 release_committed_count=3555 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:34:54.528] [info] [graph][runtime.lifecycle] node_abort_payload node=39 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.528] [info] [graph][runtime.node] source_video_trace_summary node_id=20 node=MediaAvBoundReleaseExtractorNode generation=1 reason=abort steady_ns=6370245254600 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=15 release_count=3571 release_committed_count=3555 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:34:54.528] [info] [graph][runtime.lifecycle] node_abort_payload node=20 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.528] [info] [graph][runtime.node] source_video_trace_summary node_id=61 node=ActivationReleaseSequencer generation=2 reason=abort steady_ns=6370245376800 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=2841 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:34:54.528] [info] [graph][runtime.lifecycle] node_abort_payload node=61 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.528] [info] [graph][runtime.node] source_video_trace_summary node_id=38 node=ActivationReleaseSequencer generation=1 reason=abort steady_ns=6370245499900 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3555 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:34:54.528] [info] [graph][runtime.lifecycle] node_abort_payload node=38 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.529] [info] [graph][runtime.node] source_video_trace_summary node_id=19 node=ActivationReleaseSequencer generation=1 reason=abort steady_ns=6370245630500 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=3555 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 10:34:54.529] [info] [graph][runtime.lifecycle] node_abort_payload node=19 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.529] [info] [graph][runtime.lifecycle] node_abort_payload node=60 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.529] [info] [graph][runtime.lifecycle] node_abort_payload node=37 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.529] [info] [graph][runtime.lifecycle] node_abort_payload node=18 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.529] [info] [graph][runtime.lifecycle] node_abort_payload node=58 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.529] [info] [graph][runtime.lifecycle] node_abort_payload node=35 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.529] [info] [graph][runtime.lifecycle] node_abort_payload node=16 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.529] [info] [graph][runtime.lifecycle] node_abort_payload node=57 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.529] [info] [graph][runtime.lifecycle] node_abort_payload node=56 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.529] [info] [graph][runtime.lifecycle] node_abort_payload node=34 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.529] [info] [graph][runtime.lifecycle] node_abort_payload node=15 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.529] [info] [graph][runtime.lifecycle] node_abort_payload node=59 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.529] [info] [graph][runtime.lifecycle] node_abort_payload node=55 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.529] [info] [graph][runtime.lifecycle] node_abort_payload node=54 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.529] [info] [graph][runtime.lifecycle] node_abort_payload node=36 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.530] [info] [graph][runtime.lifecycle] node_abort_payload node=33 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.530] [info] [graph][runtime.lifecycle] node_abort_payload node=17 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.530] [info] [graph][runtime.lifecycle] node_abort_payload node=14 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.530] [info] [graph][runtime.lifecycle] node_abort_payload node=53 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.530] [info] [graph][runtime.lifecycle] node_abort_payload node=32 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.530] [info] [graph][runtime.lifecycle] node_abort_payload node=13 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.530] [info] [graph][runtime.lifecycle] node_abort_payload node=52 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.530] [info] [graph][runtime.lifecycle] node_abort_payload node=51 before_objects=30 after_objects=30 before_bytes=209935 after_bytes=209935 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201347
[2026-11-02 10:34:54.530] [info] [graph][runtime.lifecycle] node_abort_payload node=50 before_objects=30 after_objects=29 before_bytes=209935 after_bytes=180522 before_reservations=201377 after_reservations=201377 before_releases=201347 after_releases=201348
[2026-11-02 10:34:54.530] [info] [graph][runtime.lifecycle] node_abort_payload node=31 before_objects=29 after_objects=29 before_bytes=180522 after_bytes=180522 before_reservations=201377 after_reservations=201377 before_releases=201348 after_releases=201348
[2026-11-02 10:34:54.530] [info] [graph][runtime.lifecycle] node_abort_payload node=30 before_objects=29 after_objects=29 before_bytes=180522 after_bytes=180522 before_reservations=201377 after_reservations=201377 before_releases=201348 after_releases=201348
[2026-11-02 10:34:54.530] [info] [graph][runtime.lifecycle] node_abort_payload node=12 before_objects=29 after_objects=29 before_bytes=180522 after_bytes=180522 before_reservations=201377 after_reservations=201377 before_releases=201348 after_releases=201348
[2026-11-02 10:34:54.530] [info] [graph][runtime.lifecycle] node_abort_payload node=11 before_objects=29 after_objects=29 before_bytes=180522 after_bytes=180522 before_reservations=201377 after_reservations=201377 before_releases=201348 after_releases=201348
[2026-11-02 10:34:54.530] [info] [graph][runtime.lifecycle] node_abort_payload node=49 before_objects=29 after_objects=29 before_bytes=180522 after_bytes=180522 before_reservations=201377 after_reservations=201377 before_releases=201348 after_releases=201348
[2026-11-02 10:34:54.530] [info] [graph][runtime.lifecycle] node_abort_payload node=29 before_objects=29 after_objects=29 before_bytes=180522 after_bytes=180522 before_reservations=201377 after_reservations=201377 before_releases=201348 after_releases=201348
[2026-11-02 10:34:54.530] [info] [graph][runtime.lifecycle] node_abort_payload node=10 before_objects=29 after_objects=29 before_bytes=180522 after_bytes=180522 before_reservations=201377 after_reservations=201377 before_releases=201348 after_releases=201348
[2026-11-02 10:34:54.531] [info] [graph][runtime.lifecycle] node_abort_payload node=69 before_objects=29 after_objects=29 before_bytes=180522 after_bytes=180522 before_reservations=201377 after_reservations=201377 before_releases=201348 after_releases=201348
[2026-11-02 10:34:54.531] [info] [graph][runtime.lifecycle] node_abort_payload node=48 before_objects=29 after_objects=29 before_bytes=180522 after_bytes=180522 before_reservations=201377 after_reservations=201377 before_releases=201348 after_releases=201348
[2026-11-02 10:34:54.531] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-11-02 10:34:54.531] [info] [graph][runtime.lifecycle] node_abort_payload node=63 before_objects=29 after_objects=29 before_bytes=180522 after_bytes=180522 before_reservations=201377 after_reservations=201377 before_releases=201348 after_releases=201348
[2026-11-02 10:34:54.531] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-11-02 10:34:54.531] [info] [graph][runtime.lifecycle] node_abort_payload node=40 before_objects=29 after_objects=29 before_bytes=180522 after_bytes=180522 before_reservations=201377 after_reservations=201377 before_releases=201348 after_releases=201348
[2026-11-02 10:34:54.531] [info] [graph][runtime.lifecycle] node_abort_payload node=28 before_objects=29 after_objects=29 before_bytes=180522 after_bytes=180522 before_reservations=201377 after_reservations=201377 before_releases=201348 after_releases=201348
[2026-11-02 10:34:54.531] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-11-02 10:34:54.531] [info] [graph][runtime.lifecycle] node_abort_payload node=21 before_objects=29 after_objects=29 before_bytes=180522 after_bytes=180522 before_reservations=201377 after_reservations=201377 before_releases=201348 after_releases=201348
[2026-11-02 10:34:54.531] [info] [graph][runtime.lifecycle] node_abort_payload node=9 before_objects=29 after_objects=29 before_bytes=180522 after_bytes=180522 before_reservations=201377 after_reservations=201377 before_releases=201348 after_releases=201348
[2026-11-02 10:34:54.531] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=9321 completion_waits=9321 completed_datagrams=9422 drained_datagrams=123 would_block=24 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-02 10:34:54.532] [info] [graph][runtime.lifecycle] node_abort_payload node=47 before_objects=29 after_objects=29 before_bytes=180522 after_bytes=180522 before_reservations=201377 after_reservations=201377 before_releases=201348 after_releases=201348
[2026-11-02 10:34:54.532] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=130384 completion_waits=130384 completed_datagrams=191108 drained_datagrams=60746 would_block=24 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-02 10:34:54.533] [info] [graph][runtime.lifecycle] node_abort_payload node=46 before_objects=29 after_objects=29 before_bytes=180522 after_bytes=180522 before_reservations=201377 after_reservations=201377 before_releases=201348 after_releases=201348
[2026-11-02 10:34:54.533] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=73879 completion_waits=73879 completed_datagrams=105379 drained_datagrams=31571 would_block=71 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-02 10:34:54.533] [info] [graph][runtime.lifecycle] node_abort_payload node=27 before_objects=29 after_objects=29 before_bytes=180522 after_bytes=180522 before_reservations=201377 after_reservations=201377 before_releases=201348 after_releases=201348
[2026-11-02 10:34:54.533] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=76558 completion_waits=76558 completed_datagrams=105414 drained_datagrams=28927 would_block=71 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-02 10:34:54.534] [info] [graph][runtime.lifecycle] node_abort_payload node=8 before_objects=29 after_objects=29 before_bytes=180522 after_bytes=180522 before_reservations=201377 after_reservations=201377 before_releases=201348 after_releases=201348
[2026-11-02 10:34:54.534] [info] [graph][runtime.lifecycle] node_abort_payload node=3 before_objects=29 after_objects=29 before_bytes=180522 after_bytes=180522 before_reservations=201377 after_reservations=201377 before_releases=201348 after_releases=201348
[2026-11-02 10:34:54.534] [info] [graph][runtime.node] codec_resolver.release slot=encoder_parameters bufferReferences=1 hasCredit=0 creditReferences=0
[2026-11-02 10:34:54.534] [info] [graph][runtime.lifecycle] node_abort_payload node=1 before_objects=29 after_objects=29 before_bytes=180522 after_bytes=180522 before_reservations=201377 after_reservations=201377 before_releases=201348 after_releases=201348
[2026-11-02 10:34:54.610] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=failed detail="Datagram GBRA reservation misses its immutable completion deadline sequence=155923 now_ns=263428900000 release_ns=263362646001 deadline_ns=263429283264 not_before_ns=263429099540 not_after_ns=263429068224 duration_ns=215040 rate=6250000 queue_wire_bytes=4624 average_queue_residence_ns=62658100" description="" error=6 native=0 stage=3 reason="Datagram GBRA reservation misses its immutable completion deadline sequence=155923 now_ns=263428900000 release_ns=263362646001 deadline_ns=263429283264 not_before_ns=263429099540 not_after_ns=263429068224 duration_ns=215040 rate=6250000 queue_wire_bytes=4624 average_queue_residence_ns=62658100"
[2026-11-02 10:34:54.611] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=retired detail="Datagram GBRA reservation misses its immutable completion deadline sequence=155923 now_ns=263428900000 release_ns=263362646001 deadline_ns=263429283264 not_before_ns=263429099540 not_after_ns=263429068224 duration_ns=215040 rate=6250000 queue_wire_bytes=4624 average_queue_residence_ns=62658100" description="" error=6 native=0 stage=3 reason="Datagram GBRA reservation misses its immutable completion deadline sequence=155923 now_ns=263428900000 release_ns=263362646001 deadline_ns=263429283264 not_before_ns=263429099540 not_after_ns=263429068224 duration_ns=215040 rate=6250000 queue_wire_bytes=4624 average_queue_residence_ns=62658100"
[CLI] final runtime report: queued=0, peakQueued=933, threads=81, processThreads=129, workers=0, workerProgress=518704, workerProcessCalls=1993626, workerWaits=1474913, workerWakeups=1455268, workerErrors=2, errors=2, stalledIntervals=0, cpuSamples=817, logicalProcessors=22, averageSystemMachineCpuPercent=8.777626, averageProcessMachineCpuPercent=1.673312, peakProcessMachineCpuPercent=5.615551, averageProcessSingleCoreCpuPercent=36.812855, peakProcessSingleCoreCpuPercent=123.542117, initialWorkingSetBytes=208199680, workingSetBytes=259301376, peakWorkingSetBytes=268324864, totalPushed=446126, totalPopped=446078, droppedBuffers=0, encodedPacketsPushed=86457, encodedPacketsPopped=86452, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=16114003, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=750, graphPayloadReservations=201377, graphPayloadReleases=201377, graphPayloadPressureFailures=1, lastInputReceivedAtNs=6370131853700
~~~

## 完整stderr

~~~text
[in @ 000001a0840c1880] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001a0840c1880] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 6389.895978
[in @ 000001a084803b40] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001a084803b40] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 23690.973933
[in @ 000001a0849af400] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001a0849af400] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 25104.250422
[in @ 000001a08ab27c80] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001a08ab27c80] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 6389.895978
[in @ 000001a0f03ae500] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001a08ab24ac0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001a08ab24ac0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 25104.250422
[in @ 000001a0f03ae500] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 23690.973933
[aac @ 000001a084e9c200] Qavg: 12995.059
[aac @ 000001a084e9c200] 2 frames left in the queue on closing
[CLI] realtime video runtime failed: IoFailure: Datagram GBRA reservation misses its immutable completion deadline sequence=155923 now_ns=263428900000 release_ns=263362646001 deadline_ns=263429283264 not_before_ns=263429099540 not_after_ns=263429068224 duration_ns=215040 rate=6250000 queue_wire_bytes=4624 average_queue_residence_ns=62658100

~~~

## source0 完成日志

~~~text
frame= 3574 fps= 30 q=-1.0 size=  118047KiB time=00:01:59.14 bitrate=8116.8kbits/s speed=   1x elapsed=0:01:58.64    
frame= 3590 fps= 30 q=-1.0 size=  118545KiB time=00:01:59.66 bitrate=8115.2kbits/s speed=   1x elapsed=0:01:59.15    
[out#0/rtp @ 000001cf4e68a3c0] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
[out#1/rtp @ 000001cf50292840] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.49    
~~~

## source1 完成日志

~~~text
frame= 3559 fps= 30 q=-1.0 size=  117543KiB time=00:01:58.63 bitrate=8116.7kbits/s speed=   1x elapsed=0:01:58.12    
frame= 3574 fps= 30 q=-1.0 size=  118047KiB time=00:01:59.13 bitrate=8117.3kbits/s speed=   1x elapsed=0:01:58.64    
frame= 3590 fps= 30 q=-1.0 size=  118545KiB time=00:01:59.66 bitrate=8115.2kbits/s speed=   1x elapsed=0:01:59.15    
[out#0/rtp @ 0000024e6d8f7fc0] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
~~~

## source2 完成日志

~~~text
frame= 3559 fps= 30 q=-1.0 size=  117543KiB time=00:01:58.63 bitrate=8116.7kbits/s speed=   1x elapsed=0:01:58.11    
frame= 3574 fps= 30 q=-1.0 size=  118047KiB time=00:01:59.13 bitrate=8117.3kbits/s speed=   1x elapsed=0:01:58.64    
frame= 3590 fps= 30 q=-1.0 size=  118545KiB time=00:01:59.66 bitrate=8115.2kbits/s speed=   1x elapsed=0:01:59.15    
[out#0/rtp @ 00000203ef205c80] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.49    
~~~

## reentry 完成日志

~~~text
frame= 3574 fps= 30 q=-1.0 size=  118047KiB time=00:01:59.13 bitrate=8117.3kbits/s speed=   1x elapsed=0:01:58.62    
frame= 3589 fps= 30 q=-1.0 size=  118528KiB time=00:01:59.63 bitrate=8116.3kbits/s speed=   1x elapsed=0:01:59.13    
[out#0/rtp @ 000001a86b327500] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
[out#1/rtp @ 000001a86cf1ea00] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.49    
~~~

## CPU与内存逐秒采样

列：elapsed_seconds,cpu_machine_percent,working_set_bytes,private_bytes。

~~~csv
1.0059039,0.988482281277746,142639104,568561664
2.0320326,0.207642746780381,145129472,570908672
3.0563948,0.13866721609354,149024768,574296064
4.084203,0.207303446127577,152219648,577445888
5.1118964,1.1057418840713,192937984,689901568
6.1470216,3.43063463592265,252620800,1254965248
7.1903889,2.38247399026733,253108224,1255538688
8.2173222,2.28228065055442,254935040,1256914944
9.2486085,1.99717487850764,257085440,1258811392
10.2760927,1.93544228089966,257122304,1258815488
11.3001525,2.01126837603536,257343488,1258815488
12.3283502,2.76299887746208,258056192,1259487232
13.3659923,1.77960291808795,258113536,1259487232
14.3974029,1.51491559229661,258920448,1260285952
15.4353213,2.05284135841682,258125824,1259491328
16.4606569,2.14730137669515,259088384,1259999232
17.4938943,2.54330796493711,261615616,1262444544
18.5241116,2.68864283645437,261615616,1262444544
19.5656461,2.72761880754704,261672960,1262444544
20.587248,2.0161073417239,261738496,1262456832
21.6221456,2.33334460073415,261746688,1262465024
22.6557018,3.22969199141584,261840896,1262469120
23.6809926,2.00885357686726,262647808,1263263744
24.7049827,2.42755809313533,262033408,1262469120
25.7522329,2.64491366402829,262090752,1262469120
26.7812271,2.27770963140511,262090752,1262469120
27.8094828,2.07213227038937,262115328,1262501888
28.8410778,1.65233977922097,262119424,1262501888
29.8662656,1.87049985999018,262115328,1262501888
30.8948129,1.58818435211752,262144000,1262501888
31.9194366,2.35674104285576,262152192,1262501888
32.9444272,1.87085972921472,263213056,1263489024
33.9730549,2.27852117923715,264654848,1265119232
35.0026095,1.72459836692312,264794112,1265119232
36.0432839,2.11565168265362,264847360,1265119232
37.0800252,1.30160901102504,264847360,1265119232
38.1178015,1.71093537385483,264204288,1264230400
39.1529833,2.53853082530131,263467008,1263435776
40.1788167,1.66162015639719,263467008,1263435776
41.2119692,1.5123614374451,263585792,1263435776
42.2450172,1.78751704576255,264765440,1264599040
43.2694758,2.35712085122105,264826880,1264599040
44.2942287,2.56436542808605,264892416,1264599040
45.3237312,1.93164792085144,262389760,1261973504
46.3458168,1.7372010542152,263184384,1262768128
47.3766479,1.99805680184571,262512640,1261973504
48.4121878,1.30311909582801,262512640,1261973504
49.4454196,1.44350693883722,263499776,1263165440
50.4741684,1.72594921308115,264650752,1263714304
51.4991899,1.80151431856885,264765440,1263714304
52.5274464,2.00305963629608,264765440,1263714304
53.5549969,2.3500282733284,264937472,1263714304
54.5788586,2.7747000311752,264957952,1263714304
55.5993657,1.94867469676238,264962048,1263718400
56.6330836,2.47341966489908,264962048,1263718400
57.6646334,1.51471116566549,264966144,1263722496
58.6981766,2.33640231707077,264970240,1263722496
59.7267553,2.2095803390905,264970240,1263722496
60.7546789,1.58914799433803,264900608,1263611904
61.7820181,1.93571545175767,264900608,1263611904
62.8152545,2.33709606753375,264957952,1263611904
63.8435049,2.27935724605602,265015296,1263611904
64.8718695,2.00284907795259,265015296,1263611904
65.9044412,2.2010357951194,265408512,1264005120
66.9418886,2.32760979233522,266944512,1265586176
67.9669827,2.49422778047222,267722752,1266372608
68.9871203,2.57596711374124,266940416,1265586176
70.016008,2.27794539676196,266940416,1265586176
71.0385138,1.5975681773861,265826304,1264472064
72.0630774,1.87163943396354,265830400,1264472064
73.083857,2.4351931156789,265830400,1264472064
74.1036878,1.88032528176598,266100736,1264472064
75.1339909,1.99908074712101,266682368,1265045504
76.1690005,2.05861068166114,266682368,1265045504
77.1993368,2.82619550352813,266686464,1265045504
78.2281667,2.27807337247876,266682368,1265045504
79.2555925,1.65904482303778,266686464,1265045504
80.2942083,1.84631664217284,266682368,1265045504
81.3301823,1.4396860082659,266690560,1265045504
82.3574342,1.93587995664585,266682368,1265045504
83.3824804,1.31645951000239,268075008,1266667520
84.4083379,1.59234857402,268070912,1266667520
85.4332586,2.07887480288165,268079104,1266667520
86.4620478,1.72588143598144,268075008,1266667520
87.4850068,2.91600596451524,265891840,1264250880
88.5146464,1.37956479670608,265891840,1264250880
89.5450671,2.41240830521502,265924608,1264439296
90.568239,1.94359947105305,267051008,1265729536
91.6020032,3.02293308280556,267292672,1265729536
92.6439834,1.97667776307947,267460608,1265729536
93.6839659,2.18535145805555,267464704,1265729536
94.7176858,1.58023728407738,266354688,1264599040
95.7477933,1.93051343052678,266424320,1264668672
96.7895278,3.34069154507566,266375168,1264611328
97.8108071,1.5299438655028,266391552,1264611328
98.8450497,1.30475366048722,266391552,1264611328
99.8721522,2.55849918493131,268324864,1266552832
100.9108004,1.77787908272591,268324864,1266552832
101.9454228,1.37292073461249,268324864,1266552832
102.9689772,2.4979797671899,268324864,1266552832
103.9890853,2.22792787619985,266100736,1264324608
105.0162215,2.6275615992929,266100736,1264324608
106.0498914,1.64902301454792,266100736,1264324608
107.0753036,1.66230268622264,266289152,1264324608
108.0990597,1.8731157121932,267460608,1265442816
109.1306942,2.27188020563485,267526144,1265442816
110.1610153,1.72331536432493,267579392,1265446912
111.1890955,2.00340313032885,266547200,1264349184
112.2142004,1.80136775181828,266547200,1264349184
113.245366,1.58415168938211,266555392,1264357376
114.2772646,1.44537193162902,266555392,1264357376
115.3076105,2.3436524833774,266559488,1264361472
116.33848,1.9290864300829,266567680,1264553984
117.3715706,1.30620859214267,267309056,1265192960
118.4146989,2.58727870422423,267370496,1265385472
119.443139,2.62423026519837,256413696,1254625280
120.4786231,1.23460040662054,243576832,1252646912
121.5094277,1.0335042248462,243585024,1252646912
122.5302772,0.974010548879325,243576832,1252646912
123.5651877,1.02940390409694,243576832,1252646912
124.5911871,0.623006743916754,243576832,1252646912
125.6168752,1.03865971447939,243576832,1252646912
126.6537596,1.09594052756858,243576832,1252663296
127.6852009,0.826293001136109,243576832,1252663296
128.7154481,0.896188268743123,243585024,1252663296
129.7472334,1.10135668376322,243576832,1252663296
130.7783848,1.03315663353695,243576832,1252663296
131.8029018,0.762554452488341,243576832,1252663296
132.848511,1.22264522051746,243576832,1252663296
133.8823353,0.686990306503028,243576832,1252663296
134.9148704,0.962987293912008,243576832,1252663296
135.9368089,0.764478488676196,243576832,1252663296
136.9741407,0.821600887269362,243576832,1252663296
138.0064183,0.550415719746132,243576832,1252663296
139.0400046,0.687148497157204,243576832,1252663296
140.0708612,1.10234889737685,243576832,1252663296
141.1044488,0.755862396181994,243576832,1252663296
142.1335817,0.828146420421231,243576832,1252663296
143.1724052,0.957157959767182,243576832,1252663296
144.2038416,0.964013080998658,243576832,1252663296
145.229895,0.899851269481159,243576832,1252663296
146.262524,0.82534262283232,243576832,1252663296
147.2902776,0.967467476463423,243576832,1252663296
148.3298306,1.16144762569715,243576832,1252663296
149.3550811,1.17765010954529,243576832,1252663296
150.3842811,1.38015404727414,243576832,1252663296
151.4173057,1.10003540705968,243576832,1252663296
152.4388318,0.973365420441206,243576832,1252663296
153.4703208,1.1705276194282,243576832,1252663296
154.4935452,1.38821410577635,243576832,1252663296
155.5195186,0.899921435141941,243576832,1252663296
156.5556967,0.822515673003248,243576832,1252663296
157.5941673,1.09426654578726,243576832,1252663296
158.6336949,0.888187533015446,243576832,1252683776
159.6571369,0.902147317137111,243576832,1252683776
160.6810159,0.971128601932638,243580928,1252683776
161.7125842,0.963889818849783,243576832,1252683776
162.737851,0.831269214289129,243576832,1252683776
163.7642994,1.03789036944372,243576832,1252683776
164.7954658,0.964265497613375,243576832,1252683776
165.8203093,0.762311513904316,243576832,1252683776
166.8554294,1.23503455387359,243576832,1252683776
167.8811482,1.03862862715482,245960704,1254330368
168.9099331,1.31167537371694,250556416,1257590784
169.9402685,1.93008642004957,251650048,1258262528
170.9613686,1.2519919358632,252604416,1258811392
171.9893271,1.72727613207942,253288448,1259081728
173.0247019,1.71490380277575,253861888,1259204608
174.0448099,1.81019157686335,254099456,1259290624
175.0677468,2.08290640232235,254267392,1259405312
176.0966918,1.7946449121099,254906368,1259433984
177.1337957,0.958745003097739,255918080,1260191744
178.1722132,1.29950797071683,255926272,1260191744
179.1944489,1.32007893891967,255971328,1260191744
180.2267347,1.16962411343482,256061440,1260257280
181.2629649,1.85056721601405,256311296,1260503040
182.2876244,1.38626982471204,256634880,1260662784
183.3183228,1.10251809487977,256860160,1260732416
184.3522346,1.64863719956137,257171456,1260937216
185.3813774,1.72528844570278,257404928,1261064192
186.4029122,1.5295612053549,257445888,1261076480
187.4301733,1.382759013706,257572864,1261076480
188.4513272,1.04327164503892,257597440,1261101056
189.4861373,1.09813736487849,257622016,1261170688
190.5121831,1.66127618722816,257622016,1261170688
191.5437884,1.44578287134356,257720320,1261170688
192.5734626,1.37951843938069,257724416,1261170688
193.5991587,1.73108602228104,257822720,1261170688
194.6332586,1.09889154458253,257585152,1260150784
195.6573679,1.17896240531784,257687552,1260150784
196.6877822,2.13671777017707,257687552,1260150784
197.7061281,1.88306707609233,257851392,1260150784
198.7392474,1.71864777070583,257945600,1260154880
199.7733867,1.23620588726209,257945600,1260154880
200.8040177,1.99844473037304,258031616,1260154880
201.8289851,1.52443872849028,258301952,1260285952
202.8654673,1.71307156246213,258301952,1260285952
203.8900133,1.52506573643352,258301952,1260285952
204.9238297,1.78618844612148,258326528,1260285952
205.9490886,2.42455389028608,258326528,1260285952
206.9757743,1.17600387697653,258347008,1260318720
208.0038199,1.24353344920606,258498560,1260470272
209.0305749,1.59095668126546,259276800,1261248512
210.0539065,2.29031332561213,258863104,1260470272
211.0825438,1.03568177927334,258875392,1260482560
212.1163015,1.71758641489992,258875392,1260572672
213.1549793,1.91458444922606,258875392,1260572672
214.1780779,1.73548100038275,258965504,1260572672
215.2005597,1.45868344329186,259022848,1260572672
216.2266835,1.52272074773045,259043328,1260589056
217.2600698,1.64947556837697,259047424,1260589056
218.2843356,1.38680266924323,259162112,1260589056
219.3150487,1.79156635254843,259162112,1260589056
220.3398633,1.94048402865884,259162112,1260589056
221.3682155,2.07193782264658,259162112,1260589056
222.3936844,1.38517564545791,259162112,1260589056
223.4274071,1.78635035207306,259162112,1260589056
224.457404,1.44804054529416,259166208,1260589056
225.4815645,1.73368156828758,259170304,1260589056
226.5052239,1.31824298021568,259170304,1260589056
227.5389131,1.51157620685212,259170304,1260589056
228.5740173,1.02921127079854,259248128,1260589056
229.6060384,2.27102914853199,259346432,1260589056
230.6336854,1.72779970341779,259338240,1260589056
231.6671817,1.58057917311628,259338240,1260589056
232.6968643,1.72438398183885,260108288,1261359104
233.7307135,1.51134227312839,258932736,1259442176
234.7566395,1.3153305581317,259022848,1259442176
235.7863661,1.58636547533367,259022848,1259442176
236.826001,1.50293146180455,259022848,1259442176
237.8489832,1.59682419427506,259022848,1259442176
238.8753459,1.45316784478551,259022848,1259442176
239.910091,1.37275793376994,259022848,1259442176
240.9372092,1.86698438053537,259022848,1259442176
241.9597351,1.04187180891059,259022848,1259442176
242.9821789,1.52820135444116,259039232,1259442176
244.0193549,1.50649455830063,259039232,1259442176
245.0493026,1.10332168940581,259080192,1259442176
246.0838427,1.37302995355575,259084288,1259442176
247.1156256,1.23902915129636,259084288,1259442176
248.1480296,1.37587082717088,259084288,1259442176
249.172316,1.94148468986445,259088384,1259442176
250.1987405,2.14502337527456,259084288,1259442176
251.2296718,1.51561990600151,259084288,1259442176
252.2661876,1.37041282482579,259084288,1259442176
253.3024157,1.91911063175794,259084288,1259442176
254.3276355,1.80116586617904,259084288,1259442176
255.358054,1.44744807350341,259088384,1259442176
256.3929449,1.71570566696281,259178496,1259532288
257.4257731,1.44407102045356,259178496,1259532288
258.4537896,1.58900438589533,259301376,1259532288
259.4905985,1.71253177110865,259301376,1259532288
260.5211748,1.79180416732938,259301376,1259532288
261.5422336,1.530274260405,259301376,1259532288
262.5703743,2.00328524190219,259301376,1259532288
263.5953207,1.45517587332109,74702848,310280192

~~~

## VLC原生日志

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
file debug: opening logfile `D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r376-vlc.log'
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
main debug: stream filter added to 0334bc38
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
main debug: resized to 73712384x53680776
main debug: resized to 73712384x53680776
main debug: resized to 73712128x53680776
main debug: resized to 73712128x53680776
main debug: resized to 73711872x53680776
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
main debug: Buffering 46%
direct3d11 debug: Direct3D11 resources created
direct3d11 debug: Direct3D11 device adapter successfully initialized
direct3d11 debug: Direct3D11 Open Succeeded
main debug: using vout display module "direct3d11"
direct3d11 debug: D3D11 pool succeed with 48 surfaces (1280x736) context 0x0a8f00e0
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
main debug: Received first picture
main debug: Buffering 53%
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
main warning: playback too late (78616): up-sampling
main debug: auto hiding mouse cursor
main warning: picture is too late to be displayed (missing 33 ms)
main debug: picture might be displayed late (missing 1 ms)
main debug: resampling stopped (drift: -10752 us)
main debug: accepted socket 3916 (from socket 1172)
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
main debug: Filter 'd3d11_filters' (06dabee0) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (06c9d9f8) appended to chain
main debug: using video converter module "chain"
main debug: Filter 'chain' (06c583a8) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: RV24 -> 1280x720 (1280x720) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (04792910) appended to chain
main debug: using video converter module "chain"
main debug: removing module "png"
main debug: removing module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 06dabee0 removed from chain
main debug: removing module "swscale"
main debug: Filter 06c9d9f8 removed from chain
main debug: Filter 06c583a8 removed from chain
main debug: removing module "swscale"
main debug: Filter 04792910 removed from chain
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r376-native-2026-11-02-10h31m03s759.png)
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (06ec63d8) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: YUVA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (06c583a8) appended to chain
main debug: using video converter module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 06ec63d8 removed from chain
main debug: removing module "swscale"
main debug: Filter 06c583a8 removed from chain
main error: read error: No error
main debug: accepted socket 3708 (from socket 1172)
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
main debug: Filter 'd3d11_filters' (1acceb50) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (06dcad98) appended to chain
main debug: using video converter module "chain"
main debug: Filter 'chain' (06ec63d8) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: RV24 -> 1280x720 (1280x720) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (06dd9b08) appended to chain
main debug: using video converter module "chain"
main debug: removing module "png"
main debug: removing module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 1acceb50 removed from chain
main debug: removing module "swscale"
main debug: Filter 06dcad98 removed from chain
main debug: Filter 06ec63d8 removed from chain
main debug: removing module "swscale"
main debug: Filter 06dd9b08 removed from chain
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r376-native-2026-11-02-10h33m03s732.png)
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (06ec63d8) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: YUVA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (1acceb50) appended to chain
main debug: using video converter module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 06ec63d8 removed from chain
main debug: removing module "swscale"
main debug: Filter 1acceb50 removed from chain
main debug: accepted socket 3328 (from socket 1172)
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
main debug: Filter 'd3d11_filters' (1acceb50) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (06dcad98) appended to chain
main debug: using video converter module "chain"
main debug: Filter 'chain' (06ec63d8) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: RV24 -> 1280x720 (1280x720) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (06dd9b08) appended to chain
main debug: using video converter module "chain"
main debug: removing module "png"
main debug: removing module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 1acceb50 removed from chain
main debug: removing module "swscale"
main debug: Filter 06dcad98 removed from chain
main debug: Filter 06ec63d8 removed from chain
main debug: removing module "swscale"
main debug: Filter 06dd9b08 removed from chain
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r376-native-2026-11-02-10h33m54s745.png)
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (06ec63d8) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: YUVA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (1acceb50) appended to chain
main debug: using video converter module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 06ec63d8 removed from chain
main debug: removing module "swscale"
main debug: Filter 1acceb50 removed from chain
main debug: accepted socket 3712 (from socket 1172)
main debug: exiting
main debug: exiting
main debug: no exit handler
main debug: removing all interfaces
main debug: removing module "qt"
main debug: deactivating the playlist
main debug: incoming request - stopping current input
main error: read error: No error
prefetch debug: end of stream
main debug: incoming request - stopping current input
ts debug: Can't read TS packet at 182057884
main debug: EOF reached
main warning: buffer too late (-41119249 us): dropped
main warning: picture is too late to be displayed (missing 41437 ms)
main warning: picture is too late to be displayed (missing 41404 ms)
main warning: picture is too late to be displayed (missing 41370 ms)
main warning: picture is too late to be displayed (missing 41337 ms)
main warning: picture is too late to be displayed (missing 41304 ms)
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
main debug: saving media library to file C:\Users\ROG\AppData\Roaming\vlc\ml.xspf.tmp28056
main debug: looking for playlist export module matching "export-xspf": 4 candidates
main debug: using playlist export module "export"
main debug: removing module "export"
main debug: deleting item `Media Library'
main debug: deleting item `udp://127.0.0.1:62024'
main debug: deleting item `Playlist'
main debug: removing module "memory"
-- logger module stopped --

~~~

## 本轮逐项清理清单

- composition-r376-cli-error.log (2374 bytes)
- composition-r376-cli.log (1126965 bytes)
- composition-r376-monitor.log (12926 bytes)
- composition-r376-native-2026-11-02-10h31m03s759.png (711645 bytes)
- composition-r376-native-2026-11-02-10h33m03s732.png (12152 bytes)
- composition-r376-native-2026-11-02-10h33m54s745.png (343064 bytes)
- composition-r376-reentry-stdout.log (540 bytes)
- composition-r376-reentry.log (29521 bytes)
- composition-r376-source0-stdout.log (540 bytes)
- composition-r376-source0.log (29521 bytes)
- composition-r376-source1-stdout.log (335 bytes)
- composition-r376-source1.log (28926 bytes)
- composition-r376-source2-stdout.log (335 bytes)
- composition-r376-source2.log (28926 bytes)
- composition-r376-vlc.log (37795 bytes)

上述15项已按清单清理，本轮临时产物残留0，指定120秒源保留；未开展远程本轮测试。

## 同序号等待证据

与r375不同，本轮失败时active batch恰包含seq155918最差样本：target263390151440、sample263389940600，计划等待210840ns；Windows due=-2109（100ns单位），API steady6370092872100到6370127474700，实际34.6026ms，result=1，PID33268/TID556。next clock steady6370127479400，返回后仅数微秒；同seq成功提交lateness34.406460ms。可以定位长停留在等待API区间，不能仅此区分timer到期晚或ready线程调度延迟。seq155923随后因累计可用时刻超过原not_after而失败，不放宽协议deadline。

## Windows取证能力边界

独立只读复核：同媒体环境token为Medium integrity，Performance Log Users启用、Administrators deny-only，未持有SeSystemProfilePrivilege；未启动ETW、未改权限。WaitForMultipleObjects result1对应timer，长API区间仍混合Waiting及Ready未运行时间。高分辨率timer与零tolerance不等于到期即执行。普通线程CPU/cycles采样可补线程执行量，但不能拆分Waiting/Ready；关闭该根因分支需有权限的thread lifetime/CSwitch/ReadyThread并按PID/TID/QPC关联。此限制只影响系统级时序归因，不阻塞输出域/owner/联合预算开发，WER不能替代调度时间线。

官方依据：[等待返回值](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitformultipleobjects)、[CPU状态分析](https://learn.microsoft.com/en-us/windows-hardware/test/wpt/cpu-analysis)、[SystemTraceProvider权限](https://learn.microsoft.com/en-us/windows/win32/etw/configuring-and-starting-a-systemtraceprovider-session)。
