# RKMPP Hardware Frame Capability Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the fixed RKMPP NV12 assumption with planner-selected, target-proven DRM PRIME frame and RGA transform contracts while preserving the shared Windows/RKMPP DAG and strict zero-copy execution.

**Architecture:** A prepared-input probe materializes the actual decoder DRM descriptor before graph creation. A target adapter turns that evidence and successfully negotiated RGA/encoder probes into typed per-edge contracts and transform tuples; the planner selects a compatible path and serializes it as the only runtime authority. Builders only bind the selected products, and generic video nodes validate storage, descriptor topology, buffer ownership, identity, and zero-copy facts without choosing formats.

**Tech Stack:** C++20, FFmpeg libavcodec/libavfilter/libavformat/libavutil, DRM PRIME descriptors, RKMPP, RGA, CMake/Ninja, Visual Studio 2026, Linux aarch64.

**Spec:** `docs/superpowers/specs/2026-08-17-industrial-realtime-resource-and-transport-shaping-design.md`

**Execution Status:** Frozen after plan authoring by user direction. Do not execute this plan until the public C/C++ library interface is designed, implemented, and accepted.

## Global Constraints

- Work only on `codex/rkmpp-zero-copy`; commit and push every completed task.
- `AV_PIX_FMT_DRM_PRIME` is the storage domain; DRM fourcc/layout is a separate fact.
- No RKMPP surface layout, dimensions, queue size, pool size, or negotiation policy may be a literal/default selected below the planner.
- Planner products are complete and fail before DAG creation when capability evidence is missing, ambiguous, or incompatible.
- Windows and RKMPP use the same planner, builder, edges, and generic nodes; only hardware/platform capability adapters differ.
- No software frame, upload, download, generic software scale, runtime negotiation, or fallback is permitted in explicit RKMPP mode.
- Use RAII for all FFmpeg contexts, frames, packets, buffer refs, prepared evidence, and filter graphs.
- Temporary TDD files/targets/binaries are permitted only for RED/GREEN and must be removed before each task commit.
- Do not add tracked CI, CTest, unit, integration, acceptance, hardware, or performance tests.
- All tracked text is UTF-8 without BOM and CRLF.
- Final builds and real-media acceptance use Release. RK builds run only after `ffenv on` and use 8 parallel jobs.
- Preserve the untracked FFmpeg headers and `out/`; delete only task-owned temporary TDD artifacts.

## Follow-on Plans

This plan is intentionally the first independently reviewable slice of the approved spec. A second plan will implement planner-derived realtime resource admission, removal of caller queue/startup capacities, aggregate transport shaping, and the complete `MediaRealtimeExecutablePlan`; the hardware contracts here become a nested member of that single authority rather than a parallel policy. A third plan will implement adaptive encoder generations, session-persistent transactions, and discontinuity contracts. Neither follow-on may duplicate or bypass the hardware contracts introduced here, and final production acceptance remains gated on the complete executable-plan projection.

---

## File Structure

- `src/internal/graph/model/MediaDrmFrameContract.{h,cpp}`: validated DRM descriptor topology and exact frame-contract equality.
- `src/internal/graph/model/MediaHardwareFrameTransformCapability.{h,cpp}`: one proven hardware transform tuple and evidence identity.
- `src/internal/graph/planner/capability/MediaPreparedHardwareFrameEvidence.{h,cpp}`: immutable, RAII-owned decoder-frame observation prepared before DAG creation.
- `src/internal/graph/planner/capability/MediaRkmppFrameCapabilityMaterializer.{h,cpp}`: RKMPP-only adapter that validates observed decoder output and materializes compatible no-filter/RGA/encoder paths.
- `src/internal/graph/planner/capability/MediaHardwareCapabilityProbe.{h,cpp}`: backend-neutral materialization entrypoint; no RKMPP policy.
- `src/internal/graph/model/MediaHardwareDescriptor.h`: attaches the optional typed DRM contract without changing non-DRM behavior.
- `src/internal/graph/planner/MediaPipelinePlanner.{h,cpp}`: selects per-edge contracts/transform tuple and rejects incomplete RKMPP products.
- `src/internal/graph/planner/MediaPipelineScorer.{h,cpp}`: scores only complete materialized candidates.
- `src/internal/graph/planner/capability/MediaVideoCapabilityScanner.cpp`: emits component identities only; removes fixed `format=nv12` and fixed RKMPP surface products.
- `src/internal/graph/builder/MediaVideoPlanOptionApplier.{h,cpp}`: serializes exact selected contracts/transform evidence into node options.
- `src/internal/graph/nodes/video/MediaVideoFrameContractValidator.{h,cpp}`: validates the complete DRM descriptor and produces runtime facts.
- `src/internal/graph/nodes/video/VideoDecodeNode.{h,cpp}`, `VideoFilterNode.{h,cpp}`, `VideoEncodeNode.{h,cpp}`: enforce the same contracts and report identity/zero-copy evidence.
- `CMakeLists.txt`: registers the focused production files only.

### Task 1: Typed DRM Descriptor and Transform Products

**Files:**
- Create: `src/internal/graph/model/MediaDrmFrameContract.h`
- Create: `src/internal/graph/model/MediaDrmFrameContract.cpp`
- Create: `src/internal/graph/model/MediaHardwareFrameTransformCapability.h`
- Create: `src/internal/graph/model/MediaHardwareFrameTransformCapability.cpp`
- Modify: `src/internal/graph/model/MediaHardwareDescriptor.h`
- Modify: `src/internal/graph/planner/MediaPipelinePlanner.h`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `MediaDrmFrameContract::create(...)`, `MediaDrmFrameContract::sameCompleteContract(...)`, `MediaHardwareFrameTransformCapability::create(...)`.
- Consumes: `MediaSize`, DRM fourcc/modifier/object/layer/plane facts obtained from FFmpeg descriptors.

- [ ] **Step 1: Write the temporary RED contract probe**

Create `out/tdd/rkmpp-frame-contract-tdd.cpp` with `apply_patch`:

```cpp
#include "internal/graph/model/MediaDrmFrameContract.h"
#include "internal/graph/model/MediaHardwareFrameTransformCapability.h"
#include <cassert>
using namespace media::ffmpeg::graph;
int main() {
    const auto input = MediaDrmFrameContract::create(
        MediaSize{2560, 1440}, {{0, 5529600, 1}},
        {{0x3231564eU, {{0, 1, 1}, {0, 1, 1}}}});
    const auto output = MediaDrmFrameContract::create(
        MediaSize{640, 360}, {{0, 345600, 1}},
        {{0x3231564eU, {{0, 1, 1}, {0, 1, 1}}}});
    assert(input && output);
    assert(!input.value().sameCompleteContract(output.value()));
    assert(MediaHardwareFrameTransformCapability::create(
        input.value(), output.value(), "scale_rkrga", "probe:rk:tuple:1"));
}
```

- [ ] **Step 2: Run RED**

Temporarily add a non-installed executable target for the file, configure the existing Release tree, and build only that target. Expected: compile failure because both product headers are absent. Record the error, then keep the temporary target uncommitted.

- [ ] **Step 3: Implement validating factories**

Use non-default-constructible products. The public shapes are:

```cpp
struct MediaDrmObjectContract {
    std::uint64_t modifier;
    std::uint64_t minimumSizeBytes;
    std::uint32_t sizeAlignmentBytes;
};
struct MediaDrmPlaneContract {
    std::uint32_t objectIndex;
    std::uint32_t pitchAlignmentBytes;
    std::uint32_t offsetAlignmentBytes;
};
struct MediaDrmLayerContract {
    std::uint32_t fourcc;
    std::vector<MediaDrmPlaneContract> planes;
};
class MediaDrmFrameContract final {
public:
    static ::media::Result<MediaDrmFrameContract> create(
        MediaSize size,
        std::vector<MediaDrmObjectContract> objects,
        std::vector<MediaDrmLayerContract> layers);
    bool sameCompleteContract(const MediaDrmFrameContract&) const noexcept;
};
class MediaHardwareFrameTransformCapability final {
public:
    static ::media::Result<MediaHardwareFrameTransformCapability> create(
        MediaDrmFrameContract input, MediaDrmFrameContract output,
        std::string componentName, std::string evidenceIdentity);
};
```

Reject zero dimensions/fourcc, empty objects/planes, invalid indices, zero alignment, overflow, empty component/evidence, and a tuple whose storage domain is not DRM PRIME. Add `std::optional<MediaDrmFrameContract> drmFrame` to `MediaHardwareDescriptor` and `std::optional<MediaHardwareFrameTransformCapability> frameTransform` to `MediaPipelineChainPlan`.

- [ ] **Step 4: Run GREEN and cleanup**

Build and run the temporary target. Expected: exit 0. Delete the temporary source, target stanza, and its exact object/executable/PDB/ILK artifacts; verify no `*tdd*` or temporary target remains in tracked or untracked task paths.

- [ ] **Step 5: Verify, review, commit, and push**

Build the affected production target, run `git diff --check`, scan modified files for UTF-8/CRLF, review factory invariants, then:

```powershell
git add CMakeLists.txt src/internal/graph/model src/internal/graph/planner/MediaPipelinePlanner.h
git commit -m "feat: add typed DRM frame transform contracts"
git push origin codex/rkmpp-zero-copy
```

### Task 2: Prepared Decoder-Frame Capability Evidence

**Files:**
- Create: `src/internal/graph/planner/capability/MediaPreparedHardwareFrameEvidence.h`
- Create: `src/internal/graph/planner/capability/MediaPreparedHardwareFrameEvidence.cpp`
- Modify: `src/internal/graph/planner/capability/MediaStreamCapabilityProbe.{h,cpp}`
- Modify: `src/internal/graph/planner/realtime/MediaPreparedRealtimeInput.{h,cpp}`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `MediaPreparedHardwareFrameEvidence::fromDecodedFrame(const AVFrame&, std::string evidenceIdentity)` and a move-only prepared evidence binding.
- Consumes: the same retained compressed packets/same prepared input used for runtime replay; it must not rebind a second realtime socket or discard media.

- [ ] **Step 1: Write temporary RED evidence probe**

Create a temporary TDD source that builds an owned `AVFrame` with `format=AV_PIX_FMT_DRM_PRIME`, a two-plane descriptor backed by `frame->buf[0]`, calls `fromDecodedFrame`, and asserts the captured size/fourcc/object/plane mapping. Add negative assertions for missing owner, invalid object index, and insufficient backing size.

- [ ] **Step 2: Run RED**

Build only the temporary target. Expected: missing `MediaPreparedHardwareFrameEvidence` API.

- [ ] **Step 3: Materialize immutable evidence before planning**

Implement a move-only RAII product that copies validated descriptor facts, not a borrowed descriptor pointer:

```cpp
class MediaPreparedHardwareFrameEvidence final {
public:
    static ::media::Result<MediaPreparedHardwareFrameEvidence> fromDecodedFrame(
        const AVFrame& frame, std::string evidenceIdentity);
    const MediaDrmFrameContract& drmFrame() const noexcept;
    AVPixelFormat storageFormat() const noexcept;
    const std::string& evidenceIdentity() const noexcept;
};
```

For realtime raw RTP, clone retained compressed packets from the already prepared same-socket replay buffer, decode one frame during preflight, then leave the original retained packets and socket binding unchanged. For file/generic URL probing, use the existing read-only input probe lifetime. Seal capture before taking the evidence snapshot. Failure to decode a frame, software output, or malformed descriptor fails explicit RKMPP planning.

- [ ] **Step 4: Run GREEN and cleanup**

Run the temporary target and a focused production compile. Delete the temporary source/target/build artifacts. Confirm no extra socket bind and no prepared-buffer byte/item mutation in diagnostics.

- [ ] **Step 5: Commit and push**

```powershell
git add CMakeLists.txt src/internal/graph/planner/capability src/internal/graph/planner/realtime
git commit -m "feat: prepare authoritative DRM frame evidence"
git push origin codex/rkmpp-zero-copy
```

### Task 3: RKMPP Transform Capability Materialization

**Files:**
- Create: `src/internal/graph/planner/capability/MediaRkmppFrameCapabilityMaterializer.h`
- Create: `src/internal/graph/planner/capability/MediaRkmppFrameCapabilityMaterializer.cpp`
- Modify: `src/internal/graph/planner/capability/MediaHardwareCapabilityProbe.{h,cpp}`
- Modify: `src/internal/graph/planner/capability/MediaVideoCapabilityScanner.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `MediaPreparedHardwareFrameEvidence`, requested target dimensions, selected decoder/filter/encoder component identities.
- Produces: `MediaRkmppFrameCapabilityMaterialization` containing exact decoder output, optional RGA tuple, encoder input, and probe evidence identities.

- [ ] **Step 1: Write temporary RED path probe**

Use an observed 2560x1440 DRM contract. Assert that no-resize accepts only the identical encoder contract; resize accepts a proven `(2560x1440 input, 640x360 output)` tuple; an encoder mismatch and an unproven tuple both fail.

- [ ] **Step 2: Run RED**

Expected: missing materializer and materialization product.

- [ ] **Step 3: Remove fixed RKMPP format policy**

Delete all production construction/validation of literal `surfacePixelFormat = "nv12"` and `:format=nv12`. Scanner emits only codec/filter identities and DRM PRIME storage requirement. It does not mark RKMPP available until materialization succeeds.

- [ ] **Step 4: Probe the actual tuple**

The adapter converts the observed fourcc to the FFmpeg filter surface name through a protocol mapping helper, not policy selection. It constructs the RGA expression from the selected observed layout and requested dimensions, negotiates a graph using the prepared DRM frame evidence, inspects the produced DRM descriptor, and opens the selected encoder with that exact output. Only a successfully negotiated tuple is returned. Do not allocate a fixed pool size; any required preparation bound comes from the backend capability/resource product or fails admission.

The materializer signature is:

```cpp
static ::media::Result<MediaRkmppFrameCapabilityMaterialization> materialize(
    const MediaPipelineChainPlan& componentPlan,
    const MediaPreparedHardwareFrameEvidence& decoderEvidence,
    MediaSize requestedOutput,
    const MediaEncoderRateControlPlan& rateControl);
```

- [ ] **Step 5: Run GREEN and cleanup**

Run the temporary probe with identity-size and resize tuples. On Windows, assert the RKMPP adapter returns platform unsupported while the shared model/planner compiles. Delete all temporary test artifacts.

- [ ] **Step 6: Commit and push**

```powershell
git add CMakeLists.txt src/internal/graph/planner/capability
git commit -m "feat: materialize target-proven RKMPP frame paths"
git push origin codex/rkmpp-zero-copy
```

### Task 4: Planner Selection and Exact Builder Projection

**Files:**
- Modify: `src/internal/graph/planner/MediaPipelinePlanner.{h,cpp}`
- Modify: `src/internal/graph/planner/MediaPipelineScorer.{h,cpp}`
- Modify: `src/internal/graph/builder/MediaVideoPlanOptionApplier.{h,cpp}`
- Modify: `src/internal/graph/builder/MediaTranscodeBranchBuilder.cpp`

**Interfaces:**
- Consumes: complete `MediaRkmppFrameCapabilityMaterialization`.
- Produces: one selected chain whose decoder/filter/encoder edge contracts and optional transform tuple are exact and builder-bindable.

- [ ] **Step 1: Write temporary RED planner probe**

Construct three candidates: incomplete RKMPP, complete no-filter, and complete RGA resize. Assert incomplete cannot score/select; no-filter requires exact decoder/encoder equality; resize requires decoder→tuple-input and tuple-output→encoder equality; any mismatch fails before builder invocation.

- [ ] **Step 2: Run RED**

Expected: current planner accepts the fixed RKMPP descriptor or lacks transform-path validation.

- [ ] **Step 3: Implement compatibility-path validation**

Replace `completeRkmppFrameContract` and filter-string equality with exact typed path checks. Scoring occurs only after materialization. Strict `--hardware-backend rkmpp` returns `HardwareUnavailable` when no materialized path exists and never selects software.

- [ ] **Step 4: Project exact node options**

Serialize every DRM field with stable indexed keys and explicit counts, plus transform component/evidence identity. Decode requires exact key arrays and rejects missing/extra/duplicate/overflowing fields. Builders may not infer a format from codec/backend names or filter strings.

- [ ] **Step 5: Run GREEN, cleanup, commit, and push**

Run the temporary probe, delete it and its artifacts, build affected production targets, then:

```powershell
git add src/internal/graph/planner src/internal/graph/builder
git commit -m "feat: select and bind exact RKMPP frame paths"
git push origin codex/rkmpp-zero-copy
```

### Task 5: Runtime DRM Contract Enforcement and Evidence

**Files:**
- Modify: `src/internal/graph/nodes/video/MediaVideoFrameContractValidator.{h,cpp}`
- Modify: `src/internal/graph/nodes/video/VideoDecodeNode.{h,cpp}`
- Modify: `src/internal/graph/nodes/video/VideoFilterNode.{h,cpp}`
- Modify: `src/internal/graph/nodes/video/VideoEncodeNode.{h,cpp}`

**Interfaces:**
- Consumes: exact builder-bound `MediaHardwareDescriptor`/`MediaDrmFrameContract`.
- Produces: `MediaVideoFrameRuntimeFacts` with descriptor identity, all object FDs, contract identity, and validation counters.

- [ ] **Step 1: Write temporary RED validator probe**

Create valid and invalid DRM descriptors covering object/layer count, fourcc, modifier, plane→object mapping, pitch/offset alignment, backing size, dimensions, and missing `AVBufferRef`. Assert every mismatch fails with the exact stage name.

- [ ] **Step 2: Run RED**

Expected: current validator cannot compare the complete typed topology.

- [ ] **Step 3: Enforce the full contract**

Extend `MediaVideoFrameRuntimeFacts` with `descriptorIdentity`, `contractEvidenceIdentity`, and all DRM object identities. Decode validates decoder output; filter validates both input and output; encoder validates input before `avcodec_send_frame`. Any software frame, transfer, malformed descriptor, unplanned modifier/layout/dimension change, or missing owner terminates.

- [ ] **Step 4: Enforce identity semantics**

No-filter paths compare the decoder descriptor/buffer identity carried through lineage with encoder admission on every frame. RGA paths require output identity different only when the selected transform says a new buffer is produced, while both sides remain DRM PRIME. Counters are actual events, never constants.

- [ ] **Step 5: Run GREEN, cleanup, commit, and push**

Run the validator probe, delete all temporary artifacts, build production nodes, then:

```powershell
git add src/internal/graph/nodes/video
git commit -m "feat: enforce exact DRM frame contracts at runtime"
git push origin codex/rkmpp-zero-copy
```

### Task 6: Diagnostics, Windows Release Gate, and RK Release Gate

**Files:**
- Modify: `src/internal/graph/planner/MediaPipelinePlanner.cpp`
- Modify: `src/internal/graph/nodes/video/VideoDecodeNode.cpp`
- Modify: `src/internal/graph/nodes/video/VideoFilterNode.cpp`
- Modify: `src/internal/graph/nodes/video/VideoEncodeNode.cpp`
- Modify: `docs/superpowers/plans/2026-08-18-rkmpp-hardware-frame-capability.md`

**Interfaces:**
- Consumes: selected capability path and runtime facts.
- Produces: human- and machine-auditable zero-copy evidence without changing policy.

- [ ] **Step 1: Add diagnostics**

Report storage domain, fourcc, dimensions, object/layer/plane topology, modifiers, transform evidence identity, per-stage frame count, software/upload/download count, identity preserved/replaced/mismatch count, CPU/RSS, queue state, drift, and final exit reason. The planner score line must no longer make `format=nv12` look like the storage contract; log `storage=drm_prime surface=<selected fourcc name> evidence=<id>`.

- [ ] **Step 2: Perform strict Windows Release build using the repository skill**

Read and follow `.agents/skills/building-with-vs2026/SKILL.md` exactly, adapting its documented full clean-first workflow to `out/build/x64-release`. Build all targets within the skill's strict wall-clock rule. Do not claim success from a focused compile.

- [ ] **Step 3: Run Windows shared-path regression**

Use direct absolute-path commands, the unchanged `D:\Code\MyCode\MediaTranscode\out\acceptance\test-continuous-120s.mp4`, visible VLC, and Release CLIs. Cover H.264→HEVC and HEVC→H.264 with resolution conversion plus CBR and VBR. Confirm the shared planner/builder/node changes do not alter Windows default hardware selection or introduce RKMPP-only graph topology.

- [ ] **Step 4: Sync committed source and build RK Release**

Create a Git archive from the committed branch, verify remote `pwd -P` equals `/home/firefly/Downloads/MediaTranscode`, replace only that directory, enter `ffenv on`, configure Release clean-first, and build with `--parallel 8`. Preserve `/home/firefly/Downloads/test16s.mp4` and existing backup directories.

- [ ] **Step 5: Run RK real-media matrix**

First run realtime VideoOnly with the real H.264 RTP source, H.264 output, 2K→640x360 RGA resize, 4 Mbps, 30 fps, and the existing MPEG-TS/RTP receiver path. Then cover H.264→HEVC and HEVC→H.264 plus no-resize H.264/H.265 paths. Use source-driven termination, visible local VLC, precise PIDs, and monitor CLI process CPU/RSS rather than aggregate shell CPU. Verify zero software/upload/download, selected tuple match, frame identity semantics, no corruption/stall, and truthful exit.

- [ ] **Step 6: Back up the verified Release CLI and publish commands**

After each accepted RK capability milestone, copy the exact Release CLI and required shared libraries into a timestamped `/home/firefly/Downloads/MediaTranscode-rkmpp-<milestone>-<timestamp>` directory. Record SHA-256, source commit, FFmpeg environment, and provide the exact absolute-path CLI/source/VLC commands. Do not label a build accepted if performance or picture quality fails.

- [ ] **Step 7: Commit and push diagnostics/evidence documentation**

```powershell
git add src/internal/graph docs/superpowers/plans/2026-08-18-rkmpp-hardware-frame-capability.md
git commit -m "docs: record RKMPP frame capability acceptance"
git push origin codex/rkmpp-zero-copy
```

### Task 7: Freeze, Dual Review, and PR Gate

**Files:**
- Modify: `QUALITY_SCORE.md`
- Modify: `docs/superpowers/plans/2026-08-18-rkmpp-hardware-frame-capability.md`

**Interfaces:**
- Consumes: frozen production diff and real Release evidence.
- Produces: a reviewed branch/PR; it does not add test infrastructure.

- [ ] **Step 1: Final hygiene**

Confirm UTF-8/CRLF, `git diff --check`, no temporary TDD source/target/binary, no credentials, no test script on Windows, no remote temporary script/process residue, and no unrelated/untracked user files staged.

- [ ] **Step 2: Update concise quality evidence**

Update `QUALITY_SCORE.md` only with the changed industrial axes: planner authority, zero-copy evidence, cross-platform reuse, resource bounds, runtime validation, and remaining risks.

- [ ] **Step 3: Freeze and dispatch two independent reviewers**

Give both reviewers the same fixed commit and full real-media evidence. One reviews Standards/RAII/Windows isolation; the other reviews Spec/DAG authority/RKMPP descriptor-transform correctness. Any Critical or Important finding reopens implementation and requires both reviewers to reassess the new fixed commit.

- [ ] **Step 4: Commit, push, and create/update PR**

After both reviewers state `APPROVED` with 0 Critical/0 Important:

```powershell
git add QUALITY_SCORE.md docs/superpowers/plans/2026-08-18-rkmpp-hardware-frame-capability.md
git commit -m "docs: finalize RKMPP capability delivery evidence"
git push origin codex/rkmpp-zero-copy
```

Create or update the PR from `codex/rkmpp-zero-copy`; attach exact Release commands/results, backup path/SHA-256, CPU/RSS/zero-copy evidence, dual-review verdicts, and remaining risks.
