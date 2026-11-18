# 合屏准备期协议接管顺序修订

状态：2026-11-02，已实施并经双独立局部审查，Release778全量构建通过；r374部分运行行为已验证，完整验收仍未通过。对应 [r373 失败](realtime-composition-r373-validation.md) 与[完整存储准入计划](realtime-composition-full-storage-admission-plan.md)。不新增公共参数，不修改外部 FFmpeg。

## 问题与证据

r373 中 source0 打开 CUDA decoder 后等待首帧，其余 raw 接收器约四秒后记录 `capacity_stop`；source1 随后安装 continuous owner 被拒绝，最后 QSV retention 错误覆盖了最终提示。说明串行“每源 open → install → 首帧/滤镜”让后序源停留在有限 raw 捕获阶段。不能把 capacity_stop 解释成所有有限 replay 都应失败，也不能增大缓冲或放宽期限代替生命周期修复。

修订此前“先打开 decoder，取得 retention，再安装 session”的顺序：**每源真实协议事实就绪后，由原有该源 worker 立即安装 continuous session；全部 worker 完成后，按原排名串行进行 decoder open/readback/probe/tile。** 不并行调用设备或 codec adapter。

## 工业依据与适用边界

[GStreamer bufferpool](https://gstreamer.freedesktop.org/documentation/additional/design/bufferpool.html) 将分配协商、池激活及旧引用排空分开；有限池耗尽必须阻塞或明确失败。[FFmpeg send/receive](https://ffmpeg.org/doxygen/trunk/group__lavc__encdec.html) 明确每输入可产生零到多输出，启动可以接受多个输入才返回首帧。因此协议收包所有权不能以某一路首帧返回作为所有其他源的前置条件。

本修订只借用阶段分离和有限所有权原则，不声称两个系统提供了本项目的预算公式。bootstrap 仍不是完整共享存储准入；所有源在原 deadline 和 root 硬预算内可能明确失败，不保证任意码流、源等待或驱动耗时都能启动。

## 实施位置、线程与锁

1. `MediaRealtimeCompositionInputPreparer::prepare` 已有每源一个有界 `jthread`。在该 worker 的 observation 和 `SourceFactsPlanner::plan` 成功后调用原 `capture.installContinuousSession(facts, masterClock)`，再将 capture、session、facts 一并放入 `Captured` 返回；`sessionVideoPlan` 此时为空。所有 futures 收集、取消传播、join 和错误优先级保留。
2. 不采用收集所有 futures 后再串行 install：该方式仍让已就绪源等待最慢源的 observation。安装使用该源独有的原 video/audio receiver mutex；协议对象和 prefix 均不跨源共享。
3. `MediaStorageBudget::reserve/release/snapshot` 使用共同 `ledgerMutex()`，root 和直接 child 在同一临界区更新。`release` 在解锁后通知；安装路径仍保留 receiver → budget 的现有锁方向。`MediaSteadyMasterClock::now()` 只读构造时锚点并采样 steady clock；不会新增共享可变时钟状态。现有 worker 数量和收包线程数量不增加。
4. 唯一接管仍为原 `Preflight::installContinuousSession`：双接收锁内校验原 owner/frontier/预算，读取 prefix 与剩余 deadline，创建协议 owner，按 observedAt 合并重放，`finishBootstrap` 成功后才清理旧 prefix 并同时发布两侧 owner。禁止拆成两次发布、重开 socket、丢弃 prefix 或重置开始时间。

## Session 与 decoder 候选是两个独立状态

- 协议已接管、`sessionVideoPlan` 未绑定是合法状态，不以 `session != nullptr` 推断 codec 已承诺。SourcePreparer 必须要求协议 session 存在，并保持原 descriptor、实际设备、decoder open 与 retention readback。
- 首次成功获得权威 decoder retention 后，先构造完整候选元数据副本，再移动发布为 `sessionVideoPlan`。复制失败不得留下半份契约。随后仍使用刚打开的同一 decoder 进行原 probe，不再创建或安装 session。
- 该候选承诺一旦发布即不回滚，包括后续 storage、probe、allocation/readiness 或 filter 失败。新候选只能按原比较项（decoder 名称、hwaccel、retention、outputFrame、lineage、filter 配置、transfer）验证一致后重试；不同契约明确拒绝，不通过重建 session 恢复候选自由度。
- 失败候选的 decoder、tile、readiness 和 reservation 由原 RAII 销毁；capture 继续持有唯一 session 与完整 AU prefix。`probeSnapshot` 只建立引用，不能消耗 prefix。NeedMoreEvidence 仍沿现有 `restartDecoder` 丢弃已提交探测状态并复用预约；成功后沿既有同 owner 交接。
- 输出/source 排名及串行候选循环不变。首次 decoder open/readback 失败尚未绑定，可继续下一候选；绑定后失败必须满足上述重用门禁。

## 费用并存和失败不可重用契约

安装重放时，旧 raw datagrams 与新协议/AU/控制对象同时存在，全部继续向原 root 收费；只有重放成功并清理旧 prefix 后才归还旧收费。每源 child 的容量不是额外预算，所有源仍共享同一个 B。提前安装不允许提前减记 raw 费用，也不代表输出编码器、画布和完整 G 已经计入联合求解。

原始接收错误、取消、原 deadline 到期、continuous 所需 raw 容量终止、session 错误或 frozen/已交接状态均不可作为可重试 capability failure。须用类型化状态查询保留失败原因，不能解析错误字符串；raw 结束原因要区分有限 replay 可用的 sealed prefix 和 continuous 安装要求的 still-receiving 状态，不全局改变 replay 语义。

Source 与 output 两层候选循环在失败后、继续下一候选前检查**所有输入源**是否仍可用于当前准备事务；不能只检查当前源，或让后续 Unsupported 覆盖其他源已发生的终态原因。取消/截止仍检查原 `MediaPreparationControl`。状态查询须在各源现有锁下读取，不复制媒体、不恢复终态；若多个源失败，保留原始非取消错误及源索引的诊断，不用清理导致的取消覆盖首因。

## 验证与完成边界

冻结后双独立审查，Release 全量构建，再以原规格 Windows realtime 三源真实链路复验。记录每源协议安装、decoder 绑定顺序和首个错误，保留全部非周期诊断与命令后再清理临时文件。需覆盖正常输出、失源/重入、取消和失败清理；构建或启动成功不代表完整验收。

本单元只处理准备期收包所有权和候选失败语义；SourcePresentation 联合容量求解、完整早期拓扑、发送期限问题及 RKMPP 验收仍独立未完成。
