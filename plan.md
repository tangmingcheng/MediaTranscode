# 当前任务：固定实时视频合屏

## 当前进度：2026-11-16 rk423

当前RKMPP产物已完成四路RTP H264/H264/H264+AAC/HEVC → HEVC CBR8Mbps、1280×720/30、AAC192kbps → MPEG-TS/UDP真实出画、单路及全失源黑屏持续输出；同一CLI中H264+AAC和HEVC重入generation2后两格恢复，再次失源全黑。原源及重入均完整120秒；generation2内部A/V约2268ns、0ppm，RSS重入前后77,688,832B稳定，errors0。详见[docs/realtime-composition-rk422-validation.md](docs/realtime-composition-rk422-validation.md)与[rk423](docs/realtime-composition-rk423-validation.md)。本轮产物及进程已清理；观测结束SIGTERM退出不代表caller-stop门禁通过。

下一步补当前产物双路/三路矩阵及选定音频恢复。剩余门禁：接收端静音/恢复与内容级A/V、直接C API及旧ABI、最新共享代码Windows同规格回归、失败退出/caller-stop及多轮可靠性。不得把四路已证明的出画/恢复重新记为未实现，也不能用一次恢复覆盖所有门禁。Windows FFmpeg保持不动；当前按授权先RKMPP增量验证，本机修改和提交，不推送/合并。目标机依赖变更仅限有证据的阻塞且保留回退。

## 历史进度：2026-10-28 r367

PreparedPush显式分配域与reservation常驻费用已贯通，均双独立源码PASS；Release91686 configure/build均0（755项），realtime6602752 bytes、18:58:39。r365完成三格→黑→重入左下恢复→黑四张VLC原生快照，四源exit0、CLI caller_stop0，payload224456匹配回零，errors/pressure/deadline_misses均0。外部CPU均值2.025%、WS峰269271040，内部raw_phase绝对最大13606ns；VLC错误/警告已保留，不能据此判完整目标通过或旧间歇故障修复。16项产物及六PID已清理，详docs/realtime-composition-r365-validation.md。

authorize临时存储显式owner已完成双审及Release43515构建；后续reservation构图前几何封印已补齐23源码消费链，获两名独立审查源码PASS。2026-10-28全量重建24865失败：Preflight头把实际类型别名错误前置声明为class（C2371及继发C2440/C2672）。已改为包含真实类型头并重新冻结，双方复审明确PASS；第二次全量构建5545成功，configure/build均0（756项），realtime6613504 bytes、2026-10-28 17:59:41。r366实流FAIL：重入阶段发送预约期限错误导致CLI exit1；首帧interval miss亦再次出现。初始三格和全黑快照有效，第三张重入画面晚于失败不能证明持续输出，无第四阶段证据。四次源均3600帧/120秒exit0；payload134880匹配回零，errors2。最坏等待请求211140ns、API实际29761500ns；不判根因已确定。15项产物已清理、六PID不存在，详r366验证文档。

完整G仍欠协议队列/AU动态成本、早期/最终共享holder事实与联合容量求值；已封印的reservation成本当前仅作footprint/溢出检查，不能代表总容量准入。noncadence门禁保持。完整Windows矩阵、内容级A/V、C API/旧ABI、周期维护及Windows之后RKMPP仍未完成，不新增外部FFmpeg修复范围。

r366后续正在实施按代际首AU/解码/过滤/真实发布/aggregate关联诊断，不改状态机或期限。完整16源码已冻结，覆盖原子/直接/延后发布与实际中间节点；两名独立审查者全单元明确源码PASS；Release11274因诊断头缺少MediaNodeId直接定义失败；已补真实头文件并获双人复审PASS。第二次全量重建63158成功（757项，configure/build0），realtime6630400 bytes、18:34:29。r367四阶段VLC原生快照均核验CLI存活，caller_stop0、四源exit0、payload246709匹配零、errors0；16产物和六PID已清理。新证据表明首AU正确发布但其区间已落后aggregate cursor，不能仅凭首帧前interval_miss判为解码丢帧或违反恢复契约；完整连续性/内容AV仍未证明，r366发送等待问题也未判修复。详r367记录。另当前主图与动态worker均未消费High优先级，独立源码调查已确认，方案单独整理，尚无证据证明这是29ms等待的根因。

统一预算与接线计划：docs/realtime-composition-full-storage-admission-plan.md；逐项实现记录：docs/realtime-composition-startup-storage-remaining.md。既往r363/r364结果见各自独立验证文档，不替代最新产物验收。

## 历史阶段记录（当时状态，不替代上述当前进度）

完整G贯通方案已集中到docs/realtime-composition-full-storage-admission-plan.md：固定/协议基项不重复扣B，AU/handoff动态成本与startup holder并计，audio Na独立于video cadence，decoded Nv与压缩AU容量分离，最后验证actual图/owner/容量一致。当前正在实施Session独立audioRetention→admission→prepared view→SourcePlanner/startup一致性链，尚未冻结或构建；noncadence门禁保持。


cancel取消路径已复用预分配通道/锁数组，移除两次临时vector分配，双独立源码PASS；Release全量重建40916 exit0（755项），最新realtime6591488 bytes、17:54:57。此改动晚于r362，未新增实流验收；普通caller-stop不能证明持有预约的取消分支，覆盖缺口保留。完整G继续按early holder→成本峰→同预算容量反解→decoded消费者→最终图一致性推进。


Extractor精确staging与shared对象封印工厂接线已双独立源码PASS，Release全量重建48554 exit0（755项），realtime6591488 bytes、17:42:50。r362同规格三源120秒+选中AV120秒重入完成，四张VLC原生快照三格→黑→左下恢复→黑，caller_stop0、四源exit0；payload220671匹配回零，无interval miss/deadline miss。本轮外部CPU均值2.048%、WS峰269651968，内部phase绝对最大13606ns；VLC错误/迟到警告保留。此cadence规格不替代Prepared/noncadence或完整G验收，不判旧间歇故障修复。16产物与六PID已清理，详docs/realtime-composition-r362-validation.md。下一项为reservation取消路径复用预分配锁数组及完整G剩余成本。


Extractor staging实际backing已改为release同scope的精确容量存储，局部RAII构造完整后发布，背压保持/原清理时机不变；两名独立源码审查PASS。此修改晚于r361，尚未构建或运行，不能沿用r361结果宣称通过。prepared对象工厂成本与owner产品仍在实施，完整G未闭合。


r360在普通执行环境下输入探测零包失败，未建立媒体链路；同二进制/参数以具备本机访问权限方式运行r361后正常输出。r361完整三源120秒及选中源120秒重入完成，四张VLC原生快照确认三格→黑→左下恢复→黑；caller_stop0、四源exit0，payload224514匹配且归零，最终无interval miss、deadline_misses0。VLC转换器/read错误与迟到警告保留，不判完整验收通过，也不判历史间歇故障已修复。内核ETW启动被OS以0x5拒绝，未采集。r360/r361临时文件均已清理，r361六PID无残留。详docs/realtime-composition-r360-validation.md与r361-validation.md。下一步继续完整G实际allocation成本接线；发送期限归因和完整Windows矩阵仍未完成。

本轮继续：启动route描述已由SegmentBuilder实际建图与EventMediaPlanner共同消费；审查发现的assembly timing/startup readiness单向检查已改为构图前双向一致性门禁，两名独立审查者复审PASS。等待诊断追加实际Windows PID/TID与QPC频率供ETW关联，保持原错误/期限，同样双独立源码PASS。Release全量重建62105 exit0（754项），configure/build均0；realtime6581760 bytes、17:17:27，cache/ninja/两个CLI产物已核验，未执行local CLI。已核对本机xperf支持调度/timer事件和导出；未启动追踪，无新实流结果。当前不需要WER注册表配置。完整G与原Windows/RKMPP验收仍未闭合。

r357/r358 interval miss已补固定诊断：同代firstcandidate/firstreal at_miss与final、epoch时刻、prepare丢弃及cursor最终提交；原策略与miss条件不变。两名非实现者独立源码PASS，Release全量重建30330 exit0，最新realtime6577152 bytes、16:53:34；r359已执行但发送期限FAIL退出1：Windows等待请求201740ns，API wall37827300ns；最终无interval miss记录但提前失败，不判旧问题修复。payload160209回零，第四VLC快照为停流旧画面，16产物已清理。详docs/realtime-composition-r359-validation.md。见docs/realtime-composition-interval-miss-observation-plan.md。

GenerationState实际shared control几何和固定footprint已接入，Coordinator两路pending改为内嵌optional引用，消除仅持一项的deque backing。两项均双独立源码PASS，Release全量重建83379 exit0；最新realtime为6566400 bytes、16:34:17。r358原规格三源与完整120秒AV重入回归已执行：四张VLC原生快照观察三格→黑→左下恢复→黑，caller_stop0、payload224041预约=释放且归零，16产物已清理。最终summary再次记录source2 generation2 interval miss cursor4303，VLC转换器错误和迟到警告保留，不判连续性或完整验收通过。详见docs/realtime-composition-r358-validation.md。完整G、非cadence入口与最终验收仍未闭合。
generation hash 的类型化容量与实际分配门禁已接线，保留既有去重和代际清理语义；冻结版本获两名独立审查者源码PASS，Release全量重建95771 exit0（753项），最新realtime为6567424 bytes、16:26:30。本单元未新增实流验证。源码复核另确认：shared control 的几何必须使用实际 allocator 类型，不能直接套用其他 allocator 的探测结果；Coordinator 两个 pending deque 在普通和 barrier 入口均仅允许一个待处理项，但其 backing 分配仍须计入完整存储成本。shared control与完整G仍为调查/待实施项，未通过完整存储或非cadence实流验收。

后续准备存储实现：Store索引几何/四角色上限、探测临时head计费、Store与Coordinator本体PMR独占所有权已接线，相关单元均双独立源码PASS；最新Release全量重建40183 exit0。最新这些改动尚未实流验证，r357仅代表此前构建。generation hash、完整G及非cadence入口仍待完成，见docs/realtime-composition-presentation-capacity-plan.md。

snapshot准备期scratch及IDs/selections精确容量已完成，两独立审查分别PASS、组合复审PASS，Release全量重建85439 exit0。r357同规格TS/UDP已观察三格→全黑→选中源重入→全黑，caller_stop0、payload223823预约=释放并归零，16产物已清理。但source2重入generation2出现first_interval_miss cursor4277（候选/channel均空），原因待定位；不判重入连续性或完整验收通过。详见docs/realtime-composition-r357-validation.md。

释放批次引用数组已改为精确容量存储，必要reanchor接线完成，两独立审查PASS、Release全量重建37415 exit0。r356三源H264/AAC→HEVC CBR8000kbps 1280×720/30、AAC CBR192kbps TS/UDP的失源与完整120秒重入观察通过，caller_stop0、payload222831预约=释放且回零；四张VLC原生快照已实际查看，16项产物已清理。VLC迟到警告及sender最大迟到9527700ns保留，不替代完整门禁。详见docs/realtime-composition-r356-validation.md。

滤镜成功接受输入后因输出背压重复提交的状态缺陷已修正；跨调用立即单输出计数、GPU引用及purge清理获两名独立审查PASS，Release全量重建成功。r353单源原规格持续120秒未复现原TS期限失败，但源BYE后进入重新获取并按既有策略ProgressTimeout退出1，整体FAIL记录保留，不改判SourceCompleted。

r354最新三源H264/AAC→HEVC CBR8000kbps 1280×720/30、AAC CBR192kbps TS/RTP正常输出与全部失源黑屏观察通过，VLC原生快照已核验，caller_stop0、payload116929预约=释放并回零，产物清理完成。r355同一最新构建TS/UDP与选中AV源完整120秒重入观察通过，四张VLC原生快照确认三格→黑→左下重入→黑，caller_stop0、payload230773预约=释放并回零；2次硬边界WouldBlock计数保留，16产物已清理。详见对应验证文档。

总目标仍未完成：周期维护忙期规划、完整存储预算、非cadence/四源HEVC重排、内容级AV/静音、直接C API与旧ABI、全Windows矩阵以及后续RKMPP均须保留门禁；单项真实链路观察不替代这些要求。当前仅本地开发验证，不推送、合并或部署。


## 最新进度：r244

RTP 协议预留账本已复用固定容量存储，两个独立审查者修订后限定 PASS；首构建语法错误已修正，Release 全量重建会话81373 exit0。完整网络存储准入及 TS 封装后 AU 上界仍未完成，已完成同规格三源失源/重入及caller-stop复验，四次VLC原生快照已检查、临时产物已清理；完整门禁仍未通过，详见 docs/realtime-composition-r244-validation.md。详见 docs/realtime-composition-r244-rtp-reservation-storage.md。

### 2026-10-16 r239–r243：存储域接线与原规格复验，整体仍未通过

保证准备额度、公共PMR/预算域、网络global账本及数组实际计费已实现；创建异常双重释放修复、固定容量存储及各实施切片已双独立限定审查和Release全量重建。网络全链计费及足额准入尚未证明，串行门控仍保留。

[r243原规格记录](docs/realtime-composition-r243-validation.md)：三路120秒H264源→HEVC720p30 CBR8000kbps/AAC192kbps MPEGTS/RTP，重入源同规格120秒；VLC原生快照确认三格、全黑、左下重入及再次全黑，caller_stop退出0，最终媒体额度231309预约=释放且归零。17件临时产物和测试进程已清理。一次未复现不等于r239串行阻塞已修复，也不替代内容AV/静音、四源异构重排、C API矩阵及Windows后RKMPP门禁。

下一步按[网络存储边界](docs/realtime-composition-r243-network-storage-domain.md)形成planner生产者身份与协议请求几何，覆盖完整存活分配后再评估多reservation；不采用endpoint数推断并发数或经验容量倍数，不新增外部FFmpeg修复范围。

### 2026-10-16 r236–r238：CUDA隔离与关联诊断完成，完整门禁仍75/FAIL

CUDA canvas私有nonblocking stream、只读黑底ready事实、默认stream host完成屏障与RAII已实现；r236约73秒物化超期，r237约21秒scheduler迟交，均完整归档且清理。reservation三谓词与单head缺流等待诊断经双独立审查、Release全量重建；r238运行约319秒无runtime错误，四次120秒源exit0，caller_stop0，payload230047预约=释放且归零。VLC原生快照覆盖三格、首次全黑与重入；播放器暂停来源不明，末次失源黑屏未证实，未记完整通过或根因修复。详见r236/r237/r238验证记录。

独立质量复核维持75/100。C API已经实现（含已批准输入事实和音频目标码率），其完整矩阵及旧ABI回归尚未闭合，不再标作未实现。确定缺口仍为非cadence SourcePresentation入口、四源HEVC输入重排、内容级AV/静音、完整Windows与后续RKMPP。下一实施先做会话级可消费保证准备额度并接实际allocator，区分compressed AU、decoded metadata和runtime像素收费；不得用准备预算除逻辑像素、均分预算或删除门禁。发送期限问题保留，后续原规格验证同时采集同AU等待与播放状态。无外部FFmpeg改动、无推送合并部署。


用户已批准分两阶段实施，详见 [合屏计划](docs/realtime-video-composition-plan.md) 与 [实施记录](docs/realtime-video-composition-progress.md)。以下保留此前计划和未完成事项，不因本轮任务覆盖历史状态。

当前推进：[完整生产纵向接线](docs/realtime-video-composition-production-wiring.md)：多源并发准备、source/output/aggregate规划、同图owner总准入和唯一输出资源闭环，共用realtime执行控制器。先实际AV+AV，再同链V；完整2～4源及恢复/双平台门禁保持。当前已完成多轮Windows多源真实运行；r148合屏C入口正常输出生命周期双审有限PASS，完整验收未通过。最新证据及未完成项以合屏计划顶部进度为准。

# 可复用工业级 Datagram 发送控制与核心参数收口

- [x] Windows 与 RKMPP 单视频 H.264 720p30 → HEVC 1080p25 CBR 6 Mbps → MPEG-TS/RTP 发送控制门禁；RK 使用 50 Mbps 受管出口事实。
- [x] 三类实时输出复用同一协议无关 materializer → service-scope pacer → nonblocking sender 执行层。
- [x] 删除对外 input layout、path MTU、receiver decode lead、hardware backend、queue、packet、pacing、socket 与 batch 内部参数；PMTU、backend、wire envelope 和资源边界由 planner 推导。
- [x] realtime 视频 RC/GOP/target bitrate 收口为显式产品意图；CBR 仅 target，VBR 为 min/target/max；删除 Auto 与输入码率 fallback。
- [x] 删除 datagram deployment/transport/shaping 中无执行消费者或重复权威的 mode、pacing、socket 和 latency 副本。
- [x] 删除剩余 graph-resource 与 prepared MPEG-TS final-plan 无执行消费者副本，并完成 Release 全量构建及高、低质量真实链路复验。
- [ ] 完成独立 RTP、MPEG-TS/UDP 的同 sender 复验及既有 56 链路最终矩阵。
- [ ] 代码冻结后由两名未参与实现的独立智能体双 PASS，创建 PR，再由新智能体审核 PR。

# Realtime Cross-Layout and MPEG-TS/RTP Implementation Plan

## RTP Video FMTP Auto-Detection

- [x] Add typed H.264/HEVC in-band signaling facts and canonical FMTP generation.
- [x] Probe separate video RTP before planning while retaining the same RTP/RTCP transport and buffered datagrams.
- [x] Bind the prepared raw RTP input to runtime without port rebinding or fallback.
- [x] Preserve manual video FMTP, required AAC FMTP, canonical A/V scheduling, and strict failure behavior.
- [x] Rebuild and repeat the hardware matrix after the finite prepared-queue handoff fix, then complete final documentation/quality review, PR, and independent PASS review.

Detailed design and execution checklist:

- `docs/superpowers/specs/2026-08-04-rtp-fmtp-autodetect-design.md`
- `docs/superpowers/plans/2026-08-04-rtp-fmtp-autodetect.md`

## Realtime VideoOnly Stream Set

- [x] Task 3: converge manual and detected H.264/HEVC raw-RTP FMTP through validated typed signaling facts before depacketizer and product planning; preserve the VideoOnly prepared-transport contract and the AudioVideo shared preflight deadline.

## P0 Input Termination and Finalization Follow-Up

- [x] Establish the finite-source and EOF failure baseline.
- [x] Unify FFmpeg input termination and RTP source-loss classification.
- [x] Coordinate the first worker failure and represent clean threaded completion.
- [x] Correct SWR residue settlement and video decoder EOF lease release.
- [x] Complete real-media P0 and nine-route validation.
- [x] Update delivery evidence and quality score; complete PR and independent review.
- [x] Repeat the reviewed branch with direct VLC URL/SDP observation, remove the invalid observer-remux evidence, and preserve the exact source-loss primary failure for each input class.
- [x] Correct the follow-up review findings for interrupt/EOF precedence and independent secondary `Cancelled` accounting.
- [x] Obtain final PASS on the corrected PR head and close the review evidence.

Detailed design and execution checklist:

- `docs/superpowers/specs/2026-08-03-realtime-termination-correctness-design.md`
- `docs/superpowers/plans/2026-08-03-realtime-termination-correctness.md`

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Support every realtime input with separate RTP, MPEG-TS/UDP, or standards-compliant MPEG-TS/RTP output while preserving the current A/V synchronization quality.

**Architecture:** Replace paired input/output topology decisions with an explicit input clock plan and an independent output adapter plan. All A/V paths share the existing canonical scheduler; Project MPEG-TS remains the only TS mux, with either a UDP datagram sink or an RTP/AVP MP2T sink using PT 33, 90 kHz, RTP/RTCP, and SDP.

**Tech Stack:** C++20, project DAG planner/builder/runtime, project MPEG-TS mux, project RTP/RTCP transport, FFmpeg input and CLI tooling, Visual Studio 2026 bundled CMake/Ninja.

## Source-Driven Realtime CLI Lifetime Follow-Up

- [x] Make `--max-duration` an optional positive realtime CLI monitoring policy.
- [x] Keep duration ownership out of the planner, graph plan, runtime, nodes, scheduler, mux, and sinks.
- [x] Validate zero, negative, and omitted direct CLI argument paths; rebuild x64 Debug clean-first.
- [x] Repeat all nine final-HEAD real-media routes without `--max-duration`, with finite 135+ second sources, FFmpeg/FFprobe receivers, and one VLC observation per route.
- [x] Record lifecycle, runtime error/drop/drift/CPU/memory evidence and obtain the final review verdict.

## Global Constraints

- Preserve the current RTP-to-RTP and MPEG-TS/UDP-to-MPEG-TS/UDP A/V startup, drift correction, recovery, telemetry, and subjective playback quality.
- Planner owns all decisions and fully resolves every input clock, output protocol, endpoint, codec, packet-layout, timing, and capacity fact.
- Runtime nodes do not choose protocols, provide defaults, use packet-arrival clocks, or fall back to alternate muxers.
- Keep one canonical scheduler and one Project MPEG-TS mux path; do not add FFmpeg remux or a second pacing authority.
- Do not add, restore, or adapt CI, CTest, unit, integration, acceptance, hardware, or performance test systems.
- Validate only with the required clean-first x64 Debug rebuild and real CLI/FFmpeg/VLC media chains.
- All edited text is UTF-8 with CRLF endings.
- Update this file as tasks complete, add concise completion evidence under `docs/completed`, and update `QUALITY_SCORE.md`.
- Commit and push every completed task to `codex/realtime-cross-layout`.

---

### Task 1: Make output encapsulation and transport independent

**Files:**
- Modify: `src/internal/graph/model/RealtimeStreamLayout.h`
- Create: `src/internal/graph/model/MediaOutputTransportKind.h`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeRequestClassifier.{h,cpp}`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeRequestValidator.cpp`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeOutputPolicyPlanner.{h,cpp}`
- Modify: `tools/realtime_video_cli/main.cpp`

**Interfaces:**
- Add `enum class MediaOutputTransportKind : std::uint8_t { UdpDatagrams = 0, RtpAvp = 1 };`.
- Add `std::optional<MediaOutputTransportKind> transport` to `MediaRealtimeOutputConfig`.
- Add classifier predicates `udpOutput()` and `rtpAvpOutput()`; keep layout predicates limited to separate versus muxed encapsulation.
- CLI requires `--output-transport udp|rtp`; no implicit transport is accepted.

- [x] **Step 1: Preserve existing enum values and add the output protocol model**

  Give all existing input/output layout enum members explicit numeric values matching their current order. This task establishes `MediaOutputTransportKind` as the single shared enum; Task 4 migrates Project MPEG-TS planning to it and removes the legacy transport enum.

- [x] **Step 2: Parse the explicit CLI transport**

  Add `requiredRealtimeOutputTransport()`, register `--output-transport`, and populate `request.output.transport`. Keep the accepted combinations exact:

  ```text
  separate + rtp = separate elementary-stream RTP
  mpegts  + udp = MPEG-TS/UDP
  mpegts  + rtp = MPEG-TS/RTP MP2T
  separate + udp = rejected
  ```

- [x] **Step 3: Separate input and output validation**

  Remove the fixed topology whitelist and validate supported input classification independently from output classification. Preserve every input-specific requirement. Apply output requirements by the selected layout/protocol:

  ```text
  separate/rtp: host, even base RTP port, packet size, SDP
  mpegts/udp: explicit udp://host:port URL
  mpegts/rtp: host, even base RTP port, packet size, SDP
  ```

  RTP/RTCP uses `basePort` and `basePort + 1`; planner validation rejects overflow, invalid parity, and collisions.

- [x] **Step 4: Refactor URL planning without compatibility branches**

  `MediaRealtimeOutputPolicyPlanner::planUrls()` produces only endpoint facts valid for the selected transport. It must not infer transport from URL scheme or layout. UDP MPEG-TS retains its UDP URL; both RTP modes use the existing numeric host/base-port path.

- [x] **Step 5: Perform static verification and commit**

  Run `git diff --check`, inspect all output classification call sites, verify UTF-8/CRLF, update this task to complete, then commit:

  ```text
  feat(realtime): separate output layout and protocol
  ```

---

### Task 2: Orthogonalize A/V input-clock and output planning

**Files:**
- Modify: `src/internal/graph/planner/avsync/MediaAvSyncPlan.h`
- Modify: `src/internal/graph/planner/avsync/MediaAvSyncPlanner.cpp`
- Modify: `src/internal/graph/planner/avsync/MediaAvSyncPlanValidator.cpp`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeAvSyncAssemblyPlan.h`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeAvSyncPlanningFactsResolver.cpp`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlanner.cpp`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlanValidator.cpp`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.cpp`
- Modify: all production builder/runtime/sync consumers of the legacy paired
  topology type
- Delete: the legacy paired-topology header under `src/internal/graph/sync`

**Interfaces:**
- Replace every production paired-topology consumer with independent
  `MediaAvSyncSourceClockMode` input decisions and
  `MediaAvSyncOutputAdapterKind`/`protocolOutput` output decisions, then delete
  the old type.
- Preserve `RtpSenderReports = 0` and `MpegTsPcr = 1`; add `DemuxTimestamps = 2`.
- Extend `MediaAvSyncInputClockPlan` with `MediaDemuxTimestampInputClockAssemblyPlan`.
- Continue using `MediaAvSyncOutputAdapterKind` independently; no cross-product enum is added.

- [x] **Step 1: Make the synchronization plan input-clock-specific**

  Remove the paired topology field and all `RtpToRtp`/`TsToTs` decisions. `MediaAvSyncPlan` contains exactly one source clock mode plus only the matching input protocol facts:

  ```text
  RtpSenderReports -> rtp input clock facts
  MpegTsPcr -> ts input clock facts
  DemuxTimestamps -> prepared stream time bases and discontinuity policy
  ```

  RTP output identity and TS output mux facts move out of input-clock selection and are planned from the output request.

- [x] **Step 2: Plan output facts from output selection**

  Resolve Project MPEG-TS codec/layout facts whenever output layout is MPEG-TS, regardless of input. Resolve scheduled elementary RTP packetization only for separate RTP output. The planner rejects missing H.264/AAC-LC 48 kHz stereo facts for both TS transports.

- [x] **Step 3: Add URL/RTSP demux timestamp planning**

  Add a focused plan containing explicit video/audio stream time bases, first-window maximum skew, discontinuity threshold, initial generation, and canonical target epoch. Values come from prepared input and existing A/V startup/recovery policy; no runtime constants or packet-arrival fallback are allowed. Any PTS/DTS regression requires generation reacquisition, so there is no separate regression-limit parameter.

- [x] **Step 4: Build assembly variants only from source clock mode**

  RTP uses the existing RTP duration plan, TS uses the existing packet-duration plan, and URL/RTSP uses positive prepared packet/frame durations with validated demux PTS/DTS. Output layout must not affect input epoch, duration, or generation decisions.

- [x] **Step 5: Validate orthogonality**

  Update validators so each of the three input clock variants accepts each of
  the two currently implemented output authorities: scheduled separate RTP and
  Project MPEG-TS/UDP. Keep input and output validation independent and retain
  all concrete RTP transport/packetization/SDP and TS duration consistency
  checks. Reject mismatched facts, duplicate output authority, incomplete
  protocol plans, and any old paired-topology dependency. Task 4 extends this
  matrix with the third MPEG-TS/RTP runtime transport variant.

- [x] **Step 6: Migrate production consumers, verify, and commit**

  Migrate every production builder, compiler, runtime bootstrap, coordinator,
  controller, state-machine, error, mapper, and sync-group consumer. Input
  configuration may read only source-clock facts; output cardinality may read
  only output adapter/protocol facts. Delete the legacy paired-topology header
  without a
  compatibility field or replacement cross-product enum. Require a whole-tree
  zero-result search for the old type and enum members, run `git diff --check`,
  verify UTF-8/CRLF, and execute the mandated clean-first x64 Debug rebuild.
  Update this task and commit:

  ```text
  refactor(avsync): decouple source clock and output protocol
  ```

---

### Task 3: Add strict URL/RTSP demux timestamp clock assembly

**Files:**
- Create: `src/internal/graph/time/MediaDemuxTimestampClockMapper.{h,cpp}`
- Create: `src/internal/graph/nodes/sync/MediaDemuxPacketClockBinderNode.{h,cpp}`
- Create: `src/internal/graph/builder/segments/MediaDemuxClockInputSegmentBuilder.{h,cpp}`
- Modify: `src/internal/graph/model/MediaNodeKind.{h,cpp}`
- Modify: `src/internal/graph/builder/segments/MediaRealtimeAvSyncNodeConfigurator.cpp`
- Modify: `src/internal/graph/runtime/factory/MediaRuntimeNodeFactory.{h,cpp}`
- Modify: `src/internal/graph/runtime/compilation/MediaGraphRuntimeCompiler.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- `::media::Result<MediaMappedTimestamp> MediaDemuxTimestampClockMapper::mapPacket(MediaScheduledStream stream, std::int64_t pts, std::int64_t dts, AVRational timeBase, std::int64_t duration, std::uint64_t generation)` maps PTS, DTS, and duration as one transaction and returns the existing canonical mapped-time/clock-evidence contract.
- `MediaDemuxPacketClockBinderNode` consumes prepared demux packets and emits packets carrying locked canonical presentation time.
- The segment builder consumes only `MediaDemuxTimestampInputClockAssemblyPlan`.

- [x] **Step 1: Implement the protocol-neutral mapper**

  Establish the source epoch from the first valid common audio/video window. Rescale with checked arithmetic into canonical running time. Reject absent timestamps, invalid time bases, regression, excessive inter-stream startup skew, unexplained discontinuity, and generation changes according to the planner plan.

- [x] **Step 2: Implement the binder node**

  Bind packet PTS/DTS and duration to canonical evidence before existing canonical input nodes. The node publishes locked/acquiring/reacquiring state through the same sync-group contract as RTP and TS paths and never synthesizes timestamps.

- [x] **Step 3: Assemble URL/RTSP input**

  Add only the nodes needed for demux timestamp binding and connect them to the existing startup, canonicalization, drift, recovery, and scheduler segments. Do not duplicate those common segments.

- [x] **Step 4: Register and compile the new node**

  Add stable node-kind serialization, runtime factory creation, sync-group injection, generation purge registration, and exact compiler cardinality checks for one demux clock binder per enabled stream.

- [x] **Step 5: Perform static verification and commit**

  Inspect the URL/RTSP graph to prove it reaches the same canonical scheduler as RTP and TS. Run the mandated clean-first x64 Debug rebuild through `.agents/skills/building-with-vs2026/scripts/rebuild_debug.ps1`. Update this task and commit:

  ```text
  feat(avsync): map realtime demux timestamps
  ```

---

### Task 4: Plan Project MPEG-TS transport independently

**Files:**
- Modify: `src/internal/graph/protocol/mpegts/MediaTsMuxPlan.{h,cpp}`
- Modify: `src/internal/graph/planner/realtime/MediaProjectMpegTsOutputPlan.{h,cpp}`
- Create: `src/internal/graph/planner/realtime/MediaMpegTsRtpOutputPlan.{h,cpp}`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlanner.cpp`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlanValidator.cpp`
- Modify: `src/internal/graph/nodes/output/MediaProjectMpegTsPlanSourceNodePlanCodec.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Reuse `MediaOutputTransportKind`; its `UdpDatagrams = 0` value preserves the existing serialized UDP transport value and `RtpAvp = 1` is additive.
- `MediaProjectMpegTsRuntimeOutputPlan` contains the shared mux plan and a transport variant: `MediaMpegTsUdpOutputPlan` or `MediaMpegTsRtpOutputPlan`.
- `MediaMpegTsRtpOutputPlan` contains `MediaRtpUdpSenderConfig`, PT 33, 90 kHz, SSRC, base timestamp, initial sequence number, CNAME, sender-report interval, maximum datagram bytes, TS packets per payload, and SDP identity.
- Extend the Task 2 input-clock-by-output-authority validator from the two
  existing authorities to the third MPEG-TS/RTP runtime transport variant.

- [x] **Step 1: Extend the TS mux transport contract**

  Replace the MPEG-TS-specific transport enum with the shared output transport enum without changing the serialized UDP value. Validate one through seven complete TS packets per emitted batch for both transports. For RTP, calculate:

  ```text
  payload_capacity = maximum_datagram_bytes - 12
  maximum_ts_packets = min(7, floor(payload_capacity / 188))
  ```

  Reject zero capacity and any plan that would fragment a TS packet.

- [x] **Step 2: Create the MP2T plan**

  Construct a single RTP/RTCP session with even RTP port and adjacent RTCP port, PT 33, clock rate 90000, planner-owned SSRC/base timestamp/CNAME, explicit SR interval, nonblocking pressure policy, and the confirmed SDP identity.

- [x] **Step 3: Keep mux facts shared**

  `MediaProjectMpegTsOutputPlan::create()` resolves the same H.264/AAC/PCR/PID/continuity facts for UDP and RTP. Only datagram batching and transport plan differ. No protocol branch may change media timestamps.

- [x] **Step 4: Encode/decode complete runtime plan facts**

  Update the typed node-plan codec to accept both stable transport enum values and all planner-owned transport facts. Decode must reject missing, out-of-range, or mismatched fields; it must not fill absent fields.

- [x] **Step 5: Perform static verification and commit**

  Run `git diff --check`, verify plan encode/decode symmetry and UTF-8/CRLF, update this task, and commit:

  ```text
  feat(mpegts): plan UDP and RTP transports
  ```

---

### Task 5: Implement RTP/AVP MP2T, RTCP, and SDP

**Files:**
- Create: `src/internal/graph/protocol/rtp/MediaMpegTsRtpPacketizer.{h,cpp}`
- Create: `src/internal/graph/nodes/output/MediaMpegTsRtpDatagramSink.{h,cpp}`
- Create: `src/internal/graph/protocol/sdp/MediaMpegTsRtpSdpDescription.{h,cpp}`
- Create: `src/internal/graph/nodes/output/MediaMpegTsRtpSdpPublisherNode.{h,cpp}`
- Modify: `src/internal/graph/protocol/mpegts/MediaTsPacketBatchWriter.{h,cpp}`
- Modify: `src/internal/graph/protocol/mpegts/MediaTsMuxSession.{h,cpp}`
- Modify: `src/internal/graph/nodes/mux/ProjectMpegTsMuxSessionAdapter.{h,cpp}`
- Modify: `src/internal/graph/runtime/factory/MediaRuntimeNodeFactory.{h,cpp}`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Add a typed TS datagram sink interface whose write operation receives both complete TS bytes and the canonical `emitOnMaster` time.
- UDP adapts the existing `MediaOutputByteSink`; RTP uses `MediaMpegTsRtpDatagramSink`.
- `MediaMpegTsRtpPacketizer::packetize(tsBatch, emitOnMaster)` emits one RTP packet with PT 33 and timestamp `base + rescale(emitOnMaster, 90000)`.

- [x] **Step 1: Carry canonical emission time to the transport sink**

  Replace the untyped TS batch write boundary with one typed datagram operation:

  ```cpp
  virtual ::media::Result<std::size_t> write(
      std::span<const std::uint8_t> completeTsPackets,
      MediaRunningTime emitOnMaster) = 0;
  ```

  `MediaTsMuxSession` supplies the already-planned emission time. The UDP adapter ignores no timing decision: it validates ordering and writes the same bytes to the existing sink. The RTP adapter uses the time only for RTP timestamp and SR correspondence, not pacing.

- [x] **Step 2: Packetize MP2T**

  Validate nonempty payload, exact 188-byte alignment, maximum planned packet count, sequence continuity, SSRC, PT 33, and 90 kHz timestamp mapping. Marker remains false for continuous MPEG-TS. No TS parsing, remuxing, or packet fragmentation occurs here.

- [x] **Step 3: Send RTP and RTCP with existing transport components**

  Reuse the project UDP sender, RTP sender state, shared NTP epoch, and RTCP sender-report construction. Maintain octet/packet counters, monotonic RTP timestamps, SR RTP/NTP correspondence, generation purge, bounded close, and preserved first failure.

- [x] **Step 4: Publish MP2T SDP atomically**

  Generate one media description:

  ```text
  m=video <rtp-port> RTP/AVP 33
  a=rtpmap:33 MP2T/90000
  ```

  Include the planned connection address, session identity, CNAME/RTCP facts supported by the existing SDP model, UTF-8, and same-directory atomic replacement. Do not reuse the dual elementary-stream SDP formatter through conditionals.

- [x] **Step 5: Perform static verification and commit**

  Inspect ownership and close paths for RAII, verify no new TS mux or pacing loop exists, run the mandated clean-first x64 Debug rebuild, update this task, and commit:

  ```text
  feat(rtp): send project MPEG-TS as MP2T
  ```

---

### Task 6: Assemble all nine production DAG paths

**Files:**
- Modify: `src/internal/graph/builder/segments/MediaScheduledMpegTsOutputSegmentBuilder.{h,cpp}`
- Modify: `src/internal/graph/builder/realtime/MediaRealtimeRtpTranscodeGraphBuilder.cpp`
- Modify: `src/internal/graph/builder/segments/MediaRealtimeAvSyncNodeConfigurator.cpp`
- Modify: `src/internal/graph/runtime/compilation/MediaGraphRuntimeCompiler.cpp`
- Modify: `src/internal/graph/runtime/compilation/MediaAvSyncRuntimeBootstrap.cpp`
- Modify: `src/internal/graph/sync/MediaAvSyncGroupRuntime.{h,cpp}`
- Modify: `src/internal/graph/nodes/sync/MediaAvStartupCoordinatorNodePreparation.cpp`

**Interfaces:**
- Input assembly switches only on `MediaAvSyncSourceClockMode`.
- Output assembly switches only on `MediaAvSyncOutputAdapterKind`, then the Project MPEG-TS transport variant.
- Compiler cardinality derives from the output plan, not the input clock.

- [x] **Step 1: Assemble all source-clock graph variants**

  With the old topology type already removed by Task 2, build RTP, TS, or demux
  input clock segments independently. Connect every input path to the same
  startup coordinator, canonical video/audio nodes, drift controller, recovery
  components, router, and scheduler.

- [x] **Step 2: Select exactly one output authority**

  Assemble exactly one of:

  ```text
  two scheduled elementary RTP senders + dual-media SDP publisher
  one scheduled TS adapter + Project TS mux + UDP sink
  one scheduled TS adapter + Project TS mux + MP2T RTP sink + MP2T SDP publisher
  ```

- [x] **Step 3: Make compiler validation plan-driven**

  Validate exact node counts for the selected output authority and reject all other output nodes. Source-clock node counts are validated separately. Register the same generation participants, scheduler, and shared clock for all outputs.

- [x] **Step 4: Defer video-only to an independent stream-set architecture task**

  Owner ruling: this task delivers the A/V 3-by-3 matrix only. Video-only
  requires an explicit planner-owned stream-set contract across startup,
  scheduling, protocol output, and Project MPEG-TS PSI/PES planning, so it is
  deferred as an independent architecture task. No compatibility path,
  fallback, fake audio plan, or partial production wiring is introduced here.

- [x] **Step 5: Perform whole-code review and commit**

  Prove the deleted paired topology has not been reintroduced; search for
  input-driven output branching, fallback/default protocol facts, duplicate
  builders, and unused code. Run the mandated clean-first x64 Debug rebuild.
  Update this task and commit:

  ```text
  feat(realtime): assemble cross-layout output DAGs
  ```

---

### Task 7: Document and execute real-media acceptance

**Files:**
- Modify: `README.md`
- Modify: `ARCHITECTURE.md`
- Modify: `QUALITY_SCORE.md`
- Create: `docs/completed/realtime-cross-layout-mpegts-rtp.md`
- Modify: `plan.md`

**Interfaces:**
- README documents `--output-transport`.
- Acceptance evidence contains exact CLI/FFmpeg/VLC commands and results, excluding build commands.

- [x] **Step 1: Update concise user and architecture documentation**

  Document all three output modes, MP2T PT 33/90 kHz/RTP-RTCP/SDP behavior, independent input/output planning, and examples for RTP input to MPEG-TS/UDP and MPEG-TS/RTP.

- [x] **Step 2: Run the required build**

  Execute only:

  ```powershell
  & "C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe" `
      -NoProfile -ExecutionPolicy Bypass -File `
      ".agents\skills\building-with-vs2026\scripts\rebuild_debug.ps1"
  ```

  Require exit code 0 and both CLI artifacts. A timeout or missing artifact fails acceptance.

- [x] **Step 3: Establish current same-layout baselines**

  Run separate RTP input to separate RTP output and MPEG-TS/UDP input to MPEG-TS/UDP output for 2 minutes each with the real CLI, FFmpeg source/receiver, and VLC. Record startup time, drops, duplicates, worker/decode errors, stalls, generations, discontinuities, A/V skew/drift, CPU, memory trend, and subjective synchronization.

- [x] **Step 4: Run the remaining seven paths**

  Run every remaining input/output pair for 2 minutes:

  ```text
  URL/RTSP -> separate RTP
  URL/RTSP -> MPEG-TS/UDP
  URL/RTSP -> MPEG-TS/RTP
  separate RTP -> MPEG-TS/UDP
  separate RTP -> MPEG-TS/RTP
  MPEG-TS/UDP -> separate RTP
  MPEG-TS/UDP -> MPEG-TS/RTP
  ```

  For MP2T, open the generated SDP directly in both VLC and FFmpeg/ffprobe and confirm PT 33, 90 kHz, RTP/RTCP activity, decodable H.264/AAC, and stable A/V.

- [x] **Step 5: Enforce bounded cleanup and evidence quality**

  Ensure every CLI, FFmpeg, ffprobe, and VLC process terminates. Save only concise commands/results in the completion document; temporary captures and scripts remain under `out/` and are not committed.

- [x] **Step 6: Commission quality scoring**

  Dispatch a fresh subagent to review the complete branch against the industrial DAG scoring rubric. Apply evidence-backed score changes to `QUALITY_SCORE.md`, including A/V preservation, clock/output orthogonality, MP2T compliance, ownership, failure semantics, and remaining risks.

- [x] **Step 7: Commit acceptance and documentation**

  Verify UTF-8/CRLF and `git diff --check`, mark all plan tasks complete, and commit:

  ```text
  docs(realtime): record cross-layout acceptance
  ```

---

### Task 8: Final review, PR, and independent approval

**Files:**
- Review: all changes from `origin/master...HEAD`
- Modify only if findings require fixes.

- [x] **Step 1: Self-review the entire branch**

  Check every design requirement, output matrix entry, planner authority, RAII owner, node cardinality, close path, failure propagation, enum stability, UTF-8/CRLF file, documentation claim, and acceptance result. Continue fixing and rebuilding until no omission remains.

- [x] **Step 2: List residual risks and optimization work**

  Record only evidence-backed remaining risks in the completion document and PR. Do not hide them by reducing validation or adding fallback.

- [x] **Step 3: Commit and push all fixes**

  Keep every fix on `codex/realtime-cross-layout`, push the final branch, and verify the remote head matches local HEAD.

- [x] **Step 4: Create the PR**

  Create a ready PR to `master` summarizing planner orthogonality, MP2T output, nine real-media paths, A/V regression results, quality score, and residual risks.

- [x] **Step 5: Dispatch a fresh PR-review agent**

  A new agent reviews the PR diff and acceptance evidence against `AGENTS.md`, the approved design, and `QUALITY_SCORE.md`. It reports blocking/nonblocking findings and an explicit pass/fail decision.

- [x] **Step 6: Close the review loop**

  If review fails, implement findings, repeat required real-media validation in proportion to the affected behavior, push updates, and ask the same reviewer to re-review. Finish only after an explicit pass with no blocking findings.

---

## 2026-08-06 全通路 VideoOnly Stream Set

详细实施计划：`docs/superpowers/plans/2026-08-06-realtime-video-only-stream-set.md`

- [x] Task 1：以显式 `MediaTranscodeStreamSet` 替换 `includeAudio`。
- [x] Task 2：完成 VideoOnly generic、RTSP、MPEG-TS 与 raw RTP 输入准备。
- [x] Task 3：统一 H.264/HEVC 手动与自动视频 FMTP 规划。
- [x] Task 4：实现互斥的 VideoOnly runtime 与单视频调度产品。
- [ ] Task 5：统一 RTP、SDP、Project MPEG-TS 输出及严格 shape 校验。
- [ ] Task 6：完成 clean-first 构建、56 条真实链路、质量评分、PR 与独立审核。

---

## 2026-08-22 可复用工业级 Datagram 发送控制

设计规格：docs/superpowers/specs/2026-08-22-reusable-datagram-transmit-control-design.md

实施计划：docs/superpowers/plans/2026-08-22-reusable-datagram-transmit-control.md

参数基线：docs/realtime-core-parameter-review-baseline.md

**目标：** 协议节点物化最终 wire datagram，公共 service-scope pacer/shaper 生成 enqueue 预约，公共 sender 在 deadline 内非阻塞原子提交；TX timestamp 与 zero-copy completion 仅作异步证据，不参与发送控制。

**全局约束：** wire sustained/peak rate 由 prepared encoder emission、完整 mux/RTP/IP/UDP overhead 和 deployment service evidence 共同规划，caller bitrate 不得直接复制为 transport rate；input AU/queue 不得推导 socket 或 service 参数；三类 UDP/RTP 输出复用同一 shaper/sender，file output 明确不复用；缺受管 service、connected-route PMTU 或 resource 证据时 DAG 前失败；临时 TDD 不入库；真实验收不得降规格。

- [x] Task 1：建立 MediaWireDatagramBatch、持有不透明 move-only RAII commit lease 的 MediaScheduledWireDatagramBatch 与完整 MediaDatagramShapingPlan 产品。
- [x] Task 2：将 MPEG-TS/UDP、MPEG-TS/RTP、独立 RTP 统一改为最终 wire bytes 物化，删除协议层 socket/pacing。
- [x] Task 3：实现跨 batch、跨 RTP/RTCP 的公共 service-scope pacer/shaper，删除 forward-only 经验 pacing。
- [x] Task 4：实现公共非阻塞 transport 与异步 evidence collector；禁止 await TX completion gate。
- [x] Task 5：接入公共 sender、Planner、DAG 和参数契约，删除 input AU -> SO_SNDBUF、5/4 headroom、2 包 burst、caller-owned startup gap 与其他内部容量；Windows/RK 三类输出 30 秒门禁已完成，round2 exact `0084a3d1` 的 H.264→HEVC MP2T/RTP 主链再次通过。停止 source 后的 source-clock expiry 与 sender 窗口 PASS 分开记录；receiver loss/order/TS continuity 留给 Task6 固定 120 秒阶段 2。
- [x] Task 5.1：删除 realtime hardware backend、low-latency、queue、packet/pacing、startup/handoff 与 encoder private controls；CBR 只接受 target，VBR 接受 min/target/max；planner 按 capability 最高评分选择后端。
- [x] Task 5.2：将 realtime core request 改为专用窄参数类型，禁止通过共享 local 参数结构暴露 queue、quality、preset/profile/tune/level、B-frame 或 global-header；planner 产品不再回写外部 request。
- [x] Task 5.3：按不可变 inclusive deadline 状态机统一 `WouldBlock` 重试边界；等值 deadline 只允许一次有界提交，超过即终态失败，不 rebase、不追赶。
- [x] Task 5.4：将 VideoOnly 协议物化与 canonical wire release 解耦；planner 从 prepared emission 与 deployment residence 产品推导内部 `protocolPreparationLead`，scheduler 提前交付 AU，shaper 保持唯一 wire 调度权。Windows Release H.264 1280×720 30 fps → HEVC 1920×1080 25 fps、CBR 6 Mbps、MPEG-TS/RTP 120 秒门禁通过：RTP loss 0、TS error 0、service-curve draw-up 1050.054 B ≤ 1356 B、VLC 解码异常 0；CPU 仍列为后置风险。
- [x] Task 5.5：在 exact `f0c52312` 上复验 Windows Release HEVC 2560×1440 30 fps → H.264 1920×1080 25 fps、VBR 5/12/13 Mbps、MPEG-TS/RTP 120 秒链路：RTP loss 0、TS error 0、service-curve draw-up 1043.348 B ≤ 1356 B、稳定播放窗口解码/late/drop 异常 0；关闭 VLC 窗口阶段保留 1 次 late 日志，CPU 继续列为后置风险。
- [x] Task 5.6：验证相同公共 shaper/sender 的 MPEG-TS/UDP 物化路径。Windows Release H.264 1280×720 30 fps → HEVC 1920×1080 25 fps、CBR 6 Mbps 运行 120 秒：64486 UDP datagrams、TS error 0、service-curve draw-up 1026.665 B ≤ UDP burst 1344 B、VLC 视频播放异常 0；CPU 保留为后置风险。
- [x] Task 5.7：将协议物化后的 wire batch 直接汇入公共 GBRA sender，删除第二级 shaper queue；planner 按 WebRTC 无反馈 2.5×、prepared peak 与 burst/deadline 共同推导 rate，GBRA 从非阻塞 submit-completion 起算下一包 debt。Windows Release H.264 1280×720 30 fps → HEVC 1920×1080 25 fps、CBR 6 Mbps、MPEG-TS/RTP 运行 120 秒：64515 datagrams、RTP loss 0、TS error 0、service-curve draw-up 1356 B ≤ burst 1356 B、VLC 播放异常 0；CPU 继续列为后置风险。
- [x] Task 5.8：删除 realtime/Beta 的 `path-mtu` 与 `receiver-transport-decode-lead` 外部参数；planner 从 connected-path PMTU 与所选出口接口 MTU 的较小值推导 packetization 上限，并从 immutable maximum wire residence 形成 transport timing。公共 service-scope queue 采用 WebRTC queue-time drain-rate 公式并受部署容量硬上限约束；queue snapshot 在 ledger 锁内采样权威时钟，submit lifecycle evidence 与 aggregate queue accounting 分离，消除 reserve/snapshot 与 submit/commit 两类 TOCTOU。Windows/Linux sender 强制 PMTUD，不以 IP fragmentation 作为运行期 fallback。Windows Release H.264 1280×720 30 fps → HEVC 1920×1080 25 fps、CBR 6 Mbps、MPEG-TS/RTP 最终复验运行 120 秒：64516 datagrams（RTP 64486、RTCP 30）、RTP loss/order error 0、TS continuity/TEI/AFC error 0、GBRA debt 1356 B 且 violation 0、maximum residence 96.9708 ms、VLC 解码/late/discontinuity error 0；实际 pacing max 2006648 B/s，部署上限来自权威 100 Mbps 出口链路事实且未被消费为发送速率。平均单核 CPU 23.131%，按用户要求暂不优化并继续列为后置风险。
- [ ] Task 6：完成固定 56 条链路验收（38 VideoOnly + 18 AudioVideo）：Windows 全矩阵实跑，RK 对 capability-admitted 链路实跑、unsupported 链路 DAG 前拒绝；完成 120 秒证据、文档、质量评分、SDD 每 Task 单 reviewer PASS、代码冻结后两名未参与者同时 PASS、PR 与最终审核。

## 2026-09-04 外部 RTP 源 RKMPP 最小范围复验

- 基线：`a5597326464140a319787d123ccc2ffef9c4e40b`；原目录分支 `codex/rk-a559-external-rtp`，没有独立工作树。
- 目标机：`/home/tang/MediaTranscode`；用户 API 真实源 H.264 1920x1080 25 fps RTP，输出 HEVC 1920x1080 25 fps CBR 6 Mbps、GOP 50、MPEG-TS/RTP、VideoOnly；不改参数。
- [x] 恢复基线，目标机全量构建，输入使用用户确认的 `192.168.130.229:61884`。
- [x] 逐次记录真实失败与已确认原因；见 `docs/rk-a559-external-rtp-validation.md`，未将失败记作通过。
- [x] RKMPP 原参数第 16/17 次分别输出 223.11/224.89 秒，源停前核心错误、丢弃、deadline 超限均为 0。
- [x] 第 16/17 次完整链路因外部输入缺口、接收抓包超额及 VLC 迟到未通过；第 22 次因使用 VLC 软件解码作废，不把此前持续运行达标冒充完整通过。
- [x] 第 18 次在不改核心的条件下完成输入证据对照：目标机入口抓包直接证明约 2.48 秒无 RTP；采样 `frames` 字段增加 51 只作辅助关联。异常位于核心收包前，最终区分源发送端与中间路径仍需源机/中间设备同时段抓包。
- [x] 持续运行修复检查点 `3f10fb4d` 已提交并推送；两名独立源码审查均 PASS，专项评分 82/100；草稿 PR #32 由新智能体审核，源码 PASS、完整交付门禁 FAIL。
- [x] 完整验收通过后另建成功测试提交；此前检查点与草稿 PR 不作为成功验收。
- 范围约束：只修导致退出的必要问题，不重新设计、不新增外部参数或诊断库。用户最新要求只做 RKMPP，不再运行 Windows 测试；已有 Windows 通过证据在独立提交 `3ebc551b`，不覆盖后续改动。
- 当前改动：发送软目标限于硬容量、已有低延迟编码契约落实、RKMPP 异步输出轮询、首次物化驻留期限、全局提交背压、按已有 planner 批次上限逐批物化。Linux timer 精度实验未解决退出，已撤销。
- 一小时期限已错过，未宣称按时完成。后续继续以真实 RKMPP 结果为准；先测试，后审查。
- [x] 第 24 次使用全硬件制作的高规格 H.264 2560x1440@30 有限源，原参数 RKMPP 转为 HEVC 1920x1080@25 CBR 6 Mbps MPEG-TS/RTP，源持续 248.018 秒；发送服务曲线超额 1356 B，接收 RTP 零丢失，`hevc_rkmpp` 全帧解码，VLC 默认 D3D11VA 持续硬解且无 deadlock、严重晚帧或解码错误；保留少量 debug late 记录。
- [x] 使用同规格 HEVC 2560x1440@30 有限源完成 RKMPP HEVC→H.264 1920x1080@25 CBR 6 Mbps MPEG-TS/RTP 全硬件验收，源持续 248.287 秒，发送无突发、接收零丢失、默认 VLC D3D11VA 与 `h264_rkmpp` 全帧解码通过。
- [x] run27 完成 H.264 2K30→HEVC 1080p25 VBR 5/12/13 Mbps MPEG-TS/RTP：源持续 248.347 秒，默认 VLC D3D11VA 正常持续硬解，收发 302889 个 RTP 包逐包哈希一致、零丢失，发送服务曲线超额 1356 B。
- [x] run29 完成 HEVC 2K30→H.264 1080p25 VBR 5/12/13 Mbps MPEG-TS/RTP：源持续 248.352 秒，默认 VLC D3D11VA 正常持续硬解，收发 303041 个 RTP 包逐包哈希一致、零丢失，发送服务曲线超额 1356 B；单独归档提交。
- [x] 四项验收完成并分别提交推送；冻结 `dccf95da` 的生产代码由两名未参与实现的智能体独立审查，均明确 PASS，专项评分 90/100。
- [x] PR #32 更新后由新的独立智能体审核 `a54ec0c1`，明确 PASS，无阻塞项。
- [x] run30 完成输入路径 lo 20% 丢包测试，出口维持原 fq；启动探测 FAIL：首个 RTP 缺口触发 5 秒重排等待，约 3.669 秒先耗尽 5 MB 探测预算。证据见 `docs/completed/2026-09-07-rk-a559-input-loss20-validation.md`，不修改核心、不记为通过。

- [x] 按用户要求 run31 先正常运行约 20 秒，再注入输入 lo 20% 丢包；约 25.27 秒后因 12 秒无新编码输出退出，测试 FAIL。实测输入丢包 19.9085%，损伤段完整 IDR 为 0，详细原因见 docs/completed/2026-09-07-rk-a559-runtime-input-loss20-validation.md；不修改核心。

- [x] 根据用户要求对照 FFmpeg、GStreamer、WebRTC 与 AWS Elemental MediaLive 的输入丢包机制；结论与适用边界见 docs/rk-rtp-input-loss-industry-comparison.md。本轮只做调研，不修改核心或新增参数。
## 2026-09-09 动态视频多输出

实施与完成记录：`docs/dynamic-video-outputs-progress.md`。本节覆盖本轮要求，历史未完成矩阵不作为本轮已通过证据。

- [x] 核对 GStreamer 动态管线、tee 与 FFmpeg 引用、硬件池和协议物化的成熟契约。
- [x] 建立本轮分支 `feat/dynamic-video-outputs`，保留原目录构建环境。
- [x] 固定共享输入与 decoder；planner 独立规划输出，准备阶段保留真实 encoder readback 产品。
- [x] 完成不可变执行分段、独立输出故障域、硬边界准入、零输出消费和有序排空回收；Windows 第22轮已覆盖异步物理回收；最新共享时间基修复后，Windows已有证据按用户三项边界复核PASS，RKMPP03已完成同规格实流。
- [x] 接入逐输出状态与 CLI/Beta 动态增删；完整契约相等时复用编码组，Windows 第23/24轮已通过长GOP等待与超期发布前拒绝。
- [x] 先以固定连续 120 秒源完成 Windows RTP → MPEG-TS/RTP 动态多输出真实验收，再进行 RKMPP 同规格验证；不降低原源规格。用户明确门禁为发送无超契约突发、动态增删正常、VLC正常解码，Windows复核与RKMPP03均完成。
- [x] 每项完整验收通过立即独立 commit/push，标题与报告写明链路、编解码、分辨率、帧率、码率和 RC；Windows范围复核471dc0c9、RKMPP03实流76c175b0分别提交推送，历史失败保留。
- [x] 自查全部动态输出改动、更新架构与质量评分、提交PR #33，源码双审及新PR交付审查均PASS；后续用户新增打包/profile范围见下节。

测试原始日志、抓包和截图只存放 D 盘的本轮目录；每次测试结束提取简短命令/结果后删除原始材料并核查进程，不累积到 C 盘或版本库。RKMPP 临时脚本与原始材料在对应测试结束后删除。

## 2026-09-10 RKMPP库交付与profile设置

- [x] 基于指定旧包示例补齐动态add/remove/list，独立双审PASS。
- [x] 使用既有打包脚本，纠正其头文件路径来源为实际CMakeCache；d11b25a7全量615项重建、打包及C11示例编译链接成功。
- [x] 按用户明确授权在Beta输出暴露profile，初始/动态输出统一映射既有planner契约；不新增level或音频字段。
- [x] 最新profile核心543f8565全量615项构建，f41a6554清晰示例重新编译链接并出包；RKMPP Profile01同规格120秒三门禁及参数集通过，独立成功提交1efc1f50。
- [x] 新包和清晰示例已交付，11项校验通过；双审Standards/Spec PASS，原始材料及进程残留已清理。最终独立PR复核58a2b33d的Standards/Spec/本范围交付均PASS。

## 2026-09-10 CLI与库统一profile入口

- [x] 用户明确要求两个入口均支持profile；旧真实CLI带--profile main退出2，错误为unsupported argument。
- [x] 对照目标FFmpeg RKMPP AVOption，CLI初始/add统一允许并映射--profile，复用已验证的库/planner契约。
- [ ] RKMPP全量重建，真实120秒CLI链路核对profile、增删、发送及VLC，成功后立即独立提交推送并交付新包。
- [ ] 双人独立审查、评分及新PR复核，记录命令结果并清理临时材料。

本轮将图像 payload 预算、原 deadline/cancellation 与共享租约接入 canvas 准备，见[阶段记录](docs/realtime-video-composition-canvas-preparation.md)。首次 pool/readback、多源完整总账及公共入口仍待完成，完整 FAIL42。

逐producer事实已从registry的整图ledger依赖中拆出，并接入原FinalCompiler；实际frame scope在消费时复核。见[阶段记录](docs/realtime-video-composition-producer-facts.md)。多源allocation总账及入口仍未完成，完整FAIL42。

逐源资源事实已接同一topology构建/绑定，核对全图成员覆盖、逐输入envelope/ingress及补偿期音频上界；完整驻留总账尚缺，合屏仍FAIL42。见[逐源资源记录](docs/realtime-video-composition-source-resources.md)。

准入调查确认maximumMetadataBytes缺planner写入者，元素数不能证明容器capacity；实施顺序调整为真实输入准备与共享metadata有限存储先闭环，再切换统一owner总账。见[准入门禁](docs/realtime-video-composition-admission-contract.md)。本轮仅调查，未新增构建/媒体通过项，完整FAIL42。

共享canonical元数据已迁移为精确元素只读owner，源链和aggregate共同factory接入，identity按内容比较并持有寿命；不等于分配前全局准入，完整FAIL42。见[存储记录](docs/realtime-video-composition-metadata-storage.md)。

raw RTP输入准备已独立为capture/sealed两阶段owner事务，原realtime preflight直接复用；startup仍从同一sealed输入权威规划。尚未接N源控制器与全局准入，完整FAIL42。见[输入准备记录](docs/realtime-video-composition-source-preflight.md)。

纯视频源调查确认clock/startup/release/epoch均强制双流，采用计划成员贯穿共同DAG；startup tick目前借用音频servo 10 ms，独立调度设计尚缺，不能填假音频。下一步先完整迁移成员型激活产品，再接纯视频源。见[成员合同与门禁](docs/realtime-video-composition-source-membership.md)。本轮仅调查，完整FAIL42。

成员型epoch/release已由plan经bootstrap进入service并贯穿release/重锚/恢复分类，现有A/V生产路径实际使用；双源码审查和Release重建通过，r51 realtime仍no-progress/4逻辑对象，纯视频装配与完整合屏未完成、FAIL42。见[激活产品记录](docs/realtime-video-composition-activation-members.md)。

## 2026-09-29 startup独立截止调度

源startup已解除音频servo周期依赖：复用worker单次绝对期限等待、原有限屏障和生命周期清理，期限与poll共用真实起点及planner原阈值。独立设计及源码双审PASS，Release全量重建通过；r52 RTP H.264/AAC→MPEG-TS/RTP HEVC CBR、1280×720、30fps、8Mbps仍自然exit1/no-progress，4逻辑对象未平衡。无到期分支实流证据，纯视频装配、多源入口/全局准入、AAC重入和双平台合屏门禁仍缺，完整FAIL42/100。见[实现和实流记录](docs/realtime-video-composition-startup-deadline.md)。

## 2026-09-29 RTP源时钟成员合同

计划成员贯穿RTP clock group、validator、snapshot及binder/projector/adapter，AV音频calibration与成员严格一致；单成员复用同一身份/时效/generation状态机，保留PlannedStreamPair现有CNAME政策。独立设计/源码双审及Release全量重建通过；r53 RTP H.264/AAC→MPEG-TS/RTP HEVC CBR、1280×720、30fps、8Mbps仍自然exit1/no-progress，4逻辑对象未平衡，非成功验收。纯视频完整startup/builder/resource、多源入口与总准入、AAC重入及双平台合屏仍未完成，六维10/8/6/12/2/4、42/100，完整FAIL。见[实现与证据](docs/realtime-video-composition-clock-members.md)。

## 2026-09-29 startup成员合同

startup config携带members与完整可选音频产品，单成员复用keyframe/正preroll窗口、代际与deadline状态机；factory端口/options、node有限barrier/EOF、release origin及最终账本startup容量按实际成员处理，start/release对照注册计划。源码双审与Release全量重建通过；r54 RTP H.264/AAC→MPEG-TS/RTP HEVC CBR、1280×720、30fps、8Mbps仍自然exit1/no-progress、4逻辑对象未平衡。完整V源prepared/planner/builder接线、多源总准入/入口及恢复和双平台验收仍缺；六维10/8/6/12/2/4、42/100，完整FAIL。见[实现和实流记录](docs/realtime-video-composition-startup-members.md)。

## 2026-09-29 scheduler退休数据消费

scheduler在purge全部确认后通过代际仲裁与session锁消费退休媒体，复用单一分类器和既有有界head；ContinuousOutput按显式initial-only合同保持独立输出。源码双审及Release全量重建通过。r55→r56同规格RTP H.264/AAC→MPEG-TS/RTP HEVC CBR、1280×720、30fps、8Mbps对照：20项退休媒体被释放，两个10/10满队列消失，编码包入/出均39971；CLI仍no-progress自然exit1、4逻辑对象未归零，完整验收FAIL。可信终止/恢复、AAC重入、V源整链、多源总准入/入口及双平台合屏仍缺，六维10/8/6/12/2/4=42/100。见[实现与实流对照](docs/realtime-video-composition-scheduler-retirement.md)。

## 2026-09-29 停止资源生命周期

resolver stop/abort释放自持快照与codec/device，mux abort复用已有会话释放，重采集配置保留策略不变。3源码双独立审查及Release全量重建通过；r57 RTP H.264/AAC→MPEG-TS/RTP HEVC CBR、1280×720、30fps、8Mbps（AAC CBR192kbps/44.1kHz双声道）仍自然exit1/no-progress，4对象/0bytes、71849/71845。resolver现存槽无credit，最终报告实时读取ledger，不能把4对象归因resolver或等同元数据。未修复完整退出/恢复，V源整链、多源总准入/入口及Windows→RKMPP仍缺，六维10/8/6/12/2/4=42/100、完整FAIL。见[资源释放与失败证据](docs/realtime-video-composition-resource-lifecycle.md)。

## 2026-09-29 stop失败收尾闭环

r58逐节点账本证实VideoEncode abort释放4项，旧final在作用域RAII reset前采集，不能称退出后持久泄漏。有效stop错误现在复用abort完成回收后原样返回；3源码双审及Release全量通过。r59 RTP H.264/AAC→MPEG-TS/RTP HEVC CBR8Mbps、1280×720、30fps（AAC CBR192kbps/44.1kHz双声道）最终0objects/0bytes、71875/71875平衡，但CLI仍no-progress自然exit1，完整验收FAIL。暂不再把已验证的报告时序问题作为4对象未解阻塞；继续可信终止/恢复、AAC重入、V源整链、多源总准入/入口及Windows→RKMPP门禁，六维10/8/6/12/2/4=42/100。见[根因与同规格对照](docs/realtime-video-composition-credit-retention.md)。

## 2026-09-29 无效终止窗口依赖清理

删除从音频servo派生但没有运行时消费者的terminalDrainWindow，保留实际确认超时与终止状态机。源码双独立审查与Release全量重建通过；r60原规格realtime结果见[记录](docs/realtime-video-composition-transition-contract.md)。这是纯V接线的依赖清理，完整prepared/planner/builder、多源控制器与总准入、恢复和双平台门禁仍未完成；六维10/8/6/12/2/4=42/100，完整FAIL。


## 2026-10-05 合屏显式停止入口（r100–r103）

realtime CLI新增RAII系统信号适配器，Ctrl+C/Break转交既有controller.requestStop，POSIX仅bridge接收信号防止媒体poll被中断。四文件源码双独立PASS，Release全量重建exit0。Windows四源120秒H264/AAC→HEVC VBR8000/8000/12000kbps、1280×720/30、AAC CBR192kbps、MPEG-TS/RTP，全失活黑场后Ctrl+C得到CallerStop/exit0、worker0、payload0及预留释放平衡；双独立运行证据复核PASS。合屏准备取消得到Cancelled/exit1。r100宿主终止及r101超时保留失败证据。26件本轮临时产物和进程已清理。完整能力仍未通过，后续门禁与命令详见[当前合屏计划](docs/realtime-video-composition-plan.md)和[真实记录](docs/realtime-video-composition-wiring-runtime.md)；不推送、合并、部署。


## 2026-10-05 r104–r106：发送窗口失败与恢复证据

r104/r105原规格Windows四路RTP/RTCP→H264 CBR 8000kbps、1280×720/30＋AAC CBR192kbps→MPEG-TS/RTP失败。r105同一音频AU在emit前17.922139ms已到scheduler，却因缺video等待，mux接收时晚于dispatch133.956661ms；此前maintenance超过dispatch11.655693ms。当前合同没有缺流下一AU的权威DTS下界，不能猜测帧率或放宽窗口规避失败。

r106仅增加成功提交时刻诊断，双独立源码PASS，Release全量684任务成功。四路完整120秒→全黑→选定AV完整120秒恢复右下格→全黑→Ctrl+C正常exit0；payload0B/0对象、203332预留=释放。接收324.185644秒录制支持画面恢复和失源静音，但VLC重新封装启动PTS有一次回退，原始线端连续性、选定音频内容与端到端同步仍待验证。r104/r105未修复，完整功能FAIL；保留独立75/100，不据单次未复现重评分。21件临时产物已逐项清理，命令、指标与证据边界见docs/realtime-video-composition-wiring-runtime.md。未运行local CLI/RKMPP，未提交、推送、合并或部署。
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

已形成[准备接收交接设计](docs/realtime-rtp-preparation-handoff.md)：全源共享设备/几何和全图准入阻止直接提前运行单源DAG；选择能力证据与live发布分离方向，继续复用原协议与startup节点。新确认MediaUdpSocket最大容量vector经resize后仍保留capacity，prepared仅size记账；r113的4173×65507=273356631B为元素容量下界，不是heap测量。Linux prepared旧接收还缺截断检测，runtime adapter保护不能替代。

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

## 2026-10-23 当前续接点：r312 完成场景，整体仍未通过

最新证据见 docs/realtime-composition-r312-validation.md：Release739项exit0，诊断切片双独立静态PASS；三路原120秒RTP H264/AAC→HEVC CBR8000kbps 1280×720/30、AAC CBR192kbps→MPEG-TS/UDP，三格→全黑→选定AV源完整120秒重入→再次全黑，均由VLC原生快照核验。四个源exit0，CLI caller_stop exit0，errors0，payload预约/释放261506一致且归零；16项临时产物清理完毕。没有线端到达及内容A/V测量，不能记完整验收通过，不能据单次未复现关闭r310/r311超期或旧崩溃。

下一实现按 docs/realtime-composition-ts-storage-closure.md：将series backing、cursor/control、continuity和batch描述符纳入同generation预算，基于最终图边与真实节点持有槽位编译容量；整体准备成功后才提交协议状态。现有queue/prefix/退休边界不重复实现。随后双审及Windows原规格验证；完整矩阵、C API和RKMPP门禁仍保留。

## 2026-11-04：RKMPP 基础合屏与单视频源重入

rk389 已解除 lineage admission 遮蔽异步 receive 的停滞；Windows Release 全量及 RKMPP 8 核增量成功，代码双审通过，三路原规格出图。rk390 完成 RTP H.264/AAC→MPEG-TS/UDP HEVC CBR8000kbps、1280×720/30、AAC192kbps：三格→源0匹配BYE后黑格→新发送进程generation2恢复→其余源自然结束仅重入格→全黑持续输出。重入FFmpeg完整120秒/3600帧exit0，CLI generation2发布3568帧；caller_stop exit0，payload163349预约/释放平衡归零。有限画面功能及文档边界双审PASS，完整验收仍未通过。临时产物与精确PID已清理；见 docs/realtime-composition-rk390-validation.md。下一步为所选音频源失源/重入及内容静音证据，再补异构2–4源、C API和双平台长期可靠性；Windows r385仍未关闭。

## 2026-11-04：RKMPP 所选音视频源失源与重入

rk391 原规格三路 RTP H.264/AAC→MPEG-TS/UDP HEVC CBR8000kbps、1280×720/30、AAC192kbps；源2匹配BYE后重入generation2。VLC录制离线原生快照证实三格→源2黑格→恢复→仅源2→全黑；-80dB/0.5秒检测支持34.966145秒近静音及随后约118.428208秒非静音恢复。发送端完整120秒/3600帧，CLI generation2发布3569帧；caller_stop exit0、payload253099预约/释放归零。DTS乱序警告保留，VLC重封装使其不能直接归因引擎；内容A/V与线端门禁未通过。证据边界双审PASS，临时媒体/日志/快照及精确PID均清理，见 docs/realtime-composition-rk391-validation.md。继续补齐2–4路异构输入及C API真实功能验证，再细化双平台可靠性；整体目标未完成。

## 2026-11-05：四路异构准备门禁失败已定位

Windows r392/r393原规格四路H264/H264/H264+AAC/HEVC→HEVC CBR8000kbps、1280×720/30、AAC192kbps准备失败。r393诊断Release全量exit0，双独立源码审查PASS；实流仍FAIL。源3 RTP marker实际有-3000/-6000 tick重排，共享MediaRtpVideoFrameRateObserver无符号差值产生11250/536870537fps，28秒未来窗口仅增1 AU，前缀60/60即耗尽。根因是cadence事实推导，不是部署2GiB不足；诊断提交ccde1aa4，不含功能修复。见docs/realtime-composition-r393-validation.md，全部临时抓包/日志清理。下一步基于真实码流/解码器权威事实修复节奏规划及重排lineage，原规格Windows后RKMPP；不得关闭B帧、扩经验容量或改外部依赖。此前RKMPP三路失源/重入有限证据仍成立，四路异构、C API与完整可靠性目标未完成。

## 2026-11-16：RKMPP 2–4路异构与C入口阶段进度

当前分支codex/realtime-composition-progress，最新生产修复5b5565d4。rk422–rk425已形成2/3/4路异构合屏、失源黑屏及重入的有限实流证据；rk426有选中音频近静音与恢复证据，不能替代内容级A/V。rk428暴露caller-stop取消等待者重复退休错误，修复经双独立源码审查后，rk429原四路120秒复验caller_stop/exit0、错误0、内部账本归零（44cb0aa1）。

rk430直接C合屏入口使用原120秒双路H264/AAC与HEVC→MPEG-TS/UDP HEVC1280×720/30fps/8Mbps CBR，AAC目标192kbps：调用方配置擦除释放后正常出流，异构UNKNOWN、RKMPP快照正确；VLC原生快照双画面→单格黑→全黑，外部停止与释放exit0（4d189958）。本轮公共C快照不能证明内部账本归零。当前推进非法配置与旧ABI证据，完整C生命周期仍未关闭。

未完成：rk427发送时限失败根因与可靠性、旧消费者真实兼容、回调/压力边界、内容级音画同步与线端证据、最新共享改动的Windows回归及整体双审。按用户最新指示优先RKMPP增量构建，本机FFmpeg保持不变；此前Windows正常不等于后续共享修改自动通过。全部阶段临时产物均按各轮清单清理，整体目标仍未完成，不推送/合并/部署。

## 2026-11-16：Windows最新共享源码回归

Release全核心clean-first all-target重建exit0；r436原四路H264/H264/H264+AAC/HEVC→MPEG-TS/UDP HEVC1280×720/30fps/8Mbps CBR、AAC192kbps，四格→全失源黑屏→所选AV源完整120秒generation2重入→再次全黑，均经VLC原生快照检查。五次源均完整3600帧exit0，CLI caller_stop/exit0、workers/errors0、账本322785预约释放平衡归零。17项临时产物与七PID清理完成。本机FFmpeg未修改，未运行local CLI。该证据关闭本次共享修复的上述Windows回归，不关闭rk427偶发时限、内容级A/V、线端交付与整体最终审查。

## 2026-11-16：r437内容时序诊断收尾

原四源120秒、HEVC720p30/8Mbps CBR与AAC192kbps输出旁路抓包恢复完整TS，输出视频4338帧解码0错误。音频四窗口相关系数>0.9988；视频源存在248帧重复内容，官方signature的候选区间不足以唯一确定时差。30/60/90秒最佳像素候选与音频相差约15.692ms，仅为采样证据，不关闭startup/p95/p99/长稳门禁。CLI caller_stop/exit0、errors0、内部账本归零；输入HEVC启动警告完整记录。30临时文件清理、7PID无残留。详见docs/realtime-composition-r437-content-validation.md。继续rk427时限故障证据补全与可靠性工作，本机FFmpeg不修改。

## 2026-11-16：rk438规划来源诊断原规格复验

e127a744有界provenance双独立源码PASS并RKMPP8核增量44项构建成功后，四路H264/H264/H264+AAC/HEVC RTP→MPEG-TS/UDP HEVC1280×720/30fps/8Mbps CBR、AAC192kbps实流完成。四源完整120秒exit0，VLC原生四格及全失源黑屏确认；caller_stop/exit0、worker/errors0、payload预约释放211778平衡归零。最坏成功包日志完整关联本次planning快照、publication、materialized和sender序号，无重复key。rk427未复现但未关闭，RTP leading SR实流和Windows共享回归仍待验证；输入MPP/HEVC启动告警、pressure2、delivery not_proven均保留。远端18项/本机6项临时产物及全部PID清理完成。详见docs/realtime-composition-rk438-validation.md。

## 2026-11-16：rk439 MPEG-TS/RTP诊断复验

四路原120秒H264/H264/H264+AAC/HEVC RTP→HEVC1280×720/30fps/8Mbps CBR、AAC192kbps MPEG-TS/RTP完成；全部源exit0，VLC原生四格与全黑确认，CLI caller_stop/exit0、errors0、payload215159预约释放平衡。媒体106313/控制38数据报已提交，新增publication身份及物化时间校验通过、字段无重复。pressure2和delivery not_proven保留，rk427未复现不等于修复。19远端/6本机临时文件和全部PID清理。详见docs/realtime-composition-rk439-validation.md。

## 2026-11-16：共享诊断Windows重建与最小剩余矩阵

e127a744诊断源码按VS2026技能300秒上限全核心clean-first all-target重建完成，24cl并行观察成立，configure/build exit0、必需四产物齐全。realtime CLI SHA256 63bdf880e76ce6fcd773ce23b8e20f9193e720ad24ac34e54092ddf876c269e4，7568384字节；未运行local测试，本机FFmpeg未改。实流回归仍待执行。独立核对原计划后收敛最小缺格为RKMPP H264 VBR目标8/范围8–12Mbps MPEG-TS/RTP（含失源重入）；不擅加全编码×RC×协议排列。详见docs/realtime-composition-current-acceptance-matrix.md，可信终止、内容同步和rk427可靠性仍保留。

## 2026-11-16：r440 Windows H264 VBR共享回归失败

新Release四路原规格H264 VBR目标8/范围8–12Mbps、720p30 MPEG-TS/RTP出画后，在首轮约93秒CLI自行exit1；未执行失源/重入门禁。audio3672等待缺失video2559约144ms，超原dispatch24.44ms后提交，mux ready超24.91ms，TS accepted-work拒绝未发布AU；sender deadline_misses0。共享AV scheduler无跨支路未来AU下界，缺head时无限等待；不得无排序证明提前输出或放宽deadline。已安排只读审查encoder/cadence/lineage能否形成权威watermark。canvas同轮75.56ms wall/71.3165ms GPU事件仅是相关证据，不归因本机FFmpeg。四源完整120秒均exit0，CLI账本159204平衡归零仍不算通过；13临时项及六PID清理。详见docs/realtime-composition-r440-windows-validation.md。继续独立RKMPP输出矩阵，Windows共享回归保持FAIL未关闭。

## 2026-11-16：rk441四源H264 VBR/RTP与完整重入

原规格四源120秒→H264720p30 VBR目标8/范围8–12Mbps、AAC192kbps MPEG-TS/RTP；选中source2再完整120秒重入，VLC原生四格/全黑/单格恢复/再次全黑确认，CLI显式停止exit0、errors0、账本374096平衡归零。pressure19、系统UDP接收错误+19042（已采样输入socket drop0）、线端交付未证明如实保留，整体验收未完成。22远端/8本机临时项与测试PID清理。Windows r440共享调度与rk427可靠性仍未关闭。详见docs/realtime-composition-rk441-validation.md。

## 2026-11-16：编码完整输出能力合同阶段1

新增MediaVideoEncoderCompletionContract与只读adapter，prepared readback记录受限NVENC逐帧/单视图/零重排/完整packet能力；CPB及公开参数不一致保持未授权。未启用scheduler放行，RKMPP未授权，r440未修复。两独立智能体均阶段SOURCE PASS。Release按现有技能300秒上限全核心clean-first all-target构建成功，观察24个cl并行，configure/build exit0、四必需产物齐全；realtime CLI SHA256 9948C8CE2CE2C91F4755A3D7B68503DAE819DA9A82A059D221D026506D08F2E4（7569920字节）。未运行local测试，本阶段尚未实流验证；接通有序进度与生命周期后执行原规格realtime复验。未改Windows或目标FFmpeg。详见docs/realtime-composition-encoded-progress-plan.md。

## 2026-11-16：共享编码进度接线完成待实流

prepared定长合同沿原metadata传递；完整AU依既有有界lineage传递canonical同轴下界，scheduler在成功commit后更新且只严格小于放行，flush/purge重校验后恢复代次能力。两独立审查发现的头声明及跨代授权问题已修复，两者重新SOURCE PASS。Release全核心clean-first all-target构建exit0，观察24cl并行，四产物存在；CLI SHA256 935C4CB53A8CFBE4750B7657D31B06DAF07DA747595F3F05A6D10BB3E1D2D064。尚未实流通过，r440/rk427未关闭，RKMPP后端仍未授权进度；下一轮r443原规格复验。

## 2026-11-16：r443 Windows四源H264 VBR/RTP完整重入回归

9cac2eb0原四源120秒→H2641280×720/30fps VBR目标8/范围8–12Mbps、AAC192kbps MPEG-TS/RTP，选中source2再完整120秒重入，全部源exit0。四格/全黑/左下恢复/再全黑VLC原生观察完成，CLI caller_stop/exit0、errors/pressure/deadline_misses0，payload348326平衡归零。两独立源码审查及全量Release成功后本轮功能回归通过；无专门水位放行计数，长期可靠性/内容A-V/RKMPP进度授权未完成。18临时项及七PID清理，详见docs/realtime-composition-r443-windows-validation.md。

## 2026-11-16：rk445 完成合同实际接入验证

50993608经双独立SOURCE PASS，目标8核增量构建exit0。四路原120秒H264/H264/H264+AAC/HEVC RTP→H2641280×720/30fps VBR目标8/范围8–12Mbps、AAC192kbps MPEG-TS/RTP，选中源完整120秒重入，VLC原生四格/全黑/左下恢复/再全黑确认。RKMPP实际只读配置授权、首完整AU匹配、成功水位放行正例成立；CLI caller_stop/exit0、errors0，payload353683平衡归零，sender期限失败0。压力6、HEVC解码警告及delivery not_proven保留；22远端/8本机临时文件与全部PID清理。本机FFmpeg未改；后续当前共享Windows回归、内容A/V、rk427、C残项和最终双审仍待完成。

## 2026-11-16：rk446 C边界与共享早停竞态

有效正准备预算不足的早期拒绝/ERROR回调读取/外部释放补充边界通过；RUNNING回调请求停止实际失败，首错为graph payload credit waiters were cancelled，workerErrors5、encoded0、consumer exit1。根因调查指向共享生命周期取消资源早于worker停止意图，修复不吞真实错误并同步检查abort/failure supervisor。Windows50993608全量Release构建exit0、24cl并行、四产物齐全，但新的停止修复后仍需重新验证。rk446全部临时项已清理，整体验收未完成。

## 2026-11-16：rk447 早停顺序修复及后续mux边界

8a6f8af1经双独立SOURCE PASS并目标5项增量构建exit0。原RUNNING回调复验workerErrors0，credit取消误判消失；随后尚未绑定的mux错误执行finish，FAILED/SHUTDOWN_FAILURE、encoded0，整项仍FAIL。继续修复共享mux停止生命周期，不吞已激活会话finish错误；Windows最终共享版本回归仍待完成。证据与清理见docs/realtime-composition-rk447-c-callback.md。

## 2026-11-16：rk448 C早停门禁通过

ce71265d统一mux完成意图：未激活准备取消、已激活严格finish、原错误保留；双独立SOURCE PASS，目标9项增量构建exit0。原C消费者RUNNING回调读取/停止成功，COMPLETED/REQUESTED_STOP、workerErrors/event_errors0、外部release句柄为空、exit0。encoded0，不冒充持续播放或全账本验证。12远端/5本机测试项与传输构建临时项清理。下一步最终共享版本Windows realtime回归及已激活正常停止，继续内容A/V、rk427与最终整体审查。详见docs/realtime-composition-rk448-c-callback.md。

## 2026-11-16：RKMPP内容取证与两个未闭合失败边界

rk454复现sender期限冲突及失败后不退出，证据归档；rk455临时脚本CRLF错误中止，非生产依赖问题。rk456原规格四源H264/HEVC RTP→H2641280×720/30fps VBR目标8/范围8–12Mbps、AAC192kbps MPEG-TS/RTP完成120秒及全失源黑场，正常caller_stop/exit0。输入联合SR与110秒相关、112–118秒留出段消歧取得有限内容对应PASS，基准扣除后残差2.791–2.812ms；另发现约1.267秒音频尾部未体现，BYE立即purge为待证共享路径。Windows FFmpeg不变，53项临时文件/9个PID清理。下一步核清BYE已接受媒体边界，保留rk454真实失败并继续原计划单源与动态多输出回归；整体目标未完成。

## 2026-11-16：rk457确认尾媒体purge缺陷

13a7d57c有界owner诊断双审通过、目标8核增量成功；四源原120秒→H264720p30 VBR8–12Mbps/AAC192 TS/RTP运行正常。BYE清理前55候选56320样本仍映射到下一输出之后，证明尾音被源代purge清除。38临时项/9PID清理，rk457回退保留。修复保持原A/V源组语义，补有序代内关闭/排空后原purge与自动重入；不扩组内成员独立重建，不改Windows FFmpeg。发送失败及旧链路回归仍待闭合。

## 2026-11-16：rk458源组排空基础接口

EndGeneration及固定关闭状态、frontier/成员完成、SourceDrained强校验、owner实际sizeof准入计费已完成基础切片双SOURCE PASS；RKMPP8核增量303项exit0。源码核验区分ingress/clock-group/playback三域，并沿原单transaction通道采用上游整组有序屏障、下游成员独立排空确认。生产BYE尚未启用，尾媒体修复仍待完整接线和双平台实流，不新增验收成功声明。回退保留，Windows FFmpeg未改。

## 2026-11-16：rk459解码器代内排空

音视频decoder新增EndGeneration按原send-null/receive EOF排空后等待owner purge，保持末帧/标记提交顺序且不永久EOF。修复双审发现的非同步实例无purge等待问题后，两者复审SOURCE PASS；目标8核增量5项exit0。控制尚未接入生产BYE，尾媒体完整修复及原规格实流仍待完成，不改Windows FFmpeg。

## 2026-11-16：rk460 startup整组屏障

源成员代内关闭复用原startup/release FIFO，收齐实际成员且held/前序pending已提交后发唯一组标记，不永久EOF；冻结代次关系显式验证。四文件双SOURCE PASS，RKMPP8核增量27项exit0。完整BYE接线及实流尚未通过。

## 2026-11-16：rk461重采样及补偿旁路排空

EndGeneration覆盖音频/补偿双边原子发布，窗口执行与Swr排空持续推进，真实exhaustion及尾样本提交后发下游，等待原purge；原EOF/Flush保持。七文件双SOURCE PASS，目标8核增量10项exit0。无媒体验收成功声明，剩余接收端和aggregate接线继续。

## 2026-11-17：rk462聚合器代内排空

三个成员入口接通Closing/Retired，待实际贡献提交后确认排空，由视频owner完成整源关闭；修正跨快照退休竞态。七文件双SOURCE PASS，RKMPP8核增量12项exit0。尚未启用生产BYE，无媒体验收成功声明。接收截止、新代交接、完整回归及失败退出继续。

## 2026-11-17：rk463启动裁剪代内关闭

trim接入EndGeneration、精确授权后转发并等待purge，实际owner sizeof计费。三文件双SOURCE PASS，RKMPP8核增量6项exit0。生产BYE尚未接通，不构成实流通过。

## 2026-11-17：rk464 RTP binder代内关闭

EndGeneration明确clock-group域，检查成员关闭身份并在实际提交时持代次授权。修复clock fanout先到竞态、移除重复sizeof计费，三文件双SOURCE PASS；目标最终8核增量5项exit0。raw生产BYE未接通，不新增实流成功声明。

## 2026-11-17：rk465 BYE失效前成员事实

匹配BYE在破坏性delta前保存owned旧ingress代次/SSRC/捕获时刻，commit验证双向配对；三文件双SOURCE PASS、RKMPP8核增量exit0。尚不改变生产BYE行为，下一步由prepared source接入关闭状态与交接。

## 2026-11-17：rk466 raw有序关闭出口

prepared packet FIFO接入EndGeneration容量计费，raw沿原边持授权发布、等待purge并取消旧pending。五文件双SOURCE PASS，RKMPP8核增量9项exit0。producer与接收截止/新代交接未完成，尚无新实流通过。

## 2026-11-17：rk467映射观察与成员退休接口

prepared owner复制runtime clock身份标量；协议新增严格身份退休接口，GroupClosed不伪造BYE。修正ingress初始0误套playback非零限制。11文件双SOURCE PASS、RKMPP8核增量38项exit0。producer未启用，完整实流仍待接线后执行。

## 2026-11-17：rk468复合BYE与退休消费证据

协议支持保留BYE前有效SR/CNAME，clockGroup回执精确已消费退休事件；12文件双SOURCE PASS，RKMPP8核增量37项exit0。producer未启用，新增全source DAG可选holder审计继续补遗漏，不计实流通过。

### rk469：视频贡献节点代内排空
VideoFilter/VideoFrameRate 的 EndGeneration、purge 复位、提交与清理互斥已实现，双独立 SOURCE PASS，RKMPP 增量 9/9 退出 0。未跑实流，HardwareTransfer 与接收关闭入口仍待完成。详见 docs/realtime-composition-rk469-validation.md。

### rk470：HardwareTransfer 代次生命周期
源组/准备身份绑定、提交仲裁、owner purge与旧FIFO标记退休分类已实现，双独立 SOURCE PASS，RKMPP增量8/8退出0。尚无实流结论，接收端关闭入口仍待接通。详见 docs/realtime-composition-rk470-validation.md。

### rk471：关闭所需协议残留事实
只读reorder/pending/assembly状态查询与显式Accepted判据已实现，双独立SOURCE PASS，RKMPP增量161/161退出0。未运行实流，关闭入口仍未启用。事务及锁序约束已补充到sender-bye调查，详见rk471验证记录。

### rk472：receiver与source事务锁边界
source consume/resumeDrain移出receiver mutex，delivery单项顺序及Accepted frontier提交保持，停止竞态不重放。双独立SOURCE PASS，RKMPP增量4/4退出0；尚未进行实流。详见docs/realtime-composition-rk472-validation.md。

### rk473：实流发现并修复重入授权回归
原规格四屏实流FAIL：source1合法generation2重入被HardwareTransfer错误套用initial preparation身份拒绝。已按实际Initial/Next发布合同修复，双独立SOURCE PASS。CLI失败退出另卡在RKMPP取帧，已取得符号栈并按精确PID清理，未宣称解决。rk473临时产物清理完毕，rk474增量构建4/4退出0，实流复验待完成。详见docs/realtime-composition-rk473-validation.md。

### rk474：原规格复验启动环境失败
CLI在30秒参数集探测超时，Windows源匹配RTP包为0，未进入四屏运行；自然exit1，无重入修复有效性结论。已记录命令、PID、CPU/RSS并逐项清理；下一轮在桌面网络执行环境启动全部本机媒体进程，保持原规格。详见docs/realtime-composition-rk474-validation.md。

### rk475：四屏真实画面及发送期限失败
桌面网络权限下四路出图，VLC原生快照确认四格；四源完整120秒exit0。约112秒sender sequence106554超过最后提交时间29521ns，CLI自然exit1。未覆盖全黑和重入，不能标记整项通过；共享发送等待/期限合同继续调查，不放宽预算。本轮临时文件与进程已清理，详见docs/realtime-composition-rk475-validation.md。
