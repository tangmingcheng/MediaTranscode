# A/V Sync Production Assembly Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` to implement this plan task-by-task, and use `superpowers:test-driven-development`, `superpowers:systematic-debugging`, and `superpowers:verification-before-completion` at their required checkpoints.

**Goal:** Replace the legacy synchronized RTP and MPEG-TS execution paths with one production A/V synchronization assembly, complete the initial locked generation, and open the real separate-RTP and MPEG-TS outputs directly in VLC for human verification.

**Architecture:** Planner-owned immutable assembly plans select every clock, generation, duration, queue, codec, pacing, and protocol parameter. Protocol-specific input binders convert clock evidence into canonical running time; one startup coordinator activates one playback epoch; one scheduler is the only output pacing authority; typed RTP or project MPEG-TS adapters commit scheduled access units. Task 12 fails closed on degradation, discontinuity, and reacquisition requests. Task 13 will add automatic cross-generation recovery.

**Tech Stack:** C++20, FFmpeg codec/filter/resampler APIs, project-owned RTP/RTCP and MPEG-TS protocol components, event-driven graph runtime, CMake/CTest, PowerShell, CUDA/NVENC, FFmpeg tools, and VLC on Windows.

**Approved scope:**

- Separate RTP video/audio input -> separate RTP video/audio output.
- MPEG-TS input -> MPEG-TS output.
- Hardware transcode acceptance only.
- No separate RTP input -> MPEG-TS output.
- No software acceptance in this round.
- No automatic generation reacquisition in Task 12.
- No FFmpeg mux or arrival-time fallback in synchronized graphs.
- This plan replaces the production-assembly portion of Task 11 in
  `2026-07-15-av-sync-production-wiring.md`; the older Task 12 remains useful
  only for its final acceptance intent.
- Production Task 12 must not call `beginEpochReacquisition()`, register purge
  participants, publish a next generation, or call `activateNext()`. Those
  existing Tasks 1-11 primitives remain disconnected until Task 13.

## Invariants and completion gate

1. Only planners decide policy; builders copy complete plans and runtime nodes only validate/execute them.
2. One protocol clock authority produces one initial generation and one source epoch for both streams.
3. `coordinator.release -> playback_epoch_binder -> bound_release_extractor` is a strict serial happens-before chain.
4. One `MediaAvOutputSchedulerNode` is the only pacing authority for both streams.
5. Synchronized RTP graphs contain no `AvPacketStartBarrier`, `RtpMux`, `RtpOutput`, timestamp repair, or per-mux pacing.
6. Synchronized MPEG-TS graphs use `ByteSink + ProjectMpegTs`; they contain no FFmpeg TS fallback.
7. Missing planner facts fail before execution. Missing clock evidence waits within planned capacities. Degraded, discontinuous, mismatched, or reacquire evidence terminates with a structured A/V sync error.
8. Production support is not declared until both supported topologies build, compile, register, run, and produce directly playable output.
9. VLC human testing starts only after the clean deterministic/integration gate passes on the new topology.

---

### Task 1: Add the planner-owned production assembly contract

**Files:**

- Create: `src/internal/graph/planner/realtime/MediaRealtimeAvSyncAssemblyPlan.h`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlanner.cpp`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlanValidator.cpp`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.cpp`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeAvSyncPlanningFacts.h`
- Create: `tests/unit/av_sync_production_plan_tests.cpp`
- Modify: `tests/unit/test_planner.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**

```cpp
enum class MediaInitialGenerationPolicy {
    FirstLockedOnlyFailOnChange
};

enum class MediaClockEvidencePolicy {
    RequireLockedFailOnDegradedOrReacquire
};

enum class MediaTerminalDurationPolicy {
    RepeatLastObservedPositiveDelta
};

enum class MediaRtpCommonEpochPolicy {
    EarliestLockedSenderReportSourceTime
};

struct MediaRtpInputClockAssemblyPlan final {
    MediaRtpCommonEpochPolicy commonEpochPolicy;
};

struct MediaMpegTsInputClockAssemblyPlan final {
};

using MediaAvSyncInputClockPlan =
    std::variant<MediaRtpInputClockAssemblyPlan,
                 MediaMpegTsInputClockAssemblyPlan>;

struct MediaRtpTimestampDeltaDurationPlan final {
    int clockRate;
    MediaTerminalDurationPolicy terminalPolicy;
};

struct MediaPacketDurationPlan final {
    bool requirePositiveDuration;
};

struct MediaPlannedAudioSamplesDurationPlan final {
    int sampleRate;
    std::uint32_t samplesPerAccessUnit;
};

using MediaCanonicalVideoDurationPlan =
    std::variant<MediaRtpTimestampDeltaDurationPlan,
                 MediaPacketDurationPlan>;

using MediaCanonicalAudioDurationPlan =
    std::variant<MediaPacketDurationPlan,
                 MediaPlannedAudioSamplesDurationPlan>;

struct MediaCanonicalVideoAssemblyPlan final {
    std::string sourceIdentity;
    MediaCanonicalVideoDurationPlan duration;
    MediaDecodeOrderMode decodeOrder;
    std::size_t acquiringCapacity;
    MediaRunningTime acquiringTimeout;
};

struct MediaCanonicalAudioAssemblyPlan final {
    std::string sourceIdentity;
    MediaCanonicalAudioDurationPlan duration;
    MediaDecodeOrderMode decodeOrder;
    std::size_t acquiringCapacity;
    MediaRunningTime acquiringTimeout;
};

struct MediaRealtimeAvSyncAssemblyPlan final {
    MediaAvSyncInputClockPlan inputClock;
    MediaInitialGenerationPolicy generationPolicy;
    MediaClockEvidencePolicy evidencePolicy;
    MediaCanonicalVideoAssemblyPlan video;
    MediaCanonicalAudioAssemblyPlan audio;
    MediaRunningTime startupClockInterval;
};
```

The concrete types must reuse existing stream/decode-order types where they
already express the same semantics. `MediaRealtimeAvSyncAssemblyPlan` is one
field of the existing `MediaRealtimeAvSyncRuntimePlan`; it is not a second
top-level product and `MediaRealtimeRtpTranscodePlan` keeps only its existing
`avSyncRuntime`. For separate RTP H.264/HEVC video, the planner selects
`MediaRtpTimestampDeltaDurationPlan`: the binder holds one AU of lookahead and
derives duration from adjacent extended RTP timestamps. EOF may reuse only the
last observed positive delta because the planner explicitly selected that
terminal policy; absence of any positive delta is terminal. RTP AAC/Opus uses
the exact planned samples-per-access-unit already resolved by packetization.
For MPEG-TS, preflight must prove the selected streams provide positive packet
duration and the planner selects `MediaPacketDurationPlan`; a zero runtime
duration fails closed. Audio sample span uses exact checked
`duration * sampleRate` conversion. No 30-fps constant, arrival time, codec PTS
repair, or implicit previous-duration fallback is introduced downstream.

- [ ] **Step 1: Write RED planner tests**

Cover complete separate-RTP and MPEG-TS products; absent source identity,
invalid duration plan, zero RTP clock rate, impossible terminal delta, video
or audio carrying invalid values in their stream-specific duration variants,
absent
acquisition capacity/timeout, inconsistent
adapter, separate-RTP-to-TS, missing synchronized audio, and any policy other
than first-Locked/fail-on-change. Assert the runtime product contains every
field consumed by later nodes.

- [ ] **Step 2: Run RED after a full rebuild**

Run `cmake --build out/build/x64-debug --clean-first --target all` and the planner target. Expected: missing assembly types/tests fail.

- [ ] **Step 3: Implement one assembly planner and validator**

Resolve the assembly field once inside the existing realtime A/V-sync runtime
planner and validate it in the existing runtime-plan validator. Do not create a
parallel assembly planner, recompute duration in builders, or attach another
optional plan to the outer transcode product.

- [ ] **Step 4: Run GREEN and review planner authority**

Run the planner target and deterministic label. Search synchronized builder/runtime code for newly introduced endpoint, duration, generation, or pacing constants; there must be none.

- [ ] **Step 5: Commit and push**

Commit as `feat: plan av sync production assembly` and push the current branch.

---

### Task 2: Bind RTP access units to the locked common clock

**Files:**

- Modify: `src/internal/graph/protocol/rtp/MediaRtpClockGroupValidator.h`
- Modify: `src/internal/graph/protocol/rtp/MediaRtpClockGroupValidator.cpp`
- Modify: `src/internal/graph/runtime/buffer/MediaRtpClockGroupBuffer.h`
- Modify: `src/internal/graph/nodes/sync/MediaRtpClockGroupNode.cpp`
- Create: `src/internal/graph/protocol/rtp/MediaRtpPacketClockProjector.h`
- Create: `src/internal/graph/protocol/rtp/MediaRtpPacketClockProjector.cpp`
- Create: `src/internal/graph/nodes/sync/MediaRtpPacketClockBinderNode.h`
- Create: `src/internal/graph/nodes/sync/MediaRtpPacketClockBinderNode.cpp`
- Create: `src/internal/graph/nodes/sync/MediaRtpClockSnapshotFanoutNode.h`
- Create: `src/internal/graph/nodes/sync/MediaRtpClockSnapshotFanoutNode.cpp`
- Modify: `src/internal/graph/model/MediaNodeKind.h`
- Modify: `src/internal/graph/runtime/factory/MediaRuntimeNodeFactory.cpp`
- Create: `tests/unit/rtp_packet_clock_binder_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**

```cpp
struct MediaRtpLockedClockGroup final {
    MediaRunningTime commonSourceEpoch;
    std::vector<std::uint8_t> cname;
    MediaRtpSourceClockCalibration video;
    MediaRtpSourceClockCalibration audio;
};

struct MediaRtpClockGroupSnapshot final {
    MediaRtpClockGroupState state;
    std::uint64_t groupGeneration;
    std::optional<MediaRtpLockedClockGroup> locked;
};

class MediaRtpPacketClockProjector final {
public:
    Result<MediaPacketSourceTiming> project(
        const MediaRtpClockGroupSnapshot& snapshot,
        MediaScheduledStream stream,
        std::uint64_t extendedRtpTimestamp) const;
};
```

The snapshot is discriminated: `locked` exists exactly in `Locked`, and is
absent for Acquiring/Degraded/ReacquireRequired, so no default epoch,
calibration, or CNAME can be fabricated. The validator computes
`commonSourceEpoch` using the planner-selected
`EarliestLockedSenderReportSourceTime` policy. The binder has `packet` and
`clock` inputs and one typed timed-packet output. It retains at most the planner
capacity while Acquiring. It never maps with system arrival time. One fanout
copies the same immutable snapshot object to video binder, audio binder, and
startup clock adapter; consumers cannot independently validate the group.

- [ ] **Step 1: Write RED join/projection tests**

Cover wraparound, common binary CNAME, earliest-SR source epoch, missing/extra
Locked payload, matching generation, reordered packet timestamps, one-AU
duration lookahead and terminal duration policy, clock-before-packet,
packet-before-clock, bounded Acquiring, fair retry after `WouldBlock`, stale
snapshots, Degraded, Invalidated, and `ReacquireRequired`.

- [ ] **Step 2: Implement projection and focused nodes**

Give the first locked group one runtime-issued nonzero generation and reject
every subsequent change under the planner's first-Locked policy. Normalize both
projected source times by the snapshot's single common source epoch. Preserve
packet ownership without extra payload copies.

- [ ] **Step 3: Register node kinds and run GREEN**

Run binder tests, node tests, and deterministic tests. Repeat the binder test 20 times to expose ordering/lost-wakeup failures.

- [ ] **Step 4: Review and commit**

Verify `RawRtpInputNode` is no longer expected to invent `sourceTiming`; only the new binder adds it. Commit as `feat: bind rtp packets to common av clock` and push.

---

### Task 3: Publish MPEG-TS clock state and canonicalize protocol timing

**Files:**

- Modify: `src/internal/graph/nodes/demux/MpegTsDemuxNode.h`
- Modify: `src/internal/graph/nodes/demux/MpegTsDemuxNode.cpp`
- Create: `src/internal/graph/runtime/buffer/MediaSourceClockStateBuffer.h`
- Create: `src/internal/graph/runtime/buffer/MediaSourceClockStateBuffer.cpp`
- Create: `src/internal/graph/nodes/sync/MediaInitialLockedPacketGateNode.h`
- Create: `src/internal/graph/nodes/sync/MediaInitialLockedPacketGateNode.cpp`
- Modify: `src/internal/graph/nodes/sync/MediaCanonicalInputNode.h`
- Modify: `src/internal/graph/nodes/sync/MediaCanonicalInputNode.cpp`
- Modify: `src/internal/graph/sync/MediaCanonicalLineage.h`
- Modify: `src/internal/graph/sync/MediaCanonicalLineage.cpp`
- Create: `tests/unit/av_sync_canonical_input_tests.cpp`
- Modify: `tests/unit/canonical_lineage_tests.cpp`
- Modify: `tests/unit/mpeg_ts_observed_avio_integration_tests.cpp`
- Modify: `CMakeLists.txt`

`MpegTsDemuxNode` emits one selected-program `MediaSourceClockStateBuffer` in
addition to timed packets. The event contains only readiness and generation;
PCR-derived source time remains on packet timing, while master time is read
only from the registry-owned `MediaSteadyMasterClock`. A separate
initial-Locked gate owns bounded
Acquiring retention and locks the first nonzero generation; it rejects every
later generation change, degradation, discontinuity, or reacquisition request.
Canonical input consumes normalized runtime `MediaPacketSourceTiming`; its
options select source identity, decode order, and duration authority but do not
contain a fabricated source epoch, running epoch, or planner-static generation.

- [ ] **Step 1: Write RED TS/canonical tests**

Cover locked selected-program packets, non-selected programs, absent timing,
bounded Acquiring, first nonzero generation, later/future generation,
discontinuity, Degraded/Reacquire, RTP/TS packet duration, and exact/invalid
audio sample-span conversion.

- [ ] **Step 2: Implement typed program-clock output and canonical timing**

Use PCR-derived timing already produced by demux. Do not add wall-clock, arrival-time, PTS repair, or a second TS clock state machine.

- [ ] **Step 3: Run GREEN, review, commit, and push**

Run canonical/node tests and MPEG-TS observed-AVIO integration. Commit as `feat: canonicalize protocol av timing`.

---

### Task 4: Complete activation-before-release and master-clock startup

**Files:**

- Modify: `src/internal/graph/nodes/sync/MediaPlaybackEpochBinderNode.h`
- Modify: `src/internal/graph/nodes/sync/MediaPlaybackEpochBinderNode.cpp`
- Create: `src/internal/graph/runtime/buffer/MediaPlaybackEpochActivatedBuffer.h`
- Create: `src/internal/graph/runtime/buffer/MediaPlaybackEpochActivatedBuffer.cpp`
- Create: `src/internal/graph/runtime/buffer/MediaStartupReleaseTransactionBuffer.h`
- Create: `src/internal/graph/runtime/buffer/MediaStartupReleaseTransactionBuffer.cpp`
- Create: `src/internal/graph/nodes/sync/MediaActivatedStartupReleaseSequencerNode.h`
- Create: `src/internal/graph/nodes/sync/MediaActivatedStartupReleaseSequencerNode.cpp`
- Modify: `src/internal/graph/runtime/MediaGraphRuntimeLifecycleExecutor.cpp`
- Create: `src/internal/graph/nodes/sync/MediaRtpSourceClockStateAdapterNode.h`
- Create: `src/internal/graph/nodes/sync/MediaRtpSourceClockStateAdapterNode.cpp`
- Create: `src/internal/graph/nodes/sync/MediaAvStartupClockNode.h`
- Create: `src/internal/graph/nodes/sync/MediaAvStartupClockNode.cpp`
- Modify: `src/internal/graph/nodes/sync/MediaAvOutputSchedulerNode.cpp`
- Modify: `src/internal/graph/model/MediaNodeKind.h`
- Modify: `src/internal/graph/runtime/factory/MediaRuntimeNodeFactory.cpp`
- Create: `tests/unit/av_sync_production_release_tests.cpp`
- Modify: `tests/unit/av_output_scheduler_tests.cpp`
- Modify: `CMakeLists.txt`

**Required sequencing:**

```text
coordinator.release
  -> playback_epoch_binder.wrap_pending_transaction
  -> activation_release_sequencer.preflight_activate_atomic_event_then_release
  -> bound_release_extractor.atomic_split
```

The binder retains a release until its single typed transaction output can
accept it, then publishes one immutable pending transaction containing the same
compound release and exact group/generation facts. It does not activate the
group. The activation-release sequencer is the sole holder of the compiler-issued
activation capability and owns all event fanout targets plus the compound release
target. It preflights every target before activation or output. While any target
is blocked the group remains Registered and every output remains empty. Once all
targets are ready it activates exactly once, constructs the immutable epoch event,
publishes that same event to every planned target, and finally publishes the
release in the same process call. Target readiness is independent, but visibility
is one transaction; partial prefix commit across `WouldBlock` is forbidden. The
RTP adapter maps
the immutable locked group snapshot to the same generic source-clock state that
TS demux publishes. The startup-clock node consumes that generic state, reads
the registered group master clock at the planned interval, and produces
coordinator ticks without polling or reinterpreting protocol readiness.
Scheduler `start()` accepts Registered, and first media
processing/configuration requires Active.

The activation-release sequencer is the one typed fanout. Its planned outputs
are exactly the video RTP sender, audio RTP sender, and (for MPEG-TS) mux-plan
source required by the selected adapter. Each output receives the same immutable
epoch/generation object before released media can reach the scheduler. This is
the only sender/mux initialization happens-before; no output node reads a
planner-static generation or invents an NTP epoch.

Graph stop and abort are terminal for one runtime incarnation. After workers are
quiesced, the lifecycle owner clears all channels so a partially retained
transaction cannot survive teardown. Restart uses a freshly compiled runtime,
fresh channels, and a newly issued activation capability; node-local stop/start
tests must not simulate a production lifecycle that the runtime does not expose.

- [ ] **Step 1: Write RED activation/lifecycle tests**

Prove activation happens before epoch-event and release visibility, activation
occurs exactly once, every output target independently blocks the whole
transaction with zero prefix visibility, the epoch event precedes release
forwarding, terminal stop/abort clears retained transactions and graph channels,
scheduler
waits while Registered, scheduler configures once after Active, and media
cannot dispatch before activation.

- [ ] **Step 2: Implement nodes without exposing activation capability elsewhere**

Keep the move-only activation capability compiler-issued only to the one bound
activation-release sequencer. The binder has no activation capability. Do not
add public registry activation methods.

- [ ] **Step 3: Run GREEN, 20x lifecycle repetition, review, commit, and push**

Commit as `feat: activate av epoch before atomic release`.

---

### Task 5: Build the protocol-neutral synchronized input segment

**Files:**

- Create: `src/internal/graph/builder/segments/MediaRealtimeAvSyncInputSegmentBuilder.h`
- Create: `src/internal/graph/builder/segments/MediaRealtimeAvSyncInputSegmentBuilder.cpp`
- Create: `src/internal/graph/builder/segments/MediaRealtimeAvSyncInputEndpoints.h`
- Modify: `src/internal/graph/builder/realtime/MediaRealtimeRtpTranscodeGraphBuilder.cpp`
- Modify: `src/internal/graph/model/MediaNodeKind.h`
- Modify: `src/internal/graph/core/MediaGraphDump.cpp`
- Modify: `src/internal/graph/diagnostics/MediaGraphDiagnostics.cpp`
- Modify: `src/internal/graph/runtime/factory/MediaRuntimeNodeFactory.cpp`
- Modify: `tests/unit/test_realtime_rtp_graph.cpp`
- Modify: `CMakeLists.txt`

**Builder result:**

```cpp
struct MediaRealtimeAvSyncInputEndpoints final {
    MediaEndpoint releasedVideo;
    MediaEndpoint releasedAudio;
    MediaEndpoint activatedRelease;
};
```

The segment selects only protocol-specific binder/clock adapters from the immutable assembly plan; canonical input, coordinator, binder, extractor, and master-clock tick topology are shared. The builder does not derive options.

- [ ] **Step 1: Write RED graph-structure tests**

Assert RTP snapshot fanout/binders and the TS selected-program clock output feed
the same generic source-clock/startup chain. Assert exact ports, types, queue
bounds, group identifiers, and one shared first-locked generation. Assert no
unbound coordinator release.

- [ ] **Step 2: Implement the segment and replace legacy input synchronization**

Keep video-only `PacketStartGateNode` behavior outside this segment. Do not reuse `AvPacketStartBarrierNode` for synchronized A/V.

- [ ] **Step 3: Run GREEN, review, commit, and push**

Commit as `feat: assemble synchronized av input segment`.

---

### Task 6: Wire drift correction and canonical encoded audio

**Files:**

- Create: `src/internal/graph/nodes/sync/MediaAudioDriftControllerNode.h`
- Create: `src/internal/graph/nodes/sync/MediaAudioDriftControllerNode.cpp`
- Create: `src/internal/graph/nodes/sync/MediaEncodedAudioCanonicalizerNode.h`
- Create: `src/internal/graph/nodes/sync/MediaEncodedAudioCanonicalizerNode.cpp`
- Modify: `src/internal/graph/builder/segments/MediaAudioBranchSegmentBuilder.cpp`
- Modify: `src/internal/graph/nodes/audio/AudioResampleNode.cpp`
- Modify: `src/internal/graph/model/MediaNodeKind.h`
- Modify: `src/internal/graph/runtime/factory/MediaRuntimeNodeFactory.cpp`
- Create: `tests/unit/av_sync_audio_control_tests.cpp`
- Modify: `tests/unit/audio_codec_lineage_tests.cpp`
- Modify: `tests/unit/audio_lineage_node_tests.cpp`
- Modify: `CMakeLists.txt`

The drift controller is placed after `MediaAudioStartupTrimNode` and before
`AudioResampleNode`. It consumes trimmed decoded canonical audio, reads only the
registered group master clock and active epoch, emits a typed correction window
before forwarding the corresponding audio buffer, and retains both decisions
transactionally across backpressure. `AudioResampleNode` applies only
planner-enabled synchronized corrections. The encoded audio canonicalizer
validates exact contiguous fragments plus `MediaAudioPlaybackOrigin`, then
emits one `MediaCanonicalAccessUnitBuffer` with checked presentation, duration,
generation, and its own monotonic encoded sequence.

- [x] **Step 1: Write RED control/canonicalization tests**

Cover zero/slew corrections, policy bounds, command-before-audio ordering, output backpressure, fragment gaps/overlaps, delayed encoder packets, multi-fragment packets, EOF tail, generation mismatch, and abort/reset.

- [x] **Step 2: Implement the acyclic downstream correction path**

No scheduler-to-resampler graph edge is allowed. Do not derive encoded
timestamps from codec packet PTS. Remove synchronized `Disabled` correction and
`Legacy` lineage options from builder output; planner options must select the
synchronized modes. Do not add a `transcodeAudio` parameter: audio transcoding
continues to be selected naturally by comparing planned target parameters with
the resolved input parameters.

- [x] **Step 3: Run GREEN and lineage lifecycle stress**

Run audio lineage/control/node targets and repeat lifecycle tests 20 times.

- [x] **Step 4: Review, commit, and push**

Commit as `feat: wire synchronized audio correction`.

Progress: implementation and independent review completed in `7c4cf86`; the
implementation and Task 6 evidence were pushed through `64c3a67`.

---

### Task 7: Assemble one shared scheduler and typed output router

**Files:**

- Create: `src/internal/graph/nodes/sync/MediaScheduledOutputRouterNode.h`
- Create: `src/internal/graph/nodes/sync/MediaScheduledOutputRouterNode.cpp`
- Create: `src/internal/graph/builder/segments/MediaRealtimeAvSchedulerSegmentBuilder.h`
- Create: `src/internal/graph/builder/segments/MediaRealtimeAvSchedulerSegmentBuilder.cpp`
- Modify: `src/internal/graph/model/MediaNodeKind.h`
- Modify: `src/internal/graph/runtime/factory/MediaRuntimeNodeFactory.cpp`
- Create: `tests/unit/av_sync_scheduled_output_tests.cpp`
- Modify: `CMakeLists.txt`

The segment accepts canonical encoded video and audio endpoints, creates exactly one scheduler bound to the runtime sync group, then creates one router. The router validates the scheduled stream discriminator and commits each immutable unit to exactly one output edge. It preflights capacity and does not partially route or clone payload unnecessarily.

- [x] **Step 1: Write RED scheduling/router tests**

Cover interleaved A/V ordering, identical dispatch epochs, blocked video/audio output, invalid stream discriminator, EOF ordering, Active-state requirement, and no duplicate/drop on retry.

- [x] **Step 2: Implement and register**

Do not add stream-specific schedulers or pacing clocks.

- [x] **Step 3: Run GREEN, review, commit, and push**

Commit as `feat: share av output scheduler`.

GREEN, independent review, commit `77cb296`, and push to
`codex/quality-priority-improvements` are complete.

---

### Task 8: Add production scheduled RTP sender and SDP nodes

**Files:**

- Create: `src/internal/graph/nodes/output/MediaScheduledRtpSenderNode.h`
- Create: `src/internal/graph/nodes/output/MediaScheduledRtpSenderNode.cpp`
- Create: `src/internal/graph/nodes/output/MediaRtpSenderDescriptionBuffer.h`
- Create: `src/internal/graph/nodes/output/MediaRtpSenderDescriptionBuffer.cpp`
- Create: `src/internal/graph/nodes/output/MediaDualMediaSdpPublisherNode.h`
- Create: `src/internal/graph/nodes/output/MediaDualMediaSdpPublisherNode.cpp`
- Create: `src/internal/graph/builder/segments/MediaScheduledRtpOutputSegmentBuilder.h`
- Create: `src/internal/graph/builder/segments/MediaScheduledRtpOutputSegmentBuilder.cpp`
- Modify: `src/internal/graph/model/MediaNodeKind.h`
- Modify: `src/internal/graph/runtime/factory/MediaRuntimeNodeFactory.cpp`
- Create: `tests/unit/scheduled_rtp_output_node_tests.cpp`
- Modify: `tests/unit/media_rtp_udp_sender_transport_tests.cpp`
- Modify: `CMakeLists.txt`

Each sender owns one `ScheduledRtpSenderSession`, RTP/RTCP UDP resources,
packetizer, counters, and lifecycle. Its three typed inputs are the same
epoch-activated event from the binder fanout, the corresponding encoder
metadata endpoint connected by the builder, and scheduled access units. The
compiler injects the exact registered `MediaAvSyncGroupRuntime`; on activation
the sender verifies the event equals the group's Active playback epoch and
obtains that group's shared NTP epoch. It then materializes
`ScheduledRtpSenderConfig` from the active generation plus the complete
`MediaScheduledRtpOutputPlan`: endpoints, payload type, SSRC, base timestamp,
clock rate, common CNAME, sender lead, SR interval, packetization descriptor,
and capacities. The planner never stores a graph endpoint or runtime encoder
extradata. Runtime encoder metadata only materializes codec configuration and
must match the planner descriptor. The sender emits an opened description only
after epoch, transport, and codec configuration are valid. SDP publication
waits for both descriptions and writes one dual-media SDP transactionally.

- [x] **Step 1: Write RED sender/SDP tests**

Cover video/audio packetization, shared NTP epoch/common CNAME, distinct SSRC/ports, periodic SR, open rollback, blocked writes, EOF/abort/reset, missing codec config, duplicate description, and atomic SDP replacement.

- [x] **Step 2: Implement RAII nodes and output segment**

Reuse `ScheduledRtpSenderSession` and its existing
`ScheduledRtpMuxFfmpegSessionFactory`. The FFmpeg session is strictly a
packetization-only implementation of `ScheduledRtpPacketizerSession`: it owns
no pacing, transport, clock selection, timestamp policy, or sender lifecycle.
The shared scheduler selects AU dispatch time, and `ScheduledRtpSenderSession`
maps the planned timestamp and owns transport/RTCP. Do not create graph-level
`RtpMuxNode` or `RtpOutputNode`, and do not invoke FFmpeg mux pacing.

- [x] **Step 3: Run GREEN and loopback integration**

Run unit targets and RTP transport integration. Decode both elementary outputs using the generated SDP.

- [x] **Step 4: Review, commit, and push**

Commit as `feat: send scheduled separate rtp output`.

Implementation commits `d142558`, `8ebfa7d`, and `9ffa1ac` complete the
production sender/SDP path. Clean verification, the ten-target matrix, real
receiver-first dual-media SDP decode, and independent review all passed. The
commits are ready to push to `codex/quality-priority-improvements`.

---

### Task 9: Add production scheduled project MPEG-TS output

**Files:**

- Create: `src/internal/graph/nodes/output/MediaProjectMpegTsPlanSourceNode.h`
- Create: `src/internal/graph/nodes/output/MediaProjectMpegTsPlanSourceNode.cpp`
- Create: `src/internal/graph/nodes/output/MediaScheduledTsAccessUnitAdapterNode.h`
- Create: `src/internal/graph/nodes/output/MediaScheduledTsAccessUnitAdapterNode.cpp`
- Create: `src/internal/graph/builder/segments/MediaScheduledMpegTsOutputSegmentBuilder.h`
- Create: `src/internal/graph/builder/segments/MediaScheduledMpegTsOutputSegmentBuilder.cpp`
- Modify: `src/internal/graph/nodes/mux/FileMuxNode.cpp`
- Modify: `src/internal/graph/model/MediaNodeKind.h`
- Modify: `src/internal/graph/runtime/factory/MediaRuntimeNodeFactory.cpp`
- Create: `tests/unit/scheduled_mpeg_ts_output_node_tests.cpp`
- Modify: `tests/unit/project_mpeg_ts_file_mux_node_tests.cpp`
- Modify: `tests/unit/project_mpeg_ts_mux_session_adapter_tests.cpp`
- Modify: `tests/unit/mpeg_ts_output_decode_integration_tests.cpp`
- Modify: `CMakeLists.txt`

The plan source consumes the binder's typed epoch-activated notification and
publishes the immutable planner-produced `MediaTsMuxPlan` plus that exact epoch
once, before released media can reach the mux. It does not poll the registry or
guess active time. Scheduled TS adapters create typed
`MediaTsAccessUnitBuffer` values from canonical schedule/presentation times and
exact encoder codec configuration. `FileOutputNode` remains the byte-sink owner
and `FileMuxNode` remains the project mux session owner.

`ProjectMpegTsMuxSessionAdapter` may honor its planner-owned `nextDeadline` only
to serialize TS packets within an already-dispatched AU and maintain required
CBR/PCR cadence. It must not delay, advance, or reinterpret an AU's canonical
dispatch/presentation time. `MediaAvOutputSchedulerNode` remains the sole media
pacing authority; TS packet cadence is transport serialization, not a second
media clock.

- [ ] **Step 1: Write RED plan/adapter tests**

Cover plan-before-media ordering, both streams, PTS/DTS/PCR derivation from playback epoch, codec config, backpressure, EOF, abort/reset, generation mismatch, and absence of FFmpeg mux fallback.

- [ ] **Step 2: Implement and integrate with the existing project mux**

Do not infer timestamps from input PCR jitter or codec packet PTS. Do not create a second pacing authority inside the mux.

- [ ] **Step 3: Run GREEN and decode integration**

Verify continuity counters, program/PIDs, PCR cadence, and decodable H.264/AAC output.

- [ ] **Step 4: Review, commit, and push**

Commit as `feat: mux scheduled project mpeg ts output`.

---

### Task 10: Switch production graph assembly and remove legacy sync authorities

**Files:**

- Modify: `src/internal/graph/builder/realtime/MediaRealtimeRtpTranscodeGraphBuilder.cpp`
- Modify: `src/internal/graph/builder/realtime/MediaRealtimeRtpTranscodeGraphBuilder.h`
- Modify: `src/internal/graph/runtime/factory/MediaRealtimeExecutableGraph.h`
- Modify: `src/internal/graph/runtime/compilation/MediaGraphRuntimeCompiler.cpp`
- Modify: `src/internal/graph/runtime/compilation/MediaAvSyncRuntimeBootstrap.cpp`
- Modify: `src/internal/graph/runtime/factory/MediaRuntimeNodeFactory.cpp`
- Modify: `tests/unit/test_realtime_rtp_graph.cpp`
- Modify: `tests/unit/av_sync_runtime_bootstrap_tests.cpp`
- Create: `tests/unit/av_sync_production_runtime_integration_tests.cpp`
- Delete after last synchronized caller is gone: `src/internal/graph/nodes/packet/AvPacketStartBarrierNode.h`
- Delete after last synchronized caller is gone: `src/internal/graph/nodes/packet/AvPacketStartBarrierNode.cpp`
- Modify: `CMakeLists.txt`

For synchronized A/V, the builder composes only the new input, codec, scheduler, and selected protocol-output segments. Remove the `ProjectMpegTs` and `avSyncRuntime` `Unsupported` guards only when both complete topologies exist. Attach exactly one `MediaAvSyncRuntimeBinding`, and require the compiler to bind exactly one playback-epoch binder and one scheduler to the same group.

- [ ] **Step 1: Write RED whole-graph tests**

Assert exact RTP and TS production topologies and all typed ports. Assert
synchronized graphs contain none of: `AvPacketStartBarrier`, `RtpMux`,
`RtpOutput`, `SdpWriter`, `PacketNormalize`, `VideoTimestamp`, FFmpeg RTP mux,
`MediaGraphPacingClock`, independent pacing, timestamp repair, correction
back-edge, generic `*.sync_group`, or multiple schedulers. Assert video-only
RTP still uses its supported gate/output path.

- [ ] **Step 2: Build the executable graphs and register every new node**

Remove fail-closed `Unsupported` only after factory registration, runtime
preparation, binding validation, and lifecycle rollback tests all pass. Extend
compiler validation only with exact typed consumers; never restore a generic
`*.sync_group` suffix rule. Keep separate-RTP-to-TS unsupported. The production
graph must not register purge participants or call any next-generation API.

- [ ] **Step 3: Delete the obsolete barrier and dead sync pacing helpers**

Before deletion, search every caller. Delete only components with zero valid callers; do not retain compatibility aliases.

- [ ] **Step 4: Run GREEN production-runtime integration**

Run both topologies through compile/start/process/stop plus abort/reset paths. Require real scheduler activation and real output adapters, not structure-only success.

- [ ] **Step 5: Review, commit, and push**

Commit as `feat: enable av sync production graphs`.

---

### Task 11: Enforce Task 12 failure boundaries and observability

**Files:**

- Modify: `src/internal/graph/sync/MediaAvSyncError.h`
- Modify: `src/internal/graph/runtime/diagnostics/MediaGraphRuntimeReport.cpp`
- Modify: `src/internal/graph/runtime/diagnostics/MediaGraphRuntimeMetrics.h`
- Create: `tests/unit/av_sync_production_failure_tests.cpp`
- Modify: `tests/unit/av_sync_production_runtime_integration_tests.cpp`
- Modify: `tests/unit/event_runtime_threading_queue_tests.cpp`

Expose structured counters/status for clock acquisition wait, locked generation, terminal degradation, discontinuity, requested reacquisition, startup release, scheduler dispatch, protocol commit, queue high-water, progress stall, and A/V drift. Task 12 never silently restarts or drops into legacy execution.

Add one structured A/V timeline diagnostic record instead of ad-hoc text logs.
Each record carries sync group, generation, stream, canonical AU sequence,
canonical presentation time, mapped master presentation/dispatch time, actual
protocol commit time, queue latency, and terminal/error state. The scheduler and
protocol adapters publish facts to the existing runtime report; they do not
independently calculate policy or emit competing drift values. A single
diagnostic analyzer derives startup offset, per-event A/V skew, P50/P95/maximum
and final absolute drift, and drift slope over the observation window.

- [ ] **Step 1: Write RED fault-injection tests**

Inject degradation/discontinuity/reacquire at each pre-start and active boundary. Assert first-error preservation, group abort, retained-state cleanup, no post-error protocol writes, and no hidden restart.

Also inject deterministic interleaved A/V access units and verify the timeline
records preserve exact sequence/generation/master-time relationships across
scheduler dispatch and protocol commit. Missing, duplicate, reordered, or
cross-generation diagnostic records fail the analyzer rather than being
silently omitted.

- [ ] **Step 2: Implement structured fail-closed reporting**

Reuse the existing runtime report; do not create a parallel report model.

- [ ] **Step 3: Run GREEN, review, commit, and push**

Commit as `feat: report av sync production failures`.

---

### Task 12: Clean verification, independent review, and human playback

**Files:**

- Modify only after actual evidence exists: `docs/completed/quality-priority-2-av-sync-production.md`
- Modify only after actual evidence exists: `plan.md`
- Modify only after actual evidence exists: `QUALITY_SCORE.md`
- Reuse/update as necessary: `tests/acceptance/prepare_runtime_dependencies.ps1`
- Reuse/update as necessary: `tests/acceptance/run_av_sync_gate.ps1`
- Reuse/update as necessary: `tests/acceptance/inspect_rtp_sync.ps1`
- Reuse/update as necessary: `tests/acceptance/inspect_mpegts_sync.ps1`
- Create: `tests/acceptance/analyze_av_sync_timeline.ps1`

- [ ] **Step 1: Full clean deterministic verification**

Run:

```powershell
cmake --build out/build/x64-debug --clean-first --target all
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -L deterministic --timeout 120
1..20 | ForEach-Object { & out/build/x64-debug/media_transcode_runtime_tests.exe; if ($LASTEXITCODE) { throw "runtime iteration $_ failed" } }
1..20 | ForEach-Object { & out/build/x64-debug/media_transcode_node_tests.exe; if ($LASTEXITCODE) { throw "node iteration $_ failed" } }
git diff --check
```

Every executed target must exit `0`. No stale symbol validation is accepted; the build is always `--clean-first --target all`.

- [ ] **Step 2: Full integration verification**

Run the production runtime, RTP transport, MPEG-TS observed input, and MPEG-TS output decode integration executables, then `ctest -L integration`. Optional hardware absence is reported unsupported; software fallback is forbidden for acceptance.

- [ ] **Step 3: Copy runtime DLLs outside CMake**

Copy the required DLLs from `D:\mabs\local64\bin-video` beside `out\build\x64-debug\media_transcode_realtime_video_cli.exe`. Do not add external paths, copy rules, or runtime environment configuration to `CMakeLists.txt`.

- [ ] **Step 4: Run a 20-30 second hardware separate-RTP smoke**

Use the deterministic flash/beep fixture and the production CLI. Input and
output are separate video/audio RTP streams. Probe the generated dual-media SDP directly;
do not substitute a captured `.ts`. Record only
`media_transcode_realtime_video_cli` process CPU, memory, threads, queue peaks,
stalls, decode errors, drops, the structured runtime A/V timeline, and decoded
flash/beep event measurements. Require both streams, zero decode/worker/drop
errors, no stall over 5 seconds, complete scheduler-to-protocol diagnostic
lineage, absolute startup/final/maximum A/V offset within 100 ms, and no
statistically sustained drift slope before opening VLC.

- [ ] **Step 5: Run a 20-30 second hardware MPEG-TS smoke**

Use UDP MPEG-TS input and UDP MPEG-TS output through the production CLI. Probe
the actual output directly and require decodable H.264/AAC, valid
program/PIDs/PCR/PTS/DTS/continuity, zero worker/decode/drop errors, no stall
over 5 seconds, complete structured runtime A/V timeline, decoded flash/beep
event measurements, absolute startup/final/maximum A/V offset within 100 ms,
and no statistically sustained drift slope.

- [ ] **Step 6: Open the separate-RTP SDP in VLC for the user**

Launch `D:\VideoLAN\VLC\vlc.exe` with the generated SDP while both scheduled
senders remain live. Let the user observe picture continuity, normal audio, no
stutter, no perceptible startup offset, and no drift. Wait for explicit user
confirmation before marking RTP human verification passed. This is a subjective
quality observation only; it does not replace or override the automated drift
gate.

- [ ] **Step 7: Open the MPEG-TS output directly in VLC for the user**

Launch VLC on the live UDP TS output, wait for explicit user confirmation, and
do not play a capture-remux. Record the result as subjective playback quality;
do not use human perception as the precise synchronization measurement and do
not override a failed automated drift gate.

- [ ] **Step 8: Run the formal 120-second hardware gates**

After human playback confirms the new topology, run separate RTP and MPEG-TS
for 120 seconds each. Hard gate: exit `0`; complete audio/video streams; zero
decode/worker/drop/continuous-frame errors; no stall over 5 seconds; final
absolute A/V drift <= 100 ms; hardware whole-machine-normalized average process
CPU for `media_transcode_realtime_video_cli` <= 5%. Record P95/average CPU,
memory, threads, queue peaks, maximum latency, protocol errors, and drift.
The formal report must include runtime-timeline and decoded-event startup
offset, per-event skew series, P50/P95/maximum/final drift, drift slope, matched
event count, and confidence. Any disagreement between internal scheduling facts
and decoded output is a failure requiring root-cause analysis.

- [ ] **Step 9: Update evidence and quality score**

Record actual test commands/results (not build commands) in the completion document. Re-run every documented PowerShell test command verbatim. Update `plan.md` and rescore `QUALITY_SCORE.md` using the existing dimensions/weights without preselecting a score. List remaining risks, including Task 13 automatic reacquisition.

- [ ] **Step 10: Fresh independent Agent review**

Ask a new Agent to review the complete branch and PR against the approved design, planner authority, RAII, SRP, legacy-authority removal, both runtime topologies, and acceptance evidence. Fix and repeat until PASS.

- [ ] **Step 11: Final commit, push, and PR refresh**

Commit evidence/doc changes, push the same branch, confirm PR #21 points to the latest head, and report real test results, human playback conclusions, review verdict, quality score, remaining risks, and Task 13 follow-up.

## Task 13 boundary

The following are intentionally excluded from Task 12 and must remain explicit terminal conditions rather than compatibility behavior:

- cross-generation purge and automatic reacquisition;
- recovery after RTP clock-group degradation or CNAME/source discontinuity;
- recovery after MPEG-TS program/PCR discontinuity;
- output permit rollover across generations;
- multi-generation drift/recovery metrics.

Task 13 will reuse the typed transition service and purge participants already built in Tasks 1-11. Task 12 must not pre-implement a partial recovery path.
