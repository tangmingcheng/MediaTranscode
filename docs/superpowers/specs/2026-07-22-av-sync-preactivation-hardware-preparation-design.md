# A/V Sync Pre-Activation Hardware Preparation Design

## Goal

The initial playback epoch must not become active until the selected hardware
video chain is ready to produce its first scheduled frame. Preparation uses
the real decoded hardware frame and its `hw_frames_ctx`; it does not estimate a
fixed startup delay and does not use Task 13 reacquisition to hide startup work.

## Decision

Use a controlled pre-activation video preparation transaction.

The startup coordinator selects the initial video key access unit and matching
audio interval as today, but ownership remains in a new preparation state. The
selected video unit may pass through Decode, HardwareTransfer, FrameRate, and
Filter while the playback epoch is still inactive. The first filtered frame is
retained exactly once. A typed readiness result then authorizes one atomic
operation that activates the initial epoch and releases the retained video
frame together with the matching audio batch.

The following alternatives are rejected:

- Prebuilding CUDA filter resources from planner dimensions alone is not
  equivalent because the decoder-owned input `hw_frames_ctx` only exists after
  decoding a real hardware frame.
- Activating first and rebasing or reacquiring after lazy initialization makes
  initialization latency part of playback drift and violates the initial epoch
  contract.
- Probing with a disposable frame would drop or duplicate lineage and is not
  permitted.

## Ownership and State

The preparation transaction owns four states:

```text
Selected -> PreparingVideo -> ReadyToActivate -> Released
                         \-> Failed
```

- `Selected` owns the coordinator's immutable initial video and audio batches.
- `PreparingVideo` transfers the selected video unit through the preparation
  lane and accepts exactly one lineage-preserving filtered result.
- `ReadyToActivate` owns the filtered video frame, original canonical timing,
  generation, sequence, and the still-retained audio batch.
- `Released` is entered only after epoch activation and atomic downstream
  capacity commit both succeed.
- Any error enters `Failed`, aborts the transaction, and releases all retained
  resources through RAII. Retry after backpressure must not decode, filter,
  activate, or publish twice.

The preparation lane is not a second playback path. After readiness, normal
video processing consumes the prepared first frame and subsequent released
units through the same production branch. The old packet barrier and legacy
A/V synchronization path remain absent.

## Planner Contract

The planner remains the sole policy owner. It must provide one complete filter
preparation contract containing the selected filter identity, input and output
time bases, input and output frame rates, dimensions, pixel formats, sample
aspect ratio, color metadata, hardware backend, and generation capacities.

Runtime evidence may supply only values that cannot exist before decode:
the real decoded frame descriptor and decoder-owned `hw_frames_ctx`. Runtime
nodes validate these values against the plan and fail on mismatch.

The existing near-duplicate filter keys and defaults are removed from the
production preparation path. Missing frame rate, sample aspect ratio, filter
identity, hardware context, or timing fails during planning, construction, or
preparation as appropriate. There is no software fallback or passthrough
default. Hardware remains the default selected path; software is used only
when the request explicitly disables hardware.

## Activation and Timing

Filter graph construction and its variable CUDA initialization time occur
before `activateInitial`. No active master-clock deadline advances for the
retained initial media during preparation.

After the readiness result arrives, activation derives the playback mapping
from the retained canonical source timing and the actual activation clock
sample. The prepared video frame and matching audio batch receive the same
generation and activation mapping. The output scheduler therefore sees two
current heads rather than an old audio head followed by a late video head.

Task 13 remains unchanged: a discontinuity after activation still requests
explicit reacquisition and fails closed until automatic cross-generation
reacquisition is implemented.

## Backpressure and Failure Rules

- Every preparation edge has a planner-defined bounded capacity.
- The transaction preflights all release outputs before activation commit.
- WouldBlock retains ownership and returns a wakeable wait result; it never
  spins, expands a queue, drops media, or repeats hardware initialization.
- Decode, filter, lineage, generation, timing, and hardware-context failures
  abort before epoch activation with the original structured error.
- Stop and abort clear the retained access unit, decoded frame, prepared frame,
  audio batch, readiness token, and pending transaction.

## Verification

Deterministic tests must prove:

1. The initial epoch remains inactive while hardware video preparation runs.
2. The first prepared frame preserves generation, canonical timing, key-frame
   lineage, and the real decoder `hw_frames_ctx` lineage.
3. Readiness activates exactly once and atomically releases one prepared video
   frame with the matching audio batch.
4. Backpressure retries do not duplicate decode, filter initialization,
   activation, or release.
5. Missing planner fields and hardware-context mismatches fail without default
   or fallback.
6. Stop and abort release every retained resource.

Production acceptance requires one fixed-hash realtime CLI run proving:

- FFmpeg sender counters grow for both RTP streams.
- The production DAG receives video and audio and selects the highest-scored
  hardware chain.
- Filter preparation completes before initial epoch activation.
- Scheduler video/audio heads and encoded push/pop counters continue advancing
  with bounded queues and memory.
- An external FFmpeg receiver decodes both streams from the generated SDP.
- Only after those gates pass is VLC opened while sender, CLI, and receiver
  parent lifetimes remain active for user observation.
