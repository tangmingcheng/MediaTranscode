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
scheduling it. `MediaTsDatagramEmissionSchedule` then receives:

- the exact materialized TS payload byte count;
- the stream identity and its observed canonical emit cadence;
- `emitOnMaster` and `dispatchOnMaster` from the canonical scheduler;
- exact per-datagram transport overhead from the planned transport.

The schedule computes the observed wire rate for the current stream, combines
it with the last committed observation of the other stream, and selects at
least the rate required to finish the current cursor inside its remaining
canonical access-unit window. There is no fixed headroom or burst constant.
Every datagram receives a fixed master-time deadline; the final datagram must
remain within `dispatchOnMaster` or the session fails closed.

Only a fully emitted access unit commits its stream observation and next
available service time. Prepared datagrams are RAII transactions, so a failed
or cancelled write cannot advance schedule state. PAT, PMT, and PCR use their
existing canonical deadlines plus the planner-owned access-unit window and
never create a second media-time authority. The runtime master clock is sampled
again after every successful transport write; this post-write fact, rather
than the planned release time, commits debt/lateness and the next service time.

## Ownership and Backpressure

The mux retains at most one packet cursor, prepared clock transaction, and
datagram transaction. It does not copy the encoded payload into another queue.
While a cursor is pending, the adapter polls its next deadline before accepting
another access unit, preserving bounded graph backpressure.

UDP and MP2T/RTP use the same schedule. Sinks only serialize/send the supplied
datagram and report success, short write, or socket pressure. They never sleep,
estimate media rate, or alter deadlines. Stop and abort destroy retained RAII
state without waiting on an uninterruptible pacing sleep.

## Diagnostics and Failure Semantics

Diagnostics are based on runtime facts and report datagrams, wire bytes,
immediate/deferred deadlines, cumulative planned wait, actual lateness,
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
