# Exact Canonical Audio Sample Lineage

## Problem

Synchronized audio currently reconstructs every decoded input interval by
independently quantizing canonical nanosecond presentation and end times.
Sample periods such as `1024 / 44100` seconds are not exactly representable as
integer nanoseconds. Repeated independent quantization can therefore create a
synthetic one-sample gap even when the upstream access units are continuous.

The audio interval accumulator must remain strict. It must not accept gaps,
overlaps, generation changes, or sample-rate changes.

## Selected design

Add a deep `MediaCanonicalAudioSourceTimeline` module at the canonical-input
seam. The module accepts planner-owned sample rate and samples-per-access-unit,
the locked source generation and sequence, and the mapped canonical
presentation time.

For the first access unit of a generation, the timeline:

1. quantizes the mapped canonical presentation exactly once to establish the
   canonical source sample index;
2. returns the exact half-open sample interval `[begin, begin + sampleCount)`.

For every later access unit in the same locked generation, `begin` is the
previous interval's `end` and `end` is derived with checked addition of the
explicit sample count. Periodic protocol clock-evidence refresh may refine the
canonical nanosecond mapping, but it must not re-anchor an established sample
cursor.

The exact interval is attached to canonical audio media and survives startup
release into synchronized audio decode. `AudioDecodeNode` consumes that
interval directly and no longer reconstructs source lineage from nanoseconds.
Canonical nanoseconds remain the A/V clock and drift domain; sample intervals
remain the audio payload-lineage domain.

## Contracts

- Only canonical input creates source audio sample intervals.
- Planner products provide sample rate and access-unit sample count.
- Video canonical media carries no audio interval.
- Canonical audio media requires one valid interval matching its generation.
- Missing locked protocol timing, non-consecutive sequence, overflow, rate
  change, or generation change fails closed.
- A real protocol timestamp gap, overlap, or discontinuity is detected by the
  upstream protocol clock authority and arrives as non-locked readiness or a
  generation change; canonical input never repairs it.
- Lifecycle stop and abort clear the timeline.
- No downstream node may synthesize, clamp, or repair an interval.
- No `+/-1` tolerance, fallback, or timestamp rewrite is permitted.
- A future automatic reacquisition flow may translate the typed discontinuity
  into a generation transition; this change does not add that fallback.

## Rejected designs

- Accumulating sample counts inside `AudioDecodeNode` makes a downstream codec
  node a timing authority and hides real source discontinuities.
- Allowing a one-sample tolerance weakens lineage proof and silently changes
  media identity.
- Shortening the production runtime test or changing its assertion would hide
  the long-running failure.

## Verification

Deterministic tests must prove:

- a non-sample-aligned canonical/RTCP anchor followed by hundreds of
  `1024 @ 44100` access units remains exactly contiguous;
- a typed upstream source discontinuity or generation change fails;
- sequence, rate/generation mismatch, missing timing, and arithmetic overflow
  fail;
- stop and abort reset the timeline;
- released audio delivers the exact interval unchanged to decode;
- existing strict accumulator gap and overlap tests remain unchanged.

Final verification requires a VS2026 clean-first build, safe local core,
planner, node, and RTP-output tests, then all four PR CI tiers. The local
high-memory production runtime executable remains prohibited.
