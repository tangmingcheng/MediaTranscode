# r268：CUDA 插桩真实链路诊断（失败）

沿用r267源码/Release，SHA256 85433A0F254B0FCAA3D07F32CABE72C9F3EB0730AA5956A2D8B2C4815814EA7C；三路120秒H264 720p30约8Mbps输入，所选AAC44100双声道；预期HEVC720p30 8000kbps CBR/AAC192kbps CBR，MPEGTS/RTP。实际启动后即失败，未完成媒体验收、失源或重入。

会话13772，VLC39276，源35584/40028/3992，nsys14100，CLI36060。CLI退出1，profiler退出1；确认CLI失败后按精确PID中止三源，退出-1，不记自然结束。VLC通过RC quit退出。本轮没有有效快照。

原始错误：IoFailure: Thread32Next failed。CUDA报告实际包含136条kernel、82条memcpy和CUDA API记录，证明当前无管理员环境可以采集CUDA，但本轮没覆盖r267长等待。报告内目标stdout未包含完整最终报告，不能证明最终payload归零。更正：本机help说明show-output默认true，但本次重定向仅得到collector输出；下一轮显式设置并继续从报告核对，不能将未转发简单归因默认关闭。

## 启动命令
~~~powershell
$ErrorActionPreference='Stop'
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r268-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r268-native- --snapshot-format=png rtp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r268-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r268-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r268-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r268-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r268-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r268-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$profiler=Start-Process -FilePath 'C:/Program Files/NVIDIA Corporation/Nsight Systems 2024.6.2/target-windows-x64/nsys.exe' -ArgumentList 'profile --trace=cuda --sample=none --cpuctxsw=none --kill=false --output=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r268-cuda D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-r268 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r268-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r268-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r268-cli-error.log' -PassThru
Write-Output "PROFILER_PID=$($profiler.Id)"
$child=$null
while(-not $child -and -not $profiler.HasExited) {
$child=Get-CimInstance Win32_Process -Filter "Name='media_transcode_realtime_video_cli.exe'" | Where-Object {$_.CommandLine -like '*composition-r268*'} | Select-Object -First 1
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
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r268-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

### Captured C:/Users/ROG/AppData/Local/Temp/nvidia/nsight_systems/quadd_session_1021940/streams/stdout_2c4a-3cb9-356b-e29c.log
~~~text
[CLI] composition_inputs=3 grid=fixed audio_source=2 audio=on max_duration=source_driven hw=planner-highest-score
[2026-10-19 15:53:17.172] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-19 15:53:17.172] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-19 15:53:19.450] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-10-19 15:53:19.450] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-10-19 15:53:19.495] [info] [graph][runtime.node] rtp_key_trace stage=clock_binder input=1 output=1
[2026-10-19 15:53:19.503] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-19 15:53:19.503] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-19 15:53:19.552] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-19 15:53:19.552] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-19 15:53:19.626] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-10-19 15:53:19.665] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-10-19 15:53:19.665] [info] [graph][planner.capability] canvas_cuda_stream stage=prepared device_stream=0000000000000000 private_nonblocking=1
[2026-10-19 15:53:19.672] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-19 15:53:19.672] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-19 15:53:19.672] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-10-19 15:53:19.678] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-19 15:53:19.678] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-19 15:53:19.678] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-10-19 15:53:19.685] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-10-19 15:53:19.685] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-10-19 15:53:19.685] [info] [graph][runtime.node] rtp_decoder_replay stage=reopened decoder=h264 source_stream=0 prior_context_destroyed=1 same_device=1 packet_bytes=8750168 owner_bytes=1350
[2026-10-19 15:53:19.702] [info] [graph][planner.capability] composition_preparation_storage stage=graph_bound scope=shared_reservations capacity=2147483648 retained=841130920 high_water=858580150
[CLI] audio_plan branch=transcode_frame reason=transcode_required_by_output_frame_contract codec=aac sample_rate=44100 channels=2 access_unit_samples=1024 encoder=aac bitrate_kbps=192
[CLI] prepared_output_id=1
[CLI] selected_chain=fixed-grid-composition score=0 decoder=h264,h264,h264 filter=scale_cuda=640:360,scale_cuda=640:360,scale_cuda=640:360 encoder=hevc_nvenc
[2026-10-19 15:53:19.702] [info] [graph][runtime.lifecycle] compile.begin
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=1 edge=1 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=2 edge=2 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=3 edge=3 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=4 edge=4 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=5 edge=5 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=6 edge=6 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=7 edge=7 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=8 edge=8 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=9 edge=9 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=10 edge=10 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=11 edge=11 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=12 edge=12 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=13 edge=13 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=14 edge=14 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=15 edge=15 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=16 edge=16 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=17 edge=17 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=18 edge=18 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=19 edge=19 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=20 edge=20 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=21 edge=21 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=22 edge=22 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=23 edge=23 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=24 edge=24 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=25 edge=25 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=26 edge=26 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=27 edge=27 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=28 edge=28 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=29 edge=29 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=30 edge=30 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=31 edge=31 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=32 edge=32 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=33 edge=33 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=34 edge=34 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=35 edge=35 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=36 edge=36 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=37 edge=37 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=38 edge=38 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=39 edge=39 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=40 edge=40 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=41 edge=41 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=42 edge=42 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=43 edge=43 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=44 edge=44 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=45 edge=45 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=46 edge=46 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=47 edge=47 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=48 edge=48 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=49 edge=49 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=50 edge=50 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=51 edge=51 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=52 edge=52 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=53 edge=53 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=54 edge=54 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=55 edge=55 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=56 edge=56 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=57 edge=57 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=58 edge=58 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=59 edge=59 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=60 edge=60 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=61 edge=61 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=62 edge=62 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=63 edge=63 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=64 edge=64 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=65 edge=65 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=66 edge=66 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=67 edge=67 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=68 edge=68 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=69 edge=69 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=70 edge=70 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=71 edge=71 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=72 edge=72 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=73 edge=73 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=74 edge=74 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=75 edge=75 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=76 edge=76 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=77 edge=77 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=78 edge=78 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=79 edge=79 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=80 edge=80 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=81 edge=81 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=82 edge=82 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=83 edge=83 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=84 edge=84 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=85 edge=85 stream=Video edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=86 edge=86 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=87 edge=87 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=88 edge=88 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=89 edge=89 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=90 edge=90 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=91 edge=91 stream=Metadata edge_kind=Metadata payload=FormatContext queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=92 edge=92 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=93 edge=93 stream=Audio edge_kind=InputPacket payload=Packet queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=94 edge=94 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=95 edge=95 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=96 edge=96 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=97 edge=97 stream=Audio edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=98 edge=98 stream=Audio edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=99 edge=99 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=100 edge=100 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=101 edge=101 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=102 edge=102 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=103 edge=103 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=104 edge=104 stream=Video edge_kind=RawFrame payload=Frame queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=105 edge=105 stream=Audio edge_kind=SoftwareFrame payload=Frame queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=106 edge=106 stream=Video edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=107 edge=107 stream=Audio edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=108 edge=108 stream=Any edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=109 edge=109 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=110 edge=110 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=111 edge=111 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=112 edge=112 stream=Metadata edge_kind=Event payload=GraphEvent queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=113 edge=113 stream=Video edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=114 edge=114 stream=Any edge_kind=EncodedPacket payload=Packet queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=115 edge=115 stream=Audio edge_kind=Metadata payload=CodecContext queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=116 edge=116 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=117 edge=117 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=118 edge=118 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=119 edge=119 stream=Any edge_kind=EncodedPacket payload=Unknown queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=120 edge=120 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=121 edge=121 stream=Metadata edge_kind=Unknown payload=Unknown queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=122 edge=122 stream=Metadata edge_kind=Unknown payload=Unknown queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.channel] create channel=123 edge=123 stream=Metadata edge_kind=Metadata payload=Unknown queued=0
[2026-10-19 15:53:19.703] [info] [graph][runtime.lifecycle] compiled nodes=82 edges=123 channels=123 execution_order=1->3->8->27->46->47->9->21->28->40->63->48->69->10->29->49->11->12->30->31->50->51->52->13->32->53->14->17->33->36->54->55->59->15->34->56->57->16->35->58->18->37->60->19->38->61->20->39->62->22->41->64->70->23->42->65->71->24->43->66->72->26->45->68->73->25->44->67->6->2->7->81->78->4->77->5->74->75->79->76->80->82
[2026-10-19 15:53:19.703] [info] [graph][runtime.lifecycle] compile.done state=DefaultRegistrationPending
[2026-10-19 15:53:19.703] [info] [graph][runtime.node] register node=1 name=composition-r268.output.video.codec_resolver kind=CodecResolver
[2026-10-19 15:53:19.703] [info] [graph][runtime.node] register node=2 name=composition-r268.output.video.encode kind=VideoEncode
[2026-10-19 15:53:19.703] [info] [graph][runtime.node] register node=3 name=composition-r268.output.audio.codec_resolver kind=AudioCodecResolver
[2026-10-19 15:53:19.703] [info] [graph][runtime.node] register node=4 name=composition-r268.output.audio.encode kind=AudioEncode
[2026-10-19 15:53:19.703] [info] [graph][runtime.node] register node=5 name=composition-r268.output.audio.canonicalizer kind=EncodedAudioCanonicalizer
[2026-10-19 15:53:19.703] [info] [graph][runtime.node] register node=8 name=composition-r268.source_0.input kind=RawRtpInput
[2026-10-19 15:53:19.703] [info] [graph][runtime.node] register node=9 name=composition-r268.source_0.rtp.clock_group kind=RtpClockGroup
[2026-10-19 15:53:19.703] [info] [graph][runtime.node] register node=10 name=composition-r268.source_0.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-10-19 15:53:19.703] [info] [graph][runtime.node] register node=11 name=composition-r268.source_0.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-10-19 15:53:19.703] [info] [graph][runtime.node] register node=12 name=composition-r268.source_0.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-10-19 15:53:19.703] [info] [graph][runtime.node] register node=13 name=composition-r268.source_0.av_sync.source_clock kind=SourceClockStateFanout
[2026-10-19 15:53:19.703] [info] [graph][runtime.node] register node=14 name=composition-r268.source_0.av_sync.video.generation_gate kind=LockedPacketGate
[2026-10-19 15:53:19.703] [info] [graph][runtime.node] register node=15 name=composition-r268.source_0.av_sync.video.canonical_input kind=CanonicalInput
[2026-10-19 15:53:19.703] [info] [graph][runtime.node] register node=16 name=composition-r268.source_0.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-10-19 15:53:19.703] [info] [graph][runtime.node] register node=17 name=composition-r268.source_0.av_sync.startup.clock kind=AvStartupClock
[2026-10-19 15:53:19.703] [info] [graph][runtime.node] register node=18 name=composition-r268.source_0.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-10-19 15:53:19.703] [info] [graph][runtime.node] register node=20 name=composition-r268.source_0.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-10-19 15:53:19.703] [info] [graph][runtime.node] register node=21 name=composition-r268.source_0.video.codec_resolver kind=CodecResolver
[2026-10-19 15:53:19.703] [info] [graph][runtime.node] register node=22 name=composition-r268.source_0.video.decode kind=VideoDecode
[2026-10-19 15:53:19.703] [info] [graph][runtime.node] register node=23 name=composition-r268.source_0.video.hwtransfer kind=HardwareTransfer
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=24 name=composition-r268.source_0.video.framerate kind=VideoFrameRate
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=25 name=composition-r268.source_0.video.preparation kind=VideoPreparation
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=26 name=composition-r268.source_0.video.filter kind=VideoFilter
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=27 name=composition-r268.source_1.input kind=RawRtpInput
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=28 name=composition-r268.source_1.rtp.clock_group kind=RtpClockGroup
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=29 name=composition-r268.source_1.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=30 name=composition-r268.source_1.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=31 name=composition-r268.source_1.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=32 name=composition-r268.source_1.av_sync.source_clock kind=SourceClockStateFanout
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=33 name=composition-r268.source_1.av_sync.video.generation_gate kind=LockedPacketGate
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=34 name=composition-r268.source_1.av_sync.video.canonical_input kind=CanonicalInput
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=35 name=composition-r268.source_1.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=36 name=composition-r268.source_1.av_sync.startup.clock kind=AvStartupClock
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=37 name=composition-r268.source_1.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=39 name=composition-r268.source_1.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=40 name=composition-r268.source_1.video.codec_resolver kind=CodecResolver
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=41 name=composition-r268.source_1.video.decode kind=VideoDecode
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=42 name=composition-r268.source_1.video.hwtransfer kind=HardwareTransfer
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=43 name=composition-r268.source_1.video.framerate kind=VideoFrameRate
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=44 name=composition-r268.source_1.video.preparation kind=VideoPreparation
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=45 name=composition-r268.source_1.video.filter kind=VideoFilter
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=46 name=composition-r268.source_2.video.input kind=RawRtpInput
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=47 name=composition-r268.source_2.audio.input kind=RawRtpInput
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=48 name=composition-r268.source_2.rtp.clock_group kind=RtpClockGroup
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=49 name=composition-r268.source_2.av_sync.rtp.clock_snapshot kind=RtpClockSnapshotFanout
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=50 name=composition-r268.source_2.av_sync.video.protocol_binder kind=RtpPacketClockBinder
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=51 name=composition-r268.source_2.av_sync.audio.protocol_binder kind=RtpPacketClockBinder
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=52 name=composition-r268.source_2.av_sync.rtp.source_clock_adapter kind=RtpSourceClockStateAdapter
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=53 name=composition-r268.source_2.av_sync.source_clock kind=SourceClockStateFanout
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=54 name=composition-r268.source_2.av_sync.video.generation_gate kind=LockedPacketGate
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=55 name=composition-r268.source_2.av_sync.audio.generation_gate kind=LockedPacketGate
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=56 name=composition-r268.source_2.av_sync.video.canonical_input kind=CanonicalInput
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=57 name=composition-r268.source_2.av_sync.audio.canonical_input kind=CanonicalInput
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=58 name=composition-r268.source_2.av_sync.startup.coordinator kind=AvStartupCoordinator
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=59 name=composition-r268.source_2.av_sync.startup.clock kind=AvStartupClock
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=60 name=composition-r268.source_2.av_sync.startup.epoch_binder kind=PlaybackEpochBinder
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=62 name=composition-r268.source_2.av_sync.startup.release_extractor kind=AvBoundReleaseExtractor
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=63 name=composition-r268.source_2.video.codec_resolver kind=CodecResolver
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=64 name=composition-r268.source_2.video.decode kind=VideoDecode
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=65 name=composition-r268.source_2.video.hwtransfer kind=HardwareTransfer
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=66 name=composition-r268.source_2.video.framerate kind=VideoFrameRate
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=67 name=composition-r268.source_2.video.preparation kind=VideoPreparation
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=68 name=composition-r268.source_2.video.filter kind=VideoFilter
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=69 name=composition-r268.source_2.audio.encode.codec_resolver kind=AudioCodecResolver
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=70 name=composition-r268.source_2.audio.encode.decode kind=AudioDecode
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=71 name=composition-r268.source_2.audio.encode.startup_trim kind=Unknown
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=72 name=composition-r268.source_2.audio.encode.drift_controller kind=AudioDriftController
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=73 name=composition-r268.source_2.audio.encode.resample kind=AudioResample
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=74 name=composition-r268.output.scheduler.scheduler kind=AvOutputScheduler
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=75 name=composition-r268.output.scheduler.router kind=ScheduledOutputRouter
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=76 name=composition-r268.output.mpegts.mux kind=FileMux
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=78 name=composition-r268.output.mpegts.plan.source kind=Unknown
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=79 name=composition-r268.output.mpegts.scheduled.adapter kind=Unknown
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=80 name=composition-r268.output.mpegts.datagram.materializer kind=MpegTsDatagramMaterializer
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=81 name=composition-r268.output.mpegts.transport.plan kind=DatagramTransportPlanSource
[2026-10-19 15:53:19.704] [info] [graph][runtime.node] register node=82 name=composition-r268.output.mpegts.transport.sender kind=ScheduledDatagramSender
[2026-10-19 15:53:19.704] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=waiting_for_random_access detail="" description="D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r268-output.sdp"
[2026-10-19 15:53:19.704] [info] [graph][runtime.lifecycle] start.begin mode=threaded
[2026-10-19 15:53:19.705] [info] [graph][runtime.lifecycle] worker.finished node=1
[2026-10-19 15:53:19.705] [info] [graph][runtime.node] worker.thread_cpu node=1 available=1 cpu_ns=0 process_calls=1
[2026-10-19 15:53:19.705] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 15:53:19.705] [info] [graph][runtime.lifecycle] worker.finished node=21
[2026-10-19 15:53:19.705] [info] [graph][runtime.node] worker.thread_cpu node=21 available=1 cpu_ns=0 process_calls=1
[2026-10-19 15:53:19.706] [info] [graph][runtime.lifecycle] start.done state=ThreadedRunning
[2026-10-19 15:53:19.706] [info] [graph][runtime.lifecycle] worker.finished node=40
[2026-10-19 15:53:19.706] [info] [graph][runtime.node] worker.thread_cpu node=40 available=1 cpu_ns=0 process_calls=1
[2026-10-19 15:53:19.706] [info] [graph][runtime.lifecycle] worker.finished node=69
[2026-10-19 15:53:19.706] [info] [graph][runtime.node] worker.thread_cpu node=69 available=1 cpu_ns=0 process_calls=1
[2026-10-19 15:53:19.707] [info] [graph][runtime.lifecycle] worker.finished node=3
[2026-10-19 15:53:19.707] [info] [graph][runtime.node] worker.thread_cpu node=3 available=1 cpu_ns=0 process_calls=1
[2026-10-19 15:53:19.707] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-10-19 15:53:19.707] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=27 generation=1
[2026-10-19 15:53:19.708] [info] [graph][runtime.lifecycle] worker.finished node=63
[2026-10-19 15:53:19.708] [info] [graph][runtime.node] worker.thread_cpu node=63 available=1 cpu_ns=0 process_calls=2
[2026-10-19 15:53:19.708] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=4198985069 dts=4198985069 size=137733 generation=1 sequence=27
[2026-10-19 15:53:19.708] [info] [graph][runtime.node] rtp_clock_group_snapshot members=video_only state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 15:53:19.709] [info] [graph][runtime.node] video_encode.bind_encoder codec=hevc_nvenc pix_fmt=cuda rate_control=cbr bitrate=8000000 min_bitrate=8000000 max_bitrate=8000000 buffer_size=267000 fps=30/1 gop=60 frame_kind=hardware hwaccel=cuda hw_device_ctx=set hw_frames_ctx=set
[2026-10-19 15:53:19.710] [info] [graph][runtime.node] rtp_clock_group_snapshot members=audio_video state=1 generation=1 invalidated_generation=none locked=1
[2026-10-19 15:53:19.710] [info] [graph][runtime.node] audio_encode_trace stage=codec_bound
[2026-10-19 15:53:19.710] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-10-19 15:53:19.710] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=28 generation=1
[2026-10-19 15:53:19.711] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=2350321029 dts=2350321029 size=137733 generation=1 sequence=28
[2026-10-19 15:53:19.712] [info] [graph][runtime.node] rtp_key_trace stage=canonical_input media=1 startup_unit=1
[2026-10-19 15:53:19.712] [info] [graph][runtime.node] av_startup_trace stage=coordinator_key sequence=28 generation=1
[2026-10-19 15:53:19.712] [info] [graph][runtime.node] video_decode_trace stage=first_packet pts=957874926 dts=957874926 size=137733 generation=1 sequence=28
[2026-10-19 15:53:19.730] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-10-19 15:53:19.731] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=4198985069 format=cuda size=1280x720 buffer_identity=0x20aa00000 drm_prime=0 software_frame=0
[2026-10-19 15:53:19.731] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=000001E58A49C730 stream=Video payload=Frame pts=4198985069 dts=4198985069 duration=3000 tb=1/90000 flags=68
[2026-10-19 15:53:19.731] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=000001E58A49C730 stream=Video payload=Frame pts=4198985069 dts=4198985069 duration=3000 tb=1/90000 flags=68
[2026-10-19 15:53:19.731] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=000001E58A49C730 stream=Video payload=Frame pts=4198985069 dts=4198985069 duration=3000 tb=1/90000 flags=68
[2026-10-19 15:53:19.731] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-10-19 15:53:19.731] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=000001E58A49C340 stream=Video payload=Frame pts=4198985069 dts=4198985069 duration=3000 tb=1/90000 flags=68
[2026-10-19 15:53:19.731] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=000001E58A49C340 stream=Video payload=Frame pts=4198985069 dts=4198985069 duration=3000 tb=1/90000 flags=68
[2026-10-19 15:53:19.731] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-10-19 15:53:19.734] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-10-19 15:53:19.734] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=2350321029 format=cuda size=1280x720 buffer_identity=0x20ae00000 drm_prime=0 software_frame=0
[2026-10-19 15:53:19.735] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=000001E58A49C730 stream=Video payload=Frame pts=2350321029 dts=2350321029 duration=3000 tb=1/90000 flags=68
[2026-10-19 15:53:19.735] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=000001E58A49C730 stream=Video payload=Frame pts=2350321029 dts=2350321029 duration=3000 tb=1/90000 flags=68
[2026-10-19 15:53:19.735] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=000001E58A49C730 stream=Video payload=Frame pts=2350321029 dts=2350321029 duration=3000 tb=1/90000 flags=68
[2026-10-19 15:53:19.735] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-10-19 15:53:19.735] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=000001E58A49DAE0 stream=Video payload=Frame pts=2350321029 dts=2350321029 duration=3000 tb=1/90000 flags=68
[2026-10-19 15:53:19.735] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=000001E58A49DAE0 stream=Video payload=Frame pts=2350321029 dts=2350321029 duration=3000 tb=1/90000 flags=68
[2026-10-19 15:53:19.735] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-10-19 15:53:19.738] [info] [graph][runtime.node] video_decode_trace stage=first_submit result=accepted
[2026-10-19 15:53:19.738] [info] [graph][runtime.node] video_decode_trace stage=first_frame pts=957874926 format=cuda size=1280x720 buffer_identity=0x20d800000 drm_prime=0 software_frame=0
[2026-10-19 15:53:19.738] [info] [graph][runtime.node] hardware_transfer.trace stage=first_input buffer=000001E5D8419230 stream=Video payload=Frame pts=957874926 dts=957874926 duration=3000 tb=1/90000 flags=68
[2026-10-19 15:53:19.738] [info] [graph][runtime.node] hardware_transfer.trace stage=first_output buffer=000001E5D8419230 stream=Video payload=Frame pts=957874926 dts=957874926 duration=3000 tb=1/90000 flags=68
[2026-10-19 15:53:19.738] [info] [graph][runtime.node] video_framerate.trace stage=first_input buffer=000001E5D8419230 stream=Video payload=Frame pts=957874926 dts=957874926 duration=3000 tb=1/90000 flags=68
[2026-10-19 15:53:19.738] [info] [graph][runtime.node] video_framerate.initialize input_tb=1/90000 target_fps=30/1 mode=control
[2026-10-19 15:53:19.738] [info] [graph][runtime.node] video_framerate.trace stage=first_output buffer=000001E5D8419620 stream=Video payload=Frame pts=957874926 dts=957874926 duration=3000 tb=1/90000 flags=68
[2026-10-19 15:53:19.738] [info] [graph][runtime.node] video_filter.trace stage=first_input buffer=000001E5D8419620 stream=Video payload=Frame pts=957874926 dts=957874926 duration=3000 tb=1/90000 flags=68
[2026-10-19 15:53:19.738] [info] [graph][runtime.node] video_filter.trace stage=initialize_begin input_fmt=cuda input_size=1280x720
[2026-10-19 15:53:19.768] [info] [graph][runtime.lifecycle] stop.begin
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=27 available=1 cpu_ns=0 process_calls=42
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=46 available=1 cpu_ns=0 process_calls=25
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=9 available=1 cpu_ns=0 process_calls=3
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=47 available=1 cpu_ns=0 process_calls=24
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=28 available=1 cpu_ns=0 process_calls=3
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=8 available=1 cpu_ns=0 process_calls=30
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=12 available=1 cpu_ns=0 process_calls=3
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=49 available=1 cpu_ns=0 process_calls=4
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=48 available=1 cpu_ns=0 process_calls=3
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=10 available=1 cpu_ns=0 process_calls=3
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=30 available=1 cpu_ns=0 process_calls=97
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=11 available=1 cpu_ns=15625000 process_calls=95
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=29 available=1 cpu_ns=0 process_calls=4
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=52 available=1 cpu_ns=0 process_calls=4
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=51 available=1 cpu_ns=0 process_calls=142
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=13 available=1 cpu_ns=0 process_calls=4
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=50 available=1 cpu_ns=0 process_calls=160
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=17 available=1 cpu_ns=0 process_calls=3
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=33 available=1 cpu_ns=0 process_calls=94
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=36 available=1 cpu_ns=0 process_calls=4
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=55 available=1 cpu_ns=0 process_calls=153
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=31 available=1 cpu_ns=0 process_calls=3
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=59 available=1 cpu_ns=0 process_calls=4
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=14 available=1 cpu_ns=15625000 process_calls=118
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=32 available=1 cpu_ns=0 process_calls=5
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=15 available=1 cpu_ns=15625000 process_calls=75
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=53 available=1 cpu_ns=0 process_calls=5
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=61 available=1 cpu_ns=0 process_calls=3
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=34 available=1 cpu_ns=0 process_calls=105
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=62 available=1 cpu_ns=0 process_calls=55
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=42 available=1 cpu_ns=0 process_calls=25
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=23 available=1 cpu_ns=0 process_calls=29
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=57 available=1 cpu_ns=0 process_calls=143
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=58 available=1 cpu_ns=0 process_calls=165
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=37 available=1 cpu_ns=0 process_calls=9
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=67 available=1 cpu_ns=0 process_calls=1
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=4 available=1 cpu_ns=0 process_calls=3
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=60 available=1 cpu_ns=15625000 process_calls=8
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=80 available=1 cpu_ns=0 process_calls=1
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=38 available=1 cpu_ns=0 process_calls=4
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=39 available=1 cpu_ns=0 process_calls=26
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=70 available=1 cpu_ns=0 process_calls=2
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=16 available=1 cpu_ns=0 process_calls=73
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=35 available=1 cpu_ns=0 process_calls=74
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=65 available=1 cpu_ns=0 process_calls=29
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=56 available=1 cpu_ns=0 process_calls=157
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=18 available=1 cpu_ns=0 process_calls=9
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=24 available=1 cpu_ns=0 process_calls=19
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=20 available=1 cpu_ns=0 process_calls=27
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=72 available=1 cpu_ns=0 process_calls=1
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=66 available=1 cpu_ns=0 process_calls=19
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=25 available=1 cpu_ns=0 process_calls=1
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=6 available=1 cpu_ns=0 process_calls=1
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=44 available=1 cpu_ns=0 process_calls=1
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=2 available=1 cpu_ns=0 process_calls=3
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=7 available=1 cpu_ns=0 process_calls=1
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=77 available=1 cpu_ns=0 process_calls=1
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=43 available=1 cpu_ns=0 process_calls=17
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=74 available=1 cpu_ns=0 process_calls=1
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=5 available=1 cpu_ns=0 process_calls=1
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=78 available=1 cpu_ns=0 process_calls=1
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=73 available=1 cpu_ns=0 process_calls=1
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=54 available=1 cpu_ns=0 process_calls=180
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=75 available=1 cpu_ns=0 process_calls=1
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=81 available=1 cpu_ns=0 process_calls=1
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=79 available=1 cpu_ns=0 process_calls=1
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=19 available=1 cpu_ns=0 process_calls=2
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=82 available=1 cpu_ns=0 process_calls=1
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=76 available=1 cpu_ns=0 process_calls=3
[2026-10-19 15:53:19.768] [info] [graph][runtime.node] worker.thread_cpu node=71 available=1 cpu_ns=0 process_calls=1
[2026-10-19 15:53:19.806] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=67
[2026-10-19 15:53:19.806] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=74
[2026-10-19 15:53:19.811] [info] [graph][runtime.node] video_filter.initialize input_tb=1/90000 input_fps=30/1 sink_tb=1/90000 output_tb=1/30 input_fmt=cuda input_size=1280x720 hardware_source=true planner_filter=scale_cuda=640:360 desc=scale_cuda=640:360 allocation_authority= completion_authority= initialize_elapsed_ms=76
[2026-10-19 15:53:19.811] [info] [graph][runtime.node] process.failed node=45 name=VideoFilterNode kind=VideoFilter inputs=[channel=51 edge=51 stream=Video edge_kind=Metadata payload=CodecContext queued=0; channel=54 edge=54 stream=Video edge_kind=RawFrame payload=Frame queued=6] outputs=[channel=55 edge=55 stream=Video edge_kind=RawFrame payload=Frame queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-19 15:53:19.811] [info] [graph][runtime.node] worker.thread_cpu node=45 available=1 cpu_ns=31250000 process_calls=3
[2026-10-19 15:53:19.811] [info] [graph][runtime.node] process.failed node=22 name=VideoDecodeNode kind=VideoDecode inputs=[channel=22 edge=22 stream=Video edge_kind=Metadata payload=CodecContext queued=0; channel=23 edge=23 stream=Video edge_kind=InputPacket payload=Packet queued=4] outputs=[channel=25 edge=25 stream=Video edge_kind=RawFrame payload=Frame queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-19 15:53:19.811] [info] [graph][runtime.node] worker.thread_cpu node=22 available=1 cpu_ns=15625000 process_calls=12
[2026-10-19 15:53:19.811] [info] [graph][runtime.node] process.failed node=26 name=VideoFilterNode kind=VideoFilter inputs=[channel=24 edge=24 stream=Video edge_kind=Metadata payload=CodecContext queued=0; channel=27 edge=27 stream=Video edge_kind=RawFrame payload=Frame queued=6] outputs=[channel=28 edge=28 stream=Video edge_kind=RawFrame payload=Frame queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-19 15:53:19.811] [info] [graph][runtime.node] worker.thread_cpu node=26 available=1 cpu_ns=31250000 process_calls=3
[2026-10-19 15:53:19.812] [info] [graph][runtime.node] process.failed node=41 name=VideoDecodeNode kind=VideoDecode inputs=[channel=49 edge=49 stream=Video edge_kind=Metadata payload=CodecContext queued=0; channel=50 edge=50 stream=Video edge_kind=InputPacket payload=Packet queued=6] outputs=[channel=52 edge=52 stream=Video edge_kind=RawFrame payload=Frame queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-19 15:53:19.812] [info] [graph][runtime.node] worker.thread_cpu node=41 available=1 cpu_ns=0 process_calls=11
[2026-10-19 15:53:19.812] [info] [graph][runtime.node] process.failed node=68 name=VideoFilterNode kind=VideoFilter inputs=[channel=86 edge=86 stream=Video edge_kind=Metadata payload=CodecContext queued=0; channel=89 edge=89 stream=Video edge_kind=RawFrame payload=Frame queued=6] outputs=[channel=90 edge=90 stream=Video edge_kind=RawFrame payload=Frame queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-19 15:53:19.812] [info] [graph][runtime.node] worker.thread_cpu node=68 available=1 cpu_ns=15625000 process_calls=3
[2026-10-19 15:53:19.812] [info] [graph][runtime.node] process.failed node=64 name=VideoDecodeNode kind=VideoDecode inputs=[channel=84 edge=84 stream=Video edge_kind=Metadata payload=CodecContext queued=0; channel=85 edge=85 stream=Video edge_kind=InputPacket payload=Packet queued=31] outputs=[channel=87 edge=87 stream=Video edge_kind=RawFrame payload=Frame queued=0] error=Cancelled: graph payload credit waiters were cancelled
[2026-10-19 15:53:19.812] [info] [graph][runtime.node] worker.thread_cpu node=64 available=1 cpu_ns=0 process_calls=13
[2026-10-19 15:53:19.812] [info] [graph][runtime.node] mpegts_wire_materializer stage=stopped maximum_materialized_after_release_ns=0 worst_materialized_at_ns=0 worst_release_ns=0 worst_deadline_ns=0 worst_global_sequence=0
[2026-10-19 15:53:19.812] [info] [graph][runtime.lifecycle] abort.begin
[2026-10-19 15:53:19.813] [info] [graph][runtime.node] scheduled_datagram_sender stage=aborted generation=0 service_scope= execution_mode=1 committed_batches=0 committed_datagrams=0 committed_payload_bytes=0 would_block=0 writable_waits=0 deadline_misses=0 pressure_failures=0 partial_submitted_failures=0 ambiguous_submitted_failures=0 ordered_queue_maximum_batches=0 ordered_queue_maximum_datagrams=0 ordered_queue_maximum_wire_bytes=0 pacing_reserved=0 pacing_cancelled=0 pacing_submitted=0 pacing_maximum_submit_lateness_ns=0 pacing_rate_adaptations=0 pacing_capacity_limited_reservations=0 pacing_maximum_wire_bytes_per_second=0 delivery_evidence=not_proven shared_scope_charged_datagrams=0 shared_scope_charged_wire_bytes=0 shared_scope_waiting_members=0 shared_scope_failed=0
[2026-10-19 15:53:19.813] [info] [graph][runtime.lifecycle] node_abort_payload node=82 before_objects=247 after_objects=247 before_bytes=5276328 after_bytes=5276328 before_reservations=394 after_reservations=394 before_releases=147 after_releases=147
[2026-10-19 15:53:19.813] [info] [graph][runtime.node] mpegts_wire_materializer stage=aborted maximum_materialized_after_release_ns=0 worst_materialized_at_ns=0 worst_release_ns=0 worst_deadline_ns=0 worst_global_sequence=0
[2026-10-19 15:53:19.813] [info] [graph][runtime.lifecycle] node_abort_payload node=80 before_objects=247 after_objects=247 before_bytes=5276328 after_bytes=5276328 before_reservations=394 after_reservations=394 before_releases=147 after_releases=147
[2026-10-19 15:53:19.813] [info] [graph][runtime.lifecycle] node_abort_payload node=76 before_objects=247 after_objects=247 before_bytes=5276328 after_bytes=5276328 before_reservations=394 after_reservations=394 before_releases=147 after_releases=147
[2026-10-19 15:53:19.813] [info] [graph][runtime.lifecycle] node_abort_payload node=79 before_objects=247 after_objects=247 before_bytes=5276328 after_bytes=5276328 before_reservations=394 after_reservations=394 before_releases=147 after_releases=147
[2026-10-19 15:53:19.813] [info] [graph][runtime.lifecycle] node_abort_payload node=75 before_objects=247 after_objects=247 before_bytes=5276328 after_bytes=5276328 before_reservations=394 after_reservations=394 before_releases=147 after_releases=147
[2026-10-19 15:53:19.813] [info] [graph][runtime.lifecycle] node_abort_payload node=74 before_objects=247 after_objects=247 before_bytes=5276328 after_bytes=5276328 before_reservations=394 after_reservations=394 before_releases=147 after_releases=147
[2026-10-19 15:53:19.813] [info] [graph][runtime.lifecycle] node_abort_payload node=5 before_objects=247 after_objects=247 before_bytes=5276328 after_bytes=5276328 before_reservations=394 after_reservations=394 before_releases=147 after_releases=147
[2026-10-19 15:53:19.813] [info] [graph][runtime.lifecycle] node_abort_payload node=77 before_objects=247 after_objects=247 before_bytes=5276328 after_bytes=5276328 before_reservations=394 after_reservations=394 before_releases=147 after_releases=147
[2026-10-19 15:53:19.813] [info] [graph][runtime.lifecycle] codec_owner_release node=4 kind=AudioEncode owner_references=4
[2026-10-19 15:53:19.813] [info] [graph][runtime.lifecycle] node_abort_payload node=4 before_objects=247 after_objects=247 before_bytes=5276328 after_bytes=5276328 before_reservations=394 after_reservations=394 before_releases=147 after_releases=147
[2026-10-19 15:53:19.813] [info] [graph][runtime.lifecycle] node_abort_payload node=78 before_objects=247 after_objects=247 before_bytes=5276328 after_bytes=5276328 before_reservations=394 after_reservations=394 before_releases=147 after_releases=147
[2026-10-19 15:53:19.813] [info] [graph][runtime.lifecycle] node_abort_payload node=81 before_objects=247 after_objects=247 before_bytes=5276328 after_bytes=5276328 before_reservations=394 after_reservations=394 before_releases=147 after_releases=147
[2026-10-19 15:53:19.813] [info] [graph][runtime.node] continuous_audio_aggregate.summary reason=abort output_group=composition-r268.output audio_commits=0 real_samples=0 silence_samples=0
[2026-10-19 15:53:19.813] [info] [graph][runtime.lifecycle] node_abort_payload node=7 before_objects=247 after_objects=247 before_bytes=5276328 after_bytes=5276328 before_reservations=394 after_reservations=394 before_releases=147 after_releases=147
[2026-10-19 15:53:19.813] [info] [graph][runtime.lifecycle] codec_owner_release node=2 kind=VideoEncode owner_references=4
[2026-10-19 15:53:19.813] [info] [graph][runtime.lifecycle] node_abort_payload node=2 before_objects=247 after_objects=247 before_bytes=5276328 after_bytes=5276328 before_reservations=394 after_reservations=394 before_releases=147 after_releases=147
[2026-10-19 15:53:19.813] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=process count=1 maximum_ns=15700 begin_steady_ns=25813626471400 end_steady_ns=25813626487100 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=waiting deadline_ns=unavailable pending_at_entry=0 entered_onprocess=1
[2026-10-19 15:53:19.813] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=purge count=1 maximum_ns=500 begin_steady_ns=25813626471400 end_steady_ns=25813626471900 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 15:53:19.813] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=bind count=1 maximum_ns=5400 begin_steady_ns=25813626473600 end_steady_ns=25813626479000 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 15:53:19.813] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=consume count=1 maximum_ns=6000 begin_steady_ns=25813626479100 end_steady_ns=25813626485100 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 15:53:19.813] [info] [graph][runtime.node] aggregate_execution node=6 generation=1 stage=activate count=1 maximum_ns=1700 begin_steady_ns=25813626485200 end_steady_ns=25813626486900 video_cursor=0 audio_cursor=not_owned next_video_cursor=0 next_audio_cursor=not_owned error=None return_state=unavailable deadline_ns=unavailable pending_at_entry=0 entered_onprocess=0
[2026-10-19 15:53:19.813] [info] [graph][runtime.node] continuous_aggregate.summary reason=abort output_group=composition-r268.output output_generation=1 video_commits=0 source[0].group=composition-r268.source.0 source[0].real_frames=0 source[0].black_frames=0 source[0].contribution_generation=0 source[1].group=composition-r268.source.1 source[1].real_frames=0 source[1].black_frames=0 source[1].contribution_generation=0 source[2].group=composition-r268.source.2 source[2].real_frames=0 source[2].black_frames=0 source[2].contribution_generation=0
[2026-10-19 15:53:19.813] [info] [graph][runtime.node] cuda_canvas_call stage=context_push count=4 operation=upload maximum_ns=3400 begin_steady_ns=25813582432100 end_steady_ns=25813582435500 native_result=0 thread_cpu_ns=0
[2026-10-19 15:53:19.813] [info] [graph][runtime.node] cuda_canvas_call stage=source_wait count=6 operation=copy maximum_ns=5800 begin_steady_ns=25813583916500 end_steady_ns=25813583922300 native_result=0 thread_cpu_ns=0
[2026-10-19 15:53:19.813] [info] [graph][runtime.node] cuda_canvas_call stage=plane_copy count=14 operation=download maximum_ns=291200 begin_steady_ns=25813582804800 end_steady_ns=25813583096000 native_result=0 thread_cpu_ns=0
[2026-10-19 15:53:19.813] [info] [graph][runtime.node] cuda_canvas_call stage=destination_wait count=2 operation=upload maximum_ns=86300 begin_steady_ns=25813582701400 end_steady_ns=25813582787700 native_result=0 thread_cpu_ns=0
[2026-10-19 15:53:19.813] [info] [graph][runtime.node] cuda_canvas_call stage=context_pop count=4 operation=copy maximum_ns=2000 begin_steady_ns=25813583767800 end_steady_ns=25813583769800 native_result=0 thread_cpu_ns=0
[2026-10-19 15:53:19.813] [info] [graph][runtime.node] cuda_canvas_call stage=batch_completion count=2 operation=copy maximum_ns=106900 begin_steady_ns=25813583660800 end_steady_ns=25813583767700 native_result=0 thread_cpu_ns=0
[2026-10-19 15:53:19.813] [info] [graph][runtime.lifecycle] node_abort_payload node=6 before_objects=247 after_objects=247 before_bytes=5276328 after_bytes=5276328 before_reservations=394 after_reservations=394 before_releases=147 after_releases=147
[2026-10-19 15:53:19.813] [info] [graph][runtime.lifecycle] node_abort_payload node=67 before_objects=247 after_objects=247 before_bytes=5276328 after_bytes=5276328 before_reservations=394 after_reservations=394 before_releases=147 after_releases=147
[2026-10-19 15:53:19.813] [info] [graph][runtime.lifecycle] node_abort_payload node=44 before_objects=247 after_objects=247 before_bytes=5276328 after_bytes=5276328 before_reservations=394 after_reservations=394 before_releases=147 after_releases=147
[2026-10-19 15:53:19.813] [info] [graph][runtime.lifecycle] node_abort_payload node=25 before_objects=247 after_objects=247 before_bytes=5276328 after_bytes=5276328 before_reservations=394 after_reservations=394 before_releases=147 after_releases=147
[2026-10-19 15:53:19.813] [info] [graph][runtime.lifecycle] codec_owner_release node=73 kind=AudioResample owner_references=1
[2026-10-19 15:53:19.813] [info] [graph][runtime.lifecycle] node_abort_payload node=73 before_objects=247 after_objects=247 before_bytes=5276328 after_bytes=5276328 before_reservations=394 after_reservations=394 before_releases=147 after_releases=147
[2026-10-19 15:53:19.814] [info] [graph][runtime.lifecycle] node_abort_payload node=68 before_objects=247 after_objects=246 before_bytes=5276328 after_bytes=5276328 before_reservations=394 after_reservations=394 before_releases=147 after_releases=148
[2026-10-19 15:53:19.816] [info] [graph][runtime.lifecycle] node_abort_payload node=45 before_objects=246 after_objects=245 before_bytes=5276328 after_bytes=5276328 before_reservations=394 after_reservations=394 before_releases=148 after_releases=149
[2026-10-19 15:53:19.818] [info] [graph][runtime.lifecycle] node_abort_payload node=26 before_objects=245 after_objects=244 before_bytes=5276328 after_bytes=5276328 before_reservations=394 after_reservations=394 before_releases=149 after_releases=150
[2026-10-19 15:53:19.818] [info] [graph][runtime.lifecycle] node_abort_payload node=72 before_objects=244 after_objects=244 before_bytes=5276328 after_bytes=5276328 before_reservations=394 after_reservations=394 before_releases=150 after_releases=150
[2026-10-19 15:53:19.818] [info] [graph][runtime.lifecycle] node_abort_payload node=66 before_objects=244 after_objects=243 before_bytes=5276328 after_bytes=5276328 before_reservations=394 after_reservations=394 before_releases=150 after_releases=151
[2026-10-19 15:53:19.818] [info] [graph][runtime.lifecycle] node_abort_payload node=43 before_objects=243 after_objects=242 before_bytes=5276328 after_bytes=5276328 before_reservations=394 after_reservations=394 before_releases=151 after_releases=152
[2026-10-19 15:53:19.818] [info] [graph][runtime.lifecycle] node_abort_payload node=24 before_objects=242 after_objects=241 before_bytes=5276328 after_bytes=5276328 before_reservations=394 after_reservations=394 before_releases=152 after_releases=153
[2026-10-19 15:53:19.818] [info] [graph][runtime.lifecycle] node_abort_payload node=71 before_objects=241 after_objects=241 before_bytes=5276328 after_bytes=5276328 before_reservations=394 after_reservations=394 before_releases=153 after_releases=153
[2026-10-19 15:53:19.818] [info] [graph][runtime.lifecycle] node_abort_payload node=65 before_objects=241 after_objects=241 before_bytes=5276328 after_bytes=5276328 before_reservations=394 after_reservations=394 before_releases=153 after_releases=153
[2026-10-19 15:53:19.818] [info] [graph][runtime.lifecycle] node_abort_payload node=42 before_objects=241 after_objects=241 before_bytes=5276328 after_bytes=5276328 before_reservations=394 after_reservations=394 before_releases=153 after_releases=153
[2026-10-19 15:53:19.818] [info] [graph][runtime.lifecycle] node_abort_payload node=23 before_objects=241 after_objects=241 before_bytes=5276328 after_bytes=5276328 before_reservations=394 after_reservations=394 before_releases=153 after_releases=153
[2026-10-19 15:53:19.818] [info] [graph][runtime.lifecycle] codec_owner_release node=70 kind=AudioDecode owner_references=2
[2026-10-19 15:53:19.818] [info] [graph][runtime.lifecycle] node_abort_payload node=70 before_objects=241 after_objects=241 before_bytes=5276328 after_bytes=5276328 before_reservations=394 after_reservations=394 before_releases=153 after_releases=153
[2026-10-19 15:53:19.818] [info] [graph][runtime.lifecycle] codec_owner_release node=64 kind=VideoDecode owner_references=2
[2026-10-19 15:53:19.825] [info] [graph][runtime.lifecycle] node_abort_payload node=64 before_objects=241 after_objects=241 before_bytes=5276328 after_bytes=5276328 before_reservations=394 after_reservations=394 before_releases=153 after_releases=153
[2026-10-19 15:53:19.825] [info] [graph][runtime.lifecycle] codec_owner_release node=41 kind=VideoDecode owner_references=2
[2026-10-19 15:53:19.830] [info] [graph][runtime.lifecycle] node_abort_payload node=41 before_objects=241 after_objects=241 before_bytes=5276328 after_bytes=5276328 before_reservations=394 after_reservations=394 before_releases=153 after_releases=153
[2026-10-19 15:53:19.830] [info] [graph][runtime.lifecycle] codec_owner_release node=22 kind=VideoDecode owner_references=2
[2026-10-19 15:53:19.836] [info] [graph][runtime.lifecycle] node_abort_payload node=22 before_objects=241 after_objects=241 before_bytes=5276328 after_bytes=5276328 before_reservations=394 after_reservations=394 before_releases=153 after_releases=153
[2026-10-19 15:53:19.836] [info] [graph][runtime.lifecycle] node_abort_payload node=62 before_objects=241 after_objects=241 before_bytes=5276328 after_bytes=5276328 before_reservations=394 after_reservations=394 before_releases=153 after_releases=153
[2026-10-19 15:53:19.836] [info] [graph][runtime.lifecycle] node_abort_payload node=39 before_objects=241 after_objects=241 before_bytes=5276328 after_bytes=5276328 before_reservations=394 after_reservations=394 before_releases=153 after_releases=153
[2026-10-19 15:53:19.836] [info] [graph][runtime.lifecycle] node_abort_payload node=20 before_objects=241 after_objects=241 before_bytes=5276328 after_bytes=5276328 before_reservations=394 after_reservations=394 before_releases=153 after_releases=153
[2026-10-19 15:53:19.836] [info] [graph][runtime.lifecycle] node_abort_payload node=61 before_objects=241 after_objects=208 before_bytes=5276328 after_bytes=4954821 before_reservations=394 after_reservations=394 before_releases=153 after_releases=186
[2026-10-19 15:53:19.836] [info] [graph][runtime.lifecycle] node_abort_payload node=38 before_objects=208 after_objects=199 before_bytes=4954821 after_bytes=4683629 before_reservations=394 after_reservations=394 before_releases=186 after_releases=195
[2026-10-19 15:53:19.836] [info] [graph][runtime.lifecycle] node_abort_payload node=19 before_objects=199 after_objects=188 before_bytes=4683629 after_bytes=4382700 before_reservations=394 after_reservations=394 before_releases=195 after_releases=206
[2026-10-19 15:53:19.836] [info] [graph][runtime.lifecycle] node_abort_payload node=60 before_objects=188 after_objects=187 before_bytes=4382700 after_bytes=4381831 before_reservations=394 after_reservations=394 before_releases=206 after_releases=207
[2026-10-19 15:53:19.836] [info] [graph][runtime.lifecycle] node_abort_payload node=37 before_objects=187 after_objects=186 before_bytes=4381831 after_bytes=4340399 before_reservations=394 after_reservations=394 before_releases=207 after_releases=208
[2026-10-19 15:53:19.836] [info] [graph][runtime.lifecycle] node_abort_payload node=18 before_objects=186 after_objects=185 before_bytes=4340399 after_bytes=4298967 before_reservations=394 after_reservations=394 before_releases=208 after_releases=209
[2026-10-19 15:53:19.836] [info] [graph][runtime.lifecycle] node_abort_payload node=58 before_objects=185 after_objects=184 before_bytes=4298967 after_bytes=4297994 before_reservations=394 after_reservations=394 before_releases=209 after_releases=210
[2026-10-19 15:53:19.836] [info] [graph][runtime.lifecycle] node_abort_payload node=35 before_objects=184 after_objects=183 before_bytes=4297994 after_bytes=4263938 before_reservations=394 after_reservations=394 before_releases=210 after_releases=211
[2026-10-19 15:53:19.836] [info] [graph][runtime.lifecycle] node_abort_payload node=16 before_objects=183 after_objects=182 before_bytes=4263938 after_bytes=4229882 before_reservations=394 after_reservations=394 before_releases=211 after_releases=212
[2026-10-19 15:53:19.836] [info] [graph][runtime.lifecycle] node_abort_payload node=57 before_objects=182 after_objects=181 before_bytes=4229882 after_bytes=4228805 before_reservations=394 after_reservations=394 before_releases=212 after_releases=213
[2026-10-19 15:53:19.836] [info] [graph][runtime.lifecycle] node_abort_payload node=56 before_objects=181 after_objects=180 before_bytes=4228805 after_bytes=4202868 before_reservations=394 after_reservations=394 before_releases=213 after_releases=214
[2026-10-19 15:53:19.836] [info] [graph][runtime.lifecycle] node_abort_payload node=34 before_objects=180 after_objects=179 before_bytes=4202868 after_bytes=4172998 before_reservations=394 after_reservations=394 before_releases=214 after_releases=215
[2026-10-19 15:53:19.836] [info] [graph][runtime.lifecycle] node_abort_payload node=15 before_objects=179 after_objects=178 before_bytes=4172998 after_bytes=4143128 before_reservations=394 after_reservations=394 before_releases=215 after_releases=216
[2026-10-19 15:53:19.836] [info] [graph][runtime.lifecycle] node_abort_payload node=59 before_objects=178 after_objects=178 before_bytes=4143128 after_bytes=4143128 before_reservations=394 after_reservations=394 before_releases=216 after_releases=216
[2026-10-19 15:53:19.836] [info] [graph][runtime.lifecycle] node_abort_payload node=55 before_objects=178 after_objects=177 before_bytes=4143128 after_bytes=4142019 before_reservations=394 after_reservations=394 before_releases=216 after_releases=217
[2026-10-19 15:53:19.836] [info] [graph][runtime.lifecycle] node_abort_payload node=54 before_objects=177 after_objects=176 before_bytes=4142019 after_bytes=4116280 before_reservations=394 after_reservations=394 before_releases=217 after_releases=218
[2026-10-19 15:53:19.836] [info] [graph][runtime.lifecycle] node_abort_payload node=36 before_objects=176 after_objects=176 before_bytes=4116280 after_bytes=4116280 before_reservations=394 after_reservations=394 before_releases=218 after_releases=218
[2026-10-19 15:53:19.836] [info] [graph][runtime.lifecycle] node_abort_payload node=33 before_objects=176 after_objects=175 before_bytes=4116280 after_bytes=4096606 before_reservations=394 after_reservations=394 before_releases=218 after_releases=219
[2026-10-19 15:53:19.836] [info] [graph][runtime.lifecycle] node_abort_payload node=17 before_objects=175 after_objects=175 before_bytes=4096606 after_bytes=4096606 before_reservations=394 after_reservations=394 before_releases=219 after_releases=219
[2026-10-19 15:53:19.836] [info] [graph][runtime.lifecycle] node_abort_payload node=14 before_objects=175 after_objects=174 before_bytes=4096606 after_bytes=4076932 before_reservations=394 after_reservations=394 before_releases=219 after_releases=220
[2026-10-19 15:53:19.836] [info] [graph][runtime.lifecycle] node_abort_payload node=53 before_objects=174 after_objects=174 before_bytes=4076932 after_bytes=4076932 before_reservations=394 after_reservations=394 before_releases=220 after_releases=220
[2026-10-19 15:53:19.836] [info] [graph][runtime.lifecycle] node_abort_payload node=32 before_objects=174 after_objects=174 before_bytes=4076932 after_bytes=4076932 before_reservations=394 after_reservations=394 before_releases=220 after_releases=220
[2026-10-19 15:53:19.836] [info] [graph][runtime.lifecycle] node_abort_payload node=13 before_objects=174 after_objects=174 before_bytes=4076932 after_bytes=4076932 before_reservations=394 after_reservations=394 before_releases=220 after_releases=220
[2026-10-19 15:53:19.836] [info] [graph][runtime.lifecycle] node_abort_payload node=52 before_objects=174 after_objects=174 before_bytes=4076932 after_bytes=4076932 before_reservations=394 after_reservations=394 before_releases=220 after_releases=220
[2026-10-19 15:53:19.837] [info] [graph][runtime.lifecycle] node_abort_payload node=51 before_objects=174 after_objects=173 before_bytes=4076932 after_bytes=4075906 before_reservations=394 after_reservations=394 before_releases=220 after_releases=221
[2026-10-19 15:53:19.837] [info] [graph][runtime.lifecycle] node_abort_payload node=50 before_objects=173 after_objects=172 before_bytes=4075906 after_bytes=4046732 before_reservations=394 after_reservations=394 before_releases=221 after_releases=222
[2026-10-19 15:53:19.837] [info] [graph][runtime.lifecycle] node_abort_payload node=31 before_objects=172 after_objects=172 before_bytes=4046732 after_bytes=4046732 before_reservations=394 after_reservations=394 before_releases=222 after_releases=222
[2026-10-19 15:53:19.837] [info] [graph][runtime.lifecycle] node_abort_payload node=30 before_objects=172 after_objects=171 before_bytes=4046732 after_bytes=4024060 before_reservations=394 after_reservations=394 before_releases=222 after_releases=223
[2026-10-19 15:53:19.837] [info] [graph][runtime.lifecycle] node_abort_payload node=12 before_objects=171 after_objects=171 before_bytes=4024060 after_bytes=4024060 before_reservations=394 after_reservations=394 before_releases=223 after_releases=223
[2026-10-19 15:53:19.837] [info] [graph][runtime.lifecycle] node_abort_payload node=11 before_objects=171 after_objects=170 before_bytes=4024060 after_bytes=4001388 before_reservations=394 after_reservations=394 before_releases=223 after_releases=224
[2026-10-19 15:53:19.837] [info] [graph][runtime.lifecycle] node_abort_payload node=49 before_objects=170 after_objects=170 before_bytes=4001388 after_bytes=4001388 before_reservations=394 after_reservations=394 before_releases=224 after_releases=224
[2026-10-19 15:53:19.837] [info] [graph][runtime.lifecycle] node_abort_payload node=29 before_objects=170 after_objects=170 before_bytes=4001388 after_bytes=4001388 before_reservations=394 after_reservations=394 before_releases=224 after_releases=224
[2026-10-19 15:53:19.837] [info] [graph][runtime.lifecycle] node_abort_payload node=10 before_objects=170 after_objects=170 before_bytes=4001388 after_bytes=4001388 before_reservations=394 after_reservations=394 before_releases=224 after_releases=224
[2026-10-19 15:53:19.837] [info] [graph][runtime.lifecycle] node_abort_payload node=69 before_objects=170 after_objects=170 before_bytes=4001388 after_bytes=40013
~~~

### Captured C:/Users/ROG/AppData/Local/Temp/nvidia/nsight_systems/quadd_session_1021940/streams/stderr_2c4a-3cb9-356b-e29c.log
~~~text
[in @ 000001e58a45f840] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001e58a45f840] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 46655.389656
[in @ 000001e58aa68500] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001e58aa68500] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 10643.054733
[in @ 000001e58aae6c00] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001e58aae6c00] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 26114.6781
[in @ 000001e5d8422c80] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001e5d8422c80] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 10643.054733
[in @ 000001e5ef6fd2c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001e5ef6fd2c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 46655.389656
[in @ 000001e58aa681c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001e58aa681c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 26114.6781
[aac @ 000001e58a464e00] Qavg: nan
[CLI] realtime video runtime failed: IoFailure: Thread32Next failed

~~~

### Captured C:/Program Files/NVIDIA Corporation/Nsight Systems 2024.6.2/target-windows-x64/config.ini
~~~text

~~~

## 产物清单与原始日志
composition-r268-cli-error.log 0 bytes
composition-r268-cli.log 520 bytes
composition-r268-cuda.nsys-rep 99623 bytes
composition-r268-cuda.sqlite 598016 bytes
composition-r268-monitor.log 651 bytes
composition-r268-source0-stdout.log 540 bytes
composition-r268-source0.log 24051 bytes
composition-r268-source1-stdout.log 335 bytes
composition-r268-source1.log 23593 bytes
composition-r268-source2-stdout.log 335 bytes
composition-r268-source2.log 23711 bytes
composition-r268-vlc.log 10904 bytes

### composition-r268-cli-error.log
~~~text

~~~

### composition-r268-cli.log
~~~text
Collecting data...
Generating 'C:\Users\ROG\AppData\Local\Temp\nsys-report-2a10.qdstrm'
[1/1] [0%                          ] composition-r268-cuda.nsys-rep[1/1] [0%                          ] composition-r268-cuda.nsys-rep[1/1] [===================81%      ] composition-r268-cuda.nsys-rep[1/1] [========================100%] composition-r268-cuda.nsys-rep[1/1] [========================100%] composition-r268-cuda.nsys-rep
Generated:
    D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r268-cuda.nsys-rep

~~~

### composition-r268-monitor.log
~~~text
seconds=1.0182277 cpu=0.348756605584032 ws=16781312 private=21250048
seconds=2.0535417 cpu=0.823202165983197 ws=56463360 private=612229120
seconds=3.0770128 cpu=0.555151794888804 ws=183541760 private=891617280
seconds=4.0975302 cpu=0.765543047085724 ws=187658240 private=895098880
seconds=5.1317533 cpu=2.26619382220335 ws=132648960 private=588812288
seconds=6.1543044 cpu=0 ws=132653056 private=588812288
seconds=7.1912852 cpu=0.0684899154089712 ws=131915776 private=580386816
seconds=8.2255318 cpu=0 ws=97738752 private=324190208
seconds=9.2630943 cpu=0 ws=97738752 private=324190208
seconds=10.2837807 cpu=0 ws=97738752 private=324190208

~~~

### composition-r268-source0-stdout.log
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

### composition-r268-source0.log
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
frame=   30 fps=0.0 q=-1.0 size=    1080KiB time=00:00:01.04 bitrate=8468.8kbits/s speed=1.97x elapsed=0:00:00.52    frame=   47 fps= 45 q=-1.0 size=    2281KiB time=00:00:01.56 bitrate=11929.8kbits/s speed=1.49x elapsed=0:00:01.05    frame=   62 fps= 40 q=-1.0 size=    2629KiB time=00:00:02.06 bitrate=10419.8kbits/s speed=1.32x elapsed=0:00:01.56    frame=   78 fps= 37 q=-1.0 size=    2921KiB time=00:00:02.60 bitrate=9201.2kbits/s speed=1.24x elapsed=0:00:02.09    frame=   93 fps= 36 q=-1.0 size=    3470KiB time=00:00:03.10 bitrate=9168.7kbits/s speed=1.19x elapsed=0:00:02.60    frame=  108 fps= 35 q=-1.0 size=    3880KiB time=00:00:03.62 bitrate=8774.9kbits/s speed=1.16x elapsed=0:00:03.11    frame=  124 fps= 34 q=-1.0 size=    4406KiB time=00:00:04.13 bitrate=8731.5kbits/s speed=1.14x elapsed=0:00:03.62    frame=  140 fps= 34 q=-1.0 size=    4773KiB time=00:00:04.66 bitrate=8378.2kbits/s speed=1.12x elapsed=0:00:04.15    frame=  155 fps= 33 q=-1.0 size=    5467KiB time=00:00:05.17 bitrate=8649.8kbits/s speed=1.11x elapsed=0:00:04.66    frame=  171 fps= 33 q=-1.0 size=    6003KiB time=00:00:05.70 bitrate=8627.3kbits/s speed= 1.1x elapsed=0:00:05.18    frame=  186 fps= 33 q=-1.0 size=    6571KiB time=00:00:06.20 bitrate=8682.4kbits/s speed=1.09x elapsed=0:00:05.69    frame=  202 fps= 33 q=-1.0 size=    6888KiB time=00:00:06.73 bitrate=8380.3kbits/s speed=1.09x elapsed=0:00:06.20    frame=  217 fps= 32 q=-1.0 size=    7382KiB time=00:00:07.23 bitrate=8360.7kbits/s speed=1.08x elapsed=0:00:06.71    frame=  232 fps= 32 q=-1.0 size=    7816KiB time=00:00:07.73 bitrate=8279.2kbits/s speed=1.07x elapsed=0:00:07.24    frame=  248 fps= 32 q=-1.0 size=    8407KiB time=00:00:08.26 bitrate=8330.6kbits/s speed=1.07x elapsed=0:00:07.75    frame=  263 fps= 32 q=-1.0 size=    8814KiB time=00:00:08.76 bitrate=8235.9kbits/s speed=1.06x elapsed=0:00:08.26    frame=  279 fps= 32 q=-1.0 size=    9867KiB time=00:00:09.31 bitrate=8680.7kbits/s speed=1.06x elapsed=0:00:08.78    frame=  294 fps= 32 q=-1.0 size=   10476KiB time=00:00:09.80 bitrate=8757.4kbits/s speed=1.05x elapsed=0:00:09.30    frame=  309 fps= 31 q=-1.0 size=   10765KiB time=00:00:10.30 bitrate=8553.7kbits/s speed=1.05x elapsed=0:00:09.81    frame=  325 fps= 31 q=-1.0 size=   11067KiB time=00:00:10.83 bitrate=8368.8kbits/s speed=1.05x elapsed=0:00:10.33    frame=  340 fps= 31 q=-1.0 size=   11663KiB time=00:00:11.36 bitrate=8405.7kbits/s speed=1.05x elapsed=0:00:10.84    frame=  356 fps= 31 q=-1.0 size=   12076KiB time=00:00:11.86 bitrate=8336.6kbits/s speed=1.04x elapsed=0:00:11.35    frame=  372 fps= 31 q=-1.0 size=   12551KiB time=00:00:12.40 bitrate=8291.6kbits/s speed=1.04x elapsed=0:00:11.88    frame=  387 fps= 31 q=-1.0 size=   12909KiB time=00:00:12.90 bitrate=8197.6kbits/s speed=1.04x elapsed=0:00:12.39    frame=  402 fps= 31 q=-1.0 size=   13671KiB time=00:00:13.43 bitrate=8336.9kbits/s speed=1.04x elapsed=0:00:12.91    frame=  418 fps= 31 q=-1.0 size=   14165KiB time=00:00:13.93 bitrate=8328.3kbits/s speed=1.04x elapsed=0:00:13.42    frame=  434 fps= 31 q=-1.0 size=   14790KiB time=00:00:14.46 bitrate=8375.0kbits/s speed=1.04x elapsed=0:00:13.93    frame=  449 fps= 31 q=-1.0 size=   15066KiB time=00:00:14.96 bitrate=8246.6kbits/s speed=1.03x elapsed=0:00:14.46    frame=  464 fps= 31 q=-1.0 size=   15574KiB time=00:00:15.46 bitrate=8248.9kbits/s speed=1.03x elapsed=0:00:14.97    frame=  480 fps= 31 q=-1.0 size=   15981KiB time=00:00:16.00 bitrate=8182.0kbits/s speed=1.03x elapsed=0:00:15.48    frame=  495 fps= 31 q=-1.0 size=   16563KiB time=00:00:16.50 bitrate=8223.3kbits/s speed=1.03x elapsed=0:00:15.99    frame=  511 fps= 31 q=-1.0 size=   16979KiB time=00:00:17.03 bitrate=8165.7kbits/s speed=1.03x elapsed=0:00:16.52    frame=  526 fps= 31 q=-1.0 size=   17773KiB time=00:00:17.53 bitrate=8303.8kbits/s speed=1.03x elapsed=0:00:17.03    frame=  542 fps= 31 q=-1.0 size=   18667KiB time=00:00:18.06 bitrate=8464.0kbits/s speed=1.03x elapsed=0:00:17.55    frame=  558 fps= 31 q=-1.0 size=   18945KiB time=00:00:18.60 bitrate=8344.2kbits/s speed=1.03x elapsed=0:00:18.07    frame=  574 fps= 31 q=-1.0 size=   19319KiB time=00:00:19.13 bitrate=8271.7kbits/s speed=1.03x elapsed=0:00:18.60    frame=  589 fps= 31 q=-1.0 size=   19773KiB time=00:00:19.63 bitrate=8250.2kbits/s speed=1.03x elapsed=0:00:19.11    frame=  604 fps= 31 q=-1.0 size=   20344KiB time=00:00:20.13 bitrate=8277.8kbits/s speed=1.03x elapsed=0:00:19.63    frame=  619 fps= 31 q=-1.0 size=   20706KiB time=00:00:20.64 bitrate=8217.3kbits/s speed=1.02x elapsed=0:00:20.14    frame=  635 fps= 31 q=-1.0 size=   21262KiB time=00:00:21.16 bitrate=8229.0kbits/s speed=1.02x elapsed=0:00:20.67    frame=  651 fps= 31 q=-1.0 size=   21916KiB time=00:00:21.70 bitrate=8273.4kbits/s speed=1.02x elapsed=0:00:21.18    frame=  667 fps= 31 q=-1.0 size=   22490KiB time=00:00:22.23 bitrate=8286.4kbits/s speed=1.02x elapsed=0:00:21.71    frame=  682 fps= 31 q=-1.0 size=   22949KiB time=00:00:22.73 bitrate=8269.7kbits/s speed=1.02x elapsed=0:00:22.22    frame=  698 fps= 31 q=-1.0 size=   23330KiB time=00:00:23.26 bitrate=8214.2kbits/s speed=1.02x elapsed=0:00:22.73    frame=  713 fps= 31 q=-1.0 size=   23719KiB time=00:00:23.76 bitrate=8175.7kbits/s speed=1.02x elapsed=0:00:23.25    frame=  728 fps= 31 q=-1.0 size=   24293KiB time=00:00:24.26 bitrate=8200.9kbits/s speed=1.02x elapsed=0:00:23.77    frame=  744 fps= 31 q=-1.0 size=   24736KiB time=00:00:24.80 bitrate=8170.8kbits/s speed=1.02x elapsed=0:00:24.28    frame=  759 fps= 31 q=-1.0 size=   25193KiB time=00:00:25.30 bitrate=8157.5kbits/s speed=1.02x elapsed=0:00:24.79    frame=  775 fps= 31 q=-1.0 size=   25725KiB time=00:00:25.83 bitrate=8157.5kbits/s speed=1.02x elapsed=0:00:25.32    frame=  790 fps= 31 q=-1.0 size=   26684KiB time=00:00:26.33 bitrate=8301.1kbits/s speed=1.02x elapsed=0:00:25.83    frame=  806 fps= 31 q=-1.0 size=   27000KiB time=00:00:26.86 bitrate=8232.8kbits/s speed=1.02x elapsed=0:00:26.35    frame=  821 fps= 31 q=-1.0 size=   27374KiB time=00:00:27.36 bitrate=8194.1kbits/s speed=1.02x elapsed=0:00:26.86    frame=  837 fps= 31 q=-1.0 size=   27919KiB time=00:00:27.90 bitrate=8197.6kbits/s speed=1.02x elapsed=0:00:27.38    frame=  852 fps= 31 q=-1.0 size=   28441KiB time=00:00:28.40 bitrate=8203.9kbits/s speed=1.02x elapsed=0:00:27.89    frame=  868 fps= 31 q=-1.0 size=   28835KiB time=00:00:28.93 bitrate=8164.0kbits/s speed=1.02x elapsed=0:00:28.41    frame=  884 fps= 31 q=-1.0 size=   29343KiB time=00:00:29.46 bitrate=8157.7kbits/s speed=1.02x elapsed=0:00:28.94    frame=  899 fps= 31 q=-1.0 size=   29962KiB time=00:00:29.97 bitrate=8188.0kbits/s speed=1.02x elapsed=0:00:29.45    frame=  915 fps= 31 q=-1.0 size=   30572KiB time=00:00:30.50 bitrate=8211.3kbits/s speed=1.02x elapsed=0:00:29.97    frame=  930 fps= 31 q=-1.0 size=   31097KiB time=00:00:31.00 bitrate=8217.6kbits/s speed=1.02x elapsed=0:00:30.48    frame=  945 fps= 30 q=-1.0 size=   31432KiB time=00:00:31.50 bitrate=8174.4kbits/s speed=1.02x elapsed=0:00:31.00    frame=  961 fps= 30 q=-1.0 size=   31850KiB time=00:00:32.03 bitrate=8145.1kbits/s speed=1.02x elapsed=0:00:31.52    frame=  976 fps= 30 q=-1.0 size=   32381KiB time=00:00:32.53 bitrate=8153.7kbits/s speed=1.02x elapsed=0:00:32.03    frame=  992 fps= 30 q=-1.0 size=   33024KiB time=00:00:33.06 bitrate=8181.5kbits/s speed=1.02x elapsed=0:00:32.54    frame= 1007 fps= 30 q=-1.0 size=   33314KiB time=00:00:33.57 bitrate=8128.1kbits/s speed=1.02x elapsed=0:00:33.06    frame= 1023 fps= 30 q=-1.0 size=   34161KiB time=00:00:34.11 bitrate=8204.2kbits/s speed=1.02x elapsed=0:00:33.58    frame= 1039 fps= 30 q=-1.0 size=   35003KiB time=00:00:34.63 bitrate=8279.4kbits/s speed=1.02x elapsed=0:00:34.10    frame= 1054 fps= 30 q=-1.0 size=   35366KiB time=00:00:35.13 bitrate=8246.2kbits/s speed=1.01x elapsed=0:00:34.61    frame= 1069 fps= 30 q=-1.0 size=   35648KiB time=00:00:35.64 bitrate=8193.2kbits/s speed=1.01x elapsed=0:00:35.13    frame= 1084 fps= 30 q=-1.0 size=   36202KiB time=00:00:36.13 bitrate=8207.5kbits/s speed=1.01x elapsed=0:00:35.64    frame= 1101 fps= 30 q=-1.0 size=   36654KiB time=00:00:36.70 bitrate=8181.8kbits/s speed=1.01x elapsed=0:00:36.16    frame= 1116 fps= 30 q=-1.0 size=   37115KiB time=00:00:37.20 bitrate=8173.3kbits/s speed=1.01x elapsed=0:00:36.69    frame= 1131 fps= 30 q=-1.0 size=   37451KiB time=00:00:37.70 bitrate=8135.9kbits/s speed=1.01x elapsed=0:00:37.20    frame= 1147 fps= 30 q=-1.0 size=   38183KiB time=00:00:38.23 bitrate=8181.2kbits/s speed=1.01x elapsed=0:00:37.72    frame= 1163 fps= 30 q=-1.0 size=   38724KiB time=00:00:38.76 bitrate=8183.0kbits/s speed=1.01x elapsed=0:00:38.24    frame= 1178 fps= 30 q=-1.0 size=   39327KiB time=00:00:39.26 bitrate=8204.6kbits/s speed=1.01x elapsed=0:00:38.75    frame= 1194 fps= 30 q=-1.0 size=   39608KiB time=00:00:39.80 bitrate=8152.5kbits/s speed=1.01x elapsed=0:00:39.27    frame= 1209 fps= 30 q=-1.0 size=   40114KiB time=00:00:40.30 bitrate=8154.3kbits/s speed=1.01x elapsed=0:00:39.79    frame= 1224 fps= 30 q=-1.0 size=   40498KiB time=00:00:40.80 bitrate=8131.4kbits/s speed=1.01x elapsed=0:00:40.30    frame= 1239 fps= 30 q=-1.0 size=   41061KiB time=00:00:41.30 bitrate=8142.9kbits/s speed=1.01x elapsed=0:00:40.81    frame= 1256 fps= 30 q=-1.0 size=   41512KiB time=00:00:41.86 bitrate=8122.6kbits/s speed=1.01x elapsed=0:00:41.33    frame= 1270 fps= 30 q=-1.0 size=   42208KiB time=00:00:42.36 bitrate=8161.4kbits/s speed=1.01x elapsed=0:00:41.84    frame= 1287 fps= 30 q=-1.0 size=   43184KiB time=00:00:42.90 bitrate=8246.3kbits/s speed=1.01x elapsed=0:00:42.36    frame= 1301 fps= 30 q=-1.0 size=   43443KiB time=00:00:43.37 bitrate=8204.9kbits/s speed=1.01x elapsed=0:00:42.87    frame= 1317 fps= 30 q=-1.0 size=   43745KiB time=00:00:43.90 bitrate=8161.5kbits/s speed=1.01x elapsed=0:00:43.39    frame= 1333 fps= 30 q=-1.0 size=   44319KiB time=00:00:44.43 bitrate=8171.0kbits/s speed=1.01x elapsed=0:00:43.91    frame= 1348 fps= 30 q=-1.0 size=   44728KiB time=00:00:44.95 bitrate=8150.8kbits/s speed=1.01x elapsed=0:00:44.43    frame= 1363 fps= 30 q=-1.0 size=   45196KiB time=00:00:45.44 bitrate=8147.7kbits/s speed=1.01x elapsed=0:00:44.94    frame= 1379 fps= 30 q=-1.0 size=   45573KiB time=00:00:45.97 bitrate=8120.3kbits/s speed=1.01x elapsed=0:00:45.45    frame= 1395 fps= 30 q=-1.0 size=   46392KiB time=00:00:46.50 bitrate=8173.0kbits/s speed=1.01x elapsed=0:00:45.96    frame= 1410 fps= 30 q=-1.0 size=   46862KiB time=00:00:47.03 bitrate=8162.2kbits/s speed=1.01x elapsed=0:00:46.50    frame= 1426 fps= 30 q=-1.0 size=   47462KiB time=00:00:47.53 bitrate=8179.7kbits/s speed=1.01x elapsed=0:00:47.02    frame= 1442 fps= 30 q=-1.0 size=   47866KiB time=00:00:48.06 bitrate=8157.8kbits/s speed=1.01x elapsed=0:00:47.54    frame= 1457 fps= 30 q=-1.0 size=   48249KiB time=00:00:48.56 bitrate=8138.4kbits/s speed=1.01x elapsed=0:00:48.07    frame= 1473 fps= 30 q=-1.0 size=   48819KiB time=00:00:49.11 bitrate=8143.5kbits/s speed=1.01x elapsed=0:00:48.59    frame= 1489 fps= 30 q=-1.0 size=   49257KiB time=00:00:49.64 bitrate=8128.0kbits/s speed=1.01x elapsed=0:00:49.12    frame= 1504 fps= 30 q=-1.0 size=   49802KiB time=00:00:50.13 bitrate=8137.8kbits/s speed=1.01x elapsed=0:00:49.63    frame= 1519 fps= 30 q=-1.0 size=   50228KiB time=00:00:50.64 bitrate=8124.9kbits/s speed=1.01x elapsed=0:00:50.14    frame= 1535 fps= 30 q=-1.0 size=   51169KiB time=00:00:51.16 bitrate=8192.4kbits/s speed=1.01x elapsed=0:00:50.66    frame= 1551 fps= 30 q=-1.0 size=   51492KiB time=00:00:51.70 bitrate=8159.1kbits/s speed=1.01x elapsed=0:00:51.18    frame= 1566 fps= 30 q=-1.0 size=   51873KiB time=00:00:52.20 bitrate=8140.7kbits/s speed=1.01x elapsed=0:00:51.68    frame= 1581 fps= 30 q=-1.0 size=   52390KiB time=00:00:52.70 bitrate=8142.3kbits/s speed=1.01x elapsed=0:00:52.20    frame= 1597 fps= 30 q=-1.0 size=   53007KiB time=00:00:53.23 bitrate=8157.2kbits/s speed=1.01x elapsed=0:00:52.72    frame= 1613 fps= 30 q=-1.0 size=   53436KiB time=00:00:53.73 bitrate=8146.7kbits/s speed=1.01x elapsed=0:00:53.24    frame= 1628 fps= 30 q=-1.0 size=   53962KiB time=00:00:54.26 bitrate=8146.1kbits/s speed=1.01x elapsed=0:00:53.75    frame= 1643 fps= 30 q=-1.0 size=   54559KiB time=00:00:54.76 bitrate=8160.9kbits/s speed=1.01x elapsed=0:00:54.27    frame= 1660 fps= 30 q=-1.0 size=   55164KiB time=00:00:55.33 bitrate=8166.9kbits/s speed=1.01x elapsed=0:00:54.80    frame= 1675 fps= 30 q=-1.0 size=   55671KiB time=00:00:55.83 bitrate=8168.2kbits/s speed=1.01x elapsed=0:00:55.31    frame= 1690 fps= 30 q=-1.0 size=   55996KiB time=00:00:56.33 bitrate=8143.0kbits/s speed=1.01x elapsed=0:00:55.82    frame= 1706 fps= 30 q=-1.0 size=   56419KiB time=00:00:56.86 bitrate=8127.5kbits/s speed=1.01x elapsed=0:00:56.35    frame= 1721 fps= 30 q=-1.0 size=   56966KiB time=00:00:57.36 bitrate=8134.8kbits/s speed=1.01x elapsed=0:00:56.85    frame= 1736 fps= 30 q=-1.0 size=   57398KiB time=00:00:57.86 bitrate=8125.7kbits/s speed=1.01x elapsed=0:00:57.37    frame= 1752 fps= 30 q=-1.0 size=   57981KiB time=00:00:58.40 bitrate=8133.2kbits/s speed=1.01x elapsed=0:00:57.89    frame= 1768 fps= 30 q=-1.0 size=   58558KiB time=00:00:58.93 bitrate=8139.8kbits/s speed=1.01x elapsed=0:00:58.40    frame= 1783 fps= 30 q=-1.0 size=   59407KiB time=00:00:59.43 bitrate=8188.4kbits/s speed=1.01x elapsed=0:00:58.93    frame= 1798 fps= 30 q=-1.0 size=   59690KiB time=00:00:59.93 bitrate=8158.7kbits/s speed=1.01x elapsed=0:00:59.44    frame= 1814 fps= 30 q=-1.0 size=   60077KiB time=00:01:00.46 bitrate=8139.2kbits/s speed=1.01x elapsed=0:00:59.95    frame= 1830 fps= 30 q=-1.0 size=   60621KiB time=00:01:01.00 bitrate=8141.0kbits/s speed=1.01x elapsed=0:01:00.47    frame= 1845 fps= 30 q=-1.0 size=   61128KiB time=00:01:01.50 bitrate=8142.5kbits/s speed=1.01x elapsed=0:01:00.99    frame= 1860 fps= 30 q=-1.0 size=   61497KiB time=00:01:02.00 bitrate=8125.6kbits/s speed=1.01x elapsed=0:01:01.50    frame= 1876 fps= 30 q=-1.0 size=   61994KiB time=00:01:02.53 bitrate=8121.4kbits/s speed=1.01x elapsed=0:01:02.02    frame= 1891 fps= 30 q=-1.0 size=   62618KiB time=00:01:03.03 bitrate=8138.1kbits/s speed=1.01x elapsed=0:01:02.53    frame= 1907 fps= 30 q=-1.0 size=   63191KiB time=00:01:03.56 bitrate=8143.6kbits/s speed=1.01x elapsed=0:01:03.05    frame= 1923 fps= 30 q=-1.0 size=   63799KiB time=00:01:04.10 bitrate=8153.5kbits/s speed=1.01x elapsed=0:01:03.57    frame= 1937 fps= 30 q=-1.0 size=   64054KiB time=00:01:04.57 bitrate=8126.0kbits/s speed=1.01x elapsed=0:01:04.07    frame= 1954 fps= 30 q=-1.0 size=   64679KiB time=00:01:05.13 bitrate=8134.9kbits/s speed=1.01x elapsed=0:01:04.60    frame= 1968 fps= 30 q=-1.0 size=   65037KiB time=00:01:05.60 bitrate=8121.7kbits/s speed=1.01x elapsed=0:01:05.12    frame= 1984 fps= 30 q=-1.0 size=   65650KiB time=00:01:06.15 bitrate=8129.6kbits/s speed=1.01x elapsed=0:01:05.64    frame= 2000 fps= 30 q=-1.0 size=   66073KiB time=00:01:06.66 bitrate=8119.0kbits/s speed=1.01x elapsed=0:01:06.16    frame= 2015 fps= 30 q=-1.0 size=   66967KiB time=00:01:07.17 bitrate=8166.6kbits/s speed=1.01x elapsed=0:01:06.67    frame= 2031 fps= 30 q=-1.0 size=   67688KiB time=00:01:07.70 bitrate=8190.5kbits/s speed=1.01x elapsed=0:01:07.19    frame= 2047 fps= 30 q=-1.0 size=   68034KiB time=00:01:08.23 bitrate=8168.1kbits/s speed=1.01x elapsed=0:01:07.70    frame= 2062 fps= 30 q=-1.0 size=   68288KiB time=00:01:08.73 bitrate=8139.0kbits/s speed=1.01x elapsed=0:01:08.21    frame= 2077 fps= 30 q=-1.0 size=   68864KiB time=00:01:09.23 bitrate=8148.3kbits/s speed=1.01x elapsed=0:01:08.74    frame= 2093 fps= 30 q=-1.0 size=   69302KiB time=00:01:09.76 bitrate=8137.5kbits/s speed=1.01x elapsed=0:01:09.25    frame= 2109 fps= 30 q=-1.0 size=   69783KiB time=00:01:10.30 bitrate=8131.8kbits/s speed=1.01x elapsed=0:01:09.76    frame= 2124 fps= 30 q=-1.0 size=   70128KiB time=00:01:10.80 bitrate=8114.3kbits/s speed=1.01x elapsed=0:01:10.28    frame= 2140 fps= 30 q=-1.0 size=   70874KiB time=00:01:11.33 bitrate=8139.2kbits/s speed=1.01x elapsed=0:01:10.80    frame= 2154 fps= 30 q=-1.0 size=   71364KiB time=00:01:11.81 bitrate=8140.0kbits/s speed=1.01x elapsed=0:01:11.31    frame= 2170 fps= 30 q=-1.0 size=   71995KiB time=00:01:12.33 bitrate=8153.6kbits/s speed=1.01x elapsed=0:01:11.84    frame= 2185 fps= 30 q=-1.0 size=   72282KiB time=00:01:12.84 bitrate=8129.1kbits/s speed=1.01x elapsed=0:01:12.34    frame= 2201 fps= 30 q=-1.0 size=   72828KiB time=00:01:13.36 bitrate=8131.9kbits/s speed=1.01x elapsed=0:01:12.85    frame= 2217 fps= 30 q=-1.0 size=   73230KiB time=00:01:13.90 bitrate=8117.8kbits/s speed=1.01x elapsed=0:01:13.38    frame= 2232 fps= 30 q=-1.0 size=   73775KiB time=00:01:14.40 bitrate=8123.1kbits/s speed=1.01x elapsed=0:01:13.89    frame= 2248 fps= 30 q=-1.0 size=   74102KiB time=00:01:14.93 bitrate=8101.1kbits/s speed=1.01x elapsed=0:01:14.41    frame= 2263 fps= 30 q=-1.0 size=   74772KiB time=00:01:15.44 bitrate=8119.3kbits/s speed=1.01x elapsed=0:01:14.93    frame= 2278 fps= 30 q=-1.0 size=   75625KiB time=00:01:15.93 bitrate=8158.7kbits/s speed=1.01x elapsed=0:01:15.44    frame= 2294 fps= 30 q=-1.0 size=   75988KiB time=00:01:16.46 bitrate=8140.7kbits/s speed=1.01x elapsed=0:01:15.96    frame= 2310 fps= 30 q=-1.0 size=   76320KiB time=00:01:17.00 bitrate=8119.6kbits/s speed=1.01x elapsed=0:01:16.47    frame= 2325 fps= 30 q=-1.0 size=   76931KiB time=00:01:17.50 bitrate=8131.0kbits/s speed=1.01x elapsed=0:01:16.98    frame= 2340 fps= 30 q=-1.0 size=   77379KiB time=00:01:18.00 bitrate=8126.8kbits/s speed=1.01x elapsed=0:01:17.50    frame= 2356 fps= 30 q=-1.0 size=   77893KiB time=00:01:18.53 bitrate=8125.3kbits/s speed=1.01x elapsed=0:01:18.01    frame= 2371 fps= 30 q=-1.0 size=   78242KiB time=00:01:19.03 bitrate=8110.0kbits/s speed=1.01x elapsed=0:01:18.53    frame= 2386 fps= 30 q=-1.0 size=   79063KiB time=00:01:19.55 bitrate=8141.7kbits/s speed=1.01x elapsed=0:01:19.04    frame= 2402 fps= 30 q=-1.0 size=   79636KiB time=00:01:20.10 bitrate=8144.5kbits/s speed=1.01x elapsed=0:01:19.57    frame= 2418 fps= 30 q=-1.0 size=   80130KiB time=00:01:20.60 bitrate=8144.2kbits/s speed=1.01x elapsed=0:01:20.07    frame= 2433 fps= 30 q=-1.0 size=   80491KiB time=00:01:21.10 bitrate=8129.8kbits/s speed=1.01x elapsed=0:01:20.59    frame= 2449 fps= 30 q=-1.0 size=   80886KiB time=00:01:21.63 bitrate=8117.0kbits/s speed=1.01x elapsed=0:01:21.11    frame= 2464 fps= 30 q=-1.0 size=   81432KiB time=00:01:22.13 bitrate=8122.1kbits/s speed=1.01x elapsed=0:01:21.62    frame= 2480 fps= 30 q=-1.0 size=   81869KiB time=00:01:22.66 bitrate=8113.0kbits/s speed=1.01x elapsed=0:01:22.14    frame= 2495 fps= 30 q=-1.0 size=   82765KiB time=00:01:23.16 bitrate=8152.5kbits/s speed=1.01x elapsed=0:01:22.67    frame= 2511 fps= 30 q=-1.0 size=   83144KiB time=00:01:23.70 bitrate=8137.6kbits/s speed=1.01x elapsed=0:01:23.18    frame= 2526 fps= 30 q=-1.0 size=   83979KiB time=00:01:24.23 bitrate=8167.2kbits/s speed=1.01x elapsed=0:01:23.70    frame= 2542 fps= 30 q=-1.0 size=   84280KiB time=00:01:24.73 bitrate=8148.1kbits/s speed=1.01x elapsed=0:01:24.22    frame= 2557 fps= 30 q=-1.0 size=   84617KiB time=00:01:25.24 bitrate=8132.1kbits/s speed=1.01x elapsed=0:01:24.73    frame= 2573 fps= 30 q=-1.0 size=   85103KiB time=00:01:25.76 bitrate=8128.6kbits/s speed=1.01x elapsed=0:01:25.25    frame= 2589 fps= 30 q=-1.0 size=   85671KiB time=00:01:26.30 bitrate=8132.3kbits/s speed=1.01x elapsed=0:01:25.77    frame= 2604 fps= 30 q=-1.0 size=   86021KiB time=00:01:26.80 bitrate=8118.5kbits/s speed=1.01x elapsed=0:01:26.28    frame= 2619 fps= 30 q=-1.0 size=   86535KiB time=00:01:27.30 bitrate=8119.5kbits/s speed=1.01x elapsed=0:01:26.79    frame= 2634 fps= 30 q=-1.0 size=   87142KiB time=00:01:27.81 bitrate=8128.9kbits/s speed=1.01x elapsed=0:01:27.31    frame= 2650 fps= 30 q=-1.0 size=   87727KiB time=00:01:28.33 bitrate=8135.8kbits/s speed=1.01x elapsed=0:01:27.82    frame= 2665 fps= 30 q=-1.0 size=   88246KiB time=00:01:28.83 bitrate=8137.2kbits/s speed=1.01x elapsed=0:01:28.35    frame= 2681 fps= 30 q=-1.0 size=   88610KiB time=00:01:29.36 bitrate=8122.6kbits/s speed=1.01x elapsed=0:01:28.85    frame= 2697 fps= 30 q=-1.0 size=   89038KiB time=00:01:29.90 bitrate=8113.4kbits/s speed=1.01x elapsed=0:01:29.38    frame= 2712 fps= 30 q=-1.0 size=   89566KiB time=00:01:30.40 bitrate=8116.4kbits/s speed=1.01x elapsed=0:01:29.89    frame= 2727 fps= 30 q=-1.0 size=   89990KiB time=00:01:30.90 bitrate=8109.4kbits/s speed=1.01x elapsed=0:01:30.41    frame= 2743 fps= 30 q=-1.0 size=   90586KiB time=00:01:31.43 bitrate=8116.0kbits/s speed=1.01x elapsed=0:01:30.93    frame= 2758 fps= 30 q=-1.0 size=   90952KiB time=00:01:31.95 bitrate=8103.0kbits/s speed=1.01x elapsed=0:01:31.44    frame= 2774 fps= 30 q=-1.0 size=   92058KiB time=00:01:32.46 bitrate=8155.8kbits/s speed=1.01x elapsed=0:01:31.95    frame= 2789 fps= 30 q=-1.0 size=   92333KiB time=00:01:32.97 bitrate=8135.6kbits/s speed=1.01x elapsed=0:01:32.47    frame= 2805 fps= 30 q=-1.0 size=   92727KiB time=00:01:33.50 bitrate=8124.3kbits/s speed=1.01x elapsed=0:01:32.98    frame= 2820 fps= 30 q=-1.0 size=   93227KiB time=00:01:34.01 bitrate=8123.1kbits/s speed=1.01x elapsed=0:01:33.51    frame= 2836 fps= 30 q=-1.0 size=   93739KiB time=00:01:34.53 bitrate=8123.2kbits/s speed=1.01x elapsed=0:01:34.04    frame= 2852 fps= 30 q=-1.0 size=   94302KiB time=00:01:35.06 bitrate=8126.1kbits/s speed=1.01x elapsed=0:01:34.55    frame= 2867 fps= 30 q=-1.0 size=   94617KiB time=00:01:35.56 bitrate=8110.6kbits/s speed=1.01x elapsed=0:01:35.06    frame= 2883 fps= 30 q=-1.0 size=   95344KiB time=00:01:36.10 bitrate=8126.9kbits/s speed=1.01x elapsed=0:01:35.59    frame= 2899 fps= 30 q=-1.0 size=   95842KiB time=00:01:36.63 bitrate=8124.9kbits/s speed=1.01x elapsed=0:01:36.11    frame= 2914 fps= 30 q=-1.0 size=   96417KiB time=00:01:37.13 bitrate=8131.6kbits/s speed=1.01x elapsed=0:01:36.62    frame= 2929 fps= 30 q=-1.0 size=   96707KiB time=00:01:37.66 bitrate=8111.5kbits/s speed=1.01x elapsed=0:01:37.13    
~~~

### composition-r268-source1-stdout.log
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

### composition-r268-source1.log
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
frame=   30 fps=0.0 q=-1.0 size=    1080KiB time=00:00:01.00 bitrate=8849.0kbits/s speed=1.98x elapsed=0:00:00.50    frame=   46 fps= 45 q=-1.0 size=    2245KiB time=00:00:01.53 bitrate=11992.8kbits/s speed=1.49x elapsed=0:00:01.03    frame=   61 fps= 40 q=-1.0 size=    2521KiB time=00:00:02.03 bitrate=10157.2kbits/s speed=1.32x elapsed=0:00:01.54    frame=   77 fps= 37 q=-1.0 size=    2900KiB time=00:00:02.56 bitrate=9255.1kbits/s speed=1.24x elapsed=0:00:02.06    frame=   93 fps= 36 q=-1.0 size=    3470KiB time=00:00:03.10 bitrate=9168.7kbits/s speed= 1.2x elapsed=0:00:02.59    frame=  108 fps= 35 q=-1.0 size=    3880KiB time=00:00:03.60 bitrate=8829.2kbits/s speed=1.16x elapsed=0:00:03.10    frame=  124 fps= 34 q=-1.0 size=    4406KiB time=00:00:04.13 bitrate=8731.5kbits/s speed=1.14x elapsed=0:00:03.61    frame=  140 fps= 34 q=-1.0 size=    4773KiB time=00:00:04.66 bitrate=8379.1kbits/s speed=1.13x elapsed=0:00:04.14    frame=  155 fps= 33 q=-1.0 size=    5467KiB time=00:00:05.16 bitrate=8668.9kbits/s speed=1.11x elapsed=0:00:04.65    frame=  170 fps= 33 q=-1.0 size=    5969KiB time=00:00:05.66 bitrate=8628.6kbits/s speed=1.09x elapsed=0:00:05.17    frame=  186 fps= 33 q=-1.0 size=    6571KiB time=00:00:06.20 bitrate=8682.4kbits/s speed=1.09x elapsed=0:00:05.68    frame=  201 fps= 32 q=-1.0 size=    6860KiB time=00:00:06.70 bitrate=8387.9kbits/s speed=1.08x elapsed=0:00:06.19    frame=  216 fps= 32 q=-1.0 size=    7357KiB time=00:00:07.20 bitrate=8370.5kbits/s speed=1.07x elapsed=0:00:06.70    frame=  232 fps= 32 q=-1.0 size=    7816KiB time=00:00:07.73 bitrate=8279.2kbits/s speed=1.07x elapsed=0:00:07.23    frame=  247 fps= 32 q=-1.0 size=    8354KiB time=00:00:08.23 bitrate=8312.2kbits/s speed=1.06x elapsed=0:00:07.74    frame=  263 fps= 32 q=-1.0 size=    8814KiB time=00:00:08.76 bitrate=8235.9kbits/s speed=1.06x elapsed=0:00:08.25    frame=  278 fps= 32 q=-1.0 size=    9819KiB time=00:00:09.30 bitrate=8649.3kbits/s speed=1.06x elapsed=0:00:08.78    frame=  294 fps= 32 q=-1.0 size=   10476KiB time=00:00:09.80 bitrate=8757.4kbits/s speed=1.05x elapsed=0:00:09.29    frame=  310 fps= 32 q=-1.0 size=   10808KiB time=00:00:10.33 bitrate=8568.2kbits/s speed=1.05x elapsed=0:00:09.80    frame=  325 fps= 31 q=-1.0 size=   11067KiB time=00:00:10.83 bitrate=8368.8kbits/s speed=1.05x elapsed=0:00:10.32    frame=  340 fps= 31 q=-1.0 size=   11663KiB time=00:00:11.36 bitrate=8405.7kbits/s speed=1.05x elapsed=0:00:10.83    frame=  356 fps= 31 q=-1.0 size=   12076KiB time=00:00:11.86 bitrate=8336.6kbits/s speed=1.05x elapsed=0:00:11.35    frame=  371 fps= 31 q=-1.0 size=   12531KiB time=00:00:12.36 bitrate=8300.5kbits/s speed=1.04x elapsed=0:00:11.87    frame=  387 fps= 31 q=-1.0 size=   12909KiB time=00:00:12.90 bitrate=8197.6kbits/s speed=1.04x elapsed=0:00:12.38    frame=  402 fps= 31 q=-1.0 size=   13671KiB time=00:00:13.40 bitrate=8357.6kbits/s speed=1.04x elapsed=0:00:12.90    frame=  418 fps= 31 q=-1.0 size=   14165KiB time=00:00:13.93 bitrate=8328.3kbits/s speed=1.04x elapsed=0:00:13.41    frame=  434 fps= 31 q=-1.0 size=   14790KiB time=00:00:14.46 bitrate=8375.0kbits/s speed=1.04x elapsed=0:00:13.94    frame=  450 fps= 31 q=-1.0 size=   15085KiB time=00:00:15.00 bitrate=8238.4kbits/s speed=1.04x elapsed=0:00:14.47    frame=  465 fps= 31 q=-1.0 size=   15598KiB time=00:00:15.50 bitrate=8243.8kbits/s speed=1.03x elapsed=0:00:14.98    frame=  480 fps= 31 q=-1.0 size=   15981KiB time=00:00:16.00 bitrate=8182.0kbits/s speed=1.03x elapsed=0:00:15.49    frame=  496 fps= 31 q=-1.0 size=   16592KiB time=00:00:16.53 bitrate=8221.0kbits/s speed=1.03x elapsed=0:00:16.00    frame=  511 fps= 31 q=-1.0 size=   16979KiB time=00:00:17.03 bitrate=8165.7kbits/s speed=1.03x elapsed=0:00:16.53    frame=  526 fps= 31 q=-1.0 size=   17773KiB time=00:00:17.53 bitrate=8303.8kbits/s speed=1.03x elapsed=0:00:17.04    frame=  542 fps= 31 q=-1.0 size=   18667KiB time=00:00:18.06 bitrate=8464.0kbits/s speed=1.03x elapsed=0:00:17.55    frame=  557 fps= 31 q=-1.0 size=   18902KiB time=00:00:18.56 bitrate=8340.0kbits/s speed=1.03x elapsed=0:00:18.08    frame=  574 fps= 31 q=-1.0 size=   19319KiB time=00:00:19.13 bitrate=8271.7kbits/s speed=1.03x elapsed=0:00:18.61    frame=  589 fps= 31 q=-1.0 size=   19773KiB time=00:00:19.63 bitrate=8250.2kbits/s speed=1.03x elapsed=0:00:19.12    frame=  604 fps= 31 q=-1.0 size=   20344KiB time=00:00:20.13 bitrate=8277.8kbits/s speed=1.03x elapsed=0:00:19.63    frame=  620 fps= 31 q=-1.0 size=   20723KiB time=00:00:20.66 bitrate=8214.4kbits/s speed=1.03x elapsed=0:00:20.15    frame=  635 fps= 31 q=-1.0 size=   21262KiB time=00:00:21.16 bitrate=8229.0kbits/s speed=1.02x elapsed=0:00:20.67    frame=  651 fps= 31 q=-1.0 size=   21916KiB time=00:00:21.70 bitrate=8273.4kbits/s speed=1.02x elapsed=0:00:21.19    frame=  667 fps= 31 q=-1.0 size=   22490KiB time=00:00:22.23 bitrate=8286.4kbits/s speed=1.02x elapsed=0:00:21.71    frame=  682 fps= 31 q=-1.0 size=   22949KiB time=00:00:22.73 bitrate=8269.7kbits/s speed=1.02x elapsed=0:00:22.23    frame=  697 fps= 31 q=-1.0 size=   23303KiB time=00:00:23.23 bitrate=8216.6kbits/s speed=1.02x elapsed=0:00:22.74    frame=  713 fps= 31 q=-1.0 size=   23719KiB time=00:00:23.76 bitrate=8175.7kbits/s speed=1.02x elapsed=0:00:23.25    frame=  729 fps= 31 q=-1.0 size=   24317KiB time=00:00:24.30 bitrate=8197.7kbits/s speed=1.02x elapsed=0:00:23.78    frame=  744 fps= 31 q=-1.0 size=   24736KiB time=00:00:24.80 bitrate=8170.8kbits/s speed=1.02x elapsed=0:00:24.29    frame=  760 fps= 31 q=-1.0 size=   25225KiB time=00:00:25.33 bitrate=8157.0kbits/s speed=1.02x elapsed=0:00:24.80    frame=  775 fps= 31 q=-1.0 size=   25725KiB time=00:00:25.83 bitrate=8157.5kbits/s speed=1.02x elapsed=0:00:25.32    frame=  790 fps= 31 q=-1.0 size=   26684KiB time=00:00:26.33 bitrate=8301.1kbits/s speed=1.02x elapsed=0:00:25.83    frame=  806 fps= 31 q=-1.0 size=   27000KiB time=00:00:26.86 bitrate=8232.8kbits/s speed=1.02x elapsed=0:00:26.36    frame=  822 fps= 31 q=-1.0 size=   27388KiB time=00:00:27.40 bitrate=8188.3kbits/s speed=1.02x elapsed=0:00:26.87    frame=  837 fps= 31 q=-1.0 size=   27919KiB time=00:00:27.90 bitrate=8197.6kbits/s speed=1.02x elapsed=0:00:27.38    frame=  852 fps= 31 q=-1.0 size=   28441KiB time=00:00:28.40 bitrate=8203.9kbits/s speed=1.02x elapsed=0:00:27.89    frame=  868 fps= 31 q=-1.0 size=   28835KiB time=00:00:28.93 bitrate=8164.0kbits/s speed=1.02x elapsed=0:00:28.42    frame=  883 fps= 31 q=-1.0 size=   29319KiB time=00:00:29.43 bitrate=8160.3kbits/s speed=1.02x elapsed=0:00:28.94    frame=  899 fps= 31 q=-1.0 size=   29962KiB time=00:00:29.96 bitrate=8190.9kbits/s speed=1.02x elapsed=0:00:29.46    frame=  915 fps= 31 q=-1.0 size=   30572KiB time=00:00:30.50 bitrate=8211.3kbits/s speed=1.02x elapsed=0:00:29.98    frame=  930 fps= 30 q=-1.0 size=   31097KiB time=00:00:31.00 bitrate=8217.6kbits/s speed=1.02x elapsed=0:00:30.49    frame=  945 fps= 30 q=-1.0 size=   31432KiB time=00:00:31.50 bitrate=8174.4kbits/s speed=1.02x elapsed=0:00:31.00    frame=  961 fps= 30 q=-1.0 size=   31850KiB time=00:00:32.03 bitrate=8145.1kbits/s speed=1.02x elapsed=0:00:31.53    frame=  976 fps= 30 q=-1.0 size=   32381KiB time=00:00:32.53 bitrate=8153.7kbits/s speed=1.02x elapsed=0:00:32.04    frame=  992 fps= 30 q=-1.0 size=   33024KiB time=00:00:33.06 bitrate=8181.5kbits/s speed=1.02x elapsed=0:00:32.55    frame= 1007 fps= 30 q=-1.0 size=   33314KiB time=00:00:33.56 bitrate=8130.3kbits/s speed=1.02x elapsed=0:00:33.06    frame= 1023 fps= 30 q=-1.0 size=   34161KiB time=00:00:34.10 bitrate=8206.6kbits/s speed=1.01x elapsed=0:00:33.59    frame= 1038 fps= 30 q=-1.0 size=   34970KiB time=00:00:34.60 bitrate=8279.6kbits/s speed=1.01x elapsed=0:00:34.10    frame= 1054 fps= 30 q=-1.0 size=   35366KiB time=00:00:35.13 bitrate=8246.2kbits/s speed=1.01x elapsed=0:00:34.62    frame= 1069 fps= 30 q=-1.0 size=   35648KiB time=00:00:35.63 bitrate=8195.3kbits/s speed=1.01x elapsed=0:00:35.13    frame= 1085 fps= 30 q=-1.0 size=   36221KiB time=00:00:36.16 bitrate=8204.3kbits/s speed=1.01x elapsed=0:00:35.65    frame= 1101 fps= 30 q=-1.0 size=   36654KiB time=00:00:36.70 bitrate=8181.8kbits/s speed=1.01x elapsed=0:00:36.17    frame= 1117 fps= 30 q=-1.0 size=   37139KiB time=00:00:37.23 bitrate=8171.3kbits/s speed=1.01x elapsed=0:00:36.70    frame= 1131 fps= 30 q=-1.0 size=   37451KiB time=00:00:37.70 bitrate=8137.9kbits/s speed=1.01x elapsed=0:00:37.21    frame= 1147 fps= 30 q=-1.0 size=   38183KiB time=00:00:38.23 bitrate=8181.2kbits/s speed=1.01x elapsed=0:00:37.72    frame= 1162 fps= 30 q=-1.0 size=   38690KiB time=00:00:38.73 bitrate=8182.9kbits/s speed=1.01x elapsed=0:00:38.24    frame= 1178 fps= 30 q=-1.0 size=   39327KiB time=00:00:39.26 bitrate=8204.6kbits/s speed=1.01x elapsed=0:00:38.76    frame= 1194 fps= 30 q=-1.0 size=   39608KiB time=00:00:39.80 bitrate=8152.5kbits/s speed=1.01x elapsed=0:00:39.28    frame= 1209 fps= 30 q=-1.0 size=   40114KiB time=00:00:40.30 bitrate=8154.3kbits/s speed=1.01x elapsed=0:00:39.79    frame= 1224 fps= 30 q=-1.0 size=   40498KiB time=00:00:40.80 bitrate=8131.4kbits/s speed=1.01x elapsed=0:00:40.31    frame= 1240 fps= 30 q=-1.0 size=   41087KiB time=00:00:41.33 bitrate=8143.1kbits/s speed=1.01x elapsed=0:00:40.82    frame= 1255 fps= 30 q=-1.0 size=   41473KiB time=00:00:41.86 bitrate=8115.0kbits/s speed=1.01x elapsed=0:00:41.34    frame= 1271 fps= 30 q=-1.0 size=   42313KiB time=00:00:42.36 bitrate=8181.7kbits/s speed=1.01x elapsed=0:00:41.85    frame= 1287 fps= 30 q=-1.0 size=   43184KiB time=00:00:42.90 bitrate=8246.3kbits/s speed=1.01x elapsed=0:00:42.37    frame= 1301 fps= 30 q=-1.0 size=   43443KiB time=00:00:43.36 bitrate=8206.4kbits/s speed=1.01x elapsed=0:00:42.88    frame= 1317 fps= 30 q=-1.0 size=   43745KiB time=00:00:43.90 bitrate=8163.2kbits/s speed=1.01x elapsed=0:00:43.39    frame= 1333 fps= 30 q=-1.0 size=   44319KiB time=00:00:44.43 bitrate=8171.0kbits/s speed=1.01x elapsed=0:00:43.92    frame= 1349 fps= 30 q=-1.0 size=   44756KiB time=00:00:44.96 bitrate=8153.6kbits/s speed=1.01x elapsed=0:00:44.44    frame= 1364 fps= 30 q=-1.0 size=   45212KiB time=00:00:45.46 bitrate=8146.2kbits/s speed=1.01x elapsed=0:00:44.95    frame= 1379 fps= 30 q=-1.0 size=   45573KiB time=00:00:45.96 bitrate=8121.9kbits/s speed=1.01x elapsed=0:00:45.46    frame= 1394 fps= 30 q=-1.0 size=   46368KiB time=00:00:46.46 bitrate=8174.6kbits/s speed=1.01x elapsed=0:00:45.97    frame= 1410 fps= 30 q=-1.0 size=   46862KiB time=00:00:47.00 bitrate=8168.0kbits/s speed=1.01x elapsed=0:00:46.50    frame= 1426 fps= 30 q=-1.0 size=   47462KiB time=00:00:47.53 bitrate=8179.7kbits/s speed=1.01x elapsed=0:00:47.03    frame= 1442 fps= 30 q=-1.0 size=   47866KiB time=00:00:48.06 bitrate=8157.8kbits/s speed=1.01x elapsed=0:00:47.55    frame= 1457 fps= 30 q=-1.0 size=   48249KiB time=00:00:48.56 bitrate=8138.4kbits/s speed=1.01x elapsed=0:00:48.08    frame= 1473 fps= 30 q=-1.0 size=   48819KiB time=00:00:49.13 bitrate=8139.7kbits/s speed=1.01x elapsed=0:00:48.60    frame= 1489 fps= 30 q=-1.0 size=   49257KiB time=00:00:49.63 bitrate=8129.8kbits/s speed=1.01x elapsed=0:00:49.12    frame= 1505 fps= 30 q=-1.0 size=   49821KiB time=00:00:50.16 bitrate=8135.6kbits/s speed=1.01x elapsed=0:00:49.63    frame= 1520 fps= 30 q=-1.0 size=   50368KiB time=00:00:50.66 bitrate=8143.7kbits/s speed=1.01x elapsed=0:00:50.15    frame= 1535 fps= 30 q=-1.0 size=   51169KiB time=00:00:51.16 bitrate=8192.4kbits/s speed=1.01x elapsed=0:00:50.67    frame= 1551 fps= 30 q=-1.0 size=   51492KiB time=00:00:51.70 bitrate=8159.1kbits/s speed=1.01x elapsed=0:00:51.19    frame= 1566 fps= 30 q=-1.0 size=   51873KiB time=00:00:52.20 bitrate=8140.7kbits/s speed=1.01x elapsed=0:00:51.69    frame= 1582 fps= 30 q=-1.0 size=   52409KiB time=00:00:52.73 bitrate=8141.6kbits/s speed=1.01x elapsed=0:00:52.21    frame= 1597 fps= 30 q=-1.0 size=   53007KiB time=00:00:53.23 bitrate=8157.2kbits/s speed=1.01x elapsed=0:00:52.72    frame= 1612 fps= 30 q=-1.0 size=   53414KiB time=00:00:53.73 bitrate=8143.3kbits/s speed=1.01x elapsed=0:00:53.24    frame= 1628 fps= 30 q=-1.0 size=   53962KiB time=00:00:54.26 bitrate=8146.1kbits/s speed=1.01x elapsed=0:00:53.75    frame= 1643 fps= 30 q=-1.0 size=   54559KiB time=00:00:54.76 bitrate=8160.9kbits/s speed=1.01x elapsed=0:00:54.28    frame= 1660 fps= 30 q=-1.0 size=   55164KiB time=00:00:55.33 bitrate=8166.9kbits/s speed=1.01x elapsed=0:00:54.81    frame= 1675 fps= 30 q=-1.0 size=   55671KiB time=00:00:55.83 bitrate=8168.2kbits/s speed=1.01x elapsed=0:00:55.31    frame= 1690 fps= 30 q=-1.0 size=   55996KiB time=00:00:56.33 bitrate=8143.0kbits/s speed=1.01x elapsed=0:00:55.83    frame= 1706 fps= 30 q=-1.0 size=   56419KiB time=00:00:56.86 bitrate=8127.5kbits/s speed=1.01x elapsed=0:00:56.36    frame= 1721 fps= 30 q=-1.0 size=   56966KiB time=00:00:57.36 bitrate=8134.8kbits/s speed=1.01x elapsed=0:00:56.86    frame= 1736 fps= 30 q=-1.0 size=   57398KiB time=00:00:57.86 bitrate=8125.7kbits/s speed=1.01x elapsed=0:00:57.38    frame= 1752 fps= 30 q=-1.0 size=   57981KiB time=00:00:58.40 bitrate=8133.2kbits/s speed=1.01x elapsed=0:00:57.89    frame= 1768 fps= 30 q=-1.0 size=   58558KiB time=00:00:58.93 bitrate=8139.8kbits/s speed=1.01x elapsed=0:00:58.41    frame= 1783 fps= 30 q=-1.0 size=   59407KiB time=00:00:59.43 bitrate=8188.4kbits/s speed=1.01x elapsed=0:00:58.93    frame= 1799 fps= 30 q=-1.0 size=   59707KiB time=00:00:59.96 bitrate=8156.5kbits/s speed=1.01x elapsed=0:00:59.45    frame= 1814 fps= 30 q=-1.0 size=   60077KiB time=00:01:00.46 bitrate=8139.2kbits/s speed=1.01x elapsed=0:00:59.96    frame= 1829 fps= 30 q=-1.0 size=   60601KiB time=00:01:00.96 bitrate=8142.9kbits/s speed=1.01x elapsed=0:01:00.48    frame= 1845 fps= 30 q=-1.0 size=   61128KiB time=00:01:01.50 bitrate=8142.5kbits/s speed=1.01x elapsed=0:01:01.00    frame= 1861 fps= 30 q=-1.0 size=   61518KiB time=00:01:02.03 bitrate=8124.0kbits/s speed=1.01x elapsed=0:01:01.51    frame= 1876 fps= 30 q=-1.0 size=   61994KiB time=00:01:02.53 bitrate=8121.4kbits/s speed=1.01x elapsed=0:01:02.02    frame= 1892 fps= 30 q=-1.0 size=   62716KiB time=00:01:03.06 bitrate=8146.4kbits/s speed=1.01x elapsed=0:01:02.53    frame= 1907 fps= 30 q=-1.0 size=   63191KiB time=00:01:03.56 bitrate=8143.6kbits/s speed=1.01x elapsed=0:01:03.06    frame= 1922 fps= 30 q=-1.0 size=   63774KiB time=00:01:04.06 bitrate=8154.6kbits/s speed=1.01x elapsed=0:01:03.58    frame= 1938 fps= 30 q=-1.0 size=   64078KiB time=00:01:04.60 bitrate=8125.8kbits/s speed=1.01x elapsed=0:01:04.08    frame= 1954 fps= 30 q=-1.0 size=   64679KiB time=00:01:05.13 bitrate=8134.9kbits/s speed=1.01x elapsed=0:01:04.61    frame= 1969 fps= 30 q=-1.0 size=   65064KiB time=00:01:05.63 bitrate=8121.0kbits/s speed=1.01x elapsed=0:01:05.13    frame= 1985 fps= 30 q=-1.0 size=   65675KiB time=00:01:06.16 bitrate=8131.2kbits/s speed=1.01x elapsed=0:01:05.65    frame= 2000 fps= 30 q=-1.0 size=   66073KiB time=00:01:06.66 bitrate=8119.0kbits/s speed=1.01x elapsed=0:01:06.17    frame= 2015 fps= 30 q=-1.0 size=   66967KiB time=00:01:07.20 bitrate=8163.6kbits/s speed=1.01x elapsed=0:01:06.68    frame= 2031 fps= 30 q=-1.0 size=   67688KiB time=00:01:07.70 bitrate=8190.5kbits/s speed=1.01x elapsed=0:01:07.19    frame= 2046 fps= 30 q=-1.0 size=   68017KiB time=00:01:08.20 bitrate=8170.0kbits/s speed=1.01x elapsed=0:01:07.71    frame= 2062 fps= 30 q=-1.0 size=   68288KiB time=00:01:08.73 bitrate=8139.0kbits/s speed=1.01x elapsed=0:01:08.22    frame= 2077 fps= 30 q=-1.0 size=   68864KiB time=00:01:09.23 bitrate=8148.3kbits/s speed=1.01x elapsed=0:01:08.75    frame= 2093 fps= 30 q=-1.0 size=   69302KiB time=00:01:09.76 bitrate=8137.5kbits/s speed=1.01x elapsed=0:01:09.26    frame= 2108 fps= 30 q=-1.0 size=   69762KiB time=00:01:10.26 bitrate=8133.1kbits/s speed=1.01x elapsed=0:01:09.77    frame= 2124 fps= 30 q=-1.0 size=   70128KiB time=00:01:10.80 bitrate=8114.3kbits/s speed=1.01x elapsed=0:01:10.29    frame= 2139 fps= 30 q=-1.0 size=   70849KiB time=00:01:11.30 bitrate=8140.2kbits/s speed=1.01x elapsed=0:01:10.80    frame= 2155 fps= 30 q=-1.0 size=   71397KiB time=00:01:11.83 bitrate=8142.3kbits/s speed=1.01x elapsed=0:01:11.31    frame= 2170 fps= 30 q=-1.0 size=   71995KiB time=00:01:12.33 bitrate=8153.6kbits/s speed=1.01x elapsed=0:01:11.84    frame= 2186 fps= 30 q=-1.0 size=   72310KiB time=00:01:12.86 bitrate=8129.5kbits/s speed=1.01x elapsed=0:01:12.35    frame= 2201 fps= 30 q=-1.0 size=   72828KiB time=00:01:13.36 bitrate=8131.9kbits/s speed=1.01x elapsed=0:01:12.86    frame= 2217 fps= 30 q=-1.0 size=   73230KiB time=00:01:13.90 bitrate=8117.8kbits/s speed=1.01x elapsed=0:01:13.38    frame= 2232 fps= 30 q=-1.0 size=   73775KiB time=00:01:14.40 bitrate=8123.1kbits/s speed=1.01x elapsed=0:01:13.89    frame= 2248 fps= 30 q=-1.0 size=   74102KiB time=00:01:14.93 bitrate=8101.1kbits/s speed=1.01x elapsed=0:01:14.41    frame= 2263 fps= 30 q=-1.0 size=   74772KiB time=00:01:15.43 bitrate=8120.2kbits/s speed=1.01x elapsed=0:01:14.93    frame= 2279 fps= 30 q=-1.0 size=   75662KiB time=00:01:15.96 bitrate=8159.1kbits/s speed=1.01x elapsed=0:01:15.45    frame= 2294 fps= 30 q=-1.0 size=   75988KiB time=00:01:16.46 bitrate=8140.7kbits/s speed=1.01x elapsed=0:01:15.96    frame= 2309 fps= 30 q=-1.0 size=   76300KiB time=00:01:16.96 bitrate=8121.0kbits/s speed=1.01x elapsed=0:01:16.48    frame= 2325 fps= 30 q=-1.0 size=   76931KiB time=00:01:17.50 bitrate=8131.8kbits/s speed=1.01x elapsed=0:01:16.99    frame= 2340 fps= 30 q=-1.0 size=   77379KiB time=00:01:18.03 bitrate=8123.3kbits/s speed=1.01x elapsed=0:01:17.51    frame= 2356 fps= 30 q=-1.0 size=   77893KiB time=00:01:18.53 bitrate=8125.3kbits/s speed=1.01x elapsed=0:01:18.02    frame= 2371 fps= 30 q=-1.0 size=   78242KiB time=00:01:19.06 bitrate=8106.6kbits/s speed=1.01x elapsed=0:01:18.53    frame= 2387 fps= 30 q=-1.0 size=   79086KiB time=00:01:19.56 bitrate=8142.5kbits/s speed=1.01x elapsed=0:01:19.05    frame= 2402 fps= 30 q=-1.0 size=   79636KiB time=00:01:20.06 bitrate=8147.9kbits/s speed=1.01x elapsed=0:01:19.58    frame= 2418 fps= 30 q=-1.0 size=   80130KiB time=00:01:20.60 bitrate=8144.2kbits/s speed=1.01x elapsed=0:01:20.08    frame= 2434 fps= 30 q=-1.0 size=   80506KiB time=00:01:21.13 bitrate=8128.7kbits/s speed=1.01x elapsed=0:01:20.60    frame= 2449 fps= 30 q=-1.0 size=   80886KiB time=00:01:21.63 bitrate=8117.0kbits/s speed=1.01x elapsed=0:01:21.11    frame= 2465 fps= 30 q=-1.0 size=   81450KiB time=00:01:22.16 bitrate=8120.5kbits/s speed=1.01x elapsed=0:01:21.63    frame= 2480 fps= 30 q=-1.0 size=   81869KiB time=00:01:22.66 bitrate=8113.0kbits/s speed=1.01x elapsed=0:01:22.15    frame= 2495 fps= 30 q=-1.0 size=   82765KiB time=00:01:23.16 bitrate=8152.5kbits/s speed=1.01x elapsed=0:01:22.67    frame= 2511 fps= 30 q=-1.0 size=   83144KiB time=00:01:23.70 bitrate=8137.6kbits/s speed=1.01x elapsed=0:01:23.19    frame= 2526 fps= 30 q=-1.0 size=   83979KiB time=00:01:24.20 bitrate=8170.5kbits/s speed=1.01x elapsed=0:01:23.71    frame= 2542 fps= 30 q=-1.0 size=   84280KiB time=00:01:24.73 bitrate=8148.1kbits/s speed=1.01x elapsed=0:01:24.22    frame= 2557 fps= 30 q=-1.0 size=   84617KiB time=00:01:25.23 bitrate=8132.7kbits/s speed=1.01x elapsed=0:01:24.74    frame= 2573 fps= 30 q=-1.0 size=   85103KiB time=00:01:25.76 bitrate=8128.6kbits/s speed=1.01x elapsed=0:01:25.26    frame= 2588 fps= 30 q=-1.0 size=   85645KiB time=00:01:26.30 bitrate=8129.9kbits/s speed=1.01x elapsed=0:01:25.78    frame= 2604 fps= 30 q=-1.0 size=   86021KiB time=00:01:26.80 bitrate=8118.5kbits/s speed=1.01x elapsed=0:01:26.29    frame= 2620 fps= 30 q=-1.0 size=   86558KiB time=00:01:27.33 bitrate=8119.3kbits/s speed=1.01x elapsed=0:01:26.80    frame= 2635 fps= 30 q=-1.0 size=   87165KiB time=00:01:27.83 bitrate=8129.7kbits/s speed=1.01x elapsed=0:01:27.31    frame= 2651 fps= 30 q=-1.0 size=   87762KiB time=00:01:28.36 bitrate=8135.9kbits/s speed=1.01x elapsed=0:01:27.83    frame= 2666 fps= 30 q=-1.0 size=   88273KiB time=00:01:28.86 bitrate=8137.3kbits/s speed=1.01x elapsed=0:01:28.35    frame= 2681 fps= 30 q=-1.0 size=   88610KiB time=00:01:29.36 bitrate=8122.6kbits/s speed=1.01x elapsed=0:01:28.86    frame= 2697 fps= 30 q=-1.0 size=   89038KiB time=00:01:29.90 bitrate=8113.4kbits/s speed=1.01x elapsed=0:01:29.38    frame= 2712 fps= 30 q=-1.0 size=   89566KiB time=00:01:30.40 bitrate=8116.4kbits/s speed=1.01x elapsed=0:01:29.90    frame= 2728 fps= 30 q=-1.0 size=   90023KiB time=00:01:30.93 bitrate=8110.0kbits/s speed=1.01x elapsed=0:01:30.41    frame= 2743 fps= 30 q=-1.0 size=   90586KiB time=00:01:31.43 bitrate=8116.0kbits/s speed=1.01x elapsed=0:01:30.93    frame= 2758 fps= 30 q=-1.0 size=   90952KiB time=00:01:31.96 bitrate=8101.6kbits/s speed=1.01x elapsed=0:01:31.45    frame= 2774 fps= 30 q=-1.0 size=   92058KiB time=00:01:32.46 bitrate=8155.8kbits/s speed=1.01x elapsed=0:01:31.96    frame= 2790 fps= 30 q=-1.0 size=   92374KiB time=00:01:33.00 bitrate=8136.9kbits/s speed=1.01x elapsed=0:01:32.48    frame= 2805 fps= 30 q=-1.0 size=   92727KiB time=00:01:33.50 bitrate=8124.3kbits/s speed=1.01x elapsed=0:01:32.99    frame= 2821 fps= 30 q=-1.0 size=   93245KiB time=00:01:34.03 bitrate=8123.4kbits/s speed=1.01x elapsed=0:01:33.52    frame= 2836 fps= 30 q=-1.0 size=   93739KiB time=00:01:34.53 bitrate=8123.2kbits/s speed=1.01x elapsed=0:01:34.04    frame= 2852 fps= 30 q=-1.0 size=   94302KiB time=00:01:35.06 bitrate=8126.1kbits/s speed=1.01x elapsed=0:01:34.55    frame= 2868 fps= 30 q=-1.0 size=   94643KiB time=00:01:35.60 bitrate=8110.0kbits/s speed=1.01x elapsed=0:01:35.06    frame= 2883 fps= 30 q=-1.0 size=   95344KiB time=00:01:36.10 bitrate=8127.5kbits/s speed=1.01x elapsed=0:01:35.59    frame= 2899 fps= 30 q=-1.0 size=   95842KiB time=00:01:36.63 bitrate=8124.9kbits/s speed=1.01x elapsed=0:01:36.12    frame= 2914 fps= 30 q=-1.0 size=   96417KiB time=00:01:37.13 bitrate=8131.6kbits/s speed=1.01x elapsed=0:01:36.63    frame= 2929 fps= 30 q=-1.0 size=   96707KiB time=00:01:37.63 bitrate=8114.3kbits/s speed=1.01x elapsed=0:01:37.14    
~~~

### composition-r268-source2-stdout.log
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

### composition-r268-source2.log
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
frame=   31 fps=0.0 q=-1.0 size=    1174KiB time=00:00:01.03 bitrate=9305.4kbits/s speed=1.95x elapsed=0:00:00.53    frame=   47 fps= 45 q=-1.0 size=    2281KiB time=00:00:01.56 bitrate=11929.8kbits/s speed=1.49x elapsed=0:00:01.05    frame=   62 fps= 40 q=-1.0 size=    2629KiB time=00:00:02.06 bitrate=10419.8kbits/s speed=1.32x elapsed=0:00:01.56    frame=   78 fps= 37 q=-1.0 size=    2921KiB time=00:00:02.60 bitrate=9203.4kbits/s speed=1.24x elapsed=0:00:02.09    frame=   93 fps= 36 q=-1.0 size=    3470KiB time=00:00:03.13 bitrate=9071.1kbits/s speed= 1.2x elapsed=0:00:02.60    frame=  108 fps= 35 q=-1.0 size=    3880KiB time=00:00:03.60 bitrate=8829.2kbits/s speed=1.16x elapsed=0:00:03.11    frame=  124 fps= 34 q=-1.0 size=    4406KiB time=00:00:04.13 bitrate=8731.5kbits/s speed=1.14x elapsed=0:00:03.62    frame=  140 fps= 34 q=-1.0 size=    4773KiB time=00:00:04.66 bitrate=8379.1kbits/s speed=1.12x elapsed=0:00:04.15    frame=  155 fps= 33 q=-1.0 size=    5467KiB time=00:00:05.16 bitrate=8668.9kbits/s speed=1.11x elapsed=0:00:04.66    frame=  171 fps= 33 q=-1.0 size=    6003KiB time=00:00:05.70 bitrate=8627.3kbits/s speed= 1.1x elapsed=0:00:05.18    frame=  186 fps= 33 q=-1.0 size=    6571KiB time=00:00:06.20 bitrate=8682.4kbits/s speed=1.09x elapsed=0:00:05.69    frame=  202 fps= 33 q=-1.0 size=    6888KiB time=00:00:06.73 bitrate=8380.3kbits/s speed=1.09x elapsed=0:00:06.20    frame=  217 fps= 32 q=-1.0 size=    7382KiB time=00:00:07.23 bitrate=8360.7kbits/s speed=1.08x elapsed=0:00:06.71    frame=  232 fps= 32 q=-1.0 size=    7816KiB time=00:00:07.73 bitrate=8279.2kbits/s speed=1.07x elapsed=0:00:07.24    frame=  248 fps= 32 q=-1.0 size=    8407KiB time=00:00:08.26 bitrate=8330.6kbits/s speed=1.07x elapsed=0:00:07.75    frame=  263 fps= 32 q=-1.0 size=    8814KiB time=00:00:08.76 bitrate=8235.9kbits/s speed=1.06x elapsed=0:00:08.26    frame=  279 fps= 32 q=-1.0 size=    9867KiB time=00:00:09.30 bitrate=8691.2kbits/s speed=1.06x elapsed=0:00:08.78    frame=  294 fps= 32 q=-1.0 size=   10476KiB time=00:00:09.80 bitrate=8757.4kbits/s speed=1.05x elapsed=0:00:09.30    frame=  309 fps= 31 q=-1.0 size=   10765KiB time=00:00:10.30 bitrate=8561.7kbits/s speed=1.05x elapsed=0:00:09.81    frame=  325 fps= 31 q=-1.0 size=   11067KiB time=00:00:10.83 bitrate=8368.8kbits/s speed=1.05x elapsed=0:00:10.33    frame=  341 fps= 31 q=-1.0 size=   11682KiB time=00:00:11.36 bitrate=8419.6kbits/s speed=1.05x elapsed=0:00:10.84    frame=  356 fps= 31 q=-1.0 size=   12076KiB time=00:00:11.86 bitrate=8336.6kbits/s speed=1.04x elapsed=0:00:11.35    frame=  372 fps= 31 q=-1.0 size=   12551KiB time=00:00:12.40 bitrate=8291.6kbits/s speed=1.04x elapsed=0:00:11.88    frame=  387 fps= 31 q=-1.0 size=   12909KiB time=00:00:12.90 bitrate=8197.6kbits/s speed=1.04x elapsed=0:00:12.39    frame=  403 fps= 31 q=-1.0 size=   13695KiB time=00:00:13.43 bitrate=8351.4kbits/s speed=1.04x elapsed=0:00:12.91    frame=  418 fps= 31 q=-1.0 size=   14165KiB time=00:00:13.93 bitrate=8328.3kbits/s speed=1.04x elapsed=0:00:13.42    frame=  434 fps= 31 q=-1.0 size=   14790KiB time=00:00:14.46 bitrate=8375.0kbits/s speed=1.04x elapsed=0:00:13.93    frame=  449 fps= 31 q=-1.0 size=   15066KiB time=00:00:14.96 bitrate=8246.6kbits/s speed=1.03x elapsed=0:00:14.46    frame=  464 fps= 31 q=-1.0 size=   15574KiB time=00:00:15.46 bitrate=8248.9kbits/s speed=1.03x elapsed=0:00:14.97    frame=  480 fps= 31 q=-1.0 size=   15981KiB time=00:00:16.00 bitrate=8182.0kbits/s speed=1.03x elapsed=0:00:15.48    frame=  495 fps= 31 q=-1.0 size=   16563KiB time=00:00:16.50 bitrate=8223.3kbits/s speed=1.03x elapsed=0:00:15.99    frame=  511 fps= 31 q=-1.0 size=   16979KiB time=00:00:17.03 bitrate=8165.7kbits/s speed=1.03x elapsed=0:00:16.52    frame=  527 fps= 31 q=-1.0 size=   17859KiB time=00:00:17.56 bitrate=8328.4kbits/s speed=1.03x elapsed=0:00:17.03    frame=  542 fps= 31 q=-1.0 size=   18667KiB time=00:00:18.06 bitrate=8464.0kbits/s speed=1.03x elapsed=0:00:17.55    frame=  557 fps= 31 q=-1.0 size=   18902KiB time=00:00:18.56 bitrate=8340.0kbits/s speed=1.03x elapsed=0:00:18.07    frame=  573 fps= 31 q=-1.0 size=   19308KiB time=00:00:19.10 bitrate=8281.3kbits/s speed=1.03x elapsed=0:00:18.60    frame=  589 fps= 31 q=-1.0 size=   19773KiB time=00:00:19.63 bitrate=8250.2kbits/s speed=1.03x elapsed=0:00:19.11    frame=  604 fps= 31 q=-1.0 size=   20344KiB time=00:00:20.13 bitrate=8277.8kbits/s speed=1.03x elapsed=0:00:19.63    frame=  619 fps= 31 q=-1.0 size=   20706KiB time=00:00:20.66 bitrate=8207.7kbits/s speed=1.03x elapsed=0:00:20.14    frame=  635 fps= 31 q=-1.0 size=   21262KiB time=00:00:21.16 bitrate=8229.0kbits/s speed=1.02x elapsed=0:00:20.67    frame=  651 fps= 31 q=-1.0 size=   21916KiB time=00:00:21.70 bitrate=8273.4kbits/s speed=1.02x elapsed=0:00:21.18    frame=  667 fps= 31 q=-1.0 size=   22490KiB time=00:00:22.23 bitrate=8286.4kbits/s speed=1.02x elapsed=0:00:21.71    frame=  682 fps= 31 q=-1.0 size=   22949KiB time=00:00:22.73 bitrate=8269.7kbits/s speed=1.02x elapsed=0:00:22.22    frame=  698 fps= 31 q=-1.0 size=   23330KiB time=00:00:23.26 bitrate=8214.2kbits/s speed=1.02x elapsed=0:00:22.73    frame=  713 fps= 31 q=-1.0 size=   23719KiB time=00:00:23.76 bitrate=8175.7kbits/s speed=1.02x elapsed=0:00:23.25    frame=  729 fps= 31 q=-1.0 size=   24317KiB time=00:00:24.30 bitrate=8197.7kbits/s speed=1.02x elapsed=0:00:23.77    frame=  744 fps= 31 q=-1.0 size=   24736KiB time=00:00:24.80 bitrate=8170.8kbits/s speed=1.02x elapsed=0:00:24.28    frame=  759 fps= 31 q=-1.0 size=   25193KiB time=00:00:25.30 bitrate=8157.5kbits/s speed=1.02x elapsed=0:00:24.79    frame=  775 fps= 31 q=-1.0 size=   25725KiB time=00:00:25.83 bitrate=8157.5kbits/s speed=1.02x elapsed=0:00:25.32    frame=  790 fps= 31 q=-1.0 size=   26684KiB time=00:00:26.33 bitrate=8301.1kbits/s speed=1.02x elapsed=0:00:25.83    frame=  806 fps= 31 q=-1.0 size=   27000KiB time=00:00:26.86 bitrate=8232.8kbits/s speed=1.02x elapsed=0:00:26.35    frame=  821 fps= 31 q=-1.0 size=   27374KiB time=00:00:27.36 bitrate=8194.1kbits/s speed=1.02x elapsed=0:00:26.86    frame=  837 fps= 31 q=-1.0 size=   27919KiB time=00:00:27.90 bitrate=8197.6kbits/s speed=1.02x elapsed=0:00:27.38    frame=  852 fps= 31 q=-1.0 size=   28441KiB time=00:00:28.40 bitrate=8203.9kbits/s speed=1.02x elapsed=0:00:27.89    frame=  868 fps= 31 q=-1.0 size=   28835KiB time=00:00:28.93 bitrate=8164.0kbits/s speed=1.02x elapsed=0:00:28.42    frame=  884 fps= 31 q=-1.0 size=   29343KiB time=00:00:29.46 bitrate=8157.7kbits/s speed=1.02x elapsed=0:00:28.94    frame=  899 fps= 31 q=-1.0 size=   29962KiB time=00:00:29.96 bitrate=8190.9kbits/s speed=1.02x elapsed=0:00:29.45    frame=  914 fps= 30 q=-1.0 size=   30538KiB time=00:00:30.46 bitrate=8211.2kbits/s speed=1.02x elapsed=0:00:29.97    frame=  930 fps= 31 q=-1.0 size=   31097KiB time=00:00:31.00 bitrate=8217.6kbits/s speed=1.02x elapsed=0:00:30.48    frame=  945 fps= 30 q=-1.0 size=   31432KiB time=00:00:31.50 bitrate=8174.4kbits/s speed=1.02x elapsed=0:00:31.00    frame=  961 fps= 30 q=-1.0 size=   31850KiB time=00:00:32.03 bitrate=8145.1kbits/s speed=1.02x elapsed=0:00:31.52    frame=  977 fps= 30 q=-1.0 size=   32404KiB time=00:00:32.56 bitrate=8151.0kbits/s speed=1.02x elapsed=0:00:32.03    frame=  992 fps= 30 q=-1.0 size=   33024KiB time=00:00:33.06 bitrate=8181.5kbits/s speed=1.02x elapsed=0:00:32.54    frame= 1007 fps= 30 q=-1.0 size=   33314KiB time=00:00:33.56 bitrate=8130.3kbits/s speed=1.02x elapsed=0:00:33.06    frame= 1023 fps= 30 q=-1.0 size=   34161KiB time=00:00:34.10 bitrate=8206.6kbits/s speed=1.02x elapsed=0:00:33.58    frame= 1039 fps= 30 q=-1.0 size=   35003KiB time=00:00:34.63 bitrate=8279.4kbits/s speed=1.02x elapsed=0:00:34.10    frame= 1054 fps= 30 q=-1.0 size=   35366KiB time=00:00:35.13 bitrate=8246.2kbits/s speed=1.01x elapsed=0:00:34.61    frame= 1069 fps= 30 q=-1.0 size=   35648KiB time=00:00:35.63 bitrate=8195.3kbits/s speed=1.01x elapsed=0:00:35.13    frame= 1085 fps= 30 q=-1.0 size=   36221KiB time=00:00:36.16 bitrate=8204.3kbits/s speed=1.01x elapsed=0:00:35.64    frame= 1101 fps= 30 q=-1.0 size=   36654KiB time=00:00:36.70 bitrate=8181.8kbits/s speed=1.01x elapsed=0:00:36.16    frame= 1116 fps= 30 q=-1.0 size=   37115KiB time=00:00:37.20 bitrate=8173.3kbits/s speed=1.01x elapsed=0:00:36.69    frame= 1131 fps= 30 q=-1.0 size=   37451KiB time=00:00:37.70 bitrate=8137.9kbits/s speed=1.01x elapsed=0:00:37.20    frame= 1147 fps= 30 q=-1.0 size=   38183KiB time=00:00:38.23 bitrate=8181.2kbits/s speed=1.01x elapsed=0:00:37.72    frame= 1163 fps= 30 q=-1.0 size=   38724KiB time=00:00:38.76 bitrate=8183.0kbits/s speed=1.01x elapsed=0:00:38.24    frame= 1178 fps= 30 q=-1.0 size=   39327KiB time=00:00:39.26 bitrate=8204.6kbits/s speed=1.01x elapsed=0:00:38.75    frame= 1194 fps= 30 q=-1.0 size=   39608KiB time=00:00:39.80 bitrate=8152.5kbits/s speed=1.01x elapsed=0:00:39.27    frame= 1209 fps= 30 q=-1.0 size=   40114KiB time=00:00:40.30 bitrate=8154.3kbits/s speed=1.01x elapsed=0:00:39.79    frame= 1224 fps= 30 q=-1.0 size=   40498KiB time=00:00:40.80 bitrate=8131.4kbits/s speed=1.01x elapsed=0:00:40.30    frame= 1239 fps= 30 q=-1.0 size=   41061KiB time=00:00:41.30 bitrate=8144.5kbits/s speed=1.01x elapsed=0:00:40.81    frame= 1255 fps= 30 q=-1.0 size=   41473KiB time=00:00:41.86 bitrate=8115.0kbits/s speed=1.01x elapsed=0:00:41.33    frame= 1270 fps= 30 q=-1.0 size=   42208KiB time=00:00:42.33 bitrate=8167.8kbits/s speed=1.01x elapsed=0:00:41.84    frame= 1286 fps= 30 q=-1.0 size=   43155KiB time=00:00:42.86 bitrate=8247.0kbits/s speed=1.01x elapsed=0:00:42.36    frame= 1301 fps= 30 q=-1.0 size=   43443KiB time=00:00:43.36 bitrate=8206.4kbits/s speed=1.01x elapsed=0:00:42.87    frame= 1317 fps= 30 q=-1.0 size=   43745KiB time=00:00:43.90 bitrate=8163.2kbits/s speed=1.01x elapsed=0:00:43.39    frame= 1333 fps= 30 q=-1.0 size=   44319KiB time=00:00:44.43 bitrate=8171.0kbits/s speed=1.01x elapsed=0:00:43.91    frame= 1348 fps= 30 q=-1.0 size=   44728KiB time=00:00:44.93 bitrate=8154.5kbits/s speed=1.01x elapsed=0:00:44.43    frame= 1363 fps= 30 q=-1.0 size=   45196KiB time=00:00:45.43 bitrate=8149.1kbits/s speed=1.01x elapsed=0:00:44.94    frame= 1379 fps= 30 q=-1.0 size=   45573KiB time=00:00:45.96 bitrate=8121.9kbits/s speed=1.01x elapsed=0:00:45.45    frame= 1394 fps= 30 q=-1.0 size=   46368KiB time=00:00:46.46 bitrate=8174.6kbits/s speed=1.01x elapsed=0:00:45.96    frame= 1410 fps= 30 q=-1.0 size=   46862KiB time=00:00:47.00 bitrate=8168.0kbits/s speed=1.01x elapsed=0:00:46.50    frame= 1426 fps= 30 q=-1.0 size=   47462KiB time=00:00:47.53 bitrate=8179.7kbits/s speed=1.01x elapsed=0:00:47.02    frame= 1442 fps= 30 q=-1.0 size=   47866KiB time=00:00:48.06 bitrate=8157.8kbits/s speed=1.01x elapsed=0:00:47.54    frame= 1457 fps= 30 q=-1.0 size=   48249KiB time=00:00:48.56 bitrate=8138.4kbits/s speed=1.01x elapsed=0:00:48.07    frame= 1473 fps= 30 q=-1.0 size=   48819KiB time=00:00:49.10 bitrate=8145.2kbits/s speed=1.01x elapsed=0:00:48.59    frame= 1489 fps= 30 q=-1.0 size=   49257KiB time=00:00:49.63 bitrate=8129.8kbits/s speed=1.01x elapsed=0:00:49.12    frame= 1504 fps= 30 q=-1.0 size=   49802KiB time=00:00:50.13 bitrate=8137.8kbits/s speed=1.01x elapsed=0:00:49.63    frame= 1520 fps= 30 q=-1.0 size=   50368KiB time=00:00:50.66 bitrate=8143.7kbits/s speed=1.01x elapsed=0:00:50.14    frame= 1535 fps= 30 q=-1.0 size=   51169KiB time=00:00:51.16 bitrate=8192.4kbits/s speed=1.01x elapsed=0:00:50.66    frame= 1551 fps= 30 q=-1.0 size=   51492KiB time=00:00:51.70 bitrate=8159.1kbits/s speed=1.01x elapsed=0:00:51.18    frame= 1566 fps= 30 q=-1.0 size=   51873KiB time=00:00:52.20 bitrate=8140.7kbits/s speed=1.01x elapsed=0:00:51.68    frame= 1581 fps= 30 q=-1.0 size=   52390KiB time=00:00:52.70 bitrate=8143.8kbits/s speed=1.01x elapsed=0:00:52.20    frame= 1597 fps= 30 q=-1.0 size=   53007KiB time=00:00:53.23 bitrate=8157.2kbits/s speed=1.01x elapsed=0:00:52.72    frame= 1612 fps= 30 q=-1.0 size=   53414KiB time=00:00:53.73 bitrate=8143.3kbits/s speed=1.01x elapsed=0:00:53.24    frame= 1628 fps= 30 q=-1.0 size=   53962KiB time=00:00:54.26 bitrate=8146.1kbits/s speed=1.01x elapsed=0:00:53.75    frame= 1643 fps= 30 q=-1.0 size=   54559KiB time=00:00:54.76 bitrate=8160.9kbits/s speed=1.01x elapsed=0:00:54.27    frame= 1660 fps= 30 q=-1.0 size=   55164KiB time=00:00:55.33 bitrate=8166.9kbits/s speed=1.01x elapsed=0:00:54.80    frame= 1675 fps= 30 q=-1.0 size=   55671KiB time=00:00:55.83 bitrate=8168.2kbits/s speed=1.01x elapsed=0:00:55.31    frame= 1690 fps= 30 q=-1.0 size=   55996KiB time=00:00:56.33 bitrate=8143.0kbits/s speed=1.01x elapsed=0:00:55.82    frame= 1706 fps= 30 q=-1.0 size=   56419KiB time=00:00:56.86 bitrate=8127.5kbits/s speed=1.01x elapsed=0:00:56.35    frame= 1721 fps= 30 q=-1.0 size=   56966KiB time=00:00:57.36 bitrate=8134.8kbits/s speed=1.01x elapsed=0:00:56.85    frame= 1737 fps= 30 q=-1.0 size=   57428KiB time=00:00:57.90 bitrate=8125.2kbits/s speed=1.01x elapsed=0:00:57.37    frame= 1752 fps= 30 q=-1.0 size=   57981KiB time=00:00:58.40 bitrate=8133.2kbits/s speed=1.01x elapsed=0:00:57.89    frame= 1767 fps= 30 q=-1.0 size=   58417KiB time=00:00:58.93 bitrate=8120.2kbits/s speed=1.01x elapsed=0:00:58.40    frame= 1783 fps= 30 q=-1.0 size=   59407KiB time=00:00:59.43 bitrate=8188.4kbits/s speed=1.01x elapsed=0:00:58.93    frame= 1798 fps= 30 q=-1.0 size=   59690KiB time=00:00:59.93 bitrate=8158.7kbits/s speed=1.01x elapsed=0:00:59.44    frame= 1814 fps= 30 q=-1.0 size=   60077KiB time=00:01:00.46 bitrate=8139.2kbits/s speed=1.01x elapsed=0:00:59.95    frame= 1830 fps= 30 q=-1.0 size=   60621KiB time=00:01:01.00 bitrate=8141.0kbits/s speed=1.01x elapsed=0:01:00.47    frame= 1845 fps= 30 q=-1.0 size=   61128KiB time=00:01:01.50 bitrate=8142.5kbits/s speed=1.01x elapsed=0:01:00.99    frame= 1860 fps= 30 q=-1.0 size=   61497KiB time=00:01:02.03 bitrate=8121.2kbits/s speed=1.01x elapsed=0:01:01.50    frame= 1876 fps= 30 q=-1.0 size=   61994KiB time=00:01:02.53 bitrate=8121.4kbits/s speed=1.01x elapsed=0:01:02.02    frame= 1891 fps= 30 q=-1.0 size=   62618KiB time=00:01:03.03 bitrate=8138.1kbits/s speed=1.01x elapsed=0:01:02.53    frame= 1907 fps= 30 q=-1.0 size=   63191KiB time=00:01:03.56 bitrate=8143.6kbits/s speed=1.01x elapsed=0:01:03.05    frame= 1923 fps= 30 q=-1.0 size=   63799KiB time=00:01:04.10 bitrate=8153.5kbits/s speed=1.01x elapsed=0:01:03.57    frame= 1937 fps= 30 q=-1.0 size=   64054KiB time=00:01:04.56 bitrate=8127.0kbits/s speed=1.01x elapsed=0:01:04.07    frame= 1953 fps= 30 q=-1.0 size=   64655KiB time=00:01:05.10 bitrate=8136.0kbits/s speed=1.01x elapsed=0:01:04.60    frame= 1968 fps= 30 q=-1.0 size=   65037KiB time=00:01:05.60 bitrate=8121.7kbits/s speed=1.01x elapsed=0:01:05.12    frame= 1984 fps= 30 q=-1.0 size=   65650KiB time=00:01:06.13 bitrate=8132.1kbits/s speed=1.01x elapsed=0:01:05.64    frame= 2000 fps= 30 q=-1.0 size=   66073KiB time=00:01:06.66 bitrate=8119.0kbits/s speed=1.01x elapsed=0:01:06.16    frame= 2015 fps= 30 q=-1.0 size=   66967KiB time=00:01:07.16 bitrate=8167.7kbits/s speed=1.01x elapsed=0:01:06.67    frame= 2031 fps= 30 q=-1.0 size=   67688KiB time=00:01:07.70 bitrate=8190.5kbits/s speed=1.01x elapsed=0:01:07.19    frame= 2047 fps= 30 q=-1.0 size=   68034KiB time=00:01:08.23 bitrate=8168.1kbits/s speed=1.01x elapsed=0:01:07.70    frame= 2062 fps= 30 q=-1.0 size=   68288KiB time=00:01:08.73 bitrate=8139.0kbits/s speed=1.01x elapsed=0:01:08.21    frame= 2077 fps= 30 q=-1.0 size=   68864KiB time=00:01:09.23 bitrate=8148.3kbits/s speed=1.01x elapsed=0:01:08.74    frame= 2093 fps= 30 q=-1.0 size=   69302KiB time=00:01:09.76 bitrate=8137.5kbits/s speed=1.01x elapsed=0:01:09.25    frame= 2109 fps= 30 q=-1.0 size=   69783KiB time=00:01:10.30 bitrate=8131.8kbits/s speed=1.01x elapsed=0:01:09.77    frame= 2124 fps= 30 q=-1.0 size=   70128KiB time=00:01:10.80 bitrate=8114.3kbits/s speed=1.01x elapsed=0:01:10.28    frame= 2140 fps= 30 q=-1.0 size=   70874KiB time=00:01:11.33 bitrate=8139.2kbits/s speed=1.01x elapsed=0:01:10.80    frame= 2154 fps= 30 q=-1.0 size=   71364KiB time=00:01:11.80 bitrate=8142.2kbits/s speed=1.01x elapsed=0:01:11.31    frame= 2170 fps= 30 q=-1.0 size=   71995KiB time=00:01:12.33 bitrate=8153.6kbits/s speed=1.01x elapsed=0:01:11.84    frame= 2186 fps= 30 q=-1.0 size=   72310KiB time=00:01:12.86 bitrate=8129.5kbits/s speed=1.01x elapsed=0:01:12.34    frame= 2201 fps= 30 q=-1.0 size=   72828KiB time=00:01:13.36 bitrate=8131.9kbits/s speed=1.01x elapsed=0:01:12.85    frame= 2216 fps= 30 q=-1.0 size=   73202KiB time=00:01:13.86 bitrate=8118.3kbits/s speed=1.01x elapsed=0:01:13.38    frame= 2232 fps= 30 q=-1.0 size=   73775KiB time=00:01:14.40 bitrate=8123.1kbits/s speed=1.01x elapsed=0:01:13.89    frame= 2247 fps= 30 q=-1.0 size=   74067KiB time=00:01:14.90 bitrate=8100.9kbits/s speed=1.01x elapsed=0:01:14.41    frame= 2263 fps= 30 q=-1.0 size=   74772KiB time=00:01:15.43 bitrate=8120.2kbits/s speed=1.01x elapsed=0:01:14.93    frame= 2278 fps= 30 q=-1.0 size=   75625KiB time=00:01:15.93 bitrate=8158.7kbits/s speed=1.01x elapsed=0:01:15.44    frame= 2294 fps= 30 q=-1.0 size=   75988KiB time=00:01:16.46 bitrate=8140.7kbits/s speed=1.01x elapsed=0:01:15.96    frame= 2309 fps= 30 q=-1.0 size=   76300KiB time=00:01:16.96 bitrate=8121.0kbits/s speed=1.01x elapsed=0:01:16.47    frame= 2325 fps= 30 q=-1.0 size=   76931KiB time=00:01:17.50 bitrate=8131.8kbits/s speed=1.01x elapsed=0:01:16.98    frame= 2341 fps= 30 q=-1.0 size=   77411KiB time=00:01:18.03 bitrate=8126.7kbits/s speed=1.01x elapsed=0:01:17.50    frame= 2356 fps= 30 q=-1.0 size=   77893KiB time=00:01:18.53 bitrate=8125.3kbits/s speed=1.01x elapsed=0:01:18.01    frame= 2371 fps= 30 q=-1.0 size=   78242KiB time=00:01:19.03 bitrate=8110.0kbits/s speed=1.01x elapsed=0:01:18.53    frame= 2386 fps= 30 q=-1.0 size=   79063KiB time=00:01:19.53 bitrate=8143.6kbits/s speed=1.01x elapsed=0:01:19.04    frame= 2403 fps= 30 q=-1.0 size=   79670KiB time=00:01:20.10 bitrate=8148.0kbits/s speed=1.01x elapsed=0:01:19.57    frame= 2418 fps= 30 q=-1.0 size=   80130KiB time=00:01:20.60 bitrate=8144.2kbits/s speed=1.01x elapsed=0:01:20.07    frame= 2433 fps= 30 q=-1.0 size=   80491KiB time=00:01:21.10 bitrate=8130.5kbits/s speed=1.01x elapsed=0:01:20.59    frame= 2449 fps= 30 q=-1.0 size=   80886KiB time=00:01:21.63 bitrate=8117.0kbits/s speed=1.01x elapsed=0:01:21.11    frame= 2464 fps= 30 q=-1.0 size=   81432KiB time=00:01:22.13 bitrate=8122.1kbits/s speed=1.01x elapsed=0:01:21.62    frame= 2479 fps= 30 q=-1.0 size=   81842KiB time=00:01:22.63 bitrate=8113.6kbits/s speed=1.01x elapsed=0:01:22.14    frame= 2495 fps= 30 q=-1.0 size=   82765KiB time=00:01:23.16 bitrate=8152.5kbits/s speed=1.01x elapsed=0:01:22.67    frame= 2511 fps= 30 q=-1.0 size=   83144KiB time=00:01:23.70 bitrate=8137.6kbits/s speed=1.01x elapsed=0:01:23.18    frame= 2527 fps= 30 q=-1.0 size=   84011KiB time=00:01:24.23 bitrate=8170.4kbits/s speed=1.01x elapsed=0:01:23.70    frame= 2542 fps= 30 q=-1.0 size=   84280KiB time=00:01:24.73 bitrate=8148.1kbits/s speed=1.01x elapsed=0:01:24.22    frame= 2557 fps= 30 q=-1.0 size=   84617KiB time=00:01:25.23 bitrate=8132.7kbits/s speed=1.01x elapsed=0:01:24.73    frame= 2573 fps= 30 q=-1.0 size=   85103KiB time=00:01:25.76 bitrate=8128.6kbits/s speed=1.01x elapsed=0:01:25.25    frame= 2589 fps= 30 q=-1.0 size=   85671KiB time=00:01:26.30 bitrate=8132.3kbits/s speed=1.01x elapsed=0:01:25.77    frame= 2604 fps= 30 q=-1.0 size=   86021KiB time=00:01:26.80 bitrate=8118.5kbits/s speed=1.01x elapsed=0:01:26.28    frame= 2619 fps= 30 q=-1.0 size=   86535KiB time=00:01:27.30 bitrate=8120.2kbits/s speed=1.01x elapsed=0:01:26.79    frame= 2635 fps= 30 q=-1.0 size=   87165KiB time=00:01:27.83 bitrate=8129.7kbits/s speed=1.01x elapsed=0:01:27.31    frame= 2650 fps= 30 q=-1.0 size=   87727KiB time=00:01:28.33 bitrate=8135.8kbits/s speed=1.01x elapsed=0:01:27.82    frame= 2666 fps= 30 q=-1.0 size=   88273KiB time=00:01:28.86 bitrate=8137.3kbits/s speed=1.01x elapsed=0:01:28.35    frame= 2681 fps= 30 q=-1.0 size=   88610KiB time=00:01:29.36 bitrate=8122.6kbits/s speed=1.01x elapsed=0:01:28.85    frame= 2697 fps= 30 q=-1.0 size=   89038KiB time=00:01:29.90 bitrate=8113.4kbits/s speed=1.01x elapsed=0:01:29.38    frame= 2712 fps= 30 q=-1.0 size=   89566KiB time=00:01:30.40 bitrate=8116.4kbits/s speed=1.01x elapsed=0:01:29.89    frame= 2727 fps= 30 q=-1.0 size=   89990KiB time=00:01:30.93 bitrate=8107.0kbits/s speed=1.01x elapsed=0:01:30.41    frame= 2743 fps= 30 q=-1.0 size=   90586KiB time=00:01:31.43 bitrate=8116.0kbits/s speed=1.01x elapsed=0:01:30.93    frame= 2758 fps= 30 q=-1.0 size=   90952KiB time=00:01:31.96 bitrate=8101.6kbits/s speed=1.01x elapsed=0:01:31.44    frame= 2774 fps= 30 q=-1.0 size=   92058KiB time=00:01:32.46 bitrate=8155.8kbits/s speed=1.01x elapsed=0:01:31.95    frame= 2789 fps= 30 q=-1.0 size=   92333KiB time=00:01:32.96 bitrate=8136.1kbits/s speed=1.01x elapsed=0:01:32.47    frame= 2805 fps= 30 q=-1.0 size=   92727KiB time=00:01:33.50 bitrate=8124.3kbits/s speed=1.01x elapsed=0:01:32.98    frame= 2820 fps= 30 q=-1.0 size=   93227KiB time=00:01:34.03 bitrate=8121.7kbits/s speed=1.01x elapsed=0:01:33.51    frame= 2836 fps= 30 q=-1.0 size=   93739KiB time=00:01:34.53 bitrate=8123.2kbits/s speed=1.01x elapsed=0:01:34.04    frame= 2852 fps= 30 q=-1.0 size=   94302KiB time=00:01:35.06 bitrate=8126.1kbits/s speed=1.01x elapsed=0:01:34.55    frame= 2867 fps= 30 q=-1.0 size=   94617KiB time=00:01:35.56 bitrate=8110.6kbits/s speed=1.01x elapsed=0:01:35.06    frame= 2883 fps= 30 q=-1.0 size=   95344KiB time=00:01:36.10 bitrate=8127.5kbits/s speed=1.01x elapsed=0:01:35.59    frame= 2899 fps= 30 q=-1.0 size=   95842KiB time=00:01:36.63 bitrate=8124.9kbits/s speed=1.01x elapsed=0:01:36.11    frame= 2914 fps= 30 q=-1.0 size=   96417KiB time=00:01:37.13 bitrate=8131.6kbits/s speed=1.01x elapsed=0:01:36.62    frame= 2929 fps= 30 q=-1.0 size=   96707KiB time=00:01:37.66 bitrate=8111.5kbits/s speed=1.01x elapsed=0:01:37.14    frame= 2945 fps= 30 q=-1.0 size=   97294KiB time=00:01:38.16 bitrate=8119.2kbits/s speed=1.01x elapsed=0:01:37.66    
~~~

### composition-r268-vlc.log
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
file debug: opening logfile `D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r268-vlc.log'
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
main debug: stream filter added to 038523d8
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
main debug: using timeshift granularity of 50 MiB
qt debug: IM: Setting an input
main debug: using timeshift path: C:\Users\ROG\AppData\Local\Temp
main debug: `rtp://@127.0.0.1:62020' gives access `rtp' demux `any' path `@127.0.0.1:62020'
main debug: creating demux: access='rtp' demux='any' location='@127.0.0.1:62020' file='\\@127.0.0.1:62020'
main debug: looking for access_demux module matching "rtp": 15 candidates
main debug: looking for meta fetcher module matching "any": 1 candidates
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\meta\fetcher
lua debug: Trying Lua scripts in D:\VideoLAN\VLC\lua\meta\fetcher
main debug: no meta fetcher modules matched
main debug: looking for art finder module matching "any": 2 candidates
main debug: net: opening 127.0.0.1 datagram port 62020
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\meta\art
lua debug: Trying Lua scripts in D:\VideoLAN\VLC\lua\meta\art
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\art\00_musicbrainz.luac
main debug: using access_demux module "rtp"
lua debug: skipping script (unmatched scope) D:\VideoLAN\VLC\lua\meta\art\00_musicbrainz.luac
main debug: looking for meta reader module matching "any": 2 candidates
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\art\01_googleimage.luac
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\meta\reader
lua debug: Trying Lua scripts in D:\VideoLAN\VLC\lua\meta\reader
lua debug: skipping script (unmatched scope) D:\VideoLAN\VLC\lua\meta\art\01_googleimage.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\reader\filename.luac
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\art\02_frenchtv.luac
lua debug: skipping script (unmatched scope) D:\VideoLAN\VLC\lua\meta\art\02_frenchtv.luac
main debug: no meta reader modules matched
main debug: `rtp://@127.0.0.1:62020' successfully opened
lua debug: Trying Lua playlist script D:\VideoLAN\VLC\lua\meta\art\03_lastfm.luac
lua debug: skipping script (unmatched scope) D:\VideoLAN\VLC\lua\meta\art\03_lastfm.luac
main debug: no art finder modules matched
main debug: accepted socket 1468 (from socket 1188)
main error: read error: No error
main debug: accepted socket 1500 (from socket 1188)
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
main debug: saving media library to file C:\Users\ROG\AppData\Roaming\vlc\ml.xspf.tmp39276
main debug: looking for playlist export module matching "export-xspf": 4 candidates
main debug: using playlist export module "export"
main debug: removing module "export"
main debug: deleting item `Media Library'
main debug: deleting item `rtp://127.0.0.1:62020'
main debug: deleting item `Playlist'
main debug: removing module "memory"
-- logger module stopped --

~~~

## 独立复核及后续诊断

Thread32Next失败后立即GetLastError，CloseHandle之前保存；sample、RunController和CLI保持ErrorInfo，describe仅在nativeCode非0时显示。因此本次可能采到0，不能声称丢失非零错误码。微软官方合同：https://learn.microsoft.com/en-us/windows/win32/api/tlhelp32/nf-tlhelp32-thread32next 。不接受0作为枚举结束，不改失败语义。下一修改只在原错误消息显式记录native（包括0）、结构尺寸、枚举项数和匹配线程数，定位API与插桩边界。Windows独有Toolhelp API，Linux使用procfs不复用；共享错误传播已检查。12个临时文件、六个PID及工具临时路径均已清理或确认不存在。

最小错误诊断已实现并获两名独立审查者明确局部PASS。entry_size仅表示失败返回后结构体内值，不能证明失败调用写入有效记录。源码冻结，Release全量重建19965进行中；尚未取得构建结果，也尚未运行增强诊断版实流。

Release重建19965已成功，增强诊断的r269实流重现native=0；详细记录见realtime-composition-r269-validation.md。
