# MediaTranscode test samples

This directory is reserved for small fixtures used by graph validation and realtime RTP smoke testing.

Current graph tests do not require a committed media sample. Realtime RTP validation normally uses a reachable RTSP or realtime video URL and the generated SDP file from `media_transcode_graph_transcode_cli --mode realtime-rtp`.

Guidelines for future samples:

- Keep samples small enough for local and CI runs.
- Prefer stable H.264/H.265 video sources with predictable stream metadata.
- Do not make default tests depend on hardware codecs.
- Do not replace existing files in place unless all graph references are intentionally updated.

You can override the sample directory at configure time:

```bash
cmake -S . -B out/build/x64-debug -DMEDIA_TRANSCODE_TEST_SAMPLES_DIR=/path/to/samples
```
