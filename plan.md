# Realtime Cross-Layout and MPEG-TS/RTP Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Support every realtime input with separate RTP, MPEG-TS/UDP, or standards-compliant MPEG-TS/RTP output while preserving the current A/V synchronization quality.

**Architecture:** Replace paired input/output topology decisions with an explicit input clock plan and an independent output adapter plan. All A/V paths share the existing canonical scheduler; Project MPEG-TS remains the only TS mux, with either a UDP datagram sink or an RTP/AVP MP2T sink using PT 33, 90 kHz, RTP/RTCP, and SDP.

**Tech Stack:** C++20, project DAG planner/builder/runtime, project MPEG-TS mux, project RTP/RTCP transport, FFmpeg input and CLI tooling, Visual Studio 2026 bundled CMake/Ninja.

## Global Constraints

- Preserve the current RTP-to-RTP and MPEG-TS/UDP-to-MPEG-TS/UDP A/V startup, drift correction, recovery, telemetry, and subjective playback quality.
- Planner owns all decisions and fully resolves every input clock, output protocol, endpoint, codec, packet-layout, timing, and capacity fact.
- Runtime nodes do not choose protocols, provide defaults, use packet-arrival clocks, or fall back to alternate muxers.
- Keep one canonical scheduler and one Project MPEG-TS mux path; do not add FFmpeg remux or a second pacing authority.
- Do not add, restore, or adapt CI, CTest, unit, integration, acceptance, hardware, or performance test systems.
- Validate only with the required clean-first x64 Debug rebuild and real CLI/FFmpeg/VLC media chains.
- All edited text is UTF-8 with CRLF endings.
- Update this file as tasks complete, add concise completion evidence under `docs/completed`, and update `QUALITY_SCORE.md`.
- Commit and push every completed task to `codex/realtime-cross-layout`.

---

### Task 1: Make output encapsulation and transport independent

**Files:**
- Modify: `src/internal/graph/model/RealtimeStreamLayout.h`
- Create: `src/internal/graph/model/MediaOutputTransportKind.h`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeRequestClassifier.{h,cpp}`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeRequestValidator.cpp`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeOutputPolicyPlanner.{h,cpp}`
- Modify: `tools/realtime_video_cli/main.cpp`

**Interfaces:**
- Add `enum class MediaOutputTransportKind : std::uint8_t { UdpDatagrams = 0, RtpAvp = 1 };`.
- Add `std::optional<MediaOutputTransportKind> transport` to `MediaRealtimeOutputConfig`.
- Add classifier predicates `udpOutput()` and `rtpAvpOutput()`; keep layout predicates limited to separate versus muxed encapsulation.
- CLI requires `--output-transport udp|rtp`; no implicit transport is accepted.

- [x] **Step 1: Preserve existing enum values and add the output protocol model**

  Give all existing input/output layout enum members explicit numeric values matching their current order. This task establishes `MediaOutputTransportKind` as the single shared enum; Task 4 migrates Project MPEG-TS planning to it and removes the legacy transport enum.

- [x] **Step 2: Parse the explicit CLI transport**

  Add `requiredRealtimeOutputTransport()`, register `--output-transport`, and populate `request.output.transport`. Keep the accepted combinations exact:

  ```text
  separate + rtp = separate elementary-stream RTP
  mpegts  + udp = MPEG-TS/UDP
  mpegts  + rtp = MPEG-TS/RTP MP2T
  separate + udp = rejected
  ```

- [x] **Step 3: Separate input and output validation**

  Remove the fixed topology whitelist and validate supported input classification independently from output classification. Preserve every input-specific requirement. Apply output requirements by the selected layout/protocol:

  ```text
  separate/rtp: host, even base RTP port, packet size, SDP
  mpegts/udp: explicit udp://host:port URL
  mpegts/rtp: host, even base RTP port, packet size, SDP
  ```

  RTP/RTCP uses `basePort` and `basePort + 1`; planner validation rejects overflow, invalid parity, and collisions.

- [x] **Step 4: Refactor URL planning without compatibility branches**

  `MediaRealtimeOutputPolicyPlanner::planUrls()` produces only endpoint facts valid for the selected transport. It must not infer transport from URL scheme or layout. UDP MPEG-TS retains its UDP URL; both RTP modes use the existing numeric host/base-port path.

- [x] **Step 5: Perform static verification and commit**

  Run `git diff --check`, inspect all output classification call sites, verify UTF-8/CRLF, update this task to complete, then commit:

  ```text
  feat(realtime): separate output layout and protocol
  ```

---

### Task 2: Orthogonalize A/V input-clock and output planning

**Files:**
- Modify: `src/internal/graph/planner/avsync/MediaAvSyncPlan.h`
- Modify: `src/internal/graph/planner/avsync/MediaAvSyncPlanner.cpp`
- Modify: `src/internal/graph/planner/avsync/MediaAvSyncPlanValidator.cpp`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeAvSyncAssemblyPlan.h`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeAvSyncPlanningFactsResolver.cpp`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlanner.cpp`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlanValidator.cpp`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.cpp`

**Interfaces:**
- Replace `MediaAvSyncTopology` consumption with `MediaAvSyncSourceClockMode`.
- Preserve `RtpSenderReports = 0` and `MpegTsPcr = 1`; add `DemuxTimestamps = 2`.
- Extend `MediaAvSyncInputClockPlan` with `MediaDemuxTimestampInputClockAssemblyPlan`.
- Continue using `MediaAvSyncOutputAdapterKind` independently; no cross-product enum is added.

- [x] **Step 1: Make the synchronization plan input-clock-specific**

  Remove the paired topology field and all `RtpToRtp`/`TsToTs` decisions. `MediaAvSyncPlan` contains exactly one source clock mode plus only the matching input protocol facts:

  ```text
  RtpSenderReports -> rtp input clock facts
  MpegTsPcr -> ts input clock facts
  DemuxTimestamps -> prepared stream time bases and discontinuity policy
  ```

  RTP output identity and TS output mux facts move out of input-clock selection and are planned from the output request.

- [x] **Step 2: Plan output facts from output selection**

  Resolve Project MPEG-TS codec/layout facts whenever output layout is MPEG-TS, regardless of input. Resolve scheduled elementary RTP packetization only for separate RTP output. The planner rejects missing H.264/AAC-LC 48 kHz stereo facts for both TS transports.

- [x] **Step 3: Add URL/RTSP demux timestamp planning**

  Add a focused plan containing explicit video/audio stream time bases, first-window maximum skew, timestamp regression limit, discontinuity threshold, and initial generation. Values come from prepared input and existing A/V startup/recovery policy; no runtime constants or packet-arrival fallback are allowed.

- [x] **Step 4: Build assembly variants only from source clock mode**

  RTP uses the existing RTP duration plan, TS uses the existing packet-duration plan, and URL/RTSP uses positive prepared packet/frame durations with validated demux PTS/DTS. Output layout must not affect input epoch, duration, or generation decisions.

- [x] **Step 5: Validate orthogonality**

  Update validators so each of the three input clock variants accepts each of the three output variants. Reject mismatched facts, duplicate output authority, incomplete protocol plans, and any old paired-topology dependency.

- [x] **Step 6: Perform static verification and commit**

  Search for every `MediaAvSyncTopology` use. Remove the type and its file only after no production consumer remains; do not leave an unused compatibility enum. Run `git diff --check`, verify UTF-8/CRLF, update this task, and commit:

  ```text
  refactor(avsync): decouple source clock and output protocol
  ```

---

### Task 3: Add strict URL/RTSP demux timestamp clock assembly

**Files:**
- Create: `src/internal/graph/time/MediaDemuxTimestampClockMapper.{h,cpp}`
- Create: `src/internal/graph/nodes/sync/MediaDemuxPacketClockBinderNode.{h,cpp}`
- Create: `src/internal/graph/builder/segments/MediaDemuxClockInputSegmentBuilder.{h,cpp}`
- Modify: `src/internal/graph/model/MediaNodeKind.{h,cpp}`
- Modify: `src/internal/graph/builder/segments/MediaRealtimeAvSyncNodeConfigurator.cpp`
- Modify: `src/internal/graph/runtime/factory/MediaRuntimeNodeFactory.{h,cpp}`
- Modify: `src/internal/graph/runtime/compilation/MediaGraphRuntimeCompiler.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- `::media::Result<MediaMappedTimestamp> MediaDemuxTimestampClockMapper::map(MediaScheduledStream stream, std::int64_t pts, AVRational timeBase, std::int64_t duration, std::uint64_t generation)` returns the existing canonical mapped-time/clock-evidence contract.
- `MediaDemuxPacketClockBinderNode` consumes prepared demux packets and emits packets carrying locked canonical presentation time.
- The segment builder consumes only `MediaDemuxTimestampInputClockAssemblyPlan`.

- [ ] **Step 1: Implement the protocol-neutral mapper**

  Establish the source epoch from the first valid common audio/video window. Rescale with checked arithmetic into canonical running time. Reject absent timestamps, invalid time bases, regression, excessive inter-stream startup skew, unexplained discontinuity, and generation changes according to the planner plan.

- [ ] **Step 2: Implement the binder node**

  Bind packet PTS/DTS and duration to canonical evidence before existing canonical input nodes. The node publishes locked/acquiring/reacquiring state through the same sync-group contract as RTP and TS paths and never synthesizes timestamps.

- [ ] **Step 3: Assemble URL/RTSP input**

  Add only the nodes needed for demux timestamp binding and connect them to the existing startup, canonicalization, drift, recovery, and scheduler segments. Do not duplicate those common segments.

- [ ] **Step 4: Register and compile the new node**

  Add stable node-kind serialization, runtime factory creation, sync-group injection, generation purge registration, and exact compiler cardinality checks for one demux clock binder per enabled stream.

- [ ] **Step 5: Perform static verification and commit**

  Inspect the URL/RTSP graph to prove it reaches the same canonical scheduler as RTP and TS. Run the mandated clean-first x64 Debug rebuild through `.agents/skills/building-with-vs2026/scripts/rebuild_debug.ps1`. Update this task and commit:

  ```text
  feat(avsync): map realtime demux timestamps
  ```

---

### Task 4: Plan Project MPEG-TS transport independently

**Files:**
- Modify: `src/internal/graph/protocol/mpegts/MediaTsMuxPlan.{h,cpp}`
- Modify: `src/internal/graph/planner/realtime/MediaProjectMpegTsOutputPlan.{h,cpp}`
- Create: `src/internal/graph/planner/realtime/MediaMpegTsRtpOutputPlan.{h,cpp}`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlanner.cpp`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlanValidator.cpp`
- Modify: `src/internal/graph/nodes/output/MediaProjectMpegTsPlanSourceNodePlanCodec.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Reuse `MediaOutputTransportKind`; its `UdpDatagrams = 0` value preserves the existing serialized UDP transport value and `RtpAvp = 1` is additive.
- `MediaProjectMpegTsRuntimeOutputPlan` contains the shared mux plan and a transport variant: `MediaMpegTsUdpOutputPlan` or `MediaMpegTsRtpOutputPlan`.
- `MediaMpegTsRtpOutputPlan` contains `MediaRtpUdpSenderConfig`, PT 33, 90 kHz, SSRC, base timestamp, CNAME, sender-report interval, maximum datagram bytes, TS packets per payload, and SDP identity.

- [ ] **Step 1: Extend the TS mux transport contract**

  Replace the MPEG-TS-specific transport enum with the shared output transport enum without changing the serialized UDP value. Validate one through seven complete TS packets per emitted batch for both transports. For RTP, calculate:

  ```text
  payload_capacity = maximum_datagram_bytes - 12
  maximum_ts_packets = min(7, floor(payload_capacity / 188))
  ```

  Reject zero capacity and any plan that would fragment a TS packet.

- [ ] **Step 2: Create the MP2T plan**

  Construct a single RTP/RTCP session with even RTP port and adjacent RTCP port, PT 33, clock rate 90000, planner-owned SSRC/base timestamp/CNAME, explicit SR interval, nonblocking pressure policy, and the confirmed SDP identity.

- [ ] **Step 3: Keep mux facts shared**

  `MediaProjectMpegTsOutputPlan::create()` resolves the same H.264/AAC/PCR/PID/continuity facts for UDP and RTP. Only datagram batching and transport plan differ. No protocol branch may change media timestamps.

- [ ] **Step 4: Encode/decode complete runtime plan facts**

  Update the typed node-plan codec to accept both stable transport enum values and all planner-owned transport facts. Decode must reject missing, out-of-range, or mismatched fields; it must not fill absent fields.

- [ ] **Step 5: Perform static verification and commit**

  Run `git diff --check`, verify plan encode/decode symmetry and UTF-8/CRLF, update this task, and commit:

  ```text
  feat(mpegts): plan UDP and RTP transports
  ```

---

### Task 5: Implement RTP/AVP MP2T, RTCP, and SDP

**Files:**
- Create: `src/internal/graph/protocol/rtp/MediaMpegTsRtpPacketizer.{h,cpp}`
- Create: `src/internal/graph/nodes/output/MediaMpegTsRtpDatagramSink.{h,cpp}`
- Create: `src/internal/graph/protocol/sdp/MediaMpegTsRtpSdpDescription.{h,cpp}`
- Create: `src/internal/graph/nodes/output/MediaMpegTsRtpSdpPublisherNode.{h,cpp}`
- Modify: `src/internal/graph/protocol/mpegts/MediaTsPacketBatchWriter.{h,cpp}`
- Modify: `src/internal/graph/protocol/mpegts/MediaTsMuxSession.{h,cpp}`
- Modify: `src/internal/graph/nodes/mux/ProjectMpegTsMuxSessionAdapter.{h,cpp}`
- Modify: `src/internal/graph/runtime/factory/MediaRuntimeNodeFactory.{h,cpp}`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Add a typed TS datagram sink interface whose write operation receives both complete TS bytes and the canonical `emitOnMaster` time.
- UDP adapts the existing `MediaOutputByteSink`; RTP uses `MediaMpegTsRtpDatagramSink`.
- `MediaMpegTsRtpPacketizer::packetize(tsBatch, emitOnMaster)` emits one RTP packet with PT 33 and timestamp `base + rescale(emitOnMaster, 90000)`.

- [ ] **Step 1: Carry canonical emission time to the transport sink**

  Replace the untyped TS batch write boundary with one typed datagram operation:

  ```cpp
  virtual ::media::Result<std::size_t> write(
      std::span<const std::uint8_t> completeTsPackets,
      MediaRunningTime emitOnMaster) = 0;
  ```

  `MediaTsMuxSession` supplies the already-planned emission time. The UDP adapter ignores no timing decision: it validates ordering and writes the same bytes to the existing sink. The RTP adapter uses the time only for RTP timestamp and SR correspondence, not pacing.

- [ ] **Step 2: Packetize MP2T**

  Validate nonempty payload, exact 188-byte alignment, maximum planned packet count, sequence continuity, SSRC, PT 33, and 90 kHz timestamp mapping. Marker remains false for continuous MPEG-TS. No TS parsing, remuxing, or packet fragmentation occurs here.

- [ ] **Step 3: Send RTP and RTCP with existing transport components**

  Reuse the project UDP sender, RTP sender state, shared NTP epoch, and RTCP sender-report construction. Maintain octet/packet counters, monotonic RTP timestamps, SR RTP/NTP correspondence, generation purge, bounded close, and preserved first failure.

- [ ] **Step 4: Publish MP2T SDP atomically**

  Generate one media description:

  ```text
  m=video <rtp-port> RTP/AVP 33
  a=rtpmap:33 MP2T/90000
  ```

  Include the planned connection address, session identity, CNAME/RTCP facts supported by the existing SDP model, UTF-8, and same-directory atomic replacement. Do not reuse the dual elementary-stream SDP formatter through conditionals.

- [ ] **Step 5: Perform static verification and commit**

  Inspect ownership and close paths for RAII, verify no new TS mux or pacing loop exists, run the mandated clean-first x64 Debug rebuild, update this task, and commit:

  ```text
  feat(rtp): send project MPEG-TS as MP2T
  ```

---

### Task 6: Assemble all nine production DAG paths

**Files:**
- Modify: `src/internal/graph/builder/segments/MediaScheduledMpegTsOutputSegmentBuilder.{h,cpp}`
- Modify: `src/internal/graph/builder/realtime/MediaRealtimeRtpTranscodeGraphBuilder.cpp`
- Modify: `src/internal/graph/builder/segments/MediaRealtimeAvSyncNodeConfigurator.cpp`
- Modify: `src/internal/graph/runtime/compilation/MediaGraphRuntimeCompiler.cpp`
- Modify: `src/internal/graph/runtime/compilation/MediaAvSyncRuntimeBootstrap.cpp`
- Modify: `src/internal/graph/sync/MediaAvSyncGroupRuntime.{h,cpp}`
- Modify: `src/internal/graph/nodes/sync/MediaAvStartupCoordinatorNodePreparation.cpp`

**Interfaces:**
- Input assembly switches only on `MediaAvSyncSourceClockMode`.
- Output assembly switches only on `MediaAvSyncOutputAdapterKind`, then the Project MPEG-TS transport variant.
- Compiler cardinality derives from the output plan, not the input clock.

- [ ] **Step 1: Remove paired-topology graph branching**

  Build RTP, TS, or demux input clock segments independently. Connect every input path to the same startup coordinator, canonical video/audio nodes, drift controller, recovery components, router, and scheduler.

- [ ] **Step 2: Select exactly one output authority**

  Assemble exactly one of:

  ```text
  two scheduled elementary RTP senders + dual-media SDP publisher
  one scheduled TS adapter + Project TS mux + UDP sink
  one scheduled TS adapter + Project TS mux + MP2T RTP sink + MP2T SDP publisher
  ```

- [ ] **Step 3: Make compiler validation plan-driven**

  Validate exact node counts for the selected output authority and reject all other output nodes. Source-clock node counts are validated separately. Register the same generation participants, scheduler, and shared clock for all outputs.

- [ ] **Step 4: Cover video-only production assembly**

  Ensure the same 3-by-3 request classification reaches the correct single-stream output without instantiating audio drift components. Missing video timestamps or unsupported TS codec/layout facts still fail in planning.

- [ ] **Step 5: Perform whole-code review and commit**

  Search for paired topology strings, input-driven output branching, fallback/default protocol facts, duplicate builders, and unused code. Run the mandated clean-first x64 Debug rebuild. Update this task and commit:

  ```text
  feat(realtime): assemble cross-layout output DAGs
  ```

---

### Task 7: Document and execute real-media acceptance

**Files:**
- Modify: `README.md`
- Modify: `ARCHITECTURE.md`
- Modify: `QUALITY_SCORE.md`
- Create: `docs/completed/realtime-cross-layout-mpegts-rtp.md`
- Modify: `plan.md`

**Interfaces:**
- README documents `--output-transport`.
- Acceptance evidence contains exact CLI/FFmpeg/VLC commands and results, excluding build commands.

- [ ] **Step 1: Update concise user and architecture documentation**

  Document all three output modes, MP2T PT 33/90 kHz/RTP-RTCP/SDP behavior, independent input/output planning, and examples for RTP input to MPEG-TS/UDP and MPEG-TS/RTP.

- [ ] **Step 2: Run the required build**

  Execute only:

  ```powershell
  & "C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe" `
      -NoProfile -ExecutionPolicy Bypass -File `
      ".agents\skills\building-with-vs2026\scripts\rebuild_debug.ps1"
  ```

  Require exit code 0 and both CLI artifacts. A timeout or missing artifact fails acceptance.

- [ ] **Step 3: Establish current same-layout baselines**

  Run separate RTP input to separate RTP output and MPEG-TS/UDP input to MPEG-TS/UDP output for 2 minutes each with the real CLI, FFmpeg source/receiver, and VLC. Record startup time, drops, duplicates, worker/decode errors, stalls, generations, discontinuities, A/V skew/drift, CPU, memory trend, and subjective synchronization.

- [ ] **Step 4: Run the remaining seven paths**

  Run every remaining input/output pair for 2 minutes:

  ```text
  URL/RTSP -> separate RTP
  URL/RTSP -> MPEG-TS/UDP
  URL/RTSP -> MPEG-TS/RTP
  separate RTP -> MPEG-TS/UDP
  separate RTP -> MPEG-TS/RTP
  MPEG-TS/UDP -> separate RTP
  MPEG-TS/UDP -> MPEG-TS/RTP
  ```

  For MP2T, open the generated SDP directly in both VLC and FFmpeg/ffprobe and confirm PT 33, 90 kHz, RTP/RTCP activity, decodable H.264/AAC, and stable A/V.

- [ ] **Step 5: Enforce bounded cleanup and evidence quality**

  Ensure every CLI, FFmpeg, ffprobe, and VLC process terminates. Save only concise commands/results in the completion document; temporary captures and scripts remain under `out/` and are not committed.

- [ ] **Step 6: Commission quality scoring**

  Dispatch a fresh subagent to review the complete branch against the industrial DAG scoring rubric. Apply evidence-backed score changes to `QUALITY_SCORE.md`, including A/V preservation, clock/output orthogonality, MP2T compliance, ownership, failure semantics, and remaining risks.

- [ ] **Step 7: Commit acceptance and documentation**

  Verify UTF-8/CRLF and `git diff --check`, mark all plan tasks complete, and commit:

  ```text
  docs(realtime): record cross-layout acceptance
  ```

---

### Task 8: Final review, PR, and independent approval

**Files:**
- Review: all changes from `origin/master...HEAD`
- Modify only if findings require fixes.

- [ ] **Step 1: Self-review the entire branch**

  Check every design requirement, output matrix entry, planner authority, RAII owner, node cardinality, close path, failure propagation, enum stability, UTF-8/CRLF file, documentation claim, and acceptance result. Continue fixing and rebuilding until no omission remains.

- [ ] **Step 2: List residual risks and optimization work**

  Record only evidence-backed remaining risks in the completion document and PR. Do not hide them by reducing validation or adding fallback.

- [ ] **Step 3: Commit and push all fixes**

  Keep every fix on `codex/realtime-cross-layout`, push the final branch, and verify the remote head matches local HEAD.

- [ ] **Step 4: Create the PR**

  Create a ready PR to `master` summarizing planner orthogonality, MP2T output, nine real-media paths, A/V regression results, quality score, and residual risks.

- [ ] **Step 5: Dispatch a fresh PR-review agent**

  A new agent reviews the PR diff and acceptance evidence against `AGENTS.md`, the approved design, and `QUALITY_SCORE.md`. It reports blocking/nonblocking findings and an explicit pass/fail decision.

- [ ] **Step 6: Close the review loop**

  If review fails, implement findings, repeat required real-media validation in proportion to the affected behavior, push updates, and ask the same reviewer to re-review. Finish only after an explicit pass with no blocking findings.
