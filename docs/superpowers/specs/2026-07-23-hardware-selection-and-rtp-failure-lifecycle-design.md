# Hardware Selection and RTP Failure Lifecycle Design

## Objective

Prevent unselected hardware backends from being instantiated during planning, and preserve the first RTP runtime failure instead of replacing it with teardown errors.

## Hardware selection

Candidate discovery is side-effect free. `MediaVideoCapabilityScanner` only describes statically available hardware or software chains. `MediaPipelineScorer` produces a deterministic order using chain category, declared stage priority, explicit preferred hardware, and a stable tie-breaker.

`MediaPipelinePlanner` is the only decision owner. In hardware mode it validates ranked hardware candidates one at a time and stops after the first fully valid chain. A failed hardware candidate may advance to the next ranked hardware candidate, but never to software. Software candidates exist only when hardware is explicitly disabled.

`MediaHardwareCapabilityProbe` validates exactly one planner-selected candidate. It creates each required hardware device at most once per validation and does not choose or fall back to another candidate.

## RTP failure lifecycle

Each worker preserves its first structured `ErrorInfo`. The threaded executor exposes the earliest primary worker failure and does not allow peer channel-abort errors produced during graph cancellation to replace it.

`synchronizeThreadedState()` aborts the failed graph but returns the preserved primary error. The realtime CLI calls `stop()` only while the runtime is still running. If synchronization already transitioned the runtime to `Aborted`, the CLI reports the primary failure and performs non-masking cleanup.

`RawRtpInput` receives an explicit diagnostic node-kind name so failure logs no longer report `kind=Unknown`.

## Validation

- Planner tests prove ranked lazy probing, deterministic priority, no unselected device creation, and no implicit software fallback.
- Runtime tests prove first-error preservation and abort-race behavior.
- CLI lifecycle tests prove an aborted runtime does not call `stop()` and does not mask the wait failure.
- All compilation uses clean-first and disables `/showIncludes`.

Objective drift telemetry is intentionally excluded from this change and remains the next A/V synchronization task.
