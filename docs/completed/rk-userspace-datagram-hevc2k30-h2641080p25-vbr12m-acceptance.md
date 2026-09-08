# RKMPP 公共用户态 Datagram 发送控制高质量 VBR 验收

- 日期：2026-09-01
- 输入：真实连续 120 秒 HEVC 2560x1440、30 fps、raw RTP。
- 输出：RKMPP H.264 1920x1080、25 fps、VBR 5/12/13 Mbps、MPEG-TS/RTP。
- 部署事实：受管 egress 50 Mbps，最大 wire residence 100 ms。

## 实际命令

Windows 接收端：

```powershell
D:\Wireshark\dumpcap.exe -i 6 -f "host 192.168.130.229 and (udp port 62882 or udp port 62883)" -a duration:240 -w D:\Code\MyCode\MediaTranscode\out\acceptance\rk-userspace-high-vbr12m-v2\receiver.pcapng

D:\VideoLAN\VLC\vlc.exe --no-one-instance --verbose=2 --stats --network-caching=1000 --file-logging --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\rk-userspace-high-vbr12m-v2\vlc.log --no-video-title-show rtp://@192.168.96.122:62882
```

RK 目标机：

```bash
source /opt/mt-tools/mtenv.sh
mtenv on
source /etc/profile.d/ffenv.sh
ffenv on

tcpdump -i eth0 -n -s 0 -w /home/tang/MediaTranscode/out/acceptance/rk-userspace-high-vbr12m-v2/sender.pcap 'dst host 192.168.96.122 and (udp port 62882 or udp port 62883)'

/home/tang/MediaTranscode/out/build/rk-release/media_transcode_realtime_video_cli --media-id rk-userspace-high-hevc2k30-h2641080p25-vbr12m-v2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --progress-timeout-ms 12000 --video-rtp-url rtp://127.0.0.1:61882 --video-rtp-codec hevc --video-rtp-payload-type 98 --video-rtp-clock-rate 90000 --rtp-host 192.168.96.122 --rtp-port 62882 --sdp /home/tang/MediaTranscode/out/acceptance/rk-userspace-high-vbr12m-v2/output.sdp --video-codec h264 --rc vbr --min-bitrate 5000 --bitrate 12000 --max-bitrate 13000 --width 1920 --height 1080 --fps 25 --gop 50 --no-audio

/usr/local/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s-2k-hevc.mp4 -map 0:v:0 -an -c:v copy -bsf:v hevc_mp4toannexb -f rtp -payload_type 98 "rtp://127.0.0.1:61882?rtcpport=61883&pkt_size=1200"
```

CLI 启动后立即启动 FFmpeg，二者之间没有等待、检测或容量探测。精确 PID 为 tcpdump 2638152、CLI 2638153、FFmpeg 2638154；VLC PID 为 11560。

## 结果

- FFmpeg 完整发送 3600 帧、120.00 秒，speed=1x；源结束后 CLI 如实以 `RTP video source clock evidence expired` 终止。
- sender 与 receiver 均捕获 143495 个 Datagram，其中 RTP 143468、RTCP 27；RTP loss/reorder 为 0，逐包 SSRC/sequence 顺序完全一致。
- MPEG-TS continuity skip/drop、TEI 和 invalid AFC 均为 0。
- sender 的 would-block、writable wait、deadline miss、pressure、partial submit 和 ambiguous submit 均为 0；最终 backlog、graph payload 和 reservation 全部归零。
- sender 最大动态 wire rate 为 5668916 B/s；发端 1/5/10/100 ms 最大 IP 字节为 5424/20340/40680/356144 B，低于对应连续服务合同上界 7025/29701/58046/568248 B，无追赶式 burst。
- 接收端 RTP 包序与发端完全一致。接收端 1/5 ms 时间戳聚集而 10/100 ms 合格，结合发端抓包平滑，判定为接收 NIC/抓包时间戳合并，不属于 sender burst。
- VLC 日志确认 MPEG-TS/H.264、1920x1080、D3D11VA 解码并收到首帧；播放期未记录 corrupt、lost、continuity、decoder error、black 或 dropping。唯一 late 10 ms 紧邻 `orphaned video window` 与 `exiting`，发生在验收结束关闭窗口阶段。
- CLI 平均单核 CPU 18.96%、峰值 29.17%；初始 RSS 137744384 B、峰值 143949824 B，最终 payload object 为 0。CPU 按当前明确范围仅记录，不在本轮优化。
- 测试完成后，目标机 CLI、FFmpeg、tcpdump 和本机 VLC 均无进程残留。

结论：RKMPP 公共用户态 Datagram 高质量 VBR 发送与完整 VLC 接收门禁通过。
