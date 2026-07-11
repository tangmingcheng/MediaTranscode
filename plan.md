# QUALITY_SCORE Priority Improvements Plan

## Goal

Implement the five `QUALITY_SCORE.md` priority improvements in order, with each phase independently reviewed and gated by deterministic tests plus local, separate-RTP, and MPEG-TS audio/video validation.

## Tasks

- [x] Priority 1: Add deterministic concurrency stress, fault injection, lifecycle coverage, fairness tests, and acceptance metrics collection.
- [x] Priority 2: Split oversized planner, mux, capability scanner, and runtime responsibilities.
- [x] Priority 3: Separate core, planner, builder, runtime, node, integration, hardware, and performance test tiers.
- [x] Priority 4: Reject or explicitly mark incomplete optimizer, GPU, and distributed execution capabilities.
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

## Priority 4 Evidence

- A single capability-maturity matrix marks CUDA/NVENC transcode stable and graph optimization, generic GPU execution, and distributed execution unsupported.
- Planner, optimizer passes, GPU executor, remote executor, mesh planner, and multi-node deployment now reject unsupported requests instead of reporting deferred or simulated success.
- Default policies no longer enable incomplete optimizer, GPU, mesh, or distributed paths.
- AAC-bound audio is explicitly reframed to the encoder frame size after resampling; 44.1 kHz to 48 kHz audio/video transcode passed for local, separate RTP, and MPEG-TS paths.
- Deterministic audio-encoder tests cover send-side EAGAIN retry, output backpressure, partial EOF framing, flush, and single terminal propagation.
- Clean deterministic, integration, hardware, and performance tiers passed. FFmpeg runtime binaries were copied beside each executable without CMake changes.
- Repeatable PowerShell gates record exact commands, full decode, stream integrity, worker/drop/stall errors, average/P95 CLI CPU, and final A/V drift.
- Sixty-second local hardware/software loops and realtime hardware/software gates passed. Final realtime average CPU was 1.58%/6.01% for RTP and 1.68%/5.94% for MPEG-TS (hardware/software respectively); final A/V drift stayed between 7 ms and 31 ms.

## Global Constraints

- Planner remains the only decision owner; runtime nodes do not add defaults or fallback behavior.
- Local and realtime CLIs remain separate.
- Modified text files use UTF-8 and CRLF.
- Existing unrelated working-tree files are excluded from all commits.
