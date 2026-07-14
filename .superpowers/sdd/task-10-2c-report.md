# Task 10.2C Report

## Result

Implemented an explicit, sender-only UDP transport for the Task 10.2B scheduled RTP/RTCP sinks.

- The public RTP UDP plan requires one address family, numeric addresses, even/+1 remote ports, fixed even/+1 local ports or independent OS assignment, explicit nonblocking pressure behavior, send buffer, and a pair-wide datagram limit.
- Sender port outcomes distinguish complete local-kernel acceptance, zero-byte side-effect-safe rejection, and ambiguous positive partial delivery.
- The transport owns two factory-created unopened ports, opens RTP then RTCP, atomically publishes verified bound endpoints, rolls back both ports on failure, and preserves sticky poison.
- Factory creation and the complete open transaction, including endpoint observation and publication, are exception-contained and convert failures into structured results.
- A mutex rejects concurrent/reentrant operations before port calls; cross-thread close waits for the active send and same-thread reentrant close is rejected.
- Close is an internal transaction: published local endpoints are cleared under the lifecycle mutex, both physical port closes run without that mutex, and `Closed` (or the existing `Poisoned` state) is published only after both closes finish.
- A same-thread close callback can inspect state/endpoints and receives a structured rejection from reentrant `close()`; other close callers wait for physical completion, while open/send are rejected without port calls during the close transaction.
- The Winsock implementation uses shared `MediaSocketRuntime`, `SO_EXCLUSIVEADDRUSE`, `SO_SNDBUF`, `FIONBIO`, bind/getsockname/sendto, one-shot open, and RAII close. It does not reuse the receive socket or event path.
- Tests are split into transport behavior, Task 10.2B composition, a shared fake-port fixture, and independent real IPv4 and IPv6 loopback integration executables.

## TDD evidence

- RED: the node target failed with `C1083` because `MediaRtpUdpSenderConfig.h` did not exist.
- Post-review RED: a close callback that reentered `transport.close()` deterministically deadlocked the old lock-held physical-close implementation; the node executable exceeded the controlled 3-second timeout and was terminated.
- GREEN: deterministic transport and composition tests built and passed after the implementation.

## Test commands and results

- `out\build\x64-debug\media_transcode_node_tests.exe`
  - Final-review fresh result: exit code 0 in 270 ms after a clean rebuild in the Visual Studio 2026 developer environment.
  - Covers strict config, factory null/throw handling, first-port returned/throw failure ordering, endpoint getter throw rollback, fixed and OS-assigned endpoint validation, delivery certainty, sticky poison, second concurrent-send rejection, same-thread close callback rejection and safe observation, close-time open/send rejection without port calls, physical close completion waiting, Task 10.2B RTCP transaction retry, ambiguous RTCP poison, accepted-only FU-A counters, sink lifetime, and sender guard reentry.
- `out\build\x64-debug\media_transcode_rtp_udp_sender_loopback_tests.exe`
  - Final-review fresh result: exit code 0 in 13 ms.
  - Covers two independent Winsock IPv4 sender sockets, RTP/RTCP routing, nonzero distinct OS-assigned ports, exclusive fixed-port collision, and no reopen after successful close or failed open.
- `out\build\x64-debug\media_transcode_rtp_udp_sender_ipv6_loopback_tests.exe`
  - Final-review fresh result on this machine: exit code 0 in 12 ms; the IPv6 executable was run independently after the IPv4 executable.
  - CTest registration uses skip code 77 only when IPv6 loopback or an adjacent IPv6 receiver pair is explicitly unavailable.

## Final verification

Commands:

```powershell
ctest --test-dir out/build/x64-debug -L deterministic --output-on-failure
ctest --test-dir out/build/x64-debug -L integration --output-on-failure
```

Results after the final Visual Studio 2026 `--clean-first --parallel 1` rebuild:

- Full rebuild: cleaned `299` artifacts and rebuilt `325` targets; exit code `0`.
- Deterministic: `6/6` passed, `0` failed, `4.11 s`.
- Integration: `5/5` passed, `0` failed, `165.30 s`.
- Final independent review: Spec `PASS`, Quality `PASS`, Critical/Important/Minor `0/0/0`.

## Changed files

- `src/internal/graph/runtime/network/MediaUdpDatagramSenderPort.{h,cpp}`
- `src/internal/graph/runtime/network/MediaUdpDatagramSenderSocket.{h,cpp}`
- `src/internal/graph/protocol/rtp/MediaRtpUdpSenderConfig.{h,cpp}`
- `src/internal/graph/protocol/rtp/MediaRtpUdpSenderTransport.{h,cpp}`
- `tests/unit/fixtures/MediaRtpUdpSenderFakePort.h`
- `tests/unit/media_rtp_udp_sender_transport_tests.cpp`
- `tests/unit/media_rtp_udp_sender_composition_tests.cpp`
- `tests/unit/media_rtp_udp_sender_loopback_tests.cpp`
- `tests/unit/media_rtp_udp_sender_ipv6_loopback_tests.cpp`
- `tests/unit/test_node.cpp`
- `CMakeLists.txt`

## Remaining scope and risks

- Planner/node production wiring, A/V composition, pacing, SDP publication, BYE, and legacy RTP mux removal remain Task 12 or Task 10.3 scope.
- `NotAccepted` intentionally provides delivery certainty, not a promise that the cause is transient; retry policy remains outside the transport.
