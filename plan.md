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
- [ ] Run build/tests, `git diff --check`, commit, push, and request review.
