# RKMPP 第1轮：压缩输入持有合同缺失

结论：FAIL，准备阶段失败，未完成媒体输出验收。commit 4bf43635，目标机192.168.130.229，源码独立目录 /home/tang/dynamic-video-4bf43635。输入与 Windows 相同120秒源，SHA256：6e0de760672a6b8386b59b22f824de442e2a5ef7594d7b872c6851b64cabca9b。链路 H.264 RTP 1280×720、30fps、8,013,434bps → HEVC MPEG-TS/RTP 1920×1080、25fps、CBR 6Mbps、GOP50。

## 实际命令

先执行 `source /opt/mt-tools/mtenv.sh`、`mtenv on`、`ffenv on`。本轮使用交互 SSH 命令，无临时脚本文件。

```bash
mkfifo /home/tang/dynamic-rk-01.fifo
exec 9<>/home/tang/dynamic-rk-01.fifo
/usr/sbin/tcpdump -i any -n -U -w /home/tang/dynamic-rk-01.pcap 'udp portrange 60620-60621 or udp portrange 61620-61621' > /home/tang/dynamic-rk-01-capture.log 2>&1 &
/home/tang/dynamic-video-4bf43635/out/build/rk-release/media_transcode_realtime_video_cli --media-id dynamic-rk-01 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60620 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 192.168.96.122 --rtp-port 61620 --sdp /home/tang/dynamic-rk-01.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50 --no-audio < /home/tang/dynamic-rk-01.fifo > /home/tang/dynamic-rk-01-cli.log 2>&1 &
/usr/local/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 'rtp://127.0.0.1:60620?rtcpport=60621&pkt_size=1200' > /home/tang/dynamic-rk-01-source.log 2>&1 &
ps -p 1999050 -o pid,pcpu,rss,etime,args
kill -INT 1999053
kill -INT 1999045
```

Windows 播放命令：
```powershell
& 'D:/VideoLAN/VLC/vlc.exe' --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-rk-01-vlc.log rtp://@192.168.96.122:61620
```

## 结果与清理

- PID：CLI 1999050、FFmpeg 1999053、tcpdump 1999045、Windows VLC 4148。
- 19:11:08.642 CLI 报 `selected decoder lacks an authoritative compressed-input retention adapter`，准备阶段退出1；发送0数据报。保留 fail-closed 校验，需补真实平台合同。
- CPU/RSS 读取时 CLI 已退出，无可用运行趋势；无输出画面，无 A/V 漂移数据。
- 停止源流后 FFmpeg 退出255（SIGINT），1486帧/49.53秒；不能作为120秒验收。tcpdump 退出0，捕获44153包，内核丢包0。
- 本机原始日志仅在 D 盘，远端原始文件位于 /home/tang；整理后删除本轮日志、pcap、SDP、FIFO，以及远端构建日志，检查精确 PID 无残留。源视频与编译结果保留。
