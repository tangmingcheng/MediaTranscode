# r397：固定准备前置的主机四路诊断

状态：FAIL（完整起播未通过），本轮固定准备阶段已到达其后的准入门禁。使用与r396相同构建/参数，在沙箱外主机运行。四路原120秒源，H264/H264/H264+AAC/HEVC；目标HEVC 1280×720、30fps、8000kbps CBR、GOP60，AAC192kbps/44100Hz/双声道。没有降低源或输出参数，没有local CLI测试。

CLI PID30000 约4秒自然退出1：`Source presentation batch requires complete storage admission`。四路decoder contract bound以及五路ingress adapter接收统计均出现；根据当前Preparer控制流，freeze之前的全部固定产品准备已返回成功。尚无具体solver，因此不代表输出、失源重入或完整预算验收通过。r396零包发生在默认沙箱，本轮沙箱外收到数据，支持测试环境隔离解释；未修改系统网络配置。

CPU累计采样78.125→187.5ms，working set约21.6→108.3MB，private约429.1→533.9MB；按采样端点计算，CPU增量109.375ms/3.096s≈3.53%单核等效占用；仅为约3秒初始化窗口，无稳定内存趋势/A-V漂移窗口。保留filter unknown→tv色彩range警告。VLC PID41448已监听62024，尚无媒体输出，未申请快照或系统截图。源PID37420/41480/25192/39848。诊断已结束，随后按精确PID清理，清理退出不算自然结束。

## 实际命令

~~~powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r397-source0.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r397-source0.log' -PassThru; Write-Output "SOURCE0_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE0_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r397-source1.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r397-source1.log' -PassThru; Write-Output "SOURCE1_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE1_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r397-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r397-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE2_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61050?rtcpport=61051&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r397-source3.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r397-source3.log' -PassThru; Write-Output "SOURCE3_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE3_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r397 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --inputs "--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r397-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r397-cli-error.log' -PassThru; Write-Output "CLI_PID=$($p.Id)"; while(-not $p.WaitForExit(500)){ $p.Refresh(); Write-Output "CLI_SAMPLE utc=$([DateTime]::UtcNow.ToString('o')) cpu_ms=$($p.TotalProcessorTime.TotalMilliseconds) working_set=$($p.WorkingSet64) private=$($p.PrivateMemorySize64)" }; $p.WaitForExit(); Write-Output "CLI_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r397-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r397-native- --snapshot-format=png udp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"
~~~

## 关键证据

~~~text
[in @ 000001eddfd445c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001eddfd445c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 34384.354233
[in @ 000001ede049d300] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001ede049d300] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 44836.7866
[in @ 000001ede1a67300] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001ede1a67300] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 24779.856189
[in @ 000001ede00e4440] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001ede00e4440] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 23897.295133
[CLI] realtime video graph preflight failed: InvalidArgument: Source presentation batch requires complete storage admission
[2026-11-06 16:26:42.132] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-r397.source.0 decoder=h264
[2026-11-06 16:26:42.201] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-06 16:26:42.201] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-06 16:26:42.201] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-r397.source.1 decoder=h264
[2026-11-06 16:26:42.243] [info] [graph][runtime.node] codec_resolver.decoder.open name=h264 planned=h264 hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-06 16:26:42.243] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=h264 hwaccel_flags=1
[2026-11-06 16:26:42.243] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-r397.source.2 decoder=h264
[2026-11-06 16:26:42.284] [info] [graph][runtime.node] codec_resolver.decoder.open name=hevc planned=hevc hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-06 16:26:42.284] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=hevc hwaccel_flags=1
[2026-11-06 16:26:42.284] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-r397.source.3 decoder=hevc
[2026-11-06 16:26:42.356] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-11-06 16:26:42.412] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=248 completion_waits=248 completed_datagrams=754 drained_datagrams=507 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-06 16:26:42.413] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=1725 completion_waits=1725 completed_datagrams=3395 drained_datagrams=1671 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-06 16:26:42.415] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=112 completion_waits=112 completed_datagrams=117 drained_datagrams=6 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-06 16:26:42.415] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=1355 completion_waits=1355 completed_datagrams=2947 drained_datagrams=1593 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-06 16:26:42.416] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=1373 completion_waits=1373 completed_datagrams=2764 drained_datagrams=1392 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
main debug: looking for art finder module matching "any": 2 candidates
udp debug: opening server=:0 local=:62024
main debug: net: opening any datagram port 62024
lua debug: Trying Lua scripts in C:\Users\ROG\AppData\Roaming\vlc\lua\meta\art
lua debug: Tr
~~~

## CLI采样

~~~text
CLI_PID=30000
CLI_SAMPLE utc=2026-11-06T08:26:38.9852243Z cpu_ms=78.125 working_set=21553152 private=429146112
CLI_SAMPLE utc=2026-11-06T08:26:39.5110527Z cpu_ms=78.125 working_set=23113728 private=430219264
CLI_SAMPLE utc=2026-11-06T08:26:40.0235389Z cpu_ms=78.125 working_set=28749824 private=446586880
CLI_SAMPLE utc=2026-11-06T08:26:40.5377697Z cpu_ms=78.125 working_set=31252480 private=448479232
CLI_SAMPLE utc=2026-11-06T08:26:41.0537712Z cpu_ms=78.125 working_set=34746368 private=451403776
CLI_SAMPLE utc=2026-11-06T08:26:41.5676640Z cpu_ms=78.125 working_set=36171776 private=452775936
CLI_SAMPLE utc=2026-11-06T08:26:42.0811152Z cpu_ms=187.5 working_set=108285952 private=533889024
CLI_EXIT=1
~~~

清理：核对可执行路径后终止四个源和VLC，四源句柄exit -1；CLI自然exit1。主机检查六个精确PID残留为0。记录后按11项文件清单删除本轮stdout/日志；无截图、抓包或录制，指定120秒源和正式构建产物保留。
