# r288：三源合屏 MPEG-TS/UDP 启动失败

Release全量重建成功（732项，configure/build=0，四项产物齐全），realtime SHA256 9C59E4D485CA51F1EC6E29A3D53578D84B67B516094A66D5494FEF0A388D6288。首轮构建C2039：UDP节点残留已删除materializeProtocolBatch调用；修复shared owner prepare/fresh clock complete后双独立复审PASS并重建。未运行local CLI。

源PID33748、19996、40004，CLI35668。CLI启动失败exit=1：MPEG-TS datagram materializer requires id and runtime authority。未到10秒VLC启动条件，无快照或有效输出验证；不算通过。源将在记录后按精确PID清理，清理退出不算自然结束。

## 实际命令

```powershell
$ErrorActionPreference='Stop'
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r288-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r288-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r288-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r288-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r288-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r288-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r288 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62020 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r288-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r288-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$vlc=$null
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
if(-not $vlc -and $watch.Elapsed.TotalSeconds -ge 10) {
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r288-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r288-native- --snapshot-format=png udp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
}
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r288-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
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

## 原始错误与启动末尾

~~~
[2026-10-21 14:21:33.988] [info] [graph][runtime.node] register node=68 name=composition-r288.source_2.video.filter kind=VideoFilter
[2026-10-21 14:21:33.988] [info] [graph][runtime.node] register node=69 name=composition-r288.source_2.audio.encode.codec_resolver kind=AudioCodecResolver
[2026-10-21 14:21:33.989] [info] [graph][runtime.node] register node=70 name=composition-r288.source_2.audio.encode.decode kind=AudioDecode
[2026-10-21 14:21:33.989] [info] [graph][runtime.node] register node=71 name=composition-r288.source_2.audio.encode.startup_trim kind=Unknown
[2026-10-21 14:21:33.989] [info] [graph][runtime.node] register node=72 name=composition-r288.source_2.audio.encode.drift_controller kind=AudioDriftController
[2026-10-21 14:21:33.989] [info] [graph][runtime.node] register node=73 name=composition-r288.source_2.audio.encode.resample kind=AudioResample
[2026-10-21 14:21:33.989] [info] [graph][runtime.node] register node=74 name=composition-r288.output.scheduler.scheduler kind=AvOutputScheduler
[2026-10-21 14:21:33.989] [info] [graph][runtime.node] register node=75 name=composition-r288.output.scheduler.router kind=ScheduledOutputRouter
[2026-10-21 14:21:33.989] [info] [graph][runtime.node] register node=76 name=composition-r288.output.mpegts.mux kind=FileMux
[2026-10-21 14:21:33.989] [info] [graph][runtime.node] register node=77 name=composition-r288.output.mpegts.plan.source kind=Unknown
[2026-10-21 14:21:33.990] [info] [graph][runtime.node] register node=78 name=composition-r288.output.mpegts.scheduled.adapter kind=Unknown
[2026-10-21 14:21:33.990] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=1957 completion_waits=1957 completed_datagrams=4333 drained_datagrams=2377 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-21 14:21:33.990] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=1946 completion_waits=1946 completed_datagrams=4369 drained_datagrams=2424 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-21 14:21:33.991] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=1846 completion_waits=1846 completed_datagrams=4165 drained_datagrams=2319 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-21 14:21:33.991] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=204 completion_waits=204 completed_datagrams=209 drained_datagrams=6 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-10-21 14:21:33.992] [info] [graph][runtime.lifecycle] abort.begin
[2026-10-21 14:21:33.992] [info] [graph][runtime.lifecycle] abort.done state=Aborted
[2026-10-21 14:21:33.996] [info] [graph][runtime.node] cuda_canvas_call stage=context_push count=4 operation=copy maximum_ns=17000 begin_steady_ns=20277847129800 end_steady_ns=20277847146800 native_result=0 thread_cpu_ns=0
[2026-10-21 14:21:33.996] [info] [graph][runtime.node] cuda_canvas_call stage=source_wait count=6 operation=copy maximum_ns=6900 begin_steady_ns=20277847219100 end_steady_ns=20277847226000 native_result=0 thread_cpu_ns=0
[2026-10-21 14:21:33.996] [info] [graph][runtime.node] cuda_canvas_call stage=plane_copy count=14 operation=download maximum_ns=347400 begin_steady_ns=20277846283200 end_steady_ns=20277846630600 native_result=0 thread_cpu_ns=0
[2026-10-21 14:21:33.996] [info] [graph][runtime.node] cuda_canvas_call stage=destination_wait count=2 operation=upload maximum_ns=92200 begin_steady_ns=20277845866600 end_steady_ns=20277845958800 native_result=0 thread_cpu_ns=0
[2026-10-21 14:21:33.996] [info] [graph][runtime.node] cuda_canvas_call stage=context_pop count=4 operation=copy maximum_ns=4100 begin_steady_ns=20277846969400 end_steady_ns=20277846973500 native_result=0 thread_cpu_ns=0
[2026-10-21 14:21:33.996] [info] [graph][runtime.node] cuda_canvas_call stage=batch_completion count=2 operation=copy maximum_ns=182500 begin_steady_ns=20277846786400 end_steady_ns=20277846968900 native_result=0 thread_cpu_ns=0
[2026-10-21 14:21:34.036] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=failed detail="MPEG-TS datagram materializer requires id and runtime authority" description="" error=1 native=0 stage=2 reason="MPEG-TS datagram materializer requires id and runtime authority"
[2026-10-21 14:21:34.037] [info] [graph][runtime.lifecycle] [CLI] output_id=1 state=retired detail="MPEG-TS datagram materializer requires id and runtime authority" description="" error=1 native=0 stage=2 reason="MPEG-TS datagram materializer requires id and runtime authority"
[in @ 000001bf361b4c00] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001bf361b4c00] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 31115.644011
[in @ 000001bf3c02bac0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001bf3c02bac0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 6619.765011
[in @ 000001bf36cc41c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001bf36cc41c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 5586.024278
[CLI] register realtime video runtime nodes failed: InvalidArgument: MPEG-TS datagram materializer requires id and runtime authority

~~~

## 清理结果

失败后按精确PID清理三个源，SOURCE0/1/2_EXIT=-1（人为清理，不是自然EOF）。无VLC。清理清单：
- composition-r288-cli-error.log
- composition-r288-cli.log
- composition-r288-monitor.log
- composition-r288-source0-stdout.log
- composition-r288-source0.log
- composition-r288-source1-stdout.log
- composition-r288-source1.log
- composition-r288-source2-stdout.log
- composition-r288-source2.log

九项临时文件已删除，r288残留0，四个精确PID已核验退出。指定连续源保留，无远程测试产物。
