# RKMPP RTP H.264 2K30 → HEVC 1080p25 CBR 6M：run41 恢复通过，发送突发 FAIL

2026-09-07，目标机 Linux 5.10.110-g1cf46ce40714-dirty。build8 全量构建成功；CLI SHA-256：`c00f9c26dd3b6fe0b05c525634ec190fca753f0f4e1189ceeead5e19f7416bea`。使用既有 248.266667 秒、2560×1440@30 有限视频，未循环、未改变 CLI 参数。实际链路为 h264_rkmpp → scale_rkrga async_depth=0 → hevc_rkmpp；VLC 默认 D3D11VA。

## 实际命令

先启动播放器、抓包及 CLI，再启动源流：

```bash
dir=/home/tang/MediaTranscode/out/acceptance/rk-a559-loss20-run41
/home/tang/MediaTranscode/out/build/rk-release/media_transcode_realtime_video_cli --media-id rk-userspace-low-h264720p30-hevc1080p25-cbr6m-v1 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --progress-timeout-ms 12000 --video-rtp-url rtp://192.168.130.229:61884 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 192.168.96.122 --rtp-port 6200 --sdp "$dir/output.sdp" --video-codec hevc --rc cbr --bitrate 6000 --width 1920 --height 1080 --fps 25 --gop 50 --no-audio
ffmpeg -hide_banner -nostdin -re -i /home/tang/rk-highspec-hw-hevc2k30-to-h2642k30-248s.mp4 -map 0:v:0 -an -c:v copy -f rtp -payload_type 96 -ssrc 3 -sdp_file "$dir/input.sdp" "rtp://192.168.130.229:61884?pkt_size=1400"
```

```powershell
D:\VideoLAN\VLC\vlc.exe --file-logging --log-verbose=2 --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-loss20-run41\vlc.log rtp://@192.168.96.122:6200
D:\Wireshark\dumpcap.exe -i 6 -f "udp and (port 6200 or port 6201)" -a duration:275 -w D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-loss20-run41\receiver.pcapng -q
Get-Counter -Counter '\GPU Engine(*)\Running Time' -SampleInterval 1 -MaxSamples 270 | ForEach-Object { $_.CounterSamples | Where-Object { $_.InstanceName -like 'pid_11492_*engtype_VideoDecode' } | Select-Object @{Name='Timestamp';Expression={$_.Timestamp.ToString('o')}},InstanceName,RawValue } | Export-Csv -LiteralPath 'D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-loss20-run41\vlc-gpu-running-time.csv' -NoTypeInformation -Encoding UTF8
```

### 输入丢包及恢复

源发往目标机自身地址，经 lo；出口 eth0 没有注入丢包。lo 已存在 netem root，以下为本次实际执行内容。等待日志出现非零 encodedPacketsPushed 后再等待 20 秒，注入持续 30 秒：

```bash
trap 'tc qdisc change dev lo root netem loss 0%' EXIT
while [ ! -f "$dir/cli.log" ] || ! grep -Eq 'encodedPacketsPushed=[1-9][0-9]*' "$dir/cli.log"; do sleep 0.1; done
sleep 20
if [ -f "$dir/cli.exit" ]; then exit 1; fi
date +%s.%N > "$dir/injection-before-epoch.txt"
tc qdisc change dev lo root netem loss 20%
date -Ins > "$dir/injection-start.txt"
date +%s.%N > "$dir/injection-start-epoch.txt"
tc -s qdisc show dev lo > "$dir/qdisc-during-loss.txt"
sleep 30
date +%s.%N > "$dir/recovery-before-epoch.txt"
tc qdisc change dev lo root netem loss 0%
date -Ins > "$dir/recovery-start.txt"
date +%s.%N > "$dir/recovery-start-epoch.txt"
tc -s qdisc show dev lo > "$dir/qdisc-after-loss.txt"
```

注入命令执行区间为 16:09:39.767651048～16:09:39.775360242，恢复区间为 16:10:09.782297769～16:10:09.789980713（UTC+08:00）。随后的日期记录分别为 16:09:39.773346536、16:10:09.788089800。切换不是一个可精确到单包的原子观测点；按命令完成时间切分得到的注入前 4 个缺包落在切换附近，不据此认定源缺陷。

## 结果与根因

- **持续与恢复通过**：16:09:52.531 进入等待可解码输入，16:10:11.074 同一 CLI 恢复输出。源 16:13:28.009507 自然结束、exit 0；CLI 在原 12 秒无输入窗口后退出、exit 1。workerErrors=0、errors=0、payload pressure failures=0。平均单核 CPU 13.957736%，峰值 32.653061%，RSS 初始 54,472,704 B、峰值 59,785,216 B。无音频，A/V 漂移不适用。
- 丢包期实际缺包 6,287，比例 19.8259279%；恢复后 197.335 秒、208,701 个输入包，缺包 0、乱序 62，输入 socket drops 全部为 0。既有 reorder 产品为 window_packets=1024、maximum_delay_ns=17057297。
- **VLC 恢复后持续硬解超过三分钟**：16:10:17.0041566～16:13:27.6453915，188 次连续正增长 VideoDecode 采样覆盖 190.6412349 秒，累计增加 65,719,423 个 100 ns 单位。恢复过渡有旧缓冲丢弃和约 30.4 秒晚帧记录；不将输入损坏期间宣称为连续画面播放。
- **捕获范围内收发无丢包**：发送端 137,872 个 RTP 包全部按顺序、按内容匹配接收端，接收端另有末尾 2 包，完整文件哈希因此不同。两端 RTP 序号与 TS 连续性错误均为 0；接收抓包报告丢包 0。发送侧捕获结束较早，不能将未捕获的尾部算入全流哈希一致证据。
- **发送无突发 FAIL，已定位聚集阶段**：按原 50,000,000 bps 服务曲线，发送抓包最大超额 11,961 B。后半程内核 trace 捕获同一次突发：应用提交对应的 net_dev_queue 曲线超额只有一包 1,356 B；net_dev_start_xmit 却在 172 微秒集中提交 11 包，超额 11,961 B。首包从入队到驱动提交滞留 3.080 ms，期间后续包积累；eth0 使用 mq/pfifo_fast，qdisc requeues 累计为 3。证据证明聚集发生于内核入队之后、驱动提交之前；尚未证明具体是哪条内核调度或驱动停队路径，不能将其归因为应用追赶发送。该 trace 只覆盖后半程，不冒充全程内核证据。
- 输出 5,427 个 PTS，媒体跨度 248.2 秒；接收抓包服务曲线超额 22,516.75 B。恢复代码未解决发送队列聚集，整项仍为 FAIL，不创建成功标签、不作为通过版本发布库。

## 证据与清理

证据目录：目标机及本机 `out/acceptance/rk-a559-loss20-run41`。关键文件：`input-phase-analysis.json`、`output-timing-analysis.json`、`receiver-timing-analysis.json`、`send-receive-alignment.json`、`kernel-analysis.json`、`kernel-net-trace.txt`、`vlc-gpu-analysis.json`、`vlc-gpu-running-time.csv`、`cli.log`、`injection-script-content.txt`、`run-script-content.txt`。

CLI / 源 PID 3994017 / 3994038；输入 / 输出抓包 3993998 / 3993999；执行 / 注入脚本 3993962 / 3993963；内核 trace reader 3996119，发送线程 TID 3994081；VLC / Windows 抓包 11492 / 568。上述目标机进程已结束，临时执行脚本已删除；独立 tracing instance 已移除，lo 恢复 0%，eth0 配置未更改。VLC 保持用户要求的可见窗口。

本次验证覆盖三项审查修复后的真实恢复路径；并不替代容量极限的源码审查，也不代表 CBR/VBR 四种转换全部回归完成。下一步只定位已有证据指向的发送队列聚集，并继续两个独立审查。
