# MPEG-TS Program Clock Input Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Feed real MPEG-TS PAT, PMT, PCR, discontinuity, PTS, and DTS evidence from one UDP byte owner into a planner-owned common source-clock mapping.

**Architecture:** An inner FFmpeg protocol AVIO owns the only UDP socket. A caller-owned outer AVIO observes the same immutable bytes before returning them unchanged to the explicit MPEG-TS demuxer. Protocol parsing, PSI inventory, evidence timeline, program-clock tracking, timestamp mapping, prepared-session ownership, demux orchestration, and planner decisions remain separate units.

**Tech Stack:** C++20, FFmpeg public AVIO/libavformat API, event-driven DAG runtime, CMake/CTest, Windows PowerShell verification.

## Global Constraints

- Supported synchronized MPEG-TS input uses 188-byte transport packets; other detected strides fail as unsupported.
- There is exactly one UDP socket and one byte consumer. No side socket, relay, duplicate reader, private FFmpeg field, or external FFmpeg patch is allowed.
- The outer AVIO returns successful inner reads byte-for-byte with identical order and length. Evidence failures are reported through session state, not by mutating a successful read result.
- Planner products include program, PMT/video/audio/PCR PIDs, packet size, I/O options, timeline limits, PCR thresholds, and lifecycle timeouts. Missing values fail before node construction.
- No arrival-time, wall-clock, latest-PCR, first-packet-zero, DTS-as-PTS, PID, program, epoch, or time-base fallback is allowed.
- PTS and DTS remain independent optional values and are mapped to signed-nanosecond `MediaRunningTime` through the selected PCR epoch.
- `RealtimeInputNode` only transfers a complete prepared binding. `MpegTsDemuxNode` orchestrates execution but does not select policy.
- Task 4 publishes MPEG-TS source timing/readiness only. Packet gating and atomic A/V release remain Task 6.
- CMake only references project files and never configures or copies external FFmpeg runtime libraries.
- All modified text files are UTF-8 without BOM and use CRLF. Model/layout changes require a clean target followed by a full rebuild.

---

### Task 1: Incremental TS framing and PSI inventory

**Files:**
- Create: `src/internal/graph/protocol/mpegts/MediaTsPacketParser.h`
- Create: `src/internal/graph/protocol/mpegts/MediaTsPacketParser.cpp`
- Create: `src/internal/graph/protocol/mpegts/MediaTsPsiSectionAssembler.h`
- Create: `src/internal/graph/protocol/mpegts/MediaTsPsiSectionAssembler.cpp`
- Create: `src/internal/graph/protocol/mpegts/MediaTsProgramInventory.h`
- Create: `tests/unit/mpeg_ts_packet_tests.cpp`
- Modify: `tests/unit/test_node.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces a transient `MediaTsPacketView { byteOffset, pid, payloadUnitStart, continuityCounter, discontinuity, optional pcr27Mhz, payloadSpan }` for each validated 188-byte packet through a synchronous packet-sink interface. The view cannot be retained after the call, so media payload is not copied.
- Produces immutable `MediaTsProgramInventorySnapshot` values containing PAT version, program number, PMT PID, PMT version, PCR PID, and elementary stream PID/type entries.
- Consumes arbitrary byte fragments while preserving absolute input offsets. Parser and assembler never select a program.

- [ ] **Step 1: Add real transport-byte fixtures and failing parser tests**

  Build valid-CRC PAT/PMT fixtures and adaptation-field PCR packets. Test byte-by-byte fragmentation, multiple packets per push, sync loss/resynchronization, invalid adaptation length, PCR base/extension, continuity counter, and discontinuity indicator. Run:

  ```powershell
  cmake --build out/build/x64-debug --config Debug --target media_transcode_node_tests
  out\build\x64-debug\media_transcode_node_tests.exe
  ```

  Expected RED: missing `MediaTsPacketParser` and `MediaTsPsiSectionAssembler` interfaces.

- [ ] **Step 2: Implement packet framing and adaptation parsing**

  Use a retained fragment buffer smaller than one packet plus absolute consumed-byte accounting. Accept only sync byte `0x47` at 188-byte stride. Parse PID, payload-unit-start, adaptation control, continuity counter, discontinuity flag, and PCR as `base * 300 + extension`; reject reserved/invalid lengths and impossible extension values.

- [ ] **Step 3: Implement PAT/PMT section assembly**

  Reassemble pointer-field and cross-packet sections. Validate MPEG-2 CRC32, section length, table id, version, `current_next`, program/PMT/PCR PIDs, and elementary streams. Duplicate current-version sections are idempotent; version changes publish a new immutable snapshot.

- [ ] **Step 4: Complete boundary regressions**

  Add invalid CRC, truncated section, multiple programs, multiple PMTs, unrelated PID payload, duplicate section, version change, continuity loss, and unsupported 192/204 stride cases. Expected GREEN: focused node tests pass.

- [ ] **Step 5: Commit**

  ```powershell
  git add CMakeLists.txt src/internal/graph/protocol/mpegts tests/unit/mpeg_ts_packet_tests.cpp
  git commit -m "feat: parse MPEG-TS clock and program evidence"
  ```

### Task 2: Evidence timeline, PCR tracker, and source mapper

**Files:**
- Create: `src/internal/graph/protocol/mpegts/MediaTsEvidenceTimeline.h`
- Create: `src/internal/graph/protocol/mpegts/MediaTsEvidenceTimeline.cpp`
- Create: `src/internal/graph/protocol/mpegts/MediaTsProgramClockTracker.h`
- Create: `src/internal/graph/protocol/mpegts/MediaTsProgramClockTracker.cpp`
- Create: `src/internal/graph/protocol/mpegts/MediaTsSourceClockMapper.h`
- Create: `src/internal/graph/protocol/mpegts/MediaTsSourceClockMapper.cpp`
- Create: `tests/unit/mpeg_ts_clock_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- `MediaTsEvidenceTimeline::append(checkpoint)` requires strictly increasing byte offsets and bounded capacity.
- `MediaTsEvidenceTimeline::atOrBefore(packetPosition)` is non-destructive and supports planned backward position queries.
- `MediaTsEvidenceTimeline::observePacketPosition(packetPosition)` advances a high watermark and permits eviction only below `highWatermark - maximumPositionRegressionBytes`; capacity exhaustion without a safe eviction fails closed.
- `MediaTsProgramClockTracker` consumes only the selected immutable program/PID policy and publishes generation/readiness plus unwrapped PCR calibration.
- `MediaTsSourceClockMapper` maps optional raw PTS and DTS independently into optional `MediaRunningTime` values.

- [ ] **Step 1: Write timeline and tracker RED tests**

  Cover exact/earlier/later offset queries, audio/video packet-position rollback, capacity overflow, required-checkpoint eviction, PCR wrap, jitter, skipped legal intervals, excessive gap, regression, discontinuity, continuity loss, program change, and PCR PID change.

- [ ] **Step 2: Implement immutable evidence timeline**

  Store checkpoints ordered by absolute offset. Reject duplicate/regressing append offsets. A query beyond `maximumPositionRegressionBytes` or requiring evicted evidence returns a structured failure; never return the newest checkpoint unconditionally.

- [ ] **Step 3: Implement program clock tracker**

  Reuse `MediaTimestampUnwrapper` with `MpegTsPcr27Mhz`. Validate selected program/PMT/PCR identity and planned PCR interval/jitter/gap. Legal discontinuity advances generation and enters reacquisition; unexplained regression fails.

- [ ] **Step 4: Write mapper RED tests**

  Cover PTS and DTS 33-bit wrap, independent presentation/decode values, missing PTS with present DTS, missing DTS with present PTS, negative source epoch, and exact nanosecond conversion.

- [ ] **Step 5: Implement source mapping**

  Reuse `MediaTimestampUnwrapper` with `MpegTsPtsDts33`. Align each unwrapped 90 kHz value to the selected PCR epoch. Keep missing values absent and reject mapping without locked PCR evidence.

- [ ] **Step 6: Run focused tests and commit**

  ```powershell
  cmake --build out/build/x64-debug --config Debug --target media_transcode_node_tests
  out\build\x64-debug\media_transcode_node_tests.exe
  git add CMakeLists.txt src/internal/graph/protocol/mpegts tests/unit/mpeg_ts_clock_tests.cpp
  git commit -m "feat: map MPEG-TS program clocks into source time"
  ```

### Task 3: Observed AVIO and move-only prepared input session

**Files:**
- Create: `src/internal/graph/runtime/ffmpeg/FFmpegObservedReadAvio.h`
- Create: `src/internal/graph/runtime/ffmpeg/FFmpegObservedReadAvio.cpp`
- Create: `src/internal/graph/protocol/mpegts/MediaTsInputSession.h`
- Create: `src/internal/graph/protocol/mpegts/MediaTsInputSession.cpp`
- Create: `src/internal/graph/runtime/buffer/MediaTsPreparedInputBuffer.h`
- Create: `src/internal/graph/runtime/buffer/MediaTsPreparedInputBuffer.cpp`
- Create: `tests/unit/mpeg_ts_input_session_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- `FFmpegObservedReadAvio::open(protocolUrl, protocolOptions, bufferBytes, observer, interruptState)` owns inner and outer AVIO contexts.
- A narrow `FFmpegProtocolAvioOpener` seam supplies the inner AVIO. Production calls `avio_open2`; deterministic tests provide a caller-owned byte source. The seam changes ownership only and cannot select protocol or demux policy.
- The concrete byte observer accepts immutable spans and absolute starting offsets; it cannot replace bytes or return transport decisions.
- `MediaTsInputSession` composes observed AVIO, parser, PSI assembler, timeline, and the custom-I/O `AVFormatContext`.
- `MediaTsPreparedInputBuffer` exclusively transfers the complete session to the MPEG-TS demux node.

- [ ] **Step 1: Write deterministic AVIO/session RED tests**

  Use a test byte-source protocol adapter with fragmented reads. Assert identical bytes reach FFmpeg-facing outer reads, observer offsets are monotonic, successful reads are not changed by evidence state, timeout is waiting, explicit closure is EOF, cancellation returns `AVERROR_EXIT`, and one prepared session transfers once.

- [ ] **Step 2: Implement `FFmpegObservedReadAvio` RAII**

  Open the inner AVIO with `avio_open2`, create the outer non-seekable context with `avio_alloc_context`, and keep protocol and demux dictionaries separate. Store structured observer failure without changing an already successful read result. Free resources in the design-specified order.

- [ ] **Step 3: Implement `MediaTsInputSession`**

  Explicitly select `av_find_input_format("mpegts")`, assign the outer AVIO to a preallocated format context, set `AVFMT_FLAG_CUSTOM_IO`, open/probe once, and retain parser/timeline state accumulated during preflight. Reject non-188 framing and incomplete program inventory.

- [ ] **Step 4: Implement prepared buffer ownership**

  The buffer exposes immutable stream/program snapshots and a single `takeSession()` operation. Copying is deleted; a second transfer fails. Generic `FFmpegFormatContextBuffer` remains unchanged for non-TS inputs.

- [ ] **Step 5: Add lifecycle and failure tests**

  Cover open failure, observer failure, probe failure, timeout, EOF, interrupt, double transfer, destruction before transfer, destruction after transfer, and exact AVFormat/outer/inner release order.

- [ ] **Step 6: Commit**

  ```powershell
  git add CMakeLists.txt src/internal/graph/runtime/ffmpeg/FFmpegObservedReadAvio.* src/internal/graph/protocol/mpegts/MediaTsInputSession.* src/internal/graph/runtime/buffer/MediaTsPreparedInputBuffer.* tests/unit/mpeg_ts_input_session_tests.cpp
  git commit -m "feat: own observed MPEG-TS AVIO session"
  ```

### Task 4: Planner-owned program inventory and complete TS plan

**Files:**
- Create: `src/internal/graph/planner/realtime/MediaTsProgramSelector.h`
- Create: `src/internal/graph/planner/realtime/MediaTsProgramSelector.cpp`
- Modify: `src/internal/graph/planner/avsync/MediaAvSyncPlan.h`
- Modify: `src/internal/graph/planner/avsync/MediaAvSyncPlanner.cpp`
- Modify: `src/internal/graph/planner/avsync/MediaAvSyncPlanValidator.cpp`
- Modify: `src/internal/graph/planner/realtime/MediaPreparedRealtimeInput.h`
- Modify: `src/internal/graph/planner/realtime/MediaPreparedRealtimeInput.cpp`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.cpp`
- Modify: `src/internal/graph/planner/realtime/MediaStreamCapabilityProbe.cpp`
- Modify: `tests/unit/test_planner.cpp`
- Modify: `tests/unit/test_realtime_rtp_graph.cpp`

**Interfaces:**
- `MediaTsProgramSelector::select(publicPrograms, parserInventory, selectedVideoStream, selectedAudioStream)` returns exactly one complete program plan or fails.
- `MediaAvSyncTsPlan` carries program/PMT/video/audio/PCR identities and PCR/timestamp policy. `MediaRealtimeRtpInputNodePlan::mpegTs` contains a required `MediaRealtimeTsInputPlan { demuxFormat, packetSize, avioBufferBytes, maximumDatagramBytes, evidenceTimelineCapacity, maximumPacketPositionRegressionBytes }`. Existing URL/open/read/analyze/probe fields remain the protocol-I/O policy.
- `MediaPreparedRealtimeInput` becomes a tagged move-only binding able to hold either the existing generic format buffer or a `MediaTsPreparedInputBuffer`; callers must inspect the planned binding kind rather than fallback.

- [ ] **Step 1: Write planner selection RED tests**

  Cover one valid program, two ambiguous programs, video/audio in different programs, missing PMT/PCR PID, public/parser inventory mismatch, selected stream absent from program, hard-coded legacy PID values not matching input, and unsupported packet size.

- [ ] **Step 2: Implement program selector**

  Build immutable public program snapshots after stream-info discovery. Cross-check parser PAT/PMT inventory. Require exactly one program containing both selected streams and return its actual program/PMT/video/audio/PCR PIDs.

- [ ] **Step 3: Expand and validate the TS plan**

  Keep source-clock identity/thresholds as explicit optionals in `MediaAvSyncTsPlan`. Add the optional `MediaRealtimeTsInputPlan` to the realtime input plan so missing TS session state is representable. Populate both only in the planner, and make validators reject every missing, zero, negative, inconsistent, or unsupported value independently.

- [ ] **Step 4: Integrate prepared TS preflight**

  For MPEG-TS UDP input, construct one `MediaTsInputSession`, run probe through it, select the program, and retain the same prepared session for runtime. Do not reopen the URL. Generic URL/RTSP and raw RTP preflight remain separate.

- [ ] **Step 5: Remove TS PID constants and add mutation coverage**

  Delete any planner assignments of standard-looking program/PID values not derived from inventory. Extend required-field mutation tests for every new plan field and verify no downstream default exists.

- [ ] **Step 6: Clean rebuild, planner tests, and commit**

  ```powershell
  cmd.exe /d /s /c "call D:\VisualStudio2026\Common7\Tools\VsDevCmd.bat -arch=x64 -host_arch=x64 >nul && cmake --build out\build\x64-debug --config Debug --clean-first"
  ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -R "media_transcode_(planner|integration)_tests"
  git add src/internal/graph/planner tests/unit/test_planner.cpp tests/unit/test_realtime_rtp_graph.cpp
  git commit -m "feat: plan MPEG-TS program clock ownership"
  ```

### Task 5: Dedicated MPEG-TS demux node and graph wiring

**Files:**
- Create: `src/internal/graph/nodes/demux/MpegTsDemuxNode.h`
- Create: `src/internal/graph/nodes/demux/MpegTsDemuxNode.cpp`
- Create: `src/internal/graph/model/MediaPacketSourceTiming.h`
- Modify: `src/internal/graph/runtime/buffer/FFmpegPacketBuffer.h`
- Modify: `src/internal/graph/runtime/buffer/FFmpegPacketBuffer.cpp`
- Modify: `src/internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h`
- Modify: `src/internal/graph/runtime/ffmpeg/FFmpegBufferFactory.cpp`
- Modify: `src/internal/graph/model/MediaNodeKind.h`
- Modify: `src/internal/graph/runtime/factory/MediaRuntimeNodeFactory.cpp`
- Modify: `src/internal/graph/core/MediaGraphDump.cpp`
- Modify: `src/internal/graph/diagnostics/MediaGraphDiagnostics.cpp`
- Modify: `src/internal/graph/builder/realtime/MediaRealtimeOptionApplier.cpp`
- Modify: `src/internal/graph/builder/realtime/MediaRealtimeRtpTranscodeGraphBuilder.cpp`
- Modify: `src/internal/graph/nodes/input/RealtimeInputNode.h`
- Modify: `src/internal/graph/nodes/input/RealtimeInputNode.cpp`
- Modify: `tests/unit/test_node.cpp`
- Modify: `tests/unit/test_builder.cpp`
- Modify: `tests/unit/test_realtime_rtp_graph.cpp`

**Interfaces:**
- `MpegTsDemuxNode` consumes one `MediaTsPreparedInputBuffer` and emits stream packets with independent mapped PTS/DTS, source-time calibration, and generation.
- `MediaPacketSourceTiming { optional presentationNs, optional decodeNs, generation }` is immutable packet metadata. `FFmpegPacketBuffer` continues to own the original packet and gains optional source timing at construction; existing generic factory calls provide no source timing. The Task 4 path never rewrites `AVPacket` timestamps.
- The synchronized MPEG-TS graph uses `RealtimeInput -> MpegTsDemux`; other inputs continue to use the generic demux path.

- [ ] **Step 1: Write graph and node RED tests**

  Assert a synchronized MPEG-TS plan contains exactly one `MpegTsDemux` and no generic `Demux` for that input. Test packet-position evidence lookup, independent PTS/DTS mapping, stream PID validation, missing position, missing evidence, timeline overflow, discontinuity generation, timeout waiting, EOF, stop, abort, and second prepared-binding rejection.

- [ ] **Step 2: Add node kind, factory, dump, and diagnostics support**

  Register one explicit `MpegTsDemux` kind. The runtime factory must reject incomplete options or the wrong buffer type; no generic-demux fallback is permitted.

- [ ] **Step 3: Implement timed packet buffer and demux orchestration**

  After `av_read_frame()`, validate stream index/PID, require non-negative `packet.pos`, query the timeline at or before that position, update tracker/mapper, and emit the owned packet plus immutable source timing and generation. Preserve raw packet PTS/DTS/time base for later canonical mapping.

- [ ] **Step 4: Wire the prepared binding and builder**

  Make `RealtimeInputNode` emit the tagged complete binding. The builder chooses `MpegTsDemux` only from the planner's explicit input/session kind and serializes all required TS values. Packet select and later branches consume the dedicated node outputs without local clock decisions.

- [ ] **Step 5: Run focused tests and commit**

  ```powershell
  cmake --build out/build/x64-debug --config Debug --target media_transcode_builder_tests media_transcode_node_tests media_transcode_integration_tests
  ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -R "media_transcode_(builder|node|integration)_tests"
  git add src/internal/graph tests/unit/test_node.cpp tests/unit/test_builder.cpp tests/unit/test_realtime_rtp_graph.cpp
  git commit -m "feat: demux MPEG-TS with program clock evidence"
  ```

### Task 6: Real UDP integration, lifecycle faults, and Task 4 gate

**Files:**
- Create: `tests/unit/mpeg_ts_observed_avio_integration_tests.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/unit/test_event_driven_runtime.cpp`
- Modify: `tests/unit/test_realtime_rtp_graph.cpp`
- Modify: `.superpowers/sdd/progress.md`
- Create: `.superpowers/sdd/task-4-combined-report.md`

**Interfaces:**
- Tests exercise the same public planner, prepared session, builder, runtime, and custom AVIO path used by the realtime CLI.
- No test-only parser, alternate socket path, or synthesized clock is accepted as integration evidence.

- [ ] **Step 1: Add real UDP observed-AVIO integration RED**

  Start a real FFmpeg MPEG-TS UDP sender with H264/AAC. Through production preflight and `av_read_frame`, require actual A/V packets, selected program/PIDs, PCR observations, and mapped PTS/DTS. A second exclusive bind to the input port must fail, proving one socket owner.

- [ ] **Step 2: Add crafted production-path fault cases**

  Send valid TS bytes containing PCR wrap, discontinuity, PAT/PMT version change, PCR PID change, gap, and regression through the same UDP/custom-AVIO session. Assert generation/reacquisition or explicit failure exactly as planned.

- [ ] **Step 3: Add lifecycle and idle-CPU evidence**

  Cover timeout, EOF test source, stop, abort, reset, read cancellation, probe failure, and teardown at every ownership stage. Count callback/worker progress during sparse input and assert event-driven waiting without busy polling.

- [ ] **Step 4: Run full clean verification**

  ```powershell
  cmd.exe /d /s /c "call D:\VisualStudio2026\Common7\Tools\VsDevCmd.bat -arch=x64 -host_arch=x64 >nul && cmake --build out\build\x64-debug --config Debug --clean-first"
  ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -L deterministic
  ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -L integration
  ```

  Expected: every enabled test passes; unsupported hardware remains explicitly skipped only in hardware tiers.

- [ ] **Step 5: Review and commit**

  Generate a review package from the Task 4 base through head. A fresh reviewer must verify single ownership, planner authority, read-ahead correlation, lifecycle, no fallback, test authenticity, and CRLF. Fix every Critical/Important finding and re-review until PASS.

  ```powershell
  git add CMakeLists.txt tests/unit/mpeg_ts_observed_avio_integration_tests.cpp tests/unit/test_event_driven_runtime.cpp tests/unit/test_realtime_rtp_graph.cpp .superpowers/sdd/task-4-combined-report.md
  git commit -m "test: verify observed MPEG-TS clock input"
  git push origin codex/quality-priority-improvements
  ```
