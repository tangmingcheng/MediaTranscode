# 合屏完整准备存储准入计划

状态：2026-10-29，实际分配域、容器费用和两阶段准入正在联合实现；上一轮 Release 全量构建通过，完整费用求解与整体双审仍未完成。历史资源审查的 bootstrap/RTCP 缺口已有修改，尚不能据此判定完整准入通过。沿当前生产 DAG、既有 owner 和已批准输入推进，不新增公共参数、不修改外部 FFmpeg；r367 仍仅为此前 cadence 运行证据，不能证明本计划通过。

## 工业依据与适用边界

沿用 [GStreamer bufferpool](https://gstreamer.freedesktop.org/documentation/additional/design/bufferpool.html) 的上游分配协商、有限 buffer 数量、引用保活及旧对象排空原则。现有实际 allocator 几何、精确数组、类型化 source/route 产品继续复用；不另建媒体链或探测用占位图。该原则不能替代具体分配上界、同 owner 关系和最小进展集合的证明。

## 预算与容量

对每个真实 continuous owner，部署总额为 B。已有基项 F 为 SourceStoragePlanner 的 fixedStorageBytes 加各 stream.workingSet.maximumBytes；SourceSession 已预约并长期持有。storageBytes/sharedCapacityBytes 是 B，不是额外占用，不能重复扣账。

准入必须证明 `F + U_AU + U_协议输出及handoff元数据 + G_startup(Nv,Na) <= B`。AU payload、共享 lease、packet/lineage wrapper、prefix FIFO/snapshot 与 startup 共用预算；handoff 不释放仍存活引用的租约。当前动态 U 尚未封闭，不能仅用 B-F 求 Nv，也不能以当前 free bytes 代替并发峰值。

音频 Na 由自己的 clockRate、AU duration ticks、最大 AU bytes、已准备 prefix 与原 acquisition window 调用既有 planStream 推导，与视频 cadence 无关。SessionPlan 已将音频计算移出视频 cadence 分支；独立产品已贯通 admission.audio、cadence retention.audio、准备事实和 SourcePlanner/startup 一致性检查。未来 G 复用同一事实，不猜预算比例。

视频 Nv 与压缩 AU retention 分离。完整成本闭合后才以 checked 单调 G 求可准入整数；搜索仅求值，不分配候选容器。溢出、缺能力和不够容纳最小进展集合必须明确失败。`sharedCapacityBytes/sizeof(MediaBufferRef)` 不是真实 decoded 单位成本。decoder output adapter 只证明独立于固定 decoder surfaces，不提供显存字节上界；引擎存储与 GPU/frame credit 分域验证。

## 唯一 holder 关系

早期关系必须复用 EdgePolicyPlanner 与 StartupRoutePlan；最终沿原 Event/Node 遍历核对，禁止第二套图。每项绑定实际 role、owner、边槽位及合法状态。payload 别名不重复计媒体，但独立 wrapper/数组分别计分配请求。

| 角色 | 已核验的同时存活边界 |
| --- | --- |
| VideoInput/AudioInput | 当前 Envelope 与 FFmpeg pending 是同一生产槽的互斥状态，不相加；输出 edge 另有实际槽位。 |
| Coordinator | video/audio 各一个 pending Envelope；当前输入是 pending 别名，Store 保存媒体而非 Envelope；新 Release 与输出 pending 互斥。 |
| Binder | pending Release 与 Transaction.payload 指向同一 Release；initial 向两边发布同一个 shared_ptr，不算两组独立对象。 |
| Sequencer | initial 原组加一次重锚组，最多两组完整批次；active 重试瞬时原/旧重锚/新重锚三组，但生产批次是单 video 或单 audio；NextAtomic 不重锚。 |
| Extractor | preparation 原事务与 bound 事务按逻辑身份归并；staging 与 reservation PreparedPush 为两份独立引用数组；replace 等长替换不创建第二套 reservation。 |

下游旧代可以与上游新 Store 并存，按边容量与节点 holder 计实例，不能假定 purge 清空全部队列或乘一个任意 generation 系数。仅已证明互斥的局部阶段取 max，其余并发部分求和。

## 必须贯通的实现链

1. 独立音频事实贯通现有真实消费者，拒绝不一致；不构造尚不存在的假 G 产品。
2. 从同一 route 提取带别名和合法分支的 holder 关系。SharedObjectPlanner 在已有真实 Prepared scope 时可于 Nv/Na 前读取真实几何，无需节点；若早期没有 resource，则将同一几何实现分离为纯能力描述与 owner 绑定，不能伪造 scope。
3. 补齐实际分配域：协议动态 U、Canonical Envelope、字符串 backing、reservation 的 OwnedBatch/PreparedPush/record/channel/lock/临时容器。现有 Runtime 域不得仅因存在 source 就宣称 Prepared 计费。对象头的 sizeof 与字符串 length 不能代替完整 backing。
4. 组合 shared/index/hash/IDs/selections/ReleasedUnit/staging/reservation 的真实请求（含 header/padding）与分支峰，再按同 owner 预算求 decoded capacity。不给既有预算再次 reserve。
5. 将封印产品传给 StartupPolicy、Coordinator/GenerationState、release batch/原子输出容量、decoder 持有额度及 NodePreparation；压缩 AU 产品继续只约束源会话。
6. 最终图验证实际 route、owner、容量与早期成本关系一致。缺项保持构图前失败，不开放 noncadence。

最小进展条件仍需从实际 display-point 选择与 decoder 保留契约证明。有限容量不等于未知时间戳密度下必然完成 acquisition，不能借输出 fps 推算源密度。

## 验证与未完成项

沿已批准 Windows Release realtime 真实流→RKMPP 顺序；原规格源、VLC 原生快照、失源黑/静音、重入、A/V、CPU/内存及释放计数均需证据。新 capacity 产品须由能实际进入该路径的流验证，cadence 回归不能替代。取消路径的 populated Pending/Authorized reservation 运行证据仍缺，普通 caller-stop 不足。

实现冻结后两名非实现者按上述算法、所有权及预算逐项审查，再全量重建与实际验证。当前完整 G、noncadence、内容级 A/V、Windows 全矩阵和 RKMPP 均未闭合，任务未完成。

## 后续接口核验

音频事实贯通已实现并双审、全量重建通过；r363 已运行 cadence 实流并由 caller_stop 正常结束，负载预约/释放均为256922，但缺重入期间画面快照，不计完整验收通过，详 r363 验证文档。压缩持有量可复用SourceResourcesPlanner的acquiring、实际队列、pending、decoder内部保留和协议完成批次关系，但不能写成Nv+decoderRetention：FFmpeg允许每输入零到多输出，内部保留数不是首帧输入数上界。应区分Cv/Ca压缩容量与Nv/Na保留容量，统一校验F+U_AU(Cv,Ca)+U_protocol_handoff(Cv,Ca)+G(Nv,Na)<=B。现noncadence acquiring仍B/sizeof(ref)，callback单批上界也不是总积压界；还需证明输出队列硬门禁与结构容量，保留原有限准入/超时失败语义。不得把观测到的首帧prefix大小提升为未来全流最坏进展保证。依据：https://raw.githubusercontent.com/FFmpeg/FFmpeg/master/libavcodec/avcodec.h 。

## reservation 贯通进度与剩余边界

实际 reserve 消费者只有 VideoPreparationNode 与 MediaAvBoundReleaseExtractorNode；authorize 位于 MediaAvStartupVideoPreparationState。PreparedPush 已贯通显式owner，精确引用数组由原scope收费，双审和Release54069通过，尚未新实流。常驻OwnedBatch、record count、channel/lock及owner字符背板现已迁入原scope，代码冻结待双审；record实际allocate_shared收费已接通，但准备期sealed几何尚缺。

实施顺序：显式分配域贯通 preparePush/prepare（含 AtomicOutputTransaction 原有 Runtime 调用），PreparedPush 先持 owner 再持 backing；随后封印 reservation 常驻 OwnedBatch、channel/锁数组与 shared record 几何，最后补 authorize 的 records/recordLocks/counts/channelLocks 临时峰值。reserve 的 required map 可用精确批次数组排序归并，必须保留同 channel checked 汇总、原锁序与容量失败行为。authorize 临时请求与现存 reservation 同时计费，不能当替代关系；owner 字符串 backing 仍需处理。

不得仅凭当前同源合屏样例给通用 authorize 新增同 owner 限制。先核验 factory 向 VideoPreparation 传递真实 capability 的路径，再决定跨 owner 临时存储归属；不从任意 buffer 猜分配域。本节仍不是完整预算保证；authorize scratch和完整G求值尚未接通。
## 早期与最终规划共用事实

2026-10-26源码核验：SourceResourcesPlanner对decoded video明确startupPackets=0，压缩Cv不能由Nv换算。可拆出同一纯事实计算，输入opened decoder retention、filter retained frames、实际frame contract和Raw completion上界；早期使用既有route关系，最终绑定实际节点/边复核。acquiringCapacity本身是候选Cv的一部分，不能拿包含Cv的总和反过来声称独立推导出了Cv。

当前循环是最终资源planner先要求已封startup容量和实际图，而早期联合求值尚缺上述共享事实入口与完整成本函数。须先抽取共享计算再接Cv/Ca与Nv/Na联合求值，禁止构造占位图。FFmpeg官方send/receive说明每个输入可能零或多个输出，也可能先接受多包才返回首帧；有限物理holder边界不等于任意输入都能在等待期限内完成acquisition：https://raw.githubusercontent.com/FFmpeg/FFmpeg/master/libavcodec/avcodec.h 。
## authorize临时存储下一接线

实际唯一调用经VideoPreparationState::authorizeRelease；可从RuntimeCompiler创建state时注入同source的显式scope，既有state亦需核对域/owner，不借任一record资源且不限制跨owner授权。设H为handles数量、S为各record只读channel集合大小之和、U为去重channel数：records和recordLocks各H，channel指针背板S（逻辑U），channelLocks为U；不必复制ChannelCount，检查仍遍历原record。

若将构造后不再变化的channel集合标为只读，可先checked累加S并完成全部scratch，再按原record→channel顺序加锁重验Pending/关闭/计数并执行授权。外层state mutex仍覆盖该操作；本单元不自行引入解锁后重验的新状态机。此为只读调查结论，尚未实施。
## reservation准备期几何贯通

下一实施单元复用现有SharedObjectPlan的封印方式，独立建立OutputReservationStoragePlan/Planner，实际record/OwnedBatch类型几何由事务完整定义处提供，使用真实LineageAllocator shared request。layout为纯能力事实，不分配候选、不依赖图或Nv；Prepared在构图前封印，Runtime明确不调用供应商几何探测。

源准备view增加已有continuousVideoResource的内部类型化引用；预算身份allocationOwner不得转换为resource。SourcePlanner使用实际sync.startup.videoReadiness选择IntervalCoverage→Runtime、DecodedPresentationSpan→真实owner，随后在SourceRuntimePlanner之前形成产品。该时点仍在实际图构建前；未来early G可先使用同一layout事实再生成容量，不创建占位sync或图。

产品进入source runtime、composition source binding及旧单源shared binding；旧路径必须显式Runtime。Compiler只验证readiness、真实owner和复用state的产品一致性，不首次查询供应商能力。State、两个reserve及authorize消费产品，Extractor核对实际release scope；authorize不限制参与record同owner。

成本按真实B/U/H/S和引用数组/标签长度求值，逐请求包含header/padding且不重复reserve。该单元仅封印可复用成本关系，不宣称跨阶段holder峰值、协议动态U或完整G已完成。完成后与authorize scratch联合真实链路复验。
## 完整准入下一整体单元：进入真实 Prepared 启动链

当前源码仍有三道明确门禁：CompositionSourcePlanner 要求 continuousRetention；StartupPolicyPlanner 要求 ledger.inputRetention 且现有 makePolicy 只产生 IntervalCoverage；Session::takePacketClock 要求 canonicalRuntimeRetention。SessionPlan 仅在 ObservedVideoCadence 分支生成该 retention，非 cadence 的 B/sizeof(MediaBufferRef) 只是未闭合的压缩 AU admission，不能充当 decoded Nv。不能删除第一道检查后宣称路径开放。

实施须形成以下完整消费闭环，不再仅增加未参与准入的成本 helper：

1. 从同一 StartupRoutePlan 与 SourceResourcesPlanner 提取早期/最终共用的 role、边容量、别名和 producer holder 事实；保留 decoder 内部保留、raw completion、filter retained frames 及 pending/receive 槽位。最终 planner 当前先要求 startup 容量，须消除这一循环依赖，禁止占位图。
2. 封闭 F、动态 U 与 G 的真实 backing：两路 unfinished AU、pending vector 扩容峰、acquiring/prefix/output deque、probe snapshot、packet/lineage/event/lease shared control、CNAME 和普通字符串 capacity、startup state/索引/数组及 reservation。队列出队仅减逻辑 bytes，下游仍持 lease；源 mutex 仅证明当前 callback 串行，不能消除其他持有者。保留标准容器时必须由真实平台 adapter 证明其增长/保留几何，MSVC 结论不得直接用于 RKMPP。
3. 根据同一预算独立封印 Cv/Ca 与 Nv/Na，实际消费完整费用和最小进展集合；仅求值候选，不分配试探、不二次 reserve。decoder 在 continuous session 建立后才打开，必须沿原 freeze/handoff 边界核验已存在 prefix、queue 与共享引用，不能事后直接缩容量。源时间戳密度未知时，有限容量仅保证有限准入和明确失败，不承诺任意流必在期限内启动，也不使用 AU=frame 或输出 fps 推算。
4. 同时让上述三门验证新产品，并贯通 Session admission/handoff、startup policy、Store/Generation、节点准备与最终实际图/owner/容量复核。缺少任一分配几何、holder 峰、最小进展事实或 handoff 一致性证明，仍在构图前失败；不降低现有超时、失败或平台门禁。

冻结后双独立审查与 Windows Release realtime 实流必须首次证明 SourcePresentation → DecodedPresentationSpan → Prepared，检查原规格正常输出、失源、重入、容量/释放证据和 VLC 原生快照；仅再跑 cadence/Runtime 不构成本单元验收。r366 的发送等待期限失败及 generation 2 缺口保留独立诊断，不以存储接线宣称修复；完整 Windows 验证通过后再 RKMPP，不扩大外部 FFmpeg 范围。
## 2026-10-28 联合实现中的调用链核验

- 原 cadence 会话仍使用实际 resource 准入；供应商容器几何仅用于新 SourcePresentation 候选求解。曾发现 StoragePlanner 无条件查询 MSVC adapter 会误拒绝 RKMPP 原路径，已要求实现者拆开逻辑容量与几何产品；联合审查仍须复核。
- probeSnapshot 当前唯一生产调用位于 MediaRealtimeCompositionSourcePreparer 的同步循环。局部 snapshot 在进入下一轮前析构，FirstFrameProbe 接收只读 span；因此该调用链同时最多一份 snapshot backing，不把返回值移动误计为多份媒体 backing。
- 本机 MSVC 14.51.36231 的 vector 普通移动构造为 noexcept，但 Debug 的 _Alloc_proxy 仍可能分配。仅删除外层 Snapshot 的 noexcept 不能恢复预算失败语义。实施采用已有 MediaLineageUniquePtr 持有稳定容器，使 Snapshot 移动不移动 vector；容器对象、背板及实际 proxy 均须计费，失败在创建点返回原 allocation error。尚未构建验证。
- G_min 不能取 producer pending/receive 的两个槽位作为启动帧数。实际 WindowSelector 要求 candidate.presentation + 正 preroll 不晚于 display frontier；至少两个不同 presentation 点只是必要条件。未知时间戳密度下，任何有限容量都不能保证启动成功，仍保留容量不足和超时失败；不得用输出 fps 或 AU/frame 比率补出保证。
- MediaBudgetMemoryResource::create 自身当前使用普通 new 和 shared control，尚未证明纳入 F。不能通过 resource 自身分配自身而形成自持环，也不能将 CanonicalInput 已迁入资源视为所有 owner 已闭合。

以上为源码核验和实施约束，不是联合审查、构建或真实链路通过记录。完整准入、Windows 全矩阵及 RKMPP 仍未完成。

## 解开 decoder 与 Session 准入的时序循环

本轮代码核验：WindowSelector 对 display-point 要求最后 presentation 至少到达候选点加正 preroll；Coordinator::create 拒绝非正 preroll。因此至少两个不同 presentation 点是必要条件，但不是任意源都能启动的充分条件，也不是 producer 的 pending/receive 两槽。相同 PTS、迟到关键帧或过密时间戳仍可耗尽有限 Nv，沿现有 StartupCapacityExceeded/截止时间失败。音频 Na 保持自身 duration/clock/窗口事实，不能换算自 Nv。

[GStreamer bufferpool](https://gstreamer.freedesktop.org/documentation/additional/design/bufferpool.html) 先协商 allocator/min/max 再激活、旧引用排空后再改变存储的原则适用；[FFmpeg send/receive](https://raw.githubusercontent.com/FFmpeg/FFmpeg/master/libavcodec/avcodec.h) 明确零到多输出和启动时接受多个输入，不能推导 AU=frame。最终采用两阶段协商：bootstrap 保持现有 root 硬预算；在真实 encoder emission、route 和 decoder/filter 事实全部取得后，于 freeze 原子边界封未来容量。不得用当前 prefix 密度预测未来，也不得忽略已占用对象/背板直接缩小容量。准备顺序：

1. CompositionInputPreparer 并行完成原 Preflight::begin/observation 与源 facts 后返回仍存活的 raw capture；此时不安装 continuous Session。原 receiver、wire budget、开始时间、deadline 和取消信号不变；raw 缓冲耗尽仍失败，不暂停或重启源。
2. CompositionPreparer::prepareCandidate 仍选择实际输出 device、tile geometry 和 source candidate。SourcePreparer 先用原 RTP descriptor 打开真实 video decoder，取得同一个 context 的 retention，再安装 continuous Session。音频无需第二次打开：raw.audio.selectedDecoder 已保存 capability 的真实 open readback，planSourceForOutput 复用它。
3. 安装继续只调用原 Preflight::installContinuousSession：同时取得原 video/audio receiver mutex，核对实际 owner/旧 frontier/budget，锁内取得 prefix AU 下界和当前剩余 deadline；此阶段只执行 bootstrap 有限预算，不能声称已有完整 G，因为输出 encoder emission/实际 route 容量仍依赖 first-frame probe。按原 observedAt 合并重放两个原有 datagram prefix，调用 finishBootstrap，最后仍在同一锁区间发布两个 continuous owner。不得先发布一侧，不复制媒体、不清空未重放包；失败销毁候选 owner，原 capture 仍保留未完成交接的 datagram。
4. SourcePreparer 复用刚打开的 decoder 读取 Session snapshot 完成原 first-frame probe。原 deadline 不重置。若后续候选失败，已安装的 Session 和其 AU prefix 保持存活；新候选必须验证既有封印的 decoder/holder 范围，容量不够即失败，禁止重建 Session 丢 prefix 或重新开始时限。
5. seal() 保持原 receiver frontier 交接，并将完整候选求值器交给 Session::freeze。Session 在同一个原 m_mutex 区间内执行 publish、快照、纯求值和容量应用，最后才置 m_frozen；freeze 后 receiver 仍收包，禁止出锁后以旧快照覆盖新状态。快照至少包含各 stream 已接收累计 AU（用于历史背板下界，不能冒充所有 liveAU）、prefix、pending lease size/capacity、binder acquiring/lookahead/待出包、outputs 及各容器增长保留界，保留已转交引用的原 lease。源 mutex 内不调用 decoder、socket 等待或分配候选图。
6. 求解的未来 Cv/Ca 必须容纳快照已持有单元，容器费用覆盖既有保留背板与未来增长的较大者；F、动态 U、G 和 bootstrap 旧 raw/新 AU 重放重叠分别验证，同 owner 不重收。未知时间戳导致容量/超时失败仍允许。不足立即返回原有限准入错误，不迁移或释放仍活跃媒体以“腾出”预算。
7. 成功封印的 Cv/Ca、Nv/Na、readiness、route、实际owner和费用进入 SourcePlanner/StartupPolicy/Store/Generation/最终图复核；SourcePlanner不再以压缩容量代替Nv。首次实流仍须证明 SourcePresentation → DecodedPresentationSpan → Prepared。

关联文件为 application 的 InputPreparer、SourcePreparer、CompositionPreparer，以及既有 Preflight、SessionPlan/StoragePlanner、SourcePlanner；不改 socket 锁序、媒体状态机或外部依赖。现 SourcePreparer 读取已被移走的 captured.facts.request.mediaId/timing，同时改为显式接收仍持有的 source facts，避免 noncadence lineage 使用 moved-from 身份。其中先 open 后 install 和显式 source facts 传递已接通；freeze 两阶段接口、完整 G 与实际容量求解尚未完成，当前门禁保留。
### 原子交接及容器失败语义补充核验

Preflight::installContinuousSession 当前在 video/audio 接收锁均持有时创建 Session，按 observedAt 合并重放原 datagrams，finishBootstrap 成功后才 sealPreflight、清空旧 datagrams 并发布两个 continuousSource。提前打开 decoder 的重排必须保留此顺序及原 deadline；重放阶段旧 raw capture 与新协议/AU owner 同时存活，完整费用不得提前扣除旧捕获存储。

Preparer 当前把 captures[i].facts 移入独立 facts 数组，SourcePreparer 却仍读取 captured.facts.request.mediaId/timing；新路径必须显式使用存活的 facts[i]，不能依赖 moved-from 字符串状态。候选重试还须验证已安装 Session 的同一容量产品，不能重复安装或替换 decoder 后沿用不匹配的持有量契约。

新增 RTCP 容器暴露了 allocator 移动生命周期问题：MSVC Debug vector 移动后源容器仍可持有 proxy，源 allocator 必须保留可用于释放的资源。实现者已将 allocator move 改为保留源 scope；这仅解决资源丢失，vector 自身 noexcept 移动中的 proxy 分配失败仍需处理，不能据此判定失败语义通过。
### 常驻配置信令 owner 的实际读回

本轮新增 MediaStandardContainerStorageReadback，并由 StoragePlanner 的 SourcePresentation 分支实际消费：本机 MSVC 145/202604、C++20 下，字符串按 xstring::_Large_mode_engaged 排除内嵌 SSO，外部分配为 capacity+1（含终止符）；vector 为实际 capacity×元素大小。Debug 的独立 proxy 请求另计。它描述普通 allocator 请求，不加不存在的 PMR header，也不包含 CRT 堆内部开销。原 cadence 不因新 readback 增加平台门禁。

依据是标准的 capacity 契约及本机 xstring::_Allocate_for_capacity/_Deallocate_for_capacity、vector backing 实现。标准仅保证逻辑容量，不独立保证某个供应商的全部分配布局，因此 adapter 明确限制已审计版本，不把 MSVC 布局用于 RKMPP：https://eel.is/c++draft/string.capacity 、https://eel.is/c++draft/vector.capacity 。

视频信令由 planner 解析一次并存入 SessionPlan::videoSignaling，会话直接消费同一移动交接的 owner，删除运行期重复解析和第二份持有。该改动及配置字符串读回尚未构建、独立审查；解析临时峰、计划移动时 Debug proxy 并存和其他协议 owner 仍须另行闭合，不能用常驻容量读回替代完整 F/U/G 证明。
### freeze 协商的并发边界

源码核验：Session::freeze 只在 publish 成功后切换 m_frozen，接收线程继续 consume，后续数据进入 runtime outputs；因此 freeze 并不等于源数据停止变化。最终占用快照、纯费用求值和新准入应用必须处于同一 Session 锁边界，或使用明确的 revision 重验协议，不能无锁依据旧 size 缩限。admittedUnits 只统计冻结前累计接收，不等于全部当前 live AU；pending、acquiring、prefix、outputs 及已交接 owner 必须按各自生命周期核对。

### 全会话联合封印的必要补充

单源 freeze 接口已经接到 Preflight，PreparedRtpSourceView 已暴露不可变 continuousAdmission；当前调用显式保留 cadence 的空 solver，完整求解未完成前不解除 SourcePresentation 门禁。安装前先完成候选元数据副本，安装成功后只移动 owner，避免已发布 Session 后复制失败留下半份匹配事实。

各 Session 的预算容量均来自同一个会话 B，不能分别最大化到 B。最终须统一取得所有 Session 锁后快照，一次联合求解，验证全部产品和原截止时间后一起无分配应用；单源先提交不能作为联合准入。原接收线程不被停止，锁顺序以唯一且稳定的 source 列表为准。已有容器下界与未来峰须逐 owner 取较大值再合计，不能对两个总和取 max。IdentityProbe 的一个 bootstrap 与 snapshot 的临时 bootstrap 是不同同时存活对象，分别计入，不能互相替代。

Nv 至少两个不同 presentation 点仅是正 preroll 的必要条件；把两槽直接作为最终容量会使真实 30fps、500ms preroll 稳定失败，不满足本任务。剩余预算必须用于所有源的容量扩展，Cv/Ca 与 Nv/Na 仍为独立维度。调查 [Berkeley/USENIX DRF](https://digicoll.lib.berkeley.edu/record/136796/files/EECS-2011-18.pdf) 后确认：其 progressive filling 的等价证明基于可分资源和固定 demand vector，本例整数容量及 deque/hash 阶梯费用不直接满足该假设。因此源间按真实字节成本公平分配可借鉴其原则，但未证明的离散轮转算法不能标作等价实现；具体联合求值仍待完成。

另一个真实消费者门禁是 source generic resource ledger 要求 productionRate，而 SourcePresentation 没有源 cadence。SourcePlanner 必须直接消费封印 Cv/Nv 的源队列与 startup 产品，不能填输出 fps 绕过；输出 ledger 的原算法保持不变。完整 G 仍需涵盖 frame payload/readiness 元数据及跨边/节点的 CNAME owner，而不止 startup index 或 Source outputs。

### 2026-10-29 接线核验与构建修复

SourcePlanner 已增加封印 route 与实际 runtime metadata/atomicMetadata 策略，以及解码器、原子输入完成、滤镜持有量的一致性校验。首次 Release 全量重建在 RTCP packet 聚合初始化处失败：explicit WireRange 构造不能由省略类型的空列表完成；现已显式构造两个空借用范围，正在重建，未宣称通过。

独立只读审查确认仍无 BatchAdmissionSolver 的具体实现，application freezeBatch 仍传 nullptr，输入入口仍为 CanonicalIntervals。另发现 SourceClockPlanner 把 startup.videoCapacity（解码帧 Nv）用于 acquiringCapacity（压缩 AU）；新路径须独立传递封印 acquiring bounds，并完成 takePacketClock 的封印消费，不能只移除 canonical 校验。上述缺口未闭合前，旧路径真实流通过不证明 SourcePresentation 完成。

本轮第二次 Release clean-first/all-target 重建已实际完成：762 targets，configure/build exit code 0。仅证明编译链接通过，未运行媒体验收。独立审查另确认 validateAudioRetention 仍约束旧 audio admission 等式，须随 sealed 音频 compressed/acquiring 产品一起修复。

### 压缩包 acquiring 与启动帧容量分离

SourceClockPlanner 现在通过 SourceRuntimeRequest 消费同一 sealedAdmission，decoded 视频/音频的 acquiringCapacity 来自各自 bounds.acquiringObjects；startup Nv/Na 不再被当作压缩包等待容量。RuntimeInputValidator 核对 assembly 与封印一致，generic 调用明确传入空 admission。takePacketClock 的封印路径核对 owner、成员和 expected/plan/实际 binder 容量，并按 compressed/prefix 上限交接，不扩容；音频独立保留 Na 与 startup 的一致性检查。冻结在 apply 前验证该音频产品。本轮局部复审及 Release 重建正在完成，联合 solver 和新路径实流验收仍未接通。

本轮 acquiring 分离及 sealed handoff 合并修改已由两个未参与实现的独立智能体分别明确局部 PASS；随后 Release clean-first/all-target 重建完成，configure/build exit code 0，762 targets。未启动媒体测试；该局部结论不涵盖尚未实现的联合费用求解或 SourcePresentation 真实流验收。

### 帧所有权和 readiness 类型几何

新增 MediaFfmpegPayloadOwnershipLayout，复用真实 LineageAllocator 的 shared geometry，描述 References/Completion、PayloadOwner 和 reference/replacement 元素。Replacement 与实际事务数组共用同一类型。CUDA adapter 暴露自身私有 CudaFrameReadiness 的真实 shared 请求，非 Windows 明确 unsupported；不创建 CUDA event，不计驱动或 GPU 私有分配。元素布局不是 vector 总费用，尚须组合 backing/proxy、同时持有量和临时峰。当前费用求解尚未消费这些出口。

视频引用数的权威上界已有依据：FFmpeg 官方 AVFrame::buf 文档明确视频所有引用均在固定 buf 数组内，extended_buf 用于超出数组容量的平面音频。本机 D:/mabs/build/ffmpeg-git/libavutil/frame.h:637–657 同样规定；因此视频可按 AV_NUM_DATA_POINTERS 推导上界，无需把首帧观测当全流上界。依据：https://ffmpeg.org/doxygen/trunk/structAVFrame.html 。后续只在视频契约入口验证该条件，不能把视频限制放入共用音频 retain 函数。本轮几何出口正在双审及 Release 重建，尚非总 G 或新路径实流证明。

本轮类型几何出口通过两位未参与实现者的独立局部审查，随后 Release 全量重建实际完成，762 targets、configure/build exit code 0。未运行媒体测试。下一步按视频固定引用数组契约组合 ownership backing/proxy/公开引用头和 readiness 常驻/临时峰，并接入联合费用，不能把未被 planner 消费的布局出口视为准入已完成。

### 视频引用上界的项目内适用边界

进一步调用链核验发现 MediaVideoCanvasProducer::publish 会在 extended_buf 添加 HeaderLease。FFmpeg 的固定视频数据引用数组契约仍成立，但不能把它推导成所有项目视频帧 nb_extended_buf=0。准备存储费用须区分数据引用上界与生产者额外所有权引用，并使用明确的内部产品组合；不能在通用 wrapFrame/retain 接口拒绝现有 canvas 租约。当前尚未增加该全局校验。

### 单帧 ownership 费用组件

MediaVideoFrameOwnershipStoragePlanner 已新增，输入为内部已规划的 extended 引用上界、hardware frames context 和既有 readiness Kind；不提供默认值。返回 snapshot、attachment、scratch、CUDA operation、completion 五个分项。空 vector reserve 按本机 MSVC 145/202604、C++20 的实际一次 allocate(count) 与 proxy 费用计入；Lineage 请求计真实资源头，FFmpeg 公共 AVBufferRef 仅计逻辑 sizeof，不宣称已迁入实际资源收费。CUDA probe 只在 CudaEvent 分支调用。

组件尚未接入联合求解器，缺少上游引用产品与并发持有组合；不能把五个分项直接相加为全图费用或将构建通过当新路径验收。本轮正在独立双审及 Release 全量构建。

单帧 ownership 费用组件已通过两个未参与实现者的独立局部审查；Release clean-first/all-target 重建实际完成，763 targets、configure/build exit code 0。没有启动媒体测试，费用组件仍待接入生产规划，不能据此证明联合准入或新路径通过。

### 引用数量产品接线前的滤镜核验

本机 vf_scale_cuda.c:566–576 的实际缩放分支通过 av_hwframe_get_buffer 获取输出再复制属性；:598 的 passthrough 分支直接传递输入帧。因此滤镜引用上界不能仅凭名称设为零，须继承已证明的输入引用产品并覆盖直通分支。MediaVideoSourcePlan 目前已有 decoderOutputAllocation、decoderReadiness、filterReadiness 和 filterInputConsumption，但尚无引用数量产品；下一接线应从已审计生产者的输出契约形成该内部产品，再同时供费用组件与发布校验消费。外部源码仅只读核验，未修改或重建 FFmpeg。

运行时滤镜输入核验位置已确认：VideoFilterNode 在 clone pending frame 前调用 MediaVideoFrameReadinessBinding::admitInput；后续 submitPendingInput 才生成输入引用 snapshot。引用契约必须在此前入口验证，并同时覆盖准备期 SourcePreparer 的直接 snapshot/capture，不能只在输出发布后验证。当前内部契约接线正在实施，入口仍显式 CanonicalIntervals，未切换新路径。

### 引用契约与共用 route 持有量接线

引用上界由 DecoderOutputAllocationAdapter 产出，SourcePresentation 准备阶段消费并形成 decoder/filterOwnershipStorage；scale/null 继承输入上界，准备期 snapshot、运行时滤镜输入和 CUDA capture 均校验同契约。该局部已双独立审查 PASS。

新增纯 MediaStartupRouteMediaPlanner，既有 Event/NodeMediaPlanner 在保留拓扑身份验证后复用同一媒体单位计算；输入保持正 int 容量上界，Extractor按role去重，未使用占位图。合并修改正在双审及 Release 全量重建；联合solver、费用总和及CNAME group路径仍未完成。

引用契约接线与纯 route 持有量抽取的合并修改已通过两个独立智能体局部审查，Release 全量重建实际完成：764 targets，configure/build exit code 0。未运行新媒体链路。下一项为实际 RTP clock group snapshot 在运行时边/节点中的 CNAME 持有费用；不能使用 startup 媒体六条边替代该独立路径。

最终 SourcePlanner 的非 cadence 分支现要求 capability 引用上界、readiness 与 decoder/filterOwnershipStorage 同时存在且一致，并通过费用planner重算逐字段核对；两位独立智能体已给该局部 PASS，尚未重建。该比较是单操作费用组件比较，不表示 alias 一定新增一份帧总费用。

CNAME实际路径核验：MediaRtpClockSnapshotFanoutNode 通过 pushToAllOutputs 转发同一 MediaBufferRef，未复制 snapshot 内容；必须区分多条边的共享引用和不同时刻的不同快照，且现有 source 费用已包含 binder 值复制。后续共用路径产品应据真实fanout及pending生命周期规划，不能逐边机械倍增CNAME副本。

### 候选费用与 CNAME 接线冻结

MediaPreparedSourceCandidateCostPlanner 已组合固定/未迁移费用、逐owner历史容器峰、实际 pendingCapacity、全部存活 lease control、packet/frame wrapper 和 startup index/generation。容器总费用扣除原pending控制块分项，再以all-live控制费用替代；输出命名分项，不生成完整Admission。

MediaRtpClockRouteStoragePlanner 已从共用8条连接描述计算额外 evidence/snapshot 队列、在途与binder replacement分项，并由SourcePlanner非cadence分支使用真实datagram和metadata策略消费，保存clockRouteStorage。两部分正合并双审与Release全量重建；AU payload、route wrapper/数组、reservation和frame ownership重叠尚待组合，不能声称完整F/U/G闭合。

候选费用分项与CNAME接线的合并修改已通过两个独立智能体局部审查；Release clean-first/all-target重建实际完成，766 targets，configure/build exit code 0。没有新媒体测试。下一实施项为startup wrapper/批次数组/reservation费用，要求复用实际owner字符串与channel契约，不能用单对象费用替代同时持有总额。

普通边算法抽取核验：FinalGraphResourceLedgerCompiler::queueSlotCount 的 SPSC capacity+1 是预分配哨兵槽，不是额外 live payload；节点 base+incident capacity 是引用保留上界，也不是唯一媒体总数。候选组合必须分别保留 storage slot、reference slot 与唯一 payload 产品，再依据实际producer/alias关系归属，不能将上述数量互换。
### 启动批次分项接线审查

MediaStartupBatchStoragePlanner 已由 SourcePlanner 使用实际 runtime.groupKey、prepared reservation layout 和封印启动容量生成并保存。分项覆盖 wrapper、released/staging 数组、groupKey、音频 wrapper、两类 reservation 与 authorization 临时存储；运行时共享 owner 常量，固定诊断名使用既有 literal 类型。两位未参与实现者已分别给出局部 PASS；尚未重建或运行媒体测试。reanchor 的新旧数组并存及各分项并发倍数仍须由联合费用组合处理。

进一步核验确认，普通视频/音频 source branch 连接目前由 builder 在 sealed admission 后生成；早期准入不能另造一份近似拓扑。正在将真实连接定义提为 planner 产品，供 builder 与早期 residence 投影共同消费。typed residence 遍历须保留 startup owner 替代、多个 retention owner 单次计费及下游 producer 不截断 alias 的现有语义。联合 solver 与 SourcePresentation 入口仍未接通。
启动批次分项与 typed residence 当前修改的 Release clean-first/all-target 重建已实际成功：767 targets，configure/build exit code 0；CMakeCache、build.ninja 及两个 CLI 产物均已核对。未启动 local 或 realtime 媒体测试。typed residence 的局部审查依据当前调用链与不变量；由于目标文件未跟踪且没有可确认的抽取前快照，不宣称完成逐行历史等价验证。普通 source branch 连接产品仍在实施，联合费用与入口切换仍未完成。
### 启动路径并发费用与共享预算剩余项

已新增并由 SourcePlanner 消费 startupRouteStorage，组合队列槽与真实节点同时持有上界：sequencer 重试时原事务、旧重锚事务与新临时事务并存；extractor 替换前新 staged 音频 wrapper 与旧 reservation 的 wrapper 并存。普通音频下游已发布 wrapper 仍需另外按实际 residence 计入，routeBytes 不是完整源费用。本次接线正在独立审查，尚未重建。

CompositionPreparer 在 freezeBatch 后才为输出编码器和画布向同一 storageScope.root 预约费用。因此联合源求解不能独占 B，必须同时计入这些共享预算消费者及准备阶段重叠；不能仅满足所有源之和不超过 B 就宣称全会话准入成立。

CUDA readiness 代码核验：complete() 缓存完成结果但不释放 consumedInputs，滤镜输出仍存活时，其输入 snapshot 和上游 attachment 继续保活。运行时 null filter 也通过 publish 新建 readiness，不能沿用准备期 null 分支的复用假设。费用组合应按真实对象寿命，不按尚未完成的 GPU 操作数计费。
startupRouteStorage 已取得两位未参与实现者的独立局部 PASS，尚未重建或运行媒体测试。普通视频/音频连接定义已抽为 planner 产品并由真实 builder 消费；SourcePlan 保留同产品供建图核验。启动前 decoder/gate 连接和早期完整 residence 仍在接线，端点绑定必须与实际图一致，不能仅以边名、payload 与 policy 相同证明拓扑相同。
### 2026-10-30 普通分支与帧并发费用复验

普通分支连接核验已复用 builder 的角色绑定，比较实际节点/端口、payload、policy 和唯一边名，并拒绝进入本源普通视频/音频内部节点的未规划边。两名新的独立审查者均给出该范围及帧 ownership 费用接线的局部 PASS；该结论不涵盖启动协议、aggregate 等全域早期拓扑，也不把帧费用必要约束当完整预算。

Release clean-first/all-target 重建实际成功：772 targets，configure/build exit code 0；cache、ninja 与两个 CLI 产物已核对，realtime 产物时间为 2026-10-30 16:36:18。未运行 local 或 realtime 媒体测试。下一项为启动前输入连接和特殊节点持有量的共用规划，再完成完整早期 residence、输出/画布共享费用及联合求解器。
### 2026-11-02 输入连接与输出预约实流回归

启动前 Input/Startup/Clock 连接已由实际 builder 消费并按真实端点集合验证；节点 reference 计算已复用统一 planner。输出/画布纯 retention 与费用计算在 freeze 前各预约一次，后续实际图与分配逐项核对。两位独立审查者给出局部 PASS，Release 全量776项目构建成功。

真实回归仍揭示缺陷：r368默认沙箱中匹配RTP包为0；r369正常环境在普通分支核验失败；r370增加错误边名后定位为音频mapper追加.encode，而验证仍用.audio。修复保存并使用实际audio选项prefix，保持端点与额外入边检查；两位独立复审PASS，正在重建原规格复验。三轮失败证据和清理记录分别归档，不能将局部源码审查或构建成功记作媒体通过。完整early residence、联合solver及SourcePresentation入口仍未接通。

### r371 原规格回归结果

音频前缀修复后已进入三源真实输出并取得VLC原生三格快照；约104秒时发送超过原预约期限373724ns，整轮FAIL。最终媒体预约/释放97354次匹配并归零，压力失败0；失源与重入未完成。三个初始源日志均3600帧/120秒，但原进程句柄失效，退出码未确认。完整命令、失败及指标见 realtime-composition-r371-validation.md，临时产物已按清单清理。r366同类发送期限问题仍未关闭，不能将本轮作为成功验收。

### aggregate边界连接共用产品

新增MediaCompositionAggregateConnectionsPlanner，由真实CompositionGraphBuilder消费，覆盖每源video/epoch/选中或丢弃audio以及输出codec/frame/activation五条边。角色绑定实际branch返回端点，逐条核对stream/payload并沿用显式policy。首次双审发现端口跨节点命名域混合，已改为VideoAggregate和AudioAggregate各自输入域唯一性，并显式拒绝audioPort与audio_codec/activated冲突；两位非实现者均复审局部PASS。Release全量重建进行中。

该产品仅闭合aggregate边界定义；统一role身份、共享aggregate owner的单次计费、完整early residence和联合solver仍未完成，不能将局部PASS当完整准入。r372实流已FAIL并归档，视频生产长耗时与音频等待链仍需继续定位。

aggregate边界连接抽取完成双独立局部PASS后，Release clean-first/all-target全量重建实际成功：777 targets，configure/build exit code 0。cache、ninja及两CLI产物已核对；本次尚未运行新媒体验证，不改变r372 FAIL结论。

### r373失败驱动的准备交接修正范围

已确认后续源仍处raw捕获时，前源串行等待首帧/准备tile，使后续源达到wire容量边界；当前容量停止在finite replay路径是可封存prefix状态，不能无条件改为全局captureError。连续安装却把该终态报告成owner不匹配，候选循环继续后由QSV能力拒绝覆盖首因。

实施方向：保留类型化捕获终态及容量事实，连续安装先核身份再传播原始接收/预算/终态原因；按输入是否可重用决定能否继续候选，不按字符串判断。准备拆为所有源先open/retention/install、再等待首帧/tile，具体依赖正核验；不改变原deadline、预算、同一decoder所有权，不增probeSize，不并发调用未经证明线程安全的设备adapter。依据沿用GStreamer有限池与FFmpeg零到多输出契约。此项尚未实现，不记为修复通过。

### 普通分支实际角色绑定

Video/Audio builder现在直接使用并返回真实角色端点数组，decoded解码器绑定经InputEndpoints透传；Composition按实际NodeId/PortId核验端点和内部节点全集，不再按普通分支prefix反查。新增类型为内部builder产品，未修改公共API。9文件已冻结、diff-check通过，一位独立审查者局部PASS，另一位复审中，尚未重建。Input/Protocol其余名称绑定和统一early拓扑仍待完成。

普通分支实际角色绑定取得两位独立局部PASS，Release全量777项目重建实际完成，configure/build exit code 0，四项产物核验完成。本次尚未实流验证。r373接管顺序修订详见 realtime-composition-preparation-handoff-plan.md，设计冻结待审查，生产代码尚未实施。

准备接管修复12文件已获两位独立局部PASS，Release778目标全量构建实际成功，configure/build exit code 0，四项产物已核对。r374原规格realtime运行中，尚无验收结论。

r374已结束并归档：协议全部先安装再decoder绑定，三初始源及重入120秒exit0，caller-stop CLI exit0，233083预约/释放匹配归零，errors/pressure0。两次黑屏及左下恢复有本轮原生快照；初始三格由残留旧VLC按旧前缀捕获，仅作注明来源的补充。旧VLC占端口已纠正且精确清理，测试隔离曾失效，发送delivery仍not_proven，故不判完整验收通过。全部非周期诊断与CPU采样已归档，临时文件已逐项清理。后续必须同启动执行环境核验进程和端口，不能用默认沙箱不可见当无残留。

### r374运行诊断后续与输入身份接线

r374最慢canvas7.9606ms主要在同次batch_completion7.6983ms，最慢encoder send18.7748ms；未复现r372长调用，不证明已修复。Sender汇总新增读取既有worstLateGlobalSequence的日志字段，与maximumSubmitLateness成对输出；0表示无正延迟样本或controller不存在，仅对应成功提交统计，不改变发送策略。该4行诊断已双独立局部PASS，尚未构建。

Input/Protocol正在透传真实shared/RTP节点绑定，使实际连接与Composition validator共用端点映射，移除剩余名称反查；未冻结，不代表完整early residence或联合solver已完成。

Input/Protocol实际绑定9文件已冻结：原SharedNodes及RTP fanout/adapter身份随builder结果返回，连接与validator共用role映射；validator以真实内部节点集合拒绝未规划入边，排除普通decoder分支并包含RTP group。全src已无mediaBranchNamedNode。非RTP端点alias保持原行为，未开放非RTP合屏。双独立审查与Release全量重建进行中，尚不记PASS。下一项为共用planner角色身份/位置及完整early residence，不能将真实ID返回当完整预算闭合。

### 普通源分支节点声明闭环

本块先覆盖Video sourceDecoder/sourceFrames与Audio source，Input/Protocol及输出域声明留后续。复用既有shape的presence，将角色的owned kind提为同产品；外部端点只声明presence，不猜kind或拆分实际alias。真实builder按声明创建源节点并校验实际绑定的owned presence/kind及必需外部端点，禁止增加未消费的平行节点表。

沿用上文GStreamer有限池及引用保活原则，节点声明只提供实际拓扑身份依据，不是分配上界算法。线程、背压、队列容量、RAII与失败语义不变；失败在建图阶段返回原错误类别。未完成跨segment角色等价、完整incident capacities、共享aggregate owner及联合solver，本块不得记作完整early residence或准入通过。

Input/Protocol实际绑定及sender诊断已完成双独立局部PASS和Release clean-first/all-target全量778目标重建（配置/构建退出码0，四项产物核对）；r375按原规格运行中，尚未形成验收结论。普通Video/Audio source声明闭环8文件也已取得双独立局部PASS，尚未重建；两批修改与对应验证状态不得混用。

r375已结束：Input/Protocol实际绑定版本完成三格→全黑→所选源重入→全黑的本轮独占VLC原生快照观察，三初始源及重入完整120秒/3600帧exit0，CLI caller_stop exit0；226023预约/释放归零，errors/pressure0。sender最大成功提交迟到32.203532ms、worst global sequence145047，delivery仍not_proven；不能以本轮未复现关闭r371/r372失败。命令、原始诊断和内存采样见realtime-composition-r375-validation.md，16临时产物已逐项清理。普通Video/Audio source节点声明修改另行双审PASS，Release全量重建进行中，不将r375证据归属于尚未构建的新声明版本。

### Input/Protocol声明与源域位置投影

本块沿既有有限池/引用保活原则，把真实创建节点的presence/kind及跨segment角色等价提升为共用内部产品。Shared启动节点继续调用StartupRouteShape::nodeKind，RTP raw/group/fanout/binder/adapter使用同一RtpClockRouteShape。真实builder消费声明；不建占位图、不猜名称、不变更线程/背压/容量/所有权或外部参数。创建责任不等于预算owner。

纯源域投影复用原Input、Startup、RTP、Video/Audio与aggregate连接产品，包含metadata及跨边界incident边。Format绑定真实raw.format；cadence decoder接released video，decoded decoder接generation gate，frame段分别接decoder或released frame；实际typed bindings验证这些等价关系、kind、全部源内节点和incident边。输出codec与aggregate为显式外部位置，kind不虚构；只核验本源owned节点全部incident边，不声称外部边界incident完整。源域投影在真实节点创建前形成，建图后复核；尚未接完整费用求解和多源共享owner结算。

本块已接通声明创建、建图前纯位置投影与完整生产图后的实际核验；owned位置保存含metadata和跨边界边的incident容量，external位置明确不提供完整容量。普通branch连接新增显式stream并由实际builder核对两端端口，原输入/分支重复校验合并到源域核验。21文件冻结待双独立审查和全量重建，未运行本块媒体验证；不代表完整residence或solver通过。

普通源节点声明8文件已双独立局部PASS，并完成Release778目标全量重建（配置/构建exit0，四产物核验）。其r376回归取得三格、全黑及所选源重入快照，但重入阶段seq155923发送预约窗口不可行，CLI exit1，判FAIL；三初始源及重入均完整120秒exit0，201377预约释放归零不改变失败结论。payload pressure计数1经源码对应新增waiter的WouldBlock，encoder0.8017ms后恢复，不与约55秒后的发送首因混同。完整证据见realtime-composition-r376-validation.md，15临时产物已清理。后续21文件源域投影及最差成功提交样本均尚未构建运行，不能归因或宣称已修复。

源域拓扑21文件与最差成功提交样本诊断均取得两位独立非实现者局部PASS；合并Release clean-first/all-target779目标重建实际成功，配置/构建exit0，四项产物核验（realtime10:41:37）。r377已通过新的实际全源拓扑校验进入输出，三格原生快照已查看；测试仍在运行，不宣称完整验收。

r377已归档：最新源域positions实际校验通过并进入输出，三格→全黑→所选源重入→全黑均为本轮独占VLC原生快照；三初始源及重入完整120秒exit0，caller_stop CLI exit0，226085预约/释放归零，errors0，pressure1为已恢复容量等待。新worst-success样本seq165387与pacing汇总一致，wait API35.0965ms、同区间encoder调用38.2935ms，仍不能区分系统调度/计时器根因；delivery not_proven。16临时产物已清理，详realtime-composition-r377-validation.md。

### 后续完整输出域接线

下一块需扩展既有Video/Audio output shape并由真实builder消费，抽取scheduler/router、RTP/MPEGTS及datagram execution的节点/连接与实际bindings，再与源域外部codec/aggregate角色显式合并成全图positions，重算完整incident容量。不能用socket角色布局代替DAG拓扑，也不能照搬源域单producer约束：Separate RTP两materializer共享sender.batch，MPEGTS两codec共享mux.codec，必须复用真实端口multiplicity。aggregate现有一个storage owner覆盖两个retaining nodes，只计一次并保留普通retention跳过规则。之后才把typed startup/普通edge-node/shared owner映射给既有MediaGraphPayloadResidencePlanner，并与最终图retained objects逐项核对，最后联合solver。此节为只读调查后的下一实施范围，尚未实现。

### 输出域声明实施边界

先完成共用输出segment声明及真实消费，涵盖输出encoder、AV scheduler/router、RTP/MPEGTS与transport execution；共享协议builder的原AV/VideoOnly分支同时接通，独立VideoOnly scheduler不在本块声明范围。节点kind、连接角色/端口/stream/payload/policy由共用产品提供，真实builder返回typed bindings。Any输入和多producer端口继续复用现有port/core matcher，不复制源域单producer假设；不改线程、调度或媒体算法。沿既有有限池与引用保活原则，本块只是后续全图positions与owner核验的权威结构，不以拓扑声明代替费用上界。全图合并和aggregate单owner双retaining位置留下一冻结块，不重复计费或声称solver完成。

声明只依赖现有运行事实/shape/边策略，不依赖实际NodeId或sealed预算。当前源投影“建图前”不等于“freeze前”；不得为早期调用伪造sealedSourcePlan。输出声明已接原AV/VideoOnly builder，Any与多连接仍由原core matcher处理；本块冻结后需双审/全量构建，尚未验证实流。

### 源域投影纯事实输入

沿同一声明与实际绑定算法，将SourceTopologyPlanner输入由完整SourcePlan拆成同步只读facts view：members/readiness/clock、原policies/branch products、input/aggregate连接span。真实CompositionGraphBuilder先验证optional事实再调用，投影结果持有自己的容器，不保存借用view。解除sealedSourcePlan类型依赖，不伪造source或图，也未改变拓扑算法；当前生产调用仍在原时点，尚未接freeze前候选求值或联合准入。3文件已双独立局部PASS，等待与输出声明共同全量构建。

输出声明20文件与纯facts view3文件均已取得双独立局部PASS，正在同批Release全量重建。MPEGTS TsPlan.transport_plan边经实现者本轮修改前读取记录确认原已存在，位于adapter→mux之后、transport→materializer之前；此次只搬移到同声明，不据HEAD缺失将其归为本轮新语义。后续全图合并仍须区分多producer端口、source/外部角色等价及共享owner，当前子块不声明完整准入。

### 全图角色合并与实际全集核验

沿前述GStreamer显式pad连接与有限池引用保活原则，本块新增纯全图位置产品：地址区分源序号、域、segment与role；源序号沿已逐项验证groupKey的aggregate/source顺序。外部codec、frame、activation、packet及execution引用通过显式attachment解析到owned位置和准确端口，同一产品由实际Composition builder消费，不按节点kind或相同role名推断等价。aggregate两节点kind同时用于实际创建。

源域既有全部连接只合入一次，另追加aggregate输出、编码器、scheduler、protocol、execution各自连接，全图重算incident容量（自环一次）。完整生产图统一核验kind、唯一owned位置、attachment端口、节点/边全集及source/output互斥成员全集；按完整from/to匹配，复用MediaPort::accepts并只对multiple=false限制连接数。原source validator收窄为runtime registration身份核验。声明重复不按label或policy消隐。

不改变线程模型、背压、队列边界、媒体处理、RAII或发送策略；缺失/冲突仍在建图失败。当前仅完成全图拓扑闭环，尚未构建或实流验证，待两个独立审查。aggregate一个storage owner对应两个retaining positions的费用、完整residence映射、freeze前候选调用及联合solver仍未实施，CanonicalIntervals/freezeBatch(nullptr)门禁保持。

### 全图residence实际消费设计

继续复用前述GStreamer有限池和引用保活模型，不新增容量或遍历算法。保存已核验全图position/connection到真实ID的拥有型映射；纯owner编译使用startup route、完整incident、真实ingress/acquiring/filter与prepared encoder事实。typed startup替换普通节点/边计数，edge owner挂source position；aggregate单owner含两个retaining positions；下游producer不截断alias。

最终CompositionResourcePlanner用纯遍历结果形成源producer预算，并以真实图reference storage、typed startup与可达集合逐项readback核验，任何差异失败。decoder/filter readiness仍按所有存活输出及consumedInputs保活计算，不只统计未完成GPU操作。本块不改变线程、队列、背压、生命周期或发送策略；不接联合solver、不伪造sealed事实，SourcePresentation与freezeBatch(nullptr)门禁保持。

本块同时发现并处理共享账本身份歧义：合法RTP多materializer边可以显示同一label，原residence按owner字符串反查会混淆边。普通node/edge条目现保存显式ID，fixed retaining group保留完整组；无媒体reference的控制/配额条目显式标为None，未声明或身份域冲突拒绝，显示owner仅用于诊断。该共享路径影响单源、动态输出及Windows/RKMPP，合屏链路通过不代表这些矩阵已覆盖；需双审关注所有entry生产点与消费者。

实现已接通最终生产消费：Topology拥有完整projection和验证返回的NodeId/EdgeId映射；SourceResources形成独立纯facts；ResourcePlanner采用纯retained数、保留additional/minimumProgress，并逐producer比较真实图可达集合与retained数。readback同时检查普通node/edge、typed startup、真实holder与aggregate单owner双位置；decoder/filter ownership仍仅核对已预约组件，不重复收费。20文件自查冻结待双审，未构建或运行本块实流。早期source/aggregate候选facts组装、完整字节费用和联合solver仍未接通，本块不构成SourcePresentation完整准入。

本块20文件已获两位未参与实现者独立局部PASS。Release clean-first/all-target784目标重建成功，配置/构建退出码0，四产物核验，realtime产物2026-11-02 11:33:21。r379已通过新全图及residence核验进入输出，本轮VLC原生三格快照已查看；测试仍在运行，不记完整验收通过。此前57133构建会话退出码无法回收，保留未确认边界，不以产物时间代替成功证据。

r379已结束FAIL：新全图与residence逐项核验通过并有运行中VLC三格快照，但约110秒音频AU4486在FileMux错过服务窗口25.172154ms，CLI退出1。同期scheduler等video144.5322ms、canvas batch completion132.5144ms；仅为相关证据，未证明GPU或OS根因。三初始源完整120秒退出0，未执行全失源/重入；98183预约释放归零、errors1、pressure1，senderdeadline_misses0且delivery not_proven。完整命令/首错/指标见realtime-composition-r379-validation.md。五PID及端口无残留，11临时产物逐项清理。不得以纯预算局部PASS或计数归零宣布实流稳定。

### 共同容量与aggregate事实内核设计

沿用已核实GStreamer先协商allocator/min/max、后激活实际pool的分层原则，本块将startup/route/queues/acquiring/window提为不含owner或sealed准入对象的内部纯事实。最终sealed adapter继续核真实owner、总额和成员，SourcePlanner实际消费adapter输出；AvSync、StartupPolicy、SourceClock及assembly校验共同消费纯容量产品，不构造半空Admission。AggregatePlanner改为窄source事实view，最终生产调用映射同一runtime事实，复用原candidate和metadata费用算法。

协商原则不提供distinct AU lease或wrapper数量证明；本块不以residence引用数冒充allocation数，不新增费用公式，不放宽CandidateCostPlanner拒canonical规则，不接solver、不切SourcePresentation入口。线程、媒体状态机、背压、队列上界、RAII及失败类别保持；真实sealed所有权核验留在最终adapter，纯事实缺失/冲突在规划失败。

本块已接实际生产消费：SourcePlanner仅从真实sealed owner adapter取得纯容量产品，再由AvSync/StartupPolicy/SourceClock规划，startup route费用使用同一产品；adapter保留owner/总额/成员/route/正window核验，纯policy继续核window与原启动策略相等，runtime共同层核queues/route，assembly核单位/字节/acquiring一致。Aggregate的窄borrowed view由最终CompositionPreparer真实runtime映射，返回拥有型字符串，原candidate和metadata公式未改。17文件冻结待双审及全量构建，未运行本块媒体。该抽取仅移除容量和aggregate计算的sealed类型耦合；完整早期SourcePlanner组装仍含实际allocation/reservation边界，distinct AU/wrapper和预算域映射、完整费用与solver仍未完成。

共同容量17文件与CUDA timing5文件完成双独立局部审查后，首次统一Release全量重建失败：session61336最终exit1，编译命令exit2；MediaRealtimeCompositionPreparer.cpp:317报C2665，vector<MediaAvAggregateSourceFacts>::push_back初始化列表类型不匹配。配置/生成成功不等于构建通过，未启动本版本媒体。正在核对domainRole等实际optional字段映射；修正后须重新全量重建，保留此次失败。

共同容量块首次统一Release全量构建61336失败（exit1/compile2）：CompositionPreparer.cpp:317的aggregate facts初始化触发C2665。根因为真实synchronization.domainRole是optional，而窄facts误声明为非optional。现改为保留optional，直接传递缺失事实；AggregatePlanner原有与SourceContribution比较继续拒绝缺失或错误角色，不默认取值或解引用。此次仅修正facts字段及本记录，重新冻结待双审和全量重建；此前局部审查PASS不代表编译通过。

optional domainRole映射修正经两位复审PASS；第二次全量73836成功，785目标、配置/构建exit0、四产物核验realtime11:53:24。r380取得三格→全黑→所选源重入原生画面，但重入近结束seq171936发送预约失败，CLI1；源与重入均完整120秒0，不记通过。219474预约释放归零、errors2、pressure0；15产物清理且六PID/端口无残留。完整记录见realtime-composition-r380-validation.md。共享容量facts生产调用已接通，不代表SourcePresentation或联合solver已接通。

### 源共同规划与分配身份分层设计

继续复用GStreamer有限buffer pool的引用保活和排空原则，以及FFmpeg send/receive零到多输出契约；不把AU、frame、引用槽或wrapper当作同一单位。SourcePlanner的纯规划产品须与真实reservation/准备owner绑定分开，最终生产路径调用同一内核，sealed adapter保留实际owner、独立音频retention、producer holders及预算核验。

真实分配路径：Session的pending创建AU lease，完成后shrink并移入packet wrapper；PacketClock取走AVPacket、创建新packet wrapper、转移同一lease，转换期间wrapper可重叠但payload lease不增加。视频另有decode lineage/decode wrapper，解码帧再建display lineage；filter包装可共享display lineage。readiness publish创建frame wrapper及独立引用owner；cloneFrame新建AVFrame/wrapper但共享payload/readiness。AudioDecode/Resample及aggregate输出使用runtime域，不能并入source preparation域。旧prefix和容器历史仅是已持有下界。

现CandidateCostPlanner的packet/frame几何不能覆盖decode/display lineage对象；必须先取得这些真实分配几何、转换并发上界和预算域映射，才可评价完整费用。共同residence仅提供引用保留上界，allocation按真实创建/转移/别名身份分别归属；缺失事实直接拒绝，不用默认数量补齐。本块不放宽canonical拒绝，不接假solver，不宣称完整早期准入。

### 共同源规划内核冻结边界（2026-11-04）

本块已接通真实调用：`MediaRealtimeCompositionSourcePlanner` 从真实 sealed owner 取得容量事实、核验 raw/continuous 独立预算及独立音频 retention，再调用 `MediaCompositionSourcePlanningKernel`；内核统一生成 input、同步/时钟、holder、startup/route、branch/input connections。`MediaRealtimeAvSourcePlanningProducts` 不含 reservation owner；最终 adapter 比较 sealed producer holders 后绑定真实 allocation scope 和 reservation，形成完整 runtime。借用的 capacity、raw、ingress 等只在同步调用期间读取，返回产品不保存这些 view；未构造半空 runtime 或伪造 sealed admission。sealed resource 同时核对指针及 shared ownership 身份。

抽取沿用 [GStreamer buffer pool](https://gstreamer.freedesktop.org/documentation/additional/design/bufferpool.html) 的先协商分配属性、后激活真实 owner 原则；本项目仍使用既有 DAG/allocator，而非引入 GStreamer 或替换背压算法。共同内核移除未使用的 emission 输入；最终 cadence adapter 的既有 ledger 规划仍使用真实 emission。前置 member/cadence 检查保护 sealed 资源读取，内核相同事实检查保护独立调用，不能因抽取而删去任一边界校验。

平台边界保持：实际 source preparer 经 `MediaDecoderOutputAllocationAdapter::inspect` 只接受 CUDA hardware frames，随后显式仅接纳 `NativeCudaSafeCopy`/`CuvidCopy` 并探测 producer；SourcePresentation 的 `CudaEvent` ownership 核验消费该已有能力。此抽取不新增 RKMPP capability，不代表该平台链路已实现或验证。普通单源 A/V 的 runtime 聚合初始化同步适配产品分层，其媒体算法不变，但仍需共享路径回归。

冻结范围为 7 个代码文件：新增 `MediaCompositionSourcePlanningKernel.{h,cpp}`；修改 `MediaRealtimeCompositionSourcePlanner.cpp`、`MediaRealtimeAvSourceRuntimePlanner.{h,cpp}`、`MediaRealtimeAvSyncRuntimePlan.h`、`MediaRealtimeAvSyncRuntimePlanner.cpp`。已做调用点/字段映射和差异空白检查，尚未双独立审查、全量构建或实流验证，不记 PASS。

完整费用仍未闭合：未修改 CandidateCostPlanner、未添加未消费的 geometry、未接联合 solver；当前入口仍 `CanonicalIntervals` 与 `freezeBatch(rtpSources, nullptr)`。[FFmpeg send/receive 契约](https://ffmpeg.org/doxygen/trunk/group__lavc__encdec.html)允许一个输入产生零或多个输出，因此不能用 AU 数代替 frame/lineage 分配数。生产 decode/filter/encode registry 的实际域是 runtime，probe registry 则由传入 allocation 决定；packet payload lease 释放也不结束 PMR wrapper/control block 生命周期。后续须按真实创建、共享和释放边界完成跨 payload alias、分配域及上界核验，不能把本块当完整早期费用准入。

绑定证据分层：`RuntimePlanner::bind` 对 reservation 自身 domain 的检查不是独立 owner readback。实际 source 身份先由 SourcePlanner 的 sealed owner/resource 指针与 shared ownership 一致性核验保证；最终 `MediaRealtimeCompositionResourceBinder.cpp` 逐源将 reservation 与真实 `rtpSources[i]->videoReferenceResource()` 再比较，且独立比较 continuous allocationOwner、字节数、成员及 authority。其他内部调用方不能凭 bind 成功跳过最终真实资源核验。

共同源规划 7 文件已于 2026-11-04 获两名未参与实现者独立局部 PASS（source_kernel_review_a、stage_summary_refresh）。两者均核对真实调用、借用寿命、member/optional 防护、sealed owner 与最终实际 session 的独立 readback、旧单源初始化和既有 CUDA capability 边界，并对照 GStreamer 分配协商/激活分层。该结论仅为源码抽取审查；尚未全量构建或真实运行，不关闭 r380 发送失败、完整费用与联合求解缺口。

r381 已完成本轮原规格三格→全失源黑屏→所选源重入→再次全黑及 caller-stop 观察：CLI退出0，四次源均完整120秒/3600帧退出0；生产MMCSS同线程enter/leave native0，最终errors/pressure/deadline_misses均0，223612预约/释放归零。sender delivery仍not_proven，单轮未复现旧错不等于稳定性已修复，更不等于完整预算或合屏验收完成。完整命令和证据见 realtime-composition-r381-validation.md；16临时产物已清理，六PID及VLC端口无残留。
### 下一实施块：分配身份贡献、真实候选费用与最终消费（2026-11-04）

本节为实施前设计，不是已实现或准入通过。以 allocation-domain-audit 当前证据为基线；r381 的运行结果不能证明分配上界。不得只提交 geometry helper 或未消费的费用表。当前 `MediaPreparedSourceCandidateCostPlanner::plan` 无生产调用、`freezeBatch(rtpSources, nullptr)` 仍在 application preparer:297；本块必须把费用产品接入实际候选求值及最终图 readback，事实未闭合的候选明确失败，不能凭局部费用启用 SourcePresentation。

#### 权威方法及复用边界

已重新核对 [GStreamer buffer pool](https://gstreamer.freedesktop.org/documentation/additional/design/bufferpool.html)：allocator/size/min/max 先协商再激活，有限池对 outstanding buffer 背压，最后引用归还；新配置与未排空旧池可并存。复用现有 planner、有限 holder、预算 reservation 与 RAII，不引入 GStreamer，不自行设计池或回收算法。[FFmpeg send/receive](https://ffmpeg.org/doxygen/trunk/group__lavc__encdec.html) 明确输入输出可以零对多，内部有限缓冲的通用描述没有给出可代入费用的数值上界。故从同时存活的引用根证明 allocation 上界，不从 AU 输出倍率推测 frame 数；引用槽总和只是可证明的保守上界，不宣称每槽都有独立分配。

线程模型维持每 node 一个 process owner；session 接收与 graph owner 是不同并发域，二者局部临时值不能合并为全图一个 slot。采用现有 atomic completion、decoder retention、filter retention、queue/startup/aggregate 产品，不改变批处理与背压。缺事实、溢出、域冲突及 final readback 不一致在建图/激活前返回原错误类别；取消仍沿既有 owner-thread 停止和 RAII 释放。

#### 类型产品与上界证明

1. 扩展现有 `MediaPayloadResidenceOwner/Projection`，保留 typed owner key 和逐项贡献：普通 node/edge、typed startup node/event、aggregate shared owner；key 由全图 position/connection 与已验证 source 身份组成，不用诊断字符串。projection 同时保留现有总数供原消费者使用。aggregate 两个 retaining positions 仍只有一个 owner；typed startup 替代普通条目的规则不变。最终图投影用真实 NodeId/EdgeId 映射回相同 key，比较贡献全集及容量，不只比较总数。
2. 新内部 `MediaCompositionAllocationResidencePlan` 逐 allocation family 保存：source/stage 创建身份、分配域、holder 贡献、构造重叠项、强引用与 weak-only 控制块上界、已预约组件 key 和证明来源。候选期用逻辑 allocation owner key，最终绑定真实 resource 的指针与 shared ownership；不把未来对象地址伪装成候选 identity。family 必须区分 AU payload lease control、packet wrapper、packet decode wrapper、AU decode lineage、decoded display lineage、各转换的 frame/decode wrapper、registry State/entry/lease/opaque，以及已有 readiness/attachment/input snapshot。
3. 建立固定类型的实际保留关系，而非按 payloadKind 截断：packet decode wrapper 保留原 packet 与 AU lineage；frame decode wrapper 保留 frame、AU lineage、display；display 保留一个 AU lineage；filter/transfer 新 wrapper 共享 display；registry entry 保留其 lineage variant；opaque 保留 lease token，lease 的 weak State 可延后 State 控制块释放。`MediaVideoDecodeLineage` 只是身份/时间事实，不持 payload lease，不能因 frame 保留 AU lineage 而延长 AU payload 费用。
4. 对每 family，枚举所有可达持有根及创建中临时根，用 checked sum 给保守 distinct allocation 上界。只有代码保证同一 owner 的同一槽或同一共享组件才去重；不同根可能共享同一对象时允许保守相加，但不得再次收费同一已预约组件。嵌套对象按真实字段 multiplicity 传播：一个 frame holder 至多引用一个 display、一个 display 至多引用一个 AU lineage，无需 AU→frame 倍率。遇未知多重引用或未列出的持有根必须拒绝，不自动使用 1。
5. 转换瞬时重叠单独列项：PacketClock 新旧 packet wrapper 与 decode wrapper；VideoDecode 创建 raw frame/display/decode wrapper；VideoFilter、HardwareTransfer 的输入 pending、输出 raw frame 和新 display wrapper；cloneFrame 的新 AVFrame/wrapper。一个局部 slot 的依据必须是实际成员/局部变量和单 owner 执行，不是经验常量；receive 循环若先累积容器则使用真实容器容量。不能把不同 node 的临时根取 max，因为可并发运行。
6. registry 在同一 State 内以实际 capacity 限制所有代 entry；purge 只改代次，不清旧 entry，因此不能再乘假定代数。entry/lease/opaque 的容量证明还须覆盖 submit 创建中重叠、失败回收、State 替换后旧 lease 的 weak 保活。若 registry 替换缺少旧代 drain 证明，不得以当前 State capacity 覆盖历史 State。控制 EOF/Flush/Abort 的 weak 槽独立计入相应 runtime control family，不混入 packet/frame wrapper。

#### 费用归属与必须补齐的真实事实

| 产品/路径 | 已有可复用事实 | 本块必须导出或核实 |
| --- | --- | --- |
| PacketClock AU lineage、packet/decode wrapper | 明确 allocate_shared 的对象与 PMR；session pending/prefix/output 有上界 | 用实际 allocator 类型导出 shared control 几何；分别保留 input/output/构造根。AU lease 转移不释放旧 wrapper，lineage 不等于 payload lease |
| Decode display、filter/transfer wrapper、clone | MediaVideoDecodeBuffer.cpp:54/59/82 以及三个真实节点创建点 | 各节点 pending/local/publication 和 clone 调用的并发槽；Packet→Frame lineage 保留必须连通，display 共享但转换 wrapper 新建 |
| Codec registry | State 私有 map、capacity、每成功 submit 至多一个 opaque owner；生产 scope 是 Runtime | 在 registry 模块导出 State/control、真实 map node/sentinel/proxy、lease/control 与 opaque raw owner 几何；不能 sizeof(Entry) 替代 map node。probe allocation scope 单独投影 |
| CUDA ownership | SourceFrameOwnershipStoragePlanner 已按输出保活计 consumedInputs；已有逐组件几何 | 将现有组件 key 化复用，不重复收 decoder/filter ownership；按真实 attachment 层数核对 refs，vector backing 使用 AV_NUM_DATA_POINTERS + maximumExtendedBufferReferences 而非非空项数 |
| Runtime frame/audio/aggregate | 创建点明确使用 runtime resource | runtime 域费用留在最终 graph ledger，不挪入 source preparation reservation；共享 aggregate container 只计一次，源帧别名仍归源 allocation |
| FFmpeg 自有对象及平台库 | AVFrame/AVPacket/AVBufferRef 公共头与引擎 opaque owner 是不同对象 | 公共头 logical bytes、库私有 AVBuffer/side data 与分配器开销分开；现 ownership 产品明确不预约 FFmpeg private storage，不能将其直接冠名物理内存完整 total |

几何必须由对象所属模块导出，复用 `MediaSharedAllocationGeometryProbe`、容器几何与实际 allocationFootprint；runtime new_delete_resource 域不能套 preparation header 公式。现 shared geometry 仅权威支持 MSVC STL145/202604，其他库返回 Unsupported；Linux/RKMPP 需要独立核实实际 STL 实现的同一分配请求，不能复制 Windows 数值。源码字节请求可计量不等于操作系统 working set 或驱动显存分配总量。

#### 可闭合文件范围与生产接线顺序

下列是同一功能块的依赖分组，不应把第一组单独宣布完成；实现中若实际转换根超出此范围，先补本文的具体证据与范围。

- 身份贡献：`planner/realtime/MediaGraphPayloadResidencePlanner.{h,cpp}`、`MediaCompositionPayloadResidencePlanner.{h,cpp}`；新增同目录 `MediaCompositionAllocationResidencePlanner.{h,cpp}` 及其拥有型产品。复用既有 traversal，不另写按名称推测的图遍历。
- 对象几何与真实创建契约：`runtime/buffer/MediaVideoDecodeBuffer.{h,cpp}`、`runtime/session/MediaRtpPacketClockSession.{h,cpp}`、`sync/lineage/MediaCodecLineageRegistry.{h,cpp}`；现 `FFmpegBufferFactory`、`MediaFfmpegPayloadOwnership` 和 `MediaVideoFrameOwnershipStoragePlanner` 仅补确实缺失的组件身份/几何。Decode/Filter/HardwareTransfer/VideoPreparation 的真实调用点用于核对和消费内部转换契约，不重写媒体算法。
- 候选与费用：`MediaPreparedSourceCandidateCostPlanner.{h,cpp}` 改为接上述按域分项产品；`MediaCompositionSourcePlanningKernel`、`MediaRealtimeCompositionSourceResourcesPlanner`、`MediaSourceFrameOwnershipStoragePlanner` 共用同一事实，移除调用方自行填 packetWrappers/hardwareFrameWrappers 等无证明裸数量的入口。`MediaPreparedRtpStorageAdmission.h` 与真实 batch solver adapter 承载费用/readback 产品；只复用既定有限候选与共享预算事务，不发明新的搜索算法。
- 实际入口及最终消费：`src/application/realtime/MediaRealtimeCompositionPreparer.cpp` 在 freeze 前用真实已打开 source/output、独立 audio retention 和 candidate capacities 调共同 kernel，再形成 aggregate/topology/residence/费用；`MediaRealtimeCompositionResourcePlanner.{h,cpp}`、builder 的 `MediaRealtimeCompositionSourceResources.cpp` 与 `MediaRealtimeCompositionResourceBinder.cpp` 从最终图、实际 session/resource 和 producer contract 独立重建并核验同一贡献表；新增编译单元加入 CMakeLists.txt。

接线严格为：真实 candidate facts → 共同 source kernel → aggregate 与完整 topology → holder identity contributions → allocation family/domain 上界 → CandidateCostPlanner 分域费用 → 同一 root 下已持有/已预约/新增费用互斥合账 → freezeBatch 的 solver 事务 → sealed bind → 最终 graph/readback。freeze 持有 session 锁期间只能读取传入 snapshot 和预先拥有的事实，不能回调加锁 session accessor；候选结构自身临时存储也须在既有 preparation 预算内计入。现输出/canvas 已在 preparer:289/293 reserve，不能再次相加；held snapshot 是下界及已有 backing，不是未来峰值，须复用原 history/capacity 高水规则。

接线完成的可观测证据是：真实候选调用费用 planner；费用各项有 owner/family/domain/count/bytes 与来源；最终 readback 比较全部项及 reservation owner，缺项/多项/域迁移/总额溢出均失败。日志仅展示同一产品，不用运行采样最大值反填上界。一个不被真实 CandidateCostPlanner 和最终 binder 消费的 geometry/export 不作为本块完成项。

#### 仍不能从当前事实推出的量与放行门禁

- 通用 FFmpeg API 没有给出 codec 内部所有 AVBufferRef 复制头、私有 AVBuffer、side data 或驱动分配数量/大小；registry capacity 只约束引擎 entry/lease/opaque identity，不约束库内同 owner 的引用头数。必须采用实际编解码器/库版本的权威 allocation contract，或保留该域未证明并禁止声称全物理内存准入；不能用输入 AU 数或初帧采样补齐。
- 当前 residence 没有输出逐 owner 贡献，也没有覆盖跨 payload 的 lineage 与节点构造临时根。所以上述 allocation 上界目前尚不存在；必须完成逐创建点契约并与最终图核验，不能将现 maximumRetainedObjects 改名后直接乘 geometry。
- 新旧 registry State/allocator owner 如可在旧 lease 尚存时替换，历史 State 控制块数量需真实 purge/drain/reconfiguration 契约；单次 capture 或代次日志不能证明硬上界。每个实例共享 capacity 可覆盖其旧新 entry，但不自动覆盖多个 State。
- 当前 CUDA decoder 的 maximumExtendedBufferReferences=0 已由 MediaDecoderOutputAllocationAdapter 的 NativeCudaSafeCopy/CuvidCopy 契约提供，共同 kernel 已比较 decoder/filter readiness，不能再把它笼统列为缺失事实。本块仍须逐转换核对 engine attachment 层数是否被该契约覆盖；其他 backend 如只有首帧 readback 则拒绝候选，已有 CUDA capability 不可外推 RKMPP，后者目前不在 SourcePresentation source adapter 支持范围。
- 目前已有费用中 preparation 实际预约字节与 FFmpeg public header logical bytes 混合，合账前须按真实 charge authority 拆项；未拆前不得生成“已预约完整 total”。这不要求新公共参数或外部依赖，而要求内部产品明确所证明的预算域。

因此可立即实施的是引擎拥有对象的证明与真实生产消费闭环；完整 SourcePresentation 准入必须同时闭合上述适用候选的缺失事实及既定联合 solver。任何一个待证明域不能默认为零或仅写入日志后放行。完成后仍按冻结双独立审查、Windows 全量构建及原规格实流、再 RKMPP 的顺序验证；本节不修改既有平台能力，不以 r381 或 MMCSS 成功替代费用证明。
#### 冻结快照预算域复核（2026-11-04）

已沿实际创建链核对：CompositionPreparer 创建 root 并传入 MediaSharedPreparationStorage；MediaRtpSourcePreflight:214 为每源创建 child budget，再传给 SourceSession。MediaStorageBudget::snapshot 返回自身 m_storage 的 retainedBytes，而不是 parent 总量。因此 validateStorageAdmissionLocked 将每源 totalBytes 与该源 snapshot.budget.retainedBytes 比较，不会在此处重复计算整个会话已用量，无需为这一疑点修改代码。

freezeBatch:708–743 的临时 ordered/locks/snapshots 使用首源 ownerResource，快照在这些分配之后取得；solver 的返回存储随后仍可能新增占用。后续完整合账必须明确这些临时存储的同时存活边界，并对根域已有输出/canvas 预约去重。当前仅检查各源 totalBytes 之和不超过部署 capacity，不能据此证明包含根域其他持有者的完整准入；现生产入口仍传 nullptr，此处是接通 solver 前必须闭合的契约，不是当前 cadence 实流失败的新归因。
#### Registry 实例上界复核结果（2026-11-04）

独立调用链核验已消除固定合屏“每次重入产生历史 State”的疑点：实际 Decode/Filter/Encode 实例各创建一次 registry，configure/reset/purge 不替换 State，旧新代 entry 共用同一 capacity；lease 仅弱引用 State，延长的是控制块存储，不保活 map/entry。Probe 重试及成功交接有释放旧 decoder/probe/readiness 的顺序，不维护历史 probe 列表。具体调用点见 realtime-composition-allocation-domain-audit.md 的本轮复核。

据此修订上文待证明项：固定合屏费用按实际节点/probe 实例计 State 与其控制块，不能按代数或 token 数重复 State；弱引用尾部仍须覆盖最后 lease 的生命周期。动态退役实例及其剩余 holder 必须独立核算，不能把固定图结论外推为任意动态重配置上界。此结论消除了一个实现前事实缺口，未完成分配几何、逐 owner 贡献、完整费用或联合 solver。
#### 持有者贡献生产接线（2026-11-04，验证中）

本轮修改四文件：MediaGraphPayloadResidencePlanner.{h,cpp}、MediaCompositionPayloadResidencePlanner.cpp、MediaRealtimeCompositionResourcePlanner.cpp。原有可达遍历和引用总数保留；返回拥有型逐 owner 贡献，区分普通 node/edge、typed startup node/edge、共享 aggregate，并保留完整拓扑连接索引。真实图路径独立转为 NodeId/EdgeId；生产 ResourcePlanner 在既有成员验证之后逐项比较 kind、节点集合、边身份和引用数量，不再仅比较总数。零引用费用条目不作为媒体持有贡献，aggregate 多保留位置仍只贡献一次。

沿用本计划已调查的 GStreamer allocation negotiation/有限持有者原则；本轮重新核对官方 bufferpool 文档。引用上界仍不等于独立对象数，尚未接跨 payload allocation family、几何费用或 batch solver。线程、媒体状态机和队列容量未改变。这是完整准入的中间实现，不能记作全部费用完成。

验证：静态差异空白检查通过；source_kernel_review_a 与 stage_summary_refresh 均已明确局部源码 PASS。Release clean-first/all-target 构建会话90692已终态退出0，789目标，configure/build exit codes均0；CMakeCache、build.ninja及两CLI产物已核验。尚未运行本轮新实流，不能沿用r381为改后代码验收。验证遵循用户要求的真实 realtime 链路，不新增测试体系。

持有者贡献四文件改动已完成r382原规格realtime行为复验：三格→全黑→所选源重入→全黑，四次源均120秒/3600帧退出0，CLI caller-stop退出0，218146预约释放归零，errors/pressure/deadline_misses均0。完整记录见realtime-composition-r382-validation.md；16临时文件清理，六PID与端口无残留。sender delivery仍not_proven，最差提交迟到29.440240ms；不构成完整费用/联合准入、线速交付、内容级A/V或整体验收通过。
#### 四类 lineage/wrapper 的实施事实闭合（2026-11-04）

本轮逐创建点核查并由独立审查者复核，详见 allocation-domain-audit：AU lineage、display lineage、packet decode wrapper、frame decode wrapper 的创建和强引用关系已列明。准备 session mutex 串行化 timedPacket，冻结后唯一 move/reset 交接到单 process owner，故同一 stream 同时最多一个 timedPacket 构造。SourcePresentation 直接返回 timed，不持 cadence lookahead；不能复制旧路径的该槽。

decoder resolve AU、filter resolve display 后都先 unref opaque 再创建输出 wrapper，需保留这一局部强引用根，不能只依赖 registry capacity。registry capacity 已由共同 runtime.queues.frame 提供并写入实际 decoder/filter options，不是尚缺外部事实。pendingTransfer 必先排空且本次 process 随即返回，不能与同 node 下一输出构造并发；但可能与 pending input/lineage 共存，费用不得隐式抵扣。pendingInputAllocations 只保 AVFrame 的底层 buffer/hw context，不作为 display/wrapper 引用根再次收费。

下一代码块据此采用明确 family、creator、allocation domain 及 root role 的拥有型产品：wrapper 根按各创建阶段的实际贡献和构造根；display 根从frame wrapper、filter registry/pending/resolve传播；AU根从packet wrapper、decoder registry/pending/resolve及display传播。同一frame直接AU与display.accessUnit为同一对象，只沿该关系计一次；不同根可能共享时保守相加，不声称独立对象。geometry必须使用创建点实际allocator类型。CandidateCostPlanner与最终ResourcePlanner消费同一分项产品并独立核验root/域/容量，不能仅新增无人消费的geometry。上述事实闭合不是费用代码已完成；本轮未修改源码或新增实流。
#### 四类 lineage/wrapper 费用实现（2026-11-04，审查/构建中）

本轮10文件实现：MediaVideoDecodeBuffer 导出实际 const AU、display、decode wrapper 的 PMR shared geometry；新增 MediaSourceLineageStoragePlanner 以完整拓扑 source 身份、typed owner contributions、实际 registry capacity 和已核验串行构造/局部根，返回四类分项、根证据与准备域字节。frame wrapper 到 display、display 到 AU 按真实引用关系传播；同一 frame 直接 AU 与 display.accessUnit 不重复，跨不同持有根只给保守上界，不宣称 distinct 实测数量。现候选不接纳 HardwareTransfer，遇到该阶段明确拒绝，不虚构其费用。

CandidateCostPlanner 已接共同分项产品；最终 SourceResources 在 decoded timing 路径核对 decoder/filter 的 registry options 与共同 runtime.queues.frame；ResourcePlanner 调同一 planner，将 lineage 与现 frame ownership 作为同一准备 reservation 的互斥组件核验并保留证据，不重复加入总账。原始packet/frame wrapper、registry本体、FFmpeg私有分配及其他域不包含在这四类费用中。

边界：CandidateCostPlanner 尚无 application 生产调用；当前 cadence 实流不会进入 decoded timing 的四类费用分支，不能把 r382 当作此块运行证据。完整早期候选/最终 sealed逐项一致性、其他分配组件和联合solver仍未接通。本轮属于完整准入的依赖实现，既有门禁保持，不生成完整total，不切SourcePresentation。源码冻结范围为10文件，正在双独立审查；Release全量构建会话51103进行中，尚无成功结论。
四类费用首次全量构建51103已成功（790目标、configure/build退出0），但随后追加复审发现 prepared prefix 漏根，原10文件PASS撤回，不能作为完整根证明。运行binder接收整个s.prefix并逐项弹出，其容量与acquiring/decoded startup frames没有已证明的大小关系。

修正后范围13文件：在原10文件外加入SourcePlan.h、SourcePlanner.cpp、ResourceBinder.cpp。新增PreparedPrefix根，候选取video.containers.prefixObjects，最终从sealed.video.bounds.prefixObjects与compressed.maximumUnits一致性验证后透传；实际ResourceBinder从rtpSources的sealedAdmission及videoReferenceResource独立核对数值、timing存在性与shared ownership。packet wrapper和AU费用均包含该独立根，其他域不受影响。

source_kernel_review_a与stage_summary_refresh均已对全部13文件明确复审PASS，原FAIL与修正证据保留。本版第二次clean-first/all-target Release构建会话60765进行中，旧51103不覆盖修正。当前不新增cadence实流来冒充SourcePresentation费用分支验证；后续在完整入口接通后取得该路径真实证据。

修后全量构建60765已终态退出0，790目标、configure/build均0，CMakeCache/build.ninja/两CLI产物已核验。13文件局部双审与构建通过，不代表新费用分支运行通过；未新增本轮媒体测试或临时测试产物。
#### 准备 payload lease control 根推导（2026-11-04）

两文件 CandidateCostPlanner.{h,cpp} 已移除 liveAccessUnitLeases 裸数量输入，费用结果保留六项typed roots：prefix、acquiring、outputs、协议pending批次、RawRtpInput credit等待槽、binder credit等待槽。容器数量来自同一候选，pending来自真实maximumAccessUnitsPerPush，等待槽对应各实际单成员；checked sum后计算真实PMR shared control几何字节。containerBytes仍扣pendingLeaseControlBytes，由全存活控制对象费用替代。

证明边界：构造第j个pending control时已有j-1，总数不超M；栈lease不是额外shared control。冻结前session mutex串行acquiring→prefix搬移，其移出槽覆盖在途control。snapshot只共享同一单调prefix，冻结后禁止新snapshot，不新增wrapper/control身份。成功graph credit attachment释放准备lease，但raw wrapper可继续存活，因此不能共用下游wrapper引用总数。probe registry预算来自root，不能当session child费用。

source_kernel_review_a与stage_summary_refresh均明确两文件源码PASS。当前仅适用于已有非canonical候选费用路径；packet/frame wrapper裸数量、probe/其他域费用、完整早期入口与联合solver仍未完成，不新增总额放行。Release全量重建已启动，未新增媒体测试，因为当前cadence不覆盖该候选分支。

lease-control修正后的Release全量构建97027已终态退出0，790目标，configure/build均0，CMakeCache/build.ninja及两CLI产物已核验。本轮无新实流或临时测试产物；双源码PASS及构建不证明候选入口已实际启用。
#### 原始 packet wrapper 根与最终容器核对（2026-11-04）

本轮12文件已删除CandidateCostFacts.packetWrappers裸输入。新MediaPreparedPacketWrapperStoragePlanner按source/stream精确定位RawInput、ClockBinder两创建者；各自使用已有typed graph contributions，加实际构造/credit等待槽、准备prefix/acquiring/output及权威atomic completion根。保留逐根身份与数量，checked sum乘实际FFmpegPacketBuffer PMR几何。图引用可别名，结果是保守上界；新旧raw wrapper重叠不误计为新增payload或lease control，外层decode wrapper仍是独立真实分配。

候选分别调用video/audio共同planner。原preparedVideoPrefixCapacity升级为已封存的video/audio三容器bounds；SourceResources传递与实际input envelope核过的holder facts。实际ResourceBinder独立核对sealed完整bounds、compressed maximum、成员及source resource共享所有权；最终ResourcePlanner将两流raw wrapper作为独立组件与lineage/frame ownership合计检查同一reservation，保存费用根，不再次加入fixedStorage。

两名独立审查者source_kernel_review_a、stage_summary_refresh均明确12文件源码PASS。Release clean-first/all-target构建91821进行中；frame wrapper裸数量、probe/其他组件、早期候选生产调用、联合solver和完整sealed费用一致性仍未完成。当前canonical链路不进入新增费用分支，不新增同样cadence测试来冒称该分支运行通过。

原始packet wrapper块全量构建91821已终态退出0，791目标，configure/build均0；四项产物已核验。新增cpp由现有CONFIGURE_DEPENDS源集合纳入。无本轮媒体测试或临时测试产物，完整SourcePresentation验收仍待入口接通。

#### SourcePresentation passthrough transfer 费用契约修正（2026-11-04）

根因及失败证据见 allocation-domain-audit 的本轮复核：真实源帧拓扑包含 transfer 节点，None 方向原样转发，而旧 lineage planner 按节点 kind 一律拒绝。行业对照采用 GStreamer GstBaseTransform 官方 passthrough 语义（https://gstreamer.freedesktop.org/documentation/base/gstbasetransform.html）：原样转发不意味着新的 buffer 分配；本项目 HardwareTransferNode 的 None 分支同样发布原 buffer。这里仅复用该所有权区分，不引入 GStreamer 或改变线程、队列、背压、媒体处理状态机及平台 adapter。

七文件修正将既有内部 MediaHardwareTransferDirection 从候选事实传入共同 lineage 费用 planner，只接受 None，要求该源唯一 HardwareTransfer 与 VideoFrames.Transfer 角色对应。真实 SourceResources 独立核对源计划为 None、实际 sourceMembers 恰有一个 transfer 且 options.transfer.direction=none，再把该类型化事实交给最终 ResourcePlanner。转发 holder 仍由 residence 计入，不新增 creator；非 None、缺失、重复或身份错配继续报错。没有新增公共输入参数，也不扩大外部依赖范围。

本轮源码已冻结七文件，正在双独立复审及 Release clean-first/all-target 构建（会话51255）；尚无构建成功或实流通过结论。原误拒绝结论保留，原局部 PASS 不覆盖该遗漏。完整候选入口、frame wrapper 与其他费用、联合 solver 仍待完成；不使用旧 cadence 测试替代新分支验证。
本轮结果：frame_wrapper_domain_review 与 transfer_contract_review_b 均对七文件明确 PASS。全量构建51255终态退出0，791目标、configure/build均0；四项产物已核验，realtime CLI为7045632字节、2026-11-04 12:39:42。未运行新媒体测试；没有本轮测试临时产物。本修正解除已确认的拓扑误拒绝，不代表完整SourcePresentation入口或费用准入完成。

#### 原始 frame wrapper 创建根与实际分配域（2026-11-04）

本轮沿既有有限持有者费用设计推进，重新核对 GStreamer 官方 allocation negotiation / bufferpool 文档（https://gstreamer.freedesktop.org/documentation/additional/design/bufferpool.html）：分配器与buffer数量须由已协商事实确定；不能把引用别名当作独立分配。本项目复用已验证的typed holder贡献和串行创建根，保守累计，不宣称这是GStreamer算法的逐行实现或FFmpeg物理显存上界。

九文件实现移除hardwareFrameWrappers/softwareFrameWrappers裸输入，候选改借已有VideoSourcePlan；新MediaPreparedFrameWrapperStoragePlanner复用共同lineage产品的FrameWrapper根，核对source/creator身份、decoder/filter成员及串行construction。当前SourcePresentation已准入CUDA readiness先capture后wrapFrame，capture要求实际hw_frames_ctx有效，因此wrapper为HardwareFrameBuffer。对不在此契约中的FrameRate/Fanout源clone创建点拒绝，不把cadence公式混用。

SourceResources解码实际decoder/filter readiness选项，逐项与上游kind/context/stream/extended reference事实比较；候选和最终ResourcePlanner消费同一raw wrapper产品。最终账本保存独立根证据，与packet/lineage/readiness ownership组件共同核对已有preparation reservation，同owner累加，不再加入fixedStorage。wrapper geometry来自实际MediaLineageAllocator；不重复计attachment/operation/completion，不把probe裸AVFrame或音频Runtime wrapper算成prepare对象。本块不改变线程、背压、队列容量、失败传播或平台adapter。

九文件已冻结并请求两名独立审查，Release全量构建29793运行中。完整registry/probe等费用、早期application调用及联合solver仍未完成；当前cadence不覆盖新路径，本轮不以重复cadence实流冒称新费用验证。
本轮九文件结果：frame_wrapper_domain_review与transfer_contract_review_b均明确源码PASS；Release全量构建29793终态退出0，792目标、configure/build均0。四项构建产物核验通过，realtime CLI为7052288字节、2026-11-04 12:48:28。无新增媒体测试或临时产物，完整候选入口仍未启用；源码及构建通过不能替代新路径实流。

#### Registry slot 独占责任与释放顺序实现（2026-11-04）

已按 allocation-domain-audit 的设计修改三个文件：MediaFfmpegLineageToken.h、MediaCodecLineageRegistry.{h,cpp}。token内持weak State、分配scope及独立slot identifier，不再创建shared LeaseControl；自定义noexcept move/assignment，转移后清空源slot，观察token无拥有权。opaque callback先将token移至局部，再销毁并deallocate engine opaque，最后token归entry。map释放仍在同一mutex内，slot覆盖完整engine opaque分配生命周期。

行业依据仍是FFmpeg AVBuffer最后引用回调及线程安全引用语义（https://ffmpeg.org/doxygen/trunk/group__lavu__buffer.html），无需新增回调线程数量事实。FFmpeg自身回调后才释放的AVBuffer/Ref不包括在此上界；runtime与probe preparation域未改变。无公共参数、外部依赖或特殊平台链路变化。

首次审查发现submit return多余右括号，已在构建前修正，保留FAIL。frame_wrapper_domain_review和transfer_contract_review_b随后均对全部三文件明确复审PASS，覆盖四个真实consumer、分配失败、观察token、move/self-move、State提前析构及锁内返回。Release全量构建5398运行中；几何产品与账本消费尚未完成。本块影响已启用共享媒体链路，需要Windows原规格realtime复验，不能用仅新候选未接通为由豁免。
生命周期三文件改后全量构建5398终态退出0，792目标、configure/build均0，四项产物核验。r383已完成原规格三格→全黑→所选源重入→全黑及caller-stop退出0；四次源均120秒/3600帧退出0，222990预约释放归零，errors/pressure/deadline_misses均0。sender最差提交lateness5.267840ms、delivery仍not_proven，不等于完整发送/准入验收。实际命令与证据见realtime-composition-r383-validation.md；16项产物已逐项清理、六PID和端口无残留。registry几何及runtime/probe费用消费者仍待实现。

#### GCC 12.4 registry 所需几何适配（2026-11-04）

目标启用mtenv后的预处理事实为 _GLIBCXX_RELEASE=12、__GLIBCXX__=20240620、_GLIBCXX_USE_CXX11_ABI=1、_GLIBCXX_HAS_GTHREADS=1。实际shared_ptr_base.h:960–980的__shared_count使用_Sp_counted_ptr_inplace<T,Allocator,LockPolicy>和__allocate_guarded；shared_ptr.h:1000–1015的make_shared使用allocator<void>；stl_tree.h的_M_get_node每次allocate(1)，_Rb_tree_impl内嵌_Rb_tree_header。独立审查同时对照GCC12.4官方源：https://github.com/gcc-mirror/gcc/tree/releases/gcc-12.4.0/libstdc%2B%2B-v3/include/bits 。

本轮修改两个已有helper：typed ActualAllocator shared geometry保留真实Object cv、allocator及默认lock policy，返回对应控制块sizeof/alignment；tree按每节点真实分配计费，GCC不另加MSVC sentinel/proxy。分支限定该libstdc++ release/date、C++20、非debug；Windows原分支公式不变。未放开vector/deque/proxyBytes及无参reservation probe，不据此声称完整Linux支持。新增路径复用现有planner调用，不建立独立媒体链路。

frame_wrapper_domain_review和transfer_contract_review_b均对两文件明确源码PASS，Windows clean-first/all-target Release构建62430运行中。目标机只读头文件和预处理宏不等于目标编译/媒体验收；runtime/probe费用接线仍未完成。第一次远程读命令因本地PowerShell引用错误未执行，修正后下列读取退出0，无远程文件或测试产物。

```text
bash -lc 'source /opt/mt-tools/mtenv.sh && mtenv on && g++ -std=c++20 -dM -E -x c++ -include memory /dev/null | grep -e __GLIBCXX__ -e _GLIBCXX_RELEASE -e _GLIBCXX_HAS_GTHREADS -e _GLIBCXX_USE_CXX11_ABI; sed -n 960,980p /opt/mt-tools/gcc-12.4-native/include/c++/12.4.0/bits/shared_ptr_base.h; sed -n 1000,1015p /opt/mt-tools/gcc-12.4-native/include/c++/12.4.0/bits/shared_ptr.h; sed -n 550,570p /opt/mt-tools/gcc-12.4-native/include/c++/12.4.0/bits/stl_tree.h; sed -n 650,670p /opt/mt-tools/gcc-12.4-native/include/c++/12.4.0/bits/stl_tree.h'
```

两文件adapter改后Windows全量构建62430已终态退出0，792目标、configure/build均0，四项产物已核验，realtime CLI7050240字节、2026-11-04 13:15:13。双独立源码PASS及Windows构建不等于GCC分支编译或RKMPP实流通过；本轮无新增媒体测试/临时产物，最新实流仍为r383。registry几何导出及runtime/probe消费者继续推进，其他STL容器和完整准入未完成。

#### Runtime registry 费用进入共同最终账本（2026-11-04）

本轮四文件沿前述实际allocator/STL证据实现：新增make_shared几何区分MSVC _Ref_count_obj2与GCC allocator<void> inplace路径；MediaCodecLineageRegistry::runtimeStorage返回outer registry、State控制块、实际map存储和capacity个engine opaque分项及checked总额。使用Runtime实际分配请求字节，不套preparation block footprint，不包括引用lineage/FFmpeg自有headers；独立lease分配已在前一轮删除。

共同compileLedger按selected实际VideoDecode/VideoFilter/VideoEncode节点调用与runtime factory共用的lineage identity/capacity验证；没有registry选项时与factory一致跳过，缺失/不匹配事实报错。有registry时计入固定runtime费用，用真实node id记录归属。selected分区复用同一算法，不建立合屏特殊计费链。probe内嵌registry不经过这条runtime factory，后续以Preparation root独立消费，不混入本轮费用。

原理沿用FFmpeg最后引用回调与有限slot完整覆盖opaque分配周期，并使用已核验MSVC/GCC allocator adapter。当前动态branch物理销毁与输出不携带encoder opaque的闭合证据沿用allocation-domain-audit，不外推未知escaping opaque。四文件双独立审查中，Release全量构建71293运行中；本轮尚未有构建/实流通过结论，probe根费用和完整联合准入仍未完成。
首次双审均FAIL：新增registry费用项使用ordinary referenceNode，与原普通node引用项重复，违反MediaCompositionResidenceReadbackValidator的唯一性。修正应使用RetainingGroup与单节点retentionNodes，maxBufferObjects保持0；双方已核对零引用项不会成为aggregate媒体owner或增加residence额度。构建71293仍运行，待终态后修改再全量重建，不能把本次构建当作修正后的验证。尚未启动本轮实流。

旧版构建71293已成功（792目标），随后实际修正registry费用身份为RetainingGroup+retentionNodes={node.id}，零媒体引用。frame_wrapper_domain_review与transfer_contract_review_b均对四文件重新明确PASS；修后全量构建63644运行中。此前FAIL保留，旧版构建不覆盖此修正。

修正后全量构建63644已核验终态退出0，792目标、configure/build均0；四项构建产物齐全，realtime CLI7052800字节、2026-11-04 13:25:26。r384原规格realtime复验已启动，当前结果尚未完整。

r384已完成修正后原规格三格→全黑→所选源重入→全黑及caller-stop退出0；三初始源及重入均120秒/3600帧退出0，220555预约释放归零、errors/pressure/deadline_misses均0。sender最差lateness26.130860ms，delivery仍not_proven。实际命令和证据见realtime-composition-r384-validation.md；16项临时产物已逐项清理、六PID及端口无残留。行为回归不等于完整准入、wire或RKMPP验收。下一块probe根预算已核验与freeze阶段重叠，见allocation-domain-audit同日末节。

#### Probe 准备费用产品接入创建入口（2026-11-04）

在r384完成后，继续实现已审查的依赖产品，共六文件：registry runtime/preparation共用按分配域计费的State、tree、opaque几何；preparation不含内嵌registry外壳。新增IdentityProbeStoragePlanner按实际BootstrapAllocator及LineageAllocator生成resource bootstrap、probe shared control、registry分项和已有public-header预约，逐次分配计入Preparation block头部而非对总额只加一次。真实create以产品publicHeaderBytes预约header，以registry.capacity创建registry及限制提交，并保存产品和同一root budget身份。

FFmpeg最后引用回调与既有slot释放顺序证明沿用allocation-domain-audit；MSVC145/202604和目标GCC12.4/20240620的实际类型适配沿用已核验vendor路径。本轮重新读取FFmpeg官方doxygen超时，不把该失败当作新依据。已核验代码事实是旧入口只计算headers、没有可供联合solver读取的probe完整分项；不能用旧cadence运行冒充该缺口的运行复现。

边界：这是可追溯费用依赖产品，不是完整峰值预约。其余组件仍逐次检查预算；未创建子预算、未新增公共参数，联合solver仍未读取此产品。共享source identity/lineage、既有decoder与FFmpeg私有对象不重复计入。后续仍必须解决完整root保证与子预算metadata尾部，再将所有活跃probe和output/canvas并入freeze峰值。按仓库真实链路优先规则，不新增持久测试体系或用伪造单元测试声称准入完成。

六文件已冻结，frame_wrapper_domain_review和transfer_contract_review_b独立审查中；Release clean-first/all-target构建65588已启动，尚无终态成功结论。

分项产品六文件经两名独立审查者明确PASS后，全量构建65588终态成功（793目标、configure/build退出0），四项产物核验，realtime CLI7058432字节、13:43:12。随后继续实施子预算，因此此构建不代表后续九文件版本已验证。

#### Probe 子预算与预算对象元数据（2026-11-04）

按frame_wrapper_domain_review设计审查，复用已有BootstrapAllocator/AllocationBlock：fromReservation改用传入reservation的原owner为allocator预算，allocate_shared预算对象，metadata预约保持到最后weak控制块native free后归还；payload backing仍只覆盖子容量。私有ConstructionKey约束构造，BootstrapFailure原错误保留。Admission枚举拆为独立头，解除BootstrapAllocator与StorageBudget头循环，枚举值不变，原create路径不改。

probe产品区分capacityBytes（resource bootstrap/probe/registry/header）、budgetBytes（子预算控制块bootstrap）及root totalBytes。create先向root预留capacityBytes，factory实际向同root计metadata，然后headers/resource只从child消费，保存root和child身份。没有把metadata重复塞入child容量。输出/canvas现有fromReservation两调用也真实计入各自metadata，余量不足时原样失败，不降低容量约束。共享source lineage及FFmpeg私有对象不因此变成已证明范围。

payload与metadata是两步预约，中途失败完整回滚；不宣称跨源原子联合准入。联合solver仍未接，冻结时所有活跃probe与输出/画布保证需要按唯一预算归属合账。九文件已冻结并送双独立复审，全量构建26902运行中，尚无修后构建/实流通过结论。

追加子预算后的九文件已由frame_wrapper_domain_review与transfer_contract_review_b分别明确复审PASS。修后全量构建26902终态退出0（793目标、configure/build均0），四项产物核验，realtime CLI7063040字节、2026-11-04 13:48:04。r385共享输出/canvas链路复验已启动，不能据此宣称新SourcePresentation probe分支已实流通过。

r385实流FAIL：约74秒audio seq2956在scheduler等待缺失video123.7101ms，已晚于dispatch4.727346ms才提交，mux late5.103046ms，TS原deadline准入拒绝，CLI退出1；三个源各120秒/3600帧退出0。仅三格快照，未覆盖失源/重入，65163预约释放归零不改变失败结论，最终errors/workerErrors/pressureFailures各1。完整证据见realtime-composition-r385-validation.md，11项产物已清理，五PID和端口无残留。当前无预算变更因果证据，继续追查scheduler缺失流等待及视频晚到；不能把局部源码PASS/构建成功记作本轮实流通过。联合solver下一缺口是同一root已占用及非源费用的类型化所有权产品，需避免root retained与源完整total重复累计。
