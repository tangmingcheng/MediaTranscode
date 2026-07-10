# QUALITY_SCORE Priority Improvements Plan

## Goal

Implement the five `QUALITY_SCORE.md` priority improvements in order, with each phase independently reviewed and gated by deterministic tests plus local, separate-RTP, and MPEG-TS audio/video validation.

## Tasks

- [x] Priority 1: Add deterministic concurrency stress, fault injection, lifecycle coverage, fairness tests, and acceptance metrics collection.
- [ ] Priority 2: Split oversized planner, mux, capability scanner, and runtime responsibilities.
- [ ] Priority 3: Separate core, planner, builder, runtime, node, integration, hardware, and performance test tiers.
- [ ] Priority 4: Reject or explicitly mark incomplete optimizer, GPU, and distributed execution capabilities.
- [ ] Priority 5: Establish repeatable performance and stability baselines and update `QUALITY_SCORE.md` from evidence.
- [ ] Run final local, separate-RTP, and MPEG-TS hardware/software acceptance, including stability and VLC review.
- [ ] Complete whole-branch review, push, create PR, and obtain independent PASS review.

## Priority 1 Evidence

- Event-runtime tests passed 20 consecutive runs with zero failures.
- Realtime graph regression passed.
- Runtime lifecycle state matrix, worker failure harvesting, deterministic queue waiter release, scripted fault sequences, and concurrent multi-input fairness are covered.
- Windows process/system CPU, working set, thread count, sampled queue high-water mark, progress stalls, and errors are exposed through a thread-safe acceptance collector and runtime report.
- Independent task review verdict: PASS.

## Global Constraints

- Planner remains the only decision owner; runtime nodes do not add defaults or fallback behavior.
- Local and realtime CLIs remain separate.
- Modified text files use UTF-8 and CRLF.
- Existing unrelated working-tree files are excluded from all commits.
