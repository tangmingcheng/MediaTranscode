# r399：Windows默认解码回调地址修正复验

状态：待执行。延续r398原四路120秒源与全部参数；Windows81419全量构建通过后运行。用于核对默认callback/export/import实际地址以及具体solver后续门禁，不提前记为修复或合屏通过。

## 执行命令

以下命令待执行，结果和精确PID结束后补录。

~~~powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r399-source0.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r399-source0.log' -PassThru; Write-Output "SOURCE0_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE0_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r399-source1.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r399-source1.log' -PassThru; Write-Output "SOURCE1_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE1_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r399-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r399-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE2_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61050?rtcpport=61051&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r399-source3.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r399-source3.log' -PassThru; Write-Output "SOURCE3_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE3_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r399 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --inputs "--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r399-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r399-cli-error.log' -PassThru; Write-Output "CLI_PID=$($p.Id)"; while(-not $p.WaitForExit(500)){ $p.Refresh(); Write-Output "CLI_SAMPLE utc=$([DateTime]::UtcNow.ToString('o')) cpu_ms=$($p.TotalProcessorTime.TotalMilliseconds) working_set=$($p.WorkingSet64) private=$($p.PrivateMemorySize64)" }; $p.WaitForExit(); Write-Output "CLI_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r399-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r399-native- --snapshot-format=png udp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"
~~~

## 结果：FAIL，已越过opaque契约门禁

Windows81419全量终态0，四产物核验，realtime CLI7400960字节、12:35:55。CLI PID35736自然退出1，约5.6秒，当前错误为 `InvalidArgument: Composition admission requires matching prepared sources and storage owners`，发生在实际solver入口。四路decoder contract bound成功；H264/HEVC实际callback=140705961036320、exported=140705961036320、imported=140702249193768，直接证明默认回调与DLL export相同而与EXE thunk不同。r398的地址误判已在原规格链路消除，合屏整体仍失败。

源PID36724/34192/31936/2512，VLC PID16560；无输出，未拍快照，没有持续A/V漂移窗口。CPU累计93.75→703.125ms/4.6157186秒，单核等效约13.20%；working set23.3→188.5MB、private440.6→829.7MB，仅初始化窗口。五路ingress有数据且pressure_failures=0；原filter unknown→tv警告仍保留。

~~~text
CLI_PID=35736
CLI_SAMPLE utc=2026-11-09T04:36:20.0741856Z cpu_ms=93.75 working_set=23322624 private=440614912
CLI_SAMPLE utc=2026-11-09T04:36:20.5937902Z cpu_ms=93.75 working_set=28471296 private=448815104
CLI_SAMPLE utc=2026-11-09T04:36:21.0985191Z cpu_ms=140.625 working_set=30502912 private=450387968
CLI_SAMPLE utc=2026-11-09T04:36:21.6020474Z cpu_ms=296.875 working_set=147787776 private=657379328
CLI_SAMPLE utc=2026-11-09T04:36:22.1176276Z cpu_ms=296.875 working_set=150622208 private=659623936
CLI_SAMPLE utc=2026-11-09T04:36:22.6388116Z cpu_ms=296.875 working_set=153161728 private=661520384
CLI_SAMPLE utc=2026-11-09T04:36:23.1565483Z cpu_ms=312.5 working_set=156033024 private=664010752
CLI_SAMPLE utc=2026-11-09T04:36:23.6727119Z cpu_ms=437.5 working_set=160018432 private=667566080
CLI_SAMPLE utc=2026-11-09T04:36:24.1728802Z cpu_ms=437.5 working_set=162365440 private=669945856
CLI_SAMPLE utc=2026-11-09T04:36:24.6899042Z cpu_ms=703.125 working_set=188465152 private=829714432
CLI_EXIT=1

~~~

本轮已判失败，记录后按精确PID与可执行路径核验清理四源/VLC，清理退出不算自然结束；随后逐项删除11个日志/stdout，无快照/抓包/临时媒体，复用源和构建产物保留。

清理终态：四源句柄均exit -1，六PID无残留，11个stdout/日志全部逐项删除；无快照或抓包。根因代码核验：freeze的batch.owner为session m_ownerResource，snapshot.owner为m_video->resource，create分别创建resource但绑定同一m_budget。solver直接比较两个resource指针错误；修复应查询resource实际budget身份，而非取消归属校验。该共享代码影响Windows和RKMPP。
