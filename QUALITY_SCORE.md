# MediaTranscode Quality Score

> Scope: `codex/realtime-cross-layout` against `origin/master` at `3852ca99fb8d2efc5fd164f9464e442165b9f6d5`, including the current tracked and named untracked feature files. Updated 2026-07-30.

| Dimension | Weight | Score | Evidence summary |
|---|---:|---:|---|
| Architecture and DAG model | 15 | 15 | Input clock and output transport are orthogonal planner products; all nine layouts converge on one canonical scheduler and exactly one output adapter. |
| Planner decision ownership | 12 | 12 | Protocol, topology, queue, timing, and transport facts are planner-owned; downstream plan decoding now receives every zero-value policy explicitly. |
| Scheduling and concurrency | 13 | 11 | Atomic generation/audio commits, bounded queues, persistent RTP sequence/counter state, key-frame restart, and TS discontinuity are explicit. A clean real-media generation-transition gate is still missing. |
| Node responsibility and decoupling | 12 | 8 | Clock adapters, mux, transport, SDP, validation, and runtime state are separated, but the 790-line MPEG-TS plan codec and several 400-633-line runtime/planner files remain broad. |
| RAII and resource safety | 10 | 10 | FFmpeg objects, sockets, sessions, packet cursors, commit reservations, and shared continuity state use scoped ownership. |
| Performance and resource efficiency | 10 | 9 | Accepted paths stayed near 3-6% process CPU with stable late working sets; one URL/RTSP-to-TS/UDP gate recorded 42 bounded drops. |
| Error model and failure boundaries | 8 | 8 | Invalid and incomplete plans fail closed; runtime generation, transport, and commit identities are checked explicitly. |
| Tests and verification | 10 | 9 | All nine paths have real CLI/FFmpeg/VLC evidence beyond two minutes. The two post-fix gates ran 136.2 and 155.5 seconds with zero worker/runtime errors or drops. |
| Maintainability and documentation | 10 | 7 | Architecture, README, plan, completion report, and exact commands are current and consistently encoded; positional option serialization and large files remain costly. |
| **Total** | **100** | **89** | **B+: implementation, repository standards, and real-media acceptance pass; remaining gaps are non-blocking maintainability and long-soak risks.** |

## Review Verdict

**FINAL PASS.** Planner-only ownership, RAII, responsibilities, duplicate semantics, temporary-test cleanup, UTF-8/CRLF hygiene, build, and all nine real-media gates satisfy the delivery contract.

## Acceptance Evidence

- Nine real-media input/output paths have CLI logs under `out/acceptance/realtime-cross-layout/`.
- Post-fix MPEG-TS/UDP to MPEG-TS/RTP: 136.2 seconds, zero worker/runtime errors/drops, 5.475% process CPU, 251,850,752-byte final working set, and no perceptible A/V drift.
- Post-fix MPEG-TS/UDP to separate RTP: 155.5 seconds, zero worker/runtime errors/drops, 5.236% process CPU, 245,547,008-byte final working set, and no perceptible A/V drift.
- The other seven gates are configured for 145/170 seconds and each log exceeds 120 seconds.
- No temporary test source, target, executable, object, symbol, or build directory remains.
- URL/RTSP to MPEG-TS/UDP completed with zero worker/runtime errors and 42 bounded drops.

## Primary Risks

1. The MPEG-TS plan codec uses many positional integers and string keys, increasing protocol-evolution risk.
2. URL/RTSP to MPEG-TS/UDP produced 42 bounded drops; longer source-specific observation is advisable.
3. Manual-only gates provide no deterministic concurrency, fault-injection, or regression safety net.
4. Long-duration generation-transition and fault-injection coverage remains manual.

## Priority Improvements

1. Resolve or formally bound the 42 URL/RTSP-to-MPEG-TS/UDP drops.
2. Add a longer manual generation-transition/fault-injection gate.
3. Split the MPEG-TS plan codec and other oversized runtime/planner files by protocol fact group and responsibility.
