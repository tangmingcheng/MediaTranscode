# Production Assembly Task 4 Report

## Scope

Task 4 commits playback-epoch activation and startup-release visibility as one
ordered transaction. Task 3 remains readiness-only; packet gating and atomic
A/V packet release remain owned by Task 6.

## Implementation

- `MediaPlaybackEpochBinderNode` validates and retains one typed release, then
  emits one blocking `MediaStartupReleaseTransactionBuffer`. It owns no
  activation authority and publishes no activation or bound-release output.
- `MediaActivatedStartupReleaseSequencerNode` is the sole owner of the
  compiler-issued move-only activation capability. It preflights every
  `activated` target and the single `bound_release` target before activation.
- A full target returns `Waiting` while the group remains `AwaitingEpoch`, with
  no transaction output visible. Closed, aborted, mismatched, or invalid
  targets fail terminally before activation.
- Initial release activates once, publishes the same immutable activation-event
  reference to every target, and then publishes the original release reference.
  Active-epoch pass-through reuses the matching event without reactivation.
- Runtime stop, abort, and threaded-failure teardown clear channel payloads
  before closing or aborting channels. Sequencer-owned pending references are
  also cleared by its lifecycle methods.
- Binder and sequencer read required inputs through one channel helper. An open
  empty input waits; a closed or aborted empty input becomes a precise,
  permanently latched terminal failure instead of waiting forever.
- The legacy activation fanout node was removed. Node-kind value 54 remains
  reserved, and the sequencer was appended as value 57.
- Compiler validation requires exactly one scheduler, binder, and sequencer for
  a synchronized graph, with matching planner-provided group options. Generic
  factory creation cannot construct the authority-bearing sequencer.

## TDD and Debugging Evidence

The first compile-time RED failed because the new sequencer header did not yet
exist. Focused tests then drove target-capacity preflight, one-time activation,
pointer identity, pass-through, mismatch, close/abort, teardown, and fresh
runtime behavior. The follow-up RED produced exactly four failures for closed
and aborted required inputs because both nodes still returned `Waiting`. The
shared required-input reader and permanent error latching made those cases
GREEN without changing open-empty behavior.

An incremental debug run later terminated in the sequencer deleting destructor.
A captured dump and CDB stack showed an invalid free caused by stale object
layout: `/showIncludes` had been removed, so an older factory object allocated
the previous class size after the header changed. The runtime clear-channel
path was not the cause. The final VS2026 clean-first all-target rebuild removed
474 old artifacts and completed 475 actions with exit code 0. `/showIncludes` remained
absent before and after the rebuild.

The threaded lifecycle fixture is a newly compiled production graph containing
the real binder, sequencer, and `MediaAvOutputScheduler`. With the sync group in
`AwaitingEpoch`, the scheduler's activation target is deterministically full,
so the sequencer retains the transaction. Both `stop()` and `abort()` terminate
all workers, clear all related channels and retained references, reach their
terminal runtime state, and reject restart. The test uses neither a synthetic
waiting node nor sleeps or polling.

## Test Results

The three focused executables were first run directly. Each exited with code 0
and no CRT error dialog.

```powershell
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -R "media_transcode_(runtime|av_playback_epoch_binder|av_sync_production_release)_tests"
```

Result: PASS, 3/3.

```powershell
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure --repeat until-fail:20 -R "media_transcode_(runtime|av_playback_epoch_binder|av_sync_production_release)_tests"
```

Result: PASS, 60/60 process executions in 11.98 seconds.

```powershell
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -L node
```

Result: PASS, 12/12.

```powershell
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -L deterministic
```

Result: PASS, 21/21 in 37.85 seconds.

```powershell
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -L integration
```

Result: 5/6 PASS. Both RTP loopback tests and all three MPEG-TS integration
tests passed. `media_transcode_integration_tests` retained three existing Opus
planner expectation failures. The failing test and planner code are outside
this Task 4 change. Total integration time was 138.37 seconds.

## Self-review

- Activation authority has one runtime owner and cannot be constructed through
  the generic factory.
- No target can observe a successful transaction prefix due only to planned
  backpressure; all targets are checked before activation.
- The transaction and fanout preserve shared buffer references without payload
  copies.
- Planner-provided group identity is mandatory in compiler and factory paths;
  downstream nodes add no fallback or default.
- Stop and abort discard queued and retained transactions, leave zero active
  workers, reach `Stopped` or `Aborted`, and refuse restart.
- Required input closure and abort cannot leave either node permanently waiting;
  the first exact terminal error is returned unchanged on later calls.
- Source, test, and build references to the removed fanout are zero.

## Remaining Concern

Preflight and commit cannot roll back if a channel is concurrently closed after
preflight. That abnormal race is terminal and fail-closed by design, but it is
not a general multi-channel rollback transaction. The separate Opus planner
expectation mismatch remains for a later scoped task.
