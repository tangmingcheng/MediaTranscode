# Realtime 核心参数审查

## 判定原则

对外参数只能表达调用方可直接取得的真实源、协议会话、产品意图或部署事实。编码器 readback、封装开销、wire rate、burst、队列、socket 与 batch 等内部产品必须由 planner 推导。无法权威推导时在 DAG 前失败，不使用经验默认值。

工业对照采用 WebRTC 公共 pacer 的聚合 token/debt 与不追赶积压策略、[RFC 8085](https://datatracker.ietf.org/doc/html/rfc8085) 的聚合限速和突发抑制、[RFC 8899](https://datatracker.ietf.org/doc/html/rfc8899) 的 DPLPMTUD 能力边界、FFmpeg send/receive 的有界状态机，以及 GStreamer 有界 queue/backpressure 语义。Windows 与 Linux 均从 connected UDP socket 取得系统 PMTU 估计；发送 socket 明确启用 PMTUD，运行期不降级为 IP fragmentation。

## 当前允许的对外事实

| 类别 | 参数 | 结论 |
|---|---|---|
| 会话标识 | `media-id` | 保留；调用方资源标识。 |
| 拓扑意图 | input type、output layout/transport、VideoOnly/AudioVideo | 保留；input layout 已由 input type 唯一推导，不再要求调用方重复描述。VideoOnly/AudioVideo 仍不得由收到的首包或 bool 猜测。 |
| 输入会话 | URL、RTSP transport、裸 RTP codec/PT/clock/channels/fmtp、bind endpoint | 保留；裸 RTP 不携带足以权威恢复全部会话描述的信息。H.264/HEVC fmtp 可由同 socket prepared probe 自动探测，探测失败即拒绝。 |
| 输出资源 | destination host/port/URL、SDP 路径 | 保留；属于调用方选择的目标资源。 |
| 视频产品意图 | codec、width/height、fps、GOP、RC mode、bitrate | 保留；GOP、RC mode 与 target bitrate 必须显式给出。CBR 只接受 target，VBR 接受 min/target/max。codec、尺寸和 fps 省略时只能从 prepared 真实源权威继承。 |
| 音频产品意图 | codec、sample rate、channels、RC 与 bitrate | 保留；仅 AudioVideo 可用。 |
| 观测工作预算 | open/read timeout、analyze duration、probe size | 保留；它们限制 I/O 等待和观测资源，无法从尚未打开的源推导，不得参与发送 wire/socket 容量。 |
| 受管出口容量 | `egress-capacity-bps` | 保留；单位 bit/s，表示本次同一 service scope 内所有视频、音频、RTP/RTCP/UDP 可共同使用的受管/预留出口上限，不是编码码率或网卡 LinkSpeed。当前验收部署事实为 50 Mbps；planner 只用于 admission 与 queue-drain 上限，实际 pacing 仍由 prepared emission、协议开销和队列产品决定。 |
| sender 服务期限 | `maximum-wire-residence-ms` | 保留；单位 ms，表示每个最终 datagram 从 canonical release 到必须完成非阻塞 socket submit 的最大允许时间。planner 用它做 burst-drain admission、backlog/内存上界和 immutable deadline；它不是接收端缓存、网络往返时间或端到端播放延迟。 |

## 当前 realtime CLI 精确输入合同

| 条件 | 参数 | 必填性与真实意义 |
|---|---|---|
| 所有链路 | `--media-id` | 必填；调用方分配的媒体会话/资源身份。 |
| 所有链路 | `--input-type url|rtp|mpegts-udp` | 必填；选择真实输入会话类型。内部 input layout 由此唯一推导。 |
| 所有链路 | `--output-layout separate|mpegts` | 必填；目标协议物化布局。 |
| 所有链路 | `--output-transport rtp|udp` | 必填；目标 datagram transport；`separate+udp` 不受支持。 |
| 所有链路 | `--egress-capacity-bps` | 必填；本请求已预留/受管的聚合 egress service 上限，单位 bit/s；当前验收事实为 `50000000`。 |
| 所有链路 | `--maximum-wire-residence-ms` | 必填；canonical release 到 socket submit 的 sender 服务期限，单位 ms。 |
| 所有链路 | `--open-timeout-ms`、`--read-timeout-ms` | 必填；分别限制可控启动/open 工作与单次网络 I/O 等待。 |
| 所有链路 | `--analyze-duration-us`、`--probe-size` | 必填；限制 prepared 输入事实探测的时间与字节工作量，不参与 egress 容量计算。 |
| 所有链路 | `--rc cbr|vbr`、`--bitrate`、`--gop` | 必填；目标视频 RC、target kbit/s 与 GOP 帧数。realtime 不接受 Auto/CRF/CVBR。 |
| VBR | `--min-bitrate`、`--max-bitrate` | 必填；与 target 共同满足 `min <= target <= max`。CBR 禁止这两项。 |
| 目标视频 | `--video-codec` | 可省略；省略时仅可从 prepared 源继承 codec，不做运行期猜测。 |
| 目标视频 | `--width` 与 `--height` | 成对可选；省略时从 prepared 源继承。 |
| 目标视频 | `--fps` | 可选；省略时从 prepared 源继承权威 cadence。 |
| URL 输入 | `--input` | 必填；真实输入 URL。raw RTP、UDP 与 SDP URL 不属于该类型。 |
| RTSP URL | `--rtsp-transport` | 必填；RTSP lower transport 会话事实。非 RTSP URL 禁止传入。 |
| raw RTP 视频 | `--video-rtp-url`、`--video-rtp-codec`、`--video-rtp-payload-type`、`--video-rtp-clock-rate` | 必填；裸 RTP 会话无法从包本身权威恢复的 endpoint/codec/PT/clock 事实。 |
| raw H.264/HEVC | `--video-rtp-fmtp` | 可选；省略时使用同 socket prepared in-band SPS/PPS/VPS 探测，无法证明即失败。 |
| raw RTP AudioVideo | `--audio-rtp-url`、`--audio-rtp-codec`、`--audio-rtp-payload-type`、`--audio-rtp-clock-rate`、`--audio-rtp-channels` | 必填；音频裸 RTP 会话事实。AAC 还必须有 fmtp；Opus 当前明确禁止 fmtp，避免静默忽略尚未实现的参数。 |
| MPEG-TS/UDP 输入 | `--input`、`--mpegts-max-pcr-gap-ms` | 必填；UDP URL 与输入 PCR 时钟失活策略。后者不参与 sender 规划。 |
| RTP 输出 | `--rtp-host`、`--rtp-port`、`--sdp` | 必填；远端 RTP/RTCP endpoint 与调用方选择的 signaling artifact 路径。 |
| UDP 输出 | `--output` | 必填；远端 `udp://` endpoint。RTP endpoint/SDP 参数禁止传入。 |
| VideoOnly | `--no-audio` | 当前 CLI 必须显式传入；核心收到的 stream set 始终是显式枚举。CLI 省略该 flag 仍表示 AudioVideo，属于待收口的旧布尔界面；替换为新的显式 stream-set 参数前需用户批准。 |
| AudioVideo 目标 | `--audio-codec`、`--audio-rc`、audio bitrate/sample-rate/channels | 按目标是否需要音频重编码选择；VideoOnly 禁止任何音频控制。copy 与 encode 只能在 prepared 源后由 planner 判定，不能为无关路径强制填写。 |
| 仅 CLI runner | `--max-duration`、`--progress-timeout-ms`、`--first-output-timeout-ms`、`--poll-interval-ms` | 不进入核心 request/sender；分别控制 CLI 停止门禁与 liveness 观测。它们不是 Datagram 发送参数。 |
| 核心诊断意图 | `--quiet-graph` | 进入核心 request 的 `diagnosticLogEnabled`，由视频、音频和硬件 capability planner 消费，只控制 graph 诊断日志；不参与 Datagram wire、pacing、queue、socket 或 deadline 规划。 |

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
| input layout | 已从 CLI、request 与 Beta profile 删除；`url`、`rtp`、`mpegts-udp` 分别唯一推导 session-described、separate-stream、muxed-TS 内部布局。 |
| RC Auto 与输入码率回填 | realtime 视频只接受显式 CBR/VBR；删除从输入平均码率回填目标码率的 fallback。CBR 必须显式 target，VBR 必须显式 min/target/max。 |
| 固定 sender mode 字段 | 删除只被构造和校验、从未被 sender/session 分支消费的 submit/order/pressure/deadline/persistent mode 枚举与字段。非阻塞、全局有序和 fail-closed 现在是公共 sender 类型本身的唯一语义。 |
| 重复 service/socket/latency 产品 | 删除 transport template 重复 pacing、deployment 重复 socket budget，以及 shaping service curve 中未执行的 target residence/release jitter 副本；保留并执行唯一 deployment service、network ledger、backlog deadline 与协议 timing 产品。 |

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
