# OpenAI Build Week Demo CLI Design

## Goal

Add a dedicated evaluator-facing executable that makes MediaTranscode's value visible within seconds, without changing the production-facing local and realtime CLIs. The Build Week work lives only on `openai-build-week/demo-cli`.

The executable is named `media_transcode_build_week_cli` and presents the existing planner, graph builder, runtime, and diagnostics as a concise product demo rather than exposing low-level validation arguments.

## Approaches Considered

### 1. PowerShell wrapper around existing CLIs

Fast to build, but platform-specific, difficult to test as a product surface, and still exposes the current verbose argument contract.

### 2. Modify the existing local and realtime CLIs

Reuses code directly, but risks polluting development tools and changing interfaces used for engineering validation.

### 3. Dedicated Build Week CLI target

Selected. It is isolated, can use judge-friendly defaults, and still executes the real MediaTranscode DAG through existing builders and runtime nodes. No duplicate transcoding engine or FFmpeg subprocess is introduced.

## User Experience

### `demo`

```powershell
media_transcode_build_week_cli.exe demo --input input.mp4 [--output build-week-output.mp4] [--disable-hw] [--no-audio]
```

Behavior:

1. Prints a short MediaTranscode banner and the requested transformation.
2. Builds the real local-file DAG using judge-friendly queue and codec defaults.
3. Prints node/edge counts and the selected decoder/filter/encoder chain when available.
4. Runs the graph through `MediaGraphRuntime`.
5. Reports completion, elapsed time, graph iterations, pushed/popped buffers, and output path.

Defaults:

- output: `build-week-output.mp4`
- video codec: `h264`
- audio codec: `aac`
- queues: metadata 1, packet 256, frame 128, mux 256
- hardware planning: enabled
- graph diagnostics: enabled

### `inspect`

```powershell
media_transcode_build_week_cli.exe inspect --input input.mp4 [same transform options]
```

Builds the same real graph but does not execute it. It prints an evaluator-readable DAG in topological construction order and highlights the planned video chain. This enables a deterministic architecture demonstration even when the evaluator does not want to wait for a full transcode.

### `live`

```powershell
media_transcode_build_week_cli.exe live --input rtsp://... --output udp://127.0.0.1:7354 [--duration 15]
```

Uses the existing realtime planner, realtime graph builder, threaded runtime, and runtime reporter. The command intentionally supports the most demo-friendly path only: session-described RTSP input to muxed MPEG-TS UDP output. It prints the selected chain and periodic runtime metrics, then stops cleanly after the configured duration.

Defaults:

- RTSP transport: TCP
- output layout: MPEG-TS UDP
- duration: 15 seconds
- open/read timeout: 5000 ms
- analyze duration: 500000 us
- probe size: 524288 bytes
- low latency: enabled
- queues: same as `demo`

## Architecture

```text
BuildWeekCliArguments
        |
        v
BuildWeekDemoConfig
        |
        +--> LocalFileTranscodeGraphBuilder --> MediaGraph --> MediaGraphRuntime
        |
        +--> MediaRealtimeRtpTranscodePlanner
              --> MediaRealtimeRtpTranscodeGraphBuilder
              --> MediaGraphRuntime (threaded)
              --> MediaGraphRuntimeReporter
```

The CLI owns only:

- command parsing and defaults
- evaluator-oriented formatting
- command orchestration
- elapsed-time measurement

The CLI does not:

- invoke an FFmpeg executable
- manually choose decoder/filter/encoder implementations
- construct graph nodes directly
- duplicate planner policy
- change core runtime behavior

## Components

### `tools/build_week_cli/BuildWeekCli.h`

Defines parsed command/config types and pure helpers used by tests.

### `tools/build_week_cli/BuildWeekCli.cpp`

Implements parsing, default parameter construction, graph summaries, local demo execution, inspect execution, and live execution.

### `tools/build_week_cli/main.cpp`

Exception boundary only. Delegates to `runBuildWeekCli`.

### `tests/unit/test_build_week_cli.cpp`

Tests command parsing, defaults, invalid arguments, and judge-facing text helpers without requiring media hardware or a live endpoint.

## Error Handling

- Invalid/missing arguments return exit code 2 and print usage.
- Planner, builder, compile, registration, runtime, and shutdown failures return exit code 1 with the existing `ErrorInfo::describe()` detail.
- `inspect` never starts runtime execution.
- `live` always attempts `runtime.stop()` after a successful threaded start.
- URLs are printed through existing user-info redaction.

## Testing

1. Unit tests for all subcommands and defaults.
2. Existing realtime and event-runtime tests remain unchanged and continue to pass.
3. Build the new target with C++20 on the existing tool configuration.
4. Run `git diff --check` equivalent through patch review.
5. Inspect the final branch diff to ensure no core or existing CLI behavior changed beyond CMake target registration and documentation.

## Branch Isolation

- Branch: `openai-build-week/demo-cli`
- Base: `76c32e06b6ee29fa513b9bb3057acaab07964d5a`
- No merge into `master` is performed as part of this work.
- A draft pull request may be created only as a review surface; it must not be merged automatically.
