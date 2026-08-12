# MediaTranscode Quality Score

> Scope: `codex/realtime-video-only-stream-set`, `master...5c7f362c`. Updated 2026-08-12.

## Scoring Rubric

| Dimension | Max | Score | Current evidence |
|---|---:|---:|---|
| Architecture responsibilities | 9 | 8 | Model, planner, builder, protocol, node and runtime-validation layers remain distinct; the branch is nevertheless broad at exactly 250 changed files. |
| Typed plans and contracts | 8 | 8 | Explicit stream-set and runtime variants model VideoOnly/A-V input, startup, scheduling, MPEG-TS program and output paths; prepared RTSP evidence and provenance are typed. |
| Planner authority | 8 | 8 | Stream selection, timing, capacities, protocol identity, prepared ownership and handoff limits are planned and fail closed; runtime consumers require exact materialized facts. |
| DAG shape and compilation | 8 | 8 | VideoOnly validation enforces exact nodes, ports, edges, PID/PES/SDP cardinality and legacy-node absence; A/V and video runtime variants compile separately. |
| Runtime scheduling | 8 | 8 | VideoOnly has one paced scheduler and shared protocol-output authority; A/V keeps the canonical scheduler, with exact queue budgets and source-clock liveness. |
| A/V synchronization | 8 | 8 | Prepared RTSP selects a bounded common timestamp window and preserves packet provenance; all counted A/V routes completed without recovery, discontinuity, duplicate or production drop. |
| Protocol outputs | 8 | 7 | Separate RTP, MPEG-TS/UDP and PT 33 MP2T RTP produce exact VideoOnly/A-V stream sets. Nine real RTSP/TCP routes prove publisher and reader signaling; VLC still reports route-dependent receiver warnings. |
| Concurrency, lifecycle and RAII | 8 | 7 | Prepared transports and generic RTSP capture use move-only ownership, bounded replay, interrupt restoration and joined workers; final PID and port residue is zero. Reconnect/replacement concurrency is not exercised. |
| Error semantics | 7 | 6 | Planning rejects missing, conflicting and undersized facts; runtime distinguishes clean drain, clock loss, cancellation and transient pressure. Fatal cancellation may intentionally purge bounded in-flight items. |
| Performance and memory | 7 | 6 | Explicit packet/byte bounds prevent silent growth; typical CLI working set is about 185-221 MiB with bounded CUDA startup transients, but no multi-hour soak or throughput envelope is established. |
| Observability | 5 | 4 | Runtime reports expose queues, workers, errors, drops, CPU, memory and drift. Evidence is detailed but remains distributed across local route directories rather than one machine-readable aggregate. |
| Real-media verification | 7 | 7 | The canonical 120-second source passed all 56 formal chains: local 2, MPEG-TS/UDP input 9, raw RTP 36 and real RTSP/TCP wire 9, with VLC frames, exact drains and zero residue. |
| Maintainability and documentation | 9 | 7 | Architecture, execution plan and concise completion evidence now describe the final topology and 56/56 matrix. The shared shape-query helper removes duplicate validator logic, but large units remain, notably the 1263-line realtime planner and 717-line VideoOnly shape validator. |
| **Total** | **100** | **92** | **A: production-ready for the validated finite-stream matrix, with bounded maintainability and operational-coverage debt.** |

## Verdict

The branch establishes an explicit `MediaTranscodeStreamSet` contract across local, MPEG-TS/UDP, raw RTP and RTSP inputs without introducing fake audio or a second scheduling authority. The prepared RTSP handoff closes the planning-to-runtime read gap with one owned FFmpeg context, bounded same-socket capture, original-order replay, exact timestamp provenance and fail-closed transfer; the final deadline check covers both stop initiation and completed ownership transfer, while explicit guard overloads keep optional cancellation private. True RTSP publisher and reader signaling is verified for all nine output/stream-set routes; the earlier HTTP-listen runs are correctly supplemental and excluded from the 56-chain count.

## Main Deductions

1. Several orchestration and exact-shape translation units remain large; the 250-file change carries substantial review and future-change surface despite focused helper extraction.
2. VLC retains route-dependent UDP TS continuity, RTP-loss, late-picture and teardown warnings. Production telemetry and continuous decoded frames are clean, but receiver interoperability is not warning-free.
3. Verification covers finite 120-second streams, not multi-hour soak, RTSP reconnect, publisher replacement, hostile traffic or repeated generation/SSRC transitions.
4. Prepared RTSP replay is explicitly bounded and fails closed, but operators must size packet and byte budgets for source rate plus planning latency.
5. Acceptance evidence is concise in the tracked report but detailed raw telemetry remains fragmented under untracked local route directories.

## Next Priorities

1. Run multi-hour memory/queue soak and repeated RTSP disconnect, reconnect and publisher-replacement validation with the same production DAG telemetry.
2. Split the realtime planner, builder and exact shape validation by input planning, runtime scheduling and output authority without duplicating policy.
3. Investigate receiver continuity, RTP-loss and late-picture warnings with packet-level evidence while preserving the canonical scheduler.
4. Consolidate per-route telemetry into a reviewable local summary artifact without introducing a committed automated-test system.
