# 可复用 Datagram 发送控制 Task5 完成记录

## 结论与范围

- 三类实时输出统一为 `protocol materializer → DatagramShaper → common ScheduledDatagramSender`；音频、视频和 RTCP 按 service scope 聚合，文件输出明确排除 datagram 节点。
- Windows 与 RK 均以 1280×720@30、8 Mbps、H.264 raw RTP 输入、HEVC 完整硬件转码完成独立 RTP、MPEG-TS/RTP、MPEG-TS/UDP 六条 30 秒发送门禁。停止 120 秒固定源后，CLI 如实以 source-clock expiry 自主退出 RC=1；该退出不改写为发送成功或 Cancelled。
- 本轮未执行 120 秒持续验收和 56 链路矩阵；二者属于 Task6，不能据此宣称全链路验收完成。

## 事实、规划产品与执行契约

- `MediaRealtimeDeploymentEnvelope` 只接收 scope/authority、address family、权威 MTU/发送上限、受管 service curve、graph/network/socket 总资源预算、local address/port reservation、target/maximum latency、observation budget/evidence policy，以及可选 receiver timing capability。IP/UDP header 和 batch/backlog/endpoint/socket/correlation 容量由 planner 推导。
- encoder preflight 在真实 `avcodec_open2` 后形成 `PreparedEncoderEmissionEnvelope`。effective rate/VBV/cadence 取 opened-context/backend readback；packet layout 优先取 extradata，缺失时用独立 preflight context 编码一个真实 probe frame，从首个非空 `AVPacket` 权威判定 Annex B 或唯一合法 length prefix。缺失、冲突或无法证明时 DAG 前失败，不按 codec 名称猜测，不软件 fallback。
- planner 由 prepared encoder emission、TS mux/RTP/IP/UDP overhead 形成唯一 `WireTrafficEnvelope`，先与受管 service curve 做 admission，再由总资源和 latency 推导 transport、batch、backlog、endpoint、socket、correlation 与媒体 queue 硬边界。datagram 容量不映射为 frame/AU/mux queue。
- TS 协议时序只消费 mux/PCR/output cadence 与 receiver timing；公共 shaper 是唯一 wire-rate authority。MTU 同时推导 TS/UDP 与 MP2T/RTP 的 TS packets/datagram，独立 RTP 再扣除 RTP header；小 MTU DAG 前拒绝。
- sender 固定执行 reservation、release wait、nonblocking submit、原 deadline 内 writable wait、精确 prefix commit。WouldBlock 保留原 job/lease；known submitted prefix 精确提交，unknown remainder 不提交并终止；`enqueueNotAfter` 为 inclusive deadline。
- generation rebind 先完整验证新 plan，再关闭旧 session 并 bind 新 session；失败终态、不回滚。Linux `SO_TXTIME` adapter 保留为 capability，但当前 production planner 只接纳共享 userspace baseline，不运行期探测或降级。

## 参数收口

保留调用方能直接取得的媒体/会话/部署事实：media-id、input/output endpoint、裸 RTP codec/PT/clock/fmtp、显式 stream set、目标 codec/尺寸/fps/GOP/RC/VBV、硬件后端请求、SDP 路径、受管网络与资源/时延/receiver timing 事实。

已从 realtime CLI、Beta 和 request 删除且不保留别名/default/fallback：caller `packet-size`、`output-pacing-bitrate-bps`、旧 `transport-decode-lead`、四类 realtime queue、startup unit/gap、prepared handoff packet/byte capacity、IP/UDP header、batch/backlog/endpoint/socket/correlation 内部容量，以及 input-AU → `SO_SNDBUF`。同时删除 `5/4` headroom、two-packet burst、默认 GOP 30、固定 TS packets/datagram=7 和 Beta 内部传输 profile。`local_video_cli` 四类队列属于独立文件产品且不参与本轮发送控制，列入后置审查而未修改。完整清单见 `docs/realtime-core-parameter-review-baseline.md`。

## 临时 TDD 与生产 RED→GREEN

- prepared readback：request 改变不改变权威 effective readback；缺失/零值/冲突拒绝。Windows CUDA/NVENC opened-context 无 layout 时，真实 probe packet RED 后由共享 preflight adapter GREEN；RKMPP 路径复用同一 adapter。
- wire admission：8 Mbps、30 fps、16 Mbit VBV、MTU 1500 的 MP2T/RTP 计算 sustained 约 1,056,700 B/s、约 780 datagrams/s、burst 2,106,015 B；service sustained 降至 1,000,000 B/s 时 DAG 前拒绝。
- MTU：payload 1200 时 TS/UDP 与 MP2T/RTP 为 6 包；payload 8972 时为 47 包；payload 187 时拒绝。
- runtime/backpressure：真实 bounded channel 首次 push WouldBlock 后保留同一 buffer/lease，capacity wake 后只交付一次；multi-output 不重复；stop/abort 保留首错并 poison 未提交 lease。
- deadline/commit：`now == enqueueNotAfter` 可提交，超过 1 ns 失败；partial/ambiguous 的 known prefix 精确 commit，unknown remainder 不 commit。
- 所有临时 fixture、源码和目标在 GREEN 后删除，未进入版本库。

## 六条 30 秒真实门禁

共同源均为固定 120 秒文件，启动顺序 VLC → CLI → 1 秒后 FFmpeg；30 秒窗口后停止 source，等待 CLI 自主退出，再停止 VLC。共同 CLI 参数为：

```text
--input-type rtp --input-layout separate --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --open-timeout-ms 10000 --read-timeout-ms 3000 --analyze-duration-us 2000000 --probe-size 5000000 --no-audio --video-codec hevc --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --min-bitrate 8000 --max-bitrate 8000 --buffer-size 16000 --gop 60 --egress-scope-kind managed --egress-scope-authority acceptance-topology --egress-address-family ipv4 --egress-maximum-ip-packet-bytes 1500 --egress-sender-maximum-payload-bytes 1472 --egress-sustained-wire-bytes-per-second 20000000 --egress-peak-wire-bytes-per-second 25000000 --egress-burst-wire-bytes 65536 --egress-service-authority managed-lan-reservation --egress-maximum-graph-memory-bytes 16777216 --egress-maximum-network-memory-bytes 8388608 --egress-maximum-socket-memory-bytes 2097152 --egress-resource-authority acceptance-budget --egress-local-port-count 4 --egress-target-residence-ms 20 --egress-maximum-residence-ms 100 --egress-latency-authority acceptance-sla --egress-observation-run-datagrams 1000000 --egress-observation-drain-residence-ms 100 --egress-tx-evidence-policy report --egress-observation-authority kernel-timestamp-report --progress-timeout-ms 10000 --first-output-timeout-ms 15000 --poll-interval-ms 250
```

RK 共同附加参数：

```text
--hardware-backend rkmpp --egress-scope-id rk-lan-gate --egress-mtu-authority eth0-mtu-readback --egress-local-address 192.168.130.229 --egress-local-first-port 51000 --egress-local-authority eth0-owned-range
```

Windows 共同附加参数：

```text
--hardware-backend auto --egress-scope-id win-lan-gate --egress-mtu-authority ifindex16-mtu-readback --egress-local-address 192.168.96.122 --egress-local-first-port 51010 --egress-local-authority ifindex16-owned-range
```

TS 输出共同附加：

```text
--receiver-transport-decode-lead-ms 100 --receiver-startup-emission-preroll-ms 20 --receiver-timing-authority acceptance-receiver-capability
```

| 平台/输出 | 完整 route 参数与 receiver | PID（VLC/CLI/source） | 30 秒窗口结果 |
|---|---|---|---|
| RK MP2T/RTP | binary `/home/tang/task5-521973f6/out/build/rk-release/media_transcode_realtime_video_cli`; `--media-id task5-rk-hevc-mpegts-rtp --video-rtp-url rtp://127.0.0.1:5004 --output-layout mpegts --output-transport rtp --rtp-host 192.168.96.122 --rtp-port 6200 --sdp /home/tang/task5-rk-work-290893f5/task5-output.sdp`; VLC `D:\VideoLAN\VLC\vlc.exe D:\Code\MyCode\MediaTranscode\out\task5-rk-mpegts-rtp-receiver.sdp` | 31488 / 2608679 / 2608890 | 3191 batches，26763 datagrams，32488752 B；WouldBlock/writable/pressure=0；939 AU；peak pending 174652 B；encoded 2817/2817；peak RSS 59248640 B |
| RK TS/UDP | 同一 binary；`--media-id task5-rk-hevc-mpegts-udp --video-rtp-url rtp://127.0.0.1:5004 --output-layout mpegts --output-transport udp --udp-output-url udp://192.168.96.122:6202`; VLC `D:\VideoLAN\VLC\vlc.exe udp://@:6202` | 10832 / 2690366 / 2690370 | 3193 batches，26695 datagrams，32132960 B；WouldBlock/writable/pressure=0；939 AU；peak pending 175968 B；encoded 2817/2817；peak RSS 53833728 B |
| RK elementary RTP | binary `/home/tang/task5-edf30d26/out/build/rk-release-edf30d26/media_transcode_realtime_video_cli`; `--media-id task5-rk-hevc-rtp --video-rtp-url rtp://127.0.0.1:5004 --output-layout separate --output-transport rtp --rtp-host 192.168.96.122 --rtp-port 6204 --sdp /home/tang/task5-rk-elementary-hevc.sdp`; VLC static H265/96/90000 SDP | 24452 / 2790536 / 2790560 | 1000 batches，21755 datagrams，31267794 B；WouldBlock/writable=0；encoded 1878/1878；peak RSS 54726656 B |
| Windows MP2T/RTP | binary `D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_realtime_video_cli.exe`; `--media-id task5-win-hevc-mpegts-rtp --video-rtp-url rtp://127.0.0.1:5004 --output-layout mpegts --output-transport rtp --rtp-host 192.168.96.122 --rtp-port 6210 --sdp D:\Code\MyCode\MediaTranscode\out\task5-win-mpegts-rtp.sdp`; VLC static MP2T/33/90000 SDP | 29172 / 21180 / 33060 | 2935 batches，24566 datagrams，29846576 B；WouldBlock/writable=0；857 AU；peak pending 228044 B；encoded 2571/2571；peak RSS 192716800 B |
| Windows TS/UDP | 同一 binary；`--media-id task5-win-hevc-mpegts-udp --video-rtp-url rtp://127.0.0.1:5006 --output-layout mpegts --output-transport udp --udp-output-url udp://192.168.96.122:6212`; VLC `D:\VideoLAN\VLC\vlc.exe udp://@:6212` | 12260 / 20988 / 33260 | 5746 batches，48182 datagrams，58044060 B；WouldBlock/writable/pressure=0；1678 AU；peak pending 228044 B；encoded 5034/5034；peak RSS 193515520 B |
| Windows elementary RTP | 同一 binary；`--media-id task5-win-hevc-rtp --video-rtp-url rtp://127.0.0.1:5008 --output-layout separate --output-transport rtp --rtp-host 192.168.96.122 --rtp-port 6214 --sdp D:\Code\MyCode\MediaTranscode\out\task5-win-elementary-hevc.sdp`; VLC static H265/96/90000 SDP | 33776 / 31156 / 5356 | 2141 batches，47741 datagrams，65910575 B；WouldBlock/writable=0；encoded 3920/3920；peak RSS 193323008 B |

RK source 精确命令：

```bash
/usr/local/bin/ffmpeg -re -i /home/tang/task5-rk-work-290893f5/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 'rtp://127.0.0.1:5004?pkt_size=1200'
```

Windows source 精确命令（`PORT` 分别为 5004、5006、5008）：

```powershell
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -re -i 'D:\Code\MyCode\MediaTranscode\out\acceptance\test-continuous-120s.mp4' -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 "rtp://127.0.0.1:PORT?pkt_size=1200"
```

六条链路 source 停止后均由 production controller 自主退出 RC=1，首错均为 `RTP video source clock evidence expired`；无残留 VLC、CLI、FFmpeg 或监听端口。该 source-loss 结论与 30 秒 sender gate PASS 分开记录。

## 构建、静态扫描与边界

- Windows VS2026 x64 Debug 最终 clean-first：554/554，RC=0。
- RK exact production code `9a88963b` 在隔离目录 `/home/tang/task5-9a88963b` 以 Release clean-first、`--parallel 8` 完整构建；最终报告提交仅修改文档，不改变构建输入。
- production shape validator 要求唯一 transport-plan source、shared shaper、common sender；三类 materializer 分别是 elementary RTP、MP2T/RTP、TS/UDP。file output validator 排除全部 datagram 节点。
- 旧 caller 参数、第二 pacing authority、固定 7 包、legacy sender/pacer/sink/direct transport、`CompletionGated`、`AwaitCompletion`、`UserspaceSendReturn` 和临时 fixture 均无生产引用。系统调用只存在于 Linux/Windows transmit adapter。

Task5 提交按时间顺序为：`92a87130`、`535bd95d`、`477e1774`、`0de94681`、`c34a5361`、`b0a351f6`、`b121af1b`、`81519a0d`、`019ff629`、`290893f5`、`d1feb9c4`、`c3a23829`、`fedd4c0f`、`8cf42a45`、`27189678`、`b1e6af64`、`6ae26f10`、`68da54a8`、`ce875d9d`、`470ac83a`、`8ace0876`、`521973f6`、`edf30d26`、`2ccbc775`、`d727929d`、`9a88963b`，以及本完成报告提交。

## 剩余风险与后置项

- 120 秒持续运行和 56 链路矩阵尚未执行；Task6 必须覆盖全部 admitted tuple，并对 unsupported tuple 验证 DAG 前 typed rejection。
- TX evidence policy 为 `report` 时，timestamp 缺失只影响证据，不阻止发送；它不是逐包 wire completion。
- 公网自适应拥塞控制、NACK/RTX/FEC 不在本轮范围；当前产品只接纳显式受管/预留 service scope。
- 输入 PCR 失活阈值、open/read/analyze/probe 工作限额、A/V servo/reacquisition 与 `local_video_cli` 文件队列仍需后续独立审查。
