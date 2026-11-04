# RTP 准备与持续接收交接设计

## 状态与目标（2026-10-05，r114）

**调查与方向评审完成；完整实现设计尚未通过，功能修复未实施。** r113已证实准备预算满后停读8.534秒，应用缺3557包但启动期对应线端完整。保持原2–4路异步RTP/RTCP、固定布局、选定音源、失源补黑/静音及恢复目标；不新增公共参数，不改外部FFmpeg，不以准备失败或降规格替代目标。

## 当前边界

| 当前代码 | 已证约束 |
|---|---|
| CompositionInputPreparer | 每源jthread并发begin；收取所有future期间早到源capture仍接收，直到预算满。 |
| CompositionPreparer::prepareCandidate | 共享设备、输出格式、全源几何/颜色决定各源decoder/filter目标，不能随意提前打开另一套。 |
| CompositionSourcePreparer | decoder readback和首帧探测借用RawRtpProbeLease，候选失败可重试，尚需新数据时重新取lease。 |
| RawRtpProbeLease | 固定deque下标及借用span；现有owner只固定整个append-only队列，不支持退休元素。 |
| RawRtpPreparedByteBudget | observedBytes为累计探测证据，retainedBytes为当前预约，两者不可互相代替；seal前还检查活跃lease。 |
| RawRtpInputNode | 已有RTCP tracker、时钟观察、reorder/depacketizer；后续startup覆盖/随机访问准入必须复用。 |

单独把SourcePreparer并行化只缩短等待，不能消除合法错峰。全部事实和全图ledger完成前启动生产源DAG会绕过现有准入，且下游有限背压仍会停住；当前不选“提前运行局部DAG”作为最小修复。

## 新确认的容量记账问题

MediaUdpSocket::receive在Windows和Linux均构造vector(maximumDatagramBytes)，收包后resize实际长度；move到prepared仍保留原capacity。prepared retain/release仅按size，未找到覆盖剩余capacity的归账。r113队列4173、maximumDatagram65507，对应vector元素容量下界273356631B；这是源码推导，不是精确heap/RSS采样，不含allocator/deque描述符。

因此不能把当前5MB载荷预算称为真实驻留硬界。优先采用单receiver复用scratch、完整长度验证后保留实际长度存储；若使用vector，仍按实际capacity计账，不能依赖shrink_to_fit。scratch、接收临时对象、保留容量和描述符必须分项形成planner产品，不能各自再分配一份5MB。

prepared Linux旧recvfrom(flags=0)不检测截断；runtime recvmmsg的MSG_TRUNC保护不是同一路径。新接收接口必须Windows保留WSAEMSGSIZE失败、Linux显式检查MSG_TRUNC或完整长度；两平台共用存储与所有权，只有socket API adapter不同。

## 成熟依据与等价范围

- [GstBuffer](https://gstreamer.freedesktop.org/documentation/gstreamer/gstbuffer.html)：引用固定不可变数据，最后引用释放后回收；适用于probe与live共享同一份载荷，不复制媒体来维持视图。
- [rtpjitterbuffer](https://gstreamer.freedesktop.org/documentation/rtpmanager/rtpjitterbuffer.html)：有界退休及带序号/原因的事件。可复用原则，不能照搬默认容量/时间，也不能把任意编码包裁剪当作可解码窗口。
- [preroll](https://gstreamer.freedesktop.org/documentation/additional/design/preroll.html)：准备状态与播放状态分开；仅作为生命周期参考，不能证明本项目已具备连续接收/准入算法。

## 选定方向与硬性合同

1. **能力证据与发布数据分离。** probe只证明特定配置的decoder/filter能力；首次发布消费当前live准入窗口，不先播旧prefix再触发缺口重锁。
2. **受预算约束的稳定引用。** lease固定条目身份及不可变载荷；队列退休只释放自己的引用，最后引用才归还唯一预约。不能仍以可移动deque下标表示同一个包。
3. **有限探测与持续接收分阶段。** 累计探测证据封闭后才转换阶段；不得清零observedBytes或提前runtimeActive规避校验。后续候选探测需要新数据时，以单调frontier识别进展，不能用可能恒定的队列长度。
4. **持续读socket。** 硬边界由planner推导；保留数据退休必须有明确原因、范围、SSRC/epoch和观察时间。不得把本机主动退休伪装成网络gap，也不能持续丢弃全部incoming算修复。
5. **协议事实连续。** SR/CNAME/BYE、SSRC和配置变化按顺序进入现有tracker；不能仅保存最新SR并拼接旧CNAME。权威配置变化须按既有合同重新验证。
6. **单owner接管。** 接收不因准备等待或replay排空停顿；交接消费frontier、未完成接收/部分AU、协议状态和候选所有权，不允许两个receiver同时读socket。
7. **复用准入。** RawRtpInputNode、MediaAvStartupCoverageIndex、MediaAvStartupWindowSelector负责原随机访问和A/V覆盖；准备缓存不另建媒体算法。未知事实在图构建前失败，正常异步等待不能被改成失败语义。

## 实施前必须关闭的三个决策

| 决策 | 必须交付的具体证据 |
|---|---|
| 真实容量及租约 | 复用MediaPreparationStorageBudget/Lease或现有ByteReservationCounter的唯一预约；明确scratch/descriptor/pin/live重叠峰值、每项来源、申请与释放时刻；不能凭平均码率分预算。 |
| 协议/配置状态 | 明确现有tracker何时创建、如何在原策略下持续更新、以什么frontier交给runtime；列明配置/SSRC/BYE失效处理。 |
| 首次live准入与交接 | 给出source未发布→可准入→发布的具体状态转换、部分AU退休和仍在完成的receive归属；证明无旧prefix闪播、无停读空档和无双读。 |

A/B独立调查一致认可方向，**上述三个决策未关闭，因此不标完整设计PASS，也不先实现rolling策略。**

## 验证顺序

先针对真实容量和所有权边界做双审及Release全量构建，再使用原120秒源、原双路VBR/约11秒启动间隔，input/output被动抓包及CLI/FFmpeg/VLC链路复验。需要证明持续接收、显式退休和新准入，而非仅不再出现错误；保留启动音频内容/静音证据、CPU/内存/漂移、停止回收。随后完整重入、2–4路及编码RC矩阵、共享单路回归，Windows门禁通过后才RKMPP。每轮记录后按精确清单清理。

## 独立复核

A/B均明确文档准确性PASS、完整实现设计未通过。下一最小合同先列每receiver的scratch、每份载荷共享唯一预约、描述符额度及重叠峰值；预算不足不得覆盖pin或静默停止接收。此顺序不授权先实现缺少协议/准入语义的rolling队列。

## r115：存储所有权调用链核验

本轮仍为设计闭合，未实施容量修复，也未重新构建或运行媒体验收。新增源码证据改变了修复边界：

| 边界 | 当前行为与必须调整的合同 |
|---|---|
| 初始视频信令探测 | `MediaRawRtpInputPreparer::prepare` 收包只 `observe(size)`，本地 `buffered` 已保留载荷，但到 `createRawRtp` 才统一 retain；同时音频 capture 已使用共享预算。预约必须从存储分配前开始，随报文移交，不能在包装为 prepared 时再预约。 |
| prepared 出队 | `MediaRawRtpPreparedInputBuffer::receive` 在返回报文前 release；`RawRtpInputNode::process` 又从返回包装中 move 出 `datagram`，随后才解析。租约必须跟随底层报文存储，不能只放在队列或 `MediaPreparedRawRtpDatagram` 包装中。 |
| replay/live 重叠 | `configureRuntimeIngress` 在 prepared 队列未清空时创建 runtime arena；当前 storage planner 只计 ingress arena 字节。不能假设两份存储生命周期互斥，必须计入交接峰值。 |
| 解析存储 | `MediaRtpPacketParser::parse` 复制 payload、CSRC 和 extension；scratch/原报文预约不能被称作整个准备过程的内存硬界。解析/reorder 现有存储也要在总账中明确归属，不能据此声称 heap/RSS 已完全有界。 |

### 现有 ingress pool 的复用边界

`MediaRtpIngressStorage::create` 强制 `byteCapacity = descriptorCapacity * maximumDatagramBytes`；`seal` 持有整批租约，租约未释放时 `reset` 失败。`MediaRtpIngressPlan::create` 还依赖准备后的 observation 与平台 capability；Windows adapter 为 RTP/RTCP 各保留一个 pending operation。

因此它可继续承担有界接收/批交接，不能原样充当可长期 pin、逐条退休的准备缓存。按单 arena 的 5000000B 和 65507B 上界计算，最多 floor(5000000/65507)=76 个槽（平台 completion 上限还可能进一步收窄）；直接替换原按实际报文长度保存的队列会改变准备窗口。也不能伪造 observation 来提前构造 runtime plan。

### 下一实现合同的具体落点

- 网络 adapter：接收调用方提供的有界 span，仅返回通道/有效长度；socket 层不分配长期载荷，也不依赖 raw probe budget。scratch 由明确的独占 receiver owner 持有，并纳入 planner 存储产品。
- 载荷 owner：复用 `MediaPreparationStorageBudget/Lease`，先预约再分配；保存不可变数据与唯一租约。移动报文、probe 引用、出队移交均不得提前释放或重复预约，最终引用销毁才释放。若保留 vector 必须计 capacity；不能把非强制 shrink 当作容量保证。
- byte budget：wire observation 仍按实际收到的长度累计，与物理存储预约分开；不能将原 size 调用全部机械替换为 capacity。原 `retain/release` 队列记账应随统一 owner 接入而移除，避免两套所有权。
- 描述符：必须有 planner 给出的数量/字节硬界以及分配前预约，不能继续无界 deque，也不能只计算元素 sizeof 而忽略其 backing。优先复用现有精确容量组件；不得以平均包长或样例包长推导数量。
- 交接：明确 scratch 释放时刻、prepared pin 保留量、runtime arena 创建时刻及重叠峰值；资源 ledger 不得只报 runtime arena。

平台完整性依据：[Microsoft recvfrom](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-recvfrom) 明确 UDP 截断报 WSAEMSGSIZE；[Linux recv(2)](https://man7.org/linux/man-pages/man2/recvmsg.2.html) 明确 MSG_TRUNC 可返回原始 datagram 长度。新接口须在建立有效 span 前拒绝超界，两平台保持相同失败语义。

以上明确了接口责任与不可采用的捷径；descriptor/pin 的具体容量产品以及持续接收的协议/准入状态仍须闭合。不能把这份调用链核验标为功能修复或完整设计 PASS。

本轮新独立审查者 prepared_storage_contract_a 已复核上述初始预约、出队释放、runtime 重叠及 ingress pool 限制，结论为方向可实施、完整设计 FAIL。尚缺描述符/总容量产品、scratch 交接时点、载荷 owner 全调用链及异常回收的具体实现合同；未形成代码双审通过结论。

## r116：实际分配预约基础组件（接入未完成）

新增 `runtime/resource/MediaPreparationMemoryResource.{h,cpp}`，通过标准 [memory_resource](https://eel.is/c++draft/mem.res.class) 接口接收实际分配字节/对齐，复用既有 `MediaPreparationStorageBudget/Lease`。每块先预约 header+padding+请求字节，再执行 aligned new；header 保活 resource 和唯一 lease；deallocate 先销毁/释放块，再归还预算，最后释放 resource owner。该寿命覆盖 payload 析构后仍存在的 shared_ptr 弱引用控制块。

这是一项基础组件实现，尚未接入 prepared 队列、载荷、scratch 或 ledger，因此既有容量漏洞和 r113 停读缺口均未修复。预算边界是实际 C++ 分配请求，不包含系统 allocator 隐藏元数据或 RSS；resource/budget 自身 bootstrap 对象与控制块也需在接入产品中明确归属，不能通过自身 resource 递归创建。

选择无 pool 的 adapter，以避免标准 pool chunk 保留和增长策略影响当前存活字节回收。prepared 的 deque/map、载荷、控制块与 probe 引用表接入后，可按实际 backing 预约，不猜测 STL 布局。所有空容器必须另持 resource owner；禁止默认 PMR 复制切换到默认 resource，异资源移动也不能当作无分配移交。分配异常必须在现有 Result 边界转换，不能逃出 capture 的 noexcept 线程。

描述符数量产品可由总存储预算除以每个存活报文必须占有的固定对象字节，形成保守数量上界；这不保证该数量均能分配，也不能替代实际字节门禁。总预算、scratch、bootstrap 成本、runtime arena 重叠仍需 planner 明确绑定。下一步接入统一 allocation owner 并移除旧队列级 retain/release，随后继续稳定 pin 与 live 协议交接。

验证状态：两个未参与实现的独立审查者 prepared_storage_contract_a、prepared_resource_review_b 均给出基础组件静态源码 PASS。Release clean-first/all-target 全量重建 exit0，日志确认新 cpp 已编译；realtime CLI 为 2026-10-05 20:38:42、5615616B，local CLI 仅随全量构建生成，未运行。未开展实流测试，组件尚未接入。临时构建日志 out/r116-build.log 在记录结果后按单文件清单清理。

## 2026-10-08 r117：接收缓冲改动与失败复验归档

共享 receiveInto(span)、单receiver复用scratch及共同交接前释放已实施；Linux拒绝截断，Windows保留原错误传播。两个独立源码审查PASS，Release全量重建exit0。原120秒双路RTP/RTCP→H264 VBR8000–12000kbps、1280×720/30、AAC CBR192kbps→MPEG-TS/RTP复验，实际首包错峰13.336秒。303315包/drop0、输入输出RTP序号与TS CC连续，4116视频包PTS步3000、5909音频包步2089/2090。

仍FAIL：准备预算满后停读10.089秒，应用缺4867包在线端全部存在；启动静音0.817982秒。离线帧支持双格→仅左→黑场，VLC桌面截图失败，内容A/V同步UNKNOWN。会话恢复后发现CLI延续到22:36发生scheduled datagram deadline失败；未取得最后进程退出码，不能标正常停止/长稳PASS。最终payload1485487预留=释放，0B/0对象。PMR总预算与消费者租约尚未接入；完整矩阵、恢复、可信终止、C API、Windows→RKMPP门禁保持未完成。既有75/100不重评分。

实际命令、PID、指标及证据局限已写入docs/realtime-video-composition-wiring-runtime.md的r117节。23件临时产物已按清单清除，原120秒源保留，五个PID均无残留；没有推送/合并/部署或外部FFmpeg修改。下一实现仍为统一准备存储owner/预算及持续live接收交接，不以晚期失败替换主任务。

## 2026-10-08 r118：PMR 接线编译通过，容量契约审查未通过

已接入 prepared 视频初始队列、音频队列、RTCP 筛选队列、transport scratch 与报文载荷；出队/丢弃不再手工归还载荷预算，由实际 allocation 析构归还。报文禁止复制，各入口显式传递 resource，prepared budget 首成员保证最后析构；capture/prepare 的分配异常转换为现有 Result 失败。manual 输入路径显式使用标准堆 resource，没有新增公共参数或外部依赖修复。

Release clean-first/all-target 全量构建成功（configure/build exit0）；realtime CLI 为 2026-10-08 12:13:20、5625856B。git diff --check 通过，本轮八个改动文件已统一 UTF-8 无 BOM/CRLF。未运行 local CLI；本轮未启动真实流或 RKMPP，不产生新的媒体验收结论。

独立审查 prepared_storage_ledger_review 给出有限接线 PASS，pmr_integration_review_b 给出 P1 FAIL，因此整体审查未通过：当前把原 5000000B wire probe 上限同时用作 PMR allocation 上限，scratch、payload header/padding、deque/map 会提前消耗它，不能保证保留原探测窗口。capture 的 maximumDatagramBytes 剩余空间检查也不再涵盖 allocation 全开销。此版本为未完成 WIP，不能用构建成功替代该阻塞项，也不能标记启动缺口已修复。

最小后续步骤：收流前准备可冻结容量的 descriptor 存储并持有实际 allocation/readback；wire 上限与 allocation 产品分离；payload 改为确定分配形状并由 resource 暴露 checked footprint，避免 planner 猜 STL 私有布局。descriptor 数量须由所有入队路径共同满足的协议/字节下界证明，不能使用平均包长；RTCP 筛选和 epoch/replay 生命周期一并审查。继续复用已有容量组件及标准容器，不自行发明滚动队列算法。依据：[C++ deque](https://eel.is/c++draft/deque)、[memory_resource](https://eel.is/c++draft/mem.res.private)、[GStreamer bufferpool](https://gstreamer.freedesktop.org/documentation/additional/design/bufferpool.html)。这些资料支持接口与所有权约束，不为本仓库尚未实现的容量产品背书。

最终 ledger 亦未闭合：每源 AV 共用一个 budget，须只计一次，再加 video/audio runtime arena 的重叠；只修两个中间 storage planner 调用点不会进入最终合屏 admission。直接追加无节点归属的 fixedStorage 又会污染 selectedNodes 动态输出后缀预算。需要保留 typed owner 归属及筛选。bootstrap、parser/observer、allocator 隐藏成本仍不在当前 PMR 边界内。

既有完整合屏 FAIL/75 分保持；Windows 原规格实流、内容 A/V、恢复/可信停止、完整矩阵及 RKMPP 门禁均未宣称通过。保留全部既有未提交改动，无 commit/push/merge/deploy。临时产物仅 out/r118-build.log；记录成功结果后按该单文件清理，本轮未启动媒体进程。

## 2026-10-08 r119/r120：容量语义 P1 源码关闭，真实合屏仍 FAIL

新增内部 MediaRawRtpPreparationStoragePlan/Planner；wire 探测与实际 allocation 上限分离。MediaOwnedByteStorage 精确单次分配，footprint 与实际 allocator 共用布局；复用 MediaFixedCapacityStorage 和 std::remove_if，预分配描述符、原地筛选RTCP，消除deque增长/双队列并存猜测。空报文在transport拒绝，N≤wireBytes为保守数量事实；每流数组在收流前分配。覆盖共享wire载荷/header及每流scratch、一个未observe载荷；未新增公共参数。budget窗口仍用wire，未把约605MB allocation上限误传为探测/ingress payload上限。

r119_storage_review_a/b 对冻结源码均PASS；Release clean-first/all-target exit0，realtime5627904B（13:28:20）。r119因调度导致第二源晚于30秒open而失败，原规格统一调度r120重跑。两源完整120秒exit0、VLC可见双路→左→黑、显式CallerStop exit0且payload102129预约=释放、0B/0对象。

完整验收仍FAIL：准备停读7.633秒，缺2611包在线端全部存在；启动静音3.254467秒，内容A/V UNKNOWN。306351包/drop0，输出TS CC及PTS连续只证明局部时间线。进程188点覆盖准备期，private峰1072721920B、WS峰228687872B；固定描述符过于保守，不能称内存优化。原source124427809B保留，全部本轮进程/临时文件已清理；最终归档命令一度解析失败，清理后依据完整已返回输出立即补记，详见runtime记录。无push/merge/deploy/RKMPP。

下一步仍须完成每源共享prepared allocation与runtime arena重叠的最终ledger、持续接收/稳定pin退休和协议交接，再以原规格复验。C API、完整矩阵、内容同步、恢复/可信停止及Windows→RKMPP门禁未完成；75/100保持。

实现边界：分配计费不含allocator隐藏元数据/RSS、bootstrap/control block、parser/observer；这些仍须独立产品。N=wireBytes在本机sizeof(descriptor)=40时，AV两个descriptor数组请求400000000B，VideoOnly请求200000000B；未根据样例平均包长压缩。相同布局机制跨平台复用，Linux尚未编译/运行。最终ledger不可把中间source ledger当作final admission，也不可无节点归属地追加fixedStorage而污染动态输出selectedNodes计费。依据标准memory_resource、typed polymorphic_allocator、标准remove_if及既有固定存储；不把标准容器正确性当作整体媒体状态机证明。

## 2026-10-08 r121：prepared allocation 接入最终账本（实流待验证）

共享预算导出实际 allocation identity、不可变额度、streamCount 与 authority；AV 必须逐项一致，prepared cap 每源只计一次，runtime ingress arena 在并存期另计。single 两个 final wrapper 与 composition source→resources→admission 均接入；fixedStorage 记录持有节点组，selectedNodes 全选计入、未选跳过、部分选择失败，重复身份拒绝。身份仅保留预算计数对象，不保留输入/socket。

r121_ledger_trace 与 r121_ledger_review_b 两位未参与实现的审查者均 PASS。初次 Release clean-first/all-target 失败：MediaRealtimeGraphResourceLedgerPlanner.cpp:411 使用 std::none_of 缺少 <algorithm>（C2039/C3861）；已补显式 include，两者重新审查均 PASS，正在再次全量构建。未运行 local CLI 或 RKMPP，未修改外部 FFmpeg。

审查边界：按实际分配身份唯一计费、重叠资源相加，沿用 memory_resource 的分配计费原则；不是新的媒体算法。generic compile(planning, selectedNodes) 的中间 payload allowance 仍来自全图 maximum，实际初始 boundProducerResidence 分区会重建并独立提取 fixedStorage；动态编码采用 fresh ledger，协议后缀 reference-only。不可把中间产品称为独立冻结的分区预算。既有停读、保守描述符内存、内容 A/V 和完整合屏 FAIL/75 分保持；allocator 隐藏成本、bootstrap、parser/observer 不在此额度内。

## 2026-10-08 r121 进度

prepared allocation 已按实际共享身份进入 single/composition 最终 ledger，AV只计一次并与runtime arena相加，节点归属避免动态输出后缀重复计费。双独立源码复审PASS；补缺失<algorithm>后Release全量重建exit0。原120秒双路RTP/RTCP→H264 VBR8000–12000kbps、1280×720/30、AAC CBR192kbps→MPEG-TS/RTP已复验，CLI caller_stop exit0、payload93426预约=释放/0B/0对象。

整体仍FAIL：准备停读7.705秒，缺2691包在线端全部存在，启动静音3.138231秒，内容A/V UNKNOWN；private峰1077166080B。输出RTP/TS CC/PTS连续仅为局部证据。完整矩阵、恢复、可信EOF、C API与Windows→RKMPP门禁未通过，75/100保持。详见runtime r121记录。22项临时文件已按清单清除（两项短暂占用后重试成功），全部本轮进程无残留，原源124427809B保留；无推送/合并/部署。

## r121 后续最小实现边界：稳定条目与快照

两位独立只读调查一致：可推进内部稳定所有权基础，但持续接收/滚动退休设计仍未闭合。当前probe以队列下标返回裸span，安全性依赖append-only；不能直接pop/覆盖。最小片使用不可变entry owner、lease冻结引用表、单调ingress ordinal；snapshot数量与wait进度分离，过滤后的ordinal允许空洞，不用ordinal减begin推导数组位置。

当前budget的m_probeActive保证每源AV合计最多一个lease；SourcePreparer内层作用域释放后才wait/重试。因此分配契约需要队列新元素backing、最多N个entry/control block、最多一张N项snapshot表及现有scratch/in-flight项，N仍由累计非空wire上界推导。不得沿用r121 cap或按sizeof(shared_ptr)猜控制块。使用同allocator/type实际allocation readback或已有已验证固定布局组件；acquire先取得独占再分配，异常RAII撤销；release先销毁表及引用，最后释放probe标志，避免瞬时双表。

此片保留seal/replay约束，不开放rolling。runtime RTCP权威链仍是RawRtpInputNode::processRtcp→CompoundParser→SenderReportTracker→invalidation/evidence；first-frame probe并未完成该tracker状态交接。AU/startup coverage向ingress ordinal的映射、SR/CNAME/BYE/SSRC有序状态移交、socket唯一receiver接管和资源退休边界尚须明确。不能只保存最新SR、任意按字节丢编码包，或将持续丢弃incoming称为修复。依据既有[GstBuffer引用规则](https://gstreamer.freedesktop.org/documentation/gstreamer/gstbuffer.html)、[rtpjitterbuffer](https://gstreamer.freedesktop.org/documentation/rtpmanager/rtpjitterbuffer.html)及[RFC3550](https://www.rfc-editor.org/rfc/rfc3550)；这里只确认适用原则，不宣称整体算法等价。

## 2026-10-08 r122：稳定条目与冻结快照实施

根据r121后续边界实施内部所有权基础：队列保存shared_ptr<const entry>，probe冻结PMR引用表；queue移除只释放自身引用，原始arrival/ordinal不可变。replay另存实际交付时间，RawRtpInputNode持有entry直到解析结束。每源AV仍只有一个probe，acquire先取得pin再分配，异常RAII归还；release先销毁引用表再归还pin。等待数据使用单调ingressFrontier，RTCP过滤与epoch清空不会倒退或重用ordinal。

分配适配器以同allocator/type的allocate_shared实际请求回读控制块+对象bytes/alignment，真实分配验证一致；不假设sizeof(entry)等于分配量。planner加入最多N个entry、每stream N个queue引用、一张N项snapshot及原payload/scratch/inflight；N保持累计非空wire上界，不按平均包长收窄。标准允许allocator rebound到未指定类型，单分配为建议而非保证，因此不支持的readback在capture前失败。依据：[C++ shared create](https://eel.is/c++draft/util.smartptr.shared.create)、[GstBuffer共享引用](https://gstreamer.freedesktop.org/documentation/gstreamer/gstbuffer.html)。

仍保留seal/replay与累计wire预算语义，未实施rolling/RTCP状态移交，不宣称停读已修复。没有公共CLI/C API变化。原r121实流是停读失败基线；仓库真实CLI规则优先于通用技能的单元测试流程，不新增永久测试设施。本片正在双独立审查与Release全量构建，尚无通过结论。

## 2026-10-08 r122 进度

稳定immutable entry、独占PMR snapshot及单调ingressFrontier已接入，replay时间独立保存；同allocator/type实际控制块分配回读进入planner容量，释放引用表先于归还probe pin。两个独立源码审查PASS，Release全量重建exit0（realtime5652480B/14:14:11）。原120秒双路RTP/RTCP→H264 VBR8000–12000kbps、1280×720/30、AAC CBR192kbps→MPEG-TS/RTP复验：caller_stop exit0，payload95082预约=释放/0B/0对象；private峰714657792B，较r121减少362508288B，但保守allocation cap增大。

完整仍FAIL：停读7.689秒、2691缺包线端完整、启动静音3.115102秒、内容A/V UNKNOWN。稳定引用未实现rolling和持续接收；完整矩阵、恢复、可信EOF、C API与Windows→RKMPP门禁未通过，75分保持。详细命令/进程/时间线/边界见runtime r122。20项临时文件已按清单删除，进程无残留，原源124427809B保留；无push/merge/deploy。

### r122 后续协议与AU归属调查

独立调查确认preflight已有RTCP liveness政策事实，但MediaRawRtpProbePlan仅带compound解析policy。RawRtpInputNode当前runtime重新创建MediaRtcpSenderReportTracker；应先抽出planner共用typed clock policy，再提取既有单owner输入会话状态，复用observeMedia→reorder连续性→RTCP observe→evidence→schedule/invalidation。仅移动tracker指针不够：pending发布标志、BYE事件、schedule、waiting flags和reorder状态独立；prepared replay还将arrival映射到runtimeOrigin，不能与最新tracker原时间轴混用。未定义有序事件和replay边界前不得删除旧RTCP。

另一独立调查确认MediaRtpPacketProvenance.ingressSequence是runtime输出事件序号，不是wire序号/新ordinal。MediaRtpPacket、MediaRtpAccessUnit与startup store目前未保留原始datagram成员；AAC一包多AU/多包一AU使单一完成ordinal无法证明安全退休。下一产品需在共享parser/reorder/depacketizer传播采集身份及所有权，AU完成/丢弃/重置显式处理归属；CoverageIndex/WindowSelector保持现有时间覆盖算法，按选中AU身份回查所有权，不另建合屏退休算法。连续区间仅能作保守包络，不能冒充精确成员。引用表、未完成AU及有序事件必须规划容量。此调查给出下一实现依赖，不代表rolling设计已通过。

## 2026-10-08 r123：提取共享输入时钟会话

r122实流仍停读7.689秒，下一接管需要保留tracker、schedule及有序输出状态，不能只迁移最新SR。本片先把生产RawRtpInputNode现有RTCP parser→tracker、evidence→schedule、continuity generation更新收进单owner可移动MediaRtpInputClockSession，节点直接使用；原planner options显式形成typed config，无默认或新公共参数。节点继续负责DAG事件排队、序号、waiting/keyframe与媒体清理，保持原触发顺序及Wait/Invalidate/Fail策略。没有增加线程、缓存、队列或复制媒体。

依据：[RFC3550 RTP/RTCP source状态](https://www.rfc-editor.org/rfc/rfc3550)、[GStreamer rtpbin会话组合](https://gstreamer.freedesktop.org/documentation/rtpmanager/rtpbin.html)。复用现有算法，只重整所有权；不宣称现有tracker实现了RFC全部成员算法。尚不把组件用于prepared收流，不改变旧arrival/replay时间轴；输入会话事件frontier、AU归属、准备预算/RTCP状态移交仍须后续接线。本片需双独立审查、Release全量及原规格realtime复验，尚未通过。

### r123 后续准备会话依赖核验

已实读CompositionPreparer::planSourceFacts与SourcePreflight::seal调用链：每源signaling/frameRate完成后，既有planRtpInputClock和planRawRtp即可推导PT/clockRate、depacketizer、AU envelope与输入政策；视频AU上界来自SPS profile/tier/level及packetization，不依赖decoder/画布/output。当前应用层等待所有capture后才遍历是组织顺序，不是这些输入事实的固有依赖。

不能因此提前使用最终ingress产品：PreparedRtpIngressPlanner要求sealed observation、实际socket、platform capability与wire事实，SourcePreflight::seal目前先停止capture再生成。后续必须显式冻结规划观察快照，保留连续接收owner，不能直接删除sealed校验。bootstrap→final reorder参数状态兼容、未完成AU/RTCP事件预算、快照冻结后能力产品有效边界与arrival/replay时间轴仍须闭合；不得以bootstrap reorder偷换final ingress，不启动未准入DAG。最终decoder/filter/output/全图准入仍按原顺序完成。

## 2026-10-08 r123 进度

已提取可移动单owner MediaRtpInputClockSession，生产RawRtpInputNode复用同一RTCP parser/tracker/schedule；planner必填配置、generation/失效策略/事件顺序保持。双独立源码PASS，Release全量exit0（realtime5654528B/14:31:40）。原120秒双路RTP/RTCP→H264 VBR8000–12000kbps、1280×720/30、AAC CBR192kbps→MPEG-TS/RTP复验：caller_stop exit0、payload93415预约=释放/0B/0对象；private峰717029376B。

整体仍FAIL：停读7.717秒、2710缺包线端完整、启动静音3.115941秒，内容A/V和VLC完整画面证据不足；完整矩阵、恢复、可信EOF、C API、Windows→RKMPP未通过，保持75分。下一步输入事实可按源提前规划，但需显式观察快照冻结、共享协议处理和AU归属及时间轴交接，不能提前运行未准入DAG或直接删sealed校验。详见准备交接设计与runtime r123。18项临时文件已清理，全部本轮进程无残留，原源124427809B保留，无push/merge/deploy。

## 2026-10-08 r124：每源事实提前规划（验证中）

将原应用层planSourceFacts等价迁入MediaRealtimeCompositionSourceFactsPlanner，每源worker完成信令和帧率观察后立即生成request/raw/color，主事务等待全部worker后仅移动结果。请求与输入引用存活至join；任一失败仍取消并join全部worker。没有提前创建DAG或改变seal、预算、reorder策略。参照既有RFC3550/GStreamer每源会话职责划分，本片只调整既有策略归属与执行时点，不构成持续接收修复。

后续观察快照必须保留累计collector及跨冻结点间隔；当前maximumReorderDelay取观测最大间隔，早期快照不能冒充最终产品。继续保留stop/join/seal门禁，待候选产品有效性、协议状态与AU归属闭合后再接管。

### r124 清理与r125诊断

r124已将命令、失败、指标和线端证据入档；按精确清单删除12项本轮媒体/日志/抓包/分析文件及r124-build.log，共13项，五个PID均无残留；原120秒源保留。源码迁移通过双审和构建，但实流FAIL，不能作为成功提交。

r125仅在RtpClockGroupNode记录每个候选NTP/arrival/SSRC/generation/node及被原路径吞掉的具体错误，不改变清空策略、阈值或状态机。目标是核实历史回放arrival压平导致配对偏差的候选根因，避免靠调参推测。待双审、全量构建和同规格实流。

### r125 时间轴调查边界

准备历史SR与live接收必须共享不可变原观察时间，媒体source time→output running time另由现有epoch binder/scheduler映射。不能用runtimeOrigin+delta局部替换：不等待会产生未来证据，等待则重复准备延迟且live切换可能倒退；直接原arrival也不能把过期SR冒充新鲜。最小行为闭环仍需要连续共享协议会话、已处理ordinal、有序clock/invalidation/AU交接、现有coverage selector的合法启动窗口及有限存储准入。依据：[GStreamer同步设计](https://gstreamer.freedesktop.org/documentation/additional/design/synchronisation.html)、[延迟设计](https://gstreamer.freedesktop.org/documentation/additional/design/latency.html)。当前仅调查，不宣称方案已实现或停读已修复。

### r124/r125 最终进度（未完成合屏验收）

每源facts提前规划及时钟拒绝诊断均经两名独立审查PASS、Release全量成功。r124首次时钟超时exit1/输出0；r125同规格完整120秒源结束后持续输出、caller_stop exit0且账本归零，但停读7.671秒/2692包线端完整缺口/启动静音3.161043秒仍FAIL，r124超时未复现未判修复。18项r125临时产物已按清单删除，全部本轮PID无残留，原源124427809B保留。详见runtime r124/r125。下一行为修复仍须连续协议会话与真实接收时间、AU归属和启动窗口交接；不扩大外部FFmpeg范围，Windows门禁通过前不进入RKMPP。

## 2026-10-08 r126：共享协议会话实施边界

r124初始clock超时及r125停读7.671秒/2692包缺口为失败驱动。下一步把现RawRtpInputNode的parser→reorder→clock→depacketizer和待处理协议状态迁入同一可移动单owner组件，runtime先接入，随后准备阶段才能复用。图调度、transport、payload reservation、buffer封装与输出队列留在node。必须两阶段消费：会话给出AU分配需求，调用者取得真实额度后才执行depacketizer；背压保留原pending item，不得重复推进。协议事件先于媒体，reset/complete明确通知调用者释放额度。

参考[GStreamer rtpbin](https://gstreamer.freedesktop.org/documentation/rtpmanager/rtpbin.html)的session/jitterbuffer/depay职责组合，严格复用本项目既有算法，无新线程和新容量策略。当前观测最大interarrival作为最终reorder delay不等价于[GStreamer jitterbuffer显式latency](https://gstreamer.freedesktop.org/documentation/rtpmanager/rtpjitterbuffer.html)，因此本片不以早期snapshot启动协议消费；稳定政策依据、连续receiver移交、AU归属/退休、原arrival和live时间轴仍需后续闭合。没有修改外部FFmpeg或新增公共参数。

### r126 进度

共享MediaRtpInputProtocolSession已接入生产RawRtpInputNode，原pending/reorder/depacketizer/clock/recovery状态同owner；两阶段额度仍由node执行，先预约后组包。双独立源码PASS、Release全量exit0。原120秒真实双路链运行至caller_stop exit0、payload94764预约=释放/0B/0对象；停读7.645秒/2676线端完整缺口/启动静音3.114921秒仍FAIL。明确停止时encoded入出差1包，不记完全drain。18项临时文件已记录后删除，5PID无残留，原源保留。新获授权逐源等待预算开始接线，尚未宣称连续接管或Windows门禁通过。

### r127 后续退休边界核验

稳定wait之后仍不能按原始字节时间直接删包。现有MediaAvStartupStreamStore::append/presentationSnapshot、CoverageIndex::build、WindowSelector::select及prefixBefore可复用完整AU的共同覆盖/关键帧选择和明确AU ID退休；selector返回首个合法窗口，不是自动滚动策略。准备期需先具备完整AU的generation、实际presentation/duration/audio sample span、ID→owner对应和planner startup契约。snapshot借用期间store不能修改。

AAC一包多AU用独立AU ID并共享backing，多包视频AU需完整输入归属，未完成assembly与reservation保持session所有权。RTCP不能参与媒体prefix退休，必须按序处理并连同pending事件/状态交接；不能只留最新SR。此为接线依赖核验，未实施退休算法。

### r127 进度：逐源等待预算已接线并实测

用户明确允许逐源input-latency-ms/input_latency_ms并要求可配置。CLI及内部typed契约已接入，合屏每源正值必填；同源AV共用，bootstrap/final/bind/validator保持声明值，旧有限probe模式不在失败后回退。旧C ABI无改，新合屏C载体其余字段仍受原提案确认边界约束。

首次构建发现校验误插helper的编译错误，修复后双独立复审PASS、Release全量exit0。真实CLI缺值/0均立即exit2。原120秒双路RTP/RTCP→H264 VBR8000–12000kbps、1280×720/30、AAC CBR192kbps→MPEG-TS/RTP，节点确认100/200/200ms，caller_stop exit0、payload97545归零。停读7.668秒/2691线端完整缺口/启动静音3.115578秒仍FAIL，未判r124时钟超时修复。18项实流临时产物及4项负例日志均记录后删除，5媒体PID无残留，原源124427809B保留。详见runtime r127。

下一步以稳定wait产品建立不中断接收的观察快照，再接共享protocol session、完成AU的ID/owner与现有coverage/window selector；尚未实施连续退休或会话交接，Windows完整门禁前不进入RKMPP。

## 2026-10-08 r128：输入容量候选与最终核验（修复审查中）

在r127稳定声明等待产品上新增非破坏累计观察快照，同一buffer互斥锁读取原始steady-clock观察、单调frontier、逐流弱owner和真实socket/wire/datagram容量。每源初始探测后形成候选，最终仍经原stop/join/budget seal门禁，由同一planner算法重建并比较身份、累计事实与产品；finite模式仍只在最终规划。候选不证明SSRC/媒体epoch连续，不开启未准入DAG，不构成连续接收修复。

行业对照：[GStreamer rtpbin](https://gstreamer.freedesktop.org/documentation/rtpmanager/rtpbin.html)按session/SSRC独立处理RTP并借RTCP同步；[rtpjitterbuffer](https://gstreamer.freedesktop.org/documentation/rtpmanager/rtpjitterbuffer.html)显式latency限定等待。本轮仅复用本项目已存在的容量规划、原等待/取消与最终准入机制，不声称实现GStreamer完整会话状态机；不增加线程、媒体复制或无界队列，快照仅为值事实和weak owner。

首次双独立审查均FAIL：视频探测完成不保证音频已获得完整观察，立即生成音频候选会在原open deadline前拒绝合法迟到音频。正在修复为原deadline/cancellation内等待明确的观察就绪状态，协议/容量错误仍立即传播，不吞泛化InvalidArgument、不增加默认时间。尚未构建或运行r128实流，不能标记通过。

### r128 修复后双审与验证安排

两名未参与实现的独立审查者对冻结11文件均明确PASS：精确WouldBlock等待、原deadline/cancel、同锁快照、weak owner与累计事实单调、最终sealed门禁均成立。保留风险：AV另一侧budget.fail没有跨流通知，若本侧无包且无取消，最多到原deadline才报告共享错误；worker/外部取消可唤醒。未承诺全局即时错误传播。

Release正在全量重建。先以真实CLI和两个视频源、暂不发送所选音频，验证不会在视频探测就绪时提前拒绝，并在原30秒open期限失败；这是负例补充，不是降低规格后的成功验收。随后执行原120秒完整双源链，命令显式源0=100ms/源1=200ms，仍检查停读、线端缺包、PTS、资源及all-loss行为。

### 2026-10-08 r128 进度：输入容量候选与最终核验

非破坏累计快照、逐流弱owner、固定wait容量候选已实接begin→seal，保留stop/join/budget seal；最终检查owner/frontier/五项累计观察不倒退及容量产品一致。初审发现音频晚到提前拒绝，修复为原deadline/cancel内仅对精确缺正时间跨度WouldBlock等待，修复后双独立PASS、Release全量exit0。共享AV预算跨流失败无即时notify仍是有界延迟风险，未宣称全局fail-fast。

无音频补充负例不再立即拒绝，8.42秒因共享prepared容量耗尽/音频capture结束exit1；不记完整30秒等待或完整验收。原120秒双源RTP/RTCP→H264 VBR8000–12000kbps、1280×720/30、AAC CBR192kbps→MPEG-TS/RTP实流caller_stop exit0、payload96931预约=释放/0B/0对象。仍停读7.694秒、2691包线端完整缺口、启动静音3.107302秒，内容A/V UNKNOWN，完整仍FAIL。独立评分复核保持75/100。19项实流/构建临时文件及7项负例产物已记录后逐项删除，全部本轮PID无残留，原源124427809B保留，无外部依赖改动或push/merge/deploy。

下一闭合实施范围：沿共享parser/reorder/depacketizer保留AU完整entry成员与额度，准备首帧probe消费同一session的有界AU产物，按序保存clock/invalidation/AU，再向runtime事务性交接真实arrival、处理ordinal、pending及未完成AU；复用现coverage/window选择退休。当前累计wire预算尚未成为resident预算，不能只放入session指针或按时间删raw包解除停读。完整Windows门禁前不进入RKMPP。详见runtime r128与准备交接文档。

## 2026-10-08 r129：修正连续接收的所有权设计（实施中，取代此前原包成员索引要求）

代码证据：MediaRtpPacketParser把payload写入自有vector；H264/HEVC/AAC组装拥有字节；MediaRtpDepacketizer::makeRtpAccessUnit使用av_new_packet+memcpy形成独立AVPacket。因此AU存活不依赖原始entry，不新增AU→raw全成员表。此前把防止重复回放误推为必须保留成员索引，现撤回该依赖。只有尚未解析的raw、Session内reorder/pending/未完成组装及probe实际引用仍须保有并计费。

行业核验：[GStreamer depayload](https://gstreamer.freedesktop.org/documentation/rtplib/gstrtpbasedepayload.html)区分used/dropped/delayed/flush；其header-extension成员缓存不是本项目自有AU载荷所需的通用索引。当前选择复用本项目已有拷贝所有权，未宣称零拷贝或allocator/RSS精确上限。

完整调用链要求：每源facts/candidate→唯一接收owner驱动同一ProtocolSession→有界AU与有序协议结果→first-frame probe引用同一AU（仅decoder flush/replay）→全图准入→一次性交接Session、未完成预约、未消费产物、原始处理水位与下一输出序号→RawRtpInputNode接续。已经处理的raw不得再次回放，真实arrival与schedule不得混用旧replayOrigin平移。线程取消、失败、状态移动与已有Windows/Linux adapter共用契约。

容量分层：协议工作集复用现descriptor/window/datagram/AU envelope；完整AU复用typed retention的对象/字节有限准入，probe引用表和decoder retention另计；交接期间准备与runtime并存费用加总。原probeSize仍为累计采样，不能改名当驻留额度。AU cadence/open deadline不证明网络突发或RTCP事件速率；事件须独立有限准入，满载不得静默合并/丢弃或暂停接收冒称成功。临时stage可明确超限失败，但最终连续门禁仍需现startup selection形成合法退休，不能以stage通过替代任务。

实现与审查门禁：不新增默认时间/包数，不提前运行未准入DAG；首次规划缺事实在DAG前失败。完成后两个未参与代码实现的智能体逐项复审，再Release全量与原120秒同规格realtime验证，沿用Windows通过后RKMPP顺序。当前未修改运行行为、未运行新实流，r128停读/缺包/启动静音结论仍有效，75/FAIL不变。

### r129 必要状态边界与首步接线

ClockGroupNode当前还独占mapper全状态、validator、generation/minimumGeneration及每流lastProcessedSequence；validator.snapshot并非只读，会更新锁定/过期/代际/commonSourceEpoch。仅移交ProtocolSession或最终ClockGroupSnapshot不能替代这些状态。现publish在无DAG输出端口时直接返回，准备期不能直接调用未准入节点。

首步将既有事件归约、序号校验、mapper/validator及snapshot转移收进同一可移动ClockGroupSession，由原生产节点实际使用；节点保留端口pending/调度/preferAudio、日志、initialPublished与emit时点。共享核心只接协议值，不依赖GraphExecutionContext或媒体buffer。保留原拒绝→invalidate→publish语义，不借重构修改算法。后续准备阶段消费同一核心并在未向下游发布epoch的事务中移交；已发布后的中间失效不能省略。

另已确认startup coordinator的tryRelease会计算masterRelease并切StreamsPrimed，Running后submit直接PassThrough，不能把它直接当成滚动准备缓存。需要复用现WindowSelector的选窗/前缀退休，同时分离候选形成与epoch发布；该闭环仍属待实现，不能以当前会话抽取声称持续接收已完成。

### r129 首次审查与构建中止

A审查FAIL：新Session::create只检查maximumRateErrorPpm>0，漏原MediaRtpSourceClockMapper::create的<1000000约束；B初审PASS未发现该项，不能据此判整体审查通过。正在抽取mapper现有完整配置校验供两构造入口复用，不创建虚假generation临时对象。

初次Release正在构建时发现上述问题，为修复后重新冻结而主动中止：核验构建脚本命令行后taskkill /PID 29444 /T /F，父构建session exit1；taskkill因CL子进程30024报operation not supported返回1，随后父进程及该子进程等精确PID复核均不存在；该错误原因未进一步确证。此为主动中止，不是构建通过或编译失败结论，未运行任何新媒体产物。

## 2026-10-08 r130：连续准备接收闭环（实施中）

r129实流确认累计probe预算耗尽后停收7.779秒、2749包在线端完整。本轮保留probeSize的累计采样意义，将协议工作集、完成AU前缀和probe/decoder引用分别准入；原open deadline约束准备和交接。以实际cadence、AU envelope、已持有prefix和显式准备期限复用现retention形成有限准入；容量不是网络突发上界，耗尽必须明确失败，不等待未启动runtime释放额度。暂不引入rolling淘汰或提前发布startup epoch；原StartupCoordinator继续负责合法起点。

行业核验：[GStreamer jitterbuffer](https://gstreamer.freedesktop.org/documentation/rtpmanager/rtpjitterbuffer.html)以显式latency约束缺包等待；[preroll](https://gstreamer.freedesktop.org/documentation/additional/design/preroll.html)说明准备与播放状态的区别，但不作为本项目提前启动未准入DAG的依据。AAC容量核对[RFC3640](https://www.rfc-editor.org/rfc/rfc3640.html)及现AU-header/fragment代码；Opus核对[RFC7587](https://www.rfc-editor.org/rfc/rfc7587.html)及现单RTP包输出代码。本项目继续原parser/reorder/depacketizer，不另造算法或更换FFmpeg。

已核验的必要状态：最终ClockGroup checkpoint不能替代历史AU所经历的SR revision/失效。PacketClockProjector与TimestampAligner自身无可变状态；binder实际持有lockedSnapshot、待定AU/deadline、视频lookahead/timestamp/lastDelta、terminal与generation cutoff。共享组件放runtime/session，复用原MediaBuffer/FFmpegPacketBuffer和RAII；不依赖GraphExecutionContext或运行未准入DAG。准备阶段没有已发布epoch，不能伪造runtime generation arbitration。对应状态与未发布AU需一次性交接，禁止重复投影或只用最后snapshot映射全部历史AU。

线程与所有权：source级同步只串行化AV状态变化及队列操作，不在该锁内等待socket/额度或调用probe。保持唯一transport接收者；runtime排空准备前缀时仍应继续接收，不能把停读移动到交接以后。已处理raw释放并记frontier，未完成组包、pending和完成AU保留实际owner/额度；交接先准入有限graph额度再释放准备lease，probe引用不得失去计费。Windows/Linux复用上述逻辑，仅保留真实socket adapter区别。

本轮root已拆出protocolWorkingSet容量产品，原首帧plan调用同一计算。视频signaling/assembly在连续处理下可并存；旧分阶段probe仍按max计算。AAC计AU-header列表、单个fragment/临时副本及所有完成AU，Opus计临时副本与完成AU；clock/CNAME/history、session对象、transport和持久AU另由owner准入。尚在实现和审查，未构建、未运行r130实流，不改变75/FAIL结论。


### 2026-10-09 r130 接线进度（尚未验收）

已接入每源连续协议 owner、原接收线程切换现有批量 ingress adapter、同一 AU 首帧探测、共享 master clock、时钟与 Binder 状态一次性交接及 typed RTP runtime registration。安装在双流同一锁定边界取得 prefix 数量与时间，按原 arrival 合并处理，之后不再原包回放。逐源输入等待时间继续显式可配置、无默认值。

本轮发现并修复的额度身份问题：ledger waiter 以 producer node/stream/payload 为键，Binder 借用 Raw 的 producer 会把唤醒投递给 Raw。现为 Binder 规划自身 producer，使用同源 AU envelope 与独立额度；已带 Raw credit 的正常包不重复申请。准备 retention 的容量依据原 open deadline 剩余时间和原 startup wait 的较大者，策略 acquisitionWindow 仍保留原 startup wait，最终 DAG 复用同一容量产品。

当前代码仍在完成自查，尚未冻结双人独立审查、Release 重建或 r130 实流测试。有限 AU/事件准入不证明任意输入突发下永不耗尽；超限须显式失败。不得据此更改现有 Windows FAIL 结论或进入 RKMPP。未修改外部 FFmpeg，未提交、推送或部署。


r130 首轮两名独立审查均 FAIL：合法过滤 RTCP 的 bootstrap ordinal 空洞被拒绝；Binder purge 未取消自身 producer waiter；双接收线程把旧 arrival 当共享状态当前时间导致时钟倒退；continuous source 实际预算未进入最终 graph ledger；持久 AU 漏计 FFmpeg padding。以上均已修正并进入第二轮双审，尚无构建或实流通过结论。Bootstrap 显式移交原 frontier，live 保持连续；共享状态复用生产节点锁内当前处理时间，SR arrival 不变；新增预算保留实际 allocation identity，与原 raw 预算分别准入，运行期 binder 校验实际身份与额度。第二轮冻结代码摘要（267 个当前改动 C++/头文件，含既有 WIP）：`544850971d58b51a59720716db438bb9ccc9c3c001fc9c560b804308639b664e`。

### 2026-10-09 r130：连续准备接收已实接，原规格启动静音回归通过

准备与runtime复用同一协议/时钟会话、AU产物及原接收线程，实际连续预算进入最终ledger。首次双审五项FAIL已修复，第二轮两者PASS；首次编译缺头文件已修复并双复审PASS，随后Release全量699目标exit0。

r130b双源原120秒H264/AAC→H264 VBR8000–12000kbps、1280×720/30、AAC CBR192kbps→MPEG-TS/RTP：两源3600帧exit0，逐源等待100/200ms仅为本次命令值。输入/输出RTP线端连续，TS计数及PTS步长正常，离线未再检测此前约3秒启动静音；双格与全黑完整VLC画面有效。caller_stop exit0，payload104992预约=释放且结束归零，encoded差1包不记完全drain。WS峰241356800B/private峰725454848B；内部漂移155..156ns不代表内容同步。

线端无缺包不能单独证明应用消费全部包；本轮无恢复，完整矩阵、内容A/V、可信EOF、C API及Windows→RKMPP仍未闭合，整体门禁维持FAIL，不调整既有75分。实际命令、PID、失败尝试及数据见runtime r130b；未更改外部FFmpeg，未提交/推送/部署。

### r130 恢复边界复核（源码风险，尚未实流复现）

独立审查追溯：RawRtpInputNode原身份拒绝仅适用于带source.playback_epoch.identity_transition=replan_session的shared-playback链；合屏不设置该选项。连续PreparedRtpSourceSession::drain则无条件拒绝SSRC/PT变化，准备期authority失效拒绝合理，但激活后快速SSRC重入可能收窄原合屏恢复语义。

不能推断所有重启都会失败：匹配BYE、时钟超时或generation purge会resetAssembly并清除reorder旧SSRC；其后新源可能正常重新捕获。旧身份尚未清除前新SSRC到达才可能触发新增Unsupported。下一实流先复用原120秒源完成结束后发送器重入，随后覆盖快速重入；不以同SSRC暂停代替。若失败，先取得生产证据，再由planner区分准备期失效与runtime源代际恢复，复用现discontinuity→关键帧→ClockGroup→源purge→新SR→Binder链，保留媒体配置契约，不新增外部FFmpeg修复范围。

### 2026-10-09 r131：所选AV源结束后重入，有限双审PASS

原120秒双源RTP H264/AAC→H264 VBR8000–12000kbps、1280×720/30、AAC CBR192kbps→MPEG-TS/RTP；初始AV、纯视频及重入AV均完整120秒/3600帧exit0。双格→右黑左画面→全黑→仅右格恢复有完整VLC证据，generation2持续工作；27.395556秒失源静音后非静音恢复约118.453秒。输出同SSRC、RTP/TS与PTS连续，caller_stop exit0、payload162868预约=释放归零；encoded差1包及AAC关闭时残留2帧，不计完全drain。两名独立复核均限定PASS：匹配BYE及失活/purge后重入；未覆盖快速SSRC切换、内容A/V、完整矩阵、可信EOF、C API或RKMPP，整体75/FAIL不变。

22项产物记录后逐项清理，六媒体PID无残留，原源124427809B保留。下一项为旧会话尚未失效时快速发送器替换的真实失败取证；不以本轮成功代替该边界，不盲删身份校验。命令和细项见runtime r131。无外部FFmpeg修复，无提交/推送/部署。

### r132 快速SSRC替换回归：真实失败驱动修复

旧源强制结束后68.812ms新SSRC首包到达，原120秒同规格源真实CLI在SourceSession::drain准备期拒绝处分发Unsupported，Raw25/26导致整图exit1；这不是外部FFmpeg错误。详见runtime r132的命令、PID、指标与清理。修复采用planner提供的既有MediaAvSourceLifecycleMode，准备阶段拒绝身份变化；runtime PreserveActivatedOutput允许SSRC discontinuity进入既有generation/ClockGroup/purge重捕获；PT及prepared媒体配置校验保持。保持原接收线程和owner锁，不添加队列、无界缓存或平台专用链。有限预算/背压与显式失败合同不变，旧/新媒体由既有代际隔离；双独立审查及Release原失败条件复验尚待完成。依据GStreamer rtpbin SSRC/PT分层与RFC3550源身份管理，固定单活动源边界不扩为任意并发发送者混流。

r132三文件修复已冻结，两名未参与实现的独立审查者均明确源码PASS。二者核对freeze仅移交责任，初次gate继续拒绝失效及非初代generation；参数集、PT、容量边界保持。仅固定媒体契约的单发送者替换，不宣称并发SSRC交错竞争。Release全量构建执行中，运行修复尚未验证。

### 2026-10-09 r132/r133：快速SSRC替换回归已修复并有限双审PASS

r132原规格快速切换68.812ms复现准备期身份限制沿用runtime导致整图Unsupported/exit1；不是外部FFmpeg问题。3文件修复由planner下发既有生命周期模式，runtime仅允许授权SSRC代际恢复，准备期SSRC及全时PT/媒体契约保持严格；双独立源码PASS、Release全量exit0。

r133原120秒源、双路RTP H264/AAC→H264 VBR8000–12000kbps、1280×720/30、AAC CBR192kbps→MPEG-TS/RTP：切换58.694ms更快，代2恢复，纯视频及新AV各完整120秒exit0，输出RTP/TS/PTS连续。切换静音5.007506秒后非静音114.493106秒；仅VLC录制60秒抽帧证明双格恢复，现场恢复截图未取得，不能记现场视觉门禁通过。caller_stop exit0、payload115446预约=释放归零；encoded差1包/AAC关闭残留2帧，非完全drain。两独立运行复核均限定PASS，不能称无缝恢复。

r132失败13件、r133实际20件产物已记录后清理，各六PID无残留，原源124427809B保留。整体75/FAIL保持：内容A/V、现场恢复画面、完整矩阵、可信EOF、C API和Windows后RKMPP未闭合。下一项在既有完整矩阵验证中补现场恢复证据，继续优先Windows；未提交、推送、部署或修改外部FFmpeg。

### r134 输出域晚到决策冲突（新生产失败，尚未修复）

三源V/V/AV→HEVC CBR8Mbps/720p30真实CLI触发约334ms晚到，ContinuousOutput错误要求source coordinator；源都尚在传输，非SSRC/失锁。MediaAvOutputSynchronizationPlanner::plan设置ContinuousOutput后调用通用policy，MediaVideoSyncController::updateFrame把硬相位误差转Reacquire，scheduler无条件requestReacquisition，而validator禁止该域源权威。终止/flush同类调用亦须核对。

行业依据GStreamer QoS https://gstreamer.freedesktop.org/documentation/additional/design/qos.html ：相对pipeline clock测迟到并区分上游处理和sink延迟；其显示端丢原始帧策略不可直接套用已编码HEVC依赖包。本仓库controller还存在非keyframe late Drop及Repeat策略，必须核对encoded输出合同，不允许提高250ms阈值、任意丢参考包、重置唯一输出时间轴或补source coordinator。需planner明确合法动作、validator拒绝矛盾合同，并沿既有transport deadline/容量产品处理过载；仅换准确错误不能替代三源复验。当前尚未定位实际晚到的线程/encoder/背压环节，不归因外部FFmpeg或用户其他程序。
### r134 调度合同调查补充（尚未实现）

VideoOnly 的协议准备提前量来自 `MediaRealtimeVideoRuntimePlanner` 的 `deployment.encode().latency.targetResidence`，运行时释放点为 `emit - protocolPreparationLead`；不能改成仅等待 emit。AV output runtime request 已持有同一 deployment，可由 planner 形成类型化调度产品，不需新增公共字段。最终 sender 的真实墙钟期限校验保持；TS emission 的 actualMasterNow 目前仅用于诊断，不能当作已存在的墙钟准入。

单源 AV 虽经 ContinuousOutput 临时输出规划，`MediaRealtimeAvSyncRuntimePlanner` 最终保留原 source synchronization，因此不能据此认定其 runtime 为 ContinuousOutput。单源共享域的 Flush→源 coordinator 与合屏持续输出域的权限需分别验证，不能一律移除。编码包提交合同应由共享输出组件实施，避免保留显示帧 Drop/Repeat/Reacquire。此调查尚未修复 r134，也未解释约334ms实际晚到；原 FAIL 与同规格复验要求保持。
### r134 编码输出调度首轮实施与审查

21个核心文件已将编码AU出口改为保序提交，AV与VideoOnly复用releaseTime；planner从deployment.targetResidence提供协议准备提前量，移除ScheduledAccessUnit显示/重复身份及AV对应缓存，SharedSourceOutput Flush保留，ContinuousOutput明确拒绝源Flush。Release clean-first/all-target构建exit0，realtime产物2026-10-09 16:29:45、5767168B；未运行本版本媒体测试。

两名独立审查均FAIL：新scheduling未完整进入最终计划及图绑定一致性验证；ContinuousOutput仍生成并验证无消费者的显示恢复策略。正在修复并将双复审，不能把本次构建成功记为源码审查或r134运行通过。无外部FFmpeg修改、提交、推送或部署。
### r134 合同复审与编译修复进度

首轮审查两项FAIL已补：最终计划/图校验与typed scheduling一致，ContinuousOutput拒绝显示恢复产品；同时按VideoOnly公式补首包activation lead。复审A发现单源SharedSourceOutput角色转换后未apply源策略的P1，已将apply移至角色确定后的公共路径；两名审查者重新沿单源RTP/TS/URL及合屏链核验后源码PASS。B前轮漏过该P1的PASS不计最终通过。

第二次全量构建exit1，中间诊断被工具截断；第三次使用临时完整日志再次全量构建exit1，准确错误为MediaRealtimeAvSyncRuntimeOutputValidator.cpp:292/294 C2039：最终MediaRealtimeRtpTranscodePlan不存在deployment成员；后续C2672/C3536为连带错误。正在改用既有datagramTransport.encode().deployment权威事实并重新复审/构建。尚未启动本版本实流，不把编译或源码结果当r134通过。

最终两处部署事实修复经双独立复审PASS；第四次Release全量构建configure/build exit0，realtime产物2026-10-09 16:43:51、5772288B。前两次失败记录保留，尚待r135原规格实流与单源回归。两件临时构建日志在记录诊断后逐项删除。

### r138 调度分段诊断与协议期限调查（2026-10-10）

采用GStreamer latency tracer按节点阶段记录实际时钟的方式（https://gstreamer.freedesktop.org/documentation/coretracers/latency.html），当前仅在既有scheduler迟到日志补head_arrived_ns与scheduled_at_ns。两个时间分别表示scheduler取到head后及构造输出前，不能代表编码完成时刻；未知时明确unavailable。数据随head和pending commit生命周期释放，不增加线程/队列/公共参数，不改变排序、期限或错误策略。可区分head取得前、scheduler内及输出提交的等待，仍需实流复现。

独立源码调查确认TS protocol canonicalDeadline在RTP与UDP materializer映射丢失，sender采用max(release,materializedAt)+maximumResidence，零deadline_misses仅代表wire驻留期限。TS原上限来自AU dispatch或maintenance completion，应独立保留；拟采用与wire上限较早者，但须同时修正maintenance生产时刻合同，不能只提前报错或冻结PCR。RFC2250§2的MP2T timestamp是目标发送时刻，不直接规定本项目具体硬deadline。该功能修复尚未实施；本轮三文件仅诊断，待双审与全量构建。真实迟到根因仍未闭合。
诊断冻结后三文件由两名未参与实现的独立审查者均明确PASS。head_arrived_ns为出队解析后观测，scheduled_at_ns到commit包括输出构造、发布与背压，不单独等同credit等待；迟到日志格式化有观测开销。全量Release构建正在执行，尚不标构建或实流通过。

Release全量clean-first/all-target重建已实际完成，configure/build exit0；realtime产物2026-10-10 09:15:06、5774336B。未运行local CLI，诊断增量尚待原规格realtime复现。

### TS期限修复的内部合同边界

2026-10-10 双独立调查确认：TS协议deadline需沿RTP和UDP保留，wire驻留预算独立；最终上限取两者较早者，重试不得延长。该修复不新增公共输入。协议超期数据报必须明确失败，不能以重新开始计算wire驻留时间掩盖。

不能只删maintenance poll的transportDecodeLead等待：mux当前无未来媒体下界，队列暂空不证明此前AU已全部送达。GstAggregator采用序列化GAP/EOS和协商live latency；FFmpeg将PCR与媒体同一mux循环内生成。本仓库独立空闲维护并不与之等价。需选择实际权威成立的上游进度合同，或由已规划处理期限驱动并把迟AU明确判为超期；不允许凭帧率猜测、冻结PCR或增加经验等待。当前仍未证明编码最大生产延迟，因此先完成r139诊断复验再实施一致修改。

依据：https://gstreamer.freedesktop.org/documentation/base/gstaggregator.html ，https://www.rfc-editor.org/rfc/rfc2250.html#section-2 。RFC的MP2T发送时间戳语义不等于规定本项目具体硬deadline。
### r140前：协议期限与维护触发一致修复

在上述调查基础上进一步反例核验：maintenance reservationCompletion为serviceStart而非completionDeadline；按计划nextTransportDeadline触发时，仅推进deadline<=now。若旧available来自前一有序AU，则不超过下一AU emit；若来自维护则不超过now。因此严格dispatch>now且正transportWindow的有序AU不会仅因维护失去全部窗口。dispatch==now的零窗口仍拒绝；这不保证剩余窗口能发送完，sender必须继续真实deadline判定。该限定结论收窄了前文对必须新增future watermark的判断；对接纳过期AU或保证任意生产延迟仍不成立。

实施5文件：wire deadline方法显式接收可选协议上限，TS通过已有protocolBatch当前未消费span同索引提供；形成min(protocolDeadline, max(release,materializedAt)+wireResidence)，协议上限早于release或物化时过期立即失败。裸RTP与RTCP明确无TS上限，保持原wire合同。无新public输入/队列/线程；事务和背压复用原RAII，失败在协议transaction消费前返回。

ProjectMpegTsMuxSessionAdapter仅活动时间线poll按原nextTransportDeadline唤醒，删除附加transportDecodeLead等待；startup分支保持。PCR不冻结，不重写AU/PCR/PTS，迟到仍fail-closed。共享Windows/Linux/单源/合屏路径均受影响，先以Windows原三源规格复验并做单源回归，之后RKMPP。此修复恢复期限一致性，不消除尚未定位的r138上游174.64ms迟到。源码冻结后正在双独立复审和Release全量重建，尚非验收通过。
### r140后：音频进度下界与迟到定位审查（2026-10-10）

已沿mapper→encoder→canonicalizer→scheduler核验，并由未参与实现的独立智能体只读复核。同步编码音频具备连续样本事实：mapper拒绝非priming packet PTS不连续；AudioEncodeNode拒绝活动origin变化；canonicalizer校验下一begin等于前end，并在输出push成功后推进状态。因此生产者可由`origin.sourceStart + checkedFromTicks(end-origin.epochOutputSampleIndex, 1, outputSampleRate)`形成同代次下一dispatch的精确下界。

不得在scheduler对任意AU直接使用presentation+duration：两者分别截断换算，相加可能比下一精确时间小1ns，不能宣称精确；当前AU也未保留origin。更重要的是CopyPacket通过同一scheduler却绕过编码canonicalizer，不具备同样的样本连续性证明。通用duration推断方案FAIL；由planner明确连续性合同、producer提供类型化下界的方案仅有条件PASS，尚未实施。下界只能在音频成功commit后生效，并随generation/purge/Flush/lifecycle清除，保留控制优先级。

本轮决定先补齐音频迟到的分阶段证据，不立即用下界放行替代根因修复。aggregate共用一个pending输出；FFmpegNodeRuntime在pending传输受阻时不调用onProcess，故视频背压可能延后后续音频生成。这是代码支持的待证假设，不是r140已证明根因。现有head出队时间无法区分aggregate受阻、encoder处理/资源等待、canonicalizer队列等待及线程停顿；10964次payload pressure也不能单独归因。

后续诊断按输出generation与真实sample interval关联aggregate生成/成功发布、encoder接收/submit/receive、canonicalizer接收/成功发布与scheduler出队；采用有界记录和失败/迟到汇总，避免逐包同步日志造成自扰。缺失阶段明确unavailable，不把发布尝试当成功，不新增公共参数、生产线程或无界队列。还需唤醒到实际执行证据才能排除线程调度停顿。方法依据为GStreamer逐元素latency tracer（https://gstreamer.freedesktop.org/documentation/coretracers/latency.html）；GstAggregator的GAP/EOS序列化机制（https://gstreamer.freedesktop.org/documentation/base/gstaggregator.html）不能被等同于本项目已有未来进度合同。

本轮无生产代码修改、无新增实流运行；r140运行FAIL、完整75/FAIL保持。下一动作是实现上述有界观测，再按原规格Windows realtime复现，依据真实阶段延迟选择修复；不能用视频较早放行掩盖音频本身已迟到。
### r141前：九阶段有界音频观测已实现

新增MediaAudioStageObservation并接入aggregate built/committed、encoder input/submitted/packet/committed、canonicalizer input/committed、scheduler input。各阶段仅保存最新观测及最大相邻事件间隔两端，携带generation、sample interval/rate及统一steady clock；不持有媒体，不新增线程/队列/公共字段。热路径不格式化日志，stop/abort在线程退出后汇总，格式化异常不改变媒体失败语义。

两名未参与实现的独立审查者均明确源码PASS。成功发布观测来自既有commit回调或Accepted之后；scheduler为解析后的出队观测。每阶段最大gap包括正常节拍、背压和线程停顿，不等于处理耗时；只有generation、sample interval匹配的跨阶段记录才可关联。固定快照并非完整trace，未命中故障附近不能排除阶段，尚无唤醒到执行证据。生产排序、期限和错误策略未改变。

Release已通过唯一技能入口完成clean-first/all-target重建，configure/build exit0，realtime产物09:44:28、5782016B。双独立源码PASS不代表实流通过；r141预检启动超时、r142正在复验。

### r142后：期限失败已位于mux入口之前，具体等待仍待证

r142三源完整120秒exit0，CLI在源活动时自然exit1：protocol datagram exceeded its absolute emission deadline before materialization。九阶段诊断有效；无late_audio_head/late_output_commit。mux ready84169824700，比dispatch84146990501晚22.834199ms；对应worst batch取出84169939100，比ready晚114.4us。后者不支持将主要迟到归于这次TS分包，但最坏记录并无AU身份，仍不可直接视为失败数据报的完整关联trace。结果与清理清单见运行记录。

独立只读审查确认scheduler→MediaScheduledTsAccessUnitAdapterNode→FileMuxNode，存在两段通道、adapter输出背压及session reservation。FileMuxNode先排session批次；FFmpegNodeRuntime先排pending transfer，下游不可写时不进入onProcess，也不消费下一AU。正常活跃maintenance用InputOrDeadline；pending分支虽有DeadlineOrCancellation，但正常writeAccessUnit同次物化整批并清空pending，不能仅凭该枚举判为长睡眠根因。

下一步按同一generation+stream+canonical dispatch/sequence关联scheduler发布、TS adapter取出/发布、FileMux取出、session reservation获得及write入口，并记录输出pending transfer阻塞/解除及批次身份。需要区分队列驻留、输出背压、session锁等待和worker停顿；缺日志不能当成0时延，也不能把不同AU的最大值相减。仍须有界、同一时间域、不逐包同步日志，不增加媒体队列、公共参数或放宽deadline。r140音频迟到根因仍未解决；r142是新的失败证据，不替代既有验收矩阵。
### r143前：同AU不可变阶段时间设计

沿用已调查GStreamer latency tracer的同媒体关联思路，r142极值没有AU身份，不能跨阶段直接相减。此次采用现有AU内的定长诊断值，记录scheduled对象创建、TS adapter出队、TS对象创建，随现有唯一TS创建调用传递；mux额外记录write进入及session取得后的时刻。使用同一steady clock、原generation/stream及canonical sequence标识，不持有新媒体引用、不新增队列/线程或公共输入。诊断关闭时标unavailable。

这些是创建/消费边界而非成功发布：创建到出队包含发布背压、队列与线程调度。先不声称能够区分其中每一项。避免commit回调在对象发布后回写时间产生跨线程竞争；字段在发布前完成并保持不可变。仅在mux入口已过dispatch时、退出session临界区后格式化日志，不将观测用于任何调度、期限或错误决策。格式化失败不覆盖原错误。若本轮捕获到哪一段迟到，再沿该段现有背压/唤醒状态继续定位，不增加经验等待或扩大期限。
同AU诊断7文件冻结后，两名独立非实现审查者均明确源码PASS；git diff --check通过。时间为阶段观测，session_ready含reservation后校验；新增定长wrapper元数据不在媒体payloadFootprint中，不能宣称payload账本覆盖完整RSS。Release全量重建进行中，尚无新运行结论。

r143执行结果：上述7文件双独立源码PASS、全量Release exit0（10:08:44，5786112B）。实流再次期限失败，但同AU关联证明此次两个迟到AU从scheduled创建到mux ready均不足0.6ms。九阶段音频同区间定位到aggregate相邻built观测150.9643ms，后一样本发布/编码接收迅速。双独立证据复核有限定位PASS，完整FAIL；不能称buildAudio调用之前，因为built记录在函数末尾。后续最小观测覆盖aggregate onProcess/consumeInputs/选择deadline/buildAudio/compose与既有pending transfer首次阻塞/解除，避免将未进入onProcess的空白直接归于worker。调用墙钟时间仍含抢占，不能当GPU执行时间。
### r144实施前：聚合调用内与调用间隔分离

r143真实失败已把150.9643ms间隔定位到第二次aggregate_built观测之前（包含该次buildAudio）。参照GStreamer latency tracer逐元素时间边界（https://gstreamer.freedesktop.org/documentation/coretracers/latency.html），本轮仅补充owner线程内process、purge、bind、consume、activate、buildVideo、buildAudio、canvas compose、emit的入口/出口墙钟记录；另记录上次process返回至下次进入的最大间隔。各阶段固定保留最大区间及媒体游标，process附返回状态、实际deadline与进入onProcess/入口pending标志，结束后统一输出；不保留媒体引用、不增加队列或线程，热路径不格式化。

当前FFmpegNodeRuntime已有pendingOutputBufferCount可观测入口状态，未进入onProcess只能记为base_without_onprocess，不能直接判为worker未唤醒。调用墙钟包含OS抢占，最大区间可能属于不同AU，不能相减后直接归责GPU。复用既有steady clock及State日志开关，无新公共输入或调度策略；错误、输出背压、planner容量及Windows/RKMPP共用链路保持。先双独立源码审查与Release全量构建，再原三源Windows realtime取证。
### r144完成记录与下一动作

聚合调用计时4文件双独立源码PASS，Release全量exit0。原三源及AV重入各120秒自然exit0，CLI caller_stop0；本轮未复现旧deadline。最大process16.6983ms与canvas16.5904ms同次调用，built最大间隔32.9401ms，启动期between_calls107.0106ms不得用来解释r143。wire RTP序号/DTS/AV解码有限证据PASS，VLC录制DTS警告未在线端复现；画面缺失、内容AV和完整矩阵等仍FAIL。22件临时产物与进程清理完成，完整记录见runtime r144。下一动作仍是原规格Windows间歇期限取证，使用当前已构建诊断，不盲改GPU/调度或放宽预算；保留C API和单源等未完成门禁，Windows通过后才RKMPP。
