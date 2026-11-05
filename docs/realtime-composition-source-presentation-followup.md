# 合屏源显示时间修复：r393 后续实施边界

状态：完整存储准入实现中，SourcePresentation 实流尚未通过；下文保留调查与各次修改证据。源费用总额已实现；当前仍缺容量求解及生产 solver 接线。目标为 Windows 后 RKMPP 的完整多路合屏，不降低 r393 的四路异构规格。

## 已核验的根因与复用位置

- r393 的 HEVC RTP 时间戳回退被 uint32 差值转换为超大正间隔，继而产生错误的准备容量。详见 [r393 实测](realtime-composition-r393-validation.md)。[RFC 3550 §5.1](https://www.rfc-editor.org/rfc/rfc3550.html#section-5.1) 允许重排媒体的 RTP 时间戳不按发送次序单调增长；取绝对值或只收集正差仍不能证明固定帧间隔。
- 合屏入口 `MediaRealtimeCompositionInputPreparer.cpp` 已切换 SourcePresentation，但完整准入门禁尚未通过。目标保持 SourcePresentation：保留 AU 身份和原始 PTS，解码后形成 SourceDisplayPoint，由已有 aggregate 的独立输出时钟选帧。不得按 H264/HEVC 建立两条处理链。
- `MediaCompositionSourcePlanningKernel` 已有无 cadence 的准备容量、资源账本、所有权校验；`MediaAvContinuousAggregateVideo` 已有显示点选帧。优先复用，不能扩大经验容量规避错误。
- 独立只读审查 timing_path_review 判定：方向可复用，但直接切换入口不能通过 RKMPP；Windows 也尚无本轮完整运行证据。

## 本机与远程固定依赖证据

2026-11-05 通过 SSH 实读 `/home/tang/ffmpeg-rkmpp-close-ownership`，HEAD 为 `ab1e61adaa21ff129caa8e01a1198156044567ed`，工作树干净。未修改依赖。远程无 rg，改用 grep/sed 读取。

| 核验位置 | 事实与边界 |
| --- | --- |
| rkmppdec.c:1054、1118、1131、675 | 输入 PTS 转换后写入 MppPacket，输出通过 MppFrame PTS 转回；存在时间戳传递通道，但尚不能证明任意源的唯一身份映射 |
| rkmppdec.h:84–90 | 中间时间基为 1/1000000，双向 av_rescale_q；后续身份方案必须证明精度、溢出、重复 PTS、跨 generation 的行为 |
| rkmppdec.c:670；decode.c:1592、1563 | 输出调用 ff_decode_frame_props；公共 helper 从 last_pkt_props 复制 opaque，并非从该 MppFrame 的对应 AU 取回。不能仅打开 COPY_OPAQUE 就宣称解决重排身份 |
| rkmppdec.c:1152 起 | 没有新输入时使用 MPP_TIMEOUT_NON_BLOCK；发送 EAGAIN 时内部才使用 100ms 接收；draining 使用阻塞接收。100ms 不是对外完成通知或有界完成 SLA，不能直接充当新的轮询常量 |
| MediaPipelinePlanner::materializeSourceExecutionContract | RKMPP 仍规划 SubmissionOrder；SourcePresentation 轮询已改为消费显式输出周期，身份关联仍未完成 |
| MediaRtpSourceFirstFrameProbe、MediaVideoDecodeLineageOption | 当前 SourceAccessUnit 仅允许 COPY_OPAQUE，探测和运行时需使用同一身份传播契约，不能只改 probe 放行 |

上述固定版本父提交及补丁范围见 [既有依赖记录](../3rds/ffmpeg-rockchip/README.md)。Windows 本机 mabs 的 rkmppdec.c 与该依赖不同，不用它替代远程能力证明。

## 实施顺序与验收门禁

1. 完成后端身份与接收调度的类型化契约设计，再统一切换合屏入口。Windows 复用 COPY_OPAQUE；RKMPP 在现有 adapter 内证明可关联的返回身份，不使用提交 FIFO 猜配。
2. 身份参考 [GStreamer V4L2 decoder](https://raw.githubusercontent.com/GStreamer/gstreamer/main/subprojects/gst-plugins-good/sys/v4l2/gstv4l2videodec.c) 的 frame number 往返关联方式，以及 [GstVideoCodecFrame](https://gstreamer.freedesktop.org/documentation/video/gstvideoutils.html) 的独立身份。它是调查候选，不是 RKMPP 已支持的证明；必须先核对 MPP 是否原样保留载荷以及是否参与排序、字段解码、时间戳修正，不能盲目替换 PTS。
3. 调度继续复用 VideoDecodeNode 和 DAG executor。FFmpeg context 单线程串行访问，不新增并发 send/receive；若需要 adapter 执行单元，必须明确取消、join、flush 和有界交接队列。完成通知能力不足时，需从已有显式部署期限形成有依据的调度产品，不能把名义 VUI 帧率、输出帧率或依赖内部重试常量当输入事实。
4. 资源继续由 planner 及既有准备预算、lineage registry 和 payload credit 管理。保留 admission 背压；容量耗尽、未知身份、非精确映射不得静默丢弃或 fallback。generation purge 应同时清理未完成身份与输出引用，使用既有 RAII。
5. 可观测证据至少包括每源提交身份、输出身份、真实 PTS、generation、输入 PTS 回退与输出身份重排、等待/背压及停止原因。现有 ReorderedFrames probe 可用于含 B 帧的验收；无 B 帧源不能被强制要求产生反转。
6. Windows Release 先复验 r393 同规格四路 H264/HEVC；再复验单源/选中音频失源、全源失源、重入、连续输出。SourceDisplayPoint 不依靠帧 duration 自动失效，必须确认 lifecycle 撤销输出后黑屏/静音正确。
7. 共享修复 Windows 通过后，RKMPP 使用现有目录增量全部目标、8 核编译，再完成相同链路。最后补两路、C API、持续 A/V 漂移、内存、出口与长时可靠性，双独立审查后才能声称完整完成。

当前没有新增公共字段，没有降低源参数，没有依赖修复授权扩展。此文档不是测试通过记录；本轮仅只读调查，无媒体进程、抓包或截图产物。

## 实施进度

实施顺序调整：先统一入口并在 Windows 验证既有 SourcePresentation 产品，以真实运行暴露共享链路缺口；随后完成 RKMPP 身份及调度 adapter。原因是 Windows 的 COPY_OPAQUE 路径已存在，无需等待 RKMPP 能力调查才能获得共享链路证据。代价是本地开发中间态的 RKMPP 合屏仍受身份与完整准入门禁阻挡；不得将此状态交付为跨平台完成，也不得回退到错误 cadence 路径掩盖缺口。

- 已修改合屏入口为 SourcePresentation，所有源统一处理；未新增时间戳算法、容量或公共参数。Windows Release 全量构建通过；两名独立审查只给进入实流诊断的阶段 PASS。
- [r394 同规格实流](realtime-composition-r394-validation.md) FAIL：freezeBatch 收到 nullptr，触发完整存储准入门禁。进一步核验 BatchAdmissionSolver 仅有抽象接口、无具体实现；因此不能称 SourcePresentation 生产接线完整。先按既有 [完整存储准入计划](realtime-composition-full-storage-admission-plan.md) 补齐 F/U/G 联合容量求值，再复验 Windows；RKMPP 身份和调度调查继续，不能用 cadence 回退替代此缺项。
- 冻结前事实复用：从 SourcePlanningKernel 提取 MediaCompositionProducerHolderPlanner，保留 opened decoder retention、协议原子完成上界、filter 执行与保留输入数的原校验；原 kernel 已消费新组件，删除旧构造块。组件不依赖封印容量、运行图或启动队列，返回独立持有的事实，后续 solver 可直接复用；当前尚未接入 freeze 前调用，不宣称消除 r394 错误。未新增算法、公共参数或容量常量。
- 后续求值接线核验：MediaPreparedSourceCandidateCostPlanner 目前无生产调用者且没有总额；startup batch/route/clock 和 frame ownership 费用只在最终源/资源规划消费。下一步共享 startup 分项产品，再合并实际 AU、frame ownership、源与 root 的唯一计费及冻结临时存储，最终实现 solver 并对照封印与最终图，不能把当前 root retained 与完整源费用重复累加。
- RKMPP 另有明确费用适配缺口：StartupBatchStoragePlanner 的 released audio wrapper 几何仅支持 MSVC 145/202604，有音频的其他平台会 Unsupported。须基于实际 GCC shared_ptr 构造几何适配；这不涉及修改外部 FFmpeg，也不能拿其他 registry 的 GCC 适配冒充该项已完成。

本单元验证：两名独立审查 timing_path_review 与 presentation_plan_review 对修正版3源码均明确PASS。首次构建47154 exit0，但holder对象早于审查修正，不能作为最终产物；随后修正版Release全量构建95350 configure/build均0，795目标，脚本核验cache/build.ninja及两CLI产物。未运行local CLI，也未重复已知freeze空solver失败的媒体测试；本单元没有新增实流通过证据或测试临时产物。

启动费用复用单元：新增 MediaCompositionStartupStoragePlanner，按同一 route、Nv/Na、group key 长度、数据报事实复用 batch/route/clock 三个既有计算器；最终 SourcePlanningKernel 已消费，CandidateCostPlanner 也调用同一入口并返回 startup 分项。没有把 batch 单件费用再叠加到 route 并发费用，没有生成 total/admission；released audio 为普通分配，clock backing 仍沿既有 resource 归属。route.metadata 与原 runtime.edgePolicies.metadata 由 RuntimePlanner 的一致性检查保证等价。候选费用入口本身仍无生产调用者，只有具体 solver 接线后才能称冻结前消费闭环。

本单元双独立审查均明确PASS；审查中已修正CRLF及clock分配域注释。修正版Release全量构建35021 configure/build均0，796目标，脚本核验产物完整；未运行local CLI，未新增媒体通过项或临时测试产物。下一步候选分项仍缺 frame ownership overlap、AU payload 和唯一 root 归属，freezeBatch 的空solver门禁保持，不称r394已修复。

RKMPP费用适配的只读前置证据：本机MSVC memory:1612使用new _Ref_count<_Ux>保存裸指针；目标GCC12.4的 /opt/mt-tools/gcc-12.4-native/include/c++/12.4.0/bits/shared_ptr_base.h:917 使用new _Sp_counted_ptr<_Ptr,_Lp>，定义位于419起。ReleasedAudioBuffer实际工厂是MediaBufferRef(new Derived)，因此需要独立对象与该裸指针control的两个请求，不能使用allocate_shared的inplace几何；适配尚未实现，也未修改外部文件。

帧所有权候选接线：MediaSourceFrameOwnershipStoragePlanner 新增纯 topology/reference/sourceIndex/holder 入口，按真实 decoder/filter 位置复用 payload residence、producer residence 与原所有权费用计算。CandidateCostPlanner 返回 frameOwnership 分项；最终 ResourcePlanner 将相同投影结果与实际 producer identity/readback 的原费用结果逐字段比较，不增加第二次费用或预约。重复/缺失生产者、filter成员不一致及不足最小进展均明确失败；没有另写快照保活算法。

裸指针控制块适配已实施：SharedAllocationGeometryProbe 增加独立控制块几何，StartupBatchStoragePlanner 调用它；Windows费用保持原式，GCC12.4使用实际 Derived* 控制块。两次普通分配不增加PMR头，不承诺RSS；门禁沿已审MSVC145/202604与GCC12/20240620、C++20非debug，其他环境仍Unsupported。尚待本轮构建及后续RKMPP编译/运行证明，不把源代码适配记作RKMPP验收通过。

本单元没有解除freeze空solver门禁；AU payload、根预算去重、候选搜索和封印后的最终对照仍未闭合。原r394失败保持，未重复无变化的起播失败测试。

本轮七源码冻结后，两名独立审查均明确PASS；Windows Release全量构建50580 configure/build均0、796目标，脚本核验产物。RKMPP新分支尚未编译/运行，不以Windows编译证明GCC实现。无新增实流通过项、local执行或临时测试产物。下一单元须对照SourceSession::drain实际预约的payload+padding+AVPacket+AVBufferRef，租约控制块已由候选单列，不重复计费。

AU费用单元已实现：MediaPreparedPacketStorage统一payload与已padding backing两种显式计费范围，SourceSession预约/结算和CandidateCostPlanner均调用；候选复用既有活跃租约根，返回最大同时预约数、每项预约上界和checked乘积。lease control、wrapper、协议assembly仍分项；snapshot别名不再收费，graph credit接替后的payload不归此准备租约域。Windows及固定RKMPP的packet.c:83–110均实读确认av_new_packet从空buf申请size+padding；只读核验未修改依赖。该上界不是FFmpeg私有内存或RSS承诺。

五源码双独立静态审查PASS，Release89221全量构建通过，r395四路原规格已运行并记录于realtime-composition-r395-validation.md：四路decoder绑定后在原freeze门禁exit1，无新增预约错误；完整起播仍预期停在未实现的freeze solver，不将该失败视为成功验收。

共享根预算观察已接线：复用[Linux cgroup v2 memory.current](https://docs.kernel.org/admin-guide/cgroup-v2.html)的父级包含子级归属原则，而非引入内核memory控制器。MediaStorageBudget在现有根/直接子共锁下取得一致快照，拒绝空、重复或异根子项，显式返回其他已持有额度；freezeBatch向solver传入该事实，并在solver返回后重读账本，以其他额度加各源完整候选费用核验容量。精确临时数组使用原准备resource，源锁→ledger锁顺序不变；canonical无solver路径不新增分配。观察不是未来headroom预约，未知其他root增长仍需最终完整费用合同闭合；输出/画布backing已经在root收费，不重复相加。尚待双审及本轮Release构建，完整solver与媒体门禁未通过。

本单元验证：两名未参与实现的审查者对三源码均明确PASS；Windows Release全核心clean-first/all构建会话21623成功，configure/build均0、797任务计划，脚本核验产物。未重复执行缺solver的已知失败实流，未生成媒体测试临时产物；不记为合屏验收通过。下一步接完整候选总额与容量选择：startup.route已含batch并发展开，不能再次相加；不同分配域与root中既有输出/画布backing须保持归属。RKMPP继续按已批准8核增量all执行，当前未编译新修改。

后续费用审计发现并修正两个分项：SourceSession每stream以真实MediaLineageAllocator<char>复制非空CanonicalIdentity，fixedOwner现按精确字符请求与独立custom-deleter control请求计一次；plan/binder/lineage随后共享同对象不重复计。几何依据本机MSVC memory:1833的_Ref_count_resource_alloc及远端GCC12.4 shared_ptr_base.h:945的_Sp_counted_deleter，两者使用实际char*、deleter、allocator类型，沿既有版本门禁。Candidate新增sourceClock，按候选outputObjects合计复用RTCP SourceClock planner计tracker/mapper/validator/binder和源队列CNAME；startup.clock只计追加图route部分，不代替此项。依据沿用[GStreamer allocation协商](https://gstreamer.freedesktop.org/documentation/additional/design/bufferpool.html)的明确allocator/metadata域，而非新建媒体链。
审计边界：生产decoder/filter registry使用Runtime域，已有FinalGraphResourceLedgerCompiler::runtimeStorage收费，不能加进source Prepared总额；probe registry则由root预约backing，归partition.other一次。普通released audio wrapper及release group key字符串不在source PMR，routeBytes不可整体当Prepared总额。尚缺ingress event包装对象的完整持有费用、完整费用总额与solver接线；本次不宣称这些已经闭合。四源码修改待双审及Release构建。

Ingress event下一费用边界已核验：SourceSession::event冻结后将原Prepared包装放入outputs；RawRtpInputNode::onProcess非packet直接emitOutput；MediaRtpClockGroupNode每stream持event/clock两个槽，processStream将其中一个move到局部selected，消费后销毁，不能把该move算成第三个长期槽。metadata evidence边容量沿MediaRtpClockRouteConnections；source构造期间与已发布旧对象可以并存。snapshot fanout持新group对象而非原ingress wrapper，应分域处理。此为待实现的持有边界，不是已计入总额。

本单元验证完成：四源码冻结后两名独立审查均明确PASS；Windows Release全核心clean-first/all构建74414成功，configure/build均0，脚本核验产物。未运行local或重复已知缺solver实流，无新增媒体临时产物。GCC几何仅完成目标头文件只读核验，尚未RKMPP编译/运行；完整合屏仍未完成。

Ingress event分项已实现：RtpClockRouteStoragePlanner从既有MediaRtpClockRouteConnections复用evidence边容量和每stream的raw待发送1、group event/clock槽2，显式返回maximumEvidenceObjects。零CNAME仍计算event上界。Candidate再加源outputObjects总和及源mutex串行构造1，按实际pmr::polymorphic_allocator<MediaRtpIngressEventBuffer>的allocate_shared请求几何计费；CNAME背板单列，不重复计入wrapper。Raw的旧pending清空前不会再次调用onProcess，Group将selected从原槽move出去而非额外第三槽。沿既有GStreamer bufferpool引用保活及明确allocator元数据设计，复用现有route，未增加运行线程/媒体链/公共字段。待双审与本轮Release构建；完整总额、容量选择、生产solver接线和实流仍未闭合。

本单元冻结验证：四源码获两名独立审查明确PASS；Windows Release全核心clean-first/all构建70798成功，configure/build均0，脚本核验产物。未启动媒体测试，当前CLI/FFmpeg/VLC无进程残留，未产生新的抓包/截图/媒体日志。未宣称实流或RKMPP通过，完整费用总额及solver接线仍待完成。

Startup费用归属与Store遗漏修正：原routeBytes混合Prepared与普通域，现移除歧义总数，按真实工厂将8个concurrent项汇入preparationBytes，releaseGroupKeyBytes及releasedAudioWrappersBytes汇入ordinaryBytes；不迁移实际allocator，不把普通费用冒充预算预约。依据是Envelope/Release/Transaction与reservation工厂的显式资源、GroupKey普通std::string、ReleasedAudioBuffer普通new，与既有GStreamer allocation域原则一致。共享CompositionStartupStoragePlanner补每stream的makeMediaLineageUnique<StreamStore>对象和m_snapshot的IndexedUnit精确背板，分别含资源header/alignment；generation产品已含其shared对象，不重复计。候选与最终kernel复用此计算，最终SourcePlan保留同一stores产品。
剩余确定缺项：Coordinator的selection与purge-ID数组及prior并存峰还未计入；冻结求解scratch亦须闭合。另源码确认StartupGenerationStateGeometry当前仅MSVC145/202604 Release，RKMPP的unordered容器几何仍缺适配；不能以已补shared_ptr几何宣称RKMPP全部可用。此次8源码待双审与Release构建，完整solver/实流门禁仍未通过。

下一单元源码边界：CoordinatorNodePreparation:235的Coordinator本体为额外Prepared unique对象，generation shared对象已另计。Coordinator::advanceGeneration返回旧store IDs，submit中的purge(prior,trailing)在旧prior背板仍存活时新建结果；tryRelease同时新建video/audio selections及purged，并保留传入prior直到返回。节点同步消费decision后才继续处理下一事件，返回数组移动不复制背板；需按这些实际作用域封闭峰值，不能用单个最大数组代替。

本单元验证：8源码冻结后两名独立审查均明确PASS；Windows Release全核心clean-first/all构建18614成功，configure/build均0，实际观测24个cl并行，脚本核验产物。无local执行、媒体测试或新增测试临时产物；未宣称实流/RKMPP通过。

Coordinator费用单元：新增独立MediaStartupCoordinatorStoragePlanner，接管旧Store/snapshot计算并补Coordinator本体、每成员selection数组、旧prior及新result ID背板。N为video+audio容量；advanceGeneration返回prior至多N，purge结果由prior、当前stores至多N和单个rejected输入组成，tryRelease仅取prefix且无trailing，所以以这些明确持有者保守求和，不采用经验倍率，也不宣称该上界紧确。每个数组分别计算实际header/alignment，移动结果不另计；Node在processStream/processClock内同步消费decision/poll结果，尚存结果不会跨下一调用积累。沿既有GStreamer引用保活、有限上游分配原则和当前状态机，不修改运行算法/公开字段。候选与最终kernel改传同一coordinator产品，旧store产品删除，避免同义费用入口。待双审/Release构建；完整source费用及freeze scratch、solver与实流尚未闭合。

Freeze scratch下一核验点：freezeBatch当前将ordered指针数组、unique_lock数组、source snapshots、budget owner数组及budget snapshots全部收费到首源ownerResource；solver返回的admissions精确背板也可能与上述同时存活。当前snapshot retained虽包含已分配scratch，但不能用该瞬时读数替代完整候选峰值；需将真实逐请求footprint及唯一归属作为求解事实，并明确返回数组resource/容量合同，不能把同一scratch加到每个源。

本单元验证：8源码冻结后两名独立审查明确PASS；Windows Release全核心clean-first/all构建58964成功，configure/build均0，新planner纳入798任务计划，脚本核验产物。无local执行或新增实流测试/临时产物。费用分项通过不代表总额、具体solver、Windows实流或RKMPP验收完成。

Freeze batch存储已规划并接线：独立BatchStoragePlanner按真实元素类型计算ordered、unique_lock、source snapshot、budget owner、budget snapshot五个精确数组及admissions结果数组，每请求含资源header/alignment。全部使用首源ownerResource，solve获得显式batch产品；调用时scratch已活跃，result仍是待分配费用。结果必须使用同一resource且size/capacity均等于源数。回读root时六数组均存活，源0的当前retained校验只扣除该batch总额，其他源不扣；全局核验为other+batchTotal+各源total，原snapshot不伪造、不重复预约。source totals合同明确排除batch；具体solver自身若增加Prepared scratch仍须计费，不能视本产品为任意求解算法开销保证。沿用已调查的层级唯一归属与显式分配域原则；锁序及canonical无solver路径保持。待双审及本轮Release构建，完整费用求值/容量选择与媒体验收未完成。

本单元验证：5源码冻结后两名独立审查明确PASS；Windows Release全核心clean-first/all构建78483成功，configure/build均0、新planner纳入799任务计划，脚本核验产物。未执行local或重复缺solver的已知失败实流，无新增媒体测试临时产物；本结论不代表完整source总额、具体solver或RKMPP验收。

候选总额已汇总：fixed沿既有fixed lease、fixedOwner及unmigrated protocol；protocol为每流container（已剔除旧pending控块）、全活跃lease控块、raw packet wrapper、payload预约，加source/route CNAME与ingress event；startup为index、generation、frame wrapper、lineage、frame ownership、route.preparation及coordinator.preparation。逐项以uint64 checked求和再核size_t，不截断。total=fixed+protocol+startup。batch scratch/root probe/output/canvas另计一次；ordinary route及graph自有payload/registry不混入。两名独立审查均明确算术/归属PASS，有限复核未发现新的确定Prepared漏项，但此项不是穷尽证明或solver/实流通过。
平台剩余：除startup hash几何外，MediaVideoFrameOwnershipStoragePlanner::reservedVector仍有MSVC专属门禁，RKMPP需在真实GCC能力证据下适配，不能以Windows编译替代。下步须用候选总额实现实际容量选择与freeze接线，并在最终图对照同一成本与持有产品。

候选总额验证：两源码冻结获双独立算术/归属PASS；Windows Release全核心clean-first/all构建94651成功，configure/build均0，脚本核验产物。未运行local或新增媒体测试，未产生新的测试临时文件。容量接线前再次核验SourceSession：admittedUnits只在非frozen阶段累计，冻结后prefix不新增；未来acquiring/output依旧是独立硬容量，不能由Nv或输出fps假定。当前仍无具体BatchAdmissionSolver，生产freeze传nullptr；本提交不是已恢复输出的成功验收。

## 候选与实际建图共用拓扑规划

将 GraphBuilder 内既有 aggregate/source/output/scheduler/protocol/execution 声明组合提取为 MediaCompositionTopologyPlanningKernel，真实 builder 已消费其 owned 产品并保持最终逐节点/边的 readback 校验。借用源 synchronization/policies/branches/inputs，不需要 sealed owner 或创建 MediaGraph；候选 solver 后续复用同一入口。此次不新增连接策略、线程或容量算法。依据仍为 [GStreamer allocation negotiation](https://gstreamer.freedesktop.org/documentation/plugin-development/advanced/allocation.html) 的协商后激活分层；本项目具体拓扑与持有上界来自既有 DAG planner，不声称 GStreamer 提供本项目的容量分配算法。候选临时产品的存储费用仍须在 solver 内解决，不将普通 vector 自动归入 Prepared 域。待双审与 Release 构建；freezeBatch 仍无 solver，本节不是实流通过记录。

容量接线定位：SourceSessionPlan.cpp 的 streamPlan 已按 descriptorCapacity+一次 expiry、maximumReorderedPackets、每包 discontinuity/evidence 及一次 clock expiry 推导输出事件界；这是有限批次容量，不承诺下一批会等待消费者。后续应抽取并复用该实际公式，不以 bootstrap 的 B/sizeof(MediaBufferRef) 当最终队列容量。源帧字节可复用 MediaVideoSurfaceFootprint::logicalBytes（SourceResourcesPlanner::videoBytes 已采用真实 stage frame contract），不得用 encoded AU 字节替代。Nv 与 acquiring/output 独立；Na 必须保持 session.audioRetention。producer.minimumProgress 含候选 acquiring，不能循环反推独立输入下界。双审已明确本拓扑提取单元 PASS，构建结果待写入。

本单元验证：presentation_plan_review 与 timing_path_review 对冻结三源码均明确局部 PASS。Windows Release clean-first/all 全核重建会话60850完成，configure/build均0，800任务计划，脚本核验产物。未运行local CLI；未重复启动缺solver的已知失败媒体测试，没有本轮抓包、截图或媒体日志。RKMPP尚未同步本次源码，继续使用8核增量all；不得据此标记跨平台或完整合屏通过。

## 准入候选的单次协议完成容量门槛

求解前核验发现：consume、tick和takeOutput共用源mutex，消费者不能在单次drain中途取走输出。protocol pending可含N个packet和N个discontinuity（不是总项数N），N=windowPackets+1；每packet至多A个AU和一次evidence，tick再有一次clock invalidation。因此以N*(A+2)+1作为保守单次输出增量上界，并由SourceStoragePlanner统一计算，核对workspace的N/A与实际reorder/envelope一致及全部加乘溢出。CandidateCostPlanner与freeze提交校验均消费该门槛；不改变接收算法、线程、原错误、旧canonical路径或外部参数。

这是空队列可容纳一次保守完成结果的充分容量门槛，不是数学上不可缩小的必要最小值；既有占用和连续多批仍可能触发原有限容量失败。descriptor batch公式覆盖不同持有范围，不能当单次源锁边界。行业参考 [GStreamer queue](https://gstreamer.freedesktop.org/documentation/coreelements/queue.html) 的独立对象/字节/时间限制与背压；本项目差异是原UDP会话超限显式失败，不能在持有消费者同一mutex时等待排空，未引入阻塞或丢弃fallback。此上界来自实际协议状态机，不以输出fps、Nv或旧prefix推导输入密度。

具体solver仍未实现。剩余设计风险已定位：多源独立容量扩展目标尚无现成策略；候选source/topology产品内部普通vector/string不能靠外层PMR收费。应贯通真实分配scope或复用提前拥有的不变声明，不能事后读capacity冒充申请前准入。此次不声称解决这些项。源码冻结待双审与Release构建，无媒体测试产物。

本单元验证：timing_path_review与presentation_plan_review均对冻结4源码明确局部PASS；Windows Release clean-first/all全核重建会话85168完成，configure/build均0、800任务计划，脚本已核验产物。未运行local或重复缺solver的已知失败实流，无本轮抓包/截图/临时媒体。完整solver、Windows实流及RKMPP仍未通过；远端后续按8核增量all。

## RKMPP 身份通道补充核验（2026-11-05）

独立SSH审查固定MPP c08762ebfadeb4e986d2fed993bc7a54862d3ebe，无文件修改。已找到两个必须排除的反例：mpp_dec_normal.c:638将输入PTS入排序链，mpp_dec.c:351在sort_pts开启时用链头覆写当前frame PTS；mpp/vproc/mpp_dec_vproc.c:301、556用(prev_pts+curr_pts)/2生成去隔行帧PTS。因此不能直接启用递增token并宣称任意输入身份可逆。HEVC非split的h265d_api.c:139→h265d_dpb.c:130以及H264的h264d_api.c:557→h264d_parse.c:875/1048→h264d_init.c:466存在按画面传递路径，但field合并、输出倍增与错误路径仍须核验。FFmpeg未设置sort_pts不等于已证明实际值为0；当前config对象初始化路径还需封闭。后续仅在权威证明禁用排序、无PTS插值并具精确rescale边界后，才能实现有界身份映射adapter；不采用提交FIFO或近似PTS匹配。

随后有限核验补齐初始化链：mpp.c:104→mpp_dec_cfg.c:98–101→kmpp_obj.c:1181–1185/1070/1116–1120→osal/mpp_mem_pool.c:239–269；用户态normal-pool的新建和复用对象均memset清零，default initializer不改sort_pts，故该固定路径初值为0。此结论未覆盖kernel-object分支或后续控制调用，尚无实际decoder config readback。FFmpeg rkmppdec.h:96有deint选项（默认1，可显式0），rkmppdec.c:291调用MPP_DEC_SET_ENABLE_DEINTERLACE且失败报错；MPP mpp_dec.c:1014–1016写enable_vproc，但mpp_dec.c:65/69允许进程环境enable_deinterlace覆盖。可继续沿既有decoder配置显式传deint=0并核验实际CLI环境，不得仅凭AVOption回读宣称vproc已关闭。独立SSH当前该环境变量未设置，这不是未来进程的证据。

当前RKMPP构建进度：[236b73d5增量记录](realtime-composition-rk-incremental-236b73d5.md)。远端已同步该源码，8核增量all完成457任务、退出0；realtime --help与动态库路径检查通过，源码前后哈希一致，临时同步文件已清理。不是媒体验收；SourcePresentation联合solver、RKMPP身份/接收调度、GNU专属存储几何及Windows→RKMPP完整实流仍未闭合。

## RKMPP 完成检查的显式消费周期

沿现有decoderReceiveInterval→node option→VideoDecodeNode的接收循环和等待机制，内部MediaVideoSourcePlanningOptions新增consumerFrameRate事实，由CompositionPreparer传入已声明输出cadence。SourcePresentation的RKMPP完成检查按该周期安排；canonical源保留观察输入cadence，其他后端仍不轮询。缺少消费事实或非正值明确失败，无新增CLI/API/profile字段。probe的局部waitCadence重命名waitReceiveCheck，取消、原deadline及send/receive调用未改。

调查依据：[GStreamer clocks](https://gstreamer.freedesktop.org/documentation/application-development/advanced/clocks.html)区分源时间戳、公共运行时钟和sink延迟；本项目复用已有deadline唤醒，不复制其完整latency协商。此周期仅为异步完成再次检查间隔，绝不参与Cv/Nv、输入密度或媒体PTS推导；不是实时完成SLA。VideoDecodeNode当前按now+interval重查，与aggregate的target-preparationLead没有相位同步，硬件send/drain阻塞、下游filter/credit等待也不受该周期约束。独立设计审查仅对有界重查用途PASS，对消费deadline保证FAIL；后者不是本次声称的能力，仍需实际链路证明。

COPY_OPAQUE probe门禁、SourceAccessUnit传播校验及SubmissionOrder身份缺口均未放宽；此修改单独不能使RKMPP SourcePresentation可运行。源码冻结待双审和Windows Release构建；之后仅增量编译RKMPP，媒体验收仍须Windows先通过。

Windows验证：Release clean-first/all全核重建会话38286成功，configure/build均0、800任务计划，脚本核验产物。两名独立审查者对6源码明确局部PASS；构建结束后只将probe旧source-cadence注释改成planned receive-check，无可执行代码变化。尚未执行媒体测试，不宣称RKMPP身份映射、输出deadline或合屏门禁通过。

RKMPP编译验证：源码c0d3e891233ddb1684a5b81f1634b9273d6f649d；同步前1,937文件均匹配236b73d5基线，仅更新6文件，构建后1,937文件均匹配新提交。远端仍在/home/tang/MediaTranscode-composition-09093919，实际执行source /etc/profile.d/ffenv.sh && ffenv on && source /opt/mt-tools/mtenv.sh && mtenv on && cmake --build out/build/rk-release --target all --parallel 8，保留缓存，109/109任务完成、退出0。

实际执行/home/tang/MediaTranscode-composition-09093919/out/build/rk-release/media_transcode_realtime_video_cli --help，退出0；仍有mpp[536116]: mpp_platform: client 12 driver is not ready!诊断，不当作媒体能力通过或全部硬件不可用。ldd六个FFmpeg库均位于/home/tang/ffmpeg-ab1e61a/lib，无not found；realtime产物SHA256为8711f9b0bfde3c0057850cfde4c546a8d18e295cc21c5213011f2b940ad6a381。pgrep无cmake --build/ninja/cc1plus/realtime CLI残留。未运行local CLI或任何媒体链路，不替代Windows先行实流门禁。

本轮临时同步文件为本机及远端项目out/rk-incremental-c0d3e891.json、out/rk-incremental-c0d3e891.tar，各2文件；结果已记录，随后按清单删除。

清理复核：本机与远端本轮同步文件均剩余0；保留源码、构建缓存和指定120秒测试源。最终6源码双独立审查均PASS。

## GCC 帧所有权 vector 存储适配（2026-11-05）

本轮目标机核验：GCC 12、__GLIBCXX__=20240620、C++20、未启用_GLIBCXX_DEBUG。实际vector.tcc:65–100的空vector reserve(n)精确申请n个元素；stl_vector.h:1911的max_size还受PTRDIFF_MAX/sizeof(T)约束。官方对照：https://github.com/gcc-mirror/gcc/blob/releases/gcc-12.4.0/libstdc%2B%2B-v3/include/bits/vector.tcc 。生产MediaFfmpegPayloadOwnership的references和replacements均为空vector一次reserve，使用原LineageAllocator。

MediaVideoFrameOwnershipStoragePlanner的reservedVector增加上述供应商限定分支，复用原request的资源头/对齐计费；空vector不计背板或MSVC proxy。Windows原分支不变，未知供应商继续unsupported。不改变线程、媒体状态机、背压、外部参数或依赖；不把逻辑准备存储解释为RSS/GPU上界。presentation_plan_review与timing_path_review均独立明确局部PASS；Windows Release构建会话25629进行中，随后RKMPP仅8核增量all，尚无本修改实流通过结论。

## 完整接线与 RKMPP 身份方案复核

当前freezeBatch仍传nullptr，完整联合solver尚未实现。独立审查确认不能仅缓存拥有型topology后套用候选成本：connections.policy、positions.incidentCapacities和residence数量随容量变化；SourcePlanningKernel仍复制拥有型vector/string。下一完整单元应同时交付预算内候选工作区、共用的容量数值填充、具体solver、真实freeze接线及最终readback，避免再增加无生产消费者的helper。预先分配也必须计费，不能藏入已有六数组费用或在root other中重复收费。

RKMPP身份替代方案明确FAIL：固定FFmpeg会丢弃discard/errinfo frame而不导出对应身份；输入消费回调不能证明重排输出已结束，故逐AU表不能按首次输出或输入消费安全退休。无表64-bit PTS编码同样不足：MediaVideoDecodeLineage含独立AU sequence、扩展RTP时间及逐AU不可变clock projection，映射anchor/slope可变化。不能用当前时钟重建历史投影、截断位宽或假设AU=frame。保留现有身份门禁；不修改外部FFmpeg/MPP依赖，不宣称该缺口已解决。
本修改验证：Windows Release clean-first/all全核构建25629终态退出0，800任务计划，configure/build均0；产物已核验。RKMPP在原目录使用8核增量all完成4/4任务，无clean-first，源码单文件同步前后哈希核验通过。没有媒体测试、抓包、截图或临时同步文件；合屏完整准入、RKMPP身份与GNU其他容器几何仍未完成。
RKMPP构建退出0，realtime二进制SHA256=f739913039e46e6f46e1fb01710f566edc3258405c737a93f33e40924f1279cc；ninja/cc1plus无残留。

## 候选工作区的传递分配审计（2026-11-05）

本轮重新沿生产候选费用调用树核验，发现仅缓存topology不能满足完整scratch边界。MediaPreparedSourceCandidateCostPlanner::plan还调用以下拥有型求值链：

- streamCosts→MediaPreparedPacketWrapperStoragePlanner：每个raw/binder creator生成roots，holder内仍拥有retainingPositions。
- MediaSourceLineageStoragePlanner：四个allocation各持roots；display复制frameWrappers、accessUnit复制packetWrappers并追加display。FrameWrapperStoragePlanner再次复制frame roots。
- 两者以及MediaSourceFrameOwnershipStoragePlanner都调用MediaCompositionPayloadResidencePlanner::plan；该函数重建filtered connections/owners，再由MediaGraphPayloadResidencePlanner::plan构建reachablePositions与contributions，贡献复制内层retainingPositions。
- CandidateCosts自身保留上述packetWrappers、lineage、frameWrappers，故这些不是函数返回前全部释放的临时项；startup/clock/ownership数值产品不能代表候选对象的实际分配总额。

实施边界修正：一次性结构产品必须同时包含连接身份、producer可达位置、唯一holder身份及root到holder的关联。容量变化只更新相同身份上的数值；媒体域的可达关系不由容量大小决定。数值求值应由现有生产planner共用，并保留原来的重复身份、角色、stream/payload和溢出校验。最终sealed证据仍应生成并对照完整root贡献，不能为了无分配而删除审计信息。候选工作区及选中证据的重叠峰应显式收费，不能仅对外层vector使用PMR或事后把估计值塞入source.total。

因此，上轮“拓扑数值更新”只是完整接线中的一部分，不能单独据此解除freeze门禁。本轮没有修改生产源码、启动已知必失败的媒体链路或运行无源码变化的构建；没有生成临时测试产物。
独立审查presentation_plan_review确认上述传递分配缺口。补充约束：零references仅更新数值，不应删除预编译证据槽；无分配只约束成功候选求值路径，错误诊断字符串仍需保留明确失败边界。最终实现必须让CandidateCostPlanner及具体solver实际消费共用数值路径，不能只增加工作区类型。

## 帧 wrapper 数值求值去重（2026-11-05）

按上一节的传递分配审计，MediaPreparedFrameWrapperStoragePlanner不再将FrameWrapper family的roots及嵌套holder位置数组复制进返回产品，而是同步遍历输入lineage中的原证据；保留全部角色/来源/构造次数/溢出校验与费用算法，返回sourceIndex和两个费用数值，不存借用指针。CandidateCosts本身拥有完整lineage，最终账本的MediaSourceFrameOwnershipStorageEvidence同样保存lineageStorage，根证据并未丢失。全仓消费者核验未发现读取旧frameWrappers.roots的其他路径。

设计沿C++ Core Guidelines F.16对只读大对象使用const引用的原则：https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#f16-for-in-parameters-pass-cheaply-copied-types-by-value-and-others-by-reference-to-const 。不是新的媒体算法，不改线程、背压、节点行为、外部输入或平台adapter。此修改实际同时作用于候选费用与最终图费用消费者；仅消除一处深拷贝，不宣称整个候选链无分配。Windows Release全核构建9501进行中，双独立源码审查中。
验证结果：presentation_plan_review和timing_path_review均对冻结2文件明确PASS。Windows Release clean-first/all全核构建9501终态退出0，800任务计划，configure/build均0；RKMPP同目录8核增量all完成6/6任务，两个源码文件覆盖前后哈希一致。未执行local或realtime媒体链路，未生成抓包/截图/临时同步文件，不能作为实流成功提交；完整solver及其余候选动态分配仍未完成。
RKMPP退出0，realtime二进制SHA256=68f61b8ce7d481f0dea5d249b6a70f42358f610697dbbdbf318fec00907e00f0，ninja/cc1plus无残留。

## 可达持有量的固定工作区求值（2026-11-05）

MediaGraphPayloadResidencePlanner将原有广度遍历及checked持有量累加提取到evaluate；调用者显式提供至少nodeCount项的size_t工作区，成功求值不分配，返回借用的有效位置前缀与总量。原plan实际调用该实现，再按共用retains谓词物化原贡献列表，供现有composition与最终图消费者继续核对。工作区不得别名topology，返回视图有效期受调用方控制；失败可修改工作区。原索引校验先于拥有型工作区分配，超max_size明确失败，错误诊断仍可能分配字符串。

行业对照为Boost Graph官方BFS文档：https://raw.githubusercontent.com/boostorg/graph/boost-1.85.0/doc/breadth_first_search.html 。沿用原FIFO发现次序、发现时去重，不改变媒体/所有权算法；共享owner有任一位置可达即仅收费一次。当前仍按边表及已发现位置线性扫描，未获得或宣称Boost邻接表实现的O(V+E)复杂度。成功路径工作区由调用者所有，无新增线程/队列/平台adapter/公共配置。

presentation_plan_review和timing_path_review均对冻结2文件明确局部PASS，覆盖环、自环、重复边、共享owner、零值、溢出及span生命周期。Windows Release全核构建68306进行中。完整候选仍需连接身份/根结构预编译和共用数值消费，现有拥有型plan仍分配最终证据；不能据此声称整个CandidateCostPlanner已无分配或solver已接通。
构建结果：Windows Release clean-first/all全核68306终态退出0、800任务计划，configure/build均0，realtime产物7117824字节；RKMPP原目录8核增量all完成20/20任务、退出0，realtime SHA256=1de0ab5492087df273bec1710192298df067d53c7124f4ec72db3028e8712b53。两源码文件同步前后哈希校验通过，远端ninja/cc1plus无残留。本轮没有实流测试或抓包/截图/临时同步文件；本次共享规划路径修改仍须随完整solver接通后的Windows→RKMPP同规格真实链路复验，不把构建当媒体验收。

## 帧所有权复用已规划持有者（2026-11-05）

沿前两节同一BFS及持有者证据算法，FrameWrapperStoragePlanner在原root校验/费用遍历中同时累计decoderReferences和可选filterReferences；只计FrameHolder，不计Construction，并校验holder存在及数量一致。SourceFrameOwnershipStoragePlanner消费该数值产品，继续用原ProducerResidence进展下界/额外持有量和原几何费用算法，不再为decoder/filter分别重建filtered topology、BFS贡献和嵌套位置数组。不是新的媒体算法，不改变线程、背压、节点、默认参数或平台adapter。

CandidateCostPlanner和最终ResourcePlanner均已实际接入。最终仍独立从真实图producer reference计算ownership，并与该数值投影比较；上游lineage继续拒绝重复producer，完整根证据照常保留。六源码经presentation_plan_review与timing_path_review分别明确局部PASS。整个候选链仍有其他拥有型分配，完整solver未接通；不能把局部去重当完整准备内存保证。

Windows Release全核构建61344进行中。远端六文件基线先核验一致，随后SCP新连接被远端关闭（退出1），改用已存活SSH会话传输压缩内容并前后校验哈希，无临时同步文件、不修改外部依赖。
构建完成：Windows Release clean-first/all全核61344终态退出0，800任务计划，configure/build均0。RKMPP原目录8核增量all完成29/29任务、退出0；六源码同步前后哈希通过。realtime SHA256=a66db74e8bc354524604e42d804c193944bea8fb7ca9611cb00f1592d95ef880，ninja/cc1plus无残留。本轮无媒体测试或临时测试文件，未运行local CLI；共享规划改变仍待完整solver接通后的同规格Windows→RKMPP实流验证。

## Composition residence 直接借用原拓扑（2026-11-05）

沿既有Boost BFS发现/去重语义，新增同步只读TopologyView，slot身份稳定，不属于当前媒体域的连接/owner返回空项。OwnerView对单位置内联保存，对共享位置借用原span，避免借用局部索引。通用evaluate消费view，原拥有型topology经栈adapter复用同一实现；composition按已有stream/payload、typed startup和ordinary/shared owner公式直接读取原事实，不再创建filtered connections、owners及每个owner的临时位置vector。

slot总数checked累加，节点索引校验、零值与缺失区分、startup乘法溢出、shared owner计费一次和贡献顺序保留。plan仍物化独立拥有的最终贡献证据；成功view读取及evaluate不分配，不等于整个候选调用树无分配。lookup在BFS扫描中重复执行，可能增加CPU成本，尚无性能改善证据，后续可在收费结构编译阶段固定索引；不改变媒体线程、背压、失败门禁、平台adapter或公共输入。

presentation_plan_review与timing_path_review均对冻结3源码明确局部PASS。Windows Release全核构建33748进行中；完整solver仍未接通，不能把借用视图视作完整候选工作区准入完成。
构建结果：Windows Release clean-first/all全核33748终态退出0，800任务计划、configure/build均0，realtime产物7119360字节。RKMPP原目录8核增量all完成20/20任务、退出0，三源码同步前后哈希通过；realtime SHA256=cb0c3e09849469d63c6d099b76b63734fe063500a857039909f05fb7470b3138，ninja/cc1plus无残留。无本轮媒体测试、临时同步文件或测试产物，未运行local CLI。共享规划变更仍需随完整solver接线后进行同规格Windows→RKMPP实流验证，不作为媒体验收成功记录。

## Lineage 根证据目录共享（2026-11-05）

继续落实候选工作区审计中的一次保存结构证据：MediaSourceLineageStoragePlan拥有单一roots目录，各allocation family只保存rootIndices。原frame→display、packet→accessUnit、display→accessUnit关联改为复制索引；目录仅追加，不按名称或数值合并不同creator的根。每family仍按原成员顺序/次数分别使用其几何计费，共享证据不等于跨family去重费用。

FrameWrapper消费者先检查索引再读取完整root；最终Candidate/账本仍拥有完整lineage。索引随完整产品copy/move保持自洽，目录扩容不会悬空，追加失败销毁局部未发布产品。沿前述只读事实复用与原BFS算法，不改线程、队列、媒体状态机或平台adapter；这不是新持有量算法。family索引数组和catalog仍有动态分配，尚不能据此声明scratch全额准入或完整solver通过。

三源码冻结，presentation_plan_review已明确局部PASS，另一独立审查进行中；Windows Release全核构建88185进行中。
构建结果：Windows Release clean-first/all全核88185终态退出0，800任务计划、configure/build均0，realtime产物7116288字节。RKMPP原目录8核增量all完成8/8任务、退出0；三源码同步前后哈希通过，realtime SHA256=506925b9edaa89e598331d767863c452f680965657bb3c12988c175f71df88d9，ninja/cc1plus无残留。无本轮媒体测试/临时同步文件/测试产物，未运行local CLI；共享规划变更仍须随完整solver接通后的原规格Windows→RKMPP实流复验。
timing_path_review随后对相同冻结3源码明确局部PASS，双独立审查均通过；无修后源码变化。

## 固定 MPP 的输入 metadata 传播边界（2026-11-05）

为判断项目内hardware adapter能否在不修改依赖的情况下解决身份问题，timing_path_review独立SSH只读核验/root/mpp，提交c08762ebfadeb4e986d2fed993bc7a54862d3ebe。结论FAIL：现有公开H.264/HEVC路径未提供可证明的输入opaque传播及逐输入退休合同。

- mpp/codec/mpp_dec_normal.c:1095–1100写入输出frame的KEY_INPUT_PACKET处于advanced thread；同文件1148–1159仅为MJPEG启用该线程。H.264/HEVC使用parser/hal线程，不能借MJPEG语义证明其能力。
- mpp/base/mpp_packet.c:143–145复制packet时保留metadata引用，不代表parser将其传播给输出frame；未找到H.264/HEVC对应传播实现，KEY_USER_DATA存在也不足以建立合同。
- mpp_dec_normal.c:375–381在parser prepare后释放已消费packet，实际parser parse在506；packet自身release callback只证明输入内存生命周期结束。decoder packet-release在115–117先deinit再回调，而mpp_packet.c:213已清空指针。
- mpp/codec/mpp_dec.c:426–440的frame-ready发生于复制并发布frame后，缺少输入身份及每输入完成标志，不能与packet-release计数合并推导零输出/多输出/延迟输出的退休。

决定：不实现依赖上述未经证明能力的项目内adapter，不以FIFO、PTS猜配或MJPEG分支放宽H.264/HEVC门禁，也不扩展用户拒绝的外部依赖修改。RKMPP身份部分仍未解决；Windows完整准入solver仍有独立可推进工作（当前CompositionPreparer:297仍传nullptr），因此总体目标保持未完成。此轮仅补充依赖能力证据，没有生产源码变更、构建或媒体验收；独立SSH已退出，无临时测试文件。
## Lineage 数值重求值（2026-11-05）

MediaSourceLineageStoragePlanner::evaluate复用已有roots/rootIndices，只重新计算四类数量、费用及总额。固定数组从零暂存，family顺序、索引范围、holder与root数量一致性和全部checked算术成功后才更新数值。原plan已消费该函数，不保留第二套公式；重复调用不叠加旧结果，失败不提交部分数值。

沿既有供应商几何与checked计费算法，不改变根成员、所有权、媒体线程、队列或平台策略。实际geometry使用type_identity allocator重载的sizeof/alignof，并非无参的动态probe；成功路径不分配或重建根。错误诊断仍可能分配/抛出，evaluate并非noexcept，原plan保留bad_alloc捕获。完整候选的容量更新、其他组件重算及联合solver仍未完成。

presentation_plan_review和timing_path_review均对冻结2源码明确局部PASS。Windows Release全核构建85561进行中，后续RKMPP按8核增量all。
构建结果：Windows Release clean-first/all全核85561终态退出0，800任务计划、configure/build均0，realtime产物7117824字节。RKMPP原目录8核增量all完成8/8任务、退出0；两源码同步前后哈希通过，realtime SHA256=8a446a465aa1298eb77b2fa1bcedc7b6fdcb487afc45761b73d67b39c406ad0b，ninja/cc1plus无残留。本轮无实流测试、临时同步文件或测试产物，未运行local CLI。共享费用重构仍须随完整solver接通后的同规格Windows→RKMPP实流复验，不作为媒体验收通过。

## Packet wrapper 数值重求值（2026-11-05）

MediaPreparedPacketWrapperStoragePlanner::evaluate读取已存在roots，从零累计数量，并用原typed geometry、allocation footprint和checked乘法计算费用；全部成功后才替换maximumWrappers/preparationBytes，失败保留旧数值。新增stream及GraphHolder/holder数量一致性校验，原plan实际调用同一函数，因此Candidate与最终ResourcePlanner同步复用，无第二套公式。

沿前述原持有者计费算法与供应商sizeof/alignof事实，成功求值不构造或复制根，不分配；错误诊断不承诺无分配/noexcept。不改线程、背压、媒体节点或外部参数。完整候选的容量更新和联合solver仍未接通。

presentation_plan_review和timing_path_review均对冻结2源码明确局部PASS；Windows Release全核构建83477进行中，随后RKMPP按8核增量all。
构建结果：Windows Release clean-first/all全核83477终态退出0，800任务计划、configure/build均0，realtime产物7117824字节。RKMPP原目录8核增量all完成6/6任务、退出0；两源码同步前后哈希通过，realtime SHA256=25d6f6ee21eadd3860aa4b206741da7d2082c47d5b16698abc0fbbb5e851b0e8，ninja/cc1plus无残留。本轮无实流测试、临时同步文件或测试产物，未运行local CLI；共享费用重构仍须随完整solver接通后的同规格Windows→RKMPP实流复验。

## 2026-11-05 GCC 普通容器存储回读

已补齐 MediaStandardContainerStorageReadback 的目标 GCC 12.4 分支：普通 string 的 SSO 不产生独立 backing，长串按 retained capacity+1 计费；fresh copy/range 按 count+1，先校验实际 max_size；普通 vector 沿用 capacity×sizeof(T)，非 debug libstdc++ 无独立 iterator proxy。门禁限定 _GLIBCXX_RELEASE=12、__GLIBCXX__=20240620、C++20、新 ABI、非 debug；Windows 原 MSVC 145/202604 逻辑与拒绝范围保持等价。没有变更外部依赖。

依据目标机实际 basic_string.h/tcc、vector.tcc，以及 [GCC 12.4 官方 basic_string.tcc](https://raw.githubusercontent.com/gcc-mirror/gcc/releases/gcc-12.4.0/libstdc++-v3/include/bits/basic_string.tcc) 的 forward 构造→_M_create(count,0) 与 [vector.tcc](https://raw.githubusercontent.com/gcc-mirror/gcc/releases/gcc-12.4.0/libstdc++-v3/include/bits/vector.tcc) 的实际 allocation。SSO 容量由空 string 读取，不引入媒体经验常量。费用只表示普通 allocator 请求，不包括 PMR header 或物理 heap 开销；不能用于推导候选构造峰值。

真实消费者为 RTP 配置、源会话普通容器及 startup batch。此前这些调用在 GCC 会被 proxyBytes 的 MSVC-only 门禁拒绝。本次不是新的单独媒体链路，也未开放尚未审计的 PMR deque/vector 或 startup hash。

presentation_plan_review、timing_path_review 均对该单文件源码明确 PASS。RKMPP 已按修改前 SHA256 校验后同步该文件，8 核增量 all-target 构建 6/6、退出 0；realtime CLI SHA256=94157a31e7ec5c15fae6446e63c6f7abb6028e2ecf3d55e1664470375e1d2caf。目标预处理重新确认 __cplusplus=202002L、__GLIBCXX__=20240620、release=12、ABI=1，未定义 debug。Windows Release 全量构建尚在等待终态，后续结果追加于本节。

未运行新媒体测试，未创建抓包/截图/媒体/测试脚本；直接 SSH 同步未生成传输临时文件。源码与构建结果不证明 solver、正预缓冲或 RKMPP 合屏通过。容量策略分歧已按补充工业调查收敛为设计条件 PASS，当前准确接线约束见 realtime-composition-storage-solver-execution.md；具体 solver、工作区和原规格实流仍待完成。

本次 Windows Release 全量构建 16947 已终态退出 0，800 任务计划，configure/build 均成功；CMakeCache、build.ninja 与两 CLI 产物已核验。未运行 local CLI。两位独立审查者对本轮单源码及两份文档整体修改明确 PASS；该结论仍不覆盖未实施的 solver 或媒体效果。

## 2026-11-05 固定引用目录与重复求值接线

本轮十个源码文件将 lineage/packet 目录构建接到 rebind→evaluate，保存原 projection slot 并保留可达零 owner；实际最终图回读仍显式 LiveOwners。每个零 frame holder 仍检查 creator/source/投影身份，Construction 保持 1；静态槽异常直接失败。成功更新只改数字，原目录、family 索引及持有位置不增删。失败后必须丢弃候选，不保证部分更新回滚；当前 plan 不发布失败结果。

沿用已有基于拓扑的保守持有数量与 [Boost BFS](https://raw.githubusercontent.com/boostorg/graph/boost-1.85.0/doc/breadth_first_search.html) 的先遍历可达位置再收集贡献语义，没有改变网络、调度、线程或平台 adapter。固定拓扑/媒体域/producer/owner membership 是调用前提；当前接口不能证明任意替换图相容。

presentation_plan_review 与 timing_path_review 已对十文件给出局部 PASS，完整 solver、固定工作区预算和实流门禁仍开放。Windows 全量 Release 构建 25629 及 RKMPP 8 核增量构建已启动，结果待终态补充。本轮未启动新媒体测试、未创建测试脚本/抓包/截图等产物；远程逐文件校验原始 SHA256、写入后十文件新 SHA256 全部匹配，未生成传输临时文件。

Windows 全量构建 25629 已终态退出 0，800 任务计划，configure/build 均为 0；四项产物核验通过，realtime CLI 7121920 字节、14:03:00。首次 RKMPP 构建命令发出后 SSH 74693 被对端 reset，结果未取得，不记作成功。重新连接 51255 检查无 ninja/cc1plus/cmake 构建进程，产物仍为上一版 13:49:37；十文件新 hash 再核验一致后才重新启动增量构建。

重连后的 RKMPP 8 核增量 all-target 构建完成 20/20，退出 0；realtime CLI SHA256=e01866efbe155b3451c6ff06b3a2d03ee59a2450775c184aef6a94c80cf78b80。首次连接中断记录保留。当前验证只覆盖双独立源码审查与两平台编译，没有新媒体运行或截图验收结论。

## 2026-11-05 持有数量编译与固定目录更新

本轮五个源码文件（含新增 MediaCompositionPayloadResidenceCompiler.cpp）将原算法迁入 compileReference 的构造/更新两种编译期模式。两个入口共用 source 成员检查、typed startup 数量规划、精确 route/policy 校验、aggregate 身份和普通节点/encoder checked 费用；更新模式核对完整目录身份及最终游标，成功时不新增动态分配。失败可能部分更新，因此丢弃整个候选；同一固定 topology/role mapping 仍为前置条件。Topology::position 供原 graph builder、view、compiler 共同使用。

依据仍为既有 typed ownership/保守持有数量模型及 GStreamer 构建前静态分配协商；未改变运行时线程、媒体队列策略或平台 adapter，没有新媒体算法。presentation_plan_review、timing_path_review 均对五文件明确局部 PASS；这不替代 solver、工作区或实流验收。

Windows Release 全量构建 79994 已终态退出 0（801 任务计划，configure/build 均为 0），四项产物核验；realtime CLI 7119872 字节、14:16:38。RKMPP 交互 SSH 51255 在传输过程中再次 reset，未取得同步成功，未据此启动重复构建。单次 SSH 90157 检查四旧文件仍是基线、第五新文件不存在；随后 99117 以原 hash/目标路径校验完成五文件写入与新 hash 核验，再启动 8 核增量 all-target 构建，待终态记录。

本轮没有新实流、抓包、截图或测试脚本，也未生成同步临时文件。完整 solver 尚未接到 freezeBatch，保持原门禁。

单次 SSH 构建 99117 已终态退出 0，RKMPP 增量完成 22/22（包含新增 compiler 对象）；CMake 因新增源自动重新配置成功。此前交互连接中断记录保留，不算构建成功。没有执行新媒体链路。

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
