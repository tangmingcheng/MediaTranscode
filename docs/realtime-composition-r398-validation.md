# r398：具体合屏准入求解器首次实流验证

状态：待执行。Windows Release84868全量构建已退出0，双独立源码审查局部PASS；运行原四路120秒源，H264/H264/H264+AAC/HEVC输入，目标HEVC 1280×720、30fps、8000kbps CBR、GOP60，AAC192kbps/44100Hz/双声道。2GiB准备预算，不降低参数，不运行local CLI。

## 执行命令

以下命令待执行，结果和精确PID结束后补录。

~~~powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r398-source0.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r398-source0.log' -PassThru; Write-Output "SOURCE0_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE0_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r398-source1.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r398-source1.log' -PassThru; Write-Output "SOURCE1_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE1_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r398-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r398-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE2_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v hevc_nvenc -rc cbr -b:v 8000k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61050?rtcpport=61051&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r398-source3.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r398-source3.log' -PassThru; Write-Output "SOURCE3_PID=$($p.Id)"; $p.WaitForExit(); Write-Output "SOURCE3_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r398 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --inputs "--input-type rtp --input-latency-ms 250 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61050 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport udp --output udp://127.0.0.1:62024 --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r398-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r398-cli-error.log' -PassThru; Write-Output "CLI_PID=$($p.Id)"; while(-not $p.WaitForExit(500)){ $p.Refresh(); Write-Output "CLI_SAMPLE utc=$([DateTime]::UtcNow.ToString('o')) cpu_ms=$($p.TotalProcessorTime.TotalMilliseconds) working_set=$($p.WorkingSet64) private=$($p.PrivateMemorySize64)" }; $p.WaitForExit(); Write-Output "CLI_EXIT=$($p.ExitCode)"; exit $p.ExitCode
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r398-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r398-native- --snapshot-format=png udp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"
~~~

## 结果：FAIL

CLI PID28484自然退出1，约7.7秒；错误为 `Unsupported: Composition decoder has no authoritative opaque retention contract`。CUDA h264已成功打开但opaque契约被拒；QSV候选随后因缺权威packet retention被拒。失败在固定decoder准备阶段，新solver尚未执行。四路源PID24612/14772/39008/3656，VLC PID37676。五路ingress均有接收且pressure_failures=0；未形成输出，无A/V漂移或稳定内存趋势窗口，未请求快照。

CPU累计15.625→968.75ms，采样窗口6.6803461秒，单核等效约14.27%；working set 5.3→166.5MB、private 15.9→661.4MB，仅初始化阶段，不代表稳定趋势。原120秒源SHA256仍为6E0DE760672A6B8386B59B22F824DE442E2A5EF7594D7B872C6851B64CABCA9B。待核对实际动态库与opaque adapter审计条件，不放宽门禁。

~~~text
CLI_PID=28484
CLI_SAMPLE utc=2026-11-09T04:27:00.6183445Z cpu_ms=15.625 working_set=5324800 private=15941632
CLI_SAMPLE utc=2026-11-09T04:27:01.1350221Z cpu_ms=31.25 working_set=5468160 private=17973248
CLI_SAMPLE utc=2026-11-09T04:27:01.6412665Z cpu_ms=31.25 working_set=5468160 private=17973248
CLI_SAMPLE utc=2026-11-09T04:27:02.1531220Z cpu_ms=46.875 working_set=16814080 private=423145472
CLI_SAMPLE utc=2026-11-09T04:27:02.6603002Z cpu_ms=93.75 working_set=22511616 private=435228672
CLI_SAMPLE utc=2026-11-09T04:27:03.1796021Z cpu_ms=93.75 working_set=25911296 private=442494976
CLI_SAMPLE utc=2026-11-09T04:27:03.6995475Z cpu_ms=140.625 working_set=27136000 private=443408384
CLI_SAMPLE utc=2026-11-09T04:27:04.2221661Z cpu_ms=187.5 working_set=31498240 private=451366912
CLI_SAMPLE utc=2026-11-09T04:27:04.7337146Z cpu_ms=187.5 working_set=33681408 private=452542464
CLI_SAMPLE utc=2026-11-09T04:27:05.2493184Z cpu_ms=203.125 working_set=36540416 private=454774784
CLI_SAMPLE utc=2026-11-09T04:27:05.7593968Z cpu_ms=203.125 working_set=37785600 private=455921664
CLI_SAMPLE utc=2026-11-09T04:27:06.2724648Z cpu_ms=250 working_set=40407040 private=458534912
CLI_SAMPLE utc=2026-11-09T04:27:06.7850372Z cpu_ms=593.75 working_set=88408064 private=594776064
CLI_SAMPLE utc=2026-11-09T04:27:07.2986906Z cpu_ms=968.75 working_set=166473728 private=661430272
CLI_EXIT=1

~~~

本轮已判定失败，记录后核验精确PID身份并清理四源和VLC；清理退出不计自然结束。随后按11项stdout/日志清单删除，无抓包/快照/录制；复用测试源和正式构建产物保留。

清理终态：四源句柄均exit -1（主动清理），VLC精确PID已清理；六个PID无残留，11个本轮日志/stdout逐项删除，目录无composition-r398产物。命令末尾Get-Process因无匹配返回1，不是删除失败。动态库只读核验：项目与Release目录avcodec SHA256均为F720E06FB04379BF8187094B15FD84CA24B25C8567E2842B5457C5D31CFCE621；直接读取Release目录avcodec_version=4072549，versionstring与两库2368字符配置全部与adapter审计值相同。

补充诊断：MSVC obj反汇编显示LEA avcodec_default_get_buffer2后与context+0x198比较，引用普通REL32导入符号；直接DLL原生H264诊断open=0、thread_count=1，callback与真实export均0x7ff8dd644a20。符合Microsoft导入thunk地址与DLL真实函数地址不同的机制，但不是原CLI三个地址的直接读回；下一轮须保留实际地址诊断并复验。[Microsoft导入调用说明](https://learn.microsoft.com/en-us/cpp/build/importing-function-calls-using-declspec-dllimport?view=msvc-170)。不修改或重建外部FFmpeg。
