# Industrial Realtime Resource and Transport Shaping Design

## Scope

Realtime CLI callers shall provide only facts they can obtain directly from
the media request, protocol session, device, or deployment. DAG edge counts,
byte capacities, startup access-unit bounds, handoff capacities, and pacing
internals are not caller inputs.

This design removes those internal controls from the public realtime CLI and
replaces the current pass-through queue policy with planner-owned, typed
resource and transport products. Windows and Linux/RKMPP use the same planner,
topology, mux, scheduler, sender, and failure semantics. Only capability and
platform I/O adapters may differ.

## External Facts

The media request retains directly known facts such as codecs, stream set,
output dimensions and frame rate, GOP, CBR/VBR rate-control values, encoder
buffer size, RTP endpoint, address family, payload type, clock rate, and
maximum datagram payload.

Resource and latency facts come from a deployment envelope supplied by the
embedding application, not from per-stream CLI arithmetic. The envelope
contains an application memory budget, maximum pipeline residence, transport
service kind, and over-budget disposition. A strict provisioned transport may
also provide an independently established service capacity. An adaptive
transport must provide the feedback and circuit-breaker capabilities required
by its selected controller. A best-effort UDP product may guarantee only
application pacing and userspace-to-kernel enqueue evidence; it must not claim
path capacity or wire completion.

The following realtime CLI options are removed rather than hidden behind
defaults:

- `--metadata-queue`;
- `--packet-queue`;
- `--frame-queue`;
- `--mux-queue`;
- `--startup-max-video-unit-bytes`;
- `--startup-max-audio-unit-bytes`;
- prepared-handoff packet and byte capacities;
- startup gap and other internal residence controls.

## Evidence Model

Observed startup maxima are telemetry, not proof of future live-stream bounds.
Each hard capacity carries an evidence kind:

- protocol maximum;
- negotiated sender contract;
- parsed conforming HRD contract;
- planner-controlled encoder capability;
- deployment acceptance limit.

When an arbitrary H.264 or HEVC RTP source exposes no authoritative future AU
bound, the planner may use a deployment acceptance limit only with that exact
semantics. A later AU exceeding the limit terminates with a source-contract or
resource-envelope error. The planner must never relabel a finite observation,
encoder buffer size, resolution, GOP, or sample maximum as a proven source
maximum.

## Planner Products

`EncoderEmissionEnvelope` describes the selected encoder's verified output:

- cadence and reorder/async depth;
- sustained and maximum encoded rates;
- burst or coded-buffer envelope;
- maximum AU evidence when the backend can prove one;
- post-open readback evidence.

The encoder buffer is an encoder VBV/CPB fact. Its size divided by a rate may
describe a coded-buffer time scale only when the opened encoder proves the
corresponding contract. It is never used directly as network delay, socket
capacity, path capacity, or end-to-end latency.

`MuxTrafficEnvelope` describes the complete multiplexed output:

- video and optional audio arrival envelopes;
- PES and TS geometry;
- PAT, PMT, PCR and RTCP maintenance traffic;
- RTP, UDP, and IP overhead known to the application;
- constant or variable multiplex policy;
- aggregate service and burst requirements.

CBR encoder mode and constant MPEG-TS transport are distinct products. A
constant transport stream uses a planner-selected aggregate mux rate and null
stuffing. VBR uses the verified maximum rate and burst envelope; target bitrate
alone is not a hard capacity. Missing maximum-rate or burst evidence fails
before DAG startup when the selected transport contract requires it.

`TransportServiceEnvelope` describes one of:

- provisioned service with independently established capacity and residence;
- adaptive service with feedback, rate adaptation, and circuit breaker;
- application-paced best-effort UDP with no path-capacity guarantee.

`MediaRealtimeResourcePlan` maps every non-direct edge to a complete
`MediaEdgeCapacityContract` containing maximum items, maximum retained bytes,
maximum residence, overflow policy, ownership cost, and evidence. It also
contains shared pool and total-budget accounting. Product types have no
business defaults and cannot be partially constructed.

## Capacity Planning

The resource planner consumes the executable topology, prepared source
contract, encoder envelope, mux envelope, transport envelope, platform
capabilities, and deployment envelope. It first reserves backend and protocol
minimums, then proves that every edge fits the global memory and residence
budgets.

For an arrival envelope `A(t) <= r*t + b`, an edge capacity is derived from
the selected residence and service contract. Item and byte capacities account
for maximum item size, rate, atomic burst, codec reorder, encoder async depth,
mux interleave, fan-out ownership, and hardware alignment. A global byte
budget constrains the sum but does not invent edge ratios or latency. Missing
facts, arithmetic overflow, or an infeasible plan fail before graph creation.

Builders bind contracts by edge identity. Runtime channels enforce item,
byte, residence, and overflow contracts without growth, fallback, silent drop,
or local policy selection. Existing generic queue and memory defaults are
removed or made structurally unreachable for realtime graphs.

## Global MPEG-TS/RTP Scheduling

The planner produces scheduling policy and proven envelopes, not a future list
of unknown live AU deadlines. At runtime the mux is the only sequencing
authority. It materializes actual video, audio, PAT, PMT, and PCR data into one
monotonic cross-AU reservation timeline.

Large keyframes are paced across the admitted buffering window instead of
being compressed into one frame interval. For constant MPEG-TS, null packets
maintain the planned aggregate mux rate. For VBR, the runtime timeline remains
inside the planned peak, burst, backlog, and residence envelope. The sender
only validates generation and reservations, waits for `enqueueNotBefore`,
performs nonblocking enqueue, and records actual evidence. It cannot change
rate, reorder, catch up by bursting, or derive a second timeline.

## Failure and Diagnostics

All violations are explicit and terminal: source-envelope excess, encoder
readback mismatch, resource-budget excess, residence expiry, timestamp or
reservation regression, socket pressure, short write, feedback loss, and
circuit-breaker activation. Strict service requested without matching platform
or network evidence fails before DAG startup.

Diagnostics report planned and observed item/byte occupancy, residence,
maximum AU, encoder envelope, mux rate, application pacing rate, burst,
enqueue timing, socket pressure, receiver feedback when available, packet
loss, PCR jitter, CPU, RSS, drops, A/V drift, and final exit reason. Userspace
enqueue is never reported as wire completion.

## Acceptance

Temporary TDD may prove product construction, capacity mathematics, exact
serialization, channel enforcement, and global timeline behavior. Every
temporary test source, target, and binary is deleted before delivery.

Final acceptance uses Release builds and the unchanged 2K continuous
120-second source. It covers H.264 and HEVC in both directions, resolution and
codec conversion, CBR and VBR, Windows and RKMPP, and the production DAG.
Machine evidence includes sender and receiver sequence/loss statistics,
application pacing distribution, maximum queue bytes and residence, CPU/RSS,
zero-copy telemetry, PCR timing, output decode, and truthful termination. A
1 Mbps diagnostic cannot replace the 4 Mbps gate, and receiver-buffer changes
cannot substitute for correcting application-generated bursts.
