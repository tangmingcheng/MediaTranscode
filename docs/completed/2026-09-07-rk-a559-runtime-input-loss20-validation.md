# RKMPP 正常启动后注入 20% 输入丢包

run31，生产代码仍为四项零损伤验收版本，未修改核心。按用户要求先正常启动源与核心，再开启输入损伤。结果 **FAIL：注入后停止产生新编码输出，12 秒无进展策略终止 CLI**。本轮与 [启动前注入 run30](2026-09-07-rk-a559-input-loss20-validation.md) 的探测预算失败不同。

## 时间与真实证据

| 事件 | 目标机时间（2026-09-07，UTC+8） | 证据 |
|---|---|---|
| 源开始 | 13:07:54.851991 | `source-start.txt` |
| 首个输出 RTP | 13:07:55.020920 | 出口 pcap |
| 正常输出后开启输入 20% 丢包 | 13:08:14.810722 | `loss-start.txt`；注入前 encodedPacketsPushed=1487、errors=0 |
| 最后一个完整输入 FU NAL | 13:08:26.631044 | 注入后完整 FU 重组分析，type=1 |
| 最后一个输出 RTP | 13:08:28.104006 | 出口 pcap |
| CLI 因无进展失败，开始停止 DAG | 13:08:40.080 | CLI/节点日志；距注入约 25.27 秒 |
| 监控确认 CLI 已退出而源存活 | 13:08:41.710595 | `resources.txt` |
| 人工停止源后结束 | 13:09:09.470866 | `kill -INT 3848390`，源退出 255 |

注入前输入窗口 19.869 秒、21649 个 RTP，序列缺失 0。注入后至 CLI 失败，收到 21173 个 RTP、序列范围内缺失 5263 个，实测 19.9085% 丢包。12 个 IDR 都存在分片缺口，完整 IDR 为 0；完整 FU NAL 只有 4 个，均为 type=1。入口 tcpdump 自身 dropped 为 0；运行时入口接收 42314 个数据报，truncations=0、pressure_failures=0，说明输入损伤与接收截断、存储容量拒绝不同。

出口发送与 Windows 接收均为 17582 个 RTP 与 7 个 RTCP；RTP 逐包聚合 SHA256 均为 `138b9384c061a6c16649494a5197ca7545cfe25f8b6fe86624086bda93d25a22`，缺失、重复、乱序为 0，接收抓包 dropped=0。RTP 序列错误与 TS continuity 错误为 0；最大 RTP 输出间隔 **7.225769 秒**。发送服务曲线超额仍为 1356 B，wire 最大驻留 44.955383 ms，deadline miss=0；这些局部指标不能覆盖长时间断流和播放晚帧，故不判定完整播放通过。

VLC 默认 D3D11VA，日志明确 NVIDIA 硬件解码；本轮有 20 条 `picture is too late to be displayed` 和 9 条 `More than ... late frames` 记录，未掩盖。全部参数与原 CBR 项相同，没有降低源规格、延长超时或使用软件解码。

## 根因与边界

输入随机丢包破坏了绝大多数分片 NAL；完整 IDR 在损伤阶段全部缺失。核心记录持续 `rtp_discontinuity ... sequence_gap`，最终 `encodedPacketsPushed` 停在 2412，队列清空，workerErrors=0。`MediaRealtimeProgressTracker::observe` 在已有输出后只以新增编码包计入进展；接收包、报告缺口或 worker 活动都不代替有效编码输出。

`MediaRealtimeVideoRunController` 按原 `--progress-timeout-ms 12000` 返回 `ProgressTimeout`，最终错误为 `NotInitialized: realtime runtime made no progress before timeout`，CLI 退出 1。最后一次 wire 输出到 DAG 停止约 11.976 秒，与编码进展采样的 12 秒超时相符。停止期间的 `Linux RTP ingress batch receive was cancelled` 是清理结果，不能倒置为初始故障。

本轮已证明：正常启动后遭遇该 20% 输入损伤，当前链路不能持续有效播放，并由已有无进展策略退出。未发现出口丢包、wire 超限、解码器 fatal error 或接收截断证据；不能把损坏 NAL 当完整帧送解码器，也不能仅让进程保持存活冒充播放恢复。上述进展跟踪与超时代码相对基线未修改；依用户“输入源问题给证据、不改核心”的约束，本轮不修改核心，不引入修复库或新参数。[RFC 6184 §5.8](https://www.rfc-editor.org/rfc/rfc6184.html#section-5.8) 支持丢失 FU 后丢弃同一 NAL 后续分片的接收处理。

## 命令与清理

运行脚本内容归档 `out/acceptance/rk-a559-loss20-run31/run-script-content.txt`。源与 CLI 的实际命令如下：

```bash
dir=/home/tang/MediaTranscode/out/acceptance/rk-a559-loss20-run31
tc qdisc change dev lo root netem loss 0%
/home/tang/MediaTranscode/out/build/rk-release/media_transcode_realtime_video_cli --media-id rk-userspace-low-h264720p30-hevc1080p25-cbr6m-v1 --egress-capacity-bps 50000000 --maximum-wire-residence-ms 100 --input-type rtp --output-layout mpegts --output-transport rtp --open-timeout-ms 30000 --read-timeout-ms 2000 --analyze-duration-us 5000000 --probe-size 5000000 --progress-timeout-ms 12000 --video-rtp-url rtp://192.168.130.229:61884 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --rtp-host 192.168.96.122 --rtp-port 6200 --sdp "$dir/output.sdp" --video-codec hevc --rc cbr --bitrate 6000 --width 1920 --height 1080 --fps 25 --gop 50 --no-audio
ffmpeg -hide_banner -nostdin -re -i /home/tang/rk-highspec-hw-hevc2k30-to-h2642k30-248s.mp4 -map 0:v:0 -an -c:v copy -f rtp -payload_type 96 -ssrc 3 -sdp_file "$dir/input.sdp" "rtp://192.168.130.229:61884?pkt_size=1400"
# 确认已有持续输出后执行：
tc qdisc change dev lo root netem loss 20%
```

```powershell
D:\VideoLAN\VLC\vlc.exe --file-logging --log-verbose=2 --logfile="D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-loss20-run31\vlc.log" rtp://@192.168.96.122:6200
D:\Wireshark\dumpcap.exe -i 6 -f "udp and (port 6200 or port 6201)" -a duration:275 -w "D:\Code\MyCode\MediaTranscode\out\acceptance\rk-a559-loss20-run31\receiver.pcapng" -q
```

输入经 lo，输出经 eth0，eth0 全程保持原 fq；lo 的 20% 设置继续保留。CLI PID 3848384、源 3848390、入口/出口 tcpdump 3848366/3848367、运行脚本 3848359、VLC 27508、dumpcap 27968；均已退出。临时 `/home/tang/run-rk-loss20-runtime.sh` 已删除并保留内容。资源数据归档，峰值 RSS 60846080 B；短失败测试不构成内存稳定性验收，VideoOnly 不适用 A/V 漂移。原始 pcap 保留目标机，日志与分析小文件同步本机，不纳入仓库。
