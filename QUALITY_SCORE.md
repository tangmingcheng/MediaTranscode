# MediaTranscode Quality Score

> Scope: `codex/realtime-cross-layout` against `origin/master` at `3852ca99fb8d2efc5fd164f9464e442165b9f6d5`, including the current tracked feature files. Updated 2026-07-31.

| Dimension | Weight | Score | Evidence summary |
|---|---:|---:|---|
| Architecture and DAG model | 15 | 15 | Input clock and output transport are orthogonal planner products; all nine layouts converge on one canonical scheduler and exactly one output adapter. |
| Planner decision ownership | 12 | 12 | Protocol, topology, queue, timing, and transport facts are planner-owned; downstream plan decoding now receives every zero-value policy explicitly. |
| Scheduling and concurrency | 13 | 11 | Atomic generation/audio commits, bounded queues, persistent RTP sequence/counter state, key-frame restart, and TS discontinuity are explicit. A clean real-media generation-transition gate is still missing. |
| Node responsibility and decoupling | 12 | 8 | Clock adapters, mux, transport, SDP, validation, and runtime state are separated, but the 790-line MPEG-TS plan codec and several 400-633-line runtime/planner files remain broad. |
| RAII and resource safety | 10 | 10 | FFmpeg objects, sockets, sessions, packet cursors, commit reservations, and shared continuity state use scoped ownership. |
| Performance and resource efficiency | 10 | 9 | Accepted paths stayed near 3-6% process CPU with stable late working sets; one URL/RTSP-to-TS/UDP gate recorded 42 bounded drops. |
| Error model and failure boundaries | 8 | 8 | Invalid and incomplete plans fail closed; runtime generation, transport, and commit identities are checked explicitly. |
| Tests and verification | 10 | 9 | All nine paths have real CLI/FFmpeg/VLC evidence. Post-review reruns cover the exact remote-IP MP2T failure, FFmpeg decode from first output, a 60-second visible gate, and cross-generation continuity telemetry. |
| Maintainability and documentation | 10 | 7 | Architecture, README, plan, completion report, and exact commands for all nine routes are current; positional option serialization and large files remain costly. |
| **Total** | **100** | **89** | **Provisional B+: blocking review findings are fixed and awaiting independent re-review.** |

## Review Verdict

**RE-REVIEW PENDING.** The previous independent review found three blockers:
32-bit RTCP rollover, incomplete exact command evidence, and missing observable
cross-generation outer-RTP continuity. The implementation and evidence now
address all three; the score and verdict remain provisional until the same
reviewer confirms the fixes.

## Source-Driven Lifetime Follow-Up

The realtime CLI duration is now optional and confined to CLI monitoring;
absence creates no deadline or fallback value in the production DAG. The nine
real-media routes were rerun with finite sources and no `--max-duration`.
During playback every route had zero worker/runtime errors and drops; all
remained below 6% average process CPU with stable late working sets and no
perceptible A/V drift. CLI termination followed source loss on every route.

## Acceptance Evidence

- Nine source-driven real-media input/output paths have final CLI logs under
  `out/acceptance/realtime-cross-layout/`; human observation passed every row.
- Final MP2T SDP was opened by VLC and directly by FFmpeg. FFmpeg decoded
  20.00 seconds, 578 video frames, and about 3.75 MiB of audio at 1.01x with
  exit code 0.
- Final separate RTP SDP was directly decoded by FFmpeg for 20.00 seconds:
  586 video frames and about 3.75 MiB of audio at 1.04x, exit code 0.
- The shared SDP serializer now publishes the connection address at session
  level and rejects media whose RTP destination differs from that planned
  session address.
- Post-fix MPEG-TS/UDP to MPEG-TS/RTP: 136.2 seconds, zero worker/runtime errors/drops, 5.475% process CPU, 251,850,752-byte final working set, and no perceptible A/V drift.
- Post-fix MPEG-TS/UDP to separate RTP: 155.5 seconds, zero worker/runtime errors/drops, 5.236% process CPU, 245,547,008-byte final working set, and no perceptible A/V drift.
- The other seven gates are configured for 145/170 seconds and each log exceeds 120 seconds.
- No temporary test source, target, executable, object, symbol, or build directory remains.
- URL/RTSP to MPEG-TS/UDP completed with zero worker/runtime errors and 42 bounded drops.
- The exact `192.168.96.185:52500` MP2T command completed 30 seconds after sender-buffer planning changed, with zero worker/runtime errors or drops.
- A final local visible MP2T gate completed 60 seconds with zero worker/runtime errors or drops, 4.60% average process CPU, stable late memory, no corruption, and no perceptible A/V drift.
- Generation-first telemetry proved that sequence deltas and cumulative packet-count deltas matched exactly across three forced transitions.

## Primary Risks

1. The MPEG-TS plan codec uses many positional integers and string keys, increasing protocol-evolution risk.
2. URL/RTSP to MPEG-TS/UDP produced 42 bounded drops; longer source-specific observation is advisable.
3. Manual-only gates provide no deterministic concurrency, fault-injection, or regression safety net.
4. Long-duration generation-transition and fault-injection coverage remains manual.
5. Finite URL/RTSP and MPEG-TS/UDP source loss is still surfaced as a runtime
   error; URL/RTSP-to-MPEG-TS/RTP also reports a source-end audio flush error.

## Priority Improvements

1. Resolve or formally bound the 42 URL/RTSP-to-MPEG-TS/UDP drops.
2. Add a longer manual generation-transition/fault-injection gate.
3. Split the MPEG-TS plan codec and other oversized runtime/planner files by protocol fact group and responsibility.
