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
| decoded video/preparation → optional filter/frame-rate → video aggregate | 各frame/pending holder完成后转发；末显示帧复用 `MediaAvContinuousAggregateNode.cpp:445–456`：最后decodedSequence等于lastDisplayCommit才移除并sealDisplayInput，不造末帧duration |
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
