# MediaTranscode Quality Score

> Scope: `codex/realtime-termination-correctness` against `origin/master`, including the P0 termination diff and retained nine-route evidence. Updated 2026-08-03.

| Dimension | Weight | Score | Evidence summary |
|---|---:|---:|---|
| Architecture and DAG model | 15 | 15 | Input clock and output transport are orthogonal planner products; all nine layouts converge on one canonical scheduler and exactly one output adapter. |
| Planner decision ownership | 12 | 12 | Protocol, topology, queue, timing, and transport facts are planner-owned; downstream plan decoding now receives every zero-value policy explicitly. |
| Scheduling and concurrency | 13 | 12 | First failure is permanent, peers are interrupted once, workers distinguish running from Finished, and the executor proves all-worker completion. Direct runtime nodes still own individual output-close contracts. |
| Node responsibility and decoupling | 12 | 9 | Input termination, failure supervision, and encoder packet lineage are focused modules; several existing scheduler, factory, runtime, mux, builder, and planner files remain oversized. |
| RAII and resource safety | 10 | 10 | FFmpeg objects, sockets, sessions, packet cursors, commit reservations, and shared continuity state use scoped ownership. |
| Performance and resource efficiency | 10 | 10 | Nine sequential routes stayed at 3.498-4.367% realtime CLI `averageProcessCpuPercent`, zero drops, and stable 244-258 MB late working sets. Machine CPU and parallel EOF runs are excluded. |
| Error model and failure boundaries | 8 | 8 | Only true EOF succeeds; HTTP `-10054`, MPEG-TS/UDP `-5`, and RTP planner timeouts each preserve one source-loss primary while coordinated cancellation is excluded. |
| Tests and verification | 10 | 9 | All nine paths have real CLI/FFmpeg/VLC evidence. Post-review reruns cover the exact remote-IP MP2T failure, FFmpeg decode from first output, a 60-second visible gate, and cross-generation continuity telemetry. |
| Maintainability and documentation | 10 | 7 | Architecture, README, plan, completion report, and exact commands for all nine routes are current; positional option serialization and large files remain costly. |
| **Total** | **100** | **92** | **A-: P0 EOF, source-loss, worker coordination, and codec drain are accepted by real-media evidence.** |

## Review Verdict

**INDEPENDENT SCORE RECOMMENDATION: 92/100, A-. PR REVIEW: PENDING.** The P0
scoring agent raised scheduling, responsibility, performance, and error-boundary
scores after reviewing the strict classifier, first-failure supervisor, explicit
completion, codec terminal proof, and nine sequential process-CPU results. The
final score remains subject to the required full PR-diff review.

## Source-Driven Lifetime Follow-Up

The realtime CLI duration is now optional and confined to CLI monitoring;
absence creates no deadline or fallback value in the production DAG. The nine
real-media routes were rerun with finite sources and no `--max-duration`.
During playback every route had zero worker/runtime errors and drops; all
remained below 4.4% average realtime CLI process CPU with stable late working sets and no
perceptible A/V drift. CLI termination followed source loss on every route.

True local EOF now exits successfully with zero worker/runtime errors and exact
2938/2938 encoded packet drain. HTTP, MPEG-TS/UDP, and separate RTP sender loss
each produce one accurate primary error with no downstream worker-error cascade.

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
- Every final nine-route playback gate completed with zero worker/runtime errors and zero drops.
- The exact `192.168.96.185:52500` MP2T command completed 30 seconds after sender-buffer planning changed, with zero worker/runtime errors or drops.
- A final local visible MP2T gate completed 60 seconds with zero worker/runtime errors or drops, 4.60% average process CPU, stable late memory, no corruption, and no perceptible A/V drift.
- Generation-first telemetry proved that sequence deltas and cumulative packet-count deltas matched exactly across three forced transitions.

## Primary Risks

1. The MPEG-TS plan codec uses many positional integers and string keys, increasing protocol-evolution risk.
2. Manual-only gates provide no deterministic concurrency, fault-injection, or regression safety net.
3. Long-duration generation-transition and fault-injection coverage remains manual.
4. Connectionless UDP cannot distinguish a normally ended sender from source disappearance.
5. Some RTP/MPEG-TS input routes recorded one nonfatal stalled interval.
6. A late-joining MP2T FFmpeg receiver logged missing-PPS/no-frame startup
   errors until the next decodable parameter-set/key-frame boundary.
7. Windows dynamically excluded UDP ports can invalidate local bind-based acceptance.

## Priority Improvements

1. Add a longer manual generation-transition/fault-injection gate.
2. Split the MPEG-TS plan codec and other oversized runtime/planner files by protocol fact group and responsibility.
3. Reduce late-join MP2T decode latency by proving parameter-set/key-frame
   availability at receiver attachment.
