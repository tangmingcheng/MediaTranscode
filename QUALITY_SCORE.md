# MediaTranscode Quality Score

## 2026-09-10 动态视频多输出增量审查

两名未参与实现的独立审查者 A/B 对当前冻结生产源码均 PASS；停止请求与回收竞态修复文件SHA256为 `838F1E716CF57CA60B84F1BB8B0E23F5B9E3F43E7A7D9E1EBF9BE17E9970DC72`。完整交付门禁尚未通过，下表不替换历史全仓评分。

| 维度 | 满分 | A | B | 依据与边界 |
|---|---:|---:|---:|---|
| 工业实现依据 | 25 | 23 | 23 | 对照GStreamer动态管线/排空、FFmpeg tee、NVENC/HEVC随机访问及固定版本RKMPP/RGA。 |
| Planner契约 | 20 | 19 | 19 | prepared readback、原事务期限、完整编码契约复用与类型化资源产品；依赖更换需重建证据。 |
| 生命周期与约束 | 25 | 23 | 22 | 停止请求屏障、异步候选/驱动清理、引用最终计账；不可取消驱动与高频增删长期风险保留。 |
| 平台边界 | 10 | 7 | 7 | 共享DAG与VideoFilter，只有能力adapter不同；RK新版本尚无真实通过证据。 |
| 真实验收 | 20 | 10 | 10 | 历史Windows13/16通过；当前Windows21仍有VLC晚帧，长GOP和RKMPP未闭环。 |
| **合计** | **100** | **82** | **81** | **源码双PASS，完整交付未通过。** |

待优化：controller文件职责继续拆分；次级Release失败可观测性；READONLY引用可能触发COW；多小时资源趋势；未来音视频共同激活与持续漂移验收。最终冻结commit/PR仍须复核。

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
