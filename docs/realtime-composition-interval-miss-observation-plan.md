# 合屏 interval miss 同代观察

范围：仅 Aggregate 节点固定大小诊断，不修改选择、等待、丢帧、时钟或原 miss 条件。r357/r358 的累计 real 计数及提交前 miss 不能证明同代连续性失败。

依据 [GStreamer latency](https://gstreamer.freedesktop.org/documentation/coretracers/latency.html) 与 [Tracing](https://gstreamer.freedesktop.org/documentation/additional/design/tracing.html) 的节点、时间及结构化事件关联原则。官方 latency 注入事件贯穿管线；本实现不注入事件，仅读取现有 Aggregate 状态，不能称全上游延迟追踪。

每源保存当前代固定观察：首个真实提交 cursor/steady、首个当前代 canonical candidate 入队 steady/映射区间、prepare 阶段过期与旧代丢弃计数及最近已映射区间。计数饱和并标记 incomplete。首次 miss 冻结 epoch sourceStart/masterRelease、上述当前代事实；后续仅补同代首次候选入队、首次真实提交及该 cursor 实际提交 generation/steady；候选字段明确区分 at_miss 与 final，后代不得覆盖。提交 generation=0 表示黑帧；无提交记录仅表示未观察到该 cursor 提交，不判定取消原因。

当前代观察在源 purge 清空，首次 miss 保留；整节点初始化重置全部。同一节点 owner 线程更新，无新锁、容器或媒体引用。字段嵌入既有 SourceStorage，现有 sizeof 几何与元数据预算自动包含；只在原 summary 输出，不新增每包日志。snapshot 与 channel depth 仍是分别读取，不假称原子全图观察。

实施后须独立双审；未构建或真实链路验证前不称运行通过。

冻结源码审查：crash_release_audit与prepared_wire_registration（均未参与本诊断实现）分别明确PASS，root另核对原策略及提交路径。首候选已区分at_miss/final，后续仅同代补齐；commit记录同cursor实际sourceGeneration，0为已提交黑而非空值。Release全量重建30330已启动，尚待结果，不作运行结论。

构建结果：Release全量重建30330 exit0，configure/build均0、clean-first/all-target753项，四项必需产物已核验。realtime为6577152 bytes、2026-10-26 16:53:34，local为4564480 bytes（仅构建未运行）。新增诊断尚未执行实流，不判根因或连续性通过。
## r366：重入首帧链路的最小诊断补齐计划

状态：仅诊断设计，尚未实现或验证。r366 在 source2 generation2、cursor4808 提交黑屏时，本代尚无 candidate/first-real，不能解释为本代已经恢复后的中途断帧。target 比 epoch.masterRelease 晚69.479034ms；首 candidate 在 miss 后12.3274ms到达，其区间从 epoch.masterRelease+100ms 开始，比该 target 晚30.520966ms，因而提前交付该 candidate 也不能覆盖4808；下一cursor4809落入该区间。发送预约 deadline FAIL 是另一故障，不合并归因。精确原始值与运行结论见 realtime-composition-r366-validation.md。

行业依据：[GStreamer latency design](https://gstreamer.freedesktop.org/documentation/additional/design/latency.html)要求区分媒体时间、实际延迟和准备状态；沿上述 tracing 的节点/时间关联方式补观察，不声称等价实现了跨管线注入式 tracer。本次不改变 NextAtomicRelease/InitialAtomicRelease 状态机、锚点、等待期限、选择区间、背压、丢弃策略或原 miss 条件，不增加公共参数，不通过调大等待掩盖缺口。

### 现有身份与证据缺口

复用 canonical lineage 的 sourceIdentity、generation、sourceSequence、presentation_ns、duration_ns；SourceDisplayPoint 路径另有 decodedSequence。releaseIdentity 仅存在 StartupReleaseTransactionBuffer，不在下游帧中：由 sequencer/extractor 输出 releaseIdentity 与实际首AU/批次范围的对应关系，下游按既有源身份/代际/源序列关联；不得为诊断给媒体对象新增身份传播字段，也不得以序列差值推断连续AU数量。各点同时输出 node_id、stage、steady_ns；实际frame另记pts及time_base，不能混同canonical presentation。

Aggregate 已有首次候选、首次真实提交、miss时/最终状态和丢弃计数，缺候选对应源序列。Decoder 的 first_packet/first_submit/first_frame、Filter 的 first_input/first_output、Extractor 的 first_release/first_commit 当前为生命周期一次bool，不覆盖重入代际；Decoder first_frame还在lineage解析前，不能证明属于新代。NextAtomicRelease 当前直接激活/发布，未采用初次启动的OutputReady/重新anchor握手；这是待调查差异，不是已证明根因或本次修改目标。

### 实施点与字段

| 文件与函数 | 最小诊断内容 |
|---|---|
| nodes/sync/MediaActivatedStartupReleaseSequencerNode.cpp，NextAtomicRelease分支及实际commitReserved之后 | 每代group、generation、release_identity、transition_sequence、epoch_source_start_ns、epoch_master_release_ns；activation与release发布分别记录steady时间，不能合并为一个事件。 |
| nodes/sync/MediaAvBoundReleaseExtractorNode.cpp，processBoundTransaction、stageRelease、commit | 每代首release的身份、video_count、首AU源身份/序列/区间、全批min_presentation/max_end；实际commit后记录首AU身份及发布steady。批次统计遍历已有数据，不保留媒体引用。 |
| nodes/video/VideoDecodeNode.cpp，onProcess、submitPendingPacket | 每代首输入身份/区间、sendRet==0的首accepted身份/steady；accepted、EAGAIN、DropStale计数分开，首次EAGAIN不能当首次成功提交。attachPendingLineage会清空pendingLineage，提交诊断必须从现有有效token或先前提取的诊断标量关联，不能丢失/猜测身份。 |
| 同文件receiveFrames | lineage解析成功后的每代首resolved frame：源身份/区间、frame pts/timebase、steady；成功resolved计数，以及收到帧但lineage失效的独立计数。不能把解析前的帧宣称为新代输出。 |
| nodes/video/VideoFilterNode.cpp，sendFrame、submitPendingFrame、emitFrame | 每代首输入、首次buffersrc接受、首次resolved输出，分别记录既有身份/区间/steady；记录实际stale/unresolved丢弃计数。输入接受与输出就绪分开。 |
| nodes/video/VideoPreparationNode.cpp，onProcess | 每代首观察帧及preparation phase、首输出尝试、DropStale计数；Initial预约与Next透传行为不变。 |
| nodes/FFmpegNodeRuntime.cpp，现有实际传输完成路径及commitReservedOutput hook | 复用原完成通知记录每代首次真实frame发布；实现时核实通知位于真正入队之后，覆盖直接及deferred完成。emitOutput成功可能仅保留待发，不能作为真实发布。不得新增改变提交语义的hook或锁。 |
| nodes/sync/MediaAvContinuousAggregateNode.cpp，候选入队及真实contribution提交 | 原firstCandidate/firstReal补源序列、presentation/duration；保留原miss target、epoch、队列深度、淘汰计数与steady。 |

实施前核对实际DAG节点顺序。若路径中存在HardwareTransfer或VideoFrameRate，必须通过同一真实发布观察覆盖，不能假定decoder到filter直连；输出无lineage/非视频/不属于已规划源的节点不伪造关联字段。共享真实发布观察应集中到明确职责的诊断helper，避免各节点复制身份格式化与计数逻辑。

### 有界存储与解释边界

每个节点仅保留当前代固定大小的首事件标量和计数，换代前及stop/abort输出汇总；没有新代事件时不得覆盖旧代记录。无需逐帧日志、媒体引用集合或“前3帧/前N帧”样例阈值。计数仅用于诊断，饱和时标记incomplete，不改变生产结果；首事件附带此前accepted/resolved/drop等累计值。旧代丢弃不切换当前观察代际；lineage解析失败时如无法权威取得generation，明确记unknown/node级计数，不能借当前代标签伪称已归因。状态受既有owner线程/lineage锁保护，不引入跨线程快照承诺；若增加对象sizeof须复核现有几何计费。

判读顺序：extractor首批最早区间已为sourceStart+100ms，则调查选窗/上游；较早AU已accepted而decoder首resolved从+100ms开始，则调查解码及lineage解析；decoder较早输出而filter/preparation较晚，则调查对应阶段；各阶段较早真实发布而aggregate未收，则调查中间节点、purge与消费；身份/区间一致而只有steady延迟，则属于交付时间问题。首事件与计数不足以证明一一AU→frame映射，必须保留解码零/多输出语义与不确定性。

冻结后按完整消费者链双独立审查，Release全量重建，再用原规格realtime真实流、VLC原生快照联合取证。构建成功或诊断字段出现不代表interval miss、发送deadline或完整合屏目标已修复；结束后按既有记录/清理规则处理临时产物。

2026-10-28实施进度：Aggregate三文件已接入固定标量sourceSequence/presentation/duration/decodedSequence及at_miss/final冻结。首轮子集审查指出仅misssummary会漏display和无miss代际，已补每代首candidate及首真实contribution各一次独立事件；日志失败隔离，原选帧/丢弃/miss/提交规则不变。复审者reservation_review_b明确此子集PASS；其余链仍在实施，尚未全单元双审、构建或新实流。新增SourceStorage字段由AggregatePlanner实际sizeof纳入成本，display无duration不构造区间。

### r366 主链诊断实现冻结（未构建、未实流）

新增统一 SourceVideoObservation，仅存当前代固定计数、node级 unknown/stale 总量及 incomplete；首事件立即输出，换代和stop/abort汇总。source identity/代际/源序列均来自既有lineage；DisplayPoint的duration明确unavailable，分别记录descriptor_time_base和AVFrame.frame_time_base。日志异常被隔离，不改变媒体返回值。Decoder/Filter pending观察仅借用原lineage/token保活期间的identity view与标量，accepted在原pending被释放前观察，EAGAIN保留原token；purge/reset随原pending清空观察，未新增媒体/shared持有。

FFmpegNodeRuntime在真实全部通道入队后、原commitReservedOutput通知前观察，覆盖direct、atomic及deferred；非atomic任何Dropped只设置诊断标志，不记Published、不改变原投递结果。HardwareTransfer和VideoFrameRate实际继承同一基类并通过原emit路径，均纳入此观察。Preparation初次预约commit会清空持有数组，因此只保留数字事实，发布时source_identity=unavailable且identity_link=node_generation_input_source_sequence，关联此前Input记录；不为补日志延长帧或字符串寿命。Next透传发布直接读取仍在原传输中的buffer身份。

Sequencer分别记录activation成功与release入队；Extractor记录原transaction releaseIdentity、完整首批min/max与实际commit，准备prefix发布另记Published，不混为整批提交。新增诊断无容器增长、无每帧日志，无额外registry resolve。计数按观察点累计，OutputAttempt与WouldBlock是尝试计数；unknown/stale为node级总量，不能归因到当前代。新增字段为普通运行时节点/lineage state的固定内嵌数据，不新增owner预算或reserve；原完整G对runtime state等未覆盖的边界仍保留，不宣称已完成全内存证明。Aggregate独立首candidate/real事件与miss冻结由本轮另一个实现者接通，待全链双独立审查。

完整16源码首次双独立审查PASS后，Release11274全量构建实际失败exit1：MediaSourceVideoObservation.h未直接包含MediaNodeId定义，产生C2061/C2065等继发错误。根代理补入core/MediaNodeId.h，UTF8/CRLF与diff检查通过；两名审查者重新复审明确源码PASS。审查遗漏与构建失败保留，不宣称编译或实流通过。

Release63158全量构建成功（757项），realtime6630400 bytes/18:34:29。r367新诊断实流四阶段快照、四源exit0、caller_stop0，payload246709匹配零；16产物与六PID清理完成。首AU正确到达Preparation发布但映射区间已过aggregate cursor，原恢复对齐契约不要求首AU必须展示，不把首帧前miss当作已证明的中途断帧；完整连续性/内容AV和r366发送等待原因仍未闭合。完整原始代际证据见r367记录。
