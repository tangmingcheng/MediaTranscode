# r243：独立网络存储域与首批实际计费

## 已实现

- 抽取 `MediaStorageBudget<Domain>` 与 lease/scope/snapshot，Preparation 改为专用域别名，删除旧 cpp，保留父子共同锁、计数、保证 backing lease 和归还逻辑；MediaBuffer 原类前置声明改为 include。
- 新增独立 Network 域及 `MediaNetworkMemoryResource`，不能与 Preparation lease/budget 互传。公共 PMR 分配逻辑继续复用 r240，不新增同义计数器或分配算法。
- transport activation 使用当前 plan 的 reservedUserspaceBytes 建网络预算；global ledger 及两组 reservation 固定数组实际向该 PMR 分配。资源由 state 保持，各 allocation header 也自持资源；额度在真实释放后归还。
- 当前资源对象和 budget 自身、RTP actions、批次载荷及其余 workspace 尚未纳入该 PMR；本项不是全链内存上限或完整准入证明，serial 仍为 true。

## 审查

模板对照初审发现三处自动抽取导致的 alias 语法错误，已修复；原实现保留至两人对照完成后才删除。对照确认原准备预算的锁和所有权机制一致，错误消息改为通用域描述，错误类别保持。

A/B 对当前已写代码切片均明确限定 PASS；完整几何及网络预算门禁仍 FAIL。

## 否决的候选几何（未进入源码）

不能使用 `(2+endpointCount)*maximumDatagrams` 作为已证明上界：UDP 通用调用路径未保证分配前请求数不超过总容量；wakeup 唯一不等于并发生产者唯一；record 出队不等于全部数组 owner 销毁。审查明确 FAIL，未将公式写入 planner，也不得据样例观测调整倍数。

后续应从实际生产者权限、transaction/commit slice 的最终销毁及分配前存储信用出发推导；完整几何未证明前不放开串行。工业原则与适用边界沿用 r239 文档的有限存储池和 credit 分离，不能照搬 Flink TCP 参数。

## 验证

Release clean-first/all-target 构建会话59133 exit0。已执行原规格三源、失源、重入及caller-stop复验，命令、画面与指标见 realtime-composition-r243-validation.md；本轮未复现r239错误不等于该问题修复，完整存储准入、四源及RKMPP未完成。
## 后续生命周期核验

独立只读审查确认正常生产路径：sender.recordSubmittedPrefix → commit slice → Model::commitSubmittedPrefix；`MediaWireDatagramBatchBuffer.h:150–157` 在累计提交到达 m_size 后立即 reset optional reservation，析构 RTP/UDP transaction 及其 global 两数组。残留 Model/partition 引用不会延长已 reset 的数组。生产 global reserve 调用仅见 RTP media、RTP terminal、MPEG-TS UDP，均通过该 Model 提交。

仍存在 global commit 归还信用到外层 Model reset 的短暂区间；部分提交时数组完整保留。以上只解决“完成后会不会被 partition 长期保留”的疑问，不重启已否决的并发容量公式。

主调用链补充：MPEG-TS UDP 的 production materializeProtocolBatch 在构造 views 前按 batchPlan.maximumDatagrams 取前缀；MPEG-TS RTP 同样按 maximumBatchDatagrams 取前缀，底层可能再附加 RTCP。shaping 校验 batch.maximumDatagrams≤backlog.maximumDatagrams，但该事实本身未覆盖额外 RTCP，也不能约束通用 materializeBatch 入口。线程执行器为 orderedRuntimeNodes 中各 node 创建 worker，worker 串行调用该 node.process；下一步必须结合 plan/node 唯一性及 generation 切换证明同一 global state 的实际生产者数量，而不能从 wakeup 注册数直接推断。

本次没有修改生产代码或重跑相同测试；r243原规格运行证据仍是最新实流结果，完整几何尚未证明。
## 实际输出拓扑核验

- `MediaDatagramOutputExecutionSegmentBuilder.cpp:19–37` 拒绝重复 session authority，每个完整输出创建一份 transport source 与一个公共 sender（:58–66）。
- MPEG-TS builder :288–303 仅把该 transport source 连接到一个 materializer；该节点调用 UDP/RTP 的 materializeProtocolBatch。
- 原生 RTP A/V builder :198–216 对 video、audio 两个节点分别连接 transport plan 和 wire batch，因此同一 global state 存在两个生产者。不能用“sender只有一个”推出“生产者只有一个”。
- transport template 拒绝重复 endpoint ID/role，但角色唯一性不等价于 reserve 调用权限；现有通用 reserve API 仍不能验证实际调用者身份。
- RTP materializer 的 maximumBatchDatagrams 返回媒体批次上限，materializeBatchReserved 另按 Sender Report 决策追加 entry。仅证明 batch≤backlog 不能证明 batch+RTCP≤backlog。

后续内部契约需要由 planner 表达生产者身份及其允许的协议请求几何，并在运行入口校验；不得新增调用方需推导的并发数或内部容量字段。当前仅为源码证据，不宣称新契约已实现，也不把输出格式差异扩成独立媒体链路。

## 原子事务与发送批次进一步核验

当前 planner 并非完全遗漏 RTCP：`MediaRealtimeDeploymentPlanner.cpp:520–557` 按实际 rtcpStreams 将 compound report 的字节数和报文数纳入 maximumAtomic 产品；`MediaRealtimeNetworkResourceLedgerPlanner.cpp:78–86` 使 backlog 不小于该原子事务。不得据“batch 可能追加一个 RTCP”直接宣称真实链路一定超限。

另一方面，发送批次与申请事务确实不是同一几何：原生 RTP 的 `MediaRtpDatagramMaterializerNode.cpp:403–432` 对整个 AU 创建 views 后一次 materializeBatch，随后才分成发送 partitions；MPEG-TS 路径先取 batch 前缀。新契约必须分别表达协议事务上界和发送批次上界，不能统一把输入 span 限制为 batch.maximumDatagrams，否则会拒绝现有合法整帧事务。

运行时 `MediaRtpWireCommitState.cpp:39–45` 将所有容量不足返回 false；materializer :249–261 将其记为 blocked demand 并返回 WouldBlock；commit :193–198 仅在该 demand 可被满足后唤醒。对大于永久上限的通用请求，该唤醒条件永远不能成立。这是入口失败语义的源码缺口，尚未证明当前真实源经 planner 可触发，也不是 r239 已定位的 serial-only 拒绝原因。后续须先证明真实生产者允许的原子几何，再区分非法永久需求和暂时背压，不得通过缩小源或丢包规避。
最终 transport 的权威调用链另经 `MediaRealtimeDatagramTransportPlanner.cpp:62/91/107` 调用 `MediaWireTrafficEnvelopePlanner::plan`，不能只引用部署初算 conservativeWire。该 planner 的 addRtcp :229–231 将 atomicDiscreteDatagrams 加一，原生 RTP 各流 :574–607 与 MPEG-TS/RTP :633–640 均调用，finish :310–326 将其纳入最终 maximumAtomicDatagrams。两条规划阶段均有 RTCP 计入证据；后续判断必须以最终 transport 产品为准。