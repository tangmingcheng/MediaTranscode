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

## 后续完整背压：固定 RTP/RTCP channel 双 pump（待双审）

本节取代上文“等待时暂停该源下一批接收”的笼统表述：只暂停 RTP pump，RTCP pump 必须继续独立进展。前述诊断和已实施步骤保留为阶段证据。本节不是实现完成或验收通过声明；不得先降低 payload 费用再补执行。

### 成熟机制与仓库差异

[GStreamer rtpbin 官方实现及接收示例](https://github.com/GStreamer/gstreamer/blob/main/subprojects/gst-plugins-good/gst/rtpmanager/gstrtpbin.c) 将独立 udpsrc 接入同一 session 的 recv_rtp_sink/recv_rtcp_sink，以 RTCP SR 同步媒体；[udpsrc](https://gstreamer.freedesktop.org/documentation/udp/udpsrc.html) 基于 GstPushSrc，可使用外部 socket。采用这种控制与媒体输入分别驱动、会话共同同步的结构，不复制第二套媒体协议或 DAG。其 udpsrc 允许超出 mtu 时扩展缓冲，本项目坚持 planner 固定 arena、最大 datagram 和显式失败，不能照搬该扩容行为或默认容量。

现代码的 `MediaRtpUdpTransport` 以一个 receiveMutex/receiveActive 覆盖两个 socket；Windows adapter 同时投递两路并 drain/repost，Linux poll 同时监听两路；`MediaRtpIngressReceiver::receiveNext` reset 唯一 arena，batch lease 尚活时失败。因此不能仅启动第二条线程共享现 receiver。固定 channel adapter 可避免动态 mask、信用 IOCP completion key、信用 eventfd 和混合 batch 移槽状态，但仍需明确 channel 独占及停止屏障。

### 类型、线程和存储

- 内部 `MediaRtpIngressChannelPlan` 绑定现有 `MediaRtpUdpChannel`、该 socket capability/readback、datagram 上限、descriptor/arena 几何和取消域。共同 receiver/adapter/pump 实现按该固定产品实例化；不增 CLI/API，不建立 codec/platform 特例。`acquireIngressReceiveLease(channel)` 每 channel 独占，两个 lease 共用 transport 生命周期；禁止同 socket 双读。
- 每个已声明 RTP/RTCP 会话对应两个 pump，是协议 channel 数推导的固定线程数。RTP 与 RTCP 各有自己的 arena、descriptor、在途 receive 和 batch lease；线程/通知对象及 allocator 请求纳入 root/metadata 产品，线程栈/OS 资源单独记录真实平台创建事实，不冒充引擎准备字节或写经验栈大小。按现有线程 RAII/stop/join 管理，不临时创建恢复线程。
- 每 channel arena 费用由现有最大 datagram、该 socket 的实际 receive buffer 能力及 planner 分配的共享部署额度，通过现有 descriptor 几何计算；还须计 batch entry、slot ownership、Windows operation 或 Linux mmsghdr/iovec 的真实容量。两 channel 实际总费用与 bootstrap 重叠一起满足 root 预算，不能各自重复宣称独占全部 B，也不能把现有混合 descriptorCapacity 直接复制两份却只计一份。缺少任一 channel 权威 capability 或固定内存时，发布前失败。
- Windows 每实例只把其 channel socket 关联自己的 completion port，并仅投递/drain/repost该 channel；Linux 每实例 poll 该 socket 与取消句柄，recvmmsg 沿用固定数组。信用等待发生在 RTP pump 的接收调用之外，复用 `MediaNodeWakeup::sequence/wait/notify`；不调用取消型 interruptReceive，不增 OS 专用信用唤醒。
- 初始化先建立两套产品/arena/wakeup，再启动 workers；任一启动失败，记录原错误，request_stop并唤醒已启动worker、join完成后销毁arena和socket。停止同时唤醒 socket wait 与信用 wait；不得持 session/capture 锁 join，worker 不自 join。root 创建时安装同一合法 release notifier，弱引用到 pump wakeup并计费；不得事后修改现有 const 通知域。

### bootstrap、身份和锁

**修订后的唯一生命周期**：从首次 bootstrap 收包开始就创建固定 RTP/RTCP 两套 receiver/pump，socket→IOCP 关联直到销毁都不改变。Windows handle 关联 IOCP 后不能在保持 socket 打开时改绑另一 port，因此废弃“先混合接收、再拆两 receiver”的交接方案；不新增中央完成分发线程。bootstrap→continuous 只切换同一 pump 的消费阶段，不重建 socket、receiver、arena、IOCP 或 worker，也不调用取消型 interruptReceive。


阶段交接只冻结消费目标与已发布prefix成员，不声称capture锁能冻结OS完成。使用现有capture mutex/condition及probe lease固定每channel的已发布prefix长度和frontier；持lease期间该轮归并不扩展成员。两pump已返回但尚未发布的batch/cursor仍在原固定arena内，费用与prefix同时计入，最终cut后按原channel ordinal进入live恰好一次。尚在途的IOCP/recvmmsg项不强行纳入cut，不取消、不移槽，完成后按原消费阶段规则交付。交接锁同时发布共同session与每channel已接受frontier，worker在发布点读阶段，避免同一条目既replay又live。

不存在跨轮tail要求：FirstFrameProbe每轮重建reorder和previousArrival=0，对该轮固定prefix全量独立遍历；最终Preflight只新建一次session，对最终固定prefix单次归并，然后finishBootstrap解除历史全局单调检查。下一轮prefix可能补入早于上一轮尾部的其他channel条目，不拼接两个cut即可；不建立在线水位，不为此停所有OS完成。现MediaRtpUdpTransportPhaseController只有receive/stop/abort诊断同步点，不冒称它已提供生产快照cut；生产切换复用capture/probe已有锁与lease生命周期。

`MediaRtpIngressProvenance` 保留 channel、channel ordinal、接收时间、既有RTP sequence/SSRC；从首次收包即使用channel ordinal，不再建立旧混合capture ordinal，不伪装跨socket wire总序。channel ordinal在本pump接受datagram时恰好前进一步，session分别检查各channel连续性。进入共同session锁后分配现有内部事件/媒体generation序号；Clock/Event共享严格递增顺序。锁序为capture状态短锁取owner→释放→session锁；信用等待/网络等待/通知均不持session锁。

### 事务、控制输出与恢复

| 状态 | 具体执行契约 |
| --- | --- |
| 接受 RTP | observeRtp成功即提交channel ordinal；prepare当前pendingFront的申请清单，后续返回AcceptedReady或AcceptedWaitingCredit，不重放datagram。事务只覆盖prepare→commit，不回滚已接受的reorder。 |
| 等待信用 | RTP pump保留本channel batch与游标，停止调用receiveNext；RTCP pump仍可进入同一session。等待前读取wakeup sequence，session锁内重试预约，锁外wait；释放/控制进展发生在检查与wait之间也不丢唤醒。 |
| 恢复 | 对象槽与整批payload费用同时取得后，才releaseExisting并commit；复用Lease::split。旧assembly与新预约共存费用保留；最后consumeFront，再继续旧batch，旧batch完成后才能reset该arena。 |
| 控制推进 | RTCP观察一次返回invalidation或可选evidence，现session分支每次最多产生其中一种控制输出；tick、RTP discontinuity以及pending reorder也会产生控制事件，必须分别由各自既有状态机产品计入，不能将RTCP的单次上界当作所有事件总界。控制对象/字节有专属保证，不能被RTP包用尽。 |
| 下游调度 | session将Packet与有序Control输出分开；Clock/Event共用Control FIFO。共同ControlOutput DAG节点通过shared session的typed control endpoint消费全部RTP/RTCP/tick生成的Control FIFO，输出到既有clock/event目标。其pendingTransfer与Raw的packet pendingTransfer天然独立，不修改所有节点的共同runtime lane。Raw仍是唯一RTP generation purge身份；ControlOutput不另立媒体源或第二个purge owner。不得让Clock越过早先Invalidation；DAG控制边及下游metadata事务必须有独立进展额度，不能被packet边反压覆盖。 |
| generation变化 | RTCP invalidation/SSRC/loss事件复用既有generation purge/frontier，先登记fence再允许新generation媒体；清理被fence覆盖的Raw pending、prepared prefix/acquiring及协议carry。若需等待purge确认，保留原有控制顺序；不能为等待packet credit而阻止发出触发purge的控制事件，也不能恢复后重发旧generation包。 |
| 取消/失败 | 任一pump真实错误终止共同会话并唤醒另一pump；未提交事务RAII回滚，已提交协议按会话失败清理，不伪称可回滚。控制容量用尽、原期限到达仍显式失败；不无限排队、不覆盖旧控制事件、不延长原期限。 |

在Raw或prepared binder成功附着graph payload credit后，原preparation payload lease释放；其后的decoder/DAG持有归graph域。进展模型的终点必须采用这个真实转交点，不能把decoder驻留再乘入prep，也不能在takeOutput时提前宣称payload释放。

### 最小进展与联合预算证明

对每源i，定义由现有状态机可同时持有集合导出的准备payload进展界 `Pmin_i(k)=max_s[heldBeforeTransfer_i(s,k)+nextAtomicPayload_i(s)]`；状态s包含prefix/acquiring、旧assembly、pending完成批、Raw/binder待graph-credit所有者及允许的generation重叠，别名payload只计一次。nextAtomic来自协议批次实际总字节守恒与完整fragment预约，不是最大AU乘所有输出槽。graph成功接管后的持有不属于该集合。对象进展界用同一集合分别计算，不能用字节额度代替对象槽。

控制保证 `Cmin_i=max_s[queuedControl_i(s)+pendingControlEmit_i(s)+nextControlTransaction_i(s)]`，以checked对象数和实际分配几何分别求界；控制边独立额度同时覆盖下游clock-group/binder/purge推进所需metadata原子事务。每源payload与control保证均不得小于对应最大不可拆事务。若计算所需headroom不能由现有RTCP/purge/edge产品证明，先补类型化产品，不用“一两个槽”替代。

整体要求：`rootOther + batch + receiver/线程管理/通知固定费用 + 固定metadata + startup增量 + Σ(payloadGuarantee_i + controlGuarantee_i) ≤ B`，且逐源保证分别≥Pmin/Cmin；每个费用归唯一owner，已有held包含在保证内。还须验证等待图：RTP等待的prep释放可由独立Control/DAG credit推进达成，Control等待的对象释放不得反向依赖新的RTP输入。图存在环则拒绝该产品，不能以增加任意容量掩盖。

这里的max集合必须在后续实施前展开成现有holder/容器/事务产品的显式有限表达式，并证明旧/新generation互斥或重叠关系；当前源码尚无完整产品，不能据本节公式直接降费。部署不可能覆盖无限UDP生产者或无限下游停顿，保持原期限、丢失处理与失败语义，完整原规格门禁不变。

### 第二轮审查阻断与有限推导任务（当前 FAIL，未实施）

两名独立审查者尚未批准完整双pump+背压。IOCP关联冲突已按上节修订设计，但仍须复审；以下执行与进展缺口不能用“异步等待”或抽象max表达式代替。

`MediaRtpInputProtocolSession::observeRtcp` 和 `expire` 当前有 hasPending guard。RTP AcceptedWaitingCredit必然保留pending，直接调用现接口会终止会话；不能删guard。须将同一协议状态机的控制推进明确为：先解析/规划本次RTCP或时钟到期结果并取得控制输出信用，再在session锁内提交。未失效的SR只更新同generation时钟证据并保留pending事务；BYE/clock invalidation先作废该generation未提交事务、清理协议pending/assembly并登记purge fence，再发布有序失效事件。不得保留指向已清空pendingFront的申请清单。reorder expire在等待期间不能覆盖已有pending；到期时间仍推进到控制时钟检查，但reorder释放须在原pending事务处理完或明确失效后进行。为避免先变clock后分配失败，需要从现有clock observer提取共同prepare/commit事务，不另写SR/BYE算法。

下一步只需完成以下四项源码调查并落实有限表达式，不再新增同义规划层：

1. **准备payload六个根**：直接沿用CandidateCostPlanner的Prefix、Acquiring、Outputs、ProtocolPending、RawInputCreditWait、BinderCreditWait。Prefix/acquiring界来自candidate containers；Raw/Binder各单对象来自实际成员，不是经验常量。handoff前prefix/acquiring由session持有，后由MediaRtpPacketClockBinderNode持有；`purgeMedia`不会替代binder清理，必须核对现有purge确认点。旧assembly固定费用已在compressed protocol域，计算新事务重叠时只增加新预约，不能再次加同一assembly。
2. **可先写出的安全payload不等式**：令U为现有preparedMediaPacketStorageBytes(maxAU)，Np/Na为prefix/acquiring上界，T为一个prepare→commit的整批最大实际预约（AAC聚合总字节+逐AU overhead、fragment完整AU、视频原最大AU），Wraw/Wbinder为实际单对象等待成员。bootstrap进展至少覆盖`protocolCompressed + (Np+Na)×U + T`；handoff运行阶段至少覆盖`protocolCompressed + (Wraw+Wbinder)×U + T`，若prefix/acquiring尚未移交graph credit或purge尚未确认，它们仍须计入同一阶段，不能取不成立的互斥max。Outputs使用受实际字节池限制的可转交积压，不再承诺每个数量槽都能同时放maxAU；但该改变必须与整批背压执行同时发布，且只有下游graph已准入必要startup/解码进展才可等待其释放。是否需要额外pending旧reservation与T共存，逐项沿releaseExisting/fragment状态证明后明确加项。
3. **控制对象计数**：从实际分支得到单次RTCP最多一个可选evidence或invalidation；单次pendingFront最多对应一个discontinuity，但RTP evidence与该项可共存；tick还可能产生invalidation。分别展开其控制事务对象数，不把三种调用叠成无界循环。Cmin至少覆盖Control FIFO已持有槽、独立pendingControlTransfer、下一控制事务最大对象及其CNAME/事件分配几何；FIFO满时RTCP pump停在本channel控制信用等待，释放由独立control DAG路径推进，不依赖新的RTP。具体FIFO与下游原子metadata需求复用edgePolicies和现有clock/purge队列产品，不任意指定槽数。
4. **共同ControlOutput节点与信用转交**：复用普通FFmpegNodeRuntime的独立实例，使控制节点自己的pendingTransfer只持Control，不受Raw packet flush阻塞；核对typed session endpoint、对应图边credit及两节点调度唤醒；逐项核对Raw attachPayloadCredit、binder m_preparedPendingCredit的成功转交和purge释放。输出控制事件保持严格sequence顺序，不能用最新SR覆盖较早失效，也不能为了调度控制绕开graph metadata预算。若既有产品不足以证明下游控制进展，只补该缺失的持有/事务事实后重新双审。

上述不等式是可审计的保守下界/覆盖要求，尚不是已证明充分的最终Pmin；不能据此降费或声称2GiB已可行。交付下一版设计时必须用上述四处实际代码把仍存活集合、互斥关系和控制容量表达式填完，并明确每处PASS/FAIL；不通过者继续阻止完整实施。

### 可独立接线的首个生产切片（仍需双审）

先将 `MediaRawRtpPreparedInputBuffer::startPreflightCapture/capture`、continuous batch入口、Transport::acquireIngressReceiveLease(channel)、AdapterFactory/Receiver::create以及两OS adapter全部接成从首次收包就固定channel的双pump。维持当前consume完整drain、RequiredAllocation和原候选费用，不seal、不启用AcceptedWaitingCredit、不删除RTCP/expire guard、不改变Raw控制调度；因此不会提前引入“pending期间收RTCP”的新行为。该切片的目标是完整生产socket/receiver生命周期，而非仅新增未消费类型。

两pump的交付串行区覆盖消费序号分配与当前consume调用，RTCP进入时前一个RTP事务已完整drain。网络接收/信用等待不得持此串行区；保留每channel原接收ordinal/time，另以内部delivery ordinal表示共同session接受顺序。异步接收完成时间可能倒序，必须同步分离原接收provenance与处理时钟：审查所有bootstrap replay、ingressObservation及m_bootstrapObservedAtNs消费者，不能重写历史时间或直接移除单调校验。双receiver的arena/control/线程管理费用精确增加，原存储公式不下降；若新增费用使原预算提前失败，记录真实结果而非缩规格。该切片通过不授权启用完整信用等待，剩余控制释放环仍受上述FAIL门禁约束。

### 固定双channel基础切片的真实时间方案（待双审）

接收时刻在各adapter成功接收/取到completion时采样并随entry保留，永不改成取得delivery/session锁的时刻。每channel本身顺序接收，分别保持原单调断言；交付串行区只保护session状态及delivery顺序，不能宣称其顺序等于捕获时序。

- `MediaRtpIngressObservationCollector` 每channel独立，仍按真实capturedAt计算该channel的首尾、interarrival与数量；RTP sequence displacement及reorder等待推导只消费RTP collector，RTCP的socket/字节观测消费RTCP collector。跨channel max gap不能从两个max gap相加或取max冒充合并流事实。共享部署成本对两个产品求和，流量时间统计保持显式channel身份。
- raw bootstrap分别保留每channel有序prefix及ordinal；probe快照通过固定数量游标的k-way归并视图（比较capturedAt，等时按稳定stream/channel次序）读取，不排序、不复制payload、不生成额外引用数组。首次视频probe合并其RTP/RTCP两视图，`MediaRtpSourceFirstFrameProbe::previousArrival`原断言保留；source bootstrap将现有Preflight两stream merge复用扩展为最多四channel游标，`m_bootstrapObservedAtNs`原跨stream单调断言保留。所有接受frontier均按channel记账，归并输出顺序不是虚构wire序号。
- `MediaRawRtpPreparedReplayClock::observe`激活前本来保存最小sourceOrigin；epoch取真实首个capture边界：各channel在首次接收采样时、对外发布entry之前同步调用共同replayClock.observe，保留全channel最小sourceOrigin；activate必须等所有已开始capture但未完成observe的发布临界区退出。尚无已采样时刻的在途OS项不需要纳入。激活后仍保留observedAt>=sourceOrigin校验；不得让线程将已采样的早时刻滞留在局部变量、越过activate后才首次observe。归并cut与epoch同步不同，不要求所有已返回项都纳入同一prefix。
- live阶段显式区分capture time和evaluation time：observeRtp/reorder、observeRtcp/SR/CNAME存储使用原capturedAt；evidence可用性、timeout/tick和运行时event发布评估使用session串行执行时真实steady now。该now仅判断现在是否过期，不替换任何接收事实、SR年龄起点或provenance；bootstrap evaluation time仍使用k-way replay的历史capturedAt，避免把重放当作当前接收。
- 必须同步修正已有tracker总序假设：`MediaRtcpSenderReportTracker::observeMedia`采纳pending SR后，当前仅在RTP capturedAt≥SR capturedAt时置evidenceUpdatePending。双channel可先处理较晚SR再处理较早首RTP，导致已齐备证据不再发布。pending标志应表示“证据变化待评估”，采纳时即置位；是否可发布继续由现有evidence(evaluationNow)检查now≥SR/CNAME capturedAt与原timeout。缺失/尚未到时仍返回NotInitialized并保留pending，不绕过校验。SR NTP倒退与同NTP改变RTP timestamp的原失败保持不变，同RTCP channel不重排SR。

该时间修正必须和基础双receiver接线一批实施，不能先双线程再吞time-regressed错误。需额外核对：activity只接受同RTP channel的真实时刻；RTCP先于首RTP的pending-source采用路径；bootstrap与live切换时所有已完成entry的归属。基础切片仍完整drain，所以observeRtcp/expire的pending guard保持，不启用信用等待。两名审查者批准这些具体消费者后，才实施完整调用链，不再增加未消费的描述产品。

### 最小完整修改与审查门禁

修改共同IngressPlan/capability/readback、Transport per-channel lease/phase、Receiver/Storage及Windows/Linux固定channel adapter；共同capture pump与bootstrap屏障、source session事务/ordinal/双输出；共同ControlOutput节点与Raw唯一purge接线；planner的receiver/control/payload进展、root notifier和保证产品；solver/sealed/final resources统一消费。禁止只提交能运行的双线程却未闭合控制释放环。双审必须逐项确认线程与buffer生命周期、原子预约、事件因果顺序及每项进展表达式；任一项未证明则实施门禁FAIL。随后Windows原规格Release realtime→RKMPP增量与原规格实流，记录并清理本轮产物。

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

同步终态：23文件写前与写后hash均通过；远端临时包已删除，本机专属包随后删除。RKMPP后台构建PID302747（16d1cffa）已完成476/476，exit0，专属log/exit残留0。AAC 0b0b4917五文件同步hash验证通过，PID448136增量11/11 exit0，专属产物清理0；正式证据见[r405](realtime-composition-r405-validation.md)，记录提交0217805d。构建成功不代表RKMPP实流合屏验收通过。

## 下一步 AAC 数量几何

依据RFC 3640的两字节AU-headers-length和当前13-bit size/3-bit index布局，公共协议定义提供两个不同查询：解析头数上界=min(UINT16_MAX/headerBits,(payloadBytes-lengthBytes)/headerBytes)；合法完成AU上界=min(UINT16_MAX/headerBits,(maximumPayload-lengthBytes)/(headerBytes+minimumAuBytes))。小于最小长度须显式报错或返回空界供原错误路径处理，不允许无符号下溢。当前headerBits=16，合法协议头数界4095来自字段宽度，不是经验常量。

parser、depacketizer factory、access-unit envelope及PMR header容量统一消费布局；保留原错误顺序与畸形无payload/短payload头表的临时分配覆盖，不能以合法完成数给parser vector计费。canonical legacy header覆盖及对应migrated扣项本步保持原值；分片仍按单AU完整size预约。只修协议数量几何，不解决跨批载荷信用、视频费用或接收背压，实施前再核对独立审查结论。
