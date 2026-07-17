# OpenAI Build Week Demo CLI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build an isolated evaluator-facing CLI that demonstrates MediaTranscode's real local and realtime DAG paths with one short command.

**Architecture:** Add a dedicated `media_transcode_build_week_cli` target. Parsing and presentation live in focused Build Week files, while all media planning, graph construction, runtime execution, and diagnostics continue to use existing MediaTranscode components.

**Tech Stack:** C++20, CMake 3.21+, FFmpeg libraries, existing MediaTranscode DAG planner/builders/runtime, GitHub Actions Ubuntu build verification.

## Global Constraints

- Work only on branch `openai-build-week/demo-cli` based on `76c32e06b6ee29fa513b9bb3057acaab07964d5a`.
- Do not modify the behavior or argument contracts of existing CLIs.
- Do not invoke an external FFmpeg executable.
- Do not manually select concrete hardware decoder/filter/encoder implementations.
- Keep hardware planning enabled by default and provide `--disable-hw` as an explicit fallback.
- Keep the evaluator path concise: queue sizes and realtime probe defaults must not be required arguments.
- Do not merge this branch into `master`.

---

## File Structure

- Create `tools/build_week_cli/BuildWeekCli.h`: command/config types plus parsing and run entry points.
- Create `tools/build_week_cli/BuildWeekCli.cpp`: parsing, defaults, graph presentation, local demo/inspect, realtime live orchestration.
- Create `tools/build_week_cli/main.cpp`: exception boundary only.
- Create `tests/unit/test_build_week_cli.cpp`: parser/default/error tests.
- Modify `CMakeLists.txt`: register CLI and unit-test target.
- Create `BUILD_WEEK.md`: judge-ready build and demo commands.
- Create `.github/workflows/build-week-cli.yml`: branch-scoped Ubuntu compile/test verification.

---

### Task 1: Define and test the command contract

**Files:**
- Create: `tools/build_week_cli/BuildWeekCli.h`
- Create: `tests/unit/test_build_week_cli.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `enum class BuildWeekCommand { Demo, Inspect, Live, Help }`
- Produces: `struct BuildWeekCliOptions`
- Produces: `BuildWeekCliOptions parseBuildWeekCliOptions(int argc, char** argv)`
- Produces: `std::string buildWeekUsage()`
- Produces: `int runBuildWeekCli(int argc, char** argv)`

- [ ] **Step 1: Write failing parser tests**

Test exact defaults:

```cpp
const char* argv[] = { "build-week", "demo", "--input", "input.mp4" };
auto options = parseBuildWeekCliOptions(4, const_cast<char**>(argv));
assert(options.command == BuildWeekCommand::Demo);
assert(options.input == "input.mp4");
assert(options.output == "build-week-output.mp4");
assert(options.videoCodec == "h264");
assert(options.audioCodec == "aac");
assert(options.videoBitrateKbps == 4000);
assert(options.includeAudio);
assert(!options.disableHardware);
```

Also test `inspect`, `live`, `--disable-hw`, `--no-audio`, width/height pairing, positive duration, unsupported arguments, and help.

- [ ] **Step 2: Register a failing test target**

Add:

```cmake
add_executable(media_transcode_build_week_cli_tests
    tests/unit/test_build_week_cli.cpp
    tools/build_week_cli/BuildWeekCli.cpp
)
media_transcode_configure_test(media_transcode_build_week_cli_tests)
add_test(NAME media_transcode_build_week_cli_tests COMMAND media_transcode_build_week_cli_tests)
```

Expected initial result: compile failure because `BuildWeekCli.h/.cpp` are not implemented.

- [ ] **Step 3: Add the header and minimal parser implementation**

Use this exact options shape:

```cpp
enum class BuildWeekCommand { Demo, Inspect, Live, Help };

struct BuildWeekCliOptions {
    BuildWeekCommand command = BuildWeekCommand::Help;
    std::string input;
    std::string output;
    std::string videoCodec = "h264";
    std::string audioCodec = "aac";
    std::optional<int> width;
    std::optional<int> height;
    int videoBitrateKbps = 4000;
    int durationSeconds = 15;
    bool includeAudio = true;
    bool disableHardware = false;
    bool diagnosticLogEnabled = true;
};
```

Parser rules:

- first positional command is `demo`, `inspect`, `live`, `help`, `--help`, or `-h`
- `--input` is required except for help
- local default output is `build-week-output.mp4`
- live default output is `udp://127.0.0.1:7354?pkt_size=1316`
- width and height must be supplied together and be positive
- bitrate and duration must be positive
- reject every unknown option

- [ ] **Step 4: Run the parser tests**

Run:

```powershell
cmake --build out/build/x64-debug --target media_transcode_build_week_cli_tests
out\build\x64-debug\media_transcode_build_week_cli_tests.exe
```

Expected: `build week CLI tests passed`.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt tools/build_week_cli/BuildWeekCli.h tools/build_week_cli/BuildWeekCli.cpp tests/unit/test_build_week_cli.cpp
git commit -m "test: define Build Week CLI contract"
```

---

### Task 2: Implement evaluator-facing local demo and inspect

**Files:**
- Modify: `tools/build_week_cli/BuildWeekCli.cpp`
- Create: `tools/build_week_cli/main.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `BuildWeekCliOptions`
- Produces: `int runBuildWeekCli(int argc, char** argv)`

- [ ] **Step 1: Add formatting tests**

Build a small synthetic `MediaGraph` and verify the graph summary contains node names, readable kinds, node count, and edge count. Verify a graph with `VideoEncode` options prints `pipeline.chain`, decoder, filter, encoder, and score without requiring runtime execution.

- [ ] **Step 2: Implement local parameter construction**

Create `MediaTranscodeParameterSet makeParameters(const BuildWeekCliOptions&)` with:

```cpp
parameters.execution.includeAudio = options.includeAudio;
parameters.execution.disableHardware = options.disableHardware;
parameters.execution.diagnosticLogEnabled = options.diagnosticLogEnabled;
parameters.queues.metadata = 1;
parameters.queues.packet = 256;
parameters.queues.frame = 128;
parameters.queues.mux = 256;
parameters.video.codecName = options.videoCodec;
parameters.video.bitrateKbps = options.videoBitrateKbps;
parameters.video.width = options.width;
parameters.video.height = options.height;
parameters.audio.codecName = options.audioCodec;
parameters.audio.bitrateKbps = 160;
```

- [ ] **Step 3: Implement `inspect`**

Build `LocalFileTranscodeOptions`, call `LocalFileTranscodeGraphBuilder::build`, print banner/request/graph/plan, and return without constructing `MediaGraphRuntime`.

- [ ] **Step 4: Implement `demo`**

Use the same graph build, then:

```cpp
MediaGraphRuntime runtime;
runtime.setDiagnosticsEnabled(options.diagnosticLogEnabled);
auto compileStatus = runtime.compile(std::move(graph));
auto registerStatus = runtime.registerDefaultRuntimeNodes();
auto runResult = runtime.run();
```

Print elapsed milliseconds plus `iterations`, `totalPushed`, `totalPopped`, `completed`, and output path. Return 0 only when completed.

- [ ] **Step 5: Add the executable**

```cmake
set(TARGET_BUILD_WEEK_CLI "media_transcode_build_week_cli")
add_executable(${TARGET_BUILD_WEEK_CLI}
    tools/build_week_cli/main.cpp
    tools/build_week_cli/BuildWeekCli.cpp
)
media_transcode_configure_app(${TARGET_BUILD_WEEK_CLI})
```

`main.cpp` catches `std::invalid_argument`, `std::exception`, and unknown exceptions, returning exit code 2 for usage errors and 1 for runtime failures.

- [ ] **Step 6: Build and run tests**

```powershell
cmake --build out/build/x64-debug --target media_transcode_build_week_cli media_transcode_build_week_cli_tests
out\build\x64-debug\media_transcode_build_week_cli_tests.exe
out\build\x64-debug\media_transcode_build_week_cli.exe --help
```

Expected: tests pass and help lists all three commands.

- [ ] **Step 7: Commit**

```bash
git add CMakeLists.txt tools/build_week_cli
 git commit -m "feat: add Build Week local demo CLI"
```

---

### Task 3: Implement the focused realtime live demo

**Files:**
- Modify: `tools/build_week_cli/BuildWeekCli.cpp`
- Modify: `tests/unit/test_build_week_cli.cpp`

**Interfaces:**
- Consumes: `BuildWeekCliOptions` with `command == BuildWeekCommand::Live`

- [ ] **Step 1: Test live request defaults**

Verify a live parse produces RTSP input semantics, 15 seconds, MPEG-TS UDP default output, and the same queue/hardware defaults as local mode.

- [ ] **Step 2: Build the realtime request**

Populate:

```cpp
request.input.type = RealtimeInputType::Url;
request.input.streamLayout = RealtimeInputStreamLayout::SessionDescribed;
request.input.url = options.input;
request.input.rtspTransport = "tcp";
request.input.openTimeoutMs = 5000;
request.input.readTimeoutMs = 5000;
request.input.analyzeDurationUs = 500000;
request.input.probeSizeBytes = 524288;
request.input.lowLatency = true;
request.output.streamLayout = RealtimeOutputStreamLayout::MuxedTransportStream;
request.output.url = options.output;
request.parameters = makeParameters(options);
request.mediaId = "openai-build-week";
```

- [ ] **Step 3: Execute the existing realtime stack**

Call planner, graph builder, graph summary, `MediaGraphRuntime::setThreadingPolicy`, compile, register, and `startThreaded`. Every 500 ms capture `MediaGraphRuntimeReporter::capture(runtime)` and print `report.summary()`. Stop after `durationSeconds`, fail immediately when `workerErrors > 0`, and always call `stop()` after a successful start.

- [ ] **Step 4: Run unit and existing runtime tests**

```powershell
out\build\x64-debug\media_transcode_build_week_cli_tests.exe
out\build\x64-debug\media_transcode_event_runtime_tests.exe
out\build\x64-debug\media_transcode_realtime_graph_tests.exe
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure
```

Expected: all tests pass.

- [ ] **Step 5: Commit**

```bash
git add tools/build_week_cli/BuildWeekCli.cpp tests/unit/test_build_week_cli.cpp
git commit -m "feat: add Build Week realtime demo"
```

---

### Task 4: Add judge documentation and branch-scoped CI

**Files:**
- Create: `BUILD_WEEK.md`
- Create: `.github/workflows/build-week-cli.yml`
- Modify: `README.md`

- [ ] **Step 1: Document the shortest path**

Provide exact Windows commands for configure, build, inspect, demo, and live. State that `demo` writes `build-week-output.mp4`, and VLC receives live MPEG-TS with `udp://@:7354`.

- [ ] **Step 2: Add Ubuntu CI**

Workflow trigger:

```yaml
on:
  push:
    branches: [openai-build-week/demo-cli]
  pull_request:
    branches: [master]
```

Install CMake, Ninja, compiler, pkg-config, and FFmpeg development packages. Configure with tests ON, build `media_transcode_build_week_cli`, `media_transcode_build_week_cli_tests`, `media_transcode_event_runtime_tests`, and `media_transcode_realtime_graph_tests`, then run CTest.

- [ ] **Step 3: Verify final scope**

Compare branch against master. Expected changed surfaces are only Build Week files, CMake registration, branch documentation, and branch CI. No existing graph/runtime implementation file may change.

- [ ] **Step 4: Commit**

```bash
git add BUILD_WEEK.md README.md .github/workflows/build-week-cli.yml
git commit -m "docs: add Build Week judge walkthrough"
```

---

### Task 5: Final verification and review

- [ ] **Step 1: Run all available verification**

```powershell
cmake --build out/build/x64-debug --target media_transcode_build_week_cli media_transcode_build_week_cli_tests media_transcode_event_runtime_tests media_transcode_realtime_graph_tests
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure
git diff --check master...HEAD
```

- [ ] **Step 2: Review the complete diff**

Check argument validation, output redaction, runtime shutdown on all live paths, no FFmpeg subprocess use, no manual hardware implementation selection, and no modifications to existing CLI behavior.

- [ ] **Step 3: Push the branch and create a draft PR**

Create a draft PR to `master` only as a review surface. Do not merge it.
