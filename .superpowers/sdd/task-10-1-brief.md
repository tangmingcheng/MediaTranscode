# Task 10.1 - Shared NTP, RTP clock mapping, and RTCP serialization

## Scope

- Add deterministic strong types for extended/wire NTP and RTP timestamps.
- Add one immutable shared NTP epoch mapping master running time to NTP time.
- Add one immutable per-stream RTP output clock mapper using the planner's
  clock rate, base timestamp, and common master origin.
- Add a pure RTCP serializer for compound SR plus SDES CNAME and BYE packets.
- Add a deterministic aligned sender-report schedule; serialization, schedule,
  transport, and node lifecycle remain separate responsibilities.

## Required contracts

- All constructors/factories require complete parameters; no default values,
  first-packet rebasing, wall-clock fallback, timestamp repair, or sleep.
- NTP capture reads wall time only at explicit epoch creation. Mapping never
  reads system time again and never controls pacing.
- RTP mapping uses presentation master time, checked arithmetic, explicit
  clock rate, and modulo 2^32 only at the wire boundary.
- SR NTP and RTP fields must be produced from the same master instant.
- Compound RTCP is network byte order and validates SSRC, CNAME, counters,
  packet lengths, alignment, interval, and deadline progression.
- Late schedule advancement skips missed intervals without burst catch-up.

## TDD matrix

- Unix/NTP offset, fraction boundaries, era wrap, negative time, and overflow.
- Video 90 kHz and audio clock mapping, base timestamp zero, wrap, exact
  boundaries, negative delta, invalid rate, and overflow.
- Exact SR+SDES bytes, common CNAME, packet/octet counters, RTP/NTP
  correspondence, invalid CNAME, and BYE.
- Initial SR deadline, exact interval boundary, notification before deadline,
  late skip without burst, maximum lateness failure, and generation reset.

## Boundary

- Task 10.1 is pure protocol/time code plus tests.
- Task 10.2 owns custom AVIO, FFmpeg `skip_rtcp`, packet counters, and scheduled
  RTP mux lifecycle.
- Task 10.3 owns typed output descriptions and SDP serialization.
- Task 12 alone replaces the production RTP graph and removes legacy pacing,
  startup delay, and timestamp normalization authorities.
