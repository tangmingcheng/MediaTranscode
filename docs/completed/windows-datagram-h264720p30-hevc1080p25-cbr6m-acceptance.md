# Windows Datagram 低质量 CBR 门禁完成记录

- 日期：2026-09-01
- 冻结代码：`6464533e`
- 输入：真实连续 120 秒 H.264 1280x720、30 fps、raw RTP。
- 输出：HEVC 1920x1080、25 fps、CBR 6 Mbps、MPEG-TS/RTP。
- 部署事实：受管 egress 50 Mbps，最大 wire residence 100 ms。

## 实际命令

```powershell
D:\Wireshark\dumpcap.exe -i 11 -f "udp port 62140 or udp port 62141" -a duration:140 -w D:\Code\MyCode\MediaTranscode\out\acceptance\windows-loopback-cbr6m-v3\receiver.pcapng

D:\VideoLAN\VLC\vlc.exe --no-one-instance --verbose=2 --stats --network-caching=1000 --file-logging --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\windows-loopback-cbr6m-v3\vlc.log --no-video-title-show rtp://@192.168.96.122:62140

D:\Code\MyCode\MediaTranscode\out\build\x64-release\media_transcode_realtime_video_cli.exe --media-id windows-loopback-h264720p30-hevc1080p25-cbr6m-v3 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:61140 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 192.168.96.122 --rtp-port 62140 --sdp D:\Code\MyCode\MediaTranscode\out\acceptance\windows-loopback-cbr6m-v3\output.sdp --video-codec hevc --rc cbr --bitrate 6000 --width 1920 --height 1080 --fps 25 --gop 50 --no-audio

D:\mabs\local64\bin-video\ffmpeg.exe -hide_banner -nostdin -re -i D:\Code\MyCode\MediaTranscode\out\acceptance\test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 "rtp://127.0.0.1:61140?rtcpport=61141&pkt_size=1200"
```

## 结果

- FFmpeg 完整发送 3600 帧并返回 0；生产 DAG 输出 2998 个 access unit。
- sender 与 Npcap Loopback 均记录 64513 个 datagram，其中 RTP 64486、RTCP 27；RTP loss/reorder 为 0，TS continuity、TEI、invalid AFC 为 0。
- planner 最大 wire rate 为 2006648 B/s，单 datagram burst 为 1356 B。1/5/10/100 ms 最大 IP 字节为 2940/8364/15144/125476 B，均低于对应合同上界 3363/11389/21422/202021 B，无追赶式 burst。
- sender would-block、writable wait、deadline miss、pressure、partial submit、ambiguous submit 均为 0；最终 backlog、graph payload 和 reservation 全部归零。
- VLC 打开 RTP URL 并识别 MPEG-TS/HEVC；用户现场观看确认无卡顿、黑屏、花屏和可感知延迟。VLC 文件日志在并发 D3D11VA 初始化枚举处未记录首帧，按用户现场结论不阻塞本轮 Datagram wire 门禁。
- CLI 平均单核 CPU 22.97%，峰值 RSS 201113600 B；CPU 按当前明确范围仅记录，不在本轮优化。
- 源结束后 CLI 如实以 RTP source-clock evidence expiry 终止，未把有限源失活伪装为成功。
