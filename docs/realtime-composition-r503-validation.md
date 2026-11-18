# r503：Windows 三源 H264 CBR CUDA/NVENC 追踪诊断（FAIL：追踪下线程统计失败）

复用r501原规格与Release产物，不改FFmpeg。增加Nsight2024.6.2 CUDA/NVENC trace，sample/cpuctxsw关闭、kill=false；不是无追踪验收。预检同工具profile realtime CLI --help exit0，生成probe.nsys-rep，只证明追踪启动，不证明CUDA活动已采集。

VLC5404、源8140/36232/30924、nsys36816、实际CLI8044（父21308）。

## 实际命令

~~~powershell
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r503-vlc.log --extraintf=rc --rc-host=127.0.0.1:42120 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r503-native- --snapshot-format=png rtp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"
~~~

~~~powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r503-source0.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r503-source0.log' -PassThru; Write-Output "SOURCE0_PID=$($p.Id)"
~~~

~~~powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r503-source1.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r503-source1.log' -PassThru; Write-Output "SOURCE1_PID=$($p.Id)"
~~~

~~~powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -vf "drawtext=fontfile=''C\:/Windows/Fonts/consola.ttf'':text=''frame=%{frame_num} pts=%{pts}'':fontsize=48:fontcolor=white:box=1:boxcolor=black:x=16:y=16" -c:v h264_nvenc -rc cbr -b:v 8200k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r503-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r503-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"
~~~

~~~powershell
$p=Start-Process -FilePath 'C:/Program Files/NVIDIA Corporation/Nsight Systems 2024.6.2/target-windows-x64/nsys.exe' -ArgumentList 'profile --trace=cuda,nvvideo --sample=none --cpuctxsw=none --kill=false --output=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r503-trace D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-r503 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62024 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r503-output.sdp --video-codec h264 --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r503-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r503-cli-error.log' -PassThru; Write-Output "CLI_PID=$($p.Id)"
~~~

## 结果

实际CLI8044很快退出，监控未及时取得句柄，数值退出码未知；错误日志明确失败，不能拿nsys的exit0当CLI成功。首错Thread32Next failed native=0 entry_size=28 enumerated_threads=7015 process_threads=102。cpuSamples0，没有有效CPU/RSS趋势；payload540预约=释放回零、errors1、encodedPackets0。后台监控因空进程对象失败已中止，所写空值不作为指标。三个源最终均完整120秒/3600帧，VLC未取得有效画面。

Nsight SQLite已确认真实CUDA API及memcpy活动：cuStreamSynchronize6次、最大129803ns；D2D16次、最大5216ns。只涉及启动，不解释r501稳态失败。完整stdout/stderr在ProcessStreams表，通过StringIds关联提取；nsys外层输出不是生产日志。

[Microsoft Thread32Next合同](https://learn.microsoft.com/en-us/windows/win32/api/tlhelp32/nf-tlhelp32-thread32next)正常耗尽使用ERROR_NO_MORE_FILES。当前源码在Thread32Next失败后立即读取GetLastError，关闭snapshot前保存；没有证据支持将native0当成功。保留校验，下一诊断仅删除nvvideo追踪，核对是否追踪影响，不据此修改生产语义。

清理清单：composition-r503-cli-error.log、composition-r503-cli.log、composition-r503-embedded-stderr.log、composition-r503-embedded-stdout.log、composition-r503-monitor.log、composition-r503-probe.nsys-rep、composition-r503-source0.log、composition-r503-source0.out、composition-r503-source1.log、composition-r503-source1.out、composition-r503-source2.log、composition-r503-source2.out、composition-r503-trace.nsys-rep、composition-r503-trace.sqlite、composition-r503-trace.txt、composition-r503-vlc.log。nsys自动临时qdstrm及quadd_session路径已查不存在；保留指定源。

16项本轮产物及精确PID均已清理。
