# MediaTranscode Quality Score

> Scope: `codex/realtime-termination-correctness` against `origin/master`. Updated 2026-08-03.

| Dimension | Weight | Score | Evidence |
|---|---:|---:|---|
| Architecture and DAG model | 15 | 15 | Nine input/output layouts converge on the canonical scheduler and one planned output adapter. |
| Planner decision ownership | 12 | 12 | RTP degraded-clock termination is a typed planner product; nodes receive explicit policy and timing facts. |
| Scheduling and concurrency | 13 | 12 | First failure is permanent, coordinated peers stop once, and all-worker `Finished` proves clean completion. |
| Node responsibility and decoupling | 12 | 9 | Input termination, failure supervision, and packet lineage are focused modules; several existing planner/runtime files remain oversized. |
| RAII and resource safety | 10 | 10 | Worker owners outlive worker destruction; FFmpeg objects, sockets, sessions, leases, and reservations remain scoped. |
| Performance and resource efficiency | 10 | 10 | Nine sequential routes used 3.498-4.367% realtime CLI `averageProcessCpuPercent`, zero drops, and stable 244-258 MB late working sets. |
| Error model and failure boundaries | 8 | 8 | EOF succeeds; HTTP `-10054`, MPEG-TS/UDP `-5`, and RTP 7/9/9-second loss facts each produce one primary failure. |
| Tests and verification | 10 | 9 | Real CLI/FFmpeg/VLC gates cover nine routes; final reviewed-head P0 logs and a visible VLC gate are retained locally. No automated safety net exists by policy. |
| Maintainability and documentation | 10 | 7 | README, plans, completion report, and full direct commands are current; positional plan codecs and large files remain debt. |
| **Total** | **100** | **92** | **A-** |

## Review Verdict

**INDEPENDENT SCORE: 92/100, A-. PR REVIEW: PASS.** The independent final review verified the executor lifetime, MPEG-TS classification precedence, planner-owned video/audio loss boundaries, exact-PID evidence, truthful secondary-failure accounting, and invalid-policy rejection at `c1433dd4`.

## Acceptance Evidence

- Clean local EOF: exit 0, `workerErrors=0`, `errors=0`, `droppedBuffers=0`, and 2938/2938 encoded packets drained.
- HTTP, MPEG-TS/UDP, and separate RTP source stops: exit 1 with exactly one worker/runtime error and preserved native/timeout facts.
- Nine sequential input/output routes: 3.498-4.367% realtime CLI process CPU, zero worker/runtime errors and drops during playback, stable late memory, visible picture and audible output.
- Final reviewed-head visible VLC gate: exit 0, 5.243545% realtime CLI process CPU, 246,734,848-byte late working set, and zero worker/runtime errors or drops.

## Primary Risks

1. Connectionless UDP cannot distinguish normal sender end from disappearance.
2. Manual gates provide no deterministic concurrency or regression safety net.
3. Long-duration fault injection and repeated generation transitions remain manual.
4. Some RTP/MPEG-TS routes recorded one nonfatal stalled interval.
5. Late RTP join can wait for the next parameter-set/key-frame boundary.
6. Windows dynamically excluded UDP ports can invalidate local bind acceptance.
