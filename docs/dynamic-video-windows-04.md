# Windows 动态视频第 4 轮诊断（未通过）

2026-09-09；第九次全量 Release 构建产物 SHA-256：10099F0E54230770ABFEBFA4CDDF263CE4BBB735A78449897503B8BE1AD0B1D0。

## 实际命令
```text
D:\Wireshark\dumpcap.exe -i 10 -q -f "udp portrange 60620-60621 or udp portrange 61620-61625" -w D:\Code\MyCode\MediaTranscode\out\acceptance\dynamic-win-04.pcapng
D:\VideoLAN\VLC\vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\dynamic-win-04-vlc.log rtp://@127.0.0.1:61620
D:\Code\MyCode\MediaTranscode\out\build\x64-release\media_transcode_realtime_video_cli.exe --media-id dynamic-win-04 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60620 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 127.0.0.1 --rtp-port 61620 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-04.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50 --no-audio
D:\mabs\local64\bin-video\ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 "rtp://127.0.0.1:60620?rtcpport=60621&pkt_size=1200"
```

CLI 与源的输出分别重定向至 D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-04-cli.log、dynamic-win-04-source.log。运行中依次输入 list，以及：

    add --output-layout mpegts --output-transport rtp --rtp-host 127.0.0.1 --rtp-port 61622 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-04-add.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50

## 结果与缺陷

- 固定 H.264 1280×720、30 fps、8,013,434 bps 源自然完成 3600 帧、120 秒，FFmpeg 退出 0；输出请求保持 HEVC 1920×1080、25 fps、CBR 6 Mbps、GOP 50、MPEG-TS/RTP。全链路未通过，无编码输出。
- 源预留按 atomic_batch=1、node_pending=1、decoder_internal=1 规划；首包停滞已消除。整个输入期间 graphPayloadPressureFailures=0，最终可观察 currentBytes/currentObjects=0，reservations/releases 均为 10800。
- 初始分支在 11:05:44.817 因分发队列超限退出，45.058 发布 Failed→Retired。解码首帧在 44.806 到达，VideoTimestamp 尚未收到目标编码器 metadata；encoder 到 44.820 才打开。source prepared replay 为 780 datagrams、201 ms arrival span。需修复准备与发布顺序，不能以经验队列扩容掩盖；日志未记录确切 queue capacity/outcome，不推断其数值。
- 11:06:38.351 动态 add 被准入为 output 2，随后因 live source facts 缺失而拒绝。源码发现 VideoOnly 没有 canonical lineage，而 fanout 只从其读取 source generation；本次泛化错误未单独指出缺失字段，后续补逐项诊断并修权威代次契约。
- 初始分支失败后输入继续完整消费，表明未升级为共享 worker 失败，但不代表输出或多输出能力通过。sender committed datagrams 为 0，VLC 无有效输出画面；没有截图。
- 源结束后空读取每约 2 秒增加 workerProgress，使零输出路径的 no-progress 计时不断续期。CLI 未自行退出；完成失败取证后按精确 PID 与可执行路径核对，强制清理残留进程。该行为仅为失败清理，不计正常退出通过。
- PID：CLI 27664、FFmpeg 31660、dumpcap 11328、VLC 31912。停止抓包 106695 包、capture drop 0。已检查无相关进程残留。
- 清理前 CLI 内部 1241 个 CPU 样本（包含源结束后的空读期）：平均单核 11.027424%，峰值 51.886792%；working set 121704448→163106816 B，峰值178921472 B。payload高水位9230724 B、22对象，workerErrors=0。不能把此数据认定为有效输出的性能门禁；视频模式 A/V 漂移不适用。
- 原始数据只存 D 盘，提取结果后删除。不创建成功验收提交。