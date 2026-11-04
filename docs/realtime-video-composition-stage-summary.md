# 实时多路合屏工作阶段总结

本次核验于 2026-11-04；最新真实链路记录为同日的 r385（失败）；最近完成四阶段行为复验的是 r384。按工作目标归纳为六个大阶段；阶段互有交叉，不表示前一阶段已完成全部验收。本页记录当前阶段，不逐轮罗列开发流水账。

**当前结论：多路合屏核心链路已实现，有真实三格、全失源黑屏和重入恢复画面；整项能力尚未完成。当前重点是第五阶段的完整预算准入，以及第四阶段仍待完整证明的 Windows 实时发送。**

| 阶段 | 工作主题 | 当前状态 |
| --- | --- | --- |
| 一 | 能力评估与架构边界 | 已形成方案及范围约束 |
| 二 | 每源准备、所有权、时钟与生命周期 | 已接通，复杂失败/取消分支待补证据 |
| 三 | 网格合成、音频选择、CLI 与 C API | 核心链路已实现，入口矩阵未验收完整 |
| 四 | Windows 实流、失源恢复与运行诊断 | r384 完成四阶段行为；r385 因调度等待导致上游失约而失败，稳定性未通过 |
| 五 | 统一预算、全图拓扑与早期规划 | 纯 residence、共同源内核已实际消费并局部双审通过；早期费用与联合求解未完成 |
| 六 | 整体审查、完整矩阵及 RKMPP | 尚未闭合，须先通过 Windows |

## 阶段一：评估现有架构，确定实现边界

- **目标**：判断现有 DAG、硬件后端和 FFmpeg 能力是否适合扩展合屏。
- **已做**：确定 2～4 路 RTP/RTCP、固定网格、指定一路音源；调查 Windows CUDA、RKMPP/RGA、设备身份、异步完成和错误传播。明确由 planner 规划、复用生产 DAG，不另建样例处理链。
- **状态与证据**：设计和能力调查已有记录。`overlay_cuda` 的错误传播风险属于依赖源码证据，不等于已复现 CUDA 启动错误；外部 FFmpeg 修复不在批准范围。
- **剩余**：平台能力调查不能代替完整真实链路验收。见[能力评估](realtime-video-composition-capability.md)、[总计划](realtime-video-composition-plan.md)。

## 阶段二：建立每源准备、所有权、时钟与生命周期

- **目标**：各源独立准备、失效和重入，协议会话、解码器及媒体对象有明确交接和清理责任。
- **已做**：接通协议事实、首帧准备、已打开解码器交接、RTP/RTCP 时钟与 CNAME、每源代次、发布权限和旧代排空；逐源等待时间可配置。准备顺序已改为各源事实就绪后立即接管协议会话，再处理解码器候选，保留原期限、容量失败和取消语义。
- **状态与证据**：已有局部双审、Release 构建和真实失败驱动修复；r374 日志直接证明协议接管先于解码器契约绑定，后续 r375/r377 有真实重入画面。
- **剩余**：复杂取消及持有预约状态下清理仍缺专门运行证据，普通 caller-stop 不能覆盖全部分支。见[解码器交接](realtime-video-composition-decoder-handoff.md)、[协议代次交接](realtime-video-composition-protocol-handoff.md)、[准备接管修订](realtime-composition-preparation-handoff-plan.md)。

## 阶段三：接通合成输出、音频选择及对外入口

- **目标**：在生产 DAG 内完成网格合成、持续音视频输出及调用方控制。
- **已做**：实现源缩放、画布、音视频聚合、指定音源、失源黑屏/静音逻辑，复用编码、封装和发送链路；接入 realtime CLI。独立 C 配置和 `mt_beta_realtime_start_composition` 已实现，包含已批准的协议事实、逐源等待、准备存储上限及音频目标码率，保持旧类型布局设计。
- **状态与证据**：CLI 已输出真实三格；C 入口有源码审查和链接符号证据。画面恢复不等于静音及内容级 A/V 已通过，CLI 也不能代替直接 C 调用验收。
- **剩余**：C API 完整调用方生命周期、旧 ABI 实际回归、异构输入和参数正负向矩阵。见[C API 提案](realtime-video-composition-c-api-proposal.md)、[C API 当前验收缺口](realtime-composition-c-api-current-gates.md)。

## 阶段四：Windows 原规格实流、失源恢复与运行诊断

- **目标**：验证正常合屏、全失源持续输出、重入恢复、停止排空，以及时序、发送和资源释放。
- **已做**：统一使用指定 120 秒连续源、FFmpeg → realtime CLI → VLC 链路和 VLC 原生快照；观察 CPU、内存、A/V 漂移、调度与发送期限。增加同一最差成功提交序号的预约/等待/提交诊断，逐轮归档后清理临时产物。
- **已取得的行为证据**：r375 和 r377 均以本轮独占 VLC 观察“三格 → 全黑 → 所选源重入 → 全黑”。规格为三源 H.264/AAC → HEVC CBR 8000 kbps、1280×720/30fps、AAC CBR 192 kbps → MPEG-TS/UDP。r377 三初始源及重入均完整 120 秒/3600 帧退出 0，CLI caller-stop 退出 0；226085 次预约/释放匹配归零、errors=0，一次容量等待已恢复。**这些是画面与停止行为证据，不是整体验收 PASS。**
- **保留的历史失败**：r380 取得“三格 → 全黑 → 所选源重入”原生快照，但重入近结束时 sender seq171936 失败、CLI 退出 1，未完成最后全黑检查。原三源与重入源均完整 120 秒退出 0；219474 次预约/释放归零、errors=2、pressure=0，不改变失败结论。失败 batch seq171904 计划等待 0.21144 ms，实际 wait API 33.9256 ms；同批 CUDA event 46.8623 ms、host completion 46.2865 ms 是更早样本，不能归为此次 sender 首因。
- **r383 行为观察**：Registry 生命周期修正后，“三格 → 全黑 → 所选源重入 → 全黑”均有本轮 VLC 原生快照。三初始源及一次重入（并非四路同时输入）均完整 120 秒/3600 帧退出 0，CLI caller-stop 退出 0；errors、pressure、deadline_misses 均为 0，222990 次预约/释放归零。**四阶段行为和停止排空复验通过，不代表完整验收 PASS。**
- **诊断限制与剩余**：r383 sender delivery 仍为 `not_proven`，最差 lateness 为 5.267840 ms；缺本轮 wire、内容级 A/V/静音及长期内存证据。r381 已证明 worker/MMCSS 同线程注册与撤销成功，r383 补充行为复验，但不能证明硬实时保证或关闭历史发送失败。等待 API 长停留尚未分离计时器与调度原因，系统调度取证曾返回 Access denied。全失源仍应持续输出，caller-stop 不记为自然退出。
- **记录**：[r375](realtime-composition-r375-validation.md)、[r376](realtime-composition-r376-validation.md)、[r377](realtime-composition-r377-validation.md)、[r378](realtime-composition-r378-validation.md)、[r379](realtime-composition-r379-validation.md)、[r380](realtime-composition-r380-validation.md)、[r381](realtime-composition-r381-validation.md)、[r382](realtime-composition-r382-validation.md)、[r383](realtime-composition-r383-validation.md)（r383 的 16 项临时产物已清理，六个测试 PID 及端口无残留）、[Windows 调度取证](realtime-composition-windows-scheduling-evidence.md)、[worker 调度方案](realtime-composition-worker-scheduling-plan.md)。

最新补充：[r384](realtime-composition-r384-validation.md) 已完成共同账本修正后的同规格四阶段复验，四次源均120秒/3600帧退出0、CLI caller-stop退出0，220555次预约释放归零；sender最差迟到26.130860ms、delivery仍not_proven，16项产物已清理。该结果更新行为回归证据，不关闭上述验收缺口。

最新失败：[r385](realtime-composition-r385-validation.md) 在正常三格阶段退出1：音频等待缺失视频123.7101ms，交给mux时已晚5.103046ms。未完成失源与重入；三源仍完整120秒退出0，65163次预约释放归零不能改变FAIL。11项产物已归档清理，当前继续调查缺失流等待与视频晚到原因。

## 阶段五：统一存储预算、全图拓扑与早期规划（当前重点）

- **目标**：在同一部署预算内覆盖所有源、输出、协议、队列及对象保活费用，在输入冻结前形成完整准入契约。
- **已做的结构基础**：输入、启动、音视频分支、聚合、调度和协议输出共用节点/连接声明，真实 builder 返回实际绑定；全图核对角色、节点、边、成员全集和全部 incident 容量，保留 `Any` 与多连接语义。共同源规划内核已由生产 SourcePlanner 调用。
- **已做的费用基础**：输出编码器及画布在冻结前预约；纯 residence 与真实图持有关系逐项对照，aggregate 单 owner 覆盖两个保留位置。已补 lineage/wrapper、准备 lease、原始 packet/frame wrapper 等分项与独立 sealed readback，按真实持有根及分配域计算，不能把引用槽数直接当成独立对象数。
- **审查与构建证据**：上述依赖块已取得各自局部双审及 Release 全量构建证据；曾遗漏 prepared prefix 根的 PASS 已撤回并修正。Registry 生命周期修正完成双审、792 目标构建及 r383 行为复验。构建成功和旧 cadence 运行不等于新增费用路径已经验证。
- **最新共同账本进展**：runtime registry 的实际分配费用已接共同最终账本。初审发现其身份与普通引用项重复，修正为独立保留组、零媒体引用后，四文件已双独立复审 PASS；修后构建 63644 已成功（792 目标），r384 已完成四阶段行为与停止排空复验；旧版成功构建不能覆盖修正。
- **跨平台边界**：已按 RKMPP 实际 GCC12.4/libstdc++ 头文件补充 shared control/tree 几何 adapter，双审及 Windows 构建通过；尚无该 GCC 分支编译或 RKMPP 实流证据，不能外推完整平台支持。
- **Probe费用最新进展**：已实现分项planner及创建入口消费，并把内部费用预留为子预算；子预算控制块通过bootstrap allocator向原root计费并覆盖weak尾部。输出/canvas共同调用同步计费。九文件双独立源码PASS、793目标全量构建通过；r385实流FAIL，不把构建或分项完成等同于完整准入。
- **关键未完成**：源码仍只有 `CandidateCostPlanner::plan` 定义、没有 application 调用；probe 根、其余分配域和扣重仍需闭合，完整“身份贡献 → 按域费用 → 候选求值 → 冻结准入 → 最终核对”及联合 solver 未接通。当前仍为 `CanonicalIntervals`、`freezeBatch(rtpSources, nullptr)`；r383 不会运行新增 decoded timing 费用分支，SourcePresentation 尚未正式接通。
- **证据与状态**：这是完整准入的依赖实现阶段，尚未完成准入；详见[完整存储准入计划](realtime-composition-full-storage-admission-plan.md)、[分配域核查](realtime-composition-allocation-domain-audit.md)。

## 阶段六：整体审查、完整矩阵与 RKMPP 验收

- **目标**：将已实现、局部可运行的能力收敛到原定完成标准。
- **已有基础**：已有多轮独立局部审查、真实 Windows 运行记录及平台调查；尚无整项合屏完成结论。
- **必须完成**：闭合第五阶段费用与新路径；修复并重复验证 Windows 发送/调度失败；完成原规格 2～4 源、异构/HEVC 重排、失源与重入、内容级 A/V/静音、停止/取消、内存与发送证据，以及直接 C 调用和旧 ABI 验证；冻结后双独立整体审查；Windows 通过后再做 RKMPP 同规格验证。
- **交付边界**：当前仅本地开发与验证，不推送、合并或部署，不修改外部 FFmpeg。本页不替代逐轮实际命令及验收记录，也不据此声明任务完成。
