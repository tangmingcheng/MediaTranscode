# MediaTranscode Quality Score

> Scope: `codex/rtp-fmtp-autodetect` against `origin/master`. Updated 2026-08-05.

| Dimension | Weight | Score | Evidence |
|---|---:|---:|---|
| Architecture and DAG model | 15 | 15 | Probe execution, typed facts, planner resolution, prepared binding, and runtime consumption remain separate modules. |
| Planner decision ownership | 12 | 12 | Codec/PT/clock and parameter-set identity are validated before planning; runtime has no automatic fallback. |
| Scheduling and concurrency | 13 | 12 | Both 1280x720 A/V routes retain the canonical scheduler with zero errors, drops, and stalls. |
| Node responsibility and decoupling | 12 | 10 | Shared NAL parsing removes probe/depacketizer duplication; existing large realtime planner files remain debt. |
| RAII and resource safety | 10 | 10 | One move-only prepared owner carries the bound transport through compile/start/failure paths. |
| Performance and resource efficiency | 10 | 9 | Two 120-second software paths show stable late memory and no monotonic growth; longer soak remains manual. |
| Error model and failure boundaries | 8 | 8 | Timeout, PT mismatch, incomplete/conflicting sets, capacity exhaustion, and missing AAC fmtp fail preflight. |
| Tests and verification | 10 | 8 | Clean rebuild and real CLI/FFmpeg gates pass; approved VLC visual observation is still pending. |
| Maintainability and documentation | 10 | 9 | CLI help, README, architecture, plan, completion evidence, and residual risks are current. |
| **Total** | **100** | **93** | **A** |

## Review Verdict

**PROVISIONAL 93/100, A.** Machine gates and local two-axis review are complete. Final score requires the approved VLC visual gate and fresh PR-head independent review.

## Primary Risks

1. AAC still requires authoritative fmtp; bare RTP does not identify codec, PT, or clock rate.
2. HEVC DONL/interleaving cannot be inferred authoritatively from bare RTP and is supported only under the planned SRST/no-DONL contract.
3. Runtime parameter-set switching after startup is outside this delivery.
4. Manual real-stream gates remain the regression system by repository policy.
5. Long-duration repeated SSRC transitions and hostile datagram floods need additional soak evidence.
