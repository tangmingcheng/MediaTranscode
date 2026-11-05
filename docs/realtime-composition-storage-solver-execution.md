# 合屏联合存储 solver 实施决策

状态：2026-11-05，基于 `92933062` 核验。尚未实现具体 solver；`MediaRealtimeCompositionPreparer.cpp` 的 `freezeBatch(rtpSources, nullptr)` 仍阻止 SourcePresentation 启动。本文件只收敛接线方案，不代表实流通过。

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
