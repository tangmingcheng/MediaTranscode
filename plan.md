# Video Tool Split and Graph Default Cleanup Plan

## Goal

Split local-file video transcoding and realtime video transcoding into separate tools, remove redundant CLI switches, and ensure graph/planner code never relies on behavior-changing defaults. Missing transcode parameters must either be resolved from observable input metadata during planning or fail before graph build.

## Implementation Checklist

- [x] Add tests first for rejected legacy switches, new tool parser semantics, source-codec inheritance, unknown-codec failures, audio-default behavior, and realtime filter scoring.
- [x] Replace mixed CLI targets with `media_transcode_local_video_cli` and `media_transcode_realtime_video_cli`.
- [x] Keep common transcode arguments optional in the CLI, but pass absence through as "inherit from source"; do not synthesize codec/rate-control/default stream values in CLI code.
- [x] Make realtime video tool require explicit `--input-type rtsp|rtp|mpegts-udp`, `--input-layout session|separate|mpegts`, and `--output-layout separate|mpegts`.
- [x] Remove `--mode`, `--video`, `--audio`, and `--enable-hw`; keep `--disable-hw`, `--no-audio`, `--quiet-graph`, and `--no-low-latency`.
- [x] Remove graph/planner defaults that decide behavior, especially default video codec, realtime timeout/probe values, queue capacities, and hardware preference strings.
- [x] Make video always planned for these video tools. Audio is enabled by default and disabled only by `--no-audio`.
- [x] Reuse the local video planner filter rule for realtime: filter is required only when resize is requested.
- [x] Update realtime plan summary so no-filter plans report `filter=not_required`.
- [x] Add a short completion note under `docs/completed/`.

## Verification

- [x] Build `media_transcode_core`, `media_transcode_local_video_cli`, `media_transcode_realtime_video_cli`, and `media_transcode_realtime_graph_tests` in `out/build/x64-debug`.
- [x] Run `out/build/x64-debug/media_transcode_realtime_graph_tests.exe`.
- [x] Run a local-file smoke using `out/build/x64-debug/test.mp4`.
- [x] Run a realtime RTP smoke with FFmpeg sender and confirm encoded packet progress when the environment is available.
- [x] Run `rg` checks for legacy CLI switches/default implementations.
- [x] Run `git diff --check`.
- [x] Review the full diff against this plan and fix any missing items before commit.
- [x] Commit and push branch `codex/split-video-tools-cli`, open PR, and request a fresh agent review.
