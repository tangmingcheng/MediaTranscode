# A/V Sync Automatic Reacquisition Design

Date: 2026-07-26

## Objective

Complete the production A/V synchronization loop after an input clock
discontinuity. The realtime CLI process and configured UDP output endpoint stay
alive while the affected A/V group pauses, discards old-generation state,
acquires a new common timeline, and resumes audio and video atomically.

This is Task 13 of the production A/V sync assembly. It replaces the Task 12
terminal behavior for a valid reacquisition request; it does not weaken
validation or hide malformed input.

## Current Failure

`MpegTsDemuxNode` and RTP clock projection can emit typed evidence that a newer
clock generation must be acquired. `MediaAvSyncGroupRuntime`,
`MediaAvEpochTransitionService`, and
`MediaAvGenerationTransitionCoordinator` already model the request, purge
transition, acknowledgements, and next-epoch activation.

The production graph does not drive those facilities. In particular,
`MediaInitialLockedPacketGateNode` treats acquisition after the initial lock,
generation changes, and discontinuity evidence as terminal. Consequently a
continuous sender can remain healthy while the CLI exits with
`Initial locked packet gate rejects clock discontinuity`.

Changing that gate to ignore discontinuity would mix generations and is not an
acceptable fix.

## Required Invariants

1. A packet from the old generation is never emitted after the transition
   output permit closes.
2. A packet from the next generation is never emitted before both streams share
   an activated playback epoch.
3. Audio and video resume through one atomic release decision.
4. The UDP sink resource and CLI process remain alive during a recoverable
   transition.
5. Only the planner selects participants, required child identities, timeout,
   and drain policy. Runtime components reject incomplete plans and have no
   defaults or fallback.
6. Only one transition can be active per A/V group. Its sequence and generation
   are strictly monotonic.
7. Purge, acknowledgement, timeout, generation mismatch, or activation failure
   is terminal and visible; it is never converted to success.

## Architecture

### Production transition owner

Add one group-scoped `MediaAvReacquisitionCoordinator` to the runtime sync
layer. It owns transition execution, not policy:

- consume a typed `MediaAvReacquisitionRequest`;
- validate it against the active playback epoch and planned transition;
- close the group output permit;
- call `MediaAvSyncGroupRuntime::beginEpochReacquisition`;
- invoke every sealed participant group's `purgeAll`;
- forward every acknowledgement through
  `acknowledgeEpochReacquisition`;
- supervise the planned acknowledgement timeout;
- authorize next-generation acquisition after purge completion; and
- publish the completed transition sequence required for next-epoch activation.

The coordinator is the sole writer of transition lifecycle state. Nodes may
report evidence and observe the permit, but may not independently start,
complete, or bypass a transition.

### Production assembly

`MediaGraphRuntimeCompiler` assembles the coordinator from the planner's
`MediaAvGenerationTransitionPlan`. `MediaRuntimeNodeFactory` exposes the purge
target and stable identity of each planned runtime node. Compilation registers
each target into its matching `MediaAvGenerationParticipantGroup`, seals every
complete group, and rejects missing, duplicate, unexpected, or null targets
before the graph starts.

The existing planned participants remain authoritative:

- canonical lineage: video decode/filter/frame-rate/encode and audio
  decode/resample/startup-trim/encode/canonicalization/startup generation state
  where selected by the plan;
- audio correction;
- output scheduler;
- RTP video and audio output state for separate RTP output; or
- project MPEG-TS output generation state for MPEG-TS output.

If implementation evidence shows a stateful generation boundary is absent from
the plan, the planner and validator must be extended first. The compiler must
not invent an implicit participant.

### Trigger semantics

Protocol clock nodes continue to distinguish valid reacquisition evidence from
invalid data. A valid `ReacquireRequired`, future generation, PCR/program
discontinuity, RTP clock degradation, or RTP CNAME discontinuity becomes a
typed group request.

The locked-packet gate becomes generation-aware:

- normal locked evidence for the active generation passes;
- old-generation evidence is rejected without entering the next epoch;
- acquiring or discontinuity evidence is withheld only when it belongs to the
  coordinator's active next generation; and
- malformed evidence, regression, an unplanned generation, or reacquisition
  without an active transition remains terminal.

The first request establishes the diagnostic reason. A strictly newer observed
generation may supersede a pending request before transition start. Once purge
starts, another incompatible generation is terminal rather than creating
parallel transitions.

### Purge and output quiescence

Closing the output permit is the linearization point. Already accepted
old-generation work may be drained only within the planner-provided terminal
drain window and may not cross the protocol output boundary afterward.

Each purge target removes buffered packets, frames, timestamps, correction
history, scheduler watermarks, startup observations, codec pipeline lineage,
and protocol output generation state associated with the old generation.
MPEG-TS mux timing and continuity state rolls to the new generation without
closing or rebinding the configured UDP endpoint. RTP output retains its
configured sockets while resetting generation-owned timestamp state.

The coordinator records each participant acknowledgement. Output remains closed
until the complete planned set succeeds.

### Next acquisition and activation

After purge acknowledgement, the clock gates admit only the planned next
generation into acquisition buffers. Both streams independently obtain locked
clock evidence. `MediaAvStartupCoordinatorNode` then derives a new common start
point using the same validated startup policy used for the initial epoch.

Extend `MediaAvStartupReleaseKind` with a new stable enum value for a
next-generation atomic release. Existing enum values do not change.
`MediaActivatedStartupReleaseSequencerNode` validates the transition sequence
and invokes `MediaPlaybackEpochActivationCapability::activateNext`. It publishes
the activated epoch before releasing either stream.

The output permit reopens only after activation succeeds and the active
generation, transition sequence, startup release, scheduler state, and protocol
output state agree exactly. Audio and video are then released as one
transaction.

## Concurrency and Failure Handling

All lifecycle mutations are serialized by the group coordinator. Node workers
submit requests and acknowledgements through typed interfaces; they do not wait
while holding codec, queue, or scheduler locks.

The coordinator polls timeout from the runtime's monotonic master clock. Any
missing acknowledgement, purge error, activation error, invalid sequence, or
generation conflict poisons the A/V group, aborts its output permit, and returns
the original diagnostic to the runtime. Normal stop remains idempotent so a
prior worker failure does not produce a misleading secondary
`runtime is not running` failure.

## Diagnostics

One structured transition record must include:

- group key, protocol, reason, old generation, next generation, and transition
  sequence;
- permit-close, purge-complete, clock-lock, activation, and resume timestamps;
- planned and received participant acknowledgements;
- paused duration and buffered/dropped counts by stream; and
- terminal error, when present.

Existing drift metrics continue across process lifetime but are segmented by
playback generation. No sample may compare timestamps from different
generations.

## Verification

Implementation follows test-driven development with deterministic, bounded
tests:

1. coordinator unit tests for begin, complete acknowledgement, duplicate or
   missing acknowledgement, timeout, supersession, and terminal failure;
2. gate tests proving active-generation pass, old-generation rejection,
   next-generation withholding, and malformed-evidence failure;
3. startup sequencer tests proving `activateNext` occurs once and before the
   atomic release;
4. compiler tests proving the exact planned purge target set is registered and
   incomplete assembly is rejected;
5. MPEG-TS integration test injecting a PCR/program discontinuity while packets
   continue, then verifying a new playback epoch, continued output, and no
   cross-generation packet;
6. RTP integration test injecting clock degradation/CNAME discontinuity with
   the same invariants; and
7. a short controlled realtime CLI test showing continuous sender and CLI
   survival across one discontinuity.

Every compilation uses clean-first with `/showIncludes` disabled. No unattended
long-duration test is part of this implementation phase. Human-eye testing
starts only after deterministic verification and explicit user approval.

## Scope Boundary

Task 13 covers automatic group-level reacquisition for both MPEG-TS and RTP.
The first runtime acceptance target is the reported MPEG-TS continuous-source
auto-stop. General queue tuning, unrelated hardware-probe logging, code cleanup,
and long-run drift diagnosis remain follow-up work unless they block these
invariants.
