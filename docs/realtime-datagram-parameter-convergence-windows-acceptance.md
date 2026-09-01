# Windows Realtime Datagram 参数收口验收

## 高质量链路：PASS

- 日期：2026-08-31
- 输入：真实 120 秒 HEVC 2560x1440 30 fps，raw RTP。
- 输出：H.264 1920x1080 25 fps，VBR 5/12/13 Mbps，MPEG-TS/RTP。
- 部署事实：预留 egress 50 Mbps，最大 wire residence 100 ms。

CLI：

```powershell
D:\Code\MyCode\MediaTranscode\out\build\x64-release\media_transcode_realtime_video_cli.exe --media-id windows-review-high-2k30-hevc-to-1080p25-h264-vbr --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60240 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 192.168.96.122 --rtp-port 61240 --sdp D:\Code\MyCode\MediaTranscode\out\acceptance\review-high-passgate-20260831\output.sdp --video-codec h264 --rc vbr --width 1920 --height 1080 --fps 25 --min-bitrate 5000 --bitrate 12000 --max-bitrate 13000 --gop 50 --no-audio
```

FFmpeg 源：

```powershell
D:\mabs\local64\bin-video\ffmpeg.exe -hide_banner -nostdin -re -i D:\Code\MyCode\MediaTranscode\out\acceptance\test-continuous-120s-2k-hevc.mp4 -map 0:v:0 -an -c:v copy -bsf:v hevc_mp4toannexb -f rtp -payload_type 96 "rtp://127.0.0.1:60240?rtcpport=60241&pkt_size=1200"
```

VLC 接收：

```powershell
D:\VideoLAN\VLC\vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\review-high-passgate-20260831\vlc.log rtp://@192.168.96.122:61240
```

结果：

- 抓包媒体跨度 119.968639 秒，RTP 113410 包，抓包丢包 0。
- RTP sequence break 0，reorder 0；MPEG-TS continuity error 0。
- 按 planner 速率 3989698 B/s 与 1356 B burst 逐前缀检查 GBRA/GCRA，最大超额 0 B。
- sender would-block、deadline miss、pressure、partial submit、ambiguous submit 均为 0；提交、发送和最终 sequence 全部为 113442。
- VLC 创建 1920x1080 D3D11 视频输出；black、corrupt、late picture、lost 和 discontinuity 日志均为 0。
- 单核 CPU 均值 26.404%，峰值 44.622%；RSS 增长 950272 B。CPU 按本轮明确范围仅记录，不作为 Datagram 发送控制修改项。
- 源结束后 CLI 以 RTP source-clock evidence expiry 失败退出，符合有限源生命周期语义。
