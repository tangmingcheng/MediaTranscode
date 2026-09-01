# Linux sch_fq 接口 MTU 公共契约 RKMPP CBR6M 验收完成记录

## 范围与命令

- 日期：2026-09-01。
- 输入：真实连续 120 秒 H.264 1280x720、30 fps、raw RTP。
- 输出：RKMPP/RGA 零拷贝 HEVC 1920x1080、25 fps、CBR target 6 Mbps、GOP 50、MPEG-TS/RTP。
- 部署事实：受管 egress 50 Mbps，最大 wire residence 100 ms；Linux 内部通过 `ifindex` 权威读取接口 MTU 1500 B。

```bash
tc qdisc replace dev eth0 root fq pacing quantum 1500 initial_quantum 1500
tc -details qdisc show dev eth0
```

```powershell
D:\Wireshark\dumpcap.exe -i 6 -f "udp port 61734 or udp port 61735" -a duration:140 -w D:\Code\MyCode\MediaTranscode\out\acceptance\rk-fq-interface-mtu-cbr6m-v3\receiver.pcapng
D:\VideoLAN\VLC\vlc.exe --no-one-instance --verbose=2 --stats --network-caching=1000 --file-logging --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\rk-fq-interface-mtu-cbr6m-v3\vlc.log --no-video-title-show rtp://@192.168.96.122:61734
```

```bash
/home/tang/task5-f5913278/out/build/rk-release/media_transcode_realtime_video_cli --media-id rk-fq-interface-mtu-h264720p30-hevc1080p25-cbr6m-v3 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60734 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 192.168.96.122 --rtp-port 61734 --sdp /home/tang/task5-f5913278/out/acceptance/rk-fq-interface-mtu-cbr6m-v3/output.sdp --video-codec hevc --rc cbr --bitrate 6000 --width 1920 --height 1080 --fps 25 --gop 50 --no-audio
/usr/local/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 'rtp://127.0.0.1:60734?rtcpport=60735&pkt_size=1200'
tcpdump -i eth0 -n -s 0 -w /home/tang/task5-f5913278/out/acceptance/rk-fq-interface-mtu-cbr6m-v3/sender.pcap 'dst host 192.168.96.122 and (udp port 61734 or udp port 61735)'
```

## 结果

- `quantum=1356` 的旧配置先被 planner 在 DAG 前拒绝；`quantum=initial_quantum=1500` 与接口 MTU 权威事实一致后才允许构建。
- FFmpeg 完整发送 3600 帧、120 秒；sender/receiver 均为 75204 个 Datagram，RTP 75174、RTCP 30，RTP loss/reorder 为 0，MPEG-TS continuity skip/drop 为 0。
- sender 1/5/10/100 ms 最大 IP 字节为 4296/16500/32544/301380 B；动态最大服务率 3621959 B/s 加一个 1356 B Datagram 的对应允许值为 4978/19466/37576/363552 B，全部满足，无追赶式 burst。
- Windows receiver 的 1 ms 抓包聚簇为 9492 B，但发送端相同序列窗口为 4296 B；10/100 ms 两端趋势一致，且完整序列零丢失，属于接收端 Npcap/NIC 批量时间戳。
- `sch_fq` dropped 为 0、requeues 增量为 0、最终 backlog 为 0；sender would-block、writable wait、deadline miss、pressure、partial submit、ambiguous submit 均为 0。
- VLC 建立 HEVC 1920x1080 D3D11VA，记录首帧与缓冲完成；无 corrupt、black、lost、discontinuity 或 decoder error，仅启动期一次 `missing 12 ms`。
- RK CLI 平均单核 CPU 10.117%，峰值 RSS 61116416 B；最终 dropped buffer、graph payload、reservation 与 sender backlog 全部清零。
- 目标机 tcpdump 在关闭边界落盘 75201 包，少于 sender/receiver 3 包且内核丢包为 0；该边界差异不用于缩减生产 sender 与接收端完整对账结论。
- 源结束后 CLI 如实以 `RTP video source clock evidence expired` 终止。

结论：接口 MTU qdisc 几何的 RKMPP MPEG-TS/RTP CBR6M 门禁通过；独立 RTP 与 MPEG-TS/UDP 仍需分别实跑证明公共 sender 复用。
