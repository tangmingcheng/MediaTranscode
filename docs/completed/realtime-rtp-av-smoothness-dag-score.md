# Realtime RTP A/V DAG Score

## Scope

Review target: realtime raw RTP audio+video smoothness changes in `src/internal/graph`.

## Score

- Planner ownership: 8.5/10. Queue policy, threading policy, audio copy/transcode, input start gate, mux pacing, and startup barrier are planner-owned.
- DAG separation: 8/10. Raw RTP A/V input chains are isolated, and packet start gating is a separate node before decode. Cross-stream output scheduling is still per-mux plus barrier, not a unified session scheduler.
- Runtime determinism: 8/10. Realtime worker idle sleep is disabled, fixed sleeps are classified, and RTP pacing is explicit. Startup delay still uses node-local sleep and should become scheduler-owned P1 work.
- Backpressure behavior: 8.5/10. Realtime RTP data lanes use non-blocking SPSC ring queues, normal overflow drops, and keyframe-preserving `DropNonKeyFrame` semantics. Further telemetry should expose per-edge drops for production operations.
- Verification strength: 9/10. Clean rebuild, unit tests, `ctest`, 20s RTP diagnostics, 60s hardware receiver smoke, and visible VLC human validation were run.

Overall: 8.4/10.

## Verdict

Pass for this root-cause closure after independent review found and the implementation fixed the keyframe-drop edge case. The remaining architectural work is P1: session-level RTP output scheduler, event-driven worker wait model, and codec-aware RTP start readiness beyond packet keyframe flags.
