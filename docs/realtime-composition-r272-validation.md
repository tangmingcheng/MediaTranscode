# r272：最终输出统一准入实流复验

状态：失败，已归档清理。Release全量重建39415成功，四项产物齐全；realtime6204416B，16:42:03，SHA256 3EC242E692C5BD9C86677DE1700E17900B484D7E8E265054A7D1C7070199B877。仅realtime、不附加调试器。三路H264720p30约8Mbps完整120秒，所选AAC44100双声道；输出HEVC720p30 8000kbps CBR、AAC192kbps CBR、MPEGTS/RTP。

## 实际命令
~~~powershell
$ErrorActionPreference='Stop'
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r272-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r272-native- --snapshot-format=png rtp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r272-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r272-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r272-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r272-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r272-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r272-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r272 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r272-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r272-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r272-cli-error.log' -PassThru
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
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r272-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

双独立局部静态审查PASS（a、b2）；会话47706，CLI31096、VLC42256、原始源28388/45072/27744。

## 失源与重入

16:46:05原生快照三格正常；三个源完整120秒结束，16:48:04原生快照全黑，CLI继续输出。重入PID45976，会话5748，实际命令：
~~~powershell
$reentry=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r272-reentry-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r272-reentry.log' -PassThru
Write-Output "REENTRY_PID=$($reentry.Id)"
$reentry.WaitForExit()
Write-Output "REENTRY_EXIT=$($reentry.ExitCode)"
exit $reentry.ExitCode
~~~

## 失败结果

16:48:45原生快照确认仅左下源恢复，拍照时CLI存活。16:49:09重入约55秒后serial-only WouldBlock九次，head50个已提交35个、余15个，count/bytes未耗尽，最终材料化超期4005020ns。CLI退出1并报告原错误，非caller_stop，未发生访问违规；三个原始源已退出0。未取得重入结束后的全黑画面，不构成完整通过。

最终errors/workerErrors=1，dropped=0，queued=0；payload bytes/objects归零，reservations/releases=194748，高水17324300B/748对象。CPU平均1.680122%、峰值5.614035%，WS峰值268472320B、最终259006464B。generation2 phase在失败前约13605ns。sender最大提交迟到29.046240ms，delivery_evidence=not_proven。最长canvas30.4158ms、其三个源event_query均Ready；batch_completion30.2266ms。此次不附加调试器，仍不能证明r267长等待已消失。

### 原始关键记录
~~~text
[CLI] composition_inputs=3 grid=fixed audio_source=2 audio=on max_duration=source_driven hw=planner-highest-score
[2026-10-19 16:45:38.706] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-19 16:45:38.707] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-19 16:45:43.113] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-10-19 16:45:43.113] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-10-19 16:45:43.114] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-10-19 16:45:43.213] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-19 16:45:43.214] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-19 16:45:43.283] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-19 16:45:43.284] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-19 16:45:43.383] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-10-19 16:45:43.430] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-10-19 16:45:43.430] [info] [graph][planner.capability] canvas_cuda_stream stage=prepared device_stream=0000000000000000 private_nonblocking=1
[2026-10-19 16:45:43.437] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-19 16:45:43.437] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-19 16:45:43.437] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-10-19 16:45:43.441] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-19 16:45:43.442] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-19 16:45:43.442] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-10-19 16:45:43.448] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-19 16:45:43.449] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-19 16:45:43.449] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-10-19 16:45:43.473] [info] [graph][planner.capability] composition_preparation_storage stage=graph_bound scope=shared_reservations capacity=2147483648 retained=847925529 high_water=865295348
[CLI] audio_plan branch=transcode_frame reason=transcode_required_by_output_frame_contract codec=aac sample_rate=44100 channels=2 access_unit_samples=1024 encoder=aac bitrate_kbps=192
[CLI] prepared_output_id=1
[CLI] selected_chain=fixed-grid-composition score=0 decoder=h264,h264,h264 filter=scale_cuda=640:360,scale_cuda=640:360,scale_cuda=640:360 encoder=hevc_nvenc
[2026-10-19 16:45:43.478] [info] [graph][runtime.lifecycle] compile.begin
[2026-10-19 16:45:43.479] [info] [graph][runtime.channel] create channel=1 edge=1 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:45:43.479] [info] [graph][runtime.channel] create channel=2 edge=2 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:45:43.479] [info] [graph][runtime.channel] create channel=3 edge=3 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-19 16:45:43.479] [info] [graph][runtime.channel] create channel=4 edge=4 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.479] [info] [graph][runtime.channel] create channel=5 edge=5 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.479] [info] [graph][runtime.channel] create channel=6 edge=6 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.479] [info] [graph][runtime.channel] create channel=7 edge=7 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.479] [info] [graph][runtime.channel] create channel=8 edge=8 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.479] [info] [graph][runtime.channel] create channel=9 edge=9 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:45:43.481] [info] [graph][runtime.channel] create channel=10 edge=10 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.481] [info] [graph][runtime.channel] create channel=11 edge=11 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.481] [info] [graph][runtime.channel] create channel=12 edge=12 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:45:43.481] [info] [graph][runtime.channel] create channel=13 edge=13 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:45:43.481] [info] [graph][runtime.channel] create channel=14 edge=14 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.481] [info] [graph][runtime.channel] create channel=15 edge=15 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.481] [info] [graph][runtime.channel] create channel=16 edge=16 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.481] [info] [graph][runtime.channel] create channel=17 edge=17 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.481] [info] [graph][runtime.channel] create channel=18 edge=18 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.481] [info] [graph][runtime.channel] create channel=19 edge=19 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.481] [info] [graph][runtime.channel] create channel=20 edge=20 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.482] [info] [graph][runtime.channel] create channel=21 edge=21 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-10-19 16:45:43.482] [info] [graph][runtime.channel] create channel=22 edge=22 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:45:43.482] [info] [graph][runtime.channel] create channel=23 edge=23 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:45:43.482] [info] [graph][runtime.channel] create channel=24 edge=24 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:45:43.482] [info] [graph][runtime.channel] create channel=25 edge=25 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:45:43.482] [info] [graph][runtime.channel] create channel=26 edge=26 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:45:43.482] [info] [graph][runtime.channel] create channel=27 edge=27 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:45:43.482] [info] [graph][runtime.channel] create channel=28 edge=28 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:45:43.482] [info] [graph][runtime.channel] create channel=29 edge=29 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:45:43.482] [info] [graph][runtime.channel] create channel=30 edge=30 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.483] [info] [graph][runtime.channel] create channel=31 edge=31 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.483] [info] [graph][runtime.channel] create channel=32 edge=32 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.483] [info] [graph][runtime.channel] create channel=33 edge=33 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.483] [info] [graph][runtime.channel] create channel=34 edge=34 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.483] [info] [graph][runtime.channel] create channel=35 edge=35 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.483] [info] [graph][runtime.channel] create channel=36 edge=36 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:45:43.483] [info] [graph][runtime.channel] create channel=37 edge=37 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.483] [info] [graph][runtime.channel] create channel=38 edge=38 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.483] [info] [graph][runtime.channel] create channel=39 edge=39 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:45:43.483] [info] [graph][runtime.channel] create channel=40 edge=40 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:45:43.483] [info] [graph][runtime.channel] create channel=41 edge=41 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.483] [info] [graph][runtime.channel] create channel=42 edge=42 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.484] [info] [graph][runtime.channel] create channel=43 edge=43 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.484] [info] [graph][runtime.channel] create channel=44 edge=44 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.484] [info] [graph][runtime.channel] create channel=45 edge=45 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.484] [info] [graph][runtime.channel] create channel=46 edge=46 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.484] [info] [graph][runtime.channel] create channel=47 edge=47 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.484] [info] [graph][runtime.channel] create channel=48 edge=48 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-10-19 16:45:43.484] [info] [graph][runtime.channel] create channel=49 edge=49 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:45:43.484] [info] [graph][runtime.channel] create channel=50 edge=50 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:45:43.484] [info] [graph][runtime.channel] create channel=51 edge=51 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:45:43.484] [info] [graph][runtime.channel] create channel=52 edge=52 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:45:43.485] [info] [graph][runtime.channel] create channel=53 edge=53 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:45:43.485] [info] [graph][runtime.channel] create channel=54 edge=54 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:45:43.485] [info] [graph][runtime.channel] create channel=55 edge=55 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:45:43.485] [info] [graph][runtime.channel] create channel=56 edge=56 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:45:43.485] [info] [graph][runtime.channel] create channel=57 edge=57 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.485] [info] [graph][runtime.channel] create channel=58 edge=58 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.485] [info] [graph][runtime.channel] create channel=59 edge=59 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.485] [info] [graph][runtime.channel] create channel=60 edge=60 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.485] [info] [graph][runtime.channel] create channel=61 edge=61 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.485] [info] [graph][runtime.channel] create channel=62 edge=62 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.485] [info] [graph][runtime.channel] create channel=63 edge=63 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.485] [info] [graph][runtime.channel] create channel=64 edge=64 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.486] [info] [graph][runtime.channel] create channel=65 edge=65 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.486] [info] [graph][runtime.channel] create channel=66 edge=66 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:45:43.486] [info] [graph][runtime.channel] create channel=67 edge=67 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:45:43.486] [info] [graph][runtime.channel] create channel=68 edge=68 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.486] [info] [graph][runtime.channel] create channel=69 edge=69 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.486] [info] [graph][runtime.channel] create channel=70 edge=70 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.486] [info] [graph][runtime.channel] create channel=71 edge=71 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:45:43.486] [info] [graph][runtime.channel] create channel=72 edge=72 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:45:43.486] [info] [graph][runtime.channel] create channel=73 edge=73 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:45:43.486] [info] [graph][runtime.channel] create channel=74 edge=74 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:45:43.487] [info] [graph][runtime.channel] create channel=75 edge=75 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.487] [info] [graph][runtime.channel] create channel=76 edge=76 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.487] [info] [graph][runtime.channel] create channel=77 edge=77 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.487] [info] [graph][runtime.channel] create channel=78 edge=78 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.487] [info] [graph][runtime.channel] create channel=79 edge=79 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.487] [info] [graph][runtime.channel] create channel=80 edge=80 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.487] [info] [graph][runtime.channel] create channel=81 edge=81 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.487] [info] [graph][runtime.channel] create channel=82 edge=82 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.487] [info] [graph][runtime.channel] create channel=83 edge=83 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-10-19 16:45:43.487] [info] [graph][runtime.channel] create channel=84 edge=84 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:45:43.487] [info] [graph][runtime.channel] create channel=85 edge=85 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:45:43.488] [info] [graph][runtime.channel] create channel=86 edge=86 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:45:43.488] [info] [graph][runtime.channel] create channel=87 edge=87 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:45:43.488] [info] [graph][runtime.channel] create channel=88 edge=88 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:45:43.488] [info] [graph][runtime.channel] create channel=89 edge=89 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:45:43.488] [info] [graph][runtime.channel] create channel=90 edge=90 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:45:43.488] [info] [graph][runtime.channel] create channel=91 edge=91 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-10-19 16:45:43.488] [info] [graph][runtime.channel] create channel=92 edge=92 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:45:43.488] [info] [graph][runtime.channel] create channel=93 edge=93 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:45:43.488] [info] [graph][runtime.channel] create channel=94 edge=94 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:45:43.488] [info] [graph][runtime.channel] create channel=95 edge=95 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:45:43.488] [info] [graph][runtime.channel] create channel=96 edge=96 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:45:43.489] [info] [graph][runtime.channel] create channel=97 edge=97 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:45:43.489] [info] [graph][runtime.channel] create channel=98 edge=98 stream=Audio edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.489] [info] [graph][runtime.channel] create channel=99 edge=99 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:45:43.489] [info] [graph][runtime.channel] create channel=100 edge=100 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.489] [info] [graph][runtime.channel] create channel=101 edge=101 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0
[2026-10-19 16:45:43.489] [info] [graph][runtime.channel] create channel=102 edge=102 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:45:43.489] [info] [graph][runtime.channel] create channel=103 edge=103 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:45:43.489] [info] [graph][runtime.channel] create channel=104 edge=104 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:45:43.489] [info] [graph][runtime.channel] create channel=105 edge=105 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0
[2026-10-19 16:45:43.489] [info] [graph][runtime.channel] create channel=106 edge=106 stream=Video edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-19 16:45:43.489] [info] [graph][runtime.channel] create channel=107 edge=107 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-19 16:45:43.489] [info] [graph][runtime.channel] create channel=108 edge=108 stream=Any edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-19 16:45:43.490] [info] [graph][runtime.channel] create channel=109 edge=109 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.490] [info] [graph][runtime.channel] create channel=110 edge=110 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.490] [info] [graph][runtime.channel] create channel=111 edge=111 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-19 16:45:43.490] [info] [graph][runtime.channel] create channel=112 edge=112 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:45:43.490] [info] [graph][runtime.channel] create channel=113 edge=113 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:45:43.490] [info] [graph][runtime.channel] create channel=114 edge=114 stream=Any edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-19 16:45:43.490] [info] [graph][runtime.channel] create channel=115 edge=115 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:45:43.490] [info] [graph][runtime.channel] create channel=116 edge=116 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-19 16:45:43.490] [info] [graph][runtime.channel] create channel=117 edge=117 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-19 16:45:43.490] [info] [graph][runtime.channel] create channel=118 edge=118 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-19 16:45:43.490] [info] [graph][runtime.channel] create channel=119 edge=119 stream=Any edge_kind=EncodedPacket payload=Unknown queued=0
[2026-10-19 16:45:43.491] [info] [graph][runtime.channel] create channel=120 edge=120 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-19 16:45:43.491] [info] [graph][runtime.channel] create channel=121 edge=121 stream=Metadata edge_kind=Unknown payload=Unknown queued=0
[2026-10-19 16:45:43.491] [info] [graph][runtime.channel] create channel=122 edge=122 stream=Metadata edge_kind=Unknown payload=Unknown queued=0
[2026-10-19 16:45:43.491] [info] [graph][runtime.channel] create channel=123 edge=123 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-19 16:45:43.492] [info] [graph][runtime.lifecycle] compiled nodes=82 edges=123 channels=123 execution_order=1->3->8->27->46->47->9->21->28->40->63->48->69->10->29->49->11->12->30->31->50->51->52->13->32->53->14->17->33->36->54->55->59->15->34->56->57->16->35->58->18->37->60->19->38->61->20->39->62->22->41->64->70->23->42->65->71->24->43->66->72->26->45->68->73->25->44->67->6->2->7->81->78->4->77->5->74->75->79->76->80->82
[2026-10-19 16:45:43.492] [info] [graph][runtime.lifecycle] compile.done state=DefaultRegistrationPending
[2026-10-19 16:45:43.492] [info] [graph][runtime.node] register node=1 name=composition-r272.output.video.codec_resolver kind=CodecResolver
[2026-10-19 16:45:43.492] [info] [graph][runtime.node] register node=2 name=composition-r272.output.video.encode kind=VideoEncode
[2026-10-19 16:45:43.492] [info] [graph][runtime.node] register node=3 name=composition-r272.output.audio.codec_resolver kind=AudioCodecResolver
[2026-10-19 16:45:43.492] [info] [graph][runtime.node] register node=4 name=composition-r272.output.audio.encode kind=AudioEncode
[2026-10-19 16:45:43.492] [info] [graph][runtime.node] register node=5 name=composition-r272.output.audio.canonicalizer kind=EncodedAudioCanonicalizer
[2026-10-19 16:45:43.493] [info] [graph][runtime.node] register node=8 name=composition-r272.source_0.input kind=RawRtpInput
[2026-10-19 16:45:43.493] [info] [graph][runtime.node] register node=9 name=composition-r272.source_0.rtp.clock_group kind=RtpClockGroup
[2026-10-19 16:45:43.493] [info] [graph][runtime.node] register node=10 name=composition-r272.source_0.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-10-19 16:45:43.493] [info] [graph][runtime.node] register node=11 name=composition-r272.source_0.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-10-19 16:45:43.493] [info] [graph][runtime.node] register node=12 name=composition-r272.source_0.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-10-19 16:45:43.493] [info] [graph][runtime.node] register node=13 name=composition-r272.source_0.av_sync.source_clock kind=SourceClockStateFanout
[2026-10-19 16:45:43.493] [info] [graph][runtime.node] register node=14 name=composition-r272.source_0.av_sync.video.generation_gate kind=LockedPacketGate
[2026-10-19 16:45:43.493] [info] [graph][runtime.node] register node=15 name=composition-r272.source_0.av_sync.video.canonical_input kind=CanonicalInput
[2026-10-19 16:45:43.493] [info] [graph][runtime.node] register node=16 name=composition-r272.source_0.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-10-19 16:45:43.493] [info] [graph][runtime.node] register node=17 name=composition-r272.source_0.av_sync.startup.clock kind=AvStartupClock
[2026-10-19 16:45:43.493] [info] [graph][runtime.node] register node=18 name=composition-r272.source_0.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-10-19 16:45:43.494] [info] [graph][runtime.node] register node=20 name=composition-r272.source_0.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-10-19 16:45:43.494] [info] [graph][runtime.node] register node=21 name=composition-r272.source_0.video.codec_resolver kind=CodecResolver
[2026-10-19 16:45:43.494] [info] [graph][runtime.node] register node=22 name=composition-r272.source_0.video.decode kind=VideoDecode
[2026-10-19 16:45:43.494] [info] [graph][runtime.node] register node=23 name=composition-r272.source_0.video.hwtransfer kind=HardwareTransfer
[2026-10-19 16:45:43.494] [info] [graph][runtime.node] register node=24 name=composition-r272.source_0.video.framerate kind=VideoFrameRate
[2026-10-19 16:45:43.494] [info] [graph][runtime.node] register node=25 name=composition-r272.source_0.video.preparation kind=VideoPreparation
[2026-10-19 16:45:43.494] [info] [graph][runtime.node] register node=26 name=composition-r272.source_0.video.filter kind=VideoFilter
[2026-10-19 16:45:43.494] [info] [graph][runtime.node] register node=27 name=composition-r272.source_1.input kind=RawRtpInput
[2026-10-19 16:45:43.494] [info] [graph][runtime.node] register node=28 name=composition-r272.source_1.rtp.clock_group kind=RtpClockGroup
[2026-10-19 16:45:43.495] [info] [graph][runtime.node] register node=29 name=composition-r272.source_1.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-10-19 16:45:43.495] [info] [graph][runtime.node] register node=30 name=composition-r272.source_1.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-10-19 16:45:43.496] [info] [graph][runtime.node] register node=31 name=composition-r272.source_1.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-10-19 16:45:43.496] [info] [graph][runtime.node] register node=32 name=composition-r272.source_1.av_sync.source_clock kind=SourceClockStateFanout
[2026-10-19 16:45:43.496] [info] [graph][runtime.node] register node=33 name=composition-r272.source_1.av_sync.video.generation_gate kind=LockedPacketGate
[2026-10-19 16:45:43.496] [info] [graph][runtime.node] register node=34 name=composition-r272.source_1.av_sync.video.canonical_input kind=CanonicalInput
[2026-10-19 16:45:43.496] [info] [graph][runtime.node] register node=35 name=composition-r272.source_1.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-10-19 16:45:43.496] [info] [graph][runtime.node] register node=36 name=composition-r272.source_1.av_sync.startup.clock kind=AvStartupClock
[2026-10-19 16:45:43.496] [info] [graph][runtime.node] register node=37 name=composition-r272.source_1.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-10-19 16:45:43.496] [info] [graph][runtime.node] register node=39 name=composition-r272.source_1.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-10-19 16:45:43.496] [info] [graph][runtime.node] register node=40 name=composition-r272.source_1.video.codec_resolver kind=CodecResolver
[2026-10-19 16:45:43.496] [info] [graph][runtime.node] register node=41 name=composition-r272.source_1.video.decode kind=VideoDecode
[2026-10-19 16:45:43.496] [info] [graph][runtime.node] register node=42 name=composition-r272.source_1.video.hwtransfer kind=HardwareTransfer
[2026-10-19 16:45:43.496] [info] [graph][runtime.node] register node=43 name=composition-r272.source_1.video.framerate kind=VideoFrameRate
[2026-10-19 16:45:43.497] [info] [graph][runtime.node] register node=44 name=composition-r272.source_1.video.preparation kind=VideoPreparation
[2026-10-19 16:45:43.497] [info] [graph][runtime.node] register node=45 name=composition-r272.source_1.video.filter kind=VideoFilter
[2026-10-19 16:45:43.497] [info] [graph][runtime.node] register node=46 name=composition-r272.source_2.video.input kind=RawRtpInput
[2026-10-19 16:45:43.497] [info] [graph][runtime.node] register node=47 name=composition-r272.source_2.audio.input kind=RawRtpInput
[2026-10-19 16:45:43.497] [info] [graph][runtime.node] register node=48 name=composition-r272.source_2.rtp.clock_group kind=RtpClockGroup
[2026-10-19 16:45:43.497] [info] [graph][runtime.node] register node=49 name=composition-r272.source_2.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-10-19 16:45:43.497] [info] [graph][runtime.node] register node=50 name=composition-r272.source_2.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-10-19 16:45:43.497] [info] [graph][runtime.node] register node=51 name=composition-r272.source_2.av_sync.audio.protocol_binder kind=RtpPacketClockBinder
[2026-10-19 16:45:43.497] [info] [graph][runtime.node] register node=52 name=composition-r272.source_2.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-10-19 16:45:43.497] [info] [graph][runtime.node] register node=53 name=composition-r272.source_2.av_sync.source_clock kind=SourceClockStateFanout
[2026-10-19 16:45:43.497] [info] [graph][runtime.node] register node=54 name=composition-r272.source_2.av_sync.video.generation_gate kind=LockedPacketGate
[2026-10-19 16:45:43.497] [info] [graph][runtime.node] register node=55 name=composition-r272.source_2.av_sync.audio.generation_gate kind=LockedPacketGate
[2026-10-19 16:45:43.497] [info] [graph][runtime.node] register node=56 name=composition-r272.source_2.av_sync.video.canonical_input kind=CanonicalInput
[2026-10-19 16:45:43.498] [info] [graph][runtime.node] register node=57 name=composition-r272.source_2.av_sync.audio.canonical_input kind=CanonicalInput
[2026-10-19 16:45:43.498] [info] [graph][runtime.node] register node=58 name=composition-r272.source_2.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-10-19 16:45:43.498] [info] [graph][runtime.node] register node=59 name=composition-r272.source_2.av_sync.startup.clock kind=AvStartupClock
[2026-10-19 16:45:43.498] [info] [graph][runtime.node] register node=60 name=composition-r272.source_2.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-10-19 16:45:43.498] [info] [graph][runtime.node] register node=62 name=composition-r272.source_2.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-10-19 16:45:43.498] [info] [graph][runtime.node] register node=63 name=composition-r272.source_2.video.codec_resolver kind=CodecResolver
[2026-10-19 16:45:43.498] [info] [graph][runtime.node] register node=64 name=composition-r272.source_2.video.decode kind=VideoDecode
[2026-10-19 16:45:43.498] [info] [graph][runtime.node] register node=65 name=composition-r272.source_2.video.hwtransfer kind=HardwareTransfer
[2026-10-19 16:45:43.498] [info] [graph][runtime.node] register node=66 name=composition-r272.source_2.video.framerate kind=VideoFrameRate
[2026-10-19 16:45:43.498] [info] [graph][runtime.node] register node=67 name=composition-r272.source_2.video.preparation kind=VideoPreparation
[2026-10-19 16:45:43.498] [info] [graph][runtime.node] register node=68 name=composition-r272.source_2.video.filter kind=VideoFilter
[2026-10-19 16:45:43.499] [info] [graph][runtime.node] register node=69 name=composition-r272.source_2.audio.encode.codec_resolver kind=AudioCodecResolver
[2026-10-19 16:45:43.499] [info] [graph][runtime.node] register node=70 name=composition-r272.source_2.audio.encode.decode kind=AudioDecode
[2026-10-19 16:45:43.499] [info] [graph][runtime.node] register node=71 name=composition-r272.source_2.audio.encode.startup_trim kind=Unknown
[2026-10-19 16:45:43.499] [info] [graph][runtime.node] register node=72 name=composition-r272.source_2.audio.encode.drift_controller kind=AudioDriftController
[2026-10-19 16:45:43.499] [info] [graph][runtime.node] register node=73 name=composition-r272.source_2.audio.encode.resample kind=AudioResample
[2026-10-19 16:45:43.499] [info] [graph][runtime.node] register node=74 name=composition-r272.output.scheduler.scheduler kind=AvOutputScheduler
[2026-10-19 16:45:43.499] [info] [graph][runtime.node] register node=75 name=composition-r272.output.scheduler.router kind=ScheduledOutputRouter
[2026-10-19 16:45:43.499] [info] [graph][runtime.node] register node=76 name=composition-r272.output.mpegts.mux kind=FileMux
[2026-10-19 16:45:43.499] [info] [graph][runtime.node] register node=78 name=composition-r272.output.mpegts.plan.source kind=Unknown
[2026-10-19 16:45:43.499] [info] [graph][runtime.node] register node=79 name=composition-r272.output.mpegts.scheduled.adapter kind=Unknown
[2026-10-19 16:45:43.500] [info] [graph][runtime.node] register node=80 name=composition-r272.output.mpegts.datagram.materializer kind=MpegTsDatagramMaterializer
[2026-10-19 16:45:43.500] [info] [graph][runtime.node] register node=81 name=composition-r272.output.mpegts.transport.plan kind=DatagramTransportPlanSource
[2026-10-19 16:45:43.500] [info] [graph][runtime.node] register node=82 name=composition-r272.output.mpegts.transport.sender kind=ScheduledDatagramSender
[2026-10-19 16:45:43.500] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=waiting_for_random_access detail="" description="D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r272-output.sdp"
[2026-10-19 16:45:43.500] [info] [graph][runtime.lifecycle] start.begin mode=threaded
[2026-10-19 16:45:43.503] [info] [graph][runtime.lifecycle] start.done state=ThreadedRunning
[2026-10-19 16:45:43.503] [info] [graph][runtime.lifecycle] worker.finished node=1
[2026-10-19 16:45:43.503] [info] [graph][runtime.node] worker.thread_cpu node=1 available=1 cpu_ns=0 process_calls=1
[2026-10-19 16:45:43.504] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:45:43.504] [info] [graph][runtime.lifecycle] worker.finished node=3
[2026-10-19 16:45:43.504] [info] [graph][runtime.lifecycle] worker.finished node=63
[2026-10-19 16:45:43.504] [info] [graph][runtime.node] worker.thread_cpu node=63 available=1 cpu_ns=0 process_calls=1
[2026-10-19 16:45:43.505] [info] [graph][runtime.lifecycle] worker.finished node=69
[2026-10-19 16:45:43.505] [info] [graph][runtime.node] worker.thread_cpu node=69 available=1 cpu_ns=15625000 process_calls=1
[2026-10-19 16:45:43.504] [info] [graph][runtime.node] worker.thread_cpu node=3 available=1 cpu_ns=0 process_calls=1
[2026-10-19 16:45:43.509] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:45:43.509] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-10-19 16:45:43.509] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=15 generation=1
[2026-10-19 16:45:43.509] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-10-19 16:45:43.510] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=15 generation=1
[2026-10-19 16:45:43.510] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:45:43.511] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=1701068727 dts=1701068727 size=317016 generation=1 sequence=15
[2026-10-19 16:45:43.511] [info] [graph][runtime.lifecycle] worker.finished node=21
[2026-10-19 16:45:43.512] [info] [graph][runtime.node] worker.thread_cpu node=21 available=1 cpu_ns=0 process_calls=1
[2026-10-19 16:45:43.512] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=3006040 dts=3006040 size=317016 generation=1 sequence=15
[2026-10-19 16:45:43.514] [info] [graph][runtime.node] video_encode.bind_encoder codec=hevc_nvenc pix_fmt=cuda rate_control=cbr bitrate=8000000 min_bitrate=8000000 max_bitrate=8000000 buffer_size=267000 fps=30/1 gop=60 frame_kind=hardware hwaccel=cuda hw_device_ctx=set hw_frames_ctx=set
[2026-10-19 16:45:43.515] [info] [graph][runtime.node] audio_encode_trace stage=codec_bound
[2026-10-19 16:45:43.515] [info] [graph][runtime.lifecycle] worker.finished node=40
[2026-10-19 16:45:43.515] [info] [graph][runtime.node] worker.thread_cpu node=40 available=1 cpu_ns=0 process_calls=1
[2026-10-19 16:45:43.517] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-10-19 16:45:43.518] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=15 generation=1
[2026-10-19 16:45:43.519] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=1543131337 dts=1543131337 size=317016 generation=1 sequence=15
[2026-10-19 16:45:43.546] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-10-19 16:45:43.547] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=1543131337 format=cuda size=1280x720 buffer_identity=0xb08400000 drm_prime=0 software_frame=0
[2026-10-19 16:45:43.547] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=0000020AE61BAB40 stream=Video payload=Frame pts=1543131337 dts=1543131337 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:45:43.547] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=0000020AE61BAB40 stream=Video payload=Frame pts=1543131337 dts=1543131337 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:45:43.547] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=0000020AE61BAB40 stream=Video payload=Frame pts=1543131337 dts=1543131337 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:45:43.547] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-10-19 16:45:43.548] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=0000020AE61BAF30 stream=Video payload=Frame pts=1543131337 dts=1543131337 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:45:43.548] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=0000020AE61BAF30 stream=Video payload=Frame pts=1543131337 dts=1543131337 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:45:43.548] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-10-19 16:45:43.549] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-10-19 16:45:43.550] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=3006040 format=cuda size=1280x720 buffer_identity=0xb08800000 drm_prime=0 software_frame=0
[2026-10-19 16:45:43.550] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=0000020AE61BAB40 stream=Video payload=Frame pts=3006040 dts=3006040 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:45:43.551] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=0000020AE61BAB40 stream=Video payload=Frame pts=3006040 dts=3006040 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:45:43.551] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=0000020AE61BAB40 stream=Video payload=Frame pts=3006040 dts=3006040 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:45:43.551] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-10-19 16:45:43.552] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=0000020AE61BA360 stream=Video payload=Frame pts=3006040 dts=3006040 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:45:43.552] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=0000020AE61BA360 stream=Video payload=Frame pts=3006040 dts=3006040 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:45:43.552] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-10-19 16:45:43.553] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-10-19 16:45:43.554] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=1701068727 format=cuda size=1280x720 buffer_identity=0xb0ae00000 drm_prime=0 software_frame=0
[2026-10-19 16:45:43.554] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=0000020AF18FA140 stream=Video payload=Frame pts=1701068727 dts=1701068727 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:45:43.554] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=0000020AF18FA140 stream=Video payload=Frame pts=1701068727 dts=1701068727 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:45:43.555] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=0000020AF18FA140 stream=Video payload=Frame pts=1701068727 dts=1701068727 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:45:43.555] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-10-19 16:45:43.555] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=0000020AF18FA530 stream=Video payload=Frame pts=1701068727 dts=1701068727 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:45:43.555] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=0000020AF18FA530 stream=Video payload=Frame pts=1701068727 dts=1701068727 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:45:43.556] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-10-19 16:45:43.667] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=110
[2026-10-19 16:45:43.667] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=114
[2026-10-19 16:45:43.667] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=119
[2026-10-19 16:45:43.669] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb08e90000 drm_prime=0 software_frame=0
[2026-10-19 16:45:43.669] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=15 audio_count=0 first_video_sequence=15
[2026-10-19 16:45:43.670] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb08f20000 drm_prime=0 software_frame=0
[2026-10-19 16:45:43.670] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=15 audio_count=42 first_video_sequence=15
[2026-10-19 16:45:43.671] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=0
[2026-10-19 16:45:43.671] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=42
[2026-10-19 16:45:43.671] [info] [graph][runtime.node] audio_decode_trace stage=first_packet
[2026-10-19 16:45:43.672] [info] [graph][runtime.node] audio_decode_trace stage=first_frame samples=1024 rate=44100
[2026-10-19 16:45:43.673] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb08e00000 drm_prime=0 software_frame=0
[2026-10-19 16:45:43.673] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=15 audio_count=0 first_video_sequence=15
[2026-10-19 16:45:43.673] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=0
[2026-10-19 16:45:43.674] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1 source_master_ns=5708535783 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-10-19 16:45:43.674] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3 source_master_ns=5754975693 raw_phase_ns=6804 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=45346
[2026-10-19 16:45:43.675] [info] [graph][runtime.node] audio_encode_trace stage=first_frame
[2026-10-19 16:45:43.676] [info] [graph][runtime.node] mpegts_emission_plan generation=1 mode=canonical_access_unit_window access_unit_window_ns=100000000 video_initial_service_window_ns=13995843 audio_initial_service_window_ns=13995843 packet_size_bytes=188 maximum_payload_bytes=1316 per_datagram_overhead_bytes=12
[2026-10-19 16:45:43.680] [info] [graph][runtime.node] video_encode.input_contract format=cuda size=1280x720 buffer_identity=0xb0a000000 drm_prime=0 software_frame=0
[2026-10-19 16:45:43.680] [info] [graph][runtime.node] video_encode.generation_start_key_frame pts=0 generation=1
[2026-10-19 16:45:43.680] [info] [graph][runtime.node] video_encode.first_frame pts=0 generation=attached
[2026-10-19 16:45:43.687] [info] [graph][runtime.node] video_encode.first_submit result=accepted
[2026-10-19 16:45:43.687] [info] [graph][runtime.node] video_encode.first_packet pts=0 dts=0 duration=0 size=108357 side_data_elems=1 received_footprint=108365 side_data[0]=Quality stats:8
[2026-10-19 16:45:43.688] [info] [graph][runtime.node] video_encode.first_packet_footprint bytes=108365 wrapped_size=108357 wrapped_side_data_elems=1
[2026-10-19 16:45:43.688] [info] [graph][runtime.node] av_scheduler_trace stage=first_video_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=5708546068 dispatch_master_ns=5708546068 now_ns=5555416400 node=74 identity_kind=output identity="composition-r272.output" canonical_sequence=1 canonical_created_steady_ns=28957605294500 head_observed_steady_ns=28957605381000 codec_receive_begin_steady_ns=28957604853400 codec_receive_end_steady_ns=28957604854200
[2026-10-19 16:45:43.688] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=5555710700 present_generation=1 present_canonical_target_ns=0 present_canonical_dispatch_ns=0 present_target_master_ns=5708546068 present_dispatch_master_ns=5708546068
[2026-10-19 16:45:43.723] [info] [graph][runtime.node] audio_encode_trace stage=first_packet
[2026-10-19 16:45:43.724] [info] [graph][runtime.node] av_scheduler_trace stage=first_audio_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=5708546068 dispatch_master_ns=5708546068 now_ns=5591445200 node=74 identity_kind=output identity="composition-r272.output" canonical_sequence=1 canonical_created_steady_ns=28957641353100 head_observed_steady_ns=28957641409800 codec_receive_begin_steady_ns=unavailable codec_receive_end_steady_ns=unavailable
[2026-10-19 16:45:43.867] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=running detail="" description="D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r272-output.sdp"
[2026-10-19 16:45:44.279] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=48 source_master_ns=6799873652 raw_phase_ns=6804 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=90692
[2026-10-19 16:45:45.300] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=92 source_master_ns=7821551656 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=136038
[2026-10-19 16:45:46.322] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=136 source_master_ns=8843229661 raw_phase_ns=6804 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=181384
[2026-10-19 16:45:47.344] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=180 source_master_ns=9864907664 raw_phase_ns=6802 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=226730
[2026-10-19 16:45:48.138] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2139400182 ntp_seconds=4001388348 ntp_fraction=588410519 arrival_ns=28962054837700
[2026-10-19 16:45:48.138] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1456157217 ntp_seconds=4001388348 ntp_fraction=588410519 arrival_ns=28962055014100
[2026-10-19 16:45:48.138] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2139400182
[2026-10-19 16:45:48.138] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1230341113 ntp_seconds=4001388348 ntp_fraction=592705486 arrival_ns=28962055382700
[2026-10-19 16:45:48.139] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1230341113
[2026-10-19 16:45:48.139] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:45:48.138] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1456157217
[2026-10-19 16:45:48.139] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:45:48.139] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:45:48.152] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3730556086 ntp_seconds=4001388348 ntp_fraction=652835028 arrival_ns=28962069315900
[2026-10-19 16:45:48.152] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3730556086
[2026-10-19 16:45:48.152] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:45:48.388] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=225 source_master_ns=10909805624 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=272076

[2026-10-19 16:45:50.432] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=313 source_master_ns=12953161633 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=362768
[2026-10-19 16:45:51.477] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=358 source_master_ns=13998059592 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=408114
[2026-10-19 16:45:52.498] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=402 source_master_ns=15019737596 raw_phase_ns=6802 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=453460
[2026-10-19 16:45:53.145] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2139400182 ntp_seconds=4001388353 ntp_fraction=618475290 arrival_ns=28967061810900
[2026-10-19 16:45:53.145] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2139400182
[2026-10-19 16:45:53.145] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:45:53.146] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1456157217 ntp_seconds=4001388353 ntp_fraction=622770257 arrival_ns=28967062812900
[2026-10-19 16:45:53.146] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1456157217
[2026-10-19 16:45:53.146] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:45:53.146] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1230341113 ntp_seconds=4001388353 ntp_fraction=627065225 arrival_ns=28967063724900
[2026-10-19 16:45:53.146] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1230341113
[2026-10-19 16:45:53.147] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:45:53.180] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3730556086 ntp_seconds=4001388353 ntp_fraction=773094113 arrival_ns=28967097493800
[2026-10-19 16:45:53.181] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3730556086
[2026-10-19 16:45:53.181] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:45:53.520] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=446 source_master_ns=16041415601 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=498806
[2026-10-19 16:45:54.542] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=490 source_master_ns=17063093605 raw_phase_ns=6802 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=544152
[2026-10-19 16:45:55.587] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=535 source_master_ns=18107991565 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=589498
[2026-10-19 16:45:56.608] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=579 source_master_ns=19129669569 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=634844
[2026-10-19 16:45:57.630] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=623 source_master_ns=20151347574 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=680190
[2026-10-19 16:45:58.150] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2139400182 ntp_seconds=4001388358 ntp_fraction=639950127 arrival_ns=28972066373600
[2026-10-19 16:45:58.150] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2139400182
[2026-10-19 16:45:58.150] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:45:58.152] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1456157217 ntp_seconds=4001388358 ntp_fraction=648540061 arrival_ns=28972068258100
[2026-10-19 16:45:58.152] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1456157217
[2026-10-19 16:45:58.152] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:45:58.163] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1230341113 ntp_seconds=4001388358 ntp_fraction=700079669 arrival_ns=28972080313600
[2026-10-19 16:45:58.163] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1230341113
[2026-10-19 16:45:58.163] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:45:58.208] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3730556086 ntp_seconds=4001388358 ntp_fraction=893353197 arrival_ns=28972125392100
[2026-10-19 16:45:58.209] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3730556086
[2026-10-19 16:45:58.209] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:45:58.675] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=668 source_master_ns=21196245533 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=725536
[2026-10-19 16:45:59.697] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=712 source_master_ns=22217923537 raw_phase_ns=6802 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=770882
[2026-10-19 16:46:00.719] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=756 source_master_ns=23239601542 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=816228
[2026-10-19 16:46:01.740] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=800 source_master_ns=24261279546 raw_phase_ns=6802 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=861574
[2026-10-19 16:46:02.785] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=845 source_master_ns=25306177506 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=906920
[2026-10-19 16:46:03.172] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1230341113 ntp_seconds=4001388363 ntp_fraction=738734374 arrival_ns=28977089595600
[2026-10-19 16:46:03.172] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1456157217 ntp_seconds=4001388363 ntp_fraction=738734374 arrival_ns=28977089718600
[2026-10-19 16:46:03.173] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1456157217
[2026-10-19 16:46:03.173] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1230341113
[2026-10-19 16:46:03.174] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:03.173] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:03.172] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2139400182 ntp_seconds=4001388363 ntp_fraction=738734374 arrival_ns=28977089810600
[2026-10-19 16:46:03.174] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2139400182
[2026-10-19 16:46:03.175] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:03.218] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3730556086 ntp_seconds=4001388363 ntp_fraction=932007903 arrival_ns=28977134872100
[2026-10-19 16:46:03.218] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3730556086
[2026-10-19 16:46:03.218] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:03.807] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=889 source_master_ns=26327855510 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=952266
[2026-10-19 16:46:04.828] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=933 source_master_ns=27349533515 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=997612
[2026-10-19 16:46:05.873] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=978 source_master_ns=28394431474 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1042958
[2026-10-19 16:46:06.895] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1022 source_master_ns=29416109478 raw_phase_ns=6802 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1088304
[2026-10-19 16:46:07.916] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1066 source_master_ns=30437787483 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1133650
[2026-10-19 16:46:08.173] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1230341113 ntp_seconds=4001388368 ntp_fraction=743029342 arrival_ns=28982090033200
[2026-10-19 16:46:08.173] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1230341113
[2026-10-19 16:46:08.174] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:08.213] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2139400182 ntp_seconds=4001388368 ntp_fraction=914828034 arrival_ns=28982130570100
[2026-10-19 16:46:08.213] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1456157217 ntp_seconds=4001388368 ntp_fraction=914828034 arrival_ns=28982130571000
[2026-10-19 16:46:08.214] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1456157217
[2026-10-19 16:46:08.214] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:08.214] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2139400182
[2026-10-19 16:46:08.214] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:08.244] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3730556086 ntp_seconds=4001388368 ntp_fraction=1047972020 arrival_ns=28982161383900
[2026-10-19 16:46:08.244] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3730556086
[2026-10-19 16:46:08.244] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:08.938] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1110 source_master_ns=31459465487 raw_phase_ns=6802 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1178996
[2026-10-19 16:46:09.984] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1155 source_master_ns=32504363447 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1224342
[2026-10-19 16:46:11.010] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1199 source_master_ns=33526041451 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1269688
[2026-10-19 16:46:12.026] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1243 source_master_ns=34547719456 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1315034
[2026-10-19 16:46:13.071] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1288 source_master_ns=35592617415 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1360380
[2026-10-19 16:46:13.188] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1230341113 ntp_seconds=4001388373 ntp_fraction=807453851 arrival_ns=28987105434200
[2026-10-19 16:46:13.188] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1230341113
[2026-10-19 16:46:13.189] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:13.216] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2139400182 ntp_seconds=4001388373 ntp_fraction=923417968 arrival_ns=28987132897000
[2026-10-19 16:46:13.216] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2139400182
[2026-10-19 16:46:13.216] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1456157217 ntp_seconds=4001388373 ntp_fraction=927712935 arrival_ns=28987133056200
[2026-10-19 16:46:13.216] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1456157217
[2026-10-19 16:46:13.216] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:13.216] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:13.246] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3730556086 ntp_seconds=4001388373 ntp_fraction=1056561954 arrival_ns=28987163189800
[2026-10-19 16:46:13.246] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3730556086
[2026-10-19 16:46:13.247] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:14.094] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1332 source_master_ns=36614295419 raw_phase_ns=6802 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1405726
[2026-10-19 16:46:15.123] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1376 source_master_ns=37635973424 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1451072
[2026-10-19 16:46:16.137] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1420 source_master_ns=38657651428 raw_phase_ns=6802 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1496418
[2026-10-19 16:46:17.182] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1465 source_master_ns=39702549388 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1541764
[2026-10-19 16:46:18.200] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1230341113 ntp_seconds=4001388378 ntp_fraction=858993459 arrival_ns=28992117672700
[2026-10-19 16:46:18.201] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1230341113
[2026-10-19 16:46:18.201] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:18.203] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1509 source_master_ns=40724227392 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1587110
[2026-10-19 16:46:18.245] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1456157217 ntp_seconds=4001388378 ntp_fraction=1047972020 arrival_ns=28992161882200
[2026-10-19 16:46:18.245] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1456157217
[2026-10-19 16:46:18.245] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2139400182 ntp_seconds=4001388378 ntp_fraction=1047972020 arrival_ns=28992161911700
[2026-10-19 16:46:18.246] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2139400182
[2026-10-19 16:46:18.246] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:18.246] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:18.283] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3730556086 ntp_seconds=4001388378 ntp_fraction=1215475744 arrival_ns=28992200135500
[2026-10-19 16:46:18.283] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3730556086
[2026-10-19 16:46:18.283] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:19.227] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1553 source_master_ns=41745905397 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1632456
[2026-10-19 16:46:20.270] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1598 source_master_ns=42790803356 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1677802
[2026-10-19 16:46:21.291] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1642 source_master_ns=43812481360 raw_phase_ns=6802 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1723148
[2026-10-19 16:46:22.314] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1686 source_master_ns=44834159365 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1768494
[2026-10-19 16:46:23.228] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1230341113 ntp_seconds=4001388383 ntp_fraction=979252543 arrival_ns=28997145417800
[2026-10-19 16:46:23.228] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1230341113
[2026-10-19 16:46:23.228] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:23.256] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2139400182 ntp_seconds=4001388383 ntp_fraction=1099511627 arrival_ns=28997173409400
[2026-10-19 16:46:23.257] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2139400182
[2026-10-19 16:46:23.257] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:23.257] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1456157217 ntp_seconds=4001388383 ntp_fraction=1103806595 arrival_ns=28997174416900
[2026-10-19 16:46:23.258] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1456157217
[2026-10-19 16:46:23.258] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:23.285] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3730556086 ntp_seconds=4001388383 ntp_fraction=1219770712 arrival_ns=28997201627400
[2026-10-19 16:46:23.285] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3730556086
[2026-10-19 16:46:23.285] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:23.334] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1730 source_master_ns=45855837370 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1813840
[2026-10-19 16:46:24.380] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1775 source_master_ns=46900735329 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1859186
[2026-10-19 16:46:25.402] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1819 source_master_ns=47922413333 raw_phase_ns=6802 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1904532
[2026-10-19 16:46:26.423] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1863 source_master_ns=48944091338 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1949878
[2026-10-19 16:46:27.468] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1908 source_master_ns=49988989297 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1995224
[2026-10-19 16:46:28.231] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1230341113 ntp_seconds=4001388388 ntp_fraction=992137445 arrival_ns=29002148111600
[2026-10-19 16:46:28.231] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1230341113
[2026-10-19 16:46:28.231] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:28.277] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2139400182 ntp_seconds=4001388388 ntp_fraction=1189705940 arrival_ns=29002194040100
[2026-10-19 16:46:28.277] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2139400182
[2026-10-19 16:46:28.277] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:28.277] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1456157217 ntp_seconds=4001388388 ntp_fraction=1189705940 arrival_ns=29002194291900
[2026-10-19 16:46:28.278] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1456157217
[2026-10-19 16:46:28.278] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:28.309] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3730556086 ntp_seconds=4001388388 ntp_fraction=1322849927 arrival_ns=29002225981300
[2026-10-19 16:46:28.309] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3730556086
[2026-10-19 16:46:28.309] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:28.489] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1952 source_master_ns=51010667302 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2040570
[2026-10-19 16:46:29.511] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1996 source_master_ns=52032345306 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2085916
[2026-10-19 16:46:30.535] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2040 source_master_ns=53054023311 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2131262
[2026-10-19 16:46:30.574] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=52441359600 present_generation=1 present_canonical_target_ns=46834648526 present_canonical_dispatch_ns=46834648526 present_target_master_ns=52543194594 present_dispatch_master_ns=52543194594
[2026-10-19 16:46:30.577] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=52444023600 present_generation=1 present_canonical_target_ns=46866666666 present_canonical_dispatch_ns=46866666666 present_target_master_ns=52575212734 present_dispatch_master_ns=52575212734
[2026-10-19 16:46:31.583] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2085 source_master_ns=54098921270 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2176608
[2026-10-19 16:46:32.601] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2129 source_master_ns=55120599274 raw_phase_ns=6802 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2221954
[2026-10-19 16:46:33.255] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1230341113 ntp_seconds=4001388393 ntp_fraction=1090921693 arrival_ns=29007171911800
[2026-10-19 16:46:33.255] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1230341113
[2026-10-19 16:46:33.256] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:33.285] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2139400182 ntp_seconds=4001388393 ntp_fraction=1219770712 arrival_ns=29007201909500
[2026-10-19 16:46:33.285] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2139400182
[2026-10-19 16:46:33.285] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:33.316] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1456157217 ntp_seconds=4001388393 ntp_fraction=1352914698 arrival_ns=29007232856700
[2026-10-19 16:46:33.316] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3730556086 ntp_seconds=4001388393 ntp_fraction=1352914698 arrival_ns=29007232947600
[2026-10-19 16:46:33.316] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3730556086
[2026-10-19 16:46:33.316] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:33.316] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1456157217
[2026-10-19 16:46:33.316] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:33.621] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2173 source_master_ns=56142277279 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2267300
[2026-10-19 16:46:34.666] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2218 source_master_ns=57187175238 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2312646
[2026-10-19 16:46:35.688] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2262 source_master_ns=58208853243 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2357992
[2026-10-19 16:46:36.711] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2306 source_master_ns=59230531247 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2403338
[2026-10-19 16:46:37.731] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2350 source_master_ns=60252209252 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2448684
[2026-10-19 16:46:38.260] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1230341113 ntp_seconds=4001388398 ntp_fraction=1116691496 arrival_ns=29012177644300
[2026-10-19 16:46:38.261] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1230341113
[2026-10-19 16:46:38.261] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:38.320] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2139400182 ntp_seconds=4001388398 ntp_fraction=1370094567 arrival_ns=29012236916200
[2026-10-19 16:46:38.320] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2139400182
[2026-10-19 16:46:38.320] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:38.321] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1456157217 ntp_seconds=4001388398 ntp_fraction=1374389534 arrival_ns=29012237053000
[2026-10-19 16:46:38.321] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1456157217
[2026-10-19 16:46:38.321] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:38.322] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3730556086
[2026-10-19 16:46:38.322] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1

[2026-10-19 16:46:38.776] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2395 source_master_ns=61297107211 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2494030
[2026-10-19 16:46:39.797] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2439 source_master_ns=62318785215 raw_phase_ns=6802 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2539376
[2026-10-19 16:46:40.819] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2483 source_master_ns=63340463220 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2584722
[2026-10-19 16:46:41.841] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2527 source_master_ns=64362141224 raw_phase_ns=6802 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2630068
[2026-10-19 16:46:42.886] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2572 source_master_ns=65407039184 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2675414
[2026-10-19 16:46:43.278] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1230341113 ntp_seconds=4001388403 ntp_fraction=1194000908 arrival_ns=29017195204800
[2026-10-19 16:46:43.278] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1230341113
[2026-10-19 16:46:43.278] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:43.351] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3730556086 ntp_seconds=4001388403 ntp_fraction=1507533520 arrival_ns=29017268332700
[2026-10-19 16:46:43.351] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3730556086
[2026-10-19 16:46:43.352] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:43.351] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2139400182 ntp_seconds=4001388403 ntp_fraction=1507533520 arrival_ns=29017268460200
[2026-10-19 16:46:43.352] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2139400182
[2026-10-19 16:46:43.353] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:43.352] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1456157217 ntp_seconds=4001388403 ntp_fraction=1511828488 arrival_ns=29017269112500
[2026-10-19 16:46:43.353] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1456157217
[2026-10-19 16:46:43.353] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:43.629] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=65496675200 present_generation=1 present_canonical_target_ns=59907482993 present_canonical_dispatch_ns=59907482993 present_target_master_ns=65616029061 present_dispatch_master_ns=65616029061
[2026-10-19 16:46:43.630] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=65497026000 present_generation=1 present_canonical_target_ns=59933333333 present_canonical_dispatch_ns=59933333333 present_target_master_ns=65641879401 present_dispatch_master_ns=65641879401
[2026-10-19 16:46:43.909] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2616 source_master_ns=66428717188 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2720760
[2026-10-19 16:46:44.929] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2660 source_master_ns=67450395193 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2766106
[2026-10-19 16:46:45.974] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2705 source_master_ns=68495293152 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2811452
[2026-10-19 16:46:46.996] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2749 source_master_ns=69516971156 raw_phase_ns=6802 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2856798
[2026-10-19 16:46:48.018] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2793 source_master_ns=70538649161 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2902144
[2026-10-19 16:46:48.287] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1230341113 ntp_seconds=4001388408 ntp_fraction=1232655613 arrival_ns=29022204846500
[2026-10-19 16:46:48.288] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1230341113
[2026-10-19 16:46:48.288] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:48.378] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3730556086 ntp_seconds=4001388408 ntp_fraction=1623497637 arrival_ns=29022294986300
[2026-10-19 16:46:48.378] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3730556086
[2026-10-19 16:46:48.378] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:48.380] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2139400182 ntp_seconds=4001388408 ntp_fraction=1627792605 arrival_ns=29022296185700
[2026-10-19 16:46:48.380] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2139400182
[2026-10-19 16:46:48.380] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:48.382] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1456157217 ntp_seconds=4001388408 ntp_fraction=1636382539 arrival_ns=29022298266600
[2026-10-19 16:46:48.382] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1456157217
[2026-10-19 16:46:48.383] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:49.039] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2837 source_master_ns=71560327165 raw_phase_ns=6802 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2947490
[2026-10-19 16:46:50.084] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2882 source_master_ns=72605225125 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2992836
[2026-10-19 16:46:51.106] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2926 source_master_ns=73626903129 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3038182
[2026-10-19 16:46:52.127] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=2970 source_master_ns=74648581134 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3083528
[2026-10-19 16:46:53.172] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3015 source_master_ns=75693479093 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3128874
[2026-10-19 16:46:53.289] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1230341113 ntp_seconds=4001388413 ntp_fraction=1241245548 arrival_ns=29027206552800
[2026-10-19 16:46:53.290] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1230341113
[2026-10-19 16:46:53.290] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:53.408] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2139400182 ntp_seconds=4001388413 ntp_fraction=1752346656 arrival_ns=29027325577200
[2026-10-19 16:46:53.409] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2139400182
[2026-10-19 16:46:53.409] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:53.409] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3730556086 ntp_seconds=4001388413 ntp_fraction=1756641624 arrival_ns=29027326379500
[2026-10-19 16:46:53.410] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3730556086
[2026-10-19 16:46:53.410] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:53.408] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1456157217 ntp_seconds=4001388413 ntp_fraction=1752346656 arrival_ns=29027325714900
[2026-10-19 16:46:53.411] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1456157217
[2026-10-19 16:46:53.411] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:54.194] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3059 source_master_ns=76715157097 raw_phase_ns=6802 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3174220
[2026-10-19 16:46:55.216] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3103 source_master_ns=77736835102 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3219566
[2026-10-19 16:46:56.237] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3147 source_master_ns=78758513107 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3264912
[2026-10-19 16:46:57.282] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3192 source_master_ns=79803411066 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3310258
[2026-10-19 16:46:58.312] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3236 source_master_ns=80825089070 raw_phase_ns=6802 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3355604
[2026-10-19 16:46:58.313] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1230341113 ntp_seconds=4001388418 ntp_fraction=1344324763 arrival_ns=29032230389200
[2026-10-19 16:46:58.313] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1230341113
[2026-10-19 16:46:58.313] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:58.416] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3730556086 ntp_seconds=4001388418 ntp_fraction=1786706395 arrival_ns=29032333201600
[2026-10-19 16:46:58.416] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3730556086
[2026-10-19 16:46:58.416] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:58.417] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2139400182 ntp_seconds=4001388418 ntp_fraction=1791001362 arrival_ns=29032334016100
[2026-10-19 16:46:58.417] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2139400182
[2026-10-19 16:46:58.417] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:58.418] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1456157217 ntp_seconds=4001388418 ntp_fraction=1791001362 arrival_ns=29032334833400
[2026-10-19 16:46:58.418] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1456157217
[2026-10-19 16:46:58.418] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:46:59.325] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3280 source_master_ns=81846767075 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3400950
[2026-10-19 16:47:00.372] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3325 source_master_ns=82891665034 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3446296
[2026-10-19 16:47:01.392] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3369 source_master_ns=83913343038 raw_phase_ns=6802 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3491642
[2026-10-19 16:47:02.415] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3413 source_master_ns=84935021043 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3536988
[2026-10-19 16:47:03.324] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1230341113 ntp_seconds=4001388423 ntp_fraction=1391569403 arrival_ns=29037241091700
[2026-10-19 16:47:03.324] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1230341113
[2026-10-19 16:47:03.325] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:47:03.435] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3457 source_master_ns=85956699048 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3582334
[2026-10-19 16:47:03.440] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1456157217 ntp_seconds=4001388423 ntp_fraction=1889785610 arrival_ns=29037357329800
[2026-10-19 16:47:03.440] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2139400182 ntp_seconds=4001388423 ntp_fraction=1889785610 arrival_ns=29037357500600
[2026-10-19 16:47:03.440] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2139400182
[2026-10-19 16:47:03.440] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1456157217
[2026-10-19 16:47:03.441] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:47:03.441] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:47:03.441] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3730556086 ntp_seconds=4001388423 ntp_fraction=1889785610 arrival_ns=29037357864700
[2026-10-19 16:47:03.442] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3730556086
[2026-10-19 16:47:03.442] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:47:04.480] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3502 source_master_ns=87001597007 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3627680
[2026-10-19 16:47:05.502] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3546 source_master_ns=88023275011 raw_phase_ns=6802 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3673026
[2026-10-19 16:47:06.524] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3590 source_master_ns=89044953016 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3718372
[2026-10-19 16:47:07.569] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3635 source_master_ns=90089850975 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3763718
[2026-10-19 16:47:08.351] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1230341113 ntp_seconds=4001388428 ntp_fraction=1507533520 arrival_ns=29042268810300
[2026-10-19 16:47:08.352] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1230341113
[2026-10-19 16:47:08.352] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:47:08.441] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1456157217 ntp_seconds=4001388428 ntp_fraction=1894080577 arrival_ns=29042358732600
[2026-10-19 16:47:08.442] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1456157217
[2026-10-19 16:47:08.442] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:47:08.472] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2139400182 ntp_seconds=4001388428 ntp_fraction=2027224563 arrival_ns=29042389161000
[2026-10-19 16:47:08.472] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2139400182
[2026-10-19 16:47:08.472] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:47:08.486] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3730556086 ntp_seconds=4001388428 ntp_fraction=2083059138 arrival_ns=29042402922800
[2026-10-19 16:47:08.486] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3730556086
[2026-10-19 16:47:08.486] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:47:08.590] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3679 source_master_ns=91111528980 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3809064
[2026-10-19 16:47:09.612] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3723 source_master_ns=92133206984 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3854410
[2026-10-19 16:47:10.634] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3767 source_master_ns=93154884989 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3899756
[2026-10-19 16:47:11.679] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3812 source_master_ns=94199782948 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3945102
[2026-10-19 16:47:12.704] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3856 source_master_ns=95221460952 raw_phase_ns=6802 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=3990448
[2026-10-19 16:47:13.328] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=95195825200 present_generation=1 present_canonical_target_ns=89605804988 present_canonical_dispatch_ns=89605804988 present_target_master_ns=95314351056 present_dispatch_master_ns=95314351056
[2026-10-19 16:47:13.329] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=95196795400 present_generation=1 present_canonical_target_ns=89633333333 present_canonical_dispatch_ns=89633333333 present_target_master_ns=95341879401 present_dispatch_master_ns=95341879401
[2026-10-19 16:47:13.352] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1230341113 ntp_seconds=4001388433 ntp_fraction=1511828488 arrival_ns=29047269422200
[2026-10-19 16:47:13.352] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1230341113
[2026-10-19 16:47:13.352] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:47:13.481] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2139400182 ntp_seconds=4001388433 ntp_fraction=2061584302 arrival_ns=29047397654300
[2026-10-19 16:47:13.481] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2139400182
[2026-10-19 16:47:13.481] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:47:13.481] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1456157217 ntp_seconds=4001388433 ntp_fraction=2065879269 arrival_ns=29047398514300
[2026-10-19 16:47:13.482] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1456157217
[2026-10-19 16:47:13.482] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:47:13.511] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3730556086 ntp_seconds=4001388433 ntp_fraction=2194728288 arrival_ns=29047428449900
[2026-10-19 16:47:13.511] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3730556086
[2026-10-19 16:47:13.512] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:47:13.722] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3900 source_master_ns=96243138957 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4035794
[2026-10-19 16:47:14.767] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3945 source_master_ns=97288036916 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4081140
[2026-10-19 16:47:15.788] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3989 source_master_ns=98309714921 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4126486
[2026-10-19 16:47:16.812] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4033 source_master_ns=99331392925 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4171832
[2026-10-19 16:47:17.832] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4077 source_master_ns=100353070930 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4217178
[2026-10-19 16:47:18.368] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1230341113 ntp_seconds=4001388438 ntp_fraction=1580547964 arrival_ns=29052285091500
[2026-10-19 16:47:18.368] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1230341113
[2026-10-19 16:47:18.369] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:47:18.485] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2139400182 ntp_seconds=4001388438 ntp_fraction=2083059138 arrival_ns=29052401979000
[2026-10-19 16:47:18.485] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2139400182
[2026-10-19 16:47:18.485] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:47:18.512] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1456157217 ntp_seconds=4001388438 ntp_fraction=2194728288 arrival_ns=29052428850300
[2026-10-19 16:47:18.512] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1456157217
[2026-10-19 16:47:18.512] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:47:18.542] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3730556086 ntp_seconds=4001388438 ntp_fraction=2327872274 arrival_ns=29052459642800
[2026-10-19 16:47:18.544] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3730556086
[2026-10-19 16:47:18.544] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:47:18.879] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4122 source_master_ns=101397968889 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4262524
[2026-10-19 16:47:19.899] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4166 source_master_ns=102419646893 raw_phase_ns=6802 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4307870
[2026-10-19 16:47:20.921] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4210 source_master_ns=103441324898 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4353216
[2026-10-19 16:47:21.965] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4255 source_master_ns=104486222857 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4398562
[2026-10-19 16:47:22.987] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4299 source_master_ns=105507900862 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4443908
[2026-10-19 16:47:23.385] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1230341113 ntp_seconds=4001388443 ntp_fraction=1653562408 arrival_ns=29057302727600
[2026-10-19 16:47:23.386] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1230341113
[2026-10-19 16:47:23.386] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:47:23.517] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2139400182 ntp_seconds=4001388443 ntp_fraction=2216203124 arrival_ns=29057434225600
[2026-10-19 16:47:23.517] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2139400182
[2026-10-19 16:47:23.517] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:47:23.519] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1456157217 ntp_seconds=4001388443 ntp_fraction=2229088026 arrival_ns=29057436264000
[2026-10-19 16:47:23.520] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1456157217
[2026-10-19 16:47:23.520] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:47:23.584] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3730556086 ntp_seconds=4001388443 ntp_fraction=2508260900 arrival_ns=29057501547400
[2026-10-19 16:47:23.585] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3730556086
[2026-10-19 16:47:23.585] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:47:24.008] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4343 source_master_ns=106529578866 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4489254
[2026-10-19 16:47:25.030] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4387 source_master_ns=107551256871 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4534600
[2026-10-19 16:47:26.076] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4432 source_master_ns=108596154830 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4579946
[2026-10-19 16:47:27.097] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4476 source_master_ns=109617832834 raw_phase_ns=6802 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4625292
[2026-10-19 16:47:28.119] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4520 source_master_ns=110639510839 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4670638
[2026-10-19 16:47:28.398] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1230341113 ntp_seconds=4001388448 ntp_fraction=1709396983 arrival_ns=29062315599500
[2026-10-19 16:47:28.399] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1230341113
[2026-10-19 16:47:28.399] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:47:28.545] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1456157217 ntp_seconds=4001388448 ntp_fraction=2340757176 arrival_ns=29062462455600
[2026-10-19 16:47:28.545] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1456157217
[2026-10-19 16:47:28.545] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:47:28.545] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2139400182 ntp_seconds=4001388448 ntp_fraction=2340757176 arrival_ns=29062462623000
[2026-10-19 16:47:28.546] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2139400182
[2026-10-19 16:47:28.546] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:47:28.635] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3730556086 ntp_seconds=4001388448 ntp_fraction=2723009265 arrival_ns=29062551946900
[2026-10-19 16:47:28.635] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3730556086
[2026-10-19 16:47:28.635] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:47:29.163] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4565 source_master_ns=111684408798 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4715984
[2026-10-19 16:47:30.185] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4609 source_master_ns=112706086803 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4761330
[2026-10-19 16:47:31.206] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4653 source_master_ns=113727764807 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4806676
[2026-10-19 16:47:32.230] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4697 source_master_ns=114749442812 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4852022
[2026-10-19 16:47:33.273] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4742 source_master_ns=115794340771 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4897368
[2026-10-19 16:47:33.407] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1230341113 ntp_seconds=4001388453 ntp_fraction=1748051689 arrival_ns=29067324298300
[2026-10-19 16:47:33.407] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1230341113
[2026-10-19 16:47:33.408] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:47:33.572] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1456157217 ntp_seconds=4001388453 ntp_fraction=2456721293 arrival_ns=29067489633700
[2026-10-19 16:47:33.573] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=2139400182 ntp_seconds=4001388453 ntp_fraction=2456721293 arrival_ns=29067489702300
[2026-10-19 16:47:33.573] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2139400182
[2026-10-19 16:47:33.573] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1456157217
[2026-10-19 16:47:33.574] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:47:33.573] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:47:33.642] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=3730556086 ntp_seconds=4001388453 ntp_fraction=2753074036 arrival_ns=29067558892500
[2026-10-19 16:47:33.642] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3730556086
[2026-10-19 16:47:33.643] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:47:34.300] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4786 source_master_ns=116816018775 raw_phase_ns=6802 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4942714
[2026-10-19 16:47:35.317] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4830 source_master_ns=117837696780 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=4988060
[2026-10-19 16:47:36.362] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4875 source_master_ns=118882594739 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5033406
[2026-10-19 16:47:37.383] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=4919 source_master_ns=119904272744 raw_phase_ns=6803 filtered_phase_ns=6803 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=5078752
[2026-10-19 16:47:37.583] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=29071499929700
[2026-10-19 16:47:37.583] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=3 generation=2 invalidated_generation=1 locked=0
[2026-10-19 16:47:37.583] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=29071500195500
[2026-10-19 16:47:37.583] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=3 generation=2 invalidated_generation=1 locked=0
[2026-10-19 16:47:37.583] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=30 group=composition-r272.source.1 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=23 video_raw_generation=0 video_ssrc=2139400182
[2026-10-19 16:47:37.584] [info] [graph][runtime.node] av_reacquisition group=composition-r272.source.1 phase=purging old=1 next=2 transition=1
[2026-10-19 16:47:37.584] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=11 group=composition-r272.source.0 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=23 video_raw_generation=0 video_ssrc=1456157217
[2026-10-19 16:47:37.585] [info] [graph][runtime.node] av_reacquisition group=composition-r272.source.0 phase=purging old=1 next=2 transition=1
[2026-10-19 16:47:37.589] [info] [graph][runtime.node] rtp_ingress_purge node=8 transition=1 cutoff=0
[2026-10-19 16:47:37.593] [info] [graph][runtime.node] rtp_binder_purge node=11 transition=1 cutoff=3608
[2026-10-19 16:47:37.594] [info] [graph][runtime.node] av_reacquisition group=composition-r272.source.0 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=1 next=2 transition=1
[2026-10-19 16:47:37.597] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=29071514568100
[2026-10-19 16:47:37.597] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=3 generation=2 invalidated_generation=1 locked=0
[2026-10-19 16:47:37.599] [info] [graph][runtime.node] rtp_source_unavailable ingress_generation=1 reason=1 observed_at_ns=29071516140700
[2026-10-19 16:47:37.599] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=3 generation=2 invalidated_generation=1 locked=0
[2026-10-19 16:47:37.600] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=51 group=composition-r272.source.2 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=46 video_raw_generation=0 video_ssrc=3730556086 audio_raw_generation=0 audio_ssrc=1230341113
[2026-10-19 16:47:37.600] [info] [graph][runtime.node] av_reacquisition group=composition-r272.source.2 phase=purging old=1 next=2 transition=1
[2026-10-19 16:47:37.600] [info] [graph][runtime.node] rtp_binder_boundary stage=clock_invalidated node=50 group=composition-r272.source.2 buffered=0 phase=0 preserves_output=1 protocol_generation=1 evidence_revision=46 video_raw_generation=0 video_ssrc=3730556086 audio_raw_generation=0 audio_ssrc=1230341113
[2026-10-19 16:47:37.600] [info] [graph][runtime.node] rtp_ingress_purge node=46 transition=1 cutoff=0
[2026-10-19 16:47:37.600] [info] [graph][runtime.node] rtp_ingress_purge node=47 transition=1 cutoff=0
[2026-10-19 16:47:37.600] [info] [graph][runtime.node] rtp_binder_purge node=51 transition=1 cutoff=5169
[2026-10-19 16:47:37.603] [info] [graph][runtime.node] rtp_binder_purge node=50 transition=1 cutoff=3608
[2026-10-19 16:47:37.603] [info] [graph][runtime.node] av_reacquisition group=composition-r272.source.2 event=purge_completed purge_ack=complete continuation=acquiring ack_count=2 old=1 next=2 transition=1
[2026-10-19 16:47:37.611] [info] [graph][runtime.node] rtp_ingress_purge node=27 transition=1 cutoff=0
[2026-10-19 16:47:37.611] [info] [graph][runtime.node] rtp_binder_purge node=30 transition=1 cutoff=3608
[2026-10-19 16:47:37.613] [info] [graph][runtime.node] av_reacquisition group=composition-r272.source.1 event=purge_completed purge_ack=complete continuation=acquiring ack_count=1 old=1 next=2 transition=1
[2026-10-19 16:48:14.799] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=726864859 ntp_seconds=4001388494 ntp_fraction=3431678869 arrival_ns=29108716216900
[2026-10-19 16:48:14.799] [info] [graph][runtime.node] rtp_binder_boundary stage=deadline_established node=51 group=composition-r272.source.2 buffered=0 phase=2 preserves_output=1 raw_ssrc=512842092 raw_generation=1 raw_sequence=5171 old_generation=1 next_generation=2 transition_sequence=1 published_generation=1
[2026-10-19 16:48:14.799] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=726864859
[2026-10-19 16:48:14.800] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=0 generation=2 invalidated_generation=1 locked=0
[2026-10-19 16:48:14.800] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=512842092 ntp_seconds=4001388494 ntp_fraction=3431678869 arrival_ns=29108716246500
[2026-10-19 16:48:14.800] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=512842092
[2026-10-19 16:48:14.800] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-19 16:48:15.345] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=1 generation=2
[2026-10-19 16:48:15.839] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1 source_master_ns=157807333352 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-10-19 16:48:15.840] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=3 source_master_ns=157853773261 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=45346
[2026-10-19 16:48:15.846] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-10-19 16:48:16.393] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=48 source_master_ns=158898671221 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=90692
[2026-10-19 16:48:17.415] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=92 source_master_ns=159920349225 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=136038
[2026-10-19 16:48:18.437] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=136 source_master_ns=160942027230 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=181384
[2026-10-19 16:48:19.481] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=181 source_master_ns=161986925189 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=226730
[2026-10-19 16:48:19.811] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=726864859 ntp_seconds=4001388499 ntp_fraction=3483218477 arrival_ns=29113728276600
[2026-10-19 16:48:19.811] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=726864859
[2026-10-19 16:48:19.812] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-19 16:48:19.812] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=512842092 ntp_seconds=4001388499 ntp_fraction=3487513444 arrival_ns=29113729140200
[2026-10-19 16:48:19.812] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=512842092
[2026-10-19 16:48:19.812] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-19 16:48:20.503] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=225 source_master_ns=163008603193 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=272076
[2026-10-19 16:48:21.525] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=269 source_master_ns=164030281198 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=317422
[2026-10-19 16:48:22.549] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=313 source_master_ns=165051959202 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=362768
[2026-10-19 16:48:23.590] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=358 source_master_ns=166096857162 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=408114
[2026-10-19 16:48:24.612] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=402 source_master_ns=167118535166 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=453460
[2026-10-19 16:48:24.827] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=512842092 ntp_seconds=4001388504 ntp_fraction=3551937953 arrival_ns=29118744812200
[2026-10-19 16:48:24.828] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=512842092
[2026-10-19 16:48:24.828] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-19 16:48:24.842] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=726864859 ntp_seconds=4001388504 ntp_fraction=3616362463 arrival_ns=29118759168000
[2026-10-19 16:48:24.842] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=726864859
[2026-10-19 16:48:24.843] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-19 16:48:25.635] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=446 source_master_ns=168140213171 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=498806
[2026-10-19 16:48:26.680] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=491 source_master_ns=169185111130 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=544152
[2026-10-19 16:48:27.701] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=535 source_master_ns=170206789134 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=589498
[2026-10-19 16:48:28.723] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=579 source_master_ns=171228467139 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=634844
[2026-10-19 16:48:29.744] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=623 source_master_ns=172250145143 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=680190
[2026-10-19 16:48:29.834] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=512842092 ntp_seconds=4001388509 ntp_fraction=3582002724 arrival_ns=29123751395800
[2026-10-19 16:48:29.835] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=512842092
[2026-10-19 16:48:29.835] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-19 16:48:29.880] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=726864859 ntp_seconds=4001388509 ntp_fraction=3775276253 arrival_ns=29123796907300
[2026-10-19 16:48:29.880] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=726864859
[2026-10-19 16:48:29.880] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-19 16:48:30.789] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=668 source_master_ns=173295043103 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=725536
[2026-10-19 16:48:31.811] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=712 source_master_ns=174316721107 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=770882
[2026-10-19 16:48:32.834] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=756 source_master_ns=175338399112 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=816228
[2026-10-19 16:48:33.878] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=801 source_master_ns=176383297071 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=861574
[2026-10-19 16:48:34.840] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=512842092 ntp_seconds=4001388514 ntp_fraction=3603477561 arrival_ns=29128757032400
[2026-10-19 16:48:34.840] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=512842092
[2026-10-19 16:48:34.840] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-19 16:48:34.899] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=845 source_master_ns=177404975075 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=906920
[2026-10-19 16:48:34.923] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=726864859 ntp_seconds=4001388514 ntp_fraction=3964254814 arrival_ns=29128840465200
[2026-10-19 16:48:34.924] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=726864859
[2026-10-19 16:48:34.924] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-19 16:48:35.920] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=889 source_master_ns=178426653080 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=952266
[2026-10-19 16:48:36.942] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=933 source_master_ns=179448331085 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=997612
[2026-10-19 16:48:37.987] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=978 source_master_ns=180493229044 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1042958
[2026-10-19 16:48:39.009] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1022 source_master_ns=181514907048 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1088304
[2026-10-19 16:48:39.856] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=512842092 ntp_seconds=4001388519 ntp_fraction=3672197038 arrival_ns=29133772637600
[2026-10-19 16:48:39.856] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=512842092
[2026-10-19 16:48:39.856] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-19 16:48:39.940] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=726864859 ntp_seconds=4001388519 ntp_fraction=4032974290 arrival_ns=29133856654000
[2026-10-19 16:48:39.941] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=726864859
[2026-10-19 16:48:39.941] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-19 16:48:40.030] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1066 source_master_ns=182536585053 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1133650
[2026-10-19 16:48:41.077] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1111 source_master_ns=183581483012 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1178996
[2026-10-19 16:48:42.099] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1155 source_master_ns=184603161017 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1224342
[2026-10-19 16:48:43.119] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1199 source_master_ns=185624839021 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1269688
[2026-10-19 16:48:44.147] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1243 source_master_ns=186646517026 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1315034
[2026-10-19 16:48:44.857] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=512842092 ntp_seconds=4001388524 ntp_fraction=3680786972 arrival_ns=29138774827700
[2026-10-19 16:48:44.858] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=512842092
[2026-10-19 16:48:44.858] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-19 16:48:44.993] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=726864859 ntp_seconds=4001388524 ntp_fraction=4260607557 arrival_ns=29138910023900
[2026-10-19 16:48:44.993] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=726864859
[2026-10-19 16:48:44.993] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-19 16:48:45.186] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1288 source_master_ns=187691414985 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1360380
[2026-10-19 16:48:46.207] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1332 source_master_ns=188713092989 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1405726
[2026-10-19 16:48:47.228] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1376 source_master_ns=189734770994 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1451072
[2026-10-19 16:48:48.251] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1420 source_master_ns=190756448998 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1496418
[2026-10-19 16:48:49.300] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1465 source_master_ns=191801346958 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1541764
[2026-10-19 16:48:49.864] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=512842092 ntp_seconds=4001388529 ntp_fraction=3706556776 arrival_ns=29143781002500
[2026-10-19 16:48:49.864] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=512842092
[2026-10-19 16:48:49.864] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-19 16:48:50.006] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=726864859 ntp_seconds=4001388530 ntp_fraction=21474836 arrival_ns=29143922907700
[2026-10-19 16:48:50.006] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=726864859
[2026-10-19 16:48:50.006] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-19 16:48:50.317] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1509 source_master_ns=192823024962 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1587110
[2026-10-19 16:48:51.340] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1553 source_master_ns=193844702967 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1632456
[2026-10-19 16:48:52.384] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1598 source_master_ns=194889600926 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1677802
[2026-10-19 16:48:53.406] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1642 source_master_ns=195911278930 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1723148
[2026-10-19 16:48:54.427] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1686 source_master_ns=196932956935 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1768494
[2026-10-19 16:48:54.865] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=512842092 ntp_seconds=4001388534 ntp_fraction=3715146711 arrival_ns=29148782804900
[2026-10-19 16:48:54.866] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=512842092
[2026-10-19 16:48:54.866] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-19 16:48:55.019] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=726864859 ntp_seconds=4001388535 ntp_fraction=77309411 arrival_ns=29148935877500
[2026-10-19 16:48:55.019] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=726864859
[2026-10-19 16:48:55.019] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-19 16:48:55.451] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1730 source_master_ns=197954634939 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1813840
[2026-10-19 16:48:56.494] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1775 source_master_ns=198999532899 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1859186
[2026-10-19 16:48:57.521] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1819 source_master_ns=200021210903 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1904532
[2026-10-19 16:48:58.537] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1863 source_master_ns=201042888908 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1949878
[2026-10-19 16:48:59.581] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1908 source_master_ns=202087786867 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=1995224
[2026-10-19 16:48:59.896] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=512842092 ntp_seconds=4001388539 ntp_fraction=3848290697 arrival_ns=29153813302000
[2026-10-19 16:48:59.896] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=512842092
[2026-10-19 16:48:59.896] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-19 16:49:00.038] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=726864859 ntp_seconds=4001388540 ntp_fraction=163208757 arrival_ns=29153955223200
[2026-10-19 16:49:00.038] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=726864859
[2026-10-19 16:49:00.038] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-19 16:49:00.603] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1952 source_master_ns=203109464871 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2040570
[2026-10-19 16:49:01.626] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=1996 source_master_ns=204131142876 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2085916
[2026-10-19 16:49:02.648] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2040 source_master_ns=205152820880 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2131262
[2026-10-19 16:49:03.692] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2085 source_master_ns=206197718840 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2176608
[2026-10-19 16:49:04.714] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2129 source_master_ns=207219396844 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2221954
[2026-10-19 16:49:04.909] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=1 ssrc=512842092 ntp_seconds=4001388544 ntp_fraction=3899830304 arrival_ns=29158825863900
[2026-10-19 16:49:04.909] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=1 ssrc=512842092
[2026-10-19 16:49:04.909] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-19 16:49:05.041] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=1 ssrc=726864859 ntp_seconds=4001388545 ntp_fraction=171798691 arrival_ns=29158957935100
[2026-10-19 16:49:05.042] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=1 ssrc=726864859
[2026-10-19 16:49:05.042] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=2 invalidated_generation=none locked=1
[2026-10-19 16:49:05.735] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2173 source_master_ns=208241074849 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2267300
[2026-10-19 16:49:06.780] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2218 source_master_ns=209285972808 raw_phase_ns=13606 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2312646
[2026-10-19 16:49:07.801] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2262 source_master_ns=210307650812 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2357992
[2026-10-19 16:49:08.824] [info] [graph][runtime.node] av_drift_trace generation=2 sequence=2306 source_master_ns=211329328817 raw_phase_ns=13605 filtered_phase_ns=13605 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=2403338
[2026-10-19 16:49:09.345] [info] [graph][runtime.node] protocol_batch_trace stage=materialization_failed generation=1 attempts=10 consumed_datagrams=0 remaining_datagrams=1 attempted_master_ns=211212555200 mux_dequeued_master_ns=211132461600 first_dequeued_master_ns=211142716100 last_wire_published_master_ns=211142711700 previous_attempt_started_master_ns=211203165300 previous_attempt_completed_master_ns=211203191300 last_attempt_completed_master_ns=211212576900 last_blocked_started_master_ns=211203165300 last_blocked_completed_master_ns=211203191300 blocked_attempts=9 last_blocked_code=WouldBlock last_blocked_native=0 last_blocked_original_message_bytes=295 last_blocked_message_truncated=0 last_blocked_message="wire global sequence reservation exceeds planner backlog capacity serial=1 count=0 bytes=0 request_count=1 request_bytes=228 used_count=15 used_bytes=19964 limit_count=2155 limit_bytes=2920356 generation=1 projected_sequence=125131 head_id=22223 head_first=125081 head_count=50 head_committed=35" remaining_front_release_master_ns=211141883512 remaining_front_deadline_master_ns=211208550180 original_error="InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=211141883512 materialized_ns=211212555200 protocol_deadline_ns=211208550180 deadline_before_release=0 materialized_after_deadline=1"
[2026-10-19 16:49:09.345] [info] [graph][runtime.node] process.failed node=80 name=MediaMpegTsDatagramMaterializerNode kind=MpegTsDatagramMaterializer inputs=[channel=118 edge=118 stream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=120 edge=120 stream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=121 edge=121 stream=Metadata edge_kind=Unknown payload=Unknown queued=7] outputs=[channel=122 edge=122 stream=Metadata edge_kind=Unknown payload=Unknown queued=0] error=InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=211141883512 materialized_ns=211212555200 protocol_deadline_ns=211208550180 deadline_before_release=0 materialized_after_deadline=1
[2026-10-19 16:49:09.345] [info] [graph][runtime.node] worker.failed node=80 kind=MpegTsDatagramMaterializer name=MPEG-TS wire protocol materializer error=InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=211141883512 materialized_ns=211212555200 protocol_deadline_ns=211208550180 deadline_before_release=0 materialized_after_deadline=1
[2026-10-19 16:49:09.345] [info] [graph][runtime.node] worker.thread_cpu node=47 available=1 cpu_ns=453125000 process_calls=134375
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=76 available=1 cpu_ns=1203125000 process_calls=57274
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=27 available=1 cpu_ns=234375000 process_calls=89724
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=46 available=1 cpu_ns=500000000 process_calls=139915
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=9 available=1 cpu_ns=15625000 process_calls=78
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=28 available=1 cpu_ns=15625000 process_calls=79
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=48 available=1 cpu_ns=31250000 process_calls=219
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=10 available=1 cpu_ns=0 process_calls=104
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=49 available=1 cpu_ns=0 process_calls=323
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=11 available=1 cpu_ns=109375000 process_calls=10582
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=12 available=1 cpu_ns=0 process_calls=80
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=29 available=1 cpu_ns=0 process_calls=103
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=30 available=1 cpu_ns=125000000 process_calls=10603
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=31 available=1 cpu_ns=0 process_calls=82
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=51 available=1 cpu_ns=328125000 process_calls=22306
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=50 available=1 cpu_ns=203125000 process_calls=15503
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=52 available=1 cpu_ns=0 process_calls=218
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=13 available=1 cpu_ns=0 process_calls=105
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=32 available=1 cpu_ns=0 process_calls=104
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=53 available=1 cpu_ns=0 process_calls=304
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=14 available=1 cpu_ns=31250000 process_calls=10704
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=17 available=1 cpu_ns=0 process_calls=78
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=33 available=1 cpu_ns=93750000 process_calls=10770
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=36 available=1 cpu_ns=0 process_calls=79
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=54 available=1 cpu_ns=78125000 process_calls=15666
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=55 available=1 cpu_ns=203125000 process_calls=22466
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=59 available=1 cpu_ns=0 process_calls=213
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=15 available=1 cpu_ns=78125000 process_calls=10695
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=34 available=1 cpu_ns=62500000 process_calls=10727
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=56 available=1 cpu_ns=62500000 process_calls=15592
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=57 available=1 cpu_ns=156250000 process_calls=22421
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=16 available=1 cpu_ns=46875000 process_calls=10748
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=35 available=1 cpu_ns=31250000 process_calls=10773
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=58 available=1 cpu_ns=359375000 process_calls=38069
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=18 available=1 cpu_ns=78125000 process_calls=10654
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=37 available=1 cpu_ns=109375000 process_calls=10652
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=60 available=1 cpu_ns=265625000 process_calls=37509
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=19 available=1 cpu_ns=46875000 process_calls=10663
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=38 available=1 cpu_ns=31250000 process_calls=10661
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=61 available=1 cpu_ns=281250000 process_calls=37318
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=20 available=1 cpu_ns=46875000 process_calls=10610
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=62 available=1 cpu_ns=328125000 process_calls=37455
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=39 available=1 cpu_ns=93750000 process_calls=10641
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=22 available=1 cpu_ns=1062500000 process_calls=8839
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=41 available=1 cpu_ns=828125000 process_calls=9089
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=64 available=1 cpu_ns=1421875000 process_calls=20958
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=70 available=1 cpu_ns=578125000 process_calls=31962
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=23 available=1 cpu_ns=78125000 process_calls=9863
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=42 available=1 cpu_ns=78125000 process_calls=9905
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=80 available=1 cpu_ns=1812500000 process_calls=81006
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=66 available=1 cpu_ns=250000000 process_calls=14648
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=65 available=1 cpu_ns=156250000 process_calls=14662
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=26 available=1 cpu_ns=1296875000 process_calls=12331
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=72 available=1 cpu_ns=312500000 process_calls=22575
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=71 available=1 cpu_ns=156250000 process_calls=22618
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=5 available=1 cpu_ns=218750000 process_calls=26574
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=77 available=1 cpu_ns=15625000 process_calls=3
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=24 available=1 cpu_ns=109375000 process_calls=10067
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=74 available=1 cpu_ns=375000000 process_calls=45416
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=82 available=1 cpu_ns=7093750000 process_calls=43607
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=43 available=1 cpu_ns=125000000 process_calls=9928
[2026-10-19 16:49:09.355] [info] [graph][runtime.node] scheduled_datagram_sender stage=finished generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=22231 committed_datagrams=125130 committed_payload_bytes=147728148 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=80004 pacing_reserved=125130 pacing_cancelled=0 pacing_submitted=125130 pacing_maximum_submit_lateness_ns=29046240 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=0 backlog_current_wire_bytes=0 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=80004 backlog_max_residence_ns=73211000 last_materialized_sequence=125130 last_scheduled_sequence=125130 last_submitted_sequence=125130 last_committed_sequence=125130 shared_scope_charged_datagrams=125130 shared_scope_charged_wire_bytes=151231788 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2155 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2155 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=379902 endpoint_1_target_effective_socket_bytes=189951 endpoint_1_api_requested_socket_bytes=189951 endpoint_1_effective_socket_bytes=189951 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=125083 endpoint_1_committed_payload_bytes=147724952 endpoint_2_target_effective_socket_bytes=189951 endpoint_2_api_requested_socket_bytes=189951 endpoint_2_effective_socket_bytes=189951 endpoint_2_timestamp_source=0 endpoint_2_committed_datagrams=47 endpoint_2_committed_payload_bytes=3196
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=45 available=1 cpu_ns=1171875000 process_calls=12581
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=73 available=1 cpu_ns=609375000 process_calls=36218
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=25 available=1 cpu_ns=218750000 process_calls=18697
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=44 available=1 cpu_ns=125000000 process_calls=18660
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=67 available=1 cpu_ns=265625000 process_calls=23410
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=6 available=1 cpu_ns=1578125000 process_calls=36874
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=81 available=1 cpu_ns=0 process_calls=5
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=78 available=1 cpu_ns=0 process_calls=5
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=75 available=1 cpu_ns=187500000 process_calls=44880
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=4 available=1 cpu_ns=4359375000 process_calls=27531
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=79 available=1 cpu_ns=156250000 process_calls=44797
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=7 available=1 cpu_ns=1015625000 process_calls=34956
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=8 available=1 cpu_ns=328125000 process_calls=88683
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] process.failed node=2 name=VideoEncodeNode kind=VideoEncode inputs=[channel=1 edge=1 stream=Video edge_kind=Metadata payload=CodecContext queued=0; channel=104 edge=104 stream=Video edge_kind=RawFrame payload=Frame queued=0] outputs=[channel=106 edge=106 stream=Video edge_kind=EncodedPacket payload=Packet queued=0; channel=113 edge=113 stream=Video edge_kind=Metadata payload=CodecContext queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-19 16:49:09.357] [info] [graph][runtime.node] worker.thread_cpu node=2 available=1 cpu_ns=2765625000 process_calls=18515
[2026-10-19 16:49:09.346] [info] [graph][runtime.node] worker.thread_cpu node=68 available=1 cpu_ns=1218750000 process_calls=17707
[2026-10-19 16:49:09.400] [info] [graph][runtime.lifecycle] node_abort_payload node=82 before_objects=49 after_objects=49 before_bytes=244878 after_bytes=244878 before_reservations=194748 after_reservations=194748 before_releases=194699 after_releases=194699
[2026-10-19 16:49:09.400] [info] [graph][runtime.node] mpegts_wire_materializer stage=aborted maximum_materialized_after_release_ns=57861172 worst_materialized_at_ns=93666409000 worst_release_ns=93608547828 worst_deadline_ns=93675214496 worst_global_sequence=62846
[2026-10-19 16:49:09.401] [info] [graph][runtime.lifecycle] node_abort_payload node=80 before_objects=49 after_objects=49 before_bytes=244878 after_bytes=244878 before_reservations=194748 after_reservations=194748 before_releases=194699 after_releases=194699
[2026-10-19 16:49:09.401] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 datagrams=0 wire_bytes=0 immediate_deadlines=0 deferred_deadlines=0 planned_wait_ns=0 late_datagrams=0 maximum_lateness_ns=0 pending_bytes=0 peak_pending_bytes=110920 pressure_failures=0 access_units=15025 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=4106 maximum_mux_input_after_emit_ns=16541299 maximum_mux_input_after_dispatch_ns=-83458701 worst_mux_input_ready_ns=103358420700 worst_mux_input_emit_ns=103341879401 worst_mux_input_dispatch_ns=103441879401 maximum_protocol_batch_produced_after_release_ns=17331380 worst_protocol_batch_produced_ns=103225879400 worst_protocol_batch_release_ns=103208548020 worst_protocol_batch_deadline_ns=103275214688 worst_protocol_batch_datagrams=1 exit_reason=aborted
[2026-10-19 16:49:09.401] [info] [graph][runtime.lifecycle] node_abort_payload node=76 before_objects=49 after_objects=49 before_bytes=244878 after_bytes=244878 before_reservations=194748 after_reservations=194748 before_releases=194699 after_releases=194699
[2026-10-19 16:49:09.401] [info] [graph][runtime.lifecycle] node_abort_payload node=79 before_objects=49 after_objects=49 before_bytes=244878 after_bytes=244878 before_reservations=194748 after_reservations=194748 before_releases=194699 after_releases=194699
[2026-10-19 16:49:09.401] [info] [graph][runtime.lifecycle] node_abort_payload node=75 before_objects=49 after_objects=49 before_bytes=244878 after_bytes=244878 before_reservations=194748 after_reservations=194748 before_releases=194699 after_releases=194699
[2026-10-19 16:49:09.401] [info] [graph][runtime.node] audio_stage_observation node=74 stage=scheduler_input count=8856 last_generation=1 last_begin=9067520 last_end=9068544 last_rate=44100 last_steady_ns=29163252732900 maximum_inter_event_gap_ns=44768800 gap_begin_generation=1 gap_begin_begin=8176640 gap_begin_end=8177664 gap_begin_rate=44100 gap_begin_steady_ns=29143051154300 gap_end_generation=1 gap_end_begin=8177664 gap_end_end=8178688 gap_end_rate=44100 gap_end_steady_ns=29143095923100
[2026-10-19 16:49:09.401] [info] [graph][runtime.node] av_scheduler_correlated_wait generation=1 canonical_sequence=5564 missing=audio begin_steady_ns=29143051190700 end_steady_ns=29143095924600 observed_wait_ns=44733900 held_dispatch_master_ns=191141879401 begin_master_ns=191001226000 end_master_ns=191045959500 dispatch_remaining_at_end_ns=95919901 resumed_dispatch_master_ns=191143103891
[2026-10-19 16:49:09.401] [info] [graph][runtime.lifecycle] node_abort_payload node=74 before_objects=49 after_objects=48 before_bytes=244878 after_bytes=205789 before_reservations=194748 after_reservations=194748 before_releases=194699 after_releases=194700
[2026-10-19 16:49:09.402] [info] [graph][runtime.node] audio_stage_observation node=5 stage=canonicalizer_input count=8856 last_generation=1 last_begin=9067520 last_end=9068544 last_rate=44100 last_steady_ns=29163252714600 maximum_inter_event_gap_ns=44770100 gap_begin_generation=1 gap_begin_begin=8176640 gap_begin_end=8177664 gap_begin_rate=44100 gap_begin_steady_ns=29143051081700 gap_end_generation=1 gap_end_begin=8177664 gap_end_end=8178688 gap_end_rate=44100 gap_end_steady_ns=29143095851800
[2026-10-19 16:49:09.402] [info] [graph][runtime.node] audio_stage_observation node=5 stage=canonicalizer_committed count=8856 last_generation=1 last_begin=9067520 last_end=9068544 last_rate=44100 last_steady_ns=29163252721800 maximum_inter_event_gap_ns=44762200 gap_begin_generation=1 gap_begin_begin=8176640 gap_begin_end=8177664 gap_begin_rate=44100 gap_begin_steady_ns=29143051096700 gap_end_generation=1 gap_end_begin=8177664 gap_end_end=8178688 gap_end_rate=44100 gap_end_steady_ns=29143095858900
[2026-10-19 16:49:09.402] [info] [graph][runtime.lifecycle] node_abort_payload node=5 before_objects=48 after_objects=48 before_bytes=205789 after_bytes=205789 before_reservations=194748 after_reservations=194748 before_releases=194700 after_releases=194700
[2026-10-19 16:49:09.403] [info] [graph][runtime.lifecycle] node_abort_payload node=77 before_objects=48 after_objects=48 before_bytes=205789 after_bytes=205789 before_reservations=194748 after_reservations=194748 before_releases=194700 after_releases=194700
[2026-10-19 16:49:09.403] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_input count=8858 last_generation=1 last_begin=9069568 last_end=9070592 last_rate=44100 last_steady_ns=29163251855900 maximum_inter_event_gap_ns=44830100 gap_begin_generation=1 gap_begin_begin=8178688 gap_begin_end=8179712 gap_begin_rate=44100 gap_begin_steady_ns=29143050447400 gap_end_generation=1 gap_end_begin=8179712 gap_end_end=8180736 gap_end_rate=44100 gap_end_steady_ns=29143095277500
[2026-10-19 16:49:09.403] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_submitted count=8858 last_generation=1 last_begin=9069568 last_end=9070592 last_rate=44100 last_steady_ns=29163252641000 maximum_inter_event_gap_ns=44731500 gap_begin_generation=1 gap_begin_begin=8178688 gap_begin_end=8179712 gap_begin_rate=44100 gap_begin_steady_ns=29143050949100 gap_end_generation=1 gap_end_begin=8179712 gap_end_end=8180736 gap_end_rate=44100 gap_end_steady_ns=29143095680600
[2026-10-19 16:49:09.403] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_packet count=8856 last_generation=1 last_begin=9067520 last_end=9068544 last_rate=44100 last_steady_ns=29163252652100 maximum_inter_event_gap_ns=44727300 gap_begin_generation=1 gap_begin_begin=8176640 gap_begin_end=8177664 gap_begin_rate=44100 gap_begin_steady_ns=29143050961600 gap_end_generation=1 gap_end_begin=8177664 gap_end_end=8178688 gap_end_rate=44100 gap_end_steady_ns=29143095688900
[2026-10-19 16:49:09.403] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_committed count=8856 last_generation=1 last_begin=9067520 last_end=9068544 last_rate=44100 last_steady_ns=29163252660500 maximum_inter_event_gap_ns=44722100 gap_begin_generation=1 gap_begin_begin=8176640 gap_begin_end=8177664 gap_begin_rate=44100 gap_begin_steady_ns=29143050974300 gap_end_generation=1 gap_end_begin=8177664 gap_end_end=8178688 gap_end_rate=44100 gap_end_steady_ns=29143095696400
[2026-10-19 16:49:09.403] [info] [graph][runtime.lifecycle] codec_owner_release node=4 kind=AudioEncode owner_references=4
[2026-10-19 16:49:09.403] [info] [graph][runtime.lifecycle] node_abort_payload node=4 before_objects=48 after_objects=48 before_bytes=205789 after_bytes=205789 before_reservations=194748 after_reservations=194748 before_releases=194700 after_releases=194700
[2026-10-19 16:49:09.403] [info] [graph][runtime.lifecycle] node_abort_payload node=78 before_objects=48 after_objects=48 before_bytes=205789 after_bytes=205789 before_reservations=194748 after_reservations=194748 before_releases=194700 after_releases=194700
[2026-10-19 16:49:09.403] [info] [graph][runtime.lifecycle] node_abort_payload node=81 before_objects=48 after_objects=48 before_bytes=205789 after_bytes=205789 before_reservations=194748 after_reservations=194748 before_releases=194700 after_releases=194700
[2026-10-19 16:49:09.403] [info] [graph][runtime.node] audio_stage_observation node=7 stage=aggregate_built count=8858 last_generation=1 last_begin=9069568 last_end=9070592 last_rate=44100 last_steady_ns=29163251656000 maximum_inter_event_gap_ns=42421400 gap_begin_generation=1 gap_begin_begin=8178688 gap_begin_end=8179712 gap_begin_rate=44100 gap_begin_steady_ns=29143050393500 gap_end_generation=1 gap_end_begin=8179712 gap_end_end=8180736 gap_end_rate=44100 gap_end_steady_ns=29143092814900
[2026-10-19 16:49:09.404] [info] [graph][runtime.node] audio_stage_observation node=7 stage=aggregate_committed count=8858 last_generation=1 last_begin=9069568 last_end=9070592 last_rate=44100 last_steady_ns=29163251761400 maximum_inter_event_gap_ns=42413200 gap_begin_generation=1 gap_begin_begin=8178688 gap_begin_end=8179712 gap_begin_rate=44100 gap_begin_steady_ns=29143050416000 gap_end_generation=1 gap_end_begin=8179712 gap_end_end=8180736 gap_end_rate=44100 gap_end_steady_ns=29143092829200
[2026-10-19 16:49:09.404] [info] [graph][runtime.node] continuous_audio_aggregate.summary reason=abort output_group=composition-r272.output audio_commits=8858 real_samples=7384064 silence_samples=1686528
[2026-10-19 16:49:09.404] [info] [graph][runtime.lifecycle] node_abort_payload node=7 before_objects=48 after_objects=42 before_bytes=205789 after_bytes=156637 before_reservations=194748 after_reservations=194748 before_releases=194700 after_releases=194706
[2026-10-19 16:49:09.404] [info] [graph][runtime.lifecycle] codec_owner_release node=2 kind=VideoEncode owner_references=4
[2026-10-19 16:49:09.404] [info] [graph][runtime.node] video_encoder_execution_observation node=2 count=6171 maximum_call_ns=17101400 maximum_begin_steady_ns=28976030128800 maximum_end_steady_ns=28976047230200 maximum_encoder_pts=553 maximum_native_result=0 maximum_between_submissions_ns=59127200 before_gap_begin_steady_ns=29004426447900 before_gap_end_steady_ns=29004430332900 before_gap_encoder_pts=1405 before_gap_native_result=0 after_gap_begin_steady_ns=29004489460100 after_gap_end_steady_ns=29004493810900 after_gap_encoder_pts=1406 after_gap_native_result=0
[2026-10-19 16:49:09.404] [info] [graph][runtime.lifecycle] node_abort_payload node=2 before_objects=42 after_objects=42 before_bytes=156637 after_bytes=156637 before_reservations=194748 after_reservations=194748 before_releases=194706 after_releases=194706
[2026-10-19 16:49:09.404] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=process count=36874 maximum_ns=31343000 begin_steady_ns=29004458720600 end_steady_ns=29004490063600 video_cursor=1406 audio_cursor=not_owned next_video_cursor=1406 next_audio_cursor=not_owned error=None return_state=progress deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-10-19 16:49:09.404] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=purge count=36874 maximum_ns=2384900 begin_steady_ns=29071528257600 end_steady_ns=29071530642500 video_cursor=3419 audio_cursor=not_owned next_video_cursor=3419 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 16:49:09.405] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=bind count=36874 maximum_ns=161700 begin_steady_ns=29115492700600 end_steady_ns=29115492862300 video_cursor=4737 audio_cursor=not_owned next_video_cursor=4737 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 16:49:09.405] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=consume count=36874 maximum_ns=26266400 begin_steady_ns=28965830083100 end_steady_ns=28965856349500 video_cursor=248 audio_cursor=not_owned next_video_cursor=248 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 16:49:09.405] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=activate count=23 maximum_ns=742400 begin_steady_ns=28957591843500 end_steady_ns=28957592585900 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 16:49:09.405] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=build_video count=6171 maximum_ns=30454800 begin_steady_ns=29004458737100 end_steady_ns=29004489191900 video_cursor=1406 audio_cursor=not_owned next_video_cursor=1406 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 16:49:09.405] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=canvas count=6171 maximum_ns=30417000 begin_steady_ns=29004458745500 end_steady_ns=29004489162500 video_cursor=1406 audio_cursor=not_owned next_video_cursor=1406 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 16:49:09.405] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=emit count=6171 maximum_ns=19986400 begin_steady_ns=28976030081100 end_steady_ns=28976050067500 video_cursor=553 audio_cursor=not_owned next_video_cursor=553 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 16:49:09.405] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=between_calls_previous_return count=36873 maximum_ns=156188700 begin_steady_ns=28957431773700 end_steady_ns=28957587962400 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=waiting deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-10-19 16:49:09.405] [info] [graph][runtime.node] continuous_aggregate.summary reason=abort output_group=composition-r272.output output_generation=1 video_commits=6171 source[0].group=composition-r272.source.0 source[0].real_frames=3418 source[0].black_frames=2753 source[0].contribution_generation=0 source[1].group=composition-r272.source.1 source[1].real_frames=3418 source[1].black_frames=2753 source[1].contribution_generation=0 source[2].group=composition-r272.source.2 source[2].real_frames=5023 source[2].black_frames=1148 source[2].contribution_generation=2 source[2].first_interval_miss_cursor=4565 source[2].first_interval_miss_target_ns=157875212734 source[2].first_interval_miss_steady_ns=29109758615600 source[2].first_interval_miss_generation=2 source[2].first_interval_miss_capacity=7 source[2].first_interval_miss_before=0 source[2].first_interval_miss_after=0 source[2].first_interval_miss_expired=0 source[2].first_interval_miss_channel_depth=0 source[2].first_interval_miss_examined_begin_ns=unavailable source[2].first_interval_miss_examined_end_ns=unavailable
[2026-10-19 16:49:09.406] [info] [graph][runtime.node] cuda_canvas_call stage=context_push count=5027 operation=copy maximum_ns=445400 begin_steady_ns=29062125487900 end_steady_ns=29062125933300 native_result=0 thread_cpu_ns=0
[2026-10-19 16:49:09.406] [info] [graph][runtime.node] cuda_canvas_call stage=source_wait count=16888 operation=copy maximum_ns=355200 begin_steady_ns=29046658807600 end_steady_ns=29046659162800 native_result=0 thread_cpu_ns=0
[2026-10-19 16:49:09.406] [info] [graph][runtime.node] cuda_canvas_call stage=plane_copy count=33778 operation=copy maximum_ns=4385600 begin_steady_ns=29126026756300 end_steady_ns=29126031141900 native_result=0 thread_cpu_ns=0
[2026-10-19 16:49:09.406] [info] [graph][runtime.node] cuda_canvas_call stage=destination_wait count=2 operation=upload maximum_ns=72200 begin_steady_ns=28957348148500 end_steady_ns=28957348220700 native_result=0 thread_cpu_ns=0
[2026-10-19 16:49:09.406] [info] [graph][runtime.node] cuda_canvas_call stage=context_pop count=5027 operation=copy maximum_ns=866600 begin_steady_ns=29047242028200 end_steady_ns=29047242894800 native_result=0 thread_cpu_ns=0
[2026-10-19 16:49:09.406] [info] [graph][runtime.node] cuda_canvas_call stage=batch_completion count=5025 operation=copy maximum_ns=30229700 begin_steady_ns=29004458898500 end_steady_ns=29004489128200 native_result=0 thread_cpu_ns=0
[2026-10-19 16:49:09.406] [info] [graph][runtime.node] canvas_compose_correlated scope=attempt begin_steady_ns=29004458746000 end_steady_ns=29004489161800 wall_ns=30415800 thread_cpu_ns=0 surface_count=1 surface_total_ns=700 surface_maximum_ns=700 surface_maximum_tile=none surface_maximum_result=unavailable validate_count=3 validate_total_ns=1800 validate_maximum_ns=900 validate_maximum_tile=0 validate_maximum_result=0 black_count=1 black_total_ns=96300 black_maximum_ns=96300 black_maximum_tile=none black_maximum_result=0 tile_count=3 tile_total_ns=44100 tile_maximum_ns=21800 tile_maximum_tile=0 tile_maximum_result=0 publish_count=1 publish_total_ns=14700 publish_maximum_ns=14700 publish_maximum_tile=none publish_maximum_result=0 context_push_count=1 context_push_total_ns=5200 context_push_maximum_ns=5200 context_push_maximum_tile=none context_push_maximum_result=0 source_wait_count=4 source_wait_total_ns=3200 source_wait_maximum_ns=2500 source_wait_maximum_tile=0 source_wait_maximum_result=0 plane_copy_count=8 plane_copy_total_ns=122200 plane_copy_maximum_ns=81200 plane_copy_maximum_tile=none plane_copy_maximum_result=0 destination_wait_count=0 destination_wait_total_ns=0 destination_wait_maximum_ns=0 destination_wait_maximum_tile=none destination_wait_maximum_result=unavailable context_pop_count=1 context_pop_total_ns=13900 context_pop_maximum_ns=13900 context_pop_maximum_tile=none context_pop_maximum_result=0 batch_completion_count=1 batch_completion_total_ns=30226600 batch_completion_maximum_ns=30226600 batch_completion_maximum_tile=none batch_completion_maximum_result=0 source_query_completed_count=0 source_query_ready_count=3 source_query_ready_maximum_ns=7000 source_query_ready_begin_steady_ns=29004458854200 source_query_ready_end_steady_ns=29004458861200 source_query_ready_tile=0 source_query_ready_operation=event_query source_query_ready_native=0 source_query_ready_restore_native=0 source_query_pending_count=0 source_query_unavailable_count=0 source_query_error_count=0
[2026-10-19 16:49:09.406] [info] [graph][runtime.lifecycle] node_abort_payload node=6 before_objects=42 after_objects=28 before_bytes=156637 after_bytes=156637 before_reservations=194748 after_reservations=194748 before_releases=194706 after_releases=194720
[2026-10-19 16:49:09.406] [info] [graph][runtime.lifecycle] node_abort_payload node=67 before_objects=28 after_objects=28 before_bytes=156637 after_bytes=156637 before_reservations=194748 after_reservations=194748 before_releases=194720 after_releases=194720
[2026-10-19 16:49:09.407] [info] [graph][runtime.lifecycle] node_abort_payload node=44 before_objects=28 after_objects=28 before_bytes=156637 after_bytes=156637 before_reservations=194748 after_reservations=194748 before_releases=194720 after_releases=194720
[2026-10-19 16:49:09.407] [info] [graph][runtime.lifecycle] node_abort_payload node=25 before_objects=28 after_objects=28 before_bytes=156637 after_bytes=156637 before_reservations=194748 after_reservations=194748 before_releases=194720 after_releases=194720
[2026-10-19 16:49:09.407] [info] [graph][runtime.lifecycle] codec_owner_release node=73 kind=AudioResample owner_references=1
[2026-10-19 16:49:09.407] [info] [graph][runtime.lifecycle] node_abort_payload node=73 before_objects=28 after_objects=26 before_bytes=156637 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194720 after_releases=194722
[2026-10-19 16:49:09.413] [info] [graph][runtime.lifecycle] node_abort_payload node=68 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.418] [info] [graph][runtime.lifecycle] node_abort_payload node=45 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.423] [info] [graph][runtime.lifecycle] node_abort_payload node=26 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.423] [info] [graph][runtime.lifecycle] node_abort_payload node=72 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.424] [info] [graph][runtime.lifecycle] node_abort_payload node=66 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.424] [info] [graph][runtime.lifecycle] node_abort_payload node=43 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.424] [info] [graph][runtime.lifecycle] node_abort_payload node=24 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.424] [info] [graph][runtime.lifecycle] node_abort_payload node=71 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.424] [info] [graph][runtime.lifecycle] node_abort_payload node=65 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.424] [info] [graph][runtime.lifecycle] node_abort_payload node=42 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.425] [info] [graph][runtime.lifecycle] node_abort_payload node=23 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.425] [info] [graph][runtime.lifecycle] codec_owner_release node=70 kind=AudioDecode owner_references=2
[2026-10-19 16:49:09.425] [info] [graph][runtime.lifecycle] node_abort_payload node=70 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.425] [info] [graph][runtime.lifecycle] codec_owner_release node=64 kind=VideoDecode owner_references=2
[2026-10-19 16:49:09.435] [info] [graph][runtime.lifecycle] node_abort_payload node=64 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.435] [info] [graph][runtime.lifecycle] codec_owner_release node=41 kind=VideoDecode owner_references=2
[2026-10-19 16:49:09.445] [info] [graph][runtime.lifecycle] node_abort_payload node=41 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.445] [info] [graph][runtime.lifecycle] codec_owner_release node=22 kind=VideoDecode owner_references=2
[2026-10-19 16:49:09.457] [info] [graph][runtime.lifecycle] node_abort_payload node=22 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.457] [info] [graph][runtime.lifecycle] node_abort_payload node=62 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.457] [info] [graph][runtime.lifecycle] node_abort_payload node=39 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.457] [info] [graph][runtime.lifecycle] node_abort_payload node=20 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.457] [info] [graph][runtime.lifecycle] node_abort_payload node=61 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.457] [info] [graph][runtime.lifecycle] node_abort_payload node=38 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.457] [info] [graph][runtime.lifecycle] node_abort_payload node=19 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.457] [info] [graph][runtime.lifecycle] node_abort_payload node=60 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.457] [info] [graph][runtime.lifecycle] node_abort_payload node=37 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.458] [info] [graph][runtime.lifecycle] node_abort_payload node=18 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.458] [info] [graph][runtime.lifecycle] node_abort_payload node=58 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.458] [info] [graph][runtime.lifecycle] node_abort_payload node=35 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.458] [info] [graph][runtime.lifecycle] node_abort_payload node=16 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.458] [info] [graph][runtime.lifecycle] node_abort_payload node=57 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.458] [info] [graph][runtime.lifecycle] node_abort_payload node=56 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.458] [info] [graph][runtime.lifecycle] node_abort_payload node=34 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.458] [info] [graph][runtime.lifecycle] node_abort_payload node=15 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.458] [info] [graph][runtime.lifecycle] node_abort_payload node=59 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.458] [info] [graph][runtime.lifecycle] node_abort_payload node=55 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.458] [info] [graph][runtime.lifecycle] node_abort_payload node=54 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.458] [info] [graph][runtime.lifecycle] node_abort_payload node=36 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.458] [info] [graph][runtime.lifecycle] node_abort_payload node=33 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.458] [info] [graph][runtime.lifecycle] node_abort_payload node=17 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.459] [info] [graph][runtime.lifecycle] node_abort_payload node=14 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.459] [info] [graph][runtime.lifecycle] node_abort_payload node=53 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.459] [info] [graph][runtime.lifecycle] node_abort_payload node=32 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.459] [info] [graph][runtime.lifecycle] node_abort_payload node=13 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.459] [info] [graph][runtime.lifecycle] node_abort_payload node=52 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.459] [info] [graph][runtime.lifecycle] node_abort_payload node=51 before_objects=26 after_objects=26 before_bytes=140253 after_bytes=140253 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194722
[2026-10-19 16:49:09.459] [info] [graph][runtime.lifecycle] node_abort_payload node=50 before_objects=26 after_objects=25 before_bytes=140253 after_bytes=106496 before_reservations=194748 after_reservations=194748 before_releases=194722 after_releases=194723
[2026-10-19 16:49:09.459] [info] [graph][runtime.lifecycle] node_abort_payload node=31 before_objects=25 after_objects=25 before_bytes=106496 after_bytes=106496 before_reservations=194748 after_reservations=194748 before_releases=194723 after_releases=194723
[2026-10-19 16:49:09.459] [info] [graph][runtime.lifecycle] node_abort_payload node=30 before_objects=25 after_objects=25 before_bytes=106496 after_bytes=106496 before_reservations=194748 after_reservations=194748 before_releases=194723 after_releases=194723
[2026-10-19 16:49:09.459] [info] [graph][runtime.lifecycle] node_abort_payload node=12 before_objects=25 after_objects=25 before_bytes=106496 after_bytes=106496 before_reservations=194748 after_reservations=194748 before_releases=194723 after_releases=194723
[2026-10-19 16:49:09.459] [info] [graph][runtime.lifecycle] node_abort_payload node=11 before_objects=25 after_objects=25 before_bytes=106496 after_bytes=106496 before_reservations=194748 after_reservations=194748 before_releases=194723 after_releases=194723
[2026-10-19 16:49:09.459] [info] [graph][runtime.lifecycle] node_abort_payload node=49 before_objects=25 after_objects=25 before_bytes=106496 after_bytes=106496 before_reservations=194748 after_reservations=194748 before_releases=194723 after_releases=194723
[2026-10-19 16:49:09.459] [info] [graph][runtime.lifecycle] node_abort_payload node=29 before_objects=25 after_objects=25 before_bytes=106496 after_bytes=106496 before_reservations=194748 after_reservations=194748 before_releases=194723 after_releases=194723
[2026-10-19 16:49:09.459] [info] [graph][runtime.lifecycle] node_abort_payload node=10 before_objects=25 after_objects=25 before_bytes=106496 after_bytes=106496 before_reservations=194748 after_reservations=194748 before_releases=194723 after_releases=194723
[2026-10-19 16:49:09.459] [info] [graph][runtime.lifecycle] node_abort_payload node=69 before_objects=25 after_objects=25 before_bytes=106496 after_bytes=106496 before_reservations=194748 after_reservations=194748 before_releases=194723 after_releases=194723
[2026-10-19 16:49:09.460] [info] [graph][runtime.lifecycle] node_abort_payload node=48 before_objects=25 after_objects=25 before_bytes=106496 after_bytes=106496 before_reservations=194748 after_reservations=194748 before_releases=194723 after_releases=194723
[2026-10-19 16:49:09.460] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-10-19 16:49:09.460] [info] [graph][runtime.lifecycle] node_abort_payload node=63 before_objects=25 after_objects=25 before_bytes=106496 after_bytes=106496 before_reservations=194748 after_reservations=194748 before_releases=194723 after_releases=194723
[2026-10-19 16:49:09.460] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-10-19 16:49:09.460] [info] [graph][runtime.lifecycle] node_abort_payload node=40 before_objects=25 after_objects=25 before_bytes=106496 after_bytes=106496 before_reservations=194748 after_reservations=194748 before_releases=194723 after_releases=194723
[2026-10-19 16:49:09.460] [info] [graph][runtime.lifecycle] node_abort_payload node=28 before_objects=25 after_objects=25 before_bytes=106496 after_bytes=106496 before_reservations=194748 after_reservations=194748 before_releases=194723 after_releases=194723
[2026-10-19 16:49:09.460] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-10-19 16:49:09.460] [info] [graph][runtime.lifecycle] node_abort_payload node=21 before_objects=25 after_objects=25 before_bytes=106496 after_bytes=106496 before_reservations=194748 after_reservations=194748 before_releases=194723 after_releases=194723
[2026-10-19 16:49:09.460] [info] [graph][runtime.lifecycle] node_abort_payload node=9 before_objects=25 after_objects=25 before_bytes=106496 after_bytes=106496 before_reservations=194748 after_reservations=194748 before_releases=194723 after_releases=194723
[2026-10-19 16:49:09.460] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=7539 completion_waits=7539 completed_datagrams=7617 drained_datagrams=96 would_block=18 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-19 16:49:09.460] [info] [graph][runtime.lifecycle] node_abort_payload node=47 before_objects=25 after_objects=25 before_bytes=106496 after_bytes=106496 before_reservations=194748 after_reservations=194748 before_releases=194723 after_releases=194723
[2026-10-19 16:49:09.460] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=116424 completion_waits=116424 completed_datagrams=154690 drained_datagrams=38284 would_block=18 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-19 16:49:09.461] [info] [graph][runtime.lifecycle] node_abort_payload node=46 before_objects=25 after_objects=25 before_bytes=106496 after_bytes=106496 before_reservations=194748 after_reservations=194748 before_releases=194723 after_releases=194723
[2026-10-19 16:49:09.461] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=82775 completion_waits=82775 completed_datagrams=105379 drained_datagrams=22649 would_block=45 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-19 16:49:09.461] [info] [graph][runtime.lifecycle] node_abort_payload node=27 before_objects=25 after_objects=25 before_bytes=106496 after_bytes=106496 before_reservations=194748 after_reservations=194748 before_releases=194723 after_releases=194723
[2026-10-19 16:49:09.461] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=85458 completion_waits=85458 completed_datagrams=105395 drained_datagrams=19983 would_block=45 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-19 16:49:09.462] [info] [graph][runtime.lifecycle] node_abort_payload node=8 before_objects=25 after_objects=25 before_bytes=106496 after_bytes=106496 before_reservations=194748 after_reservations=194748 before_releases=194723 after_releases=194723
[2026-10-19 16:49:09.462] [info] [graph][runtime.lifecycle] node_abort_payload node=3 before_objects=25 after_objects=25 before_bytes=106496 after_bytes=106496 before_reservations=194748 after_reservations=194748 before_releases=194723 after_releases=194723
[2026-10-19 16:49:09.462] [info] [graph][runtime.node] codec_resolver.release slot=encoder_parameters bufferReferences=1 hasCredit=0 creditReferences=0
[2026-10-19 16:49:09.462] [info] [graph][runtime.lifecycle] node_abort_payload node=1 before_objects=25 after_objects=25 before_bytes=106496 after_bytes=106496 before_reservations=194748 after_reservations=194748 before_releases=194723 after_releases=194723
[2026-10-19 16:49:09.528] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=failed detail="protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=211141883512 materialized_ns=211212555200 protocol_deadline_ns=211208550180 deadline_before_release=0 materialized_after_deadline=1" description="D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r272-output.sdp" error=1 native=0 stage=3 reason="protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=211141883512 materialized_ns=211212555200 protocol_deadline_ns=211208550180 deadline_before_release=0 materialized_after_deadline=1"
[2026-10-19 16:49:09.528] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=retired detail="protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=211141883512 materialized_ns=211212555200 protocol_deadline_ns=211208550180 deadline_before_release=0 materialized_after_deadline=1" description="D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r272-output.sdp" error=1 native=0 stage=3 reason="protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=211141883512 materialized_ns=211212555200 protocol_deadline_ns=211208550180 deadline_before_release=0 materialized_after_deadline=1"
[CLI] final runtime report: queued=0, peakQueued=910, threads=82, processThreads=60, workers=0, workerProgress=436595, workerProcessCalls=1654821, workerWaits=1218218, workerWakeups=1202422, workerErrors=1, errors=1, stalledIntervals=0, cpuSamples=645, logicalProcessors=22, averageSystemMachineCpuPercent=7.217084, averageProcessMachineCpuPercent=1.680122, peakProcessMachineCpuPercent=5.614035, averageProcessSingleCoreCpuPercent=36.962683, peakProcessSingleCoreCpuPercent=123.508772, initialWorkingSetBytes=209002496, workingSetBytes=259006464, peakWorkingSetBytes=268472320, totalPushed=398198, totalPopped=398148, droppedBuffers=0, encodedPacketsPushed=68957, encodedPacketsPopped=68957, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=17324300, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=748, graphPayloadReservations=194748, graphPayloadReleases=194748, graphPayloadPressureFailures=0, lastInputReceivedAtNs=29163252673300
~~~
### 标准错误
~~~text
[in @ 0000020aeb3a8dc0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000020aeb3a8dc0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 33.400444
[in @ 0000020aeb4f3300] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000020aeb4f3300] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 17145.903744
[in @ 0000020aeb4f3a80] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000020aeb4f3a80] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 18900.763633
[in @ 0000020af101ad80] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000020aeb532680] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000020aeb532680] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 33.400444
[in @ 0000020af101ad80] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 18900.763633
[in @ 0000020af1225ec0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000020af1225ec0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 17145.903744
[aac @ 0000020aeb391c40] Qavg: 12666.987
[aac @ 0000020aeb391c40] 2 frames left in the queue on closing
[CLI] realtime video runtime failed: InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=211141883512 materialized_ns=211212555200 protocol_deadline_ns=211208550180 deadline_before_release=0 materialized_after_deadline=1

~~~

四次源均完整120秒/3600帧、退出0；CLI1，VLC经RC quit结束。
~~~text
source0
[out#1/rtp @ 0000024a921dca40] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.49    
source1
[out#0/rtp @ 00000230afa37180] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
source2
[out#0/rtp @ 00000118aec89700] video:117386KiB audio:0KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 1.212977%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
reentry
[out#1/rtp @ 000001c1e7ddf240] video:0KiB audio:4026KiB subtitle:0KiB other streams:0KiB global headers:0KiB muxing overhead: 2.023504%
frame= 3600 fps= 30 q=-1.0 Lsize=  118810KiB time=00:02:00.00 bitrate=8110.8kbits/s speed=   1x elapsed=0:01:59.48    
~~~
清理清单：
composition-r272-cli-error.log
composition-r272-cli.log
composition-r272-monitor.log
composition-r272-native-2026-10-19-16h46m05s730.png
composition-r272-native-2026-10-19-16h48m04s625.png
composition-r272-native-2026-10-19-16h48m45s806.png
composition-r272-output.sdp
composition-r272-reentry-stdout.log
composition-r272-reentry.log
composition-r272-source0-stdout.log
composition-r272-source0.log
composition-r272-source1-stdout.log
composition-r272-source1.log
composition-r272-source2-stdout.log
composition-r272-source2.log
composition-r272-vlc.log

清理完成：上述16文件已逐项删除，六个精确PID均无残留；复用源保留，未运行远程测试。
