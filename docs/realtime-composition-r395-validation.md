# r395：AU准备租约费用共享后的四路回归

状态：FAIL（完整起播未通过）。基线50ee9c20加本轮五源码；Windows Release89221全量构建configure/build均0、797目标，脚本核验产物。两名独立静态审查均PASS，但不是实流成功。

原120秒源、四路H264/H264/H264+AAC/HEVC与HEVC720p30/8Mbps CBR、AAC192kbps输出规格不变。CLI19792在约5.7秒后exit1：Source presentation batch requires complete storage admission。四路protocol installed及decoder contract bound均出现，原AU准备租约预约/结算可推进到同一freeze门禁，未见新增预约溢出/结算错误；不能据此证明完整费用或输出正确。保留filter色彩range变化警告。没有输出画面、稳定CPU/内存趋势或A/V漂移窗口，未运行local CLI。

源0/1/2/3 PID35144/6776/17228/32160；VLC34748。失败已判定，将核对身份后清理本轮进程；清理退出不算自然结束。不产生快照、抓包或录制。

## 实际命令

~~~powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r395-source0.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r395-source0.log' -PassThru; Write-Output "SOURCE0_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE0_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r395-source1.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r395-source1.log' -PassThru; Write-Output "SOURCE1_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE1_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r395-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r395-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE2_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61050?rtcpport=61051&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r395-source3.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r395-source3.log' -PassThru; Write-Output "SOURCE3_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE3_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r395 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --inputs "--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r395-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r395-cli-error.log' -PassThru; Write-Output "CLI_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "CLI_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r395-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r395-native- --snapshot-format=png udp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"
~~~

## 关键证据

~~~text
[in @ 000001bb9a557300] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001bb9a557300] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 5484.639
[in @ 000001bb9a556340] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001bb9a556340] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 16351.225544
[in @ 000001bb9a556dc0] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001bb9a556dc0] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 2029.077956
[in @ 000001bb9a557440] Changing video frame properties on the fly is not supported by all filters.
[in @ 000001bb9a557440] filter context - w: 1280 h: 720 fmt: 117 csp: unknown range: unknown alpha: unspecified, incoming frame - w: 1280 h: 720 fmt: 117 csp: unknown range: tv alpha: unspecified pts_time: 10826.546044
[CLI] realtime video graph preflight failed: InvalidArgument: Source presentation batch requires complete storage admission

[2026-11-05 10:20:36.134] [info] [graph][planner.capability] composition_protocol_installed source=1
[2026-11-05 10:20:36.147] [info] [graph][planner.capability] composition_protocol_installed source=2
[2026-11-05 10:20:36.634] [info] [graph][planner.capability] composition_protocol_installed source=3
[2026-11-05 10:20:36.634] [info] [graph][planner.capability] composition_protocol_installed source=0
[2026-11-05 10:20:36.707] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-r395.source.0 decoder=h264
[2026-11-05 10:20:41.173] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-r395.source.1 decoder=h264
[2026-11-05 10:20:41.213] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-r395.source.2 decoder=h264
[2026-11-05 10:20:41.253] [info] [graph][planner.capability] composition_decoder_contract_bound source=composition-r395.source.3 decoder=hevc

~~~


清理核验：四个源均中止exit -1，VLC RC quit，六个本轮PID均不存在。11项CLI/源/VLC日志及stdout按本轮清单删除；指定120秒源和正式构建产物保留。无远程媒体测试或测试产物。
