# RKMPP RTP H.264 2K30 → HEVC 1080p25 CBR 6M：run40 恢复与发送通过，完整门禁待补证

## 命令与环境

CLI SHA-256：`fbc3985dbb5da780121fe4fecd660eb452bd93c744548e4adad41c98ff775df9`。沿用现有 248.266667 秒 H.264 2560×1440@30 有限视频，参数未变，未使用循环或软件解码。

```bash
dir=/home/tang/MediaTranscode/out/acceptance/rk-a559-loss20-run40
/home/tang/MediaTranscode/out/build/rk-release/media_transcode_realtime_video_cli --media-id rk-userspace-low-h264720p30-hevc1080p25-cbr6m-v1 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --progress-timeout-ms 12000 --video-rtp-url rtp://192.168.130.229:61884 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 192.168.96.122 --rtp-port 6200 --sdp "$dir/output.sdp" --video-codec hevc --rc cbr --bitrate 6000 --width 1920 --height 1080 --fps 25 --gop 50 --no-audio
ffmpeg -hide_banner -nostdin -re -i /home/tang/rk-highspec-hw-hevc2k30-to-h2642k30-248s.mp4 -map 0:v:0 -an -c:v copy -f rtp -payload_type 96 -ssrc 3 -sdp_file "$dir/input.sdp" "rtp://192.168.130.229:61884?pkt_size=1400"
```

```powershell
D:\VideoLAN\VLC\vlc.exe --file-logging --log-verbose=2 --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-loss20-run40\vlc.log rtp://@192.168.96.122:6200
D:\Wireshark\dumpcap.exe -i 6 -f "udp and (port 6200 or port 6201)" -a duration:275 -w D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-loss20-run40\receiver.pcapng -q
```

源自然结束后，从接收抓包还原 TS，另行硬件解码校验；没有在实时播放链路中插入 FFmpeg 监控进程：

```bash
ffmpeg -hide_banner -nostdin -v info -c:v hevc_rkmpp -i "$dir/receiver.ts" -map 0:v:0 -an -f null -
```

CLI / 源 PID 3969273 / 3969298；输入/输出抓包 3969256 / 3969257；执行/注入脚本 3969211 / 3969212；内核 trace reader 3969225；离线硬解 3974377；VLC / Windows 抓包 33296 / 33240。远程脚本内容保留在证据目录，临时执行脚本已删除，以上目标机进程均结束。内核独立 tracing instance 已删除，原全局 tracing 保持关闭；eth0 的 mq/pfifo_fast 未改，lo 已恢复 0% 丢包。VLC 按用户要求恢复可见，未重启播放进程。

## 结果及原因对照

- 15:35:50.679 注入 20% 输入丢包，15:36:20.693 撤销。实际缺包 6,208，19.5552%；注入前缺包 0，撤销后 197.301613 秒、208,666 包，缺包 0、乱序 55，socket drops 全部为 0。
- 15:36:03.411 进入输入活跃但等待可解码数据状态；15:36:21.953 同一会话恢复输出，一直持续至有限源结束。源于 15:39:39.765 自然退出，exit 0；CLI 按原 12 秒无输入窗口于 15:39:50.793 退出，exit 1。workerErrors=0、errors=0、payload pressure failures=0，平均单核 CPU 15.146063%，峰值 RSS 60,719,104 B；无音频，A/V 漂移不适用。
- **run39 乱序误判未复现**：运行日志记录 planner 形成的 `window_packets=1024 maximum_delay_ns=7613513`。撤销丢包之后没有额外 discontinuity；既有等待时间界限未放宽。修复只将有限样本位移上限替换为已有接收描述符预算，未修改批次消费顺序。
- **发送端无突发**：137,851 个 RTP 包，两端序号与 TS 连续性错误均为 0；RTP 内容流 SHA-256 均为 `d0bf600950dc73a12d5b23ea3231c6820804b441a57e12db487b2d7998b52eb7`。发送抓包服务曲线最大超额为一包 1,356 B；发送线程对应的 RTP 内核入队与驱动提交事件也均为 1,356 B，trace buffer 无 overrun。该项并不证明 run39 的偶发发送突发根因已经修复，本轮未改发送代码。
- **接收端局部聚集仍存在**：接收服务曲线超额 15,686 B；序号 24565–24587 的 23 包接收集中在 2.360 ms，同一组包目标机发送跨度为 6.416 ms，主要包间隔 268–324 µs。现有证据将聚集范围缩小到驱动提交之后；不能进一步武断区分网卡实际发送、交换网络或接收抓包时间，也不能据此修改核心发送代码。用户要求的收发无丢包已满足。
- 输出包含 5,428 个 PTS，跨度 248.2 秒；实际硬件链路为 h264_rkmpp → scale_rkrga async_depth=0 → hevc_rkmpp。接收 TS 由 hevc_rkmpp 解码全部 5,428 帧，exit 0；离线 FFmpeg 报告与输入损伤相符的 31.12 秒时间缺口，未将时间缺口当作媒体完整性通过。
- VLC 默认 D3D11VA，恢复时有 5 条约 30.4 秒晚帧记录及丢弃缓冲旧帧记录，随后仅见 10–11 ms 提示，未再出现 run39 的约 1.3 秒迟到。VLC CPU 累计从播放中 2.46875 秒增长到结束后 21.90625 秒，最终 GPU VideoDecode 累计为 82,488,922 个 100 ns 单位。没有采集覆盖恢复后三分钟的连续 GPU 解码计数，不能把 CPU 或最终累计值单独写成整段 VLC 持续解码已证明。

核心同会话恢复、发送节奏和收发零丢包取得通过证据；完整播放门禁仍待补齐连续 VLC 解码观测。本项不是完整成功验收提交；后续保持代码不变补证，并继续 CBR/VBR、H.264↔HEVC 回归和双独立审查。
