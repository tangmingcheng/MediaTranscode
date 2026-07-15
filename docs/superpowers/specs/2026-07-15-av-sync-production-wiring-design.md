# A/V Synchronization Production Wiring Design

## Goal and Scope

Complete the production A/V synchronization system for exactly two realtime topologies:

1. Separate RTP video/audio input to separate RTP video/audio output.
2. MPEG-TS input to MPEG-TS output.

Both paths must use one planner-owned synchronization domain from protocol clock evidence through encoded output. Separate RTP input to MPEG-TS output remains unsupported and fails in the planner. Local-file behavior and video-only realtime behavior remain separate non-synchronized paths.

The finish line is real hardware playback with synchronized startup, no perceptible drift or stalls, and CPU measured only for `media_transcode_realtime_video_cli`. Passing component tests without production graph wiring is not acceptance.

## Completed Task 1-11 Baseline

Tasks 1 through 11 in `docs/superpowers/plans/2026-07-11-av-sync-system.md` are completed baselines and are not reimplemented by this design. Task 12 reuses their accepted components and contracts, including strong running time, RTP/TS source clocks, canonical mapping, startup coordination, audio servo policy, video synchronization control, the shared scheduler, RTP/RTCP output primitives, and project-owned MPEG-TS serialization.

Task 12 is limited to the missing production seams: typed runtime group bootstrap, canonical lineage across the existing codec pipeline, exactly-once epoch activation, `emitOnMaster`, production RTP/TS adapters, complete graph wiring, and atomic removal of obsolete synchronized authorities. Existing Task 1-11 behavior changes only when a missing Task 12 interface must be added; its previously verified policy is not duplicated or redesigned.

## Non-Negotiable Invariants

- `MediaSteadyMasterClock` is the only media pacing clock in synchronized A/V execution.
- One `MediaAvSyncGroupKey` identifies one immutable `MediaAvSyncPlan`, one master clock, and at most one active `MediaPlaybackEpoch` generation.
- The planner produces every topology, queue, clock, codec, RTP, TS, lifecycle, and correction decision. Builders only assemble the selected topology; nodes do not infer defaults or fall back.
- Canonical presentation, decode, duration, source identity, source sequence, mapping confidence, and generation remain attached to media through decode, filter, resample, and encode.
- Missing canonical lineage, ambiguous output-to-input identity, mixed generations, or an unregistered epoch is terminal. Arrival time, first-packet-zero, DTS-as-PTS, timestamp synthesis, and local stream rebasing are not recovery modes.
- The startup coordinator never releases one stream alone. The epoch binder activates exactly one complete audio/video release transaction.
- The shared scheduler owns A/V media-time dispatch. Protocol outputs may enforce byte-transport capacity but cannot sleep, rebase, or repair media timestamps independently.
- Synchronized MPEG-TS uses `byte_sink` plus `project_mpegts`; there is no FFmpeg MPEG-TS fallback.
- Synchronized separate RTP uses the project scheduled RTP/RTCP sender boundary; it does not use `RtpMuxNode` or an FFmpeg RTP mux clock.
- `PacketStartGateNode` remains only for valid video-only paths.

## Planner and Runtime Bootstrap

`MediaRealtimeRtpTranscodePlan` contains one complete synchronized runtime product for the two supported A/V topologies. It carries the validated `MediaAvSyncPlan`, `MediaAvSyncGroupKey`, protocol output plan, explicit output adapter kinds, and every queue/lifecycle limit required by the graph.

Legacy synchronized fields for `MediaRealtimeAvStartBarrierPlan`, per-mux pacing, startup delay, local RTP timestamp normalization, and independent stream epochs are removed from the synchronized plan instead of retained as compatibility seams.

`MediaRealtimeExecutableGraph` carries a typed `MediaAvSyncRuntimeBinding` beside prepared input bindings. The binding contains the group key and immutable synchronization plan; it never contains a manufactured playback epoch. During compile/start preparation, `MediaGraphRuntimeCompiler` creates the sole `MediaSteadyMasterClock`, registers the group exactly once, and leaves it in `AwaitingEpoch`. Duplicate registration, a node referring to another group, or missing runtime binding fails before workers start.

For separate RTP output, the runtime captures `(masterNow, wallNow)` exactly once after group registration and constructs the sole immutable `MediaSharedNtpEpoch` for that group. The group owns this value and injects the same object into both RTP senders. SDP publication consumes the same typed RTP plan, codec descriptions, and CNAME identity but no NTP epoch. The planner supplies policy and identifiers but cannot manufacture a runtime epoch; protocol nodes cannot create, replace, or locally adjust it.

The runtime owns the clock and group through RAII. Stop, abort, full graph-context reconstruction, and compile failure close the group once and wake every deadline waiter.

## Canonical Timing Lineage

Task 12 extends the existing `MediaCanonicalAccessUnitBuffer` with one shared immutable `MediaCanonicalLineage` value containing:

- canonical presentation and optional canonical decode time;
- canonical duration;
- `MediaDecodeOrderMode`;
- source stream identity and monotonically increasing source sequence;
- mapping confidence and synchronization generation.

Focused `MediaCanonicalVideoFrameBuffer` and `MediaCanonicalAudioSamplesBuffer` stage wrappers carry the same lineage through decoded and processed payloads. They extend the existing access-unit contract instead of introducing a parallel generic media buffer, and they do not copy packet, frame, or sample payloads merely to carry metadata.

RTP ingress derives the stamp only from validated RTP/RTCP source-clock evidence. MPEG-TS ingress derives it only from the selected program PCR/PTS/DTS evidence. Neither topology uses packet arrival time.

Processing boundaries preserve or deliberately transform lineage:

- Video decode transfers packet lineage to decoded frames using explicit FFmpeg opaque identity and a bounded pending-sequence registry. Reordered output must resolve to exactly one submitted access unit.
- Video filtering preserves identity for one-to-one frames and creates an explicit derived sequence for rate conversion, with the planner-selected duration relationship.
- Video encoding propagates the frame lineage to every encoded packet through FFmpeg opaque identity and a bounded pending registry. Delayed or reordered packets must still resolve uniquely.
- Audio decode converts packet lineage into a canonical sample interval.
- Audio resampling consumes and produces explicit sample intervals. Duration and presentation advance from exact input/output sample counts and the applied compensation window; timestamps are not rewritten to hide correction.
- Audio encoding associates each encoded packet with the exact accumulated canonical sample interval that produced it.

Flush and EOF drain the lineage registries transactionally. A codec packet/frame without a matching lineage entry, duplicate identity, capacity exhaustion, or leftover uncommitted lineage at terminal completion is a structured synchronization error.

## Startup, Readiness, and Epoch Activation

Input clock readiness and media buffering remain separate responsibilities. Clock trackers publish typed readiness; `MediaAvStartupCoordinatorNode` consumes canonical encoded audio/video access units before decode and the registered group clock.

The coordinator progresses through acquiring, priming, armed, and released states. It selects a decodable encoded video keyframe, complete encoded audio coverage, sample-accurate `trimLeadingSamples`, and a common preroll window. It publishes one atomic release containing both encoded stream sets, the trim count, and a proposed `MediaPlaybackEpoch`.

`MediaPlaybackEpochBinderNode` is the only component issued a `MediaPlaybackEpochActivationCapability`. It validates the release group, plan, generation, and common window; uses `activatePlaybackEpoch` for the first generation and `activateNextPlaybackEpoch` only after a completed generation-transition barrier; and forwards both released stream sets only after activation succeeds. General runtime and node contexts cannot activate an epoch without this capability. Repeated, partial, old-generation, or conflicting release transactions fail without forwarding media.

After binding, the video path is decode, filter, encode, and shared scheduler. The audio path is decode, one `MediaAudioStartupTrimNode`, resample, encode, and shared scheduler. The trim node is the sole consumer of `trimLeadingSamples`; no codec, resampler, scheduler, or protocol adapter may infer or repeat startup trimming.

On discontinuity or hard synchronization error, `MediaAvGenerationTransitionCoordinator` executes one in-band transaction for the whole group:

1. Revoke the old generation output permit so neither protocol adapter can commit more media.
2. Broadcast a typed purge carrying old generation, proposed next generation, and transition sequence to startup, lineage registries, servo/correction mailbox, scheduler, RTP output, and MPEG-TS output.
3. Require every planner-declared participant to clear queued media, pending commits, correction commands, deadlines, protocol session state, and generation-local counters, then acknowledge that exact transition sequence.
4. Issue the binder its next-generation activation permission only after every acknowledgement succeeds; `activateNextPlaybackEpoch` publishes the new generation output permit atomically.

Old-generation media cannot cross the revoked permit or the acknowledgement barrier. A stale, missing, duplicate, or failed acknowledgement poisons the group and aborts both streams. Audio and video never independently rebase.

## Production Topology

Both synchronized graphs share this media spine:

```text
protocol clock mapping
  -> MediaCanonicalAccessUnitBuffer
  -> MediaAvStartupCoordinatorNode
  -> MediaPlaybackEpochBinderNode
     -> video decode -> canonical video frame -> filter -> video encode ----\
                                                                            +-> MediaAvOutputSchedulerNode
     -> audio decode -> MediaAudioStartupTrimNode -> resample -> audio encode /
MediaAvOutputSchedulerNode
  -> video protocol adapter
  -> audio protocol adapter -> MediaAudioPlayheadCommitNode
```

For separate RTP, the two protocol adapters are the two `MediaScheduledRtpOutputNode` instances and share the group-owned NTP epoch. For MPEG-TS, the scheduler ports enter the typed TS access-unit adapters and one project-owned mux session. Readiness publication and audio correction use group-owned control mailboxes beside the DAG; they are not media edges and cannot form cycles.

## Shared Scheduler and Drift Control

One `MediaAvOutputSchedulerNode` consumes both canonical encoded streams for the active group. Its typed output contract is `MediaScheduledAccessUnit` with:

- canonical presentation/decode/duration;
- `presentationOnMaster`;
- `dispatchOnMaster` for content decode order;
- `emitOnMaster` for protocol emission;
- source sequence, generation, and video recovery decision.

The planner supplies the output emission policy. For MPEG-TS, `dispatchOnMaster - emitOnMaster` equals the exact positive `transportDecodeLead`. RTP uses its explicit sender lead; it is never inherited from TS or set to zero implicitly.

The scheduler requires `emitOnMaster <= dispatchOnMaster <= presentationOnMaster` for every unit without exception. Reordered video changes only inter-unit decode ordering: `canonicalDispatch` and `dispatchOnMaster` remain monotonic in decode order while each unit still satisfies the same inequality. `dispatchOnMaster` is ordering metadata only and is never a pacing deadline. Across both input ports the scheduler globally selects the earliest `(emitOnMaster, deterministicStreamTieBreak, sourceSequence)` and waits only until `emitOnMaster`, reporting `waitingUntil(group, emitOnMaster)` when early. Equal-deadline arbitration is deterministic and fair. Backpressure does not advance scheduler/controller state until the selected output is committed. PSI, PCR, RTP, and RTCP deadlines use the same group master clock.

Both output protocols expose the same typed `MediaProtocolCommitAcknowledgement`. The RTP adapter creates it only after `ScheduledRtpSenderSession::sendAccessUnit` has successfully written the complete access-unit datagram batch to the UDP sink. The project MPEG-TS adapter creates it only after `MediaMuxSession::write` has successfully completed every byte-sink write for that access unit; `FileMuxNode` publishes the acknowledgement on a typed side output even though audio and video share one mux. A returned `Status` alone is not a measurement: the adapter adds the committed generation, stream, source sequence, presentation/emission times, exact output sample interval, and master-clock commit observation. Partial or deferred sink writes cannot acknowledge.

`MediaAudioPlayheadCommitNode`, consuming the audio `MediaProtocolCommitAcknowledgement`, is the sole producer of `MediaAudioDriftMeasurement`. Let the committed output sample interval be `[sampleBegin, sampleEnd)`, `epochSampleIndex` be the exact output sample index aligned with `epoch.sourceStart`, the unit master presentation window be `[presentationOnMaster, presentationOnMaster + duration)`, and `observedAt` be a group master-clock read taken immediately after the complete sink write. It computes:

```text
sampleEndOnMaster = epoch.masterRelease
                  + (sampleEnd - epochSampleIndex) / outputSampleRate
phaseError = (presentationOnMaster + duration) - sampleEndOnMaster
commitLateness = observedAt - emitOnMaster
```

Division uses the project running-time checked rational conversion and the committed acknowledgement's exact interval. Positive `phaseError` means audio content leads the master schedule and the existing positive stretch command lengthens future output; negative means audio lags. `commitLateness` is separate transport-health telemetry and is never fed into the servo, so planned sender/transport lead and transient sink latency cannot masquerade as clock drift. Only after this calculation succeeds does the node report `phaseError`, `observedAt`, `generation`, `sequence`, `effectiveOutputSampleIndex = sampleEnd`, and `outputSampleRate`; a queued or targeted sample position is never treated as played.

The feedback path is a control plane, not a DAG edge. The group runtime owns one bounded, generation-tagged `MediaAudioCorrectionMailbox` and a monotonic resampler `highestProducedSampleIndex` watermark. `MediaAudioDriftServoNode` publishes a correction with a unique sequence, future effective output sample index, compensation window, and planner-selected command lead. `AudioResampleNode` reads the mailbox before the matching future sample window, applies each command exactly once, and records an acknowledgement containing the actual produced sample interval. The next measurement is based on that acknowledged interval. Late, stale, duplicate, skipped, or unacknowledged commands are structured synchronization errors and trigger reacquisition; mailbox capacity and expiry are planner-owned.

Before graph construction the planner converts every bounded audio decode/resample/encode/scheduler/protocol queue, decoder delay, encoder lookahead, protocol batch, mailbox delivery bound, and maximum resampler output block to output samples and proves:

```text
commandLeadSamples > worstCaseInFlightSamples
                   + mailboxDeliveryMarginSamples
                   + maximumResamplerOutputBlockSamples
effectiveOutputSampleIndex >= measuredSampleEnd + commandLeadSamples
```

`correctionLookaheadWindows` and the compensation window must cover that lead. Missing bounds, overflow, or an unsatisfied inequality rejects the plan before graph construction. At command publication the runtime rechecks the target against the current `highestProducedSampleIndex`; a target that is not strictly beyond the watermark plus one maximum output block is unreachable and triggers reacquisition instead of silently applying late. This bounded credit/watermark contract prevents normal buffering from causing a reacquisition loop.

Locked correction is limited to plus or minus 1000 ppm, recovery correction to plus or minus 5000 ppm, and hard error triggers group reacquisition.

The existing video controller remains part of the shared scheduler. It may hold, display, drop a permitted non-key frame, repeat an explicitly identified previous frame, or request reacquisition. It never changes the master clock or shifts later timestamps.

## MPEG-TS Production Output

The planner selects `MediaOutputResourceKind::ByteSink` and `MediaMuxSessionKind::ProjectMpegTs` for synchronized MPEG-TS output.

`MediaTsRuntimePlanSourceNode` combines the immutable planner `MediaTsMuxPlan` with the active runtime epoch and publishes exactly one `MediaTsMuxRuntimePlanBuffer` per generation. It validates, but does not choose, the plan or epoch.

`MediaScheduledToTsAccessUnitNode` converts scheduled encoded packets to `MediaTsAccessUnitBuffer`. It verifies stream, generation, keyframe metadata, presentation/decode/emission times, and exact transport decode lead. Encoder configuration is snapshotted once into explicit `FFmpegCodecParametersBuffer` instances before packet delivery.

The output graph binds plan, byte sink, video/audio codec parameters, and scheduled access units to `FileMuxNode` with the project MPEG-TS session. `ProjectMpegTsMuxSessionAdapter` remains the lifecycle boundary; it does not read string policy or create an epoch.

## Separate RTP Production Output

`MediaScheduledRtpOutputNode` is a single-stream protocol adapter instantiated once for video and once for audio. Each instance consumes only its scheduled stream and a complete typed RTP runtime plan.

The node is the only production RTP output seam and composes the existing `ScheduledRtpSenderSession`, `ScheduledRtpMuxFfmpegSessionFactory`, UDP transport, clock mapper, and sender-report generator. Task 12 wires and owns these accepted Task 1-11 components; it does not duplicate their packetization, clock mapping, or RTCP behavior.

The two plans share one `MediaSharedNtpEpoch` and common planner-produced CNAME while retaining distinct SSRC, payload, clock-rate, RTP/RTCP endpoints, and sequence/timestamp bases. RTP timestamps, RTCP sender reports, and sender counters derive exclusively from the scheduled canonical/master times.

The node owns the RTP/RTCP UDP sender session through RAII, maps its next protocol deadline to `waitingUntil`, and forwards no media-time decisions to transport code. SDP publication consumes the same immutable typed plan and exact materialized codec descriptions; VLC must be able to open the generated SDP directly.

## Obsolete Authority Removal

After both synchronized graphs pass contract tests, remove the old authorities atomically from synchronized A/V execution:

- `AvPacketStartBarrierNode` and its planner product;
- `PacketNormalizeNode` timestamp shifting on synchronized paths;
- `VideoTimestampNode` missing-timestamp synthesis and local PTS/DTS selection on synchronized paths;
- `RtpMuxNode`/`RtpMuxFfmpegSession` local rebase, `+1 tick` repair, startup sleep, and pacing clock on synchronized paths;
- `MediaGraphPacingClock` as a synchronized media authority;
- `FFmpegPacedAvio` from synchronized RTP/TS media timing.

Physical deletion is allowed only when no valid local or video-only path still depends on a component. Otherwise the component remains isolated to that explicit non-synchronized topology and cannot be referenced by either synchronized graph. Existing enum numeric values remain stable; removed node kinds become reserved values when necessary.

## Fault and Lifecycle Contract

Nodes support deterministic start, flush, EOF, finish, abort, and the in-band generation-transition transaction defined above. A full graph reset is stop or abort followed by reconstruction of the runtime context; there is no assumed node-level `reset` virtual. The first error is sticky and propagates to the group. No node may report progress without consuming, producing, committing, or advancing a due protocol deadline.

The first clean input EOF moves the group into draining rather than finishing one stream. Buffered audio and video may complete only within the planner-provided terminal drain window. The group finishes only after both input EOFs arrive and every lineage entry, scheduler commit, protocol batch, and control-plane acknowledgement is complete. Drain timeout, unplanned early EOF, or leftover generation state is terminal. Abort closes both streams immediately and exactly once.

Fault coverage includes missing/late clock evidence, SR/PCR interruption, timestamp wrap/regression, source identity change, packet loss/jitter, lineage mismatch, keyframe delay, queue pressure, sink partial failure, worker failure, and reacquisition. Deadline waits use the existing sequence-based wakeup mechanism; no busy loop or fixed polling sleep is introduced.

## Implementation Sequence

1. Planner product and runtime group bootstrap.
2. Canonical timing lineage through video and audio processing.
3. Startup coordinator integration and exactly-once epoch binder.
4. Shared scheduler `emitOnMaster`, audio servo execution, and explicit A/V outputs.
5. Project MPEG-TS production adapters and graph.
6. Scheduled RTP/RTCP/SDP production adapters and graph.
7. Atomic legacy-authority removal, lifecycle/fault matrix, and production graph regression.
8. Hardware-only playback and stability acceptance.

Each sequence item is independently TDD-driven, clean-built after model changes, committed, pushed, and reviewed by a fresh Agent before the next dependency is enabled.

## Acceptance

The acceptance harness creates one deterministic 120-second, 1280x720 30 fps H.264 video plus 48 kHz AAC audio ground-truth source outside CMake and records its SHA-256 plus an event manifest. A simultaneous one-frame full-field flash and short broadband beep begins at 500 ms and repeats every two seconds, giving 60 known common-timeline events. The separate RTP sender and MPEG-TS sender must both derive their inputs from this exact source and preserve the manifest time base; neither output is capture-remuxed for analysis or playback. Fixture generation or checksum/manifest validation failure blocks the run. Automated correlation requires at least 50 unambiguous matched flash/beep pairs per topology; fewer pairs are inconclusive, not passing. The existing `test.mp4` remains useful for ordinary playback regression but cannot replace this synchronization ground truth.

For separate RTP input/output and MPEG-TS input/output respectively:

- run the hardware encoding path for two minutes;
- on Windows, sample only the exact process name `media_transcode_realtime_video_cli` and calculate normalized whole-machine CPU as `delta TotalProcessorTime / (wall interval * logical processor count) * 100`; require the time-weighted whole-process average to be at most 5%;
- require exit code zero, complete audio/video streams, zero decode errors, worker errors, drops, continuity errors, and continuous-frame anomalies;
- permit no progress stall longer than five seconds;
- correlate decoded flash/beep events and require startup A/V offset at most 40 ms, steady-state P95 at most 20 ms, P99 at most 40 ms, final absolute drift at most 100 ms, and fitted drift slope at most 1 ms/hour;
- for each two-minute run, report event sample count, fitted slope, confidence interval, and residual distribution; an insufficient sample count or confidence interval that cannot demonstrate the slope gate is inconclusive rather than passing;
- record P95/process CPU, working set, active threads, queue high-water marks, maximum latency, and A/V drift;
- capture separate RTP output and verify distinct audio/video SSRCs, common CNAME, the one shared NTP epoch, periodic sender reports, and RTP-to-SR timestamp consistency;
- parse MPEG-TS output and verify the selected program and PIDs, continuity counters, PCR cadence and jitter, and PTS/DTS/PCR relationships;
- open the generated SDP directly for RTP and the TS output directly for MPEG-TS in VLC;
- require continuous picture, normal sound, no perceptible startup offset, no stutter, and no perceptible drift during the user-observed playback interval.

An automated or subjective failure blocks completion. Thresholds are not relaxed and failed runs are not replaced by retries without first identifying the root cause.
