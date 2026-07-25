# Task 8 - Video synchronization controller

## Result

- Added a deterministic `MediaVideoSyncController` that consumes one complete,
  validated `MediaAvSyncPlan`; it does not accept disconnected policy fragments.
- Added strong frame-measurement and cadence-repeat-request variants. The
  controller calculates `presentation - master` with checked arithmetic.
- Added explicit Hold, Display, DisplayLate, key-frame preservation, Drop,
  RepeatPreviousFrame, Reacquire, old-generation drop, and no-action decisions.
- Recovery Drop/Repeat actions share one planner-bounded consecutive budget.
  Accepted same-generation non-recovery actions break the chain; isolated
  old/future-generation observations do not mutate current-generation state.
- Reset requires a strictly increasing generation, preventing same-generation
  sequence replay.
- Added video-specific structured synchronization errors without changing
  existing enum values; `Unknown` is appended as an explicit invalid topology.

## Contract coverage

- Exact early-hold, late-display, drop, and hard-discontinuity boundaries are
  covered, including the DisplayLate interval between late-display and drop.
- Key frames are never dropped below the hard-discontinuity threshold.
- Repeat requires an explicit synthetic cadence slot satisfying
  `last-emitted < repeat-slot <= master-now`; disabled repeat cannot hide an
  invalid request.
- Sequence zero/reuse, checked-subtraction overflow, incomplete plans, invalid
  topology, invalid generation, strict reset, recovery-limit restart, and hard
  reacquisition restart are covered.
- Old/future generations, including zero sequence and overflowing payload time,
  are isolated before current-generation payload evaluation.
- Decisions retain the original frame presentation time; the controller never
  rewrites current or later timestamps to conceal lateness.

## Verification

- `ctest --test-dir out/build/x64-debug -L deterministic --output-on-failure`:
  6/6 passed in 4.15 seconds.
- `ctest --test-dir out/build/x64-debug -L integration --output-on-failure`:
  3/3 passed in 151.04 seconds. Realtime integration passed in 93.22 seconds,
  MPEG-TS integration in 16.71 seconds, and crafted UDP fault integration in
  41.10 seconds.
- A clean-first rebuild removed 287 outputs and rebuilt all 288 targets.
- Task-owned `git diff --check`: exit 0.
- Task-owned source and test files use UTF-8 without BOM and CRLF line endings.
- Independent final review: 0 Critical / 0 Important / 0 Minor, PASS.

## Remaining integration boundary

- Task 8 intentionally does not attach presentation-clock behavior to
  `VideoFrameRateNode`; that node owns cadence conversion and cannot perform a
  deadline wait with the current worker contract without deadlock or busy wait.
- Task 9 must make the single A/V scheduler the only executor of Hold, Drop, and
  RepeatPreviousFrame decisions. It owns the deadline wakeup, last emitted video
  frame, and synthetic repeat access unit. Task 12 wires the complete plan into
  the production graph. Until then, Task 8 is a controller foundation and does
  not claim to control live output presentation.
