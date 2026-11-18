# r403：预算快照迁移后的原规格回归

状态：待执行。Windows76595全量构建退出0，六文件封存机制双独立源码局部PASS。当前尚无生产seal调用，本轮仅验证未封存共享路径没有被错误放行，不证明封存/释放通知实流覆盖。仍保持2GiB与原四路规格。

## 执行命令

~~~powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r403-source0.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r403-source0.log' -PassThru; Write-Output "SOURCE0_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE0_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r403-source1.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r403-source1.log' -PassThru; Write-Output "SOURCE1_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE1_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r403-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r403-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE2_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61050?rtcpport=61051&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r403-source3.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r403-source3.log' -PassThru; Write-Output "SOURCE3_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE3_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r403 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --inputs "--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r403-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r403-cli-error.log' -PassThru; Write-Output "CLI_PID=$($p.Id)"; while(-not $p.WaitForExit(500)){ $p.Refresh(); Write-Output "CLI_SAMPLE utc=$([DateTime]::UtcNow.ToString('o')) cpu_ms=$($p.TotalProcessorTime.TotalMilliseconds) working_set=$($p.WorkingSet64) private=$($p.PrivateMemorySize64)" }; $p.WaitForExit(); Write-Output "CLI_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r403-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r403-native- --snapshot-format=png udp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"
~~~

## 结果：FAIL，原预算门禁仍拒绝

CLI PID4092自然退出1，k0源费用33,184,653,184字节，可用1,662,078,786字节，未错误放行。batch从6096变6352字节反映快照结构真实sizeof增长；本轮实际held随源到达变化，不应期待与r402费用逐字相等。无输出、无持续A/V漂移或画面证据，不计合屏通过；未覆盖sealPartition执行路径。

源PID40864/18188/3908/39236，VLC PID25828。CPU累计93.75→218.75ms/9.800秒，约1.28%单核；working set14.2→86.6MB、private421.3→538.7MB，仅初始化窗口。

~~~text
[in @ 00000177e806a380] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 17604.763211
[in @ 00000177d9644a00] Changing video frame properties on the fly is not supported by all filters.
[in @ 00000177d9644a00] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 2681.672144
[CLI] realtime video graph preflight failed: InvalidArgument: Composition minimum legal capacity exceeds deployment storage: k=0 capacity=2147483648 root_other=485398510 batch=6352 available=1662078786 sources=33184653184 source[0]={total=2364861610 fixed=61770474 protocol=2302188513 startup=902623 video_payload=2301300496} source[1]={total=3327672786 fixed=61770474 protocol=3264858009 startup=1044303 video_payload=3263821616} source[2]={total=23253105939 fixed=71946136 protocol=23111104620 startup=70055183 video_payload=3928836208 audio_payload=14278613006 audio_reservations=1703282 audio_reservation_bytes=8383} source[3]={total=4239012849 fixed=74270585 protocol=4163603617 startup=1138647 video_payload=4162571040}
[2026-11-09 14:08:28.666] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-11-09 14:08:28.755] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=191 completion_waits=191 completed_datagrams=883 drained_datagrams=693 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-09 14:08:28.756] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=1882 completion_waits=1882 completed_datagrams=4232 drained_datagrams=2351 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-09 14:08:28.758] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=286 completion_waits=286 completed_datagrams=289 drained_datagrams=4 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-09 14:08:28.759] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=2677 completion_waits=2677 completed_datagrams=6367 drained_datagrams=3691 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-09 14:08:28.762] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=1836 completion_waits=1836 completed_datagrams=4302 drained_datagrams=2467 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none

CLI_PID=4092
CLI_SAMPLE utc=2026-11-09T06:08:18.5116185Z cpu_ms=93.75 working_set=14200832 private=421269504
CLI_SAMPLE utc=2026-11-09T06:08:19.0408389Z cpu_ms=93.75 working_set=14200832 private=421269504
CLI_SAMPLE utc=2026-11-09T06:08:19.5537299Z cpu_ms=93.75 working_set=14200832 private=421269504
CLI_SAMPLE utc=2026-11-09T06:08:20.0658401Z cpu_ms=93.75 working_set=14217216 private=421269504
CLI_SAMPLE utc=2026-11-09T06:08:20.5735121Z cpu_ms=93.75 working_set=14217216 private=421269504
CLI_SAMPLE utc=2026-11-09T06:08:21.0896572Z cpu_ms=93.75 working_set=14217216 private=421269504
CLI_SAMPLE utc=2026-11-09T06:08:21.6041238Z cpu_ms=93.75 working_set=14217216 private=421269504
CLI_SAMPLE utc=2026-11-09T06:08:22.1136987Z cpu_ms=93.75 working_set=19255296 private=432361472
CLI_SAMPLE utc=2026-11-09T06:08:22.6306323Z cpu_ms=93.75 working_set=22065152 private=434245632
CLI_SAMPLE utc=2026-11-09T06:08:23.1483836Z cpu_ms=93.75 working_set=22900736 private=434786304
CLI_SAMPLE utc=2026-11-09T06:08:23.6704530Z cpu_ms=93.75 working_set=23461888 private=435109888
CLI_SAMPLE utc=2026-11-09T06:08:24.1830601Z cpu_ms=93.75 working_set=23576576 private=435183616
CLI_SAMPLE utc=2026-11-09T06:08:24.7032017Z cpu_ms=125 working_set=28778496 private=447770624
CLI_SAMPLE utc=2026-11-09T06:08:25.2144192Z cpu_ms=156.25 working_set=32718848 private=450650112
CLI_SAMPLE utc=2026-11-09T06:08:25.7353476Z cpu_ms=156.25 working_set=35074048 private=452616192
CLI_SAMPLE utc=2026-11-09T06:08:26.2460954Z cpu_ms=156.25 working_set=36790272 private=454311936
CLI_SAMPLE utc=2026-11-09T06:08:26.7508516Z cpu_ms=171.875 working_set=38305792 private=455409664
CLI_SAMPLE utc=2026-11-09T06:08:27.2716981Z cpu_ms=187.5 working_set=40505344 private=457551872
CLI_SAMPLE utc=2026-11-09T06:08:27.7873012Z cpu_ms=203.125 working_set=42242048 private=459153408
CLI_SAMPLE utc=2026-11-09T06:08:28.3114364Z cpu_ms=218.75 working_set=86568960 private=538656768
CLI_EXIT=1

~~~

记录后核验精确PID，清理四源/VLC及本轮11项日志/stdout；未生成快照、抓包或录制，保留指定连续源。

清理核验：四源句柄exit -1，六个精确PID均不存在；11项日志/stdout已逐项删除，本轮文件残留0。
