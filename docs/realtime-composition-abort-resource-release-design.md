# 共享DAG取消时的资源释放设计

状态：只读调查与候选设计；不修改当前核心，不影响rk485实流。本节不构成实现、双审或运行通过。

## 问题与现有证据

- `MediaGraphThreadedExecutor::abort`先requestStop/worker.abort，再abortChannels，随后逐worker join，最后scheduler.abort。首错supervisor当前只调用requestStop。
- `MediaChannel::abort`在mutation锁内reset terminal引用；Blocking/SpscRing队列abort只标志关闭并唤醒，未释放队列缓存。channel.clear与queue.clear是另外的操作，已有terminal锁内析构也须审查回入风险。
- `MediaGraphWorker::run`已有RAII OwnerRelease：任何退出路径先调用本node的finishExecution，再退出线程注册，最后发布exit token。基础MediaRuntimeNode::finishExecution为空；FFmpegNodeRuntime的pendingTransfer也在初始化、取消传输及正常提交路径reset，但基础finishExecution不清理它，异常退出时遗留的pending transfer可能持续到后续stop/abort。sender与audio drift已有真实owner退出清理，可作为复用依据。
- scheduler.abort在join之后反向调用node.abort。因此下游worker已退出却未释放node持有引用时，上游硬件处理仍可能等待其释放；控制线程又等待上游join，构成可达等待环。栈中decode停滞与此机制相容，但栈本身不能证明具体最后一份硬件引用所在。

## 工业依据与适用边界

[GStreamer事件设计](https://gstreamer.freedesktop.org/documentation/additional/design/events.html)的FLUSH_START先拒绝新buffer、向下游传播取消、释放持有数据并解除等待；不先获取可能被阻塞stream占有的STREAM_LOCK。后续FLUSH_STOP再与stream互斥同步。[插件事件文档](https://gstreamer.freedesktop.org/documentation/plugin-development/advanced/events.html)说明queue在flush时清缓存，使上游返回FLUSHING。

复用的是“先取消准入并释放阻塞资源，再等待执行结束”的顺序。这里是最终abort，不引入FLUSH_STOP或重启；不把正常EOF/BYE drain改成丢弃。GStreamer规则不授权其他线程调用avcodec_flush_buffers，也不能证明目标RKMPP驱动一定可中断。

## 最小共享修复边界

1. **通道取消与缓存释放。** 在现有channel mutation锁下关闭准入，原子发布aborted并唤醒生产者/消费者/事务等待；已授权但未提交的事务必须沿原取消路径失效。随后释放所有队列buffer和terminal引用，同步扣回queuedPayloadBytes及对象账本。不能只把计数归零。Blocking和Spsc实现均已有锁保护，仍须核tryPop及reserved push在abort后不再交付旧项。
2. **析构不持全局锁。** 不直接在channel/queue锁内销毁可能触发payload归还、AVBuffer释放或平台回调的最后一份引用。沿现有固定存储逐项移动到一个栈上MediaBufferRef，在锁内移除并记账，解锁后释放，再取下一项；不分配第二队列、不复制媒体。整个过程准入保持关闭。若复用现有clear，必须先证明其锁内析构没有回入/等待风险，否则不能直接拼接abort+clear。
3. **owner退出先释放自身持有媒体。** 扩展既有finishExecution合同，不新增平行线程或清理回调框架：在原worker线程、process已返回之后，清理pending transfer、候选帧、滤镜/重采样输出、lineage及其他retained媒体引用，再发布退出事实。以FFmpegNodeRuntime公共pending为基础，逐个实际持有者复用已有reset helper；不能只覆盖decoder或aggregate。已正常Finished但仍存在于图中的node也必须在其既有退出hook中结算自身持有引用；不能等到未来abort才要求一个已经退出的worker执行清理。已提交到channel的buffer由通道拥有，不受node本地清理影响。
4. **codec与平台owner隔离。** 执行中的codec不允许控制线程flush/reset/free。codec收尾只能在本owner退出路径、确认process不再使用后执行，并与现有scheduler stop/abort形成幂等的终止状态；共享codec metadata引用仍按RAII生命周期释放。尚未进入worker或准备失败的node继续由原scheduler清理。不得为了提前回收而把所有node.abort移动到join之前。
5. **协调顺序。** 首个失败先记录不可替换的ErrorInfo及node/phase；协调器发布全部stop和准入取消，解除payload/clock/socket等待，完成通道缓存释放；各worker在自己的退出hook释放holder；然后join；最后scheduler回收剩余非运行资源。取消请求必须能从现有首错supervisor路径触发同一阶段，不能只在显式executor.abort触发。协调回调不能持workersMutex/channel锁执行buffer析构、join或调用codec。

共享Windows/Linux执行相同所有权协议；仅socket取消、硬件driver或平台库adapter可有差别。若某平台只允许在特定线程释放最后硬件帧引用，必须由权威API资料和实际adapter实现证明，并在该adapter内处理，不能新增独立媒体链路。

## 约束、观测与验收

队列容量和payload预算均不增加，逐项释放的工作量受原队列容量约束；控制器只取消准入/唤醒，worker保留node唯一执行权。首错保持primary，清理失败作为secondary并继续清理其他owner，不用Cancelled覆盖原始失败，不把强制终止记为正常结束。

增加有界摘要即可：取消起点/首错identity、每channel取消前后对象与字节、每node finishExecution开始/完成及held计数、join等待的node、最终ledger余额；无需逐帧日志。当前退出hook错误已有Release phase记录，应复用。

修复需先完成实际holder清单及双独立审查，再用原规格真实CLI失败路径验证：首错未丢失、queued与node-held引用实际归还、所有worker自然退出、无残留receiver/driver任务。保留正常BYE排空验证，不能以取消丢弃路径替代。Windows共享影响须按用户授权安排同规格验证。

## 不能仅由当前栈确定

- decode究竟等待硬件surface、driver内部任务、codec内部锁还是其他同步；最后引用是否在queue、退出node、codec内部、外部播放器或平台池。
- AVBuffer最后unref是否可从控制线程执行、是否同步阻塞或回入ledger；需核实际FFmpeg/RKMPP版本与adapter。
- 已发布格式/codec metadata的共享引用是否需要延迟销毁，以及各派生finishExecution是否会调用公共清理。
- 正常Finished的各node是否仍有语义上需要保留的统计/描述符；清媒体与保存诊断事实必须分别核验。
- abort与atomic/reserved输出提交的线性化及所有队列消费入口是否一致。已有mutex不自动证明整个取消事务正确。

未证明上述条件前，不宣称该候选设计已消除所有硬件挂起。
