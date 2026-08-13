# MediaTranscode Quality Score

> Scope: `codex/rkmpp-zero-copy`, updated 2026-08-13.

| Dimension | Max | Score | Evidence / debt |
|---|---:|---:|---|
| Architecture and responsibility boundaries | 12 | 11 | Planner, builder, runtime validation and protocol output remain separated; the realtime surface is still broad. |
| Typed plans and planner authority | 12 | 12 | Backend, frame, transfer, filter, packetization, SDP and audio branch decisions are typed and fail closed. |
| DAG shape and lifecycle | 10 | 9 | Strict shapes, bounded queues, RAII and source-loss propagation are preserved; target FFmpeg has an external RKMPP close-order defect. |
| Zero-copy correctness | 14 | 14 | DRM PRIME format, planes, backing, dimensions and lineage are validated; original-size identity and RGA-only replacement are observed. |
| Protocol interoperability | 10 | 9 | H.264/HEVC separate RTP and H.265 SDP are real-media validated; broader receiver coverage remains. |
| A/V synchronization | 10 | 9 | Shared audio planner owns copy/transcode; synchronized output uses sample-domain correction with stable drift and zero drops. |
| Performance and resource bounds | 10 | 8 | RK video-only CPU is low and RSS bounded; software AAC dominates full A/V CPU and long thermal soak is absent. |
| Diagnostics and error semantics | 8 | 8 | CPU/RSS, worker names, drift, DRM/RGA/transfer counters and final cause are explicit without suppressing target errors. |
| Cross-platform isolation | 7 | 7 | Windows clean build and fixed 120-second local/realtime chains pass using the unchanged default auto backend. |
| Maintainability and documentation | 7 | 6 | Shared Annex-B/Base64/packetization helpers avoid duplicate semantics; this delivery still changes a wide realtime surface. |
| **Total** | **100** | **93** | **A: production-ready for the validated RKMPP matrix, with upstream shutdown and soak risks recorded.** |

## Remaining priorities

1. Fix the installed FFmpeg RKMPP encoder close order upstream, then repeat source-loss shutdown validation.
2. Run multi-hour RKMPP/RGA thermal, memory, queue and repeated-source-replacement soak.
3. Profile and optimize the software AAC sample-domain path without weakening the synchronized-output contract.
4. Reduce the remaining large realtime planning/runtime translation units while preserving planner authority.
