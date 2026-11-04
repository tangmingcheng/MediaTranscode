# 连续合屏音视频执行隔离设计（r158）
 
## 状态与目标

本轮为源码和工业实现调查后的设计收敛，尚未修改生产行为、构建或实流验证。r157现场画面有限PASS，整体仍75/FAIL。目标是消除视频canvas调用及视频输出背压对音频生成线程的直接阻塞；不承诺任意长GPU/驱动停顿下整条实时链路仍满足硬期限，不改变已批准的2–4源、连续黑场/静音、重入、原规格与Windows先于RKMPP门禁。

## 依据与选型

- [CUDA官方同步语义](https://docs.nvidia.com/cuda/cuda-driver-api/api-sync-behavior.html)：任何CUDA API可能因内部资源争用阻塞；Async名称不是主机调用时限保证。因此只改事件/异步拷贝不能提供音频执行隔离。
- [GStreamer queue](https://gstreamer.freedesktop.org/documentation/coreelements/queue.html)：独立流线程加有界队列提供执行解耦，队列满仍向上游背压。本项目复用现有DAG worker/channel，不能照搬默认容量或以丢帧规避。
- [FFmpeg当前CUDA源码](https://ffmpeg.org/doxygen/trunk/hwcontext__cuda_8c_source.html)：默认设备创建设置stream=NULL，设备传输与主机读回有不同同步边界。这是上游源码参考，不等同于本机二进制stream事实，不授权修改外部依赖。

当前代码证据：
1. MediaAvContinuousAggregateVideo.cpp::buildVideo在共同owner内同步compose。
2. MediaAvContinuousAggregateNode.cpp::onProcess依最早A/V游标选路，只有一个m_pending；视频不能提交时不能推进音频。
3. FFmpegNodeRuntime.cpp::processImpl在pending transfer未排出时不调用onProcess。只新增canvas任务节点会把同步阻塞变成视频队列满时的共同owner阻塞。
4. reserveOutputCommit/commitReservedOutput在真正帧发布时校验generation并推进游标。异步任务受理不等价于发布；若被purge，不能静默丢掉输出tick。
5. MediaRealtimeCompositionResourcePlanner当前为单aggregate规划，并以两类pending元数据最大值计量；拆分后两路pending可同时存在，必须改为各自上界及总和。

两名独立设计审查均否决“仅canvas节点”和“仅CUDA异步”作为完整隔离方案；推荐独立音频/视频DAG owner，适用性条件PASS，不是实现PASS。

## 目标职责与合同

| 组件 | 独占状态与工作 | 共享边界 |
| --- | --- | --- |
| 连续视频聚合节点 | 各源视频候选、非所选音频排空、canvas、视频pending/游标、视频贡献与提交 | 各源group、不可变output activation、仅元数据协调状态 |
| 连续音频聚合节点 | 所选源音频候选与sample容量、精确音频块组装、静音、音频pending/游标和提交 | 所选源group、同一output activation、仅元数据协调状态 |
| 单一激活权威 | 保留一次共同准备门禁和唯一output epoch/audio origin发布 | 复用MediaOutputEpochActivationCapability及MediaAtomicOutputTransaction，不另建时钟策略 |
| planner | 类型化角色、节点/通道容量、各owner retention/credits、公共时间与贡献规则 | 全部来自源、prepared readback及已有部署期限事实 |

实施优先让视频owner保留现有activation权威，音频owner发布有限准备事实；准备协调只存generation、ready、可信输入结束和时间边界，不存媒体帧、不执行CUDA、不等待媒体输出。音频owner获取同一已提交activation后独立按音频游标运行。初始化不得由音频owner自行重算release；不得把共同准备门禁降为仅音频或仅视频准备。

协调状态需要明确的短临界区和现有MediaNodeWakeup通知，通知在解锁后执行；不持协调锁调用CUDA、FFmpeg、reserveOutputCommit或阻塞操作。既有MediaAvStartupVideoPreparationState展示了本仓库状态快照与wakeup模式，但其源启动阶段不等价于本次输出准备，不能直接混用阶段。实现前须明确准备事实的generation失效检查和原子activation线性化点，不能添加循环媒体依赖或轮询线程。

## 所有权、purge及结束

- 视频owner仍同步完成compose后才释放其借用源帧，复用现有MediaVideoCanvasProducer和CUDA/RGA adapter；不新增异步任务请求、额外源引用或设备完成事件。
- 每路仅在自己的真正输出提交后推进游标。purge取消未提交帧时保持该游标，随后沿原贡献规则重建该tick；禁止跳帧或下游擅自替换黑场。
- 各源登记视频purge target；所选音频源同时登记音频target。非所选音频保留在视频owner排空，不能遗失该流原有生命周期推进。
- factory现有单aggregate动态转换及固定aggregate_source登记必须更新为明确的实际持有者集合；selected源barrier等待两者确认。GPU仍在执行时不能提前确认视频清理完成。
- 共同output epoch在源重入时保持，源generation分别失效和恢复；实际贡献发布保留现有源permit和output permit。
- 可信EOF与source loss严格区分。共享各源可信结束及最大输入end事实，两路各自提交EOF；连续失源仍按原合同输出黑场/静音。
- 任一路不可恢复失败仍失败整个会话，保留原首错。停止/abort由现有runtime加入worker并回收；不能新增detached线程或放宽超时。

## 资源和拓扑实施项

1. 拆分MediaAvContinuousAggregatePlan的运行角色及存储产品，保持公共时间、源映射与贡献算法唯一实现；避免复制两套aggregate或在一个大类到处按role分支。
2. 从现Video/Audio文件分别迁移对应owner实现，激活/共享协调独立命名。两者各自继承既有节点发布机制，各有独立pending与输入候选。
3. 更新MediaRealtimeCompositionGraphBuilder、SourceTargets、ResourceBinder、RuntimeDependencies、RuntimeNodeFactory、输出域registration与processing members；同一拓扑用于Windows/RKMPP。
4. 更新MediaAvAggregateFrameCreditPlanner、CompositionResourcePlanner、引用存储/最终账本编译及验证。视频pool仍按prepared retention；音频仍按真实cadence和既有latency。两节点、两个pending、协调对象、额外控制引用必须全部计入。
5. 复核现有ledger的producer账户映射，保证视频owner容量满不能借用或占尽音频owner已规划额度；不能仅看总预算或新增经验容量。
6. 不新增公共CLI/API配置；旧枚举数值稳定，新增内部节点种类需同步检查所有穷举分支。
7. 冻结后两名未参与实现的智能体交叉审查：逐项对照独立流线程/有界背压模型、共享activation、generation清理及账本。任一未明确PASS不得进入完成结论。

## 验证与剩余风险

实现后全量Release clean-first/all-target（已批准300秒），仅运行realtime。原三源120秒规格验证现场/输出、CPU/内存、A/V telemetry、原始失败、失源黑场静音、所选源及全源重入、资源回收；扩大至原2–4源/异构矩阵和C入口/旧路径要求。Windows完整门禁通过后才进入RKMPP。

诊断必须分开记录两个owner时间线与各自游标；r157失败datagram还缺精确deadline/媒体关联，后续需复用既有sequence和不可变AU诊断确认，而不是把分线程后一次未复现宣称根因修复。下游scheduler/mux仍会约束A/V共同出口，资源竞争、设备长停顿、尾部排空和内容A/V同步仍为开放风险。

## 实施前协调合同细化

### 准备与激活线性化

音频ready事实必须包含所选source group、source generation、已验证的audio origin、有效首个候选区间及prepared输出音频合同身份。发布条件是codec合同已验证、候选非空且sample/metadata界限有效。激活前该候选由音频owner保留，不随时间自行淘汰；ready对同一generation保持有效，只有该源purge或会话停止使其失效。

视频owner先取得ready不可变快照并释放协调锁，再按planner源索引顺序持有所有源的精确generation permit，随后建立现有原子activation输出容量事务。所选音频generation必须与对应视频源generation一致；源transition已开始或代际不符则释放所有预约、保持未激活，等待准备更新。MediaAvEpochTransitionService::reserveOutputCommit持有源transition mutex直至reservation析构，因此源开始purge不能穿过该验证区间。持permit期间禁止再次调用同一group的snapshot/playbackEpoch以免递归锁。

release及activation值在锁外预构造，仍采用现有now+preparationLead与各源masterRelease的最大值，不新增时序策略。沿现有顺序activateInitial成功后调用不可失败的commitReserved发布同一activation；所有可失败分配/容量预约必须前置。音频owner只通过新增的既有类型activation控制输入接收这次事务的事件，不能靠output group已Locked或ready通知提前输出。这样不再需要共享“已激活”布尔状态，队列发布即音频可见线性化点。事件重复时同值幂等、异值失败；输出域不允许重入改epoch。

音频ready通知只唤醒视频owner检查事实。注册wakeup与获取现有ready快照必须在同一个短临界区完成，写入后解锁再notify，复用现有wakeup持久通知机制。epoch事件的等待/唤醒复用MediaChannel。停止撤销未激活ready并通知；activateInitial之前失败不发布事件；成功激活后发生会话取消仍沿原runtime abort，不把取消当成功启动。

### 锁序与purge

协调锁不得与channel/queue锁、source/output permit锁、purge target内部锁嵌套。快照复制后释放协调锁。正常媒体发布顺序以FFmpegNodeRuntime::transferOrDefer及drainPendingTransfers为准：按源索引source permit→output permit→channel/queue事务。初始activation沿既有activateInitial/commitReserved协议，但先持source permits，再预约activation channels，最后调用唯一output激活权威；此路径仅在output尚未激活时存在，必须证明任何输出消费者都不能在收到activation事件前发布媒体，且没有持output锁再索取activation channel的反向路径。禁止反向锁序、在permit中执行GPU或等待purge。

每个owner在处理自己基类pending之前服务purge。音频owner使ready旧代失效、清所选源候选/旧pending并确认自身purge后，可继续按已有output epoch生成静音；不能等待视频owner GPU清理或整个source barrier完成。旧真实贡献被group outputPermitted/精确permit拒绝。视频owner返回同步GPU调用后清自己状态再确认；不释放运行中帧，也不等待音频确认。协调事实更新与purge complete分两段，均不持另一类锁。

### 可信结束封闭

每个owner只有消费到其负责输入的可信EOF，且把该输入所有已消费区间end纳入事实后，才能发布对应sealed结束事实。所选音频由音频owner seal；各视频及非所选音频由视频owner seal。source loss/RTCP失联不是sealed EOF。未封闭的end按source generation保存，purge撤销旧代事实；已封闭输入不允许重新发布新代媒体，矛盾时失败。

共同最终end只有全部计划输入sealed后才冻结，为有效输入end最大值（无有效输入保持空）。两路根据同一冻结值与自身tick独立发布EOF；不得在另一owner尚未上报尾部end时提前EOF。整体结束仍由既有DAG下游EOF/排空与会话完成逻辑决定，不能靠任一owner结束提前报告完成。

### 资源核验补充

当前CompositionResourcePlanner::addOwner已按node/stream/payload建立独立producer账户，MediaGraphPayloadCreditLedger::accountForProducer按该键映射，不能误称现有实现只有全局池。拆分需重映射两个owner并保留独立账户；校验生产者覆盖唯一、账户预算总和与总admission一致。

准备期ready仅引用不可变元数据，候选媒体留在所属owner既有候选容量；禁止复制候选队列。账本按“两个节点固定存储+全部候选+两路可同时存活pending+ready快照/activation控制队列+canvas既有pool/header+编码器prepared retention”逐项覆盖，重复引用按既有引用存储规则计量。不能只把旧max改sum而漏掉协调和控制引用；不通过加经验余量补足。

## 本轮审查结论

两名独立审查在主审纠正permit/channel锁序后重新读取调用链，均明确设计合同PASS。此前锁序错误的旧PASS不沿用。初始activation全部消费者无反向等待仍是实现阶段必须证明的义务，不把文档要求当已证明无死锁。本轮未改生产、未构建和实流；整体仍75/FAIL。

## r159实施进度

时间换算已独立到sync/MediaAvAggregateTimeline.h，原三方法移除，全部调用显式传plan/epoch。两独立源码等价PASS、全量Release configure/build0；没有运行本轮媒体链路，尚未拆分owner或完成任何运行修复。下一实施项为独立音频owner与共同准备事实，必须连同注册/purge/资源产品完成后才能宣称隔离成立。

## r160实施中：双执行节点接线与审查

已拆分独立音频aggregate节点，视频节点保留canvas和唯一activation；共享协调对象只存ready、generation和sealed结束事实。builder、runtime注册、所选源双purge、producer账户及两路pending求和已接线，尚未通过构建和真实流。首次编译失败；完整日志重现定位为协调头缺少MediaStreamKind直接依赖，已补齐。生命周期审查发现stop未接入，已在两owner清理前撤销协调状态，并补音频基类start失败清理，第一审查者复审通过。第二审查者发现音频ring调用连续view在pop_front后会抛异常，待改为直接迭代有界存储；当前整体源码审查FAIL。

下一步修复ring接口、完成全量Release重建与双审，再运行原规格realtime三源120秒链路。r157发送deadline失败仍开放，不能用源码隔离或编译成功替代期限、排空、内容AV和跨平台验收；整体仍75/FAIL。

### r160修复复审与最终构建

音频metadata helper已直接接收固定容量ring的const引用并逻辑迭代，两调用不再使用连续view，无媒体复制。两名独立智能体对最终源码均明确PASS。最终Release全量clean-first/all-target成功，configure/build0（session96110）；前一头依赖修复构建亦0，之前缺依赖失败保留。现按原三源规格进行realtime实流，结果见C入口实施记录；源码PASS不等于期限根因或全部门禁关闭。

### r160运行闭环（有限通过）

原三源及所选AV重入四次完整120秒均exit0；现场三格→黑→所选格恢复、payload184686/184686归零及wire176512RTP序号/DTS/AV解码通过，两独立审查有限PASS。canvas100.767ms、音频built全程最大26.6717ms支持本轮聚合隔离；scheduler仍90.6241ms，VLC有late及音频buffer丢弃告警，尾1包228B/AAC2帧未drain。完整目标仍75/FAIL。28件临时产物与七PID已核验清理，命令及结果详见C入口记录。
