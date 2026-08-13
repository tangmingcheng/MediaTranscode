# Canonical MPEG-TS Datagram Pacing Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace transport-local blocking pacing with one planner-owned, non-blocking canonical MPEG-TS datagram emission schedule shared by UDP and MP2T/RTP.

**Architecture:** The realtime planner produces a required typed emission plan in `MediaProjectMpegTsRuntimeOutputPlan`. `MediaTsMuxSession` consumes that plan and retains at most one packet cursor while its `poll()` method emits due datagrams and returns the next canonical deadline. Datagram sinks remain transport-only and never sleep or infer pacing.

**Tech Stack:** C++20, existing MediaTranscode DAG/planner/runtime, FFmpeg 5/7 compatibility layer, CMake/Ninja, VS2026, RKMPP/RGA FFmpeg selected by `ffenv on`.

## Global Constraints

- Work only on `codex/rkmpp-zero-copy`; never modify `master` or create another worktree.
- Planner is the only policy authority. Missing or contradictory pacing facts fail before DAG startup.
- Windows, Linux, UDP, and MP2T/RTP share the canonical mux scheduling implementation; only socket and hardware APIs may differ.
- Do not lower source resolution, bitrate, frame rate, duration, codec conversion, scaling, or any acceptance threshold.
- Temporary TDD sources and targets must demonstrate RED then GREEN and must be deleted before each production commit.
- Do not add persistent CI, CTest, unit, integration, acceptance, performance, or hardware tests.
- Preserve RAII transactions, cursor ownership, generation lineage, clock commits, and stop/abort semantics.
- All committed text is UTF-8 without BOM and CRLF.
- Every task ends with review, commit, and push to the same branch.
- Final Windows build uses the repository VS2026 clean-first script and its 120-second hard limit.
- Final RK build runs under `ffenv on` and may use eight parallel jobs.

---

## File Structure

**Create**

- `src/internal/graph/planner/realtime/MediaTsDatagramEmissionPlan.h/.cpp`: validated immutable planner product for wire rate, burst, lateness, payload bound, and per-datagram overhead.
- `src/internal/graph/protocol/mpegts/MediaTsDatagramEmissionSchedule.h/.cpp`: pure canonical-time token transaction that prepares, commits, or cancels one datagram deadline without sleeping.
- `src/internal/graph/protocol/mpegts/MediaTsPendingEmission.h/.cpp`: owns one cursor, its prepared MPEG-TS clock transaction, and the access-unit framing lifetime.
- `src/internal/graph/diagnostics/MediaTsEmissionDiagnostics.h/.cpp`: real counters and snapshot formatting for emission plan, waits, lateness, pending bytes, and transport pressure.

**Modify**

- `src/internal/graph/planner/realtime/MediaRealtimeOutputPlanningDraft.h`
- `src/internal/graph/planner/realtime/MediaRealtimeOutputPolicyPlanner.cpp`
- `src/internal/graph/planner/realtime/MediaRealtimeProtocolOutputPlan.h/.cpp`
- `src/internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlanner.cpp`
- `src/internal/graph/planner/realtime/MediaRealtimeVideoRuntimePlanner.cpp`
- `src/internal/graph/nodes/output/MediaProjectMpegTsPlanSourceNodePlanCodec.cpp`
- `src/internal/graph/nodes/mux/ProjectMpegTsMuxSessionAdapter.h/.cpp`
- `src/internal/graph/nodes/mux/ProjectMpegTsDatagramSinkFactory.cpp`
- `src/internal/graph/nodes/mux/MediaMuxSession.h`
- `src/internal/graph/nodes/mux/FFmpegFileMuxSession.h/.cpp`
- `src/internal/graph/nodes/mux/FileMuxNode.cpp`
- `src/internal/graph/protocol/mpegts/MediaTsMuxSession.h/.cpp`
- `src/internal/graph/protocol/mpegts/MediaTsPacketBatchWriter.h/.cpp`
- `src/internal/graph/nodes/output/MediaMpegTsRtpDatagramSink.h/.cpp`
- `src/internal/graph/runtime/ffmpeg/FFmpegPacedAvio.cpp`
- `ARCHITECTURE.md`, `QUALITY_SCORE.md`, and `docs/completed/rkmpp-zero-copy.md`

**Delete**

- `src/internal/graph/runtime/io/MediaWritePacingClock.h/.cpp`

---

### Task 1: Typed Datagram Emission Product

**Files:**

- Create: `src/internal/graph/planner/realtime/MediaTsDatagramEmissionPlan.h`
- Create: `src/internal/graph/planner/realtime/MediaTsDatagramEmissionPlan.cpp`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeOutputPlanningDraft.h`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeOutputPolicyPlanner.cpp`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeProtocolOutputPlan.h`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeProtocolOutputPlan.cpp`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlanner.cpp`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeVideoRuntimePlanner.cpp`
- Modify: `src/internal/graph/nodes/output/MediaProjectMpegTsPlanSourceNodePlanCodec.cpp`
- Temporary test: `out/tdd/media_ts_datagram_emission_plan_tdd.cpp`

**Interfaces:**

- Produces:

```cpp
class MediaTsDatagramEmissionPlan final {
public:
    static ::media::Result<MediaTsDatagramEmissionPlan> create(
        std::int64_t wireBytesPerSecond,
        std::size_t burstWireBytes,
        MediaRunningTime maximumLateness,
        std::size_t maximumPayloadBytes,
        std::size_t perDatagramOverheadBytes);
    std::int64_t wireBytesPerSecond() const noexcept;
    std::size_t burstWireBytes() const noexcept;
    MediaRunningTime maximumLateness() const noexcept;
    std::size_t maximumPayloadBytes() const noexcept;
    std::size_t perDatagramOverheadBytes() const noexcept;
    std::size_t maximumWireDatagramBytes() const noexcept;
    friend bool operator==(const MediaTsDatagramEmissionPlan&,
                           const MediaTsDatagramEmissionPlan&) = default;
};
```

- `MediaProjectMpegTsRuntimeOutputPlan` gains required field
  `MediaTsDatagramEmissionPlan emission` before its transport variant.
- `MediaRealtimeMpegTsOutputPlanningDraft` replaces two primitive pacing
  fields with `std::optional<MediaTsDatagramEmissionPlan> emission`.

- [ ] **Step 1: Write the failing temporary plan test**

Create a temporary executable that asserts rejection of zero rate, burst below
one complete datagram, zero lateness, payload overflow, and an RTP overhead
mismatch. Assert a valid UDP product uses zero overhead and a valid MP2T/RTP
product uses 12 bytes.

```cpp
auto invalid = MediaTsDatagramEmissionPlan::create(0, 2656, ms(100), 1316, 12);
assert(!invalid);
auto valid = MediaTsDatagramEmissionPlan::create(
    1'271'000, 2656, ms(100), 1316, 12);
assert(valid);
assert(valid.value().maximumWireDatagramBytes() == 1328);
```

- [ ] **Step 2: Run RED**

Temporarily add one guarded CMake executable for the source under `out/tdd`,
configure a disposable `out/tdd/build`, and build it. Expected: compilation
fails because `MediaTsDatagramEmissionPlan` does not exist. Capture the exact
failure, then keep the temporary target untracked.

```powershell
cmd.exe /d /s /c "call D:\VisualStudio2026\Common7\Tools\VsDevCmd.bat -arch=amd64 -host_arch=amd64 >nul && D:\VisualStudio2026\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe -S D:\Code\MyCode\MediaTranscode -B D:\Code\MyCode\MediaTranscode\out\tdd\build -G Ninja -DCMAKE_CXX_COMPILER=cl.exe -DCMAKE_BUILD_TYPE=Debug -DMEDIA_TRANSCODE_TEMP_TDD=ON -DCMAKE_MAKE_PROGRAM=D:\VisualStudio2026\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe && D:\VisualStudio2026\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe --build D:\Code\MyCode\MediaTranscode\out\tdd\build --target media_ts_datagram_emission_plan_tdd"
```

- [ ] **Step 3: Implement the immutable product and planner calculation**

Compute the media bitrate from resolved video plus optional audio. Convert to
bytes per second with the existing 5/4 headroom using checked arithmetic. Use
`maximumPacketsPerDatagram * 188` as payload bound, add 12 bytes only for RTP,
set burst to exactly two maximum wire datagrams, and set maximum lateness to
the Project MPEG-TS transport decode lead. Both UDP and RTP must receive this
product.

- [ ] **Step 4: Serialize and validate the complete product**

Add exact graph option fields for all five values. Decode through
`MediaTsDatagramEmissionPlan::create`, reject missing/extra keys, include the
product in planner-vs-decoded equality, and require it in both video-only and
A/V runtime planners.

- [ ] **Step 5: Run GREEN and delete temporary test infrastructure**

Build and run the disposable target; expected exit code `0`. Remove the test
source, temporary CMake option/target, and `out/tdd/build`. Confirm `git status`
contains no test source or test target.

- [ ] **Step 6: Review, commit, and push**

Check checked arithmetic, enum stability, exact option keys, UTF-8/CRLF, and
`git diff --check`. Commit:

```text
feat: plan canonical MPEG-TS datagram emission
```

Push `codex/rkmpp-zero-copy`.

---

### Task 2: Transactional Canonical Deadline Schedule

**Files:**

- Create: `src/internal/graph/protocol/mpegts/MediaTsDatagramEmissionSchedule.h`
- Create: `src/internal/graph/protocol/mpegts/MediaTsDatagramEmissionSchedule.cpp`
- Temporary test: `out/tdd/media_ts_datagram_schedule_tdd.cpp`

**Interfaces:**

```cpp
class MediaTsPreparedDatagramEmission final {
public:
    ~MediaTsPreparedDatagramEmission();
    MediaTsPreparedDatagramEmission(MediaTsPreparedDatagramEmission&&) noexcept;
    MediaRunningTime deadline() const noexcept;
    MediaRunningTime plannedWait() const noexcept;
    std::size_t wireBytes() const noexcept;
};

class MediaTsDatagramEmissionSchedule final {
public:
    static ::media::Result<MediaTsDatagramEmissionSchedule> create(
        MediaTsDatagramEmissionPlan plan,
        MediaRunningTime origin);
    ::media::Result<MediaTsPreparedDatagramEmission> prepare(
        std::size_t payloadBytes,
        MediaRunningTime notBefore);
    ::media::Status commit(MediaTsPreparedDatagramEmission&& prepared);
    const MediaTsDatagramEmissionPlan& plan() const noexcept;
};
```

- [ ] **Step 1: Write failing transactional schedule tests**

Test exactly these facts with real schedule state:

```cpp
// Two maximum datagrams consume the planned burst at origin.
assert(first.deadline() == origin);
assert(second.deadline() == origin);
// The third deadline is origin + wireBytes / wireBytesPerSecond.
assert(third.deadline() > origin);
// Destroying an uncommitted transaction preserves the previous state.
assert(reprepared.deadline() == third.deadline());
// A notBefore jump refills credit without moving time backward.
// Overflow and maximum-lateness violations fail.
```

- [ ] **Step 2: Run RED**

Build the guarded disposable target. Expected: missing schedule types and
methods. Confirm the failure is not caused by CMake or include paths.

```powershell
cmd.exe /d /s /c "call D:\VisualStudio2026\Common7\Tools\VsDevCmd.bat -arch=amd64 -host_arch=amd64 >nul && D:\VisualStudio2026\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe -S D:\Code\MyCode\MediaTranscode -B D:\Code\MyCode\MediaTranscode\out\tdd\build -G Ninja -DCMAKE_CXX_COMPILER=cl.exe -DCMAKE_BUILD_TYPE=Debug -DMEDIA_TRANSCODE_TEMP_TDD=ON -DCMAKE_MAKE_PROGRAM=D:\VisualStudio2026\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe && D:\VisualStudio2026\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe --build D:\Code\MyCode\MediaTranscode\out\tdd\build --target media_ts_datagram_schedule_tdd"
```

- [ ] **Step 3: Implement prepare/commit/cancel**

Use integer nanosecond arithmetic with quotient/remainder decomposition; do
not use floating point. Store committed credit, committed master time, and a
single pending revision. Preparation computes but does not mutate committed
state. The prepared RAII destructor cancels its revision. Commit rejects stale
or foreign transactions.

- [ ] **Step 4: Run GREEN and remove temporary files**

Run the executable twice with exit code `0`, remove its source/target/build
tree, and verify no test residue remains.

- [ ] **Step 5: Review, commit, and push**

Review boundary arithmetic, move semantics, cancellation, and absence of wall
clock/sleep calls. Commit:

```text
feat: add transactional MPEG-TS emission schedule
```

Push the branch.

---

### Task 3: Non-blocking Mux Cursor and Bounded Backpressure

**Files:**

- Create: `src/internal/graph/protocol/mpegts/MediaTsPendingEmission.h`
- Create: `src/internal/graph/protocol/mpegts/MediaTsPendingEmission.cpp`
- Modify: `src/internal/graph/protocol/mpegts/MediaTsPacketBatchWriter.h`
- Modify: `src/internal/graph/protocol/mpegts/MediaTsPacketBatchWriter.cpp`
- Modify: `src/internal/graph/protocol/mpegts/MediaTsMuxSession.h`
- Modify: `src/internal/graph/protocol/mpegts/MediaTsMuxSession.cpp`
- Modify: `src/internal/graph/nodes/mux/ProjectMpegTsMuxSessionAdapter.h`
- Modify: `src/internal/graph/nodes/mux/ProjectMpegTsMuxSessionAdapter.cpp`
- Modify: `src/internal/graph/nodes/mux/ProjectMpegTsDatagramSinkFactory.cpp`
- Modify: `src/internal/graph/nodes/mux/MediaMuxSession.h`
- Modify: `src/internal/graph/nodes/mux/FFmpegFileMuxSession.h`
- Modify: `src/internal/graph/nodes/mux/FFmpegFileMuxSession.cpp`
- Modify: `src/internal/graph/nodes/mux/FileMuxNode.cpp`
- Temporary test: `out/tdd/media_ts_nonblocking_mux_tdd.cpp`

**Interfaces:**

```cpp
struct MediaTsBatchWriteResult final {
    std::size_t packetsWritten;
    std::size_t payloadBytes;
    bool cursorFinished;
};

::media::Result<MediaTsBatchWriteResult>
MediaTsPacketBatchWriter::writeNext(
    MediaTsPacketCursor& cursor,
    MediaRunningTime emitOnMaster);

struct MediaTsMuxPollResult final {
    bool progressed;
    std::optional<MediaRunningTime> nextDeadline;
    std::size_t packetsWritten;
};

bool MediaTsMuxSession::hasPendingEmission() const noexcept;
::media::Result<MediaTsMuxPollResult>
MediaTsMuxSession::poll(MediaRunningTime masterNow);

// Existing graph-facing session contract.
virtual bool MediaMuxSession::hasPendingOutput() const noexcept = 0;
```

- [ ] **Step 1: Write the failing mux state-machine test**

Use a temporary recording datagram sink and real TS packetizer. Submit one
video PES requiring more than two datagrams. Assert `writeAccessUnit` retains
the cursor, sends only the due burst, rejects a second access unit while
pending, returns a future deadline, and finishes through repeated `poll`
calls. Assert abort cancels cursor and prepared clock state without a sink
commit.

- [ ] **Step 2: Run RED**

Expected failure: current `writeCursor` drains every datagram synchronously and
`MediaTsMuxSession` has no pending-emission state.

```powershell
cmd.exe /d /s /c "call D:\VisualStudio2026\Common7\Tools\VsDevCmd.bat -arch=amd64 -host_arch=amd64 >nul && D:\VisualStudio2026\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe -S D:\Code\MyCode\MediaTranscode -B D:\Code\MyCode\MediaTranscode\out\tdd\build -G Ninja -DCMAKE_CXX_COMPILER=cl.exe -DCMAKE_BUILD_TYPE=Debug -DMEDIA_TRANSCODE_TEMP_TDD=ON -DCMAKE_MAKE_PROGRAM=D:\VisualStudio2026\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe && D:\VisualStudio2026\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe --build D:\Code\MyCode\MediaTranscode\out\tdd\build --target media_ts_nonblocking_mux_tdd"
```

- [ ] **Step 3: Implement single-batch writer transactions**

Replace the draining loop with `writeNext`. Prepare at most
`maximumPacketsPerDatagram`, call the sink once, and commit the cursor token
only after the exact complete write. Preserve short-write and first-failure
behavior.

- [ ] **Step 4: Implement the retained mux emission state**

Retain one cursor, framing workspace, stream clock transaction, and datagram
schedule transaction. PAT, PMT, PCR, audio PES, and video PES use the same
state machine. Commit packet clock only after the final cursor batch commits.
Reject a second access unit while pending rather than copying it internally.

- [ ] **Step 5: Integrate graph backpressure**

Expose the retained-cursor fact through
`MediaMuxSession::hasPendingOutput()`: the Project adapter delegates to
`MediaTsMuxSession::hasPendingEmission()`, while `FFmpegFileMuxSession`
returns false. Before `FileMuxNode` pops an encoded input, call the existing
graph-facing session `poll` when `hasPendingOutput()` is true. The Project
adapter converts the protocol poll deadline through the existing output
authority `deadlineWait`; do not add a second deadline representation to the
node. Only dequeue another access unit after the prior cursor finishes. Abort
must bypass pending deadlines.

- [ ] **Step 6: Run GREEN and delete temporary test infrastructure**

Run the state-machine executable with exit code `0`; repeat abort and lateness
cases. Delete the temporary source, CMake target, and disposable build tree.

- [ ] **Step 7: Review, commit, and push**

Review single-cursor ownership, no extra payload copy, generation safety,
clock/cursor commit order, queue bounds, finish/flush, and abort. Commit:

```text
refactor: schedule MPEG-TS datagrams from canonical mux
```

Push the branch.

---

### Task 4: Transport Purity and Real Pacing Diagnostics

**Files:**

- Create: `src/internal/graph/diagnostics/MediaTsEmissionDiagnostics.h`
- Create: `src/internal/graph/diagnostics/MediaTsEmissionDiagnostics.cpp`
- Modify: `src/internal/graph/nodes/output/MediaMpegTsRtpDatagramSink.h`
- Modify: `src/internal/graph/nodes/output/MediaMpegTsRtpDatagramSink.cpp`
- Modify: `src/internal/graph/runtime/ffmpeg/FFmpegPacedAvio.cpp`
- Delete: `src/internal/graph/runtime/io/MediaWritePacingClock.h`
- Delete: `src/internal/graph/runtime/io/MediaWritePacingClock.cpp`
- Temporary test: `out/tdd/media_ts_emission_diagnostics_tdd.cpp`

**Interfaces:**

```cpp
struct MediaTsEmissionSnapshot final {
    std::uint64_t datagrams;
    std::uint64_t wireBytes;
    std::uint64_t immediateDeadlines;
    std::uint64_t deferredDeadlines;
    std::int64_t plannedWaitNanoseconds;
    std::uint64_t lateDatagrams;
    std::int64_t maximumLatenessNanoseconds;
    std::size_t pendingBytes;
    std::size_t peakPendingBytes;
    std::uint64_t pressureFailures;
};
```

- [ ] **Step 1: Write failing diagnostics tests**

Record immediate, deferred, late, committed, cancelled, and pressure events.
Assert only committed writes increase datagram/wire-byte totals and every
reported value is derived from an event.

- [ ] **Step 2: Run RED**

Expected failure: no typed emission diagnostics exist.

```powershell
cmd.exe /d /s /c "call D:\VisualStudio2026\Common7\Tools\VsDevCmd.bat -arch=amd64 -host_arch=amd64 >nul && D:\VisualStudio2026\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe -S D:\Code\MyCode\MediaTranscode -B D:\Code\MyCode\MediaTranscode\out\tdd\build -G Ninja -DCMAKE_CXX_COMPILER=cl.exe -DCMAKE_BUILD_TYPE=Debug -DMEDIA_TRANSCODE_TEMP_TDD=ON -DCMAKE_MAKE_PROGRAM=D:\VisualStudio2026\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe && D:\VisualStudio2026\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe --build D:\Code\MyCode\MediaTranscode\out\tdd\build --target media_ts_emission_diagnostics_tdd"
```

- [ ] **Step 3: Implement and emit diagnostics**

Log the selected plan once at activation, periodic cumulative snapshots during
runtime, and one final snapshot with exit reason. Add pending and lateness
events at the mux schedule boundary and pressure failures at the sink boundary.

- [ ] **Step 4: Remove the second transport authority**

Delete `MediaWritePacingClock` from the MP2T sink. Restore
`FFmpegPacedAvio.cpp` to its exact `9b702015` cumulative AVIO pacing behavior;
do not route separate RTP through the new TS schedule. Confirm the RTP sink
contains no wait, sleep, rate, burst, or fallback logic.

- [ ] **Step 5: Run GREEN and delete temporary tests**

Run with exit code `0`, remove all temporary test artifacts, and search the
repository for `MediaWritePacingClock` and transport-local pacing fields;
expected: no matches outside historical documentation.

- [ ] **Step 6: Review, commit, and push**

Review real counter sources, final diagnostic emission, transport purity,
FFmpeg AVIO restoration, UTF-8/CRLF, and residue. Commit:

```text
fix: keep MPEG-TS pacing under canonical authority
```

Push the branch.

---

### Task 5: Build and Real-Media Acceptance

**Files:**

- Modify: `ARCHITECTURE.md`
- Modify: `QUALITY_SCORE.md`
- Modify: `docs/completed/rkmpp-zero-copy.md`
- Remote temporary script: `/tmp/start-rkmpp-canonical-pacing.sh`

- [ ] **Step 1: Run final Windows clean-first build**

Run the documented VS2026 script. Require exit code `0`, all targets rebuilt,
both CLI executables linked, and elapsed time below 120 seconds. Do not run
CTest.

- [ ] **Step 2: Execute Windows same-spec route with absolute commands**

Use visible `D:\VideoLAN\VLC\vlc.exe`, hidden local FFmpeg, and hidden realtime
CLI commands without a Windows script. Use the fixed 120-second 2K source,
HEVC to H.264, 2560x1440 to 1280x720 scaling, AAC, separate RTP input with
automatic video fmtp, and MP2T/RTP output. Record exact commands, PIDs,
receiver RTP sequence statistics, pacing telemetry, CPU/RSS, drops, A/V drift,
and exit reason.

- [ ] **Step 3: Sync the committed archive and build RK with eight jobs**

Verify remote `pwd -P` equals
`/home/firefly/Downloads/MediaTranscode`, replace only that directory from the
committed Git archive, then run `ffenv on`, a clean Release configure, and an
eight-job all-target build. Require both CLI binaries.

- [ ] **Step 4: Execute RK-to-Windows human-eye acceptance**

Create `/tmp/start-rkmpp-canonical-pacing.sh` containing the exact approved
2K HEVC/AAC source, HEVC to H.264 RKMPP conversion, RGA 2560x1440 to 1280x720,
8 Mbps, 30 fps, GOP 60, separate RTP input, automatic video fmtp, and MP2T/RTP
to Windows. Check ports first, record exact CLI/source/monitor PIDs, start
visible Windows VLC before the source, and monitor CLI CPU/RSS plus pacing and
zero-copy counters.

- [ ] **Step 5: Verify receiver sequence and media quality**

After the VLC human-eye pass, repeat the unchanged full-spec route once with a
Windows FFmpeg receiver instead of VLC and capture its RTP sequence loss/reorder
diagnostics plus the complete decoded output. This supplemental run must not
replace the VLC pass. Require no sustained loss/reorder, no bottom-frame
corruption, stable A/V, `software_frame=0`, `download=0`, `upload=0`, no runtime
fallback, no dropped buffers, bounded pending bytes, and stable RSS.

- [ ] **Step 6: Stop by source and clean residue**

Stop only the source when an explicit stop check is needed; otherwise allow the
120-second source to finish. Record the CLI's actual exit truthfully. Delete
the remote script, close VLC/receiver, and confirm no process, port, script, or
temporary capture remains. Do not classify the known RTP clock-degradation
exit as a pacing success; track it for the separate termination change.

- [ ] **Step 7: Update architecture, completion, and score**

Document the single canonical authority, typed plan, non-blocking deadline
flow, real telemetry, commands, observed results, remaining input-termination
risk, and updated industrial DAG score. Keep documents concise.

- [ ] **Step 8: Final review, commit, and push**

Check the complete branch diff, UTF-8/CRLF, `git diff --check`, no tracked or
untracked temporary test files, no script residue, and preservation of user
FFmpeg headers and `out/`. Commit:

```text
docs: record canonical MPEG-TS pacing acceptance
```

Push the branch.

---

### Task 6: Pull Request and Independent Review

**Files:**

- No production file is modified unless review finds an actionable issue.

- [ ] **Step 1: Create the pull request**

Create a ready PR from `codex/rkmpp-zero-copy` with the planner contract,
canonical scheduling architecture, Windows/RK evidence, exact media commands,
and remaining separate RTP termination risk.

- [ ] **Step 2: Dispatch a fresh independent reviewer**

Require review of planner authority, absence of a second pacing clock,
transactional RAII, single-cursor ownership, bounded backpressure, no copies,
diagnostic truthfulness, Windows/RK reuse, receiver sequence evidence, CPU/RSS,
and source-end exit truthfulness.

- [ ] **Step 3: Fix and revalidate until approved**

For each Critical or Important finding, add a new temporary RED/GREEN test,
delete it after the fix, repeat affected Windows/RK real-media gates, commit,
push, and request a fresh review. Finish only after explicit approval.
