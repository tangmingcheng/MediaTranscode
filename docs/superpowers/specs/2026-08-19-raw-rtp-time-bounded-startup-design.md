# Raw RTP Time-Bounded Startup Design

## Problem and Scope

Raw RTP automatic signaling discovery currently reuses `probeSizeBytes` as a
cumulative prepared-input byte budget. A real H.264 source at approximately
8 Mbit/s reached `observed_bytes=4999540` with a 5,000,000-byte capacity and
failed before DAG construction. The failure is caused by bitrate multiplied
by startup duration, not by an invalid source or an exhausted runtime resource.

This design removes cumulative-byte probing from every raw RTP input path. It
applies to VideoOnly and AudioVideo, automatic and caller-supplied signaling,
and the shared Windows/Linux/RK realtime core. FFmpeg `probesize` remains valid
only for URL and MPEG-TS inputs and is outside this change.

## Public Facts and Startup Deadline

A raw RTP request supplies one caller-observable startup fact:
`maximumStartupWait`. The application creates one absolute steady-clock
deadline before resource admission. Admission, socket bind, discovery, every
serial replan, hardware capability validation, graph build and prepare,
random-access wait, and final commit all consume the same deadline. No phase
may recreate or extend it. Socket read timeouts are cancellation quanta, not
business deadlines. The existing first-output deadline begins only after the
graph startup transaction commits.

Raw RTP planning and runtime must not read `probeSizeBytes`,
`maximumBufferedBytes`, or `MediaRawRtpPreparedByteBudget`. Total observed
bytes are saturating telemetry only and never control success or allocation.

## Engine Resource Authority

`MediaRealtimeEngineResourcePolicy` is a deployment-level engine construction
input, not a media request, CLI media parameter, or Beta C session field. It
contains facts the hosting environment can directly establish, including the
engine ingress-memory limit, maximum admitted sessions, and allocator,
cgroup, or job-object limits. The core provides no default or sample-derived
capacity.

One engine-scoped `MediaRealtimeResourceGovernor` owns the admission ledger.
Before binding sockets, a controller atomically acquires one non-copyable RAII
`MediaRealtimeResourceLease`. The lease reserves both a session slot and all
ingress bytes and exposes an immutable `MediaRealtimeResourceEnvelope` plus a
resource-contract revision. Failure returns `ResourceAdmissionDenied` before
network state is created. Destroying the authority releases the lease only
after receive activity has stopped and all owned storage has been destroyed.

The planner uses checked arithmetic to divide the admitted envelope among the
platform receive arena, descriptors, canonical parameter-set state, reorder
ownership, candidate access unit, RTCP state, and bounded runtime event
storage. Platform socket capacity and observations constrain the product but
cannot create or enlarge the budget. A capability smaller than the planned
product may shrink it within the existing lease or fail before DAG startup;
the runtime cannot acquire more memory, resize, or fall back.

The Beta facade may supply its explicitly temporary fixed deployment profile.
Existing CLI queue and maximum-unit fields may be mapped only by the
application adapter as migration debt. Neither mapping may use raw RTP
`probeSizeBytes`, cumulative observed bytes, or sample-specific constants, and
neither becomes a core planner or node default.

## Single Ingress Authority and DAG Seam

`MediaRawRtpIngressAuthority` is the execution-scoped RAII owner of the
resource lease and the VideoOnly session or AudioVideo session pair. Each
session owns the same RTP/RTCP sockets and shared parser, reorder,
depacketizer, signaling, and clock state from bind through streaming. The
platform adapter only performs bounded receive and cancellation: Windows uses
the implemented IOCP/overlapped adapter and Linux/RK uses the implemented
`recvmmsg` adapter. RIO and io_uring are not advertised until complete runtime
adapters prove their ownership and cancellation contracts.

The authority is not a media pipeline. It cannot emit to a decoder or bypass
the graph. The compiler moves a node capability into the existing
`RawRtpInputNode`; all format, clock, packet, and event output continues through
that node's existing DAG edges. A controller planning capability and an
uncommitted node capability reference the same authority but cannot copy or
own its socket, parser, or media state.

The socket receive thread is the only writer of mutable ingress state.
Snapshots, arm, rollback, commit, stop, and abort are serialized by that
owner. Controllers receive immutable snapshots and tokens published as one
consistent revision; they never read parser, reorder, or clock members
directly.

## Incremental Discovery and Generations

The session state machine is:

1. `DiscoverFacts`
2. `AwaitGraphArm`
3. `AwaitRandomAccess`
4. `Streaming`

During discovery the session incrementally parses RTP/RTCP and discards
ordinary media payload. It does not retain received datagrams or construct a
replay queue. It retains only planner-bounded canonical signaling state,
generation-scoped clock evidence, ingress observations, and one candidate
random-access unit after arm.

H.264 parameter sets are tracked by identifier and PPS-to-SPS references.
HEVC parameter sets additionally track SPS-to-VPS references. A configuration
is ready only when the random-access unit's referenced dependency closure is
complete for the same source generation. Repeated semantically identical
sets do not create a new generation. Full normalized semantic comparison is
authoritative; a digest is diagnostic only.

The immutable contract token contains only:

- `sourceIdentityGeneration`: validated transport identity, SSRC, and CNAME;
- `signalingGeneration`: payload type, clock rate, codec, and canonical
  parameter-closure semantics;
- `clockDomainGeneration`: clock identity or restart, not the latest SR value;
- `resourceContractRevision`: admitted envelope, platform capability, and
  planner product fingerprint.

Normal RTCP Sender Reports, evidence freshness, repeated identical parameter
sets, interarrival statistics, reorder statistics, and datagram geometry that
remains within the plan update telemetry without changing the token. A token
changes only when a fact invalidates a planner product.

## Transactional Graph Startup

The graph executor provides a general prepare/commit/rollback transaction; it
is not raw-RTP- or RK-specific. All nodes and edges first complete non-running
prepare. Workers cannot call `process()` before commit. `RawRtpInputNode`
registers a pull-only consumer capability, with at most one outstanding pull.

After graph prepare succeeds, `prepareArm(token)` verifies the composite facts
without permitting output and enters `AwaitRandomAccess`. The same receiver
continues incremental processing. H.264 waits for a complete independently
decodable IDR. HEVC follows its codec policy for IDR/BLA/CRA, AP/FU ordering,
DON fields, and RASL exclusion. Candidate storage is part of the resource
lease.

When configuration, clock evidence, and a complete random-access unit are
ready, the authority compares the token again. A stable token permits one
commit barrier that activates the executor and the consumer lease. The
controller then relinquishes its planning capability. The node pulls one
generation-consistent ordered event batch: configuration, clock, then encoded
access unit. The builder, not the session, decides whether canonical signaling
becomes codec extradata, packet side data, or an Annex-B prefix.

Before commit, a stale token returns `ReplanRequired`. The authority rolls back
to `AwaitGraphArm`, clears the candidate, and retains the same socket and
lease. The unstarted graph is destroyed through RAII before one serial rebuild
begins. At most one planning/build transaction exists per session. Any node
prepare failure, deadline expiry, or commit failure follows the same zero-
output rollback rules.

AudioVideo uses one composite token and a two-phase arm transaction across
both sessions. Neither session can commit or output alone. Cross-stream common
start, trimming, release, and drift policy remain exclusively in the existing
DAG A/V coordinator.

## Streaming and Failure Semantics

After commit there is no implicit hot replan. A change to source identity,
payload/clock/codec contract, canonical parameter-set semantics, or clock
domain terminates the graph with `SourceConfigurationChanged`. Normal Sender
Report freshness does not change the clock domain. Future seamless graph
replacement requires a separate application-level design.

The pull path and every receive/event slot remain inside the lease envelope.
There is no unbounded mailbox or handoff queue and no post-start heap growth.
If a background receive adapter is used, its bounded ring and descriptors are
part of the planner product. Stop and abort interrupt receive, wait for all
outstanding pulls to return, join the sole writer, destroy storage, and only
then release the governor lease.

Overflow is typed by resource category:

- incomplete candidate AU or reorder pressure records discontinuity, clears
  the candidate, and returns to `AwaitRandomAccess` only when the planner
  product explicitly permits bounded resynchronization;
- incomplete or conflicting signaling returns to `DiscoverFacts` and
  invalidates the pre-commit token;
- a confirmed legal parameter set, closure, datagram, or access unit that
  cannot fit the admitted envelope fails immediately with
  `ResourceEnvelopeExceeded`;
- adapter storage-contract violations are terminal;
- missing startup facts fail only when the absolute deadline expires.

There is no fixed retry count, capacity growth, software fallback, source
parameter reduction, or hidden packet replay.

## Diagnostics and Acceptance

Diagnostics report selected adapter; source and signaling generations; token
staleness and rollback count; governor reserved sessions and bytes; every arena
capacity and high-water mark; total observed bytes; datagrams discarded before
arm; random-access wait; reorder, duplicate, loss, discontinuity, and resync;
outstanding pulls; post-start allocations; commit generation; and final exit
reason. Kernel/socket drop evidence is reported only when the platform adapter
can prove it; otherwise the value is `unavailable`, never a fabricated zero.

Temporary TDD proves the red 5 MB failure and then proves time-bounded
incremental discovery, stable contract tokens, admission accounting,
transaction rollback, bounded random-access handoff, and stop/abort. All test
sources, targets, and artifacts are deleted before delivery.

Production verification uses Release binaries and the unchanged 120-second
source without lowering bitrate, resolution, frame rate, duration, or required
transcode steps. Windows and RK229 must run the same raw RTP scenario with
automatic signaling, total received bytes greater than 5 MB, codec and
resolution conversion, and MPEG-TS over RTP output. Acceptance requires a
complete startup, visible playback without corruption or stutter, bounded
memory, truthful CPU and ingress telemetry, zero unexpected fallback, and
source-driven termination. Final frozen code requires the prescribed Windows
clean-first build, `ffenv on` plus `mtenv on` RK229 Release build with eight
workers, cleanup evidence, and approval by two independent reviewers.
