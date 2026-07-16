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
  FIFO and `BlockProducer`; one realtime edge-policy planner produces the full
  policy set, and validation compares the complete structural product without
  duplicating policy decisions.
- Source endpoints are validate-only. Missing ports, mismatched types, or
  non-fanout ports fail immediately; the segment never invents an upstream
  protocol interface. For synchronized assembly only, the main builder
  explicitly declares Raw RTP packet and MPEG-TS video/audio/clock outputs;
  legacy branch builders remain the sole port owners outside that path.
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
compiler fail-closed behavior are covered. Independent mutations of every
previously under-validated synchronized-packet policy field formed the policy
validation RED; exact comparison with the single planner product made all
mutations GREEN. A video-only graph-validation RED exposed duplicate legacy
port ownership, which was removed by limiting explicit source-port ownership
to synchronized assembly.

## Test Results

The permanent selector covers eight Task 5 cases: RTP assembly, compiler
fail-closed boundary, direct MPEG-TS assembly with exact released endpoints,
missing-port rejection, video-only legacy isolation, two synchronized audio
decode-source checks, and replacement of the legacy inert start barrier.

```powershell
& .\out\build\x64-debug\media_transcode_integration_tests.exe --task5-av-sync-input
```

Result: PASS, eight focused cases, exit code 0.

```powershell
& .\out\build\x64-debug\media_transcode_integration_tests.exe
```

Result: FAIL, exit code 1, with exactly three pre-existing
Opus/scheduled-RTP planner expectations:

- `testRawRtpRejectsUnsupportedOpusOutputAndPlansOpusInputToAac`: `opusPlan`.
- `testRawRtpPlansOpusAudioInput`: one-channel request plan.
- `testRawRtpPlansOpusAudioInput`: two-channel request plan.

Both failed plans report `NotInitialized: scheduled RTP packetization does
not publish audio batch timing`. No Task 5 structure, policy, endpoint, or
legacy-isolation expectation failed.

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

The final VS2026 clean-first rebuild removed 480 artifacts and completed the
481-step build with exit code 0. `/showIncludes` remained absent. Every command
above was rerun directly against those clean artifacts; the focused selector
and all four layered executables returned exit code 0, while the unfiltered
integration result remained the explicit three-expectation baseline above.

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
  input, whose production EOF/control contract is not yet completed. This is
  explicit Task 6 RED prerequisite debt: Task 5 adds no EOF/control bypass and
  does not claim end-to-end executable EOF completion.
- The existing Opus/scheduled RTP planner expectation failures remain for a
  separate scoped fix.
