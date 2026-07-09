# Realtime RTP Quality and Playback Stability Plan

## Goal

Fix realtime RTP playback quality and stability without adding graph/node defaults or patch-style runtime behavior.

## Root Cause Evidence So Far

- Source `test.mp4` video bitrate is about `8405 kb/s`.
- Current raw RTP realtime output SDP advertises `b=AS:2000` when `--bitrate` is omitted.
- Raw RTP input metadata currently provides codec/payload/fmtp but no observable source bitrate.
- This means the graph can reach the encoder without an explicit or observable bitrate, allowing FFmpeg/NVENC defaults to affect behavior.
- Video+audio smoke writes both audio and video packets, but playback can still show stutter/corruption; remaining candidates are low output bitrate, RTP/H264 parameter signaling, and realtime queue overflow/drop policy.

## Architecture Rules

- Planner owns all decisions.
- Runtime nodes must not invent codec, bitrate, queue, or RTP defaults.
- Missing transcode parameters must resolve from explicit CLI input or observable input metadata; otherwise planning must fail.
- If realtime queue behavior changes, it must be represented in the plan/edge policy, not hardcoded in nodes.

## Tasks

- [x] Add regression tests proving raw RTP video transcode without explicit bitrate is rejected when bitrate is not observable.
- [x] Add regression tests proving raw RTP video transcode with explicit bitrate plans successfully and propagates the value to graph node options.
- [x] Extend video input stream planning data to carry observable bitrate for file/realtime URL inputs.
- [x] Make raw RTP input planning reject missing bitrate for transcode-frame video when no source bitrate is observable.
- [x] Verify no-audio RTP smoke with explicit source-equivalent bitrate and hardware enabled.
- [x] Verify video+audio RTP smoke with explicit source-equivalent bitrate and hardware enabled.
- [x] Use VLC or receiver-side diagnostics to confirm playback behavior and document the result.
- [x] Update `docs/completed/` with commands and actual results.
- [x] Run build/tests, `git diff --check`, commit, push, and request review, including review follow-up fixes.

## Realtime RTP Audio+Video Stutter Investigation

## Goal

Find the root cause for VLC stutter/artifacts that appear only when realtime RTP outputs video and audio together, while video-only RTP output is smooth. Any fix must preserve DAG responsibilities: planner owns decisions, graph builder owns topology, runtime nodes only execute explicit options, and no node adds hidden fallbacks or timing patches.

## Current Observation

- Video-only realtime RTP output is smooth when hardware is enabled.
- Video+audio realtime RTP output has synchronized audio/video, but VLC playback can stutter and show artifacts.
- This narrows the suspect area to audio branch planning/execution, separate RTP dual-output topology, shared scheduler/backpressure, timestamp/rescale behavior, or receiver SDP/session behavior.

## Investigation Tasks

- [x] Reproduce and collect logs for graph video-only RTP output plus VLC.
- [x] Reproduce and collect logs for graph video+audio RTP output plus VLC.
- [x] Reproduce FFmpeg direct video+audio RTP output plus VLC using the same source and receiver settings.
- [x] Compare DAG topology and runtime summaries for video-only vs video+audio output.
- [x] Inspect whether audio branch work or RTP mux/output writes can block video progress in the current scheduler.
- [x] Inspect audio/video RTP timestamps, packet counts, packet loss diagnostics, and SDP session layout.
- [x] Add regression coverage that separate RTP H264 output carries an explicit planner-produced global-header encoder option.
- [x] Implement the architecture-level fix in the planner/builder/encoder-context boundary, then rebuild and smoke test.
- [ ] Commit, push, PR update, and request a fresh review.

## Investigation Result

- The repeated `media_transcode_realtime_graph_tests.exe` access violation was reproduced after adding `std::optional<bool>` to `MediaVideoTranscodeParameters`; a clean rebuild fixed it. Root cause is stale incremental object layout, not a remaining graph source crash.
- Realtime video+audio RTP uses the hardware path: `h264_cuvid` decode, CUDA frames, `h264_nvenc` encode, and `filter=not_required` when no resize is requested.
- FFmpeg/NVENC RTP output does not include H264 parameter sets in SDP unless the encoder context uses `AV_CODEC_FLAG_GLOBAL_HEADER`.
- The graph fix is planner-owned: separate RTP + resolved H264 output codec sets `video.global_header=1`; segment builder propagates it; encoder context builder applies `AV_CODEC_FLAG_GLOBAL_HEADER`.
- VLC now receives SDP with `sprop-parameter-sets` and logs `found NAL_PPS` plus `Received first picture`; no `waiting for SPS/PPS` line was observed in the retest log.
- VLC still reports RTP loss, playback-late events, and hardware acceleration picture allocation failures. Those remain receiver/network/timing risks to investigate separately if visible stutter persists after the H264 parameter-set signaling fix.
