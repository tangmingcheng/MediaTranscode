# RTP Video FMTP Auto-Detection Design

## Goal

Allow separate raw RTP H.264 and HEVC video inputs to omit
`--video-rtp-fmtp`. Before graph construction, the engine observes validated
in-band parameter sets, lets the planner produce canonical FMTP, and transfers
the same bound RTP/RTCP transport and buffered datagrams into the runtime.

AAC keeps its required manual FMTP. Codec, payload type, clock rate, and audio
channels remain explicit caller facts.

## Architecture

Raw RTP preflight has two explicit modes. Manual mode performs no network
probe and uses the supplied FMTP. Auto-detection mode is available only for
H.264 and HEVC video and executes a planner-produced probe plan containing the
endpoint, codec identity, limits, and supported packetization policy.

The probe returns typed H.264 or HEVC signaling facts rather than an arbitrary
string. The planner validates those facts against the request, serializes
canonical FMTP, and runs the existing codec descriptor and depacketizer
validation. Runtime nodes receive only a complete planned product.

The probe owns one RAII UDP transport throughout preflight and buffers original
RTP/RTCP datagrams. A raw-RTP prepared-input buffer transfers that transport
and queue to the video `RawRtpInputNode`. The node processes the queued
datagrams through the normal parser, reorder, RTCP clock, and depacketizer path
before receiving more data from the same sockets.

## Detection Contract

- `open-timeout-ms` is the total target deadline for controllable preflight
  work. FFmpeg/driver capability calls have no cancellation API; each is
  checked immediately after return and an overrun fails preflight.
- `analyze-duration-us` is the maximum evidence interval after the first
  matching RTP packet. Detection completes as soon as one complete,
  unambiguous parameter-set family has been observed.
- `probe-size` bounds total buffered datagram bytes.
- `read-timeout-ms` bounds each cancellable receive wait.
- Only RTP v2 packets with the planned payload type contribute codec evidence.
- An SSRC change before completion starts a new evidence epoch and discards the
  prior epoch's codec facts and buffered datagrams.
- H.264 requires one unique SPS and PPS. The planner derives
  `profile-level-id` from the SPS and emits `packetization-mode=1`.
- HEVC requires one unique VPS, SPS, and PPS.
- Repeated identical parameter sets observed before detection completes are
  accepted. A second different set in that evidence interval,
  malformed payload, unsupported packetization, missing evidence, limit
  exhaustion, or identity conflict fails preflight.

H.264 single NAL, STAP-A, and FU-A and HEVC single NAL, AP, and FU parsing are
shared between detection and depacketization. Interleaved H.264 packetization,
HEVC PACI, and DONL remain unsupported.

## Interfaces

- `MediaRealtimeRtpInputMetadata::fmtp` becomes
  `std::optional<std::string>`.
- `MediaRtpVideoSignalingFacts` is a variant of typed H.264 and HEVC facts and
  includes observed RTP identity and bounded diagnostic counters.
- `MediaPreparedRealtimeInputKind` gains `RawRtp`.
- `MediaRawRtpPreparedInputBuffer` exclusively owns the prepared transport and
  buffered datagrams until runtime binding consumes it.
- Automatic raw RTP node plans require an exact prepared binding. Manual plans
  explicitly require node-owned transport creation. Neither path falls back to
  the other.

## Validation and Delivery

No CI, CTest, unit, integration, or acceptance test target is added. Validation
uses the clean-first VS2026 build plus real FFmpeg/VLC H.264 A/V, HEVC video,
manual-FMTP regression, and strict failure gates. Temporary malformed-stream
senders may live only under `out/` and must be removed before delivery.

All changed text is UTF-8 with CRLF. Work is committed and pushed on
`codex/rtp-fmtp-autodetect`, followed by a ready PR and independent agent
review. Remaining risks are AAC's authoritative signaling requirement, the
inability of bare RTP to discover codec/PT/clock rate, and runtime parameter-set
  changes, and the inability to interrupt a blocked FFmpeg/driver capability
  call at the exact startup deadline.
