# RKMPP 输入丢包恢复 run39：完整门禁 FAIL

链路：现有 H.264 2560×1440@30 高规格有限源 → RTP H.264 → RKMPP 解码 / RGA 缩放 / RKMPP HEVC 编码 → MPEG-TS/RTP 1920×1080@25、CBR 6 Mbit/s → 默认硬件 VLC。CLI SHA-256 `417564e0ecd21cc0f0a078473252267826320be671f004193a444b44583efbe68`。

## 实际命令

```bash
dir=/home/tang/MediaTranscode/out/acceptance/rk-a559-loss20-run39
/home/tang/MediaTranscode/out/build/rk-release/media_transcode_realtime_video_cli --media-id rk-userspace-low-h264720p30-hevc1080p25-cbr6m-v1 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --progress-timeout-ms 12000 --video-rtp-url rtp://192.168.130.229:61884 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 192.168.96.122 --rtp-port 6200 --sdp "$dir/output.sdp" --video-codec hevc --rc cbr --bitrate 6000 --width 1920 --height 1080 --fps 25 --gop 50 --no-audio
ffmpeg -hide_banner -nostdin -re -i /home/tang/rk-highspec-hw-hevc2k30-to-h2642k30-248s.mp4 -map 0:v:0 -an -c:v copy -f rtp -payload_type 96 -ssrc 3 -sdp_file "$dir/input.sdp" "rtp://192.168.130.229:61884?pkt_size=1400"
```

```powershell
D:\VideoLAN\VLC\vlc.exe --file-logging --log-verbose=2 --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-loss20-run39\vlc.log rtp://@192.168.96.122:6200
```

CLI / 源 PID 3946911 / 3946934，输入/输出抓包 3946892 / 3946893，执行/注入脚本 3946863 / 3946864，VLC / Windows 抓包 16868 / 29428。脚本完整内容保留在同名证据目录，临时执行文件已删除，所有进程已退出。

## 结果与逐项根因

- 源 15:06:11.022 启动，15:10:20.500 自然结束，exit 0；源本身长度 248.266667 秒。15:06:31.456 注入 lo 20% 丢包，15:07:01.471 撤销；实际缺包 6,384，20.1413%。注入前和撤销后均无输入缺包，撤销后的完整输入持续 197.36791 秒；socket drops 全部采样为 0。
- 15:06:44.244 输入活跃但无可编码帧，CLI 等待；15:07:02.622 完整关键帧到达，15:07:02.787 恢复编码，同一会话一直运行至源结束。workerErrors=0、errors=0、payload pressure failures=0；平均单核 CPU 13.829055%，峰值 RSS 57,761,792 B。源结束后按原 12 秒窗口退出，CLI exit 1 是输入停止后的进展超时，不是运行中失败。A/V 漂移不适用。
- 原 RGA 旧帧窗口失败和 PCR API 跨度失败未再出现。输出画面 5,344 帧，PTS 跨度 248.2 秒；30 秒丢包段等待可解码输入，未伪造或补齐丢失画面。
- **仍有额外画面缺口**：15:07:30.685 核心报告 `first_missing=21886 resumed=21887`，15:07:32.622 再等到完整关键帧。抓包中这一段实际顺序为 21884、21885、21887、21888、21886、21889，全部包存在，socket drops=0；因此不能归因于源丢包。`MediaRtpIngressPlan::create` 用有限探测的最大乱序位移加一作为运行期队列硬上限，`MediaRtpReorderBuffer::push` 超过该上限即宣布缺包；下一步需核对该容量契约与批处理过期顺序，不能把这次额外等待记作无卡顿通过。
- **仍有发送突发**：输出 RTP 136,102，RTP 序号和 TS 连续性错误均为 0；发送服务曲线超额 24,317.75 B，超过一包 1,356 B 的既有门限。发送器已经按实际 submitCompletedAt 推进 pacing，故不能未经核对就归因于理论时钟追赶；需定位突发时间与两端抓包、内核发送行为。接收抓包已保留，尚未完成整流哈希及该突发事件的对照。
- VLC 使用 D3D11VA，恢复初期有约 30 秒及约 1.3 秒的迟到帧警告，之后日志大小持续不变；15:08:00、15:08:49、15:11:38 的 CPU 累计分别为 5.609375、9.234375、18.84375 秒，进程继续执行。该证据不能单独证明所有画面持续呈现，完整播放门禁不标记通过。

结论：持续运行与同会话编码恢复得到真实链路证明，但额外乱序丢弃和发送突发仍未解决，整体 **FAIL**。本报告不是成功验收记录，待继续修复、完整复测及独立审查。
