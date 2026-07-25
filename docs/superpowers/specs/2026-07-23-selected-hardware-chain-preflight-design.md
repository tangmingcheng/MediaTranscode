# Selected Hardware Chain Preflight Design

## Goal

The planner must select the highest-scoring structurally available chain before
creating any hardware device. It must then preflight exactly that selected
chain. Hardware remains the default; software candidates exist only when
hardware is explicitly disabled.

## Planning Flow

1. Enumerate candidates using static FFmpeg codec and filter registration.
2. Score and sort candidates without creating devices or opening codecs.
3. Select the first structurally available candidate from the requested tier.
4. Store that candidate as the plan's selected chain.
5. Run one complete decoder, filter, and encoder preflight for the selected
   hardware chain.
6. Return `HardwareUnavailable` if that preflight fails. Do not probe or select
   a lower-scoring candidate.

RTP and MPEG-TS use the same video pipeline planner and therefore share this
flow.

## Runtime Boundary

Planner preflight may create temporary FFmpeg resources for the selected chain
only. RAII releases all temporary resources after validation. Runtime nodes
materialize only the immutable selected plan and have no fallback authority.

On Windows, a D3D11VA message is valid when QSV is the selected chain because
FFmpeg may implement QSV device creation through D3D11. No QSV or D3D11 device
may be created when another chain, such as CUDA, was selected.

## Diagnostics

Diagnostics preserve this order:

1. `planner.score` for all static candidates.
2. One `planner.capability` record for the selected hardware chain.
3. `planner.select` after successful preflight.

Failed selected-chain preflight emits its single capability failure and returns
an error without a selection record.

## Verification

- A unit validator records exactly one invocation and the selected candidate
  identity.
- A failed top candidate does not invoke validation for the second candidate.
- Explicit software mode selects software without invoking hardware preflight.
- Candidate enumeration and scoring do not call hardware device creation.
- Existing planner, RTP, MPEG-TS, and test-tier suites remain green.
- A current CLI diagnostic run must not contain the removed
  `probe=device_create` diagnostic. CUDA selection must not produce D3D11VA
  output before runtime materialization.
