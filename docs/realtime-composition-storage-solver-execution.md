# 合屏联合存储 solver 实施决策

状态：2026-11-05，基于 `d4237d73` 及本轮工作树核验。源、拓扑、residence 与费用的固定目录刷新已具备；尚未实现具体 solver；`MediaRealtimeCompositionPreparer.cpp` 的 `freezeBatch(rtpSources, nullptr)` 仍阻止 SourcePresentation 启动。本文件只收敛接线方案，不代表实流通过。

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
