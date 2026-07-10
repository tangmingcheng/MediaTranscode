# Realtime Event-Driven Runtime Plan

## Goal

Eliminate realtime audio/video DAG busy waiting without fixed sleeps or node-specific polling, while preserving planner-owned topology, capacity, overflow, and codec decisions.

## Confirmed Root Causes

- Per-node workers retried immediately when nodes had no input, and SPSC blocking operations used yield loops.
- Concurrent runtime nodes originally shared mutable input `AVFormatContext` / `AVStream` state after probing.

## Tasks

- [x] Make every runtime node return `Progress`, `Waiting`, or `Finished` directly; keep errors as `Result` failures.
- [x] Add per-node sequence-based wakeups with targeted channel `push`, `close`, `abort`, and `clear` notification.
- [x] Make workers continue on progress, block without lost wakeups on waiting, and exit on finished.
- [x] Replace SPSC blocking spin loops with condition-variable waits and deterministic lifecycle notifications.
- [x] Add multi-input terminal tracking and exact mux config/terminal completion state.
- [x] Remove realtime idle sleep/spin planner parameters.
- [x] Construct all workers before starting any thread, then start them in a second stage.
- [x] Publish immutable per-stream FFmpeg snapshots after input probing, including deep-copied codec parameters, format, time, index, and stream kind.
- [x] Make Demux the exclusive owner and mutable consumer of the input format context; downstream metadata and packet nodes consume snapshots only.
- [x] Run event-runtime tests, realtime graph tests, CTest, hardware A/V smoke, and software A/V comparison smoke.
- [x] Update the project-wide `QUALITY_SCORE.md` from an industrial DAG-engine review.
- [x] Complete final implementation review closure.
- [x] Commit, push, and create PR #20.
- [ ] Complete independent review of the final PR head.
- [x] Perform 60-second VLC subjective playback validation with human observation.

## Acceptance Evidence

- Event-runtime and realtime graph test executables pass; CTest passes 2/2.
- Hardware CUDA/NVENC 60-second smoke after the final ownership fixes: 0.40% whole-machine CPU, 0.875 CPU seconds, 1663 frames, `drop_frames=0`, `workerErrors=0`.
- Software 60-second smoke: 16.8% whole-machine CPU and 36.953 CPU seconds; scheduler idle spinning is absent and codec cost remains measurable.
- `git diff --check` passes; modified text files remain UTF-8 with CRLF line endings.
- VLC subjective playback completed: the observer reported smooth video, normal audio, and no perceptible A/V drift.
