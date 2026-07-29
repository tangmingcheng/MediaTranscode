# Realtime Cross-Layout Design

## Goal

Allow every supported realtime input to select either separate RTP or MPEG-TS
over UDP output:

| Input | Separate RTP output | MPEG-TS UDP output |
| --- | --- | --- |
| URL/RTSP session | Supported | Supported |
| Separate RTP streams | Supported | Supported |
| MPEG-TS over UDP | Supported | Supported |

Each realtime task selects exactly one output layout. Simultaneous RTP and
MPEG-TS fan-out is outside this change.

## Non-Negotiable A/V Quality

The existing separate-RTP-to-separate-RTP and MPEG-TS-to-MPEG-TS paths are the
quality baseline. This change must preserve their current startup lock,
canonical scheduling, audio drift correction, video recovery, long-running
drift telemetry, and subjective VLC playback quality.

All six input/output combinations use the same canonical A/V scheduler and
protocol-neutral running-time domain. No compatibility path, packet-arrival
clock fallback, FFmpeg remux bypass, second scheduler, or output-owned pacing
authority is permitted. Missing or invalid clock facts fail planning or cause
the existing explicit reacquisition/failure behavior.

## Architecture

Replace the paired `MediaAvSyncTopology` decision with two orthogonal planner
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
- MPEG-TS uses the existing project MPEG-TS mux, PCR/PTS/DTS derivation,
  datagram sink, and planner-owned transport lead.

The planner resolves all codec, packet layout, time-base, duration, queue,
transport, and output clock facts before graph construction. Builders and
runtime nodes consume the selected variants and fail on missing facts; they do
not choose protocols or supply defaults.

## Component Boundaries

### Request and planning

The request validator accepts the complete 3-by-2 matrix while preserving
input-specific and output-specific validation. Output URL, RTP endpoint, SDP,
packet size, MPEG-TS PCR policy, RTSP transport, RTP metadata, and queue
requirements remain explicit.

The A/V planner creates the input clock plan independently from the output
adapter. MPEG-TS resolved output facts are required whenever the output layout
is MPEG-TS, regardless of input. Scheduled RTP packetization facts are required
whenever the output layout is separate RTP, regardless of input.

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
scheduler. The existing scheduled RTP and project MPEG-TS segments are reused.
Input type cannot affect protocol output construction.

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
- Runtime protocol nodes cannot fall back to local clocks, guessed sample
  rates, default payload facts, FFmpeg-owned pacing, or alternate muxers.
- Existing generation transition and reacquisition rules remain authoritative
  for all layouts.

## CLI and Documentation

The realtime CLI retains the current `--input-type`, `--input-layout`, and
`--output-layout` interface. It accepts all valid independent combinations:

- `--output-layout separate` requires RTP host/base port, packet size, and SDP.
- `--output-layout mpegts` requires one explicit `udp://host:port` output.
- Input-specific arguments remain required only by their selected input.

README examples cover both new cross-protocol directions and URL/RTSP to
MPEG-TS. The acceptance report records the exact real CLI, FFmpeg, and VLC
commands and results; build commands are excluded as required by the project.

## Verification

Perform the required clean-first x64 Debug rebuild through the project VS2026
skill, then validate the production DAG only with real processes and media.
Run each of the six matrix combinations for 2 minutes.

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
even if all new cross-layout paths produce decodable output.

## Delivery

All code, documentation, quality-score updates, review fixes, and acceptance
evidence are committed and pushed to `codex/realtime-cross-layout`. After the
implementation self-review, a PR is created and a fresh review agent evaluates
the entire PR against this design and the repository rules. The PR is approved
only when that review has no blocking findings and all six real-media paths
meet the acceptance contract.
