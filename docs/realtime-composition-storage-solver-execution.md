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

## 待闭合的搜索目标提案

提案（尚未通过双独立审查，不作为已批准算法实施）：对通过全部合法性与进展检查的基线 L，只对独立可增长的数量轴使用 x_i(k)=L_i+k，寻找全源联合费用可承受的最大整数 k。prefix 和音频 startup 固定；由其他轴派生的队列容量不重复当作独立轴。各字段表示范围共同限制 k，不允许静默截断个别轴。

这是最大共同数量增量目标，不是字节公平、吞吐最优、完整逐级 max-min 公平或任意多维最优。k+1 不可行时允许余量闲置，不再按源顺序临时分配。必须先闭合合法基线和队列派生关系，再在固定 codec/filter、成员、readiness、拓扑 shape 下验证费用单调性；非法配置、平台不支持和超预算须区分，不能都作为二分搜索的“太大”。

## 下一实施批次必须形成的完整调用链

1. 具体 solver 借用已准备的源事实、video plan、output runtime 和 canvas geometry；在 freeze 的全部源锁下仅读取传入 snapshots，不再调用 session accessor。
2. 同一次候选计算生成 startup、queues；复用 `MediaRealtimeEdgePolicyPlanner` 与 `MediaStartupRoutePlanner::bind`，形成 `MediaAvSourceCapacityFacts`。复用 source kernel、aggregate planner、topology kernel，更新全部 edge policy 和 incident capacity，不复用陈旧数值。
3. 容量选择策略通过双独立审查后，用实际 topology/residence 调用 `MediaPreparedSourceCandidateCostPlanner::plan`。所有源联合求和；root 的 probe/output/canvas、batch 六数组只按所属域计一次。新增候选 workspace 必须显式受预算约束，不能假称已有六数组已覆盖嵌套 vector/string。
4. 搜索结果生成恰好 sourceCount 个 admission，放入 batch.owner 上的固定容量结果。替换实际 nullptr；由 freeze 原事务检查后一次性发布，任一步失败保持未冻结状态与原错误。
5. sealed 后用现有共同 planner 独立复算 topology、费用和容量一致性。原规格 Windows 四源 realtime/HEVC 重排、VLC 快照、失源黑屏/静音与重入通过后再做 RKMPP 实流。

未闭合项：异构数量轴分配策略的工业依据及适用性、队列派生及合法基线的完整进展证明、候选工作区计费、具体 solver 与生产接线、sealed 完整一致性。不要继续提交只有独立费用 helper 而不推进这些调用关系的修改。

## 审查与当前验证

presentation_plan_review：上述共同增量目标条件 PASS，要求合法基线、固定 shape 单调性、字段上限及准确声明目标边界。timing_path_review：确认表中事实下界及有限容量尝试启动路径；明确反对“构建前保证正 preroll 必定成功”。复审时 timing_path_review 对共同 k 策略判 FAIL：上述工业资料不直接证明异构数量轴同步增加的适用性；presentation_plan_review 为条件 PASS。故整体设计审查未通过，不能据此实现该搜索策略；必须补充适用依据或选择已有充分依据的策略。事实核验与设计讨论不能记作代码或实流 PASS。

本轮 RKMPP 8 核 all-target 增量构建退出 0，Ninja 报 no work to do；未修改生产代码，未产生新媒体测试或临时产物。没有推送、合并、部署或外部依赖修改。RKMPP 身份传播问题仍单独保留，不能由 Windows solver 接通推定已解决。
