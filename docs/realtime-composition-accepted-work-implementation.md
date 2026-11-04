# 已接受工作观察接入

状态：完整观察、服务产品和两处消费点已实现并双审；Release重建成功。r337正常TS/RTP及r338 UDP/重入观察通过；r336捕获的共享scheduler诊断自锁已修复，r339补充诊断已命中目标分支并正常错误清理。完整周期、其余矩阵与整体合屏门禁仍未完成。

## 范围与依据

覆盖 TS mux 成功接收 AU 后至 sender 提交的工作。未接受的 FileMux WouldBlock 输入不属于本观察。复用现有 DAG、planner、预约和唤醒，不新增公共字段、媒体队列或线程。

[WebRTC PacingController](https://webrtc.googlesource.com/src/+/c52e627c831ace2e6e6b0738715a51d851cd4865/modules/pacing/pacing_controller.cc)区分排队工作与发送债务；本项目跨线程交接以不可变预约身份保持一份工作。[RFC 2212](https://www.rfc-editor.org/rfc/rfc2212.html)要求已承诺服务及误差界，当前峰值速率和观测 frontier 不满足充分保证前提；本单元不将必要检查改称确定时延保证。

## 一致观察与所有权

1. 协议值记录包含 identity、原 protocol work、可选 Bound/Prepared。mux 单线程先读取自己的未发布 AU 尾部，然后短时持 packetizer 锁复制 ledger，并在同一锁域读取一次 global snapshot；禁止进入 series/RTP 锁。
2. 锁外按 publicationIdentity 排序和规划。Bound 按同次 watermark 裁剪，Prepared 计一次，已发布未 prepared 区间由预约末端减去两者覆盖范围；不再加 global.currentWireBytes。
3. 当前 AU 尾部来自 remainingPackets，候选 prefix 从尾部划出，不能重复加服务时间。publication 量子保持单数据报，不能为简化计算把整 AU 当成下一批。
4. scratch 使用 maximumLiveSeries 条记录。Factory::prepare 在 owner 锁下检查 liveSeries 硬上限，成功 payload/PCR cursor 增加计数，析构扣减；每 cursor 同时一个 pending reservation。复制仍检查容量，禁止增长。
5. scratch holder 纳入实际 packetizer control probe，backing 按 observation sizeof/alignof 加入 MediaTsSeriesStoragePlan 与 create 的相同一次准入。借用限定 mux 单读者、下一次观察前消费完；如 API 暴露可重入观察，则必须用独占借用责任保证 scratch 不被覆盖。

## 服务产品与消费点

扩展已有 MediaTsPrefixServicePlan：实际 Bound/Prepared 使用 endpoint 与真实 serialized payload，不再次加 RTP 头；未 prepared 使用协议几何及可能 SR 次数。现有 durationForPayload 每调用只预留一个 SR，不能直接用于整个 AU：当前尾部每次发布一个数据报，可能 SR 次数等于其剩余数据报数。maintenance 的实际分组上限须来自同源 materializer batch plan，不得从存储容量猜测。可能 SR 是保守需求，相关拒绝不能诊断为已证明物理不可发送。

新增专职 accepted-work planner，在锁外验证 generation/frontier，按已发布 FIFO 工作推进服务时刻并保留每段 release、wire deadline、protocol deadline/completionLimit。输出类型化的已发布工作可用时刻，禁止伪造已提交服务记录。下一 prefix release 只能作为尾部的乐观下界，完整周期干扰仍须后续独立分析。

- emitPendingThrough：同一次观察替代 submittedServiceObservation；检查已发布工作和全部 AU 尾部，候选独立交给现有 prefix planner。
- writeDueMaintenance：使用同一观察及服务产品，检查已发布工作、本组维护和当前全部 AU 尾部；不只检查下一媒体数据报。

等待实际提交时复用 owner 锁下的条件检查/登记及 sequence-before-check 唤醒。取消、poison、purge保持原失败，禁止轮询、无界revision重试或回退至零债务。

## 冻结前剩余条件

TS builder 的单 materializer/global 不能推广到 SeparateAvRtp。须核验 TS 媒体规划与 TerminalReport 准备的阶段互斥，以及 sealed topology 单媒体入边。TerminalReport可能与旧媒体global预约共存，不能假设队列已经排空；已有真实字节/期限的sender pacing门禁需确认是否覆盖终止分析，不能自行省略或重复实现。

独立审查结果：阶段证明当前FAIL。FileMux在多个EncodedPacket输入尚未全部结束时仍可转发单路terminal，TS packet端口允许多连接，而最终TS ownership/storage planner未核验单入边。实际builder使用聚合adapter并不足以证明最终图不变量。先在已有最终拓扑planner验证单个正确聚合的EncodedPacket输入，保持通用FileMux语义；随后可依据成功finish无pending、adapter Finished及FIFO终止顺序建立TS阶段互斥。Terminal沿已有真实期限、global reserve和sender pacing/scope门禁处理，无证据要求新建重复终止算法。

前置修复已在 `MediaTsSeriesOwnershipPlanner.cpp` 实施并冻结：唯一mux packet输入来自同plan的TS adapter，adapter唯一scheduled输入追溯到既有单视频scheduler或AV双EOF scheduler/serialized router，并核验session/streamSet。两名独立审查均PASS；上述FAIL对应修复前状态。此约束封闭正常EOF提前传播反例，不假设旧global媒体已经排空。Release全量重建成功，r334原规格AV聚合TS/RTP三格→黑屏、caller_stop退出0，错误/deadline miss零、payload回零；13项产物与五PID清理。VideoOnly实流及负向构图未在本轮运行，详见r334记录。

改动完成后两名未参与实现的独立审查须同时PASS，再执行Release全量重建及原规格realtime TS/UDP、TS/RTP、失源/重入验证；VLC原生快照、发送对账、CPU/内存、时钟遥测与退出原因归档后清理本轮产物。已有间歇崩溃、generation Data预算、内容AV及跨平台门禁保持未关闭。

## 本轮边界预审

两名独立审查确认：materializer 的 maximumBytes 仅拆同一 prepared transaction 的输出 partition，不增加 SR 决策；prepare 次数按同一 protocol view 的 maximumDatagrams 计算，不得跨媒体单数据报 view 合并。已绑定范围按同次 global watermark 裁剪，publication identity 本身不是已发生的 wire 排序证据。可能 SR 只构成保守准入需求。

正常 mux write/poll 的 generation/session permit 可阻止同代 purge 并发清空 global；冻结复审仍须核对该 permit 覆盖 scratch 借用及全部 planner 消费。global.poisoned=false 单独不能证明未 purge。此次为边界预审，不是实现 PASS。

FIFO 追加预审已确认当前封闭 TS 拓扑：mux 的 PAT→PMT→PCR 及媒体身份发布后同步入队；takeScheduledBatch 取队首，adapter 优先排空，FileMux pending transfer 先于下一次处理，materializer 排尽 pendingProtocolBatch 才取下一项。故同 owner/generation 的未绑定工作可按 publicationIdentity 排序；已绑定部分仍按实际 global sequence 裁剪。该结论不适用于多 producer、多 materializer 或重排队列，发布后入队失败须保留 poison 路径。

## 完整实现首轮复审

15 个生产文件已完成一致观察、实际 scratch 计费、服务产品及两处消费点接入。首轮独立复审发现两个阻断：Bound 范围错误地用提交 nextGlobalSequence 校验，合法未提交范围会失败；acceptedObservationGeometry 漏填 count，导致实际 alignment 为零。当前判定 FAIL，修复与复审中；尚未构建或运行该版本。范围校验应复用 reserve 更新的 lastMaterializedSequence，不改变 global 提交语义；存储 geometry 按真实三字段结构初始化。

两项阻断已修复并重新冻结：Bound 上界使用同次 lastMaterializedSequence，提交裁剪仍用 lastCommittedSequence；scratch geometry 正确为 count=1、实际 sizeof 与 alignof。crash_release_audit 与 publication_identity_review_b 均实际复审并明确静态 PASS，包含 SR-only、部分提交、global 已完成而 protocol callback 尚未完成窗口。Release 全量重建已启动，真实复验记录见 realtime-composition-r335-validation.md。

本单元Release全量746项重建成功，但r335真实链路0xc0000409退出，不能验收通过。随后r336同构建CDB捕获共享AV scheduler晚提交诊断的transition mutex重入，snapshot noexcept触发terminate；真实栈已归档。该缺陷修复及同规格复验优先处理，完整观察仍待运行验收。r335/r336临时产物和进程已清理。

r337修复后Release三源TS/RTP：三源120秒自然exit0，VLC原生三格→全黑，CLI caller_stop exit0；84295数据报/99199816字节对账，sender期限/压力及worker错误零、payload回零、reserve/release143773。13产物及五PID清理。late_output_commit含other映射分支0条，修复触发验证仍未完成；下一步同规格UDP和重入。

r338同Release完成UDP及完整120秒输入重入，四原生快照三格→黑→左下重入→黑；四源自然exit0、CLI caller_stop exit0，173073数据报/203483116字节对账，期限/压力/worker错误零，payload回零、reserve/release250712。16产物及六PID清理。原故障分支仍0次，下一步补充单变量调试器延迟诊断，不替代未干预真实链路。

下一规划单元已由只读代码核验定位：maintenanceObligations仍固定下一PSI/PCR两项，validateTail未插入尾部执行期间后继维护，有限列表反推latest-start不能证明列表之后干扰已消退。需依据真实周期/原期限/批次几何形成周期需求产品，并先证明有限影响区间，不能用固定周期数或经验迭代上限截断。峰值必要检查、possible-SR保守准入与最低服务承诺保持区分；已确定mux接受边界不再向上游队列扩张。

r339以同Release/原规格源做单次CDB诊断，实际暂停约315ms，命中同代other映射并输出正确数值；随后保持原AU期限失败，CLI正常exit1、payload回零、reserve/release1487，无原自锁fatal。独立核验确认目标分支PASS，不能记为未干预整链通过。三源均完整120秒exit0，12产物与六PID清理。具体原始断点栈、映射字段、错误及边界见r339记录。
