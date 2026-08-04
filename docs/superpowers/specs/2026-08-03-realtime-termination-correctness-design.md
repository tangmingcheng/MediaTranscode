# Realtime Input Termination Correctness Design

## Objective

Make finite-source completion and source loss unambiguous across URL, MPEG-TS/UDP, and separate RTP inputs. A true FFmpeg EOF drains the production DAG and exits successfully. Network loss remains one preserved `IoFailure` with the native read or planned RTP timeout fact.

## Boundaries

- A shared protocol classifier owns FFmpeg read termination: only `AVERROR_EOF` is `EndOfStream`; an active interrupt is `Cancelled`; every other negative read result is `SourceLost` with its native code.
- The first worker that records a failure permanently owns the primary failure. A dedicated supervisor requests stop and interrupts all other workers; cancellation caused by that coordination is not another worker error.
- A worker records `Finished` independently from `running`. The executor reports completion only when every started worker finished and no primary failure exists.
- RTP fail-on-degraded is enforced at the raw input clock boundary using planner-provided observation deadlines. Downstream gates never compete to diagnose the same loss.
- SWR exhaustion is strong proof that no more output samples can appear. Only that proof authorizes exact settlement of the lineage accumulator's remaining residue.
- Video decoder EOF drain flushes codec buffers before generation lineage closure so FFmpeg opaque frame leases are released first.

## Control Flow

`EOF` drains and propagates terminal controls, all workers become `Finished`, the CLI observes threaded completion, and normal `stop()` closes the runtime. `SourceLost` records one primary failure and triggers coordinated interruption. Explicit `Abort` remains immediate cancellation and does not drain.

## Non-Goals

No public API change, planner timeout change, `--max-duration` change, fallback, default inference, CI, CTest, or committed automated test target is introduced.
