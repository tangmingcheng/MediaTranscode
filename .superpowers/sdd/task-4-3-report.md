# Task 4.3 Observed MPEG-TS Input Session Report

## Result

- Added a public-protocol inner AVIO ownership seam whose production implementation calls `avio_open2`, plus a non-seekable outer custom AVIO that forwards successful bytes unchanged and observes immutable byte spans at absolute offsets.
- Added structured observer-failure retention. A successful transport read keeps its original length and bytes; the stored evidence error is reported through the AVIO/session status boundary.
- Added a move-only MPEG-TS input session that explicitly selects the `mpegts` demuxer, keeps protocol and demux dictionaries separate, uses `AVFMT_FLAG_CUSTOM_IO`, probes once, rejects non-188 framing, and fails incomplete PAT/PMT inventory.
- Added a prepared MPEG-TS buffer with immutable program/stream snapshots and one successful session transfer.
- Evolved timeline checkpoints to carry an optional raw PCR observation containing its real PID, value, discontinuity flag, and exact packet offset. Different PCR PIDs remain separate offset-indexed checkpoints; preflight does not select a program.

## TDD Evidence

RED command:

```powershell
cmd.exe /d /s /c "call D:\VisualStudio2026\Common7\Tools\VsDevCmd.bat -arch=x64 -host_arch=x64 && cmake --build out\build\x64-debug --config Debug --target media_transcode_node_tests"
```

Result: failed because `MediaTsPreparedInputBuffer.h` did not exist, confirming the test target exercised the missing Task 4.3 surface.

GREEN commands and results:

```powershell
out\build\x64-debug\media_transcode_node_tests.exe
ctest --test-dir out\build\x64-debug -C Debug -R "^media_transcode_node_tests$" --output-on-failure
ctest --test-dir out\build\x64-debug -C Debug -L deterministic --output-on-failure
```

- Focused executable: exit 0.
- Focused CTest: 1/1 passed.
- Deterministic label: 6/6 passed.

## Ownership Review

- Local FFmpeg `libavformat/aviobuf.c` shows `avio_context_free` does not free `AVIOContext::buffer`; the outer buffer is therefore freed explicitly once before the outer context.
- Session destruction cancels reads, closes the format context with its custom `pb` detached, frees the outer buffer/context, then closes the inner protocol AVIO. `AVFMT_FLAG_CUSTOM_IO` prevents FFmpeg from owning the outer AVIO.
- The outer callback opaque points to the owning `FFmpegObservedReadAvio` for the full outer-context lifetime. The protocol opener outlives inner ownership; production uses a process-lifetime implementation and deterministic tests keep the seam alive through AVIO destruction.
- Absolute read offsets use checked unsigned addition. Observer failure, timeout, EOF, and cancellation do not enter retry loops.

## Remaining Concerns

- Task 4.4 must filter raw PCR evidence by the planner-selected PCR PID; Task 4.3 intentionally retains all observed PID identities and performs no selection.
- Timeline capacity and maximum FFmpeg packet-position regression still require planner-owned joint validation.
- Production UDP integration and node consumption remain Task 4.4 scope; this task verifies the deterministic AVIO/session ownership boundary.

## Reviewer Follow-up

- Replaced the temporary owning wrapper around the session's borrowed `AVFormatContext` with `FFmpegInputStreamSnapshotFactory::fromFormatContext(const AVFormatContext&)`. Both `FFmpegFormatContextBuffer` and `MediaTsInputSession` now use this single snapshot implementation. A failure on a later stream leaves the borrowed context owned and safely destructible by its original owner.
- Added production-like session tests using valid-CRC PAT/PMT packets and AAC PES bytes through the inner AVIO seam. They exercise explicit MPEG-TS open/probe, complete inventory, stream snapshots, separate protocol/demux dictionaries, unsupported stride, incomplete input, protocol failure, prepared creation, one transfer, double transfer, and destruction before and after transfer.
- Observer failures and exceptions are stored under synchronization. The successful read that discovered a failure remains unchanged; the next callback returns `AVERROR_INVALIDDATA` without reading inner AVIO again.
- Added callback quiescence with closing state, active-callback accounting, a condition variable, and an RAII callback guard. `close()` cancels and requests protocol interruption, then waits without polling. Resource release occurs after quiescence during destruction.
- Corrected custom-I/O teardown to quiesce observed reads first, retain `pb` while `avformat_close_input()` closes the format context, then destroy observed AVIO to release the outer buffer/context and close inner AVIO once. No borrowed user callback executes during production destruction.
- Added the session-owned `readFrame()` and `close()` boundary. Mutable format/AVIO access is no longer exposed for runtime reads; close rejects new reads, interrupts and quiesces active observed callbacks, waits for active frame reads, and only then tears resources down.
- A callback-thread attempt to reenter `FFmpegObservedReadAvio::close()` fails immediately with a structured error rather than waiting on itself.
- A session-level blocking source test uses real PAT/PMT/PES probing, then blocks a `readFrame()` callback without polling. `close()` requests interruption, waits for the callback and frame-read lease to exit, rejects later reads, and closes inner AVIO once.
- Session read admission now permits exactly one active `av_read_frame()` call. A second reader fails before entering FFmpeg or inner AVIO. Cancellation classification precedes timeout/EOF classification, so an interrupted callback returning `AVERROR_EXIT`, `EAGAIN`, or EOF is always reported as `Cancelled`.
- Program inventory is returned by value and evidence is queried by packet position through `evidenceAtOrBefore()`. The concrete evidence observer protects parser, assembler, inventory, and timeline state with one recursive mutex; it never calls session lifecycle methods while holding that lock. Stream snapshots remain immutable after successful open.
- Session `status()` now reads observed ownership and closed state only while holding the session mutex. Close quiesces without that mutex, captures the final observer failure or stable `Cancelled` status while locked, moves observed ownership to a local, and destroys it after unlocking. A condition-synchronized blocked-read test interleaves status and close and verifies repeated post-close status snapshots are identical without touching released AVIO state.

Follow-up RED was the missing borrowed snapshot factory include. Subsequent behavior RED covered the next-read observer error boundary and real session/prepared ownership. The final focused and deterministic results are recorded from the commands below rather than inferred from the original Task 4.3 run.
