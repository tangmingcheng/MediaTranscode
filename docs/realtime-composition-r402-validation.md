# r402：最小合法候选预算分项诊断

状态：待执行。保持r401四路原规格与2GiB预算，仅增加失败费用明细，不改公式、搜索或准入条件。两个独立审查者对诊断修改均局部PASS。

## 执行命令

以下为拟执行命令，实际结果后补。

~~~powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r402-source0.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r402-source0.log' -PassThru; Write-Output "SOURCE0_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE0_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r402-source1.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r402-source1.log' -PassThru; Write-Output "SOURCE1_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE1_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r402-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r402-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE2_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61050?rtcpport=61051&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r402-source3.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r402-source3.log' -PassThru; Write-Output "SOURCE3_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE3_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r402 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --inputs "--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r402-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r402-cli-error.log' -PassThru; Write-Output "CLI_PID=$($p.Id)"; while(-not $p.WaitForExit(500)){ $p.Refresh(); Write-Output "CLI_SAMPLE utc=$([DateTime]::UtcNow.ToString('o')) cpu_ms=$($p.TotalProcessorTime.TotalMilliseconds) working_set=$($p.WorkingSet64) private=$($p.PrivateMemorySize64)" }; $p.WaitForExit(); Write-Output "CLI_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r402-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r402-native- --snapshot-format=png udp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"
~~~

## 结果：FAIL，费用放大已由真实链路确认

Windows63803全量构建退出0。CLI PID37836自然退出1，未起播。源PID33392/40584/19800/25164，VLC PID19424。CPU累计62.5→281.25ms/7.729秒，约2.83%单核；working set19.2→51.1MB、private432.0→468.7MB，仅初始化样本，不是稳定趋势。无持续A/V漂移或画面证据。

~~~text
[in @ 000002029b597040] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002029b597040] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 47173.864133
[in @ 000002029b597ac0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002029b597ac0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 40655.875833
[in @ 000002029b596c00] Changing video frame properties on the fly is not supported by all filters.
[in @ 000002029b596c00] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 44940.8106
[in @ 0000020290f630c0] Changing video frame properties on the fly is not supported by all filters.
[in @ 0000020290f630c0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 30826.742333
[CLI] realtime video graph preflight failed: InvalidArgument: Composition minimum legal capacity exceeds deployment storage: k=0 capacity=2147483648 root_other=485459422 batch=6096 available=1662018130 sources=34421201947 source[0]={total=4281728354 fixed=61770474 protocol=4218773185 startup=1184695 video_payload=4217592544} source[1]={total=3642773370 fixed=61770474 protocol=3579912225 startup=1090671 video_payload=3578828528} source[2]={total=23653038158 fixed=71946136 protocol=23491590511 startup=89501511 video_payload=4305094464 audio_payload=14279124369 audio_reservations=1703343 audio_reservation_bytes=8383} source[3]={total=2843662065 fixed=74270585 protocol=2768412545 startup=978935 video_payload=2767547232}
[2026-11-09 13:47:57.998] [info] [graph][runtime.node] codec_resolver.decoder.open name=hevc planned=hevc hardware=true hwaccel=cuda hw_pix_fmt=cuda hw_device_ctx=set hw_frames_contract=required pkt_tb=1/90000
[2026-11-09 13:47:57.999] [info] [graph][runtime.node] codec_resolver.decoder.runtime name=hevc hwaccel_flags=1
[2026-11-09 13:47:57.999] [info] [graph][planner.capability] decoder_opaque_callback codec=hevc callback=140705404176928 exported=140705404176928 imported=140701427175720
[2026-11-09 13:47:57.999] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-r402.source.3 decoder=hevc
[2026-11-09 13:47:58.076] [info] [graph][planner.capability] encoder_output_buffering verified=immediate delay=0 zerolatency=1 rc_lookahead=0 b_frames=0
[2026-11-09 13:47:58.147] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=4245 completion_waits=4245 completed_datagrams=7485 drained_datagrams=3241 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-09 13:47:58.150] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=2494 completion_waits=2494 completed_datagrams=5406 drained_datagrams=2913 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-09 13:47:58.151] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=364 completion_waits=364 completed_datagrams=371 drained_datagrams=8 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-09 13:47:58.151] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=3964 completion_waits=3964 completed_datagrams=7506 drained_datagrams=3542 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-09 13:47:58.155] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=75 completion_waits=75 completed_datagrams=422 drained_datagrams=348 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none

CLI_PID=37836
CLI_SAMPLE utc=2026-11-09T05:47:50.0147159Z cpu_ms=62.5 working_set=19193856 private=431955968
CLI_SAMPLE utc=2026-11-09T05:47:50.5410291Z cpu_ms=62.5 working_set=22556672 private=439013376
CLI_SAMPLE utc=2026-11-09T05:47:51.0555845Z cpu_ms=62.5 working_set=24711168 private=441298944
CLI_SAMPLE utc=2026-11-09T05:47:51.5687718Z cpu_ms=62.5 working_set=26898432 private=442404864
CLI_SAMPLE utc=2026-11-09T05:47:52.0827009Z cpu_ms=62.5 working_set=27459584 private=442871808
CLI_SAMPLE utc=2026-11-09T05:47:52.5974498Z cpu_ms=125 working_set=29069312 private=444334080
CLI_SAMPLE utc=2026-11-09T05:47:53.1119607Z cpu_ms=125 working_set=32882688 private=451661824
CLI_SAMPLE utc=2026-11-09T05:47:53.6251707Z cpu_ms=125 working_set=35909632 private=454270976
CLI_SAMPLE utc=2026-11-09T05:47:54.1353871Z cpu_ms=125 working_set=37367808 private=455704576
CLI_SAMPLE utc=2026-11-09T05:47:54.6597008Z cpu_ms=250 working_set=39297024 private=457478144
CLI_SAMPLE utc=2026-11-09T05:47:55.1696400Z cpu_ms=250 working_set=40861696 private=459030528
CLI_SAMPLE utc=2026-11-09T05:47:55.6823149Z cpu_ms=250 working_set=43196416 private=460996608
CLI_SAMPLE utc=2026-11-09T05:47:56.2054762Z cpu_ms=250 working_set=44888064 private=462692352
CLI_SAMPLE utc=2026-11-09T05:47:56.7135063Z cpu_ms=250 working_set=47575040 private=465379328
CLI_SAMPLE utc=2026-11-09T05:47:57.2355390Z cpu_ms=281.25 working_set=49586176 private=467173376
CLI_SAMPLE utc=2026-11-09T05:47:57.7440690Z cpu_ms=281.25 working_set=51126272 private=468652032
CLI_EXIT=1

~~~

k=0已合法，但候选源费用34,421,201,947字节超过可用1,662,018,130字节；AAC payload14,279,124,369字节，四源视频payload也各约2.77–4.31GB。不能仅修AAC或提高部署预算。输出可跨批累积，后续需联合对象/字节信用及原子推进契约，不能以单批字节界替代总积压。测试已结束，记录后按精确PID清理源/VLC，删除本轮11项日志；未产生快照、抓包、录制。

清理终态：四源句柄均exit -1，六个精确PID均不存在；本轮11项文件已逐项删除，文件残留0。复用输入源保留。
