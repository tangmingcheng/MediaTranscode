# Industrial Realtime Resource and Transport Shaping Design

## Scope and Authority

Realtime callers provide only facts obtained directly from the media request,
protocol session, device, or deployment. DAG queue counts, retained-byte
capacities, startup access-unit limits, prepared-handoff capacities, and
pacing internals are not per-stream caller inputs.

The planner owns admission, topology resource contracts, mux policy, RTP
session policy, and the finite set of legal rate generations. Builders bind
complete products. Runtime modules execute or validate those products without
defaults, reconstruction, fallback, silent drop, or local policy selection.

This design amends the canonical MPEG-TS datagram pacing design. Observed AU
size and cadence measure demand only; they never authorize a service rate
above the planner-owned envelope. The earlier rule that accepts any
representable AU applies only inside the admitted rate, burst, backlog, and
residence envelope.

Windows and Linux/RKMPP use the same planner, topology composition, mux, RTP
session scheduler, transport shaper, sender, ownership, and failure semantics.
Only operating-system I/O, timer, socket, driver, and hardware adapters may
differ.

## External Facts

The media request retains directly known facts such as codecs, stream set,
output dimensions and frame rate, GOP, CBR/VBR rate-control values, encoder
buffer size, RTP endpoint, address family, payload type, clock rate, and
maximum RTP payload.

Resource, latency, and network-service facts come from a deployment envelope
supplied by the embedding application rather than per-stream CLI arithmetic.
It contains an application memory budget, maximum pipeline residence, maximum
network jitter, and one transport service contract. Admission excess fails
before graph creation; runtime contract excess terminates. Drop, clamp,
degrade, resize, or retry policy is not caller-selectable.

A provisioned transport contract carries an authoritative service curve,
scope, validity, maximum outage or burst residence, MTU, and independently
established aggregate capacity. A closed-domain, application-paced UDP
contract without feedback is permitted only when the deployment supplies
equivalent managed-network evidence. It proves bounded application resources
and pacing, not delivery or finite path residence. Any other long-lived UDP or
RTP contract requires negotiated feedback, congestion control, bounded
reaction, and a circuit breaker.

The following realtime CLI resource controls are removed rather than hidden
behind defaults:

- `--metadata-queue`;
- `--packet-queue`;
- `--frame-queue`;
- `--mux-queue`;
- `--startup-max-video-unit-bytes`;
- `--startup-max-audio-unit-bytes`;
- startup gap and prepared-handoff packet or byte capacities;
- `--probe-size` as a retained-memory or handoff-capacity input.

Any remaining observation-work limit has a distinct typed meaning and comes
from the deployment observation budget. It cannot be reused as retained-byte,
runtime-edge, socket, or handoff capacity.

## Capacity Evidence

Observed startup maxima are telemetry, not proof of future live-stream bounds.
Every hard capacity records one evidence kind:

- protocol maximum;
- negotiated sender contract;
- parsed conforming HRD contract;
- planner-controlled encoder capability;
- deployment acceptance limit.

H.264 and HEVC RTP packetization or fragmentation do not provide a protocol
maximum for future NAL or AU size. HRD evidence is admissible only after
complete parameter-set and conformance validation proves its coded-buffer
semantics. A parameter-set or HRD change starts a planner-authorized generation
or terminates.

Every live source product covers raw RTP video and audio, generic or RTSP
input, MPEG-TS input, packet copy, and transcoded branches with an arrival and
maximum-item envelope. If negotiation or protocol facts cannot prove a future
bound, the planner may use only a typed deployment acceptance limit. Runtime
enforces that limit incrementally during packet, FU, NAL, PES, and AU assembly
before retained bytes exceed it, including missing-marker, loss, and
incomplete-unit cases. An excess reports a source-contract or resource-envelope
failure; it never grows storage or relabels an observation as source capacity.

## Prepared Encoder Session

A planner-owned `PreparedEncoderSession` opens the selected encoder before
resource admission, verifies all public and backend-private rate-control
readback, and owns the exact opened codec context through RAII. It produces an
`EncoderEmissionEnvelope` containing:

- cadence, reorder depth, async depth, lookahead, and hardware surface needs;
- sustained and maximum encoded rates;
- burst or coded-buffer envelope;
- maximum AU evidence when the backend can prove it;
- the complete post-open readback evidence.

The resource planner consumes that envelope. The builder move-binds the same
prepared session into the encoder node. Reopening or independently rebuilding
the encoder context is forbidden. Failure to retain or bind the prepared
session fails before DAG startup.

Encoder buffer size is a VBV/CPB fact. Dividing it by a rate may describe a
coded-buffer time scale only when the prepared encoder proves that contract.
It never directly becomes network delay, socket capacity, path capacity,
transport backlog, or end-to-end latency.

## Independent Rate Products

`EncoderRateControlMode`, `TsMuxRateMode`, and
`TransportPacingPolicy` are independent typed products.

CBR encoder mode does not imply constant MPEG-TS transport. Constant TS uses a
planner-selected aggregate mux rate, consumes fixed TS packet slots, and
inserts null packets. Variable TS uses its complete arrival envelope for
admission. For VBR encoding, target bitrate is an average objective; the
verified maximum rate and burst envelope constrain admission and pacing.
Missing maximum-rate or burst evidence fails before DAG startup when required
by the selected mux or transport service.

## Mux and RTP Session Products

`MuxTrafficEnvelope` describes only the MPEG-TS program and contains:

- video and optional audio arrival envelopes;
- PES header and fragmentation geometry;
- TS packet and adaptation-field geometry;
- PAT and PMT section sizes, repetition deadlines, versions, and change rules;
- PCR PID, insertion form, repetition deadline, and jitter limit;
- continuity-counter, discontinuity, and null-stuffing policy;
- exact TS packet cost of all program maintenance.

`RtpSessionEnvelope` is separate from the mux product and contains:

- RTP profile, SSRC lifecycle, timestamp and sequence contracts;
- RTP and RTCP endpoint relationships;
- sender-report, receiver-report, CNAME, feedback, BYE, and generation rules;
- RTCP bandwidth, randomization, interval, and control-packet envelope;
- packetization and transport overhead contract.

The packetization contract includes IP version and maximum IP-header length,
UDP overhead, RTP base, CSRC, extension, padding and security overhead, path
MTU or authoritative maximum IP packet size, and integral TS packets per RTP
payload. Output requiring IP fragmentation is rejected. Byte-rate, packet-rate,
and burst envelopes are calculated after all header, ceiling, and alignment
effects. Inclusion or exclusion of link-layer overhead is explicit.

RTP media and RTCP control retain their protocol-specific scheduling
authorities. The mux exclusively orders TS packets. The RTP session scheduler
packetizes ordered TS data and schedules RTCP. Both feed one destination-scoped
aggregate transport shaper, which accounts for all application-visible UDP/IP
traffic without reordering TS payload.

## Adaptive Rate Generations

An `AdaptiveTransportServiceEnvelope` identifies the negotiated feedback
format, controller, control interval, admitted minimum and maximum rates,
reaction deadline, feedback-loss timeout, circuit breaker, and a post-open
proven encoder and mux actuator.

The planner owns the finite set and transition rules of legal rate
generations. A controller transition quiesces the old generation, drains or
terminates its bounded reservations according to the existing contract,
applies and reads back encoder rate/CPB and mux policy, then atomically publishes
the new generation. Unsupported transitions, actuator failure, readback
mismatch, missing feedback, or reaction expiry are terminal. The transport
shaper and sender remain execution-only and cannot choose an encoder rate.

## Complete Resource Ledger

`MediaRealtimeResourcePlan` covers every executable edge and every runtime
retention locus. A direct edge has an explicit zero-retention synchronous
ownership contract; it is not omitted. Other ledger entries include:

- producer-held incoming items and atomic reservations;
- node-private pending input or output;
- prepared input and handoff state;
- decoder, filter, encoder, and hardware surface pools;
- mux cursors, PSI/PCR state, RTP/RTCP state, and scheduled batches;
- fan-out references, allocator and alignment slack;
- capped userspace and kernel socket buffers.

Admission reserves item, physical-byte, and residence budgets before
materialization or ownership transfer. Shared physical allocations are charged
once to the graph allocation ledger while every lease is tracked until release.
Residence begins at the first planner-defined retention. An interruptible
deadline executor expires retained state even when no producer or consumer is
otherwise runnable.

Each `MediaEdgeCapacityContract` and runtime-owner contract contains maximum
items, maximum physical bytes, maximum residence, overflow behavior, ownership
cost, release rule, and evidence. Every product is created through a validating
factory, cannot be partially or default constructed, and has no policy-valued
member initializer.

The resource planner consumes the executable topology, prepared source
contracts, prepared encoder session, mux envelope, RTP session envelope,
transport service envelope, platform capabilities, and deployment envelope.
For an arrival envelope `A(t) <= r*t + b`, it accounts for maximum item size,
item and byte rates, atomic burst, codec reorder, encoder async depth, mux
interleave, ownership fan-out, hardware alignment, and the selected residence.
The global byte budget constrains the sum but does not invent edge ratios or
latency. Missing facts, arithmetic overflow, or an infeasible service curve
fail before graph creation.

## Exact Plan Projection

`MediaRealtimeResourcePlan` is serialized once as a complete typed runtime-plan
member keyed by stable edge and runtime-owner identities. Decoding reproduces
every field exactly and rejects unknown, missing, duplicate, extra,
stale-topology, or overflowing entries. Builders bind decoded contracts only;
string defaults, enum ordinals, edge-class lookup, and local reconstruction are
forbidden.

Realtime plan and product types are not default constructible. A completeness
validator rejects legacy generic queue and memory defaults. Raw RTP retains the
exact prepared RTP/RTCP sockets, shared replay clock, and one aggregate A/V
byte ledger. VideoOnly creates no audio transport, budget participant, node,
edge, mux stream, or diagnostic entry. AudioVideo and VideoOnly use the same
planner, builders, nodes, and topology composition on Windows and RKMPP.

## Global Scheduling and Sender Contract

The planner produces service envelopes and scheduling policy, not a list of
unknown future AU deadlines. Runtime uses one continuous destination aggregate
token-bucket or constant-rate state across media AUs and RTP/RTCP control data.

Every packet reservation carries `enqueueNotBefore` and `enqueueNotAfter`,
derived from one monotonic startup anchor, media and PCR timing, CPB/decoder
deadline, maximum residence, batching allowance, and verified timer error.
A large AU may be smoothed only when every fragment remains inside its
latest-send, PCR, coded-buffer, decoder, and end-to-end latency constraints.
Demand exceeding peak, burst, backlog, residence, or deadline terminates; it
never raises the service envelope or catches up by bursting. Timestamp wrap or
discontinuity starts only a planner-authorized generation.

The sender validates generation and non-overlapping reservations, waits for
`enqueueNotBefore`, and performs nonblocking atomic datagram enqueue. On
`EAGAIN` or `WSAEWOULDBLOCK` it may retry only until `enqueueNotAfter`; short
or late writes are terminal. It cannot change rate, reorder, rebase deadlines,
or create another timeline. A successful userspace enqueue is not wire
completion evidence.

## Failure and Diagnostics

All violations are explicit and terminal: source-envelope excess, encoder
readback mismatch, resource-budget excess, residence expiry, generation
conflict, timestamp or reservation regression, late packet, socket pressure,
short write, feedback loss, and circuit-breaker activation. Strict or adaptive
service requested without matching platform, actuator, session, or network
evidence fails before DAG startup.

Diagnostics report planned and observed item and physical-byte occupancy,
residence, maximum AU, encoder envelope and readback, mux rate and maintenance,
RTP/RTCP aggregate pacing, burst, deadlines, enqueue timing, socket pressure,
receiver feedback, packet loss, PCR interval and jitter, CPU, RSS, drops, A/V
drift, and final exit reason. Observations never become runtime authority.

## Acceptance

Temporary TDD may prove product construction, capacity mathematics, exact
serialization, generation transitions, channel enforcement, and scheduling.
All temporary test source, targets, and binaries are deleted before delivery.

Final acceptance uses Release builds and the unchanged 2K continuous
120-second source. It covers H.264 and HEVC in both directions, resolution and
codec conversion, CBR and VBR encoder modes, constant and variable TS where
selected, VideoOnly and AudioVideo, Windows and RKMPP, and the production DAG.

Production-DAG failure gates cover provisioned under-capacity rejection,
best-effort pressure, feedback step-down and recovery, feedback loss,
circuit-breaking, failed generation readback, oversized fragmented RTP AU,
late reservations, and resource/residence excess. Protocol gates verify
PAT/PMT/PCR periods, continuity, no IP fragmentation, RTP/RTCP aggregate
pacing, cross-AU burst bounds, and receiver sequence/loss evidence.

Each gate records exact source, CLI, receiver and cleanup commands, precise
PIDs, packet timing and loss, queue bytes and residence, CPU/RSS, A/V drift,
PCR jitter, zero-copy telemetry, full output decode, and truthful termination.
A 1 Mbps diagnostic cannot replace the 4 Mbps gate, receiver-buffer changes
cannot substitute for correcting application-generated bursts, and no source
or acceptance parameter may be reduced to obtain a pass.
