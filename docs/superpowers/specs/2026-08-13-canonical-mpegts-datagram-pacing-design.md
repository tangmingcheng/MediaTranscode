# Canonical MPEG-TS Datagram Pacing Design

## Problem

Project MPEG-TS currently releases a scheduled access unit at `emitOnMaster`,
then writes every TS/RTP datagram for that unit synchronously. Large video
access units therefore create UDP bursts. Commit `9f543b7c` reduced the burst
with a transport-local blocking token bucket, but that introduced a second
pacing authority, an uninterruptible sleep, and no pacing telemetry.

The correction must preserve the existing canonical A/V scheduler, share the
same MPEG-TS emission logic on Windows and Linux, and leave UDP/RTP sinks as
transport-only components.

## Authority and Typed Plan

The realtime output planner produces one required
`MediaTsDatagramEmissionPlan` inside the Project MPEG-TS product. It contains:

- the planned wire-byte rate;
- the maximum initial burst in wire bytes;
- the maximum permitted emission lateness;
- the exact transport datagram byte bound.

The values are derived from resolved video/audio bitrate, TS/RTP overhead,
packet size, transport decode lead, and the existing latency policy. Draft,
runtime plan, graph options, and decoded plan must agree exactly. Missing,
zero, overflowing, or contradictory fields fail before DAG startup. Runtime
nodes do not infer or default any pacing value.

## Non-blocking Emission Schedule

`MediaTsMuxSession` owns a `MediaTsDatagramEmissionSchedule`. The schedule uses
canonical master time and token-bucket arithmetic only to produce the next
datagram deadline; it never reads a wall clock and never sleeps.

For each prepared datagram, the schedule:

1. refills byte credit through the access unit's canonical
   `emitOnMaster` or the preceding committed datagram deadline;
2. returns an immediate deadline when sufficient burst credit exists;
3. otherwise returns the exact future master-time deadline required by the
   planned wire-byte rate;
4. commits credit only after the datagram sink confirms the complete write.

Deadline calculations are overflow checked. A deadline beyond the planned
lateness boundary fails closed instead of accumulating unbounded latency.
UDP and MP2T/RTP use this same schedule. The RTP sink only maps the supplied
canonical deadline to its RTP timestamp, packetizes PT 33, and sends.

## Cursor Ownership and Backpressure

`MediaTsMuxSession` becomes a non-blocking emission state machine. It retains
at most one packet cursor, its framing workspace, prepared clock transaction,
and current datagram transaction. No encoded payload is copied into a second
queue.

`writeAccessUnit` accepts one access unit only when no previous cursor is
pending. `poll` emits only the datagrams whose planned deadline is due and
returns the next deadline through the existing `MediaMuxSessionPollResult`.
`FileMuxNode` polls a pending mux emission before popping another encoded
input. Existing graph queues therefore provide bounded backpressure.

PAT, PMT, PCR, video PES, and audio PES all pass through the same emission
schedule. Clock, cursor, continuity, and byte-credit transactions commit only
after a successful sink write. Abort destroys the retained RAII transactions
without committing them.

## Diagnostics and Failure Semantics

Runtime diagnostics report the selected emission plan and cumulative:

- datagrams and wire bytes sent;
- immediate and deferred deadlines;
- planned wait nanoseconds;
- late datagrams and maximum lateness;
- pending cursor bytes and peak pending bytes;
- transport pressure failures and final pacing exit reason.

No diagnostic counter is hard-coded. Socket pressure, scheduling lateness,
short writes, clock regression, or an impossible plan remain terminal errors.
Stop and abort never wait on a pacing sleep because pacing is expressed only
as a worker deadline.

## Scope Isolation

The transport-local `MediaWritePacingClock` is removed. The unrelated FFmpeg
AVIO pacing path is restored to its pre-`9f543b7c` behavior so this correction
does not change separate RTP output semantics.

Finite separate-RTP input termination is a distinct lifecycle problem. The
observed `RTP video source clock evidence degraded` exit will be handled in a
separate change based on RTCP BYE/source-loss facts; it is not reclassified or
hidden by the pacing work.

## Verification

Temporary TDD probes may verify plan rejection, deadline arithmetic,
transaction commit/cancel, backpressure, and lateness failure. They must show
RED then GREEN and be deleted before delivery.

Final acceptance requires:

- VS2026 Debug clean-first all-target build within 120 seconds;
- Windows and RKMPP real CLI chains using the unchanged 120-second, 2K source,
  HEVC to H.264 conversion, 2560x1440 to 1280x720 scaling, AAC, separate RTP
  input with automatic video fmtp, and MPEG-TS over RTP output;
- RKMPP DRM PRIME/RGA zero-copy evidence with no software frame, download, or
  upload;
- receiver-side RTP sequence loss/reorder evidence in addition to VLC visual
  acceptance;
- pacing, CPU, RSS, queue, drop, A/V drift, and exit-reason telemetry;
- no temporary test, script, process, or port residue.

