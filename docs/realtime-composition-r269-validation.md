# r269：线程枚举错误明细复验（失败）

Release全量重建19965成功，configure/build退出0，四项产物齐全；realtime6195712B，2026-10-19 16:00:44，SHA256 4C80B318E999EB24AD554FD3BE4117A06A5E8EA22BDFD56A98E8664F61440BAA。未运行local。

预期原规格三路H264 720p30约8Mbps、AAC44100双声道；合屏HEVC720p30 8000kbps CBR、AAC192kbps CBR经MPEGTS/RTP。实际仅启动阶段，未达到画面/失源/重入验收。

会话35708，VLC14944，源44240/43476/43108，profiler14264，CLI11108。CLI与profiler退出1；失败后主动清理源，三源-1，不作自然结束。VLC RC quit，无有效快照。

确定错误：Thread32Next failed native=0 entry_size=28 enumerated_threads=6978 process_threads=97。正常结束应是ERROR_NO_MORE_FILES，不放宽校验。最终errors=1、workerErrors=0，payload bytes/objects=0，reserve/release=370；无CPU有效样本，0不是实测CPU。显式show-output=true仍未将目标日志转发至重定向文件，完整可用日志从报告ProcessStreams提取。未证明Nsight导致API异常，更未修复r267 CUDA170ms等待。

## 实际命令
~~~powershell
$ErrorActionPreference='Stop'
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r269-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r269-native- --snapshot-format=png rtp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r269-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r269-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r269-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r269-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r269-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r269-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$profiler=Start-Process -FilePath 'C:/Program Files/NVIDIA Corporation/Nsight Systems 2024.6.2/target-windows-x64/nsys.exe' -ArgumentList 'profile --show-output=true --trace=cuda --sample=none --cpuctxsw=none --kill=false --output=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r269-cuda D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-r269 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r269-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r269-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r269-cli-error.log' -PassThru
Write-Output "PROFILER_PID=$($profiler.Id)"
$child=$null
while(-not $child -and -not $profiler.HasExited) {
$child=Get-CimInstance Win32_Process -Filter "Name='media_transcode_realtime_video_cli.exe'" | Where-Object {$_.CommandLine -like '*composition-r269*'} | Select-Object -First 1
if(-not $child){Start-Sleep -Milliseconds 100}
}
if(-not $child){throw 'Profiler did not create realtime CLI'}
$cli=Get-Process -Id $child.ProcessId
$null=$cli.Handle
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
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r269-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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
$profiler.WaitForExit()
Write-Output "PROFILER_EXIT=$($profiler.ExitCode)"
exit $profiler.ExitCode
~~~

### Captured C:/Program Files/NVIDIA Corporation/Nsight Systems 2024.6.2/target-windows-x64/config.ini
~~~text

~~~

### Captured C:/Users/ROG/AppData/Local/Temp/nvidia/nsight_systems/quadd_session_1031940/streams/stdout_ac88-6a42-a397-9bc8.log
~~~text
[CLI] composition_inputs=3 grid=fixed audio_source=2 audio=on max_duration=source_driven hw=planner-highest-score
[2026-10-19 16:01:25.950] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-19 16:01:25.950] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-19 16:01:28.253] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-10-19 16:01:28.253] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-10-19 16:01:28.253] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-10-19 16:01:28.302] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-19 16:01:28.302] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-19 16:01:28.346] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-19 16:01:28.346] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-19 16:01:28.415] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-10-19 16:01:28.448] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-10-19 16:01:28.448] [info] [graph][planner.capability] canvas_cuda_stream stage=prepared device_stream=0000000000000000 private_nonblocking=1
[2026-10-19 16:01:28.455] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-19 16:01:28.455] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-19 16:01:28.455] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-10-19 16:01:28.460] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-19 16:01:28.460] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-19 16:01:28.460] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-10-19 16:01:28.465] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-19 16:01:28.465] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-19 16:01:28.465] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-10-19 16:01:28.481] [info] [graph][planner.capability] composition_preparation_storage stage=graph_bound scope=shared_reservations capacity=2147483648 retained=841079518 high_water=849804133
[CLI] audio_plan branch=transcode_frame reason=transcode_required_by_output_frame_contract codec=aac sample_rate=44100 channels=2 access_unit_samples=1024 encoder=aac bitrate_kbps=192
[CLI] prepared_output_id=1
[CLI] selected_chain=fixed-grid-composition score=0 decoder=h264,h264,h264 filter=scale_cuda=640:360,scale_cuda=640:360,scale_cuda=640:360 encoder=hevc_nvenc
[2026-10-19 16:01:28.481] [info] [graph][runtime.lifecycle] compile.begin
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=1 edge=1 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=2 edge=2 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=3 edge=3 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=4 edge=4 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=5 edge=5 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=6 edge=6 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=7 edge=7 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=8 edge=8 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=9 edge=9 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=10 edge=10 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=11 edge=11 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=12 edge=12 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=13 edge=13 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=14 edge=14 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=15 edge=15 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=16 edge=16 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=17 edge=17 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=18 edge=18 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=19 edge=19 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=20 edge=20 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=21 edge=21 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=22 edge=22 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=23 edge=23 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=24 edge=24 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=25 edge=25 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=26 edge=26 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=27 edge=27 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=28 edge=28 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=29 edge=29 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=30 edge=30 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=31 edge=31 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=32 edge=32 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=33 edge=33 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=34 edge=34 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=35 edge=35 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=36 edge=36 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=37 edge=37 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=38 edge=38 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=39 edge=39 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=40 edge=40 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=41 edge=41 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=42 edge=42 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=43 edge=43 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=44 edge=44 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=45 edge=45 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=46 edge=46 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=47 edge=47 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=48 edge=48 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=49 edge=49 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=50 edge=50 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=51 edge=51 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=52 edge=52 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=53 edge=53 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=54 edge=54 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=55 edge=55 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=56 edge=56 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=57 edge=57 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=58 edge=58 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=59 edge=59 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=60 edge=60 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=61 edge=61 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=62 edge=62 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=63 edge=63 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=64 edge=64 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=65 edge=65 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=66 edge=66 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=67 edge=67 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=68 edge=68 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=69 edge=69 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=70 edge=70 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=71 edge=71 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=72 edge=72 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=73 edge=73 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=74 edge=74 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=75 edge=75 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=76 edge=76 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=77 edge=77 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=78 edge=78 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=79 edge=79 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=80 edge=80 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=81 edge=81 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=82 edge=82 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=83 edge=83 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=84 edge=84 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=85 edge=85 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=86 edge=86 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=87 edge=87 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=88 edge=88 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=89 edge=89 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=90 edge=90 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=91 edge=91 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=92 edge=92 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=93 edge=93 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=94 edge=94 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=95 edge=95 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=96 edge=96 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=97 edge=97 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=98 edge=98 stream=Audio edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=99 edge=99 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=100 edge=100 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=101 edge=101 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=102 edge=102 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=103 edge=103 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=104 edge=104 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=105 edge=105 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=106 edge=106 stream=Video edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=107 edge=107 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=108 edge=108 stream=Any edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=109 edge=109 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=110 edge=110 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=111 edge=111 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=112 edge=112 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=113 edge=113 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=114 edge=114 stream=Any edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=115 edge=115 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=116 edge=116 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=117 edge=117 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=118 edge=118 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=119 edge=119 stream=Any edge_kind=EncodedPacket payload=Unknown queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=120 edge=120 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=121 edge=121 stream=Metadata edge_kind=Unknown payload=Unknown queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=122 edge=122 stream=Metadata edge_kind=Unknown payload=Unknown queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.channel] create channel=123 edge=123 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-19 16:01:28.482] [info] [graph][runtime.lifecycle] compiled nodes=82 edges=123 channels=123 execution_order=1->3->8->27->46->47->9->21->28->40->63->48->69->10->29->49->11->12->30->31->50->51->52->13->32->53->14->17->33->36->54->55->59->15->34->56->57->16->35->58->18->37->60->19->38->61->20->39->62->22->41->64->70->23->42->65->71->24->43->66->72->26->45->68->73->25->44->67->6->2->7->81->78->4->77->5->74->75->79->76->80->82
[2026-10-19 16:01:28.482] [info] [graph][runtime.lifecycle] compile.done state=DefaultRegistrationPending
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=1 name=composition-r269.output.video.codec_resolver kind=CodecResolver
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=2 name=composition-r269.output.video.encode kind=VideoEncode
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=3 name=composition-r269.output.audio.codec_resolver kind=AudioCodecResolver
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=4 name=composition-r269.output.audio.encode kind=AudioEncode
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=5 name=composition-r269.output.audio.canonicalizer kind=EncodedAudioCanonicalizer
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=8 name=composition-r269.source_0.input kind=RawRtpInput
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=9 name=composition-r269.source_0.rtp.clock_group kind=RtpClockGroup
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=10 name=composition-r269.source_0.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=11 name=composition-r269.source_0.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=12 name=composition-r269.source_0.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=13 name=composition-r269.source_0.av_sync.source_clock kind=SourceClockStateFanout
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=14 name=composition-r269.source_0.av_sync.video.generation_gate kind=LockedPacketGate
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=15 name=composition-r269.source_0.av_sync.video.canonical_input kind=CanonicalInput
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=16 name=composition-r269.source_0.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=17 name=composition-r269.source_0.av_sync.startup.clock kind=AvStartupClock
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=18 name=composition-r269.source_0.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=20 name=composition-r269.source_0.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=21 name=composition-r269.source_0.video.codec_resolver kind=CodecResolver
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=22 name=composition-r269.source_0.video.decode kind=VideoDecode
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=23 name=composition-r269.source_0.video.hwtransfer kind=HardwareTransfer
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=24 name=composition-r269.source_0.video.framerate kind=VideoFrameRate
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=25 name=composition-r269.source_0.video.preparation kind=VideoPreparation
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=26 name=composition-r269.source_0.video.filter kind=VideoFilter
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=27 name=composition-r269.source_1.input kind=RawRtpInput
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=28 name=composition-r269.source_1.rtp.clock_group kind=RtpClockGroup
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=29 name=composition-r269.source_1.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=30 name=composition-r269.source_1.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=31 name=composition-r269.source_1.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=32 name=composition-r269.source_1.av_sync.source_clock kind=SourceClockStateFanout
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=33 name=composition-r269.source_1.av_sync.video.generation_gate kind=LockedPacketGate
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=34 name=composition-r269.source_1.av_sync.video.canonical_input kind=CanonicalInput
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=35 name=composition-r269.source_1.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=36 name=composition-r269.source_1.av_sync.startup.clock kind=AvStartupClock
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=37 name=composition-r269.source_1.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=39 name=composition-r269.source_1.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=40 name=composition-r269.source_1.video.codec_resolver kind=CodecResolver
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=41 name=composition-r269.source_1.video.decode kind=VideoDecode
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=42 name=composition-r269.source_1.video.hwtransfer kind=HardwareTransfer
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=43 name=composition-r269.source_1.video.framerate kind=VideoFrameRate
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=44 name=composition-r269.source_1.video.preparation kind=VideoPreparation
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=45 name=composition-r269.source_1.video.filter kind=VideoFilter
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=46 name=composition-r269.source_2.video.input kind=RawRtpInput
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=47 name=composition-r269.source_2.audio.input kind=RawRtpInput
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=48 name=composition-r269.source_2.rtp.clock_group kind=RtpClockGroup
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=49 name=composition-r269.source_2.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=50 name=composition-r269.source_2.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=51 name=composition-r269.source_2.av_sync.audio.protocol_binder kind=RtpPacketClockBinder
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=52 name=composition-r269.source_2.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=53 name=composition-r269.source_2.av_sync.source_clock kind=SourceClockStateFanout
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=54 name=composition-r269.source_2.av_sync.video.generation_gate kind=LockedPacketGate
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=55 name=composition-r269.source_2.av_sync.audio.generation_gate kind=LockedPacketGate
[2026-10-19 16:01:28.482] [info] [graph][runtime.node] register node=56 name=composition-r269.source_2.av_sync.video.canonical_input kind=CanonicalInput
[2026-10-19 16:01:28.483] [info] [graph][runtime.node] register node=57 name=composition-r269.source_2.av_sync.audio.canonical_input kind=CanonicalInput
[2026-10-19 16:01:28.483] [info] [graph][runtime.node] register node=58 name=composition-r269.source_2.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-10-19 16:01:28.483] [info] [graph][runtime.node] register node=59 name=composition-r269.source_2.av_sync.startup.clock kind=AvStartupClock
[2026-10-19 16:01:28.483] [info] [graph][runtime.node] register node=60 name=composition-r269.source_2.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-10-19 16:01:28.483] [info] [graph][runtime.node] register node=62 name=composition-r269.source_2.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-10-19 16:01:28.483] [info] [graph][runtime.node] register node=63 name=composition-r269.source_2.video.codec_resolver kind=CodecResolver
[2026-10-19 16:01:28.483] [info] [graph][runtime.node] register node=64 name=composition-r269.source_2.video.decode kind=VideoDecode
[2026-10-19 16:01:28.483] [info] [graph][runtime.node] register node=65 name=composition-r269.source_2.video.hwtransfer kind=HardwareTransfer
[2026-10-19 16:01:28.483] [info] [graph][runtime.node] register node=66 name=composition-r269.source_2.video.framerate kind=VideoFrameRate
[2026-10-19 16:01:28.483] [info] [graph][runtime.node] register node=67 name=composition-r269.source_2.video.preparation kind=VideoPreparation
[2026-10-19 16:01:28.483] [info] [graph][runtime.node] register node=68 name=composition-r269.source_2.video.filter kind=VideoFilter
[2026-10-19 16:01:28.483] [info] [graph][runtime.node] register node=69 name=composition-r269.source_2.audio.encode.codec_resolver kind=AudioCodecResolver
[2026-10-19 16:01:28.483] [info] [graph][runtime.node] register node=70 name=composition-r269.source_2.audio.encode.decode kind=AudioDecode
[2026-10-19 16:01:28.483] [info] [graph][runtime.node] register node=71 name=composition-r269.source_2.audio.encode.startup_trim kind=Unknown
[2026-10-19 16:01:28.483] [info] [graph][runtime.node] register node=72 name=composition-r269.source_2.audio.encode.drift_controller kind=AudioDriftController
[2026-10-19 16:01:28.483] [info] [graph][runtime.node] register node=73 name=composition-r269.source_2.audio.encode.resample kind=AudioResample
[2026-10-19 16:01:28.483] [info] [graph][runtime.node] register node=74 name=composition-r269.output.scheduler.scheduler kind=AvOutputScheduler
[2026-10-19 16:01:28.483] [info] [graph][runtime.node] register node=75 name=composition-r269.output.scheduler.router kind=ScheduledOutputRouter
[2026-10-19 16:01:28.483] [info] [graph][runtime.node] register node=76 name=composition-r269.output.mpegts.mux kind=FileMux
[2026-10-19 16:01:28.483] [info] [graph][runtime.node] register node=78 name=composition-r269.output.mpegts.plan.source kind=Unknown
[2026-10-19 16:01:28.483] [info] [graph][runtime.node] register node=79 name=composition-r269.output.mpegts.scheduled.adapter kind=Unknown
[2026-10-19 16:01:28.483] [info] [graph][runtime.node] register node=80 name=composition-r269.output.mpegts.datagram.materializer kind=MpegTsDatagramMaterializer
[2026-10-19 16:01:28.483] [info] [graph][runtime.node] register node=81 name=composition-r269.output.mpegts.transport.plan kind=DatagramTransportPlanSource
[2026-10-19 16:01:28.483] [info] [graph][runtime.node] register node=82 name=composition-r269.output.mpegts.transport.sender kind=ScheduledDatagramSender
[2026-10-19 16:01:28.483] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=waiting_for_random_access detail="" description="D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r269-output.sdp"
[2026-10-19 16:01:28.483] [info] [graph][runtime.lifecycle] start.begin mode=threaded
[2026-10-19 16:01:28.484] [info] [graph][runtime.lifecycle] worker.finished node=1
[2026-10-19 16:01:28.484] [info] [graph][runtime.node] worker.thread_cpu node=1 available=1 cpu_ns=0 process_calls=1
[2026-10-19 16:01:28.484] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:01:28.485] [info] [graph][runtime.lifecycle] start.done state=ThreadedRunning
[2026-10-19 16:01:28.486] [info] [graph][runtime.lifecycle] worker.finished node=3
[2026-10-19 16:01:28.486] [info] [graph][runtime.node] worker.thread_cpu node=3 available=1 cpu_ns=0 process_calls=1
[2026-10-19 16:01:28.486] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:01:28.487] [info] [graph][runtime.lifecycle] worker.finished node=69
[2026-10-19 16:01:28.487] [info] [graph][runtime.node] worker.thread_cpu node=69 available=1 cpu_ns=0 process_calls=2
[2026-10-19 16:01:28.487] [info] [graph][runtime.lifecycle] worker.finished node=21
[2026-10-19 16:01:28.487] [info] [graph][runtime.node] worker.thread_cpu node=21 available=1 cpu_ns=0 process_calls=1
[2026-10-19 16:01:28.487] [info] [graph][runtime.lifecycle] worker.finished node=40
[2026-10-19 16:01:28.487] [info] [graph][runtime.node] worker.thread_cpu node=40 available=1 cpu_ns=0 process_calls=1
[2026-10-19 16:01:28.487] [info] [graph][runtime.lifecycle] worker.finished node=63
[2026-10-19 16:01:28.487] [info] [graph][runtime.node] worker.thread_cpu node=63 available=1 cpu_ns=0 process_calls=1
[2026-10-19 16:01:28.488] [info] [graph][runtime.node] video_encode.bind_encoder codec=hevc_nvenc pix_fmt=cuda rate_control=cbr bitrate=8000000 min_bitrate=8000000 max_bitrate=8000000 buffer_size=267000 fps=30/1 gop=60 frame_kind=hardware hwaccel=cuda hw_device_ctx=set hw_frames_ctx=set
[2026-10-19 16:01:28.488] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 16:01:28.489] [info] [graph][runtime.node] audio_encode_trace stage=codec_bound
[2026-10-19 16:01:28.489] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-10-19 16:01:28.489] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=27 generation=1
[2026-10-19 16:01:28.490] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-10-19 16:01:28.490] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=28 generation=1
[2026-10-19 16:01:28.490] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=2589528156 dts=2589528156 size=137733 generation=1 sequence=28
[2026-10-19 16:01:28.490] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-10-19 16:01:28.491] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=2104137803 dts=2104137803 size=137733 generation=1 sequence=27
[2026-10-19 16:01:28.491] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=28 generation=1
[2026-10-19 16:01:28.492] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=2056688943 dts=2056688943 size=137733 generation=1 sequence=28
[2026-10-19 16:01:28.519] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-10-19 16:01:28.519] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=2589528156 format=cuda size=1280x720 buffer_identity=0xb0aa00000 drm_prime=0 software_frame=0
[2026-10-19 16:01:28.519] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=000001E3852C0010 stream=Video payload=Frame pts=2589528156 dts=2589528156 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:01:28.519] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=000001E3852C0010 stream=Video payload=Frame pts=2589528156 dts=2589528156 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:01:28.519] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=000001E3852C0010 stream=Video payload=Frame pts=2589528156 dts=2589528156 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:01:28.519] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-10-19 16:01:28.520] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=000001E3852C0FD0 stream=Video payload=Frame pts=2589528156 dts=2589528156 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:01:28.520] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=000001E3852C0FD0 stream=Video payload=Frame pts=2589528156 dts=2589528156 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:01:28.520] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-10-19 16:01:28.522] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-10-19 16:01:28.522] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=2104137803 format=cuda size=1280x720 buffer_identity=0xb0ae00000 drm_prime=0 software_frame=0
[2026-10-19 16:01:28.522] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=000001E3852C0BE0 stream=Video payload=Frame pts=2104137803 dts=2104137803 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:01:28.522] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=000001E3852C0BE0 stream=Video payload=Frame pts=2104137803 dts=2104137803 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:01:28.522] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=000001E3852C0BE0 stream=Video payload=Frame pts=2104137803 dts=2104137803 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:01:28.523] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-10-19 16:01:28.523] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=000001E3852C07F0 stream=Video payload=Frame pts=2104137803 dts=2104137803 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:01:28.523] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=000001E3852C07F0 stream=Video payload=Frame pts=2104137803 dts=2104137803 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:01:28.523] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-10-19 16:01:28.525] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-10-19 16:01:28.525] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=2056688943 format=cuda size=1280x720 buffer_identity=0xb0d600000 drm_prime=0 software_frame=0
[2026-10-19 16:01:28.525] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=000001E38B200B50 stream=Video payload=Frame pts=2056688943 dts=2056688943 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:01:28.525] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=000001E38B200B50 stream=Video payload=Frame pts=2056688943 dts=2056688943 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:01:28.525] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=000001E38B200B50 stream=Video payload=Frame pts=2056688943 dts=2056688943 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:01:28.525] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-10-19 16:01:28.525] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=000001E38B200F40 stream=Video payload=Frame pts=2056688943 dts=2056688943 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:01:28.525] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=000001E38B200F40 stream=Video payload=Frame pts=2056688943 dts=2056688943 duration=3000 tb=1/90000 flags=68
[2026-10-19 16:01:28.525] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-10-19 16:01:28.538] [info] [graph][runtime.lifecycle] stop.begin
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=8 available=1 cpu_ns=0 process_calls=30
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=9 available=1 cpu_ns=0 process_calls=3
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=29 available=1 cpu_ns=0 process_calls=3
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=30 available=1 cpu_ns=0 process_calls=104
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=33 available=1 cpu_ns=0 process_calls=101
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=36 available=1 cpu_ns=0 process_calls=4
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=50 available=1 cpu_ns=15625000 process_calls=117
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=59 available=1 cpu_ns=0 process_calls=4
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=17 available=1 cpu_ns=0 process_calls=4
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=54 available=1 cpu_ns=0 process_calls=124
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=15 available=1 cpu_ns=0 process_calls=119
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=55 available=1 cpu_ns=0 process_calls=128
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=47 available=1 cpu_ns=0 process_calls=12
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=35 available=1 cpu_ns=0 process_calls=67
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=18 available=1 cpu_ns=0 process_calls=9
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=60 available=1 cpu_ns=0 process_calls=9
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=37 available=1 cpu_ns=0 process_calls=9
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=19 available=1 cpu_ns=0 process_calls=3
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=78 available=1 cpu_ns=0 process_calls=1
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=12 available=1 cpu_ns=0 process_calls=4
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=4 available=1 cpu_ns=0 process_calls=3
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] process.failed node=41 name=VideoDecodeNode kind=VideoDecode inputs=[channel=49 edge=49 stream=Video edge_kind=Metadata payload=CodecContext queued=0; channel=50 edge=50 stream=Video edge_kind=InputPacket payload=Packet queued=4] outputs=[channel=52 edge=52 stream=Video edge_kind=RawFrame payload=Frame queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=72 available=1 cpu_ns=0 process_calls=1
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=41 available=1 cpu_ns=15625000 process_calls=15
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=27 available=1 cpu_ns=0 process_calls=15
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=46 available=1 cpu_ns=0 process_calls=10
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=73 available=1 cpu_ns=0 process_calls=1
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=77 available=1 cpu_ns=0 process_calls=1
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=81 available=1 cpu_ns=0 process_calls=1
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=42 available=1 cpu_ns=0 process_calls=29
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=56 available=1 cpu_ns=0 process_calls=110
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=76 available=1 cpu_ns=0 process_calls=3
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=80 available=1 cpu_ns=0 process_calls=1
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=57 available=1 cpu_ns=0 process_calls=130
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=52 available=1 cpu_ns=0 process_calls=4
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=70 available=1 cpu_ns=0 process_calls=2
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=13 available=1 cpu_ns=0 process_calls=5
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=24 available=1 cpu_ns=0 process_calls=18
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=31 available=1 cpu_ns=0 process_calls=4
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=32 available=1 cpu_ns=0 process_calls=5
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=23 available=1 cpu_ns=0 process_calls=25
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=10 available=1 cpu_ns=0 process_calls=3
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=49 available=1 cpu_ns=0 process_calls=4
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=38 available=1 cpu_ns=0 process_calls=4
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=61 available=1 cpu_ns=0 process_calls=3
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=20 available=1 cpu_ns=0 process_calls=26
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=62 available=1 cpu_ns=0 process_calls=32
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=71 available=1 cpu_ns=0 process_calls=1
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=44 available=1 cpu_ns=0 process_calls=1
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=28 available=1 cpu_ns=0 process_calls=3
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=75 available=1 cpu_ns=0 process_calls=1
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=67 available=1 cpu_ns=0 process_calls=1
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=6 available=1 cpu_ns=0 process_calls=1
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=7 available=1 cpu_ns=0 process_calls=4
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=2 available=1 cpu_ns=0 process_calls=3
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=79 available=1 cpu_ns=0 process_calls=1
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=14 available=1 cpu_ns=0 process_calls=104
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=16 available=1 cpu_ns=0 process_calls=74
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=65 available=1 cpu_ns=0 process_calls=29
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=43 available=1 cpu_ns=0 process_calls=19
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=48 available=1 cpu_ns=0 process_calls=3
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=25 available=1 cpu_ns=0 process_calls=1
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=74 available=1 cpu_ns=0 process_calls=1
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=51 available=1 cpu_ns=0 process_calls=122
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=34 available=1 cpu_ns=0 process_calls=88
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=82 available=1 cpu_ns=0 process_calls=1
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=53 available=1 cpu_ns=0 process_calls=5
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=66 available=1 cpu_ns=0 process_calls=19
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=11 available=1 cpu_ns=0 process_calls=100
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=58 available=1 cpu_ns=0 process_calls=159
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=5 available=1 cpu_ns=0 process_calls=1
[2026-10-19 16:01:28.538] [info] [graph][runtime.node] worker.thread_cpu node=39 available=1 cpu_ns=0 process_calls=28
[2026-10-19 16:01:28.539] [info] [graph][runtime.node] process.failed node=22 name=VideoDecodeNode kind=VideoDecode inputs=[channel=22 edge=22 stream=Video edge_kind=Metadata payload=CodecContext queued=0; channel=23 edge=23 stream=Video edge_kind=InputPacket payload=Packet queued=6] outputs=[channel=25 edge=25 stream=Video edge_kind=RawFrame payload=Frame queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-19 16:01:28.539] [info] [graph][runtime.node] worker.thread_cpu node=22 available=1 cpu_ns=15625000 process_calls=13
[2026-10-19 16:01:28.539] [info] [graph][runtime.node] process.failed node=64 name=VideoDecodeNode kind=VideoDecode inputs=[channel=84 edge=84 stream=Video edge_kind=Metadata payload=CodecContext queued=0; channel=85 edge=85 stream=Video edge_kind=InputPacket payload=Packet queued=8] outputs=[channel=87 edge=87 stream=Video edge_kind=RawFrame payload=Frame queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-19 16:01:28.539] [info] [graph][runtime.node] worker.thread_cpu node=64 available=1 cpu_ns=0 process_calls=15
[2026-10-19 16:01:28.595] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=69
[2026-10-19 16:01:28.595] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=72
[2026-10-19 16:01:28.595] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=75
[2026-10-19 16:01:28.596] [info] [graph][runtime.node] process.failed node=26 name=VideoFilterNode kind=VideoFilter inputs=[channel=24 edge=24 stream=Video edge_kind=Metadata payload=CodecContext queued=0; channel=27 edge=27 stream=Video edge_kind=RawFrame payload=Frame queued=6] outputs=[channel=28 edge=28 stream=Video edge_kind=RawFrame payload=Frame queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-19 16:01:28.596] [info] [graph][runtime.node] worker.thread_cpu node=26 available=1 cpu_ns=31250000 process_calls=3
[2026-10-19 16:01:28.596] [info] [graph][runtime.node] process.failed node=68 name=VideoFilterNode kind=VideoFilter inputs=[channel=86 edge=86 stream=Video edge_kind=Metadata payload=CodecContext queued=0; channel=89 edge=89 stream=Video edge_kind=RawFrame payload=Frame queued=6] outputs=[channel=90 edge=90 stream=Video edge_kind=RawFrame payload=Frame queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-19 16:01:28.596] [info] [graph][runtime.node] process.failed node=45 name=VideoFilterNode kind=VideoFilter inputs=[channel=51 edge=51 stream=Video edge_kind=Metadata payload=CodecContext queued=0; channel=54 edge=54 stream=Video edge_kind=RawFrame payload=Frame queued=6] outputs=[channel=55 edge=55 stream=Video edge_kind=RawFrame payload=Frame queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-19 16:01:28.596] [info] [graph][runtime.node] worker.thread_cpu node=68 available=1 cpu_ns=31250000 process_calls=3
[2026-10-19 16:01:28.596] [info] [graph][runtime.node] worker.thread_cpu node=45 available=1 cpu_ns=15625000 process_calls=3
[2026-10-19 16:01:28.596] [info] [graph][runtime.node] mpegts_wire_materializer stage=stopped maximum_materialized_after_release_ns=0 worst_materialized_at_ns=0 worst_release_ns=0 worst_deadline_ns=0 worst_global_sequence=0
[2026-10-19 16:01:28.596] [info] [graph][runtime.lifecycle] abort.begin
[2026-10-19 16:01:28.596] [info] [graph][runtime.node] scheduled_datagram_sender stage=aborted generation=0 service_scope= execution_mode=1 committed_batches=0 committed_datagrams=0 committed_payload_bytes=0 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=0 ordered_queue_maximum_datagrams=0 ordered_queue_maximum_wire_bytes=0 pacing_reserved=0 pacing_cancelled=0 pacing_submitted=0 pacing_maximum_submit_lateness_ns=0 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=0 delivery_evidence=not_proven shared_scope_charged_datagrams=0 shared_scope_charged_wire_bytes=0 shared_scope_waiting_members=0 shared_scope_failed=0
[2026-10-19 16:01:28.596] [info] [graph][runtime.lifecycle] node_abort_payload node=82 before_objects=223 after_objects=223 before_bytes=4358270 after_bytes=4358270 before_reservations=370 after_reservations=370 before_releases=147 after_releases=147
[2026-10-19 16:01:28.596] [info] [graph][runtime.node] mpegts_wire_materializer stage=aborted maximum_materialized_after_release_ns=0 worst_materialized_at_ns=0 worst_release_ns=0 worst_deadline_ns=0 worst_global_sequence=0
[2026-10-19 16:01:28.597] [info] [graph][runtime.lifecycle] node_abort_payload node=80 before_objects=223 after_objects=223 before_bytes=4358270 after_bytes=4358270 before_reservations=370 after_reservations=370 before_releases=147 after_releases=147
[2026-10-19 16:01:28.597] [info] [graph][runtime.lifecycle] node_abort_payload node=76 before_objects=223 after_objects=223 before_bytes=4358270 after_bytes=4358270 before_reservations=370 after_reservations=370 before_releases=147 after_releases=147
[2026-10-19 16:01:28.597] [info] [graph][runtime.lifecycle] node_abort_payload node=79 before_objects=223 after_objects=223 before_bytes=4358270 after_bytes=4358270 before_reservations=370 after_reservations=370 before_releases=147 after_releases=147
[2026-10-19 16:01:28.597] [info] [graph][runtime.lifecycle] node_abort_payload node=75 before_objects=223 after_objects=223 before_bytes=4358270 after_bytes=4358270 before_reservations=370 after_reservations=370 before_releases=147 after_releases=147
[2026-10-19 16:01:28.597] [info] [graph][runtime.lifecycle] node_abort_payload node=74 before_objects=223 after_objects=223 before_bytes=4358270 after_bytes=4358270 before_reservations=370 after_reservations=370 before_releases=147 after_releases=147
[2026-10-19 16:01:28.597] [info] [graph][runtime.lifecycle] node_abort_payload node=5 before_objects=223 after_objects=223 before_bytes=4358270 after_bytes=4358270 before_reservations=370 after_reservations=370 before_releases=147 after_releases=147
[2026-10-19 16:01:28.597] [info] [graph][runtime.lifecycle] node_abort_payload node=77 before_objects=223 after_objects=223 before_bytes=4358270 after_bytes=4358270 before_reservations=370 after_reservations=370 before_releases=147 after_releases=147
[2026-10-19 16:01:28.597] [info] [graph][runtime.lifecycle] codec_owner_release node=4 kind=AudioEncode owner_references=4
[2026-10-19 16:01:28.597] [info] [graph][runtime.lifecycle] node_abort_payload node=4 before_objects=223 after_objects=223 before_bytes=4358270 after_bytes=4358270 before_reservations=370 after_reservations=370 before_releases=147 after_releases=147
[2026-10-19 16:01:28.597] [info] [graph][runtime.lifecycle] node_abort_payload node=78 before_objects=223 after_objects=223 before_bytes=4358270 after_bytes=4358270 before_reservations=370 after_reservations=370 before_releases=147 after_releases=147
[2026-10-19 16:01:28.597] [info] [graph][runtime.lifecycle] node_abort_payload node=81 before_objects=223 after_objects=223 before_bytes=4358270 after_bytes=4358270 before_reservations=370 after_reservations=370 before_releases=147 after_releases=147
[2026-10-19 16:01:28.597] [info] [graph][runtime.node] continuous_audio_aggregate.summary reason=abort output_group=composition-r269.output audio_commits=0 real_samples=0 silence_samples=0
[2026-10-19 16:01:28.597] [info] [graph][runtime.lifecycle] node_abort_payload node=7 before_objects=223 after_objects=223 before_bytes=4358270 after_bytes=4358270 before_reservations=370 after_reservations=370 before_releases=147 after_releases=147
[2026-10-19 16:01:28.597] [info] [graph][runtime.lifecycle] codec_owner_release node=2 kind=VideoEncode owner_references=4
[2026-10-19 16:01:28.597] [info] [graph][runtime.lifecycle] node_abort_payload node=2 before_objects=223 after_objects=223 before_bytes=4358270 after_bytes=4358270 before_reservations=370 after_reservations=370 before_releases=147 after_releases=147
[2026-10-19 16:01:28.597] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=process count=1 maximum_ns=22600 begin_steady_ns=26302404829400 end_steady_ns=26302404852000 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=waiting deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-10-19 16:01:28.597] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=purge count=1 maximum_ns=400 begin_steady_ns=26302404829500 end_steady_ns=26302404829900 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 16:01:28.597] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=bind count=1 maximum_ns=8500 begin_steady_ns=26302404833900 end_steady_ns=26302404842400 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 16:01:28.597] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=consume count=1 maximum_ns=8400 begin_steady_ns=26302404842400 end_steady_ns=26302404850800 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 16:01:28.597] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=activate count=1 maximum_ns=700 begin_steady_ns=26302404850900 end_steady_ns=26302404851600 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 16:01:28.597] [info] [graph][runtime.node] continuous_aggregate.summary reason=abort output_group=composition-r269.output output_generation=1 video_commits=0 source[0].group=composition-r269.source.0 source[0].real_frames=0 source[0].black_frames=0 source[0].contribution_generation=0 source[1].group=composition-r269.source.1 source[1].real_frames=0 source[1].black_frames=0 source[1].contribution_generation=0 source[2].group=composition-r269.source.2 source[2].real_frames=0 source[2].black_frames=0 source[2].contribution_generation=0
[2026-10-19 16:01:28.597] [info] [graph][runtime.node] cuda_canvas_call stage=context_push count=4 operation=upload maximum_ns=7400 begin_steady_ns=26302365936100 end_steady_ns=26302365943500 native_result=0 thread_cpu_ns=0
[2026-10-19 16:01:28.597] [info] [graph][runtime.node] cuda_canvas_call stage=source_wait count=6 operation=copy maximum_ns=5700 begin_steady_ns=26302367307700 end_steady_ns=26302367313400 native_result=0 thread_cpu_ns=0
[2026-10-19 16:01:28.597] [info] [graph][runtime.node] cuda_canvas_call stage=plane_copy count=14 operation=download maximum_ns=309600 begin_steady_ns=26302366364800 end_steady_ns=26302366674400 native_result=0 thread_cpu_ns=0
[2026-10-19 16:01:28.597] [info] [graph][runtime.node] cuda_canvas_call stage=destination_wait count=2 operation=upload maximum_ns=94100 begin_steady_ns=26302366247700 end_steady_ns=26302366341800 native_result=0 thread_cpu_ns=0
[2026-10-19 16:01:28.597] [info] [graph][runtime.node] cuda_canvas_call stage=context_pop count=4 operation=upload maximum_ns=2400 begin_steady_ns=26302366342000 end_steady_ns=26302366344400 native_result=0 thread_cpu_ns=0
[2026-10-19 16:01:28.597] [info] [graph][runtime.node] cuda_canvas_call stage=batch_completion count=2 operation=copy maximum_ns=113800 begin_steady_ns=26302367043700 end_steady_ns=26302367157500 native_result=0 thread_cpu_ns=0
[2026-10-19 16:01:28.597] [info] [graph][runtime.lifecycle] node_abort_payload node=6 before_objects=223 after_objects=223 before_bytes=4358270 after_bytes=4358270 before_reservations=370 after_reservations=370 before_releases=147 after_releases=147
[2026-10-19 16:01:28.597] [info] [graph][runtime.lifecycle] node_abort_payload node=67 before_objects=223 after_objects=223 before_bytes=4358270 after_bytes=4358270 before_reservations=370 after_reservations=370 before_releases=147 after_releases=147
[2026-10-19 16:01:28.597] [info] [graph][runtime.lifecycle] node_abort_payload node=44 before_objects=223 after_objects=223 before_bytes=4358270 after_bytes=4358270 before_reservations=370 after_reservations=370 before_releases=147 after_releases=147
[2026-10-19 16:01:28.597] [info] [graph][runtime.lifecycle] node_abort_payload node=25 before_objects=223 after_objects=223 before_bytes=4358270 after_bytes=4358270 before_reservations=370 after_reservations=370 before_releases=147 after_releases=147
[2026-10-19 16:01:28.597] [info] [graph][runtime.lifecycle] codec_owner_release node=73 kind=AudioResample owner_references=1
[2026-10-19 16:01:28.597] [info] [graph][runtime.lifecycle] node_abort_payload node=73 before_objects=223 after_objects=223 before_bytes=4358270 after_bytes=4358270 before_reservations=370 after_reservations=370 before_releases=147 after_releases=147
[2026-10-19 16:01:28.599] [info] [graph][runtime.lifecycle] node_abort_payload node=68 before_objects=223 after_objects=222 before_bytes=4358270 after_bytes=4358270 before_reservations=370 after_reservations=370 before_releases=147 after_releases=148
[2026-10-19 16:01:28.601] [info] [graph][runtime.lifecycle] node_abort_payload node=45 before_objects=222 after_objects=221 before_bytes=4358270 after_bytes=4358270 before_reservations=370 after_reservations=370 before_releases=148 after_releases=149
[2026-10-19 16:01:28.604] [info] [graph][runtime.lifecycle] node_abort_payload node=26 before_objects=221 after_objects=220 before_bytes=4358270 after_bytes=4358270 before_reservations=370 after_reservations=370 before_releases=149 after_releases=150
[2026-10-19 16:01:28.604] [info] [graph][runtime.lifecycle] node_abort_payload node=72 before_objects=220 after_objects=220 before_bytes=4358270 after_bytes=4358270 before_reservations=370 after_reservations=370 before_releases=150 after_releases=150
[2026-10-19 16:01:28.604] [info] [graph][runtime.lifecycle] node_abort_payload node=66 before_objects=220 after_objects=219 before_bytes=4358270 after_bytes=4358270 before_reservations=370 after_reservations=370 before_releases=150 after_releases=151
[2026-10-19 16:01:28.604] [info] [graph][runtime.lifecycle] node_abort_payload node=43 before_objects=219 after_objects=218 before_bytes=4358270 after_bytes=4358270 before_reservations=370 after_reservations=370 before_releases=151 after_releases=152
[2026-10-19 16:01:28.604] [info] [graph][runtime.lifecycle] node_abort_payload node=24 before_objects=218 after_objects=217 before_bytes=4358270 after_bytes=4358270 before_reservations=370 after_reservations=370 before_releases=152 after_releases=153
[2026-10-19 16:01:28.604] [info] [graph][runtime.lifecycle] node_abort_payload node=71 before_objects=217 after_objects=217 before_bytes=4358270 after_bytes=4358270 before_reservations=370 after_reservations=370 before_releases=153 after_releases=153
[2026-10-19 16:01:28.604] [info] [graph][runtime.lifecycle] node_abort_payload node=65 before_objects=217 after_objects=217 before_bytes=4358270 after_bytes=4358270 before_reservations=370 after_reservations=370 before_releases=153 after_releases=153
[2026-10-19 16:01:28.604] [info] [graph][runtime.lifecycle] node_abort_payload node=42 before_objects=217 after_objects=217 before_bytes=4358270 after_bytes=4358270 before_reservations=370 after_reservations=370 before_releases=153 after_releases=153
[2026-10-19 16:01:28.604] [info] [graph][runtime.lifecycle] node_abort_payload node=23 before_objects=217 after_objects=217 before_bytes=4358270 after_bytes=4358270 before_reservations=370 after_reservations=370 before_releases=153 after_releases=153
[2026-10-19 16:01:28.604] [info] [graph][runtime.lifecycle] codec_owner_release node=70 kind=AudioDecode owner_references=2
[2026-10-19 16:01:28.604] [info] [graph][runtime.lifecycle] node_abort_payload node=70 before_objects=217 after_objects=217 before_bytes=4358270 after_bytes=4358270 before_reservations=370 after_reservations=370 before_releases=153 after_releases=153
[2026-10-19 16:01:28.604] [info] [graph][runtime.lifecycle] codec_owner_release node=64 kind=VideoDecode owner_references=2
[2026-10-19 16:01:28.609] [info] [graph][runtime.lifecycle] node_abort_payload node=64 before_objects=217 after_objects=217 before_bytes=4358270 after_bytes=4358270 before_reservations=370 after_reservations=370 before_releases=153 after_releases=153
[2026-10-19 16:01:28.609] [info] [graph][runtime.lifecycle] codec_owner_release node=41 kind=VideoDecode owner_references=2
[2026-10-19 16:01:28.615] [info] [graph][runtime.lifecycle] node_abort_payload node=41 before_objects=217 after_objects=217 before_bytes=4358270 after_bytes=4358270 before_reservations=370 after_reservations=370 before_releases=153 after_releases=153
[2026-10-19 16:01:28.615] [info] [graph][runtime.lifecycle] codec_owner_release node=22 kind=VideoDecode owner_references=2
[2026-10-19 16:01:28.621] [info] [graph][runtime.lifecycle] node_abort_payload node=22 before_objects=217 after_objects=217 before_bytes=4358270 after_bytes=4358270 before_reservations=370 after_reservations=370 before_releases=153 after_releases=153
[2026-10-19 16:01:28.621] [info] [graph][runtime.lifecycle] node_abort_payload node=62 before_objects=217 after_objects=217 before_bytes=4358270 after_bytes=4358270 before_reservations=370 after_reservations=370 before_releases=153 after_releases=153
[2026-10-19 16:01:28.621] [info] [graph][runtime.lifecycle] node_abort_payload node=39 before_objects=217 after_objects=217 before_bytes=4358270 after_bytes=4358270 before_reservations=370 after_reservations=370 before_releases=153 after_releases=153
[2026-10-19 16:01:28.621] [info] [graph][runtime.lifecycle] node_abort_payload node=20 before_objects=217 after_objects=217 before_bytes=4358270 after_bytes=4358270 before_reservations=370 after_reservations=370 before_releases=153 after_releases=153
[2026-10-19 16:01:28.621] [info] [graph][runtime.lifecycle] node_abort_payload node=61 before_objects=217 after_objects=184 before_bytes=4358270 after_bytes=4036763 before_reservations=370 after_reservations=370 before_releases=153 after_releases=186
[2026-10-19 16:01:28.621] [info] [graph][runtime.lifecycle] node_abort_payload node=38 before_objects=184 after_objects=173 before_bytes=4036763 after_bytes=3735834 before_reservations=370 after_reservations=370 before_releases=186 after_releases=197
[2026-10-19 16:01:28.621] [info] [graph][runtime.lifecycle] node_abort_payload node=19 before_objects=173 after_objects=164 before_bytes=3735834 after_bytes=3464642 before_reservations=370 after_reservations=370 before_releases=197 after_releases=206
[2026-10-19 16:01:28.621] [info] [graph][runtime.lifecycle] node_abort_payload node=60 before_objects=164 after_objects=163 before_bytes=3464642 after_bytes=3430754 before_reservations=370 after_reservations=370 before_releases=206 after_releases=207
[2026-10-19 16:01:28.621] [info] [graph][runtime.lifecycle] node_abort_payload node=37 before_objects=163 after_objects=162 before_bytes=3430754 after_bytes=3389322 before_reservations=370 after_reservations=370 before_releases=207 after_releases=208
[2026-10-19 16:01:28.621] [info] [graph][runtime.lifecycle] node_abort_payload node=18 before_objects=162 after_objects=161 before_bytes=3389322 after_bytes=3347890 before_reservations=370 after_reservations=370 before_releases=208 after_releases=209
[2026-10-19 16:01:28.621] [info] [graph][runtime.lifecycle] node_abort_payload node=58 before_objects=161 after_objects=159 before_bytes=3347890 after_bytes=3322419 before_reservations=370 after_reservations=370 before_releases=209 after_releases=211
[2026-10-19 16:01:28.621] [info] [graph][runtime.lifecycle] node_abort_payload node=35 before_objects=159 after_objects=158 before_bytes=3322419 after_bytes=3288363 before_reservations=370 after_reservations=370 before_releases=211 after_releases=212
[2026-10-19 16:01:28.621] [info] [graph][runtime.lifecycle] node_abort_payload node=16 before_objects=158 after_objects=157 before_bytes=3288363 after_bytes=3254307 before_reservations=370 after_reservations=370 before_releases=212 after_releases=213
[2026-10-19 16:01:28.621] [info] [graph][runtime.lifecycle] node_abort_payload node=57 before_objects=157 after_objects=156 before_bytes=3254307 after_bytes=3253230 before_reservations=370 after_reservations=370 before_releases=213 after_releases=214
[2026-10-19 16:01:28.621] [info] [graph][runtime.lifecycle] node_abort_payload node=56 before_objects=156 after_objects=155 before_bytes=3253230 after_bytes=3230008 before_reservations=370 after_reservations=370 before_releases=214 after_releases=215
[2026-10-19 16:01:28.621] [info] [graph][runtime.lifecycle] node_abort_payload node=34 before_objects=155 after_objects=154 before_bytes=3230008 after_bytes=3200138 before_reservations=370 after_reservations=370 before_releases=215 after_releases=216
[2026-10-19 16:01:28.621] [info] [graph][runtime.lifecycle] node_abort_payload node=15 before_objects=154 after_objects=153 before_bytes=3200138 after_bytes=3170268 before_reservations=370 after_reservations=370 before_releases=216 after_releases=217
[2026-10-19 16:01:28.621] [info] [graph][runtime.lifecycle] node_abort_payload node=59 before_objects=153 after_objects=153 before_bytes=3170268 after_bytes=3170268 before_reservations=370 after_reservations=370 before_releases=217 after_releases=217
[2026-10-19 16:01:28.621] [info] [graph][runtime.lifecycle] node_abort_payload node=55 before_objects=153 after_objects=152 before_bytes=3170268 after_bytes=3169159 before_reservations=370 after_reservations=370 before_releases=217 after_releases=218
[2026-10-19 16:01:28.621] [info] [graph][runtime.lifecycle] node_abort_payload node=54 before_objects=152 after_objects=151 before_bytes=3169159 after_bytes=3146887 before_reservations=370 after_reservations=370 before_releases=218 after_releases=219
[2026-10-19 16:01:28.621] [info] [graph][runtime.lifecycle] node_abort_payload node=36 before_objects=151 after_objects=151 before_bytes=3146887 after_bytes=3146887 before_reservations=370 after_reservations=370 before_releases=219 after_releases=219
[2026-10-19 16:01:28.621] [info] [graph][runtime.lifecycle] node_abort_payload node=33 before_objects=151 after_objects=150 before_bytes=3146887 after_bytes=3127213 before_reservations=370 after_reservations=370 before_releases=219 after_releases=220
[2026-10-19 16:01:28.621] [info] [graph][runtime.lifecycle] node_abort_payload node=17 before_objects=150 after_objects=150 before_bytes=3127213 after_bytes=3127213 before_reservations=370 after_reservations=370 before_releases=220 after_releases=220
[2026-10-19 16:01:28.621] [info] [graph][runtime.lifecycle] node_abort_payload node=14 before_objects=150 after_objects=149 before_bytes=3127213 after_bytes=3107539 before_reservations=370 after_reservations=370 before_releases=220 after_releases=221
[2026-10-19 16:01:28.621] [info] [graph][runtime.lifecycle] node_abort_payload node=53 before_objects=149 after_objects=149 before_bytes=3107539 after_bytes=3107539 before_reservations=370 after_reservations=370 before_releases=221 after_releases=221
[2026-10-19 16:01:28.621] [info] [graph][runtime.lifecycle] node_abort_payload node=32 before_objects=149 after_objects=149 before_bytes=3107539 after_bytes=3107539 before_reservations=370 after_reservations=370 before_releases=221 after_releases=221
[2026-10-19 16:01:28.621] [info] [graph][runtime.lifecycle] node_abort_payload node=13 before_objects=149 after_objects=149 before_bytes=3107539 after_bytes=3107539 before_reservations=370 after_reservations=370 before_releases=221 after_releases=221
[2026-10-19 16:01:28.621] [info] [graph][runtime.lifecycle] node_abort_payload node=52 before_objects=149 after_objects=149 before_bytes=3107539 after_bytes=3107539 before_reservations=370 after_reservations=370 before_releases=221 after_releases=221
[2026-10-19 16:01:28.622] [info] [graph][runtime.lifecycle] node_abort_payload node=51 before_objects=149 after_objects=148 before_bytes=3107539 after_bytes=3106513 before_reservations=370 after_reservations=370 before_releases=221 after_releases=222
[2026-10-19 16:01:28.622] [info] [graph][runtime.lifecycle] node_abort_payload node=50 before_objects=148 after_objects=147 before_bytes=3106513 after_bytes=3080576 before_reservations=370 after_reservations=370 before_releases=222 after_releases=223
[2026-10-19 16:01:28.622] [info] [graph][runtime.lifecycle] node_abort_payload node=31 before_objects=147 after_objects=147 before_bytes=3080576 after_bytes=3080576 before_reservations=370 after_reservations=370 before_releases=223 after_releases=223
[2026-10-19 16:01:28.622] [info] [graph][runtime.lifecycle] node_abort_payload node=30 before_objects=147 after_objects=146 before_bytes=3080576 after_bytes=3057904 before_reservations=370 after_reservations=370 before_releases=223 after_releases=224
[2026-10-19 16:01:28.622] [info] [graph][runtime.lifecycle] node_abort_payload node=12 before_objects=146 after_objects=146 before_bytes=3057904 after_bytes=3057904 before_reservations=370 after_reservations=370 before_releases=224 after_releases=224
[2026-10-19 16:01:28.622] [info] [graph][runtime.lifecycle] node_abort_payload node=11 before_objects=146 after_objects=145 before_bytes=3057904 after_bytes=3035232 before_reservations=370 after_reservations=370 before_releases=224 after_releases=225
[2026-10-19 16:01:28.622] [info] [graph][runtime.lifecycle] node_abort_payload node=49 before_objects=145 after_objects=145 before_bytes=3035232 after_bytes=3035232 before_reservations=370 after_reservations=370 before_releases=225 after_releases=225
[2026-10-19 16:01:28.622] [info] [graph][runtime.lifecycle] node_abort_payload node=29 before_objects=145 after_objects=145 before_bytes=3035232 after_bytes=3035232 before_reservations=370 after_reservations=370 before_releases=225 after_releases=225
[2026-10-19 16:01:28.622] [info] [graph][runtime.lifecycle] node_abort_payload node=10 before_objects=145 after_objects=145 before_bytes=3035232 after_bytes=3035232 before_reservations=370 after_reservations=370 before_releases=225 after_releases=225
[2026-10-19 16:01:28.622] [info] [graph][runtime.lifecycle] node_abort_payload node=69 before_objects=145 after_objects=145 before_bytes=3035232 after_bytes=3035232 before_reservations=370 after_reservations=370 before_releases=225 after_releases=225
[2026-10-19 16:01:28.622] [info] [graph][runtime.lifecycle] node_abort_payload node=48 before_objects=145 after_objects=145 before_bytes=3035232 after_bytes=3035232 before_reservations=370 after_reservations=370 before_releases=225 after_releases=225
[2026-10-19 16:01:28.622] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-10-19 16:01:28.622] [info] [graph][runtime.lifecycle] node_abort_payload node=63 before_objects=145 after_objects=145 before_bytes=3035232 after_bytes=3035232 before_reservations=370 after_reservations=370 before_releases=225 after_releases=225
[2026-10-19 16:01:28.622] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-10-19 16:01:28.622] [info] [graph][runtime.lifecycle] node_abort_payload node=40 before_objects=145 after_objects=145 before_bytes=3035232 after_bytes=3035232 before_reservations=370 after_reservations=370 before_releases=225 after_releases=225
[2026-10-19 16:01:28.622] [info] [graph][runtime.lifecycle] node_abort_payload node=28 before_objects=145 after_objects=145 before_bytes=3035232 after_bytes=3035232 before_reservations=370 after_reservations=370 before_releases=225 after_releases=225
[2026-10-19 16:01:28.622] [info] [graph][runtime.node] codec_resolver.release slot=input bufferReferences=2 hasCredit=0 creditReferences=0
[2026-10-19 16:01:28.622] [info] [graph][runtime.lifecycle] node_abort_payload node=21 before_objects=145 after_objects=145 before_bytes=3035232 after_bytes=3035232 before_reservations=370 after_reservations=370 before_releases=225 after_releases=225
[2026-10-19 16:01:28.622] [info] [graph][runtime.lifecycle] node_abort_payload node=9 before_objects=145 after_objects=145 before_bytes=3035232 after_bytes=3035232 before_reservations=370 after_reservations=370 before_releases=225 after_releases=225
[2026-10-19 16:01:28.622] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=118 completion_waits=118 completed_datagrams=122 drained_datagrams=4 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-19 16:01:28.623] [info] [graph][runtime.lifecycle] node_abort_payload node=47 before_objects=145 after_objects=145 before_bytes=3035232 after_bytes=3035232 before_reservations=370 after_reservations=370 before_releases=225 after_releases=225
[2026-10-19 16:01:28.623] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=1297 completion_waits=1297 completed_datagrams=2383 drained_datagrams=1087 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-19 16:01:28.623] [info] [graph][runtime.lifecycle] node_abort_payload node=46 before_objects=145 after_objects=145 before_bytes=3035232 after_bytes=3035232 before_reservations=370 after_reservations=370 before_releases=225 after_releases=225
[2026-10-19 16:01:28.623] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=1450 completion_waits=1450 completed_datagrams=2447 drained_datagrams=998 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-19 16:01:28.623] [info] [graph][runtime.lifecycle] node_abort_payload node=27 before_objects=145 after_objects=145 before_bytes=3035232 after_bytes=3035232 before_reservations=370 after_reservations=370 before_releases=225 after_releases=225
[2026-10-19 16:01:28.623] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=833 completion_waits=833 completed_datagrams=2501 drained_datagrams=1669 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-19 16:01:28.623] [info] [graph][runtime.lifecycle] node_abort_payload node=8 before_objects=145 after_objects=145 before_bytes=3035232 after_bytes=3035232 before_reservations=370 after_reservations=370 before_releases=225 after_releases=225
[2026-10-19 16:01:28.623] [info] [graph][runtime.lifecycle] node_abort_payload node=3 before_objects=145 after_objects=145 before_bytes=3035232 after_bytes=3035232 before_reservations=370 after_reservations=370 before_releases=225 after_releases=225
[2026-10-19 16:01:28.623] [info] [graph][runtime.node] codec_resolver.release slot=encoder_parameters bufferReferences=1 hasCredit=0 creditReferences=0
[2026-10-19 16:01:28.623] [info] [graph][runtime.lifecycle] node_abort_payload node=1 before_objects=145 after_objects=145 before_bytes=3035232 after_bytes=3035232 before_reservations=370 after_reservations=370 before_releases=225 after_releases=225
[2026-10-19 16:01:28.629] [info] [graph][runtime.lifecycle] abort.done state=Aborted
[2026-10-19 16:01:28.669] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=failed detail="Thread32Next failed native=0 entry_size=28 enumerated_threads=6978 process_threads=97" description="D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r269-output.sdp" error=6 native=0 stage=3 reason="Thread32Next failed native=0 entry_size=28 enumerated_threads=6978 process_threads=97"
[2026-10-19 16:01:28.669] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=retired detail="Thread32Next failed native=0 entry_size=28 enumerated_threads=6978 process_threads=97" description="D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r269-output.sdp" error=6 native=0 stage=3 reason="Thread32Next failed native=0 entry_size=28 enumerated_threads=6978 process_threads=97"
[CLI] final runtime report: queued=0, peakQueued=222, threads=82, processThreads=0, workers=0, workerProgress=1135, workerProcessCalls=2124, workerWaits=977, workerWakeups=907, workerErrors=0, errors=1, stalledIntervals=0, cpuSamples=0, logicalProcessors=0, averageSystemMachineCpuPercent=0.000000, averageProcessMachineCpuPercent=0.000000, peakProcessMachineCpuPercent=0.000000, averageProcessSingleCoreCpuPercent=0.000000, peakProcessSingleCoreCpuPercent=0.000000, initialWorkingSetBytes=0, workingSetBytes=0, peakWorkingSetBytes=0, totalPushed=953, totalPopped=804, droppedBuffers=0, encodedPacketsPushed=0, encodedPacketsPopped=0, backpressureItems=123, graphPayloadCurrentBytes=0, graphPayloadHighWaterBytes=4857926, graphPayloadCurrentObjects=0, graphPayloadHighWaterObjects=243, graphPayloadReservations=370, graphPayloadReleases=370, graphPayloadPressureFailures=0, lastInputReceivedAtNs=26302437720200

~~~

### Captured C:/Users/ROG/AppData/Local/Temp/nvidia/nsight_systems/quadd_session_1031940/streams/stderr_ac88-6a42-a397-9bc8.log
~~~text
[in @ 000001e38500c8c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001e38500c8c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 22852.099367
[in @ 000001e38552f580] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001e38552f580] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 28772.535067
[in @ 000001e384ffcdc0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001e384ffcdc0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 23379.308922
[in @ 000001e384da14c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001e384da14c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 22852.099367
[in @ 000001e384da1280] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001e3d2d2e200] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001e384da1280] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 23379.308922
[in @ 000001e3d2d2e200] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 28772.535067
[aac @ 000001e38a681540] Qavg: nan
[CLI] realtime video runtime failed: IoFailure: Thread32Next failed native=0 entry_size=28 enumerated_threads=6978 process_threads=97

~~~

## 产物清单与原始日志
composition-r269-cli-error.log 0 bytes
composition-r269-cli.log 520 bytes
composition-r269-cuda.nsys-rep 100678 bytes
composition-r269-cuda.sqlite 606208 bytes
composition-r269-monitor.log 649 bytes
composition-r269-source0-stdout.log 540 bytes
composition-r269-source0.log 11307 bytes
composition-r269-source1-stdout.log 335 bytes
composition-r269-source1.log 10849 bytes
composition-r269-source2-stdout.log 335 bytes
composition-r269-source2.log 10967 bytes
composition-r269-vlc.log 10816 bytes

### composition-r269-cli-error.log
~~~text

~~~

### composition-r269-cli.log
~~~text
Collecting data...
Generating 'C:\Users\ROG\AppData\Local\Temp\nsys-report-bc26.qdstrm'
[1/1] [0%                          ] composition-r269-cuda.nsys-rep[1/1] [0%                          ] composition-r269-cuda.nsys-rep[1/1] [===================81%      ] composition-r269-cuda.nsys-rep[1/1] [========================100%] composition-r269-cuda.nsys-rep[1/1] [========================100%] composition-r269-cuda.nsys-rep
Generated:
    D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r269-cuda.nsys-rep

~~~

### composition-r269-monitor.log
~~~text
seconds=1.0128675 cpu=0.350602261760434 ws=16707584 private=21282816
seconds=2.0348569 cpu=0.486462081611699 ws=56913920 private=612306944
seconds=3.0754409 cpu=0.546022058941727 ws=184905728 private=894275584
seconds=4.1026998 cpu=1.72845246881597 ws=189018112 private=897691648
seconds=5.131372 cpu=2.96885370551209 ws=133124096 private=590839808
seconds=6.1550283 cpu=0 ws=133128192 private=590839808
seconds=7.1832398 cpu=0.0690740448562647 ws=130572288 private=578199552
seconds=8.2060462 cpu=0 ws=96395264 private=322342912
seconds=9.2320649 cpu=0 ws=96395264 private=322342912
seconds=10.2586541 cpu=0 ws=96395264 private=322342912

~~~

### composition-r269-source0-stdout.log
~~~text
SDP:
v=0
o=- 0 0 IN IP4 127.0.0.1
s=Naraka
t=0 0
a=tool:libavformat 62.19.101
m=video 61020 RTP/AVP 96
c=IN IP4 127.0.0.1
b=AS:8013
a=framerate:30
a=rtpmap:96 H264/90000
a=fmtp:96 packetization-mode=1; sprop-parameter-sets=Z01AH5WQBQBbsBEAAAMD6AAA6mDgAAAehIAAA9CQbvKI+OFSQA==,aOuPIA==; profile-level-id=4D401F
m=audio 61022 RTP/AVP 97
c=IN IP4 127.0.0.1
b=AS:274
a=rtpmap:97 MPEG4-GENERIC/44100/2
a=fmtp:97 profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3; config=121056E500


~~~

### composition-r269-source0.log
~~~text
Input #0, mov,mp4,m4a,3gp,3g2,mj2, from 'D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4':
  Metadata:
    major_brand     : isom
    minor_version   : 512
    compatible_brands: isomiso2avc1mp41
    title           : Naraka
    encoder         : Lavf62.19.101
  Duration: 00:02:00.00, start: 0.000000, bitrate: 8295 kb/s
  Stream #0:0[0x1](eng): Video: h264 (Main) (avc1 / 0x31637661), yuv420p(progressive), 1280x720 [SAR 1:1 DAR 16:9], 8013 kb/s, 30 fps, 30 tbr, 15360 tbn (default)
    Metadata:
      handler_name    : ?Mainconcept Video Media Handler
      encoder         : Lavc62.36.101 h264_nvenc
  Stream #0:1[0x2](eng): Audio: aac (LC) (mp4a / 0x6134706D), 44100 Hz, stereo, fltp, 274 kb/s (default)
    Metadata:
      handler_name    : #Mainconcept MP4 Sound Media Handler
Stream mapping:
  Stream #0:0 -> #0:0 (copy)
  Stream #0:1 -> #1:0 (copy)
Output #0, rtp, to 'rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200':
  Metadata:
    major_brand     : isom
    minor_version   : 512
    compatible_brands: isomiso2avc1mp41
    title           : Naraka
    encoder         : Lavf62.19.101
  Stream #0:0(eng): Video: h264 (Main) (avc1 / 0x31637661), yuv420p(progressive), 1280x720 [SAR 1:1 DAR 16:9], q=2-31, 8013 kb/s, 30 fps, 30 tbr, 90k tbn (default)
    Metadata:
      handler_name    : ?Mainconcept Video Media Handler
      encoder         : Lavc62.36.101 h264_nvenc
Output #1, rtp, to 'rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200':
  Metadata:
    major_brand     : isom
    minor_version   : 512
    compatible_brands: isomiso2avc1mp41
    title           : Naraka
    encoder         : Lavf62.19.101
  Stream #1:0(eng): Audio: aac (LC) (mp4a / 0x6134706D), 44100 Hz, stereo, fltp, 274 kb/s (default)
    Metadata:
      handler_name    : #Mainconcept MP4 Sound Media Handler
frame=   31 fps=0.0 q=-1.0 size=    1174KiB time=00:00:01.03 bitrate=9305.4kbits/s speed=1.99x elapsed=0:00:00.51    frame=   46 fps= 45 q=-1.0 size=    2245KiB time=00:00:01.53 bitrate=11992.8kbits/s speed=1.49x elapsed=0:00:01.03    frame=   62 fps= 40 q=-1.0 size=    2629KiB time=00:00:02.06 bitrate=10419.8kbits/s speed=1.33x elapsed=0:00:01.54    frame=   77 fps= 37 q=-1.0 size=    2900KiB time=00:00:02.57 bitrate=9216.5kbits/s speed=1.25x elapsed=0:00:02.06    frame=   92 fps= 36 q=-1.0 size=    3450KiB time=00:00:03.06 bitrate=9216.5kbits/s speed=1.19x elapsed=0:00:02.57    frame=  108 fps= 35 q=-1.0 size=    3880KiB time=00:00:03.60 bitrate=8829.2kbits/s speed=1.17x elapsed=0:00:03.08    frame=  124 fps= 34 q=-1.0 size=    4406KiB time=00:00:04.13 bitrate=8731.5kbits/s speed=1.14x elapsed=0:00:03.61    frame=  139 fps= 34 q=-1.0 size=    4747KiB time=00:00:04.64 bitrate=8373.6kbits/s speed=1.12x elapsed=0:00:04.14    frame=  155 fps= 33 q=-1.0 size=    5467KiB time=00:00:05.16 bitrate=8668.9kbits/s speed=1.11x elapsed=0:00:04.65    frame=  170 fps= 33 q=-1.0 size=    5969KiB time=00:00:05.66 bitrate=8628.6kbits/s speed= 1.1x elapsed=0:00:05.17    frame=  186 fps= 33 q=-1.0 size=    6571KiB time=00:00:06.20 bitrate=8682.4kbits/s speed=1.09x elapsed=0:00:05.68    frame=  201 fps= 32 q=-1.0 size=    6860KiB time=00:00:06.70 bitrate=8387.9kbits/s speed=1.08x elapsed=0:00:06.19    frame=  217 fps= 32 q=-1.0 size=    7382KiB time=00:00:07.23 bitrate=8360.7kbits/s speed=1.08x elapsed=0:00:06.72    frame=  232 fps= 32 q=-1.0 size=    7816KiB time=00:00:07.75 bitrate=8255.6kbits/s speed=1.07x elapsed=0:00:07.24    frame=  248 fps= 32 q=-1.0 size=    8407KiB time=00:00:08.26 bitrate=8330.6kbits/s speed=1.06x elapsed=0:00:07.76    frame=  263 fps= 32 q=-1.0 size=    8814KiB time=00:00:08.77 bitrate=8226.0kbits/s speed=1.06x elapsed=0:00:08.28    frame=  279 fps= 32 q=-1.0 size=    9867KiB time=00:00:09.30 bitrate=8691.2kbits/s speed=1.06x elapsed=0:00:08.79    frame=  295 fps= 32 q=-1.0 size=   10503KiB time=00:00:09.83 bitrate=8750.0kbits/s speed=1.06x elapsed=0:00:09.31    frame=  310 fps= 31 q=-1.0 size=   10808KiB time=00:00:10.33 bitrate=8568.2kbits/s speed=1.05x elapsed=0:00:09.84    frame=  326 fps= 31 q=-1.0 size=   11079KiB time=00:00:10.86 bitrate=8352.1kbits/s speed=1.05x elapsed=0:00:10.36    frame=  341 fps= 31 q=-1.0 size=   11682KiB time=00:00:11.37 bitrate=8411.4kbits/s speed=1.05x elapsed=0:00:10.87    frame=  357 fps= 31 q=-1.0 size=   12105KiB time=00:00:11.91 bitrate=8324.7kbits/s speed=1.05x elapsed=0:00:11.38    frame=  372 fps= 31 q=-1.0 size=   12551KiB time=00:00:12.40 bitrate=8291.6kbits/s speed=1.04x elapsed=0:00:11.89    frame=  388 fps= 31 q=-1.0 size=   12936KiB time=00:00:12.93 bitrate=8193.3kbits/s speed=1.04x elapsed=0:00:12.42    frame=  403 fps= 31 q=-1.0 size=   13695KiB time=00:00:13.43 bitrate=8351.4kbits/s speed=1.04x elapsed=0:00:12.93    frame=  418 fps= 31 q=-1.0 size=   14165KiB time=00:00:13.93 bitrate=8328.3kbits/s speed=1.04x elapsed=0:00:13.44    frame=  434 fps= 31 q=-1.0 size=   14790KiB time=00:00:14.50 bitrate=8355.7kbits/s speed=1.04x elapsed=0:00:13.97    frame=  450 fps= 31 q=-1.0 size=   15085KiB time=00:00:15.00 bitrate=8238.4kbits/s speed=1.04x elapsed=0:00:14.48    frame=  465 fps= 31 q=-1.0 size=   15598KiB time=00:00:15.50 bitrate=8243.8kbits/s speed=1.03x elapsed=0:00:15.00    frame=  481 fps= 31 q=-1.0 size=   16003KiB time=00:00:16.03 bitrate=8176.7kbits/s speed=1.03x elapsed=0:00:15.51    frame=  496 fps= 31 q=-1.0 size=   16592KiB time=00:00:16.53 bitrate=8221.0kbits/s speed=1.03x elapsed=0:00:16.02    frame=  512 fps= 31 q=-1.0 size=   17340KiB time=00:00:17.06 bitrate=8323.2kbits/s speed=1.03x elapsed=0:00:16.55    frame=  528 fps= 31 q=-1.0 size=   17986KiB time=00:00:17.60 bitrate=8371.3kbits/s speed=1.03x elapsed=0:00:17.08    frame=  543 fps= 31 q=-1.0 size=   18688KiB time=00:00:18.10 bitrate=8457.9kbits/s speed=1.03x elapsed=0:00:17.59    frame=  559 fps= 31 q=-1.0 size=   18963KiB time=00:00:18.63 bitrate=8336.7kbits/s speed=1.03x elapsed=0:00:18.10    frame=  574 fps= 31 q=-1.0 size=   19319KiB time=00:00:19.13 bitrate=8271.7kbits/s speed=1.03x elapsed=0:00:18.62    frame=  589 fps= 31 q=-1.0 size=   19773KiB time=00:00:19.63 bitrate=8250.2kbits/s speed=1.03x elapsed=0:00:19.13    frame=  605 fps= 31 q=-1.0 size=   20376KiB time=00:00:20.16 bitrate=8277.0kbits/s speed=1.03x elapsed=0:00:19.65    frame=  620 fps= 31 q=-1.0 size=   20723KiB time=00:00:20.66 bitrate=8214.4kbits/s speed=1.02x elapsed=0:00:20.16    frame=  635 fps= 31 q=-1.0 size=   21262KiB time=00:00:21.17 bitrate=8225.2kbits/s speed=1.02x elapsed=0:00:20.68    frame=  652 fps= 31 q=-1.0 size=   21937KiB time=00:00:21.73 bitrate=8268.9kbits/s speed=1.02x elapsed=0:00:21.20    frame=  667 fps= 31 q=-1.0 size=   22490KiB time=00:00:22.23 bitrate=8286.4kbits/s speed=1.02x elapsed=0:00:21.72    frame=  682 fps= 31 q=-1.0 size=   22949KiB time=00:00:22.73 bitrate=8269.7kbits/s speed=1.02x elapsed=0:00:22.23    frame=  697 fps= 31 q=-1.0 size=   23303KiB time=00:00:23.24 bitrate=8213.1kbits/s speed=1.02x elapsed=0:00:22.74    frame=  714 fps= 31 q=-1.0 size=   23746KiB time=00:00:23.80 bitrate=8173.5kbits/s speed=1.02x elapsed=0:00:23.27    frame=  729 fps= 31 q=-1.0 size=   24317KiB time=00:00:24.30 bitrate=8197.7kbits/s speed=1.02x elapsed=0:00:23.78    frame=  744 fps= 31 q=-1.0 size=   24736KiB time=00:00:24.80 bitrate=8170.8kbits/s speed=1.02x elapsed=0:00:24.30    frame=  759 fps= 31 q=-1.0 size=   25193KiB time=00:00:25.30 bitrate=8154.3kbits/s speed=1.02x elapsed=0:00:24.81    frame=  775 fps= 31 q=-1.0 size=   25725KiB time=00:00:25.83 bitrate=8157.5kbits/s speed=1.02x elapsed=0:00:25.32    frame=  791 fps= 31 q=-1.0 size=   26721KiB time=00:00:26.33 bitrate=8312.6kbits/s speed=1.02x elapsed=0:00:25.84    frame=  806 fps= 31 q=-1.0 size=   27000KiB time=00:00:26.86 bitrate=8232.8kbits/s speed=1.02x elapsed=0:00:26.35    frame=  821 fps= 31 q=-1.0 size=   27374KiB time=00:00:27.37 bitrate=8191.2kbits/s speed=1.02x elapsed=0:00:26.88    frame=  837 fps= 31 q=-1.0 size=   27919KiB time=00:00:27.91 bitrate=8194.6kbits/s speed=1.02x elapsed=0:00:27.38    frame=  852 fps= 31 q=-1.0 size=   28441KiB time=00:00:28.40 bitrate=8203.9kbits/s speed=1.02x elapsed=0:00:27.90    frame=  867 fps= 31 q=-1.0 size=   28818KiB time=00:00:28.90 bitrate=8166.2kbits/s speed=1.02x elapsed=0:00:28.41    frame=  883 fps= 31 q=-1.0 size=   29319KiB time=00:00:29.44 bitrate=8157.6kbits/s speed=1.02x elapsed=0:00:28.94    frame=  899 fps= 31 q=-1.0 size=   29962KiB time=00:00:29.96 bitrate=8190.9kbits/s speed=1.02x elapsed=0:00:29.45    frame=  914 fps= 30 q=-1.0 size=   30538KiB time=00:00:30.46 bitrate=8211.2kbits/s speed=1.02x elapsed=0:00:29.96    frame=  929 fps= 30 q=-1.0 size=   31070KiB time=00:00:30.97 bitrate=8216.9kbits/s speed=1.02x elapsed=0:00:30.47    frame=  945 fps= 30 q=-1.0 size=   31432KiB time=00:00:31.50 bitrate=8174.4kbits/s speed=1.02x elapsed=0:00:30.99    frame=  961 fps= 30 q=-1.0 size=   31850KiB time=00:00:32.03 bitrate=8145.1kbits/s speed=1.02x elapsed=0:00:31.51    frame=  976 fps= 30 q=-1.0 size=   32381KiB time=00:00:32.53 bitrate=8153.7kbits/s speed=1.02x elapsed=0:00:32.02    frame=  992 fps= 30 q=-1.0 size=   33024KiB time=00:00:33.06 bitrate=8181.5kbits/s speed=1.02x elapsed=0:00:32.54    frame= 1007 fps= 30 q=-1.0 size=   33314KiB time=00:00:33.57 bitrate=8128.1kbits/s speed=1.02x elapsed=0:00:33.05    frame= 1022 fps= 30 q=-1.0 size=   34061KiB time=00:00:34.06 bitrate=8190.6kbits/s speed=1.01x elapsed=0:00:33.57    frame= 1038 fps= 30 q=-1.0 size=   34970KiB time=00:00:34.60 bitrate=8279.6kbits/s speed=1.01x elapsed=0:00:34.09    frame= 1054 fps= 30 q=-1.0 size=   35366KiB time=00:00:35.13 bitrate=8246.2kbits/s speed=1.02x elapsed=0:00:34.60    frame= 1069 fps= 30 q=-1.0 size=   35648KiB time=00:00:35.63 bitrate=8195.3kbits/s speed=1.01x elapsed=0:00:35.12    frame= 1084 fps= 30 q=-1.0 size=   36202KiB time=00:00:36.13 bitrate=8207.5kbits/s speed=1.01x elapsed=0:00:35.63    frame= 1100 fps= 30 q=-1.0 size=   36626KiB time=00:00:36.66 bitrate=8182.9kbits/s speed=1.01x elapsed=0:00:36.14    frame= 1116 fps= 30 q=-1.0 size=   37115KiB time=00:00:37.20 bitrate=8173.3kbits/s speed=1.01x elapsed=0:00:36.67    frame= 1131 fps= 30 q=-1.0 size=   37451KiB time=00:00:37.73 bitrate=8130.7kbits/s speed=1.01x elapsed=0:00:37.20    frame= 1146 fps= 30 q=-1.0 size=   38156KiB time=00:00:38.20 bitrate=8182.6kbits/s speed=1.01x elapsed=0:00:37.71    frame= 1163 fps= 30 q=-1.0 size=   38724KiB time=00:00:38.76 bitrate=8183.0kbits/s speed=1.01x elapsed=0:00:38.24    frame= 1178 fps= 30 q=-1.0 size=   39327KiB time=00:00:39.26 bitrate=8204.6kbits/s speed=1.01x elapsed=0:00:38.75    frame= 1193 fps= 30 q=-1.0 size=   39589KiB time=00:00:39.76 bitrate=8155.5kbits/s speed=1.01x elapsed=0:00:39.26    frame= 1208 fps= 30 q=-1.0 size=   40087KiB time=00:00:40.28 bitrate=8151.5kbits/s speed=1.01x elapsed=0:00:39.78    frame= 1224 fps= 30 q=-1.0 size=   40498KiB time=00:00:40.80 bitrate=8131.4kbits/s speed=1.01x elapsed=0:00:40.29    frame= 1240 fps= 30 q=-1.0 size=   41087KiB time=00:00:41.33 bitrate=8143.1kbits/s speed=1.01x elapsed=0:00:40.80    frame= 1255 fps= 30 q=-1.0 size=   41473KiB time=00:00:41.84 bitrate=8119.7kbits/s speed=1.01x elapsed=0:00:41.32    
~~~

### composition-r269-source1-stdout.log
~~~text
SDP:
v=0
o=- 0 0 IN IP4 127.0.0.1
s=Naraka
c=IN IP4 127.0.0.1
t=0 0
a=tool:libavformat 62.19.101
m=video 61030 RTP/AVP 96
b=AS:8013
a=framerate:30
a=rtpmap:96 H264/90000
a=fmtp:96 packetization-mode=1; sprop-parameter-sets=Z01AH5WQBQBbsBEAAAMD6AAA6mDgAAAehIAAA9CQbvKI+OFSQA==,aOuPIA==; profile-level-id=4D401F


~~~

### composition-r269-source1.log
~~~text
Input #0, mov,mp4,m4a,3gp,3g2,mj2, from 'D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4':
  Metadata:
    major_brand     : isom
    minor_version   : 512
    compatible_brands: isomiso2avc1mp41
    title           : Naraka
    encoder         : Lavf62.19.101
  Duration: 00:02:00.00, start: 0.000000, bitrate: 8295 kb/s
  Stream #0:0[0x1](eng): Video: h264 (Main) (avc1 / 0x31637661), yuv420p(progressive), 1280x720 [SAR 1:1 DAR 16:9], 8013 kb/s, 30 fps, 30 tbr, 15360 tbn (default)
    Metadata:
      handler_name    : ?Mainconcept Video Media Handler
      encoder         : Lavc62.36.101 h264_nvenc
  Stream #0:1[0x2](eng): Audio: aac (LC) (mp4a / 0x6134706D), 44100 Hz, stereo, fltp, 274 kb/s (default)
    Metadata:
      handler_name    : #Mainconcept MP4 Sound Media Handler
Stream mapping:
  Stream #0:0 -> #0:0 (copy)
Output #0, rtp, to 'rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200':
  Metadata:
    major_brand     : isom
    minor_version   : 512
    compatible_brands: isomiso2avc1mp41
    title           : Naraka
    encoder         : Lavf62.19.101
  Stream #0:0(eng): Video: h264 (Main) (avc1 / 0x31637661), yuv420p(progressive), 1280x720 [SAR 1:1 DAR 16:9], q=2-31, 8013 kb/s, 30 fps, 30 tbr, 90k tbn (default)
    Metadata:
      handler_name    : ?Mainconcept Video Media Handler
      encoder         : Lavc62.36.101 h264_nvenc
frame=   31 fps=0.0 q=-1.0 size=    1174KiB time=00:00:01.03 bitrate=9305.4kbits/s speed=1.98x elapsed=0:00:00.52    frame=   47 fps= 45 q=-1.0 size=    2281KiB time=00:00:01.56 bitrate=11929.8kbits/s speed=1.51x elapsed=0:00:01.03    frame=   62 fps= 40 q=-1.0 size=    2629KiB time=00:00:02.06 bitrate=10419.8kbits/s speed=1.33x elapsed=0:00:01.55    frame=   77 fps= 37 q=-1.0 size=    2900KiB time=00:00:02.56 bitrate=9255.1kbits/s speed=1.24x elapsed=0:00:02.06    frame=   93 fps= 36 q=-1.0 size=    3470KiB time=00:00:03.10 bitrate=9168.7kbits/s speed= 1.2x elapsed=0:00:02.57    frame=  108 fps= 35 q=-1.0 size=    3880KiB time=00:00:03.60 bitrate=8829.2kbits/s speed=1.17x elapsed=0:00:03.08    frame=  124 fps= 34 q=-1.0 size=    4406KiB time=00:00:04.13 bitrate=8731.5kbits/s speed=1.14x elapsed=0:00:03.61    frame=  139 fps= 34 q=-1.0 size=    4747KiB time=00:00:04.63 bitrate=8392.9kbits/s speed=1.12x elapsed=0:00:04.12    frame=  155 fps= 33 q=-1.0 size=    5467KiB time=00:00:05.16 bitrate=8668.9kbits/s speed=1.11x elapsed=0:00:04.64    frame=  170 fps= 33 q=-1.0 size=    5969KiB time=00:00:05.66 bitrate=8628.6kbits/s speed= 1.1x elapsed=0:00:05.16    frame=  186 fps= 33 q=-1.0 size=    6571KiB time=00:00:06.20 bitrate=8682.4kbits/s speed=1.09x elapsed=0:00:05.67    frame=  201 fps= 32 q=-1.0 size=    6860KiB time=00:00:06.70 bitrate=8387.9kbits/s speed=1.08x elapsed=0:00:06.18    frame=  216 fps= 32 q=-1.0 size=    7357KiB time=00:00:07.23 bitrate=8331.9kbits/s speed=1.08x elapsed=0:00:06.71    frame=  232 fps= 32 q=-1.0 size=    7816KiB time=00:00:07.73 bitrate=8279.2kbits/s speed=1.07x elapsed=0:00:07.22    frame=  248 fps= 32 q=-1.0 size=    8407KiB time=00:00:08.26 bitrate=8330.6kbits/s speed=1.07x elapsed=0:00:07.75    frame=  263 fps= 32 q=-1.0 size=    8814KiB time=00:00:08.76 bitrate=8235.9kbits/s speed=1.06x elapsed=0:00:08.26    frame=  279 fps= 32 q=-1.0 size=    9867KiB time=00:00:09.30 bitrate=8691.2kbits/s speed=1.06x elapsed=0:00:08.77    frame=  294 fps= 32 q=-1.0 size=   10476KiB time=00:00:09.80 bitrate=8757.4kbits/s speed=1.05x elapsed=0:00:09.30    frame=  310 fps= 32 q=-1.0 size=   10808KiB time=00:00:10.36 bitrate=8540.7kbits/s speed=1.05x elapsed=0:00:09.83    frame=  326 fps= 32 q=-1.0 size=   11079KiB time=00:00:10.86 bitrate=8352.3kbits/s speed=1.05x elapsed=0:00:10.34    frame=  341 fps= 31 q=-1.0 size=   11682KiB time=00:00:11.36 bitrate=8419.6kbits/s speed=1.05x elapsed=0:00:10.86    frame=  356 fps= 31 q=-1.0 size=   12076KiB time=00:00:11.86 bitrate=8336.6kbits/s speed=1.04x elapsed=0:00:11.37    frame=  372 fps= 31 q=-1.0 size=   12551KiB time=00:00:12.40 bitrate=8291.6kbits/s speed=1.04x elapsed=0:00:11.88    frame=  388 fps= 31 q=-1.0 size=   12936KiB time=00:00:12.93 bitrate=8193.5kbits/s speed=1.04x elapsed=0:00:12.40    frame=  403 fps= 31 q=-1.0 size=   13695KiB time=00:00:13.43 bitrate=8351.4kbits/s speed=1.04x elapsed=0:00:12.91    frame=  418 fps= 31 q=-1.0 size=   14165KiB time=00:00:13.93 bitrate=8328.3kbits/s speed=1.04x elapsed=0:00:13.43    frame=  434 fps= 31 q=-1.0 size=   14790KiB time=00:00:14.46 bitrate=8375.0kbits/s speed=1.04x elapsed=0:00:13.96    frame=  450 fps= 31 q=-1.0 size=   15085KiB time=00:00:15.00 bitrate=8238.4kbits/s speed=1.04x elapsed=0:00:14.47    frame=  465 fps= 31 q=-1.0 size=   15598KiB time=00:00:15.50 bitrate=8243.8kbits/s speed=1.03x elapsed=0:00:14.98    frame=  481 fps= 31 q=-1.0 size=   16003KiB time=00:00:16.03 bitrate=8176.7kbits/s speed=1.03x elapsed=0:00:15.50    frame=  495 fps= 31 q=-1.0 size=   16563KiB time=00:00:16.53 bitrate=8206.7kbits/s speed=1.03x elapsed=0:00:16.01    frame=  512 fps= 31 q=-1.0 size=   17340KiB time=00:00:17.06 bitrate=8323.2kbits/s speed=1.03x elapsed=0:00:16.54    frame=  528 fps= 31 q=-1.0 size=   17986KiB time=00:00:17.60 bitrate=8371.7kbits/s speed=1.03x elapsed=0:00:17.07    frame=  543 fps= 31 q=-1.0 size=   18688KiB time=00:00:18.10 bitrate=8457.9kbits/s speed=1.03x elapsed=0:00:17.58    frame=  558 fps= 31 q=-1.0 size=   18945KiB time=00:00:18.60 bitrate=8344.2kbits/s speed=1.03x elapsed=0:00:18.09    frame=  573 fps= 31 q=-1.0 size=   19308KiB time=00:00:19.10 bitrate=8281.3kbits/s speed=1.03x elapsed=0:00:18.60    frame=  589 fps= 31 q=-1.0 size=   19773KiB time=00:00:19.63 bitrate=8250.2kbits/s speed=1.03x elapsed=0:00:19.12    frame=  604 fps= 31 q=-1.0 size=   20344KiB time=00:00:20.13 bitrate=8277.8kbits/s speed=1.02x elapsed=0:00:19.64    frame=  620 fps= 31 q=-1.0 size=   20723KiB time=00:00:20.66 bitrate=8214.4kbits/s speed=1.03x elapsed=0:00:20.15    frame=  636 fps= 31 q=-1.0 size=   21288KiB time=00:00:21.20 bitrate=8226.1kbits/s speed=1.03x elapsed=0:00:20.66    frame=  651 fps= 31 q=-1.0 size=   21916KiB time=00:00:21.70 bitrate=8273.4kbits/s speed=1.02x elapsed=0:00:21.19    frame=  666 fps= 31 q=-1.0 size=   22456KiB time=00:00:22.20 bitrate=8286.4kbits/s speed=1.02x elapsed=0:00:21.70    frame=  682 fps= 31 q=-1.0 size=   22949KiB time=00:00:22.73 bitrate=8269.7kbits/s speed=1.02x elapsed=0:00:22.22    frame=  697 fps= 31 q=-1.0 size=   23303KiB time=00:00:23.23 bitrate=8216.6kbits/s speed=1.02x elapsed=0:00:22.73    frame=  713 fps= 31 q=-1.0 size=   23719KiB time=00:00:23.76 bitrate=8175.7kbits/s speed=1.02x elapsed=0:00:23.25    frame=  728 fps= 31 q=-1.0 size=   24293KiB time=00:00:24.26 bitrate=8200.9kbits/s speed=1.02x elapsed=0:00:23.77    frame=  744 fps= 31 q=-1.0 size=   24736KiB time=00:00:24.80 bitrate=8170.8kbits/s speed=1.02x elapsed=0:00:24.28    frame=  760 fps= 31 q=-1.0 size=   25225KiB time=00:00:25.33 bitrate=8157.0kbits/s speed=1.02x elapsed=0:00:24.80    frame=  774 fps= 31 q=-1.0 size=   25613KiB time=00:00:25.80 bitrate=8132.7kbits/s speed=1.02x elapsed=0:00:25.31    frame=  790 fps= 31 q=-1.0 size=   26684KiB time=00:00:26.33 bitrate=8301.1kbits/s speed=1.02x elapsed=0:00:25.82    frame=  805 fps= 31 q=-1.0 size=   26959KiB time=00:00:26.83 bitrate=8230.3kbits/s speed=1.02x elapsed=0:00:26.34    frame=  822 fps= 31 q=-1.0 size=   27388KiB time=00:00:27.40 bitrate=8188.3kbits/s speed=1.02x elapsed=0:00:26.86    frame=  836 fps= 31 q=-1.0 size=   27898KiB time=00:00:27.86 bitrate=8201.3kbits/s speed=1.02x elapsed=0:00:27.37    frame=  852 fps= 31 q=-1.0 size=   28441KiB time=00:00:28.40 bitrate=8203.9kbits/s speed=1.02x elapsed=0:00:27.88    frame=  868 fps= 31 q=-1.0 size=   28835KiB time=00:00:28.93 bitrate=8164.0kbits/s speed=1.02x elapsed=0:00:28.40    frame=  883 fps= 31 q=-1.0 size=   29319KiB time=00:00:29.43 bitrate=8160.3kbits/s speed=1.02x elapsed=0:00:28.92    frame=  899 fps= 31 q=-1.0 size=   29962KiB time=00:00:29.96 bitrate=8190.9kbits/s speed=1.02x elapsed=0:00:29.44    frame=  914 fps= 31 q=-1.0 size=   30538KiB time=00:00:30.46 bitrate=8211.2kbits/s speed=1.02x elapsed=0:00:29.95    frame=  929 fps= 30 q=-1.0 size=   31070KiB time=00:00:30.96 bitrate=8219.2kbits/s speed=1.02x elapsed=0:00:30.46    frame=  944 fps= 30 q=-1.0 size=   31417KiB time=00:00:31.46 bitrate=8179.2kbits/s speed=1.02x elapsed=0:00:30.97    frame=  960 fps= 30 q=-1.0 size=   31825KiB time=00:00:32.03 bitrate=8138.8kbits/s speed=1.02x elapsed=0:00:31.50    frame=  976 fps= 30 q=-1.0 size=   32381KiB time=00:00:32.53 bitrate=8153.7kbits/s speed=1.02x elapsed=0:00:32.01    frame=  991 fps= 30 q=-1.0 size=   32797KiB time=00:00:33.03 bitrate=8133.3kbits/s speed=1.02x elapsed=0:00:32.52    frame= 1007 fps= 30 q=-1.0 size=   33314KiB time=00:00:33.56 bitrate=8130.3kbits/s speed=1.02x elapsed=0:00:33.04    frame= 1023 fps= 30 q=-1.0 size=   34161KiB time=00:00:34.10 bitrate=8206.6kbits/s speed=1.02x elapsed=0:00:33.56    frame= 1037 fps= 30 q=-1.0 size=   34926KiB time=00:00:34.56 bitrate=8277.3kbits/s speed=1.01x elapsed=0:00:34.07    frame= 1053 fps= 30 q=-1.0 size=   35317KiB time=00:00:35.10 bitrate=8242.6kbits/s speed=1.01x elapsed=0:00:34.58    frame= 1068 fps= 30 q=-1.0 size=   35629KiB time=00:00:35.60 bitrate=8198.7kbits/s speed=1.01x elapsed=0:00:35.11    frame= 1084 fps= 30 q=-1.0 size=   36202KiB time=00:00:36.13 bitrate=8207.5kbits/s speed=1.01x elapsed=0:00:35.62    frame= 1100 fps= 30 q=-1.0 size=   36626KiB time=00:00:36.66 bitrate=8182.9kbits/s speed=1.01x elapsed=0:00:36.13    frame= 1115 fps= 30 q=-1.0 size=   37100KiB time=00:00:37.16 bitrate=8177.4kbits/s speed=1.01x elapsed=0:00:36.66    frame= 1131 fps= 30 q=-1.0 size=   37451KiB time=00:00:37.70 bitrate=8137.9kbits/s speed=1.01x elapsed=0:00:37.19    frame= 1146 fps= 30 q=-1.0 size=   38156KiB time=00:00:38.20 bitrate=8182.6kbits/s speed=1.01x elapsed=0:00:37.69    frame= 1162 fps= 30 q=-1.0 size=   38690KiB time=00:00:38.73 bitrate=8182.9kbits/s speed=1.01x elapsed=0:00:38.22    frame= 1178 fps= 30 q=-1.0 size=   39327KiB time=00:00:39.26 bitrate=8204.6kbits/s speed=1.01x elapsed=0:00:38.74    frame= 1193 fps= 30 q=-1.0 size=   39589KiB time=00:00:39.76 bitrate=8155.5kbits/s speed=1.01x elapsed=0:00:39.25    frame= 1209 fps= 30 q=-1.0 size=   40114KiB time=00:00:40.30 bitrate=8154.3kbits/s speed=1.01x elapsed=0:00:39.76    frame= 1224 fps= 30 q=-1.0 size=   40498KiB time=00:00:40.80 bitrate=8131.4kbits/s speed=1.01x elapsed=0:00:40.28    frame= 1239 fps= 30 q=-1.0 size=   41061KiB time=00:00:41.30 bitrate=8144.5kbits/s speed=1.01x elapsed=0:00:40.79    frame= 1255 fps= 30 q=-1.0 size=   41473KiB time=00:00:41.83 bitrate=8121.4kbits/s speed=1.01x elapsed=0:00:41.31    
~~~

### composition-r269-source2-stdout.log
~~~text
SDP:
v=0
o=- 0 0 IN IP4 127.0.0.1
s=Naraka
c=IN IP4 127.0.0.1
t=0 0
a=tool:libavformat 62.19.101
m=video 61040 RTP/AVP 96
b=AS:8013
a=framerate:30
a=rtpmap:96 H264/90000
a=fmtp:96 packetization-mode=1; sprop-parameter-sets=Z01AH5WQBQBbsBEAAAMD6AAA6mDgAAAehIAAA9CQbvKI+OFSQA==,aOuPIA==; profile-level-id=4D401F


~~~

### composition-r269-source2.log
~~~text
Input #0, mov,mp4,m4a,3gp,3g2,mj2, from 'D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4':
  Metadata:
    major_brand     : isom
    minor_version   : 512
    compatible_brands: isomiso2avc1mp41
    title           : Naraka
    encoder         : Lavf62.19.101
  Duration: 00:02:00.00, start: 0.000000, bitrate: 8295 kb/s
  Stream #0:0[0x1](eng): Video: h264 (Main) (avc1 / 0x31637661), yuv420p(progressive), 1280x720 [SAR 1:1 DAR 16:9], 8013 kb/s, 30 fps, 30 tbr, 15360 tbn (default)
    Metadata:
      handler_name    : ?Mainconcept Video Media Handler
      encoder         : Lavc62.36.101 h264_nvenc
  Stream #0:1[0x2](eng): Audio: aac (LC) (mp4a / 0x6134706D), 44100 Hz, stereo, fltp, 274 kb/s (default)
    Metadata:
      handler_name    : #Mainconcept MP4 Sound Media Handler
Stream mapping:
  Stream #0:0 -> #0:0 (copy)
Output #0, rtp, to 'rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200':
  Metadata:
    major_brand     : isom
    minor_version   : 512
    compatible_brands: isomiso2avc1mp41
    title           : Naraka
    encoder         : Lavf62.19.101
  Stream #0:0(eng): Video: h264 (Main) (avc1 / 0x31637661), yuv420p(progressive), 1280x720 [SAR 1:1 DAR 16:9], q=2-31, 8013 kb/s, 30 fps, 30 tbr, 90k tbn (default)
    Metadata:
      handler_name    : ?Mainconcept Video Media Handler
      encoder         : Lavc62.36.101 h264_nvenc
frame=   31 fps=0.0 q=-1.0 size=    1174KiB time=00:00:01.03 bitrate=9305.4kbits/s speed=1.98x elapsed=0:00:00.52    frame=   47 fps= 45 q=-1.0 size=    2281KiB time=00:00:01.56 bitrate=11929.8kbits/s speed=1.51x elapsed=0:00:01.03    frame=   62 fps= 40 q=-1.0 size=    2629KiB time=00:00:02.06 bitrate=10419.8kbits/s speed=1.33x elapsed=0:00:01.55    frame=   77 fps= 37 q=-1.0 size=    2900KiB time=00:00:02.56 bitrate=9255.1kbits/s speed=1.24x elapsed=0:00:02.06    frame=   93 fps= 36 q=-1.0 size=    3470KiB time=00:00:03.10 bitrate=9168.7kbits/s speed= 1.2x elapsed=0:00:02.57    frame=  108 fps= 35 q=-1.0 size=    3880KiB time=00:00:03.60 bitrate=8829.2kbits/s speed=1.17x elapsed=0:00:03.08    frame=  124 fps= 34 q=-1.0 size=    4406KiB time=00:00:04.13 bitrate=8731.5kbits/s speed=1.14x elapsed=0:00:03.61    frame=  139 fps= 34 q=-1.0 size=    4747KiB time=00:00:04.63 bitrate=8392.9kbits/s speed=1.12x elapsed=0:00:04.12    frame=  155 fps= 33 q=-1.0 size=    5467KiB time=00:00:05.16 bitrate=8668.9kbits/s speed=1.11x elapsed=0:00:04.64    frame=  170 fps= 33 q=-1.0 size=    5969KiB time=00:00:05.66 bitrate=8628.6kbits/s speed= 1.1x elapsed=0:00:05.15    frame=  186 fps= 33 q=-1.0 size=    6571KiB time=00:00:06.20 bitrate=8682.4kbits/s speed=1.09x elapsed=0:00:05.67    frame=  201 fps= 32 q=-1.0 size=    6860KiB time=00:00:06.70 bitrate=8387.9kbits/s speed=1.08x elapsed=0:00:06.18    frame=  216 fps= 32 q=-1.0 size=    7357KiB time=00:00:07.20 bitrate=8370.5kbits/s speed=1.07x elapsed=0:00:06.71    frame=  232 fps= 32 q=-1.0 size=    7816KiB time=00:00:07.73 bitrate=8279.2kbits/s speed=1.07x elapsed=0:00:07.22    frame=  248 fps= 32 q=-1.0 size=    8407KiB time=00:00:08.26 bitrate=8330.6kbits/s speed=1.07x elapsed=0:00:07.75    frame=  264 fps= 32 q=-1.0 size=    8849KiB time=00:00:08.80 bitrate=8238.1kbits/s speed=1.06x elapsed=0:00:08.26    frame=  278 fps= 32 q=-1.0 size=    9819KiB time=00:00:09.26 bitrate=8680.4kbits/s speed=1.06x elapsed=0:00:08.77    frame=  294 fps= 32 q=-1.0 size=   10476KiB time=00:00:09.80 bitrate=8757.4kbits/s speed=1.05x elapsed=0:00:09.30    frame=  310 fps= 32 q=-1.0 size=   10808KiB time=00:00:10.36 bitrate=8540.7kbits/s speed=1.05x elapsed=0:00:09.83    frame=  326 fps= 32 q=-1.0 size=   11079KiB time=00:00:10.86 bitrate=8352.3kbits/s speed=1.05x elapsed=0:00:10.34    frame=  341 fps= 31 q=-1.0 size=   11682KiB time=00:00:11.36 bitrate=8419.6kbits/s speed=1.05x elapsed=0:00:10.86    frame=  356 fps= 31 q=-1.0 size=   12076KiB time=00:00:11.86 bitrate=8336.6kbits/s speed=1.04x elapsed=0:00:11.37    frame=  372 fps= 31 q=-1.0 size=   12551KiB time=00:00:12.40 bitrate=8291.6kbits/s speed=1.04x elapsed=0:00:11.88    frame=  388 fps= 31 q=-1.0 size=   12936KiB time=00:00:12.93 bitrate=8193.5kbits/s speed=1.04x elapsed=0:00:12.40    frame=  403 fps= 31 q=-1.0 size=   13695KiB time=00:00:13.43 bitrate=8351.4kbits/s speed=1.04x elapsed=0:00:12.91    frame=  418 fps= 31 q=-1.0 size=   14165KiB time=00:00:13.93 bitrate=8328.3kbits/s speed=1.04x elapsed=0:00:13.43    frame=  434 fps= 31 q=-1.0 size=   14790KiB time=00:00:14.46 bitrate=8375.0kbits/s speed=1.04x elapsed=0:00:13.95    frame=  450 fps= 31 q=-1.0 size=   15085KiB time=00:00:15.00 bitrate=8238.4kbits/s speed=1.04x elapsed=0:00:14.47    frame=  465 fps= 31 q=-1.0 size=   15598KiB time=00:00:15.50 bitrate=8243.8kbits/s speed=1.03x elapsed=0:00:14.98    frame=  481 fps= 31 q=-1.0 size=   16003KiB time=00:00:16.03 bitrate=8176.7kbits/s speed=1.03x elapsed=0:00:15.50    frame=  496 fps= 31 q=-1.0 size=   16592KiB time=00:00:16.53 bitrate=8221.0kbits/s speed=1.03x elapsed=0:00:16.01    frame=  512 fps= 31 q=-1.0 size=   17340KiB time=00:00:17.06 bitrate=8323.2kbits/s speed=1.03x elapsed=0:00:16.54    frame=  528 fps= 31 q=-1.0 size=   17986KiB time=00:00:17.60 bitrate=8371.7kbits/s speed=1.03x elapsed=0:00:17.07    frame=  543 fps= 31 q=-1.0 size=   18688KiB time=00:00:18.10 bitrate=8457.9kbits/s speed=1.03x elapsed=0:00:17.58    frame=  558 fps= 31 q=-1.0 size=   18945KiB time=00:00:18.60 bitrate=8344.2kbits/s speed=1.03x elapsed=0:00:18.09    frame=  574 fps= 31 q=-1.0 size=   19319KiB time=00:00:19.13 bitrate=8271.7kbits/s speed=1.03x elapsed=0:00:18.60    frame=  589 fps= 31 q=-1.0 size=   19773KiB time=00:00:19.63 bitrate=8250.2kbits/s speed=1.03x elapsed=0:00:19.12    frame=  604 fps= 31 q=-1.0 size=   20344KiB time=00:00:20.13 bitrate=8277.8kbits/s speed=1.02x elapsed=0:00:19.64    frame=  620 fps= 31 q=-1.0 size=   20723KiB time=00:00:20.66 bitrate=8214.4kbits/s speed=1.03x elapsed=0:00:20.15    frame=  636 fps= 31 q=-1.0 size=   21288KiB time=00:00:21.20 bitrate=8226.1kbits/s speed=1.03x elapsed=0:00:20.66    frame=  651 fps= 31 q=-1.0 size=   21916KiB time=00:00:21.70 bitrate=8273.4kbits/s speed=1.02x elapsed=0:00:21.19    frame=  666 fps= 31 q=-1.0 size=   22456KiB time=00:00:22.20 bitrate=8286.4kbits/s speed=1.02x elapsed=0:00:21.70    frame=  682 fps= 31 q=-1.0 size=   22949KiB time=00:00:22.73 bitrate=8269.7kbits/s speed=1.02x elapsed=0:00:22.21    frame=  697 fps= 31 q=-1.0 size=   23303KiB time=00:00:23.23 bitrate=8216.6kbits/s speed=1.02x elapsed=0:00:22.73    frame=  713 fps= 31 q=-1.0 size=   23719KiB time=00:00:23.76 bitrate=8175.7kbits/s speed=1.02x elapsed=0:00:23.25    frame=  728 fps= 31 q=-1.0 size=   24293KiB time=00:00:24.26 bitrate=8200.9kbits/s speed=1.02x elapsed=0:00:23.77    frame=  744 fps= 31 q=-1.0 size=   24736KiB time=00:00:24.80 bitrate=8170.8kbits/s speed=1.02x elapsed=0:00:24.28    frame=  760 fps= 31 q=-1.0 size=   25225KiB time=00:00:25.33 bitrate=8157.0kbits/s speed=1.02x elapsed=0:00:24.80    frame=  774 fps= 31 q=-1.0 size=   25613KiB time=00:00:25.80 bitrate=8132.7kbits/s speed=1.02x elapsed=0:00:25.31    frame=  790 fps= 31 q=-1.0 size=   26684KiB time=00:00:26.33 bitrate=8301.1kbits/s speed=1.02x elapsed=0:00:25.82    frame=  806 fps= 31 q=-1.0 size=   27000KiB time=00:00:26.86 bitrate=8232.8kbits/s speed=1.02x elapsed=0:00:26.34    frame=  821 fps= 31 q=-1.0 size=   27374KiB time=00:00:27.36 bitrate=8194.1kbits/s speed=1.02x elapsed=0:00:26.86    frame=  837 fps= 31 q=-1.0 size=   27919KiB time=00:00:27.90 bitrate=8197.6kbits/s speed=1.02x elapsed=0:00:27.37    frame=  852 fps= 31 q=-1.0 size=   28441KiB time=00:00:28.40 bitrate=8203.9kbits/s speed=1.02x elapsed=0:00:27.88    frame=  867 fps= 31 q=-1.0 size=   28818KiB time=00:00:28.90 bitrate=8168.7kbits/s speed=1.02x elapsed=0:00:28.40    frame=  883 fps= 31 q=-1.0 size=   29319KiB time=00:00:29.43 bitrate=8160.3kbits/s speed=1.02x elapsed=0:00:28.92    frame=  899 fps= 31 q=-1.0 size=   29962KiB time=00:00:29.96 bitrate=8190.9kbits/s speed=1.02x elapsed=0:00:29.44    frame=  914 fps= 31 q=-1.0 size=   30538KiB time=00:00:30.46 bitrate=8211.2kbits/s speed=1.02x elapsed=0:00:29.95    frame=  929 fps= 30 q=-1.0 size=   31070KiB time=00:00:30.96 bitrate=8219.2kbits/s speed=1.02x elapsed=0:00:30.46    frame=  944 fps= 30 q=-1.0 size=   31417KiB time=00:00:31.46 bitrate=8179.2kbits/s speed=1.02x elapsed=0:00:30.97    frame=  961 fps= 31 q=-1.0 size=   31850KiB time=00:00:32.03 bitrate=8145.1kbits/s speed=1.02x elapsed=0:00:31.50    frame=  976 fps= 30 q=-1.0 size=   32381KiB time=00:00:32.53 bitrate=8153.7kbits/s speed=1.02x elapsed=0:00:32.01    frame=  991 fps= 30 q=-1.0 size=   32797KiB time=00:00:33.03 bitrate=8133.3kbits/s speed=1.02x elapsed=0:00:32.52    frame= 1007 fps= 30 q=-1.0 size=   33314KiB time=00:00:33.56 bitrate=8130.3kbits/s speed=1.02x elapsed=0:00:33.04    frame= 1022 fps= 30 q=-1.0 size=   34061KiB time=00:00:34.06 bitrate=8190.6kbits/s speed=1.01x elapsed=0:00:33.56    frame= 1037 fps= 30 q=-1.0 size=   34926KiB time=00:00:34.56 bitrate=8277.3kbits/s speed=1.01x elapsed=0:00:34.07    frame= 1053 fps= 30 q=-1.0 size=   35317KiB time=00:00:35.10 bitrate=8242.6kbits/s speed=1.01x elapsed=0:00:34.58    frame= 1069 fps= 30 q=-1.0 size=   35648KiB time=00:00:35.63 bitrate=8195.3kbits/s speed=1.01x elapsed=0:00:35.11    frame= 1084 fps= 30 q=-1.0 size=   36202KiB time=00:00:36.13 bitrate=8207.5kbits/s speed=1.01x elapsed=0:00:35.62    frame= 1099 fps= 30 q=-1.0 size=   36593KiB time=00:00:36.63 bitrate=8182.9kbits/s speed=1.01x elapsed=0:00:36.13    frame= 1115 fps= 30 q=-1.0 size=   37100KiB time=00:00:37.16 bitrate=8177.4kbits/s speed=1.01x elapsed=0:00:36.66    frame= 1131 fps= 30 q=-1.0 size=   37451KiB time=00:00:37.70 bitrate=8137.9kbits/s speed=1.01x elapsed=0:00:37.19    frame= 1146 fps= 30 q=-1.0 size=   38156KiB time=00:00:38.20 bitrate=8182.6kbits/s speed=1.01x elapsed=0:00:37.69    frame= 1162 fps= 30 q=-1.0 size=   38690KiB time=00:00:38.73 bitrate=8182.9kbits/s speed=1.01x elapsed=0:00:38.22    frame= 1178 fps= 30 q=-1.0 size=   39327KiB time=00:00:39.26 bitrate=8204.6kbits/s speed=1.01x elapsed=0:00:38.74    frame= 1193 fps= 30 q=-1.0 size=   39589KiB time=00:00:39.76 bitrate=8155.5kbits/s speed=1.01x elapsed=0:00:39.25    frame= 1209 fps= 30 q=-1.0 size=   40114KiB time=00:00:40.30 bitrate=8154.3kbits/s speed=1.01x elapsed=0:00:39.76    frame= 1224 fps= 30 q=-1.0 size=   40498KiB time=00:00:40.80 bitrate=8131.4kbits/s speed=1.01x elapsed=0:00:40.28    frame= 1239 fps= 30 q=-1.0 size=   41061KiB time=00:00:41.30 bitrate=8144.5kbits/s speed=1.01x elapsed=0:00:40.79    frame= 1255 fps= 30 q=-1.0 size=   41473KiB time=00:00:41.83 bitrate=8121.4kbits/s speed=1.01x elapsed=0:00:41.31    frame= 1270 fps= 30 q=-1.0 size=   42208KiB time=00:00:42.33 bitrate=8167.8kbits/s speed=1.01x elapsed=0:00:41.83    
~~~

### composition-r269-vlc.log
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
file debug: opening logfile `D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r269-vlc.log'
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
main debug: stream filter added to 03c923d8
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
main debug: resyncing on rtp://127.0.0.1:62020
main debug: rtp://127.0.0.1:62020 is at 0
main debug: creating new input thread
main debug: Creating an input for 'rtp://127.0.0.1:62020'
main debug: requesting art for new input thread
qt debug: IM: Setting an input
main debug: using timeshift granularity of 50 MiB
main debug: using timeshift path: C:\Users\ROG\AppData\Local\Temp
main debug: looking for meta fetcher module matching "any": 1 candidates
main debug: `rtp://@127.0.0.1:62020' gives access `rtp' demux `any' path `@127.0.0.1:62020'
main debug: creating demux: access='rtp' demux='any' location='@127.0.0.1:62020' file='\\@127.0.0.1:62020'
main debug: looking for access_demux module matching "rtp": 15 candidates
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\meta\fetcher
lua debug: Trying Lua scripts in D:\VideoLAN\VLC\lua\meta\fetcher
main debug: no meta fetcher modules matched
main debug: looking for art finder module matching "any": 2 candidates
main debug: net: opening 127.0.0.1 datagram port 62020
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\meta\art
lua debug: Trying Lua scripts in D:\VideoLAN\VLC\lua\meta\art
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\art\00_musicbrainz.luac
lua debug: skipping script (unmatched scope) D:\VideoLAN\VLC\lua\meta\art\00_musicbrainz.luac
main debug: using access_demux module "rtp"
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\art\01_googleimage.luac
main debug: looking for meta reader module matching "any": 2 candidates
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\meta\reader
lua debug: Trying Lua scripts in D:\VideoLAN\VLC\lua\meta\reader
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\reader\filename.luac
lua debug: skipping script (unmatched scope) D:\VideoLAN\VLC\lua\meta\art\01_googleimage.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\art\02_frenchtv.luac
main debug: no meta reader modules matched
main debug: `rtp://@127.0.0.1:62020' successfully opened
lua debug: skipping script (unmatched scope) D:\VideoLAN\VLC\lua\meta\art\02_frenchtv.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\art\03_lastfm.luac
lua debug: skipping script (unmatched scope) D:\VideoLAN\VLC\lua\meta\art\03_lastfm.luac
main debug: no art finder modules matched
main debug: accepted socket 980 (from socket 1188)
main debug: exiting
main debug: exiting
main debug: no exit handler
main debug: removing all interfaces
main debug: removing module "qt"
main debug: deactivating the playlist
main debug: incoming request - stopping current input
main debug: removing module "rtp"
main debug: dead input
main debug: nothing to play
main debug: removing module "mmdevice"
qt debug: requesting exit...
qt debug: waiting for UI thread...
qt debug: IM: Deleting the input
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
main debug: saving media library to file C:\Users\ROG\AppData\Roaming\vlc\ml.xspf.tmp14944
main debug: looking for playlist export module matching "export-xspf": 4 candidates
main debug: using playlist export module "export"
main debug: removing module "export"
main debug: deleting item `Media Library'
main debug: deleting item `rtp://127.0.0.1:62020'
main debug: deleting item `Playlist'
main debug: removing module "memory"
-- logger module stopped --

~~~

清理核验：12个本轮产物按清单删除，六个测试PID均不存在，Nsight临时session目录不存在；源文件保留。下一步转内部非阻塞readiness event状态观测，保留原同步、帧所有权和错误语义；这不是宣称外部工具已修复，也不放弃输出预算闭合。
