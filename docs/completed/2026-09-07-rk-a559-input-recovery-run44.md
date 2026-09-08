# RKMPP RTP HEVC 2560×1440@30 → MPEG-TS/RTP H.264 1920×1080@25 CBR 6 Mbps：run44 输入丢包恢复 PASS

冻结生产提交 2d597ba60e0c670956f36be5ba8bb8e54bc26b70，CLI SHA-256 `cf7ededa126a41643da3bb28ed1170a3c260a724b14917adc092a6c69ac86ce9`。沿用既有 248.266667 秒 HEVC 2K30 高规格文件，源仅 copy；实际 hevc_rkmpp → scale_rkrga async_depth=0 → h264_rkmpp，zero_copy=true；VLC 默认 D3D11VA。没有修改生产代码或调低媒体参数。

## 实际命令

```bash
dir=/home/tang/MediaTranscode/out/acceptance/rk-a559-loss20-run44
/home/tang/MediaTranscode/out/build/rk-release/media_transcode_realtime_video_cli --media-id rk-userspace-high-hevc2k30-h2641080p25-cbr6m-v1 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --progress-timeout-ms 12000 --video-rtp-url rtp://192.168.130.229:61884 --video-rtp-codec hevc --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 192.168.96.122 --rtp-port 6200 --sdp "$dir/output.sdp" --video-codec h264 --rc cbr --bitrate 6000 --width 1920 --height 1080 --fps 25 --gop 50 --no-audio
ffmpeg -hide_banner -nostdin -re -i /home/tang/rk-highspec-hw-hevc2k30-248s.mp4 -map 0:v:0 -an -c:v copy -f rtp -payload_type 96 -ssrc 3 -sdp_file "$dir/input.sdp" "rtp://192.168.130.229:61884?pkt_size=1400"
```

```powershell
D:\VideoLAN\VLC\vlc.exe --file-logging --log-verbose=2 --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-loss20-run44\vlc.log rtp://@192.168.96.122:6200
D:\Wireshark\dumpcap.exe -i 6 -f "udp and (port 6200 or port 6201)" -a duration:275 -w D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-loss20-run44\receiver.pcapng -q
$watch=[Diagnostics.Stopwatch]::StartNew(); while($watch.Elapsed.TotalSeconds -lt 275) { $samples=(Get-Counter -Counter '\GPU Engine(*)\Running Time').CounterSamples; $samples | Where-Object {$_.InstanceName -like 'pid_32796_*engtype_VideoDecode'} | Select-Object @{Name='Timestamp';Expression={$_.Timestamp.ToString('o')}},InstanceName,RawValue | Export-Csv -LiteralPath 'D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-loss20-run44\vlc-gpu-running-time.csv' -Append -NoTypeInformation -Encoding UTF8 }
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

实际注入 2026-09-07 17:26:30.754053480，恢复 17:27:00.768821580，均 UTC+08:00。

## 数据与判定

| 项目 | 结果 |
| --- | --- |
| 同一会话恢复 | 17:26:43.483 等待可解码输入，17:27:01.874 收到完整关键 AU，17:27:02.017 恢复输出；源运行期间 CLI 不退出 |
| 输入损伤 | 注入前 22,022 包、缺包 0；注入期缺包 6,307，实测 19.8752088%；恢复后 209,320 包、197.335070 秒、缺包 0、乱序 8 |
| VLC 连续硬解 | 恢复后 17:27:07.2031453～17:30:18.0932294，59 次连续正增长 VideoDecode 采样，190.8900841 秒、累计增加 130,640,251 个 100 ns 单位；过渡段一次不增长未计入 |
| 收发完整性 | 两端均 137,803 RTP，逐包完全一致，缺包/重复/乱序及 TS 连续性错误均为 0；接收 137,863 UDP、抓包丢包 0 |
| 发送节奏 | 50 Mbps 下，发送抓包、核心 net_dev_queue 与 net_dev_start_xmit 服务曲线最大超额均为 1,356 B，即一包；无发送突发 |
| 内核队列 | 最大入队至驱动提交 113,999 ns，P99 51,000 ns；RTP 事件数与抓包一致；trace overrun、输入 socket drops、两路 tcpdump kernel drops 均为 0 |
| 运行错误 | workerErrors=0、errors=0、droppedBuffers=0、graphPayloadPressureFailures=0 |
| CPU / 内存 | 平均单核 CPU 14.731066%，峰值 36.734694%；RSS 初始 51,007,488 B、峰值 56,815,616 B；payload 高水位 12,840,947 B |
| 源驱动结束 | 有限源约 248 秒后于 17:30:19.656725126 自然结束、exit 0；CLI 17:30:30.967 按既有无输入窗口退出、exit 1；非源期间崩溃 |
| A/V 漂移 | VideoOnly，无音频，不适用 |

两端 RTP 内容流 SHA-256：`546ea834d2aba0d1bb573976d7bf6b5e06069986074ed3415dd7ed8aadfefc39`。本项完整门禁 **PASS**，立即独立提交并推送。当前版本两项 CBR 恢复已通过；两项 VBR 恢复仍待测。run41/run42 的偶发内核突发没有因本次通过而被宣称修复。

## PID 与证据

CLI 4050386、源 4050411、主脚本 4050323、注入脚本 4050324、输入/输出 tcpdump 4050365/4050366、trace reader 4050336、发送 TID 4050450；VLC 32796、Windows dumpcap 33868。目标机测试进程均已结束，临时执行脚本已删除、tracing instance 已清理、lo 恢复 0%。

目标机与本机各自 `out/acceptance/rk-a559-loss20-run44/` 保存实际脚本内容、命令、PID、日志、资源数据、抓包、GPU CSV 与分析结果；诊断分析器不入库。库继续保留目标机，没有下载。
