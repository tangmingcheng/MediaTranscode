# Realtime VideoOnly Stream Set Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task. Repository rules prohibit committed automated tests; use static inspection, clean-first build, and real media chains.

**Goal:** Deliver planner-owned `VideoOnly` across local file and every realtime input/output route without adding PCMA or regressing the canonical A/V path.

**Architecture:** Replace the ambiguous audio boolean with an explicit request stream set. The realtime planner produces exactly one typed runtime product for VideoOnly or synchronized A/V; builders and runtime validators consume that product without fallback. VideoOnly reuses the production RTP and Project MPEG-TS output authorities while omitting every audio node, port, queue, PID, PES, and SDP media description.

**Tech Stack:** C++20, FFmpeg, CMake/Ninja, Visual Studio 2026, PowerShell, VLC.

## Global Constraints

- Work only on `codex/realtime-video-only-stream-set`; commit and push every completed task to this branch.
- `MediaTranscodeStreamSet` has stable values `AudioVideo = 0` and `VideoOnly = 1`; planner input is explicit and has no default.
- Remove `includeAudio`; do not retain compatibility aliases or duplicate semantic helpers.
- Planner owns every stream-set, timing, PID, port, codec, and output decision. Downstream fallback, fake audio, sentinel audio PIDs, and ignored conflicting parameters are forbidden.
- Preserve current A/V startup, servo, correction, canonical scheduler, and H.264 + AAC-LC 48 kHz stereo Project MPEG-TS contract.
- VideoOnly Project MPEG-TS contains H.264 video only. HEVC input must be explicitly transcoded to H.264 for MPEG-TS output.
- Manual and automatic H.264/HEVC video FMTP converge on validated typed signaling facts; insufficient or conflicting evidence fails planning.
- Do not add, restore, or commit CI, CTest, unit, integration, acceptance, hardware, or performance tests.
- Text files are UTF-8 with CRLF. Preserve untracked FFmpeg headers and `out/`.

---

### Task 1: Explicit StreamSet request contract

**Interfaces:**
- Produce `enum class MediaTranscodeStreamSet : std::uint8_t { AudioVideo = 0, VideoOnly = 1 };` in the graph model.
- Replace `MediaTranscodeExecutionParameters::includeAudio` with `std::optional<MediaTranscodeStreamSet> streamSet`.
- Local and realtime CLI always assign the field; `--no-audio` selects `VideoOnly`, otherwise `AudioVideo`.

- [ ] Add the focused enum model and integrate it into CMake.
- [ ] Migrate presets, local planning, realtime classification, CLI parsing, and summaries away from `includeAudio`.
- [ ] Reject missing stream set and reject VideoOnly combined with any explicit audio codec/rate/channel/bitrate/profile/preset/startup/A-V-gap or raw-RTP audio metadata.
- [ ] Require the video startup bound for both stream sets; permit audio startup and A/V gap only for AudioVideo.
- [ ] Search for residual `includeAudio`, inspect the diff, commit `refactor(graph): make transcode stream set explicit`, and push.

### Task 2: Stream-set-aware input preparation

**Interfaces:**
- Generic/RTSP probing consumes the explicit stream set and selects video only for VideoOnly.
- MPEG-TS selected-program and runtime-binding products use explicit VideoOnly/A-V variants; absence of audio is not represented by `-1` or an invalid PID.
- Raw RTP VideoOnly owns only a video transport and prepared resource.

- [x] Make generic input scanning independent of audio for VideoOnly, including sources that still contain audio.
- [x] Add typed MPEG-TS VideoOnly selection/binding carrying video PID, PCR PID, time base, and video provenance capacity only.
- [x] Remove the VideoOnly rejection in MPEG-TS preparation and stop creating audio PES provenance/bindings.
- [x] Ensure raw RTP VideoOnly never validates, opens, waits for, or seals companion audio.
- [x] Inspect all ownership/close paths, commit `feat(realtime): prepare explicit video-only inputs`, and push.

### Task 3: Manual and automatic FMTP convergence

**Interfaces:**
- Manual and automatic H.264/HEVC paths both yield the existing validated typed video signaling facts and one sealed prepared video transport when probing is used.
- Request codec/PT/clock identity must equal detected identity.

- [x] Keep manual FMTP validation before product planning for H.264 and HEVC.
- [x] Make automatic VideoOnly probing independent of audio and preserve the single startup deadline.
- [x] Converge manual/automatic paths before depacketizer and product construction; reject incomplete or conflicting facts.
- [x] Verify prepared transport handoff remains exactly once and no runtime inference/fallback is added.
- [x] Commit `feat(rtp): support video-only manual and detected signaling`, and push.

### Task 4: Typed VideoOnly runtime and scheduling

**Interfaces:**
- Replace optional competing output products with one explicit runtime variant containing `MediaRealtimeVideoRuntimePlan` or `MediaRealtimeAvSyncRuntimePlan`.
- VideoOnly runtime owns video startup, source timing, scheduling, and one output-adapter plan.

- [ ] Add the focused VideoOnly runtime product and exact validator.
- [ ] Extract media-independent output-clock/commit behavior needed by both VideoOnly and A/V rather than copying a pacing loop.
- [ ] Build VideoOnly startup and scheduled-video flow without audio or A/V correction components.
- [ ] Keep the AudioVideo runtime topology and timing behavior unchanged.
- [ ] Commit `feat(realtime): plan video-only runtime scheduling`, and push.

### Task 5: Unified production outputs and shape validation

**Interfaces:**
- Scheduled RTP, SDP, Project MPEG-TS, compiler, and runtime shape validators consume the planned stream set.
- VideoOnly separate RTP creates one video sender and one video SDP media description.
- VideoOnly MPEG-TS creates H.264-only PMT/PES and uses video-derived planned PCR authority; MPEG-TS/RTP remains PT 33 at 90 kHz.

- [ ] Generalize production scheduled-output segments to the exact planned stream set without optional-port guessing.
- [ ] Support VideoOnly Project MPEG-TS over UDP and RTP; do not broaden the mux codec contract.
- [ ] Enforce exact node/port/PID/PES/SDP cardinality in builder, compiler, and runtime validators.
- [ ] Remove the obsolete realtime `singleStreamOutput`/FFmpeg MPEG-TS branch and other now-dead compatibility code while preserving local-file mux users.
- [ ] Search for fake audio/default/fallback and semantic duplication, commit `feat(realtime): deliver video-only production outputs`, and push.

### Task 6: Build, real-media acceptance, review, and delivery

- [ ] Run the repository VS2026 clean-first x64 Debug build within 120 seconds; do not run CTest.
- [ ] Execute 38 VideoOnly chains: local 2; RTSP 6; MPEG-TS input 6; raw RTP 24 across H.264/HEVC, manual/automatic FMTP, two source semantics, and three outputs.
- [ ] Execute 18 A/V regressions: RTSP 3; MPEG-TS input 3; raw RTP 12 across H.264/HEVC, manual/automatic FMTP, and three outputs.
- [ ] For every realtime chain use `out/acceptance/test-continuous-120s.mp4`, start CLI then FFmpeg then VLC, stop only the source, and capture exact commands/PIDs, source-driven exit, CPU, memory, errors, SDP/PID evidence, and residue checks.
- [ ] Write a concise completion report under `docs/completed/` without build commands; update `QUALITY_SCORE.md` through an independent quality-review agent.
- [ ] Perform whole-branch self-review, commit/push all fixes, create a ready PR, and obtain a fresh independent PR-agent PASS. Do not merge.
