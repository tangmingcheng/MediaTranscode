# Windows 动态视频第 9 轮诊断（未通过）

2026-09-09；第十四次全量 Release 构建 SHA-256：B36DEC781E82364B45F61E6E3DB666A1073C086430D8253185961AB7A07822DA。

## 实际命令
```text
D:\Wireshark\dumpcap.exe -i 10 -q -f "udp portrange 60620-60621 or udp portrange 61620-61625" -w D:\Code\MyCode\MediaTranscode\out\acceptance\dynamic-win-09.pcapng
D:\VideoLAN\VLC\vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\dynamic-win-09-vlc.log rtp://@127.0.0.1:61620
D:\Code\MyCode\MediaTranscode\out\build\x64-release\media_transcode_realtime_video_cli.exe --media-id dynamic-win-09 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60620 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 127.0.0.1 --rtp-port 61620 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-09.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50 --no-audio
D:\mabs\local64\bin-video\ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 "rtp://127.0.0.1:60620?rtcpport=60621&pkt_size=1200"
```

CLI/source分别重定向到D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-09-cli.log和dynamic-win-09-source.log。新增接收与控制：

    D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-09-vlc-add.log rtp://@127.0.0.1:61622
    add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61622 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-09-add.sdp --video-codec h264 --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50

后续 CLI 控制依次执行：

    remove 1
    remove 2
    add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61624 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-09-readd.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50

第三路接收：

    D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-09-vlc-readd.log rtp://@127.0.0.1:61624

抓包分析：

    D:/Wireshark/tshark.exe -r D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-09.pcapng -d udp.port==61620,rtp -d udp.port==61622,rtp -q -z rtp,streams
    D:/Wireshark/tshark.exe -r D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-09.pcapng -d udp.port==61620,rtp -d udp.port==61622,rtp -q -z expert,error

## 结果

- 固定 H.264 1280×720、30fps、8,013,434bps 连续源自然完成 3600 帧、120 秒，FFmpeg 退出 0。初始 HEVC、新增 H.264 均为 1920×1080、25fps、CBR 6Mbps、GOP50、MPEG-TS/RTP。
- output1 在 12:44:19.983 Running；output2 在 12:44:30.073 Preparing、30.366 WaitingForRandomAccess、30.660 Running。已观察并截图确认 61622 的 VLC 游戏画面。
- remove1：12:44:58.296 Draining，58.595 排空期限失败，58.920 实际退休；output2 继续运行。remove2：12:45:22.894 Draining，23.191 同类期限失败，23.533 实际退休。零输出期间输入继续消费。
- 根因证据：beginDrain 当时只设置状态和 EOS 引用，实际 EOS 等下一轮 controller poll 才投递，而 poll 先判过期；另全分支期限误用了网络 maximumDrainResidence，不能证明滤镜/编码器的服务上界。修复应按 FIFO 终止与真实无进展证据形成分支契约，不能经验扩大网络期限。
- 第三路 add 因人工工具操作延迟到源 EOF 后的 12:46:20.749 才被接纳；21.063 等待 RAP，23.985 因真实输入无进展失败，24.271 退休；不计为有效的零输出后恢复验证。CLI 自然退出 1。
- 抓包 153422 包、drop 0；输出 61620 的 RTP 20850 包、丢失 0，61622 的 RTP 25853 包、丢失 0，无 expert error。两路 sender 分别提交 20860 / 25867 datagrams，网络 deadline/pressure/partial/ambiguous/WouldBlock 均 0；删除失败各留下 1 个 228B wire 报文。没有完整通过排空门禁，不记录 PASS。
- 最终 payload currentBytes/currentObjects=0，reservations/releases 均 26822，高水位 15166515B、30 对象；drop/payloadPressure=0。CPU 417 个内部样本平均单核 20.206832%、峰值 58.595642%；working set 119345152→189018112B，峰值 237649920B。期间含双输出和零输出，不能作为单一转码吞吐基准；VideoOnly 的 A/V 漂移不适用。
- PID：CLI 15216、源 30304、dumpcap 6044、VLC 13224/31648/30912。停止抓包并关闭本轮 VLC；原始日志、抓包、截图和 SDP 均仅存 D 盘，提取后删除。失败项不创建成功验收提交。