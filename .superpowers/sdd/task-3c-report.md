# Task 3C Report: Cross-stream SR/CNAME source clock mapping

## Status

`COMPLETE; INDEPENDENT REVIEW PASSED`

Task 3C maps per-stream RTP timestamps into the RTCP NTP source domain and establishes an explicit audio/video clock-group boundary. It does not select a canonical running-time epoch, buffer startup packets, pace output, or rewrite packet PTS/DTS.

## RED

- Pure clock tests first failed because `MediaRtpSourceClockMapper` and `MediaRtpClockGroupValidator` did not exist.
- Graph contract tests then failed because `MediaNodeKind::RtpClockGroup` and the four explicit clock/event ownership edges did not exist.
- Exact deadline tests failed before `MediaRtpClockObservationSchedule` and structured observation/invalidation events existed.
- Duplicate-SR regression failed because an identical SR was incorrectly treated as a zero-slope update.
- Independent CNAME-age regression first failed because CNAME freshness was incorrectly coupled to SR freshness.
- Cross-port ordering regression failed at `test_node.cpp:634/641`: after `ClockInvalidation{2}` was consumed, queued generation-1 clock evidence could still publish `Locked`.
- The explicit sender-rate bound and `INT64_MAX` deadline saturation cases were review-driven hardening. The rate-bound case did not have a standalone pre-implementation RED; this is retained here rather than overstating TDD evidence.
- Planner-owned sender residual policy first failed to compile because `maximumSenderClockResidualNs` did not exist.
- Sustained-event fairness first failed because one `video_event` consumption returned early, leaving audio event and both clock queues unserved.
- Deadline boundary regression showed schedule transitions at equality while mapper/validator remained valid until `+1ns`.
- Ordinary RTP discontinuity regression showed same-generation clock evidence could relock immediately because only explicit clock invalidations carried a generation floor.
- Empty CNAME producer regression showed an identity failure could emit an invalidation without first advancing tracker generation.
- Negative-time regressions showed mapper evidence and group observation events needed the same explicit non-negative contract as the deadline schedule.

## Implemented contract

- `MediaRtpSourceClockMapper` uses the Task 1 strong RTP32 unwrapper and explicit stream clock rate.
- Real SR RTP/NTP pairs establish source-domain calibration. NTP regression, non-positive slope, excessive residual, SSRC/CNAME mismatch, and generation mismatch invalidate the mapping.
- Accepted SR updates refine rate while anchoring the new estimate to the previously published source time, avoiding a source-time jump.
- Sender clock-rate error is bounded by the planner-owned `maximumSenderClockRateErrorPpm`; downstream code has no borrowed threshold or fallback.
- Sender clock residual is independently bounded by planner-owned `maximumSenderClockResidualNs`; it does not borrow the recovery hard-discontinuity threshold.
- Identical duplicate SRs refresh evidence age without changing calibration.
- SR age transitions from locked to bounded degraded extrapolation, then expiry/reacquisition; steady time is used only for evidence age.
- `MediaRtpClockGroupValidator` requires explicit observed-media, sender-report, and CNAME SSRC equality, non-empty exact audio/video CNAME octet equality, current per-stream generations, and bounded SR source-time skew.
- SR and CNAME freshness are tracked independently per stream. Deadline arithmetic saturates at `INT64_MAX`, and negative observation times or non-positive receive limits are explicitly rejected.
- Deadline equality only schedules the next bounded wake; state advances on the first representable instant beyond the threshold, matching mapper and validator age semantics. A saturated `INT64_MAX` deadline never fabricates an unrepresentable expiry.
- `MediaRtpClockGroupNode` owns both Raw RTP inputs' clock/event ports and publishes immutable acquiring/locked/degraded/reacquire snapshots with both stream calibrations.
- Each input stream retains the minimum generation from every consumed `ClockInvalidation`; later-consumed evidence below that floor is rejected, while valid current-generation evidence can reacquire the group.
- Queue/SSRC discontinuities carry the tracker generation explicitly. Raw ingress increments the tracker once per continuity-loss result, resets the observation schedule immediately, and the group applies the same generation floor before consuming queued clock evidence.
- Tracker identity failures that invalidate clock evidence advance generation before Raw publishes invalidation; mapper evidence and group observation events reject negative observation times at their own boundaries.
- Each process turn consumes at most one item from every event-first port. This bounded batch preserves invalidation-before-clock ordering for inputs queued at turn start without allowing a continuously active video event port to starve audio events or either clock port.
- `RawRtpInputNode` caps its blocking receive wait to the earliest planner read timeout, reorder deadline, SR degraded deadline, or extrapolation expiry deadline. Clock identity failures become structured group invalidation events.
- Raw RTP packet timestamps and time bases remain unchanged on the existing packet edges. No SDP, wall-clock, arrival-time, or first-packet-zero fallback was introduced.

## Tests

Command:

```powershell
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -L deterministic
```

Result: passed, 6/6 tests, 0 failures, 3.35 seconds.

Command:

```powershell
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -L integration
```

Result: passed, 1/1 test, 0 failures, 75.29 seconds.

Coverage includes matching CNAME mapping, RTP wrap, bounded SR jitter/rate refinement, planner-owned sender-rate and residual rejection, duplicate SR, NTP regression, invalid slope, independent SR/CNAME timeout/degraded/expiry, SSRC/CNAME change, generation isolation, cross-port stale-generation rejection and reacquisition, sustained-event fairness, missing evidence, mismatched CNAME, explicit no-fallback behavior, long transport-wait deadline capping, negative observation-time rejection, `INT64_MAX` deadline saturation, structured invalidation, and graph port ownership.

## Remaining concerns

- The `clock_group` output intentionally has no packet join or startup consumer until the later canonical/startup tasks.
- Source calibration uses nanosecond NTP representation; sub-nanosecond NTP fraction precision is not representable in `MediaRunningTime`.
- The application-owned UDP transport remains Windows-specific as recorded in Task 3A.

## Independent review

Fresh reviewer v3 approved Task 3C with Critical 0, Important 0, and Minor 0. Its independent rerun passed deterministic 6/6 and integration 1/1.
