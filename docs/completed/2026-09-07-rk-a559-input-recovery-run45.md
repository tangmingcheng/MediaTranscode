# RKMPP RTP H.264 2560×1440@30 → MPEG-TS/RTP HEVC 1920×1080@25 VBR 5/12/13 Mbps：run45 输入丢包恢复 PASS

冻结生产提交 2d597ba60e0c670956f36be5ba8bb8e54bc26b70，CLI SHA-256 `cf7ededa126a41643da3bb28ed1170a3c260a724b14917adc092a6c69ac86ce9`。沿用既有 248.266667 秒 H.264 2K30 高规格文件，源仅 copy；实际 h264_rkmpp → scale_rkrga async_depth=0 → hevc_rkmpp，zero_copy=true；VLC 默认 D3D11VA。没有修改生产代码或调低媒体参数。

## 实际命令

```bash
dir=/home/tang/MediaTranscode/out/acceptance/rk-a559-loss20-run45
/home/tang/MediaTranscode/out/build/rk-release/media_transcode_realtime_video_cli --media-id rk-userspace-high-h2642k30-hevc1080p25-vbr12m-v1 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --progress-timeout-ms 12000 --video-rtp-url rtp://192.168.130.229:61884 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 192.168.96.122 --rtp-port 6200 --sdp "$dir/output.sdp" --video-codec hevc --rc vbr --min-bitrate 5000 --bitrate 12000 --max-bitrate 13000 --width 1920 --height 1080 --fps 25 --gop 50 --no-audio
ffmpeg -hide_banner -nostdin -re -i /home/tang/rk-highspec-hw-hevc2k30-to-h2642k30-248s.mp4 -map 0:v:0 -an -c:v copy -f rtp -payload_type 96 -ssrc 3 -sdp_file "$dir/input.sdp" "rtp://192.168.130.229:61884?pkt_size=1400"
```

```powershell
D:\VideoLAN\VLC\vlc.exe --file-logging --log-verbose=2 --logfile=D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-loss20-run45\vlc.log rtp://@192.168.96.122:6200
D:\Wireshark\dumpcap.exe -i 6 -f "udp and (port 6200 or port 6201)" -a duration:275 -w D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-loss20-run45\receiver.pcapng -q
$watch=[Diagnostics.Stopwatch]::StartNew(); while($watch.Elapsed.TotalSeconds -lt 275) { $samples=(Get-Counter -Counter '\GPU Engine(*)\Running Time').CounterSamples; $samples | Where-Object {$_.InstanceName -like 'pid_36848_*engtype_VideoDecode'} | Select-Object @{Name='Timestamp';Expression={$_.Timestamp.ToString('o')}},InstanceName,RawValue | Export-Csv -LiteralPath 'D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-loss20-run45\vlc-gpu-running-time.csv' -Append -NoTypeInformation -Encoding UTF8 }
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

实际注入 2026-09-07 19:11:28.968249122，恢复 19:11:58.981841210，均为目标机 UTC+08:00。

## 数据与判定

| 项目 | 结果 |
| --- | --- |
| 同一会话恢复 | 19:11:41.750 等待可解码输入，19:12:00.154 收到完整关键 AU，19:12:00.290 恢复输出；源运行期间 CLI 不退出 |
| 输入损伤 | 注入前 21,986 包、缺包 0；注入期缺包 6,383，实测 20.1483586%；恢复后 208,731 包、197.368150 秒、缺包 0、乱序 15 |
| VLC 连续硬解 | 接收端 19:12:05.9702850～19:15:17.3520502，81 次连续正增长 VideoDecode 采样，191.3817652 秒、累计增加 84,447,428 个 100 ns 单位；过渡段一次不增长未计入 |
| 收发完整性 | 发送 266,799 RTP，接收覆盖起始连续 266,516 RTP、逐包一致，序列缺包/重复/乱序及 TS 连续性错误均为 0；全部源视频负载在接收覆盖内 |
| 接收抓包 | 266,575 UDP、抓包丢包 0；EOF 后 283 个无视频负载维护 RTP 未被接收抓包覆盖，其交付状态未验证 |
| 发送节奏 | 50 Mbps 下，完整发送抓包、核心 net_dev_queue 与 net_dev_start_xmit 服务曲线最大超额均为 1,356 B，即一包；无发送突发 |
| 内核队列 | 最大入队至驱动提交 93,000 ns，P99 48,999 ns；trace overrun、输入 socket drops、两路 tcpdump kernel drops 均为 0 |
| 运行错误 | workerErrors=0、errors=0、droppedBuffers=0、graphPayloadPressureFailures=0 |
| CPU / 内存 | 平均单核 CPU 18.948218%，峰值 36.923077%；RSS 初始 52,355,072 B、峰值 58,916,864 B；payload 高水位 109,176,822 B |
| 源驱动结束 | 源 19:11:08.560543600～19:15:17.831692171 自然结束、exit 0；CLI 按既有无输入窗口结束、exit 1；收尾 19:15:30.936110741 |
| A/V 漂移 | VideoOnly，无音频，不适用 |

**源视频链路完整门禁 PASS**，立即独立提交并推送；不声称整个 CLI 生命周期全部 RTP 均获得接收验证。独立 PR 审查者已复核此判定边界：用户要求的源运行期与恢复后三分钟视频持续播放均已覆盖，EOF 后维护尾部证据缺口不冒充媒体丢包或已验证交付。

## 抓包差异根因与边界

初始总包数不等，暂缓判定后逐包定位：接收从发送第 0 包开始，全部 266,516 包严格对应发送前缀；余下 283 RTP 位于目标机 19:15:20.398789～19:15:29.038856，晚于源结束。逐 TS 检查尾部 PID 257 的 payload 标志（AFC 1/3）计数为 0，只有协议维护，没有视频负载。根因是接收 dumpcap 的 275 秒定时结束没有覆盖 CLI 收尾，不是已证实的网络丢包或核心失败。

按对应 RTP 的时间戳差值，接收端时钟较目标机约快 6.053367 秒（含网络与抓包路径时间），不能直接比较两机墙钟。尾部先后关系全部采用发送侧时间；GPU 连续段仍保守限制在源结束数值时间前。下一项将抓包延长至 330 秒，核心与媒体参数不变。

GPU 全局计数查询出现三次样本有效性错误，未保存 Status，不能确认对应实例；本次保存的 VLC VideoDecode 序列有持续正增长的 191.38 秒证据，不宣称所有全局查询均成功。后续采样另保存 Status 字段，便于直接核对选中实例。

发送完整 RTP SHA-256 `40dd8e6855c593f27193961f4183ada50145ebe450a006177ec32b19fbc9cda5`；接收前缀 SHA-256 `a99b5004b0bb0f37d13f1e1233c32a7ff0f44a478d37342221e87a63c97ddc58`。不把两个不同覆盖范围的哈希声称为一致。run41/run42 的旧内核偶发突发风险继续保留。

## PID 与证据

CLI 4123523、源 4123547、主脚本 4123459、注入脚本 4123460、输入/输出 tcpdump 4123503/4123504、trace reader 4123474、发送 TID 4123588；VLC 36848、Windows dumpcap 9912。目标机测试进程均已结束，临时执行脚本已删除、lo 恢复 0%；VLC 已在下一项前关闭。

目标机与本机各自 `out/acceptance/rk-a559-loss20-run45/` 保存实际脚本内容、命令、PID、日志、资源数据、抓包、GPU CSV 与分析结果，包括 `capture-coverage-analysis.json`。诊断工具不入库，库没有下载到本机。
