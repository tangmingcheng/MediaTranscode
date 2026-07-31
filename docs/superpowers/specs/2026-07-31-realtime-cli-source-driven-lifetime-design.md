# Realtime CLI Source-Driven Lifetime Design

## Goal

Allow the realtime video CLI to run without a CLI-imposed maximum duration
while preserving the existing optional duration gate. The media graph runtime
remains entirely input- and lifecycle-driven and receives no maximum-duration
policy.

## Interface

`--max-duration SECONDS` becomes optional:

- Present: the CLI stops a still-running runtime after the positive duration.
- Absent: the CLI creates no duration deadline and waits for the runtime to
  stop because its input or lifecycle stops.
- Zero and negative values remain invalid. There is no sentinel, magic large
  value, fallback, or implicit default duration.

The startup deadline, progress timeout, and polling interval remain required
positive CLI monitoring policies and are unchanged.

## Ownership and Data Flow

`RealtimeVideoRuntimeOptions` stores the optional CLI duration. Only
`waitForRealtimeProgress()` reads it. The request planner, graph plan, runtime,
input nodes, scheduler, mux, and output nodes do not receive or infer this
value.

When the option is absent, the monitoring loop continues while
`runtime.threadedRunning()` is true. It still:

- propagates the preserved primary runtime failure;
- rejects worker errors, missing workers, startup-output timeout, and progress
  stalls;
- samples the existing acceptance and resource telemetry.

The CLI must not translate a runtime failure caused by input loss into success.
A clean runtime lifecycle stop and an error stop retain their existing core
semantics.

## CLI Reporting

The startup summary prints the positive duration when configured and
`source_driven` when it is absent. Help text documents both forms without
claiming that streaming protocols provide a graceful EOF when they do not.

## Verification

No permanent test infrastructure is added. Verification uses:

1. the mandated VS2026 x64 Debug clean-first rebuild;
2. a direct finite FFmpeg source command;
3. the realtime CLI without `--max-duration`;
4. VLC playback plus runtime telemetry while the source is active;
5. evidence that the CLI does not stop on a duration deadline and stops only
   after the core reports input/lifecycle termination;
6. the existing final-HEAD nine-path real-media acceptance matrix.

Any attempt to use zero, a large sentinel duration, or a core duration field
fails the design review.
