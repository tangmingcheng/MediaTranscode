# r294：附加调试诱发截止失败及异常清理

原规格三源H.264→HEVC/AAC MPEG-TS/UDP，1280×72030、8000kbps CBR/GOP60、AAC192kbps CBR。仅诊断，FAIL，不作为原始运行时序验收。

CLI7808；源45084/32424/36728；VLC42232。附加cdb产生暂停，materialization=26610290600ns，协议截止26047987644ns，晚562302956ns。worker失败后观测C++ EH 0xe06d7363，随后0xc0000409 Subcode7 FAST_FAIL_FATAL_APP_EXIT。

cdb仅设置首次异常命令，但fast-fail为second chance，命令未执行；没有转储和线程栈。cdb因输入结束退出1，CLI最终-1073740972，不能把该退出等同原生fast-fail退出。源与VLC按精确PID中止清理，源退出-1，不是自然结束；无重入/快照。

下一轮同时配置-c和-c2，保留原异常，定位清理路径抛出/terminate来源。不得将附加扰动形成的迟到用于证明原r291时序根因。

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r294-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r294-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r294-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r294-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r294-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r294-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r294 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62020 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r294-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r294-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r294-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r294-native- --snapshot-format=png udp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r294-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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


& 'C:/Program Files (x86)/Windows Kits/10/Debuggers/x64/cdb.exe' -p 7808 -G -logo 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r294-cdb.log' -y 'D:/Code/MyCode/MediaTranscode/out/build/x64-release' -c 'sxe -c ".exr -1;.ecxr;~* kb;.dump /ma D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r294-crash.dmp;qd" 0xc0000409;g'
~~~~

~~~~text
[CLI] composition_inputs=3 grid=fixed audio_source=2 audio=on max_duration=source_driven hw=planner-highest-score
[2026-10-21 16:43:22.610] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-21 16:43:22.612] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-21 16:43:27.008] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-10-21 16:43:27.008] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-10-21 16:43:27.008] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-10-21 16:43:27.084] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-21 16:43:27.085] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-21 16:43:27.148] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-21 16:43:27.148] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-21 16:43:27.244] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-10-21 16:43:27.291] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-10-21 16:43:27.291] [info] [graph][planner.capability] canvas_cuda_stream stage=prepared device_stream=0000000000000000 private_nonblocking=1
[2026-10-21 16:43:27.300] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-21 16:43:27.300] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-21 16:43:27.300] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-10-21 16:43:27.305] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-21 16:43:27.305] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-21 16:43:27.306] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-10-21 16:43:27.313] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-21 16:43:27.313] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-21 16:43:27.313] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-10-21 16:43:27.340] [info] [graph][planner.capability] composition_preparation_storage stage=graph_bound scope=shared_reservations capacity=2147483648 retained=847841479 high_water=865250640
[CLI] audio_plan branch=transcode_frame reason=transcode_required_by_output_frame_contract codec=aac sample_rate=44100 channels=2 access_unit_samples=1024 encoder=aac bitrate_kbps=192
[CLI] prepared_output_id=1
[CLI] selected_chain=fixed-grid-composition score=0 decoder=h264,h264,h264 filter=scale_cuda=640:360,scale_cuda=640:360,scale_cuda=640:360 encoder=hevc_nvenc
[2026-10-21 16:43:27.346] [info] [graph][runtime.lifecycle] compile.begin
[2026-10-21 16:43:27.347] [info] [graph][runtime.channel] create channel=1 edge=1 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-21 16:43:27.347] [info] [graph][runtime.channel] create channel=2 edge=2 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-21 16:43:27.347] [info] [graph][runtime.channel] create channel=3 edge=3 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-21 16:43:27.347] [info] [graph][runtime.channel] create channel=4 edge=4 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.347] [info] [graph][runtime.channel] create channel=5 edge=5 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.347] [info] [graph][runtime.channel] create channel=6 edge=6 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.347] [info] [graph][runtime.channel] create channel=7 edge=7 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.348] [info] [graph][runtime.channel] create channel=8 edge=8 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.348] [info] [graph][runtime.channel] create channel=9 edge=9 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:43:27.348] [info] [graph][runtime.channel] create channel=10 edge=10 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.348] [info] [graph][runtime.channel] create channel=11 edge=11 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.348] [info] [graph][runtime.channel] create channel=12 edge=12 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:43:27.348] [info] [graph][runtime.channel] create channel=13 edge=13 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:43:27.348] [info] [graph][runtime.channel] create channel=14 edge=14 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.348] [info] [graph][runtime.channel] create channel=15 edge=15 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.348] [info] [graph][runtime.channel] create channel=16 edge=16 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.348] [info] [graph][runtime.channel] create channel=17 edge=17 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.348] [info] [graph][runtime.channel] create channel=18 edge=18 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.348] [info] [graph][runtime.channel] create channel=19 edge=19 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.348] [info] [graph][runtime.channel] create channel=20 edge=20 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.348] [info] [graph][runtime.channel] create channel=21 edge=21 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-10-21 16:43:27.348] [info] [graph][runtime.channel] create channel=22 edge=22 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-21 16:43:27.349] [info] [graph][runtime.channel] create channel=23 edge=23 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:43:27.349] [info] [graph][runtime.channel] create channel=24 edge=24 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-21 16:43:27.349] [info] [graph][runtime.channel] create channel=25 edge=25 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:43:27.349] [info] [graph][runtime.channel] create channel=26 edge=26 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:43:27.349] [info] [graph][runtime.channel] create channel=27 edge=27 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:43:27.349] [info] [graph][runtime.channel] create channel=28 edge=28 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:43:27.349] [info] [graph][runtime.channel] create channel=29 edge=29 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:43:27.349] [info] [graph][runtime.channel] create channel=30 edge=30 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.349] [info] [graph][runtime.channel] create channel=31 edge=31 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.349] [info] [graph][runtime.channel] create channel=32 edge=32 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.349] [info] [graph][runtime.channel] create channel=33 edge=33 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.349] [info] [graph][runtime.channel] create channel=34 edge=34 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.349] [info] [graph][runtime.channel] create channel=35 edge=35 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.349] [info] [graph][runtime.channel] create channel=36 edge=36 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:43:27.349] [info] [graph][runtime.channel] create channel=37 edge=37 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.350] [info] [graph][runtime.channel] create channel=38 edge=38 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.350] [info] [graph][runtime.channel] create channel=39 edge=39 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:43:27.350] [info] [graph][runtime.channel] create channel=40 edge=40 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:43:27.350] [info] [graph][runtime.channel] create channel=41 edge=41 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.350] [info] [graph][runtime.channel] create channel=42 edge=42 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.350] [info] [graph][runtime.channel] create channel=43 edge=43 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.350] [info] [graph][runtime.channel] create channel=44 edge=44 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.350] [info] [graph][runtime.channel] create channel=45 edge=45 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.350] [info] [graph][runtime.channel] create channel=46 edge=46 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.350] [info] [graph][runtime.channel] create channel=47 edge=47 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.350] [info] [graph][runtime.channel] create channel=48 edge=48 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-10-21 16:43:27.351] [info] [graph][runtime.channel] create channel=49 edge=49 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-21 16:43:27.351] [info] [graph][runtime.channel] create channel=50 edge=50 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:43:27.351] [info] [graph][runtime.channel] create channel=51 edge=51 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-21 16:43:27.351] [info] [graph][runtime.channel] create channel=52 edge=52 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:43:27.351] [info] [graph][runtime.channel] create channel=53 edge=53 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:43:27.351] [info] [graph][runtime.channel] create channel=54 edge=54 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:43:27.351] [info] [graph][runtime.channel] create channel=55 edge=55 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:43:27.351] [info] [graph][runtime.channel] create channel=56 edge=56 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:43:27.351] [info] [graph][runtime.channel] create channel=57 edge=57 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.351] [info] [graph][runtime.channel] create channel=58 edge=58 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.351] [info] [graph][runtime.channel] create channel=59 edge=59 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.351] [info] [graph][runtime.channel] create channel=60 edge=60 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.351] [info] [graph][runtime.channel] create channel=61 edge=61 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.351] [info] [graph][runtime.channel] create channel=62 edge=62 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.352] [info] [graph][runtime.channel] create channel=63 edge=63 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.352] [info] [graph][runtime.channel] create channel=64 edge=64 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.352] [info] [graph][runtime.channel] create channel=65 edge=65 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.352] [info] [graph][runtime.channel] create channel=66 edge=66 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:43:27.352] [info] [graph][runtime.channel] create channel=67 edge=67 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:43:27.352] [info] [graph][runtime.channel] create channel=68 edge=68 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.352] [info] [graph][runtime.channel] create channel=69 edge=69 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.352] [info] [graph][runtime.channel] create channel=70 edge=70 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.352] [info] [graph][runtime.channel] create channel=71 edge=71 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:43:27.352] [info] [graph][runtime.channel] create channel=72 edge=72 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:43:27.353] [info] [graph][runtime.channel] create channel=73 edge=73 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:43:27.353] [info] [graph][runtime.channel] create channel=74 edge=74 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:43:27.353] [info] [graph][runtime.channel] create channel=75 edge=75 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.353] [info] [graph][runtime.channel] create channel=76 edge=76 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.353] [info] [graph][runtime.channel] create channel=77 edge=77 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.353] [info] [graph][runtime.channel] create channel=78 edge=78 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.353] [info] [graph][runtime.channel] create channel=79 edge=79 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.353] [info] [graph][runtime.channel] create channel=80 edge=80 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.353] [info] [graph][runtime.channel] create channel=81 edge=81 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.353] [info] [graph][runtime.channel] create channel=82 edge=82 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.354] [info] [graph][runtime.channel] create channel=83 edge=83 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-10-21 16:43:27.354] [info] [graph][runtime.channel] create channel=84 edge=84 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-21 16:43:27.354] [info] [graph][runtime.channel] create channel=85 edge=85 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:43:27.354] [info] [graph][runtime.channel] create channel=86 edge=86 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-21 16:43:27.354] [info] [graph][runtime.channel] create channel=87 edge=87 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:43:27.354] [info] [graph][runtime.channel] create channel=88 edge=88 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:43:27.355] [info] [graph][runtime.channel] create channel=89 edge=89 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:43:27.355] [info] [graph][runtime.channel] create channel=90 edge=90 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:43:27.355] [info] [graph][runtime.channel] create channel=91 edge=91 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-10-21 16:43:27.355] [info] [graph][runtime.channel] create channel=92 edge=92 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-21 16:43:27.355] [info] [graph][runtime.channel] create channel=93 edge=93 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:43:27.355] [info] [graph][runtime.channel] create channel=94 edge=94 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-21 16:43:27.355] [info] [graph][runtime.channel] create channel=95 edge=95 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:43:27.355] [info] [graph][runtime.channel] create channel=96 edge=96 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:43:27.355] [info] [graph][runtime.channel] create channel=97 edge=97 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:43:27.356] [info] [graph][runtime.channel] create channel=98 edge=98 stream=Audio edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.356] [info] [graph][runtime.channel] create channel=99 edge=99 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:43:27.356] [info] [graph][runtime.channel] create channel=100 edge=100 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.356] [info] [graph][runtime.channel] create channel=101 edge=101 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0
[2026-10-21 16:43:27.356] [info] [graph][runtime.channel] create channel=102 edge=102 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-21 16:43:27.356] [info] [graph][runtime.channel] create channel=103 edge=103 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-21 16:43:27.356] [info] [graph][runtime.channel] create channel=104 edge=104 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:43:27.356] [info] [graph][runtime.channel] create channel=105 edge=105 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0
[2026-10-21 16:43:27.356] [info] [graph][runtime.channel] create channel=106 edge=106 stream=Video edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-21 16:43:27.357] [info] [graph][runtime.channel] create channel=107 edge=107 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-21 16:43:27.357] [info] [graph][runtime.channel] create channel=108 edge=108 stream=Any edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-21 16:43:27.357] [info] [graph][runtime.channel] create channel=109 edge=109 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.357] [info] [graph][runtime.channel] create channel=110 edge=110 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.357] [info] [graph][runtime.channel] create channel=111 edge=111 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-21 16:43:27.357] [info] [graph][runtime.channel] create channel=112 edge=112 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:43:27.357] [info] [graph][runtime.channel] create channel=113 edge=113 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-21 16:43:27.357] [info] [graph][runtime.channel] create channel=114 edge=114 stream=Any edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-21 16:43:27.357] [info] [graph][runtime.channel] create channel=115 edge=115 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-21 16:43:27.357] [info] [graph][runtime.channel] create channel=116 edge=116 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-21 16:43:27.357] [info] [graph][runtime.channel] create channel=117 edge=117 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-21 16:43:27.357] [info] [graph][runtime.channel] create channel=118 edge=118 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-21 16:43:27.357] [info] [graph][runtime.channel] create channel=119 edge=119 stream=Any edge_kind=EncodedPacket payload=Unknown queued=0
[2026-10-21 16:43:27.357] [info] [graph][runtime.channel] create channel=120 edge=120 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-21 16:43:27.357] [info] [graph][runtime.channel] create channel=121 edge=121 stream=Metadata edge_kind=Unknown payload=Unknown queued=0
[2026-10-21 16:43:27.358] [info] [graph][runtime.channel] create channel=122 edge=122 stream=Metadata edge_kind=Unknown payload=Unknown queued=0
[2026-10-21 16:43:27.358] [info] [graph][runtime.lifecycle] compiled nodes=81 edges=122 channels=122 execution_order=1->3->8->27->46->47->9->21->28->40->63->48->69->10->29->49->11->12->30->31->50->51->52->13->32->53->14->17->33->36->54->55->59->15->34->56->57->16->35->58->18->37->60->19->38->61->20->39->62->22->41->64->70->23->42->65->71->24->43->66->72->26->45->68->73->25->44->67->6->2->7->80->77->4->5->74->75->78->76->79->81
[2026-10-21 16:43:27.358] [info] [graph][runtime.lifecycle] compile.done state=DefaultRegistrationPending
[2026-10-21 16:43:27.358] [info] [graph][runtime.node] register node=1 name=composition-r294.output.video.codec_resolver kind=CodecResolver
[2026-10-21 16:43:27.359] [info] [graph][runtime.node] register node=2 name=composition-r294.output.video.encode kind=VideoEncode
[2026-10-21 16:43:27.359] [info] [graph][runtime.node] register node=3 name=composition-r294.output.audio.codec_resolver kind=AudioCodecResolver
[2026-10-21 16:43:27.360] [info] [graph][runtime.node] register node=4 name=composition-r294.output.audio.encode kind=AudioEncode
[2026-10-21 16:43:27.360] [info] [graph][runtime.node] register node=5 name=composition-r294.output.audio.canonicalizer kind=EncodedAudioCanonicalizer
[2026-10-21 16:43:27.360] [info] [graph][runtime.node] register node=8 name=composition-r294.source_0.input kind=RawRtpInput
[2026-10-21 16:43:27.360] [info] [graph][runtime.node] register node=9 name=composition-r294.source_0.rtp.clock_group kind=RtpClockGroup
[2026-10-21 16:43:27.360] [info] [graph][runtime.node] register node=10 name=composition-r294.source_0.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-10-21 16:43:27.360] [info] [graph][runtime.node] register node=11 name=composition-r294.source_0.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-10-21 16:43:27.360] [info] [graph][runtime.node] register node=12 name=composition-r294.source_0.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-10-21 16:43:27.360] [info] [graph][runtime.node] register node=13 name=composition-r294.source_0.av_sync.source_clock kind=SourceClockStateFanout
[2026-10-21 16:43:27.361] [info] [graph][runtime.node] register node=14 name=composition-r294.source_0.av_sync.video.generation_gate kind=LockedPacketGate
[2026-10-21 16:43:27.361] [info] [graph][runtime.node] register node=15 name=composition-r294.source_0.av_sync.video.canonical_input kind=CanonicalInput
[2026-10-21 16:43:27.361] [info] [graph][runtime.node] register node=16 name=composition-r294.source_0.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-10-21 16:43:27.361] [info] [graph][runtime.node] register node=17 name=composition-r294.source_0.av_sync.startup.clock kind=AvStartupClock
[2026-10-21 16:43:27.361] [info] [graph][runtime.node] register node=18 name=composition-r294.source_0.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-10-21 16:43:27.361] [info] [graph][runtime.node] register node=20 name=composition-r294.source_0.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-10-21 16:43:27.361] [info] [graph][runtime.node] register node=21 name=composition-r294.source_0.video.codec_resolver kind=CodecResolver
[2026-10-21 16:43:27.361] [info] [graph][runtime.node] register node=22 name=composition-r294.source_0.video.decode kind=VideoDecode
[2026-10-21 16:43:27.362] [info] [graph][runtime.node] register node=23 name=composition-r294.source_0.video.hwtransfer kind=HardwareTransfer
[2026-10-21 16:43:27.362] [info] [graph][runtime.node] register node=24 name=composition-r294.source_0.video.framerate kind=VideoFrameRate
[2026-10-21 16:43:27.362] [info] [graph][runtime.node] register node=25 name=composition-r294.source_0.video.preparation kind=VideoPreparation
[2026-10-21 16:43:27.362] [info] [graph][runtime.node] register node=26 name=composition-r294.source_0.video.filter kind=VideoFilter
[2026-10-21 16:43:27.362] [info] [graph][runtime.node] register node=27 name=composition-r294.source_1.input kind=RawRtpInput
[2026-10-21 16:43:27.362] [info] [graph][runtime.node] register node=28 name=composition-r294.source_1.rtp.clock_group kind=RtpClockGroup
[2026-10-21 16:43:27.362] [info] [graph][runtime.node] register node=29 name=composition-r294.source_1.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-10-21 16:43:27.362] [info] [graph][runtime.node] register node=30 name=composition-r294.source_1.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-10-21 16:43:27.362] [info] [graph][runtime.node] register node=31 name=composition-r294.source_1.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-10-21 16:43:27.363] [info] [graph][runtime.node] register node=32 name=composition-r294.source_1.av_sync.source_clock kind=SourceClockStateFanout
[2026-10-21 16:43:27.363] [info] [graph][runtime.node] register node=33 name=composition-r294.source_1.av_sync.video.generation_gate kind=LockedPacketGate
[2026-10-21 16:43:27.363] [info] [graph][runtime.node] register node=34 name=composition-r294.source_1.av_sync.video.canonical_input kind=CanonicalInput
[2026-10-21 16:43:27.363] [info] [graph][runtime.node] register node=35 name=composition-r294.source_1.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-10-21 16:43:27.363] [info] [graph][runtime.node] register node=36 name=composition-r294.source_1.av_sync.startup.clock kind=AvStartupClock
[2026-10-21 16:43:27.363] [info] [graph][runtime.node] register node=37 name=composition-r294.source_1.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-10-21 16:43:27.363] [info] [graph][runtime.node] register node=39 name=composition-r294.source_1.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-10-21 16:43:27.363] [info] [graph][runtime.node] register node=40 name=composition-r294.source_1.video.codec_resolver kind=CodecResolver
[2026-10-21 16:43:27.363] [info] [graph][runtime.node] register node=41 name=composition-r294.source_1.video.decode kind=VideoDecode
[2026-10-21 16:43:27.363] [info] [graph][runtime.node] register node=42 name=composition-r294.source_1.video.hwtransfer kind=HardwareTransfer
[2026-10-21 16:43:27.363] [info] [graph][runtime.node] register node=43 name=composition-r294.source_1.video.framerate kind=VideoFrameRate
[2026-10-21 16:43:27.363] [info] [graph][runtime.node] register node=44 name=composition-r294.source_1.video.preparation kind=VideoPreparation
[2026-10-21 16:43:27.363] [info] [graph][runtime.node] register node=45 name=composition-r294.source_1.video.filter kind=VideoFilter
[2026-10-21 16:43:27.364] [info] [graph][runtime.node] register node=46 name=composition-r294.source_2.video.input kind=RawRtpInput
[2026-10-21 16:43:27.364] [info] [graph][runtime.node] register node=47 name=composition-r294.source_2.audio.input kind=RawRtpInput
[2026-10-21 16:43:27.364] [info] [graph][runtime.node] register node=48 name=composition-r294.source_2.rtp.clock_group kind=RtpClockGroup
[2026-10-21 16:43:27.364] [info] [graph][runtime.node] register node=49 name=composition-r294.source_2.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-10-21 16:43:27.364] [info] [graph][runtime.node] register node=50 name=composition-r294.source_2.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-10-21 16:43:27.364] [info] [graph][runtime.node] register node=51 name=composition-r294.source_2.av_sync.audio.protocol_binder kind=RtpPacketClockBinder
[2026-10-21 16:43:27.364] [info] [graph][runtime.node] register node=52 name=composition-r294.source_2.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-10-21 16:43:27.364] [info] [graph][runtime.node] register node=53 name=composition-r294.source_2.av_sync.source_clock kind=SourceClockStateFanout
[2026-10-21 16:43:27.364] [info] [graph][runtime.node] register node=54 name=composition-r294.source_2.av_sync.video.generation_gate kind=LockedPacketGate
[2026-10-21 16:43:27.364] [info] [graph][runtime.node] register node=55 name=composition-r294.source_2.av_sync.audio.generation_gate kind=LockedPacketGate
[2026-10-21 16:43:27.364] [info] [graph][runtime.node] register node=56 name=composition-r294.source_2.av_sync.video.canonical_input kind=CanonicalInput
[2026-10-21 16:43:27.364] [info] [graph][runtime.node] register node=57 name=composition-r294.source_2.av_sync.audio.canonical_input kind=CanonicalInput
[2026-10-21 16:43:27.365] [info] [graph][runtime.node] register node=58 name=composition-r294.source_2.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-10-21 16:43:27.365] [info] [graph][runtime.node] register node=59 name=composition-r294.source_2.av_sync.startup.clock kind=AvStartupClock
[2026-10-21 16:43:27.365] [info] [graph][runtime.node] register node=60 name=composition-r294.source_2.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-10-21 16:43:27.365] [info] [graph][runtime.node] register node=62 name=composition-r294.source_2.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-10-21 16:43:27.365] [info] [graph][runtime.node] register node=63 name=composition-r294.source_2.video.codec_resolver kind=CodecResolver
[2026-10-21 16:43:27.365] [info] [graph][runtime.node] register node=64 name=composition-r294.source_2.video.decode kind=VideoDecode
[2026-10-21 16:43:27.365] [info] [graph][runtime.node] register node=65 name=composition-r294.source_2.video.hwtransfer kind=HardwareTransfer
[2026-10-21 16:43:27.365] [info] [graph][runtime.node] register node=66 name=composition-r294.source_2.video.framerate kind=VideoFrameRate
[2026-10-21 16:43:27.365] [info] [graph][runtime.node] register node=67 name=composition-r294.source_2.video.preparation kind=VideoPreparation
[2026-10-21 16:43:27.365] [info] [graph][runtime.node] register node=68 name=composition-r294.source_2.video.filter kind=VideoFilter
[2026-10-21 16:43:27.365] [info] [graph][runtime.node] register node=69 name=composition-r294.source_2.audio.encode.codec_resolver kind=AudioCodecResolver
[2026-10-21 16:43:27.365] [info] [graph][runtime.node] register node=70 name=composition-r294.source_2.audio.encode.decode kind=AudioDecode
[2026-10-21 16:43:27.365] [info] [graph][runtime.node] register node=71 name=composition-r294.source_2.audio.encode.startup_trim kind=Unknown
[2026-10-21 16:43:27.365] [info] [graph][runtime.node] register node=72 name=composition-r294.source_2.audio.encode.drift_controller kind=AudioDriftController
[2026-10-21 16:43:27.365] [info] [graph][runtime.node] register node=73 name=composition-r294.source_2.audio.encode.resample kind=AudioResample
[2026-10-21 16:43:27.365] [info] [graph][runtime.node] register node=74 name=composition-r294.output.scheduler.scheduler kind=AvOutputScheduler
[2026-10-21 16:43:27.365] [info] [graph][runtime.node] register node=75 name=composition-r294.output.scheduler.router kind=ScheduledOutputRouter
[2026-10-21 16:43:27.366] [info] [graph][runtime.node] register node=76 name=composition-r294.output.mpegts.mux kind=FileMux
[2026-10-21 16:43:27.366] [info] [graph][runtime.node] register node=77 name=composition-r294.output.mpegts.plan.source kind=Unknown
[2026-10-21 16:43:27.366] [info] [graph][runtime.node] register node=78 name=composition-r294.output.mpegts.scheduled.adapter kind=Unknown
[2026-10-21 16:43:27.366] [info] [graph][runtime.node] register node=79 name=composition-r294.output.mpegts.datagram.materializer kind=MpegTsDatagramMaterializer
[2026-10-21 16:43:27.366] [info] [graph][runtime.node] register node=80 name=composition-r294.output.mpegts.transport.plan kind=DatagramTransportPlanSource
[2026-10-21 16:43:27.366] [info] [graph][runtime.node] register node=81 name=composition-r294.output.mpegts.transport.sender kind=ScheduledDatagramSender
[2026-10-21 16:43:27.366] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=waiting_for_random_access detail="" description=""
[2026-10-21 16:43:27.366] [info] [graph][runtime.lifecycle] start.begin mode=threaded
[2026-10-21 16:43:27.369] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:43:27.369] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:43:27.369] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:43:27.369] [info] [graph][runtime.lifecycle] worker.finished node=63
[2026-10-21 16:43:27.369] [info] [graph][runtime.lifecycle] worker.finished node=3
[2026-10-21 16:43:27.370] [info] [graph][runtime.node] worker.thread_cpu node=3 available=1 cpu_ns=0 process_calls=1
[2026-10-21 16:43:27.370] [info] [graph][runtime.node] worker.thread_cpu node=63 available=1 cpu_ns=0 process_calls=2
[2026-10-21 16:43:27.370] [info] [graph][runtime.node] audio_encode_trace stage=codec_bound
[2026-10-21 16:43:27.370] [info] [graph][runtime.lifecycle] start.done state=ThreadedRunning
[2026-10-21 16:43:27.370] [info] [graph][runtime.lifecycle] worker.finished node=21
[2026-10-21 16:43:27.371] [info] [graph][runtime.node] worker.thread_cpu node=21 available=1 cpu_ns=0 process_calls=2
[2026-10-21 16:43:27.370] [info] [graph][runtime.lifecycle] worker.finished node=40
[2026-10-21 16:43:27.372] [info] [graph][runtime.node] worker.thread_cpu node=40 available=1 cpu_ns=0 process_calls=1
[2026-10-21 16:43:27.372] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-10-21 16:43:27.372] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=15 generation=1
[2026-10-21 16:43:27.373] [info] [graph][runtime.lifecycle] worker.finished node=1
[2026-10-21 16:43:27.373] [info] [graph][runtime.node] worker.thread_cpu node=1 available=1 cpu_ns=0 process_calls=1
[2026-10-21 16:43:27.374] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-10-21 16:43:27.374] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=15 generation=1
[2026-10-21 16:43:27.375] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=577626839 dts=577626839 size=317016 generation=1 sequence=15
[2026-10-21 16:43:27.382] [info] [graph][runtime.node] video_encode.bind_encoder codec=hevc_nvenc pix_fmt=cuda rate_control=cbr bitrate=8000000 min_bitrate=8000000 max_bitrate=8000000 buffer_size=267000 fps=30/1 gop=60 frame_kind=hardware hwaccel=cuda hw_device_ctx=set hw_frames_ctx=set
[2026-10-21 16:43:27.384] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-10-21 16:43:27.384] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=15 generation=1
[2026-10-21 16:43:27.385] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=714007577 dts=714007577 size=317016 generation=1 sequence=15
[2026-10-21 16:43:27.387] [info] [graph][runtime.lifecycle] worker.finished node=69
[2026-10-21 16:43:27.387] [info] [graph][runtime.node] worker.thread_cpu node=69 available=1 cpu_ns=0 process_calls=2
[2026-10-21 16:43:27.389] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=592174187 dts=592174187 size=317016 generation=1 sequence=15
[2026-10-21 16:43:27.404] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-10-21 16:43:27.404] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=577626839 format=cuda size=1280x720 buffer_identity=0xb08200000 drm_prime=0 software_frame=0
[2026-10-21 16:43:27.404] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=0000019019819DA0 stream=Video payload=Frame pts=577626839 dts=577626839 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:43:27.405] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=0000019019819DA0 stream=Video payload=Frame pts=577626839 dts=577626839 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:43:27.405] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=0000019019819DA0 stream=Video payload=Frame pts=577626839 dts=577626839 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:43:27.405] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-10-21 16:43:27.405] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=000001901981A190 stream=Video payload=Frame pts=577626839 dts=577626839 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:43:27.405] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=000001901981A190 stream=Video payload=Frame pts=577626839 dts=577626839 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:43:27.405] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-10-21 16:43:27.408] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-10-21 16:43:27.409] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=714007577 format=cuda size=1280x720 buffer_identity=0xb08a00000 drm_prime=0 software_frame=0
[2026-10-21 16:43:27.410] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=000001901E6E0A70 stream=Video payload=Frame pts=714007577 dts=714007577 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:43:27.410] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=000001901E6E0A70 stream=Video payload=Frame pts=714007577 dts=714007577 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:43:27.410] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=000001901E6E0A70 stream=Video payload=Frame pts=714007577 dts=714007577 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:43:27.410] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-10-21 16:43:27.410] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=000001901E6E1A30 stream=Video payload=Frame pts=714007577 dts=714007577 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:43:27.410] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=000001901E6E1A30 stream=Video payload=Frame pts=714007577 dts=714007577 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:43:27.411] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-10-21 16:43:27.414] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-10-21 16:43:27.414] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=592174187 format=cuda size=1280x720 buffer_identity=0xb0b200000 drm_prime=0 software_frame=0
[2026-10-21 16:43:27.415] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=000001901E6E35C0 stream=Video payload=Frame pts=592174187 dts=592174187 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:43:27.415] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=000001901E6E35C0 stream=Video payload=Frame pts=592174187 dts=592174187 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:43:27.415] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=000001901E6E35C0 stream=Video payload=Frame pts=592174187 dts=592174187 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:43:27.415] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-10-21 16:43:27.415] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=000001901E6E1E20 stream=Video payload=Frame pts=592174187 dts=592174187 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:43:27.415] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=000001901E6E1E20 stream=Video payload=Frame pts=592174187 dts=592174187 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:43:27.416] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-10-21 16:43:27.518] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=112
[2026-10-21 16:43:27.518] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=102
[2026-10-21 16:43:27.518] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=107
[2026-10-21 16:43:27.521] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb08e00000 drm_prime=0 software_frame=0
[2026-10-21 16:43:27.521] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=15 audio_count=0 first_video_sequence=15
[2026-10-21 16:43:27.522] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=0
[2026-10-21 16:43:27.523] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb08e90000 drm_prime=0 software_frame=0
[2026-10-21 16:43:27.523] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=15 audio_count=0 first_video_sequence=15
[2026-10-21 16:43:27.523] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=146 audio_count=23 first_video_sequence=15
[2026-10-21 16:43:27.523] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb08f20000 drm_prime=0 software_frame=0
[2026-10-21 16:43:27.523] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=0
[2026-10-21 16:43:27.524] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=23
[2026-10-21 16:43:27.524] [info] [graph][runtime.node] audio_decode_trace stage=first_packet
[2026-10-21 16:43:27.525] [info] [graph][runtime.node] audio_decode_trace stage=first_frame samples=1024 rate=44100
[2026-10-21 16:43:27.526] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1 source_master_ns=5682114118 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-10-21 16:43:27.526] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3 source_master_ns=5728554028 raw_phase_ns=11339 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=45346
[2026-10-21 16:43:27.527] [info] [graph][runtime.node] mpegts_emission_plan generation=1 mode=canonical_access_unit_window access_unit_window_ns=100000000 video_initial_service_window_ns=13990650 audio_initial_service_window_ns=13990650 packet_size_bytes=188 maximum_payload_bytes=1316 per_datagram_overhead_bytes=0
[2026-10-21 16:43:27.527] [info] [graph][runtime.node] audio_encode_trace stage=first_frame
[2026-10-21 16:43:27.529] [info] [graph][runtime.node] video_encode.input_contract format=cuda size=1280x720 buffer_identity=0xb0a000000 drm_prime=0 software_frame=0
[2026-10-21 16:43:27.529] [info] [graph][runtime.node] video_encode.generation_start_key_frame pts=0 generation=1
[2026-10-21 16:43:27.529] [info] [graph][runtime.node] video_encode.first_frame pts=0 generation=attached
[2026-10-21 16:43:27.537] [info] [graph][runtime.node] video_encode.first_submit result=accepted
[2026-10-21 16:43:27.537] [info] [graph][runtime.node] video_encode.first_packet pts=0 dts=0 duration=0 size=108357 side_data_elems=1 received_footprint=108365 side_data[0]=Quality stats:8
[2026-10-21 16:43:27.537] [info] [graph][runtime.node] video_encode.first_packet_footprint bytes=108365 wrapped_size=108357 wrapped_side_data_elems=1
[2026-10-21 16:43:27.538] [info] [graph][runtime.node] av_scheduler_trace stage=first_video_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=5681320568 dispatch_master_ns=5681320568 now_ns=5525963000 node=74 identity_kind=output identity="composition-r294.output" canonical_sequence=1 canonical_created_steady_ns=28791454432000 head_observed_steady_ns=28791454479900 codec_receive_begin_steady_ns=28791454031900 codec_receive_end_steady_ns=28791454032300
[2026-10-21 16:43:27.538] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=5526217700 present_generation=1 present_canonical_target_ns=0 present_canonical_dispatch_ns=0 present_target_master_ns=5681320568 present_dispatch_master_ns=5681320568
[2026-10-21 16:43:27.576] [info] [graph][runtime.node] audio_encode_trace stage=first_packet
[2026-10-21 16:43:27.576] [info] [graph][runtime.node] av_scheduler_trace stage=first_audio_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=5681320568 dispatch_master_ns=5681320568 now_ns=5564518300 node=74 identity_kind=output identity="composition-r294.output" canonical_sequence=1 canonical_created_steady_ns=28791492992700 head_observed_steady_ns=28791493035300 codec_receive_begin_steady_ns=unavailable codec_receive_end_steady_ns=unavailable
[2026-10-21 16:43:27.766] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=running detail="" description=""
[2026-10-21 16:43:28.154] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=48 source_master_ns=6773451987 raw_phase_ns=11339 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=90692
[2026-10-21 16:43:29.175] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=92 source_master_ns=7795129991 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=136038
[2026-10-21 16:43:30.197] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=136 source_master_ns=8816807996 raw_phase_ns=11339 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=181384
[2026-10-21 16:43:31.219] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=180 source_master_ns=9838485999 raw_phase_ns=11337 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=226730
[2026-10-21 16:43:32.013] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=318065457 ntp_seconds=4001561012 ntp_fraction=55834574 arrival_ns=28795930204600
[2026-10-21 16:43:32.015] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=318065457
[2026-10-21 16:43:32.014] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1513245279 ntp_seconds=4001561012 ntp_fraction=60129542 arrival_ns=28795930375500
[2026-10-21 16:43:32.015] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:43:32.015] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1513245279
[2026-10-21 16:43:32.016] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:43:32.034] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2603436744 ntp_seconds=4001561012 ntp_fraction=146028888 arrival_ns=28795950944700
[2026-10-21 16:43:32.035] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2603436744
[2026-10-21 16:43:32.035] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:43:32.044] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=4093888626 ntp_seconds=4001561012 ntp_fraction=188978561 arrival_ns=28795961094900
[2026-10-21 16:43:32.045] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4093888626
[2026-10-21 16:43:32.045] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:43:32.264] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=225 source_master_ns=10883383959 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=272076
[2026-10-21 16:43:33.285] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=269 source_master_ns=11905061963 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=317422
[2026-10-21 16:43:34.307] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=313 source_master_ns=12926739968 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=362768
[2026-10-21 16:43:35.352] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=358 source_master_ns=13971637927 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=408114
[2026-10-21 16:43:36.373] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=402 source_master_ns=14993315931 raw_phase_ns=11337 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=453460
[2026-10-21 16:43:37.017] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=318065457 ntp_seconds=4001561017 ntp_fraction=68719476 arrival_ns=28800933192300
[2026-10-21 16:43:37.017] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=318065457
[2026-10-21 16:43:37.017] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:43:37.017] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1513245279 ntp_seconds=4001561017 ntp_fraction=73014444 arrival_ns=28800933939300
[2026-10-21 16:43:37.017] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1513245279
[2026-10-21 16:43:37.017] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:43:37.047] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=4093888626 ntp_seconds=4001561017 ntp_fraction=197568495 arrival_ns=28800963337500
[2026-10-21 16:43:37.047] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4093888626
[2026-10-21 16:43:37.047] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:43:37.062] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2603436744 ntp_seconds=4001561017 ntp_fraction=266287972 arrival_ns=28800978988400
[2026-10-21 16:43:37.063] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2603436744
[2026-10-21 16:43:37.063] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:43:37.395] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=446 source_master_ns=16014993936 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=498806
[2026-10-21 16:43:38.046] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=video deadline_ns=none now_ns=16034686300 present_generation=1 present_canonical_target_ns=10472199546 present_canonical_dispatch_ns=10472199546 present_target_master_ns=16153520114 present_dispatch_master_ns=16153520114
[2026-10-21 16:43:38.047] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=16035117600 present_generation=1 present_canonical_target_ns=10500000000 present_canonical_dispatch_ns=10500000000 present_target_master_ns=16181320568 present_dispatch_master_ns=16181320568
[2026-10-21 16:43:38.417] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=490 source_master_ns=17036671941 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=544152
[2026-10-21 16:43:39.462] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=535 source_master_ns=18081569900 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=589498
[2026-10-21 16:43:40.483] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=579 source_master_ns=19103247904 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=634844
[2026-10-21 16:43:41.505] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=623 source_master_ns=20124925909 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=680190
[2026-10-21 16:43:42.026] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=318065457 ntp_seconds=4001561022 ntp_fraction=111669149 arrival_ns=28805943159900
[2026-10-21 16:43:42.027] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=318065457
[2026-10-21 16:43:42.027] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:43:42.055] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1513245279 ntp_seconds=4001561022 ntp_fraction=236223201 arrival_ns=28805971514300
[2026-10-21 16:43:42.055] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1513245279
[2026-10-21 16:43:42.055] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:43:42.055] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=4093888626 ntp_seconds=4001561022 ntp_fraction=236223201 arrival_ns=28805971909000
[2026-10-21 16:43:42.056] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4093888626
[2026-10-21 16:43:42.056] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:43:42.069] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2603436744 ntp_seconds=4001561022 ntp_fraction=296352743 arrival_ns=28805985711200
[2026-10-21 16:43:42.069] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2603436744
[2026-10-21 16:43:42.069] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:43:42.550] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=668 source_master_ns=21169823868 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=725536
[2026-10-21 16:43:43.572] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=712 source_master_ns=22191501872 raw_phase_ns=11337 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=770882
[2026-10-21 16:43:44.593] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=756 source_master_ns=23213179877 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=816228
[2026-10-21 16:43:45.615] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=800 source_master_ns=24234857882 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=861574
[2026-10-21 16:43:46.660] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=845 source_master_ns=25279755841 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=906920
[2026-10-21 16:43:47.046] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=318065457 ntp_seconds=4001561027 ntp_fraction=197568495 arrival_ns=28810963123500
[2026-10-21 16:43:47.047] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=318065457
[2026-10-21 16:43:47.047] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:43:47.075] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1513245279 ntp_seconds=4001561027 ntp_fraction=322122547 arrival_ns=28810992156700
[2026-10-21 16:43:47.076] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1513245279
[2026-10-21 16:43:47.075] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=4093888626 ntp_seconds=4001561027 ntp_fraction=322122547 arrival_ns=28810992206400
[2026-10-21 16:43:47.076] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:43:47.076] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=4093888626
[2026-10-21 16:43:47.076] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:43:47.107] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2603436744 ntp_seconds=4001561027 ntp_fraction=455266533 arrival_ns=28811023173300
[2026-10-21 16:43:47.107] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2603436744
[2026-10-21 16:43:47.107] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:43:47.682] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=889 source_master_ns=26301433845 raw_phase_ns=11338 filtered_phase_ns=11338 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=952266
[2026-10-21 16:43:48.622] [info] [graph][runtime.node] protocol_batch_trace stage=materialization_failed generation=1 attempts=1 consumed_datagrams=0 remaining_datagrams=1 attempted_master_ns=26610290600 mux_dequeued_master_ns=26610189900 first_dequeued_master_ns=26610289900 last_wire_published_master_ns=25930999600 previous_attempt_started_master_ns=unavailable previous_attempt_completed_master_ns=unavailable last_attempt_completed_master_ns=26610296300 last_blocked_started_master_ns=unavailable last_blocked_completed_master_ns=unavailable blocked_attempts=0 remaining_front_release_master_ns=25981320976 remaining_front_deadline_master_ns=26047987644 original_error="InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=25981320976 materialized_ns=26610290600 protocol_deadline_ns=26047987644 deadline_before_release=0 materialized_after_deadline=1"
[2026-10-21 16:43:48.623] [info] [graph][runtime.node] process.failed node=79 name=MediaMpegTsDatagramMaterializerNode kind=MpegTsDatagramMaterializer inputs=[channel=118 edge=118 stream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=120 edge=120 stream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=121 edge=121 stream=Metadata edge_kind=Unknown payload=Unknown queued=10] outputs=[channel=122 edge=122 stream=Metadata edge_kind=Unknown payload=Unknown queued=0] error=InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=25981320976 materialized_ns=26610290600 protocol_deadline_ns=26047987644 deadline_before_release=0 materialized_after_deadline=1
[2026-10-21 16:43:48.624] [info] [graph][runtime.node] worker.failed node=79 kind=MpegTsDatagramMaterializer name=MPEG-TS wire protocol materializer error=InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=25981320976 materialized_ns=26610290600 protocol_deadline_ns=26047987644 deadline_before_release=0 materialized_after_deadline=1
[2026-10-21 16:43:48.624] [info] [graph][runtime.node] process.failed node=7 name=MediaContinuousAudioAggregateNode kind=ContinuousAudioAggregate inputs=[channel=101 edge=101 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=5; channel=103 edge=103 stream=Audio edge_kind=Metadata payload=CodecContext queued=0; channel=109 edge=109 stream=Metadata edge_kind=Event payload=GraphEvent queued=0] outputs=[channel=105 edge=105 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=5] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-21 16:43:48.624] [info] [graph][runtime.node] worker.thread_cpu node=7 available=1 cpu_ns=15625000 process_calls=3755
[2026-10-21 16:43:48.624] [info] [graph][runtime.node] worker.thread_cpu node=79 available=1 cpu_ns=406250000 process_calls=8245
[2026-10-21 16:43:48.624] [info] [graph][runtime.node] worker.thread_cpu node=45 available=1 cpu_ns=312500000 process_calls=2224
[2026-10-21 16:43:48.624] [info] [graph][runtime.node] worker.thread_cpu node=16 available=1 cpu_ns=0 process_calls=2294
[2026-10-21 16:43:48.624] [info] [graph][runtime.node] worker.thread_cpu node=30 available=1 cpu_ns=0 process_calls=2182
[2026-10-21 16:43:48.624] [info] [graph][runtime.node] worker.thread_cpu node=33 available=1 cpu_ns=0 process_calls=2309
[2026-10-21 16:43:48.624] [info] [graph][runtime.node] worker.thread_cpu node=68 available=1 cpu_ns=265625000 process_calls=2083
[2026-10-21 16:43:48.624] [info] [graph][runtime.node] worker.thread_cpu node=39 available=1 cpu_ns=15625000 process_calls=2159
[2026-10-21 16:43:48.624] [info] [graph][runtime.node] worker.thread_cpu node=35 available=1 cpu_ns=0 process_calls=2269
[2026-10-21 16:43:48.624] [info] [graph][runtime.node] worker.thread_cpu node=37 available=1 cpu_ns=46875000 process_calls=2231
[2026-10-21 16:43:48.624] [info] [graph][runtime.node] worker.thread_cpu node=64 available=1 cpu_ns=140625000 process_calls=2290
[2026-10-21 16:43:48.624] [info] [graph][runtime.node] process.failed node=8 name=RawRtpInputNode kind=RawRtpInput inputs=[] outputs=[channel=4 edge=4 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=5 edge=5 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=9 edge=9 stream=Video edge_kind=InputPacket payload=Packet queued=0; channel=21 edge=21 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-21 16:43:48.624] [info] [graph][runtime.node] worker.thread_cpu node=58 available=1 cpu_ns=62500000 process_calls=5375
[2026-10-21 16:43:48.624] [info] [graph][runtime.node] worker.thread_cpu node=54 available=1 cpu_ns=31250000 process_calls=2230
[2026-10-21 16:43:48.624] [info] [graph][runtime.node] worker.thread_cpu node=60 available=1 cpu_ns=0 process_calls=5006
[2026-10-21 16:43:48.624] [info] [graph][runtime.node] worker.thread_cpu node=71 available=1 cpu_ns=31250000 process_calls=2986
[2026-10-21 16:43:48.624] [info] [graph][runtime.node] worker.thread_cpu node=73 available=1 cpu_ns=78125000 process_calls=4447
[2026-10-21 16:43:48.624] [info] [graph][runtime.node] worker.thread_cpu node=5 available=1 cpu_ns=46875000 process_calls=2643
[2026-10-21 16:43:48.624] [info] [graph][runtime.node] worker.thread_cpu node=62 available=1 cpu_ns=46875000 process_calls=5011
[2026-10-21 16:43:48.624] [info] [graph][runtime.node] worker.thread_cpu node=19 available=1 cpu_ns=0 process_calls=2173
[2026-10-21 16:43:48.624] [info] [graph][runtime.node] worker.thread_cpu node=22 available=1 cpu_ns=187500000 process_calls=1641
[2026-10-21 16:43:48.623] [info] [graph][runtime.node] av_scheduler_trace stage=late_audio_head generation=1 mapping_epoch_generation=1 canonical_target_ns=20387120181 canonical_dispatch_ns=20387120181 target_master_ns=26068440749 dispatch_master_ns=26068440749 now_ns=26611264100 node=74 identity_kind=output identity="composition-r294.output" canonical_sequence=879 canonical_created_steady_ns=28812539763400 head_observed_steady_ns=28812539780900 codec_receive_begin_steady_ns=unavailable codec_receive_end_steady_ns=unavailable
[2026-10-21 16:43:48.624] [info] [graph][runtime.node] worker.thread_cpu node=12 available=1 cpu_ns=0 process_calls=15
[2026-10-21 16:43:48.624] [info] [graph][runtime.node] worker.thread_cpu node=27 available=1 cpu_ns=78125000 process_calls=19530
ModLoad: 00007ff9`027a0000 00007ff9`02c27000   C:\WINDOWS\System32\SETUPAPI.dll
ModLoad: 00007ff9`01e00000 00007ff9`01ed9000   C:\WINDOWS\System32\OLEAUT32.dll
ModLoad: 00007ff8`ccf20000 00007ff8`ccfcf000   C:\WINDOWS\System32\DriverStore\FileRepository\nvami.inf_amd64_97bcbdbdb2ab507c\nvdxgdmal64.dll
ModLoad: 00007ff8`cbed0000 00007ff8`cccd2000   C:\WINDOWS\System32\DriverStore\FileRepository\nvami.inf_amd64_97bcbdbdb2ab507c\nvobjectloader64.dll
ModLoad: 00007fff`84280000 00007fff`86133000   C:\WINDOWS\SYSTEM32\nvcuvid.dll
ModLoad: 00007ff9`00870000 00007ff9`008f2000   C:\WINDOWS\System32\WINTRUST.dll
ModLoad: 00007ff9`02150000 00007ff9`0215a000   C:\WINDOWS\System32\NSI.dll
ModLoad: 00007fff`b1610000 00007fff`b1715000   C:\WINDOWS\SYSTEM32\nvEncodeAPI64.dll
(1e80.8e08): Break instruction exception - code 80000003 (first chance)
ntdll!DbgBreakPoint:
00007ff9`03083f40 cc              int     3
0:131> sxe -c ".exr -1;.ecxr;~* kb;.dump /ma D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r294-crash.dmp;qd" 0xc0000409;g
(1e80.b570): C++ EH exception - code e06d7363 (first chance)
(1e80.b570): Security check failure or stack buffer overrun - code c0000409 (!!! second chance !!!)
Subcode: 0x7 FAST_FAIL_FATAL_APP_EXIT 
ucrtbase!abort+0x4e:
00007ff9`002b527e cd29            int     29h
NatVis script unloaded from 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\Visualizers\atlmfc.natvis'
NatVis script unloaded from 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\Visualizers\ObjectiveC.natvis'
NatVis script unloaded from 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\Visualizers\concurrency.natvis'
NatVis script unloaded from 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\Visualizers\cpp_rest.natvis'
NatVis script unloaded from 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\Visualizers\stl.natvis'
NatVis script unloaded from 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\Visualizers\Windows.Data.Json.natvis'
NatVis script unloaded from 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\Visualizers\Windows.Devices.Geolocation.natvis'
NatVis script unloaded from 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\Visualizers\Windows.Devices.Sensors.natvis'
NatVis script unloaded from 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\Visualizers\Windows.Media.natvis'
NatVis script unloaded from 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\Visualizers\windows.natvis'
NatVis script unloaded from 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\Visualizers\winrt.natvis'
[in @ 000001907bcbbc80] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001907bcbbc80] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 6418.075989
[in @ 000001907bcbb400] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001907bcbb400] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 7933.417522
[in @ 0000019023b50100] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000019023b50100] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 6579.713189
[in @ 000001907bcb63c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001907bcb63c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 6418.075989
[in @ 000001901df32d80] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001901973ee40] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001901df32d80] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 7933.417522
[in @ 000001901973ee40] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 6579.713189
~~~~

## 清理清单

composition-r294-cdb.log
composition-r294-cli-error.log
composition-r294-cli.log
composition-r294-monitor.log
composition-r294-source0-stdout.log
composition-r294-source0.log
composition-r294-source1-stdout.log
composition-r294-source1.log
composition-r294-source2-stdout.log
composition-r294-source2.log
composition-r294-vlc.log
