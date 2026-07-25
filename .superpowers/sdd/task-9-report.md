# Task 9 - Shared master clock and A/V output scheduler

## Result

- Added one shared steady master clock and sync-group runtime registry with
  explicit playback epochs, generation isolation, observable reacquisition,
  and terminal abort state.
- Added canonical and scheduled access-unit contracts with independent dispatch
  and presentation times. Reordered streams require DTS; only planner-declared
  no-reorder streams may dispatch by PTS.
- Added one A/V output scheduler with strict cross-stream dispatch ordering,
  bounded equal-time fairness, sparse-stream waiting, exact EOF convergence,
  explicit Flush/Abort handling, and BlockProducer atomic commit semantics.
- Extended the video controller to use dispatch time only for early Hold and
  presentation time only for lateness, Drop, key-frame preservation, and
  Reacquire decisions.
- Added deadline-aware worker wakeup through one sequence-based wait API. Timed
  waits, notifications, stop, and abort share the same lost-wakeup-safe path.
- Added safe repeat scheduling with cloned AVPacket objects, preserved payload
  PTS/DTS, explicit repeat request identity, and exact-once backpressure commit.
- Canonical and scheduled packet factories reject raw frames, non-packet media,
  and typed packet buffers whose underlying AVPacket is null.

## Contract coverage

- Playback epoch activation/reset, registry move construction/assignment,
  checked clock mapping, old/future generation, and aborted terminal state.
- DTS dispatch versus PTS presentation for reordered video, equal-time
  round-robin, sparse input, single/double EOF, channel close, malformed control,
  group Flush, runtime Flush, and Abort.
- Hold deadline expiry, notification race, stop/abort interruption, and idle
  process-call plateau without busy waiting.
- Output-full retry without duplicate pop, controller action, media output, or
  terminal event.
- Channel Abort preempts cached heads and deferred output transfers; native
  Abort controls retain their Control/ControlSignal type at the packet edge.
- Simultaneous controls are arbitrated symmetrically with Abort before Flush
  before EOF; Unknown remains a fail-closed global error and cannot hide Abort.
- Scheduled output parameters have no default or partial construction path;
  all required and optional fields must be passed explicitly.
- Real packet repeat ownership, unchanged payload timestamps, request identity,
  missing-history rejection, multiple repeats, and repeat backpressure.
- Factory, graph dump, diagnostic name, missing group/options, and non-blocking
  output configuration failures.

## Verification

- `ctest --test-dir out/build/x64-debug -C Debug -L deterministic --output-on-failure`:
  6/6 passed in 4.28 seconds.
- `ctest --test-dir out/build/x64-debug -C Debug -L integration --output-on-failure`:
  3/3 passed in 137.39 seconds. Realtime integration passed in 82.09 seconds,
  MPEG-TS integration in 15.51 seconds, and crafted UDP fault integration in
  39.78 seconds.
- Final all-target clean rebuild removed 298 outputs and completed 299 build
  steps with exit code 0.
- Independent final review: 0 Critical / 0 Important / 0 Minor, PASS.

## Remaining integration boundary

- Task 9 provides the clock, epoch, deadline wakeup, typed access units, and
  scheduler component. It intentionally does not insert them into production
  RTP or MPEG-TS graphs.
- Task 12 remains responsible for planner propagation, production builder
  wiring, scheduled-output consumption, removal of legacy per-output pacing,
  and the final two-minute hardware RTP/MPEG-TS playback acceptance.
