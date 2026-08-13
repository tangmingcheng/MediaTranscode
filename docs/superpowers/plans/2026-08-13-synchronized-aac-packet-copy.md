# Synchronized AAC Packet Copy Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make synchronized realtime AAC packet copy a planner-selected shared Windows/Linux product whenever complete input and output contracts match, while retaining strict frame transcode for real differences and lowering RKMPP realtime CPU without weakening the 2K acceptance chain.

**Architecture:** The shared audio planner remains the only branch authority. It produces a typed synchronized copy or transcode runtime product; builders map that product without fallback, and Project MPEG-TS derives its AAC ADTS sampling index from resolved output facts. Platform-specific UDP and RKMPP codec/filter code are not involved in the audio decision.

**Tech Stack:** C++20, FFmpeg libavcodec/libavutil, existing MediaTranscode DAG/planner/builders, CMake/Ninja, VS2026, RKMPP/RGA target FFmpeg under `ffenv on`.

## Global Constraints

- Work only on `codex/rkmpp-zero-copy`; commit and push each completed production task to this branch.
- Windows and Linux/RKMPP share planner, builder, scheduler, AAC framing, and MPEG-TS code. Separate code is allowed only for a proven operating-system API, driver, hardware backend, or platform-library difference.
- The planner owns every branch and contract decision. Runtime and builders fail on missing or conflicting products and never fall back.
- Temporary TDD sources/targets may exist only for red/green proof. Delete them and their build artifacts before each production commit; never add repository test infrastructure.
- Preserve untracked FFmpeg headers and `out/`; do not stage or delete them.
- Keep edited text UTF-8 without BOM and CRLF.
- Do not reduce source resolution, bitrate, frame rate, duration, codec conversion, scaling, or acceptance criteria. Diagnostic reductions cannot replace final acceptance.
- Windows testing uses direct absolute-path commands and no test scripts. RKMPP may use a temporary target-side script invoked over SSH; delete it and verify PID/port residue afterward.
- Final RKMPP build runs only after `ffenv on` and may use eight parallel jobs.

---

### Task 1: Contract-Driven Realtime Audio Branch Selection

**Files:**
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.cpp`
- Modify only if a missing fact is proven: `src/internal/graph/planner/realtime/MediaRealtimeRtpCodecDescriptor.cpp`
- Temporary test: `out/tdd/synchronized-aac-selection-tdd.cpp` (delete before commit)

**Interfaces:**
- Consumes: `MediaInputAudioStreamInfo`, `MediaAudioPipelinePlannerOptions`, `MediaAudioOutputRequirement`, and raw-RTP AAC signaling facts.
- Produces: existing `MediaAudioPipelinePlan` with `branchMode`, `resolvedOutput`, `maximumAccessUnitSamples`, and a truthful `reason`; no new CLI bool.

- [ ] **Step 1: Write the temporary RED probe**

Create a standalone probe that invokes production planning for three complete AAC-LC stereo inputs:

```cpp
assert(planAac(44100, 2, 317000, std::nullopt, std::nullopt).branchMode ==
       MediaBranchMode::CopyPacket);
assert(planAac(44100, 2, 317000, 48000, std::nullopt).branchMode ==
       MediaBranchMode::TranscodeFrame);
assert(planAac(44100, 2, 317000, std::nullopt, 275).branchMode ==
       MediaBranchMode::TranscodeFrame);
```

The helper must construct real `MediaInputAudioStreamInfo` and `MediaAudioPipelinePlannerOptions`; it must not mock the planner.

- [ ] **Step 2: Compile and run RED**

Compile through the VS2026 developer environment against the current production sources. Expected: the matching request fails because realtime planning injects `requireFrameTranscode=true`; the two differing requests remain transcode.

- [ ] **Step 3: Remove the unconditional synchronization override**

Delete the assignment that derives `requireFrameTranscode` from `AudioVideo`. Populate output requirements only from actual selected-output protocol facts. Preserve explicit requested bitrate/sample-rate/channel/profile/rate-control semantics in `MediaResolvedAudioTargetDecision`.

The resulting decision remains:

```cpp
copy = codecMatches && profileMatches && sampleRateMatches &&
       channelsMatch && bitrateMatches && !encoderOnlyRequest &&
       !outputRequirement.requireFrameTranscode;
```

- [ ] **Step 4: Run GREEN and negative cases**

Expected: exact facts select `CopyPacket`; explicit 48 kHz and explicit differing bitrate select `TranscodeFrame`; incomplete AAC profile/access-unit facts fail rather than copy.

- [ ] **Step 5: Delete the temporary probe and review the diff**

Remove `out/tdd/synchronized-aac-selection-tdd.cpp` and its binary/object files. Confirm no test/CMake target is tracked and no platform conditional was added.

- [ ] **Step 6: Commit and push**

Stage only Task 1 production files, commit `fix: select realtime audio branch from contracts`, and push `codex/rkmpp-zero-copy`.

### Task 2: Typed Synchronized Copy and Transcode Runtime Products

**Files:**
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeAvSyncPlanningFacts.h`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeAvSyncComponentBoundsPlanner.cpp`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeAvSyncComponentBoundsPlanner.h`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlanner.cpp`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlanValidator.cpp`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeAvSyncPlanningFactsResolver.cpp`
- Modify: `src/internal/graph/planner/realtime/MediaAudioCorrectionReachabilityPlanner.cpp`
- Temporary test: `out/tdd/synchronized-aac-runtime-product-tdd.cpp` (delete before commit)

**Interfaces:**
- Produces:

```cpp
struct MediaSynchronizedAudioPacketCopyBounds {
    std::int64_t accessUnitSamples;
    std::int64_t schedulerQueueSamples;
};

struct MediaSynchronizedAudioFrameTranscodeBounds {
    std::int64_t decoderDelaySamples;
    std::int64_t decodeQueueSamples;
    std::int64_t resampleQueueSamples;
    std::int64_t encodeQueueSamples;
    std::int64_t schedulerQueueSamples;
    std::int64_t mailboxDeliveryMarginSamples;
    std::int64_t maximumResamplerOutputBlockSamples;
    std::size_t mailboxCapacity;
};

using MediaRealtimeAvSyncComponentBounds = std::variant<
    MediaSynchronizedAudioPacketCopyBounds,
    MediaSynchronizedAudioFrameTranscodeBounds>;
```

- Consumes: `MediaAudioPipelinePlan::branchMode`, `maximumAccessUnitSamples`, selected decoder/resampler, resolved encoder frame samples, and queue capacities.

- [ ] **Step 1: Write the runtime-product RED probe**

Assert that a copy plan with `maximumAccessUnitSamples=1024` returns the copy alternative and that a transcode plan returns the transcode alternative. Assert copy rejects selected decoder/resampler/correction facts, while transcode rejects missing decoder/resampler.

- [ ] **Step 2: Compile and run RED**

Expected: copy planning fails with `synchronized audio components did not publish timing bounds` because only transcode bounds exist.

- [ ] **Step 3: Implement the variant and branch-specific planner**

For `CopyPacket`, require a positive access-unit sample count and compute only checked scheduler capacity. For `TranscodeFrame`, keep all current decoder/resampler/encoder/mailbox arithmetic. Reject `Drop` in an AudioVideo runtime.

- [ ] **Step 4: Make correction reachability transcode-only**

Change the runtime plan to hold correction reachability only in the transcode product (or as an optional that validators require exclusively for the transcode alternative). Planning facts for copy publish source/output rate, access-unit size, and scheduler capacity, but leave decoder/resampler/encoder/mailbox facts absent. Validators reject mixed alternatives and zero-value substitutes.

- [ ] **Step 5: Run GREEN and validator cases**

Expected: copy and transcode alternatives pass only with their own facts; mixed products, missing access-unit samples, and overflow fail before graph construction.

- [ ] **Step 6: Delete temporary TDD artifacts, review, commit, and push**

Commit `feat: model synchronized audio copy runtime` and push the branch.

### Task 3: Map the Planner Product into the Shared Synchronized DAG

**Files:**
- Modify: `src/internal/graph/builder/realtime/MediaRealtimeRtpTranscodeGraphBuilder.cpp`
- Modify: `src/internal/graph/builder/segments/MediaAudioBranchSegmentBuilder.cpp`
- Modify: `src/internal/graph/builder/segments/MediaAudioBranchSegmentBuilder.h`
- Modify: `src/internal/graph/builder/segments/MediaAudioBranchOptionsMapper.cpp`
- Inspect without duplicating: `src/internal/graph/builder/segments/MediaAudioPacketCopyBranchBuilder.cpp`
- Inspect without modifying unless a real invariant is missing: `src/internal/graph/nodes/sync/MediaAvScheduledOutputBuilder.cpp`
- Temporary test: `out/tdd/synchronized-aac-topology-tdd.cpp` (delete before commit)

**Interfaces:**
- Copy mapping:

```cpp
correctionMode = MediaAudioCorrectionExecutionMode::Disabled;
lineageMode = MediaAudioLineageExecutionMode::SynchronizedReleasedAudio;
```

- Transcode mapping retains `ExternalCorrectionRequired`, synchronized released lineage, correction generation/lookahead, group key, and frame capacity.

- [ ] **Step 1: Write the topology RED probe**

Build the production graph from a copy runtime product and assert it contains the shared RTP input/clock/startup/scheduler/MPEG-TS nodes but contains no `AudioDecode`, `AudioResample`, `AudioEncode`, or `AudioDriftController`. Build a transcode product and assert those four nodes remain.

- [ ] **Step 2: Run RED**

Expected: builder fails with `synchronized audio packet copy is unsupported`.

- [ ] **Step 3: Make branch options planner-derived**

In `MediaRealtimeRtpTranscodeGraphBuilder`, visit the typed component-bound product. Populate copy options with disabled correction and synchronized lineage; populate transcode options with the existing correction product. Do not infer from codec names or platform.

- [ ] **Step 4: Permit synchronized released lineage for copy**

Update `MediaAudioBranchSegmentBuilder` validation so copy accepts exactly `Disabled + SynchronizedReleasedAudio` with no correction mailbox/generation/lookahead/group facts. Continue rejecting any mixed copy/correction state. Reuse `MediaAudioPacketCopyBranchBuilder`; do not create a second copy implementation.

- [ ] **Step 5: Run GREEN and graph validation**

Expected: both graph shapes validate, and the copy graph has no audio codec/correction nodes.

- [ ] **Step 6: Delete temporary artifacts, review, commit, and push**

Commit `feat: build synchronized AAC packet copy branch` and push.

### Task 4: Derive Project MPEG-TS AAC Facts from Resolved Output

**Files:**
- Modify: `src/internal/graph/planner/realtime/MediaProjectMpegTsOutputPlan.cpp`
- Create: `src/internal/graph/protocol/codec/MediaAacSamplingFrequency.cpp`
- Create: `src/internal/graph/protocol/codec/MediaAacSamplingFrequency.h`
- Modify: `src/internal/graph/protocol/codec/MediaAacAudioSpecificConfigParser.cpp`
- Modify: `CMakeLists.txt` to add the shared mapping source/header to the existing core source list.
- Temporary test: `out/tdd/project-mpegts-aac-rate-tdd.cpp` (delete before commit)

**Interfaces:**
- Produces `MediaTsAacAdtsPlan{0, MediaAacLcAudioObjectType, samplingFrequencyIndex, channels}` from `MediaResolvedAudioOutputPlan`.
- Supports every sample rate already represented by the shared `MediaAacSampleRates` table; rejects missing table entries.

- [ ] **Step 1: Write RED for 44.1 kHz**

Create resolved AAC-LC stereo copy output at 44,100 Hz and assert `createAudioVideo` succeeds with ADTS sampling index `4`. Also assert 48,000 Hz yields index `3` and an unsupported rate fails.

- [ ] **Step 2: Run RED**

Expected: 44.1 kHz fails the current fixed 48 kHz project contract.

- [ ] **Step 3: Replace fixed sample-rate/index facts**

Use one shared sample-rate-to-index mapping. Validate AAC-LC, supported channels, long-frame access units, and resolved sample rate. Keep PMT stream type `0x0F`, PID/PCR, PSI, and scheduler policies unchanged.

- [ ] **Step 4: Run GREEN and serialize one ADTS header**

Expected: 44.1/48 kHz plans pass with indices 4/3, unsupported rates fail, and the production ADTS framer emits the planned index.

- [ ] **Step 5: Delete temporary artifacts, review, commit, and push**

Commit `feat: derive MPEG-TS AAC rate contract` and push.

### Task 5: Shared Diagnostics and CLI Truthfulness

**Files:**
- Modify: `tools/realtime_video_cli/main.cpp`
- Modify: `src/internal/graph/runtime/diagnostics/MediaGraphRuntimeReport.cpp`
- Modify: `src/internal/graph/runtime/diagnostics/MediaGraphRuntimeReport.h`
- Modify: `src/internal/graph/runtime/threading/MediaGraphThreadedExecutor.cpp`
- Modify: `src/internal/graph/runtime/threading/MediaGraphThreadedExecutor.h`

**Interfaces:**
- Emits planner-selected `audio_plan branch=copy_packet|transcode_frame reason=...` and resolved codec/profile/rate/channels.
- Emits per-node worker process/wait/wakeup/error totals at final report without platform branches.
- Produces `MediaGraphWorkerReport { nodeId, processCalls, progress, waits, wakeups, deadlines, errors }` snapshots from the executor; the reporter joins `nodeId` to the compiled graph name/kind.

- [ ] **Step 1: Prove the current diagnostic gap with a production CLI log**

Use the existing RK log showing aggregate `workerProcessCalls` and `mt-n29` CPU but no node-level metrics. Record this as failure evidence; do not add a persistent automated test.

- [ ] **Step 2: Add minimal shared diagnostics**

Print complete resolved audio facts in the CLI summary. Add one executor snapshot accessor that copies each worker's existing node ID and metrics while the executor owns the workers. Serialize the joined node name/kind and counters only in the final report; do not add high-frequency logging or a second metrics collector.

- [ ] **Step 3: Focused compile and review**

Verify diagnostics build on Windows and contain no hard-coded zero counters or RKMPP-only branches.

- [ ] **Step 4: Commit and push**

Commit `diag: report synchronized audio branch cost` and push.

### Task 6: Final Builds, Real-Media Acceptance, Cleanup, and Review

**Files:**
- Update: `docs/completed/2026-08-13-rkmpp-zero-copy-acceptance.md`
- Update: `QUALITY_SCORE.md`
- Update if plan status is tracked there: `docs/superpowers/plans/2026-08-12-rkmpp-zero-copy.md`

**Interfaces:**
- Acceptance product: shared code on Windows and RKMPP, strict RKMPP DRM PRIME video chain, truthful audio copy/transcode choice, no reduced source facts.

- [ ] **Step 1: Remove every temporary TDD artifact**

Search tracked/untracked files for the four temporary probe names, temporary CMake targets, `test`, `tdd`, and generated objects. Preserve user-owned headers/out; delete only task-owned temporary artifacts.

- [ ] **Step 2: Run the final Windows clean-first build**

Run the documented absolute VS2026 rebuild command with the 120-second hard limit. Require all targets and both CLIs to link. This build must run on final bytes after all fixes.

- [ ] **Step 3: Run Windows real-media acceptance with direct absolute commands**

Use a 2560x1440 continuous 120-second source derived without lowering source parameters. Run separate RTP input with automatic video fmtp, video codec conversion, 2560x1440 to 1280x720 scaling, and MPEG-TS/RTP output. Launch FFmpeg and hidden CLI through direct absolute commands with small intervals; launch only VLC visibly. Record exact commands, PIDs, CPU/RSS trend, A/V drift, output codec/size, selected audio branch, continuity, and exit reason. Stop FFmpeg source and wait for CLI exit.

- [ ] **Step 4: Archive-sync committed source to RKMPP and clean-build under ffenv**

Verify remote `pwd -P` equals `/home/firefly/Downloads/MediaTranscode`, replace only that directory from the committed branch archive, then run `ffenv on` followed by a clean all-target CMake build with `--parallel 8`.

- [ ] **Step 5: Run RKMPP local gates**

Using `/home/firefly/Downloads/test16s.mp4`, validate H264/HEVC decode and encode combinations required by the RK plan and at least H264 2560x1440 to H264 1280x720 RGA scaling. Require DRM PRIME contracts, zero software/download/upload counts, complete ffprobe/full decode, and clean frame output.

- [ ] **Step 6: Run RKMPP realtime human-eye acceptance**

Create a temporary target-side script that starts the strict RKMPP CLI, waits 1.5 seconds, then streams the 2560x1440 120-second source as separate RTP with omitted video fmtp and explicit audio signaling. Output MPEG-TS/RTP to visible Windows VLC. Require video codec change and resolution change in the final matrix where the current Project MPEG-TS codec contract supports it; do not lower source parameters. Record exact script, CLI/source/VLC PIDs, per-node CPU, RSS, queues, A/V drift, RTP/TS continuity, zero-copy counters, branch selection, and exit reason. Stop the source, wait for CLI exit, delete the remote script, and verify no port/process residue.

- [ ] **Step 7: Diagnose any remaining flower-screen evidence at the failing boundary**

If visual corruption remains, compare production local output, raw RTP ingress continuity, encoded H.264 access-unit validity, TS continuity, and VLC receive evidence without weakening the chain. Classify each finding as shared or platform-only before modifying code; return to Task 1-5 with a new temporary RED probe for any code fix.

- [ ] **Step 8: Update concise completion and quality records**

Document exact real-media commands/results, branch choices, CPU/RSS/A/V drift, remaining HEVC MPEG-TS risk, MPP cleanup warnings, and any blocked A/V item. Keep `QUALITY_SCORE.md` concise and evidence-based.

- [ ] **Step 9: Final hygiene, commit, and push**

Verify UTF-8/CRLF, `git diff --check`, no credentials/scripts/tests/processes/ports, and no accidental headers/out staging. Commit final docs and push.

- [ ] **Step 10: Independent review and PR**

Create/update the PR for `codex/rkmpp-zero-copy`. Dispatch a fresh independent reviewer to assess planner authority, typed copy/transcode products, shared Windows/Linux behavior, zero-copy evidence, CPU evidence, RAII, lifecycle, temporary-test removal, and Windows isolation. Fix and revalidate every Critical/Important finding until explicitly approved.
