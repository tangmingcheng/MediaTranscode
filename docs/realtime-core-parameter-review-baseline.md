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
| 部署事实 | path MTU/maximum IP packet/sender payload limit、transport service scope/rate/peak/burst、资源与 residence budget | 当前缺少完整类型化入口 | 本轮应新增部署 envelope；每项必须带 evidence，不接受裸经验数字 |
| planner 产品 | maximum UDP payload、TS packets per datagram、RTP/RTCP endpoint plan、socket requested/effective bound | 当前部分由 packet-size 或代码推导 | 移出 CLI/API，由 MTU、packetization 和平台 probe 推导 |
| planner 产品 | queue item/byte/residence、startup AU acceptance、prepared handoff、shaper backlog、batch byte bound | 当前部分由 CLI 或 beta profile 填写 | 移出调用方媒体请求；由 deployment resource/source acceptance facts规划 |
| planner 产品 | pacing reservation、wire service duration、enqueue window、transport lead、service-scope token/debt | 当前部分由 pacing bitrate/lead 手工输入 | 形成唯一 shaping plan；runtime 不重建 |
| planner 产品 | SSRC、RTP base sequence/timestamp、CNAME、RTCP schedule、MPEG-TS PID/continuity/PCR policy | 当前 planner/protocol plan | 保持 planner-owned，不新增对外手工参数 |
| 后置审查 | max-duration、progress-timeout-ms、first-output-timeout-ms、poll-interval-ms | realtime CLI | 仅 runner/验收控制，不得进入 production DAG 媒体规划；正式 120 秒门禁禁用 max-duration |
| 后置审查 | quiet-graph/diagnostic log、event callback/user data | CLI、beta callbacks | 仅诊断与通知，不得改变失败语义或执行策略 |

## 当前不合理参数与硬编码

| 位置 | 当前内容 | 问题与处置 |
|---|---|---|
| tools/common/VideoCliTranscodeOptions.h:27-30 | metadata/packet/frame/mux queue CLI | 调用方手工填写 DAG 内部容量；删除 CLI，改由 resource planner 产品 |
| tools/realtime_video_cli/main.cpp:185-192 | packet-size、output-pacing-bitrate-bps、output-transport-lead-ms | packet-size 是 MTU/封装推导产品；后两者是无 scope/evidence 的裸数字。以 deployment transport envelope 替换 |
| tools/realtime_video_cli/main.cpp:235-263 | startup max unit/gap、prepared-handoff packet/byte capacities | gap 可在类型化源时钟契约保留；unit/handoff capacities 不应由调用方手算，移入 source/resource planner |
| src/internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h:51-66 | packetSize、pacingBitrateBps、transportDecodeLeadMs、startup/handoff capacities | 当前 request 混入 planner 产品；按上述分类拆分 |
| src/internal/graph/planner/realtime/MediaRealtimeOutputPolicyPlanner.cpp:16-18,31-65 | 5/4 pacing headroom、2 packet burst | 无部署或协议证据的经验常量；删除，以 service curve/burst evidence 规划 |
| src/internal/graph/planner/realtime/MediaRealtimeOutputPolicyPlanner.cpp:37-56,204-228 | maximum input AU -> RTP SO_SNDBUF | 输入媒体容量错误替代输出 socket memory；删除关联，按 endpoint datagram 与 kernel budget 规划并验证 effective value |
| src/internal/graph/planner/realtime/MediaRealtimeQueueCapacityPlanner.cpp:9-12 | 1/256/256/256 queue ceilings | 硬编码内部容量且无 topology/resource evidence；改由 complete resource ledger 推导 |
| src/media_transcode_beta/MediaRealtimeBetaFixedProfile.cpp:8-33 | queue、4 MiB startup AU、30 s/2 s/5 MB、1328 packet、timeout 等位置式常量 | 样例/版本 profile 混合协议、资源、传输与审查参数；拆成显式事实或 planner 产品，禁止继续以匿名位置常量承载 |
| include/media_transcode_beta/realtime.h:117-118 | transport_pacing_bitrate_bps、transport_decode_lead_ms | 可取得的部署意图被压成无 scope/evidence 裸值；替换为类型化 transport service/decode contract |
| src/internal/graph/model/MediaTranscodeParameters.h:110-132 | Auto/default bool 与 queue=0 默认成员 | 审查每项是否为策略默认；核心请求必须显式或由 validating factory 生成，不能用零值/Auto 推断合法计划 |

## 本轮参数收口门槛

- realtime CLI、beta API、request、planner plan 四层逐字段可追踪，不能同一事实多处独立拥有。
- 全树不存在 caller-provided queue、handoff、packet-size 或 input AU -> SO_SNDBUF。
- network service、MTU、resource/residence 缺少 scope/evidence 时 DAG 前失败。
- encoder 参数只影响编码产品；TX timestamp/MSG_ZEROCOPY 只影响 evidence telemetry；后置审查参数不改变生产 DAG。
