# Complete A/V Synchronization System Implementation Plan

> **For Codex:** Execute this plan with `superpowers:subagent-driven-development`. Every production behavior must follow RED-GREEN-REFACTOR and each task must pass independent specification and code-quality review.

**Goal:** Replace the current independent RTP/TS timestamp normalization and pacing behavior with one planner-owned synchronization domain that keeps separate RTP A/V output and MPEG-TS output synchronized at startup and over time.

**Architecture:** Protocol clock trackers map RTP/RTCP or MPEG-TS PCR/PTS into a strong canonical nanosecond running-time type. A graph-scoped coordinator selects and atomically releases one playback epoch. One steady master clock and one A/V scheduler pace both streams, with bounded audio resampling correction and planner-directed video recovery. RTP SR/timestamps and MPEG-TS PCR/PTS/DTS are derived from the same canonical epoch.

**Technology:** C++20, FFmpeg libavformat/libavcodec/libswresample, existing MediaGraph runtime, CMake/CTest, PowerShell acceptance harness, VLC and ffprobe.

## Global Constraints

- Supported synchronized topologies are exactly separate RTP A/V input to separate RTP A/V output and MPEG-TS input to MPEG-TS output.
- Separate RTP input to MPEG-TS output is rejected by the planner; no compatibility path is introduced.
- `MediaAvSyncPlan` is complete and immutable after planning. Builders and nodes must reject missing values and never provide defaults, fallback clocks, arrival-time timestamps, first-packet-zero rebasing, DTS-as-PTS, or independent stream epochs.
- Canonical media time is signed nanoseconds represented by a strong type; raw protocol ticks always travel with an explicit time base.
- `MediaSteadyMasterClock` is the only pacing clock. `system_clock` is used only once to establish a shared RTP NTP epoch and cannot alter an active epoch.
- The startup coordinator releases audio and video atomically at one common decodable point; it never releases one stream alone.
- Locked audio correction is limited to plus or minus 1000 ppm. Recovery correction is limited to plus or minus 5000 ppm. Hard errors trigger group reacquisition.
- Output RTP streams use distinct SSRC values, the same non-sensitive CNAME, one shared NTP epoch, and periodic RTCP SR.
- TS program, audio/video PID, PCR PID, time bases, and PCR interval are planner products.
- `PacketStartGateNode` remains only for valid video-only paths. `AvPacketStartBarrierNode`, per-mux synchronization pacing, mux `+1 tick` repair, and local RTP/TS epoch decisions are removed from synchronized A/V paths.
- CMake does not configure or copy external runtime libraries. Required FFmpeg DLLs are copied manually beside executables for tests.
- All modified text files are UTF-8 with CRLF. Unrelated `.codex`, FFmpeg header, `AGENTS.md`, `out`, and unrelated documentation changes are never staged.
- Model/layout changes require `cmake --build out/build/x64-debug --config Debug --target clean` followed by a full rebuild before runtime conclusions.

## Task 1: Strong running time and protocol timestamp unwrap

**Files:**

- Create `src/internal/graph/time/MediaRunningTime.h`
- Create `src/internal/graph/time/MediaTimestampUnwrapper.h`
- Create `src/internal/graph/time/MediaTimestampUnwrapper.cpp`
- Modify `tests/unit/test_core.cpp`

**RED:** Add deterministic tests for strong nanosecond arithmetic, checked rational rescale, RTP 32-bit wrap, PTS/DTS 33-bit wrap, PCR wrap, invalid backward movement, and generation reset. Run `out/build/x64-debug/Debug/media_transcode_core_tests.exe`; confirm failures identify missing time types/unwrapper behavior.

**GREEN:** Implement overflow-checked rescale and a configurable-bit timestamp unwrapper with explicit discontinuity results. No clock reading, pacing, or policy thresholds belong here. Rebuild and run the core target until green.

**REFACTOR:** Ensure RTP, PTS, and PCR use the same implementation and no semantically equivalent helpers are introduced.

**Commit:** `feat: add canonical running time primitives`

## Task 2: Complete planner-owned A/V synchronization contract

**Files:**

- Create `src/internal/graph/planner/avsync/MediaAvSyncPlan.h`
- Create `src/internal/graph/planner/avsync/MediaAvSyncPlanValidator.h`
- Create `src/internal/graph/planner/avsync/MediaAvSyncPlanValidator.cpp`
- Create `src/internal/graph/planner/avsync/MediaAvSyncPlanner.h`
- Create `src/internal/graph/planner/avsync/MediaAvSyncPlanner.cpp`
- Modify `src/internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h`
- Modify `src/internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.cpp`
- Modify `src/internal/graph/planner/realtime/MediaRealtimeRequestValidator.cpp`
- Modify `tests/unit/test_planner.cpp`
- Modify `tests/unit/test_realtime_rtp_graph.cpp`

**RED:** Test both supported topologies, explicit rejection of separate RTP to TS, complete RTP identities/clock rates/CNAME/SR parameters, complete TS program/PID/PCR parameters, ordered thresholds, and 1000/5000 ppm bounds. Test that every missing required field fails before graph construction.

**GREEN:** Add one complete `MediaAvSyncPlan` to the realtime plan and make the specialized planner classify the topology and validate every new synchronization value. The pre-existing barrier/mux fields remain byte-for-byte only as a temporary build seam because their replacement scheduler and protocol clocks are implemented in later tasks; Task 2 must not add adapters, fallback, derived defaults, or new consumers for those fields. Task 12 removes the legacy fields and path atomically when all replacement runtime components exist.

**REFACTOR:** Keep request validation, topology classification, and A/V sync policy construction in separate files while the realtime planner remains the sole orchestration/decision owner.

**Commit:** `feat: plan complete av synchronization policy`

## Task 3: Application-owned RTP/RTCP ingress and source clock

FFmpeg's public RTP API exposes received SR side data but not incoming SDES CNAME, and privately owns both RTP/RTCP sockets. Custom AVIO is not used by the `AVFMT_NOFILE` RTP demuxer, and a loopback relay would add a second UDP buffer, TOCTOU port allocation, and changed RTCP semantics. Guaranteed synchronization therefore replaces the raw-RTP FFmpeg transport/depacketizer boundary instead of adding a side reader or fallback. Task 3 is complete only when 3A, 3B, and 3C pass one combined review gate.

### Task 3A: RTP/RTCP transport and protocol evidence

**Files:**

- Create `src/internal/graph/runtime/network/MediaSocketRuntime.h`
- Create `src/internal/graph/runtime/network/MediaSocketRuntime.cpp`
- Create `src/internal/graph/runtime/network/MediaUdpSocket.h`
- Create `src/internal/graph/runtime/network/MediaUdpSocket.cpp`
- Create `src/internal/graph/protocol/rtp/MediaRtpUdpTransport.h`
- Create `src/internal/graph/protocol/rtp/MediaRtpUdpTransport.cpp`
- Create `src/internal/graph/protocol/rtp/MediaRtpPacketParser.h`
- Create `src/internal/graph/protocol/rtp/MediaRtpPacketParser.cpp`
- Create `src/internal/graph/protocol/rtp/MediaRtcpCompoundParser.h`
- Create `src/internal/graph/protocol/rtp/MediaRtcpCompoundParser.cpp`
- Create `src/internal/graph/protocol/rtp/MediaRtcpClockEvidence.h`
- Create `src/internal/graph/protocol/rtp/MediaRtcpSenderReportTracker.h`
- Create `src/internal/graph/protocol/rtp/MediaRtcpSenderReportTracker.cpp`
- Modify `src/internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h`
- Modify `src/internal/graph/planner/realtime/MediaRealtimeInputPlanner.cpp`
- Modify `tests/unit/test_node.cpp`
- Modify `tests/unit/test_event_driven_runtime.cpp`

**RED:** Test strict RTP header parsing and RTCP compound SR/SDES/BYE parsing, including version, padding, length, multiple chunks/items, alignment, unknown packet types, truncation, and malformed packets. Test same-SSRC media/SR/CNAME readiness, missing evidence, SSRC/CNAME change, NTP regression, expiry, and BYE. Test socket lifecycle, bind failure, cancellation, sparse/simultaneous RTP+RTCP, stop/reset/abort, and no spin.

**GREEN:** Each raw input owns one RAII RTP/RTCP socket pair. The event-driven transport blocks on RTP, RTCP, and cancellation; steady receive time is used only for timeout/health metrics. Planner supplies bind family/address, exact RTP/RTCP ports, payload/clock identity, receive buffer, datagram limit, reorder policy, and SR/CNAME timeouts. Nodes add no port, timeout, buffer, multicast, or clock defaults.

**REFACTOR:** Keep socket ownership, datagram classification, RTP parsing, RTCP parsing, and evidence tracking as separate responsibilities.

**Commit:** `feat: own RTP RTCP ingress and clock evidence`

### Task 3B: Registry-driven RTP depacketization and graph boundary

**Files:**

- Create `src/internal/graph/protocol/rtp/MediaRtpReorderBuffer.h`
- Create `src/internal/graph/protocol/rtp/MediaRtpReorderBuffer.cpp`
- Create `src/internal/graph/protocol/rtp/MediaRtpDepacketizer.h`
- Create `src/internal/graph/protocol/rtp/MediaRtpDepacketizerFactory.h`
- Create `src/internal/graph/protocol/rtp/MediaRtpDepacketizerFactory.cpp`
- Create focused H264, HEVC, AAC MPEG4-GENERIC, and Opus depacketizers under `src/internal/graph/protocol/rtp/depacketizer/`
- Create `src/internal/graph/nodes/input/MediaRawRtpStreamDescriptorFactory.h`
- Create `src/internal/graph/nodes/input/MediaRawRtpStreamDescriptorFactory.cpp`
- Modify `src/internal/graph/nodes/input/RawRtpInputNode.h`
- Modify `src/internal/graph/nodes/input/RawRtpInputNode.cpp`
- Modify `src/internal/graph/runtime/ffmpeg/FFmpegFormatContextBuffer.h`
- Modify `src/internal/graph/runtime/ffmpeg/FFmpegFormatContextBuffer.cpp`
- Modify `src/internal/graph/builder/realtime/MediaRealtimeOptionApplier.cpp`
- Modify `src/internal/graph/builder/realtime/MediaRealtimeRtpTranscodeGraphBuilder.cpp`
- Modify `tests/unit/test_node.cpp`
- Modify `tests/unit/test_realtime_rtp_graph.cpp`

**RED:** Test reorder, duplicate, gap, sequence wrap, SSRC/payload change, incomplete access-unit loss, and exact RTP timestamp preservation. Cover H264 single NAL/STAP-A/FU-A, HEVC single NAL/AP/FU, strict AAC AU headers/config, and Opus packet duration; malformed and unsupported packetization modes must fail. Assert raw RTP graphs no longer contain an SDP input, `DemuxNode`, `StreamSplitNode`, or private-socket side reader.

**GREEN:** `RawRtpInputNode` directly emits an immutable stream snapshot, complete FFmpeg packet access units, clock evidence, and structured RTP/RTCP events. FFmpeg starts at `AVPacket + AVCodecParameters`; the raw path no longer opens an RTP URL or synthetic SDP. The depacketizer factory supports exactly the codecs/modes already accepted by the raw-RTP planner and rejects unsupported modes during planning.

**REFACTOR:** Parser, reorder buffer, codec depacketizers, descriptor creation, and graph I/O remain independent. No codec-specific branch belongs in the transport or node lifecycle.

**Commit:** `feat: depacketize planned RTP streams in graph`

### Task 3C: Cross-stream SR/CNAME source-clock mapping

**Files:**

- Create `src/internal/graph/protocol/rtp/MediaRtpSourceClockMapper.h`
- Create `src/internal/graph/protocol/rtp/MediaRtpSourceClockMapper.cpp`
- Create `src/internal/graph/protocol/rtp/MediaRtpClockGroupValidator.h`
- Create `src/internal/graph/protocol/rtp/MediaRtpClockGroupValidator.cpp`
- Modify `src/internal/graph/nodes/input/RawRtpInputNode.cpp`
- Modify `tests/unit/test_node.cpp`
- Modify `tests/unit/test_realtime_rtp_graph.cpp`

**RED:** Test matching-CNAME audio/video mapping, RTP wrap, bounded SR jitter/rate estimation, NTP regression, invalid slope, timeout/extrapolation expiry, SSRC/CNAME change, and group generation reset. Confirm missing SR/CNAME never uses SDP, wall clock, or arrival time and prevents startup readiness.

**GREEN:** Map unwrapped RTP through valid SR RTP/NTP pairs into a common source domain. Accept a group only when observed media SSRC, SR SSRC, and non-empty SDES CNAME match for each stream and audio/video CNAME octets are identical. Accepted SR updates refine rate/offset without jumping published running time; expiry requests group reacquisition.

**REFACTOR:** Transport supplies datagrams, parser supplies evidence, tracker maintains per-stream validity, group validator establishes identity, and mapper only converts timestamps.

**Commit:** `feat: map RTP sender clocks into common source time`

## Task 4: MPEG-TS program clock evidence

Implementation is governed by `docs/superpowers/plans/2026-07-12-mpeg-ts-program-clock-input.md`. The expanded plan replaces the original narrow task because FFmpeg's public packet boundary does not expose PCR/adaptation evidence; the approved design uses one observed custom-AVIO byte owner without modifying FFmpeg.

## Task 5: Canonical mapper and generation isolation

**Files:**

- Create `src/internal/graph/time/MediaMappedTimestamp.h`
- Create `src/internal/graph/time/MediaCanonicalTimeMapper.h`
- Create `src/internal/graph/time/MediaCanonicalTimeMapper.cpp`
- Create `src/internal/graph/sync/MediaAvSyncError.h`
- Modify `tests/unit/test_core.cpp`

**RED:** Test different protocol time bases mapping to the same running time, decode/presentation separation, large values without overflow, unmappable source evidence, generation reset, and rejection of old-generation data.

**GREEN:** Implement the protocol-neutral mapper and structured error context. The mapper only converts and tags time; it does not pace, drop, or decide discontinuities.

**REFACTOR:** Remove any newly redundant rescale/mapping helper encountered in synchronized code paths and route all callers through the canonical types.

**Commit:** `feat: add generation-aware canonical time mapping`

## Task 6: Atomic common startup coordinator

**Files:**

- Create `src/internal/graph/sync/MediaPlaybackEpoch.h`
- Create `src/internal/graph/sync/MediaAvSyncStateMachine.h`
- Create `src/internal/graph/sync/MediaAvSyncStateMachine.cpp`
- Create `src/internal/graph/sync/MediaAvStartupCoordinator.h`
- Create `src/internal/graph/sync/MediaAvStartupCoordinator.cpp`
- Create `src/internal/graph/nodes/sync/MediaAvStartupCoordinatorNode.h`
- Create `src/internal/graph/nodes/sync/MediaAvStartupCoordinatorNode.cpp`
- Modify `src/internal/graph/model/MediaNodeKind.h`
- Modify `src/internal/graph/runtime/factory/MediaNodeRuntimeFactory.cpp`
- Modify `tests/unit/test_node.cpp`
- Modify `tests/unit/test_event_driven_runtime.cpp`

**RED:** Test audio-first/video-first arrival, required keyframe, common-window selection, sample-accurate audio trim, preroll, single atomic release, startup timeout, EOF/error before release, stop/reset/abort, and old-generation purge.

**GREEN:** Implement the explicit startup states and publish one `MediaPlaybackEpoch{sourceStart, masterRelease, generation}` only when both streams are armed.

**REFACTOR:** Keep state transition validation, buffered-media selection, and graph I/O in separate responsibilities.

**Commit:** `feat: coordinate atomic av startup`

## Task 7: Audio drift servo and resampler execution

**Files:**

- Create `src/internal/graph/sync/MediaAudioDriftServo.h`
- Create `src/internal/graph/sync/MediaAudioDriftServo.cpp`
- Create `src/internal/graph/sync/MediaAudioCorrection.h`
- Modify `src/internal/graph/nodes/audio/AudioResampleNode.h`
- Modify `src/internal/graph/nodes/audio/AudioResampleNode.cpp`
- Modify `src/internal/graph/builder/segments/MediaAudioBranchSegmentBuilder.cpp`
- Modify `tests/unit/test_core.cpp`
- Modify `tests/unit/test_node.cpp`

**RED:** Test deadband, phase/frequency filtering, PI anti-windup, slew limiting, normal clamp at 1000 ppm, recovery clamp at 5000 ppm, recovery entry/exit hysteresis, hard discontinuity, reset, and conversion from requested ppm/window to exact `swr_set_compensation` sample delta/distance.

**GREEN:** The servo calculates immutable correction commands from plan and measured playhead error. `AudioResampleNode` only applies commands over explicit sample windows and advances timestamps by actual produced samples.

**REFACTOR:** Keep policy/control math out of the FFmpeg node and eliminate fixed compensation values or timestamp rewriting.

**Commit:** `feat: apply bounded audio drift correction`

## Task 8: Video synchronization controller

**Files:**

- Create `src/internal/graph/sync/MediaVideoSyncController.h`
- Create `src/internal/graph/sync/MediaVideoSyncController.cpp`
- Create `src/internal/graph/sync/MediaVideoSyncDecision.h`
- Modify `src/internal/graph/nodes/video/VideoFrameRateNode.h`
- Modify `src/internal/graph/nodes/video/VideoFrameRateNode.cpp`
- Modify `tests/unit/test_core.cpp`
- Modify `tests/unit/test_node.cpp`

**RED:** Test early hold, normal late display, recoverable non-key drop, optional repeat, keyframe preservation, consecutive recovery limit, and reacquisition request. Verify later timestamps are never shifted to mask lateness.

**GREEN:** Implement a pure controller driven only by plan and measured error; the video execution node applies the returned action.

**REFACTOR:** Remove local hardcoded lateness thresholds and duplicate frame-timing decisions.

**Commit:** `feat: control video presentation against master time`

## Task 9: Shared master clock and single A/V output scheduler

**Files:**

- Create `src/internal/graph/time/MediaMasterClock.h`
- Create `src/internal/graph/time/MediaSteadyMasterClock.h`
- Create `src/internal/graph/time/MediaSteadyMasterClock.cpp`
- Create `src/internal/graph/sync/MediaScheduledAccessUnit.h`
- Create `src/internal/graph/nodes/sync/MediaAvOutputSchedulerNode.h`
- Create `src/internal/graph/nodes/sync/MediaAvOutputSchedulerNode.cpp`
- Modify `src/internal/graph/runtime/context/MediaGraphExecutionContext.h`
- Modify `src/internal/graph/runtime/context/MediaGraphExecutionContext.cpp`
- Modify `src/internal/graph/model/MediaNodeKind.h`
- Modify `src/internal/graph/runtime/factory/MediaNodeRuntimeFactory.cpp`
- Modify `tests/unit/test_event_driven_runtime.cpp`
- Modify `tests/unit/event_runtime_multi_input_tests.cpp`

**RED:** With a fake monotonic clock, test atomic epoch start, A/V ordering, early waits without busy spinning, fair simultaneous inputs, sparse/busy/EOF coexistence, stop/abort wakeup, deadline interruption, and wall-clock jump immunity.

**GREEN:** Store one master clock/sync-group runtime per planned group and schedule both streams through one node. Scheduler waits through existing event-driven wakeup primitives and outputs complete scheduled access units.

**REFACTOR:** Delete graph-level per-mux pacing-clock ownership from synchronized A/V execution; retain byte throttling only as transport rate limiting with no media-time authority.

**Commit:** `feat: schedule av output on one master clock`

## Task 10: RTP output clock, RTCP SR, and SDP synchronization contract

**Files:**

- Create `src/internal/graph/time/MediaSharedNtpEpoch.h`
- Create `src/internal/graph/time/MediaSharedNtpEpoch.cpp`
- Create `src/internal/graph/protocol/rtp/MediaRtpOutputClockMapper.h`
- Create `src/internal/graph/protocol/rtp/MediaRtpOutputClockMapper.cpp`
- Create `src/internal/graph/protocol/rtp/MediaRtcpSenderReportGenerator.h`
- Create `src/internal/graph/protocol/rtp/MediaRtcpSenderReportGenerator.cpp`
- Modify `src/internal/graph/nodes/mux/RtpMuxFfmpegSession.h`
- Modify `src/internal/graph/nodes/mux/RtpMuxFfmpegSession.cpp`
- Modify `src/internal/graph/nodes/mux/RtpMuxNode.h`
- Modify `src/internal/graph/nodes/mux/RtpMuxNode.cpp`
- Modify `src/internal/graph/nodes/output/RtpOutputNode.cpp`
- Modify `src/internal/graph/nodes/output/SdpWriterNode.cpp`
- Modify `tests/unit/test_node.cpp`
- Modify `tests/unit/test_realtime_rtp_graph.cpp`

**RED:** Test distinct planned SSRC values, common CNAME, shared NTP epoch, periodic SR, audio/video RTP timestamp conversion from canonical time, SR RTP/NTP correspondence, SDP media/clock/control attributes, wrap, and rejection of missing output-clock plan.

**GREEN:** Make RTP mux/output consume scheduled canonical timestamps and planned RTCP identities. Remove per-stream rebase, `+1 tick` repair, independent wall-clock epoch, and media pacing sleeps.

**REFACTOR:** Keep FFmpeg mux session ownership, RTCP clock generation, protocol I/O, and mux state machine separate.

**Commit:** `feat: derive RTP output clocks from shared epoch`

## Task 11: MPEG-TS output clock generation

**Files:**

- Create `src/internal/graph/protocol/mpegts/MediaTsOutputClockGenerator.h`
- Create `src/internal/graph/protocol/mpegts/MediaTsOutputClockGenerator.cpp`
- Modify `src/internal/graph/nodes/mux/FileMuxNode.h`
- Modify `src/internal/graph/nodes/mux/FileMuxNode.cpp`
- Modify `src/internal/graph/nodes/output/FileOutputNode.cpp`
- Modify `tests/unit/test_node.cpp`
- Modify `tests/unit/test_realtime_rtp_graph.cpp`

**RED:** Test planner-selected program/stream/PCR PID, monotonic PCR, maximum PCR interval, canonical PTS/DTS conversion, valid PTS/PCR relation, timestamp wrap, and no copying of input PCR jitter.

**GREEN:** Feed planned stream/program metadata and scheduled access units into the TS mux. Generate PCR/PTS/DTS from one output epoch without mux-side policy selection.

**REFACTOR:** Keep protocol timestamp generation outside FFmpeg ownership and mux lifecycle code.

**Commit:** `feat: derive MPEG-TS clocks from canonical timeline`

## Task 12: Replace synchronized graph wiring and remove obsolete authorities

**Files:**

- Modify `src/internal/graph/builder/realtime/MediaRealtimeOptionApplier.h`
- Modify `src/internal/graph/builder/realtime/MediaRealtimeOptionApplier.cpp`
- Modify `src/internal/graph/builder/realtime/MediaRealtimeRtpTranscodeGraphBuilder.cpp`
- Modify `src/internal/graph/nodes/packet/PacketStartGateNode.cpp`
- Delete `src/internal/graph/nodes/packet/AvPacketStartBarrierNode.h`
- Delete `src/internal/graph/nodes/packet/AvPacketStartBarrierNode.cpp`
- Delete `src/internal/graph/runtime/MediaGraphPacingClock.h`
- Delete `src/internal/graph/runtime/MediaGraphPacingClock.cpp`
- Modify `tests/unit/test_builder.cpp`
- Modify `tests/unit/test_realtime_rtp_graph.cpp`

**RED:** Assert the two supported A/V graphs contain the clock mapper, coordinator, servo/controller, and one shared scheduler; assert they do not contain the old barrier or per-mux pacing authority. Assert video-only paths still use the single-stream gate and mixed unsupported topology fails in the planner.

**GREEN:** Wire the complete `MediaAvSyncPlan` into graph options and runtime components. Remove obsolete synchronized behavior rather than retaining compatibility branches.

**REFACTOR:** Split builder helpers by input clock, synchronization segment, and output protocol; keep builder topology-only.

**Commit:** `refactor: replace legacy av pacing path`

## Task 13: Lifecycle, discontinuity, metrics, and deterministic faults

**Files:**

- Create `src/internal/graph/sync/MediaAvSyncMetrics.h`
- Create `src/internal/graph/sync/MediaAvSyncMetrics.cpp`
- Create `src/internal/graph/sync/MediaAvDiscontinuityCoordinator.h`
- Create `src/internal/graph/sync/MediaAvDiscontinuityCoordinator.cpp`
- Modify `src/internal/graph/runtime/MediaGraphRuntimeReport.h`
- Modify `src/internal/graph/runtime/MediaGraphRuntimeReport.cpp`
- Modify `tests/unit/test_event_driven_runtime.cpp`
- Modify `tests/unit/event_runtime_ffmpeg_ownership_tests.cpp`
- Modify `tests/unit/event_runtime_multi_input_tests.cpp`
- Modify `tests/performance/test_performance.cpp`

**RED:** Test every legal startup/running/recovering/reacquiring/stop/reset transition, group-generation purge, queue overflow discontinuity, SR/PCR interruption, packet loss/jitter scripts, keyframe delay, worker error propagation, progress-stall detection, and metric p50/p95/p99/max aggregation.

**GREEN:** Add generation-aware metrics and one group discontinuity coordinator that resets mapper/servo/scheduler state together. Fault injection remains test-side and deterministic.

**REFACTOR:** Ensure runtime report reads immutable snapshots and does not own synchronization policy.

**Commit:** `test: harden av sync lifecycle and fault reporting`

## Task 14: Direct RTP and MPEG-TS acceptance tooling

**Files:**

- Modify `tests/acceptance/Priority5Acceptance.psm1`
- Modify `tests/acceptance/run_priority4_realtime_gate.ps1`
- Modify `tests/acceptance/test_priority5_reporting.ps1`
- Create `tests/acceptance/run_av_sync_gate.ps1`
- Create `tests/acceptance/inspect_rtp_sync.ps1`
- Create `tests/acceptance/inspect_mpegts_sync.ps1`

**RED:** Extend script contract tests first. Require direct generated-SDP VLC playback for RTP, direct TS VLC playback for MPEG-TS, process-scoped CPU for `media_transcode_realtime_video_cli`, exact stream/decode/stall/error gates, and structured sync/protocol metrics. The harness must reject capture-remux playback and missing SR/CNAME/PCR evidence.

**GREEN:** Implement repeatable hardware-only RTP and MPEG-TS scenarios with flash/beep skew analysis, packet/protocol inspection, and commands recorded exactly as run. Keep dependency copying in the harness/setup command and outside CMake.

**REFACTOR:** Share process monitoring/report serialization while keeping RTP and TS protocol inspection separate.

**Commit:** `test: add direct av synchronization acceptance gates`

## Task 15: Clean verification, evidence, quality score, and delivery

**Files:**

- Modify `plan.md`
- Create or update `docs/completed/quality_priority_5_av_sync.md`
- Modify `QUALITY_SCORE.md`
- Update `docs/av_sync_system_design.md` only if implementation-approved interface names changed

**Verification order:**

1. Normalize modified text to UTF-8 CRLF and run `git diff --check`.
2. Fully clean and rebuild x64-debug with all configured deterministic/integration/hardware/performance targets.
3. Copy required DLLs from `D:\mabs\local64\bin-video` beside test and CLI executables without changing CMake.
4. Run all CTest tiers and repeat concurrency/runtime tests 20 times.
5. Run hardware-only separate RTP A/V to separate RTP A/V for 2 minutes. Open the generated SDP directly in VLC for uninterrupted review, with no other integration/acceptance workload running concurrently.
6. Run hardware-only MPEG-TS to MPEG-TS for 2 minutes and play the TS directly in VLC, again without competing test workloads.
7. Enforce exit code 0, complete A/V streams, zero decode/worker/drop/continuous-frame errors, no progress stall above 5 seconds, final absolute drift at most 100 ms, hardware average CPU at most 5%, and collect P95 CPU/memory/thread/queue/latency metrics.
8. Enforce the stronger A/V targets where the flash/beep source supports them: startup at most 40 ms, p95 at most 20 ms, p99 at most 40 ms, and measured drift slope at most 1 ms/hour.
9. Re-run every command documented in the completion report verbatim in PowerShell.
10. Update `QUALITY_SCORE.md` using observed evidence without preselecting a score.

**Review and delivery:** Perform a global responsibility/fallback/copy/RAII review, fix all findings, commit and push. Request a fresh independent whole-branch Agent review. Fix, retest, push, and repeat until PASS. Update PR #21 with real evidence and report remaining risks.

**Commit:** `docs: complete priority 5 av sync evidence`

## Task Review Gate

After every task:

1. Record the task base and head commits.
2. Generate a diff review package for that exact range.
3. Dispatch a fresh reviewer with the task brief, implementation report, review package, and Global Constraints.
4. Require both specification-compliance and code-quality PASS.
5. Return all Critical/Important findings to a fixer, rerun the covering tests, and request re-review.
6. Append the clean task result to `.superpowers/sdd/progress.md` before starting the next task.
