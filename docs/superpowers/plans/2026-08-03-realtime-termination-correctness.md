# P0 Realtime Input Termination Correctness Plan

**Goal:** Deliver clean EOF success, single-failure source-loss semantics, and strict codec terminal drain on `codex/realtime-termination-correctness`.

## Tasks

- [x] Capture current URL/HTTP, MPEG-TS/UDP, RTP degraded, SWR exhaustion, and codec lineage failure evidence.
- [x] Add and share the typed FFmpeg input read termination classifier.
- [x] Fail RTP degraded clock evidence at the input boundary with stream and planned timeout facts.
- [x] Add worker first-failure supervision and explicit finished/completed state.
- [x] Expose `MediaGraphRuntime::threadedCompleted()` and use it in realtime CLI completion.
- [x] Settle audio terminal residue only from SWR exhaustion proof.
- [x] Flush video decoder buffers before EOF generation closure.
- [x] Remove temporary RED/GREEN verification artifacts.
- [x] Rebuild all x64 Debug targets clean-first.
- [x] Pass exact P0 live-media gates and the nine-route matrix.
- [x] Update completion, README, root plan, progress, and quality score documents.
- [ ] Complete global self-review, commit, push, PR, and fresh-agent PASS review.

## Baseline Evidence

- URL/RTSP EOF: `formal-url-to-tsrtp/final-head-cli.log` records `audio correction exhaustion cannot prove partial non-zero compensation`, then `Codec flush left live lineage leases`.
- HTTP source close: `formal-url-to-tsudp/final-head-cli.log` preserves FFmpeg `native=-10054`, but runtime completion is failure-driven.
- MPEG-TS/UDP source close: `formal-tsudp-to-tsudp/final-head-cli.log` and `formal-tsudp-to-tsrtp/final-head-cli.log` preserve `native=-5` and then abort the runtime.
- Separate RTP source close: the three RTP-input final logs show downstream locked packet gates competing on degraded evidence; earlier final reports recorded 4-5 worker errors.

These failures are the RED acceptance baseline. Production behavior must remove EOF cleanup errors and collapse each source-loss run to one primary worker failure.
