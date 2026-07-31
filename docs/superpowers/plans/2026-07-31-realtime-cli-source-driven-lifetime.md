# Realtime CLI Source-Driven Lifetime Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the realtime CLI maximum duration optional so omitted duration leaves the core runtime entirely input- and lifecycle-driven.

**Architecture:** Represent the CLI-only duration as an optional positive integer in `RealtimeVideoRuntimeOptions`. The CLI monitoring loop applies a deadline only when that value exists; no duration fact enters the request, planner, graph, or runtime.

**Tech Stack:** C++20, project realtime CLI, production DAG runtime telemetry, FFmpeg, VLC, Visual Studio 2026 CMake/Ninja.

## Global Constraints

- Core planner, graph, runtime, input, scheduler, mux, and output types receive no maximum-duration field.
- `--max-duration` present means a positive CLI-only deadline; absent means no deadline.
- Zero, negative values, magic large sentinels, implicit defaults, and runtime fallback are rejected.
- Startup-output deadline, progress timeout, polling, worker errors, and preserved primary failures remain unchanged.
- Do not add or restore CI, CTest, unit, integration, acceptance, hardware, or performance test infrastructure.
- Validate with the mandated clean-first x64 Debug rebuild and direct real CLI/FFmpeg/VLC commands.
- Any temporary local validation artifact stays under `out/`, is not versioned, and is removed when no longer required.
- All edited text uses UTF-8 and CRLF.

---

### Task 1: Make CLI duration optional without changing core lifetime

**Files:**
- Modify: `tools/realtime_video_cli/main.cpp`
- Modify: `README.md`
- Modify: `docs/completed/realtime-cross-layout-mpegts-rtp.md`
- Modify: `plan.md`
- Modify: `QUALITY_SCORE.md`

**Interfaces:**
- `RealtimeVideoRuntimeOptions::maxDurationSeconds` becomes `std::optional<int>`.
- `parseRuntimeOptions(int argc, char** argv)` consumes optional `--max-duration`.
- `waitForRealtimeProgress(MediaGraphRuntime&, const RealtimeVideoRuntimeOptions&)` evaluates the maximum-output-duration condition only when the optional value exists.
- No production core interface changes.

- [x] **Step 1: Capture the current failure evidence**

Run the realtime CLI without `--max-duration` and retain the local error showing
`missing required integer argument: --max-duration`. Confirm no VLC is opened
and no source or CLI process remains.

- [x] **Step 2: Implement the optional CLI policy**

In `tools/realtime_video_cli/main.cpp`:

```cpp
#include <optional>

struct RealtimeVideoRuntimeOptions {
    std::optional<int> maxDurationSeconds;
    int progressTimeoutMs = 5000;
    int firstOutputTimeoutMs = 30000;
    int pollIntervalMs = 250;
};
```

Parse with `optionalIntArg()`. Reject a configured value when it is nonpositive,
but do not synthesize a value when absent. Construct and evaluate
`maximumOutputDuration` only for a configured value:

```cpp
if (options.maxDurationSeconds &&
    progressTracker.maximumOutputDurationExpired(
        elapsedMs, std::chrono::seconds(*options.maxDurationSeconds))) {
    return ::media::Status::success();
}
```

Print `max_duration=source_driven` when absent and the integer when present.
Update the usage string so `--max-duration SECONDS` is visibly optional.

- [x] **Step 3: Verify static ownership boundaries**

Search the whole tracked tree for `maxDurationSeconds`, `max-duration`, and new
duration fields. Require that the optional duration remains confined to the
realtime CLI and user documentation. Reject any planner, plan codec, graph,
runtime node, scheduler, mux, or sink duration addition.

- [x] **Step 4: Run direct CLI argument validation**

Run three direct commands without creating a test source:

1. `--max-duration 0` must exit nonzero with the positive-duration validation.
2. `--max-duration -1` must exit nonzero with the same validation.
3. Omitting `--max-duration` must pass argument parsing and proceed to input
   startup, printing `max_duration=source_driven`; an unavailable input may then
   fail normally.

Save only concise results in the completion report.

- [x] **Step 5: Run the mandated production build**

Execute the repository VS2026 skill command:

```powershell
& 'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe' -NoProfile -ExecutionPolicy Bypass -File 'D:\Code\MyCode\MediaTranscode\.agents\skills\building-with-vs2026\scripts\rebuild_debug.ps1'
```

Require configure/build exit code 0, all targets rebuilt, and both CLI
executables present.

- [x] **Step 6: Run source-driven real-media verification**

For each final-HEAD matrix route, use a direct finite FFmpeg source command with
at least 135 seconds of media and omit `--max-duration` from the CLI. Open one
VLC only, observe more than 2 minutes of playback, and let the source end
naturally. Confirm from timestamps that no CLI duration deadline stopped the
runtime. Record the core lifecycle outcome after input loss exactly; do not
translate a runtime error into success.

Use direct FFmpeg/ffprobe receiver commands as appropriate. For every generated
SDP output, record the exact command:

```powershell
& 'D:\mabs\local64\bin-video\ffmpeg.exe' -hide_banner -loglevel warning -protocol_whitelist 'file,udp,rtp' -i 'D:\Code\MyCode\MediaTranscode\out\acceptance\realtime-cross-layout\formal-rtp-to-rtp\final-head-output.sdp' -t 20 -map 0:v:0 -map 0:a:0 -f null NUL
```

For MPEG-TS/UDP, record the corresponding direct
`udp://127.0.0.1:54220?fifo_size=65536&overrun_nonfatal=1` receiver command.

- [x] **Step 7: Update delivery evidence**

Update README usage, the completion report, main `plan.md`, and
`QUALITY_SCORE.md` with:

- optional CLI duration semantics;
- exact source-driven commands;
- nine final-HEAD results;
- FFmpeg/ffprobe receiver commands;
- human VLC observations;
- runtime error/drop/drift/CPU/memory evidence;
- remaining streaming-protocol EOF and manual-regression risks.

- [ ] **Step 8: Verify, commit, push, and re-review**

Run `git diff --check`, verify every changed text file is UTF-8/CRLF, confirm no
temporary test/script/dump is tracked or left under `out/`, and rerun the
strongest affected real-media gate after any code correction. Commit and push
to `codex/realtime-cross-layout`, verify local and remote HEAD match, then ask
the existing PR reviewer to re-review until it reports explicit PASS with no
blocking findings.
