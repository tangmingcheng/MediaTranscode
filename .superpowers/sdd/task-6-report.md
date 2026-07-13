# Task 6 Report: Atomic common A/V startup

## Outcome

Implemented a protocol-neutral startup coordinator and runtime node without wiring it into the production RTP or MPEG-TS graph. Production wiring and removal of the legacy barrier remain Task 12 responsibilities.

## Delivered

- Explicit startup state machine covering acquisition, priming, armed, release, running, failure, stop, abort, reset, and reacquisition.
- Planner-owned startup thresholds, explicit per-stream maximum unit size, checked unit/byte capacities, stream identities, maximum gap, and degraded-clock denial. Same-generation media and an explicit master-clock channel are structural coordinator/node invariants rather than configurable policy fields.
- Three-input runtime node with one pending head per audio, video, and clock input. Arbitration uses a global event-time watermark; equal timestamps use audio, video, then clock.
- Bounded per-stream input ordering, old-generation rejection, higher-generation purge, and reacquisition gate closure.
- Keyframe-qualified common-window selection scans later video keyframes and audio candidates, enforces bilateral skew and continuous maximum-gap-bounded preroll coverage, and emits a sample-exact leading-audio trim directive using checked time/sample arithmetic.
- A single immutable release buffer containing the playback epoch and both initial stream selections. No two-channel partial release is possible.
- Explicit clock ticks drive startup and keyframe timeouts. The coordinator does not read a system or steady clock.
- Clock and terminal snapshot barriers freeze media and clock intake without draining future channel capacity. Continuous post-snapshot clock ticks cannot starve queued EOF, while media already in the terminal snapshot can still complete an atomic release first.
- Startup buffering is split into three internal responsibilities: a `list`-owned arrival store with a signed presentation-key map and persistent per-candidate coverage frontier, an immutable presentation snapshot, and a monotonic window selector. Append incrementally advances reachable intervals and retains unreachable intervals in a signed ordered index so a later bridge can connect them. Coverage never uses units that the selected arrival prefix will purge, supports negative canonical PTS without a zero sentinel, and preserves release arrival order.
- Selection work is explicitly bounded at the planner and plan-validator capacity of 256. Each candidate/later-unit pair is visited once on append and at most once again when an ordered pending interval becomes reachable, so two full 256-unit streams perform at most `2 * 256^2 = 131,072` cumulative coverage operations. Ordered-index insertion and removal have the same 131,072 mutation bound and `O(C^2 log C)` comparison complexity. The 512-submit reverse-chain regression forces capacity-scale late-bridge promotion in both streams and observes 65,788 cumulative coverage operations and 1,016 ordered mutations, while the last selector attempt visits at most 768 candidates. Checked interval end/expanded-end arithmetic completes before Store mutation.
- Errors remain typed through the state machine and coordinator and are converted to runtime `ErrorInfo` only at the node boundary. Packet footprint accounting includes FFmpeg packet side data and fails closed for missing main/side-data pointers, malformed metadata, and checked-addition overflow.
- Runtime factory, node-kind diagnostics, and focused planner/node tests.

## TDD evidence

- Pure coordinator RED: missing coordinator interface.
- Runtime node RED: missing coordinator node interface.
- Planner contract RED: missing startup capacity fields (`C2039`).
- Review RED: missing bounded-selection diagnostics (`C2039`), followed by behavioral RED coverage for terminal-clock starvation, negative PTS, arrival-suffix isolation, and malformed FFmpeg packet metadata. The persistent-coverage review RED then failed on the missing clearly separated last-attempt and cumulative-work fields (`C2039`).
- Each RED was followed by a focused GREEN build and test run.

## Verification

Commands executed:

```powershell
ctest --test-dir out/build/x64-debug -L deterministic --output-on-failure
```

Result: 6/6 passed in 4.06 seconds (core, planner, builder, runtime, node, tier contract). The node tier completed in 0.18 seconds.

```powershell
ctest --test-dir out/build/x64-debug -L integration --output-on-failure
```

Result: 3/3 passed in 142.80 seconds. Realtime integration passed in 81.68 seconds; MPEG-TS integration passed in 15.45 seconds; crafted UDP fault integration passed in 45.67 seconds.

The final verification followed a complete clean rebuild: 276 stale outputs were removed and all 277 targets rebuilt successfully. An immediate repeat build reported `ninja: no work to do`, confirming the clean rebuild was complete. This prevented stale class-layout symbols from masking runtime results.

## Deferred integration boundary

- Raw RTP packets still lack the complete common canonical media envelope required by this node. The coordinator fails closed instead of interpreting RTP clock-group data.
- Task 12 must serialize this planner contract, connect the common canonical media and master-clock inputs, apply the audio trim directive at the unique decoded-audio execution point, replace the synchronized-path legacy barrier, and retain the video-only packet gate.
- Real RTP and MPEG-TS playback validation is therefore not claimed by Task 6 alone.
