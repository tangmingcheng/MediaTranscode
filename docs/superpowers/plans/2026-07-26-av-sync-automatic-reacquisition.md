# A/V Sync Automatic Reacquisition Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Keep the realtime CLI and its configured UDP output alive across a recoverable RTP or MPEG-TS clock discontinuity by purging the old A/V generation and atomically activating the next playback epoch.

**Architecture:** A group-owned `MediaAvReacquisitionCoordinator` is the only production transition writer. The runtime compiler registers the planner-declared purge targets before execution; generation-aware input gates withhold the next generation until purge completes, and the existing startup transaction activates and releases the next epoch atomically.

**Tech Stack:** C++20, FFmpeg, project DAG runtime, CMake/Ninja, GoogleTest, Visual Studio 2026, PowerShell.

## Global Constraints

- The approved design is `docs/superpowers/specs/2026-07-26-av-sync-automatic-reacquisition-design.md`.
- Only planners choose participants, child identities, acknowledgement timeout, and terminal drain window.
- Runtime nodes execute an immutable complete plan; they have no fallback, inferred defaults, or independent transition authority.
- The CLI process and configured UDP sink remain alive during a recoverable transition.
- Old-generation data never crosses the output boundary after the permit closes.
- Next-generation data never crosses the output boundary before atomic `activateNext`.
- Invalid evidence, timeout, purge failure, sequence mismatch, or activation failure remains terminal and preserves the first error.
- Existing enum numeric values remain stable; new values append.
- All ownership is RAII. Runtime control objects use owned or shared lifetime, never borrowed asynchronous pointers.
- Modified text is UTF-8 without BOM and CRLF.
- Every compilation uses `--clean-first`; remove the exact `/showIncludes` token from the selected build directory's generated `CMakeFiles/rules.ninja` before invoking CMake and verify that it remains absent.
- Do not run `media_transcode_av_sync_production_runtime_integration_tests.exe`.
- Do not start an unattended long-duration test. Human-eye testing requires explicit user approval after deterministic verification.
- Stage only task-owned files. Preserve unrelated tracked modifications and untracked files.

## Build Discipline Used by Every Task

Before each build, run the following for the selected build directory. Replace
only `$buildDir` and `$target`; do not omit `--clean-first`.

```powershell
$buildDir = 'out/build/x64-debug'
$target = 'media_transcode_av_generation_transition_tests'
$rules = Join-Path $buildDir 'CMakeFiles/rules.ninja'
$content = [IO.File]::ReadAllText((Resolve-Path $rules))
$content = $content.Replace(' /showIncludes', '')
[IO.File]::WriteAllText(
    (Resolve-Path $rules),
    $content,
    [Text.UTF8Encoding]::new($false))
if (Select-String -Path $rules -SimpleMatch '/showIncludes' -Quiet) {
    throw '/showIncludes remains in rules.ninja'
}
cmake --build $buildDir --clean-first --target $target
if ($LASTEXITCODE) { throw "clean-first build failed: $target" }
```

If CMake regenerates `rules.ninja`, stop and remove the token again before the
next explicit clean-first build. Never automatically retry a failed build.

---

### Task 1: Group-Owned Reacquisition State Machine

**Files:**
- Create: `src/internal/graph/sync/MediaAvReacquisitionCoordinator.h`
- Create: `src/internal/graph/sync/MediaAvReacquisitionCoordinator.cpp`
- Modify: `src/internal/graph/sync/MediaAvSyncGroupRuntime.h`
- Modify: `src/internal/graph/sync/MediaAvSyncGroupRuntime.cpp`
- Modify: `src/internal/graph/sync/MediaAvEpochTransitionService.h`
- Modify: `src/internal/graph/sync/MediaAvEpochTransitionService.cpp`
- Test: `tests/unit/av_generation_transition_tests.cpp`

**Interfaces:**
- Consumes: `MediaAvReacquisitionRequest`, `MediaAvGenerationParticipantGroup`, `MediaAvEpochTransitionService`, and the group master clock.
- Produces:

```cpp
enum class MediaAvReacquisitionPhase : std::uint8_t {
    Inactive = 0,
    Purging = 1,
    Acquiring = 2,
    ReadyForActivation = 3,
    Aborted = 4
};

struct MediaAvReacquisitionSnapshot final {
    MediaAvReacquisitionPhase phase;
    std::optional<MediaAvGenerationPurge> transition;
    std::optional<MediaAvReacquisitionReason> reason;
};

class MediaAvReacquisitionCoordinator final {
public:
    static ::media::Result<std::unique_ptr<MediaAvReacquisitionCoordinator>>
    create(std::shared_ptr<MediaAvEpochTransitionService> transition,
           std::shared_ptr<MediaMasterClock> clock,
           std::vector<MediaAvGenerationParticipantGroup> participants);

    ::media::Status request(MediaAvReacquisitionRequest request);
    ::media::Status pollTimeout();
    MediaAvReacquisitionSnapshot snapshot() const noexcept;
    ::media::Status markReadyForActivation(
        std::uint64_t generation,
        std::uint64_t transitionSequence);
    ::media::Status markActivated(
        std::uint64_t generation,
        std::uint64_t transitionSequence);
    void abort() noexcept;
};
```

`MediaAvSyncGroupRuntime` adds:

```cpp
::media::Status installReacquisitionCoordinator(
    std::unique_ptr<MediaAvReacquisitionCoordinator> coordinator);
MediaAvReacquisitionSnapshot reacquisitionSnapshot() const noexcept;
::media::Status markReacquisitionReadyForActivation(
    std::uint64_t generation, std::uint64_t transitionSequence);
::media::Status markReacquisitionActivated(
    std::uint64_t generation, std::uint64_t transitionSequence);
```

Its existing `requestReacquisition` delegates to the installed coordinator.
Production groups reject a request until the coordinator is installed.

- [ ] **Step 1: Write failing state-machine tests**

Add tests that create two planned participant groups with recording purge
targets and assert:

```cpp
EXPECT_TRUE(group->requestReacquisition({
    1, MediaAvReacquisitionReason::HardDiscontinuity}));
const auto acquiring = group->reacquisitionSnapshot();
ASSERT_EQ(acquiring.phase, MediaAvReacquisitionPhase::Acquiring);
ASSERT_TRUE(acquiring.transition);
EXPECT_EQ(acquiring.transition->oldGeneration, 1u);
EXPECT_EQ(acquiring.transition->nextGeneration, 2u);
EXPECT_FALSE(group->epochTransitionSnapshot().outputPermitted);
EXPECT_EQ(videoTarget->purges(), 1u);
EXPECT_EQ(audioTarget->purges(), 1u);
```

Also assert future-generation selection, strictly newer pre-begin
supersession, duplicate request idempotence, incompatible request after begin,
purge failure, missing acknowledgement timeout, and first-error poisoning.

- [ ] **Step 2: Run RED**

Use the build discipline above for
`media_transcode_av_generation_transition_tests`, then run:

```powershell
out/build/x64-debug/media_transcode_av_generation_transition_tests.exe
```

Expected: compilation fails because the coordinator, snapshot, and group
installation API do not exist.

- [ ] **Step 3: Implement the coordinator**

Implement a mutex-protected single transition. For a same-generation
discontinuity, choose `activeGeneration + 1`; for `FutureGeneration`, require
the observed generation to be strictly newer and use it exactly. Call
`beginReacquisition`, synchronously `purgeAll` every sealed participant group,
then forward each acknowledgement. Move to `Acquiring` only when the complete
planned set succeeds.

Do not duplicate the participant or timeout rules already enforced by
`MediaAvGenerationTransitionCoordinator`; delegate them through
`MediaAvEpochTransitionService`.

- [ ] **Step 4: Run GREEN**

Repeat the clean-first build and executable from Step 2.

Expected: exit `0`; output is closed immediately after request, every planned
target is purged exactly once, and all terminal cases preserve the first error.

- [ ] **Step 5: Review, commit, and push**

Review locking, transition monotonicity, RAII, and absence of fallback. Then:

```powershell
git add src/internal/graph/sync/MediaAvReacquisitionCoordinator.h `
  src/internal/graph/sync/MediaAvReacquisitionCoordinator.cpp `
  src/internal/graph/sync/MediaAvSyncGroupRuntime.h `
  src/internal/graph/sync/MediaAvSyncGroupRuntime.cpp `
  src/internal/graph/sync/MediaAvEpochTransitionService.h `
  src/internal/graph/sync/MediaAvEpochTransitionService.cpp `
  tests/unit/av_generation_transition_tests.cpp
git commit -m "feat: drive A/V generation reacquisition"
git push origin codex/quality-priority-improvements
```

---

### Task 2: Planner-Exact Purge Participant Assembly

**Files:**
- Create: `src/internal/graph/runtime/compilation/MediaAvGenerationParticipantAssembler.h`
- Create: `src/internal/graph/runtime/compilation/MediaAvGenerationParticipantAssembler.cpp`
- Modify: `src/internal/graph/runtime/factory/MediaRuntimeNodeFactory.h`
- Modify: `src/internal/graph/runtime/factory/MediaRuntimeNodeFactory.cpp`
- Modify: `src/internal/graph/runtime/compilation/MediaGraphRuntimeCompiler.cpp`
- Modify: `src/internal/graph/runtime/compilation/MediaAvSyncRuntimeBootstrap.h`
- Modify: `src/internal/graph/runtime/compilation/MediaAvSyncRuntimeBootstrap.cpp`
- Modify: `CMakeLists.txt`
- Create test: `tests/unit/av_reacquisition_runtime_assembly_tests.cpp`

**Interfaces:**
- Consumes: the immutable `MediaAvGenerationTransitionPlan` and prepared runtime nodes before scheduler registration.
- Produces:

```cpp
struct MediaAvGenerationPurgeRegistration final {
    std::string identity;
    std::shared_ptr<MediaAvGenerationPurgeTarget> target;
};

class MediaAvGenerationParticipantAssembler final {
public:
    static ::media::Result<MediaAvGenerationParticipantAssembler> create(
        MediaAvGenerationTransitionPlan plan);
    ::media::Status registerTarget(
        MediaAvGenerationParticipant participant,
        MediaAvGenerationPurgeRegistration registration);
    ::media::Result<std::vector<MediaAvGenerationParticipantGroup>>
    seal();
};
```

`MediaRuntimeNodeFactory::generationPurgeRegistration(MediaRuntimeNode&)`
returns an optional typed participant/identity/target tuple. It is the only
runtime-kind-to-participant mapping.

- [ ] **Step 1: Write failing assembly tests**

Register all canonical lineage, audio correction, scheduler, and project
MPEG-TS identities from a real planner product. Assert `seal()` succeeds only
for the exact planned set. Add one test each for missing, duplicate,
unexpected, empty, and null registrations.

Add a compiler test proving a production graph cannot enter
`MediaGraphRuntimeState::Compiled` when any planned purge target is absent.

- [ ] **Step 2: Run RED**

Add a dedicated CMake target
`media_transcode_av_reacquisition_runtime_assembly_tests`. Clean-first build
that target and run its executable.

Expected: compilation fails because the assembler and factory registration API
do not exist.

- [ ] **Step 3: Implement exact assembly**

Build one `MediaAvGenerationParticipantGroup` per planner entry. Reject every
registration not named in that entry. In `registerDefaults`, retain prepared
node ownership while extracting registrations, seal the assembler, construct
the coordinator, and install it into the exact registered sync group before
moving nodes into `MediaGraphScheduler`.

Bootstrap must pass the existing transition service and master clock by owned
shared lifetime. No compiler path may synthesize a participant or identity.

- [ ] **Step 4: Run GREEN**

Use one clean-first build with both targets:

```powershell
cmake --build out/build/x64-debug --clean-first --target `
  media_transcode_av_reacquisition_runtime_assembly_tests `
  media_transcode_runtime_tests
out/build/x64-debug/media_transcode_av_reacquisition_runtime_assembly_tests.exe
out/build/x64-debug/media_transcode_runtime_tests.exe
```

Expected: both exit `0`; exact plans seal, while every incomplete or extra set
fails before execution.

- [ ] **Step 5: Review, commit, and push**

```powershell
git add CMakeLists.txt `
  src/internal/graph/runtime/compilation/MediaAvGenerationParticipantAssembler.h `
  src/internal/graph/runtime/compilation/MediaAvGenerationParticipantAssembler.cpp `
  src/internal/graph/runtime/compilation/MediaGraphRuntimeCompiler.cpp `
  src/internal/graph/runtime/compilation/MediaAvSyncRuntimeBootstrap.h `
  src/internal/graph/runtime/compilation/MediaAvSyncRuntimeBootstrap.cpp `
  src/internal/graph/runtime/factory/MediaRuntimeNodeFactory.h `
  src/internal/graph/runtime/factory/MediaRuntimeNodeFactory.cpp `
  tests/unit/av_reacquisition_runtime_assembly_tests.cpp
git commit -m "feat: assemble planned A/V purge participants"
git push origin codex/quality-priority-improvements
```

---

### Task 3: Generation-Aware Locked Packet Gate

**Files:**
- Move: `src/internal/graph/nodes/sync/MediaInitialLockedPacketGateNode.h` to `src/internal/graph/nodes/sync/MediaLockedPacketGateNode.h`
- Move: `src/internal/graph/nodes/sync/MediaInitialLockedPacketGateNode.cpp` to `src/internal/graph/nodes/sync/MediaLockedPacketGateNode.cpp`
- Modify: `src/internal/graph/nodes/sync/MediaLockedPacketGateNode.h`
- Modify: `src/internal/graph/nodes/sync/MediaLockedPacketGateNode.cpp`
- Modify: `src/internal/graph/sync/MediaAvSyncError.h`
- Modify: `src/internal/graph/model/MediaNodeKind.h`
- Modify: `src/internal/graph/builder/segments/MediaRealtimeAvSyncNodeConfigurator.h`
- Modify: `src/internal/graph/builder/segments/MediaRealtimeAvSyncNodeConfigurator.cpp`
- Modify: `src/internal/graph/builder/segments/MediaRealtimeAvSyncProtocolInputBuilder.cpp`
- Modify: `src/internal/graph/core/MediaGraphDump.cpp`
- Modify: `src/internal/graph/diagnostics/MediaGraphDiagnostics.cpp`
- Modify: `src/internal/graph/runtime/compilation/MediaGraphRuntimeCompiler.cpp`
- Modify: `src/internal/graph/runtime/factory/MediaRuntimeNodeFactory.cpp`
- Test: `tests/unit/av_sync_canonical_input_tests.cpp`
- Test: `tests/unit/mpeg_ts_demux_node_tests.cpp`
- Test: `tests/unit/rtp_packet_clock_binder_tests.cpp`
- Test: `tests/unit/test_realtime_rtp_graph.cpp`

**Interfaces:**
- Consumes: typed `MediaSourceClockState`, `MediaAvReacquisitionSnapshot`, and `MediaAvSyncGroupRuntime::requestReacquisition`.
- Produces:

```cpp
enum class MediaLockedPacketGateDisposition : std::uint8_t {
    Pass = 0,
    WithholdForReacquisition = 1,
    DropOldGeneration = 2
};
```

The gate retains at most its planner-bounded pending packet. It never owns
transition policy.

- [ ] **Step 1: Replace Task 12 terminal expectations with Task 13 tests**

Keep malformed evidence and generation regression terminal. Add tests proving:

```cpp
EXPECT_EQ(processLocked(activeGeneration), Pass);
EXPECT_EQ(processDiscontinuity(activeGeneration),
          WithholdForReacquisition);
EXPECT_EQ(processAcquiring(nextGeneration),
          WithholdForReacquisition);
EXPECT_EQ(processLocked(oldGeneration), DropOldGeneration);
EXPECT_EQ(processLocked(nextGenerationBeforeActivation),
          WithholdForReacquisition);
```

Assert a discontinuity creates exactly one group request and does not return
`Initial locked packet gate rejects clock discontinuity`.

- [ ] **Step 2: Run RED**

Clean-first build
`media_transcode_av_sync_canonical_input_tests` and run it.

Expected: the existing gate returns the Task 12 terminal error for valid
discontinuity and reacquisition evidence.

- [ ] **Step 3: Implement gate classification**

Rename the node to `MediaLockedPacketGateNode` and update its node-kind factory
mapping because it is no longer initial-only. `acceptClock` must classify
against the group snapshot:

- pass only active locked generation;
- request once and withhold the planned next generation during transition;
- discard an explicitly old generation;
- fail on zero, regression outside the old-generation case, unplanned future
  generation, or acquiring evidence without an active transition.

Do not convert malformed protocol evidence into a reacquisition request.

- [ ] **Step 4: Run GREEN**

Use one clean-first build with all required targets, then run:

```powershell
cmake --build out/build/x64-debug --clean-first --target `
  media_transcode_av_sync_canonical_input_tests `
  media_transcode_rtp_packet_clock_binder_tests `
  media_transcode_node_tests
out/build/x64-debug/media_transcode_av_sync_canonical_input_tests.exe
out/build/x64-debug/media_transcode_rtp_packet_clock_binder_tests.exe
out/build/x64-debug/media_transcode_node_tests.exe
```

Expected: all exit `0`; valid discontinuity is withheld, while malformed
evidence still fails.

- [ ] **Step 5: Review, commit, and push**

Stage the gate move, the exact builder/model/compiler/factory references listed
above, and the four test files. Commit and push:

```powershell
git add -A -- `
  src/internal/graph/nodes/sync/MediaInitialLockedPacketGateNode.h `
  src/internal/graph/nodes/sync/MediaInitialLockedPacketGateNode.cpp `
  src/internal/graph/nodes/sync/MediaLockedPacketGateNode.h `
  src/internal/graph/nodes/sync/MediaLockedPacketGateNode.cpp `
  src/internal/graph/sync/MediaAvSyncError.h `
  src/internal/graph/model/MediaNodeKind.h `
  src/internal/graph/builder/segments/MediaRealtimeAvSyncNodeConfigurator.h `
  src/internal/graph/builder/segments/MediaRealtimeAvSyncNodeConfigurator.cpp `
  src/internal/graph/builder/segments/MediaRealtimeAvSyncProtocolInputBuilder.cpp `
  src/internal/graph/core/MediaGraphDump.cpp `
  src/internal/graph/diagnostics/MediaGraphDiagnostics.cpp `
  src/internal/graph/runtime/compilation/MediaGraphRuntimeCompiler.cpp `
  src/internal/graph/runtime/factory/MediaRuntimeNodeFactory.cpp `
  tests/unit/av_sync_canonical_input_tests.cpp `
  tests/unit/mpeg_ts_demux_node_tests.cpp `
  tests/unit/rtp_packet_clock_binder_tests.cpp `
  tests/unit/test_realtime_rtp_graph.cpp
git commit -m "feat: gate packets across A/V generations"
git push origin codex/quality-priority-improvements
```

---

### Task 4: Atomic Next-Epoch Startup Activation

**Files:**
- Modify: `src/internal/graph/runtime/buffer/MediaAvStartupEnvelopeBuffer.h`
- Modify: `src/internal/graph/runtime/buffer/MediaAvStartupEnvelopeBuffer.cpp`
- Modify: `src/internal/graph/nodes/sync/MediaAvStartupCoordinatorNode.cpp`
- Modify: `src/internal/graph/nodes/sync/MediaActivatedStartupReleaseSequencerNode.h`
- Modify: `src/internal/graph/nodes/sync/MediaActivatedStartupReleaseSequencerNode.cpp`
- Modify: `src/internal/graph/nodes/sync/MediaPlaybackEpochBinderNode.cpp`
- Modify: `src/internal/graph/nodes/sync/MediaAvBoundReleaseExtractorNode.cpp`
- Test: `tests/unit/av_startup_coordinator_tests.cpp`
- Test: `tests/unit/av_sync_production_release_tests.cpp`
- Test: `tests/unit/av_playback_epoch_binder_tests.cpp`

**Interfaces:**
- Appends `NextAtomicRelease = 2` to `MediaAvStartupReleaseKind`.
- Extends `MediaAvStartupReleaseBuffer::create` with an explicit
  `std::optional<std::uint64_t> completedTransitionSequence`.
- `InitialAtomicRelease` and `ActiveEpochPassThrough` require `std::nullopt`;
  `NextAtomicRelease` requires a non-zero sequence.

- [ ] **Step 1: Write failing next-release tests**

Create a completed purge for generation 1 to 2, feed locked audio/video startup
units for generation 2, and assert the coordinator emits:

```cpp
EXPECT_EQ(release->releaseKind(),
          MediaAvStartupReleaseKind::NextAtomicRelease);
ASSERT_TRUE(release->completedTransitionSequence());
EXPECT_EQ(*release->completedTransitionSequence(),
          purge.transitionSequence);
```

In the sequencer test, assert `activateNext` is invoked once before either
released stream is pushed. Add duplicate sequence, wrong generation,
zero/missing sequence, and activation failure cases.

- [ ] **Step 2: Run RED**

Clean-first build
`media_transcode_av_sync_production_release_tests` and run it.

Expected: compilation fails because `NextAtomicRelease` and transition sequence
metadata do not exist.

- [ ] **Step 3: Implement next activation**

Update every release-buffer construction call to pass explicit metadata.
During reacquisition, startup coordination reuses the initial common-start
algorithm but labels the transaction `NextAtomicRelease`. The sequencer checks
the group transition snapshot, calls:

```cpp
m_activationCapability.activateNext(
    release->epoch(),
    release->audioOrigin(),
    *release->completedTransitionSequence());
```

Only after success may it publish the activated event and push either media
batch. Then call `markReacquisitionActivated` with the exact generation and
sequence.

- [ ] **Step 4: Run GREEN**

Use one clean-first build with all required targets, then run:

```powershell
cmake --build out/build/x64-debug --clean-first --target `
  media_transcode_node_tests `
  media_transcode_av_playback_epoch_binder_tests `
  media_transcode_av_sync_production_release_tests
out/build/x64-debug/media_transcode_node_tests.exe
out/build/x64-debug/media_transcode_av_playback_epoch_binder_tests.exe
out/build/x64-debug/media_transcode_av_sync_production_release_tests.exe
```

Expected: all exit `0`; activation precedes both outputs and occurs exactly
once.

- [ ] **Step 5: Review, commit, and push**

Stage only the listed buffers, nodes, and tests. Commit and push:

```powershell
git commit -m "feat: activate next A/V playback epoch"
git push origin codex/quality-priority-improvements
```

---

### Task 5: Protocol Output Generation Rollover

**Files:**
- Create: `src/internal/graph/sync/MediaProtocolOutputGenerationState.h`
- Create: `src/internal/graph/sync/MediaProtocolOutputGenerationState.cpp`
- Modify: `src/internal/graph/nodes/sync/MediaAvOutputSchedulerNode.h`
- Modify: `src/internal/graph/nodes/sync/MediaAvOutputSchedulerNode.cpp`
- Modify: `src/internal/graph/nodes/output/MediaScheduledRtpSenderNode.h`
- Modify: `src/internal/graph/nodes/output/MediaScheduledRtpSenderNode.cpp`
- Modify: `src/internal/graph/nodes/output/MediaScheduledTsAccessUnitAdapterNode.h`
- Modify: `src/internal/graph/nodes/output/MediaScheduledTsAccessUnitAdapterNode.cpp`
- Modify: `src/internal/graph/nodes/output/MediaProjectMpegTsPlanSourceNode.h`
- Modify: `src/internal/graph/nodes/output/MediaProjectMpegTsPlanSourceNode.cpp`
- Modify: `src/internal/graph/planner/avsync/MediaAvGenerationTransitionPlanner.cpp`
- Modify: `src/internal/graph/planner/avsync/MediaAvSyncPlanValidator.cpp`
- Test: `tests/unit/av_sync_scheduled_output_tests.cpp`
- Test: `tests/unit/scheduled_rtp_output_node_tests.cpp`
- Test: `tests/unit/scheduled_mpeg_ts_output_node_tests.cpp`

**Interfaces:**
- Each planned output authority exposes one stable
  `MediaAvGenerationPurgeTarget` through the factory registration API from
  Task 2.
- UDP socket/resource ownership remains outside purge state.
- Scheduler and protocol adapters call
  `epochTransitionSnapshot().outputPermitted` before commit and require exact
  generation equality.

- [ ] **Step 1: Write failing rollover tests**

For scheduler, RTP, and MPEG-TS, enqueue old-generation output, request
reacquisition, and assert no old unit commits after permit close. Complete
generation 2 activation and assert the same fake socket/resource receives
generation 2 output.

For MPEG-TS, assert PCR/DTS/PTS watermark and continuity state reset while the
injected byte sink object identity remains unchanged.

- [ ] **Step 2: Run RED**

Use one clean-first build with the three focused output targets, then run the
three executables.

Expected: at least one planned identity is absent or old-generation output
state survives purge.

- [ ] **Step 3: Implement planned output targets**

Use focused state objects per participant:

```cpp
class MediaProtocolOutputGenerationState final
    : public MediaAvGenerationPurgeTarget {
public:
    ::media::Status permitActivatedGeneration(
        std::uint64_t generation,
        std::uint64_t transitionSequence);
    ::media::Status validateCommitGeneration(
        std::uint64_t generation) const;
    ::media::Status purge(
        const MediaAvGenerationPurge& purge) override;
};
```

Keep RTP sockets and MPEG-TS byte sink/resource outside this state. Purge only
generation-owned timestamp, continuity, buffered-unit, and watermark data.
Extend planner participant identities only when the implementation introduces
a distinct stateful boundary; update validator exact-set assertions with it.

- [ ] **Step 4: Run GREEN**

Use one clean-first build with all three targets, then run:

```powershell
cmake --build out/build/x64-debug --clean-first --target `
  media_transcode_av_sync_scheduled_output_tests `
  media_transcode_scheduled_rtp_output_node_tests `
  media_transcode_scheduled_mpeg_ts_output_node_tests
out/build/x64-debug/media_transcode_av_sync_scheduled_output_tests.exe
out/build/x64-debug/media_transcode_scheduled_rtp_output_node_tests.exe
out/build/x64-debug/media_transcode_scheduled_mpeg_ts_output_node_tests.exe
```

Expected: all exit `0`; no cross-generation commit, and resources remain
bound.

- [ ] **Step 5: Review, commit, and push**

Stage only output state, planner/validator, and focused tests. Commit and push:

```powershell
git commit -m "feat: roll protocol output generations"
git push origin codex/quality-priority-improvements
```

---

### Task 6: Structured Transition Diagnostics and Drift Segmentation

**Files:**
- Create: `src/internal/graph/runtime/diagnostics/MediaAvReacquisitionRecord.h`
- Create: `src/internal/graph/runtime/diagnostics/MediaAvReacquisitionDiagnostics.h`
- Create: `src/internal/graph/runtime/diagnostics/MediaAvReacquisitionDiagnostics.cpp`
- Modify: `src/internal/graph/sync/MediaAvReacquisitionCoordinator.h`
- Modify: `src/internal/graph/sync/MediaAvReacquisitionCoordinator.cpp`
- Modify: `src/internal/graph/nodes/sync/MediaAudioDriftControllerNode.cpp`
- Modify: `src/internal/graph/runtime/diagnostics/MediaGraphRuntimeReport.h`
- Modify: `src/internal/graph/runtime/diagnostics/MediaGraphRuntimeReport.cpp`
- Modify: `src/internal/graph/runtime/MediaGraphRuntime.h`
- Modify: `src/internal/graph/runtime/MediaGraphRuntime.cpp`
- Test: `tests/unit/av_generation_transition_tests.cpp`
- Test: `tests/unit/av_sync_audio_control_tests.cpp`
- Test: `tests/unit/test_event_driven_runtime.cpp`

**Interfaces:**
- Produces one immutable record:

```cpp
struct MediaAvReacquisitionRecord final {
    MediaAvSyncGroupKey groupKey;
    MediaAvSyncTopology topology;
    MediaAvReacquisitionReason reason;
    std::uint64_t oldGeneration;
    std::uint64_t nextGeneration;
    std::uint64_t transitionSequence;
    MediaRunningTime permitClosedAt;
    std::optional<MediaRunningTime> purgeCompletedAt;
    std::optional<MediaRunningTime> clocksLockedAt;
    std::optional<MediaRunningTime> activatedAt;
    std::vector<MediaAvGenerationParticipant> acknowledgements;
    std::optional<::media::ErrorInfo> failure;
};
```

- Drift samples carry the playback generation and reject cross-generation
  comparison.

- [ ] **Step 1: Write failing diagnostic tests**

Assert the successful record contains every timestamp in monotonic order and
the exact participant set. Assert a purge failure retains the same first error.
Feed generation 1 then generation 2 drift samples and assert no delta is
computed across the boundary. Abort a runtime through a transition failure,
then assert `stop()` is idempotent and does not replace the recorded worker
failure with `runtime is not running`.

- [ ] **Step 2: Run RED**

Clean-first build
`media_transcode_av_generation_transition_tests` and
`media_transcode_av_sync_audio_control_tests`, then run both.

Expected: record and generation-segmented drift API are missing.

- [ ] **Step 3: Implement diagnostics**

The coordinator writes timestamps at the existing lifecycle linearization
points. `MediaRuntimeReport` owns a bounded vector of completed records and at
most one active record per group. Diagnostics log one structured line on begin,
purge completion, activation, resume, and failure.

On drift-controller purge, clear the previous generation baseline before
accepting the next generation. Never compare or aggregate positions from
different generations.

- [ ] **Step 4: Run GREEN**

Use one clean-first build for
`media_transcode_av_generation_transition_tests`,
`media_transcode_av_sync_audio_control_tests`, and
`media_transcode_runtime_tests`, then run the three executables.

Expected: all exit `0`; metrics are generation segmented and terminal
diagnostics preserve the first error.

- [ ] **Step 5: Review, commit, and push**

Stage only diagnostics, report, coordinator, drift controller, and tests.
Commit and push:

```powershell
git commit -m "feat: report A/V reacquisition transitions"
git push origin codex/quality-priority-improvements
```

---

### Task 7: Deterministic MPEG-TS and RTP Recovery Integration

**Files:**
- Create: `tests/unit/av_sync_reacquisition_integration_tests.cpp`
- Modify: `tests/unit/mpeg_ts_crafted_udp_fault_tests.cpp`
- Modify: `tests/unit/rtp_packet_clock_binder_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Uses only public planner, graph builder, compiler, and runtime entry points.
- Injects one deterministic discontinuity; it does not call coordinator
  internals.

- [ ] **Step 1: Write failing production-graph tests**

Build the real MPEG-TS-to-MPEG-TS synchronized graph. A test fixture
`RecordingGenerationSink` records committed packet generation and retains the
sink object address. Feed generation 1 audio and video, inject a valid
PCR/program discontinuity while continuing packets, then feed generation 2.
Assert:

```cpp
EXPECT_TRUE(runtime.running());
EXPECT_EQ(report.metrics.workerErrors, 0u);
EXPECT_EQ(report.reacquisitions.size(), 1u);
EXPECT_EQ(report.reacquisitions.front().oldGeneration, 1u);
EXPECT_EQ(report.reacquisitions.front().nextGeneration, 2u);
EXPECT_GT(sink.packetCount(2), 0u);
EXPECT_EQ(sink.crossGenerationPacketCount(), 0u);
```

Add the equivalent RTP clock degradation/CNAME discontinuity case. Assert the
configured sink/socket identity before and after is identical.

- [ ] **Step 2: Run RED**

Register
`media_transcode_av_sync_reacquisition_integration_tests` with deterministic
labels. Clean-first build and run it.

Expected: the current production graph terminates at the locked gate or cannot
assemble the reacquisition participants.

- [ ] **Step 3: Complete only missing production wiring**

If the test exposes a missing connection, add it at the planner, builder,
compiler, or factory layer that owns that connection. Do not add
protocol-specific bypasses to gates, schedulers, or output nodes.

The final topology must visibly traverse:

```text
protocol clock evidence
  -> group reacquisition request
  -> permit close
  -> planned participant purge/acknowledgement
  -> next-generation A/V startup acquisition
  -> activateNext
  -> atomic release
  -> existing bound protocol output resource
```

- [ ] **Step 4: Run deterministic GREEN verification**

Clean-first build the deterministic preset with target `all`, then run:

```powershell
out/build/x64-debug/media_transcode_av_sync_reacquisition_integration_tests.exe
ctest --test-dir out/build/x64-debug -C Debug `
  --output-on-failure -L deterministic --timeout 60
```

Expected: all commands exit `0`. No high-memory production runtime test runs.

- [ ] **Step 5: Run one bounded realtime CLI diagnostic**

Use independent local ports, a continuously sending FFmpeg input, and a
20-second supervisor timeout. Inject or loop across exactly one verified
discontinuity. At the timeout, assert sender and CLI were both still alive,
then terminate both processes and inspect the final report.

Expected: the CLI did not self-stop, the report contains one successful
generation 1-to-2 transition, `workerErrors=0`, and output packet count
continued increasing after activation. Do not leave either process running.

- [ ] **Step 6: Review, commit, and push**

Stage integration tests, CMake registration, and only wiring files proven
necessary by Step 3. Commit and push:

```powershell
git commit -m "test: prove production A/V reacquisition"
git push origin codex/quality-priority-improvements
```

---

### Task 8: Completion Evidence, Independent Review, and PR

**Files:**
- Create: `docs/completed/av-sync-automatic-reacquisition.md`
- Modify: `plan.md`
- Modify: `QUALITY_SCORE.md`

**Interfaces:**
- Consumes the exact test results from Tasks 1-7.
- Produces no runtime behavior.

- [ ] **Step 1: Review the entire branch**

Inspect the merge-base diff for design coverage, planner authority, complete
purge target registration, RAII, enum stability, first-error preservation,
UTF-8/CRLF, and unrelated-file exclusion. Fix every discovered omission with a
focused RED/GREEN cycle before proceeding.

- [ ] **Step 2: Run strongest bounded verification**

Clean-first build the deterministic and integration presets separately with
`/showIncludes` removed. Run their matching CTest presets with a finite timeout.
Do not run the prohibited high-memory executable and do not run a background
long-duration test.

Expected: all supported tests pass; environment-dependent absence is reported
only through an existing explicit skip contract.

- [ ] **Step 3: Record evidence and score**

The completion document records test commands and results, not build commands.
Update `plan.md` status and rescore the existing `QUALITY_SCORE.md` dimensions
from evidence. Record remaining risks, including long-run objective drift
monitoring and pending human-eye acceptance.

- [ ] **Step 4: Commit and push documentation**

```powershell
git add docs/completed/av-sync-automatic-reacquisition.md plan.md QUALITY_SCORE.md
git commit -m "docs: complete automatic A/V reacquisition"
git push origin codex/quality-priority-improvements
```

- [ ] **Step 5: Fresh independent Agent review**

Give a new Agent the approved design, this plan, and complete branch diff. The
review must issue a PASS or list concrete blocking findings for:

- group-level closed-loop recovery;
- planner-only policy;
- exact participant assembly;
- no cross-generation output;
- atomic next activation;
- same endpoint/resource lifetime;
- deterministic test evidence; and
- no unrelated compatibility behavior.

Fix blocking findings, repeat focused verification, commit/push, and request a
new review until PASS.

- [ ] **Step 6: Open or refresh the PR**

Confirm the remote branch head equals local `HEAD`, open or update the PR
against `master`, and verify all four CI checks. Do not merge until CI and the
fresh review pass. Human-eye testing remains a separately approved action.
