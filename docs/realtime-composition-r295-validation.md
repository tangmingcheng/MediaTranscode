# r295：调试附加扰动下的错误清理复核

原规格三源H.264→HEVC/AAC MPEG-TS/UDP，1280×72030、8000kbps CBR/GOP60、AAC192kbps CBR。仅诊断，FAIL。CLI40796，源43520/43548/30828，VLC41360。

附加cdb暂停导致sender超期：deadline25815303450ns，observed26272370000ns。未捕获C++/fast-fail，无dump；CLI正常报告错误退出1，最终payload0、25683次预约/释放匹配。cdb退出1与目标错误退出相符，不能将其认定调试器命令失败或崩溃。源/VLC随后中止清理，源退出-1，非自然结束。无重入/快照。

对比r294：错误清理并非每次引发fast-fail。下一诊断应同时保留C++首次异常栈与fast-fail第二次异常栈，不能只凭ucrtbase模块归因。

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r295-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r295-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r295-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r295-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r295-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r295-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r295 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62020 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r295-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r295-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r295-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r295-native- --snapshot-format=png udp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r295-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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


& 'C:/Program Files (x86)/Windows Kits/10/Debuggers/x64/cdb.exe' -p 40796 -G -logo 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r295-cdb.log' -y 'D:/Code/MyCode/MediaTranscode/out/build/x64-release' -c 'sxe -c ".exr -1;.ecxr;~* kb;.dump /ma D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r295-crash.dmp;qd" -c2 ".exr -1;.ecxr;~* kb;.dump /ma D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r295-crash.dmp;qd" 0xc0000409;g'
~~~~

~~~~text
[CLI] composition_inputs=3 grid=fixed audio_source=2 audio=on max_duration=source_driven hw=planner-highest-score
[2026-10-21 16:45:03.507] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-10-21 16:45:03.514] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-10-21 16:45:04.088] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-21 16:45:04.089] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-21 16:45:04.131] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-21 16:45:04.132] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-21 16:45:04.171] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-21 16:45:04.171] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-21 16:45:08.497] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-10-21 16:45:08.604] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-10-21 16:45:08.645] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-10-21 16:45:08.645] [info] [graph][planner.capability] canvas_cuda_stream stage=prepared device_stream=0000000000000000 private_nonblocking=1
[2026-10-21 16:45:08.652] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-21 16:45:08.652] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-21 16:45:08.652] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-10-21 16:45:08.658] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-21 16:45:08.658] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-21 16:45:08.658] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-10-21 16:45:08.664] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-21 16:45:08.664] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-21 16:45:08.664] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-10-21 16:45:08.686] [info] [graph][planner.capability] composition_preparation_storage stage=graph_bound scope=shared_reservations capacity=2147483648 retained=848868268 high_water=866309756
[CLI] audio_plan branch=transcode_frame reason=transcode_required_by_output_frame_contract codec=aac sample_rate=44100 channels=2 access_unit_samples=1024 encoder=aac bitrate_kbps=192
[CLI] prepared_output_id=1
[CLI] selected_chain=fixed-grid-composition score=0 decoder=h264,h264,h264 filter=scale_cuda=640:360,scale_cuda=640:360,scale_cuda=640:360 encoder=hevc_nvenc
[2026-10-21 16:45:08.688] [info] [graph][runtime.lifecycle] compile.begin
[2026-10-21 16:45:08.688] [info] [graph][runtime.channel] create channel=1 edge=1 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-21 16:45:08.688] [info] [graph][runtime.channel] create channel=2 edge=2 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-21 16:45:08.689] [info] [graph][runtime.channel] create channel=3 edge=3 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-21 16:45:08.689] [info] [graph][runtime.channel] create channel=4 edge=4 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.689] [info] [graph][runtime.channel] create channel=5 edge=5 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.689] [info] [graph][runtime.channel] create channel=6 edge=6 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.689] [info] [graph][runtime.channel] create channel=7 edge=7 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.689] [info] [graph][runtime.channel] create channel=8 edge=8 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.689] [info] [graph][runtime.channel] create channel=9 edge=9 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:45:08.689] [info] [graph][runtime.channel] create channel=10 edge=10 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.689] [info] [graph][runtime.channel] create channel=11 edge=11 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.689] [info] [graph][runtime.channel] create channel=12 edge=12 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:45:08.689] [info] [graph][runtime.channel] create channel=13 edge=13 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:45:08.690] [info] [graph][runtime.channel] create channel=14 edge=14 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.690] [info] [graph][runtime.channel] create channel=15 edge=15 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.690] [info] [graph][runtime.channel] create channel=16 edge=16 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.690] [info] [graph][runtime.channel] create channel=17 edge=17 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.690] [info] [graph][runtime.channel] create channel=18 edge=18 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.690] [info] [graph][runtime.channel] create channel=19 edge=19 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.690] [info] [graph][runtime.channel] create channel=20 edge=20 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.690] [info] [graph][runtime.channel] create channel=21 edge=21 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-10-21 16:45:08.690] [info] [graph][runtime.channel] create channel=22 edge=22 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-21 16:45:08.690] [info] [graph][runtime.channel] create channel=23 edge=23 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:45:08.691] [info] [graph][runtime.channel] create channel=24 edge=24 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-21 16:45:08.691] [info] [graph][runtime.channel] create channel=25 edge=25 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:45:08.691] [info] [graph][runtime.channel] create channel=26 edge=26 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:45:08.691] [info] [graph][runtime.channel] create channel=27 edge=27 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:45:08.691] [info] [graph][runtime.channel] create channel=28 edge=28 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:45:08.691] [info] [graph][runtime.channel] create channel=29 edge=29 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:45:08.692] [info] [graph][runtime.channel] create channel=30 edge=30 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.692] [info] [graph][runtime.channel] create channel=31 edge=31 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.692] [info] [graph][runtime.channel] create channel=32 edge=32 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.692] [info] [graph][runtime.channel] create channel=33 edge=33 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.692] [info] [graph][runtime.channel] create channel=34 edge=34 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.692] [info] [graph][runtime.channel] create channel=35 edge=35 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.692] [info] [graph][runtime.channel] create channel=36 edge=36 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:45:08.692] [info] [graph][runtime.channel] create channel=37 edge=37 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.692] [info] [graph][runtime.channel] create channel=38 edge=38 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.692] [info] [graph][runtime.channel] create channel=39 edge=39 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:45:08.692] [info] [graph][runtime.channel] create channel=40 edge=40 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:45:08.692] [info] [graph][runtime.channel] create channel=41 edge=41 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.693] [info] [graph][runtime.channel] create channel=42 edge=42 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.693] [info] [graph][runtime.channel] create channel=43 edge=43 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.693] [info] [graph][runtime.channel] create channel=44 edge=44 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.693] [info] [graph][runtime.channel] create channel=45 edge=45 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.693] [info] [graph][runtime.channel] create channel=46 edge=46 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.693] [info] [graph][runtime.channel] create channel=47 edge=47 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.693] [info] [graph][runtime.channel] create channel=48 edge=48 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-10-21 16:45:08.693] [info] [graph][runtime.channel] create channel=49 edge=49 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-21 16:45:08.693] [info] [graph][runtime.channel] create channel=50 edge=50 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:45:08.693] [info] [graph][runtime.channel] create channel=51 edge=51 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-21 16:45:08.693] [info] [graph][runtime.channel] create channel=52 edge=52 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:45:08.693] [info] [graph][runtime.channel] create channel=53 edge=53 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:45:08.693] [info] [graph][runtime.channel] create channel=54 edge=54 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:45:08.693] [info] [graph][runtime.channel] create channel=55 edge=55 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:45:08.693] [info] [graph][runtime.channel] create channel=56 edge=56 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:45:08.693] [info] [graph][runtime.channel] create channel=57 edge=57 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.694] [info] [graph][runtime.channel] create channel=58 edge=58 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.694] [info] [graph][runtime.channel] create channel=59 edge=59 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.694] [info] [graph][runtime.channel] create channel=60 edge=60 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.694] [info] [graph][runtime.channel] create channel=61 edge=61 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.694] [info] [graph][runtime.channel] create channel=62 edge=62 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.694] [info] [graph][runtime.channel] create channel=63 edge=63 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.694] [info] [graph][runtime.channel] create channel=64 edge=64 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.694] [info] [graph][runtime.channel] create channel=65 edge=65 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.694] [info] [graph][runtime.channel] create channel=66 edge=66 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:45:08.694] [info] [graph][runtime.channel] create channel=67 edge=67 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:45:08.694] [info] [graph][runtime.channel] create channel=68 edge=68 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.694] [info] [graph][runtime.channel] create channel=69 edge=69 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.694] [info] [graph][runtime.channel] create channel=70 edge=70 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.694] [info] [graph][runtime.channel] create channel=71 edge=71 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:45:08.694] [info] [graph][runtime.channel] create channel=72 edge=72 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:45:08.695] [info] [graph][runtime.channel] create channel=73 edge=73 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:45:08.695] [info] [graph][runtime.channel] create channel=74 edge=74 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:45:08.695] [info] [graph][runtime.channel] create channel=75 edge=75 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.695] [info] [graph][runtime.channel] create channel=76 edge=76 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.695] [info] [graph][runtime.channel] create channel=77 edge=77 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.695] [info] [graph][runtime.channel] create channel=78 edge=78 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.695] [info] [graph][runtime.channel] create channel=79 edge=79 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.695] [info] [graph][runtime.channel] create channel=80 edge=80 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.695] [info] [graph][runtime.channel] create channel=81 edge=81 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.696] [info] [graph][runtime.channel] create channel=82 edge=82 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.696] [info] [graph][runtime.channel] create channel=83 edge=83 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-10-21 16:45:08.696] [info] [graph][runtime.channel] create channel=84 edge=84 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-21 16:45:08.696] [info] [graph][runtime.channel] create channel=85 edge=85 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:45:08.696] [info] [graph][runtime.channel] create channel=86 edge=86 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-21 16:45:08.696] [info] [graph][runtime.channel] create channel=87 edge=87 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:45:08.696] [info] [graph][runtime.channel] create channel=88 edge=88 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:45:08.696] [info] [graph][runtime.channel] create channel=89 edge=89 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:45:08.696] [info] [graph][runtime.channel] create channel=90 edge=90 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:45:08.696] [info] [graph][runtime.channel] create channel=91 edge=91 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-10-21 16:45:08.696] [info] [graph][runtime.channel] create channel=92 edge=92 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-21 16:45:08.696] [info] [graph][runtime.channel] create channel=93 edge=93 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:45:08.696] [info] [graph][runtime.channel] create channel=94 edge=94 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-21 16:45:08.697] [info] [graph][runtime.channel] create channel=95 edge=95 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:45:08.697] [info] [graph][runtime.channel] create channel=96 edge=96 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:45:08.697] [info] [graph][runtime.channel] create channel=97 edge=97 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:45:08.697] [info] [graph][runtime.channel] create channel=98 edge=98 stream=Audio edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.697] [info] [graph][runtime.channel] create channel=99 edge=99 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:45:08.697] [info] [graph][runtime.channel] create channel=100 edge=100 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.697] [info] [graph][runtime.channel] create channel=101 edge=101 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0
[2026-10-21 16:45:08.697] [info] [graph][runtime.channel] create channel=102 edge=102 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-21 16:45:08.697] [info] [graph][runtime.channel] create channel=103 edge=103 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-21 16:45:08.697] [info] [graph][runtime.channel] create channel=104 edge=104 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:45:08.697] [info] [graph][runtime.channel] create channel=105 edge=105 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0
[2026-10-21 16:45:08.697] [info] [graph][runtime.channel] create channel=106 edge=106 stream=Video edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-21 16:45:08.697] [info] [graph][runtime.channel] create channel=107 edge=107 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-21 16:45:08.698] [info] [graph][runtime.channel] create channel=108 edge=108 stream=Any edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-21 16:45:08.698] [info] [graph][runtime.channel] create channel=109 edge=109 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.698] [info] [graph][runtime.channel] create channel=110 edge=110 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.698] [info] [graph][runtime.channel] create channel=111 edge=111 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-21 16:45:08.698] [info] [graph][runtime.channel] create channel=112 edge=112 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:45:08.698] [info] [graph][runtime.channel] create channel=113 edge=113 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-21 16:45:08.698] [info] [graph][runtime.channel] create channel=114 edge=114 stream=Any edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-21 16:45:08.698] [info] [graph][runtime.channel] create channel=115 edge=115 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-21 16:45:08.698] [info] [graph][runtime.channel] create channel=116 edge=116 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-21 16:45:08.698] [info] [graph][runtime.channel] create channel=117 edge=117 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-21 16:45:08.698] [info] [graph][runtime.channel] create channel=118 edge=118 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-21 16:45:08.698] [info] [graph][runtime.channel] create channel=119 edge=119 stream=Any edge_kind=EncodedPacket payload=Unknown queued=0
[2026-10-21 16:45:08.698] [info] [graph][runtime.channel] create channel=120 edge=120 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-21 16:45:08.698] [info] [graph][runtime.channel] create channel=121 edge=121 stream=Metadata edge_kind=Unknown payload=Unknown queued=0
[2026-10-21 16:45:08.698] [info] [graph][runtime.channel] create channel=122 edge=122 stream=Metadata edge_kind=Unknown payload=Unknown queued=0
[2026-10-21 16:45:08.699] [info] [graph][runtime.lifecycle] compiled nodes=81 edges=122 channels=122 execution_order=1->3->8->27->46->47->9->21->28->40->63->48->69->10->29->49->11->12->30->31->50->51->52->13->32->53->14->17->33->36->54->55->59->15->34->56->57->16->35->58->18->37->60->19->38->61->20->39->62->22->41->64->70->23->42->65->71->24->43->66->72->26->45->68->73->25->44->67->6->2->7->80->77->4->5->74->75->78->76->79->81
[2026-10-21 16:45:08.699] [info] [graph][runtime.lifecycle] compile.done state=DefaultRegistrationPending
[2026-10-21 16:45:08.699] [info] [graph][runtime.node] register node=1 name=composition-r295.output.video.codec_resolver kind=CodecResolver
[2026-10-21 16:45:08.700] [info] [graph][runtime.node] register node=2 name=composition-r295.output.video.encode kind=VideoEncode
[2026-10-21 16:45:08.700] [info] [graph][runtime.node] register node=3 name=composition-r295.output.audio.codec_resolver kind=AudioCodecResolver
[2026-10-21 16:45:08.700] [info] [graph][runtime.node] register node=4 name=composition-r295.output.audio.encode kind=AudioEncode
[2026-10-21 16:45:08.700] [info] [graph][runtime.node] register node=5 name=composition-r295.output.audio.canonicalizer kind=EncodedAudioCanonicalizer
[2026-10-21 16:45:08.700] [info] [graph][runtime.node] register node=8 name=composition-r295.source_0.input kind=RawRtpInput
[2026-10-21 16:45:08.700] [info] [graph][runtime.node] register node=9 name=composition-r295.source_0.rtp.clock_group kind=RtpClockGroup
[2026-10-21 16:45:08.700] [info] [graph][runtime.node] register node=10 name=composition-r295.source_0.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-10-21 16:45:08.700] [info] [graph][runtime.node] register node=11 name=composition-r295.source_0.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-10-21 16:45:08.700] [info] [graph][runtime.node] register node=12 name=composition-r295.source_0.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-10-21 16:45:08.700] [info] [graph][runtime.node] register node=13 name=composition-r295.source_0.av_sync.source_clock kind=SourceClockStateFanout
[2026-10-21 16:45:08.701] [info] [graph][runtime.node] register node=14 name=composition-r295.source_0.av_sync.video.generation_gate kind=LockedPacketGate
[2026-10-21 16:45:08.701] [info] [graph][runtime.node] register node=15 name=composition-r295.source_0.av_sync.video.canonical_input kind=CanonicalInput
[2026-10-21 16:45:08.702] [info] [graph][runtime.node] register node=16 name=composition-r295.source_0.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-10-21 16:45:08.702] [info] [graph][runtime.node] register node=17 name=composition-r295.source_0.av_sync.startup.clock kind=AvStartupClock
[2026-10-21 16:45:08.702] [info] [graph][runtime.node] register node=18 name=composition-r295.source_0.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-10-21 16:45:08.702] [info] [graph][runtime.node] register node=20 name=composition-r295.source_0.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-10-21 16:45:08.702] [info] [graph][runtime.node] register node=21 name=composition-r295.source_0.video.codec_resolver kind=CodecResolver
[2026-10-21 16:45:08.702] [info] [graph][runtime.node] register node=22 name=composition-r295.source_0.video.decode kind=VideoDecode
[2026-10-21 16:45:08.702] [info] [graph][runtime.node] register node=23 name=composition-r295.source_0.video.hwtransfer kind=HardwareTransfer
[2026-10-21 16:45:08.702] [info] [graph][runtime.node] register node=24 name=composition-r295.source_0.video.framerate kind=VideoFrameRate
[2026-10-21 16:45:08.702] [info] [graph][runtime.node] register node=25 name=composition-r295.source_0.video.preparation kind=VideoPreparation
[2026-10-21 16:45:08.702] [info] [graph][runtime.node] register node=26 name=composition-r295.source_0.video.filter kind=VideoFilter
[2026-10-21 16:45:08.702] [info] [graph][runtime.node] register node=27 name=composition-r295.source_1.input kind=RawRtpInput
[2026-10-21 16:45:08.702] [info] [graph][runtime.node] register node=28 name=composition-r295.source_1.rtp.clock_group kind=RtpClockGroup
[2026-10-21 16:45:08.702] [info] [graph][runtime.node] register node=29 name=composition-r295.source_1.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-10-21 16:45:08.702] [info] [graph][runtime.node] register node=30 name=composition-r295.source_1.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-10-21 16:45:08.702] [info] [graph][runtime.node] register node=31 name=composition-r295.source_1.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-10-21 16:45:08.702] [info] [graph][runtime.node] register node=32 name=composition-r295.source_1.av_sync.source_clock kind=SourceClockStateFanout
[2026-10-21 16:45:08.703] [info] [graph][runtime.node] register node=33 name=composition-r295.source_1.av_sync.video.generation_gate kind=LockedPacketGate
[2026-10-21 16:45:08.703] [info] [graph][runtime.node] register node=34 name=composition-r295.source_1.av_sync.video.canonical_input kind=CanonicalInput
[2026-10-21 16:45:08.703] [info] [graph][runtime.node] register node=35 name=composition-r295.source_1.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-10-21 16:45:08.703] [info] [graph][runtime.node] register node=36 name=composition-r295.source_1.av_sync.startup.clock kind=AvStartupClock
[2026-10-21 16:45:08.703] [info] [graph][runtime.node] register node=37 name=composition-r295.source_1.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-10-21 16:45:08.703] [info] [graph][runtime.node] register node=39 name=composition-r295.source_1.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-10-21 16:45:08.703] [info] [graph][runtime.node] register node=40 name=composition-r295.source_1.video.codec_resolver kind=CodecResolver
[2026-10-21 16:45:08.704] [info] [graph][runtime.node] register node=41 name=composition-r295.source_1.video.decode kind=VideoDecode
[2026-10-21 16:45:08.704] [info] [graph][runtime.node] register node=42 name=composition-r295.source_1.video.hwtransfer kind=HardwareTransfer
[2026-10-21 16:45:08.704] [info] [graph][runtime.node] register node=43 name=composition-r295.source_1.video.framerate kind=VideoFrameRate
[2026-10-21 16:45:08.704] [info] [graph][runtime.node] register node=44 name=composition-r295.source_1.video.preparation kind=VideoPreparation
[2026-10-21 16:45:08.704] [info] [graph][runtime.node] register node=45 name=composition-r295.source_1.video.filter kind=VideoFilter
[2026-10-21 16:45:08.704] [info] [graph][runtime.node] register node=46 name=composition-r295.source_2.video.input kind=RawRtpInput
[2026-10-21 16:45:08.705] [info] [graph][runtime.node] register node=47 name=composition-r295.source_2.audio.input kind=RawRtpInput
[2026-10-21 16:45:08.705] [info] [graph][runtime.node] register node=48 name=composition-r295.source_2.rtp.clock_group kind=RtpClockGroup
[2026-10-21 16:45:08.705] [info] [graph][runtime.node] register node=49 name=composition-r295.source_2.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-10-21 16:45:08.705] [info] [graph][runtime.node] register node=50 name=composition-r295.source_2.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-10-21 16:45:08.705] [info] [graph][runtime.node] register node=51 name=composition-r295.source_2.av_sync.audio.protocol_binder kind=RtpPacketClockBinder
[2026-10-21 16:45:08.705] [info] [graph][runtime.node] register node=52 name=composition-r295.source_2.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-10-21 16:45:08.705] [info] [graph][runtime.node] register node=53 name=composition-r295.source_2.av_sync.source_clock kind=SourceClockStateFanout
[2026-10-21 16:45:08.705] [info] [graph][runtime.node] register node=54 name=composition-r295.source_2.av_sync.video.generation_gate kind=LockedPacketGate
[2026-10-21 16:45:08.705] [info] [graph][runtime.node] register node=55 name=composition-r295.source_2.av_sync.audio.generation_gate kind=LockedPacketGate
[2026-10-21 16:45:08.705] [info] [graph][runtime.node] register node=56 name=composition-r295.source_2.av_sync.video.canonical_input kind=CanonicalInput
[2026-10-21 16:45:08.705] [info] [graph][runtime.node] register node=57 name=composition-r295.source_2.av_sync.audio.canonical_input kind=CanonicalInput
[2026-10-21 16:45:08.705] [info] [graph][runtime.node] register node=58 name=composition-r295.source_2.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-10-21 16:45:08.706] [info] [graph][runtime.node] register node=59 name=composition-r295.source_2.av_sync.startup.clock kind=AvStartupClock
[2026-10-21 16:45:08.706] [info] [graph][runtime.node] register node=60 name=composition-r295.source_2.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-10-21 16:45:08.706] [info] [graph][runtime.node] register node=62 name=composition-r295.source_2.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-10-21 16:45:08.706] [info] [graph][runtime.node] register node=63 name=composition-r295.source_2.video.codec_resolver kind=CodecResolver
[2026-10-21 16:45:08.706] [info] [graph][runtime.node] register node=64 name=composition-r295.source_2.video.decode kind=VideoDecode
[2026-10-21 16:45:08.706] [info] [graph][runtime.node] register node=65 name=composition-r295.source_2.video.hwtransfer kind=HardwareTransfer
[2026-10-21 16:45:08.706] [info] [graph][runtime.node] register node=66 name=composition-r295.source_2.video.framerate kind=VideoFrameRate
[2026-10-21 16:45:08.706] [info] [graph][runtime.node] register node=67 name=composition-r295.source_2.video.preparation kind=VideoPreparation
[2026-10-21 16:45:08.706] [info] [graph][runtime.node] register node=68 name=composition-r295.source_2.video.filter kind=VideoFilter
[2026-10-21 16:45:08.706] [info] [graph][runtime.node] register node=69 name=composition-r295.source_2.audio.encode.codec_resolver kind=AudioCodecResolver
[2026-10-21 16:45:08.706] [info] [graph][runtime.node] register node=70 name=composition-r295.source_2.audio.encode.decode kind=AudioDecode
[2026-10-21 16:45:08.706] [info] [graph][runtime.node] register node=71 name=composition-r295.source_2.audio.encode.startup_trim kind=Unknown
[2026-10-21 16:45:08.706] [info] [graph][runtime.node] register node=72 name=composition-r295.source_2.audio.encode.drift_controller kind=AudioDriftController
[2026-10-21 16:45:08.706] [info] [graph][runtime.node] register node=73 name=composition-r295.source_2.audio.encode.resample kind=AudioResample
[2026-10-21 16:45:08.706] [info] [graph][runtime.node] register node=74 name=composition-r295.output.scheduler.scheduler kind=AvOutputScheduler
[2026-10-21 16:45:08.706] [info] [graph][runtime.node] register node=75 name=composition-r295.output.scheduler.router kind=ScheduledOutputRouter
[2026-10-21 16:45:08.707] [info] [graph][runtime.node] register node=76 name=composition-r295.output.mpegts.mux kind=FileMux
[2026-10-21 16:45:08.707] [info] [graph][runtime.node] register node=77 name=composition-r295.output.mpegts.plan.source kind=Unknown
[2026-10-21 16:45:08.707] [info] [graph][runtime.node] register node=78 name=composition-r295.output.mpegts.scheduled.adapter kind=Unknown
[2026-10-21 16:45:08.707] [info] [graph][runtime.node] register node=79 name=composition-r295.output.mpegts.datagram.materializer kind=MpegTsDatagramMaterializer
[2026-10-21 16:45:08.707] [info] [graph][runtime.node] register node=80 name=composition-r295.output.mpegts.transport.plan kind=DatagramTransportPlanSource
[2026-10-21 16:45:08.707] [info] [graph][runtime.node] register node=81 name=composition-r295.output.mpegts.transport.sender kind=ScheduledDatagramSender
[2026-10-21 16:45:08.707] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=waiting_for_random_access detail="" description=""
[2026-10-21 16:45:08.707] [info] [graph][runtime.lifecycle] start.begin mode=threaded
[2026-10-21 16:45:08.709] [info] [graph][runtime.lifecycle] worker.finished node=1
[2026-10-21 16:45:08.709] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:45:08.709] [info] [graph][runtime.node] worker.thread_cpu node=1 available=1 cpu_ns=0 process_calls=1
[2026-10-21 16:45:08.710] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:45:08.710] [info] [graph][runtime.lifecycle] start.done state=ThreadedRunning
[2026-10-21 16:45:08.710] [info] [graph][runtime.lifecycle] worker.finished node=63
[2026-10-21 16:45:08.710] [info] [graph][runtime.node] worker.thread_cpu node=63 available=1 cpu_ns=0 process_calls=2
[2026-10-21 16:45:08.711] [info] [graph][runtime.lifecycle] worker.finished node=3
[2026-10-21 16:45:08.711] [info] [graph][runtime.node] worker.thread_cpu node=3 available=1 cpu_ns=0 process_calls=1
[2026-10-21 16:45:08.712] [info] [graph][runtime.lifecycle] worker.finished node=21
[2026-10-21 16:45:08.712] [info] [graph][runtime.node] worker.thread_cpu node=21 available=1 cpu_ns=0 process_calls=2
[2026-10-21 16:45:08.713] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-10-21 16:45:08.713] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=1 generation=1
[2026-10-21 16:45:08.714] [info] [graph][runtime.lifecycle] worker.finished node=40
[2026-10-21 16:45:08.714] [info] [graph][runtime.node] worker.thread_cpu node=40 available=1 cpu_ns=0 process_calls=1
[2026-10-21 16:45:08.714] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=3948005681 dts=3948005681 size=33389 generation=1 sequence=1
[2026-10-21 16:45:08.717] [info] [graph][runtime.lifecycle] worker.finished node=69
[2026-10-21 16:45:08.717] [info] [graph][runtime.node] worker.thread_cpu node=69 available=1 cpu_ns=0 process_calls=1
[2026-10-21 16:45:08.717] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-10-21 16:45:08.718] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=1 generation=1
[2026-10-21 16:45:08.718] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=3005418352 dts=3005418352 size=33389 generation=1 sequence=1
[2026-10-21 16:45:08.719] [info] [graph][runtime.node] video_encode.bind_encoder codec=hevc_nvenc pix_fmt=cuda rate_control=cbr bitrate=8000000 min_bitrate=8000000 max_bitrate=8000000 buffer_size=267000 fps=30/1 gop=60 frame_kind=hardware hwaccel=cuda hw_device_ctx=set hw_frames_ctx=set
[2026-10-21 16:45:08.720] [info] [graph][runtime.node] audio_encode_trace stage=codec_bound
[2026-10-21 16:45:08.721] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:45:08.723] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-10-21 16:45:08.723] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=15 generation=1
[2026-10-21 16:45:08.724] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=2289121656 dts=2289121656 size=317016 generation=1 sequence=15
[2026-10-21 16:45:08.739] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-10-21 16:45:08.740] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=3948005681 format=cuda size=1280x720 buffer_identity=0x208000000 drm_prime=0 software_frame=0
[2026-10-21 16:45:08.741] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=0000014A56667C40 stream=Video payload=Frame pts=3948005681 dts=3948005681 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:45:08.741] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=0000014A56667C40 stream=Video payload=Frame pts=3948005681 dts=3948005681 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:45:08.741] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=0000014A56667C40 stream=Video payload=Frame pts=3948005681 dts=3948005681 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:45:08.741] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-10-21 16:45:08.742] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=0000014A60BDCF60 stream=Video payload=Frame pts=3948005681 dts=3948005681 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:45:08.742] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=0000014A60BDCF60 stream=Video payload=Frame pts=3948005681 dts=3948005681 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:45:08.742] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-10-21 16:45:08.742] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-10-21 16:45:08.742] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=3005418352 format=cuda size=1280x720 buffer_identity=0x208600000 drm_prime=0 software_frame=0
[2026-10-21 16:45:08.742] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=0000014A566192F0 stream=Video payload=Frame pts=3005418352 dts=3005418352 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:45:08.742] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=0000014A566192F0 stream=Video payload=Frame pts=3005418352 dts=3005418352 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:45:08.743] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=0000014A566192F0 stream=Video payload=Frame pts=3005418352 dts=3005418352 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:45:08.743] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-10-21 16:45:08.743] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=0000014A56619EC0 stream=Video payload=Frame pts=3005418352 dts=3005418352 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:45:08.743] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=0000014A56619EC0 stream=Video payload=Frame pts=3005418352 dts=3005418352 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:45:08.743] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-10-21 16:45:08.750] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-10-21 16:45:08.751] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=2289121656 format=cuda size=1280x720 buffer_identity=0x20b400000 drm_prime=0 software_frame=0
[2026-10-21 16:45:08.751] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=0000014A659F87E0 stream=Video payload=Frame pts=2289121656 dts=2289121656 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:45:08.751] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=0000014A659F87E0 stream=Video payload=Frame pts=2289121656 dts=2289121656 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:45:08.751] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=0000014A659F87E0 stream=Video payload=Frame pts=2289121656 dts=2289121656 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:45:08.751] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-10-21 16:45:08.752] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=0000014A659F93B0 stream=Video payload=Frame pts=2289121656 dts=2289121656 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:45:08.752] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=0000014A659F93B0 stream=Video payload=Frame pts=2289121656 dts=2289121656 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:45:08.753] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-10-21 16:45:08.834] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=80
[2026-10-21 16:45:08.834] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=91
[2026-10-21 16:45:08.835] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=92
[2026-10-21 16:45:08.837] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0x20b320000 drm_prime=0 software_frame=0
[2026-10-21 16:45:08.837] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=18 audio_count=23 first_video_sequence=15
[2026-10-21 16:45:08.837] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0x20b200000 drm_prime=0 software_frame=0
[2026-10-21 16:45:08.837] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=16 audio_count=0 first_video_sequence=1
[2026-10-21 16:45:08.837] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=23
[2026-10-21 16:45:08.837] [info] [graph][runtime.node] audio_decode_trace stage=first_packet
[2026-10-21 16:45:08.838] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=0
[2026-10-21 16:45:08.838] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0x20b290000 drm_prime=0 software_frame=0
[2026-10-21 16:45:08.838] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=16 audio_count=0 first_video_sequence=1
[2026-10-21 16:45:08.838] [info] [graph][runtime.node] audio_decode_trace stage=first_frame samples=1024 rate=44100
[2026-10-21 16:45:08.839] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1 source_master_ns=5521706051 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-10-21 16:45:08.840] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=0
[2026-10-21 16:45:08.840] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3 source_master_ns=5568145960 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=45346
[2026-10-21 16:45:08.841] [info] [graph][runtime.node] audio_encode_trace stage=first_frame
[2026-10-21 16:45:08.841] [info] [graph][runtime.node] mpegts_emission_plan generation=1 mode=canonical_access_unit_window access_unit_window_ns=100000000 video_initial_service_window_ns=13990650 audio_initial_service_window_ns=13990650 packet_size_bytes=188 maximum_payload_bytes=1316 per_datagram_overhead_bytes=0
[2026-10-21 16:45:08.843] [info] [graph][runtime.node] video_encode.input_contract format=cuda size=1280x720 buffer_identity=0x20a000000 drm_prime=0 software_frame=0
[2026-10-21 16:45:08.843] [info] [graph][runtime.node] video_encode.generation_start_key_frame pts=0 generation=1
[2026-10-21 16:45:08.843] [info] [graph][runtime.node] video_encode.first_frame pts=0 generation=attached
[2026-10-21 16:45:08.851] [info] [graph][runtime.node] video_encode.first_submit result=accepted
[2026-10-21 16:45:08.851] [info] [graph][runtime.node] video_encode.first_packet pts=0 dts=0 duration=0 size=39648 side_data_elems=1 received_footprint=39656 side_data[0]=Quality stats:8
[2026-10-21 16:45:08.852] [info] [graph][runtime.node] video_encode.first_packet_footprint bytes=39656 wrapped_size=39648 wrapped_side_data_elems=1
[2026-10-21 16:45:08.852] [info] [graph][runtime.node] av_scheduler_trace stage=first_video_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=5521217968 dispatch_master_ns=5521217968 now_ns=5366270700 node=74 identity_kind=output identity="composition-r295.output" canonical_sequence=1 canonical_created_steady_ns=28892768786200 head_observed_steady_ns=28892768830000 codec_receive_begin_steady_ns=28892768202800 codec_receive_end_steady_ns=28892768203700
[2026-10-21 16:45:08.852] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=5366520000 present_generation=1 present_canonical_target_ns=0 present_canonical_dispatch_ns=0 present_target_master_ns=5521217968 present_dispatch_master_ns=5521217968
[2026-10-21 16:45:08.889] [info] [graph][runtime.node] audio_encode_trace stage=first_packet
[2026-10-21 16:45:08.890] [info] [graph][runtime.node] av_scheduler_trace stage=first_audio_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=5521217968 dispatch_master_ns=5521217968 now_ns=5403771000 node=74 identity_kind=output identity="composition-r295.output" canonical_sequence=1 canonical_created_steady_ns=28892806298400 head_observed_steady_ns=28892806330200 codec_receive_begin_steady_ns=unavailable codec_receive_end_steady_ns=unavailable
[2026-10-21 16:45:09.087] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=running detail="" description=""
[CLI] [2026-10-21 16:45:09.467] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=48 source_master_ns=6613043919 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=90692
runtime report: queued=712, peakQueued=928, threads=81, processThreads=131, workers=75, workerProgress=6884, workerProcessCalls=19654, workerWaits=12764, workerWakeups=12643, workerErrors=0, errors=0, stalledIntervals=0, cpuSamples=2, logicalProcessors=22, averageSystemMachineCpuPercent=8.665438, averageProcessMachineCpuPercent=3.513033, peakProcessMachineCpuPercent=5.420561, averageProcessSingleCoreCpuPercent=77.286719, peakProcessSingleCoreCpuPercent=119.252336, initialWorkingSetBytes=208470016, workingSetBytes=253939712, peakWorkingSetBytes=253939712, totalPushed=6669, totalPopped=5957, droppedBuffers=0, encodedPacketsPushed=195, encodedPacketsPopped=194, backpressureItems=122, graphPayloadCurrentBytes=13823390, graphPayloadHighWaterBytes=17850820, graphPayloadCurrentObjects=772, graphPayloadHighWaterObjects=776, graphPayloadReservations=1803, graphPayloadReleases=1031, graphPayloadPressureFailures=0, lastInputReceivedAtNs=28893372885100
[2026-10-21 16:45:10.490] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=92 source_master_ns=7634721924 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=136038
[2026-10-21 16:45:11.511] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=136 source_master_ns=8656399928 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=181384
[2026-10-21 16:45:12.533] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=180 source_master_ns=9678077932 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=226730
[2026-10-21 16:45:13.510] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1809815419 ntp_seconds=4001561113 ntp_fraction=2186138353 arrival_ns=28897426250200
[2026-10-21 16:45:13.510] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1809815419
[2026-10-21 16:45:13.510] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:45:13.525] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=1992153125 ntp_seconds=4001561113 ntp_fraction=2250562863 arrival_ns=28897441303400
[2026-10-21 16:45:13.525] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1992153125
[2026-10-21 16:45:13.525] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:45:13.525] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2979829113 ntp_seconds=4001561113 ntp_fraction=2254857830 arrival_ns=28897442007900
[2026-10-21 16:45:13.526] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2979829113
[2026-10-21 16:45:13.526] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:45:13.540] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1817192252 ntp_seconds=4001561113 ntp_fraction=2319282339 arrival_ns=28897456592900
[2026-10-21 16:45:13.540] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1817192252
[2026-10-21 16:45:13.540] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:45:13.578] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=225 source_master_ns=10722975891 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=272076
[2026-10-21 16:45:14.600] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=269 source_master_ns=11744653896 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=317422
[2026-10-21 16:45:15.621] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=313 source_master_ns=12766331900 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=362768
[2026-10-21 16:45:16.666] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=358 source_master_ns=13811229859 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=408114
[2026-10-21 16:45:17.688] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=402 source_master_ns=14832907864 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=453460
[2026-10-21 16:45:18.532] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1809815419 ntp_seconds=4001561118 ntp_fraction=2284922601 arrival_ns=28902448926800
[2026-10-21 16:45:18.532] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1809815419
[2026-10-21 16:45:18.533] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:45:18.547] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=1992153125 ntp_seconds=4001561118 ntp_fraction=2349347110 arrival_ns=28902464255600
[2026-10-21 16:45:18.547] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1817192252 ntp_seconds=4001561118 ntp_fraction=2349347110 arrival_ns=28902464281600
[2026-10-21 16:45:18.548] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1817192252
[2026-10-21 16:45:18.548] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1992153125
[2026-10-21 16:45:18.548] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:45:18.548] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:45:18.564] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2979829113 ntp_seconds=4001561118 ntp_fraction=2422361554 arrival_ns=28902480642200
[2026-10-21 16:45:18.564] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2979829113
[2026-10-21 16:45:18.564] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:45:18.709] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=446 source_master_ns=15854585868 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=498806
[2026-10-21 16:45:19.731] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=490 source_master_ns=16876263873 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=544152
[2026-10-21 16:45:20.776] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=535 source_master_ns=17921161832 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=589498
[2026-10-21 16:45:21.797] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=579 source_master_ns=18942839837 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=634844
[2026-10-21 16:45:22.819] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=623 source_master_ns=19964517841 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=680190
[2026-10-21 16:45:23.548] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1809815419 ntp_seconds=4001561123 ntp_fraction=2353642078 arrival_ns=28907464587300
[2026-10-21 16:45:23.548] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1809815419
[2026-10-21 16:45:23.548] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:45:23.577] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1817192252 ntp_seconds=4001561123 ntp_fraction=2478196129 arrival_ns=28907493960200
[2026-10-21 16:45:23.577] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1817192252
[2026-10-21 16:45:23.578] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:45:23.594] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2979829113 ntp_seconds=4001561123 ntp_fraction=2551210573 arrival_ns=28907510688700
[2026-10-21 16:45:23.594] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2979829113
[2026-10-21 16:45:23.594] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:45:23.594] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=1992153125 ntp_seconds=4001561123 ntp_fraction=2551210573 arrival_ns=28907511145300
[2026-10-21 16:45:23.595] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1992153125
[2026-10-21 16:45:23.595] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:45:23.869] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=668 source_master_ns=21009415800 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=725536
[2026-10-21 16:45:24.886] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=712 source_master_ns=22031093805 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=770882
[2026-10-21 16:45:25.908] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=756 source_master_ns=23052771809 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=816228
[2026-10-21 16:45:26.929] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=800 source_master_ns=24074449814 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=861574
[2026-10-21 16:45:27.974] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=845 source_master_ns=25119347773 raw_phase_ns=9070 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=906920
[2026-10-21 16:45:28.555] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1809815419 ntp_seconds=4001561128 ntp_fraction=2383706849 arrival_ns=28912471851000
[2026-10-21 16:45:28.556] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1809815419
[2026-10-21 16:45:28.556] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:45:28.602] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=1817192252 ntp_seconds=4001561128 ntp_fraction=2585570312 arrival_ns=28912518668600
[2026-10-21 16:45:28.602] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1817192252
[2026-10-21 16:45:28.602] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:45:28.603] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=1992153125 ntp_seconds=4001561128 ntp_fraction=2589865279 arrival_ns=28912519530100
[2026-10-21 16:45:28.603] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=1992153125
[2026-10-21 16:45:28.603] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:45:28.617] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2979829113 ntp_seconds=4001561128 ntp_fraction=2649994821 arrival_ns=28912533800400
[2026-10-21 16:45:28.618] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2979829113
[2026-10-21 16:45:28.618] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:45:28.996] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=889 source_master_ns=26141025778 raw_phase_ns=9071 filtered_phase_ns=9071 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=952266
[2026-10-21 16:45:29.758] [info] [graph][runtime.node] process.failed node=81 name=MediaScheduledDatagramSenderNode kind=ScheduledDatagramSender inputs=[channel=111 edge=111 stream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=122 edge=122 stream=Metadata edge_kind=Unknown payload=Unknown queued=0] outputs=[] error=IoFailure: scheduled datagram submit exceeded its original deadline generation=1 sequence=14644 endpoint=1 release_ns=25715458330 reserved_at_ns=25701857100 not_before_ns=25715458330 deadline_ns=25815303450 observed_at_ns=26272370000
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.failed node=81 kind=ScheduledDatagramSender name=Common GCRA-paced nonblocking datagram sender error=IoFailure: scheduled datagram submit exceeded its original deadline generation=1 sequence=14644 endpoint=1 release_ns=25715458330 reserved_at_ns=25701857100 not_before_ns=25715458330 deadline_ns=25815303450 observed_at_ns=26272370000
[2026-10-21 16:45:29.758] [info] [graph][runtime.node] protocol_batch_trace stage=materialization_failed generation=1 attempts=1 consumed_datagrams=0 remaining_datagrams=1 attempted_master_ns=26272571200 mux_dequeued_master_ns=26272336900 first_dequeued_master_ns=26272570800 last_wire_published_master_ns=25701839000 previous_attempt_started_master_ns=unavailable previous_attempt_completed_master_ns=unavailable last_attempt_completed_master_ns=26272575400 last_blocked_started_master_ns=unavailable last_blocked_completed_master_ns=unavailable blocked_attempts=0 remaining_front_release_master_ns=25754551300 remaining_front_deadline_master_ns=25854551300 original_error="InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=25754551300 materialized_ns=26272571200 protocol_deadline_ns=25854551300 deadline_before_release=0 materialized_after_deadline=1"
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=48 available=1 cpu_ns=0 process_calls=29
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=30 available=1 cpu_ns=0 process_calls=2154
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=31 available=1 cpu_ns=0 process_calls=15
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=50 available=1 cpu_ns=31250000 process_calls=2150
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=51 available=1 cpu_ns=0 process_calls=3066
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=11 available=1 cpu_ns=15625000 process_calls=2153
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=52 available=1 cpu_ns=15625000 process_calls=28
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] process.failed node=70 name=AudioDecodeNode kind=AudioDecode inputs=[channel=92 edge=92 stream=Audio edge_kind=Metadata payload=CodecContext queued=0; channel=93 edge=93 stream=Audio edge_kind=InputPacket payload=Packet queued=176] outputs=[channel=95 edge=95 stream=Audio edge_kind=RawFrame payload=Frame queued=1] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=12 available=1 cpu_ns=0 process_calls=16
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=15 available=1 cpu_ns=0 process_calls=2253
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=55 available=1 cpu_ns=31250000 process_calls=3235
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=18 available=1 cpu_ns=0 process_calls=2247
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=37 available=1 cpu_ns=31250000 process_calls=2254
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=59 available=1 cpu_ns=0 process_calls=28
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=60 available=1 cpu_ns=0 process_calls=5252
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=19 available=1 cpu_ns=0 process_calls=2165
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=38 available=1 cpu_ns=31250000 process_calls=2186
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=65 available=1 cpu_ns=0 process_calls=1872
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=45 available=1 cpu_ns=234375000 process_calls=2212
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=68 available=1 cpu_ns=234375000 process_calls=2186
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] process.failed node=46 name=RawRtpInputNode kind=RawRtpInput inputs=[] outputs=[channel=58 edge=58 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=59 edge=59 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=66 edge=66 stream=Video edge_kind=InputPacket payload=Packet queued=0; channel=83 edge=83 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=73 available=1 cpu_ns=62500000 process_calls=4414
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] process.failed node=4 name=AudioEncodeNode kind=AudioEncode inputs=[channel=2 edge=2 stream=Audio edge_kind=Metadata payload=CodecContext queued=0; channel=105 edge=105 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=5] outputs=[channel=3 edge=3 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0; channel=115 edge=115 stream=Audio edge_kind=Metadata payload=CodecContext queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=25 available=1 cpu_ns=46875000 process_calls=3307
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=44 available=1 cpu_ns=31250000 process_calls=3262
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=78 available=1 cpu_ns=15625000 process_calls=4412
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=35 available=1 cpu_ns=15625000 process_calls=2318
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] process.failed node=47 name=RawRtpInputNode kind=RawRtpInput inputs=[] outputs=[channel=60 edge=60 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=61 edge=61 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=67 edge=67 stream=Audio edge_kind=InputPacket payload=Packet queued=0; channel=91 edge=91 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] process.failed node=8 name=RawRtpInputNode kind=RawRtpInput inputs=[] outputs=[channel=4 edge=4 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=5 edge=5 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=9 edge=9 stream=Video edge_kind=InputPacket payload=Packet queued=0; channel=21 edge=21 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=58 available=1 cpu_ns=31250000 process_calls=5404
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=33 available=1 cpu_ns=0 process_calls=2285
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=7 available=1 cpu_ns=78125000 process_calls=3710
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=23 available=1 cpu_ns=0 process_calls=1858
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=9 available=1 cpu_ns=0 process_calls=15
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=28 available=1 cpu_ns=0 process_calls=15
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=42 available=1 cpu_ns=15625000 process_calls=1852
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=10 available=1 cpu_ns=0 process_calls=20
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=29 available=1 cpu_ns=0 process_calls=21
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=72 available=1 cpu_ns=46875000 process_calls=2877
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=49 available=1 cpu_ns=0 process_calls=42
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=32 available=1 cpu_ns=0 process_calls=20
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=13 available=1 cpu_ns=0 process_calls=21
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=53 available=1 cpu_ns=0 process_calls=39
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=17 available=1 cpu_ns=0 process_calls=16
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=14 available=1 cpu_ns=0 process_calls=2284
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=36 available=1 cpu_ns=0 process_calls=15
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=34 available=1 cpu_ns=31250000 process_calls=2282
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=54 available=1 cpu_ns=0 process_calls=2298
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=56 available=1 cpu_ns=0 process_calls=2297
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=16 available=1 cpu_ns=0 process_calls=2310
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=57 available=1 cpu_ns=31250000 process_calls=3257
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=61 available=1 cpu_ns=15625000 process_calls=5075
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=20 available=1 cpu_ns=15625000 process_calls=2183
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=62 available=1 cpu_ns=46875000 process_calls=5052
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=39 available=1 cpu_ns=15625000 process_calls=2162
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=22 available=1 cpu_ns=203125000 process_calls=1592
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=41 available=1 cpu_ns=125000000 process_calls=1662
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=26 available=1 cpu_ns=281250000 process_calls=2161
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=64 available=1 cpu_ns=187500000 process_calls=2432
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=71 available=1 cpu_ns=15625000 process_calls=2938
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=74 available=1 cpu_ns=15625000 process_calls=4459
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=24 available=1 cpu_ns=0 process_calls=1954
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=43 available=1 cpu_ns=15625000 process_calls=1958
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=66 available=1 cpu_ns=15625000 process_calls=1961
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=67 available=1 cpu_ns=62500000 process_calls=3327
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=75 available=1 cpu_ns=0 process_calls=4430
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=2 available=1 cpu_ns=390625000 process_calls=1835
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=80 available=1 cpu_ns=0 process_calls=5
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=77 available=1 cpu_ns=0 process_calls=5
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=5 available=1 cpu_ns=15625000 process_calls=2627
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=76 available=1 cpu_ns=187500000 process_calls=5494
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=81 available=1 cpu_ns=859375000 process_calls=4387
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] process.failed node=27 name=RawRtpInputNode kind=RawRtpInput inputs=[] outputs=[channel=31 edge=31 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=32 edge=32 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=36 edge=36 stream=Video edge_kind=InputPacket payload=Packet queued=0; channel=48 edge=48 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] worker.thread_cpu node=6 available=1 cpu_ns=390625000 process_calls=4378
[2026-10-21 16:45:29.759] [info] [graph][runtime.node] process.failed node=79 name=MediaMpegTsDatagramMaterializerNode kind=MpegTsDatagramMaterializer inputs=[channel=118 edge=118 stream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=120 edge=120 stream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=121 edge=121 stream=Metadata edge_kind=Unknown payload=Unknown queued=10] outputs=[channel=122 edge=122 stream=Metadata edge_kind=Unknown payload=Unknown queued=0] error=InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=25754551300 materialized_ns=26272571200 protocol_deadline_ns=25854551300 deadline_before_release=0 materialized_after_deadline=1
[2026-10-21 16:45:29.761] [info] [graph][runtime.node] worker.thread_cpu node=70 available=1 cpu_ns=78125000 process_calls=4421
[2026-10-21 16:45:29.764] [info] [graph][runtime.node] worker.thread_cpu node=46 available=1 cpu_ns=46875000 process_calls=20143
[2026-10-21 16:45:29.764] [info] [graph][runtime.node] worker.thread_cpu node=4 available=1 cpu_ns=656250000 process_calls=2777
[2026-10-21 16:45:29.765] [info] [graph][runtime.node] worker.thread_cpu node=47 available=1 cpu_ns=46875000 process_calls=18989
[2026-10-21 16:45:29.765] [info] [graph][runtime.node] worker.thread_cpu node=8 available=1 cpu_ns=62500000 process_calls=16974
[2026-10-21 16:45:29.772] [info] [graph][runtime.node] scheduled_datagram_sender stage=failed generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=2198 committed_datagrams=14643 committed_payload_bytes=17591724 would_block=0 writable_waits=0 deadline_misses=1 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=79296 pacing_reserved=14644 pacing_cancelled=0 pacing_submitted=14643 pacing_maximum_submit_lateness_ns=7097866 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=1 backlog_current_wire_bytes=968 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=79296 backlog_max_residence_ns=45922200 last_materialized_sequence=14644 last_scheduled_sequence=14644 last_submitted_sequence=14643 last_committed_sequence=14643 shared_scope_charged_datagrams=14643 shared_scope_charged_wire_bytes=18001728 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2154 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2154 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=188010 endpoint_1_target_effective_socket_bytes=188010 endpoint_1_api_requested_socket_bytes=188010 endpoint_1_effective_socket_bytes=188010 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=14643 endpoint_1_committed_payload_bytes=17591724
[2026-10-21 16:45:29.772] [info] [graph][runtime.node] worker.thread_cpu node=27 available=1 cpu_ns=31250000 process_calls=18493
[2026-10-21 16:45:29.772] [info] [graph][runtime.node] worker.secondary_failure node=79 kind=MpegTsDatagramMaterializer name=MPEG-TS wire protocol materializer error=InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=25754551300 materialized_ns=26272571200 protocol_deadline_ns=25854551300 deadline_before_release=0 materialized_after_deadline=1
[2026-10-21 16:45:29.773] [info] [graph][runtime.node] worker.secondary_failure node=81 kind=ScheduledDatagramSender name=Common GCRA-paced nonblocking datagram sender error=IoFailure: scheduled datagram submit exceeded its original deadline generation=1 sequence=14644 endpoint=1 release_ns=25715458330 reserved_at_ns=25701857100 not_before_ns=25715458330 deadline_ns=25815303450 observed_at_ns=26272370000
[2026-10-21 16:45:29.773] [info] [graph][runtime.node] worker.thread_cpu node=79 available=1 cpu_ns=250000000 process_calls=8190
[2026-10-21 16:45:29.826] [info] [graph][runtime.lifecycle] node_abort_payload node=81 before_objects=771 after_objects=771 before_bytes=14112398 after_bytes=14112398 before_reservations=25683 after_reservations=25683 before_releases=24912 after_releases=24912
[2026-10-21 16:45:29.826] [info] [graph][runtime.node] mpegts_wire_materializer stage=aborted maximum_materialized_after_release_ns=39569892 worst_materialized_at_ns=19127454800 worst_release_ns=19087884908 worst_deadline_ns=19154551576 worst_global_sequence=9954
[2026-10-21 16:45:29.827] [info] [graph][runtime.lifecycle] node_abort_payload node=79 before_objects=771 after_objects=771 before_bytes=14112398 after_bytes=14112398 before_reservations=25683 after_reservations=25683 before_releases=24912 after_releases=24912
[2026-10-21 16:45:29.827] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 datagrams=0 wire_bytes=0 immediate_deadlines=0 deferred_deadlines=0 planned_wait_ns=0 late_datagrams=0 maximum_lateness_ns=0 pending_bytes=0 peak_pending_bytes=94564 pressure_failures=0 access_units=1484 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=403 maximum_mux_input_after_emit_ns=7393532 maximum_mux_input_after_dispatch_ns=-92606468 worst_mux_input_ready_ns=5428611500 worst_mux_input_emit_ns=5421217968 worst_mux_input_dispatch_ns=5521217968 maximum_protocol_batch_produced_after_release_ns=517799992 worst_protocol_batch_produced_ns=26272351700 worst_protocol_batch_release_ns=25754551708 worst_protocol_batch_deadline_ns=25821218376 worst_protocol_batch_datagrams=1 exit_reason=aborted
[2026-10-21 16:45:29.827] [info] [graph][runtime.lifecycle] node_abort_payload node=76 before_objects=771 after_objects=771 before_bytes=14112398 after_bytes=14112398 before_reservations=25683 after_reservations=25683 before_releases=24912 after_releases=24912
[2026-10-21 16:45:29.828] [info] [graph][runtime.lifecycle] node_abort_payload node=78 before_objects=771 after_objects=771 before_bytes=14112398 after_bytes=14112398 before_reservations=25683 after_reservations=25683 before_releases=24912 after_releases=24912
[2026-10-21 16:45:29.828] [info] [graph][runtime.lifecycle] node_abort_payload node=75 before_objects=771 after_objects=771 before_bytes=14112398 after_bytes=14112398 before_reservations=25683 after_reservations=25683 before_releases=24912 after_releases=24912
[2026-10-21 16:45:29.828] [info] [graph][runtime.node] audio_stage_observation node=74 stage=scheduler_input count=875 last_generation=1 last_begin=894976 last_end=896000 last_rate=44100 last_steady_ns=28913099252000 maximum_inter_event_gap_ns=27548900 gap_begin_generation=1 gap_begin_begin=441344 gap_begin_end=442368 gap_begin_rate=44100 gap_begin_steady_ns=28902812104100 gap_end_generation=1 gap_end_begin=442368 gap_end_end=443392 gap_end_rate=44100 gap_end_steady_ns=28902839653000
[2026-10-21 16:45:29.828] [info] [graph][runtime.node] av_scheduler_correlated_wait generation=1 canonical_sequence=1 missing=audio begin_steady_ns=28892769075400 end_steady_ns=28892806601000 observed_wait_ns=37525600 held_dispatch_master_ns=5521217968 begin_master_ns=5366515200 end_master_ns=5404040300 dispatch_remaining_at_end_ns=117177668 resumed_dispatch_master_ns=5521217968
[2026-10-21 16:45:29.828] [info] [graph][runtime.lifecycle] node_abort_payload node=74 before_objects=771 after_objects=770 before_bytes=14112398 after_bytes=14082198 before_reservations=25683 after_reservations=25683 before_releases=24912 after_releases=24913
[2026-10-21 16:45:29.829] [info] [graph][runtime.node] audio_stage_observation node=5 stage=canonicalizer_input count=875 last_generation=1 last_begin=894976 last_end=896000 last_rate=44100 last_steady_ns=28913099221600 maximum_inter_event_gap_ns=27548600 gap_begin_generation=1 gap_begin_begin=441344 gap_begin_end=442368 gap_begin_rate=44100 gap_begin_steady_ns=28902812077800 gap_end_generation=1 gap_end_begin=442368 gap_end_end=443392 gap_end_rate=44100 gap_end_steady_ns=28902839626400
[2026-10-21 16:45:29.829] [info] [graph][runtime.node] audio_stage_observation node=5 stage=canonicalizer_committed count=875 last_generation=1 last_begin=894976 last_end=896000 last_rate=44100 last_steady_ns=28913099238900 maximum_inter_event_gap_ns=27550800 gap_begin_generation=1 gap_begin_begin=441344 gap_begin_end=442368 gap_begin_rate=44100 gap_begin_steady_ns=28902812090000 gap_end_generation=1 gap_end_begin=442368 gap_end_end=443392 gap_end_rate=44100 gap_end_steady_ns=28902839640800
[2026-10-21 16:45:29.829] [info] [graph][runtime.lifecycle] node_abort_payload node=5 before_objects=770 after_objects=770 before_bytes=14082198 after_bytes=14082198 before_reservations=25683 after_reservations=25683 before_releases=24913 after_releases=24913
[2026-10-21 16:45:29.829] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_input count=878 last_generation=1 last_begin=898048 last_end=899072 last_rate=44100 last_steady_ns=28913675161300 maximum_inter_event_gap_ns=577218100 gap_begin_generation=1 gap_begin_begin=897024 gap_begin_end=898048 gap_begin_rate=44100 gap_begin_steady_ns=28913097943200 gap_end_generation=1 gap_end_begin=898048 gap_end_end=899072 gap_end_rate=44100 gap_end_steady_ns=28913675161300
[2026-10-21 16:45:29.829] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_submitted count=878 last_generation=1 last_begin=898048 last_end=899072 last_rate=44100 last_steady_ns=28913675857500 maximum_inter_event_gap_ns=576669500 gap_begin_generation=1 gap_begin_begin=897024 gap_begin_end=898048 gap_begin_rate=44100 gap_begin_steady_ns=28913099188000 gap_end_generation=1 gap_end_begin=898048 gap_end_end=899072 gap_end_rate=44100 gap_end_steady_ns=28913675857500
[2026-10-21 16:45:29.829] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_packet count=875 last_generation=1 last_begin=894976 last_end=896000 last_rate=44100 last_steady_ns=28913099195200 maximum_inter_event_gap_ns=27587700 gap_begin_generation=1 gap_begin_begin=441344 gap_begin_end=442368 gap_begin_rate=44100 gap_begin_steady_ns=28902812010300 gap_end_generation=1 gap_end_begin=442368 gap_end_end=443392 gap_end_rate=44100 gap_end_steady_ns=28902839598000
[2026-10-21 16:45:29.829] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_committed count=875 last_generation=1 last_begin=894976 last_end=896000 last_rate=44100 last_steady_ns=28913099205000 maximum_inter_event_gap_ns=27590100 gap_begin_generation=1 gap_begin_begin=441344 gap_begin_end=442368 gap_begin_rate=44100 gap_begin_steady_ns=28902812018200 gap_end_generation=1 gap_end_begin=442368 gap_end_end=443392 gap_end_rate=44100 gap_end_steady_ns=28902839608300
[2026-10-21 16:45:29.830] [info] [graph][runtime.lifecycle] codec_owner_release node=4 kind=AudioEncode owner_references=4
[2026-10-21 16:45:29.830] [info] [graph][runtime.lifecycle] node_abort_payload node=4 before_objects=770 after_objects=770 before_bytes=14082198 after_bytes=14082198 before_reservations=25683 after_reservations=25683 before_releases=24913 after_releases=24913
[2026-10-21 16:45:29.830] [info] [graph][runtime.lifecycle] node_abort_payload node=77 before_objects=770 after_objects=770 before_bytes=14082198 after_bytes=14082198 before_reservations=25683 after_reservations=25683 before_releases=24913 after_releases=24913
[2026-10-21 16:45:29.830] [info] [graph][runtime.lifecycle] node_abort_payload node=80 before_objects=770 after_objects=770 before_bytes=14082198 after_bytes=14082198 before_reservations=25683 after_reservations=25683 before_releases=24913 after_releases=24913
[2026-10-21 16:45:29.830] [info] [graph][runtime.node] audio_stage_observation node=7 stage=aggregate_built count=883 last_generation=1 last_begin=903168 last_end=904192 last_rate=44100 last_steady_ns=28913675034100 maximum_inter_event_gap_ns=577026200 gap_begin_generation=1 gap_begin_begin=897024 gap_begin_end=898048 gap_begin_rate=44100 gap_begin_steady_ns=28913097917900 gap_end_generation=1 gap_end_begin=898048 gap_end_end=899072 gap_end_rate=44100 gap_end_steady_ns=28913674944100
[2026-10-21 16:45:29.830] [info] [graph][runtime.node] audio_stage_observation node=7 stage=aggregate_committed count=883 last_generation=1 last_begin=903168 last_end=904192 last_rate=44100 last_steady_ns=28913675036000 maximum_inter_event_gap_ns=577022500 gap_begin_generation=1 gap_begin_begin=897024 gap_begin_end=898048 gap_begin_rate=44100 gap_begin_steady_ns=28913097932300 gap_end_generation=1 gap_end_begin=898048 gap_end_end=899072 gap_end_rate=44100 gap_end_steady_ns=28913674954800
[2026-10-21 16:45:29.830] [info] [graph][runtime.node] continuous_audio_aggregate.summary reason=abort output_group=composition-r295.output audio_commits=883 real_samples=904192 silence_samples=0
[2026-10-21 16:45:29.831] [info] [graph][runtime.lifecycle] node_abort_payload node=7 before_objects=770 after_objects=764 before_bytes=14082198 after_bytes=14041238 before_reservations=25683 after_reservations=25683 before_releases=24913 after_releases=24919
[2026-10-21 16:45:29.831] [info] [graph][runtime.lifecycle] codec_owner_release node=2 kind=VideoEncode owner_references=4
[2026-10-21 16:45:29.831] [info] [graph][runtime.node] video_encoder_execution_observation node=2 count=611 maximum_call_ns=7622500 maximum_begin_steady_ns=28892760261100 maximum_end_steady_ns=28892767883600 maximum_encoder_pts=0 maximum_native_result=0 maximum_between_submissions_ns=45487200 before_gap_begin_steady_ns=28899058748400 before_gap_end_steady_ns=28899063654100 before_gap_encoder_pts=189 before_gap_native_result=0 after_gap_begin_steady_ns=28899109141300 after_gap_end_steady_ns=28899111079100 after_gap_encoder_pts=190 after_gap_native_result=0
[2026-10-21 16:45:29.831] [info] [graph][runtime.lifecycle] node_abort_payload node=2 before_objects=764 after_objects=764 before_bytes=14041238 after_bytes=14041238 before_reservations=25683 after_reservations=25683 before_releases=24919 after_releases=24919
[2026-10-21 16:45:29.831] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=process count=4378 maximum_ns=18379800 begin_steady_ns=28899090682600 end_steady_ns=28899109062400 video_cursor=190 audio_cursor=not_owned next_video_cursor=190 next_audio_cursor=not_owned error=None return_state=progress deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-10-21 16:45:29.831] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=purge count=4378 maximum_ns=385400 begin_steady_ns=28910924129100 end_steady_ns=28910924514500 video_cursor=545 audio_cursor=not_owned next_video_cursor=545 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-21 16:45:29.831] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=bind count=4378 maximum_ns=61500 begin_steady_ns=28899090686700 end_steady_ns=28899090748200 video_cursor=190 audio_cursor=not_owned next_video_cursor=190 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-21 16:45:29.831] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=consume count=4378 maximum_ns=9083000 begin_steady_ns=28894327488400 end_steady_ns=28894336571400 video_cursor=48 audio_cursor=not_owned next_video_cursor=48 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-21 16:45:29.832] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=activate count=16 maximum_ns=44200 begin_steady_ns=28892757109600 end_steady_ns=28892757153800 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-21 16:45:29.832] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=build_video count=612 maximum_ns=18277100 begin_steady_ns=28899090764900 end_steady_ns=28899109042000 video_cursor=190 audio_cursor=not_owned next_video_cursor=190 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-21 16:45:29.832] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=canvas count=612 maximum_ns=18243700 begin_steady_ns=28899090776000 end_steady_ns=28899109019700 video_cursor=190 audio_cursor=not_owned next_video_cursor=190 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-21 16:45:29.832] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=emit count=612 maximum_ns=3751300 begin_steady_ns=28906498810000 end_steady_ns=28906502561300 video_cursor=412 audio_cursor=not_owned next_video_cursor=412 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-21 16:45:29.832] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=between_calls_previous_return count=4377 maximum_ns=579042800 begin_steady_ns=28913096080900 end_steady_ns=28913675123700 video_cursor=611 audio_cursor=not_owned next_video_cursor=611 next_audio_cursor=not_owned error=None return_state=waiting deadline_ns=25721217966 deadline_clock=master wake_policy=0 pending_at_entry=0 entered_onprocess=1
[2026-10-21 16:45:29.832] [info] [graph][runtime.node] continuous_aggregate.summary reason=abort output_group=composition-r295.output output_generation=1 video_commits=612 source[0].group=composition-r295.source.0 source[0].real_frames=612 source[0].black_frames=0 source[0].contribution_generation=1 source[1].group=composition-r295.source.1 source[1].real_frames=612 source[1].black_frames=0 source[1].contribution_generation=1 source[2].group=composition-r295.source.2 source[2].real_frames=612 source[2].black_frames=0 source[2].contribution_generation=1
[2026-10-21 16:45:29.833] [info] [graph][runtime.node] cuda_canvas_call stage=context_push count=616 operation=copy maximum_ns=439300 begin_steady_ns=28906324153100 end_steady_ns=28906324592400 native_result=0 thread_cpu_ns=0
[2026-10-21 16:45:29.833] [info] [graph][runtime.node] cuda_canvas_call stage=source_wait count=2454 operation=copy maximum_ns=233100 begin_steady_ns=28912024685900 end_steady_ns=28912024919000 native_result=0 thread_cpu_ns=0
[2026-10-21 16:45:29.833] [info] [graph][runtime.node] cuda_canvas_call stage=plane_copy count=4910 operation=copy maximum_ns=1336800 begin_steady_ns=28892757880200 end_steady_ns=28892759217000 native_result=0 thread_cpu_ns=0
[2026-10-21 16:45:29.833] [info] [graph][runtime.node] cuda_canvas_call stage=destination_wait count=2 operation=upload maximum_ns=94200 begin_steady_ns=28892562611600 end_steady_ns=28892562705800 native_result=0 thread_cpu_ns=0
[2026-10-21 16:45:29.833] [info] [graph][runtime.node] cuda_canvas_call stage=context_pop count=616 operation=copy maximum_ns=54200 begin_steady_ns=28900724601500 end_steady_ns=28900724655700 native_result=0 thread_cpu_ns=0
[2026-10-21 16:45:29.833] [info] [graph][runtime.node] cuda_canvas_call stage=batch_completion count=614 operation=copy maximum_ns=17979600 begin_steady_ns=28899091020200 end_steady_ns=28899108999800 native_result=0 thread_cpu_ns=0
[2026-10-21 16:45:29.834] [info] [graph][runtime.node] canvas_compose_correlated scope=attempt begin_steady_ns=28899090776700 end_steady_ns=28899109019100 wall_ns=18242400 thread_cpu_ns=0 surface_count=1 surface_total_ns=1200 surface_maximum_ns=1200 surface_maximum_tile=none surface_maximum_result=unavailable validate_count=3 validate_total_ns=1900 validate_maximum_ns=900 validate_maximum_tile=0 validate_maximum_result=0 black_count=1 black_total_ns=48800 black_maximum_ns=48800 black_maximum_tile=none black_maximum_result=0 tile_count=3 tile_total_ns=185800 tile_maximum_ns=79900 tile_maximum_tile=1 tile_maximum_result=0 publish_count=1 publish_total_ns=9100 publish_maximum_ns=9100 publish_maximum_tile=none publish_maximum_result=0 context_push_count=1 context_push_total_ns=1200 context_push_maximum_ns=1200 context_push_maximum_tile=none context_push_maximum_result=0 source_wait_count=4 source_wait_total_ns=17800 source_wait_maximum_ns=15000 source_wait_maximum_tile=0 source_wait_maximum_result=0 plane_copy_count=8 plane_copy_total_ns=162900 plane_copy_maximum_ns=69000 plane_copy_maximum_tile=1 plane_copy_maximum_result=0 destination_wait_count=0 destination_wait_total_ns=0 destination_wait_maximum_ns=0 destination_wait_maximum_tile=none destination_wait_maximum_result=unavailable context_pop_count=1 context_pop_total_ns=6700 context_pop_maximum_ns=6700 context_pop_maximum_tile=none context_pop_maximum_result=0 batch_completion_count=1 batch_completion_total_ns=17977900 batch_completion_maximum_ns=17977900 batch_completion_maximum_tile=none batch_completion_maximum_result=0 source_query_completed_count=0 source_query_ready_count=3 source_query_ready_maximum_ns=38100 source_query_ready_begin_steady_ns=28899090833800 source_query_ready_end_steady_ns=28899090871900 source_query_ready_tile=0 source_query_ready_operation=event_query source_query_ready_native=0 source_query_ready_restore_native=0 source_query_pending_count=0 source_query_unavailable_count=0 source_query_error_count=0
[2026-10-21 16:45:29.834] [info] [graph][runtime.lifecycle] node_abort_payload node=6 before_objects=764 after_objects=729 before_bytes=14041238 after_bytes=14041238 before_reservations=25683 after_reservations=25683 before_releases=24919 after_releases=24954
[2026-10-21 16:45:29.834] [info] [graph][runtime.lifecycle] node_abort_payload node=67 before_objects=729 after_objects=728 before_bytes=14041238 after_bytes=14041238 before_reservations=25683 after_reservations=25683 before_releases=24954 after_releases=24955
[2026-10-21 16:45:29.834] [info] [graph][runtime.lifecycle] node_abort_payload node=44 before_objects=728 after_objects=727 before_bytes=14041238 after_bytes=14041238 before_reservations=25683 after_reservations=25683 before_releases=24955 after_releases=24956
[2026-10-21 16:45:29.834] [info] [graph][runtime.lifecycle] node_abort_payload node=25 before_objects=727 after_objects=725 before_bytes=14041238 after_bytes=14041238 before_reservations=25683 after_reservations=25683 before_releases=24956 after_releases=24958
[2026-10-21 16:45:29.835] [info] [graph][runtime.lifecycle] codec_owner_release node=73 kind=AudioResample owner_references=1
[2026-10-21 16:45:29.835] [info] [graph][runtime.lifecycle] node_abort_payload node=73 before_objects=725 after_objects=723 before_bytes=14041238 after_bytes=14024854 before_reservations=25683 after_reservations=25683 before_releases=24958 after_releases=24960
[2026-10-21 16:45:29.840] [info] [graph][runtime.lifecycle] node_abort_payload node=68 before_objects=723 after_objects=722 before_bytes=14024854 after_bytes=14024854 before_reservations=25683 after_reservations=25683 before_releases=24960 after_releases=24961
[2026-10-21 16:45:29.843] [info] [graph][runtime.lifecycle] node_abort_payload node=45 before_objects=722 after_objects=721 before_bytes=14024854 after_bytes=14024854 before_reservations=25683 after_reservations=25683 before_releases=24961 after_releases=24962
[2026-10-21 16:45:29.846] [info] [graph][runtime.lifecycle] node_abort_payload node=26 before_objects=721 after_objects=719 before_bytes=14024854 after_bytes=14024854 before_reservations=25683 after_reservations=25683 before_releases=24962 after_releases=24964
[2026-10-21 16:45:29.846] [info] [graph][runtime.lifecycle] node_abort_payload node=72 before_objects=719 after_objects=719 before_bytes=14024854 after_bytes=14024854 before_reservations=25683 after_reservations=25683 before_releases=24964 after_releases=24964
[2026-10-21 16:45:29.847] [info] [graph][runtime.lifecycle] node_abort_payload node=66 before_objects=719 after_objects=718 before_bytes=14024854 after_bytes=14024854 before_reservations=25683 after_reservations=25683 before_releases=24964 after_releases=24965
[2026-10-21 16:45:29.847] [info] [graph][runtime.lifecycle] node_abort_payload node=43 before_objects=718 after_objects=717 before_bytes=14024854 after_bytes=14024854 before_reservations=25683 after_reservations=25683 before_releases=24965 after_releases=24966
[2026-10-21 16:45:29.847] [info] [graph][runtime.lifecycle] node_abort_payload node=24 before_objects=717 after_objects=716 before_bytes=14024854 after_bytes=14024854 before_reservations=25683 after_reservations=25683 before_releases=24966 after_releases=24967
[2026-10-21 16:45:29.847] [info] [graph][runtime.lifecycle] node_abort_payload node=71 before_objects=716 after_objects=715 before_bytes=14024854 after_bytes=14016662 before_reservations=25683 after_reservations=25683 before_releases=24967 after_releases=24968
[2026-10-21 16:45:29.847] [info] [graph][runtime.lifecycle] node_abort_payload node=65 before_objects=715 after_objects=714 before_bytes=14016662 after_bytes=14016662 before_reservations=25683 after_reservations=25683 before_releases=24968 after_releases=24969
[2026-10-21 16:45:29.847] [info] [graph][runtime.lifecycle] node_abort_payload node=42 before_objects=714 after_objects=713 before_bytes=14016662 after_bytes=14016662 before_reservations=25683 after_reservations=25683 before_releases=24969 after_releases=24970
[2026-10-21 16:45:29.847] [info] [graph][runtime.lifecycle] node_abort_payload node=23 before_objects=713 after_objects=712 before_bytes=14016662 after_bytes=14016662 before_reservations=25683 after_reservations=25683 before_releases=24970 after_releases=24971
[2026-10-21 16:45:29.848] [info] [graph][runtime.lifecycle] codec_owner_release node=70 kind=AudioDecode owner_references=2
[2026-10-21 16:45:29.848] [info] [graph][runtime.lifecycle] node_abort_payload node=70 before_objects=712 after_objects=712 before_bytes=14016662 after_bytes=14016662 before_reservations=25683 after_reservations=25683 before_releases=24971 after_releases=24971
[2026-10-21 16:45:29.848] [info] [graph][runtime.lifecycle] codec_owner_release node=64 kind=VideoDecode owner_references=2
[2026-10-21 16:45:29.857] [info] [graph][runtime.lifecycle] node_abort_payload node=64 before_objects=712 after_objects=711 before_bytes=14016662 after_bytes=14016662 before_reservations=25683 after_reservations=25683 before_releases=24971 after_releases=24972
[2026-10-21 16:45:29.857] [info] [graph][runtime.lifecycle] codec_owner_release node=41 kind=VideoDecode owner_references=2
[2026-10-21 16:45:29.864] [info] [graph][runtime.lifecycle] node_abort_payload node=41 before_objects=711 after_objects=710 before_bytes=14016662 after_bytes=14016662 before_reservations=25683 after_reservations=25683 before_releases=24972 after_releases=24973
[2026-10-21 16:45:29.865] [info] [graph][runtime.lifecycle] codec_owner_release node=22 kind=VideoDecode owner_references=2
[2026-10-21 16:45:29.873] [info] [graph][runtime.lifecycle] node_abort_payload node=22 before_objects=710 after_objects=709 before_bytes=14016662 after_bytes=14016662 before_reservations=25683 after_reservations=25683 before_releases=24973 after_releases=24974
[2026-10-21 16:45:29.874] [info] [graph][runtime.lifecycle] node_abort_payload node=62 before_objects=709 after_objects=709 before_bytes=14016662 after_bytes=14016662 before_reservations=25683 after_reservations=25683 before_releases=24974 after_releases=24974
[2026-10-21 16:45:29.874] [info] [graph][runtime.lifecycle] node_abort_payload node=39 before_objects=709 after_objects=709 before_bytes=14016662 after_bytes=14016662 before_reservations=25683 after_reservations=25683 before_releases=24974 after_releases=24974
[2026-10-21 16:45:29.874] [info] [graph][runtime.lifecycle] node_abort_payload node=20 before_objects=709 after_objects=709 before_bytes=14016662 after_bytes=14016662 before_reservations=25683 after_reservations=25683 before_releases=24974 after_releases=24974
[2026-10-21 16:45:29.874] [info] [graph][runtime.lifecycle] node_abort_payload node=61 before_objects=709 after_objects=709 before_bytes=14016662 after_bytes=14016662 before_reservations=25683 after_reservations=25683 before_releases=24974 after_releases=24974
[2026-10-21 16:45:29.875] [info] [graph][runtime.lifecycle] node_abort_payload node=38 before_objects=709 after_objects=709 before_bytes=14016662 after_bytes=14016662 before_reservations=25683 after_reservations=25683 before_releases=24974 after_releases=24974
[2026-10-21 16:45:29.875] [info] [graph][runtime.lifecycle] node_abort_payload node=19 before_objects=709 after_objects=709 before_bytes=14016662 after_bytes=14016662 before_reservations=25683 after_reservations=25683 before_releases=24974 after_releases=24974
[2026-10-21 16:45:29.875] [info] [graph][runtime.lifecycle] node_abort_payload node=60 before_objects=709 after_objects=709 before_bytes=14016662 after_bytes=14016662 before_reservations=25683 after_reservations=25683 before_releases=24974 after_releases=24974
[2026-10-21 16:45:29.875] [info] [graph][runtime.lifecycle] node_abort_payload node=37 before_objects=709 after_objects=709 before_bytes=14016662 after_bytes=14016662 before_reservations=25683 after_reservations=25683 before_releases=24974 after_releases=24974
[2026-10-21 16:45:29.875] [info] [graph][runtime.lifecycle] node_abort_payload node=18 before_objects=709 after_objects=709 before_bytes=14016662 after_bytes=14016662 before_reservations=25683 after_reservations=25683 before_releases=24974 after_releases=24974
[2026-10-21 16:45:29.875] [info] [graph][runtime.lifecycle] node_abort_payload node=58 before_objects=709 after_objects=709 before_bytes=14016662 after_bytes=14016662 before_reservations=25683 after_reservations=25683 before_releases=24974 after_releases=24974
[2026-10-21 16:45:29.875] [info] [graph][runtime.lifecycle] node_abort_payload node=35 before_objects=709 after_objects=709 before_bytes=14016662 after_bytes=14016662 before_reservations=25683 after_reservations=25683 before_releases=24974 after_releases=24974
[2026-10-21 16:45:29.875] [info] [graph][runtime.lifecycle] node_abort_payload node=16 before_objects=709 after_objects=709 before_bytes=14016662 after_bytes=14016662 before_reservations=25683 after_reservations=25683 before_releases=24974 after_releases=24974
[2026-10-21 16:45:29.875] [info] [graph][runtime.lifecycle] node_abort_payload node=57 before_objects=709 after_objects=709 before_bytes=14016662 after_bytes=14016662 before_reservations=25683 after_reservations=25683 before_releases=24974 after_releases=24974
[2026-10-21 16:45:29.875] [info] [graph][runtime.lifecycle] node_abort_payload node=56 before_objects=709 after_objects=709 before_bytes=14016662 after_bytes=14016662 before_reservations=25683 after_reservations=25683 before_releases=24974 after_releases=24974
[2026-10-21 16:45:29.875] [info] [graph][runtime.lifecycle] node_abort_payload node=34 before_objects=709 after_objects=709 before_bytes=14016662 after_bytes=14016662 before_reservations=25683 after_reservations=25683 before_releases=24974 after_releases=24974
[2026-10-21 16:45:29.875] [info] [graph][runtime.lifecycle] node_abort_payload node=15 before_objects=709 after_objects=709 before_bytes=14016662 after_bytes=14016662 before_reservations=25683 after_reservations=25683 before_releases=24974 after_releases=24974
[2026-10-21 16:45:29.875] [info] [graph][runtime.lifecycle] node_abort_payload node=59 before_objects=709 after_objects=709 before_bytes=14016662 after_bytes=14016662 before_reservations=25683 after_reservations=25683 before_releases=24974 after_releases=24974
[2026-10-21 16:45:29.876] [info] [graph][runtime.lifecycle] node_abort_payload node=55 before_objects=709 after_objects=709 before_bytes=14016662 after_bytes=14016662 before_reservations=25683 after_reservations=25683 before_releases=24974 after_releases=24974
[2026-10-21 16:45:29.876] [info] [graph][runtime.lifecycle] node_abort_payload node=54 before_objects=709 after_objects=709 before_bytes=14016662 after_bytes=14016662 before_reservations=25683 after_reservations=25683 before_releases=24974 after_releases=24974
[2026-10-21 16:45:29.876] [info] [graph][runtime.lifecycle] node_abort_payload node=36 before_objects=709 after_objects=709 before_bytes=14016662 after_bytes=14016662 before_reservations=25683 after_reservations=25683 before_releases=24974 after_releases=24974
[2026-10-21 16:45:29.876] [info] [graph][runtime.lifecycle] node_abort_payload node=33 before_objects=709 after_objects=709 before_bytes=14016662 after_bytes=14016662 before_reservations=25683 after_reservations=25683 before_releases=24974 after_releases=24974
[2026-10-21 16:45:29.876] [info] [graph][runtime.lifecycle] node_abort_payload node=17 before_objects=709 after_objects=709 before_bytes=14016662 after_bytes=14016662 before_reservations=25683 after_reservations=25683 before_releases=24974 after_releases=24974
[2026-10-21 16:45:29.876] [info] [graph][runtime.lifecycle] node_abort_payload node=14 before_objects=709 after_objects=709 before_bytes=14016662 after_bytes=14016662 before_reservations=25683 after_reservations=25683 before_releases=24974 after_releases=24974
[2026-10-21 16:45:29.876] [info] [graph][runtime.lifecycle] node_abort_payload node=53 before_objects=709 after_objects=709 before_bytes=14016662 after_bytes=14016662 before_reservations=25683 after_reservations=25683 before_releases=24974 after_releases=24974
[2026-10-21 16:45:29.876] [info] [graph][runtime.lifecycle] node_abort_payload node=32 before_objects=709 after_objects=709 before_bytes=14016662 after_bytes=14016662 before_reservations=25683 after_reservations=25683 before_releases=24974 after_releases=24974
[2026-10-21 16:45:29.876] [info] [graph][runtime.lifecycle] node_abort_payload node=13 before_objects=709 after_objects=709 before_bytes=14016662 after_bytes=14016662 before_reservations=25683 after_reservations=25683 before_releases=24974 after_releases=24974
[2026-10-21 16:45:29.876] [info] [graph][runtime.lifecycle] node_abort_payload node=52 before_objects=709 after_objects=709 before_bytes=14016662 after_bytes=14016662 before_reservations=25683 after_reservations=25683 before_releases=24974 after_releases=24974
[2026-10-21 16:45:29.876] [info] [graph][runtime.lifecycle] node_abort_payload node=51 before_objects=709 after_objects=709 before_bytes=14016662 after_bytes=14016662 before_reservations=25683 after_reservations=25683 before_releases=24974 after_releases=24974
[2026-10-21 16:45:29.877] [info] [graph][runtime.lifecycle] node_abort_payload node=50 before_objects=709 after_objects=708 before_bytes=14016662 after_bytes=13948447 before_reservations=25683 after_reservations=25683 before_releases=24974 after_releases=24975
[2026-10-21 16:45:29.877] [info] [graph][runtime.lifecycle] node_abort_payload node=31 before_objects=708 after_objects=708 before_bytes=13948447 after_bytes=13948447 before_reservations=25683 after_reservations=25683 before_releases=24975 after_releases=24975
[2026-10-21 16:45:29.877] [info] [graph][runtime.lifecycle] node_abort_payload node=30 before_objects=708 after_objects=707 before_bytes=13948447 after_bytes=13887691 before_reservations=25683 after_reservations=25683 before_releases=24975 after_releases=24976
[2026-10-21 16:45:29.877] [info] [graph][runtime.lifecycle] node_abort_payload node=12 before_objects=707 after_objects=707 before_bytes=13887691 after_bytes=13887691 before_reservations=25683 after_reservations=25683 before_releases=24976 after_releases=24976
[2026-10-21 16:45:29.878] [info] [graph][runtime.lifecycle] node_abort_payload node=11 before_objects=707 after_objects=706 before_bytes=13887691 after_bytes=13819476 before_reservations=25683 after_reservations=25683 before_releases=24976 after_releases=24977
[2026-10-21 16:45:29.878] [info] [graph][runtime.lifecycle] node_abort_payload node=49 before_objects=706 after_objects=706 before_bytes=13819476 after_bytes=13819476 before_reservations=25683 after_reservations=25683 before_releases=24977 after_releases=24977
[2026-10-21 16:45:29.878] [info] [graph][runtime.lifecycle] node_abort_payload node=29 before_objects=706 after_objects=706 before_bytes=13819476 after_bytes=13819476 before_reservations=25683 after_reservations=25683 before_releases=24977 after_releases=24977
[2026-10-21 16:45:29.878] [info] [graph][runtime.lifecycle] node_abort_payload node=10 before_objects=706 after_objects=706 before_bytes=13819476 after_bytes=13819476 before_reservations=25683 after_reservations=25683 before_releases=24977 after_releases=24977
[2026-10-21 16:45:29.878] [info] [graph][runtime.lifecycle] node_abort_payload node=69 before_objects=706 after_objects=706 before_bytes=13819476 after_bytes=13819476 before_reservations=25683 after_reservations=25683 before_releases=24977 after_releases=24977
[2026-10-21 16:45:29.878] [info] [graph][runtime.lifecycle] node_abort_payload node=48 before_objects=706 after_objects=706 before_bytes=13819476 after_bytes=13819476 before_reservations=25683 after_reservations=25683 before_releases=24977 after_releases=24977
[2026-10-21 16:45:29.878] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-10-21 16:45:29.878] [info] [graph][runtime.lifecycle] node_abort_payload node=63 before_objects=706 after_objects=706 before_bytes=13819476 after_bytes=13819476 before_reservations=25683 after_reservations=25683 before_releases=24977 after_releases=24977
[2026-10-21 16:45:29.878] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-10-21 16:45:29.878] [info] [graph][runtime.lifecycle] node_abort_payload node=40 before_objects=706 after_objects=706 before_bytes=13819476 after_bytes=13819476 before_reservations=25683 after_reservations=25683 before_releases=24977 after_releases=24977
[2026-10-21 16:45:29.879] [info] [graph][runtime.lifecycle] node_abort_payload node=28 before_objects=706 after_objects=706 before_bytes=13819476 after_bytes=13819476 before_reservations=25683 after_reservations=25683 before_releases=24977 after_releases=24977
[2026-10-21 16:45:29.879] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-10-21 16:45:29.879] [info] [graph][runtime.lifecycle] node_abort_payload node=21 before_objects=706 after_objects=706 before_bytes=13819476 after_bytes=13819476 before_reservations=25683 after_reservations=25683 before_releases=24977 after_releases=24977
[2026-10-21 16:45:29.879] [info] [graph][runtime.lifecycle] node_abort_payload node=9 before_objects=706 after_objects=706 before_bytes=13819476 after_bytes=13819476 before_reservations=25683 after_reservations=25683 before_releases=24977 after_releases=24977
[2026-10-21 16:45:29.879] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=1083 completion_waits=1083 completed_datagrams=1125 drained_datagrams=43 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-21 16:45:29.879] [info] [graph][runtime.lifecycle] node_abort_payload node=47 before_objects=706 after_objects=706 before_bytes=13819476 after_bytes=13819476 before_reservations=25683 after_reservations=25683 before_releases=24977 after_releases=24977
[2026-10-21 16:45:29.879] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=14273 completion_waits=14273 completed_datagrams=22768 drained_datagrams=8494 would_block=0 cancellations=0 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-21 16:45:29.880] [info] [graph][runtime.lifecycle] node_abort_payload node=46 before_objects=706 after_objects=706 before_bytes=13819476 after_bytes=13819476 before_reservations=25683 after_reservations=25683 before_releases=24977 after_releases=24977
[2026-10-21 16:45:29.880] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=15799 completion_waits=15799 completed_datagrams=23687 drained_datagrams=7889 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-21 16:45:29.880] [info] [graph][runtime.lifecycle] node_abort_payload node=27 before_objects=706 after_objects=706 before_bytes=13819476 after_bytes=13819476 before_reservations=25683 after_reservations=25683 before_releases=24977 after_releases=24977
[2026-10-21 16:45:29.880] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=14974 completion_waits=14974 completed_datagrams=23354 drained_datagrams=8381 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-21 16:45:29.881] [info] [graph][runtime.lifecycle] node_abort_payload node=8 before_objects=706 after_objects=706 before_bytes=13819476 after_bytes=13819476 before_reservations=25683 after_reservations=25683 before_releases=24977 after_releases=24977
[2026-10-21 16:45:29.881] [info] [graph][runtime.lifecycle] node_abort_payload node=3 before_objects=706 after_objects=706 before_bytes=13819476 after_bytes=13819476 before_reservations=25683 after_reservations=25683 before_releases=24977 after_releases=24977
[2026-10-21 16:45:29.881] [info] [graph][runtime.node] codec_resolver.release slot=encoder_parameters bufferReferences=1 hasCredit=0 creditReferences=0
[2026-10-21 16:45:29.881] [info] [graph][runtime.lifecycle] node_abort_payload node=1 before_objects=706 after_objects=706 before_bytes=13819476 after_bytes=13819476 before_reservations=25683 after_reservations=25683 before_releases=24977 after_releases=24977
[2026-10-21 16:45:29.966] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=failed detail="scheduled datagram submit exceeded its original deadline generation=1 sequence=14644 endpoint=1 release_ns=25715458330 reserved_at_ns=25701857100 not_before_ns=25715458330 deadline_ns=25815303450 observed_at_ns=26272370000" description="" error=6 native=0 stage=3 reason="scheduled datagram submit exceeded its original deadline generation=1 sequence=14644 endpoint=1 release_ns=25715458330 reserved_at_ns=25701857100 not_before_ns=25715458330 deadline_ns=25815303450 observed_at_ns=26272370000"
[2026-10-21 16:45:29.966] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=retired detail="scheduled datagram submit exceeded its original deadline generation=1 sequence=14644 endpoint=1 release_ns=25715458330 reserved_at_ns=25701857100 not_before_ns=25715458330 deadline_ns=25815303450 observed_at_ns=26272370000" description="" error=6 native=0 stage=3 reason="scheduled datagram submit exceeded its original deadline generation=1 sequence=14644 endpoint=1 release_ns=25715458330 reserved_at_ns=25701857100 not_before_ns=25715458330 deadline_ns=25815303450 observed_at_ns=26272370000"
[CLI] final runtime report: queued=0, peakQueued=945, threads=81, processThreads=53, workers=0, workerProgress=60831, workerProcessCalls=238224, workerWaits=177379, workerWakeups=175755, workerErrors=3, errors=3, stalledIntervals=0, cpuSamples=63, logicalProcessors=22, averageSystemMachineCpuPercent=9.666338, averageProcessMachineCpuPercent=2.132532, peakProcessMachineCpuPercent=5.420561, averageProcessSingleCoreCpuPercent=46.915707, peakProcessSingleCoreCpuPercent=119.252336, initialWorkingSetBytes=208470016, workingSetBytes=264839168, peakWorkingSetBytes=264839168, totalPushed=56264, totalPopped=55568, droppedBuffers=0, encodedPacketsPushed=6813, encodedPacketsPopped=6812, backpressureItems=122, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=18516268, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=778, graphPayloadReservations=25683, graphPayloadReleases=25683, graphPayloadPressureFailures=1, lastInputReceivedAtNs=28913675861900
ModLoad: 00007ff9`027a0000 00007ff9`02c27000   C:\WINDOWS\System32\SETUPAPI.dll
ModLoad: 00007ff9`01e00000 00007ff9`01ed9000   C:\WINDOWS\System32\OLEAUT32.dll
ModLoad: 00007ff8`ccf20000 00007ff8`ccfcf000   C:\WINDOWS\System32\DriverStore\FileRepository\nvami.inf_amd64_97bcbdbdb2ab507c\nvdxgdmal64.dll
ModLoad: 00007ff8`cbed0000 00007ff8`cccd2000   C:\WINDOWS\System32\DriverStore\FileRepository\nvami.inf_amd64_97bcbdbdb2ab507c\nvobjectloader64.dll
ModLoad: 00007fff`84280000 00007fff`86133000   C:\WINDOWS\SYSTEM32\nvcuvid.dll
ModLoad: 00007ff9`00870000 00007ff9`008f2000   C:\WINDOWS\System32\WINTRUST.dll
ModLoad: 00007ff9`02150000 00007ff9`0215a000   C:\WINDOWS\System32\NSI.dll
ModLoad: 00007fff`b7570000 00007fff`b7675000   C:\WINDOWS\SYSTEM32\nvEncodeAPI64.dll
(9f5c.ba24): Break instruction exception - code 80000003 (first chance)
ntdll!DbgBreakPoint:
00007ff9`03083f40 cc              int     3
0:132> sxe -c ".exr -1;.ecxr;~* kb;.dump /ma D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r295-crash.dmp;qd" -c2 ".exr -1;.ecxr;~* kb;.dump /ma D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r295-crash.dmp;qd" 0xc0000409;g
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
[in @ 0000014a56a49a80] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000014a56a49a80] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 33393.537244
[in @ 0000014a5ff78c40] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000014a5ff78c40] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 43866.729789
[in @ 0000014a64eb5080] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000014a64eb5080] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 25434.685067
[in @ 0000014a659fea80] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000014a659fea80] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 25434.685067
[in @ 0000014a659fbbc0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000014a659fecc0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000014a659fbbc0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 43866.729789
[in @ 0000014a659fecc0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 33393.537244
[aac @ 0000014a56cc81c0] Qavg: 621.927
[aac @ 0000014a56cc81c0] 2 frames left in the queue on closing
[CLI] realtime video runtime failed: IoFailure: scheduled datagram submit exceeded its original deadline generation=1 sequence=14644 endpoint=1 release_ns=25715458330 reserved_at_ns=25701857100 not_before_ns=25715458330 deadline_ns=25815303450 observed_at_ns=26272370000
~~~~

## 清理清单

composition-r295-cdb.log
composition-r295-cli-error.log
composition-r295-cli.log
composition-r295-monitor.log
composition-r295-source0-stdout.log
composition-r295-source0.log
composition-r295-source1-stdout.log
composition-r295-source1.log
composition-r295-source2-stdout.log
composition-r295-source2.log
composition-r295-vlc.log
