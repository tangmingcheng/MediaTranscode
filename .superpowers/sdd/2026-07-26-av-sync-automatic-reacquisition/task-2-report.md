# Task 2 Report: Planner-Exact Purge Participant Assembly

## Status

GREEN for the dedicated participant-assembly target and the required runtime
regression target.

## Commit

- Branch: `codex/quality-priority-improvements`
- Commit message: `feat: assemble planned A/V purge participants`

## RED

Command:

```powershell
cmd /d /s /c '"D:\VisualStudio2026\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake --build out\build\x64-debug --clean-first --target media_transcode_av_reacquisition_runtime_assembly_tests'
```

Before compilation, `/showIncludes` was removed from
`out/build/x64-debug/CMakeFiles/rules.ninja` and its absence was verified.

Result: exit `1`, as expected. The new test translation unit required
`MediaAvGenerationParticipantAssembler`, which did not yet exist.

## GREEN

Build command:

```powershell
cmd /d /s /c '"D:\VisualStudio2026\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake --build out\build\x64-debug --clean-first --target media_transcode_av_reacquisition_runtime_assembly_tests media_transcode_runtime_tests'
```

Before every compilation, the Visual Studio 2026 x64 environment was imported,
the exact `/showIncludes` token was removed from the selected `rules.ninja`,
its absence was verified, and `--clean-first` was used.

Build result: exit `0`; both executables linked.

Test commands and results:

```powershell
out/build/x64-debug/media_transcode_av_reacquisition_runtime_assembly_tests.exe
# exit 0

out/build/x64-debug/media_transcode_runtime_tests.exe
# exit 0
```

No production runtime executable or long-duration test was started.

## Implemented

- Added an assembler that creates one participant group per immutable plan
  entry and rejects missing, duplicate, unexpected, empty, null, or repeated
  registrations.
- Added the factory's single runtime-to-participant registration boundary for
  canonical lineage, audio correction, scheduler, separate RTP outputs, and
  project MPEG-TS output.
- Added stable scheduler/RTP/MPEG-TS generation-state targets with strict
  old/next-generation and transition-sequence validation.
- Bootstrap now preserves shared ownership of the exact transition service and
  master clock used by the registered sync group.
- Compiler default assembly retains prepared node ownership, extracts and
  seals the exact registrations, constructs the coordinator, installs it into
  the scheduler-declared exact sync group, and only then transfers the node
  batch to `MediaGraphScheduler`.
- Kept sockets, byte sinks, transports, packetizers, and mux sessions outside
  `MediaProtocolOutputGenerationState`.
- Added a dedicated CMake test target using a real planner product for the full
  participant set and explicit negative cases.
- Updated the scheduler-only ComponentCore test fixture to declare its actual
  scheduler-only participant plan. This change is part of participant target
  coverage: the former fixture declared canonical/RTP participants that its
  graph did not own, which the exact assembler must reject.

## Files

- `CMakeLists.txt`
- `src/internal/graph/runtime/compilation/MediaAvGenerationParticipantAssembler.h`
- `src/internal/graph/runtime/compilation/MediaAvGenerationParticipantAssembler.cpp`
- `src/internal/graph/runtime/compilation/MediaGraphRuntimeCompiler.cpp`
- `src/internal/graph/runtime/compilation/MediaAvSyncRuntimeBootstrap.h`
- `src/internal/graph/runtime/compilation/MediaAvSyncRuntimeBootstrap.cpp`
- `src/internal/graph/runtime/factory/MediaRuntimeNodeFactory.h`
- `src/internal/graph/runtime/factory/MediaRuntimeNodeFactory.cpp`
- `src/internal/graph/sync/MediaProtocolOutputGenerationState.h`
- `src/internal/graph/sync/MediaProtocolOutputGenerationState.cpp`
- `src/internal/graph/sync/MediaAvEpochTransitionService.h`
- `src/internal/graph/sync/MediaAvEpochTransitionService.cpp`
- `src/internal/graph/nodes/sync/MediaAvOutputSchedulerNode.h`
- `src/internal/graph/nodes/sync/MediaAvOutputSchedulerNode.cpp`
- `src/internal/graph/nodes/output/MediaScheduledRtpSenderNode.h`
- `src/internal/graph/nodes/output/MediaScheduledRtpSenderNode.cpp`
- `src/internal/graph/nodes/output/MediaProjectMpegTsPlanSourceNode.h`
- `src/internal/graph/nodes/output/MediaProjectMpegTsPlanSourceNode.cpp`
- `tests/unit/av_reacquisition_runtime_assembly_tests.cpp`
- `tests/unit/av_output_scheduler_tests.cpp`

## Self-review

- Participant identities and policies originate only from the immutable plan.
- Factory code maps runtime types to existing identities and targets; it does
  not synthesize timeouts, drain windows, or fallback participants.
- Missing or extra registrations fail before scheduler batch ownership moves.
- Coordinator dependencies are captured before the activation capability is
  moved into the sequencer; the first GREEN attempt exposed and then fixed this
  ownership-order defect.
- Unrelated dirty and untracked files were neither edited nor staged.

## Concerns and follow-up

- Task 5 still owns protocol-specific timestamp, RTP sequence/timestamp,
  MPEG-TS continuity, mux-session, and buffered-output rollover semantics.
- `MediaGraphRuntime::compile()` establishes the graph/group and reports
  `Compiled` before `registerDefaultRuntimeNodes()` performs target extraction.
  The exact assembly itself fails closed before execution and scheduler
  ownership transfer, but a future transaction-level refinement may move
  production default-node preparation into the compile transaction if the
  state name must mean that participant assembly has already completed.
- The MPEG-TS participant boundary is exposed by the project plan source in
  this task. Sharing that state with the scheduled TS adapter belongs with the
  protocol rollover work, so no duplicate planned identity was invented here.

## Continuation: Runtime Readiness Lifecycle

The post-commit review found that an A/V compile still reported `Compiled`
before default runtime registration had sealed the planner-exact participant
set. The continuation appends `DefaultRegistrationPending` after all existing
runtime states and applies this lifecycle:

- only an executable with an A/V sync binding compiles to
  `DefaultRegistrationPending`;
- `run()` and `startThreaded()` reject that state;
- successful default registration transitions it to `Compiled`;
- failed default registration preserves the original exact-assembly error,
  performs runtime abort cleanup, and transitions to `Aborted`;
- non-A/V graphs and custom runtime-node registration retain their existing
  direct-to-`Compiled` lifecycle.

The existing threaded cleanup fixture intentionally bypassed default A/V
assembly. It now compiles a non-A/V single-node graph, registers its custom
runtime node unchanged, and registers a real sync group directly in the
execution context so stop, abort, and reset still verify group cleanup without
starting unrelated production media nodes.

### Continuation RED

The first continuation build linked both targets. The participant-assembly
test exited `0`, while `media_transcode_runtime_tests.exe` exited `1` with 14
expectation failures in the cleanup fixture. Starting the newly registered
production A/V nodes without media input failed during threaded startup. This
confirmed that the cleanup fixture, not the exact participant boundary, needed
to be isolated from default production assembly.

### Continuation GREEN

Build command:

```powershell
cmd /d /s /c '"D:\VisualStudio2026\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake --build out\build\x64-debug --clean-first --target media_transcode_av_reacquisition_runtime_assembly_tests media_transcode_runtime_tests'
```

Before each compilation, `/showIncludes` was removed from
`out/build/x64-debug/CMakeFiles/rules.ninja` and its absence was verified.

Test commands and results:

```powershell
out/build/x64-debug/media_transcode_av_reacquisition_runtime_assembly_tests.exe
# exit 0

out/build/x64-debug/media_transcode_runtime_tests.exe
# exit 0; event runtime tests passed
```

The missing-target test additionally verifies that the returned error remains
`InvalidArgument` with the exact participant-seal message after abort cleanup.
No long-running or production media test was executed.
