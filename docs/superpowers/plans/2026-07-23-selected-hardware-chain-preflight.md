# Selected Hardware Chain Preflight Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Select the highest-scoring structurally available pipeline without creating devices, then preflight exactly that selected chain once.

**Architecture:** `MediaPipelinePlanner` separates pure candidate selection from selected-chain preflight. The plan stores the selected candidate before preflight, and a failed preflight terminates planning without probing a lower-scoring chain.

**Tech Stack:** C++20, FFmpeg hardware contexts, CMake presets, CTest.

## Global Constraints

- Hardware is the default; software candidates exist only when hardware is explicitly disabled.
- Only the selected hardware chain may create temporary FFmpeg devices during planner preflight.
- A selected-chain preflight failure returns `HardwareUnavailable`; there is no lower-ranked fallback.
- Runtime nodes consume the immutable selected plan and retain no fallback authority.
- Every compilation uses `--clean-first`, and include-tree output is filtered.
- Do not run `media_transcode_av_sync_production_runtime_integration_tests.exe`.
- Modified text files remain UTF-8 without BOM and use CRLF line endings.

---

### Task 1: Separate Pure Selection from Selected-Chain Preflight

**Files:**
- Modify: `src/internal/graph/planner/MediaPipelinePlanner.h`
- Modify: `src/internal/graph/planner/MediaPipelinePlanner.cpp`
- Test: `tests/unit/test_planner.cpp`

**Interfaces:**
- Produces: `MediaPipelinePlanner::selectHighestRankedCandidate(const std::vector<MediaPipelineChainPlan>&, const MediaPipelinePlannerOptions&)`
- Produces: `MediaPipelinePlanner::preflightSelectedCandidate(MediaPipelineChainPlan&, const MediaPipelinePlannerOptions&, MediaHardwareCapabilityProbe&)`
- Removes: `MediaPipelinePlanner::selectRankedCandidate(...)`

- [ ] **Step 1: Replace the fallback test with a single-selected-chain RED test**

Replace `testPlannerContinuesAfterHighestRankedHardwareChainCannotOpenEncoder`
with:

```cpp
void testPlannerPreflightsOnlyHighestRankedHardwareChain(TestContext& ctx)
{
    MediaPipelinePlannerOptions options(false, true, false, true);
    options.preferredHardware = "auto";
    std::vector<std::string> attempted;
    MediaHardwareCapabilityProbe probe(
        [&](const MediaPipelineChainPlan& candidate,
            const MediaPipelinePlannerOptions&)
        {
            attempted.push_back(candidate.label);
            return MediaHardwareCapability{
                false, "test selected encoder open failed"};
        });

    auto candidates = MediaPipelineScorer::scoreAndSortChains(
        {makeAvailableHardwareChain(
             "qsv", MediaHardwareDeviceKind::QSV, "qsv", 20),
         makeAvailableHardwareChain(
             "cuda", MediaHardwareDeviceKind::CUDA, "cuda", 30),
         makeAvailableHardwareChain(
             "d3d11va", MediaHardwareDeviceKind::D3D11VA, "d3d11va", 10)},
        options);
    const auto selected =
        MediaPipelinePlanner::selectHighestRankedCandidate(candidates, options);

    EXPECT_TRUE(ctx, selected);
    if (!selected) return;
    EXPECT_EQ(ctx, candidates.at(selected.value()).label, std::string("cuda"));
    MediaPipelineChainPlan selectedChain = candidates.at(selected.value());
    const auto preflight = MediaPipelinePlanner::preflightSelectedCandidate(
        selectedChain, options, probe);

    EXPECT_FALSE(ctx, preflight);
    EXPECT_EQ(ctx, attempted.size(), std::size_t{1});
    if (!attempted.empty()) {
        EXPECT_EQ(ctx, attempted.front(), std::string("cuda"));
    }
}
```

Update the existing implicit-software and explicit-software tests to call
`selectHighestRankedCandidate`, copy the selected chain, and then call
`preflightSelectedCandidate`. The hardware-failure test must expect one probe;
the explicit-software test must expect zero probes.

- [ ] **Step 2: Run the planner test to verify RED**

Run:

```powershell
cmd.exe /d /s /c '"call D:\VisualStudio2026\Common7\Tools\VsDevCmd.bat -arch=x64 -host_arch=x64 >nul && cmake --build --preset deterministic --clean-first --target media_transcode_planner_tests"'
out\build\x64-debug-deterministic\media_transcode_planner_tests.exe
```

Expected: compilation fails because the two new planner methods do not exist.

- [ ] **Step 3: Add the pure selection and preflight interfaces**

In `MediaPipelinePlanner.h`, replace `selectRankedCandidate` with:

```cpp
static ::media::Result<std::size_t> selectHighestRankedCandidate(
    const std::vector<MediaPipelineChainPlan>& candidates,
    const MediaPipelinePlannerOptions& options);

static ::media::Status preflightSelectedCandidate(
    MediaPipelineChainPlan& selected,
    const MediaPipelinePlannerOptions& options,
    MediaHardwareCapabilityProbe& hardwareProbe);
```

In `MediaPipelinePlanner.cpp`, implement pure selection without invoking the
probe:

```cpp
::media::Result<std::size_t>
MediaPipelinePlanner::selectHighestRankedCandidate(
    const std::vector<MediaPipelineChainPlan>& candidates,
    const MediaPipelinePlannerOptions& options)
{
    for (std::size_t index = 0; index < candidates.size(); ++index) {
        const MediaPipelineChainPlan& candidate = candidates[index];
        if (!candidate.available) continue;
        if (options.disableHardware) {
            if (isSoftwareChain(candidate, options)) {
                return ::media::Result<std::size_t>::success(index);
            }
            continue;
        }
        if (candidate.allHardware) {
            return ::media::Result<std::size_t>::success(index);
        }
    }
    return ::media::Result<std::size_t>::failure(
        ::media::ErrorInfo::hardwareUnavailable(
            options.disableHardware
                ? "no available explicit software decoder/filter/encoder chain found"
                : "no structurally available hardware decoder/filter/encoder chain found"));
}

::media::Status MediaPipelinePlanner::preflightSelectedCandidate(
    MediaPipelineChainPlan& selected,
    const MediaPipelinePlannerOptions& options,
    MediaHardwareCapabilityProbe& hardwareProbe)
{
    if (options.disableHardware) {
        return ::media::Status::success();
    }
    return hardwareProbe.validate(selected, options);
}
```

- [ ] **Step 4: Store selection before preflight**

In `buildVideoTranscodePlan`, replace the current combined selection loop with:

```cpp
auto selected = MediaPipelinePlanner::selectHighestRankedCandidate(
    plan.candidates, options);
if (!selected) {
    return ::media::Result<MediaPipelinePlan>::failure(selected.error());
}

plan.selected = plan.candidates.at(selected.value());
MediaHardwareCapabilityProbe hardwareProbe;
if (auto preflight = MediaPipelinePlanner::preflightSelectedCandidate(
        plan.selected, options, hardwareProbe); !preflight) {
    return ::media::Result<MediaPipelinePlan>::failure(preflight.error());
}
logSelectedPlan(options, plan);
```

Remove the old loop that rescored failed candidates and continued to the next
hardware backend.

- [ ] **Step 5: Run focused GREEN verification**

Run the same clean-first build command from Step 2, filtering lines beginning
with `娉ㄦ剰: 鍖呭惈鏂囦欢:` or `Note: including file:` from displayed output. Then
run:

```powershell
out\build\x64-debug-deterministic\media_transcode_planner_tests.exe
```

Expected: exit code 0 and planner tests pass.

- [ ] **Step 6: Commit Task 1**

```powershell
git add -- src/internal/graph/planner/MediaPipelinePlanner.h src/internal/graph/planner/MediaPipelinePlanner.cpp tests/unit/test_planner.cpp
git commit -m "fix: preflight only selected hardware chain"
```

### Task 2: Verify Shared RTP and MPEG-TS Planning Contracts

**Files:**
- Verify: `tools/realtime_video_cli/main.cpp`
- Verify: `src/internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.cpp`
- Verify: `src/internal/graph/planner/MediaPipelinePlanner.cpp`
- Verify: `tests/unit/test_planner.cpp`

**Interfaces:**
- Consumes: the pure selection and single preflight methods from Task 1.
- Produces: verified common behavior for raw RTP, URL, and MPEG-TS inputs.

- [ ] **Step 1: Verify the common planning call path**

Run:

```powershell
rg -n "planVideoTranscodeKnownInput|preflightSelectedCandidate|selectHighestRankedCandidate" tools/realtime_video_cli/main.cpp src/internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.cpp src/internal/graph/planner/MediaPipelinePlanner.cpp
```

Expected: RTP and MPEG-TS reach `planVideoTranscodeKnownInput`, and the shared
video planner contains one selected-chain preflight call.

- [ ] **Step 2: Run full deterministic validation**

Run:

```powershell
cmd.exe /d /s /c '"call D:\VisualStudio2026\Common7\Tools\VsDevCmd.bat -arch=x64 -host_arch=x64 >nul && cmake --build --preset deterministic --clean-first"'
ctest --preset deterministic --output-on-failure
```

Filter include-tree lines from displayed compiler output.

Expected: all 26 deterministic tests pass. Confirm `ctest -N` does not list
`media_transcode_av_sync_production_runtime_integration_tests`.

- [ ] **Step 3: Run all explicit CI tiers**

For each preset `integration`, `hardware`, and `performance`, run its configured
clean-first build and matching CTest preset. Run
`tests/acceptance/test_priority5_reporting.ps1` after the performance tier.

Expected: every command exits 0. Hardware-unavailable tests may report the
repository's configured skip code but must not fail the tier.

- [ ] **Step 4: Verify formatting and scope**

Run:

```powershell
git diff --check
git status --short
```

Expected: no whitespace errors; only planned source, test, plan, and existing
user-owned untracked files are present. Verify modified text files are UTF-8
without BOM and CRLF.

- [ ] **Step 5: Push and monitor CI**

```powershell
git push origin codex/quality-priority-improvements
gh pr checks 21 --repo tangmingcheng/MediaTranscode --watch --interval 10
```

Expected: deterministic, integration, hardware, and performance checks all
reach `SUCCESS`.
