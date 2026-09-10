# Windows 第23轮：长GOP复用加入通过

结论：补充真实链路完整PASS。生产源码41bb17a1，二进制SHA256为72BCDF64B3BB406F70FDCB0B78D9CB565750D3909C2782D642CEE378228065E5。连续120秒H.264 RTP 1280×720、30fps、8,013,434bps → 两路HEVC MPEG-TS/RTP 1920×1080、25fps、CBR6Mbps、GOP500。原30秒事务期限不变；本轮补长IDR间隔，原GOP50完整矩阵已由第22轮通过。

## 实际命令
```powershell
& 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' --media-id dynamic-win-23 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60620 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 127.0.0.1 --rtp-port 61620 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-23.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 500 --no-audio > 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-23-cli.log' 2>&1; exit $LASTEXITCODE
& 'D:/mabs/local64/bin-video/ffmpeg.exe' -hide_banner -nostdin -re -i 'D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4' -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 'rtp://127.0.0.1:60620?rtcpport=60621&pkt_size=1200' > 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-23-source.log' 2>&1; exit $LASTEXITCODE
& 'D:/Wireshark/dumpcap.exe' -i 10 -q -f 'udp portrange 60620-60621 or udp portrange 61620-61623' -w 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-23.pcapng'
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --no-file-logging --network-caching=1000 --extraintf=rc --rc-host=127.0.0.1:62720 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-23-initial- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61620 2>&1 | ForEach-Object { '{0:o} {1}' -f [DateTime]::UtcNow, $_.ToString() } | Out-File -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-23-vlc.log' -Encoding UTF8
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --no-file-logging --network-caching=1000 --extraintf=rc --rc-host=127.0.0.1:62722 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-23-add- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61622 2>&1 | ForEach-Object { '{0:o} {1}' -f [DateTime]::UtcNow, $_.ToString() } | Out-File -LiteralPath 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-23-vlc-add.log' -Encoding UTF8
```

初始Running后按其日志时间22秒加入，第二路Running后6秒删除第一路：
```text
add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61622 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-23-add.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 500
remove 1
```
源及CLI结束后，对62720/22发送RC `snapshot\r\n`，实际查看两张PNG后关闭VLC。

## 结果
- PID：CLI27844、FFmpeg17184、dumpcap20892；VLC28288/29636。
- 初始09:19:19.736 Running；第二路42.115 Preparing、42.704复用group1并WaitingForRandomAccess，59.818 Running，实际等待17.114秒。全程仅初始encoder first_packet，证明同组复用；没有扩大30秒期限或重新起算。
- 第一输出09:20:07.377正常Retired；第二路继续至源结束。源3600帧/120秒退出0，CLI09:21:23.714按裸RTP源结束无进展策略退出1。
- 两路D3D11VA、两张后续游戏画面正常；无too-late/deadlock/corrupt，仅10/11ms debug late。截图候选转换失败后成功导出及关闭窗口/RC错误按既有边界单列。
- 捕获174593包/drop0；两路RTP25396/42469包，零丢失；TS TEI/AFC/连续性和expert error无命中。新增路首RTP在抓包40.649541秒，符合自然20秒IDR周期中的下一边界。
- 聚合67898数据报/85485340 IP字节，与scope一致；50Mbps服务曲线超额1356B、最大包1356B。sender deadline/pressure/partial/ambiguous/WouldBlock0，最终backlog0。
- CPU431样本，单核等效平均18.040055%、峰值59.024390%；工作集121982976→203108352B、峰值相同；payload最终0字节/对象，38131次申请释放相等。workerErrors/errors/pressure/drop0，stalledIntervals1保留；视频A/V漂移不适用。
- 整理后删除D盘本轮日志、pcap、SDP、PNG，核查精确PID无残留。单独提交本项成功；原期限不足的提前拒绝与RKMPP仍需完成。
