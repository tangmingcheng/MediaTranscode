# Realtime RTSP to RTP Video Transcode Completion

Date: 2026-07-07

## Completed Scope

- Added the realtime RTP transcode request model for input, output, queues, and video parameters.
- Reused the graph planner to select the video decode, filter, and encode chain for realtime URL input.
- Added the realtime RTP graph builder on top of the existing DAG segment builders.
- Added runtime nodes for realtime input, RTP output context creation, RTP muxing, and SDP writing.
- Added `media_transcode_graph_transcode_cli --mode realtime-rtp`.
- Rejected raw RTP, UDP, and SDP input for this stage.
- Redacted URL userinfo in CLI and planner diagnostics.
- Covered validation, graph construction, URL redaction, timestamp rescale behavior, and software/hardware runtime compile paths in `media_transcode_realtime_graph_tests`.

## Verification

- Build `media_transcode_core`.
- Build `media_transcode_graph_transcode_cli`.
- Build and run `media_transcode_realtime_graph_tests`.
- Run a 15 second RTSP to RTP smoke test when a configured RTSP source is reachable.

## Deferred

- Raw RTP stream input.
- Audio RTP output.
- Detailed decoded and encoded frame counters in CLI output.
- Receiver-side SDP playback automation.
