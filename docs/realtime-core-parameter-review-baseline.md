# Realtime 核心参数审查

## 判定原则

对外参数只能表达调用方可直接取得的真实源、协议会话、产品意图或部署事实。编码器 readback、封装开销、wire rate、burst、队列、socket 与 batch 等内部产品必须由 planner 推导。无法权威推导时在 DAG 前失败，不使用经验默认值。

工业对照采用 WebRTC 公共 pacer 的聚合 token/debt 与不追赶积压策略、[RFC 8085](https://datatracker.ietf.org/doc/html/rfc8085) 的聚合限速和突发抑制、[RFC 8899](https://datatracker.ietf.org/doc/html/rfc8899) 的 DPLPMTUD 能力边界、FFmpeg send/receive 的有界状态机，以及 GStreamer 有界 queue/backpressure 语义。Windows 与 Linux 均从 connected UDP socket 取得系统 PMTU 估计；发送 socket 明确启用 PMTUD，运行期不降级为 IP fragmentation。

## 当前允许的对外事实

| 类别 | 参数 | 结论 |
|---|---|---|
| 会话标识 | `media-id` | 保留；调用方资源标识。 |
| 拓扑意图 | input type/layout、output layout/transport、VideoOnly/AudioVideo | 保留；不得由收到的首包或 bool 猜测。 |
| 输入会话 | URL、RTSP transport、裸 RTP codec/PT/clock/channels/fmtp、bind endpoint | 保留；裸 RTP 不携带足以权威恢复全部会话描述的信息。H.264/HEVC fmtp 可由同 socket prepared probe 自动探测，探测失败即拒绝。 |
| 输出资源 | destination host/port/URL、SDP 路径 | 保留；属于调用方选择的目标资源。 |
| 视频产品意图 | codec、width/height、fps、GOP、RC mode、bitrate | 保留；CBR 只接受 target，VBR 接受 min/target/max。未指定的尺寸、fps 或 target 只有在真实源或 opened encoder 能权威给出时才可继承。 |
| 音频产品意图 | codec、sample rate、channels、RC 与 bitrate | 保留；仅 AudioVideo 可用。 |
| 观测工作预算 | open/read timeout、analyze duration、probe size | 保留；它们限制 I/O 等待和观测资源，无法从尚未打开的源推导，不得参与发送 wire/socket 容量。 |
| 受管出口容量 | `egress-capacity-bps` | 保留；来自部署网络服务，不等同于编码码率。planner 仅用于 admission 上限。物理 link speed 只有在产品明确接纳整条链路为受管服务时才是权威容量事实；它不能推导某个预留 service。Windows 复验中未经证据支持的 25 Mbps 上限被 drain-rate admission 拒绝，随后只读路由证据确认 100 Mbps 物理出口并以整链路受管事实复验；实际 pacing 仍由媒体与 queue 产品决定，而不是按 100 Mbps 发送。 |
| wire 驻留 SLA | `maximum-wire-residence-ms` | 保留；属于产品/部署时延约束，planner 用它验证 prepared burst 是否可服务。 |

## 已从 realtime 对外入口移除

| 原字段 | 当前处理 |
|---|---|
| path MTU | 删除 `path-mtu-bytes`；planner 取系统 connected-path PMTU 与所选出口接口 MTU 的较小值，形成同一 MTU/packetization 契约。对本机地址，接口 MTU 防止系统 65535 local-route PMTU 生成巨型 UDP datagram；无法取得两项平台证据时 DAG 前失败。 |
| receiver transport decode lead | 删除 `receiver-transport-decode-lead-ms`；接收端 caching 不再参与发送端 RTP/MPEG-TS 决策。planner 以 immutable maximum wire residence 形成 sender transport lead，协议 preparation lead、cadence 与 startup preroll 继续内部推导。 |
| hardware backend、disable-hw | planner 根据真实 capability probe 选择最高评分链路；不允许调用方指定或运行期软件 fallback。Beta 的 `selected_backend` 仅是结果 telemetry。 |
| packet size、pacing bitrate、transport lead | planner 从 MTU、opened encoder emission、RTP/RTCP/MPEG-TS/IP/UDP 开销与部署事实形成唯一 wire 产品。 |
| metadata/packet/frame/mux queue | planner 从 prepared emission、媒体 cadence、驻留与资源 ledger 推导。 |
| sender backlog/batch/socket/correlation、prepared handoff packet/byte capacity | planner 形成硬边界；调用方无法可靠计算。 |
| startup unit/gap/preroll | 协议与 transport timing planner 产品；不接受调用方填数。 |
| video quality/preset/tune/profile/level/B-frame/global-header | 已从 realtime core request 类型删除；由所选 encoder 与 realtime/output contract 决定。 |
| audio quality/preset/profile | 已从 realtime core request 类型删除；由 audio planner 和 output protocol contract 决定。 |
| low-latency bool | 已从调用方与 Beta profile 删除；realtime 输入类型本身形成 planner 产品。 |

`MediaRealtimeRtpTranscodeRequest` 已使用 realtime 专用窄参数类型，不再复用 local 的 `MediaTranscodeParameterSet`。planner 派生 queue、resolved audio 和 encoder 私有产品不会回写外部 request 副本。

## 本轮后置项

- MPEG-TS input maximum PCR gap 是源时钟失活策略，不参与 wire envelope、deadline 或 sender 容量；后续需按 MPEG-TS 规范和真实 PCR cadence 单独收口。
- A/V startup、reacquisition、servo 与 runner progress/first-output/poll budgets 不参与 Datagram 发送控制；后续按对应行业状态机独立审查。
- Beta fixed profile 的 probe/runtime budgets 是固定产品策略，不是 sender 参数；后续应单独验证其来源与适用边界。

## 发送控制审查门槛

- 三种输出只能生成最终 wire datagram，再汇入同一 service-scope shaper/sender；协议层不得读取部署 pacing rate。
- shaper 使用连续 token/debt，不做 deadline rebase、积压追赶或运行期扩容；视频、音频、RTCP 聚合消费同一服务曲线。
- sender 只在原始 inclusive deadline 内非阻塞提交；`WouldBlock` 仅等待 writability 后重试，partial/ambiguous delivery 均终止 graph，已知成功前缀精确 commit。
- `send` 成功和 socket buffer 大小不等于 wire delivery。TX timestamp 仅异步记录证据，不参与逐包 credit。
- 验收必须同时给出真实 CLI/FFmpeg/VLC 命令、接收端日志、抓包 burst/loss/order/TS continuity、CPU/RSS/queue 与退出原因。
