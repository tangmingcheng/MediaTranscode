# Task 10.2B - Scheduled RTP Sender and Transactional RTCP Dispatch

## Result

- Added a distinct single-stream sender session with explicit `Created`, `Open`, and `Poisoned` lifecycle states. It composes the shared NTP epoch, RTP mapper, successful RTP counters, and transactional RTCP Sender Report schedule.
- Split packetizer construction behind the production `ScheduledRtpPacketizerFactory` boundary. The default FFmpeg factory is explicitly injected; there is no hidden default or test mutator.
- Extended the one RTP datagram rewriter to return validated payload-format octets through a typed sink; no second RTP parser or per-datagram allocation was added.
- Corrected custom write AVIO ownership by freeing the current context buffer before freeing the AVIO context.
- Kept UDP sockets, SDP, graph/planner wiring, pacing, BYE lifecycle, and legacy removal outside this task.

## Contract Coverage

- Complete configuration rejects invalid CNAME, zero/mismatched generation, empty sinks, and H264/AAC clock-rate mismatches before FFmpeg is opened. SSRC remains uniquely owned by the stream identity and RTP base/rate/origin by the mapper; no duplicate comparison fields were introduced.
- H264 FU-A and AAC-LATM tests verify each accepted RTP datagram and exact RTP payload-format octets. The AAC test independently parses RTP CSRC, extension, padding, and payload boundaries, then verifies the one-byte LATM length field and known 100-byte AU without trusting sender metadata. Counter increments are preflighted against the RTCP `uint32_t` wire limit before sink delivery and committed together only after sink success.
- Partial RTP delivery counts only accepted fragments. RTP sink, packetizer, or counter failure preserves the first terminal error and poisons later AU/SR operations. A thrown RTP sink exception is converted at the FFmpeg callback boundary and treated as an ambiguous terminal failure with zero counter commit.
- Sender Report dispatch returns typed `NotDue` or `Sent` results. A due report uses one report instant for NTP and RTP mapping, includes the current successful counters, sends synchronously, and commits the schedule only after RTCP acceptance.
- Returned RTCP `Status` failure is retryable without deadline mutation. A thrown RTCP sink exception has ambiguous side effects and permanently poisons the session. Operation guards reject AU/SR sink reentry before state mutation.
- Tests cover SSRC/CNAME/fixed-vector NTP/RTP/counter wire fields, RTP wrap, exact `uint32_t` maximum serialization, overflow-before-delivery, retry at the same scheduled deadline, all legal lifecycle transitions, deterministic packetizer-open poisoning, and no token exposure in diagnostics.

## TDD Evidence

### Initial RED

Command:

```powershell
cmd.exe /d /s /c ""D:\VisualStudio2026\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build out\build\x64-debug --target media_transcode_node_tests --config Debug --parallel 1"
```

Result: failed as expected with `C1083` because `internal/graph/nodes/mux/ScheduledRtpSenderConfig.h` did not exist.

### Lifecycle Review RED

Command, from `out/build/x64-debug`:

```powershell
& '.\media_transcode_node_tests.exe'
```

Result: exit code `1` as expected. Created-state AU/SR calls were accepted and a real malformed AAC configuration did not deterministically fail `open()`.

### GREEN Direct Test

Command, from `out/build/x64-debug`:

```powershell
& '.\media_transcode_node_tests.exe'; $code=$LASTEXITCODE; Write-Output "EXIT=$code"; exit $code
```

Result: final fresh run after CRLF/UTF-8 normalization and relink returned `EXIT=0` in `0.122 s`. Existing FFmpeg/MPEG-TS diagnostic output was present; no assertion or process error occurred.

## First Review Closure

- C1: added explicit `Created -> Open -> Poisoned` ownership, deterministic open-failure injection through the production packetizer factory boundary, and coverage proving Created-state misuse and duplicate `open()` do not poison while operational open failure preserves the first error.
- I1: replaced AAC counter self-confirmation with an independent RTP boundary parser and known LATM/AU wire vector.
- I2: added thrown RTP-sink coverage proving ambiguous side effects poison the sender, preserve the structured first error for later AU/SR calls, and do not commit counters.
- M1: NTP diagnostics and RTCP wire fields now each assert the fixed epoch/report vector `3908988801:429496729`, independently of one another.
- Commit-after-send invariant: no fault seam was added. `MediaRtcpSenderReportSchedule` is sender-owned, its commit token is not exposed, the operation guard spans prepare/send/commit, and no reset or concurrent mutation path exists. Therefore a freshly prepared token cannot become stale between accepted synchronous send and commit in this ownership model; a commit failure remains a defensive poisoned-state branch, not an injectable runtime transition.

## Second Review Closure

- M1 RED: `EmptySuccessPacketizerFactory` returned `success(nullptr)` and the direct node test failed because sender creation incorrectly succeeded.
- M1 GREEN: `ScheduledRtpSenderSession::create()` now enforces the factory postcondition and returns structured `InternalError: scheduled RTP packetizer factory returned no session` before exposing a sender. The regression verifies both failure code and message.
- Second-review verification: the focused target rebuilt with exit code `0`; the fresh direct node test returned exit code `0` in `0.131 s`.

## Review

- RAII: caller-provided AVIO buffer and context are released exactly once; the 64-cycle open/close/reset regression passes.
- Responsibilities: configuration validation, packetizer construction, RTP rewrite metadata, FFmpeg packetization, sender orchestration, time mapping, RTCP serialization, and schedule state remain separate components.
- Failure semantics: RTP counters and RTCP schedule commits are transactional at their declared synchronous boundaries; first terminal failures are retained.
- Authority: all clocks, identity, CNAME, generation, packetization mode, counter seed, and sinks are explicit. No fallback, timestamp repair, first-packet rebase, sleep, clock read, URL/socket open, or hidden retry exists.
- Scope: no legacy RTP node, planner, builder, graph registration, SDP, UDP transport, or production path was modified.
- Final independent review: Spec `PASS`, Quality `PASS`, Critical/Important/Minor `0/0/0`.

## Final Verification

Commands:

```powershell
ctest --test-dir out/build/x64-debug -L deterministic --output-on-failure
ctest --test-dir out/build/x64-debug -L integration --output-on-failure
```

Results after enabling the integration tier in the existing build cache and performing a second `--clean-first --parallel 1` full rebuild:

- Full rebuild: cleaned `310` artifacts and rebuilt `315` targets; exit code `0`.
- Deterministic: `6/6` passed, `0` failed, `3.95 s`.
- Integration: `3/3` passed, `0` failed, `173.56 s`.

## Owned Files

- `CMakeLists.txt`
- `src/internal/graph/runtime/ffmpeg/FFmpegDatagramWriteAvio.cpp`
- `src/internal/graph/protocol/rtp/MediaRtpDatagramRewriter.h`
- `src/internal/graph/protocol/rtp/MediaRtpDatagramRewriter.cpp`
- `src/internal/graph/nodes/mux/ScheduledRtpMuxFfmpegSession.h`
- `src/internal/graph/nodes/mux/ScheduledRtpMuxFfmpegSession.cpp`
- `src/internal/graph/nodes/mux/ScheduledRtpPacketizerSession.h`
- `src/internal/graph/nodes/mux/ScheduledRtpMuxFfmpegSessionFactory.h`
- `src/internal/graph/nodes/mux/ScheduledRtpMuxFfmpegSessionFactory.cpp`
- `src/internal/graph/nodes/mux/ScheduledRtpSenderConfig.h`
- `src/internal/graph/nodes/mux/ScheduledRtpSenderConfig.cpp`
- `src/internal/graph/nodes/mux/ScheduledRtpSenderSession.h`
- `src/internal/graph/nodes/mux/ScheduledRtpSenderSession.cpp`
- `tests/unit/scheduled_rtp_packetization_tests.cpp`
- `tests/unit/scheduled_rtp_sender_tests.cpp`
- `tests/unit/test_node.cpp`
- `.superpowers/sdd/task-10-2b-report.md`
