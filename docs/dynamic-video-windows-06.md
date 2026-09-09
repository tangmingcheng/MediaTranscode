# Windows 动态视频第 6 轮诊断（未通过）

2026-09-09；第十一次全量 Release 构建 SHA-256：E687D1C82F6A3B514A5EA82ADB0F67EBDE439187E9B1EFF01B9DDFD389370A0F。

## 实际命令
```text
D:\Wireshark\dumpcap.exe -i 10 -q -f "udp portrange 60620-60621 or udp portrange 61620-61625" -w D:\Code\MyCode\MediaTranscode\out\acceptance\dynamic-win-06.pcapng
D:\VideoLAN\VLC\vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\dynamic-win-06-vlc.log rtp://@127.0.0.1:61620
D:\Code\MyCode\MediaTranscode\out\build\x64-release\media_transcode_realtime_video_cli.exe --media-id dynamic-win-06 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60620 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 127.0.0.1 --rtp-port 61620 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-06.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50 --no-audio
D:\mabs\local64\bin-video\ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 "rtp://127.0.0.1:60620?rtcpport=60621&pkt_size=1200"
```

CLI/source分别重定向到D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-06-cli.log和dynamic-win-06-source.log。新增接收与控制：

    D:/VideoLAN/VLC/vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-06-vlc-add.log rtp://@127.0.0.1:61622
    add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61622 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-06-add.sdp --video-codec h264 --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50

## 结果

- 固定H.264 1280×720、30fps、8,013,434bps源自然完成3600帧120秒，FFmpeg退出0；首路HEVC 1920×1080、25fps、CBR6Mbps、GOP50、MPEG-TS/RTP持续运行。源码结束后CLI按既有no-progress失败收敛，退出1。
- output1在12:14:21.666进入Running。output2在12:15:43.393进入Preparing，43.688因invalid complete SDP session identity拒绝，初始输出未被中断。
- 根因是typed SDP identity factory错用token语法校验origin username。RFC8866的username为non-ws-string，冒号合法；输出唯一身份拼接产生合法冒号后被本仓错误拒绝。修复采用username专属语法，而非更换分隔符或伪造hash身份。依据：https://www.rfc-editor.org/rfc/rfc8866.html#section-9。
- sender提交64680 datagrams（64648 RTP、32 RTCP）；deadline/pressure/partial/ambiguous/WouldBlock均0，最大residence60.9936ms。未对本轮抓包完成完整服务曲线/画面门禁，不能记多输出PASS。
- 最终workerErrors/drop/payloadPressure=0，payload currentBytes/currentObjects=0，reservations/releases均22812；payload高水位15172179B、26对象。CPU435个内部样本：平均单核17.851441%、峰值54.246575%；working set120885248→198283264B，峰值224706560B。VideoOnly的A/V漂移不适用。
- PID：CLI30996、FFmpeg29036、dumpcap13912、VLC29656/16820。抓包171375包、drop0；源自然结束后停止抓包并关闭VLC，无相关进程残留。
- 本轮原始日志、抓包、SDP只存D盘，提取结果后删除；不创建成功验收提交。