# r405：AAC 协议数量几何复验

状态：已执行，预算准入失败。AAC五文件双独立源码PASS；Windows11355全量构建退出0。原四路及2GiB参数不变；合法完成数增加16-bit头区长度上界，解析头数仍独立覆盖畸形报文，canonical旧header覆盖与migrated保持原值。未启用信用/背压，不预记通过。

## 执行命令

~~~powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r405-source0.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r405-source0.log' -PassThru; Write-Output "SOURCE0_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE0_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r405-source1.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r405-source1.log' -PassThru; Write-Output "SOURCE1_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE1_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r405-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r405-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE2_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61050?rtcpport=61051&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r405-source3.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r405-source3.log' -PassThru; Write-Output "SOURCE3_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE3_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r405 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --inputs "--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r405-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r405-cli-error.log' -PassThru; Write-Output "CLI_PID=$($p.Id)"; while(-not $p.WaitForExit(500)){ $p.Refresh(); Write-Output "CLI_SAMPLE utc=$([DateTime]::UtcNow.ToString('o')) cpu_ms=$($p.TotalProcessorTime.TotalMilliseconds) working_set=$($p.WorkingSet64) private=$($p.PrivateMemorySize64)" }; $p.WaitForExit(); Write-Output "CLI_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r405-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r405-native- --snapshot-format=png udp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"
~~~

## 结果：FAIL，AAC数量修正生效但整体预算仍不足

Windows11355全量构建退出0。CLI PID41664自然退出1，未起播。AAC payload费用为2,679,240,332字节、319604个reservation（单个8383字节），相较r404约14.28GB不再使用超出协议头区长度的数量上界。各源实际held随准备耗时变化，本轮视频费用更高；总源费用26,911,372,342字节，可用1,661,734,578字节，仍被k0预算门禁拒绝。不能将数量修正记成合屏通过。

源PID28868/19496/42236/41884，VLC PID42120。CPU累计15.625→890.625ms/16.477秒，约5.31%单核；working set18.4→178.4MB、private427.3→842.8MB，均初始化窗口。五路ingress pressure_failures=0，原filter range警告保留；无稳态内存、持续A/V漂移或画面证据。

~~~text
[in @ 0000027bad135440] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 40823.516933
[in @ 0000027bad16c3c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000027bad16c3c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 19882.345444
[CLI] realtime video graph preflight failed: InvalidArgument: Composition minimum legal capacity exceeds deployment storage: k=0 capacity=2147483648 root_other=485741238 batch=7832 available=1661734578 sources=26911372342 source[0]={total=6566221634 fixed=61770746 protocol=6502930025 startup=1520863 video_payload=6501392656} source[1]={total=6408663018 fixed=61770746 protocol=6345394593 startup=1497679 video_payload=6343889200} source[2]={total=5995299361 fixed=71946424 protocol=5903409362 startup=19943575 video_payload=2283800112 audio_payload=2679240332 audio_reservations=319604 audio_reservation_bytes=8383} source[3]={total=7941188329 fixed=74270857 protocol=7865355073 startup=1562399 video_payload=7863884208}
[2026-11-09 14:47:22.835] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-11-09 14:47:22.907] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=7631 completion_waits=7631 completed_datagrams=14820 drained_datagrams=7188 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-09 14:47:22.911] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=7136 completion_waits=7136 completed_datagrams=14631 drained_datagrams=7496 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-09 14:47:22.915] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=19 completion_waits=19 completed_datagrams=23 drained_datagrams=5 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-09 14:47:22.916] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=133 completion_waits=133 completed_datagrams=606 drained_datagrams=474 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-09 14:47:22.917] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=6820 completion_waits=6820 completed_datagrams=13966 drained_datagrams=7146 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none

CLI_PID=41664
CLI_SAMPLE utc=2026-11-09T06:47:06.4198219Z cpu_ms=15.625 working_set=18448384 private=427307008
CLI_SAMPLE utc=2026-11-09T06:47:06.9361150Z cpu_ms=15.625 working_set=19013632 private=427618304
CLI_SAMPLE utc=2026-11-09T06:47:07.4543375Z cpu_ms=31.25 working_set=23769088 private=439566336
CLI_SAMPLE utc=2026-11-09T06:47:07.9621677Z cpu_ms=46.875 working_set=26607616 private=442208256
CLI_SAMPLE utc=2026-11-09T06:47:08.4733604Z cpu_ms=46.875 working_set=28880896 private=444628992
CLI_SAMPLE utc=2026-11-09T06:47:08.9948159Z cpu_ms=62.5 working_set=31301632 private=446234624
CLI_SAMPLE utc=2026-11-09T06:47:09.5060709Z cpu_ms=78.125 working_set=32698368 private=447590400
CLI_SAMPLE utc=2026-11-09T06:47:10.0137488Z cpu_ms=78.125 working_set=34631680 private=449400832
CLI_SAMPLE utc=2026-11-09T06:47:10.5270121Z cpu_ms=93.75 working_set=36339712 private=451014656
CLI_SAMPLE utc=2026-11-09T06:47:11.0356864Z cpu_ms=140.625 working_set=37687296 private=452227072
CLI_SAMPLE utc=2026-11-09T06:47:11.5483460Z cpu_ms=171.875 working_set=40177664 private=454746112
CLI_SAMPLE utc=2026-11-09T06:47:12.0827956Z cpu_ms=171.875 working_set=42008576 private=456564736
CLI_SAMPLE utc=2026-11-09T06:47:12.6009086Z cpu_ms=171.875 working_set=43958272 private=458424320
CLI_SAMPLE utc=2026-11-09T06:47:13.1205019Z cpu_ms=203.125 working_set=46153728 private=460623872
CLI_SAMPLE utc=2026-11-09T06:47:13.6318002Z cpu_ms=203.125 working_set=47751168 private=462127104
CLI_SAMPLE utc=2026-11-09T06:47:14.1415910Z cpu_ms=203.125 working_set=49410048 private=463654912
CLI_SAMPLE utc=2026-11-09T06:47:14.6604774Z cpu_ms=203.125 working_set=51113984 private=465170432
CLI_SAMPLE utc=2026-11-09T06:47:15.1782947Z cpu_ms=218.75 working_set=52776960 private=466571264
CLI_SAMPLE utc=2026-11-09T06:47:15.6953267Z cpu_ms=218.75 working_set=55255040 private=469147648
CLI_SAMPLE utc=2026-11-09T06:47:16.2091838Z cpu_ms=234.375 working_set=56934400 private=470773760
CLI_SAMPLE utc=2026-11-09T06:47:16.7184807Z cpu_ms=296.875 working_set=58814464 private=472576000
CLI_SAMPLE utc=2026-11-09T06:47:17.2349658Z cpu_ms=312.5 working_set=61079552 private=474632192
CLI_SAMPLE utc=2026-11-09T06:47:17.7497128Z cpu_ms=328.125 working_set=63012864 private=476381184
CLI_SAMPLE utc=2026-11-09T06:47:18.2637324Z cpu_ms=359.375 working_set=64528384 private=477827072
CLI_SAMPLE utc=2026-11-09T06:47:18.7779772Z cpu_ms=375 working_set=66146304 private=479354880
CLI_SAMPLE utc=2026-11-09T06:47:19.2965726Z cpu_ms=406.25 working_set=67596288 private=480743424
CLI_SAMPLE utc=2026-11-09T06:47:19.8066736Z cpu_ms=453.125 working_set=69738496 private=482701312
CLI_SAMPLE utc=2026-11-09T06:47:20.3165337Z cpu_ms=468.75 working_set=71872512 private=484913152
CLI_SAMPLE utc=2026-11-09T06:47:20.8326461Z cpu_ms=531.25 working_set=73732096 private=486744064
CLI_SAMPLE utc=2026-11-09T06:47:21.3464390Z cpu_ms=546.875 working_set=75632640 private=488660992
CLI_SAMPLE utc=2026-11-09T06:47:21.8595066Z cpu_ms=562.5 working_set=77701120 private=490582016
CLI_SAMPLE utc=2026-11-09T06:47:22.3770116Z cpu_ms=578.125 working_set=78893056 private=491724800
CLI_SAMPLE utc=2026-11-09T06:47:22.8971466Z cpu_ms=890.625 working_set=178380800 private=842756096
CLI_EXIT=1

~~~

记录后按精确PID清理四源/VLC及本轮11项日志/stdout；未生成快照、抓包或录制，保留指定源。

清理复核：四个源进程工具句柄均已终止，退出码 -1（主动清理，不是自然结束）；CLI 41664、源 28868/19496/42236/41884、VLC 42120 均无残留，本轮文件前缀查询为空。

RKMPP 构建补记：上一批 16d1cffa 的 PID 302747 已不存在，8 核增量 all-target 完成 476/476，realtime CLI 链接成功，退出标记为 0。本次 AAC 五文件尚未同步；构建不代表媒体验收。

AAC 同步：五文件写前/写后 hash 均通过；专属 zip SHA256 为 98972bccd4f9ee6c2a821e7a4626857c3a073d1cdbf5ee90fe41024721e103ef，本机及远端同步包已删除。上一批 RKMPP 两个构建临时文件也已删除，残留 0。初次同步因远端 Python 不支持 Path.is_relative_to 在写入前终止，改用 parents 路径核验后完成。

AAC 五源码再次由两名独立审查者明确 SOURCE PASS，未更改其冻结版本；RKMPP 新增量构建 PID 448136，专属日志/退出标记 composition-build-aac-r405，终态待核验。

RKMPP AAC 终态：PID 448136 已不存在，增量 all-target 11/11、退出标记 0，realtime CLI 成功链接；未执行 local 媒体测试，也未将构建视为 RKMPP 合屏验收。两个专属构建临时文件在记录后逐项清理。
