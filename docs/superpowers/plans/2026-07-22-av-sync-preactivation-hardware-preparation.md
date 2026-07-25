# A/V Sync Pre-Activation Hardware Preparation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Prepare the real selected hardware video filter chain before the initial playback epoch activates, then complete continuous A/V output and reach the VLC human-test gate.

**Architecture:** A group-scoped RAII preparation state coordinates the existing single media path. The epoch binder publishes the same immutable initial transaction to the preparation extractor and activation sequencer; the extractor feeds an exact video prefix through the existing decode/filter path, the filter retains its first output and marks readiness, and the sequencer activates once before the extractor releases the unconsumed video suffix and complete audio batch. No second decode/filter/encode path, fixed delay, software fallback, or Task 13 reacquisition is introduced.

**Tech Stack:** C++20, FFmpeg 8.x APIs, existing MediaGraph runtime/channel abstractions, CMake Visual Studio 2026 generator, deterministic unit/runtime tests, FFmpeg/VLC acceptance tools.

## Global Constraints

- The planner is the only policy owner; missing filter identity, timing, SAR, color, format, hardware, or capacity fields fail explicitly.
- Hardware is selected by default using the highest planner score; software is used only when the request explicitly disables hardware.
- Preparation reuses the only production media path and preserves generation, canonical timing, key-frame lineage, and packet identity exactly once.
- Task 13 automatic cross-generation reacquisition remains out of scope; post-activation discontinuity still fails closed.
- Every compilation uses the VS2026 build directory, `--clean-first`, `/nodeReuse:false`, and no `/showIncludes`.
- Source and documentation files end as UTF-8 with CRLF.
- Do not stage `.codex/`, `out/`, copied third-party headers, or unrelated recovered files.

## Execution Priority

Execute Tasks 1-3, then immediately execute Task 5 through the external FFmpeg
and VLC gates. Task 4 and the cleanup/PR tail of Task 5 are post-human-test
work and must not delay showing a verified picture.

---

### Task 1: Group-Scoped Preparation State and Bootstrap

**Files:**
- Create: `src/internal/graph/sync/startup/MediaAvStartupVideoPreparationState.h`
- Create: `src/internal/graph/sync/startup/MediaAvStartupVideoPreparationState.cpp`
- Create: `src/internal/graph/sync/startup/MediaAvStartupVideoPreparationCapability.h`
- Modify: `src/internal/graph/runtime/factory/MediaAvSyncRuntimeBinding.h`
- Modify: `src/internal/graph/runtime/factory/MediaRuntimeNodeFactory.cpp`
- Modify: `src/internal/graph/runtime/compilation/MediaGraphRuntimeCompiler.cpp`
- Modify: `CMakeLists.txt`
- Test: `tests/unit/av_sync_runtime_bootstrap_tests.cpp`

**Interfaces:**
- Produces: `MediaAvStartupVideoPreparationState::begin`, `claimNextVideoUnit`, `markFilterReady`, `markActivated`, `cancel`, and immutable `snapshot`.
- Produces: move-only `MediaAvStartupVideoPreparationCapability` handles restricted to extractor feed, filter readiness, and sequencer activation roles.
- Consumes: existing `MediaAvSyncGroupKey`, playback generation, and immutable initial release identity.

- [ ] **Step 1: Write the failing state-machine and bootstrap tests**

Add tests that create one group binding and assert all three node roles receive the same state identity; assert the only valid transition sequence is:

```cpp
AwaitingInitialRelease -> FeedingVideo -> FilterReady -> Activated
```

Also assert duplicate begin, cross-generation readiness, activation before readiness, and use after cancel return structured failures.

- [ ] **Step 2: Run RED with a clean-first build**

Run:

```powershell
$env:CL='/MP8'
& 'D:\VisualStudio2026\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build out/build/x64-debug-vs2026 --config Debug --clean-first --target media_transcode_av_sync_runtime_bootstrap_tests media_transcode_runtime_tests media_transcode_node_tests media_transcode_realtime_video_cli -- /m:8 /nodeReuse:false
& 'out\build\x64-debug-vs2026\Debug\media_transcode_av_sync_runtime_bootstrap_tests.exe'
```

Expected: build or tests fail because the preparation state and shared binding do not exist.

- [ ] **Step 3: Implement the strict RAII state and role capabilities**

Use one mutex-protected state per sync group. Store only explicit group key, generation, release identity, next video index, phase, and terminal error. Every mutating method validates phase and generation before changing state. `cancel` clears retained identity and makes later calls fail.

- [ ] **Step 4: Inject one shared state through runtime binding**

Create/register the state during A/V runtime bootstrap and pass restricted capabilities through `MediaRuntimeNodeFactory`; do not use globals, node lookups, or planner decisions in the state.

- [ ] **Step 5: Run GREEN**

Repeat the exact clean-first build, then run bootstrap, runtime, and node executables. Expected: all exit 0.

### Task 2: Initial Video Prefix Without Duplicate Release

**Files:**
- Modify: `src/internal/graph/runtime/buffer/MediaStartupReleaseTransactionBuffer.h`
- Modify: `src/internal/graph/runtime/buffer/MediaStartupReleaseTransactionBuffer.cpp`
- Modify: `src/internal/graph/nodes/sync/MediaPlaybackEpochBinderNode.cpp`
- Modify: `src/internal/graph/nodes/sync/MediaAvBoundReleaseExtractorNode.h`
- Modify: `src/internal/graph/nodes/sync/MediaAvBoundReleaseExtractorNode.cpp`
- Modify: `src/internal/graph/builder/segments/MediaRealtimeAvSyncInputSegmentBuilder.cpp`
- Modify: `src/internal/graph/model/MediaRealtimeEdgePolicySet.h`
- Test: `tests/unit/av_sync_production_release_tests.cpp`
- Test: `tests/unit/test_realtime_rtp_graph.cpp`

**Interfaces:**
- Consumes: extractor-feed capability from Task 1.
- Produces: stable initial preparation identity/generation and one indexed video prefix on the existing extractor `video` output.
- Preserves: existing `bound_release` path for the activation commit and active-generation pass-through.

- [ ] **Step 1: Write failing release-ownership tests**

Add `testPreparationPrefixIsNotReleasedTwiceAfterActivation`, `testAudioIsNotReleasedBeforeVideoFilterReady`, and `testPreparationRejectsMismatchedGeneration`. Assert preparation publishes video only, bound commit publishes the unconsumed video suffix plus all audio, and every original video sequence appears exactly once.

- [ ] **Step 2: Write the failing topology assertion**

Assert the production graph has one extractor video output entering the existing packet/decode chain, no second Decode/Filter/Encode nodes, and separate typed control edges named `preparation` and `bound_release`.

- [ ] **Step 3: Run RED using the Task 1 clean-first command**

Expected: release/topology tests fail because preparation input/output ports and indexed ownership do not exist.

- [ ] **Step 4: Implement immutable identity and two-phase extractor ownership**

The binder atomically publishes the same immutable initial transaction reference to preparation and activation control outputs. The extractor preparation input calls `begin`, claims video indices monotonically, and feeds only the existing `video` edge. The bound input verifies the identical release identity, skips the claimed prefix, and atomically commits the remaining video plus the complete audio batch. Control and active-pass-through transactions retain current single-path behavior.

- [ ] **Step 5: Run GREEN**

Repeat the clean-first build and run production release, realtime graph, runtime, and node tests. Expected: all exit 0 and sequence uniqueness assertions pass.

### Task 3: Filter Readiness and Activation-Once Commit

**Files:**
- Modify: `src/internal/graph/nodes/video/VideoFilterNode.h`
- Modify: `src/internal/graph/nodes/video/VideoFilterNode.cpp`
- Modify: `src/internal/graph/nodes/sync/MediaActivatedStartupReleaseSequencerNode.h`
- Modify: `src/internal/graph/nodes/sync/MediaActivatedStartupReleaseSequencerNode.cpp`
- Test: `tests/unit/event_runtime_ffmpeg_ownership_tests.cpp`
- Test: `tests/unit/av_sync_production_release_tests.cpp`

**Interfaces:**
- Consumes: filter-readiness and sequencer-activation capabilities from Task 1.
- Produces: readiness only after one real filtered frame with matching generation is retained.
- Guarantees: `activateInitial` occurs once before the retained filtered frame, unconsumed video suffix, or audio batch can publish downstream.

- [ ] **Step 1: Write failing filter-retention tests**

Add `testVideoFilterRetainsFirstPreparedFrameUntilEpochActivation`, `testVideoFilterRejectsPreparationGenerationMismatch`, and `testVideoFilterPreparationStateClearsOnStopAndAbort`. Assert graph initialization may finish while the group remains inactive, output remains blocked, and activation releases the identical retained frame once.

- [ ] **Step 2: Write the failing sequencer test**

Add `testInitialActivationWaitsForVideoFilterPreparation`. Feed an initial transaction, verify `activateInitial` is not called, mark filter ready, process again, and verify one activation followed by one bound release. WouldBlock retry must not repeat either operation.

- [ ] **Step 3: Run RED with clean-first**

Expected: tests fail because Filter publishes immediately and sequencer activates before readiness.

- [ ] **Step 4: Implement readiness and held-frame lifecycle**

After `VideoFilterGraphBuilder::build` and first successful sink output, validate canonical generation, retain that `MediaBufferRef`, and call `markFilterReady`. Until the state is Activated, return a wakeable waiting result without publishing the frame. Stop/abort clears the held frame through RAII and cancels preparation.

- [ ] **Step 5: Move initial activation behind readiness**

The sequencer retains the initial transaction while phase is FeedingVideo, returns waiting without polling/spinning, and only calls `activateInitial` in FilterReady. After successful activation it marks Activated and publishes activated event plus bound release through the existing atomic preflight/commit path.

- [ ] **Step 6: Run GREEN**

Repeat the clean-first build and run ownership, production release, runtime, and node tests. Expected: all exit 0 with activation/readiness ordering assertions.

### Task 4: Planner-Owned Filter Preparation Contract

**Files:**
- Modify: `src/internal/graph/builder/video/VideoFilterGraphBuilder.h`
- Modify: `src/internal/graph/builder/video/VideoFilterGraphBuilder.cpp`
- Modify: `src/internal/graph/nodes/video/VideoFilterNode.cpp`
- Modify: `src/internal/graph/builder/segments/MediaVideoBranchOptionsMapper.cpp`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.cpp`
- Test: `tests/unit/test_planner.cpp`
- Test: `tests/unit/event_runtime_ffmpeg_ownership_tests.cpp`

**Interfaces:**
- Produces: complete `VideoFilterInputDescriptor` with filter identity, time bases, frame rates, dimensions, formats, SAR, color metadata, backend, and planned capacities.
- Consumes: the real first frame only for `hw_frames_ctx` and strict equality validation against the descriptor.

- [ ] **Step 1: Write failing no-default tests**

Assert missing filter identity, input FPS, output FPS, SAR, color range, pixel format, dimensions, or hardware backend fails planning/building. Assert hardware-auto selects the highest-scored CUDA/NVENC chain and explicit hardware disable selects software.

- [ ] **Step 2: Run RED with clean-first**

Expected: tests expose the current multi-key filter-name fallback, passthrough default, encoder-FPS fallback, or SAR `1/1` default.

- [ ] **Step 3: Replace defaults with one descriptor contract**

Remove the production fallback lookup chain and defaults. Map one planner-owned descriptor into Filter. On the real first frame, require exact format/dimension/color/timing agreement and a non-null compatible `hw_frames_ctx` for hardware plans.

- [ ] **Step 4: Run GREEN**

Repeat clean-first and run planner, ownership, runtime, and node tests. Expected: all exit 0; missing-field cases fail at the asserted boundary.

### Task 5: Production Closed-Loop and Human-Test Gate

**Files:**
- Modify: `tests/unit/av_sync_production_runtime_integration_tests.cpp`
- Modify: `tests/unit/fixtures/AvSyncProductionRuntimeIntegrationSupport.cpp`
- Modify: `docs/superpowers/plans/2026-07-22-av-sync-preactivation-hardware-preparation.md`
- Modify: `plan.md`

**Interfaces:**
- Consumes: the complete preparation/activation path from Tasks 1-4.
- Produces: fixed-hash CLI evidence for sender, ingress, readiness-before-activation, drift/resample, scheduler, RTP egress, external decode, and VLC launch.

- [ ] **Step 1: Add deterministic production ordering coverage**

Assert `filter-ready < activateInitial < first-audio-release` and the initial video sequence occurs once. Assert AudioDriftController emits a valid correction window that reaches `swr_set_compensation`, and scheduler video/audio heads continue across the observation window.

- [ ] **Step 2: Run the strongest clean-first build**

Build at least production runtime integration tests, production release tests, bootstrap tests, runtime tests, node tests, planner tests, and realtime CLI in one clean-first command. Run each executable and require exit 0.

- [ ] **Step 3: Run external FFmpeg gate with bounded resources**

Keep one parent PowerShell process alive. Start sender with identical non-empty CNAME, start fixed-hash CLI with `hw=auto` and explicit `--fps 30`, then open generated SDP with external FFmpeg. Require sender frame/size growth, video and audio ingress, `cuda-nvenc` selection, filter readiness before activation, growing encoded push/pop counters, bounded queues/memory, and decoded frames for both streams.

- [ ] **Step 4: Open VLC only after external decode passes**

While sender and CLI parent lifetime remains active, launch:

```powershell
& 'D:\VideoLAN\VLC\vlc.exe' --no-one-instance --network-caching=200 'D:\Code\MyCode\MediaTranscode\out\<verified-output>.sdp'
```

Keep the pipeline alive for user observation. Report PIDs and do not claim visual quality on the user's behalf.

- [ ] **Step 5: Remove temporary diagnostics and audit the worktree**

Retain only low-volume production diagnostics that establish lifecycle state. Remove ad-hoc first-packet traces and generated evidence from source control. Normalize task files to UTF-8 CRLF, run `git diff --check`, review every modified/untracked file, and stage only closed-loop implementation, tests, concise docs, and intentional deletions.

- [ ] **Step 6: Commit, push, PR, and independent review**

Commit the reviewed coherent change set on `codex/quality-priority-improvements`, push it, create/update the PR, and assign a fresh Agent to review the PR and update `QUALITY_SCORE.md`. Address blocking findings before declaring completion.
