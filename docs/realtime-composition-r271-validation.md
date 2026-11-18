# r271：硬件帧上下文保活诊断复验

状态：失败，已完成诊断归档与清理。Release全量重建95859成功，configure/build退出0；双独立局部静态PASS。三路H264720p30约8Mbps完整120秒，所选AAC44100双声道；输出HEVC720p30 8000kbps CBR、AAC192kbps CBR、MPEGTS/RTP。附加ProcDump的诊断运行，不替代最终验收。仅realtime。

## 实际启动命令
~~~powershell
$ErrorActionPreference='Stop'
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r271-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r271-native- --snapshot-format=png rtp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r271-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r271-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r271-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r271-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r271-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r271-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r271 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r271-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r271-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r271-cli-error.log' -PassThru
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
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r271-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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
~~~

运行：会话65082，CLI2808，VLC34320，三个源32216/35192/34368。二进制6198272B、16:26:20，SHA256 8046C92389E336B635C6506D238C8F48162DA8F7A45DE2D4EA86DD8A70F0971D。ProcDump PID22848/会话12301，命令：D:/Procdump/procdump.exe -ma -e -n 1 2808 D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r271-crash.dmp。

## 诊断结果：FAIL，清理路径本次未崩溃

16:27:20 VLC原生快照已查看，三格画面正常、右下黑。16:27:34在首轮源期间串行准入8次WouldBlock，余17包等待整笔提交，count/bytes未耗尽；材料化超期9677364ns。CLI正常报告该InvalidArgument并退出1（非caller_stop），ProcDump会话12301报告Process Exit 0x00000001、Dump count not reached，没有dump。未进入失源/重入验证，不得标为完整验收通过。

最终errors=1/workerErrors=1、dropped=0、queued=0，payload bytes/objects均0，reservations/releases=62210；高水17115191B/745对象。CPU平均2.134778%、峰值5.8%，WS峰值266014720B；sender最大提交迟到17.48432ms，delivery_evidence=not_proven。最长canvas23.6106ms，对应三个源查询Ready、batch_completion23.4088ms。上述均为附加调试器的诊断结果，不替代无调试器实流门禁。

本次证明该故障触发后的清理完整返回并归还载荷；没有异常栈，不能确认r270唯一根因，更不能关闭重入及完整预算验收。

### 原始关键记录
~~~text
[CLI] composition_inputs=3 grid=fixed audio_source=2 audio=on max_duration=source_driven hw=planner-highest-score
[2026-10-19 16:26:39.664] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-19 16:26:39.666] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-19 16:26:44.074] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-10-19 16:26:44.077] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-10-19 16:26:44.082] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-10-19 16:26:44.188] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-19 16:26:44.189] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-19 16:26:44.253] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-19 16:26:44.253] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-19 16:26:44.354] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-10-19 16:26:44.396] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-10-19 16:26:44.396] [info] [graph][planner.capability] canvas_cuda_stream stage=prepared device_stream=0000000000000000 private_nonblocking=1
[2026-10-19 16:26:44.402] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-19 16:26:44.403] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-19 16:26:44.403] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-10-19 16:26:44.410] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-19 16:26:44.410] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-19 16:26:44.410] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-10-19 16:26:44.415] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-19 16:26:44.416] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-19 16:26:44.416] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-10-19 16:26:44.446] [info] [graph][planner.capability] composition_preparation_storage stage=graph_bound scope=shared_reservations capacity=2147483648 retained=848052471 high_water=865340943
[CLI] audio_plan branch=transcode_frame reason=transcode_required_by_output_frame_contract codec=aac sample_rate=44100 channels=2 access_unit_samples=1024 encoder=aac bitrate_kbps=192
[CLI] prepared_output_id=1
[CLI] selected_chain=fixed-grid-composition score=0 decoder=h264,h264,h264 filter=scale_cuda=640:360,scale_cuda=640:360,scale_cuda=640:360 encoder=hevc_nvenc
[2026-10-19 16:26:44.449] [info] [graph][runtime.lifecycle] compile.begin
[2026-10-19 16:26:44.450] [info] [graph][runtime.channel] create channel=1 edge=1 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:26:44.450] [info] [graph][runtime.channel] create channel=2 edge=2 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:26:44.450] [info] [graph][runtime.channel] create channel=3 edge=3 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-19 16:26:44.450] [info] [graph][runtime.channel] create channel=4 edge=4 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.450] [info] [graph][runtime.channel] create channel=5 edge=5 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.450] [info] [graph][runtime.channel] create channel=6 edge=6 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.450] [info] [graph][runtime.channel] create channel=7 edge=7 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.450] [info] [graph][runtime.channel] create channel=8 edge=8 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.451] [info] [graph][runtime.channel] create channel=9 edge=9 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:26:44.451] [info] [graph][runtime.channel] create channel=10 edge=10 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.451] [info] [graph][runtime.channel] create channel=11 edge=11 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.451] [info] [graph][runtime.channel] create channel=12 edge=12 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:26:44.451] [info] [graph][runtime.channel] create channel=13 edge=13 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:26:44.451] [info] [graph][runtime.channel] create channel=14 edge=14 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.451] [info] [graph][runtime.channel] create channel=15 edge=15 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.452] [info] [graph][runtime.channel] create channel=16 edge=16 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.452] [info] [graph][runtime.channel] create channel=17 edge=17 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.452] [info] [graph][runtime.channel] create channel=18 edge=18 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.452] [info] [graph][runtime.channel] create channel=19 edge=19 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.452] [info] [graph][runtime.channel] create channel=20 edge=20 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.452] [info] [graph][runtime.channel] create channel=21 edge=21 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-10-19 16:26:44.452] [info] [graph][runtime.channel] create channel=22 edge=22 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:26:44.452] [info] [graph][runtime.channel] create channel=23 edge=23 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:26:44.452] [info] [graph][runtime.channel] create channel=24 edge=24 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:26:44.452] [info] [graph][runtime.channel] create channel=25 edge=25 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:26:44.452] [info] [graph][runtime.channel] create channel=26 edge=26 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:26:44.452] [info] [graph][runtime.channel] create channel=27 edge=27 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:26:44.453] [info] [graph][runtime.channel] create channel=28 edge=28 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:26:44.453] [info] [graph][runtime.channel] create channel=29 edge=29 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:26:44.453] [info] [graph][runtime.channel] create channel=30 edge=30 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.453] [info] [graph][runtime.channel] create channel=31 edge=31 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.453] [info] [graph][runtime.channel] create channel=32 edge=32 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.453] [info] [graph][runtime.channel] create channel=33 edge=33 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.453] [info] [graph][runtime.channel] create channel=34 edge=34 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.453] [info] [graph][runtime.channel] create channel=35 edge=35 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.453] [info] [graph][runtime.channel] create channel=36 edge=36 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:26:44.453] [info] [graph][runtime.channel] create channel=37 edge=37 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.453] [info] [graph][runtime.channel] create channel=38 edge=38 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.453] [info] [graph][runtime.channel] create channel=39 edge=39 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:26:44.453] [info] [graph][runtime.channel] create channel=40 edge=40 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:26:44.454] [info] [graph][runtime.channel] create channel=41 edge=41 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.454] [info] [graph][runtime.channel] create channel=42 edge=42 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.454] [info] [graph][runtime.channel] create channel=43 edge=43 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.454] [info] [graph][runtime.channel] create channel=44 edge=44 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.454] [info] [graph][runtime.channel] create channel=45 edge=45 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.454] [info] [graph][runtime.channel] create channel=46 edge=46 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.454] [info] [graph][runtime.channel] create channel=47 edge=47 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.454] [info] [graph][runtime.channel] create channel=48 edge=48 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-10-19 16:26:44.454] [info] [graph][runtime.channel] create channel=49 edge=49 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:26:44.454] [info] [graph][runtime.channel] create channel=50 edge=50 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:26:44.454] [info] [graph][runtime.channel] create channel=51 edge=51 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:26:44.454] [info] [graph][runtime.channel] create channel=52 edge=52 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:26:44.454] [info] [graph][runtime.channel] create channel=53 edge=53 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:26:44.455] [info] [graph][runtime.channel] create channel=54 edge=54 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:26:44.455] [info] [graph][runtime.channel] create channel=55 edge=55 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:26:44.455] [info] [graph][runtime.channel] create channel=56 edge=56 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:26:44.455] [info] [graph][runtime.channel] create channel=57 edge=57 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.455] [info] [graph][runtime.channel] create channel=58 edge=58 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.455] [info] [graph][runtime.channel] create channel=59 edge=59 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.455] [info] [graph][runtime.channel] create channel=60 edge=60 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.455] [info] [graph][runtime.channel] create channel=61 edge=61 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.455] [info] [graph][runtime.channel] create channel=62 edge=62 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.455] [info] [graph][runtime.channel] create channel=63 edge=63 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.455] [info] [graph][runtime.channel] create channel=64 edge=64 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.456] [info] [graph][runtime.channel] create channel=65 edge=65 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.456] [info] [graph][runtime.channel] create channel=66 edge=66 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:26:44.456] [info] [graph][runtime.channel] create channel=67 edge=67 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:26:44.456] [info] [graph][runtime.channel] create channel=68 edge=68 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.456] [info] [graph][runtime.channel] create channel=69 edge=69 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.456] [info] [graph][runtime.channel] create channel=70 edge=70 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.456] [info] [graph][runtime.channel] create channel=71 edge=71 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:26:44.456] [info] [graph][runtime.channel] create channel=72 edge=72 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:26:44.456] [info] [graph][runtime.channel] create channel=73 edge=73 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:26:44.456] [info] [graph][runtime.channel] create channel=74 edge=74 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:26:44.456] [info] [graph][runtime.channel] create channel=75 edge=75 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.456] [info] [graph][runtime.channel] create channel=76 edge=76 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.456] [info] [graph][runtime.channel] create channel=77 edge=77 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.456] [info] [graph][runtime.channel] create channel=78 edge=78 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.457] [info] [graph][runtime.channel] create channel=79 edge=79 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.457] [info] [graph][runtime.channel] create channel=80 edge=80 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.457] [info] [graph][runtime.channel] create channel=81 edge=81 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.457] [info] [graph][runtime.channel] create channel=82 edge=82 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.457] [info] [graph][runtime.channel] create channel=83 edge=83 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-10-19 16:26:44.457] [info] [graph][runtime.channel] create channel=84 edge=84 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:26:44.457] [info] [graph][runtime.channel] create channel=85 edge=85 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:26:44.457] [info] [graph][runtime.channel] create channel=86 edge=86 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:26:44.457] [info] [graph][runtime.channel] create channel=87 edge=87 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:26:44.457] [info] [graph][runtime.channel] create channel=88 edge=88 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:26:44.457] [info] [graph][runtime.channel] create channel=89 edge=89 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:26:44.457] [info] [graph][runtime.channel] create channel=90 edge=90 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:26:44.457] [info] [graph][runtime.channel] create channel=91 edge=91 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-10-19 16:26:44.457] [info] [graph][runtime.channel] create channel=92 edge=92 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:26:44.458] [info] [graph][runtime.channel] create channel=93 edge=93 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:26:44.458] [info] [graph][runtime.channel] create channel=94 edge=94 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:26:44.458] [info] [graph][runtime.channel] create channel=95 edge=95 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:26:44.458] [info] [graph][runtime.channel] create channel=96 edge=96 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:26:44.458] [info] [graph][runtime.channel] create channel=97 edge=97 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:26:44.458] [info] [graph][runtime.channel] create channel=98 edge=98 stream=Audio edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.458] [info] [graph][runtime.channel] create channel=99 edge=99 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:26:44.458] [info] [graph][runtime.channel] create channel=100 edge=100 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.458] [info] [graph][runtime.channel] create channel=101 edge=101 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0
[2026-10-19 16:26:44.458] [info] [graph][runtime.channel] create channel=102 edge=102 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:26:44.458] [info] [graph][runtime.channel] create channel=103 edge=103 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:26:44.458] [info] [graph][runtime.channel] create channel=104 edge=104 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:26:44.458] [info] [graph][runtime.channel] create channel=105 edge=105 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0
[2026-10-19 16:26:44.458] [info] [graph][runtime.channel] create channel=106 edge=106 stream=Video edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-19 16:26:44.459] [info] [graph][runtime.channel] create channel=107 edge=107 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-19 16:26:44.459] [info] [graph][runtime.channel] create channel=108 edge=108 stream=Any edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-19 16:26:44.459] [info] [graph][runtime.channel] create channel=109 edge=109 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.459] [info] [graph][runtime.channel] create channel=110 edge=110 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.459] [info] [graph][runtime.channel] create channel=111 edge=111 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-19 16:26:44.459] [info] [graph][runtime.channel] create channel=112 edge=112 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:26:44.459] [info] [graph][runtime.channel] create channel=113 edge=113 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:26:44.459] [info] [graph][runtime.channel] create channel=114 edge=114 stream=Any edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-19 16:26:44.459] [info] [graph][runtime.channel] create channel=115 edge=115 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:26:44.459] [info] [graph][runtime.channel] create channel=116 edge=116 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-19 16:26:44.460] [info] [graph][runtime.channel] create channel=117 edge=117 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-19 16:26:44.460] [info] [graph][runtime.channel] create channel=118 edge=118 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-19 16:26:44.460] [info] [graph][runtime.channel] create channel=119 edge=119 stream=Any edge_kind=EncodedPacket payload=Unknown queued=0
[2026-10-19 16:26:44.460] [info] [graph][runtime.channel] create channel=120 edge=120 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-19 16:26:44.460] [info] [graph][runtime.channel] create channel=121 edge=121 stream=Metadata edge_kind=Unknown payload=Unknown queued=0
[2026-10-19 16:26:44.460] [info] [graph][runtime.channel] create channel=122 edge=122 stream=Metadata edge_kind=Unknown payload=Unknown queued=0
[2026-10-19 16:26:44.460] [info] [graph][runtime.channel] create channel=123 edge=123 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-19 16:26:44.460] [info] [graph][runtime.lifecycle] compiled nodes=82 edges=123 channels=123 execution_order=1->3->8->27->46->47->9->21->28->40->63->48->69->10->29->49->11->12->30->31->50->51->52->13->32->53->14->17->33->36->54->55->59->15->34->56->57->16->35->58->18->37->60->19->38->61->20->39->62->22->41->64->70->23->42->65->71->24->43->66->72->26->45->68->73->25->44->67->6->2->7->81->78->4->77->5->74->75->79->76->80->82
[2026-10-19 16:26:44.460] [info] [graph][runtime.lifecycle] compile.done state=DefaultRegistrationPending
[2026-10-19 16:26:44.461] [info] [graph][runtime.node] register node=1 name=composition-r271.output.video.codec_resolver kind=CodecResolver
[2026-10-19 16:26:44.461] [info] [graph][runtime.node] register node=2 name=composition-r271.output.video.encode kind=VideoEncode
[2026-10-19 16:26:44.461] [info] [graph][runtime.node] register node=3 name=composition-r271.output.audio.codec_resolver kind=AudioCodecResolver
[2026-10-19 16:26:44.461] [info] [graph][runtime.node] register node=4 name=composition-r271.output.audio.encode kind=AudioEncode
[2026-10-19 16:26:44.461] [info] [graph][runtime.node] register node=5 name=composition-r271.output.audio.canonicalizer kind=EncodedAudioCanonicalizer
[2026-10-19 16:26:44.461] [info] [graph][runtime.node] register node=8 name=composition-r271.source_0.input kind=RawRtpInput
[2026-10-19 16:26:44.461] [info] [graph][runtime.node] register node=9 name=composition-r271.source_0.rtp.clock_group kind=RtpClockGroup
[2026-10-19 16:26:44.461] [info] [graph][runtime.node] register node=10 name=composition-r271.source_0.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-10-19 16:26:44.461] [info] [graph][runtime.node] register node=11 name=composition-r271.source_0.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-10-19 16:26:44.461] [info] [graph][runtime.node] register node=12 name=composition-r271.source_0.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-10-19 16:26:44.461] [info] [graph][runtime.node] register node=13 name=composition-r271.source_0.av_sync.source_clock kind=SourceClockStateFanout
[2026-10-19 16:26:44.462] [info] [graph][runtime.node] register node=14 name=composition-r271.source_0.av_sync.video.generation_gate kind=LockedPacketGate
[2026-10-19 16:26:44.462] [info] [graph][runtime.node] register node=15 name=composition-r271.source_0.av_sync.video.canonical_input kind=CanonicalInput
[2026-10-19 16:26:44.462] [info] [graph][runtime.node] register node=16 name=composition-r271.source_0.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-10-19 16:26:44.462] [info] [graph][runtime.node] register node=17 name=composition-r271.source_0.av_sync.startup.clock kind=AvStartupClock
[2026-10-19 16:26:44.462] [info] [graph][runtime.node] register node=18 name=composition-r271.source_0.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-10-19 16:26:44.462] [info] [graph][runtime.node] register node=20 name=composition-r271.source_0.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-10-19 16:26:44.462] [info] [graph][runtime.node] register node=21 name=composition-r271.source_0.video.codec_resolver kind=CodecResolver
[2026-10-19 16:26:44.462] [info] [graph][runtime.node] register node=22 name=composition-r271.source_0.video.decode kind=VideoDecode
[2026-10-19 16:26:44.462] [info] [graph][runtime.node] register node=23 name=composition-r271.source_0.video.hwtransfer kind=HardwareTransfer
[2026-10-19 16:26:44.462] [info] [graph][runtime.node] register node=24 name=composition-r271.source_0.video.framerate kind=VideoFrameRate
[2026-10-19 16:26:44.462] [info] [graph][runtime.node] register node=25 name=composition-r271.source_0.video.preparation kind=VideoPreparation
[2026-10-19 16:26:44.462] [info] [graph][runtime.node] register node=26 name=composition-r271.source_0.video.filter kind=VideoFilter
[2026-10-19 16:26:44.463] [info] [graph][runtime.node] register node=27 name=composition-r271.source_1.input kind=RawRtpInput
[2026-10-19 16:26:44.463] [info] [graph][runtime.node] register node=28 name=composition-r271.source_1.rtp.clock_group kind=RtpClockGroup
[2026-10-19 16:26:44.463] [info] [graph][runtime.node] register node=29 name=composition-r271.source_1.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-10-19 16:26:44.463] [info] [graph][runtime.node] register node=30 name=composition-r271.source_1.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-10-19 16:26:44.463] [info] [graph][runtime.node] register node=31 name=composition-r271.source_1.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-10-19 16:26:44.463] [info] [graph][runtime.node] register node=32 name=composition-r271.source_1.av_sync.source_clock kind=SourceClockStateFanout
[2026-10-19 16:26:44.463] [info] [graph][runtime.node] register node=33 name=composition-r271.source_1.av_sync.video.generation_gate kind=LockedPacketGate
[2026-10-19 16:26:44.463] [info] [graph][runtime.node] register node=34 name=composition-r271.source_1.av_sync.video.canonical_input kind=CanonicalInput
[2026-10-19 16:26:44.463] [info] [graph][runtime.node] register node=35 name=composition-r271.source_1.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-10-19 16:26:44.463] [info] [graph][runtime.node] register node=36 name=composition-r271.source_1.av_sync.startup.clock kind=AvStartupClock
[2026-10-19 16:26:44.463] [info] [graph][runtime.node] register node=37 name=composition-r271.source_1.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-10-19 16:26:44.463] [info] [graph][runtime.node] register node=39 name=composition-r271.source_1.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-10-19 16:26:44.463] [info] [graph][runtime.node] register node=40 name=composition-r271.source_1.video.codec_resolver kind=CodecResolver
[2026-10-19 16:26:44.464] [info] [graph][runtime.node] register node=41 name=composition-r271.source_1.video.decode kind=VideoDecode
[2026-10-19 16:26:44.464] [info] [graph][runtime.node] register node=42 name=composition-r271.source_1.video.hwtransfer kind=HardwareTransfer
[2026-10-19 16:26:44.464] [info] [graph][runtime.node] register node=43 name=composition-r271.source_1.video.framerate kind=VideoFrameRate
[2026-10-19 16:26:44.464] [info] [graph][runtime.node] register node=44 name=composition-r271.source_1.video.preparation kind=VideoPreparation
[2026-10-19 16:26:44.464] [info] [graph][runtime.node] register node=45 name=composition-r271.source_1.video.filter kind=VideoFilter
[2026-10-19 16:26:44.464] [info] [graph][runtime.node] register node=46 name=composition-r271.source_2.video.input kind=RawRtpInput
[2026-10-19 16:26:44.464] [info] [graph][runtime.node] register node=47 name=composition-r271.source_2.audio.input kind=RawRtpInput
[2026-10-19 16:26:44.464] [info] [graph][runtime.node] register node=48 name=composition-r271.source_2.rtp.clock_group kind=RtpClockGroup
[2026-10-19 16:26:44.464] [info] [graph][runtime.node] register node=49 name=composition-r271.source_2.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-10-19 16:26:44.464] [info] [graph][runtime.node] register node=50 name=composition-r271.source_2.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-10-19 16:26:44.464] [info] [graph][runtime.node] register node=51 name=composition-r271.source_2.av_sync.audio.protocol_binder kind=RtpPacketClockBinder
[2026-10-19 16:26:44.464] [info] [graph][runtime.node] register node=52 name=composition-r271.source_2.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-10-19 16:26:44.464] [info] [graph][runtime.node] register node=53 name=composition-r271.source_2.av_sync.source_clock kind=SourceClockStateFanout
[2026-10-19 16:26:44.465] [info] [graph][runtime.node] register node=54 name=composition-r271.source_2.av_sync.video.generation_gate kind=LockedPacketGate
[2026-10-19 16:26:44.465] [info] [graph][runtime.node] register node=55 name=composition-r271.source_2.av_sync.audio.generation_gate kind=LockedPacketGate
[2026-10-19 16:26:44.465] [info] [graph][runtime.node] register node=56 name=composition-r271.source_2.av_sync.video.canonical_input kind=CanonicalInput
[2026-10-19 16:26:44.465] [info] [graph][runtime.node] register node=57 name=composition-r271.source_2.av_sync.audio.canonical_input kind=CanonicalInput
[2026-10-19 16:26:44.465] [info] [graph][runtime.node] register node=58 name=composition-r271.source_2.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-10-19 16:26:44.465] [info] [graph][runtime.node] register node=59 name=composition-r271.source_2.av_sync.startup.clock kind=AvStartupClock
[2026-10-19 16:26:44.465] [info] [graph][runtime.node] register node=60 name=composition-r271.source_2.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-10-19 16:26:44.465] [info] [graph][runtime.node] register node=62 name=composition-r271.source_2.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-10-19 16:26:44.465] [info] [graph][runtime.node] register node=63 name=composition-r271.source_2.video.codec_resolver kind=CodecResolver
[2026-10-19 16:26:44.465] [info] [graph][runtime.node] register node=64 name=composition-r271.source_2.video.decode kind=VideoDecode
[2026-10-19 16:26:44.465] [info] [graph][runtime.node] register node=65 name=composition-r271.source_2.video.hwtransfer kind=HardwareTransfer
[2026-10-19 16:26:44.465] [info] [graph][runtime.node] register node=66 name=composition-r271.source_2.video.framerate kind=VideoFrameRate
[2026-10-19 16:26:44.465] [info] [graph][runtime.node] register node=67 name=composition-r271.source_2.video.preparation kind=VideoPreparation
[2026-10-19 16:26:44.465] [info] [graph][runtime.node] register node=68 name=composition-r271.source_2.video.filter kind=VideoFilter
[2026-10-19 16:26:44.465] [info] [graph][runtime.node] register node=69 name=composition-r271.source_2.audio.encode.codec_resolver kind=AudioCodecResolver
[2026-10-19 16:26:44.465] [info] [graph][runtime.node] register node=70 name=composition-r271.source_2.audio.encode.decode kind=AudioDecode
[2026-10-19 16:26:44.466] [info] [graph][runtime.node] register node=71 name=composition-r271.source_2.audio.encode.startup_trim kind=Unknown
[2026-10-19 16:26:44.466] [info] [graph][runtime.node] register node=72 name=composition-r271.source_2.audio.encode.drift_controller kind=AudioDriftController
[2026-10-19 16:26:44.466] [info] [graph][runtime.node] register node=73 name=composition-r271.source_2.audio.encode.resample kind=AudioResample
[2026-10-19 16:26:44.466] [info] [graph][runtime.node] register node=74 name=composition-r271.output.scheduler.scheduler kind=AvOutputScheduler
[2026-10-19 16:26:44.466] [info] [graph][runtime.node] register node=75 name=composition-r271.output.scheduler.router kind=ScheduledOutputRouter
[2026-10-19 16:26:44.466] [info] [graph][runtime.node] register node=76 name=composition-r271.output.mpegts.mux kind=FileMux
[2026-10-19 16:26:44.466] [info] [graph][runtime.node] register node=78 name=composition-r271.output.mpegts.plan.source kind=Unknown
[2026-10-19 16:26:44.467] [info] [graph][runtime.node] register node=79 name=composition-r271.output.mpegts.scheduled.adapter kind=Unknown
[2026-10-19 16:26:44.467] [info] [graph][runtime.node] register node=80 name=composition-r271.output.mpegts.datagram.materializer kind=MpegTsDatagramMaterializer
[2026-10-19 16:26:44.467] [info] [graph][runtime.node] register node=81 name=composition-r271.output.mpegts.transport.plan kind=DatagramTransportPlanSource
[2026-10-19 16:26:44.467] [info] [graph][runtime.node] register node=82 name=composition-r271.output.mpegts.transport.sender kind=ScheduledDatagramSender
[2026-10-19 16:26:44.467] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=waiting_for_random_access detail="" description="D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r271-output.sdp"
[2026-10-19 16:26:44.467] [info] [graph][runtime.lifecycle] start.begin mode=threaded
[2026-10-19 16:26:44.469] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:26:44.470] [info] [graph][runtime.lifecycle] worker.finished node=3
[2026-10-19 16:26:44.471] [info] [graph][runtime.lifecycle] start.done state=ThreadedRunning
[2026-10-19 16:26:44.471] [info] [graph][runtime.node] worker.thread_cpu node=3 available=1 cpu_ns=0 process_calls=1
[2026-10-19 16:26:44.471] [info] [graph][runtime.lifecycle] worker.finished node=40
[2026-10-19 16:26:44.475] [info] [graph][runtime.node] worker.thread_cpu node=40 available=1 cpu_ns=0 process_calls=1
[2026-10-19 16:26:44.475] [info] [graph][runtime.lifecycle] worker.finished node=69
[2026-10-19 16:26:44.475] [info] [graph][runtime.lifecycle] worker.finished node=21
[2026-10-19 16:26:44.476] [info] [graph][runtime.node] worker.thread_cpu node=21 available=1 cpu_ns=0 process_calls=1
[2026-10-19 16:26:44.476] [info] [graph][runtime.node] worker.thread_cpu node=69 available=1 cpu_ns=0 process_calls=2
[2026-10-19 16:26:44.471] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:26:44.477] [info] [graph][runtime.lifecycle] worker.finished node=1
[2026-10-19 16:26:44.477] [info] [graph][runtime.node] worker.thread_cpu node=1 available=1 cpu_ns=0 process_calls=1
[2026-10-19 16:26:44.477] [info] [graph][runtime.node] video_encode.bind_encoder codec=hevc_nvenc pix_fmt=cuda rate_control=cbr bitrate=8000000 min_bitrate=8000000 max_bitrate=8000000 buffer_size=267000 fps=30/1 gop=60 frame_kind=hardware hwaccel=cuda hw_device_ctx=set hw_frames_ctx=set
[2026-10-19 16:26:44.477] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-10-19 16:26:44.478] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-10-19 16:26:44.478] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=15 generation=1
[2026-10-19 16:26:44.478] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=15 generation=1
[2026-10-19 16:26:44.481] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=800797229 dts=800797229 size=317016 generation=1 sequence=15
[2026-10-19 16:26:44.481] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:26:44.483] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-10-19 16:26:44.483] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=15 generation=1
[2026-10-19 16:26:44.487] [info] [graph][runtime.lifecycle] worker.finished node=63
[2026-10-19 16:26:44.487] [info] [graph][runtime.node] worker.thread_cpu node=63 available=1 cpu_ns=0 process_calls=2
[2026-10-19 16:26:44.487] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=1498867428 dts=1498867428 size=317016 generation=1 sequence=15
[2026-10-19 16:26:44.488] [info] [graph][runtime.node] audio_encode_trace stage=codec_bound
[2026-10-19 16:26:44.490] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=3627058180 dts=3627058180 size=317016 generation=1 sequence=15
[2026-10-19 16:26:44.506] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-10-19 16:26:44.506] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=800797229 format=cuda size=1280x720 buffer_identity=0xb08000000 drm_prime=0 software_frame=0
[2026-10-19 16:26:44.506] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=000001CF522DA970 stream=Video payload=Frame pts=800797229 dts=800797229 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:26:44.507] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=000001CF522DA970 stream=Video payload=Frame pts=800797229 dts=800797229 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:26:44.507] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=000001CF522DA970 stream=Video payload=Frame pts=800797229 dts=800797229 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:26:44.507] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-10-19 16:26:44.507] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=000001CF522D9DA0 stream=Video payload=Frame pts=800797229 dts=800797229 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:26:44.507] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=000001CF522D9DA0 stream=Video payload=Frame pts=800797229 dts=800797229 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:26:44.508] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-10-19 16:26:44.517] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-10-19 16:26:44.518] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=1498867428 format=cuda size=1280x720 buffer_identity=0xb0b000000 drm_prime=0 software_frame=0
[2026-10-19 16:26:44.518] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=000001CF5C916780 stream=Video payload=Frame pts=1498867428 dts=1498867428 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:26:44.518] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=000001CF5C916780 stream=Video payload=Frame pts=1498867428 dts=1498867428 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:26:44.518] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=000001CF5C916780 stream=Video payload=Frame pts=1498867428 dts=1498867428 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:26:44.519] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-10-19 16:26:44.519] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=000001CF5C917740 stream=Video payload=Frame pts=1498867428 dts=1498867428 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:26:44.519] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=000001CF5C917740 stream=Video payload=Frame pts=1498867428 dts=1498867428 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:26:44.519] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-10-19 16:26:44.519] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-10-19 16:26:44.521] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=3627058180 format=cuda size=1280x720 buffer_identity=0xb0b400000 drm_prime=0 software_frame=0
[2026-10-19 16:26:44.521] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=000001CF5C916390 stream=Video payload=Frame pts=3627058180 dts=3627058180 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:26:44.521] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=000001CF5C916390 stream=Video payload=Frame pts=3627058180 dts=3627058180 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:26:44.521] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=000001CF5C916390 stream=Video payload=Frame pts=3627058180 dts=3627058180 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:26:44.522] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-10-19 16:26:44.522] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=000001CF5C915BB0 stream=Video payload=Frame pts=3627058180 dts=3627058180 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:26:44.522] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-10-19 16:26:44.522] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=000001CF5C915BB0 stream=Video payload=Frame pts=3627058180 dts=3627058180 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:26:44.545] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=37
[2026-10-19 16:26:44.571] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=48
[2026-10-19 16:26:44.602] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=81
[2026-10-19 16:26:44.603] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb08c00000 drm_prime=0 software_frame=0
[2026-10-19 16:26:44.603] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb08d20000 drm_prime=0 software_frame=0
[2026-10-19 16:26:44.603] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=15 audio_count=0 first_video_sequence=15
[2026-10-19 16:26:44.603] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=15 audio_count=0 first_video_sequence=15
[2026-10-19 16:26:44.605] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=0
[2026-10-19 16:26:44.605] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=0
[2026-10-19 16:26:44.605] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb08c90000 drm_prime=0 software_frame=0
[2026-10-19 16:26:44.605] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=44 audio_count=23 first_video_sequence=15
[2026-10-19 16:26:44.606] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=23
[2026-10-19 16:26:44.606] [info] [graph][runtime.node] audio_decode_trace stage=first_packet
[2026-10-19 16:26:44.606] [info] [graph][runtime.node] audio_decode_trace stage=first_frame samples=1024 rate=44100
[2026-10-19 16:26:44.607] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1 source_master_ns=5694829986 raw_phase_ns=11339 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-10-19 16:26:44.607] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3 source_master_ns=5741269895 raw_phase_ns=11339 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=45346
[2026-10-19 16:26:44.608] [info] [graph][runtime.node] audio_encode_trace stage=first_frame
[2026-10-19 16:26:44.608] [info] [graph][runtime.node] mpegts_emission_plan generation=1 mode=canonical_access_unit_window access_unit_window_ns=100000000 video_initial_service_window_ns=13995843 audio_initial_service_window_ns=13995843 packet_size_bytes=188 maximum_payload_bytes=1316 per_datagram_overhead_bytes=12
[2026-10-19 16:26:44.612] [info] [graph][runtime.node] video_encode.input_contract format=cuda size=1280x720 buffer_identity=0xb0a000000 drm_prime=0 software_frame=0
[2026-10-19 16:26:44.613] [info] [graph][runtime.node] video_encode.generation_start_key_frame pts=0 generation=1
[2026-10-19 16:26:44.613] [info] [graph][runtime.node] video_encode.first_frame pts=0 generation=attached
[2026-10-19 16:26:44.620] [info] [graph][runtime.node] video_encode.first_submit result=accepted
[2026-10-19 16:26:44.620] [info] [graph][runtime.node] video_encode.first_packet pts=0 dts=0 duration=0 size=108357 side_data_elems=1 received_footprint=108365 side_data[0]=Quality stats:8
[2026-10-19 16:26:44.620] [info] [graph][runtime.node] video_encode.first_packet_footprint bytes=108365 wrapped_size=108357 wrapped_side_data_elems=1
[2026-10-19 16:26:44.620] [info] [graph][runtime.node] av_scheduler_trace stage=first_video_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=5694023068 dispatch_master_ns=5694023068 now_ns=5540653800 node=74 identity_kind=output identity="composition-r271.output" canonical_sequence=1 canonical_created_steady_ns=27818537828500 head_observed_steady_ns=27818537880900 codec_receive_begin_steady_ns=27818537511600 codec_receive_end_steady_ns=27818537512300
[2026-10-19 16:26:44.621] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=5540868800 present_generation=1 present_canonical_target_ns=0 present_canonical_dispatch_ns=0 present_target_master_ns=5694023068 present_dispatch_master_ns=5694023068
[2026-10-19 16:26:44.656] [info] [graph][runtime.node] audio_encode_trace stage=first_packet
[2026-10-19 16:26:44.657] [info] [graph][runtime.node] av_scheduler_trace stage=first_audio_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=5694023068 dispatch_master_ns=5694023068 now_ns=5576827100 node=74 identity_kind=output identity="composition-r271.output" canonical_sequence=1 canonical_created_steady_ns=27818574031000 head_observed_steady_ns=27818574054200 codec_receive_begin_steady_ns=unavailable codec_receive_end_steady_ns=unavailable
[2026-10-19 16:26:44.828] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=running detail="" description="D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r271-output.sdp"
[2026-10-19 16:26:45.235] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=48 source_master_ns=6786167854 raw_phase_ns=11338 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=90692
[2026-10-19 16:26:46.257] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=92 source_master_ns=7807845859 raw_phase_ns=11339 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=136038
[2026-10-19 16:26:47.278] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=136 source_master_ns=8829523864 raw_phase_ns=11339 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=181384
[2026-10-19 16:26:48.324] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=181 source_master_ns=9874421822 raw_phase_ns=11338 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=226730
[2026-10-19 16:26:49.091] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=1097620722 ntp_seconds=4001387209 ntp_fraction=390842023 arrival_ns=27823008615300
[2026-10-19 16:26:49.092] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1097620722
[2026-10-19 16:26:49.091] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=1399786005 ntp_seconds=4001387209 ntp_fraction=390842023 arrival_ns=27823008611000
[2026-10-19 16:26:49.092] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:26:49.092] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=4130002906 ntp_seconds=4001387209 ntp_fraction=395136991 arrival_ns=27823009151000
[2026-10-19 16:26:49.092] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1399786005
[2026-10-19 16:26:49.092] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:26:49.092] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4130002906
[2026-10-19 16:26:49.093] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:26:49.107] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1210775715 ntp_seconds=4001387209 ntp_fraction=459561500 arrival_ns=27823024460800
[2026-10-19 16:26:49.107] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1210775715
[2026-10-19 16:26:49.108] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:26:49.345] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=225 source_master_ns=10896099826 raw_phase_ns=11338 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=272076
[2026-10-19 16:26:50.366] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=269 source_master_ns=11917777831 raw_phase_ns=11338 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=317422
[2026-10-19 16:26:51.388] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=313 source_master_ns=12939455835 raw_phase_ns=11338 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=362768
[2026-10-19 16:26:52.433] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=358 source_master_ns=13984353794 raw_phase_ns=11337 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=408114
[2026-10-19 16:26:53.455] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=402 source_master_ns=15006031799 raw_phase_ns=11338 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=453460
[2026-10-19 16:26:54.103] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=4130002906 ntp_seconds=4001387214 ntp_fraction=433791696 arrival_ns=27828018894200
[2026-10-19 16:26:54.104] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4130002906
[2026-10-19 16:26:54.104] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:26:54.103] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=1097620722 ntp_seconds=4001387214 ntp_fraction=438086664 arrival_ns=27828019561800
[2026-10-19 16:26:54.104] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1097620722
[2026-10-19 16:26:54.105] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:26:54.104] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=1399786005 ntp_seconds=4001387214 ntp_fraction=442381631 arrival_ns=27828020288000
[2026-10-19 16:26:54.105] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1399786005
[2026-10-19 16:26:54.106] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:26:54.131] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1210775715 ntp_seconds=4001387214 ntp_fraction=562640715 arrival_ns=27828048585600
[2026-10-19 16:26:54.131] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1210775715
[2026-10-19 16:26:54.132] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:26:54.478] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=446 source_master_ns=16027709804 raw_phase_ns=11338 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=498806
[2026-10-19 16:26:55.521] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=491 source_master_ns=17072607763 raw_phase_ns=11338 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=544152
[2026-10-19 16:26:56.009] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=16929179600 present_generation=1 present_canonical_target_ns=11354557823 present_canonical_dispatch_ns=11354557823 present_target_master_ns=17048580891 present_dispatch_master_ns=17048580891
[2026-10-19 16:26:56.032] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=16952445900 present_generation=1 present_canonical_target_ns=11366666666 present_canonical_dispatch_ns=11366666666 present_target_master_ns=17060689734 present_dispatch_master_ns=17060689734
[2026-10-19 16:26:56.033] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=16952908300 present_generation=1 present_canonical_target_ns=11377777777 present_canonical_dispatch_ns=11377777777 present_target_master_ns=17071800845 present_dispatch_master_ns=17071800845
[2026-10-19 16:26:56.048] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=16967854100 present_generation=1 present_canonical_target_ns=11400000000 present_canonical_dispatch_ns=11400000000 present_target_master_ns=17094023068 present_dispatch_master_ns=17094023068
[2026-10-19 16:26:56.543] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=535 source_master_ns=18094285767 raw_phase_ns=11338 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=589498
[2026-10-19 16:26:57.564] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=579 source_master_ns=19115963772 raw_phase_ns=11338 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=634844
[2026-10-19 16:26:58.586] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=623 source_master_ns=20137641776 raw_phase_ns=11338 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=680190
[2026-10-19 16:26:59.125] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=1097620722 ntp_seconds=4001387219 ntp_fraction=532575944 arrival_ns=27833041858400
[2026-10-19 16:26:59.125] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1097620722
[2026-10-19 16:26:59.125] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:26:59.126] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=4130002906 ntp_seconds=4001387219 ntp_fraction=541165879 arrival_ns=27833043146000
[2026-10-19 16:26:59.127] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4130002906
[2026-10-19 16:26:59.127] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:26:59.127] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=1399786005 ntp_seconds=4001387219 ntp_fraction=545460846 arrival_ns=27833044283600
[2026-10-19 16:26:59.127] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1399786005
[2026-10-19 16:26:59.127] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:26:59.145] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1210775715 ntp_seconds=4001387219 ntp_fraction=622770257 arrival_ns=27833062624000
[2026-10-19 16:26:59.146] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1210775715
[2026-10-19 16:26:59.146] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:26:59.631] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=668 source_master_ns=21182539736 raw_phase_ns=11338 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=725536
[2026-10-19 16:27:00.653] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=712 source_master_ns=22204217740 raw_phase_ns=11338 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=770882
[2026-10-19 16:27:01.675] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=756 source_master_ns=23225895745 raw_phase_ns=11338 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=816228
[2026-10-19 16:27:02.696] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=800 source_master_ns=24247573749 raw_phase_ns=11338 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=861574
[2026-10-19 16:27:03.741] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=845 source_master_ns=25292471708 raw_phase_ns=11338 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=906920
[2026-10-19 16:27:04.133] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=1399786005 ntp_seconds=4001387224 ntp_fraction=566935683 arrival_ns=27838049923800
[2026-10-19 16:27:04.133] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1399786005
[2026-10-19 16:27:04.133] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:27:04.145] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=1097620722 ntp_seconds=4001387224 ntp_fraction=618475290 arrival_ns=27838062059300
[2026-10-19 16:27:04.145] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1097620722
[2026-10-19 16:27:04.145] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:27:04.160] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1210775715 ntp_seconds=4001387224 ntp_fraction=687194767 arrival_ns=27838077087600
[2026-10-19 16:27:04.160] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1210775715
[2026-10-19 16:27:04.160] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:27:04.174] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=4130002906 ntp_seconds=4001387224 ntp_fraction=747324309 arrival_ns=27838091747200
[2026-10-19 16:27:04.175] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4130002906
[2026-10-19 16:27:04.175] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:27:04.764] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=889 source_master_ns=26314149713 raw_phase_ns=11338 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=952266
[2026-10-19 16:27:05.784] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=933 source_master_ns=27335827717 raw_phase_ns=11337 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=997612
[2026-10-19 16:27:06.829] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=978 source_master_ns=28380725677 raw_phase_ns=11338 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1042958
[2026-10-19 16:27:07.851] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1022 source_master_ns=29402403681 raw_phase_ns=11338 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1088304
[2026-10-19 16:27:08.873] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1066 source_master_ns=30424081686 raw_phase_ns=11338 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1133650
[2026-10-19 16:27:09.155] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=1399786005 ntp_seconds=4001387229 ntp_fraction=665719930 arrival_ns=27843072008400
[2026-10-19 16:27:09.156] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1399786005
[2026-10-19 16:27:09.156] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:27:09.170] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=1097620722 ntp_seconds=4001387229 ntp_fraction=725849473 arrival_ns=27843087018200
[2026-10-19 16:27:09.170] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1097620722
[2026-10-19 16:27:09.170] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:27:09.170] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1210775715 ntp_seconds=4001387229 ntp_fraction=730144440 arrival_ns=27843087021700
[2026-10-19 16:27:09.170] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1210775715
[2026-10-19 16:27:09.171] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:27:09.200] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=4130002906 ntp_seconds=4001387229 ntp_fraction=854698491 arrival_ns=27843117061700
[2026-10-19 16:27:09.200] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4130002906
[2026-10-19 16:27:09.200] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:27:09.900] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1110 source_master_ns=31445759690 raw_phase_ns=11338 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1178996
[2026-10-19 16:27:10.939] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1155 source_master_ns=32490657649 raw_phase_ns=11338 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1224342
[2026-10-19 16:27:11.961] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1199 source_master_ns=33512335654 raw_phase_ns=11338 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1269688
[2026-10-19 16:27:12.982] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1243 source_master_ns=34534013658 raw_phase_ns=11337 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1315034
[2026-10-19 16:27:14.029] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1288 source_master_ns=35578911618 raw_phase_ns=11338 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1360380
[2026-10-19 16:27:14.161] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=1399786005 ntp_seconds=4001387234 ntp_fraction=691489734 arrival_ns=27848078419700
[2026-10-19 16:27:14.162] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1399786005
[2026-10-19 16:27:14.162] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:27:14.192] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1210775715 ntp_seconds=4001387234 ntp_fraction=820338753 arrival_ns=27848108934500
[2026-10-19 16:27:14.192] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1210775715
[2026-10-19 16:27:14.192] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:27:14.192] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=1097620722 ntp_seconds=4001387234 ntp_fraction=820338753 arrival_ns=27848108984200
[2026-10-19 16:27:14.193] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1097620722
[2026-10-19 16:27:14.193] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:27:14.221] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=4130002906 ntp_seconds=4001387234 ntp_fraction=949187772 arrival_ns=27848138525900
[2026-10-19 16:27:14.222] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4130002906
[2026-10-19 16:27:14.222] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:27:15.050] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1332 source_master_ns=36600589622 raw_phase_ns=11338 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1405726
[2026-10-19 16:27:16.071] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1376 source_master_ns=37622267627 raw_phase_ns=11338 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1451072
[2026-10-19 16:27:17.093] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1420 source_master_ns=38643945631 raw_phase_ns=11338 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1496418
[2026-10-19 16:27:18.138] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1465 source_master_ns=39688843590 raw_phase_ns=11337 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1541764
[2026-10-19 16:27:19.159] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1509 source_master_ns=40710521595 raw_phase_ns=11338 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1587110
[2026-10-19 16:27:19.180] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=1399786005 ntp_seconds=4001387239 ntp_fraction=768799145 arrival_ns=27853097014900
[2026-10-19 16:27:19.180] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1399786005
[2026-10-19 16:27:19.180] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:27:19.194] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=1097620722 ntp_seconds=4001387239 ntp_fraction=833223655 arrival_ns=27853111548600
[2026-10-19 16:27:19.194] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1097620722
[2026-10-19 16:27:19.195] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:27:19.195] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1210775715 ntp_seconds=4001387239 ntp_fraction=833223655 arrival_ns=27853111550000
[2026-10-19 16:27:19.195] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1210775715
[2026-10-19 16:27:19.195] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:27:19.224] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=4130002906 ntp_seconds=4001387239 ntp_fraction=962072674 arrival_ns=27853141644700
[2026-10-19 16:27:19.225] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4130002906
[2026-10-19 16:27:19.225] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:27:20.180] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1553 source_master_ns=41732199599 raw_phase_ns=11337 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1632456
[2026-10-19 16:27:21.225] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1598 source_master_ns=42777097559 raw_phase_ns=11338 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1677802
[2026-10-19 16:27:22.247] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1642 source_master_ns=43798775563 raw_phase_ns=11338 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1723148
[2026-10-19 16:27:23.270] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1686 source_master_ns=44820453568 raw_phase_ns=11338 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1768494
[2026-10-19 16:27:24.183] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=1399786005 ntp_seconds=4001387244 ntp_fraction=781684047 arrival_ns=27858099872900
[2026-10-19 16:27:24.183] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1399786005
[2026-10-19 16:27:24.183] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:27:24.208] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1210775715 ntp_seconds=4001387244 ntp_fraction=893353197 arrival_ns=27858125810400
[2026-10-19 16:27:24.209] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1210775715
[2026-10-19 16:27:24.209] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:27:24.239] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=1097620722 ntp_seconds=4001387244 ntp_fraction=1026497183 arrival_ns=27858156213300
[2026-10-19 16:27:24.239] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1097620722
[2026-10-19 16:27:24.240] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:27:24.265] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=4130002906 ntp_seconds=4001387244 ntp_fraction=1133871366 arrival_ns=27858181308300
[2026-10-19 16:27:24.265] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4130002906
[2026-10-19 16:27:24.265] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:27:24.291] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1730 source_master_ns=45842131572 raw_phase_ns=11338 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1813840
[2026-10-19 16:27:25.336] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1775 source_master_ns=46887029531 raw_phase_ns=11337 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1859186
[2026-10-19 16:27:26.357] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1819 source_master_ns=47908707536 raw_phase_ns=11338 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1904532
[2026-10-19 16:27:27.379] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1863 source_master_ns=48930385541 raw_phase_ns=11338 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1949878
[2026-10-19 16:27:28.424] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1908 source_master_ns=49975283500 raw_phase_ns=11338 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1995224
[2026-10-19 16:27:29.190] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=1399786005 ntp_seconds=4001387249 ntp_fraction=811748818 arrival_ns=27863106925100
[2026-10-19 16:27:29.190] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1399786005
[2026-10-19 16:27:29.190] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:27:29.235] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1210775715 ntp_seconds=4001387249 ntp_fraction=1009317314 arrival_ns=27863152585700
[2026-10-19 16:27:29.236] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1210775715
[2026-10-19 16:27:29.236] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:27:29.264] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=1097620722 ntp_seconds=4001387249 ntp_fraction=1133871366 arrival_ns=27863181547600
[2026-10-19 16:27:29.265] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1097620722
[2026-10-19 16:27:29.265] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:27:29.295] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=4130002906 ntp_seconds=4001387249 ntp_fraction=1262720385 arrival_ns=27863211979800
[2026-10-19 16:27:29.295] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4130002906
[2026-10-19 16:27:29.295] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:27:29.446] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1952 source_master_ns=50996961504 raw_phase_ns=11338 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2040570
[2026-10-19 16:27:30.467] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1996 source_master_ns=52018639509 raw_phase_ns=11338 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2085916
[2026-10-19 16:27:31.489] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2040 source_master_ns=53040317513 raw_phase_ns=11338 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2131262
[2026-10-19 16:27:32.533] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2085 source_master_ns=54085215472 raw_phase_ns=11337 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2176608
[2026-10-19 16:27:33.555] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2129 source_master_ns=55106893477 raw_phase_ns=11338 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2221954
[2026-10-19 16:27:34.234] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=1399786005 ntp_seconds=4001387254 ntp_fraction=996432412 arrival_ns=27868149845500
[2026-10-19 16:27:34.234] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1399786005
[2026-10-19 16:27:34.234] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:27:34.246] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1210775715 ntp_seconds=4001387254 ntp_fraction=1056561954 arrival_ns=27868163488100
[2026-10-19 16:27:34.246] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1210775715
[2026-10-19 16:27:34.246] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:27:34.294] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=1097620722 ntp_seconds=4001387254 ntp_fraction=1262720385 arrival_ns=27868211339900
[2026-10-19 16:27:34.295] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1097620722
[2026-10-19 16:27:34.295] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:27:34.323] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=4130002906 ntp_seconds=4001387254 ntp_fraction=1387274436 arrival_ns=27868240079300
[2026-10-19 16:27:34.323] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4130002906
[2026-10-19 16:27:34.323] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:27:34.577] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2173 source_master_ns=56128571482 raw_phase_ns=11338 filtered_phase_ns=11339 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2267300
[2026-10-19 16:27:34.750] [info] [graph][runtime.node] protocol_batch_trace stage=materialization_failed generation=1 attempts=9 consumed_datagrams=0 remaining_datagrams=1 attempted_master_ns=55670368100 mux_dequeued_master_ns=55590465400 first_dequeued_master_ns=55590563600 last_wire_published_master_ns=55590561400 previous_attempt_started_master_ns=55660445900 previous_attempt_completed_master_ns=55660452500 last_attempt_completed_master_ns=55670387900 last_blocked_started_master_ns=55660445900 last_blocked_completed_master_ns=55660452500 blocked_attempts=8 last_blocked_code=WouldBlock last_blocked_native=0 last_blocked_original_message_bytes=292 last_blocked_message_truncated=0 last_blocked_message="wire global sequence reservation exceeds planner backlog capacity serial=1 count=0 bytes=0 request_count=1 request_bytes=228 used_count=17 used_bytes=22864 limit_count=2155 limit_bytes=2920356 generation=1 projected_sequence=35724 head_id=5411 head_first=35665 head_count=59 head_committed=42" remaining_front_release_master_ns=55594024068 remaining_front_deadline_master_ns=55660690736 original_error="InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=55594024068 materialized_ns=55670368100 protocol_deadline_ns=55660690736 deadline_before_release=0 materialized_after_deadline=1"
[2026-10-19 16:27:34.750] [info] [graph][runtime.node] process.failed node=80 name=MediaMpegTsDatagramMaterializerNode kind=MpegTsDatagramMaterializer inputs=[channel=118 edge=118 stream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=120 edge=120 stream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=121 edge=121 stream=Metadata edge_kind=Unknown payload=Unknown queued=9] outputs=[channel=122 edge=122 stream=Metadata edge_kind=Unknown payload=Unknown queued=0] error=InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=55594024068 materialized_ns=55670368100 protocol_deadline_ns=55660690736 deadline_before_release=0 materialized_after_deadline=1
[2026-10-19 16:27:34.750] [info] [graph][runtime.node] worker.failed node=80 kind=MpegTsDatagramMaterializer name=MPEG-TS wire protocol materializer error=InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=55594024068 materialized_ns=55670368100 protocol_deadline_ns=55660690736 deadline_before_release=0 materialized_after_deadline=1
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=82 available=1 cpu_ns=2453125000 process_calls=10614
[2026-10-19 16:27:34.752] [info] [graph][runtime.node] worker.thread_cpu node=80 available=1 cpu_ns=812500000 process_calls=19904
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=32 available=1 cpu_ns=0 process_calls=45
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=55 available=1 cpu_ns=46875000 process_calls=7144
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=20 available=1 cpu_ns=15625000 process_calls=4827
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=27 available=1 cpu_ns=93750000 process_calls=39547
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=23 available=1 cpu_ns=46875000 process_calls=4377
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=46 available=1 cpu_ns=187500000 process_calls=43858
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=47 available=1 cpu_ns=187500000 process_calls=46043
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=6 available=1 cpu_ns=1015625000 process_calls=10504
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=9 available=1 cpu_ns=0 process_calls=33
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=28 available=1 cpu_ns=0 process_calls=34
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=5 available=1 cpu_ns=93750000 process_calls=6478
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=48 available=1 cpu_ns=31250000 process_calls=57
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=10 available=1 cpu_ns=0 process_calls=45
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=29 available=1 cpu_ns=0 process_calls=46
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=49 available=1 cpu_ns=0 process_calls=96
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=12 available=1 cpu_ns=0 process_calls=34
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=11 available=1 cpu_ns=0 process_calls=4837
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=30 available=1 cpu_ns=62500000 process_calls=4845
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=50 available=1 cpu_ns=78125000 process_calls=4856
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=31 available=1 cpu_ns=0 process_calls=34
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=51 available=1 cpu_ns=15625000 process_calls=6941
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=13 available=1 cpu_ns=0 process_calls=45
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=52 available=1 cpu_ns=0 process_calls=65
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=53 available=1 cpu_ns=0 process_calls=96
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=14 available=1 cpu_ns=15625000 process_calls=4968
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=17 available=1 cpu_ns=0 process_calls=33
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=33 available=1 cpu_ns=31250000 process_calls=4956
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=36 available=1 cpu_ns=0 process_calls=34
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=54 available=1 cpu_ns=31250000 process_calls=4988
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=15 available=1 cpu_ns=46875000 process_calls=4981
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=59 available=1 cpu_ns=0 process_calls=64
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=34 available=1 cpu_ns=62500000 process_calls=4987
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=57 available=1 cpu_ns=31250000 process_calls=7112
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=56 available=1 cpu_ns=62500000 process_calls=5009
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=16 available=1 cpu_ns=31250000 process_calls=5000
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=35 available=1 cpu_ns=78125000 process_calls=5009
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=58 available=1 cpu_ns=156250000 process_calls=12114
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=18 available=1 cpu_ns=31250000 process_calls=4908
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=37 available=1 cpu_ns=62500000 process_calls=4910
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=60 available=1 cpu_ns=93750000 process_calls=11848
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=19 available=1 cpu_ns=15625000 process_calls=4891
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=38 available=1 cpu_ns=46875000 process_calls=4889
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=61 available=1 cpu_ns=109375000 process_calls=11640
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=62 available=1 cpu_ns=125000000 process_calls=11644
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=39 available=1 cpu_ns=78125000 process_calls=4834
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=22 available=1 cpu_ns=453125000 process_calls=4196
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=64 available=1 cpu_ns=468750000 process_calls=6070
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=41 available=1 cpu_ns=515625000 process_calls=3883
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=70 available=1 cpu_ns=109375000 process_calls=10422
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=65 available=1 cpu_ns=31250000 process_calls=4379
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=42 available=1 cpu_ns=15625000 process_calls=4359
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=71 available=1 cpu_ns=62500000 process_calls=6953
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=43 available=1 cpu_ns=46875000 process_calls=4286
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=24 available=1 cpu_ns=78125000 process_calls=4380
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=66 available=1 cpu_ns=31250000 process_calls=4360
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=72 available=1 cpu_ns=125000000 process_calls=6791
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=26 available=1 cpu_ns=578125000 process_calls=5766
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=45 available=1 cpu_ns=328125000 process_calls=5478
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=68 available=1 cpu_ns=468750000 process_calls=5584
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=73 available=1 cpu_ns=140625000 process_calls=10854
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=25 available=1 cpu_ns=62500000 process_calls=8429
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=44 available=1 cpu_ns=46875000 process_calls=8198
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=67 available=1 cpu_ns=62500000 process_calls=8213
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=2 available=1 cpu_ns=671875000 process_calls=4516
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=7 available=1 cpu_ns=250000000 process_calls=9040
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=78 available=1 cpu_ns=0 process_calls=7
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=81 available=1 cpu_ns=0 process_calls=5
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=4 available=1 cpu_ns=1171875000 process_calls=6707
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=74 available=1 cpu_ns=140625000 process_calls=11019
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=77 available=1 cpu_ns=0 process_calls=3
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=75 available=1 cpu_ns=93750000 process_calls=10966
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=76 available=1 cpu_ns=406250000 process_calls=14013
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=79 available=1 cpu_ns=93750000 process_calls=10895
[2026-10-19 16:27:34.751] [info] [graph][runtime.node] worker.thread_cpu node=8 available=1 cpu_ns=156250000 process_calls=37198
[2026-10-19 16:27:34.752] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=5413 committed_datagrams=35723 committed_payload_bytes=43325740 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=80004 pacing_reserved=35723 pacing_cancelled=0 pacing_submitted=35723 pacing_maximum_submit_lateness_ns=17484320 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=80004 backlog_max_residence_ns=79861900 last_materialized_sequence=35723 last_scheduled_sequence=35723 last_submitted_sequence=35723 last_committed_sequence=35723 shared_scope_charged_datagrams=35723 shared_scope_charged_wire_bytes=44325984 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2155 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2155 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=379902 endpoint_1_target_effective_socket_bytes=189951 endpoint_1_api_requested_socket_bytes=189951 endpoint_1_effective_socket_bytes=189951 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=35710 endpoint_1_committed_payload_bytes=43324856 endpoint_2_target_effective_socket_bytes=189951 endpoint_2_api_requested_socket_bytes=189951 endpoint_2_effective_socket_bytes=189951 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=13 endpoint_2_committed_payload_bytes=884
[2026-10-19 16:27:34.991] [info] [graph][runtime.lifecycle] node_abort_payload node=82 before_objects=737 after_objects=737 before_bytes=12708164 after_bytes=12708164 before_reservations=62210 after_reservations=62210 before_releases=61473 after_releases=61473
[2026-10-19 16:27:34.991] [info] [graph][runtime.node] mpegts_wire_materializer stage=aborted maximum_materialized_after_release_ns=46714848 worst_materialized_at_ns=31507405100 worst_release_ns=31460690252 worst_deadline_ns=31527356920 worst_global_sequence=18486
[2026-10-19 16:27:34.991] [info] [graph][runtime.lifecycle] node_abort_payload node=80 before_objects=737 after_objects=737 before_bytes=12708164 after_bytes=12708164 before_reservations=62210 after_reservations=62210 before_releases=61473 after_releases=61473
[2026-10-19 16:27:34.991] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 datagrams=0 wire_bytes=0 immediate_deadlines=0 deferred_deadlines=0 planned_wait_ns=0 late_datagrams=0 maximum_lateness_ns=0 pending_bytes=0 peak_pending_bytes=110920 pressure_failures=0 access_units=3661 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=992 maximum_mux_input_after_emit_ns=8288099 maximum_mux_input_after_dispatch_ns=-91711901 worst_mux_input_ready_ns=9735644500 worst_mux_input_emit_ns=9727356401 worst_mux_input_dispatch_ns=9827356401 maximum_protocol_batch_produced_after_release_ns=10139516 worst_protocol_batch_produced_ns=39737496600 worst_protocol_batch_release_ns=39727357084 worst_protocol_batch_deadline_ns=39794023752 worst_protocol_batch_datagrams=1 exit_reason=aborted
[2026-10-19 16:27:34.991] [info] [graph][runtime.lifecycle] node_abort_payload node=76 before_objects=737 after_objects=737 before_bytes=12708164 after_bytes=12708164 before_reservations=62210 after_reservations=62210 before_releases=61473 after_releases=61473
[2026-10-19 16:27:34.991] [info] [graph][runtime.lifecycle] node_abort_payload node=79 before_objects=737 after_objects=737 before_bytes=12708164 after_bytes=12708164 before_reservations=62210 after_reservations=62210 before_releases=61473 after_releases=61473
[2026-10-19 16:27:34.991] [info] [graph][runtime.lifecycle] node_abort_payload node=75 before_objects=737 after_objects=737 before_bytes=12708164 after_bytes=12708164 before_reservations=62210 after_reservations=62210 before_releases=61473 after_releases=61473
[2026-10-19 16:27:34.991] [info] [graph][runtime.node] audio_stage_observation node=74 stage=scheduler_input count=2158 last_generation=1 last_begin=2208768 last_end=2209792 last_rate=44100 last_steady_ns=27868657515300 maximum_inter_event_gap_ns=31912900 gap_begin_generation=1 gap_begin_begin=897024 gap_begin_end=898048 gap_begin_rate=44100 gap_begin_steady_ns=27838912329800 gap_end_generation=1 gap_end_begin=898048 gap_end_end=899072 gap_end_rate=44100 gap_end_steady_ns=27838944242700
[2026-10-19 16:27:34.991] [info] [graph][runtime.node] av_scheduler_correlated_wait generation=1 canonical_sequence=1 missing=audio begin_steady_ns=27818538093300 end_steady_ns=27818574306800 observed_wait_ns=36213500 held_dispatch_master_ns=5694023068 begin_master_ns=5540865900 end_master_ns=5577079000 dispatch_remaining_at_end_ns=116944068 resumed_dispatch_master_ns=5694023068
[2026-10-19 16:27:34.991] [info] [graph][runtime.lifecycle] node_abort_payload node=74 before_objects=737 after_objects=736 before_bytes=12708164 after_bytes=12679543 before_reservations=62210 after_reservations=62210 before_releases=61473 after_releases=61474
[2026-10-19 16:27:34.991] [info] [graph][runtime.node] audio_stage_observation node=5 stage=canonicalizer_input count=2158 last_generation=1 last_begin=2208768 last_end=2209792 last_rate=44100 last_steady_ns=27868657493200 maximum_inter_event_gap_ns=31738700 gap_begin_generation=1 gap_begin_begin=897024 gap_begin_end=898048 gap_begin_rate=44100 gap_begin_steady_ns=27838912304800 gap_end_generation=1 gap_end_begin=898048 gap_end_end=899072 gap_end_rate=44100 gap_end_steady_ns=27838944043500
[2026-10-19 16:27:34.991] [info] [graph][runtime.node] audio_stage_observation node=5 stage=canonicalizer_committed count=2158 last_generation=1 last_begin=2208768 last_end=2209792 last_rate=44100 last_steady_ns=27868657499000 maximum_inter_event_gap_ns=31751800 gap_begin_generation=1 gap_begin_begin=897024 gap_begin_end=898048 gap_begin_rate=44100 gap_begin_steady_ns=27838912311100 gap_end_generation=1 gap_end_begin=898048 gap_end_end=899072 gap_end_rate=44100 gap_end_steady_ns=27838944062900
[2026-10-19 16:27:34.992] [info] [graph][runtime.lifecycle] node_abort_payload node=5 before_objects=736 after_objects=736 before_bytes=12679543 after_bytes=12679543 before_reservations=62210 after_reservations=62210 before_releases=61474 after_releases=61474
[2026-10-19 16:27:34.992] [info] [graph][runtime.lifecycle] node_abort_payload node=77 before_objects=736 after_objects=736 before_bytes=12679543 after_bytes=12679543 before_reservations=62210 after_reservations=62210 before_releases=61474 after_releases=61474
[2026-10-19 16:27:34.992] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_input count=2160 last_generation=1 last_begin=2210816 last_end=2211840 last_rate=44100 last_steady_ns=27868657178500 maximum_inter_event_gap_ns=31137400 gap_begin_generation=1 gap_begin_begin=1507328 gap_begin_end=1508352 gap_begin_rate=44100 gap_begin_steady_ns=27852704585900 gap_end_generation=1 gap_end_begin=1508352 gap_end_end=1509376 gap_end_rate=44100 gap_end_steady_ns=27852735723300
[2026-10-19 16:27:34.992] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_submitted count=2160 last_generation=1 last_begin=2210816 last_end=2211840 last_rate=44100 last_steady_ns=27868657477500 maximum_inter_event_gap_ns=31676700 gap_begin_generation=1 gap_begin_begin=899072 gap_begin_end=900096 gap_begin_rate=44100 gap_begin_steady_ns=27838912288200 gap_end_generation=1 gap_end_begin=900096 gap_end_end=901120 gap_end_rate=44100 gap_end_steady_ns=27838943964900
[2026-10-19 16:27:34.992] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_packet count=2158 last_generation=1 last_begin=2208768 last_end=2209792 last_rate=44100 last_steady_ns=27868657481900 maximum_inter_event_gap_ns=31683000 gap_begin_generation=1 gap_begin_begin=897024 gap_begin_end=898048 gap_begin_rate=44100 gap_begin_steady_ns=27838912293300 gap_end_generation=1 gap_end_begin=898048 gap_end_end=899072 gap_end_rate=44100 gap_end_steady_ns=27838943976300
[2026-10-19 16:27:34.992] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_committed count=2158 last_generation=1 last_begin=2208768 last_end=2209792 last_rate=44100 last_steady_ns=27868657486400 maximum_inter_event_gap_ns=31685200 gap_begin_generation=1 gap_begin_begin=897024 gap_begin_end=898048 gap_begin_rate=44100 gap_begin_steady_ns=27838912298000 gap_end_generation=1 gap_end_begin=898048 gap_end_end=899072 gap_end_rate=44100 gap_end_steady_ns=27838943983200
[2026-10-19 16:27:34.992] [info] [graph][runtime.lifecycle] codec_owner_release node=4 kind=AudioEncode owner_references=4
[2026-10-19 16:27:34.992] [info] [graph][runtime.lifecycle] node_abort_payload node=4 before_objects=736 after_objects=736 before_bytes=12679543 after_bytes=12679543 before_reservations=62210 after_reservations=62210 before_releases=61474 after_releases=61474
[2026-10-19 16:27:34.992] [info] [graph][runtime.lifecycle] node_abort_payload node=78 before_objects=736 after_objects=736 before_bytes=12679543 after_bytes=12679543 before_reservations=62210 after_reservations=62210 before_releases=61474 after_releases=61474
[2026-10-19 16:27:34.992] [info] [graph][runtime.lifecycle] node_abort_payload node=81 before_objects=736 after_objects=736 before_bytes=12679543 after_bytes=12679543 before_reservations=62210 after_reservations=62210 before_releases=61474 after_releases=61474
[2026-10-19 16:27:34.992] [info] [graph][runtime.node] audio_stage_observation node=7 stage=aggregate_built count=2160 last_generation=1 last_begin=2210816 last_end=2211840 last_rate=44100 last_steady_ns=27868657152100 maximum_inter_event_gap_ns=31122300 gap_begin_generation=1 gap_begin_begin=1507328 gap_begin_end=1508352 gap_begin_rate=44100 gap_begin_steady_ns=27852704568200 gap_end_generation=1 gap_end_begin=1508352 gap_end_end=1509376 gap_end_rate=44100 gap_end_steady_ns=27852735690500
[2026-10-19 16:27:34.992] [info] [graph][runtime.node] audio_stage_observation node=7 stage=aggregate_committed count=2160 last_generation=1 last_begin=2210816 last_end=2211840 last_rate=44100 last_steady_ns=27868657508500 maximum_inter_event_gap_ns=31596100 gap_begin_generation=1 gap_begin_begin=1507328 gap_begin_end=1508352 gap_begin_rate=44100 gap_begin_steady_ns=27852704573600 gap_end_generation=1 gap_end_begin=1508352 gap_end_end=1509376 gap_end_rate=44100 gap_end_steady_ns=27852736169700
[2026-10-19 16:27:34.992] [info] [graph][runtime.node] continuous_audio_aggregate.summary reason=abort output_group=composition-r271.output audio_commits=2160 real_samples=2211840 silence_samples=0
[2026-10-19 16:27:34.993] [info] [graph][runtime.lifecycle] node_abort_payload node=7 before_objects=736 after_objects=730 before_bytes=12679543 after_bytes=12630391 before_reservations=62210 after_reservations=62210 before_releases=61474 after_releases=61480
[2026-10-19 16:27:34.993] [info] [graph][runtime.lifecycle] codec_owner_release node=2 kind=VideoEncode owner_references=4
[2026-10-19 16:27:34.993] [info] [graph][runtime.node] video_encoder_execution_observation node=2 count=1505 maximum_call_ns=33671300 maximum_begin_steady_ns=27829915926800 maximum_end_steady_ns=27829949598100 maximum_encoder_pts=341 maximum_native_result=0 maximum_between_submissions_ns=50954000 before_gap_begin_steady_ns=27829860614300 before_gap_end_steady_ns=27829864972800 before_gap_encoder_pts=340 before_gap_native_result=0 after_gap_begin_steady_ns=27829915926800 after_gap_end_steady_ns=27829949598100 after_gap_encoder_pts=341 after_gap_native_result=0
[2026-10-19 16:27:34.993] [info] [graph][runtime.lifecycle] node_abort_payload node=2 before_objects=730 after_objects=730 before_bytes=12630391 after_bytes=12630391 before_reservations=62210 after_reservations=62210 before_releases=61480 after_releases=61480
[2026-10-19 16:27:34.993] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=process count=10504 maximum_ns=23697200 begin_steady_ns=27829892098100 end_steady_ns=27829915795300 video_cursor=341 audio_cursor=not_owned next_video_cursor=341 next_audio_cursor=not_owned error=None return_state=progress deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-10-19 16:27:34.993] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=purge count=10504 maximum_ns=230900 begin_steady_ns=27842293152200 end_steady_ns=27842293383100 video_cursor=714 audio_cursor=not_owned next_video_cursor=714 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 16:27:34.993] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=bind count=10504 maximum_ns=496200 begin_steady_ns=27838564720000 end_steady_ns=27838565216200 video_cursor=602 audio_cursor=not_owned next_video_cursor=602 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 16:27:34.993] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=consume count=10504 maximum_ns=17906300 begin_steady_ns=27838998246500 end_steady_ns=27839016152800 video_cursor=615 audio_cursor=not_owned next_video_cursor=615 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 16:27:34.993] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=activate count=17 maximum_ns=378300 begin_steady_ns=27818524582900 end_steady_ns=27818524961200 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 16:27:34.993] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=build_video count=1505 maximum_ns=23648500 begin_steady_ns=27829892123300 end_steady_ns=27829915771800 video_cursor=341 audio_cursor=not_owned next_video_cursor=341 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 16:27:34.993] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=canvas count=1505 maximum_ns=23612300 begin_steady_ns=27829892139400 end_steady_ns=27829915751700 video_cursor=341 audio_cursor=not_owned next_video_cursor=341 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 16:27:34.993] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=emit count=1505 maximum_ns=6363500 begin_steady_ns=27836063254900 end_steady_ns=27836069618400 video_cursor=526 audio_cursor=not_owned next_video_cursor=526 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 16:27:34.993] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=between_calls_previous_return count=10503 maximum_ns=117828200 begin_steady_ns=27818404117600 end_steady_ns=27818521945800 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=waiting deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-10-19 16:27:34.993] [info] [graph][runtime.node] continuous_aggregate.summary reason=abort output_group=composition-r271.output output_generation=1 video_commits=1505 source[0].group=composition-r271.source.0 source[0].real_frames=1505 source[0].black_frames=0 source[0].contribution_generation=1 source[1].group=composition-r271.source.1 source[1].real_frames=1505 source[1].black_frames=0 source[1].contribution_generation=1 source[2].group=composition-r271.source.2 source[2].real_frames=1505 source[2].black_frames=0 source[2].contribution_generation=1
[2026-10-19 16:27:34.994] [info] [graph][runtime.node] cuda_canvas_call stage=context_push count=1509 operation=copy maximum_ns=287300 begin_steady_ns=27826025372800 end_steady_ns=27826025660100 native_result=0 thread_cpu_ns=0
[2026-10-19 16:27:34.994] [info] [graph][runtime.node] cuda_canvas_call stage=source_wait count=6026 operation=copy maximum_ns=239600 begin_steady_ns=27865592281600 end_steady_ns=27865592521200 native_result=0 thread_cpu_ns=0
[2026-10-19 16:27:34.994] [info] [graph][runtime.node] cuda_canvas_call stage=plane_copy count=12054 operation=copy maximum_ns=3523100 begin_steady_ns=27820658194300 end_steady_ns=27820661717400 native_result=0 thread_cpu_ns=0
[2026-10-19 16:27:34.994] [info] [graph][runtime.node] cuda_canvas_call stage=destination_wait count=2 operation=upload maximum_ns=150600 begin_steady_ns=27818314124100 end_steady_ns=27818314274700 native_result=0 thread_cpu_ns=0
[2026-10-19 16:27:34.994] [info] [graph][runtime.node] cuda_canvas_call stage=context_pop count=1509 operation=copy maximum_ns=82300 begin_steady_ns=27836192919100 end_steady_ns=27836193001400 native_result=0 thread_cpu_ns=0
[2026-10-19 16:27:34.994] [info] [graph][runtime.node] cuda_canvas_call stage=batch_completion count=1507 operation=copy maximum_ns=23411600 begin_steady_ns=27829892323600 end_steady_ns=27829915735200 native_result=0 thread_cpu_ns=0
[2026-10-19 16:27:34.994] [info] [graph][runtime.node] canvas_compose_correlated scope=attempt begin_steady_ns=27829892140200 end_steady_ns=27829915750800 wall_ns=23610600 thread_cpu_ns=0 surface_count=1 surface_total_ns=1400 surface_maximum_ns=1400 surface_maximum_tile=none surface_maximum_result=unavailable validate_count=3 validate_total_ns=3300 validate_maximum_ns=2100 validate_maximum_tile=0 validate_maximum_result=0 black_count=1 black_total_ns=90200 black_maximum_ns=90200 black_maximum_tile=none black_maximum_result=0 tile_count=3 tile_total_ns=77500 tile_maximum_ns=32400 tile_maximum_tile=0 tile_maximum_result=0 publish_count=1 publish_total_ns=7900 publish_maximum_ns=7900 publish_maximum_tile=none publish_maximum_result=0 context_push_count=1 context_push_total_ns=2400 context_push_maximum_ns=2400 context_push_maximum_tile=none context_push_maximum_result=0 source_wait_count=4 source_wait_total_ns=4100 source_wait_maximum_ns=2600 source_wait_maximum_tile=0 source_wait_maximum_result=0 plane_copy_count=8 plane_copy_total_ns=143700 plane_copy_maximum_ns=64100 plane_copy_maximum_tile=none plane_copy_maximum_result=0 destination_wait_count=0 destination_wait_total_ns=0 destination_wait_maximum_ns=0 destination_wait_maximum_tile=none destination_wait_maximum_result=unavailable context_pop_count=1 context_pop_total_ns=4300 context_pop_maximum_ns=4300 context_pop_maximum_tile=none context_pop_maximum_result=0 batch_completion_count=1 batch_completion_total_ns=23408800 batch_completion_maximum_ns=23408800 batch_completion_maximum_tile=none batch_completion_maximum_result=0 source_query_completed_count=0 source_query_ready_count=3 source_query_ready_maximum_ns=6200 source_query_ready_begin_steady_ns=27829892245700 source_query_ready_end_steady_ns=27829892251900 source_query_ready_tile=0 source_query_ready_operation=event_query source_query_ready_native=0 source_query_ready_restore_native=0 source_query_pending_count=0 source_query_unavailable_count=0 source_query_error_count=0
[2026-10-19 16:27:34.994] [info] [graph][runtime.lifecycle] node_abort_payload node=6 before_objects=730 after_objects=693 before_bytes=12630391 after_bytes=12630391 before_reservations=62210 after_reservations=62210 before_releases=61480 after_releases=61517
[2026-10-19 16:27:34.994] [info] [graph][runtime.lifecycle] node_abort_payload node=67 before_objects=693 after_objects=692 before_bytes=12630391 after_bytes=12630391 before_reservations=62210 after_reservations=62210 before_releases=61517 after_releases=61518
[2026-10-19 16:27:34.995] [info] [graph][runtime.lifecycle] node_abort_payload node=44 before_objects=692 after_objects=691 before_bytes=12630391 after_bytes=12630391 before_reservations=62210 after_reservations=62210 before_releases=61518 after_releases=61519
[2026-10-19 16:27:34.995] [info] [graph][runtime.lifecycle] node_abort_payload node=25 before_objects=691 after_objects=690 before_bytes=12630391 after_bytes=12630391 before_reservations=62210 after_reservations=62210 before_releases=61519 after_releases=61520
[2026-10-19 16:27:34.995] [info] [graph][runtime.lifecycle] codec_owner_release node=73 kind=AudioResample owner_references=1
[2026-10-19 16:27:34.995] [info] [graph][runtime.lifecycle] node_abort_payload node=73 before_objects=690 after_objects=688 before_bytes=12630391 after_bytes=12614007 before_reservations=62210 after_reservations=62210 before_releases=61520 after_releases=61522
[2026-10-19 16:27:35.000] [info] [graph][runtime.lifecycle] node_abort_payload node=68 before_objects=688 after_objects=687 before_bytes=12614007 after_bytes=12614007 before_reservations=62210 after_reservations=62210 before_releases=61522 after_releases=61523
[2026-10-19 16:27:35.003] [info] [graph][runtime.lifecycle] node_abort_payload node=45 before_objects=687 after_objects=686 before_bytes=12614007 after_bytes=12614007 before_reservations=62210 after_reservations=62210 before_releases=61523 after_releases=61524
[2026-10-19 16:27:35.008] [info] [graph][runtime.lifecycle] node_abort_payload node=26 before_objects=686 after_objects=685 before_bytes=12614007 after_bytes=12614007 before_reservations=62210 after_reservations=62210 before_releases=61524 after_releases=61525
[2026-10-19 16:27:35.008] [info] [graph][runtime.lifecycle] node_abort_payload node=72 before_objects=685 after_objects=685 before_bytes=12614007 after_bytes=12614007 before_reservations=62210 after_reservations=62210 before_releases=61525 after_releases=61525
[2026-10-19 16:27:35.008] [info] [graph][runtime.lifecycle] node_abort_payload node=66 before_objects=685 after_objects=684 before_bytes=12614007 after_bytes=12614007 before_reservations=62210 after_reservations=62210 before_releases=61525 after_releases=61526
[2026-10-19 16:27:35.008] [info] [graph][runtime.lifecycle] node_abort_payload node=43 before_objects=684 after_objects=683 before_bytes=12614007 after_bytes=12614007 before_reservations=62210 after_reservations=62210 before_releases=61526 after_releases=61527
[2026-10-19 16:27:35.008] [info] [graph][runtime.lifecycle] node_abort_payload node=24 before_objects=683 after_objects=682 before_bytes=12614007 after_bytes=12614007 before_reservations=62210 after_reservations=62210 before_releases=61527 after_releases=61528
[2026-10-19 16:27:35.008] [info] [graph][runtime.lifecycle] node_abort_payload node=71 before_objects=682 after_objects=681 before_bytes=12614007 after_bytes=12605815 before_reservations=62210 after_reservations=62210 before_releases=61528 after_releases=61529
[2026-10-19 16:27:35.009] [info] [graph][runtime.lifecycle] node_abort_payload node=65 before_objects=681 after_objects=680 before_bytes=12605815 after_bytes=12605815 before_reservations=62210 after_reservations=62210 before_releases=61529 after_releases=61530
[2026-10-19 16:27:35.009] [info] [graph][runtime.lifecycle] node_abort_payload node=42 before_objects=680 after_objects=679 before_bytes=12605815 after_bytes=12605815 before_reservations=62210 after_reservations=62210 before_releases=61530 after_releases=61531
[2026-10-19 16:27:35.009] [info] [graph][runtime.lifecycle] node_abort_payload node=23 before_objects=679 after_objects=678 before_bytes=12605815 after_bytes=12605815 before_reservations=62210 after_reservations=62210 before_releases=61531 after_releases=61532
[2026-10-19 16:27:35.009] [info] [graph][runtime.lifecycle] codec_owner_release node=70 kind=AudioDecode owner_references=2
[2026-10-19 16:27:35.009] [info] [graph][runtime.lifecycle] node_abort_payload node=70 before_objects=678 after_objects=677 before_bytes=12605815 after_bytes=12597623 before_reservations=62210 after_reservations=62210 before_releases=61532 after_releases=61533
[2026-10-19 16:27:35.010] [info] [graph][runtime.lifecycle] codec_owner_release node=64 kind=VideoDecode owner_references=2
[2026-10-19 16:27:35.018] [info] [graph][runtime.lifecycle] node_abort_payload node=64 before_objects=677 after_objects=676 before_bytes=12597623 after_bytes=12597623 before_reservations=62210 after_reservations=62210 before_releases=61533 after_releases=61534
[2026-10-19 16:27:35.018] [info] [graph][runtime.lifecycle] codec_owner_release node=41 kind=VideoDecode owner_references=2
[2026-10-19 16:27:35.024] [info] [graph][runtime.lifecycle] node_abort_payload node=41 before_objects=676 after_objects=675 before_bytes=12597623 after_bytes=12597623 before_reservations=62210 after_reservations=62210 before_releases=61534 after_releases=61535
[2026-10-19 16:27:35.024] [info] [graph][runtime.lifecycle] codec_owner_release node=22 kind=VideoDecode owner_references=2
[2026-10-19 16:27:35.031] [info] [graph][runtime.lifecycle] node_abort_payload node=22 before_objects=675 after_objects=674 before_bytes=12597623 after_bytes=12597623 before_reservations=62210 after_reservations=62210 before_releases=61535 after_releases=61536
[2026-10-19 16:27:35.031] [info] [graph][runtime.lifecycle] node_abort_payload node=62 before_objects=674 after_objects=674 before_bytes=12597623 after_bytes=12597623 before_reservations=62210 after_reservations=62210 before_releases=61536 after_releases=61536
[2026-10-19 16:27:35.031] [info] [graph][runtime.lifecycle] node_abort_payload node=39 before_objects=674 after_objects=674 before_bytes=12597623 after_bytes=12597623 before_reservations=62210 after_reservations=62210 before_releases=61536 after_releases=61536
[2026-10-19 16:27:35.031] [info] [graph][runtime.lifecycle] node_abort_payload node=20 before_objects=674 after_objects=674 before_bytes=12597623 after_bytes=12597623 before_reservations=62210 after_reservations=62210 before_releases=61536 after_releases=61536
[2026-10-19 16:27:35.031] [info] [graph][runtime.lifecycle] node_abort_payload node=61 before_objects=674 after_objects=674 before_bytes=12597623 after_bytes=12597623 before_reservations=62210 after_reservations=62210 before_releases=61536 after_releases=61536
[2026-10-19 16:27:35.031] [info] [graph][runtime.lifecycle] node_abort_payload node=38 before_objects=674 after_objects=674 before_bytes=12597623 after_bytes=12597623 before_reservations=62210 after_reservations=62210 before_releases=61536 after_releases=61536
[2026-10-19 16:27:35.031] [info] [graph][runtime.lifecycle] node_abort_payload node=19 before_objects=674 after_objects=674 before_bytes=12597623 after_bytes=12597623 before_reservations=62210 after_reservations=62210 before_releases=61536 after_releases=61536
[2026-10-19 16:27:35.031] [info] [graph][runtime.lifecycle] node_abort_payload node=60 before_objects=674 after_objects=674 before_bytes=12597623 after_bytes=12597623 before_reservations=62210 after_reservations=62210 before_releases=61536 after_releases=61536
[2026-10-19 16:27:35.031] [info] [graph][runtime.lifecycle] node_abort_payload node=37 before_objects=674 after_objects=674 before_bytes=12597623 after_bytes=12597623 before_reservations=62210 after_reservations=62210 before_releases=61536 after_releases=61536
[2026-10-19 16:27:35.031] [info] [graph][runtime.lifecycle] node_abort_payload node=18 before_objects=674 after_objects=674 before_bytes=12597623 after_bytes=12597623 before_reservations=62210 after_reservations=62210 before_releases=61536 after_releases=61536
[2026-10-19 16:27:35.031] [info] [graph][runtime.lifecycle] node_abort_payload node=58 before_objects=674 after_objects=674 before_bytes=12597623 after_bytes=12597623 before_reservations=62210 after_reservations=62210 before_releases=61536 after_releases=61536
[2026-10-19 16:27:35.031] [info] [graph][runtime.lifecycle] node_abort_payload node=35 before_objects=674 after_objects=674 before_bytes=12597623 after_bytes=12597623 before_reservations=62210 after_reservations=62210 before_releases=61536 after_releases=61536
[2026-10-19 16:27:35.032] [info] [graph][runtime.lifecycle] node_abort_payload node=16 before_objects=674 after_objects=674 before_bytes=12597623 after_bytes=12597623 before_reservations=62210 after_reservations=62210 before_releases=61536 after_releases=61536
[2026-10-19 16:27:35.032] [info] [graph][runtime.lifecycle] node_abort_payload node=57 before_objects=674 after_objects=674 before_bytes=12597623 after_bytes=12597623 before_reservations=62210 after_reservations=62210 before_releases=61536 after_releases=61536
[2026-10-19 16:27:35.032] [info] [graph][runtime.lifecycle] node_abort_payload node=56 before_objects=674 after_objects=674 before_bytes=12597623 after_bytes=12597623 before_reservations=62210 after_reservations=62210 before_releases=61536 after_releases=61536
[2026-10-19 16:27:35.032] [info] [graph][runtime.lifecycle] node_abort_payload node=34 before_objects=674 after_objects=674 before_bytes=12597623 after_bytes=12597623 before_reservations=62210 after_reservations=62210 before_releases=61536 after_releases=61536
[2026-10-19 16:27:35.032] [info] [graph][runtime.lifecycle] node_abort_payload node=15 before_objects=674 after_objects=674 before_bytes=12597623 after_bytes=12597623 before_reservations=62210 after_reservations=62210 before_releases=61536 after_releases=61536
[2026-10-19 16:27:35.032] [info] [graph][runtime.lifecycle] node_abort_payload node=59 before_objects=674 after_objects=674 before_bytes=12597623 after_bytes=12597623 before_reservations=62210 after_reservations=62210 before_releases=61536 after_releases=61536
[2026-10-19 16:27:35.032] [info] [graph][runtime.lifecycle] node_abort_payload node=55 before_objects=674 after_objects=674 before_bytes=12597623 after_bytes=12597623 before_reservations=62210 after_reservations=62210 before_releases=61536 after_releases=61536
[2026-10-19 16:27:35.032] [info] [graph][runtime.lifecycle] node_abort_payload node=54 before_objects=674 after_objects=674 before_bytes=12597623 after_bytes=12597623 before_reservations=62210 after_reservations=62210 before_releases=61536 after_releases=61536
[2026-10-19 16:27:35.032] [info] [graph][runtime.lifecycle] node_abort_payload node=36 before_objects=674 after_objects=674 before_bytes=12597623 after_bytes=12597623 before_reservations=62210 after_reservations=62210 before_releases=61536 after_releases=61536
[2026-10-19 16:27:35.032] [info] [graph][runtime.lifecycle] node_abort_payload node=33 before_objects=674 after_objects=674 before_bytes=12597623 after_bytes=12597623 before_reservations=62210 after_reservations=62210 before_releases=61536 after_releases=61536
[2026-10-19 16:27:35.032] [info] [graph][runtime.lifecycle] node_abort_payload node=17 before_objects=674 after_objects=674 before_bytes=12597623 after_bytes=12597623 before_reservations=62210 after_reservations=62210 before_releases=61536 after_releases=61536
[2026-10-19 16:27:35.032] [info] [graph][runtime.lifecycle] node_abort_payload node=14 before_objects=674 after_objects=674 before_bytes=12597623 after_bytes=12597623 before_reservations=62210 after_reservations=62210 before_releases=61536 after_releases=61536
[2026-10-19 16:27:35.032] [info] [graph][runtime.lifecycle] node_abort_payload node=53 before_objects=674 after_objects=674 before_bytes=12597623 after_bytes=12597623 before_reservations=62210 after_reservations=62210 before_releases=61536 after_releases=61536
[2026-10-19 16:27:35.032] [info] [graph][runtime.lifecycle] node_abort_payload node=32 before_objects=674 after_objects=674 before_bytes=12597623 after_bytes=12597623 before_reservations=62210 after_reservations=62210 before_releases=61536 after_releases=61536
[2026-10-19 16:27:35.032] [info] [graph][runtime.lifecycle] node_abort_payload node=13 before_objects=674 after_objects=674 before_bytes=12597623 after_bytes=12597623 before_reservations=62210 after_reservations=62210 before_releases=61536 after_releases=61536
[2026-10-19 16:27:35.032] [info] [graph][runtime.lifecycle] node_abort_payload node=52 before_objects=674 after_objects=674 before_bytes=12597623 after_bytes=12597623 before_reservations=62210 after_reservations=62210 before_releases=61536 after_releases=61536
[2026-10-19 16:27:35.033] [info] [graph][runtime.lifecycle] node_abort_payload node=51 before_objects=674 after_objects=674 before_bytes=12597623 after_bytes=12597623 before_reservations=62210 after_reservations=62210 before_releases=61536 after_releases=61536
[2026-10-19 16:27:35.033] [info] [graph][runtime.lifecycle] node_abort_payload node=50 before_objects=674 after_objects=673 before_bytes=12597623 after_bytes=12581665 before_reservations=62210 after_reservations=62210 before_releases=61536 after_releases=61537
[2026-10-19 16:27:35.033] [info] [graph][runtime.lifecycle] node_abort_payload node=31 before_objects=673 after_objects=673 before_bytes=12581665 after_bytes=12581665 before_reservations=62210 after_reservations=62210 before_releases=61537 after_releases=61537
[2026-10-19 16:27:35.033] [info] [graph][runtime.lifecycle] node_abort_payload node=30 before_objects=673 after_objects=672 before_bytes=12581665 after_bytes=12565707 before_reservations=62210 after_reservations=62210 before_releases=61537 after_releases=61538
[2026-10-19 16:27:35.033] [info] [graph][runtime.lifecycle] node_abort_payload node=12 before_objects=672 after_objects=672 before_bytes=12565707 after_bytes=12565707 before_reservations=62210 after_reservations=62210 before_releases=61538 after_releases=61538
[2026-10-19 16:27:35.033] [info] [graph][runtime.lifecycle] node_abort_payload node=11 before_objects=672 after_objects=671 before_bytes=12565707 after_bytes=12549749 before_reservations=62210 after_reservations=62210 before_releases=61538 after_releases=61539
[2026-10-19 16:27:35.034] [info] [graph][runtime.lifecycle] node_abort_payload node=49 before_objects=671 after_objects=671 before_bytes=12549749 after_bytes=12549749 before_reservations=62210 after_reservations=62210 before_releases=61539 after_releases=61539
[2026-10-19 16:27:35.034] [info] [graph][runtime.lifecycle] node_abort_payload node=29 before_objects=671 after_objects=671 before_bytes=12549749 after_bytes=12549749 before_reservations=62210 after_reservations=62210 before_releases=61539 after_releases=61539
[2026-10-19 16:27:35.034] [info] [graph][runtime.lifecycle] node_abort_payload node=10 before_objects=671 after_objects=671 before_bytes=12549749 after_bytes=12549749 before_reservations=62210 after_reservations=62210 before_releases=61539 after_releases=61539
[2026-10-19 16:27:35.034] [info] [graph][runtime.lifecycle] node_abort_payload node=69 before_objects=671 after_objects=671 before_bytes=12549749 after_bytes=12549749 before_reservations=62210 after_reservations=62210 before_releases=61539 after_releases=61539
[2026-10-19 16:27:35.034] [info] [graph][runtime.lifecycle] node_abort_payload node=48 before_objects=671 after_objects=671 before_bytes=12549749 after_bytes=12549749 before_reservations=62210 after_reservations=62210 before_releases=61539 after_releases=61539
[2026-10-19 16:27:35.035] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-10-19 16:27:35.035] [info] [graph][runtime.lifecycle] node_abort_payload node=63 before_objects=671 after_objects=671 before_bytes=12549749 after_bytes=12549749 before_reservations=62210 after_reservations=62210 before_releases=61539 after_releases=61539
[2026-10-19 16:27:35.035] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-10-19 16:27:35.035] [info] [graph][runtime.lifecycle] node_abort_payload node=40 before_objects=671 after_objects=671 before_bytes=12549749 after_bytes=12549749 before_reservations=62210 after_reservations=62210 before_releases=61539 after_releases=61539
[2026-10-19 16:27:35.035] [info] [graph][runtime.lifecycle] node_abort_payload node=28 before_objects=671 after_objects=671 before_bytes=12549749 after_bytes=12549749 before_reservations=62210 after_reservations=62210 before_releases=61539 after_releases=61539
[2026-10-19 16:27:35.035] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-10-19 16:27:35.035] [info] [graph][runtime.lifecycle] node_abort_payload node=21 before_objects=671 after_objects=671 before_bytes=12549749 after_bytes=12549749 before_reservations=62210 after_reservations=62210 before_releases=61539 after_releases=61539
[2026-10-19 16:27:35.035] [info] [graph][runtime.lifecycle] node_abort_payload node=9 before_objects=671 after_objects=671 before_bytes=12549749 after_bytes=12549749 before_reservations=62210 after_reservations=62210 before_releases=61539 after_releases=61539
[2026-10-19 16:27:35.035] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=2377 completion_waits=2377 completed_datagrams=2414 drained_datagrams=37 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-19 16:27:35.036] [info] [graph][runtime.lifecycle] node_abort_payload node=47 before_objects=671 after_objects=671 before_bytes=12549749 after_bytes=12549749 before_reservations=62210 after_reservations=62210 before_releases=61539 after_releases=61539
[2026-10-19 16:27:35.036] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=36685 completion_waits=36685 completed_datagrams=48923 drained_datagrams=12238 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-19 16:27:35.037] [info] [graph][runtime.lifecycle] node_abort_payload node=46 before_objects=671 after_objects=671 before_bytes=12549749 after_bytes=12549749 before_reservations=62210 after_reservations=62210 before_releases=61539 after_releases=61539
[2026-10-19 16:27:35.037] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=38260 completion_waits=38260 completed_datagrams=48923 drained_datagrams=10664 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-19 16:27:35.037] [info] [graph][runtime.lifecycle] node_abort_payload node=27 before_objects=671 after_objects=671 before_bytes=12549749 after_bytes=12549749 before_reservations=62210 after_reservations=62210 before_releases=61539 after_releases=61539
[2026-10-19 16:27:35.037] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=37241 completion_waits=37241 completed_datagrams=48931 drained_datagrams=11691 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-19 16:27:35.038] [info] [graph][runtime.lifecycle] node_abort_payload node=8 before_objects=671 after_objects=671 before_bytes=12549749 after_bytes=12549749 before_reservations=62210 after_reservations=62210 before_releases=61539 after_releases=61539
[2026-10-19 16:27:35.038] [info] [graph][runtime.lifecycle] node_abort_payload node=3 before_objects=671 after_objects=671 before_bytes=12549749 after_bytes=12549749 before_reservations=62210 after_reservations=62210 before_releases=61539 after_releases=61539
[2026-10-19 16:27:35.038] [info] [graph][runtime.node] codec_resolver.release slot=encoder_parameters bufferReferences=1 hasCredit=0 creditReferences=0
[2026-10-19 16:27:35.038] [info] [graph][runtime.lifecycle] node_abort_payload node=1 before_objects=671 after_objects=671 before_bytes=12549749 after_bytes=12549749 before_reservations=62210 after_reservations=62210 before_releases=61539 after_releases=61539
[2026-10-19 16:27:35.122] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=failed detail="protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=55594024068 materialized_ns=55670368100 protocol_deadline_ns=55660690736 deadline_before_release=0 materialized_after_deadline=1" description="D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r271-output.sdp" error=1 native=0 stage=3 reason="protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=55594024068 materialized_ns=55670368100 protocol_deadline_ns=55660690736 deadline_before_release=0 materialized_after_deadline=1"
[2026-10-19 16:27:35.122] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=retired detail="protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=55594024068 materialized_ns=55670368100 protocol_deadline_ns=55660690736 deadline_before_release=0 materialized_after_deadline=1" description="D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r271-output.sdp" error=1 native=0 stage=3 reason="protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=55594024068 materialized_ns=55670368100 protocol_deadline_ns=55660690736 deadline_before_release=0 materialized_after_deadline=1"
[CLI] final runtime report: queued=0, peakQueued=917, threads=82, processThreads=133, workers=0, workerProgress=142357, workerProcessCalls=551202, workerWaits=408838, workerWakeups=404935, workerErrors=1, errors=1, stalledIntervals=0, cpuSamples=157, logicalProcessors=22, averageSystemMachineCpuPercent=8.356358, averageProcessMachineCpuPercent=2.134778, peakProcessMachineCpuPercent=5.800000, averageProcessSingleCoreCpuPercent=46.965112, peakProcessSingleCoreCpuPercent=127.600000, initialWorkingSetBytes=207278080, workingSetBytes=265306112, peakWorkingSetBytes=266014720, totalPushed=131173, totalPopped=130513, droppedBuffers=0, encodedPacketsPushed=16804, encodedPacketsPopped=16803, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=17115191, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=745, graphPayloadReservations=62210, graphPayloadReleases=62210, graphPayloadPressureFailures=0, lastInputReceivedAtNs=27868657719800
~~~
### stderr
~~~text
[in @ 000001cf518afd00] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001cf518afd00] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 8897.746989
[in @ 000001cf518b0280] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001cf518b0280] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 40300.646444
[in @ 000001cf518b0380] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001cf518b0380] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 16654.082533
[in @ 000001cf2db47c80] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001cf2db47c80] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 8897.746989
[in @ 000001cf42d99f40] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001cf42d99f40] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 40300.646444
[in @ 000001cf51900f00] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001cf51900f00] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 16654.082533
[aac @ 000001cf56c347c0] Qavg: 639.871
[aac @ 000001cf56c347c0] 2 frames left in the queue on closing
[CLI] realtime video runtime failed: InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=55594024068 materialized_ns=55670368100 protocol_deadline_ns=55660690736 deadline_before_release=0 materialized_after_deadline=1

~~~

三个源均完整120秒/3600帧、退出0；CLI1，ProcDump1（目标正常错误退出，未触发dump），VLC使用RC quit退出。重入未运行。
~~~text
source0
[out#1/rtp @ 000002416c346400] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.49    
source1
[out#0/rtp @ 0000024f57445c40] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
source2
[out#0/rtp @ 0000021fb79f8300] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.47    
~~~
清理清单：
composition-r271-cli-error.log
composition-r271-cli.log
composition-r271-monitor.log
composition-r271-native-2026-10-19-16h27m20s256.png
composition-r271-output.sdp
composition-r271-source0-stdout.log
composition-r271-source0.log
composition-r271-source1-stdout.log
composition-r271-source1.log
composition-r271-source2-stdout.log
composition-r271-source2.log
composition-r271-vlc.log

12个本轮临时文件已按清单删除，六个精确PID无残留，未生成dump；复用源保留。未进行RKMPP测试。
