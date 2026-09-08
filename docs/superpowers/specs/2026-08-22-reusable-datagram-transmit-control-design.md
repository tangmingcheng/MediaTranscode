# 可复用 Datagram 发送控制设计

## 目标与裁决

建立一条协议无关、链路无关、跨平台共享语义的 Datagram 输出流水线：

```text
协议物化 -> 公共 pacer/shaper -> 非阻塞 transport -> 异步发送证据
```

本设计废止“每个 datagram 必须等待 TX timestamp/completion 才能提交下一个 datagram”的旧方案。发送正确性只以 planner 生成的服务包络、全局预约顺序、非阻塞原子提交结果和提交期限为权威；TX timestamp、error queue、Winsock timestamp 与 `MSG_ZEROCOPY` completion 只能异步证明或观测平台发送路径，不能释放 credit、推进协议状态、决定下一包提交，也不能成为 DAG 启动前提。

优先验收链路是单视频 raw RTP 输入、RKMPP H.264 转 HEVC、MPEG-TS/RTP 输出；MPEG-TS/UDP 与独立 elementary RTP 必须复用同一公共 shaper 和 sender，不得为 RKMPP、VideoOnly、MP2T 或某个地址建立专属发送链。file output 不是 Datagram transport，保持独立文件写入路径，不接入、不复用也不依赖 Datagram shaper/sender。

## 当前代码证据

- `MediaScheduledDatagramSenderNode` 直接依赖 `MediaProjectMpegTsRuntimePlanBuffer`、`MediaMpegTsRtpDatagramSink` 和 `MediaForwardOnlyDatagramPacer`，尚不是公共 Datagram sender。
- `MediaScheduledDatagramBatchBuilder` 同时接收协议 payload 与 enqueue 预约，协议物化和传输 shaping 尚未形成显式 DAG 边界。
- `MediaScheduledRtpSenderNode` 自己持有 packetizer、RTP/RTCP sender session 和 UDP transport，独立 RTP 未复用 MPEG-TS/RTP 的发送 seam。
- `MediaRealtimeOutputPolicyPlanner.cpp` 以 `5/4` pacing headroom 和 `2` 包 burst 作为经验常量，并把 input access-unit byte bound 传入 `SO_SNDBUF` 规划，混淆输入容量、服务包络和输出提交容量。
- 当前 `UserspaceSendReturn` 只证明成功进入本机 socket 发送路径，不是 wire completion；发送端 pcap 存在而接收端连续缺包的现场证据也不能通过逐包 completion gate 证明或修复网络交付。

## 工业依据与适用结论

- [RFC 8085](https://www.rfc-editor.org/rfc/rfc8085) 要求 UDP 应用控制发送速率、响应拥塞并限制 burst；因此公共 shaper 必须先于 socket，且按 service scope 聚合 RTP、RTCP 与 UDP 流量。
- [WebRTC pacing 模块](https://webrtc.googlesource.com/src/+/refs/heads/main/modules/pacing/)体现生产发送路径以媒体/探测包排队和预算 pacing 控制出队，不以逐包内核完成事件串行化应用层提交。
- [Linux timestamping](https://docs.kernel.org/networking/timestamping.html)通过 `SO_TIMESTAMPING`、`MSG_ERRQUEUE`、`SO_EE_ORIGIN_TIMESTAMPING`异步返回 TX timestamp；它是相关联的观测事件，不是应用层 pacing credit。
- [Winsock timestamping](https://learn.microsoft.com/en-us/windows/win32/winsock/winsock-timestamping)通过 `SIO_TIMESTAMPING`、`SO_TIMESTAMP_ID`、`WSASendMsg`、`SIO_GET_TX_TIMESTAMP`取得异步时间戳，语义同样不能改变跨平台公共执行模型。
- [Linux MSG_ZEROCOPY](https://docs.kernel.org/networking/msg_zerocopy.html) completion 表示用户缓冲区可安全复用，不表示数据已交付到接收端；本轮 payload 已有 RAII 所有权，零拷贝只能作为以后经能力证明的传输优化，不能进入基础正确性路径。

弱网恢复仍是独立产品：RTCP feedback、NACK/RTX、FEC 或拥塞控制必须由协议协商和独立 planner 产品启用，不能用 pacing、TX timestamp 或扩大 socket buffer 冒充。

## 模块边界

### 协议物化层

协议节点消费已调度编码包或完整 TS packet，产出 `MediaWireDatagramBatchBuffer`。每个 descriptor 只包含：

- `generation`；
- planner 分配的 `endpointId`；
- 完整 UDP payload 的 offset/size；
- 上游 canonical release/deadline 事实；
- session 内全局严格递增的 `sequence`。

每个 wire entry 还独占一个不透明、move-only 的 `MediaDatagramSubmitCommitLease`。它封装该 datagram 对 RTP/RTCP sequence/counter、MPEG-TS continuity 或其他协议 reservation 的唯一提交权；不得暴露裸回调、裸指针或依赖进程级隐藏共享映射。shaper 只能把 lease 原样移动到对应 `MediaScheduledWireDatagramBatchBuffer` entry，不能调用、复制或重建。

MP2T packetization、RTP/RTCP header、SSRC、payload type、RTP timestamp、sequence、RTCP SR/BYE、MPEG-TS continuity 和最终 wire bytes 全部留在协议层。sender 只有在同一 datagram 非阻塞原子 enqueue 明确成功后才能对 lease 精确 commit 一次；`EAGAIN`/`WSAEWOULDBLOCK` 未消费 datagram 时保留同一 bytes/sequence/lease 并允许在同一 deadline 内重试。late、短写、delivery ambiguity、stop failure 或未提交 lease 析构都不得推进协议状态，并必须终止 graph。协议层不得创建 socket、等待 timer、计算 token 或选择 pressure 策略。

### Planner 产品

`MediaDatagramShapingPlan` 是唯一发送控制权威，至少包含：

- session key、generation authority 和稳定 `TransportServiceScopeId`；
- 一个或多个类型化 UDP endpoint plan；
- 权威 MTU/maximum IP packet/sender limit 证据和推导后的 `maximumDatagramBytes`；
- `maximumBatchBytes`、本地 backlog、residence、socket memory 与每 endpoint hard bound；
- service rate、peak rate、burst/service curve 及其部署或协商证据；
- `NonBlockingAtomicEnqueue`、`CanonicalOrdered`、pressure/deadline 终止语义；
- 可选 `MediaDatagramTransmitEvidencePlan`，只描述平台能异步采集的 timestamp/zero-copy 证据和相关 ID 范围。

wire sustained/peak demand 必须由 prepared encoder emission envelope、完整 mux/RTP/IP/UDP overhead 与 deployment service evidence 共同规划；caller bitrate 或 VBV 不得直接复制为 transport rate，prepared emission 变化必须改变 wire demand/shaping plan，或因 deployment service 不足在 DAG 构建前失败。input AU bound 或 queue capacity 不得成为 socket buffer、datagram payload、service rate 或 burst。若仅缺 TX timestamp/zero-copy capability，则关闭对应证据采集并如实标记 `unavailable`，不得改变已规划的发送语义。

### 公共 pacer/shaper

`MediaDatagramShaperNode` 消费 `MediaDatagramShapingPlanBuffer` 和 `MediaWireDatagramBatchBuffer`，输出 `MediaScheduledWireDatagramBatchBuffer`：

1. 校验 session、generation、endpoint、payload、global sequence 和 batch hard bound。
2. 在完整 `TransportServiceScopeId` 上维护一份连续 token/debt 或 constant-rate 状态；同一 scope 的 RTP、RTCP 与 UDP job 共享账本。
3. 为每个 datagram 生成 `enqueueNotBefore`、`enqueueNotAfter`、`wireServiceDuration` 和 reservation sequence。
4. demand 超过 rate、peak、burst、backlog、residence 或 deadline 时终止，不提高速率、不 catch-up burst、不丢包、不重排。
5. rate generation 变化时按 planner 的 persistent-state transition 保留或精确迁移 shaper debt，不因 codec、RTP wrap 或 source jitter 重置。

shaper 不创建 socket、不解析 RTP/TS/codec，也不依据发送回调或 timestamp 调整 token。

### 公共 sender 与非阻塞 transport

`MediaScheduledDatagramSenderNode` 只消费 shaping plan 和 scheduled wire datagram：

1. 校验 plan/session/generation/endpoint/reservation sequence。
2. 等待 `enqueueNotBefore`，最迟不超过 `enqueueNotAfter`。
3. 调用 `MediaDatagramTransmitPort::trySubmit()` 做一次非阻塞原子 datagram enqueue。
4. `WouldBlock` 时只等待 socket writable/wakeup 并在原 deadline 内重试同一 datagram；Submitted 后在释放 scheduled batch entry 前对其不透明 RAII commit lease 精确 commit 一次。
5. late、短写、不可分类 socket pressure、endpoint mismatch 或 ambiguous delivery 均为终态失败。
6. stop/abort 中断 timer 与 writable wait，RAII 关闭 socket，保留首个真实失败。

Windows 与 Linux/RK 仅在 `MediaDatagramTransmitPort` adapter 中实现 socket、timer/writable notification 和 OS 错误映射差异。公共 sender 不选择平台策略，不按协议类型分支。

### 异步证据采集

`MediaDatagramTransmitEvidenceCollector` 是旁路观测组件。它以非阻塞方式 drain Linux error queue、Winsock timestamp 或以后经计划启用的 zero-copy completion，并用 planner 分配的 submission/evidence ID 关联成功 enqueue 的 datagram。

- 证据迟到、丢失、乱序或队列 overflow 只降低观测完整度并计数；不得阻塞 sender 或回写 shaper。
- 未关联、重复、越界或跨 generation 证据必须被拒绝并记录，不能伪造 completion。
- close 时只做 planner 限定的有界 drain；未收齐证据仍如实报告，不把正常发送改写为失败或成功交付。
- timestamp 不证明接收端交付；`MSG_ZEROCOPY` completion 只证明 buffer ownership 可回收。

## 线程、所有权、背压与内存

- 协议 materializer、shaper、sender 使用现有 graph worker；不得新增协议专属发送线程。
- `MediaDatagramTransmitSession` RAII 独占 endpoint socket；scheduled batch entry 同时独占 payload 与 `MediaDatagramSubmitCommitLease`，至少持有到 `trySubmit()` 明确成功并 commit，或终态失败后未提交析构。仅启用 zero-copy 时，另一个独立 RAII buffer lease 持有到对应 completion，不能与协议 commit lease 合并。
- materialized channel、scheduled channel、shaper backlog、sender pending job、socket effective buffer 和 evidence correlation table 都必须出现在 planner resource ledger 中并具有 item/byte/residence hard bound。
- 背压沿 DAG edge 传播；发送节点最多持有一个当前重试 job，但吞吐不由“逐包等待 TX completion”人为串行化。
- socket request 与 `getsockopt(SO_SNDBUF)` 有效值都进入 telemetry；有效值不能被当作允许应用 burst 的容量。

## 失败语义

- 不完整 endpoint、MTU、service scope、rate/burst、resource 或 deadline 事实：DAG 前 `InvalidArgument`/`Unsupported`。
- shaping 超出 service/backlog/residence：终态资源或 deadline 失败，不 drop、resize 或提速。
- `WouldBlock` 超过 `enqueueNotAfter`：终态 socket pressure/deadline 失败。
- UDP 短写、未知返回、提交结果不明确：poison session 并保留 ambiguous delivery。
- commit lease 缺失、重复提交、跨 entry/generation 使用或 Submitted 后 commit 失败：终态内部契约错误；不得查找隐藏映射补提，也不得继续发送。
- timestamp/zero-copy capability 与 plan 声明冲突：证据功能在打开阶段失败；基础发送只有在错误同时破坏 transport 时才失败。
- stop/abort：只有 worker-local stop/abort 因果可返回 `Cancelled`；source loss 和真实 I/O 失败不得被覆盖。

## Telemetry

至少输出：planned/actual service scope、rate/peak/burst、shaper token/debt、scheduled/submitted datagrams 与 bytes、reservation lateness、writable waits、pressure/late/short/ambiguous counts、backlog/high-water/residence、socket requested/effective buffer、每 endpoint counters、最后 materialized/scheduled/submitted sequence、evidence requested/available/matched/late/lost/overflow/unmatched、timestamp source，以及明确的 `delivery_evidence=not_proven`。

## 验收门槛

1. Windows clean-first x64 Debug 全量构建成功；RK Release 使用仓库既有流程构建成功。
2. 临时 TDD 证明物化、聚合 shaping、deadline pressure、非阻塞 retry、generation 和异步证据隔离；交付前删除全部测试源码、target 和二进制残留。
3. 完整验收固定为 56 条链路：38 条 VideoOnly + 18 条 AudioVideo。Windows 实跑全部 56 条；RK 对 capability probe/admission 明确支持的链路实跑，对不支持链路以类型化 capability evidence 在 DAG 构建前明确拒绝，禁止运行期 fallback 或静默跳过。
4. 每条 admitted 链路先做 30 秒门禁，再执行固定 120 秒源；覆盖 raw RTP -> H.264/RKMPP -> HEVC -> MPEG-TS/RTP、MPEG-TS/UDP 与独立 RTP，记录 sender/receiver RTP loss/order、TS continuity、burst、shaper backlog、socket pressure、CPU、RSS、退出原因和异步证据覆盖率。
5. 不能降低源参数、码率、分辨率、帧率、时长或转码步骤；每条门禁保留精确 source、CLI、FFmpeg/VLC、端口、PID、清理和进程残留证据，停止源流而不是强停 CLI。
6. SDD 每个 Task 先由一名未参与该 Task 实现的 reviewer 明确 PASS；全部代码冻结后，两名未参与任何实现的独立智能体必须同时 PASS，修复后两者都重新复审；随后提交 PR 并由新智能体给出最终结论。

## 非目标

- 不以本轮改动重新定义 encoder bitrate、CBR/VBR、VBV、GOP、fps、分辨率或 A/V timestamp/PCR 语义。
- 不实现 RTCP NACK/RTX/FEC、公共互联网拥塞控制或动态编码 generation；缺少所选服务所需能力时继续 fail-closed。
- 不把 TX timestamp、sender pcap、`MSG_ZEROCOPY` completion 或 VLC 画面解释为端到端交付证明。
- 不修改网卡、接收端播放器或系统 socket 默认值来替代应用发送控制修复。
- 不把 file output 改造为 Datagram output，也不让文件写入依赖 socket、shaper 或 transmit evidence。
