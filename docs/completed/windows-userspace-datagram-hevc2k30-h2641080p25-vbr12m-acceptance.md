# Windows 公共用户态 Datagram 发送控制高质量 VBR 验收

- 日期：2026-09-01
- 输入：真实连续 120 秒 HEVC 2560x1440、30 fps、raw RTP。
- 输出：H.264 1920x1080、25 fps、VBR 5/12/13 Mbps、MPEG-TS/RTP。
- 部署事实：受管 egress 50 Mbps，最大 wire residence 100 ms。

## 实际命令

```powershell
D:\Wireshark\dumpcap.exe -i 11 -f "udp port 62580 or udp port 62581" -a duration:180 -w D:\Code\MyCode\MediaTranscode\out\acceptance\windows-userspace-high-vbr12m-v2\receiver.pcapng

D:\VideoLAN\VLC\vlc.exe --no-one-instance --verbose=2 --stats --network-caching=1000 --file-logging --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\windows-userspace-high-vbr12m-v2\vlc.log --no-video-title-show rtp://@192.168.96.122:62580

D:\Code\MyCode\MediaTranscode\out\build\x64-release\media_transcode_realtime_video_cli.exe --media-id windows-userspace-high-hevc2k30-h2641080p25-vbr12m-v2 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61580 --video-rtp-codec hevc --video-rtp-payload-type 98 --video-rtp-clock-rate 90000 --rtp-host 192.168.96.122 --rtp-port 62580 --sdp D:\Code\MyCode\MediaTranscode\out\acceptance\windows-userspace-high-vbr12m-v2\output.sdp --video-codec h264 --rc vbr --min-bitrate 5000 --bitrate 12000 --max-bitrate 13000 --width 1920 --height 1080 --fps 25 --gop 50 --no-audio

D:\mabs\local64\bin-video\ffmpeg.exe -hide_banner -nostdin -re -i D:\Code\MyCode\MediaTranscode\out\acceptance\test-continuous-120s-2k-hevc.mp4 -map 0:v:0 -an -c:v copy -bsf:v hevc_mp4toannexb -f rtp -payload_type 98 "rtp://127.0.0.1:61580?rtcpport=61581&pkt_size=1200"
```

CLI 启动后立即启动 FFmpeg，二者之间没有等待、检测或容量探测。

## 结果

- FFmpeg 完整发送 3600 帧、120.00 秒并正常退出；源结束后 CLI 如实以 `RTP video source clock evidence expired` 终止。
- receiver 抓包共 113440 个 Datagram：RTP 113410、RTCP 30；RTP loss/reorder 为 0。
- MPEG-TS continuity skip/drop、TEI 和 invalid AFC 均为 0。
- sender 的 would-block、writable wait、deadline miss、pressure、partial submit 和 ambiguous submit 均为 0；最终 backlog、graph payload 和 reservation 全部归零。
- sender 最大动态 wire rate 为 3989698 B/s；1/5/10/100 ms 最大 IP 字节为 4068/13788/27120/172144 B，低于对应服务合同上界 5346/21305/41253/400326 B，无追赶式 burst。
- VLC 日志确认 MPEG-TS/H.264、1920x1080 和 D3D11 解码；未记录 corrupt、lost、discontinuity、decoder error、black、late picture 或 dropping frame。
- CLI 累计 CPU 34.33 秒，约为单核 27.7%；峰值 RSS 230187008 B。CPU 按当前明确范围仅记录，不在本轮优化。

结论：Windows 公共用户态 Datagram 高质量 VBR 发送与完整 VLC 接收门禁通过。
