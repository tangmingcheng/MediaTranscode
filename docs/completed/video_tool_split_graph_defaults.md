# Video Tool Split and Graph Default Cleanup

## Completed Changes

- Split the old mixed graph CLI into `media_transcode_local_video_cli` and `media_transcode_realtime_video_cli`.
- Removed the old `media_transcode_graph_transcode_cli` and `media_transcode_realtime_rtp_input_cli` targets and entry files.
- Moved common video transcode CLI parsing into `tools/common/VideoCliTranscodeOptions.h`.
- Realtime video input now requires explicit input type, input layout, output layout, realtime open/probe options, queue capacities, and output settings.
- Optional transcode codec arguments now mean "inherit from source"; planner no longer requires an explicit realtime video codec.
- Queue capacities now default to invalid `0` in graph parameters and must be supplied by upstream request construction.
- Realtime video planner reuses the same resize rule as local planning: filter score is included only when resize is requested.
- Realtime plan summaries report `filter=not_required` when the selected plan does not require a filter stage.

## Validation

- Added regression coverage in `tests/unit/test_realtime_rtp_graph.cpp` for tool target split, legacy switch removal, source codec inheritance, unknown source codec rejection, and realtime filter scoring.
- Build/test targets used by this change:
  - `media_transcode_core`
  - `media_transcode_local_video_cli`
  - `media_transcode_realtime_video_cli`
  - `media_transcode_realtime_graph_tests`
- `out/build/x64-debug/media_transcode_realtime_graph_tests.exe` passed.
- Local-file smoke passed with `out/build/x64-debug/test.mp4`.
- Realtime RTP smoke passed with `encodedPacketsPushed=42` and `workerErrors=0`.
- Release target build also passed in `out/build/x64-release`.
