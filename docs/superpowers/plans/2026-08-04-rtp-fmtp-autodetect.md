# RTP Video FMTP Auto-Detection Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Detect strict H.264/HEVC video FMTP from separate raw RTP before graph construction and transfer the same live transport into runtime.

**Architecture:** A planner-owned probe plan drives a bounded raw RTP preparer. Typed signaling facts return to the planner, while a prepared-input buffer retains the transport and original datagrams for exact runtime binding.

**Tech Stack:** C++20, MediaTranscode DAG planner/builder/runtime, Win32 UDP RTP/RTCP transport, FFmpeg/VLC, Visual Studio 2026 CMake/Ninja.

## Global Constraints

- Planner is the only decision owner; runtime receives complete parameters and never guesses or falls back.
- H.264/HEVC video only; AAC FMTP remains manual and Opus keeps its existing no-FMTP contract.
- Do not add or restore tracked tests. Use real CLI media chains and removable local tools under `out/`.
- Preserve canonical A/V synchronization and all existing manual raw RTP behavior.
- UTF-8 and CRLF for every changed text file.

### Task 1: Signaling facts and shared NAL assembly

- [ ] Add typed H.264/HEVC signaling facts and canonical FMTP serialization.
- [ ] Extract shared H.264/HEVC RTP NAL assembly used by both detection and depacketization.
- [ ] Verify strict duplicate, conflict, malformed, and unsupported packetization behavior with an untracked local harness.
- [ ] Commit the focused signaling change.

### Task 2: Raw RTP preflight and prepared input

- [ ] Add the explicit raw RTP probe plan and bounded preparer.
- [ ] Add the `RawRtp` prepared-input buffer holding the RAII transport and current-SSRC datagram queue.
- [ ] Integrate auto mode into preflight and planner FMTP resolution; keep manual mode I/O-free.
- [ ] Commit the focused preflight change.

### Task 3: Runtime handoff and CLI contract

- [ ] Bind raw prepared input to the exact video `RawRtpInputNode`.
- [ ] Consume buffered datagrams before receiving from the same transport.
- [ ] Make video FMTP optional at the CLI while retaining strict AAC validation.
- [ ] Add safe detection diagnostics and commit the focused runtime change.

### Task 4: Build and real-media acceptance

- [ ] Run the mandated clean-first x64 Debug rebuild.
- [ ] Run two-minute H.264 A/V automatic detection and FFmpeg/VLC output validation.
- [ ] Run two-minute HEVC automatic detection and H.264 output validation.
- [ ] Run manual H.264 regression and strict preflight failure gates.
- [ ] Remove all temporary validation tools and confirm no tracked test residue.

### Task 5: Documentation, quality, and delivery

- [ ] Update README, ARCHITECTURE, completion evidence, root plan, and QUALITY_SCORE.
- [ ] Review the full diff for planner authority, RAII, duplication, lifecycle, UTF-8/CRLF, and `git diff --check`.
- [ ] Commit and push all changes, create a ready PR, and record residual risks.
- [ ] Have a fresh agent review the PR; fix and revalidate until it returns PASS.
