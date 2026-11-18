# 合屏分配身份与预算域事实审计

核验日期：2026-11-04。范围：当前候选费用计算与真实 buffer/lineage 分配、转移和释放。**这是实现前事实审计，不是完整费用实现、审查 PASS 或验收通过；不改变原规格验收要求。** 行号为本次工作树位置，后续修改可能移动。

## 当前费用边界

`src/internal/graph/planner/realtime/MediaPreparedSourceCandidateCostPlanner.cpp:94` 当前只有定义，仓库搜索未发现生产调用。其 facts 只提供 AU lease、packet wrapper、软硬件 frame wrapper 等数量；头文件明确没有 total/admission。容器、startup index/generation 的局部计算不能证明完整准入，引用槽也不能直接等同独立分配数。

下表路径均相对 `src/internal/graph/`；PMR 指实际传入的分配资源，不能仅按媒体类型推断归属。

| 对象 | 创建与释放证据 | 预算域及尚缺事实 |
| --- | --- | --- |
| Packet wrapper/control block | `runtime/ffmpeg/FFmpegBufferFactory.cpp:167` 使用 PMR `allocate_shared`；最后控制块释放时归还原资源。`runtime/session/MediaRtpPacketClockSession.cpp:156` 新包装，`:163` 转移 payload owner | 原 PMR 域不随 payload 转移。需实际队列、lookahead、节点局部持有和转换输入/输出重叠的 wrapper 上界；两个 wrapper 不代表两个 AU lease |
| Preparation payload lease | `runtime/buffer/MediaBuffer.cpp:19` 转移 owner；`:30` 接 graph credit，`:38` reset preparation lease | payload 从 preparation 交给 graph credit，但 wrapper 仍活着。必须分别核对 lease 与 wrapper 终点，不能同时扣除两者费用 |
| Decode lineage | `runtime/session/MediaRtpPacketClockSession.cpp:132` 为源 AU 创建；display lineage 的 `accessUnit`、codec registry 和 decode wrapper 保留共享引用 | clock session 的分配域。需跨 Packet→Frame 保留路径与独立 identity 上界；AU 可零到多个 decoded frame，packet residence 不能独自覆盖 |
| Decode wrapper、display lineage | `runtime/buffer/MediaVideoDecodeBuffer.cpp:54` 创建 display，`:59` 创建外层 wrapper；`:82` 转换再建 wrapper、共享 display；最后引用释放 | 调用点 PMR 域。各转换的 wrapper 是不同分配；共享 display/AU 需去重，但不能因此漏计新 wrapper；需转换并发与 decoded display 持有上界 |
| Registry State、map node、lease control、opaque owner | `sync/lineage/MediaCodecLineageRegistry.cpp:190/217/218/115` 分别创建；`:57` 删除 entry，lease 析构触发删除，`:32` 的 opaque callback 释放 raw owner | `sync/lineage/MediaVideoLineageStagePreparation.cpp:54` 的生产 registry 明确为 runtime；probe 使用传入 allocation。capacity 可约束 entry，仍需私有 map 实际节点、control block、raw owner 几何及各自同时存活数；不得统一算入 preparation |
| Frame wrapper、clone | `nodes/video/MediaVideoFrameReadinessBinding.cpp:28` 包装；`runtime/ffmpeg/FFmpegBufferFactory.cpp:253` clone 新 AVFrame 与 wrapper，`:278` 按 readiness resource 选择域，随后共享 readiness/credit | 有 CUDA readiness resource 时为 preparation，否则 runtime。需 clone 位置和并发持有；hardware/software 分类不能代替预算域，不能重复计共享 payload |
| Readiness/completion、输入引用 owner | `nodes/video/MediaVideoFrameReadinessBinding.cpp:24` 捕获；`runtime/ffmpeg/MediaFfmpegPayloadOwnership.cpp:21/60` 创建 owner/completion；最终引用或 AVBuffer callback 释放 | 显式 preparation resource；部分 codec payload 保留调用为 runtime。应与已有 producer contract 逐项对照避免重复收费。输入引用 vector 按 `AV_NUM_DATA_POINTERS + nb_extended_buf` reserve，不能仅按非空引用计 backing |
| Audio 与 aggregate frame wrapper | `nodes/audio/AudioDecodeNode.cpp:415`、`AudioResampleNode.cpp:686/747`、`nodes/sync/MediaAvContinuousAggregateVideo.cpp:133`、`MediaContinuousAudioAggregateBuild.cpp:168` | 显式 runtime，不能混入 source preparation frame 总数；runtime 的持有边界仍须独立核验 |

FFmpeg 的 AVBuffer/AVBufferRef 等库对象有独立契约；上表的引擎 opaque owner 几何不能替代库对象开销，也不得猜测其大小。

## 已证实与未证实的生命周期边界

`wrapPacket` 的 `polymorphic_allocator` 虽只保存原始 resource 指针，但真实 `MediaPreparationMemoryResource` 是 `MediaBudgetMemoryResource`：`runtime/resource/MediaBudgetMemoryResource.h:33` 的 allocation block 保存 `shared_ptr<Resource>`；`:151` 用 `shared_from_this()` 将 owner 和独立存储 lease 写入每次真实分配头。因而 wrapper/control block 自身的 allocation header 已覆盖其原资源寿命，不能据 allocator 的 raw 指针判断悬空。

`runtime/resource/MediaBudgetAllocationBlock.h:50` 先将 header owner 移到局部变量；随后移动 reservation、析构 header、释放 native backing，最后依次释放 reservation 与 owner。resource 只持预算、不持这些分配的拥有引用；该局部结构没有 resource→allocation 的反向拥有边。resource 自己的 bootstrap block 只保留 budget，不自持 resource，因此最后一次 block 释放可以解除保活，并非必须等待外部 session 存活。

两条实际路径都使用上述资源：`runtime/session/MediaPreparedRtpSourceSession.cpp:368` 通过 stream resource 包装原包；clock session 使用同一 prepared allocation 产生新 wrapper。当前 CanonicalIntervals 即使转为 graph payload credit，wrapper 的独立 header/lease 仍保留；SourcePresentation 的 AU/display lineage PMR 分配亦由各自 header 覆盖，frame readiness 的 allocator 另保留共享 resource。Session stream 的 `resource` 声明在其容器之前，正常析构先销毁容器；`runtime/compilation/MediaAvRuntimeDomainState.h:19` 与 RawRtpInput/PacketClock 节点还保留 session，source frame processor 由 registrar 绑定 video resource。显式 runtime reset 在清 channels、销毁 scheduler 节点后才清 domains（`runtime/lifecycle/MediaGraphRuntimeLifecycleExecutor.cpp:222–227`）；即使外部 owner 较早释放，已分配对象的 header 仍是独立保障。

**结论修正：此前仅由原始 PMR 指针提出、尚未证实的 owner 悬空疑虑，已被实际 allocation header 的保活和释放顺序证据消除；本次未发现需据此修复的悬空缺陷。** 这不证明所有生命周期或业务引用环均已穷尽，也不证明费用上界完整。wrapper/control block 仍应计费到真正 deallocate；若存在 weak 引用，该时点可以晚于对象析构，payload lease 转移不能提前扣除 wrapper 费用。已有 holder/residence 尚不足以直接证明 distinct-allocation 上界，新旧代并存仍需按真实引用与分配关系核验。
## 后续必须由生产路径消费

1. 在对象所属模块导出精确分配几何，包含真实 allocator 的 control block/map node；不可只用 `sizeof(Entry)`，不可新增猜测数值。
2. 由现有拓扑、typed holder、codec retention 与 producer ownership 推导按分配身份和域区分的产品，明确 transfer/share/new allocation；补齐转换重叠、跨 payload lineage 与新旧代并存。
3. 共同源规划、候选费用和最终 owner readback 消费同一产品；缺事实即失败，不能留下只供展示的费用 helper。确认已预约项与新增费用互斥后才形成完整 total/admission。
4. 完成冻结双审、Release 全量构建和原规格 Windows realtime 实流，再按原顺序推进 RKMPP；本审计不提供任何运行通过结论。

## 补充核验：weak 引用与 opaque 数量

- `sync/lineage/MediaRetainedControlFreshness.cpp:43–57` 的 capture 仅接受实际 `MediaControlBuffer`，且必须是 Eof/Flush/Abort；宽泛 isCandidate 不放宽 capture。每个实际 audio/video lineage state 只有一个 weak 槽，新 capture 覆盖旧值，generation reset、purge 或析构释放。它可延长终端控制对象的 control block 寿命，不能计作 prepared packet/frame wrapper 的保活来源；数量应由真实 DAG state 实例推导。
- opaque 仅有四个实际创建调用：`nodes/video/VideoDecodeNode.cpp:229`、`VideoFilterNode.cpp:566`、`VideoEncodeNode.cpp:312`、`planner/realtime/MediaRtpSourceIdentityProbe.cpp:61`。各处紧随成功 submit，move-only token 移交一次；每次成功 submit 至多创建一个 opaque owner。Decode/Encode 在 opaque_ref 和 pendingSubmissionLineage 均为空时才 attach，Filter 检查 opaque_ref，EAGAIN 保留 pending 并复用。非 copy 路径将同一引用移入 submissionOrderLineage；copy 路径共享底层 owner。
- submit 插入 entry 后分配 lease 失败会删除 entry；probe clone 失败或 opaque owner 分配失败由 token 析构释放 lease；av_buffer_create 失败调用 releaseOpaqueLease。致命 send 错误之后，节点停止/purge 清理 pending 与 submission 队列，Decode `:63–67`、Encode `:88–92` 明确 unref。不得把待清理 pending 当成即时释放。

这些证据收窄了数量未知项：可结合实际 registry capacity 推导 entry/lease/opaque 并存，但 FFmpeg 复制的 AVBufferRef 头仍需独立上界，也不能据此推导 AU 与输出 frame 一一对应。本补充仅为源码核验，未修改代码或运行测试。

## 补充核验：registry State 替换与 weak 控制块（2026-11-04）

结论：**当前生产节点没有在 configure、reset、purge 或源重入时替换 registry State；不能继续把“每次重入新增历史 State”列为现有费用阻碍。** 此结论只关闭该生命周期疑问，不代表完整 allocation 上界或准入完成。

- 全 src 的 `MediaCodecLineageRegistry::create` 实际调用只有两处：`sync/lineage/MediaVideoLineageStagePreparation.cpp:54`（runtime 域）和 `planner/realtime/MediaRtpSourceIdentityProbe.cpp:37`（probe preparation 域）。前者经 `runtime/factory/MediaRuntimeNodeFactory.cpp:155–169` 构造节点，实际类型为 Decode/Filter/Encode（:387/:406/:408）；Audio lineage preparation 不创建此 registry。
- 每个节点构造时保存一个 shared registry，同时其 lineage state 保存同一 registry（Decode :101–118、Filter :137–141、Encode :183–200）。全 src 未发现之后向这两个 registry 成员赋值或 reset；`MediaVideoLineageState.cpp:114` 的 resetGenerationLifecycle 只清控制弱引用/代次，`:122` 的 purge 调同一个 registry。节点 resetForLifecycle（Decode :83、Filter :85、Encode :108）清 pending、submissionOrder、codec/filter 绑定，不重建 registry。registry 自身 move 构造/赋值为 default（Registry.cpp:174–177），当前调用仅转移 create 结果；没有生产运行期替换调用。
- State 由 registry 的唯一 `shared_ptr<State>` 持有（Registry.h:74）；entry 持 lineage，不持 lease。每个 lease control 仅有 `weak_ptr<State>`（Registry.cpp:73–89），析构时临时 lock 后 erase 对应 entry（:79–84）。所以 weak 可以延长 allocate_shared 控制块存储，但不能延长 State 对象、map 或 entry 的寿命；没有 State→lease→State 的强引用环。submit 在同一锁下以整个 map.size 检查 capacity（:208–218），purge（:337）不删旧 entry、不新建 State，旧新代 entry 共用该容量，不乘代数。State 对象释放后，若外部 opaque 仍持 lease，只剩原来的那一个 State 控制块，不能按剩余 token 数重复收 State。
- Probe 不会按重试次数积累 State：`MediaRtpSourceFirstFrameProbe.cpp:239–249` 为一次 evidence 创建一次 probe；不足证据的重试先 `restartDecoder`（application/MediaRealtimeCompositionSourcePreparer.cpp:267–272），其 `MediaRtpSourceDecoderReplay.cpp:31–33` 依次释放 firstFrame、旧 decoder、identityProbe 后才创建 replacement。evidence 禁止复制和 move assignment，成员声明顺序使正常析构先销毁 firstFrame/decoder、后 identityProbe（FirstFrameProbe.h:69–77）。成功路径的 tile 可能复制 opaque，因此不以 firstFrame.reset 断言所有 token 已释放；生产 handoff 先完成并释放 tile/readiness（application/MediaRealtimeCompositionPreparer.cpp:371–375），再 prepareReplay→restartDecoder（:378；DecoderReplay.cpp:90），旧 probe 不交给 runtime decoder。候选失败的 tile/filter/帧均为本次 RAII owner，没有历史 probe 容器。
- 可直接消费的计数事实是：每个实际已构造 lineage 节点实例一个 State 分配；每个同时存活 probe evidence 一个 State 分配。节点和 purge target 共享同一 registry 不能重复收费；源 generation 次数不进入 State 数公式。节点销毁后尚存的 opaque 可保留其唯一控制块，必须延长该实例的计费终点，不能在 scheduler erase 时提前扣除。当前固定合屏图没有运行期创建替代 Decode/Filter/Encode 实例；动态 branch 如有活跃/退役节点并存，应按真实实例 owner 及其外部残留 token holder 另计，不能把“每实例一个”推广为所有历史动态实例只有一个。其孤立控制块数至多为仍存活的不同历史实例 token 所占 holder 根数；具体跨 branch holder 上界不由本次 registry 核验代替。

因此下一费用块可以取消固定合屏路径的“未知历史 registry 代数”门禁，使用真实节点/probe 实例身份计 State，按最后 weak 释放结束控制块费用。仍需导出实际 State/control、map node/sentinel/proxy、lease/control 几何，并把 opaque/外部持有槽加入分配身份贡献；FFmpeg 内部复制 AVBufferRef 的数量仍是独立问题。仅文档及源码核验，未构建、未测试。

## 四类 lineage/wrapper 的有限持有根（2026-11-04）

本节为下一费用产品的源码依据，不是完整费用实现。产品应按 source、创建 stage、allocation family、实际 domain 保存 owner contributions 与额外根；计数是允许别名的保守 distinct-allocation 上界，不把引用槽宣称为不同对象。

| family / 实际创建点 | 持有根与归属 |
| --- | --- |
| AU lineage：MediaRtpPacketClockSession.cpp:132 | session preparation resource。packet wrapper 持有根、decoder pendingLineage/registry entry、decode resolve 局部根，以及 display.accessUnit 的保活；不是 AU payload lease 的保活 |
| Packet decode wrapper：同文件:180 | 同一 preparation resource；SourcePresentation packet 路径 contributions 加正在构造的一个 wrapper。bindPacket:188–192 直接返回 timed，不加 cadence 专用 videoLookahead |
| Display lineage：MediaVideoDecodeBuffer.cpp:54，由 VideoDecodeNode.cpp:573 调用 | source reference resource；frame wrapper 根、filter pendingLineage/registry entry 与 filter resolve 局部根。Decode :517–528 resolve 后 unref opaque，至 :573 之间的 sourceLineage 是独立根；不能假定一直被 registry entry 覆盖 |
| Frame decode wrapper：MediaVideoDecodeBuffer.cpp:59/82 | Decode :573 新建 display+wrapper；Filter :791、HardwareTransfer :302 仅新 wrapper、共享 display。按各实际 stage 输出 contributions 加构造根分别计费；源 resource 身份不随转换改变，不能把共享 display 再算为新 display 创建 |

Filter :468/540/564 的 pendingLineage 与 registry entry 可以在原输入 wrapper 释放后保活 display；:729–737 resolve 后 unref opaque，至 :791 之间须保留一个局部 display 根。Transfer 的 m_pendingInput（HardwareTransferNode.h:50）与新输出可重叠；当前合屏 source candidate 拒绝 transfer，只有真实拓扑有该阶段才产生其根。pendingInputAllocations 仅捕获 AVFrame buf/extended_buf/hw_frames_ctx，不持外层 wrapper/display，不能将 readiness snapshot 数量再次加入 lineage 根。每个外层 frame wrapper 的直接 AU 与 display.accessUnit 是相同身份，只传播一次该引用关系；不同根共享对象允许保守相加。

**构造串行性已闭合：每个实际 stream 的 timedPacket 至多一个正在构造的调用。** PreparedSourceSession 的 consume:148、finishBootstrap:223、tick:457 持同一 m_mutex，publish:272→bindAvailable:293/:303 顺序处理；freezeBatch:718 锁住各 session 后调用 prepareFreezeLocked:722，其 publish:592 仍在锁下。takePacketClock:792 持锁，:798 要求 frozen 且 binder 存在，:837 move/reset 原 binder 后唯一交接；frozen consume 的 :378–379 仅排队原 packet，不再调用原 binder，tick:451 不再 publish。运行节点 MediaRtpPacketClockBinderNode:393 接管，:445 仅通过拥有的 clockSession 调 bindPacket，由该节点单 worker 执行。故准备线程与运行线程不为同一 stream 同时新增两个 timedPacket 根；不同 stream/节点的构造可并发，不能合并成全图一个根。

**pending output 与同 node 的下一次输出构造互斥已闭合，但不推出与 pending input 互斥。** FFmpegNodeRuntime::process:72–82 先 drain，入口已有 pending 即使本次排空也立即返回，:91 才执行新的节点 process；transferOrDefer:608–610 拒绝已有 pending，:641/:656 保存当前同一个 output 的引用。当前构造输出交给 pending 的瞬间是共享同一对象，不是第二个 allocation。因此同 stage 可用一个“构造或待发布输出”额外根；仍可能同时持 node pending input/lineage，后者单独列项。现 NodeReferenceRetentionPlanner 的 one-working-reference+incident-capacities 是宽泛保守契约，不能因上述互斥就从既有贡献总数直接减一；在 root 身份覆盖映射未证明前，额外根保守相加并注明覆盖关系。

落地顺序：contributions 与真实 registry capacity/节点 presence → 按 family 的 root 表 → checked count 与对象模块导出的实际 allocator geometry → CandidateCostPlanner 分域费用 → 最终图/真实 resource 独立重建 root 表逐项 readback。尚缺的是四类真实 shared-control 几何及上述 root 表的生产接线；session 构造并发与同 node pending-output 构造重叠不再列为未知。此结论不覆盖 FFmpeg 私有分配，不改媒体算法，未构建或测试。

registry capacity 也已有早期权威来源：MediaRealtimeInputGraphBuilder.cpp:475 将 runtime.queues.frame 赋给 decoder.canonicalLineageCapacity，MediaRealtimeCompositionGraphBuilder.cpp:347 对 source video 同样赋值；builder/segments/MediaVideoTranscodeBranchBuilder.cpp:267–308 将该 capacity 写入真实 decoder/filter 的 video.lineage.capacity。共同 SourcePlanningKernel.cpp:181 返回拥有型 runtime 产品，包含 queues。因此 candidate 可以直接按实际 stage presence 使用该 frame capacity，最终从节点 option 独立 readback；无需新公共参数，也不能再将 decoder/filter registry capacity 笼统列为未知。

## 原始 packet wrapper 与 preparation lease control（2026-11-04）

这两类数量必须分开推导，不能继续由调用方填写无证明裸数量。raw FFmpegPacketBuffer 与四 family 中外层 MediaVideoDecodeBuffer 是两个真实分配；一个外层 wrapper 只保留一个内层 raw wrapper。

| 持有根 | raw packet wrapper | preparation lease control |
| --- | --- | --- |
| prefix / acquiring / session outputs | 对应容量内每个 packet 至多一个；outputs 中事件不产生此对象 | 未接 graph credit 的包仍持一个；转移时沿用原 control |
| session pending | 尚不一定有 wrapper | 实际 maximumAccessUnitsPerPush 覆盖本次全部 controls，包括创建中尚未 push 的一个 |
| RawRtpInput.m_preparedPendingPacket | 一个 | 等待 graph credit 时一个，成功 attach 后释放 |
| binder.m_preparedPendingCredit | 一个内层 raw wrapper | prefix/acquiring 等准备包等待 credit 时一个，成功 attach 后释放 |
| raw-input/binder 创建阶段的 graph packet contributions | 分别保留创建 stage 身份，允许保守重复引用上界 | 成功接 credit 后不再保留 preparation control，不沿全部下游 residence 延长 |
| session/PacketClock 构造 | 各真实执行 owner 的局部 wrapper；PacketClock 新旧 wrapper 可重叠 | move/transfer 不创建新 control，不能按局部变量数叠加 |

真实调用：Session.cpp:326–335 仅在 pending 空时按 reserveBytes 创建 controls，:327 限制数量，:86–87 核对 maximumPendingLeases 等于 maximumAccessUnitsPerPush；:353 要求该 lease 独占，:355 shrink，:372 move 到 wrapper，:392 清 pending。创建第 j 个 control 时已有 j-1 个，连同 allocate_shared 返回但尚未 push 的临时对象仍不超过本次 M；因此**不得在 M 之外再加一个“lease control 构造槽”**。栈上的 reserve 返回值是尚未包装的 lease 对象，不是另一 shared control。完成批次逐项转移到容器后，剩余 pending 与已发布包的引用可能重叠于同批 identity；按 M 加各容器根是保守上界，不宣称这些 control 全部不同。

PacketClockSession.cpp:147–180 取走 packet、创建新 raw wrapper、transfer 原 owner；旧新 wrapper 同存而 lease control 仍仅一个。RawRtpInputNode.cpp:185–205 的 pending packet 经 reserve/attach 再发布；prefix 绕过 RawRtpInput，MediaRtpPacketClockBinderNode.cpp:463–474 解包 raw owner 并接 graph credit。MediaBuffer.cpp:30–38 的 attachPayloadCredit 明确 reset preparation lease，但不销毁 wrapper；control 与 wrapper 的费用终点不同。SourcePresentation 无 cadence lookahead；覆盖 cadence 时应另列真实 m_videoLookahead 根，不用 SourcePresentation 公式直接套用。

**snapshot 身份疑问已闭合，不额外增加上述两类对象数量。** probeSnapshot（Session.cpp:548–564）持 mutex、拒绝 frozen，仅复制 prefix 中的 shared_ptr；session prefix 唯一增删操作是 :308 push_back，准备阶段不 clear/pop。purgeMedia :526 只允许 frozen，:528–533 清 outputs 和协议 pending，不清 prefix。唯一生产 probeSnapshot 调用在 SourcePreparer.cpp:207–231 的块作用域；所有准备 snapshot 只引用同一个单调 prefix 集合，其 packet/control identities 已受 prefix 硬容量覆盖。IdentityProbe::prepare :59 只 av_packet_clone，不创建 FFmpegPacketBuffer 或新 preparation payload lease control。snapshot backing/resource 自身仍由既有 snapshotBytes 计费。另 SourcePreparer :215–216 将 storageScope.root 传给 probe goal，probe registry/header reservation 属该 root，不能伪归 session child 的原包对象费用；这一独立 probe 组件尚须完整费用产品消费。

扣重规则：CandidateCostPlanner 原 containerBytes 已排除 pendingLeaseControlBytes，新全存活 control 费用替代该项；prefix/acquiring/output/pending 容器 backing 继续原项收费。AU payload reservation 已含对应 payload/AVPacket/AVBufferRef 字节，不在 raw wrapper geometry 再收费。session outputBytes 是准入计量，不是额外实际 allocation；不能重复叠入分配总额。下一实现应以 typed contributions、实际 prefix/acquiring/output bounds、M 和两个 credit-wait 成员根生成拥有型数量产品，再供 candidate 与实际 readback 共用；不重新开放裸 packetWrappers/liveAccessUnitLeases 数量入口。本节只核验对象关系，未构建、未测试。

### Frame wrapper 分配域与 transfer 节点误拒绝复核（2026-11-04）

本轮沿真实创建点复核，而非沿旧局部 PASS 推断：
- MediaVideoFrameReadinessBinding::publish 无 readiness 契约时使用 Runtime 分配域；存在契约时先 capture，再以 source preparation resource 创建 raw frame wrapper。FFmpegBufferFactory::wrapFrame 根据实际 hw_frames_ctx 选择 HardwareFrameBuffer 或 FFmpegFrameBuffer，硬件属性与预算域是两个事实。
- cloneFrame 创建新的 raw wrapper，并由源 readiness.resource 决定分配域。VideoFrameRateNode 和 VideoOutputFanoutNode 存在该创建点；sourceFrames(false, filterActive) 不包含 FrameRate，sourceDecoder/sourceFrames 也不包含 VideoOutputFanout。因此当前 SourcePresentation 的两 creator 公式不能直接外推 cadence 或动态输出链路。
- MediaRtpSourceFirstFrameProbe 保留的是 FramePtr/AVFrame，不是 FFmpegFrameBuffer；不能作为 preparation raw wrapper 计数。现 readiness ownership 费用核算 attachment、operation、completion、snapshot，不应把这些持有底层引用的对象当作 raw wrapper。

发现并由独立审查者 frame_wrapper_domain_review 明确判定 FAIL：MediaVideoBranchConnectionsPlanner::sourceFrames 第34–35行始终声明 Transfer；ownedKind 将其映射为 HardwareTransfer，MediaCompositionSourceTopologyPlanner 的 owned(VideoFrames) 保留该节点，实际 MediaVideoTranscodeBranchBuilder 第535行同样创建。MediaSourceLineageStoragePlanner 第37–38行却无条件拒绝 HardwareTransfer，故当前真实 SourcePresentation 拓扑将被误拒绝。共同 kernel 验证 transferDirection=None 并不删除节点；HardwareTransferNode::transferOrForward 的 None 分支原样转发同一 buffer，不分配新 wrapper。

此前 lineage 根范围的局部 PASS 不能继续代表真实拓扑可用；本问题尚未修复。下一修复需把 planner 选定的 transfer 契约接入费用判断，并由最终实际 node options 独立核对；只有已证明 None 的节点可按转发持有者计，不得直接删掉拒绝条件或放行真正转换。随后两名独立审查者重新复审、全量 Release 构建；当前 cadence 实流不能验证此分支。本轮未改生产代码、未运行新媒体测试。
后续修正结果：上述误拒绝已在七文件中按显式transfer契约修复，实际node options独立核对None；frame_wrapper_domain_review与transfer_contract_review_b均明确PASS，Release全量构建51255成功（791目标）。此前FAIL保留作为发现记录；新费用路径仍无实流证据。详见full-storage-admission-plan的同日修正节。

### Registry 释放尾部上界与独占 token 复核（2026-11-04）

本轮确认一个此前 entry 容量分析未覆盖的窗口：State::release 在 mutex 内 erase entry 后解锁；MediaFfmpegLineageLeaseControl 析构体随后返回，shared control block 尚待释放；releaseOpaqueLease 还要在成员 token 销毁后才 deallocate 外层 opaque owner。因此另一个 submit 可在旧控制块/opaque 分配仍存活时补入新 entry。entries<=capacity 只能直接证明 map entry 上界，不能直接证明 lease allocation 和 opaque allocation 同时不超过 capacity；硬加1也没有依据。独立审查 frame_wrapper_domain_review 确认这一问题。

FFmpeg 官方 AVBuffer 文档允许多线程同时引用/解除引用（https://ffmpeg.org/doxygen/trunk/group__lavu__buffer.html），最后一次解除引用触发释放回调；不能假设所有回调串行落在本项目单node线程。生产 registry 仍属Runtime域，probe registry属Preparation root域；本轮没有把两者混账，也没有将该源码窗口归因为既往实流故障。

已核对所有 token 调用：Token禁止copy，Decode、Filter、Encode、Probe均move到makeMediaFfmpegCodecOpaque；从AVBufferRef解码得到的观察token没有lease。没有要求lease共享所有权的真实调用。下一实现拟将slot责任内联到move-only token，取消独立shared lease分配；opaque回调先move owning token至局部，销毁并释放opaque内存后，再由局部token归还entry。由此让slot归还发生在引擎opaque allocation释放之后，不依赖回调线程数。State继续以弱引用被token观察，避免保活entry/map或引入环。

实现门禁：自定义noexcept move并清空源拥有权；move assignment先归还目标旧slot且处理self-move；观察token不得归还槽；map插入失败不得留下拥有token；成功submit的结果移动不得在持mutex时析构owned临时token导致递归锁；opaque分配失败与av_buffer_create失败都必须归还恰好一次。FFmpeg自身AVBuffer/Ref在回调之后的释放不属于此引擎对象数量证明，继续单列。

本节为已核验的设计修正，尚未修改registry生产代码。该共享组件影响Windows/RKMPP及Decode/Filter/Encode/Probe，修后须双独立复审、全量构建及Windows原规格realtime链路验证，再进入后续RKMPP；不得用仅SourcePresentation dormant分支为理由跳过当前生产回归。
### Registry 几何消费者及真实 STL 平台边界（2026-11-04）

共同runtime账本应按实际selected Decode/Filter/Encode节点的lineage identity/capacity计费；probe使用root preparation resource，外层对象为IdentityProbe，不能重复计runtime外层registry。独立审查transfer_contract_review_b确认：compileLedger现有协议generation几何只在选中materializer且缺少sealed geometry时进入MSVC门禁；encoding/shared子图不是必经。无条件接入新的MSVC-only registry几何将扩大Linux/RKMPP拒绝范围，不能以“已有门禁”忽略该回归。

Windows实际14.51.36231头文件memory:2943–2944中，非数组make_shared使用new _Ref_count_obj2<T>；allocate_shared:3035使用_Ref_count_obj_alloc3<T,Allocator>。生产外层registry是make_shared，State是带MediaLineageAllocator的allocate_shared，两者不得混用同一控制块公式。

只读远程核验（没有运行RKMPP媒体测试、没有写远程文件）：密钥认证失败后通过既有口令交互认证成功；uname为aarch64。默认/usr/bin/g++为Kylin GCC9.3，不是项目工具链。执行项目既定环境命令后，实际g++为/opt/mt-tools/gcc-12.4-native/bin/g++，版本12.4.0。实际include/c++/12.4.0/bits/shared_ptr_base.h:570–630显示_Sp_counted_ptr_inplace<T,Allocator,LockPolicy>、allocator rebind及_M_destroy释放路径；bits/stl_tree.h:210–240显示_Rb_tree_node<Value>。下一adapter必须继续核对实际allocation调用、lock policy、tree sentinel及编译模式，不得直接把MSVC节点/额外sentinel费用搬到libstdc++。

只读命令记录（SSH通过已知host key校验；认证口令不写文档）：
```text
ssh -o StrictHostKeyChecking=yes -o ConnectTimeout=5 -o PreferredAuthentications=password -o PubkeyAuthentication=no root@192.168.130.229 "uname -m; command -v g++; g++ --version"
ssh -o StrictHostKeyChecking=yes -o ConnectTimeout=5 -o PreferredAuthentications=password -o PubkeyAuthentication=no root@192.168.130.229 "bash -lc 'source /opt/mt-tools/mtenv.sh && mtenv on && command -v g++ && g++ --version'"
ssh -o StrictHostKeyChecking=yes -o ConnectTimeout=5 -o PreferredAuthentications=password -o PubkeyAuthentication=no root@192.168.130.229 "bash -lc 'ls /opt/mt-tools/gcc-12.4-native/include/c++; sed -n 570,630p /opt/mt-tools/gcc-12.4-native/include/c++/12.4.0/bits/shared_ptr_base.h; sed -n 210,240p /opt/mt-tools/gcc-12.4-native/include/c++/12.4.0/bits/stl_tree.h'"
```

退役范围：现有branch resource lease覆盖worker join、scheduler节点销毁及channel/context清理，编码group还等待输出物理退役；VideoEncodeNode在发布packet前解析并unref opaque。当前闭合链有无逃逸依据，但Runtime token不持branch lease，不能声称任意外部逃逸opaque/weak State尾部都自动延长预约。消费者必须限定已证明的闭合实例，并保持未知生命周期不被默认放行。本轮未改生产源码或新增构建，r383仍是最新实流证据。
### Probe 与冻结准入的重叠核验（2026-11-04）

独立审查 frame_wrapper_domain_review 与主代理核对当前调用顺序：每路成功 probe 保留在 sources.evidence 中；CompositionPreparer 先预约输出与画布，再 freezeBatch（第297行），随后构图和实际输出准备，直到第378行 prepareReplay 才按 firstFrame、decoder、identityProbe 顺序释放。故 probe 可从最终运行阶段排除，但不得从冻结时峰值排除；前面成功源与当前探测源同时存活。同源重试先销毁旧 probe，不把历次尝试累加。

下一费用产品须覆盖 resource bootstrap、probe allocate_shared 控制块、registry State、逐项 map 节点、逐项 engine opaque，以及已有 public-header 预约；数量来自实际 snapshot packets.size()。Registry 内嵌，不能复制 runtime outer registry 费用；共享 source identity、AU lineage、原 packet/payload、已预约首帧不得重复计费。FFmpeg 私有分配仍不能由这些对象几何证明。

若采用 root 完整预约再转子预算，后续组件必须消费子预算，不能再次向 root 收费；新子预算自身对象及控制块也必须计费。联合 solver 应按唯一预算身份核对所有活跃 probe、输出/画布预约与源候选，不能把 root 总用量逐源重复相加。当前 freezeBatch 仍传 nullptr，以上是已核验的待实施边界，不是完整准入已实现。

### r385 调度等待调查边界（2026-11-04）

r385失败证据已归档：scheduler音频seq2956因缺video等待123.7101ms，晚于dispatch后才输出，mux拒绝原deadline。源码selectMediaHead在任一流无head且未EOF时返回nullopt，onProcess返回无期限等待；processMedia里已有typed emit/protocolPreparationLead及deadlineWait，但该缺失流分支不进入它。

官方对照：[GStreamer GstAggregator get_next_time](https://gstreamer.freedesktop.org/documentation/base/gstaggregator.html#get_next_time)明确区分默认等待全部sink和live deadline aggregation。该机制支持调查有限等待，却不证明本项目encoded DTS排序可直接超时放行；现有缺失流watermark尚未接线，不能用恢复后才知道的视频时间戳倒推当时已具备安全发布事实。FFmpeg mux.c官方页已打开，但定位max_interleave_delta时超时，不据此补写未经核验的等价结论。

下一调查需拆开两个问题：持有AU等待策略与视频到达迟延。deadline唤醒只能暴露过期，不能单独修复连续输出；任何安全提前发布必须由已证明的每流排序/下界事实规划，不得直接放宽期限、跳过视频或以样例帧率硬编码下一DTS。此次尚未修改scheduler，预算九文件源码PASS不能抵消r385实流FAIL。
