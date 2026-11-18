# r400：合屏准入预算身份复验

状态：待执行。使用与r399完全相同的四路120秒连续源和输出参数；核对同预算不同PMR资源的归属修正、具体容量求解和后续门禁。等待Windows52637全量构建通过后执行，不预记通过。

## 执行命令

以下命令待执行，结果和精确PID结束后补录。

~~~powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r400-source0.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r400-source0.log' -PassThru; Write-Output "SOURCE0_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE0_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r400-source1.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r400-source1.log' -PassThru; Write-Output "SOURCE1_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE1_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r400-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r400-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE2_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61050?rtcpport=61051&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r400-source3.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r400-source3.log' -PassThru; Write-Output "SOURCE3_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE3_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r400 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --inputs "--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r400-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r400-cli-error.log' -PassThru; Write-Output "CLI_PID=$($p.Id)"; while(-not $p.WaitForExit(500)){ $p.Refresh(); Write-Output "CLI_SAMPLE utc=$([DateTime]::UtcNow.ToString('o')) cpu_ms=$($p.TotalProcessorTime.TotalMilliseconds) working_set=$($p.WorkingSet64) private=$($p.PrivateMemorySize64)" }; $p.WaitForExit(); Write-Output "CLI_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r400-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r400-native- --snapshot-format=png udp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"
~~~

## 结果：FAIL，进入A/V规划后拒绝

Windows52637全量终态0，四产物已核验，realtime CLI7401472字节、12:42:15。CLI PID38448自然退出1，约5.5秒；四路decoder contract与opaque export校验通过，预算资源入口不再报错，当前错误：`InvalidArgument: Incomplete or inconsistent A/V synchronization plan: incomplete policy contains finalized correction timing`。尚未起播，不计合屏通过。需核查固定策略与候选完成态的共享规划调用边界，不取消校验。

源PID34120/28572/34488/18516，VLC PID556；CLI CPU累计62.5→406.25ms/4.1274762秒，单核等效约8.33%，working set23.2→162.9MB、private439.9→671.0MB。只有初始化窗口，无持续A/V漂移或稳定内存趋势。五路ingress接收且pressure_failures=0，原filter色彩range警告保留。无输出，未请求快照。

~~~text
CLI_PID=38448
CLI_SAMPLE utc=2026-11-09T04:42:58.5787828Z cpu_ms=62.5 working_set=23150592 private=439865344
CLI_SAMPLE utc=2026-11-09T04:42:59.1036925Z cpu_ms=140.625 working_set=143355904 private=653635584
CLI_SAMPLE utc=2026-11-09T04:42:59.6167717Z cpu_ms=140.625 working_set=146927616 private=656801792
CLI_SAMPLE utc=2026-11-09T04:43:00.1285949Z cpu_ms=250 working_set=149135360 private=658665472
CLI_SAMPLE utc=2026-11-09T04:43:00.6426969Z cpu_ms=375 working_set=152182784 private=661159936
CLI_SAMPLE utc=2026-11-09T04:43:01.1559388Z cpu_ms=375 working_set=154857472 private=663445504
CLI_SAMPLE utc=2026-11-09T04:43:01.6686240Z cpu_ms=375 working_set=157790208 private=665964544
CLI_SAMPLE utc=2026-11-09T04:43:02.1807574Z cpu_ms=375 working_set=159883264 private=667971584
CLI_SAMPLE utc=2026-11-09T04:43:02.7062590Z cpu_ms=406.25 working_set=162897920 private=670990336
CLI_EXIT=1

~~~

本轮已判失败，记录后核验精确PID身份清理四源及VLC；清理退出不计自然结束。随后按11项日志/stdout清单删除，无快照、抓包或录制，保留复用源与构建产物。

清理终态：四源句柄exit -1，六PID和本轮文件残留均0。只读根因：bindFacts在调用AvSyncPlanner::bind之前覆盖fixed.audioServo为已完成结果，触发validatePolicy；rebind同样复用完成态，原始immutable servo policy未独立保留。两平台共享该代码。
