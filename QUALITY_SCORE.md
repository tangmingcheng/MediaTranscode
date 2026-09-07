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
