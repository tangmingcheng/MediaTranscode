# RTP Video FMTP Auto-Detection Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Detect strict H.264/HEVC video FMTP from separate raw RTP before graph construction and transfer the same live transport into runtime.

**Architecture:** A planner-owned probe plan drives a bounded raw RTP preparer. Typed signaling facts return to the planner, while a prepared-input buffer retains the transport and original datagrams for exact runtime binding.

**Tech Stack:** C++20, MediaTranscode DAG planner/builder/runtime, Win32 UDP RTP/RTCP transport, FFmpeg/VLC, Visual Studio 2026 CMake/Ninja.

## Global Constraints

- Planner is the only decision owner; runtime receives complete parameters and never guesses or falls back.
- `analyze-duration-us` is the maximum interval to obtain complete unambiguous
  facts, not a mandatory dwell after the facts are complete.
- `open-timeout-ms` bounds controllable startup work; non-cancellable
  FFmpeg/driver capability calls are checked for overrun immediately on return.
- H.264/HEVC video only; AAC FMTP remains manual and Opus keeps its existing no-FMTP contract.
- Do not add or restore tracked tests. Use real CLI media chains and removable local tools under `out/`.
- Preserve canonical A/V synchronization and all existing manual raw RTP behavior.
- UTF-8 and CRLF for every changed text file.

### Task 1: Signaling facts and shared NAL assembly

- [x] Add typed H.264/HEVC signaling facts and canonical FMTP serialization.
- [x] Extract shared H.264/HEVC RTP NAL assembly used by both detection and depacketization.
- [x] Verify strict duplicate, conflict, malformed, and unsupported packetization behavior with direct malformed real RTP sources.
- [x] Commit the focused signaling change.

### Task 2: Raw RTP preflight and prepared input

- [x] Add the explicit raw RTP probe plan and bounded preparer.
- [x] Add the `RawRtp` prepared-input buffer holding the RAII transport and current-SSRC datagram queue.
- [x] Integrate auto mode into preflight and planner FMTP resolution; keep manual mode I/O-free.
- [x] Commit the focused preflight change.

### Task 3: Runtime handoff and CLI contract

- [x] Bind raw prepared input to the exact video `RawRtpInputNode`.
- [x] Consume buffered datagrams before receiving from the same transport.
- [x] Make video FMTP optional at the CLI while retaining strict AAC validation.
- [x] Add safe detection diagnostics and commit the focused runtime change.

### Task 4: Build and real-media acceptance

- [x] Run the mandated clean-first x64 Debug rebuild.
- [x] Run two-minute H.264 A/V automatic detection and FFmpeg/VLC output validation.
- [x] Run two-minute HEVC automatic detection and H.264 output validation.
- [x] Run manual H.264 regression and strict preflight failure gates.
- [x] Remove all temporary validation tools and confirm no tracked test residue.

### Task 5: Documentation, quality, and delivery

- [x] Update README, ARCHITECTURE, completion evidence, root plan, and QUALITY_SCORE.
- [x] Review the full diff for planner authority, RAII, duplication, lifecycle, UTF-8/CRLF, and `git diff --check`.
- [x] Commit and push all changes, create a ready PR, and record residual risks.
- [x] Have a fresh agent review the PR; fix and revalidate until it returns PASS.

### Task 6: Synchronize prepared A/V transport ownership

**Files:**

- Modify: `src/internal/graph/planner/realtime/MediaRawRtpInputPreparer.h`
- Modify: `src/internal/graph/planner/realtime/MediaRawRtpInputPreparer.cpp`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeInputPlanner.h`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeInputPlanner.cpp`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.cpp`
- Modify: `src/internal/graph/builder/realtime/MediaRealtimeRtpTranscodeGraphBuilder.cpp`
- Modify only if required by the resulting interface: `src/internal/graph/planner/realtime/MediaPreparedRealtimeInput.h`

**Interfaces:**

- Consumes: explicit video/audio RTP endpoints, codec identity, PT, clock rate, probe limits, and video packetization policy from `MediaRealtimeRtpTranscodeRequest`.
- Produces: two independently owned `MediaPreparedRealtimeInput` values identified as video and audio, plus the existing `MediaDetectedRtpVideoSignaling` facts.
- Planner product: both raw RTP node plans set `requiresPreparedInput=true` only in automatic video fmtp mode; manual mode sets neither prepared requirement and performs no network I/O.
- Executable product: each prepared owner is bound exactly once to the `RawRtpInputNode` whose planned `rtp.stream_kind` matches its stream identity.

- [x] **Step 1: Preserve the failing production evidence**

  Record the observed automatic-mode failure: video prepared transport starts before audio, `locked_generation=1`, `state_generation=2`, peak queue 829, then `A/V startup clock rejects malformed reacquisition evidence`. Do not add a tracked test or test script.

- [x] **Step 2: Deepen the preparer interface around an A/V session**

  Make the raw RTP preparer open both explicitly planned transports before receiving probe evidence. Probe only video; capture audio as opaque RTP/RTCP datagrams with original monotonic arrival times. Both streams share the request's single aggregate `probe-size` capacity while returning distinct RAII transport owners.

- [x] **Step 3: Carry typed per-stream prepared resources through preflight**

  Replace the single raw-RTP prepared resource assumption with a per-stream prepared collection. Reject duplicate stream identities, missing video/audio resources, kind mismatches, or an audio prepared resource when no isolated audio input exists.

- [x] **Step 4: Bind both resources without runtime decisions**

  Have the planner mark both raw input node plans as prepared-required in automatic mode. Have the executable builder match planned stream identity to the exact node and create two bindings. Keep compiler and runtime factory symmetric: required means one exact binding; node-owned means no binding.

- [x] **Step 5: Verify the root-cause fix with the production chain**

  Rebuild only through `.agents/skills/building-with-vs2026/scripts/rebuild_debug.ps1`. Then start VLC, CLI, and the absolute FFmpeg command using `out/acceptance/test-continuous-120s.mp4`. Confirm both prepared queues cover the same source interval, queue and working-set trends stabilize, A/V drift is not perceptible, and no false generation transition occurs.

- [x] **Step 6: Verify strict failure and manual behavior**

  Confirm any prepared-stream capacity overflow fails before DAG startup, a missing binding fails compilation, and manual H.264 fmtp logs no probe/prepared path. Do not widen queues or clock liveness thresholds and do not add fallback.

- [x] **Step 6a: Remove permanent prepared capture**

  Bind both sockets before capture, stop preflight capture at runtime
  activation, drain the finite prepared queue, then receive directly from the
  same transport using the planner-provided read timeout.

- [x] **Step 7: Commit and cross-review**

  Commit the focused ownership fix, push the current branch, and assign two fresh subagents to review planner authority, RAII, A/V start alignment, error visibility, and absence of hidden-error strategies. Resolve every blocking finding before final acceptance.
