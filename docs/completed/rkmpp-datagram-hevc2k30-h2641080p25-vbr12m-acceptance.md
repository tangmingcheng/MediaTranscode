# RKMPP Datagram HEVC 2K30 转 H.264 1080p25 VBR12M 验收完成记录

## 范围与命令

- 日期：2026-09-01。
- 输入：真实连续 120 秒 HEVC 2560x1440、30 fps、raw RTP。
- 输出：RKMPP/RGA 零拷贝 H.264 1920x1080、25 fps、VBR 5/12/13 Mbps、GOP 50、MPEG-TS/RTP。
- 部署事实：受管 egress 50 Mbps，最大 wire residence 100 ms，Linux `sch_fq`，接口 MTU 1500 B。
- 运行控制器观察窗口为 12 秒，大于核心声明的 9 秒 RTP source-clock 失活边界，不参与发送策略。

```powershell
D:\Wireshark\dumpcap.exe -i 6 -f "udp port 62752 or udp port 62753" -a duration:180 -w D:\Code\MyCode\MediaTranscode\out\acceptance\rk-high-vbr12m-v2\receiver.pcapng
D:\VideoLAN\VLC\vlc.exe --no-one-instance --verbose=2 --stats --network-caching=1000 --file-logging --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\rk-high-vbr12m-v2\vlc.log --no-video-title-show rtp://@192.168.96.122:62752
```

```bash
source /etc/profile.d/ffenv.sh
ffenv on
source /opt/mt-tools/mtenv.sh
mtenv on
tcpdump -i eth0 -n -s 0 -w /home/tang/task5-f5913278/out/acceptance/rk-high-vbr12m-v2/sender.pcap 'dst host 192.168.96.122 and (udp port 62752 or udp port 62753)'
/home/tang/task5-f5913278/out/build/rk-release/media_transcode_realtime_video_cli --media-id rk-high-hevc2k30-h2641080p25-vbr12m-v2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --progress-timeout-ms 12000 --video-rtp-url rtp://127.0.0.1:61752 --video-rtp-codec hevc --video-rtp-payload-type 98 --video-rtp-clock-rate 90000 --rtp-host 192.168.96.122 --rtp-port 62752 --sdp /home/tang/task5-f5913278/out/acceptance/rk-high-vbr12m-v2/output.sdp --video-codec h264 --rc vbr --min-bitrate 5000 --bitrate 12000 --max-bitrate 13000 --width 1920 --height 1080 --fps 25 --gop 50 --no-audio
/usr/local/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s-2k-hevc.mp4 -map 0:v:0 -an -c:v copy -bsf:v hevc_mp4toannexb -f rtp -payload_type 98 'rtp://127.0.0.1:61752?rtcpport=61753&pkt_size=1200'
```

CLI 先启动，FFmpeg 随后直接启动，二者之间没有检测、容量探测或等待。

## 结果

- FFmpeg 完整发送 3600 帧、120 秒，退出码 0；源结束后 CLI 如实以 `RTP video source clock evidence expired` 终止。
- planner 自动选择 `hevc_rkmpp -> scale_rkrga -> h264_rkmpp`，未传入 hardware backend。
- sender/receiver 均为 143447 个 Datagram；RTP 143420、RTCP 27，RTP loss/reorder 为 0，MPEG-TS continuity skip/drop 为 0。
- sender 1/5/10/100 ms 最大 IP 字节为 5424/21696/41136/361568 B；动态最大服务率 5734489 B/s 加一个 1356 B Datagram 的对应允许值为 7090/30028/58700/574804 B，全部满足，无追赶式 burst。
- Windows receiver 的短窗口出现 Npcap/NIC 批量时间戳聚簇；发送端相同序列窗口满足服务曲线，100 ms 两端一致，且完整序列零丢失。
- sender would-block、writable wait、deadline miss、pressure、partial submit、ambiguous submit 均为 0；最终 backlog 为 0；`sch_fq` drop 增量为 0。
- VLC 识别 MPEG-TS/H.264，建立 1920x1080 D3D11VA 并记录 `Received first picture`；首帧后无 decoder、corrupt、black、lost 或 discontinuity 错误。启动期硬解上下文的一次 `get_buffer/no frame` 在首帧前恢复。
- RK CLI 平均单核 CPU 19.173%，峰值 RSS 178823168 B；CPU 按当前明确范围只记录。最终 queued、dropped buffer、graph payload bytes/objects、reservation 和 sender backlog 全部归零。

结论：RKMPP MPEG-TS/RTP HEVC 2K30 转 H.264 1080p25 VBR 5/12/13M Datagram 发送门禁通过。
