# r270：原规格 realtime 源事件观测复验

状态：失败，已归档并清理。Release重建58271成功，四项产物齐全；realtime6197760B，2026-10-19 16:10:09，SHA256 337A2F6ED39546E78F614BC8C3DD8BF588D95E328872D2876C638920ACF9F1BE。未运行local或Nsight。

三路H264720p30约8Mbps完整120秒输入，源2附AAC44100双声道；HEVC720p30 8000kbps CBR/AAC192kbps CBR，MPEGTS/RTP输出。会话40134，VLC38080，源23212/23496/41044，CLI33980。16:10:55原生快照已查看，三格正常、右下空格黑。

## 实际启动命令
~~~powershell
$ErrorActionPreference='Stop'
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r270-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r270-native- --snapshot-format=png rtp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r270-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r270-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r270-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r270-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r270-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r270-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r270 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r270-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r270-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r270-cli-error.log' -PassThru
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
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r270-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

## 重入阶段

16:16:15 VLC原生快照确认全黑；CLI继续运行。16:16:21左右启动所选音视频源重入，PID36720，会话39618；16:16:49原生快照确认仅左下格恢复，其他格为黑。完整命令：
~~~powershell
$reentry=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r270-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r270-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($reentry.Id)"
$reentry.WaitForExit()
Write-Output "REENTRY_EXIT=$($reentry.ExitCode)"
exit $reentry.ExitCode
~~~

## 失败结论与退出

CLI退出-1073741819（0xC0000005），非caller_stop、非自然结束。三个原始源与重入源均退出0、3600帧/120秒。重入原生快照在CLI失败后取得，只能证明VLC保留了重入画面，不能证明该时刻链路仍运行；未取得重入结束后全黑画面。本轮FAIL。

直接错误：serial=1且count=0/bytes=0，7次WouldBlock后报文材料化超过绝对截止时间9606684ns（9.606684ms）。最长canvas20.2074ms，同次三个真实源event_query均Ready（native=0），batch_completion20.0916ms；仅限该最长事件，不证明r267根因消失。最后正常采样CPU均值1.436555%、峰值4.988662%，WS249040896、峰值251969536B，payload高水4975437B；没有正常最终报告，禁止声称payload归零或错误计数0通过。generation2 phase约15873ns仅覆盖失败前。sender最大提交迟到37.449266ms，delivery_evidence=not_proven。

### 原始关键运行记录
~~~text
[CLI] composition_inputs=3 grid=fixed audio_source=2 audio=on max_duration=source_driven hw=planner-highest-score
[2026-10-19 16:10:33.852] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-10-19 16:10:33.852] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-10-19 16:10:33.856] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-10-19 16:10:33.995] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-19 16:10:33.996] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-19 16:10:34.038] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-19 16:10:34.038] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-19 16:10:34.079] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-19 16:10:34.079] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-19 16:10:34.151] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-10-19 16:10:34.190] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-10-19 16:10:34.190] [info] [graph][planner.capability] canvas_cuda_stream stage=prepared device_stream=0000000000000000 private_nonblocking=1
[2026-10-19 16:10:34.198] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-19 16:10:34.198] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-19 16:10:34.198] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-10-19 16:10:34.205] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-19 16:10:34.205] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-19 16:10:34.205] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-10-19 16:10:34.212] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-19 16:10:34.213] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-19 16:10:34.213] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-10-19 16:10:34.230] [info] [graph][planner.capability] composition_preparation_storage stage=graph_bound scope=shared_reservations capacity=2147483648 retained=832926849 high_water=850355604
[CLI] audio_plan branch=transcode_frame reason=transcode_required_by_output_frame_contract codec=aac sample_rate=44100 channels=2 access_unit_samples=1024 encoder=aac bitrate_kbps=192
[CLI] prepared_output_id=1
[CLI] selected_chain=fixed-grid-composition score=0 decoder=h264,h264,h264 filter=scale_cuda=640:360,scale_cuda=640:360,scale_cuda=640:360 encoder=hevc_nvenc
[2026-10-19 16:10:34.232] [info] [graph][runtime.lifecycle] compile.begin
[2026-10-19 16:10:34.232] [info] [graph][runtime.channel] create channel=1 edge=1 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:10:34.233] [info] [graph][runtime.channel] create channel=2 edge=2 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:10:34.233] [info] [graph][runtime.channel] create channel=3 edge=3 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-19 16:10:34.233] [info] [graph][runtime.channel] create channel=4 edge=4 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.233] [info] [graph][runtime.channel] create channel=5 edge=5 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.233] [info] [graph][runtime.channel] create channel=6 edge=6 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.233] [info] [graph][runtime.channel] create channel=7 edge=7 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.233] [info] [graph][runtime.channel] create channel=8 edge=8 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.233] [info] [graph][runtime.channel] create channel=9 edge=9 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:10:34.233] [info] [graph][runtime.channel] create channel=10 edge=10 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.233] [info] [graph][runtime.channel] create channel=11 edge=11 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.233] [info] [graph][runtime.channel] create channel=12 edge=12 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:10:34.234] [info] [graph][runtime.channel] create channel=13 edge=13 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:10:34.234] [info] [graph][runtime.channel] create channel=14 edge=14 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.234] [info] [graph][runtime.channel] create channel=15 edge=15 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.234] [info] [graph][runtime.channel] create channel=16 edge=16 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.234] [info] [graph][runtime.channel] create channel=17 edge=17 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.234] [info] [graph][runtime.channel] create channel=18 edge=18 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.234] [info] [graph][runtime.channel] create channel=19 edge=19 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.234] [info] [graph][runtime.channel] create channel=20 edge=20 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.234] [info] [graph][runtime.channel] create channel=21 edge=21 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-10-19 16:10:34.234] [info] [graph][runtime.channel] create channel=22 edge=22 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:10:34.234] [info] [graph][runtime.channel] create channel=23 edge=23 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:10:34.234] [info] [graph][runtime.channel] create channel=24 edge=24 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:10:34.234] [info] [graph][runtime.channel] create channel=25 edge=25 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:10:34.234] [info] [graph][runtime.channel] create channel=26 edge=26 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:10:34.234] [info] [graph][runtime.channel] create channel=27 edge=27 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:10:34.234] [info] [graph][runtime.channel] create channel=28 edge=28 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:10:34.235] [info] [graph][runtime.channel] create channel=29 edge=29 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:10:34.235] [info] [graph][runtime.channel] create channel=30 edge=30 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.235] [info] [graph][runtime.channel] create channel=31 edge=31 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.235] [info] [graph][runtime.channel] create channel=32 edge=32 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.235] [info] [graph][runtime.channel] create channel=33 edge=33 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.235] [info] [graph][runtime.channel] create channel=34 edge=34 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.235] [info] [graph][runtime.channel] create channel=35 edge=35 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.235] [info] [graph][runtime.channel] create channel=36 edge=36 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:10:34.235] [info] [graph][runtime.channel] create channel=37 edge=37 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.235] [info] [graph][runtime.channel] create channel=38 edge=38 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.235] [info] [graph][runtime.channel] create channel=39 edge=39 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:10:34.235] [info] [graph][runtime.channel] create channel=40 edge=40 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:10:34.235] [info] [graph][runtime.channel] create channel=41 edge=41 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.235] [info] [graph][runtime.channel] create channel=42 edge=42 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.235] [info] [graph][runtime.channel] create channel=43 edge=43 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.236] [info] [graph][runtime.channel] create channel=44 edge=44 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.236] [info] [graph][runtime.channel] create channel=45 edge=45 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.236] [info] [graph][runtime.channel] create channel=46 edge=46 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.236] [info] [graph][runtime.channel] create channel=47 edge=47 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.236] [info] [graph][runtime.channel] create channel=48 edge=48 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-10-19 16:10:34.236] [info] [graph][runtime.channel] create channel=49 edge=49 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:10:34.236] [info] [graph][runtime.channel] create channel=50 edge=50 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:10:34.236] [info] [graph][runtime.channel] create channel=51 edge=51 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:10:34.236] [info] [graph][runtime.channel] create channel=52 edge=52 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:10:34.236] [info] [graph][runtime.channel] create channel=53 edge=53 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:10:34.236] [info] [graph][runtime.channel] create channel=54 edge=54 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:10:34.236] [info] [graph][runtime.channel] create channel=55 edge=55 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:10:34.236] [info] [graph][runtime.channel] create channel=56 edge=56 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:10:34.236] [info] [graph][runtime.channel] create channel=57 edge=57 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.236] [info] [graph][runtime.channel] create channel=58 edge=58 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.236] [info] [graph][runtime.channel] create channel=59 edge=59 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.236] [info] [graph][runtime.channel] create channel=60 edge=60 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.237] [info] [graph][runtime.channel] create channel=61 edge=61 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.237] [info] [graph][runtime.channel] create channel=62 edge=62 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.237] [info] [graph][runtime.channel] create channel=63 edge=63 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.237] [info] [graph][runtime.channel] create channel=64 edge=64 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.237] [info] [graph][runtime.channel] create channel=65 edge=65 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.237] [info] [graph][runtime.channel] create channel=66 edge=66 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:10:34.237] [info] [graph][runtime.channel] create channel=67 edge=67 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:10:34.237] [info] [graph][runtime.channel] create channel=68 edge=68 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.237] [info] [graph][runtime.channel] create channel=69 edge=69 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.237] [info] [graph][runtime.channel] create channel=70 edge=70 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.237] [info] [graph][runtime.channel] create channel=71 edge=71 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:10:34.237] [info] [graph][runtime.channel] create channel=72 edge=72 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:10:34.237] [info] [graph][runtime.channel] create channel=73 edge=73 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:10:34.237] [info] [graph][runtime.channel] create channel=74 edge=74 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:10:34.237] [info] [graph][runtime.channel] create channel=75 edge=75 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.238] [info] [graph][runtime.channel] create channel=76 edge=76 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.238] [info] [graph][runtime.channel] create channel=77 edge=77 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.238] [info] [graph][runtime.channel] create channel=78 edge=78 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.238] [info] [graph][runtime.channel] create channel=79 edge=79 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.238] [info] [graph][runtime.channel] create channel=80 edge=80 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.238] [info] [graph][runtime.channel] create channel=81 edge=81 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.238] [info] [graph][runtime.channel] create channel=82 edge=82 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.238] [info] [graph][runtime.channel] create channel=83 edge=83 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-10-19 16:10:34.238] [info] [graph][runtime.channel] create channel=84 edge=84 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:10:34.238] [info] [graph][runtime.channel] create channel=85 edge=85 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:10:34.238] [info] [graph][runtime.channel] create channel=86 edge=86 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:10:34.238] [info] [graph][runtime.channel] create channel=87 edge=87 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:10:34.238] [info] [graph][runtime.channel] create channel=88 edge=88 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:10:34.238] [info] [graph][runtime.channel] create channel=89 edge=89 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:10:34.238] [info] [graph][runtime.channel] create channel=90 edge=90 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:10:34.238] [info] [graph][runtime.channel] create channel=91 edge=91 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-10-19 16:10:34.239] [info] [graph][runtime.channel] create channel=92 edge=92 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:10:34.239] [info] [graph][runtime.channel] create channel=93 edge=93 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:10:34.239] [info] [graph][runtime.channel] create channel=94 edge=94 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:10:34.239] [info] [graph][runtime.channel] create channel=95 edge=95 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:10:34.239] [info] [graph][runtime.channel] create channel=96 edge=96 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:10:34.239] [info] [graph][runtime.channel] create channel=97 edge=97 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:10:34.239] [info] [graph][runtime.channel] create channel=98 edge=98 stream=Audio edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.239] [info] [graph][runtime.channel] create channel=99 edge=99 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:10:34.239] [info] [graph][runtime.channel] create channel=100 edge=100 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.239] [info] [graph][runtime.channel] create channel=101 edge=101 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0
[2026-10-19 16:10:34.239] [info] [graph][runtime.channel] create channel=102 edge=102 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:10:34.239] [info] [graph][runtime.channel] create channel=103 edge=103 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:10:34.239] [info] [graph][runtime.channel] create channel=104 edge=104 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:10:34.240] [info] [graph][runtime.channel] create channel=105 edge=105 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0
[2026-10-19 16:10:34.240] [info] [graph][runtime.channel] create channel=106 edge=106 stream=Video edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-19 16:10:34.240] [info] [graph][runtime.channel] create channel=107 edge=107 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-19 16:10:34.240] [info] [graph][runtime.channel] create channel=108 edge=108 stream=Any edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-19 16:10:34.240] [info] [graph][runtime.channel] create channel=109 edge=109 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.240] [info] [graph][runtime.channel] create channel=110 edge=110 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.240] [info] [graph][runtime.channel] create channel=111 edge=111 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-19 16:10:34.240] [info] [graph][runtime.channel] create channel=112 edge=112 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:10:34.240] [info] [graph][runtime.channel] create channel=113 edge=113 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:10:34.240] [info] [graph][runtime.channel] create channel=114 edge=114 stream=Any edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-19 16:10:34.240] [info] [graph][runtime.channel] create channel=115 edge=115 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:10:34.240] [info] [graph][runtime.channel] create channel=116 edge=116 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-19 16:10:34.240] [info] [graph][runtime.channel] create channel=117 edge=117 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-19 16:10:34.240] [info] [graph][runtime.channel] create channel=118 edge=118 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-19 16:10:34.240] [info] [graph][runtime.channel] create channel=119 edge=119 stream=Any edge_kind=EncodedPacket payload=Unknown queued=0
[2026-10-19 16:10:34.241] [info] [graph][runtime.channel] create channel=120 edge=120 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-19 16:10:34.241] [info] [graph][runtime.channel] create channel=121 edge=121 stream=Metadata edge_kind=Unknown payload=Unknown queued=0
[2026-10-19 16:10:34.241] [info] [graph][runtime.channel] create channel=122 edge=122 stream=Metadata edge_kind=Unknown payload=Unknown queued=0
[2026-10-19 16:10:34.241] [info] [graph][runtime.channel] create channel=123 edge=123 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-19 16:10:34.241] [info] [graph][runtime.lifecycle] compiled nodes=82 edges=123 channels=123 execution_order=1->3->8->27->46->47->9->21->28->40->63->48->69->10->29->49->11->12->30->31->50->51->52->13->32->53->14->17->33->36->54->55->59->15->34->56->57->16->35->58->18->37->60->19->38->61->20->39->62->22->41->64->70->23->42->65->71->24->43->66->72->26->45->68->73->25->44->67->6->2->7->81->78->4->77->5->74->75->79->76->80->82
[2026-10-19 16:10:34.241] [info] [graph][runtime.lifecycle] compile.done state=DefaultRegistrationPending
[2026-10-19 16:10:34.241] [info] [graph][runtime.node] register node=1 name=composition-r270.output.video.codec_resolver kind=CodecResolver
[2026-10-19 16:10:34.241] [info] [graph][runtime.node] register node=2 name=composition-r270.output.video.encode kind=VideoEncode
[2026-10-19 16:10:34.241] [info] [graph][runtime.node] register node=3 name=composition-r270.output.audio.codec_resolver kind=AudioCodecResolver
[2026-10-19 16:10:34.242] [info] [graph][runtime.node] register node=4 name=composition-r270.output.audio.encode kind=AudioEncode
[2026-10-19 16:10:34.242] [info] [graph][runtime.node] register node=5 name=composition-r270.output.audio.canonicalizer kind=EncodedAudioCanonicalizer
[2026-10-19 16:10:34.242] [info] [graph][runtime.node] register node=8 name=composition-r270.source_0.input kind=RawRtpInput
[2026-10-19 16:10:34.242] [info] [graph][runtime.node] register node=9 name=composition-r270.source_0.rtp.clock_group kind=RtpClockGroup
[2026-10-19 16:10:34.242] [info] [graph][runtime.node] register node=10 name=composition-r270.source_0.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-10-19 16:10:34.242] [info] [graph][runtime.node] register node=11 name=composition-r270.source_0.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-10-19 16:10:34.242] [info] [graph][runtime.node] register node=12 name=composition-r270.source_0.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-10-19 16:10:34.242] [info] [graph][runtime.node] register node=13 name=composition-r270.source_0.av_sync.source_clock kind=SourceClockStateFanout
[2026-10-19 16:10:34.242] [info] [graph][runtime.node] register node=14 name=composition-r270.source_0.av_sync.video.generation_gate kind=LockedPacketGate
[2026-10-19 16:10:34.242] [info] [graph][runtime.node] register node=15 name=composition-r270.source_0.av_sync.video.canonical_input kind=CanonicalInput
[2026-10-19 16:10:34.242] [info] [graph][runtime.node] register node=16 name=composition-r270.source_0.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-10-19 16:10:34.242] [info] [graph][runtime.node] register node=17 name=composition-r270.source_0.av_sync.startup.clock kind=AvStartupClock
[2026-10-19 16:10:34.242] [info] [graph][runtime.node] register node=18 name=composition-r270.source_0.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-10-19 16:10:34.243] [info] [graph][runtime.node] register node=20 name=composition-r270.source_0.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-10-19 16:10:34.243] [info] [graph][runtime.node] register node=21 name=composition-r270.source_0.video.codec_resolver kind=CodecResolver
[2026-10-19 16:10:34.243] [info] [graph][runtime.node] register node=22 name=composition-r270.source_0.video.decode kind=VideoDecode
[2026-10-19 16:10:34.243] [info] [graph][runtime.node] register node=23 name=composition-r270.source_0.video.hwtransfer kind=HardwareTransfer
[2026-10-19 16:10:34.243] [info] [graph][runtime.node] register node=24 name=composition-r270.source_0.video.framerate kind=VideoFrameRate
[2026-10-19 16:10:34.243] [info] [graph][runtime.node] register node=25 name=composition-r270.source_0.video.preparation kind=VideoPreparation
[2026-10-19 16:10:34.243] [info] [graph][runtime.node] register node=26 name=composition-r270.source_0.video.filter kind=VideoFilter
[2026-10-19 16:10:34.243] [info] [graph][runtime.node] register node=27 name=composition-r270.source_1.input kind=RawRtpInput
[2026-10-19 16:10:34.243] [info] [graph][runtime.node] register node=28 name=composition-r270.source_1.rtp.clock_group kind=RtpClockGroup
[2026-10-19 16:10:34.243] [info] [graph][runtime.node] register node=29 name=composition-r270.source_1.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-10-19 16:10:34.243] [info] [graph][runtime.node] register node=30 name=composition-r270.source_1.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-10-19 16:10:34.243] [info] [graph][runtime.node] register node=31 name=composition-r270.source_1.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-10-19 16:10:34.243] [info] [graph][runtime.node] register node=32 name=composition-r270.source_1.av_sync.source_clock kind=SourceClockStateFanout
[2026-10-19 16:10:34.243] [info] [graph][runtime.node] register node=33 name=composition-r270.source_1.av_sync.video.generation_gate kind=LockedPacketGate
[2026-10-19 16:10:34.243] [info] [graph][runtime.node] register node=34 name=composition-r270.source_1.av_sync.video.canonical_input kind=CanonicalInput
[2026-10-19 16:10:34.243] [info] [graph][runtime.node] register node=35 name=composition-r270.source_1.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-10-19 16:10:34.243] [info] [graph][runtime.node] register node=36 name=composition-r270.source_1.av_sync.startup.clock kind=AvStartupClock
[2026-10-19 16:10:34.244] [info] [graph][runtime.node] register node=37 name=composition-r270.source_1.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-10-19 16:10:34.244] [info] [graph][runtime.node] register node=39 name=composition-r270.source_1.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-10-19 16:10:34.244] [info] [graph][runtime.node] register node=40 name=composition-r270.source_1.video.codec_resolver kind=CodecResolver
[2026-10-19 16:10:34.244] [info] [graph][runtime.node] register node=41 name=composition-r270.source_1.video.decode kind=VideoDecode
[2026-10-19 16:10:34.244] [info] [graph][runtime.node] register node=42 name=composition-r270.source_1.video.hwtransfer kind=HardwareTransfer
[2026-10-19 16:10:34.244] [info] [graph][runtime.node] register node=43 name=composition-r270.source_1.video.framerate kind=VideoFrameRate
[2026-10-19 16:10:34.244] [info] [graph][runtime.node] register node=44 name=composition-r270.source_1.video.preparation kind=VideoPreparation
[2026-10-19 16:10:34.244] [info] [graph][runtime.node] register node=45 name=composition-r270.source_1.video.filter kind=VideoFilter
[2026-10-19 16:10:34.244] [info] [graph][runtime.node] register node=46 name=composition-r270.source_2.video.input kind=RawRtpInput
[2026-10-19 16:10:34.244] [info] [graph][runtime.node] register node=47 name=composition-r270.source_2.audio.input kind=RawRtpInput
[2026-10-19 16:10:34.244] [info] [graph][runtime.node] register node=48 name=composition-r270.source_2.rtp.clock_group kind=RtpClockGroup
[2026-10-19 16:10:34.244] [info] [graph][runtime.node] register node=49 name=composition-r270.source_2.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-10-19 16:10:34.244] [info] [graph][runtime.node] register node=50 name=composition-r270.source_2.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-10-19 16:10:34.244] [info] [graph][runtime.node] register node=51 name=composition-r270.source_2.av_sync.audio.protocol_binder kind=RtpPacketClockBinder
[2026-10-19 16:10:34.244] [info] [graph][runtime.node] register node=52 name=composition-r270.source_2.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-10-19 16:10:34.244] [info] [graph][runtime.node] register node=53 name=composition-r270.source_2.av_sync.source_clock kind=SourceClockStateFanout
[2026-10-19 16:10:34.244] [info] [graph][runtime.node] register node=54 name=composition-r270.source_2.av_sync.video.generation_gate kind=LockedPacketGate
[2026-10-19 16:10:34.245] [info] [graph][runtime.node] register node=55 name=composition-r270.source_2.av_sync.audio.generation_gate kind=LockedPacketGate
[2026-10-19 16:10:34.245] [info] [graph][runtime.node] register node=56 name=composition-r270.source_2.av_sync.video.canonical_input kind=CanonicalInput
[2026-10-19 16:10:34.245] [info] [graph][runtime.node] register node=57 name=composition-r270.source_2.av_sync.audio.canonical_input kind=CanonicalInput
[2026-10-19 16:10:34.245] [info] [graph][runtime.node] register node=58 name=composition-r270.source_2.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-10-19 16:10:34.245] [info] [graph][runtime.node] register node=59 name=composition-r270.source_2.av_sync.startup.clock kind=AvStartupClock
[2026-10-19 16:10:34.245] [info] [graph][runtime.node] register node=60 name=composition-r270.source_2.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-10-19 16:10:34.245] [info] [graph][runtime.node] register node=62 name=composition-r270.source_2.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-10-19 16:10:34.245] [info] [graph][runtime.node] register node=63 name=composition-r270.source_2.video.codec_resolver kind=CodecResolver
[2026-10-19 16:10:34.245] [info] [graph][runtime.node] register node=64 name=composition-r270.source_2.video.decode kind=VideoDecode
[2026-10-19 16:10:34.245] [info] [graph][runtime.node] register node=65 name=composition-r270.source_2.video.hwtransfer kind=HardwareTransfer
[2026-10-19 16:10:34.245] [info] [graph][runtime.node] register node=66 name=composition-r270.source_2.video.framerate kind=VideoFrameRate
[2026-10-19 16:10:34.245] [info] [graph][runtime.node] register node=67 name=composition-r270.source_2.video.preparation kind=VideoPreparation
[2026-10-19 16:10:34.245] [info] [graph][runtime.node] register node=68 name=composition-r270.source_2.video.filter kind=VideoFilter
[2026-10-19 16:10:34.246] [info] [graph][runtime.node] register node=69 name=composition-r270.source_2.audio.encode.codec_resolver kind=AudioCodecResolver
[2026-10-19 16:10:34.246] [info] [graph][runtime.node] register node=70 name=composition-r270.source_2.audio.encode.decode kind=AudioDecode
[2026-10-19 16:10:34.246] [info] [graph][runtime.node] register node=71 name=composition-r270.source_2.audio.encode.startup_trim kind=Unknown
[2026-10-19 16:10:34.246] [info] [graph][runtime.node] register node=72 name=composition-r270.source_2.audio.encode.drift_controller kind=AudioDriftController
[2026-10-19 16:10:34.246] [info] [graph][runtime.node] register node=73 name=composition-r270.source_2.audio.encode.resample kind=AudioResample
[2026-10-19 16:10:34.246] [info] [graph][runtime.node] register node=74 name=composition-r270.output.scheduler.scheduler kind=AvOutputScheduler
[2026-10-19 16:10:34.246] [info] [graph][runtime.node] register node=75 name=composition-r270.output.scheduler.router kind=ScheduledOutputRouter
[2026-10-19 16:10:34.246] [info] [graph][runtime.node] register node=76 name=composition-r270.output.mpegts.mux kind=FileMux
[2026-10-19 16:10:34.246] [info] [graph][runtime.node] register node=78 name=composition-r270.output.mpegts.plan.source kind=Unknown
[2026-10-19 16:10:34.246] [info] [graph][runtime.node] register node=79 name=composition-r270.output.mpegts.scheduled.adapter kind=Unknown
[2026-10-19 16:10:34.246] [info] [graph][runtime.node] register node=80 name=composition-r270.output.mpegts.datagram.materializer kind=MpegTsDatagramMaterializer
[2026-10-19 16:10:34.246] [info] [graph][runtime.node] register node=81 name=composition-r270.output.mpegts.transport.plan kind=DatagramTransportPlanSource
[2026-10-19 16:10:34.246] [info] [graph][runtime.node] register node=82 name=composition-r270.output.mpegts.transport.sender kind=ScheduledDatagramSender
[2026-10-19 16:10:34.247] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=waiting_for_random_access detail="" description="D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r270-output.sdp"
[2026-10-19 16:10:34.247] [info] [graph][runtime.lifecycle] start.begin mode=threaded
[2026-10-19 16:10:34.248] [info] [graph][runtime.lifecycle] worker.finished node=1
[2026-10-19 16:10:34.248] [info] [graph][runtime.node] worker.thread_cpu node=1 available=1 cpu_ns=0 process_calls=1
[2026-10-19 16:10:34.248] [info] [graph][runtime.lifecycle] worker.finished node=63
[2026-10-19 16:10:34.248] [info] [graph][runtime.node] worker.thread_cpu node=63 available=1 cpu_ns=0 process_calls=1
[2026-10-19 16:10:34.249] [info] [graph][runtime.lifecycle] start.done state=ThreadedRunning
[2026-10-19 16:10:34.249] [info] [graph][runtime.lifecycle] worker.finished node=3
[2026-10-19 16:10:34.250] [info] [graph][runtime.lifecycle] worker.finished node=40
[2026-10-19 16:10:34.250] [info] [graph][runtime.node] worker.thread_cpu node=40 available=1 cpu_ns=0 process_calls=2
[2026-10-19 16:10:34.250] [info] [graph][runtime.node] worker.thread_cpu node=3 available=1 cpu_ns=0 process_calls=1
[2026-10-19 16:10:34.250] [info] [graph][runtime.lifecycle] worker.finished node=21
[2026-10-19 16:10:34.251] [info] [graph][runtime.node] worker.thread_cpu node=21 available=1 cpu_ns=0 process_calls=1
[2026-10-19 16:10:34.251] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:10:34.251] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:10:34.251] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-10-19 16:10:34.251] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-10-19 16:10:34.251] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=1 generation=1
[2026-10-19 16:10:34.251] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=1 generation=1
[2026-10-19 16:10:34.252] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=3536824886 dts=3536824886 size=33389 generation=1 sequence=1
[2026-10-19 16:10:34.252] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=4069709191 dts=4069709191 size=33389 generation=1 sequence=1
[2026-10-19 16:10:34.252] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:10:34.252] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-10-19 16:10:34.253] [info] [graph][runtime.lifecycle] worker.finished node=69
[2026-10-19 16:10:34.253] [info] [graph][runtime.node] worker.thread_cpu node=69 available=1 cpu_ns=0 process_calls=1
[2026-10-19 16:10:34.253] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=1 generation=1
[2026-10-19 16:10:34.255] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=1780659482 dts=1780659482 size=33389 generation=1 sequence=1
[2026-10-19 16:10:34.257] [info] [graph][runtime.node] video_encode.bind_encoder codec=hevc_nvenc pix_fmt=cuda rate_control=cbr bitrate=8000000 min_bitrate=8000000 max_bitrate=8000000 buffer_size=267000 fps=30/1 gop=60 frame_kind=hardware hwaccel=cuda hw_device_ctx=set hw_frames_ctx=set
[2026-10-19 16:10:34.258] [info] [graph][runtime.node] audio_encode_trace stage=codec_bound
[2026-10-19 16:10:34.278] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-10-19 16:10:34.279] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=3536824886 format=cuda size=1280x720 buffer_identity=0xb08400000 drm_prime=0 software_frame=0
[2026-10-19 16:10:34.279] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=0000017FFF01E2B0 stream=Video payload=Frame pts=3536824886 dts=3536824886 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:10:34.279] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=0000017FFF01E2B0 stream=Video payload=Frame pts=3536824886 dts=3536824886 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:10:34.279] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=0000017FFF01E2B0 stream=Video payload=Frame pts=3536824886 dts=3536824886 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:10:34.280] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-10-19 16:10:34.280] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=0000017FFF01D2F0 stream=Video payload=Frame pts=3536824886 dts=3536824886 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:10:34.280] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=0000017FFF01D2F0 stream=Video payload=Frame pts=3536824886 dts=3536824886 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:10:34.280] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-10-19 16:10:34.281] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-10-19 16:10:34.281] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=4069709191 format=cuda size=1280x720 buffer_identity=0xb08800000 drm_prime=0 software_frame=0
[2026-10-19 16:10:34.281] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=0000017FFF01DAD0 stream=Video payload=Frame pts=4069709191 dts=4069709191 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:10:34.282] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=0000017FFF01DAD0 stream=Video payload=Frame pts=4069709191 dts=4069709191 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:10:34.282] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=0000017FFF01DAD0 stream=Video payload=Frame pts=4069709191 dts=4069709191 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:10:34.282] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-10-19 16:10:34.282] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=0000017FFF01E2B0 stream=Video payload=Frame pts=4069709191 dts=4069709191 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:10:34.282] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=0000017FFF01E2B0 stream=Video payload=Frame pts=4069709191 dts=4069709191 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:10:34.283] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-10-19 16:10:34.283] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-10-19 16:10:34.283] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=1780659482 format=cuda size=1280x720 buffer_identity=0xb08c00000 drm_prime=0 software_frame=0
[2026-10-19 16:10:34.284] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=0000017FA621A4A0 stream=Video payload=Frame pts=1780659482 dts=1780659482 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:10:34.284] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=0000017FA621A4A0 stream=Video payload=Frame pts=1780659482 dts=1780659482 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:10:34.284] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=0000017FA621A4A0 stream=Video payload=Frame pts=1780659482 dts=1780659482 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:10:34.284] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-10-19 16:10:34.285] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=0000017FA6217560 stream=Video payload=Frame pts=1780659482 dts=1780659482 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:10:34.285] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=0000017FA6217560 stream=Video payload=Frame pts=1780659482 dts=1780659482 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:10:34.285] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-10-19 16:10:34.333] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=49
[2026-10-19 16:10:34.333] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=52
[2026-10-19 16:10:34.333] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=47
[2026-10-19 16:10:34.334] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb08e90000 drm_prime=0 software_frame=0
[2026-10-19 16:10:34.334] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb08e00000 drm_prime=0 software_frame=0
[2026-10-19 16:10:34.334] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=16 audio_count=0 first_video_sequence=1
[2026-10-19 16:10:34.334] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=16 audio_count=0 first_video_sequence=1
[2026-10-19 16:10:34.335] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=0
[2026-10-19 16:10:34.335] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=0
[2026-10-19 16:10:34.336] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb08f20000 drm_prime=0 software_frame=0
[2026-10-19 16:10:34.336] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=24 audio_count=22 first_video_sequence=1
[2026-10-19 16:10:34.336] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=22
[2026-10-19 16:10:34.336] [info] [graph][runtime.node] audio_decode_trace stage=first_packet
[2026-10-19 16:10:34.336] [info] [graph][runtime.node] audio_decode_trace stage=first_frame samples=1024 rate=44100
[2026-10-19 16:10:34.337] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1 source_master_ns=682514421 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-10-19 16:10:34.337] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2 source_master_ns=705734376 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=45346
[2026-10-19 16:10:34.337] [info] [graph][runtime.node] audio_encode_trace stage=first_frame
[2026-10-19 16:10:34.338] [info] [graph][runtime.node] mpegts_emission_plan generation=1 mode=canonical_access_unit_window access_unit_window_ns=100000000 video_initial_service_window_ns=13995843 audio_initial_service_window_ns=13995843 packet_size_bytes=188 maximum_payload_bytes=1316 per_datagram_overhead_bytes=12
[2026-10-19 16:10:34.353] [info] [graph][runtime.node] video_encode.input_contract format=cuda size=1280x720 buffer_identity=0xb0a000000 drm_prime=0 software_frame=0
[2026-10-19 16:10:34.353] [info] [graph][runtime.node] video_encode.generation_start_key_frame pts=0 generation=1
[2026-10-19 16:10:34.353] [info] [graph][runtime.node] video_encode.first_frame pts=0 generation=attached
[2026-10-19 16:10:34.359] [info] [graph][runtime.node] video_encode.first_submit result=accepted
[2026-10-19 16:10:34.359] [info] [graph][runtime.node] video_encode.first_packet pts=0 dts=0 duration=0 size=259 side_data_elems=1 received_footprint=267 side_data[0]=Quality stats:8
[2026-10-19 16:10:34.359] [info] [graph][runtime.node] video_encode.first_packet_footprint bytes=267 wrapped_size=259 wrapped_side_data_elems=1
[2026-10-19 16:10:34.359] [info] [graph][runtime.node] av_scheduler_trace stage=first_video_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=660586868 dispatch_master_ns=660586868 now_ns=516031400 node=74 identity_kind=output identity="composition-r270.output" canonical_sequence=1 canonical_created_steady_ns=26848276752500 head_observed_steady_ns=26848276810900 codec_receive_begin_steady_ns=26848276440300 codec_receive_end_steady_ns=26848276441800
[2026-10-19 16:10:34.360] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=516174500 present_generation=1 present_canonical_target_ns=0 present_canonical_dispatch_ns=0 present_target_master_ns=660586868 present_dispatch_master_ns=660586868
[2026-10-19 16:10:34.384] [info] [graph][runtime.node] audio_encode_trace stage=first_packet
[2026-10-19 16:10:34.384] [info] [graph][runtime.node] av_scheduler_trace stage=first_audio_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=660586868 dispatch_master_ns=660586868 now_ns=541101300 node=74 identity_kind=output identity="composition-r270.output" canonical_sequence=1 canonical_created_steady_ns=26848301860300 head_observed_steady_ns=26848301880900 codec_receive_begin_steady_ns=unavailable codec_receive_end_steady_ns=unavailable
[2026-10-19 16:10:34.565] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=running detail="" description="D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r270-output.sdp"
[2026-10-19 16:10:34.942] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=47 source_master_ns=1750632335 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=90692
[2026-10-19 16:10:35.963] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=91 source_master_ns=2772310340 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=136038
[2026-10-19 16:10:36.986] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=135 source_master_ns=3793988344 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=181384
[2026-10-19 16:10:38.030] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=180 source_master_ns=4838886303 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=226730
[2026-10-19 16:10:38.852] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=29718129 ntp_seconds=4001386238 ntp_fraction=3659312136 arrival_ns=26852769051500
[2026-10-19 16:10:38.852] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=29718129
[2026-10-19 16:10:38.852] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:10:38.852] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2870565155 ntp_seconds=4001386238 ntp_fraction=3659312136 arrival_ns=26852769083700
[2026-10-19 16:10:38.853] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2870565155
[2026-10-19 16:10:38.853] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:10:38.866] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3853990634 ntp_seconds=4001386238 ntp_fraction=3719441678 arrival_ns=26852783477500
[2026-10-19 16:10:38.867] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3853990634
[2026-10-19 16:10:38.867] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:10:38.894] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3368638434 ntp_seconds=4001386238 ntp_fraction=3839700762 arrival_ns=26852811168100
[2026-10-19 16:10:38.894] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3368638434
[2026-10-19 16:10:38.895] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:10:39.051] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=224 source_master_ns=5860564308 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=272076
[2026-10-19 16:10:40.073] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=268 source_master_ns=6882242312 raw_phase_ns=22674 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=317422
[2026-10-19 16:10:41.095] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=312 source_master_ns=7903920317 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=362768
[2026-10-19 16:10:42.140] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=357 source_master_ns=8948818276 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=408114
[2026-10-19 16:10:43.161] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=401 source_master_ns=9970496281 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=453460
[2026-10-19 16:10:43.866] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2870565155 ntp_seconds=4001386243 ntp_fraction=3719441678 arrival_ns=26857783569900
[2026-10-19 16:10:43.866] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=29718129 ntp_seconds=4001386243 ntp_fraction=3719441678 arrival_ns=26857783558000
[2026-10-19 16:10:43.867] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=29718129
[2026-10-19 16:10:43.867] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:10:43.867] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2870565155
[2026-10-19 16:10:43.867] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:10:43.881] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3853990634 ntp_seconds=4001386243 ntp_fraction=3783866187 arrival_ns=26857798298600
[2026-10-19 16:10:43.882] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3853990634
[2026-10-19 16:10:43.882] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:10:43.895] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3368638434 ntp_seconds=4001386243 ntp_fraction=3843995729 arrival_ns=26857812836200
[2026-10-19 16:10:43.896] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3368638434
[2026-10-19 16:10:43.896] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:10:44.183] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=445 source_master_ns=10992174285 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=498806
[2026-10-19 16:10:45.228] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=490 source_master_ns=12037072244 raw_phase_ns=22674 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=544152
[2026-10-19 16:10:46.249] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=534 source_master_ns=13058750249 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=589498
[2026-10-19 16:10:47.271] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=578 source_master_ns=14080428253 raw_phase_ns=22674 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=634844
[2026-10-19 16:10:48.293] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=622 source_master_ns=15102106258 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=680190
[2026-10-19 16:10:48.758] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=14914386000 present_generation=1 present_canonical_target_ns=14373151927 present_canonical_dispatch_ns=14373151927 present_target_master_ns=15033738795 present_dispatch_master_ns=15033738795
[2026-10-19 16:10:48.759] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=14915537400 present_generation=1 present_canonical_target_ns=14400000000 present_canonical_dispatch_ns=14400000000 present_target_master_ns=15060586868 present_dispatch_master_ns=15060586868
[2026-10-19 16:10:48.893] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2870565155 ntp_seconds=4001386248 ntp_fraction=3835405795 arrival_ns=26862810405300
[2026-10-19 16:10:48.894] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2870565155
[2026-10-19 16:10:48.894] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:10:48.894] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=29718129 ntp_seconds=4001386248 ntp_fraction=3835405795 arrival_ns=26862810905000
[2026-10-19 16:10:48.895] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=29718129
[2026-10-19 16:10:48.895] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:10:48.895] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3853990634 ntp_seconds=4001386248 ntp_fraction=3843995729 arrival_ns=26862812581300
[2026-10-19 16:10:48.895] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3853990634
[2026-10-19 16:10:48.895] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:10:48.924] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3368638434 ntp_seconds=4001386248 ntp_fraction=3964254814 arrival_ns=26862841080000
[2026-10-19 16:10:48.924] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3368638434
[2026-10-19 16:10:48.924] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:10:49.338] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=667 source_master_ns=16147004217 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=725536
[2026-10-19 16:10:50.359] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=711 source_master_ns=17168682222 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=770882
[2026-10-19 16:10:51.381] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=755 source_master_ns=18190360226 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=816228
[2026-10-19 16:10:52.426] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=800 source_master_ns=19235258185 raw_phase_ns=22674 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=861574
[2026-10-19 16:10:53.448] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=844 source_master_ns=20256936190 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=906920
[2026-10-19 16:10:53.911] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3853990634 ntp_seconds=4001386253 ntp_fraction=3912715206 arrival_ns=26867828410500
[2026-10-19 16:10:53.912] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3853990634
[2026-10-19 16:10:53.912] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:10:53.927] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2870565155 ntp_seconds=4001386253 ntp_fraction=3977139716 arrival_ns=26867843790200
[2026-10-19 16:10:53.927] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2870565155
[2026-10-19 16:10:53.927] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:10:53.927] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3368638434 ntp_seconds=4001386253 ntp_fraction=3977139716 arrival_ns=26867843720100
[2026-10-19 16:10:53.927] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=29718129 ntp_seconds=4001386253 ntp_fraction=3977139716 arrival_ns=26867843868500
[2026-10-19 16:10:53.928] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=29718129
[2026-10-19 16:10:53.928] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3368638434
[2026-10-19 16:10:53.929] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:10:53.929] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:10:54.470] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=888 source_master_ns=21278614195 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=952266
[2026-10-19 16:10:55.491] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=932 source_master_ns=22300292199 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=997612
[2026-10-19 16:10:56.536] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=977 source_master_ns=23345190158 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1042958
[2026-10-19 16:10:57.558] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1021 source_master_ns=24366868163 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1088304
[2026-10-19 16:10:58.579] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1065 source_master_ns=25388546167 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1133650
[2026-10-19 16:10:58.924] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3853990634 ntp_seconds=4001386258 ntp_fraction=3964254814 arrival_ns=26872840950000
[2026-10-19 16:10:58.924] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3853990634
[2026-10-19 16:10:58.924] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:10:58.953] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=29718129 ntp_seconds=4001386258 ntp_fraction=4093103833 arrival_ns=26872870245800
[2026-10-19 16:10:58.953] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2870565155 ntp_seconds=4001386258 ntp_fraction=4093103833 arrival_ns=26872870314100
[2026-10-19 16:10:58.953] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2870565155
[2026-10-19 16:10:58.953] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=29718129
[2026-10-19 16:10:58.953] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3368638434 ntp_seconds=4001386258 ntp_fraction=4093103833 arrival_ns=26872870426100
[2026-10-19 16:10:58.954] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:10:58.954] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:10:58.954] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3368638434
[2026-10-19 16:10:58.954] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:10:59.624] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1110 source_master_ns=26433444126 raw_phase_ns=22674 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1178996
[2026-10-19 16:11:00.646] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1154 source_master_ns=27455122131 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1224342
[2026-10-19 16:11:01.667] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1198 source_master_ns=28476800136 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1269688
[2026-10-19 16:11:02.689] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1242 source_master_ns=29498478140 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1315034
[2026-10-19 16:11:03.734] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1287 source_master_ns=30543376099 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1360380
[2026-10-19 16:11:03.939] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3853990634 ntp_seconds=4001386263 ntp_fraction=4032974290 arrival_ns=26877856809700
[2026-10-19 16:11:03.940] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3853990634
[2026-10-19 16:11:03.940] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:03.954] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=29718129 ntp_seconds=4001386263 ntp_fraction=4097398800 arrival_ns=26877871686000
[2026-10-19 16:11:03.955] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=29718129
[2026-10-19 16:11:03.955] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:03.955] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2870565155 ntp_seconds=4001386263 ntp_fraction=4101693767 arrival_ns=26877872328700
[2026-10-19 16:11:03.955] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2870565155
[2026-10-19 16:11:03.955] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:03.955] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3368638434 ntp_seconds=4001386263 ntp_fraction=4097398800 arrival_ns=26877871685900
[2026-10-19 16:11:03.956] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3368638434
[2026-10-19 16:11:03.956] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:04.756] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1331 source_master_ns=31565054104 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1405726
[2026-10-19 16:11:05.778] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1375 source_master_ns=32586732108 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1451072
[2026-10-19 16:11:06.823] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1420 source_master_ns=33631630068 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1496418
[2026-10-19 16:11:07.844] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1464 source_master_ns=34653308072 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1541764
[2026-10-19 16:11:08.866] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1508 source_master_ns=35674986077 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1587110
[2026-10-19 16:11:08.961] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3853990634 ntp_seconds=4001386268 ntp_fraction=4127463571 arrival_ns=26882878469400
[2026-10-19 16:11:08.961] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3853990634
[2026-10-19 16:11:08.961] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:08.962] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3368638434 ntp_seconds=4001386268 ntp_fraction=4131758538 arrival_ns=26882879341800
[2026-10-19 16:11:08.962] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3368638434
[2026-10-19 16:11:08.963] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:08.991] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2870565155 ntp_seconds=4001386268 ntp_fraction=4252017623 arrival_ns=26882907664300
[2026-10-19 16:11:08.991] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2870565155
[2026-10-19 16:11:08.991] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:08.991] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=29718129 ntp_seconds=4001386268 ntp_fraction=4256312590 arrival_ns=26882908470400
[2026-10-19 16:11:08.991] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=29718129
[2026-10-19 16:11:08.991] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:09.888] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1552 source_master_ns=36696664081 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1632456
[2026-10-19 16:11:10.932] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1597 source_master_ns=37741562040 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1677802
[2026-10-19 16:11:11.954] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1641 source_master_ns=38763240045 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1723148
[2026-10-19 16:11:12.976] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1685 source_master_ns=39784918049 raw_phase_ns=22674 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1768494
[2026-10-19 16:11:13.973] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3853990634 ntp_seconds=4001386273 ntp_fraction=4174708211 arrival_ns=26887889844400
[2026-10-19 16:11:13.973] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3853990634
[2026-10-19 16:11:13.974] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:13.987] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3368638434 ntp_seconds=4001386273 ntp_fraction=4239132721 arrival_ns=26887904478500
[2026-10-19 16:11:13.988] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3368638434
[2026-10-19 16:11:13.988] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:14.018] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2870565155 ntp_seconds=4001386274 ntp_fraction=77309411 arrival_ns=26887935492000
[2026-10-19 16:11:14.018] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2870565155
[2026-10-19 16:11:14.019] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:14.018] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=29718129 ntp_seconds=4001386274 ntp_fraction=77309411 arrival_ns=26887935571800
[2026-10-19 16:11:14.019] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=29718129
[2026-10-19 16:11:14.019] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:14.021] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1730 source_master_ns=40829816009 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1813840
[2026-10-19 16:11:15.043] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1774 source_master_ns=41851494013 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1859186
[2026-10-19 16:11:16.064] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1818 source_master_ns=42873172018 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1904532
[2026-10-19 16:11:17.086] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1862 source_master_ns=43894850022 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1949878
[2026-10-19 16:11:18.131] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1907 source_master_ns=44939747981 raw_phase_ns=22674 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1995224
[2026-10-19 16:11:18.993] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3853990634 ntp_seconds=4001386278 ntp_fraction=4260607557 arrival_ns=26892909869000
[2026-10-19 16:11:18.993] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3853990634
[2026-10-19 16:11:18.994] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:18.994] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3368638434 ntp_seconds=4001386278 ntp_fraction=4264902524 arrival_ns=26892910260900
[2026-10-19 16:11:18.994] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3368638434
[2026-10-19 16:11:18.995] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:19.022] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=29718129 ntp_seconds=4001386279 ntp_fraction=90194313 arrival_ns=26892938937200
[2026-10-19 16:11:19.022] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=29718129
[2026-10-19 16:11:19.022] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2870565155 ntp_seconds=4001386279 ntp_fraction=90194313 arrival_ns=26892938883400
[2026-10-19 16:11:19.022] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:19.023] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2870565155
[2026-10-19 16:11:19.023] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:19.152] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1951 source_master_ns=45961425986 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2040570
[2026-10-19 16:11:20.174] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1995 source_master_ns=46983103990 raw_phase_ns=22674 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2085916
[2026-10-19 16:11:21.219] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2040 source_master_ns=48028001950 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2131262
[2026-10-19 16:11:22.241] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2084 source_master_ns=49049679954 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2176608
[2026-10-19 16:11:23.262] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2128 source_master_ns=50071357959 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2221954
[2026-10-19 16:11:24.004] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3853990634 ntp_seconds=4001386284 ntp_fraction=17179869 arrival_ns=26897921255300
[2026-10-19 16:11:24.004] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3853990634
[2026-10-19 16:11:24.004] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:24.018] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3368638434 ntp_seconds=4001386284 ntp_fraction=77309411 arrival_ns=26897935850700
[2026-10-19 16:11:24.019] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3368638434
[2026-10-19 16:11:24.019] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:24.049] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2870565155 ntp_seconds=4001386284 ntp_fraction=206158430 arrival_ns=26897965776600
[2026-10-19 16:11:24.049] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2870565155
[2026-10-19 16:11:24.049] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:24.049] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=29718129 ntp_seconds=4001386284 ntp_fraction=210453397 arrival_ns=26897966266700
[2026-10-19 16:11:24.050] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=29718129
[2026-10-19 16:11:24.050] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:24.284] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2172 source_master_ns=51093035963 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2267300
[2026-10-19 16:11:25.329] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2217 source_master_ns=52137933922 raw_phase_ns=22674 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2312646
[2026-10-19 16:11:26.351] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2261 source_master_ns=53159611927 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2357992
[2026-10-19 16:11:27.379] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2305 source_master_ns=54181289931 raw_phase_ns=22674 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2403338
[2026-10-19 16:11:28.417] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2350 source_master_ns=55226187891 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2448684
[2026-10-19 16:11:29.012] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3853990634 ntp_seconds=4001386289 ntp_fraction=51539607 arrival_ns=26902929264900
[2026-10-19 16:11:29.012] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3853990634
[2026-10-19 16:11:29.012] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:29.043] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3368638434 ntp_seconds=4001386289 ntp_fraction=184683593 arrival_ns=26902960199700
[2026-10-19 16:11:29.044] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3368638434
[2026-10-19 16:11:29.044] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:29.055] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=29718129 ntp_seconds=4001386289 ntp_fraction=231928233 arrival_ns=26902971925500
[2026-10-19 16:11:29.056] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=29718129
[2026-10-19 16:11:29.056] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:29.056] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2870565155 ntp_seconds=4001386289 ntp_fraction=240518168 arrival_ns=26902973713100
[2026-10-19 16:11:29.056] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2870565155
[2026-10-19 16:11:29.057] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:29.440] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2394 source_master_ns=56247865895 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2494030
[2026-10-19 16:11:29.489] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=55645572400 present_generation=1 present_canonical_target_ns=55100952380 present_canonical_dispatch_ns=55100952380 present_target_master_ns=55761539248 present_dispatch_master_ns=55761539248
[2026-10-19 16:11:29.494] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=55650259200 present_generation=1 present_canonical_target_ns=55133333333 present_canonical_dispatch_ns=55133333333 present_target_master_ns=55793920201 present_dispatch_master_ns=55793920201
[2026-10-19 16:11:30.461] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2438 source_master_ns=57269543900 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2539376
[2026-10-19 16:11:31.482] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2482 source_master_ns=58291221904 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2584722
[2026-10-19 16:11:32.527] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2527 source_master_ns=59336119863 raw_phase_ns=22674 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2630068
[2026-10-19 16:11:33.549] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2571 source_master_ns=60357797868 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2675414
[2026-10-19 16:11:34.034] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3853990634 ntp_seconds=4001386294 ntp_fraction=146028888 arrival_ns=26907951140800
[2026-10-19 16:11:34.034] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3853990634
[2026-10-19 16:11:34.034] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:34.062] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3368638434 ntp_seconds=4001386294 ntp_fraction=266287972 arrival_ns=26907979114800
[2026-10-19 16:11:34.062] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3368638434
[2026-10-19 16:11:34.063] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:34.090] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=29718129 ntp_seconds=4001386294 ntp_fraction=386547056 arrival_ns=26908007593600
[2026-10-19 16:11:34.091] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=29718129
[2026-10-19 16:11:34.091] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2870565155 ntp_seconds=4001386294 ntp_fraction=386547056 arrival_ns=26908007938300
[2026-10-19 16:11:34.091] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:34.091] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2870565155
[2026-10-19 16:11:34.092] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:34.570] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2615 source_master_ns=61379475873 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2720760
[2026-10-19 16:11:35.592] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2659 source_master_ns=62401153877 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2766106
[2026-10-19 16:11:36.637] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2704 source_master_ns=63446051836 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2811452
[2026-10-19 16:11:37.658] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2748 source_master_ns=64467729841 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2856798
[2026-10-19 16:11:38.680] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2792 source_master_ns=65489407845 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2902144
[2026-10-19 16:11:39.040] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3853990634 ntp_seconds=4001386299 ntp_fraction=171798691 arrival_ns=26912957123200
[2026-10-19 16:11:39.040] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3853990634
[2026-10-19 16:11:39.040] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:39.095] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3368638434 ntp_seconds=4001386299 ntp_fraction=403726925 arrival_ns=26913012051900
[2026-10-19 16:11:39.095] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3368638434
[2026-10-19 16:11:39.095] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:39.124] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2870565155 ntp_seconds=4001386299 ntp_fraction=532575944 arrival_ns=26913041635300
[2026-10-19 16:11:39.125] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2870565155
[2026-10-19 16:11:39.125] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:39.124] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=29718129 ntp_seconds=4001386299 ntp_fraction=532575944 arrival_ns=26913041637800
[2026-10-19 16:11:39.125] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=29718129
[2026-10-19 16:11:39.125] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:39.726] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2837 source_master_ns=66534305805 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2947490
[2026-10-19 16:11:40.747] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2881 source_master_ns=67555983809 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2992836
[2026-10-19 16:11:41.768] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2925 source_master_ns=68577661814 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3038182
[2026-10-19 16:11:42.790] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2969 source_master_ns=69599339818 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3083528
[2026-10-19 16:11:43.835] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3014 source_master_ns=70644237777 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3128874
[2026-10-19 16:11:44.061] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3853990634 ntp_seconds=4001386304 ntp_fraction=261993005 arrival_ns=26917978662900
[2026-10-19 16:11:44.062] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3853990634
[2026-10-19 16:11:44.062] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:44.118] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3368638434 ntp_seconds=4001386304 ntp_fraction=502511173 arrival_ns=26918034585400
[2026-10-19 16:11:44.118] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3368638434
[2026-10-19 16:11:44.118] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:44.133] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2870565155 ntp_seconds=4001386304 ntp_fraction=571230650 arrival_ns=26918050405000
[2026-10-19 16:11:44.133] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2870565155
[2026-10-19 16:11:44.134] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:44.134] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=29718129 ntp_seconds=4001386304 ntp_fraction=575525617 arrival_ns=26918051193400
[2026-10-19 16:11:44.135] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=29718129
[2026-10-19 16:11:44.135] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:44.857] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3058 source_master_ns=71665915782 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3174220
[2026-10-19 16:11:45.878] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3102 source_master_ns=72687593786 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3219566
[2026-10-19 16:11:46.923] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3147 source_master_ns=73732491746 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3264912
[2026-10-19 16:11:47.945] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3191 source_master_ns=74754169750 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3310258
[2026-10-19 16:11:48.967] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3235 source_master_ns=75775847755 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3355604
[2026-10-19 16:11:49.081] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3853990634 ntp_seconds=4001386309 ntp_fraction=343597383 arrival_ns=26922997631700
[2026-10-19 16:11:49.081] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3853990634
[2026-10-19 16:11:49.082] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:49.122] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3368638434 ntp_seconds=4001386309 ntp_fraction=519691042 arrival_ns=26923038920400
[2026-10-19 16:11:49.123] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3368638434
[2026-10-19 16:11:49.123] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:49.166] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2870565155 ntp_seconds=4001386309 ntp_fraction=712964571 arrival_ns=26923083650300
[2026-10-19 16:11:49.166] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=29718129 ntp_seconds=4001386309 ntp_fraction=712964571 arrival_ns=26923083695900
[2026-10-19 16:11:49.167] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=29718129
[2026-10-19 16:11:49.167] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2870565155
[2026-10-19 16:11:49.168] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:49.168] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:49.988] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3279 source_master_ns=76797525759 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3400950
[2026-10-19 16:11:51.033] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3324 source_master_ns=77842423718 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3446296
[2026-10-19 16:11:52.055] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3368 source_master_ns=78864101723 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3491642
[2026-10-19 16:11:53.076] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3412 source_master_ns=79885779727 raw_phase_ns=22674 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3536988
[2026-10-19 16:11:54.090] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3853990634 ntp_seconds=4001386314 ntp_fraction=386547056 arrival_ns=26928007151700
[2026-10-19 16:11:54.090] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3853990634
[2026-10-19 16:11:54.091] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:54.122] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3457 source_master_ns=80930677687 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3582334
[2026-10-19 16:11:54.164] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3368638434 ntp_seconds=4001386314 ntp_fraction=700079669 arrival_ns=26928081127800
[2026-10-19 16:11:54.165] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3368638434
[2026-10-19 16:11:54.165] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:54.179] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=29718129 ntp_seconds=4001386314 ntp_fraction=768799145 arrival_ns=26928096565100
[2026-10-19 16:11:54.180] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=29718129
[2026-10-19 16:11:54.180] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:54.179] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2870565155 ntp_seconds=4001386314 ntp_fraction=768799145 arrival_ns=26928096648900
[2026-10-19 16:11:54.180] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2870565155
[2026-10-19 16:11:54.180] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:55.143] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3501 source_master_ns=81952355691 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3627680
[2026-10-19 16:11:56.165] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3545 source_master_ns=82974033696 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3673026
[2026-10-19 16:11:57.186] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3589 source_master_ns=83995711700 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3718372
[2026-10-19 16:11:58.231] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3634 source_master_ns=85040609659 raw_phase_ns=22674 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3763718
[2026-10-19 16:11:59.103] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3853990634 ntp_seconds=4001386319 ntp_fraction=442381631 arrival_ns=26933020653400
[2026-10-19 16:11:59.103] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3853990634
[2026-10-19 16:11:59.104] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:59.190] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2870565155 ntp_seconds=4001386319 ntp_fraction=816043786 arrival_ns=26933107571400
[2026-10-19 16:11:59.191] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2870565155
[2026-10-19 16:11:59.191] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:59.192] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3368638434 ntp_seconds=4001386319 ntp_fraction=820338753 arrival_ns=26933108749000
[2026-10-19 16:11:59.192] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3368638434
[2026-10-19 16:11:59.192] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=29718129 ntp_seconds=4001386319 ntp_fraction=824633720 arrival_ns=26933109195900
[2026-10-19 16:11:59.192] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=29718129
[2026-10-19 16:11:59.192] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:59.192] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:11:59.254] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3678 source_master_ns=86062287664 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3809064
[2026-10-19 16:12:00.275] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3722 source_master_ns=87083965668 raw_phase_ns=22674 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3854410
[2026-10-19 16:12:01.320] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3767 source_master_ns=88128863628 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3899756
[2026-10-19 16:12:02.341] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3811 source_master_ns=89150541632 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3945102
[2026-10-19 16:12:03.363] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3855 source_master_ns=90172219637 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3990448
[2026-10-19 16:12:04.129] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3853990634 ntp_seconds=4001386324 ntp_fraction=554050781 arrival_ns=26938046325000
[2026-10-19 16:12:04.130] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3853990634
[2026-10-19 16:12:04.130] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:12:04.199] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3368638434 ntp_seconds=4001386324 ntp_fraction=854698491 arrival_ns=26938116325800
[2026-10-19 16:12:04.200] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3368638434
[2026-10-19 16:12:04.200] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:12:04.227] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=29718129 ntp_seconds=4001386324 ntp_fraction=974957576 arrival_ns=26938144096400
[2026-10-19 16:12:04.227] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=29718129
[2026-10-19 16:12:04.227] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:12:04.227] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2870565155 ntp_seconds=4001386324 ntp_fraction=974957576 arrival_ns=26938144111200
[2026-10-19 16:12:04.228] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2870565155
[2026-10-19 16:12:04.229] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:12:04.385] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3899 source_master_ns=91193897641 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4035794
[2026-10-19 16:12:05.430] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3944 source_master_ns=92238795600 raw_phase_ns=22674 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4081140
[2026-10-19 16:12:06.451] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3988 source_master_ns=93260473605 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4126486
[2026-10-19 16:12:07.473] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4032 source_master_ns=94282151609 raw_phase_ns=22674 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4171832
[2026-10-19 16:12:08.518] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4077 source_master_ns=95327049569 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4217178
[2026-10-19 16:12:09.144] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3853990634 ntp_seconds=4001386329 ntp_fraction=618475290 arrival_ns=26943061582400
[2026-10-19 16:12:09.145] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3853990634
[2026-10-19 16:12:09.145] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:12:09.227] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3368638434 ntp_seconds=4001386329 ntp_fraction=974957576 arrival_ns=26943144440900
[2026-10-19 16:12:09.227] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3368638434
[2026-10-19 16:12:09.228] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:12:09.251] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=29718129 ntp_seconds=4001386329 ntp_fraction=1078036791 arrival_ns=26943168399100
[2026-10-19 16:12:09.251] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=29718129
[2026-10-19 16:12:09.251] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2870565155 ntp_seconds=4001386329 ntp_fraction=1078036791 arrival_ns=26943168437400
[2026-10-19 16:12:09.252] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2870565155
[2026-10-19 16:12:09.252] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:12:09.252] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:12:09.540] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4121 source_master_ns=96348727573 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4262524
[2026-10-19 16:12:10.561] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4165 source_master_ns=97370405578 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4307870
[2026-10-19 16:12:11.583] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4209 source_master_ns=98392083582 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4353216
[2026-10-19 16:12:12.628] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4254 source_master_ns=99436981541 raw_phase_ns=22674 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4398562
[2026-10-19 16:12:13.649] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4298 source_master_ns=100458659546 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4443908
[2026-10-19 16:12:14.164] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3853990634 ntp_seconds=4001386334 ntp_fraction=700079669 arrival_ns=26948080466200
[2026-10-19 16:12:14.164] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3853990634
[2026-10-19 16:12:14.164] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:12:14.253] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2870565155 ntp_seconds=4001386334 ntp_fraction=1086626725 arrival_ns=26948170024200
[2026-10-19 16:12:14.253] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2870565155
[2026-10-19 16:12:14.253] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:12:14.254] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=29718129 ntp_seconds=4001386334 ntp_fraction=1086626725 arrival_ns=26948170788100
[2026-10-19 16:12:14.254] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=29718129
[2026-10-19 16:12:14.254] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:12:14.255] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3368638434 ntp_seconds=4001386334 ntp_fraction=1090921693 arrival_ns=26948171799800
[2026-10-19 16:12:14.255] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3368638434
[2026-10-19 16:12:14.255] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:12:14.672] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4342 source_master_ns=101480337551 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4489254
[2026-10-19 16:12:15.716] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4387 source_master_ns=102525235510 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4534600
[2026-10-19 16:12:16.738] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4431 source_master_ns=103546913514 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4579946
[2026-10-19 16:12:17.759] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4475 source_master_ns=104568591519 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4625292
[2026-10-19 16:12:18.781] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4519 source_master_ns=105590269523 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4670638
[2026-10-19 16:12:19.166] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3853990634 ntp_seconds=4001386339 ntp_fraction=712964571 arrival_ns=26953083460000
[2026-10-19 16:12:19.167] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3853990634
[2026-10-19 16:12:19.167] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:12:19.256] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3368638434 ntp_seconds=4001386339 ntp_fraction=1099511627 arrival_ns=26953173115900
[2026-10-19 16:12:19.256] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=29718129 ntp_seconds=4001386339 ntp_fraction=1099511627 arrival_ns=26953173097300
[2026-10-19 16:12:19.256] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2870565155 ntp_seconds=4001386339 ntp_fraction=1099511627 arrival_ns=26953173073300
[2026-10-19 16:12:19.257] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2870565155
[2026-10-19 16:12:19.256] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=29718129
[2026-10-19 16:12:19.256] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3368638434
[2026-10-19 16:12:19.257] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:12:19.257] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:12:19.257] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:12:19.826] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4564 source_master_ns=106635167483 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4715984
[2026-10-19 16:12:20.850] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4608 source_master_ns=107656845487 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4761330
[2026-10-19 16:12:21.869] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4652 source_master_ns=108678523492 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4806676
[2026-10-19 16:12:22.914] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4697 source_master_ns=109723421451 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4852022
[2026-10-19 16:12:23.936] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4741 source_master_ns=110745099455 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4897368
[2026-10-19 16:12:24.182] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3853990634 ntp_seconds=4001386344 ntp_fraction=777389080 arrival_ns=26958098858000
[2026-10-19 16:12:24.182] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3853990634
[2026-10-19 16:12:24.182] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:12:24.263] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3368638434 ntp_seconds=4001386344 ntp_fraction=1129576398 arrival_ns=26958180157900
[2026-10-19 16:12:24.263] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3368638434
[2026-10-19 16:12:24.263] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:12:24.294] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2870565155 ntp_seconds=4001386344 ntp_fraction=1258425417 arrival_ns=26958210748600
[2026-10-19 16:12:24.294] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=29718129 ntp_seconds=4001386344 ntp_fraction=1258425417 arrival_ns=26958210760300
[2026-10-19 16:12:24.294] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=29718129
[2026-10-19 16:12:24.295] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:12:24.294] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2870565155
[2026-10-19 16:12:24.295] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:12:24.958] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4785 source_master_ns=111766777460 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4942714
[2026-10-19 16:12:25.980] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4829 source_master_ns=112788455464 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4988060
[2026-10-19 16:12:27.024] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4874 source_master_ns=113833353424 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5033406
[2026-10-19 16:12:28.046] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4918 source_master_ns=114855031428 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5078752
[2026-10-19 16:12:29.068] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4962 source_master_ns=115876709433 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5124098
[2026-10-19 16:12:29.195] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=3853990634 ntp_seconds=4001386349 ntp_fraction=837518622 arrival_ns=26963112713200
[2026-10-19 16:12:29.196] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=3853990634
[2026-10-19 16:12:29.196] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:12:29.286] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3368638434 ntp_seconds=4001386349 ntp_fraction=1228360646 arrival_ns=26963203720800
[2026-10-19 16:12:29.287] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3368638434
[2026-10-19 16:12:29.287] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:12:29.316] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2870565155 ntp_seconds=4001386349 ntp_fraction=1357209665 arrival_ns=26963233587000
[2026-10-19 16:12:29.316] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2870565155
[2026-10-19 16:12:29.317] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:12:29.317] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=29718129 ntp_seconds=4001386349 ntp_fraction=1361504632 arrival_ns=26963234225600
[2026-10-19 16:12:29.317] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=29718129
[2026-10-19 16:12:29.317] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:12:30.112] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=5007 source_master_ns=116921607392 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5169444
[2026-10-19 16:12:31.134] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=5051 source_master_ns=117943285396 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5214790
[2026-10-19 16:12:32.156] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=5095 source_master_ns=118964963401 raw_phase_ns=22675 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5260136
[2026-10-19 16:12:33.178] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=5139 source_master_ns=119986641405 raw_phase_ns=22674 filtered_phase_ns=22675 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5305482
[2026-10-19 16:12:33.339] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=26967256104100
[2026-10-19 16:12:33.339] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=3 generation=2 invalidated_generation=1 locked=0
[2026-10-19 16:12:33.340] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=50 group=composition-r270.source.2 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=48 video_raw_generation=0 video_ssrc=3368638434 audio_raw_generation=0 audio_ssrc=3853990634
[2026-10-19 16:12:33.340] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=26967256866300
[2026-10-19 16:12:33.340] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=3 generation=2 invalidated_generation=1 locked=0
[2026-10-19 16:12:33.340] [info] [graph][runtime.node] av_reacquisition group=composition-r270.source.2 phase=purging old=1 next=2 transition=1
[2026-10-19 16:12:33.340] [info] [graph][runtime.node] rtp_ingress_purge node=46 transition=1 cutoff=0
[2026-10-19 16:12:33.340] [info] [graph][runtime.node] rtp_binder_purge node=50 transition=1 cutoff=3625
[2026-10-19 16:12:33.340] [info] [graph][runtime.node] rtp_ingress_purge node=47 transition=1 cutoff=0
[2026-10-19 16:12:33.340] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=51 group=composition-r270.source.2 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=48 video_raw_generation=0 video_ssrc=3368638434 audio_raw_generation=0 audio_ssrc=3853990634
[2026-10-19 16:12:33.342] [info] [graph][runtime.node] rtp_binder_purge node=51 transition=1 cutoff=5193
[2026-10-19 16:12:33.341] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=26967258542100
[2026-10-19 16:12:33.343] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=3 generation=2 invalidated_generation=1 locked=0
[2026-10-19 16:12:33.341] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=26967258094900
[2026-10-19 16:12:33.343] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=11 group=composition-r270.source.0 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=24 video_raw_generation=0 video_ssrc=29718129
[2026-10-19 16:12:33.343] [info] [graph][runtime.node] av_reacquisition group=composition-r270.source.0 phase=purging old=1 next=2 transition=1
[2026-10-19 16:12:33.343] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=3 generation=2 invalidated_generation=1 locked=0
[2026-10-19 16:12:33.345] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=30 group=composition-r270.source.1 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=24 video_raw_generation=0 video_ssrc=2870565155
[2026-10-19 16:12:33.345] [info] [graph][runtime.node] av_reacquisition group=composition-r270.source.1 phase=purging old=1 next=2 transition=1
[2026-10-19 16:12:33.347] [info] [graph][runtime.node] av_reacquisition group=composition-r270.source.2 event=purge_completed purge_ack=complete continuation=acquiring ack_count=2 old=1 next=2 transition=1
[2026-10-19 16:12:33.355] [info] [graph][runtime.node] rtp_ingress_purge node=8 transition=1 cutoff=0
[2026-10-19 16:12:33.355] [info] [graph][runtime.node] rtp_binder_purge node=11 transition=1 cutoff=3625
[2026-10-19 16:12:33.355] [info] [graph][runtime.node] rtp_ingress_purge node=27 transition=1 cutoff=0
[2026-10-19 16:12:33.355] [info] [graph][runtime.node] rtp_binder_purge node=30 transition=1 cutoff=3625
[2026-10-19 16:12:33.356] [info] [graph][runtime.node] av_reacquisition group=composition-r270.source.0 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=1 next=2 transition=1
[2026-10-19 16:12:33.356] [info] [graph][runtime.node] av_reacquisition group=composition-r270.source.1 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=1 next=2 transition=1
[2026-10-19 16:12:38.820] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=124976344700 present_generation=1 present_canonical_target_ns=124435736961 present_canonical_dispatch_ns=124435736961 present_target_master_ns=125096323829 present_dispatch_master_ns=125096323829
[2026-10-19 16:12:38.821] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=124977943300 present_generation=1 present_canonical_target_ns=124466666666 present_canonical_dispatch_ns=124466666666 present_target_master_ns=125127253534 present_dispatch_master_ns=125127253534
[2026-10-19 16:12:46.390] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=132546281300 present_generation=1 present_canonical_target_ns=132005442176 present_canonical_dispatch_ns=132005442176 present_target_master_ns=132666029044 present_dispatch_master_ns=132666029044
[2026-10-19 16:12:46.410] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=132566634400 present_generation=1 present_canonical_target_ns=132033333333 present_canonical_dispatch_ns=132033333333 present_target_master_ns=132693920201 present_dispatch_master_ns=132693920201
[2026-10-19 16:12:46.437] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=132593151700 present_generation=1 present_canonical_target_ns=132051882086 present_canonical_dispatch_ns=132051882086 present_target_master_ns=132712468954 present_dispatch_master_ns=132712468954
[2026-10-19 16:12:46.437] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=132593947600 present_generation=1 present_canonical_target_ns=132066666666 present_canonical_dispatch_ns=132066666666 present_target_master_ns=132727253534 present_dispatch_master_ns=132727253534
[2026-10-19 16:12:46.506] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=132662800200 present_generation=1 present_canonical_target_ns=132121541950 present_canonical_dispatch_ns=132121541950 present_target_master_ns=132782128818 present_dispatch_master_ns=132782128818
[2026-10-19 16:12:46.509] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=132665559800 present_generation=1 present_canonical_target_ns=132133333333 present_canonical_dispatch_ns=132133333333 present_target_master_ns=132793920201 present_dispatch_master_ns=132793920201
[2026-10-19 16:12:46.529] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=132686109700 present_generation=1 present_canonical_target_ns=132144761904 present_canonical_dispatch_ns=132144761904 present_target_master_ns=132805348772 present_dispatch_master_ns=132805348772
[2026-10-19 16:12:46.537] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=132693501200 present_generation=1 present_canonical_target_ns=132166666666 present_canonical_dispatch_ns=132166666666 present_target_master_ns=132827253534 present_dispatch_master_ns=132827253534
[2026-10-19 16:13:22.056] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=168212231300 present_generation=1 present_canonical_target_ns=167671292517 present_canonical_dispatch_ns=167671292517 present_target_master_ns=168331879385 present_dispatch_master_ns=168331879385
[2026-10-19 16:13:22.063] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=168219738400 present_generation=1 present_canonical_target_ns=167700000000 present_canonical_dispatch_ns=167700000000 present_target_master_ns=168360586868 present_dispatch_master_ns=168360586868
[2026-10-19 16:14:08.519] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=214675505300 present_generation=1 present_canonical_target_ns=214134421768 present_canonical_dispatch_ns=214134421768 present_target_master_ns=214795008636 present_dispatch_master_ns=214795008636
[2026-10-19 16:14:08.521] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=214677775100 present_generation=1 present_canonical_target_ns=214166666666 present_canonical_dispatch_ns=214166666666 present_target_master_ns=214827253534 present_dispatch_master_ns=214827253534
[2026-10-19 16:14:22.752] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=228908998900 present_generation=1 present_canonical_target_ns=228368253968 present_canonical_dispatch_ns=228368253968 present_target_master_ns=229028840836 present_dispatch_master_ns=229028840836
[2026-10-19 16:14:22.754] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=228910456200 present_generation=1 present_canonical_target_ns=228400000000 present_canonical_dispatch_ns=228400000000 present_target_master_ns=229060586868 present_dispatch_master_ns=229060586868
[2026-10-19 16:14:45.253] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=251410144100 present_generation=1 present_canonical_target_ns=250868390022 present_canonical_dispatch_ns=250868390022 present_target_master_ns=251528976890 present_dispatch_master_ns=251528976890
[2026-10-19 16:14:45.254] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=251411014300 present_generation=1 present_canonical_target_ns=250900000000 present_canonical_dispatch_ns=250900000000 present_target_master_ns=251560586868 present_dispatch_master_ns=251560586868
[2026-10-19 16:14:53.410] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=259566966600 present_generation=1 present_canonical_target_ns=259018594104 present_canonical_dispatch_ns=259018594104 present_target_master_ns=259679180972 present_dispatch_master_ns=259679180972
[2026-10-19 16:14:53.412] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=259568178900 present_generation=1 present_canonical_target_ns=259033333333 present_canonical_dispatch_ns=259033333333 present_target_master_ns=259693920201 present_dispatch_master_ns=259693920201
[2026-10-19 16:15:21.986] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=288143149400 present_generation=1 present_canonical_target_ns=287602358276 present_canonical_dispatch_ns=287602358276 present_target_master_ns=288262945144 present_dispatch_master_ns=288262945144
[2026-10-19 16:15:21.987] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=288143734600 present_generation=1 present_canonical_target_ns=287633333333 present_canonical_dispatch_ns=287633333333 present_target_master_ns=288293920201 present_dispatch_master_ns=288293920201
[2026-10-19 16:16:21.961] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3923361208 ntp_seconds=4001386581 ntp_fraction=4123168604 arrival_ns=27195878176800
[2026-10-19 16:16:21.961] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3923361208
[2026-10-19 16:16:21.961] [info] [graph][runtime.node] rtp_binder_boundary stage=deadline_established node=51 group=composition-r270.source.2 buffered=0 phase=2 preserves_output=1 raw_ssrc=3555849450 raw_generation=1 raw_sequence=5195 old_generation=1 next_generation=2 transition_sequence=1 published_generation=1
[2026-10-19 16:16:21.961] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=0 generation=2 invalidated_generation=1 locked=0
[2026-10-19 16:16:21.962] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3555849450 ntp_seconds=4001386581 ntp_fraction=4123168604 arrival_ns=27195878233900
[2026-10-19 16:16:21.962] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3555849450
[2026-10-19 16:16:21.962] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-19 16:16:22.505] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=1 generation=2
[2026-10-19 16:16:23.009] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1 source_master_ns=349265519620 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-10-19 16:16:23.009] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3 source_master_ns=349311959529 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=45346
[2026-10-19 16:16:23.014] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-10-19 16:16:23.566] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=48 source_master_ns=350356857488 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=90692
[2026-10-19 16:16:24.589] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=92 source_master_ns=351378535493 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=136038
[2026-10-19 16:16:25.609] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=136 source_master_ns=352400213497 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=181384
[2026-10-19 16:16:26.654] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=181 source_master_ns=353445111456 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=226730
[2026-10-19 16:16:26.966] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3923361208 ntp_seconds=4001386586 ntp_fraction=4148938407 arrival_ns=27200883149900
[2026-10-19 16:16:26.966] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3923361208
[2026-10-19 16:16:26.967] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-19 16:16:26.967] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3555849450 ntp_seconds=4001386586 ntp_fraction=4144643440 arrival_ns=27200883005500
[2026-10-19 16:16:26.967] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3555849450
[2026-10-19 16:16:26.967] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-19 16:16:27.676] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=225 source_master_ns=354466789461 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=272076
[2026-10-19 16:16:28.699] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=269 source_master_ns=355488467465 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=317422
[2026-10-19 16:16:29.719] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=313 source_master_ns=356510145470 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=362768
[2026-10-19 16:16:30.764] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=358 source_master_ns=357555043429 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=408114
[2026-10-19 16:16:31.786] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=402 source_master_ns=358576721434 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=453460
[2026-10-19 16:16:31.970] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3923361208 ntp_seconds=4001386591 ntp_fraction=4166118277 arrival_ns=27205887109700
[2026-10-19 16:16:31.970] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3923361208
[2026-10-19 16:16:31.970] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-19 16:16:31.984] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3555849450 ntp_seconds=4001386591 ntp_fraction=4226247819 arrival_ns=27205901137500
[2026-10-19 16:16:31.984] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3555849450
[2026-10-19 16:16:31.984] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-19 16:16:32.808] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=446 source_master_ns=359598399438 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=498806
[2026-10-19 16:16:33.852] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=491 source_master_ns=360643297397 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=544152
[2026-10-19 16:16:34.874] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=535 source_master_ns=361664975402 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=589498
[2026-10-19 16:16:35.896] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=579 source_master_ns=362686653407 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=634844
[2026-10-19 16:16:36.917] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=623 source_master_ns=363708331411 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=680190
[2026-10-19 16:16:36.992] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3555849450 ntp_seconds=4001386596 ntp_fraction=4260607557 arrival_ns=27210909196300
[2026-10-19 16:16:36.992] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3555849450
[2026-10-19 16:16:36.992] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-19 16:16:37.005] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3923361208 ntp_seconds=4001386597 ntp_fraction=21474836 arrival_ns=27210922232700
[2026-10-19 16:16:37.006] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3923361208
[2026-10-19 16:16:37.006] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-19 16:16:37.962] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=668 source_master_ns=364753229370 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=725536
[2026-10-19 16:16:38.984] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=712 source_master_ns=365774907375 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=770882
[2026-10-19 16:16:40.006] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=756 source_master_ns=366796585379 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=816228
[2026-10-19 16:16:41.050] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=801 source_master_ns=367841483339 raw_phase_ns=15874 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=861574
[2026-10-19 16:16:41.995] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=3555849450 ntp_seconds=4001386601 ntp_fraction=4273492459 arrival_ns=27215912298600
[2026-10-19 16:16:41.995] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=3555849450
[2026-10-19 16:16:41.995] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-19 16:16:42.037] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=3923361208 ntp_seconds=4001386602 ntp_fraction=154618822 arrival_ns=27215953787100
[2026-10-19 16:16:42.037] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=3923361208
[2026-10-19 16:16:42.037] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-19 16:16:42.074] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=845 source_master_ns=368863161343 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=906920
[2026-10-19 16:16:43.097] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=889 source_master_ns=369884839348 raw_phase_ns=15873 filtered_phase_ns=15873 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=952266
[2026-10-19 16:16:43.480] [info] [graph][runtime.node] protocol_batch_trace stage=materialization_failed generation=1 attempts=8 consumed_datagrams=0 remaining_datagrams=1 attempted_master_ns=369636867600 mux_dequeued_master_ns=369552844900 first_dequeued_master_ns=369554089800 last_wire_published_master_ns=369554086200 previous_attempt_started_master_ns=369627084900 previous_attempt_completed_master_ns=369627093100 last_attempt_completed_master_ns=369636893300 last_blocked_started_master_ns=369627084900 last_blocked_completed_master_ns=369627093100 blocked_attempts=7 last_blocked_code=WouldBlock last_blocked_native=0 last_blocked_original_message_bytes=294 last_blocked_message_truncated=0 last_blocked_message="wire global sequence reservation exceeds planner backlog capacity serial=1 count=0 bytes=0 request_count=1 request_bytes=228 used_count=9 used_bytes=11640 limit_count=2155 limit_bytes=2920356 generation=1 projected_sequence=124949 head_id=39885 head_first=124899 head_count=50 head_committed=41" remaining_front_release_master_ns=369560594248 remaining_front_deadline_master_ns=369627260916 original_error="InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=369560594248 materialized_ns=369636867600 protocol_deadline_ns=369627260916 deadline_before_release=0 materialized_after_deadline=1"
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] process.failed node=80 name=MediaMpegTsDatagramMaterializerNode kind=MpegTsDatagramMaterializer inputs=[channel=118 edge=118 stream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=120 edge=120 stream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=121 edge=121 stream=Metadata edge_kind=Unknown payload=Unknown queued=9] outputs=[channel=122 edge=122 stream=Metadata edge_kind=Unknown payload=Unknown queued=0] error=InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=369560594248 materialized_ns=369636867600 protocol_deadline_ns=369627260916 deadline_before_release=0 materialized_after_deadline=1
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.failed node=80 kind=MpegTsDatagramMaterializer name=MPEG-TS wire protocol materializer error=InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=369560594248 materialized_ns=369636867600 protocol_deadline_ns=369627260916 deadline_before_release=0 materialized_after_deadline=1
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=27 available=1 cpu_ns=265625000 process_calls=105767
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=7 available=1 cpu_ns=1875000000 process_calls=55101
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=9 available=1 cpu_ns=0 process_calls=83
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=29 available=1 cpu_ns=0 process_calls=108
[CLI] [2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=49 available=1 cpu_ns=0 process_calls=283
runtime report: queued=30, peakQueued=400, threads=82, processThreads=131, workers=76, workerProgress=529050, workerProcessCalls=1942909, workerWaits=1413852, workerWakeups=1385674, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=1169, logicalProcessors=22, averageSystemMachineCpuPercent=8.179679, averageProcessMachineCpuPercent=1.436555, peakProcessMachineCpuPercent=4.988662, averageProcessSingleCoreCpuPercent=31.604201, peakProcessSingleCoreCpuPercent=109.750567, initialWorkingSetBytes=189399040, workingSetBytes=249040896, peakWorkingSetBytes=251969536, totalPushed=475226, totalPopped=475172, droppedBuffers=0, encodedPacketsPushed=123774, encodedPacketsPopped=123773, backpressureItems=123, graphPayloadCurrentBytes=370489, graphPayloadHighWaterBytes=4975437, graphPayloadCurrentObjects=54, graphPayloadHighWaterObjects=205, graphPayloadReservations=212871, graphPayloadReleases=212817, graphPayloadPressureFailures=0, lastInputReceivedAtNs=27217386637000[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=11 available=1 cpu_ns=140625000 process_calls=10804

[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=12 available=1 cpu_ns=0 process_calls=83
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=80 available=1 cpu_ns=3375000000 process_calls=139842
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=30 available=1 cpu_ns=46875000 process_calls=10801
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=31 available=1 cpu_ns=0 process_calls=83
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=50 available=1 cpu_ns=156250000 process_calls=12735
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=53 available=1 cpu_ns=0 process_calls=281
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=14 available=1 cpu_ns=109375000 process_calls=10853
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=17 available=1 cpu_ns=0 process_calls=82
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=57 available=1 cpu_ns=281250000 process_calls=18345
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=16 available=1 cpu_ns=187500000 process_calls=10893
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=58 available=1 cpu_ns=250000000 process_calls=31152
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=35 available=1 cpu_ns=46875000 process_calls=10899
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=37 available=1 cpu_ns=125000000 process_calls=10769
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=18 available=1 cpu_ns=46875000 process_calls=10766
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=60 available=1 cpu_ns=296875000 process_calls=30645
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=39 available=1 cpu_ns=109375000 process_calls=10830
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=62 available=1 cpu_ns=359375000 process_calls=30842
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=22 available=1 cpu_ns=1218750000 process_calls=10040
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=43 available=1 cpu_ns=93750000 process_calls=11111
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=66 available=1 cpu_ns=156250000 process_calls=13053
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=72 available=1 cpu_ns=203125000 process_calls=19027
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=26 available=1 cpu_ns=1468750000 process_calls=12772
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=68 available=1 cpu_ns=1093750000 process_calls=15143
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=45 available=1 cpu_ns=1109375000 process_calls=12915
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=73 available=1 cpu_ns=515625000 process_calls=30194
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=2 available=1 cpu_ns=5718750000 process_calls=33224
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=81 available=1 cpu_ns=0 process_calls=5
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=82 available=1 cpu_ns=9812500000 process_calls=79347
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=46 available=1 cpu_ns=578125000 process_calls=132874
[2026-10-19 16:16:43.486] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=39890 committed_datagrams=124948 committed_payload_bytes=126449216 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=80004 pacing_reserved=124948 pacing_cancelled=0 pacing_submitted=124948 pacing_maximum_submit_lateness_ns=37449266 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=80004 backlog_max_residence_ns=82816900 last_materialized_sequence=124948 last_scheduled_sequence=124948 last_submitted_sequence=124948 last_committed_sequence=124948 shared_scope_charged_datagrams=124948 shared_scope_charged_wire_bytes=129947760 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2155 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2155 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=379902 endpoint_1_target_effective_socket_bytes=189951 endpoint_1_api_requested_socket_bytes=189951 endpoint_1_effective_socket_bytes=189951 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=124858 endpoint_1_committed_payload_bytes=126443096 endpoint_2_target_effective_socket_bytes=189951 endpoint_2_api_requested_socket_bytes=189951 endpoint_2_effective_socket_bytes=189951 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=90 endpoint_2_committed_payload_bytes=6120
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=44 available=1 cpu_ns=250000000 process_calls=19673
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=47 available=1 cpu_ns=484375000 process_calls=133062
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=28 available=1 cpu_ns=15625000 process_calls=82
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=48 available=1 cpu_ns=15625000 process_calls=189
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=10 available=1 cpu_ns=0 process_calls=109
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=52 available=1 cpu_ns=0 process_calls=189
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=51 available=1 cpu_ns=156250000 process_calls=18363
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=13 available=1 cpu_ns=0 process_calls=108
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=32 available=1 cpu_ns=0 process_calls=107
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=33 available=1 cpu_ns=62500000 process_calls=10846
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=36 available=1 cpu_ns=0 process_calls=82
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=55 available=1 cpu_ns=156250000 process_calls=18475
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=54 available=1 cpu_ns=93750000 process_calls=12819
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=59 available=1 cpu_ns=0 process_calls=182
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=34 available=1 cpu_ns=125000000 process_calls=10810
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=15 available=1 cpu_ns=62500000 process_calls=10806
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=56 available=1 cpu_ns=140625000 process_calls=12786
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=19 available=1 cpu_ns=140625000 process_calls=10781
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=38 available=1 cpu_ns=125000000 process_calls=10800
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=61 available=1 cpu_ns=421875000 process_calls=30602
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=20 available=1 cpu_ns=171875000 process_calls=10857
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=41 available=1 cpu_ns=1328125000 process_calls=10044
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=64 available=1 cpu_ns=1593750000 process_calls=18096
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=70 available=1 cpu_ns=500000000 process_calls=27400
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=42 available=1 cpu_ns=109375000 process_calls=10525
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=23 available=1 cpu_ns=78125000 process_calls=10553
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=65 available=1 cpu_ns=109375000 process_calls=12458
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=24 available=1 cpu_ns=46875000 process_calls=11126
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=71 available=1 cpu_ns=187500000 process_calls=19454
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=25 available=1 cpu_ns=265625000 process_calls=19650
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=6 available=1 cpu_ns=4937500000 process_calls=57611
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=67 available=1 cpu_ns=281250000 process_calls=21478
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=78 available=1 cpu_ns=0 process_calls=5
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=4 available=1 cpu_ns=4984375000 process_calls=49455
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=5 available=1 cpu_ns=484375000 process_calls=47700
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=74 available=1 cpu_ns=1109375000 process_calls=81613
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=77 available=1 cpu_ns=0 process_calls=3
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=79 available=1 cpu_ns=437500000 process_calls=80410
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=75 available=1 cpu_ns=500000000 process_calls=80744
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=76 available=1 cpu_ns=2265625000 process_calls=103706
[2026-10-19 16:16:43.481] [info] [graph][runtime.node] worker.thread_cpu node=8 available=1 cpu_ns=359375000 process_calls=87391
[2026-10-19 16:16:43.747] [info] [graph][runtime.lifecycle] node_abort_payload node=82 before_objects=53 after_objects=53 before_bytes=362297 after_bytes=362297 before_reservations=212871 after_reservations=212871 before_releases=212818 after_releases=212818
[2026-10-19 16:16:43.748] [info] [graph][runtime.node] mpegts_wire_materializer stage=aborted maximum_materialized_after_release_ns=56245342 worst_materialized_at_ns=47521140600 worst_release_ns=47464895258 worst_deadline_ns=47564895258 worst_global_sequence=33483
[2026-10-19 16:16:43.748] [info] [graph][runtime.lifecycle] node_abort_payload node=80 before_objects=53 after_objects=53 before_bytes=362297 after_bytes=362297 before_reservations=212871 after_reservations=212871 before_releases=212818 after_releases=212818
[2026-10-19 16:16:43.748] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 datagrams=0 wire_bytes=0 immediate_deadlines=0 deferred_deadlines=0 planned_wait_ns=0 late_datagrams=0 maximum_lateness_ns=0 pending_bytes=0 peak_pending_bytes=92872 pressure_failures=0 access_units=26969 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=4631031 maximum_mux_input_after_emit_ns=21642466 maximum_mux_input_after_dispatch_ns=-78357534 worst_mux_input_ready_ns=253448896000 worst_mux_input_emit_ns=253427253534 worst_mux_input_dispatch_ns=253527253534 maximum_protocol_batch_produced_after_release_ns=21664608 worst_protocol_batch_produced_ns=253448923200 worst_protocol_batch_release_ns=253427258592 worst_protocol_batch_deadline_ns=253527253534 worst_protocol_batch_datagrams=1 exit_reason=aborted
[2026-10-19 16:16:43.749] [info] [graph][runtime.lifecycle] node_abort_payload node=76 before_objects=53 after_objects=53 before_bytes=362297 after_bytes=362297 before_reservations=212871 after_reservations=212871 before_releases=212818 after_releases=212818
[2026-10-19 16:16:43.749] [info] [graph][runtime.lifecycle] node_abort_payload node=79 before_objects=53 after_objects=53 before_bytes=362297 after_bytes=362297 before_reservations=212871 after_reservations=212871 before_releases=212818 after_releases=212818
[2026-10-19 16:16:43.749] [info] [graph][runtime.lifecycle] node_abort_payload node=75 before_objects=53 after_objects=53 before_bytes=362297 after_bytes=362297 before_reservations=212871 after_reservations=212871 before_releases=212818 after_releases=212818
[2026-10-19 16:16:43.749] [info] [graph][runtime.node] audio_stage_observation node=74 stage=scheduler_input count=15896 last_generation=1 last_begin=16276480 last_end=16277504 last_rate=44100 last_steady_ns=27217387664400 maximum_inter_event_gap_ns=52798500 gap_begin_generation=1 gap_begin_begin=11420672 gap_begin_end=11421696 gap_begin_rate=44100 gap_begin_steady_ns=27107274210300 gap_end_generation=1 gap_end_begin=11421696 gap_end_end=11422720 gap_end_rate=44100 gap_end_steady_ns=27107327008800
[2026-10-19 16:16:43.750] [info] [graph][runtime.node] av_scheduler_correlated_wait generation=1 canonical_sequence=7771 missing=audio begin_steady_ns=27107274229100 end_steady_ns=27107327011300 observed_wait_ns=52782200 held_dispatch_master_ns=259660586868 begin_master_ns=259513449500 end_master_ns=259566231000 dispatch_remaining_at_end_ns=94355868 resumed_dispatch_master_ns=259655961017
[2026-10-19 16:16:43.750] [info] [graph][runtime.lifecycle] node_abort_payload node=74 before_objects=53 after_objects=52 before_bytes=362297 after_bytes=324107 before_reservations=212871 after_reservations=212871 before_releases=212818 after_releases=212819
[2026-10-19 16:16:43.750] [info] [graph][runtime.node] audio_stage_observation node=5 stage=canonicalizer_input count=15896 last_generation=1 last_begin=16276480 last_end=16277504 last_rate=44100 last_steady_ns=27217387636900 maximum_inter_event_gap_ns=52785200 gap_begin_generation=1 gap_begin_begin=11420672 gap_begin_end=11421696 gap_begin_rate=44100 gap_begin_steady_ns=27107274183700 gap_end_generation=1 gap_end_begin=11421696 gap_end_end=11422720 gap_end_rate=44100 gap_end_steady_ns=27107326968900
[2026-10-19 16:16:43.750] [info] [graph][runtime.node] audio_stage_observation node=5 stage=canonicalizer_committed count=15896 last_generation=1 last_begin=16276480 last_end=16277504 last_rate=44100 last_steady_ns=27217387643700 maximum_inter_event_gap_ns=52798500 gap_begin_generation=1 gap_begin_begin=11420672 gap_begin_end=11421696 gap_begin_rate=44100 gap_begin_steady_ns=27107274192100 gap_end_generation=1 gap_end_begin=11421696 gap_end_end=11422720 gap_end_rate=44100 gap_end_steady_ns=27107326990600
[2026-10-19 16:16:43.750] [info] [graph][runtime.lifecycle] node_abort_payload node=5 before_objects=52 after_objects=52 before_bytes=324107 after_bytes=324107 before_reservations=212871 after_reservations=212871 before_releases=212819 after_releases=212819
[2026-10-19 16:16:43.751] [info] [graph][runtime.lifecycle] node_abort_payload node=77 before_objects=52 after_objects=52 before_bytes=324107 after_bytes=324107 before_reservations=212871 after_reservations=212871 before_releases=212819 after_releases=212819
[2026-10-19 16:16:43.752] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_input count=15898 last_generation=1 last_begin=16278528 last_end=16279552 last_rate=44100 last_steady_ns=27217387265400 maximum_inter_event_gap_ns=52714800 gap_begin_generation=1 gap_begin_begin=11422720 gap_begin_end=11423744 gap_begin_rate=44100 gap_begin_steady_ns=27107274091700 gap_end_generation=1 gap_end_begin=11423744 gap_end_end=11424768 gap_end_rate=44100 gap_end_steady_ns=27107326806500
[2026-10-19 16:16:43.752] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_submitted count=15898 last_generation=1 last_begin=16278528 last_end=16279552 last_rate=44100 last_steady_ns=27217387616200 maximum_inter_event_gap_ns=52765200 gap_begin_generation=1 gap_begin_begin=11422720 gap_begin_end=11423744 gap_begin_rate=44100 gap_begin_steady_ns=27107274163600 gap_end_generation=1 gap_end_begin=11423744 gap_end_end=11424768 gap_end_rate=44100 gap_end_steady_ns=27107326928800
[2026-10-19 16:16:43.752] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_packet count=15896 last_generation=1 last_begin=16276480 last_end=16277504 last_rate=44100 last_steady_ns=27217387623400 maximum_inter_event_gap_ns=52768100 gap_begin_generation=1 gap_begin_begin=11420672 gap_begin_end=11421696 gap_begin_rate=44100 gap_begin_steady_ns=27107274168700 gap_end_generation=1 gap_end_begin=11421696 gap_end_end=11422720 gap_end_rate=44100 gap_end_steady_ns=27107326936800
[2026-10-19 16:16:43.752] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_committed count=15896 last_generation=1 last_begin=16276480 last_end=16277504 last_rate=44100 last_steady_ns=27217387629000 maximum_inter_event_gap_ns=52774600 gap_begin_generation=1 gap_begin_begin=11420672 gap_begin_end=11421696 gap_begin_rate=44100 gap_begin_steady_ns=27107274175500 gap_end_generation=1 gap_end_begin=11421696 gap_end_end=11422720 gap_end_rate=44100 gap_end_steady_ns=27107326950100
[2026-10-19 16:16:43.753] [info] [graph][runtime.lifecycle] codec_owner_release node=4 kind=AudioEncode owner_references=4
[2026-10-19 16:16:43.753] [info] [graph][runtime.lifecycle] node_abort_payload node=4 before_objects=52 after_objects=52 before_bytes=324107 after_bytes=324107 before_reservations=212871 after_reservations=212871 before_releases=212819 after_releases=212819
[2026-10-19 16:16:43.753] [info] [graph][runtime.lifecycle] node_abort_payload node=78 before_objects=52 after_objects=52 before_bytes=324107 after_bytes=324107 before_reservations=212871 after_reservations=212871 before_releases=212819 after_releases=212819
[2026-10-19 16:16:43.753] [info] [graph][runtime.lifecycle] node_abort_payload node=81 before_objects=52 after_objects=52 before_bytes=324107 after_bytes=324107 before_reservations=212871 after_reservations=212871 before_releases=212819 after_releases=212819
[2026-10-19 16:16:43.753] [info] [graph][runtime.node] audio_stage_observation node=7 stage=aggregate_built count=15898 last_generation=1 last_begin=16278528 last_end=16279552 last_rate=44100 last_steady_ns=27217387230600 maximum_inter_event_gap_ns=52700200 gap_begin_generation=1 gap_begin_begin=11422720 gap_begin_end=11423744 gap_begin_rate=44100 gap_begin_steady_ns=27107274052100 gap_end_generation=1 gap_end_begin=11423744 gap_end_end=11424768 gap_end_rate=44100 gap_end_steady_ns=27107326752300
[2026-10-19 16:16:43.753] [info] [graph][runtime.node] audio_stage_observation node=7 stage=aggregate_committed count=15898 last_generation=1 last_begin=16278528 last_end=16279552 last_rate=44100 last_steady_ns=27217387655900 maximum_inter_event_gap_ns=52565600 gap_begin_generation=1 gap_begin_begin=11422720 gap_begin_end=11423744 gap_begin_rate=44100 gap_begin_steady_ns=27107274202500 gap_end_generation=1 gap_end_begin=11423744 gap_end_end=11424768 gap_end_rate=44100 gap_end_steady_ns=27107326768100
[2026-10-19 16:16:43.753] [info] [graph][runtime.node] continuous_audio_aggregate.summary reason=abort output_group=composition-r270.output audio_commits=15898 real_samples=6150144 silence_samples=10129408
[2026-10-19 16:16:43.753] [info] [graph][runtime.lifecycle] node_abort_payload node=7 before_objects=52 after_objects=46 before_bytes=324107 after_bytes=283147 before_reservations=212871 after_reservations=212871 before_releases=212819 after_releases=212825
[2026-10-19 16:16:43.753] [info] [graph][runtime.lifecycle] codec_owner_release node=2 kind=VideoEncode owner_references=4
[2026-10-19 16:16:43.753] [info] [graph][runtime.node] video_encoder_execution_observation node=2 count=11075 maximum_call_ns=38797400 maximum_begin_steady_ns=26980288511200 maximum_end_steady_ns=26980327308600 maximum_encoder_pts=3961 maximum_native_result=0 maximum_between_submissions_ns=64623900 before_gap_begin_steady_ns=27107255088000 before_gap_end_steady_ns=27107260847600 before_gap_encoder_pts=7770 before_gap_native_result=0 after_gap_begin_steady_ns=27107325471500 after_gap_end_steady_ns=27107328902200 after_gap_encoder_pts=7771 after_gap_native_result=0
[2026-10-19 16:16:43.754] [info] [graph][runtime.lifecycle] node_abort_payload node=2 before_objects=46 after_objects=46 before_bytes=283147 after_bytes=283147 before_reservations=212871 after_reservations=212871 before_releases=212825 after_releases=212825
[2026-10-19 16:16:43.754] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=process count=57611 maximum_ns=21894500 begin_steady_ns=26901696457500 end_steady_ns=26901718352000 video_cursor=1604 audio_cursor=not_owned next_video_cursor=1604 next_audio_cursor=not_owned error=None return_state=waiting deadline_ns=53960586866 deadline_clock=master wake_policy=0 pending_at_entry=0 entered_onprocess=1
[2026-10-19 16:16:43.754] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=purge count=57611 maximum_ns=6688700 begin_steady_ns=26967257825700 end_steady_ns=26967264514400 video_cursor=3570 audio_cursor=not_owned next_video_cursor=3570 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 16:16:43.754] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=bind count=57611 maximum_ns=457200 begin_steady_ns=26890757153700 end_steady_ns=26890757610900 video_cursor=1276 audio_cursor=not_owned next_video_cursor=1276 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 16:16:43.754] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=consume count=57611 maximum_ns=21892100 begin_steady_ns=26901696458400 end_steady_ns=26901718350500 video_cursor=1604 audio_cursor=not_owned next_video_cursor=1604 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 16:16:43.754] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=activate count=16 maximum_ns=52800 begin_steady_ns=26848254699200 end_steady_ns=26848254752000 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 16:16:43.754] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=build_video count=11076 maximum_ns=20243200 begin_steady_ns=26945288405100 end_steady_ns=26945308648300 video_cursor=2911 audio_cursor=not_owned next_video_cursor=2911 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 16:16:43.754] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=canvas count=11076 maximum_ns=20209000 begin_steady_ns=26945288413500 end_steady_ns=26945308622500 video_cursor=2911 audio_cursor=not_owned next_video_cursor=2911 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 16:16:43.754] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=emit count=11076 maximum_ns=13313700 begin_steady_ns=27121559207800 end_steady_ns=27121572521500 video_cursor=8199 audio_cursor=not_owned next_video_cursor=8199 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 16:16:43.754] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=between_calls_previous_return count=57610 maximum_ns=77757400 begin_steady_ns=26848174549600 end_steady_ns=26848252307000 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=waiting deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-10-19 16:16:43.754] [info] [graph][runtime.node] continuous_aggregate.summary reason=abort output_group=composition-r270.output output_generation=1 video_commits=11075 source[0].group=composition-r270.source.0 source[0].real_frames=3570 source[0].black_frames=7505 source[0].contribution_generation=0 source[1].group=composition-r270.source.1 source[1].real_frames=3570 source[1].black_frames=7505 source[1].contribution_generation=0 source[2].group=composition-r270.source.2 source[2].real_frames=4184 source[2].black_frames=6891 source[2].contribution_generation=2
[2026-10-19 16:16:43.754] [info] [graph][runtime.node] cuda_canvas_call stage=context_push count=4189 operation=copy maximum_ns=336800 begin_steady_ns=27210922176800 end_steady_ns=27210922513600 native_result=0 thread_cpu_ns=0
[2026-10-19 16:16:43.755] [info] [graph][runtime.node] cuda_canvas_call stage=source_wait count=15518 operation=copy maximum_ns=408400 begin_steady_ns=26848269270100 end_steady_ns=26848269678500 native_result=0 thread_cpu_ns=0
[2026-10-19 16:16:43.755] [info] [graph][runtime.node] cuda_canvas_call stage=plane_copy count=31038 operation=copy maximum_ns=12928800 begin_steady_ns=26848255681900 end_steady_ns=26848268610700 native_result=0 thread_cpu_ns=0
[2026-10-19 16:16:43.755] [info] [graph][runtime.node] cuda_canvas_call stage=destination_wait count=2 operation=upload maximum_ns=95400 begin_steady_ns=26848108514400 end_steady_ns=26848108609800 native_result=0 thread_cpu_ns=0
[2026-10-19 16:16:43.755] [info] [graph][runtime.node] cuda_canvas_call stage=context_pop count=4189 operation=copy maximum_ns=1483900 begin_steady_ns=26866723504900 end_steady_ns=26866724988800 native_result=0 thread_cpu_ns=0
[2026-10-19 16:16:43.755] [info] [graph][runtime.node] cuda_canvas_call stage=batch_completion count=4187 operation=copy maximum_ns=20094800 begin_steady_ns=26945288509500 end_steady_ns=26945308604300 native_result=0 thread_cpu_ns=0
[2026-10-19 16:16:43.755] [info] [graph][runtime.node] canvas_compose_correlated scope=attempt begin_steady_ns=26945288414300 end_steady_ns=26945308621700 wall_ns=20207400 thread_cpu_ns=0 surface_count=1 surface_total_ns=600 surface_maximum_ns=600 surface_maximum_tile=none surface_maximum_result=unavailable validate_count=3 validate_total_ns=1700 validate_maximum_ns=1000 validate_maximum_tile=0 validate_maximum_result=0 black_count=1 black_total_ns=42200 black_maximum_ns=42200 black_maximum_tile=none black_maximum_result=0 tile_count=3 tile_total_ns=45000 tile_maximum_ns=19500 tile_maximum_tile=0 tile_maximum_result=0 publish_count=1 publish_total_ns=8200 publish_maximum_ns=8200 publish_maximum_tile=none publish_maximum_result=0 context_push_count=1 context_push_total_ns=1400 context_push_maximum_ns=1400 context_push_maximum_tile=none context_push_maximum_result=0 source_wait_count=4 source_wait_total_ns=2800 source_wait_maximum_ns=2000 source_wait_maximum_tile=0 source_wait_maximum_result=0 plane_copy_count=8 plane_copy_total_ns=73400 plane_copy_maximum_ns=34000 plane_copy_maximum_tile=none plane_copy_maximum_result=0 destination_wait_count=0 destination_wait_total_ns=0 destination_wait_maximum_ns=0 destination_wait_maximum_tile=none destination_wait_maximum_result=unavailable context_pop_count=1 context_pop_total_ns=5100 context_pop_maximum_ns=5100 context_pop_maximum_tile=none context_pop_maximum_result=0 batch_completion_count=1 batch_completion_total_ns=20091600 batch_completion_maximum_ns=20091600 batch_completion_maximum_tile=none batch_completion_maximum_result=0 source_query_completed_count=0 source_query_ready_count=3 source_query_ready_maximum_ns=3700 source_query_ready_begin_steady_ns=26945288464200 source_query_ready_end_steady_ns=26945288467900 source_query_ready_tile=0 source_query_ready_operation=event_query source_query_ready_native=0 source_query_ready_restore_native=0 source_query_pending_count=0 source_query_unavailable_count=0 source_query_error_count=0
[2026-10-19 16:16:43.755] [info] [graph][runtime.lifecycle] node_abort_payload node=6 before_objects=46 after_objects=32 before_bytes=283147 after_bytes=283147 before_reservations=212871 after_reservations=212871 before_releases=212825 after_releases=212839
[2026-10-19 16:16:43.756] [info] [graph][runtime.lifecycle] node_abort_payload node=67 before_objects=32 after_objects=31 before_bytes=283147 after_bytes=283147 before_reservations=212871 after_reservations=212871 before_releases=212839 after_releases=212840
[2026-10-19 16:16:43.756] [info] [graph][runtime.lifecycle] node_abort_payload node=44 before_objects=31 after_objects=31 before_bytes=283147 after_bytes=283147 before_reservations=212871 after_reservations=212871 before_releases=212840 after_releases=212840
[2026-10-19 16:16:43.756] [info] [graph][runtime.lifecycle] node_abort_payload node=25 before_objects=31 after_objects=31 before_bytes=283147 after_bytes=283147 before_reservations=212871 after_reservations=212871 before_releases=212840 after_releases=212840
[2026-10-19 16:16:43.756] [info] [graph][runtime.lifecycle] codec_owner_release node=73 kind=AudioResample owner_references=1
[2026-10-19 16:16:43.756] [info] [graph][runtime.lifecycle] node_abort_payload node=73 before_objects=31 after_objects=29 before_bytes=283147 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212840 after_releases=212842
[2026-10-19 16:16:43.764] [info] [graph][runtime.lifecycle] node_abort_payload node=68 before_objects=29 after_objects=29 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212842 after_releases=212842
[2026-10-19 16:16:43.769] [info] [graph][runtime.lifecycle] node_abort_payload node=45 before_objects=29 after_objects=29 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212842 after_releases=212842
[2026-10-19 16:16:43.772] [info] [graph][runtime.lifecycle] node_abort_payload node=26 before_objects=29 after_objects=29 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212842 after_releases=212842
[2026-10-19 16:16:43.772] [info] [graph][runtime.lifecycle] node_abort_payload node=72 before_objects=29 after_objects=29 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212842 after_releases=212842
[2026-10-19 16:16:43.773] [info] [graph][runtime.lifecycle] node_abort_payload node=66 before_objects=29 after_objects=28 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212842 after_releases=212843
[2026-10-19 16:16:43.773] [info] [graph][runtime.lifecycle] node_abort_payload node=43 before_objects=28 after_objects=28 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212843 after_releases=212843
[2026-10-19 16:16:43.773] [info] [graph][runtime.lifecycle] node_abort_payload node=24 before_objects=28 after_objects=28 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212843 after_releases=212843
[2026-10-19 16:16:43.773] [info] [graph][runtime.lifecycle] node_abort_payload node=71 before_objects=28 after_objects=28 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212843 after_releases=212843
[2026-10-19 16:16:43.773] [info] [graph][runtime.lifecycle] node_abort_payload node=65 before_objects=28 after_objects=28 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212843 after_releases=212843
[2026-10-19 16:16:43.773] [info] [graph][runtime.lifecycle] node_abort_payload node=42 before_objects=28 after_objects=28 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212843 after_releases=212843
[2026-10-19 16:16:43.773] [info] [graph][runtime.lifecycle] node_abort_payload node=23 before_objects=28 after_objects=28 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212843 after_releases=212843
[2026-10-19 16:16:43.773] [info] [graph][runtime.lifecycle] codec_owner_release node=70 kind=AudioDecode owner_references=2
[2026-10-19 16:16:43.774] [info] [graph][runtime.lifecycle] node_abort_payload node=70 before_objects=28 after_objects=28 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212843 after_releases=212843
[2026-10-19 16:16:43.774] [info] [graph][runtime.lifecycle] codec_owner_release node=64 kind=VideoDecode owner_references=2
[2026-10-19 16:16:43.786] [info] [graph][runtime.lifecycle] node_abort_payload node=64 before_objects=28 after_objects=28 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212843 after_releases=212843
[2026-10-19 16:16:43.786] [info] [graph][runtime.lifecycle] codec_owner_release node=41 kind=VideoDecode owner_references=2
[2026-10-19 16:16:43.796] [info] [graph][runtime.lifecycle] node_abort_payload node=41 before_objects=28 after_objects=28 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212843 after_releases=212843
[2026-10-19 16:16:43.796] [info] [graph][runtime.lifecycle] codec_owner_release node=22 kind=VideoDecode owner_references=2
[2026-10-19 16:16:43.807] [info] [graph][runtime.lifecycle] node_abort_payload node=22 before_objects=28 after_objects=28 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212843 after_releases=212843
[2026-10-19 16:16:43.807] [info] [graph][runtime.lifecycle] node_abort_payload node=62 before_objects=28 after_objects=28 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212843 after_releases=212843
[2026-10-19 16:16:43.807] [info] [graph][runtime.lifecycle] node_abort_payload node=39 before_objects=28 after_objects=28 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212843 after_releases=212843
[2026-10-19 16:16:43.807] [info] [graph][runtime.lifecycle] node_abort_payload node=20 before_objects=28 after_objects=28 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212843 after_releases=212843
[2026-10-19 16:16:43.807] [info] [graph][runtime.lifecycle] node_abort_payload node=61 before_objects=28 after_objects=28 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212843 after_releases=212843
[2026-10-19 16:16:43.807] [info] [graph][runtime.lifecycle] node_abort_payload node=38 before_objects=28 after_objects=28 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212843 after_releases=212843
[2026-10-19 16:16:43.807] [info] [graph][runtime.lifecycle] node_abort_payload node=19 before_objects=28 after_objects=28 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212843 after_releases=212843
[2026-10-19 16:16:43.807] [info] [graph][runtime.lifecycle] node_abort_payload node=60 before_objects=28 after_objects=28 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212843 after_releases=212843
[2026-10-19 16:16:43.807] [info] [graph][runtime.lifecycle] node_abort_payload node=37 before_objects=28 after_objects=28 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212843 after_releases=212843
[2026-10-19 16:16:43.808] [info] [graph][runtime.lifecycle] node_abort_payload node=18 before_objects=28 after_objects=28 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212843 after_releases=212843
[2026-10-19 16:16:43.808] [info] [graph][runtime.lifecycle] node_abort_payload node=58 before_objects=28 after_objects=28 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212843 after_releases=212843
[2026-10-19 16:16:43.808] [info] [graph][runtime.lifecycle] node_abort_payload node=35 before_objects=28 after_objects=28 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212843 after_releases=212843
[2026-10-19 16:16:43.808] [info] [graph][runtime.lifecycle] node_abort_payload node=16 before_objects=28 after_objects=28 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212843 after_releases=212843
[2026-10-19 16:16:43.808] [info] [graph][runtime.lifecycle] node_abort_payload node=57 before_objects=28 after_objects=28 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212843 after_releases=212843
[2026-10-19 16:16:43.808] [info] [graph][runtime.lifecycle] node_abort_payload node=56 before_objects=28 after_objects=28 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212843 after_releases=212843
[2026-10-19 16:16:43.808] [info] [graph][runtime.lifecycle] node_abort_payload node=34 before_objects=28 after_objects=28 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212843 after_releases=212843
[2026-10-19 16:16:43.808] [info] [graph][runtime.lifecycle] node_abort_payload node=15 before_objects=28 after_objects=28 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212843 after_releases=212843
[2026-10-19 16:16:43.808] [info] [graph][runtime.lifecycle] node_abort_payload node=59 before_objects=28 after_objects=28 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212843 after_releases=212843
[2026-10-19 16:16:43.808] [info] [graph][runtime.lifecycle] node_abort_payload node=55 before_objects=28 after_objects=28 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212843 after_releases=212843
[2026-10-19 16:16:43.808] [info] [graph][runtime.lifecycle] node_abort_payload node=54 before_objects=28 after_objects=28 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212843 after_releases=212843
[2026-10-19 16:16:43.808] [info] [graph][runtime.lifecycle] node_abort_payload node=36 before_objects=28 after_objects=28 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212843 after_releases=212843
[2026-10-19 16:16:43.808] [info] [graph][runtime.lifecycle] node_abort_payload node=33 before_objects=28 after_objects=28 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212843 after_releases=212843
[2026-10-19 16:16:43.809] [info] [graph][runtime.lifecycle] node_abort_payload node=17 before_objects=28 after_objects=28 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212843 after_releases=212843
[2026-10-19 16:16:43.809] [info] [graph][runtime.lifecycle] node_abort_payload node=14 before_objects=28 after_objects=28 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212843 after_releases=212843
[2026-10-19 16:16:43.809] [info] [graph][runtime.lifecycle] node_abort_payload node=53 before_objects=28 after_objects=28 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212843 after_releases=212843
[2026-10-19 16:16:43.809] [info] [graph][runtime.lifecycle] node_abort_payload node=32 before_objects=28 after_objects=28 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212843 after_releases=212843
[2026-10-19 16:16:43.809] [info] [graph][runtime.lifecycle] node_abort_payload node=13 before_objects=28 after_objects=28 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212843 after_releases=212843
[2026-10-19 16:16:43.809] [info] [graph][runtime.lifecycle] node_abort_payload node=52 before_objects=28 after_objects=28 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212843 after_releases=212843
[2026-10-19 16:16:43.809] [info] [graph][runtime.lifecycle] node_abort_payload node=51 before_objects=28 after_objects=28 before_bytes=266763 after_bytes=266763 before_reservations=212871 after_reservations=212871 before_releases=212843 after_releases=212843
[2026-10-19 16:16:43.809] [info] [graph][runtime.lifecycle] node_abort_payload node=50 before_objects=28 after_objects=27 before_bytes=266763 after_bytes=153508 before_reservations=212871 after_reservations=212871 before_releases=212843 after_releases=212844
[2026-10-19 16:16:43.809] [info] [graph][runtime.lifecycle] node_abort_payload node=31 before_objects=27 after_objects=27 before_bytes=153508 after_bytes=153508 before_reservations=212871 after_reservations=212871 before_releases=212844 after_releases=212844
[2026-10-19 16:16:43.810] [info] [graph][runtime.lifecycle] node_abort_payload node=30 before_objects=27 after_objects=27 before_bytes=153508 after_bytes=153508 before_reservations=212871 after_reservations=212871 before_releases=212844 after_releases=212844
[2026-10-19 16:16:43.810] [info] [graph][runtime.lifecycle] node_abort_payload node=12 before_objects=27 after_objects=27 before_bytes=153508 after_bytes=153508 before_reservations=212871 after_reservations=212871 before_releases=212844 after_releases=212844
[2026-10-19 16:16:43.810] [info] [graph][runtime.lifecycle] node_abort_payload node=11 before_objects=27 after_objects=27 before_bytes=153508 after_bytes=153508 before_reservations=212871 after_reservations=212871 before_releases=212844 after_releases=212844
[2026-10-19 16:16:43.810] [info] [graph][runtime.lifecycle] node_abort_payload node=49 before_objects=27 after_objects=27 before_bytes=153508 after_bytes=153508 before_reservations=212871 after_reservations=212871 before_releases=212844 after_releases=212844
[2026-10-19 16:16:43.810] [info] [graph][runtime.lifecycle] node_abort_payload node=29 before_objects=27 after_objects=27 before_bytes=153508 after_bytes=153508 before_reservations=212871 after_reservations=212871 before_releases=212844 after_releases=212844
[2026-10-19 16:16:43.810] [info] [graph][runtime.lifecycle] node_abort_payload node=10 before_objects=27 after_objects=27 before_bytes=153508 after_bytes=153508 before_reservations=212871 after_reservations=212871 before_releases=212844 after_releases=212844
[2026-10-19 16:16:43.810] [info] [graph][runtime.lifecycle] node_abort_payload node=69 before_objects=27 after_objects=27 before_bytes=153508 after_bytes=153508 before_reservations=212871 after_reservations=212871 before_releases=212844 after_releases=212844
[2026-10-19 16:16:43.810] [info] [graph][runtime.lifecycle] node_abort_payload node=48 before_objects=27 after_objects=27 before_bytes=153508 after_bytes=153508 before_reservations=212871 after_reservations=212871 before_releases=212844 after_releases=212844
[2026-10-19 16:16:43.810] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-10-19 16:16:43.810] [info] [graph][runtime.lifecycle] node_abort_payload node=63 before_objects=27 after_objects=27 before_bytes=153508 after_bytes=153508 before_reservations=212871 after_reservations=212871 before_releases=212844 after_releases=212844
[2026-10-19 16:16:43.810] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-10-19 16:16:43.810] [info] [graph][runtime.lifecycle] node_abort_payload node=40 before_objects=27 after_objects=27 before_bytes=153508 after_bytes=153508 before_reservations=212871 after_reservations=212871 before_releases=212844 after_releases=212844
[2026-10-19 16:16:43.810] [info] [graph][runtime.lifecycle] node_abort_payload node=28 before_objects=27 after_objects=27 before_bytes=153508 after_bytes=153508 before_reservations=212871 after_reservations=212871 before_releases=212844 after_releases=212844
[2026-10-19 16:16:43.810] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-10-19 16:16:43.810] [info] [graph][runtime.lifecycle] node_abort_payload node=21 before_objects=27 after_objects=27 before_bytes=153508 after_bytes=153508 before_reservations=212871 after_reservations=212871 before_releases=212844 after_releases=212844
[2026-10-19 16:16:43.810] [info] [graph][runtime.lifecycle] node_abort_payload node=9 before_objects=27 after_objects=27 before_bytes=153508 after_bytes=153508 before_reservations=212871 after_reservations=212871 before_releases=212844 after_releases=212844
[2026-10-19 16:16:43.811] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=6207 completion_waits=6207 completed_datagrams=6195 drained_datagrams=97 would_block=113 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-19 16:16:43.811] [info] [graph][runtime.lifecycle] node_abort_payload node=47 before_objects=27 after_objects=27 before_bytes=153508 after_bytes=153508 before_reservations=212871 after_reservations=212871 before_releases=212844 after_releases=212844
[2026-10-19 16:16:43.811] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=88119 completion_waits=88119 completed_datagrams=126130 drained_datagrams=38121 would_block=113 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-19 16:16:43.811] [info] [graph][runtime.lifecycle] node_abort_payload node=46 before_objects=27 after_objects=27 before_bytes=153508 after_bytes=153508 before_reservations=212871 after_reservations=212871 before_releases=212844 after_releases=212844
[2026-10-19 16:16:43.812] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=82331 completion_waits=82331 completed_datagrams=106345 drained_datagrams=24139 would_block=124 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-19 16:16:43.812] [info] [graph][runtime.lifecycle] node_abort_payload node=27 before_objects=27 after_objects=27 before_bytes=153508 after_bytes=153508 before_reservations=212871 after_reservations=212871 before_releases=212844 after_releases=212844
[2026-10-19 16:16:43.813] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=72558 completion_waits=72558 completed_datagrams=106111 drained_datagrams=33674 would_block=124 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-19 16:16:43.814] [info] [graph][runtime.lifecycle] node_abort_payload node=8 before_objects=27 after_objects=27 before_bytes=153508 after_bytes=153508 before_reservations=212871 after_reservations=212871 before_releases=212844 after_releases=212844
[2026-10-19 16:16:43.815] [info] [graph][runtime.lifecycle] node_abort_payload node=3 before_objects=27 after_objects=27 before_bytes=153508 after_bytes=153508 before_reservations=212871 after_reservations=212871 before_releases=212844 after_releases=212844
[2026-10-19 16:16:43.815] [info] [graph][runtime.node] codec_resolver.release slot=encoder_parameters bufferReferences=1 hasCredit=0 creditReferences=0
[2026-10-19 16:16:43.815] [info] [graph][runtime.lifecycle] node_abort_payload node=1 before_objects=27 after_objects=27 before_bytes=153508 after_bytes=153508 before_reservations=212871 after_reservations=212871 before_releases=212844 after_releases=212844
~~~
### 标准错误
~~~text
[in @ 0000017f9c93b180] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000017f9c93b180] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 39298.054289
[in @ 0000017f9ca23400] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000017f9ca23400] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 45218.991011
[in @ 0000017f9cdbff80] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000017f9cdbff80] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 19785.105356
[in @ 0000017f9cb0b540] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000017fa694ea80] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000017fa694ea80] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 39298.054289
[in @ 0000017f9cb0b540] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 45218.991011
[in @ 0000017f9cb0c780] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000017f9cb0c780] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 19785.105356
[aac @ 0000017fa66c2600] Qavg: 41046.340
[aac @ 0000017fa66c2600] 2 frames left in the queue on closing

~~~
### Windows故障记录
~~~text

TimeCreated : 2026/10/19 16:16:48
Id          : 1001
Message     : 故障存储段 2124067525144082775，类型 4
              事件名称: APPCRASH
              响应: 不可用
              Cab ID: 0
              
              问题签名:
              P1: media_transcode_realtime_video_cli.exe
              P2: 0.0.0.0
              P3: 6ad5d061
              P4: avutil-60.dll
              P5: 60.33.101.0
              P6: 6a36a4a3
              P7: c0000005
              P8: 000000000002753d
              P9: 
              P10: 
              
              附加文件:
              \\?\C:\ProgramData\Microsoft\Windows\WER\Temp\WER.a1bea9b1-c4cf-4fd8-bcba-b4e922bdd512.tmp.mdmp
              \\?\C:\ProgramData\Microsoft\Windows\WER\Temp\WER.e4dc772c-ba82-42d6-85e3-b3f3cc0ddd9f.tmp.WERInternalMetadata.xml
              WPR_initiated_DiagTrackMiniLogger_OneTrace_User_Logger_20261016_1_EC_0_inject.etl
              \\?\C:\ProgramData\Microsoft\Windows\WER\Temp\WER.c05c4a91-e19a-4d79-ac49-b72fe0f1f847.tmp.etl
              WPR_initiated_DiagTrackMiniLogger_WPR System Collector_inject.etl
              \\?\C:\ProgramData\Microsoft\Windows\WER\Temp\WER.9f8b4d79-1fba-4803-9de9-89e3c1147442.tmp.etl
              \\?\C:\ProgramData\Microsoft\Windows\WER\Temp\WER.5723064e-33dd-4839-a673-7ab34449d073.tmp.csv
              \\?\C:\ProgramData\Microsoft\Windows\WER\Temp\WER.1e7a8d14-d7ff-4651-af0f-4878c80e56ee.tmp.txt
              \\?\C:\ProgramData\Microsoft\Windows\WER\Temp\WER.3013fd88-7e70-4dfa-b2b5-f215928e33b3.tmp.xml
              
              可在此处获取这些文件:
              \\?\C:\ProgramData\Microsoft\Windows\WER\ReportArchive\AppCrash_media_transcode__c66d16027345b415bc3258aa93efbb75e9742b2_1ced5e8e_e319810b-464f-45a3-8b42-f62b4eda23b6
              
              分析符号: 
              重新检查解决方案: 0
              报告 ID: 6a498714-f31b-48f6-9a01-99e9905bc799
              报告状态: 268435456
              哈希存储段: 4c2c77237bafd568cd7a34290b27f957
              Cab GUID: 0

TimeCreated : 2026/10/19 16:16:44
Id          : 1000
Message     : 出错应用程序名称： media_transcode_realtime_video_cli.exe，版本： 0.0.0.0，时间戳： 0x6ad5d061
              出错模块名称： avutil-60.dll， 版本： 60.33.101.0，时间戳： 0x6a36a4a3
              异常代码： 0xc0000005
              错误偏移： 0x000000000002753d
              出错进程 ID： 0x84BC
              出错应用程序开始时间： 0x1DD5FA150943D98
              Faulting 应用程序路径： D:\Code\MyCode\MediaTranscode\out\build\x64-release\media_transcode_realtime_video_cli.exe
              Faulting 模块路径： D:\Code\MyCode\MediaTranscode\out\build\x64-release\avutil-60.dll
              Report ID： 6a498714-f31b-48f6-9a01-99e9905bc799
              Faulting 包全名： 
              Faulting 程序包相对应用程序 ID：  个


~~~
### 本轮清理清单
composition-r270-cli-error.log
composition-r270-cli.log
composition-r270-monitor.log
composition-r270-native-2026-10-19-16h10m55s650.png
composition-r270-native-2026-10-19-16h12m54s891.png
composition-r270-native-2026-10-19-16h16m15s226.png
composition-r270-native-2026-10-19-16h16m49s757.png
composition-r270-output.sdp
composition-r270-reentry-stdout.log
composition-r270-reentry.log
composition-r270-source0-stdout.log
composition-r270-source0.log
composition-r270-source1-stdout.log
composition-r270-source1.log
composition-r270-source2-stdout.log
composition-r270-source2.log
composition-r270-vlc.log

清理完成：上述17个项目内临时文件已逐项删除；六个精确PID均不存在。复用源保留。未运行远程测试，系统WER记录仅只读查询，未删除项目外文件。
