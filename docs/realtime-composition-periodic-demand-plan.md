# 合屏周期维护需求规划

状态：设计调查，尚未实施；不替代已接受工作观察或既有原期限检查。目标是消除仅观察下一PSI/PCR的遗漏，并覆盖当前已接受工作对后继维护的影响。

## 当前代码事实

- MediaTsMuxSession::maintenanceObligations固定两项；PrefixPublicationPlanner只对传入列表反推latest-start，AcceptedWorkPlanner::validateTail未插入后继维护。
- 接受范围仍为TS mux成功接收AU至sender提交，未接受的上游队列不纳入。本轮不扩张此边界。
- 当前EmissionSchedule的prefix release来自timelineAvailableAt；字节计数用于额度，不按字节插值释放时刻。maintenance可能推进timeline；下一prefix仍等待前一prefix提交，不能将全部尾部称为实际同时就绪。
- 生产周期唯一来自MediaMpegTsOutputTimingPlanner，经视频/AV policy planner进入图；codec恢复产品，最终shape validator核对validateAgainstPlanner。底层TimingPolicy::create的可构造范围不是新的公共周期入口。
- 成功生产规划中PCR=frameNs×floor(99999999/frameNs)，范围[50000000,99999999]ns；PSI=100000000ns。该事实属于现有生产支持域，算法应消费实际period，不另写经验下界。
- beginAccessUnit要求dispatch−emit等于计划accessUnitWindow，原deadline为dispatch。该窗口来自sender transport lead；不能直接替换成maximumWireResidence。

## 下一产品与门禁

复用现有nextPsi/nextPcr、实际period、每次原deadline规则、PAT/PMT/PCR包数、materializer批次界与SR allowance形成类型化周期描述。采用常量存储的周期游标或闭式需求，不建立增长的事件数组；两处消费者必须共同使用，不增加无消费者字段。

有限AU时窗内的实例数可由原deadline与实际period整数计算，含边界/相位/同刻PSI先于PCR；该计数本身不证明尾部与维护的交错执行，也不证明AU完成后维护积压已经清空。完整分析须证明有限carry-out边界，保留已发布工作、全部尾部和候选互斥，保留各自原期限。

实际调度存在单数据报不可抢占和维护提前让行。不能未经等价证明套固定优先级或work-conserving响应时间公式。特别需要界定主动空隙造成的服务损失，以及何时达到不再受当前AU影响的队列空边界。禁止用固定周期数、经验迭代次数或分析超时作为通过条件；数学有限也不能自动代替计算成本评估。

## 成熟依据与结论边界

[网络演算作者资料](https://github.com/leboudec/netcal)区分到达需求与服务合同；[线速和服务曲线研究](https://arxiv.org/abs/1905.04956)不能被解释为峰率自动提供最低服务。[York响应时间研究](https://www.cs.york.ac.uk/rts/publications/Davis2007b.html)的方法必须先核对调度/阻塞假设。当前正在核验具体等价条件，尚未选定可实施的完整忙期算法。

三种结论必须分开：确定字节的峰率必要检查；可能SR的保守准入；依赖最低服务及误差界的充分保证。当前snapshot/frontier/峰率只支持前两类，不能虚构第三类。

源码事实已由两名独立调查者交叉核验；这不是算法设计PASS。算法边界收敛并审查后才修改生产消费者，随后按Windows Release真实realtime→RKMPP顺序验证。

## 本轮设计裁决

完整忙期方案仍为FAIL，不能用仅AU时窗内的必要需求检查替代原目标。该检查能发现后继周期遗漏，但不能证明提前让行、提交背压及AU完成后的维护积压全部计入。本轮先实施独立的generation固定控制存储，周期策略保持不变，继续收敛完整状态机。

已核对emitPendingThrough先waitForCommit，再计算维护义务及尾部；遇到未来维护边界时返回唤醒时刻，到期后经advanceThroughAvailable进入materializeMaintenanceThrough。后者仅在writeDueMaintenance成功后推进对应nextPsi/nextPcr。下一步证明需覆盖上述成功推进、WouldBlock重试与失败终止，不能将一次planner调用等同于一次维护消耗。

对release为r、周期为T、原AU期限为H的流，窗口实例数为H<r时0，否则floor((H-r)/T)+1；这是有限计数，不是调度证明。提前让行若E+c>L且L>=r，只能推出max(0,r-E)<c，其中E是服务frontier而非墙钟now。尚未证明每次空隙与维护消耗一一对应，禁止直接把每周期增加一个数据报时长当成等价算法。

完成条件仍包括：真实提前让行策略的离散状态机等价、全部carry-out初始状态互斥计费、维护忙期有限边界、checked整数运算及在线计算成本。峰率模型只用于准入，不推导操作系统最低服务保证；不可表示或无法证明闭合时必须明确失败，不能固定迭代后返回通过。

## 状态机证据更新与两个完成切点

本节更新前述“尚未证明每次空隙与维护消耗一一对应”的调查状态：按planner调用计数不成立；按同一release-group合并等待区间，可证明成功路径只推进一次。完整忙期仍为FAIL，本节不批准实施窗口检查替代品，也不改变生产策略。

- `MediaTsMuxSession.cpp:603`先等待上一媒体prefix提交；`:609`随后判断AU完成，尚未完成才规划下一prefix。`:629`的未来boundary只返回release、不改维护游标；到期由`:634`推进到该release。
- `MediaTsMuxSession.cpp:722`按最早release取组；同刻PSI/PCR一起处理，PSI内PAT先于PMT。`:737`仅在writeDueMaintenance成功后推进nextPsi/nextPcr。WouldBlock不推进本组；其他失败退出，不形成成功调度轨迹。
- `ProjectMpegTsMuxSessionAdapter.cpp:693`先排已有批次，再poll。重复poll可能多次返回同一boundary，因此不能给每次返回都加阻塞成本；这些返回只切分同一等待区间。
- `MediaTsPrefixPublicationPlanner`的PreserveSelectedMaintenance分支不会提前让行；超过后继latest-start直接失败。分析不得把该失败改写为可以再等一个周期，也不得丢弃后继latest-start检查。

令B为当前AU剩余各单datagram的最大模型服务时长，使用真实payload、endpoint开销、峰率及每个媒体view一次possible SR推导。对纯`earliestCompletion > limit`提前让行，E=max(prefixRelease,publishedWork.availableAt)，候选时长c<=B，反推校验已保证latestStart>=下一维护release r；且earliestCompletion<=AU原期限，因此该分支有max(0,r-E)<c<=B。按同一release-group等待区间的并集计一次，不能按poll次数计。`prefixRelease>=r`表示媒体未来release约束，不计为B覆盖的损失；`observedNow>=r`表示维护已到期。B不覆盖真实提交等待、资源背压、线程唤醒或其他scope成员新增债务。

**真实完成切点**：`MediaTsPendingEmission::finished`要求最后prefix已commit；`MediaTsMuxSession.cpp:603–613`先观察该commit再completePending。`MediaRtpWireCommitState.cpp:187–192`先提交global的submitted facts，再提交protocol prefix并唤醒；`MediaScheduledDatagramSenderNode.cpp:641`传入真实提交完成时间。封闭单TS producer/FIFO成功路径中，该切点之前发表的TS媒体和维护已经提交，等待最后prefix期间不能再生成维护。因此不能在该真实切点重加全部旧published backlog。仍须保留当次global/scope服务前沿；提交完成不等于pacing服务尾部结束。尚未生成维护由nextPsi/nextPcr表示，可能已到期。此结论不泛化到SeparateAvRtp或并发终止发布。

**预测完成切点**：当前准入尚未发生最后提交。只能从固定观察的frontier、当前尾部、周期维护及其插入顺序构造预测轨迹；必须逐步证明预测队列结构对应上面的成功状态转换。不得引用未来真实final commit来证明当前预测已清空。global的scopeAvailableAt只是当次共享scope前沿：`MediaDatagramServiceScopeArbiter::SubmitGuard::complete`按每次实际提交推进，其他成员后续仍可增加债务；它既不是本TS独占空闲时刻，也不是未来共享scope服务保证。

## 可计算候选包络及不能越过的门禁

下面仅记录待审查的数学候选，不能直接作为生产通过公式。对固定观察时刻t0、下一release ri、实际周期Ti>0，长度L内维护实例数可精确计算：t0+L<ri时Ni(L)=0，否则Ni(L)=floor((t0+L-ri)/Ti)+1。令Ci为该类维护一次的真实模型服务需求，包含实际prepare批次允许的possible SR。按两个流分别收费会对同刻组保守重复计B；令Ci'=Ci+B，则有Ni(L)<=ai+L/Ti，其中ai=1+ceil(max(0,t0-ri)/Ti)。

若另行证明Q完整覆盖初始观察债务、媒体release约束及全部媒体服务，并证明整个预测轨迹的额外空隙仅为上述按组损失，则D(L)<=A+U'L，其中A=Q+sum(ai*Ci')，U'=sum(Ci'/Ti)。仅在U'<1且上述前提成立时，Lbar=ceil(A/(1-U'))可作为该理想化模型的有限忙窗候选界。U'>=1表示此包络不能闭合，不等于真实系统必然不可调度。不能把此候选界叫作OS完成时限。

目前未闭合的具体需求是：Q如何从现有观察和EmissionSchedule的维护推进规则构造，保证媒体未来release和逐prefix commit门控既不漏计也不重复；以及预测每个维护组仍满足原deadline与successor latest-start。仅给累计需求上界不能代替这两个顺序证明。固定观察后其他scope成员的新债务与真实背压没有上界，因而现有事实不能实现“未来实际服务充分保证”；允许的目标仍只能是固定观察债务下的准入投影，后续实际延迟由原错误语义处理。

若该投影证明完成，两个周期游标可常量存储归并，事件数由剩余媒体datagram数及Ni(Lbar)导出，而非经验迭代次数。所有时间差、release推进、实例数及需求累加均须checked；利用率和闭式有理数运算也需可表示性证明，现有部分MediaUInt128操作不能自动视为任意宽度算术。数学有限不代表已有在线CPU时限保证。

成熟依据为[Network Calculus作者资料](https://leboudec.github.io/netcal/)的到达/服务合同区分及[ECRTS 2020忙窗分析](https://drops.dagstuhl.de/entities/document/10.4230/LIPIcs.ECRTS.2020.22)的适用假设。上述按组空隙是本仓库状态机到成熟方法的待审查适配，不声称文献已经证明本实现。后续独立审查须分别判定状态机计费、预测frontier映射及忙窗终止；三者未全部通过前保持完整设计FAIL。
