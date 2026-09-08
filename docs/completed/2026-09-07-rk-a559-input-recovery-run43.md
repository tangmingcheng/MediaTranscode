# RKMPP RTP H.264 2560×1440@30 → MPEG-TS/RTP HEVC 1920×1080@25 CBR 6 Mbps：run43 输入丢包恢复 PASS

生产提交 2d597ba60e0c670956f36be5ba8bb8e54bc26b70；使用已成功构建的 build9，代码、CLI 参数与 run42 相同。CLI SHA-256：`cf7ededa126a41643da3bb28ed1170a3c260a724b14917adc092a6c69ac86ce9`。本次补齐连续 GPU 采样，未修改媒体参数或发送实现。既有有限源 248.266667 秒、H.264 2560×1440@30；实际 h264_rkmpp → scale_rkrga async_depth=0 → hevc_rkmpp、zero_copy=true，VLC 默认 D3D11VA。

## 实际命令

```bash
dir=/home/tang/MediaTranscode/out/acceptance/rk-a559-loss20-run43
/home/tang/MediaTranscode/out/build/rk-release/media_transcode_realtime_video_cli --media-id rk-userspace-low-h264720p30-hevc1080p25-cbr6m-v1 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --progress-timeout-ms 12000 --video-rtp-url rtp://192.168.130.229:61884 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 192.168.96.122 --rtp-port 6200 --sdp "$dir/output.sdp" --video-codec hevc --rc cbr --bitrate 6000 --width 1920 --height 1080 --fps 25 --gop 50 --no-audio
ffmpeg -hide_banner -nostdin -re -i /home/tang/rk-highspec-hw-hevc2k30-to-h2642k30-248s.mp4 -map 0:v:0 -an -c:v copy -f rtp -payload_type 96 -ssrc 3 -sdp_file "$dir/input.sdp" "rtp://192.168.130.229:61884?pkt_size=1400"
```

```powershell
D:\VideoLAN\VLC\vlc.exe --file-logging --log-verbose=2 --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-loss20-run43\vlc.log rtp://@192.168.96.122:6200
D:\Wireshark\dumpcap.exe -i 6 -f "udp and (port 6200 or port 6201)" -a duration:275 -w D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-loss20-run43\receiver.pcapng -q
$watch=[Diagnostics.Stopwatch]::StartNew(); while($watch.Elapsed.TotalSeconds -lt 275) { $samples=(Get-Counter -Counter '\GPU Engine(*)\Running Time').CounterSamples; $samples | Where-Object {$_.InstanceName -like 'pid_34800_*engtype_VideoDecode'} | Select-Object @{Name='Timestamp';Expression={$_.Timestamp.ToString('o')}},InstanceName,RawValue | Export-Csv -LiteralPath 'D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-loss20-run43\vlc-gpu-running-time.csv' -Append -NoTypeInformation -Encoding UTF8 }
```

## 输入丢包与恢复

先正常出流，等待 20 秒后执行以下命令；源到本机地址经 lo，出口 eth0 未注入丢包、未更改配置：

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

实际注入时间 2026-09-07 17:09:59.722228137，恢复时间 17:10:29.735648628（UTC+08:00，tc 后日期记录）。命令前后原始时间戳及完整执行脚本保留在证据目录。

## 验收数据

| 项目 | 结果 |
| --- | --- |
| 同一核心持续运行 | 源持续约 248 秒，CLI 没有提前退出；17:10:12.527 等待可解码输入，17:10:30.904 收到完整关键 AU，17:10:31.069 恢复输出 |
| 源驱动结束 | 源 17:13:48.382248396 自然结束、exit 0；CLI 按原无输入窗口结束、exit 1；测试收尾 17:14:01.475930850 |
| 真实输入丢包 | 注入前 21,986 包、缺包 0；注入期缺包 6,527，20.6016034%；恢复后 208,731 包、197.367988 秒、缺包 0、乱序 92 |
| 持续硬解 | 17:10:36.6271525～17:13:46.6636070，58 次连续正增长 VideoDecode 采样、190.0364545 秒、累计增加 66,585,616 个 100 ns 单位；采样间隔约 3 秒，输入恢复过渡有一次不增长区间，未计入连续通过段 |
| 收发完整性 | 两端均 137,877 个 RTP，逐包完全一致、缺包/重复/乱序均为 0；TS 连续性错误 0；接收抓包丢包 0 |
| 发送节奏 | 发送抓包、核心 RTP 对应 net_dev_queue 与 net_dev_start_xmit 服务曲线最大超额均为一包 1,356 B；本次没有发送突发 |
| 运行错误 | workerErrors=0、errors=0、droppedBuffers=0、graphPayloadPressureFailures=0 |
| CPU / 内存 | 平均单核 CPU 14.952920%，峰值 28.426396%；RSS 初始 55,939,072 B、峰值 60,317,696 B；payload 高水位 108,951,309 B |
| A/V 漂移 | VideoOnly，无音频，不适用 |

两端 RTP 内容流 SHA-256：`6f7f1d9bca24dddaa317e39218620c111340b8b920741abd885f492b600977fb`。接收抓包共 137,936 个 UDP 包。发送末尾覆盖完整，收发没有额外尾包差异。

## 诊断修正与适用边界

run42 的 GPU 采样器在引擎创建前枚举实例，输出没有样本；本次逐次查询并追加写入，运行早期已检查到实际递增记录，补齐连续硬解证据。

内核分析初值 1,404.75 B 混入了发送线程名下 `..s1` 中断上下文的一条非 RTP 网络包（以太帧长度 469 B、gso_type=0x1）。修正诊断过滤以排除所有硬/软中断上下文后，RTP 内核事件数与发送抓包同为 137,877，服务曲线均为 1,356 B。原始 trace 保留，未删包或改动抓包分析；发送 pcap 的独立结果始终为 1,356 B。

后半程另用独立 function tracer 记录 __netif_schedule/netif_tx_wake_queue 调用栈；原实例因 trace_pipe 正打开而拒绝切换 tracer，随后在独立实例成功采集，两个实例均已清理。只观察到调度及 watchdog 等路径，未据此判定旧突发根因。此次通过不证明 run41/run42 的偶发内核队列聚集已经修复，也不证明特定 WouldBlock 分支已由运行数据覆盖。

本项真实链路 PASS，立即独立提交、推送并按用户指令产出本版本库。其他 CBR/VBR、H.264↔HEVC 矩阵回归仍未完成。审查服务恢复后，两名未参与实现的独立审查者均对冻结 2d597ba6 明确给出 Standards PASS、Spec 源码 PASS；专项评分各为 88/100，运行范围限制仍保留。

## PID 与证据清理

目标机 `out/acceptance/rk-a559-loss20-run43` 保存脚本内容、CLI/源日志、输入/输出抓包、包分析及内核诊断；本机对应目录保存 VLC 日志、接收抓包与 GPU CSV。CLI/源 4042749/4042772，输入/输出抓包 4042728/4042729，主/注入脚本 4042684/4042685，网络 trace reader 4042698，附加函数 trace reader 4044207，发送 TID 4042816；VLC 34800，Windows 抓包 34988。

目标机测试进程均已结束，临时执行脚本及独立 tracing instances 已删除，lo 恢复 0%，出口配置未改。已交付 53c5c0d0 库保留，新版另建目录，不覆盖旧包。
## 本次库交付

已按用户指定脚本生成新版，旧 53c5c0d0 库保留：

```bash
/home/tang/package_media_transcode_beta.sh /home/tang/MediaTranscode/out/build/rk-release /home/tang/packages/media-transcode-beta-rkmpp-ffed7187-run43-20260907
```

提交并推送成功记录 ffed7187，标签 rk-input-loss20-cbr6m-run43 指向该提交。库目录 `/home/tang/packages/media-transcode-beta-rkmpp-ffed7187-run43-20260907`，同名 `.tar.gz` 包 19,176,566 B，SHA-256：`d6833e491c73e73d195cbcfe5e1a46d61892f0c0429601106001760edcdf6e9e`。按用户要求只保留目标机，未下载库包。

包含合并静态库、公开头文件、CLI、C 示例源码与可执行文件、实际依赖、源码清单、run43 报告、抓包分析和 GPU CSV。C 示例用新库完成 C11 严格编译及链接，exit 0；包内全部文件 sha256sum 校验成功。标签与 README 保留旧内核偶发突发及其余矩阵限制。包内 RUN43 的 GPU 命令已按实际 PID 34800 修正文档生成错误；媒体二进制保持不变。