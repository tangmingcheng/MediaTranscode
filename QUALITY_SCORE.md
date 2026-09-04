# MediaTranscode Quality Score

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

范围：`a5597326` 至本轮的 23 个生产文件；历史全仓评分保留，本表不代表全库重评。两位未参与实现的独立审查者重新审查当前全部差异，均给源码 PASS；完整实流验收仍未 PASS。

| 维度 | 得分/满分 | 依据与未完成项 |
|---|---:|---|
| 工业实现依据 | 22/25 | RKMPP 真实异步接口、既有 LOW_DELAY 契约、RFC 1363 逐包债务；非完整 BQL/CoDel 算法移植。 |
| Planner 契约 | 19/20 | 批次、容量、轮询周期由既有事实规划；没有新对外参数。 |
| 生命周期与约束 | 22/25 | prefix 顺序提交、失败 abandon/poison、提交后唤醒闭合；100 ms 仅约束 wire 准入后的驻留。 |
| 平台边界 | 8/10 | 共享 DAG 复用；按用户要求不再进行 Windows 实测，不声明后续改动已覆盖。 |
| 真实验收证据 | 11/20 | RKMPP 两轮输出超过 223 秒，核心及 deadline 错误为 0；接收曲线与 VLC 迟到未达完整门禁。 |
| **专项合计** | **82/100** | **源码 PASS；完整链路未通过。** |

残余风险：未物化尾部等待不计入 wire residence；LOW_DELAY 的阻塞取包在硬件失去响应时可能阻塞 worker；输入缺口、接收节拍异常、VLC 迟到和长期稳定性仍未关闭。详见 `docs/rk-a559-external-rtp-validation.md`。
