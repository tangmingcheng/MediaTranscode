# MPEG-TS H.264/HEVC and RKMPP Rate-Control Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add shared Project MPEG-TS H.264/HEVC output and make RKMPP CBR/VBR, bitrate, frame rate, GOP, and dimensions authoritative and verifiable.

**Architecture:** Replace the H.264-shaped mux interface with one codec-typed elementary-stream contract consumed by the existing planner, mux, PES, TS packetizer, scheduled batch, and sender modules. Parse and frame AVC or HEVC behind shared interfaces, and map RKMPP rate control through probed FFmpeg AVOption named constants rather than numeric literals.

**Tech Stack:** C++20, FFmpeg libavcodec/libavutil, Project MPEG-TS DAG, RKMPP/DRM PRIME/RGA, CMake/Ninja, VS2026.

## Global Constraints

- H.264 and HEVC use the same DAG topology and runtime modules.
- Planner products are the only policy authority; downstream code has no defaults or fallback.
- PMT stream types are H.264 `0x1B` and HEVC `0x24` as assigned by H.222.0.
- MPEG-TS video access units are Annex-B; length-prefixed encoder output is converted from its explicit packet-layout contract.
- RKMPP supports only named CBR and VBR capabilities found in the installed encoder AVOptions; CRF fails before DAG startup.
- Temporary TDD code and targets are deleted after each red/green cycle and never committed.
- Windows and RK production media logic remains shared; only FFmpeg/hardware adapters may vary.
- Files are UTF-8 without BOM and CRLF.
- Every RK build runs after `ffenv on`; production performance uses Release and eight build jobs.

---

### Task 1: Codec-Typed MPEG-TS Elementary-Stream Contract

**Files:**
- Create: `src/internal/graph/protocol/mpegts/MediaTsVideoElementaryStreamContract.h`
- Create: `src/internal/graph/protocol/mpegts/MediaTsVideoElementaryStreamContract.cpp`
- Modify: `src/internal/graph/protocol/mpegts/MediaTsMuxPlan.h`
- Modify: `src/internal/graph/protocol/mpegts/MediaTsMuxPlan.cpp`
- Modify: `src/internal/graph/planner/realtime/MediaProjectMpegTsOutputPlan.cpp`

**Interfaces:**
- Produces: `MediaTsVideoCodec { H264, Hevc }`, `MediaTsNalLayout { AnnexB, LengthPrefixed }`, and `MediaTsVideoElementaryStreamContract::create(codec, layout, nalLengthBytes, streamType)`.
- Consumes: planner-selected canonical codec name and `MediaEncodedPacketLayout`.

- [ ] **Step 1: Write a temporary failing TDD probe**

Create `out/tdd/mpegts-video-contract-tdd.cpp` and a temporary CMake executable that asserts H.264/Annex-B accepts stream type `0x1B`, HEVC/length-prefixed accepts `0x24`, codec/stream-type mismatches fail, and length widths outside one through four fail.

```cpp
auto hevc = MediaTsVideoElementaryStreamContract::create(
    MediaTsVideoCodec::Hevc, MediaTsNalLayout::LengthPrefixed, 4, 0x24);
assert(hevc && hevc.value().streamType() == 0x24);
assert(!MediaTsVideoElementaryStreamContract::create(
    MediaTsVideoCodec::Hevc, MediaTsNalLayout::AnnexB, 4, 0x1B));
```

- [ ] **Step 2: Run RED**

Run the temporary target with the existing Windows CMake tree. Expected: compile failure because the typed contract does not exist.

- [ ] **Step 3: Implement the contract and replace H.264-shaped mux fields**

`MediaTsMuxPlanParameters` carries one `MediaTsVideoElementaryStreamContract video` instead of `h264InputLayout` and `h264NalLengthBytes`. `MediaProjectMpegTsOutputPlan` resolves `h264` and `hevc` only, maps packet layout without guessing, and constructs the matching stream type.

- [ ] **Step 4: Run GREEN and delete the temporary probe**

Run the temporary executable, require exit 0, then delete its source, CMake target, binary, PDB/ILK/object directory, and verify no `tdd` entry remains.

- [ ] **Step 5: Review and commit Task 1**

Verify exact contract equality, serialization readiness, CRLF, and `git diff --check`; commit only Task 1 production files.

### Task 2: AVC/HEVC Codec-Parameter Materialization

**Files:**
- Modify: `src/internal/graph/nodes/mux/MediaTsFfmpegStreamConfigMaterializer.cpp`
- Modify: `src/internal/graph/protocol/mpegts/MediaTsMaterializedStreamConfig.h`
- Modify: `src/internal/graph/protocol/mpegts/MediaTsMaterializedStreamConfig.cpp`

**Interfaces:**
- Produces: `MediaTsMaterializedVideoConfig::create(contract, parameterSetsAnnexB)` with ordered typed parameter sets.
- Consumes: `AVCodecParameters` and the Task 1 elementary-stream contract.

- [ ] **Step 1: Write a temporary failing materializer probe**

Use real minimal AVCDecoderConfigurationRecord and HEVCDecoderConfigurationRecord byte arrays to assert H.264 yields SPS/PPS, HEVC yields VPS/SPS/PPS, the codec must match the plan, duplicate/missing parameter sets fail, and the hvcC length width must equal the packet-layout contract.

```cpp
auto result = MediaTsFfmpegStreamConfigMaterializer::video(plan, parameters);
assert(result && result.value().parameterSetsAnnexB().size() == 3);
assert(result.value().codec() == MediaTsVideoCodec::Hevc);
```

- [ ] **Step 2: Run RED**

Expected: HEVC parameters are rejected by the current H.264-only materializer.

- [ ] **Step 3: Implement strict AVC/hvcC and Annex-B parsing**

Keep common start-code and length parsing in one internal helper. AVC accepts exactly SPS/PPS; HEVC accepts exactly VPS/SPS/PPS, validates NAL header type and forbidden bit, and canonicalizes each parameter set to a four-byte start code. No FFmpeg bitstream-filter side chain is introduced.

- [ ] **Step 4: Run GREEN and delete all temporary artifacts**

Require both codec dialects and negative cases to pass, then remove the probe and target completely.

- [ ] **Step 5: Review and commit Task 2**

Check ownership, overflow-safe bounds, no duplicate parser semantics, CRLF, and diff hygiene.

### Task 3: Shared H.264/HEVC Access-Unit Framing and Mux Integration

**Files:**
- Create: `src/internal/graph/protocol/mpegts/MediaTsVideoAccessUnitFramer.h`
- Create: `src/internal/graph/protocol/mpegts/MediaTsVideoAccessUnitFramer.cpp`
- Delete: `src/internal/graph/protocol/mpegts/MediaTsH264AccessUnitFramer.h`
- Delete: `src/internal/graph/protocol/mpegts/MediaTsH264AccessUnitFramer.cpp`
- Modify: `src/internal/graph/protocol/mpegts/MediaTsMuxSession.cpp`
- Modify: `src/internal/graph/protocol/mpegts/MediaTsPsiSerializer.cpp`

**Interfaces:**
- Produces: `MediaTsVideoAccessUnitFramer::frame(plan, config, payload, randomAccess, workspace)`.
- Consumes: Task 1 contract and Task 2 parameter sets.

- [ ] **Step 1: Write a temporary failing framer probe**

Assert length-prefixed H.264 and HEVC convert to Annex-B, existing Annex-B is validated with `MediaAnnexBAccessUnitValidator`, parameter sets precede random-access units in VPS/SPS/PPS or SPS/PPS order, non-random frames are not injected, and malformed lengths/NAL headers fail.

- [ ] **Step 2: Run RED**

Expected: no HEVC-aware shared framer exists.

- [ ] **Step 3: Implement one codec-aware framer and wire the mux**

Replace the H.264 framer call without changing PES, PTS/DTS, PCR, TS continuity, batching, pacing, or sender ownership. PMT serialization consumes the planner stream type already carried by the program plan.

- [ ] **Step 4: Run GREEN and delete temporary TDD artifacts**

Require H.264 regression and HEVC cases to pass, then remove all temporary files and target edits.

- [ ] **Step 5: Review and commit Task 3**

Confirm deletion test: removing the shared framer would force both codecs to duplicate conversion/injection logic.

### Task 4: Exact Plan Serialization and Graph Validation

**Files:**
- Modify: `src/internal/graph/nodes/output/MediaProjectMpegTsPlanSourceNodePlanCodec.cpp`
- Modify: `src/internal/graph/runtime/validation/MediaOutputAuthorityShapeValidator.cpp`
- Modify: `src/internal/graph/runtime/validation/MediaRealtimeVideoGraphShapeValidator.cpp`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimeOutputValidator.cpp`
- Modify: `src/internal/graph/planner/realtime/MediaRealtimeVideoRuntimePlanValidator.cpp`

**Interfaces:**
- Produces: exact encoded codec/layout/length/stream-type fields and equality validation against the planner product.
- Consumes: Task 1 contract.

- [ ] **Step 1: Write a temporary codec round-trip probe**

Encode/decode H.264 and HEVC plans and assert exact equality. Mutate each serialized field independently and assert decode or planner validation fails rather than normalizing the value.

- [ ] **Step 2: Run RED**

Expected: the existing wire codec only stores H.264 layout fields.

- [ ] **Step 3: Implement exact serialization and validators**

Keep existing enum wire values stable, add new codec values without renumbering old values, and reject unknown codecs/layouts/stream types.

- [ ] **Step 4: Run GREEN and remove the probe**

Delete source, target, and binary artifacts after exit 0.

- [ ] **Step 5: Review and commit Task 4**

Run stale-name searches for `h264InputLayout`, `h264NalLengthBytes`, and `MediaTsH264AccessUnitFramer`; expected result is empty.

### Task 5: Authoritative RKMPP CBR/VBR Mapping and Parameter Evidence

**Files:**
- Create: `src/internal/graph/builder/codec/MediaEncoderRateControlOptionAdapter.h`
- Create: `src/internal/graph/builder/codec/MediaEncoderRateControlOptionAdapter.cpp`
- Modify: `src/internal/graph/builder/codec/CodecResolverEncoderContextBuilder.cpp`
- Modify: `src/internal/graph/planner/capability/MediaHardwareCapabilityProbe.cpp`
- Modify: `src/internal/graph/nodes/video/VideoEncodeNode.cpp`
- Modify: `tools/common/GraphCliSupport.h`
- Modify: `tools/common/VideoCliTranscodeOptions.h`
- Modify: `tools/realtime_video_cli/main.cpp`
- Modify: `tools/local_video_cli/main.cpp`

**Interfaces:**
- Produces: a typed adapter that probes named FFmpeg AVOptions and applies/verifies CBR or VBR.
- Consumes: planner-selected encoder and `MediaRateControlMode` with explicit bitrate facts.

- [ ] **Step 1: Write a temporary failing rate-control probe**

Probe `h264_rkmpp` and `hevc_rkmpp` when present. Assert CBR/VBR named constants are discoverable, an absent CRF option fails, option-set errors propagate, and the opened encoder context reports requested dimensions, frame rate, GOP, bitrate bounds, and selected mode.

- [ ] **Step 2: Run RED**

Expected: current generic builder does not map RKMPP `rc_mode`, and private option errors are not authoritative.

- [ ] **Step 3: Implement named-option capability and strict mode contracts**

Enumerate the encoder private AVOptions and resolve `CBR`/`VBR` by name; never embed their numeric values. Apply with `av_opt_set`, check every return code, and read back the selected value. CBR caller input requires target bitrate and explicit buffer size; the planner derives target=min=max and rejects conflicting optional bounds. VBR requires explicit minimum, target, and maximum bitrate facts with planner-validated relationships. CRF on RKMPP fails during capability planning.

- [ ] **Step 4: Add runtime evidence without hot-path work**

Log the opened encoder's codec, dimensions, time base/frame rate, GOP, bitrate/min/max/buffer, selected named rate-control mode, hardware frame format, and backend once at initialization.

- [ ] **Step 5: Run GREEN and delete temporary artifacts**

Verify both RKMPP encoders on the target and software/Windows capability behavior where available. Remove all temporary probes.

- [ ] **Step 6: Review and commit Task 5**

Check no numeric RKMPP mode literal, ignored `av_opt_set`, quality fallback, or platform media-chain fork exists.

### Task 6: Production Builds, Real Media Acceptance, Backup, and Review

**Files:**
- Modify: `QUALITY_SCORE.md`
- Modify: `docs/superpowers/plans/2026-08-17-mpegts-h264-hevc-rate-control.md`
- Create: `docs/superpowers/completions/2026-08-17-mpegts-h264-hevc-rate-control.md`

**Interfaces:**
- Consumes: Tasks 1-5 production code.
- Produces: reproducible commands, measured evidence, backup paths/hashes, risks, and quality score.

- [ ] **Step 1: Run the strict Windows build**

Use `.agents/skills/building-with-vs2026/scripts/rebuild_debug.ps1`; require clean-first all-target completion within 120 seconds and both CLI binaries.

- [ ] **Step 2: Run Windows shared-path regression**

Use the required 120-second source and absolute commands. Cover H.264 and HEVC MPEG-TS output with codec and resolution changes, CBR and VBR, visible VLC, ffprobe, full decode, CPU/RSS, drops, A/V drift where audio applies, and truthful source-driven exit.

- [ ] **Step 3: Synchronize exact committed sources and build RK**

Validate `/home/firefly/Downloads/MediaTranscode` with `pwd -P`, enter `ffenv on`, configure Release, and run `cmake --build ... --clean-first --target all --parallel 8`.

- [ ] **Step 4: Run RK four-codec matrix and rate-control gates**

Run H.264-to-H.264, H.264-to-HEVC, HEVC-to-H.264, and HEVC-to-HEVC with resolution conversion. Exercise CBR and VBR for both output codecs. Require planner-selected RKMPP, DRM PRIME decode/filter/encode, `software_frame=0`, `download=0`, `upload=0`, zero drops, bounded CPU/RSS, valid PMT stream type, complete ffprobe/decode, requested frame rate, GOP, dimensions, and measured bitrate evidence.

- [ ] **Step 5: Back up the accepted version and publish commands**

Create a timestamped `/home/firefly/Downloads/MediaTranscode-rkmpp-<capability>-<timestamp>` directory, copy the Release CLIs, record SHA-256, and provide absolute target commands before starting the next feature.

- [ ] **Step 6: Clean and freeze**

Delete all local/remote temporary TDD sources, targets, archives, scripts, logs selected for cleanup, and verify no media process or port residue. Normalize modified text to UTF-8 CRLF and run `git diff --check`.

- [ ] **Step 7: Commit, push, and obtain two independent approvals**

Commit scoped production/docs changes to `codex/rkmpp-zero-copy`, push, create/update the PR, and have two uninvolved agents independently review the frozen commit. Fix, rebuild, revalidate, push, and obtain both approvals before marking complete.
