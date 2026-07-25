# Task 4.2 Report

## Result

- Added an immutable absolute-byte-offset MPEG-TS evidence timeline with planned rollback bounds and fail-closed capacity handling.
- Added a selected-program PCR tracker using the shared 27 MHz timestamp unwrapper, strict identity/interval/jitter/gap validation, and generation-based reacquisition.
- Added independent 33-bit PTS/DTS source-time mapping anchored to the nearest legal PCR epoch, with no missing-value substitution.

## TDD Evidence

- Initial RED: the node target failed because the three requested clock component headers did not exist.
- Review RED: selected identity fields/APIs were absent; negative nearest-epoch mapping selected a distant future epoch; pair and discontinuity transactions lacked regression coverage.
- GREEN: `cmake --build out/build/x64-debug --config Debug --target media_transcode_node_tests` succeeded.
- GREEN: `out\build\x64-debug\media_transcode_node_tests.exe` exited with code 0.

## Review

- PCR observation and PTS/DTS pair mapping use candidate-state transactions; rejected operations publish no partial generation, calibration, unwrap, or epoch-offset state.
- PCR modulus is `(1 << 33) * 300`; PTS/DTS modulus is `1 << 33`.
- Epoch alignment evaluates representable adjacent candidates with unsigned distance, chooses the earlier epoch on exact half ties, and uses checked signed arithmetic for offsets and source deltas.
- Timeline eviction retains the predecessor needed by the earliest legal rollback query and fails when capacity cannot be reclaimed safely.
- PCR, elementary PID, program, PMT, and selected stream identity are immutable inputs; unrelated continuity loss does not invalidate calibration.
- Across discontinuity generations, a new PCR-only source anchor is clamped to the last published source time and then advances exclusively by PCR delta.
- Timeline queries return checkpoints by value for clear immutable ownership; checkpoint payload copies remain a future profiling concern.

## Remaining Concerns

- Session integration must attach complete inventory/PCR/discontinuity checkpoints at the correct observed byte offsets.
- Planner validation must ensure timeline capacity and rollback distance are jointly sufficient for FFmpeg read-ahead behavior.
