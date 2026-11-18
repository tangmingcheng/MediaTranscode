# r279：三路RTP合屏转HEVC MPEG-TS/RTP验证

状态：输入探测失败，已归档清理；未进入合屏链路。Release SHA256：5E278491F4C3396A0614B4040BBA3C38288C8B079C45472ABE3942750C039CF7。仅realtime，原120秒源、三路H.264 720p30约8Mbps；输出HEVC720p30 8Mbps CBR、AAC192kbps、MPEG-TS/RTP。

会话83197；CLI11912，VLC12276，三源21724/27344/31528。

## 实际命令
~~~powershell
$ErrorActionPreference='Stop'
$vlc=Start-Process -FilePath 'D:/VideoLAN/VLC/vlc.exe' -ArgumentList '--no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r279-vlc.log --extraintf=rc --rc-host=127.0.0.1:63020 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=composition-r279-native- --snapshot-format=png rtp://@127.0.0.1:62020' -WindowStyle Hidden -PassThru
Write-Output "VLC_PID=$($vlc.Id)"
$source0=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61020?rtcpport=61021&pkt_size=1200" -map 0:a:0 -vn -c:a copy -f rtp -payload_type 97 -rtpflags send_bye "rtp://127.0.0.1:61022?rtcpport=61023&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r279-source0-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r279-source0.log' -PassThru
$source1=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61030?rtcpport=61031&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r279-source1-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r279-source1.log' -PassThru
$source2=Start-Process -FilePath 'D:/mabs/local64/bin-video/ffmpeg.exe' -ArgumentList '-hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 -rtpflags send_bye "rtp://127.0.0.1:61040?rtcpport=61041&pkt_size=1200"' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r279-source2-stdout.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r279-source2.log' -PassThru
Write-Output "SOURCE_PIDS=$($source0.Id),$($source1.Id),$($source2.Id)"
$cli=Start-Process -FilePath 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' -ArgumentList '--media-id composition-r279 --inputs "--input-type rtp --input-latency-ms 100 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61030 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 150 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61040 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000" --inputs "--input-type rtp --input-latency-ms 200 --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61020 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --audio-rtp-url rtp://127.0.0.1:61022 --audio-rtp-codec aac --audio-rtp-payload-type 97 --audio-rtp-clock-rate 44100 --audio-rtp-channels 2 --audio-rtp-fmtp ''profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210''" --preparation-storage-limit-bytes 2147483648 --grid fixed --audio-source 2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 62020 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r279-output.sdp --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --gop 60 --audio-codec aac --audio-rc cbr --audio-bitrate 192 --sample-rate 44100 --channels 2' -WindowStyle Hidden -RedirectStandardOutput 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r279-cli.log' -RedirectStandardError 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r279-cli-error.log' -PassThru
Write-Output "CLI_PID=$($cli.Id)"
$watch=[Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0
$lastCpu=0.0
$cores=[Environment]::ProcessorCount
while(-not $cli.WaitForExit(1000)) {
$cli.Refresh()
$time=$watch.Elapsed.TotalSeconds
$cpu=$cli.TotalProcessorTime.TotalSeconds
$usage=100*($cpu-$lastCpu)/(($time-$lastTime)*$cores)
Add-Content -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/composition-r279-monitor.log' -Value "seconds=$time cpu=$usage ws=$($cli.WorkingSet64) private=$($cli.PrivateMemorySize64)"
$lastTime=$time
$lastCpu=$cpu
}
Write-Output "CLI_EXIT=$($cli.ExitCode)"
$source0.WaitForExit()
Write-Output "SOURCE0_EXIT=$($source0.ExitCode)"
$source1.WaitForExit()
Write-Output "SOURCE1_EXIT=$($source1.ExitCode)"
$source2.WaitForExit()
Write-Output "SOURCE2_EXIT=$($source2.ExitCode)"
exit $cli.ExitCode
~~~

## 结果：输入探测失败，未进入生产DAG
CLI会话83197报告退出1，30秒open timeout，matching_rtp_packets=0、matching_datagram_bytes=0、parameter_sets=none。本次启动在受限环境执行，常规CIM无法访问且Get-Process不可见，而提权只读查询确认源进程仍存在；尚不能仅凭此断言网络隔离是唯一根因。需要同产物同命令在可访问本机进程/通信的执行环境复验。

### CLI错误
~~~text
[CLI] realtime video graph preflight failed: NotInitialized: raw RTP video signaling probe reached open timeout before complete parameter sets; matching_rtp_packets=0 matching_datagram_bytes=0 parameter_sets=none

~~~
### 清理前证据
~~~text
seconds=1.0119091 cpu=0.140373729760365 ws=5414912 private=17920000
seconds=2.0456436 cpu=0.068704998500802 ws=13922304 private=340860928
seconds=3.065421 cpu=0 ws=13922304 private=340860928
seconds=4.0898872 cpu=0 ws=13930496 private=340860928
seconds=5.1115838 cpu=0 ws=13930496 private=340860928
seconds=6.1345168 cpu=0 ws=13930496 private=340860928
seconds=7.159372 cpu=0 ws=13930496 private=340860928
seconds=8.1917014 cpu=0 ws=13930496 private=340860928
seconds=9.2142781 cpu=0 ws=13930496 private=340860928
seconds=10.2379447 cpu=0 ws=13930496 private=340860928
seconds=11.2712353 cpu=0 ws=13930496 private=340860928
seconds=12.2966877 cpu=0 ws=13930496 private=340860928
seconds=13.3271636 cpu=0 ws=13930496 private=340860928
seconds=14.3463665 cpu=0 ws=13930496 private=340860928
seconds=15.3760543 cpu=0 ws=13930496 private=340860928
seconds=16.4031057 cpu=0 ws=13930496 private=340860928
seconds=17.4255765 cpu=0 ws=13930496 private=340860928
seconds=18.4470853 cpu=0 ws=13930496 private=340860928
seconds=19.4714988 cpu=0 ws=13930496 private=340860928
seconds=20.494549 cpu=0 ws=13930496 private=340860928
seconds=21.5217161 cpu=0 ws=13930496 private=340860928
seconds=22.5466638 cpu=0 ws=13930496 private=340860928
seconds=23.5757833 cpu=0 ws=13930496 private=340860928
seconds=24.600425 cpu=0 ws=13930496 private=340860928
seconds=25.621661 cpu=0 ws=13930496 private=340860928
seconds=26.645322 cpu=0 ws=13930496 private=340860928
seconds=27.6655838 cpu=0 ws=13930496 private=340860928
seconds=28.6919993 cpu=0 ws=13930496 private=340860928
seconds=29.7142043 cpu=0 ws=13930496 private=340860928
seconds=30.7216138 cpu=0 ws=13930496 private=340860928

source0
frame= 2601 fps= 30 q=-1.0 size=   85923KiB time=00:01:26.70 bitrate=8118.2kbits/s speed=1.01x elapsed=0:01:26.21    
frame= 2617 fps= 30 q=-1.0 size=   86491KiB time=00:01:27.23 bitrate=8122.3kbits/s speed=1.01x elapsed=0:01:26.71    
source1
frame= 2602 fps= 30 q=-1.0 size=   85985KiB time=00:01:26.73 bitrate=8121.3kbits/s speed=1.01x elapsed=0:01:26.21    
frame= 2617 fps= 30 q=-1.0 size=   86491KiB time=00:01:27.23 bitrate=8122.3kbits/s speed=1.01x elapsed=0:01:26.72    
source2
frame= 2602 fps= 30 q=-1.0 size=   85985KiB time=00:01:26.73 bitrate=8121.3kbits/s speed=1.01x elapsed=0:01:26.21    
frame= 2617 fps= 30 q=-1.0 size=   86491KiB time=00:01:27.23 bitrate=8122.3kbits/s speed=1.01x elapsed=0:01:26.72    
~~~
CLI未建立媒体链路，无A/V漂移或合屏快照。本轮不构成r279运行正确性的证据。测试判定失败后按精确PID清理源21724/27344/31528及VLC12276，清理退出不算自然EOF。

源清理退出均-1；五个PID经提权查询确认不存在。清理文件清单：
composition-r279-cli-error.log
composition-r279-cli.log
composition-r279-monitor.log
composition-r279-source0-stdout.log
composition-r279-source0.log
composition-r279-source1-stdout.log
composition-r279-source1.log
composition-r279-source2-stdout.log
composition-r279-source2.log
composition-r279-vlc.log

清理完成：10文件删除；保留复用120秒源。
