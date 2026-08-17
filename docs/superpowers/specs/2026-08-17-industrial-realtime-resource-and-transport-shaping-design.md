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
buffer size, RTP endpoint, address family, payload type, and clock rate. A
caller does not calculate an RTP payload size. The deployment or negotiated
session supplies an authoritative path MTU, maximum IP packet fact, or sender
payload limit together with its evidence kind.

Resource, latency, and network-service facts come from a deployment envelope
supplied by the embedding application rather than per-stream CLI arithmetic.
It contains independent resource-domain budgets, one required
`PipelineResidenceRequirement`, and one transport service contract.
`PipelineResidenceRequirement` is a closed variant:
`TerminalDeadlineOnly(maximumResidence)` permits `BoundedResourceOnly` stages
and promises bounded retention plus terminal expiry only;
`ProvenSuccessfulResidence(maximumResidence)` requires `ProvenService` for
every reachable stage and fails before graph creation otherwise. No layer
infers a requirement variant from a numeric duration. Finite network jitter and receiver or
decoder residence are required only for service variants that prove a finite
path-service bound. Admission excess fails
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

`TransportServiceEnvelope` is a closed variant of `ProvisionedService`,
`AdaptiveService`, and `ManagedBestEffortService`. Managed best effort is an
explicit closed-domain deployment choice with no positive path-service or
delivery guarantee; it proves bounded local retention, application pacing,
and terminal pressure behavior only. Public or unmanaged long-lived UDP/RTP
without negotiated congestion control is rejected before DAG startup.

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
- `--packet-size` as a caller-calculated RTP or UDP payload capacity.

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

Prepared encoder creation is a pre-admission resource transaction. The backend
capability product first provides enforceable upper bounds for codec-private,
pageable, pinned, registered, device, DRM-surface, and backend-opaque
allocations. The planner reserves those bounds in their resource domains
before `avcodec_open2`. A backend unable to prove or enforce a preparation
bound fails planning without being opened.

The planner-owned `PreparedEncoderSession` then opens the selected encoder,
verifies actual allocation and all public and backend-private rate-control
readback, commits the reservation, releases unused capacity, and owns the exact
opened codec context through RAII. Failure destroys the transaction and
session. It produces an
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

## Route-Neutral Datagram, Mux, and RTP Products

`DatagramTransportEnvelope` exclusively owns endpoint facts, IP and UDP
overhead, authoritative MTU, legal UDP payload, socket contract, and service
scope. It references one
closed packetization variant:

- `MpegTsUdpPacketization`;
- `MpegTsRtpPacketization`;
- codec-specific `ElementaryRtpPacketization` for video or audio.

`RtpSessionEnvelope` exists only for RTP variants. The rule that an RTP payload
contains an integral number of TS packets applies only to MPEG-TS/RTP.
Elementary H.264, HEVC, AAC, and other admitted RTP payloads retain their own
protocol packetization contracts. The planner consumes the selected variant;
runtime cannot reconstruct route-local policy.

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
- a reference to the selected RTP packetization product.

The datagram envelope owns IP version, maximum IP-header length, UDP overhead,
MTU, and maximum IP packet size. The selected packetization variant exclusively
owns payload framing plus RTP base, CSRC, extension, padding, security, and TS
alignment overhead where applicable. The planner derives one immutable
`WireTrafficEnvelope` from these non-duplicated facts and validates any
negotiated sender limit. Output requiring
IP fragmentation is rejected. Byte-rate, packet-rate,
and burst envelopes are calculated after all header, ceiling, and alignment
effects. Inclusion or exclusion of link-layer overhead is explicit.

Every `TransportServiceEnvelope` variant supplies an authoritative stable
`TransportServiceScopeId` rather than deriving one from a destination or socket
at runtime. Provisioned service obtains it from the reserved path or interface;
adaptive service obtains it from the congestion-controller/path scope; managed
best effort obtains it from the explicitly managed egress scope. Every socket
in that scope shares one capacity ledger, resource ledger, shaper, packet-rate
envelope, and controller where applicable; separate video, audio, and RTCP
ports cannot each claim the complete service capacity.

RTP media and RTCP control retain their protocol-specific scheduling
authorities. The mux exclusively orders TS packets. The RTP session scheduler
packetizes the selected MPEG-TS or elementary media variant and schedules
RTCP. Both feed one service-scope
aggregate transport shaper, which accounts for all application-visible UDP/IP
traffic without reordering TS payload.

## Adaptive Rate Generations

An `AdaptiveTransportServiceEnvelope` identifies the negotiated feedback
format, controller, control interval, admitted minimum and maximum rates,
reaction deadline, feedback-loss timeout, circuit breaker, and one selected
`MediaEncoderGenerationMechanism`. The mechanism is a closed variant:

- `PreparedSessionSwitch` owns one immutable, pre-opened encoder session per
  retained generation. Admission charges every simultaneously retained
  session and its old-generation drain/new-generation startup state. A
  transition switches sessions; it never mutates a session after open.
- `InPlaceActuatorTransition` owns exactly one encoder session. Generation
  products contain immutable target capability, rate-control, mux, transport,
  resource, and scheduling facts but no second prepared session. This variant
  is legal only when a target-specific capability probe proves atomic
  post-open bitrate and CPB mutation plus authoritative readback for the
  selected encoder/backend.

The variants cannot be mixed or reconstructed at runtime. If neither variant
is proven for a selected Windows or RKMPP backend, adaptive service is rejected
before DAG creation; static provisioned or managed-best-effort service remains
available when its own facts are complete.

This design also amends the MPEG-TS H.264/HEVC rate-control design. Every legal
generation contains a complete prepared encoder capability,
`MediaEncoderRateControlPlan`, mux plan where present, datagram and RTP session
envelopes, edge and runtime-owner resource contracts, and scheduling
parameters. It contains a prepared session only for `PreparedSessionSwitch`.
Admission validates every transition and reserves the mechanism-specific
simultaneous footprint.

The planner owns the finite set and transition table of legal rate generations.
Feedback events map only to planner-authored transition keys; a controller
cannot calculate, clamp, or invent bitrate values. At a rate decrease, the new
rate ceiling is effective no later than the planner-authored reaction deadline.
Every retained object and node-private state carries one immutable generation
identity from encoder-input admission through final datagram enqueue. Before
either mechanism commits, a transition barrier stops new old-generation
admission and enumerates input frames, encoder lookahead and async output,
hardware surfaces, encoded AUs, mux cursors and maintenance state, packetizer
state, RTP/RTCP control state, scheduled batches, and packet reservations.
The planner-authored transition assigns every enumerated object exactly one
disposition: complete under the new service ceiling before its original
deadline, or terminate the graph. Drop, old-rate drain beyond the reaction
deadline, deadline rebasing, burst catch-up, and cross-generation relabeling
are forbidden.

`PreparedSessionSwitch` flushes all delayed old-session output into the old
generation before atomically switching to the admitted session; every resulting
packet is revalidated against the lower ceiling and original deadline.
`InPlaceActuatorTransition` is legal only when the backend additionally proves
a deterministic frame-level mutation boundary covering already queued work.
Otherwise the old encoder state must be proven empty before mutation begins or
the variant is rejected. The reaction deadline covers quiescence, complete
old-state disposition, session switch or actuator mutation, readback, and
atomic publication. Runtime replanning or product reconstruction is forbidden.
Unsupported transitions, actuator failure, readback
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

Source and codec adapters expose a bounded allocator or incremental assembly
interface that enforces the accepted item limit before opaque allocation. An
adapter unable to enforce the limit is rejected before DAG startup.

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

The deployment envelope contains independent hard budgets for pageable process
memory, pinned or registered host memory, device and DRM surfaces, kernel
socket memory, and backend-opaque memory. Every physical allocation has one
stable allocation identity and one checked cost vector with at most one charge
in each applicable resource domain. Shared references reuse that identity and
do not charge the same domain twice. Imports, mappings, metadata, and backing
storage have separate typed costs when they consume distinct resources. Every
nonzero vector component names an authoritative accounting adapter and is
validated against its domain budget; bytes from different domains are never
substituted or pooled. An additional total-host-physical limit may constrain
applicable host domains.

Every executable node provides a `MediaStageServiceEnvelope` containing its
execution authority and thread, concurrency, batch limit, bounded internal
depth, backpressure seam, cancellation behavior, and one closed
`MediaStageServiceEvidence` variant. `ProvenService` contains an authoritative
service curve or worst-case processing latency and scheduling jitter;
`BoundedResourceOnly` contains no service claim and proves only bounded memory
plus terminal deadline enforcement. Edge backlog and delay are derived only
from `ProvenService` evidence. A deployment that requires proven maximum
pipeline residence rejects any `BoundedResourceOnly` stage before graph
creation. Otherwise the executable plan states that successful residence is
unproven and never reports it as guaranteed.

The resource planner consumes the executable topology, prepared source
contracts, the mechanism-selected prepared encoder binding, optional mux and
RTP session envelopes, datagram transport envelope, stage service envelopes,
platform capabilities, and deployment envelope.
For an arrival envelope `A(t) <= r*t + b`, it accounts for maximum item size,
item and byte rates, atomic burst, codec reorder, encoder async depth, mux
interleave, ownership fan-out, hardware alignment, and the selected residence.
Each resource-domain budget constrains its own ledger and does not invent edge
ratios or latency. Missing facts, arithmetic overflow, or an infeasible service curve
fail before graph creation.

## Exact Plan Projection

One immutable `MediaRealtimeExecutablePlan` is serialized as the complete
runtime authority. It contains the resource ledger, selected datagram
transport and packetization variants, optional mux and RTP session products,
the derived `WireTrafficEnvelope`, service scope, stage service envelopes,
prepared source and encoder binding identities, the selected
`TransportServiceEnvelope`, legal generations, selected generation mechanism,
and transition table. For adaptive service it contains the complete
`AdaptiveTransportServiceEnvelope`: feedback binding, controller and control
interval, reaction and feedback-loss deadlines, circuit breaker, prepared
runtime binding identities, aggregate shaper/deadline policy, and all
generation references. No runtime transport or controller policy exists
outside this plan. Decoding is atomic, reproduces every field exactly,
validates every cross-reference, and rejects unknown,
missing, duplicate, extra, stale-topology, or overflowing entries. Builders
bind this decoded product only; parallel policy strings, enum ordinals,
edge-class lookup, and local reconstruction are forbidden.

Realtime plan and product types are not default constructible. A completeness
validator rejects legacy generic queue and memory defaults. Raw RTP retains the
exact prepared RTP/RTCP sockets, shared replay clock, and one aggregate A/V
byte ledger. VideoOnly creates no audio transport, budget participant, node,
edge, mux stream, or diagnostic entry. AudioVideo and VideoOnly use the same
planner, builders, nodes, and topology composition on Windows and RKMPP.

## Global Scheduling and Sender Contract

The planner produces service envelopes and scheduling policy, not a list of
unknown future AU deadlines. Runtime uses one continuous service-scope
token-bucket or constant-rate state across media AUs and all selected UDP,
RTP, and RTCP data.

Every packet reservation carries `enqueueNotBefore` and `enqueueNotAfter`,
derived from one monotonic startup anchor and only the facts proven by the
selected service variant. For `ManagedBestEffortService`, the latest time is
bounded only by local retention, media ordering, PCR, batching, timer, and
socket-pressure contracts; it makes no receiver, decoder, path-residence, or
end-to-end claim. A finite-path service variant may additionally use its proven
path, receiver, decoder, and end-to-end bounds. A large AU may be smoothed only
when every fragment remains inside all constraints proven for that variant.
Demand exceeding peak, burst, backlog, residence, or deadline terminates; it
never raises the service envelope or catches up by bursting. RTP timestamp and
MPEG PTS, DTS, and PCR wrap are extended into monotonic internal domains and
do not reset scheduling state or create a generation. Only independently
detected source discontinuity may use a planner-authorized transition with
explicit decoder, PCR, RTP, and RTCP rules.

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

Every acceptance gate derives numeric pass/fail bounds from the serialized
plan: zero late or overlapping reservations, occupancy and residence within
contract, allocations within each resource-domain ledger, PCR interval and
jitter within the mux envelope, aggregate UDP or RTP/RTCP pacing within rate
and burst envelopes, adaptive reaction within its deadline, and zero
unclassified drops.

Final acceptance uses Release builds and the unchanged 2K continuous
120-second source. It covers H.264 and HEVC in both directions, resolution and
codec conversion, CBR and VBR encoder modes, constant and variable TS where
selected, VideoOnly and AudioVideo, Windows and RKMPP, and the production DAG.

The route matrix explicitly covers MPEG-TS/UDP, MPEG-TS/RTP, and separate codec
RTP on every supported platform and stream set. Production-DAG failure gates
cover provisioned under-capacity rejection, managed-best-effort pressure,
oversized fragmented RTP AU, late reservations, and resource/residence excess.
Feedback step-down and recovery, feedback loss, circuit-breaking, and failed
generation readback are mandatory only on a platform/backend whose probe proves
the selected `MediaEncoderGenerationMechanism`; unsupported Windows and RKMPP
actuators must instead pass an explicit pre-DAG rejection gate. Protocol gates
verify PAT/PMT/PCR periods, continuity, no IP fragmentation, RTP/RTCP aggregate
pacing, cross-AU burst bounds, and receiver sequence/loss evidence.

Timestamp gates use authoritative near-wrap RTP, PTS, DTS, and PCR inputs in the
unchanged 120-second production DAG. A normal modular wrap must preserve the
extended monotonic timeline and current generation. A separately signaled or
authoritatively detected discontinuity must follow its planner-authored decoder,
mux, RTP, and RTCP transition; it cannot be inferred from wrap or silently reset
the schedule.

Each gate records exact source, CLI, receiver and cleanup commands, precise
PIDs, packet timing and loss, queue bytes and residence, CPU/RSS, A/V drift,
PCR jitter, zero-copy telemetry, full output decode, and truthful termination.
A 1 Mbps diagnostic cannot replace the 4 Mbps gate, receiver-buffer changes
cannot substitute for correcting application-generated bursts, and no source
or acceptance parameter may be reduced to obtain a pass.
