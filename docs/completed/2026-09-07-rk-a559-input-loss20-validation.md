# RKMPP 输入 RTP 20% 丢包测试

版本 `a54ec0c1`，生产代码与四项零丢包验收相同。run30 结果 **FAIL：启动预探测退出，未进入持续转码**。不计入成功验收，不修改核心、不改变 CLI 参数。

## 丢包方向与实测

本地已有 H.264 2560x1440@30 连续文件发往 `192.168.130.229:61884`，目标机路由为 `local ... dev lo`。执行 `tc qdisc replace dev lo root handle 30: netem loss 20%`，返回 0；`tc -s qdisc show dev lo` 确认 `loss 20%`。这是整个 lo 的随机丢包设置，覆盖本轮输入 RTP/RTCP，并非 eth0 入站规则；外部源改走 eth0 时不能沿用此配置证明入口损伤。

最初误设的 eth0 出口丢包已在测试前撤销，恢复原 `fq 8042:`；到播放器 `192.168.96.122` 的路由经 eth0，测试后出口 qdisc dropped 为 0。实际恢复命令：

```bash
tc qdisc replace dev eth0 root handle 8042: fq limit 10000 flow_limit 100 buckets 1024 orphan_mask 1023 quantum 3028 initial_quantum 15140 low_rate_threshold 550kbit refill_delay 40ms
```

输入抓包覆盖 23.503 秒：收到 20251 个 RTP，序列范围内缺失 5084 个，实测损失率 20.0671%；tcpdump kernel dropped 为 0。出口抓包为 0 包，VLC 未收到媒体，不能称为持续解码通过。

## 失败根因

1. 输入第一处序列缺口为 `1203 → 1205`，缺少 `1204`；首包 STAP-A 中的 SPS（26 B）与 PPS（5 B）都已收到，不是参数集缺失。
2. `MediaRawRtpBootstrapPlan::create` 从 `analyze-duration-us=5000000` 得出重排等待 5000 ms，窗口从 5000000/12 得出 416666 包。`MediaRtpReorderBuffer` 在缺口后保留后续包，直至等待到期或窗口溢出；本轮这两个释放条件都未到达。
3. `MediaRawRtpInputPreparer` 在重排前累计接收字节，而帧率事实只观察重排交付的 marker。抓包前 3618 个 RTP 共 4998941 B，加首个 RTCP 28 B，与日志的 `observed_bytes=4998969` 完全吻合；约 3.669 秒已达到探测预算边界，早于 5 秒重排等待。
4. 约 3.700 秒到达的下一条 1400 B 数据报触发 `MediaRawRtpPreparedByteBudget::observeLocked` 的总探测字节上限，返回 `AllocationFailed: raw RTP probe exceeded total byte capacity: observed_bytes=4998969 capacity=5000000`。这是显式预算拒绝，不能解读成操作系统内存分配失败。CLI 退出码 1，监控在源仍存活时记录退出。

因此，本轮直接失败链是输入丢包 → 启动重排等待 → 总探测预算先耗尽。限制位于已有 bootstrap 规划与探测预算的配合，相关代码相对 `a5597326` 未改；本轮不能据此宣称新增持续运行修复回归，也不能宣称支持 20% 损伤启动。

补充证据：预算窗口内三个 IDR 的 FU-A 序列分别至少缺 50、24、19 个分片，首个还缺起始分片；这是真实码流损伤，但并非本次直接退出消息。保留损坏分片丢弃行为符合 [RFC 6184 §5.8](https://www.rfc-editor.org/rfc/rfc6184.html#section-5.8) 的接收建议。未改参数集、未放宽预算、未添加软解或错误隐藏；运行中才注入损伤的行为本轮未覆盖，后续按用户要求另做 [run31](2026-09-07-rk-a559-runtime-input-loss20-validation.md)。

## 实际命令与进程

```bash
dir=/home/tang/MediaTranscode/out/acceptance/rk-a559-loss20-run30
/home/tang/MediaTranscode/out/build/rk-release/media_transcode_realtime_video_cli --media-id rk-userspace-low-h264720p30-hevc1080p25-cbr6m-v1 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --progress-timeout-ms 12000 --video-rtp-url rtp://192.168.130.229:61884 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 192.168.96.122 --rtp-port 6200 --sdp "$dir/output.sdp" --video-codec hevc --rc cbr --bitrate 6000 --width 1920 --height 1080 --fps 25 --gop 50 --no-audio
ffmpeg -hide_banner -nostdin -re -i /home/tang/rk-highspec-hw-hevc2k30-to-h2642k30-248s.mp4 -map 0:v:0 -an -c:v copy -f rtp -payload_type 96 -ssrc 3 -sdp_file "$dir/input.sdp" "rtp://192.168.130.229:61884?pkt_size=1400"
tcpdump -i lo -n -s 0 -U -w "$dir/input.pcap" 'udp and (port 61884 or port 61885)'
tcpdump -i eth0 -n -s 0 -U -w "$dir/output.pcap" 'udp and (port 6200 or port 6201)'
```

```powershell
D:\VideoLAN\VLC\vlc.exe --file-logging --log-verbose=2 --logfile="D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-loss20-run30\vlc.log" rtp://@192.168.96.122:6200
D:\Wireshark\dumpcap.exe -i 6 -f "udp and (port 6200 or port 6201)" -a duration:275 -w "D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-loss20-run30\receiver.pcapng" -q
```

CLI PID 3844613，源 3844618，入口/出口 tcpdump 3844593/3844594，临时运行脚本 3844586，VLC 28500，dumpcap 15560。源开始 `12:58:48.772759517+08:00`；`12:58:52.869004345+08:00` 检测到 CLI 已退出而源仍在发送。随后用 `kill -INT 3844618` 停止源，源结束 `12:59:12.375355591+08:00`，退出码 255；没有先停止 CLI。资源日志保留源期间 CPU/RSS，短运行不形成内存稳定性结论；VideoOnly 不适用 A/V 漂移。

上述 CLI、源、抓包和 VLC 进程均已退出。临时运行脚本 `/home/tang/run-rk-loss20.sh` 已删除，内容归档为 `run-script-content.txt`。抓包及分析证据位于目标机与本机 `out/acceptance/rk-a559-loss20-run30`；大体积原始输入 pcap 保留目标机。输入 lo 的 20% 丢包设置按用户要求保留，出口维持原 fq。
