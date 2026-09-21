# MediaTranscode Quality Score
## 2026-09-21 合屏域与聚合接线独立审查 B

独立审查B对`2ff6bab0`之后当前合屏WIP给出**源码安全保留PASS、完整合屏FAIL**：交叉复核确认的RGA隐式plane布局P2已修复，完整布局及RGB24/BGR24字节序冻结复审通过。域注册、连续贡献、黑帧/静音及CUDA/RGA源码已对照FFmpeg/GStreamer；preflight、全局资源准入、CLI/C API和多源Windows→RKMPP验收仍未闭环。r22属于单源回归，不能替代合屏证明。沿用六维10/8/6/12/2/4，合计**42/100**，不提高分数；B结论不代替A或最终PR审查。

## 2026-09-21 编码提交事务复审

两名独立审查者 composition_transaction_review_a/b 均 Standards/局部事务 Spec PASS，未发现新增阻塞；合屏就绪度维持42/100（10/8/6/12/2/4）。Release全量构建成功；r21首段120秒有VLC画面，重入AAC timeline失败，CLI自然exit1、逻辑资源归零。完整合屏FAIL。提交事务前置不能替代独立输出生命周期、mapper驻留硬界、黑帧/静音及多源验收。[命令与结果](docs/realtime-video-composition-transaction.md)。

## 2026-09-21 输出身份与 FIFO 边界复审

两名未参与实现的智能体对源/输出身份、canonical 音频贡献保存、完整 timeline 校验及同步 FIFO 上界给出 Standards/局部 Spec PASS，直接 include 和同步组 contract 修复后复审结论保持。Release全量成功；r20重入AAC时间轴失败，CLI自然退出1、逻辑资源归零。完整合屏 FAIL，就绪度仍为 42/100（10/8/6/12/2/4）；尚缺独立输出生命周期、连续聚合、黑帧/静音、多源与双平台运行证据。FIFO PCM payload 界不覆盖 metadata/codec 物理内存，贡献尚未穿过调度到协议层。构建与实流结果见[本轮记录](docs/realtime-video-composition-output-identity.md)，源码 PASS 不替代运行验收。

## 2026-09-16 显式处理归属与 r16–r18 复审

两名未参与实现的审查者均Standards/局部Spec PASS，未发现新增P1/P2；合屏就绪度维持42/100（10/8/6/12/2/4）。segment显式归属、互斥覆盖校验与按角色准备注入已贯通，仍执行旧整体transition。Release全量重试成功；r16无源超时退出且最终保留4个逻辑对象，r17测试启动时序失败不计验收，r18完整双120秒源重入仍报AAC时间轴错误，CLI自然退出1且逻辑资源归零。完整合屏FAIL，连续输出、贡献模型、黑场/静音和跨平台验收仍缺失，不能据结构重构提高分数。[命令与证据](docs/realtime-video-composition-processing-ownership.md)。


## 2026-09-16 协议代次交接增量

两名独立审查者对owner线程清理、异步purge、协议/SDP注册及RTP事件顺序源码给出Standards/局部Spec PASS；r13七组ack并恢复源锁2、新MPEG-TS计划，随后AAC不支持flush导致编码lineage冲突，且漂移pending长期持epoch锁阻塞退出。短授权修复再次双审PASS，r14同规格Debug实流保留AAC失败并自然退出1、逻辑资源归零；完整恢复FAIL。旧r13挂起进程已按用户明确授权清理，r15 Release全量构建通过，同规格重入仍报AAC错误，但CLI自然退出1、逻辑资源归零，并实际取消53个未提交数据报；完整恢复仍FAIL。本轮产物与进程已清理。合屏就绪度维持42/100（10/8/6/12/2/4），未以基础修复提高评分；独立输出域、黑场/静音、多源与Windows→RKMPP验收仍未完成。[证据与命令](docs/realtime-video-composition-protocol-handoff.md)。

## 2026-09-16 purge屏障与恢复控制增量

两名独立审查者复审10文件源码增量，Standards/局部Spec均PASS：修复背压重试跳过代次仲裁、purge完成缺少域唤醒、startup重复失效/控制消息滞留，以及传输计划遗漏purge注册。r9/r10定位的错误已越过；r11全部5组ack并恢复源锁2，最终仍因MPEG-TS构造器保留旧计划而退出1。完整恢复FAIL，详见[purge屏障记录](docs/realtime-video-composition-purge-barrier.md)。就绪度仍42/100；剩余协议代次交接、sender线程归属清理、输出时间轴、黑场/静音及跨平台验收不能由局部PASS替代。

## 2026-09-16 源失活代次增量

两位独立审查对validator三阶段、重复失效幂等、old/next投影及耗尽失败均给出Standards/局部Spec PASS。r8实际解析两次匹配源BYE，next=2/old=1稳定，无原malformed discontinuity；120秒有编码与VLC画面，随后仍无进展超时。完整交付FAIL，六维就绪度保持10/8/6/12/2/4，共42/100；不外推完整恢复或跨平台。剩余恢复获取期限、purge并发、尾部和物理内存风险见[专项记录](docs/realtime-video-composition-source-generation.md)。

## 2026-09-15 阶段一基础修改复审

两位未参与实现者对阶段一及2026-09-16输入保留增量均给出 Standards PASS / 当前 WIP 源码 Spec PASS。本轮贯通输入启动 retention、字节/对象信用、binder与重复帧凭证生命周期及独立启动批次发布容量；r5、r6真实链路FAIL，修复后的r7已进入持续编码与VLC播放，最终结果见[输入保留记录](docs/realtime-video-composition-input-retention.md)。独立复审建议维持下表六维尺度42/100：仍是单源基础，多源绑定、公共合成域、逐源黑屏/静音与恢复及双平台合屏验收尚未完成。逻辑credit不代表clone side data等全部物理分配边界；控制进展、完整结束和Windows→RKMPP回归仍待闭环，不因局部修复提高评分。

## 2026-09-15 固定实时合屏就绪度（实施前）

基线 `d590cc3b`；独立智能体仅进行源码专项评估，未运行合屏。下表衡量本次 2～4 路 RTP/RTCP 合屏的就绪度，不替换历史项目质量分，也不表示验收通过。

| 维度 | 满分 | 得分 | 缺口 |
|---|---:|---:|---|
| DAG 复用与多源绑定 | 20 | 10 | 单源运行时基数与绑定 |
| 多源时钟与合成来源 | 20 | 8 | 公共输出域、贡献记录 |
| 逐源断流和恢复 | 20 | 6 | 黑屏、静音、局部恢复产品 |
| 线程、背压和资源边界 | 20 | 12 | 多路总体准入与逐分配计量 |
| 双平台合成能力证据 | 10 | 2 | 实际合成及吞吐未验证 |
| 合屏观测与真实验收 | 10 | 4 | 合屏专属观测及运行证据 |
| **合计** | **100** | **42** | **尚未就绪** |

后续能力查询确认两平台滤镜存在；Windows CUDA 异常返回值传播属于静态风险，并无当前启动失败的复现证据，不能认定必须先修依赖。实施前评分保留，不据滤镜存在提高分数。详见 [能力门禁](docs/realtime-video-composition-capability.md) 和 [实施记录](docs/realtime-video-composition-progress.md)。

## 2026-09-10 Beta profile与示例交付增量

代码冻结f41a6554（核心543f8565），真实120秒RKMPP Profile01及清晰版C示例由两名未参与实现者独立复核，Standards/Spec均PASS。首次0b71a44c构建失败暴露实时请求字段/转换遗漏，旧源码PASS已撤回；修正后全量构建及真实参数集验证成功。保留该过程，不能仅靠源码口头确认放行。

| 维度 | 满分 | A | B | 依据 |
|---|---:|---:|---:|---|
| 工业实现依据 | 25 | 24 | 24 | 复用FFmpeg AVOption及既有编码契约，实际RKMPP选项对照。 |
| Planner契约 | 20 | 19 | 19 | 初始/动态请求完整传递，profile纳入编码组相等。 |
| 生命周期与约束 | 25 | 23 | 23 | 同步字符串所有权、独立分支拒绝/退役，清晰示例不改变核心。 |
| 平台边界 | 10 | 8 | 8 | profile依用户要求先RKMPP，Windows新profile未测。 |
| 真实验收 | 20 | 17 | 18 | 同规格120秒、收发曲线/序号、SPS/PPS/PTL及四VLC画面；分差保留。 |
| **合计** | **100** | **91** | **92** | **本新增范围PASS；不覆盖或改写下方历史93/93评分。** |

风险/待优化：同包头文件与库必须配套重编；其他后端profile未由本次验证，物理TX时间、多小时资源趋势和驱动释放边界仍保留。本评分为增量证据审查，不是全仓重新扫描。

## 2026-09-10 动态视频多输出验收补齐

用户明确门禁为发送无超契约突发、动态增删正常、VLC正常解码。冻结98742fd6既往源码双审PASS；Windows同冻结证据按明确边界复核PASS，RKMPP03固定120秒真实链路三项PASS，独立成功提交471dc0c9/76c175b0。两位未参与实现者windows_scope_review_a/b独立复算本轮原始证据，均明确PASS。

| 维度 | 满分 | A | B | 更新依据 |
|---|---:|---:|---:|---|
| 工业实现依据 | 25 | 24 | 24 | 沿用冻结源码审查的工业方案对照。 |
| Planner契约 | 20 | 19 | 19 | 真实readback、时间基、组契约及容量产品不变。 |
| 生命周期与约束 | 25 | 23 | 23 | 沿用源码审查；RK实流68294次申请释放相等、payload归零。 |
| 平台边界 | 10 | 9 | 9 | 新依赖实际进程maps、哈希及共享DAG的RK实流已确认。 |
| 真实验收 | 20 | 18 | 18 | Windows/RKMPP三项完成；不外推长期或所有布局。 |
| **合计** | **100** | **93** | **93** | **本次范围PASS；证据增量评分，不是重审全库。** |

保留风险/待优化：软件出口与物理TX时刻有证据边界，接收端存在到包聚集；驱动内部最终释放仍无时限保证，MPP退出提示保留；controller职责拆分、READONLY潜在COW、长期资源趋势及未来音视频共同激活/同步属于后续演进，不扩成本轮验收。

## 2026-09-10 动态视频多输出增量审查（验收补齐前历史记录）

两名未参与实现的独立审查者 A/B 及新 PR 审查者对冻结 `98742fd6` 源码均 PASS，完整交付均 FAIL。下表是本冻结增量评分，不替换历史全仓评分；分差保留，不由实现者平均或提高。

| 维度 | 满分 | A | B | 新PR审查 | 依据与边界 |
|---|---:|---:|---:|---:|---|
| 工业实现依据 | 25 | 24 | 24 | 23 | GStreamer动态管线、FFmpeg tee/解码边界/私有包关闭、NVENC/HEVC随机访问及版本限定RKMPP/RGA。 |
| Planner契约 | 20 | 19 | 19 | 19 | 真实时间基、prepared readback、原事务期限、完整编码契约复用及统一依赖身份。 |
| 生命周期与约束 | 25 | 23 | 23 | 22 | 停止屏障、异步回收、真实引用计账；RK关闭修复有源码证据，待新实流归零。 |
| 平台边界 | 10 | 8 | 8 | 7 | 共享DAG，新依赖全量构建及七库哈希；新CLI实际映射与完整RK链路仍待验证。 |
| 真实验收 | 20 | 12 | 11 | 10 | Windows22/23/24动态及长GOP通过；最新25晚帧FAIL，不能以历史结果替代。 |
| **合计** | **100** | **86** | **85** | **81** | **源码审查PASS，完整交付FAIL。** |

优先风险：Windows晚帧根因未闭环；新RK链路和依赖关闭实流未通过；不可取消驱动的最终释放无时限保证。待优化：controller职责拆分、次级Release失败可观测性、READONLY引用可能触发COW、多小时资源趋势、未来音视频共同激活与持续漂移验收。

> Baseline scope: `codex/rkmpp-zero-copy`, scored 2026-08-17. Task5 evidence was appended on 2026-08-26; numeric scores remain frozen until the required two independent reviewers complete the current branch review.

| Dimension | Max | Score | Evidence / debt |
|---|---:|---:|---|
| Architecture and responsibility boundaries | 12 | 11 | Planner, builder, runtime validation and protocol output remain separated; the realtime surface is still broad. |
| Typed plans and planner authority | 12 | 12 | Backend, frame, transfer, filter, packetization and RTP ingress decisions are typed, serialized exactly and fail closed. |
| DAG shape and lifecycle | 10 | 8 | Strict shapes, bounded queues, RAII and source-loss propagation are preserved; bare RTP has no authoritative finite-source completion and RKMPP teardown remains noisy. |
| Zero-copy correctness | 14 | 14 | DRM PRIME format, planes, backing, dimensions and lineage are validated; original-size identity and RGA-only replacement are observed. |
| Protocol interoperability | 10 | 9 | H.264/HEVC separate RTP and H.265 SDP are real-media validated; broader receiver coverage remains. |
| A/V synchronization | 10 | 9 | Shared audio planner owns copy/transcode; synchronized output uses sample-domain correction with stable drift and zero drops. |
| Performance and resource bounds | 10 | 9 | RK VideoOnly averages 3.20% machine CPU with bounded 54 MB RSS; batched ingress reduced input calls and CPU, but long thermal soak is absent. |
| Diagnostics and error semantics | 8 | 8 | CPU/RSS, worker names, drift, DRM/RGA/transfer counters and final cause are explicit without suppressing target errors. |
| Cross-platform isolation | 7 | 5 | Shared code clean-builds on Windows; the Windows batched receive adapter and same-spec realtime acceptance remain frozen, not complete. |
| Maintainability and documentation | 7 | 6 | Planner materialization, adapter factory, plan decoder and receiver are separated; the overall realtime diff remains broad. |
| **Total** | **100** | **90** | **A-: RK VideoOnly is usable and bounded; lifecycle, Windows ingress and soak gates prevent a general production-ready claim.** |

## Task5 Datagram 发送控制证据增量（未重评分）

- 三类实时输出已统一为 protocol materializer、公共 service-scope `DatagramShaper` 和公共 nonblocking sender；file output 的 shape validator 明确排除 datagram 节点。
- `PreparedEncoderEmissionEnvelope`、`WireTrafficEnvelope`、graph/network resource ledger、PMTU 与 transport timing 产品均由 planner 形成；caller 不再提供 realtime queue、packet size、pacing rate、path MTU、receiver decode lead、batch/backlog/endpoint/socket/correlation 或 startup preroll。
- Windows VS2026 Release clean-first 已成功；Windows CUDA/NVENC 的 H.264 1280×720@30 raw RTP → HEVC 1920×1080@25 CBR 6 Mbps MP2T/RTP 完成固定源 120 秒复验。公共 queue-time drain 采用 WebRTC 公式并受部署容量硬上限约束，queue clock/commit accounting 的两类 TOCTOU 已通过临时 RED→GREEN 验证且临时测试未入库。
- Windows 最终复验提交 64516 datagrams；RTP loss/order、TS continuity/TEI/AFC、WouldBlock、deadline、pressure、partial/ambiguous submit 与 GCRA violation 均为 0，VLC 未记录 late/corrupt/discontinuity/decode error。TX timestamp 未被 adapter tracked，按 report policy 如实记为 untracked，不能据此声称 wire completion。
- 本增量不提高现有分数：最新公共队列改动的 RKMPP 同规格复验、56 链路矩阵、两名独立 reviewer 同时 PASS 和 Task6 最终验收仍未完成；Windows 平均单核 CPU 23.131% 按用户要求暂缓优化并保留为风险。

## Remaining priorities

1. Define an authoritative bare-RTP finite-source completion contract without converting source-clock loss into success.
2. Implement the planned Windows completion-driven batch adapter and repeat the same 2K realtime chain.
3. Resolve RKMPP teardown diagnostics and run multi-hour RKMPP/RGA thermal and repeated-source soak.
4. Complete AudioVideo batched ingress after the VideoOnly gate without creating a separate media path.

## 2026-09-04 RKMPP 持续运行修复专项评分

范围：`a5597326` 至本轮的 23 个生产文件；历史全仓评分保留，本表不代表全库重评。四项 CBR/VBR、H.264/HEVC 互转均完成约 248 秒 RKMPP 实流验收；冻结 dccf95da 已由两名独立智能体交叉审查并均明确 PASS，均建议专项评分维持 90/100。

| 维度 | 得分/满分 | 依据与未完成项 |
|---|---:|---|
| 工业实现依据 | 22/25 | RKMPP 真实异步接口、既有 LOW_DELAY 契约、RFC 1363 逐包债务；非完整 BQL/CoDel 算法移植。 |
| Planner 契约 | 19/20 | 批次、容量、轮询周期由既有事实规划；没有新对外参数。 |
| 生命周期与约束 | 22/25 | prefix 顺序提交、失败 abandon/poison、提交后唤醒闭合；100 ms 仅约束 wire 准入后的驻留。 |
| 平台边界 | 8/10 | 共享 DAG 复用；按用户要求不再进行 Windows 实测，不声明后续改动已覆盖。 |
| 真实验收证据 | 19/20 | 四项 RKMPP CBR/VBR 互转均以本地 2K30 高规格源持续约 248 秒；源期间核心不停，发送服务曲线超额 1356 B，RTP/TS 错误为 0，接收 RTP 零丢失，目标机 RKMPP 和 VLC 默认 D3D11VA 硬解通过。四项分别形成独立测试提交。 |
| **专项合计** | **90/100** | **本地高规格源完整链路 PASS；长期与平台覆盖风险保留。** |

残余风险：Linux TX timestamp telemetry 当前未跟踪；H.264→HEVC CBR 的 Windows Npcap 抓包晚于源开始约 59 秒，但有效连续窗口仍为 196.285 秒；裸 RTP 源自然结束后仍以 source-clock expiry 退出码 1 收敛；248 秒验收不能替代多小时硬件 soak。另有未物化尾部等待不计入 wire residence、LOW_DELAY 阻塞取包在硬件失去响应时可能阻塞 worker 的设计风险。详见 `docs/rk-a559-external-rtp-validation.md`。

追加损伤范围：run30 在输入 lo 设置 20% 丢包后启动探测失败，已有重排等待 5 秒与探测字节预算 5 MB 的组合在首个缺口后先耗尽预算。上述 90 分只覆盖原持续运行修复及四项零损伤验收，不代表 20% 损伤容忍能力；详见 docs/completed/2026-09-07-rk-a559-input-loss20-validation.md。

run31 正常启动后再注入 20% 输入丢包同样 FAIL：有效编码输出停止后触发原 12 秒进展超时，完整 IDR 缺失，VLC 有明显晚帧。该损伤运行项单独保留，不纳入原零损伤 PASS。

## 2026-09-07 输入丢包恢复审查状态

冻结 `3529c232` 的两名独立审查者均判标准与规格 **FAIL**：损伤关键 AU 准入不充分、FU/AU 追加容量未落实、PCR 维护批次无累计硬边界。run40 已证明同会话恢复、发送节奏及收发零丢包，尚未完成连续 VLC 解码补证；上述历史 90 分不适用于恢复增量。三项局部修复正在构建，待真实复测及两位重新明确 PASS 后按既有五维专项体系重评分。详细依据见 [审查记录](docs/completed/2026-09-07-rk-a559-recovery-review-3529c232.md)。
- 当前恢复代码 2d597ba6 的 run43 真实 RKMPP H.264→HEVC CBR 丢包恢复链路通过并已单独标记；这只补充运行证据，不替代两位独立复审。原评分不外推至本增量；此处为当时状态：复审曾因服务额度中断；服务恢复后的最终双 PASS 与专项评分见下节。

## 2026-09-07 输入丢包恢复增量独立评分

冻结生产代码 `2d597ba6`，两名未参与实现的审查者 A/B 均明确 **Standards PASS / Spec 源码 PASS**。以下为恢复增量专项评分，不替换历史全仓评分；评分时 run43 单项完整通过，其余恢复矩阵尚待补齐。

| 维度 | 满分 | A | B | 扣分依据与边界 |
| --- | ---: | ---: | ---: | --- |
| 工业实现依据 | 25 | 22 | 24 | 已逐项对照 WebRTC、GStreamer、FFmpeg、部署 RKMPP/RGA；没有反馈或重传事实，保守 AU 边界可能多舍弃一帧。 |
| Planner 契约 | 20 | 19 | 20 | 沿用既有时限、容量和能力事实，无新增调用参数；损伤启动仍受原探测预算门禁约束。 |
| 生命周期与约束 | 25 | 23 | 22 | 四项源码阻塞已关闭；裸 RTP 有限源退出语义、硬件失去响应与长期反复恢复风险保留。 |
| 平台边界 | 10 | 8 | 8 | 复用生产 DAG；按用户要求未进行 Windows 转码实测，不声称共享改动已获得全平台验证。 |
| 真实验收证据 | 20 | 16 | 14 | run43 完整通过；其余恢复矩阵、WouldBlock 特定分支运行覆盖及旧内核偶发突发仍需证据。 |
| **合计** | **100** | **88** | **88** | **源码双 PASS；不表示完整恢复矩阵或所有发送环境已通过。** |

审查后新增 run44 HEVC→H.264 CBR 恢复完整 PASS，独立提交 `5197f73a`；数值维持审查时评分，不自行加分。run41/run42 的旧内核突发根因尚未闭合。

后续证据：run45 H.264→HEVC VBR 源视频链路 PASS（EOF 后维护尾部未覆盖）；run46 HEVC→H.264 VBR 同会话恢复、191.14 秒有效 GPU 连续硬解且收发一致，但内核入队后突发 FAIL。分数不自行调整，完整矩阵仍未闭合。另有基线既存 Linux TX timestamp 编译条件与有限观测 ID 契约缺陷，尚未修改；它们属于诊断能力债务，不能据此解释或宣称修复内核聚集。

## 2026-09-08 实际源接收容量增量独立评分

冻结 `6fb07f19...73ad5fbd`，两名未参与实现者 A/B 均明确 Standards PASS / Spec（容量修复源码）PASS，分别评分如下；不替换历史全仓评分。

| 维度 | 满分 | A | B | 依据与边界 |
| --- | ---: | ---: | ---: | --- |
| 工业实现依据 | 25 | 24 | 24 | FFmpeg完整UDP接收、GStreamer超预期MTU处理；复用协议容量，固定大槽降低小包缓存效率。 |
| Planner 契约 | 20 | 20 | 20 | prepared transport权威容量，原预算推导槽数，无新增公开参数或fallback。 |
| 生命周期与约束 | 25 | 23 | 23 | 原RAII和截断拒绝保留；64槽深乱序与反复损伤组合尚待实测。 |
| 平台边界 | 10 | 8 | 8 | 共用planner/storage/DAG，Windows源码核对，按用户要求未做Windows转码实测。 |
| 真实验收证据 | 20 | 13 | 13 | run48输入240.06秒、硬解228.36秒、140575 RTP一致、大包无截断；内核突发FAIL，新槽丢包回归待做。 |
| **合计** | **100** | **88** | **88** | **容量修复双源码PASS，完整无突发门禁未通过。** |

用户明确要求先出库，已按已知限制交付73ad5fbd；发布行为不代表完整验收通过。详见run48与2026-09-08库交付报告。
## 2026-09-08 合并master完整增量交叉审查

冻结范围：master `d832820b` 至 `97d55142`，加BetaSession.cpp四行分类修复（SHA-256 `a0d8941ec334697cd854ef2b90b9c9f4d203d6bab8ed26784542008a52e0305e`）。两位未参与实现者A/B独立检查完整变更清单及功能/架构风险路径，均最终Standards PASS、Spec PASS、允许合并。此前历史审核缺口通过本次扩大审查补齐，不追认历史全量PASS。唯一P2出口IoFailure误归SOURCE_LOSS已修复并经两者复审。

| 维度 | 满分 | A | B | 依据与边界 |
|---|---:|---:|---:|---|
| 工业实现依据 | 25 | 23 | 24 | 接收、恢复、发送、硬件及错误分类有权威对照，不宣称公网拥塞控制。 |
| Planner契约 | 20 | 19 | 19 | 事实/readback、类型化容量及装配贯通；历史诊断容量债务保留。 |
| 生命周期与约束 | 25 | 22 | 22 | RAII、顺序/prefix提交、背压及错误传播；长期恢复和硬件失响应未穷尽。 |
| 平台边界 | 10 | 8 | 8 | 共享DAG与平台adapter分离，本轮无Windows转码实测。 |
| 真实验收证据 | 20 | 16 | 15 | RKMPP四路RC/编码方向损伤恢复闭合，未外推全部A/V、布局或多小时负载。 |
| **合计** | **100** | **88** | **88** | **本次合并范围PASS，不是全平台生产成熟度认证。** |

run53/55/56/57的CBR/VBR双向恢复均通过，核心实际入队发送满足既定门禁；历史驱动聚集仍是部署风险。run57输入捕获自身遗漏已单列。Beta红绿分类诊断与8核构建成功，CLI哈希不变，未重复四路媒体测试。详见2026-09-08-rk-beta-classification-review.md及各验收报告。
