# 有限prefix批量发布可行性调查

状态：容量与算法设计已获两名独立审查条件通过，实现已冻结待双独立代码审查；尚未构建或实流验证。r348单报闭环及纯媒体超期已经证实，但不能把全部上游等待归因于闭环。不得扩大队列、延长期限或使用任意batch常量。

## 现有可复用机制

修改前 MediaTsPendingEmission.cpp 以takePrefixView(1)发布并等待整份transaction提交后才继续。FileMuxNode.cpp:77在hasPendingOutput时优先poll，当前AU结束前不拉下一AU，因此每报承担完整跨节点调度往返。

MediaMpegTsProtocolDatagramBatchBuffer.cpp:331的reserveNextPrefix(count)已经支持连续多报，要求上个reservation全部提交；:561的takePrefixView(count)共享同一series backing，只创建一个已准入view，不复制媒体。MediaTsDatagramEmissionSchedule.cpp的wireBytesForPayload按实际payload计算报数及逐报协议开销，prepareAccessUnit接受多报payload。MediaTsPrefixServicePlan::materializationBatches按真实shaping.batch.maximumDatagrams给出materializer分块数量。

当前逐报schedule.commit使用window.release，没有因wire服务时长自行增加canonical release。因此无maintenance插入时，多个报共同release与现语义一致；共同release不是一次突发发送，sender仍按逐报真实wirebytes进行pacing。批内不得跨AU、generation或原deadline；有maintenance插入时只能把插入点前合法的连续范围归为一批，不能提前吸收插入点之后的报。

## 权威批界与维护保护

planner接收剩余payload/报数、原AU deadline、当前preview release、accepted-work availableAt、真实维护obligations、shaping产品及实际holder可用几何。选择满足全部条件的最大连续报数count：

- 非零且不超过剩余AU报数、原协议范围及实际可用payload/holder容量；最后一报可以短，其前必须保持完整最大payload几何。
- 以真实逐报ceil服务量及每materialization可能SR计算整批duration；max(release,availableAt)+duration不得越过原AU deadline及现有维护backward latestStart得到的completionLimit。
- 维护已经due或canonical release已达维护release时，沿原YieldMediaToDueMaintenance先yield。若一报都放不下，保留现有yield或真实超期错误，不使用零时长/放宽deadline。
- 不能把shaping.maximumDatagrams当唯一容量：必须使用既有maximumBytes、wire分区geometry、全局outstanding/backlog及真实memory admission；RTP可能有leading SR，可能形成额外wire分区。有限prefix可先限定不超过真实单次media materialization报数，但wire分区仍须权威核验。

现有PrefixPublicationPlanner的服务量参数可复用；批界选择应由planner完成，PendingEmission只执行类型化count/bytes/window。对于固定release、固定维护集合和合法连续payload，服务量随count单调，可用有界整数选择；必须保留checked算术、完整调用现有边界校验，不另写一套维护算法。该模型仍只给序列化需求准入，未证明OS调度延迟或共享scope最低服务保证，不能升级为完整实时可调度性结论。

## 提交、取消与存储门禁

保留一份活跃prefix reservation，不引入多个并行reservation或新队列。当前commit模型按连续count推进；只有transaction committed==size才唤醒producer继续下一prefix。部分成功已计入真实watermark，余部失败或析构仍走abandon/poison，不能回滚已发送报或重发整个prefix。批量化不得把任意部分提交当整批成功。

修改前需逐项确认：RTP/UDP两个consumer真实分块都保持相同publication identity及连续范围；两个固定prepared/bound槽在较长reservation内替换时不覆盖尚活跃工作；SR插入不推进媒体commit count；global callback释放global锁再进入series的锁序不变；actual prepared/bound与未prepared剩余范围计费没有重叠漏项。

一个view占一个现有mux holder，但其payload footprint变大。须核对validateProtocolBatchCapacity的剩余字节与slots、wire准备最大临时重叠、固定commit geometry和准备失败原子性。不得只以队列还有一个slot宣布整批可准入；不得把已有预算不完整问题宣称已经解决。保留先准备存储后改变continuity状态的现有顺序。

## 工业对照及结论

[GStreamer queue](https://gstreamer.freedesktop.org/documentation/coreelements/queue.html)支持有限buffers/bytes/time边界、满时背压及线程解耦；[Linux NAPI](https://docs.kernel.org/networking/napi.html)以有界poll budget摊销逐事件调度，并在仍有工作时继续服务。两者仅支持有限批处理机制的方向，不提供本项目AU/维护deadline算法，也不证明Windows拥有NAPI平台能力。

结论：不必为维护保护强制永久count=1；整批服务完成上界受既有维护latestStart约束即可保留当前模型级保护。但直接把1改为最大batch是FAIL。完整实现必须闭合上述count产品、RTP/UDP分块、实际holder/临时资源及部分提交证据，再双独立审查和原规格realtime实流验证。当前调查不替代periodic忙期原目标；建议先补每AU commit等待与唤醒分段汇总确认收益来源。

## 后续门禁核验

crash_release_audit只读确认RTP/UDP连续分块、Prepared/Bound两槽替换、global返回后再protocol commit锁序、commit/output/workspace统一Network准入机制可复用。此前容量设计判定FAIL已由下面的真实预算核验取代；不代表实现或运行通过。


## 本轮实施范围与容量裁决更新

两名独立审查已确认：NetworkResourceLedgerPlanner 的 batch<=backlog（UDP）及 batch<backlog（RTP）保留可能 SR 的 outstanding entry；mpegTsStoragePlan 按完整 batch 的 payload/workspace 和可能 SR 形成准备预算，WireProducerStoragePlanner 覆盖 preparation、active、handoff 并存与最坏每报一个 wire partition。maximumBytes 只拆分 wire partition，不截断一次协议 preparation。故 prefix 上限使用该已准入的单次 media materialization maximumDatagrams；不额外读取 global/Network 瞬时空闲快照冒充预留。运行期仍以 mux 当前剩余 bytes/slots 收窄，实际 wire/Network 准入保持原样。

实施复用既有 PrefixPublicationPlanner 的单报判定及 backward latestStart：先完整评价一报，原 error/yield 不变；只有单报获得 window 后，固定 release、accepted snapshot、维护 window，以既有整数服务计算的单调性进行有限二分选择最大合法 count。候选计算错误直接传播，不捕获后当作不可行，也不按错误字符串分类。结果为 count/bytes/window 类型化产品，Pending 仅验证并执行；每次只保留一个 active reservation。尾部 validateTail 仍按每报可能 SR 保守计费，不实施 SR sliding-window 或完整周期需求的新算法。

成熟机制对应仍为上述 GStreamer 有界队列和 NAPI 有限预算摊销；deadline 判定复用仓库既有服务产品和 latestStart，二分仅选择单调可行集合，不宣称工业系统提供本项目截止期保证。r350 的 12 次 prefix publish→protocol commit 合计约 7.21 ms、commit→resume 约 0.413 ms 是本轮减少串行握手的事实动机，不证明全部迟到来自握手。

独立诊断范围：失败/abort 时复用固定 previous observation 输出其已有 processing trace，stage 明确 previous_completed_au_trace(_on_abort)，不增加历史、身份策略或生产状态。prefix 计数仍指 reservation 次数，批量后不等同 datagram 数。本单元不代表完整 periodic 目标完成。


## 实施冻结

已接入 MediaTsPrefixPublicationPlanner::planBatch → MuxSession::preparePendingPrefix → PendingEmission::materializePrefix，结果含 datagramCount、payloadBytes 与原 window。原单报 plan 及维护消费者不变；二分最多 O(log(min(AU剩余报数, shaping最大报数, mux字节界))) 次既有服务求值，整数运算按合法尾部几何避免乘法溢出，计算错误原样传播。单次选取不分配内存，产品只在栈上存在，不新增持久字段或预算项；protocol backing/view/commit 的实际 sizeof 计费和原准备顺序保持。

运行期 mux bytes 与 slots 在既有会话串行边界内读取并消费；global/Network 仍执行真实准入，没有 snapshot 预留。diagnostics 的 published/completed/samples/offset 仍以 prefix reservation 为单位。已有 worst_protocol_batch_datagrams 在 takeScheduledBatch 记录真实 view 报数，可观察被采样的最迟 batch，但不是最大 batch 报数统计，不新增逐包日志。

本轮仅修改 PrefixPublicationPlanner.h/.cpp、PrefixServicePlan.h、PendingEmission.h/.cpp、MuxSession.h/.cpp 及本文；没有新增 cpp 或构建配置。已检查接口引用与 UTF-8/CRLF，未执行构建或运行验证。剩余门禁为两名独立代码审查和原规格 realtime 链路；尚不证明性能收益、原失败已修复或完整 periodic 服务闭合。

有限prefix实现经crash_release_audit与publication_identity_review_b双独立源码PASS，确认单报error/yield不变、typed count/bytes/window完整接线、短尾与实际容量成立、整reservation提交和部分失败语义保留。开始Release全量重建，原规格r351尚待运行；不得将源码PASS当性能/周期目标完成。

r351实际worst_protocol_batch_datagrams=16，sender75批/600数据报；最近完成生命周期仅1prefix，但publish到protocol commit仍9.2503ms。sender最后16报首末提交间隔9.1138ms，说明减小mux交接次数未消除批内等待。继续追踪sender等待与同lineage多输出，不把批量生效当原期限/整链通过。证据及精确命令见realtime-composition-r351-validation.md；产物与进程已清理。
