# 编码器身份与 sender 等待观测

状态：r351 批量真实达到 16 报，但链路仍失败；本单元仅增加固定诊断，已实现冻结待两名独立代码审查，尚未构建或运行验证，不改变期限、批量、SR、调度策略或失败处理。

## 编码器身份

依据 [FFmpeg send/receive](https://ffmpeg.org/doxygen/trunk/group__lavc__encdec.html) 的异步解耦与 [COPY_OPAQUE](https://ffmpeg.org/doxygen/trunk/group__lavc__core.html) 的 M:N 关联语义，不使用最后一次 send、PTS 相等或一帧一包假设。成功 send 才在既有 bounded registry entry 按 token 保存原始 PTS/timebase 与成功次数；receive 在原 registry 锁下复制该 token 事实并增加输出序号，在 opaque_unref 前保留 packet 实际 opaque 与选中 token、PTS/DTS/size。非 COPY_OPAQUE 明确标为 submission-order，不冒充 packet 自带身份。

固定不可变输出快照沿现有 receive observation、canonical、scheduled 参数显式复制、adapter trace 到 mux 当前/前一个 AU 的失败或 abort 摘要。AV scheduled builder 会剥 canonical，必须显式传递。缺失或计数溢出只标 incomplete/unavailable，不改变媒体结果，不新建 map、历史队列或逐包日志。一个 token 多个输出只能证明 opaque 关联，不能证明 packet 不含其他输入。

registry entry 的 allocator 实际 sizeof 随字段增长，entry 数仍受既有 capacity 与 lease 管理。运行阶段 MediaVideoLineageStagePreparation 使用 MediaRuntimeLineageAllocation→new_delete_resource，未计入 Preparation/Network 硬预算；只有 source probe 的 prepared allocation 使用共享准备预算实际计费。未发现旧固定 Entry sizeof 上界，本轮不扩展重构这个既有 runtime 控制对象预算缺口。canonical/scheduled wrapper 当前 new 分配，新增固定快照不代表全量 wrapper 硬预算已经闭合。节点持有的固定诊断不保留媒体或 opaque lease。锁序仍为节点 lineage→registry，不反向。

## 等待观测

复用唯一 MediaNodeWakeup::wait，增加内部可空 observation 指针，非 sender 调用显式空。栈 owned 记录 entered/returned steady；Windows 记录 arm before/after、wait API before/returned、实际负 100ns due 与原 DWORD，其他平台这些字段 unavailable。RAII 在所有 return 记录退出时间；Windows API 失败立即捕获 GetLastError 后才读取时钟，保留原错误。依据 [SetWaitableTimerEx](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-setwaitabletimerex) 的相对 100ns due 与 [WaitForMultipleObjects](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitformultipleobjects) 返回值区分 API 结果和最终 WaitOutcome，Notified 不等价事件唤醒。API before→returned 是系统等待调用墙钟时间，包含等待和线程恢复调度，没有 ETW/内核证据时不能拆为纯定时器或调度延迟。

sender 以原目标、已有 master now 的配对 steady、原 timeout 开始 attempt，以下一次已有 master now 完成；不额外读取 master 时钟改变状态机。每 reservation/batch 固定 count/sum/max 和最坏样本，失败/结束复用现有日志；未完成、倒序、溢出只 incomplete。不存在新等待策略、队列或历史；堆预算不增加，固定节点/栈大小如实增长，不能据此宣称全量节点对象预算闭合。


## 实施冻结与证据边界

编码器范围：新增 diagnostics/MediaCodecTokenObservation.h；修改 MediaCodecReceiveObservation.h、MediaAccessUnitProcessingTrace.h、sync/lineage/MediaCodecLineageRegistry.h/.cpp、nodes/video/VideoEncodeNode.cpp 以及 Decode/Filter 的 resolve 调用（显式无观测）；sync/MediaScheduledAccessUnit.h/.cpp、nodes/sync/MediaAvScheduledOutputBuilder.cpp、MediaVideoOutputSchedulerNode.cpp、nodes/output/MediaScheduledTsAccessUnitAdapterNode.cpp。token identity 还带 encoder node，以免不同 registry 的本地 identifier 混淆；缺失 copyOpaque 配置输出 unavailable。原 opaque 释放和 lineage 生命周期未变。

等待范围：新增 diagnostics/MediaNodeWaitObservation.h、MediaDatagramWaitObservation.h；修改 runtime/threading/MediaNodeWakeup.h/.cpp、MediaGraphWorker.cpp 的三个显式空观测调用、nodes/output/MediaScheduledDatagramSenderNode.cpp、diagnostics/MediaDatagramReservationObservation.h、MediaDatagramBatchObservation.h。sample 在原 master 读取之后配对 steady，下一次已有 master 读取完成；退出前尚无下一次读则固定汇总 incomplete，不新增 master 读。最坏样本按 wait 调用墙钟区间选择，原 DWORD 与最终 outcome 单独输出；各阶段 count/total/max 不等价根因归属。

新文件均 header-only，无新增 cpp 或构建机制。编码器快照与等待汇总不产生新媒体保留、map、队列或逐包日志；新增固定结构使原节点/对象 sizeof 增长，运行 lineage 与 wrapper 既有预算缺口保持明确。已核对接口调用与文件格式，尚无运行证据，不据此认定 r351 根因已修复。

## 本轮审查修正

审查 A 指出缺少 Notified 次数汇总，原实现不能从最坏单次样本判断多次唤醒。已补齐每 reservation/batch 的 Notified、Deadline、Interrupted、unavailable 固定计数，溢出保留最大值并标 incomplete。Notified 是 wait 最终结果，不等价 Windows event，也不保证随后实际执行了下一次等待。两名独立审查均因服务额度限制终止，尚未取得双 PASS；本修正仍待双重复审，构建和运行不得代替该门禁。

## 最新双审结论

本轮重新请求后，crash_release_audit 与 publication_identity_review_b 均已独立完成源码复审并明确 PASS，覆盖滤镜接受状态及token/wait诊断（含Notified计数修正）。此前服务额度终止是历史状态，不再是当前审查阻塞。源码PASS不等于运行修复，Release重建及r353验证仍待完成；全量预算和完整合屏门禁仍未闭合。
