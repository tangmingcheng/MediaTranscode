# Windows 动态视频第 7 轮诊断（未通过）

2026-09-09；第十二次全量 Release 构建 SHA-256：9A08DE0A6C25840BD2CC2D8A50FD575A0F25E96A7CC7E0F2DED2449F56B4D356。

## 实际命令
```text
D:\Wireshark\dumpcap.exe -i 10 -q -f "udp portrange 60620-60621 or udp portrange 61620-61625" -w D:\Code\MyCode\MediaTranscode\out\acceptance\dynamic-win-07.pcapng
D:\VideoLAN\VLC\vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\dynamic-win-07-vlc.log rtp://@127.0.0.1:61620
D:\Code\MyCode\MediaTranscode\out\build\x64-release\media_transcode_realtime_video_cli.exe --media-id dynamic-win-07 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60620 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 127.0.0.1 --rtp-port 61620 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-07.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50 --no-audio
D:\mabs\local64\bin-video\ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 "rtp://127.0.0.1:60620?rtcpport=60621&pkt_size=1200"
```

CLI/source分别重定向到D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-07-cli.log和dynamic-win-07-source.log。新增接收与控制：

    D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-07-vlc-add.log rtp://@127.0.0.1:61622
    add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61622 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-07-add.sdp --video-codec h264 --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50

## 结果

- 固定 H.264 1280×720、30fps、8,013,434bps 连续源；初始 HEVC 1920×1080、25fps、CBR 6Mbps、GOP50、MPEG-TS/RTP。新增 H.264 输出保持 1920×1080、25fps、CBR 6Mbps、GOP50。
- output1 在 12:29:58.834 Running；output2 在 12:30:08.942 Preparing，09.249 因 VideoOnly scheduler rejects duplicate scheduling authority 失败并退休。初始输出继续运行。新增已越过 SDP 校验，当前失败位于 DAG 构建。
- 本轮明确判定失败后向源进程发送 Ctrl+C；末条源进度 1299 帧、43.30 秒。CLI 随后因真实输入无进展超时自然退出 1，没有停止 CLI 绕过收敛。
- sender 提交 23707 datagrams（23696 RTP、11 RTCP）；deadline/pressure/partial/ambiguous/WouldBlock 均 0，最大 residence 58.2292ms；未完成画面和完整抓包验收门禁，不记录 PASS。
- 最终 workerErrors/drop/payloadPressure=0，payload currentBytes/currentObjects=0，reservations/releases 均 8341，高水位 15152065B、27 对象。CPU 169 个内部样本：平均单核 17.983193%、峰值 68.095238%；working set 117284864→196763648B，峰值 222314496B。VideoOnly 的 A/V 漂移不适用。
- PID：CLI 31808、FFmpeg 16540、dumpcap 31612、VLC 24936/9540。捕获 62964 包、drop 0。停止抓包、关闭本轮 VLC 并检查进程残留。
- 原始日志、抓包与 SDP 仅存 D 盘，提取本报告后删除；本轮不创建成功验收提交。