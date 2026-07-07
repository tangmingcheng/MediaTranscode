# Realtime RTSP to RTP Video Transcode Plan

## Current Stage

Build the first realtime video path:

```text
RTSP or realtime URL -> graph DAG -> planner selected video chain -> RTP output + SDP
```

This stage is limited to video-only RTSP/realtime URL input. RTP raw input and audio output remain out of scope.

## Scope

- Add a realtime graph option model for input, output, and transcode parameters.
- Reuse the graph planner to probe realtime URLs and select the best available decoder/filter/encoder chain.
- Add a realtime RTP transcode graph builder.
- Add runtime nodes for realtime input, RTP output, RTP muxing, and SDP writing.
- Add `tools/graph_transcode_cli --mode realtime-rtp`.
- Explicitly reject raw RTP/SDP input in this phase.
- Redact URL userinfo in CLI and planner diagnostics.
- Add unit coverage for validation, graph construction, and planner-selected software execution when hardware is disabled.

## Verification

- Build `media_transcode_core`.
- Build `media_transcode_graph_transcode_cli`.
- Build and run `media_transcode_realtime_graph_tests`.
- Run a 15 second RTSP to RTP smoke test when the configured RTSP source is reachable.

## Deferred

- RTP raw stream input.
- Audio RTP output.
- Detailed decoded/encoded frame counters in CLI output.
- Receiver-side SDP playback automation.
