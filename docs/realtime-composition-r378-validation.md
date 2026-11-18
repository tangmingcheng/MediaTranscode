# r378：输出域声明与源拓扑纯事实回归

日期：2026-11-02。结果FAIL：发送预约窗口不可行。

本轮版本：共用输出segment声明及真实builder消费20文件、source topology纯facts view3文件均已双独立局部PASS；Release clean-first/all-target780目标成功，realtime产物11:01:37。后续全图positions合并正在源码实施，不属于本次运行版本。完整residence/owner费用、联合solver和SourcePresentation尚未完成。

## 实际启动命令

```powershell
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r378-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r378-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r378-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r378-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r378-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r378-source2.log' -PassThru
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r378 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r378-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r378-cli-error.log' -PassThru
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r378-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r378-native- --snapshot-format=png udp://@127.0.0.1:62024' -WindowStyle Hidden -PassThru
```

PID：source0=32596、source1=15312、source2=25472、CLI=14840、VLC=19716。启动前同执行环境CIM及端口无残留；启动后UDP62024与TCP63020归VLC19716。

## 画面观察

- 已查看 composition-r378-native-2026-11-02-11h03m25s064.png：三格游戏画面，右下黑格，来自本轮VLC原生snapshot。

实际快照命令：
```powershell
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
try{$s=$c.GetStream();$s.ReadTimeout=2000;$b=[Text.Encoding]::ASCII.GetBytes('snapshot'+[char]10);$s.Write($b,0,$b.Length);$buf=[byte[]]::new(4096);try{$n=$s.Read($buf,0,$buf.Length);[Text.Encoding]::UTF8.GetString($buf,0,$n)}catch{Write-Output $_.Exception.Message}}finally{$c.Dispose()}
```

## 结果：FAIL

CLI14840退出1，首因是输出约24秒时seq17207的GBRA预约窗口不可行：not_before24593577240 > not_after24593426216，差151024ns；原协议deadline24627008668、prefix completion limit24593641256。三初始源32596/15312/25472仍各自完整120秒/3600帧退出0，这不代表CLI处理了完整源。未执行失源/重入阶段。

特别更正：11h03m25s064快照晚于CLI11:03:12终止，是VLC保留的最后画面，只证明之前曾输出三格，不是持续输出验收。输出节点声明和source纯facts调用确已建图进入运行，不将既有发送失败掩盖或记PASS。

最终workerErrors/errors=2、pressure=0，23066预约/释放匹配归零。平均进程整机CPU2.186934%、峰4.175824%；WS初始190074880、峰及最终245526528bytes。sender已提交17206datagrams、20665148payload bytes，delivery仍not_proven。

同一active batch17161..17210已提交46个packet，46次等待累计70.6551ms、累计target lateness55.115732ms，单次最大lateness3.404660ms；与r376单次34ms长等待不是同一形态。权威worst-success仍是启动seq6的7.9197ms，matches=1/sample_available=1，但不是本次失败批次，不混作其原因。需要同时保留active batch全程累计与失败原期限，不能以新max样本替代全部诊断。

VLC已按身份核验退出，全部五个本轮PID与UDP62024/TCP63020同环境确认无残留。实际退出命令：
```powershell
$v=Get-CimInstance Win32_Process -Filter "ProcessId=19716"
if(-not $v -or $v.Name -ne 'vlc.exe' -or $v.CommandLine -notlike '*composition-r378*'){throw 'VLC identity mismatch'}
if((Get-NetTCPConnection -State Listen -LocalPort 63020).OwningProcess -ne 19716){throw 'VLC port mismatch'}
$c=[Net.Sockets.TcpClient]::new('127.0.0.1',63020)
try{$s=$c.GetStream();$b=[Text.Encoding]::ASCII.GetBytes('quit'+[char]10);$s.Write($b,0,$b.Length)}finally{$c.Dispose()}
```

## 非周期CLI诊断

~~~text
[CLI] composition_inputs=3 grid=fixed audio_source=2 audio=on max_duration=source_driven hw=planner-highest-score
[2026-11-02 11:02:47.191] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-11-02 11:02:47.191] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-11-02 11:02:47.195] [info] [graph][planner.capability] composition_protocol_installed source=1
[2026-11-02 11:02:47.195] [info] [graph][planner.capability] composition_protocol_installed source=0
[2026-11-02 11:02:47.196] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-11-02 11:02:47.199] [info] [graph][planner.capability] composition_protocol_installed source=2
[2026-11-02 11:02:47.298] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 11:02:47.299] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 11:02:47.299] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-r378.source.0 decoder=h264
[2026-11-02 11:02:47.340] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 11:02:47.340] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 11:02:47.340] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-r378.source.1 decoder=h264
[2026-11-02 11:02:47.388] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 11:02:47.388] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 11:02:47.388] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-r378.source.2 decoder=h264
[2026-11-02 11:02:47.451] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-11-02 11:02:47.487] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-11-02 11:02:47.487] [info] [graph][planner.capability] canvas_cuda_stream stage=prepared device_stream=0000000000000000 private_nonblocking=1
[2026-11-02 11:02:47.495] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 11:02:47.495] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 11:02:47.495] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-11-02 11:02:47.500] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 11:02:47.500] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 11:02:47.500] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-11-02 11:02:47.505] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-02 11:02:47.505] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-02 11:02:47.505] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-11-02 11:02:47.528] [info] [graph][planner.capability] composition_preparation_storage stage=graph_bound scope=shared_reservations capacity=2147483648 retained=816853463 high_water=834325277
[CLI] audio_plan branch=transcode_frame reason=transcode_required_by_output_frame_contract codec=aac sample_rate=44100 channels=2 access_unit_samples=1024 encoder=aac bitrate_kbps=192
[CLI] prepared_output_id=1
[CLI] selected_chain=fixed-grid-composition score=0 decoder=h264,h264,h264 filter=scale_cuda=640:360,scale_cuda=640:360,scale_cuda=640:360 encoder=hevc_nvenc
[2026-11-02 11:02:47.530] [info] [graph][runtime.lifecycle] compile.begin
[2026-11-02 11:02:47.531] [info] [graph][runtime.channel] create channel=1 edge=1 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 11:02:47.531] [info] [graph][runtime.channel] create channel=2 edge=2 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 11:02:47.531] [info] [graph][runtime.channel] create channel=3 edge=3 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-02 11:02:47.531] [info] [graph][runtime.channel] create channel=4 edge=4 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.531] [info] [graph][runtime.channel] create channel=5 edge=5 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.532] [info] [graph][runtime.channel] create channel=6 edge=6 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.532] [info] [graph][runtime.channel] create channel=7 edge=7 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.532] [info] [graph][runtime.channel] create channel=8 edge=8 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.532] [info] [graph][runtime.channel] create channel=9 edge=9 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:02:47.532] [info] [graph][runtime.channel] create channel=10 edge=10 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.532] [info] [graph][runtime.channel] create channel=11 edge=11 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.532] [info] [graph][runtime.channel] create channel=12 edge=12 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:02:47.532] [info] [graph][runtime.channel] create channel=13 edge=13 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:02:47.532] [info] [graph][runtime.channel] create channel=14 edge=14 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.532] [info] [graph][runtime.channel] create channel=15 edge=15 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.532] [info] [graph][runtime.channel] create channel=16 edge=16 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.532] [info] [graph][runtime.channel] create channel=17 edge=17 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.532] [info] [graph][runtime.channel] create channel=18 edge=18 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.532] [info] [graph][runtime.channel] create channel=19 edge=19 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.532] [info] [graph][runtime.channel] create channel=20 edge=20 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.532] [info] [graph][runtime.channel] create channel=21 edge=21 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-02 11:02:47.532] [info] [graph][runtime.channel] create channel=22 edge=22 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 11:02:47.533] [info] [graph][runtime.channel] create channel=23 edge=23 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:02:47.533] [info] [graph][runtime.channel] create channel=24 edge=24 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 11:02:47.533] [info] [graph][runtime.channel] create channel=25 edge=25 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:02:47.533] [info] [graph][runtime.channel] create channel=26 edge=26 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:02:47.533] [info] [graph][runtime.channel] create channel=27 edge=27 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:02:47.533] [info] [graph][runtime.channel] create channel=28 edge=28 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:02:47.533] [info] [graph][runtime.channel] create channel=29 edge=29 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:02:47.533] [info] [graph][runtime.channel] create channel=30 edge=30 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.533] [info] [graph][runtime.channel] create channel=31 edge=31 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.533] [info] [graph][runtime.channel] create channel=32 edge=32 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.533] [info] [graph][runtime.channel] create channel=33 edge=33 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.533] [info] [graph][runtime.channel] create channel=34 edge=34 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.533] [info] [graph][runtime.channel] create channel=35 edge=35 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.533] [info] [graph][runtime.channel] create channel=36 edge=36 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:02:47.533] [info] [graph][runtime.channel] create channel=37 edge=37 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.534] [info] [graph][runtime.channel] create channel=38 edge=38 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.534] [info] [graph][runtime.channel] create channel=39 edge=39 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:02:47.534] [info] [graph][runtime.channel] create channel=40 edge=40 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:02:47.534] [info] [graph][runtime.channel] create channel=41 edge=41 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.534] [info] [graph][runtime.channel] create channel=42 edge=42 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.534] [info] [graph][runtime.channel] create channel=43 edge=43 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.534] [info] [graph][runtime.channel] create channel=44 edge=44 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.534] [info] [graph][runtime.channel] create channel=45 edge=45 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.534] [info] [graph][runtime.channel] create channel=46 edge=46 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.534] [info] [graph][runtime.channel] create channel=47 edge=47 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.534] [info] [graph][runtime.channel] create channel=48 edge=48 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-02 11:02:47.534] [info] [graph][runtime.channel] create channel=49 edge=49 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 11:02:47.534] [info] [graph][runtime.channel] create channel=50 edge=50 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:02:47.535] [info] [graph][runtime.channel] create channel=51 edge=51 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 11:02:47.535] [info] [graph][runtime.channel] create channel=52 edge=52 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:02:47.535] [info] [graph][runtime.channel] create channel=53 edge=53 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:02:47.535] [info] [graph][runtime.channel] create channel=54 edge=54 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:02:47.535] [info] [graph][runtime.channel] create channel=55 edge=55 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:02:47.535] [info] [graph][runtime.channel] create channel=56 edge=56 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:02:47.535] [info] [graph][runtime.channel] create channel=57 edge=57 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.535] [info] [graph][runtime.channel] create channel=58 edge=58 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.535] [info] [graph][runtime.channel] create channel=59 edge=59 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.535] [info] [graph][runtime.channel] create channel=60 edge=60 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.535] [info] [graph][runtime.channel] create channel=61 edge=61 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.535] [info] [graph][runtime.channel] create channel=62 edge=62 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.535] [info] [graph][runtime.channel] create channel=63 edge=63 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.535] [info] [graph][runtime.channel] create channel=64 edge=64 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.535] [info] [graph][runtime.channel] create channel=65 edge=65 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.535] [info] [graph][runtime.channel] create channel=66 edge=66 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:02:47.535] [info] [graph][runtime.channel] create channel=67 edge=67 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:02:47.536] [info] [graph][runtime.channel] create channel=68 edge=68 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.536] [info] [graph][runtime.channel] create channel=69 edge=69 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.536] [info] [graph][runtime.channel] create channel=70 edge=70 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.536] [info] [graph][runtime.channel] create channel=71 edge=71 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:02:47.536] [info] [graph][runtime.channel] create channel=72 edge=72 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:02:47.536] [info] [graph][runtime.channel] create channel=73 edge=73 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:02:47.536] [info] [graph][runtime.channel] create channel=74 edge=74 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:02:47.536] [info] [graph][runtime.channel] create channel=75 edge=75 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.536] [info] [graph][runtime.channel] create channel=76 edge=76 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.536] [info] [graph][runtime.channel] create channel=77 edge=77 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.536] [info] [graph][runtime.channel] create channel=78 edge=78 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.536] [info] [graph][runtime.channel] create channel=79 edge=79 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.536] [info] [graph][runtime.channel] create channel=80 edge=80 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.536] [info] [graph][runtime.channel] create channel=81 edge=81 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.536] [info] [graph][runtime.channel] create channel=82 edge=82 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.536] [info] [graph][runtime.channel] create channel=83 edge=83 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-02 11:02:47.536] [info] [graph][runtime.channel] create channel=84 edge=84 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 11:02:47.537] [info] [graph][runtime.channel] create channel=85 edge=85 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:02:47.537] [info] [graph][runtime.channel] create channel=86 edge=86 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 11:02:47.537] [info] [graph][runtime.channel] create channel=87 edge=87 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:02:47.537] [info] [graph][runtime.channel] create channel=88 edge=88 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:02:47.537] [info] [graph][runtime.channel] create channel=89 edge=89 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:02:47.537] [info] [graph][runtime.channel] create channel=90 edge=90 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:02:47.537] [info] [graph][runtime.channel] create channel=91 edge=91 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-11-02 11:02:47.537] [info] [graph][runtime.channel] create channel=92 edge=92 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 11:02:47.537] [info] [graph][runtime.channel] create channel=93 edge=93 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-11-02 11:02:47.537] [info] [graph][runtime.channel] create channel=94 edge=94 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 11:02:47.537] [info] [graph][runtime.channel] create channel=95 edge=95 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:02:47.537] [info] [graph][runtime.channel] create channel=96 edge=96 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:02:47.537] [info] [graph][runtime.channel] create channel=97 edge=97 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:02:47.537] [info] [graph][runtime.channel] create channel=98 edge=98 stream=Audio edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.537] [info] [graph][runtime.channel] create channel=99 edge=99 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:02:47.537] [info] [graph][runtime.channel] create channel=100 edge=100 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.537] [info] [graph][runtime.channel] create channel=101 edge=101 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0
[2026-11-02 11:02:47.538] [info] [graph][runtime.channel] create channel=102 edge=102 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 11:02:47.538] [info] [graph][runtime.channel] create channel=103 edge=103 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 11:02:47.538] [info] [graph][runtime.channel] create channel=104 edge=104 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-11-02 11:02:47.538] [info] [graph][runtime.channel] create channel=105 edge=105 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0
[2026-11-02 11:02:47.538] [info] [graph][runtime.channel] create channel=106 edge=106 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.538] [info] [graph][runtime.channel] create channel=107 edge=107 stream=Video edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-02 11:02:47.538] [info] [graph][runtime.channel] create channel=108 edge=108 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-02 11:02:47.538] [info] [graph][runtime.channel] create channel=109 edge=109 stream=Any edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-02 11:02:47.538] [info] [graph][runtime.channel] create channel=110 edge=110 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.538] [info] [graph][runtime.channel] create channel=111 edge=111 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-02 11:02:47.538] [info] [graph][runtime.channel] create channel=112 edge=112 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-11-02 11:02:47.538] [info] [graph][runtime.channel] create channel=113 edge=113 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 11:02:47.539] [info] [graph][runtime.channel] create channel=114 edge=114 stream=Any edge_kind=EncodedPacket payload=Packet queued=0
[2026-11-02 11:02:47.539] [info] [graph][runtime.channel] create channel=115 edge=115 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-11-02 11:02:47.539] [info] [graph][runtime.channel] create channel=116 edge=116 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-02 11:02:47.539] [info] [graph][runtime.channel] create channel=117 edge=117 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-02 11:02:47.539] [info] [graph][runtime.channel] create channel=118 edge=118 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-02 11:02:47.539] [info] [graph][runtime.channel] create channel=119 edge=119 stream=Any edge_kind=EncodedPacket payload=Unknown queued=0
[2026-11-02 11:02:47.539] [info] [graph][runtime.channel] create channel=120 edge=120 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-02 11:02:47.539] [info] [graph][runtime.channel] create channel=121 edge=121 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-11-02 11:02:47.539] [info] [graph][runtime.channel] create channel=122 edge=122 stream=Metadata edge_kind=Unknown payload=Unknown queued=0
[2026-11-02 11:02:47.539] [info] [graph][runtime.channel] create channel=123 edge=123 stream=Metadata edge_kind=Unknown payload=Unknown queued=0
[2026-11-02 11:02:47.539] [info] [graph][runtime.lifecycle] compiled nodes=81 edges=123 channels=123 execution_order=1->3->8->27->46->47->9->21->28->40->63->48->69->10->29->49->11->12->30->31->50->51->52->13->32->53->14->17->33->36->54->55->59->15->34->56->57->16->35->58->18->37->60->19->38->61->20->39->62->22->41->64->70->23->42->65->71->24->43->66->72->26->45->68->73->25->44->67->6->2->7->80->4->77->5->74->75->78->76->79->81
[2026-11-02 11:02:47.539] [info] [graph][runtime.lifecycle] compile.done state=DefaultRegistrationPending
[2026-11-02 11:02:47.540] [info] [graph][runtime.node] register node=1 name=composition-r378.output.video.codec_resolver kind=CodecResolver
[2026-11-02 11:02:47.540] [info] [graph][runtime.node] register node=2 name=composition-r378.output.video.encode kind=VideoEncode
[2026-11-02 11:02:47.540] [info] [graph][runtime.node] register node=3 name=composition-r378.output.audio.codec_resolver kind=AudioCodecResolver
[2026-11-02 11:02:47.540] [info] [graph][runtime.node] register node=4 name=composition-r378.output.audio.encode kind=AudioEncode
[2026-11-02 11:02:47.540] [info] [graph][runtime.node] register node=5 name=composition-r378.output.audio.canonicalizer kind=EncodedAudioCanonicalizer
[2026-11-02 11:02:47.540] [info] [graph][runtime.node] register node=8 name=composition-r378.source_0.input kind=RawRtpInput
[2026-11-02 11:02:47.540] [info] [graph][runtime.node] register node=9 name=composition-r378.source_0.rtp.clock_group kind=RtpClockGroup
[2026-11-02 11:02:47.540] [info] [graph][runtime.node] register node=10 name=composition-r378.source_0.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-11-02 11:02:47.540] [info] [graph][runtime.node] register node=11 name=composition-r378.source_0.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-11-02 11:02:47.540] [info] [graph][runtime.node] register node=12 name=composition-r378.source_0.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-11-02 11:02:47.540] [info] [graph][runtime.node] register node=13 name=composition-r378.source_0.av_sync.source_clock kind=SourceClockStateFanout
[2026-11-02 11:02:47.540] [info] [graph][runtime.node] register node=14 name=composition-r378.source_0.av_sync.video.generation_gate kind=LockedPacketGate
[2026-11-02 11:02:47.540] [info] [graph][runtime.node] register node=15 name=composition-r378.source_0.av_sync.video.canonical_input kind=CanonicalInput
[2026-11-02 11:02:47.541] [info] [graph][runtime.node] register node=16 name=composition-r378.source_0.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-11-02 11:02:47.541] [info] [graph][runtime.node] register node=17 name=composition-r378.source_0.av_sync.startup.clock kind=AvStartupClock
[2026-11-02 11:02:47.541] [info] [graph][runtime.node] register node=18 name=composition-r378.source_0.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-11-02 11:02:47.541] [info] [graph][runtime.node] register node=20 name=composition-r378.source_0.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-11-02 11:02:47.541] [info] [graph][runtime.node] register node=21 name=composition-r378.source_0.video.codec_resolver kind=CodecResolver
[2026-11-02 11:02:47.541] [info] [graph][runtime.node] register node=22 name=composition-r378.source_0.video.decode kind=VideoDecode
[2026-11-02 11:02:47.541] [info] [graph][runtime.node] register node=23 name=composition-r378.source_0.video.hwtransfer kind=HardwareTransfer
[2026-11-02 11:02:47.541] [info] [graph][runtime.node] register node=24 name=composition-r378.source_0.video.framerate kind=VideoFrameRate
[2026-11-02 11:02:47.541] [info] [graph][runtime.node] register node=25 name=composition-r378.source_0.video.preparation kind=VideoPreparation
[2026-11-02 11:02:47.541] [info] [graph][runtime.node] register node=26 name=composition-r378.source_0.video.filter kind=VideoFilter
[2026-11-02 11:02:47.541] [info] [graph][runtime.node] register node=27 name=composition-r378.source_1.input kind=RawRtpInput
[2026-11-02 11:02:47.541] [info] [graph][runtime.node] register node=28 name=composition-r378.source_1.rtp.clock_group kind=RtpClockGroup
[2026-11-02 11:02:47.541] [info] [graph][runtime.node] register node=29 name=composition-r378.source_1.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-11-02 11:02:47.541] [info] [graph][runtime.node] register node=30 name=composition-r378.source_1.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-11-02 11:02:47.541] [info] [graph][runtime.node] register node=31 name=composition-r378.source_1.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-11-02 11:02:47.541] [info] [graph][runtime.node] register node=32 name=composition-r378.source_1.av_sync.source_clock kind=SourceClockStateFanout
[2026-11-02 11:02:47.542] [info] [graph][runtime.node] register node=33 name=composition-r378.source_1.av_sync.video.generation_gate kind=LockedPacketGate
[2026-11-02 11:02:47.542] [info] [graph][runtime.node] register node=34 name=composition-r378.source_1.av_sync.video.canonical_input kind=CanonicalInput
[2026-11-02 11:02:47.542] [info] [graph][runtime.node] register node=35 name=composition-r378.source_1.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-11-02 11:02:47.542] [info] [graph][runtime.node] register node=36 name=composition-r378.source_1.av_sync.startup.clock kind=AvStartupClock
[2026-11-02 11:02:47.542] [info] [graph][runtime.node] register node=37 name=composition-r378.source_1.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-11-02 11:02:47.542] [info] [graph][runtime.node] register node=39 name=composition-r378.source_1.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-11-02 11:02:47.542] [info] [graph][runtime.node] register node=40 name=composition-r378.source_1.video.codec_resolver kind=CodecResolver
[2026-11-02 11:02:47.542] [info] [graph][runtime.node] register node=41 name=composition-r378.source_1.video.decode kind=VideoDecode
[2026-11-02 11:02:47.542] [info] [graph][runtime.node] register node=42 name=composition-r378.source_1.video.hwtransfer kind=HardwareTransfer
[2026-11-02 11:02:47.542] [info] [graph][runtime.node] register node=43 name=composition-r378.source_1.video.framerate kind=VideoFrameRate
[2026-11-02 11:02:47.542] [info] [graph][runtime.node] register node=44 name=composition-r378.source_1.video.preparation kind=VideoPreparation
[2026-11-02 11:02:47.542] [info] [graph][runtime.node] register node=45 name=composition-r378.source_1.video.filter kind=VideoFilter
[2026-11-02 11:02:47.542] [info] [graph][runtime.node] register node=46 name=composition-r378.source_2.video.input kind=RawRtpInput
[2026-11-02 11:02:47.542] [info] [graph][runtime.node] register node=47 name=composition-r378.source_2.audio.input kind=RawRtpInput
[2026-11-02 11:02:47.542] [info] [graph][runtime.node] register node=48 name=composition-r378.source_2.rtp.clock_group kind=RtpClockGroup
[2026-11-02 11:02:47.542] [info] [graph][runtime.node] register node=49 name=composition-r378.source_2.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-11-02 11:02:47.542] [info] [graph][runtime.node] register node=50 name=composition-r378.source_2.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-11-02 11:02:47.542] [info] [graph][runtime.node] register node=51 name=composition-r378.source_2.av_sync.audio.protocol_binder kind=RtpPacketClockBinder
[2026-11-02 11:02:47.543] [info] [graph][runtime.node] register node=52 name=composition-r378.source_2.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-11-02 11:02:47.543] [info] [graph][runtime.node] register node=53 name=composition-r378.source_2.av_sync.source_clock kind=SourceClockStateFanout
[2026-11-02 11:02:47.543] [info] [graph][runtime.node] register node=54 name=composition-r378.source_2.av_sync.video.generation_gate kind=LockedPacketGate
[2026-11-02 11:02:47.543] [info] [graph][runtime.node] register node=55 name=composition-r378.source_2.av_sync.audio.generation_gate kind=LockedPacketGate
[2026-11-02 11:02:47.543] [info] [graph][runtime.node] register node=56 name=composition-r378.source_2.av_sync.video.canonical_input kind=CanonicalInput
[2026-11-02 11:02:47.543] [info] [graph][runtime.node] register node=57 name=composition-r378.source_2.av_sync.audio.canonical_input kind=CanonicalInput
[2026-11-02 11:02:47.543] [info] [graph][runtime.node] register node=58 name=composition-r378.source_2.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-11-02 11:02:47.543] [info] [graph][runtime.node] register node=59 name=composition-r378.source_2.av_sync.startup.clock kind=AvStartupClock
[2026-11-02 11:02:47.543] [info] [graph][runtime.node] register node=60 name=composition-r378.source_2.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-11-02 11:02:47.543] [info] [graph][runtime.node] register node=62 name=composition-r378.source_2.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-11-02 11:02:47.543] [info] [graph][runtime.node] register node=63 name=composition-r378.source_2.video.codec_resolver kind=CodecResolver
[2026-11-02 11:02:47.543] [info] [graph][runtime.node] register node=64 name=composition-r378.source_2.video.decode kind=VideoDecode
[2026-11-02 11:02:47.543] [info] [graph][runtime.node] register node=65 name=composition-r378.source_2.video.hwtransfer kind=HardwareTransfer
[2026-11-02 11:02:47.543] [info] [graph][runtime.node] register node=66 name=composition-r378.source_2.video.framerate kind=VideoFrameRate
[2026-11-02 11:02:47.543] [info] [graph][runtime.node] register node=67 name=composition-r378.source_2.video.preparation kind=VideoPreparation
[2026-11-02 11:02:47.544] [info] [graph][runtime.node] register node=68 name=composition-r378.source_2.video.filter kind=VideoFilter
[2026-11-02 11:02:47.544] [info] [graph][runtime.node] register node=69 name=composition-r378.source_2.audio.encode.codec_resolver kind=AudioCodecResolver
[2026-11-02 11:02:47.544] [info] [graph][runtime.node] register node=70 name=composition-r378.source_2.audio.encode.decode kind=AudioDecode
[2026-11-02 11:02:47.544] [info] [graph][runtime.node] register node=71 name=composition-r378.source_2.audio.encode.startup_trim kind=Unknown
[2026-11-02 11:02:47.544] [info] [graph][runtime.node] register node=72 name=composition-r378.source_2.audio.encode.drift_controller kind=AudioDriftController
[2026-11-02 11:02:47.544] [info] [graph][runtime.node] register node=73 name=composition-r378.source_2.audio.encode.resample kind=AudioResample
[2026-11-02 11:02:47.544] [info] [graph][runtime.node] register node=74 name=composition-r378.output.scheduler.scheduler kind=AvOutputScheduler
[2026-11-02 11:02:47.544] [info] [graph][runtime.node] register node=75 name=composition-r378.output.scheduler.router kind=ScheduledOutputRouter
[2026-11-02 11:02:47.544] [info] [graph][runtime.node] register node=76 name=composition-r378.output.mpegts.mux kind=FileMux
[2026-11-02 11:02:47.544] [info] [graph][runtime.node] register node=77 name=composition-r378.output.mpegts.plan.source kind=Unknown
[2026-11-02 11:02:47.544] [info] [graph][runtime.node] register node=78 name=composition-r378.output.mpegts.scheduled.adapter kind=Unknown
[2026-11-02 11:02:47.544] [info] [graph][runtime.node] register node=79 name=composition-r378.output.mpegts.datagram.materializer kind=MpegTsDatagramMaterializer
[2026-11-02 11:02:47.544] [info] [graph][runtime.node] register node=80 name=composition-r378.output.mpegts.transport.plan kind=DatagramTransportPlanSource
[2026-11-02 11:02:47.545] [info] [graph][runtime.node] register node=81 name=composition-r378.output.mpegts.transport.sender kind=ScheduledDatagramSender
[2026-11-02 11:02:47.545] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=waiting_for_random_access detail="" description=""
[2026-11-02 11:02:47.545] [info] [graph][runtime.lifecycle] start.begin mode=threaded
[2026-11-02 11:02:47.546] [info] [graph][runtime.lifecycle] worker.finished node=1
[2026-11-02 11:02:47.546] [info] [graph][runtime.node] worker.thread_cpu node=1 available=1 cpu_ns=0 process_calls=1
[2026-11-02 11:02:47.547] [info] [graph][runtime.lifecycle] start.done state=ThreadedRunning
[2026-11-02 11:02:47.547] [info] [graph][runtime.node] audio_encode_trace stage=codec_bound
[2026-11-02 11:02:47.547] [info] [graph][runtime.lifecycle] worker.finished node=3
[2026-11-02 11:02:47.548] [info] [graph][runtime.node] worker.thread_cpu node=3 available=1 cpu_ns=0 process_calls=1
[2026-11-02 11:02:47.547] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:02:47.547] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:02:47.548] [info] [graph][runtime.lifecycle] worker.finished node=69
[2026-11-02 11:02:47.549] [info] [graph][runtime.node] worker.thread_cpu node=69 available=1 cpu_ns=0 process_calls=1
[2026-11-02 11:02:47.547] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:02:47.550] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-11-02 11:02:47.550] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-11-02 11:02:47.550] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=1 generation=1
[2026-11-02 11:02:47.551] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=1 generation=1
[2026-11-02 11:02:47.552] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=release source_identity=composition-r378.source.2.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-10000000 duration_ns=33333333 decoded_sequence=unavailable pts=3352997316 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=8043268868000 group=composition-r378.source.2 release_identity=3 transition_sequence=unavailable epoch_source_start_ns=-5011337 epoch_master_release_ns=466673400 video_count=23 min_presentation_ns=-10000000 max_end_ns=756666666 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.552] [info] [graph][runtime.node] source_video_trace node_id=20 node=MediaAvBoundReleaseExtractorNode stage=release source_identity=composition-r378.source.0.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-6000000 duration_ns=33333333 decoded_sequence=unavailable pts=1172003513 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=8043268942800 group=composition-r378.source.0 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-6000000 epoch_master_release_ns=466717000 video_count=16 min_presentation_ns=-6000000 max_end_ns=527333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.552] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=published source_identity=composition-r378.source.2.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-10000000 duration_ns=33333333 decoded_sequence=unavailable pts=3352997316 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=8043269017400 group=composition-r378.source.2 release_identity=3 transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.552] [info] [graph][runtime.node] source_video_trace node_id=20 node=MediaAvBoundReleaseExtractorNode stage=published source_identity=composition-r378.source.0.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-6000000 duration_ns=33333333 decoded_sequence=unavailable pts=1172003513 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=8043269120500 group=composition-r378.source.0 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.552] [info] [graph][runtime.lifecycle] worker.finished node=40
[2026-11-02 11:02:47.552] [info] [graph][runtime.node] worker.thread_cpu node=40 available=1 cpu_ns=0 process_calls=2
[2026-11-02 11:02:47.553] [info] [graph][runtime.lifecycle] worker.finished node=63
[2026-11-02 11:02:47.553] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=3352997316 dts=3352997316 size=33389 generation=1 sequence=1
[2026-11-02 11:02:47.553] [info] [graph][runtime.node] video_encode.bind_encoder codec=hevc_nvenc pix_fmt=cuda rate_control=cbr bitrate=8000000 min_bitrate=8000000 max_bitrate=8000000 buffer_size=267000 fps=30/1 gop=60 frame_kind=hardware hwaccel=cuda hw_device_ctx=set hw_frames_ctx=set
[2026-11-02 11:02:47.553] [info] [graph][runtime.node] worker.thread_cpu node=63 available=1 cpu_ns=0 process_calls=2
[2026-11-02 11:02:47.553] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=input source_identity=composition-r378.source.2.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-10000000 duration_ns=33333333 decoded_sequence=unavailable pts=3352997316 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=8043270265600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.553] [info] [graph][runtime.lifecycle] worker.finished node=21
[2026-11-02 11:02:47.555] [info] [graph][runtime.node] worker.thread_cpu node=21 available=1 cpu_ns=0 process_calls=1
[2026-11-02 11:02:47.553] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=1172003513 dts=1172003513 size=33389 generation=1 sequence=1
[2026-11-02 11:02:47.555] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=input source_identity=composition-r378.source.0.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-6000000 duration_ns=33333333 decoded_sequence=unavailable pts=1172003513 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=8043272025700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.557] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-11-02 11:02:47.557] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=1 generation=1
[2026-11-02 11:02:47.558] [info] [graph][runtime.node] source_video_trace node_id=39 node=MediaAvBoundReleaseExtractorNode stage=release source_identity=composition-r378.source.1.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-6000000 duration_ns=33333333 decoded_sequence=unavailable pts=3960264574 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=8043274758900 group=composition-r378.source.1 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-6000000 epoch_master_release_ns=473112200 video_count=16 min_presentation_ns=-6000000 max_end_ns=527333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.558] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=3960264574 dts=3960264574 size=33389 generation=1 sequence=1
[2026-11-02 11:02:47.558] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=input source_identity=composition-r378.source.1.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-6000000 duration_ns=33333333 decoded_sequence=unavailable pts=3960264574 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=8043275107800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.558] [info] [graph][runtime.node] source_video_trace node_id=39 node=MediaAvBoundReleaseExtractorNode stage=published source_identity=composition-r378.source.1.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-6000000 duration_ns=33333333 decoded_sequence=unavailable pts=3960264574 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=8043274949500 group=composition-r378.source.1 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=1 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.580] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-11-02 11:02:47.580] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=accepted source_identity=composition-r378.source.2.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-10000000 duration_ns=33333333 decoded_sequence=unavailable pts=3352997316 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=8043297550700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.581] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=3352997316 format=cuda size=1280x720 buffer_identity=0xb08400000 drm_prime=0 software_frame=0
[2026-11-02 11:02:47.581] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-11-02 11:02:47.581] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=resolved source_identity=composition-r378.source.2.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-10000000 duration_ns=33333333 decoded_sequence=unavailable pts=3352997316 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=8043297930300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.581] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=accepted source_identity=composition-r378.source.1.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-6000000 duration_ns=33333333 decoded_sequence=unavailable pts=3960264574 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=8043297976600 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.581] [info] [graph][runtime.node] source_video_trace node_id=64 node=VideoDecodeNode stage=published source_identity=composition-r378.source.2.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-10000000 duration_ns=33333333 decoded_sequence=unavailable pts=3352997316 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=8043298094300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.581] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=000002639292CBE0 stream=Video payload=Frame pts=3352997316 dts=3352997316 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:02:47.581] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=3960264574 format=cuda size=1280x720 buffer_identity=0xb08600000 drm_prime=0 software_frame=0
[2026-11-02 11:02:47.581] [info] [graph][runtime.node] source_video_trace node_id=65 node=HardwareTransferNode stage=published source_identity=composition-r378.source.2.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-10000000 duration_ns=33333333 decoded_sequence=unavailable pts=3352997316 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=8043298411100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.581] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=000002639292CBE0 stream=Video payload=Frame pts=3352997316 dts=3352997316 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:02:47.581] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=resolved source_identity=composition-r378.source.1.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-6000000 duration_ns=33333333 decoded_sequence=unavailable pts=3960264574 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=8043298542800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.582] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=000002639292CBE0 stream=Video payload=Frame pts=3352997316 dts=3352997316 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:02:47.582] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-11-02 11:02:47.582] [info] [graph][runtime.node] source_video_trace node_id=41 node=VideoDecodeNode stage=published source_identity=composition-r378.source.1.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-6000000 duration_ns=33333333 decoded_sequence=unavailable pts=3960264574 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=8043298827000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.582] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=00000263929247E0 stream=Video payload=Frame pts=3960264574 dts=3960264574 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:02:47.582] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=00000263899C5220 stream=Video payload=Frame pts=3352997316 dts=3352997316 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:02:47.582] [info] [graph][runtime.node] source_video_trace node_id=66 node=VideoFrameRateNode stage=published source_identity=composition-r378.source.2.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-10000000 duration_ns=33333333 decoded_sequence=unavailable pts=3352997316 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=8043299080800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.582] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=00000263899C5220 stream=Video payload=Frame pts=3352997316 dts=3352997316 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:02:47.582] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=00000263929247E0 stream=Video payload=Frame pts=3960264574 dts=3960264574 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:02:47.582] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-11-02 11:02:47.582] [info] [graph][runtime.node] source_video_trace node_id=42 node=HardwareTransferNode stage=published source_identity=composition-r378.source.1.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-6000000 duration_ns=33333333 decoded_sequence=unavailable pts=3960264574 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=8043299244400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.583] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-11-02 11:02:47.583] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=00000263929247E0 stream=Video payload=Frame pts=3960264574 dts=3960264574 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:02:47.583] [info] [graph][runtime.node] source_video_trace node_id=43 node=VideoFrameRateNode stage=published source_identity=composition-r378.source.1.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-6000000 duration_ns=33333333 decoded_sequence=unavailable pts=3960264574 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=8043299866800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.583] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=000002639292CBE0 stream=Video payload=Frame pts=3960264574 dts=3960264574 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:02:47.583] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=000002639292CBE0 stream=Video payload=Frame pts=3960264574 dts=3960264574 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:02:47.583] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-11-02 11:02:47.587] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-11-02 11:02:47.587] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=accepted source_identity=composition-r378.source.0.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-6000000 duration_ns=33333333 decoded_sequence=unavailable pts=1172003513 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=8043303858000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.587] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=1172003513 format=cuda size=1280x720 buffer_identity=0xb0ae00000 drm_prime=0 software_frame=0
[2026-11-02 11:02:47.587] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=resolved source_identity=composition-r378.source.0.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-6000000 duration_ns=33333333 decoded_sequence=unavailable pts=1172003513 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=8043304203900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.587] [info] [graph][runtime.node] source_video_trace node_id=22 node=VideoDecodeNode stage=published source_identity=composition-r378.source.0.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-6000000 duration_ns=33333333 decoded_sequence=unavailable pts=1172003513 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=8043304348100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.588] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=0000026393459280 stream=Video payload=Frame pts=1172003513 dts=1172003513 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:02:47.588] [info] [graph][runtime.node] source_video_trace node_id=23 node=HardwareTransferNode stage=published source_identity=composition-r378.source.0.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-6000000 duration_ns=33333333 decoded_sequence=unavailable pts=1172003513 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=8043304724500 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.588] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=0000026393459280 stream=Video payload=Frame pts=1172003513 dts=1172003513 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:02:47.588] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=0000026393459280 stream=Video payload=Frame pts=1172003513 dts=1172003513 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:02:47.588] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-11-02 11:02:47.588] [info] [graph][runtime.node] source_video_trace node_id=24 node=VideoFrameRateNode stage=published source_identity=composition-r378.source.0.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-6000000 duration_ns=33333333 decoded_sequence=unavailable pts=1172003513 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=8043305380900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.588] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=0000026393459670 stream=Video payload=Frame pts=1172003513 dts=1172003513 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:02:47.588] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=0000026393459670 stream=Video payload=Frame pts=1172003513 dts=1172003513 duration=3000 tb=1/90000 flags=68
[2026-11-02 11:02:47.589] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-11-02 11:02:47.637] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=54
[2026-11-02 11:02:47.637] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=54
[2026-11-02 11:02:47.637] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=48
[2026-11-02 11:02:47.638] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=input source_identity=composition-r378.source.1.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-6000000 duration_ns=33333333 decoded_sequence=unavailable pts=3960264574 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=8043354888900 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.638] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=input source_identity=composition-r378.source.2.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-10000000 duration_ns=33333333 decoded_sequence=unavailable pts=3352997316 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=8043354896300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.638] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=input source_identity=composition-r378.source.0.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-6000000 duration_ns=33333333 decoded_sequence=unavailable pts=1172003513 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=8043355038000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.639] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=accepted source_identity=composition-r378.source.1.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-6000000 duration_ns=33333333 decoded_sequence=unavailable pts=3960264574 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=8043355929100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.639] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=accepted source_identity=composition-r378.source.0.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-6000000 duration_ns=33333333 decoded_sequence=unavailable pts=1172003513 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=8043356282100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.639] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=resolved source_identity=composition-r378.source.1.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-6000000 duration_ns=33333333 decoded_sequence=unavailable pts=1320088 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=8043356331400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.639] [info] [graph][runtime.node] source_video_trace node_id=45 node=VideoFilterNode stage=published source_identity=composition-r378.source.1.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-6000000 duration_ns=33333333 decoded_sequence=unavailable pts=1320088 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=8043356572100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.640] [info] [graph][runtime.node] source_video_trace node_id=44 node=VideoPreparationNode stage=input source_identity=composition-r378.source.1.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-6000000 duration_ns=33333333 decoded_sequence=unavailable pts=1320088 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=8043356582300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.640] [info] [graph][runtime.node] source_video_trace node_id=44 node=VideoPreparationNode stage=output_attempt source_identity=composition-r378.source.1.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-6000000 duration_ns=33333333 decoded_sequence=unavailable pts=1320088 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=8043356874300 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.640] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb08c90000 drm_prime=0 software_frame=0
[2026-11-02 11:02:47.640] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=resolved source_identity=composition-r378.source.0.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-6000000 duration_ns=33333333 decoded_sequence=unavailable pts=390668 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=8043356752400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.640] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=accepted source_identity=composition-r378.source.2.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-10000000 duration_ns=33333333 decoded_sequence=unavailable pts=3352997316 descriptor_time_base=1/90000 frame_time_base=1/90000 steady_ns=8043356699700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.640] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=16 audio_count=0 first_video_sequence=1
[2026-11-02 11:02:47.640] [info] [graph][runtime.node] source_video_trace node_id=26 node=VideoFilterNode stage=published source_identity=composition-r378.source.0.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-6000000 duration_ns=33333333 decoded_sequence=unavailable pts=390668 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=8043357119100 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.640] [info] [graph][runtime.node] source_video_trace node_id=25 node=VideoPreparationNode stage=input source_identity=composition-r378.source.0.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-6000000 duration_ns=33333333 decoded_sequence=unavailable pts=390668 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=8043357128400 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.640] [info] [graph][runtime.node] source_video_trace node_id=38 node=ActivationReleaseSequencer stage=activation source_identity=composition-r378.source.1.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-6000000 duration_ns=33333333 decoded_sequence=unavailable pts=3960264574 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=8043357425400 group=composition-r378.source.1 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-6000000 epoch_master_release_ns=622486168 video_count=16 min_presentation_ns=-6000000 max_end_ns=527333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.640] [info] [graph][runtime.node] source_video_trace node_id=44 node=VideoPreparationNode stage=published source_identity=unavailable identity_link=node_generation_input_source_sequence generation=1 source_sequence=1 presentation_ns=-6000000 duration_ns=33333333 decoded_sequence=unavailable pts=1320088 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=8043357433800 group=composition-r378.source.1 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.640] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb08d20000 drm_prime=0 software_frame=0
[2026-11-02 11:02:47.640] [info] [graph][runtime.node] source_video_trace node_id=25 node=VideoPreparationNode stage=output_attempt source_identity=composition-r378.source.0.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-6000000 duration_ns=33333333 decoded_sequence=unavailable pts=390668 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=8043357578800 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.641] [info] [graph][runtime.node] source_video_trace node_id=38 node=ActivationReleaseSequencer stage=release_published source_identity=composition-r378.source.1.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-6000000 duration_ns=33333333 decoded_sequence=unavailable pts=3960264574 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=8043357673500 group=composition-r378.source.1 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-6000000 epoch_master_release_ns=622486168 video_count=16 min_presentation_ns=-6000000 max_end_ns=527333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.641] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_candidate source_identity=composition-r378.source.1.input.video generation=1 steady_ns=8043357746400 source_sequence=1 presentation_ns=-6000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 11:02:47.641] [info] [graph][runtime.node] source_video_trace node_id=39 node=MediaAvBoundReleaseExtractorNode stage=release_committed source_identity=composition-r378.source.1.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-6000000 duration_ns=33333333 decoded_sequence=unavailable pts=3960264574 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=8043357749900 group=composition-r378.source.1 release_identity=7 transition_sequence=unavailable epoch_source_start_ns=-6000000 epoch_master_release_ns=473112200 video_count=16 min_presentation_ns=-6000000 max_end_ns=527333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=16 release_count=18 release_committed_count=1 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.641] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=resolved source_identity=composition-r378.source.2.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-10000000 duration_ns=33333333 decoded_sequence=unavailable pts=1117666 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=8043357825700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.641] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=16 audio_count=0 first_video_sequence=1
[2026-11-02 11:02:47.641] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=0
[2026-11-02 11:02:47.641] [info] [graph][runtime.node] source_video_trace node_id=68 node=VideoFilterNode stage=published source_identity=composition-r378.source.2.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-10000000 duration_ns=33333333 decoded_sequence=unavailable pts=1117666 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=8043358425000 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=unavailable input_count=1 accepted_count=1 would_block_count=0 resolved_count=1 output_attempt_count=0 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.642] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=input source_identity=composition-r378.source.2.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-10000000 duration_ns=33333333 decoded_sequence=unavailable pts=1117666 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=8043358433700 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.642] [info] [graph][runtime.node] source_video_trace node_id=25 node=VideoPreparationNode stage=published source_identity=unavailable identity_link=node_generation_input_source_sequence generation=1 source_sequence=1 presentation_ns=-6000000 duration_ns=33333333 decoded_sequence=unavailable pts=390668 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=8043358664900 group=composition-r378.source.0 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.642] [info] [graph][runtime.node] source_video_trace node_id=19 node=ActivationReleaseSequencer stage=activation source_identity=composition-r378.source.0.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-6000000 duration_ns=33333333 decoded_sequence=unavailable pts=1172003513 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=8043358669000 group=composition-r378.source.0 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-6000000 epoch_master_release_ns=623749968 video_count=16 min_presentation_ns=-6000000 max_end_ns=527333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.642] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb08c00000 drm_prime=0 software_frame=0
[2026-11-02 11:02:47.642] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=output_attempt source_identity=composition-r378.source.2.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-10000000 duration_ns=33333333 decoded_sequence=unavailable pts=1117666 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=8043359036500 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=1 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=0 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.642] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_candidate source_identity=composition-r378.source.0.input.video generation=1 steady_ns=8043359273500 source_sequence=1 presentation_ns=-6000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 11:02:47.642] [info] [graph][runtime.node] source_video_trace node_id=19 node=ActivationReleaseSequencer stage=release_published source_identity=composition-r378.source.0.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-6000000 duration_ns=33333333 decoded_sequence=unavailable pts=1172003513 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=8043359273200 group=composition-r378.source.0 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-6000000 epoch_master_release_ns=623749968 video_count=16 min_presentation_ns=-6000000 max_end_ns=527333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.642] [info] [graph][runtime.node] source_video_trace node_id=20 node=MediaAvBoundReleaseExtractorNode stage=release_committed source_identity=composition-r378.source.0.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-6000000 duration_ns=33333333 decoded_sequence=unavailable pts=1172003513 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=8043359277300 group=composition-r378.source.0 release_identity=1 transition_sequence=unavailable epoch_source_start_ns=-6000000 epoch_master_release_ns=466717000 video_count=16 min_presentation_ns=-6000000 max_end_ns=527333333 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=16 release_count=18 release_committed_count=1 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.642] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=23 audio_count=22 first_video_sequence=1
[2026-11-02 11:02:47.643] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=0
[2026-11-02 11:02:47.643] [info] [graph][runtime.node] source_video_trace node_id=67 node=VideoPreparationNode stage=published source_identity=unavailable identity_link=node_generation_input_source_sequence generation=1 source_sequence=1 presentation_ns=-10000000 duration_ns=33333333 decoded_sequence=unavailable pts=1117666 descriptor_time_base=1/30 frame_time_base=1/90000 steady_ns=8043360236000 group=composition-r378.source.2 release_identity=unavailable transition_sequence=unavailable epoch_source_start_ns=unavailable epoch_master_release_ns=unavailable video_count=unavailable min_presentation_ns=unavailable max_end_ns=unavailable preparation_phase=3 input_count=1 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=1 published_count=1 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.644] [info] [graph][runtime.node] source_video_trace node_id=61 node=ActivationReleaseSequencer stage=activation source_identity=composition-r378.source.2.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-10000000 duration_ns=33333333 decoded_sequence=unavailable pts=3352997316 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=8043360630300 group=composition-r378.source.2 release_identity=3 transition_sequence=unavailable epoch_source_start_ns=-5011337 epoch_master_release_ns=625279168 video_count=23 min_presentation_ns=-10000000 max_end_ns=756666666 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.644] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_candidate source_identity=composition-r378.source.2.input.video generation=1 steady_ns=8043360758800 source_sequence=1 presentation_ns=-10000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 11:02:47.644] [info] [graph][runtime.node] audio_decode_trace stage=first_packet
[2026-11-02 11:02:47.644] [info] [graph][runtime.node] source_video_trace node_id=61 node=ActivationReleaseSequencer stage=release_published source_identity=composition-r378.source.2.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-10000000 duration_ns=33333333 decoded_sequence=unavailable pts=3352997316 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=8043360762500 group=composition-r378.source.2 release_identity=3 transition_sequence=unavailable epoch_source_start_ns=-5011337 epoch_master_release_ns=625279168 video_count=23 min_presentation_ns=-10000000 max_end_ns=756666666 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=1 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.644] [info] [graph][runtime.node] source_video_trace node_id=62 node=MediaAvBoundReleaseExtractorNode stage=release_committed source_identity=composition-r378.source.2.input.video identity_link=direct generation=1 source_sequence=1 presentation_ns=-10000000 duration_ns=33333333 decoded_sequence=unavailable pts=3352997316 descriptor_time_base=0/1 frame_time_base=unavailable steady_ns=8043360770900 group=composition-r378.source.2 release_identity=3 transition_sequence=unavailable epoch_source_start_ns=-5011337 epoch_master_release_ns=466673400 video_count=23 min_presentation_ns=-10000000 max_end_ns=756666666 preparation_phase=unavailable input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=23 release_count=25 release_committed_count=1 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:02:47.644] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=22
[2026-11-02 11:02:47.644] [info] [graph][runtime.node] audio_decode_trace stage=first_frame samples=1024 rate=44100
[2026-11-02 11:02:47.645] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1 source_master_ns=648499121 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-11-02 11:02:47.645] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2 source_master_ns=671719076 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=45346
[2026-11-02 11:02:47.645] [info] [graph][runtime.node] audio_encode_trace stage=first_frame
[2026-11-02 11:02:47.645] [info] [graph][runtime.node] output_preparation stage=transport_activate generation=1 elapsed_ns=203100 planned_bytes=180767114966 planned_items=1 event=scope_exit
[2026-11-02 11:02:47.646] [info] [graph][runtime.node] output_preparation stage=global_sequence generation=1 elapsed_ns=48800 planned_bytes=68968 planned_items=2154 event=scope_exit
[2026-11-02 11:02:47.646] [info] [graph][runtime.node] output_preparation stage=transport_buffer generation=1 elapsed_ns=564500 planned_bytes=180767114966 planned_items=1 event=scope_exit
[2026-11-02 11:02:47.646] [info] [graph][runtime.node] output_preparation stage=ts_packetizer generation=1 elapsed_ns=89400 planned_bytes=180765122912 planned_items=61257 event=scope_exit
[2026-11-02 11:02:47.646] [info] [graph][runtime.node] output_preparation stage=ts_mux generation=1 elapsed_ns=410200 planned_bytes=945416 planned_items=59086 event=scope_exit
[2026-11-02 11:02:47.646] [info] [graph][runtime.node] mpegts_emission_plan generation=1 mode=canonical_access_unit_window access_unit_window_ns=100000000 video_initial_service_window_ns=13990650 audio_initial_service_window_ns=13990650 packet_size_bytes=188 maximum_payload_bytes=1316 per_datagram_overhead_bytes=0
[2026-11-02 11:02:47.647] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-r378.source.0.input.video generation=1 steady_ns=8043364335800 source_sequence=1 presentation_ns=-6000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 11:02:47.647] [info] [graph][runtime.node] video_encode.input_contract format=cuda size=1280x720 buffer_identity=0xb0a000000 drm_prime=0 software_frame=0
[2026-11-02 11:02:47.647] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-r378.source.1.input.video generation=1 steady_ns=8043364447300 source_sequence=1 presentation_ns=-6000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 11:02:47.648] [info] [graph][runtime.node] video_encode.generation_start_key_frame pts=0 generation=1
[2026-11-02 11:02:47.648] [info] [graph][runtime.node] source_video_trace node_id=6 node=MediaAvContinuousAggregateNode stage=aggregate_contribution source_identity=composition-r378.source.2.input.video generation=1 steady_ns=8043364698600 source_sequence=1 presentation_ns=-10000000 duration_ns=33333333 decoded_sequence=unavailable
[2026-11-02 11:02:47.648] [info] [graph][runtime.node] video_encode.first_frame pts=0 generation=attached
[2026-11-02 11:02:47.653] [info] [graph][runtime.node] video_encode.first_submit result=accepted
[2026-11-02 11:02:47.653] [info] [graph][runtime.node] video_encode.first_packet pts=0 dts=0 duration=0 size=259 side_data_elems=1 received_footprint=267 side_data[0]=Quality stats:8
[2026-11-02 11:02:47.654] [info] [graph][runtime.node] video_encode.first_packet_footprint bytes=267 wrapped_size=259 wrapped_side_data_elems=1
[2026-11-02 11:02:47.654] [info] [graph][runtime.node] av_scheduler_trace stage=first_video_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=627008668 dispatch_master_ns=627008668 now_ns=469267100 node=74 identity_kind=output identity="composition-r378.output" canonical_sequence=1 canonical_created_steady_ns=8043370762600 head_observed_steady_ns=8043370791700 codec_receive_begin_steady_ns=8043370561100 codec_receive_end_steady_ns=8043370562000
[2026-11-02 11:02:47.654] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=469359200 present_generation=1 present_canonical_target_ns=0 present_canonical_dispatch_ns=0 present_target_master_ns=627008668 present_dispatch_master_ns=627008668
[2026-11-02 11:02:47.692] [info] [graph][runtime.node] audio_encode_trace stage=first_packet
[2026-11-02 11:02:47.693] [info] [graph][runtime.node] av_scheduler_trace stage=first_audio_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=627008668 dispatch_master_ns=627008668 now_ns=508130200 node=74 identity_kind=output identity="composition-r378.output" canonical_sequence=1 canonical_created_steady_ns=8043409638200 head_observed_steady_ns=8043409654700 codec_receive_begin_steady_ns=unavailable codec_receive_end_steady_ns=unavailable
[2026-11-02 11:02:47.865] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=running detail="" description=""
[2026-11-02 11:02:48.249] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=47 source_master_ns=1716617035 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=90692
[2026-11-02 11:02:49.271] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=91 source_master_ns=2738295040 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=136038
[2026-11-02 11:02:50.292] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=135 source_master_ns=3759973044 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=181384
[2026-11-02 11:02:51.337] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=180 source_master_ns=4804871003 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=226730
[2026-11-02 11:02:52.199] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=1615050999 ntp_seconds=4002577372 ntp_fraction=854698491 arrival_ns=8047915961300
[2026-11-02 11:02:52.199] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1615050999
[2026-11-02 11:02:52.200] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:02:52.199] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=3041898516 ntp_seconds=4002577372 ntp_fraction=854698491 arrival_ns=8047915990000
[2026-11-02 11:02:52.200] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3041898516
[2026-11-02 11:02:52.200] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:02:52.200] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3453373339 ntp_seconds=4002577372 ntp_fraction=854698491 arrival_ns=8047916181000
[2026-11-02 11:02:52.201] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3453373339
[2026-11-02 11:02:52.201] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:02:52.201] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=1710618471 ntp_seconds=4002577372 ntp_fraction=858993459 arrival_ns=8047917041700
[2026-11-02 11:02:52.201] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1710618471
[2026-11-02 11:02:52.201] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:02:52.359] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=224 source_master_ns=5826549008 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=272076
[2026-11-02 11:02:53.381] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=268 source_master_ns=6848227012 raw_phase_ns=22674 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=317422
[2026-11-02 11:02:54.402] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=312 source_master_ns=7869905017 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=362768
[2026-11-02 11:02:55.447] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=357 source_master_ns=8914802976 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=408114
[2026-11-02 11:02:56.469] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=401 source_master_ns=9936480981 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=453460
[2026-11-02 11:02:57.212] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3453373339 ntp_seconds=4002577377 ntp_fraction=906238099 arrival_ns=8052928588700
[2026-11-02 11:02:57.212] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3453373339
[2026-11-02 11:02:57.212] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:02:57.226] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=1710618471 ntp_seconds=4002577377 ntp_fraction=970662608 arrival_ns=8052942828400
[2026-11-02 11:02:57.226] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1710618471
[2026-11-02 11:02:57.226] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=3041898516 ntp_seconds=4002577377 ntp_fraction=970662608 arrival_ns=8052942950900
[2026-11-02 11:02:57.226] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3041898516
[2026-11-02 11:02:57.226] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:02:57.226] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=1615050999 ntp_seconds=4002577377 ntp_fraction=970662608 arrival_ns=8052943440600
[2026-11-02 11:02:57.227] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1615050999
[2026-11-02 11:02:57.227] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:02:57.227] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:02:57.491] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=445 source_master_ns=10958158985 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=498806
[2026-11-02 11:02:58.536] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=490 source_master_ns=12003056944 raw_phase_ns=22674 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=544152
[2026-11-02 11:02:59.557] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=534 source_master_ns=13024734949 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=589498
[2026-11-02 11:03:00.579] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=578 source_master_ns=14046412953 raw_phase_ns=22674 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=634844
[2026-11-02 11:03:01.601] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=622 source_master_ns=15068090958 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=680190
[2026-11-02 11:03:02.241] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3453373339 ntp_seconds=4002577382 ntp_fraction=1030792151 arrival_ns=8057957733400
[2026-11-02 11:03:02.241] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3453373339
[2026-11-02 11:03:02.241] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=1710618471 ntp_seconds=4002577382 ntp_fraction=1035087118 arrival_ns=8057957810500
[2026-11-02 11:03:02.242] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1710618471
[2026-11-02 11:03:02.241] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:03:02.241] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=3041898516 ntp_seconds=4002577382 ntp_fraction=1030792151 arrival_ns=8057957733200
[2026-11-02 11:03:02.242] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:03:02.242] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=1615050999 ntp_seconds=4002577382 ntp_fraction=1035087118 arrival_ns=8057957733100
[2026-11-02 11:03:02.243] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1615050999
[2026-11-02 11:03:02.243] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:03:02.242] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3041898516
[2026-11-02 11:03:02.243] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:03:02.646] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=667 source_master_ns=16112988917 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=725536
[2026-11-02 11:03:03.667] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=711 source_master_ns=17134666922 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=770882
[2026-11-02 11:03:04.689] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=755 source_master_ns=18156344926 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=816228
[2026-11-02 11:03:05.734] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=800 source_master_ns=19201242885 raw_phase_ns=22674 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=861574
[2026-11-02 11:03:06.755] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=844 source_master_ns=20222920890 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=906920
[2026-11-02 11:03:07.249] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3453373339 ntp_seconds=4002577387 ntp_fraction=1069446856 arrival_ns=8062965733400
[2026-11-02 11:03:07.249] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3453373339
[2026-11-02 11:03:07.249] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:03:07.264] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=3041898516 ntp_seconds=4002577387 ntp_fraction=1129576398 arrival_ns=8062980703300
[2026-11-02 11:03:07.264] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3041898516
[2026-11-02 11:03:07.264] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=1615050999 ntp_seconds=4002577387 ntp_fraction=1133871366 arrival_ns=8062980796500
[2026-11-02 11:03:07.264] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1615050999
[2026-11-02 11:03:07.264] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:03:07.264] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=1710618471 ntp_seconds=4002577387 ntp_fraction=1129576398 arrival_ns=8062980652600
[2026-11-02 11:03:07.264] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:03:07.265] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1710618471
[2026-11-02 11:03:07.265] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-11-02 11:03:07.777] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=888 source_master_ns=21244598895 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=952266
[2026-11-02 11:03:08.799] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=932 source_master_ns=22266276899 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=997612
[2026-11-02 11:03:09.844] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=977 source_master_ns=23311174858 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1042958
[2026-11-02 11:03:10.865] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1021 source_master_ns=24332852863 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1088304
[2026-11-02 11:03:11.778] [info] [graph][runtime.node] datagram_reservation_trace stage=reservation_failed generation=1 endpoint=1 sequence=17207 release_master_ns=24527008668 deadline_master_ns=24593641256 original_protocol_deadline_master_ns=24627008668 prefix_completion_limit_master_ns=24593641256 activation_started_master_ns=24520808500 activation_completed_master_ns=24520809700 permit_started_master_ns=24593365000 permit_completed_master_ns=24593395900 snapshot_started_master_ns=24593396000 snapshot_sampled_master_ns=24593396100 snapshot_completed_master_ns=24593396300 reservation_completed_master_ns=24593460900 wait_started_master_ns=unavailable wait_completed_master_ns=unavailable submit_observed_master_ns=unavailable wait_attempts=0 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=0 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=0 wait_call_total_ns=0 wait_call_max_ns=0 wait_before_call_count=0 wait_before_call_total_ns=0 wait_before_call_max_ns=0 wait_arm_count=0 wait_arm_total_ns=0 wait_arm_max_ns=0 wait_api_wall_count=0 wait_api_wall_total_ns=0 wait_api_wall_max_ns=0 wait_return_to_next_clock_count=0 wait_return_to_next_clock_total_ns=0 wait_return_to_next_clock_max_ns=0 wait_target_lateness_count=0 wait_target_lateness_total_ns=0 wait_target_lateness_max_ns=0 original_error="IoFailure: Datagram GBRA reservation misses its immutable completion deadline sequence=17207 now_ns=24593396100 release_ns=24527008668 deadline_ns=24593641256 not_before_ns=24593577240 not_after_ns=24593426216 duration_ns=215040 rate=6250000 queue_wire_bytes=4812 average_queue_residence_ns=72912200"
[2026-11-02 11:03:11.778] [info] [graph][runtime.node] process.failed node=81 name=MediaScheduledDatagramSenderNode kind=ScheduledDatagramSender inputs=[channel=111 edge=111 stream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=123 edge=123 stream=Metadata edge_kind=Unknown payload=Unknown queued=0] outputs=[] error=IoFailure: Datagram GBRA reservation misses its immutable completion deadline sequence=17207 now_ns=24593396100 release_ns=24527008668 deadline_ns=24593641256 not_before_ns=24593577240 not_after_ns=24593426216 duration_ns=215040 rate=6250000 queue_wire_bytes=4812 average_queue_residence_ns=72912200
[2026-11-02 11:03:11.778] [info] [graph][runtime.node] worker.failed node=81 kind=ScheduledDatagramSender name=Common GCRA-paced nonblocking datagram sender error=IoFailure: Datagram GBRA reservation misses its immutable completion deadline sequence=17207 now_ns=24593396100 release_ns=24527008668 deadline_ns=24593641256 not_before_ns=24593577240 not_after_ns=24593426216 duration_ns=215040 rate=6250000 queue_wire_bytes=4812 average_queue_residence_ns=72912200
[2026-11-02 11:03:11.778] [info] [graph][runtime.node] worker.thread_cpu node=2 available=1 cpu_ns=296875000 process_calls=2176
[2026-11-02 11:03:11.778] [info] [graph][runtime.node] worker.thread_cpu node=8 available=1 cpu_ns=62500000 process_calls=13903
[2026-11-02 11:03:11.778] [info] [graph][runtime.node] worker.thread_cpu node=27 available=1 cpu_ns=62500000 process_calls=19236
[2026-11-02 11:03:11.778] [info] [graph][runtime.node] worker.thread_cpu node=46 available=1 cpu_ns=31250000 process_calls=22169
[2026-11-02 11:03:11.778] [info] [graph][runtime.node] worker.thread_cpu node=47 available=1 cpu_ns=93750000 process_calls=21553
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=9 available=1 cpu_ns=0 process_calls=15
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=28 available=1 cpu_ns=0 process_calls=16
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=4 available=1 cpu_ns=640625000 process_calls=3283
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=48 available=1 cpu_ns=0 process_calls=23
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=10 available=1 cpu_ns=0 process_calls=20
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=29 available=1 cpu_ns=0 process_calls=20
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=49 available=1 cpu_ns=0 process_calls=44
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=11 available=1 cpu_ns=62500000 process_calls=2232
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=12 available=1 cpu_ns=0 process_calls=16
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=77 available=1 cpu_ns=0 process_calls=7
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=5 available=1 cpu_ns=46875000 process_calls=3117
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=30 available=1 cpu_ns=31250000 process_calls=2226
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=74 available=1 cpu_ns=31250000 process_calls=5249
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=31 available=1 cpu_ns=0 process_calls=16
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=50 available=1 cpu_ns=15625000 process_calls=2237
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=75 available=1 cpu_ns=0 process_calls=5255
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=51 available=1 cpu_ns=0 process_calls=3198
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=78 available=1 cpu_ns=62500000 process_calls=5203
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=52 available=1 cpu_ns=0 process_calls=27
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=13 available=1 cpu_ns=0 process_calls=20
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=76 available=1 cpu_ns=406250000 process_calls=10668
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=32 available=1 cpu_ns=0 process_calls=20
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=53 available=1 cpu_ns=0 process_calls=44
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=79 available=1 cpu_ns=296875000 process_calls=8508
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=14 available=1 cpu_ns=15625000 process_calls=2255
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=33 available=1 cpu_ns=15625000 process_calls=2258
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=17 available=1 cpu_ns=0 process_calls=15
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=36 available=1 cpu_ns=0 process_calls=15
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=55 available=1 cpu_ns=31250000 process_calls=3229
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=54 available=1 cpu_ns=31250000 process_calls=2258
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=59 available=1 cpu_ns=0 process_calls=27
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=15 available=1 cpu_ns=31250000 process_calls=2248
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=34 available=1 cpu_ns=31250000 process_calls=2265
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=57 available=1 cpu_ns=31250000 process_calls=3208
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=56 available=1 cpu_ns=15625000 process_calls=2242
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=16 available=1 cpu_ns=31250000 process_calls=2260
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=58 available=1 cpu_ns=31250000 process_calls=5473
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=35 available=1 cpu_ns=31250000 process_calls=2266
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=18 available=1 cpu_ns=15625000 process_calls=2206
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=37 available=1 cpu_ns=15625000 process_calls=2211
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=60 available=1 cpu_ns=46875000 process_calls=5363
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=38 available=1 cpu_ns=0 process_calls=2228
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=19 available=1 cpu_ns=15625000 process_calls=2215
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=61 available=1 cpu_ns=93750000 process_calls=5335
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=39 available=1 cpu_ns=15625000 process_calls=2258
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=20 available=1 cpu_ns=0 process_calls=2264
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=62 available=1 cpu_ns=46875000 process_calls=5417
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=22 available=1 cpu_ns=250000000 process_calls=2281
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=41 available=1 cpu_ns=265625000 process_calls=2296
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=70 available=1 cpu_ns=62500000 process_calls=4932
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=64 available=1 cpu_ns=265625000 process_calls=3351
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=23 available=1 cpu_ns=15625000 process_calls=2282
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=42 available=1 cpu_ns=15625000 process_calls=2282
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=65 available=1 cpu_ns=62500000 process_calls=2282
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=24 available=1 cpu_ns=31250000 process_calls=2249
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=71 available=1 cpu_ns=0 process_calls=3464
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=43 available=1 cpu_ns=46875000 process_calls=2249
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=66 available=1 cpu_ns=62500000 process_calls=2250
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=72 available=1 cpu_ns=31250000 process_calls=3344
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=45 available=1 cpu_ns=265625000 process_calls=2908
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=26 available=1 cpu_ns=265625000 process_calls=2900
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=68 available=1 cpu_ns=281250000 process_calls=2887
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=25 available=1 cpu_ns=46875000 process_calls=2281
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=73 available=1 cpu_ns=93750000 process_calls=5239
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=44 available=1 cpu_ns=0 process_calls=2281
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=67 available=1 cpu_ns=31250000 process_calls=2268
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=6 available=1 cpu_ns=406250000 process_calls=4539
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=80 available=1 cpu_ns=0 process_calls=5
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=7 available=1 cpu_ns=203125000 process_calls=4385
[2026-11-02 11:03:11.779] [info] [graph][runtime.node] worker.thread_cpu node=81 available=1 cpu_ns=1281250000 process_calls=5183
[2026-11-02 11:03:11.790] [info] [graph][runtime.node] datagram_batch_trace stage=last_completed generation=1 first_sequence=17160 last_sequence=17160 maximum_submit_interval_ns=67500 maximum_submit_interval_sequence=17160 maximum_start_lateness_ns=504481 maximum_start_lateness_sequence=17160 first_release_master_ns=24513221819 first_deadline_master_ns=24593641256 last_release_master_ns=24513221819 last_deadline_master_ns=24593641256 first_submitted_master_ns=24513793800 last_submitted_master_ns=24513793800 worst_wait_started_master_ns=24507629300 worst_wait_completed_master_ns=24513724400 worst_submit_observed_master_ns=24513726300 wait_attempts=1 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=1 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=1 wait_call_total_ns=6093200 wait_call_max_ns=6093200 wait_before_call_count=1 wait_before_call_total_ns=0 wait_before_call_max_ns=0 wait_arm_count=1 wait_arm_total_ns=500 wait_arm_max_ns=500 wait_api_wall_count=1 wait_api_wall_total_ns=6090900 wait_api_wall_max_ns=6090900 wait_return_to_next_clock_count=1 wait_return_to_next_clock_total_ns=2000 wait_return_to_next_clock_max_ns=2000 wait_target_lateness_count=1 wait_target_lateness_total_ns=502581 wait_target_lateness_max_ns=502581 worst_wait_metric=call_wall worst_wait_sequence=17160 worst_wait_target_master_ns=24513221819 worst_wait_sampled_master_ns=24507629300 worst_wait_sampled_steady_ns=8067409153800 worst_wait_timeout_ns=5592519 worst_wait_entered_steady_ns=8067409153800 worst_wait_returned_steady_ns=8067415247000 worst_wait_arm_before_steady_ns=8067409154000 worst_wait_arm_after_steady_ns=8067409154500 worst_wait_api_before_steady_ns=8067409154500 worst_wait_api_returned_steady_ns=8067415245400 worst_wait_windows_due_100ns=-55926 worst_wait_windows_result=1 worst_wait_windows_error=unavailable worst_wait_windows_pid=14840 worst_wait_windows_tid=16480 worst_wait_windows_qpc_frequency_hz=10000000 worst_wait_next_master_ns=24513724400 worst_wait_next_steady_ns=8067415249000 worst_wait_outcome=deadline
[2026-11-02 11:03:11.790] [info] [graph][runtime.node] datagram_batch_trace stage=active generation=1 first_sequence=17161 last_sequence=17210 maximum_submit_interval_ns=114700 maximum_submit_interval_sequence=17206 maximum_start_lateness_ns=3404660 maximum_start_lateness_sequence=17204 first_release_master_ns=24527008668 first_deadline_master_ns=24593641256 last_release_master_ns=24527008668 last_deadline_master_ns=24593641256 first_submitted_master_ns=24527266200 last_submitted_master_ns=24593362200 worst_wait_started_master_ns=24585646400 worst_wait_completed_master_ns=24589259600 worst_submit_observed_master_ns=24589260400 wait_attempts=46 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=46 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=46 wait_call_total_ns=70655100 wait_call_max_ns=6423600 wait_before_call_count=46 wait_before_call_total_ns=3100 wait_before_call_max_ns=200 wait_arm_count=46 wait_arm_total_ns=403200 wait_arm_max_ns=69800 wait_api_wall_count=46 wait_api_wall_total_ns=70222700 wait_api_wall_max_ns=6422000 wait_return_to_next_clock_count=46 wait_return_to_next_clock_total_ns=28600 wait_return_to_next_clock_max_ns=1900 wait_target_lateness_count=46 wait_target_lateness_total_ns=55115732 wait_target_lateness_max_ns=3403860 worst_wait_metric=call_wall worst_wait_sequence=17161 worst_wait_target_master_ns=24527008668 worst_wait_sampled_master_ns=24520811900 worst_wait_sampled_steady_ns=8067422336500 worst_wait_timeout_ns=6196768 worst_wait_entered_steady_ns=8067422336500 worst_wait_returned_steady_ns=8067428760100 worst_wait_arm_before_steady_ns=8067422336800 worst_wait_arm_after_steady_ns=8067422337700 worst_wait_api_before_steady_ns=8067422337700 worst_wait_api_returned_steady_ns=8067428759700 worst_wait_windows_due_100ns=-61968 worst_wait_windows_result=1 worst_wait_windows_error=unavailable worst_wait_windows_pid=14840 worst_wait_windows_tid=16480 worst_wait_windows_qpc_frequency_hz=10000000 worst_wait_next_master_ns=24527236500 worst_wait_next_steady_ns=8067428761100 worst_wait_outcome=deadline
[2026-11-02 11:03:11.790] [info] [graph][runtime.node] datagram_successful_submit_trace stage=worst_success generation=1 endpoint=1 sequence=6 pacing_lateness_ns=7919700 matches_pacing_worst=1 materialized_master_ns=unavailable enqueued_master_ns=unavailable submit_interval_scope=pre_transport_checks_and_transport socket_only_ns=unavailable release_master_ns=527008668 deadline_master_ns=627008668 original_protocol_deadline_master_ns=627008668 prefix_completion_limit_master_ns=627008668 reserved_at_master_ns=535301000 not_before_master_ns=527383900 not_after_master_ns=626913948 service_duration_ns=94720 wire_bytes_per_second=6250000 submit_started_master_ns=535303600 submit_completed_master_ns=535358600 activation_started_master_ns=535299400 activation_completed_master_ns=535300600 permit_started_master_ns=535300800 permit_completed_master_ns=535300900 snapshot_started_master_ns=535300900 snapshot_sampled_master_ns=535301000 snapshot_completed_master_ns=535301300 reservation_completed_master_ns=535302400 wait_started_master_ns=535303100 wait_completed_master_ns=535303100 submit_observed_master_ns=535303600 wait_attempts=0 wait_incomplete=0 wait_notified_count=0 wait_deadline_count=0 wait_interrupted_count=0 wait_unavailable_count=0 wait_call_count=0 wait_call_total_ns=0 wait_call_max_ns=0 wait_before_call_count=0 wait_before_call_total_ns=0 wait_before_call_max_ns=0 wait_arm_count=0 wait_arm_total_ns=0 wait_arm_max_ns=0 wait_api_wall_count=0 wait_api_wall_total_ns=0 wait_api_wall_max_ns=0 wait_return_to_next_clock_count=0 wait_return_to_next_clock_total_ns=0 wait_return_to_next_clock_max_ns=0 wait_target_lateness_count=0 wait_target_lateness_total_ns=0 wait_target_lateness_max_ns=0
[2026-11-02 11:03:11.790] [info] [graph][runtime.node] scheduled_datagram_sender stage=failed generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=2599 committed_datagrams=17206 committed_payload_bytes=20665148 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=79296 pacing_reserved=17206 pacing_cancelled=0 pacing_submitted=17206 pacing_maximum_submit_lateness_ns=7919700 pacing_worst_late_global_sequence=6 pacing_worst_success_sample_available=1 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=4 backlog_current_wire_bytes=4812 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=79296 backlog_max_residence_ns=72878300 last_materialized_sequence=17210 last_scheduled_sequence=17210 last_submitted_sequence=17206 last_committed_sequence=17206 submitted_service_generation=1 submitted_service_sequence=17206 submitted_service_completed_ns=24593362200 submitted_service_duration_ns=215040 submitted_service_available_ns=24593577240 submitted_scope_available_ns=24593577240 shared_scope_charged_datagrams=17206 shared_scope_charged_wire_bytes=21146916 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2154 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2154 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=188010 endpoint_1_target_effective_socket_bytes=188010 endpoint_1_api_requested_socket_bytes=188010 endpoint_1_effective_socket_bytes=188010 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=17206 endpoint_1_committed_payload_bytes=20665148
[2026-11-02 11:03:11.790] [info] [graph][runtime.node] worker.secondary_failure node=81 kind=ScheduledDatagramSender name=Common GCRA-paced nonblocking datagram sender error=IoFailure: Datagram GBRA reservation misses its immutable completion deadline sequence=17207 now_ns=24593396100 release_ns=24527008668 deadline_ns=24593641256 not_before_ns=24593577240 not_after_ns=24593426216 duration_ns=215040 rate=6250000 queue_wire_bytes=4812 average_queue_residence_ns=72912200
[2026-11-02 11:03:12.035] [info] [graph][runtime.lifecycle] node_abort_payload node=81 before_objects=197 after_objects=197 before_bytes=351426 after_bytes=351426 before_reservations=23066 after_reservations=23066 before_releases=22869 after_releases=22869
[2026-11-02 11:03:12.036] [info] [graph][runtime.node] mpegts_wire_materializer stage=aborted maximum_materialized_after_release_ns=33281892 worst_materialized_at_ns=22560291000 worst_release_ns=22527009108 worst_deadline_ns=22593675776 worst_global_sequence=15760
[2026-11-02 11:03:12.036] [info] [graph][runtime.lifecycle] node_abort_payload node=79 before_objects=197 after_objects=197 before_bytes=351426 after_bytes=351426 before_reservations=23066 after_reservations=23066 before_releases=22869 after_releases=22869
[2026-11-02 11:03:12.036] [info] [graph][runtime.node] au_processing_trace stage=pending_au_abort generation=1 stream=video canonical_sequence=721 emit_master_ns=24527008668 dispatch_master_ns=24627008668 ready_master_ns=24520376400 scheduled_created_steady_ns=8067421553500 adapter_dequeued_steady_ns=8067421871400 adapted_created_steady_ns=8067421872600 write_entered_steady_ns=8067421899600 session_ready_steady_ns=8067421901000 codec_receive_begin_steady_ns=8067366811800 codec_receive_end_steady_ns=8067366812400 codec_encoder_node=2 codec_selected_token_id=721 codec_selected_token_generation=1 codec_packet_token_id=721 codec_packet_token_generation=1 codec_token_mapping=copy_opaque codec_input_pts=720 codec_timebase_num=1 codec_timebase_den=30 codec_accepted_send_count=1 codec_token_output_ordinal=1 codec_packet_pts=720 codec_packet_dts=720 codec_packet_bytes=63554 codec_identity_incomplete=0
[2026-11-02 11:03:12.036] [info] [graph][runtime.node] ts_prefix_commit_summary stage=current_au_abort generation=1 stream=video canonical_sequence=721 emit_master_ns=24527008668 dispatch_master_ns=24627008668 published=1 completed=0 samples=0 would_block=8 incomplete=0 publish_to_protocol_commit_completed_total_ns=0 publish_to_protocol_commit_completed_max_ns=0 publish_to_protocol_commit_completed_max_prefix_offset=0 protocol_commit_completed_to_mux_resume_total_ns=0 protocol_commit_completed_to_mux_resume_max_ns=0 protocol_commit_completed_to_mux_resume_max_prefix_offset=0
[2026-11-02 11:03:12.037] [info] [graph][runtime.node] au_processing_trace stage=previous_completed_au_trace_on_abort generation=1 stream=audio canonical_sequence=1034 emit_master_ns=24513221819 dispatch_master_ns=24613221819 ready_master_ns=24507568700 scheduled_created_steady_ns=8067395575700 adapter_dequeued_steady_ns=8067395630200 adapted_created_steady_ns=8067395630400 write_entered_steady_ns=8067409091900 session_ready_steady_ns=8067409093200 codec_identity=unavailable
[2026-11-02 11:03:12.037] [info] [graph][runtime.node] ts_prefix_commit_summary stage=previous_completed_au_on_abort generation=1 stream=audio canonical_sequence=1034 emit_master_ns=24513221819 dispatch_master_ns=24613221819 published=1 completed=1 samples=1 would_block=2 incomplete=0 publish_to_protocol_commit_completed_total_ns=6211400 publish_to_protocol_commit_completed_max_ns=6211400 publish_to_protocol_commit_completed_max_prefix_offset=0 protocol_commit_completed_to_mux_resume_total_ns=19300 protocol_commit_completed_to_mux_resume_max_ns=19300 protocol_commit_completed_to_mux_resume_max_prefix_offset=0
[2026-11-02 11:03:12.037] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 produced_datagrams=17210 produced_ts_payload_bytes=20669848 pending_bytes=0 peak_pending_bytes=92872 access_units=1755 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=20226759 maximum_mux_input_after_emit_ns=26388579 maximum_mux_input_after_dispatch_ns=-73611421 worst_mux_input_ready_ns=12558113800 worst_mux_input_emit_ns=12531725221 worst_mux_input_dispatch_ns=12631725221 maximum_protocol_batch_produced_after_release_ns=33246392 worst_protocol_batch_produced_ns=22560255500 worst_protocol_batch_release_ns=22527009108 worst_protocol_batch_deadline_ns=22593675776 worst_protocol_batch_datagrams=1 exit_reason=aborted
[2026-11-02 11:03:12.037] [info] [graph][runtime.lifecycle] node_abort_payload node=76 before_objects=197 after_objects=197 before_bytes=351426 after_bytes=351426 before_reservations=23066 after_reservations=23066 before_releases=22869 after_releases=22869
[2026-11-02 11:03:12.037] [info] [graph][runtime.lifecycle] node_abort_payload node=78 before_objects=197 after_objects=197 before_bytes=351426 after_bytes=351426 before_reservations=23066 after_reservations=23066 before_releases=22869 after_releases=22869
[2026-11-02 11:03:12.038] [info] [graph][runtime.lifecycle] node_abort_payload node=75 before_objects=197 after_objects=197 before_bytes=351426 after_bytes=351426 before_reservations=23066 after_reservations=23066 before_releases=22869 after_releases=22869
[2026-11-02 11:03:12.038] [info] [graph][runtime.node] audio_stage_observation node=74 stage=scheduler_input count=1038 last_generation=1 last_begin=1061888 last_end=1062912 last_rate=44100 last_steady_ns=8067490718200 maximum_inter_event_gap_ns=34193600 gap_begin_generation=1 gap_begin_begin=201728 gap_begin_end=202752 gap_begin_rate=44100 gap_begin_steady_ns=8047983298400 gap_end_generation=1 gap_end_begin=202752 gap_end_end=203776 gap_end_rate=44100 gap_end_steady_ns=8048017492000
[2026-11-02 11:03:12.038] [info] [graph][runtime.node] av_scheduler_correlated_wait generation=1 canonical_sequence=1 missing=audio begin_steady_ns=8043370881600 end_steady_ns=8043409791800 observed_wait_ns=38910200 held_dispatch_master_ns=627008668 begin_master_ns=469356700 end_master_ns=508266600 dispatch_remaining_at_end_ns=118742068 resumed_dispatch_master_ns=627008668
[2026-11-02 11:03:12.038] [info] [graph][runtime.lifecycle] node_abort_payload node=74 before_objects=197 after_objects=196 before_bytes=351426 after_bytes=331035 before_reservations=23066 after_reservations=23066 before_releases=22869 after_releases=22870
[2026-11-02 11:03:12.038] [info] [graph][runtime.node] audio_stage_observation node=5 stage=canonicalizer_input count=1038 last_generation=1 last_begin=1061888 last_end=1062912 last_rate=44100 last_steady_ns=8067490688600 maximum_inter_event_gap_ns=34162500 gap_begin_generation=1 gap_begin_begin=201728 gap_begin_end=202752 gap_begin_rate=44100 gap_begin_steady_ns=8047983266000 gap_end_generation=1 gap_end_begin=202752 gap_end_end=203776 gap_end_rate=44100 gap_end_steady_ns=8048017428500
[2026-11-02 11:03:12.038] [info] [graph][runtime.node] audio_stage_observation node=5 stage=canonicalizer_committed count=1038 last_generation=1 last_begin=1061888 last_end=1062912 last_rate=44100 last_steady_ns=8067490696500 maximum_inter_event_gap_ns=34166000 gap_begin_generation=1 gap_begin_begin=201728 gap_begin_end=202752 gap_begin_rate=44100 gap_begin_steady_ns=8047983280400 gap_end_generation=1 gap_end_begin=202752 gap_end_end=203776 gap_end_rate=44100 gap_end_steady_ns=8048017446400
[2026-11-02 11:03:12.038] [info] [graph][runtime.lifecycle] node_abort_payload node=5 before_objects=196 after_objects=196 before_bytes=331035 after_bytes=331035 before_reservations=23066 after_reservations=23066 before_releases=22870 after_releases=22870
[2026-11-02 11:03:12.039] [info] [graph][runtime.lifecycle] node_abort_payload node=77 before_objects=196 after_objects=196 before_bytes=331035 after_bytes=331035 before_reservations=23066 after_reservations=23066 before_releases=22870 after_releases=22870
[2026-11-02 11:03:12.039] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_input count=1040 last_generation=1 last_begin=1063936 last_end=1064960 last_rate=44100 last_steady_ns=8067490389300 maximum_inter_event_gap_ns=33671400 gap_begin_generation=1 gap_begin_begin=203776 gap_begin_end=204800 gap_begin_rate=44100 gap_begin_steady_ns=8047982768700 gap_end_generation=1 gap_end_begin=204800 gap_end_end=205824 gap_end_rate=44100 gap_end_steady_ns=8048016440100
[2026-11-02 11:03:12.039] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_submitted count=1040 last_generation=1 last_begin=1063936 last_end=1064960 last_rate=44100 last_steady_ns=8067490669400 maximum_inter_event_gap_ns=34140500 gap_begin_generation=1 gap_begin_begin=203776 gap_begin_end=204800 gap_begin_rate=44100 gap_begin_steady_ns=8047983234300 gap_end_generation=1 gap_end_begin=204800 gap_end_end=205824 gap_end_rate=44100 gap_end_steady_ns=8048017374800
[2026-11-02 11:03:12.039] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_packet count=1038 last_generation=1 last_begin=1061888 last_end=1062912 last_rate=44100 last_steady_ns=8067490674600 maximum_inter_event_gap_ns=34147100 gap_begin_generation=1 gap_begin_begin=201728 gap_begin_end=202752 gap_begin_rate=44100 gap_begin_steady_ns=8047983244200 gap_end_generation=1 gap_end_begin=202752 gap_end_end=203776 gap_end_rate=44100 gap_end_steady_ns=8048017391300
[2026-11-02 11:03:12.039] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_committed count=1038 last_generation=1 last_begin=1061888 last_end=1062912 last_rate=44100 last_steady_ns=8067490680300 maximum_inter_event_gap_ns=34153400 gap_begin_generation=1 gap_begin_begin=201728 gap_begin_end=202752 gap_begin_rate=44100 gap_begin_steady_ns=8047983255600 gap_end_generation=1 gap_end_begin=202752 gap_end_end=203776 gap_end_rate=44100 gap_end_steady_ns=8048017409000
[2026-11-02 11:03:12.039] [info] [graph][runtime.lifecycle] codec_owner_release node=4 kind=AudioEncode owner_references=4
[2026-11-02 11:03:12.040] [info] [graph][runtime.lifecycle] node_abort_payload node=4 before_objects=196 after_objects=196 before_bytes=331035 after_bytes=331035 before_reservations=23066 after_reservations=23066 before_releases=22870 after_releases=22870
[2026-11-02 11:03:12.040] [info] [graph][runtime.lifecycle] node_abort_payload node=80 before_objects=196 after_objects=196 before_bytes=331035 after_bytes=331035 before_reservations=23066 after_reservations=23066 before_releases=22870 after_releases=22870
[2026-11-02 11:03:12.040] [info] [graph][runtime.node] audio_stage_observation node=7 stage=aggregate_built count=1040 last_generation=1 last_begin=1063936 last_end=1064960 last_rate=44100 last_steady_ns=8067490355900 maximum_inter_event_gap_ns=33625300 gap_begin_generation=1 gap_begin_begin=203776 gap_begin_end=204800 gap_begin_rate=44100 gap_begin_steady_ns=8047982733300 gap_end_generation=1 gap_end_begin=204800 gap_end_end=205824 gap_end_rate=44100 gap_end_steady_ns=8048016358600
[2026-11-02 11:03:12.040] [info] [graph][runtime.node] audio_stage_observation node=7 stage=aggregate_committed count=1040 last_generation=1 last_begin=1063936 last_end=1064960 last_rate=44100 last_steady_ns=8067490709600 maximum_inter_event_gap_ns=34726900 gap_begin_generation=1 gap_begin_begin=203776 gap_begin_end=204800 gap_begin_rate=44100 gap_begin_steady_ns=8047982745400 gap_end_generation=1 gap_end_begin=204800 gap_end_end=205824 gap_end_rate=44100 gap_end_steady_ns=8048017472300
[2026-11-02 11:03:12.040] [info] [graph][runtime.node] continuous_audio_aggregate.summary reason=abort output_group=composition-r378.output audio_commits=1040 real_samples=1064960 silence_samples=0
[2026-11-02 11:03:12.040] [info] [graph][runtime.lifecycle] node_abort_payload node=7 before_objects=196 after_objects=190 before_bytes=331035 after_bytes=281883 before_reservations=23066 after_reservations=23066 before_releases=22870 after_releases=22876
[2026-11-02 11:03:12.040] [info] [graph][runtime.lifecycle] codec_owner_release node=2 kind=VideoEncode owner_references=4
[2026-11-02 11:03:12.040] [info] [graph][runtime.node] video_encoder_execution_observation node=2 count=724 maximum_call_ns=8513900 maximum_begin_steady_ns=8064463317700 maximum_end_steady_ns=8064471831600 maximum_encoder_pts=633 maximum_native_result=0 maximum_between_submissions_ns=39094800 before_gap_begin_steady_ns=8047829124300 before_gap_end_steady_ns=8047830607600 before_gap_encoder_pts=134 before_gap_native_result=0 after_gap_begin_steady_ns=8047869702400 after_gap_end_steady_ns=8047871209600 after_gap_encoder_pts=135 after_gap_native_result=0
[2026-11-02 11:03:12.041] [info] [graph][runtime.lifecycle] node_abort_payload node=2 before_objects=190 after_objects=190 before_bytes=281883 after_bytes=281883 before_reservations=23066 after_reservations=23066 before_releases=22876 after_releases=22876
[2026-11-02 11:03:12.041] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=process count=4539 maximum_ns=10433100 begin_steady_ns=8046602326900 end_steady_ns=8046612760000 video_cursor=98 audio_cursor=not_owned next_video_cursor=98 next_audio_cursor=not_owned error=None return_state=waiting deadline_ns=3727008666 deadline_clock=master wake_policy=0 pending_at_entry=0 entered_onprocess=1
[2026-11-02 11:03:12.041] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=purge count=4539 maximum_ns=48000 begin_steady_ns=8051299742600 end_steady_ns=8051299790600 video_cursor=239 audio_cursor=not_owned next_video_cursor=239 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 11:03:12.041] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=bind count=4539 maximum_ns=259600 begin_steady_ns=8049730309000 end_steady_ns=8049730568600 video_cursor=192 audio_cursor=not_owned next_video_cursor=192 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 11:03:12.041] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=consume count=4539 maximum_ns=10430900 begin_steady_ns=8046602327700 end_steady_ns=8046612758600 video_cursor=98 audio_cursor=not_owned next_video_cursor=98 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 11:03:12.041] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=activate count=23 maximum_ns=327600 begin_steady_ns=8043361866000 end_steady_ns=8043362193600 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 11:03:12.041] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=build_video count=724 maximum_ns=7153400 begin_steady_ns=8043395354600 end_steady_ns=8043402508000 video_cursor=1 audio_cursor=not_owned next_video_cursor=1 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 11:03:12.041] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=canvas count=724 maximum_ns=7130900 begin_steady_ns=8043395363700 end_steady_ns=8043402494600 video_cursor=1 audio_cursor=not_owned next_video_cursor=1 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 11:03:12.042] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=emit count=724 maximum_ns=4860900 begin_steady_ns=8050865469000 end_steady_ns=8050870329900 video_cursor=225 audio_cursor=not_owned next_video_cursor=225 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-11-02 11:03:12.042] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=between_calls_previous_return count=4538 maximum_ns=87315900 begin_steady_ns=8043270128200 end_steady_ns=8043357444100 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=waiting deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-11-02 11:03:12.042] [info] [graph][runtime.node] continuous_aggregate.summary reason=abort output_group=composition-r378.output output_generation=1 video_commits=724 source[0].group=composition-r378.source.0 source[0].identity=composition-r378.source.0.input.video source[0].real_frames=724 source[0].black_frames=0 source[0].contribution_generation=1 source[1].group=composition-r378.source.1 source[1].identity=composition-r378.source.1.input.video source[1].real_frames=724 source[1].black_frames=0 source[1].contribution_generation=1 source[2].group=composition-r378.source.2 source[2].identity=composition-r378.source.2.input.video source[2].real_frames=724 source[2].black_frames=0 source[2].contribution_generation=1
[2026-11-02 11:03:12.042] [info] [graph][runtime.node] cuda_canvas_call stage=context_push count=728 operation=copy maximum_ns=418300 begin_steady_ns=8060062596800 end_steady_ns=8060063015100 native_result=0 thread_cpu_ns=0
[2026-11-02 11:03:12.042] [info] [graph][runtime.node] cuda_canvas_call stage=source_wait count=2902 operation=copy maximum_ns=311400 begin_steady_ns=8046630500100 end_steady_ns=8046630811500 native_result=0 thread_cpu_ns=0
[2026-11-02 11:03:12.042] [info] [graph][runtime.node] cuda_canvas_call stage=plane_copy count=5806 operation=copy maximum_ns=6136700 begin_steady_ns=8043395378900 end_steady_ns=8043401515600 native_result=0 thread_cpu_ns=0
[2026-11-02 11:03:12.042] [info] [graph][runtime.node] cuda_canvas_call stage=destination_wait count=2 operation=upload maximum_ns=81500 begin_steady_ns=8043205046000 end_steady_ns=8043205127500 native_result=0 thread_cpu_ns=0
[2026-11-02 11:03:12.042] [info] [graph][runtime.node] cuda_canvas_call stage=context_pop count=728 operation=copy maximum_ns=217700 begin_steady_ns=8043364097300 end_steady_ns=8043364315000 native_result=0 thread_cpu_ns=0
[2026-11-02 11:03:12.043] [info] [graph][runtime.node] cuda_canvas_call stage=batch_completion count=726 operation=copy maximum_ns=4201500 begin_steady_ns=8052595868100 end_steady_ns=8052600069600 native_result=0 thread_cpu_ns=0
[2026-11-02 11:03:12.043] [info] [graph][runtime.node] canvas_compose_correlated scope=attempt begin_steady_ns=8043395364600 end_steady_ns=8043402494100 wall_ns=7129500 thread_cpu_ns=0 surface_count=1 surface_total_ns=800 surface_maximum_ns=800 surface_maximum_tile=none surface_maximum_result=unavailable validate_count=3 validate_total_ns=1300 validate_maximum_ns=800 validate_maximum_tile=0 validate_maximum_result=0 black_count=1 black_total_ns=6252100 black_maximum_ns=6252100 black_maximum_tile=none black_maximum_result=0 tile_count=3 tile_total_ns=809800 tile_maximum_ns=491800 tile_maximum_tile=2 tile_maximum_result=0 publish_count=1 publish_total_ns=4200 publish_maximum_ns=4200 publish_maximum_tile=none publish_maximum_result=0 context_push_count=1 context_push_total_ns=1900 context_push_maximum_ns=1900 context_push_maximum_tile=none context_push_maximum_result=0 source_wait_count=4 source_wait_total_ns=223500 source_wait_maximum_ns=216900 source_wait_maximum_tile=2 source_wait_maximum_result=0 plane_copy_count=8 plane_copy_total_ns=6801200 plane_copy_maximum_ns=6135100 plane_copy_maximum_tile=none plane_copy_maximum_result=0 destination_wait_count=0 destination_wait_total_ns=0 destination_wait_maximum_ns=0 destination_wait_maximum_tile=none destination_wait_maximum_result=unavailable context_pop_count=1 context_pop_total_ns=1800 context_pop_maximum_ns=1800 context_pop_maximum_tile=none context_pop_maximum_result=0 batch_completion_count=1 batch_completion_total_ns=47900 batch_completion_maximum_ns=47900 batch_completion_maximum_tile=none batch_completion_maximum_result=0 source_query_completed_count=0 source_query_ready_count=3 source_query_ready_maximum_ns=9000 source_query_ready_begin_steady_ns=8043401945200 source_query_ready_end_steady_ns=8043401954200 source_query_ready_tile=2 source_query_ready_operation=event_query source_query_ready_native=0 source_query_ready_restore_native=0 source_query_pending_count=0 source_query_unavailable_count=0 source_query_error_count=0
[2026-11-02 11:03:12.043] [info] [graph][runtime.lifecycle] node_abort_payload node=6 before_objects=190 after_objects=148 before_bytes=281883 after_bytes=281883 before_reservations=23066 after_reservations=23066 before_releases=22876 after_releases=22918
[2026-11-02 11:03:12.043] [info] [graph][runtime.node] source_video_trace_summary node_id=67 node=VideoPreparationNode generation=1 reason=abort steady_ns=8067759930200 input_count=738 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=738 published_count=737 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:03:12.043] [info] [graph][runtime.lifecycle] node_abort_payload node=67 before_objects=148 after_objects=146 before_bytes=281883 after_bytes=281883 before_reservations=23066 after_reservations=23066 before_releases=22918 after_releases=22920
[2026-11-02 11:03:12.043] [info] [graph][runtime.node] source_video_trace_summary node_id=44 node=VideoPreparationNode generation=1 reason=abort steady_ns=8067760123500 input_count=738 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=738 published_count=737 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:03:12.043] [info] [graph][runtime.lifecycle] node_abort_payload node=44 before_objects=146 after_objects=144 before_bytes=281883 after_bytes=281883 before_reservations=23066 after_reservations=23066 before_releases=22920 after_releases=22922
[2026-11-02 11:03:12.043] [info] [graph][runtime.node] source_video_trace_summary node_id=25 node=VideoPreparationNode generation=1 reason=abort steady_ns=8067760293500 input_count=738 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=738 published_count=737 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:03:12.043] [info] [graph][runtime.lifecycle] node_abort_payload node=25 before_objects=144 after_objects=142 before_bytes=281883 after_bytes=281883 before_reservations=23066 after_reservations=23066 before_releases=22922 after_releases=22924
[2026-11-02 11:03:12.043] [info] [graph][runtime.lifecycle] codec_owner_release node=73 kind=AudioResample owner_references=1
[2026-11-02 11:03:12.044] [info] [graph][runtime.lifecycle] node_abort_payload node=73 before_objects=142 after_objects=140 before_bytes=281883 after_bytes=265499 before_reservations=23066 after_reservations=23066 before_releases=22924 after_releases=22926
[2026-11-02 11:03:12.044] [info] [graph][runtime.node] source_video_trace_summary node_id=68 node=VideoFilterNode generation=1 reason=abort steady_ns=8067760867100 input_count=745 accepted_count=745 would_block_count=0 resolved_count=745 output_attempt_count=0 published_count=744 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:03:12.046] [info] [graph][runtime.lifecycle] node_abort_payload node=68 before_objects=140 after_objects=138 before_bytes=265499 after_bytes=265499 before_reservations=23066 after_reservations=23066 before_releases=22926 after_releases=22928
[2026-11-02 11:03:12.046] [info] [graph][runtime.node] source_video_trace_summary node_id=45 node=VideoFilterNode generation=1 reason=abort steady_ns=8067763152500 input_count=745 accepted_count=745 would_block_count=0 resolved_count=745 output_attempt_count=0 published_count=744 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:03:12.050] [info] [graph][runtime.lifecycle] node_abort_payload node=45 before_objects=138 after_objects=136 before_bytes=265499 after_bytes=265499 before_reservations=23066 after_reservations=23066 before_releases=22928 after_releases=22930
[2026-11-02 11:03:12.050] [info] [graph][runtime.node] source_video_trace_summary node_id=26 node=VideoFilterNode generation=1 reason=abort steady_ns=8067766948200 input_count=745 accepted_count=745 would_block_count=0 resolved_count=745 output_attempt_count=0 published_count=744 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:03:12.054] [info] [graph][runtime.lifecycle] node_abort_payload node=26 before_objects=136 after_objects=134 before_bytes=265499 after_bytes=265499 before_reservations=23066 after_reservations=23066 before_releases=22930 after_releases=22932
[2026-11-02 11:03:12.054] [info] [graph][runtime.lifecycle] node_abort_payload node=72 before_objects=134 after_objects=134 before_bytes=265499 after_bytes=265499 before_reservations=23066 after_reservations=23066 before_releases=22932 after_releases=22932
[2026-11-02 11:03:12.054] [info] [graph][runtime.node] source_video_trace_summary node_id=66 node=VideoFrameRateNode generation=1 reason=abort steady_ns=8067771256300 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=751 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:03:12.054] [info] [graph][runtime.lifecycle] node_abort_payload node=66 before_objects=134 after_objects=133 before_bytes=265499 after_bytes=265499 before_reservations=23066 after_reservations=23066 before_releases=22932 after_releases=22933
[2026-11-02 11:03:12.055] [info] [graph][runtime.node] source_video_trace_summary node_id=43 node=VideoFrameRateNode generation=1 reason=abort steady_ns=8067771763300 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=751 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:03:12.055] [info] [graph][runtime.lifecycle] node_abort_payload node=43 before_objects=133 after_objects=132 before_bytes=265499 after_bytes=265499 before_reservations=23066 after_reservations=23066 before_releases=22933 after_releases=22934
[2026-11-02 11:03:12.055] [info] [graph][runtime.node] source_video_trace_summary node_id=24 node=VideoFrameRateNode generation=1 reason=abort steady_ns=8067771911700 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=751 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:03:12.055] [info] [graph][runtime.lifecycle] node_abort_payload node=24 before_objects=132 after_objects=131 before_bytes=265499 after_bytes=265499 before_reservations=23066 after_reservations=23066 before_releases=22934 after_releases=22935
[2026-11-02 11:03:12.055] [info] [graph][runtime.lifecycle] node_abort_payload node=71 before_objects=131 after_objects=130 before_bytes=265499 after_bytes=257307 before_reservations=23066 after_reservations=23066 before_releases=22935 after_releases=22936
[2026-11-02 11:03:12.055] [info] [graph][runtime.node] source_video_trace_summary node_id=65 node=HardwareTransferNode generation=1 reason=abort steady_ns=8067772124600 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=752 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:03:12.055] [info] [graph][runtime.lifecycle] node_abort_payload node=65 before_objects=130 after_objects=130 before_bytes=257307 after_bytes=257307 before_reservations=23066 after_reservations=23066 before_releases=22936 after_releases=22936
[2026-11-02 11:03:12.055] [info] [graph][runtime.node] source_video_trace_summary node_id=42 node=HardwareTransferNode generation=1 reason=abort steady_ns=8067772246200 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=752 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:03:12.055] [info] [graph][runtime.lifecycle] node_abort_payload node=42 before_objects=130 after_objects=130 before_bytes=257307 after_bytes=257307 before_reservations=23066 after_reservations=23066 before_releases=22936 after_releases=22936
[2026-11-02 11:03:12.055] [info] [graph][runtime.node] source_video_trace_summary node_id=23 node=HardwareTransferNode generation=1 reason=abort steady_ns=8067772368100 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=752 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:03:12.055] [info] [graph][runtime.lifecycle] node_abort_payload node=23 before_objects=130 after_objects=130 before_bytes=257307 after_bytes=257307 before_reservations=23066 after_reservations=23066 before_releases=22936 after_releases=22936
[2026-11-02 11:03:12.055] [info] [graph][runtime.lifecycle] codec_owner_release node=70 kind=AudioDecode owner_references=2
[2026-11-02 11:03:12.056] [info] [graph][runtime.lifecycle] node_abort_payload node=70 before_objects=130 after_objects=129 before_bytes=257307 after_bytes=249115 before_reservations=23066 after_reservations=23066 before_releases=22936 after_releases=22937
[2026-11-02 11:03:12.056] [info] [graph][runtime.node] source_video_trace_summary node_id=64 node=VideoDecodeNode generation=1 reason=abort steady_ns=8067772665000 input_count=752 accepted_count=752 would_block_count=0 resolved_count=752 output_attempt_count=0 published_count=752 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:03:12.056] [info] [graph][runtime.lifecycle] codec_owner_release node=64 kind=VideoDecode owner_references=2
[2026-11-02 11:03:12.062] [info] [graph][runtime.lifecycle] node_abort_payload node=64 before_objects=129 after_objects=129 before_bytes=249115 after_bytes=249115 before_reservations=23066 after_reservations=23066 before_releases=22937 after_releases=22937
[2026-11-02 11:03:12.062] [info] [graph][runtime.node] source_video_trace_summary node_id=41 node=VideoDecodeNode generation=1 reason=abort steady_ns=8067778821600 input_count=752 accepted_count=752 would_block_count=0 resolved_count=752 output_attempt_count=0 published_count=752 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:03:12.062] [info] [graph][runtime.lifecycle] codec_owner_release node=41 kind=VideoDecode owner_references=2
[2026-11-02 11:03:12.068] [info] [graph][runtime.lifecycle] node_abort_payload node=41 before_objects=129 after_objects=129 before_bytes=249115 after_bytes=249115 before_reservations=23066 after_reservations=23066 before_releases=22937 after_releases=22937
[2026-11-02 11:03:12.068] [info] [graph][runtime.node] source_video_trace_summary node_id=22 node=VideoDecodeNode generation=1 reason=abort steady_ns=8067785236400 input_count=752 accepted_count=752 would_block_count=0 resolved_count=752 output_attempt_count=0 published_count=752 release_count=0 release_committed_count=0 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:03:12.068] [info] [graph][runtime.lifecycle] codec_owner_release node=22 kind=VideoDecode owner_references=2
[2026-11-02 11:03:12.074] [info] [graph][runtime.lifecycle] node_abort_payload node=22 before_objects=129 after_objects=129 before_bytes=249115 after_bytes=249115 before_reservations=23066 after_reservations=23066 before_releases=22937 after_releases=22937
[2026-11-02 11:03:12.074] [info] [graph][runtime.node] source_video_trace_summary node_id=62 node=MediaAvBoundReleaseExtractorNode generation=1 reason=abort steady_ns=8067790980900 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=23 release_count=754 release_committed_count=730 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:03:12.074] [info] [graph][runtime.lifecycle] node_abort_payload node=62 before_objects=129 after_objects=129 before_bytes=249115 after_bytes=249115 before_reservations=23066 after_reservations=23066 before_releases=22937 after_releases=22937
[2026-11-02 11:03:12.074] [info] [graph][runtime.node] source_video_trace_summary node_id=39 node=MediaAvBoundReleaseExtractorNode generation=1 reason=abort steady_ns=8067791123200 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=16 release_count=754 release_committed_count=737 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:03:12.074] [info] [graph][runtime.lifecycle] node_abort_payload node=39 before_objects=129 after_objects=129 before_bytes=249115 after_bytes=249115 before_reservations=23066 after_reservations=23066 before_releases=22937 after_releases=22937
[2026-11-02 11:03:12.074] [info] [graph][runtime.node] source_video_trace_summary node_id=20 node=MediaAvBoundReleaseExtractorNode generation=1 reason=abort steady_ns=8067791248900 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=16 release_count=754 release_committed_count=737 activation_count=0 release_published_count=0 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:03:12.074] [info] [graph][runtime.lifecycle] node_abort_payload node=20 before_objects=129 after_objects=129 before_bytes=249115 after_bytes=249115 before_reservations=23066 after_reservations=23066 before_releases=22937 after_releases=22937
[2026-11-02 11:03:12.074] [info] [graph][runtime.node] source_video_trace_summary node_id=61 node=ActivationReleaseSequencer generation=1 reason=abort steady_ns=8067791368100 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=730 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:03:12.074] [info] [graph][runtime.lifecycle] node_abort_payload node=61 before_objects=129 after_objects=129 before_bytes=249115 after_bytes=249115 before_reservations=23066 after_reservations=23066 before_releases=22937 after_releases=22937
[2026-11-02 11:03:12.074] [info] [graph][runtime.node] source_video_trace_summary node_id=38 node=ActivationReleaseSequencer generation=1 reason=abort steady_ns=8067791505300 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=737 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:03:12.074] [info] [graph][runtime.lifecycle] node_abort_payload node=38 before_objects=129 after_objects=129 before_bytes=249115 after_bytes=249115 before_reservations=23066 after_reservations=23066 before_releases=22937 after_releases=22937
[2026-11-02 11:03:12.075] [info] [graph][runtime.node] source_video_trace_summary node_id=19 node=ActivationReleaseSequencer generation=1 reason=abort steady_ns=8067791628000 input_count=0 accepted_count=0 would_block_count=0 resolved_count=0 output_attempt_count=0 published_count=0 release_count=0 release_committed_count=0 activation_count=1 release_published_count=737 unknown_lineage_node_total=0 stale_node_total=0 incomplete=0
[2026-11-02 11:03:12.075] [info] [graph][runtime.lifecycle] node_abort_payload node=19 before_objects=129 after_objects=129 before_bytes=249115 after_bytes=249115 before_reservations=23066 after_reservations=23066 before_releases=22937 after_releases=22937
[2026-11-02 11:03:12.075] [info] [graph][runtime.lifecycle] node_abort_payload node=60 before_objects=129 after_objects=129 before_bytes=249115 after_bytes=249115 before_reservations=23066 after_reservations=23066 before_releases=22937 after_releases=22937
[2026-11-02 11:03:12.075] [info] [graph][runtime.lifecycle] node_abort_payload node=37 before_objects=129 after_objects=129 before_bytes=249115 after_bytes=249115 before_reservations=23066 after_reservations=23066 before_releases=22937 after_releases=22937
[2026-11-02 11:03:12.075] [info] [graph][runtime.lifecycle] node_abort_payload node=18 before_objects=129 after_objects=129 before_bytes=249115 after_bytes=249115 before_reservations=23066 after_reservations=23066 before_releases=22937 after_releases=22937
[2026-11-02 11:03:12.075] [info] [graph][runtime.lifecycle] node_abort_payload node=58 before_objects=129 after_objects=129 before_bytes=249115 after_bytes=249115 before_reservations=23066 after_reservations=23066 before_releases=22937 after_releases=22937
[2026-11-02 11:03:12.075] [info] [graph][runtime.lifecycle] node_abort_payload node=35 before_objects=129 after_objects=129 before_bytes=249115 after_bytes=249115 before_reservations=23066 after_reservations=23066 before_releases=22937 after_releases=22937
[2026-11-02 11:03:12.075] [info] [graph][runtime.lifecycle] node_abort_payload node=16 before_objects=129 after_objects=129 before_bytes=249115 after_bytes=249115 before_reservations=23066 after_reservations=23066 before_releases=22937 after_releases=22937
[2026-11-02 11:03:12.075] [info] [graph][runtime.lifecycle] node_abort_payload node=57 before_objects=129 after_objects=129 before_bytes=249115 after_bytes=249115 before_reservations=23066 after_reservations=23066 before_releases=22937 after_releases=22937
[2026-11-02 11:03:12.075] [info] [graph][runtime.lifecycle] node_abort_payload node=56 before_objects=129 after_objects=129 before_bytes=249115 after_bytes=249115 before_reservations=23066 after_reservations=23066 before_releases=22937 after_releases=22937
[2026-11-02 11:03:12.075] [info] [graph][runtime.lifecycle] node_abort_payload node=34 before_objects=129 after_objects=129 before_bytes=249115 after_bytes=249115 before_reservations=23066 after_reservations=23066 before_releases=22937 after_releases=22937
[2026-11-02 11:03:12.075] [info] [graph][runtime.lifecycle] node_abort_payload node=15 before_objects=129 after_objects=129 before_bytes=249115 after_bytes=249115 before_reservations=23066 after_reservations=23066 before_releases=22937 after_releases=22937
[2026-11-02 11:03:12.075] [info] [graph][runtime.lifecycle] node_abort_payload node=59 before_objects=129 after_objects=129 before_bytes=249115 after_bytes=249115 before_reservations=23066 after_reservations=23066 before_releases=22937 after_releases=22937
[2026-11-02 11:03:12.075] [info] [graph][runtime.lifecycle] node_abort_payload node=55 before_objects=129 after_objects=129 before_bytes=249115 after_bytes=249115 before_reservations=23066 after_reservations=23066 before_releases=22937 after_releases=22937
[2026-11-02 11:03:12.076] [info] [graph][runtime.lifecycle] node_abort_payload node=54 before_objects=129 after_objects=129 before_bytes=249115 after_bytes=249115 before_reservations=23066 after_reservations=23066 before_releases=22937 after_releases=22937
[2026-11-02 11:03:12.076] [info] [graph][runtime.lifecycle] node_abort_payload node=36 before_objects=129 after_objects=129 before_bytes=249115 after_bytes=249115 before_reservations=23066 after_reservations=23066 before_releases=22937 after_releases=22937
[2026-11-02 11:03:12.076] [info] [graph][runtime.lifecycle] node_abort_payload node=33 before_objects=129 after_objects=129 before_bytes=249115 after_bytes=249115 before_reservations=23066 after_reservations=23066 before_releases=22937 after_releases=22937
[2026-11-02 11:03:12.076] [info] [graph][runtime.lifecycle] node_abort_payload node=17 before_objects=129 after_objects=129 before_bytes=249115 after_bytes=249115 before_reservations=23066 after_reservations=23066 before_releases=22937 after_releases=22937
[2026-11-02 11:03:12.076] [info] [graph][runtime.lifecycle] node_abort_payload node=14 before_objects=129 after_objects=129 before_bytes=249115 after_bytes=249115 before_reservations=23066 after_reservations=23066 before_releases=22937 after_releases=22937
[2026-11-02 11:03:12.076] [info] [graph][runtime.lifecycle] node_abort_payload node=53 before_objects=129 after_objects=129 before_bytes=249115 after_bytes=249115 before_reservations=23066 after_reservations=23066 before_releases=22937 after_releases=22937
[2026-11-02 11:03:12.076] [info] [graph][runtime.lifecycle] node_abort_payload node=32 before_objects=129 after_objects=129 before_bytes=249115 after_bytes=249115 before_reservations=23066 after_reservations=23066 before_releases=22937 after_releases=22937
[2026-11-02 11:03:12.076] [info] [graph][runtime.lifecycle] node_abort_payload node=13 before_objects=129 after_objects=129 before_bytes=249115 after_bytes=249115 before_reservations=23066 after_reservations=23066 before_releases=22937 after_releases=22937
[2026-11-02 11:03:12.076] [info] [graph][runtime.lifecycle] node_abort_payload node=52 before_objects=129 after_objects=129 before_bytes=249115 after_bytes=249115 before_reservations=23066 after_reservations=23066 before_releases=22937 after_releases=22937
[2026-11-02 11:03:12.076] [info] [graph][runtime.lifecycle] node_abort_payload node=51 before_objects=129 after_objects=129 before_bytes=249115 after_bytes=249115 before_reservations=23066 after_reservations=23066 before_releases=22937 after_releases=22937
[2026-11-02 11:03:12.076] [info] [graph][runtime.lifecycle] node_abort_payload node=50 before_objects=129 after_objects=128 before_bytes=249115 after_bytes=245241 before_reservations=23066 after_reservations=23066 before_releases=22937 after_releases=22938
[2026-11-02 11:03:12.076] [info] [graph][runtime.lifecycle] node_abort_payload node=31 before_objects=128 after_objects=128 before_bytes=245241 after_bytes=245241 before_reservations=23066 after_reservations=23066 before_releases=22938 after_releases=22938
[2026-11-02 11:03:12.076] [info] [graph][runtime.lifecycle] node_abort_payload node=30 before_objects=128 after_objects=127 before_bytes=245241 after_bytes=241367 before_reservations=23066 after_reservations=23066 before_releases=22938 after_releases=22939
[2026-11-02 11:03:12.076] [info] [graph][runtime.lifecycle] node_abort_payload node=12 before_objects=127 after_objects=127 before_bytes=241367 after_bytes=241367 before_reservations=23066 after_reservations=23066 before_releases=22939 after_releases=22939
[2026-11-02 11:03:12.076] [info] [graph][runtime.lifecycle] node_abort_payload node=11 before_objects=127 after_objects=126 before_bytes=241367 after_bytes=237493 before_reservations=23066 after_reservations=23066 before_releases=22939 after_releases=22940
[2026-11-02 11:03:12.076] [info] [graph][runtime.lifecycle] node_abort_payload node=49 before_objects=126 after_objects=126 before_bytes=237493 after_bytes=237493 before_reservations=23066 after_reservations=23066 before_releases=22940 after_releases=22940
[2026-11-02 11:03:12.077] [info] [graph][runtime.lifecycle] node_abort_payload node=29 before_objects=126 after_objects=126 before_bytes=237493 after_bytes=237493 before_reservations=23066 after_reservations=23066 before_releases=22940 after_releases=22940
[2026-11-02 11:03:12.077] [info] [graph][runtime.lifecycle] node_abort_payload node=10 before_objects=126 after_objects=126 before_bytes=237493 after_bytes=237493 before_reservations=23066 after_reservations=23066 before_releases=22940 after_releases=22940
[2026-11-02 11:03:12.077] [info] [graph][runtime.lifecycle] node_abort_payload node=69 before_objects=126 after_objects=126 before_bytes=237493 after_bytes=237493 before_reservations=23066 after_reservations=23066 before_releases=22940 after_releases=22940
[2026-11-02 11:03:12.077] [info] [graph][runtime.lifecycle] node_abort_payload node=48 before_objects=126 after_objects=126 before_bytes=237493 after_bytes=237493 before_reservations=23066 after_reservations=23066 before_releases=22940 after_releases=22940
[2026-11-02 11:03:12.077] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-11-02 11:03:12.077] [info] [graph][runtime.lifecycle] node_abort_payload node=63 before_objects=126 after_objects=126 before_bytes=237493 after_bytes=237493 before_reservations=23066 after_reservations=23066 before_releases=22940 after_releases=22940
[2026-11-02 11:03:12.077] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-11-02 11:03:12.077] [info] [graph][runtime.lifecycle] node_abort_payload node=40 before_objects=126 after_objects=126 before_bytes=237493 after_bytes=237493 before_reservations=23066 after_reservations=23066 before_releases=22940 after_releases=22940
[2026-11-02 11:03:12.077] [info] [graph][runtime.lifecycle] node_abort_payload node=28 before_objects=126 after_objects=126 before_bytes=237493 after_bytes=237493 before_reservations=23066 after_reservations=23066 before_releases=22940 after_releases=22940
[2026-11-02 11:03:12.077] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-11-02 11:03:12.078] [info] [graph][runtime.lifecycle] node_abort_payload node=21 before_objects=126 after_objects=126 before_bytes=237493 after_bytes=237493 before_reservations=23066 after_reservations=23066 before_releases=22940 after_releases=22940
[2026-11-02 11:03:12.078] [info] [graph][runtime.lifecycle] node_abort_payload node=9 before_objects=126 after_objects=126 before_bytes=237493 after_bytes=237493 before_reservations=23066 after_reservations=23066 before_releases=22940 after_releases=22940
[2026-11-02 11:03:12.078] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=1053 completion_waits=1053 completed_datagrams=1073 drained_datagrams=21 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-02 11:03:12.078] [info] [graph][runtime.lifecycle] node_abort_payload node=47 before_objects=126 after_objects=126 before_bytes=237493 after_bytes=237493 before_reservations=23066 after_reservations=23066 before_releases=22940 after_releases=22940
[2026-11-02 11:03:12.078] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=14151 completion_waits=14151 completed_datagrams=22018 drained_datagrams=7868 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-02 11:03:12.079] [info] [graph][runtime.lifecycle] node_abort_payload node=46 before_objects=126 after_objects=126 before_bytes=237493 after_bytes=237493 before_reservations=23066 after_reservations=23066 before_releases=22940 after_releases=22940
[2026-11-02 11:03:12.079] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=15271 completion_waits=15271 completed_datagrams=22016 drained_datagrams=6746 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-02 11:03:12.079] [info] [graph][runtime.lifecycle] node_abort_payload node=27 before_objects=126 after_objects=126 before_bytes=237493 after_bytes=237493 before_reservations=23066 after_reservations=23066 before_releases=22940 after_releases=22940
[2026-11-02 11:03:12.080] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=13601 completion_waits=13601 completed_datagrams=22022 drained_datagrams=8421 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-02 11:03:12.080] [info] [graph][runtime.lifecycle] node_abort_payload node=8 before_objects=126 after_objects=126 before_bytes=237493 after_bytes=237493 before_reservations=23066 after_reservations=23066 before_releases=22940 after_releases=22940
[2026-11-02 11:03:12.081] [info] [graph][runtime.lifecycle] node_abort_payload node=3 before_objects=126 after_objects=126 before_bytes=237493 after_bytes=237493 before_reservations=23066 after_reservations=23066 before_releases=22940 after_releases=22940
[2026-11-02 11:03:12.081] [info] [graph][runtime.node] codec_resolver.release slot=encoder_parameters bufferReferences=1 hasCredit=0 creditReferences=0
[2026-11-02 11:03:12.081] [info] [graph][runtime.lifecycle] node_abort_payload node=1 before_objects=126 after_objects=126 before_bytes=237493 after_bytes=237493 before_reservations=23066 after_reservations=23066 before_releases=22940 after_releases=22940
[2026-11-02 11:03:12.148] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=failed detail="Datagram GBRA reservation misses its immutable completion deadline sequence=17207 now_ns=24593396100 release_ns=24527008668 deadline_ns=24593641256 not_before_ns=24593577240 not_after_ns=24593426216 duration_ns=215040 rate=6250000 queue_wire_bytes=4812 average_queue_residence_ns=72912200" description="" error=6 native=0 stage=3 reason="Datagram GBRA reservation misses its immutable completion deadline sequence=17207 now_ns=24593396100 release_ns=24527008668 deadline_ns=24593641256 not_before_ns=24593577240 not_after_ns=24593426216 duration_ns=215040 rate=6250000 queue_wire_bytes=4812 average_queue_residence_ns=72912200"
[2026-11-02 11:03:12.148] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=retired detail="Datagram GBRA reservation misses its immutable completion deadline sequence=17207 now_ns=24593396100 release_ns=24527008668 deadline_ns=24593641256 not_before_ns=24593577240 not_after_ns=24593426216 duration_ns=215040 rate=6250000 queue_wire_bytes=4812 average_queue_residence_ns=72912200" description="" error=6 native=0 stage=3 reason="Datagram GBRA reservation misses its immutable completion deadline sequence=17207 now_ns=24593396100 release_ns=24527008668 deadline_ns=24593641256 not_before_ns=24593577240 not_after_ns=24593426216 duration_ns=215040 rate=6250000 queue_wire_bytes=4812 average_queue_residence_ns=72912200"
[CLI] final runtime report: queued=0, peakQueued=360, threads=81, processThreads=131, workers=0, workerProgress=63684, workerProcessCalls=258163, workerWaits=194472, workerWakeups=192561, workerErrors=2, errors=2, stalledIntervals=0, cpuSamples=77, logicalProcessors=22, averageSystemMachineCpuPercent=9.701464, averageProcessMachineCpuPercent=2.186934, peakProcessMachineCpuPercent=4.175824, averageProcessSingleCoreCpuPercent=48.112548, peakProcessSingleCoreCpuPercent=91.868132, initialWorkingSetBytes=190074880, workingSetBytes=245526528, peakWorkingSetBytes=245526528, totalPushed=57115, totalPopped=57025, droppedBuffers=0, encodedPacketsPushed=8083, encodedPacketsPopped=8077, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=4182753, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=200, graphPayloadReservations=23066, graphPayloadReleases=23066, graphPayloadPressureFailures=0, lastInputReceivedAtNs=8067490100000
~~~

## 完整stderr

~~~text
[in @ 00000263fa0fde40] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000263fa0fde40] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 13022.261256
[in @ 00000263e6d4ac80] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000263e6d4ac80] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 44002.939711
[in @ 00000263e6d49ec0] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000263e6d49ec0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 37255.525733
[in @ 00000263e520b300] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000026392b481c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000263e520b300] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 44002.939711
[in @ 0000026393200680] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000026393200680] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 13022.261256
[in @ 0000026392b481c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 37255.525733
[aac @ 0000026392d0ef00] Qavg: 1031.507
[aac @ 0000026392d0ef00] 2 frames left in the queue on closing
[CLI] realtime video runtime failed: IoFailure: Datagram GBRA reservation misses its immutable completion deadline sequence=17207 now_ns=24593396100 release_ns=24527008668 deadline_ns=24593641256 not_before_ns=24593577240 not_after_ns=24593426216 duration_ns=215040 rate=6250000 queue_wire_bytes=4812 average_queue_residence_ns=72912200

~~~

## source0 完成日志

~~~text
frame= 3578 fps= 30 q=-1.0 size=  118214KiB time=00:01:59.28 bitrate=8118.7kbits/s speed=   1x elapsed=0:01:58.78    
frame= 3594 fps= 30 q=-1.0 size=  118662KiB time=00:01:59.80 bitrate=8114.2kbits/s speed=   1x elapsed=0:01:59.29    
[out#0/rtp @ 000001c072b97f40] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
[out#1/rtp @ 000001c074a34280] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
~~~

## source1 完成日志

~~~text
frame= 3563 fps= 30 q=-1.0 size=  117662KiB time=00:01:58.76 bitrate=8115.8kbits/s speed=   1x elapsed=0:01:58.25    
frame= 3579 fps= 30 q=-1.0 size=  118258KiB time=00:01:59.30 bitrate=8120.4kbits/s speed=   1x elapsed=0:01:58.78    
frame= 3594 fps= 30 q=-1.0 size=  118662KiB time=00:01:59.80 bitrate=8114.2kbits/s speed=   1x elapsed=0:01:59.30    
[out#0/rtp @ 0000020d93068400] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.46    
~~~

## source2 完成日志

~~~text
frame= 3563 fps= 30 q=-1.0 size=  117662KiB time=00:01:58.76 bitrate=8115.8kbits/s speed=   1x elapsed=0:01:58.25    
frame= 3579 fps= 30 q=-1.0 size=  118258KiB time=00:01:59.30 bitrate=8120.4kbits/s speed=   1x elapsed=0:01:58.78    
frame= 3594 fps= 30 q=-1.0 size=  118662KiB time=00:01:59.80 bitrate=8114.2kbits/s speed=   1x elapsed=0:01:59.30    
[out#0/rtp @ 00000287f5aa93c0] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.46    
~~~

## CPU与内存逐秒采样

列：elapsed_seconds,cpu_machine_percent,working_set_bytes,private_bytes。

~~~csv
1.0061253,3.24715565203008,239230976,1151410176
2.0260916,1.46228093293599,239628288,1151418368
3.053143,2.00540994434075,239804416,1151418368
4.0841473,2.06660808124837,239927296,1149431808
5.1106884,2.07559328913554,240472064,1149739008
6.142404,1.7898255188648,240472064,1149739008
7.1689534,2.07557650725997,240906240,1149927424
8.1933969,1.10924969152875,241434624,1150320640
9.2198409,2.49094756442454,244449280,1153282048
10.2475667,2.76426756135643,244510720,1153286144
11.2739865,2.07583857811572,244514816,1153286144
12.3050899,1.92864882768921,244510720,1153286144
13.3310403,2.49214599635439,244568064,1153286144
14.3610298,2.06864421256898,244568064,1153286144
15.3799556,2.78814128654814,244584448,1153294336
16.4151249,1.85246378187958,244633600,1153294336
17.4382665,2.56840393264325,245059584,1153662976
18.4710114,2.54452082899748,245055488,1153662976
19.4981365,2.42014867959652,245116928,1153662976
20.5245543,2.62940065572094,245342208,1153662976
21.5560746,2.06557429667823,245403648,1153667072
22.5803015,2.28831131070664,245469184,1153671168
23.6154483,2.60722791816932,245526528,1153671168
24.6618324,1.69686081986355,243179520,1150140416

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
file debug: opening logfile `D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r378-vlc.log'
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
main debug: stream filter added to 0338bc38
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
qt debug: IM: Setting an input
main debug: using timeshift granularity of 50 MiB
main debug: using timeshift path: C:\Users\ROG\AppData\Local\Temp
main debug: looking for meta fetcher module matching "any": 1 candidates
main debug: `udp://@127.0.0.1:62024' gives access `udp' demux `any' path `@127.0.0.1:62024'
main debug: creating demux: access='udp' demux='any' location='@127.0.0.1:62024' file='\\@127.0.0.1:62024'
main debug: looking for access_demux module matching "udp": 15 candidates
main debug: no access_demux modules matched
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\meta\fetcher
main debug: creating access: udp://@127.0.0.1:62024
main debug:  (path: \\@127.0.0.1:62024)
main debug: looking for access module matching "udp": 27 candidates
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
main debug: resized to 73843456x53942728
main debug: resized to 73843456x53942728
main debug: resized to 73843200x53942728
main debug: resized to 73843200x53942728
main debug: resized to 73842688x53942728
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
direct3d11 debug: D3D11 pool succeed with 48 surfaces (1280x736) context 0x0ab500e0
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
main debug: Buffering 39%
main debug: Received first picture
main debug: Buffering 46%
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
main debug: Buffering 53%
main debug: Buffering 59%
main debug: Buffering 66%
main debug: Buffering 73%
main debug: Buffering 79%
main debug: Buffering 86%
main debug: Buffering 93%
main debug: Buffering 99%
main debug: Stream buffering done (1066 ms in 935 ms)
main debug: Decoder wait done in 0 ms
main debug: inserting 9594 zeroes
mmdevice debug: state changed: 1
main warning: playback too late (78368): up-sampling
main debug: auto hiding mouse cursor
main warning: picture is too late to be displayed (missing 35 ms)
main debug: picture might be displayed late (missing 2 ms)
main debug: resampling stopped (drift: -11778 us)
main debug: accepted socket 3348 (from socket 1176)
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
main debug: Filter 'd3d11_filters' (06d9d970) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (06d9d420) appended to chain
main debug: using video converter module "chain"
main debug: Filter 'chain' (06d9d6c8) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: RV24 -> 1280x720 (1280x720) chroma: RV24 with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (06d9cc28) appended to chain
main debug: using video converter module "chain"
main debug: removing module "png"
main debug: removing module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 06d9d970 removed from chain
main debug: removing module "swscale"
main debug: Filter 06d9d420 removed from chain
main debug: Filter 06d9d6c8 removed from chain
main debug: removing module "swscale"
main debug: Filter 06d9cc28 removed from chain
main debug: snapshot taken (D:/Code/MyCode/MediaTranscode/out/acceptance\composition-r378-native-2026-11-02-11h03m25s064.png)
main debug: looking for video converter module matching "any": 24 candidates
chain debug: Trying to use chroma I420 as middle man
main debug: looking for video converter module matching "any": 24 candidates
main debug: using video converter module "d3d11_filters"
main debug: Filter 'd3d11_filters' (06d9ced0) appended to chain
main debug: looking for video converter module matching "any": 24 candidates
swscale debug: 1280x720 (1280x736) chroma: I420 -> 1280x720 (1280x736) chroma: YUVA with scaling using Bicubic (good quality)
main debug: using video converter module "swscale"
main debug: Filter 'Swscale' (06d9d178) appended to chain
main debug: using video converter module "chain"
main debug: removing module "chain"
main debug: removing module "d3d11_filters"
main debug: Filter 06d9ced0 removed from chain
main debug: removing module "swscale"
main debug: Filter 06d9d178 removed from chain
main error: read error: No error
main debug: accepted socket 3972 (from socket 1176)
main debug: exiting
main debug: exiting
main debug: no exit handler
main debug: removing all interfaces
main debug: removing module "qt"
main debug: deactivating the playlist
main debug: incoming request - stopping current input
prefetch debug: end of stream
main debug: incoming request - stopping current input
ts debug: Can't read TS packet at 20665148
main debug: EOF reached
main warning: buffer too late (-155504551 us): dropped
main warning: picture is too late to be displayed (missing 155824 ms)
main error: read error: No error
main warning: picture is too late to be displayed (missing 155790 ms)
main warning: picture is too late to be displayed (missing 155757 ms)
main warning: picture is too late to be displayed (missing 155724 ms)
main warning: picture is too late to be displayed (missing 155690 ms)
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
main debug: saving media library to file C:\Users\ROG\AppData\Roaming\vlc\ml.xspf.tmp19716
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

- composition-r378-cli-error.log (2369 bytes)
- composition-r378-cli.log (263424 bytes)
- composition-r378-monitor.log (1188 bytes)
- composition-r378-native-2026-11-02-11h03m25s064.png (877857 bytes)
- composition-r378-source0-stdout.log (540 bytes)
- composition-r378-source0.log (29589 bytes)
- composition-r378-source1-stdout.log (335 bytes)
- composition-r378-source1.log (28926 bytes)
- composition-r378-source2-stdout.log (335 bytes)
- composition-r378-source2.log (28926 bytes)
- composition-r378-vlc.log (31620 bytes)

上述11项已逐项清理，本轮临时产物残留0；指定120秒源保留，未运行远程本轮测试。
