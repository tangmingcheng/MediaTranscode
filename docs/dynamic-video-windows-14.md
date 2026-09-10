# Windows 动态视频第14轮：验收不完整

链路：H.264 RTP 1280×720、30fps、8,013,434bps 连续输入 → 两路 HEVC MPEG-TS/RTP 1920×1080、25fps、CBR 6Mbps、GOP50。Release CLI SHA256：C848499793E2236298E1FA62CD23FA597B48938F1BFDB6BD9225EC88B76C75E9。

## 实际命令

```powershell
& 'D:/Code/MyCode/MediaTranscode/out/build/x64-release/media_transcode_realtime_video_cli.exe' --media-id dynamic-win-14 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60620 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 127.0.0.1 --rtp-port 61620 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-14.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50 --no-audio > 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-14-cli.log' 2>&1; exit $LASTEXITCODE
& 'D:/mabs/local64/bin-video/ffmpeg.exe' -hide_banner -nostdin -re -i 'D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4' -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 'rtp://127.0.0.1:60620?rtcpport=60621&pkt_size=1200' > 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-14-source.log' 2>&1; exit $LASTEXITCODE
& 'D:/Wireshark/dumpcap.exe' -i 10 -q -f 'udp portrange 60620-60621 or udp portrange 61620-61627' -w 'D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-14.pcapng'
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-14-vlc.log rtp://@127.0.0.1:61620
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-14-vlc-add.log rtp://@127.0.0.1:61622
```

CLI 实际执行新增：

```text
add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61622 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-14-add.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50
```

源结束后才启动第三路 VLC（同播放参数，61624，日志 dynamic-win-14-vlc-third.log）；向已退出CLI写入第三路命令未执行。删除及零输出恢复未执行，不把宣布的计划命令记作成功。

## 结果

- 源自然完成3600帧/120秒，FFmpeg退出0。初始Running 17:52:23.928；第二路17:52:33.920明确action=reused、group_id=1、encoding_segment_id=1、protocol_segment_id=3，35.973 Running。全程只出现一次video_encode.first_packet，未为第二路新开生产encoder。
- 源结束后17:54:28.042按无进展超时退出，CLI退出1；两路退役。原始capture229412包/drop0，RTP64593/58069包零丢失。两路VLC D3D11VA，初始11/7/3ms与第二路11ms debug late；未取得画面截图。
- final payload bytes/objects=0，reservations/releases=37947，高水位9230724B/13对象；workerErrors/errors/drop/pressure均0。CPU426样本平均单核23.180824%、峰值59.900990%；WS118505472→200368128B。VideoOnly A/V漂移不适用。
- 控制调度延迟导致源结束前未执行剩余步骤，验收不完整，不创建成功提交。PID：CLI37180、source29508、capture35820、VLC35252/36620/33916；原始日志/抓包/SDP只在D盘，提取本报告后删除，播放器正常关闭。
