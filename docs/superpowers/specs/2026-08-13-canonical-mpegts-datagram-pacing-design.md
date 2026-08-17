# Canonical MPEG-TS Datagram Emission Design

## Problem

Project MPEG-TS releases each scheduled access unit at `emitOnMaster`. Writing
all of its TS/RTP datagrams immediately creates a burst, while a fixed bitrate,
burst allowance, or lateness threshold cannot describe variable-size encoded
access units. Such constants either reject a valid keyframe or accumulate
unbounded scheduling debt when the real wire rate exceeds the estimate.

The correction must preserve the canonical A/V scheduler, share one MPEG-TS
implementation on Windows and Linux, and keep UDP/RTP sinks transport-only.

## Authority and Contract

`MediaTsMuxPlan` remains the planner-owned protocol contract. Its packet size,
maximum packets per datagram, transport kind, and transport decode lead,
together with planner-resolved output video/audio cadences, are the complete
inputs used to create `MediaTsDatagramEmissionPlan`. The emission product
contains the canonical access-unit window, planner-resolved initial video/audio
service windows, and exact datagram geometry. Video cadence comes from the
resolved output frame rate; audio cadence comes from protocol batch samples
and output sample rate. The initial windows are bounded by the mux stream count
so one first stream cannot consume the shared A/V lead. Runtime does not infer
missing facts. The product has no sample-tuned rate, burst, or lateness value.

Runtime plans, graph options, and decoded plans must reproduce this projection
exactly. Missing, overflowing, or contradictory facts fail before DAG startup.
The runtime cannot choose a policy or fall back to another pacing mode.

## Access-unit Scheduling

`MediaTsMuxSession` materializes one access unit as a TS packet cursor before
scheduling it. It produces one bounded `ScheduledDatagramBatch` DAG payload;
it never owns a socket. `MediaTsDatagramEmissionSchedule` receives:

- the exact materialized TS payload byte count;
- the stream identity and its observed canonical emit cadence;
- `emitOnMaster` and `dispatchOnMaster` from the canonical scheduler;
- exact per-datagram transport overhead from the planned transport.

The schedule computes the observed wire rate for the current stream, combines
it with the last committed observation of the other stream, and selects at
least the rate required to finish the current cursor inside its remaining
canonical access-unit window. There is no fixed headroom or burst constant.
Every media datagram receives an enqueue-not-before time and enqueue deadline;
the final datagram must remain within `dispatchOnMaster` or planning fails
closed before the batch is published. PAT, PMT, and
PCR retain their planner-derived maintenance window when a bounded media cursor
is active. A delayed maintenance write uses its sampled master-time emission,
never a past planned deadline, so the transport timestamp cannot regress.

Only a fully materialized access unit commits its stream observation and next
planned service time. Prepared datagrams are RAII transactions, so a failed
batch publication cannot advance schedule state. PAT, PMT, and PCR use their
existing canonical deadlines plus the planner-owned access-unit window and
never create a second media-time authority.

## Ownership and Backpressure

The mux retains at most one packet cursor, prepared clock transaction, and
unpublished batch. The planner builds a blocking, bounded batch edge; the mux
does not accept another access unit while publication is blocked.

UDP and MP2T/RTP use the same schedule. For MP2T/RTP, a dedicated scheduled
datagram sender node consumes the typed batch and reuses the existing RTP
transport, MP2T packetizer, RTCP schedule, and continuity state. Its private
interruptible deadline executor sends each datagram separately; the DAG never
wakes the mux worker once per datagram and never combines datagrams into a
burst. Platform code is limited to the deadline-wait adapter.

The sender samples time after `send()` returns and records this only as
`actual_enqueue`. A successful UDP send is not wire-completion evidence. A
transport may report actual wire completion only when a platform adapter
provides an authoritative transmit timestamp; otherwise no diagnostic or
validator claims wire completion. Stop and abort interrupt the sender's wait
and destroy retained RAII state.

## Diagnostics and Failure Semantics

Diagnostics are based on runtime facts and report datagrams, wire bytes,
immediate/deferred deadlines, cumulative planned wait, enqueue lateness,
pending/peak bytes, pressure failures, access-unit count, current/maximum
scheduling debt, selected/maximum wire rate, and the final exit reason.

Invalid timing order, exhausted canonical windows, arithmetic overflow, clock
regression, socket pressure, and short writes remain terminal errors. The
implementation adapts to any representable access-unit size and cadence that
can physically complete within the planner-owned window; impossible transport
capacity is reported rather than hidden by fallback or relaxed acceptance.

## Verification

Temporary TDD may prove deadline arithmetic and transactional behavior, but all
test source, target, and binary artifacts must be deleted before delivery.
Final acceptance requires the prescribed VS2026 clean build and equal-spec
Windows/RKMPP real CLI chains using the unchanged 2K 120-second source, codec
and resolution conversion, AAC, separate RTP input with automatic fmtp, and
MPEG-TS over RTP output. Evidence includes receiver RTP sequence statistics,
VLC observation, CPU/RSS, queues, drops, A/V drift, pacing diagnostics, exit
reason, and zero temporary process/script/port residue.
