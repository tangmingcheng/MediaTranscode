# Realtime RTP Audio+Video Smoothness Root-Cause Plan

## Goal

Fix realtime RTP audio+video stutter and video corruption at the DAG architecture boundary. Planner owns queue, threading, and copy/transcode decisions; builders materialize planned topology; runtime nodes execute explicit options without hidden fallback timing behavior.

## Root Cause Evidence

- `MediaRealtimeRtpTranscodePlanner` plans `SpscRing` edge policy, but the realtime RTP builder and shared segment builders still call `blockingQueuePolicy(...)` for most heavy data-path edges.
- `tools/realtime_video_cli` directly starts `MediaGraphRuntime` without applying realtime planner threading policy, so workers keep the default `idleSleepMs=1`.
- `MediaAudioPipelinePlanner::planKnownAudioTranscode()` always emits `TranscodeFrame`, so raw RTP AAC->AAC cannot use the existing audio packet-copy branch.
- Raw RTP audio+video shared one `av_read_frame` point; audio-first startup could lead video by about 1.5-2 seconds and expose scheduling coupling even after removing blocking queues.
- RTP output lacked an explicit A/V startup barrier and relied only on per-stream pacing, so separate RTP outputs could start on different media wall-clock points.
- Raw RTP video could enter the decoder after the receiver had missed sender startup packets. Waiting for an encoded output keyframe was too late because the decoder could already have emitted damaged frames.
- RTP mux startup delay originally slept before writing but kept old pacing baselines, so buffered packets could be sent as a startup burst after the delay elapsed.
- Fixed sleep in graph worker hot paths is not an industrial realtime DAG default. It is acceptable only as explicit media pacing, non-realtime throttling, shutdown/wait behavior, or external polling.

## Tasks

- [x] Add failing tests for raw RTP AAC->AAC packet copy, raw RTP audio transcode when target differs, realtime RTP non-blocking data-path queues, realtime `idleSleepMs=0`, CLI application of planner threading policy, and graph sleep audit classification.
- [x] Add planner-owned realtime runtime policy and lane-specific edge policies to `MediaRealtimeRtpTranscodePlan`.
- [x] Update realtime graph/segment builders to consume planned edge policies instead of hardcoded blocking policies on RTP data paths.
- [x] Refactor known-input audio planning to reuse source-vs-target copy decision and enable raw RTP packet-copy fast path when source and target match.
- [x] Update realtime CLI to plan once, build from that plan, and set runtime threading policy before `startThreaded()`.
- [x] Review fixed sleeps under `src/internal/graph`, classify allowed cases, and keep realtime worker hot-path sleep disabled by policy.
- [x] Add isolated raw RTP audio/video input chains so separate RTP A/V no longer shares one demux/read point.
- [x] Treat realtime queue overflow drops as normal backpressure, preserve queued keyframes, preserve incoming keyframes when no non-key packet can be dropped, and verify FFmpeg packet keyframe flags map into `MediaBuffer`.
- [x] Add planner-owned RTP mux pacing and monotonic packet timestamp options.
- [x] Add an A/V packet start barrier for separate RTP output so first video/audio packets open together and pre-open packets keep latest media.
- [x] Add a planner-enabled video packet start gate for raw RTP input so the video decode branch opens on a keyframe after packet loss or mid-stream join.
- [x] Reset RTP mux packet pacing and byte pacing session baselines immediately before first packet write, after startup delay, so buffered startup packets are not burst-sent.
- [x] Run build, unit tests, `ctest`, `git diff --check`, and raw RTP audio+video smoke with FFmpeg/VLC or receiver diagnostics.
- [x] Document test/smoke commands and actual results in `docs/completed/realtime-rtp-av-smoothness.md`.
- [x] Review all touched code for missed blocking queues, duplicated audio copy semantics, planner/runtime policy gaps, and unclassified sleeps.
- [x] Commit, push, open PR, and request a fresh agent PR review.

## Verification Commands

```powershell
cmake --build out/build/x64-debug --target clean
cmake --build out/build/x64-debug --target media_transcode_core media_transcode_realtime_video_cli media_transcode_realtime_graph_tests
out\build\x64-debug\media_transcode_realtime_graph_tests.exe
ctest --test-dir out/build/x64-debug --output-on-failure
powershell -ExecutionPolicy Bypass -File out\build\x64-debug\codex_av_scheduler_smoke.ps1
powershell -ExecutionPolicy Bypass -File out\build\x64-debug\codex_human_vlc_60s.ps1
git diff --check
```

## Assumptions

- Worker-pool or node-fusion rewrite is out of P0 scope unless smoke proves it is required.
- Audio packet copy is allowed only when source codec and requested target parameters match; encode-only audio options force transcode.
- Fixed sleep in CLI polling or explicit media pacing can remain when classified. Fixed sleep in realtime worker hot path must not be active for realtime execution.
