# 合屏源显示时间修复：r393 后续实施边界

状态：调查完成，尚未实现或验收。生产基线 ed1a22ae；目标仍为 Windows 后 RKMPP 的完整多路合屏，不降低 r393 的四路异构规格。

## 已核验的根因与复用位置

- r393 的 HEVC RTP 时间戳回退被 uint32 差值转换为超大正间隔，继而产生错误的准备容量。详见 [r393 实测](realtime-composition-r393-validation.md)。[RFC 3550 §5.1](https://www.rfc-editor.org/rfc/rfc3550.html#section-5.1) 允许重排媒体的 RTP 时间戳不按发送次序单调增长；取绝对值或只收集正差仍不能证明固定帧间隔。
- 合屏入口 `MediaRealtimeCompositionInputPreparer.cpp` 当前强制 CanonicalIntervals。统一目标是 SourcePresentation：保留 AU 身份和原始 PTS，解码后形成 SourceDisplayPoint，由已有 aggregate 的独立输出时钟选帧。不得按 H264/HEVC 建立两条处理链。
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
| MediaPipelinePlanner.cpp:347、365 | RKMPP 目前规划 SubmissionOrder，并要求输入 cadence 规划 receive interval；这两项均与统一 SourcePresentation 尚不兼容 |
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

实施顺序调整：先统一入口并在 Windows 验证既有 SourcePresentation 产品，以真实运行暴露共享链路缺口；随后完成 RKMPP 身份及调度 adapter。原因是 Windows 的 COPY_OPAQUE 路径已存在，无需等待 RKMPP 能力调查才能获得共享链路证据。代价是本地开发中间态的 RKMPP 合屏会被现有无 cadence 校验拒绝；不得将此状态交付为跨平台完成，也不得回退到错误 cadence 路径掩盖缺口。

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
