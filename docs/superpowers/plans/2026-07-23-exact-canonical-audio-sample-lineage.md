# Exact Canonical Audio Sample Lineage Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Preserve exact synchronized-audio sample intervals across periodic clock-map refreshes so long-running 44.1 kHz RTP never invents a one-sample lineage gap.

**Architecture:** A deep `MediaCanonicalAudioSourceTimeline` module owns the one-time generation sample anchor and exact count-based cursor at the canonical-input seam. `MediaCanonicalAccessUnitBuffer` carries the resulting interval through startup release, and `AudioDecodeNode` consumes it directly instead of quantizing canonical nanoseconds back to samples.

**Tech Stack:** C++20, FFmpeg packet buffers, CMake/Ninja, Visual Studio 2026, project `Result`/`Status` types.

## Global Constraints

- Planner products remain the only source of sample rate and samples-per-access-unit.
- The sample cursor is anchored once per locked generation; RTCP mapping refresh never re-anchors it.
- Canonical nanoseconds remain the A/V clock and drift domain.
- The strict interval accumulator remains unchanged; no `+/-1` tolerance, fallback, timestamp rewrite, or downstream repair is permitted.
- Non-locked readiness, generation change, sequence discontinuity, invalid interval, or overflow fails closed.
- Every compilation uses `--clean-first`; `/showIncludes` tree output is suppressed from displayed logs.
- Files are UTF-8 without BOM and CRLF.
- Never run `media_transcode_av_sync_production_runtime_integration_tests.exe` locally.

---

### Task 1: Exact canonical audio source timeline

**Files:**
- Create: `src/internal/graph/sync/lineage/MediaCanonicalAudioSourceTimeline.h`
- Create: `src/internal/graph/sync/lineage/MediaCanonicalAudioSourceTimeline.cpp`
- Create: `tests/unit/canonical_audio_source_timeline_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `MediaRunningTime`, planner-owned `sampleRate`, per-access-unit `sampleCount`, locked `generation`, and monotonic `sequence`.
- Produces:

```cpp
class MediaCanonicalAudioSourceTimeline final {
public:
    static ::media::Result<MediaCanonicalAudioSourceTimeline> create(
        int sampleRate);
    ::media::Result<MediaCanonicalAudioSampleInterval> append(
        MediaRunningTime canonicalPresentation,
        std::uint32_t sampleCount,
        std::uint64_t generation,
        std::uint64_t sequence);
    void reset() noexcept;
};
```

- [ ] **Step 1: Write the failing deterministic timeline test**

Create a standalone test executable that:

```cpp
auto created = MediaCanonicalAudioSourceTimeline::create(44'100);
assert(created);
auto timeline = std::move(created).value();
const auto anchor = MediaRunningTime::fromNanoseconds(123'456'789);
std::int64_t expectedBegin = 5'444; // nearest 44.1 kHz sample to the anchor
for (std::uint64_t sequence = 1; sequence <= 600; ++sequence) {
    auto interval = timeline.append(
        sequence == 1
            ? anchor
            : MediaRunningTime::fromNanoseconds(
                  anchor.nanoseconds() +
                  static_cast<std::int64_t>(sequence - 1) * 23'219'955 +
                  static_cast<std::int64_t>(sequence % 3)),
        1'024, 7, sequence);
    assert(interval);
    assert(interval.value().begin == expectedBegin);
    assert(interval.value().end == expectedBegin + 1'024);
    assert(interval.value().sampleRate == 44'100);
    expectedBegin += 1'024;
}
```

Also assert rejection of sample rate `0`, sample count `0`, sequence `0`,
skipped/duplicate sequence, generation change, and checked-add overflow. Assert
`reset()` permits a fresh generation and fresh anchor.

- [ ] **Step 2: Register and run the test to verify RED**

Add:

```cmake
add_executable(media_transcode_canonical_audio_source_timeline_tests
    tests/unit/canonical_audio_source_timeline_tests.cpp)
media_transcode_configure_test(
    media_transcode_canonical_audio_source_timeline_tests)
media_transcode_register_test(
    media_transcode_canonical_audio_source_timeline_tests
    "node;deterministic")
```

Run:

```powershell
cmake --build out/build/x64-debug --target media_transcode_canonical_audio_source_timeline_tests --clean-first
```

Expected: compilation fails because `MediaCanonicalAudioSourceTimeline` does
not exist.

- [ ] **Step 3: Implement the minimal deep module**

The implementation must:

```cpp
if (!m_initialized) {
    auto grid = MediaAudioSampleGrid::create(m_sampleRate);
    auto begin = grid
        ? grid.value().nearestSample(canonicalPresentation)
        : ::media::Result<std::int64_t>::failure(grid.error());
    // Validate begin and checked end, then establish generation/sequence/cursor.
} else {
    // Require identical generation and sequence == m_lastSequence + 1.
    // begin = m_expectedNextBegin; end = checked begin + sampleCount.
}
```

Use explicit state fields for sample rate, generation, last sequence,
expected-next begin, and initialization. Return `InvalidArgument` for contract
violations and checked arithmetic errors for unrepresentable intervals.

- [ ] **Step 4: Run the timeline test to verify GREEN**

Run the same clean-first target build, then:

```powershell
out\build\x64-debug\media_transcode_canonical_audio_source_timeline_tests.exe
```

Expected: exit `0`.

- [ ] **Step 5: Commit Task 1**

```powershell
git add -- CMakeLists.txt src/internal/graph/sync/lineage/MediaCanonicalAudioSourceTimeline.h src/internal/graph/sync/lineage/MediaCanonicalAudioSourceTimeline.cpp tests/unit/canonical_audio_source_timeline_tests.cpp
git commit -m "feat: add exact canonical audio source timeline"
```

---

### Task 2: Carry exact intervals through canonical audio media

**Files:**
- Modify: `src/internal/graph/sync/MediaCanonicalAccessUnitBuffer.h`
- Modify: `src/internal/graph/sync/MediaCanonicalAccessUnitBuffer.cpp`
- Modify: `src/internal/graph/nodes/sync/MediaCanonicalInputNode.h`
- Modify: `src/internal/graph/nodes/sync/MediaCanonicalInputNode.cpp`
- Modify: `src/internal/graph/nodes/sync/MediaEncodedAudioCanonicalizerNode.cpp`
- Modify: all production and test call sites returned by `rg -n "MediaCanonicalAccessUnitBuffer::create\\(" src tests`
- Modify: `tests/unit/canonical_lineage_tests.cpp`
- Modify: `tests/unit/av_sync_canonical_input_tests.cpp`

**Interfaces:**
- Consumes: Task 1 `MediaCanonicalAudioSourceTimeline::append`.
- Produces:

```cpp
static ::media::Result<MediaBufferRef> create(
    MediaBufferRef media,
    std::shared_ptr<const MediaCanonicalLineage> lineage,
    std::optional<MediaCanonicalAudioSampleInterval> audioInterval);

const std::optional<MediaCanonicalAudioSampleInterval>&
audioSampleInterval() const noexcept;
```

- [ ] **Step 1: Write failing canonical-buffer contract tests**

Extend `canonical_lineage_tests.cpp` to assert:

```cpp
EXPECT_FALSE(ctx, MediaCanonicalAccessUnitBuffer::create(
    audioPacket, audioLineage, std::nullopt));
EXPECT_FALSE(ctx, MediaCanonicalAccessUnitBuffer::create(
    videoPacket, videoLineage,
    MediaCanonicalAudioSampleInterval{0, 1'024, 48'000}));
auto audio = MediaCanonicalAccessUnitBuffer::create(
    audioPacket, audioLineage,
    MediaCanonicalAudioSampleInterval{5'444, 6'468, 44'100});
EXPECT_TRUE(ctx, audio);
```

Reject invalid intervals and accept video only with `std::nullopt`.

- [ ] **Step 2: Write failing canonical-input refresh test**

Extend `av_sync_canonical_input_tests.cpp` with hundreds of audio packets in
one locked generation. Vary each packet's `MediaPacketSourceTiming::presentationNs`
by sub-sample mapping-refresh residuals while preserving sequence and planned
`1024 @ 44100`. Assert every emitted canonical buffer carries adjacent exact
intervals and the final end equals `firstBegin + 600 * 1024`.

Add negative cases for non-locked readiness and a later generation; both must
fail rather than reset the established cursor.

- [ ] **Step 3: Run RED**

```powershell
cmake --build out/build/x64-debug --target media_transcode_canonical_lineage_tests --clean-first
cmake --build out/build/x64-debug --target media_transcode_av_sync_canonical_input_tests --clean-first
```

Expected: compilation or assertions fail because canonical media has no exact
audio interval contract.

- [ ] **Step 4: Implement the buffer and canonical-input contracts**

`MediaCanonicalAccessUnitBuffer::create` must require:

```cpp
const bool audio = media->streamKind() == MediaStreamKind::Audio;
if (audio != audioInterval.has_value()) {
    return failure("Canonical audio media requires exact sample lineage");
}
if (audioInterval && !audioInterval->sampleCount()) {
    return failure("Canonical audio media rejects an invalid sample interval");
}
```

`MediaCanonicalInputNode` owns:

```cpp
std::optional<MediaCanonicalAudioSourceTimeline> m_audioTimeline;
```

Create it from the planner-provided audio sample rate during configuration,
append once per audio access unit before emission, pass the interval into the
canonical buffer, and clear it in `resetState()`. Video passes `std::nullopt`.

`MediaEncodedAudioCanonicalizerNode` passes the already proven encoded fragment
range `{begin, end, origin.outputSampleRate}`. Every other call site passes
either its known exact audio interval or explicit `std::nullopt` for video.

- [ ] **Step 5: Run GREEN**

Run both clean-first target builds and both executables. Expected: exit `0`.

- [ ] **Step 6: Commit Task 2**

Stage only the files changed for Task 2 and commit:

```powershell
git commit -m "feat: propagate exact canonical audio intervals"
```

---

### Task 3: Make synchronized decode consume upstream sample lineage

**Files:**
- Modify: `src/internal/graph/nodes/audio/MediaAudioDecodeInputView.h`
- Modify: `src/internal/graph/nodes/audio/MediaAudioDecodeInputView.cpp`
- Modify: `src/internal/graph/nodes/audio/AudioDecodeNode.cpp`
- Modify: `tests/unit/audio_lineage_node_tests.cpp`
- Modify: `tests/unit/audio_codec_lineage_tests.cpp`

**Interfaces:**
- Consumes: `MediaCanonicalAccessUnitBuffer::audioSampleInterval()` from Task 2.
- Produces:

```cpp
struct MediaSynchronizedAudioDecodeInput final {
    std::shared_ptr<const MediaCanonicalLineage> lineage;
    MediaCanonicalAudioSampleInterval sourceInterval;
    MediaAudioPlaybackOrigin origin;
    std::uint32_t trimLeadingSamples;
};
```

- [ ] **Step 1: Write the failing decode-lineage test**

Create a released canonical packet whose canonical nanosecond presentation
would quantize one sample away from the supplied interval. Decode input
resolution must preserve the supplied interval:

```cpp
const MediaCanonicalAudioSampleInterval expected{426'929, 427'953, 44'100};
auto view = resolveMediaAudioDecodeInput(released, synchronizedMode);
EXPECT_TRUE(ctx, view);
EXPECT_EQ(ctx, view.value().synchronized->sourceInterval.begin, expected.begin);
EXPECT_EQ(ctx, view.value().synchronized->sourceInterval.end, expected.end);
EXPECT_EQ(ctx, view.value().synchronized->sourceInterval.sampleRate,
          expected.sampleRate);
```

Also assert released audio without a valid exact interval is rejected at the
canonical-media seam, not repaired by decode.

- [ ] **Step 2: Run RED**

```powershell
cmake --build out/build/x64-debug --target media_transcode_audio_lineage_node_tests --clean-first
```

Expected: compilation fails because synchronized decode input has no
`sourceInterval`.

- [ ] **Step 3: Remove nanosecond-to-sample reconstruction**

In `resolveMediaAudioDecodeInput`, copy the canonical buffer's required exact
interval into `MediaSynchronizedAudioDecodeInput`.

In `AudioDecodeNode`, delete the `MediaAudioSampleGrid` begin/end reconstruction
and use:

```cpp
const auto& sourceInterval = synchronized.sourceInterval;
if (sourceInterval.sampleRate != codecContext()->sample_rate) {
    return failure("AudioDecodeNode source interval sample rate conflicts with codec");
}
const MediaAudioIntervalFragment incomingFragment{
    synchronized.lineage,
    sourceInterval};
```

Keep `MediaAudioIntervalAccumulator::push` unchanged so any genuine gap,
overlap, rate change, or generation change remains terminal.

- [ ] **Step 4: Run focused GREEN tests**

Clean-first build and run:

```powershell
out\build\x64-debug\media_transcode_audio_lineage_node_tests.exe
out\build\x64-debug\media_transcode_audio_codec_lineage_tests.exe
out\build\x64-debug\media_transcode_av_sync_canonical_input_tests.exe
out\build\x64-debug\media_transcode_canonical_lineage_tests.exe
```

Expected: all exit `0`.

- [ ] **Step 5: Commit Task 3**

```powershell
git commit -m "fix: consume exact audio source lineage"
```

---

### Task 4: Final verification, review, and integration

**Files:**
- Modify only if verification exposes a defect directly within this design.

**Interfaces:**
- Consumes: completed Tasks 1-3.
- Produces: one reviewed PR revision with four green CI tiers.

- [ ] **Step 1: Normalize and inspect**

Ensure every changed text file is UTF-8 without BOM and CRLF. Run:

```powershell
git diff --check
rg -n "nearestSample\\(" src/internal/graph/nodes/audio/AudioDecodeNode.cpp
rg -n "MediaCanonicalAccessUnitBuffer::create\\(" src tests
```

Expected: no diff errors, no decode-side source-interval reconstruction, and
every factory call supplies the explicit interval argument.

- [ ] **Step 2: Run the strongest permitted clean-first build**

```powershell
cmake --build out/build/x64-debug --clean-first
```

Expected: exit `0`; suppress `/showIncludes` tree output from the displayed
log.

- [ ] **Step 3: Run safe local tests**

With `D:\mabs\local64\bin-video` first in process `PATH`, run:

```powershell
out\build\x64-debug\media_transcode_canonical_audio_source_timeline_tests.exe
out\build\x64-debug\media_transcode_canonical_lineage_tests.exe
out\build\x64-debug\media_transcode_av_sync_canonical_input_tests.exe
out\build\x64-debug\media_transcode_audio_lineage_node_tests.exe
out\build\x64-debug\media_transcode_audio_codec_lineage_tests.exe
out\build\x64-debug\media_transcode_core_tests.exe
out\build\x64-debug\media_transcode_planner_tests.exe
out\build\x64-debug\media_transcode_node_tests.exe
```

Expected: every executable exits `0`. Do not start the prohibited production
runtime executable.

- [ ] **Step 4: Fresh-agent review**

Require a fresh reviewer to inspect the complete PR for:

- one-time sample anchoring;
- no RTCP-refresh re-anchor;
- exact interval propagation;
- unchanged strict accumulator;
- planner-only parameters;
- no downstream fallback or tolerance.

Critical/Important findings must be zero before merge.

- [ ] **Step 5: Push and wait for CI**

Push the same branch and require:

- `deterministic`: pass
- `explicit-tiers (hardware)`: pass
- `explicit-tiers (integration)`: pass
- `explicit-tiers (performance)`: pass

- [ ] **Step 6: Merge**

Only after fresh review approval and four green checks, merge PR #21 to
`master` and verify the merged commit is reachable from the remote master
branch.
