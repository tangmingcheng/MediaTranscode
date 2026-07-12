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

## Fresh reviewer v2 closure

### RED

The virtual operation observer was first removed from the tests and replaced with the wished-for concrete phase-controller API. The focused build failed because `MediaRtpUdpTransportPhaseController` and `MediaRtpUdpTransportPhase` did not exist. This proved that the deterministic, non-callback handshake facility was absent.

The five-invalidation backlog regression then failed at the first bounded turn: its last snapshot was not `Acquiring`. The trace showed that a regular video clock selected ahead of the queued invalidation was incorrectly counted against the invalidation priority quota, so cross-stream audio evidence ran after only one invalidation and was immediately cleared by the second.

Focused RED/GREEN command:

```powershell
cmd.exe /d /s /c "call D:\VisualStudio2026\Common7\Tools\VsDevCmd.bat -arch=x64 -host_arch=x64 >nul && cmake --build out\build\x64-debug --config Debug --target media_transcode_runtime_tests media_transcode_node_tests"
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -R "media_transcode_(runtime|node)_tests"
```

Final focused result: 2/2 passed, 0 failed.

### Concrete phase controller

- `MediaRtpUdpTransportOperationObserver` and all virtual callbacks were removed.
- `MediaRtpUdpTransportPhaseController` is concrete, final, non-copyable, and supports only five fixed phases: protected receive entry, stop lifetime acquisition, abort lifetime acquisition, close receive-lock wait, and close receive-lock acquisition.
- The controller owns only phase state, a mutex, and a condition variable. It cannot execute caller code.
- Every controller operation is `noexcept`; synchronization exceptions mark the controller unhealthy instead of escaping through transport code.
- Armed waits have an explicit maximum duration. A timeout marks the controller unhealthy and releases the transport path, preventing teardown deadlock.
- Production supplies a null controller explicitly; the controller does not influence planner decisions or normal transport behavior.

The receive-close regression now uses symmetric handshakes:

1. Receive reaches `ReceiveProtected` while holding the receive mutex and after setting `receiveActive`.
2. Close reaches `CloseReceiveWait` immediately before attempting that mutex.
3. The test releases the close wait phase, so close must block on the already-owned mutex.
4. The test releases receive, then observes `CloseReceiveAcquired`.
5. Receive must return `Cancelled`; `NotInitialized` is not accepted.

No timeout-based scheduling inference remains in the close-race coverage. Stop-close and abort-close remain paused after stable lifetime acquisition and resume only after close has completed event teardown.

### Bounded invalidation scheduling

`processStream` now returns `NoInput`, `Regular`, or `Invalidation`. Only the `Invalidation` result consumes the named maximum of two prioritized invalidations per stream per processing turn; an ordinary clock selected ahead of an event cannot consume this quota.

The regression queues one earlier regular video clock, five stacked video invalidations, and cross-stream audio evidence:

- Turn 1 processes the earlier clock, exactly two prioritized invalidations, then audio evidence. All snapshots are non-`Locked`; the last snapshot is `Acquiring` and its group generation has advanced by exactly two, proving the audio evidence affected validator state within the bounded turn.
- Turn 2 processes the remaining three invalidations. Every snapshot remains non-`Locked`.
- Turn 3 supplies fresh video generation 8 and audio generation 3 evidence. The final snapshot is `Locked` and exposes exactly those two generations.

This proves no temporary lock, no starvation, real cross-stream processing, and final generation correctness for a backlog larger than the priority bound.

### v2 final verification

```powershell
cmd.exe /d /s /c "call D:\VisualStudio2026\Common7\Tools\VsDevCmd.bat -arch=x64 -host_arch=x64 >nul && cmake --build out\build\x64-debug --config Debug --clean-first"
```

Result: exit 0; 239 files cleaned; 240/240 build steps completed.

```powershell
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -L deterministic
```

Result: final fresh rerun 6/6 passed, 0 failed, 2.98 seconds.

```powershell
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -R media_transcode_integration_tests
```

Result: 1/1 passed, 0 failed, 80.31 seconds.

v2 modification whitelist:

- `.superpowers/sdd/task-3-combined-fix-report.md`
- `src/internal/graph/nodes/sync/MediaRtpClockGroupNode.cpp`
- `src/internal/graph/nodes/sync/MediaRtpClockGroupNode.h`
- `src/internal/graph/protocol/rtp/MediaRtpUdpTransport.cpp`
- `src/internal/graph/protocol/rtp/MediaRtpUdpTransport.h`
- `src/internal/graph/protocol/rtp/MediaRtpUdpTransportPhaseController.cpp`
- `src/internal/graph/protocol/rtp/MediaRtpUdpTransportPhaseController.h`
- `tests/unit/test_event_driven_runtime.cpp`
- `tests/unit/test_node.cpp`

Startup-consumer coordination remains untouched.

## Final combined reviewer v3 closure

### Opus mapping-family-zero boundary

- Added `validateOpusRtpMappingFamilyZeroChannels` as the single `1..2` channel capability constraint.
- The realtime RTP codec registry and `MediaRtpDepacketizerFactory` both reuse that constraint; `MediaRawRtpStreamDescriptorFactory` continues to validate through the factory instead of duplicating codec policy.
- Planner and descriptor regressions prove mono and stereo succeed while channel counts 0 and 3 fail with `InvalidArgument`.

### Active-source SDES identity

- `MediaRtcpSenderReportTracker` now filters SDES chunks by the active media SSRC before CNAME validation.
- A parsed compound regression covers multiple chunks and multiple items: an active non-empty CNAME plus an unrelated empty CNAME preserves evidence and generation; an active empty CNAME invalidates exactly once.

### Transport test port ownership

- Initial, replacement, receive-close, stop-close, and abort-close transport tests now share `openTransportInPortRange`.
- The replacement move/rebind regression no longer assumes a fixed free RTP/RTCP port pair.

### Fresh verification

Full clean rebuild result: exit 0; 240 files cleaned; 241/241 build steps completed.

```powershell
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -L deterministic
```

Result: 6/6 passed, 0 failed, 3.36 seconds.

```powershell
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -R media_transcode_integration_tests
```

Result: 1/1 passed, 0 failed, 80.47 seconds.

Final combined reviewer v3 changed only the Opus capability/consumers, RTCP tracker, the three focused test files, and this report. Startup-consumer coordination remains untouched.
