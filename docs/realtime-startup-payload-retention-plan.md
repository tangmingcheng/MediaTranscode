# 解码启动的媒体保留容量计划

## 当前结论（2026-10-15，r222）

整体仍为75/FAIL，两名独立只读审查均判完整display容量FAIL。该计划补齐r210后尚未实现的GraphEvent传播，不能用已有分配器计费或构建成功替代容量证明。未改变原合屏验收范围。

## 已确认的缺陷

- `MediaGraphPayloadResidencePlanner`只遍历同stream/payload边，解码帧到Metadata/GraphEvent后断链；目前仅initial output partition使用它，source frame配额尚未接入。
- `MediaFinalGraphResourceLedgerCompiler`的node `maximumBufferObjects`混合pending、所有incident queue容量、coordinator的video/audio startup容量。该混合数量不能统一乘batch，否则既混入另一流又产生重复放大的batch²项。
- `MediaRealtimeCompositionSourceResourcesPlanner`仍全部发出CadenceResidence；frame的prepared为0，startup保留只加入packet分支。ResourcePlanner虽能消费CountBoundResidence，但没有实际source产品，末尾reference storage编译不再调整已冻结quota。

## 工业依据与适用边界

采用Clang官方数据流分析描述的具名事实传播、转移函数与汇合原则（https://clang.llvm.org/docs/DataFlowAnalysisIntro.html），在有限DAG上计算保留可达性；它支持分析方法，不替本项目证明节点持有语义。共享引用和最后持有者释放原则沿用GStreamer官方bufferpool设计（https://gstreamer.freedesktop.org/documentation/additional/design/bufferpool.html）。本项目仍用既有节点、credit和PMR，不新增媒体路径或平台适配器。

## 必须分别表达的事实

1. 直接媒体对象上限：startup store、pending frame、滤镜保留等，按stream/payload累计。
2. 包装槽位：每个具名端口队列及节点内部可同时持有的envelope/release/transaction数量。
3. 每包装媒体上限：按源、stream/payload描述；只有包装槽位乘此上限。
4. 引用别名：同一release identity的preparation、reanchor、staged/reservation可共享媒体；数组和控制块费用分别计，媒体本体只在有完整身份与生命周期证据时去重。无法证明别名时保守保留，但不得将已按媒体计数的项再次乘batch。

令V/A为已规划startup video/audio容量（纯视频无A产品），已核验传播如下：

| 节点具名转移 | 媒体保留语义 |
| --- | --- |
| DecodedVideoStartup frame→out→Coordinator video | 每envelope一个decoded frame |
| CanonicalInput out→Coordinator audio | 每envelope一个音频AU |
| Coordinator video/audio→release | initial/next最多V/A，pass-through一个媒体，control无媒体 |
| EpochBinder release→transaction/preparation | 同一transaction共享引用，preparation仅initial |
| Sequencer transaction→bound_release | 保留原batch；reanchor仅复制包装/数组，媒体仍别名 |
| Sequencer activated及clock/control | 不传播媒体producer |
| Extractor preparation/bound_release→video/audio | 每输出元素一个媒体，原子提交可含多元素 |
| VideoPreparation frame→frame | 每元素一帧，pending/reservation分别核查 |

当前display路径Extractor输入是`bound_release`；旧无preparation capability路径才使用`in`，两者必须按实际拓扑区分。

## 实施顺序与不变量

- 在planner建立独立类型化payload retention事实；保留原metadata存储账目，不把混合maximumBufferObjects重新解释为批次数。事实来源限于真实节点成员、边容量、已有startup及filter合同。
- 按具名输入→输出传播，保留producer及源归属；遇到未描述的持媒体wrapper、缺失batch容量、流不匹配或溢出，在DAG构建前失败。禁止遍历全部metadata或把视频传播到音频出口。
- 先冻结reference/retention事实，再计算source CountBoundResidence，最后生成账户与总预算；不从最终quota反推自身保留数量。
- Coordinator、Binder、Sequencer、Extractor的并存持有按状态机逐项核查。Extractor initial确认前同时持preparation/pending/staged/reservation，正常路径同一release；下游队列与后续batch可并存，不能把所有批次都当同一身份。
- 解码surface进展与最低工作集单独证明；packet decoder retention不等于DPB/surface空位。保留设备显存observed-only范围，不虚构全VRAM保证。
- 无新线程或运行时探测循环；planner仅做有限图计算。运行时沿原背压、RAII lease和失败语义。控制终止路径的预算须在其生命周期设计中处理，不能简单在预算耗尽后临时分配EOF来宣称可靠终止。

## 验证与完成门槛

代码冻结后两名未参与实现的审查者逐节点对照以上事实，任一FAIL则修复并双复审；Release全量构建。随后仅realtime原规格120秒真实源，覆盖启动、失源黑屏/静音、重入、HEVC重排、内存趋势、内容AV和deadline，VLC原生快照；Windows通过后才RKMPP。记录实际命令与失败，再按本轮清单清除临时产物。未满足这些条件不开放生产入口、不标通过。本轮为设计/源码审计，无代码修改、构建或实流测试。

## r223：启动批次类型化产品已接入账本

新增MediaStartupBatchPayloadPlanner，按现有coordinator members/readiness和容量产品生成coordinator归属、视频Packet/Frame类型、视频媒体上限及可选音频Packet上限。容量复用运行时相同requiredPositiveIntNodeOption，缺失/零/负值/溢出直接失败；纯视频拒绝audio capacity。FinalGraphResourceLedgerCompiler使用该产品计算原startup batch媒体总量，替代原缺失值按0处理的局部解析。

两名独立静态审查PASS；当前音频实际来自CanonicalInput包路径，视频类型随readiness明确。Release全量重建会话53861完成，configure/build退出0，715项构建计划及必需产物检查通过。realtime CLI 6019072 字节，SHA256 15D217F39EAC74B401A964403A6622C30C405AF7E1D97C18E951D334A79CE550。此步只建立可复用批次产品，尚未改变混合账本、传播GraphEvent或发出source CountBoundResidence；其余实施顺序与完整验收门槛不变。本轮无实流测试及临时产物。

## r224：具名启动事件媒体传播产品

新增MediaStartupEventMediaPlanner，复用r223批次产品，生成每条携媒体事件边的edge/coordinator归属、envelope/release/transaction形式、视频payload类型、逐流媒体上限及独立wrapperSlots。输入envelope为一个媒体；release/transaction取已规划批次上界。clock/activated不传播媒体，输出分支按实际端口严格限制，并验证source identity、已有group选项、端口方向/类型、队列有界、分支完整及preparation/bound_release汇入同一Extractor。

FinalGraphResourceLedger及ReferenceStoragePlan保存该产品，两种reference编译入口均返回它；所选coordinator的媒体链被拆开或边重复归属时失败。不含coordinator的输出分区不强制纳入源链。decoded startup无独立group option，源域完整性仍依赖既有source membership注册验证，不能宣称本planner单独证明全部归属。

两名独立静态审查均明确PASS；Release全量重建会话60920完成，configure/build退出0，716项构建计划及必需产物检查通过。realtime CLI 6031872 字节，SHA256 98C69F3EC309E2BB660FC20F4B0124889973F4F2DC1B182EF1CADBE8AA20B783。未将wrapperSlots乘入原混合maximumBufferObjects，新产品尚未参与source配额，节点内部直接媒体/批量引用与别名计算仍待完成。无新增线程、公共输入或运行时媒体路径。本轮无实流测试及临时产物，完整display容量仍FAIL，原验收门槛不变。
## r225：节点内部媒体持有上界

新增MediaStartupNodeMediaPlanner，消费r224已验证的完整事件产品，按节点生成独立于边队列的媒体上界。Coordinator每路最多一个pending，形成V+1及可选A+1；envelope生产节点保留一个媒体；Binder、Sequencer、Extractor内部共享同批媒体，按V/A计一次。此处distinct仅指原始媒体payload，不包括包装、控制块、数组或派生像素分配；跨节点别名尚未消重。最终账本与两种reference编译入口均保存产品，并校验所选节点完整性和重复归属。

两名未参与实现的独立审查员均明确限定PASS。Release全量重建会话35330完成，configure/build退出0，717项构建计划及必需产物检查通过。realtime CLI 6036992字节，SHA256 0FA394383124EEFE5C099AB9C1E6C601C2966FD0B57795BB840F5A6EA66E7507。source CountBoundResidence及配额接线仍未完成，完整display容量仍FAIL。未执行实流测试，未产生抓包或截图；后续画面仅使用VLC原生快照，不使用系统屏幕截图。原验收标准不变。
## r226：共享保留遍历接通启动事件

MediaGraphPayloadResidencePlanner消费完整账本的storage/startupEvents/startupNodes视图；沿当前可达节点出边传播对应stream/payload，不因同一coordinator而加入无关分支。事件队列按wrapperSlots乘对应媒体上限，节点使用内部distinct上界，替换对应混合entry；Extractor后恢复普通媒体边。音频释放包装虽为Event buffer，实际声明Audio/Packet，未在此断链。InitialOutputResourcePartition已传入完整三类事实。普通节点及共享存储仍使用保守数量，不宣称全图去重。

沿用本计划Clang具名事实传播方法；本轮重新读取官方说明，GStreamer页面请求超时，未据此新增外部行为结论。两名独立审查员均限定PASS（输入为完整已验证ledger）。Release全量重建会话25088完成，configure/build退出0，717项构建计划及必需产物检查通过。realtime CLI 6039040字节，SHA256 016D820726C3DFCD9FC0E8F6DE8579205B9BFAFEDA526683635BBB33397D46AF。无实流测试、抓包或快照产物；source quota尚未接通，完整display容量仍FAIL。

### 下一步已核验的接线条件

先汇集producer、preparedStorage、aggregate fixedStorage及audio FIFO，再编译reference事实、计算source count residence、生成账户及总预算。当前compileLedger即使不执行budget admission仍要求非零总预算，必须拆开纯事实计算和预算完整性校验，禁止用占位预算绕过。

Decoder内部packet必须使用已有preparedInputRetention.maximumInternalPackets()；Raw输入单次完成批次必须使用已有maximumPayloadsPerInputCompletion，当前混合账本无法证明覆盖二者。不得再整块叠加旧prepared数量，其中startup及acquisition已被现有产品覆盖。普通pending/队列、filter缓存、aggregate候选及encoder保留已有保守上界，但不是逐源精确去重。minimumProgressObjects和硬件decoder surface进展证据仍须单独闭合；完成这些条件前不开放display入口，不替换输出encoder原有emission配额。
## r227：源数量配额接线及原规格真实复验

固定存储先编译，随后按r226可达媒体上界和独立codec/atomic owner生成source账户；纯reference不再要求占位预算，正式准入不放宽。删除未实际产出count的cadence/count variant，保留additionalRetainedObjects与独立minimumProgress。packet补opened decoder内部输入；Raw再补单次completion；decoded startup的帧容量不计入packet minimum。输出encoder/aggregate配额不变。两名独立静态审查限定PASS，Release全量重建退出0。

Windows三源H264720p30→HEVC720p30/8Mbps CBR＋AAC192kbps真实120秒及同规格AV重入完成。四次VLC原生快照确认三格→全黑→左下格重入→全黑；CLI caller_stop退出0、排空255ms、错误和pressure failure均0、227286次保留释放配平。完整命令、CPU/内存/漂移、警告、离线验证和25件清理清单见[本轮真实记录](realtime-composition-r227-validation.md)。这些结果不证明内容级AV或静音；非cadence入口、硬件surface进展及RKMPP仍未完成，整体75/FAIL。
