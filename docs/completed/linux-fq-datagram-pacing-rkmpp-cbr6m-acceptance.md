# Linux sch_fq Datagram 发送控制 RKMPP CBR6M 验收完成记录

## 验收范围

- 日期：2026-09-01。
- 输入：真实连续 120 秒 H.264 1280x720、30 fps、raw RTP。
- 处理：planner 自动选择 `h264_rkmpp -> scale_rkrga -> hevc_rkmpp`，`zero_copy=true`。
- 输出：HEVC 1920x1080、25 fps、CBR target 6 Mbps、GOP 50、MPEG-TS/RTP。
- 部署事实：受管 egress 50 Mbps，最大 wire residence 100 ms。
- Linux 执行：用户态 GBRA/WebRTC queue-drain pacer；socket 固定 `SO_MAX_PACING_RATE=6250000 B/s`；根 `sch_fq` 启用 pacing。

## 实际命令

目标机 qdisc：

```bash
tc qdisc replace dev eth0 root fq pacing quantum 1356 initial_quantum 1356
tc -details qdisc show dev eth0
```

接收端抓包：

```powershell
D:\Wireshark\dumpcap.exe -i 6 -f "udp port 61732 or udp port 61733" -a duration:140 -w D:\Code\MyCode\MediaTranscode\out\acceptance\rk-fq-cbr6m-v2\receiver.pcapng
```

VLC：

```powershell
D:\VideoLAN\VLC\vlc.exe --no-one-instance --verbose=2 --stats --network-caching=1000 --file-logging --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\rk-fq-cbr6m-v2\vlc.log --no-video-title-show rtp://@192.168.96.122:61732
```

CLI：

```bash
/home/tang/task5-f5913278/out/build/rk-release/media_transcode_realtime_video_cli --media-id rk-fq-h264720p30-hevc1080p25-cbr6m-v2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60732 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 192.168.96.122 --rtp-port 61732 --sdp /home/tang/task5-f5913278/out/acceptance/rk-fq-cbr6m-v2/output.sdp --video-codec hevc --rc cbr --bitrate 6000 --width 1920 --height 1080 --fps 25 --gop 50 --no-audio
```

源 FFmpeg：

```bash
/usr/local/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 'rtp://127.0.0.1:60732?rtcpport=60733&pkt_size=1200'
```

目标机发送端抓包：

```bash
tcpdump -i eth0 -n -s 0 -w /home/tang/task5-f5913278/out/acceptance/rk-fq-cbr6m-v2/sender.pcap 'dst host 192.168.96.122 and (udp port 61732 or udp port 61733)'
```

## 结果

- FFmpeg 完整发送 3600 帧、120 秒并正常退出；源结束后 CLI 如实以 `RTP video source clock evidence expired` 终止，不改写为成功。
- sender、目标机抓包、Windows receiver 均为 75199 个 Datagram；RTP 75174、RTCP 25，RTP loss/reorder 为 0，MPEG-TS continuity skip/drop 为 0。
- sender 1/5/10/100 ms 最大 IP 字节分别为 4296/16500/31644/295728 B；对应最大动态服务率 3889267 B/s 加一个 1356 B Datagram 余量的允许值分别为 5245/20802/40249/390283 B，全部满足，无追赶式 burst。
- receiver 的 1 ms 抓包窗口出现 9492 B 聚簇，但发送端相同序列窗口为 4296 B；10/100 ms 趋势一致，且完整序列零丢失。该差异属于接收端 Npcap/NIC 批量时间戳，不作为发送端 burst 证据。
- `sch_fq` 测试前后 dropped 均为 0、requeues 均为 8，增量为 0，最终 backlog 为 0。
- sender 提交 75199 个 Datagram；would-block、writable wait、deadline miss、pressure、partial submit、ambiguous submit 均为 0；最终 materialized/scheduled/submitted/committed sequence 均为 75199，backlog 为 0。
- 两个 endpoint 的 `SO_MAX_PACING_RATE` readback 均为 6250000 B/s，与 50 Mbps 部署上限一致。
- VLC 建立 HEVC 1920x1080 D3D11VA 解码并记录 `Received first picture`、`Stream buffering done`；无 corrupt、black、lost、discontinuity 或 decoder error。启动后仅一次 `might be displayed late (missing 9 ms)`，没有连续 late/drop 证据。
- RK CLI 平均单核 CPU 10.641%，峰值 RSS 62828544 B；最终 dropped buffer、graph payload、reservation 和 sender backlog 全部清零。

结论：该 RKMPP MPEG-TS/RTP CBR6M 实流门禁通过；不代表独立 RTP、MPEG-TS/UDP 或完整链路矩阵已经验收。
