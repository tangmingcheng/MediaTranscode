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
