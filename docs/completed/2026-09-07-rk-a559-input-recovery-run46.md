# RKMPP RTP HEVC 2560×1440@30 → MPEG-TS/RTP H.264 1920×1080@25 VBR 5/12/13 Mbps：run46 发送突发 FAIL

冻结生产提交 2d597ba60e0c670956f36be5ba8bb8e54bc26b70，CLI SHA-256 `cf7ededa126a41643da3bb28ed1170a3c260a724b14917adc092a6c69ac86ce9`。沿用既有 248.266667 秒 HEVC 2K30 高规格文件，源仅 copy；实际 hevc_rkmpp → scale_rkrga async_depth=0 → h264_rkmpp，zero_copy=true；VLC 默认 D3D11VA。没有修改生产代码或调低媒体参数。

## 实际命令

```bash
dir=/home/tang/MediaTranscode/out/acceptance/rk-a559-loss20-run46
/home/tang/MediaTranscode/out/build/rk-release/media_transcode_realtime_video_cli --media-id rk-userspace-high-hevc2k30-h2641080p25-vbr12m-v1 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --progress-timeout-ms 12000 --video-rtp-url rtp://192.168.130.229:61884 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 192.168.96.122 --rtp-port 6200 --sdp "$dir/output.sdp" --video-codec h264 --rc vbr --min-bitrate 5000 --bitrate 12000 --max-bitrate 13000 --width 1920 --height 1080 --fps 25 --gop 50 --no-audio
ffmpeg -hide_banner -nostdin -re -i /home/tang/rk-highspec-hw-hevc2k30-248s.mp4 -map 0:v:0 -an -c:v copy -f rtp -payload_type 96 -ssrc 3 -sdp_file "$dir/input.sdp" "rtp://192.168.130.229:61884?pkt_size=1400"
```

```powershell
D:\VideoLAN\VLC\vlc.exe --file-logging --log-verbose=2 --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-loss20-run46\vlc.log rtp://@192.168.96.122:6200
D:\Wireshark\dumpcap.exe -i 6 -f "udp and (port 6200 or port 6201)" -a duration:330 -w D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-loss20-run46\receiver.pcapng -q
$watch=[Diagnostics.Stopwatch]::StartNew(); while($watch.Elapsed.TotalSeconds -lt 300) { $samples=(Get-Counter -Counter '\GPU Engine(*)\Running Time').CounterSamples; $samples | Where-Object {$_.InstanceName -like 'pid_37480_*engtype_VideoDecode'} | Select-Object @{Name='Timestamp';Expression={$_.Timestamp.ToString('o')}},InstanceName,RawValue,Status | Export-Csv -LiteralPath 'D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-loss20-run46\vlc-gpu-running-time.csv' -Append -NoTypeInformation -Encoding UTF8 }
```

先启动抓包、VLC，然后 CLI 与源。正常出流 20 秒后，输入 lo 注入 20% 丢包 30 秒；出口 eth0 配置未改：

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

实际注入 2026-09-07 19:20:45.453326845，恢复 19:21:15.466552417，均为目标机 UTC+08:00。

## 数据与判定

| 项目 | 结果 |
| --- | --- |
| 同一会话恢复 | 19:20:58.217 等待可解码输入，19:21:16.590 收到完整关键 AU，19:21:16.760 恢复输出；源运行期间 CLI 不退出 |
| 输入损伤 | 注入前 22,000 包、缺包 0；注入期缺包 6,323，实测 19.9250016%；恢复后 209,320 包、197.334881 秒、缺包 0、乱序 6 |
| VLC 连续硬解 | 接收端 19:21:22.3776284～19:24:33.5155342，87 次连续正增长且 Status=0 的 VideoDecode 采样，191.1379058 秒；累计增加 128,535,194 个 100 ns 单位 |
| 收发完整性 | 两端均 266,162 RTP、全部逐包一致，序列缺包/重复/乱序及 TS 连续性错误均为 0；接收 266,225 UDP、抓包丢包 0；完整收尾已覆盖 |
| 核心入队 | 50 Mbps 下，net_dev_queue 服务曲线最大超额 1,356 B，一包上限成立 |
| 发送突发 | 完整发送 pcap 最大超额 20,896 B，序列 27872～27887 的 16 包在 128 μs 内发送；对应驱动提交最大超额 20,908.5 B、16 包跨度 126 μs，**FAIL** |
| 内核等待 | 最长入队至驱动提交 5,738,001 ns；P99 50,000 ns；本轮 RTP 使用 TX queue 0 |
| 运行错误 | workerErrors=0、errors=0、droppedBuffers=0、graphPayloadPressureFailures=0；trace overrun、输入 socket drops、两路 tcpdump kernel drops 均为 0 |
| CPU / 内存 | 平均单核 CPU 18.865086%，峰值 37.696335%；RSS 初始 52,482,048 B、峰值 58,519,552 B；payload 高水位 13,158,128 B |
| 源驱动结束 | 源 19:20:25.053764884～19:24:34.475450721 自然结束、exit 0；CLI 按既有无输入窗口结束、exit 1；收尾 19:24:47.562624408 |
| A/V 漂移 | VideoOnly，无音频，不适用 |

两端 RTP 内容流 SHA-256：`5bc366432ce689dc10b9596c8b43654c37297c8a6b3bd0ac03e15764a915c535`。完整门禁 **FAIL**；恢复与持续解码通过不能替代无突发要求，不建立成功标签，不更新已交付库的二进制。

## 本次失败原因

1. **已确认发生位置**：核心逐包入队满足原容量服务曲线，包在 Linux 网络栈内等待后集中交给驱动；发送抓包独立确认同一组 16 包，不能解释为只存在于诊断脚本的假象。最长队列等待约 5.74 ms，突发位于目标机 19:23:49.927065 附近，发生在输入恢复之后。
2. **不能归因输入源**：恢复后输入 197.33 秒零缺包，输出收发全部一致，worker 与资源压力错误为 0。本次失败是输出节奏，不是源损伤导致核心退出。
3. **尚未证明的底层原因**：run41/run42 与本次均使用 TX queue 0，已有通过轮次使用 queue 1；这是相关证据，不能当成 BQL、驱动 stop/wake 或特定竞争进程的已证实根因。可见 qdisc_dequeue 状态不能排除 trace 之前的停队列分支。保留原始 trace 与最差窗口继续定位，不绑定队列、改端口或调低发送要求来规避。
4. **修改责任边界**：run43～run46 使用同一生产二进制，本轮没有生产代码修改；该内核聚集风险在 run41/run42 已存在。背压所有权修复没有被证明解决它，也没有证据称为本轮新增回归。

额外源码检查发现 Linux TX timestamp 编译条件错误地对 enum `SOF_TIMESTAMPING_MASK` 使用 defined，造成既有能力分支被关闭；目标机 SO_TIMESTAMPING flags 2194 设置与读回成功。但独立审查同时发现启用后有限 evidence ID 与 socket 全包关联 ID 的潜伏契约冲突。未修改这些代码；它们属于诊断能力问题，修复不能冒称解决本次突发。Linux 官方说明 [TX_SOFTWARE 位于驱动交给网卡之前](https://docs.kernel.org/5.10/networking/timestamping.html)，不能当成真实线上完成时间。

## PID 与证据

CLI 4128168、源 4128191、主脚本 4127845、注入脚本 4127850、输入/输出 tcpdump 4128149/4128150、trace reader 4127913、发送 TID 4128231；VLC 37480、Windows dumpcap 31084。目标机测试进程均已结束，临时执行脚本已删除、tracing instance 已清理、lo 恢复 0%。

目标机与本机各自 `out/acceptance/rk-a559-loss20-run46/` 保存实际脚本内容、命令、PID、日志、资源数据、抓包、GPU CSV 与分析结果；`worst-kernel-window.txt` 保存失败时间窗。诊断工具不入库，库没有下载到本机。
