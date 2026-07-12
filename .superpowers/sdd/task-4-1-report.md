# Task 4.1 MPEG-TS Parser Report

## Outcome

- Added incremental 188-byte MPEG-TS framing with absolute byte offsets and synchronous transient payload views.
- Added strict transport header, adaptation field, PCR, continuity, and discontinuity validation.
- Added PAT/PMT section reassembly, ordered multi-section aggregation, MPEG-2 CRC32 validation, current/version validation, and immutable multi-program inventory snapshots.
- Added deterministic node tests with generated valid-CRC transport bytes and explicit 192/204-byte stride rejection.
- Reviewer follow-up added confirmed 188-byte stride acquisition/reacquisition, complete adaptation-field bounds validation, PSI table identity checks, and generation-safe discontinuity handling.

## TDD Evidence

RED command:

```powershell
cmd.exe /d /s /c "call D:\VisualStudio2026\Common7\Tools\VsDevCmd.bat -arch=x64 -host_arch=x64 && cmake --build out\build\x64-debug --config Debug --target media_transcode_node_tests"
```

Result: failed as expected because `internal/graph/protocol/mpegts/MediaTsPacketParser.h` did not exist. A prior bare PowerShell attempt was discarded because the Visual Studio standard-library environment was missing and therefore was not a valid feature RED.

Multi-section RED command used the same build-and-run sequence after adding out-of-order, duplicate, missing-section, and version-replacement tests. Result: executable exited 1 with eight expected assertions because the previous implementation rejected `last_section_number > 0`.

Final GREEN results:

```powershell
cmake --build out/build/x64-debug --config Debug --target media_transcode_node_tests
out\build\x64-debug\media_transcode_node_tests.exe
ctest --test-dir out/build/x64-debug -C Debug -R "^media_transcode_node_tests$" --output-on-failure
ctest --test-dir out/build/x64-debug -C Debug -L deterministic --output-on-failure
```

- Focused build: exit 0.
- Focused executable: exit 0.
- Focused CTest: 1/1 passed.
- Deterministic CTest label: 6/6 passed.

Reviewer follow-up RED/GREEN:

- RED command: the focused build-and-run command above. Result: executable exited 1 with failures for false sync acquisition, cross-fragment acquisition, truncated OPCR/private/extension/LTW/piecewise/seamless fields, PAT TSID conflict, and PSI discontinuity/continuity generation reset.
- A separate generation RED exited 1 because reacquiring an identical PMT after discontinuity did not republish inventory for the new generation.
- GREEN command: the same focused build-and-run command after each implementation cycle. Result: exit 0 with all reviewer regressions active.

## Boundary Review

- Payload bytes are exposed as a `std::span` over the parser's packet buffer only during the synchronous sink call; the parser creates no media-payload vector copy.
- Unlocked acquisition requires sync candidates exactly 188 bytes apart and retains at most 188 candidate bytes across pushes; locked partial packet state retains at most 187 bytes. Packet offsets use checked `uint64_t` accounting.
- Locked sync loss enters sliding reacquisition without publishing unconfirmed PCR/PSI. False sync, one-byte insertion, and one-byte deletion recover at the next confirmed stride; structurally malformed packets at confirmed alignment fail closed.
- Adaptation parsing validates fields in specification order: PCR, OPCR, splice countdown, private-data length/data, adaptation-extension length/data, and LTW/piecewise-rate/seamless-splice lengths. All bounds are checked before indexing.
- PAT/PMT section length and descriptor loops are bounded before access; CRC, table id, version, `current_next`, PAT transport-stream id, PMT program-number identity, PID identity, duplicate mapping, and same-version mutation are fail-closed.
- The assembler inventories every PAT program and waits for every mapped PMT; it never selects a program or supplies fallback PIDs.
- Duplicate current-version tables are idempotent; PAT/PMT version changes publish a distinct snapshot after the complete inventory is available.
- Multi-section PAT/PMT tables accept out-of-order sections, remain acquiring while any section is missing, aggregate only `0..last_section_number`, and discard old incomplete aggregation on version replacement.
- Continuity loss and explicit discontinuity clear the affected PID's partial section and incomplete table aggregation. Reacquired identical inventory is republished for the new generation rather than deduplicated across the boundary.
- 192-byte and 204-byte packet strides return `Unsupported`; only 188-byte packets are accepted.

## Whitelist

- `CMakeLists.txt`
- `src/internal/graph/protocol/mpegts/MediaTsPacketParser.h`
- `src/internal/graph/protocol/mpegts/MediaTsPacketParser.cpp`
- `src/internal/graph/protocol/mpegts/MediaTsPsiSectionAssembler.h`
- `src/internal/graph/protocol/mpegts/MediaTsPsiSectionAssembler.cpp`
- `src/internal/graph/protocol/mpegts/MediaTsProgramInventory.h`
- `tests/unit/mpeg_ts_packet_tests.cpp`
- `tests/unit/test_node.cpp`
- `.superpowers/sdd/task-4-1-report.md`
