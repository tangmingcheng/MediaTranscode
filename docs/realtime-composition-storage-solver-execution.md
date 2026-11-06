# 合屏联合存储 solver 实施决策

状态：2026-11-06，基于 `bd1cbfe0` 核验。源、拓扑、residence 与费用的固定目录刷新已具备，连接、拓扑、引用与费用目录已接入预算资源；借用固定源事实的候选工作区、合法容量基线及具体 solver 尚未完成；`MediaRealtimeCompositionPreparer.cpp` 的 `freezeBatch(rtpSources, nullptr)` 仍阻止 SourcePresentation 启动。本文件只收敛接线方案，不代表实流通过。

## 容量语义与依据

采用有限容量、原预缓冲要求、超限显式失败。未知展示点密度不能由输出 FPS、压缩 AU 数、decoder retention 或 probe 已观察帧数补齐；有限 Nv 不保证任意输入都能覆盖 500 ms。保留既有 10 秒 acquisition deadline 和 500 ms preroll，不以缩短它们换取启动成功。

[GStreamer queue](https://gstreamer.freedesktop.org/documentation/coreelements/queue.html) 将 buffer、byte、time 上限独立执行，任一上限可先达到。这支持上述有界缓冲语义；不提供本项目容量分配的性能保证。[DRF 原论文](https://www.usenix.org/legacy/event/nsdi11/tech/full_papers/Ghodsi.pdf) 的多资源需求模型不能直接等同于本项目的非线性、异构对象费用，不采用其完整公平性结论。

## 已核验的固定轴和候选下界

| 产品 | 事实与边界 |
| --- | --- |
| 每流 prefix | 冻结后不再增长。逻辑下界为 max(1, held.admittedUnits, held.prefixObjects)，compressed.maximumUnits 与其相等；held.history 的实际背板费用仍保留。不得随 Nv 扩大。 |
| 音频 startup | 完全沿用 session.audioRetention 的 units、unitBytes、bytes，不参与视频搜索。 |
| 每流 acquiring | 至少 max(1, held.acquiringObjects)。这只保护现有对象，不保证任意 RTCP 等待期间永不耗尽。 |
| 每流 outputs | 至少 max(held.outputObjects, completionOutputObjects(stream))。后者 N×(A+2)+1 只界定单次串行完成增量，不证明当前占用加未来连续批次均可容纳。 |
| 视频 startup Nv | 正展示跨度至少需要两个不同展示点，Nv≥2 仅为必要下界，不是充足的预缓冲保证。 |
| source queues | 必需字段必须为正；还须通过原子发布、producer residence 和真实拓扑进展校验。不能统一填 1 后就宣称基线可执行。 |

依据：`MediaPreparedRtpSourceSession::validateStorageAdmissionLocked`、`MediaPreparedRtpSourceStoragePlanner::completionOutputObjects`、`MediaAvStartupWindowSelector`、`MediaAvSyncStartupPolicyPlanner`、`MediaCompositionProducerResidencePlanner`。session 的冻结状态决定 prefix；实际 source/kernel 和 edge planner 决定队列与 route 合同。

## 容量选择目标（设计条件通过，尚未实现）

依据下文补充调查，两位审查者均给出设计条件 PASS：对通过全部合法性与进展检查的基线 L，只对独立可增长的数量轴使用 x_i(k)=L_i+k，寻找全源联合费用可承受的最大整数 k。prefix 和音频 startup 固定；由其他轴派生的队列容量不重复当作独立轴。各字段表示范围共同限制 k，不允许静默截断个别轴。

这是最大共同数量增量目标，不是字节公平、吞吐最优、完整逐级 max-min 公平或任意多维最优。k+1 不可行时允许余量闲置，不再按源顺序临时分配。必须先闭合合法基线和队列派生关系，再在固定 codec/filter、成员、readiness、拓扑 shape 下验证费用单调性；非法配置、平台不支持和超预算须区分，不能都作为二分搜索的“太大”。

## 下一实施批次必须形成的完整调用链

1. 具体 solver 借用已准备的源事实、video plan、output runtime 和 canvas geometry；在 freeze 的全部源锁下仅读取传入 snapshots，不再调用 session accessor。
2. 同一次候选计算生成 startup、queues；复用 `MediaRealtimeEdgePolicyPlanner` 与 `MediaStartupRoutePlanner::bind`，形成 `MediaAvSourceCapacityFacts`。复用 source kernel、aggregate planner、topology kernel，更新全部 edge policy 和 incident capacity，不复用陈旧数值。
3. 在基线、单调性及工作区预算条件全部落实后，用实际 topology/residence 调用 `MediaPreparedSourceCandidateCostPlanner::plan`。所有源联合求和；root 的 probe/output/canvas、batch 六数组只按所属域计一次。新增候选 workspace 必须显式受预算约束，不能假称已有六数组已覆盖嵌套 vector/string。
4. 搜索结果生成恰好 sourceCount 个 admission，放入 batch.owner 上的固定容量结果。替换实际 nullptr；由 freeze 原事务检查后一次性发布，任一步失败保持未冻结状态与原错误。
5. sealed 后用现有共同 planner 独立复算 topology、费用和容量一致性。原规格 Windows 四源 realtime/HEVC 重排、VLC 快照、失源黑屏/静音与重入通过后再做 RKMPP 实流。

未闭合项：队列派生及合法基线的完整进展证明、候选工作区计费、具体 solver 与生产接线、sealed 完整一致性。不要继续提交只有独立费用 helper 而不推进这些调用关系的修改。

## 初次审查与当时验证（后续修正见下一节）

presentation_plan_review：上述共同增量目标条件 PASS，要求合法基线、固定 shape 单调性、字段上限及准确声明目标边界。timing_path_review：确认表中事实下界及有限容量尝试启动路径；明确反对“构建前保证正 preroll 必定成功”。复审时 timing_path_review 对共同 k 策略判 FAIL：上述工业资料不直接证明异构数量轴同步增加的适用性；presentation_plan_review 为条件 PASS。故整体设计审查未通过，不能据此实现该搜索策略；必须补充适用依据或选择已有充分依据的策略。事实核验与设计讨论不能记作代码或实流 PASS。

本轮 RKMPP 8 核 all-target 增量构建退出 0，Ninja 报 no work to do；未修改生产代码，未产生新媒体测试或临时产物。没有推送、合并、部署或外部依赖修改。RKMPP 身份传播问题仍单独保留，不能由 Windows solver 接通推定已解决。

## 2026-11-05 补充调查与判定修正

保留上一节初次 FAIL 的历史。timing_path_review 补充调查后修正：要求第三方项目采用完全相同的共同 k 目标过严。项目可定义确定性的数量上限选择目标；这不等于自行发明媒体处理算法。[GStreamer allocation](https://gstreamer.freedesktop.org/documentation/plugin-development/advanced/allocation.html) 在启动前协商 min/max、大小与 allocator，由上游决定具体策略；[Flink network buffers](https://nightlies.apache.org/flink/flink-docs-release-1.20/docs/deployment/memory/network_mem_tuning/) 区分最低进展需求和额外缓冲。本项目只复用静态最低需求与额外容量分离，不引入 Flink 动态 floating 分配。

当前双方均为设计条件 PASS：合法基线、独立/派生轴、全费用单调性、固定工作区预算、最终 sealed 复算必须在代码中落实。标准二分只用于已证明单调的合法区间。尚未有具体 solver 或对应实流 PASS。

工作区实施方向：freeze 前编译固定身份和证据，freeze 内只更新容量、edge policy/incident capacities、residence 数量及 root 数量。既有数值 evaluate 可复用；仍缺稳定 root→owner 映射及完整更新入口。普通 vector/string 提前构造不等于免费，必须有构造前的峰值预算，或改用预算资源上的精确数组和借用字符串。禁止把预编译产品实测 capacity 当成事前硬界，也不能遗漏零数量 owner 后续变为非零的 catalog 成员。

## 2026-11-05 固定引用目录数量更新已接入共同规划

已实现显式 LiveOwners/ReachableOwners 选择：最终图 readback 保持 LiveOwners，lineage/packet 候选目录使用 ReachableOwners，保留可达的零数量 owner，并记录其原 view slot；不是过滤后贡献数组下标。BFS 与费用总额算法不变，零值与 domain 中不存在的 slot 继续区分。

共同 rebind 按原 slot 核验 kind、完整 connection 索引和 retaining positions，随后更新数量并调用原 checked evaluate；实际 plan() 已消费这条路径。FrameWrapper 接受合法零 holder，Construction 仍严格等于 1；固定单槽证据异常直接失败，不通过重新赋 1 掩盖。成功路径不重新分配目录；失败允许部分数量已改，但整个候选必须作废，不能作为预算过大继续搜索。

适用前提是同一固定拓扑、domain、producer 与 owner membership，可达性不在 rebind 中重新推导。尚不能把该 API 用于任意替换图。完整 solver 工作区必须保管这些不变身份；topology/residence 数量更新、完整工作区计费、候选费用总更新与 freeze 入口仍未完成。本项是引用身份到费用的实际接线，不是 solver 或实流验收通过。

## 2026-11-05 持有数量编译复用

ReferenceResidence 的首次 compile 与固定工作区 rebind 现在复用同一编译遍历；编译期 writer 分别执行构造写入或身份检查后的数量更新。typed startup node/event 保持顺序、position/connection/coordinator、payload/form 与 video/audio 可选成员；aggregate 保持原 Shared 身份及持有位置；ordinary 槽保持存在性，optional{0} 不当作缺席。原 route 端口/重复匹配/policy/stream 校验、节点持有数量及 encoder checked add 完整复用。更新路径不 append/resize，失败后候选整体作废。

计算移到独立 Compiler.cpp；Topology::position 同时供原构图、投影与编译流程使用，未增加第二套角色查找算法。双独立源码审查通过。仍需完成 topology policy/incident 数量更新、容量基线与工作区分配计费、全部候选费用重算及具体 freeze solver；当前不得宣称完整候选入口已开放。

## 2026-11-05 固定连接策略更新

输入、视频和音频连接声明采用共同构造/更新遍历；更新模式按顺序核对端点、端口、payload、stream、label 及适用的 phase，只替换 policy，不增长容器。源分支入口核对完整视频/音频 shape 与 externalCorrection；SourcePresentation 的 preparedVideoFrame 政策和原连接选择保持。失败保留首个错误并使整个候选作废。原 plan 已使用共同遍历，成功 rebind 不分配目录。

本轮延续已记录的 GStreamer 静态分配协商设计，不增加媒体算法或平台 adapter。presentation_plan_review 与 timing_path_review 对九个源码文件均明确局部 PASS。Windows Release 全量构建 51374 退出 0（801 任务计划），四项产物核验通过，realtime CLI 7122944 字节、14:34:25。

尚缺 topology policy/incident capacities 同步、完整工作区预算及具体 freeze solver；本轮不构成合屏实流通过。没有新媒体测试、抓包、截图、临时测试脚本或同步临时文件。RKMPP 使用现有目录、8 核增量 all-target 构建。

RKMPP 单次 SSH 1343 已终态退出 0，增量构建完成 54/54；九个源码文件写入前逐项核对基线 hash，写入后核对目标 hash。未执行 clean-first。两平台编译通过仅覆盖本轮代码，不替代原规格 realtime 链路验收。

## 2026-11-05 源拓扑策略与关联边容量更新

SourceTopology 的初次 plan 与固定形状 rebind 共用 compileTopology。声明/alias 按游标检查 role、position、kind，查找限制于已声明前缀；连接按原顺序核对全部非 policy 字段后更新策略。节点关联边容量按原边顺序重算，自环计一次，外部位置维持 nullopt，核对完整数组数量和有界非空队列。成功更新不构造字符串、不扩容；失败候选整体作废。

保持既有静态规划与 GStreamer 构建前分配协商边界，没有改变媒体运行时算法。为避免 freeze 中分配游标数组，逐位置扫描连接；端点查找仍是线性 binding 查找，因此计算成本高于原单次边遍历，不宣称 O(VE)。两个独立审查者对本轮两文件明确局部 PASS。

Windows 全量 Release 62153 退出 0（801 任务计划），四项产物核验通过，realtime CLI 7122432 字节、14:45:05。完整图的共享输出/跨源绑定容量更新、全部费用工作区预算、具体 freeze solver 和原规格实流验收仍待完成。本轮没有新抓包、截图、日志、测试脚本或传输临时文件。

RKMPP 单次 SSH 25660 增量构建 23/23、终态退出 0；两文件同步前后 hash 核验成功，使用 8 核，未执行 clean-first。无新媒体链路通过结论。

## 2026-11-05 完整图容量更新

完整 GraphTopology 的初次 plan 与固定目录 rebind 共用声明、attachment、binding、连接及 incident 遍历。更新核对节点 kind/source、role/position、owner/port、边全部非 policy 字段和目录数量，保持原顺序；自环关联容量只计一次。成功更新不增长容器、不构造字符串，失败候选整体作废。

每源临时 positions vector 已消除，owned 位置由源起点加前序 owned 项数推导，external 仍通过 Aggregate boundary 显式解析。每次附加 alias 前重新取得 port 视图，构造模式先生成拥有字符串再 push，避免重分配使视图失效。两位独立审查者均局部 PASS；保持既有静态分配协商原则，没有新媒体算法。增加了映射/incident 扫描成本，不宣称性能提升。

完整候选费用重算、预分配工作区预算及具体 freeze solver 尚未接通；本轮不是实流验收通过。没有产生抓包、截图、测试脚本或同步临时文件。

Windows Release 全量构建 1353 终态退出 0（801 任务计划），四项产物核验通过，realtime CLI 7121408 字节、14:54:30。RKMPP 单次 SSH 60191 核验两文件同步前后 hash，8 核增量 all-target 完成 22/22、终态退出 0，未执行 clean-first。无新媒体链路验收结论。

## 2026-11-05 候选费用原地重算

CandidateCostPlanner 的首次 plan 与 rebind 共用 computeCosts/streamCosts。首次构造 packet/lineage 根目录，更新模式核对 source、stream 和 audio optional 后复用既有 rebind；其余 container/lease/payload/index/generation/frame/startup/clock/ownership 数字产品重新规划。保留原历史 backing 上界、当前持有者下界与 checked 算术，protocol/startup/total 每轮从零汇总。失败候选整体作废；固定 topology、producer、projection domain 和 transfer 模式仍是前置条件。

成功路径分配审计确认：除预建根目录外，当前调用树只查询类型几何、执行 checked 计算和写入数值，没有实际运行容器分配。错误路径不作无分配保证。依据仍是既有静态费用与 GStreamer 启动前分配协商；没有改变媒体算法。完整工作区预算和具体 freeze solver 仍未接通。

RKMPP 运行时能力缺口单独保留：MediaPmrContainerGeometry::proxyBytes、MediaAvStartupStreamStoreGeometry、MediaAvStartupGenerationStateGeometry 仍有 MSVC 专属门禁。GCC 编译通过不等于这些查询可用，也不等于实流通过；需要项目内供应商 geometry adapter，不能用预编译或 fallback 隐藏 unsupported。另有原记录的 RKMPP 源 AU 身份限制未解决。

Windows Release 全量构建 2252 终态退出 0（801 任务计划），四项产物核验通过，realtime CLI 7121408 字节、15:05:04。RKMPP 单次 SSH 90652 核验两文件同步前后 hash，8 核增量 all-target 完成 4/4、终态退出 0；无新媒体链路运行。

复审发现构建/更新验证不等价：更新绕过 lineage 首次规划的 passthrough transfer 门禁，presentation_plan_review 初次判 FAIL。已在共同 compute、更新根目录前补回 None 检查与原 unsupported 错误；此前 2252/90652 构建只覆盖修复前版本，需重新构建与双人复审。

修复后 presentation_plan_review、timing_path_review 均重新明确局部 PASS。Windows Release 全量 30292 终态退出 0（801 任务计划），四项产物核验，realtime CLI 7121408 字节、15:11:40。RKMPP SSH 43829 核验补充源码前后 hash，8 核增量 4/4、退出 0。生产入口仍为 freezeBatch(rtpSources, nullptr)，候选费用规划器尚无生产调用方，明确不据此宣称求解器或合屏完成。

## 2026-11-05 拓扑规划 kernel 更新调用链

TopologyPlanningKernel 已串接 sourcePolicies→aggregate→每源 topology→完整 graph 的固定目录更新，初次构造与更新复用同一遍历。产品持有固定 output policy、video/audio branch 声明及 scheduler/protocol/execution 三段声明；rebind 不接受新 outputRuntime。调用前必须先刷新源 branch/input，失败后整个 products 作废。

Aggregate 首次构造和更新共用边声明；epoch 端口通过按 size_t 十进制最大宽度确定的栈数组及 to_chars 形成，构造时复制、更新时即时比对。全部边身份核对完成后按目标 aggregate 节点与输入端口做 pairwise 冲突检查，保留原分节点集合语义，避免动态 unordered_set。端口/label 借用仅指向静态字面量，未保留栈视图。两位独立审查者对四文件均明确局部 PASS。

沿用既有静态拓扑与分配协商模型，未增加媒体算法。更新成功路径不增长容器；新增保留的 sourcePolicies、输出连接及三段声明必须计入事前工作区预算，不能当作免费。源 branch/input 的上游刷新、合法容量基线、完整预算与 freeze solver 仍未接通，没有实流通过结论。本轮未生成抓包、截图、测试脚本或同步临时文件。

Windows Release 全量 12757 终态退出 0（801 任务计划），四项产物核验通过，realtime CLI 7126016 字节、15:18:34。RKMPP 单次 SSH 83928 核验四文件同步前后 hash，8 核增量 all-target 完成 24/24、终态退出 0。没有新的实流运行或完成声明。

## 2026-11-05 输入组装容量更新

SourceClockPlanner 的 assembly 构造与更新共用 compileAssembly，保留原 startup/acquiring/route/member/time-window 交叉验证及 RTP、MPEG-TS、demux 时序选择。更新核对固定 clock、generation、identity、video timing、audio duration/member；demux 全字段比较已有 owning 产品，不复制字符串。全部检查通过后只写 acquiringCapacity/Timeout，成功路径未发现分配。

两位独立审查者均给出 assembly 级局部 PASS。接口借用初次 prepared timing 和固定 input request，外层 duration facts、输入有效性与 bounded receive 校验仍是前置条件；不得拿此接口替代完整 runtime 校验。初次 plan 已消费共同流程。

本轮调查确认上游刷新还必须包括 startup 容量、音频 commandLead/compensationWindow/frequencyFilterTimeConstant 与 audioCorrection、queues/edgePolicies、startup batch/route/clock/coordinator 费用及 branch/input。generation transition 所依赖事实需保持不变。原 planFacts 按值参数、groupKey 和 timing/assembly identity 拷贝仍需在接线时避免；不能只改 edges 或保留旧音频校正量。本轮不构成完整刷新链路或 solver 完成，没有新实流、截图、抓包、测试脚本或同步临时文件。

首次 Windows 全量 79262 退出 1，Ninja 报子命令失败（底层构建退出 2），未启动 RKMPP；终端中间输出截断，未引用未经保存的具体编译诊断。源码检查发现新增 assembly{} 要求无默认构造的 MediaRunningTime，已改构造模式使用 optional 延迟 emplace 完整产品，不引入占位时间；更新模式仍接受已有产品引用。重新进行双人复审和全量构建。

修复后两位审查者重新给出局部 PASS。Windows Release 全量 20924 终态退出 0（801 任务计划），四项产物核验通过，realtime CLI 7126528 字节、15:30:20。RKMPP SSH 87977 核验两文件同步前后 hash，8 核增量 all-target 8/8、退出 0。首次失败保留，不以编译通过替代完整 runtime 更新或实流验收。

## 2026-11-05 源容量刷新调用链

SourcePlanningKernel::rebindCapacity 借用原 prepared request/timing，串接 StartupPolicy→SourceRuntime→startup storage→branch/input 更新。runtime 初次 plan 与更新共用 source 验证及 audio correction/edge/route 规划；更新 assembly、三个 derived servo 时间值、audioCorrection、queues 与 edges。startup 只允许六个容量/字节字段变化，其他 optional 字段逐项固定，identity 不提供替换时保留既有值、提供时必须相等。音频 startup 与原独立 retention units/unitBytes/bytes 核对。

clockRequest 的 audio/isolated 指向 products 对应成员；不复制 owning audio/input/sync/groupKey。原 prepared input/timing、products 非容量事实必须由调用方保持不变，接口不修改 clock/recovery/filter/selectedAudio，因此原 transition 在此前提下保留。成功更新未发现动态分配；任意失败作废所有候选产品，不能继续预算搜索。

调用方固定事实寿命、合法容量基线、全部工作区预算和具体 solver 仍未接通；本轮不是合屏实流通过。没有媒体测试、抓包、截图、测试脚本或同步临时文件。

两位独立审查者对四文件均明确局部 PASS。Windows Release 全量 61446 终态退出 0（801 任务计划），四项产物核验通过，realtime CLI 7128576 字节、15:39:52。RKMPP SSH 34033 核验四文件同步前后 hash，8 核增量 all-target 6/6、退出 0。未修改外部依赖，未运行新的媒体链路。

## 2026-11-05 工作区边界决策与 packet 进展门禁

现有 FixedCapacityStorage 只约束外层元素背板，不能覆盖 SourcePlanningProducts、Topology、Reference、Cost 内的普通 vector/string。选择“借用不可变 prepared 事实＋显式预算 allocator 的可变目录”，不对完整 owning 产品做易遗漏的事前全峰值手算，也不以事后 capacity readback 替代准入。

借用 input/video/audioPipeline/isolatedAudioInput/groupKey/threading/transition/prepared clock timing/固定输出声明；原 owner 必须跨 freeze 存活且地址稳定，原存储仍归既有预算域，不因借用而免费。自持 startup 六容量、derived servo/correction、assembly acquiring、queues/policies、数值费用，以及 branch/input、aggregate、source/full topology、reference、packet/lineage 目录。所有内层 rootIndices、retainingPositions、贡献目录和拥有字符串同样必须使用预算资源。现有 owning rebind 接口尚需借用视图适配；必须复用已审查的共同算法，不另建候选媒体链路。

依据：[C++ polymorphic_allocator 工作草案](https://eel.is/c++draft/mem.poly.allocator.class) 将分配交给显式 memory_resource；默认构造会取默认资源，因此不能只把类型改名而漏传资源。[GStreamer 分配文档](https://gstreamer.freedesktop.org/documentation/plugin-development/advanced/allocation.html) 支持构建前 allocator/pool 协商和复用。这里只采用显式归属与有界分配原则，不宣称两系统内部工作区完全等价。

基线核验纠正：SourceResourcesPlanner 对 decoded video 的 startupPackets 实际传 0；音频才传 Na。原 ProducerResidence 的 packet 条件为 R+I >= S+U+I+1，RawInput 两端另加 A，均化简为 R>=S+U+1。禁止把视频 prefix 或 Nv 代入 S，制造虚假循环。frame 仍需原 decoder/filter progress 检查；通用 queue=1 只是单对象结构下界，尚须逐原子端口核对完整批次才能证明可执行。

本轮 CandidateCost 首次计算和 rebind 均在目录更新后新增 raw/binder packet progress 校验。按 creatorPosition＋stream 只汇总 GraphHolder，核对 root/holder 数量，checked 求和后复用 ProducerResidence 原公式；不计 Construction/CreditWait/Prefix/Acquiring/Outputs/AtomicCompletion。GraphHolder 来自同一 payload residence 遍历，其 ReachableOwners 与最终 LiveOwners 仅差零数量项；此等价性是保守规划量，不是 wrapper 与 payload 真实寿命相同。缺口返回 source/creator/stream/现有与所需数量，属于非法基线，不得作为预算过大继续二分。只覆盖目录内 raw/binder，最终全图独立 readback 和其他实际 producer/原子端口检查仍须保留。

本轮 Windows Release 全量 57466 终态退出 0（801 任务计划），四项产物核验通过，realtime CLI 7128576 字节、15:51:00。RKMPP SSH 66559 核验单文件同步前后 hash，8 核增量 all-target 4/4、退出 0。双独立局部代码审查通过；没有新媒体测试或临时测试产物，完整 solver 和 workspace 仍未完成。

## 2026-11-05 生产拓扑目录的预算分配接入

Source/GraphTopology 的外层目录、incident 内层数组及拥有的端口字符串改用显式 preparation resource；首次构造与既有 rebind 仍复用同一拓扑声明。生产 preparer 从共享根预算创建资源，沿 builder/kernel 传入，最终 projection 自持 owner，保证离开 kernel 后仍有效。根预算上的实际存储属于 otherRetainedBytes，不挂到 source 0，也不重复加入 source total。保留分配头和 padding 的既有计费语义，不等于 RSS 或显存。

依据 C++ polymorphic_allocator 的显式资源分配及不传播复制 allocator 规则（本文件前述标准链接）：禁止拓扑隐式复制，跨 owner 移动赋值整体替换产品；每个内层 string/vector 单独绑定资源，不能依靠外层 PMR 自动注入普通结构体。最终图验证排序用的 incident 临时数组同样显式计费。

双独立首轮审查发现 kernel 总产品仍有隐式 move assignment，可能让 sources/sourcePolicies 留旧 allocator 而 graph 转入新 owner，均判局部 FAIL；须补齐总产品 owner 与移动约束后复审。Windows Release 构建 80468 正在运行，本记录不预判结果。

本轮只接入实际生产拓扑目录，尚未覆盖 branch/input、aggregate、output declarations、reference、packet/lineage 及其首次 projection 临时目录；不能把此结果视为完整 workspace 预算或 solver。freezeBatch 仍未接入具体 solver，没有新的媒体验收结论。

80468 已终态退出 0，完成 Windows Release 全量构建。该构建仍是审查修复前版本：随后 kernel 总产品新增先声明的 owner、禁 copy 与 move assignment，仅允许 noexcept move construction；修复后构建 64080 正在进行。首次追加本段时 Python 默认 GBK 读取中文文档失败，未改动文档内容，已改为显式 UTF-8；源码修改先于该错误完成。

presentation_plan_review 与 timing_path_review 已对修正版完整 11 源码明确局部 PASS，确认 kernel owner 与移动约束关闭首轮缺口；保留首轮 FAIL，不把源码审查等同于实流验证。

修复后 Windows Release 全量 64080 终态退出 0（801 任务计划），四项产物核验通过，realtime CLI 7126016 字节、16:08:58。RKMPP 首次整包同步命令超过 Windows 命令长度上限，进程未创建；拆批后首次检查使用远端 Python 不支持的 Path.is_relative_to，写入前失败。改为父目录归属检查，三批 4＋4＋3 文件均前后 SHA256 核验成功；无同步临时文件，最终 SSH 21853 正进行 8 核增量 all-target 构建。

RKMPP SSH 21853 已终态退出 0，8 核增量 all-target 完成 23/23。两平台本轮只验证构建，没有执行 local 或 realtime 媒体测试。没有新增测试产物待清理；用户指定复用源与既有未跟踪文件保持原状。下一项是沿同一资源迁移 aggregate/branch/reference/cost 目录并接具体 solver，仍保留 SourcePresentation 完整准入门禁。

## 2026-11-05 聚合连接目录预算接入

Aggregate 的外层源目录、每源内层连接数组、输出数组及端口字符串改用与 Source/GraphTopology 相同的 preparation resource。双层 PMR vector 依标准 uses-allocator 构造传播资源，普通 connection 的两个 string 仍显式传入资源；禁止总产品复制，整体移动替换 owner 和 allocator。kernel 首次构造与生产 builder 已使用新目录，builder 以 span 借用。原声明顺序、音频成员与端口冲突检查、固定目录 rebind 保持不变。

延续前述 C++ 显式 allocator 与 GStreamer 静态分配协商依据，没有新媒体算法或平台分支。尚未覆盖 branch/input、固定输出声明、reference、packet/lineage 及其临时投影目录，freeze 仍为 nullptr，不构成完整 solver 或实流通过。

presentation_plan_review 与 timing_path_review 对本轮 5 源码均明确局部 PASS，Windows Release 全量构建 61725 正在运行。下批调用核验：video/audio branch planner 还由通用 segment builder 消费，不能仅改合屏调用方；input planner 同时由 realtime protocol/input segment 消费。后续须统一显式分配入口及借用消费，禁止普通 vector 临时构造后再复制到预算目录。

Windows Release 全量 61725 终态退出 0（801 任务计划），四项产物核验通过，realtime CLI 7126016 字节、16:18:07。RKMPP SSH 84140 同步五文件前后 SHA256 核验成功，正在运行 8 核增量 all-target；无同步临时文件或新媒体测试产物。

2026-11-06 恢复执行时，84140 会话句柄已失效，未取得其最终退出码；远端核验无 ninja/cmake/cc1plus 编译进程，realtime CLI 时间为 2026-11-05 16:19:15、12379960 字节。重新运行同目录 8 核增量 all-target，SSH 43668 终态退出 0，Ninja 报 no work to do。该结果验证当前增量构建状态，不补造原会话退出码。无新媒体测试或临时产物。

## 2026-11-06 共用 branch/input 连接目录预算接入

video/audio/input 三个连接规划器统一显式接收 memory_resource，仍复用同一个声明 compiler/writer；合屏分支、输入连接及输出 video/audio 连接直接在根 preparation resource 上构造，没有普通 vector 中转复制。资源在 freeze 前创建，并贯穿 source kernel、最终 source 产品及 topology kernel。源产品首字段 const shared_ptr 与不可复制 branch 成员保管目录寿命并禁止隐式复制/移动赋值；输出目录在构造时绑定资源，按成员接收同资源产品。

连接只含值和静态 string_view。新增 ConnectionCatalog 采用已有 LineageVector 的 allocator-extended 可失败移动方式；核验本机 VS 14.51.36231 的 vector:761–767，普通 noexcept move 在 Debug 仍调用 _Alloc_proxy。新目录移动不声明 noexcept，外层 source/branch/kernel 的对应错误边界保留 allocationFailed。既有 Source/GraphTopology/Aggregate 的普通 PMR 移动仍有同类 Debug 风险，明确列为未完成可靠性项；当前只做用户要求的 Release 验证，不宣称跨配置失败语义已完成。

通用 segment builder 的短期 graph-binding 连接声明显式使用原普通堆分配域；它们不属于候选 workspace，没有引入媒体 fallback。scheduler/protocol/execution 三段输出声明、reference、packet/lineage 及临时投影仍待预算迁移；完整 solver 与实流验收仍未完成。

branch_budget_review_a 与 branch_budget_review_b 对本轮 22 源码均明确局部 PASS；Windows Release 全量 99294 正在运行。受限环境 CIM 查询被拒，继续轮询原构建会话，没有重启构建。同步清单已通过 git cat-file 批量读取基线生成，尚未写入远端。

Windows Release 全量 99294 终态退出 0（801 任务计划），四项产物核验通过，realtime CLI 7133696 字节、2026-11-06 11:55:56。22 源码分五批完成同步前后 SHA256 核验，新头文件核对原先不存在；RKMPP SSH 33999 正进行 8 核增量 all-target，新头文件触发现有 CMake 自动重新配置。无同步临时文件或新媒体测试产物。

RKMPP SSH 33999 已终态退出 0，8 核增量 all-target 完成 54/54。本轮两平台构建通过，未执行 local 或 realtime 媒体测试；没有抓包、截图、日志、临时测试脚本或同步临时文件需清理。原规格实流与既有 Debug 风险仍明确未完成。

## 2026-11-06 固定输出段声明预算接入

scheduler/execution/RTP/MPEG-TS 声明的节点和连接目录统一显式使用 memory_resource。原可失败移动目录更名为 MediaPlanningCatalog，直接用于节点及连接，不保留旧类型/include alias；assign 与 insert 均作用于目标预算容器，initializer_list 只有栈上值和静态视图。节点、边、payload、policy 与声明顺序保持，MPEG-TS 音频边仍插到首个 TsPlan 来源边之前。

合屏输出 owner 构造时同时绑定 scheduler/protocol/execution 三段目录，kernel 传入共享预算资源；原通用 graph-binding 路径显式保留 new_delete_resource 分配域。scheduler builder 的默认构造后赋 bindings 改成一次完整结果初始化，避免无资源默认目录。沿用前述标准 allocator 与既有静态分配协商，不增加媒体算法。

此后固定输出声明目录完成预算接线；reference、packet/lineage/cost 的目录及临时投影、借用固定事实的具体 workspace、容量基线与实际 freeze solver 仍未完成，既有 Debug PMR 移动风险保留。当前无新的实流验收结论。

branch_budget_review_a 与 branch_budget_review_b 对本轮冻结源码均明确局部 PASS；旧标识源码检索为零，git diff --check 通过。Windows Release 全量 82917 正在运行。下一批核验已确认引用投影的 retainingPositions 仍为嵌套普通数组，必须连同外层与临时 contributions 一起迁移，不能只改外层类型。

Windows Release 全量 82917 终态退出 0（801 任务计划），四项产物核验通过，realtime CLI 7140352 字节、2026-11-06 12:10:12。远端分三批核验 21 个源码路径的前后 SHA256（包括更名旧头删除与新头创建），最终 SSH 4357 正进行 8 核增量 all-target；沿用现有 CMake 自动重新配置，没有新建构建系统。没有同步临时文件或新媒体测试产物。

RKMPP SSH 4357 终态退出 0，8 核增量 all-target 完成 60/60。两平台本轮构建通过，未执行 local 或 realtime 媒体测试。没有抓包、截图、临时媒体、测试脚本或同步临时文件待清理；用户复用源及原有未跟踪文件保持不变。

## 2026-11-06 引用目录与临时投影预算接入

reference 的 node/startup/event 目录、projection 的遍历空间与 contributions，以及每个 Owner 的 retainingPositions 均直接绑定 preparation resource；复用原遍历、零数量可达槽与 rebind 算法。Owner 与 projection/reference 均先声明 const shared_ptr 资源所有者，保证目录逆序析构及移动后源容器的资源寿命。packet/lineage 根会被最终账本持久化（ResourcePlanner 的 sourceFrameOwnership），因此每个 Owner 自持资源，不能只借用临时 topology。普通最终图 readback 仍使用原堆分配域，以空 owner alias 引用进程级 new_delete_resource，不分配控制块。

依据既有 polymorphic_allocator 标准及 [shared_ptr aliasing 规则](https://eel.is/c++draft/util.smartptr.shared.const)。residence_budget_audit 与 residence_budget_review_b 对冻结的六源码均明确局部 PASS。packet/lineage 外层根目录、rootIndices、候选费用产品、具体 workspace 与 freeze solver 仍未完成；本轮没有媒体测试，不代表完整预算或合屏验收通过。Windows Release 全核心全量构建 4848 正在运行，结果待终态核验。

Windows Release 全量 4848 终态退出 0（801 任务计划），四项产物核验通过，realtime CLI 7147008 字节、2026-11-06 12:25:44。六源码同步前后 SHA256 均已核验；RKMPP SSH 42610 正执行 8 核增量 all-target。没有同步临时文件或新媒体测试产物。

RKMPP SSH 42610 终态退出 0，8 核增量 all-target 完成 61/61。两平台构建通过，没有新实流测试或临时产物。下一批合并迁移 packet/lineage 外层根目录与四组 rootIndices，并把 CandidateCost 的计划接收改为延迟构造，避免不可赋值 Owner 经 PMR 容器整体赋值；随后继续具体 workspace 和 solver 接线。

## 2026-11-06 packet/lineage 与候选费用目录预算接入

packet/lineage 外层 roots 和 lineage 四组 rootIndices 直接使用显式 preparation resource；两个计划先保存 const shared_ptr，预算资源随最终共享账本存活。原两个数值索引复制改为目标资源上的范围 assign，族间追加仍使用范围 insert，算法与保守费用不变。CandidateCost 的两个计划使用 optional 延迟 emplace，避免不可赋值 Owner 进入 PMR vector 整体赋值；更新先核验产品存在，底层 rebind 在更新前核对 plan/reference/topology 的资源身份。首次构造及返回移动的 bad_alloc 由 plan 边界转换，成功更新路径不分配。

沿用前述 C++ allocator 和 shared ownership 标准；residence_budget_audit 与 residence_budget_review_b 对冻结七源码均明确局部 PASS。Windows Release 全核心全量构建 97550 正在运行。此批关闭剩余引用与费用目录的预算接线，不代表具体 workspace、合法容量基线、freeze solver 或实流验收完成。

Windows Release 全量 97550 终态退出 0（801 任务计划），四项产物核验通过，realtime CLI 7150080 字节、2026-11-06 12:33:20。七源码分两批 4＋3 文件核验同步前后 SHA256；RKMPP SSH 6408 正执行 8 核增量 all-target。未生成同步临时文件或媒体测试产物。

RKMPP SSH 6408 终态退出 0，8 核增量 all-target 完成 61/61。本轮两平台构建通过，无新媒体测试或临时产物。接线前核验发现：freeze 前尚无 source runtime/synchronization/assembly 的稳定 owner，当前在 seal 后才生成。下一步必须将固定事实编译前移并稳定保管，或由现有 prepared 事实适配共同编译器；不能只创建指向尚不存在 runtime 的借用视图，也不能每候选重复 owning plan。

## 2026-11-06 同步政策无分配读取接线

新增同步读取视图：固定 protocol/生命周期/恢复等字段借用 owning plan，startup 数值和 servo 是无分配快照，startup 两个 identity 保留原字符串引用。六容量可由候选值覆写；完整 startup 的 19 字段值比较保持原连续输出校验语义。禁止从 const/非 const owning 右值构造视图；每次修改候选容量后必须重新生成快照，保存视图不延长原 owner 寿命。依据 [C++ 临时对象寿命](https://eel.is/c++draft/class.temporary) 与此前显式分配协商设计。

整个原 PlanValidator 改为消费同一视图，所有 owning 入口隐式适配，不保留第二验证算法；TopologySourceFacts 按值保存视图并由现有 kernel 同步消费。AudioCorrectionReachabilityPlanner 只接其实际读取的 servo，五个现有调用全部更新，计算不变。此批尚未接 assembly/runtime 数值 workspace，也未前移固定事实准备或接 freeze solver。Windows Release 全量 73873 正在运行；源码双独立复审结果待归档。

residence_budget_audit 与 residence_budget_review_b 对冻结的 11 源码（含新视图头文件）均明确局部 PASS。Windows Release 全量 73873 终态退出 0（801 任务计划），四项产物核验通过，realtime CLI 7155712 字节、2026-11-06 12:43:07。远端分四批 1＋5＋3＋2 文件核验同步前后 SHA256，新头核对原先不存在；RKMPP SSH 27335 正执行 8 核增量 all-target。无新媒体测试或同步临时文件。

RKMPP SSH 27335 终态退出 0，8 核增量 all-target 完成 21/21。本轮两平台构建通过，无新媒体测试、抓包、截图、日志或临时同步文件；原复用源与未跟踪依赖文件保持不变。下一步是 assembly/source runtime 的数值状态适配及 freeze 前固定事实准备，完整 solver 和原规格实流仍待完成。

## 2026-11-06 assembly 与 runtime 候选数值状态

assembly 的固定 clock variant、身份和 timing/duration 改为借用视图，acquiring 数量/超时自持；主/子 view 均禁止 owning 右值构造。原 clock compiler 和 RTP/MPEG-TS/demux 输入校验统一读取视图，未另建候选算法。SourceCapacityState 借原 runtime，保存六 startup 数量/字节、servo、assembly 数值、queues、edges、correction；SourceKernel 通过唯一 runtime rebind 更新，再核验同 owner 与音频成员并发布数值。初次 planFacts 同样复用政策规划并回写 servo。失败仍作废整个候选，不宣称整个 SourceKernel 事务回滚；首次 owning plan 仍会分配。

沿用前述 C++ 借用寿命规则及构建前静态分配协商。首轮独立审查发现 planSynchronizedSource 仍接受 owning startup，无法接新视图，明确局部 FAIL；待当前 Windows 全量 26267 终态后修复共用入口并复审、重建。freeze 前固定事实 owner、完整工作区和 solver 仍未接通。

26267 终态退出 1，Ninja 子命令退出 2；实际诊断 C2664 位于 MediaRealtimeAvSourceRuntimePlanner.cpp:71，确认 StartupPolicyView 无法转换到旧 owning 入口。已将 EdgePolicyPlanner 的唯一共用 planSynchronizedSource 声明/实现接入视图，原数值算法不变；重新复审及 Windows 全量构建，不启动失败版本的 RKMPP 验证。

residence_budget_audit 与 residence_budget_review_b 已对修正版 12 源码（含三个新文件）完整复审，均明确局部 PASS，确认 EdgePolicy 遗漏关闭、两条 servo 回写及完整固定校验保留。Windows Release 全量 71286 正在进行；首轮 26267 失败记录保留。

Windows 修正版全量 71286 终态退出 0（802 任务计划），四项产物核验通过，realtime CLI 7156736 字节、2026-11-06 12:56:39。远端三批 3＋6＋3 文件前后 SHA256 校验成功，三个新文件核对原先不存在；RKMPP SSH 1913 正执行 8 核增量 all-target。

下一完整批次的硬边界：精确 held/history 只来自 freezeBatch 锁内 storageSnapshotLocked，不能由公开 session plan 或 probeSnapshot 替代。现有 Kernel::plan 会经 planSourceForOutput→AudioResamplerCapabilityProvider::verify 调用 swr_alloc_set_opts2/swr_init，不能整体移入 freeze 锁。必须锁外完成固定 capability/input/identity 准备并明确新存储的实际预算、固定地址与生命周期；锁内按真实快照确定合法基线，再一次初始化目录并数值刷新。seal 后应消费同一固定 owner，避免重复完整 Kernel::plan，同时保留最终 DAG 独立 readback。当前资源仅覆盖目录，普通 owning input/audio/sync/assembly 的新字符串并不因固定或借用而免计费；后续需闭合这项实际存储表示，不能继续仅加未接线视图。

RKMPP SSH 1913 终态退出 0，8 核增量 all-target 完成 18/18。两平台修正版构建通过；本轮未运行 local/realtime 媒体链路，没有抓包、截图、临时日志或同步文件待清理。首次 Windows 失败保留；原规格实流、固定 owner 完整准入、具体 solver 和已有平台可靠性风险仍未完成。

## 2026-11-06 源音频 capability 移到 freeze 外

Preparer 在 freeze 前调用 SourceAudioPlanner，按全部源数在根 planningResource 上建立精确长度数值目录；无音频保留空槽。所选源核对完整 prepared decoder（含格式、声道和 retention authority）后复用 planKnownAudio 的 resampler；其余源用各自 decoder 与同一最终 resolvedOutput 进行原 verify，保留原 deadline/cancellation 前后检查。目录随 preparation 局部 owner 存活，归根的 otherRetainedBytes，不重复归源 0。

共用 planSourceForOutput 改为绑定显式 preparedResampler，不再调用 swr_init；通过 SourcePlanner/Kernel 的原请求链消费对应槽。planKnownAudio 按原 TranscodeFrame 且 selectedDecoder 条件先验证，再调用同一绑定算法。通用无预选 decoder 的既有分支语义保留，合屏仍要求 decoder/resampler 齐全。输入改为 const 借用，最终 owning 计划仍复制必要事实；四个整数只在当前同源、同 output 不变链上复用，不宣称它们是格式/声道完整指纹。

复用现有 libswresample 能力验证算法（swr_alloc_set_opts2、swr_init、原 steadyStateOutputBound），没有添加替代媒体算法。本批只闭合 source capability 调用位置及新数值目录预算；既有 capability 临时证明空间、固定 input/sync/assembly 新存储与实际 solver 仍须继续闭合。Windows Release 全量 61027 正在运行，源码双独立审查结果待归档。

Windows Release 全量 61027 已退出 0（803 任务计划），四项产物核验通过，realtime CLI 7168000 字节、2026-11-06 13:22:09。residence_budget_audit 与 residence_budget_review_b 对本批源码均明确局部 PASS。按用户要求，RKMPP 保留已有构建产物，10 个源码分两批核验 SHA256 后同步，SSH 78720 执行 8 核增量 all-target，完成 159/159，终态退出 0；没有 clean-first。共享头文件变更导致依赖重编译，新增文件仅触发 CMake 自动更新清单。

本批只确认两平台构建和局部审查通过，未运行媒体测试，未产生抓包、截图、临时日志或临时脚本。固定事实存储的完整预算、联合容量 solver 接入及原规格 Windows→RKMPP realtime 验收仍未完成。

## 2026-11-06 固定事实与容量依赖的实施边界

核验当前生产链：`SourcePlanner::plan` 在 seal 后才调用 `SourcePlanningKernel::plan`；后者同时创建 input/audio/sync/group/assembly 的 owning 字段和容量相关目录。`freezeBatch` 已在全部源锁下提供 session plan 与精确 snapshot，且有最终预算复查与事务发布。无需改变冻结事务或增加运行时 fallback；应先消除源计划的固定事实与容量相互依赖。

沿用 [GStreamer 启动前 allocation negotiation](https://gstreamer.freedesktop.org/documentation/plugin-development/advanced/allocation.html) 的事实准备、分配协商、激活顺序；它不证明本项目字符串成本或共同增量目标。字符串成本只采用仓库 `MediaStandardContainerStorageReadback::stringCopy` 已审计的 STL 分支与真实构造路径，未知 STL 显式失败。

下一实现批次按以下调用关系完成，不再只提交新增借用视图：

1. **固定同步产品。** 从 `MediaAvSyncPlanner::plan` 提取同一固定编译过程：原参数校验、domain/lifecycle、同步策略、audio servo、RTP/TS/demux 身份与时钟声明。`StartupPolicyPlanner` 把固定等待策略与六容量字段的绑定分开；maximumGap、outputLead 仍来自原事实。使用独立的未完成产品类型承载，不把缺容量的 `MediaAvSyncPlan` 交给完整 validator，也不填虚构容量通过校验。原 plan 入口组合固定编译与容量绑定，保留原最终 validatePolicy。
2. **固定输入与时钟产品。** `InputPlanner::applyNodePlans/fillNodePlan` 仍为唯一输入映射。`SourceClockPlanner::plan` 的 timing 解析（包括 boundedReceive）与 assembly 的固定 clock/identity/timing 声明前置，acquiring 数量与期限由原共同 compiler 绑定。RTP、TS、demux 共用原分支，不能另写合屏专属时钟算法。
3. **真实存储 owner。** 源固定 owner 在 freeze 前构造，保留 input/audio/group/sync/timing/assembly 及其 root 租约；video 借用已有 prepared plan，先明确其既有预算归属。`stringCopy` 仅适用于 fresh copy/range，不适用于 `operator+`、append、赋值扩容。需逐项审计 URL/SDP/transport/depacketizer/envelope、audio 输出与 decoder、groupKey、RTP 身份拼接，以及 startup→timing→assembly 的多份身份。临时副本、容器 proxy、嵌套数组与对象目录均计入同时存活峰值；先 reserve，后构造，失败释放。固定 owner 销毁前不得释放租约，借用后不得移动 owner。
4. **真实快照与候选。** 锁内仅从传入 snapshots 读取 held/history，推导合法基线并填入数值工作区；固定 owner 不再创建字符串。复用既有 runtime、edge、route、topology、residence 和 cost 的共同计算/刷新，预算覆盖所有目录实际分配。随后实施共同 k 搜索及源 admission；非法事实和平台不支持不能归类为超预算。
5. **最终消费。** seal 后移动同一已计费固定事实并绑定最终数值，取消再次完整 Kernel::plan 的复制。保留 sealed owner、producer holders、成员、容量及最终 DAG 独立 readback；readback 使用实际最终图，不能拿候选缓存自证。

本轮核验没有发现外部依赖修改的必要授权，也没有解除 RKMPP 的 AU 身份传播与剩余容器几何限制。上述为下一批实施边界，不是 solver 已实现；本轮尚未修改生产代码或运行媒体链路，不新增构建通过声明。

双独立设计审查（residence_budget_audit、residence_budget_review_b）均为局部 PASS，限本节实施边界。补足交接约束：SourceCapacityState 必须直接借新固定 owner，不能再构造完整 runtime 中转；maximumGap 若属于固定事实必须校验一致，否则随容量绑定显式更新，不能只覆盖六个数量字段。最终移动 owning 字段前先销毁全部候选借用，或仅移动地址稳定的 owner 句柄；租约随最终事实存活，Debug proxy 的移动分配也必须纳入预算与异常边界。

## 2026-11-06 同步固定产品与容量绑定实施中

已拆分 StartupFixedPolicy 与 StartupCapacity（六容量及 maximumGap），完整 StartupPolicy 组合两者；FixedFields 不含 startup，FixedPlan 与完整 Plan 不互相转换。原 AvSyncPlanner::plan 组合 prepareFixed/bind，RTP/TS/demux、servo、等待规则仍为共同实现；完整 bind 保留原 validatePolicy。State 与 view 同步传递 maximumGap，最终 publish 不再漏该容量事实。尚未接通 freeze 前完整 owner 与 solver。

执行约束：沿用本文件作为实施记录；仓库真实 realtime 验收与双独立复审要求优先于通用 executing-plans 技能的自动化测试和单次审查流程，不新增测试体系。

首轮源码审查双 FAIL：误删 PreparedDemuxTimestampFacts.startup，FixedFields 残留重复完整 startup。Windows 全量 96284 终态退出 1（Ninja 子命令 2），编译证实缺失字段；修复后两者完整复审局部 PASS。第二轮 Windows 62823 终态退出 1（Ninja 子命令 2）：PlanValidator.cpp:521 的 view 与 owning 比较触发 C2666，新增容量基类 friend== 引起候选歧义。已开始修正比较声明并重新复审/构建；不得把源码审查 PASS 作为构建成功。

第三轮 Windows 61857 终态退出 1，同处 C2666：仅将 Capacity 比较改成员仍留下 Policy 继承成员的 C++20 反向候选。进一步将完整 Policy 默认比较改成员，使 Policy/View 两侧都隐藏基类比较；保留完整逐字段比较。修正版重新双独立复审及第四轮 Windows 40362 全量构建，不掩盖前三轮失败。

最终比较修正版双独立完整源码审查均为局部 PASS。Windows Release 第四轮 40362 退出 0（803 任务计划），四项产物核验通过，realtime CLI 7177728 字节、2026-11-06 13:53:42。远端两批 4＋6 源码 SHA256 核验同步成功；RKMPP SSH 72181 正在 8 核增量 all-target 构建（228 项），等待终态。

下一步时钟拆分已按实际代码核对：validateSourceClock 不依赖尚未求出的 source startup 容量，但 demux 仍检查既有 prepared input 包数/字节证据，必须保留。固定 timing/clock/identity 声明由共同 compiler 生成；真实 route/acquisition 与数量在 bind 时核对。旧 plan 必须组合调用同一固定准备与 bind，rebind 复用声明核验和数值写入，不新增第二套协议算法；固定 owning 字符串仍需预算，不因模板或类型拆分而免计费。

RKMPP SSH 72181 已完成 228/228，8 核增量 all-target 终态退出 0。本批修正版两平台构建通过，双独立源码复审 PASS；保留前三次 Windows 失败。10 源码 UTF-8 无 BOM、CRLF 与 diff 检查通过。本轮未运行 local/realtime 媒体链路，未生成抓包、截图、录制、临时日志或临时脚本；所有既有未跟踪文件保持。固定 owner 预算、clock/assembly 固定准备、实际 solver 接线及原规格实流仍待完成。

## 2026-11-06 源时钟固定声明与容量绑定

固定 ClockRequest 不含 sourceCapacity；固定 assembly 仅保存 clock/generation/evidence、视频身份和 timing、可选音频身份/duration/decodeOrder，完整类型在 bind 时才取得 acquiring 数量和期限。原 plan 实际执行共同 prepareClock 与 bind；没有新增第二套 RTP/TS/demux 算法。

SourceClock validator 两入口复用同一模板，保留 demux prepared input 的包数/字节证据；timing compiler 同时用于首次准备和真实输入复核，保留 boundedReceive、音频分支、packet-duration 与整数范围检查。bind/rebind 先验证输入 timing 与固定声明，再由唯一 planAcquiring 校验成员、route/readiness、startup/acquisition 一致性及正数，最后写值。检查路径不复制身份字符串；demux 仍逐字段核对已有 clock，避免创建 owning 临时副本。

类型基类的两处 request 聚合构造与唯一 assembly 构造已同步，所有新增相等比较使用成员形式。双独立审查（residence_budget_audit、residence_budget_review_b）均为本批 7 源码局部 PASS；固定事实校验现在先于容量校验，多项同时非法时首错误可能变化，但原拒绝条件与错误保留。Windows Release 全量 72746 正在运行。

本批只拆共同编译阶段，固定 owning 字符串还未受完整租约约束；尚未替换 freezeBatch 的 nullptr，也未完成实际 solver 或原规格媒体链路。

源时钟拆分 Windows Release 72746 首次全量退出 0（803 任务计划），四项产物核验通过，realtime CLI 7189504 字节、2026-11-06 14:12:27。远端两批 3＋4 源码 SHA256 核验同步；RKMPP SSH 67716 完成 124/124，8 核增量 all-target 退出 0。7 源码双独立审查 PASS、UTF-8/CRLF 与 diff 检查通过。本批没有运行媒体链路或生成媒体测试临时产物，完整目标仍未完成。

## 2026-11-06 连续准备 decoder 契约的实际预算

核验发现 SourcePreparer 原先在 reserve 前复制整份 sessionVideoPlan，实际只用于 10 项准备会话一致性检查。现用 application 层不可变 DecoderContract 保留完全相同的字段与 optional presence；创建成功后才发布，后续 probe/filter 失败仍不撤销原承诺。该合同不决定解码器或新增 fallback。

普通元数据按已审计 stringCopy 逐项事前预留：3 个名称、可选 retention authority、3 个可选 HardwareDescriptor 各 4 个字符串，最多 16 项；engaged 空字符串的 Debug proxy 仍计费。构造直接从 const 成员 fresh-copy，无拼接和中间字符串。root 的预算 memory_resource 经 allocate_shared 计对象/control block 与真实分配头，普通字符串租约最先初始化、最后析构；零请求字节不创建租约，资源块自持 owner 至最后 weak owner，构造失败自动回收。

首轮交叉审查整体 FAIL：新 application cpp 未列入 CMake 显式源列表。构建前补齐后，residence_budget_audit 与 residence_budget_review_b 对 5 文件完整复审均局部 PASS。Windows Release 全量 58419 正在运行。该修复只关闭 session 合同副本的预算，原 candidate、raw/request/facts、日志以及后续固定 owner 的其他元数据仍须处理；不能据此宣称准备存储已全面闭合。

Decoder 契约 Windows Release 全量 58419 终态退出 0（804 任务计划），四项产物核验通过，realtime CLI 7195136 字节、2026-11-06 14:24:49。5 文件 SHA256 核验同步后，RKMPP SSH 16726 完成 7/7，8 核增量 all-target 退出 0。依用户最新要求，RKMPP 继续采用增量构建。本批未运行媒体验收，未生成抓包、截图、录制、临时日志或临时脚本；原有未跟踪文件保持不动。双独立源码复审局部 PASS 不代表合屏完整验收通过。

## 2026-11-06 固定源构造路径复核与输入中间副本移除

当前固定 owner 前置不能直接调用旧 Kernel：其 plan 仍要求容量，CapacityState 又借完整 runtime。下一完整接线须由 Kernel prepareFixed 生成 input/audio/sync/clock/运行政策，候选仅更新数值，最终 bind 消费同一 owner；原 plan 必须组合调用这条链路，不能保留第二次拥有型规划。双独立调查确认此边界。

实际分配还有 input 嵌套协议字符串、audio resolvedOutput 的 7 字符串与 decoder 的 3 字符串及可选 authority、RTP identity 拼接、startup/timing/assembly 身份副本和 groupKey 副本。视频计划不能在现有 SourcePlanner 调用处直接移动：Preparer 后续 prepareReplay 及 decoder/filter 报告仍读取它。稳定借用只避免新副本，不证明原 video/raw/request 已预算；最终移动需调整真实消费顺序或为必要副本先取得租约。

本批只修改共同 InputPlanner 的实际 applyNodePlans：URL/SDP 同步借用 string_view，协议事实借用指针，在目标 optional 直接构造；音频直接构造在 draft 的 optional，消除按值协议参数及局部 audioInput 的中间拥有者。C++ 标准的 [string assign](https://eel.is/c%2B%2Bdraft/string.assign) 与 [optional emplace](https://eel.is/c%2B%2Bdraft/optional.optional) 是此所有权重构的依据；没有改变媒体算法、参数或平台 adapter。所有调用方使用独立输入和新建 draft；异常退出时半成品销毁，不继续使用。string assign 的实际分配仍须核算，不能套 fresh-copy 费用宣称完整预算闭合。

Windows Release 全量会话 8307 正在运行。尚未执行本批媒体链路；freezeBatch 的 nullptr 门禁保持，完整 solver 与实流验收未完成。

本批双独立源码审查 residence_budget_audit、residence_budget_review_b 均局部 PASS。Windows Release 8307 全量终态退出 0（804 任务计划），四项产物核验通过，realtime CLI 7193600 字节、2026-11-06 14:38:32。RKMPP SSH 75141 在同步前后核验源码 SHA256，8 核增量 all-target 完成 4/4、退出 0。本批未运行 local/realtime 媒体链路，未生成抓包、截图、录制、临时日志或脚本；既有未跟踪文件保持。完整 owner 预算、solver 接线及原规格实流仍未完成。

## 2026-11-06 固定同步和时钟声明的真实容量视图

AvSync 的 startup view 可以借固定政策并持有实际容量，plan view 借固定字段；原 bind 在移动 owner 前用这组完整字段运行原 validatePolicy，成功后才构造拥有型产品。未补默认数量或削弱 validator。

Clock 的 planCapacityView 先共同检查真实 timing/固定声明，再由原 planAcquiring 验证成员、route/readiness、startup 数量及 acquisitionWindow，最后构造借用 assembly view。原 bind 实际消费该 view，rebind 复用同一 capacityForAssembly，不复制协议算法、不生成 acquiring=0 占位。optional 音频在共同检查通过后才解引用；固定 owner 的 const/非 const 右值入口均禁止。bind 移动后只读取 view 自持数值，不读已移动身份。

两位独立审查 residence_budget_audit、residence_budget_review_b 对本批 5 源码均局部 PASS。Windows Release 全量会话 77536 正在运行。本批使固定声明可直接组成可校验候选，但 Runtime CapacityState 仍借完整 runtime；尚须迁移共同固定事实视图、Kernel 固定准备及预算，再前置到 freeze。生产 nullptr 门禁仍保留，不能视为完整 solver 或媒体验收通过。

下一 Runtime 迁移使用不拥有字符串的固定事实 view，分别从完整产品和固定 owner 映射同步/启动政策/assembly 的固定基类、audio/isolated input/groupKey、threading/activation/transition 和 inputAudioSampleRate；timing 仍显式传入，不由 sampleRate 重建。State 工厂先取得真实 acquiring，保存纯数值 startup（含 maximumGap）/servo/queues/edges/correction。旧完整产品只允许发布回原 owner 地址，固定 owner 走独立 consume，并在移动前结束候选借用；不得提供调用方可伪造的 owner token 或默认可用状态。此段为已审查下一实施边界，尚未实现。

Windows Release 77536 全量终态退出 0（804 任务计划），四项产物核验通过，realtime CLI 7195136 字节、2026-11-06 14:48:05。RKMPP 4＋1 文件同步前后 SHA256 核验通过，SSH 6984 以 8 核增量 all-target 完成 29/29、退出 0。5 源码双独立审查局部 PASS、UTF-8/CRLF 与 diff 检查通过。本批未运行媒体链路或生成抓包、截图、录制、临时日志及脚本；既有未跟踪文件保持，完整目标仍未完成。

## 2026-11-06 源 Runtime 固定事实与容量状态实际分离

原 planFacts 已实际组合 prepareFixed、planCapacity、bindFacts。固定 Request 不含 queues/sourceCapacity；固定产品持有 audio/isolated input/groupKey、SyncFixed、ClockFixed、threading/activation/filter/transition。候选 State 通过同一不拥有字符串的 FixedView 借固定产品或既有完整产品，原 Kernel rebind 已使用此映射。完整同步、输入、queue/route、acquiring 与音频修正政策仍复用原 planner；cadence 的 ledger 域和 SourcePresentation 容量域保持。固定 transition 现在先于候选容量检查生成，多项同时非法时首错误可能改变，不省略原门禁。

Clock bind 与 Runtime bindFacts 复用唯一私有 consumeCapacity，验证原声明的引用、成员、generation/evidence/decodeOrder、正数量与期限后移动；没有第二套 assembly 组装。Runtime 的固定产品仍在现有 seal 后调用路径内，本批没有提前占用未预算的 freeze 工作区。

初轮交叉审查整体 FAIL：新增 State 的公开可写字段允许合法规划后修改 acquiring/queues 再绑定，原身份检查不足以保证已验证候选。修复为私有字段、仅 RuntimePlanner 可更新、move-only 且移动后原对象失效；rebind 先置失效，全部成功才恢复，失败后不能重试或发布；bindFacts 消费右值候选并在移动固定事实前置失效；Clock 消费入口私有且只允许 RuntimePlanner 调用。两位独立审查 residence_budget_audit、residence_budget_review_b 对修正版 8 源码均局部 PASS。

固定 owner 必须在候选使用期存活、不移动、不改写。普通固定产品字符串仍需构造前预算；Kernel 固定准备、Preparer 前置、合法基线、具体 solver 和实流验收仍未完成。Windows Release 全量会话 13622 正在运行；保留 freezeBatch 的 nullptr 失败门禁。

Windows Release 13622 全量终态退出 0（804 任务计划），四项产物核验通过，realtime CLI 7214080 字节、2026-11-06 15:03:02。远端 6＋2 源码同步前后 SHA256 核验通过，新头文件触发正常 CMake 重配置；RKMPP SSH 45885 以 8 核增量 all-target 完成 11/11、退出 0。修正版 8 源码双独立复审局部 PASS，保留初轮整体 FAIL；UTF-8/CRLF 与 diff 检查通过。本批未运行 local/realtime 媒体链路，未生成抓包、截图、录制、临时日志或脚本；既有未跟踪文件保持。完整预算、Kernel/Preparer 固定准备前置、solver 与原规格实流仍未完成。

## 2026-11-06 Kernel 固定准备进入共同生产调用链

Kernel 的 FixedRequest 不含 capacity；prepareFixed 复用原 decoder/filter readiness、allocation/ownership、audio、prepared ingress 和 producer-holder 校验，按真实 timing 调用同一 StartupPolicy prepare，再生成 SyncFixed 与 RuntimeFixed。FixedProducts 拥有 input/video/runtime/holders，timing 按值保存；只借原 request/raw/deployment/audioRetention 的外部 referent，不借按值 request 的局部 owning 成员。外部对象必须存活到所有候选与 bind 结束；当前仍是 seal 后同步组合，不提前移动 replay/report 后续所需的 prepared video。

原 Kernel plan 实际调用 prepareFixed→bind；bind 保留 timing/capacity variant 匹配、原 StartupPolicyPlanner 的 route/acquisitionWindow/ledger retention 校验，以及音频 units/bytes/unitBytes 三项一致，再走 Runtime 容量规划与消费、原 storage/connections。Runtime 初次容量与 rebind 共用固定 startup 政策比较，除容量及 maximumGap 外等待政策/readiness/outputLead 必须一致，提供的身份也须匹配。容量域错误现在可能晚于固定准备错误返回，原拒绝条件保留。

迁移后全 src 确认旧 Runtime planFacts 及其专用完整 Request 已无调用者，删除该包装，保留唯一 prepareFixed/planCapacity/bindFacts。两位独立审查 residence_budget_audit、residence_budget_review_b 对最终 5 源码均局部 PASS。普通 fixed 产品仍未完整事前计费；Preparer 前置、候选工作区/合法基线、具体 solver 和原规格实流仍待完成，freezeBatch 的 nullptr 门禁保持。

Windows Release 82893 全量终态退出 0（804 任务计划），四项产物核验通过，realtime CLI 7218176 字节、2026-11-06 15:16:10。远端 3＋2 源码同步前后 SHA256 核验通过；RKMPP SSH 74435 以 8 核增量 all-target 完成 6/6、退出 0。最终 5 源码双独立复审局部 PASS，UTF-8/CRLF 与 diff 检查通过。本批未运行媒体链路，未生成抓包、截图、录制、临时日志或脚本；既有未跟踪文件保持。

预算范围复核：两位独立审查均 PASS。依据 MediaSourcePreparationStoragePlan 的 logical retained payload/public headers 声明及 C API 提案 r182，准备额度不是 RSS/不透明设备总内存；此前把全部日志列为本批前置条件过宽，此处收窄。下一批必须覆盖新增固定 owner/workspace 的 input/audio/group/sync/timing/assembly、嵌套分配、临时/Debug proxy、对象/control block 及最终消费峰值；不要求先重构全进程日志和所有历史元数据。借用既有事实不增加 backing，但不证明原存储已计费；新增复制必须计费。原媒体/输出/画布/目录预算不得减免，完成报告须明确新增域与历史未覆盖域，不能宣称整体分配硬界已经闭合。

## 2026-11-06 固定准备实际分配清单修正与临时构造消除

逐字段复核纠正此前口述计数：每路 input 为 4 直接字符串＋transport.bindAddress＋depacketizer 的 2 字符串＋envelope 的 3 字符串，共 10 项；audio resolvedOutput 为 5 直接字符串＋profile.canonicalName，共 6 项，非 7 项。普通 audio 自身、decoder authority、每成员 protocol/startup/timing/assembly 身份、groupKey 与 producer-holder authority 仍需覆盖。runtime.transition 的 participant vector 及每项 requiredChildren vector<string> 也是固定产品目录，不能漏计。

本批将 decoder retention 原 encode 的完整校验移到模型唯一 validate，encode 和 holder 直接共用。保留 threadCount/threadType/authority、INT64_MAX 分段短路和非零总量检查及原错误文本；holder 不再为校验创建 7 项 options。envelope 已校验且角色匹配后直接取 maximumAccessUnitsPerPush，与原 asInputPayloadEnvelope 映射相同，避免为一个数创建 vector 和 authority 副本。校验先于 holder authority 复制，非法输入可更早返回原错误；正常输出事实不变。

两位独立审查 residence_budget_audit、residence_budget_review_b 对 4 源码均局部 PASS，新 model cpp 已确认命中现有 graph CONFIGURE_DEPENDS。Windows Release 全量会话 90630 正在运行。此批只消除实际准备临时分配，新增固定 owner/workspace 的完整事前预算仍待实现。

后续 STL 几何必须分别证明 fresh-copy、空串 assign、拼接和 vector 增长，不能套同一费用公式。若配置未完成权威审计，应在对应新分配前显式 Unsupported，不得漏计 Debug proxy 或使用估算 fallback；现有 DecoderContract 的已审计 Debug fresh-copy 能力应保留。此处仅记录设计边界，尚未新增配置拒绝门禁，也未宣称 Release 或 Debug 总预算闭合。

临时构造消除批次：Windows Release 90630 全量终态退出 0（805 任务计划），四项产物核验通过，realtime CLI 7218688 字节、2026-11-06 15:30:06。4 源码同步前后 SHA256 核验通过，新 model cpp 触发正常重配置；RKMPP SSH 72980 以 8 核增量 all-target 完成 194/194、退出 0，出现 FFmpeg sample_fmts/supported_samplerates/pix_fmts 弃用警告，未修改依赖。双独立源码局部 PASS，UTF-8/CRLF 与 diff 检查通过。本批无媒体链路测试或抓包、截图、录制、临时日志/脚本，原有未跟踪文件保持。

## 2026-11-06 transition 固定成员目录的计数与构造统一

processingParticipants 使用唯一 visitor 两遍输出同一成员集合：先计数，再 reserve 并直接 emplace 字符串；不再用字符串初始化列表、头部 insert 或 source 函数末尾追加。保留 demux video/audio 前缀、startup/video/audio/RTP 顺序及 aggregate_source→selected aggregate_audio；CanonicalLineage 在 AudioCorrection 前，后者的一项 child 也直接构造。输出域显式不选择 aggregate_audio，其后输出参与者追加逻辑未变。

两位独立审查均局部 PASS，原输入拒绝条件、错误文本及成员顺序保持。此处 reserve 按实际成员数请求，不能仅凭标准把 capacity 当作精确值；本机 MSVC 14.51.36231 的 vector::reserve→_Reallocate→xmemory::_Allocate_at_least_helper 在 C++20 下调用 allocator.allocate(count)，已定位其实现路径，最终费用仍须结合实际目标 STL adapter。没有新增事前租约，不能宣称 transition 或固定 owner 预算已闭合。

Windows Release 4099 全量终态退出 0（805 任务计划），四项产物核验通过，realtime CLI 7218176 字节、2026-11-06 15:37:50。源码哈希核验同步后，RKMPP SSH 25831 正在进行 8 核增量 all-target 构建。

Transition 批次 RKMPP SSH 25831 终态 4/4、退出 0；两平台构建及双独立源码局部审查通过。UTF-8/CRLF 与 diff 检查通过。本批没有运行媒体链路，也未生成抓包、截图、录制、临时日志或脚本；原有未跟踪文件保持。固定 owner 事前预算、solver 接线及原规格 Windows/RKMPP 实流仍未完成。

## 2026-11-06 固定产品借用原视频计划

Kernel 的 FixedRequest、FixedProducts、PlanningProducts 改为借用 const video；唯一生产调用方 SourcePlanner 的按值 request 保持稳定至全部校验结束，最后直接将 request.video 移入 SourcePlan。Kernel 不再在三个中间产品间移动拥有型 video；失败路径仍由原 request 析构。引用 owner 必须覆盖所有候选和 bind，禁止保存从临时 video 构造的产品。

两位独立审查 residence_budget_audit、residence_budget_review_b 对冻结的 3 源码均局部 PASS。此变更只允许未来 freeze 前借用 prepared video；当前 Preparer 到 SourcePlanner 的初始副本仍存在，未因本批消除或计费。原容量、producer holder、sealed admission 校验保持。

后续实施收敛为一个完整批次：新增固定 owner 构造前预算、具体 batch solver、freezeBatch 接线及 seal 后消费同一事实。候选数组使用现有 MediaFixedCapacityStorage，拓扑/residence/cost 目录复用 MediaPlanningCatalog；普通字符串及元素嵌套分配仍需单独计费，不能因外层 PMR 认定已覆盖。锁内只使用 solve 传入的实际 snapshot/partition，不再次获取 session 锁；非法事实与 Unsupported 不能作为预算不足缩小搜索。当前 nullptr 门禁仍保留，合屏实流尚未恢复验收。

Windows Release 全量会话 79248 终态退出 0（805 任务计划），四项产物核验通过，realtime CLI 7217152 字节、2026-11-06 15:49:20。3 源码同步前后 SHA256 核验通过；RKMPP SSH 1263 使用 8 核增量 all-target，5/5 完成、退出 0。两位独立源码审查均局部 PASS，UTF-8/CRLF 与 diff 检查通过。本批未运行 local/realtime 媒体链路，未产生抓包、截图、临时录制、日志或脚本；原有未跟踪文件保持。固定 owner 预算、具体 solver 接线与原规格实流验收仍未完成。

## 2026-11-06 固定产品普通分配预算接入生产准备

Kernel prepareFixed 在构造 input/audio/sync/clock/transition/holder 前调用 FixedStoragePlanner::reserve。新增普通 backing 按实际空目标 assign、fresh copy、字符串拼接与确定的 reserve 操作计算；音频输出含 profile 共六字符串，有效 RTP 成员各保留 protocol/startup/timing/assembly 四份身份，group request 与结果两份，holder authority 单独计费。身份后缀与生产 planner 共用定义；transition 成员计数、字符串费用复用同一 visitor。准备过程中保守峰值预约保留到 owner 释放，不宣称是实际 RSS 或逐时刻最小费用。

新几何依据本机 MSVC 145/202604 的 xstring concat/_Calculate_growth 和 vector::_Reallocate→C++20 allocate(count)，以及 RKMPP 实际 /opt/mt-tools/gcc-12.4-native/include/c++/12.4.0 的 _M_create 与 vector::reserve。GCC 空串 assign 可能按旧 SSO capacity 倍增；const string 拼接先复制再 append，费用包含增长时旧、新背板。新增固定准备路径只接受已审计 Release move 几何，Debug 或未知 STL 在拥有型构造前明确 Unsupported；既有 DecoderContract 的 Debug fresh-copy 支持未删改。

租约控制块用预算 resource 的 allocate_shared，真实请求另行计费。共享句柄放在 FixedProducts、PlanningProducts、SourcePlan、最终 RuntimeDomainBinding 的首字段，并由 Binder 显式传递。设计审查曾判 FAIL：仅让 transition 持租约时，domain 的 role 比 sync plan 先析构，会提前退费；最终方案由 domain 自持，最后释放。两位独立审查对冻结的 14 源码均局部 PASS，额外核验 inputDraft 默认构造在审计 Release 没有额外非空字符串或 vector 元素 backing。

范围仅为新增固定产品普通 backing 和构造峰值；历史 video/raw/request、后续独立图复制及未来固定 owner/候选数组仍不得冒充已收费。未来数组须通过预算资源实际构造，纳入 root otherRetainedBytes。本批未提前调用 freeze 前准备，nullptr 门禁仍保留；具体 solver、最终 sealed 复算与原规格实流待完成。Windows Release 全量会话 26341 正在运行，RKMPP 将继续 8 核增量。

Windows Release 会话 26341 终态退出 0（806 任务计划），四项产物核验通过，realtime CLI 7224320 字节、2026-11-06 16:07:31。14 源码分批同步前后 SHA256 核验全部通过；RKMPP SSH 45071 已在原目录启动 8 核增量 all-target，新文件触发正常重配置。本批没有媒体验收或临时测试产物。

后续基线审查确认 route 无需依赖完整 runtime：先 describe(members/readiness)，由 startup 数值视图和候选 queues 调用 planSynchronizedSource，取其 metadata/atomicMetadata 进行 route bind，再进入 Runtime 核验同一产品。packet 进展要求 reachableGraphHolders 至少 startupPackets＋acquiringPackets＋1（decoded video startupPackets 为 0，audio 为固定 Na）；decoder/filter retention 与 atomic completion 仍计入完整存储，不能因进展式约去就免计费。frame 由原 SourceFrameOwnershipStoragePlanner 核验。队列下界必须反推实际 topology/atomic-port，不能统一填 1 或解析错误文本扩容；只有已合法候选的明确总费用超预算才允许缩小搜索。

RKMPP SSH 45071 终态 61/61、退出 0，保持 8 核增量 all-target。最终 14 源码双独立审查局部 PASS，15 修改文件 UTF-8/CRLF 与 diff 检查通过；未运行任何 local/realtime 媒体链路，无本轮抓包、截图、临时录制、日志或脚本，原有未跟踪文件保留。预算已进入现有 prepareFixed 生产调用，但 SourcePresentation 的 freeze 前工作区、具体 solver 和原规格实流仍未完成，不能标记合屏验收通过。

## 2026-11-06 固定准备前移到真实 freeze 前调用

Preparer 使用 planningResource 上按源数一次分配的 MediaFixedCapacityStorage 保存全部 FixedProducts，实际在 freeze 前 prepareFixed。数组地址稳定，候选借用尚未生成；原 facts/prepared video/deployment/session owners 在消费完成前不移动。continuousIngress 先验证连续准备 owner/receiver/phase，再借已安装候选 ingress；Kernel 当次复制所需事实，不跨 seal 保存该借用。原 seal 仍重读 ingress 并核验 sameProduct 及 frozen 门禁。

SourcePlanner 删除整个重复 Request，直接接受 FixedProducts、sealedView、emission，所有固定事实只有一个来源。两路 sealed ingress sameProduct 与 audioRetention 同地址检查后调用 Kernel bind，不重复 prepare。最终仍从同一外部 prepared video 显式复制一份，原计划继续供 replay/report 使用，未把历史 video 副本归入本批已收费域。已删除全 src 无调用者的旧 Kernel plan 包装与请求类型。七源码双独立审查均局部 PASS；容量域、sealed owner、retention、producer holders 和 reservation 门禁保留，nullptr 仍拒绝 SourcePresentation。

Windows Release 42441 全量终态退出0（806任务计划），四项产物通过，realtime CLI7225856字节、2026-11-06 16:23:20。实际四路诊断见 [r396](realtime-composition-r396-validation.md) 与 [r397](realtime-composition-r397-validation.md)：默认沙箱r396匹配RTP包为0、30秒probe超时；主机r397同规格收到包、四路解码准备后约4秒自然exit1，停在完整存储准入门禁。按当前控制流，该门禁前固定准备数组已全部构造成功；不能据此声称输出/失源重入或整体预算通过。两轮进程/实际日志已按清单清理，不保留临时测试设施。

七源码同步前后哈希全部核验，RKMPP SSH90320正在原目录以8核增量all-target构建。下一批仍需合法候选基线、聚合/拓扑工作区完整计费、具体solver和sealed独立费用复算；本批不是合屏完成。

RKMPP SSH90320终态14/14、退出0，使用8核增量all-target。七源码双独立局部PASS，两平台构建完成；r396/r397失败记录完整，本轮测试文件残留为0、复用120秒源保留。未推送、合并、部署或修复外部依赖；不创建成功媒体验收提交。

## 2026-11-06 聚合候选容量刷新

当前真实入口仍在 freezeBatch(nullptr) 拒绝完整存储准入（r397），候选 solver 尚未接入。源码核验发现聚合 plan 是源/拓扑/驻留/费用候选链中尚无无分配刷新能力的一环。本批复用原 candidate 数量、audio sample、lineage 与 storage 算式，将首次身份构造与容量重算放在同一 compileAggregate 内核；生产 plan 调用同一内核，新增内部 rebindCapacity 只刷新数值。不是独立媒体链或新增公共参数。

已重新读取 GStreamer 官方 allocation 文档：https://gstreamer.freedesktop.org/documentation/plugin-development/advanced/allocation.html 。沿用先协商有限数量/分配属性再激活的原则；其规则不能替代本项目实际费用证明。线程、背压、所有权、平台 adapter 与运行时算法不变。初次 sources 目录按实际数量 reserve；仅此不能证明新增聚合普通 backing 已事前计费，该预算仍须在候选工作区构造前完成。

刷新借用同一不可变源声明，仅 queue/reachability 容量变化；输出、canvas、端口、字符串及目录保持原 owner。核对源数、group、媒体成员、identity、lineage product、generation、选中音频起点与 normalization，lead 不得超过已准备界；成功路径不创建容器或字符串。失败保留原错误，工作区整体失效，禁止发布或把非法事实当成预算不足缩小搜索。尚未接具体 solver、sealed 独立复算或完成原规格实流。仓库要求真实链路驱动并禁止 Windows 测试脚本，本批不新增测试框架或伪造媒体事实的单元测试；源码审查与构建不能替代实流验收。

两名独立审查 residence_budget_audit、residence_budget_review_b 对两源码与本节均局部 PASS；固定声明不变及刷新失败丢弃仍是调用契约，不宣称有自动回滚。Windows Release 全量会话30563终态退出0（806任务计划），四项产物核验通过，realtime CLI7226368字节、2026-11-06 16:46:12。两源码同步前后哈希核验通过，RKMPP会话99659正在原目录进行8核增量all-target。本批没有媒体链路测试或临时测试产物，最新SourcePresentation实流证据仍为r397失败；未运行local测试。具体solver、聚合首次backing预算和sealed独立复算未完成。

RKMPP会话99659终态6/6、退出0，保持8核增量all-target；本轮三文件UTF-8/CRLF及diff检查通过。无媒体测试临时产物，既有未跟踪文件保留。未推送、合并或部署，不创建成功媒体验收提交。

下一步只读审查再次确认：现有 checker 可验证进展，却没有从 locked snapshot 反推合法 queues 的入口。须在数值基线 planner 中复用实际 input/branch/atomic 连接约束：视频 packet 的 startupPackets为0，音频为固定Na；reachableGraphHolders须覆盖startupPackets＋acquiring＋1。decoder frame下界含filter retained inputs，不能把需求随意均摊或统一队列设为1。还须定位prepared-frame videoUnitBytes、maximumGap、typed queue贡献与atomic发布批量的权威产品；没有这些事实不能调用候选费用checker试错扩容。
