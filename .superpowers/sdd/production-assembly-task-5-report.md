# Production Assembly Task 5 Report

## Scope

Task 5 assembles one protocol-neutral synchronized A/V input segment. RTP and
MPEG-TS retain protocol-specific clock binding at the segment boundary, then
share canonical input, startup coordination, playback-epoch binding, activated
release sequencing, and atomic release extraction. Video-only input keeps its
existing `PacketStartGate` outside this segment.

## Implementation

- RTP input uses one immutable clock snapshot fanout, separate video/audio RTP
  packet clock binders, and an RTP-to-generic source-clock adapter.
- MPEG-TS input uses the selected-program demux clock through one generic
  source-clock fanout and separate initial-locked packet gates.
- Both paths feed the same canonical input and startup chain and expose only
  released video, released audio, and the sequencer `activated` endpoint.
- `MediaRealtimeAvSyncInputSegmentBuilder` owns only shared topology
  orchestration. `MediaRealtimeAvSyncProtocolInputBuilder` owns RTP/MPEG-TS
  variants and adapter endpoints, `MediaRealtimeAvSyncNodeConfigurator` copies
  exact planner options, and one graph support helper owns validated mutation.
  Every graph mutation is sequential and fail-fast.
- The planner owns a dedicated bounded synchronized-packet edge policy. It is
  FIFO and `BlockProducer`; the segment consumes it without deriving policy.
- Source endpoints are validate-only. Missing ports, mismatched types, or
  non-fanout ports fail immediately; the segment never invents an upstream
  protocol interface. The main builder explicitly declares Raw RTP packet and
  MPEG-TS video/audio/clock outputs.
- Compiler sync-group validation accepts only the exact RTP binder, initial
  lock gate, and startup coordinator option consumers.

## TDD Evidence

The initial RTP structure test failed on every missing production node from
clock snapshot through release extraction. The implementation made the RTP
structure GREEN. A later fail-fast RED showed that a missing MPEG-TS source
port was silently created by the segment; the test exited with one failed
expectation. Replacing interface creation with validate-only behavior made the
same test GREEN. Exact edge types, capacities, FIFO ordering, blocking
backpressure, group identifiers, protocol variants, video-only isolation, and
compiler fail-closed behavior are covered.

## Test Results

The focused integration executable selected these five Task 5 cases: RTP
assembly, compiler fail-closed boundary, direct MPEG-TS assembly, missing-port
rejection, and video-only legacy gating.

```powershell
& .\out\build\x64-debug\media_transcode_integration_tests.exe
```

Result: PASS, five focused cases, exit code 0.

```powershell
& .\out\build\x64-debug\media_transcode_planner_tests.exe
```

Result: PASS, exit code 0.

```powershell
& .\out\build\x64-debug\media_transcode_builder_tests.exe
```

Result: PASS, exit code 0.

```powershell
& .\out\build\x64-debug\media_transcode_runtime_tests.exe
```

Result: PASS, exit code 0.

```powershell
& .\out\build\x64-debug\media_transcode_node_tests.exe
```

Result: PASS, exit code 0.

The final VS2026 clean-first rebuild removed 479 artifacts and completed all
480 actions with exit code 0. `/showIncludes` remained absent. The focused five
cases and all four layered executables were rerun after that rebuild and each
returned exit code 0.

The normal unfiltered integration executable still reports six existing
planner expectations in the Opus/scheduled RTP audio timing area. Those
failures predate Task 5 and are not hidden or changed by this work.

## Explicit Boundaries and Risks

- Task 5 validates MPEG-TS through the direct generic segment. A full TS graph
  is still rejected by the existing planner/output capability boundary until
  the Task 7/8 scheduled output assembly exists. The test does not mutate a
  planner product or bypass this boundary.
- The sequencer `activated` endpoint intentionally has no consumer in Task 5.
  Runtime compilation with an A/V sync binding fails closed because the shared
  scheduler/output assembly is incomplete. `buildExecutable` likewise returns
  `Unsupported`; later output tasks must connect the endpoint.
- Protocol binders and gates forward terminal/control input toward canonical
  input, whose production EOF/control contract is not yet completed. Task 5 is
  therefore structural input assembly and does not claim end-to-end executable
  EOF completion.
- The existing Opus/scheduled RTP planner expectation failures remain for a
  separate scoped fix.
