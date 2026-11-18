# 固定实时视频合屏实施计划

## 当前进度：2026-11-18 r501 / rk502

四屏基础生命周期已跑通：最新RKMPP产物，四路RTP H264/H264/H264+AAC/HEVC → H264 VBR 目标8000/最高12000kbps、1280×720/30、AAC192kbps → MPEGTS/RTP。同一CLI两轮完整120秒，四格→全黑→同端口四路重入恢复→再次全黑→caller_stop exit0；workerErrors0、payload回零。VLC原生快照及日志经独立审查确认，详见[rk498](realtime-composition-rk498-validation.md)。这不是完整交付通过。

当前源码已具备2–4源公共入口及独立C合屏API，本次独立审查未确认新的功能源码缺项；历史“尚未实现”只能描述当时状态。下一步按以下最小矩阵补证据，不重开已完成实现：

| 门禁 | 后续工作 |
|---|---|
| Windows共享回归 | r499已完成Release全量重建及同规格四源两轮生命周期，日志正常停止/errors0/pressure0/payload回零；未获取数值退出码，见r499记录；其他矩阵仍待 |
| 双/三源矩阵 | r500双格HEVC CBR发送超期FAIL；r501三格H264 CBR同AU canvas/编码延迟导致TS接纳超期FAIL；rk502同规格RKMPP三格/全黑/所选源恢复/再次全黑/caller_stop0有限PASS，pressure7保留；Windows与RKMPP仍需补左右双格、三源四宫格空格黑，每源完整120秒；分配H264 CBR、HEVC CBR/VBR覆盖 |
| 选定音频 | 保持其他视频在线，单独停音源，验证接收端静音/恢复和内容级A/V，不能仅凭内部drift |
| C API/旧ABI | 已有rk430正向直接C调用、rk431负例、rk432 aarch64布局、rk434启动期回调证据；rk433旧消费者出画但源结束超时FAIL。补最新库回归、Windows x64 ABI和运行期回调stop；生命周期是release，无destroy |
| 失败与可靠性 | 保留rk495/r500/r501不同阶段的期限失败、rk498 pressureFailures=9、异常退出、重复恢复/RSS和NIC出线证据，不用一次未复现关闭 |

当前沿本会话限定范围开发验证和本地提交；未推送、合并或部署。目标机依赖修改仅限有证据的阻塞并保留回退。本轮公共控制修复已完成r499 Windows同规格生命周期回归，完整矩阵仍未闭合。

r503/r504/r505的Nsight cuda+nvvideo、cuda-only、trace=none均在启动线程统计报Thread32Next native0；r506同产物直接启动正常统计/出画/全黑/caller_stop0，限定为启动环境相关诊断，不计完整120秒验收。不能为追踪放宽生产校验，也未获得r501稳态GPU时间线。下一步先推进独立音频失源/恢复及接收端证据；Windows时间线等待可用的非干扰诊断途径，原期限问题保持开放。

## 历史进度：2026-11-16 rk423

RKMPP四路HEVC CBR8Mbps MPEGTS/UDP曾完成出画、全失源及两路重入；记录见[rk422](realtime-composition-rk422-validation.md)和[rk423](realtime-composition-rk423-validation.md)。该证据保留，不用较新H264 VBR链路覆盖其不同输出范围。

## 历史进度：2026-10-28（最近实流 r367）

按代际首AU/解码/过滤/真实发布/aggregate诊断已完成双独立复审；修复缺少MediaNodeId头依赖后，Release63158全量构建成功（757项）。r367原规格三源H.264/AAC → HEVC CBR8000kbps、1280×720/30、AAC CBR192kbps → MPEG-TS/UDP，四张VLC原生快照覆盖三格→全黑→左下重入→全黑且截图前后CLI存活。四次源完整120秒/3600帧exit0，CLI caller_stop0、errors0、payload246709预约/释放匹配归零；16项产物及六PID已清理。详见 [r367记录](realtime-composition-r367-validation.md)。

新证据确认generation2首AU正确经过decoder、transfer、frame-rate、filter及Preparation发布，抵达时其映射区间已落后aggregate cursor，首候选为晚100ms的seq4。原计划要求恢复重新对齐，不要求重入首AU必须展示；不能仅凭发生在本代首个真实贡献前的interval_miss字段判为解码丢帧或新的生产故障，也不以此宣称完整连续性/内容A/V通过。r366发送等待API异常长导致的期限失败仍保留，本轮未复现不等于修复。

当前回到完整存储准入闭环：补齐F/U/G实际backing与early/final共享holder事实，独立规划Cv/Ca与Nv/Na，并以真实消费者贯通SourcePresentation→DecodedPresentationSpan→Prepared；非cadence门禁仍保留，不用cadence测试替代该路径。worker High优先级未消费的确定契约缺口另有 [方案](realtime-worker-priority-contract-plan.md)，尚未实施或证明与发送失败有因果关系。

完整Windows矩阵、内容级A/V与静音、直接C API/旧ABI、周期维护，以及Windows通过后的RKMPP仍未完成。详见 [统一预算计划](realtime-composition-full-storage-admission-plan.md) 与根目录plan.md。当前仅本地开发验证，不推送、合并或部署，不扩展外部FFmpeg修复。

## 历史进度：r312

最新Release三源原120秒H264/AAC→HEVC CBR8000kbps 1280×720/30、AAC CBR192kbps→MPEG-TS/UDP完成三格、全黑、选定AV源120秒重入、再次全黑；仅使用VLC原生快照。四源进程及CLI退出0，CLI为caller_stop，payload261506预约/释放一致且归零；16项产物已清理。详见[本轮记录](realtime-composition-r312-validation.md)。

完整门禁仍未通过：历史超期/崩溃因果未关闭，完整TS存储预算正在实施；非cadence/HEVC重排入口仍为实现缺项。C API已实现但完整调用/旧ABI验证未闭合；内容AV、出口交付、完整Windows矩阵和后续RKMPP仍缺证据。后续以[TS存储契约](realtime-composition-ts-storage-closure.md)推进，不以本轮未复现替代修复。

### 历史进度：r244

RTP 协议预留账本已复用固定容量存储，两个独立审查者修订后限定 PASS；首构建语法错误已修正，Release 全量重建会话81373 exit0。完整网络存储准入及 TS 封装后 AU 上界仍未完成，已完成同规格三源失源/重入及caller-stop复验，四次VLC原生快照已检查、临时产物已清理；完整门禁仍未通过，详见 docs/realtime-composition-r244-validation.md。详见 docs/realtime-composition-r244-rtp-reservation-storage.md。

### 2026-10-16 r239–r243：存储域接线与原规格复验，整体仍未通过

保证准备额度、公共PMR/预算域、网络global账本及数组实际计费已实现；创建异常双重释放修复、固定容量存储及各实施切片已双独立限定审查和Release全量重建。网络全链计费及足额准入尚未证明，串行门控仍保留。

[r243原规格记录](realtime-composition-r243-validation.md)：三路120秒H264源→HEVC720p30 CBR8000kbps/AAC192kbps MPEGTS/RTP，重入源同规格120秒；VLC原生快照确认三格、全黑、左下重入及再次全黑，caller_stop退出0，最终媒体额度231309预约=释放且归零。17件临时产物和测试进程已清理。一次未复现不等于r239串行阻塞已修复，也不替代内容AV/静音、四源异构重排、C API矩阵及Windows后RKMPP门禁。

下一步按[网络存储边界](realtime-composition-r243-network-storage-domain.md)形成planner生产者身份与协议请求几何，覆盖完整存活分配后再评估多reservation；不采用endpoint数推断并发数或经验容量倍数，不新增外部FFmpeg修复范围。

### 2026-10-16 r236–r238：CUDA隔离与关联诊断完成，完整门禁仍75/FAIL

CUDA canvas私有nonblocking stream、只读黑底ready事实、默认stream host完成屏障与RAII已实现；r236约73秒物化超期，r237约21秒scheduler迟交，均完整归档且清理。reservation三谓词与单head缺流等待诊断经双独立审查、Release全量重建；r238运行约319秒无runtime错误，四次120秒源exit0，caller_stop0，payload230047预约=释放且归零。VLC原生快照覆盖三格、首次全黑与重入；播放器暂停来源不明，末次失源黑屏未证实，未记完整通过或根因修复。详见r236/r237/r238验证记录。

独立质量复核维持75/100。C API已经实现（含已批准输入事实和音频目标码率），其完整矩阵及旧ABI回归尚未闭合，不再标作未实现。确定缺口仍为非cadence SourcePresentation入口、四源HEVC输入重排、内容级AV/静音、完整Windows与后续RKMPP。下一实施先做会话级可消费保证准备额度并接实际allocator，区分compressed AU、decoded metadata和runtime像素收费；不得用准备预算除逻辑像素、均分预算或删除门禁。发送期限问题保留，后续原规格验证同时采集同AU等待与播放状态。无外部FFmpeg改动、无推送合并部署。

### 2026-10-16 r235：原规格重入复现sender超期，获得同次CUDA同步证据，整体75/FAIL

复用r233产物，四源均120秒/3600帧exit0，VLC原生快照确认三格→全黑→左下重入。CLI约270秒sender seq168644 reservation失败exit1；激活开始已超completion deadline215827ns，激活至snapshot9.8us。同次cursor8086 canvas134.1904ms，黑底copy117.3218ms、source stream同步91.8757ms；scheduler最长等待在该compose完成前37ms结束，不能认定同一帧因果。完整关联原文已归档，16件临时产物及本轮PID清理。双独立审查明确因果边界；下一步核验CUDA实际stream并评估私有non-blocking adapter，动态tile依赖与失败清理保持，不改外部FFmpeg。详见[r235记录](realtime-composition-r235-validation.md)。

### 2026-10-16 r233：sender边界诊断复验，上游scheduler迟到证据，整体75/FAIL

六文件固定大小边界观测及共享采样helper完成，双独立静态PASS，Release全量重建exit0。原规格三源120秒/3600帧各exit0，VLC原生快照确认三格；CLI约30秒物化前超期exit1。失败batch首次尝试、无WouldBlock，mux出口已超9.780561ms，其后仅21.6us进入物化。既有同audio sequence1049证据显示scheduler缺video等待91.0054ms，结束时已超过dispatch9.294861ms；下一步核对缺头等待策略及缺失video生产链，不用不相关全局峰值归因。sender失败trace未触发，非修复通过。最终payload归零但pressureFailures=1，完整错误保留；12件临时产物和5个本轮PID已清理，源保留。详见[r233实际命令与证据](realtime-composition-r233-validation.md)。
### 2026-10-15～16 r232：重试诊断接入，原规格重入阶段sender超期，整体75/FAIL

两文件新增批次最近/前次尝试及WouldBlock原文有界观测，双独立静态PASS；Release全量构建exit0。三源H264720p30→HEVC720p30/8Mbps CBR＋AAC192kbps，各初始源120秒exit0，VLC原生快照确认三格→全黑。AV重入期间sender sequence92093在reservation创建前已过期，CLI exit1；物化调用开始采样至pacer锁后采样7.667ms，超完成deadline5.395748ms。不能把开始采样称作物化完成，也不能认定为该包pacing等待超时。重入清理exit-1；新增materializer失败trace本轮未触发。最终payload归零，16件临时产物和本轮PID已清理，源保留；会话中断导致的延迟归档已注明。完整命令、结果及双审边界见[r232记录](realtime-composition-r232-validation.md)。下一步关联物化返回、发布、sender出队及snapshot锁前/后时刻，原期限不变。另经完整源码链审查发现High线程优先级未被worker执行端消费，主图与动态branch均存在；仅是独立契约缺口，尚非此次超期根因。非cadence容量与完整验收仍未完成。
### 2026-10-15 r231：r229 Release原规格三源实流失败，整体75/FAIL

三源H264720p30→HEVC720p30/8Mbps CBR＋AAC192kbps已启动，VLC原生快照确认三格；约60秒物化前超deadline13.810672ms，CLI自行exit1，三源均完成120秒exit0。失败batch七次尝试未消费，首次出队至失败51.4869ms；双独立源码调查指向需区分reservation容量/提交唤醒/调度，尚无根因定论。最终payload归零；12件临时产物及本轮PID已清理。未进行失源重入、抓包或离线解码，不能记通过。详见[实际命令与失败记录](realtime-composition-r231-validation.md)。下一步补齐批次尝试/阻塞来源关联证据，保持原deadline；非cadence容量工作仍未完成。

### 2026-10-15 r230：共享容量收费边界核验，整体75/FAIL

双独立只读调查确认：共享预算限制实际总分配但不保证每源额度；生产decoded像素由runtime payload计费，准备PMR仅支付包装/lineage，故不能直接用准备预算除以frame footprint形成数量。固定ring需与媒体容量联合核算；下一步先确定decoded startup收费契约，再接会话必需工作集和共享余量，最后解除入口。详见[共享容量调查](realtime-composition-shared-capacity-design.md)。无生产修改、构建或实流测试，r229运行证据仍待补充。
### 2026-10-15 r229：共享解码输出分配能力产品，整体75/FAIL

实际首帧frames context与decoder回读形成CUDA独立输出证据；动态输出与合屏复用同一adapter，replay核对外部池事实，source planner核对产品身份。补充实际设备类型检查后双独立复审PASS，Release全量重建exit0。本轮未新增实流结论，非cadence容量和入口仍未完成，详见[解码输出证据](realtime-decoder-output-retention-evidence.md)。
### 2026-10-15 r228：CUDA输出隔离及非cadence依赖调查，整体75/FAIL

双独立源码审查限定认可本机native safe CUDA/cuvid公开输出与固定解码表面隔离；核验三份avcodec DLL哈希一致，并明确官方trunk opaque路径与本机差异。剩余入口依赖已定位到SourcePresentation retention缺失、surface预算要求fps、SourcePlanner门禁；下一步形成共享实际输出能力产品及准备容量数量产品，不能仅删除检查。详见[解码输出证据](realtime-decoder-output-retention-evidence.md)。本轮无代码修改或新增实流结论。
### 2026-10-15 r227：源数量配额接通并完成Windows原规格复验，整体75/FAIL

固定存储事实先于预算；source按图保留上界加codec/atomic owner计费，去除输入cadence配额推算，独立检查minimumProgress。双审限定PASS、Release全量重建exit0。三源H264720p30→HEVC720p30/8Mbps CBR＋AAC192kbps：四发送器各120秒，三格/全黑/AV重入/全黑VLC原生快照已核验，CLI caller_stop0、排空255ms、runtime错误及payload pressure failure0。RTP序号连续、TS同步0错误、离线全量解码0；VLC警告保留，25件产物已清除。内容级AV/静音、非cadence输入、硬件进展及RKMPP仍待完成。详见[真实验证记录](realtime-composition-r227-validation.md)。
### 2026-10-15 r226：事件媒体可达性接入共享遍历，整体75/FAIL

共享residence遍历已跨越启动事件并恢复Extractor媒体出口，按事件槽位与节点内部媒体分别计数，替换对应混合entry；InitialOutput调用传完整产品。双独立静态审查限定PASS，Release全量重建exit0。source配额接线仍需分离事实编译与预算验证，并补decoder内部packet及input atomic completion事实；详见[媒体保留容量计划](realtime-startup-payload-retention-plan.md)。无新增实流结论。
### 2026-10-15 r225：节点内部媒体上界，整体75/FAIL

形成Coordinator待处理头、envelope生产节点及Binder/Sequencer/Extractor共享批次的节点级产品，边队列仍单独计数。双独立静态审查限定PASS，Release全量重建exit0；source配额尚未接通，不构成完整容量或实流验收通过。详见[媒体保留容量计划](realtime-startup-payload-retention-plan.md)。后续画面证据仅用VLC原生快照。

### 2026-10-15 r224：启动事件媒体传播产品，整体75/FAIL

按具名端口生成事件内单媒体/批量媒体上限与独立队列槽位，最终账本及两种reference计划均保存；验证分支、类型、身份、组及完整所有权。两名独立静态审查PASS，Release全量重建exit0。产品尚未用于source配额，节点内部保留与别名计算仍待完成，详见[媒体保留容量计划](realtime-startup-payload-retention-plan.md)。无新增实流结论。

### 2026-10-15 r223：启动批次媒体类型与数量产品，整体75/FAIL

新增共享planner产品区分视频Packet/Frame及可选音频Packet上限，账本已消费；容量与runtime同用正int校验，缺失不再按0继续。两名独立静态审查PASS，Release全量重建exit0。GraphEvent传播、混合账本分离及source CountBound接线仍待完成，详见[媒体保留容量计划](realtime-startup-payload-retention-plan.md)。无新增实流结论。

### 2026-10-15 r222：最终媒体保留容量审计，整体75/FAIL

双审确认GraphEvent处可达性断链、source仍全用cadence，且账本混合事件槽位与已计媒体数量，不能统一乘batch。已补具名端口、别名和分阶段规划要求，详见[媒体保留容量计划](realtime-startup-payload-retention-plan.md)。本轮无生产代码修改或新增实流结论；下一步按直接媒体数/包装槽位/每包装媒体上限分离类型化事实，再接入source配额。

### 2026-10-15 r221：prepared源身份寿命计费，整体75/FAIL

身份字符和控制块改由同源资源实际分配，随强/弱引用保留预算；取消原固定字符预收费，避免session结束后下游仍持身份却提前释放费用。两名独立静态审查PASS，Release全量重建exit0。控制/固定存储、GraphEvent批量引用和最终容量合同仍待闭合，无新增实流结论。详情见realtime-rtp-reordered-video-timing.md。

### 2026-10-15 r220：启动单元共享原身份，整体75/FAIL

启动单元复用原lineage的不可变身份，消除逐单元字符串复制。双审发现诊断转换遗漏，首次构建C2446失败；修复后两名复审PASS，Release全量重建exit0。原身份创建、控制/固定费用及最终容量合同仍待闭合，非cadence入口保持关闭，无新增实流结论。详情见realtime-rtp-reordered-video-timing.md。

### 2026-10-15 r219：媒体释放事务对象计费，整体75/FAIL

create/reanchor共用同源分配，transaction对象与control block纳入原资源，保留identity与epoch语义。两名独立静态审查PASS，Release全量重建exit0。control、身份/固定费用和最终对象容量合同仍待闭合；重新确认SourceResources/GraphResourceLedger仍依赖cadence，非cadence入口保持关闭，无新增实流结论。详情见realtime-rtp-reordered-video-timing.md。

### 2026-10-15 r217–r218：清理选择列表与批量释放计费，整体75/FAIL

IDs/Selections和释放事件媒体数组使用同源allocator；purge直接汇集后清理，减少临时复制。ReleaseBuffer对象/control block接入计费，reanchor复制保留原资源并覆盖分配错误。两轮各获两名独立静态审查PASS，分别Release全量重建exit0。transaction本体、身份、GraphEvent引用容量和surface进展仍待闭合；非cadence入口保持关闭，无新增实流结论。详情见realtime-rtp-reordered-video-timing.md。

### 2026-10-15 r216：启动选择快照计费与去除重复数组，整体75/FAIL

原快照使用持有同源资源的allocator，ReadinessIndex直接移动快照，保留选择顺序并消除重复数组复制。两名独立静态审查PASS，Release全量重建exit0。批量释放、身份和容量合同仍未闭合，非cadence入口未开放，无新增实流结论。详情见realtime-rtp-reordered-video-timing.md。

### 2026-10-15 r215：启动媒体引用和待处理队列计费，整体75/FAIL

GenerationState的引用表、去重表、本体/control block及节点待处理队列接入同源资源，插入失败回滚去重登记。两位独立静态审查PASS，Release全量重建exit0。临时结果、身份、批量释放与容量合同仍待闭合，非cadence入口未开放，无新增实流结论。后续画面验收只使用VLC原生快照，不使用系统屏幕截图。详情见realtime-rtp-reordered-video-timing.md。

### 2026-10-15 r214：启动持久索引计费与失败清理，整体75/FAIL

启动list/map及嵌套覆盖索引接入同源计费资源。修复双审发现的低内存失败清理仍分配IDs问题，无需IDs的清理改为无分配；两位复审PASS，最终Release全量重建exit0。临时结果、释放批次、身份及固定对象等仍未闭合，非cadence入口未开放，无新增实流结论。详情见realtime-rtp-reordered-video-timing.md。

### 2026-10-15 r213：滤镜与传输显示点重包装计费，整体75/FAIL

三类源帧节点共用运行时分配能力，Registrar统一按唯一源注入PMR，filter/transfer重包装不再使用通用堆；lineage保持纯时间身份产品。两名独立静态审查PASS。首次构建失败原因未确认，同源码第二次Release全量重建exit0；完整预算与实流门禁仍未闭合。详情见realtime-rtp-reordered-video-timing.md。

### 2026-10-15 r212：解码显示点包装计费，整体75/FAIL

Registrar按唯一源归属注入原session PMR，SourceAccessUnit解码器要求资源严格对应；显示点lineage及包装对象进入共享预算，分配失败显式返回。两名独立静态审查PASS，Release全量重建exit0。filter/transfer重包装、startup容器与批量事件等仍未闭合，非cadence入口未开放，无新增实流结论。详情见realtime-rtp-reordered-video-timing.md。

### 2026-10-14 r211：解码启动envelope计费接线，整体75/FAIL

复用原源session的计费PMR，经唯一成员归属注入decoded startup，envelope对象/control block纳入共享预算并随最后引用释放；缺失资源与分配失败明确报错。两名独立静态审查PASS，Release全量重建exit0。decoder/display lineage、identity字符串、store与批量release费用未闭合，非cadence入口继续关闭，无新增实流结论。详情见realtime-rtp-reordered-video-timing.md。

### 2026-10-14 r210：共用引用驻留规划，整体75/FAIL

抽取既有可达引用上界算法，支持共享retentionNodes的单条目计数，原配额决策保留原模块。两名独立静态审查PASS，Release全量重建exit0。GraphEvent批量持有的具名端口传播、准备预算与surface进展仍待闭合；非cadence入口未开放，无新增实流结论。详情见realtime-rtp-reordered-video-timing.md。

### 2026-10-14 r209：显示点容量消费去除源帧率，整体75/FAIL

显示点aggregate直接消费有界候选计数，canonical维持原区间规划；DecodedVideoStartup纳入引用账本。两名独立静态审查PASS，Release全量重建exit0，产物哈希已记录。尚需闭合共享retentionNodes归属、GraphEvent原payload保留及硬件surface进展证据；当前未开放非cadence入口，无新增实流结论。画面验收仅使用VLC原生快照。详情见realtime-rtp-reordered-video-timing.md。

### 2026-10-14 r208：显示点聚合与提交后EOF接线，整体75/FAIL

实际display候选接收、latest<=target选帧、成功commit证据及EOF排空已接入；held计入原有界队列。修复双审发现的EOF待提交时purge遗留结束标记问题，两者复审PASS，最终Release全量重建exit0。输入帧率容量假设与完整资源预算尚未去除，生产入口未开放，无新增实流结论。详情见realtime-rtp-reordered-video-timing.md。

### 2026-10-14 r207：显示点结束协调合同，整体75/FAIL

明确区分canonical输入end与display实际提交outputEnd，普通区间接口拒绝显示点，双方输出共享finalOutputEnd。两名独立静态PASS；首次构建中断且产物缺失，重新全量Release构建exit0。显示点消费/commit/EOF调用尚未接通，资源预算和实流仍未闭合。详情见realtime-rtp-reordered-video-timing.md。

### 2026-10-14 r206：显示点来源记录与EOF设计核验，整体75/FAIL

新增不带duration的display contribution及校验、metadata计费，双独立静态PASS，Release全量重建exit0。FFmpeg TS_DEFAULT用于点选帧依据，GStreamer用于hold/EOS状态对照；末点须按成功commit消费，不能以末PTS替代输入结束时刻。aggregate运行时与预算仍未闭合，无新增实流结论。详情见realtime-rtp-reordered-video-timing.md。

### 2026-10-14 r205：外层接线及原子出边修复，整体75/FAIL

InputGraph返回互斥packet/frame，composition显示点分支仅建帧处理段，补代际child及源ownership/cardinality。双审发现release首边非atomic，显式首边策略及实际图校验修复后两者复审PASS；Release全量重建exit0。生产入口仍待资源与aggregate合同闭合后开放，无新增实流结论。详情见realtime-rtp-reordered-video-timing.md。

### 2026-10-14 r204：输入段前置decoder接线，整体75/FAIL

同步输入段可按typed decoded选项连接gate→既有decoder→decoded startup并释放RawFrame；两名独立静态审查及最终复审PASS，Release全量重建exit0。外层composition接入、代际/ownership/cardinality及资源预算仍未完成，没有新增实流结论。详见realtime-rtp-reordered-video-timing.md。

### 2026-10-14 r203：显示点时钟装配及协议校验，整体75/FAIL

RTP source contribution按typed readiness规划decoded assembly，不再要求包时长；协议时钟、源身份与startup事实交叉核对。两名独立静态审查PASS，Release全量重建exit0。生产builder与资源/aggregate接线未完成，无新增实流通过结论；详情见realtime-rtp-reordered-video-timing.md。

### 2026-10-14 r202：解码显示点启动节点双审与构建通过，整体75/FAIL

真实decoded frame启动包装及source group注入已实现，两名独立静态审查PASS，Release全量重建exit0。尚未生成生产拓扑，不增加实流通过结论；下一步接入前置唯一decoder与RawFrame释放，补资源硬预算及显示点aggregate。详见realtime-rtp-reordered-video-timing.md的r202记录。只使用VLC原生快照。

### 2026-10-14 r201：输入装配分离区间与显示点，整体保持75/FAIL

视频assembly显式区分canonical timing与decoded marker，显示点不携带伪造duration/decodeOrder。现有canonical消费者先检查variant；RTP binder显示点分支使用协议clockRate，并要求protocol/startup/assembly源身份一致，不配置terminal duration。双独立源码复审PASS，Release全量重建成功。生产decoded入口仍关闭，下一步接前置唯一decoder、decoded envelope、缓存/预算及aggregate；不新增实流通过结论。


### 2026-10-14 r200：显示点帧段可省略FPS，整体保持75/FAIL

源frame builder改用typed timing，cadence保留FPS，presentation按transfer/可选filter/preparation直接串接。source代际children及shape FPS数量同步按startup readiness形成；两名独立源码审查PASS，Release全量重建成功。旧组合入口与生产planner仍拒绝未完成的presentation链路，不新增实流通过结论。下一项为前置唯一decoder的typed input assembly、decoded startup envelope/缓存预算与aggregate显示点消费，详见realtime-rtp-reordered-video-timing.md。


### 2026-10-14 r198/r199：准备边界生产迁移与真实复验，整体保持75/FAIL

独立VideoPreparation已接入同步source最终发布边界，旧FPS/filter准备逻辑已删除，注册/shape/基础驻留计费同步迁移。r198实流暴露代际children遗漏，修复并双独立复审、Release全量重建后，r199三源H264720p30→HEVC720p30/8Mbps CBR＋AAC192kbps及120秒重入获有限回归PASS。五张VLC原生快照包括CLI运行中的两次失源全黑，最终归零、排空264ms、RTP连续、完整AV解码成功。失败记录及告警保留，临时产物已清理。

未完成：SourcePresentation去FPS及前置唯一decoder/startup/aggregate、固定元数据硬预算、内容AV、既有deadline与RKMPP。命令和证据见realtime-composition-output-drain.md的r198/r199。


### 2026-10-14 r197：准备节点运行时依赖接线，整体保持75/FAIL

专用factory和registrar已将独立VideoPreparation绑定到原输入源同步组，显式验证准备状态身份与frame product，并注册代际清理。三文件源码双独立复审PASS；Release全量重建退出0。生产builder、旧FPS/filter准备删除、shape及资源计费迁移仍待完成，未新增实流验收结论。详见realtime-rtp-reordered-video-timing.md的r197记录。


### 2026-10-14 r196：通用准备发布节点基础，整体仍75/FAIL

为无FPS/无filter路径实现VideoPreparationNode，复用owner-thread purge、原输出reservation与source发布许可，无新pixels分配。双审修复正常撤销误报fatal和Abort控制遗漏，最终双静态PASS、冻结Release全量重建成功。生产仍由旧FPS/filter承担准备；factory/builder/validator/ledger迁移未完成，新节点未启用，无本轮媒体验收。详见realtime-rtp-reordered-video-timing.md。

### 2026-10-14 r195：filter显示点接线，整体仍75/FAIL

滤镜执行合同显式区分canonical和display；显示身份贯穿pending/opaque/output/代际与preparation，保留sink真实时基，未知帧率不填encoder FPS。双独立静态审查PASS，冻结Release全量重建成功。尚无本轮媒体验收；source帧段固定FPS、decoded startup和资源/aggregate完整接线仍待办，不开放生产SourcePresentation。细节见realtime-rtp-reordered-video-timing.md。

### 2026-10-14 r194：显示身份与硬件下载保留，整体仍75/FAIL

独立显示产品保存原AU和实际解码显示点，注册表支持第三种产品，硬件下载保留原显示身份；双独立源码PASS、冻结Release全量重建成功。本轮未做媒体测试。filter时基/产品与准备存储等完整SourcePresentation接线继续待办，详见realtime-rtp-reordered-video-timing.md。

### 2026-10-14 r190–r193：启动显示点合同及source分段，整体保持75/FAIL

完成typed startup跨度、Running顺序和真实帧证据校验、Source-AU代际门、解码/帧处理配置与builder分段。双独立静态审查及冻结Release全量构建通过。r193三路H264720p30→HEVC720p30/8Mbps CBR＋AAC192kbps及120秒重入获双独立有限回归PASS：caller_stop0、drain265ms、RTP171670连续、两流DTS递增、完整AV解码0。六张VLC原生快照覆盖三格更新、首次失源黑屏与左下重入；末黑图在CLI停止后取得，不宣称第二次失源停前黑图。pressure47546、VLC迟到/converter告警保留。详细命令指标见realtime-composition-output-drain.md。

SourcePresentation生产仍关闭。下一步将typed assembly接到前置唯一decoder和启动缓存，完成filter/aggregate显示血缘及准备资源计费，再验收HEVC重排。当前结果不关闭内容AV、旧deadline或RKMPP门禁。


### 2026-10-14 r189：冻结解码显示点投影，整体保持75/FAIL

AU保存原SR校准锚、斜率和共同epoch，probe/runtime对实际decoded PTS共用投影，不补源duration。修复大tick先浮点相减及绝对NTP时间相消；整数差溢出显式失败，不宣称全int64范围纳秒精确。冻结源码双独立PASS，最终Release全量成功。

共享canonical三源H264720p30→HEVC720p30/8Mbps CBR＋AAC192kbps及120秒选源重入完成：四次源exit0、CallerStop0、drain258ms、runtime归零；172578 RTP连续，两流DTS递增、完整AV解码成功。五张VLC原生快照确认三格更新/黑屏/左下重入/末黑。pressureFailures47572、VLC10迟到和5converter告警保留；实流证据双独立有限回归PASS；29件本轮临时产物已清理，7个媒体PID均不存在。

下一项仍是同一decoder前移到startup，真实显示点跨度满足原正preroll、音频真实sample coverage与共同激活、aggregate持有/失效清理及surface重叠预算。production SourcePresentation仍拒绝；不能以本次canonical回归替代HEVC重排、完整资源门禁、内容AV、旧deadline及RKMPP验收。


### 2026-10-13 r187/r188：源时序与连续准备接线，整体保持75/FAIL

r187已完成同一probe的Prepared身份路径实现、全量Release与现有canonical三源有限回归；四张VLC原生快照、全AV解码及退出结果已双审归档，26件临时产物清理。新身份目标尚未生产启用。

r188将cadence/presentation事实贯穿source facts和候选planner，区分普通映射帧与重排见证；准备filter允许明确省略未知frame_rate。连续准备admission与runtime retention分离，FIFO及包装对象按实际分配收费、带owner移交。冻结源码双独立审查通过，全量Release构建成功；三源及重入真实链路、五张VLC原生快照、175575包RTP连续性、两流DTS及完整AV解码获双独立有限回归PASS。CLI为CallerStop0，排空255ms；pressureFailures47557及VLC迟到、converter告警仍保留。

下一完整垂直接线仍为同一generation gate→decoder→显示点startup→filter→aggregate，并保留音频共同激活、purge和实际存储边界。当前typed runtime gate明确拒绝未规划的presentation合同；不能用已有三源回归替代HEVC重排。完整资源准入、内容AV、旧deadline根因与RKMPP仍开放；Windows完成后才验证RKMPP。


### 2026-10-13 r185/r186：准备引用与身份记录分配推进，保持75/FAIL

快照引用改实际PMR准入；registry统一显式Runtime/Prepared资源，State/map/lease/opaque owner按owner生命周期分配和退休。各切片双独立源码PASS，最终Release全量成功。三源H264 720p30→HEVC 720p30/8Mbps CBR＋AAC192kbps MPEGTS/RTP及选源120秒重入，四次源exit0、CLI CallerStop0、drain261ms、runtime归零；174884 RTP连续，两流DTS递增、完整AV解码成功，双独立有限回归PASS。VLC原生5快照确认三格/全黑/左下重入/全黑；25迟到、converter及色彩警告保留。

Prepared registry尚未接入身份probe；完整准备存储、未知cadence接线、四源HEVC重排、内容AV、旧发送deadline及RKMPP仍未完成。下一步直接接入同一probe的typed身份与有界证据，再接准备/运行时合同，不能继续把三源回归当完整合屏完成。27件临时产物记录后清理；保留指定源和未提交改动。

### 2026-10-13 r184：分阶段诊断与VLC原生快照有限回归PASS，保持75/FAIL

已补有界mux出队/首次pop/attempt/发布观测，正常WouldBlock不打印失败。双源码PASS、最终Release全量成功；原规格三源120秒及AV重入120秒，CallerStop exit0、排空254ms、runtime归零，线端174953 RTP连续、DTS递增、完整AV解码成功，双独立有限回归PASS。五张VLC原生实时快照覆盖三格更新、全失源黑、左下重入与末黑；28件临时产物和7个媒体PID已清理复核，指定源保留。VLC2迟到及颜色警告仍存在。

本轮没有复现r183 deadline失败，诊断失败分支仍无真实样本，不判根因已修复。后续保持原规格取得可关联失败证据，禁止放宽期限；并继续完整准备存储准入、source身份/四源HEVC、内容AV，Windows通过后才RKMPP。完整合屏仍未完成。

## r184当前工作：发送期限根因诊断

在r183真实失败后补同running-time的mux出队、materializer首次取批次/本次尝试及前次发布观察；正常WouldBlock不打印失败。双独立源码PASS，最终Release全量构建进行中，随后保持原规格复验；期限根因尚未闭合。仅VLC原生视频快照。


### r183最终复验结果（2026-10-13）

最终Release成功、双独立源码PASS。相同2GiB额度已建图并输出，共享预约graph_bound高水位881967494B；真实链路于09:28:18.086发生datagram materialization超deadline4.891855ms，CLI exit1，完整验收FAIL。原三源和AV重入均120秒exit0；runtime资源归零仅为失败清理，AAC尾帧未排空。155283输出RTP连续、DTS递增、已捕获片段完整解码成功不能覆盖尾部。双独立运行审查FAIL，保持75/FAIL。用户要求后仅采用VLC原生视频快照；本次实际取得的是录制回放快照，不冒充实时图。32件临时产物及9个相关进程已复核无残留；指定源124427809B保留。后续沿绝对发送期限首次失败调用链调查，不能放宽期限；完整准备硬界、四源HEVC、内容AV和RKMPP仍待完成。详见realtime-composition-output-drain.md。

## 最新进展 r182/r183：共享预约已接线，真实预检暴露重复收费

r182共享根、CLI/C显式部署额度、五入口及owner寿命修复经双独立源码PASS、最终Release成功。原规格三源在2GiB额度下预检exit1，尚无输出；16件临时产物及精确媒体PID已清理。调用链双审确认completed-AU probe重复预约raw协议owner。r183以内部输入产品区分去除重复费用，保留配置解析、decoder及frame，经双独立源码PASS，最终构建与同规格复验进行中。完整准备准入、source身份、四源HEVC、内容AV及RKMPP仍未完成，整体75/FAIL。

## 最新进展 r181：AU实际持有费用结算，原链路有限回归

完成3文件：最大AU预留在发布前按fresh AVBufferRef.size结算，补公开头费用；拒收packet先销毁后退费。修正后双源码PASS，最终Release708项exit0。原三源120秒＋AV重入120秒，CLI caller_stop0、drain259ms、runtime资源归零；输出172458个RTP连续、DTS递增、完整AV解码成功。VLC6条迟到与颜色警告仍开放，初始截图有DPI截边，不计完整画布几何证明。

共享根尚未接线；独立审计确认还需修正Session工作集、snapshot引用和clock/binder交接的费用寿命顺序。完整2048MiB共享准入、source身份probe、四源HEVC、内容AV及RKMPP仍未完成，整体75/FAIL。

## 最新进展 r180：探测作用域纠正，完整验收证据未闭合

七文件纠正已获双独立源码PASS，最终Release全量708项exit0。三源及重入看到三格、失源黑屏与恢复；现存抓包222301包序号连续、两流DTS递增、完整片段解码成功。执行会话中断，缺CLI退出码、最终drain和资源归零，完整验收仍FAIL；详见realtime-composition-output-drain.md。29件本轮产物及9个媒体PID复核无残留，指定120秒源保留。

用户委托选择准备存储额度，本机取全会话共享2048 MiB（2147483648字节），仅作为显式可配置验收部署值，不是产品默认值或RSS/GPU上限。共享预算尚未接线；下一步闭合所有准备owner的准入与退休，再推进source身份probe、四源HEVC和Windows完整门禁。整体75/FAIL，RKMPP未启动。


## 最新进展 r179：wire阶段实验暴露作用域错误，验收FAIL

7文件将累计计费延长至continuous freeze，双实现审查PASS、Release708项exit0；原规格三源实际约5秒CUDA源0在4999975/5000000字节后超限，CLI exit1，未进入DAG。独立合同复审FAIL：probeSize原约束流信息探测，不足以作为整段decoder/clock/canvas准备累计流量合同；不能把计数正确当方案正确。该修改仍为未验收WIP，禁止宣称修复或交付。下一步纠正typed准备阶段与独立容量语义，保留原规格、首错和拒绝外部FFmpeg范围；不增大验收probeSize、不重置账本掩盖。14件临时产物和6PID已清理，整体75/FAIL，Windows尚未通过，RKMPP未启动。


## 最新进展 r178：探测与生产decoder隔离，准备owner跨等待

探测首帧使用后销毁旧context，通过同一builder和设备重开生产decoder，复核线程/timebase/propagation/retention及重新计算的准备费用；不再依赖私有FIFO flush。审查发现NeedMoreEvidence等待前lease提前释放，已改为mandatory decoder+lease owner跨wait和probe移动同一准入。双源码PASS、最终Release708项exit0；三源120秒＋重入120秒确认三次真实重开、画面更新/黑屏/恢复、drain254ms、runtime资源归零、wire/DTS/完整AV解码有限双审PASS。NeedMoreEvidence仅源码覆盖、11条VLC迟到与局部画质问题仍开放；25件产物清理。整体75/FAIL；下一步将continuous准备接入原AV共享wire累计准入，再闭合源AU身份probe、前置decoder与帧startup/显示策略，原规格四源验证后再RKMPP。

## 最新进展 r177：显式源呈现点与AU身份，生产接线仍未完成

共享RTP session支持CanonicalInterval/SourcePresentation；后者保留AU身份与映射PTS、未知DTS/duration、立即发布，准备/runtime核对同一产品。审查发现wrapper信用归属遗漏，修正后双源码PASS及最终Release706项exit0。原规格三源120秒＋选源重入120秒，drain260ms、资源归零、wire连续/DTS/AV解码有限双审PASS；首轮现场三格缺证据，VLC13迟到仍在，25件产物清理。现planner仍canonical，整体75/FAIL；下一步闭合独立于错误fps的准备资源合同、probe身份/生产重放隔离、前置decode与frame startup/显示策略，随后原规格四源验证。

## 最新进展 r176：共享decoder源产品支持与旧路径回归

源packet/frame wrapper、typed产品与COPY_OPAQUE校验接入共享VideoDecodeNode，保持现生产builder为canonical。时间基遗漏经复审修复，两独立源码PASS，最终Release706项exit0。三源120秒＋选源重入120秒，VLC三格/黑屏/恢复、drain254ms、资源归零、RTP连续/DTS递增/完整AV解码有限双审PASS；11条VLC迟到及颜色警告仍在，24件产物清理。HEVC乱序新路径尚未接线，整体75/FAIL；继续完成显式源时间产品、AU身份、显示策略、startup及资源合同。

## 最新进展 r174/r175：重排身份合同拆分，旧路径有限回归通过

r174真实HEVC trace否定固定duration捷径：时钟比1/30但两个fixed_pic_rate标志均0。r175将可未知时长/DTS的源AU身份与完整canonical lineage拆为不同类型，复用有界registry/token/purge，现decode/filter/encode显式请求canonical。8文件切片双源码PASS、全量Release成功；原三源120秒＋选源重入120秒、正常drain256ms、payload/队列归零、wire序号/DTS/完整AV解码有限双审PASS。早期缺格、VLC12告警仍记录；25件产物清理。新AU路径尚未接线，四源重排FAIL、内容AV、旧deadline和RKMPP未关闭，整体75/FAIL。下一步补齐显示点/区间、尾帧、prepared probe与前置decode资源合同。详见[时序设计](realtime-rtp-reordered-video-timing.md)与[实际回归](realtime-composition-output-drain.md)。

## 最新进展 r173：四源异构暴露输入重排时序合同缺口

三路H264＋一路HEVC（同120秒源、720p30、12Mbps CBR）建图失败：CUDA source2 binder拒绝RTP时间戳回退，最终QSV retention错误只是后续候选。线端已证实AU呈现时间回退；共享binder以传输顺序正PTS差赋duration并伪造DTS，canonical/startup在解码前要求正时长，frame-rate observer也假定相邻时间戳正增。不能关闭B帧、取abs或硬填帧率绕过。四源已中止、16件产物与7PID清理，整体75/FAIL。下一步按RFC7798及成熟解码顺序/显示时序模型重整共享输入合同，先Windows再RKMPP。详见[时序调查](realtime-rtp-reordered-video-timing.md)。

## 最新进展 r170/r171：失败时序诊断已实施，间歇deadline未闭环

物化失败三时间/条件、发送失败同包预约与检查时序已补齐，分别双独立审查PASS、全量Release成功。r170同规格单路在发送前超期，剩1包；r171完整120秒未复现deadline，失源后既有ProgressTimeout退出1，不能判修复或完整验收。两轮日志关键行与命令已归档、临时产物和PID清理。下一步原规格合屏与共享链路对照，获取失败时间样本；不得放宽期限或只因未复现提升评分。整体75/FAIL，Windows先于RKMPP。详见[记录](realtime-composition-output-drain.md)。

## 最新进展 r169：共享单路复现运行期 deadline，回归 FAIL

r168命令fmtp引号导致建图前失败，已修正命令并清理。r169单路H264/AAC RTP→HEVC 1280×720/30fps/8Mbps CBR＋AAC192kbps MPEGTS/RTP运行约38秒时materializer deadline失败，CLI1，源随后中止；payload归零但sender剩17包。无合屏canvas，证实同类错误在共享输出链也可发生，不能仅归因CUDA合屏。独立审查确认覆盖ExternalCorrection；需补失败包精确时序诊断再定位，不放宽期限。12项r169产物清理、进程无残留。整体75/FAIL，暂停增加验收矩阵，优先共享deadline根因。详见[证据](realtime-composition-output-drain.md)。

## 最新进展 r167：完整重入与排空线端有限 PASS

r166量子执行合同双审及全量Release完成。r167原规格三路120秒H264 RTP→固定合屏HEVC 1280×720/30fps/8Mbps CBR＋AAC192kbps→MPEGTS/RTP，选定AV源再重入120秒，四源退出0、CLI正常停止0；drain262ms、pending/backlog/payload归零。171941 RTP与sender一致、无序号缺口，视频7853/音频11274 DTS无回退，全量解码0。VLC三格→黑场→左下重入可见，但20条迟到丢弃告警保留。两独立复审有限PASS；25件临时产物清理、7个PID无残留。原deadline、内容AV、旧realtime AV共享链路回归、完整矩阵及RKMPP仍待，整体75/FAIL。详见[完整记录](realtime-composition-output-drain.md)。

## 最新进展 r165：pending诊断修正，重入暴露resampler块容量合同缺口

单行pending清零修复双审PASS、Release全量0。实流原三源120秒正常，但所选源重入generation2时aggregate音频候选样本超界，CLI1，不能判排空/重入通过。18件临时产物清理、7个PID无残留。两独立审查定位planner稳态单块上限与执行补偿窗口不等价，未发现候选计数漏减；待补精确数值并统一规划/执行量子，不能增经验容量。完整尾部wire仍未取得，原deadline、内容AV及矩阵不缩减，75/FAIL保持。详见[失败与合同分析](realtime-composition-output-drain.md)。

## 最新进展 r164：关联诊断与运行时排空有限 PASS

同compose阶段及同AU缺流等待诊断已完成，双审PASS、冻结后全量Release0。原规格三源120秒、选源完整重入120秒均完成，CLI正常CallerStop退出0，drain254ms、sender零backlog、AAC无残留、payload全部回收；VLC三格→黑场→左下重入可见。捕获区间RTP序号/DTS/全量解码通过，但抓包370秒先于最终停止，尾部wire未覆盖。最慢compose69.67ms中tile0 source_wait63.239ms，尚未复现r163的136ms失败，不能声称deadline修复。26个临时产物已清理。mux188字节诊断疑似陈旧快照待审查修正。整体75/FAIL、Windows后RKMPP顺序保持。详见[排空及诊断记录](realtime-composition-output-drain.md)。

## 最新进展 r163：终端代际源码双审通过，实流因运行期 deadline 失败

r162 全量 Release 成功，但正常停止实流因 EOF 缺少精确输出 generation 失败，CLI 退出 1，sender 剩余 1 包/228 字节。已记录结果并清理 19 个临时产物。r163 两 aggregate 使用已激活输出 epoch；scheduler 在权威激活且媒体排空后才合成关闭终端，恢复期不猜 generation。两名独立审查者明确源码 PASS，最终全量重建退出 0；原规格实流约 85 秒发生 materializer deadline 失败，未进入排空及重入验证。VLC 截图为退出后的保留画面，不作为持续播放通过；18 个本轮临时产物已清理，6 个 PID 已退出。整体维持 75/FAIL；Windows 通过前不进入 RKMPP。详见 [排空记录](realtime-composition-output-drain.md)。

## 最新进展 r161：显式停止缺失EOF已定位，排空设计双审通过

最新源码确认CallerStop直接停止worker/关闭channel，未走编码器EOF drain；不是AAC析构或外部FFmpeg依赖问题。复用官方EOS与仓库动态branch drain合同；两独立审查发现退出aggregate与源purge冲突，修订为EOF提交并关闭全部媒体/activation输出后保留ControlOnly服务，待其余输出节点finished且exited再整体stop。修订设计双PASS，尚未修改生产或运行新实流。详见[排空设计与调用链](realtime-composition-output-drain.md)。下一项实施和Windows原规格排空验收；全部原门禁与75/FAIL保持。

## 最新进展 r160：双owner隔离与所选源重入有限PASS

视频canvas与音频aggregate已拆到独立DAG节点，协调准备/activation、双purge、资源账户和pending求和已接线。头依赖、停止协调状态撤销和音频ring连续view误用均已修复，两独立源码复审PASS；最终全量Release configure/build0（session96110）。原三源及所选AV重入四次完整120秒均exit0；CLI caller_stop0、payload184686/184686归零。canvas最大100.767ms而音频built最大26.6717ms，聚合隔离有限双审PASS；scheduler仍90.6241ms、尾1包/AAC2帧未drain。wire176512RTP序号/DTS/AV解码正常，现场三格→黑→所选格恢复。完整门禁未闭合。详见[执行隔离实施记录](realtime-composition-execution-isolation.md)。保留全部既有验收门禁，整体75/FAIL。

## 最新进展 r159：共享时间换算已抽取，执行隔离仍待接线

新增MediaAvAggregateTimeline纯换算helper，移走原节点videoTime/audioTime/presentationMaster并更新Node/Video/Audio全部调用，保留原算法、错误文本和溢出检查；不引入线程/状态/时序策略。五文件源码冻结后两名独立审查均明确等价PASS。Release全量clean-first/all-target实际成功（session87812，configure/build均0），UTF8无BOM/CRLF与diffcheck通过；本轮未运行实流，不宣称期限修复或媒体验收。下一步按[执行隔离设计](realtime-composition-execution-isolation.md)拆分两owner、共同准备/activation、双purge、runtime注册及账本。当前仍为单owner，完整目标75/FAIL，原r157发送超期、尾部排空、内容AV及跨平台门禁仍开放。

## 最新进展 r158：音视频执行隔离设计收敛，待实施

源码确认共同aggregate的同步canvas、单pending及基类pending transfer会直接耦合音视频推进。官方CUDA/GStreamer资料与两独立审查否决仅异步API/仅新增canvas节点作为完整隔离方案；选择独立音频/视频DAG owner，复用唯一activation和现有账户。已细化准备generation、activation控制事件发布、双purge独立静音、EOF seal及两owner存储合同；主审纠正正常发布锁序为permit先于channel。详见[执行隔离设计](realtime-composition-execution-isolation.md)。本轮仅设计与源码调查，无生产修改/构建/实流；不是问题修复或验收PASS，整体75/FAIL。下一步实施两owner及完整planner/builder/registration/账本接线，冻结双审后原规格Windows复验。

## 最新进展 r157：现场三格有限PASS，原规格发送期限FAIL

桌面恢复后两时刻VLC无遮挡三格+右下黑格，内容更新，主智能体与双独立审查均确认有限现场PASS。CLI在源活动时自然exit1：scheduled datagram submit exceeded its original deadline；三源各完整120秒/3600帧exit0。112.8455ms音频built间隔包含canvas111.5223ms及其source_wait65.5392ms，但缺失败datagram精确关联，不能断定唯一根因。payload60887/60887归零，sender仍5包/6404B、AAC2帧，运行FAIL/整体75。18件本轮产物已记录后清理、六PID无残留。未改生产代码；继续定位期限与同步等待，完整矩阵/内容AV/排空/Windows后RKMPP仍开放。详见[C入口与CLI记录](realtime-video-composition-c-api-proposal.md)。

## 最新进展 r156：CUDA阶段诊断落地，目标stream等待关联有限PASS

固定容量五阶段诊断复用线程CPU/steady时钟，保持原CUDA调用与同步；双独立源码PASS、Release全量0。原三源120秒/3600帧exit0、CLI caller_stop0；payload118514/118514归零，wire92531RTP序号/DTS/AV解码通过，双原始日志复核有限PASS。最大canvas70.1879ms内包含destination_wait41.7732ms；source_wait36.6752ms属于另一调用不得相加，CPU0仅量化观测。未复现r155超期，不关闭根因；尾部drain、内容AV、现场/完整矩阵/RKMPP仍未完成，整体75/FAIL。19件产物清理，详见[C入口与CLI记录](realtime-video-composition-c-api-proposal.md)。

## 最新进展 r154/r155：首错保留修复，真实超期对应canvas停顿

两文件修复完成双源码PASS，Release全量0；r154原Nsight场景最终保留IoFailure: Thread32Next failed，双审有限PASS，不再被停止期Cancelled覆盖。API失败根因待查，stdout截断不称资源归零。r155无分析器原规格三源仍FAIL：CLI1、三源完整120秒0；144.9279ms音频生成间隔完整包含同cursor canvas144.6856ms，随后同失败AU到达调度器晚27.719075ms，发送窗口耗尽。双独立定位PASS，不能直接归因GPU；原worker错误完整保留，payload54453/54453归零但非drain。现场截图失败，完整矩阵/RKMPP仍开放。两轮产物均清理，整体75/FAIL；下一步分解canvas内部同步/拷贝等待诊断，详见[C入口与CLI记录](realtime-video-composition-c-api-proposal.md)。

## 最新进展 r153：官方CUDA诊断启动失败，先修首错保留

原规格Nsight诊断未进入稳定输出；CPU采样/线程切换因无管理员令牌关闭，CUDA仅启动数据。stop.begin先于Cancelled；完成流程stop后worker失败优先于原waitStatus，有遮蔽首错路径，双独立确认。原始启动失败尚未知，不归因GPU或stdin EOF。下一步保留原始错误后复验；不放宽时限、不改外部FFmpeg。三源120秒自然0、profiler1，CLI退出码未独立取得。15件顶层产物及2件临时lock与空目录清理，完整目标75/FAIL。详见[C入口与CLI记录](realtime-video-composition-c-api-proposal.md)。
## 最新进展 r151/r152：旧入口有限回归，CLI线端与回收有限双审PASS

r151历史头消费者成功通过旧单视频入口运行；源完整120秒后触发既有无进度超时，调用方主动停止断言不匹配而exit7，原失败保留。READY/退役/完成与SDP回收事实正常，输出RTP序号/DTS/解码正常；不判完整ABI或主动停止通过。17件产物及空目录已清理。

r152沿用r148 Release完成三源realtime CLI原规格复验：三源120秒/3600帧exit0，CLI caller_stop0；83797RTP序号连续，4072视频/5845音频DTS及完整解码正常，离线三格确认，payload回收归零，双独立有限PASS。现场截图因Screen-saver失败，尾部sender/mux/AAC仍有残留。104.0526ms音频生成间隔内定位canvas调用86.2322ms墙钟耗时，尚不能区分GPU执行或线程抢占；旧期限问题未关闭。20件本轮产物已清理，无进程残留。详细命令、指标与审查边界见[C入口记录](realtime-video-composition-c-api-proposal.md)。完整目标仍75/FAIL，先Windows后RKMPP。

## 最新进展 r149/r150：Waiting回调停止/异常辅助门禁有限PASS

沿用r148 Release、无生产修改。r149因分次启动错位，两次预检超时/trigger0/exit7，失败已保留。r150改一次宿主直接启动三路原规格源，stop与throw Waiting目标各触发一次、caller各exit0；stop保持CANCELLED/STOP、一次退役完成并删除SDP；throw禁用后续回调、FAILED/STARTUP_FAILURE，输出列表0、releaseNULL，SDP释放后不存在（不冒称直接证明释放前删除）。两者无runtime启动日志、progress0，双独立有限PASS。三源各完整120秒/3600帧自然exit0；每轮15件临时文件/空目录清理，无进程残留。

此为启动前异常辅助验证，没有媒体输出，不替代r148正常链路。仍需已有运行首错后回调异常、READY后活动取消、旧ABI/固定AV/动态/realtime CLI回归、旧间歇deadline与完整媒体/跨平台矩阵。整体75/FAIL；详细命令和指标见[C入口记录](realtime-video-composition-c-api-proposal.md)。

## 最新进展 r148：C入口输出生命周期真实验证有限PASS

固定AV/合屏初始输出注册、READY及Retired已接入共享发布和发送证据；七文件双独立源码PASS、Release全量exit0。真实C调用方配置释放后三源各完整120秒/3600帧，输出HEVC1280×720/30 CBR8Mbps、AAC目标192kbps→MPEG-TS/RTP；READY/Retired/COMPLETED各一次，ID1、终态集合0、COMPLETED前SDP删除、releaseNULL，caller_stop exit0。双独立运行复核有限PASS。86537 RTP序号连续，DTS与全AV解码正常；现场DPI裁切图为局部三格，完整现场截图为失源黑场，完整三格另有wire抽帧。24件产物及空目录已清理，无媒体进程残留。

旧间歇deadline、AAC尾帧/mux/sender未排空、内容AV、旧ABI、固定AV/动态输出与realtime CLI回归、完整矩阵及Windows后RKMPP仍开放。下一步优先验证新生命周期异常取消/回调和旧路径影响；整体75/FAIL，不推送合并部署。详细命令及证据见[C入口记录](realtime-video-composition-c-api-proposal.md)。

## 最新进展 r147：共享就绪证据抽取完成，生命周期接线仍待完成

原RuntimeBranch sender证据收集已抽为共享helper，两名独立源码审查PASS，全量Release configure/build exit0（realtime产物11:09:50）。未运行本轮媒体链路，不算r146修复。设计核对确认：Retired必须在runtime完整销毁后发布；回调异常不能抢占原始运行首错；现有firstOutputTimeout可作为操作watchdog并比较sender证据时间/activation代际，但不能冒充媒体硬期限。下一步复用发布机制接入固定AV/合屏初始输出完整生命周期，随后真实C入口与旧单源回归。整体75/FAIL，外部FFmpeg范围不变。

## 最新进展 r146：C入口实流失败，待接入完整输出生命周期

参数拒绝3项、preflight前取消复验通过；首次取消caller断言错误原exit6已如实保留。真实三源C入口在prepared通知报output identity mismatch，未RUNNING、未产输出，不算媒体验收。双审确认合屏缺少初始输出注册及READY/快照/退役完整接线，下一步复用已有输出生命周期和sender提交的随机访问证据，不能仅填ID绕过。16件临时文件、空temp目录及进程已清理；完整命令与结果见[C入口记录](realtime-video-composition-c-api-proposal.md)。此前间歇期限、内容AV、完整矩阵、旧ABI和Windows后RKMPP门禁仍开放；本轮未修改生产代码。

## 最新验证进度（2026-10-10，r145三源全部重入有限PASS／完整FAIL）

沿用r144双审Release，本轮无生产源码修改。三源初始及三源同时重入共六次，均原120秒/3600帧自然exit0；HEVC CBR8Mbps、1280×720/30、AAC192kbps→MPEG-TS/RTP，CLI caller_stop0。process/video/canvas同次最大19.5471/19.5079/19.4483ms，built间隔31.7151ms；启动调用间隔106.264ms不能解释r143活动期停顿。旧deadline未复现，不等于修复。

线端171905RTP序号异常0，视频8127/音频11666包DTS非递增0，完整AV解码exit0无warning。四个原始出口抽帧经主智能体及双独立审查实际查看，三格→全黑→三格恢复→全黑；静音检测20.787143秒间隔后恢复，末尾19.082562秒持续至文件结束。均为有限证据，不替代内容AV或全时段连续性。现场截图仍失败，已查到input桌面Screen-saver、执行线程Default，已请求用户恢复正常桌面。

payload205107预约=释放、结束0B/0对象；账本背压尝试61660，encoded差1包、AAC关闭2帧、mux pending188B仍非完整drain。250次外部监控CPU均值2.171767%、WS峰265408512B。两独立有限证据PASS，整体75/FAIL不变。

31件本轮产物记录后全部清理，原源和Release保留，宿主媒体/分析进程无残留。接下来推进真实C调用方的配置释放、事件/停止/取消与旧单源ABI运行门禁；恢复桌面后补现场验收，旧间歇期限仍待同规格证据定位。完整矩阵、内容AV、长稳及Windows后RKMPP保持开放，不推送、合并或部署。

## 前次验证进度（2026-10-10，r144有限证据PASS／完整FAIL）

聚合阶段调用墙钟与调用间隔诊断4文件已接入；双独立源码PASS，Release全量configure/build exit0（10:25:08、5792768B）。三源V/V/AV及随后所选AV重入均原120秒/3600帧、各自然exit0；HEVC CBR8Mbps、1280×720/30、AAC192kbps→MPEG-TS/RTP。CLI caller_stop exit0，不是自然EOF。

本轮未重现旧deadline故障。process/video/canvas最大16.6983/16.6567/16.5904ms为同次调用；audio最大1.4976ms，built相邻间隔32.9401ms；107.0106ms最大调用间隔在启动cursor0，不能解释r143活动期150.9643ms。两独立运行复核均限定PASS，不证明旧故障已修复。

出口RTP173558包序号异常0，原始TS两流DTS非递增0，完整AV离线解码exit0且warning日志0B；VLC录制文件另有启动DTS警告，现证据不能据此改生产时间戳。现场截图失败、RC快照无文件，画面门禁未通过。payload177987预约=释放归零，仍有47337次账本背压尝试；encoded差1包、AAC关闭2帧和mux pending188B，非完整drain。CPU外部均值2.113729%、WS峰263262208B；内部drift不代替内容AV。

22件产物已记录后逐项清理（pcap首次占用、核验进程后重试成功），宿主无本轮媒体/分析进程，原源和Release保留。下一步沿同规格Windows复现间歇deadline，使用新阶段记录定位；同时补画面和内容AV证据，不把一次未复现当修复。完整矩阵、单源回归、C API真实生命周期及Windows后RKMPP仍未闭合，维持75/FAIL。未提交、推送、部署或修改外部FFmpeg。

## 前次验证进度（2026-10-10，r143运行FAIL）

同AU不可变阶段诊断7文件已实现，双独立源码PASS；全量Release configure/build exit0，产物10:08:44、5786112B。r143已实际观察三格视频与右下黑格，CLI在源活动时因协议物化前绝对deadline超时自然exit1。

关联证据：迟到audio/video从scheduled创建到mux ready分别0.4488/0.5675ms，不能把这两个AU主要迟到归于TS后段。连续音频aggregate built间隔150.9643ms；后一块built→commit8.9us、commit→encoder input36.9us、input→submit577us。两名独立复核均确认数值与有限定位；built位于buildAudio末尾，仍包含该次构建本身，不能直接定责GPU/背压/worker。

资源53475预留=释放、最终0B/0对象；41次监控CPU均值2.506468%、峰4.268317%，WS峰251637760B。capture342145包/drop0、VLC与monitor正常退出；三源在失败后清理，约108–110秒，不计完整120秒。16件临时产物记录后清理，无媒体进程残留。

下一步记录aggregate调用进入、输入消费完成、流选择/deadline、buildAudio/buildVideo compose前后，以及pending transfer首次阻塞/解除；固定记录并关联样本/视频序号，区分实际处理区间与未进入onProcess的等待。r140/r142根因与原完整矩阵、单源、内容AV、C API生命周期、Windows后RKMPP仍未完成，完整75/FAIL保持。

## 前次验证进度（2026-10-10，r142运行FAIL）

九阶段有界音频诊断已实现，双独立源码PASS；Release全量clean-first/all-target exit0，产物09:44:28、5782016B。r141串行启动超时、未进入DAG，13件临时产物清理。r142改为三源并行启动，三路均完整120秒/3600帧exit0；CLI仍在源活动时因protocol datagram exceeded its absolute emission deadline before materialization自然exit1。

本轮未见late_audio_head/late_output_commit，mux最坏AU已晚于dispatch22.834199ms，batch取出再晚114.4us；需继续核验scheduler成功发布到mux消费之间的队列、背压及worker调度。九阶段音频最大事件间隔约36–45ms，不可当处理耗时，不证明r140根因消失。资源68059预留=释放、最终0B/0对象；60次CPU监控均值2.407691%、峰3.371331%，WS峰257654784B；80条内部filtered_phase均2461ns，不代替内容AV。

VLC截图句柄无效且RC快照无产物，未核验画面。续轮时全部媒体进程已不存在、工具句柄失效，capture缺最终drop统计、VLC缺正常退出证据。15件临时产物归档后逐项清理。原规格完整验收、单源回归、内容AV、C API真实生命周期及Windows后RKMPP仍未完成，整体75/FAIL保持。

## 前次验证进度（2026-10-10，r140运行FAIL）

5文件已恢复TS协议绝对deadline至RTP/UDP sender，并令maintenance按既定release唤醒；双独立源码PASS，Release全量exit0（09:29:03，5775360B）。r140原规格三源画面已确认，但运行约45秒输出时间时失败。新诊断定位：video head提前143.055468ms已到scheduler，在其中停留229.4934ms，输出构造前到commit仅10.8us，mux收到时晚86.809832ms；audio head自身晚63.53545ms。双独立核验支持跨流head等待拖晚视频，但未排除线程调度/音频上游生产延迟，不把停留时间全称音频等待。

最终payload35388预留=释放、0B/0对象，CLI自然exit1；三源因失败中止约99–101秒，不计完整120秒。capture/VLC正常退出，capture302278包/drop0；16件临时产物已记录后清理（pcapng首次文件占用，核验进程后重试成功）。原源与正式Release保留，无媒体进程残留。

后续源码与独立审查已确认：固定origin的同步编码音频可由sample end形成精确下界，但CopyPacket共享scheduler且无该证明，禁止通用duration推断。先补aggregate→encoder→canonicalizer分阶段有界观测，定位音频自身迟到，再决定是否实施producer下界合同；详见realtime-rtp-preparation-handoff.md的r140后审查。期限修复不等于完整合屏通过；单源回归、完整矩阵、内容AV、C API真实生命周期及Windows后RKMPP仍待完成，整体75/FAIL保持。

## 前次验证进度（2026-10-10，r139有限场景PASS，完整FAIL）

r139三路初始源和选中AV重入均完整120秒exit0，观察三格→全黑→左下恢复→全黑；CLI显式caller_stop exit0，payload175644预约=释放、最终0B/0对象。抓包609617包/drop0，输出172325个RTP包序号连续，原始TS音视频解码exit0，视频7959/音频11425包DTS严格递增，两段失源静音已记录。双独立审查仅有限场景PASS；主动停止仍取消1数据报、AAC残留2帧，不宣称无损drain或可信EOF自动完成。26件产物记录后逐项清理，无进程残留。

本轮没有迟到日志，最坏AU到mux仍早于dispatch74.779168ms，故r138迟到未定位、更未修复。原TS协议deadline映射缺口已源码确认；下一步恢复协议与wire驻留期限的独立约束，并形成maintenance与未来媒体进度的一致合同，不能只更早失败或扩大预算。内容级A/V、完整矩阵、C API真实生命周期及Windows后RKMPP仍未闭合，整体75/FAIL保持。

## 前次验证进度（2026-10-10，r138运行FAIL）

补充进度：scheduler新增head出队观测和输出构造前时间，仅丰富迟到日志；双独立源码PASS，Release全量重建exit0（09:15:06，5774336B）。源码另确认TS协议deadline未传递至RTP/UDP wire期限，maintenance时机及两类期限需一起核验；功能修复尚未实施，不能把诊断构建成功记为r138修复。

r138三源均完整120秒exit0，已观察三个视频格与空黑格；CLI在源仍活动时因MPEG-TS AU无发送窗口exit1。双独立调查确认AU提交前已晚174.350232ms，到mux晚174.642132ms；maintenance推进超过该AU截止33.334672ms为直接触发，不能认定唯一根因。资源账本57109/57109、末尾0B/0对象，仍不能抵消运行失败或AAC关闭残留2帧。下一步定位编码/跨路等待/credit提交迟到，核验maintenance deadline到sender的映射，保留原期限，不修改外部FFmpeg。17件产物记录后清理，无媒体进程残留。C API两名新独立源码审查均PASS，真实调用方仍待验。完整75/FAIL保持。

## 前次验证进度（2026-10-10，r136中断、完整FAIL）

r134编码AU调度修复最终双独立源码PASS，Release全量构建exit0。r136在宿主实流环境完成初始三源120秒（各exit0），全失源黑场/静音持续输出，并观察源2左下画面恢复；重入仅69.61秒后工具宿主中断，没有CLI退出/drain证据。原始输出144356个RTP包序号连续，提取TS音视频离线解码均exit0且无解码错误；VLC录制DTS/AAC错误未在线端文件复现。抓包缺最终drop统计，初始三格截图缺失，不判完整通过。

218次样本CPU均值1.229872%、峰2.94321%，WS峰269373440B；内部两代drift数据不等于内容A/V证据。双独立证据审查均判完整FAIL。命令、PID、统计、提取首轮失败与证据边界已记录运行文档；25件临时产物逐项清理，原源保留。

audio_bitrate_bps授权已落实：新C配置正值、精确整数kbps映射，无默认，不声称CBR。真实C调用方生命周期、原r134约334ms晚到根因、三源完整重入/结束、单源回归、完整矩阵、内容A/V、长稳、可信EOF与Windows之后RKMPP仍未闭合；完整75/FAIL保持。下一步沿原规格realtime复验，不修改外部FFmpeg。
## 历史验证进度（2026-10-05，r109–r113）

r113已以input/output抓包和预算边界诊断确认启动缺口位于本机接收覆盖，prepared满后停读8.534秒与有限socket耗尽相容；具体kernel/adapter位置未精确定责。诊断双审与重建完成，功能修复待所有权/持续接收设计，不能用扩容代替。

r109内部NVENC Immediate输出缓冲契约已实现、双独立源码PASS、Release全量构建成功。r110四路V/V/V/AV→HEVC CBR8Mbps、1280×720/30，初始和选定AV重入均完整120秒；原始182888 RTP包/drop0、RTP/TS/PTS/PCR连续，四格→黑场→右下恢复→黑场，CallerStop资源归零。内容匹配因原源重复多解为UNKNOWN，不能把相关性峰相减当真实A/V偏移；重入PID记录不足，完整验收未通过。

r112双路V+AV、第二路音源、H264 VBR8–12Mbps同规格失源/恢复与线端连续性获双独立有限PASS；额外启动2.441451秒静音对应input序号缺口，根因待关联prepared捕获/交接与入站包，完整音频门禁未过。27件产物已清理。

r111共享单路同规格HEVC CBR运行期画面更新、payload71867预留=释放；源自然exit0，CLI源结束后仍既有ProgressTimeout/exit1，完整优雅结束FAIL。两轮均已双独立复核有限证据、归档并逐项清理；未改外部FFmpeg，未推送/合并/部署。

剩余门禁：当前版本双/三路及编码RC矩阵、音源失活选择与内容级A/V同步、长稳、活动媒体stop/可信EOF、clock退休、r105服务窗口因果闭环、C API和Windows通过后的RKMPP。C API新增音频协议字段的[具体提案](realtime-video-composition-c-api-proposal.md)已获授权并实现，真实调用方验收待完成。保留独立75/100与完整FAIL。历史记录保留其当时边界；实际命令、指标见[运行记录](realtime-video-composition-wiring-runtime.md)。

## 目标与约束

在现有生产 DAG 内实现固定 2～4 路 RTP/RTCP 合屏，并同时提供 realtime CLI 和 C API。两路左右分屏，三/四路四宫格，按输入顺序排列，等比留黑边；指定一路音频。各源独立实时播放，不保证跨源拍摄时刻一致。单格断流黑屏，音源断流静音，恢复重新对齐；全部断流保持输出，显式停止或全部可信结束后关闭。启动前所有必需源必须完成权威探测。

验收输入统一使用既有 120 秒连续源，不低于 1280×720、30 fps、约 8 Mbps；合屏输出 MPEG-TS/RTP、1280×720、30 fps、目标 8 Mbps，覆盖 H.264/HEVC 与 CBR/VBR。Windows 通过后再验收 RKMPP。首期不包含动态输入、动态布局、自定义矩形、混音。

公共输入限于本会话已批准的输入列表、固定网格、指定音源、逐源输入等待期限、准备存储部署上限及独立 C 合屏提案中的协议事实和目标音频码率；具体语义见 realtime-rtp-input-latency-proposal.md 与 realtime-video-composition-c-api-proposal.md。编码意图与输出配置复用现有模型。内部时序、容量、同步和硬件产品必须由 planner 从真实证据推导。不得新增平台专用媒体链路、运行时 fallback、测试体系或样例常量。

## 工业实现依据

- [GStreamer 同步](https://gstreamer.freedesktop.org/documentation/additional/design/synchronisation.html)：公共时钟与源时间到 running-time 的映射。
- [GstAggregator](https://gstreamer.freedesktop.org/documentation/base/gstaggregator.html)：每输入队列、单聚合线程、带 PTS/duration 的 GAP 与事件顺序。
- [FFmpeg framesync](https://ffmpeg.org/ffmpeg-filters.html#framesync)：多输入选帧与显式结束策略；不能代替网络失活、重连与资源准入。
- [Rockchip 滤镜](https://github.com/nyanmisaka/ffmpeg-rockchip/wiki/Filter)：平台合成候选能力，必须与实际部署版本核对。

## 阶段一：多源基础

最新实施：组合图改收源专用视频/运行合同，源segment不再索取虚构encoder/FIFO；同decoder一次性交接及真实画布准备资源已贯通绑定。双独立源码审查和Release全量构建通过，r28实时及本地回归失败；完整prepare调用、跨源总准入、纯视频源和公共入口仍缺失，详见[源合同与准备画布](realtime-video-composition-source-contract.md)。本轮继续移除唯一输出encoder/发送规划对完整源计划的反向依赖，见[唯一输出合同](realtime-video-composition-output-contract.md)；准备协调器尚未闭环。以下完成门禁未因内部切片而勾选。

- [ ] 核实实际 Windows/RKMPP 合成依赖、帧池与完成语义，记录能力门禁。
- [ ] 取得现有真实 CLI 的功能缺失/单源约束证据，完成全量基线构建。
- [ ] 按 planner 源域组织 runtime binding、时钟、启动与恢复；保持最终输出单权威。
- [ ] 建立独立输出时间轴与有界多源贡献记录，避免任一路重连重置输出代次。
- [ ] 规划带时间区间的源缺口与恢复，内部故障仍明确失败。
- [ ] 汇总 decoder surfaces、滤镜持帧、合成输出、暂存、编码缓存、队列与线程准入预算。
- [ ] 回归现有单源 VideoOnly、AudioVideo 与动态视频多输出生产链路。

## 阶段二：合屏交付

阶段一恢复屏障之后的切入点经独立设计审查确认：在解码后的规范化帧与共享输出编码之间建立源域→输出域映射。`MediaRealtimeAvSyncRuntimePlan`、runtime registration及transition planner明确归属；单源purge只清其输入/解码/源侧缓存，共享encode、scheduler、mux和RTP序号保留在输出域。lineage须区分真实源贡献与生成黑帧/静音，不伪造源AU。聚合节点复用master clock和执行器deadline，以整数帧号/样本号推导连续时间，按有限候选选帧，到期缺失则生成planner确认格式/硬件驻留的黑帧或静音；下游背压时禁止积累无界tick。不能只让startup clock继续发tick，因为现有整链generation仍会重置输出。该设计尚未实现，不代表阶段一已通过。

r13真实恢复已锁新代并生成协议计划，随后暴露AAC不支持flush而mapper被重置的契约冲突。下一完整切片须同步修改source/output域产品、runtime归属注册、builder边界及音视频贡献模型；AAC codec、编码FIFO、packet mapper和唯一output origin从输出启动到全局终止连续保留。音频按prepared readback规划连续整数样本区间，缺口显式GeneratedSilence，恢复只接入尚未提交区间；现有interval accumulator复用区间算法，校验output generation，source generation移至贡献记录。仅删encoder purge注册、补静音却不处理视频、重写PTS或忽略旧包均不构成完成。源失锁不终止输出，只有显式停止或所有源可信结束才排空编码和协议。

- [ ] 通过既有 DAG 节点组合输入、解码、缩放、聚合、硬件合成、编码与协议输出。
- [ ] 聚合线程独占选帧状态，每次处理有界批次；慢源不阻塞输出，所有权和资源凭证使用 RAII。
- [ ] 音频解码、重采样与编码，连续样本时间轴补静音并在恢复时对齐。
- [ ] CLI/C API 共用控制入口；独立合屏配置保持既有 C ABI、枚举与生命周期语义。
- [ ] 增加逐源帧龄、缺口区间、代次、合成输出与 A/V 漂移观测。
- [ ] Windows→RKMPP 逐项真实验收，覆盖布局、转码、RC、断流、恢复、背压与停止。

## 当前接线门禁

2026-09-29：AV+AV生产准备、CLI共用controller、aggregate和逐owner总准入已接线，正在全量Release构建及独立审查修正；尚无本轮多源运行通过证据。纯V、C API与完整恢复/双平台门禁继续保留，见[生产接线记录](realtime-video-composition-production-wiring.md)。

基于a7836de5的源码核查确认encoder/canvas准备与最终graph池容量编译依赖成环，且现有单链ledger不能表示多源并发。先按[接线依赖核查](realtime-video-composition-wiring-gate.md)完成最终逻辑拓扑→总资源合同→真实准备→强制绑定，再接公共多源事务；此前基础组件保留，不再将额外同步helper视为公共接线完成。所有原验收项保持未完成。

逻辑拓扑与bind的拆分已实施，见[阶段记录](realtime-video-composition-topology.md)。下一块为准备授权及实际readback总准入；两者必须分开，不能用geometry先验值冒充实际分配字节。

输出池驻留数量已可由同一逻辑拓扑生成，见[数量合同](realtime-video-composition-pool-retention.md)。这不允许直接分配；有界准备预算及实际readback总账仍需完成。

## 当前实施顺序：源处理与持续输出分离

1. segment 显式返回源处理/输出处理成员；registration 校验互斥、完整覆盖和角色归属，runtime 消费同一产品。此步保留原整体 transition，不宣称恢复已修复。
2. 连续输出 generation/origin 与真实源贡献分开；音频复用整数区间运算，视频记录各源贡献；静音/黑帧不伪造源 AU。贡献条数、样本、字节分别形成有界契约。
3. 在规范化帧与共享编码之间接入聚合节点；单 owner、既有 master clock/deadline、有限候选与单 pending 事务，到期缺口生成黑帧/静音，背压不积累无界 tick。
4. 聚合边界完整后切换逐源恢复权限与唯一输出权限；AAC context、FIFO、packet mapper、scheduler、mux 与发送序号保持连续，源恢复仅清本源未提交候选。
5. 同规格真实源断流/重入验证后接入 2–4 路组合，完成 Windows→RKMPP 矩阵与双独立审查。

音频量子取 prepared encoder frameSizeSamples，不能写死编码器帧长。黑帧的格式、色彩范围和硬件上传必须经实际 capability/readback 规划；已有 packet-layout probe 的清零帧不能等同于黑帧能力证明。上述边界对应 [GstAggregator 的输入队列与 GAP/flush 生命周期](https://gstreamer.freedesktop.org/documentation/base/gstaggregator.html)，不是新增平台专用媒体链路。

## 交付门禁

同一分支 `feat/realtime-video-composition` 完成 commit/push；每个完整通过的真实链路立即独立提交，标题与报告列出协议、编码转换、分辨率、帧率、码率及 RC。冻结后两个未参与实现的独立智能体同时明确 PASS，再提交 PR 并由新智能体审核。更新架构、质量评分和中文完成记录，UTF-8/CRLF，不纳入临时测试或既有未跟踪产物。

进度与证据见 [实施记录](realtime-video-composition-progress.md)。

## 2026-09-21：编码提交事务与画布事实

编码器接受帧后无法撤销，贡献元数据须在 send 前完成验证和分配，成功后仅执行不分配的提交；EAGAIN 保留待发送帧并先接收，再重新准备候选。沿用现有 owner/lineage lock，不改变包时间轴与恢复语义。随后使用 Release 和原规格真实链路检查回归，双独立审查后交付。

已核实版本的 AAC 驻留推导及 CUDA 黑帧路径分别记录在[编码驻留调查](realtime-video-composition-encoder-retention.md)和[能力调查](realtime-video-composition-capability.md)。未知编码器支持范围不得因版本白名单而隐式收缩；黑帧尚需权威 SPS/VUI 范围、生产帧池预算与 aggregate 消费闭环。以上不替代阶段一/二原验收门禁。

## 2026-09-21：下一实施边界

持续聚合、域绑定与复用段的组合图已落代码，尚无组合入口；r22单源原规格结束后仍无进展超时，记录见[聚合接线](realtime-video-composition-aggregate.md)。下一步按以下依赖补全，不复制多个整链单源输出计划充当合屏：

1. 从现有preflight抽出逐源事实准备，验证真实SAR；将固定grid cell与等比内容矩形区分，常黑画布只拷贝内容矩形。
2. 抽取源处理/时钟和唯一输出规划，共用实际prepared编码器、设备与帧池；黑场、tile copy及分配证据前移到DAG构建前。
3. 现有资源compiler按明确owner选择事实，统一核算源输入/decoder/filter、聚合候选/贡献/canvas及唯一输出/协议；不能直接相加多个单源总账。
4. 内部生命周期产品已区分旧单源失败语义与持续合屏缺流，未发布代再次失锁、owner-thread purge、到期等待新证据及代次仲裁已通过源码双审。仍须由composition planner选择Preserve并通过真实源链路验证，首次准入及purge ack超时保持失败。r23仍走Shared单源路径，不能作为此模式运行通过证据，见[源生命周期记录](realtime-video-composition-source-lifecycle.md)。
5. 接通已批准的inputs/grid/audioSource公共配置与共用controller，补纯视频非音源、逐源缺口观测及Windows→RKMPP完整验收。迟到输出须有planner有界策略，单pending不等于历史追赶工作有界。

逐源纯规划公共切片已完成源码双审和Release全量构建：decoder/filter候选、评分和源执行契约由新source与旧single共用；显式首帧协商仅提供graph配置/readback。r24原规格单源仍在源结束后无进展退出，完整门禁未通过。下一步继续真实输入lease、decoder callback/首帧lineage交接和逐owner资源准入，不把候选或synthetic probe当作准备成功。见[逐源规划记录](realtime-video-composition-source-planning.md)。

canvas 图像 payload 准备已接入租约与取消，见[准备记录](realtime-video-composition-canvas-preparation.md)。仅覆盖公开图像 payload，不得冒充全部准备预算；下一步仍为首次 pool/readback 准备事务与多源总账。

逐producer事实与Registry已解耦并用于原生产编译，见[事实合同](realtime-video-composition-producer-facts.md)。下一步仍为逐allocation owner的独立源容量、候选与metadata总账；不以此解除aggregate最终门禁。

逐源资源事实已接同一topology构建/绑定，核对全图成员覆盖、逐输入envelope/ingress及补偿期音频上界；完整驻留总账尚缺，合屏仍FAIL42。见[逐源资源记录](realtime-video-composition-source-resources.md)。

准入调查确认maximumMetadataBytes缺planner写入者，元素数不能证明容器capacity；实施顺序调整为真实输入准备与共享metadata有限存储先闭环，再切换统一owner总账。见[准入门禁](realtime-video-composition-admission-contract.md)。本轮仅调查，未新增构建/媒体通过项，完整FAIL42。

共享canonical元数据已迁移为精确元素只读owner，源链和aggregate共同factory接入，identity按内容比较并持有寿命；不等于分配前全局准入，完整FAIL42。见[存储记录](realtime-video-composition-metadata-storage.md)。

raw RTP输入准备已独立为capture/sealed两阶段owner事务，原realtime preflight直接复用；startup仍从同一sealed输入权威规划。尚未接N源控制器与全局准入，完整FAIL42。见[输入准备记录](realtime-video-composition-source-preflight.md)。

## 2026-10-04 当前执行边界与进度

用户当前仅授权继续开发和本地验证，不执行push、合并、部署或外部服务购买；既有未提交改动全部保留，仍在feat/realtime-video-composition。仅运行realtime验收，Windows通过后再RKMPP，不修改外部FFmpeg依赖。完整合屏门禁保持未完成。

截至r73：Release全量重建与旧校正命令隔离修复双审通过。双路原120秒源、全部断流黑场、两路分别完整120秒重入、逐路再次断流均取得真实画面与持续输出证据，运行约7分钟错误/停滞为0；音频静音/恢复及明确stop仍缺完整证据，不能标记完整PASS。四源自然exit0，CLI/VLC测试后按精确PID清理，六日志/SDP/六截图已清除，命令与指标见运行记录。源码复核确认3/4路AV走通用容器路径待实测；纯V非音源仍被逐源强制音频阻塞，C API虽共用controller但composition配置尚未映射。r74四路实流在首源capture历史与probe描述符共用5MB预算处失败，尚未建图；必须先闭合准备保留合同，不能仅改变启动次序宣布修复。下一步继续该准备问题及上述接线。r66准备总预算、长稳/播放A/V、单源回归与RKMPP门禁仍未完成。

截至r83：源runtime可选音频合同16文件、聚合成员合同4文件均完成双独立阶段PASS和Release clean-first/all-target重建。r81混合实流越过源准备，在aggregate音频要求失败；r82越过aggregate，当前真实阻塞为CompositionGraphBuilder::validateOptions强制AV。r83双AV原120秒回归取得两幅更新画面和全失活黑场，源均自然exit0，CLI持续编码48810包、错误/停滞0、末尾WS214716416B；完整音频/同步/重入/stop/长稳门禁未关闭。各轮准确命令、退出原因、采样局限及清理见运行记录。全部r81–r83临时产物与精确测试PID已清理，复用源保留。

下一实现单元须完整按实际members贯通：CompositionGraphBuilder条件音频分支/target；InputGraphBuilder RTP单成员clock group；ProtocolInputBuilder及InputSegmentBuilder的binder/gate/canonical/端口/连接/成员；StartupCoordinator配置仅真实音频参数；SourceResources实际input/producer/retention；CommonCore与Aggregate图形状对应成员；CompositionPreparer仅绑定实际sealed audio。不得只去掉validateOptions门禁。其后Windows混合断流/重入和单源共享AV回归，C API映射及剩余门禁完成后才进入RKMPP。外部FFmpeg范围不变。

截至r85：12文件已贯通V SourceContribution的实际RTP clock/binder、startup/gate/canonical、音频可选source分支、sealed绑定、资源producer和shape。r84暴露factory拒Drop及复审发现的clock skew选项/Extractor六处固定AV输出事务均修正，最终双独立源码PASS与Release全量构建成功。r85真实AV＋V各原120秒、全黑、V原120秒独立重入、AV原120秒重入、各自结束后黑场持续，四源均自然exit0，CLI0errors/stalls，末次采样108658输出包、WS213831680B。只认有限启动/断流/重入/画面证据，完整音频质量与端到端同步、明确stop、长稳/回收、3/4混合、编码RC/协议矩阵、C API、RKMPP仍未完成。独立质量复评75/100，完整FAIL。r86同规格单源共享AV运行期有画面，源自然exit0后purge完成进入acquiring，约5秒后CLI无进展超时自然exit1；最终payload0且71918预留/释放平衡，完整回归FAIL。需核对共享源生命周期与应用无进展策略，不将BYE伪装为可信EOF。测试记录及6项清理已完成。

截至r87：四源AV+V+V+V均原120秒自然exit0，第四路V原120秒独立重入仅右下格更新，再次全黑后CLI持续输出；记录129524输出包、错误/停滞0。未核验初始四格画面，不能记完整四格通过。WS准备峰1060016128B、首次黑场280748032B、重入后黑场305573888B，长期驻留风险未关闭。12项临时产物及精确PID已清理，源保留。C API ABI安全提案已形成，新增音频协议字段尚待用户确认，见[入口提案](realtime-video-composition-c-api-proposal.md)。独立源码复核确认r86与r24的BYE→SenderLeft→重采集→ProgressTimeout一致，目前无证据归因optional audio回归；不修改单源终止语义、不将其记可信EOF通过。

截至r88：三路AV+V+V原120秒源均自然exit0，两次VLC画面更新且右下空格黑场，结束后全部黑场、CLI持续输出47379包，错误/停滞0；末WS241205248B、准备峰853467136B。9项临时产物及本轮媒体进程已清理，完整音频/同步/长稳/stop/C API/RKMPP门禁仍未关闭。r87/r88均为有限验证证据，不创建成功验收提交。

截至r90：r89在零匹配RTP包时准备超时，未建图，失败记录及6日志/进程清理完成；r90保持同参数、同批调度CLI与源，四路AV+V+V+V原120秒均自然exit0。H264_NVENC VBR（min/target/max 8000/8000/12000kbps）/AAC→MPEG-TS/RTP，1280×720/30，四格两次更新、全失活全黑且持续输出43399包，错误/停滞0。准备WS峰1061666816B、末282009600B；只记有限编码RC/画面/全失活证据，完整门禁未通过。10项产物及精确PID已清理，未改核心/外部FFmpeg或新增公开字段。

截至r91（2026-10-05）：V+V+V+AV、audio-source3、HEVC VBR原120秒源均自然exit0，四格运行正常，但最后纯V源purge后10秒binder初始时钟deadline失败，CLI自然exit1。最终payload0bytes/2objects，114708预留/114706释放，完整回收亦未证明。14项产物与精确PID清理完成。优先闭合[RTP binder断流边界](realtime-video-composition-rtp-binder-lifecycle.md)，禁止继续把早期有限成功视作全失活门禁完成。C API字段仍待确认。

r92：原始RTP AU的SSRC/tracker代际/共用序号已贯通packet buffer；BYE/clock失效owner清未组装数据，修正初代0误拒及旧reorder跨代两项审查发现。7文件双独立PASS、最终Release全量重建exit0。未运行媒体，r91尚待binder/source barrier完整修复及原规格复验，不能标记功能通过。两构建日志记录后清理。

### 2026-10-05 r93：原规格诊断复验

已完成binder稀疏状态边界日志、双独立审查PASS、Release全量重建exit0。四源V+V+V+AV（音源3）H264/AAC→HEVC VBR 8000/8000/12000kbps、1280×720/30、AAC CBR192kbps、MPEG-TS/RTP：初始四源及末位AV重入均完整120秒自然exit0，四格→全黑→仅右下恢复→全黑，输出持续、errors0。本轮未复现r91，不能视为确定修复或完整验收。仍待binder原始身份消费、全成员owner边界、首采/重采期限、音频线端验证、内存长稳、停止与C API、Windows通过后的RKMPP。实际命令与证据见wiring-runtime的r93；记录后清理14件产物和精确测试进程，保留源，不推送/合并/部署。

### 2026-10-05 r94：媒体owner屏障与预检遗漏修正

已将实际Raw/binder纳入源purge参与者，按逐流AU序号frontier退休旧媒体；复用既有有限read timeout、精确producer waiter取消及源域唤醒，不更改协议代际权威。首轮三项审查问题修正后18文件双审PASS、Release构建exit0，真实预检仍因Raw同步域kind/key合同遗漏自然exit1，不能计通过。已补齐明确合同，19文件再次双审PASS，进入r95全量重建和原规格复验。r94四源被中止，8日志及精确进程已清理；完整门禁仍未通过。

### 2026-10-05 r95/r96：屏障运行证据与共享链路回归

19文件最终双审PASS、Release全量重建exit0。r95原四源V+V+V+AV及末位AV重入均完整120秒自然exit0；HEVC VBR 8000/8000/12000kbps、1280×720/30、AAC CBR192kbps、MPEG-TS/RTP，四格→全黑→右下恢复→全黑，两次owner/binder逐流cutoff一致、无expired、运行错误0。r96同规格单路HEVC CBR8000kbps播放正常，源exit0，BYE后CLI经既有ProgressTimeout自然exit1；payload最终0B/0对象、71903预留=释放，不能标为优雅结束成功。独立复核确认无证据将该既有退出归因本次修改。r95/r96共20件临时产物及精确进程已清理。

剩余门禁：全部clock分支退休、已激活源的等待期限、端到端音频/同步、内存长稳、显式停止/可信EOF、C API及Windows通过后的RKMPP；本轮有限证据不等于合屏完成，不提高质量评分，不推送/合并/部署。

### r97：已激活源等待音频SR的确定失败

四源原规格完整120秒后仅令末位AV重入音频skip_rtcp（视频SR正常），node65于11:51:08.527建立首次期限、11:51:18.527精确10秒到期。日志preserves_output1、phase2、300待绑定视频包；CLI自然exit1。明确证明r94媒体退休屏障不能替代已激活源等待合同。初始四源exit0，失败后诊断源中止exit=-1，10件产物和精确PID清理完成，实际命令及全部指标见wiring-runtime。

下一修复必须把候选媒体驻留退休与协议/playback代际关系作为一个合同：首次准入仍失败，输出已激活时仅隔离失活源并保留黑场/静音持续输出；不得借用推进playback代际的重采集请求来盲目清包，也不得只抑制错误。先明确clock消费/输出授权与非协议重采集映射，再实现有界候选退休，双审、Release重建后复验该失败条件以及正常SR原规格链路。

### r98：候选驻留修复及失败条件复验

两文件修改双独立PASS、Release全量重建exit0。原四源H264/AAC→HEVC VBR8000/8000/12000kbps、1280×720/30、AAC CBR192kbps、MPEG-TS/RTP，各完整120秒；随后缺音频SR与恢复正常SR两个重入源也各完整120秒自然exit0。原r97失败点改为单次退休301个未绑定AU，黑场输出持续；正常SR恢复后Target2媒体重新绑定，右下格及第2代音频内部同步恢复，再次断流继续全黑。CLI运行errors/workerErrors/stalls0。17件产物及8个精确PID清理完成。

该修复不改变generation、不新增Raw退休事务，复用解码前startup关键帧门禁。仅证明已激活Acquiring阶段候选过期不再杀死会话且可恢复，未关闭非协议代际映射、clock完整退休、线端静音/音频质量/同步、长稳、停止/C API和RKMPP。共享单路新版本回归r99正在执行，未推送/合并/部署。

r99共享单路新版本回归完成：原120秒源exit0、两次VLC画面更新，BYE后CLI沿既有ProgressTimeout自然exit1；无新增候选丢弃分支，最终payload0B/0objects、71901预留=释放。完整优雅结束仍未通过。6件产物及精确进程清理完成；本轮r98/r99共23件临时产物无残留，指定源保留。r98两文件双审PASS、构建成功和缺SR失败条件修复证据成立，完整合屏仍按剩余门禁推进。

### r100设计：CLI显式停止入口

源码确认CLI未连接control.requestStop，而Beta已有统一入口。参照FFmpeg sigterm_handler只记录停止事实、主执行路径清理，以及Windows SetConsoleCtrlHandler官方语义，在tools/realtime_video_cli添加RAII信号适配器：Windows只接Ctrl+C/Break，POSIX接SIGINT/SIGTERM；回调仅写保证lock-free的atomic_flag，普通jthread按现有runPolicy.pollInterval等待并转交requestStop。注册覆盖controller.run，析构join线程并恢复handler；一个固定线程、一个停止位，无队列，不在信号回调调用mutex/stop callback。平台差异仅为OS注册/恢复，媒体停止链完全共用。无新增CLI参数或stdin命令，不把console关闭/关机强杀当优雅停止。官方依据：https://ffmpeg.org/doxygen/trunk/ffmpeg_8c_source.html 、https://learn.microsoft.com/windows/console/setconsolectrlhandler 。

验证分开记录：运行期全失活后信号应得到CallerStop及正常回收；合屏准备取消可为Cancelled/CallerStop非零退出；单源preflight未传token，不能声称立即取消。先双审和Release全量重建，再仅Windows realtime真实源验证；不测试local，不扩大FFmpeg。

r100双审修正：POSIX进程任意线程接收SIGINT/TERM会使网络poll返回EINTR，在controller停止标志建立前可能误报WorkerFailure。因此主线程在创建媒体线程前屏蔽这两个信号，仅bridge线程解除屏蔽；bridge启动通过promise传回注册错误，析构先join再恢复handler及原mask。采用pthread_sigmask官方继承语义（https://man7.org/linux/man-pages/man3/pthread_sigmask.3.html），保留同一requestStop媒体路径，Windows继续SetConsoleCtrlHandler。此为源码修正，尚未运行Linux/RKMPP验证。

### r100–r103完成范围：Windows全失活后的显式停止

四文件信号适配器最终双审PASS、Release全量重建exit0。r100四源120秒播放/黑场正常，但PTY Ctrl+C终止宿主，停止证据不通过；r101无源预检先超时，不算信号验证；r102独立控制台定向Ctrl+C得到CallerStop/Cancelled exit1，证明合屏准备取消。r103四源完整120秒自然exit0，四格→全黑持续→定向Ctrl+C后CLI exit0，69ms内部stop段，workers0、payload0B/0objects、120996预留=释放，双独立运行证据复核PASS。仅关闭Windows全失活后取消停止，不代表活动媒体中途停止、可信EOF排空、全堆/GPU泄漏审计或POSIX实测。stderr滤镜属性提示和AAC尾帧未排空已保留。完整命令、指标与26件临时产物清理见wiring-runtime。

剩余：协议/playback非协议重采集映射、完整clock退休、线端静音/音频质量/同步、长稳、其他停止阶段、C API和Windows通过后的RKMPP。保持75/100及完整功能未通过，不推送/合并/部署。


### r104失败与r105诊断

为补齐H264 CBR及接收端音频门禁，四源原120秒配置首次运行在22秒触发MPEG-TS access unit has no transport service window，CLI自然exit1，不能标记音频/恢复通过。源码双独立调查确定dispatch-max(emit,availableAt)<=0，已有窗口字段通过校验；未确定是媒体准备迟到还是维护时间线推进等上游原因。r105单文件只增强失败诊断（stream/emit/dispatch/available/last commit/actual now/bytes），不修改调度或放宽窗口，双独立PASS后Release全量重建，同规格复验。FFmpeg官方DTS/PCR关系说明已对照，不扩大外部依赖。r104九件产物和进程已清理。


r105同规格再次失败且取得audio AU现场：到mux迟于dispatch133.956661ms，maintenance可用时间超过dispatch11.655693ms；同一audio在emit前17.922139ms已是scheduler head，因missing video无deadline等待，排除该AU直到截止后才首次由AAC产出的说法。r106追加成功发布后的late_output_commit诊断，划分scheduler与下游延迟，两个诊断文件双审PASS，未修改功能。FFmpeg max_interleave_delta提供有界跨流等待先例（https://ffmpeg.org/ffmpeg-formats.html#Format-Options），但其通用策略不能直接保证本项目全局emit单调与严格TS服务期限；不得直接删barrier或添加经验超时。r105十件产物及进程已清理。


## 2026-10-05 r104–r106：发送窗口失败与恢复证据

r104/r105原规格Windows四路RTP/RTCP→H264 CBR 8000kbps、1280×720/30＋AAC CBR192kbps→MPEG-TS/RTP失败。r105同一音频AU在emit前17.922139ms已到scheduler，却因缺video等待，mux接收时晚于dispatch133.956661ms；此前maintenance超过dispatch11.655693ms。当前合同没有缺流下一AU的权威DTS下界，不能猜测帧率或放宽窗口规避失败。

r106仅增加成功提交时刻诊断，双独立源码PASS，Release全量684任务成功。四路完整120秒→全黑→选定AV完整120秒恢复右下格→全黑→Ctrl+C正常exit0；payload0B/0对象、203332预留=释放。接收324.185644秒录制支持画面恢复和失源静音，但VLC重新封装启动PTS有一次回退，原始线端连续性、选定音频内容与端到端同步仍待验证。r104/r105未修复，完整功能FAIL；保留独立75/100，不据单次未复现重评分。21件临时产物已逐项清理，命令、指标与证据边界见docs/realtime-video-composition-wiring-runtime.md。未运行local CLI/RKMPP，未提交、推送、合并或部署。
### 2026-10-05 r107诊断进展
原四路120秒Release realtime复跑，源均exit0；全源结束后持续输出，定向Ctrl+C后CLI CallerStop/exit0，payload117897预留=释放、0B/0对象。未复现既有服务窗口错误，未宣称修复或完整验收。共享唤醒链未发现丢通知；missing日志受状态变化抑制，不能据230ms间隔推导编码批量。下一步补编码包实际产出/发布与scheduler接收的同AU时间关联。r107九件产物记录后清理，原源保留。详见runtime记录。

### r108诊断实施边界
复用已有scheduler首head日志，扩展为首次/迟到head到达诊断；仅State启用且实际mapped dispatch已到期时补日志，以stream/generation/canonical dispatch关联既有成功commit与mux失败。普通head不写日志，不修改排序、deadline、失败语义或参数；映射/时钟读取失败只限制诊断，不新增媒体失败。取head后的采样不是channel发布或编码产包时刻，不能单独归因上游。沿用FFmpeg按DTS交织与严格TS时间线依据，阶段取证遵循已读systematic-debugging技能；禁止凭结果猜提前量。Release全量重建后原规格realtime验证，双独立审查。

### r108结果与下一修复候选
修正版代际一致head诊断双独立PASS，两次Release全量重建成功；四路原规格120秒，源自然exit0，输出持续后Ctrl+C/CallerStop exit0，payload116285预留=释放，最终0B/0对象。未复现窗口失败；11件临时产物已清理。完整合屏FAIL及75/100维持。

只读独立调查确认：NVENC后端不消费通用AV_CODEC_FLAG_LOW_DELAY；项目仍通过禁B帧实现一部分低延迟，但没有覆盖官方私有delay输出积攒门槛和zerolatency重排选项。FFmpeg delay默认INT_MAX，经surfaces裁剪；无B帧/无lookahead/默认surfaces条件下async_depth为3，这是队列阈值而非固定3帧延迟。现有prepared回读未按低延迟需求校验这些选项。

下一候选：从已有lowLatency需求形成内部类型化输出缓冲/重排契约，由NVENC adapter应用官方delay=0及所需zeroReorderDelay；prepared/production/packet-layout共用应用路径，打开后按计划值回读校验，核对tune/lookahead冲突。保留preset、分辨率、码率、帧率、编码复杂度，不新增公共参数、不改FFmpeg依赖、不放宽窗口；RKMPP不能照搬NVENC选项。此候选尚未实现，也不是r105根因证明。权威依据：https://raw.githubusercontent.com/FFmpeg/FFmpeg/master/libavcodec/nvenc.c 与 https://docs.nvidia.com/video-technologies/video-codec-sdk/13.1/nvenc-video-encoder-api-prog-guide/index.html 。实施前仍需核对当前生产lowLatency事实及probe路径一致性。
### r109内部NVENC输出缓冲契约
已核验CompositionPreparer调用planOpenContract(...,true)。planner对NVENC低延迟选择内部Immediate产品，其余BackendManaged；内部option必须显式传递，无新增公共参数。adapter映射官方delay=0/zerolatency=1，打开后校验计划值及无lookahead/B帧冲突；不改preset/tune、编码复杂度或外部FFmpeg。生产builder与packet-layout probe共用adapter，拒绝不支持或不匹配，不fallback。依据延续r108官方源码/SDK调查；这修复已证实的低延迟契约缺口，不承诺r105根因已修复或硬件瞬时完成。Windows原规格验证后才RKMPP。

## 2026-10-05 r109：NVENC输出缓冲契约已实现

内部Immediate产品由planner选择，生产与所有prepared/probe路径共用官方delay=0/zerolatency=1应用及计划值回读；无新增外部参数、依赖修改或规格降低。修正主probe漏校验与FFmpeg只读查询const接口问题后双独立源码PASS，Release全量重建成功。原四路120秒及选定AV重入120秒均完成；CallerStop exit0，payload202418预留=释放、最终0B/0对象。

原始线端180秒覆盖黑场→重入→黑场：94048 RTP包/drop0、无RTP序号及TS CC缺口，视频5400包步长3000、音频7751包步长2089/2090，PTS/DTS无回退。四个离线画面采样支持四格/黑场/右下恢复/黑场；不代表内容级A/V同步、首轮失源边界或长稳。r105未复现但根因未闭合；完整Windows矩阵、共享单路回归、音频内容同步、长稳、C API及RKMPP仍未完成。保留此前75/100及完整功能FAIL，不以局部修复重评分。详见docs/realtime-video-composition-wiring-runtime.md。

## 2026-10-05 r110：HEVC CBR四源复验

原120秒四路RTP/RTCP与选定AV重入120秒→HEVC CBR8Mbps 1280×720/30、AAC CBR192kbps→MPEG-TS/RTP；CallerStop exit0，payload203585预留=释放、末0B/0对象。原始182888包/drop0，RTP/TS连续，视频8935包PTS步3000、音频12826包步2089/2090；四格→黑场→右下恢复→黑场。双独立有限PASS，内容匹配重复多解，内容A/V为UNKNOWN；重入PID缺失。未关闭r105因果、完整矩阵/长稳/C API/RKMPP，完整FAIL及75/100保留。30件产物已按清单清理，原源保留；详见runtime记录。

## 2026-10-05 r112：双路 H.264 VBR 与启动交接取证

原120秒V左/AV右、选定index1，错开约11秒并让右源完整120秒重入；H264 VBR目标/最低8Mbps、最高12Mbps，1280×720/30与AAC CBR192kbps→MPEG-TS/RTP。172858包/drop0，RTP/TS/PTS/PCR连续；五帧支持双路、仅左、黑场、仅右恢复、黑场及DAR留边。CallerStop exit0，payload170757平衡归零。双独立有限PASS，27件产物已清理。

完整音频门禁仍FAIL：额外启动静音0.085873–2.527324秒（原源同阈值无静音），对应输入video序号10048→13450、时钟失效和重新锁定。独立代码调查确认并行capture在非probe阶段预算耗尽可无错误停止drain，runtime先replay后live；未记录本轮预算命中/停止时刻且未抓input，不能据此认定丢包根因。下一步保持相同源规格及启动间隔，关联入站包、capture停止/预算、seal、replay排空及首live序号。禁止猜测扩容或调整规格。内容同步UNKNOWN、r105因果/剩余矩阵/长稳/C API/RKMPP仍待完成，保留75/100。r111共享单路完整结束仍ProgressTimeout/exit1。见runtime完整命令和结果。

## 2026-10-05 r113：启动缺口已定位至本机接收覆盖

单文件prepared capacity_stop/sealed诊断，修正快照竞态及Result访问后双独立PASS、Release全量重建成功。原双路H264 VBR8–12Mbps、720p30、AAC192，同原120秒源及约11秒错开启动，抓input/output304326包/drop0。实际prepared 5MB预算耗尽后停读8.534秒再seal；应用缺3557包在线端启动期对应序列全部存在，排除发送/抓包点缺失。停读尾后已覆盖约5MB，随后出现缺口，与有限socket耗尽高度相容；没有kernel计数，不唯一归因OS。主动purge晚于gap，非首次缺口原因。

启动静音再现2.270998秒；诊断成立不等于修复。CallerStop exit0、payload92095平衡归零，19件产物清理，原源保留。下一项为有限探测与持续live接收阶段、pin/驻留共同预算、稳定退休身份及单owner交接合同；不能直接弹出probe借用deque或猜测扩容。完整功能FAIL/75分及其他门禁保持，未推送/合并/部署；详细命令和因果边界见runtime记录。

## 2026-10-05 r114：持续接收与真实容量设计调查

已形成[准备接收交接设计](realtime-rtp-preparation-handoff.md)：全源共享设备/几何和全图准入阻止直接提前运行单源DAG；选择能力证据与live发布分离方向，继续复用原协议与startup节点。新确认MediaUdpSocket最大容量vector经resize后仍保留capacity，prepared仅size记账；r113的4173×65507=273356631B为元素容量下界，不是heap测量。Linux prepared旧接收还缺截断检测，runtime adapter保护不能替代。

真实容量/租约预算、协议状态连续移交、首次live准入/接管三个具体决策尚未闭合，完整设计未PASS，不先写rolling策略或宣称修复。无新增公共参数/外部依赖，本轮尚无功能代码变更或新实流测试，r113证据和完整FAIL/75分保持。

## 2026-10-05 r115：准备存储生命周期核验

新增确认：初始视频信令 deque 在移交前未 retain；prepared 出队在调用方仍持有载荷时提前 release；runtime arena 创建与旧 prepared/pin 存储重叠。现有 ingress storage 是固定最大报文槽、整批 lease，不能直接替换长期 pin 队列。接口责任与调用点已记入 realtime-rtp-preparation-handoff.md；下一实现需以载荷 owner 携带唯一 RAII 预约，明确 descriptor 容量产品及交接峰值。仅改 size→capacity 不成立。本轮未改生产代码、未构建/实流复验，未完成合屏验收，既有失败与评分保持。

## 2026-10-05 r116：准备内存实际分配预约组件

新增 MediaPreparationMemoryResource 两文件，复用既有 RAII budget/lease，在 std::pmr 分配边界计入请求字节、header 和对齐 padding；每块保活 resource，实际释放后才归还预约。两个独立源码审查均 PASS，Release 全量重建 exit0，新 cpp 已编译；realtime 产物时间20:38:42、5615616B。未测试 local，未运行新实流，未推送。组件尚未接入 prepared 队列，不能标记容量或停读问题修复。下一步接入 payload/descriptor/scratch 的唯一分配 owner、明确 planner 总预算及交接峰值并删除旧队列级重复记账；完整合屏门禁保持未通过。详见准备交接文档 r116；本轮单件构建日志记录后清理。

## 2026-10-08 r117：接收缓冲改动与失败复验归档

共享 receiveInto(span)、单receiver复用scratch及共同交接前释放已实施；Linux拒绝截断，Windows保留原错误传播。两个独立源码审查PASS，Release全量重建exit0。原120秒双路RTP/RTCP→H264 VBR8000–12000kbps、1280×720/30、AAC CBR192kbps→MPEG-TS/RTP复验，实际首包错峰13.336秒。303315包/drop0、输入输出RTP序号与TS CC连续，4116视频包PTS步3000、5909音频包步2089/2090。

仍FAIL：准备预算满后停读10.089秒，应用缺4867包在线端全部存在；启动静音0.817982秒。离线帧支持双格→仅左→黑场，VLC桌面截图失败，内容A/V同步UNKNOWN。会话恢复后发现CLI延续到22:36发生scheduled datagram deadline失败；未取得最后进程退出码，不能标正常停止/长稳PASS。最终payload1485487预留=释放，0B/0对象。PMR总预算与消费者租约尚未接入；完整矩阵、恢复、可信终止、C API、Windows→RKMPP门禁保持未完成。既有75/100不重评分。

实际命令、PID、指标及证据局限已写入docs/realtime-video-composition-wiring-runtime.md的r117节。23件临时产物已按清单清除，原120秒源保留，五个PID均无残留；没有推送/合并/部署或外部FFmpeg修改。下一实现仍为统一准备存储owner/预算及持续live接收交接，不以晚期失败替换主任务。

## 2026-10-08 r118：PMR 接线构建通过，容量语义 P1 未关闭

prepared 队列/payload/scratch 已接入实际 allocation 预约，移除出队提前释放；Release 全量构建 exit0，realtime 5625856B（12:13:20）。两个独立审查结论不一致，其中一位明确 FAIL：同值 wire/allocation 上限缩短探测窗口，不能以接线成功判定正确。尚未跑本轮实流，完整合屏仍未通过。下一步为收流前固定 descriptor 准备/readback、独立 wire/storage 产品及最终 ledger 每源一次的重叠计费；不得凭经验扩容。完整问题、边界与后续步骤见 docs/realtime-rtp-preparation-handoff.md 的 r118 节。构建日志记录后按单文件清理；无新增公共参数、外部 FFmpeg 修复或推送/部署。

## 2026-10-08 r119/r120：容量语义 P1 源码关闭，真实合屏仍 FAIL

新增内部 MediaRawRtpPreparationStoragePlan/Planner；wire 探测与实际 allocation 上限分离。MediaOwnedByteStorage 精确单次分配，footprint 与实际 allocator 共用布局；复用 MediaFixedCapacityStorage 和 std::remove_if，预分配描述符、原地筛选RTCP，消除deque增长/双队列并存猜测。空报文在transport拒绝，N≤wireBytes为保守数量事实；每流数组在收流前分配。覆盖共享wire载荷/header及每流scratch、一个未observe载荷；未新增公共参数。budget窗口仍用wire，未把约605MB allocation上限误传为探测/ingress payload上限。

r119_storage_review_a/b 对冻结源码均PASS；Release clean-first/all-target exit0，realtime5627904B（13:28:20）。r119因调度导致第二源晚于30秒open而失败，原规格统一调度r120重跑。两源完整120秒exit0、VLC可见双路→左→黑、显式CallerStop exit0且payload102129预约=释放、0B/0对象。

完整验收仍FAIL：准备停读7.633秒，缺2611包在线端全部存在；启动静音3.254467秒，内容A/V UNKNOWN。306351包/drop0，输出TS CC及PTS连续只证明局部时间线。进程188点覆盖准备期，private峰1072721920B、WS峰228687872B；固定描述符过于保守，不能称内存优化。原source124427809B保留，全部本轮进程/临时文件已清理；最终归档命令一度解析失败，清理后依据完整已返回输出立即补记，详见runtime记录。无push/merge/deploy/RKMPP。

下一步仍须完成每源共享prepared allocation与runtime arena重叠的最终ledger、持续接收/稳定pin退休和协议交接，再以原规格复验。C API、完整矩阵、内容同步、恢复/可信停止及Windows→RKMPP门禁未完成；75/100保持。

## 2026-10-08 r121 进度

prepared allocation 已按实际共享身份进入 single/composition 最终 ledger，AV只计一次并与runtime arena相加，节点归属避免动态输出后缀重复计费。双独立源码复审PASS；补缺失<algorithm>后Release全量重建exit0。原120秒双路RTP/RTCP→H264 VBR8000–12000kbps、1280×720/30、AAC CBR192kbps→MPEG-TS/RTP已复验，CLI caller_stop exit0、payload93426预约=释放/0B/0对象。

整体仍FAIL：准备停读7.705秒，缺2691包在线端全部存在，启动静音3.138231秒，内容A/V UNKNOWN；private峰1077166080B。输出RTP/TS CC/PTS连续仅为局部证据。完整矩阵、恢复、可信EOF、C API与Windows→RKMPP门禁未通过，75/100保持。详见runtime r121记录。22项临时文件已按清单清除（两项短暂占用后重试成功），全部本轮进程无残留，原源124427809B保留；无推送/合并/部署。

## 2026-10-08 r122 进度

稳定immutable entry、独占PMR snapshot及单调ingressFrontier已接入，replay时间独立保存；同allocator/type实际控制块分配回读进入planner容量，释放引用表先于归还probe pin。两个独立源码审查PASS，Release全量重建exit0（realtime5652480B/14:14:11）。原120秒双路RTP/RTCP→H264 VBR8000–12000kbps、1280×720/30、AAC CBR192kbps→MPEG-TS/RTP复验：caller_stop exit0，payload95082预约=释放/0B/0对象；private峰714657792B，较r121减少362508288B，但保守allocation cap增大。

完整仍FAIL：停读7.689秒、2691缺包线端完整、启动静音3.115102秒、内容A/V UNKNOWN。稳定引用未实现rolling和持续接收；完整矩阵、恢复、可信EOF、C API与Windows→RKMPP门禁未通过，75分保持。详细命令/进程/时间线/边界见runtime r122。20项临时文件已按清单删除，进程无残留，原源124427809B保留；无push/merge/deploy。

## 2026-10-08 r123 进度

已提取可移动单owner MediaRtpInputClockSession，生产RawRtpInputNode复用同一RTCP parser/tracker/schedule；planner必填配置、generation/失效策略/事件顺序保持。双独立源码PASS，Release全量exit0（realtime5654528B/14:31:40）。原120秒双路RTP/RTCP→H264 VBR8000–12000kbps、1280×720/30、AAC CBR192kbps→MPEG-TS/RTP复验：caller_stop exit0、payload93415预约=释放/0B/0对象；private峰717029376B。

整体仍FAIL：停读7.717秒、2710缺包线端完整、启动静音3.115941秒，内容A/V和VLC完整画面证据不足；完整矩阵、恢复、可信EOF、C API、Windows→RKMPP未通过，保持75分。下一步输入事实可按源提前规划，但需显式观察快照冻结、共享协议处理和AU归属及时间轴交接，不能提前运行未准入DAG或直接删sealed校验。详见准备交接设计与runtime r123。18项临时文件已清理，全部本轮进程无残留，原源124427809B保留，无push/merge/deploy。

## 2026-10-08 r124/r125 进度

每源事实已从应用层等价迁入planner，在各自探测完成后生成；跨源准入仍在汇合后。r124双独立源码PASS、Release全量成功，但实流首次时钟获取超时exit1、输出0，保持FAIL；停读7.709秒和2629线端完整缺口未解决。r125补充时钟组候选NTP/arrival和原拒绝错误诊断，双审复审PASS、全量成功，原规格复验中。没有修改阈值、启用未准入DAG或修复外部FFmpeg。后续需闭合原arrival与live时间轴、共享协议处理和AU归属；完整Windows门禁前不进入RKMPP。

### r124/r125 最终进度（未完成合屏验收）

每源facts提前规划及时钟拒绝诊断均经两名独立审查PASS、Release全量成功。r124首次时钟超时exit1/输出0；r125同规格完整120秒源结束后持续输出、caller_stop exit0且账本归零，但停读7.671秒/2692包线端完整缺口/启动静音3.161043秒仍FAIL，r124超时未复现未判修复。18项r125临时产物已按清单删除，全部本轮PID无残留，原源124427809B保留。详见runtime r124/r125。下一行为修复仍须连续协议会话与真实接收时间、AU归属和启动窗口交接；不扩大外部FFmpeg范围，Windows门禁通过前不进入RKMPP。

### r126 进度

共享MediaRtpInputProtocolSession已接入生产RawRtpInputNode，原pending/reorder/depacketizer/clock/recovery状态同owner；两阶段额度仍由node执行，先预约后组包。双独立源码PASS、Release全量exit0。原120秒真实双路链运行至caller_stop exit0、payload94764预约=释放/0B/0对象；停读7.645秒/2676线端完整缺口/启动静音3.114921秒仍FAIL。明确停止时encoded入出差1包，不记完全drain。18项临时文件已记录后删除，5PID无残留，原源保留。新获授权逐源等待预算开始接线，尚未宣称连续接管或Windows门禁通过。

### r127 进度：逐源等待预算已接线并实测

用户明确允许逐源input-latency-ms/input_latency_ms并要求可配置。CLI及内部typed契约已接入，合屏每源正值必填；同源AV共用，bootstrap/final/bind/validator保持声明值，旧有限probe模式不在失败后回退。旧C ABI无改，新合屏C载体其余字段仍受原提案确认边界约束。

首次构建发现校验误插helper的编译错误，修复后双独立复审PASS、Release全量exit0。真实CLI缺值/0均立即exit2。原120秒双路RTP/RTCP→H264 VBR8000–12000kbps、1280×720/30、AAC CBR192kbps→MPEG-TS/RTP，节点确认100/200/200ms，caller_stop exit0、payload97545归零。停读7.668秒/2691线端完整缺口/启动静音3.115578秒仍FAIL，未判r124时钟超时修复。18项实流临时产物及4项负例日志均记录后删除，5媒体PID无残留，原源124427809B保留。详见runtime r127。

下一步以稳定wait产品建立不中断接收的观察快照，再接共享protocol session、完成AU的ID/owner与现有coverage/window selector；尚未实施连续退休或会话交接，Windows完整门禁前不进入RKMPP。

### 2026-10-08 r128 进度：输入容量候选与最终核验

非破坏累计快照、逐流弱owner、固定wait容量候选已实接begin→seal，保留stop/join/budget seal；最终检查owner/frontier/五项累计观察不倒退及容量产品一致。初审发现音频晚到提前拒绝，修复为原deadline/cancel内仅对精确缺正时间跨度WouldBlock等待，修复后双独立PASS、Release全量exit0。共享AV预算跨流失败无即时notify仍是有界延迟风险，未宣称全局fail-fast。

无音频补充负例不再立即拒绝，8.42秒因共享prepared容量耗尽/音频capture结束exit1；不记完整30秒等待或完整验收。原120秒双源RTP/RTCP→H264 VBR8000–12000kbps、1280×720/30、AAC CBR192kbps→MPEG-TS/RTP实流caller_stop exit0、payload96931预约=释放/0B/0对象。仍停读7.694秒、2691包线端完整缺口、启动静音3.107302秒，内容A/V UNKNOWN，完整仍FAIL。独立评分复核保持75/100。19项实流/构建临时文件及7项负例产物已记录后逐项删除，全部本轮PID无残留，原源124427809B保留，无外部依赖改动或push/merge/deploy。

下一闭合实施范围：沿共享parser/reorder/depacketizer保留AU完整entry成员与额度，准备首帧probe消费同一session的有界AU产物，按序保存clock/invalidation/AU，再向runtime事务性交接真实arrival、处理ordinal、pending及未完成AU；复用现coverage/window选择退休。当前累计wire预算尚未成为resident预算，不能只放入session指针或按时间删raw包解除停读。完整Windows门禁前不进入RKMPP。详见runtime r128与准备交接文档。

### 2026-10-08 r129 进度：共享时钟组状态核心

生产节点已接入单所有者、可移动的 MediaRtpClockGroupSession，纯协议事件视图不依赖图运行时；保留原消费/快照发布边界。初审缺失mapper完整参数校验已通过复用原validateConfig修复，双独立复审PASS、Release全量exit0。

原120秒双源RTP/RTCP H264/AAC→H264 VBR8000–12000kbps、1280×720/30、AAC CBR192kbps→MPEG-TS/RTP实流caller_stop exit0，payload132796预约=释放、0B/0对象。仍停读7.779秒、2749线端完整包缺口、启动静音2.998549秒，内容A/V UNKNOWN；完整门禁FAIL，独立评分75/100。双画面截图有效，全丢失截图237×39无完整画面覆盖，不计视觉通过。19项产物记录后按清单删除，5个媒体PID无残留，原源124427809B保留。详见runtime r129。未改外部FFmpeg、未push/merge/deploy，Windows完整门禁前不进入RKMPP。

修正r128后续计划中“AU完整entry成员”的不必要要求：现parser payload、组装和最终AVPacket均独立拥有数据；原始entry在协议消费后可释放，不需要为生存期引入AU→raw成员索引。需保留实际pending/未完成AU占用及瞬时复制重叠额度、完成AU独立owner/序号、raw消费frontier。下一范围仍是准备期使用同一protocol/clock核心连续消费、独立有界AU保留、首帧probe复用产物、启动窗口选择与epoch发布分离，再事务性交给runtime；仅移动核心尚未解除停读。

### 2026-10-09 r130：连续准备接收已实接，原规格启动静音回归通过

准备与runtime复用同一协议/时钟会话、AU产物及原接收线程，实际连续预算进入最终ledger。首次双审五项FAIL已修复，第二轮两者PASS；首次编译缺头文件已修复并双复审PASS，随后Release全量699目标exit0。

r130b双源原120秒H264/AAC→H264 VBR8000–12000kbps、1280×720/30、AAC CBR192kbps→MPEG-TS/RTP：两源3600帧exit0，逐源等待100/200ms仅为本次命令值。输入/输出RTP线端连续，TS计数及PTS步长正常，离线未再检测此前约3秒启动静音；双格与全黑完整VLC画面有效。caller_stop exit0，payload104992预约=释放且结束归零，encoded差1包不记完全drain。WS峰241356800B/private峰725454848B；内部漂移155..156ns不代表内容同步。

线端无缺包不能单独证明应用消费全部包；本轮无恢复，完整矩阵、内容A/V、可信EOF、C API及Windows→RKMPP仍未闭合，整体门禁维持FAIL，不调整既有75分。实际命令、PID、失败尝试及数据见runtime r130b；未更改外部FFmpeg，未提交/推送/部署。

### 2026-10-09 r131：所选AV源结束后重入，有限双审PASS

原120秒双源RTP H264/AAC→H264 VBR8000–12000kbps、1280×720/30、AAC CBR192kbps→MPEG-TS/RTP；初始AV、纯视频及重入AV均完整120秒/3600帧exit0。双格→右黑左画面→全黑→仅右格恢复有完整VLC证据，generation2持续工作；27.395556秒失源静音后非静音恢复约118.453秒。输出同SSRC、RTP/TS与PTS连续，caller_stop exit0、payload162868预约=释放归零；encoded差1包及AAC关闭时残留2帧，不计完全drain。两名独立复核均限定PASS：匹配BYE及失活/purge后重入；未覆盖快速SSRC切换、内容A/V、完整矩阵、可信EOF、C API或RKMPP，整体75/FAIL不变。

22项产物记录后逐项清理，六媒体PID无残留，原源124427809B保留。下一项为旧会话尚未失效时快速发送器替换的真实失败取证；不以本轮成功代替该边界，不盲删身份校验。命令和细项见runtime r131。无外部FFmpeg修复，无提交/推送/部署。

### 2026-10-09 r132/r133：快速SSRC替换回归已修复并有限双审PASS

r132原规格快速切换68.812ms复现准备期身份限制沿用runtime导致整图Unsupported/exit1；不是外部FFmpeg问题。3文件修复由planner下发既有生命周期模式，runtime仅允许授权SSRC代际恢复，准备期SSRC及全时PT/媒体契约保持严格；双独立源码PASS、Release全量exit0。

r133原120秒源、双路RTP H264/AAC→H264 VBR8000–12000kbps、1280×720/30、AAC CBR192kbps→MPEG-TS/RTP：切换58.694ms更快，代2恢复，纯视频及新AV各完整120秒exit0，输出RTP/TS/PTS连续。切换静音5.007506秒后非静音114.493106秒；仅VLC录制60秒抽帧证明双格恢复，现场恢复截图未取得，不能记现场视觉门禁通过。caller_stop exit0、payload115446预约=释放归零；encoded差1包/AAC关闭残留2帧，非完全drain。两独立运行复核均限定PASS，不能称无缝恢复。

r132失败13件、r133实际20件产物已记录后清理，各六PID无残留，原源124427809B保留。整体75/FAIL保持：内容A/V、现场恢复画面、完整矩阵、可信EOF、C API和Windows后RKMPP未闭合。下一项在既有完整矩阵验证中补现场恢复证据，继续优先Windows；未提交、推送、部署或修改外部FFmpeg。

### 2026-10-09 r134：三源HEVC CBR门禁FAIL；C API获授权实施

当前Release三源V/V/AV、原120秒、HEVC CBR8Mbps/720p30、AAC192kbps→MPEG-TS/RTP于输出scheduler约334ms晚到时报缺source coordinator，CLIexit1；三源均完整3600帧exit0，重入未执行、截图被遮挡。连续输出错误复用显示/源恢复策略，不能补source coordinator、调阈值或丢HEVC参考包规避。需typed encoded-AU调度合同复用现emit=dispatch-transportLead及下游真实deadline，查实际迟到来源后原规格复验。12项临时产物清理完成（两占用文件重试后成功），六PID无残留，原源保留。

用户已明确批准独立C合屏入口与音频协议事实，以及后续最小audio_bitrate_bps目标字段（正值、精确kbps、无默认，不称CBR）。Beta接线实施中，旧ABI及全部原验收门禁保持。整体75/FAIL不变，Windows完整通过前不进入RKMPP；无外部FFmpeg改动、无提交/推送/部署。


## 停止证据更正：rk422—rk426

这些轮次的SIGTERM由RealtimeStopSignal桥接到requestStop，并非直接强杀；exit1由main中的失败outcome返回。此前“清理退出”仅说明发起时机，不能解释为正常停止成功。RunController的continuous drain、runtime completion或fixed output finish仍可能失败。既有出画/恢复证据保留，停止门禁保持未通过；历史日志已按清理要求删除，不能补造具体错误。rk427按原四路完整源重现，保留停止阶段完整stderr、end_reason、final report及资源释放，以实际错误驱动修复。
