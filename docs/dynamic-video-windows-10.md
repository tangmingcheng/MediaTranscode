# Windows 动态视频第 10 轮诊断（未通过）

2026-09-09；第十五次全量 Release 构建 SHA-256：E95F084EB75E723A2462AA4E89ACAF54239463C419CCDC8394AEEFC1BA14B6B9。

## 实际命令
```text
D:\Wireshark\dumpcap.exe -i 10 -q -f "udp portrange 60620-60621 or udp portrange 61620-61625" -w D:\Code\MyCode\MediaTranscode\out\acceptance\dynamic-win-10.pcapng
D:\VideoLAN\VLC\vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\dynamic-win-10-vlc.log rtp://@127.0.0.1:61620
D:\Code\MyCode\MediaTranscode\out\build\x64-release\media_transcode_realtime_video_cli.exe --media-id dynamic-win-10 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60620 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 127.0.0.1 --rtp-port 61620 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-10.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50 --no-audio
D:\mabs\local64\bin-video\ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 "rtp://127.0.0.1:60620?rtcpport=60621&pkt_size=1200"
```

CLI/source分别重定向到D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-10-cli.log和dynamic-win-10-source.log。新增接收与控制：

    D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-10-vlc-add.log rtp://@127.0.0.1:61622
    add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61622 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-10-add.sdp --video-codec h264 --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50

后续 CLI 控制依次执行：

    remove 1
    remove 2
    add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61624 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-10-readd.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50

第三路接收：

    D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-10-vlc-readd.log rtp://@127.0.0.1:61624


## 结果

- 固定 H.264 1280×720、30fps、8,013,434bps 连续源自然完成 3600 帧、120 秒，FFmpeg 退出 0。初始 HEVC、新增 H.264、重新新增 HEVC 均为 1920×1080、25fps、CBR 6Mbps、GOP50、MPEG-TS/RTP。
- output1 在 12:58:35.303 Running；output2 在 12:58:45.473 Preparing，46.062 Running。
- remove1 在 12:59:08.769 Draining；09.634 MPEG-TS datagram materializer requires a protocol batch，09.705 退休。remove2 在 12:59:32.737 Draining，33.065 同类失败并退休。当前 EOS 已真实沿媒体链路传递，物化节点缺少终止状态分支；没有再触发旧网络排空期限误用。
- 零输出后第三路 12:59:51.052 Preparing，51.365 等待 RAP，51.670 Running；61624 的 VLC 游戏画面已截图确认。源仍有数据，第三路持续到源结束，补充验证了输入消费和新增恢复。
- 三路 sender 分别提交 18518、23662、23377 datagrams；网络 deadline/pressure/partial/ambiguous/WouldBlock 均 0。前两路失败时分别有 25/20 个未完成报文，第三路最终 backlog 0；删除未完整排空，不记录 PASS。
- 最终 workerErrors/errors=2，drop/payloadPressure=0；payload currentBytes/currentObjects=0，reservations/releases 均 34897，高水位 15191317B、31 对象。CPU 421 个内部样本平均单核 20.483751%、峰值 53.012048%；working set 121262080→203137024B，峰值 236658688B。包含不同输出数，不作为单一吞吐基准；VideoOnly 的 A/V 漂移不适用。
- PID：CLI 33548、源 2508、dumpcap 31380、VLC 18168/7796/30976。捕获 165875 包、drop 0；诊断抓包在源约 115 秒时停止，未覆盖完整源尾，不能作为完整抓包验收。CLI 在源结束后按真实输入无进展自然退出 1。
- 原始日志、抓包、截图和 SDP 仅存 D 盘，提取报告后删除并关闭本轮 VLC；不创建成功验收提交。