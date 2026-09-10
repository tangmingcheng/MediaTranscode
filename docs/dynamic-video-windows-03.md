# Windows 动态视频第 3 轮诊断（未通过）

2026-09-09；第七次全量 Release 构建产物 SHA-256：6686B5A6E7CA2F0D35A46AD102A388561FDBC44A8365FB28C1DFF1058B876CB3。未完整验收。

## 实际命令
```text
D:\Wireshark\dumpcap.exe -i 10 -q -f "udp portrange 60620-60621 or udp portrange 61620-61625" -w D:\Code\MyCode\MediaTranscode\out\acceptance\dynamic-win-03.pcapng
D:\VideoLAN\VLC\vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\dynamic-win-03-vlc.log rtp://@127.0.0.1:61620
D:\Code\MyCode\MediaTranscode\out\build\x64-release\media_transcode_realtime_video_cli.exe --media-id dynamic-win-03 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60620 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 127.0.0.1 --rtp-port 61620 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-03.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50 --no-audio
D:\mabs\local64\bin-video\ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 "rtp://127.0.0.1:60620?rtcpport=60621&pkt_size=1200"
```

CLI 与源通过 PowerShell 直接调用绝对路径，分别重定向到 D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-03-cli.log 和 dynamic-win-03-source.log，以 exit LASTEXITCODE 传递原生退出码。

## 结果

- 固定 H.264 1280×720、30 fps、8,013,434 bps、120 秒源；输出请求 HEVC 1920×1080、25 fps、CBR 6 Mbps、GOP 50，MPEG-TS/RTP。未降低规格。
- 初始分区成功：总量 38,611,732 B，源载荷 8,750,000 B，输出载荷 24,883,200 B，独立存储 4,978,532 B。源单次最大预留等于源账户总配额。
- 10:41:34.489 起等待首个随机访问单元。首包占用 33,389 B 后，载荷压力次数为 1；队列清空、workerProgress 固定为 45，无法继续输入。10:41:40.278 因 no-progress timeout 退出，无编码输出。需同时核查压缩包真实缓冲所有权及原子预留与在途持有预算，不得以经验倍数掩盖。
- CLI PID 27788，FFmpeg PID 16536，dumpcap PID 19492，VLC PID 11940。CLI 自行失败后才停止源；源最后记录 728 帧、24.23 秒。本轮不是完整 120 秒验收。
- CLI 内部 19 个 CPU 样本：平均单核占用 11.827797%，峰值 45.727483%；working set 从 121,528,320 B 至 237,158,400 B。workerErrors=0，droppedBuffers=0，encodedPackets=0。视频模式 A/V 漂移不适用；不能据此认定持续运行内存或时钟门禁通过。
- 停止时抓包 22,567 包、capture drop 0；抓包数量不代表输出通过。已停止本次源、抓包和 VLC。提取失败证据后删除原始材料，不创建成功验收提交。
补充退役证据：最终报告 reservations 与 releases 均为 10，但 currentBytes 为 8,750,000、currentObjects 为 1。源码显示停止期间释放首包可能向已停止 worker 的 waiter 发放预留，而待申请取消只发生在后续 context.reset；需将终止时的待申请撤销纳入正式生命周期，不能把它当作实际活动 payload 泄漏或提前释放真实 lease。
