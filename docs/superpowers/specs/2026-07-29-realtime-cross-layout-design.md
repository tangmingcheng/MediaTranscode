# Realtime Cross-Layout Design

## Goal

Allow every supported realtime input to select separate elementary-stream RTP,
MPEG-TS over UDP, or MPEG-TS over RTP output:

| Input | Separate RTP output | MPEG-TS UDP output | MPEG-TS RTP output |
| --- | --- | --- | --- |
| URL/RTSP session | Supported | Supported | Supported |
| Separate RTP streams | Supported | Supported | Supported |
| MPEG-TS over UDP | Supported | Supported | Supported |

Each realtime task selects exactly one output encapsulation and transport.
Simultaneous output fan-out is outside this change.

## Non-Negotiable A/V Quality

The existing separate-RTP-to-separate-RTP and MPEG-TS-to-MPEG-TS paths are the
quality baseline. This change must preserve their current startup lock,
canonical scheduling, audio drift correction, video recovery, long-running
drift telemetry, and subjective VLC playback quality.

All nine input/output combinations use the same canonical A/V scheduler and
protocol-neutral running-time domain. No compatibility path, packet-arrival
clock fallback, FFmpeg remux bypass, second scheduler, or output-owned pacing
authority is permitted. Missing or invalid clock facts fail planning or cause
the existing explicit reacquisition/failure behavior.

## Architecture

Replace the paired legacy topology decision with two orthogonal planner
decisions:

1. An input clock plan selected only from the prepared input.
2. An output adapter plan selected only from the requested output layout.

The production data flow is:

```text
prepared input
  -> protocol input clock and canonical timestamp mapping
  -> shared startup, drift, recovery, and output scheduler
  -> exactly one planned protocol output adapter
```

The input clock plan is a closed variant:

- Separate RTP uses RTP timestamps, RTCP sender reports, and common CNAME.
- MPEG-TS uses the selected program, PCR, PTS/DTS, continuity evidence, and the
  existing generation model.
- URL/RTSP uses prepared stream time bases and demux PTS/DTS. It locks from the
  first valid common audio/video source window and explicitly rejects missing,
  regressing, or discontinuous timestamps. Packet arrival time is not accepted
  as clock evidence.

The output plan is a closed variant:

- Separate RTP uses the existing scheduled RTP packetizers, common NTP epoch,
  sender reports, CNAME, SDP publication, and planner-owned transport facts.
- MPEG-TS over UDP uses the existing project MPEG-TS mux, PCR/PTS/DTS
  derivation, UDP datagram sink, and planner-owned transport lead.
- MPEG-TS over RTP uses the same project MPEG-TS mux and output clocks, then
  wraps complete 188-byte transport packets in one RTP/AVP MP2T session. It
  uses static payload type 33, a 90 kHz RTP clock, an RTP/RTCP port pair,
  sender reports, CNAME, planner-owned SSRC/base timestamp, and SDP.

The MPEG-TS RTP sender is a transport adapter after the project mux. It does
not remux media, derive PCR/PTS/DTS, reschedule access units, or create a
second pacing authority. Its RTP timestamp is derived from the canonical
scheduled emission time on the planned 90 kHz clock. Each RTP payload contains
only complete MPEG-TS packets; the planner derives the payload batch limit
from the explicit maximum RTP datagram size after subtracting the 12-byte RTP
header, bounded to the project mux limit of one through seven TS packets.

The planner resolves all codec, packet layout, time-base, duration, queue,
transport, and output clock facts before graph construction. Builders and
runtime nodes consume the selected variants and fail on missing facts; they do
not choose protocols or supply defaults.

## Component Boundaries

### Request and planning

The request validator accepts the complete 3-by-3 matrix while preserving
input-specific and output-specific validation. Output URL, RTP endpoint, SDP,
packet size, MPEG-TS PCR policy, RTSP transport, RTP metadata, and queue
requirements remain explicit.

The A/V planner creates the input clock plan independently from the output
adapter. MPEG-TS resolved output facts are required whenever the output
encapsulation is MPEG-TS, regardless of input or UDP/RTP transport. Scheduled
elementary-stream RTP packetization facts are required only for separate RTP
output. MPEG-TS RTP additionally requires the fixed MP2T protocol identity,
RTP/RTCP endpoints, SSRC, base timestamp, CNAME, sender-report interval,
maximum datagram size, and SDP identity.

### Canonical input

Existing RTP and MPEG-TS clock adapters remain the only implementations for
their protocols. Add one focused demux timestamp clock adapter for URL/RTSP
input; it maps validated prepared-stream timestamps into the canonical
running-time domain and publishes the same clock-state contract consumed by
startup and scheduling.

Input-specific clock evidence and duration derivation remain separate. Output
selection cannot change input epoch, duration, discontinuity, or generation
semantics.

### Output assembly

Graph assembly branches only on the planned output adapter after the common A/V
scheduler. The existing scheduled separate-RTP and project MPEG-TS segments are
reused. MPEG-TS transport then selects the UDP sink or the focused MP2T RTP
sender. Input type cannot affect protocol output construction.

Video-only requests use the same independent input/output classification but do
not instantiate the A/V drift controller. They still require planner-owned
timestamps, codec layouts, and output transport facts.

## Error Handling

- Unsupported input codecs, MPEG-TS output codec/layout contracts, missing RTP
  sender reports, mismatched CNAME, invalid PCR, missing demux timestamps, and
  timestamp discontinuities remain explicit failures.
- The planner rejects incomplete URL/RTSP stream time bases and any requested
  MPEG-TS output that cannot resolve H.264 plus AAC-LC 48 kHz stereo according
  to the existing project mux contract.
- MPEG-TS RTP rejects non-RTP endpoints, invalid or colliding RTP/RTCP ports,
  absent SDP identity, datagram limits that cannot carry one complete
  188-byte TS packet plus the RTP header, and any payload type or clock rate
  other than the confirmed MP2T values 33 and 90 kHz.
- Runtime protocol nodes cannot fall back to local clocks, guessed sample
  rates, default payload facts, FFmpeg-owned pacing, or alternate muxers.
- Existing generation transition and reacquisition rules remain authoritative
  for all layouts.

## CLI and Documentation

The realtime CLI retains `--input-type` and `--input-layout`, and makes output
encapsulation and transport explicit:

- `--output-layout separate --output-transport rtp` requires RTP host/base
  port, maximum datagram size, and SDP.
- `--output-layout mpegts --output-transport udp` requires one explicit
  `udp://host:port` output.
- `--output-layout mpegts --output-transport rtp` requires RTP host/base port,
  maximum datagram size, and SDP. The planner owns the paired RTCP port,
  payload type 33, 90 kHz clock, SSRC, base timestamp, CNAME, and sender-report
  cadence.
- `--output-layout separate --output-transport udp` is rejected.
- Input-specific arguments remain required only by their selected input.

README examples cover cross-layout input/output, MPEG-TS over UDP, and
MPEG-TS over RTP. The generated MP2T SDP contains one `RTP/AVP 33` media
description with `MP2T/90000` and can be opened directly by VLC and FFmpeg.
The acceptance report records the exact real CLI, FFmpeg, and VLC commands
and results; build commands are excluded as required by the project.

## Verification

Perform the required clean-first x64 Debug rebuild through the project VS2026
skill, then validate the production DAG only with real processes and media.
Run each of the nine matrix combinations for 2 minutes.

For every path:

- Use the real realtime CLI and a real FFmpeg sender or source.
- Receive with FFmpeg/ffprobe as appropriate and observe playback in VLC.
- Confirm both audio and video start, remain subjectively synchronized, and
  show no worsening versus the current same-layout baseline.
- Record worker errors, drops, duplicates, decode errors, progress stalls,
  generation changes, discontinuities, A/V skew/drift telemetry, CPU, and
  process memory trend.
- Require bounded cleanup of CLI, FFmpeg, and VLC processes.

The two existing same-layout paths are explicit regression gates. Any degraded
startup, drift, telemetry, stability, or subjective playback fails the change
even if all new cross-layout and MPEG-TS RTP paths produce decodable output.

## Delivery

All code, documentation, quality-score updates, review fixes, and acceptance
evidence are committed and pushed to `codex/realtime-cross-layout`. After the
implementation self-review, a PR is created and a fresh review agent evaluates
the entire PR against this design and the repository rules. The PR is approved
only when that review has no blocking findings and all nine real-media paths
meet the acceptance contract.
