# 可复用 Datagram 发送控制 Task5 完成记录

## 结论与边界

- Task5 的 production planner、三类实时输出公共拓扑、发送状态机和参数收口已完成。
- RK 单视频 raw RTP → RKMPP H.264 → MPEG-TS/RTP 的 30 秒 sender gate 通过；这不是 120 秒持续验收，也不是 56 链路全矩阵通过。
- 停止源后，CLI 保留真实源时钟失败并自主退出 RC=1：`RTP video source clock evidence expired`。该结果与 30 秒发送窗口分开记录，不改写为成功或 Cancelled。
- 文件输出不使用 datagram sender；独立 RTP、MPEG-TS/RTP、MPEG-TS/UDP 均使用 `protocol materializer → DatagramShaper → common ScheduledDatagramSender`。

## 生产契约

- `MediaRealtimeDeploymentEnvelope` 只接受受管 service scope、MTU 及证据、服务曲线、资源/批次/socket 硬边界、local endpoint range、目标/最大 residence 和异步 TX evidence budget。
- planner 形成唯一 `WireTrafficEnvelope`、`DatagramTransportPlanTemplate`、activation 和 shaping 产品；local bind 仅来自 deployment endpoint 事实。
- 协议 materializer 只产生最终 wire datagram、全局顺序、canonical release/deadline 和 opaque ordered commit lease。公共 sender 不感知 codec、RTP 或 MPEG-TS。
- sender 固定执行 reservation、release wait、nonblocking submit、deadline 内 writable wait、精确 prefix commit；WouldBlock 保留 lease，partial/ambiguous fail closed。
- 三类 materializer 共用 planner batch datagram/byte/deadline partition；单 datagram 不拆分，跨片 lease 保持全局顺序，abandon/gap poison generation。
- Linux 仅在 platform adapter 使用 `sendmsg`，Windows 仅在 platform adapter 使用 `WSASendMsg`；已删除旧 direct socket/sink/transport/open transaction。

## 核心对外参数审查

完整基线见 `docs/realtime-core-parameter-review-baseline.md`。Task5 收口后的分类如下。

| 归属 | 对外事实/产品 | 结论 |
|---|---|---|
| 调用方事实 | media-id、输入类型/布局/endpoint；裸 RTP codec/PT/clock/channels/fmtp；显式 VideoOnly/AudioVideo；输出布局/协议/目的 endpoint/SDP；目标 codec、尺寸、fps、GOP、RC、bitrate/VBV、quality/profile/tune/level；硬件后端请求 | 保留。裸 RTP 会话事实不能靠收包猜测；编码请求不得复用为网络容量事实。 |
| 部署事实 | egress scope/authority；MTU/IP/transport/payload 上限及证据；sustained/peak/burst service；backlog/batch/endpoint/socket resource budget；local address/port range；target/maximum residence；observation run/correlation/drain/evidence policy | 保留为类型化 envelope。必需事实或 authority 缺失时 DAG 前失败。 |
| planner 产品 | UDP payload/TS packetization、wire rate/overhead、burst/debt、canonical deadline、所有 queue item/byte/residence、prepared handoff、socket/batch/endpoint hard bound、RTP identity/RTCP schedule、TS PID/continuity/PCR | 不允许调用方手工传入；由真实源/prepared encoder readback、协议 overhead 和 deployment facts 推导。 |
| runner/观察 | progress timeout、first-output timeout、poll interval、诊断输出和 callback | 仅控制 runner/观察，不进入 wire、媒体或容量规划。 |

已从 CLI/Beta/request 删除且不保留别名或 fallback：`packet-size`、`output-pacing-bitrate-bps`、`transport-decode-lead`、四类 queue、startup unit/gap、prepared-handoff packet/byte capacity，以及 input-AU → `SO_SNDBUF`。同时删除 `5/4` headroom、two-packet burst、默认 GOP 30 和 Beta 内部传输/容量 profile。

本轮不扩大的后置项：输入 PCR 失活阈值，open/read/analyze/probe 工作限额，A/V servo/reacquisition 参数，公网自适应拥塞控制及 NACK/RTX/FEC。若后续证明其中任一项进入 wire envelope、deadline 或资源容量，必须先转为类型化事实并在 DAG 前校验。

## 静态证据

- production shape validator 要求一个 transport-plan source、一个 shared shaper、一个 common sender，并校验 materializer 的 wire/plan edge；三种实时输出分别选择 elementary RTP、MP2T/RTP、TS/UDP materializer。
- file output shape 明确排除 datagram plan/shaper/sender/materializer。
- 全树零命中：旧 caller 三字段、`--packet-size`、旧 RTP sender config/local-port policy/I/O policy、旧 sender port/transport/open transaction、TS UDP sink、`CompletionGated`、`AwaitCompletion`、`UserspaceSendReturn` 和 `sendTo`。
- 允许的发送系统调用只有 Linux transmit adapter 的 `sendmsg` 与 Windows transmit adapter 的 `WSASendMsg`。
- 临时 TDD 均已删除，仓库未加入测试源码、测试目标或测试基础设施。

## RK 30 秒首验

接收端命令：

```powershell
& 'D:\VideoLAN\VLC\vlc.exe' 'rtp://@:6200'
```

RK CLI 命令：

```bash
/home/tang/task5-68da54a8/out/build/rk-release/media_transcode_realtime_video_cli --media-id task5-rk-h264-mpegts-rtp --input-type rtp --input-layout separate --video-rtp-url rtp://127.0.0.1:5004 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --open-timeout-ms 10000 --read-timeout-ms 3000 --analyze-duration-us 2000000 --probe-size 5000000 --output-layout mpegts --output-transport rtp --rtp-host 192.168.96.122 --rtp-port 6200 --sdp /home/tang/task5-rk-work-290893f5/task5-output.sdp --no-audio --video-codec h264 --hardware-backend rkmpp --rc cbr --width 1280 --height 720 --fps 30 --bitrate 8000 --min-bitrate 8000 --max-bitrate 8000 --buffer-size 16000 --gop 60 --egress-scope-kind managed --egress-scope-id rk-lan-gate --egress-scope-authority acceptance-topology --egress-mtu-authority eth0-mtu-readback --egress-maximum-ip-packet-bytes 1500 --egress-ip-header-bytes 20 --egress-transport-header-bytes 8 --egress-sender-maximum-payload-bytes 1472 --egress-sustained-wire-bytes-per-second 20000000 --egress-peak-wire-bytes-per-second 25000000 --egress-burst-wire-bytes 65536 --egress-service-authority managed-lan-reservation --egress-maximum-backlog-datagrams 4096 --egress-maximum-backlog-bytes 8388608 --egress-maximum-residence-ms 100 --egress-maximum-batch-datagrams 32 --egress-maximum-batch-bytes 65536 --egress-maximum-endpoint-pending-datagrams 256 --egress-maximum-endpoint-pending-bytes 1048576 --egress-socket-hard-bound-bytes 2097152 --egress-resource-authority acceptance-budget --egress-local-address 192.168.130.229 --egress-local-first-port 51000 --egress-local-port-count 4 --egress-local-authority eth0-owned-range --egress-target-residence-ms 20 --egress-latency-authority acceptance-sla --egress-observation-run-datagrams 1000000 --egress-observation-correlation-entries 4096 --egress-observation-drain-residence-ms 100 --egress-tx-evidence-policy report --egress-observation-authority kernel-timestamp-report --progress-timeout-ms 10000 --first-output-timeout-ms 15000 --poll-interval-ms 250
```

120 秒固定源命令：

```bash
/usr/local/bin/ffmpeg -re -i /home/tang/task5-rk-work-290893f5/test-continuous-120s.mp4 -map 0:v:0 -an -c:v copy -bsf:v h264_mp4toannexb -f rtp -payload_type 96 'rtp://127.0.0.1:5004?pkt_size=1200'
```

启动顺序为 VLC → CLI → 1 秒后 source；30 秒后停止 source，等待 CLI 自主退出，再停止 VLC。PID：VLC 11996、CLI 1957506、source 1957940，清理后无残留。

30 秒窗口结果：3312 batches、26758 datagrams、32498656 bytes；WouldBlock=0、writable wait=0、TX evidence submitted=26758；MPEG-TS pressure failure=0、late=0、最大 lateness=0；939 AU，peak pending=169576 bytes；worker error=0、drop=0、peak queued=66；RSS 约 48.7→53.7 MiB，采样 CPU 13.1%。停止源约 9 秒后 source-clock expiry，CLI 自主 RC=1。

## 构建与提交证据

- Windows VS2026 x64 Debug clean-first：553/553 target，RC=0。
- RK exact `ce875d9df57cfc39b387c5f499c2fcfdd729b63a`，隔离目录 `/home/tang/task5-ce875d9d`，Release clean-first、8 核：552/552 target，RC=0。
- Task5 收口提交：`92a87130`、`535bd95d`、`477e1774`、`0de94681`、`c34a5361`、`b0a351f6`、`b121af1b`、`81519a0d`、`019ff629`、`290893f5`、`d1feb9c4`、`c3a23829`、`fedd4c0f`、`8cf42a45`、`27189678`、`b1e6af64`、`6ae26f10`、`68da54a8`、`ce875d9d`。

## 剩余风险

- 120 秒持续运行、独立 RTP、MPEG-TS/UDP 和 56 链路矩阵属于 Task6，当前不得宣称全链路验收。
- TX evidence policy 为 `report` 时，timestamp 缺失不阻止发送；这提供异步证据而非逐包 wire completion。
- 公网自适应拥塞控制、NACK/RTX/FEC 尚未实现，当前仅适用于显式受管/预留 service scope。
- RK abort/源失活路径仍可能打印 MPP buffer-pool 清理警告；需在 Task6 长跑与退出清理证据中继续审查。
