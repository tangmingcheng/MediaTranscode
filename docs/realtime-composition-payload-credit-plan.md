# 合屏压缩负载联合信用计划

状态：待双独立设计审查，尚未实施、构建或验收。依据 [r402 实流诊断](realtime-composition-r402-validation.md)；本计划补充 [全存储准入计划](realtime-composition-full-storage-admission-plan.md)，不替代其完整门禁。

## 问题与边界

原四路、2 GiB 部署预算下，最小合法候选 k=0 的源费用为 34,421,201,947 字节，可用为 1,662,018,130 字节。其中 AAC payload 为 14,279,124,369 字节，各源视频 payload 为 2.77–4.31 GB；这是候选最坏费用，不是实测同时驻留内存。CLI 自然退出 1，未通过合屏验收。

`MediaPreparedSourceCandidateCostPlanner::streamCosts` 将 prefix、acquiring、outputs、protocol pending 和信用等待持有者的数量累计，再逐个乘最大 AU 分配费用。`MediaPreparedRtpSourceStoragePlanner::completionOutputObjects` 又将 reorder 窗口与每次最大 AU 数相乘。该模型没有表达同一报文内的数量/字节相关性，也没有采用可执行的跨批字节信用契约。

`MediaPreparedRtpSourceSession::consume/drain/takeOutput` 允许多次回调积压输出；锁仅互斥，不能证明只保留一批。现有 outputBytes 和共享预算限制实际字节，但未保护未来固定元数据和启动所需信用。因此不能用单批 datagram 总字节替换全部 retained outputs，也不能直接以 `min(原费用, 剩余预算)` 降费。handoff 出队不等于最后一个 payload lease 释放。

范围：共享 planner、协议产品和 runtime 信用执行；不新增对外字段，不改外部 FFmpeg，不提高预算，不降低源参数、时长、编解码步骤或验收要求。分阶段只划分实现工作，不缩减最终能力。

## 成熟依据与适用差异

- [RFC 3640 §3.2.1](https://www.rfc-editor.org/rfc/rfc3640.html#section-3.2.1) 定义两字节 AU-headers-length；当前每头 16 bit 时头数上限为 4095。[§3.2.3.1](https://www.rfc-editor.org/rfc/rfc3640.html#section-3.2.3.1) 区分完整 AU 聚合与单 AU 分片，分片声明的是整个 AU 大小。完整聚合的 AU 字节和受当前 payload 限制；分片须保留整个 AU 的信用，不能混为一个字节界。
- [FFmpeg MPEG4 RTP depay](https://github.com/FFmpeg/FFmpeg/blob/master/libavformat/rtpdec_mpeg4.c) 与 [GStreamer MPEG4 generic depay](https://github.com/GStreamer/gstreamer/blob/main/subprojects/gst-plugins-good/gst/rtp/gstrtpmp4gdepay.c) 都根据实际头、剩余 payload 和分片状态处理。仓库 `MediaAacRtpAuHeaderPlan`、`MediaAacRtpDepacketizer` 已有对应解析，必须复用；修正头数上限本身不足以解决视频及跨批积压费用。
- [GStreamer queue](https://gstreamer.freedesktop.org/documentation/coreelements/queue.html) 独立限制对象、字节和时间；[buffer pool](https://gstreamer.freedesktop.org/documentation/plugin-development/advanced/allocation.html) 在配置阶段确定容量，最后引用释放后归还缓冲。复用独立容量维度、生命周期归还和上游协商机制，不复制其默认常量。
- [Apache Flink 网络栈](https://flink.apache.org/2019/06/05/a-deep-dive-into-flinks-network-stack/) 以每 channel exclusive buffers 保护进展、floating buffers 共享弹性，释放后返回 credit。本计划映射专属保证和释放归还机制，不照搬默认数量或线程模型；首批只实现固定保证，不实现 floating 池，后续 planner 再证明是否需要。UDP 不能向发送者发送等价的接收信用。
- 本项目 UDP 输入不能阻塞远端发送者，session 锁内也不能同步等待。因而采用有界保留、异步释放通知及原有取消/期限失败，而非直接套用线程阻塞；有限预算不能承诺任意长下游停顿下无损接收。

## 实施前必须双审的契约

1. **协议完成批次产品**：分别携带最大完成对象数、完整聚合总 payload 界、分片续存/新分配界、控制事件数及每 AU wrapper/控制块费用；以共享解析结果生成执行所需实际申请清单。AAC 聚合与分片合法性必须在申请前验证；H.264/HEVC 按现有分片、重排和完成状态机提供权威界，不能另建样例专用链路。
2. **联合信用产品**：对象槽与 payload 字节独立，明确池身份、源归属、进展所需保留额度、固定/启动保留和最大不可拆事务。共享池只计一次，独立 wrapper/数组仍分别计费。池如何按源保护最小进展，以及是否需要独立 output 子池，必须先用同时持有关系证明，不能由剩余字节随意平分。
3. **原子事务与所有权**：一次推进先同时取得对象槽和整批字节信用，再提交协议状态；失败全部回滚。拆分 lease 后每字节始终只有一个计费 owner，别名不重复 payload 计费，独立控制块照常计费。若底层分配失败，保留原始错误，不把分配失败伪装成等待。
4. **执行状态机**：解析待提交 → 等待信用或已预约 → 提交 → 持有/转交 → 最后引用释放；取消或失败可从未提交状态回滚。逐项列出输入报文、协议 carry、pending、output、binder、raw node 及重入旧/新 generation 的所有权和同时存活集合。等待不得占 session 锁，不忙轮询；注册通知与检查信用必须避免丢唤醒，取消/flush/期限到达必须唤醒并释放，重试不得延长原期限或重复提交。
5. **唯一预算转换方案**：source 从创建起将 metadata 与每流 payload 放入同一 preparation root 的不同 direct children，freeze 原子封存这些原 owner 的额度；不枚举、不迁移已发布 lease，不建立平行账本。旧 held 已包含在最终额度内，释放后成为原 child 可复用信用，不永久扣除、不重复计费。`fromReservation` 保留其既有独立 backing 语义，不用于迁移这些 child；事务内继续复用未发布 lease 的 `split`。
6. **可行性与失败**：固定费用、启动进展和最大不可拆事务无法共同容纳时，在 DAG 发布前明确失败。暂时占用产生异步背压；协议错误、取消、原期限耗尽、分配错误各自原样传播。必须证明信用等待不会阻止释放所需的下游进展；不能靠实际运行到压力失败来代替准入证明。

## 首批独立实施边界：共享预算原子封存

本批仅修改共享 `MediaStorageBudget` 的原子封存、释放与严格快照语义及必要计数器支持。先经两名独立审查者明确 PASS 再实施；不接入 source 分类、协议事务或新费用公式，不降低候选费用，不宣称解决 r402。所有新增内部结构及临时数组仍按实际 sizeof/容量计费。

定义每个 direct child 的实际已预约字节 `Aᵢ`、封存保证 `Gᵢ`、未使用保证 `Uᵢ=max(Gᵢ−Aᵢ,0)`，计费占用 `Cᵢ=Aᵢ+Uᵢ`。未封存时 `Gᵢ=0`；封存后容量等于保证且 `Aᵢ≤Gᵢ`。root 的 `Aroot` 包含所有 child 实际预约及 root 直接预约，`Uroot=ΣUᵢ`，`Croot=Aroot+Uroot≤B`。这里 actual 指实际 lease 预约，不是 RSS；reservation-backed budget 的 backing lease 仍按原语义算作原 owner 的 actual，不能穿透再算子分配。

- 快照显式区分 `actualRetainedBytes`、`guaranteedBytes`、`chargedBytes`（以及容量）；child guaranteed 为 `Gᵢ`，root guaranteed 为所有 direct child 保证之和，root charged 由 actual 加 unused 保证计算，不能将 actual 与整个 guaranteed 直接相加。未使用保证可由账本保存并核验。现有 retained/high-water 消费点逐一明确语义，不能静默改变字段含义。
- partition 同一锁下返回 selectedActual、selectedCharged、otherActual=`Aroot−ΣAselected`、otherCharged=`Croot−ΣCselected`。求解可用额度用 `B−otherCharged`；实际 held 比较用 actual。batch 若另列，必须在其实际所属 selected child 中只扣一次；已封存额度中的 batch 释放不会自动释放 root charged，不能沿用“扣 actual 即得到可用”的旧等式。
- 所有 reserve 路径统一保护 `Uroot`：root 直接申请、未封存 child 申请不能消耗其他 child 的 unused 保证。封存 child 在保证内申请使 actual 增加、unused 等量减少，charged 不变；超过其容量按既有失败/背压语义处理，不借用其他保证。
- 封存 child 释放 lease 时 actual 减少、unused 增加，charged 不变，其他源不能抢走保证。必须唤醒该 child 的信用等待者；复用同一 root 通知域。child 最后销毁时无存活 lease，锁内解除其 unused 保证并更新 root，锁外通知；析构路径不得分配或回调重入锁。
- 批量封存接受同 root、互异 direct children 与其目标额度，拒绝 root/跨根/重复成员、低于 actual 的额度和重复封存。锁前准备输入及结果；root 锁内先检查全部身份、加法溢出、held、总 charged，再无抛出地同时更新所有 child 与 root。任一检查失败，全部账目和配额不变；锁内无分配、析构外部 owner 或通知。
- 后续 source freeze 持有全部 session 锁，先完成可能失败的产品构造，再调用一次 root 原子封存；之后只能 noexcept 发布全部 admission。失败时原 owner、lease 和配额不变。发布后锁外通知。root 直接分配即使不受 session 锁约束，也受 root charged 检查约束。

## 后续协议推进状态（本批不实施）

事务粒度为一个 `pendingFront` 的 `preparePacket→commitPacket`，不是整个 reorder batch。`observeRtp` 已接受并改变 reorder 后，ordinal 必须恰好推进一次；内部返回类型区分 AcceptedReady、AcceptedWaitingCredit 和真正错误，不能用普通 WouldBlock 表示“未接受”并重放报文。

| 状态 | 动作与下一状态 |
| --- | --- |
| Ready → Prepared | 接受报文并推进 ordinal；准备当前 pending 项的实际申请清单与输出槽需求，不调用 releaseExisting。 |
| Prepared → WaitingCredit | 无足够对象槽或字节时保留 pending；暂停该源下一批接收，保存当前 receive batch 及游标，下游继续取已有输出。 |
| WaitingCredit → Reserved | root 信用释放或 output 槽释放唤醒接收泵，调用明确 resume/drain 入口；session 锁内同时确认槽与整批字节，失败不保留部分预约，锁外等待。 |
| Reserved → Committed | 新事务预约成功后才执行 releaseExisting/协议 commit；因此费用包含旧 assembly 与新预约重叠。分拆 lease，发布输出，再 consumeFront；不得重复提交。 |
| 未提交 → Cancelled/Failed | 取消、flush、原期限到达或真实错误回滚未提交信用并唤醒；不延长期限，不吞原错误。协议已提交后的错误按会话失败清理，不伪称协议可回滚。 |

`MediaPreparedRtpSourceSession::consume/drain` 与 `MediaRawRtpPreparedInputBuffer` 的 batch/单包捕获路径必须共同修改：当前两者都把任意失败记为终止错误。通知注册/信用检查要避免丢唤醒；等待不持 session 锁，已收到 batch 的剩余数据也计费。`takeOutput` 只归还槽；payload 信用直到其最后 lease 引用释放或合法下游信用转交才归还。

**尚未完成的进展推导**：各源关键帧装配、音频 startup、pending/output/binder 同时持有、旧/新 generation 重叠决定最小进展额度；尚未得到完整权威上界与池分配产品。不能将最大单包替代整个进展需求，也不能平均切预算。此项、共享弹性额度策略及完整最大 k 可行域证明必须在后续接入前另经双审，首批封存机制通过不代表这些事项通过。

## 下一批完整接线：创建时分类与同域快照（待双审）

本批从共同 source 创建到 freeze/solver 的身份与 held 观察完整接线；不调用 seal、不启用背压、不降低原候选费用。单源 canonical 与 composition 共用预算 bundle、协议分项和快照类型，不新增专用媒体链。

**root 与创建控制块**：bundle 接收明确容量 B 与现有 scope。Shared 必须指向 root，直接复用；Standalone 仅在已有权威 B 时先创建一个独立 root，再把 metadata、videoPayload、可选 audioPayload 都创建为该 root 的 direct children，不将原 source child 当父节点。当前 `MediaRtpSourcePreflight` 的 continuous 分支明确要求 Shared 且从 root 取得 B；本批保留该校验，不从 probeSize 或 legacy workingSet 猜测 standalone 的 B，也不扩大原拒绝场景。有限 probe 路径保持既有行为。bundle 的 standalone 能力仅供已持有明确容量的共同创建入口使用。

现有 `MediaStorageBudget::create` 使用 new+shared_ptr，其 child 对象/control block 未归预算；本批 child 创建改用现有 `MediaBudgetBootstrapAllocator` 对 root 的一次实际 allocate_shared 申请计费（含 `MediaBudgetAllocationBlock` header/alignment），由 control allocator 持有 root，费用一直保留到最后 weak reference。费用归 rootOther 一次，不计入任何 source metadata/payload child，不叠加相同预计费。child 按真实 allocator 请求及 allocation block 几何直接计费，不以 sizeof(Budget)+经验控制块大小代替，也不增加未消费的预测接口。原 `MediaBudgetMemoryResource` 的 allocator-request readback 提取为单一职责的共用组件，保持其现有消费者和测量行为。根预算自身保持既有 bootstrap 创建，不令其 allocator 持有自己形成循环；本批新增的 child 控制块均有原 root 承担。失败沿用原错误，新增结构/资源控制块的真实费用不声称为零。

**协议分项守恒**：新增内部产品 `metadataBytes / compressedBytes / totalBytes`，在原公式构造处累加同一批项，checked 验证两项之和等于旧 total，严禁在消费者重新复制公式。metadata 包括对象/控制块、树节点、vector 描述/proxy、AU/NAL/header 描述数组、配置字符串与 clock/binder 状态；compressed 包括 reorder payload/extension、NAL fragment、AU assembly、signaling 参数集字节及临时 AU 字节。lease 控制块与 packet wrapper 留 metadata，AU allocation 的原预约仍归 payload，已有 padding/AVPacket 费用仍按原函数整体承保，不在本批拆小或扣减。

- canonical 原式位于 `MediaSourcePreparationStoragePlanner::protocolWorkingSet`：parser=packetPayload+packetHeaders；video signaling=(2×parameterSets+1)×AU+NAL views，assembly=2×AU+emitted+NAL views；AAC assembly=AU+emitted+headers；Opus assembly=AU+emitted；configuration 独立。原 emitted 含每 AU payload、padding、AVPacket 与 MediaRtpAccessUnit，按已明确的归属分项，保留每一个原费用项。
- `MediaPreparedRtpSourceStoragePlanner` 的 migrated 项（pendingResult、allocationRequest、MediaRtpAccessUnit 数组、AAC headers）全部从对应 metadata 分项扣除，不从 compressed 扣除。canonical 原式中即使与另行 AU reservation 存在保守重叠，本批也保留，不以分类名义降费。
- 非 canonical 的 `MediaRtpProtocolOrdinaryStoragePlanner` 保留原 tree/rangeVector/proxy peak：reorder payload/extension byte-vector backing、H.264/HEVC 两个 AU byte-vector peak、AAC fragment、Opus temporary 归 compressed；其余对象及配置 peak 归 metadata。byte-vector 的存储几何整体归对应 byte owner；不得另加一次相同 proxy。返回类型化总和，不改变普通容器峰值算法。

**执行归属**：source 原 PMR resource 改绑定 metadata child；`create` 的固定费用和协议 metadata 固定 lease 留此域；每流协议 compressed 固定 lease 与 `drain` 的 AU reservation 使用该流 payload child。stream 持有两个协议固定 backing，确保普通分配从创建到销毁始终有覆盖。pending→prefix/acquiring→output 的 payload lease owner 原样延续，退出/重入仍遵循既有生命周期。startup route/coordinator、holder、规划产品和 batch scratch 保留既有 metadata/root owner；decoded frame、GPU surface、graph payload credit 不迁入压缩 payload。仅费用分类，暂不改变 prepare/commit 或接收泵状态机。

**类型化成员与 exact arrays**：新增 source budget bundle 及 source role mapping，显式 metadata/videoPayload/optional audioPayload 索引；flat child 数为各源 `2+audio存在` 之和，checked 计算。freeze 使用既有 exact-capacity storage 构造 child array 与 snapshot array；每源 snapshot 保存角色映射和相应成员观察，明确总 actual/charged 求和，不把多 owner 汇总伪装成单个 `MediaStorageSnapshot`。`snapshotPartition` 一次观察全部 direct children，otherCharged 从 root charged 扣 selectedCharged；solver 校验各角色 owner 与 root 身份。source 0 metadata 中的 batch 费用只扣一次。sourceCount 数组和 childCount 数组分别计费，结果 admissions 仍按 sourceCount。新结构 sizeof 增长由现有精确数组计费同步覆盖。

**完整文件范围**：`MediaRtpSourcePreflight`；`MediaPreparedRtpSourceSession`及其 stream/session plan；`MediaSourcePreparationStoragePlan`；`MediaRtpProtocolOrdinaryStoragePlanner`；`MediaPreparedRtpSourceStoragePlanner`；`MediaPreparedSourceCandidateCostPlanner`（只消费守恒分项）；`MediaPreparedRtpStorageAdmission`；`MediaPreparedRtpBatchStoragePlanner`；`MediaCompositionBatchAdmissionSolver`；必要的 budget child bootstrap/readback 共用组件。新增 bundle 放 runtime/resource，角色及规划快照放 planner/realtime；不产生未被共同生产路径消费的独立 helper。

**后续未闭合项**：当前 composition root 用 `MediaStorageReleaseUnobserved` 创建，通知字段不可替换；后续背压必须从 root 创建时安装真实释放 notifier，并规划对象/目标存储费用及生命周期，不能 freeze 后更换或容忍 NotInitialized。本分类批不启用背压，因此不顺带改变通知。分类批之后须紧接修正共享 AAC parser/header geometry：按两字节 AU-headers-length 与当前 16-bit 头格式得到 4095 合法头上界，保留全部合法聚合/分片，不使用样例包大小；否则旧 1.68m outputs 对象元数据仍可能超预算。该修正不混入本批守恒分类。各源最小进展、保证/弹性分配及事务状态机仍须后续完成；分类全量构建与原规格回归只证明接线没有改变既有失败，不代表 r402 修复。

## 修改阶段

| 阶段 | 共享修改与交付证据 |
| --- | --- |
| 一：协议与成本事实 | 扩展 RTP 完成批次类型，复用 AU parser/depacketizer；将最大数量、总字节、分片持有和控制费用分离。列出各协议合法极值及多批累积反例，保留所有原合法输入覆盖。 |
| 二：预算与执行 | 在首批封存机制通过审查后接入原 owner 的 metadata/payload 分类及原子对象/字节信用；`MediaRtpInputProtocolSession` 和 prepared session 按事务提交，复用释放通知，贯通取消、flush、重入及失败清理。不能先降低 planner 费用后补执行契约。 |
| 三：共同规划发布 | 同步 CandidateCostPlanner、BatchAdmissionSolver、AdmissionMaterializer、sealed validator 和最终资源编译；共同规划固定/启动保留、源进展和联合 payload 池，保持完整合法容量搜索。sealed 独立 readback 核验信用身份、实际 held、容量与费用；最终 runtime 必须消费同一产品。 |
| 四：审查与实流 | 冻结后两名未参与实现的审查者按上述权威机制、所有权及失败状态机分别明确 PASS；Windows Release 原规格真实 realtime 先行，随后 RKMPP 增量构建与同规格链路。共享算法不分平台，仅保留真实硬件/系统 adapter 差异。 |

## 原完整门禁与观测

保持指定 120 秒连续源、原四路 H.264/HEVC 与选定 AAC 输入、2 GiB 准备预算以及 r402 的完整转换/输出参数；不以简化链路替代。验证 2–4 路固定格、选定音频、逐源丢失黑屏/静音与重入、全源丢失时连续输出、恢复后持续 A/V 漂移。画面使用 VLC 原生快照，不使用系统截图。停止源流后观察 CLI 的场景预期行为，测试终止后按精确 PID 清理。

保留必要 telemetry：候选 k、root/other/batch/available、各源 fixed/protocol/startup、实际 held、联合池容量/占用、对象槽占用、不可拆事务费用、等待/释放/取消及失败原因；同步观察 CPU、RSS/private 趋势和 A/V 漂移。计费必须包含新增类型的真实 sizeof 与容器容量，不能宣称内联字段无预算增长。

Windows 通过后才进入 RKMPP 对应验收；两平台使用同一 planner/runtime 信用算法。记录真实 CLI/FFmpeg/VLC 命令、结果与限制后，立即按本轮清单删除日志、快照、抓包和临时媒体，保留指定源与正式文档。实现、构建、单轮输出或局部审查均不等于任务完成；未通过项目仍须明确列出。

## 首轮独立设计审查

两名独立审查者均判定实施设计 FAIL（方向成立）：尚需确定每源进展保留不等式、唯一旧 lease 迁移方式、等待时接收泵/expiry/RTCP及release重试路径，以及prepare→commit的真实事务边界。当前reorder已先变更，consume的ordinal又在drain成功后推进；不能把普通错误替换为等待并重放同一报文。下一版需明确内部“已接受但待信用”的类型化状态，保持一次接受、一次提交和原期限，不新增对外参数。未降低任何费用或开始执行此预算算法。

首批预算封存设计已获两名独立审查者明确局部PASS，允许实现该机制；完整每源进展、协议事务与接收背压仍未通过。本轮RKMPP bd6aafc8增量121/121退出0，四个专属构建日志/退出标记清理后残留0。未执行本轮RKMPP媒体验收。

## 首批实现进度

六文件实现已冻结并获两名独立源码审查者局部PASS：MediaStorageBudget提供一次性sealPartition、actual/guaranteed/charged快照及双高水位；同root全验后无抛出提交，释放/析构锁外通知。未修改fromReservation语义，未接入source分类或降低费用。旧solver与source admission显式拒绝尚未接线的sealed产品，防止误用旧batch算式。Windows76595全核心Release重建退出0；[r403](realtime-composition-r403-validation.md)原四路回归仍因33.18GB候选费用拒绝，不能计合屏通过。没有生产sealPartition调用，封存执行与唤醒尚无实流覆盖。RKMPP六文件分组同步进行中，后续8核增量构建终态待收取。

RKMPP710832f9构建终态已核验：后台PID153698已不存在，8核增量476/476、退出标记0，realtime CLI成功链接。该批未执行RKMPP媒体测试；不能据此推断seal执行或合屏通过。两个专属log/exit文件读取并记录后逐项清理，正式构建产物保留。

## 分类完整接线实现进度

23源码已冻结并获双独立局部PASS；原协议项费用守恒，新 child/control/数组实际费用完整计入。Windows97346全核心Release重建退出0。[r404](realtime-composition-r404-validation.md)原四路已进入k0预算计算，无新增角色身份错误，仍因31.26GB源费用超限而自然退出1，不计合屏通过。四源清理exit -1，六精确PID与11项日志残留均0。未启用seal/背压，最小进展与实际输出仍未完成。

为避免逐文件SSH往返，23源码生成临时同步包，73465字节，SHA256 `1ccd785c0e0833c1a29502b1c29708ce055baaf2ab8c5b35011f6db9a56d3fc8`；内含每文件原版本与新版本hash，远端先全部验证再逐项写入并复核。仅项目源码，不含媒体或外部依赖。同步后删除本机/远端该专属包，RKMPP构建终态另记。

同步终态：23文件写前与写后hash均通过；远端临时包已删除，本机专属包随后删除。RKMPP后台构建PID302747已启动，8核增量all-target，日志/退出标记为composition-build-16d1cffa，终态待收取。

## 下一步 AAC 数量几何

依据RFC 3640的两字节AU-headers-length和当前13-bit size/3-bit index布局，公共协议定义提供两个不同查询：解析头数上界=min(UINT16_MAX/headerBits,(payloadBytes-lengthBytes)/headerBytes)；合法完成AU上界=min(UINT16_MAX/headerBits,(maximumPayload-lengthBytes)/(headerBytes+minimumAuBytes))。小于最小长度须显式报错或返回空界供原错误路径处理，不允许无符号下溢。当前headerBits=16，合法协议头数界4095来自字段宽度，不是经验常量。

parser、depacketizer factory、access-unit envelope及PMR header容量统一消费布局；保留原错误顺序与畸形无payload/短payload头表的临时分配覆盖，不能以合法完成数给parser vector计费。canonical legacy header覆盖及对应migrated扣项本步保持原值；分片仍按单AU完整size预约。只修协议数量几何，不解决跨批载荷信用、视频费用或接收背压，实施前再核对独立审查结论。
