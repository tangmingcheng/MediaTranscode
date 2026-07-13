# Task 4.2 Report

## Result

- Added an immutable absolute-byte-offset MPEG-TS evidence timeline with planned rollback bounds and fail-closed capacity handling.
- Added a selected-program PCR tracker using the shared 27 MHz timestamp unwrapper, strict identity/interval/jitter/gap validation, and generation-based reacquisition.
- Added independent 33-bit PTS/DTS source-time mapping anchored to the nearest legal PCR epoch, with no missing-value substitution.

## TDD Evidence

- RED: the node target failed because the three requested clock component headers did not exist.
- GREEN: `cmake --build out/build/x64-debug --config Debug --target media_transcode_node_tests` succeeded.
- GREEN: `out\build\x64-debug\media_transcode_node_tests.exe` exited with code 0.

## Review

- Rejected observations do not mutate PCR, PTS, or DTS unwrap state.
- PCR modulus is `(1 << 33) * 300`; PTS/DTS modulus is `1 << 33`.
- Epoch alignment and source-time conversion use checked signed arithmetic paths.
- Timeline eviction retains the predecessor needed by the earliest legal rollback query and fails when capacity cannot be reclaimed safely.

## Remaining Concerns

- Session integration must attach complete inventory/PCR/discontinuity checkpoints at the correct observed byte offsets.
- Planner validation must ensure timeline capacity and rollback distance are jointly sufficient for FFmpeg read-ahead behavior.
