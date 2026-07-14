# Task 10.1 - Shared NTP, RTP Clock Mapping, and RTCP Serialization

## Result

- Added an immutable shared NTP epoch that maps master media running time to extended NTP timestamps without runtime wall-clock reads.
- Added explicit RTP timestamp mapping with planner-supplied clock rate, RTP base, and master origin.
- Added strict RTCP Sender Report, SDES, and BYE serialization with checked wire-width conversion and network byte order.
- Added a transactional RTCP Sender Report schedule: callers prepare a deadline decision and commit it only after successful external transmission.
- Added focused unit coverage to the existing node test target.

## Design Boundary

- Task 10.1 owns only pure timestamp mapping, RTCP packet generation, SDES text validation, and report scheduling primitives.
- The shared epoch, RTP base, clock rate, SSRC, CNAME, packet count, and octet count are explicit inputs; none are inferred or defaulted downstream.
- No FFmpeg node, custom AVIO transport, UDP socket, SDP publication, graph topology, startup gate, or output pacing behavior is implemented here.
- Task 10.2 and Task 10.3 consume these primitives in protocol I/O and runtime nodes; Task 12 remains responsible for planner and graph integration.

## Contract Coverage

- NTP mapping uses one immutable epoch and checked master-time deltas, including fractional-second conversion and modulo wire seconds only at serialization boundaries.
- RTP mapping uses checked rescaling and an extended timestamp domain; negative results and arithmetic overflow fail instead of rebasing or repairing timestamps.
- Sender Reports derive NTP and RTP timestamps from the same master instant.
- Compound RTCP serialization emits SR followed by SDES, with optional BYE, strict lengths, nonzero SSRC validation, UTF-8 CNAME validation, and checked 32-bit counters.
- Scheduling supports prepare/commit, token replay rejection, concurrent stale-token rejection, generation reset invalidation, and late deadline advancement without burst emission.
- Public callers cannot construct mapped NTP or RTP timestamps, or schedule commit tokens, outside their owning mapper and scheduler.

## TDD Evidence

### Initial RED

Command:

```powershell
cmd /d /c "call D:\VisualStudio2026\Common7\Tools\VsDevCmd.bat -arch=x64 -host_arch=x64 >nul && cmake --build out\build\x64-debug --target media_transcode_node_tests --parallel 1"
```

Result: failed as expected while compiling the new tests because `internal/graph/protocol/rtp/MediaRtcpSenderReportGenerator.h` did not exist.

### Transactional-schedule RED

Command:

```powershell
cmd /d /c "call D:\VisualStudio2026\Common7\Tools\VsDevCmd.bat -arch=x64 -host_arch=x64 >nul && cmake --build out\build\x64-debug --target media_transcode_node_tests --parallel 1"
```

Result: failed as expected with `LNK2019` for the removed `MediaRtcpSenderReportSchedule::poll(...)` API before the tests were migrated to prepare/commit semantics; the link ended with `LNK1120`.

### GREEN Build

Command:

```powershell
cmd /d /c "call D:\VisualStudio2026\Common7\Tools\VsDevCmd.bat -arch=x64 -host_arch=x64 >nul && cmake --build out\build\x64-debug --target media_transcode_node_tests --parallel 1"
```

Result: exit code `0`.

### GREEN Test

Command, run from `out/build/x64-debug`:

```powershell
& .\media_transcode_node_tests.exe; $code = $LASTEXITCODE; Write-Output "EXIT=$code"; exit $code
```

Result: `EXIT=0`. Existing FFmpeg and MPEG diagnostic output was present, with no assertion failure or test-process error.

## Review Closure

The first independent review reported 0 Critical, 2 Important, and 3 Minor findings. All five findings were addressed:

- Replaced state-mutating schedule polling with transactional prepare/commit semantics.
- Checked late-interval step bounds before multiplication and narrowing.
- Restricted NTP timestamp, RTP timestamp, and commit-token construction to their owning components.
- Added strict UTF-8 and control-character validation for RTCP SDES text.
- Normalized all owned text files to UTF-8 without BOM and CRLF line endings.

Fresh independent staged review completed with `Spec PASS`, `Quality PASS`, and `Critical/Important/Minor = 0/0/0`.

## Final Stage Verification

- Full clean rebuild removed 304 prior outputs and completed 305 build steps with exit code 0.
- `ctest --test-dir out/build/x64-debug -L deterministic --output-on-failure`: 6/6 passed in 4.11 seconds.
- `ctest --test-dir out/build/x64-debug -L integration --output-on-failure`: 3/3 passed in 168.32 seconds.

## Self-Review

- No wall-clock or sleep call is present in the mapping or scheduling path.
- No first-packet rebase, timestamp repair, fallback, hidden default, or duplicate semantic helper was introduced.
- Mapping, packet serialization, text validation, and scheduling remain separate responsibilities.
- Schedule state advances only after a valid commit representing successful external transmission.
- Owned-file encoding audit reported `OWNED=15 ENCODING_ISSUES=0`; tracked diff whitespace validation was clean.

## Owned Files

- `CMakeLists.txt`
- `src/internal/graph/time/MediaNtpTimestamp.h`
- `src/internal/graph/time/MediaSharedNtpEpoch.h`
- `src/internal/graph/time/MediaSharedNtpEpoch.cpp`
- `src/internal/graph/protocol/rtp/MediaRtpTimestamp.h`
- `src/internal/graph/protocol/rtp/MediaRtpOutputClockMapper.h`
- `src/internal/graph/protocol/rtp/MediaRtpOutputClockMapper.cpp`
- `src/internal/graph/protocol/rtp/MediaRtcpSenderReportGenerator.h`
- `src/internal/graph/protocol/rtp/MediaRtcpSenderReportGenerator.cpp`
- `src/internal/graph/protocol/rtp/MediaRtcpSenderReportSchedule.h`
- `src/internal/graph/protocol/rtp/MediaRtcpSenderReportSchedule.cpp`
- `src/internal/graph/protocol/rtp/MediaRtcpSdesTextValidator.h`
- `src/internal/graph/protocol/rtp/MediaRtcpSdesTextValidator.cpp`
- `tests/unit/rtp_output_clock_tests.cpp`
- `tests/unit/test_node.cpp`
