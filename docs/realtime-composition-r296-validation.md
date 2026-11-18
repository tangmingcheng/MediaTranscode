# r296：C++异常捕获配置与附加扰动复核

原规格三源H.264→HEVC/AAC MPEG-TS/UDP，1280×72030、8000kbps CBR/GOP60、AAC192kbps CBR。仅诊断，FAIL；CLI41316，源37572/45432/44328，VLC35472。

cdb配置eh首次异常记录并gn保持未处理语义、fast-fail第二次异常记录及dump。此轮附加暂停再次导致截止失败，但未产生C++/fast-fail事件和dump；CLI错误退出1，payload最终0，25472次预约/释放匹配。源/VLC中止清理，非自然退出。无重入/快照。

连续两次附加仅复现扰动导致的正常错误清理，没有新增崩溃栈。下一次应在CLI启动前配置调试器，避免中途附加的全线程暂停破坏实时截止；保持原媒体规格和完整重入场景。不以本诊断当生产验收。

~~~~powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r296-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r296-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r296-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r296-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r296-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r296-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r296 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62020 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r296-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r296-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r296-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r296-native- --snapshot-format=png udp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r296-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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


& 'C:/Program Files (x86)/Windows Kits/10/Debuggers/x64/cdb.exe' -p 41316 -G -logo 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r296-cdb.log' -y 'D:/Code/MyCode/MediaTranscode/out/build/x64-release' -c 'sxe -c ".exr -1;kb;gn" eh;sxe -c2 ".exr -1;.ecxr;~* kb;.dump /ma D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r296-crash.dmp;qd" 0xc0000409;g'
~~~~

~~~~text
[CLI] composition_inputs=3 grid=fixed audio_source=2 audio=on max_duration=source_driven hw=planner-highest-score
[2026-10-21 16:46:52.373] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-10-21 16:46:52.377] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-10-21 16:46:52.973] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-21 16:46:52.975] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-21 16:46:53.027] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-21 16:46:53.027] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-21 16:46:53.068] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-21 16:46:53.068] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-21 16:46:57.369] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-10-21 16:46:57.484] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-10-21 16:46:57.539] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-10-21 16:46:57.539] [info] [graph][planner.capability] canvas_cuda_stream stage=prepared device_stream=0000000000000000 private_nonblocking=1
[2026-10-21 16:46:57.548] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-21 16:46:57.549] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-21 16:46:57.549] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-10-21 16:46:57.556] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-21 16:46:57.557] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-21 16:46:57.557] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-10-21 16:46:57.564] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-21 16:46:57.565] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-21 16:46:57.565] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-10-21 16:46:57.598] [info] [graph][planner.capability] composition_preparation_storage stage=graph_bound scope=shared_reservations capacity=2147483648 retained=848927740 high_water=866374002
[CLI] audio_plan branch=transcode_frame reason=transcode_required_by_output_frame_contract codec=aac sample_rate=44100 channels=2 access_unit_samples=1024 encoder=aac bitrate_kbps=192
[CLI] prepared_output_id=1
[CLI] selected_chain=fixed-grid-composition score=0 decoder=h264,h264,h264 filter=scale_cuda=640:360,scale_cuda=640:360,scale_cuda=640:360 encoder=hevc_nvenc
[2026-10-21 16:46:57.602] [info] [graph][runtime.lifecycle] compile.begin
[2026-10-21 16:46:57.603] [info] [graph][runtime.channel] create channel=1 edge=1 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-21 16:46:57.603] [info] [graph][runtime.channel] create channel=2 edge=2 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-21 16:46:57.603] [info] [graph][runtime.channel] create channel=3 edge=3 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-21 16:46:57.603] [info] [graph][runtime.channel] create channel=4 edge=4 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.603] [info] [graph][runtime.channel] create channel=5 edge=5 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.604] [info] [graph][runtime.channel] create channel=6 edge=6 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.604] [info] [graph][runtime.channel] create channel=7 edge=7 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.604] [info] [graph][runtime.channel] create channel=8 edge=8 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.604] [info] [graph][runtime.channel] create channel=9 edge=9 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:46:57.604] [info] [graph][runtime.channel] create channel=10 edge=10 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.604] [info] [graph][runtime.channel] create channel=11 edge=11 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.604] [info] [graph][runtime.channel] create channel=12 edge=12 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:46:57.604] [info] [graph][runtime.channel] create channel=13 edge=13 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:46:57.604] [info] [graph][runtime.channel] create channel=14 edge=14 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.605] [info] [graph][runtime.channel] create channel=15 edge=15 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.605] [info] [graph][runtime.channel] create channel=16 edge=16 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.605] [info] [graph][runtime.channel] create channel=17 edge=17 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.605] [info] [graph][runtime.channel] create channel=18 edge=18 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.605] [info] [graph][runtime.channel] create channel=19 edge=19 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.605] [info] [graph][runtime.channel] create channel=20 edge=20 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.605] [info] [graph][runtime.channel] create channel=21 edge=21 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-10-21 16:46:57.605] [info] [graph][runtime.channel] create channel=22 edge=22 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-21 16:46:57.605] [info] [graph][runtime.channel] create channel=23 edge=23 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:46:57.605] [info] [graph][runtime.channel] create channel=24 edge=24 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-21 16:46:57.606] [info] [graph][runtime.channel] create channel=25 edge=25 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:46:57.606] [info] [graph][runtime.channel] create channel=26 edge=26 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:46:57.606] [info] [graph][runtime.channel] create channel=27 edge=27 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:46:57.606] [info] [graph][runtime.channel] create channel=28 edge=28 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:46:57.606] [info] [graph][runtime.channel] create channel=29 edge=29 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:46:57.606] [info] [graph][runtime.channel] create channel=30 edge=30 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.606] [info] [graph][runtime.channel] create channel=31 edge=31 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.606] [info] [graph][runtime.channel] create channel=32 edge=32 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.607] [info] [graph][runtime.channel] create channel=33 edge=33 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.607] [info] [graph][runtime.channel] create channel=34 edge=34 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.607] [info] [graph][runtime.channel] create channel=35 edge=35 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.607] [info] [graph][runtime.channel] create channel=36 edge=36 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:46:57.607] [info] [graph][runtime.channel] create channel=37 edge=37 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.607] [info] [graph][runtime.channel] create channel=38 edge=38 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.607] [info] [graph][runtime.channel] create channel=39 edge=39 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:46:57.607] [info] [graph][runtime.channel] create channel=40 edge=40 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:46:57.608] [info] [graph][runtime.channel] create channel=41 edge=41 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.608] [info] [graph][runtime.channel] create channel=42 edge=42 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.608] [info] [graph][runtime.channel] create channel=43 edge=43 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.608] [info] [graph][runtime.channel] create channel=44 edge=44 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.608] [info] [graph][runtime.channel] create channel=45 edge=45 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.608] [info] [graph][runtime.channel] create channel=46 edge=46 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.608] [info] [graph][runtime.channel] create channel=47 edge=47 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.608] [info] [graph][runtime.channel] create channel=48 edge=48 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-10-21 16:46:57.608] [info] [graph][runtime.channel] create channel=49 edge=49 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-21 16:46:57.609] [info] [graph][runtime.channel] create channel=50 edge=50 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:46:57.609] [info] [graph][runtime.channel] create channel=51 edge=51 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-21 16:46:57.609] [info] [graph][runtime.channel] create channel=52 edge=52 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:46:57.609] [info] [graph][runtime.channel] create channel=53 edge=53 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:46:57.609] [info] [graph][runtime.channel] create channel=54 edge=54 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:46:57.609] [info] [graph][runtime.channel] create channel=55 edge=55 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:46:57.609] [info] [graph][runtime.channel] create channel=56 edge=56 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:46:57.609] [info] [graph][runtime.channel] create channel=57 edge=57 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.609] [info] [graph][runtime.channel] create channel=58 edge=58 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.610] [info] [graph][runtime.channel] create channel=59 edge=59 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.610] [info] [graph][runtime.channel] create channel=60 edge=60 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.610] [info] [graph][runtime.channel] create channel=61 edge=61 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.610] [info] [graph][runtime.channel] create channel=62 edge=62 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.610] [info] [graph][runtime.channel] create channel=63 edge=63 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.610] [info] [graph][runtime.channel] create channel=64 edge=64 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.610] [info] [graph][runtime.channel] create channel=65 edge=65 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.610] [info] [graph][runtime.channel] create channel=66 edge=66 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:46:57.610] [info] [graph][runtime.channel] create channel=67 edge=67 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:46:57.610] [info] [graph][runtime.channel] create channel=68 edge=68 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.611] [info] [graph][runtime.channel] create channel=69 edge=69 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.611] [info] [graph][runtime.channel] create channel=70 edge=70 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.611] [info] [graph][runtime.channel] create channel=71 edge=71 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:46:57.611] [info] [graph][runtime.channel] create channel=72 edge=72 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:46:57.611] [info] [graph][runtime.channel] create channel=73 edge=73 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:46:57.611] [info] [graph][runtime.channel] create channel=74 edge=74 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:46:57.611] [info] [graph][runtime.channel] create channel=75 edge=75 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.612] [info] [graph][runtime.channel] create channel=76 edge=76 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.612] [info] [graph][runtime.channel] create channel=77 edge=77 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.612] [info] [graph][runtime.channel] create channel=78 edge=78 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.612] [info] [graph][runtime.channel] create channel=79 edge=79 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.612] [info] [graph][runtime.channel] create channel=80 edge=80 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.612] [info] [graph][runtime.channel] create channel=81 edge=81 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.612] [info] [graph][runtime.channel] create channel=82 edge=82 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.612] [info] [graph][runtime.channel] create channel=83 edge=83 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-10-21 16:46:57.612] [info] [graph][runtime.channel] create channel=84 edge=84 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-21 16:46:57.613] [info] [graph][runtime.channel] create channel=85 edge=85 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:46:57.613] [info] [graph][runtime.channel] create channel=86 edge=86 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-21 16:46:57.613] [info] [graph][runtime.channel] create channel=87 edge=87 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:46:57.613] [info] [graph][runtime.channel] create channel=88 edge=88 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:46:57.613] [info] [graph][runtime.channel] create channel=89 edge=89 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:46:57.613] [info] [graph][runtime.channel] create channel=90 edge=90 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:46:57.613] [info] [graph][runtime.channel] create channel=91 edge=91 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-10-21 16:46:57.613] [info] [graph][runtime.channel] create channel=92 edge=92 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-21 16:46:57.613] [info] [graph][runtime.channel] create channel=93 edge=93 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-10-21 16:46:57.614] [info] [graph][runtime.channel] create channel=94 edge=94 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-21 16:46:57.614] [info] [graph][runtime.channel] create channel=95 edge=95 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:46:57.614] [info] [graph][runtime.channel] create channel=96 edge=96 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:46:57.614] [info] [graph][runtime.channel] create channel=97 edge=97 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:46:57.614] [info] [graph][runtime.channel] create channel=98 edge=98 stream=Audio edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.614] [info] [graph][runtime.channel] create channel=99 edge=99 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:46:57.614] [info] [graph][runtime.channel] create channel=100 edge=100 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.614] [info] [graph][runtime.channel] create channel=101 edge=101 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0
[2026-10-21 16:46:57.615] [info] [graph][runtime.channel] create channel=102 edge=102 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-21 16:46:57.615] [info] [graph][runtime.channel] create channel=103 edge=103 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-21 16:46:57.615] [info] [graph][runtime.channel] create channel=104 edge=104 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-21 16:46:57.615] [info] [graph][runtime.channel] create channel=105 edge=105 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0
[2026-10-21 16:46:57.615] [info] [graph][runtime.channel] create channel=106 edge=106 stream=Video edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-21 16:46:57.615] [info] [graph][runtime.channel] create channel=107 edge=107 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-21 16:46:57.615] [info] [graph][runtime.channel] create channel=108 edge=108 stream=Any edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-21 16:46:57.615] [info] [graph][runtime.channel] create channel=109 edge=109 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.615] [info] [graph][runtime.channel] create channel=110 edge=110 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.616] [info] [graph][runtime.channel] create channel=111 edge=111 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-21 16:46:57.616] [info] [graph][runtime.channel] create channel=112 edge=112 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-21 16:46:57.616] [info] [graph][runtime.channel] create channel=113 edge=113 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-21 16:46:57.616] [info] [graph][runtime.channel] create channel=114 edge=114 stream=Any edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-21 16:46:57.617] [info] [graph][runtime.channel] create channel=115 edge=115 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-21 16:46:57.617] [info] [graph][runtime.channel] create channel=116 edge=116 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-21 16:46:57.617] [info] [graph][runtime.channel] create channel=117 edge=117 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-21 16:46:57.617] [info] [graph][runtime.channel] create channel=118 edge=118 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-21 16:46:57.617] [info] [graph][runtime.channel] create channel=119 edge=119 stream=Any edge_kind=EncodedPacket payload=Unknown queued=0
[2026-10-21 16:46:57.617] [info] [graph][runtime.channel] create channel=120 edge=120 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-21 16:46:57.617] [info] [graph][runtime.channel] create channel=121 edge=121 stream=Metadata edge_kind=Unknown payload=Unknown queued=0
[2026-10-21 16:46:57.617] [info] [graph][runtime.channel] create channel=122 edge=122 stream=Metadata edge_kind=Unknown payload=Unknown queued=0
[2026-10-21 16:46:57.618] [info] [graph][runtime.lifecycle] compiled nodes=81 edges=122 channels=122 execution_order=1->3->8->27->46->47->9->21->28->40->63->48->69->10->29->49->11->12->30->31->50->51->52->13->32->53->14->17->33->36->54->55->59->15->34->56->57->16->35->58->18->37->60->19->38->61->20->39->62->22->41->64->70->23->42->65->71->24->43->66->72->26->45->68->73->25->44->67->6->2->7->80->77->4->5->74->75->78->76->79->81
[2026-10-21 16:46:57.618] [info] [graph][runtime.lifecycle] compile.done state=DefaultRegistrationPending
[2026-10-21 16:46:57.618] [info] [graph][runtime.node] register node=1 name=composition-r296.output.video.codec_resolver kind=CodecResolver
[2026-10-21 16:46:57.618] [info] [graph][runtime.node] register node=2 name=composition-r296.output.video.encode kind=VideoEncode
[2026-10-21 16:46:57.619] [info] [graph][runtime.node] register node=3 name=composition-r296.output.audio.codec_resolver kind=AudioCodecResolver
[2026-10-21 16:46:57.619] [info] [graph][runtime.node] register node=4 name=composition-r296.output.audio.encode kind=AudioEncode
[2026-10-21 16:46:57.619] [info] [graph][runtime.node] register node=5 name=composition-r296.output.audio.canonicalizer kind=EncodedAudioCanonicalizer
[2026-10-21 16:46:57.619] [info] [graph][runtime.node] register node=8 name=composition-r296.source_0.input kind=RawRtpInput
[2026-10-21 16:46:57.619] [info] [graph][runtime.node] register node=9 name=composition-r296.source_0.rtp.clock_group kind=RtpClockGroup
[2026-10-21 16:46:57.619] [info] [graph][runtime.node] register node=10 name=composition-r296.source_0.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-10-21 16:46:57.619] [info] [graph][runtime.node] register node=11 name=composition-r296.source_0.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-10-21 16:46:57.620] [info] [graph][runtime.node] register node=12 name=composition-r296.source_0.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-10-21 16:46:57.620] [info] [graph][runtime.node] register node=13 name=composition-r296.source_0.av_sync.source_clock kind=SourceClockStateFanout
[2026-10-21 16:46:57.620] [info] [graph][runtime.node] register node=14 name=composition-r296.source_0.av_sync.video.generation_gate kind=LockedPacketGate
[2026-10-21 16:46:57.620] [info] [graph][runtime.node] register node=15 name=composition-r296.source_0.av_sync.video.canonical_input kind=CanonicalInput
[2026-10-21 16:46:57.620] [info] [graph][runtime.node] register node=16 name=composition-r296.source_0.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-10-21 16:46:57.620] [info] [graph][runtime.node] register node=17 name=composition-r296.source_0.av_sync.startup.clock kind=AvStartupClock
[2026-10-21 16:46:57.620] [info] [graph][runtime.node] register node=18 name=composition-r296.source_0.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-10-21 16:46:57.620] [info] [graph][runtime.node] register node=20 name=composition-r296.source_0.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-10-21 16:46:57.621] [info] [graph][runtime.node] register node=21 name=composition-r296.source_0.video.codec_resolver kind=CodecResolver
[2026-10-21 16:46:57.621] [info] [graph][runtime.node] register node=22 name=composition-r296.source_0.video.decode kind=VideoDecode
[2026-10-21 16:46:57.621] [info] [graph][runtime.node] register node=23 name=composition-r296.source_0.video.hwtransfer kind=HardwareTransfer
[2026-10-21 16:46:57.621] [info] [graph][runtime.node] register node=24 name=composition-r296.source_0.video.framerate kind=VideoFrameRate
[2026-10-21 16:46:57.621] [info] [graph][runtime.node] register node=25 name=composition-r296.source_0.video.preparation kind=VideoPreparation
[2026-10-21 16:46:57.621] [info] [graph][runtime.node] register node=26 name=composition-r296.source_0.video.filter kind=VideoFilter
[2026-10-21 16:46:57.621] [info] [graph][runtime.node] register node=27 name=composition-r296.source_1.input kind=RawRtpInput
[2026-10-21 16:46:57.621] [info] [graph][runtime.node] register node=28 name=composition-r296.source_1.rtp.clock_group kind=RtpClockGroup
[2026-10-21 16:46:57.621] [info] [graph][runtime.node] register node=29 name=composition-r296.source_1.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-10-21 16:46:57.621] [info] [graph][runtime.node] register node=30 name=composition-r296.source_1.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-10-21 16:46:57.622] [info] [graph][runtime.node] register node=31 name=composition-r296.source_1.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-10-21 16:46:57.622] [info] [graph][runtime.node] register node=32 name=composition-r296.source_1.av_sync.source_clock kind=SourceClockStateFanout
[2026-10-21 16:46:57.622] [info] [graph][runtime.node] register node=33 name=composition-r296.source_1.av_sync.video.generation_gate kind=LockedPacketGate
[2026-10-21 16:46:57.622] [info] [graph][runtime.node] register node=34 name=composition-r296.source_1.av_sync.video.canonical_input kind=CanonicalInput
[2026-10-21 16:46:57.622] [info] [graph][runtime.node] register node=35 name=composition-r296.source_1.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-10-21 16:46:57.622] [info] [graph][runtime.node] register node=36 name=composition-r296.source_1.av_sync.startup.clock kind=AvStartupClock
[2026-10-21 16:46:57.622] [info] [graph][runtime.node] register node=37 name=composition-r296.source_1.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-10-21 16:46:57.622] [info] [graph][runtime.node] register node=39 name=composition-r296.source_1.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-10-21 16:46:57.622] [info] [graph][runtime.node] register node=40 name=composition-r296.source_1.video.codec_resolver kind=CodecResolver
[2026-10-21 16:46:57.622] [info] [graph][runtime.node] register node=41 name=composition-r296.source_1.video.decode kind=VideoDecode
[2026-10-21 16:46:57.623] [info] [graph][runtime.node] register node=42 name=composition-r296.source_1.video.hwtransfer kind=HardwareTransfer
[2026-10-21 16:46:57.623] [info] [graph][runtime.node] register node=43 name=composition-r296.source_1.video.framerate kind=VideoFrameRate
[2026-10-21 16:46:57.623] [info] [graph][runtime.node] register node=44 name=composition-r296.source_1.video.preparation kind=VideoPreparation
[2026-10-21 16:46:57.623] [info] [graph][runtime.node] register node=45 name=composition-r296.source_1.video.filter kind=VideoFilter
[2026-10-21 16:46:57.623] [info] [graph][runtime.node] register node=46 name=composition-r296.source_2.video.input kind=RawRtpInput
[2026-10-21 16:46:57.623] [info] [graph][runtime.node] register node=47 name=composition-r296.source_2.audio.input kind=RawRtpInput
[2026-10-21 16:46:57.623] [info] [graph][runtime.node] register node=48 name=composition-r296.source_2.rtp.clock_group kind=RtpClockGroup
[2026-10-21 16:46:57.623] [info] [graph][runtime.node] register node=49 name=composition-r296.source_2.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-10-21 16:46:57.623] [info] [graph][runtime.node] register node=50 name=composition-r296.source_2.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-10-21 16:46:57.624] [info] [graph][runtime.node] register node=51 name=composition-r296.source_2.av_sync.audio.protocol_binder kind=RtpPacketClockBinder
[2026-10-21 16:46:57.624] [info] [graph][runtime.node] register node=52 name=composition-r296.source_2.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-10-21 16:46:57.624] [info] [graph][runtime.node] register node=53 name=composition-r296.source_2.av_sync.source_clock kind=SourceClockStateFanout
[2026-10-21 16:46:57.624] [info] [graph][runtime.node] register node=54 name=composition-r296.source_2.av_sync.video.generation_gate kind=LockedPacketGate
[2026-10-21 16:46:57.624] [info] [graph][runtime.node] register node=55 name=composition-r296.source_2.av_sync.audio.generation_gate kind=LockedPacketGate
[2026-10-21 16:46:57.624] [info] [graph][runtime.node] register node=56 name=composition-r296.source_2.av_sync.video.canonical_input kind=CanonicalInput
[2026-10-21 16:46:57.624] [info] [graph][runtime.node] register node=57 name=composition-r296.source_2.av_sync.audio.canonical_input kind=CanonicalInput
[2026-10-21 16:46:57.625] [info] [graph][runtime.node] register node=58 name=composition-r296.source_2.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-10-21 16:46:57.625] [info] [graph][runtime.node] register node=59 name=composition-r296.source_2.av_sync.startup.clock kind=AvStartupClock
[2026-10-21 16:46:57.625] [info] [graph][runtime.node] register node=60 name=composition-r296.source_2.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-10-21 16:46:57.625] [info] [graph][runtime.node] register node=62 name=composition-r296.source_2.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-10-21 16:46:57.625] [info] [graph][runtime.node] register node=63 name=composition-r296.source_2.video.codec_resolver kind=CodecResolver
[2026-10-21 16:46:57.625] [info] [graph][runtime.node] register node=64 name=composition-r296.source_2.video.decode kind=VideoDecode
[2026-10-21 16:46:57.625] [info] [graph][runtime.node] register node=65 name=composition-r296.source_2.video.hwtransfer kind=HardwareTransfer
[2026-10-21 16:46:57.625] [info] [graph][runtime.node] register node=66 name=composition-r296.source_2.video.framerate kind=VideoFrameRate
[2026-10-21 16:46:57.625] [info] [graph][runtime.node] register node=67 name=composition-r296.source_2.video.preparation kind=VideoPreparation
[2026-10-21 16:46:57.625] [info] [graph][runtime.node] register node=68 name=composition-r296.source_2.video.filter kind=VideoFilter
[2026-10-21 16:46:57.626] [info] [graph][runtime.node] register node=69 name=composition-r296.source_2.audio.encode.codec_resolver kind=AudioCodecResolver
[2026-10-21 16:46:57.626] [info] [graph][runtime.node] register node=70 name=composition-r296.source_2.audio.encode.decode kind=AudioDecode
[2026-10-21 16:46:57.626] [info] [graph][runtime.node] register node=71 name=composition-r296.source_2.audio.encode.startup_trim kind=Unknown
[2026-10-21 16:46:57.626] [info] [graph][runtime.node] register node=72 name=composition-r296.source_2.audio.encode.drift_controller kind=AudioDriftController
[2026-10-21 16:46:57.626] [info] [graph][runtime.node] register node=73 name=composition-r296.source_2.audio.encode.resample kind=AudioResample
[2026-10-21 16:46:57.626] [info] [graph][runtime.node] register node=74 name=composition-r296.output.scheduler.scheduler kind=AvOutputScheduler
[2026-10-21 16:46:57.626] [info] [graph][runtime.node] register node=75 name=composition-r296.output.scheduler.router kind=ScheduledOutputRouter
[2026-10-21 16:46:57.626] [info] [graph][runtime.node] register node=76 name=composition-r296.output.mpegts.mux kind=FileMux
[2026-10-21 16:46:57.627] [info] [graph][runtime.node] register node=77 name=composition-r296.output.mpegts.plan.source kind=Unknown
[2026-10-21 16:46:57.627] [info] [graph][runtime.node] register node=78 name=composition-r296.output.mpegts.scheduled.adapter kind=Unknown
[2026-10-21 16:46:57.627] [info] [graph][runtime.node] register node=79 name=composition-r296.output.mpegts.datagram.materializer kind=MpegTsDatagramMaterializer
[2026-10-21 16:46:57.627] [info] [graph][runtime.node] register node=80 name=composition-r296.output.mpegts.transport.plan kind=DatagramTransportPlanSource
[2026-10-21 16:46:57.627] [info] [graph][runtime.node] register node=81 name=composition-r296.output.mpegts.transport.sender kind=ScheduledDatagramSender
[2026-10-21 16:46:57.627] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=waiting_for_random_access detail="" description=""
[2026-10-21 16:46:57.628] [info] [graph][runtime.lifecycle] start.begin mode=threaded
[2026-10-21 16:46:57.629] [info] [graph][runtime.lifecycle] worker.finished node=1
[2026-10-21 16:46:57.629] [info] [graph][runtime.node] worker.thread_cpu node=1 available=1 cpu_ns=0 process_calls=1
[2026-10-21 16:46:57.629] [info] [graph][runtime.lifecycle] worker.finished node=3
[2026-10-21 16:46:57.630] [info] [graph][runtime.node] worker.thread_cpu node=3 available=1 cpu_ns=0 process_calls=1
[2026-10-21 16:46:57.630] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:46:57.629] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:46:57.630] [info] [graph][runtime.lifecycle] worker.finished node=69
[2026-10-21 16:46:57.631] [info] [graph][runtime.node] worker.thread_cpu node=69 available=1 cpu_ns=15625000 process_calls=1
[2026-10-21 16:46:57.631] [info] [graph][runtime.lifecycle] worker.finished node=40
[2026-10-21 16:46:57.631] [info] [graph][runtime.node] worker.thread_cpu node=40 available=1 cpu_ns=0 process_calls=2
[2026-10-21 16:46:57.631] [info] [graph][runtime.lifecycle] worker.finished node=21
[2026-10-21 16:46:57.631] [info] [graph][runtime.node] worker.thread_cpu node=21 available=1 cpu_ns=0 process_calls=1
[2026-10-21 16:46:57.631] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-10-21 16:46:57.632] [info] [graph][runtime.lifecycle] worker.finished node=63
[2026-10-21 16:46:57.632] [info] [graph][runtime.node] worker.thread_cpu node=63 available=1 cpu_ns=0 process_calls=1
[2026-10-21 16:46:57.632] [info] [graph][runtime.lifecycle] start.done state=ThreadedRunning
[2026-10-21 16:46:57.632] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-10-21 16:46:57.633] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=1 generation=1
[2026-10-21 16:46:57.633] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=1 generation=1
[2026-10-21 16:46:57.634] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=2434456261 dts=2434456261 size=33389 generation=1 sequence=1
[2026-10-21 16:46:57.634] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=38827722 dts=38827722 size=33389 generation=1 sequence=1
[2026-10-21 16:46:57.635] [info] [graph][runtime.node] video_encode.bind_encoder codec=hevc_nvenc pix_fmt=cuda rate_control=cbr bitrate=8000000 min_bitrate=8000000 max_bitrate=8000000 buffer_size=267000 fps=30/1 gop=60 frame_kind=hardware hwaccel=cuda hw_device_ctx=set hw_frames_ctx=set
[2026-10-21 16:46:57.636] [info] [graph][runtime.node] audio_encode_trace stage=codec_bound
[2026-10-21 16:46:57.636] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:46:57.637] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-10-21 16:46:57.637] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=15 generation=1
[2026-10-21 16:46:57.638] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=2431864937 dts=2431864937 size=317016 generation=1 sequence=15
[2026-10-21 16:46:57.659] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-10-21 16:46:57.659] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=38827722 format=cuda size=1280x720 buffer_identity=0xb08600000 drm_prime=0 software_frame=0
[2026-10-21 16:46:57.659] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-10-21 16:46:57.660] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=2434456261 format=cuda size=1280x720 buffer_identity=0xb08800000 drm_prime=0 software_frame=0
[2026-10-21 16:46:57.660] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=0000024AA4D16BB0 stream=Video payload=Frame pts=38827722 dts=38827722 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:46:57.660] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=0000024AA4D16BB0 stream=Video payload=Frame pts=38827722 dts=38827722 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:46:57.660] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=0000024AA4D16BB0 stream=Video payload=Frame pts=38827722 dts=38827722 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:46:57.660] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=0000024AAE4FFB50 stream=Video payload=Frame pts=2434456261 dts=2434456261 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:46:57.660] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-10-21 16:46:57.660] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=0000024AAE4FFB50 stream=Video payload=Frame pts=2434456261 dts=2434456261 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:46:57.660] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=0000024AAE4FFB50 stream=Video payload=Frame pts=2434456261 dts=2434456261 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:46:57.660] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=0000024AAE32B340 stream=Video payload=Frame pts=38827722 dts=38827722 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:46:57.660] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=0000024AAE32B340 stream=Video payload=Frame pts=38827722 dts=38827722 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:46:57.661] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-10-21 16:46:57.661] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-10-21 16:46:57.661] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=0000024AB2843810 stream=Video payload=Frame pts=2434456261 dts=2434456261 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:46:57.661] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=0000024AB2843810 stream=Video payload=Frame pts=2434456261 dts=2434456261 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:46:57.661] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-10-21 16:46:57.662] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-10-21 16:46:57.662] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=2431864937 format=cuda size=1280x720 buffer_identity=0xb08a00000 drm_prime=0 software_frame=0
[2026-10-21 16:46:57.662] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=0000024AB2842460 stream=Video payload=Frame pts=2431864937 dts=2431864937 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:46:57.662] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=0000024AB2842460 stream=Video payload=Frame pts=2431864937 dts=2431864937 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:46:57.662] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=0000024AB2842460 stream=Video payload=Frame pts=2431864937 dts=2431864937 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:46:57.663] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-10-21 16:46:57.663] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=0000024AB2842850 stream=Video payload=Frame pts=2431864937 dts=2431864937 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:46:57.663] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=0000024AB2842850 stream=Video payload=Frame pts=2431864937 dts=2431864937 duration=3000 tb=1/90000 flags=68
[2026-10-21 16:46:57.663] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-10-21 16:46:57.755] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=91
[2026-10-21 16:46:57.755] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=93
[2026-10-21 16:46:57.756] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=94
[2026-10-21 16:46:57.756] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb0ae90000 drm_prime=0 software_frame=0
[2026-10-21 16:46:57.756] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=16 audio_count=0 first_video_sequence=1
[2026-10-21 16:46:57.757] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=26 audio_count=23 first_video_sequence=15
[2026-10-21 16:46:57.757] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb0af20000 drm_prime=0 software_frame=0
[2026-10-21 16:46:57.757] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=0
[2026-10-21 16:46:57.757] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=23
[2026-10-21 16:46:57.757] [info] [graph][runtime.node] audio_decode_trace stage=first_packet
[2026-10-21 16:46:57.758] [info] [graph][runtime.node] audio_decode_trace stage=first_frame samples=1024 rate=44100
[2026-10-21 16:46:57.758] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=1 source_master_ns=5566408821 raw_phase_ns=18141 filtered_phase_ns=18141 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=0
[2026-10-21 16:46:57.758] [info] [graph][runtime.node] video_filter.trace stage=first_output format=cuda size=640x360 buffer_identity=0xb0ae00000 drm_prime=0 software_frame=0
[2026-10-21 16:46:57.758] [info] [graph][runtime.node] av_release_trace stage=first_release generation=1 video_count=16 audio_count=0 first_video_sequence=1
[2026-10-21 16:46:57.758] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=3 source_master_ns=5612848730 raw_phase_ns=18141 filtered_phase_ns=18141 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=45346
[2026-10-21 16:46:57.759] [info] [graph][runtime.node] av_release_trace stage=first_commit video_count=0 audio_count=0
[2026-10-21 16:46:57.759] [info] [graph][runtime.node] mpegts_emission_plan generation=1 mode=canonical_access_unit_window access_unit_window_ns=100000000 video_initial_service_window_ns=13990650 audio_initial_service_window_ns=13990650 packet_size_bytes=188 maximum_payload_bytes=1316 per_datagram_overhead_bytes=0
[2026-10-21 16:46:57.759] [info] [graph][runtime.node] audio_encode_trace stage=first_frame
[2026-10-21 16:46:57.761] [info] [graph][runtime.node] video_encode.input_contract format=cuda size=1280x720 buffer_identity=0xb0a000000 drm_prime=0 software_frame=0
[2026-10-21 16:46:57.761] [info] [graph][runtime.node] video_encode.generation_start_key_frame pts=0 generation=1
[2026-10-21 16:46:57.761] [info] [graph][runtime.node] video_encode.first_frame pts=0 generation=attached
[2026-10-21 16:46:57.768] [info] [graph][runtime.node] video_encode.first_submit result=accepted
[2026-10-21 16:46:57.769] [info] [graph][runtime.node] video_encode.first_packet pts=0 dts=0 duration=0 size=39648 side_data_elems=1 received_footprint=39656 side_data[0]=Quality stats:8
[2026-10-21 16:46:57.769] [info] [graph][runtime.node] video_encode.first_packet_footprint bytes=39656 wrapped_size=39648 wrapped_side_data_elems=1
[2026-10-21 16:46:57.769] [info] [graph][runtime.node] av_scheduler_trace stage=first_video_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=5564985268 dispatch_master_ns=5564985268 now_ns=5408311900 node=74 identity_kind=output identity="composition-r296.output" canonical_sequence=1 canonical_created_steady_ns=29001685711100 head_observed_steady_ns=29001685734600 codec_receive_begin_steady_ns=29001685355900 codec_receive_end_steady_ns=29001685357000
[2026-10-21 16:46:57.769] [info] [graph][runtime.node] av_scheduler_trace stage=missing_media_wait missing=audio deadline_ns=none now_ns=5408449900 present_generation=1 present_canonical_target_ns=0 present_canonical_dispatch_ns=0 present_target_master_ns=5564985268 present_dispatch_master_ns=5564985268
[2026-10-21 16:46:57.807] [info] [graph][runtime.node] audio_encode_trace stage=first_packet
[2026-10-21 16:46:57.808] [info] [graph][runtime.node] av_scheduler_trace stage=first_audio_head generation=1 mapping_epoch_generation=1 canonical_target_ns=0 canonical_dispatch_ns=0 target_master_ns=5564985268 dispatch_master_ns=5564985268 now_ns=5447146100 node=74 identity_kind=output identity="composition-r296.output" canonical_sequence=1 canonical_created_steady_ns=29001724545100 head_observed_steady_ns=29001724568900 codec_receive_begin_steady_ns=unavailable codec_receive_end_steady_ns=unavailable
[2026-10-21 16:46:58.003] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=running detail="" description=""
[2026-10-21 16:46:58.386] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=48 source_master_ns=6657746690 raw_phase_ns=18142 filtered_phase_ns=18141 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=90692
[2026-10-21 16:46:59.409] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=92 source_master_ns=7679424694 raw_phase_ns=18141 filtered_phase_ns=18141 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=136038
[2026-10-21 16:47:00.431] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=136 source_master_ns=8701102699 raw_phase_ns=18142 filtered_phase_ns=18141 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=181384
[2026-10-21 16:47:01.452] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=180 source_master_ns=9722780702 raw_phase_ns=18140 filtered_phase_ns=18141 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=226730
[2026-10-21 16:47:02.392] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1099626866 ntp_seconds=4001561222 ntp_fraction=1679332212 arrival_ns=29006308201900
[2026-10-21 16:47:02.392] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1099626866
[2026-10-21 16:47:02.392] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:47:02.407] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2461219329 ntp_seconds=4001561222 ntp_fraction=1748051689 arrival_ns=29006323404900
[2026-10-21 16:47:02.407] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2461219329
[2026-10-21 16:47:02.407] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2184902093 ntp_seconds=4001561222 ntp_fraction=1748051689 arrival_ns=29006323871700
[2026-10-21 16:47:02.407] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2184902093
[2026-10-21 16:47:02.407] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:47:02.407] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:47:02.422] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3708029367 ntp_seconds=4001561222 ntp_fraction=1812476198 arrival_ns=29006338977900
[2026-10-21 16:47:02.423] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3708029367
[2026-10-21 16:47:02.423] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:47:02.496] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=225 source_master_ns=10767678661 raw_phase_ns=18140 filtered_phase_ns=18141 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=272076
[2026-10-21 16:47:03.518] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=269 source_master_ns=11789356666 raw_phase_ns=18141 filtered_phase_ns=18141 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=317422
[2026-10-21 16:47:04.540] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=313 source_master_ns=12811034670 raw_phase_ns=18140 filtered_phase_ns=18141 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=362768
[2026-10-21 16:47:05.588] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=358 source_master_ns=13855932630 raw_phase_ns=18141 filtered_phase_ns=18141 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=408114
[2026-10-21 16:47:06.606] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=402 source_master_ns=14877610634 raw_phase_ns=18140 filtered_phase_ns=18141 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=453460
[2026-10-21 16:47:07.397] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1099626866 ntp_seconds=4001561227 ntp_fraction=1705102016 arrival_ns=29011313966800
[2026-10-21 16:47:07.398] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1099626866
[2026-10-21 16:47:07.398] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:47:07.411] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2461219329 ntp_seconds=4001561227 ntp_fraction=1760936591 arrival_ns=29011327088200
[2026-10-21 16:47:07.411] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2461219329
[2026-10-21 16:47:07.411] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:47:07.426] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2184902093 ntp_seconds=4001561227 ntp_fraction=1829656068 arrival_ns=29011342914800
[2026-10-21 16:47:07.427] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2184902093
[2026-10-21 16:47:07.427] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:47:07.444] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3708029367 ntp_seconds=4001561227 ntp_fraction=1902670512 arrival_ns=29011360318500
[2026-10-21 16:47:07.444] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3708029367
[2026-10-21 16:47:07.444] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:47:07.630] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=446 source_master_ns=15899288639 raw_phase_ns=18141 filtered_phase_ns=18141 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=498806
[2026-10-21 16:47:08.649] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=490 source_master_ns=16920966643 raw_phase_ns=18140 filtered_phase_ns=18141 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=544152
[2026-10-21 16:47:09.694] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=535 source_master_ns=17965864602 raw_phase_ns=18140 filtered_phase_ns=18141 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=589498
[2026-10-21 16:47:10.716] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=579 source_master_ns=18987542607 raw_phase_ns=18141 filtered_phase_ns=18141 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=634844
[2026-10-21 16:47:11.738] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=623 source_master_ns=20009220611 raw_phase_ns=18140 filtered_phase_ns=18141 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=680190
[2026-10-21 16:47:12.420] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1099626866 ntp_seconds=4001561232 ntp_fraction=1803886264 arrival_ns=29016336866500
[2026-10-21 16:47:12.420] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1099626866
[2026-10-21 16:47:12.421] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:47:12.435] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2184902093 ntp_seconds=4001561232 ntp_fraction=1868310773 arrival_ns=29016351836900
[2026-10-21 16:47:12.435] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2184902093
[2026-10-21 16:47:12.436] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:47:12.450] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2461219329 ntp_seconds=4001561232 ntp_fraction=1932735283 arrival_ns=29016366956900
[2026-10-21 16:47:12.450] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3708029367 ntp_seconds=4001561232 ntp_fraction=1932735283 arrival_ns=29016367053900
[2026-10-21 16:47:12.451] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3708029367
[2026-10-21 16:47:12.451] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:47:12.451] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2461219329
[2026-10-21 16:47:12.451] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:47:12.783] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=668 source_master_ns=21054118571 raw_phase_ns=18141 filtered_phase_ns=18141 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=725536
[2026-10-21 16:47:13.804] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=712 source_master_ns=22075796575 raw_phase_ns=18140 filtered_phase_ns=18141 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=770882
[2026-10-21 16:47:14.826] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=756 source_master_ns=23097474580 raw_phase_ns=18141 filtered_phase_ns=18141 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=816228
[2026-10-21 16:47:15.848] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=800 source_master_ns=24119152584 raw_phase_ns=18140 filtered_phase_ns=18141 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=861574
[2026-10-21 16:47:16.893] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=845 source_master_ns=25164050543 raw_phase_ns=18140 filtered_phase_ns=18141 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=906920
[2026-10-21 16:47:17.431] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=audio generation=0 ssrc=1099626866 ntp_seconds=4001561237 ntp_fraction=1846835937 arrival_ns=29021347228500
[2026-10-21 16:47:17.431] [info] [graph][runtime.node] rtp_clock_group_evidence stream=audio generation=0 ssrc=1099626866
[2026-10-21 16:47:17.431] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:47:17.461] [info] [graph][runtime.node] rtp_clock_group_candidate node=48 stream=video generation=0 ssrc=2184902093 ntp_seconds=4001561237 ntp_fraction=1979979923 arrival_ns=29021378052600
[2026-10-21 16:47:17.462] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2184902093
[2026-10-21 16:47:17.462] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:47:17.477] [info] [graph][runtime.node] rtp_clock_group_candidate node=9 stream=video generation=0 ssrc=2461219329 ntp_seconds=4001561237 ntp_fraction=2048699400 arrival_ns=29021393927800
[2026-10-21 16:47:17.477] [info] [graph][runtime.node] rtp_clock_group_candidate node=28 stream=video generation=0 ssrc=3708029367 ntp_seconds=4001561237 ntp_fraction=2048699400 arrival_ns=29021393996800
[2026-10-21 16:47:17.478] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=2461219329
[2026-10-21 16:47:17.478] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:47:17.478] [info] [graph][runtime.node] rtp_clock_group_evidence stream=video generation=0 ssrc=3708029367
[2026-10-21 16:47:17.479] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-21 16:47:18.432] [info] [graph][runtime.node] process.failed node=81 name=MediaScheduledDatagramSenderNode kind=ScheduledDatagramSender inputs=[channel=111 edge=111 stream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=122 edge=122 stream=Metadata edge_kind=Unknown payload=Unknown queued=0] outputs=[] error=IoFailure: scheduled datagram submit exceeded its original deadline generation=1 sequence=14492 endpoint=1 release_ns=25527026084 reserved_at_ns=25507841800 not_before_ns=25527026084 deadline_ns=25626901284 observed_at_ns=26071760200
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.failed node=81 kind=ScheduledDatagramSender name=Common GCRA-paced nonblocking datagram sender error=IoFailure: scheduled datagram submit exceeded its original deadline generation=1 sequence=14492 endpoint=1 release_ns=25527026084 reserved_at_ns=25507841800 not_before_ns=25527026084 deadline_ns=25626901284 observed_at_ns=26071760200
[2026-10-21 16:47:18.432] [info] [graph][runtime.node] protocol_batch_trace stage=materialization_failed generation=1 attempts=1 consumed_datagrams=0 remaining_datagrams=1 attempted_master_ns=26071750800 mux_dequeued_master_ns=26065025100 first_dequeued_master_ns=26071749600 last_wire_published_master_ns=25507835900 previous_attempt_started_master_ns=unavailable previous_attempt_completed_master_ns=unavailable last_attempt_completed_master_ns=26071796600 last_blocked_started_master_ns=unavailable last_blocked_completed_master_ns=unavailable blocked_attempts=0 remaining_front_release_master_ns=25531652336 remaining_front_deadline_master_ns=25598319004 original_error="InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=25531652336 materialized_ns=26071750800 protocol_deadline_ns=25598319004 deadline_before_release=0 materialized_after_deadline=1"
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] av_drift_trace generation=1 sequence=889 source_master_ns=26185728548 raw_phase_ns=18141 filtered_phase_ns=18141 filtered_frequency_ppm=0 integral_ppm=0 recovering=0 stretch_ppm=0 sample_delta=0 compensation_distance=45346 effective_output_sample_index=952266
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] process.failed node=73 name=AudioResampleNode kind=AudioResample inputs=[channel=94 edge=94 stream=Audio edge_kind=Metadata payload=CodecContext queued=0; channel=97 edge=97 stream=Audio edge_kind=RawFrame payload=Frame queued=3; channel=98 edge=98 stream=Audio edge_kind=Event payload=GraphEvent queued=0] outputs=[channel=101 edge=101 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=5] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=51 available=1 cpu_ns=0 process_calls=3065
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=31 available=1 cpu_ns=0 process_calls=15
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] process.failed node=7 name=MediaContinuousAudioAggregateNode kind=ContinuousAudioAggregate inputs=[channel=101 edge=101 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=5; channel=103 edge=103 stream=Audio edge_kind=Metadata payload=CodecContext queued=0; channel=109 edge=109 stream=Metadata edge_kind=Event payload=GraphEvent queued=0] outputs=[channel=105 edge=105 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=5] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=52 available=1 cpu_ns=0 process_calls=28
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=50 available=1 cpu_ns=15625000 process_calls=2131
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=13 available=1 cpu_ns=0 process_calls=21
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=32 available=1 cpu_ns=0 process_calls=21
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=34 available=1 cpu_ns=15625000 process_calls=2245
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=37 available=1 cpu_ns=0 process_calls=2259
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=60 available=1 cpu_ns=15625000 process_calls=5246
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=53 available=1 cpu_ns=0 process_calls=42
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=61 available=1 cpu_ns=15625000 process_calls=5080
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=19 available=1 cpu_ns=31250000 process_calls=2210
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=38 available=1 cpu_ns=31250000 process_calls=2210
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] process.failed node=71 name=MediaAudioStartupTrimNode kind=Unknown inputs=[channel=95 edge=95 stream=Audio edge_kind=RawFrame payload=Frame queued=6] outputs=[channel=96 edge=96 stream=Audio edge_kind=RawFrame payload=Frame queued=5] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-21 16:47:18.437] [info] [graph][runtime.node] worker.thread_cpu node=71 available=1 cpu_ns=31250000 process_calls=2882
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=68 available=1 cpu_ns=359375000 process_calls=2232
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=14 available=1 cpu_ns=0 process_calls=2244
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=44 available=1 cpu_ns=31250000 process_calls=3255
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=25 available=1 cpu_ns=46875000 process_calls=3319
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=67 available=1 cpu_ns=31250000 process_calls=3212
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=76 available=1 cpu_ns=125000000 process_calls=5437
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=20 available=1 cpu_ns=31250000 process_calls=2166
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=64 available=1 cpu_ns=140625000 process_calls=2472
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=58 available=1 cpu_ns=93750000 process_calls=5273
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=39 available=1 cpu_ns=15625000 process_calls=2175
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=70 available=1 cpu_ns=62500000 process_calls=4389
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] process.failed node=4 name=AudioEncodeNode kind=AudioEncode inputs=[channel=2 edge=2 stream=Audio edge_kind=Metadata payload=CodecContext queued=0; channel=105 edge=105 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=5] outputs=[channel=3 edge=3 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0; channel=115 edge=115 stream=Audio edge_kind=Metadata payload=CodecContext queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=62 available=1 cpu_ns=46875000 process_calls=5034
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=55 available=1 cpu_ns=0 process_calls=3248
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=81 available=1 cpu_ns=859375000 process_calls=4325
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] process.failed node=46 name=RawRtpInputNode kind=RawRtpInput inputs=[] outputs=[channel=58 edge=58 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=59 edge=59 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=66 edge=66 stream=Video edge_kind=InputPacket payload=Packet queued=0; channel=83 edge=83 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] process.failed node=27 name=RawRtpInputNode kind=RawRtpInput inputs=[] outputs=[channel=31 edge=31 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=32 edge=32 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=36 edge=36 stream=Video edge_kind=InputPacket payload=Packet queued=0; channel=48 edge=48 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=9 available=1 cpu_ns=15625000 process_calls=16
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=28 available=1 cpu_ns=15625000 process_calls=15
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=2 available=1 cpu_ns=296875000 process_calls=1814
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=10 available=1 cpu_ns=0 process_calls=20
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=49 available=1 cpu_ns=0 process_calls=43
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=29 available=1 cpu_ns=31250000 process_calls=21
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=48 available=1 cpu_ns=0 process_calls=30
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=11 available=1 cpu_ns=0 process_calls=2133
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=12 available=1 cpu_ns=15625000 process_calls=16
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=33 available=1 cpu_ns=0 process_calls=2273
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=36 available=1 cpu_ns=0 process_calls=15
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=30 available=1 cpu_ns=31250000 process_calls=2103
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=15 available=1 cpu_ns=31250000 process_calls=2249
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=18 available=1 cpu_ns=46875000 process_calls=2241
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=17 available=1 cpu_ns=0 process_calls=16
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=57 available=1 cpu_ns=78125000 process_calls=3243
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=56 available=1 cpu_ns=31250000 process_calls=2278
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=16 available=1 cpu_ns=31250000 process_calls=2312
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=42 available=1 cpu_ns=0 process_calls=1798
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=35 available=1 cpu_ns=15625000 process_calls=2307
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=54 available=1 cpu_ns=46875000 process_calls=2285
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=65 available=1 cpu_ns=0 process_calls=1798
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] process.failed node=8 name=RawRtpInputNode kind=RawRtpInput inputs=[] outputs=[channel=4 edge=4 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=5 edge=5 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=9 edge=9 stream=Video edge_kind=InputPacket payload=Packet queued=0; channel=21 edge=21 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=45 available=1 cpu_ns=250000000 process_calls=2183
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=59 available=1 cpu_ns=0 process_calls=29
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=75 available=1 cpu_ns=15625000 process_calls=4391
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=78 available=1 cpu_ns=62500000 process_calls=4369
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=23 available=1 cpu_ns=15625000 process_calls=1862
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=41 available=1 cpu_ns=187500000 process_calls=1629
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=22 available=1 cpu_ns=203125000 process_calls=1637
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=43 available=1 cpu_ns=0 process_calls=1877
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=66 available=1 cpu_ns=0 process_calls=1875
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=26 available=1 cpu_ns=218750000 process_calls=2212
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=24 available=1 cpu_ns=15625000 process_calls=1934
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=77 available=1 cpu_ns=0 process_calls=6
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=80 available=1 cpu_ns=0 process_calls=5
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=5 available=1 cpu_ns=62500000 process_calls=2596
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=74 available=1 cpu_ns=46875000 process_calls=4390
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] process.failed node=79 name=MediaMpegTsDatagramMaterializerNode kind=MpegTsDatagramMaterializer inputs=[channel=118 edge=118 stream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=120 edge=120 stream=Metadata edge_kind=Metadata payload=Unknown queued=0; channel=121 edge=121 stream=Metadata edge_kind=Unknown payload=Unknown queued=10] outputs=[channel=122 edge=122 stream=Metadata edge_kind=Unknown payload=Unknown queued=0] error=InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=25531652336 materialized_ns=26071750800 protocol_deadline_ns=25598319004 deadline_before_release=0 materialized_after_deadline=1
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] worker.thread_cpu node=72 available=1 cpu_ns=46875000 process_calls=2818
[2026-10-21 16:47:18.434] [info] [graph][runtime.node] worker.thread_cpu node=73 available=1 cpu_ns=109375000 process_calls=4371
[2026-10-21 16:47:18.434] [info] [graph][runtime.node] worker.thread_cpu node=7 available=1 cpu_ns=171875000 process_calls=3635
[2026-10-21 16:47:18.435] [info] [graph][runtime.node] worker.thread_cpu node=6 available=1 cpu_ns=468750000 process_calls=4174
[2026-10-21 16:47:18.433] [info] [graph][runtime.node] process.failed node=47 name=RawRtpInputNode kind=RawRtpInput inputs=[] outputs=[channel=60 edge=60 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=61 edge=61 stream=Metadata edge_kind=Event payload=GraphEvent queued=0; channel=67 edge=67 stream=Audio edge_kind=InputPacket payload=Packet queued=0; channel=91 edge=91 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-21 16:47:18.440] [info] [graph][runtime.node] worker.thread_cpu node=4 available=1 cpu_ns=546875000 process_calls=2721
[2026-10-21 16:47:18.440] [info] [graph][runtime.node] scheduled_datagram_sender stage=failed generation=1 service_scope=ifindex:1 execution_mode=1 committed_batches=2173 committed_datagrams=14491 committed_payload_bytes=17402972 would_block=0 writable_waits=0 deadline_misses=1 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=1 ordered_queue_maximum_datagrams=59 ordered_queue_maximum_wire_bytes=79296 pacing_reserved=14492 pacing_cancelled=0 pacing_submitted=14491 pacing_maximum_submit_lateness_ns=5972740 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=6250000 delivery_evidence=not_proven backlog_cancelled_datagrams=0 backlog_cancelled_wire_bytes=0 backlog_current_datagrams=1 backlog_current_wire_bytes=780 backlog_high_water_datagrams=59 backlog_high_water_wire_bytes=79296 backlog_max_residence_ns=50825400 last_materialized_sequence=14492 last_scheduled_sequence=14492 last_submitted_sequence=14491 last_committed_sequence=14491 shared_scope_charged_datagrams=14491 shared_scope_charged_wire_bytes=17808720 shared_scope_waiting_members=0 shared_scope_failed=0 evidence_submitted=2154 evidence_timestamp_tracked=0 evidence_timestamp_untracked=2154 evidence_observed=0 evidence_lost=0 evidence_late=0 evidence_duplicate=0 evidence_unmatched=0 evidence_coverage_complete=0 aggregate_effective_socket_bytes=188010 endpoint_1_target_effective_socket_bytes=188010 endpoint_1_api_requested_socket_bytes=188010 endpoint_1_effective_socket_bytes=188010 endpoint_1_timestamp_source=0 endpoint_1_committed_datagrams=14491 endpoint_1_committed_payload_bytes=17402972
[2026-10-21 16:47:18.440] [info] [graph][runtime.node] worker.thread_cpu node=46 available=1 cpu_ns=62500000 process_calls=15348
[2026-10-21 16:47:18.441] [info] [graph][runtime.node] worker.thread_cpu node=27 available=1 cpu_ns=78125000 process_calls=16115
[2026-10-21 16:47:18.445] [info] [graph][runtime.node] worker.thread_cpu node=8 available=1 cpu_ns=62500000 process_calls=15680
[2026-10-21 16:47:18.448] [info] [graph][runtime.node] worker.secondary_failure node=79 kind=MpegTsDatagramMaterializer name=MPEG-TS wire protocol materializer error=InvalidArgument: protocol datagram exceeded its absolute emission deadline before materialization endpoint=1 release_ns=25531652336 materialized_ns=26071750800 protocol_deadline_ns=25598319004 deadline_before_release=0 materialized_after_deadline=1
[2026-10-21 16:47:18.449] [info] [graph][runtime.node] worker.thread_cpu node=47 available=1 cpu_ns=62500000 process_calls=15929
[2026-10-21 16:47:18.449] [info] [graph][runtime.node] worker.secondary_failure node=81 kind=ScheduledDatagramSender name=Common GCRA-paced nonblocking datagram sender error=IoFailure: scheduled datagram submit exceeded its original deadline generation=1 sequence=14492 endpoint=1 release_ns=25527026084 reserved_at_ns=25507841800 not_before_ns=25527026084 deadline_ns=25626901284 observed_at_ns=26071760200
[2026-10-21 16:47:18.450] [info] [graph][runtime.node] worker.thread_cpu node=79 available=1 cpu_ns=109375000 process_calls=8121
[2026-10-21 16:47:18.501] [info] [graph][runtime.lifecycle] node_abort_payload node=81 before_objects=784 after_objects=784 before_bytes=13731858 after_bytes=13731858 before_reservations=25472 after_reservations=25472 before_releases=24688 after_releases=24688
[2026-10-21 16:47:18.501] [info] [graph][runtime.node] mpegts_wire_materializer stage=aborted maximum_materialized_after_release_ns=47731052 worst_materialized_at_ns=9512716400 worst_release_ns=9464985348 worst_deadline_ns=9531652016 worst_global_sequence=3027
[2026-10-21 16:47:18.502] [info] [graph][runtime.lifecycle] node_abort_payload node=79 before_objects=784 after_objects=784 before_bytes=13731858 after_bytes=13731858 before_reservations=25472 after_reservations=25472 before_releases=24688 after_releases=24688
[2026-10-21 16:47:18.502] [info] [graph][runtime.node] mpegts_emission stage=final generation=1 datagrams=0 wire_bytes=0 immediate_deadlines=0 deferred_deadlines=0 planned_wait_ns=0 late_datagrams=0 maximum_lateness_ns=0 pending_bytes=0 peak_pending_bytes=94564 pressure_failures=0 access_units=1467 current_scheduling_debt_ns=0 maximum_scheduling_debt_ns=392 maximum_mux_input_after_emit_ns=11229799 maximum_mux_input_after_dispatch_ns=-88770201 worst_mux_input_ready_ns=13209548400 worst_mux_input_emit_ns=13198318601 worst_mux_input_dispatch_ns=13298318601 maximum_protocol_batch_produced_after_release_ns=533372764 worst_protocol_batch_produced_ns=26065025100 worst_protocol_batch_release_ns=25531652336 worst_protocol_batch_deadline_ns=25598319004 worst_protocol_batch_datagrams=1 exit_reason=aborted
[2026-10-21 16:47:18.502] [info] [graph][runtime.lifecycle] node_abort_payload node=76 before_objects=784 after_objects=784 before_bytes=13731858 after_bytes=13731858 before_reservations=25472 after_reservations=25472 before_releases=24688 after_releases=24688
[2026-10-21 16:47:18.503] [info] [graph][runtime.lifecycle] node_abort_payload node=78 before_objects=784 after_objects=784 before_bytes=13731858 after_bytes=13731858 before_reservations=25472 after_reservations=25472 before_releases=24688 after_releases=24688
[2026-10-21 16:47:18.503] [info] [graph][runtime.lifecycle] node_abort_payload node=75 before_objects=784 after_objects=784 before_bytes=13731858 after_bytes=13731858 before_reservations=25472 after_reservations=25472 before_releases=24688 after_releases=24688
[2026-10-21 16:47:18.503] [info] [graph][runtime.node] audio_stage_observation node=74 stage=scheduler_input count=865 last_generation=1 last_begin=884736 last_end=885760 last_rate=44100 last_steady_ns=29021785078100 maximum_inter_event_gap_ns=28851400 gap_begin_generation=1 gap_begin_begin=65536 gap_begin_end=66560 gap_begin_rate=44100 gap_begin_steady_ns=29003209023100 gap_end_generation=1 gap_end_begin=66560 gap_end_end=67584 gap_end_rate=44100 gap_end_steady_ns=29003237874500
[2026-10-21 16:47:18.504] [info] [graph][runtime.node] av_scheduler_correlated_wait generation=1 canonical_sequence=1 missing=audio begin_steady_ns=29001685870600 end_steady_ns=29001724738400 observed_wait_ns=38867800 held_dispatch_master_ns=5564985268 begin_master_ns=5408447800 end_master_ns=5447314300 dispatch_remaining_at_end_ns=117670968 resumed_dispatch_master_ns=5564985268
[2026-10-21 16:47:18.504] [info] [graph][runtime.lifecycle] node_abort_payload node=74 before_objects=784 after_objects=783 before_bytes=13731858 after_bytes=13709763 before_reservations=25472 after_reservations=25472 before_releases=24688 after_releases=24689
[2026-10-21 16:47:18.504] [info] [graph][runtime.node] audio_stage_observation node=5 stage=canonicalizer_input count=865 last_generation=1 last_begin=884736 last_end=885760 last_rate=44100 last_steady_ns=29021785058700 maximum_inter_event_gap_ns=28833500 gap_begin_generation=1 gap_begin_begin=65536 gap_begin_end=66560 gap_begin_rate=44100 gap_begin_steady_ns=29003209000000 gap_end_generation=1 gap_end_begin=66560 gap_end_end=67584 gap_end_rate=44100 gap_end_steady_ns=29003237833500
[2026-10-21 16:47:18.504] [info] [graph][runtime.node] audio_stage_observation node=5 stage=canonicalizer_committed count=865 last_generation=1 last_begin=884736 last_end=885760 last_rate=44100 last_steady_ns=29021785067800 maximum_inter_event_gap_ns=28837700 gap_begin_generation=1 gap_begin_begin=65536 gap_begin_end=66560 gap_begin_rate=44100 gap_begin_steady_ns=29003209005400 gap_end_generation=1 gap_end_begin=66560 gap_end_end=67584 gap_end_rate=44100 gap_end_steady_ns=29003237843100
[2026-10-21 16:47:18.505] [info] [graph][runtime.lifecycle] node_abort_payload node=5 before_objects=783 after_objects=783 before_bytes=13709763 after_bytes=13709763 before_reservations=25472 after_reservations=25472 before_releases=24689 after_releases=24689
[2026-10-21 16:47:18.505] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_input count=868 last_generation=1 last_begin=887808 last_end=888832 last_rate=44100 last_steady_ns=29022349448500 maximum_inter_event_gap_ns=564625400 gap_begin_generation=1 gap_begin_begin=886784 gap_begin_end=887808 gap_begin_rate=44100 gap_begin_steady_ns=29021784823100 gap_end_generation=1 gap_end_begin=887808 gap_end_end=888832 gap_end_rate=44100 gap_end_steady_ns=29022349448500
[2026-10-21 16:47:18.505] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_submitted count=868 last_generation=1 last_begin=887808 last_end=888832 last_rate=44100 last_steady_ns=29022349682900 maximum_inter_event_gap_ns=564648200 gap_begin_generation=1 gap_begin_begin=886784 gap_begin_end=887808 gap_begin_rate=44100 gap_begin_steady_ns=29021785034700 gap_end_generation=1 gap_end_begin=887808 gap_end_end=888832 gap_end_rate=44100 gap_end_steady_ns=29022349682900
[2026-10-21 16:47:18.505] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_packet count=865 last_generation=1 last_begin=884736 last_end=885760 last_rate=44100 last_steady_ns=29021785041600 maximum_inter_event_gap_ns=28828500 gap_begin_generation=1 gap_begin_begin=65536 gap_begin_end=66560 gap_begin_rate=44100 gap_begin_steady_ns=29003208987500 gap_end_generation=1 gap_end_begin=66560 gap_end_end=67584 gap_end_rate=44100 gap_end_steady_ns=29003237816000
[2026-10-21 16:47:18.506] [info] [graph][runtime.node] audio_stage_observation node=4 stage=encoder_committed count=865 last_generation=1 last_begin=884736 last_end=885760 last_rate=44100 last_steady_ns=29021785050100 maximum_inter_event_gap_ns=28829900 gap_begin_generation=1 gap_begin_begin=65536 gap_begin_end=66560 gap_begin_rate=44100 gap_begin_steady_ns=29003208992800 gap_end_generation=1 gap_end_begin=66560 gap_end_end=67584 gap_end_rate=44100 gap_end_steady_ns=29003237822700
[2026-10-21 16:47:18.506] [info] [graph][runtime.lifecycle] codec_owner_release node=4 kind=AudioEncode owner_references=4
[2026-10-21 16:47:18.506] [info] [graph][runtime.lifecycle] node_abort_payload node=4 before_objects=783 after_objects=783 before_bytes=13709763 after_bytes=13709763 before_reservations=25472 after_reservations=25472 before_releases=24689 after_releases=24689
[2026-10-21 16:47:18.506] [info] [graph][runtime.lifecycle] node_abort_payload node=77 before_objects=783 after_objects=783 before_bytes=13709763 after_bytes=13709763 before_reservations=25472 after_reservations=25472 before_releases=24689 after_releases=24689
[2026-10-21 16:47:18.506] [info] [graph][runtime.lifecycle] node_abort_payload node=80 before_objects=783 after_objects=783 before_bytes=13709763 after_bytes=13709763 before_reservations=25472 after_reservations=25472 before_releases=24689 after_releases=24689
[2026-10-21 16:47:18.506] [info] [graph][runtime.node] audio_stage_observation node=7 stage=aggregate_built count=873 last_generation=1 last_begin=892928 last_end=893952 last_rate=44100 last_steady_ns=29022349550300 maximum_inter_event_gap_ns=564679900 gap_begin_generation=1 gap_begin_begin=886784 gap_begin_end=887808 gap_begin_rate=44100 gap_begin_steady_ns=29021784724600 gap_end_generation=1 gap_end_begin=887808 gap_end_end=888832 gap_end_rate=44100 gap_end_steady_ns=29022349404500
[2026-10-21 16:47:18.507] [info] [graph][runtime.node] audio_stage_observation node=7 stage=aggregate_committed count=873 last_generation=1 last_begin=892928 last_end=893952 last_rate=44100 last_steady_ns=29022349553200 maximum_inter_event_gap_ns=564638500 gap_begin_generation=1 gap_begin_begin=886784 gap_begin_end=887808 gap_begin_rate=44100 gap_begin_steady_ns=29021784776300 gap_end_generation=1 gap_end_begin=887808 gap_end_end=888832 gap_end_rate=44100 gap_end_steady_ns=29022349414800
[2026-10-21 16:47:18.507] [info] [graph][runtime.node] continuous_audio_aggregate.summary reason=abort output_group=composition-r296.output audio_commits=873 real_samples=893952 silence_samples=0
[2026-10-21 16:47:18.507] [info] [graph][runtime.lifecycle] node_abort_payload node=7 before_objects=783 after_objects=777 before_bytes=13709763 after_bytes=13660611 before_reservations=25472 after_reservations=25472 before_releases=24689 after_releases=24695
[2026-10-21 16:47:18.507] [info] [graph][runtime.lifecycle] codec_owner_release node=2 kind=VideoEncode owner_references=4
[2026-10-21 16:47:18.507] [info] [graph][runtime.node] video_encoder_execution_observation node=2 count=604 maximum_call_ns=9212100 maximum_begin_steady_ns=29014944177300 maximum_end_steady_ns=29014953389400 maximum_encoder_pts=398 maximum_native_result=0 maximum_between_submissions_ns=36373300 before_gap_begin_steady_ns=29009443189100 before_gap_end_steady_ns=29009444891600 before_gap_encoder_pts=233 before_gap_native_result=0 after_gap_begin_steady_ns=29009481264900 after_gap_end_steady_ns=29009482696000 after_gap_encoder_pts=234 after_gap_native_result=0
[2026-10-21 16:47:18.507] [info] [graph][runtime.lifecycle] node_abort_payload node=2 before_objects=777 after_objects=777 before_bytes=13660611 after_bytes=13660611 before_reservations=25472 after_reservations=25472 before_releases=24695 after_releases=24695
[2026-10-21 16:47:18.507] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=process count=4174 maximum_ns=10123800 begin_steady_ns=29002882591000 end_steady_ns=29002892714800 video_cursor=37 audio_cursor=not_owned next_video_cursor=37 next_audio_cursor=not_owned error=None return_state=waiting deadline_ns=6631651933 deadline_clock=master wake_policy=0 pending_at_entry=0 entered_onprocess=1
[2026-10-21 16:47:18.508] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=purge count=4174 maximum_ns=355900 begin_steady_ns=29010511226500 end_steady_ns=29010511582400 video_cursor=266 audio_cursor=not_owned next_video_cursor=266 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-21 16:47:18.508] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=bind count=4174 maximum_ns=24000 begin_steady_ns=29020610506400 end_steady_ns=29020610530400 video_cursor=569 audio_cursor=not_owned next_video_cursor=569 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-21 16:47:18.508] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=consume count=4174 maximum_ns=10119700 begin_steady_ns=29002882591700 end_steady_ns=29002892711400 video_cursor=37 audio_cursor=not_owned next_video_cursor=37 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-21 16:47:18.508] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=activate count=13 maximum_ns=275800 begin_steady_ns=29001675740800 end_steady_ns=29001676016600 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-21 16:47:18.508] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=build_video count=605 maximum_ns=4302300 begin_steady_ns=29020975878700 end_steady_ns=29020980181000 video_cursor=579 audio_cursor=not_owned next_video_cursor=579 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-21 16:47:18.508] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=canvas count=605 maximum_ns=4262900 begin_steady_ns=29020975901200 end_steady_ns=29020980164100 video_cursor=579 audio_cursor=not_owned next_video_cursor=579 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-21 16:47:18.509] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=emit count=605 maximum_ns=6194400 begin_steady_ns=29003278507300 end_steady_ns=29003284701700 video_cursor=48 audio_cursor=not_owned next_video_cursor=48 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-21 16:47:18.509] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=between_calls_previous_return count=4173 maximum_ns=567937500 begin_steady_ns=29021781284400 end_steady_ns=29022349221900 video_cursor=604 audio_cursor=not_owned next_video_cursor=604 next_audio_cursor=not_owned error=None return_state=waiting deadline_ns=25531651933 deadline_clock=master wake_policy=0 pending_at_entry=0 entered_onprocess=1
[2026-10-21 16:47:18.509] [info] [graph][runtime.node] continuous_aggregate.summary reason=abort output_group=composition-r296.output output_generation=1 video_commits=605 source[0].group=composition-r296.source.0 source[0].real_frames=605 source[0].black_frames=0 source[0].contribution_generation=1 source[1].group=composition-r296.source.1 source[1].real_frames=605 source[1].black_frames=0 source[1].contribution_generation=1 source[2].group=composition-r296.source.2 source[2].real_frames=605 source[2].black_frames=0 source[2].contribution_generation=1
[2026-10-21 16:47:18.509] [info] [graph][runtime.node] cuda_canvas_call stage=context_push count=609 operation=copy maximum_ns=232800 begin_steady_ns=29019510060400 end_steady_ns=29019510293200 native_result=0 thread_cpu_ns=0
[2026-10-21 16:47:18.509] [info] [graph][runtime.node] cuda_canvas_call stage=source_wait count=2426 operation=copy maximum_ns=216600 begin_steady_ns=29016242846200 end_steady_ns=29016243062800 native_result=0 thread_cpu_ns=0
[2026-10-21 16:47:18.509] [info] [graph][runtime.node] cuda_canvas_call stage=plane_copy count=4854 operation=copy maximum_ns=1432100 begin_steady_ns=29007477188700 end_steady_ns=29007478620800 native_result=0 thread_cpu_ns=0
[2026-10-21 16:47:18.509] [info] [graph][runtime.node] cuda_canvas_call stage=destination_wait count=2 operation=upload maximum_ns=110500 begin_steady_ns=29001457297400 end_steady_ns=29001457407900 native_result=0 thread_cpu_ns=0
[2026-10-21 16:47:18.510] [info] [graph][runtime.node] cuda_canvas_call stage=context_pop count=609 operation=copy maximum_ns=151500 begin_steady_ns=29015143727100 end_steady_ns=29015143878600 native_result=0 thread_cpu_ns=0
[2026-10-21 16:47:18.510] [info] [graph][runtime.node] cuda_canvas_call stage=batch_completion count=607 operation=copy maximum_ns=4003100 begin_steady_ns=29020976145300 end_steady_ns=29020980148400 native_result=0 thread_cpu_ns=0
[2026-10-21 16:47:18.510] [info] [graph][runtime.node] canvas_compose_correlated scope=attempt begin_steady_ns=29020975902400 end_steady_ns=29020980163100 wall_ns=4260700 thread_cpu_ns=0 surface_count=1 surface_total_ns=1100 surface_maximum_ns=1100 surface_maximum_tile=none surface_maximum_result=unavailable validate_count=3 validate_total_ns=2700 validate_maximum_ns=1900 validate_maximum_tile=0 validate_maximum_result=0 black_count=1 black_total_ns=148200 black_maximum_ns=148200 black_maximum_tile=none black_maximum_result=0 tile_count=3 tile_total_ns=78700 tile_maximum_ns=35600 tile_maximum_tile=0 tile_maximum_result=0 publish_count=1 publish_total_ns=6400 publish_maximum_ns=6400 publish_maximum_tile=none publish_maximum_result=0 context_push_count=1 context_push_total_ns=4600 context_push_maximum_ns=4600 context_push_maximum_tile=none context_push_maximum_result=0 source_wait_count=4 source_wait_total_ns=4800 source_wait_maximum_ns=3500 source_wait_maximum_tile=0 source_wait_maximum_result=0 plane_copy_count=8 plane_copy_total_ns=198000 plane_copy_maximum_ns=110300 plane_copy_maximum_tile=none plane_copy_maximum_result=0 destination_wait_count=0 destination_wait_total_ns=0 destination_wait_maximum_ns=0 destination_wait_maximum_tile=none destination_wait_maximum_result=unavailable context_pop_count=1 context_pop_total_ns=4900 context_pop_maximum_ns=4900 context_pop_maximum_tile=none context_pop_maximum_result=0 batch_completion_count=1 batch_completion_total_ns=4001000 batch_completion_maximum_ns=4001000 batch_completion_maximum_tile=none batch_completion_maximum_result=0 source_query_completed_count=0 source_query_ready_count=3 source_query_ready_maximum_ns=8400 source_query_ready_begin_steady_ns=29020976066300 source_query_ready_end_steady_ns=29020976074700 source_query_ready_tile=0 source_query_ready_operation=event_query source_query_ready_native=0 source_query_ready_restore_native=0 source_query_pending_count=0 source_query_unavailable_count=0 source_query_error_count=0
[2026-10-21 16:47:18.510] [info] [graph][runtime.lifecycle] node_abort_payload node=6 before_objects=777 after_objects=747 before_bytes=13660611 after_bytes=13660611 before_reservations=25472 after_reservations=25472 before_releases=24695 after_releases=24725
[2026-10-21 16:47:18.511] [info] [graph][runtime.lifecycle] node_abort_payload node=67 before_objects=747 after_objects=746 before_bytes=13660611 after_bytes=13660611 before_reservations=25472 after_reservations=25472 before_releases=24725 after_releases=24726
[2026-10-21 16:47:18.511] [info] [graph][runtime.lifecycle] node_abort_payload node=44 before_objects=746 after_objects=745 before_bytes=13660611 after_bytes=13660611 before_reservations=25472 after_reservations=25472 before_releases=24726 after_releases=24727
[2026-10-21 16:47:18.511] [info] [graph][runtime.lifecycle] node_abort_payload node=25 before_objects=745 after_objects=743 before_bytes=13660611 after_bytes=13660611 before_reservations=25472 after_reservations=25472 before_releases=24727 after_releases=24729
[2026-10-21 16:47:18.511] [info] [graph][runtime.lifecycle] codec_owner_release node=73 kind=AudioResample owner_references=1
[2026-10-21 16:47:18.512] [info] [graph][runtime.lifecycle] node_abort_payload node=73 before_objects=743 after_objects=742 before_bytes=13660611 after_bytes=13652419 before_reservations=25472 after_reservations=25472 before_releases=24729 after_releases=24730
[2026-10-21 16:47:18.516] [info] [graph][runtime.lifecycle] node_abort_payload node=68 before_objects=742 after_objects=740 before_bytes=13652419 after_bytes=13652419 before_reservations=25472 after_reservations=25472 before_releases=24730 after_releases=24732
[2026-10-21 16:47:18.520] [info] [graph][runtime.lifecycle] node_abort_payload node=45 before_objects=740 after_objects=739 before_bytes=13652419 after_bytes=13652419 before_reservations=25472 after_reservations=25472 before_releases=24732 after_releases=24733
[2026-10-21 16:47:18.523] [info] [graph][runtime.lifecycle] node_abort_payload node=26 before_objects=739 after_objects=738 before_bytes=13652419 after_bytes=13652419 before_reservations=25472 after_reservations=25472 before_releases=24733 after_releases=24734
[2026-10-21 16:47:18.523] [info] [graph][runtime.lifecycle] node_abort_payload node=72 before_objects=738 after_objects=738 before_bytes=13652419 after_bytes=13652419 before_reservations=25472 after_reservations=25472 before_releases=24734 after_releases=24734
[2026-10-21 16:47:18.524] [info] [graph][runtime.lifecycle] node_abort_payload node=66 before_objects=738 after_objects=737 before_bytes=13652419 after_bytes=13652419 before_reservations=25472 after_reservations=25472 before_releases=24734 after_releases=24735
[2026-10-21 16:47:18.524] [info] [graph][runtime.lifecycle] node_abort_payload node=43 before_objects=737 after_objects=736 before_bytes=13652419 after_bytes=13652419 before_reservations=25472 after_reservations=25472 before_releases=24735 after_releases=24736
[2026-10-21 16:47:18.524] [info] [graph][runtime.lifecycle] node_abort_payload node=24 before_objects=736 after_objects=735 before_bytes=13652419 after_bytes=13652419 before_reservations=25472 after_reservations=25472 before_releases=24736 after_releases=24737
[2026-10-21 16:47:18.525] [info] [graph][runtime.lifecycle] node_abort_payload node=71 before_objects=735 after_objects=735 before_bytes=13652419 after_bytes=13652419 before_reservations=25472 after_reservations=25472 before_releases=24737 after_releases=24737
[2026-10-21 16:47:18.525] [info] [graph][runtime.lifecycle] node_abort_payload node=65 before_objects=735 after_objects=734 before_bytes=13652419 after_bytes=13652419 before_reservations=25472 after_reservations=25472 before_releases=24737 after_releases=24738
[2026-10-21 16:47:18.525] [info] [graph][runtime.lifecycle] node_abort_payload node=42 before_objects=734 after_objects=733 before_bytes=13652419 after_bytes=13652419 before_reservations=25472 after_reservations=25472 before_releases=24738 after_releases=24739
[2026-10-21 16:47:18.525] [info] [graph][runtime.lifecycle] node_abort_payload node=23 before_objects=733 after_objects=732 before_bytes=13652419 after_bytes=13652419 before_reservations=25472 after_reservations=25472 before_releases=24739 after_releases=24740
[2026-10-21 16:47:18.525] [info] [graph][runtime.lifecycle] codec_owner_release node=70 kind=AudioDecode owner_references=2
[2026-10-21 16:47:18.526] [info] [graph][runtime.lifecycle] node_abort_payload node=70 before_objects=732 after_objects=731 before_bytes=13652419 after_bytes=13644227 before_reservations=25472 after_reservations=25472 before_releases=24740 after_releases=24741
[2026-10-21 16:47:18.526] [info] [graph][runtime.lifecycle] codec_owner_release node=64 kind=VideoDecode owner_references=2
[2026-10-21 16:47:18.534] [info] [graph][runtime.lifecycle] node_abort_payload node=64 before_objects=731 after_objects=730 before_bytes=13644227 after_bytes=13644227 before_reservations=25472 after_reservations=25472 before_releases=24741 after_releases=24742
[2026-10-21 16:47:18.534] [info] [graph][runtime.lifecycle] codec_owner_release node=41 kind=VideoDecode owner_references=2
[2026-10-21 16:47:18.543] [info] [graph][runtime.lifecycle] node_abort_payload node=41 before_objects=730 after_objects=729 before_bytes=13644227 after_bytes=13644227 before_reservations=25472 after_reservations=25472 before_releases=24742 after_releases=24743
[2026-10-21 16:47:18.544] [info] [graph][runtime.lifecycle] codec_owner_release node=22 kind=VideoDecode owner_references=2
[2026-10-21 16:47:18.552] [info] [graph][runtime.lifecycle] node_abort_payload node=22 before_objects=729 after_objects=728 before_bytes=13644227 after_bytes=13644227 before_reservations=25472 after_reservations=25472 before_releases=24743 after_releases=24744
[2026-10-21 16:47:18.553] [info] [graph][runtime.lifecycle] node_abort_payload node=62 before_objects=728 after_objects=728 before_bytes=13644227 after_bytes=13644227 before_reservations=25472 after_reservations=25472 before_releases=24744 after_releases=24744
[2026-10-21 16:47:18.553] [info] [graph][runtime.lifecycle] node_abort_payload node=39 before_objects=728 after_objects=728 before_bytes=13644227 after_bytes=13644227 before_reservations=25472 after_reservations=25472 before_releases=24744 after_releases=24744
[2026-10-21 16:47:18.553] [info] [graph][runtime.lifecycle] node_abort_payload node=20 before_objects=728 after_objects=728 before_bytes=13644227 after_bytes=13644227 before_reservations=25472 after_reservations=25472 before_releases=24744 after_releases=24744
[2026-10-21 16:47:18.553] [info] [graph][runtime.lifecycle] node_abort_payload node=61 before_objects=728 after_objects=728 before_bytes=13644227 after_bytes=13644227 before_reservations=25472 after_reservations=25472 before_releases=24744 after_releases=24744
[2026-10-21 16:47:18.553] [info] [graph][runtime.lifecycle] node_abort_payload node=38 before_objects=728 after_objects=728 before_bytes=13644227 after_bytes=13644227 before_reservations=25472 after_reservations=25472 before_releases=24744 after_releases=24744
[2026-10-21 16:47:18.553] [info] [graph][runtime.lifecycle] node_abort_payload node=19 before_objects=728 after_objects=728 before_bytes=13644227 after_bytes=13644227 before_reservations=25472 after_reservations=25472 before_releases=24744 after_releases=24744
[2026-10-21 16:47:18.554] [info] [graph][runtime.lifecycle] node_abort_payload node=60 before_objects=728 after_objects=728 before_bytes=13644227 after_bytes=13644227 before_reservations=25472 after_reservations=25472 before_releases=24744 after_releases=24744
[2026-10-21 16:47:18.554] [info] [graph][runtime.lifecycle] node_abort_payload node=37 before_objects=728 after_objects=728 before_bytes=13644227 after_bytes=13644227 before_reservations=25472 after_reservations=25472 before_releases=24744 after_releases=24744
[2026-10-21 16:47:18.555] [info] [graph][runtime.lifecycle] node_abort_payload node=18 before_objects=728 after_objects=728 before_bytes=13644227 after_bytes=13644227 before_reservations=25472 after_reservations=25472 before_releases=24744 after_releases=24744
[2026-10-21 16:47:18.555] [info] [graph][runtime.lifecycle] node_abort_payload node=58 before_objects=728 after_objects=728 before_bytes=13644227 after_bytes=13644227 before_reservations=25472 after_reservations=25472 before_releases=24744 after_releases=24744
[2026-10-21 16:47:18.555] [info] [graph][runtime.lifecycle] node_abort_payload node=35 before_objects=728 after_objects=728 before_bytes=13644227 after_bytes=13644227 before_reservations=25472 after_reservations=25472 before_releases=24744 after_releases=24744
[2026-10-21 16:47:18.556] [info] [graph][runtime.lifecycle] node_abort_payload node=16 before_objects=728 after_objects=728 before_bytes=13644227 after_bytes=13644227 before_reservations=25472 after_reservations=25472 before_releases=24744 after_releases=24744
[2026-10-21 16:47:18.556] [info] [graph][runtime.lifecycle] node_abort_payload node=57 before_objects=728 after_objects=727 before_bytes=13644227 after_bytes=13643272 before_reservations=25472 after_reservations=25472 before_releases=24744 after_releases=24745
[2026-10-21 16:47:18.556] [info] [graph][runtime.lifecycle] node_abort_payload node=56 before_objects=727 after_objects=727 before_bytes=13643272 after_bytes=13643272 before_reservations=25472 after_reservations=25472 before_releases=24745 after_releases=24745
[2026-10-21 16:47:18.556] [info] [graph][runtime.lifecycle] node_abort_payload node=34 before_objects=727 after_objects=727 before_bytes=13643272 after_bytes=13643272 before_reservations=25472 after_reservations=25472 before_releases=24745 after_releases=24745
[2026-10-21 16:47:18.556] [info] [graph][runtime.lifecycle] node_abort_payload node=15 before_objects=727 after_objects=727 before_bytes=13643272 after_bytes=13643272 before_reservations=25472 after_reservations=25472 before_releases=24745 after_releases=24745
[2026-10-21 16:47:18.556] [info] [graph][runtime.lifecycle] node_abort_payload node=59 before_objects=727 after_objects=727 before_bytes=13643272 after_bytes=13643272 before_reservations=25472 after_reservations=25472 before_releases=24745 after_releases=24745
[2026-10-21 16:47:18.557] [info] [graph][runtime.lifecycle] node_abort_payload node=55 before_objects=727 after_objects=727 before_bytes=13643272 after_bytes=13643272 before_reservations=25472 after_reservations=25472 before_releases=24745 after_releases=24745
[2026-10-21 16:47:18.557] [info] [graph][runtime.lifecycle] node_abort_payload node=54 before_objects=727 after_objects=727 before_bytes=13643272 after_bytes=13643272 before_reservations=25472 after_reservations=25472 before_releases=24745 after_releases=24745
[2026-10-21 16:47:18.557] [info] [graph][runtime.lifecycle] node_abort_payload node=36 before_objects=727 after_objects=727 before_bytes=13643272 after_bytes=13643272 before_reservations=25472 after_reservations=25472 before_releases=24745 after_releases=24745
[2026-10-21 16:47:18.557] [info] [graph][runtime.lifecycle] node_abort_payload node=33 before_objects=727 after_objects=727 before_bytes=13643272 after_bytes=13643272 before_reservations=25472 after_reservations=25472 before_releases=24745 after_releases=24745
[2026-10-21 16:47:18.558] [info] [graph][runtime.lifecycle] node_abort_payload node=17 before_objects=727 after_objects=727 before_bytes=13643272 after_bytes=13643272 before_reservations=25472 after_reservations=25472 before_releases=24745 after_releases=24745
[2026-10-21 16:47:18.558] [info] [graph][runtime.lifecycle] node_abort_payload node=14 before_objects=727 after_objects=727 before_bytes=13643272 after_bytes=13643272 before_reservations=25472 after_reservations=25472 before_releases=24745 after_releases=24745
[2026-10-21 16:47:18.558] [info] [graph][runtime.lifecycle] node_abort_payload node=53 before_objects=727 after_objects=727 before_bytes=13643272 after_bytes=13643272 before_reservations=25472 after_reservations=25472 before_releases=24745 after_releases=24745
[2026-10-21 16:47:18.558] [info] [graph][runtime.lifecycle] node_abort_payload node=32 before_objects=727 after_objects=727 before_bytes=13643272 after_bytes=13643272 before_reservations=25472 after_reservations=25472 before_releases=24745 after_releases=24745
[2026-10-21 16:47:18.558] [info] [graph][runtime.lifecycle] node_abort_payload node=13 before_objects=727 after_objects=727 before_bytes=13643272 after_bytes=13643272 before_reservations=25472 after_reservations=25472 before_releases=24745 after_releases=24745
[2026-10-21 16:47:18.558] [info] [graph][runtime.lifecycle] node_abort_payload node=52 before_objects=727 after_objects=727 before_bytes=13643272 after_bytes=13643272 before_reservations=25472 after_reservations=25472 before_releases=24745 after_releases=24745
[2026-10-21 16:47:18.559] [info] [graph][runtime.lifecycle] node_abort_payload node=51 before_objects=727 after_objects=727 before_bytes=13643272 after_bytes=13643272 before_reservations=25472 after_reservations=25472 before_releases=24745 after_releases=24745
[2026-10-21 16:47:18.559] [info] [graph][runtime.lifecycle] node_abort_payload node=50 before_objects=727 after_objects=726 before_bytes=13643272 after_bytes=13509297 before_reservations=25472 after_reservations=25472 before_releases=24745 after_releases=24746
[2026-10-21 16:47:18.559] [info] [graph][runtime.lifecycle] node_abort_payload node=31 before_objects=726 after_objects=726 before_bytes=13509297 after_bytes=13509297 before_reservations=25472 after_reservations=25472 before_releases=24746 after_releases=24746
[2026-10-21 16:47:18.559] [info] [graph][runtime.lifecycle] node_abort_payload node=30 before_objects=726 after_objects=725 before_bytes=13509297 after_bytes=13477941 before_reservations=25472 after_reservations=25472 before_releases=24746 after_releases=24747
[2026-10-21 16:47:18.559] [info] [graph][runtime.lifecycle] node_abort_payload node=12 before_objects=725 after_objects=725 before_bytes=13477941 after_bytes=13477941 before_reservations=25472 after_reservations=25472 before_releases=24747 after_releases=24747
[2026-10-21 16:47:18.559] [info] [graph][runtime.lifecycle] node_abort_payload node=11 before_objects=725 after_objects=724 before_bytes=13477941 after_bytes=13446585 before_reservations=25472 after_reservations=25472 before_releases=24747 after_releases=24748
[2026-10-21 16:47:18.559] [info] [graph][runtime.lifecycle] node_abort_payload node=49 before_objects=724 after_objects=724 before_bytes=13446585 after_bytes=13446585 before_reservations=25472 after_reservations=25472 before_releases=24748 after_releases=24748
[2026-10-21 16:47:18.560] [info] [graph][runtime.lifecycle] node_abort_payload node=29 before_objects=724 after_objects=724 before_bytes=13446585 after_bytes=13446585 before_reservations=25472 after_reservations=25472 before_releases=24748 after_releases=24748
[2026-10-21 16:47:18.560] [info] [graph][runtime.lifecycle] node_abort_payload node=10 before_objects=724 after_objects=724 before_bytes=13446585 after_bytes=13446585 before_reservations=25472 after_reservations=25472 before_releases=24748 after_releases=24748
[2026-10-21 16:47:18.560] [info] [graph][runtime.lifecycle] node_abort_payload node=69 before_objects=724 after_objects=724 before_bytes=13446585 after_bytes=13446585 before_reservations=25472 after_reservations=25472 before_releases=24748 after_releases=24748
[2026-10-21 16:47:18.560] [info] [graph][runtime.lifecycle] node_abort_payload node=48 before_objects=724 after_objects=724 before_bytes=13446585 after_bytes=13446585 before_reservations=25472 after_reservations=25472 before_releases=24748 after_releases=24748
[2026-10-21 16:47:18.560] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-10-21 16:47:18.560] [info] [graph][runtime.lifecycle] node_abort_payload node=63 before_objects=724 after_objects=724 before_bytes=13446585 after_bytes=13446585 before_reservations=25472 after_reservations=25472 before_releases=24748 after_releases=24748
[2026-10-21 16:47:18.560] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-10-21 16:47:18.560] [info] [graph][runtime.lifecycle] node_abort_payload node=40 before_objects=724 after_objects=724 before_bytes=13446585 after_bytes=13446585 before_reservations=25472 after_reservations=25472 before_releases=24748 after_releases=24748
[2026-10-21 16:47:18.560] [info] [graph][runtime.lifecycle] node_abort_payload node=28 before_objects=724 after_objects=724 before_bytes=13446585 after_bytes=13446585 before_reservations=25472 after_reservations=25472 before_releases=24748 after_releases=24748
[2026-10-21 16:47:18.561] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-10-21 16:47:18.561] [info] [graph][runtime.lifecycle] node_abort_payload node=21 before_objects=724 after_objects=724 before_bytes=13446585 after_bytes=13446585 before_reservations=25472 after_reservations=25472 before_releases=24748 after_releases=24748
[2026-10-21 16:47:18.561] [info] [graph][runtime.lifecycle] node_abort_payload node=9 before_objects=724 after_objects=724 before_bytes=13446585 after_bytes=13446585 before_reservations=25472 after_reservations=25472 before_releases=24748 after_releases=24748
[2026-10-21 16:47:18.561] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=1079 completion_waits=1079 completed_datagrams=1116 drained_datagrams=37 would_block=0 cancellations=0 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-21 16:47:18.561] [info] [graph][runtime.lifecycle] node_abort_payload node=47 before_objects=724 after_objects=724 before_bytes=13446585 after_bytes=13446585 before_reservations=25472 after_reservations=25472 before_releases=24748 after_releases=24748
[2026-10-21 16:47:18.562] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=13369 completion_waits=13369 completed_datagrams=22437 drained_datagrams=9069 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-21 16:47:18.562] [info] [graph][runtime.lifecycle] node_abort_payload node=46 before_objects=724 after_objects=724 before_bytes=13446585 after_bytes=13446585 before_reservations=25472 after_reservations=25472 before_releases=24748 after_releases=24748
[2026-10-21 16:47:18.562] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=15104 completion_waits=15104 completed_datagrams=23183 drained_datagrams=8080 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-21 16:47:18.563] [info] [graph][runtime.lifecycle] node_abort_payload node=27 before_objects=724 after_objects=724 before_bytes=13446585 after_bytes=13446585 before_reservations=25472 after_reservations=25472 before_releases=24748 after_releases=24748
[2026-10-21 16:47:18.563] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=15477 completion_waits=15477 completed_datagrams=22964 drained_datagrams=7487 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-21 16:47:18.564] [info] [graph][runtime.lifecycle] node_abort_payload node=8 before_objects=724 after_objects=724 before_bytes=13446585 after_bytes=13446585 before_reservations=25472 after_reservations=25472 before_releases=24748 after_releases=24748
[2026-10-21 16:47:18.564] [info] [graph][runtime.lifecycle] node_abort_payload node=3 before_objects=724 after_objects=724 before_bytes=13446585 after_bytes=13446585 before_reservations=25472 after_reservations=25472 before_releases=24748 after_releases=24748
[2026-10-21 16:47:18.564] [info] [graph][runtime.node] codec_resolver.release slot=encoder_parameters bufferReferences=1 hasCredit=0 creditReferences=0
[2026-10-21 16:47:18.564] [info] [graph][runtime.lifecycle] node_abort_payload node=1 before_objects=724 after_objects=724 before_bytes=13446585 after_bytes=13446585 before_reservations=25472 after_reservations=25472 before_releases=24748 after_releases=24748
[2026-10-21 16:47:18.659] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=failed detail="scheduled datagram submit exceeded its original deadline generation=1 sequence=14492 endpoint=1 release_ns=25527026084 reserved_at_ns=25507841800 not_before_ns=25527026084 deadline_ns=25626901284 observed_at_ns=26071760200" description="" error=6 native=0 stage=3 reason="scheduled datagram submit exceeded its original deadline generation=1 sequence=14492 endpoint=1 release_ns=25527026084 reserved_at_ns=25507841800 not_before_ns=25527026084 deadline_ns=25626901284 observed_at_ns=26071760200"
[2026-10-21 16:47:18.660] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=retired detail="scheduled datagram submit exceeded its original deadline generation=1 sequence=14492 endpoint=1 release_ns=25527026084 reserved_at_ns=25507841800 not_before_ns=25527026084 deadline_ns=25626901284 observed_at_ns=26071760200" description="" error=6 native=0 stage=3 reason="scheduled datagram submit exceeded its original deadline generation=1 sequence=14492 endpoint=1 release_ns=25527026084 reserved_at_ns=25507841800 not_before_ns=25527026084 deadline_ns=25626901284 observed_at_ns=26071760200"
[CLI] final runtime report: queued=0, peakQueued=958, threads=81, processThreads=58, workers=0, workerProgress=60339, workerProcessCalls=225176, workerWaits=164821, workerWakeups=163221, workerErrors=3, errors=3, stalledIntervals=0, cpuSamples=63, logicalProcessors=22, averageSystemMachineCpuPercent=10.177446, averageProcessMachineCpuPercent=2.085197, peakProcessMachineCpuPercent=6.378601, averageProcessSingleCoreCpuPercent=45.874331, peakProcessSingleCoreCpuPercent=140.329218, initialWorkingSetBytes=209387520, workingSetBytes=265359360, peakWorkingSetBytes=265359360, totalPushed=55771, totalPopped=55064, droppedBuffers=0, encodedPacketsPushed=6735, encodedPacketsPopped=6734, backpressureItems=122, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=18737958, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=788, graphPayloadReservations=25472, graphPayloadReleases=25472, graphPayloadPressureFailures=1, lastInputReceivedAtNs=29022349717500
ModLoad: 00007ff9`027a0000 00007ff9`02c27000   C:\WINDOWS\System32\SETUPAPI.dll
ModLoad: 00007ff9`01e00000 00007ff9`01ed9000   C:\WINDOWS\System32\OLEAUT32.dll
ModLoad: 00007ff8`ccf20000 00007ff8`ccfcf000   C:\WINDOWS\System32\DriverStore\FileRepository\nvami.inf_amd64_97bcbdbdb2ab507c\nvdxgdmal64.dll
ModLoad: 00007ff8`cbed0000 00007ff8`cccd2000   C:\WINDOWS\System32\DriverStore\FileRepository\nvami.inf_amd64_97bcbdbdb2ab507c\nvobjectloader64.dll
ModLoad: 00007fff`88df0000 00007fff`8aca3000   C:\WINDOWS\SYSTEM32\nvcuvid.dll
ModLoad: 00007ff9`00870000 00007ff9`008f2000   C:\WINDOWS\System32\WINTRUST.dll
ModLoad: 00007ff9`02150000 00007ff9`0215a000   C:\WINDOWS\System32\NSI.dll
ModLoad: 00007fff`b1610000 00007fff`b1715000   C:\WINDOWS\SYSTEM32\nvEncodeAPI64.dll
(a164.9eec): Break instruction exception - code 80000003 (first chance)
ntdll!DbgBreakPoint:
00007ff9`03083f40 cc              int     3
0:131> sxe -c ".exr -1;kb;gn" eh;sxe -c2 ".exr -1;.ecxr;~* kb;.dump /ma D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r296-crash.dmp;qd" 0xc0000409;g
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
[in @ 0000024a95a37200] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000024a95a37200] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 27049.514011
[in @ 0000024aadbc5e40] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000024aadbc5e40] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 431.419133
[in @ 0000024aadb9fd40] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000024aadb9fd40] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 27020.721522
[in @ 0000024a81088880] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000024ab30cf800] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000024ab30cf800] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 27049.514011
[in @ 0000024a81088880] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 27020.721522
[in @ 0000024a81088540] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000024a81088540] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 431.419133
[aac @ 0000024aadca4a80] Qavg: 596.514
[aac @ 0000024aadca4a80] 2 frames left in the queue on closing
[CLI] realtime video runtime failed: IoFailure: scheduled datagram submit exceeded its original deadline generation=1 sequence=14492 endpoint=1 release_ns=25527026084 reserved_at_ns=25507841800 not_before_ns=25527026084 deadline_ns=25626901284 observed_at_ns=26071760200
~~~~

## 清理清单

composition-r296-cdb.log
composition-r296-cli-error.log
composition-r296-cli.log
composition-r296-monitor.log
composition-r296-source0-stdout.log
composition-r296-source0.log
composition-r296-source1-stdout.log
composition-r296-source1.log
composition-r296-source2-stdout.log
composition-r296-source2.log
composition-r296-vlc.log
