# 动态多输出前的 Windows 单路基线

日期：2026-09-09。源码基线：`b90530dd`。结论：**未通过完整验收**；尚未验证动态多输出，不进入 RKMPP。

输入 H.264 1280×720@30、约 8.01 Mbps、120 秒 RTP；输出 HEVC 1920×1080@25、CBR 6 Mbps、GOP 50、MPEG-TS over RTP。实际选择 CUDA/NVENC，VLC 使用 D3D11。出口配置沿用本机验收 50 Mbps、最大 wire residence 100 ms。

## 实际执行命令

```powershell
D:\Code\MyCode\MediaTranscode\out\build\x64-release\media_transcode_realtime_video_cli.exe --media-id dynamic-video-win-baseline --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --video-rtp-url rtp://127.0.0.1:60620 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 127.0.0.1 --rtp-port 61620 --sdp D:\Code\MyCode\MediaTranscode\out\acceptance\dynamic-video-win-baseline.sdp --video-codec hevc --rc cbr --width 1920 --height 1080 --fps 25 --bitrate 6000 --gop 50 --no-audio
D:\mabs\local64\bin-video\ffmpeg.exe -hide_banner -nostdin -re -i D:\Code\MyCode\MediaTranscode\out\acceptance\test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 "rtp://127.0.0.1:60620?rtcpport=60621&pkt_size=1200"
D:\VideoLAN\VLC\vlc.exe --no-one-instance --verbose=2 --network-caching=1000 --file-logging --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\dynamic-video-win-baseline-vlc.log rtp://@127.0.0.1:61620
```

CLI 和 FFmpeg 通过 PowerShell `> ...-cli.log 2>&1`、`> ...-source.log 2>&1` 重定向至 `D:\Code\MyCode\MediaTranscode\out\acceptance\`。PID：CLI 8376、FFmpeg 7780、VLC 18604。源自然结束，CLI 自行退出，之后用 `CloseMainWindow()` 关闭本次 VLC。

## 结果与限制

- FFmpeg 日志结束于 3600 帧、120.00 秒、约 1x；源工具会话返回 1，不能据此声称 native FFmpeg 退出码为 0。PowerShell 合并 stderr 的包装退出结果与媒体发送完成分别记录。
- CLI 工具会话退出 1，最终错误 `NotInitialized: realtime runtime made no progress before timeout`。不能把该错误描述为 source-clock expiry。
- 输出 mux 记录 2998 个 access unit；sender 提交 64685 个 datagram。would-block、deadline miss、pressure failure、partial/ambiguous submit 均为 0；最大 submit lateness 100486800 ns，最大 backlog residence 79608300 ns。
- 最终 workerErrors=0、droppedBuffers=0、queued=0；但 stalledIntervals=1，最终报告采集时仍有 25287 B、4 个 payload 对象。该报告在 runtime reset 前采集，尚不能据此断言泄漏或最终资源全部回收。
- 平均单核 CPU 19.039362%，峰值 82.823529%；初始 working set 124121088 B，最终 196444160 B，峰值 197660672 B。未证明多轮增删后的内存趋势。
- VLC 日志显示 1920×1080、D3D11、`Stream buffering done`；截图被资源管理器窗口遮挡，不能算完整画面观察通过。
- 没有完整抓包，sender 标记 `delivery_evidence=not_proven`，不能宣称线速包序、TS continuity 或交付时刻门禁通过。视频模式 A/V 漂移不适用；本次没有完整独立视频时钟偏差结论。
- 原始日志、SDP、截图仅放 D 盘，提取以上结果后删除；不提交原始产物。该项不得创建成功验收提交。
