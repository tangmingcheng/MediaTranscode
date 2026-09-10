# Windows 第20轮：排空正常，播放器晚帧门禁未通过

结论：FAIL，不创建成功验收提交。固定120秒H.264 RTP 1280×720、30fps、8,013,434bps → HEVC/H.264 MPEG-TS/RTP 1920×1080、25fps、CBR6Mbps、GOP50。二进制SHA256：9D8B050C439CDEB994CA9868DDA820779E854F8D5EC697DD4A652BE77C0D2E72。

## 实际命令

```powershell
& 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' --media-id dynamic-win-20 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60620 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 127.0.0.1 --rtp-port 61620 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-20.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50 --no-audio > 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-20-cli.log' 2>&1; exit $LASTEXITCODE
& 'D:/mabs/local64/bin-video/ffmpeg.exe' -hide_banner -nostdin -re -i 'D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4' -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 'rtp://127.0.0.1:60620?rtcpport=60621&pkt_size=1200' > 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-20-source.log' 2>&1; exit $LASTEXITCODE
& 'D:/Wireshark/dumpcap.exe' -i 10 -q -f 'udp portrange 60620-60621 or udp portrange 61620-61627' -w 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-20.pcapng'
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-20-vlc.log --extraintf=rc --rc-host=127.0.0.1:62720 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-20-initial- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61620
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-20-vlc-add.log --extraintf=rc --rc-host=127.0.0.1:62722 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-20-add- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61622
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-20-vlc-third.log --extraintf=rc --rc-host=127.0.0.1:62724 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-20-third- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61624
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-20-vlc-readd.log --extraintf=rc --rc-host=127.0.0.1:62726 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-20-readd- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61626
```

标准输入依Running/Retired衔接：
```text
add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61622 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-20-add.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50
add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61624 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-20-third.sdp --video-codec h264 --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50
remove 1
remove 2
remove 3
add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61626 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-20-readd.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50
```

媒体结束后，对62720/22/24/26发送RC `snapshot\r\n`，实际查看四张保留画面，然后正常关闭VLC。


## 结果

- PID：CLI27008、源32700、dumpcap4572、VLC38352/34772/33112/32636。
- 第二路19:54:13.489复用group1，14.659 Running；第三路23.139新group2，23.442 Running。第一/二/三路29.547、36.576、38.309无错误Retired；第四路42.659新group3，42.953 Running。第19轮FileMux排空失败未再发生。
- 源3600帧/120秒自然结束、退出0；CLI源结束后19:56:00.464按裸RTP无EOS既有无进展策略退出1，非媒体期失败。
- 四张实际画面正常、均D3D11VA；初始VLC有49/21ms、第二路21ms too-late-to-display告警，故不判完整通过。日志没有时间戳，不能据邻接行推断具体发生时间；均在截图请求前出现，本轮截图在媒体结束后。
- 截图转换候选失败后仍完成PNG导出；RC短连接read/write error及关闭窗口SetThumbNailClip单列，不冒充生产解码失败。
- 捕获181887包/drop0；四路RTP17523/11407/7002/39223包、零丢失；TS TEI/AFC/连续性及expert error无命中。分析首次进程异常退出，恢复后同一pcap重跑exit0取得结果。
- 聚合75192数据报/94615784 IP字节，与scope账目一致；50Mbps服务曲线超额1356B，最大包1356B。各sender deadline/pressure/partial/ambiguous/WouldBlock为0、backlog最终0。
- CPU430样本，单核等效平均19.381495%、峰值65.296804%；工作集118587392→205434880B、峰值241246208B；payload最终0字节/对象，40123次申请与释放相等。workerErrors/errors/pressure为0。纯视频A/V漂移不适用。
- 原始日志、pcap、SDP、PNG只在D盘。2026-09-10续接时先核查媒体进程均已退出，完成分析后删除本轮全部原始材料；源与构建产物保留。
