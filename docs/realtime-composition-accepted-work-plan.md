# 合屏已接受工作与周期规划闭合

状态：预约身份、协议事实及 prepared 登记/绑定/取消已实现并双审；r332 TS/RTP 与 r333 TS/UDP 重入观察完成。完整工作观察及其两个 planner 消费点尚未接入，周期门禁未通过。当前核验与范围修正在末节。

## 工业依据及适用边界

- [FFmpeg MPEG-TS mux](https://ffmpeg.org/doxygen/8.1/mpegtsenc_8c_source.html)支持包边界维护插入；不能据此推导本项目跨线程 FIFO 的完整截止时间保证。
- [WebRTC PacingController](https://webrtc.googlesource.com/src/+/c52e627c831ace2e6e6b0738715a51d851cd4865/modules/pacing/pacing_controller.cc)区分待发送队列与已发送债务；r323 的最后提交观察不能替代尚未提交工作。
- [RFC 2212](https://www.rfc-editor.org/rfc/rfc2212.html)的确定时延保证依赖已承诺服务速率、准入与实现误差界。当前共享域 capacity 是发送峰值约束，不能当作最低服务保证，也不能将本机观测最大延迟当作平台硬界。后续必要条件检查与充分保证必须分别标明。

## 当前代码证据

| 阶段 | 权威所有权/身份 | global 是否覆盖 |
| --- | --- | --- |
| mux 已生成未取出 | m_protocolBatches、series owner、reservationBegin/end | 否 |
| FileMux/protocol edge/materializer 当前项 | 同一 series 区间及 materialized offset | 未 reserve 部分否 |
| global reserve 成功，wire 未发布 | ReservationRecord identity/sequence 范围 | 是 |
| wire edge/sender 等待或发送 | 同一 global reservation | 是，不再另加一份 |
| global commit 后、protocol commit 前 | global 已扣除，series 尚未推进 | 存在交接瞬间，禁止独立读取后求和 |
| protocol commit 完成 | transaction committed，随后通知 producer | 已完成 |

定位：MediaTsMuxSession.cpp 的 takeScheduledBatch 在移交时扣减 m_protocolBatchBytes；MediaMpegTsWireDatagramMaterializer.cpp 的 complete 在下游才 reserve global 并取得 protocol transaction；MediaMpegTsUdpWireCommitTransaction.h 先 global commit 再 protocol commit。RTP 还可能附加 SR，protocol 数量不等于 wire 数量。

## 实施前必须确定的契约

1. 复用现有有界 series/continuity reservation ledger 附加 release、原 deadline、completion limit、实际几何和区间身份；不复制媒体、不另建媒体工作队列。
2. 在既有 UDP/RTP materializer 交接中关联 protocol 区间与 global reservation/sequence，包含附加报告。全程只计一份服务需求；绑定失败维持现有终止与撤销语义。
3. 形成一个一致的已接受工作观察。不能分别抓取 mux/global 数量相减，也不能将 global 已提交而 protocol 尚未更新的瞬间算成新增债务。锁顺序、交接状态和取消协议需设计审查后才能编码。
4. 元数据容量由现有并发预约边界推导，真实 sizeof/holder geometry 同步纳入准入；不得加经验容量。无新线程，无公共参数；沿现有 producer 唤醒及背压机制。
5. 延续同一 output authority、generation permit 与 purge 生命周期。旧代观察不得进入新代；shared scope 其他成员债务不能因本 session 清理而消失。

## 预约映射预审结论（未通过设计冻结）

独立时序审查暂未给设计PASS：continuity ledger按PID分组，仅保存CC预约，不能直接升格为跨PID发布账本。还须给出明确publication全序、已绑定/未绑定/部分物化/失败状态，以及附加SR的精确范围。不能把缺少global范围解释成零工作。

已继续核验的锁路径：

- TS protocol prefix commit持series mutex，再进入packetizer mutex；唤醒发生在series解锁后。
- RTP commit持RTP mutex，调用global commit；global内部解锁后才进入protocol/series/packetizer提交。不得新增packetizer→RTP的反向获取。
- global abandon在自身锁内只处理计数/poison，解锁后释放state；purge在锁内清自身预约与观察，解锁后通知waiter。
- UDP/RTP materializer先成功reserve global，再绑定protocol prefix，最终才发布wire。绑定元数据必须在发布前完成，失败仍终止，不能将绑定中间态当已完成映射。

当前RTP实际actions先可选SenderReport、再Media；SenderReport不消耗protocol entry。映射必须沿materializer真实action记录，分别保留endpoint、wire bytes、release/deadline及global sequence；不能仅保存一个protocol计数。末尾独立TerminalReport还需单独证明其分析边界。

优先评估固定锁序的短时元数据快照，不采用无界revision重试。所有创建、绑定、取消、purge和资源析构路径的完整锁序审计及临时快照预算尚未完成；上述主路径证据不能替代该审计。

## 周期分析门禁（范围保持）

仅列出 AU deadline 前的任务不充分：此前 release、但 deadline 在 AU 之后的维护也受阻塞，并可能继续影响后继周期。需要以成熟 busy-period/响应时间分析给出有限影响边界，或证明等价累计服务合同；没有证明不得用循环上限或经验周期数截断。

先完成上述已接受工作覆盖产品，再让 MediaTsPendingEmission::publicationDecision 与 writeDueMaintenance 复用同一 planner 消费。现有两个 next 维护事件仍仅局部必要条件检查；保持原期限和失败传播，不将其改称完整可调度保证。

## 活动范围映射方案（待实现复审）

已核验resource ledger validate强制commitBeforeNextReservation，TS/UDP和TS/RTP均经同一global reserve门禁。因此每个protocol预约可保存一个当前活动global范围及已确认protocol offset，无须另建逐包工作队列；这不代表旧回调已结束。

类型化范围须包含：预约publication identity、cursor revision、protocol begin/end、global范围identity及first/last sequence；可选SR与媒体区间由materializer实际actions给出，不能用wire数量替代protocol数量。范围尚未绑定与绑定后失败必须是不同状态，失败不退回可重试的未绑定状态。

UDP存在明确交接窗口：global commit解锁并通知后，materializer可建立下一预约，而旧protocol commit尚未执行。下一绑定只能在同代确认watermark覆盖旧global范围后推进确认offset；旧callback必须携带不可变范围identity及protocol区间，只推进自身协议确认，不能读取“当前活动范围”后清除新映射。RTP mutex收紧了此窗口，但公共方案不得依赖RTP特有锁。

后续一致观察以同一已接受工作元数据为唯一工作量来源，按global确认watermark裁剪已发送范围，不额外累加global backlog。已完成服务间隔仍由submittedService frontier衔接。尚未绑定的部分保留planner产生的协议几何服务下界；绑定后使用实际wire映射。此处仍是必要条件模型，不是OS或共享域最低服务保证。

独立时序预审对该有界活动范围方向有条件PASS；条件是证明旧callback不覆盖新范围、SR裁剪正确、所有失败/purge保持原语义。完整映射与一致快照尚未实现，未计为设计全门禁PASS。

## 验证顺序（保持原门禁）

设计冻结前由两名独立审查者检查工业等价性、交接并发和内存；实现冻结后再次双审。Windows Release 原规格 realtime 覆盖媒体、维护、源结束/重入、代切换和资源回收，VLC 原生快照；失败证据归档后清理。完整 Windows 门禁通过后才进入 RKMPP，不修外部 FFmpeg。

## 第一切片：预约身份

已在既有ContinuityReservation保存cursorRevision和publicationIdentity；后者由同owner跨PID单调计数器产生，表示预约顺序而非wire发送。prepareSeries在任何backing/ledger变更前检查耗尽，成功入账才递增，失败不复用身份；commit校验head cursor identity+revision并保留token owner校验。真实control probe及reservation sizeof/alignof自动流入既有storage planner，无新队列、公共参数或线程。两个独立审查者对实际代码均静态PASS。Release745项clean-first all-target重建成功，configure/build均exit0，四项必需产物已核验。未运行local CLI，尚无本切片realtime运行结果。

此切片未实现跨PID工作快照、global覆盖映射或完整周期分析，整体设计冻结条件保持未通过。

预约身份切片已完成r324原规格realtime：三路120秒及音视频120秒重入，177116数据报发布/提交一致，错误0、payload回零。4张VLC原生快照确认三格→黑→左下重入→黑；输入generation2不代表输出purge，sender仍generation1。指标及命令见r324，16项产物和6个进程已清理。映射/完整周期仍未完成。

## 第二切片：实际wire范围绑定

已在takeCommitTransaction接入实际global owner/reservation及leading control数量，核验对象身份、generation、非零reservation identity与媒体数量。series保存固定WireRange(first/firstMedia/last sequence、protocol begin/end、wire reservation identity)，替换前要求旧范围已global确认且protocol offset连续。UDP传0，RTP按prepared真实SR决策传1/0。旧callback不访问或清除当前wireRange，保持自身不可变区间。

bindPrepared在series锁外，避免失败prefix析构重入同锁；成功后重锁检查failed再发布。新增固定owner/range由实际SeriesState geometry probe计费，不新增逐包队列。两名独立审查者均对实际切片静态PASS；Release745项clean-first all-target重建成功，configure/build均exit0，四项必需产物核验齐全。未运行local CLI，未实测，尚未传播publication identity至映射，未提供完整服务几何或planner一致工作快照。

r325 TS/RTP实际运行完成，媒体/RTCP计数对账、错误0、payload回零；但初始VLC打开SDP后停止，修正为RTP URI时已失源，仅全黑快照，故画面验收未通过。详见r325；12项产物及5个进程已清理。下一轮保留原规格从启动使用RTP URI复验。

## r326 复验与当前阻塞

r326 从启动使用 VLC RTP URI，原生快照观察到三格视频，但 CLI 在约 99–103 秒、源仍运行时以 0xC0000409 崩溃。Windows 事件 1000 确认 ucrtbase.dll+0xa527e；尚无调用栈，不能归因到范围绑定或外部 FFmpeg。缺失最终资源/发送计数与失源重入证据，本轮 FAIL，Windows 门禁未通过。详见 realtime-composition-r326-validation.md；12 项产物已删除、五 PID 无残留。下一步定位致命终止调用栈，同时审查存储释放不变量；不改变期限、不扩大外部依赖修复范围。

## r327 与身份传播调查

r327 同规格 TS/RTP 在 CDB 标准堆下运行，三路源自然120秒退出0，原生VLC快照确认三格及失源全黑；未复现致命异常。CLI在诊断结束后清理退出-1，不是自然结束或完整门禁通过。14项临时产物与六PID已清理，见r327。r326崩溃仍未关闭。

源码确认 publicationIdentity 目前只在 continuity reservation 内部保存；MediaTsPacketCommitToken 只携带 owner/cursorIdentity/revision，prepareSeries 返回 token 时未传播发布身份。后续协议 series 和 WireRange 因而仍无法携带该跨PID身份。下一切片须从真实预约记录传播，不能另建计数器或以global sequence替代。

同时 prepare() 的非series路径由 MediaTsPacketBatchWriter::prepareNext 调用，它使用 active cursor 而不发布 continuity reservation。身份设计必须明确区分这两种已有语义，不能给非series token填造假的发布身份。移动后token仍须失效；协议prefix回调必须保持自己的不可变预约身份，不能读取被后续预约覆盖的“当前身份”。新增固定字段应随实际holder geometry进入现有准入产品；不增加公共参数或队列。此处为实施前调查，尚未编码或判定通过。
## 第三切片：预约发布身份传播

已实现内部 MediaTsPacketReservationIdentity(cursorIdentity、cursorRevision、publicationIdentity)。prepareSeries 从实际 continuity ledger 传播身份到 move-only token；prepare 非series保持无publication，移动清空原token。commit核对ledger publication，并保留owner/cursor/revision验证。

协议series保存当前身份，prefix和WireRange保存不可变副本；commit/abandon同时比较身份与区间。锁外bindPrepared后重锁复核身份和区间再发布。缺失已发布身份进入poison，后续预约同时failed；没有添加可重试fallback。身份仅owner内唯一，未来跨owner快照必须携带owner域。

新增固定字段沿实际token model、prefix model、SeriesState geometry进入既有预算。新model在旧model存活时先reserve，受到真实硬上限约束；这不证明完整容量规划或无阻塞进展。

独立审查 crash_release_audit 与 publication_identity_review_b 均给出本切片静态PASS。Release重建进行中，尚无本切片运行验收。完整一致快照、服务几何、周期分析及r326间歇崩溃仍未解决。
第三切片Release全量重建已成功：745项，configure/build exit0，四项必需产物已核验；未运行local。开始r328无调试器原规格TS/RTP复验。

第三切片r328原规格TS/RTP无调试器复验完成：三路源120秒exit0，三格→全黑，CLI caller_stop exit0。TS87280/98410668字节、RTP/RTCP总87323/99460960字节对账，错误与deadline miss为0，payload回零、reservation/release150240。13项产物及五PID已清理。未覆盖重入与完整矩阵，不关闭r326崩溃或完整周期门禁。

## 第四切片实施前核验：一致工作观察

独立调查确认 continuity 固定表只能锚定已发布预约：mux出队后预约仍存在，直至protocol commit。它不含 PendingEmission 的未发布AU尾部，也不含RTP TerminalReport。不能直接用该表总量替代全部已接受工作。

拟采用既有账本上的固定元数据，不新建媒体队列：

- 已发布预约：与prepareSeries同一原子发布边界写入原release/deadline/completion limit、区间、真实payload几何及身份。禁止空工作记录先发布后补齐。
- 物化交接：实际prepared wire描述在reserve前关联protocol身份，明确未绑定/准备绑定/已绑定/失败。global reserve至绑定期间不能以缺少映射解释为零工作，也不能直接另加global backlog。
- 已提交裁剪：以同一次global watermark裁去绑定范围中的已发送部分，覆盖global commit早于protocol commit的窗口；完成服务frontier单列。
- 未发布AU尾部：由mux单线程持有的PendingEmission和schedule提供原期限、剩余packet几何与发布约束。materializePrefix发布时即减少remainingPackets，因此已发布前缀不再加回；下一候选前缀必须从尾部划出，不能双计。
- RTP控制：实际SR按firstMediaSequence分界，保存endpoint与真实bytes/期限；TerminalReport无TS身份，需共享控制记录或完整证明终止阶段不再准入媒体。仅发现finishProtocol调用点不足以排除跨代/共享域影响。
- 快照：候选锁序packetizer→global，锁内只复制固定元数据，锁外由planner计算；不获取series/RTP锁，不使用无界revision重试。存储按真实ledger及控制项上限预分配并计入现有预算。

当前仍未冻结：reserve关联原子边界、所有purge/cancel/destructor锁路径、TerminalReport观察域、完整AU尾部表示及快照容量。不得先接入不完整求和到planner并宣称闭合。

工业对照补充：[WebRTC PrioritizedPacketQueue](https://webrtc.googlesource.com/src/+/refs/heads/main/modules/pacing/prioritized_packet_queue.h) 的大小与排队时间针对队内对象；本项目跨线程出队后的工作仍须独立保有身份，不能把其队列统计直接用作端到端工作量。[RFC2212](https://www.rfc-editor.org/rfc/rfc2212.html)的时延保证依赖路径服务合同；本切片只消除工作量遗漏/重复，不把峰值速率变为最低服务保证。
独立锁序核查 crash_release_audit：现有global commit在解锁后才进入protocol提交，abandon在解锁后reset；purge锁内clear当前仅整数ReservationRecord。notifyReservationWaiters仍持global→registry→wakeup锁，但notify仅事件/CV通知、不执行媒体回调。当前可维持RTP→series→packetizer→global→registry→wakeup方向；若新增记录持有series owner，必须重审pop/clear/destructor，不得沿用POD结论。此为条件性锁序依据，不是完整设计PASS。

### 第四切片最先实施的原子描述

独立调查收敛为协议值类型 MediaTsPacketReservationWork：generation、release/deadline/completionLimit、protocolBegin、datagramCount、packetCount、maximumPacketsPerDatagram。prepareSeries接受精确packetCount，不再截断maximumPackets；仅两个现有series调用点，非series prepare保持不变。ledger同一次push保存work与身份，不包含runtime对象引用。

create使用现有入参及reservedEntries/reservedPackets；reserveNextPrefix使用已选descriptor和window，series保存创建时已有分包宽度。packetizer锁内在任何变更前核验期限、非零几何、余量、区间加法及ceil(packetCount/width)==datagramCount。验证后才推进身份、pending和continuity。已发布后bind失败沿现有poison，不回收身份。只保存已预约协议事实，不提前声称完整快照；后续仍须覆盖AU尾部、SR、TerminalReport和原子wire绑定。当前r329运行期间尚未实施此切片。
r329完成原规格TS/RTP重入观察：四张VLC原生快照三格→黑→左下重入→黑；初始三源及重入源均120秒exit0、CLI caller_stop exit0。总提交176146/207357636字节对账，worker错误及deadline miss为0、payload回零。payload压力等待计数1在首轮失源附近出现，具体account未闭合；完整矩阵、内容AV及间歇崩溃仍未关闭。17项临时产物、六PID已清理。

第四切片已实施：新增协议层纯值MediaTsPacketReservationWork，prepareSeries在原子入账前核验原期限、精确包数、plan宽度及区间；work与身份同一ledger push发布。首次create及下一prefix均提供现有事实，后续descriptor核验整包与分组形状。真实ledger/series sizeof进入原预算，不新增独立队列或runtime owner引用。两名独立审查crash_release_audit、publication_identity_review_b均静态PASS；Release重建进行中。该切片不提供generation authority的独立绑定，不代表一致工作快照已完成。
第四切片Release745项全量重建成功，configure/build exit0、四产物齐全。r330续接时监控句柄与五PID消失，缺最终退出/资源证据，记录未通过并清理12项。r331同构建重跑：三源120秒exit0，三格→黑，CLI caller_stop exit0；总88167/103477332字节对账、错误及deadline miss零、payload回零、reservation/release150197。13项产物与五PID清理。完整一致快照、控制工作与周期分析仍未实现，间歇崩溃未关闭。

## 第五切片设计：实际prepared几何与范围绑定

两名独立调查确认：global单活动约束不能识别protocol identity，也不能排除多个prepared或TerminalReport。禁止用reservationActive直接归属某个TS预约。

协议层新增纯值prepared事实：媒体endpoint、count、regularPayloadBytes、lastPayloadBytes、release/deadline；可选前置SR保留独立endpoint/payloadBytes/release/deadline。payload取实际globalEntries，RTP头已包含，网络开销仍由既有global endpoint事实计算。运行时从实际outputInputs/globalEntries逐项核验统一媒体endpoint/时间、等宽非尾包与合法尾包后压缩，不能用平均包长或猜测SR。协议层不持有runtime数组或对象。

登记顺序：prepared backing/control全部构造成功→按不可变protocol identity在packetizer账本登记PreparedUnbound→仅noexcept转移现有prepared transaction所有权→global reserve→同步绑定真实global identity及first/media-first/last范围→把失败责任交给既有protocol commit prefix。UDP/RTP prepared holder复用既有protocol shared_ptr与protocolOffset承担RAII责任，不新增holder字段。登记最后执行，其后只noexcept move；未绑定析构通过buffer的不可变登记身份poison并notify。成功构造既有prefix后立即清空prepared protocol指针，后续失败交给prefix/global RAII；WouldBlock保留owner。UDP显式noexcept move清空来源，禁copy及不需要的move assignment；RTP先释放state锁再abandon，避免锁内最终析构。buffer在series锁内拒绝重复活动登记，offset与不可变身份共同核对。ledger不反持series，无强引用环。新增登记字段仅放入已有geometry probe覆盖的batch owner/ledger。

一致观察采用明确的未完成边界：packetizer锁内读取global同次快照，活动范围无可证明关联时返回typed Incomplete/InFlight；不得当零债务或立即自旋。绑定/失败完成在解锁后通知观察waiter，使用sequence-before-check，不能只依赖global容量释放通知。若未来要求该窗口也输出完整观察，必须在global ReservationRecord同一次reserve入账保存关联键/类别；当前bool不足以证明。

TerminalReport无TS预约，不能塞进该guard或伪造最后媒体身份。其独立记录/终止阶段证明以及未预约AU尾部仍属于完整后续工作。本切片设计方向已明确，尚未实现，也未被评为完整快照PASS。
实施前预算核验：prepared holder所属generation Data当前由普通make_unique分配，未找到实际sizeof/probe→planner计费路径，purge新旧Data还会并存。因此撤销新增内联guard字段的方案，采用上述现有owner槽复用；独立设计审查对该方向条件PASS。既有generation owner预算缺口保持未闭合，不能声称整体内存预算完成。另须同时保留旧Bound范围与新PreparedUnbound事实：物化prepare可先于上一global完成，不能用单variant覆盖旧范围。series只允许一个活动prepared登记，结合既有单active global约束形成固定槽上限，实施需核验取消及所有权转移。

本轮进一步核验：`MediaProtocolOutputGenerationData::prepareForGenerationPurge` 在释放旧 `data` 前分配 `m_prepared`；`purge` 与 `resetLifecycle` 都经过该路径，因此后续容量计算须覆盖同时存在的两个 Data，而不仅是正常态一个。该模板只用于 TS 与独立 RTP materializer，两者应共同处理；现有 `MediaTsSeriesStoragePlanner` 的 ledger/batch 实际 geometry 不能覆盖这两项普通分配。第五切片只增加已计费 ledger/batch 字段，未修复该既有缺口。

登记排他还须在 packetizer reservation record 中核验：单 materializer pending 或单 batch 字段均不能证明共享 series 的不同 prefix view 不会重复登记。真实数组核验包括每项 input/global endpoint、序列化尺寸、materializedAt，以及媒体统一 release/deadline、非尾等宽和合法尾包；原 protocol deadline/completionLimit 保持独立。TerminalReport 的原生准备路径具有独立 RTCP 字节及期限，不能从最后一条 TS ledger 记录代替推导。

第五切片代码已冻结，10个直接相关文件完成登记、实际范围绑定和取消责任转移。packetizer 按完整身份查找环形 ledger，并行保存 Bound/Prepared 两槽；runtime 核对真实数组，UDP/RTP 在 backing/control 准备完毕后登记，成功 prefix 接管后解除 prepared 责任。新增字段沿现有实际 ledger/batch geometry 计费，未新增公共参数。当前正在独立双审与 Release 全量重建，尚无本切片真实流结果；一致快照观察者及绑定通知尚未接入，不能以本切片替代完整准入分析。

第五切片双独立审查均PASS，Release全量745项重建configure/build exit0。r332原规格三源TS/RTP：源均120秒exit0，VLC原生快照三格→黑，CLI caller_stop exit0；85928包/99561328字节对账，deadline/worker错误零、payload回零、reservation/release146958。五PID和13项产物已清理，详见realtime-composition-r332-validation.md。下一步需覆盖同修改UDP与重入；完整snapshot/通知、TerminalReport、AU尾部、周期分析及既有崩溃/预算缺口仍未关闭。

## 完整观察接入前的交叉核验

两名独立审查澄清此前保守窗口判断：对同一 packetizer 已登记工作，持 packetizer 锁读取一次 global watermark 时，Bound 按 watermark 裁剪、Prepared 计一次且不加 global backlog，即使 reserve 已成功但 bind 未完成也不会漏计。该 Prepared 尚不能发布给 sender，bind 又需要同一 packetizer 锁。因此这个窗口本身不必返回 Incomplete；这仅是已覆盖子集的一致性证明，不是完整会话证明。

下一实施单元应同时交付有界观察产品及 `writeDueMaintenance`、`emitPendingThrough` 两个 planner 消费点，不能仅新增无消费者 getter。范围必须互斥：已绑定未提交、prepared 未绑定、已发布未 prepared、当前 PendingEmission 未发布尾部；候选 prefix 从尾部划出。`materializePrefix` 发布即扣减 remainingPackets，不能再次计入已发布部分。protocol 原期限与 wire 期限分别保留。

仍需在实施前闭合：TerminalReport 的独立有界控制登记、同 global 下其他 producer 的覆盖域、未 prepared RTP 的未来 SR 需求，以及上游已接受但尚未进入当前 cursor 的工作。当前 adapter 拒绝第二个 pending AU，`writeAccessUnit` 建立 cursor 后才报告成功，但这不能证明上游 DAG 队列为空。工作区上限必须由实际 ledger、producer/control 槽和尾部数量推导并计费；等待提交的条件检查与 waiter 登记须在同 owner 锁下完成，禁止无界重试。

该观察产品即使完整，也只支撑完整已接受工作的一致必要条件分析；现有服务速率与 frontier 不能证明共享执行环境的最低服务保证，全周期有限影响区间及实现误差仍需独立依据。r333 运行期间仅调查和记录，未修改生产代码。

r333同一Release构建完成UDP三源与完整120秒音视频重入：四快照三格→黑→左下重入→黑；四源自然exit0、CLI caller_stop exit0，178846数据报/207961088字节生成提交一致。deadline/worker/pressure错误零，payload回零、reserve/release262871。六PID及16项产物清理，详见realtime-composition-r333-validation.md。未关闭内容AV、输出purge、间歇崩溃或完整矩阵；下一实施以完整观察产品及两个planner消费点为目标，先解决控制与上游接受范围，不新增无消费者字段。

### 接受边界及实例域核验

本计划表格的上游边界是 TS mux 成功接收 AU。`MediaTsMuxSession::writeAccessUnit` 建立 cursor、pendingEmission 和 schedule 后转移所有权；adapter 成功后才更新 latestAcceptedEmission。`FileMuxNode::submitSessionInput` 明确把 WouldBlock 输入作为未接受项保留。因此当前完整 TS 观察无需遍历全部上游 DAG 队列；迟到候选仍按原 dispatch/emit 期限校验，上游存储和端到端时序另有各自门禁。此前对上游待接收项的担忧已由该调用链消除，不能无限扩大 accepted 的定义。

global 仅在 `MediaDatagramTransportPlanBuffer::create` 创建，经 transport source 分发；TS builder 为一个 mux/materializer，SeparateAvRtp 则为两个 RTP materializer 共享同 global。新 activation 创建新 global，旧 global purge 后即使仍被RAII持有也不是新代观察域。TS 单生产者事实不能推广到独立音视频 RTP。

未来 SR 并非没有任何规划事实：`MediaTsPrefixServicePlan` 已按真实 CNAME、RTCP端点及网络头计算每次 `durationForPayload` 一个可能报告的 allowance。它是保守需求而非实际报告；prepared 后用实际 SR 替代，禁止叠加。整个 AU/多次 prepare 不能仅调用一次而漏掉后续报告，必须依据实际批次划分核算，并区分需求上界与必要服务下界。

终止阶段前置校验已落入MediaTsSeriesOwnershipPlanner并双独立PASS；Release全量重建通过，r334原规格AV聚合TS/RTP三格→黑屏、源及CLI exit0，85258包/99186480字节对账，错误/deadline miss零、payload回零、reserve/release145908。13项产物及五PID清理。完整观察与两个消费点的实施设计见realtime-composition-accepted-work-implementation.md；控制阶段边界已明确，下一步接入有界一致观察及SR批次数服务产品，仍不宣称周期或整体目标完成。
