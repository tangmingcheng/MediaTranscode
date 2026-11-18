# 非 cadence 合屏源容量与启动接线

状态：当前源码调查完成，设计尚未闭合，不开放生产入口。r354/r355只覆盖已知cadence输入，不能证明本项。

## 当前三个生产阻断

1. MediaRealtimeCompositionSourcePlanner开头拒绝非MediaObservedVideoCadence，随后又检查raw.video.frameRate；构造surface时把该rate交给MediaRealtimeGraphResourceLedgerPlanner。后者以productionRate×residence推导数量。该ledger在这里仅提供源队列/启动中间产品，最终合屏准入已另按实际DAG producer/holder计数，不能把旧ledger要求误认为最终预算必需依赖fps。
2. MediaPreparedRtpSourceSessionPlan只有cadence分支生成continuousRetention。SourcePresentation分支保留timing identity和有限AU admission，但SourcePlanner随后要求continuousRetention。当前admission数量来自sharedCapacityBytes/sizeof(MediaBufferRef)，是压缩引用数量上界，不能直接当decoded帧队列数量或实际内存可用帧数。
3. MediaAvSyncStartupPolicyPlanner::makePolicy固定IntervalCoverage，并将compressed retention的unit bytes/count用于startup。显示点启动需独立的decoded产品，不能只改readiness枚举、把压缩AU数量当帧数，或以输出fps填充未知输入fps。

上述断点由root和publication_identity_review_b独立核对当前源码；不是实现或运行PASS。

## 复用范围与工业依据

沿用[GStreamer bufferpool](https://gstreamer.freedesktop.org/documentation/additional/design/bufferpool.html)的实际allocation几何与min/max引用数量协商、owner随引用生命周期保留的原则；该文档不提供未知帧率的最大密度。沿用[GstAggregator](https://gstreamer.freedesktop.org/documentation/base/gstaggregator.html)的各源队列与有序事件/flush处理原则，不另造每codec媒体链。遵循[FFmpeg send/receive](https://ffmpeg.org/doxygen/trunk/group__lavc__encdec.html)的输入/输出解耦：输入AU数不能未经codec能力证明换算成输出帧数。

已有AvSourceClockPlanner的DecodedPresentationSpan assembly、CompositionGraphBuilder的released-frame入口、AggregatePlanner按admitted count的display候选，以及SourceResourcesPlanner的pending/receive/filter最小进展数量都继续复用。最终CompositionResourcePlanner已按真实DAG引用、codec和atomic owner计费，不重写这些模块。解码/滤镜/aggregate仍为现有worker与通道，新增容量必须在DAG构建前由planner决定，运行端仅执行背压/失败。

## 必须先闭合的产品

- 压缩准备retention：真实源共享预算及已存在AU size/completion envelope形成有限准入，不声称未来到达率或任意稠密输入都能成功；音频继续使用其已知AU样本事实，不能随视频变成未知。
- decoded startup retention：明确逻辑像素、实际metadata/control-block、固定索引、临时选择与批量release重叠的各自计费域；数量既要有实际存储上界，也要覆盖decoder/filter/receive最小进展。不能把准备存储预算直接除逻辑像素，因为该部署字段不等于GPU显存预算。
- 源队列产品：与decoded startup/aggregate已保留current及next显示点的持有数量一致，区分最大允许保留与保证时间跨度。容量不足明确失败，不能降低原preroll、静默丢掉启动帧或采用经验数量。

现有MediaRtpSourceIdentityProbe只证明映射帧或输入/输出重排关联；其complete不证明已缓存完整preroll，也不证明未来帧密度。不能将probe.decoded直接当可保证的startup容量。下一步须先确认已有prepared decoder/output allocation和同源预算能提供何种准确几何与最小进展合同，再确定typed容量构造；目前不新增公共参数。

## 实施与验收边界

三个容量产品和startup readiness→generation transition→assembly→frame DAG→最终准入形成完整接线后才解除入口拒绝。保留原timeout/preroll、硬件处理和失败语义。需要两名未参与实现的独立审查、Release全量构建、真实非cadence/HEVC重排及四源原规格Windows链路，再进行RKMPP；旧三源cadence成功不替代。当前无代码改动、无新测试产物；完整预算、周期维护、内容A/V和C API门禁仍开放。

## 2026-10-26 分配证据补充

当前 readback 不提供 decoded 最大容量：MediaDecoderOutputAllocationAdapter 证明 CUDA copied output 与固定 decode surfaces 独立；不提供可用显存、最大输出帧数，非 CUDA 也没有对应能力证明。MediaGraphFrameCreditContractPlanner 对硬件使用 ExternalDeviceObservedOnly，最终资源计划硬件字节为零并单独计对象；不能把它解释为已预留显存。

root 已复核启动存储的实际存活边界：

- MediaAvStartupStreamStore::append 每帧同时加入 arrival list 与 presentation map；显示点路径不建立 interval coverage，但所选音频仍使用 interval 路径，不能忽略其 coverage/pending 索引。
- presentationSnapshot 在原存储仍存活时 reserve 并建立独立 vector；Coordinator 同时持有视频和音频快照，随后生成 prefix IDs 与 release selections。容量必须覆盖这些重叠，以及 vector 扩容时新旧存储的暂时重叠。
- MediaAvStartupEnvelopeBuffer::create 将真实 payload footprint 写入 unit.payloadBytes，同时通过 PMR allocate_shared 分配 envelope。前者是媒体逻辑 footprint，后者才是这次引擎元数据分配；两者不能重复当作同一预算已支付的像素。
- MediaLineageAllocator 的 prepared 分支走同源 PreparationMemoryResource，runtime 分支走 new_delete_resource；必须检查每一项实际 scope，不能因为类型支持预算就宣称所有运行期分配已受约束。
- 现有 probeMediaSharedAllocationGeometry 仅覆盖受审计的 MSVC allocate_shared；它不提供 list/map 节点几何或 vector 增长上界，不能复用其结果代替其他容器证据。

下一实现前置项是明确完整元数据成本 G(N)：既有固定占用、decoded wrapper/lineage、arrival/presentation 节点、音频索引、快照和选择/释放重叠；使用实际 allocator 几何或显式固定容量存储产品形成可检查的上界，再求预算内允许保留数并验证最小进展。此数仅为有限准入，不保证未知帧密度必定满足原 preroll；容量耗尽必须保留明确失败。暂不采用 codec level 的吞吐限制替代该存储产品。

本次是源码证据补充，未修改生产代码、未重新构建或运行链路，不能标为实现或验收通过。

## 第一实施单元：release 引用数组精确 backing

仅将 MediaAvReleasedUnits 改用现有 MediaFixedCapacityStorage。Coordinator 根据已选择 release 的 video/audio 实际集合大小一次分配，PassThrough 仅实际媒体项为 1、另一项为 0；不改变选择、preroll 或容量门禁。复用已核查 GStreamer bufferpool 的显式容量及引用持有分配 owner 原则，不自创容器。

Release 显式保留原 MediaLineageAllocationScope，并核对两数组 resource 与 scope 一致；shared control allocator继续持有同一 owner，禁止由裸 resource 指针构造伪 owner。reanchor 原本复制 vector，改为相同数量的精确 backing 逐项复制媒体引用再 move；新旧 backing 并存事实保留。分配和长度错误在 Result 边界明确返回，既有媒体错误原样传播。

本单元只消除 released-units 动态增长与 spare capacity；每个非空数组原始请求为 count*sizeof(MediaAvReleasedUnit)，真正准备预算还包含 MediaBudgetMemoryResource::allocationFootprint 的 header/padding。其他索引、selection/snapshot、shared control、跨阶段重叠仍未闭合，不能称完整 G(N) 或非cadence入口完成。

本单元实现已冻结待双审：修改 MediaAvStartupEnvelopeBuffer.h/.cpp、MediaAvStartupCoordinatorNode.cpp、MediaStartupReleaseTransactionBuffer.cpp 与本文。已完整搜索构造、reanchor、allocateRelease及extractor读取调用；只保留size/empty/front/index/range读取，必要reanchor显式复制引用。scope成员在storage之前声明，析构晚于storage；工厂同时校验resource一致和size==capacity。prepareOutput新增函数异常边界，bad_alloc及length_error返回allocationFailed；reanchor原bad_alloc诊断保留，新增长度错误边界。其他已有Result错误继续原样传播。未构建、测试或提交；本轮无测试临时产物。

独立审查：crash_release_audit 与 publication_identity_review_b 均已明确给予本单元源码 PASS，核对同资源/精确容量、scope 析构顺序、两个构造调用、reanchor 引用与身份保留及异常回收。两者均未参与实现；PASS 不覆盖完整容量契约或运行验收。Release 全量重建已启动，结果尚待确认。

本单元 Release 全量重建会话37415已完成exit0，脚本报告configure/build均为0、clean-first/all-target；本轮尚未运行真实媒体回归，不将编译通过记为合屏验收通过。

运行补充：r356同规格三源TS/UDP、全部失源与选中AV源完整120秒重入已完成阶段观察，caller_stop0，payload222831预约=释放并归零，四张VLC原生快照已检查，16项产物已清理。详见realtime-composition-r356-validation.md；保留VLC迟到警告及全部未完成门禁。


## 第二实施单元：snapshot 准备期复用 scratch

仅将 Store 的 presentation snapshot 改为现有 FixedCapacityStorage，容量来自既有 Coordinator videoCapacity/audio.capacity，构造时一次分配。每次选择清空逻辑长度并填入当前实际项，ReadinessIndex持有只读span；同一次tryRelease内有效，不保存到decision或跨调用使用。Window保留的是Store Record中的unit指针而非scratch元素指针；Store clear同时清除scratch指针。scope先于scratch声明，释放backing前owner保持存活。

沿已核查GStreamer显式容量和allocator生命周期原则，保持原list/map、覆盖和线性窗口选择策略；不修改IDs/selections，也不新增外部参数。准备分配失败映射现有StartupCapacityExceeded，不靠运行时增长。该项仅消除每次选择的snapshot分配，不代表完整G(N)已闭合。

本单元冻结文件：MediaAvStartupStreamStore.h/.cpp、MediaAvStartupReadinessIndex.h/.cpp、MediaAvStartupCoordinator.cpp及本文。已核对仅两处Store构造均传明确容量，Coordinator原append前数量门禁不变；ReadinessIndex只读span，WindowSelector不改，selected指针指向Record而非scratch，clear后原流程不再访问index。准备分配bad_alloc/length_error使用现有typed容量错误并保留移动前的源身份上下文。仅填写实际snapshot项，反复选择不重新分配最大容量。尚未构建/测试或独立审查。


## 第三实施单元：IDs/selections 精确容量

复用 FixedCapacityStorage，按实际arrival-order partition先计数后一次分配，prefix不含selected、suffix含selected且trim仅首项。tryRelease接收owned prior IDs，所有Buffered早返move保留prior；成功顺序prior→video prefix→audio prefix。purge按checked总数一次分配并填充prior→video→audio→可选current末项，再clear。重复/回退drop不新增current ID；stop/abort/markFailed/retireGeneration仍使用无分配clear。

结果结构显式保存原allocation scope且位于storage前，维持move-only结果的资源owner；private purge/advance临时由Coordinator owner覆盖。新分配/容量溢出在submit/poll Result边界转真实StartupCapacityExceeded，不变为Buffered。原索引与窗口选择不改，仅线性计数与填充；不每帧分配最大配置容量。本项仍不代表完整G(N)或非cadence入口完成。

第三单元冻结：修改 MediaAvStartupCoordinator.h/.cpp、MediaAvStartupStreamStore.h/.cpp及本文。全部单项/空项构造、两处合并与成功release前置合并已迁移；tryRelease三个Buffered早返全部move prior，成功partition计数与填充期间Store不变。private advanceGeneration与tryRelease仅由submit调用，新异常由submit/poll边界返回typed真实失败。结果allocation成员先于backing；默认move赋值先替换scope的短暂阶段由prepared allocation header的shared owner覆盖，runtime资源为静态new_delete_resource，未伪造owner。旧prior与组合新backing短时并存仍须纳入后续G。

已搜索所有结果构造/读取、alias和旧insert/prefixBefore调用；Node expiry使用const引用，selection只读遍历，结果均move回传。stop/abort/markFailed/retireGeneration的clear路径未改变。尚未构建、测试或独立审查；本轮没有测试临时产物。

第二、第三单元独立审查完成：crash_release_audit 与 publication_identity_review_b 均分别明确源码 PASS，并对第三单元与scratch组合复核。确认只读span限一次选择、arrival-order分区、所有Buffered保留prior、purge完整填充后clear、结果scope及分配头owner寿命和容量失败边界。仅覆盖本次定容改造，不覆盖完整G或非cadence入口。Release全量重建会话85439正在运行，尚无本次运行验收结论。

第二/三单元运行补充：Release全量重建85439 exit0，r357三源TS/UDP失源与选中AV源重入画面已观察，caller_stop0、payload223823预约=释放且归零。记录source2 generation2 first_interval_miss cursor4277，未定位，不能判连续性通过。16项产物已清理，详见r357验证文档。

后续索引预算调查还须核实seen生命周期：GenerationState::take保留seen，而通用状态机允许同代Running→RequireReacquisition；当前不能直接把seen上界设为一次videoCapacity+audioCapacity。应先核验生产失锁/代际purge的完整调用证据，保留原接受语义。

## 生产 seen 生命周期核验

root与crash_release_audit复核当前全src：Envelope仅CanonicalInputNode和DecodedVideoStartupNode两处生产构造。前者拒绝非Locked时钟证据且对视频/音频恒定产生Locked unit；后者验证display mapping Locked并产生Locked unit。Running的PassThrough不store；Running没有采集deadline。同代初始采集超时仅清未释放Store，poll返回IDs由节点立即erasePurged。真实失锁经reacquisition请求进入下一代，CoordinatorNode先retireGeneration再GenerationState::purge清payloads和seen，随后才恢复输入。

因此之前通用状态机同代RequireReacquisition路径只是API可达性风险，不能据此判定当前生产seen无界。当前生产builder入口可以按每代一组成功startup release保留seen继续推导；该证明不能自动覆盖自定义拓扑。预算仍须计算store(current)先于erasePurged的瞬时并存，不能只看最终payload数量。本核验不改行为、不新增较小门禁，也不把未运行的非cadence入口记为通过。

后续索引产品应保留原list/map/hash算法，分开计empty-head/proxy、每node、音频及interval视频coverage pending最坏三角数量、嵌套map构造临时head、hash bucket reserve新旧重叠以及真实PMR header/padding。实际allocator请求读取只能提供尺寸，必须另以目标STL源码证明操作次数与支持范围；Windows库证据不能外推RKMPP。此产品尚未实现。

## 第四实施单元：Store 索引请求几何与准备域门禁

范围限 arrival list、presentation map、coverage map 与每个 coverage 的 pending map，保留现有索引/选择算法，不处理 generation hash。本次沿用 GStreamer bufferpool 的显式容量和引用持有资源原则，并核验本机 MSVC STL 145/202604、Release `_ITERATOR_DEBUG_LEVEL=0` 的 list/xtree：head 与每元素均通过同一 rebind node 分配一次；map move 先分配新 head 再交换。实际同类型 PMR 容器探测读取 head 与一次 emplace 的请求尺寸/对齐，有限探测只给尺寸，次数上界来自已审计源码结构，不外推到其他 STL 或 RKMPP。

设容量 N，IntervalCoverage 的 C=N，DecodedPresentationSpan 的 C=0。arrival/presentation 请求各最多 N+1；coverage 为 C+1（空 head 始终存在）；pending 为 C(C−1)/2+C+1（C>0，含构造最后一个 CoverageState 的临时 move head；C=0 时为0）。每个后到 interval 对每个先到 candidate 至多一个 pending 项，promote 仅删除；清空后重启同一上界。各类逐请求调用 MediaPreparationMemoryResource::allocationFootprint，包含 header/padding，再 checked 乘加。snapshot 继续独立计费，不进入这四类计数。

planner/preparation 根据既有 MediaLineageAllocationScope 形成明确内部 Runtime 或 Prepared 契约并传递 capacity/readiness：Runtime 保留原未预算域语义；Prepared 必须完整几何产品，不支持的 STL 明确 Unsupported，绝不因探测失败退回 Runtime。Store 只执行已传产品，四个原位 PMR resource 校验请求几何/同时存活上限并转发原资源；不再 reserve，不伪造 owner，不增加外部分配容器。scope/resource 成员先于被分配容器，保证析构顺序。此产品不代表完整 G、runtime 域计费或 GPU 显存闭合。

准备接线为 NodePreparation→Coordinator→Store；新 cpp 沿现有 GRAPH_SOURCES GLOB_RECURSE CONFIGURE_DEPENDS 自动纳入，避免重复注册。当前仅实施阶段，未构建/验证。

第四单元实现冻结待双审：新增 MediaAvStartupIndexStoragePlan、MediaAvStartupIndexStoragePlanner、MediaAvStartupIndexMemoryResource、MediaAvStartupStreamStoreGeometry；NodePreparation 根据已有 scope 明确构造 video/audio 产品，Coordinator 验证成员、容量、readiness及计费域，Store仅执行。产品由 Store 自持，声明顺序为 scope→plan→4个原位resource→snapshot/容器，Store 禁止复制移动；pending临时与所有节点均走其专属resource。原资源分配成功后增加live，真实释放后归还live；异常展开不重复计费，构造失败由已有 typed Result 边界承接。

产品总量是这四类请求的保守上界，当前消费者使用各类真实几何和live上限，不额外reserve整份上界。完整G尚未组合，Store本体仍为既有make_unique分配：本次plan与4个wrapper的固定sizeof增量属于后续控制对象成本，不能声称已由Prepared PMR计费。geometry探测仅临时分配一个空head，由同容器析构释放；在第二次node请求处停止，不建立样例媒体或历史队列。准备域不支持的库版本/iterator配置返回Unsupported，运行域明确保持原未预算契约。hash、decoded最大容量规划、Envelope/shared control几何及完整释放重叠仍未闭合。

已核对唯一Coordinator::create调用及两个Store构造、独立pending allocator、容量算术和新cpp的既有CMake自动发现；未构建、测试或提交，无本轮测试产物。

第四单元独立源码审查：crash_release_audit与publication_identity_review_b均明确PASS，已实际核对本机STL head/node及move分配源码、四角色几何/live门禁、异常展开、原位资源owner和NodePreparation真实接线。范围仅索引分配契约，不代表完整G或运行验证。Release全量重建63479正在运行。

完整预算补充待办：NodeGeometryProbe的临时head当前经new_delete_resource分配，虽异常展开正常释放，尚未由准备预算计费；后续应将真实探测请求接入已有Prepared resource并计算顺序探测峰值。与Store本体固定开销、hash、shared control及跨阶段重叠一起保留开放，不能遗漏。

第四单元构建结果：Release全量重建63479 exit0，脚本报告configure/build均0、clean-first/all-target共750项，四项必需产物已核验。此单元未运行新真实媒体测试；当前cadence入口不能证明新增Prepared几何校验已执行，须待非cadence接线后验证，不重复同一cadence样例充当该路径通过。


## 第五实施单元：几何探测临时 head 的真实计费

第四单元的 NodeGeometryProbe 首次 head 曾通过 new_delete_resource 分配，此临时 engine 内存尚未进入准备预算。本单元保持同容器探测和已审计 STL 算法，allocationGeometry 显式借用原 Prepared PMR resource；planner 在确认 scope owner 后传入。首次 head 的实际 allocate/deallocate 只转发一次，由原资源按真实请求含header/padding计费；第二次 node 请求只记录尺寸并停止，不发生分配、不收费。四类探测依次完成并释放后才开始下一类，临时峰值为各单head实际footprint的最大值，而非四项之和。资源引用只在同步调用栈上借用，owner始终由调用方scope持有，无缓存、伪owner或额外reserve。Runtime分支不变，未支持STL仍明确Unsupported。

此修复只闭合探测head的实际计费，不表示Store本体、hash及完整G已闭合。尚未构建或验证。

第五单元冻结待双审：只修改上述几何接口所在Store头、StoreGeometry、IndexStoragePlanner与本文。每次探测的容器在捕获ProbeComplete前完成析构；任一失败立即返回原Result，不继续后续探测。已核对唯一调用传原prepared resource且原scope持有owner、UTF8无BOM/CRLF和diff空白检查；未构建、测试或提交。


## 第六实施单元：Store 与 Coordinator 本体实账

新增唯一 MediaLineageUniquePtr 工具，复用标准 C++20 polymorphic_allocator::new_object/delete_object（https://eel.is/c%2B%2Bdraft/mem.poly.allocator.mem）：new_object负责构造异常时回收原分配，不另写回滚算法。deleter显式持有原MediaLineageAllocationScope，先验证Prepared owner有效，再从同resource构造/销毁；标准unique_ptr移动deleter不抛异常。null音频owner也携带明确scope，不使用默认分配域。

只迁移Coordinator拥有的两个Store及NodePreparation构造并交给Node的Coordinator owner，Store不移动。每次真实对象请求按实际sizeof/alignof转发原resource一次，包括Store内原位plan/wrapper的固定大小；不预扣sizeof、不伪造shared_ptr、不修改Node本体或shared GenerationState。构造失败由标准PMR释放对象内存，已构造成员按RAII回收；deleter在对象析构与deallocate完成期间仍持有scope。运行域沿原明确new_delete资源语义，Prepared域含原allocation header/padding实账。此项不代表完整G或hash计费规划闭合，未构建/验证。

第六单元冻结待双审：修改新增工具头、Coordinator.h/.cpp、CoordinatorNodePreparation.h/.cpp、CoordinatorNode.h及本文，共7文件；未修改第五单元的Store头/Geometry/IndexPlanner。已全src检索旧unique_ptr/make_unique消费者全部迁移，Node构造仍直接移动preparation内owner无需修改cpp。deleter在new_object前验证resource，两个生产调用均已经planner/Coordinator校验scope；无效内部scope报invalid_argument而不选择其他域。Store分配异常继续由Coordinator::create现有边界转typed容量失败，Coordinator本体分配异常由RuntimeNodeFactory现有bad_alloc边界保留。标准new_object负责失败deallocate，unique_ptr的deleter持scope保证reset和移动赋值销毁旧对象时原resource仍存活。格式及diff检查完成；未构建、测试或提交。

第五/第六单元验证：crash_release_audit与publication_identity_review_b均分别明确源码PASS，并核验组合生命周期；Release全量重建40183 exit0，configure/build均0、clean-first/all-target750项，四项必需产物已核对。本轮没有运行真实媒体测试，不将编译或cadence历史记录当作新Prepared路径证明。


## 第七实施单元：generation hash 准备容量与请求门禁

生产输入已核验仅CanonicalInputNode和DecodedVideoStartupNode构造startup envelope，二者均只发布Locked；Running为PassThrough不store、不poll采集期限，初始采集超时的purged由Node erase。因此每代payload/seen各自最大条目N=Nv+Na，计入store先于erasePurged的瞬时集合；不把通用API同代非Locked分支误当当前生产无界，也不修改该通用政策。

沿本机MSVC STL145/202604 iterator0的xhash/list源码：读取默认max_load_factor并核对受审计值1，不设置新load factor；checked B=bit_ceil(max(8,N))，显式rehash(B)后核对actual bucket_count，避免reserve经float舍入。初始8 buckets对应16cells，目标2Bcells；新旧数组分配时短暂共存，B=8时rehash不请求新数组。节点包括空sentinel，上限N+1。clear/erase不缩bucket，seal后禁止任何新bucket分配。

node/bucket可能同尺寸，不能按bytes判角色。专属hash resource按已审计构造阶段识别首head、初始bucket、空表rehash目标bucket，保存最多两个真实bucket指针；deallocate按指针身份分桶，其余为node，seal后只允许node请求。固定资源成员与产品在容器之前声明，GenerationState禁移动。probe同样按阶段记录，所有实际临时请求通过原Prepared资源计费并顺序释放，不新增无界索引。Runtime域保留原明确未预算契约；不支持STL的Prepared域返回Unsupported。复用现有Index几何类型、allocationFootprint及checked算术，不重复reserve。完整G和shared control仍未组合，本单元未构建/验证。

几何探测不分配目标大桶数组：原小表head与初始16cells实际经过Prepared资源计费；rehash先执行STL的max_bucket_count/长度门禁，在目标allocate请求处只读取bytes/alignment并抛专属完成标记。xhash的_Assign_grow在请求返回之前不改旧表，因此局部捕获后核对小表仍完整，再读取首node请求且不分配node。目标数组只在最终GenerationState的真实rehash中分配一次；不通过分配巨量候选求G。

第七单元冻结待双审：GenerationStoragePlan记录两hash节点/桶几何与逐表峰值；IndexStoragePlanner的planGeneration/validateGeneration复用同一纯计数和footprint函数，validator不重复probe或rehash。NodePreparation在构造前验证scope非空、N、B、节点N+1、实际16cells→2Bcells请求比例、footprint和total；GenerationState初始化后仅执行产品rehash并核对真实bucket_count/load_factor，再seal允许接收输入。每表峰值为max((N+1)个node+最终桶，1个head+初始桶+目标桶)，B=8时准备项只一个桶；两表峰值相加为保守总上界。所有请求含原资源header/padding，一次真实计费。

已核对构造失败、目标分配失败、B=8无rehash、clear复用、seen保留以及store前N门禁；bucket释放按实际指针，不按尺寸。Runtime保持原明确未预算分配策略，不受Prepared node限额。Store/hash共享几何类型与计费/checked工具，hash生命周期独立资源未膨胀Store专用类。新增cpp沿既有CMake GLOB自动发现。UTF8无BOM/CRLF与diff检查完成；未构建、测试、提交。后续仍有实际allocator shared control几何、selection/reanchor重叠、pending deque backing及identity/group字符串成本，完整G未闭合。

第七冻结版本双独立源码审查完成：crash_release_audit 与 publication_identity_review_b 均明确 PASS。确认构造前校验 owner/domain/N/B/节点和footprint、目标桶仅捕获请求、最终实例一次rehash、B=8分支、seen保留与代际清理及RAII失败回收。结论限定当前已审MSVC生产路径，不覆盖完整G、其他STL或Prepared实流。Release全量重建会话95771已启动，尚待退出结果。

第七单元构建结果：Release全量重建95771 exit0，configure/build均0、clean-first/all-target753项。四项必需产物已核验；realtime为6567424 bytes、2026-10-26 16:26:30，local为4555264 bytes（仅构建未运行）。没有新增真实媒体测试，Prepared路径运行与完整G仍未通过。


## 第八实施单元：GenerationState 实际 shared control 几何

本机MSVC memory:3035–3040的普通非数组allocate_shared准确使用std::_Ref_count_obj_alloc3<remove_cv_t<Object>, ActualAllocator>，再rebind为该类型并allocate(1)；xmemory:1178–1181确认一次请求。既有geometry能力新增显式ActualAllocator入口，STL145/202604版本门禁内从该真实类型读取sizeof/alignof，不手写控制块、不包装allocator、不使用TLS或真实临时分配；其他库Prepared能力明确Unsupported。旧network探测入口行为不变。

GenerationState公开内部AllocationAllocator别名，geometry与NodePreparation实际allocate_shared共用该类型。generation产品纳入合并对象/control几何与经MediaPreparationMemoryResource::allocationFootprint计算的固定footprint，并checked加入原两hash峰值；构造前validator复用相同纯规划函数并比对真实类型几何。计费仍由实际allocate_shared原scope请求一次完成，不预留第二份对象预算。Runtime在查询供应商能力前返回原明确未预算契约。仅该完整消费者本轮实施；Envelope/Release/Transaction及字符串、holder与跨代重叠留后续G，完整预算未闭合。

第八单元冻结待双审：修改既有geometry能力头、GenerationStoragePlan、GenerationState头/Geometry、GenerationStoragePlanner、NodePreparation与两份存储文档。GenerationState::AllocationAllocator同时用于真实allocate_shared与几何查询，validator依据同一真实类型重新取得无分配几何，调用原纯prepare校验全部固定成本与hash关系；Runtime仍在能力查询之前返回。原network零参数probe实现保持不变；本轮没有allocator wrapper、TLS或临时control allocation。未修改Node.h/.cpp（pending holder由root独立实施）。UTF8无BOM/CRLF与diff检查完成，未构建/测试/提交。

第八GenerationState shared几何及Coordinator pending单项引用改造均获crash_release_audit、publication_identity_review_b独立源码PASS。后者使用optional<MediaBufferRef>保持已占用但引用为空的原错误语义，移除两个逻辑最多一项deque的动态backing，不改变barrier/排序/背压/释放点。Release全量重建83379已启动，尚待退出结果。

第八及pending改造构建结果：Release全量重建83379 exit0，configure/build均0、clean-first/all-target753项。四项必需产物已核验；realtime为6566400 bytes、2026-10-26 16:34:17，local为4554752 bytes（仅构建未运行）。pending真实realtime回归尚未执行；不得以编译或历史画面替代。
