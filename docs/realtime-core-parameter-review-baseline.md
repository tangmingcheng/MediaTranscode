# Realtime 核心参数审查基线

## 分类规则

| 分类 | 允许来源 | 处理方式 |
|---|---|---|
| 调用方事实 | 真实源、协议会话、设备请求直接给出 | 作为显式 request fact；不得要求调用方推导内部容量或时序 |
| 部署事实 | 部署环境、预留网络、设备能力、资源预算或协商结果直接给出 | 必须携带 scope、有效期和 evidence；缺失时规划失败 |
| planner 产品 | 由前两类事实和权威探测推导 | 只存在类型化 plan；CLI/API 不得手工填写 |
| 后置审查 | 运行控制、诊断、telemetry 与验收观察 | 不参与媒体策略、容量、pacing 或 socket 规划 |

## 当前核心对外参数

| 分类 | 参数 | 当前入口 | 审查结论 |
|---|---|---|---|
| 调用方事实 | media-id | realtime CLI、beta config | 保留；session identity |
| 调用方事实 | input-type、input-layout、input URL/RTSP transport | realtime CLI | 保留；真实源与协议会话事实 |
| 调用方事实 | video/audio RTP URL、codec、payload type、clock rate、channels、fmtp | realtime CLI；beta input 的 bind address/port/codec/PT/clock | 保留；裸 RTP 无法自行权威推导这些协议事实 |
| 调用方事实 | stream set/no-audio | common CLI | 保留；显式 VideoOnly/AudioVideo，不得 bool 推断 |
| 调用方事实 | output-layout、output-transport、destination host/address/port、SDP path、UDP output URL | realtime CLI；beta output | 保留；调用方选择输出协议和目的 endpoint |
| 调用方事实 | output codec、width、height、fps、GOP、rate-control mode、target/min/max bitrate、VBV、quality、preset/profile/tune/level | common CLI；beta output | 保留为编码请求；不得直接复用为 network service/socket 事实 |
| 部署事实 | hardware backend/disable-hw | common CLI、beta fixed profile | 保留但应绑定 capability probe；Auto/default 不得绕过显式平台能力失败 |
| 部署事实 | open/read timeout、analyze duration、probe observation budget、low-latency | realtime CLI | 需纳入输入 observation contract；probe-size 不能复用为 retention/handoff 容量 |
| 部署事实 | MPEG-TS maximum PCR gap | realtime CLI | 保留为源协议时钟 acceptance fact，仅适用于 MPEG-TS 输入 |
| 部署事实 | address family、path MTU/maximum IP packet/sender payload limit、managed service scope/rate/peak/burst、graph/network/socket 总预算、target/maximum residence | realtime CLI、beta deployment envelope | 已收口。IP/UDP header 由 address family 与 UDP 标准推导；批次、backlog、endpoint pending、socket per-endpoint 与 correlation 容量不对外 |
| 部署事实 | receiver transport decode lead、startup emission preroll 及 authority | realtime CLI、beta optional receiver timing capability | TS 输出必需；缺失时 DAG 前失败。独立 RTP 不要求该事实 |
| planner 产品 | `PreparedEncoderEmissionEnvelope`、encoder packet-layout evidence、`WireTrafficEnvelope` | encoder open readback/真实 probe packet、mux/RTP overhead | encoder preflight 在 `avcodec_open2` 后读取有效 target/max/VBV/cadence/private RC；layout 优先取 extradata，缺失时由独立 preflight context 编码真实 probe frame 并解析首个非空 packet。缺失、冲突或无法证明即失败，不按 codec 名猜测。wire demand 只消费 prepared 产品，不复制调用方码率 |
| planner 产品 | maximum UDP payload、TS packets per datagram、RTP/RTCP endpoint plan、socket requested/effective bound | MTU、协议封装、服务曲线与总资源预算 | 已移出 CLI/API。TS/UDP 与 MP2T/RTP 均按 MTU 推导；小 MTU DAG 前拒绝 |
| planner 产品 | queue item/byte/residence、shaper backlog、batch、endpoint pending、socket per-endpoint、correlation bound | prepared emission、媒体 cadence、graph/network/socket 总预算与 latency | realtime 产品已由 planner 独立推导；禁止把 datagram 容量映射为 packet/frame/mux queue 或单 AU 上限 |
| planner 产品 | pacing reservation、wire service duration、enqueue window、service-scope token/debt | `WireTrafficEnvelope` 与 managed service curve | 公共 shaper 是唯一 network rate authority；runtime 不重建，TS schedule 不读取 deployment wire rate |
| planner 产品 | SSRC、RTP base sequence/timestamp、CNAME、RTCP schedule、MPEG-TS PID/continuity/PCR policy | 当前 planner/protocol plan | 保持 planner-owned，不新增对外手工参数 |
| 后置审查 | max-duration、progress-timeout-ms、first-output-timeout-ms、poll-interval-ms | realtime CLI | 仅 runner/验收控制，不得进入 production DAG 媒体规划；正式 120 秒门禁禁用 max-duration |
| 后置审查 | quiet-graph/diagnostic log、event callback/user data | CLI、beta callbacks | 仅诊断与通知，不得改变失败语义或执行策略 |

## 已整改项与后置项

| 原位置/范围 | 原问题 | 当前处置 |
|---|---|---|
| realtime metadata/packet/frame/mux queue CLI | caller 被要求提供内部容量 | realtime CLI/Beta 已删除；媒体 queue 由 prepared emission、latency 和 graph memory 独立规划。`local_video_cli` 属于文件产品且不参与 datagram sender，本轮后置审查，不声称全仓删除 |
| realtime packet-size、output-pacing-bitrate、旧 transport lead | MTU/封装/时序 planner 产品被暴露为裸数字 | 已删除且无兼容别名；MTU/完整 wire overhead 与 receiver timing capability 分别形成类型化事实和产品 |
| startup max unit/gap、prepared-handoff packet/byte capacity | caller 被要求猜测内部启动与 handoff 容量 | 已移出 caller；prepared handoff 由 datagram geometry 和资源 ledger 规划，encoded AU 单包 admission 使用独立媒体总预算 |
| realtime request 的 packet/pacing/startup/handoff 字段 | request 混入 planner 产品 | 已删除；request 只保留可取得事实 |
| `5/4` pacing headroom、two-packet burst、固定 7 TS packets/datagram | 无权威证据的经验常量 | 已删除；service curve/burst 来自受管部署事实，TS packetization 按 MTU 推导 |
| maximum input AU → RTP `SO_SNDBUF` | 输入媒体容量错误替代输出 socket memory | 已删除；socket bound 从 network/socket 总预算、wire envelope、latency 和 endpoint count 推导并校验 effective value |
| datagram backlog/batch → frame/AU/mux queue | 网络容量错误映射为媒体容量 | 已删除；两套容量分别规划。metadata 单槽为类型化 `RetainLatest` 协议语义 |
| Beta 固定 queue、startup AU、packet/pacing/lead profile | 样例位置常量混合协议、资源和传输策略 | 已删除；Beta 与 CLI 使用同一 deployment/receiver facts 和 planner 产品 |
| model 中的 Auto/default bool | 可能以默认成员绕过显式策略 | 不影响本轮 wire envelope/发送控制，列入后置审查；核心 validating factory 仍必须拒绝无法证明的计划 |

## 本轮参数收口门槛

- realtime CLI、beta API、request、planner plan 四层逐字段可追踪，不能同一事实多处独立拥有。
- 全树不存在 caller-provided queue、handoff、packet-size 或 input AU -> SO_SNDBUF。
- 全树不存在 caller-provided startup gap；若链路需要该约束，必须能追溯到真实源探测证据或类型化源时钟契约。
- network service、MTU、resource/residence 缺少 scope/evidence 时 DAG 前失败。
- caller encoder 参数只进入 encoder planning；transport planner 只消费 prepared encoder emission envelope 与完整 wire overhead，不能直接复制 caller bitrate/VBV。TX timestamp/MSG_ZEROCOPY 只影响 evidence telemetry；后置审查参数不改变生产 DAG。

## Task5 review round1 结果

- `MediaRealtimeDeploymentEnvelope` 对外只保留 scope/authority、address family 与 MTU 证据、managed service curve、graph/network/socket 总预算、local address/port reservation、target/maximum latency、observation budget/evidence policy，以及可选 receiver timing capability。
- 已删除 caller IP/UDP header、backlog、batch、endpoint pending、socket per-endpoint 和 correlation entries；CLI/Beta 不保留别名或默认回填。
- `PreparedEncoderEmissionEnvelope` 来自实际 encoder open 后的有效 readback；selected request contract 仅用于冲突校验。`WireTrafficEnvelope` 再加入 TS mux、RTP、UDP/IP overhead 并执行 checked arithmetic 与 service admission。
- encoder packet layout 不再按 codec 名称硬编码；opened-context extradata 不足时，由独立 preflight context 的真实首包形成 Annex B/length-prefix evidence，无法唯一证明则 DAG 前拒绝。
- metadata queue 的单槽位是类型化 `RetainLatest` 语义；其他 realtime media queue 按 cadence、latency 与 graph memory 规划。`local_video_cli` 的文件队列参数列入后置项。
- known submitted prefix 仍精确 commit，unknown remainder 不 commit 并终止；`enqueueNotAfter` 保持 inclusive，等于 deadline 可提交，超过 1 ns 失败。
