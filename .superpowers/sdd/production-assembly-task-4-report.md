# Production Assembly Task 4 Report

## Scope

Task 4 completes activation-before-release and registered-master-clock startup.
The packet gate and atomic A/V packet release remain owned by Task 6.

## Implementation

- `MediaPlaybackEpochBinderNode` retains one release, activates the initial
  playback epoch once, commits the immutable typed activation event, and then
  forwards the same release. Activation-event and release commits are tracked
  independently across `WouldBlock`; stop and abort clear retained state.
- `MediaPlaybackEpochActivatedFanoutNode` forwards the same immutable typed
  activation event through the runtime's retained multi-output transfer.
- `MediaRtpSourceClockStateAdapterNode` maps RTP group readiness and generation
  into the protocol-neutral source-clock state published by MPEG-TS input.
- `MediaAvStartupClockNode` accepts only planner-provided group and interval
  options, consumes generic source-clock readiness, reads the registered group
  master clock, and returns `DeadlineWait` between ticks.
- `MediaAvOutputSchedulerNode` may start while the group is registered and
  awaiting its epoch. It leaves queued media untouched until the group becomes
  Active, then creates its controller exactly once from the activated epoch.
  The controller factory is an explicit dependency, so the once-only behavior
  is verified without exposing private scheduler state.
- Runtime compilation accepts only the scheduler, epoch binder, and startup
  clock as typed `.sync_group` consumers. Activation authority remains issued
  by the compiler to the single bound epoch binder.

## TDD Evidence

The resumed reviewer-fix cycle captured two compile-time RED results before the
implementation was changed:

- `av_output_scheduler_tests.cpp:1368` failed with C2661 because the scheduler
  did not yet accept an observable controller-factory dependency.
- `av_playback_epoch_binder_tests.cpp:38-39` failed both static assertions
  because the direct `publishInitial` and `publishNext` activation APIs were
  still public.

An initial GREEN runtime run then exposed a process-exit hang. Marker-based
localization proved all runtime tests had completed and the hang was in static
destruction: `startFixture` retained runtimes in a function-static vector.
Replacing that hidden process-lifetime retention with fixture-owned RAII fixed
the hang; the direct runtime executable and the repeated CTest runs now exit
normally. All final GREEN commands below were run against the clean VS2026
rebuild.

## Test Results

`/showIncludes` count was zero before and after the full clean rebuild. The
rebuild completed all 473 build actions successfully.

```powershell
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -R "media_transcode_(runtime|av_playback_epoch_binder|av_sync_production_release)_tests"
```

Result: PASS, 3/3.

```powershell
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure --repeat until-fail:20 -R "media_transcode_(runtime|av_playback_epoch_binder|av_sync_production_release)_tests"
```

Result: PASS, all three runtime/lifecycle executables completed 20 consecutive
runs (60/60 process executions).

```powershell
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -L node
```

Result: PASS, 12/12.

```powershell
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -L deterministic
```

Result: PASS, 21/21 in 39.21 seconds.

```powershell
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -L integration
```

Result: 5/6 PASS. `media_transcode_integration_tests` failed three existing
Opus planner expectations because scheduled RTP packetization does not publish
audio batch timing (`test_realtime_rtp_graph.cpp:1816`, `:1957`, and `:1970`).
The failing test and planner files have no Task 4 diff from baseline `d4f6068`;
the five protocol integration executables passed. This is a pre-existing
planner/test-contract issue and is not hidden or changed by Task 4.

## Self-review

- Binder state has one owner and one reset path per terminal lifecycle action.
- Epoch activation is reachable only through the typed release input; the
  binder exposes no direct initial or next-generation activation API.
- Activation, activation-event commit, and bound-release commit are distinct;
  a retry cannot reactivate or duplicate an already committed output.
- Stop/restart is covered both after activation-before-event-commit and after
  event-commit-before-release-commit; neither retained output leaks or repeats.
- No protocol node interprets planner policy or invents group, interval,
  generation, epoch, or clock values.
- No polling, sleeps, or `steady_clock` calls were added to runtime nodes.
- All Task 4 text is UTF-8 without BOM and uses CRLF line endings.

## Remaining Concern

The integration-label Opus expectations must be reconciled with the stricter
planner-owned audio batch-timing contract in a separate scoped task. It does
not invalidate Task 4 focused, lifecycle, node, or deterministic evidence.
