# r256：CUDA 消费输入所有权修复后 Release realtime 复验

状态：待执行。命令已沿 r251 原规格核对，尚无本轮 PID、画面或运行结果；不得以本文存在判定已运行。

三路 RTP H.264 720p30，所选 AAC 44.1kHz 双声道；输出 HEVC 1280×720/30 CBR8000kbps、AAC CBR192kbps、MPEG-TS/RTP。每路使用指定连续120秒源，不循环。逐源输入等待100/150/200ms，准备存储2147483648字节，出口50Mbps、wire100ms。先观察三格、全部失源黑屏，再让选定AV源完整120秒重入。VLC原生快照；记录CPU、内存、漂移、退出原因和资源账本。

构建产物：realtime 6118400字节，SHA256 3F0F2CFF6B47379D3BA375BB2848DB7636FBE74581A4F40495D3EF1A86F48CB2。完整目标还包括四源、HEVC输入重排、非固定节奏、C API/ABI及Windows后RKMPP，本轮不替代这些门禁。

## 启动命令（待执行）
```powershell
$ErrorActionPreference='Stop'
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r256-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r256-native- --snapshot-format=png rtp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r256-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r256-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r256-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r256-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r256-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r256-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r256 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r256-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r256-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r256-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r256-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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
```

## VLC原生快照命令（待执行）
```powershell
$client=[Net.Sockets.TcpClient]::new('127.0.0.1',63020);try{$stream=$client.GetStream();$stream.ReadTimeout=2000;$bytes=[Text.Encoding]::ASCII.GetBytes("status`r`nsnapshot`r`n");$stream.Write($bytes,0,$bytes.Length);$buffer=[byte[]]::new(4096);try{while(($n=$stream.Read($buffer,0,4096))-gt 0){[Text.Encoding]::UTF8.GetString($buffer,0,$n)}}catch [IO.IOException]{}}finally{$client.Dispose()}
Get-ChildItem -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance' -Filter 'composition-r256-native-*.png' | Select-Object -ExpandProperty FullName
```

## 重入命令（待执行）
```powershell
$source=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r256-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r256-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($source.Id)"
$source.WaitForExit()
Write-Output "REENTRY_EXIT=$($source.ExitCode)"
exit $source.ExitCode
```

## 结果与清理

待运行后填写实际PID、命令结果、快照观察和关键指标；先记录，再按本轮精确文件/PID清单清理。保留指定源。


本轮归档要求：保留全部graph运行节点/生命周期诊断及stderr；清理前核验protocol_batch_trace、au_processing_trace、scheduler等待是否存在，缺失须明确记录。

## 执行结果：FAIL

会话87828，VLC29964，源29532/21840/26656，CLI32600自然退出1。无快照、未执行重入、未完成120秒；按已判定失败清理源流。物化attempts4、blocked3，阻塞为serial=1而count/bytes均0（容量未满），前批head_committed=0。失败物化1457017000ns，对应deadline1437179952ns，超期19837048ns。完整stdout（仅去周期CLI runtime report，保留final）及stderr如下：
```text
[CLI] composition_inputs=3 grid=fixed audio_source=2 audio=on max_duration=source_driven hw=planner-highest-score
[2026-10-19 09:15:47.433] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-10-19 09:15:47.443] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-10-19 09:15:47.450] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-10-19 09:15:47.565] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-19 09:15:47.567] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-19 09:15:47.612] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-19 09:15:47.613] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-19 09:15:47.654] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-19 09:15:47.654] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-19 09:15:47.724] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-10-19 09:15:47.766] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-10-19 09:15:47.767] [info] [graph][planner.capability] canvas_cuda_stream stage=prepared device_stream=0000000000000000 private_nonblocking=1
[2026-10-19 09:15:47.775] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-19 09:15:47.775] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-19 09:15:47.775] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-10-19 09:15:47.781] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-19 09:15:47.781] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-19 09:15:47.782] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-10-19 09:15:47.787] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-19 09:15:47.788] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-19 09:15:47.788] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-10-19 09:15:47.805] [info] [graph][planner.capability] composition_preparation_storage stage=graph_bound scope=shared_reservations capacity=2147483648 retained=832853516 high_water=841589375
[CLI] audio_plan branch=transcode_frame reason=transcode_required_by_output_frame_contract codec=aac sample_rate=44100 channels=2 access_unit_samples=1024 encoder=aac bitrate_kbps=192
[CLI] prepared_output_id=1
[CLI] selected_chain=fixed-grid-composition score=0 decoder=h264,h264,h264 filter=scale_cuda=640:360,scale_cuda=640:360,scale_cuda=640:360 encoder=hevc_nvenc
[2026-10-19 09:15:47.808] [info] [graph][runtime.lifecycle] compile.begin
[2026-10-19 09:15:47.808] [info] [graph][runtime.channel] create channel=1 edge=1 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 09:15:47.808] [info] [graph][runtime.channel] create channel=2 edge=2 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 09:15:47.809] [info] [graph][runtime.channel] create channel=3 edge=3 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-19 09:15:47.809] [info] [graph][runtime.channel] create channel=4 edge=4 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.809] [info] [graph][runtime.channel] create channel=5 edge=5 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.809] [info] [graph][runtime.channel] create channel=6 edge=6 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.809] [info] [graph][runtime.channel] create channel=7 edge=7 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.809] [info] [graph][runtime.channel] create channel=8 edge=8 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.809] [info] [graph][runtime.channel] create channel=9 edge=9 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 09:15:47.809] [info] [graph][runtime.channel] create channel=10 edge=10 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.809] [info] [graph][runtime.channel] create channel=11 edge=11 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.809] [info] [graph][runtime.channel] create channel=12 edge=12 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 09:15:47.809] [info] [graph][runtime.channel] create channel=13 edge=13 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 09:15:47.810] [info] [graph][runtime.channel] create channel=14 edge=14 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.810] [info] [graph][runtime.channel] create channel=15 edge=15 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.810] [info] [graph][runtime.channel] create channel=16 edge=16 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.810] [info] [graph][runtime.channel] create channel=17 edge=17 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.810] [info] [graph][runtime.channel] create channel=18 edge=18 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.810] [info] [graph][runtime.channel] create channel=19 edge=19 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.810] [info] [graph][runtime.channel] create channel=20 edge=20 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.810] [info] [graph][runtime.channel] create channel=21 edge=21 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-10-19 09:15:47.810] [info] [graph][runtime.channel] create channel=22 edge=22 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 09:15:47.810] [info] [graph][runtime.channel] create channel=23 edge=23 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 09:15:47.810] [info] [graph][runtime.channel] create channel=24 edge=24 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 09:15:47.810] [info] [graph][runtime.channel] create channel=25 edge=25 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 09:15:47.810] [info] [graph][runtime.channel] create channel=26 edge=26 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 09:15:47.811] [info] [graph][runtime.channel] create channel=27 edge=27 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 09:15:47.811] [info] [graph][runtime.channel] create channel=28 edge=28 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 09:15:47.811] [info] [graph][runtime.channel] create channel=29 edge=29 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 09:15:47.811] [info] [graph][runtime.channel] create channel=30 edge=30 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.811] [info] [graph][runtime.channel] create channel=31 edge=31 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.811] [info] [graph][runtime.channel] create channel=32 edge=32 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.812] [info] [graph][runtime.channel] create channel=33 edge=33 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.812] [info] [graph][runtime.channel] create channel=34 edge=34 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.812] [info] [graph][runtime.channel] create channel=35 edge=35 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.812] [info] [graph][runtime.channel] create channel=36 edge=36 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 09:15:47.812] [info] [graph][runtime.channel] create channel=37 edge=37 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.812] [info] [graph][runtime.channel] create channel=38 edge=38 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.812] [info] [graph][runtime.channel] create channel=39 edge=39 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 09:15:47.812] [info] [graph][runtime.channel] create channel=40 edge=40 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 09:15:47.812] [info] [graph][runtime.channel] create channel=41 edge=41 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.812] [info] [graph][runtime.channel] create channel=42 edge=42 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.812] [info] [graph][runtime.channel] create channel=43 edge=43 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.812] [info] [graph][runtime.channel] create channel=44 edge=44 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.813] [info] [graph][runtime.channel] create channel=45 edge=45 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.813] [info] [graph][runtime.channel] create channel=46 edge=46 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.813] [info] [graph][runtime.channel] create channel=47 edge=47 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.813] [info] [graph][runtime.channel] create channel=48 edge=48 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-10-19 09:15:47.813] [info] [graph][runtime.channel] create channel=49 edge=49 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 09:15:47.813] [info] [graph][runtime.channel] create channel=50 edge=50 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 09:15:47.813] [info] [graph][runtime.channel] create channel=51 edge=51 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 09:15:47.813] [info] [graph][runtime.channel] create channel=52 edge=52 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 09:15:47.813] [info] [graph][runtime.channel] create channel=53 edge=53 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 09:15:47.813] [info] [graph][runtime.channel] create channel=54 edge=54 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 09:15:47.813] [info] [graph][runtime.channel] create channel=55 edge=55 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 09:15:47.813] [info] [graph][runtime.channel] create channel=56 edge=56 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 09:15:47.814] [info] [graph][runtime.channel] create channel=57 edge=57 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.814] [info] [graph][runtime.channel] create channel=58 edge=58 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.814] [info] [graph][runtime.channel] create channel=59 edge=59 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.814] [info] [graph][runtime.channel] create channel=60 edge=60 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.814] [info] [graph][runtime.channel] create channel=61 edge=61 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.814] [info] [graph][runtime.channel] create channel=62 edge=62 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.814] [info] [graph][runtime.channel] create channel=63 edge=63 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.814] [info] [graph][runtime.channel] create channel=64 edge=64 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.814] [info] [graph][runtime.channel] create channel=65 edge=65 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.814] [info] [graph][runtime.channel] create channel=66 edge=66 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 09:15:47.814] [info] [graph][runtime.channel] create channel=67 edge=67 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 09:15:47.814] [info] [graph][runtime.channel] create channel=68 edge=68 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.815] [info] [graph][runtime.channel] create channel=69 edge=69 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.815] [info] [graph][runtime.channel] create channel=70 edge=70 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.815] [info] [graph][runtime.channel] create channel=71 edge=71 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 09:15:47.815] [info] [graph][runtime.channel] create channel=72 edge=72 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 09:15:47.815] [info] [graph][runtime.channel] create channel=73 edge=73 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 09:15:47.815] [info] [graph][runtime.channel] create channel=74 edge=74 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 09:15:47.815] [info] [graph][runtime.channel] create channel=75 edge=75 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.815] [info] [graph][runtime.channel] create channel=76 edge=76 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.815] [info] [graph][runtime.channel] create channel=77 edge=77 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.815] [info] [graph][runtime.channel] create channel=78 edge=78 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.815] [info] [graph][runtime.channel] create channel=79 edge=79 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.815] [info] [graph][runtime.channel] create channel=80 edge=80 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.815] [info] [graph][runtime.channel] create channel=81 edge=81 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.815] [info] [graph][runtime.channel] create channel=82 edge=82 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.816] [info] [graph][runtime.channel] create channel=83 edge=83 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-10-19 09:15:47.816] [info] [graph][runtime.channel] create channel=84 edge=84 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 09:15:47.816] [info] [graph][runtime.channel] create channel=85 edge=85 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 09:15:47.816] [info] [graph][runtime.channel] create channel=86 edge=86 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 09:15:47.816] [info] [graph][runtime.channel] create channel=87 edge=87 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 09:15:47.816] [info] [graph][runtime.channel] create channel=88 edge=88 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 09:15:47.816] [info] [graph][runtime.channel] create channel=89 edge=89 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 09:15:47.816] [info] [graph][runtime.channel] create channel=90 edge=90 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 09:15:47.816] [info] [graph][runtime.channel] create channel=91 edge=91 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-10-19 09:15:47.816] [info] [graph][runtime.channel] create channel=92 edge=92 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 09:15:47.816] [info] [graph][runtime.channel] create channel=93 edge=93 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 09:15:47.817] [info] [graph][runtime.channel] create channel=94 edge=94 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 09:15:47.817] [info] [graph][runtime.channel] create channel=95 edge=95 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 09:15:47.817] [info] [graph][runtime.channel] create channel=96 edge=96 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 09:15:47.817] [info] [graph][runtime.channel] create channel=97 edge=97 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 09:15:47.817] [info] [graph][runtime.channel] create channel=98 edge=98 stream=Audio edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.817] [info] [graph][runtime.channel] create channel=99 edge=99 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 09:15:47.817] [info] [graph][runtime.channel] create channel=100 edge=100 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.817] [info] [graph][runtime.channel] create channel=101 edge=101 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0
[2026-10-19 09:15:47.817] [info] [graph][runtime.channel] create channel=102 edge=102 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 09:15:47.817] [info] [graph][runtime.channel] create channel=103 edge=103 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 09:15:47.817] [info] [graph][runtime.channel] create channel=104 edge=104 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 09:15:47.817] [info] [graph][runtime.channel] create channel=105 edge=105 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0
[2026-10-19 09:15:47.817] [info] [graph][runtime.channel] create channel=106 edge=106 stream=Video edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-19 09:15:47.817] [info] [graph][runtime.channel] create channel=107 edge=107 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-19 09:15:47.817] [info] [graph][runtime.channel] create channel=108 edge=108 stream=Any edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-19 09:15:47.817] [info] [graph][runtime.channel] create channel=109 edge=109 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.817] [info] [graph][runtime.channel] create channel=110 edge=110 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.818] [info] [graph][runtime.channel] create channel=111 edge=111 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-19 09:15:47.818] [info] [graph][runtime.channel] create channel=112 edge=112 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 09:15:47.818] [info] [graph][runtime.channel] create channel=113 edge=113 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 09:15:47.818] [info] [graph][runtime.channel] create channel=114 edge=114 stream=Any edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-19 09:15:47.818] [info] [graph][runtime.channel] create channel=115 edge=115 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 09:15:47.818] [info] [graph][runtime.channel] create channel=116 edge=116 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-19 09:15:47.818] [info] [graph][runtime.channel] create channel=117 edge=117 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-19 09:15:47.818] [info] [graph][runtime.channel] create channel=118 edge=118 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-19 09:15:47.818] [info] [graph][runtime.channel] create channel=119 edge=119 stream=Any edge_kind=EncodedPacket payload=Unknown queued=0
[2026-10-19 09:15:47.818] [info] [graph][runtime.channel] create channel=120 edge=120 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-19 09:15:47.818] [info] [graph][runtime.channel] create channel=121 edge=121 stream=Metadata edge_kind=Unknown payload=Unknown queued=0
[2026-10-19 09:15:47.818] [info] [graph][runtime.channel] create channel=122 edge=122 stream=Metadata edge_kind=Unknown payload=Unknown queued=0
[2026-10-19 09:15:47.818] [info] [graph][runtime.channel] create channel=123 edge=123 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-19 09:15:47.818] [info] [graph][runtime.lifecycle] compiled nodes=82 edges=123 channels=123 execution_order=1->3->8->27->46->47->9->21->28->40->63->48->69->10->29->49->11->12->30->31->50->51->52->13->32->53->14->17->33->36->54->55->59->15->34->56->57->16->35->58->18->37->60->19->38->61->20->39->62->22->41->64->70->23->42->65->71->24->43->66->72->26->45->68->73->25->44->67->6->2->7->81->78->4->77->5->74->75->79->76->80->82
[2026-10-19 09:15:47.819] [info] [graph][runtime.lifecycle] compile.done state=DefaultRegistrationPending
[2026-10-19 09:15:47.819] [info] [graph][runtime.node] register node=1 name=composition-r256.output.video.codec_resolver kind=CodecResolver
[2026-10-19 09:15:47.819] [info] [graph][runtime.node] register node=2 name=composition-r256.output.video.encode kind=VideoEncode
[2026-10-19 09:15:47.819] [info] [graph][runtime.node] register node=3 name=composition-r256.output.audio.codec_resolver kind=AudioCodecResolver
[2026-10-19 09:15:47.819] [info] [graph][runtime.node] register node=4 name=composition-r256.output.audio.encode kind=AudioEncode
[2026-10-19 09:15:47.819] [info] [graph][runtime.node] register node=5 name=composition-r256.output.audio.canonicalizer kind=EncodedAudioCanonicalizer
[2026-10-19 09:15:47.819] [info] [graph][runtime.node] register node=8 name=composition-r256.source_0.input kind=RawRtpInput
[2026-10-19 09:15:47.819] [info] [graph][runtime.node] register node=9 name=composition-r256.source_0.rtp.clock_group kind=RtpClockGroup
[2026-10-19 09:15:47.819] [info] [graph][runtime.node] register node=10 name=composition-r256.source_0.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-10-19 09:15:47.820] [info] [graph][runtime.node] register node=11 name=composition-r256.source_0.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-10-19 09:15:47.820] [info] [graph][runtime.node] register node=12 name=composition-r256.source_0.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-10-19 09:15:47.820] [info] [graph][runtime.node] register node=13 name=composition-r256.source_0.av_sync.source_clock kind=SourceClockStateFanout
[2026-10-19 09:15:47.820] [info] [graph][runtime.node] register node=14 name=composition-r256.source_0.av_sync.video.generation_gate kind=LockedPacketGate
[2026-10-19 09:15:47.820] [info] [graph][runtime.node] register node=15 name=composition-r256.source_0.av_sync.video.canonical_input kind=CanonicalInput
[2026-10-19 09:15:47.820] [info] [graph][runtime.node] register node=16 name=composition-r256.source_0.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-10-19 09:15:47.820] [info] [graph][runtime.node] register node=17 name=composition-r256.source_0.av_sync.startup.clock kind=AvStartupClock
[2026-10-19 09:15:47.820] [info] [graph][runtime.node] register node=18 name=composition-r256.source_0.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-10-19 09:15:47.820] [info] [graph][runtime.node] register node=20 name=composition-r256.source_0.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-10-19 09:15:47.820] [info] [graph][runtime.node] register node=21 name=composition-r256.source_0.video.codec_resolver kind=CodecResolver
[2026-10-19 09:15:47.820] [info] [graph][runtime.node] register node=22 name=composition-r256.source_0.video.decode kind=VideoDecode
[2026-10-19 09:15:47.820] [info] [graph][runtime.node] register node=23 name=composition-r256.source_0.video.hwtransfer kind=HardwareTransfer
[2026-10-19 09:15:47.820] [info] [graph][runtime.node] register node=24 name=composition-r256.source_0.video.framerate kind=VideoFrameRate
[2026-10-19 09:15:47.820] [info] [graph][runtime.node] register node=25 name=composition-r256.source_0.video.preparation kind=VideoPreparation
[2026-10-19 09:15:47.821] [info] [graph][runtime.node] register node=26 name=composition-r256.source_0.video.filter kind=VideoFilter
[2026-10-19 09:15:47.821] [info] [graph][runtime.node] register node=27 name=composition-r256.source_1.input kind=RawRtpInput
[2026-10-19 09:15:47.821] [info] [graph][runtime.node] register node=28 name=composition-r256.source_1.rtp.clock_group kind=RtpClockGroup
[2026-10-19 09:15:47.821] [info] [graph][runtime.node] register node=29 name=composition-r256.source_1.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-10-19 09:15:47.821] [info] [graph][runtime.node] register node=30 name=composition-r256.source_1.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-10-19 09:15:47.821] [info] [graph][runtime.node] register node=31 name=composition-r256.source_1.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-10-19 09:15:47.821] [info] [graph][runtime.node] register node=32 name=composition-r256.source_1.av_sync.source_clock kind=SourceClockStateFanout
[2026-10-19 09:15:47.821] [info] [graph][runtime.node] register node=33 name=composition-r256.source_1.av_sync.video.generation_gate kind=LockedPacketGate
[2026-10-19 09:15:47.821] [info] [graph][runtime.node] register node=34 name=composition-r256.source_1.av_sync.video.canonical_input kind=CanonicalInput
[2026-10-19 09:15:47.821] [info] [graph][runtime.node] register node=35 name=composition-r256.source_1.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-10-19 09:15:47.821] [info] [graph][runtime.node] register node=36 name=composition-r256.source_1.av_sync.startup.clock kind=AvStartupClock
[2026-10-19 09:15:47.821] [info] [graph][runtime.node] register node=37 name=composition-r256.source_1.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-10-19 09:15:47.821] [info] [graph][runtime.node] register node=39 name=composition-r256.source_1.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-10-19 09:15:47.821] [info] [graph][runtime.node] register node=40 name=composition-r256.source_1.video.codec_resolver kind=CodecResolver
[2026-10-19 09:15:47.821] [info] [graph][runtime.node] register node=41 name=composition-r256.source_1.video.decode kind=VideoDecode
[2026-10-19 09:15:47.821] [info] [graph][runtime.node] register node=42 name=composition-r256.source_1.video.hwtransfer kind=HardwareTransfer
[2026-10-19 09:15:47.821] [info] [graph][runtime.node] register node=43 name=composition-r256.source_1.video.framerate kind=VideoFrameRate
[2026-10-19 09:15:47.821] [info] [graph][runtime.node] register node=44 name=composition-r256.source_1.video.preparation kind=VideoPreparation
[2026-10-19 09:15:47.822] [info] [graph][runtime.node] register node=45 name=composition-r256.source_1.video.filter kind=VideoFilter
[2026-10-19 09:15:47.822] [info] [graph][runtime.node] register node=46 name=composition-r256.source_2.video.input kind=RawRtpInput
[2026-10-19 09:15:47.822] [info] [graph][runtime.node] register node=47 name=composition-r256.source_2.audio.input kind=RawRtpInput
[2026-10-19 09:15:47.822] [info] [graph][runtime.node] register node=48 name=composition-r256.source_2.rtp.clock_group kind=RtpClockGroup
[2026-10-19 09:15:47.822] [info] [graph][runtime.node] register node=49 name=composition-r256.source_2.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-10-19 09:15:47.822] [info] [graph][runtime.node] register node=50 name=composition-r256.source_2.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-10-19 09:15:47.822] [info] [graph][runtime.node] register node=51 name=composition-r256.source_2.av_sync.audio.protocol_binder kind=RtpPacketClockBinder
[2026-10-19 09:15:47.822] [info] [graph][runtime.node] register node=52 name=composition-r256.source_2.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-10-19 09:15:47.822] [info] [graph][runtime.node] register node=53 name=composition-r256.source_2.av_sync.source_clock kind=SourceClockStateFanout
[2026-10-19 09:15:47.822] [info] [graph][runtime.node] register node=54 name=composition-r256.source_2.av_sync.video.generation_gate kind=LockedPacketGate
[2026-10-19 09:15:47.822] [info] [graph][runtime.node] register node=55 name=composition-r256.source_2.av_sync.audio.generation_gate kind=LockedPacketGate
[2026-10-19 09:15:47.822] [info] [graph][runtime.node] register node=56 name=composition-r256.source_2.av_sync.video.canonical_input kind=CanonicalInput
[2026-10-19 09:15:47.822] [info] [graph][runtime.node] register node=57 name=composition-r256.source_2.av_sync.audio.canonical_input kind=CanonicalInput
[2026-10-19 09:15:47.822] [info] [graph][runtime.node] register node=58 name=composition-r256.source_2.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-10-19 09:15:47.822] [info] [graph][runtime.node] register node=59 name=composition-r256.source_2.av_sync.startup.clock kind=AvStartupClock
[2026-10-19 09:15:47.822] [info] [graph][runtime.node] register node=60 name=composition-r256.source_2.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-10-19 09:15:47.823] [info] [graph][runtime.node] register node=62 name=composition-r256.source_2.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-10-19 09:15:47.823] [info] [graph][runtime.node] register node=63 name=composition-r256.source_2.video.codec_resolver kind=CodecResolver
[2026-10-19 09:15:47.823] [info] [graph][runtime.node] register node=64 name=composition-r256.source_2.video.decode kind=VideoDecode
[2026-10-19 09:15:47.823] [info] [graph][runtime.node] register node=65 name=composition-r256.source_2.video.hwtransfer kind=HardwareTransfer
[2026-10-19 09:15:47.823] [info] [graph][runtime.node] register node=66 name=composition-r256.source_2.video.framerate kind=VideoFrameRate
[2026-10-19 09:15:47.823] [info] [graph][runtime.node] register node=67 name=composition-r256.source_2.video.preparation kind=VideoPreparation
[2026-10-19 09:15:47.823] [info] [graph][runtime.node] register node=68 name=composition-r256.source_2.video.filter kind=VideoFilter
[2026-10-19 09:15:47.823] [info] [graph][runtime.node] register node=69 name=composition-r256.source_2.audio.encode.codec_resolver kind=AudioCodecResolver
[2026-10-19 09:15:47.823] [info] [graph][runtime.node] register node=70 name=composition-r256.source_2.audio.encode.decode kind=AudioDecode
[2026-10-19 09:15:47.823] [info] [graph][runtime.node] register node=71 name=composition-r256.source_2.audio.encode.startup_trim kind=Unknown
[2026-10-19 09:15:47.823] [info] [graph][runtime.node] register node=72 name=composition-r256.source_2.audio.encode.drift_controller kind=AudioDriftController
[2026-10-19 09:15:47.823] [info] [graph][runtime.node] register node=73 name=composition-r256.source_2.audio.encode.resample kind=AudioResample
[2026-10-19 09:15:47.824] [info] [graph][runtime.node] register node=74 name=composition-r256.output.scheduler.scheduler kind=AvOutputScheduler
[2026-10-19 09:15:47.824] [info] [graph][runtime.node] register node=75 name=composition-r256.output.scheduler.router kind=ScheduledOutputRouter
[2026-10-19 09:15:47.824] [info] [graph][runtime.node] register node=76 name=composition-r256.output.mpegts.mux kind=FileMux
[2026-10-19 09:15:47.824] [info] [graph][runtime.node] register node=78 name=composition-r256.output.mpegts.plan.source kind=Unknown
[2026-10-19 09:15:47.824] [info] [graph][runtime.node] register node=79 name=composition-r256.output.mpegts.scheduled.adapter kind=Unknown
[2026-10-19 09:15:47.824] [info] [graph][runtime.node] register node=80 name=composition-r256.output.mpegts.datagram.materializer kind=MpegTsDatagramMaterializer
[2026-10-19 09:15:47.824] [info] [graph][runtime.node] register node=81 name=composition-r256.output.mpegts.transport.plan kind=DatagramTransportPlanSource
[2026-10-19 09:15:47.824] [info] [graph][runtime.node] register node=82 name=composition-r256.output.mpegts.transport.sender kind=ScheduledDatagramSender
[2026-10-19 09:15:47.824] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=waiting_for_random_access detail="" description="D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r256-output.sdp"
[2026-10-19 09:15:47.825] [info] [graph][runtime.lifecycle] start.begin mode=threaded
[2026-10-19 09:15:47.825] [info] [graph][runtime.lifecycle] worker.finished node=1
[2026-10-19 09:15:47.826] [info] [graph][runtime.node] worker.thread_cpu node=1 available=1 cpu_ns=0 process_calls=1
[2026-10-19 09:15:47.826] [info] [graph][runtime.lifecycle] worker.finished node=3
[2026-10-19 09:15:47.826] [info] [graph][runtime.node] worker.thread_cpu node=3 available=1 cpu_ns=0 process_calls=1
[2026-10-19 09:15:47.826] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 09:15:47.826] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 09:15:47.827] [info] [graph][runtime.lifecycle] start.done state=ThreadedRunning
[2026-10-19 09:15:47.828] [info] [graph][runtime.lifecycle] worker.finished node=63
[2026-10-19 09:15:47.828] [info] [graph][runtime.node] worker.thread_cpu node=63 available=1 cpu_ns=0 process_calls=2
[2026-10-19 09:15:47.829] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-10-19 09:15:47.829] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=1 generation=1
[2026-10-19 09:15:47.829] [info] [graph][runtime.lifecycle] worker.finished node=69
[2026-10-19 09:15:47.829] [info] [graph][runtime.node] worker.thread_cpu node=69 available=1 cpu_ns=0 process_calls=2
[2026-10-19 09:15:47.830] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 09:15:47.830] [info] [graph][runtime.lifecycle] worker.finished node=21
[2026-10-19 09:15:47.830] [info] [graph][runtime.node] worker.thread_cpu node=21 available=1 cpu_ns=0 process_calls=1
[2026-10-19 09:15:47.831] [info] [graph][runtime.lifecycle] worker.finished node=40
[2026-10-19 09:15:47.831] [info] [graph][runtime.node] worker.thread_cpu node=40 available=1 cpu_ns=0 process_calls=1
[2026-10-19 09:15:47.831] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-10-19 09:15:47.832] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-10-19 09:15:47.832] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=1 generation=1
[2026-10-19 09:15:47.832] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=1 generation=1
[2026-10-19 09:15:47.832] [info] [graph][runtime.node] video_encode.bind_encoder codec=hevc_nvenc pix_fmt=cuda rate_control=cbr bitrate=8000000 min_bitrate=8000000 max_bitrate=8000000 buffer_size=267000 fps=30/1 gop=60 frame_kind=hardware hwaccel=cuda hw_device_ctx=set hw_frames_ctx=set
[2026-10-19 09:15:47.832] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=4193259992 dts=4193259992 size=33389 generation=1 sequence=1
[2026-10-19 09:15:47.832] [info] [graph][runtime.node] audio_encode_trace stage=codec_bound
[2026-10-19 09:15:47.832] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=2476383912 dts=2476383912 size=33389 generation=1 sequence=1
[2026-10-19 09:15:47.833] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=1345300032 dts=1345300032 size=33389 generation=1 sequence=1
[2026-10-19 09:15:47.860] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-10-19 09:15:47.860] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-10-19 09:15:47.860] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=4193259992 format=cuda size=1280x720 buffer_identity=0xb08600000 drm_prime=0 software_frame=0
[2026-10-19 09:15:47.860] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=2476383912 format=cuda size=1280x720 buffer_identity=0xb08800000 drm_prime=0 software_frame=0
[2026-10-19 09:15:47.860] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=0000016599AAE2D0 stream=Video payload=Frame pts=4193259992 dts=4193259992 duration=3000 tb=1/90000 flags=68
[2026-10-19 09:15:47.860] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=00000165994AF9A0 stream=Video payload=Frame pts=2476383912 dts=2476383912 duration=3000 tb=1/90000 flags=68
[2026-10-19 09:15:47.860] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=0000016599AAE2D0 stream=Video payload=Frame pts=4193259992 dts=4193259992 duration=3000 tb=1/90000 flags=68
[2026-10-19 09:15:47.861] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=0000016599AAE2D0 stream=Video payload=Frame pts=4193259992 dts=4193259992 duration=3000 tb=1/90000 flags=68
[2026-10-19 09:15:47.861] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=00000165994AF9A0 stream=Video payload=Frame pts=2476383912 dts=2476383912 duration=3000 tb=1/90000 flags=68
[2026-10-19 09:15:47.861] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=00000165994AF9A0 stream=Video payload=Frame pts=2476383912 dts=2476383912 duration=3000 tb=1/90000 flags=68
[2026-10-19 09:15:47.861] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-10-19 09:15:47.861] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-10-19 09:15:47.861] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-10-19 09:15:47.862] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=00000165992EBBB0 stream=Video payload=Frame pts=4193259992 dts=4193259992 duration=3000 tb=1/90000 flags=68
[2026-10-19 09:15:47.862] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=00000165994EA630 stream=Video payload=Frame pts=2476383912 dts=2476383912 duration=3000 tb=1/90000 flags=68
[2026-10-19 09:15:47.862] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=1345300032 format=cuda size=1280x720 buffer_identity=0xb08a00000 drm_prime=0 software_frame=0
[2026-10-19 09:15:47.862] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=00000165994EA630 stream=Video payload=Frame pts=2476383912 dts=2476383912 duration=3000 tb=1/90000 flags=68
[2026-10-19 09:15:47.862] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=00000165992EBBB0 stream=Video payload=Frame pts=4193259992 dts=4193259992 duration=3000 tb=1/90000 flags=68
[2026-10-19 09:15:47.862] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-10-19 09:15:47.862] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=0000016599AAE2D0 stream=Video payload=Frame pts=1345300032 dts=1345300032 duration=3000 tb=1/90000 flags=68
[2026-10-19 09:15:47.862] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-10-19 09:15:47.862] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=0000016599AAE2D0 stream=Video payload=Frame pts=1345300032 dts=1345300032 duration=3000 tb=1/90000 flags=68
[2026-10-19 09:15:47.862] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=0000016599AAE2D0 stream=Video payload=Frame pts=1345300032 dts=1345300032 duration=3000 tb=1/90000 flags=68
[2026-10-19 09:15:47.863] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-10-19 09:15:47.863] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=00000165994AF950 stream=Video payload=Frame pts=1345300032 dts=1345300032 duration=3000 tb=1/90000 flags=68
[2026-10-19 09:15:47.863] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=00000165994AF950 stream=Video payload=Frame pts=1345300032 dts=1345300032 duration=3000 tb=1/90000 flags=68
[2026-10-19 09:15:47.863] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-10-19 09:15:47.913] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=50
[2026-10-19 09:15:47.913] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=50
[2026-10-19 09:15:47.914] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=51
[2026-10-19 09:15:47.915] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb0ae00000 drm_prime=0 software_frame=0
[2026-10-19 09:15:47.915] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=16 audio_count=0 first_video_sequence=1
[2026-10-19 09:15:47.915] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb0ae90000 drm_prime=0 software_frame=0
[2026-10-19 09:15:47.915] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=0
[2026-10-19 09:15:47.915] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=16 audio_count=0 first_video_sequence=1
[2026-10-19 09:15:47.915] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=0
[2026-10-19 09:15:47.916] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb0af20000 drm_prime=0 software_frame=0
[2026-10-19 09:15:47.916] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=26 audio_count=22 first_video_sequence=1
[2026-10-19 09:15:47.916] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=22
[2026-10-19 09:15:47.916] [info] [graph][runtime.node] audio_decode_trace stage=first_packet
[2026-10-19 09:15:47.916] [info] [graph][runtime.node] audio_decode_trace stage=first_frame samples=1024 rate=44100
[2026-10-19 09:15:47.917] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1 source_master_ns=692796621 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-10-19 09:15:47.917] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2 source_master_ns=716016576 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=45346
[2026-10-19 09:15:47.917] [info] [graph][runtime.node] audio_encode_trace stage=first_frame
[2026-10-19 09:15:47.920] [info] [graph][runtime.node] mpegts_emission_plan generation=1 mode=canonical_access_unit_window access_unit_window_ns=100000000 video_initial_service_window_ns=13995843 audio_initial_service_window_ns=13995843 packet_size_bytes=188 maximum_payload_bytes=1316 per_datagram_overhead_bytes=12
[2026-10-19 09:15:47.921] [info] [graph][runtime.node] video_encode.input_contract format=cuda size=1280x720 buffer_identity=0xb0a000000 drm_prime=0 software_frame=0
[2026-10-19 09:15:47.921] [info] [graph][runtime.node] video_encode.generation_start_key_frame pts=0 generation=1
[2026-10-19 09:15:47.921] [info] [graph][runtime.node] video_encode.first_frame pts=0 generation=attached
[2026-10-19 09:15:47.927] [info] [graph][runtime.node] video_encode.first_submit result=accepted
[2026-10-19 09:15:47.927] [info] [graph][runtime.node] video_encode.first_packet pts=0 dts=0 duration=0 size=259 side_data_elems=1 received_footprint=267 side_data[0]=Quality stats:8
[2026-10-19 09:15:47.927] [info] [graph][runtime.node] video_encode.first_packet_footprint bytes=267 wrapped_size=259 wrapped_side_data_elems=1
[2026-10-19 09:15:47.927] [info] [graph][runtime.node] av_scheduler_trace stage=first_video_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=670513268 dispatch_master_ns=670513268 now_ns=514369700 node=74 identity_kind=output identity="composition-r256.output" canonical_sequence=1 canonical_created_steady_ns=1961844785800 head_observed_steady_ns=1961844836700 codec_receive_begin_steady_ns=1961844461600 codec_receive_end_steady_ns=1961844462500
[2026-10-19 09:15:47.928] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=514532800 present_generation=1 present_canonical_target_ns=0 present_canonical_dispatch_ns=0 present_target_master_ns=670513268 present_dispatch_master_ns=670513268
[2026-10-19 09:15:47.964] [info] [graph][runtime.node] audio_encode_trace stage=first_packet
[2026-10-19 09:15:47.964] [info] [graph][runtime.node] av_scheduler_trace stage=first_audio_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=670513268 dispatch_master_ns=670513268 now_ns=551439600 node=74 identity_kind=output identity="composition-r256.output" canonical_sequence=1 canonical_created_steady_ns=1961881868100 head_observed_steady_ns=1961881906600 codec_receive_begin_steady_ns=unavailable codec_receive_end_steady_ns=unavailable
[2026-10-19 09:15:48.149] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=running detail="" description="D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r256-output.sdp"
[2026-10-19 09:15:48.521] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=47 source_master_ns=1760914535 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=90692
[2026-10-19 09:15:48.870] [info] [graph][runtime.node] protocol_batch_trace stage=materialization_failed generation=1 attempts=4 consumed_datagrams=0 remaining_datagrams=1 attempted_master_ns=1457017000 mux_dequeued_master_ns=1364017700 first_dequeued_master_ns=1436948100 last_wire_published_master_ns=1436940500 previous_attempt_started_master_ns=1436986200 previous_attempt_completed_master_ns=1436996600 last_attempt_completed_master_ns=1457023300 last_blocked_started_master_ns=1436986200 last_blocked_completed_master_ns=1436996600 blocked_attempts=3 last_blocked_code=WouldBlock last_blocked_native=0 last_blocked_original_message_bytes=285 last_blocked_message_truncated=0 last_blocked_message="wire global sequence reservation exceeds planner backlog capacity serial=1 count=0 bytes=0 request_count=1 request_bytes=228 used_count=58 used_bytes=78460 limit_count=2155 limit_bytes=2920356 generation=1 projected_sequence=667 head_id=94 head_first=609 head_count=58 head_committed=0" remaining_front_release_master_ns=1370513284 remaining_front_deadline_master_ns=1437179952 original_error="InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=1370513284 materialized_ns=1457017000 protocol_deadline_ns=1437179952 deadline_before_release=0 materialized_after_deadline=1"
[2026-10-19 09:15:48.870] [info] [graph][runtime.node] process.failed node=80 name=MediaMpegTsDatagramMaterializerNode kind=MpegTsDatagramMaterializer inputs=[channel=118 edge=118 stream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=120 edge=120 stream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=121 edge=121 stream=Metadata edge_kind=Unknown payload=Unknown queued=10] outputs=[channel=122 edge=122 stream=Metadata edge_kind=Unknown payload=Unknown queued=0] error=InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=1370513284 materialized_ns=1457017000 protocol_deadline_ns=1437179952 deadline_before_release=0 materialized_after_deadline=1
[2026-10-19 09:15:48.870] [info] [graph][runtime.node] worker.failed node=80 kind=MpegTsDatagramMaterializer name=MPEG-TS wire protocol materializer error=InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=1370513284 materialized_ns=1457017000 protocol_deadline_ns=1437179952 deadline_before_release=0 materialized_after_deadline=1
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=8 available=1 cpu_ns=0 process_calls=1539
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] process.failed node=82 name=MediaScheduledDatagramSenderNode kind=ScheduledDatagramSender inputs=[channel=111 edge=111 stream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=122 edge=122 stream=Metadata edge_kind=Unknown payload=Unknown queued=0] outputs=[] error=Cancelled: scheduled datagram sender reservation wait was interrupted
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=46 available=1 cpu_ns=0 process_calls=1427
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=47 available=1 cpu_ns=0 process_calls=1340
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=11 available=1 cpu_ns=0 process_calls=136
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=28 available=1 cpu_ns=0 process_calls=3
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=31 available=1 cpu_ns=0 process_calls=3
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=48 available=1 cpu_ns=0 process_calls=3
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=50 available=1 cpu_ns=0 process_calls=138
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=29 available=1 cpu_ns=0 process_calls=4
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=10 available=1 cpu_ns=0 process_calls=4
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=14 available=1 cpu_ns=0 process_calls=148
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=49 available=1 cpu_ns=0 process_calls=6
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=17 available=1 cpu_ns=0 process_calls=4
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=9 available=1 cpu_ns=0 process_calls=3
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=12 available=1 cpu_ns=0 process_calls=4
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=37 available=1 cpu_ns=0 process_calls=126
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=30 available=1 cpu_ns=0 process_calls=139
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=62 available=1 cpu_ns=0 process_calls=329
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=60 available=1 cpu_ns=0 process_calls=274
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=51 available=1 cpu_ns=0 process_calls=196
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=52 available=1 cpu_ns=0 process_calls=3
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=22 available=1 cpu_ns=31250000 process_calls=111
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=13 available=1 cpu_ns=0 process_calls=5
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=73 available=1 cpu_ns=0 process_calls=245
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=68 available=1 cpu_ns=78125000 process_calls=127
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=32 available=1 cpu_ns=0 process_calls=5
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=53 available=1 cpu_ns=0 process_calls=6
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=7 available=1 cpu_ns=0 process_calls=184
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=33 available=1 cpu_ns=0 process_calls=162
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=54 available=1 cpu_ns=0 process_calls=161
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=36 available=1 cpu_ns=0 process_calls=4
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=55 available=1 cpu_ns=0 process_calls=224
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=59 available=1 cpu_ns=0 process_calls=3
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=34 available=1 cpu_ns=0 process_calls=154
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=15 available=1 cpu_ns=0 process_calls=150
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=57 available=1 cpu_ns=0 process_calls=231
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=16 available=1 cpu_ns=0 process_calls=153
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=35 available=1 cpu_ns=0 process_calls=154
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=56 available=1 cpu_ns=0 process_calls=162
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=58 available=1 cpu_ns=0 process_calls=357
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=18 available=1 cpu_ns=0 process_calls=123
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=19 available=1 cpu_ns=0 process_calls=120
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=38 available=1 cpu_ns=0 process_calls=115
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=80 available=1 cpu_ns=15625000 process_calls=385
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=61 available=1 cpu_ns=0 process_calls=271
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=20 available=1 cpu_ns=0 process_calls=159
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=39 available=1 cpu_ns=0 process_calls=164
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=41 available=1 cpu_ns=15625000 process_calls=111
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=70 available=1 cpu_ns=31250000 process_calls=229
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=64 available=1 cpu_ns=15625000 process_calls=147
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=23 available=1 cpu_ns=0 process_calls=166
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=42 available=1 cpu_ns=0 process_calls=164
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=71 available=1 cpu_ns=0 process_calls=213
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=65 available=1 cpu_ns=0 process_calls=164
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=24 available=1 cpu_ns=0 process_calls=165
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=81 available=1 cpu_ns=0 process_calls=6
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=66 available=1 cpu_ns=0 process_calls=167
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=43 available=1 cpu_ns=0 process_calls=163
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=72 available=1 cpu_ns=0 process_calls=167
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=26 available=1 cpu_ns=31250000 process_calls=124
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=45 available=1 cpu_ns=46875000 process_calls=122
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=25 available=1 cpu_ns=0 process_calls=170
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=44 available=1 cpu_ns=0 process_calls=175
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=6 available=1 cpu_ns=15625000 process_calls=268
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=2 available=1 cpu_ns=31250000 process_calls=90
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=67 available=1 cpu_ns=0 process_calls=180
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=78 available=1 cpu_ns=0 process_calls=5
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=4 available=1 cpu_ns=31250000 process_calls=131
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=77 available=1 cpu_ns=0 process_calls=3
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=5 available=1 cpu_ns=0 process_calls=121
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=74 available=1 cpu_ns=15625000 process_calls=208
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=75 available=1 cpu_ns=0 process_calls=204
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=79 available=1 cpu_ns=0 process_calls=207
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=76 available=1 cpu_ns=15625000 process_calls=247
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=27 available=1 cpu_ns=0 process_calls=1422
[2026-10-19 09:15:48.871] [info] [graph][runtime.node] worker.thread_cpu node=82 available=1 cpu_ns=78125000 process_calls=190
[2026-10-19 09:15:48.879] [info] [graph][runtime.node] scheduled_datagram_sender stage=failed generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=93 committed_datagrams=645 committed_payload_bytes=785308 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=80004 pacing_reserved=646 pacing_cancelled=0 pacing_submitted=645 pacing_maximum_submit_lateness_ns=8374580 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=21 backlog_current_wire_bytes=28288 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=80004 backlog_max_residence_ns=39627300 last_materialized_sequence=666 last_scheduled_sequence=666 last_submitted_sequence=645 last_committed_sequence=645 shared_scope_charged_datagrams=645 shared_scope_charged_wire_bytes=803368 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=645 evidence_timestamp_tracked=0 evidence_timestamp_untracked=645 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=379902 endpoint_1_target_effective_socket_bytes=189951 endpoint_1_api_requested_socket_bytes=189951 endpoint_1_effective_socket_bytes=189951 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=645 endpoint_1_committed_payload_bytes=785308 endpoint_2_target_effective_socket_bytes=189951 endpoint_2_api_requested_socket_bytes=189951 endpoint_2_effective_socket_bytes=189951 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=0 endpoint_2_committed_payload_bytes=0
[2026-10-19 09:15:49.030] [info] [graph][runtime.lifecycle] node_abort_payload node=82 before_objects=197 after_objects=197 before_bytes=358392 after_bytes=358392 before_reservations=1761 after_reservations=1761 before_releases=1564 after_releases=1564
[2026-10-19 09:15:49.030] [info] [graph][runtime.node] mpegts_wire_materializer stage=aborted maximum_materialized_after_release_ns=66317432 worst_materialized_at_ns=1436830700 worst_release_ns=1370513268 worst_deadline_ns=1470513268 worst_global_sequence=609
[2026-10-19 09:15:49.030] [info] [graph][runtime.lifecycle] node_abort_payload node=80 before_objects=197 after_objects=197 before_bytes=358392 after_bytes=358392 before_reservations=1761 after_reservations=1761 before_releases=1564 after_releases=1564
[2026-10-19 09:15:49.030] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 datagrams=0 wire_bytes=0 immediate_deadlines=0 deferred_deadlines=0 planned_wait_ns=0 late_datagrams=0 maximum_lateness_ns=0 pending_bytes=0 peak_pending_bytes=231428 pressure_failures=0 access_units=68 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=10 maximum_mux_input_after_emit_ns=8928032 maximum_mux_input_after_dispatch_ns=-91071968 worst_mux_input_ready_ns=579441300 worst_mux_input_emit_ns=570513268 worst_mux_input_dispatch_ns=670513268 maximum_protocol_batch_produced_after_release_ns=8965732 worst_protocol_batch_produced_ns=579479000 worst_protocol_batch_release_ns=570513268 worst_protocol_batch_deadline_ns=670513268 worst_protocol_batch_datagrams=1 exit_reason=aborted
[2026-10-19 09:15:49.030] [info] [graph][runtime.lifecycle] node_abort_payload node=76 before_objects=197 after_objects=197 before_bytes=358392 after_bytes=358392 before_reservations=1761 after_reservations=1761 before_releases=1564 after_releases=1564
[2026-10-19 09:15:49.030] [info] [graph][runtime.lifecycle] node_abort_payload node=79 before_objects=197 after_objects=197 before_bytes=358392 after_bytes=358392 before_reservations=1761 after_reservations=1761 before_releases=1564 after_releases=1564
[2026-10-19 09:15:49.030] [info] [graph][runtime.lifecycle] node_abort_payload node=75 before_objects=197 after_objects=197 before_bytes=358392 after_bytes=358392 before_reservations=1761 after_reservations=1761 before_releases=1564 after_releases=1564
[2026-10-19 09:15:49.031] [info] [graph][runtime.node] audio_stage_observation node=74 stage=scheduler_input count=40 last_generation=1 last_begin=39936 last_end=40960 last_rate=44100 last_steady_ns=1962787368700 maximum_inter_event_gap_ns=27923200 gap_begin_generation=1 gap_begin_begin=0 gap_begin_end=1024 gap_begin_rate=44100 gap_begin_steady_ns=1961881905700 gap_end_generation=1 gap_end_begin=1024 gap_end_end=2048 gap_end_rate=44100 gap_end_steady_ns=1961909828900
[2026-10-19 09:15:49.031] [info] [graph][runtime.node] av_scheduler_correlated_wait generation=1 canonical_sequence=1 missing=audio begin_steady_ns=1961844994600 end_steady_ns=1961882059500 observed_wait_ns=37064900 held_dispatch_master_ns=670513268 begin_master_ns=514527100 end_master_ns=551591500 dispatch_remaining_at_end_ns=118921768 resumed_dispatch_master_ns=670513268
[2026-10-19 09:15:49.031] [info] [graph][runtime.lifecycle] node_abort_payload node=74 before_objects=197 after_objects=196 before_bytes=358392 after_bytes=325828 before_reservations=1761 after_reservations=1761 before_releases=1564 after_releases=1565
[2026-10-19 09:15:49.031] [info] [graph][runtime.node] audio_stage_observation node=5 stage=canonicalizer_input count=40 last_generation=1 last_begin=39936 last_end=40960 last_rate=44100 last_steady_ns=1962787352200 maximum_inter_event_gap_ns=27944200 gap_begin_generation=1 gap_begin_begin=0 gap_begin_end=1024 gap_begin_rate=44100 gap_begin_steady_ns=1961881861500 gap_end_generation=1 gap_end_begin=1024 gap_end_end=2048 gap_end_rate=44100 gap_end_steady_ns=1961909805700
[2026-10-19 09:15:49.031] [info] [graph][runtime.node] audio_stage_observation node=5 stage=canonicalizer_committed count=40 last_generation=1 last_begin=39936 last_end=40960 last_rate=44100 last_steady_ns=1962787360900 maximum_inter_event_gap_ns=27927000 gap_begin_generation=1 gap_begin_begin=0 gap_begin_end=1024 gap_begin_rate=44100 gap_begin_steady_ns=1961881889900 gap_end_generation=1 gap_end_begin=1024 gap_end_end=2048 gap_end_rate=44100 gap_end_steady_ns=1961909816900
[2026-10-19 09:15:49.031] [info] [graph][runtime.lifecycle] node_abort_payload node=5 before_objects=196 after_objects=196 before_bytes=325828 after_bytes=325828 before_reservations=1761 after_reservations=1761 before_releases=1565 after_releases=1565
[2026-10-19 09:15:49.031] [info] [graph][runtime.lifecycle] node_abort_payload node=77 before_objects=196 after_objects=196 before_bytes=325828 after_bytes=325828 before_reservations=1761 after_reservations=1761 before_releases=1565 after_releases=1565
[2026-10-19 09:15:49.031] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_input count=42 last_generation=1 last_begin=41984 last_end=43008 last_rate=44100 last_steady_ns=1962786706300 maximum_inter_event_gap_ns=23739100 gap_begin_generation=1 gap_begin_begin=23552 gap_begin_end=24576 gap_begin_rate=44100 gap_begin_steady_ns=1962368802000 gap_end_generation=1 gap_end_begin=24576 gap_end_end=25600 gap_end_rate=44100 gap_end_steady_ns=1962392541100
[2026-10-19 09:15:49.031] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_submitted count=42 last_generation=1 last_begin=41984 last_end=43008 last_rate=44100 last_steady_ns=1962787323200 maximum_inter_event_gap_ns=28160300 gap_begin_generation=1 gap_begin_begin=2048 gap_begin_end=3072 gap_begin_rate=44100 gap_begin_steady_ns=1961881612200 gap_end_generation=1 gap_end_begin=3072 gap_end_end=4096 gap_end_rate=44100 gap_end_steady_ns=1961909772500
[2026-10-19 09:15:49.031] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_packet count=40 last_generation=1 last_begin=39936 last_end=40960 last_rate=44100 last_steady_ns=1962787336100 maximum_inter_event_gap_ns=28157400 gap_begin_generation=1 gap_begin_begin=0 gap_begin_end=1024 gap_begin_rate=44100 gap_begin_steady_ns=1961881625300 gap_end_generation=1 gap_end_begin=1024 gap_end_end=2048 gap_end_rate=44100 gap_end_steady_ns=1961909782700
[2026-10-19 09:15:49.031] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_committed count=40 last_generation=1 last_begin=39936 last_end=40960 last_rate=44100 last_steady_ns=1962787348700 maximum_inter_event_gap_ns=27953500 gap_begin_generation=1 gap_begin_begin=0 gap_begin_end=1024 gap_begin_rate=44100 gap_begin_steady_ns=1961881839900 gap_end_generation=1 gap_end_begin=1024 gap_end_end=2048 gap_end_rate=44100 gap_end_steady_ns=1961909793400
[2026-10-19 09:15:49.031] [info] [graph][runtime.lifecycle] codec_owner_release node=4 kind=AudioEncode owner_references=4
[2026-10-19 09:15:49.031] [info] [graph][runtime.lifecycle] node_abort_payload node=4 before_objects=196 after_objects=196 before_bytes=325828 after_bytes=325828 before_reservations=1761 after_reservations=1761 before_releases=1565 after_releases=1565
[2026-10-19 09:15:49.031] [info] [graph][runtime.lifecycle] node_abort_payload node=78 before_objects=196 after_objects=196 before_bytes=325828 after_bytes=325828 before_reservations=1761 after_reservations=1761 before_releases=1565 after_releases=1565
[2026-10-19 09:15:49.031] [info] [graph][runtime.lifecycle] node_abort_payload node=81 before_objects=196 after_objects=196 before_bytes=325828 after_bytes=325828 before_reservations=1761 after_reservations=1761 before_releases=1565 after_releases=1565
[2026-10-19 09:15:49.031] [info] [graph][runtime.node] audio_stage_observation node=7 stage=aggregate_built count=42 last_generation=1 last_begin=41984 last_end=43008 last_rate=44100 last_steady_ns=1962786672600 maximum_inter_event_gap_ns=23716000 gap_begin_generation=1 gap_begin_begin=23552 gap_begin_end=24576 gap_begin_rate=44100 gap_begin_steady_ns=1962368763900 gap_end_generation=1 gap_end_begin=24576 gap_end_end=25600 gap_end_rate=44100 gap_end_steady_ns=1962392479900
[2026-10-19 09:15:49.031] [info] [graph][runtime.node] audio_stage_observation node=7 stage=aggregate_committed count=42 last_generation=1 last_begin=41984 last_end=43008 last_rate=44100 last_steady_ns=1962786686000 maximum_inter_event_gap_ns=23719700 gap_begin_generation=1 gap_begin_begin=23552 gap_begin_end=24576 gap_begin_rate=44100 gap_begin_steady_ns=1962368782300 gap_end_generation=1 gap_end_begin=24576 gap_end_end=25600 gap_end_rate=44100 gap_end_steady_ns=1962392502000
[2026-10-19 09:15:49.032] [info] [graph][runtime.node] continuous_audio_aggregate.summary reason=abort output_group=composition-r256.output audio_commits=42 real_samples=43008 silence_samples=0
[2026-10-19 09:15:49.032] [info] [graph][runtime.lifecycle] node_abort_payload node=7 before_objects=196 after_objects=190 before_bytes=325828 after_bytes=284868 before_reservations=1761 after_reservations=1761 before_releases=1565 after_releases=1571
[2026-10-19 09:15:49.032] [info] [graph][runtime.lifecycle] codec_owner_release node=2 kind=VideoEncode owner_references=4
[2026-10-19 09:15:49.032] [info] [graph][runtime.lifecycle] node_abort_payload node=2 before_objects=190 after_objects=190 before_bytes=284868 after_bytes=284868 before_reservations=1761 after_reservations=1761 before_releases=1571 after_releases=1571
[2026-10-19 09:15:49.032] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=process count=268 maximum_ns=13852500 begin_steady_ns=1961867894700 end_steady_ns=1961881747200 video_cursor=1 audio_cursor=not_owned next_video_cursor=1 next_audio_cursor=not_owned error=None return_state=progress deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-10-19 09:15:49.032] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=purge count=268 maximum_ns=2500 begin_steady_ns=1962367726300 end_steady_ns=1962367728800 video_cursor=16 audio_cursor=not_owned next_video_cursor=16 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 09:15:49.032] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=bind count=268 maximum_ns=6300 begin_steady_ns=1961749041800 end_steady_ns=1961749048100 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 09:15:49.032] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=consume count=268 maximum_ns=160100 begin_steady_ns=1962235000200 end_steady_ns=1962235160300 video_cursor=13 audio_cursor=not_owned next_video_cursor=13 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 09:15:49.032] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=activate count=16 maximum_ns=70200 begin_steady_ns=1961834313200 end_steady_ns=1961834383400 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 09:15:49.032] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=build_video count=29 maximum_ns=13822800 begin_steady_ns=1961867911600 end_steady_ns=1961881734400 video_cursor=1 audio_cursor=not_owned next_video_cursor=1 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 09:15:49.032] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=canvas count=29 maximum_ns=3612800 begin_steady_ns=1961834407300 end_steady_ns=1961838020100 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 09:15:49.032] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=emit count=29 maximum_ns=278500 begin_steady_ns=1962035331600 end_steady_ns=1962035610100 video_cursor=6 audio_cursor=not_owned next_video_cursor=6 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 09:15:49.033] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=between_calls_previous_return count=267 maximum_ns=83562200 begin_steady_ns=1961749058600 end_steady_ns=1961832620800 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=waiting deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-10-19 09:15:49.033] [info] [graph][runtime.node] continuous_aggregate.summary reason=abort output_group=composition-r256.output output_generation=1 video_commits=29 source[0].group=composition-r256.source.0 source[0].real_frames=26 source[0].black_frames=3 source[0].contribution_generation=1 source[1].group=composition-r256.source.1 source[1].real_frames=26 source[1].black_frames=3 source[1].contribution_generation=1 source[2].group=composition-r256.source.2 source[2].real_frames=26 source[2].black_frames=3 source[2].contribution_generation=1
[2026-10-19 09:15:49.033] [info] [graph][runtime.node] cuda_canvas_call stage=context_push count=30 operation=upload maximum_ns=5300 begin_steady_ns=1961684449200 end_steady_ns=1961684454500 native_result=0 thread_cpu_ns=0
[2026-10-19 09:15:49.033] [info] [graph][runtime.node] cuda_canvas_call stage=source_wait count=110 operation=copy maximum_ns=59900 begin_steady_ns=1962669022200 end_steady_ns=1962669082100 native_result=0 thread_cpu_ns=0
[2026-10-19 09:15:49.033] [info] [graph][runtime.node] cuda_canvas_call stage=plane_copy count=222 operation=copy maximum_ns=678000 begin_steady_ns=1961835038800 end_steady_ns=1961835716800 native_result=0 thread_cpu_ns=0
[2026-10-19 09:15:49.033] [info] [graph][runtime.node] cuda_canvas_call stage=destination_wait count=2 operation=upload maximum_ns=87900 begin_steady_ns=1961684745800 end_steady_ns=1961684833700 native_result=0 thread_cpu_ns=0
[2026-10-19 09:15:49.033] [info] [graph][runtime.node] cuda_canvas_call stage=context_pop count=30 operation=copy maximum_ns=523300 begin_steady_ns=1961837491700 end_steady_ns=1961838015000 native_result=0 thread_cpu_ns=0
[2026-10-19 09:15:49.033] [info] [graph][runtime.node] cuda_canvas_call stage=batch_completion count=28 operation=copy maximum_ns=899200 begin_steady_ns=1961836592200 end_steady_ns=1961837491400 native_result=0 thread_cpu_ns=0
[2026-10-19 09:15:49.033] [info] [graph][runtime.node] canvas_compose_correlated scope=attempt begin_steady_ns=1961834407800 end_steady_ns=1961838019500 wall_ns=3611700 thread_cpu_ns=0 surface_count=1 surface_total_ns=500 surface_maximum_ns=500 surface_maximum_tile=none surface_maximum_result=unavailable validate_count=3 validate_total_ns=900 validate_maximum_ns=700 validate_maximum_tile=0 validate_maximum_result=0 black_count=1 black_total_ns=1303100 black_maximum_ns=1303100 black_maximum_tile=none black_maximum_result=0 tile_count=3 tile_total_ns=874000 tile_maximum_ns=460100 tile_maximum_tile=2 tile_maximum_result=0 publish_count=1 publish_total_ns=2300 publish_maximum_ns=2300 publish_maximum_tile=none publish_maximum_result=0 context_push_count=1 context_push_total_ns=1500 context_push_maximum_ns=1500 context_push_maximum_tile=none context_push_maximum_result=0 source_wait_count=4 source_wait_total_ns=21700 source_wait_maximum_ns=8200 source_wait_maximum_tile=1 source_wait_maximum_result=0 plane_copy_count=8 plane_copy_total_ns=2123400 plane_copy_maximum_ns=676700 plane_copy_maximum_tile=none plane_copy_maximum_result=0 destination_wait_count=0 destination_wait_total_ns=0 destination_wait_maximum_ns=0 destination_wait_maximum_tile=none destination_wait_maximum_result=unavailable context_pop_count=1 context_pop_total_ns=522400 context_pop_maximum_ns=522400 context_pop_maximum_tile=none context_pop_maximum_result=0 batch_completion_count=1 batch_completion_total_ns=898000 batch_completion_maximum_ns=898000 batch_completion_maximum_tile=none batch_completion_maximum_result=0
[2026-10-19 09:15:49.033] [info] [graph][runtime.lifecycle] node_abort_payload node=6 before_objects=190 after_objects=151 before_bytes=284868 after_bytes=284868 before_reservations=1761 after_reservations=1761 before_releases=1571 after_releases=1610
[2026-10-19 09:15:49.034] [info] [graph][runtime.lifecycle] node_abort_payload node=67 before_objects=151 after_objects=149 before_bytes=284868 after_bytes=284868 before_reservations=1761 after_reservations=1761 before_releases=1610 after_releases=1612
[2026-10-19 09:15:49.034] [info] [graph][runtime.lifecycle] node_abort_payload node=44 before_objects=149 after_objects=147 before_bytes=284868 after_bytes=284868 before_reservations=1761 after_reservations=1761 before_releases=1612 after_releases=1614
[2026-10-19 09:15:49.034] [info] [graph][runtime.lifecycle] node_abort_payload node=25 before_objects=147 after_objects=145 before_bytes=284868 after_bytes=284868 before_reservations=1761 after_reservations=1761 before_releases=1614 after_releases=1616
[2026-10-19 09:15:49.034] [info] [graph][runtime.lifecycle] codec_owner_release node=73 kind=AudioResample owner_references=1
[2026-10-19 09:15:49.034] [info] [graph][runtime.lifecycle] node_abort_payload node=73 before_objects=145 after_objects=143 before_bytes=284868 after_bytes=268484 before_reservations=1761 after_reservations=1761 before_releases=1616 after_releases=1618
[2026-10-19 09:15:49.036] [info] [graph][runtime.lifecycle] node_abort_payload node=68 before_objects=143 after_objects=142 before_bytes=268484 after_bytes=268484 before_reservations=1761 after_reservations=1761 before_releases=1618 after_releases=1619
[2026-10-19 09:15:49.038] [info] [graph][runtime.lifecycle] node_abort_payload node=45 before_objects=142 after_objects=141 before_bytes=268484 after_bytes=268484 before_reservations=1761 after_reservations=1761 before_releases=1619 after_releases=1620
[2026-10-19 09:15:49.039] [info] [graph][runtime.lifecycle] node_abort_payload node=26 before_objects=141 after_objects=140 before_bytes=268484 after_bytes=268484 before_reservations=1761 after_reservations=1761 before_releases=1620 after_releases=1621
[2026-10-19 09:15:49.040] [info] [graph][runtime.lifecycle] node_abort_payload node=72 before_objects=140 after_objects=140 before_bytes=268484 after_bytes=268484 before_reservations=1761 after_reservations=1761 before_releases=1621 after_releases=1621
[2026-10-19 09:15:49.040] [info] [graph][runtime.lifecycle] node_abort_payload node=66 before_objects=140 after_objects=139 before_bytes=268484 after_bytes=268484 before_reservations=1761 after_reservations=1761 before_releases=1621 after_releases=1622
[2026-10-19 09:15:49.040] [info] [graph][runtime.lifecycle] node_abort_payload node=43 before_objects=139 after_objects=138 before_bytes=268484 after_bytes=268484 before_reservations=1761 after_reservations=1761 before_releases=1622 after_releases=1623
[2026-10-19 09:15:49.040] [info] [graph][runtime.lifecycle] node_abort_payload node=24 before_objects=138 after_objects=137 before_bytes=268484 after_bytes=268484 before_reservations=1761 after_reservations=1761 before_releases=1623 after_releases=1624
[2026-10-19 09:15:49.040] [info] [graph][runtime.lifecycle] node_abort_payload node=71 before_objects=137 after_objects=136 before_bytes=268484 after_bytes=260292 before_reservations=1761 after_reservations=1761 before_releases=1624 after_releases=1625
[2026-10-19 09:15:49.040] [info] [graph][runtime.lifecycle] node_abort_payload node=65 before_objects=136 after_objects=135 before_bytes=260292 after_bytes=260292 before_reservations=1761 after_reservations=1761 before_releases=1625 after_releases=1626
[2026-10-19 09:15:49.040] [info] [graph][runtime.lifecycle] node_abort_payload node=42 before_objects=135 after_objects=134 before_bytes=260292 after_bytes=260292 before_reservations=1761 after_reservations=1761 before_releases=1626 after_releases=1627
[2026-10-19 09:15:49.040] [info] [graph][runtime.lifecycle] node_abort_payload node=23 before_objects=134 after_objects=133 before_bytes=260292 after_bytes=260292 before_reservations=1761 after_reservations=1761 before_releases=1627 after_releases=1628
[2026-10-19 09:15:49.040] [info] [graph][runtime.lifecycle] codec_owner_release node=70 kind=AudioDecode owner_references=2
[2026-10-19 09:15:49.040] [info] [graph][runtime.lifecycle] node_abort_payload node=70 before_objects=133 after_objects=132 before_bytes=260292 after_bytes=252100 before_reservations=1761 after_reservations=1761 before_releases=1628 after_releases=1629
[2026-10-19 09:15:49.040] [info] [graph][runtime.lifecycle] codec_owner_release node=64 kind=VideoDecode owner_references=2
[2026-10-19 09:15:49.046] [info] [graph][runtime.lifecycle] node_abort_payload node=64 before_objects=132 after_objects=132 before_bytes=252100 after_bytes=252100 before_reservations=1761 after_reservations=1761 before_releases=1629 after_releases=1629
[2026-10-19 09:15:49.046] [info] [graph][runtime.lifecycle] codec_owner_release node=41 kind=VideoDecode owner_references=2
[2026-10-19 09:15:49.051] [info] [graph][runtime.lifecycle] node_abort_payload node=41 before_objects=132 after_objects=132 before_bytes=252100 after_bytes=252100 before_reservations=1761 after_reservations=1761 before_releases=1629 after_releases=1629
[2026-10-19 09:15:49.051] [info] [graph][runtime.lifecycle] codec_owner_release node=22 kind=VideoDecode owner_references=2
[2026-10-19 09:15:49.057] [info] [graph][runtime.lifecycle] node_abort_payload node=22 before_objects=132 after_objects=132 before_bytes=252100 after_bytes=252100 before_reservations=1761 after_reservations=1761 before_releases=1629 after_releases=1629
[2026-10-19 09:15:49.057] [info] [graph][runtime.lifecycle] node_abort_payload node=62 before_objects=132 after_objects=132 before_bytes=252100 after_bytes=252100 before_reservations=1761 after_reservations=1761 before_releases=1629 after_releases=1629
[2026-10-19 09:15:49.057] [info] [graph][runtime.lifecycle] node_abort_payload node=39 before_objects=132 after_objects=132 before_bytes=252100 after_bytes=252100 before_reservations=1761 after_reservations=1761 before_releases=1629 after_releases=1629
[2026-10-19 09:15:49.057] [info] [graph][runtime.lifecycle] node_abort_payload node=20 before_objects=132 after_objects=132 before_bytes=252100 after_bytes=252100 before_reservations=1761 after_reservations=1761 before_releases=1629 after_releases=1629
[2026-10-19 09:15:49.057] [info] [graph][runtime.lifecycle] node_abort_payload node=61 before_objects=132 after_objects=132 before_bytes=252100 after_bytes=252100 before_reservations=1761 after_reservations=1761 before_releases=1629 after_releases=1629
[2026-10-19 09:15:49.057] [info] [graph][runtime.lifecycle] node_abort_payload node=38 before_objects=132 after_objects=132 before_bytes=252100 after_bytes=252100 before_reservations=1761 after_reservations=1761 before_releases=1629 after_releases=1629
[2026-10-19 09:15:49.057] [info] [graph][runtime.lifecycle] node_abort_payload node=19 before_objects=132 after_objects=132 before_bytes=252100 after_bytes=252100 before_reservations=1761 after_reservations=1761 before_releases=1629 after_releases=1629
[2026-10-19 09:15:49.057] [info] [graph][runtime.lifecycle] node_abort_payload node=60 before_objects=132 after_objects=132 before_bytes=252100 after_bytes=252100 before_reservations=1761 after_reservations=1761 before_releases=1629 after_releases=1629
[2026-10-19 09:15:49.058] [info] [graph][runtime.lifecycle] node_abort_payload node=37 before_objects=132 after_objects=132 before_bytes=252100 after_bytes=252100 before_reservations=1761 after_reservations=1761 before_releases=1629 after_releases=1629
[2026-10-19 09:15:49.058] [info] [graph][runtime.lifecycle] node_abort_payload node=18 before_objects=132 after_objects=132 before_bytes=252100 after_bytes=252100 before_reservations=1761 after_reservations=1761 before_releases=1629 after_releases=1629
[2026-10-19 09:15:49.058] [info] [graph][runtime.lifecycle] node_abort_payload node=58 before_objects=132 after_objects=132 before_bytes=252100 after_bytes=252100 before_reservations=1761 after_reservations=1761 before_releases=1629 after_releases=1629
[2026-10-19 09:15:49.058] [info] [graph][runtime.lifecycle] node_abort_payload node=35 before_objects=132 after_objects=132 before_bytes=252100 after_bytes=252100 before_reservations=1761 after_reservations=1761 before_releases=1629 after_releases=1629
[2026-10-19 09:15:49.058] [info] [graph][runtime.lifecycle] node_abort_payload node=16 before_objects=132 after_objects=132 before_bytes=252100 after_bytes=252100 before_reservations=1761 after_reservations=1761 before_releases=1629 after_releases=1629
[2026-10-19 09:15:49.058] [info] [graph][runtime.lifecycle] node_abort_payload node=57 before_objects=132 after_objects=132 before_bytes=252100 after_bytes=252100 before_reservations=1761 after_reservations=1761 before_releases=1629 after_releases=1629
[2026-10-19 09:15:49.058] [info] [graph][runtime.lifecycle] node_abort_payload node=56 before_objects=132 after_objects=132 before_bytes=252100 after_bytes=252100 before_reservations=1761 after_reservations=1761 before_releases=1629 after_releases=1629
[2026-10-19 09:15:49.058] [info] [graph][runtime.lifecycle] node_abort_payload node=34 before_objects=132 after_objects=132 before_bytes=252100 after_bytes=252100 before_reservations=1761 after_reservations=1761 before_releases=1629 after_releases=1629
[2026-10-19 09:15:49.058] [info] [graph][runtime.lifecycle] node_abort_payload node=15 before_objects=132 after_objects=132 before_bytes=252100 after_bytes=252100 before_reservations=1761 after_reservations=1761 before_releases=1629 after_releases=1629
[2026-10-19 09:15:49.058] [info] [graph][runtime.lifecycle] node_abort_payload node=59 before_objects=132 after_objects=132 before_bytes=252100 after_bytes=252100 before_reservations=1761 after_reservations=1761 before_releases=1629 after_releases=1629
[2026-10-19 09:15:49.058] [info] [graph][runtime.lifecycle] node_abort_payload node=55 before_objects=132 after_objects=132 before_bytes=252100 after_bytes=252100 before_reservations=1761 after_reservations=1761 before_releases=1629 after_releases=1629
[2026-10-19 09:15:49.058] [info] [graph][runtime.lifecycle] node_abort_payload node=54 before_objects=132 after_objects=132 before_bytes=252100 after_bytes=252100 before_reservations=1761 after_reservations=1761 before_releases=1629 after_releases=1629
[2026-10-19 09:15:49.058] [info] [graph][runtime.lifecycle] node_abort_payload node=36 before_objects=132 after_objects=132 before_bytes=252100 after_bytes=252100 before_reservations=1761 after_reservations=1761 before_releases=1629 after_releases=1629
[2026-10-19 09:15:49.058] [info] [graph][runtime.lifecycle] node_abort_payload node=33 before_objects=132 after_objects=132 before_bytes=252100 after_bytes=252100 before_reservations=1761 after_reservations=1761 before_releases=1629 after_releases=1629
[2026-10-19 09:15:49.058] [info] [graph][runtime.lifecycle] node_abort_payload node=17 before_objects=132 after_objects=132 before_bytes=252100 after_bytes=252100 before_reservations=1761 after_reservations=1761 before_releases=1629 after_releases=1629
[2026-10-19 09:15:49.059] [info] [graph][runtime.lifecycle] node_abort_payload node=14 before_objects=132 after_objects=132 before_bytes=252100 after_bytes=252100 before_reservations=1761 after_reservations=1761 before_releases=1629 after_releases=1629
[2026-10-19 09:15:49.059] [info] [graph][runtime.lifecycle] node_abort_payload node=53 before_objects=132 after_objects=132 before_bytes=252100 after_bytes=252100 before_reservations=1761 after_reservations=1761 before_releases=1629 after_releases=1629
[2026-10-19 09:15:49.059] [info] [graph][runtime.lifecycle] node_abort_payload node=32 before_objects=132 after_objects=132 before_bytes=252100 after_bytes=252100 before_reservations=1761 after_reservations=1761 before_releases=1629 after_releases=1629
[2026-10-19 09:15:49.059] [info] [graph][runtime.lifecycle] node_abort_payload node=13 before_objects=132 after_objects=132 before_bytes=252100 after_bytes=252100 before_reservations=1761 after_reservations=1761 before_releases=1629 after_releases=1629
[2026-10-19 09:15:49.059] [info] [graph][runtime.lifecycle] node_abort_payload node=52 before_objects=132 after_objects=132 before_bytes=252100 after_bytes=252100 before_reservations=1761 after_reservations=1761 before_releases=1629 after_releases=1629
[2026-10-19 09:15:49.059] [info] [graph][runtime.lifecycle] node_abort_payload node=51 before_objects=132 after_objects=132 before_bytes=252100 after_bytes=252100 before_reservations=1761 after_reservations=1761 before_releases=1629 after_releases=1629
[2026-10-19 09:15:49.059] [info] [graph][runtime.lifecycle] node_abort_payload node=50 before_objects=132 after_objects=131 before_bytes=252100 after_bytes=235570 before_reservations=1761 after_reservations=1761 before_releases=1629 after_releases=1630
[2026-10-19 09:15:49.059] [info] [graph][runtime.lifecycle] node_abort_payload node=31 before_objects=131 after_objects=131 before_bytes=235570 after_bytes=235570 before_reservations=1761 after_reservations=1761 before_releases=1630 after_releases=1630
[2026-10-19 09:15:49.059] [info] [graph][runtime.lifecycle] node_abort_payload node=30 before_objects=131 after_objects=130 before_bytes=235570 after_bytes=219040 before_reservations=1761 after_reservations=1761 before_releases=1630 after_releases=1631
[2026-10-19 09:15:49.059] [info] [graph][runtime.lifecycle] node_abort_payload node=12 before_objects=130 after_objects=130 before_bytes=219040 after_bytes=219040 before_reservations=1761 after_reservations=1761 before_releases=1631 after_releases=1631
[2026-10-19 09:15:49.059] [info] [graph][runtime.lifecycle] node_abort_payload node=11 before_objects=130 after_objects=129 before_bytes=219040 after_bytes=202510 before_reservations=1761 after_reservations=1761 before_releases=1631 after_releases=1632
[2026-10-19 09:15:49.059] [info] [graph][runtime.lifecycle] node_abort_payload node=49 before_objects=129 after_objects=129 before_bytes=202510 after_bytes=202510 before_reservations=1761 after_reservations=1761 before_releases=1632 after_releases=1632
[2026-10-19 09:15:49.059] [info] [graph][runtime.lifecycle] node_abort_payload node=29 before_objects=129 after_objects=129 before_bytes=202510 after_bytes=202510 before_reservations=1761 after_reservations=1761 before_releases=1632 after_releases=1632
[2026-10-19 09:15:49.059] [info] [graph][runtime.lifecycle] node_abort_payload node=10 before_objects=129 after_objects=129 before_bytes=202510 after_bytes=202510 before_reservations=1761 after_reservations=1761 before_releases=1632 after_releases=1632
[2026-10-19 09:15:49.059] [info] [graph][runtime.lifecycle] node_abort_payload node=69 before_objects=129 after_objects=129 before_bytes=202510 after_bytes=202510 before_reservations=1761 after_reservations=1761 before_releases=1632 after_releases=1632
[2026-10-19 09:15:49.059] [info] [graph][runtime.lifecycle] node_abort_payload node=48 before_objects=129 after_objects=129 before_bytes=202510 after_bytes=202510 before_reservations=1761 after_reservations=1761 before_releases=1632 after_releases=1632
[2026-10-19 09:15:49.060] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-10-19 09:15:49.060] [info] [graph][runtime.lifecycle] node_abort_payload node=63 before_objects=129 after_objects=129 before_bytes=202510 after_bytes=202510 before_reservations=1761 after_reservations=1761 before_releases=1632 after_releases=1632
[2026-10-19 09:15:49.060] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-10-19 09:15:49.060] [info] [graph][runtime.lifecycle] node_abort_payload node=40 before_objects=129 after_objects=129 before_bytes=202510 after_bytes=202510 before_reservations=1761 after_reservations=1761 before_releases=1632 after_releases=1632
[2026-10-19 09:15:49.060] [info] [graph][runtime.lifecycle] node_abort_payload node=28 before_objects=129 after_objects=129 before_bytes=202510 after_bytes=202510 before_reservations=1761 after_reservations=1761 before_releases=1632 after_releases=1632
[2026-10-19 09:15:49.060] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-10-19 09:15:49.060] [info] [graph][runtime.lifecycle] node_abort_payload node=21 before_objects=129 after_objects=129 before_bytes=202510 after_bytes=202510 before_reservations=1761 after_reservations=1761 before_releases=1632 after_releases=1632
[2026-10-19 09:15:49.060] [info] [graph][runtime.lifecycle] node_abort_payload node=9 before_objects=129 after_objects=129 before_bytes=202510 after_bytes=202510 before_reservations=1761 after_reservations=1761 before_releases=1632 after_releases=1632
[2026-10-19 09:15:49.060] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=62 completion_waits=62 completed_datagrams=62 drained_datagrams=1 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-19 09:15:49.061] [info] [graph][runtime.lifecycle] node_abort_payload node=47 before_objects=129 after_objects=129 before_bytes=202510 after_bytes=202510 before_reservations=1761 after_reservations=1761 before_releases=1632 after_releases=1632
[2026-10-19 09:15:49.061] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=687 completion_waits=687 completed_datagrams=1629 drained_datagrams=943 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-19 09:15:49.061] [info] [graph][runtime.lifecycle] node_abort_payload node=46 before_objects=129 after_objects=129 before_bytes=202510 after_bytes=202510 before_reservations=1761 after_reservations=1761 before_releases=1632 after_releases=1632
[2026-10-19 09:15:49.061] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=986 completion_waits=986 completed_datagrams=1808 drained_datagrams=823 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-19 09:15:49.061] [info] [graph][runtime.lifecycle] node_abort_payload node=27 before_objects=129 after_objects=129 before_bytes=202510 after_bytes=202510 before_reservations=1761 after_reservations=1761 before_releases=1632 after_releases=1632
[2026-10-19 09:15:49.062] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=1006 completion_waits=1006 completed_datagrams=1860 drained_datagrams=855 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-19 09:15:49.062] [info] [graph][runtime.lifecycle] node_abort_payload node=8 before_objects=129 after_objects=129 before_bytes=202510 after_bytes=202510 before_reservations=1761 after_reservations=1761 before_releases=1632 after_releases=1632
[2026-10-19 09:15:49.062] [info] [graph][runtime.lifecycle] node_abort_payload node=3 before_objects=129 after_objects=129 before_bytes=202510 after_bytes=202510 before_reservations=1761 after_reservations=1761 before_releases=1632 after_releases=1632
[2026-10-19 09:15:49.062] [info] [graph][runtime.node] codec_resolver.release slot=encoder_parameters bufferReferences=1 hasCredit=0 creditReferences=0
[2026-10-19 09:15:49.062] [info] [graph][runtime.lifecycle] node_abort_payload node=1 before_objects=129 after_objects=129 before_bytes=202510 after_bytes=202510 before_reservations=1761 after_reservations=1761 before_releases=1632 after_releases=1632
[2026-10-19 09:15:49.112] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=failed detail="protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=1370513284 materialized_ns=1457017000 protocol_deadline_ns=1437179952 deadline_before_release=0 materialized_after_deadline=1" description="D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r256-output.sdp" error=1 native=0 stage=3 reason="protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=1370513284 materialized_ns=1457017000 protocol_deadline_ns=1437179952 deadline_before_release=0 materialized_after_deadline=1"
[2026-10-19 09:15:49.113] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=retired detail="protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=1370513284 materialized_ns=1457017000 protocol_deadline_ns=1437179952 deadline_before_release=0 materialized_after_deadline=1" description="D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r256-output.sdp" error=1 native=0 stage=3 reason="protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=1370513284 materialized_ns=1457017000 protocol_deadline_ns=1437179952 deadline_before_release=0 materialized_after_deadline=1"
[CLI] final runtime report: queued=0, peakQueued=391, threads=82, processThreads=132, workers=0, workerProgress=3731, workerProcessCalls=15501, workerWaits=11762, workerWakeups=11617, workerErrors=1, errors=1, stalledIntervals=0, cpuSamples=3, logicalProcessors=22, averageSystemMachineCpuPercent=19.341284, averageProcessMachineCpuPercent=2.495405, peakProcessMachineCpuPercent=3.724395, averageProcessSingleCoreCpuPercent=54.898905, peakProcessSingleCoreCpuPercent=81.936685, initialWorkingSetBytes=189472768, workingSetBytes=237092864, peakWorkingSetBytes=237092864, totalPushed=3483, totalPopped=3368, droppedBuffers=0, encodedPacketsPushed=313, encodedPacketsPopped=313, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=4960888, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=212, graphPayloadReservations=1761, graphPayloadReleases=1761, graphPayloadPressureFailures=0, lastInputReceivedAtNs=1962775954900
```
```text
[in @ 000001659355c040] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001659355c040] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 46591.777689
[in @ 000001659388bf80] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001659388bf80] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 27515.3768
[in @ 0000016599658340] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000016599658340] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 14947.778133
[in @ 00000165993287c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000016599329240] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000016599326f40] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000016599326f40] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 46591.777689
[in @ 0000016599329240] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 27515.3768
[in @ 00000165993287c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 14947.778133
[aac @ 00000165998f1300] Qavg: 14825.154
[aac @ 00000165998f1300] 2 frames left in the queue on closing
[CLI] realtime video runtime failed: InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=1370513284 materialized_ns=1457017000 protocol_deadline_ns=1437179952 deadline_before_release=0 materialized_after_deadline=1
```

源退出均-1（失败后清理），不计自然结束。清理文件清单：
composition-r256-cli-error.log
composition-r256-cli.log
composition-r256-monitor.log
composition-r256-output.sdp
composition-r256-source0-stdout.log
composition-r256-source0.log
composition-r256-source1-stdout.log
composition-r256-source1.log
composition-r256-source2-stdout.log
composition-r256-source2.log
composition-r256-vlc.log

清理复核：11件清单产物逐项删除、前缀无残留、五PID无残留；指定源保留。protocol_batch_trace已归档；此轮无late mux AU记录，不能称复现r254。

## 当前代码边界复核

MediaDatagramTransportPlan仍显式设置commitBeforeNextReservation=true；GlobalSequenceState在任一前笔未完成时阻塞，并在整笔完成后通知等待者。现有NetworkMemoryResource及global固定数组已经接入（r243），不能笼统称网络PMR完全未实现。当前RTP materializer的payload嵌套vector、timestamps、deadlines、globalEntries、actions和protocol state仍使用普通分配；WireDatagramBatchBuilder的payload/entries仍动态增长。r252/r253修复的是媒体frame allocation完成所有权，不等于这些网络事务workspace已受同一资源合同管理。继续按当前代码补齐实际存活存储准入后再评审取消serial，不能仅翻转布尔值。
