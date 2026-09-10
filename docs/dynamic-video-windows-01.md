# Windows 动态视频第 1 轮诊断（未通过）

2026-09-09；分支 `feat/dynamic-video-outputs`，第三次全量 Release 构建产物 SHA-256：`00704B5A44CE3FE75DE9C3B1F5C40C1615260245098402A8BB8151330670CEEC`。这是未提交的开发版本，不是完整验收。

## 实际命令

```text
D:\Wireshark\dumpcap.exe -i 10 -f "udp portrange 60620-60621 or udp portrange 61620-61625" -w D:\Code\MyCode\MediaTranscode\out\acceptance\dynamic-win-01.pcapng
D:\VideoLAN\VLC\vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\dynamic-win-01-vlc.log rtp://@127.0.0.1:61620
D:\Code\MyCode\MediaTranscode\out\build\x64-release\media_transcode_realtime_video_cli.exe --media-id dynamic-win-01 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60620 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 127.0.0.1 --rtp-port 61620 --sdp D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-01.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50 --no-audio
D:\mabs\local64\bin-video\ffmpeg.exe -hide_banner -nostdin -re -i D:/Code/MyCode/MediaTranscode/out/acceptance/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 "rtp://127.0.0.1:60620?rtcpport=60621&pkt_size=1200"
```

最终通过 PowerShell 直接调用上述绝对路径；CLI/源命令分别用 `> D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-01-cli.log 2>&1`、`> D:/Code/MyCode/MediaTranscode/out/acceptance/dynamic-win-01-source.log 2>&1` 重定向，并用 `exit $LASTEXITCODE` 传递退出码。首次 cmd 包装的 dumpcap 因引号传递失败退出，随后改用 PowerShell 参数传递正常抓包。

## 结果与根因

- 固定源 H.264 1280×720、30 fps、8,013,434 bps、120 秒；目标 HEVC 1920×1080、25 fps、CBR 6 Mbps、GOP 50、MPEG-TS/RTP。
- 实际选择 `h264` + CUDA NVDEC、`scale_cuda=1920:1080`、`hevc_nvenc`，不是 `h264_cuvid`。解码已产生 CUDA 1280×720 帧。
- 10:10:36.523 初始 CodecResolver 新建的时间元数据缺少 `codec_type=AVMEDIA_TYPE_VIDEO`，包装后为 Unknown，严格通道校验报 `emit failed: stream type mismatch channel=Video buffer=Unknown`。CLI 自行失败退出，状态 Failed → Retired，未发出输出媒体；这是实现缺陷，校验本身正确。
- 初始失败后已提取分支的 wakeup 仍属于源 context，被源失败路径永久 interrupt；branch 尚未收到停止请求，等待立即返回，约 0.25 秒产生数百万次调用。修复方向是明确移交 wakeup 所有权，并处理 Interrupted 终止语义，不添加经验 sleep。
- 最终 workerErrors=1，encodedPacketsPushed/Popped=0，graphPayloadCurrentBytes/Objects=0；CPU 正式采样数为 0，不能声称完整内存、CPU、时钟或画面门禁通过。视频模式 A/V 漂移不适用。
- 源 PID 19572，抓包 PID 5408，VLC PID 8640；CLI 在 PID 采样前已退出，未取得其 PID。源最后日志为 1113 帧、37.10 秒，因生产链路已失败通过 Ctrl-C 停止源；随后停止抓包并关闭本次 VLC。捕获 34032 包，dumpcap 报捕获丢包 0，不代表输出验收通过。
- 不创建成功验收提交。原始日志和抓包只存 D 盘，提取本报告后删除；进程残留核查无本次 CLI/FFmpeg/dumpcap/VLC。
