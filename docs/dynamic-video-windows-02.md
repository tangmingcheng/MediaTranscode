# Windows 动态视频第 2 轮诊断（未通过）

2026-09-09；第六次全量 Release 构建产物 SHA-256：`B1EB10A478923D19FBD56B1C782D42528823DA995A9BF03BCC2941EDE86BD59D`。同一开发分支未提交版本，未完整验收。

## 实际命令
```text
D:\Wireshark\dumpcap.exe -i 10 -q -f "udp portrange 60620-60621 or udp portrange 61620-61625" -w D:\Code\MyCode\MediaTranscode\out\acceptance\dynamic-win-02.pcapng
D:\VideoLAN\VLC\vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\dynamic-win-02-vlc.log rtp://@127.0.0.1:61620
D:\Code\MyCode\MediaTranscode\out\build\x64-release\media_transcode_realtime_video_cli.exe --media-id dynamic-win-02 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60620 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 127.0.0.1 --rtp-port 61620 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-02.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50 --no-audio
D:\mabs\local64\bin-video\ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 "rtp://127.0.0.1:60620?rtcpport=60621&pkt_size=1200"
```
CLI 与源通过 PowerShell 直接调用绝对路径，分别重定向到 D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-02-cli.log 和 dynamic-win-02-source.log，并以 `exit $LASTEXITCODE` 传递结果。

## 结果

- 源保持固定 H.264 1280×720、30 fps、8,013,434 bps、120 秒；输出请求仍为 HEVC 1920×1080、25 fps、CBR 6 Mbps、GOP 50、MPEG-TS/RTP，planner 选择 CUDA NVDEC/scale_cuda/NVENC。
- 10:22:13.985 在初始分区准入阶段拒绝：`initial producer residence partition exceeds session memory budget: required=1161281332 available=51053332`。worker 尚未启动，没有输出媒体；初始输出 Failed → Retired，CLI 退出 1。
- 新算法将最大单包载荷乘以全部可达引用槽，过度估计分区需求；旧 51,053,332 B 是单链路内部规划值，并非用户部署的物理内存限制。修复应分别使用源原子分配配额、prepared encoder emission/residence 配额及共享帧引用窗口，不能删除硬边界或降低输入规格。
- CLI PID 8716，FFmpeg PID 9368，dumpcap PID 27024，VLC PID 12096。启动前 CLI working set 13,381,632 B；没有运行期正式 CPU、内存趋势或输出时钟样本，不能宣称相关门禁通过。视频模式 A/V 漂移不适用。
- CLI 自行退出后停止源；源最后记录 881 帧、29.36 秒。随后停止抓包和关闭本次 VLC；抓包 27166 包、capture drop 0，不代表输出交付通过。无本次相关进程残留。
- 原始材料仅存 D 盘，提取原因后删除。不创建成功验收提交。
