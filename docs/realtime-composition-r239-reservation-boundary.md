# r239 后续：多 reservation 安全边界核查

## 当前结论

r239 最后一次拒绝是 serial=1、count=0、bytes=0。sender 仅一个 active batch 不等于只能预备一个 batch；但现有证据不足以直接取消 planner 串行门控。本次未修改生产行为，也未新增实流通过结论。

## 已核验的调用链

- `MediaDatagramTransportPlan.cpp:255` 将 commitBeforeNextReservation 设为 true；sender 的 enqueueWireBatch 已支持有界、有序队列，activateNextWireBatch 只激活队首。
- `MediaWireGlobalSequenceState.cpp:244–266` 随提交前缀归还待发送包数和 wire 字节；reservation 自身的 wireBytes/materializedAt 数组不会随前缀缩小。这是发送 backlog，不是全部存活存储的账本。
- `MediaRtpWireDatagramMaterializer.cpp:245–369` 先检查协议包数额度，再取得全局 reservation，保留完整 actions、嵌套 TS transaction，并推进 projected RTP/RTCP 状态。
- `MediaRtpWireCommitState.cpp:184–201` 整批完成才归还协议 outstandingDatagrams。完整 actions 在事务完成前仍存活；不能仅为了增加并行度把该计数改成逐前缀归还。
- `MediaWireDatagramBatchBuilder.cpp` 复制载荷进入批次 vector。部分发送并不立即释放已发送前缀的底层载荷存储。
- `MediaRealtimeNetworkResourceLedgerPlanner.cpp:114–164` 当前网络存储公式列出 wire payload、datagram/shared_ptr、batch descriptor、endpoint pointer 与 evidence；本段公式未显式列出 RTP actions、全局 reservation 数组及事务对象。需要继续核对其他账本是否覆盖这些存储，不能将 wire 上限当成完整内存上限。

## 失败与换代语义

独立只读审查确认：正常连续前缀提交未发现单 reservation 限制；未完成事务单独析构会 poison，不支持撤销中间投影后继续同代。全局 purge 是整代取消。RTCP BYE 使用 projected counters 并排在所有已投影媒体之后。

保留该失败语义，不新增 rollback。下一步必须核对换代重建私有 RTP/TS 状态、完整存储所有权与现有预算覆盖，再决定 planner 是否可以允许有界预备。仅有队列结构或解除门控后的样例通过，都不能证明安全。

## 未完成门禁

- active、queued、channel pending、materializer workspace 及完整事务存储的同时持有上限。
- 多 reservation 重叠存活下的原规格实流、顺序、截止期及清理证据。
- 原合屏计划的非 cadence 输入、四源、内容 A/V、Windows 完整验收及其后的 RKMPP；均未因本次核查变为通过。
## 补充核查：预算绑定与换代（2026-10-16）

两名未参与本次实现的独立审查分别完成以下检查：

- 换代对象隔离：限定 PASS。`MediaProtocolOutputGenerationState.cpp:243–249` prepare/reset；`MediaProtocolOutputGenerationData.h:16–25` 替换新 Data；新 materializer 调用 `MediaRtpWireDatagramMaterializer::create` 新建协议状态。TS adapter abort/reset 后重新 create/start session，新 packetizer control 与旧 cursor owner 分离；transport buffer 新建 global ledger。此结论不是换代实流通过，未授权 poison 后的恢复仍不支持。
- 完整存活存储计费：FAIL。网络 ledger 未发现其他账本补齐 actions、reservation 数组、控制块及构建 workspace；普通 vector 未绑定存储 lease。batch 的 payloadFootprintBytes 仅返回 size，不含 capacity 和事务元数据。
- 主智能体追踪确认：`MediaRealtimeDeploymentPlanner.cpp:788–805` 用 ledger 总和派生网络预算；`MediaDatagramTransportPlan.cpp:259–262` 将该和传入 networkMemory；`MediaDatagramShapingPlan.cpp:230–237` 仅检查这组数值相容。该检查不能证明实际分配已受约束。transmit session 另检查 socket 实际额度，不能覆盖用户态遗漏。

### 实施方向及复用边界

[Flink 官方网络栈说明](https://flink.apache.org/2019/06/05/a-deep-dive-into-flinks-network-stack/)把有限 buffer pool 与网络 credit 区分，并在 buffer 不可用时背压。适用原则是按实际存活 buffer 保持额度；其接收端 TCP credit 不直接证明本项目 UDP/RTP deadline 与事务正确性。

仓库已有 `MediaByteReservationCounter`、RAII storage lease 和 `MediaPreparationMemoryResource` 的分配前计费/释放后归还机制。实现时复用其通用计费算法和所有权机制，不复制第二套同义 allocator；准备预算与网络运行存储预算必须保留不同的 typed scope，不将网络存储挪入用户的准备预算。

后续修改顺序：

1. planner 为运行网络存储形成独立内部契约，覆盖同时持有的构建 workspace、完整批次与事务元数据，按真实类型布局及分配容量推导；不新增公共参数，不用固定倍数估算。
2. 把分配额度绑定实际 owner，覆盖 vector 扩容时新旧块重叠及 shared_ptr 控制块寿命；在形成协议投影前取得足够信用。发送前缀归还 wire credit，不归还仍存活的存储信用。
3. 保留单 sender、全局连续提交及未授权取消终止整代的语义；只有两类额度均可证明有界后才删除 planner 的串行准入限制。
4. 两名独立智能体复审后，完整 Release 重建，执行原规格 realtime 链路、VLC status+snapshot、失源和重入；记录后按清单清理。Windows 门禁通过前不开展 RKMPP 验收。

本次仅核查及收敛设计，没有改串行开关，没有运行新测试，也没有将 r239 FAIL 改为通过。
