# r282：打包收集与发送准备的原子存储预约

状态：全局预约、RTP/UDP/terminal 跨重试 owner 与统一 wire 存储预算已接入；本切片双独立源码审查 PASS、Release 全量构建成功，r290 MPEG-TS/UDP 实流功能观察完成。整体门禁未通过；固定元数据、上游 TS buffer 预算、完整背压覆盖、接收端 A/V 与完整 Windows/RKMPP 矩阵仍待闭合。原合屏验收范围不变。

## 依据与边界

采用 [GStreamer allocation negotiation / buffer pool](https://gstreamer.freedesktop.org/documentation/plugin-development/advanced/allocation.html) 的使用前确定容量、缓冲所有权随数据保留、最后引用释放的原则。该资料不提供本项目跨收集与发送账本的原子性证明；下列所有权与重试条件仍须对当前调用链逐项验证。共享 C++ 路径覆盖 Windows/RKMPP，无新增平台媒体链路或外部 FFmpeg 修改。

## 当前源码确认

- MediaRtpDatagramMaterializerNode：packetizedBytes、packetizedPayloadOctets 和临时 views 仍用普通 vector；packetizer 已推进后，wire 准备才预约内存。r280 单包 scratch 仍未纳入预算。
- MediaRealtimeAvProtocolOutputPlanner 先形成 protocolOutput，再生成 datagramTransport；逐流 emission contract 在前者可取，commit/batch geometry 由网络 ledger 生成。
- MediaRealtimeDeploymentPlanner:810 与 MediaDatagramTransportPlanTemplate::activate 均调用同一 network ledger。新增收集费用必须同时覆盖部署预估与激活，不能只改后者。
- MediaRtpWireDatagramMaterializer:366 在 workspace、output、commit backing 已分配之后调用 globalSequence->reserve。
- MediaWireGlobalSequenceState::reserve 按值接收 backing，前部移出两个数组，随后才检查串行和队列容量并可能返回 WouldBlock。失败会销毁这份 backing。因此，仅提前传入 prepaid lease 不足以保持跨重试预约。
- 全局 reserve 的三个当前调用点为 RTP 媒体批、RTP terminal 和 MPEG-TS UDP；改变其所有权契约必须逐项核验，不能只修 raw RTP 调用点。

## 预约与存储产品

N、D 来自逐流 emission contract 的最大 AU 包数、最大 datagram 字节；R 来自已规划 RTCP endpoint 上界；E=N+1，表示一个批次最多附带一份 SR。

- C：连续 N*D 收集 backing、N 个 datagram view、一个 D 字节 scratch 的真实 allocationFootprint，含对齐与分配头，全部 checked arithmetic。
- W：N*D+R 加 E 项 output partition overhead、RTP workspace footprint(N,E)、commit preparation footprint(E)。复用现有 footprint authority，不复制另一套常量。
- 打包前同根一次预约 C+W；C 分配后由 AU owner 持有，W 保留给发送准备。额度不足时不推进 packetizer。
- ledger 不得把现有 wire/backlog 费用重新相加；需证明同时存在的 collector 数量与额外准备所有者容量，不能用经验倍数。
- collector 明确区分已分配、打包完成、等待发送，不能用非空容器同时表示这些状态。

## 必须先闭合的重试契约

1. global reserve 在可重试拒绝时不消费调用方的 prepared backing；成功后才转移。
2. 完整 wire preparation 必须有跨重试 owner，不能仅保留 collector。它拥有所有 workspace、output 和 commit backing。
3. serialization view 中对 RTP/RTCP plan 的引用必须指向该 owner 的稳定存储；禁止保留栈上 rtcpPlan 或移动后失效地址。
4. 重试仍须按现有权威时钟和 RTCP 规则处理时间；不能因冻结旧计划而隐藏过期，也不能重算后增大需求并重新抢 root。
5. 尚未取得全局序列时可重试；一旦取得，后续失败沿现有 terminal/purge 语义处理，禁止取消后假装未推进。
6. purge/stop 释放 collector、scratch、未提交准备和额度；已发布输出由最后 owner 释放。不得因资源等待阻塞持有全局锁的线程。
7. 保持 commitBeforeNextReservation 原值；本项不宣称解决全局公平性、所有普通堆对象或跨 generation 总容量。

## 实施与验证顺序

先完成跨重试 owner 和 global reserve 所有权设计审查，再把 planner 产品、ledger、collector、两种 packetizer scratch 与 materializer 一次接通；禁止只增加无人使用的 helper 就标完成。代码冻结后双独立复审、Release 全量构建、原规格 Windows realtime 实流。继续保留 r281 的迟交、接收端 A/V 与 delivery_evidence 缺口；Windows 完整门禁通过后再 RKMPP。

## 独立审查结论与后续接线

r280_admission_review 只读结论：事实接线方向 PASS，完整原子预约设计尚不通过；阶段账本及跨 global reserve 的 prepared owner 必须先落实。此为设计审查，不是实现验收。

- R 直接复用 MediaRtcpWireGeometry::compoundPayloadBytes(cname.size())，由已存在的 wire planner 路径交叉核对，无需新增尺寸算法。
- MediaRealtimeDeploymentPlanner::planBase(request, preparedEmission) 已有真实编码、AU 上界/布局和 MTU。复用 MediaRtpAccessUnitEmissionContract::createVideo/createAacLatm 产生内部逐流事实，随内部 base/deployment 产品传递。禁止从聚合 admittedWire 反推逐流 N/D。
- complete 和 activate 使用同一账本；最终 transport planner 用 scheduled contract 校验这些事实，并经 plan codec 交付节点。MPEG-TS/UDP 明确不存在 raw collector 产品，不填默认容量。
- 账本须区分固定状态、已发布输出、每个 producer 的一个未发布 AU。原 workspace 贡献需迁移或抵扣；不能保留现有费用又重复加 W。若为“上一批输出＋下一批准备”预留同时存活容量，必须按实际 topology/owner 数证明，跨 generation 仍独立检查。
- 下一项先设计并落实 prepared owner 的稳定地址、全局预约成功前不消费 backing 的接口，再接通逐流 planner/ledger；仅增加 C 不能证明有界推进。

## 第一步实现：全局预约失败不消费 backing

已修改 MediaWireGlobalSequenceState::reserve 为独占 prepared storage 引用；校验、背压和 residence 计算先于填充，只有成功发布 reservation 时才移动 backing。填充异常清空元素但保留分配。RTP 媒体、terminal、MPEG-TS UDP 三个调用点均改为传递左值。

两个未参与实现的审查者 r280_packetizer_review_a、r280_packetizer_review_b 均明确 PASS，仅限该接口变更。三个调用点当前仍是局部准备对象，故本步未实现跨调用存储保留，更未完成 C+W 与账本。正在进行冻结源码的 Release 全量构建；尚无本版本实流证据。

后续 owner 可进一步简化：MediaWireDatagramBatchPartitionBuilder::prepare 调用 MediaWireDatagramBatchBuffer::prepare，后者同步执行 input.bytes.writeInto 并持有生成字节。因而已完成 prepare 的输出分区不需要保留局部 RTCP/RTP serialization plan 引用。跨重试只持有生成字节、global entries/backing、actions、commit backing 与协议投影事实；不得为了保留无用 views 增加堆分配。冻结的 deadline/materializedAt 与现有重算语义差异仍须设计核验。

Release 全量构建完成：configure/build 均返回0，726项构建结束，CMakeCache.txt、build.ninja、local/realtime CLI齐全。realtime SHA256：DF538A749EEC487342D323908DCFC99F852899CBA8E8E5C615E02E761C43CA00。仅检查 local 产物存在，未运行 local。未把构建当作实流验收，本版本实流待后续接线完成。

时间契约核验：MediaDatagramWireDeadlinePlan::canonicalDeadline 明确要求首次 materialization 固定 deadline，sender retry 不得延长。后续 owner 应保留首次 materializedAt/deadline，使等待时间进入真实 residence；需要同规格实流验证此行为差异，不用重算来掩盖超期。

## 第二步接口与生命周期决议

独立审查 r280_admission_review 对显式 move-only Prepared owner 方案给出局部设计 PASS。生产接线尚未实现；此结论不替代冻结代码双审。

采用 prepareBatch(input, materializedAt) → Prepared，再 complete(Prepared&, now) → 输出集合。complete 不接受新的 input/span/protocolBatch，避免重试混用另一个批次。raw 节点和 MPEG-TS 节点的 generation data 各以 optional 值持有 owner；不在共享 RTP state 中隐式缓存并猜测下一次输入身份，不增加每 AU 的独立 owner 堆分配。

Prepared 保存已计费 globalEntries、global/output backing、actions、prepared commit control、投影摘要（包数/字节数/末尾时间戳及 release/SR commit token）、原始最早 deadline、所属 state 与唯一 preparation ticket。不保留已经同步序列化的局部计划或原始 raw span。

- state mutex 下独占活动 ticket；prepare、complete、terminal 均校验，避免共享 materializer 副本并行推进相同投影状态。
- global 预约成功前，owner 析构只释放 backing、注销 ticket，不推进/毒化 RTP 状态。析构不得在已持有同一 state mutex 时再次加锁；失败返回、move assignment 和 optional reset 必须逐路径审查。
- complete 的 WouldBlock 保留同一 owner；不重新申请 root、不重新打包、不更新投影、不重写 SR 时间。
- global 预约成功后沿现有不可撤销提交/poison/purge 语义推进；成功交接释放 ticket，再由输出最后 owner 释放 backing。
- MPEG-TS prepare 保存真实共享 batch owner、初始 materializedDatagrams 游标及 count。complete 在取得 global 许可前核对游标，成功后才 takeCommitTransaction；不得用当前游标的另一个 slice 替代。
- generation purge 已通过 MediaProtocolOutputGenerationData::resetForGenerationPurge 替换整个 data。Prepared 须在其中受同一 owner-thread 清理，并保持其引用的 state/batch 生命周期。
- terminal 与 media 共用 preparation 生命周期为目标；未参与时不发 BYE 的现有 RFC3550 分支保持。至少先拒绝在活动 ticket 存在时推进 terminal，不能静默抢先。
- UDP 使用相同的公共准备存储职责，RTP 特有投影/动作单独持有；MPEG-TS/RTP adapter 仅转交 views 给同一 RTP 实现，不另建媒体处理链路。
- optional 增大的 generation 固定元数据仍须计费；“无额外堆分配”不等于“无存储成本”。

下一步生产修改范围：准备 owner 与 ticket、RTP materializer 两阶段实现、MPEG-TS adapter/UDP 公共准备接线、两个 node 的 generation data 与重试分支。完成后继续 collector+wire 前置预约和逐流 ledger，最终冻结后再全量构建及原规格实流。

## 第二步实现进度

已接入 MediaRtpWirePreparedTransaction：显式 prepareBatch/complete，raw 与 MPEG-TS/RTP generation data 持有 optional owner；MPEG-TS 保存共享 batch 与 materialized 游标，state 使用单一活动 preparation permit，terminal 拒绝超越。输出、global backing、actions、control 和投影摘要跨 WouldBlock 保留。UDP、terminal 自身的跨重试准备及 C+W/ledger 尚未接通。

第一轮双审均 FAIL：两个 node 首次 complete 使用 prepare 前采样时间，可能漏检准备期间发生的过期。需每次 complete 前重新读 authority 时钟，保留原 materializedAt/deadline。此时全量构建已启动，冻结期间不修改源码；构建后修复并双审、重建。未进行本版本实流测试。

## 第二步最终验证记录

两 node 的 complete 前均重新读取 authority 时钟，原 materializedAt/deadline 保持。A、B 双独立复审明确 PASS；修复后 Release 全量构建 configure/build=0，727项完成，realtime SHA256 724C69C9F8CB2106C0A37DA31403E9ED54098B38B241CF6211331A0967C797BF。

本机 separate RTP 原规格三路与所选源重入各完整120秒；VLC原生快照确认三格→全黑→左下重入→全黑。CLI caller_stop=0。最终 errors/workerErrors/graphPayloadPressureFailures/deadline_misses 均0，payload预约/释放均257963且最终0字节/0对象。未见 late_output_commit；最大submit lateness=52573580ns，delivery_evidence=not_proven。CPU平均1.610284%、峰值5.360825%，工作集初始208388096、最终261820416、峰值269873152字节。具体命令/指标/错误记录见 realtime-composition-r282-validation.md；17个临时文件和六个PID均已核验清理。

本次只验证 separate RTP 生产运行；未用该结果代替MPEG-TS/RTP复验，也未证明每条global WouldBlock路径实际触发。下一步仍须完成C+W前置预约、逐流ledger及UDP/terminal跨重试owner和固定元数据计费；整体合屏门禁、receiver A/V/wire证据、Windows完整矩阵与RKMPP均保持开放。

## 后续容量规划：音频编码事实

已在内部 MediaPreparedAudioEncoderEmissionEnvelope 增加 codecId，由唯一构造点在 avcodec_open2 成功后读取 context->codec_id。pipeline 与 resolver 整对象复制保留该事实；resolver 拒绝 NONE，AAC LATM 规划仅接受实际 AAC 编码器，其他输出仍保留非 AAC 能力。未新增公共输入。

两名未参与实现的独立审查者均明确 PASS（仅此四处事实传递与校验）；核对本机 FFmpeg 打开编码器的 ID 校验和复制路径。Release 全量重建完成，configure/build=0，727项，必需四项构建产物齐全，git diff --check 成功。realtime SHA256 11E1E007BF831F832B9C3CCFF421F4BBE86E8A52FFB880185D1F8F465A6602C9。未运行 local CLI，尚无本切片新增实流结论。C+W 前置预约和逐流 ledger 尚未完成。

## UDP 载荷上限复用与账本寿命核验

依据 RFC 8085 第3.2节（https://www.rfc-editor.org/rfc/rfc8085#section-3.2），复用既有路由 MTU 事实扣除 IP/UDP 头的计算；在 MediaRealtimeDatagramPayloadPlanner 增加 maximumPayloadBytes，并接入 conservativeWire 和 applyOutput，避免部署与实际打包使用两份计算。有效事实保持原数值，缺失事实明确失败；不宣称增加路径 MTU 探测能力。两名独立源码审查均明确 PASS；Release 全量重建 configure/build=0，727项完成，四项必需产物已核验。realtime SHA256 A627E11D908E6945A9BB532FDFB97B30A9EDF93E89E07E47B76823D60FFECA6C。本切片未新增实流验收。

只读所有权调查确认：prepare 返回后 workspace 的六组数组仅 globalEntries 随 pending 保留，其余五组释放；发布后 workspace 不保留；global 两组数组、RTP actions 和 control 随 transaction 保留到最后未完成 slice 释放 control。MediaDatagramCommitSlice::commitSubmittedPrefix 在分片完整提交时立即 reset control（不是等 batch 析构），bind 的局部 transaction 也在返回时释放引用。因此后续费用迁移只能移出 workspace 部分，不能删掉 global/actions/control 费用。部分或完整 commit 可先返还 wire 额度，而旧 backing 尚在；不得用未提交 backlog 数量冒充全部存活 owner 上界。该寿命边界尚未证明，C+W 完整设计仍未通过。

### 已提交存储的上界调查

当前 activate 明确生成 commitBeforeNextReservation=true；global reserve 在有活动 reservation 时拒绝下一份。MediaFixedCapacityStorage::pop_front 会销毁元素，不保留已删除 shared_ptr。producer 的 pendingWireOutputs 在 commitReservedOutput 时弹出，FFmpegNodeRuntime 最多一个 pendingTransfer，并在它排空之前不处理下一份输出。sender 只有一个 m_pendingBatch，在提交完成后下一次循环 reset；process 中局部 batchInput 也只在本次调用存活。

这些证据将待证明项收窄为：发布成功与 producer 回调之间仍持有的 batch、sender 当前已提交 batch、发布容器 backing，以及跨 generation 的清理屏障。不能把它们与 transaction control 的寿命混为一谈，也不能仅按引用数累加同一 backing。下一步核对 fanout 拓扑与 purge 屏障后，按真实所有者位置形成费用产品；目前未改运行时释放行为，未宣称容量闭环通过。

### 最终图唯一消费者校验

设计依据：GStreamer pads 与显式 tee（https://gstreamer.freedesktop.org/documentation/application-development/basics/pads.html）将数据分发体现为图拓扑；本项目已有单消费者 wire 输出契约，本项补校验而不新增分发机制。MediaDatagramOutputGraphShapeValidator 现检查 wire.multiple=false、该端口全部出边恰一条、目标为该会话唯一 sender.batch 且节点身份与 edge policy 匹配；sender.batch 全部入边数等于已逐个核验的 materializer 集合。普通 RTP/MPEG-TS builder 原本满足这些条件。两名独立审查均明确 PASS，Release 全量重建 configure/build=0，727项完成且四项必需产物齐全，git diff --check 成功。realtime SHA256 0D85182075069A7CD82C5BE195898BF3520369A709DC7CEBC3BC8CEC77640F83。尚无新增实流结论。

单代串行前提下，已提交尾部最多为每 producer 一个发布回调中的 partition 加 sender 一个当前 partition；未消费队列项属于唯一活动事务。slice 完成即释放 control，不能把历史尾部算成持有完整事务数组。purge 检查已确认 producer/sender 清理先于 acknowledge，全部参与者确认后才发布下一代；固定元数据重叠仍需另计。后续 ledger 必须同时绑定串行策略、实际 producer 集合及逐流最大几何，不得将上述结论泛化到 nonserial 或 fanout 图。

### 逐流打包事实的共同规划入口

新增内部 MediaPreparedRtpEmissionPlanner，直接消费已打开编码器 snapshot、prepared emission 中的布局和 AU 上界，复用现有 MediaRtpAccessUnitEmissionContract 的视频及 AAC LATM 算法；不新增分包算法。scheduled packetization 与 conservativeWire 均调用它，后者单 AU 包数不再另算。视频明确只接受实际 H.264/HEVC，scheduled 另核对重复持有的布局一致性；音频明确要求实际 AAC。codecName 与实际 mode 的一致性仍由既有 scheduled plan create 校验。

此项沿用 r280 对 RFC 视频打包与 FFmpeg LATM 实现的调查，尚不改变窗口速率/burst 估算，也尚未把逐流产品传入 ledger。两名独立源码审查均明确 PASS；Release 全量重建 configure/build=0，728项完成，四项必需产物齐全，git diff --check 成功。realtime SHA256 C22F757271873A57FB682F0B84B65DB66591438D3F6929542BDFAA09257BA1FE。尚无新增实流验收。

### 逐流产品贯穿部署与激活

新增内部 MediaRawRtpProducerPlan：role、既有 emission contract、CNAME 推导的 RTCP compound 字节上界。planBase 由已打开编码器事实与真实 MTU 构造，BasePlan 经 complete 写入 deployment.resources；首次和 activation 的 network ledger 均接收并验证同一产品。Envelope 校验角色集合、模式、布局与 MTU；transport planner 对实际 scheduled emission 全量相等及 RTCP compound 字节数进行核对。MPEG-TS 布局要求 raw 产品为空。本项尚未增加收费或改变运行时预约。

首轮独立审查发现 plan source node codec 漏接（FAIL），会使 raw RTP 往返解码丢失产品。已补 producer_count 及 role/mode/AU/D/N/authority/layout/RTCP 编码与解码；计数按 topology 限定后分配，重建既有 contract 并核对 N，缺失或不一致失败。另一份首轮 PASS 因漏查 codec 不作为完整审查通过依据；修复后两者均明确 PASS；Release 全量重建 configure/build=0，729项完成。r283 原规格实流在启动时失败，详见 realtime-composition-r283-validation.md，不能判定此版本运行通过。

### r283 失败后的诊断

MediaDatagramShapingPlan::decode 原失败条件保持，错误中补齐 session/generation/endpoints/service/backlog/batch/network budget 数值，保留原错误前缀及类别；双独立诊断审查 PASS，诊断版本重建中。只读调查发现两阶段 AAC rate 公式不同：early 使用 peak bytes/容量加 AU 率，final 使用合同最大包数乘 AU 率；这可能让 early 预算不足，但尚不能仅凭公式差异断言本次失败因果。需实流数值确认，不修改外部 FFmpeg、不降低规格。

### r284 数值确认与音频窗口对齐

r284确认shaping失败的实际关系：userspace14473918+socket756580=15230498，大于部署15227668，差2830字节，其余打印guard条件满足。原命令、错误和清理见 realtime-composition-r284-validation.md。此证据证明预算闭合失败，不单独证明哪项公式造成全部差额。

已将conservativeWire的AAC rate改为与final elementaryDemand相同的ceilScale(N,AU_num,AU_den)，burst与atomic均使用同一AAC LATM contract的N。移除按平均字节率估算包数的另一套公式，未抬高固定预算、未加经验余量。仍保留初始15字节最大RTP头计费；MPEG-TS分支未改。修复后双独立审查PASS、Release全量构建成功（729项）。r285原规格三源及重入各完整120秒，VLC原生快照三格→全黑→左下重入→全黑；CLI调用方停止0，errors/workerErrors/pressure/deadline_misses均0，payload最终0且预约/释放均252004。该轮启动预算失败不再发生。最大submit lateness=29709520ns、delivery_evidence=not_proven；整体门禁仍未通过。17项临时文件及六个PID均已核验清理，详见realtime-composition-r285-validation.md。

## 联合预约实施：所有权上界修正

本轮只读审查及源码复核确认一个此前未单列的交接窗口：global commit 在 MediaWireGlobalSequenceState.cpp 中先 notifyReservationWaiters，再 releaseCompleted；RTP commit 继续更新协议状态，返回后 MediaDatagramCommitSlice 才 reset control。因此 serial=true 只限制未完成 global reservation，不能推出任意瞬间仅有一个 transaction backing。旧 control/actions/global 数组可能与新事务短暂共存，需按唯一 sender 的一个当前提交调用计入，不能只计尾部分区 payload。

独立设计审查给出的容量上界（并非最小实测峰值）：

- 每源 E=N+1、U=N*D+R；C 为 collection 的三块真实分配 footprint；K 为 commit preparation footprint(E)。
- W=workspace(N,E)+K+U+output overhead(E)，一次预约 C+W。
- A 为整个 AU 发布后 payload/descriptor/batch control 加 K，排除 workspace、临时 Partition[] 和 producer collection。
- T 为一个分区的 payload/descriptor/batch control 上界；Q 为 producer publication collection 的 shared_ptr backing。
- 每源槽位为 max(C+W,Q+T)；raw owner 容量上界为各源槽位之和 + max(A) + max(T+K)。共享引用不重复收费；容量上界允许各互斥 owner 类别的保守组合。

接线前必须由 partition 模块提供 A/T/Q 的实际几何，绑定唯一 sender、单 pending、serial 和跨代清理屏障。原 BacklogPayload、BatchContainer、CommitControlBlocks、CommitReservationStorage 的 raw 费用需迁移，不能在保留旧额的同时完整增加 W；固定 records/registry、独立调度容器、evidence 和 socket 仍须按各自实际所有者计费。

新增 MediaRtpAccessUnitCollection.h/.cpp：从已预约 lease 分割连续 N*D 字节、N 个 view、D 字节 scratch，使用现有预算 allocation header 和固定容量存储；append 不增长，seal 区分收集与可读状态，移动不迁移 backing。当前只是接线中的实现，尚未替换 node 的旧 vector，也未改变 packetizer 接口或 ledger。不得将其视为原子预约已完成；本轮尚未构建或运行新实流。
两名未参与实现的审查者 r280_packetizer_review_a、r280_packetizer_review_b 对 collection 局部静态审查均明确 PASS，范围仅为固定存储/RAII/溢出/移动/借用生命周期。接线方必须将 prepare 分配中途失败视为终止：已经 split 的部分会由 RAII 归还 root，不恢复原 reservation，不能拿余额重试。原子预约、节点接线与 ledger 仍未通过。

另外已核验唤醒：MediaDatagramTransportPlanSourceNode::start 从所有 typed plan 输出收集消费者 wakeup，创建冻结通知域；plan buffer 将该通知域传入网络预算。MediaStorageBudget::reserve 的 Backpressure 在不足时置 pressureArmed，release 在返还后通知。因此 packetizer 前的预约可复用该机制，不需依赖 wireMaterializer 已创建或另设轮询。
collection 切片 Release 全量重建成功：configure/build 均0，730项；CMakeCache.txt、build.ninja、local/realtime CLI四项产物齐全，realtime SHA256 E43532743324D7D5B73A01D6DDE45F7C53CC18C415FDC201450B4DF492747E2A。git diff --check 成功，新文件UTF-8无BOM/CRLF复核通过。未运行local CLI，也未运行新版本实流；未产生本轮抓包、快照或临时媒体。构建与局部双审不能替代尚未接通的联合预约及最终真实链路门禁。
## raw RTP 联合预约生产接线

已将 C+W 接入生产节点：MediaRawRtpStoragePlanner 从每源 emission、RTCP 大小、commit/batch geometry 推导 collection、wire preparation、producer slot、active transaction 和 sender handoff 产品。分区模块统一提供 published、single partition、publication collection 的 footprint，原运行时 Sizing 复用 single partition 计算。无新增公共参数。

network ledger 的 raw RTP 原 payload/container/batch/control/reservation 贡献迁入 RawRtpOwnedStorage；固定 records/registry、endpoint/evidence 与 kernel socket 保留。ledger 持有事实和产品并重新计算校验，typed serial 标志随激活下传，两个规划阶段使用相同入口。MPEG-TS 不产生 raw collection 产品，媒体链路未分叉。

节点在 packetizer 前对同一个网络 root 一次预约 C+W，collection 从其中分割 C；两种 packetizer 同步借用 D 字节 scratch，旧 vector backing 已移除。seal 后保留同一 AU 及剩余 W，prepareBatch 在可重试检查后将 W shrink 到真实 footprint，再分割 workspace/output/commit；raw 不再二次 reserve。global WouldBlock 保留原 prepared owner。stop/abort 显式释放所有 pending，purge 仍替换 generation data。未使用默认容量或新增预算余量。

MPEG-TS prepare 显式不传 prepaid，保持已有流程；UDP 和 terminal 自身的跨重试 owner 仍为后续未完成项。固定元数据全量计费、delivery/receiver A/V、完整 Windows 矩阵与 RKMPP 不因本次接线而判定完成。当前源码冻结待双独立复审及 Release 构建，尚无此版本实流结果。
本次raw RTP C+W接线已获两名独立审查者明确PASS；Release全量重建configure/build=0（731项）。r286实流初始三源及所选源重入均完整120秒、源退出0；VLC原生快照三格→全黑→左下重入→全黑。CLI caller_stop=0，errors/workerErrors/pressure/deadline_misses均0，媒体额度最终0且预约/释放均258997。最大submit lateness=65539840ns，delivery_evidence=not_proven；整体门禁仍未通过。17项临时文件及六PID已核验清理，具体命令、指标、VLC错误和限制见realtime-composition-r286-validation.md。下一步继续UDP/terminal跨重试owner及固定元数据计费，不以本轮替代全部背压分支、Windows完整矩阵和RKMPP证明。
## RTCP SDES 长度一致性修复

依据 RFC 3550 §6.5（https://datatracker.ietf.org/doc/html/rfc3550#section-6.5），SDES CNAME 后必须包含 END，再补齐32位边界。旧共享 planner 使用 align4(10+L)，生成器使用 align4(11+L)；L%4=2 时前者少算4字节。现由 MediaRtcpWireGeometry::sdesPayloadBytes 统一算法与1..255字节长度范围，生成器复用该入口及 SR/SDES/BYE 协议长度常数。raw RTP 与 MPEG-TS/RTP 的所有现有 compound 调用共享修正；无外部依赖修改。

两名独立审查者 r280_packetizer_review_a/b 均对本次三文件修复明确 PASS；git diff --check 通过。Release 全量重建成功（731项，configure/build=0），四项必需产物齐全；realtime SHA256 840C500A2D5D3BBF1349A3E779BABF4D608A8DA54E43B2BFB57392BD351EF7A2。未运行 local CLI，尚无修复后实流证据。终止 owner、终止报文显式存储上界、UDP owner 与固定元数据计费仍未完成；不得将本次局部修复视为完整验收。
## RTP 终止事务跨重试所有权

沿用 RFC 3550 §6.3.7 的静默离开语义及现有 prepare/complete 资源所有权设计。prepareTerminalReport 返回可空的同一 MediaRtpWirePreparedTransaction，单 entry 内联存于 variant；无媒体 collector/workspace，无新增公共参数。序列化完成后持有原始 materializedAt、deadline、commit/global/actions/output backing；complete 遇到 global WouldBlock 保留全部资源及截止时间。成功提交全局预约后才设置 projectedTerminal，不推进媒体计数或覆盖既有 RTP timestamp；reservation identity 溢出在共用 complete 内、全局预约前检查。

raw 与 MPEG-TS/RTP 节点共用 pending preparation 和 complete；MPEG-TS stop/abort resetState 补充释放 pending。raw planner 校验 R+BYE<=D/batch，并显式计算 terminal preparation、publication、active/handoff，与媒体互斥阶段分别取最大值，媒体 C+W 预约量保持按其实际分配计算。

两名独立审查者 r280_packetizer_review_a/b 均对本切片明确 PASS；git diff --check 通过。Release 全量重建成功（731项，configure/build=0，四项产物齐全）。r287三源及所选源重入各完整120秒，VLC原生快照三格→全黑→左下重入→全黑；CLI caller_stop=0，errors/workerErrors/deadline_misses=0，媒体额度最终0且预约/释放均267130。payload新等待计数1，不是硬失败，也不是network owner分支证据。最大submit lateness=44587840ns，delivery_evidence=not_proven；完整门禁未通过。17项临时产物与六PID已清理，见 realtime-composition-r287-validation.md。MPEG-TS 旧 ledger 虽包含基本分配类型，仍未证明准备、活动事务、sender 交接同时持有时的数量上界；UDP prepared owner、固定元数据、完整 Windows/RKMPP 门禁亦未完成。

## MPEG-TS/UDP prepared owner

依据GStreamer allocation所有权模型（https://gstreamer.freedesktop.org/documentation/plugin-development/advanced/allocation.html）及现有DAG单worker/代际清理契约，UDP materializer内部以唯一optional持有prepared事务，不新增线程、mutex或独占ticket堆。prepare仅接受同一protocol owner/cursor重试，保留原报文、materializedAt和deadline；complete重新取时钟，global WouldBlock不丢backing，global成功后任何嵌套事务/bind失败进入不可重试状态并保留原错误。stop/purge整体销毁materializer释放pending。

删除无调用的materialize/materializeBatch入口与四组vector；直接读protocol descriptors，以两组fixed storage保存global entries及临时output inputs。workspace+commit+精确output sizing一次admit，分割workspace后剩余lease交fromReservation；UDP workspace进入ledger碎片分配贡献。该贡献不证明MPEGTS同时存在prepared/active/sender交接的总体上界，仍需完成容量闭合。

首轮双审FAIL及构建C2039确认节点UDP分支漏接旧接口；修复shared owner prepare/fresh clock complete后两者复审PASS，Release全量构建成功（732项，configure/build=0，四项产物齐全）。r288原规格三源MPEGTS/UDP启动失败，未覆盖输出owner，原错误为MPEG-TS datagram materializer requires id and runtime authority。源码确认通用create错误地强制要求sharedNtpEpoch；工厂已验证authority，A/V runtime则要求epoch是否存在与显式输出policy完全一致。现将NTP强校验移至RTP专属初始化，UDP不伪造NTP。此修复经两名独立审查明确PASS，Release全量重建成功（732项，configure/build=0，四项产物齐全）。r289同规格MPEGTS/UDP已正常输出，三源及重入源各完整120秒，VLC原生快照三格→全黑→左下重入→全黑。CLI caller_stop=0，errors/workerErrors/payload pressure/deadline_misses均0，媒体额度最终0、预约/释放均261524；最大submit lateness=16298860ns，delivery_evidence=not_proven，整体门禁未通过。16项临时产物及六PID已清理，详见realtime-composition-r289-validation.md。r288命令、错误及九项清理清单见realtime-composition-r288-validation.md；四PID已退出，文件残留0，无远程产物。

## MPEG-TS 统一 wire 存储预算

已抽取 MediaWireProducerStoragePlanner，raw RTP 与 MPEG-TS 复用准备、发布、活动事务及 sender 交接的生命周期容量计算。MPEG-TS 的实际整包大小和 RTCP compound 几何作为内部类型化事实贯穿 deployment、codec、envelope 和 activation，并在最终 transport planner 与实际 mux 产品核对；旧 payload/container/control/reservation 贡献迁入单一 owned storage 项，避免重复计费。无新增公共输入。

两名独立审查者 r280_packetizer_review_a/b 对本切片均明确 PASS。2026-10-21 Release clean-first all-target 全量重建成功（734项，configure/build=0），cache、Ninja 与两个 CLI 产物齐全，git diff --check 通过；未运行 local CLI。本版本尚未进行真实流验证，上游 TS protocol buffer、固定元数据总预算、发送交付与接收端 A/V、完整 Windows/RKMPP 门禁仍未闭合。

## r290 实流与证据边界核验

r290 原规格三源及重入各完整120秒，VLC原生快照三格→全黑→左下重入→全黑；CLI caller_stop=0，错误/压力/截止失败为0，payload最终0且263894次预约/释放匹配。16项临时文件与六PID清理完成，详见 realtime-composition-r290-validation.md。

源码核验发现 sender 的 delivery_evidence=not_proven 是固定文本，不是动态失败判据。r290实际证据为178584个datagram本地提交，timestamp tracked/observed均0；Report策略允许平台timestamp不可用时继续，现有日志无法区分路由能力查询失败、active能力缺失/歧义、容量不可表示或SIO_TIMESTAMPING失败。独立只读审查确认以上边界。TX timestamp只能证明软件/硬件发送观察点，不证明远端交付；接收抓包和VLC也不能替代发送时间戳。下一步只读核验实际路由supported/active能力，再决定所需观测；不修改策略或默认假定硬件不支持。dumpcap -D已确认本机NPF_Loopback可用于下一轮接收连续性取证。依据微软Winsock timestamping说明：https://learn.microsoft.com/zh-cn/windows/win32/winsock/winsock-timestamping 。

## 上游 TS protocol buffer 审查：FAIL

独立审查与本机源码核对确认：MediaMpegTsProtocolDatagramBatchBuffer::create 的普通 payload/descriptor vectors、shared commit state及类型擦除控制对象未绑定预算lease；payloadFootprintBytes仅返回payload.size。FinalGraphResourceLedgerCompiler却将该payload归为network ledger已覆盖，而当前MPEG-TS wire产品collectionBytes=0。下游wire backing不能覆盖仍存活的上游副本。

MediaTsMuxSession内部排队字节在pop时即扣除，但materializer pending、prepared owner及protocol commit仍可能持有buffer和原cursor packet series；因此队列限制不是完整所有权上界。本项FAIL不否定已审查的wire阶段公式，否定整体存储闭合。下一实现必须沿既有GStreamer allocation/最后引用释放原则补齐上游typed footprint、使用前预约、跨阶段共享lease及同时持有上界；不得仅加预算常量或用RSS平稳代替证明。共享代码同时影响Windows/RKMPP，先Windows原规格验证。此轮只读审查，无源码改动或新增实流。

## TS 共享 backing 修复进度

已消除protocol batch第二份payload vector：buffer持有MediaTsPreparedPacketSeries，descriptor引用std::as_bytes给出的原packet存储，takeCommitToken只转移提交令牌。cursor缓存回收要求use_count==1，引用仍在时不会复用backing。此步骤只减少副本，descriptor/control及原packet存储尚未接入lease，预算FAIL保持。源码尚未重新构建/实流。

后续typed产品必须复用prepared最大AU和MediaTsTransportPacketBuilder::payloadPacketCount，以及PAT/PMT/PCR权威计数；最终图边容量形成outstanding series上界。需覆盖构建cursor、mux queue、pending transfer、materializer、wire prefix和sender交接；prepareRemaining释放active cursor但保留continuity reservation，不能用每PID一个active推导总存量。packetWorkspace缓存若保留必须持续计费。沿GStreamer共享backing/最后引用释放及有界buffer pool原则，在begin分配及连续性变更前预约，删除错误network覆盖声明，prefix控制对象也必须纳入准备预约。

共享TS backing修改已获两名独立审查PASS，并完成Release全量重建734项，configure/build=0，四项必需产物齐全；本版本未实流，未运行local CLI。

接线核对：video/audio prepared emission均已有maximumAccessUnitPayloadBytes；视频复用MediaTsVideoAccessUnitFramer::maximumFramedPayloadBytes，音频ADTS需将现有长度校验抽成framer共享sizing，PES/TS继续复用实际serializer及payloadPacketCount。最终容量在CompositionResourceBinder及普通/动态realtime final-resource路径生成内部typed产品，再经输出plan-source传至ProjectMpegTsMuxSessionAdapter和MediaTsMuxSession::Binding；fixedStorage条目本身不是runtime allocator，不可从authority字符串反解析。当前plan-source早于final compiler绑定，需要最终产品回填和重校验，禁止新建脱离图总预算的root。

ADTS共享sizing已提取为MediaTsAacAdtsFramer::framedPayloadBytes，实际frame复用同一计划校验与1..8184原始payload边界，再增加协议规定7字节；错误顺序和文本保持不变，两名独立审查均PASS。此修改尚未重建/实流，亦尚未接入TS series planner，不记预算闭合。下一步使用prepared最大AU与视频现有maximumFramedPayloadBytes、ADTS共享sizing形成series上界，并沿CompositionResourceBinder最终资源编译→output protocol product→mux adapter Binding传递；普通/动态输出需同步覆盖。

媒体series bounds已实现但未接线/构建：prepared最大AU→共享video/ADTS framing→PES headerBytes→实际TS payloadPacketCount→datagram数量，含加法及packet backing乘法溢出检查。PES serializer自身已复用headerBytes。双审FAIL发现AAC capability将maximumAccessUnit至少设8191，而ADTS最大完整帧8191含7字节头；直接将该prepared上界代入framer会拒绝所有当前AAC产品。非AAC codec校验遗漏已补，但尚未复审；该上界冲突未修，不能直接缩小事实或接入生产。后续须核验AAC编码器原始AU权威上界与ADTS可表示范围，保留原规格和错误语义。当前整体预算仍FAIL。

AAC上界冲突已按保守存储设计处理：不更改encoder envelope，ADTS maximumFramedPayloadBytes计算rawUpper+7并检查正数/溢出，8191原始上界对应8198存储；实际frame仍在resize前严格检查8191总帧长度，保留原错误。未取交集、截断、降低参数或假定1536-byte编码硬上界。本机FFmpeg aacenc.c:913分配8192*channels；6144-bit/channel是循环控制目标，存在break路径，不能替代严格事实；adtsenc.c:135按header+payload检查完整帧。外部源码只读。官方参考：https://ffmpeg.org/doxygen/trunk/aacenc_8c_source.html 。设计独立审查PASS；存储充分不等价于所有AU可封装。series planner另补AAC codec身份及packet backing乘法溢出校验；当前仍未接线/构建。

保守ADTS/媒体series修订已获两名独立复审PASS；2026-10-21 16:08 Release全量重建735项成功，configure/build=0，cache/Ninja/两个CLI产物齐全，git diff --check通过。realtime SHA256=F5772BDD3F41CBAA94A640E8261DC6F435D4F282E589186B0E8C47C9B8F1573B。未运行local CLI，未做此版本实流。series产品仍未接入生产预算。已核验接线必须同步更新两个planner构造入口、UDP/RTP clone以及UDP/RTP codec decode六处，不能只接合屏。PAT/PMT/PCR、最终图容量、共享lease和运行时预约仍待实施。

维护series上界已加入：PAT/PMT复用实际PsiSerializer及discontinuity=true的packetizer只读计数，PCR单包形状定义在实际builder并由runtime计数共享；字段仅代表单次series，不代表频率或owner总量。首审发现规划期vector/make_shared可抛bad_alloc，已用函数try block转allocationFailed，双独立复审均PASS。此最新增量尚未构建、接线或实流；完整预算仍未闭合。

seriesBounds生产传递已开始接线（未构建/未通过审查）：规划入口改接完整prepared emission及codec snapshot，用同一FFmpeg stream-config materializer形成视频事实；MediaProjectMpegTsRuntimeOutputPlan携带seriesBounds，普通video与AV planner、UDP/RTP clone、两种codec往返同步更新。内部9字段必须完整且unsigned可表示、audio presence匹配；几何重算校验仍未补齐，当前不能作为可信运行时容量使用。实际storage lease、final graph存活容量和mux Binding预约仍未接入。已请求双独立检查其他遗漏。

series传递及几何重验已完成本切片：codec apply/decode均调用统一validate，媒体series与维护packet计数共享原planner算法；六处构造完整，头使用prepared emission前置声明。双独立复审PASS，Release全量重建735项configure/build=0，四项必需产物齐全。尚未运行此版本实流；必须核验prepared snapshot extradata真实可用。纯decode只证明framed上界内部几何一致，权威来源仍为prepared planner。runtime lease及全owner预算尚未接通，整体FAIL保持。

## r291失败定位：音频进入下游前已超期

原规格realtime验证失败，CLI退出1，三源各自然退出0；无重入/快照。完整命令与必要诊断见realtime-composition-r291-validation.md。10项临时产物已逐项删除，五PID核验无残留。未运行local CLI。

同一audio canonical_dispatch=34853151927ns、master dispatch=40575234895ns：missing-video等待126108900ns后于40585123000ns提交，已晚9888105ns。下游mux dequeue=40585992100ns，materializer首次尝试=40586026700ns，晚10791805ns，blocked_attempts=0。因此失败前已存在scheduler提交迟到，不能将此例归因于materializer背压重试。画布85.69ms耗时尚不能单独证明因果；需追查缺路等待与视频产出延迟，保留原截止契约。

参考GStreamer全链路latency模型（https://gstreamer.freedesktop.org/documentation/additional/design/latency.html），多输入同步需要共同的可行延迟区间；本仓库有额外协议发送截止，不能直接照搬播放sink的丢帧/延后策略。当前进行共享scheduler只读根因复核，不新增默认延迟或放宽验收。TS存储预算接线、完整Windows矩阵、持续A/V和RKMPP仍未完成。

## r291恢复头取证增量

独立审查确认selectMediaHead缺任一路时返回nullopt，onProcess转无deadline的processWaiting；已有release deadline仅在选头后生效。r292同产物/参数初始三源各完整120秒自然退出0，VLC原生快照三格→全黑，CLI caller_stop=0；错误/压力0，最大缺视频等待40.3727ms，未复现r291，不能宣称修复。12项临时产物及五PID均已清理，见realtime-composition-r292-validation.md。

依据GstAggregator的live get_next_time与等待所有pad模式区分（https://gstreamer.freedesktop.org/documentation/base/gstaggregator.html#get_next_time），当前需明确缺路期间的顺序事实与截止处理，不能直接提前发另一头或放宽协议截止。现仅在late_output_commit补other generation/sequence/canonical dispatch/arrival，同generation才映射master dispatch；提交前读有效head，观察失败不改变原提交。两名独立审查均PASS。

2026-10-21 Release clean-first all-target全量构建735项成功，configure/build=0，cache/Ninja/两个CLI产物齐全，git diff --check通过；未执行local CLI。本增量尚未实流。下一轮必须保留canvas_compose_correlated、cuda_canvas_call、canonical/codec/head时序与protocol trace，归档后再清理。CUDA画布批完成同步可能占用生产时间，但目前未证明是r291根因，不删除完成屏障。调度修复、TS预算及整体验收仍未完成。

## r293重入异常退出：FAIL

最新诊断版本初始三源及所选源重入各完整120秒、FFmpeg均退出0，VLC原生画面三格→全黑→左下恢复。但CLI PID46196于重入期间发生0xC0000409异常，Windows事件PID0xB474吻合，ucrtbase.dll偏移0xa527e，BEX64附P9=7；无final report，不能证明资源归还。原生末次快照仍为左下画面，不能记为失源黑屏。调用方停止未发送：PID已消失。16项临时产物及六PID已清理。命令、事件和指标见realtime-composition-r293-validation.md。系统WER只剩Report.wer，所列临时mdmp不存在；后续用已存在cdb捕获原规格异常栈，不修改系统或外部FFmpeg。

独立只读调查还确认：通用canonical lineage无下一dispatch下界契约，不能直接用上一AU+duration解除等待。合屏连续区间与prepared no-reorder readback已有，但完整编码AU输出结束/未来输出frontier仍需权威证明；不得把r292事后视频时间当作提前发送权限。当前优先捕获崩溃栈，保留截止、预算和完整Windows→RKMPP目标。

## r294–r296异常诊断边界

中途附加cdb暂停全线程，三轮均破坏原实时截止；仅诊断，不能替代生产验收。r294在截止失败清理后出现C++ EH及0xc0000409第二次异常，Subcode7 FAST_FAIL_FATAL_APP_EXIT；首次异常命令未捕获second chance栈，调试器结束后CLI退出值也受影响。r295/r296配置第二次捕获后没有重现fast-fail，均正常错误退出1且payload归零；不能据此宣称崩溃修复。

三轮命令/关键原错误/清理清单分别见realtime-composition-r294-validation.md、r295、r296；各11项临时产物及全部精确PID已清理，源为中止清理退出，非完整验收。下一次从CLI启动前设置异常捕获，避免中途附加暂停；保留原规格初始源及重入，获取C++异常栈/fast-fail栈后再修改。微软sx事件配置依据：https://learn.microsoft.com/en-us/windows-hardware/drivers/debuggercmds/sx--sxd--sxe--sxi--sxn--sxr--sx---set-exceptions- 。本轮未修改生产代码或外部依赖。

## Release匹配符号修复

r297改为启动前配置cdb，但记录所有C++首次异常仍会暂停处理，早期截止失败；首次异常可能被正常处理，不作为崩溃根因。命令/原始地址/清理见realtime-composition-r297-validation.md；10项临时产物及五PID已清理，无VLC/重入。GetProcessById的退出码读到空值，wrapper退出0不能判CLI成功。

确认此前Release不生成PDB，目录中的CLI PDB为2026-07-08旧文件。CMake现仅对MSVC Release统一/Z7，app helper链接/DEBUG:FULL并显式/OPT:REF /OPT:ICF；保持/O2 /Ob2 /DNDEBUG与/INCREMENTAL:NO，Debug配置不改。依据微软/DEBUG文档对优化默认值的说明：https://learn.microsoft.com/en-us/cpp/build/reference/debug-generate-debug-info 。双独立审查均PASS。

2026-10-21 16:55:26 Release全量735项构建configure/build=0，四项必需产物齐全；realtime EXE6292480字节，PDB86413312字节。直接读取PE CodeView与MSF PDB stream1确认GUID=146b0265-447c-4633-8df5-19dc5724d7a0、Age=7一致；CDB .reload/ lmv及x能加载私有符号并定位MediaAvOutputSchedulerNode::commitReservedOutput。未运行local CLI。符号生成与匹配验证不代表媒体验收或崩溃修复；外部预编译FFmpeg无私有符号，优化内联/ICF仍限制部分栈解释。下一轮只在最终fatal异常保存栈，避免常规首次异常取证干扰实时运行。

## r299 原规格重入诊断收尾

三初始源及重入源均完整120秒、退出0；VLC原生快照确认三格→全黑→左下恢复→全黑。匹配符号的fatal诊断期间未复现崩溃，不能宣称r293修复。诊断结束后核验身份强制清理CLI/VLC，CDB随目标退出；CLI_EXIT=-1为清理结果，不是自然退出。普通权限进程空查询曾误判退出，提升权限CIM及原会话已纠正；重入旧PID被cl.exe复用，未触碰。实际命令、关键报告及17项临时产物清单见realtime-composition-r299-validation.md；产物已逐项删除。TS预算、调度截止、完整Windows及RKMPP门禁保持未完成。

## TS控制资源接线与r300失败

已通过project plan-source汇合activation/transport，runtime plan及mux持有同session/generation网络root；AV/VideoOnly shape同步，复用owner-thread purge并处理正常撤权等待。双独立最终源码PASS，最新Release全量735项exit0。该切片尚未预算化TS allocation。r300原规格三源输出后约33秒sender原截止超期94.872us，CLI1、payload0/34323预约释放一致；无late_output_commit，不能沿用r291根因。三个源中止-1、VLC清理、10项产物归档后删除，未获画面/失源/重入完整验收。实施契约见realtime-composition-ts-storage-closure.md，完整命令诊断见realtime-composition-r300-validation.md。整体门禁仍FAIL，未改外部依赖、未运行local或RKMPP。
