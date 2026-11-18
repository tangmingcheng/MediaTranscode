# 合屏 worker 调度准备与启动方案

日期：2026-11-04。状态：源码已接通，等待双独立审查、Release 全量构建及真实链路验证；不代表调度失败已修复或合屏验收完成。

## 问题与范围

r378 的46次等待累计迟到55.115732 ms，单次最大约3.405 ms，随后预约窗口倒置151024 ns；当前严格逐包服务间隔会保留累计延迟。保留原 GCRA、100 ms 总预算、burst、源规格及失败语义；不采用忙等、经验提前量、任意线程优先级或外部 FFmpeg 修复。

[本机能力证据](realtime-composition-windows-scheduling-evidence.md)已确认非管理员启用令牌真实注册 Distribution 成功，并在同线程撤销、join。它不保证生产注册成功或215微秒期限，ETW权限也不作为本方案执行前置条件。

## 成熟实现及适用边界

[GStreamer线程模型](https://gstreamer.freedesktop.org/documentation/application-development/advanced/threads.html)将线程创建配置与进入/离开回调分开；其实际[gst_task_func源码](https://github.com/GStreamer/gstreamer/blob/main/subprojects/gstreamer/gst/gsttask.c)在任务函数前执行enter回调，离开回调在发布running=false和唤醒join之前执行，且调用leave时释放对象锁。采用其“owner线程进入/离开、释放后发布退出、锁外回调”的生命周期原则。GstTask本身不证明Distribution注册或整图原子启动；本项目的全部worker准备门禁用于既有静态executor与动态branch，不能声称照搬了GStreamer整图屏障。独立worker继续保留，避免让互相依赖的阻塞节点共享有限任务池；[GstSharedTaskPool官方说明](https://gstreamer.freedesktop.org/documentation/gstreamer/gsttaskpool.html)明确提示这类死锁风险。

[Microsoft注册接口](https://learn.microsoft.com/en-us/windows/win32/api/avrt/nf-avrt-avsetmmthreadcharacteristicsw)要求任务名来自既有SystemProfile任务，首次index=0；[撤销接口](https://learn.microsoft.com/en-us/windows/win32/api/avrt/nf-avrt-avrevertmmthreadcharacteristics)要求同一线程。以本机Distribution能力事实实现适配，不复制其他系统的优先级常量。[MMCSS说明](https://learn.microsoft.com/en-us/windows/win32/procthread/multimedia-class-scheduler-service)仅支持CPU优先访问，Windows 7起取消Clock Rate保证；本机Clock Rate=10000不是硬期限保证。本次未取得可核验的GStreamer Windows MMCSS专用实现，因此不声称GStreamer已证明该任务选择或网络发送性能。

## 当前接点与内部产品

| 现有接点 | 变更职责 |
| --- | --- |
| `planner/MediaWorkerExecutionPlanner.cpp` 的统一 Sender 节点识别 | planner选择时间敏感分发执行角色，覆盖统一发送节点；不按输入数量或编码器另建链路 |
| `model/MediaThreadingPolicy.h` | 现有全局High只被赋值/校验，未应用OS策略；不得直接把它映射成全节点MMCSS |
| `MediaGraphExecutionContext::compile/compileSegment` | 调用执行 planner 并持有每节点固定容量契约；静态与动态路径按 nodeId 及执行顺序完整验证，禁止缺项补默认 |
| `MediaGraphWorker.{h,cpp}` | 消费执行契约、同线程注册/撤销、准备结果交接、等待放行、退出发布 |
| `MediaGraphThreadedExecutor.cpp`、`MediaRuntimeBranch.cpp` | 复用同一个启动协调组件；禁止各写一套状态机 |
| `MediaPreparedRuntimeBranch`及准备/构建传递路径 | 传递所属节点精确执行产品及启动资源预留 |
| 资源账本与构建文件 | 计入新增固定存储；Windows链接Avrt，Linux无此依赖 |

拟新增内部`MediaWorkerExecutionPlan`与平台`MediaWorkerThreadRegistration`，不增加公共API/CLI/profile字段。产品分开表达媒体执行角色与平台动作：Windows发送worker采用已探测的Distribution契约，其他worker及Linux明确采用继承现有原生调度契约；缺契约是错误，不是继承策略。task名来自已核验的既有任务标识，planner决策，节点不设优先级。

准备期能力adapter串行创建一个短生命周期探测线程，真实注册、同线程撤销后join，再允许planner产生Windows契约。服务/注册表读取只是事实预检，不替代权限探测；记录原Win32错误、任务、PID/TID。探测handle不得转移给生产worker，结果也不得永久缓存成权限保证。生产worker仍逐一真实注册，失败不降级。

## 启动、取消与释放

启动协调组件以所属executor/branch为作用域，持有固定worker数、逐worker准备结果及一个`Pending/Committed/Cancelled`门闩；它不是队列或额外线程池。

1. 准备全部worker及上下文绑定，预分配结果槽；先禁止媒体process。创建线程后，worker执行自己的注册契约，发布Ready或Preparation失败，再等待门闩。
2. 全部Ready且未取消时，启动方将门闩原子提交；此前任何worker不得调用process。线程创建异常、注册失败或外部取消均转Cancelled，唤醒所有等待者，不允许随后提交。
3. 失败保留现有`MediaGraphWorkerFailureRecorder`的首错与Preparation阶段，使用supervisor协调停止；未创建线程标记cancelledBeforeStart，已创建线程走owner清理并join。不能把准备失败记为自然Finished。
4. 运行后的失败沿用Runtime/Drain协议。每个worker退出时完成其既有finishExecution职责，再同线程撤销注册，最后发布exited/token；销毁顺序显式保证。未运行process也可能持有scheduler准备资源，必须按实际准备阶段清理，不能跳过或重复finishExecution。
5. RAII确保所有返回/异常路径撤销；显式close取得撤销失败并记Release，析构不抛异常。撤销失败不得覆盖原首错或伪报释放成功；同线程结束及join作为生命周期终点，保留释放异常证据，不跨线程重试AVRT handle。
6. 不持协调锁调用节点回调、supervisor或join。取消先发布状态并通知，再锁外中断/回收；工作线程禁止join自己。门闩等待响应取消，不新增经验超时或detach逃生路径。

当前`MediaRuntimeBranch::start()`持有publication mutex。实现必须把Starting状态发布与线程准备/等待拆开，锁外等待；回锁后核验状态再提交Running。停止/取消可以在准备期间将状态转为取消，且提交与取消共享唯一判定点。reclamation owner复用现有回收职责，不能与启动方并发清空workers；启动协调所有权释放后才能回收该容器。锁外join前必须保证没有worker等待调用方持有的锁。

## scheduler.start之前的风险

当前`MediaGraphScheduler::start()`先执行configure，再逐节点start；两处executor均在此后才创建worker。门闩本身只能挡住process，不能挡住这些回调里的发送、独立线程或设备执行。

实施前逐项核查目标图configure/start/finishExecution，记录资源创建和媒体执行边界。可保留纯资源准备在门闩前，但部分start失败必须回滚已准备集合；若start会执行媒体，必须先完成真实owner注册准备并保持gate关闭，再执行经拆分的资源准备/提交步骤。不得仅因未进process就宣称“媒体执行前失败”。不能把必须绑定owner线程的初始化搬到控制线程；最终调用顺序需由各资源真实owner契约确定并复审。

本轮已核实的回调边界：`MediaGraphScheduler::start()`按顺序调用节点start，首个失败直接返回；静态executor在该失败路径回到Idle，未在该函数内回滚已启动集合，最终清理责任仍须追踪。`RawRtpInputNode::start()`会绑定prepared源的wakeup/activity并启动purge协调；`activateStream()`本身只在锁内写入这两个绑定，不发送媒体。sender的start重置状态、启动purge和校验端口，未调用submit。这些局部证据不能替代所有节点start/finishExecution的完整审计。
## 存储、平台与验证门禁

N由已规划的节点集合确定：每worker一个固定执行契约/准备结果/注册状态，作用域一个协调对象；存储为N乘编译期结构大小加固定同步对象，纳入现有资源账本并检查算术溢出，不引入无界日志、重试队列或媒体缓存。沿用现有worker线程预算；准备探测最多一个额外线程，join后才能进入生产启动，不能与生产线程预算混计为同时运行。OS内部MMCSS资源不声称能由sizeof精确计费，运行注册失败必须传播。

Linux与Windows共用planner角色、状态机、门闩、失败与释放路径；仅Windows adapter调用AVRT，Linux显式继承原调度，不加入SCHED_FIFO或权限要求。共享启动时序变化意味着RKMPP必须回归，不能以Linux没有MMCSS为由跳过。

实施门禁：先审查上述资源回调/锁序及全部静态和动态契约传递；Release全量重建；只做realtime原规格真实FFmpeg→CLI→VLC链路，覆盖静态启动、动态分支、失源/重入、全失源持续输出、取消及退出。观察VLC快照、CPU/内存/A/V、逐batch累计lateness/等待时长、原deadline与sender wire burst；没有达到原指标不得标记修复。生产注册失败路径以真实可得失败事实和代码审查验证，不增公共失败注入参数或持久测试体系。Windows通过后再同规格RKMPP；代码冻结后两名未参与实现者独立审查。按既有要求先归档结果再清除本轮临时产物。当前源码已接通，尚未通过本轮双审、构建及真实链路门禁。


## 本轮源码收敛与待验证项

- 实际执行使用 `ExecutionContext.executionOrder`，原 `MediaRuntimeSchedulePlanner::plan` 没有生产调用，因此未把契约放在该闲置产品。`compile/compileSegment` 生成并持有执行契约；启动 gate 只核对完整同序身份和消费产品。Windows 对统一 `ScheduledDatagramSender` 做真实 Distribution 探测及 owner 注册；Linux 使用显式原生契约。没有把全局 High 映射成操作系统优先级。
- 静态 executor 和动态 branch 复用同一 gate。prepare 完成容器，scheduler 配置/启动资源，逐线程注册并 Ready，全部 Ready 后先设 Runtime 阶段再 commit。线程创建异常和注册失败取消 gate；已创建者 join，未创建者发布取消前置证据。branch 的 Starting 事务持有唯一回收权，等待和 join 不持 publication mutex，poll 在事务结束前不回收容器。
- scheduler 保存实际尝试 configure 的前缀（包含失败节点），abort 逆序回滚且不触及未尝试的节点；启动线程期间外层 runtime 尚为 Compiled 时，abort 依据内层 executor 状态先取消/join，然后才能清理上下文。
- worker 的 finishExecution、MMCSS 显式 leave 和 RAII 最后释放都先于 exit token。finishExecution 接口本身声明 noexcept；新增失败记录在复制文本失败时移动预留的无分配记录，保留原 code/nativeCode，不覆盖已记录首错。日志异常不阻断协调停止；leave 返回原 Win32 数值时不分配错误字符串。异常从 process 逃出也转为 worker 失败，并继续 owner 清理。
- 契约、worker owner 槽、scheduler 回滚身份复用 `MediaFixedCapacityStorage`，不依赖 vector spare capacity。root 契约按最初全图 N 元素计费，detach 只删逻辑成员，原 backing 持续计费；每个新 branch 的 N 元素契约另计，覆盖提取期并存。每个实际 worker 对象（已含其契约）只计一次；root context/scheduler/executor 固定对象另计，branch 对应对象与 gate 已含在既有 branch fixedStorageBytes 中，不重复加入。停止回调是内嵌的 owner/context 两指针与 noexcept 函数指针，已含在相应 controller sizeof 中，无函数闭包或共享控制块分配；原生线程栈、MMCSS 内部资源和分配器控制块仍明确不宣称 sizeof 精确覆盖。
- 已逐项读取现有节点 start：codec/filter/transfer 仅重置及校验契约；aggregate、RTP clock/input、mux/协议输出初始化或绑定 purge/wakeup；source input 激活既有准备会话；release extractor 仅获取并释放空输出事务；sender 不提交 datagram，SDP publisher 不在 start 发布媒体。因 prepared 接收会话原已运行，本门禁保证的是所属 DAG worker 在 commit 前不执行 process，不声称准备期没有协议接收活动。部分资源初始化失败继续由实际 configure 前缀回滚。
- 新 cpp 由现有 CMake `GLOB_RECURSE CONFIGURE_DEPENDS` 纳入，Windows 链接 Avrt；没有额外构建系统或外部依赖修复。本轮只做源码与编码检查，未编译、未运行媒体。仍须双审、完整 Release clean-first/all-target 构建和原规格 realtime 静态/动态启动、取消、三格/失源/重入、发送期限与 A/V 复验；Windows 通过后再验证 RKMPP。MMCSS 注册成功不代表硬实时期限保证。

- 回调借用寿命：唯一通知调用者是所属 worker。静态 stop/abort/clear 与 branch 启动失败/回收均先 join 全部已创建 worker，再 disarm、清理 owner/context；Starting 期间回调只取消 gate，启动方继续独占容器并负责 join。`disarm` 本身不充当 in-flight 屏障，真正屏障是 join；回调在 supervisor 锁外运行且不 join 自己。新描述是平凡固定值，arm 无闭包分配；线程创建前失败没有通知 worker。
- root 计数调用范围已核对：完整图和 initial shared partition 仅在初始构建编译；动态 `appendEncodingGroup` / `appendProtocolOutput` 始终传入新追加节点集合，该集合不包含原 `VideoOutputFanout`，故只收取新 branch backing，不用增长后的图重新收取 root 契约。`detachExecutionNodes` 对契约与 executionOrder 使用同一节点集合、保序删除，gate 再逐索引验证。

## 首轮双审失败与本次修正

2026-11-04 首轮独立审查为一项 PASS、一项 FAIL，整体未通过。FAIL 的 P1 是启动失败先复制 ErrorInfo/记录文本再取消与 join，gate 的异常翻译和分支 catch 也可能再次分配；std::thread 创建失败后的二次 bad_alloc 可留下 Starting、未 join worker 或未回滚 scheduler。P2 是统一转换为 InternalError 丢失 bad_alloc 类别和 system_error 原始 native code。Windows/Linux 的共享启动路径均受影响。

已在同一 gate 中加入无分配启动事务 guard：静态和 branch 都在首次状态修改前建立，所有异常及失败出口无条件取消、join、disarm、逆序回滚；branch 先释放 publication 锁再等待，最终锁内发布 Failed，最后撤销 starting。guard 所借用的锁与上下文先构造、后析构，成功 commit 后才解除回滚责任。

scope 失败记录和首错 Status 读取采用无分配保底：文本复制失败时仍保留原 code/nativeCode，已有首错不覆盖。统一异常转换将 bad_alloc 保持为 AllocationFailed，将 system_error 的 code().value() 留在 nativeCode；错误文本复制本身失败也不改变这两个事实。生产线程与临时 MMCSS 探测线程共用该转换。新增事务仅占控制线程栈，无持久分配；FailureRecorder 仅增加方法，没有新增实例字段或改变既有 sizeof 计费。

本次修正尚待原两名独立审查者重新复审，未构建、未运行媒体，原 FAIL 记录保留。

## 第二轮复审失败与回滚依赖修正

第二轮仍为一项 PASS、一项 FAIL，整体未通过。P2 的异常类别/native 保留已闭合；P1 继续发现启动事务调用的底层 noexcept 清理会分配：channel 快照 vector 在 join 前分配、sender abort 构造长错误文本，以及同链路 stop_source、RTP 取消/诊断、generation session 重建和 RKMPP encoder drain 失败处理。外层 RAII 不能修复被调用 noexcept 内部的 terminate，因此本次修到实际依赖，不以捕获后跳过取消替代。

| 已核实的依赖 | 本次修改 |
| --- | --- |
| channel close/clear/abort 及所有 registry 遍历 | 现有 channels() 直接返回借用 range，const 返回 const 指针；删除 vector 快照实现。全部调用点仅范围遍历或 size，不在遍历中删除/加入 registry，也不保存越过 owner 寿命的 view。生命周期清理先持有 owner 并等待 worker，再清除 registry。 |
| generation purge stop | 用 nostopstate 构造临时 source，再复制原 stop state，继续真实 request_stop 和通知。已直接核验本机 MSVC 14.51.36231/include/stop_token:179–185：默认构造 new 状态，nostopstate 不分配，复制 noexcept。 |
| sender、SDP publisher、activation sequencer、playback binder、project mux abort | 复用内部 MediaCleanupError，尽可能保留原文，分配失败仍保留 code/native；不覆盖原首错。sender owner 清理中的错误复制同步使用此 helper。 |
| RTP transport、prepared input、Windows ingress cancellation | closed/aborted/native 错误构造使用同一 helper；原 OS 取消、唤醒、等待 drain 和 join 顺序保留。Windows/Linux ingress 的整段诊断表达式保护，日志失败不能阻断清理。 |
| A/V scheduler session 重建失败 | make_shared 的 bad_alloc 转为安全 AllocationFailed；保留 abort 原有取消 pending transfer、释放 generationData 的失败分支。 |
| RKMPP DrainThenAbort | 仍调用原 drain；FFmpeg 失败保留原数值，错误文本分配失败保留操作名和数值。packet 分配失败保持 AllocationFailed；诊断失败不阻断 codec abort 和 lineage reset。没有取消或降低 drain 要求。 |
| 外层 runtime abort | begin/done 日志连同参数构造全部保护，清理顺序不依赖日志分配。 |

内部 helper 同时供启动 scope 错误记录和异常翻译使用，不新增公共 API、媒体策略、默认值或持久分配。此次保证范围是 commit 前的启动回滚及其实际依赖；sender 的新会话尚未 process，active transmit session 此时为空，不据此声称运行期 active-session close 的全部故障路径已审完。已有 prepared RTP 接收会话则确实可能已运行，故两平台取消链纳入本次修复。

源码范围从原 25 文件扩到 41 文件；未构建、未运行媒体，等待两名原审查者对该范围复审。原两轮 FAIL 均保留。

## 第三轮补链复审与最终增量

第三轮补链复审仍发现 P1：scheduler abort 经 group.markAborted/shutdown 调用 reacquisition coordinator.abort，其 noexcept 中构造 Cancelled 文本仍可触发 terminate。已改用同一 MediaCleanupError，原首错存在时不覆盖，随后 transition abort、phase 与 in-flight 复位保持原顺序。进一步确认 epoch transition 的 abort 无新增分配；reservation abandon 只由 process 创建的事务触发，不把它混入本轮 commit 前回滚证明。

同一 shutdown 链发现 registry.clear 的局部默认 unordered_map 也会分配：本机 MSVC `unordered_map:98` 调用 `xhash:305–311` 的 `_Hash` 构造，后者执行 `_Vec._Assign_grow`。现改为进入 clear 时读取有限 size，每次锁内移出既有 shared_ptr 并 erase 节点，锁外 shutdown；不分配临时表，不无限消费新注册条目。空表 bucket backing 保留到 registry 析构或后续复用，不宣称 clear 立即归还全部索引内存。

生命周期适用依据：唯一生产注册调用是 RuntimeCompiler:201 向本轮局部未发布 preparedContext 注册（:165 创建），旧 context 在 :282 shutdown，随后才移入新 context；运行期没有向同一 registry 注册 group 的生产入口。MediaGraphRuntime::compile 拒绝 Running/ThreadedRunning；启动失败的 worker 先 join，再由外层 shutdown。此 clear 依赖注册/生命周期串行的生产所有权，不提供通用并发 register/clear 的原子摘除承诺；group shutdown 链本身不重新查询或注册该 registry。

本次仅新增 `MediaAvReacquisitionCoordinator.cpp`、`MediaAvSyncGroupRegistry.cpp` 两个源码文件及本页修订，审查总范围为 43 文件。未构建、未运行媒体，需两位审查者确认真实调用边界及修正后再进入构建。

最终 43 文件于 2026-11-04 获原两名独立审查者 source_kernel_review_a、stage_summary_refresh 明确 PASS。复审覆盖原 P1/P2、实际启动回滚依赖、最后的 reacquisition 与 registry shutdown 生命周期前提；前述各轮 FAIL 保留。结论仅为限定源码及可达清理链审查，不代表持续 OOM 实测、构建或实流通过。接下来执行 Release 全量 clean-first/all-target 重建。

最终 43 文件双审后，Release 全量 clean-first/all-target 构建 session 83850 成功：789 目标，configure/build 退出码均为 0。该构建同时包含已双审的共同源规划 7 文件；尚未执行本版本真实媒体，不能关闭 r380 发送期限失败或完整合屏验收缺口。

r381 已完成本轮原规格三格→全失源黑屏→所选源重入→再次全黑及 caller-stop 观察：CLI退出0，四次源均完整120秒/3600帧退出0；生产MMCSS同线程enter/leave native0，最终errors/pressure/deadline_misses均0，223612预约/释放归零。sender delivery仍not_proven，单轮未复现旧错不等于稳定性已修复，更不等于完整预算或合屏验收完成。完整命令和证据见 realtime-composition-r381-validation.md；16临时产物已清理，六PID及VLC端口无残留。
