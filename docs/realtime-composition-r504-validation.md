# r504：三源H264 CBR仅CUDA追踪诊断（FAIL：同样在线程统计阶段失败）

原规格同r501/r503，仅将trace=cuda,nvvideo改为trace=cuda；未修改源码或FFmpeg。VLC38188、源6464/13612/15968、nsys37540、实际CLI14736父28036。

## 实际命令

~~~powershell
$p=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r504-vlc.log --extraintf=rc --rc-host=127.0.0.1:42120 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r504-native- --snapshot-format=png rtp://@:62024' -WindowStyle Hidden -PassThru; Write-Output "VLC_PID=$($p.Id)"
~~~

~~~powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r504-source0.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r504-source0.log' -PassThru; Write-Output "SOURCE0_PID=$($p.Id)"
~~~

~~~powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r504-source1.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r504-source1.log' -PassThru; Write-Output "SOURCE1_PID=$($p.Id)"
~~~

~~~powershell
$p=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -vf "drawtext=fontfile=''C\:/Windows/Fonts/consola.ttf'':text=''frame=%{frame_num} pts=%{pts}'':fontsize=48:fontcolor=white:box=1:boxcolor=black:x=16:y=16" -c:v h264_nvenc -rc cbr -b:v 8200k -g 60 -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r504-source2.out' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r504-source2.log' -PassThru; Write-Output "SOURCE2_PID=$($p.Id)"
~~~

~~~powershell
$p=Start-Process -FilePath 'C:/Program Files/NVIDIA Corporation/Nsight Systems 2024.6.2/target-windows-x64/nsys.exe' -ArgumentList 'profile --trace=cuda --sample=none --cpuctxsw=none --kill=false --output=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r504-trace D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe --media-id composition-r504 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62024 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r504-output.sdp --video-codec h264 --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r504-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r504-cli-error.log' -PassThru; Write-Output "CLI_PID=$($p.Id)"
~~~

## 结果

首错Thread32Next failed native=0 entry_size=28 enumerated_threads=6961 process_threads=102；CLI14736在监控取得句柄前已退出，数值退出码未知，不能使用nsys的退出码代替。SQLite有865条CUDA API及30条memcpy记录，但仍仅启动阶段，未取得原稳态延迟证据。stdout未包含完整final report，不推断最终账本或CPU/RSS。三源日志最终完整120秒/3600帧。

独立审查retired_control_review_b未发现解释native0的具体源码缺陷：dwSize28、GetLastError立即读取、CloseHandle在保存之后。cuda-only失败仅排除nvvideo为必要条件，不证明Nsight根因。下一单变量为trace=none与直接启动对照；禁止native0成功化，不修改FFmpeg。

清理清单：composition-r504-cli-error.log、composition-r504-cli.log、composition-r504-embedded-stderr.log、composition-r504-embedded-stdout.log、composition-r504-source0.log、composition-r504-source0.out、composition-r504-source1.log、composition-r504-source1.out、composition-r504-source2.log、composition-r504-source2.out、composition-r504-trace.nsys-rep、composition-r504-trace.sqlite、composition-r504-vlc.log。nsys-report-5fb5.qdstrm及quadd_session_1028036需核对自动清理。

13项本轮产物已删除；Nsight外部临时路径已不存在。
