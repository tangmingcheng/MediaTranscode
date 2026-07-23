# MediaTranscode Quality Score

> Scope: the complete project, evaluated against an industrial DAG media transcoding engine standard. Updated 2026-07-23.

| Dimension | Weight | Score | Evidence summary |
|---|---:|---:|---|
| Architecture and DAG model | 15 | 13 | Explicit graph, topology, validation, descriptors, and documented planner/builder/runtime layering. Several optimizer, distributed, and GPU areas remain framework-level. |
| Planner decision ownership | 12 | 11 | Planner ranks hardware-only candidates by default, validates the complete selected chain lazily, and permits software only when explicitly disabled. The realtime planner remains large. |
| Scheduling and concurrency | 13 | 10 | The graph-level A/V scheduler, first-worker-failure preservation, bounded CLI startup, channels, wakeups, and lifecycle abstractions are covered. Long-running race evidence remains limited. |
| Node responsibility and decoupling | 12 | 8 | Capability-oriented node and segment structure is clear. Several planner, mux, codec, and runtime files exceed 400-700 lines and still combine multiple concerns. |
| RAII and resource safety | 10 | 10 | FFmpeg resources, including hardware device/frame capability probes, buffers, sessions, and workers use explicit ownership and RAII cleanup. |
| Performance and resource efficiency | 10 | 6 | SPSC, buffer pools, zero-copy, hardware paths, pacing, and profiling exist. Advanced optimizer and executor modules are not yet mature production optimizations. |
| Error model and failure boundaries | 8 | 8 | `Result`, `Status`, planner validation, required options, and primary worker failure propagation preserve actionable causes through runtime and CLI boundaries. |
| Tests and verification | 10 | 6 | Planner, node, Task 5 A/V sync, runtime lifecycle, and live hardware RTP paths are exercised. Nine known runtime baseline assertions and the receiver audio-remux gate remain unresolved. |
| Maintainability and documentation | 10 | 6 | Architecture and naming provide good navigation. Large source/test files and partially implemented advanced modules increase long-term maintenance cost. |
| **Total** | **100** | **78** | **B+: the new A/V synchronization and hardware-first runtime paths are closed for subjective use, with objective drift, reacquisition, and long-run gates still required.** |

## Primary Project Risks

1. Objective post-write A/V drift percentiles and long-run drift slope are not yet measured continuously.
2. Clock discontinuity currently reports explicit reacquisition requirements but does not complete automatic group re-lock.
3. Nine existing runtime-test assertions and the RTP receiver's AAC-to-TS remux capture contract remain unresolved.
4. Concurrent stop, abort, queue pressure, and long-duration behavior still lack a systematic stress matrix.
5. Large realtime planner, capability probe, mux, codec resolver, and runtime files need later responsibility-focused decomposition.

## Priority Improvements

1. Add concurrency stress and fault-injection coverage for wakeup, full/empty queues, multi-input fairness, and every lifecycle transition.
2. Split oversized planner, mux, codec, and runtime files by validation, policy, FFmpeg session, protocol I/O, and state-machine responsibility.
3. Separate tests into layer-specific targets and run integration, hardware, and performance tiers in CI.
4. Mark incomplete optimizer, GPU, and distributed capabilities experimental or reject unsupported execution explicitly.
5. Establish repeatable realtime performance and 30-60 minute stability baselines.
