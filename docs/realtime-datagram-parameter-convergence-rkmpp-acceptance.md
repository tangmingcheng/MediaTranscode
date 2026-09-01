# Realtime Datagram 参数收口 RKMPP 验收

## HEVC 2K30 → H.264 1080p25 VBR 5/12/13 Mbps

- 冻结代码：`f5913278`
- 日期：2026-09-01
- 输入：HEVC 2560×1440、30 fps、真实连续 120 秒源
- 输出：H.264 1920×1080、25 fps、VBR min/target/max = 5/12/13 Mbps、GOP 50、MPEG-TS/RTP
- 部署事实：受管 egress 容量 50 Mbps，最大 wire residence 100 ms

### 实际命令

CLI：

```bash
/home/tang/task5-f5913278/out/build/rk-release/media_transcode_realtime_video_cli --media-id rk-f5913278-high-2k30-hevc-to-1080p25-h264-vbr --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60340 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 192.168.96.122 --rtp-port 61340 --sdp /home/tang/task5-f5913278/out/acceptance/rk-high-pass/output.sdp --video-codec h264 --rc vbr --width 1920 --height 1080 --fps 25 --min-bitrate 5000 --bitrate 12000 --max-bitrate 13000 --gop 50 --no-audio
```

FFmpeg 源流：

```bash
/usr/local/bin/ffmpeg -hide_banner -nostdin -re -i /home/tang/test-continuous-120s-2k-hevc.mp4 -map 0:v:0 -an -c:v copy -bsf:v hevc_mp4toannexb -f rtp -payload_type 96 "rtp://127.0.0.1:60340?rtcpport=60341&pkt_size=1200"
```

VLC 接收：

```powershell
D:\VideoLAN\VLC\vlc.exe --no-one-instance --verbose=2 --stats --network-caching=1000 --file-logging --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\rk-high-pass-f5913278\vlc.log rtp://@192.168.96.122:61340
```

### 结果

- planner 自动选择 `hevc_rkmpp → scale_rkrga → h264_rkmpp`，`zero_copy=true`；编码 readback 为 1920×1080、25 fps、VBR 5/12/13 Mbps、GOP 50。
- 生产 DAG 输出 2993 个 access unit；RTP 145451 包、RTCP 30 包，媒体跨度 119.783343 秒。
- RTP sequence 断点 0，TS continuity 错误 0；RK `eth0` 与 Windows 接收侧均捕获 145481 包，两端捕获丢包均为 0。
- sender 实际最高服务率 5,760,776 B/s；以最大 IP Datagram 1356 B 为 burst 上限做 GCRA，最大 debt 1356 B、超额 0 B，判定无 burst。
- sender `would_block=0`、`deadline_misses=0`、`pressure_failures=0`、`partial_submitted_failures=0`、`ambiguous_submitted_failures=0`。
- VLC 日志确认 `Received first picture`、`Stream buffering done`、H.264 解码器和 1920×1080 D3D11 输出建立；decoder/corrupt/late/lost/black/discontinuity 错误匹配为 0，VLC 正常退出。
- RK CLI 平均单核 CPU 17.928%，峰值 28.426%；RSS 从 55,590,912 B 增至 62,115,840 B，增长 6,524,928 B。CPU 优化按当前范围暂缓。
- 源结束后 CLI 以 `RTP video source clock evidence expired` 终止；最终 `queued=0`、`droppedBuffers=0`、资源 reservation/release 相等，无进程残留。

结论：该 RKMPP 高规格参数收口与 MPEG-TS/RTP Datagram 发送控制门禁通过。
