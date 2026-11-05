# r393：四路异构准备失败的现场确认

状态：FAIL，根因证据已确认；本轮只有诊断增强，没有修复媒体算法。
代码基线7072718c加两个诊断cpp；Windows Release全量构建会话89074退出0，794目标及所需产物已核对。上一轮28376句柄失效、无存活构建进程且CLI产物缺失，其结果未获证明，不记成功。本轮保持原120秒连续源、四路H.264/H.264/H.264+AAC/HEVC与HEVC720p30/8Mbps CBR输出规格。

## 结果和根因

CLI32044 exit1。源3规划rate=11250/536870537，window=28065560500ns、replay_units=59、maximum_units=60；失败时admitted=60、maximum=60、prefix=0、acquiring=60，原deadline仍剩27968777600ns。其他三路rate=30/1、容量899～901。

MediaRtpVideoFrameRateObserver对相邻marker的uint32时间戳直接做减法，以单个差值覆盖cadence。实际-3000被解释为4294964296；90000/4294964296约分恰为11250/536870537。于是28秒未来窗口只增加1个AU，导致准备准入耗尽。不是共享2GiB部署存储耗尽。

按固定名称Loopback补采10秒，8602包、drop0。有效marker299个，同一SSRC0x12e755a0；相邻timestamp差值：-6000×69、-3000×79、6000×70、9000×5、12000×10、15000×65。seq持续递增而PTS前后重排，符合视频RTP允许按不同于呈现顺序发送的语义，见[RFC3550 §5.1](https://www.rfc-editor.org/rfc/rfc3550.html#section-5.1)。tshark插件额外输出“No matching RTP/H.264 payload dumped.”，已从数值行统计排除，不当作marker。

首次使用历史接口11，当前对应本地连接2，45秒捕获0包；未用此空捕获证明网络异常。重查接口后使用固定名称NPF_Loopback。补采发生于CLI失败后的同一发送进程，不能声称精确捕获了冻结帧率的那一对包；但同一发送流重排、源码公式与失败rate完全对应。

测试判定失败后清理源32864/33248/29848/4896，均exit -1；VLC20076经RC quit。CLI未进入输出，没有画面PASS或完整120秒验收；未取得稳定CPU/内存/A/V窗口。没有调低源参数、扩容、关闭B帧或改外部依赖。

## 实际媒体命令

~~~powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r393-source0.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r393-source0.log' -PassThru; Write-Output "SOURCE0_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE0_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r393-source1.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r393-source1.log' -PassThru; Write-Output "SOURCE1_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE1_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r393-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r393-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE2_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61050?rtcpport=61051&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r393-source3.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r393-source3.log' -PassThru; Write-Output "SOURCE3_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE3_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r393 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --inputs "--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r393-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r393-cli-error.log' -PassThru; Write-Output "CLI_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "CLI_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r393-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r393-native- --snapshot-format=png udp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"
~~~

## 抓包与分析命令

~~~powershell
& 'D:/Wireshark/dumpcap.exe' -i 11 -f 'udp port 61050' -a duration:45 -w 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r393-hevc.pcapng'
& 'D:/Wireshark/dumpcap.exe' -D
& 'D:/Wireshark/dumpcap.exe' -i '\Device\NPF_Loopback' -f 'udp port 61050' -a duration:10 -w 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r393-hevc-loopback.pcapng'
& 'D:/Wireshark/tshark.exe' -r 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r393-hevc-loopback.pcapng' -d udp.port==61050,rtp -Y 'rtp.marker == 1' -T fields -e frame.number -e rtp.seq -e rtp.timestamp -e rtp.ssrc
~~~

## 修复方向与未完成项

不能用abs(delta)、忽略回退或经验最大fps替代权威时序；前向跳跃同样可能跨越多个显示帧。已只读核对外部FFmpeg hevc/parser.c：解析VPS/VUI timing后形成AVCodecContext.framerate；H264 parser同样读取SPS timing。下一步调查如何在现有有界准备链复用此事实及其有效边界，缺乏固定节奏证明时保留SourcePresentation语义，不能把名义时基冒充严格容量上界。还须核对编码重排下lineage映射，特别是RKMPP现有SubmissionOrder路径。共享修改必须Windows原规格先复验，再RKMPP，仍不扩外部依赖修复范围。

## 关键诊断


~~~text
[CLI] realtime video graph preflight failed: InvalidArgument: Prepared RTP AU prefix admission exhausted: admitted=60 maximum=60 prefix=0 acquiring=60 canonical_retention=1 remaining_ns=27968777600
[CLI] composition_inputs=4 grid=fixed audio_source=2 audio=on max_duration=source_driven hw=planner-highest-score
[2026-11-05 09:29:22.321] [info] [graph][planner.capability] composition_prepared_video_admission media_id=composition-r393.source.1 rate=30/1 window_ns=28945438800 replay_units=30 maximum_units=899
[2026-11-05 09:29:22.321] [info] [graph][planner.capability] composition_prepared_video_admission media_id=composition-r393.source.0 rate=30/1 window_ns=28945347400 replay_units=31 maximum_units=900
[2026-11-05 09:29:22.322] [info] [graph][planner.capability] composition_prepared_video_admission media_id=composition-r393.source.2 rate=30/1 window_ns=28944628100 replay_units=32 maximum_units=901
[2026-11-05 09:29:22.325] [info] [graph][planner.capability] composition_protocol_installed source=1
[2026-11-05 09:29:22.326] [info] [graph][planner.capability] composition_protocol_installed source=0
[2026-11-05 09:29:22.327] [info] [graph][planner.capability] composition_protocol_installed source=2
[2026-11-05 09:29:23.201] [info] [graph][planner.capability] composition_prepared_video_admission media_id=composition-r393.source.3 rate=11250/536870537 window_ns=28065560500 replay_units=59 maximum_units=60
[2026-11-05 09:29:23.210] [info] [graph][planner.capability] composition_protocol_installed source=3
[2026-11-05 09:29:23.354] [info] [graph][planner.capability] composition_preparation_input_terminal source=3 error=InvalidArgument: Prepared RTP AU prefix admission exhausted: admitted=60 maximum=60 prefix=0 acquiring=60 canonical_retention=1 remaining_ns=27968777600
[2026-11-05 09:29:23.389] [info] [graph][planner.capability] composition_output_candidate_rejected encoder=hevc_nvenc error=InvalidArgument: Prepared RTP AU prefix admission exhausted: admitted=60 maximum=60 prefix=0 acquiring=60 canonical_retention=1 remaining_ns=27968777600
[2026-11-05 09:29:23.389] [info] [graph][planner.capability] composition_preparation_input_terminal source=3 error=InvalidArgument: Prepared RTP AU prefix admission exhausted: admitted=60 maximum=60 prefix=0 acquiring=60 canonical_retention=1 remaining_ns=27968777600
[2026-11-05 09:29:23.390] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=720 completion_waits=720 completed_datagrams=1017 drained_datagrams=298 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-05 09:29:23.391] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=749 completion_waits=749 completed_datagrams=1027 drained_datagrams=279 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-05 09:29:23.392] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=47 completion_waits=47 completed_datagrams=48 drained_datagrams=2 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-05 09:29:23.392] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=734 completion_waits=734 completed_datagrams=978 drained_datagrams=245 would_block=0 cancellations=1 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
[2026-11-05 09:29:23.393] [info] [graph][runtime.node] rtp_ingress_adapter stage=aborted adapter=windows_overlapped_completion_queue socket_capacity_bytes=5000000 batch_capacity_bytes=4978532 descriptor_capacity=76 maximum_datagram_bytes=65507 receive_calls=15 completion_waits=15 completed_datagrams=74 drained_datagrams=59 would_block=0 cancellations=0 truncations=0 pressure_failures=0 storage_reuse=preallocated post_start_allocation_path=none
14	25356	4282238006	0x12e755a0
24	25366	4282244006	0x12e755a0
55	25397	4282259006	0x12e755a0
69	25411	4282253006	0x12e755a0
79	25421	4282250006	0x12e755a0
88	25430	4282256006	0x12e755a0
137	25479	4282271006	0x12e755a0
153	25495	4282265006	0x12e755a0
160	25502	4282262006	0x12e755a0
170	25512	4282268006	0x12e755a0
225	25567	4282283006	0x12e755a0
249	25591	4282277006	0x12e755a0
259	25601	4282274006	0x12e755a0
270	25612	4282280006	0x12e755a0
336	25678	4282295006	0x12e755a0
363	25705	4282289006	0x12e755a0
372	25714	4282286006	0x12e755a0
384	25726	4282292006	0x12e755a0
425	25767	4282307006	0x12e755a0
438	25780	4282301006	0x12e755a0
~~~

清理复核：本轮14项CLI/源/VLC日志、stdout、两份pcapng及marker TSV已按清单逐项删除，r393文件无残留；六个原媒体进程均已退出；复核时PID32864已被系统dllhost复用，保留该无关进程，其他本轮PID不存在。两次dumpcap会话均正常exit0。指定源和正式构建产物保留。
