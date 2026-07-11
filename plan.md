# QUALITY_SCORE Priority Improvements Plan

## Goal

Implement the five `QUALITY_SCORE.md` priority improvements in order, with each phase independently reviewed and gated by deterministic tests plus local, separate-RTP, and MPEG-TS audio/video validation.

## Tasks

- [x] Priority 1: Add deterministic concurrency stress, fault injection, lifecycle coverage, fairness tests, and acceptance metrics collection.
- [x] Priority 2: Split oversized planner, mux, capability scanner, and runtime responsibilities.
- [x] Priority 3: Separate core, planner, builder, runtime, node, integration, hardware, and performance test tiers.
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

## Priority 2 Evidence

- Capability scanning is separated into input, video, audio, and hardware probes behind the scanner facade.
- Realtime planning is separated into request validation, request classification, input planning, and output policy while the planner remains the decision owner.
- RTP muxing is separated into FFmpeg session ownership, protocol I/O, and state-machine components.
- Runtime compilation/registration and lifecycle execution are separated behind the runtime facade.
- A clean rebuild completed before verification; all CTest targets passed and the event-runtime target passed 20 consecutive runs.
- Local MP4, separate RTP, and MPEG-TS UDP H264/AAC hardware/software 60-second gates passed.
- Audio and video were both transcoded by explicit target parameters; no test-only transcode switch was added.
- Independent review findings were addressed: audio RTP pacing has no bitrate fallback, RTP mux state transitions are constrained, capability stream inspection moved out of the facade, and audio decode waits for codec metadata.
- Fourth independent Priority 2 review verdict: PASS.

## Priority 3 Evidence

- Core, planner, builder, runtime, and node tests are independent deterministic targets and labels.
- Integration, hardware, and performance targets require explicit CMake options and have dedicated presets and CI jobs.
- Unsupported hardware exits with code 77 and is reported by CTest as skipped; CUDA device creation passed on the validation host.
- A clean rebuild completed before the final 9/9 CTest pass.
- Local MP4, separate RTP, and MPEG-TS UDP H264/AAC hardware/software 60-second gates passed; audio transcode was naturally selected from the 44.1 kHz target difference.
- A discovered audio-encoder startup race is covered: frames remain queued until required codec metadata is bound.

## Global Constraints

- Planner remains the only decision owner; runtime nodes do not add defaults or fallback behavior.
- Local and realtime CLIs remain separate.
- Modified text files use UTF-8 and CRLF.
- Existing unrelated working-tree files are excluded from all commits.
