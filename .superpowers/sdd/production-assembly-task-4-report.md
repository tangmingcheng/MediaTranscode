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
- Runtime compilation accepts only the scheduler, epoch binder, and startup
  clock as typed `.sync_group` consumers. Activation authority remains issued
  by the compiler to the single bound epoch binder.

## TDD Evidence

The paused workspace preserved the Task 4 behavior tests and implementation,
but did not preserve a command transcript or report for the original RED run.
The baseline-to-worktree diff proves that the new production-release test
target covers behavior absent at baseline, but no command-level RED result is
claimed. All resumed-session GREEN commands below were run against the clean
VS2026 rebuild.

## Test Results

`/showIncludes` count was zero before and after the full clean rebuild. The
rebuild completed all 472 build actions successfully.

```powershell
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -R "media_transcode_(runtime|av_playback_epoch_binder|av_sync_production_release)_tests"
```

Result: PASS, 3/3.

```powershell
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure --repeat until-fail:20 -R "media_transcode_(av_playback_epoch_binder|av_sync_production_release)_tests"
```

Result: PASS, both lifecycle executables completed 20 consecutive runs.

```powershell
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -L node
```

Result: PASS, 12/12.

```powershell
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -L deterministic
```

Result: PASS, 21/21 in 41.14 seconds.

```powershell
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -L integration
```

Result: 5/6 PASS. `media_transcode_integration_tests` failed three existing
Opus planner expectations because scheduled RTP packetization does not publish
audio batch timing. The failing test and planner files have no Task 4 diff from
baseline `d4f6068`; the five protocol integration executables passed. This is a
pre-existing planner/test-contract issue and is not hidden or changed by Task 4.

## Self-review

- Binder state has one owner and one reset path per terminal lifecycle action.
- Activation, activation-event commit, and bound-release commit are distinct;
  a retry cannot reactivate or duplicate an already committed output.
- No protocol node interprets planner policy or invents group, interval,
  generation, epoch, or clock values.
- No polling, sleeps, or `steady_clock` calls were added to runtime nodes.
- All Task 4 text is UTF-8 without BOM and uses CRLF line endings.

## Remaining Concern

The integration-label Opus expectations must be reconciled with the stricter
planner-owned audio batch-timing contract in a separate scoped task. It does
not invalidate Task 4 focused, lifecycle, node, or deterministic evidence.
