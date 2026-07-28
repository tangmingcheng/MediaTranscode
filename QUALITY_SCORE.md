# MediaTranscode Quality Score

> Scope: the complete project, evaluated against an industrial DAG media transcoding engine standard. Updated 2026-07-28.

| Dimension | Weight | Score | Evidence summary |
|---|---:|---:|---|
| Architecture and DAG model | 15 | 14 | RTP and MPEG-TS use one planned executable DAG and share the complete A/V synchronization path, branching only at the protocol output segment. Invalid output-only test assemblies were removed. |
| Planner decision ownership | 12 | 11 | Planner ranks hardware-only candidates by default, validates the complete selected chain lazily, permits software only when explicitly disabled, and owns RTP stream-association/RTCP policy consumed by transport and clock-group construction. The realtime planner remains large. |
| Scheduling and concurrency | 13 | 11 | Atomic metadata fan-out now prevents partial MPEG-TS session activation; graph validation rejects mixed atomic contracts. Long-running race evidence remains limited. |
| Node responsibility and decoupling | 12 | 9 | Production assembly remains in the realtime builder and protocol observers no longer reimplement runtime compilation. Several large planner, mux, codec, and runtime files remain. |
| RAII and resource safety | 10 | 10 | FFmpeg resources, including hardware device/frame capability probes, buffers, sessions, and workers use explicit ownership and RAII cleanup. |
| Performance and resource efficiency | 10 | 6 | SPSC, buffer pools, zero-copy, hardware paths, pacing, and profiling exist. Advanced optimizer and executor modules are not yet mature production optimizations. |
| Error model and failure boundaries | 8 | 8 | `Result`, `Status`, planner validation, required options, and primary worker failure propagation preserve actionable causes through runtime and CLI boundaries. |
| Tests and verification | 10 | 7 | Full-production RTP and MPEG-TS integration now observes protocol output, dual-stream decode, planned startup skew, and steady P99 skew. The integration tier is CI-only in the standard local cache, and long-duration objective drift remains outstanding. |
| Maintainability and documentation | 10 | 6 | Architecture and naming provide good navigation. Large source/test files and partially implemented advanced modules increase long-term maintenance cost. |
| **Total** | **100** | **82** | **A-: the new A/V synchronization path has one production assembly and protocol-level decode evidence; objective long-run drift and stress gates remain required.** |

## Primary Project Risks

1. Objective post-write A/V drift percentiles and long-run drift slope are not yet measured continuously.
2. The standard local cache does not compile the production integration target; CI is the current compile/run gate for its protocol observers.
3. Clock discontinuity and long-duration reacquisition still need a dedicated automatic group re-lock campaign.
4. Concurrent stop, abort, queue pressure, and 30-60 minute memory behavior still lack a systematic stress matrix.
5. Large realtime planner, capability probe, mux, codec resolver, and runtime files need later responsibility-focused decomposition.

## Priority Improvements

1. Add concurrency stress and fault-injection coverage for wakeup, full/empty queues, multi-input fairness, and every lifecycle transition.
2. Split oversized planner, mux, codec, and runtime files by validation, policy, FFmpeg session, protocol I/O, and state-machine responsibility.
3. Run production protocol integration, hardware, and performance tiers in CI and add 30-60 minute drift/memory gates.
4. Mark incomplete optimizer, GPU, and distributed capabilities experimental or reject unsupported execution explicitly.
5. Establish repeatable realtime performance and 30-60 minute stability baselines.
