# RKMPP Datagram H.264 720p30 转 HEVC 1080p25 CBR6M 验收完成记录

## 范围与命令

- 日期：2026-09-01。
- 输入：真实连续 120 秒 H.264 1280x720、30 fps、raw RTP。
- 输出：RKMPP/RGA 零拷贝 HEVC 1920x1080、25 fps、CBR 6 Mbps、GOP 50、MPEG-TS/RTP。
- 部署事实：受管 egress 50 Mbps，最大 wire residence 100 ms，Linux `sch_fq`，接口 MTU 1500 B。
- 运行控制器观察窗口为 12 秒，大于核心声明的 9 秒 RTP source-clock 失活边界，不参与发送策略。

```powershell
D:\Wireshark\dumpcap.exe -i 6 -f "udp port 62740 or udp port 62741" -a duration:180 -w D:\Code\MyCode\MediaTranscode\out\acceptance\rk-low-cbr6m-v3\receiver.pcapng
D:\VideoLAN\VLC\vlc.exe --no-one-instance --verbose=2 --stats --network-caching=1000 --file-logging --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\rk-low-cbr6m-v3\vlc.log --no-video-title-show rtp://@192.168.96.122:62740
```

```bash
source /etc/profile.d/ffenv.sh
ffenv on
source /opt/mt-tools/mtenv.sh
mtenv on
tcpdump -i eth0 -n -s 0 -w /home/tang/task5-f5913278/out/acceptance/rk-low-cbr6m-v3/sender.pcap 'dst host 192.168.96.122 and (udp port 62740 or udp port 62741)'
/home/tang/task5-f5913278/out/build/rk-release/media_transcode_realtime_video_cli --media-id rk-low-h264720p30-hevc1080p25-cbr6m-v3 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --progress-timeout-ms 12000 --video-rtp-url rtp://127.0.0.1:61740 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 192.168.96.122 --rtp-port 62740 --sdp /home/tang/task5-f5913278/out/acceptance/rk-low-cbr6m-v3/output.sdp --video-codec hevc --rc cbr --bitrate 6000 --width 1920 --height 1080 --fps 25 --gop 50 --no-audio
/usr/local/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 'rtp://127.0.0.1:61740?rtcpport=61741&pkt_size=1200'
```

CLI 先启动，FFmpeg 随后直接启动，二者之间没有检测、容量探测或等待。

## 结果

- FFmpeg 完整发送 3600 帧、120 秒，退出码 0；源结束后 CLI 如实以 `RTP video source clock evidence expired` 终止。
- sender/receiver 均为 74712 个 Datagram；RTP 74684、RTCP 28，RTP loss/reorder 为 0，MPEG-TS continuity skip/drop 为 0。
- sender 1/5/10/100 ms 最大 IP 字节为 4068/16272/31188/290116 B；动态最大服务率 3774538 B/s 加一个 1356 B Datagram 的对应允许值为 5130/20228/39101/378809 B，全部满足，无追赶式 burst。
- Windows receiver 的短窗口出现 Npcap/NIC 批量时间戳聚簇；发送端相同序列窗口满足服务曲线，100 ms 两端一致，且完整序列零丢失。
- sender would-block、writable wait、deadline miss、pressure、partial submit、ambiguous submit 均为 0；最终 backlog 为 0；`sch_fq` drop 增量为 0。
- VLC 建立 HEVC 1920x1080 D3D11VA 视频输出并完成缓冲；日志无 corrupt、black、lost、discontinuity 或 decoder error。
- RK CLI 平均单核 CPU 12.685%，峰值 RSS 108339200 B；最终 queued、dropped buffer、graph payload bytes/objects、reservation 和 sender backlog 全部归零。

结论：RKMPP MPEG-TS/RTP H.264 720p30 转 HEVC 1080p25 CBR6M Datagram 发送门禁通过。
