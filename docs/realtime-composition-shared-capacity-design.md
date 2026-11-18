# 非固定帧率合屏共享容量调查（r230）

## 判定

当前不能解除 SourcePresentation 合屏门禁。两名独立只读审查分别核验共享预算/固定队列和解码帧收费边界；以下是源码调查结果，不是已实现的容量方案或实流验收。完整目标保持75/FAIL。

## 成熟机制与适用边界

[Flink网络缓冲生命周期](https://nightlies.apache.org/flink/flink-docs-master/docs/deployment/memory/network_mem_tuning/#network-buffer-lifecycle)区分必需缓冲与可选缓冲；必需资源拿不到会失败，可选资源不足影响吞吐。其输出池共享缓冲，并单独处理完成一次操作所需的额外缓冲。[官方网络栈说明](https://flink.apache.org/2019/06/05/a-deep-dive-into-flinks-network-stack/)说明独占缓冲、共享浮动缓冲与接收端credit的配合。

可复用的是“先证明必需工作集、共享剩余资源、额度随所有权归还”的机制，不能照抄其默认数量或将网络缓冲进展证明当作解码/启动整批发布证明。UDP源也不能依赖接收端credit让发送方停发。GStreamer multiqueue文档本轮访问失败，未将其作为本轮已核实依据。

## 当前代码事实

| 对象 | 已有事实 | 不能据此推导 |
|---|---|---|
| MediaPreparationStorageBudget | reserve在root互斥锁内原子累计父子实际占用；lease析构释放 | create子预算不预留额度；snapshot剩余量不是任何源的保证额度 |
| MediaPreparationMemoryResource | allocationFootprint与实际allocate共用布局计算，含owner/lease头和对齐；先reserve再分配，释放内存后归还 | PMR只覆盖经过它的分配，不涵盖另一个allocator或GPU像素 |
| MediaPreparedRtpSourceSessionPlan | 非cadence仅以sharedCapacityBytes/sizeof(MediaBufferRef)限制引用数量，AU分配实际竞争共享预算 | 不能直接拿这一宽泛数量界预分配每条ring，也不能视作每源独享额度 |
| MediaBlockingQueueStorage / MediaSpscRingQueue | AtomicPrepared与SPSC按capacity立即建立固定ring | 按需deque不自动满足既有原子准备/无失败发布及最终固定存储准入条件 |
| VideoDecodeNode::receiveFrames | 像素先reservePayload，实际footprint结算并attach至输出；source PMR创建包装及display lineage | 生产像素不会因进入startup store而自动计入preparation root |
| MediaVideoDecoderPreparationStoragePlanner | 软件首帧有softwareFrameBytes；硬件为0，产品服务解码探测准备 | 软件首帧费用不涵盖生产启动帧集合；logical geometry不是实际VRAM证明 |
| MediaRealtimeCompositionSourceResourcesPlanner | 已有receive/pending、filter持有、codec输入和atomic completion事实；图引用上界已复用 | 这些进展下限不能自行变成无限速输入在等待窗口内的帧数量上界 |

关键数学边界：若要由字节预算B推导最多floor(B/c)个持有帧，必须证明每个持有帧始终占用该预算至少c字节。单帧最大允许footprint不是最小收费额；当前准备预算甚至不支付生产像素，故直接相除FAIL。增加固定逻辑额度也必须显式定义范围、拥有者和归还时刻，不能伪称实际GPU分配量。

## 实施依赖顺序

1. 先确定唯一的decoded startup资源收费契约：与压缩AU acquisition分开；明确采用哪一既有预算、固定额度或权威实际费用、像素与包装各自所属范围。尚未选择或实施新收费方式。
2. 在会话级汇集所有源的必需工作集和固定存储。不得按源均分，也不得让各源重复承诺同一份headroom。若增加保证额度，必须同时实现allocator消费预留额度而非再次向root收费；孤立的reserveRemaining helper不能解决问题。
3. 从上述事实形成有限数量与批次产品，联合核算ring、prepared push副本、节点pending、启动批次和同时保留别名。先用整个预算算帧数、再追加所有ring会超额；最终payload quota也不能反推自己依赖的队列容量。
4. 将同一产品接入startup policy/store、runtime queue和最终source quota；压缩AU retention不能悄悄改成decoded frame retention。现有makePolicy仍使用IntervalCoverage，display-point路径需要正确的readiness产品。
5. 消费r229解码输出隔离证据并分别验证表面进展、预算不足、purge/重入/取消的释放。最后才解除SourcePlanner的cadence和continuousRetention门禁。

保持PerNodeWorker线程模型及既有DAG节点组合，不新增平台专属媒体链路；仅后端能力adapter可不同。分配失败必须沿现有错误通路可见，控制终止不得依赖已经耗尽的媒体预算。共享逻辑改变后先Windows同规格realtime复验，再RKMPP。

## 本轮边界

两名独立审查对最终调查记录均明确限定PASS，仅认可事实与边界，不代表实施设计或容量验收通过。UTF-8无BOM、CRLF及计划diff检查通过。

本轮新增上述调查及实施依赖记录，未修改生产代码、未构建、未启动媒体测试，无本轮抓包/截图/日志或测试进程需要清理。r229构建成功尚没有对应版本实流验收；不得沿用r227记录宣称新版本运行通过。后续画面取证只使用VLC原生snapshot。
