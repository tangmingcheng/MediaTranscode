# RKMPP Zero-Copy Implementation Plan

> **For agentic workers:** implement task-by-task with scoped review after every task. Production verification uses real CLI/media chains; do not add tracked automated tests.

**Goal:** Deliver strict RKMPP/DRM PRIME zero-copy decode, optional RGA resize, and encode for local and realtime DAGs without regressing Windows.

**Architecture:** Extend the existing FFmpeg planner and generic codec nodes with typed backend and frame contracts. The planner selects a complete RKMPP chain; builders map it; runtime validates DRM PRIME facts and never falls back.

**Tech Stack:** C++20, FFmpeg libavcodec/libavfilter/libavutil, CMake, Visual Studio 2026, Linux aarch64 RKMPP/RGA.

## Global Constraints

- Work only on `codex/rkmpp-zero-copy`; commit and push every completed task to that branch.
- Preserve current Windows automatic planning and real-media behavior.
- Planner is the only decision authority. Missing facts fail; downstream nodes do not default or fallback.
- Use RAII and focused files; do not duplicate lifecycle, format-selection, or codec logic.
- Do not add or restore CI, CTest, unit, integration, acceptance, hardware, or performance tests.
- Keep UTF-8 and CRLF for tracked text files.
- Do not touch the original checkout's untracked FFmpeg headers or `out/`.

### Task 1: Target capability baseline and typed backend request

- Capture target SoC, kernel, DRM/MPP devices, FFmpeg configuration, RKMPP codecs, DRM PRIME format, and RGA filter facts after `ffenv on`.
- Add `--hardware-backend auto|rkmpp`, reject its conflict with `--disable-hw`, and carry a typed required backend through local and realtime planner requests.
- Strict RKMPP selection must reject unavailable/incomplete RKMPP candidates instead of selecting software.
- Verify by Windows compilation of affected translation units and target-side planner diagnostics; commit and push.

### Task 2: RKMPP capability and frame contracts

- Represent decoder output, optional filter input/output, and encoder input with existing hardware descriptors rather than RKMPP string inference.
- Remove fake `passthrough_rkmpp`; omit the filter for source-size RKMPP and use the target's real RGA syntax only for resize.
- Make capability preflight distinguish internally managed RKMPP codecs from device/frames-context backends and validate the complete planned chain.
- Verify source-size and resize planning on the target; commit, push, and review.

### Task 3: DRM PRIME runtime enforcement and diagnostics

- Validate `AV_PIX_FMT_DRM_PRIME`, a valid DRM frame descriptor, backing `AVBufferRef`, planned dimensions, and stage format at decode/filter/encode boundaries.
- Preserve references for source-size zero-copy; allow only RGA-produced DRM PRIME replacement for resize. Reject software frames and all upload/download fallback.
- Add counters and buffer-identity diagnostics without duplicating codec lifecycle, flush, abort, generation, or lineage logic.
- Verify with focused target CLI probes; commit, push, and review.

### Task 4: Cross-platform build and real-media acceptance

- Run the clean VS2026 Debug rebuild and existing Windows local/realtime A/V chains with the fixed 120-second source.
- Sync the committed branch archive to the verified target path and build after `ffenv on`.
- Run RK local four-codec matrix, two RGA resize cases, realtime four-codec matrix, and one H.264 A/V route. Record exact commands, outputs, CPU/RSS, queue/drop state, zero-copy counters, A/V drift, and exit reasons.
- Stop and preserve evidence if the target FFmpeg lacks a required capability; do not weaken the contract.

### Task 5: Documentation, scoring, delivery, and review

- Record concise completed-work evidence, commands/results, remaining risks, and update `QUALITY_SCORE.md`.
- Confirm no temporary tests, credentials, process residue, LF-only changed text, or unrelated files remain.
- Run whole-branch review, fix all load-bearing findings, push, create the PR, and obtain a fresh-agent PASS verdict.
