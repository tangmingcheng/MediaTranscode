# Task 10.2A - Scheduled RTP Packetization Boundary

## Result

- Added an application-owned FFmpeg custom write AVIO that receives complete RTP datagrams through an injected sink and owns its buffer/context with explicit open, close, and reset lifecycle.
- Added strict RTP datagram validation and final rewriting of only the planned dynamic payload type, mapped RTP timestamp, and SSRC.
- Added a distinct single-stream scheduled FFmpeg RTP mux session for explicit H264 Annex-B and AAC LATM packetization modes.
- Kept transport, pacing, clocks, planner/builder integration, SDP, and legacy RTP timing removal outside this component task.

## Design Boundary

- `FFmpegDatagramWriteAvio` owns only custom AVIO allocation, datagram callback delivery, first structured sink failure, and lifecycle. It does not open URLs or sockets, pace, sleep, or read a clock.
- `MediaRtpDatagramRewriter` validates version, RTP/RTCP identity, header shape, extension, padding, payload, and exact source payload type before applying the planned identity and mapped timestamp.
- `ScheduledRtpMuxFfmpegOptions` is the sole FFmpeg option application/verification boundary. H264 requires `skip_rtcp`; AAC LATM requires `skip_rtcp+latm`. `send_bye` is never enabled.
- Options are applied and immediately verified before `avformat_write_header`; every emitted datagram is still validated and rewritten by the final application-owned authority.
- `ScheduledRtpMuxFfmpegSession` owns exactly one format context, one configured stream, one custom AVIO, and one reusable rewrite scratch buffer. The custom `pb` is detached before format-context destruction.
- MPEG4-GENERIC AAC is intentionally not exposed: FFmpeg batches small access units across calls in that mode, which violates per-access-unit timestamp ownership. AAC LATM emits each tested small access unit during its own write.
- Task 12 remains responsible for planner/graph wiring and legacy timing-authority removal.

## TDD Evidence

### Initial RED

Command:

```powershell
cmd.exe /d /s /c ""D:\VisualStudio2026\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build out\build\x64-debug --target media_transcode_node_tests --config Debug"
```

Result: failed as expected while compiling the new focused tests because `ScheduledRtpMuxFfmpegSession.h` did not exist.

An earlier build attempted without the Visual Studio developer environment failed on missing standard-library headers; it was an environment failure and is not counted as TDD RED evidence.

### Sink-Failure Preservation RED

Command:

```powershell
& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_node_tests.exe'; $code = $LASTEXITCODE; Write-Output "EXIT=$code"; exit $code
```

Result: failed the new close-after-sink-failure assertion because AVIO cleanup discarded the first callback failure. The lifecycle was then corrected so close flushes, preserves the first structured failure, and releases ownership exactly once.

### Independent-Review RED

Command:

```powershell
cmd.exe /d /s /c ""D:\VisualStudio2026\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build out\build\x64-debug --target media_transcode_node_tests --config Debug"
```

Result: exit code `1` as expected. The updated tests rejected the old duplicate AVIO buffer-capacity parameter, old eight-argument stream configuration, and allocation-returning RTP rewrite API before the implementation was changed. The same RED set also introduced strict AAC LATM completeness, real FU-A structure/reassembly, reusable caller-owned scratch, and partial-fragment sink-failure/reset coverage.

### Final GREEN Build

Command:

```powershell
cmd.exe /d /s /c ""D:\VisualStudio2026\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build out\build\x64-debug --target media_transcode_node_tests --config Debug"
```

Result: exit code `0`.

### Final GREEN Test

Command:

```powershell
& 'D:\Code\MyCode\MediaTranscode\out\build\x64-debug\media_transcode_node_tests.exe'; $code = $LASTEXITCODE; Write-Output "EXIT=$code"; exit $code
```

Result: `EXIT=0`. Existing FFmpeg/MPEG-TS diagnostic output was present; there were no assertion failures or test-process errors.

## Independent Review Closure

The first independent review returned `FAIL` with `0 Critical / 4 Important / 2 Minor`. All six findings were structurally addressed: the duplicate packet-size parameter was removed, mux options are verified before header writing, partial fragmented-write failure is terminal and reset-tested, AAC LATM completeness is rejected at configuration, FU-A wire semantics are fully verified, and rewrite storage is session-owned and reused. No compatibility overload or production wiring was added.

## Contract Coverage

- AVIO complete configuration accepts only `maximumDatagramBytes` and rejects values no larger than the RTP fixed header. The actual AVIO buffer capacity, `pb->max_packet_size`, and format `packet_size` all equal that one value; no compatibility overload remains.
- AVIO tests cover open/close/reset, complete datagram delivery, invalid configuration, no callback after close, returned sink failures, and preservation of the first failure. The callback boundary also converts unexpected sink exceptions to a structured internal error.
- RTP rewrite tests cover minimal headers, CSRC, extension, padding, marker, sequence, payload preservation, timestamp wrap, malformed/truncated input, wrong version, RTCP rejection, exact payload-type matching, and caller-provided storage reuse without capacity growth.
- Real FFmpeg H264 tests prove FU-A type 28, start/end and marker bits, consecutive sequence numbers, exact NAL payload reassembly, one mapped timestamp across all fragments, and distinct mapped timestamps across access units while FFmpeg PTS values differ.
- Real FFmpeg AAC LATM tests prove the first small access unit emits in its own write and consecutive access units carry their own mapped timestamps. Stream configuration separately rejects missing sample rate, invalid/empty channel layout, missing ASC extradata, and time bases other than exact `1/sample_rate`.
- Session tests cover explicit mode/codec/kind/time-base validation, lifecycle duplication rejection, zero RTCP across header/write/trailer, and partial FU-A failure after one successful fragment. The first original sink error is returned unchanged by the failed write and all later write/trailer calls, no same-context retry reaches the sink, and reset/reconfigure/open/write succeeds.
- Compile-time checks prevent default construction of complete configuration and mapped timestamp values.

## Self-Review

- RAII: custom AVIO buffer/context and FFmpeg format context have explicit, single-owner cleanup; `pb` is detached before format free.
- Single responsibility: AVIO delivery, RTP validation/rewrite, mux configuration, option application/verification, and session lifecycle are separate files.
- Planner authority: all payload type, SSRC, packet size, codec parameters, stream kind, time base, and packetization mode are required inputs; no fallback or inferred mode was added.
- Failure semantics: callback/sink failure preserves the first structured error and permanently poisons the session until reset/recreate; no retry or false transactional claim exists.
- Copying: codec parameters are copied once into owned session configuration. Each FFmpeg datagram requires one copy into the session-owned rewrite scratch, but capacity is reserved once from `maximumDatagramBytes` and reused without per-datagram allocation; the sink synchronously consumes a span.
- Scope: no production builder, planner, node, SDP, socket, pacing, or legacy timing path was modified.
- Encoding: all owned text files were normalized to UTF-8 without BOM and CRLF.

## Final Stage Verification

- Full clean rebuild removed 310 prior outputs and completed 311 build steps with exit code 0.
- `ctest --test-dir out/build/x64-debug -L deterministic --output-on-failure`: 6/6 passed in 3.99 seconds.
- `ctest --test-dir out/build/x64-debug -L integration --output-on-failure`: 3/3 passed in 170.67 seconds.
- Final independent staged review: `Spec PASS`, `Quality PASS`, `Critical/Important/Minor = 0/0/0`.

## Owned Files

- `CMakeLists.txt`
- `tests/unit/test_node.cpp`
- `tests/unit/scheduled_rtp_packetization_tests.cpp`
- `src/internal/graph/runtime/ffmpeg/FFmpegDatagramWriteAvio.h`
- `src/internal/graph/runtime/ffmpeg/FFmpegDatagramWriteAvio.cpp`
- `src/internal/graph/protocol/rtp/MediaRtpDatagramRewriter.h`
- `src/internal/graph/protocol/rtp/MediaRtpDatagramRewriter.cpp`
- `src/internal/graph/protocol/rtp/MediaScheduledRtpPacketizationMode.h`
- `src/internal/graph/nodes/mux/ScheduledRtpMuxStreamConfig.h`
- `src/internal/graph/nodes/mux/ScheduledRtpMuxStreamConfig.cpp`
- `src/internal/graph/nodes/mux/ScheduledRtpMuxFfmpegOptions.h`
- `src/internal/graph/nodes/mux/ScheduledRtpMuxFfmpegOptions.cpp`
- `src/internal/graph/nodes/mux/ScheduledRtpMuxFfmpegSession.h`
- `src/internal/graph/nodes/mux/ScheduledRtpMuxFfmpegSession.cpp`
- `.superpowers/sdd/task-10-2a-report.md`
