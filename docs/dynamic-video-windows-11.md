# Windows 动态视频第 11 轮诊断（未通过）

2026-09-09；第十六次全量 Release 构建 SHA-256：D44F8010F390E21641DE73FFE01C24F9C20D3944F2F8BE66734A6DAA67F4625F。

## 实际命令
```text
D:\Wireshark\dumpcap.exe -i 10 -q -f "udp portrange 60620-60621 or udp portrange 61620-61625" -w D:\Code\MyCode\MediaTranscode\out\acceptance\dynamic-win-11.pcapng
D:\VideoLAN\VLC\vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\dynamic-win-11-vlc.log rtp://@127.0.0.1:61620
D:\Code\MyCode\MediaTranscode\out\build\x64-release\media_transcode_realtime_video_cli.exe --media-id dynamic-win-11 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60620 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 127.0.0.1 --rtp-port 61620 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-11.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50 --no-audio
D:\mabs\local64\bin-video\ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 "rtp://127.0.0.1:60620?rtcpport=60621&pkt_size=1200"
```

CLI/source分别重定向到D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-11-cli.log和dynamic-win-11-source.log。新增接收与控制：

    D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-11-vlc-add.log rtp://@127.0.0.1:61622
    add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61622 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-11-add.sdp --video-codec h264 --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50

后续 CLI 控制依次执行：

    remove 1
    remove 2
    add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61624 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-11-readd.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50

第三路接收：

    D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-11-vlc-readd.log rtp://@127.0.0.1:61624


抓包分析：

    D:/Wireshark/tshark.exe -r D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-11.pcapng -d udp.port==61620,rtp -d udp.port==61622,rtp -d udp.port==61624,rtp -q -z rtp,streams

## 结果

- 固定 H.264 1280×720、30fps、8,013,434bps 连续源自然完成 3600 帧、120 秒，FFmpeg 退出 0。初始 HEVC、新增 H.264、重新新增 HEVC 均为 1920×1080、25fps、CBR 6Mbps、GOP50、MPEG-TS/RTP。
- output1 在 13:10:09.081 Running；output2 在 13:10:19.283 Preparing、19.874 Running。
- remove1 在 13:10:39.887 Draining，41.064 因 Datagram transmit session is a non-migrating single-owner object 失败并退休；remove2 在 13:11:13.496 Draining，13.814 同类失败并退休。两路发送端全部媒体和终止 RTCP 已提交，backlog 均 0；错误位于随后 controller 调用 sender.stop 的线程归属。正确修复须在执行线程关闭会话后才发布退出，不能放宽 owner guard。
- 零输出后第三路 13:11:32.639 Preparing，33.248 Running，持续到源结束；CLI 随真实输入无进展自然退出 1。截图工具 CopyFromScreen 报 The handle is invalid，本轮无成功画面截图证据。
- 捕获 170251 包、drop 0，覆盖源和 CLI 结束。61620/61622/61624 的 RTP 分别 17074/27265/19184 包，均丢失 0；删除仍报错，不记录完整 PASS。
- 最终 payload currentBytes/currentObjects=0，reservations/releases 均 34590，高水位 15186197B、29 对象；drop/payloadPressure=0。CPU 429 个样本平均单核 18.468590%、峰值 65.835411%；working set 121163776→202362880B，峰值 236171264B。workerErrors=0 不代表释放阶段无错误，以逐输出错误为准；VideoOnly 的 A/V 漂移不适用。
- PID：CLI 16816、源 17512、dumpcap 32292、VLC 30920/30624/12944。源及 CLI 自然结束后停止抓包，关闭本轮 VLC，原始日志、抓包与 SDP 仅存 D 盘、提取后删除。不创建成功验收提交。