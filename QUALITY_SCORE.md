# MediaTranscode Quality Score

> Scope: PR #25 against `master`, evaluated against an industrial DAG media transcoding engine standard. Updated 2026-07-29.

| Dimension | Weight | Score | Evidence summary |
|---|---:|---:|---|
| Architecture and DAG model | 15 | 12 | RTP and MPEG-TS share the synchronized input, canonical branches, scheduler, and executable DAG, branching only at the protocol output adapter. The realtime plan still carries a parallel legacy output product. |
| Planner decision ownership | 12 | 9 | Planner remains the policy authority and rejects incomplete synchronized products, but legacy pacing, transport, packetization, mux, and barrier fields are still populated or explicitly cleared after constructing `avSyncRuntime`. |
| Scheduling and concurrency | 13 | 11 | Atomic metadata fan-out now prevents partial MPEG-TS session activation; graph validation rejects mixed atomic contracts. Long-running race evidence remains limited. |
| Node responsibility and decoupling | 12 | 8 | Shared A/V segments are focused, but the realtime builder still assembles both synchronized and legacy output paths and remains oversized. Several large planner, mux, codec, and runtime files remain. |
| RAII and resource safety | 10 | 10 | FFmpeg resources, including hardware device/frame capability probes, buffers, sessions, and workers use explicit ownership and RAII cleanup. |
| Performance and resource efficiency | 10 | 6 | SPSC, buffer pools, zero-copy, hardware paths, pacing, and profiling exist. Advanced optimizer and executor modules are not yet mature production optimizations. |
| Error model and failure boundaries | 8 | 8 | `Result`, `Status`, planner validation, required options, and primary worker failure propagation preserve actionable causes through runtime and CLI boundaries. |
| Tests and verification | 10 | 5 | Four CI tiers pass, but the only full production RTP/MPEG-TS runtime closure, dual-stream decode, protocol artifact, and memory probe were deleted, as was scheduler commit/rollover linearization coverage. Subjective validation passed; objective long-run drift remains outstanding. |
| Maintainability and documentation | 10 | 6 | Architecture and naming provide good navigation. Large source/test files and partially implemented advanced modules increase long-term maintenance cost. |
| **Total** | **100** | **75** | **B: the shared production A/V path is operational, but merge is blocked by the parallel legacy plan product and loss of production-closure CI coverage.** |

## Review Verdict

**Not approved for merge.** Remove the legacy output-plan fields and translation/clearing path; restore resource-bounded CI coverage of the real RTP and MPEG-TS production closure without restoring the 4 GB behavior; restore focused scheduler commit/rollover concurrency coverage.

## Primary Project Risks

1. The realtime plan still transports legacy pacing, packetization, mux, and barrier state beside the canonical A/V runtime plan.
2. CI no longer closes the production RTP/MPEG-TS path through emitted protocol data and decode/artifact verification.
3. Scheduler output commit versus generation close no longer has a deterministic linearization regression.
4. Objective drift, clock discontinuity, concurrent lifecycle, queue pressure, and 30-60 minute memory behavior lack systematic gates.
5. Large realtime planner, builder, capability probe, mux, codec resolver, and runtime files need responsibility-focused decomposition.

## Priority Improvements

1. Delete legacy output-plan fields and make the canonical A/V runtime plan the only synchronized production product.
2. Add resource-bounded production closure tests for RTP decode and MPEG-TS artifact/timestamp validation.
3. Restore deterministic scheduler commit/rollover linearization coverage.
4. Add objective drift, discontinuity, concurrent lifecycle, and 30-60 minute memory gates.
5. Split oversized planner, builder, mux, codec, and runtime files by responsibility.
