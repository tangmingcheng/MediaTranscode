# RTCP BYE 与合屏尾媒体排空调查

状态：设计及基础接口切片已获双独立审查通过；BYE接线和尾媒体排空尚未完成，有界purge取证已实施并运行。原始实测证据及命令见 [rk456 验证记录](realtime-composition-rk456-validation.md)。

## 已证实与待证边界

rk456 输入 AAC 共 5168 AU；aggregate 最终 `real_samples=5226496`，相当于 5104 个 1024 样本块，差 64 AU。输出尾部与参考的对应关系显示尾音未全部出现，但总数差不能单独证明具体丢弃位置。

原日志 1344–1354 行已归档：18:46:19.441，source2 出现 `reason=1`、clock generation 2 invalidated generation 1，随后 `clock_invalidated → purging → binder_purge → purge_completed`。音频 binder cutoff=5193、视频 cutoff=3623。此事件比最终 caller stop 早约 48 秒，不支持直接归因最终 stop drain。1669 行记录真实样本数；1732 行记录音频入口 `completed_datagrams=5185`。入口数据报数并非 AAC AU 数，不能直接相减。

rk456单独证据尚不足以将64 AU全部归属某一owner；后续rk457已直接证明aggregate清理持有尾媒体，见下节。

## 官方依据及适用范围

- [RFC3550 §6.2.1](https://datatracker.ietf.org/doc/html/rfc3550#section-6.2.1) 指出 BYE 后仍可能到达迟延 RTP，成员应先标记离开再删除；这是成员管理规则，不提供应用播放排空时长。[§6.6](https://datatracker.ietf.org/doc/html/rfc3550#section-6.6) 的 BYE 不包含最后 RTP 序号或媒体时间，不能据此构造无损媒体终点。
- [WebRTC RTCPReceiver::HandleBye](https://webrtc.googlesource.com/src/+/refs/heads/main/modules/rtp_rtcp/source/rtcp_receiver.cc) 清除 RTT、报告和 FIR 状态，没有直接调用播放队列 flush。该函数证明控制状态清理与媒体缓存销毁可以分离，不证明所有上层 teardown 都保留缓存；查询时 blob 为 `5498f6870f8fcd377b83a74cde89c9cef43cb341`。
- [GStreamer jitterbuffer 源码](https://raw.githubusercontent.com/GStreamer/gstreamer/main/subprojects/gst-plugins-good/gst/rtpmanager/gstrtpjitterbuffer.c) 的 `queue_event` 将 EOS 作为串行事件追加到队列；`gst_rtp_jitter_buffer_sink_event` 的 FLUSH_START 独立立即执行 flush。[官方 EOS 说明](https://gstreamer.freedesktop.org/documentation/plugin-development/advanced/events.html) 同样区分处理缓冲数据后转发 EOS。可复用的是有序排空与取消分离，不能声称 BYE 自动等于 EOS。

## 当前共享实现落点

- `src/internal/graph/protocol/rtp/MediaRtpSourceUnavailable.h`：`SenderLeft` 为枚举值 1。
- `src/internal/graph/protocol/rtp/MediaRtcpSenderReportTracker.cpp:190`：匹配 BYE 后立即 `invalidatePrepared` 并发布 `MediaRtpClockInvalidation`。
- `src/internal/graph/nodes/sync/MediaRtpPacketClockBinderNode.cpp:412`：`acceptClock` 接收失效快照，交给 clock session。
- `src/internal/graph/sync/MediaAvEpochTransitionService.h`、`.cpp:53`、`.cpp:89`、`.cpp:229`：现有 activation、输出许可和代次转换；当前单一 `outputPermitted` 不能独自表达禁止新绑定而允许旧媒体排空。
- `src/internal/graph/nodes/sync/MediaContinuousAudioAggregateNode.cpp:88`：`servicePurge` 取消旧代 pending、清 `m_audio`、清输入 channel，并完成 purge 确认。
- `src/internal/graph/nodes/sync/MediaContinuousAudioAggregateBuild.cpp:100`：仅源 snapshot 允许输出且有 playback epoch 时复制真实媒体，否则生成静音。

以上属于共享 DAG，不是 RKMPP 编码器独立路径；Windows 存在同类潜在影响，不能以其先前验证通过排除此边界。无需修改 Windows FFmpeg。

## rk457直接实证

rk457 已在原规格实流确认：BYE purge 前 aggregate 持有同旧代 55 个候选、56320 样本；原区间包络 `[5225728,5282048)`，映射包络 `[5225635,5281955)`，`next_output_sample=5226496`，`committed_real_samples=5225635`，`pending_cancelled=0`，映射 incomplete=0，且无 deadline 失败。这证明已绑定、仍由 owner 持有的尾媒体被 purge 清除；不能再仅称测量嫌疑。56320 包括可能已部分消费的候选头部，不能整体宣称未播放；包络也不替代逐区间连续性证据。

修复必须关闭整个已接受媒体流水线，再回收代次；仅绕过 aggregate 清理仍会被输出许可撤销、上游 purge 或解码缓存清除破坏。

## 已实施的有界取证

`MediaContinuousAudioAggregatePurgeObservation.cpp` 在 source purge 清理前输出一条摘要，包含owner候选数量/样本账本、同旧代原始与映射sum/min/max包络、next输出位置、pending真实贡献及已提交real总量；复用audioOffset。关闭State日志时不扫描；溢出/映射缺失标incomplete，日志异常不改变生产返回值。无媒体复制、持久计数或公开参数。pending可能与候选重叠，包络不证明连续性，记录不覆盖输入channel或其他owner。后续仅补关闭frontier/成员完成的有界摘要，不增加逐帧日志。
## 整源组关闭实施方案

本方案不新增成员子代。原 `realtime-video-composition-plan.md`“目标与约束”要求各输入源独立；`realtime-composition-source-membership.md` 明确同一 A/V 源共享 startup/generation/epoch。现有 `MediaAvGenerationTransitionPlanner.cpp:visitProcessingChildren` 和 `MediaAvGenerationTransitionCoordinator.cpp:begin` 也是整组处理。因此选择：任一 SenderLeft 触发该源当前组代有界关闭，两个成员分别封闭接收、排空已接受媒体后复用整组 purge/reprepare。其他输入源不受影响；不承诺组内音频重启时视频绝不中断。

### 表一：成员状态受整源组关闭协调

| 状态/事件 | 明确动作及下一个状态 |
|---|---|
| 组Active，音频首次有效BYE，视频仍发送 | 建立一个组关闭身份和固定截止；音频原因SenderLeft、视频原因GroupClose，两个成员均进入ClosingIngress。视频在截止前仍可接纳，但不能无限发送拖住关闭 |
| 重复BYE | 同关闭身份幂等，截止不续期；不建立第二个关闭事务 |
| 原代包到达 | 使用真实receivedAt，严格小于截止且原入站合法性成立才纳入旧代；等于截止不纳入。不能按下游处理时间判定 |
| 到达组关闭截止 | 各接收owner固定自己的accepted frontier，分别在前序完整媒体之后发布EndGeneration；变为DrainingAccepted。未BYE成员不是SenderLeft，不伪造BYE |
| 音频先Drained、视频仍Draining | 音频按原规则静音，视频继续提交已接受候选；不立即整组purge |
| 视频先Drained、音频仍Draining | 视频黑屏，音频继续提交已接受尾音 |
| 两个要求成员均Drained | 整组进入原purge/ack，所有旧codec/lineage/candidate归零后reprepare/acquire；纯视频源只需视频确认 |
| 新音频提前重入 | 经原ingress身份/序号判据确认后进入有界待交接区，绝不送旧codec；组仍按固定关闭流程结束，再以原完整成员准备条件重新激活 |
| 同SSRC重启/身份不明 | 复用已有ingress重启判据；不能仅凭BYE后到包猜新代。同关闭窗口旧包不得误升新代 |
| 错误、强失效、显式取消 | 原取消/Poisoned优先，保留错误；不为了尾排空放宽deadline |

关闭事务采用既有source group上下文、旧playback generation和原成员边；控制buffer的generation必须遵守所在边的代次域，不能全图直接填playback generation。关闭成员记录仅保留stream、原SSRC/ingress generation、原因(SenderLeft或GroupClose)、frontier/完成位，不生成成员子代。截止只从已授权input_latency产品checked推导；各成员用对应prepared等待事实，无经验常量。

### 表二：实际原图逐边holder排空条件

路径依据 `MediaSourceInputConnectionsPlanner.cpp`、`MediaRealtimeAvSyncInputSegmentBuilder.cpp`、`MediaAudioBranchConnectionsPlanner.cpp` 及 transition planner参与者列表；下列控制均走原媒体FIFO，时钟事件不能替代串行屏障。

| 边与holder | EndGeneration何时可转发/完成 |
|---|---|
| RawVideo/RawAudio → ProtocolVideo/ProtocolAudio | ingress固定frontier；已接受完整AU/重组及待发交易完成后转发；不把RTCP到达次序伪装跨socket总序 |
| protocol binder → VideoGate/AudioGate | acquiring旧代包、video lookahead、pending输出按原映射全部结算；最后lookahead必须走已有可靠终止处理，不能造duration；gate保留旧关闭身份的授权 |
| VideoGate → VideoDecode → decoded VideoStartup（或原interval路由） | send-null/receive排空、registry/retained input完成；新代不能进入该drained codec |
| AudioGate → AudioStartup；两路startup → coordinator | 原startup held/release交易完成后转发成员关闭事实；组已同时关闭两成员，不等待永远不存在的永久EOF |
| coordinator → PlaybackEpochBinder → ActivationReleaseSequencer → BoundReleaseExtractor | coordinator收齐成员关闭并完成held/release后，形成整组屏障；复用单transaction FIFO及extractor原子成员广播。旧activation、pending多边交易先成功提交；不得先撤销旧播放许可 |
| released audio → AudioDecode → AudioStartupTrim | 原decoder drain与lineage/discard-padding结算；trim缓冲完成；metadata端不伪造媒体EOS |
| trim → AudioDriftController → AudioResample | 原pending correction交易及样本完成；SWResample drain发完内部延迟；停止为封闭旧代产生新补偿命令后再转发 |
| decoded video/preparation → optional filter/frame-rate → video aggregate | 各frame/pending holder完成后转发；末显示帧复用 `MediaAvContinuousAggregateNode.cpp:445–456`：最后decodedSequence等于lastDisplayCommit才移除并确认本代drained，不永久sealDisplayInput，不造末帧duration |
| resample → selected audio aggregate | 标记只封闭当前源代；候选消费、pending真实贡献提交后ack，不调用永久输入EOF；未选中音频由原discard消费者确认前序释放 |
| source aggregate → composition encoder/scheduler/mux | 源标记不成为输出EOF。source完成以aggregate提交为界，已提交合屏包由原输出FIFO继续排空；其他源与输出时钟持续 |

同步修改终止ledger和控制分类，EndGeneration为Continue且携带组关闭身份；不能依靠现有Flush在aggregate被忽略的行为。所有实际optional节点必须由planner生成明确支持合同，不新增平行处理链。

### 表三：旧新槽位与容量交接

| 现有槽位/权威容量 | 旧代关闭时 | 新代准入与释放顺序 |
|---|---|---|
| ingress预分配datagram/重组、prepared ingress budget、原edge policy | 保存窗口内已接受媒体到frontier完成 | 同一容量，不复制第二套；新身份不得覆盖仍需排空的旧重组状态。以现有待处理数据报/压缩AU槽承接，容量满沿原明确背压/失败合同，不默默丢包 |
| `MediaRtpPacketClockSession::m_acquiringPackets` / `m_acquiringCapacity` | 旧未绑定包先完成；已绑定包已在原FIFO | 旧集合排空前不拿同槽伪装新队列。由binder以关闭身份隔离，旧包移出后同容量承接已确认新代压缩AU；总数同上限，无第二容器 |
| decoder preparedInputRetention、codec内部输出、lineage registry | 仅一套旧codec与在途工作 | 旧组全部成员drain确认后原purge/reset释放；再reprepare并允许新AU进入。禁止并行decoder/双resampler |
| startup held/release、sequencer/extractor pending、各edge queue | 完成旧组原子交易与控制标记 | 持有新AU的前置gate暂不发布到旧startup；旧purge完成后才调用原startup/reprepare；不越过atomic提交 |
| trim/drift/resample/correction既有sourceBounds | 完成旧样本与命令 | 旧事务确认、原组purge归零，然后复用同owner新状态 |
| aggregate m_audio / maximumAudioCandidates与maximumAudioCandidateSamples | 消耗旧尾部；pending可能与候选重叠 | 确认真实提交后旧组回收；新代只能经新activation进入同候选容器 |
| video source.video、lastDisplayCommit、aggregate pending | 最后decodedSequence已成功提交后释放 | 整组回收后复用相同容量，其他输入源容器不动 |
| 关闭事实/标记 | 每源一个组关闭记录、每成员固定frontier/完成位 | 实际sizeof加入metadata/owner计费；旧记录释放后方可提升下一关闭身份，无历史代列表 |

旧新交接不能把所有reacquisition清理都推迟后仍让无限新包累积。选择复用现有有界前置槽、原ledger及新代等待期限，等待过期/容量不足按原明确失败语义；不声称能无损容纳任意长新流。planner必须把关闭期间旧尾部与待交接压缩AU的同时占用纳入现有总预算，采用实际现有容量与源事实证明，不新设经验槽数。若实际raw ingress重组不能在原槽保留两身份，则在其边界先完成旧已接受重组，再允许新身份重组，不能临时添加第二解码链。

本方案满足原“各源独立”的范围：被关闭的是一个源组，其他源持续；旧尾部按已有时间线提交后黑/静音，整个源重新准备并恢复。组内无缝独立重建不属于本修复承诺。

### 控制载体与代次不变量

不新增平行身份/边界类。采用现有 `MediaControlBuffer` 追加 EndGeneration 枚举，并复用其已有 generation（表示当前边的媒体代次域）；source group 来自节点既有绑定上下文，成员来自承载标记的原输入port/edge。`MediaControlBuffer::initialize` 与 `classifyMediaControlBuffer` 均要求 streamKind=Control，不能把它直接改为Audio/Video来偷带成员身份。

成立条件由现有源协调owner强制：一个已发布source playback generation最多发生一次Active→Closing；首次记录固定首BYE时刻/截止，重复通知幂等；只有完成该代drain和原purge后才能进入更高generation。由此 `(既有group, buffer.generation, 原member edge)` 足以标识关闭，不需要给每个control添加closeSequence或新成员子代。

关闭观察时刻、截止、成员frontier与完成位只在现有源协调owner保留一个固定状态记录；复用既有MediaRtpIngressBinding与frontier布局，不新建平行身份框架。旧控制进入FIFO时固定generation，下游必须与被持有的旧关闭记录核对；不得在消费时从可变的新activation补回旧身份。实际需要扩容的只有这份owner状态及现有frontier合同，按真实sizeof计费；普通MediaControlBuffer对象大小无需增长。实际startup coordinator之后只有单release/transaction通道：coordinator须收齐实际成员的有序关闭并确认全部held/release已提交，才形成唯一整组EndGeneration；extractor沿既有原子广播恢复各成员控制。组屏障证明所有上游成员前序完成，不等同下游aggregate已提交，也不伪造成员BYE。

最终设计交叉审查：pacing_diagnostic_review、remaining_acceptance_audit 均 DESIGN PASS，可按切片开始实现。实施核查接收代与播放代身份、固定截止与真实receivedAt、全holder成功提交后的串行标记、旧purge对新代前置数据的cutoff、代内完成而非永久EOF、停止/首错优先。设计通过不代表代码或验收完成。

### 基础接口实施与代次核验

基础切片新增非永久EndGeneration、每源单一关闭状态、不可变frontier、逐成员完成确认和SourceDrained回收校验。强失效与取消仍清除关闭状态；尚无生产入口发出新标记。完整Epoch owner实际sizeof按源数加输出域计入既有aggregate固定准入费用，不宣称覆盖全进程RSS或所有分配器开销。两位独立审查均给出基础切片SOURCE PASS；构建与实流结论另行记录。

源码核验纠正全程playback身份假设：MediaRtpPacketClockProjector返回snapshot.groupGeneration，PacketClockSession将其写入SourceAccessUnit lineage，前置VideoDecode registry因此属于clock-group域；成员ingress generation另与calibration核对。EndGeneration在绑定前沿用当前边clock-group代次，在原startup/activation/bound-release边界依据被冻结的关系转换为旧playback代次，之后才使用playback generation。禁止假设三域数字相等，或放宽retained-control精确校验。此项是后续接线必需条件，当前基础未启用BYE。

### 2026-11-17：生产接收入口核验（尚未接线）

rk462 已完成聚合器确认；本节仅记录继续实施所需的当前代码证据，不代表 BYE 修复完成。

- `MediaRawRtpPreparedInputBuffer::captureRuntimeBatch` 的 RTP/RTCP 两个线程都调用 tick；`m_deliveryMutex` 只序列化单数据报准入，不覆盖整批。RTCP 超时或批后 tick 不能证明另一线程的 RTP 批次已交付，credit retry 也不能封 frontier。关闭边界必须由本成员 RTP 接收 owner 提交；跨截止批次需要在逐项准入前处理边界，避免首个新代数据报等待 purge、而 purge 又等待批后封口的环形等待。
- Linux adapter 在 recvmmsg 返回后给整批标注同一用户态观察时间；Windows adapter 在 IOCP completion/同步 recv 取得数据后逐项标注。现有 observedAtNanoseconds 不是内核到包时间，不得声称保全截止前已到网卡但尚未被 adapter 观察的数据。两平台复用同一截止算法，仅接收 adapter 不同。
- prepared source candidate 明确要求 DeclaredInputLatency；`MediaRtpInputWaitPlan::finalNanoseconds` 对该模式保持调用方等待期限，源 stream 的 reorder.maximumDelay 来自该产品。关闭期限可以追溯到已授权部署事实，不能替换成读取超时或出口预算。
- tracker 的 prepareRtcp 是尚未提交的破坏性 delta；BYE 分流必须发生在 commitControl 前。该函数会提交协议失效并清 pending，后续 clockGroup invalidation 会清双成员 mapper，binder 又会清 lookahead。只在 binder 忽略 BYE 无法恢复已丢状态。
- prepared source 在 takeClockGroup 后已不再拥有 clockGroup；其 mutex 内不得直接调用 graph/coordinator。需要冻结旧成员协议事实，通过明确交接传给现有源组生命周期 owner；不能保存 borrowed prepared transaction 跨等待。
- EndGeneration 必须进入原 packet FIFO，位于已接受 AU 后，且控制对象费用纳入原容量合同。现有 packets/controls 分队列，控制队列先消费不能充当媒体屏障。
- `purgeMedia` 当前删除全部 Packet 项并 resetAssembly；新代若提前塞入同一队列会被误清。必须先明确新数据报在原 receiver arena 中等待的状态、唤醒及失败期限，再开放新代准入；不得新增无界队列或第二套 codec。
- binder 目前只接 EOF/Flush；trim 也只识别这两种控制，须补 EndGeneration。binder 的既有 interval 尾包处理使用 lastPositiveVideoDelta，不能将该历史间隔称为真实末帧 duration；SourceDisplayPoint 与 interval 合同需分别核对。

依据：[GStreamer 串行 EOS 与非串行事件](https://gstreamer.freedesktop.org/documentation/plugin-development/advanced/events.html)、[RFC 3550 BYE](https://www.rfc-editor.org/rfc/rfc3550.html#section-6.6)。RFC BYE 本身不提供跨 RTP/RTCP socket 的已消费媒体 frontier。source_drain_review_b 对整体方向 PASS，但明确上述代次、锁序、截止、容量与交接约束未落实前不能判入口实施完成；接收边界细节继续由 source_drain_review_a 复核。

source_drain_review_a 已完成边界复核：采用 observedAt < 截止准入旧代，首个 observedAt >= 截止的项在等待之前封口；无未处理批次的 RTP owner 批末/接收超时/下次接收前亦可提供边界。封口不等于排空，pending、重组和输出交易结算后才发布 EndGeneration。时间单调性由 MediaRtpIngressStorage::commitReceivedSlot 校验，原 batch lease 持有待交接项，无新增媒体队列。

### 2026-11-17：冻结映射与最终退休接口复核

source_drain_review_a 建议由 clockGroup publish 通过 observeRuntimeClockGroup(snapshot) 向 prepared source 复制固定 groupGeneration 及各 Stream 的 ingress generation/SSRC。沿原 mutex 保护，现有 storageFacts 的实际 sizeof 覆盖；不保存整个 buffer 或 calibration（含 CNAME），不延长未计费分配生命周期。此接口仅证明观察到映射，不证明 graph 发布或 activation 已完成；关闭建立仍须在 source mutex 外取得 generation arbitration。非 Locked 清新关闭可用映射，已冻结事务不被覆盖；同代不能替换成员身份。

source_drain_review_b 对最终协议退休方向 DESIGN PASS：Tracker 增加 prepareMemberRetirement(expectedIngressGeneration, expectedSsrc, reason, observedAtNs)，复用 prepareContinuityChange(true) 和原 commitControl 原子预留/提交；InputClockSession、ProtocolSession 只转发。原因仅 SenderLeft 与追加 GroupClosed；后者不伪造 BYE 或 SSRC 变化。SenderLeft 由原departure事实产生，GroupClosed使用组关闭真实时刻。

purgeMedia 需要显式表达尚未完成（例如 optional frontier）：control FIFO 对象或字节信用不足时不提交、不确认purge；重新准备而非跨等待保存borrowed事务。每成员退休位防重复递增，双成员都提交invalidation前不放行新媒体。各成员新证据排在原FIFO退休事件后，不能清controls或另建旁路。只resetAssembly不会推进tracker或清clockGroup，不能作为新代交接完成证据。身份已被强失效改变时须按原退休/取消授权处理，不能假装正常关闭成功。

### 2026-11-17：生产接线复核补充（消费屏障尚待实施）

source_drain_review_a 确认锁顺序必须是 publication arbitration → source mutex。复制weak group后先解source锁，再取arbitration；source锁内登记完整不可续期关闭意图，锁外begin/seal，重入锁后核事务和首错。未激活保持原BYE行为；已激活但映射未匹配不能一概当启动期。任何borrowed prepared delta不得跨解锁/等待保存。

source_drain_review_b 对“退休事件只要两路入队即可放行”的候选实现判DESIGN FAIL：两成员控制边没有全局FIFO，音频迟到的退休失效可能清掉已建立的视频新映射。修正约束为每成员记录退休事件精确ingress sequence及新ingress generation，由clockGroup成功消费对应失效后回执；两路消费回执齐全才释放新代门。非Locked snapshot不能替代精确回执。旧控制积压继续沿原序消费，不能清空或停止control endpoint。本项尚未实现，不得报告生产关闭方案已完全通过。

receiver新增generation-wait状态时，accepted必须显式匹配AcceptedReady/AcceptedWaitingCredit，不再用“非WaitingControl”判断；否则ordinal错误推进。未接受数据报保持原batch，telemetry不重复。等待仍推进原clock expiry、截止和取消，避免过期deadline零超时自旋。RTP owner的capture边界与已接受媒体排空分开；截止处不完整AU须明确处理。

BYE compound可能含此前SR/CNAME更新；延迟整个delta前必须优先处理rejection，并明确这些更新的保留语义，不能无意丢失强校验。purge需区分退休已提交、失效已消费和最终完成，信用不足不ack；所有状态固定双成员，不新增媒体队列。

### rk471：关闭入口事务与协议尾部事实

生产入口采用短 Opening 事务：先在 source mutex 内 Preserve prepare，复制 owned departure 后销毁 delta；释放 source 锁，取得 publication arbitration 再重新取 source 锁，对同一个 receiver batch 数据报重新 prepare、验证身份及信用。锁内原子提交 compound 的 SR/CNAME/evidence、冻结双成员 request/原截止并推进 ordinal；释放 source 锁但保留 arbitration，调用 beginSourceDrain；最后锁内核同一 Opening，成功转 Closing，失败保留首错并唤醒，不回滚已接受 compound 或重放 BYE。Opening 不跨信用等待，重复 BYE 不续期。接线前还须核 receiver delivery mutex 与组路径的锁序。

关闭期间继续真实 expiry；SequenceGap、时钟失效或取消仍走强失效，不隐去错误。RTP owner 封捕获边界后，先 drain 原 protocol pending，再按真实时间 expire reorder 并继续 drain。不得传虚构未来时间强制排空。只有完整 AU 成功进入原 packet FIFO后，才检查尾部组装。

新增只读 drainSnapshot 返回实际 reorder 数量、protocol pending 数量与 typed assembly Empty/Incomplete。H264/HEVC 检查 AU/timestamp 与 NAL fragment 状态，AAC 检查 fragmented AU，Opus 无跨包 holder。该事实不代表 receiver batch、信用 lease、输出 FIFO或下游已排空，也不证明历史未丢弃。残缺尾 AU 不自动拼成完整媒体，不通过 resetAssembly 清掉完整待发数据。最终关闭策略必须显式记录残缺结果，不能把残缺当无损排空。

依据：RFC 6184 第5.8节 FU 组装、RFC 3640 第3.2.3.1节 AU 分片，以及既有 GStreamer 串行 EOS 模型。生产关闭入口仍未启用；此处是下一阶段接线约束，不是运行验收结果。

锁序复核：continuous Raw purge不进入receiver discardReplayMedia；progressPurge在无activation仲裁锁时调用children，receiver stop在join前释放自身mutex，当前未发现反向等待。新增接线应持delivery排序锁但在receiver mutex外执行source.consume，随后重取receiver锁记录已提交Accepted frontier；期间停止也不能抹去接受事实或重放。原batch生命周期由调用栈及stop/join保证，禁止持组仲裁调用receiver stop/join。

### rk476–477：接收owner事实接口已落地
prepared source已绑定一次性weak runtime group；RTP owner独占推进exclusive-before观察水位，当前项未接受不计入水位之前。水位跨代连续，不等价于已接纳frontier或完整AU排空。后续不再单独启用局部BYE行为：Opening冻结、capture截止gate、原FIFO EndGeneration、pending purge与双成员精确退休消费屏障须作为一个行为闭环实现及验收。关闭等待必须把固定截止纳入receive/credit唤醒并防止过期零超时自旋；本次水位写入尚无通知消费者。

### 完整Closing接线前复核：代次、末尾证据与唤醒

代次域已由source_drain_review_a核实：RTP locked snapshot.groupGeneration经SourceClockProjection、PacketClockProjector与startup unit进入MediaPlaybackEpoch，当前链路明确同域。关闭事务须在source锁外取得generation arbitration及clockEvidence，再锁source，核activated epoch、clockEvidence generation、m_runtimeClockGeneration三者一致且Target/outputPermitted；BYE ingress generation/SSRC另与对应member精确匹配。仅观察Locked snapshot不等于activation已发表。

末尾证据经source_drain_review_b核实：Incomplete合并FU残片、完整NAL未marker形成AU和timestamp/parser holder，不能统称损坏FU。RFC6184第5.8节与RFC7798第4.4.3节不许可从该enum补造完整AU，也不要求一律终止源。候选关闭合同为先确认capture cutoff，按真实时间排完reorder/pending并使完整AU进入原FIFO；剩余组装显式记录terminal_incomplete、未证明完整而在关闭时丢弃，不称无损。既有SequenceGap/expiry/取消/协议错误优先，不得被该末尾处理覆盖。此合同尚待接线实流证明。

发现控制容量等待的实际唤醒缺口：takeOutput释放control FIFO后仅notifyCredit，后者仅通知receiver；普通旧SR出队不会触发retirement消费回执，因此等待退休入队的Raw可睡住。完整接线必须同时notify对应stream.wakeup。observeConsumedRetirement已通知双成员Raw及receiver，无需新增线程/通知设施。Raw收到optional pending purge必须保留请求并等待，不能publish frontier/complete或返回普通媒体处理；退休提交须记录精确tuple确保幂等。

本次只读核查，无新构建或媒体通过结论，不把文档当功能完成。下一实现覆盖同一闭环，不提前启用Preserve BYE。

source_drain_review_a对上述完整Closing尾合同明确DESIGN PASS；普通完整源若出现terminal_incomplete，必须报告尾部损失，不能视作完整媒体保留通过。source_drain_review_b独立确认同一能力边界及control credit唤醒缺口。尚无实现或运行PASS。


### 完整Closing生产接线：实现待审与实流验证

本次将有效BYE接入Opening→Closing→原FIFO EndGeneration→整组purge→双成员退休消费回执。Opening在发布仲裁外复制departure，仲裁内重新prepare并核激活代、clockEvidence、观察代及成员SSRC/入站代；原compound的SR/CNAME先提交，首个截止由各成员既有reorder.maximumDelay形成且不续期。source锁内不调用group。

RTP owner独占capture fence，跨截止数据报保留原batch和ordinal；关闭等待仍执行真实clock expiry与reorder到期，停止与强失效优先。完整pending/AU进入原packet FIFO后才封frontier并追加EndGeneration。剩余Incomplete明确记为terminal_incomplete后清理，不宣称完整或无损尾媒体。控制对象使用原packet对象/字节信用，不新增媒体队列。

purge携带既有完整transition身份，并缓存已处理frontier防重试清新数据；正常关闭按SenderLeft/GroupClosed提交各成员精确退休，双成员对应sequence及invalidation实际消费后才开新代门。强失效取消正常关闭，仍走原purge。新增固定记录由source/Stream实际sizeof计费。截止与reorder唤醒纳入现有等待，过期capture截止不作为零超时循环。

本节为实现范围说明，尚未完成独立代码双审、RKMPP构建与原规格真实流验收。待核：正常完整源不得出现terminal_incomplete；尾媒体提交数量/时间范围、自然停止/持续输出、重新入流、强失效取消、双成员退休消费等待及内存/漂移均需运行证据。共享Windows路径存在影响；本轮不修改其FFmpeg且未在本机运行CLI，不能报告Windows运行通过。


独立审查首轮为SOURCE FAIL，发现capture前置校验、退休背压闭环和tick早返通知三个问题。本轮修正为：统一validateIngressLocked，先校验原数据报再推进capture；单成员退休已入原FIFO且旧媒体已清理后即返回其purge frontier，允许binder/gate结束purge并继续消费clock；双精确消费回执独立保留新RTP/SR准入门，由observeConsumedRetirement推进完成。tickLocked统一比较实际packet/control发布与准备revision，在早返路径也通知对应消费者；无进展不重复通知receiver credit。修正仍待双审复审及真实流验证。


第二轮复审仍为SOURCE FAIL：即使不等待receipt，等待退休control入队信用仍与clock fanout/purge形成环；取消也不能豁免已入队的退休事件。本轮进一步解耦：Raw只清旧媒体、冻结真实purge frontier及授权并完成自身ack，完全不等待退休队列信用；原receiver tick在publication arbitration→source mutex顺序核精确purge身份，按原控制FIFO信用生产退休。control出队与Raw purge通过现有credit wakeup唤醒receiver，无新增线程或媒体队列。cancel仅豁免尚未入队退休的成员，所有已入队退休仍等待精确消费回执后才开放新媒体。

旧退休在purge frontier之后是正常控制事实：ReacquireRequired投影invalidatedGeneration旧代，LockedGate先分类Retired并丢弃，再考虑HardDiscontinuity；validator只在ActiveGeneration首次失效推进组代。此为源码核验，仍待独立复审和运行验证。


第三轮复审发现取消门还须覆盖触发取消的强失效control，而不只是正常退休。现将既有ClockGroup回执扩展为每个成功处理的原FIFO控制序号（包括明确拒绝的旧证据，后者已消费且不会再破坏新映射）。source记录实际发布的最后control序号与成功消费水位；cancel解除门前逐成员确认消费水位覆盖所有已发布旧control，同时仍核已入队正常退休的精确tuple。该扩展必须修改既有MediaRtpClockGroupNode回执调用点，源码范围由9文件增加为10文件；没有新增控制队列或线程。固定标量仍由Stream实际sizeof计费。该第四稿后续已获双源码PASS，见rk480记录；rk483实流失败，不能宣称运行PASS。


### rk483：EndGeneration在硬字节通道的载荷事实遗漏

真实日志1297为11:53:27.611首次input_sealed，1298为Raw node8立即报`FFmpegNodeRuntime output channel aborted`，随后sender取消；此前workerErrors/errors均为0。源码确定该消息并不必然代表有人先调用abort：MediaChannel::payloadBytesForBudget仅为EOF/Flush特判0；EndGeneration没有这两个标志，MediaControlBuffer继承基础payloadFootprintBytes的nullopt，硬字节通道据此返回Aborted，再被FFmpegNodeRuntime合并为上述消息。

统一修正位于MediaControlBuffer本身：合法Eof/Flush/Abort/EndGeneration均显式报告媒体payload为0，Unknown或身份/flow不合法仍返回缺失。这里0是无媒体载荷的事实，不是容量放宽；对象本体、source队列entry与槽位仍按原sizeof和对象数量计费，source.queue已有sizeof(MediaControlBuffer)，最终资源ledger另计固定控制对象。未改变字节上限、对象上限、EOF或generation关闭语义。

行业依据：[GStreamer事件设计](https://gstreamer.freedesktop.org/documentation/additional/design/events.html)将事件对象与buffer数据区分，并要求EOS与buffer有序；[事件开发文档](https://gstreamer.freedesktop.org/documentation/plugin-development/advanced/events.html)明确EOS无属性。适用的是控制对象无媒体payload且保留有序性，不把非永久EndGeneration等同永久EOS。

错误传播另记：MediaChannelPushResult仅含outcome/capacity/queued，缺失footprint、超单项字节上限与真实queue abort目前共用Aborted。准确保留原因需连同channel、事务和node调用方修改返回契约，本次不顺手扩展。

seal延迟独立观察：Closing日志11:53:25.613/25.619，首seal为27.611；截止88154400341957ns与seal邻近steady观测相差约1.9秒。源码receiveTimeoutMs只在receiveNext前采样；BYE的credit wakeup不能中断已进入Linux poll的接收等待，poll只监视transport cancellation与socket。因此已开始的接收仍受原planned read timeout约束，等其真实返回后才可报告RTP owner idle。这解释了可达延迟机制，日志未记录该次poll入场/超时值，不能据此精确归因每一纳秒；不是本次首错，也未改成伪capture fence或缩短经验超时。

本修正已获source_drain_review_a/b双源码PASS，构建与原规格实流状态见rk484记录。rk483保留FAIL，不以该静态结论代替关闭尾媒体验收。
