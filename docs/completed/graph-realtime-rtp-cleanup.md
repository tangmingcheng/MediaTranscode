# Graph Realtime RTP Cleanup

## Completed

- Removed old uncompiled architecture surfaces outside the active graph path, including old public API headers, local/realtime engines, old probes, old examples, and old local/API tests.
- Moved the FFmpeg RAII and error helpers used by graph code into `src/internal/graph/runtime/ffmpeg`.
- Removed legacy realtime graph kinds and builders for packet relay and ingest-to-mux.
- Added a realtime RTP planner model. The builder now consumes planner output for input node options, video branch planning, edge policy, RTP output, SDP writer, mux expectations, and queue policy.
- Made realtime node runtime options explicit. Realtime input and RTP output now fail on missing required options instead of substituting runtime values.
- Added raw RTP planner validation for codec name, payload type, and clock rate. Fully populated raw RTP input still fails as unsupported until RTP ingest/depacketize/normalize nodes are implemented.
- Updated README, samples docs, and CLI help to describe only the active graph surfaces.

## Validation

```powershell
cmake --build out/build/x64-debug --target media_transcode_core
cmake --build out/build/x64-debug --target media_transcode_graph_transcode_cli
cmake --build out/build/x64-debug --target media_transcode_realtime_graph_tests
out/build/x64-debug/media_transcode_realtime_graph_tests.exe
```

## Remaining Work

- Implement graph-native raw RTP ingest, depacketize, codec metadata normalization, and connection into the existing video branch.
- Replace fixed-duration CLI sleep/stop with runtime/session report driven completion and failure handling.
- Add live RTP integration coverage with an FFmpeg sender and SDP/receiver validation.
