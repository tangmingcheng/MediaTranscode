# Windows 第18轮：动态操作成功，播放器晚帧导致完整验收失败

结论：FAIL，不创建成功验收提交。固定120秒 H.264 RTP 1280×720、30fps、8,013,434bps → HEVC/H.264 MPEG-TS/RTP 1920×1080、25fps、CBR 6Mbps、GOP50。Release SHA256：BD81B15EC1438202381825C94BFD8292E52A9D01161AA5206696E48A3B924E65。

## 实际命令

```powershell
& 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' --media-id dynamic-win-18 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60620 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 127.0.0.1 --rtp-port 61620 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-18.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50 --no-audio > 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-18-cli.log' 2>&1; exit $LASTEXITCODE
& 'D:/mabs/local64/bin-video/ffmpeg.exe' -hide_banner -nostdin -re -i 'D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4' -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 'rtp://127.0.0.1:60620?rtcpport=60621&pkt_size=1200' > 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-18-source.log' 2>&1; exit $LASTEXITCODE
& 'D:/Wireshark/dumpcap.exe' -i 10 -q -f 'udp portrange 60620-60621 or udp portrange 61620-61627' -w 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-18.pcapng'
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-18-vlc.log --extraintf=rc --rc-host=127.0.0.1:62720 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-18-initial- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61620
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-18-vlc-add.log --extraintf=rc --rc-host=127.0.0.1:62722 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-18-add- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61622
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-18-vlc-third.log --extraintf=rc --rc-host=127.0.0.1:62724 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-18-third- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61624
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-18-vlc-readd.log --extraintf=rc --rc-host=127.0.0.1:62726 --rc-quiet --snapshot-path=D:/Code/MyCode/MediaTranscode/out/acceptance --snapshot-prefix=dynamic-win-18-readd- --snapshot-format=png --no-snapshot-preview rtp://@127.0.0.1:61626
```

CLI 标准输入依 Running/Retired 状态衔接：
```text
add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61622 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-18-add.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50
add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61624 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-18-third.sdp --video-codec h264 --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50
remove 1
remove 2
remove 3
add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61626 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-18-readd.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50
```

截图通过各自 RC 端口发送 `snapshot\r\n`，与第16轮相同。PNG均实际查看。

## 结果与边界

- PID：CLI35548、FFmpeg8804、dumpcap17048；VLC16344/28556/38360/35540。
- 输出2 19:32:49.062复用group1、51.265 Running；输出3 58.022新group2、58.360 Running；输出1/2/3分别19:33:04.021、09.750、11.372正常Retired；输出4 15.146新group3、15.475 Running。删除初始消费者后第二路新画面已检查。
- FFmpeg完成3600帧/120秒退出0；CLI源结束后无进展超时退出1，最终workerErrors/errors为0。视频链路无A/V漂移指标。
- 捕获179919包、丢包0；四路RTP16309/9540/5993/41345包，序号丢失0。TS TEI/AFC/连续性错误和expert error均无结果。
- 出口73224数据报、92111112个IP线速字节，与scope计费一致；50Mbps服务曲线超额1356B，最大单包1356B。全部sender deadline/pressure/partial/ambiguous/WouldBlock为0，末尾backlog归零。
- 最终payload字节/对象0，申请与释放39593相等；CPU396样本，单核等效均值26.737212%、峰值87.224670%；工作集121843712→203284480、峰值243417088字节。
- 四路均VLC D3D11VA硬解。初始路3次too-late-to-display（53/45/35ms），第二路2次（45/49ms），故完整验收FAIL。均在对应首次PNG导出日志之后，时间相关但尚不足以证明截图是唯一原因；其他截图/RC/关闭窗口信息单列，不隐藏错误。
- [VLC源码](https://raw.githubusercontent.com/videolan/vlc/3.0.x/src/video_output/video_output.c)表明截图在显示路径复制图像并执行导出转换。后续将截图导出移到对应输出停止接收媒体后作单变量观察，保留原媒体参数和全部动态操作，排查观察操作干扰。
- 报告整理后删除本轮D盘原始日志、pcap、SDP与PNG，核查精确PID无残留。
