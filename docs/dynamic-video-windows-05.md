# Windows 动态视频第 5 轮诊断（多输出未通过）

2026-09-09；第十次全量 Release 构建 SHA-256：F4CB2A2A7D3EB3FED333261481D7DF646F376B901E941D8AD8ACF2E6B23F256C。

## 实际命令
```text
D:\Wireshark\dumpcap.exe -i 10 -q -f "udp portrange 60620-60621 or udp portrange 61620-61625" -w D:\Code\MyCode\MediaTranscode\out\acceptance\dynamic-win-05.pcapng
D:\VideoLAN\VLC\vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\dynamic-win-05-vlc.log rtp://@127.0.0.1:61620
D:\Code\MyCode\MediaTranscode\out\build\x64-release\media_transcode_realtime_video_cli.exe --media-id dynamic-win-05 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60620 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 127.0.0.1 --rtp-port 61620 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-05.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50 --no-audio
D:\mabs\local64\bin-video\ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 "rtp://127.0.0.1:60620?rtcpport=60621&pkt_size=1200"
```

CLI/source 日志重定向到 D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-05-cli.log、dynamic-win-05-source.log。

新增接收端与控制命令：

    D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-05-vlc-add.log rtp://@127.0.0.1:61622
    add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61622 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-05-add.sdp --video-codec h264 --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50

抓包核查使用：

    D:/Wireshark/tshark.exe -r D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-05.pcapng -d udp.port==61620,rtp -q -z rtp,streams
    D:/Wireshark/tshark.exe -r D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-05.pcapng -d udp.port==61620,rtp -q -z expert,error
    D:/Wireshark/tshark.exe -r D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-05.pcapng -d udp.port==61620,rtp -Y "udp.dstport == 61620 && (mp2t.analysis.drops || mp2t.cc.drop || mp2t.msg.fragment.error)" -T fields -e frame.number
    D:/Wireshark/tshark.exe -r D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-05.pcapng -d udp.port==61620,rtp -Y "udp.dstport == 61620 && (mp2t.tei != 0 || mp2t.afc.invalid)" -T fields -e frame.number

## 结果

- 固定 H.264 1280×720、30 fps、8,013,434 bps 源自然完成 3600 帧、120 秒，FFmpeg 退出 0。首路 HEVC 1920×1080、25 fps、CBR 6 Mbps、GOP 50、MPEG-TS/RTP 在 11:24:52.249 进入 Running 并持续输出，VLC 使用 D3D11 解码且画面可见。
- 11:25:32.797 新增 H.264 同尺寸/帧率/码率分支进入 Preparing，33.077 因源 metadata 事实不足而 Failed→Retired，未影响首路。Raw RTP snapshot 只提供 codec/extradata/timebase，新增准备错误地强求尺寸/FPS；修复应复用初始 preparedVideoSource，不能新增默认 FPS。本轮没有完成动态增删或编码共享，整体不记 PASS。
- 首路 sender 提交 64681 个 datagrams：64652 RTP、29 RTCP。抓包 RTP loss=0；TS dropped/fragment/TEI/AFC 错误过滤无匹配，expert error 无记录。capture 总171376包、drop0。未完成独立服务曲线与所有时钟门禁，不把单路补充证据写成完整多输出通过。
- sender deadline/pressure/partial/ambiguous/would-block 均0，backlog峰值48包、65088B、最大residence59.247ms。TX timestamp 未跟踪，delivery_evidence=not_proven，不能声称硬件wire completion已验证。
- 最终 workerErrors/drop/payloadPressure 均0，payload currentBytes/currentObjects=0，reservations/releases均22812；payload高水位15166745 B、28对象。encodedPacketsPushed/Popped均8994是多边累计，不能当作8994个独立AU。
- CPU 433个内部样本，平均单核17.000888%、峰值58.313253%；working set 120438784→199081984 B。120秒不足以认定长期无增长。VideoOnly的A/V漂移不适用。
- 源自然结束后CLI按既有no-progress失败收敛，退出1；没有强制停止CLI。PID：CLI33308、源604、dumpcap30072、初始VLC28024、新增VLC32016。两VLC与抓包均关闭，检查无本轮进程残留。
- 原始截图、抓包、日志及SDP仅存D盘；提取结果后删除。不创建多输出成功提交。