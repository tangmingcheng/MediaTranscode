# Task 4.5A Clock Projection Report

## Result

- Added immutable optional packet source timing at `FFmpegPacketBuffer` construction. Generic packet producers now pass `std::nullopt` explicitly and raw `AVPacket` timestamps remain unchanged.
- Added bounded, ordered evidence snapshots by value on the MPEG-TS timeline and input session.
- Added `MediaTsClockProjection`, which replays selected-program evidence strictly by byte offset, supports preflight plus incremental replay, stores immutable calibration/generation checkpoints, and answers packet-position rollback from history without moving the tracker backward.
- Projection replay is transactional. Identity mismatch, unordered evidence, tracker rejection, and capacity exhaustion publish no partial state.
- Video/audio timestamp mapper state and the dedicated MPEG-TS demux node remain Task 4.5B scope.

## TDD Evidence

RED 1 failed because `MediaPacketSourceTiming.h` did not exist. RED 2 failed because `MediaTsClockProjection.h` did not exist. RED 3 failed because `MediaTsInputSession::evidenceSnapshotAfter()` did not exist.

Final verification:

```powershell
cmake --build out/build/x64-debug --config Debug --clean-first --target media_transcode_node_tests media_transcode_runtime_tests
cmake --build out/build/x64-debug --config Debug --target media_transcode_node_tests media_transcode_runtime_tests
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -R "media_transcode_(node|runtime)_tests"
```

- Clean-first rebuild removed 256 outputs and rebuilt both targets successfully.
- Final focused build succeeded.
- Focused CTest passed 2/2 with zero failures.

## Review

- FFmpeg read-ahead cannot reorder clock observation: only ordered raw evidence advances the tracker.
- Duplicate evidence from incremental snapshots is ignored only after the incoming range itself is proven strictly ordered.
- A journal beginning after an earlier discontinuity seeds the tracker from the retained evidence generation.
- Projection retention uses the planner-provided packet-position regression bound and keeps the predecessor required by the earliest legal rollback.

## Remaining Risk

- Task 4.5B must call `observePacketPosition()` before incremental replay so safe projection eviction follows actual FFmpeg packet-position progress.
- Task 4.5B must keep independent video and audio PTS/DTS mapper state while sharing this PCR-domain projection.

## Reviewer Follow-up

- Raw transport checkpoints now carry an explicit continuity event with PID, packet byte offset, and either counter-loss or discontinuity-indicator reason. The parser publishes the event before the packet, and the session observer merges both into one same-offset checkpoint.
- Raw transport generation describes every observed transport continuity event. Selected source generation is the clock tracker's generation and changes only for selected video, audio, or PCR PID events; unrelated PID events leave source calibration and generation unchanged.
- Projection requires non-regressing raw generation. After the first retained range item seeds state, every increment must be exactly one and must have one matching event; jumps and event/generation mismatches fail transactionally.
- Selected elementary or PCR continuity events clear calibration and require PCR reacquisition. A PCR discontinuity event and same-packet PCR observation advance selected source generation only once.
- Parser-to-session production-shaped coverage verifies counter-loss evidence reaches the ordered session snapshot. Deterministic projection tests cover selected and unrelated PIDs, same-offset validation, generation jumps/regressions, capacity, rollback bounds, exhaustion, and eviction recovery.
- Projection construction now receives independent initial selected-source generation and expected raw-transport origin. It never derives selected-source generation from a retained raw checkpoint.
- Initial replay proves complete bootstrap history: the first checkpoint is either raw origin generation 0 without an event or the explicit 0-to-1 event transition. Cropped journals, synthetic high raw generations, and event/generation mismatches fail as missing bootstrap/history without changing projection state.
- Projection binding and bootstrap replay occur before packet-position observation can evict history. Tests distinguish unrelated raw events, a first selected event advancing source exactly once, raw synthetic maximum rejection, and genuine selected-source generation exhaustion.
