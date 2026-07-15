# A/V Synchronization Production Wiring Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Complete the remaining production A/V synchronization work after Tasks 1-11 so separate RTP A/V input to separate RTP A/V output and MPEG-TS input to MPEG-TS output share one media clock, start atomically, remain synchronized without drift, and pass two-minute hardware acceptance.

**Architecture:** The planner emits one complete synchronized runtime product. Runtime compilation creates one group-owned steady master clock and, for RTP only, one shared NTP epoch; canonical lineage then survives startup selection, codec reordering, filtering, resampling, encoding, scheduling, and protocol commit. One binder owns epoch activation, one transaction coordinator owns generation changes, one scheduler paces only `emitOnMaster`, and protocol adapters acknowledge only completely committed access units.

**Tech Stack:** C++20, FFmpeg codec/filter/resampler APIs, project-owned RTP/RTCP and MPEG-TS protocol components, event-driven graph runtime, CMake/Ninja/CTest, PowerShell, FFmpeg tools, VLC, CUDA/NVENC hardware encoding on Windows.

## Global Constraints

- Tasks 1-11 in `docs/superpowers/plans/2026-07-11-av-sync-system.md` are accepted baselines. Reuse their components; do not create parallel clock mappers, schedulers, RTP senders, TS serializers, or startup policies.
- Supported synchronized topologies are exactly separate RTP A/V to separate RTP A/V and MPEG-TS A/V to MPEG-TS. Separate RTP input to MPEG-TS output remains planner-rejected.
- `MediaSteadyMasterClock` is the only synchronized media pacing clock. One group has one immutable plan, one active generation, and for RTP one runtime-captured `MediaSharedNtpEpoch` shared by both senders.
- Planner owns all topology, codec, queue, protocol, correction, lifecycle, transition, and capacity decisions. Builders assemble; runtime validates; nodes never infer defaults or fall back.
- Audio/video re-encoding remains naturally driven by comparing requested output parameters with input facts. Do not add `transcodeAudio` or any equivalent switch.
- Synchronized paths cannot use arrival-time rebasing, DTS-as-PTS, timestamp synthesis, local stream zeroing, `+1 tick`, startup sleeps, per-mux pacing, or FFmpeg RTP/MPEG-TS timing authority.
- `PacketStartGateNode` remains only for explicit video-only paths. No synchronized A/V graph may contain `AvPacketStartBarrierNode`, `PacketNormalizeNode` timestamp shifting, `VideoTimestampNode` repair, `RtpMuxNode`, or `MediaGraphPacingClock`.
- New enum values append without renumbering. Deleted values become reserved when compatibility of numeric values matters.
- All ownership is RAII. FFmpeg payloads are not copied merely to carry lineage; queued objects retain `MediaBufferRef`, never borrowed raw pointers.
- Every code task is TDD-driven, followed by a full `--clean-first --target all` rebuild, covering tests, a focused commit/push, and a fresh two-stage spec/code-quality review before the next task.
- Modified text is UTF-8 without BOM and CRLF. Stage only task-owned files; preserve unrelated dirty workspace files.
- CMake may register project sources and tests only. It must not copy runtime DLLs, discover VLC, or reference `D:\mabs\local64\bin-video`.
- Final acceptance is hardware-only: separate RTP and MPEG-TS each run 120 seconds. Local and software paths are not rerun in this cycle.
- CPU acceptance measures only the exact `media_transcode_realtime_video_cli` process and normalizes `delta TotalProcessorTime / (wall * logical processors) * 100`; time-weighted average must be at most 5%.

---

### Task 1: Produce one complete planner-owned synchronized runtime product

**Files:**
- Create: `src/internal/graph/planner/avsync/MediaAvGenerationTransitionPlan.h`
- Create: `src/internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h`
- Create: `src/internal/graph/planner/realtime/MediaScheduledRtpOutputPlan.h`
- Create: `src/internal/graph/planner/realtime/MediaScheduledRtpOutputPlan.cpp`
- Create: `src/internal/graph/sync/MediaAudioPlaybackOrigin.h`
- Modify: `src/internal/graph/planner/avsync/MediaAvSyncPlan.h`
- Modify: `src/internal/graph/planner/avsync/MediaAvSyncPlanValidator.cpp`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.cpp`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeOutputPolicyPlanner.cpp`
- Modify: `tests/unit/test_planner.cpp`

**Interfaces:**
- Consumes the accepted `MediaAvSyncPlan`, `MediaGraphQueueParameters`, codec delay facts, RTP sender policy, and `MediaTsMuxPlan`.
- Produces the sole `MediaRealtimeAvSyncRuntimePlan` consumed by the executable binding and synchronized builder.

```cpp
enum class MediaAvSyncOutputAdapterKind : std::uint8_t {
    ScheduledSeparateRtp = 0,
    ProjectMpegTs = 1
};

enum class MediaAvGenerationParticipant : std::uint8_t {
    CanonicalLineage = 0,
    AudioCorrection = 1,
    Scheduler = 2,
    RtpVideoOutput = 3,
    RtpAudioOutput = 4,
    ProjectMpegTsOutput = 5
};

struct MediaAudioCorrectionReachabilityPlan final {
    int outputSampleRate;
    std::int64_t epochOutputSampleIndex;
    std::int64_t worstCaseInFlightSamples;
    std::int64_t mailboxDeliveryMarginSamples;
    std::int64_t maximumResamplerOutputBlockSamples;
    std::int64_t commandLeadSamples;
    std::size_t mailboxCapacity;
};

struct MediaAvGenerationParticipantPlan final {
    MediaAvGenerationParticipant participant;
    std::vector<std::string> requiredChildren;
};

struct MediaAvGenerationTransitionPlan final {
    std::vector<MediaAvGenerationParticipantPlan> participants;
    MediaRunningTime acknowledgementTimeout;
    MediaRunningTime terminalDrainWindow;
};

struct MediaScheduledRtpOutputPlan final {
    MediaScheduledStream stream;
    MediaRtpUdpSenderConfig transport;
    ScheduledRtpMuxStreamConfig packetization;
    std::uint32_t ssrc;
    std::uint32_t baseTimestamp;
    int clockRate;
    std::string cname;
    MediaRunningTime senderLead;
    MediaRunningTime senderReportInterval;
};

struct MediaSeparateRtpOutputRuntimePlan final {
    MediaScheduledRtpOutputPlan video;
    MediaScheduledRtpOutputPlan audio;
    std::string sdpPath;
};

struct MediaProjectMpegTsRuntimeOutputPlan final {
    std::string url;
    MediaOutputResourceKind resourceKind;
    MediaMuxSessionKind muxSessionKind;
    MediaProjectMpegTsOutputPlan protocol;
};

struct MediaRealtimeAvSyncRuntimePlan final {
    MediaAvSyncGroupKey groupKey;
    MediaAvSyncPlan synchronization;
    MediaAvSyncOutputAdapterKind outputAdapter;
    std::variant<MediaSeparateRtpOutputRuntimePlan,
                 MediaProjectMpegTsRuntimeOutputPlan> protocolOutput;
    MediaGraphQueueParameters queues;
    MediaRealtimeEdgePolicySet edgePolicies;
    MediaThreadingPolicy threadingPolicy;
    MediaAvGenerationTransitionPlan transition;
    MediaAudioCorrectionReachabilityPlan audioCorrection;
};

struct MediaAudioPlaybackOrigin final {
    std::uint64_t generation;
    MediaRunningTime sourceStart;
    MediaRunningTime masterRelease;
    std::int64_t epochOutputSampleIndex;
    int outputSampleRate;
};
```

`MediaSeparateRtpOutputRuntimePlan` contains complete video/audio `MediaScheduledRtpOutputPlan` values and the typed SDP publication path. `MediaProjectMpegTsRuntimeOutputPlan` contains the output URL, explicit `ByteSink + ProjectMpegTs` kinds, and the accepted `MediaProjectMpegTsOutputPlan`. Synchronized builders consume only this variant and the nested queue/edge/threading products; they do not combine decisions from outer legacy plan fields.

- [ ] **Step 1: Write failing planner-product tests**

Add table-driven tests that require `realtime.av` as the non-empty group key, explicit `epochOutputSampleIndex == 0`, the RTP participant set `{CanonicalLineage, AudioCorrection, Scheduler, RtpVideoOutput, RtpAudioOutput}`, and the TS set `{CanonicalLineage, AudioCorrection, Scheduler, ProjectMpegTsOutput}`. Require exact child identities for every aggregate: canonical lineage includes `startup_generation_state` and every video/audio codec/filter/trim registry, while audio correction has exactly one child, `audio_correction_generation_state`. That child transactionally owns servo, mailbox, resampler watermark, and pending commit acknowledgements. Mutate every protocol output, queue/edge/threading product, child identity, capacity, sample bound, participant, timeout, topology/adapter pair, and legacy pacing/barrier field independently and assert planning fails.

```cpp
const auto planned = MediaRealtimeRtpTranscodePlanner::plan(validSeparateRtpAvRequest());
EXPECT_TRUE(ctx, planned);
EXPECT_TRUE(ctx, planned.value().avSyncRuntime.has_value());
EXPECT_EQ(ctx, planned.value().avSyncRuntime->groupKey.value(), "realtime.av");
EXPECT_EQ(ctx, planned.value().avSyncRuntime->outputAdapter,
          MediaAvSyncOutputAdapterKind::ScheduledSeparateRtp);
EXPECT_TRUE(ctx, planned.value().avSyncRuntime->audioCorrection.commandLeadSamples >
    planned.value().avSyncRuntime->audioCorrection.worstCaseInFlightSamples +
    planned.value().avSyncRuntime->audioCorrection.mailboxDeliveryMarginSamples +
    planned.value().avSyncRuntime->audioCorrection.maximumResamplerOutputBlockSamples);
```

- [ ] **Step 2: Run RED with a full clean rebuild**

Run `cmake --build out/build/x64-debug --clean-first --target all`. Expected: compilation fails because `MediaRealtimeAvSyncRuntimePlan` does not exist.

- [ ] **Step 3: Implement the planner product and proof**

Replace `std::optional<MediaAvSyncPlan> avSync` with `std::optional<MediaRealtimeAvSyncRuntimePlan> avSyncRuntime`. Until Task 11 physically removes the legacy struct fields, synchronized requests must leave them inert and validation must reject any conflicting barrier/pacing value. Convert every bounded audio queue, decoder delay, encoder lookahead, protocol batch, mailbox delivery margin, and maximum resampler block to output samples with checked arithmetic. Reject missing facts, overflow, duplicate/missing participants, incorrect adapter/topology pairs, or failure of:

```text
commandLeadSamples > worstCaseInFlightSamples
                   + mailboxDeliveryMarginSamples
                   + maximumResamplerOutputBlockSamples
```

Keep non-synchronized/video-only plan fields separate. Protocol runtime products reference the accepted RTP/TS synchronization policy and add only concrete output resources/limits; they do not re-decide or contradict it.

- [ ] **Step 4: Run GREEN and deterministic tests**

Run `cmake --build out/build/x64-debug --clean-first --target all`, then `out/build/x64-debug/media_transcode_planner_tests.exe` and `ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -L deterministic --timeout 60`. Expected: build succeeds and all commands exit `0`.

- [ ] **Step 5: Review, commit, and push**

Require a fresh spec reviewer and code-quality reviewer to report zero Critical/Important findings. Commit only Task 1 as `feat: plan synchronized runtime product`, push `codex/quality-priority-improvements`, and append the base/head/review result to `.superpowers/sdd/progress.md` without staging unrelated progress history.

---

### Task 2: Bootstrap one typed runtime group, master clock, and RTP NTP epoch

**Files:**
- Create: `src/internal/graph/runtime/factory/MediaAvSyncRuntimeBinding.h`
- Create: `src/internal/graph/runtime/compilation/MediaAvSyncRuntimeBootstrap.h`
- Create: `src/internal/graph/runtime/compilation/MediaAvSyncRuntimeBootstrap.cpp`
- Create: `tests/unit/av_sync_runtime_bootstrap_tests.cpp`
- Modify: `src/internal/graph/runtime/factory/MediaRealtimeExecutableGraph.h`
- Modify: `src/internal/graph/runtime/MediaGraphRuntime.h`
- Modify: `src/internal/graph/runtime/MediaGraphRuntime.cpp`
- Modify: `src/internal/graph/runtime/compilation/MediaGraphRuntimeCompiler.h`
- Modify: `src/internal/graph/runtime/compilation/MediaGraphRuntimeCompiler.cpp`
- Modify: `src/internal/graph/runtime/context/MediaGraphExecutionContext.h`
- Modify: `src/internal/graph/runtime/context/MediaGraphExecutionContext.cpp`
- Modify: `src/internal/graph/runtime/context/MediaAvSyncGroupRegistry.h`
- Modify: `src/internal/graph/runtime/context/MediaAvSyncGroupRegistry.cpp`
- Modify: `src/internal/graph/sync/MediaAvSyncGroupRuntime.h`
- Modify: `src/internal/graph/sync/MediaAvSyncGroupRuntime.cpp`
- Modify: `src/internal/graph/runtime/lifecycle/MediaGraphRuntimeLifecycleExecutor.cpp`
- Modify: `src/internal/graph/builder/realtime/MediaRealtimeRtpTranscodeGraphBuilder.cpp`
- Modify: `tests/unit/test_event_driven_runtime.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**

```cpp
struct MediaAvSyncRuntimeBinding final {
    MediaAvSyncGroupKey groupKey;
    MediaAvSyncPlan plan;
};

struct MediaAvSyncClockBundle final {
    std::shared_ptr<MediaSteadyMasterClock> masterClock;
    std::shared_ptr<const MediaSharedNtpEpoch> sharedNtpEpoch;
};

class MediaAvSyncClockSource {
public:
    virtual ~MediaAvSyncClockSource() = default;
    virtual ::media::Result<MediaAvSyncClockBundle> capture(
        bool requireSharedNtpEpoch) = 0;
};

class MediaAvSyncRuntimeBootstrap final {
public:
    static ::media::Result<MediaAvSyncClockBundle> createClocks(
        const MediaAvSyncRuntimeBinding& binding,
        MediaAvSyncClockSource& source);
    static ::media::Status registerGroup(
        const MediaAvSyncRuntimeBinding& binding,
        MediaAvSyncClockBundle clocks,
        MediaGraphExecutionContext& context);
};
```

`MediaRealtimeExecutableGraph` gains exactly one optional `avSyncBinding`; it never contains a playback epoch or node-created clock.

- [ ] **Step 1: Write failing bootstrap transaction tests**

Cover missing/duplicate bindings, invalid group, graph node group mismatch, compiler rollback, `AwaitingEpoch` initial state, one steady clock, RTP shared-NTP presence, TS shared-NTP absence, and exact cleanup/wakeup on stop, abort, reset, recompilation, and compile failure. Use an injected deterministic capture source in tests to prove both RTP sender configs later observe the same epoch value.

- [ ] **Step 2: Run RED with a full clean rebuild**

Run `cmake --build out/build/x64-debug --clean-first --target all`. Expected: compilation fails for the missing binding/bootstrap types.

- [ ] **Step 3: Implement transactional bootstrap**

Pass the complete executable into compiler validation before moving out graph/input bindings. Create a temporary context and let the production `MediaAvSyncClockSource` capture the sole `MediaSteadyMasterClock` plus `(masterNow, system_clock::now())` once for RTP. Construct one shared immutable `MediaSharedNtpEpoch`, register the group, and commit the prepared context only after all validation succeeds. Store that shared object in `MediaAvSyncGroupRuntime` and inject the same pointer into both sender configurations; never create or copy an epoch in a node. Group shutdown is idempotent and wakes every deadline waiter.

- [ ] **Step 4: Run GREEN**

Run `cmake --build out/build/x64-debug --clean-first --target all`, `out/build/x64-debug/media_transcode_runtime_tests.exe`, and `ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -L deterministic --timeout 60`. Expected: all exit `0`, and the test runtime remains `AwaitingEpoch` until a binder activates it.

- [ ] **Step 5: Review, commit, and push**

After two-stage fresh review, commit as `feat: bootstrap av sync runtime group` and push the same branch.

---

### Task 3: Enforce binder-only epoch activation and atomic generation transitions

**Files:**
- Create: `src/internal/graph/sync/MediaPlaybackEpochActivationCapability.h`
- Create: `src/internal/graph/sync/MediaAvGenerationTransition.h`
- Create: `src/internal/graph/sync/MediaAvGenerationTransitionCoordinator.h`
- Create: `src/internal/graph/sync/MediaAvGenerationTransitionCoordinator.cpp`
- Create: `src/internal/graph/sync/MediaAvGenerationPurgeTarget.h`
- Create: `src/internal/graph/sync/MediaAvGenerationParticipantGroup.h`
- Create: `src/internal/graph/sync/MediaAvGenerationParticipantGroup.cpp`
- Create: `src/internal/graph/sync/MediaAvEpochTransitionService.h`
- Create: `src/internal/graph/sync/MediaAvEpochTransitionService.cpp`
- Create: `src/internal/graph/nodes/sync/MediaPlaybackEpochBinderNode.h`
- Create: `src/internal/graph/nodes/sync/MediaPlaybackEpochBinderNode.cpp`
- Create: `tests/unit/av_generation_transition_tests.cpp`
- Create: `tests/unit/av_playback_epoch_binder_tests.cpp`
- Modify: `src/internal/graph/model/MediaNodeKind.h`
- Modify: `src/internal/graph/core/MediaGraphDump.cpp`
- Modify: `src/internal/graph/diagnostics/MediaGraphDiagnostics.cpp`
- Modify: `src/internal/graph/runtime/compilation/MediaGraphRuntimeCompiler.h`
- Modify: `src/internal/graph/runtime/compilation/MediaGraphRuntimeCompiler.cpp`
- Modify: `src/internal/graph/runtime/factory/MediaRuntimeNodeFactory.h`
- Modify: `src/internal/graph/runtime/factory/MediaRuntimeNodeFactory.cpp`
- Modify: `src/internal/graph/runtime/context/MediaGraphExecutionContext.h`
- Modify: `src/internal/graph/runtime/context/MediaGraphExecutionContext.cpp`
- Modify: `src/internal/graph/runtime/context/MediaAvSyncGroupRegistry.h`
- Modify: `src/internal/graph/runtime/context/MediaAvSyncGroupRegistry.cpp`
- Modify: `src/internal/graph/sync/MediaAvSyncGroupRuntime.h`
- Modify: `src/internal/graph/sync/MediaAvSyncGroupRuntime.cpp`
- Modify: `src/internal/graph/runtime/buffer/MediaAvStartupEnvelopeBuffer.h`
- Modify: `src/internal/graph/runtime/buffer/MediaAvStartupEnvelopeBuffer.cpp`
- Modify: `tests/unit/test_event_driven_runtime.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**

```cpp
struct MediaAvGenerationPurge final {
    std::uint64_t oldGeneration;
    std::uint64_t nextGeneration;
    std::uint64_t transitionSequence;
};

struct MediaAvGenerationAcknowledgement final {
    MediaAvGenerationParticipant participant;
    std::uint64_t transitionSequence;
    ::media::Status status;
};

class MediaAvGenerationTransitionCoordinator final {
public:
    static ::media::Result<MediaAvGenerationTransitionCoordinator> create(
        MediaAvGenerationTransitionPlan plan);
    ::media::Result<MediaAvGenerationPurge> begin(
        std::uint64_t oldGeneration, std::uint64_t nextGeneration);
    ::media::Result<bool> acknowledge(MediaAvGenerationAcknowledgement ack);
    bool outputPermitted(std::uint64_t generation) const noexcept;
};

class MediaAvGenerationPurgeTarget {
public:
    virtual ~MediaAvGenerationPurgeTarget() = default;
    virtual ::media::Status purge(const MediaAvGenerationPurge& purge) = 0;
};

class MediaAvGenerationParticipantGroup final {
public:
    static ::media::Result<MediaAvGenerationParticipantGroup> create(
        MediaAvGenerationParticipantPlan plan);
    ::media::Status registerChild(
        std::string identity,
        std::shared_ptr<MediaAvGenerationPurgeTarget> child);
    ::media::Status seal();
    ::media::Result<MediaAvGenerationAcknowledgement> purgeAll(
        const MediaAvGenerationPurge& purge);
};

class MediaAvEpochTransitionService final {
public:
    ::media::Status activateInitial(MediaPlaybackEpoch epoch,
                                    MediaAudioPlaybackOrigin audioOrigin);
    ::media::Status activateNextAfter(
        std::uint64_t completedTransitionSequence,
        MediaPlaybackEpoch epoch,
        MediaAudioPlaybackOrigin audioOrigin);
};
```

Only the runtime-issued, move-only `MediaPlaybackEpochActivationCapability` can call the group activation service. Remove the general activation methods from `MediaGraphExecutionContext`, registry consumers, and public group API.

```cpp
class MediaPlaybackEpochActivationCapability final {
public:
    MediaPlaybackEpochActivationCapability(MediaPlaybackEpochActivationCapability&&) noexcept;
    MediaPlaybackEpochActivationCapability& operator=(
        MediaPlaybackEpochActivationCapability&&) noexcept;
    MediaPlaybackEpochActivationCapability(const MediaPlaybackEpochActivationCapability&) = delete;
    ::media::Status activateInitial(MediaPlaybackEpoch epoch,
                                    MediaAudioPlaybackOrigin audioOrigin);
    ::media::Status activateNext(MediaPlaybackEpoch epoch,
                                 MediaAudioPlaybackOrigin audioOrigin,
                                 std::uint64_t completedTransitionSequence);
private:
    friend class MediaAvSyncRuntimeBootstrap;
    explicit MediaPlaybackEpochActivationCapability(
        std::weak_ptr<MediaAvEpochTransitionService> transition);
};
```

- [ ] **Step 1: Write failing transition and binder tests**

Test first activation, reacquisition, repeated/partial/conflicting release rejection, old permit revocation before purge, exact participant acknowledgement set, missing/duplicate/unplanned child registration, registration after seal, stale/duplicate/missing/failed acknowledgement, timeout, abort poisoning, and no next-generation permit before all acknowledgements. Each logical participant test owns multiple fake child targets and proves it emits one acknowledgement only after every required child purge succeeds. Assert `activateNext` rejects a sequence not completed by the same transition service and that epoch, audio origin, and output permit become visible together or not at all.

- [ ] **Step 2: Run RED**

Run `cmake --build out/build/x64-debug --clean-first --target all`. Expected: missing capability, coordinator, node kind, and binder cause compilation failure.

- [ ] **Step 3: Implement authority and transaction core**

Append the binder node kind. Give the compiler-created group one `MediaAvEpochTransitionService` that owns the coordinator and the epoch/output-permit state under one mutex. During default-node registration, the compiler privately takes the capability for the one planner-bound binder node and calls `MediaRuntimeNodeFactory::createPlaybackEpochBinder(node, std::move(capability))`; generic factory callers and execution-context consumers cannot obtain it. `begin()` revokes the old output permit before returning the typed purge. Each aggregate participant purges all child state before returning its one acknowledgement. `activateNextAfter` validates the completed sequence in the same service and publishes epoch, `MediaAudioPlaybackOrigin`, and output permit in one critical section. Core tests use fake child targets; production aggregate owners register in Tasks 4-10.

- [ ] **Step 4: Run GREEN**

Run `cmake --build out/build/x64-debug --clean-first --target all`, `out/build/x64-debug/media_transcode_runtime_tests.exe`, `out/build/x64-debug/media_transcode_node_tests.exe`, and `ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -L deterministic --timeout 60`. Expected: all exit `0`; direct context/group activation no longer compiles in production callers.

- [ ] **Step 5: Review, commit, and push**

Require fresh spec/quality PASS, commit as `feat: coordinate av sync generations`, and push.

---
### Task 4: Carry canonical lineage through ingress and atomic startup release

**Files:**
- Create: `src/internal/graph/sync/MediaCanonicalLineage.h`
- Create: `src/internal/graph/sync/MediaCanonicalLineage.cpp`
- Create: `src/internal/graph/sync/MediaCanonicalVideoFrameBuffer.h`
- Create: `src/internal/graph/sync/MediaCanonicalVideoFrameBuffer.cpp`
- Create: `src/internal/graph/sync/MediaCanonicalAudioSampleInterval.h`
- Create: `src/internal/graph/sync/MediaCanonicalAudioSamplesBuffer.h`
- Create: `src/internal/graph/sync/MediaCanonicalAudioSamplesBuffer.cpp`
- Create: `src/internal/graph/nodes/sync/MediaCanonicalInputNode.h`
- Create: `src/internal/graph/nodes/sync/MediaCanonicalInputNode.cpp`
- Create: `src/internal/graph/nodes/sync/MediaAvBoundReleaseExtractorNode.h`
- Create: `src/internal/graph/nodes/sync/MediaAvBoundReleaseExtractorNode.cpp`
- Create: `src/internal/graph/sync/startup/MediaAvStartupGenerationState.h`
- Create: `src/internal/graph/sync/startup/MediaAvStartupGenerationState.cpp`
- Create: `tests/unit/canonical_lineage_tests.cpp`
- Modify: `src/internal/graph/sync/MediaCanonicalAccessUnitBuffer.h`
- Modify: `src/internal/graph/sync/MediaCanonicalAccessUnitBuffer.cpp`
- Modify: `src/internal/graph/runtime/buffer/MediaAvStartupEnvelopeBuffer.h`
- Modify: `src/internal/graph/runtime/buffer/MediaAvStartupEnvelopeBuffer.cpp`
- Modify: `src/internal/graph/nodes/sync/MediaAvStartupCoordinatorNode.h`
- Modify: `src/internal/graph/nodes/sync/MediaAvStartupCoordinatorNode.cpp`
- Modify: `src/internal/graph/nodes/sync/MediaPlaybackEpochBinderNode.cpp`
- Modify: `src/internal/graph/model/MediaNodeKind.h`
- Modify: `src/internal/graph/runtime/factory/MediaRuntimeNodeFactory.cpp`
- Modify: `tests/unit/av_startup_coordinator_tests.cpp`
- Modify: `tests/unit/test_node.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**

```cpp
struct MediaCanonicalLineage final {
    MediaRunningTime presentation;
    std::optional<MediaRunningTime> decode;
    MediaRunningTime duration;
    MediaDecodeOrderMode decodeOrder;
    std::string sourceIdentity;
    MediaSourceAccessUnitSequence sourceSequence;
    MediaTimeMappingConfidence mappingConfidence;
    std::uint64_t generation;
};

struct MediaCanonicalAudioSampleInterval final {
    std::int64_t begin;
    std::int64_t end;
    int sampleRate;
};

enum class MediaAvStartupReleaseKind : std::uint8_t {
    InitialAtomicRelease = 0,
    ActiveEpochPassThrough = 1
};
```

`MediaCanonicalAccessUnitBuffer` owns one payload plus `std::shared_ptr<const MediaCanonicalLineage>`. Frame/sample wrappers retain the same shared immutable value. `MediaAvStartupReleaseBuffer` gains group key and release kind; its initial release must contain both streams, while active pass-through may contain one.

- [ ] **Step 1: Write failing canonical/startup tests**

Test exact RTP/RTCP and MPEG-TS PCR/PTS/DTS mapping into lineage, source identity, confidence, generation, no arrival-time path, shared lineage pointer identity, no payload copy, duplicate sequence rejection, group mismatch, initial single-stream release rejection, active pass-through acceptance without repeat activation, and extractor backpressure that never loses one side of the initial compound release.

```cpp
const auto canonical = MediaCanonicalAccessUnitBuffer::create(packet, lineage);
EXPECT_TRUE(ctx, canonical);
EXPECT_EQ(ctx, canonical.value()->media().get(), packet.get());
EXPECT_EQ(ctx, canonical.value()->lineage().get(), lineage.get());
```

- [ ] **Step 2: Run RED**

Run `cmake --build out/build/x64-debug --clean-first --target all`. Expected: compilation fails because the shared lineage and canonical input/extractor nodes do not exist.

- [ ] **Step 3: Implement canonical ingress and release routing**

Create one canonical-input node per stream. It consumes only validated protocol clock evidence plus encoded access units, calls the accepted `MediaCanonicalTimeMapper`, and emits `MediaAvStartupEnvelopeBuffer` whose media is a canonical AU. The coordinator delegates generation-local queues/readiness/payload maps to `MediaAvStartupGenerationState`, which implements the planner-required `startup_generation_state` purge target. The coordinator publishes one compound release. Binder activates only `InitialAtomicRelease`; it validates but does not reactivate `ActiveEpochPassThrough`. The extractor stages both sides, preflights both output queues for an initial release, then commits video/audio without dropping either side. No node synthesizes missing timing.

- [ ] **Step 4: Run GREEN**

Run `cmake --build out/build/x64-debug --clean-first --target all`, `out/build/x64-debug/media_transcode_core_tests.exe`, `out/build/x64-debug/media_transcode_node_tests.exe`, `out/build/x64-debug/media_transcode_runtime_tests.exe`, and `ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -L deterministic --timeout 60`. Expected: all exit `0`.

- [ ] **Step 5: Review, commit, and push**

After fresh review, commit as `feat: bind canonical av startup lineage` and push.

---

### Task 5: Preserve video lineage through decode, filter, and encode

**Files:**
- Create: `src/internal/graph/sync/lineage/MediaFfmpegLineageToken.h`
- Create: `src/internal/graph/sync/lineage/MediaCodecLineageRegistry.h`
- Create: `src/internal/graph/sync/lineage/MediaCodecLineageRegistry.cpp`
- Create: `tests/unit/video_codec_lineage_tests.cpp`
- Modify: `src/internal/graph/runtime/ffmpeg/FFmpegPacketView.h`
- Modify: `src/internal/graph/runtime/ffmpeg/FFmpegPacketView.cpp`
- Modify: `src/internal/graph/runtime/ffmpeg/FFmpegFrameView.h`
- Modify: `src/internal/graph/runtime/ffmpeg/FFmpegFrameView.cpp`
- Modify: `src/internal/graph/builder/codec/CodecResolverEncoderContextBuilder.cpp`
- Modify: `src/internal/graph/nodes/video/VideoDecodeNode.h`
- Modify: `src/internal/graph/nodes/video/VideoDecodeNode.cpp`
- Modify: `src/internal/graph/nodes/video/VideoFilterNode.h`
- Modify: `src/internal/graph/nodes/video/VideoFilterNode.cpp`
- Modify: `src/internal/graph/nodes/video/VideoFrameRateNode.h`
- Modify: `src/internal/graph/nodes/video/VideoFrameRateNode.cpp`
- Modify: `src/internal/graph/nodes/video/VideoEncodeNode.h`
- Modify: `src/internal/graph/nodes/video/VideoEncodeNode.cpp`
- Modify: `src/internal/graph/builder/segments/MediaVideoTranscodeBranchBuilder.h`
- Modify: `src/internal/graph/builder/segments/MediaVideoTranscodeBranchBuilder.cpp`
- Modify: `src/internal/graph/builder/segments/MediaVideoTranscodeBranchNodes.h`
- Modify: `tests/unit/test_builder.cpp`
- Modify: `tests/unit/test_realtime_rtp_graph.cpp`
- Modify: `tests/unit/test_node.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**

```cpp
class MediaCodecLineageRegistry final {
public:
    static ::media::Result<MediaCodecLineageRegistry> create(std::size_t capacity);
    ::media::Result<MediaFfmpegLineageToken> submit(
        std::shared_ptr<const MediaCanonicalLineage> lineage);
    ::media::Result<std::shared_ptr<const MediaCanonicalLineage>> resolve(
        MediaFfmpegLineageToken token, std::uint64_t generation);
    ::media::Status finishGeneration(std::uint64_t generation);
    void purge(std::uint64_t generation) noexcept;
};
```

- [ ] **Step 1: Write failing codec-lineage tests**

Use real delayed/reordered codec fixtures where available and deterministic fake codec sequences otherwise. Cover decode reorder, one input to multiple outputs, input with no immediate output, encode delay, multiple encoded packets, duplicate/missing opaque token, capacity exhaustion, generation mismatch, flush residue, filter one-to-one identity, explicit rate-conversion derived sequences, and no `VideoTimestampNode` in a synchronized branch.

- [ ] **Step 2: Run RED**

Run `cmake --build out/build/x64-debug --clean-first --target all`. Expected: missing registry/wrapper support or failing lineage assertions.

- [ ] **Step 3: Implement bounded FFmpeg lineage transfer**

Teach packet/frame views to unwrap canonical wrappers. Enable `AV_CODEC_FLAG_COPY_OPAQUE` only when supported, attach an owned token through `opaque_ref`, and pair it with the bounded registry; never use `AVCodecContext::opaque`, which remains owned by hardware pixel-format selection. Each registry implements `MediaAvGenerationPurgeTarget` and registers its exact planner identity (`video_decode`, `video_filter`, `video_frame_rate`, or `video_encode`) with the canonical-lineage aggregate before it is sealed. Decode emits `MediaCanonicalVideoFrameBuffer`; filters preserve or explicitly derive lineage; encode returns canonical access units. Flush/EOF must resolve all legitimate delayed outputs and fail on leftover or unowned lineage. Synchronized branches omit timestamp-repair nodes rather than disabling them with flags.

- [ ] **Step 4: Run GREEN**

Run `cmake --build out/build/x64-debug --clean-first --target all`, `out/build/x64-debug/media_transcode_node_tests.exe`, `out/build/x64-debug/media_transcode_builder_tests.exe`, `out/build/x64-debug/media_transcode_integration_tests.exe`, and `ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -L deterministic --timeout 60`. Expected: all exit `0` and graph tests prove the exact video topology.

- [ ] **Step 5: Review, commit, and push**

After fresh review, commit as `feat: preserve video canonical lineage` and push.

---

### Task 6: Preserve exact audio sample intervals and apply startup trim once

**Files:**
- Create: `src/internal/graph/nodes/audio/MediaAudioStartupTrimNode.h`
- Create: `src/internal/graph/nodes/audio/MediaAudioStartupTrimNode.cpp`
- Create: `src/internal/graph/sync/lineage/MediaAudioIntervalAccumulator.h`
- Create: `src/internal/graph/sync/lineage/MediaAudioIntervalAccumulator.cpp`
- Create: `tests/unit/audio_startup_trim_node_tests.cpp`
- Create: `tests/unit/audio_codec_lineage_tests.cpp`
- Modify: `src/internal/graph/model/MediaNodeKind.h`
- Modify: `src/internal/graph/runtime/factory/MediaRuntimeNodeFactory.cpp`
- Modify: `src/internal/graph/nodes/audio/AudioDecodeNode.h`
- Modify: `src/internal/graph/nodes/audio/AudioDecodeNode.cpp`
- Modify: `src/internal/graph/nodes/audio/AudioResampleNode.h`
- Modify: `src/internal/graph/nodes/audio/AudioResampleNode.cpp`
- Modify: `src/internal/graph/nodes/audio/AudioEncoderFrameQueue.h`
- Modify: `src/internal/graph/nodes/audio/AudioEncoderFrameQueue.cpp`
- Modify: `src/internal/graph/nodes/audio/AudioEncodeNode.h`
- Modify: `src/internal/graph/nodes/audio/AudioEncodeNode.cpp`
- Modify: `src/internal/graph/builder/segments/MediaAudioEncodeBranchBuilder.h`
- Modify: `src/internal/graph/builder/segments/MediaAudioEncodeBranchBuilder.cpp`
- Modify: `src/internal/graph/builder/segments/MediaAudioEncodeBranchNodes.h`
- Modify: `tests/unit/audio_resample_node_tests.cpp`
- Modify: `tests/unit/test_builder.cpp`
- Modify: `tests/unit/test_node.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**

```cpp
struct MediaAudioIntervalFragment final {
    std::shared_ptr<const MediaCanonicalLineage> lineage;
    MediaCanonicalAudioSampleInterval interval;
};

class MediaAudioIntervalAccumulator final {
public:
    ::media::Status push(MediaAudioIntervalFragment fragment);
    ::media::Result<std::vector<MediaAudioIntervalFragment>> take(int samples);
    ::media::Status finish() const;
};
```

- [ ] **Step 1: Write failing trim/interval tests**

Cover exact `[begin,end)` decode intervals, zero/partial/full-frame trim, rejection above selected `trimLeadingSamples`, trim exactly once, and proof that the first post-trim canonical sample equals `epoch.sourceStart`. Configure the resampler with the active `MediaAudioPlaybackOrigin`, assert its first output interval begins at the explicit `epochOutputSampleIndex`, then cover compensation counts, FIFO aggregation across input intervals, encoder frame-size splitting, delayed packets, EOF tail, non-contiguous intervals, generation changes, and payload ownership. Assert `AudioResampleNode` no longer creates a local timestamp or sample-index authority.

- [ ] **Step 2: Run RED**

Run `cmake --build out/build/x64-debug --clean-first --target all`. Expected: missing trim/accumulator types and failing interval assertions.

- [ ] **Step 3: Implement the audio lineage path**

Append the trim node kind and place the node immediately after audio decode. Decode converts canonical packet duration to exact sample intervals. The trim node alone consumes the release trim count, updates the first interval, and verifies the resulting canonical start equals the active origin's `sourceStart`. Resample initializes its sample counter from the origin's planner-provided `epochOutputSampleIndex` and derives presentation/duration from exact input/output sample counts and the applied compensation window. Audio decode, trim, resample-lineage, and encode accumulators implement purge targets and register the exact planner child identities before aggregate sealing. Upgrade `AudioEncoderFrameQueue` so every FIFO range has interval provenance; each encoded packet receives the exact accumulated output interval. Missing, overlapping, gapped, or leftover intervals are terminal.

- [ ] **Step 4: Run GREEN**

Run `cmake --build out/build/x64-debug --clean-first --target all`, `out/build/x64-debug/media_transcode_node_tests.exe`, `out/build/x64-debug/media_transcode_builder_tests.exe`, and `ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -L deterministic --timeout 60`. Expected: all exit `0`.

- [ ] **Step 5: Review, commit, and push**

After fresh review, commit as `feat: preserve audio sample lineage` and push.

---

### Task 7: Make `emitOnMaster` the sole scheduler pacing deadline

**Files:**
- Modify: `src/internal/graph/sync/MediaScheduledAccessUnit.h`
- Modify: `src/internal/graph/sync/MediaScheduledAccessUnit.cpp`
- Modify: `src/internal/graph/nodes/sync/MediaAvScheduledOutputBuilder.h`
- Modify: `src/internal/graph/nodes/sync/MediaAvScheduledOutputBuilder.cpp`
- Modify: `src/internal/graph/nodes/sync/MediaAvSchedulerHead.h`
- Modify: `src/internal/graph/nodes/sync/MediaAvSchedulerHead.cpp`
- Modify: `src/internal/graph/nodes/sync/MediaAvSchedulerPendingCommit.h`
- Modify: `src/internal/graph/nodes/sync/MediaAvOutputSchedulerNode.h`
- Modify: `src/internal/graph/nodes/sync/MediaAvOutputSchedulerNode.cpp`
- Modify: `tests/unit/av_output_scheduler_tests.cpp`
- Modify: `tests/unit/mpeg_ts_mux_runtime_buffer_tests.cpp`

**Interfaces:**

`MediaScheduledAccessUnitParameters` gains required `MediaRunningTime emitOnMaster` and, for audio, `std::optional<MediaCanonicalAudioSampleInterval> audioInterval`. Construction rejects any unit that violates `emitOnMaster <= dispatchOnMaster <= presentationOnMaster` or has stream-incompatible metadata.

- [ ] **Step 1: Write failing scheduler tests**

Test invalid time triples, reordered video where inter-unit dispatch order differs but every unit satisfies the inequality, global earliest `(emitOnMaster, deterministicStreamTieBreak, sourceSequence)` selection, fair equal-deadline alternation, `waitingUntil(group, emitOnMaster)`, no wait on dispatch/presentation, backpressure without state advance, generation permit rejection, EOF draining, and PSI/PCR/RTCP deadlines using the same group clock.

- [ ] **Step 2: Run RED**

Run `cmake --build out/build/x64-debug --clean-first --target all`. Expected: constructor/test failures because scheduled units lack `emitOnMaster`.

- [ ] **Step 3: Implement emission scheduling**

Compute RTP sender lead and TS transport decode lead only from the complete planner product. Scheduler start accepts an `AwaitingEpoch` group and waits for binder activation rather than requiring a preactivated epoch. Arbitration compares emission time first. `dispatchOnMaster` remains metadata/decode ordering only. A successful downstream push commits controller/scheduler state; a blocked push preserves the selected head and decision exactly.

- [ ] **Step 4: Run GREEN**

Run `cmake --build out/build/x64-debug --clean-first --target all`, `out/build/x64-debug/media_transcode_runtime_tests.exe`, `out/build/x64-debug/media_transcode_node_tests.exe`, and `ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -L deterministic --timeout 60`. Expected: all exit `0` with no busy polling.

- [ ] **Step 5: Review, commit, and push**

After fresh review, commit as `feat: schedule av protocol emission` and push.

---

### Task 8: Close the protocol-commit audio servo control loop without a DAG cycle

**Files:**
- Create: `src/internal/graph/sync/MediaProtocolCommitAcknowledgement.h`
- Create: `src/internal/graph/runtime/buffer/MediaProtocolCommitAcknowledgementBuffer.h`
- Create: `src/internal/graph/runtime/buffer/MediaProtocolCommitAcknowledgementBuffer.cpp`
- Create: `src/internal/graph/sync/MediaAudioCorrectionMailbox.h`
- Create: `src/internal/graph/sync/MediaAudioCorrectionMailbox.cpp`
- Create: `src/internal/graph/sync/MediaAudioCorrectionGenerationState.h`
- Create: `src/internal/graph/sync/MediaAudioCorrectionGenerationState.cpp`
- Create: `src/internal/graph/nodes/sync/MediaAudioPlayheadCommitNode.h`
- Create: `src/internal/graph/nodes/sync/MediaAudioPlayheadCommitNode.cpp`
- Create: `src/internal/graph/nodes/sync/MediaAudioDriftServoNode.h`
- Create: `src/internal/graph/nodes/sync/MediaAudioDriftServoNode.cpp`
- Create: `tests/unit/media_protocol_commit_acknowledgement_tests.cpp`
- Create: `tests/unit/audio_correction_mailbox_tests.cpp`
- Create: `tests/unit/audio_playhead_commit_tests.cpp`
- Modify: `src/internal/graph/model/MediaNodeKind.h`
- Modify: `src/internal/graph/model/MediaPayloadKind.h`
- Modify: `src/internal/graph/runtime/buffer/MediaBuffer.h`
- Modify: `src/internal/graph/runtime/factory/MediaRuntimeNodeFactory.cpp`
- Modify: `src/internal/graph/sync/MediaAudioCorrection.h`
- Modify: `src/internal/graph/sync/MediaAudioCorrectionQuantizer.h`
- Modify: `src/internal/graph/sync/MediaAudioCorrectionQuantizer.cpp`
- Modify: `src/internal/graph/sync/MediaAudioDriftServo.h`
- Modify: `src/internal/graph/sync/MediaAudioDriftServo.cpp`
- Modify: `src/internal/graph/sync/MediaAvSyncGroupRuntime.h`
- Modify: `src/internal/graph/sync/MediaAvSyncGroupRuntime.cpp`
- Modify: `src/internal/graph/nodes/audio/AudioResampleNode.h`
- Modify: `src/internal/graph/nodes/audio/AudioResampleNode.cpp`
- Modify: `src/internal/graph/builder/segments/MediaAudioEncodeBranchBuilder.cpp`
- Modify: `tests/unit/audio_drift_servo_tests.cpp`
- Modify: `tests/unit/audio_resample_node_tests.cpp`
- Modify: `tests/unit/test_node.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**

```cpp
struct MediaProtocolCommitAcknowledgement final {
    MediaAvSyncGroupKey groupKey;
    MediaScheduledStream stream;
    std::uint64_t generation;
    MediaSourceAccessUnitSequence sourceSequence;
    MediaRunningTime presentationOnMaster;
    MediaRunningTime duration;
    MediaRunningTime emitOnMaster;
    MediaRunningTime observedAt;
    std::optional<MediaCanonicalAudioSampleInterval> audioInterval;
};

class MediaAudioCorrectionMailbox final {
public:
    static ::media::Result<MediaAudioCorrectionMailbox> create(
        std::size_t capacity, std::uint64_t generation);
    ::media::Status publish(MediaAudioCompensationCommand command);
    ::media::Result<std::optional<MediaAudioCompensationCommand>> takeFor(
        std::int64_t nextOutputSampleIndex, std::uint64_t generation);
    ::media::Status acknowledge(std::uint64_t sequence,
                                MediaCanonicalAudioSampleInterval actualInterval);
    void purge(std::uint64_t generation) noexcept;
};
```

- [ ] **Step 1: Write failing commit/measurement/mailbox tests**

Test acknowledgement completeness, stream/interval mismatch, monotonic observed time, checked arithmetic, mailbox capacity, exactly-once sequence, generation purge, expiry, late target, skipped command, missing acknowledgement, and current resampler watermark. For a committed interval `[sampleBegin,sampleEnd)`, assert:

```text
sampleEndOnMaster = origin.masterRelease
                  + (sampleEnd - origin.epochOutputSampleIndex) / origin.outputSampleRate
phaseError = (presentationOnMaster + duration) - sampleEndOnMaster
commitLateness = observedAt - emitOnMaster
```

Positive phase error must produce a positive stretch command; commit lateness is recorded as transport telemetry and must not change the servo result.
Use a counting fake group clock: the protocol adapter's post-write capture increments it exactly once, while `MediaAudioPlayheadCommitNode` consumes the acknowledgement's `observedAt` without another clock read. Assert the value is preserved bit-for-bit into the drift measurement.

- [ ] **Step 2: Run RED**

Run `cmake --build out/build/x64-debug --clean-first --target all`. Expected: missing acknowledgement/mailbox/node types and failing future-target tests.

- [ ] **Step 3: Implement the group-owned control plane**

Append playhead/servo node kinds. Group runtime owns one `MediaAudioCorrectionGenerationState`, registered as the sole planner-named `audio_correction_generation_state` child of the AudioCorrection participant. It contains the bounded mailbox, servo state, monotonic `highestProducedSampleIndex`, and pending command/commit acknowledgements; its purge clears all four atomically and returns success only after every internal state is empty. RTP/TS protocol adapters are the only commit-observation authorities and put their immediate post-write group-clock sample into the acknowledgement. The playhead node consumes that `observedAt` unchanged, reads the matching group-owned `MediaAudioPlaybackOrigin`, and never samples the clock again. Quantization schedules `effectiveOutputSampleIndex >= measuredSampleEnd + commandLeadSamples`; publication rechecks it is strictly beyond `highestProducedSampleIndex + maximumResamplerOutputBlockSamples`. `AudioResampleNode` consumes commands from the mailbox before a matching future window, applies once, and acknowledges the actual interval. Remove the correction DAG input/edge completely.

- [ ] **Step 4: Run GREEN and fault matrix**

Run `cmake --build out/build/x64-debug --clean-first --target all`, `out/build/x64-debug/media_transcode_core_tests.exe`, `out/build/x64-debug/media_transcode_runtime_tests.exe`, `out/build/x64-debug/media_transcode_node_tests.exe`, `ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -L deterministic --timeout 60`, and `1..20 | ForEach-Object { & out/build/x64-debug/media_transcode_runtime_tests.exe; if ($LASTEXITCODE) { throw "runtime iteration $_ failed" } }`. Expected: every run exits `0`; no cycle, spin, late normal command, or lost acknowledgement.

- [ ] **Step 5: Review, commit, and push**

After fresh review, commit as `feat: close audio drift correction loop` and push.

---

### Task 9: Add project MPEG-TS runtime adapters and per-access-unit commit acknowledgement

**Files:**
- Create: `src/internal/graph/nodes/mux/MediaTsRuntimePlanSourceNode.h`
- Create: `src/internal/graph/nodes/mux/MediaTsRuntimePlanSourceNode.cpp`
- Create: `src/internal/graph/nodes/mux/MediaScheduledToTsAccessUnitNode.h`
- Create: `src/internal/graph/nodes/mux/MediaScheduledToTsAccessUnitNode.cpp`
- Modify: `src/internal/graph/nodes/mux/MediaMuxSession.h`
- Modify: `src/internal/graph/nodes/mux/ProjectMpegTsMuxSessionAdapter.h`
- Modify: `src/internal/graph/nodes/mux/ProjectMpegTsMuxSessionAdapter.cpp`
- Modify: `src/internal/graph/nodes/mux/FileMuxNode.h`
- Modify: `src/internal/graph/nodes/mux/FileMuxNode.cpp`
- Modify: `src/internal/graph/protocol/mpegts/MediaTsMuxSession.h`
- Modify: `src/internal/graph/protocol/mpegts/MediaTsMuxSession.cpp`
- Modify: `src/internal/graph/runtime/buffer/MediaTsAccessUnitBuffer.h`
- Modify: `src/internal/graph/runtime/buffer/MediaTsAccessUnitBuffer.cpp`
- Modify: `src/internal/graph/protocol/mpegts/MediaTsAccessUnitView.h`
- Modify: `src/internal/graph/builder/segments/MediaOutputSegmentBuilder.h`
- Modify: `src/internal/graph/builder/segments/MediaOutputSegmentBuilder.cpp`
- Modify: `src/internal/graph/model/MediaNodeKind.h`
- Modify: `src/internal/graph/runtime/factory/MediaRuntimeNodeFactory.cpp`
- Modify: `tests/unit/mpeg_ts_output_session_tests.cpp`
- Modify: `tests/unit/project_mpeg_ts_mux_session_adapter_tests.cpp`
- Modify: `tests/unit/project_mpeg_ts_file_mux_node_tests.cpp`
- Modify: `tests/unit/mpeg_ts_mux_runtime_buffer_tests.cpp`

**Interfaces:**

`MediaTsAccessUnitBuffer` gains required source sequence, canonical duration, and optional audio sample interval. `MediaMuxSession::write` returns a typed result that distinguishes committed access-unit metadata from protocol-only deadline advancement; non-project mux sessions return no synchronized acknowledgement.

```cpp
struct MediaMuxSessionWriteResult final {
    std::optional<MediaProtocolCommitAcknowledgement> acknowledgement;
};

class MediaMuxSession {
public:
    virtual ::media::Result<MediaMuxSessionWriteResult> write(
        MediaGraphExecutionContext& context,
        const MediaBufferRef& buffer) = 0;
};
```

- [ ] **Step 1: Write failing TS commit tests**

Test one runtime plan per active generation, adapter validation without policy selection, video/audio conversion, source sequence and interval preservation, exact transport lead, complete TS packet/datagram writes, one acknowledgement per AU, no acknowledgement on short/partial/failing sink write, shared-mux stream identity, PCR/PSI-only advance without fake AU acknowledgement, and generation purge.

- [ ] **Step 2: Run RED**

Run `cmake --build out/build/x64-debug --clean-first --target all`. Expected: missing adapter nodes and commit result contract.

- [ ] **Step 3: Implement typed TS production boundary**

Runtime-plan source combines the immutable `MediaTsMuxPlan` with the active playback epoch once per generation. Scheduled adapters convert both scheduler ports to typed TS AUs. `MediaTsPacketBatchWriter` remains the byte-commit authority; only after every byte-sink write for one AU succeeds does the project adapter read the group master clock once, create the complete acknowledgement including `observedAt`, and return it. `FileMuxNode` publishes that acknowledgement unchanged on a typed side output even though both streams share one mux. Keep `ByteSink + ProjectMpegTs` mandatory and do not add an FFmpeg TS fallback.

- [ ] **Step 4: Run GREEN and decode integration**

Run `cmake --build out/build/x64-debug --clean-first --target all`, `out/build/x64-debug/media_transcode_node_tests.exe`, `out/build/x64-debug/media_transcode_mpeg_ts_integration_tests.exe`, `out/build/x64-debug/media_transcode_mpeg_ts_output_decode_integration_tests.exe`, `ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -L deterministic --timeout 60`, and `ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -L integration --timeout 180`. Expected: all supported tests exit `0`; optional environment absence is an explicit `77` skip.

- [ ] **Step 5: Review, commit, and push**

After fresh review, commit as `feat: acknowledge project MPEG-TS commits` and push.

---

### Task 10: Add the scheduled RTP/RTCP output node and typed SDP publication

**Files:**
- Create: `src/internal/graph/runtime/buffer/MediaScheduledRtpRuntimePlanBuffer.h`
- Create: `src/internal/graph/runtime/buffer/MediaScheduledRtpRuntimePlanBuffer.cpp`
- Create: `src/internal/graph/nodes/output/MediaScheduledRtpRuntimePlanSourceNode.h`
- Create: `src/internal/graph/nodes/output/MediaScheduledRtpRuntimePlanSourceNode.cpp`
- Create: `src/internal/graph/nodes/output/MediaScheduledRtpOutputNode.h`
- Create: `src/internal/graph/nodes/output/MediaScheduledRtpOutputNode.cpp`
- Create: `src/internal/graph/nodes/output/MediaTypedRtpSdpWriterNode.h`
- Create: `src/internal/graph/nodes/output/MediaTypedRtpSdpWriterNode.cpp`
- Create: `tests/unit/scheduled_rtp_output_node_tests.cpp`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.cpp`
- Modify: `src/internal/graph/nodes/mux/ScheduledRtpSenderSession.h`
- Modify: `src/internal/graph/nodes/mux/ScheduledRtpSenderSession.cpp`
- Modify: `src/internal/graph/nodes/mux/ScheduledRtpSenderConfig.h`
- Modify: `src/internal/graph/nodes/mux/ScheduledRtpSenderConfig.cpp`
- Modify: `src/internal/graph/model/MediaNodeKind.h`
- Modify: `src/internal/graph/runtime/factory/MediaRuntimeNodeFactory.cpp`
- Modify: `tests/unit/scheduled_rtp_sender_tests.cpp`
- Modify: `tests/unit/media_rtp_sdp_description_tests.cpp`
- Modify: `tests/unit/media_atomic_utf8_file_publisher_tests.cpp`
- Modify: `tests/unit/media_rtp_udp_sender_composition_tests.cpp`
- Modify: `tests/unit/media_rtp_udp_sender_loopback_tests.cpp`
- Modify: `tests/unit/media_rtp_udp_sender_ipv6_loopback_tests.cpp`
- Modify: `tests/unit/test_node.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**

Consumes the exact `MediaSeparateRtpOutputRuntimePlan` defined by Task 1. The runtime-plan source combines each nested sender plan with the group-owned `MediaSharedNtpEpoch` and publishes one immutable buffer per generation; SDP consumes the planner identity/codec/CNAME only and never the NTP epoch.

```cpp
static ::media::Result<ScheduledRtpSenderConfig> create(
    ScheduledRtpMuxStreamConfig stream,
    MediaRtpUdpSenderConfig transport,
    std::shared_ptr<const MediaSharedNtpEpoch> ntpEpoch,
    std::uint32_t ssrc,
    std::uint32_t baseTimestamp,
    std::string cname,
    MediaRunningTime senderReportInterval);

const std::shared_ptr<const MediaSharedNtpEpoch>& ntpEpoch() const noexcept;
```

`ScheduledRtpSenderConfig` and `ScheduledRtpSenderSession` retain this shared immutable pointer. Tests compare `.get()` for video/audio configs and sessions; value-equal independent objects are rejected by the runtime-plan source.

- [ ] **Step 1: Write failing RTP transaction tests**

Cover separate video/audio endpoints, distinct SSRC/base timestamps, common CNAME and NTP epoch, H.264/AAC packetization, complete access-unit datagram batch acknowledgement, no acknowledgement on any datagram failure, periodic SR, RTP-to-SR mapping, next RTCP deadline as `waitingUntil`, stop/abort RAII, and typed SDP byte-for-byte consistency with both sender plans. Prove VLC-openable SDP contains both media sections and no FFmpeg format-context dependency.

- [ ] **Step 2: Run RED**

Run `cmake --build out/build/x64-debug --clean-first --target all`. Expected: missing scheduled output/typed SDP node types.

- [ ] **Step 3: Implement by composition, not rewrite**

Instantiate one node per stream. Compose the accepted `ScheduledRtpSenderSession`, `ScheduledRtpMuxFfmpegSessionFactory`, UDP sender transport, RTP clock mapper, and RTCP generator. Extend the sender transaction boundary only enough to report success after the full access-unit datagram batch; the node then stamps one `MediaProtocolCommitAcknowledgement` with group-clock `observedAt`. Use `MediaRtpSdpDescription`, `MediaRtpSdpSerializer`, and `MediaAtomicUtf8FilePublisher`; do not use `av_sdp_create` or `std::ofstream` replacement.

- [ ] **Step 4: Run GREEN and integration tests**

Run `cmake --build out/build/x64-debug --clean-first --target all`, `out/build/x64-debug/media_transcode_node_tests.exe`, `out/build/x64-debug/media_transcode_rtp_sdp_description_tests.exe`, `out/build/x64-debug/media_transcode_rtp_udp_sender_loopback_tests.exe`, `out/build/x64-debug/media_transcode_rtp_udp_sender_ipv6_loopback_tests.exe`, `ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -L deterministic --timeout 60`, and `ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -L integration --timeout 180`. Unsupported IPv6 cases may return the existing skip code `77` with an explicit reason; no false pass.

- [ ] **Step 5: Review, commit, and push**

After fresh review, commit as `feat: wire scheduled rtp output` and push.

---

### Task 11: Assemble both production graphs and remove obsolete synchronized authorities

**Files:**
- Create: `src/internal/graph/builder/realtime/MediaRealtimeAvSyncInputSegmentBuilder.h`
- Create: `src/internal/graph/builder/realtime/MediaRealtimeAvSyncInputSegmentBuilder.cpp`
- Create: `src/internal/graph/builder/realtime/MediaRealtimeAvSyncProcessingSegmentBuilder.h`
- Create: `src/internal/graph/builder/realtime/MediaRealtimeAvSyncProcessingSegmentBuilder.cpp`
- Create: `src/internal/graph/builder/realtime/MediaRealtimeScheduledRtpOutputSegmentBuilder.h`
- Create: `src/internal/graph/builder/realtime/MediaRealtimeScheduledRtpOutputSegmentBuilder.cpp`
- Create: `src/internal/graph/builder/realtime/MediaRealtimeProjectMpegTsOutputSegmentBuilder.h`
- Create: `src/internal/graph/builder/realtime/MediaRealtimeProjectMpegTsOutputSegmentBuilder.cpp`
- Modify: `src/internal/graph/builder/realtime/MediaRealtimeRtpTranscodeGraphBuilder.h`
- Modify: `src/internal/graph/builder/realtime/MediaRealtimeRtpTranscodeGraphBuilder.cpp`
- Modify: `src/internal/graph/builder/realtime/MediaRealtimeOptionApplier.h`
- Modify: `src/internal/graph/builder/realtime/MediaRealtimeOptionApplier.cpp`
- Modify: `src/internal/graph/builder/segments/MediaVideoBranchSegmentBuilder.h`
- Modify: `src/internal/graph/builder/segments/MediaVideoBranchSegmentBuilder.cpp`
- Modify: `src/internal/graph/builder/segments/MediaAudioBranchSegmentBuilder.h`
- Modify: `src/internal/graph/builder/segments/MediaAudioBranchSegmentBuilder.cpp`
- Modify: `src/internal/graph/runtime/compilation/MediaGraphRuntimeCompiler.cpp`
- Modify: `src/internal/graph/runtime/lifecycle/MediaGraphRuntimeLifecycleExecutor.cpp`
- Modify: `src/internal/graph/nodes/packet/PacketStartGateNode.cpp`
- Modify: `src/internal/graph/nodes/packet/PacketNormalizeNode.cpp`
- Modify: `src/internal/graph/nodes/video/VideoTimestampNode.cpp`
- Modify: `src/internal/graph/nodes/mux/RtpMuxNode.cpp`
- Modify: `src/internal/graph/nodes/mux/RtpMuxFfmpegSession.cpp`
- Modify: `src/internal/graph/runtime/ffmpeg/FFmpegPacedAvio.cpp`
- Delete if no legal caller remains: `src/internal/graph/nodes/packet/AvPacketStartBarrierNode.h`
- Delete if no legal caller remains: `src/internal/graph/nodes/packet/AvPacketStartBarrierNode.cpp`
- Delete if no legal caller remains: `src/internal/graph/runtime/MediaGraphPacingClock.h`
- Delete if no legal caller remains: `src/internal/graph/runtime/MediaGraphPacingClock.cpp`
- Modify: `src/internal/graph/model/MediaNodeKind.h`
- Modify: `src/internal/graph/core/MediaGraphDump.cpp`
- Modify: `src/internal/graph/diagnostics/MediaGraphDiagnostics.cpp`
- Modify: `src/internal/graph/runtime/factory/MediaRuntimeNodeFactory.cpp`
- Modify: `tests/unit/test_builder.cpp`
- Modify: `tests/unit/test_realtime_rtp_graph.cpp`
- Modify: `tests/unit/event_runtime_ffmpeg_ownership_tests.cpp`
- Modify: `tests/unit/event_runtime_multi_input_tests.cpp`
- Modify: `tests/unit/event_runtime_threading_queue_tests.cpp`
- Modify: `tests/performance/test_performance.cpp`

**Interfaces:**
- Input segment produces canonical video/audio envelopes plus typed clock readiness from RTP clock-group evidence or the selected MPEG-TS program clock.
- Processing segment owns coordinator, binder, release extractor, decode/filter/resample/encode lineage, shared scheduler, playhead/servo control nodes, and transition participants.
- Each output segment consumes the scheduler's explicit video/audio ports and a complete typed runtime plan; no output segment reads request strings or chooses policy.

- [ ] **Step 1: Write failing exact-topology and lifecycle tests**

For separate RTP assert the exact spine:

```text
RawRtpInput(video/audio) -> RtpClockGroup -> CanonicalInput(video/audio)
  -> AvStartupCoordinator -> PlaybackEpochBinder -> BoundReleaseExtractor
  -> video decode/filter/encode + audio decode/trim/resample/encode
  -> one AvOutputScheduler
  -> ScheduledRtpOutput(video/audio) -> ProtocolCommitAck(audio)
  -> AudioPlayheadCommit -> AudioDriftServo control mailbox
```

For MPEG-TS assert `RealtimeInput -> MpegTsDemux -> canonical/startup/processing -> one scheduler -> scheduled-to-TS adapters -> one FileMux(ProjectMpegTs)`. Assert synchronized graphs contain no old barrier, packet/timestamp repair, `RtpMux`, independent pacing, correction back-edge, or mixed generation path. Assert video-only still contains `PacketStartGate` and its valid legacy RTP output components.

Add deterministic lifecycle tests for first EOF draining, both EOF completion, terminal drain timeout, abort, queue pressure, SR/PCR interruption, lineage residue, transition purge acknowledgements from every production participant, revoked output permit, and no progress stall/busy loop.

- [ ] **Step 2: Run RED**

Run `cmake --build out/build/x64-debug --clean-first --target all`. Expected: graph topology tests fail because the production builder still creates `AvPacketStartBarrier`/`RtpMux` and bypasses synchronization nodes.

- [ ] **Step 3: Split and wire the production builder**

Refactor the current multi-responsibility realtime builder by responsibility into the four focused builders above. Branch builders return explicit codec/packet endpoints instead of requiring a mux destination. Put the identical planner group key on every synchronized node and put the same immutable plan in `MediaRealtimeExecutableGraph::avSyncBinding`; compiler rejects mismatches before workers start.

Connect generation participants to the group transaction coordinator. During runtime registration, register every planner-named child purge target, reject missing/extra/duplicate identities, and seal all aggregate groups before workers start. On transition, each aggregate clears all old queued media, pending lineage, corrections/acks, scheduler heads/commits/deadlines, RTP sessions, or TS generation state before acknowledging the exact transition sequence. First clean EOF enters group draining; finish requires both EOFs and empty lineage/scheduler/protocol/control state inside the planner drain window. Abort closes both streams once.

Remove obsolete synchronized fields from `MediaRealtimeRtpTranscodePlan` and their option keys. Delete old classes only when repository-wide search proves no legal local/video-only caller; otherwise leave them physically present but make synchronized builders unable to reference them. Preserve enum values as reserved.

- [ ] **Step 4: Run full GREEN and repeat fault tests**

Run:

```powershell
cmake --build out/build/x64-debug --clean-first --target all
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -L deterministic --timeout 60
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -L integration --timeout 180
1..20 | ForEach-Object { & out/build/x64-debug/media_transcode_runtime_tests.exe; if ($LASTEXITCODE) { throw "runtime iteration $_ failed" } }
```

Expected: every command exits `0`; integration-only unsupported environments report skip `77` with a reason.

- [ ] **Step 5: Global architecture review, commit, and push**

Search all `src/internal/graph` callers for duplicated planner decisions, fallback/default values, timestamp repair, semantic duplicates, raw ownership, unnecessary payload copies, oversized files, and old synchronized authorities. Fix every Critical/Important finding and repeat tests until fresh reviewers return PASS. Commit as `refactor: replace legacy av pacing path` and push.

---

### Task 12: Add deterministic hardware acceptance, evidence, score, and delivery

**Files:**
- Create: `tests/acceptance/new_av_sync_fixture.ps1`
- Create: `tests/acceptance/prepare_runtime_dependencies.ps1`
- Create: `tests/acceptance/run_av_sync_gate.ps1`
- Create: `tests/acceptance/inspect_rtp_sync.ps1`
- Create: `tests/acceptance/inspect_mpegts_sync.ps1`
- Modify: `tests/acceptance/Priority5Acceptance.psm1`
- Modify: `tests/acceptance/test_priority5_reporting.ps1`
- Modify: `tests/acceptance/priority5_scenarios.json`
- Modify: `tests/acceptance/run_priority4_realtime_gate.ps1`
- Modify: `tools/realtime_video_cli/main.cpp`
- Modify: `src/internal/graph/runtime/diagnostics/MediaGraphRuntimeReport.h`
- Modify: `src/internal/graph/runtime/diagnostics/MediaGraphRuntimeReport.cpp`
- Modify: `tests/performance/test_performance.cpp`
- Modify: `plan.md`
- Modify: `docs/av_sync_system_design.md`
- Create: `docs/completed/quality_priority_5_av_sync.md`
- Modify: `QUALITY_SCORE.md`

**Interfaces:**

```powershell
param(
    [ValidateSet('SeparateRtp','MpegTs')][string]$Scenario,
    [int]$DurationSeconds = 120,
    [string]$BuildDirectory = 'out/build/x64-debug',
    [string]$FfmpegBin = 'D:\mabs\local64\bin-video',
    [string]$VlcPath = 'D:\VideoLAN\VLC\vlc.exe'
)
```

The gate returns nonzero on any automated or subjective failure and writes one structured report containing commands as executed, input checksum/manifest, CPU/memory/thread/queue/latency metrics, matched events, skew distribution, fitted drift slope/confidence/residuals, and RTP/TS protocol evidence.

- [ ] **Step 1: Write failing PowerShell contract tests**

Extend `test_priority5_reporting.ps1` to reject wrong process names, missing logical-core normalization, duration below 120, ordinary `test.mp4` as sync ground truth, fewer than 50 matched events, missing confidence/residual data, capture-remux playback, missing common RTP CNAME/shared NTP/SR mapping, missing TS PCR/continuity evidence, software fallback, hidden skip, absent VLC result, or dependency copying configured in CMake.

- [ ] **Step 2: Generate and validate the deterministic fixture**

`new_av_sync_fixture.ps1` creates a 120-second 1280x720 30 fps H.264 plus 48 kHz AAC source outside CMake. A one-frame full-field flash and short broadband beep occur together at 500 ms and every two seconds thereafter. Write 60 expected timestamps to JSON and record SHA-256. Reuse the exact fixture and manifest for RTP and TS input senders; a checksum mismatch blocks the run. Generate and immediately validate it with:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/acceptance/new_av_sync_fixture.ps1 -OutputPath out/build/x64-debug/av_sync_fixture.mp4 -ManifestPath out/build/x64-debug/av_sync_fixture.json -FfmpegBin D:\mabs\local64\bin-video
powershell -NoProfile -ExecutionPolicy Bypass -File tests/acceptance/new_av_sync_fixture.ps1 -ValidateOnly -OutputPath out/build/x64-debug/av_sync_fixture.mp4 -ManifestPath out/build/x64-debug/av_sync_fixture.json -FfmpegBin D:\mabs\local64\bin-video
```

- [ ] **Step 3: Implement direct protocol inspection and process-scoped metrics**

The automated RTP phase captures raw RTP/RTCP packets without remuxing, verifies distinct SSRCs, common CNAME, shared NTP epoch, periodic SR, and RTP/SR mapping, then decodes the two elementary streams only for flash/beep event extraction. The TS phase parses program/PIDs, continuity, PCR cadence/jitter, and PTS/DTS/PCR before decoding events. A separate subjective phase opens the generated SDP directly in VLC for RTP and the TS output directly for MPEG-TS; no capture-remux artifact is used for playback.

Resolve the process by exact executable identity and compute each sample as:

```powershell
$cpu = 100.0 * ($current.TotalProcessorTime.TotalSeconds - $previous.TotalProcessorTime.TotalSeconds) /
    (($now - $then).TotalSeconds * [Environment]::ProcessorCount)
```

Aggregate time-weighted average and P95 for only `media_transcode_realtime_video_cli`. Sender, inspector, FFmpeg, VLC, and unrelated processes are excluded.

- [ ] **Step 4: Fully rebuild and stage runtime dependencies outside CMake**

Run `cmake --build out/build/x64-debug --clean-first --target all`. `prepare_runtime_dependencies.ps1` recursively resolves PE imports for the named CLI/test executables, copies only missing non-system DLLs from the supplied FFmpeg directory beside those executables, and fails on an unresolved import. Run exactly:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/acceptance/prepare_runtime_dependencies.ps1 -BuildDirectory out/build/x64-debug -FfmpegBin D:\mabs\local64\bin-video -Executables media_transcode_realtime_video_cli.exe,media_transcode_hardware_tests.exe,media_transcode_performance_tests.exe
```

Do not edit CMake for runtime environment preparation.

- [ ] **Step 5: Run all automated tiers**

Run exactly:

```powershell
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -L deterministic --timeout 60
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -L integration --timeout 180
out/build/x64-debug/media_transcode_hardware_tests.exe
out/build/x64-debug/media_transcode_performance_tests.exe
powershell -NoProfile -ExecutionPolicy Bypass -File tests/acceptance/test_priority5_reporting.ps1
1..20 | ForEach-Object { & out/build/x64-debug/media_transcode_runtime_tests.exe; if ($LASTEXITCODE) { throw "runtime iteration $_ failed" } }
```

Expected: every command exits `0`. NVENC absence is a blocker for final acceptance, not a software fallback.

- [ ] **Step 6: Run the two hardware gates sequentially**

With no competing integration/acceptance workload, run exactly:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/acceptance/run_av_sync_gate.ps1 -Scenario SeparateRtp -DurationSeconds 120 -BuildDirectory out/build/x64-debug -FfmpegBin D:\mabs\local64\bin-video -VlcPath D:\VideoLAN\VLC\vlc.exe
powershell -NoProfile -ExecutionPolicy Bypass -File tests/acceptance/run_av_sync_gate.ps1 -Scenario MpegTs -DurationSeconds 120 -BuildDirectory out/build/x64-debug -FfmpegBin D:\mabs\local64\bin-video -VlcPath D:\VideoLAN\VLC\vlc.exe
```

Require exit `0`, complete A/V, zero decode/worker/drop/continuity/continuous-frame errors, no progress stall above five seconds, startup offset at most 40 ms, steady P95 at most 20 ms, P99 at most 40 ms, final absolute drift at most 100 ms, fitted drift slope at most 1 ms/hour, at least 50 event pairs, and hardware average CPU at most 5%. An inconclusive confidence interval fails.

- [ ] **Step 7: Obtain user playback confirmation**

Play the generated SDP and TS directly in VLC for the user's observation. Record picture continuity, normal audio, no stutter, no perceptible startup offset, and no perceptible drift. Do not substitute a `.ts` capture when the requested check is separate RTP.

- [ ] **Step 8: Update evidence and quality score**

Record actual test commands/results (not build commands) in the completion document, update `plan.md`, keep the existing quality dimensions/weights, rescore `QUALITY_SCORE.md` from evidence without preselecting a score, and list remaining risks/optimization items. Re-run every documented PowerShell test command verbatim.

- [ ] **Step 9: Final independent review and PR delivery**

Commit as `docs: complete priority 5 av sync evidence`, push the same branch, update PR #21, and dispatch a fresh whole-branch reviewer. Fix, clean-build, retest, push, and re-review until Critical/Important/Minor are all zero and the verdict is PASS.

---

## Plan Self-Review Checklist

- [x] Every production-sync design invariant maps to one task and one test gate.
- [x] Every type first appears under a `Create` entry before later tasks consume it.
- [x] Task 1-11 baseline types are reused and not duplicated.
- [x] Planner remains the only policy authority; builders and nodes have no defaults/fallbacks.
- [x] RTP and TS both produce identical typed audio commit acknowledgements.
- [x] Audio correction is group-owned control state, not a DAG back-edge.
- [x] Generation change revokes output before purge and activates next epoch only after all acknowledgements.
- [x] Only `emitOnMaster` can block for media pacing.
- [x] Video-only and local paths remain isolated and functional.
- [x] Final commands measure only the realtime CLI and use hardware encoding.
- [x] No placeholders, stale interface names, mixed line endings, or unrelated staged files remain.
