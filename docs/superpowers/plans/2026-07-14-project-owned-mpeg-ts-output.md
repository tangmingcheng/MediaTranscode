# Project-Owned MPEG-TS Output Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Complete Task 11 with a project-owned MPEG-TS output session that serializes planner-controlled PAT, PMT, PES, PTS, DTS, PCR, continuity, and UDP packet batches from one playback epoch.

**Architecture:** A complete immutable `MediaTsMuxPlan` and `MediaPlaybackEpoch` drive a pure protocol stack under `protocol/mpegts`. A synchronous byte sink owns transport I/O, `MediaTsMuxSession` owns TS state, and `FileMuxNode` delegates lifecycle to an explicitly selected session implementation. Task 12 will connect the synchronized production graph and scheduler deadlines after this protocol and node boundary is proven.

**Tech Stack:** C++20, FFmpeg AVIO only for byte transport and existing non-TS file muxing, existing graph runtime, existing MPEG-TS parser/PSI assembler, CMake/CTest, Visual Studio 2026.

## Global Constraints

- Planner owns every policy value. Runtime code accepts complete typed products and fails on missing, unknown, or inconsistent values.
- No FFmpeg `mpegtsenc` fallback, codec sniffing, inferred PID/time base, default option, compatibility branch, or `+1 tick` repair.
- Supported project-owned payloads are exactly H.264 (`stream_type=0x1B`) and AAC with ADTS (`stream_type=0x0F`).
- MPEG-TS packets are exactly 188 bytes. UDP `maximumPacketsPerDatagram` is an upper bound; deadlines may produce shorter datagrams containing only complete TS packets.
- `dispatchOnMaster` remains decode time. `emitOnMaster` is the only new output-timing fact and satisfies `dispatchOnMaster - emitOnMaster == transportDecodeLead`; Task 11 carries it in `MediaTsAccessUnitBuffer`, while Task 12 adds it to production scheduler output.
- PCR is independently scheduled from emission master time. All PCR samples due at or before an access-unit emission must be written first.
- Sink failure is terminal. Successfully written bytes are never retried or represented as rolled back.
- FFmpeg codec/context pointers and packet spans never outlive the synchronous adapter call. Queues retain `MediaBufferRef`, not raw FFmpeg pointers.
- New/modified text is UTF-8 without BOM and CRLF. Do not add external paths or runtime DLL-copy behavior to CMake.
- Do not stage unrelated existing workspace changes. Every task has a focused commit and an independent spec/quality review.

---

### Task 1: Complete TS mux plan and emission/decode clock contract

**Files:**
- Create: `src/internal/graph/protocol/mpegts/MediaTsMuxPlan.h`
- Create: `src/internal/graph/protocol/mpegts/MediaTsMuxPlan.cpp`
- Modify: `src/internal/graph/protocol/mpegts/MediaTsOutputClockGenerator.h`
- Modify: `src/internal/graph/protocol/mpegts/MediaTsOutputClockGenerator.cpp`
- Modify: `src/internal/graph/planner/avsync/MediaAvSyncPlan.h`
- Modify: `src/internal/graph/planner/avsync/MediaAvSyncPlanner.cpp`
- Modify: `src/internal/graph/planner/avsync/MediaAvSyncPlanValidator.cpp`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeOutputPolicyPlanner.cpp`
- Modify: `tests/unit/test_planner.cpp`
- Modify: `tests/unit/test_node.cpp`

**Interfaces:**
- Produces `MediaTsMuxPlan`, the complete immutable protocol contract consumed by Tasks 3-5.
- Extends `MediaTsOutputClockGenerator::project` with `emitOnMaster` and validates the exact transport lead before committing DTS state.
- Narrows `MediaTsOutputClockPolicy` to clock-only fields. Program/PID identity exists only in `MediaTsMuxPlan`.

- [ ] **Step 1: Write failing plan and clock tests**

Add tests that construct one valid plan with transport-stream ID `1`, program `1`, PAT PID `0x0000`, PMT PID `0x0100`, video PID/PCR PID `0x0101`, audio PID `0x0102`, PAT/PMT version `0`, PSI repeat `100 ms`, H.264/AAC stream types `0x1B/0x0F`, four-byte length-prefixed H.264, SPS/PPS-before-random-access policy, AAC-LC/48 kHz/stereo, PCR interval/gap/jitter `20/100/5 ms`, transport lead `100 ms`, packet size `188`, four explicit continuity seeds, and maximum seven packets per datagram.

Mutate every field independently and assert rejection: PAT PID not zero, PID collision, PCR PID not an ES PID, wrong stream type, unknown H.264 layout/policy, NAL width outside `1..4`, invalid AAC object/sample/channel fields, nonpositive or unordered PCR/PSI periods, nonpositive transport lead, packet size not 188, continuity above 15, and datagram maximum outside `1..7`.

Add clock tests equivalent to:

```cpp
auto clock = MediaTsOutputClockGenerator::create(plan.clockPolicy(), epoch);
EXPECT_TRUE(ctx, clock);
EXPECT_TRUE(ctx, clock.value().project(
    epoch.generation, MediaScheduledStream::Video,
    master(240), master(220), master(120), plan.transportDecodeLead()));
EXPECT_FALSE(ctx, clock.value().project(
    epoch.generation, MediaScheduledStream::Video,
    master(260), master(240), master(141), plan.transportDecodeLead()));
```

The first unit has `dispatch-emit=100 ms`; the second differs by one millisecond and must fail without advancing video DTS state.

- [ ] **Step 2: Run RED**

Run `out\build\x64-debug\media_transcode_planner_tests.exe` and `out\build\x64-debug\media_transcode_node_tests.exe` after building those two targets. Expected: compilation fails because `MediaTsMuxPlan` and the new `project` contract do not exist.

- [ ] **Step 3: Implement the typed contract**

Define these exact public concepts:

```cpp
enum class MediaTsH264InputLayout : std::uint8_t { AnnexB = 0, LengthPrefixed = 1 };
enum class MediaTsParameterSetPolicy : std::uint8_t { Never = 0, BeforeRandomAccess = 1 };
enum class MediaTsOutputTransportKind : std::uint8_t { Udp = 0 };

struct MediaTsContinuitySeeds final {
    std::uint8_t pat;
    std::uint8_t pmt;
    std::uint8_t video;
    std::uint8_t audio;
};

struct MediaTsAacAdtsPlan final {
    std::uint8_t mpegId;
    std::uint8_t audioObjectType;
    std::uint8_t samplingFrequencyIndex;
    std::uint8_t channelConfiguration;
};

struct MediaTsMuxPlanParameters final {
    std::uint16_t transportStreamId;
    std::uint16_t programNumber;
    std::uint16_t patPid;
    std::uint16_t programMapPid;
    std::uint16_t videoPid;
    std::uint16_t audioPid;
    std::uint16_t pcrPid;
    std::uint8_t tableVersion;
    MediaRunningTime psiRepeatInterval;
    std::uint8_t videoStreamType;
    std::uint8_t audioStreamType;
    MediaTsH264InputLayout h264InputLayout;
    std::uint8_t h264NalLengthBytes;
    MediaTsParameterSetPolicy parameterSetPolicy;
    MediaTsAacAdtsPlan aac;
    MediaTsOutputClockPolicy clock;
    MediaRunningTime transportDecodeLead;
    std::uint16_t packetSize;
    MediaTsContinuitySeeds continuity;
    std::uint8_t maximumPacketsPerDatagram;
    MediaTsOutputTransportKind transportKind;
};

class MediaTsMuxPlan final {
public:
    static ::media::Result<MediaTsMuxPlan> create(MediaTsMuxPlanParameters parameters);
    const MediaTsMuxPlanParameters& parameters() const noexcept;
    const MediaTsOutputClockPolicy& clockPolicy() const noexcept;
    MediaRunningTime transportDecodeLead() const noexcept;
private:
    explicit MediaTsMuxPlan(MediaTsMuxPlanParameters parameters) noexcept;
    MediaTsMuxPlanParameters m_parameters;
};
```

Refactor `MediaTsOutputClockPolicy` so it contains only PCR interval/gap/jitter and the fixed timestamp time base. Remove its program/PID fields and move that validation exclusively into `MediaTsMuxPlan::create`; do not retain the old constructor.

`MediaAvSyncTsPlan` gains exactly one `std::optional<MediaTsMuxPlan> outputMux` rather than a parallel set of optional output fields. Existing TS input-selection fields remain input facts. `MediaAvSyncPlanner::planTs` creates the complete output plan, sets its `transportDecodeLead` to the existing planner-selected `startup.outputLeadNs` (`100 ms`), and stores it. Input program/PID selection and output program/PID assignment are independent planner facts: no input PID or program number is copied to, or compared with, the output plan. `MediaAvSyncPlanValidator` requires `outputMux` and never repeats the mux plan's internal field validation. Task 12 will bind the runtime `MediaPlaybackEpoch` to this already complete output policy.

`MediaRealtimeOutputPolicyPlanner` accepts project MPEG-TS output only when the explicit transport is `Udp` and the parsed URL scheme is exactly `udp`. Add near-miss regressions for `rtp://`, file paths, empty authority, missing port, and unknown schemes. It does not infer transport from a successfully opened AVIO URL.

Change clock projection to:

```cpp
::media::Result<MediaTsPacketClock> project(
    std::uint64_t generation,
    MediaScheduledStream stream,
    MediaRunningTime presentationOnMaster,
    MediaRunningTime dispatchOnMaster,
    MediaRunningTime emitOnMaster,
    MediaRunningTime transportDecodeLead);
```

Use checked subtraction to validate the lead before PTS/DTS conversion or monotonic-state mutation. PCR remains driven only by `advancePcrThrough(generation, emitOnMaster)`.

- [ ] **Step 4: Run GREEN and refactor**

Run `out\build\x64-debug\media_transcode_planner_tests.exe` and `out\build\x64-debug\media_transcode_node_tests.exe`. Expected: both exit `0`. Confirm invalid enum values are rejected by `switch` and that the old four-argument projection overload does not remain.

Before commit, generate this task's review package and require a fresh reviewer to return spec compliance PASS and code quality PASS with zero Critical/Important findings.

- [ ] **Step 5: Commit**

Commit only this task as `feat: complete MPEG-TS mux clock plan`.

---

### Task 2: Add protocol-neutral byte sink ownership

**Files:**
- Create: `src/internal/graph/runtime/io/MediaOutputByteSink.h`
- Create: `src/internal/graph/runtime/ffmpeg/FFmpegAvioOutputByteSink.h`
- Create: `src/internal/graph/runtime/ffmpeg/FFmpegAvioOutputByteSink.cpp`
- Create: `src/internal/graph/runtime/buffer/MediaOutputByteSinkBuffer.h`
- Create: `src/internal/graph/runtime/buffer/MediaOutputByteSinkBuffer.cpp`
- Create: `tests/unit/output_byte_sink_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**

```cpp
class MediaOutputByteSink {
public:
    virtual ~MediaOutputByteSink() = default;
    virtual ::media::Result<std::size_t> write(std::span<const std::uint8_t> bytes) = 0;
    virtual ::media::Status flush() = 0;
    virtual ::media::Status close() = 0;
};

class FFmpegAvioOutputByteSink final : public MediaOutputByteSink {
public:
    static ::media::Result<std::unique_ptr<FFmpegAvioOutputByteSink>> open(
        std::string url,
        int writeFlags);
    ~FFmpegAvioOutputByteSink();
    ::media::Result<std::size_t> write(std::span<const std::uint8_t> bytes) override;
    ::media::Status flush() override;
    ::media::Status close() override;
};
```

`MediaOutputByteSinkBuffer` is the sole RAII owner and exposes `takeSink()` exactly once. It has a distinct `MediaBufferType`/`MediaPayloadKind` appended without renumbering existing values.

- [ ] **Step 1: Write failing ownership and error tests**

Test null/empty URL rejection, exact byte count, error propagation, close idempotence, write-after-close rejection, destructor cleanup, one-time ownership transfer, and a fake sink returning `bytes.size()-1` so callers can detect a short write.

- [ ] **Step 2: Run RED**

Run `media_transcode_node_tests`. Expected: compilation fails for missing sink and buffer types.

- [ ] **Step 3: Implement RAII sink and buffer**

Open with `avio_open2`; `write` calls `avio_write`, flushes the synchronous batch, checks `avio_error`, and returns the exact accepted count only on success. `close` uses `avio_closep` once and preserves the first failure. The destructor closes without throwing. No CMake path or DLL-copy rule is added; only the new test source is added to the existing node-test target.

- [ ] **Step 4: Run GREEN and review ownership**

Run `out\build\x64-debug\media_transcode_node_tests.exe`; expected exit `0`. Inspect all new raw FFmpeg handles and confirm each has one RAII owner and no borrowed handle escapes.

Before commit, generate this task's review package and require a fresh reviewer to return spec compliance PASS and code quality PASS with zero Critical/Important findings.

- [ ] **Step 5: Commit**

Commit as `feat: add owned MPEG-TS byte sink`.

---

### Task 3: Serialize PSI/PES and explicit H.264/AAC framing

**Files:**
- Create: `src/internal/graph/protocol/mpegts/MediaTsCrc32.h`
- Create: `src/internal/graph/protocol/mpegts/MediaTsCrc32.cpp`
- Create: `src/internal/graph/protocol/mpegts/MediaTsPsiSerializer.h`
- Create: `src/internal/graph/protocol/mpegts/MediaTsPsiSerializer.cpp`
- Create: `src/internal/graph/protocol/mpegts/MediaTsTimestampFieldSerializer.h`
- Create: `src/internal/graph/protocol/mpegts/MediaTsTimestampFieldSerializer.cpp`
- Create: `src/internal/graph/protocol/mpegts/MediaTsPesSerializer.h`
- Create: `src/internal/graph/protocol/mpegts/MediaTsPesSerializer.cpp`
- Create: `src/internal/graph/protocol/mpegts/MediaTsMaterializedStreamConfig.h`
- Create: `src/internal/graph/protocol/mpegts/MediaTsH264AccessUnitFramer.h`
- Create: `src/internal/graph/protocol/mpegts/MediaTsH264AccessUnitFramer.cpp`
- Create: `src/internal/graph/protocol/mpegts/MediaTsAacAdtsFramer.h`
- Create: `src/internal/graph/protocol/mpegts/MediaTsAacAdtsFramer.cpp`
- Create: `tests/unit/mpeg_ts_output_serializer_tests.cpp`
- Create: `tests/unit/mpeg_ts_access_unit_framer_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**

```cpp
struct MediaTsProgramTables final { std::vector<std::uint8_t> pat; std::vector<std::uint8_t> pmt; };
class MediaTsPsiSerializer final {
public:
    static ::media::Result<MediaTsProgramTables> serialize(const MediaTsMuxPlan& plan);
};

struct MediaTsPesHeader final { std::array<std::uint8_t, 19> bytes{}; std::size_t size = 0; };
class MediaTsPesSerializer final {
public:
    static ::media::Result<MediaTsPesHeader> header(
        MediaScheduledStream stream,
        const MediaTsPacketClock& clock,
        std::size_t framedPayloadBytes);
};

struct MediaTsMaterializedVideoConfig final {
    MediaTsH264InputLayout layout;
    std::uint8_t nalLengthBytes;
    std::vector<std::uint8_t> spsAnnexB;
    std::vector<std::uint8_t> ppsAnnexB;
};

struct MediaTsMaterializedAudioConfig final {
    std::uint8_t audioObjectType;
    std::uint8_t samplingFrequencyIndex;
    std::uint8_t channelConfiguration;
};

class MediaTsFramedAccessUnit final {
public:
    static MediaTsFramedAccessUnit borrowed(std::span<const std::uint8_t> bytes);
    static MediaTsFramedAccessUnit owned(std::vector<std::uint8_t> bytes);
    std::span<const std::uint8_t> bytes() const noexcept;
private:
    std::variant<std::span<const std::uint8_t>, std::vector<std::uint8_t>> m_storage;
};
```

`MediaTsH264AccessUnitFramer::frame` takes plan, validated materialized config, payload, and explicit `randomAccess`. `MediaTsAacAdtsFramer::frame` takes only the explicit ADTS plan and raw AAC payload.

- [ ] **Step 1: Write failing golden-byte tests**

Use fixed PAT/PMT expected byte arrays and assert CRC. PMT must publish PCR/video/audio PIDs and stream types `0x1B/0x0F`. Add fixed PTS-only, PTS+DTS, and 33-bit wrap headers. Verify stream IDs `0xE0/0xC0`, marker bits, and audio PES length overflow rejection.

For H.264, cover borrowed Annex-B, exact four-byte length-prefix conversion, SPS/PPS injection only before explicit random access, malformed NAL lengths, missing required parameter sets, and plan/materialized-config mismatch. For AAC, verify the seven-byte ADTS golden header, exact frame length, 8191-byte limit, and invalid object/frequency/channel values.

- [ ] **Step 2: Run RED**

Run `media_transcode_node_tests`. Expected: compilation fails for the missing serializers/framers.

- [ ] **Step 3: Implement serializers without mux state**

Implement MPEG-2 CRC-32 polynomial `0x04C11DB7`, PSI section-length/CRC encoding, five-byte 33-bit timestamp fields, and bounded PES headers. The serializer returns headers separately from payload to avoid a second full access-unit copy.

The H.264 framer reserves the exact converted size after a checked first pass. Annex-B without injection is borrowed; conversion or injection allocates one exact owned buffer. The AAC framer allocates exactly `7 + payload.size()` bytes. No function inspects codec payload to choose policy or random-access state.

- [ ] **Step 4: Run GREEN and parser cross-check**

Run `out\build\x64-debug\media_transcode_node_tests.exe`; expected exit `0`. Feed serialized PAT/PMT sections through the existing PSI test helpers and confirm the inventory matches the plan.

Before commit, generate this task's review package and require a fresh reviewer to return spec compliance PASS and code quality PASS with zero Critical/Important findings.

- [ ] **Step 5: Commit**

Commit as `feat: serialize MPEG-TS program and access units`.

---

### Task 4: Implement transactional 188-byte TS packetization and PCR insertion

**Files:**
- Create: `src/internal/graph/protocol/mpegts/MediaTsTransportPacketizer.h`
- Create: `src/internal/graph/protocol/mpegts/MediaTsTransportPacketizer.cpp`
- Create: `tests/unit/mpeg_ts_output_packetizer_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**

```cpp
class MediaTsPacketCommitToken final {
public:
    MediaTsPacketCommitToken(const MediaTsPacketCommitToken&) = delete;
    MediaTsPacketCommitToken& operator=(const MediaTsPacketCommitToken&) = delete;
    MediaTsPacketCommitToken(MediaTsPacketCommitToken&& other) noexcept;
    MediaTsPacketCommitToken& operator=(MediaTsPacketCommitToken&& other) noexcept;
private:
    friend class MediaTsPacketCursor;
    MediaTsPacketCommitToken(std::uint64_t cursorIdentity,
                             std::uint64_t revision) noexcept;
    std::uint64_t m_cursorIdentity;
    std::uint64_t m_revision;
};

struct MediaTsPreparedPacketBatch final {
    std::vector<std::array<std::uint8_t, 188>> packets;
    MediaTsPacketCommitToken commitToken;
};

class MediaTsPacketCursor final {
public:
    ::media::Result<MediaTsPreparedPacketBatch> prepare(std::size_t maximumPackets);
    ::media::Status commit(MediaTsPacketCommitToken commitToken);
    bool finished() const noexcept;
};

class MediaTsTransportPacketizer final {
public:
    static ::media::Result<MediaTsTransportPacketizer> create(const MediaTsMuxPlan& plan);
    ::media::Result<MediaTsPacketCursor> beginPat(
        std::span<const std::uint8_t> section);
    ::media::Result<MediaTsPacketCursor> beginPmt(
        std::span<const std::uint8_t> section);
    ::media::Result<MediaTsPacketCursor> beginPcrOnly(const MediaTsPcrClock& pcr);
    ::media::Result<MediaTsPacketCursor> beginPes(
        MediaScheduledStream stream,
        const MediaTsPesHeader& header,
        std::span<const std::uint8_t> payload,
        bool randomAccess);
};
```

- [ ] **Step 1: Write failing packet-level tests**

Assert exact 188-byte size/sync, TEI/scrambling clear, valid adaptation-field-control, PUSI only on first PSI/PES packet, pointer field on PSI, PCR reserved bits/base/extension, random-access indicator, adaptation-only PCR continuity behavior, payload continuity modulo 16, final-packet adaptation stuffing, and PCR wire wrap. Assert `beginPat`, `beginPmt`, `beginPcrOnly`, and `beginPes(stream, ...)` always use the plan-selected PID; reject invalid stream values and audio/video PES-header mismatch.

Prepare a batch and assert continuity does not change before `commit`. Commit the exact revision and assert the per-PID continuity advances by the payload-bearing packets only. Reject duplicate, stale, foreign, out-of-order, and moved-from commit tokens. Task 5 injects the actual sink failure and proves the failed prepared batch is never committed. Test that no stuffing byte is visible inside reconstructed PES payload.

- [ ] **Step 2: Run RED**

Run `media_transcode_node_tests`. Expected: compilation fails for missing packetizer.

- [ ] **Step 3: Implement one-packet-at-a-time commit**

Each cursor maintains provisional counters for its prepared batch but cannot mutate the packetizer's committed counters until the caller presents the matching commit revision after a real sink write. Commit tokens are move-only; moving transfers identity/revision and clears the source token so a moved-from token is rejected. A packetizer permits only one active cursor and one prepared batch; beginning another logical unit before the prior cursor finishes fails. PAT/PMT/PCR/ES PIDs are read only from the stored plan; callers never pass a PID. Build every packet in a local `std::array<uint8_t, 188>` and cap each preparation at the caller-supplied `1..maximumPacketsPerDatagram`. PCR encoding uses `planned.wire27Mhz`; Task 5 validates the planned sample through its `MediaTsOutputClockGenerator` before invoking the packetizer. Adaptation-only packets do not advance payload continuity.

- [ ] **Step 4: Run GREEN against the existing parser**

Run `out\build\x64-debug\media_transcode_node_tests.exe`; expected exit `0`. Parse all emitted packets with `MediaTsPacketParser` and require zero continuity events plus exact PID/PCR evidence.

Before commit, generate this task's review package and require a fresh reviewer to return spec compliance PASS and code quality PASS with zero Critical/Important findings.

- [ ] **Step 5: Commit**

Commit as `feat: packetize planner-owned MPEG-TS bytes`.

---

### Task 5: Build the MPEG-TS output session and deadline-aware packet batches

**Files:**
- Create: `src/internal/graph/protocol/mpegts/MediaTsAccessUnitView.h`
- Create: `src/internal/graph/protocol/mpegts/MediaTsPacketBatchWriter.h`
- Create: `src/internal/graph/protocol/mpegts/MediaTsPacketBatchWriter.cpp`
- Create: `src/internal/graph/protocol/mpegts/MediaTsMuxSession.h`
- Create: `src/internal/graph/protocol/mpegts/MediaTsMuxSession.cpp`
- Create: `tests/unit/mpeg_ts_output_session_tests.cpp`
- Create: `tests/unit/mpeg_ts_output_decode_integration_tests.cpp`
- Create: `tests/unit/fixtures/MediaTsPesTimestampInspector.h`
- Create: `tests/unit/fixtures/MediaTsPesTimestampInspector.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**

```cpp
struct MediaTsAccessUnitView final {
    std::span<const std::uint8_t> payload;
    MediaScheduledStream stream;
    std::uint64_t generation;
    MediaRunningTime presentationOnMaster;
    MediaRunningTime dispatchOnMaster;
    MediaRunningTime emitOnMaster;
    bool randomAccess;
};

class MediaTsMuxSession final {
public:
    struct Binding final {
        MediaTsMuxPlan plan;
        MediaPlaybackEpoch epoch;
        MediaTsMaterializedVideoConfig video;
        MediaTsMaterializedAudioConfig audio;
        std::unique_ptr<MediaOutputByteSink> sink;
    };
    struct AdvanceResult final {
        MediaRunningTime nextDeadline;
        std::size_t packetsWritten;
    };
    static ::media::Result<std::unique_ptr<MediaTsMuxSession>> create(
        Binding binding);
    ::media::Status start(MediaRunningTime emitOnMaster);
    ::media::Result<AdvanceResult> advanceThrough(MediaRunningTime emitOnMaster);
    ::media::Status writeAccessUnit(const MediaTsAccessUnitView& unit);
    ::media::Status finish();
    void abort() noexcept;
};
```

`MediaTsPacketBatchWriter` owns the sole `std::unique_ptr<MediaOutputByteSink>`. It flattens one prepared batch of `1..maximumPacketsPerDatagram` complete packets into one datagram write. A result not equal to `packets.size() * 188` is a terminal short-write error. Only after this exact write succeeds does `MediaTsMuxSession` call the cursor's `commit(revision)`. Deadline/explicit flush/finish may write a one-packet short datagram; no incomplete TS packet is buffered.

- [ ] **Step 1: Write failing session tests**

Cover exact lifecycle, initial PAT/PMT before the first PES, 100 ms PSI repetition, 20 ms PCR during sparse input, typed `AdvanceResult.nextDeadline`, all due PCR before AU emission, audio/video scheduled order, exact 100 ms transport lead, generation mismatch, wrap, duplicate start/finish, packet after finish, malformed materialized config, short write, sink failure, permanent poison, and idempotent noexcept abort cleanup.

Parse complete output with `MediaTsPacketParser`, `MediaTsPsiSectionAssembler`, and the test-only PES inspector. Assert selected program/PIDs, zero continuity events, H.264/AAC stream types, exact PTS/DTS, monotonic PCR, maximum gap, and no copied input PCR API.

- [ ] **Step 2: Run RED**

Run `media_transcode_node_tests`. Expected: compilation fails for missing session/batcher.

- [ ] **Step 3: Implement the state machine**

Use states `Created`, `Open`, `Finished`, and `Poisoned`. `start` emits initial PSI through prepare/write/commit. `advanceThrough` emits every PSI/PCR deadline up to the supplied emission time and completes every real sink write before returning. `writeAccessUnit` first calls `advanceThrough(unit.emitOnMaster)`, validates generation/lead, projects clocks, frames payload, writes PES batches, and never retains the input span. PCR is emitted only through its independent due-sample cursor; `beginPes` has no optional PCR path. `finish` flushes and closes the owned sink once and preserves the first failure.

- [ ] **Step 4: Run GREEN and real-byte decode probe**

Run `out\build\x64-debug\media_transcode_node_tests.exe`; expected exit `0`. Register `media_transcode_mpeg_ts_output_decode_integration_tests` with the `integration` label and `MEDIA_TRANSCODE_SOURCE_DIR`. The test reads `tests/samples/sample_h264_aac_2560x1440.mp4`, materializes its explicit H.264/AAC configuration and a bounded packet sequence, writes project-owned TS to a temporary file, reopens it with libavformat/libavcodec, and requires at least one decoded video frame and one decoded audio frame with no decode error. Run:

```powershell
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -R media_transcode_mpeg_ts_output_decode_integration_tests --timeout 120
```

Expected: `1/1` passed. Missing sample/decoder returns configured skip code `77` only in integration tier, never a deterministic pass.

Before commit, generate this task's review package and require a fresh reviewer to return spec compliance PASS and code quality PASS with zero Critical/Important findings.

- [ ] **Step 5: Commit**

Commit as `feat: add deadline-aware MPEG-TS mux session`.

---

### Task 6: Extract existing FFmpeg file mux lifecycle from FileMuxNode

**Files:**
- Create: `src/internal/graph/nodes/mux/MediaMuxSession.h`
- Create: `src/internal/graph/nodes/mux/FFmpegFileMuxSession.h`
- Create: `src/internal/graph/nodes/mux/FFmpegFileMuxSession.cpp`
- Create: `src/internal/graph/nodes/mux/MediaMuxSessionFactory.h`
- Create: `src/internal/graph/nodes/mux/MediaMuxSessionFactory.cpp`
- Modify: `src/internal/graph/nodes/mux/FileMuxNode.h`
- Modify: `src/internal/graph/nodes/mux/FileMuxNode.cpp`
- Modify: `tests/unit/test_node.cpp`

**Interfaces:**

```cpp
enum class MediaMuxSessionKind : std::uint8_t { FFmpegFile = 0, ProjectMpegTs = 1 };

struct MediaMuxSessionPollResult final {
    bool progressed;
    std::optional<MediaNodeProcessResult::DeadlineWait> nextWait;
};

class MediaMuxSession {
public:
    virtual ~MediaMuxSession() = default;
    virtual ::media::Status bindResource(MediaGraphExecutionContext& context,
                                         const MediaBufferRef& buffer) = 0;
    virtual ::media::Status bindStreamConfig(MediaGraphExecutionContext& context,
                                             const MediaBufferRef& buffer) = 0;
    virtual ::media::Status write(MediaGraphExecutionContext& context,
                                  const MediaBufferRef& buffer) = 0;
    virtual ::media::Result<MediaMuxSessionPollResult> poll(
        MediaGraphExecutionContext& context) = 0;
    virtual ::media::Status flush(MediaGraphExecutionContext& context) = 0;
    virtual ::media::Status finish(MediaGraphExecutionContext& context) = 0;
    virtual void abort() noexcept = 0;
};
```

The factory requires the exact planner-applied `mux.session_kind`; missing/unknown values fail. This task implements only `FFmpegFileMuxSession` and moves the current AVFormatContext/stream/header/packet/trailer behavior into it without semantic change.

- [ ] **Step 1: Write failing delegation tests**

Assert `FileMuxNode` rejects missing/unknown session kind, creates the explicit FFmpeg session, forwards metadata/config/packet/flush/EOF/abort in order, and propagates the first failure. Assert `poll.progressed` maps to process progress and `poll.nextWait` maps exactly to `MediaNodeProcessResult::waitingUntil(group, deadline)` when queues are empty. Assert `FileMuxNode` no longer owns AVFormatContext, stream indices, pending packets, header/trailer flags, or timestamp rescaling helpers.

- [ ] **Step 2: Run RED**

Run `media_transcode_node_tests`. Expected: tests fail because the node still owns FFmpeg mux behavior and no required session kind exists.

- [ ] **Step 3: Move behavior into `FFmpegFileMuxSession`**

Perform a responsibility move, not a second implementation. Keep FFmpeg stream registration and pending-packet semantics in the session. `FFmpegFileMuxSession::poll` reports no deadline. `FileMuxNode` retains only input arbitration, completion tracking, session lifecycle, poll/deadline mapping, abort forwarding, and graph forwarding. The factory uses an exhaustive `switch`; `ProjectMpegTs` returns unsupported until Task 7, never an FFmpeg fallback.

- [ ] **Step 4: Run GREEN and local regression**

Use the exact executables `out\build\x64-debug\media_transcode_node_tests.exe` and `out\build\x64-debug\media_transcode_builder_tests.exe`; both must exit `0`, including their explicit FFmpeg-file session regressions. Before commit, generate this task's review package and require a fresh reviewer to return spec compliance PASS and code quality PASS with zero Critical/Important findings.

- [ ] **Step 5: Commit**

Commit as `refactor: isolate FFmpeg file mux session`.

---

### Task 7: Integrate the project TS session boundary and verify Task 11

**Files:**
- Create: `src/internal/graph/nodes/mux/ProjectMpegTsMuxSessionAdapter.h`
- Create: `src/internal/graph/nodes/mux/ProjectMpegTsMuxSessionAdapter.cpp`
- Create: `src/internal/graph/nodes/mux/MediaTsFfmpegStreamConfigMaterializer.h`
- Create: `src/internal/graph/nodes/mux/MediaTsFfmpegStreamConfigMaterializer.cpp`
- Modify: `src/internal/graph/nodes/mux/MediaMuxSessionFactory.cpp`
- Modify: `src/internal/graph/nodes/mux/FileMuxNode.cpp`
- Modify: `src/internal/graph/nodes/output/FileOutputNode.h`
- Modify: `src/internal/graph/nodes/output/FileOutputNode.cpp`
- Create: `src/internal/graph/runtime/buffer/MediaTsMuxRuntimePlanBuffer.h`
- Create: `src/internal/graph/runtime/buffer/MediaTsMuxRuntimePlanBuffer.cpp`
- Create: `src/internal/graph/runtime/buffer/MediaTsAccessUnitBuffer.h`
- Create: `src/internal/graph/runtime/buffer/MediaTsAccessUnitBuffer.cpp`
- Modify: `src/internal/graph/runtime/buffer/MediaBuffer.h`
- Modify: `src/internal/graph/model/MediaPayloadKind.h`
- Modify: `src/internal/graph/builder/segments/MediaOutputSegmentBuilder.h`
- Modify: `src/internal/graph/builder/segments/MediaOutputSegmentBuilder.cpp`
- Modify: `src/internal/graph/model/MediaTranscodeParameters.h`
- Modify: `tests/unit/test_builder.cpp`
- Modify: `tests/unit/test_node.cpp`
- Create: `tests/unit/mpeg_ts_ffmpeg_config_materializer_tests.cpp`
- Modify: `tests/unit/test_realtime_rtp_graph.cpp`
- Modify: `CMakeLists.txt`
- Modify: `.superpowers/sdd/progress.md`

**Interfaces:**
- `MediaTsMuxRuntimePlanBuffer` is a typed immutable buffer containing `MediaTsMuxPlan`, `MediaPlaybackEpoch`, and `MediaAvSyncGroupKey`. Task 12 creates it from the planner product and active runtime epoch; Task 11 tests construct it directly.
- `MediaTsAccessUnitBuffer` owns the outer encoded `MediaBufferRef` plus explicit generation, presentation, dispatch, emission, and stream facts. Its factory requires an `FFmpegPacketBuffer`, requires outer `streamKind` to match the explicit stream, derives video `randomAccess` only from validated outer `KeyFrame` metadata, and fixes audio `randomAccess=false`; callers cannot supply or contradict that flag. The factory receives the exact `transportDecodeLead` from the typed plan and validates `dispatchOnMaster - emitOnMaster`; the adapter cross-checks it again against its runtime-plan buffer before writing. It exposes the AVPacket payload only as a synchronous view and does not retain a raw pointer.
- Their new `MediaBufferType` and `MediaPayloadKind` values are appended after existing values; no existing enum number changes.
- `MediaTsFfmpegStreamConfigMaterializer` copies and validates H.264/AAC facts from `AVCodecParameters` into `MediaTsMaterializedVideoConfig`/`MediaTsMaterializedAudioConfig`. For planned length-prefixed H.264 it parses only AVCDecoderConfigurationRecord; for planned Annex-B it parses only Annex-B parameter-set configuration. It never chooses between them by sniffing.
- `ProjectMpegTsMuxSessionAdapter` binds exactly one runtime-plan buffer, one taken sink, and exact video/audio materialized configurations, constructs `MediaTsMuxSession::Binding`, accepts only `MediaTsAccessUnitBuffer`, polls the group master clock for due PSI/PCR, and delegates lifecycle.
- `FileOutputNode` requires explicit `output.resource_kind=ffmpeg_format_context|byte_sink`. The byte-sink path opens `FFmpegAvioOutputByteSink`; no format-context fallback is allowed.
- `MediaOutputSegmentBuilder` requires explicit output and mux kinds. Existing local output supplies `ffmpeg_format_context`/`ffmpeg_file`; Task 12 will supply `byte_sink`/`project_mpegts` for the synchronized TS graph.

- [ ] **Step 1: Write failing boundary tests**

Manually construct a node graph with explicit project-TS session/resource kinds and typed runtime-plan/sink/config/access-unit buffers. Assert binding order, access-unit-buffer-only packet input, explicit emission and metadata-derived video random-access/audio false propagation, no retained raw AVPacket pointer, deadline `poll` to `waitingUntil`, EOF/flush/abort behavior, and exact real output bytes. Reject non-packet outer buffers, outer/explicit stream mismatch, missing/duplicate/wrong-generation buffers, and every case before the first PES byte is written.

Add materializer tests for valid AVCC and Annex-B SPS/PPS, valid AAC-LC parameters, malformed/truncated extradata, NAL-width mismatch, missing SPS/PPS, wrong codec ID, sample-rate/channel mismatch, config lifetime after the source `AVCodecParameters` is destroyed, and no automatic layout selection.

Add graph contract assertions that local/file output explicitly selects the FFmpeg resource/session and that no builder infers a session kind from `format=mpegts` or a URL.

- [ ] **Step 2: Run RED**

Run `out\build\x64-debug\media_transcode_node_tests.exe`, `out\build\x64-debug\media_transcode_builder_tests.exe`, and `out\build\x64-debug\media_transcode_integration_tests.exe`. Expected: project-TS construction fails because the typed buffers, adapter, materializer, and byte-sink resource kind are absent.

- [ ] **Step 3: Implement explicit adapters**

The materializer copies all required codec facts and releases FFmpeg pointers immediately. The runtime-plan buffer supplies the only epoch; the adapter cross-checks its group/generation against the execution context but never reads policy from string options or manufactures an epoch. `MediaTsAccessUnitBuffer` keeps its outer `MediaBufferRef` alive for the synchronous call, exposes a span only while calling the session, and never `av_packet_ref` inside the TS protocol layer.

On every node poll, the adapter reads the selected group's master clock, calls `advanceThrough(now)` when due, and returns the session's exact next deadline. `FileMuxNode` returns progress if packets were emitted; otherwise, with empty queues, it returns `waitingUntil(group, nextDeadline)`. Abort is forwarded exactly once and destroys the owned sink through RAII.

`FileOutputNode` opens only the planner-selected resource kind. `MediaMuxSessionFactory` creates `ProjectMpegTsMuxSessionAdapter` only for the exact enum. Missing configuration, wrong buffer types, generation mismatch, and late binding are terminal errors. Task 12 remains responsible for creating production `MediaTsMuxRuntimePlanBuffer` and `MediaTsAccessUnitBuffer` instances after it adds `emitOnMaster` to scheduler output.

- [ ] **Step 4: Full verification and review**

Perform a full clean rebuild of all configured targets. Then run:

```powershell
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -L deterministic --timeout 60
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -L integration --timeout 240
```

Expected: all deterministic and integration tests pass or an explicitly unsupported external dependency returns the configured skip code. Run `git diff --check`; verify UTF-8 without BOM and CRLF for every touched text file.

Create a review package from the Task 11 base commit through HEAD. A fresh reviewer must separately return spec compliance PASS and code quality PASS with zero Critical/Important findings. Fix and repeat until PASS.

- [ ] **Step 5: Record and commit**

Append Task 11 completion evidence to `.superpowers/sdd/progress.md`, including exact test results and the explicit note that Task 12 production wiring and two-minute RTP/TS acceptance remain pending. Commit as `feat: complete project-owned MPEG-TS output boundary` and push the branch.

---

## Task 12 Handoff Boundary

Do not claim playback acceptance at the end of this plan. Task 12 must still:

- add `emitOnMaster` to scheduled units and schedule it exactly `transportDecodeLead` before `dispatchOnMaster`;
- bind the runtime playback epoch and complete TS mux plan into the production graph;
- select `byte_sink`/`project_mpegts` for MPEG-TS output;
- remove obsolete synchronized barriers, timestamp repair, and per-mux pacing authorities;
- run hardware-only separate-RTP and MPEG-TS playback tests for two minutes each, measuring only `media_transcode_realtime_video_cli` CPU and enforcing the existing drift/stall/error gates.
