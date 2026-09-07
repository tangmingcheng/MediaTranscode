# RKMPP RTP H.264 2K30 → HEVC 1080p25 CBR 6M：run42 背压版本恢复验证，完整门禁 FAIL

生产代码 2d597ba60e0c670956f36be5ba8bb8e54bc26b70，全量构建成功；目标机 1,350 个源码/构建配置归一行尾后全部匹配。CLI SHA-256：cf7ededa126a41643da3bb28ed1170a3c260a724b14917adc092a6c69ac86ce9。本轮使用原 CLI 参数和既有 248.266667 秒 H.264 2560×1440@30 有限源；硬件链路为 h264_rkmpp → scale_rkrga → hevc_rkmpp，VLC 默认 D3D11VA。

## 实际命令

```bash
dir=/home/tang/MediaTranscode/out/acceptance/rk-a559-loss20-run42
/home/tang/MediaTranscode/out/build/rk-release/media_transcode_realtime_video_cli --media-id rk-userspace-low-h264720p30-hevc1080p25-cbr6m-v1 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --progress-timeout-ms 12000 --video-rtp-url rtp://192.168.130.229:61884 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 192.168.96.122 --rtp-port 6200 --sdp "$dir/output.sdp" --video-codec hevc --rc cbr --bitrate 6000 --width 1920 --height 1080 --fps 25 --gop 50 --no-audio
ffmpeg -hide_banner -nostdin -re -i /home/tang/rk-highspec-hw-hevc2k30-to-h2642k30-248s.mp4 -map 0:v:0 -an -c:v copy -f rtp -payload_type 96 -ssrc 3 -sdp_file "$dir/input.sdp" "rtp://192.168.130.229:61884?pkt_size=1400"
```

```powershell
D:\VideoLAN\VLC\vlc.exe --file-logging --log-verbose=2 --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-loss20-run42\vlc.log rtp://@192.168.96.122:6200
D:\Wireshark\dumpcap.exe -i 6 -f "udp and (port 6200 or port 6201)" -a duration:275 -w D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-loss20-run42\receiver.pcapng -q
Get-Counter -Counter '\GPU Engine(*)\Running Time' -SampleInterval 1 -MaxSamples 270 | ForEach-Object { $_.CounterSamples | Where-Object { $_.InstanceName -like 'pid_30548_*engtype_VideoDecode' } | Select-Object @{Name='Timestamp';Expression={$_.Timestamp.ToString('o')}},InstanceName,RawValue } | Export-Csv -LiteralPath 'D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-loss20-run42\vlc-gpu-running-time.csv' -NoTypeInformation -Encoding UTF8
```

## 输入丢包与恢复命令

正常出流后等待 20 秒，输入源路由经过 lo；仅对 lo 注入 20% 丢包，持续 30 秒。出口 eth0 配置没有更改：

```bash
trap 'tc qdisc change dev lo root netem loss 0%' EXIT
while [ ! -f "$dir/cli.log" ] || ! grep -Eq 'encodedPacketsPushed=[1-9][0-9]*' "$dir/cli.log"; do sleep 0.1; done
sleep 20
tc qdisc change dev lo root netem loss 20%
tc -s qdisc show dev lo
sleep 30
tc qdisc change dev lo root netem loss 0%
tc -s qdisc show dev lo
```

实际注入时间 2026-09-07 16:58:46.773706712，恢复时间 16:59:16.788011886（UTC+08:00，均为 tc 后的日期记录）。完整脚本及命令前后时间戳保存在目标机证据目录。

## 结果及失败原因

- 同一 CLI 于 16:58:59.541 进入等待可解码输入状态，16:59:17.923 收到完整关键 AU，16:59:18.084 恢复出流。源 17:02:35.556089479 自然结束、exit 0；CLI 在原无输入窗口后于 17:02:46.839 结束、exit 1，未在源仍活动时退出。
- 注入前 22,003 包、缺包 0；注入期缺包 6,377，比例 20.1205275%；恢复后 208,701 包、197.335104 秒、缺包 0、乱序 170。输入 socket drops 全程为 0；撤销丢包后未出现额外 discontinuity。
- workerErrors=0、errors=0、droppedBuffers=0、graphPayloadPressureFailures=0；平均单核 CPU 15.293626%，峰值 33.160622%，RSS 初始 54,333,440 B、峰值 59,658,240 B，payload 高水位 109,011,381 B。VideoOnly，A/V 漂移不适用。backpressureItems=27 是运行时总计，不能单独证明触发本次特定 WouldBlock 分支。
- **收发零丢包通过**：两端均 137,874 个 RTP 包，序号连续、TS 连续性错误 0、全部逐包匹配，RTP 内容流 SHA-256 均为 `9da067d6b2168ae78e31fd51d26da3f9572a018c28c8a60e281ca70ac5ddfd74`。接收抓包 137,933 个 UDP 包，抓包丢包 0。本次在 CLI 结束后保留 1 秒抓包尾部，避免 run41 尾部覆盖不一致。
- **发送无突发 FAIL**：核心对应 net_dev_queue 服务曲线仍为一包 1,356 B；驱动提交最大为 14 包/391 微秒、超额 16,540.25 B，发送抓包对应 14 包/388 微秒、超额 16,559 B。最大入队至驱动提交延迟 8.096 ms。没有修改核心发送实现，不能把这一结果写成背压修复新引入的发送问题；run41 已存在相同阶段的聚集。
- 所记录 305,573 次 net_dev_xmit 返回值全部为 0，604,470 次 qdisc_dequeue 的可见 txq_state 均为 0，trace overrun=0。这些是 eth0 总体诊断事件，不全部属于 RTP。仍未证明聚集具体由哪条停队/调度路径导致；Linux 5.10 sch_generic.c 的部分 stopped 分支在 trace_qdisc_dequeue 前返回，因此“记录的状态全为 0”不能排除未记录的停队。
- **连续播放证据不完整**：VLC 日志确认默认 D3D11VA，恢复时约 30.4 秒旧帧丢弃，随后仅见 40 ms/近零晚帧提示；现场单次查询 VideoDecode 累计从 23,556,536 增至 62,388,640，结束后 71,449,163。但上述连续 Get-Counter 命令输出文件只有 UTF-8 BOM，没有样本。采集开始时 VLC 尚未创建视频解码引擎，后续单次查询已有对应实例；需在实例出现后启动采集并确认首批数据。当前证据不足以认定完整三分钟连续硬解，不能用单次累计值代替连续观测，也不能据此认定播放卡顿。

## 证据、清理与后续

目标机目录 `/home/tang/MediaTranscode/out/acceptance/rk-a559-loss20-run42`；本机仅保存接收抓包与 VLC 诊断数据。CLI / 源 PID 4037667 / 4037688；输入 / 输出抓包 4037650 / 4037651；主脚本 / 注入脚本 4037119 / 4037120；内核 trace reader 4037173，发送 TID 4037731；VLC 30548，Windows 抓包 33020。GPU 采集命令退出 0，但无样本，不能当成采样通过。

目标机测试进程全部结束，临时执行脚本已删除，脚本内容保留在证据目录；独立 tracing instance 已删除，lo 已恢复 0%，eth0 未修改。库包仍固定为 53c5c0d0，没有替换为本轮版本。

本轮验证背压版本的正常丢包恢复没有观察到回归，不能替代特定背压路径的源码审查。完整门禁因发送突发与连续播放证据缺失仍为 FAIL；后续保持代码与参数不变补齐采样，继续查明内核排队路径，两位独立复审仍待服务额度恢复。

诊断依据：[Linux 5.10 sch_generic.c](https://raw.githubusercontent.com/torvalds/linux/v5.10/net/sched/sch_generic.c)、[Linux 5.10 netdevice.h](https://raw.githubusercontent.com/torvalds/linux/v5.10/include/linux/netdevice.h)。仅使用其事件位置与状态位定义解释观测，不将主线源码冒称为目标厂商内核逐字一致的实现。