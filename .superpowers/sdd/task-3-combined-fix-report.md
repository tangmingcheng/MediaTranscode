# Task 3 Combined Review Fix Report

## Status

`DONE`

The four approved combined-review findings are fixed. The startup-consumer finding was intentionally not changed because Task 6 owns atomic startup coordination.

## RED evidence

Focused build:

```powershell
cmd.exe /d /s /c "call D:\VisualStudio2026\Common7\Tools\VsDevCmd.bat -arch=x64 -host_arch=x64 >nul && cmake --build out\build\x64-debug --config Debug --target media_transcode_runtime_tests media_transcode_node_tests media_transcode_integration_tests"
```

The new tests compiled against the pre-fix implementation.

Focused RED run:

```powershell
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -R "media_transcode_(runtime|node|integration)_tests"
```

`media_transcode_node_tests` failed at `tests/unit/test_node.cpp:728`: generation 2 clock evidence produced `MediaRtpClockGroupState::Locked` while generation 2 and generation 3 invalidations were stacked on the same stream. This directly reproduced the clock ordering finding.

The subsequent bounded-fairness RED ran:

```powershell
out\build\x64-debug\media_transcode_node_tests.exe
```

It failed at `tests/unit/test_node.cpp:742`: five sustained video invalidations prevented the queued audio clock from being consumed in the bounded processing turn.

The public raw planner and option-applier regressions were written before their production changes. They cover absent, zero, and negative `readTimeoutMs`, plus missing and unsupported RTCP composition products.

The transport stress regression starts 16 concurrent receive entries and races them against `close()`, then requires every entry to finish within one second with `Cancelled` or `NotInitialized`. The old implementation's unsynchronized `unique_ptr` read/reset race was confirmed directly in `receive`, `isOpen`, and `close`; one pre-fix stress run did not reproduce the timing-dependent crash. No stronger claim of an observed transport RED is made.

## Implemented fixes

### Stable RTP/RTCP transport lifetime

- The public transport handle now owns a mutex-protected `shared_ptr<Impl>`.
- Every operation takes a stable lifetime snapshot before accessing implementation state.
- `close()` atomically removes the public handle, marks the implementation aborted, signals cancellation, waits for serialized receives, then closes sockets and the WSA event.
- Socket mutation remains under the receive mutex; lifecycle state remains under the lifecycle mutex.
- Immutable bound-port values avoid blocking port queries behind a receive wait.
- Move construction and move assignment explicitly synchronize handle ownership.

### Ordered clock-group ingress

- Every RTP clock/event buffer carries an explicit per-stream sequence assigned by `RawRtpInputNode`.
- `MediaRtpClockGroupNode` holds one pending envelope from each event/clock input and merges each stream by sequence.
- Duplicate, zero, or regressing sequence values fail fast.
- Queued invalidations receive priority before clocks, while a per-stream per-turn invalidation quota and a four-envelope processing budget preserve bounded cross-stream fairness.
- Generation watermarks still reject evidence older than the latest invalidated generation.

### Planner-owned raw RTP timeout and RTCP composition

- `MediaRealtimeInputPlanner::planRawRtp` rejects missing and non-positive `readTimeoutMs`; `value_or(2500)` and its local fallback constant are removed.
- `rtcpCompositionMode` is an optional planner product so missing state is representable and rejected.
- The sole enum-to-option and option-to-enum authority is `MediaRtcpCompositionPolicy`; the option applier serializes the planner product instead of writing a literal.
- Missing and unsupported composition values fail before node construction; `RawRtpInputNode` parses and retains the planned mode for RTCP parsing.

### Encoding

The reviewer-listed eight LF-only Task 3 files and every file modified by this fix are UTF-8 without BOM and CRLF-only. The eight original files were:

- `MediaIpAddressFamily.h`
- `MediaRtcpCompositionPolicy.h`
- `MediaRtcpCompoundParser.cpp`
- `MediaRtcpCompoundParser.h`
- `MediaSocketRuntime.cpp`
- `MediaSocketRuntime.h`
- `MediaUdpSocket.cpp`
- `MediaUdpSocket.h`

## Final verification

Full clean rebuild:

```powershell
cmd.exe /d /s /c "call D:\VisualStudio2026\Common7\Tools\VsDevCmd.bat -arch=x64 -host_arch=x64 >nul && cmake --build out\build\x64-debug --config Debug --clean-first"
```

Result: exit 0; 238 files cleaned and all 239 build steps completed.

Deterministic suite:

```powershell
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -L deterministic
```

Result: final fresh rerun 6/6 passed, 0 failed, 4.83 seconds.

Integration suite:

```powershell
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -R media_transcode_integration_tests
```

Result: 1/1 passed, 0 failed, 76.53 seconds.

An earlier incremental integration run crashed after the planner aggregate layout changed. A clean rebuild removed the stale-object ABI mismatch; the integration suite then passed twice, in 77.37 seconds and 76.53 seconds.

## Modification whitelist

- `.superpowers/sdd/task-3-combined-fix-report.md`
- `src/internal/graph/builder/realtime/MediaRealtimeOptionApplier.cpp`
- `src/internal/graph/model/MediaIpAddressFamily.h`
- `src/internal/graph/nodes/input/RawRtpInputNode.cpp`
- `src/internal/graph/nodes/input/RawRtpInputNode.h`
- `src/internal/graph/nodes/sync/MediaRtpClockGroupNode.cpp`
- `src/internal/graph/nodes/sync/MediaRtpClockGroupNode.h`
- `src/internal/graph/planner/realtime/MediaRealtimeInputPlanner.cpp`
- `src/internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h`
- `src/internal/graph/protocol/rtp/MediaRtcpCompositionPolicy.cpp`
- `src/internal/graph/protocol/rtp/MediaRtcpCompositionPolicy.h`
- `src/internal/graph/protocol/rtp/MediaRtcpCompoundParser.cpp`
- `src/internal/graph/protocol/rtp/MediaRtcpCompoundParser.h`
- `src/internal/graph/protocol/rtp/MediaRtpUdpTransport.cpp`
- `src/internal/graph/protocol/rtp/MediaRtpUdpTransport.h`
- `src/internal/graph/runtime/buffer/MediaRtpIngressEventBuffer.cpp`
- `src/internal/graph/runtime/buffer/MediaRtpIngressEventBuffer.h`
- `src/internal/graph/runtime/network/MediaSocketRuntime.cpp`
- `src/internal/graph/runtime/network/MediaSocketRuntime.h`
- `src/internal/graph/runtime/network/MediaUdpSocket.cpp`
- `src/internal/graph/runtime/network/MediaUdpSocket.h`
- `tests/unit/test_event_driven_runtime.cpp`
- `tests/unit/test_node.cpp`
- `tests/unit/test_realtime_rtp_graph.cpp`

Existing unrelated acceptance-script changes, generated FFmpeg headers, `.codex`, `out`, `AGENTS.md`, and the existing documentation file were not modified by this task.

## Remaining risks

- The transport remains Windows-only; a non-Windows cancellable event implementation is still required for cross-platform raw RTP ingress.
- The 64-bit ingress sequence intentionally fails closed if it ever wraps to zero; practical wrap is unreachable, but no reset protocol exists while a node is running.
- Fixed local UDP port ranges in transport tests can collide with an unrelated process on the same host; the tests scan a range but do not ask the OS for an atomic adjacent even/odd pair.
- Startup consumption/atomic release remains deferred to Task 6 by explicit instruction.

## Fresh review 299bd36 Important fixes

### Deterministic RED

The transport concurrency tests were replaced with a controlled operation observer. The first focused build failed because `MediaRtpUdpTransportOperationObserver` and `MediaRtpUdpTransportOperation` did not exist, proving that the old API had no deterministic way to establish a real protected receive/lifecycle entry:

```powershell
cmd.exe /d /s /c "call D:\VisualStudio2026\Common7\Tools\VsDevCmd.bat -arch=x64 -host_arch=x64 >nul && cmake --build out\build\x64-debug --config Debug --target media_transcode_runtime_tests media_transcode_node_tests"
```

The compiler reported the missing observer base, operation enum, and observer-bearing config contract.

The fairness authenticity RED then compiled and ran, but failed because the final bounded-turn snapshot had neither audio nor video calibration and was not `Locked`. This proved that an emptied input channel was not evidence that audio clock evidence had affected the validator.

### Cancellation resource lifetime

- `Impl::lifecycleMutex` is now the sole authority for cancellation state, cancellation sequence, `receiveActive`, and every `WSASetEvent`, `WSAResetEvent`, `WSACloseEvent`, and invalid-handle assignment.
- The consistent nested lock order is `receiveMutex` then `lifecycleMutex`. `close()` performs its initial cancellation under lifecycle ownership, releases it, acquires the receive lock, then reacquires lifecycle ownership for event closure.
- `stop()` and `abort()` take a stable `Impl` snapshot, then revalidate state and the event under lifecycle ownership. If `close()` won the race, they return `Cancelled` without touching the closed event.
- The receive observer fires only after the receive mutex is held and `receiveActive` is set. The stop/abort observer fires after the stable lifetime snapshot and before lifecycle acquisition.

Deterministic tests now prove:

- receive-entry versus close: close remains blocked on the real active receive until the barrier releases; receive returns `Cancelled`, never `NotInitialized`.
- stop versus close: close completes while stop is paused after snapshot; resumed stop returns `Cancelled` without an invalid event operation.
- abort versus close: the same controlled interleaving returns `Cancelled` without an invalid event operation.

### Bounded fairness evidence

The clock-group test now uses two stacked video invalidations followed by fresh video generation 5 and audio generation 2 evidence. Within the explicit four-envelope processing budget:

- every snapshot before the final one is asserted not `Locked`;
- the final snapshot must be `Locked`;
- the final video calibration must carry generation 5;
- the final audio calibration must carry generation 2.

The scheduler prioritizes at most two invalidations per stream per turn. This drains the bounded stacked invalidation window before its clocks without introducing an unbounded drain, then gives the other stream a processing opportunity within the same turn.

### Fresh final verification

```powershell
cmd.exe /d /s /c "call D:\VisualStudio2026\Common7\Tools\VsDevCmd.bat -arch=x64 -host_arch=x64 >nul && cmake --build out\build\x64-debug --config Debug --clean-first"
```

Result: exit 0; 238 files cleaned; 239/239 build steps completed.

```powershell
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -L deterministic
```

Result: final fresh rerun 6/6 passed, 0 failed, 3.18 seconds.

```powershell
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -R media_transcode_integration_tests
```

Result: 1/1 passed, 0 failed, 79.75 seconds.

The modification whitelist remains the list above; this review round changed only `MediaRtpUdpTransport.h/.cpp`, `RawRtpInputNode.cpp`, `MediaRtpClockGroupNode.cpp`, `test_event_driven_runtime.cpp`, `test_node.cpp`, and this report. The startup-consumer finding remains intentionally untouched.
