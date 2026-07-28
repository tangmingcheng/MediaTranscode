# Realtime Startup Exactly-Once and Memory-Bound Plan

**Goal:** Ensure RTP and MPEG-TS share one realtime A/V path that never republishes a canonical access unit within a playback generation and does not retain hundreds of megabytes of raw video frames.

**Architecture:** Keep protocol adapters downstream of the shared canonical A/V startup and scheduler path. The startup coordinator owns per-generation source-sequence acceptance, while a dedicated realtime queue-capacity planner selects one bounded queue product consumed by startup, component-bound, edge-policy, and runtime validation. Runtime nodes do not deduplicate, clamp, or fall back.

**Constraints:**

- All fixes apply to the shared RTP/MPEG-TS A/V DAG before protocol output.
- The planner remains the only owner of queue policy.
- Hardware planning and the selected zero-copy chain remain unchanged.
- Every production change begins with a focused failing test.
- Every build uses `--clean-first`, `--parallel 4`, and no `/showIncludes`.

## Task 1: Enforce per-generation exactly-once canonical input

**Files:**

- Modify: `tests/unit/av_startup_coordinator_tests.cpp`
- Modify: `src/internal/graph/sync/MediaAvStartupCoordinator.h`
- Modify: `src/internal/graph/sync/MediaAvStartupCoordinator.cpp`

1. Add a Running-state regression test that completes the initial release and then resubmits a video access unit with the same generation and source sequence at a later observation time. Assert failure with `StartupInvalidTransition`.
2. Add the equivalent audio assertion and a lower-sequence assertion, while proving the next strictly increasing sequence remains `PassThrough`.
3. Run the focused coordinator test and record RED: current Running state returns `PassThrough` before sequence validation.
4. Add one persistent last-accepted sequence per stream to the coordinator. Reset both only when advancing/resetting the playback generation. Validate locked same-generation units before Running pass-through and update the ledger only after accepting the unit.
5. Run the focused test and the production release tests.

## Task 2: Bound realtime in-flight media residency in the planner

**Files:**

- Modify: `tests/unit/test_planner.cpp`
- Add: `src/internal/graph/planner/realtime/MediaRealtimeQueueCapacityPlanner.h`
- Add: `src/internal/graph/planner/realtime/MediaRealtimeQueueCapacityPlanner.cpp`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.cpp`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeEdgePolicyPlanner.cpp`

1. Add a planner regression test with requested capacities `metadata=1`, `packet=256`, `frame=128`, and `mux=256`. Assert the selected realtime product is `1/32/8/32`.
2. Run the focused planner test and record RED: current planning preserves all oversized requested values.
3. Add a dedicated queue-capacity planner and have the realtime transcode planner store only its selected product. The edge-policy planner maps that product without further hidden clamping.
4. Run planner validation and graph-assembly tests. Verify startup limits, component bounds, runtime validation, the selected hardware chain, and both RTP/MPEG-TS topologies consume the same selected capacities.

## Task 3: Remove the redundant local `--format` argument

**Files:**

- Modify: `tests/unit/test_planner.cpp`
- Modify: `src/internal/graph/planner/local/MediaLocalFileOutputPlanner.h`
- Modify: `src/internal/graph/planner/local/MediaLocalFileOutputPlanner.cpp`
- Modify: `src/internal/graph/builder/local/LocalFileTranscodeGraphBuilder.cpp`
- Modify: `src/internal/graph/builder/local/LocalFileTranscodeGraphBuilder.h`
- Modify: `tools/local_video_cli/main.cpp`

1. Add a local-output planner test proving `out.mp4` resolves to `mp4` and an extensionless output is rejected before graph construction.
2. Run the focused planner test and record RED.
3. Make the local output planner derive the muxer from the output URL extension. Remove `outputFormat` from local builder options and remove `--format` from the local CLI accepted arguments. Keep explicit planner-owned MPEG-TS output format unchanged.
4. Run the local planner/builder tests and a real local-file transcode without `--format`.

## Task 4: Verification and delivery

1. Clean-first build only the focused unit targets and both video CLIs; do not build or run the high-memory production runtime integration executable.
2. Run focused unit suites, then real RTP and MPEG-TS input/output tests with independent FFmpeg receivers.
3. For MPEG-TS, sample Working Set and Private Bytes after warm-up and verify the raw-frame high-water plateau is materially below the prior approximately 362 MB level.
4. Review the complete diff for shared-DAG reuse, planner ownership, no legacy fallback, CRLF, and unrelated dirty-file exclusion.
5. Commit and push the existing A/V-sync branch, update PR #25, and ask a fresh review agent to assess the PR.
